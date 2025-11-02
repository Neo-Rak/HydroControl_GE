#ifndef AQUA_RESERV_LOGIC_H
#define AQUA_RESERV_LOGIC_H

#include <Arduino.h>
#include <LoRa.h>
#include <Preferences.h>
#include "Message.h"

// Hardware configuration for AquaReservPro role
#define ARP_LORA_SS_PIN    5
#define ARP_LORA_RST_PIN   14
#define ARP_LORA_DIO0_PIN  2
#define ARP_LORA_FREQ      433E6

#define ARP_LEVEL_SENSOR_PIN 23
#define ARP_BUTTON_PIN       22

// Logic configuration
#define SENSOR_STABILITY_MS 5000

// State enumerations
enum OperatingMode { AUTO, MANUAL };
enum LevelState { LEVEL_EMPTY, LEVEL_FULL, LEVEL_UNKNOWN };

class AquaReservLogic {
public:
    AquaReservLogic();
    void initialize();

private:
    String deviceId;
    String assignedWellId = "";
    bool isWellShared = false;
    OperatingMode currentMode = AUTO;
    LevelState currentLevel = LEVEL_UNKNOWN;
    bool currentPumpCommand = false;
    volatile unsigned long lastLoRaTransmissionTimestamp = 0;

    void setupHardware();
    void setupLoRa();
    void startTasks();
    void loadOperationalConfig();
    void saveOperationalConfig();
    void triggerPumpCommand(bool command);

    static void onReceive(int packetSize);
    static void handleLoRaPacket(const String& packet);
    static void sendLoRaMessage(const String& message);
    bool sendReliableCommand(const String& packet);

    static AquaReservLogic* instance;

    // FreeRTOS task prototypes
    static void Task_Control_Logic(void *pvParameters);
    static void Task_Sensor_Handler(void *pvParameters);
    static void Task_GPIO_Handler(void *pvParameters);
    static void Task_Status_Reporter(void *pvParameters);
};

#endif // AQUA_RESERV_LOGIC_H
