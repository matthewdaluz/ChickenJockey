#pragma once
#include <string>
#include <vector>

/**
 * @brief Generates cryptographically secure random bytes.
 *
 * @param buffer Pointer to the buffer that will receive random bytes.
 * @param numBytes The number of bytes to generate.
 * @return true on success, false if generation fails.
 */
bool GenerateRandomBytes(unsigned char* buffer, int numBytes);

/**
 * @brief Generates a random alphanumeric filename.
 *
 * @param length The length of the random string to generate.
 * @return A random alphanumeric string.
 */
std::string GenerateRandomFilename(size_t length);

/**
 * @brief Encrypts plaintext using AES-256-CBC.
 *
 * This function uses a static key and IV (for offline operation) to encrypt the plaintext.
 *
 * @param plaintext The input data to encrypt.
 * @param ciphertext Output vector that will receive the encrypted data.
 * @return true on success, false if encryption fails.
 */
bool EncryptData(const std::vector<unsigned char>& plaintext, std::vector<unsigned char>& ciphertext);

/**
 * @brief Decrypts ciphertext using AES-256-CBC.
 *
 * This function uses a static key and IV to decrypt the ciphertext.
 *
 * @param ciphertext The input data to decrypt.
 * @param plaintext Output vector that will receive the decrypted data.
 * @return true on success, false if decryption fails.
 */
bool DecryptData(const std::vector<unsigned char>& ciphertext, std::vector<unsigned char>& plaintext);

/**
 * @brief Generates a secure random password.
 *
 * @param password Output vector that will receive the random password bytes.
 * @param length The number of bytes to generate (default is 32).
 * @return true on success, false if generation fails.
 */
bool GenerateRandomPassword(std::vector<unsigned char>& password, size_t length = 32);

/**
 * @brief Writes binary data to a file.
 *
 * @param filePath The path to the file.
 * @param data The binary data to write.
 * @return true on success, false if file write fails.
 */
bool WriteBinaryToFile(const std::string& filePath, const std::vector<unsigned char>& data);

/**
 * @brief Reads binary data from a file.
 *
 * @param filePath The path to the file.
 * @param data Output vector that will receive the file data.
 * @return true on success, false if file read fails.
 */
bool ReadBinaryFromFile(const std::string& filePath, std::vector<unsigned char>& data);

/**
 * @brief Generates a random password, encrypts it, and stores it in a file.
 *
 * The file is given a random name and stored in the specified directory.
 *
 * @param storageDir The directory where the file will be stored.
 * @param outFilePath Output parameter that receives the full path of the stored file.
 * @return true on success, false if any step fails.
 */
bool GenerateAndStorePassword(const std::string& storageDir, std::string& outFilePath);

/**
 * @brief Loads and decrypts a password from a file.
 *
 * Reads the encrypted password from the specified file and decrypts it.
 *
 * @param filePath The path to the encrypted password file.
 * @param password Output vector that will receive the decrypted password bytes.
 * @return true on success, false if decryption or file read fails.
 */
bool LoadAndDecryptPassword(const std::string& filePath, std::vector<unsigned char>& password);
