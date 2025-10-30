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

// --- FreeRTOS Handles ---
QueueHandle_t ledStateQueue;

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
String deviceId;
volatile bool relayState = false;
volatile long lastCommandRssi = 0;
volatile unsigned long lastLoRaTransmissionTimestamp = 0;

// Prototypes
void startApMode();
void startStaMode();
bool loadConfiguration();
void onReceive(int packetSize);
void handleLoRaPacket(const String& packet);
void sendLoRaMessage(const String& message);
void setRelayState(bool newState);
void Task_LED_Manager(void *pvParameters);

void Task_Status_Reporter(void *pvParameters) {
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(HEARTBEAT_INTERVAL_MS));

        if (millis() - lastLoRaTransmissionTimestamp < HEARTBEAT_INTERVAL_MS) {
            continue;
        }

        StaticJsonDocument<256> doc;
        doc["type"] = "STATUS_UPDATE";
        doc["sourceId"] = deviceId;
        doc["relayState"] = relayState ? "ON" : "OFF";
        doc["rssi"] = lastCommandRssi;

        String packet;
        serializeJson(doc, packet);
        sendLoRaMessage(packet);
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

    pinMode(RELAY_PIN, OUTPUT);
    digitalWrite(RELAY_PIN, LOW);
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    deviceId = WiFi.macAddress();
    deviceId.replace(":", "");

    ledStateQueue = xQueueCreate(10, sizeof(LED_State));

    LED_State initState = INIT;
    xQueueSend(ledStateQueue, &initState, 0);

    if (loadConfiguration()) startStaMode();
    else startApMode();
}

void loop() {
    vTaskDelay(portMAX_DELAY);
}

bool loadConfiguration() {
    preferences.begin(PREF_NAMESPACE, true);
    currentConfig.wifi_ssid = preferences.getString(PREF_KEY_WIFI_SSID, "");
    currentConfig.wifi_pass = preferences.getString(PREF_KEY_WIFI_PASS, "");
    currentConfig.lora_key = preferences.getString(PREF_KEY_LORA_KEY, "");
    preferences.end();
    return (currentConfig.wifi_ssid.length() > 0 && currentConfig.lora_key.length() > 0);
}

const char* AP_FORM_HTML = R"rawliteral(
<!DOCTYPE HTML><html><head>
<title>HydroControl-GE Configuration</title>
<meta name="viewport" content="width=device-width, initial-scale=1">
</head><body>
<h1>Configuration WellguardPro</h1>
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
        while(1);
    }

    String discoveryPacket = LoRaMessage::serializeDiscovery(deviceId.c_str(), ROLE_WELLGUARD_PRO);
    sendLoRaMessage(discoveryPacket);

    LoRa.onReceive(onReceive);
    LoRa.receive();

    LED_State opState = OPERATIONAL;
    xQueueSend(ledStateQueue, &opState, 0);

    xTaskCreate(Task_LED_Manager, "LED Manager", 2048, NULL, 0, NULL);
    xTaskCreate(Task_Status_Reporter, "Status Reporter", 2048, NULL, 1, NULL);

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(LittleFS, "/index.html", "text/html");
    });
    server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *request){
        StaticJsonDocument<128> doc;
        doc["id"] = deviceId;
        doc["relay_state"] = relayState ? "ON" : "OFF";
        doc["last_rssi"] = lastCommandRssi;
        String output;
        serializeJson(doc, output);
        request->send(200, "application/json", output);
    });
    server.begin();
}

void onReceive(int packetSize) {
    if (packetSize == 0) return;

    LED_State activityState = LORA_ACTIVITY;
    xQueueSendFromISR(ledStateQueue, &activityState, NULL);

    String encryptedPacket = "";
    while (LoRa.available()) encryptedPacket += (char)LoRa.read();

    lastCommandRssi = LoRa.packetRssi();
    String packet = CryptoManager::decrypt(encryptedPacket, currentConfig.lora_key);
    if(packet.length() > 0) handleLoRaPacket(packet);
}

void handleLoRaPacket(const String& packet) {
    StaticJsonDocument<256> doc;
    deserializeJson(doc, packet);

    String targetId = doc["tgt"];
    if (!deviceId.equals(targetId)) return;

    int type = doc["type"];
    if (type == MessageType::COMMAND) {
        int cmd = doc["cmd"];
        bool newRelayState = (cmd == CMD_PUMP_ON);
        setRelayState(newRelayState);

        String sourceId = doc["src"];
        String ackPacket = LoRaMessage::serializeCommandAck(deviceId.c_str(), sourceId.c_str(), true);
        sendLoRaMessage(ackPacket);
        lastLoRaTransmissionTimestamp = millis();
    }
}

void setRelayState(bool newState) {
    relayState = newState;
    digitalWrite(RELAY_PIN, relayState ? HIGH : LOW);
    Serial.printf("Relay state set to: %s\n", relayState ? "ON" : "OFF");

    String status = relayState ? "ON" : "OFF";
    String statusPacket = LoRaMessage::serializeStatusUpdate(deviceId.c_str(), status.c_str(), LoRa.packetRssi());
    sendLoRaMessage(statusPacket);
}

void sendLoRaMessage(const String& message) {
    LED_State activityState = LORA_ACTIVITY;
    xQueueSend(ledStateQueue, &activityState, 0);

    String encrypted = CryptoManager::encrypt(message, currentConfig.lora_key);
    LoRa.beginPacket();
    LoRa.print(encrypted);
    LoRa.endPacket();
    Serial.printf("Sent LoRa: %s\n", message.c_str());
}

void Task_LED_Manager(void *pvParameters) {
    LED_State currentState = INIT;
    LED_State previousState = OPERATIONAL;
    unsigned long lastBlinkTime = 0;
    bool ledOn = false;

    auto turnOffAllLeds = []() {
        digitalWrite(RED_LED_PIN, LOW);
        digitalWrite(YELLOW_LED_PIN, LOW);
        digitalWrite(BLUE_LED_PIN, LOW);
    };

    for (;;) {
        if (xQueueReceive(ledStateQueue, &currentState, 0) == pdPASS) {
            turnOffAllLeds();
            if (currentState != LORA_ACTIVITY) {
                previousState = currentState;
            }
        }

        switch (currentState) {
            case INIT:
                if (millis() - lastBlinkTime > 500) {
                    ledOn = !ledOn;
                    digitalWrite(YELLOW_LED_PIN, ledOn);
                    lastBlinkTime = millis();
                }
                break;
            case CONFIG_MODE:
                digitalWrite(YELLOW_LED_PIN, HIGH);
                break;
            case OPERATIONAL:
                digitalWrite(BLUE_LED_PIN, HIGH);
                break;
            case LORA_ACTIVITY:
                digitalWrite(BLUE_LED_PIN, HIGH);
                vTaskDelay(pdMS_TO_TICKS(50));
                digitalWrite(BLUE_LED_PIN, LOW);
                vTaskDelay(pdMS_TO_TICKS(50));
                currentState = previousState;
                turnOffAllLeds();
                break;
            case CONNECTIVITY_ERROR:
                if (millis() - lastBlinkTime > 500) {
                    ledOn = !ledOn;
                    digitalWrite(RED_LED_PIN, ledOn);
                    lastBlinkTime = millis();
                }
                break;
            case CRITICAL_ERROR:
                digitalWrite(RED_LED_PIN, HIGH);
                break;
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
