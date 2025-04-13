#include "watcher.h"
#include <windows.h>
#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include "blocker.h"

// Helper: Retrieve the last modified FILETIME for a given file.
FILETIME GetLastWriteTime(const std::string& filePath) {
    FILETIME ftWrite = {0};
    HANDLE hFile = CreateFileA(filePath.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile != INVALID_HANDLE_VALUE) {
        BY_HANDLE_FILE_INFORMATION fileInfo;
        if (GetFileInformationByHandle(hFile, &fileInfo)) {
            ftWrite = fileInfo.ftLastWriteTime;
        }
        CloseHandle(hFile);
    }
    return ftWrite;
}

// Helper: Compare two FILETIME structures.
bool IsFileTimeDifferent(const FILETIME& ft1, const FILETIME& ft2) {
    return (ft1.dwLowDateTime != ft2.dwLowDateTime) || (ft1.dwHighDateTime != ft2.dwHighDateTime);
}

// Helper: Relaunch the peer process given its role, using our own executable path.
bool RelaunchPeer(const std::string& peerRole, const std::string& exePath) {
    std::string commandLine = "\"" + exePath + "\" --watchdog " + peerRole;
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    // CreateProcess requires a modifiable C-string.
    char* cmdLineCStr = new char[commandLine.size() + 1];
    strcpy_s(cmdLineCStr, commandLine.size() + 1, commandLine.c_str());

    BOOL result = CreateProcessA(
        NULL,              // No module name—use command line.
        cmdLineCStr,       // Command line buffer.
        NULL,              // Process handle not inheritable.
        NULL,              // Thread handle not inheritable.
        FALSE,             // Do not inherit handles.
        0,                 // No creation flags.
        NULL,              // Use parent's environment.
        NULL,              // Use parent's current directory.
        &si,
        &pi
    );

    delete[] cmdLineCStr;
    
    if (!result) {
        std::cerr << "[Watcher] Error: Failed to relaunch peer process with role " << peerRole 
                  << ". Error code: " << GetLastError() << std::endl;
        return false;
    }
    
    std::cout << "[Watcher] Successfully relaunched peer process with role " << peerRole << "." << std::endl;
    // Immediately close the process and thread handles.
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return true;
}

// RunWatcher: Main function for a watcher process.
// Expects at least: --watchdog <A|B> and optionally a peerPID.
int RunWatcher(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "[Watcher] Insufficient arguments. Usage: --watchdog <A|B> [peerPID]" << std::endl;
        return 1;
    }
    
    // argv[1] is the flag "--watchdog", argv[2] holds the role ("A" or "B")
    std::string role = argv[2];
    std::string peerRole = (role == "A") ? "B" : "A";
    DWORD peerPID = 0;
    if (argc >= 4) {
        try {
            peerPID = std::stoul(argv[3]);
        } catch (...) {
            std::cerr << "[Watcher " << role << "] Warning: Invalid peerPID argument. Defaulting to peer not provided." << std::endl;
            peerPID = 0;
        }
    }
    
    // Retrieve the full path of the current executable for relaunching peer process.
    char exePath[MAX_PATH] = {0};
    GetModuleFileNameA(NULL, exePath, MAX_PATH);
    
    std::cout << "[Watcher " << role << "] Starting watcher process." << std::endl;
    std::cout << "[Watcher " << role << "] Monitoring hosts file and peer process (" 
              << (peerPID ? std::to_string(peerPID) : "not provided") << ")." << std::endl;

    // Create a Blocker object so that we may reapply blocks if tampering is detected.
    Blocker blocker;
    const std::string hostsPath = "C:\\Windows\\System32\\drivers\\etc\\hosts";
    FILETIME lastWriteTime = GetLastWriteTime(hostsPath);

    // Main monitoring loop.
    while (true) {
        // 1. Monitor the hosts file for unauthorized modifications.
        FILETIME currentWriteTime = GetLastWriteTime(hostsPath);
        if (IsFileTimeDifferent(lastWriteTime, currentWriteTime)) {
            std::cout << "[Watcher " << role << "] Detected hosts file modification." << std::endl;
            // Check if the hosts file is in a blocked state; if not, reapply the block.
            if (!blocker.isBlocked()) {
                std::cout << "[Watcher " << role << "] Tampering detected. Reapplying website blocks." << std::endl;
                if (!blocker.reapplyBlock()) {
                    std::cerr << "[Watcher " << role << "] Error: Failed to reapply website block." << std::endl;
                }
            }
            lastWriteTime = currentWriteTime;
        }

        // 2. Monitor the peer process (if peerPID provided).
        if (peerPID != 0) {
            HANDLE hPeer = OpenProcess(SYNCHRONIZE, FALSE, peerPID);
            if (hPeer == NULL) {
                std::cerr << "[Watcher " << role << "] Peer process (PID " << peerPID 
                          << ") not found. Attempting to relaunch peer with role " << peerRole << "." << std::endl;
                RelaunchPeer(peerRole, exePath);
                // Reset peerPID after relaunch.
                peerPID = 0;
            } else {
                DWORD waitStatus = WaitForSingleObject(hPeer, 0);
                if (waitStatus == WAIT_OBJECT_0) {
                    std::cerr << "[Watcher " << role << "] Peer process (PID " << peerPID 
                              << ") has terminated. Relaunching peer with role " << peerRole << "." << std::endl;
                    RelaunchPeer(peerRole, exePath);
                    CloseHandle(hPeer);
                    peerPID = 0;
                } else {
                    CloseHandle(hPeer);
                }
            }
        } else {
            // 3. If no peer PID is provided (or it was reset), periodically attempt to ensure the peer is running.
            std::cout << "[Watcher " << role << "] No peer PID provided. Verifying peer (role " 
                      << peerRole << ") is active." << std::endl;
            RelaunchPeer(peerRole, exePath);
        }
        
        // 4. Sleep to minimize resource usage.
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }

    return 0;
}
