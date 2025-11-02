#include "LedManager.h"
#include "config.h"

// Global instance definition
LedManager ledManager(LED_RED_PIN, LED_GREEN_PIN, LED_YELLOW_PIN);

// Constructor
LedManager::LedManager(int redPin, int greenPin, int yellowPin)
    : _redPin(redPin), _greenPin(greenPin), _yellowPin(yellowPin),
      _ledTaskHandle(NULL), _ledStateQueue(NULL), _currentState(OFF) {}

// Initialisation
void LedManager::begin() {
    pinMode(_redPin, OUTPUT);
    pinMode(_greenPin, OUTPUT);
    pinMode(_yellowPin, OUTPUT);

    digitalWrite(_redPin, LOW);
    digitalWrite(_greenPin, LOW);
    digitalWrite(_yellowPin, LOW);

    _ledStateQueue = xQueueCreate(10, sizeof(LedState));

    xTaskCreate(
        ledTask,
        "LedTask",
        2048,
        this,
        1,
        &_ledTaskHandle
    );
}

// Set new state
void LedManager::setState(LedState newState) {
    // A temporary state should not overwrite the persistent state
    if (_currentState != LORA_ACTIVITY) {
        xQueueSend(_ledStateQueue, &newState, 0);
    }
}

void LedManager::setTemporaryState(LedState tempState, uint32_t durationMs) {
    // This is a simplified approach. A more robust implementation might queue temporary states.
    LedState persistentState = _currentState;
    setState(tempState);
    vTaskDelay(pdMS_TO_TICKS(durationMs));
    setState(persistentState);
}


// FreeRTOS Task
void LedManager::ledTask(void* parameter) {
    LedManager* self = (LedManager*)parameter;
    LedState receivedState;
    uint32_t tickCount = 0;

    while (true) {
        // Check for new state, but don't block
        if (xQueueReceive(self->_ledStateQueue, &receivedState, 0) == pdPASS) {
            self->_currentState = receivedState;
        }

        // Turn all LEDs off before setting the new pattern
        digitalWrite(self->_redPin, LOW);
        digitalWrite(self->_greenPin, LOW);
        digitalWrite(self->_yellowPin, LOW);

        tickCount++;

        // State logic
        switch (self->_currentState) {
            case OFF:
                // All off (already done)
                break;
            case BOOTING: // Yellow pulse
                if (tickCount % 10 < 5) digitalWrite(self->_yellowPin, HIGH);
                break;
            case SYSTEM_OK: // Green solid
                digitalWrite(self->_greenPin, HIGH);
                break;
            case SETUP_MODE: // Alternating Yellow/Green
                if (tickCount % 10 < 5) digitalWrite(self->_yellowPin, HIGH);
                else digitalWrite(self->_greenPin, HIGH);
                break;
            case LORA_ACTIVITY: // Green with Yellow flash
                digitalWrite(self->_greenPin, HIGH);
                if (tickCount % 4 < 2) digitalWrite(self->_yellowPin, HIGH);
                break;
            case ACTION_IN_PROGRESS: // Green breathing
                if (tickCount % 10 < 7) digitalWrite(self->_greenPin, HIGH);
                break;
            case WARNING: // Yellow solid
                digitalWrite(self->_yellowPin, HIGH);
                break;
            case CRITICAL_ERROR: // Red fast blink
                if (tickCount % 2 < 1) digitalWrite(self->_redPin, HIGH);
                break;
        }
        vTaskDelay(pdMS_TO_TICKS(100)); // Update every 100ms
    }
}
