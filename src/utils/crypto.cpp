// crypto.cpp
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/err.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <iomanip>
#include <cstdlib>
#include <cstring>

// ----- Hardcoded Key & IV -----
// For AES-256-CBC, we need a 32-byte key and a 16-byte IV.
// In a production system, these would be derived or further protected.
static const unsigned char staticKey[32] = {
    0xA3, 0xB1, 0xC2, 0xD4, 0xE5, 0xF6, 0x12, 0x34,
    0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0, 0x11, 0x22,
    0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA,
    0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x00, 0x10, 0x20
};

static const unsigned char staticIV[16] = {
    0x1F, 0x2E, 0x3D, 0x4C, 0x5B, 0x6A, 0x79, 0x88,
    0x97, 0xA6, 0xB5, 0xC4, 0xD3, 0xE2, 0xF1, 0x00
};

// ----- Helper function: Generate secure random bytes -----
// Fills 'buffer' with 'numBytes' of cryptographically secure random bytes.
bool GenerateRandomBytes(unsigned char* buffer, int numBytes) {
    if (RAND_bytes(buffer, numBytes) != 1) {
        std::cerr << "[Crypto] Error: Failed to generate random bytes: "
                  << ERR_error_string(ERR_get_error(), nullptr) << std::endl;
        return false;
    }
    return true;
}

// ----- Helper function: Generate a random alphanumeric filename -----
// Generates a random string of the specified length.
std::string GenerateRandomFilename(size_t length) {
    const char charset[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";
    const size_t max_index = (sizeof(charset) - 1);
    std::string filename;
    filename.reserve(length);
    unsigned char buffer[1];
    for (size_t i = 0; i < length; ++i) {
        if (!GenerateRandomBytes(buffer, 1))
            return "";
        filename += charset[buffer[0] % max_index];
    }
    return filename;
}

// ----- Encryption using AES-256-CBC -----
// Encrypts the plaintext (input as binary data) and stores result in ciphertext vector.
bool EncryptData(const std::vector<unsigned char>& plaintext, std::vector<unsigned char>& ciphertext) {
    bool success = false;
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        std::cerr << "[Crypto] Error: Failed to create EVP_CIPHER_CTX." << std::endl;
        return false;
    }

    // Initialize context for AES-256-CBC encryption.
    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr, staticKey, staticIV) != 1) {
        std::cerr << "[Crypto] Error: EVP_EncryptInit_ex failed: "
                  << ERR_error_string(ERR_get_error(), nullptr) << std::endl;
        goto cleanup;
    }

    // Allocate ciphertext buffer with extra space for block padding.
    ciphertext.resize(plaintext.size() + EVP_CIPHER_block_size(EVP_aes_256_cbc()));
    int outLen1 = 0;
    if (EVP_EncryptUpdate(ctx,
                          ciphertext.data(),
                          &outLen1,
                          plaintext.data(),
                          static_cast<int>(plaintext.size())) != 1) {
        std::cerr << "[Crypto] Error: EVP_EncryptUpdate failed: "
                  << ERR_error_string(ERR_get_error(), nullptr) << std::endl;
        goto cleanup;
    }

    int outLen2 = 0;
    if (EVP_EncryptFinal_ex(ctx,
                            ciphertext.data() + outLen1,
                            &outLen2) != 1) {
        std::cerr << "[Crypto] Error: EVP_EncryptFinal_ex failed: "
                  << ERR_error_string(ERR_get_error(), nullptr) << std::endl;
        goto cleanup;
    }
    ciphertext.resize(outLen1 + outLen2);
    success = true;

cleanup:
    EVP_CIPHER_CTX_free(ctx);
    return success;
}

// ----- Decryption using AES-256-CBC -----
// Decrypts the ciphertext (input as binary data) and stores result in plaintext vector.
bool DecryptData(const std::vector<unsigned char>& ciphertext, std::vector<unsigned char>& plaintext) {
    bool success = false;
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        std::cerr << "[Crypto] Error: Failed to create EVP_CIPHER_CTX for decryption." << std::endl;
        return false;
    }

    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr, staticKey, staticIV) != 1) {
        std::cerr << "[Crypto] Error: EVP_DecryptInit_ex failed: "
                  << ERR_error_string(ERR_get_error(), nullptr) << std::endl;
        goto cleanup;
    }

    // Allocate plaintext buffer.
    plaintext.resize(ciphertext.size());
    int outLen1 = 0;
    if (EVP_DecryptUpdate(ctx,
                          plaintext.data(),
                          &outLen1,
                          ciphertext.data(),
                          static_cast<int>(ciphertext.size())) != 1) {
        std::cerr << "[Crypto] Error: EVP_DecryptUpdate failed: "
                  << ERR_error_string(ERR_get_error(), nullptr) << std::endl;
        goto cleanup;
    }

    int outLen2 = 0;
    if (EVP_DecryptFinal_ex(ctx,
                            plaintext.data() + outLen1,
                            &outLen2) != 1) {
        std::cerr << "[Crypto] Error: EVP_DecryptFinal_ex failed: "
                  << ERR_error_string(ERR_get_error(), nullptr) << std::endl;
        goto cleanup;
    }
    plaintext.resize(outLen1 + outLen2);
    success = true;

