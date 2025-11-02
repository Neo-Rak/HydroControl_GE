#include "CentraleLogic.h"
#include <WiFi.h>
#include <SPI.h>
#include "Crypto.h"
#include "LittleFS.h"
#include "WatchdogManager.h"
#include "LedManager.h"

CentraleLogic* CentraleLogic::instance = nullptr;

// --- FreeRTOS Handles ---
QueueHandle_t loraRxQueue_Centrale;
SemaphoreHandle_t nodeListMutex_Centrale;

CentraleLogic::CentraleLogic() : server(80), events("/events") {
    instance = this;
}

void CentraleLogic::initialize() {
    Serial.println("Centrale Logic Initializing...");

    // --- Connect to Wi-Fi ---
    Preferences prefs;
    prefs.begin("network_config", true);
    String ssid = prefs.getString("ssid", "");
    String password = prefs.getString("password", "");
    prefs.end();

    if (ssid.length() > 0) {
        WiFi.begin(ssid.c_str(), password.c_str());
        Serial.print("Connecting to WiFi...");
        int retries = 0;
        while (WiFi.status() != WL_CONNECTED && retries < 20) {
            delay(500);
            Serial.print(".");
            retries++;
        }
        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("\nWiFi connected!");
            Serial.print("IP Address: ");
            Serial.println(WiFi.localIP());
        } else {
            Serial.println("\nFailed to connect to WiFi. Web server will not be available.");
        }
    } else {
        Serial.println("No WiFi credentials found. Web server will not be available.");
    }
    // --- End Wi-Fi Connection ---

    deviceId = WiFi.macAddress();
    deviceId.replace(":", "");

    loraRxQueue_Centrale = xQueueCreate(10, LORA_RX_PACKET_MAX_LEN);
    nodeListMutex_Centrale = xSemaphoreCreateMutex();

    if(!LittleFS.begin()){
        Serial.println("An Error has occurred while mounting LittleFS");
        return;
    }

    setupLoRa();
    setupWebServer();
    startTasks();

    Serial.println("Centrale Logic Initialized.");
}

void CentraleLogic::setupLoRa() {
    SPI.begin(LORA_SCK_PIN, LORA_MISO_PIN, LORA_MOSI_PIN);
    LoRa.setPins(LORA_SS_PIN, LORA_RST_PIN, LORA_DIO0_PIN);
    if (!LoRa.begin(433E6)) {
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

void CentraleLogic::startTasks() {
    xTaskCreate(Task_LoRa_Handler, "LoRaHandler", 4096, this, 3, NULL);
    xTaskCreate(Task_Node_Janitor, "NodeJanitor", 2048, this, 1, NULL);
    xTaskCreate(Task_SSE_Publisher, "SSEPublisher", 4096, this, 2, NULL);
}

void CentraleLogic::setupWebServer() {
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(LittleFS, "/index_centrale.html", "text/html");
    });

    // Endpoint pour récupérer l'état complet du système en JSON
    server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "application/json", instance->getSystemStatusJson());
    });

    server.on("/api/assign", HTTP_POST, [](AsyncWebServerRequest *request){
        if (request->hasParam("reservoirId", true) && request->hasParam("wellId", true)) {
            String reservoirId = request->getParam("reservoirId", true)->value();
            String wellId = request->getParam("wellId", true)->value();

            if (xSemaphoreTake(nodeListMutex_Centrale, portMAX_DELAY) == pdTRUE) {
                bool isShared = false;
                for (int i = 0; i < instance->nodeCount; i++) {
                    if (instance->nodeList[i].id.equals(reservoirId)) {
                        instance->nodeList[i].assignedTo = wellId;
                        break;
                    }
                }

                int assignments = 0;
                for (int i = 0; i < instance->nodeCount; i++) {
                    if (instance->nodeList[i].type == ROLE_AQUA_RESERV_PRO && instance->nodeList[i].assignedTo.equals(wellId)) {
                        assignments++;
                    }
                }
                isShared = (assignments > 1);

                for (int i = 0; i < instance->nodeCount; i++) {
                    if (instance->nodeList[i].type == ROLE_AQUA_RESERV_PRO && instance->nodeList[i].assignedTo.equals(wellId)) {
                        StaticJsonDocument<256> cmdDoc;
                        cmdDoc["type"] = MessageType::COMMAND;
                        cmdDoc["tgt"] = instance->nodeList[i].id;
                        cmdDoc["cmd"] = "ASSIGN_WELL";
                        cmdDoc["well_id"] = wellId;
                        cmdDoc["is_shared"] = isShared;
                        String packet;
                        serializeJson(cmdDoc, packet);
                        sendLoRaMessage(packet);
                    }
                }
                xSemaphoreGive(nodeListMutex_Centrale);
            }
            request->send(200, "text/plain", "Assignment updated.");
        } else {
            request->send(400, "text/plain", "Missing parameters.");
        }
    });

    server.on("/api/set-name", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
        StaticJsonDocument<128> doc;
        deserializeJson(doc, (const char*) data, len);
        String nodeId = doc["id"];
        String nodeName = doc["name"];

        if (nodeId.length() > 0) {
            if (xSemaphoreTake(nodeListMutex_Centrale, portMAX_DELAY) == pdTRUE) {
                for (int i = 0; i < instance->nodeCount; i++) {
                    if (instance->nodeList[i].id.equals(nodeId)) {
                        instance->nodeList[i].name = nodeName;
                        instance->saveNodeName(nodeId, nodeName);
                        break;
                    }
                }
                xSemaphoreGive(nodeListMutex_Centrale);
            }
            request->send(200, "text/plain", "Name updated.");
        } else {
            request->send(400, "text/plain", "Invalid request.");
        }
    });

    server.addHandler(&events);
    server.begin();
}


