#include "Crypto.h"

AESLib aes;

void generate_aes_key(const String& psk, byte* key_buffer) {
    memset(key_buffer, 0, 16);
    psk.getBytes(key_buffer, 17);
}

String CryptoManager::encrypt(const String& plainText, const String& key) {
    byte aes_key[16];
    generate_aes_key(key, aes_key);

    int plainTextLen = plainText.length() + 1;
    char plainTextChar[plainTextLen];
    plainText.toCharArray(plainTextChar, plainTextLen);

    int cipherLen = aes.get_cipher_length(plainTextLen);
    char encrypted[cipherLen];

    byte iv[N_BLOCK];
    memcpy(iv, aes_iv, N_BLOCK);

    aes.encrypt((byte*)plainTextChar, plainTextLen, (byte*)encrypted, aes_key, 128, iv);

    // The result is not Base64 encoded, so we do it manually.
    int base64Len = base64_enc_len(cipherLen);
    char base64_buff[base64Len];
    base64_encode(base64_buff, (char*)encrypted, cipherLen);

    return String(base64_buff);
}

String CryptoManager::decrypt(const String& encryptedBase64, const String& key) {
    byte aes_key[16];
    generate_aes_key(key, aes_key);

    int encryptedBase64Len = encryptedBase64.length() + 1;
    char encryptedBase64Char[encryptedBase64Len];
    encryptedBase64.toCharArray(encryptedBase64Char, encryptedBase64Len);

    int decodedLen = base64_dec_len(encryptedBase64Char, encryptedBase64Len);
    byte decoded[decodedLen];
    base64_decode((char*)decoded, encryptedBase64Char, encryptedBase64Len);

    char decrypted[decodedLen];
    byte iv[N_BLOCK];
    memcpy(iv, aes_iv, N_BLOCK);

    aes.decrypt(decoded, decodedLen, (byte*)decrypted, aes_key, 128, iv);

    return String(decrypted);
}
