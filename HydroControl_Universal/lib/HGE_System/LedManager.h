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
    LORA_ACTIVITY,
    ACTION_IN_PROGRESS,
    WARNING,
    CRITICAL_ERROR
};

class LedManager {
public:
    LedManager(int redPin, int greenPin, int yellowPin);
    void begin();
    void setState(LedState newState);
    void setTemporaryState(LedState tempState, uint32_t durationMs);

private:
    // Pins
    int _redPin;
    int _greenPin;
    int _yellowPin;

    // FreeRTOS
    TaskHandle_t _ledTaskHandle;
    QueueHandle_t _ledStateQueue;
    volatile LedState _currentState;

    static void ledTask(void* parameter);
};
