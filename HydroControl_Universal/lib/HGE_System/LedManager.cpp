#include "LedManager.h"
#include "config.h" // Inclure pour avoir accès aux pins des LEDs

// Global instance declaration
LedManager ledManager(LED_RED_PIN, LED_GREEN_PIN, LED_BLUE_PIN);

// Constructeur
LedManager::LedManager(int redPin, int greenPin, int bluePin)
    : _redPin(redPin), _greenPin(greenPin), _bluePin(bluePin),
      _ledTaskHandle(NULL), _ledStateQueue(NULL), _currentState(OFF) {}

// Initialisation
void LedManager::begin() {
    // Setup LEDC PWM channels
    ledcSetup(_redChannel, 5000, 8); // 5 kHz PWM, 8-bit resolution
    ledcSetup(_greenChannel, 5000, 8);
    ledcSetup(_blueChannel, 5000, 8);

    // Attach pins to channels
    ledcAttachPin(_redPin, _redChannel);
    ledcAttachPin(_greenPin, _greenChannel);
    ledcAttachPin(_bluePin, _blueChannel);

    // Initial state: OFF
    setColor(0, 0, 0);

    _ledStateQueue = xQueueCreate(10, sizeof(LedState)); // Increased queue size

    xTaskCreate(
        ledTask,
        "LedTask",
        2048,
        this,
        1,
        &_ledTaskHandle
    );
}

// Changer l'état de la LED
void LedManager::setState(LedState newState) {
    xQueueSend(_ledStateQueue, &newState, portMAX_DELAY);
}

// Tâche FreeRTOS pour gérer les LEDs de manière non-bloquante
void LedManager::ledTask(void* parameter) {
    LedManager* self = (LedManager*)parameter;
    LedState receivedState;

    while (true) {
        // Attendre un nouvel état de la file d'attente
        if (xQueueReceive(self->_ledStateQueue, &receivedState, 100 / portTICK_PERIOD_MS)) {
            self->_currentState = receivedState;
        }

        // Logique des états
        switch (self->_currentState) {
            case OFF:
                self->setColor(0, 0, 0); // Éteint
                break;

            case BOOTING:
                // Blanc clignotant lentement (200ms on, 800ms off)
                self->setColor(255, 255, 255);
                vTaskDelay(200 / portTICK_PERIOD_MS);
                self->setColor(0, 0, 0);
                vTaskDelay(800 / portTICK_PERIOD_MS);
                break;

            case SYSTEM_OK:
                self->setColor(0, 255, 0); // Vert fixe
                break;

            case SETUP_MODE:
                self->setColor(0, 0, 255); // Bleu fixe
                break;

            case LORA_TRANSMITTING:
                // Cyan clignotant (100ms on, 100ms off)
                self->setColor(0, 255, 255);
                vTaskDelay(100 / portTICK_PERIOD_MS);
                self->setColor(0, 0, 0);
                vTaskDelay(100 / portTICK_PERIOD_MS);
                break;

            case LORA_RECEIVING:
                // Magenta clignotant (100ms on, 100ms off)
                self->setColor(255, 0, 255);
                vTaskDelay(100 / portTICK_PERIOD_MS);
                self->setColor(0, 0, 0);
                vTaskDelay(100 / portTICK_PERIOD_MS);
                break;

            case ACTION_IN_PROGRESS:
                // Jaune clignotant (500ms on, 500ms off)
                self->setColor(255, 255, 0);
                vTaskDelay(500 / portTICK_PERIOD_MS);
                self->setColor(0, 0, 0);
                vTaskDelay(500 / portTICK_PERIOD_MS);
                break;

            case WARNING:
                // Orange clignotant (250ms on, 250ms off)
                self->setColor(255, 165, 0);
                vTaskDelay(250 / portTICK_PERIOD_MS);
                self->setColor(0, 0, 0);
                vTaskDelay(250 / portTICK_PERIOD_MS);
                break;

            case CRITICAL_ERROR:
                // Rouge clignotant rapidement (100ms on, 100ms off)
                self->setColor(255, 0, 0);
                vTaskDelay(100 / portTICK_PERIOD_MS);
                self->setColor(0, 0, 0);
                vTaskDelay(100 / portTICK_PERIOD_MS);
                break;

            default:
                self->setColor(0, 0, 0);
                break;
        }
    }
}

// Fonction utilitaire pour définir la couleur
void LedManager::setColor(int red, int green, int blue) {
    ledcWrite(_redChannel, red);
    ledcWrite(_greenChannel, green);
    ledcWrite(_blueChannel, blue);
}
