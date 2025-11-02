#include "WellguardLogic.h"
#include <WiFi.h>
#include <SPI.h>
#include "Crypto.h"
#include "WatchdogManager.h"
#include "LedManager.h"

WellguardLogic* WellguardLogic::instance = nullptr;

// --- FreeRTOS Handles ---
QueueHandle_t loraRxQueue_WGP;

// Constructor
WellguardLogic::WellguardLogic() {
    instance = this;
}

void WellguardLogic::initialize() {
    Serial.println("Wellguard Logic Initializing...");

    deviceId = WiFi.macAddress();
    deviceId.replace(":", "");
    Serial.println("Device ID: " + deviceId);

    loraRxQueue_WGP = xQueueCreate(10, LORA_RX_PACKET_MAX_LEN);

    setupHardware();
    setupLoRa();
    startTasks();

    Serial.println("Wellguard Logic Initialized.");
}

void WellguardLogic::setupHardware() {
    // Configuration des broches pour le rôle WellguardPro
    pinMode(ROLE_PIN_1, OUTPUT);       // Commande du relais
    digitalWrite(ROLE_PIN_1, LOW);     // S'assurer que le relais est éteint au démarrage
    pinMode(ROLE_PIN_2, INPUT_PULLUP); // Entrée de défaut matériel
}

void WellguardLogic::setupLoRa() {
    SPI.begin(LORA_SCK_PIN, LORA_MISO_PIN, LORA_MOSI_PIN);
    LoRa.setPins(LORA_SS_PIN, LORA_RST_PIN, LORA_DIO0_PIN);
    if (!LoRa.begin(433E6)) { // Fréquence 433 MHz
        Serial.println("Starting LoRa failed!");
        while (1);
    }

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
    xTaskCreate(Task_LoRa_Handler, "LoRaHandler", 4096, this, 3, NULL);
    xTaskCreate(Task_Fault_Monitor, "FaultMonitor", 2048, this, 4, NULL); // Lancer la nouvelle tâche avec une haute priorité
    xTaskCreate(Task_Status_Reporter, "StatusReporter", 4096, this, 1, NULL);
}

// --- FreeRTOS Tasks ---

void WellguardLogic::Task_LoRa_Handler(void *pvParameters) {
    WatchdogManager::registerTask();
    char packetBuffer[LORA_RX_PACKET_MAX_LEN];
    const TickType_t xTicksToWait = pdMS_TO_TICKS(1000); // Wait for 1 second
    for (;;) {
        WatchdogManager::pet();
        if (xQueueReceive(loraRxQueue_WGP, &packetBuffer, xTicksToWait) == pdPASS) {
            String fullPacket(packetBuffer);
            int separatorIndex = fullPacket.lastIndexOf('\1');
            String encryptedPacket = fullPacket.substring(0, separatorIndex);
            instance->lastCommandRssi = fullPacket.substring(separatorIndex + 1).toInt();

            String decryptedPacket = CryptoManager::decrypt(encryptedPacket);
            if (decryptedPacket.length() > 0) {
                handleLoRaPacket(decryptedPacket);
            }
        }
    }
}

void WellguardLogic::Task_Fault_Monitor(void* pvParameters) {
    WellguardLogic* self = (WellguardLogic*)pvParameters;
    WatchdogManager::registerTask();
    for(;;) {
        WatchdogManager::pet();
        // Rappel: INPUT_PULLUP, donc LOW signifie que le défaut est actif.
        bool faultDetected = (digitalRead(ROLE_PIN_2) == LOW);

        if (faultDetected && !self->hardwareFaultActive) {
            // Un nouveau défaut est détecté
            self->hardwareFaultActive = true;
            Serial.println("CRITICAL: Hardware fault detected! Forcing pump OFF.");
            self->setRelayState(false); // Forcer l'arrêt du relais
        } else if (!faultDetected && self->hardwareFaultActive) {
            // Le défaut a été résolu
            self->hardwareFaultActive = false;
            Serial.println("INFO: Hardware fault cleared.");
        }
        vTaskDelay(pdMS_TO_TICKS(100)); // Vérifier toutes les 100ms
    }
}


void WellguardLogic::Task_Status_Reporter(void *pvParameters) {
    WellguardLogic* self = (WellguardLogic*)pvParameters;
    const int HEARTBEAT_INTERVAL_MS = 60000; // Réduire pour un rapport plus fréquent en cas de défaut

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(HEARTBEAT_INTERVAL_MS));

        if (millis() - self->lastLoRaTransmissionTimestamp < HEARTBEAT_INTERVAL_MS) {
            continue;
        }

        String status;
        if(self->hardwareFaultActive) {
            status = "FAULT";
        } else {
            status = self->relayState ? "ON" : "OFF";
        }

        String statusPacket = LoRaMessage::serializeStatusUpdate(self->deviceId.c_str(), status.c_str(), self->lastCommandRssi);
        sendLoRaMessage(statusPacket);
    }
}


// --- LoRa Communication ---

void WellguardLogic::onReceive(int packetSize) {
    if (packetSize == 0 || packetSize > LORA_RX_PACKET_MAX_LEN) return;

    ledManager.setTemporaryState(LORA_ACTIVITY, 500);

    char packetBuffer[LORA_RX_PACKET_MAX_LEN];
    int len = 0;
    while (LoRa.available()) packetBuffer[len++] = (char)LoRa.read();
    packetBuffer[len] = '\0';
    snprintf(packetBuffer + len, sizeof(packetBuffer) - len, "\1%d", LoRa.packetRssi());
    xQueueSendFromISR(loraRxQueue_WGP, &packetBuffer, NULL);
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
    // Logique de sécurité : Ne jamais allumer si un défaut matériel est actif.
    if (newState && hardwareFaultActive) {
        Serial.println("WARN: Pump activation inhibited by active hardware fault.");
        ledManager.setState(WARNING);
        return;
    }

    relayState = newState;
    digitalWrite(ROLE_PIN_1, relayState ? HIGH : LOW);
    Serial.printf("Relay state set to: %s\n", relayState ? "ON" : "OFF");

    if (hardwareFaultActive) {
        ledManager.setState(WARNING);
    } else if (relayState) {
        ledManager.setState(ACTION_IN_PROGRESS);
    } else {
        ledManager.setState(SYSTEM_OK);
    }

    String status = hardwareFaultActive ? "FAULT" : (relayState ? "ON" : "OFF");
    String statusPacket = LoRaMessage::serializeStatusUpdate(deviceId.c_str(), status.c_str(), LoRa.packetRssi());
    sendLoRaMessage(statusPacket);
}

void WellguardLogic::sendLoRaMessage(const String& message) {
    ledManager.setTemporaryState(LORA_ACTIVITY, 500);

    String encrypted = CryptoManager::encrypt(message);

    LoRa.beginPacket();
    LoRa.print(encrypted);
    LoRa.endPacket();

    instance->lastLoRaTransmissionTimestamp = millis();
    Serial.printf("Sent LoRa Packet: %s\n", message.c_str());
}
