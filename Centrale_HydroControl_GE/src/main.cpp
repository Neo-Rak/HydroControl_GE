#include <Arduino.h>
#include <SPI.h>
#include <LoRa.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <Preferences.h>
#include "LittleFS.h"
#include "config.h"
#include "Message.h"
#include "Crypto.h"

// --- FreeRTOS Handles ---
QueueHandle_t loraRxQueue;
SemaphoreHandle_t nodeListMutex;
#define LORA_RX_QUEUE_SIZE 10
#define LORA_RX_PACKET_MAX_LEN 256

// ... (structures, variables globales, etc. comme avant) ...
// --- Clés de Stockage ---
#define PREF_KEY_WIFI_SSID "wifi_ssid"
#define PREF_KEY_WIFI_PASS "wifi_pass"
#define PREF_KEY_LORA_KEY  "lora_key"
#define PREF_NAMESPACE     "hydro_cfg"

// --- Structure de Configuration ---
struct SystemConfig {
    String wifi_ssid;
    String wifi_pass;
    String lora_key;
};

SystemConfig currentConfig;

// --- Structures de Données des Noeuds ---
struct Node {
    String id;
    String name; // NOUVEAU CHAMP
    NodeRole type;
    long lastSeen;
    int rssi;
    String status;
    String assignedTo; // Pour un WellguardPro, l'ID de l'AquaReservPro qu'il sert
};

Node nodeList[MAX_NODES];
int nodeCount = 0;

// Objets globaux
AsyncWebServer server(80);
AsyncEventSource events("/events");
Preferences preferences;

// Prototypes
void startApMode();
void startStaMode();
bool loadConfiguration();
void onReceive(int packetSize);
void handleLoRaPacket(String packet, int rssi);
void registerOrUpdateNode(String id, NodeRole type, String status, int rssi);
String getSystemStatusJson();
void sendLoRaMessage(const String& message);
void Task_LoRa_Handler(void *pvParameters);
void Task_Node_Janitor(void *pvParameters);
void saveNodeName(const String& nodeId, const String& nodeName);
String loadNodeName(const String& nodeId);


// --- Fonctions de Persistance des Noms ---
void saveNodeName(const String& nodeId, const String& nodeName) {
    preferences.begin("node-names", false); // R/W mode
    preferences.putString(nodeId.c_str(), nodeName);
    preferences.end();
    Serial.printf("Saved name for %s: %s\n", nodeId.c_str(), nodeName.c_str());
}

String loadNodeName(const String& nodeId) {
    preferences.begin("node-names", true); // Read-only
    String name = preferences.getString(nodeId.c_str(), "");
    preferences.end();
    if (name.length() > 0) {
        Serial.printf("Loaded name for %s: %s\n", nodeId.c_str(), name.c_str());
    }
    return name;
}


void setup() {
    Serial.begin(115200);

    loraRxQueue = xQueueCreate(LORA_RX_QUEUE_SIZE, sizeof(char[LORA_RX_PACKET_MAX_LEN]));
    nodeListMutex = xSemaphoreCreateMutex();

    if (loadConfiguration()) startStaMode();
    else startApMode();
}

void loop() {
     if (WiFi.getMode() == WIFI_STA) {
        events.send(getSystemStatusJson().c_str(), "update", millis());
        delay(2000);
    }
}

