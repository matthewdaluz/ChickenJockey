// path.cpp

#define NOMINMAX

#include <windows.h>
#include "path.h"
#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <vector>
#include <system_error>
#include <mutex>
#include <iostream>
#include <thread>
#include <chrono>

namespace PathUtil {
    namespace fs = std::filesystem;

    // ----- Constants & Configuration -----
    constexpr int MAX_DIR_CREATION_ATTEMPTS = 3;

    // ----- Thread-safe random number generation -----
    namespace {
        std::mutex rng_mutex;
        
        template<typename T>
        T get_cryptographically_secure_random() {
            static_assert(std::is_unsigned_v<T>, "T must be an unsigned type");
        
            std::lock_guard<std::mutex> lock(rng_mutex);
            std::random_device rd;
            if (rd.entropy() < 0.1) {
                throw std::runtime_error("Insufficient random device entropy");
            }
        
            // Use a wider distribution and cast down safely
            return static_cast<T>(std::uniform_int_distribution<uint16_t>{
                0, static_cast<uint16_t>(std::numeric_limits<T>::max())
            }(rd));
        }
        
    }

    // ----- Secure Filename Generation -----
    std::string GenerateRandomFilename(size_t length, const std::string& extension) {
        constexpr char charset[] = 
            "0123456789"
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "abcdefghijklmnopqrstuvwxyz";
        constexpr size_t charset_size = sizeof(charset) - 1;
        constexpr int max_acceptable = 256 - (256 % charset_size);

        std::string filename;
        filename.reserve(length + extension.size());

        for (size_t i = 0; i < length; ++i) {
            unsigned char byte;
            do {
                byte = get_cryptographically_secure_random<unsigned char>();
            } while (byte >= max_acceptable);
            
            filename += charset[byte % charset_size];
        }

        return filename + extension;
    }

    // ----- Secure Path Construction -----
    fs::path GetObscureFilePath(const fs::path& base_dir, const std::string& filename) {
        fs::path full_path = base_dir / filename;
        
        // Normalize path to prevent directory traversal
        full_path = full_path.lexically_normal();
        
        // Verify the path remains within base directory
        if (!fs::equivalent(full_path.parent_path(), base_dir)) {
            throw std::runtime_error("Invalid path construction attempt");
        }
        
        return full_path;
    }

    // ----- Secure Directory Creation -----
    bool EnsureDirectoryExists(const fs::path& directory) noexcept {
        std::error_code ec;
        
        for (int attempt = 0; attempt < MAX_DIR_CREATION_ATTEMPTS; ++attempt) {
            if (fs::create_directories(directory, ec)) return true;
            if (ec == std::errc::file_exists) return true;
            
            // Handle transient errors with retry
            if (ec == std::errc::resource_unavailable_try_again ||
                ec == std::errc::device_or_resource_busy) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }
            
            std::cerr << "[PathUtil] Directory creation failed: " << directory 
                      << " - " << ec.message() << "\n";
            return false;
        }
        
        return false;
    }

    // ----- Secure File Writing -----
    bool WriteFile(const fs::path& full_path, const std::vector<unsigned char>& data) noexcept {
        fs::path temp_path = full_path;
        temp_path += ".tmp";
        
        try {
            {
                std::ofstream ofs(temp_path, std::ios::binary | std::ios::trunc);
                if (!ofs) return false;
                
                ofs.exceptions(std::ofstream::failbit | std::ofstream::badbit);
                ofs.write(reinterpret_cast<const char*>(data.data()), data.size());
            }
            
            // Atomic file replacement
            if (!ReplaceFileW(full_path.c_str(), temp_path.c_str(), NULL, REPLACEFILE_IGNORE_MERGE_ERRORS, NULL, NULL)) {
                std::wcerr << L"[PathUtil] ReplaceFile failed. Error: " << GetLastError() << "\n";
                fs::remove(temp_path); // Clean up the temp file
                return false;
            }
            
            fs::permissions(full_path,
                fs::perms::owner_read | fs::perms::owner_write,
                fs::perm_options::replace);
            
            return true;
        } catch (const std::exception& e) {
            std::cerr << "[PathUtil] Write failed: " << e.what() << "\n";
            fs::remove(temp_path);
            return false;
        }
    }

    // ----- Secure File Reading -----
    bool ReadFile(const fs::path& full_path, std::vector<unsigned char>& data) noexcept {
        try {
            std::error_code ec;
            auto file_size = fs::file_size(full_path, ec);
            if (ec || file_size == static_cast<uintmax_t>(-1)) return false;

            data.resize(static_cast<size_t>(file_size));
            
            std::ifstream ifs(full_path, std::ios::binary);
            if (!ifs) return false;
            
            ifs.exceptions(std::ifstream::failbit | std::ifstream::badbit);
            ifs.read(reinterpret_cast<char*>(data.data()), data.size());
            
            return true;
        } catch (const std::exception& e) {
            std::cerr << "[PathUtil] Read failed: " << e.what() << "\n";
            data.clear();
            return false;
        }
    }

} // namespace PathUtil