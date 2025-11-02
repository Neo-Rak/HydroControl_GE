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
    // Initialization Vector (IV) - must be 16 bytes
    static uint8_t iv[16];
    static AESLib aesLib;
};

#endif // CRYPTO_H
