/**
 * @file Constants.h
 * @author Jules
 * @brief Defines global constants for the HydroControl-GE system.
 * @version 3.1.0
 * @date 2025-11-02
 *
 * @copyright Copyright (c) 2025
 */

#ifndef HGE_CONSTANTS_H
#define HGE_CONSTANTS_H

// --- Preferences Namespaces ---
#define PREFS_NAMESPACE_NETWORK "network_config"
#define PREFS_NAMESPACE_SECURITY "security_config"
#define PREFS_NAMESPACE_NODE_NAMES "node-names"

// --- Preferences Keys ---
#define PREFS_KEY_SSID "ssid"
#define PREFS_KEY_PASSWORD "password"
#define PREFS_KEY_LORA_PSK "lora_psk"

// --- Operational Config ---
#define PREFS_NAMESPACE_OPERATIONAL "hydro_config"
#define PREFS_KEY_ASSIGNED_WELL "assigned_well"
#define PREFS_KEY_IS_WELL_SHARED "is_well_shared"
#define PREFS_KEY_OP_MODE "op_mode"

#endif // HGE_CONSTANTS_H
