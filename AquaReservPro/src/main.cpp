#include <Arduino.h>
#include <SPI.h>
#include <LoRa.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include "LittleFS.h"
#include "config.h"
#include "Message.h"
#include "Crypto.h"

// ... (déclarations globales et FreeRTOS handles inchangés) ...
QueueHandle_t commandQueue;
SemaphoreHandle_t ackSemaphore;
// ... (déclarations des variables globales et structures comme avant)
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
// Objets globaux
Preferences preferences;
AsyncWebServer server(80);

// Variables d'état globales
String deviceId;
String assignedWellId = "";
OperatingMode currentMode = AUTO;
LevelState currentLevel = LEVEL_UNKNOWN;
bool currentPumpCommand = false;


// Prototypes
void startApMode();
void startStaMode();
bool loadConfiguration();
void saveOperationalConfig();
void Task_Control_Logic(void *pvParameters);
void Task_Sensor_Handler(void *pvParameters);
void Task_LoRa_Manager(void *pvParameters);
void Task_GPIO_Handler(void *pvParameters);
void sendLoRaMessage(const String& message);
bool sendReliableCommand(const String& packet);
void triggerPumpCommand(bool command);
void setup() {
    Serial.begin(115200);
    pinMode(LEVEL_SENSOR_PIN, INPUT_PULLUP);
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    deviceId = WiFi.macAddress();
    deviceId.replace(":", "");

    commandQueue = xQueueCreate(10, sizeof(char[256]));
    ackSemaphore = xSemaphoreCreateBinary();

    if (loadConfiguration()) startStaMode();
    else startApMode();
}

void loop() { vTaskDelay(portMAX_DELAY); }

// ... (loadConfiguration, saveOperationalConfig, startApMode inchangés) ...
bool loadConfiguration() {
    preferences.begin(PREF_NAMESPACE, true);
    currentConfig.wifi_ssid = preferences.getString(PREF_KEY_WIFI_SSID, "");
    currentConfig.wifi_pass = preferences.getString(PREF_KEY_WIFI_PASS, "");
    currentConfig.lora_key = preferences.getString(PREF_KEY_LORA_KEY, "");
    assignedWellId = preferences.getString(PREF_KEY_WELL_ID, "");
    currentMode = (OperatingMode)preferences.getUChar(PREF_KEY_MODE, AUTO);
    preferences.end();

    return (currentConfig.wifi_ssid.length() > 0 && currentConfig.lora_key.length() > 0);
}
const char* AP_FORM_HTML = R"rawliteral(
<!DOCTYPE HTML><html><head>
<title>HydroControl-GE Configuration</title>
<meta name="viewport" content="width=device-width, initial-scale=1">
</head><body>
<h1>Configuration AquaReservPro</h1>
<form action="/save" method="POST">
  <label for="ssid">SSID Wi-Fi</label><br>
  <input type="text" id="ssid" name="ssid" required><br>
  <label for="pass">Mot de Passe Wi-Fi</label><br>
  <input type="password" id="pass" name="pass"><br>
  <label for="lora_key">Cl&eacute; LoRa</label><br>
  <input type="text" id="lora_key" name="lora_key" required><br><br>
  <input type="submit" value="Sauvegarder et Red&eacute;marrer">
