#ifndef CRYPTO_H
#define CRYPTO_H

#include <AESLib.h>

class CryptoManager {
public:
    static void setKey(const uint8_t* key);
    static String encrypt(const String& plainText);
    static String decrypt(const String& encryptedHex);

private:
    static uint8_t aesKey[16];
    // Define a static IV. IMPORTANT: This must be the same across all devices.
    static uint8_t iv[16];
    static AESLib aesLib;
};

#endif // CRYPTO_H
