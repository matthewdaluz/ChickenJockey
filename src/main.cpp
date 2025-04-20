// Chicken Jockey
// Main entry point for the program

#include <windows.h>
#include <iostream>
#include <exception>
#include <string>
#include <vector>

// Internal modules
#include "blocker.h"
#include "utils/watcher.h"
#include "service.h"
#include "gui.h"
#include "crypto.h"
#include "path.h"

// Check for write permissions on the hosts file
bool HasHostsWritePermission() {
    HANDLE hFile = CreateFileA("C:\\Windows\\System32\\drivers\\etc\\hosts",
                               GENERIC_WRITE, FILE_SHARE_WRITE, NULL,
                               OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile != INVALID_HANDLE_VALUE) {
        CloseHandle(hFile);
        return true;
    }
    return false;
}

// Dummy stubs for module initialization used in Debug Mode.
bool InitializeBlocker() {
    Blocker blocker;
    return true;
}

bool InitializeWatcher() {
    return true;
}

// Function to run the program in Debug Mode.
void RunDebugMode() {
    std::cout << "[Debug] Initializing Chicken Jockey in debug mode..." << std::endl;
    
    try {
        if (!InitializeBlocker()) {
            throw std::runtime_error("Blocker initialization failed.");
        }
        std::cout << "[Debug] Blocker module initialized successfully." << std::endl;
        
        if (!InitializeWatcher()) {
            throw std::runtime_error("Watcher initialization failed.");
        }
        std::cout << "[Debug] Watcher module initialized successfully." << std::endl;
    } catch (const std::exception &ex) {
        std::cerr << "[Error] Module initialization error: " << ex.what() << std::endl;
        throw;
    }
    
    std::cout << "[Debug] Chicken Jockey is running in Debug Mode." << std::endl;
    std::cout << "Press Enter to exit debug mode..." << std::endl;
    std::cin.get();
}

// Optional: Test function for crypto (generates, encrypts, stores, then decrypts the password).
void TestCryptoFunctions() {
    std::string storageDir = "C:\\ProgramData\\ChickenJockey";
    if (!PathUtil::EnsureDirectoryExists(storageDir)) {
        std::cerr << "[Crypto Test] Failed to ensure storage directory exists." << std::endl;
        return;
    }
    std::string encryptedFilePath;
    if (!GenerateAndStorePassword(storageDir, encryptedFilePath)) {
        std::cerr << "[Crypto Test] Failed to generate and store password." << std::endl;
        return;
    }
    std::cout << "[Crypto Test] Encrypted password stored at: " << encryptedFilePath << std::endl;

    std::vector<unsigned char> decryptedPassword;
    if (!LoadAndDecryptPassword(encryptedFilePath, decryptedPassword)) {
        std::cerr << "[Crypto Test] Failed to load and decrypt password." << std::endl;
        return;
    }
    std::cout << "[Crypto Test] Password successfully decrypted." << std::endl;
}

int main(int argc, char* argv[]) {
    std::cout << "----- Chicken Jockey Initialization -----" << std::endl;

    // Check for required permission
    if (!HasHostsWritePermission()) {
        std::cerr << "[Error] This program requires administrative privileges to modify the hosts file." << std::endl;
        MessageBoxA(NULL,
            "Chicken Jockey requires administrator rights to block websites.\n\n"
            "Please run this program as Administrator.",
            "Permission Denied",
            MB_ICONERROR | MB_OK);
        return EXIT_FAILURE;
    }

    // Command-line argument flags
    bool debugMode = false;
    bool guiMode = false;
    bool cryptoTest = false;
    bool installService = false;

    // Parse command-line arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--debug" || arg == "-debug") {
            debugMode = true;
        } else if (arg == "--gui" || arg == "-gui") {
            guiMode = true;
        } else if (arg == "--test-crypto") {
            cryptoTest = true;
        } else if (arg == "--install-service" || arg == "-install-service") {
            installService = true;
        }
    }

    if (installService) {
        if (!InstallService()) {
            std::cerr << "[Installer] Failed to install service." << std::endl;
            return EXIT_FAILURE;
        }
        std::cout << "[Installer] Service installed. You can now start it with `sc start ChickenJockeyService`." << std::endl;
        return EXIT_SUCCESS;
    }

    if (cryptoTest) {
        TestCryptoFunctions();
        return EXIT_SUCCESS;
    }

    if (guiMode) {
        std::cout << "Launching GUI for initial configuration." << std::endl;
        int guiResult = RunGUI();
        std::cout << "GUI configuration completed with exit code: " << guiResult << std::endl;
        return guiResult;
    }

    if (debugMode) {
        std::cout << "Running in Debug Mode." << std::endl;
        try {
            RunDebugMode();
        } catch (const std::exception& e) {
            std::cerr << "[Critical Error] " << e.what() << std::endl;
            return EXIT_FAILURE;
        }
    } else {
        std::cout << "Running as a Windows Service." << std::endl;
        if (!StartServiceHandler()) {
            std::cerr << "[Error] Windows Service handler failed to start." << std::endl;
            return EXIT_FAILURE;
        }
    }

    std::cout << "Chicken Jockey exiting." << std::endl;
    return EXIT_SUCCESS;
}