</form>
</body></html>
)rawliteral";
void saveOperationalConfig() {
    preferences.begin(PREF_NAMESPACE, false);
    preferences.putUChar(PREF_KEY_MODE, (unsigned char)currentMode);
    preferences.putString(PREF_KEY_WELL_ID, assignedWellId);
    preferences.end();
    Serial.println("Operational config saved.");
}
void startApMode() {
    WiFi.softAP(AP_SSID, AP_PASSWORD);
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){ request->send(200, "text/html", AP_FORM_HTML); });
    server.on("/save", HTTP_POST, [](AsyncWebServerRequest *request) {
        preferences.begin(PREF_NAMESPACE, false);
        if (request->hasParam("ssid", true)) preferences.putString(PREF_KEY_WIFI_SSID, request->getParam("ssid", true)->value());
        if (request->hasParam("pass", true)) preferences.putString(PREF_KEY_WIFI_PASS, request->getParam("pass", true)->value());
        if (request->hasParam("lora_key", true)) preferences.putString(PREF_KEY_LORA_KEY, request->getParam("lora_key", true)->value());
        preferences.end();
        request->send(200, "text/plain", "Sauvegarde... Redemarrage.");
        delay(1000);
        ESP.restart();
    });
    server.begin();
    Serial.println("AP Mode Started. Connect to " + String(AP_SSID));
}
void startStaMode() {
    WiFi.mode(WIFI_STA);
    WiFi.begin(currentConfig.wifi_ssid.c_str(), currentConfig.wifi_pass.c_str());
    while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
    Serial.println("\nWiFi Connected. IP: " + WiFi.localIP().toString());

    if(!LittleFS.begin(true)) { Serial.println("LittleFS Mount Failed"); return; }

    SPI.begin();
    LoRa.setPins(LORA_SS_PIN, LORA_RST_PIN, LORA_DIO0_PIN);
    if (!LoRa.begin(LORA_FREQ)) while (1);

    // Démarrage des tâches
    xTaskCreate(Task_Sensor_Handler, "Sensor", 2048, NULL, 2, NULL);
    xTaskCreate(Task_Control_Logic, "Logic", 4096, NULL, 1, NULL);
    xTaskCreate(Task_LoRa_Manager, "LoRa", 4096, NULL, 3, NULL);
    xTaskCreate(Task_GPIO_Handler, "GPIO", 2048, NULL, 2, NULL);

    // --- Serveur Web ---
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(LittleFS, "/index.html", "text/html");
    });
    server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *request){
        StaticJsonDocument<256> doc;
        doc["id"] = deviceId;
        doc["level"] = (currentLevel == LEVEL_FULL) ? "PLEIN" : "VIDE";
        doc["mode"] = (currentMode == AUTO) ? "AUTO" : "MANUEL";
        doc["pump_command"] = currentPumpCommand ? "ON" : "OFF";
        doc["assigned_well"] = assignedWellId;
        String output;
        serializeJson(doc, output);
        request->send(200, "application/json", output);
    });
    server.begin();
}

// ... (toutes les tâches FreeRTOS restent identiques) ...
void Task_Sensor_Handler(void *pvParameters) { /* ... (inchangé) ... */
    LevelState lastUnstableLevel = LEVEL_UNKNOWN;
    unsigned long levelChangeTimestamp = 0;
    for (;;) {
        bool rawValue = digitalRead(LEVEL_SENSOR_PIN);
        LevelState detectedLevel = (rawValue == LOW) ? LEVEL_FULL : LEVEL_EMPTY;
        if (detectedLevel != lastUnstableLevel) {
            lastUnstableLevel = detectedLevel;
            levelChangeTimestamp = millis();
        }
        if (millis() - levelChangeTimestamp > SENSOR_STABILITY_MS) {
            if (currentLevel != detectedLevel) {
                currentLevel = detectedLevel;
                Serial.printf("New stable level: %s\n", currentLevel == LEVEL_FULL ? "FULL" : "EMPTY");
            }
        }
        vTaskDelay(250 / portTICK_PERIOD_MS);
    }
}