// ... (loadConfiguration et startApMode restent les mêmes) ...
bool loadConfiguration() {
    preferences.begin(PREF_NAMESPACE, true); // Lecture seule
    currentConfig.wifi_ssid = preferences.getString(PREF_KEY_WIFI_SSID, "");
    currentConfig.wifi_pass = preferences.getString(PREF_KEY_WIFI_PASS, "");
    currentConfig.lora_key = preferences.getString(PREF_KEY_LORA_KEY, "");
    preferences.end();

    if (currentConfig.wifi_ssid.length() > 0 && currentConfig.lora_key.length() > 0) {
        Serial.println("Configuration found.");
        return true;
    }
    Serial.println("No configuration found. Starting in AP mode.");
    return false;
}
const char* AP_FORM_HTML = R"rawliteral(
<!DOCTYPE HTML><html><head>
<title>HydroControl-GE Configuration</title>
<meta name="viewport" content="width=device-width, initial-scale=1">
<style>
  body { font-family: Arial, sans-serif; background-color: #f4f4f4; margin: 40px; }
  form { background-color: #fff; padding: 20px; border-radius: 8px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }
  input[type=text], input[type=password] { width: 100%; padding: 10px; margin: 10px 0; border: 1px solid #ddd; border-radius: 4px; box-sizing: border-box; }
  input[type=submit] { background-color: #4CAF50; color: white; padding: 14px 20px; margin: 8px 0; border: none; border-radius: 4px; cursor: pointer; width: 100%; }
  input[type=submit]:hover { background-color: #45a049; }
</style>
</head><body>
<h1>Configuration de la Centrale HydroControl-GE</h1>
<form action="/save" method="POST">
  <label for="ssid">SSID du R&eacute;seau Wi-Fi</label>
  <input type="text" id="ssid" name="ssid" required>
  <label for="pass">Mot de Passe Wi-Fi</label>
  <input type="password" id="pass" name="pass">
  <label for="lora_key">Cl&eacute; Secrete LoRa (16 caract&egrave;res max)</label>
  <input type="text" id="lora_key" name="lora_key" required>
  <input type="submit" value="Sauvegarder et Red&eacute;marrer">
</form>
</body></html>
)rawliteral";
void startApMode() {
    Serial.println("Starting Access Point: " + String(AP_SSID));
    WiFi.softAP(AP_SSID, AP_PASSWORD);
    IPAddress IP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(IP);

    server.on("/", HTTP_GET, [AP_FORM_HTML](AsyncWebServerRequest *request) {
        request->send(200, "text/html", AP_FORM_HTML);
    });

    server.on("/save", HTTP_POST, [](AsyncWebServerRequest *request) {
        SystemConfig newConfig;
        if (request->hasParam("ssid", true)) newConfig.wifi_ssid = request->getParam("ssid", true)->value();
        if (request->hasParam("pass", true)) newConfig.wifi_pass = request->getParam("pass", true)->value();
        if (request->hasParam("lora_key", true)) newConfig.lora_key = request->getParam("lora_key", true)->value();

        preferences.begin(PREF_NAMESPACE, false); // Lecture/Écriture
        preferences.putString(PREF_KEY_WIFI_SSID, newConfig.wifi_ssid);
        preferences.putString(PREF_KEY_WIFI_PASS, newConfig.wifi_pass);
        preferences.putString(PREF_KEY_LORA_KEY, newConfig.lora_key);
        preferences.end();

        String response = "Configuration sauvegard&eacute;e. Red&eacute;marrage en cours...";
        request->send(200, "text/plain", response);
        delay(1000);
        ESP.restart();
    });
    server.begin();
}
void startStaMode() {
    WiFi.mode(WIFI_STA);
    WiFi.begin(currentConfig.wifi_ssid.c_str(), currentConfig.wifi_pass.c_str());
    while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
    Serial.println("\nWiFi Connected. IP: " + WiFi.localIP().toString());

    if(!LittleFS.begin(true)) { Serial.println("LittleFS Mount Failed"); return; }

    SPI.begin();
    LoRa.setPins(LORA_SS_PIN, LORA_RST_PIN, LORA_DIO0_PIN);
    if (!LoRa.begin(LORA_FREQ)) { Serial.println("Starting LoRa failed!"); while (1); }

    // Configurer le callback LoRa et mettre en mode réception
    LoRa.onReceive(onReceive);
    LoRa.receive();
    Serial.println("LoRa Initialized.");

    // Démarrer la tâche de traitement LoRa
    xTaskCreate(Task_LoRa_Handler, "LoRa Handler", 4096, NULL, 3, NULL);
    xTaskCreate(Task_Node_Janitor, "Node Janitor", 2048, NULL, 1, NULL);

    // --- Serveur Web ---
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){ request->send(LittleFS, "/index.html", "text/html"); });
    server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request){ request->send(LittleFS, "/style.css", "text/css"); });
    server.on("/script.js", HTTP_GET, [](AsyncWebServerRequest *request){ request->send(LittleFS, "/script.js", "application/javascript"); });
    server.on("/api/assign", HTTP_POST, [](AsyncWebServerRequest *request) {
        String reservoirId, wellId;
        if (request->hasParam("reservoir", true)) reservoirId = request->getParam("reservoir", true)->value();
        if (request->hasParam("well", true)) wellId = request->getParam("well", true)->value();

        if (reservoirId.length() > 0 && wellId.length() > 0) {
            // 1. Envoyer la commande de configuration
            StaticJsonDocument<256> doc;
            doc["type"] = MessageType::COMMAND;
            doc["tgt"] = reservoirId;
            doc["cmd"] = "ASSIGN_WELL";
            doc["well_id"] = wellId;
            String packet;
            serializeJson(doc, packet);
            sendLoRaMessage(packet);

            // 2. Mettre à jour l'état local dans la centrale
            if (xSemaphoreTake(nodeListMutex, portMAX_DELAY) == pdTRUE) {
                for (int i = 0; i < nodeCount; i++) {
                    if (nodeList[i].id.equals(reservoirId)) {
                        nodeList[i].assignedTo = wellId;
                    }
                    if (nodeList[i].id.equals(wellId)) {
                        nodeList[i].assignedTo = reservoirId;
                    }
                }
                xSemaphoreGive(nodeListMutex);
            }

            request->send(200, "text/plain", "Assignation command sent and saved for " + reservoirId);
        } else {
            request->send(400, "text/plain", "Missing parameters.");
        }
    });
    server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *request){ request->send(200, "application/json", getSystemStatusJson()); });

    // Nouvelle route pour définir le nom d'un noeud
    server.on("/api/set-name", HTTP_POST, [](AsyncWebServerRequest *request){
        String nodeId, nodeName;
        // ArduinoJson 6+
        StaticJsonDocument<128> doc;
        deserializeJson(doc, request->getParam("body", true)->value());

        nodeId = doc["id"].as<String>();
        nodeName = doc["name"].as<String>();

        if (nodeId.length() > 0 && nodeName.length() > 0) {
            bool updated = false;
            if (xSemaphoreTake(nodeListMutex, portMAX_DELAY) == pdTRUE) {
                for (int i = 0; i < nodeCount; i++) {
                    if (nodeList[i].id.equals(nodeId)) {
                        nodeList[i].name = nodeName;
                        saveNodeName(nodeId, nodeName);
                        updated = true;
                        break;
                    }
                }
                xSemaphoreGive(nodeListMutex);
            }

            if (updated) {
                events.send(getSystemStatusJson().c_str(), "update", millis());
            }

            request->send(200, "text/plain", "Name updated successfully.");
        } else {
            request->send(400, "text/plain", "Invalid request.");
        }
    }, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
        // Handler pour le corps de la requête JSON
        if (!index) {
            request->_tempObject = new String();
        }
        String* body = (String*)request->_tempObject;
        body->concat((char*)data, len);
        if (index + len == total) {
            request->addParam(new AsyncWebParameter("body", *body, true)); // true for POST
            delete body;
            request->_tempObject = nullptr;
        }
    });

    server.addHandler(&events);
    server.begin();
}

