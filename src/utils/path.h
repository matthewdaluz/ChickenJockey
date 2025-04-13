#pragma once
#include <string>
#include <vector>

namespace PathUtil {

    // Generate a random alphanumeric filename with a specified length.
    // Example: "D7FHxHDUO834.txt" (default length = 16, default extension = ".txt")
    std::string GenerateRandomFilename(size_t length = 16, const std::string &extension = ".txt");

    // Construct a full file path using the given filename and base directory.
    // By default, it uses "C:\Program Files\UnrelatedProgram\" as the base directory.
    std::string GetObscureFilePath(const std::string &filename, const std::string &baseDir = "C:\\Program Files\\UnrelatedProgram\\");

    // Ensure that the provided directory exists. Creates it if necessary.
    bool EnsureDirectoryExists(const std::string &directory);

    // Securely writes binary data to the specified file path.
    bool WriteFile(const std::string &fullPath, const std::vector<unsigned char> &data);

    // Reads binary data from the specified file path.
    bool ReadFile(const std::string &fullPath, std::vector<unsigned char> &data);
}
