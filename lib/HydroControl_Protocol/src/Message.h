#pragma once

#include <ArduinoJson.h>

// --- Énumérations pour le protocole ---
enum MessageType {
    DISCOVERY,
    WELCOME_ACK,
    STATUS_UPDATE,
    COMMAND,
    COMMAND_ACK,
    HEARTBEAT,
    RELAY_REQUEST,
    SYNC_COMMAND
};

enum NodeRole {
    ROLE_UNKNOWN,
    ROLE_CENTRALE,
    ROLE_AQUA_RESERV_PRO,
    ROLE_WELLGUARD_PRO
};

enum CommandType {
    CMD_PUMP_ON,
    CMD_PUMP_OFF,
    CMD_SET_MODE_AUTO,
    CMD_SET_MODE_MANUAL
};

// --- Structure de base d'un message ---
// Note: L'utilisation de templates ou de classes plus complexes est évitée
// pour rester simple et compatible avec les contraintes mémoire de l'ESP32.

class LoRaMessage {
public:
    // --- Sérialisation d'un message de découverte ---
    static String serializeDiscovery(const char* deviceId, NodeRole role) {
        StaticJsonDocument<128> doc;
        doc["type"] = MessageType::DISCOVERY;
        doc["id"] = deviceId;
        doc["role"] = role;
        String output;
        serializeJson(doc, output);
        return output;
    }

    // --- Sérialisation d'une commande ---
    static String serializeCommand(const char* sourceId, const char* targetId, CommandType cmd) {
        StaticJsonDocument<256> doc;
        doc["type"] = MessageType::COMMAND;
        doc["src"] = sourceId;
        doc["tgt"] = targetId;
        doc["cmd"] = cmd;
        String output;
        serializeJson(doc, output);
        return output;
    }

    // --- Sérialisation d'un ACK de commande ---
     static String serializeCommandAck(const char* sourceId, const char* targetId, bool success) {
        StaticJsonDocument<256> doc;
        doc["type"] = MessageType::COMMAND_ACK;
        doc["src"] = sourceId;
        doc["tgt"] = targetId;
        doc["success"] = success;
        String output;
        serializeJson(doc, output);
        return output;
    }

    // --- Sérialisation d'une mise à jour de statut ---
    static String serializeStatusUpdate(const char* deviceId, const char* status, int rssi) {
        StaticJsonDocument<256> doc;
        doc["type"] = MessageType::STATUS_UPDATE;
        doc["id"] = deviceId;
        doc["status"] = status;
        doc["rssi"] = rssi;
        String output;
        serializeJson(doc, output);
        return output;
    }

    // --- Désérialisation ---
    // La désérialisation sera gérée par chaque module en fonction du type de message attendu.
    // L'attribut "type" sera le premier champ à inspecter.
};
