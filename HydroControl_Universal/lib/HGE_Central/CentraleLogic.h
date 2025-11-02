/**
 * @file CentraleLogic.h
 * @author Jules
 * @brief Defines the logic for the Centrale module of the HydroControl-GE system.
 * @version 3.1.0
 * @date 2025-11-02
 *
 * @copyright Copyright (c) 2025
 *
 * This file contains the declaration of the CentraleLogic class, which is responsible
 * for orchestrating the entire HydroControl-GE network. It manages LoRa communication,
 * hosts a web server for monitoring and control, and makes decisions about shared resources.
 */

#ifndef CENTRALE_LOGIC_H
#define CENTRALE_LOGIC_H

#include <Arduino.h>
#include <LoRa.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include "Message.h"
#include "config.h"

#define MAX_NODES 16
#define LORA_RX_PACKET_MAX_LEN 256

/**
 * @struct Node
 * @brief Represents a single remote device (node) in the HydroControl-GE network.
 *
 * This structure holds all relevant information about a discovered node, including its
 * identity, state, and relationship with other nodes.
 */
struct Node {
    String id;          ///< Unique identifier of the node (MAC address).
    String name;        ///< User-defined alias for the node.
    NodeRole type;      ///< The role of the node (e.g., AQUA_RESERV_PRO).
    long rssi;          ///< Received Signal Strength Indicator from the last message.
    String status;      ///< Current operational status (e.g., "FULL", "EMPTY").
    bool pumpState;     ///< The current state of the pump controlled by the node (true=ON).
    unsigned long lastSeen; ///< Timestamp (millis) of the last received message.
    String assignedTo;  ///< For AquaReserv, stores the Wellguard ID it's assigned to.
};

/**
 * @class CentraleLogic
 * @brief Core logic handler for the Centrale device role.
 *
 * This class encapsulates all functionality for the central controller. It initializes
 * and manages the LoRa radio, the web server, and multiple FreeRTOS tasks for
 * concurrent operations like handling incoming LoRa packets, managing node lifecycle,
 * and publishing server-sent events (SSE) to the web UI.
 */
class CentraleLogic {
public:
    /**
     * @brief Construct a new CentraleLogic object.
     */
    CentraleLogic();

    /**
     * @brief Initializes the Centrale module.
     *
     * This method sets up Wi-Fi, LoRa, the web server, and starts all
     * the necessary FreeRTOS tasks. It is the main entry point for this class.
     */
    void initialize();

private:
    Node nodeList[MAX_NODES];       ///< Array to store all discovered nodes.
    int nodeCount = 0;              ///< Current number of discovered nodes.
    AsyncWebServer server;          ///< The asynchronous web server object.
    AsyncEventSource events;        ///< Server-Sent Events source for live UI updates.
    String deviceId;                ///< The unique ID (MAC address) of this Centrale device.

    // --- Setup Methods ---

    /**
     * @brief Configures and initializes the LoRa module.
     */
    void setupLoRa();

    /**
     * @brief Sets up the web server endpoints and event source.
     */
    void setupWebServer();

    /**
     * @brief Creates and starts all FreeRTOS tasks managed by this class.
     */
    void startTasks();

    // --- Core Logic Methods ---

    /**
     * @brief Registers a new node or updates an existing one.
     * @param id The unique ID of the node.
     * @param role The role of the node.
     * @param status The current status of the node.
     * @param pumpState The current pump state of the node.
     * @param rssi The RSSI of the received packet.
     */
    void registerOrUpdateNode(const String& id, NodeRole role, const String& status, bool pumpState, int rssi);

    /**
     * @brief Handles a pump request from an AquaReservPro module for a shared well.
     * @param requesterId The ID of the module requesting the pump action.
     * @param requestType The type of request (REQUEST_PUMP_ON or REQUEST_PUMP_OFF).
     */
    void handlePumpRequest(const String& requesterId, MessageType requestType);

    /**
     * @brief Generates a JSON string representing the complete status of the system.
     * @return A JSON formatted String.
     */
    String getSystemStatusJson();

    /**
     * @brief Saves a user-defined name for a node to non-volatile storage.
     * @param nodeId The ID of the node.
     * @param nodeName The name to save.
     */
    void saveNodeName(const String& nodeId, const String& nodeName);

    // --- Static callbacks & ISR handlers ---

    static CentraleLogic* instance; ///< Static instance pointer for callbacks.

    /**
     * @brief ISR-safe callback function for LoRa packet reception.
     * @param packetSize The size of the received packet.
     */
    static void onReceive(int packetSize);

    /**
     * @brief Processes a decrypted LoRa packet. Called from a FreeRTOS task.
     * @param packet The decrypted packet content.
     * @param rssi The RSSI of the packet.
     */
    static void handleLoRaPacket(const String& packet, int rssi);

    /**
     * @brief Encrypts and sends a message via LoRa.
     * @param message The plaintext message to send.
     */
    static void sendLoRaMessage(const String& message);

    // --- FreeRTOS Tasks ---

    /**
     * @brief Task to handle incoming LoRa packets from a queue.
     * @param pvParameters Pointer to the CentraleLogic instance.
     */
    static void Task_LoRa_Handler(void *pvParameters);

    /**
     * @brief Task to periodically check for disconnected nodes.
     * @param pvParameters Pointer to the CentraleLogic instance.
     */
    static void Task_Node_Janitor(void *pvParameters);

    /**
     * @brief Task to periodically publish system status via SSE.
     * @param pvParameters Pointer to the CentraleLogic instance.
     */
    static void Task_SSE_Publisher(void* pvParameters);
};

#endif // CENTRALE_LOGIC_H
