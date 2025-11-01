#pragma once

// --- Configuration Matérielle ---
// Broches standard et sûres pour une carte ESP32 DevKit
// Le bus VSPI par défaut (MOSI:23, MISO:19, SCK:18) est utilisé par la bibliothèque LoRa.
#define LORA_SS_PIN    5  // GPIO5 (VSPI_SS)
#define LORA_RST_PIN   14 // GPIO14
#define LORA_DIO0_PIN  4  // GPIO4 (évite les broches de strapping comme GPIO2)
#define LORA_FREQ      433E6 // 433 MHz

// Le bus I2C (SDA:21, SCL:22) est libre.
// Le bus HSPI (MOSI:13, MISO:12, SCK:14) est partiellement utilisé (RST_PIN).

// Broche de sortie pour commander un relais. GPIO26 est un DAC et un choix sûr.
#define RELAY_PIN      26 // GPIO26 (DAC2)
// Broche en entrée pour un bouton. GPIO32 est un choix sûr.
#define BUTTON_PIN     32 // GPIO32

// --- Configuration Réseau ---
#define PRE_SHARED_KEY "HydroControl-GE-Super-Secret-Key-2025"

// --- Configuration du point d'accès initial ---
#define AP_SSID "WellguardPro-Setup"
#define AP_PASSWORD "config1234"

// --- Identifiants du module ---
// L'adresse MAC sera utilisée comme ID unique

// --- Configuration des LEDs de Diagnostic ---
// Broches choisies pour éviter les conflits et les problèmes de démarrage
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
