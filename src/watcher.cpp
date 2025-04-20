#include "utils/watcher.h"
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
    const int MAX_RESTARTS = 5;
    int restartCount = 0;

    if (argc < 3) {
        std::cerr << "[Watcher] Insufficient arguments. Usage: --watchdog <A|B> [peerPID]" << std::endl;
        return 1;
    }

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

    char exePath[MAX_PATH] = {0};
    GetModuleFileNameA(NULL, exePath, MAX_PATH);

    std::cout << "[Watcher " << role << "] Starting watcher process." << std::endl;
    std::cout << "[Watcher " << role << "] Monitoring hosts file and peer process (" 
              << (peerPID ? std::to_string(peerPID) : "not provided") << ")." << std::endl;

    Blocker blocker;
    const std::string hostsPath = "C:\\Windows\\System32\\drivers\\etc\\hosts";
    FILETIME lastWriteTime = GetLastWriteTime(hostsPath);

    while (true) {
        // Hosts file monitoring
        FILETIME currentWriteTime = GetLastWriteTime(hostsPath);
        if (IsFileTimeDifferent(lastWriteTime, currentWriteTime)) {
            std::cout << "[Watcher " << role << "] Detected hosts file modification." << std::endl;
            if (!blocker.isBlocked()) {
                std::cout << "[Watcher " << role << "] Tampering detected. Reapplying website blocks." << std::endl;
                if (!blocker.reapplyBlock()) {
                    std::cerr << "[Watcher " << role << "] Error: Failed to reapply website block." << std::endl;
                }
            }
            lastWriteTime = currentWriteTime;
        }

        // Peer process monitoring and restart
        bool shouldAttemptRestart = false;

        if (peerPID != 0) {
            HANDLE hPeer = OpenProcess(SYNCHRONIZE, FALSE, peerPID);
            if (hPeer == NULL || WaitForSingleObject(hPeer, 0) == WAIT_OBJECT_0) {
                std::cerr << "[Watcher " << role << "] Peer (PID " << peerPID << ") has exited or is unavailable." << std::endl;
                shouldAttemptRestart = true;
                if (hPeer) CloseHandle(hPeer);
                peerPID = 0;
            } else {
                CloseHandle(hPeer);
            }
        } else {
            std::cout << "[Watcher " << role << "] No peer PID. Checking if peer needs relaunch." << std::endl;
            shouldAttemptRestart = true;
        }

        // Restart logic with cap
        if (shouldAttemptRestart) {
            if (restartCount >= MAX_RESTARTS) {
                std::cerr << "[Watcher " << role << "] Max restart attempts for peer reached. Halting relaunch." << std::endl;
            } else {
                if (RelaunchPeer(peerRole, exePath)) {
                    restartCount++;
                    std::cout << "[Watcher " << role << "] Relaunch attempt #" << restartCount << std::endl;
                }
                std::this_thread::sleep_for(std::chrono::seconds(10)); // cooldown delay
            }
        }

        std::this_thread::sleep_for(std::chrono::seconds(5));
    }

    return 0;
}
