// main.cpp

#include <windows.h>
#include <iostream>
#include <exception>
#include <string>
#include <vector>
#include <sddl.h>

#include "blocker.h"
#include "utils/watcher.h"
#include "gui.h"
#include "crypto.h"
#include "path.h"
#include <thread>   // Needed for std::this_thread
#include <chrono>      // for std::chrono::milliseconds
#include <fstream>       // Required for std::ofstream

bool PerformFactoryReset();
constexpr int EXIT_ADMIN_REQUIRED = 1001;
constexpr int EXIT_WATCHDOG_ERROR = 1003;

// Check for administrative privileges
bool IsRunningAsAdmin() {
    BOOL isAdmin = FALSE;
    PSID adminGroup = nullptr;
    SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;

    if (!AllocateAndInitializeSid(&NtAuthority, 2,
        SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS,
        0, 0, 0, 0, 0, 0, &adminGroup)) {
        return false;
    }

    if (!CheckTokenMembership(nullptr, adminGroup, &isAdmin)) {
        isAdmin = FALSE;
    }

    FreeSid(adminGroup);
    return isAdmin == TRUE;
}

void TestCryptoFunctions() {
    try {
        std::filesystem::path storageDir = L"C:\\ProgramData\\ChickenJockey";
        std::filesystem::path encryptedFilePath;
        std::vector<unsigned char> decryptedPassword;

        if (!crypto::GenerateAndStorePassword(storageDir, encryptedFilePath)) {
            throw std::runtime_error("Password generation failed");
        }

        if (!crypto::LoadAndDecryptPassword(encryptedFilePath, decryptedPassword)) {
            throw std::runtime_error("Decryption failed");
        }

        SecureZeroMemory(decryptedPassword.data(), decryptedPassword.size());
        std::wcout << L"[Crypto Test] All operations completed successfully." << std::endl;
    }
    catch (const std::exception& ex) {
        std::wcerr << L"[Crypto Error] " << ex.what() << std::endl;
    }
}

void RunDebugMode() {
    std::wcout << L"\n===== [Debug] Starting Chicken Jockey Diagnostic Tests =====\n\n";
    
    // Create Blocker with debug enabled
    Blocker blocker;
    blocker.setDebugMode(true); // Enable debug mode
    
    // Section 1: Basic System Checks
    std::wcout << L"[Debug] === System Checks ===\n";
    
    // Test admin privileges check (now accessible)
    bool isAdmin = blocker.checkAdminPrivileges();
    std::wcout << L"[Debug] Admin privileges: " << (isAdmin ? L"Yes" : L"No") << L"\n";
    
    // Test hosts file accessibility using new getter method
    bool hostsExists = fs::exists(blocker.getHostsPath());
    std::wcout << L"[Debug] Hosts file exists: " << (hostsExists ? L"Yes" : L"No") << L"\n";
    if (hostsExists) {
        std::wcout << L"[Debug] Hosts file size: " 
                  << fs::file_size(blocker.getHostsPath()) << L" bytes\n";
        std::wcout << L"[Debug] Hosts file path: " << blocker.getHostsPath() << L"\n";
    }
    
    // Section 2: Core Functionality Tests
    std::wcout << L"\n[Debug] === Core Functionality Tests ===\n";
    
    // Test hosts file status
    bool isBlocked = blocker.isBlocked();
    std::wcout << L"[Debug] Initial blocked status: " 
              << (isBlocked ? L"Blocked" : L"Not blocked") << L"\n";
    
    // Test loading domains
    std::vector<std::string> testDomains = {"example.com", "test.org", "debug.example.net"};
    std::wcout << L"[Debug] Testing with " << testDomains.size() << L" domains\n";
    if (blocker.loadDomains(testDomains)) {
        std::wcout << L"[Debug] Domain loading test passed\n";
        
        // Test backup functionality
        std::wcout << L"[Debug] Testing backup functionality...\n";
        if (blocker.backupHosts()) {
            std::wcout << L"[Debug] Backup created successfully\n";
            
            // Test applying block
            std::wcout << L"[Debug] Testing block application...\n";
            if (blocker.applyBlock()) {
                std::wcout << L"[Debug] Block applied successfully\n";
                
                // Verify block status
                if (blocker.isBlocked()) {
                    std::wcout << L"[Debug] Block verification passed\n";
                } else {
                    std::wcerr << L"[Debug] ERROR: Block verification failed\n";
                }
                
                // Test reapply
                std::wcout << L"[Debug] Testing reapply functionality...\n";
                if (blocker.reapplyBlock()) {
                    std::wcout << L"[Debug] Reapply test passed\n";
                } else {
                    std::wcerr << L"[Debug] ERROR: Reapply test failed\n";
                }
            } else {
                std::wcerr << L"[Debug] ERROR: Failed to apply block\n";
            }
        } else {
            std::wcerr << L"[Debug] ERROR: Failed to create backup\n";
        }
    } else {
        std::wcerr << L"[Debug] ERROR: Domain loading test failed\n";
    }
    
    // Section 3: File Operations
    std::wcout << L"\n[Debug] === File Operation Tests ===\n";
    try {
        fs::path testFilePath = L"C:\\ProgramData\\ChickenJockey\\debug_test.txt";
        std::wcout << L"[Debug] Testing secureWrite with test file: " << testFilePath << L"\n";
        
        if (blocker.secureWrite(testFilePath, "This is a test content\nSecond line")) {
            std::wcout << L"[Debug] secureWrite test passed\n";
            
            // Verify file content
            std::ifstream testFile(testFilePath);
            if (testFile) {
                std::wcout << L"[Debug] File content verification passed\n";
                testFile.close();
            } else {
                std::wcerr << L"[Debug] ERROR: Failed to verify file content\n";
            }
            
            // Clean up
            fs::remove(testFilePath);
        } else {
            std::wcerr << L"[Debug] ERROR: secureWrite test failed\n";
        }
    } catch (const std::exception& e) {
        std::wcerr << L"[Debug] EXCEPTION in file operation test: " << e.what() << L"\n";
    }
    
    // Section 4: Watcher Tests
    std::wcout << L"\n[Debug] === Watcher Tests ===\n";
    if (!utils::Watcher::Initialize()) {
        std::wcerr << L"[Debug] ERROR: Watcher initialization failed\n";
        throw std::runtime_error("Watcher initialization failed");
    }
    std::wcout << L"[Debug] Watcher test passed\n";
    
    // Final summary
    std::wcout << L"\n===== [Debug] Diagnostic Tests Completed =====\n\n";
    
    // Prompt user to check results
    MessageBoxW(nullptr,
        L"Debug tests completed. Check console for results.\n"
        L"All tests should show as passed for normal operation.",
        L"Debug Tests Complete", MB_ICONINFORMATION | MB_OK);
}

