#pragma once

// --- Configuration Matérielle ---
// Broches standard et sûres pour une carte ESP32 DevKit
// Le bus VSPI par défaut (MOSI:23, MISO:19, SCK:18) est utilisé par la bibliothèque LoRa.
#define LORA_SS_PIN    5  // GPIO5 (VSPI_SS)
#define LORA_RST_PIN   14 // GPIO14
#define LORA_DIO0_PIN  4  // GPIO4 (évite les broches de strapping comme GPIO2)
#define LORA_FREQ      433E6 // 433 MHz

// --- Configuration Réseau ---
#define PRE_SHARED_KEY "HydroControl-GE-Super-Secret-Key-2025"

// --- Configuration du point d'accès initial ---
#define AP_SSID "HydroControl-Setup"
#define AP_PASSWORD "config1234"

// --- Limites du système ---
#define MAX_NODES 32 // Nombre maximum de modules gérés

// --- Configuration des LEDs de Diagnostic ---
// Broches choisies pour éviter les conflits et les problèmes de démarrage (strapping pins)
#define RED_LED_PIN    13 // GPIO13
#define YELLOW_LED_PIN 27 // GPIO27
#define BLUE_LED_PIN   25 // GPIO25

enum LED_State {
    INIT,
    CONFIG_MODE,
    OPERATIONAL,
    LORA_ACTIVITY,
    CONNECTIVITY_ERROR,
    CRITICAL_ERROR
};
