#include "WatchdogManager.h"
#include <esp_task_wdt.h>

void WatchdogManager::initialize(uint32_t timeout_s) {
    Serial.println("Initializing Task Watchdog Timer...");
    // Initialise le TWDT avec le timeout spécifié et indique que le système
    // doit paniquer (et donc redémarrer) si le timeout est atteint.
    esp_task_wdt_init(timeout_s, true);
}

void WatchdogManager::registerTask() {
    // Récupère le handle de la tâche FreeRTOS en cours d'exécution
    TaskHandle_t currentTaskHandle = xTaskGetCurrentTaskHandle();

    // Enregistre la tâche auprès du TWDT. Si l'enregistrement échoue,
    // une erreur est affichée.
    if (esp_task_wdt_add(currentTaskHandle) != ESP_OK) {
        Serial.printf("Failed to register task '%s' with watchdog!\n", pcTaskGetTaskName(currentTaskHandle));
    } else {
        Serial.printf("Task '%s' registered with watchdog.\n", pcTaskGetTaskName(currentTaskHandle));
    }
}

void WatchdogManager::pet() {
    // Réinitialise le timer du watchdog pour la tâche en cours.
    esp_task_wdt_reset();
}