void ShowHelp() {
    std::wcout << L"Chicken Jockey - Website Blocker v0.3\n"
               << L"Usage:\n"
               << L"  --gui              Launch graphical interface\n"
               << L"  --debug            Run diagnostic tests\n"
               << L"  --test-crypto      Test encryption modules\n"
               << L"  --watchdog <A|B>   Run as watchdog process\n"
               << L"  --stop-everything  Kill all Chicken Jockey processes\n"
               << L"  --factory-reset    Restore defaults and delete app data\n"
               << L"  --help             Show this help message\n";
}


bool ConfirmAndStopEverything() {
    int response = MessageBoxW(nullptr,
        L"Are you sure you want to shut down Chicken Jockey?\n\n"
        L"This will stop the watchdog processes and remove all protections.\n\n"
        L"If you’re struggling with porn addiction, please consider keeping Chicken Jockey active.\n"
        L"Remember: cravings are temporary — your goals are not.",
        L"WARNING: Disable Protection?", MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2);

    if (response != IDYES) {
        MessageBoxW(nullptr,
            L"Chicken Jockey will remain active. Stay strong — you’ve got this.",
            L"Stay Focused", MB_OK | MB_ICONINFORMATION);
        return false;
    }

    // Kill both watchdogs if they are running
    DWORD currentPID = GetCurrentProcessId();
    std::wstring cmd = L"wmic process where \"name='ChickenJockey.exe' and ProcessId!=" + std::to_wstring(currentPID) + L"\" call terminate >nul 2>&1";
    _wsystem(cmd.c_str());
    std::this_thread::sleep_for(std::chrono::milliseconds(250));

    MessageBoxW(nullptr,
        L"Chicken Jockey has been stopped.\nWe hope you return stronger.\n\nTake care.",
        L"Protection Disabled", MB_OK | MB_ICONINFORMATION);
    return true;
}

#include <fstream>  // Make sure this is at the top of your file