// --- FreeRTOS Tasks ---

void CentraleLogic::Task_LoRa_Handler(void *pvParameters) {
    WatchdogManager::registerTask();
    char packetBuffer[LORA_RX_PACKET_MAX_LEN];
    const TickType_t xTicksToWait = pdMS_TO_TICKS(1000); // Wait for 1 second
    for (;;) {
        WatchdogManager::pet();
        if (xQueueReceive(loraRxQueue_Centrale, &packetBuffer, xTicksToWait) == pdPASS) {
            String fullPacket(packetBuffer);
            int separatorIndex = fullPacket.lastIndexOf('\1');
            String encryptedPacket = fullPacket.substring(0, separatorIndex);
            int rssi = fullPacket.substring(separatorIndex + 1).toInt();

            String decryptedPacket = CryptoManager::decrypt(encryptedPacket);
            if (decryptedPacket.length() > 0) {
                handleLoRaPacket(decryptedPacket, rssi);
            }
        }
    }
}

void CentraleLogic::Task_Node_Janitor(void *pvParameters) {
    const long NODE_TIMEOUT_MS = 300000; // 5 minutes
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(30000)); // Run every 30 seconds
        if (xSemaphoreTake(nodeListMutex_Centrale, portMAX_DELAY) == pdTRUE) {
            unsigned long currentTime = millis();
            for (int i = 0; i < instance->nodeCount; i++) {
                if (instance->nodeList[i].status != "DISCONNECTED" && (currentTime - instance->nodeList[i].lastSeen > NODE_TIMEOUT_MS)) {
                    instance->nodeList[i].status = "DISCONNECTED";
                    Serial.printf("Node %s timed out.\n", instance->nodeList[i].id.c_str());
                }
            }
            xSemaphoreGive(nodeListMutex_Centrale);
        }
    }
}

void CentraleLogic::Task_SSE_Publisher(void* pvParameters) {
    for(;;) {
        vTaskDelay(pdMS_TO_TICKS(2000)); // Push updates every 2 seconds
        instance->events.send(instance->getSystemStatusJson().c_str(), "update", millis());
    }
}


// --- Logic Methods ---

void CentraleLogic::registerOrUpdateNode(const String& id, NodeRole role, const String& status, int rssi) {
    if (xSemaphoreTake(nodeListMutex_Centrale, portMAX_DELAY) == pdTRUE) {
        int existingNodeIndex = -1;
        for (int i = 0; i < nodeCount; i++) {
            if (nodeList[i].id.equals(id)) {
                existingNodeIndex = i;
                break;
            }
        }

        if (existingNodeIndex != -1) { // Update existing node
            nodeList[existingNodeIndex].lastSeen = millis();
            nodeList[existingNodeIndex].rssi = rssi;
            nodeList[existingNodeIndex].status = status;
            if (role != ROLE_UNKNOWN) nodeList[existingNodeIndex].type = role;
        } else if (nodeCount < MAX_NODES) { // Add new node
            nodeList[nodeCount].id = id;
            nodeList[nodeCount].name = loadNodeName(id);
            nodeList[nodeCount].type = role;
            nodeList[nodeCount].lastSeen = millis();
            nodeList[nodeCount].rssi = rssi;
            nodeList[nodeCount].status = status;
            nodeList[nodeCount].assignedTo = "";
            nodeCount++;
        }
        xSemaphoreGive(nodeListMutex_Centrale);
    }
}

