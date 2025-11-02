#ifndef AQUA_RESERV_LOGIC_H
#define AQUA_RESERV_LOGIC_H

#include <Arduino.h>
#include <LoRa.h>
#include <Preferences.h>
#include "Message.h"
#include "config.h" // Utilisation de la configuration centralisée

// Logic configuration
#define SENSOR_STABILITY_MS 2000 // Temps en ms avant de considérer un état de capteur comme stable

// State enumerations
enum OperatingMode { AUTO, MANUAL };
enum LevelState { LEVEL_EMPTY, LEVEL_OK, LEVEL_FULL, LEVEL_ERROR }; // Ajout de l'état OK et ERROR

class AquaReservLogic {
public:
    AquaReservLogic();
    void initialize();

private:
    String deviceId;
    String assignedWellId = "";
    bool isWellShared = false;
    OperatingMode currentMode = AUTO;
    LevelState currentLevel = LEVEL_OK; // Initialiser à OK
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
