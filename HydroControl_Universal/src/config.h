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

// LEDs de statut (RGB)
#define LED_RED_PIN    15
#define LED_GREEN_PIN  16
#define LED_BLUE_PIN   17

// -----------------------------------------------------------------
// Brochage spécifique au rôle AQUA_RESERV_PRO
// -----------------------------------------------------------------

// Capteurs de niveau d'eau (logique INPUT_PULLUP)
// Le capteur doit connecter la broche au GND lorsqu'il est activé.
#define AQUA_RESERV_LEVEL_HIGH_PIN 25 // Capteur de niveau haut
#define AQUA_RESERV_LEVEL_LOW_PIN  26 // Capteur de niveau bas

// Bouton pour le mode manuel (logique INPUT_PULLUP)
#define AQUA_RESERV_BUTTON_PIN     32

// -----------------------------------------------------------------
// Brochage spécifique au rôle WELLGUARD_PRO
// -----------------------------------------------------------------

// Commande du relais de la pompe de puits
#define WELLGUARD_RELAY_PIN        27

// Entrée pour un défaut matériel externe (ex: pressostat, arrêt d'urgence)
// Logique INPUT_PULLUP : le capteur doit connecter la broche au GND en cas de défaut.
#define WELLGUARD_FAULT_PIN        33
