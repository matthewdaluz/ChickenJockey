#pragma once
#include <string>
#include <vector>
#include <filesystem> // Include this since your cpp uses std::filesystem::path

namespace crypto {

/**
 * @brief Generates cryptographically secure random bytes.
 */
bool GenerateRandomBytes(unsigned char* buffer, int numBytes);

/**
 * @brief Generates a random alphanumeric filename.
 */
std::string GenerateRandomFilename(size_t length);

/**
 * @brief Encrypts plaintext using AES-256-CBC.
 */
bool EncryptData(const std::vector<unsigned char>& plaintext, std::vector<unsigned char>& ciphertext);

/**
 * @brief Decrypts ciphertext using AES-256-CBC.
 */
bool DecryptData(const std::vector<unsigned char>& ciphertext, std::vector<unsigned char>& plaintext);

/**
 * @brief Generates a secure random password.
 */
bool GenerateRandomPassword(std::vector<unsigned char>& password, size_t length = 32);

/**
 * @brief Writes binary data to a file.
 */
bool WriteBinaryToFile(const std::filesystem::path& filePath, const std::vector<unsigned char>& data);

/**
 * @brief Reads binary data from a file.
 */
bool ReadBinaryFromFile(const std::filesystem::path& filePath, std::vector<unsigned char>& data);

/**
 * @brief Generates a random password, encrypts it, and stores it in a file.
 */
bool GenerateAndStorePassword(const std::filesystem::path& storageDir, std::filesystem::path& outFilePath);

/**
 * @brief Loads and decrypts a password from a file.
 */
bool LoadAndDecryptPassword(const std::filesystem::path& filePath, std::vector<unsigned char>& password);

} // namespace crypto