void onReceive(int packetSize) {
    if (packetSize == 0 || packetSize > LORA_RX_PACKET_MAX_LEN) return;

    char packetBuffer[LORA_RX_PACKET_MAX_LEN];
    int len = 0;
    while (LoRa.available()) {
        packetBuffer[len++] = (char)LoRa.read();
    }
    packetBuffer[len] = '\0'; // Terminer la chaîne

    // Ajouter le RSSI à la fin du paquet (séparé par un caractère non-imprimable)
    snprintf(packetBuffer + len, sizeof(packetBuffer) - len, "\1%d", LoRa.packetRssi());

    // Envoyer à la queue pour traitement hors de l'ISR
    xQueueSendFromISR(loraRxQueue, &packetBuffer, NULL);
}

void Task_LoRa_Handler(void *pvParameters) {
    char packetBuffer[LORA_RX_PACKET_MAX_LEN];
    for (;;) {
        if (xQueueReceive(loraRxQueue, &packetBuffer, portMAX_DELAY) == pdPASS) {
            String fullPacket(packetBuffer);

            // Extraire le paquet et le RSSI
            int separatorIndex = fullPacket.lastIndexOf('\1');
            String encryptedPacket = fullPacket.substring(0, separatorIndex);
            int rssi = fullPacket.substring(separatorIndex + 1).toInt();

            String decryptedPacket = CryptoManager::decrypt(encryptedPacket, currentConfig.lora_key);
            if (decryptedPacket.length() > 0) {
                handleLoRaPacket(decryptedPacket, rssi);
            } else {
                Serial.println("Failed to decrypt packet in LoRa Task.");
            }
        }
    }
}


