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

    _ledStateQueue = xQueueCreate(10, sizeof(LedCommand));

    xTaskCreate(
        ledTask,
        "LedTask",
        2048,
        this,
        1,
        &_ledTaskHandle
    );
}

// Set a persistent state
void LedManager::setState(LedState newState) {
    LedCommand cmd = {newState, 0};
    xQueueSend(_ledStateQueue, &cmd, 0);
}

// Set a temporary state that reverts after a duration
void LedManager::setTemporaryState(LedState tempState, uint32_t durationMs) {
    LedCommand cmd = {tempState, durationMs};
    xQueueSend(_ledStateQueue, &cmd, 0);
}

// FreeRTOS Task
void LedManager::ledTask(void* parameter) {
    LedManager* self = (LedManager*)parameter;
    LedCommand receivedCommand;
    LedState persistentState = OFF;
    LedState renderState = OFF;
    uint32_t tempStateEndTime = 0;
    uint32_t tickCount = 0;

    while (true) {
        // Check for new commands without blocking
        if (xQueueReceive(self->_ledStateQueue, &receivedCommand, 0) == pdPASS) {
            if (receivedCommand.durationMs > 0) {
                // This is a temporary state
                renderState = receivedCommand.state;
                tempStateEndTime = millis() + receivedCommand.durationMs;
            } else {
                // This is a new persistent state
                persistentState = receivedCommand.state;
                // Only apply it if no temporary state is active
                if (millis() >= tempStateEndTime) {
                    renderState = persistentState;
                }
            }
        }

        // If a temporary state was active and has now expired, revert to the persistent state
        if (renderState != persistentState && millis() >= tempStateEndTime) {
            renderState = persistentState;
        }

        self->_currentState = renderState;

        // Turn all LEDs off before setting the new pattern
        digitalWrite(self->_redPin, LOW);
        digitalWrite(self->_greenPin, LOW);
        digitalWrite(self->_yellowPin, LOW);

        tickCount++;

        // State logic based on the current renderState
        switch (renderState) {
            case OFF:
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
