#ifndef CENTRALE_LOGIC_H
#define CENTRALE_LOGIC_H

#include <Arduino.h>
#include <LoRa.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include "Message.h"
#include "config.h" // Utilisation de la configuration centralisée

#define MAX_NODES 16
#define LORA_RX_PACKET_MAX_LEN 256

struct Node {
    String id;
    String name;
    NodeRole type;
    long rssi;
    String status;
    bool pumpState;
    unsigned long lastSeen;
    String assignedTo; // For AquaReserv, stores the Wellguard ID it's assigned to
};

class CentraleLogic {
public:
    CentraleLogic();
    void initialize();

private:
    Node nodeList[MAX_NODES];
    int nodeCount = 0;
    AsyncWebServer server;
    AsyncEventSource events;
    String deviceId;

    void setupLoRa();
    void setupWebServer();
    void startTasks();

    void registerOrUpdateNode(const String& id, NodeRole role, const String& status, bool pumpState, int rssi);
    void handlePumpRequest(const String& requesterId, MessageType requestType);
    String getSystemStatusJson();
    void saveNodeName(const String& nodeId, const String& nodeName);

    // Static members to be accessed by ISR/callbacks
    static CentraleLogic* instance;
    static void onReceive(int packetSize);
    static void handleLoRaPacket(const String& packet, int rssi);
    static void sendLoRaMessage(const String& message);

    // FreeRTOS tasks and synchronization
    static void Task_LoRa_Handler(void *pvParameters);
    static void Task_Node_Janitor(void *pvParameters);
    static void Task_SSE_Publisher(void* pvParameters);
};

#endif // CENTRALE_LOGIC_H
