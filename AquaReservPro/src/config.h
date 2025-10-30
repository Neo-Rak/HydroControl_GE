#pragma once

// --- Configuration Matérielle ---
#define LORA_SS_PIN    5
#define LORA_RST_PIN   14
#define LORA_DIO0_PIN  2
#define LORA_FREQ      433E6

#define LEVEL_SENSOR_PIN 23 // GPIO pour le capteur de niveau (PLEIN/VIDE)
#define BUTTON_PIN     22 // GPIO pour le bouton manuel

// --- Configuration Logique ---
#define PRE_SHARED_KEY "HydroControl-GE-Super-Secret-Key-2025"
#define SENSOR_STABILITY_MS 5000 // 5 secondes de stabilité requise

// --- Configuration du point d'accès initial ---
#define AP_SSID "AquaReservPro-Setup"
#define AP_PASSWORD "config1234"

// --- Clés pour le stockage non-volatile ---
#define PREF_KEY_MODE "opMode"
#define PREF_KEY_WELL_ID "assignedWell"

// --- États possibles ---
enum OperatingMode { AUTO, MANUAL };
enum LevelState { LEVEL_EMPTY, LEVEL_FULL, LEVEL_UNKNOWN };

// --- Configuration des LEDs de Diagnostic ---
#define RED_LED_PIN    13
#define YELLOW_LED_PIN 12
#define BLUE_LED_PIN   15

enum LED_State {
    INIT,
    CONFIG_MODE,
    OPERATIONAL,
    LORA_ACTIVITY,
    CONNECTIVITY_ERROR,
    CRITICAL_ERROR
};
