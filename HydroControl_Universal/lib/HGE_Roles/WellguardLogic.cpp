#include "WellguardLogic.h"
#include <WiFi.h>
#include <SPI.h>
#include "Crypto.h"

WellguardLogic* WellguardLogic::instance = nullptr;

// Constructor
WellguardLogic::WellguardLogic() {
    instance = this;
}

void WellguardLogic::initialize() {
    Serial.println("Wellguard Logic Initializing...");

    // Generate a unique device ID from MAC address
    deviceId = WiFi.macAddress();
    deviceId.replace(":", "");
    Serial.println("Device ID: " + deviceId);

    setupHardware();
    setupLoRa();
    startTasks();

    Serial.println("Wellguard Logic Initialized.");
}

void WellguardLogic::setupHardware() {
    pinMode(WELLGUARD_RELAY_PIN, OUTPUT);
    digitalWrite(WELLGUARD_RELAY_PIN, LOW);
}

void WellguardLogic::setupLoRa() {
    SPI.begin(LORA_SCK_PIN, LORA_MISO_PIN, LORA_MOSI_PIN);
    LoRa.setPins(LORA_SS_PIN, LORA_RST_PIN, LORA_DIO0_PIN);
    if (!LoRa.begin(433E6)) { // Fréquence 433 MHz
        Serial.println("Starting LoRa failed!");
        while (1);
    }

    // Load LoRa PSK from preferences
    Preferences prefs;
    prefs.begin("security_config", true);
    String psk = prefs.getString("lora_psk", "");
    prefs.end();

    if (psk.length() == 16) {
        CryptoManager::setKey((const uint8_t*)psk.c_str());
    } else {
        Serial.println("FATAL: LoRa PSK is not 16 characters. Halting.");
        while(1);
    }

    LoRa.onReceive(onReceive);
    LoRa.receive();
    Serial.println("LoRa receiver started.");
}

void WellguardLogic::startTasks() {
    xTaskCreate(
        Task_Status_Reporter,
        "StatusReporter",
        4096,
        this,
        1,
        NULL
    );
}

// --- FreeRTOS Tasks ---

void WellguardLogic::Task_Status_Reporter(void *pvParameters) {
    WellguardLogic* self = (WellguardLogic*)pvParameters;
    const int HEARTBEAT_INTERVAL_MS = 120000;

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(HEARTBEAT_INTERVAL_MS));

        if (millis() - self->lastLoRaTransmissionTimestamp < HEARTBEAT_INTERVAL_MS) {
            continue;
        }

        String status = self->relayState ? "ON" : "OFF";
        String statusPacket = LoRaMessage::serializeStatusUpdate(self->deviceId.c_str(), status.c_str(), self->lastCommandRssi);
        sendLoRaMessage(statusPacket);
    }
}


// --- LoRa Communication ---

void WellguardLogic::onReceive(int packetSize) {
    if (packetSize == 0) return;

    String encryptedPacket = "";
    while (LoRa.available()) {
        encryptedPacket += (char)LoRa.read();
    }

    instance->lastCommandRssi = LoRa.packetRssi();

    String decryptedPacket = CryptoManager::decrypt(encryptedPacket);

    if (decryptedPacket.length() > 0) {
        handleLoRaPacket(decryptedPacket);
    } else {
        Serial.println("Decryption failed.");
    }
}

void WellguardLogic::handleLoRaPacket(const String& packet) {
    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, packet);

    if (error) {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.c_str());
        return;
    }

    const char* targetId = doc["tgt"];
    if (targetId != nullptr && instance->deviceId.equals(targetId)) {

        int type = doc["type"];
        if (type == MessageType::COMMAND) {
            int cmd = doc["cmd"];
            bool newRelayState = (cmd == CMD_PUMP_ON);
            instance->setRelayState(newRelayState);

            const char* sourceId = doc["src"];
            if(sourceId){
                String ackPacket = LoRaMessage::serializeCommandAck(instance->deviceId.c_str(), sourceId, true);
                sendLoRaMessage(ackPacket);
            }
        }
    }
}

void WellguardLogic::setRelayState(bool newState) {
    relayState = newState;
    digitalWrite(WELLGUARD_RELAY_PIN, relayState ? HIGH : LOW);
    Serial.printf("Relay state set to: %s\n", relayState ? "ON" : "OFF");

    String status = relayState ? "ON" : "OFF";
    String statusPacket = LoRaMessage::serializeStatusUpdate(deviceId.c_str(), status.c_str(), LoRa.packetRssi());
    sendLoRaMessage(statusPacket);
}

void WellguardLogic::sendLoRaMessage(const String& message) {
    String encrypted = CryptoManager::encrypt(message);

    LoRa.beginPacket();
    LoRa.print(encrypted);
    LoRa.endPacket();

    instance->lastLoRaTransmissionTimestamp = millis();
    Serial.printf("Sent LoRa Packet: %s\n", message.c_str());
}
