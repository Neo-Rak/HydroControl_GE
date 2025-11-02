#include "AquaReservLogic.h"
#include <WiFi.h>
#include <SPI.h>
#include "Crypto.h"
#include <ArduinoJson.h>
#include "WatchdogManager.h"
#include "LedManager.h"

AquaReservLogic* AquaReservLogic::instance = nullptr;

// --- FreeRTOS Handles ---
QueueHandle_t commandQueue_ARP;
SemaphoreHandle_t ackSemaphore_ARP;


AquaReservLogic::AquaReservLogic() {
    instance = this;
}

void AquaReservLogic::initialize() {
    Serial.println("AquaReserv Logic Initializing...");

    deviceId = WiFi.macAddress();
    deviceId.replace(":", "");
    Serial.println("Device ID: " + deviceId);

    commandQueue_ARP = xQueueCreate(10, sizeof(char[256]));
    ackSemaphore_ARP = xSemaphoreCreateBinary();

    loadOperationalConfig();
    setupHardware();
    setupLoRa();
    startTasks();

    Serial.println("AquaReserv Logic Initialized.");
}

void AquaReservLogic::loadOperationalConfig() {
    Preferences prefs;
    prefs.begin("hydro_config", true);
    assignedWellId = prefs.getString("assigned_well", "");
    isWellShared = prefs.getBool("is_well_shared", false);
    currentMode = (OperatingMode)prefs.getUChar("op_mode", AUTO);
    prefs.end();
}

void AquaReservLogic::saveOperationalConfig() {
    Preferences prefs;
    prefs.begin("hydro_config", false);
    prefs.putString("assigned_well", assignedWellId);
    prefs.putBool("is_well_shared", isWellShared);
    prefs.putUChar("op_mode", (unsigned char)currentMode);
    prefs.end();
    Serial.println("Operational config saved.");
}


void AquaReservLogic::setupHardware() {
    // Initialisation des broches pour les capteurs de niveau
    pinMode(AQUA_RESERV_LEVEL_HIGH_PIN, INPUT_PULLUP);
    pinMode(AQUA_RESERV_LEVEL_LOW_PIN, INPUT_PULLUP);
    pinMode(AQUA_RESERV_BUTTON_PIN, INPUT_PULLUP);
}

void AquaReservLogic::setupLoRa() {
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

    String discoveryPacket = LoRaMessage::serializeDiscovery(deviceId.c_str(), ROLE_AQUA_RESERV_PRO);
    sendLoRaMessage(discoveryPacket);
}

void AquaReservLogic::startTasks() {
    xTaskCreate(Task_Sensor_Handler, "SensorHandler", 2048, this, 2, NULL);
    xTaskCreate(Task_Control_Logic, "ControlLogic", 4096, this, 1, NULL);
    xTaskCreate(Task_GPIO_Handler, "GPIOHandler", 2048, this, 2, NULL);
    xTaskCreate(Task_Status_Reporter, "StatusReporter", 4096, this, 1, NULL);
}


void AquaReservLogic::triggerPumpCommand(bool command) {
    currentPumpCommand = command;

    if (command) {
        ledManager.setState(ACTION_IN_PROGRESS);
    } else {
        ledManager.setState(SYSTEM_OK);
    }

    if (assignedWellId.isEmpty()) return;

    String packet;
    if (isWellShared) {
        MessageType requestType = command ? REQUEST_PUMP_ON : REQUEST_PUMP_OFF;
        packet = LoRaMessage::serializePumpRequest(deviceId.c_str(), requestType);
        Serial.println("Well is shared. Sending request to Centrale.");
        sendLoRaMessage(packet);
    } else {
        CommandType cmdType = command ? CMD_PUMP_ON : CMD_PUMP_OFF;
        packet = LoRaMessage::serializeCommand(deviceId.c_str(), assignedWellId.c_str(), cmdType);
        Serial.println("Well is not shared. Sending direct command.");
        if (!sendReliableCommand(packet)) {
            Serial.println("Command failed after all retries.");
        }
    }
}