void CentraleLogic::handlePumpRequest(const String& requesterId, MessageType requestType) {
    if (xSemaphoreTake(nodeListMutex_Centrale, portMAX_DELAY) != pdTRUE) return;

    String wellId = "";
    for (int i = 0; i < nodeCount; i++) {
        if (nodeList[i].id.equals(requesterId)) {
            wellId = nodeList[i].assignedTo;
            break;
        }
    }

    if (wellId.isEmpty()) {
        xSemaphoreGive(nodeListMutex_Centrale);
        return;
    }

    if (requestType == REQUEST_PUMP_ON) {
        bool isAnyReservoirFull = false;
        for (int i = 0; i < nodeCount; i++) {
            if (nodeList[i].type == ROLE_AQUA_RESERV_PRO && nodeList[i].assignedTo.equals(wellId) && nodeList[i].status.equalsIgnoreCase("FULL")) {
                isAnyReservoirFull = true;
                break;
            }
        }
        if (!isAnyReservoirFull) {
            String cmdPkt = LoRaMessage::serializeCommand("", wellId.c_str(), CMD_PUMP_ON);
            sendLoRaMessage(cmdPkt);
        }
    } else if (requestType == REQUEST_PUMP_OFF) {
        bool isAnotherReservoirEmpty = false;
        for (int i = 0; i < nodeCount; i++) {
            if (nodeList[i].type == ROLE_AQUA_RESERV_PRO && nodeList[i].assignedTo.equals(wellId) && !nodeList[i].id.equals(requesterId) && nodeList[i].status.equalsIgnoreCase("EMPTY")) {
                isAnotherReservoirEmpty = true;
                break;
            }
        }
        if (!isAnotherReservoirEmpty) {
            String cmdPkt = LoRaMessage::serializeCommand("", wellId.c_str(), CMD_PUMP_OFF);
            sendLoRaMessage(cmdPkt);
        }
    }

    xSemaphoreGive(nodeListMutex_Centrale);
}


String CentraleLogic::getSystemStatusJson() {
    String output = "{}";
    if (xSemaphoreTake(nodeListMutex_Centrale, pdMS_TO_TICKS(1000)) == pdTRUE) {
        StaticJsonDocument<2048> doc;
        JsonArray nodes = doc.createNestedArray("nodes");
        for (int i = 0; i < nodeCount; i++) {
            JsonObject node = nodes.createNestedObject();
            node["id"] = nodeList[i].id;
            node["name"] = nodeList[i].name;
            node["type"] = (int)nodeList[i].type;
            node["rssi"] = nodeList[i].rssi;
            node["status"] = nodeList[i].status;
            node["lastSeen"] = nodeList[i].lastSeen;
            node["assignedTo"] = nodeList[i].assignedTo;
        }
        serializeJson(doc, output);
        xSemaphoreGive(nodeListMutex_Centrale);
    }
    return output;
}

void CentraleLogic::saveNodeName(const String& nodeId, const String& nodeName) {
    Preferences prefs;
    prefs.begin("node-names", false);
    prefs.putString(nodeId.c_str(), nodeName);
    prefs.end();
}

String CentraleLogic::loadNodeName(const String& nodeId) {
    Preferences prefs;
    prefs.begin("node-names", false); // Open in read-write mode to create if not exists
    String name = prefs.getString(nodeId.c_str(), "");
    prefs.end();
    return name;
}

// --- LoRa Static Methods ---

void CentraleLogic::onReceive(int packetSize) {
    if (packetSize == 0 || packetSize > LORA_RX_PACKET_MAX_LEN) return;

    ledManager.setTemporaryState(LORA_ACTIVITY, 500);

    char packetBuffer[LORA_RX_PACKET_MAX_LEN];
    int len = 0;
    while (LoRa.available()) packetBuffer[len++] = (char)LoRa.read();
    packetBuffer[len] = '\0';
    snprintf(packetBuffer + len, sizeof(packetBuffer) - len, "\1%d", LoRa.packetRssi());
    xQueueSendFromISR(loraRxQueue_Centrale, &packetBuffer, NULL);
}

void CentraleLogic::handleLoRaPacket(const String& packet, int rssi) {
    StaticJsonDocument<256> doc;
    deserializeJson(doc, packet);
    MessageType type = (MessageType)doc["type"].as<int>();
    String id = doc.containsKey("id") ? doc["id"].as<String>() : doc["src"].as<String>();

    switch (type) {
        case DISCOVERY:
            instance->registerOrUpdateNode(id, (NodeRole)doc["role"].as<int>(), "Discovered", rssi);
            break;
        case STATUS_UPDATE:
            instance->registerOrUpdateNode(id, ROLE_UNKNOWN, doc["status"].as<String>(), rssi);
            break;
        case REQUEST_PUMP_ON:
        case REQUEST_PUMP_OFF:
            instance->handlePumpRequest(id, type);
            break;
        default:
            break;
    }
}

void CentraleLogic::sendLoRaMessage(const String& message) {
    ledManager.setTemporaryState(LORA_ACTIVITY, 500);

    String encrypted = CryptoManager::encrypt(message);
    LoRa.beginPacket();
    LoRa.print(encrypted);
    LoRa.endPacket();
    Serial.printf("Sent LoRa Packet: %s\n", message.c_str());
}