void handleLoRaPacket(String packet, int rssi) {
    // ... (la logique de cette fonction reste identique à avant) ...
    StaticJsonDocument<256> doc;
    deserializeJson(doc, packet);
    int type = doc["type"];
    
    if (type == MessageType::DISCOVERY) {
        String id = doc["id"];
        NodeRole role = (NodeRole)doc["role"].as<int>();
        registerOrUpdateNode(id, role, "Discovered", rssi);
        // Envoyer un WELCOME_ACK
        StaticJsonDocument<128> ackDoc;
        ackDoc["type"] = MessageType::WELCOME_ACK;
        ackDoc["tgt"] = id;
        String ackPacket;
        serializeJson(ackDoc, ackPacket);
        sendLoRaMessage(ackPacket);
    } else if (type == MessageType::STATUS_UPDATE) {
        String id = doc["id"];
        String status = doc["status"];
        registerOrUpdateNode(id, ROLE_UNKNOWN, status, rssi);
    } else if (type == MessageType::RELAY_REQUEST) {
        Serial.println("Received a relay request.");
        // Le paquet de relais contient la commande originale.
        // Nous la modifions pour la renvoyer comme une commande directe.
        doc["type"] = MessageType::COMMAND;
        String relayedPacket;
        serializeJson(doc, relayedPacket);
        sendLoRaMessage(relayedPacket); // La fonction d'envoi s'occupe du chiffrement.
    }
}
void registerOrUpdateNode(String id, NodeRole role, String status, int rssi) {
    if (xSemaphoreTake(nodeListMutex, portMAX_DELAY) == pdTRUE) {
        int existingNodeIndex = -1;
        for (int i = 0; i < nodeCount; i++) {
            if (nodeList[i].id.equals(id)) {
                existingNodeIndex = i;
                break;
            }
        }

        if (existingNodeIndex != -1) { // Mise à jour
            nodeList[existingNodeIndex].lastSeen = millis();
            nodeList[existingNodeIndex].rssi = rssi;
            nodeList[existingNodeIndex].status = status;
            if (role != ROLE_UNKNOWN) { // Mettre à jour le rôle si fourni
                nodeList[existingNodeIndex].type = role;
            }
        } else if (nodeCount < MAX_NODES) { // Nouveau noeud
            nodeList[nodeCount].id = id;
            nodeList[nodeCount].name = loadNodeName(id); // Charger le nom
            nodeList[nodeCount].type = role;
            nodeList[nodeCount].lastSeen = millis();
            nodeList[nodeCount].rssi = rssi;
            nodeList[nodeCount].status = status;
            nodeList[nodeCount].assignedTo = ""; // Initialisation
            nodeCount++;
        }
        xSemaphoreGive(nodeListMutex);
    }
}
void sendLoRaMessage(const String& message) {
    String encrypted = CryptoManager::encrypt(message, currentConfig.lora_key);
    LoRa.beginPacket();
    LoRa.print(encrypted);
    LoRa.endPacket();
    Serial.printf("Sent LoRa: %s\n", message.c_str());
}
String getSystemStatusJson() {
    String output;
    if (xSemaphoreTake(nodeListMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        StaticJsonDocument<1024> doc;
        doc["nodeCount"] = nodeCount;
        JsonArray nodes = doc.createNestedArray("nodes");

        for (int i = 0; i < nodeCount; i++) {
            JsonObject node = nodes.createNestedObject();
            node["id"] = nodeList[i].id;
            node["name"] = nodeList[i].name;
            switch(nodeList[i].type) {
                case ROLE_AQUA_RESERV_PRO: node["type"] = "AquaReservPro"; break;
                case ROLE_WELLGUARD_PRO: node["type"] = "WellguardPro"; break;
                default: node["type"] = "Unknown"; break;
            }
            node["rssi"] = nodeList[i].rssi;
            node["status"] = nodeList[i].status;
            node["lastSeen"] = nodeList[i].lastSeen;
            node["assignedTo"] = nodeList[i].assignedTo;
        }
        serializeJson(doc, output);
        xSemaphoreGive(nodeListMutex);
    } else {
        // Could not take mutex, return an empty JSON object or an error message
        output = "{\"error\":\"Could not access node list\"}";
    }
    return output;
}

void Task_Node_Janitor(void *pvParameters) {
    const long NODE_TIMEOUT_MS = 300000; // 5 minutes
    const TickType_t TASK_INTERVAL_TICKS = pdMS_TO_TICKS(30000); // 30 secondes

    for (;;) {
        vTaskDelay(TASK_INTERVAL_TICKS);

        if (xSemaphoreTake(nodeListMutex, portMAX_DELAY) == pdTRUE) {
            unsigned long currentTime = millis();
            bool changed = false;
            for (int i = 0; i < nodeCount; i++) {
                if (nodeList[i].status != "DISCONNECTED" && (currentTime - nodeList[i].lastSeen > NODE_TIMEOUT_MS)) {
                    nodeList[i].status = "DISCONNECTED";
                    changed = true;
                    Serial.printf("Node %s timed out. Marked as DISCONNECTED.\n", nodeList[i].id.c_str());
                }
            }
            xSemaphoreGive(nodeListMutex);
        }
    }
}