// --- FreeRTOS Tasks ---

void AquaReservLogic::Task_Sensor_Handler(void *pvParameters) {
    WatchdogManager::registerTask(); // Enregistrer cette tâche
    AquaReservLogic* self = (AquaReservLogic*)pvParameters;
    LevelState lastUnstableLevel = self->currentLevel;
    unsigned long levelChangeTimestamp = 0;

    for (;;) {
        WatchdogManager::pet(); // Nourrir le watchdog
        // Lecture des capteurs. Rappel : INPUT_PULLUP, donc LOW = activé.
        bool highSensorActive = (digitalRead(AQUA_RESERV_LEVEL_HIGH_PIN) == LOW);
        bool lowSensorActive = (digitalRead(AQUA_RESERV_LEVEL_LOW_PIN) == LOW);

        LevelState detectedLevel;

        if (highSensorActive && lowSensorActive) {
            detectedLevel = LEVEL_FULL;
        } else if (!highSensorActive && !lowSensorActive) {
            detectedLevel = LEVEL_EMPTY;
        } else if (!highSensorActive && lowSensorActive) {
            detectedLevel = LEVEL_OK;
        } else { // highSensorActive && !lowSensorActive
            // État incohérent : le capteur haut est actif mais pas le bas.
            detectedLevel = LEVEL_ERROR;
        }

        if (detectedLevel != lastUnstableLevel) {
            lastUnstableLevel = detectedLevel;
            levelChangeTimestamp = millis();
        }

        if (millis() - levelChangeTimestamp > SENSOR_STABILITY_MS) {
            if (self->currentLevel != detectedLevel) {
                self->currentLevel = detectedLevel;
                const char* levelStr = "UNKNOWN";
                if(self->currentLevel == LEVEL_FULL) levelStr = "FULL";
                else if(self->currentLevel == LEVEL_OK) levelStr = "OK";
                else if(self->currentLevel == LEVEL_EMPTY) levelStr = "EMPTY";
                else if(self->currentLevel == LEVEL_ERROR) levelStr = "ERROR";
                Serial.printf("New stable level: %s\n", levelStr);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(250));
    }
}

void AquaReservLogic::Task_Control_Logic(void *pvParameters) {
    WatchdogManager::registerTask(); // Enregistrer cette tâche
    AquaReservLogic* self = (AquaReservLogic*)pvParameters;

    for (;;) {
        WatchdogManager::pet(); // Nourrir le watchdog
        if (self->currentMode == AUTO) {
            bool desiredCommand = self->currentPumpCommand;

            // Logique de contrôle basée sur les niveaux
            if (self->currentLevel == LEVEL_EMPTY) {
                desiredCommand = true; // Demander le démarrage
            } else if (self->currentLevel == LEVEL_FULL) {
                desiredCommand = false; // Demander l'arrêt
            }
            // Si le niveau est OK, on ne change rien à la commande en cours.

            // On envoie la commande seulement si elle change
            if (desiredCommand != self->currentPumpCommand) {
                self->triggerPumpCommand(desiredCommand);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void AquaReservLogic::Task_GPIO_Handler(void *pvParameters) {
    AquaReservLogic* self = (AquaReservLogic*)pvParameters;
    int lastButtonState = HIGH;
    unsigned long lastDebounceTime = 0;
    const int debounceDelay = 50;

    for (;;) {
        int buttonState = digitalRead(AQUA_RESERV_BUTTON_PIN);
        if (buttonState != lastButtonState) {
            lastDebounceTime = millis();
        }

        if ((millis() - lastDebounceTime) > debounceDelay) {
            if (buttonState == LOW && lastButtonState == HIGH) {
                self->currentMode = (self->currentMode == AUTO) ? MANUAL : AUTO;
                Serial.printf("Mode switched to %s\n", self->currentMode == AUTO ? "AUTO" : "MANUAL");

                if (self->currentMode == MANUAL) {
                    bool newCommand = !self->currentPumpCommand;
                    if (newCommand && self->currentLevel == LEVEL_FULL) {
                        Serial.println("Manual start inhibited: reservoir is full.");
                    } else {
                        self->triggerPumpCommand(newCommand);
                    }
                }
                self->saveOperationalConfig();
            }
        }
        lastButtonState = buttonState;
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void AquaReservLogic::Task_Status_Reporter(void *pvParameters) {
    AquaReservLogic* self = (AquaReservLogic*)pvParameters;
    const int HEARTBEAT_INTERVAL_MS = 120000;
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(HEARTBEAT_INTERVAL_MS));

        if (millis() - self->lastLoRaTransmissionTimestamp < HEARTBEAT_INTERVAL_MS) {
            continue;
        }

        const char* levelStr = "UNKNOWN";
        if(self->currentLevel == LEVEL_FULL) levelStr = "FULL";
        else if(self->currentLevel == LEVEL_OK) levelStr = "OK";
        else if(self->currentLevel == LEVEL_EMPTY) levelStr = "EMPTY";
        else if(self->currentLevel == LEVEL_ERROR) levelStr = "ERROR";

        String statusPacket = LoRaMessage::serializeStatusUpdate(self->deviceId.c_str(), levelStr, LoRa.packetRssi());
        sendLoRaMessage(statusPacket);
    }
}


// --- LoRa Communication ---

void AquaReservLogic::onReceive(int packetSize) {
    if (packetSize == 0) return;

    ledManager.setTemporaryState(LORA_ACTIVITY, 500);

    String encryptedPacket = "";
    while (LoRa.available()) {
        encryptedPacket += (char)LoRa.read();
    }

    String decryptedPacket = CryptoManager::decrypt(encryptedPacket);

    if (decryptedPacket.length() > 0) {
        handleLoRaPacket(decryptedPacket);
    } else {
        Serial.println("Decryption failed.");
    }
}

void AquaReservLogic::handleLoRaPacket(const String& packet) {
    StaticJsonDocument<256> doc;
    deserializeJson(doc, packet);

    int type = doc["type"];
    const char* src = doc["src"];

    if (type == MessageType::COMMAND_ACK && src != nullptr && instance->assignedWellId.equals(src)) {
        xSemaphoreGive(ackSemaphore_ARP);
    }

    const char* targetId = doc["tgt"];
    if(targetId != nullptr && instance->deviceId.equals(targetId)) {
        if (type == MessageType::COMMAND) {
            const char* cmd = doc["cmd"];
            if (cmd != nullptr && strcmp(cmd, "ASSIGN_WELL") == 0) {
                instance->assignedWellId = doc["well_id"].as<String>();
                instance->isWellShared = doc["is_shared"].as<bool>();
                Serial.printf("Received new well assignment: %s (Shared: %s)\n", instance->assignedWellId.c_str(), instance->isWellShared ? "Yes" : "No");
                instance->saveOperationalConfig();
            }
        }
    }
}

bool AquaReservLogic::sendReliableCommand(const String& packet) {
    const int MAX_RETRIES = 3;
    const TickType_t ACK_TIMEOUT = pdMS_TO_TICKS(2000);

    for (int i = 0; i < MAX_RETRIES; i++) {
        sendLoRaMessage(packet);
        if (xSemaphoreTake(ackSemaphore_ARP, ACK_TIMEOUT) == pdTRUE) {
            lastLoRaTransmissionTimestamp = millis();
            return true;
        }
        Serial.printf("ACK timeout. Retry %d/%d\n", i + 1, MAX_RETRIES);
    }
    return false;
}

void AquaReservLogic::sendLoRaMessage(const String& message) {
    ledManager.setTemporaryState(LORA_ACTIVITY, 500);

    String encrypted = CryptoManager::encrypt(message);
    LoRa.beginPacket();
    LoRa.print(encrypted);
    LoRa.endPacket();

    instance->lastLoRaTransmissionTimestamp = millis();
    Serial.printf("Sent LoRa Packet: %s\n", message.c_str());
}
