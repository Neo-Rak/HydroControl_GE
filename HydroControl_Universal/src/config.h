#pragma once

// =================================================================
// FICHIER DE CONFIGURATION GLOBAL POUR HydroControl_Universal
// =================================================================
// Ce fichier centralise toutes les définitions de broches (GPIO)
// et autres paramètres matériels pour les différents rôles.

// -----------------------------------------------------------------
// Brochage commun
// -----------------------------------------------------------------

// Module LoRa (RFM95/SX127x) - Standard SPI VSPI
#define LORA_SCK_PIN   18
#define LORA_MISO_PIN  19
#define LORA_MOSI_PIN  23
#define LORA_SS_PIN    5
#define LORA_RST_PIN   14
#define LORA_DIO0_PIN  2

// LEDs de statut (Rouge, Vert, Jaune)
#define LED_RED_PIN    15
#define LED_GREEN_PIN  16
#define LED_YELLOW_PIN 17

// -----------------------------------------------------------------
// Brochage universel pour les rôles
// -----------------------------------------------------------------
// Ces broches sont utilisées différemment selon le rôle configuré.
// Voir la documentation pour le câblage exact.
#define ROLE_PIN_1    25
#define ROLE_PIN_2    26
#define ROLE_PIN_3    27
