#include "path.h"
#include <windows.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <random>
#include <algorithm>

namespace PathUtil {

// ---------------------------------------------------------------------
// GenerateRandomFilename:
// Creates a random alphanumeric filename with the given length and appends
// the provided file extension.
std::string GenerateRandomFilename(size_t length, const std::string &extension) {
    const std::string charset = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    std::string filename;
    filename.reserve(length);
    
    // Use std::random_device for randomness.
    std::random_device rd;
    std::uniform_int_distribution<> distrib(0, static_cast<int>(charset.size() - 1));
    
    for (size_t i = 0; i < length; ++i) {
        filename.push_back(charset[distrib(rd)]);
    }
    filename += extension;
    return filename;
}

// ---------------------------------------------------------------------
// GetObscureFilePath:
// Returns a full path string by concatenating the base directory (default or user-supplied)
// with the given filename. Ensures the base directory ends with a backslash.
std::string GetObscureFilePath(const std::string &filename, const std::string &baseDir) {
    std::string path = baseDir;
    if (!path.empty() && path.back() != '\\' && path.back() != '/') {
        path.push_back('\\');
    }
    path.append(filename);
    return path;
}

// ---------------------------------------------------------------------
// EnsureDirectoryExists:
// Checks if the specified directory exists; if not, attempts to create it.
// Returns true on success.
bool EnsureDirectoryExists(const std::string &directory) {
    DWORD attributes = GetFileAttributesA(directory.c_str());
    if (attributes != INVALID_FILE_ATTRIBUTES) {
        if (attributes & FILE_ATTRIBUTE_DIRECTORY) {
            return true;
        } else {
            std::cerr << "[PathUtil] Error: " << directory << " exists but is not a directory." << std::endl;
            return false;
        }
    }
    // Directory does not exist; attempt to create it.
    if (CreateDirectoryA(directory.c_str(), NULL) || GetLastError() == ERROR_ALREADY_EXISTS) {
        std::cout << "[PathUtil] Directory created or already exists: " << directory << std::endl;
        return true;
    } else {
        std::cerr << "[PathUtil] Error: Failed to create directory: " << directory 
                  << " (Error code: " << GetLastError() << ")" << std::endl;
        return false;
    }
}

// ---------------------------------------------------------------------
// WriteFile:
// Writes the provided binary data to the full file path.
// Returns true on success.
bool WriteFile(const std::string &fullPath, const std::vector<unsigned char> &data) {
    std::ofstream ofs(fullPath, std::ios::binary);
    if (!ofs) {
        std::cerr << "[PathUtil] Error: Unable to open file for writing: " << fullPath << std::endl;
        return false;
    }
    ofs.write(reinterpret_cast<const char*>(data.data()), data.size());
    if (!ofs.good()) {
        std::cerr << "[PathUtil] Error: Failed during write to file: " << fullPath << std::endl;
        return false;
    }
    ofs.close();
    std::cout << "[PathUtil] Successfully wrote file: " << fullPath << std::endl;
    return true;
}

// ---------------------------------------------------------------------
// ReadFile:
// Reads binary data from the specified file path into the provided vector.
// Returns true on success.
bool ReadFile(const std::string &fullPath, std::vector<unsigned char> &data) {
    std::ifstream ifs(fullPath, std::ios::binary | std::ios::ate);
    if (!ifs) {
        std::cerr << "[PathUtil] Error: Unable to open file for reading: " << fullPath << std::endl;
        return false;
    }
    std::streamsize size = ifs.tellg();
    if (size <= 0) {
        std::cerr << "[PathUtil] Error: File is empty or unreadable: " << fullPath << std::endl;
        return false;
    }
    ifs.seekg(0, std::ios::beg);
    data.resize(static_cast<size_t>(size));
    if (!ifs.read(reinterpret_cast<char*>(data.data()), size)) {
        std::cerr << "[PathUtil] Error: Failed to read file: " << fullPath << std::endl;
        return false;
    }
    ifs.close();
    std::cout << "[PathUtil] Successfully read file: " << fullPath << std::endl;
    return true;
}

} // namespace PathUtil
