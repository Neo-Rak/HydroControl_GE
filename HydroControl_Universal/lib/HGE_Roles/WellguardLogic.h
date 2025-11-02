#ifndef WELLGUARD_LOGIC_H
#define WELLGUARD_LOGIC_H

#include <Arduino.h>
#include <LoRa.h>
#include <Preferences.h>
#include "Message.h"

// Hardware configuration for WellguardPro role
#define WGP_LORA_SS_PIN    5
#define WGP_LORA_RST_PIN   14
#define WGP_LORA_DIO0_PIN  2
#define WGP_LORA_FREQ      433E6

#define WGP_RELAY_PIN      23

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
