#include "DiagnosticLED.h"

// Variables globales internes au module pour la file d'attente et les broches
static QueueHandle_t ledStateQueue_internal;
static int _redPin, _yellowPin, _bluePin;

QueueHandle_t DiagnosticLED::initialize(int redPin, int yellowPin, int bluePin) {
    _redPin = redPin;
    _yellowPin = yellowPin;
    _bluePin = bluePin;

    pinMode(_redPin, OUTPUT);
    pinMode(_yellowPin, OUTPUT);
    pinMode(_bluePin, OUTPUT);
    digitalWrite(_redPin, LOW);
    digitalWrite(_yellowPin, LOW);
    digitalWrite(_bluePin, LOW);

    ledStateQueue_internal = xQueueCreate(10, sizeof(LED_State));
    return ledStateQueue_internal;
}

void DiagnosticLED::Task_LED_Manager(void *pvParameters) {
    LED_State currentState = INIT;
    LED_State previousState = OPERATIONAL;
    unsigned long lastBlinkTime = 0;
    bool ledOn = false;

    auto turnOffAllLeds = []() {
        digitalWrite(_redPin, LOW);
        digitalWrite(_yellowPin, LOW);
        digitalWrite(_bluePin, LOW);
    };

    for (;;) {
        if (xQueueReceive(ledStateQueue_internal, &currentState, 0) == pdPASS) {
            turnOffAllLeds();
            if (currentState != LORA_ACTIVITY) {
                previousState = currentState;
            }
        }

        switch (currentState) {
            case INIT:
                if (millis() - lastBlinkTime > 500) {
                    ledOn = !ledOn;
                    digitalWrite(_yellowPin, ledOn);
                    lastBlinkTime = millis();
                }
                break;
            case CONFIG_MODE:
                digitalWrite(_yellowPin, HIGH);
                break;
            case OPERATIONAL:
                digitalWrite(_bluePin, HIGH);
                break;
            case LORA_ACTIVITY:
                digitalWrite(_bluePin, HIGH);
                vTaskDelay(pdMS_TO_TICKS(50));
                digitalWrite(_bluePin, LOW);
                vTaskDelay(pdMS_TO_TICKS(50));
                currentState = previousState;
                turnOffAllLeds();
                break;
            case CONNECTIVITY_ERROR:
                if (millis() - lastBlinkTime > 500) {
                    ledOn = !ledOn;
                    digitalWrite(_redPin, ledOn);
                    lastBlinkTime = millis();
                }
                break;
            case CRITICAL_ERROR:
                digitalWrite(_redPin, HIGH);
                break;
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
