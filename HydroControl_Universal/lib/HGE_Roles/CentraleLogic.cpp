#include "CentraleLogic.h"
#include <WiFi.h>
#include <SPI.h>
#include <LoRa.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <LittleFS.h>
#include "Crypto.h"

CentraleLogic* CentraleLogic::instance = nullptr;

// --- FreeRTOS Handles ---
QueueHandle_t loraRxQueue_Centrale;
SemaphoreHandle_t nodeListMutex_Centrale;
#define LORA_RX_QUEUE_SIZE 10
#define LORA_RX_PACKET_MAX_LEN 256

CentraleLogic::CentraleLogic() : server(80), events("/events") {
    instance = this;
}

void CentraleLogic::initialize() {
    Serial.println("Centrale Logic Initializing...");

    nodeListMutex_Centrale = xSemaphoreCreateMutex();
    loraRxQueue_Centrale = xQueueCreate(LORA_RX_QUEUE_SIZE, sizeof(char[LORA_RX_PACKET_MAX_LEN]));

    setupHardware();

    // Load WiFi credentials from provisioning
    Preferences netPrefs;
    netPrefs.begin("network_config", true);
    String ssid = netPrefs.getString("ssid", "");
    String password = netPrefs.getString("password", "");
    netPrefs.end();

    if(ssid.length() > 0) {
        WiFi.mode(WIFI_STA);
        WiFi.begin(ssid.c_str(), password.c_str());
        Serial.print("Connecting to WiFi...");
        int retries = 0;
        while (WiFi.status() != WL_CONNECTED && retries < 20) {
            delay(500);
            Serial.print(".");
            retries++;
        }
        if(WiFi.status() == WL_CONNECTED){
            Serial.println("\nWiFi Connected. IP: " + WiFi.localIP().toString());
        } else {
            Serial.println("\nWiFi connection failed. Functionality will be limited.");
        }
    } else {
        Serial.println("No WiFi credentials found.");
    }

    if(!LittleFS.begin()){
        Serial.println("An Error has occurred while mounting LittleFS");
    }

    setupLoRa();
    setupWebServer();
    startTasks();

    Serial.println("Centrale Logic Initialized.");
}

void CentraleLogic::setupHardware() {
    // LEDs will be managed by a common manager
}

void CentraleLogic::setupLoRa() {
    SPI.begin();
    LoRa.setPins(CENTRALE_LORA_SS_PIN, CENTRALE_LORA_RST_PIN, CENTRALE_LORA_DIO0_PIN);
    if (!LoRa.begin(CENTRALE_LORA_FREQ)) {
        Serial.println("Starting LoRa failed!");
        while (1);
    }

    Preferences secPrefs;
    secPrefs.begin("security_config", true);
    String psk = secPrefs.getString("lora_psk", "");
    secPrefs.end();

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
    xTaskCreate(Task_SSE_Publisher, "SSEPublisher", 2048, this, 2, NULL);
}

void CentraleLogic::setupWebServer() {
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(LittleFS, "/index_centrale.html", "text/html");
    });

    // Route to get system status
    server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "application/json", instance->getSystemStatusJson());
    });

    // Route to assign a reservoir to a well
    server.on("/api/assign", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {

        StaticJsonDocument<128> doc;
        deserializeJson(doc, (const char*)data, len);
        String reservoirId = doc["reservoirId"];
        String wellId = doc["wellId"];

        if (reservoirId.length() > 0 && wellId.length() > 0) {
            bool isShared = false;
            if (xSemaphoreTake(nodeListMutex_Centrale, portMAX_DELAY) == pdTRUE) {
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

    // Route to set a node's name
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
    char packetBuffer[LORA_RX_PACKET_MAX_LEN];
    for (;;) {
        if (xQueueReceive(loraRxQueue_Centrale, &packetBuffer, portMAX_DELAY) == pdPASS) {
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
    prefs.begin("node-names", true);
    String name = prefs.getString(nodeId.c_str(), "");
    prefs.end();
    return name;
}

// --- LoRa Static Methods ---

void CentraleLogic::onReceive(int packetSize) {
    if (packetSize == 0 || packetSize > LORA_RX_PACKET_MAX_LEN) return;
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
    String encrypted = CryptoManager::encrypt(message);
    LoRa.beginPacket();
    LoRa.print(encrypted);
    LoRa.endPacket();
    Serial.printf("Sent LoRa Packet: %s\n", message.c_str());
}
