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
#include "DiagnosticLED.h"

// --- FreeRTOS Handles ---
QueueHandle_t commandQueue;
QueueHandle_t ledStateQueue;
SemaphoreHandle_t ackSemaphore;

// --- Clés de Stockage ---
#define PREF_KEY_WIFI_SSID "wifi_ssid"
#define PREF_KEY_WIFI_PASS "wifi_pass"
#define PREF_KEY_LORA_KEY  "lora_key"
#define PREF_NAMESPACE     "hydro_cfg"

#define HEARTBEAT_INTERVAL_MS 120000 // 2 minutes

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
bool isWellShared = false; // NOUVEAU: Indique si le puits assigné est partagé
OperatingMode currentMode = AUTO;
LevelState currentLevel = LEVEL_UNKNOWN;
bool currentPumpCommand = false;
volatile unsigned long lastLoRaTransmissionTimestamp = 0;


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

void Task_Status_Reporter(void *pvParameters) {
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(HEARTBEAT_INTERVAL_MS));
        unsigned long elapsedTime = millis() - lastLoRaTransmissionTimestamp;
        if (elapsedTime < HEARTBEAT_INTERVAL_MS) {
            continue;
        }
        StaticJsonDocument<256> doc;
        doc["type"] = "STATUS_UPDATE";
        doc["sourceId"] = deviceId;
        doc["level"] = (currentLevel == LEVEL_FULL) ? "FULL" : "EMPTY";
        doc["mode"] = (currentMode == AUTO) ? "AUTO" : "MANUAL";
        doc["pump"] = currentPumpCommand ? "ON" : "OFF";
        String packet;
        serializeJson(doc, packet);
        sendLoRaMessage(packet); // Utilise la fonction qui gère le chiffrement et l'état LED
        lastLoRaTransmissionTimestamp = millis();
    }
}

void setup() {
    Serial.begin(115200);

    pinMode(RED_LED_PIN, OUTPUT);
    pinMode(YELLOW_LED_PIN, OUTPUT);
    pinMode(BLUE_LED_PIN, OUTPUT);
    digitalWrite(RED_LED_PIN, LOW);
    digitalWrite(YELLOW_LED_PIN, LOW);
    digitalWrite(BLUE_LED_PIN, LOW);

    pinMode(LEVEL_SENSOR_PIN, INPUT_PULLUP);
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    deviceId = WiFi.macAddress();
    deviceId.replace(":", "");

    ledStateQueue = DiagnosticLED::initialize(RED_LED_PIN, YELLOW_LED_PIN, BLUE_LED_PIN);
    commandQueue = xQueueCreate(10, sizeof(char[256]));
    ackSemaphore = xSemaphoreCreateBinary();

    LED_State initState = INIT;
    xQueueSend(ledStateQueue, &initState, 0);

    if (loadConfiguration()) startStaMode();
    else startApMode();
}

void loop() { vTaskDelay(portMAX_DELAY); }

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
  <label for="lora_key">Clé LoRa</label><br>
  <input type="text" id="lora_key" name="lora_key" required><br><br>
  <input type="submit" value="Sauvegarder et Redémarrer">
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
    LED_State configState = CONFIG_MODE;
    xQueueSend(ledStateQueue, &configState, 0);

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
    int retries = 0;
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
        if (++retries > 20) {
            LED_State errorState = CONNECTIVITY_ERROR;
            xQueueSend(ledStateQueue, &errorState, 0);
        }
    }
    Serial.println("\nWiFi Connected. IP: " + WiFi.localIP().toString());

    if(!LittleFS.begin(true)) {
        Serial.println("LittleFS Mount Failed");
        LED_State errorState = CRITICAL_ERROR;
        xQueueSend(ledStateQueue, &errorState, 0);
        while(1);
    }

    SPI.begin();
    LoRa.setPins(LORA_SS_PIN, LORA_RST_PIN, LORA_DIO0_PIN);
    if (!LoRa.begin(LORA_FREQ)) {
        LED_State errorState = CRITICAL_ERROR;
        xQueueSend(ledStateQueue, &errorState, 0);
        while (1);
    }

    LED_State opState = OPERATIONAL;
    xQueueSend(ledStateQueue, &opState, 0);

    xTaskCreate(DiagnosticLED::Task_LED_Manager, "LED Manager", 2048, NULL, 0, NULL);
    xTaskCreate(Task_Sensor_Handler, "Sensor", 2048, NULL, 2, NULL);
    xTaskCreate(Task_Control_Logic, "Logic", 4096, NULL, 1, NULL);
    xTaskCreate(Task_LoRa_Manager, "LoRa", 4096, NULL, 3, NULL);
    xTaskCreate(Task_GPIO_Handler, "GPIO", 2048, NULL, 2, NULL);
    xTaskCreate(Task_Status_Reporter, "Status Reporter", 2048, NULL, 1, NULL);

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

