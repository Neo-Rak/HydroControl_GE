#pragma once

#include <Arduino.h>
#include <AESLib.h>

// --- Paramètres de Chiffrement ---
// L'IV (Vecteur d'Initialisation) doit être partagé entre les modules, tout comme la clé.
// Il est souvent dérivé de la clé ou pré-partagé. Pour HydroControl-GE, nous le gardons fixe.
const byte aes_iv[] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10 };

class CryptoManager {
public:
    /**
     * @brief Chiffre un message String et retourne le résultat encodé en Base64.
     * @param plainText Le message à chiffrer.
     * @param key La clé de chiffrement (doit faire 16, 24 ou 32 bytes).
     * @return Le message chiffré et encodé en Base64.
     */
    static String encrypt(const String& plainText, const String& key);

    /**
     * @brief Déchiffre un message encodé en Base64.
     * @param encryptedBase64 Le message chiffré.
     * @param key La clé de déchiffrement.
     * @return Le message en clair, ou une chaîne vide en cas d'erreur.
     */
    static String decrypt(const String& encryptedBase64, const String& key);
};
