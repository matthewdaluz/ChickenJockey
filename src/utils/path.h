// path.h
#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace PathUtil {
    namespace fs = std::filesystem;

    // ----- Constants -----
    constexpr size_t DEFAULT_FILENAME_LENGTH = 16;
    constexpr const char* DEFAULT_EXTENSION = ".txt";
    constexpr const char* DEFAULT_BASE_DIR = "C:\\Program Files\\UnrelatedProgram\\";

    // ----- Secure Filename Generation -----
    // Generates a cryptographically secure random filename
    // Length: Number of random characters (default 16)
    // Extension: File extension including dot (default ".txt")
    std::string GenerateRandomFilename(
        size_t length = DEFAULT_FILENAME_LENGTH,
        const std::string& extension = DEFAULT_EXTENSION
    );

    // ----- Secure Path Construction -----
    // Constructs a full path with security checks
    // filename: The target filename
    // baseDir: Base directory (defaults to DEFAULT_BASE_DIR)
    // Returns: Normalized absolute path
    // Throws: std::runtime_error if path construction is unsafe
    fs::path GetObscureFilePath(
        const fs::path& filename,
        const fs::path& baseDir = fs::path(DEFAULT_BASE_DIR)
    );

    // Backward-compatible version using strings
    inline std::string GetObscureFilePath(
        const std::string& filename,
        const std::string& baseDir = DEFAULT_BASE_DIR
    ) {
        return GetObscureFilePath(fs::path(filename), fs::path(baseDir)).string();
    }

    // ----- Secure Directory Handling -----
    // Creates directory with all parents if needed
    // Implements retry logic for transient failures
    // Returns: true if directory exists or was created successfully
    bool EnsureDirectoryExists(const fs::path& directory) noexcept;

    // Backward-compatible version using string
    inline bool EnsureDirectoryExists(const std::string& directory) noexcept {
        return EnsureDirectoryExists(fs::path(directory));
    }

    // ----- Secure File Operations -----
    // Writes data atomically using a temporary file
    // fullPath: Destination file path
    // data: Binary data to write
    // Returns: true if write succeeded
    bool WriteFile(const fs::path& fullPath, const std::vector<unsigned char>& data) noexcept;

    // Backward-compatible version using string
    inline bool WriteFile(
        const std::string& fullPath,
        const std::vector<unsigned char>& data
    ) noexcept {
        return WriteFile(fs::path(fullPath), data);
    }

    // Reads file contents securely
    // fullPath: Source file path
    // data: Output buffer (will be cleared on failure)
    // Returns: true if read succeeded
    bool ReadFile(const fs::path& fullPath, std::vector<unsigned char>& data) noexcept;

    // Backward-compatible version using string
    inline bool ReadFile(
        const std::string& fullPath,
        std::vector<unsigned char>& data
    ) noexcept {
        return ReadFile(fs::path(fullPath), data);
    }
} // namespace PathUtil