void Task_Sensor_Handler(void *pvParameters) {
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
    const int debounceDelay = 50;
    bool wasInManualMode = false; // Pour suivre si nous étions en mode manuel

    for (;;) {
        int buttonState = digitalRead(BUTTON_PIN);
        if (buttonState != lastButtonState) {
            lastDebounceTime = millis();
        }

        if ((millis() - lastDebounceTime) > debounceDelay) {
            if (buttonState == LOW && lastButtonState == HIGH) { // Flanc descendant = bouton pressé

                if (currentMode == AUTO) {
                    // Passage de AUTO à MANUAL
                    currentMode = MANUAL;
                    wasInManualMode = true; // On entre en mode manuel
                    Serial.println("Mode switched to MANUAL.");
                    // Aucune commande n'est envoyée lors du passage en mode manuel
                } else { // currentMode == MANUAL
                    if (wasInManualMode) {
                        // C'est la première action en mode manuel : on inverse la pompe
                        bool newCommand = !currentPumpCommand;
                        if (newCommand && currentLevel == LEVEL_FULL) {
                            Serial.println("Manual start inhibited: reservoir is full.");
                        } else {
                            triggerPumpCommand(newCommand);
                            Serial.printf("Manual command: Pump %s\n", newCommand ? "ON" : "OFF");
                        }
                        wasInManualMode = false; // La prochaine pression sera pour sortir du mode manuel
                    } else {
                        // On était déjà en mode manuel et on a déjà agi, donc on retourne en AUTO
                        currentMode = AUTO;
                        wasInManualMode = false;
                        Serial.println("Mode switched back to AUTO.");
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
    if (assignedWellId.isEmpty()) return;

    String packet;
    if (isWellShared) {
        // Si le puits est partagé, on envoie une REQUÊTE à la centrale
        MessageType requestType = command ? REQUEST_PUMP_ON : REQUEST_PUMP_OFF;
        packet = LoRaMessage::serializePumpRequest(deviceId.c_str(), requestType);
        Serial.println("Well is shared. Sending request to Centrale.");
    } else {
        // Sinon, on envoie une COMMANDE directe au puits
        CommandType cmdType = command ? CMD_PUMP_ON : CMD_PUMP_OFF;
        packet = LoRaMessage::serializeCommand(deviceId.c_str(), assignedWellId.c_str(), cmdType);
        Serial.println("Well is not shared. Sending direct command.");
    }

    char buffer[256];
    packet.toCharArray(buffer, sizeof(buffer));
    // Pour les requêtes, nous n'attendons pas d'ACK du puits, donc on les envoie directement.
    // Pour les commandes directes, on passe par la file de commande fiable.
    if (isWellShared) {
        sendLoRaMessage(packet);
    } else {
        xQueueSend(commandQueue, &buffer, (TickType_t)10);
    }
}

void Task_LoRa_Manager(void *pvParameters) {
    String discoveryPacket = LoRaMessage::serializeDiscovery(deviceId.c_str(), ROLE_AQUA_RESERV_PRO);
    sendLoRaMessage(discoveryPacket);

    char commandToSend[256];

    for (;;) {
        if (xQueueReceive(commandQueue, &commandToSend, (TickType_t)10) == pdPASS) {
            if (sendReliableCommand(String(commandToSend))) {
                Serial.println("Command sent successfully with ACK.");
            } else {
                Serial.println("Command failed after all retries.");
            }
        }

        int packetSize = LoRa.parsePacket();
        if (packetSize) {
            LED_State activityState = LORA_ACTIVITY;
            xQueueSend(ledStateQueue, &activityState, 0);
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
                String targetId = doc["tgt"];

                if (targetId.equals(deviceId)) {
                    if (type == MessageType::COMMAND) {
                        String cmd = doc["cmd"];
                        if (cmd.equals("ASSIGN_WELL")) {
                            assignedWellId = doc["well_id"].as<String>();
                            // NOUVEAU: La centrale nous dit si le puits est partagé
                            isWellShared = doc["is_shared"].as<bool>();
                            Serial.printf("Received new well assignment: %s (Shared: %s)\n", assignedWellId.c_str(), isWellShared ? "Yes" : "No");
                            saveOperationalConfig();
                        }
                    }
                }
            }
        }
    }
 }

bool sendReliableCommand(const String& packet) {
    const int MAX_RETRIES = 3;
    const TickType_t ACK_TIMEOUT = 2000 / portTICK_PERIOD_MS;

    for (int i = 0; i < MAX_RETRIES; i++) {
        sendLoRaMessage(packet);
        if (xSemaphoreTake(ackSemaphore, ACK_TIMEOUT) == pdTRUE) {
            lastLoRaTransmissionTimestamp = millis();
            return true;
        }
        Serial.printf("ACK timeout. Retry %d/%d\n", i + 1, MAX_RETRIES);
    }

    Serial.println("Direct communication failed. Requesting relay from Centrale.");
    StaticJsonDocument<256> doc;
    deserializeJson(doc, packet);
    doc["type"] = MessageType::RELAY_REQUEST;
    String relayPacket;
    serializeJson(doc, relayPacket);

    sendLoRaMessage(relayPacket);
    if (xSemaphoreTake(ackSemaphore, ACK_TIMEOUT * 2) == pdTRUE) {
        return true;
    }

    return false;
 }

void sendLoRaMessage(const String& message) {
    LED_State activityState = LORA_ACTIVITY;
    xQueueSend(ledStateQueue, &activityState, 0);

    String encrypted = CryptoManager::encrypt(message, currentConfig.lora_key);
    LoRa.beginPacket();
    LoRa.print(encrypted);
    LoRa.endPacket();
    Serial.printf("Sent LoRa message: %s\n", message.c_str());
 }
