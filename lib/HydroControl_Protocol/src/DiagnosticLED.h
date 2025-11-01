#pragma once
#include <Arduino.h>

// --- États des LEDs ---
// Enum partagé pour tous les modules
enum LED_State {
    INIT,
    CONFIG_MODE,
    OPERATIONAL,
    LORA_ACTIVITY,
    CONNECTIVITY_ERROR,
    CRITICAL_ERROR
};

// --- API du Module ---
namespace DiagnosticLED {
    // Initialise le matériel et la file d'attente (queue)
    QueueHandle_t initialize(int redPin, int yellowPin, int bluePin);

    // Tâche FreeRTOS à lancer dans le `setup` de chaque module
    void Task_LED_Manager(void *pvParameters);
}