bool PerformFactoryReset() {
    int response = MessageBoxW(nullptr,
        L"Are you sure you want to reset Chicken Jockey?\n\n"
        L"This will restore the original hosts file and delete all app data.\n"
        L"If you’re battling porn addiction, remember: you’ve come a long way.\n"
        L"Don’t let one moment of weakness undo your progress.",
        L"Factory Reset", MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2);

    if (response != IDYES) {
        MessageBoxW(nullptr,
            L"Chicken Jockey remains active. Stay strong — you’ve got this.",
            L"Stay Focused", MB_OK | MB_ICONINFORMATION);
        return false;
    }

    // 💣 Kill other ChickenJockey instances
    DWORD currentPID = GetCurrentProcessId();
    std::wstring cmd = L"wmic process where \"name='ChickenJockey.exe' and ProcessId!=" + std::to_wstring(currentPID) + L"\" call terminate >nul 2>&1";
    _wsystem(cmd.c_str());
    Sleep(250);

    // 🛡 Fix permissions first
    system("icacls C:\\Windows\\System32\\drivers\\etc\\hosts /grant Everyone:F >nul 2>&1");
    system("icacls C:\\Windows\\System32\\drivers\\etc\\hosts /inheritance:r >nul 2>&1");

    // 📁 Attempt to restore from backup
    std::filesystem::path backupPath = L"C:\\ProgramData\\ChickenJockey\\hosts.bak";
    std::filesystem::path targetPath = L"C:\\Windows\\System32\\drivers\\etc\\hosts";
    std::error_code ec;

    bool restored = false;
    if (std::filesystem::exists(backupPath)) {
        std::filesystem::copy_file(backupPath, targetPath, std::filesystem::copy_options::overwrite_existing, ec);
        if (!ec) {
            restored = true;
             // std::filesystem::remove(backupPath, ec); (Not needed due to issues)
        } else {
            std::wcerr << L"[Reset] Failed to restore hosts file: " << ec.message().c_str() << std::endl;
        }
    }

    // ✏️ Recreate minimal default hosts file if missing or not restored
    if (!restored || !std::filesystem::exists(targetPath)) {
        std::ofstream hosts(targetPath);
        if (hosts.is_open()) {
            hosts << "127.0.0.1 localhost\n";
            hosts << "::1 localhost\n";
            hosts.close();
        } else {
            MessageBoxW(nullptr,
                L"[Reset Error] Failed to recreate missing hosts file.\nPlease create it manually:\n\nC:\\Windows\\System32\\drivers\\etc\\hosts",
                L"Critical Error", MB_OK | MB_ICONERROR);
            return false;
        }
    }

    // 🧹 Remove Chicken Jockey config directory
    std::filesystem::remove_all(L"C:\\ProgramData\\ChickenJockey", ec);

    MessageBoxW(nullptr,
        L"Chicken Jockey has been reset.\nWe hope to see you again — stronger and more focused.\n\nTake care.",
        L"Factory Reset Complete", MB_OK | MB_ICONINFORMATION);

    return true;
}

int wmain(int argc, wchar_t* argv[]) {
    SetConsoleOutputCP(CP_UTF8);
    std::wcout << L"----- Chicken Jockey Initialization -----\n";

    // Remove the standalone debugMode declaration here
    // bool debugMode = false;  // <-- This is the duplicate causing the error

    if (!IsRunningAsAdmin()) {
        std::wcerr << L"[Error] Administrator privileges required.\n";
        MessageBoxW(nullptr,
            L"Chicken Jockey requires administrator rights.\n\n"
            L"Please run this program as Administrator.",
            L"Permission Denied", MB_ICONERROR | MB_OK);
        return EXIT_ADMIN_REQUIRED;
    }

    // This is the only place debugMode should be declared
    bool guiMode = false, debugMode = false, cryptoTest = false, stopAll = false, factoryReset = false;

    for (int i = 1; i < argc; ++i) {
        std::wstring arg = argv[i];
        if (arg == L"--gui") guiMode = true;
        else if (arg == L"--stop-everything") stopAll = true;
        else if (arg == L"--factory-reset") factoryReset = true;
        else if (arg == L"--debug") debugMode = true;
        else if (arg == L"--test-crypto") cryptoTest = true;
        else if (arg == L"--help") {
            ShowHelp();
            return 0;
        } else {
            std::wcerr << L"[Error] Unknown option: " << arg << L"\n";
            ShowHelp();
            return 1;
        }
    }

    
    if (stopAll) {
        bool result = ConfirmAndStopEverything();
        ExitProcess(result ? 0 : 1);
    }
    
    if (factoryReset) {
        bool result = PerformFactoryReset();
        ExitProcess(result ? 0 : 1);
    }
    
    

    // 🔒 Enforce blocking after those critical flags
    Blocker blocker;
    if (blocker.isBlocked()) {
        std::wcerr << L"[Chicken Jockey] Access denied: Hosts file is already blocked.\n";
        MessageBoxW(nullptr,
            L"Chicken Jockey has already applied website blocks and is now locked.\n"
            L"Watchdog processes will continue running in the background.",
            L"Access Denied", MB_ICONWARNING | MB_OK);
        return EXIT_FAILURE;
    }

    // 🧠 Main logic modes
    if (guiMode || argc == 1) {
        std::wcout << L"Launching GUI...\n";
        return RunGUI();
    }

    if (debugMode) {
        try {
            RunDebugMode();
            return 0;
        } catch (...) {
            return 1;
        }
    }

    if (cryptoTest) {
        TestCryptoFunctions();
        return 0;
    }

    ShowHelp();
    return 1;
}
