#pragma once

// --- Configuration Matérielle ---
#define LORA_SS_PIN    5  // GPIO5 (VSPI_SS)
#define LORA_RST_PIN   14 // GPIO14
#define LORA_DIO0_PIN  2  // GPIO2
#define LORA_FREQ      433E6 // 433 MHz

#define RELAY_PIN      23 // GPIO23 pour commander le relais
#define BUTTON_PIN     22 // GPIO22 pour le bouton manuel

// --- Configuration Réseau ---
#define PRE_SHARED_KEY "HydroControl-GE-Super-Secret-Key-2025"

// --- Configuration du point d'accès initial ---
#define AP_SSID "WellguardPro-Setup"
#define AP_PASSWORD "config1234"

// --- Identifiants du module ---
// L'adresse MAC sera utilisée comme ID unique
