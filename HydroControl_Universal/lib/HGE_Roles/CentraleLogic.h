#ifndef CENTRALE_LOGIC_H
#define CENTRALE_LOGIC_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include "Message.h"

// Hardware configuration
#define CENTRALE_LORA_SS_PIN    5
#define CENTRALE_LORA_RST_PIN   14
#define CENTRALE_LORA_DIO0_PIN  2
#define CENTRALE_LORA_FREQ      433E6

// System limits
#define MAX_NODES 32

// Node Data Structure
struct Node {
    String id;
    String name;
    NodeRole type;
    long lastSeen;
    int rssi;
    String status;
    String assignedTo;
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

    void setupHardware();
    void setupLoRa();
    void setupWebServer();
    void startTasks();

    void registerOrUpdateNode(const String& id, NodeRole role, const String& status, int rssi);
    void handlePumpRequest(const String& requesterId, MessageType requestType);
    String getSystemStatusJson();
    void saveNodeName(const String& nodeId, const String& nodeName);
    String loadNodeName(const String& nodeId);

    static void onReceive(int packetSize);
    static void handleLoRaPacket(const String& packet, int rssi);
    static void sendLoRaMessage(const String& message);

    static CentraleLogic* instance;

    // FreeRTOS task prototypes
    static void Task_Node_Janitor(void *pvParameters);
    static void Task_LoRa_Handler(void *pvParameters);
    static void Task_SSE_Publisher(void* pvParameters);
};

#endif // CENTRALE_LOGIC_H
