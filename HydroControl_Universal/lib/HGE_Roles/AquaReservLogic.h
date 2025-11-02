/**
 * @file AquaReservLogic.h
 * @author Jules
 * @brief Defines the logic for the AquaReservPro module of the HydroControl-GE system.
 * @version 3.1.0
 * @date 2025-11-02
 *
 * @copyright Copyright (c) 2025
 *
 * This file contains the declaration of the AquaReservLogic class, which is responsible
 * for managing a water reservoir. It monitors water levels, controls a pump, and
 * communicates with the central controller (Centrale) or directly with a well pump.
 */

#ifndef AQUA_RESERV_LOGIC_H
#define AQUA_RESERV_LOGIC_H

#include <Arduino.h>
#include <LoRa.h>
#include <Preferences.h>
#include "Message.h"
#include "config.h"

// Logic configuration
#define SENSOR_STABILITY_MS 2000 ///< Time in ms to wait before considering a sensor state as stable.

// State enumerations

/**
 * @enum OperatingMode
 * @brief Defines the operational mode of the AquaReservPro.
 */
enum OperatingMode {
    AUTO,   ///< The device operates autonomously based on sensor readings.
    MANUAL  ///< The user controls the pump via the manual override button.
};

/**
 * @enum LevelState
 * @brief Defines the possible water level states of the reservoir.
 */
enum LevelState {
    LEVEL_EMPTY, ///< The reservoir is empty.
    LEVEL_OK,    ///< The water level is between the high and low sensors.
    LEVEL_FULL,  ///< The reservoir is full.
    LEVEL_ERROR  ///< The sensor readings are inconsistent (e.g., high sensor active but low is not).
};


/**
 * @class AquaReservLogic
 * @brief Core logic handler for the AquaReservPro device role.
 *
 * This class encapsulates all functionality for a reservoir controller. It reads level
 * sensors, implements the control logic for a pump, handles manual override, and manages
 * all LoRa communication.
 */
class AquaReservLogic {
public:
    /**
     * @brief Construct a new AquaReservLogic object.
     */
    AquaReservLogic();

    /**
     * @brief Initializes the AquaReservPro module.
     *
     * This method sets up the hardware pins, LoRa, and starts all necessary
     * FreeRTOS tasks. It is the main entry point for this class.
     */
    void initialize();

private:
    String deviceId;            ///< Unique identifier of the node (MAC address).
    String assignedWellId = ""; ///< The ID of the well pump this reservoir is assigned to.
    bool isWellShared = false;  ///< True if the assigned well is shared with other reservoirs.
    bool isProvisioned = false; ///< True once the Centrale has acknowledged and configured this node.
    OperatingMode currentMode = AUTO; ///< The current operating mode (AUTO or MANUAL).
    LevelState currentLevel = LEVEL_OK; ///< The current stable water level.
    bool currentPumpCommand = false; ///< The current command state for the pump (true=ON).
    volatile unsigned long lastLoRaTransmissionTimestamp = 0; ///< Timestamp of the last LoRa Tx.

    // --- Setup Methods ---

    /**
     * @brief Configures the hardware pins for sensors and buttons.
     */
    void setupHardware();

    /**
     * @brief Configures and initializes the LoRa module.
     */
    void setupLoRa();

    /**
     * @brief Creates and starts all FreeRTOS tasks managed by this class.
     */
    void startTasks();

    // --- Configuration Persistence ---

    /**
     * @brief Loads the operational configuration from non-volatile storage.
     */
    void loadOperationalConfig();

    /**
     * @brief Saves the operational configuration to non-volatile storage.
     */
    void saveOperationalConfig();

    // --- Core Logic ---

    /**
     * @brief Triggers a pump command (ON/OFF).
     *
     * Depending on whether the assigned well is shared, this will either send a direct
     * command to the well or a request to the Centrale.
     * @param command The desired pump state (true for ON, false for OFF).
     */
    void triggerPumpCommand(bool command);

    // --- Static callbacks & ISR handlers ---

    /**
     * @brief ISR-safe callback function for LoRa packet reception.
     * @param packetSize The size of the received packet.
     */
    static void onReceive(int packetSize);

    /**
     * @brief Processes a decrypted LoRa packet.
     * @param packet The decrypted packet content.
     */
    static void handleLoRaPacket(const String& packet);

    /**
     * @brief Encrypts and sends a message via LoRa.
     * @param message The plaintext message to send.
     */
    static void sendLoRaMessage(const String& message);

    /**
     * @brief Sends a LoRa message and waits for an ACK, with retries.
     * @param packet The packet to send.
     * @return true if ACK was received, false otherwise.
     */
    bool sendReliableCommand(const String& packet);

    static AquaReservLogic* instance; ///< Static instance pointer for callbacks.

    // --- FreeRTOS Tasks ---

    /**
     * @brief Task that contains the main control logic (AUTO mode).
     * @param pvParameters Pointer to the AquaReservLogic instance.
     */
    static void Task_Control_Logic(void *pvParameters);

    /**
     * @brief Task that reads and debounces the level sensors.
     * @param pvParameters Pointer to the AquaReservLogic instance.
     */
    static void Task_Sensor_Handler(void *pvParameters);

    /**
     * @brief Task that handles GPIO inputs like the manual override button.
     * @param pvParameters Pointer to the AquaReservLogic instance.
     */
    static void Task_GPIO_Handler(void *pvParameters);

    /**
     * @brief Task that periodically sends discovery or status update messages.
     * @param pvParameters Pointer to the AquaReservLogic instance.
     */
    static void Task_Status_Reporter(void *pvParameters);
};

#endif // AQUA_RESERV_LOGIC_H
