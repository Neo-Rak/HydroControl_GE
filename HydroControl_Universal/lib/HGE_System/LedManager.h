#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

class LedManager; // Forward declaration
extern LedManager ledManager;

enum LedState {
    OFF,
    BOOTING,
    SYSTEM_OK,
    SETUP_MODE,
    LORA_TRANSMITTING,
    LORA_RECEIVING,
    ACTION_IN_PROGRESS,
    WARNING,
    CRITICAL_ERROR
};

class LedManager {
public:
    LedManager(int redPin, int greenPin, int bluePin);
    void begin();
    void setState(LedState newState);

private:
    // Pins
    int _redPin;
    int _greenPin;
    int _bluePin;

    // LEDC PWM Channels
    const int _redChannel = 0;
    const int _greenChannel = 1;
    const int _blueChannel = 2;

    // FreeRTOS
    TaskHandle_t _ledTaskHandle;
    QueueHandle_t _ledStateQueue;
    volatile LedState _currentState;

    static void ledTask(void* parameter);
    void setColor(int red, int green, int blue);
};
