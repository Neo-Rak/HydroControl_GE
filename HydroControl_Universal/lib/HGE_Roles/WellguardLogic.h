#ifndef WELLGUARD_LOGIC_H
#define WELLGUARD_LOGIC_H

#include <Arduino.h>
#include <LoRa.h>
#include <Preferences.h>
#include "Message.h"
#include "config.h" // Utilisation de la configuration centralisée

class WellguardLogic {
public:
    WellguardLogic();
    void initialize();

private:
    String deviceId;
    volatile bool relayState = false;
    volatile long lastCommandRssi = 0;
    volatile unsigned long lastLoRaTransmissionTimestamp = 0;

    void setupHardware();
    void setupLoRa();
    void startTasks();

    static void onReceive(int packetSize);
    static void handleLoRaPacket(const String& packet);
    static void sendLoRaMessage(const String& message);
    void setRelayState(bool newState);

    // Static members to be accessed by ISR
    static WellguardLogic* instance;

    // FreeRTOS tasks
    static void Task_Status_Reporter(void *pvParameters);
};

#endif // WELLGUARD_LOGIC_H
