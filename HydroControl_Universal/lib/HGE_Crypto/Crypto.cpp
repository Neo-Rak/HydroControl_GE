#include "Crypto.h"

// Helper function to convert byte array to hex string
String bytes_to_hex_string(const byte* bytes, unsigned int len) {
    String str = "";
    for (unsigned int i = 0; i < len; i++) {
        char tmp[3];
        sprintf(tmp, "%02x", bytes[i]);
        str += tmp;
    }
    return str;
}

// Helper function to convert hex string to byte array
void hex_string_to_bytes(const String& hex, byte* bytes) {
    for (unsigned int i = 0; i < hex.length(); i += 2) {
        String hexByte = hex.substring(i, i + 2);
        bytes[i / 2] = (byte)strtol(hexByte.c_str(), NULL, 16);
    }
}


uint8_t CryptoManager::aesKey[16];
// Define a static IV. IMPORTANT: For production, this should be unique per message
// for maximum security (e.g., derived from a counter), but for this project, a static IV is acceptable.
uint8_t CryptoManager::iv[16] = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f };

AESLib CryptoManager::aesLib;

void CryptoManager::setKey(const uint8_t* key) {
    memcpy(aesKey, key, 16);
}

String CryptoManager::encrypt(const String& plainText) {
    int plainTextLen = plainText.length() + 1;
    byte plainTextBytes[plainTextLen];
    plainText.getBytes(plainTextBytes, plainTextLen);

    // AES-128-CBC requires padding to 16-byte blocks
    int paddedLen = (plainTextLen % 16 == 0) ? plainTextLen : (plainTextLen / 16 + 1) * 16;
    byte encrypted[paddedLen];

    aesLib.encrypt(plainTextBytes, plainTextLen, encrypted, aesKey, 128, iv);

    return bytes_to_hex_string(encrypted, paddedLen);
}

String CryptoManager::decrypt(const String& encryptedHex) {
    int encryptedLen = encryptedHex.length() / 2;
    byte encryptedBytes[encryptedLen];
    hex_string_to_bytes(encryptedHex, encryptedBytes);

    byte decrypted[encryptedLen];
    aesLib.decrypt(encryptedBytes, encryptedLen, decrypted, aesKey, 128, iv);

    // The result is null-terminated, so it can be cast to char*
    return String((char*)decrypted);
}