void Task_Control_Logic(void *pvParameters) {
    bool lastPumpCommandState = !currentPumpCommand;
    for (;;) {
        if (currentMode == AUTO) {
            bool desiredCommand = currentPumpCommand;
            if (currentLevel == LEVEL_EMPTY) desiredCommand = true;
            else if (currentLevel == LEVEL_FULL) desiredCommand = false;

            if (desiredCommand != lastPumpCommandState) {
                triggerPumpCommand(desiredCommand);
                lastPumpCommandState = desiredCommand;
            }
        }
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

void Task_GPIO_Handler(void *pvParameters) {
    int lastButtonState = HIGH;
    unsigned long lastDebounceTime = 0;
    const int debounceDelay = 50; // ms

    for (;;) {
        int buttonState = digitalRead(BUTTON_PIN);
        if (buttonState != lastButtonState) {
            lastDebounceTime = millis();
        }

        if ((millis() - lastDebounceTime) > debounceDelay) {
            if (buttonState == LOW && lastButtonState == HIGH) { // Front descendant (bouton pressé)
                currentMode = (currentMode == AUTO) ? MANUAL : AUTO;
                Serial.printf("Mode switched to %s\n", currentMode == AUTO ? "AUTO" : "MANUAL");

                if (currentMode == MANUAL) {
                    bool newCommand = !currentPumpCommand;
                    if (newCommand && currentLevel == LEVEL_FULL) {
                        Serial.println("Manual start inhibited: reservoir is full.");
                    } else {
                        triggerPumpCommand(newCommand);
                    }
                }
                saveOperationalConfig();
            }
        }
        lastButtonState = buttonState;
        vTaskDelay(20 / portTICK_PERIOD_MS);
    }
}

void triggerPumpCommand(bool command) {
    currentPumpCommand = command;
    if (!assignedWellId.isEmpty()) {
        String cmdPacket = LoRaMessage::serializeCommand(deviceId.c_str(), assignedWellId.c_str(), command ? CMD_PUMP_ON : CMD_PUMP_OFF);
        char buffer[256];
        cmdPacket.toCharArray(buffer, sizeof(buffer));
        xQueueSend(commandQueue, &buffer, (TickType_t)10);
    }
}


void Task_LoRa_Manager(void *pvParameters) { /* ... (inchangé, mais la logique d'envoi est maintenant appelée par triggerPumpCommand) ... */
    String discoveryPacket = LoRaMessage::serializeDiscovery(deviceId.c_str(), ROLE_AQUA_RESERV_PRO);
    sendLoRaMessage(discoveryPacket);

    char commandToSend[256];

    for (;;) {
        // Priorité 1: Traiter les commandes en attente
        if (xQueueReceive(commandQueue, &commandToSend, (TickType_t)10) == pdPASS) {
            if (sendReliableCommand(String(commandToSend))) {
                Serial.println("Command sent successfully with ACK.");
            } else {
                Serial.println("Command failed after all retries.");
                // TODO: Logique de gestion d'erreur (ex: notifier la centrale)
            }
        }

        // Priorité 2: Traiter les paquets entrants
        int packetSize = LoRa.parsePacket();
        if (packetSize) {
            String encryptedPacket = "";
            while (LoRa.available()) encryptedPacket += (char)LoRa.read();
            String packet = CryptoManager::decrypt(encryptedPacket, currentConfig.lora_key);

            if (packet.length() > 0) {
                StaticJsonDocument<256> doc;
                deserializeJson(doc, packet);
                int type = doc["type"];
                String src = doc["src"];

                if (type == MessageType::COMMAND_ACK && src.equals(assignedWellId)) {
                    xSemaphoreGive(ackSemaphore);
                }
                // ... (autre logique de réception comme l'assignation)
                String targetId = doc["tgt"];

                if (targetId.equals(deviceId)) {
                    if (type == MessageType::COMMAND) {
                        String cmd = doc["cmd"];
                        if (cmd.equals("ASSIGN_WELL")) {
                            assignedWellId = doc["well_id"].as<String>();
                            Serial.printf("Received new well assignment: %s\n", assignedWellId.c_str());
                            saveOperationalConfig();
                        }
                    }
                }
            }
        }
    }
 }

bool sendReliableCommand(const String& packet) { /* ... (inchangé) ... */
    const int MAX_RETRIES = 3;
    const TickType_t ACK_TIMEOUT = 2000 / portTICK_PERIOD_MS;

    for (int i = 0; i < MAX_RETRIES; i++) {
        sendLoRaMessage(packet);
        if (xSemaphoreTake(ackSemaphore, ACK_TIMEOUT) == pdTRUE) {
            return true; // ACK reçu
        }
        Serial.printf("ACK timeout. Retry %d/%d\n", i + 1, MAX_RETRIES);
    }

    // Si on arrive ici, les tentatives directes ont échoué. On demande un relais.
    Serial.println("Direct communication failed. Requesting relay from Centrale.");
    StaticJsonDocument<256> doc;
    deserializeJson(doc, packet);
    doc["type"] = MessageType::RELAY_REQUEST;
    String relayPacket;
    serializeJson(doc, relayPacket);

    sendLoRaMessage(relayPacket);
    if (xSemaphoreTake(ackSemaphore, ACK_TIMEOUT * 2) == pdTRUE) { // Timeout plus long pour le relais
        return true;
    }

    return false; // Échec final
 }

void sendLoRaMessage(const String& message) { /* ... (inchangé) ... */
    String encrypted = CryptoManager::encrypt(message, currentConfig.lora_key);
    LoRa.beginPacket();
    LoRa.print(encrypted);
    LoRa.endPacket();
    Serial.printf("Sent LoRa message: %s\n", message.c_str());
 }