cleanup:
    EVP_CIPHER_CTX_free(ctx);
    return success;
}

// ----- Generate a secure random password -----
// Generates a random password of the specified length (in bytes) as binary data.
bool GenerateRandomPassword(std::vector<unsigned char>& password, size_t length = 32) {
    password.resize(length);
    if (!GenerateRandomBytes(password.data(), static_cast<int>(length))) {
        std::cerr << "[Crypto] Error: Failed to generate random password." << std::endl;
        return false;
    }
    return true;
}

// ----- Write binary data to a file -----
// Writes the provided binary data to a file specified by filePath.
bool WriteBinaryToFile(const std::string& filePath, const std::vector<unsigned char>& data) {
    std::ofstream ofs(filePath, std::ios::binary);
    if (!ofs) {
        std::cerr << "[Crypto] Error: Unable to open file for writing: " << filePath << std::endl;
        return false;
    }
    ofs.write(reinterpret_cast<const char*>(data.data()), data.size());
    ofs.close();
    return true;
}

// ----- Read binary data from a file -----
// Reads the entire file specified by filePath into data vector.
bool ReadBinaryFromFile(const std::string& filePath, std::vector<unsigned char>& data) {
    std::ifstream ifs(filePath, std::ios::binary | std::ios::ate);
    if (!ifs) {
        std::cerr << "[Crypto] Error: Unable to open file for reading: " << filePath << std::endl;
        return false;
    }
    std::streamsize size = ifs.tellg();
    ifs.seekg(0, std::ios::beg);
    data.resize(static_cast<size_t>(size));
    if (!ifs.read(reinterpret_cast<char*>(data.data()), size)) {
        std::cerr << "[Crypto] Error: Failed to read data from file: " << filePath << std::endl;
        return false;
    }
    ifs.close();
    return true;
}

// ----- Generate, encrypt, and store the password -----
// Generates a secure random password, encrypts it, and stores it in a file with a random name.
// The 'storageDir' parameter should be a valid directory (e.g., "C:\\ProgramData\\ChickenJockey\\").
// On success, outFilePath is set to the full path of the stored file.
bool GenerateAndStorePassword(const std::string& storageDir, std::string& outFilePath) {
    // Step 1: Generate the random password.
    std::vector<unsigned char> password;
    if (!GenerateRandomPassword(password)) {
        return false;
    }
    std::cout << "[Crypto] Random password generated." << std::endl;

    // Step 2: Encrypt the random password.
    std::vector<unsigned char> encryptedData;
    if (!EncryptData(password, encryptedData)) {
        std::cerr << "[Crypto] Error: Failed to encrypt the password." << std::endl;
        return false;
    }
    std::cout << "[Crypto] Password successfully encrypted." << std::endl;

    // Step 3: Generate a random filename for storing the encrypted password.
    std::string randomFilename = GenerateRandomFilename(12) + ".dat";
    outFilePath = storageDir;
    // Ensure the directory ends with a path separator.
    if (outFilePath.back() != '\\' && outFilePath.back() != '/') {
        outFilePath += "\\";
    }
    outFilePath += randomFilename;

    // Step 4: Write the encrypted data to the file (in binary mode).
    if (!WriteBinaryToFile(outFilePath, encryptedData)) {
        std::cerr << "[Crypto] Error: Failed to write encrypted password to file." << std::endl;
        return false;
    }
    std::cout << "[Crypto] Encrypted password stored in file: " << outFilePath << std::endl;

    return true;
}

// ----- Load and decrypt the password from file -----
// Reads the encrypted password from 'filePath', decrypts it, and stores the result in 'password'.
// Returns true on success.
bool LoadAndDecryptPassword(const std::string& filePath, std::vector<unsigned char>& password) {
    // Step 1: Read the encrypted data from the file.
    std::vector<unsigned char> encryptedData;
    if (!ReadBinaryFromFile(filePath, encryptedData)) {
        std::cerr << "[Crypto] Error: Failed to read encrypted password from file: " << filePath << std::endl;
        return false;
    }
    std::cout << "[Crypto] Encrypted password loaded from file." << std::endl;

    // Step 2: Decrypt the data.
    if (!DecryptData(encryptedData, password)) {
        std::cerr << "[Crypto] Error: Failed to decrypt the password." << std::endl;
        return false;
    }
    std::cout << "[Crypto] Password successfully decrypted." << std::endl;
    return true;
}

/*
Usage Example:

#include "crypto.h"  // if you separate declarations in a header

int main() {
    std::string storageDir = "C:\\ProgramData\\ChickenJockey";
    std::string encryptedFile;
    if (GenerateAndStorePassword(storageDir, encryptedFile)) {
        std::cout << "Password stored at: " << encryptedFile << std::endl;
    }

    std::vector<unsigned char> decryptedPassword;
    if (LoadAndDecryptPassword(encryptedFile, decryptedPassword)) {
        // Print decrypted password as hex (for demonstration purposes only).
        std::ostringstream oss;
        for (unsigned char c : decryptedPassword)
            oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(c);
        std::cout << "Decrypted password (hex): " << oss.str() << std::endl;
    }
    return 0;
}
*/

