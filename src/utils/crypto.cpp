// crypto.cpp
#include "crypto.h"
#pragma message("Using OpenSSL header from: " __FILE__)

#include <windows.h>  // Required before OpenSSL on Windows

extern "C" {
    #include <openssl/evp.h>
    #include <openssl/err.h>
    #include <openssl/rand.h>
}

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>
#include <stdexcept>


namespace crypto {

// ----- Constants & Configuration -----
constexpr size_t AES_KEY_LENGTH        = 32;  // 256 bits
constexpr size_t AES_IV_LENGTH         = 16;  // 128 bits
constexpr size_t FILENAME_LENGTH       = 12;
constexpr size_t DEFAULT_PASSWORD_LENGTH = 32;

// ----- Security Critical Data -----
// (volatile removed so we can pass pointers to OpenSSL)
static const unsigned char staticKey[AES_KEY_LENGTH] = { /* ... */ };
static const unsigned char staticIV [AES_IV_LENGTH ] = { /* ... */ };

// ----- RAII Wrapper for OpenSSL Context -----
struct EVPCipherContext {
    EVP_CIPHER_CTX* ctx;
    EVPCipherContext() : ctx(EVP_CIPHER_CTX_new()) {
        if (!ctx) throw std::runtime_error("Failed to create EVP_CIPHER_CTX");
    }
    ~EVPCipherContext() {
        EVP_CIPHER_CTX_free(ctx);
    }
    EVPCipherContext(const EVPCipherContext&) = delete;
    EVPCipherContext& operator=(const EVPCipherContext&) = delete;
};

// ----- Error Handling -----
void log_openssl_error(const std::string& context) {
    char err_buf[256];
    ERR_error_string_n(ERR_get_error(), err_buf, sizeof(err_buf));
    std::cerr << "[Crypto] Error in " << context << ": " << err_buf << "\n";
}

// ----- Secure Random Generation -----
bool GenerateRandomBytes(std::vector<unsigned char>& buffer, size_t numBytes) {
    buffer.resize(numBytes);
    if (RAND_bytes(buffer.data(), static_cast<int>(numBytes)) != 1) {
        log_openssl_error("GenerateRandomBytes");
        buffer.clear();
        return false;
    }
    return true;
}

std::string GenerateRandomFilename(size_t length /*= FILENAME_LENGTH*/) {
    constexpr char charset[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";
    constexpr size_t charset_size = sizeof(charset) - 1;
    constexpr int max_acceptable = 256 - (256 % charset_size);

    std::vector<unsigned char> rnd(1);
    std::string filename;
    filename.reserve(length);

    for (size_t i = 0; i < length; ++i) {
        // get one random byte
        if (!GenerateRandomBytes(rnd, 1)) return "";
        int value = rnd[0];
        while (value >= max_acceptable) {
            if (!GenerateRandomBytes(rnd, 1)) return "";
            value = rnd[0];
        }
        filename += charset[value % charset_size];
    }
    return filename;
}

// ----- Encryption/Decryption -----
bool EncryptData(const std::vector<unsigned char>& plaintext,
                 std::vector<unsigned char>& ciphertext) {
    try {
        EVPCipherContext ctx;
        const EVP_CIPHER* cipher = EVP_aes_256_cbc();  // 5. explicit type

        if (EVP_EncryptInit_ex(ctx.ctx, cipher, nullptr, staticKey, staticIV) != 1) {
            log_openssl_error("EVP_EncryptInit_ex");
            return false;
        }

        ciphertext.resize(plaintext.size() + EVP_CIPHER_block_size(cipher));
        int out_len = 0;
        if (EVP_EncryptUpdate(ctx.ctx, ciphertext.data(), &out_len,
                              plaintext.data(), (int)plaintext.size()) != 1) {
            log_openssl_error("EVP_EncryptUpdate");
            return false;
        }

        int final_len = 0;
        if (EVP_EncryptFinal_ex(ctx.ctx, ciphertext.data() + out_len, &final_len) != 1) {
            log_openssl_error("EVP_EncryptFinal_ex");
            return false;
        }

        ciphertext.resize(out_len + final_len);
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[Crypto] Exception: " << e.what() << "\n";
        return false;
    }
}

bool DecryptData(const std::vector<unsigned char>& ciphertext,
                 std::vector<unsigned char>& plaintext) {
    try {
        EVPCipherContext ctx;
        const EVP_CIPHER* cipher = EVP_aes_256_cbc();

        if (EVP_DecryptInit_ex(ctx.ctx, cipher, nullptr, staticKey, staticIV) != 1) {
            log_openssl_error("EVP_DecryptInit_ex");
            return false;
        }

        plaintext.resize(ciphertext.size());
        int out_len = 0;
        if (EVP_DecryptUpdate(ctx.ctx, plaintext.data(), &out_len,
                              ciphertext.data(), (int)ciphertext.size()) != 1) {
            log_openssl_error("EVP_DecryptUpdate");
            return false;
        }

        int final_len = 0;
        if (EVP_DecryptFinal_ex(ctx.ctx, plaintext.data() + out_len, &final_len) != 1) {
            log_openssl_error("EVP_DecryptFinal_ex");
            return false;
        }

        plaintext.resize(out_len + final_len);
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[Crypto] Exception: " << e.what() << "\n";
        return false;
    }
}

// ----- File Operations -----
bool WriteBinaryToFile(const std::filesystem::path& file_path,
                       const std::vector<unsigned char>& data) {
    std::ofstream ofs(file_path, std::ios::binary);
    if (!ofs) {
        std::cerr << "[Crypto] Error opening file: " << file_path << "\n";
        return false;
    }
    ofs.write((const char*)data.data(), data.size());
    return ofs.good();
}

bool ReadBinaryFromFile(const std::filesystem::path& file_path,
                        std::vector<unsigned char>& data) {
    std::ifstream ifs(file_path, std::ios::binary | std::ios::ate);
    if (!ifs) {
        std::cerr << "[Crypto] Error opening file: " << file_path << "\n";
        return false;
    }
    auto size = (size_t)ifs.tellg();
    ifs.seekg(0);
    data.resize(size);
    ifs.read((char*)data.data(), size);
    return ifs.good();
}

// ----- Main Operations -----
bool GenerateAndStorePassword(const std::filesystem::path& storage_dir,
                              std::filesystem::path& out_file_path) {
    std::vector<unsigned char> password;
    if (!GenerateRandomBytes(password, DEFAULT_PASSWORD_LENGTH)) {
        std::cerr << "[Crypto] Password generation failed\n";
        return false;
    }

    std::vector<unsigned char> encrypted;
    if (!EncryptData(password, encrypted)) {
        std::fill(password.begin(), password.end(), 0);
        return false;
    }
    std::fill(password.begin(), password.end(), 0);

    auto filename = GenerateRandomFilename(FILENAME_LENGTH) + ".dat";
    out_file_path = storage_dir / filename;
    if (!WriteBinaryToFile(out_file_path, encrypted)) {
        std::fill(encrypted.begin(), encrypted.end(), 0);
        return false;
    }
    std::fill(encrypted.begin(), encrypted.end(), 0);
    return true;
}

bool LoadAndDecryptPassword(const std::filesystem::path& file_path,
                            std::vector<unsigned char>& password) {
    std::vector<unsigned char> encrypted;
    if (!ReadBinaryFromFile(file_path, encrypted)) return false;
    bool ok = DecryptData(encrypted, password);
    std::fill(encrypted.begin(), encrypted.end(), 0);
    return ok;
}

} // namespace crypto
