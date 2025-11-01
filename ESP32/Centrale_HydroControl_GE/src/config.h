#pragma once

// --- Configuration Matérielle ---
#define LORA_SS_PIN    5
#define LORA_RST_PIN   14
#define LORA_DIO0_PIN  2
#define LORA_FREQ      433E6

// --- Configuration Réseau ---
#define PRE_SHARED_KEY "HydroControl-GE-Super-Secret-Key-2025"

// --- Configuration du point d'accès initial ---
#define AP_SSID "HydroControl-Setup"
#define AP_PASSWORD "config1234"

// --- Limites du système ---
#define MAX_NODES 32 // Nombre maximum de modules gérés

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
