// watcher.cpp
#include "watcher.h"
#include "blocker.h"
#include <windows.h>
#include <iostream>
#include <sstream>
#include <thread>
#include <chrono>
#include <vector>
#include <stringapiset.h>

namespace fs = std::filesystem;  // Filesystem namespace alias

namespace {

constexpr DWORD MAX_RESTARTS = 5;
constexpr std::chrono::seconds MONITOR_INTERVAL(5);
constexpr std::chrono::seconds RESTART_COOLDOWN(10);
constexpr DWORD MAX_PATH_LENGTH = 32767;

// Secure command line construction
std::wstring CreateCommandLine(const std::wstring& exePath, const std::wstring& role) {
    std::wostringstream oss;
    oss << L"\"" << exePath << L"\" --watchdog " << role;
    return oss.str();
}

// RAII wrapper for process creation
struct ProcessGuard {
    PROCESS_INFORMATION pi{};
    ~ProcessGuard() {
        if (pi.hProcess) CloseHandle(pi.hProcess);
        if (pi.hThread) CloseHandle(pi.hThread);
    }
};

} // anonymous namespace

namespace utils {

// Initialize static method
bool Watcher::Initialize() {
    std::wcout << L"[Watcher] Initializing watchdog system\n";
    return true; // Initialization logic placeholder
}

int Watcher::Run(int argc, char* argv[]) {
    try {
        auto [pid, role, exe_path] = ParseArguments(argc, argv); // Fixed structured binding
        Blocker blocker;
        fs::path hostsPath = L"C:\\Windows\\System32\\drivers\\etc\\hosts";
        FILETIME lastWriteTime = GetLastWriteTime(hostsPath);
        int restartCount = 0;

        std::wcout << L"[Watcher " << role.c_str() << L"] Monitoring system (PID: " 
                  << GetCurrentProcessId() << L")\n";

        while (true) {
            if (!MonitorHostsFile(blocker, hostsPath, lastWriteTime)) {
                std::cerr << "[Critical] Hosts file monitoring failed\n";
                return EXIT_FAILURE;
            }

            if (!MonitorPeerProcess(pid, {pid, role, exe_path}, restartCount)) { // Fixed struct init
                std::cerr << "[Critical] Peer monitoring failed\n";
                return EXIT_FAILURE;
            }

            std::this_thread::sleep_for(MONITOR_INTERVAL);
        }
    } catch (const std::exception& e) {
        std::cerr << "[Fatal Error] " << e.what() << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

Watcher::ProcessInfo Watcher::ParseArguments(int argc, char* argv[]) {
    if (argc < 3) {
        throw std::invalid_argument("Insufficient arguments. Usage: --watchdog <A|B> [peerPID]");
    }

    ProcessInfo info;
    info.role = argv[2];
    if (info.role != "A" && info.role != "B") {
        throw std::invalid_argument("Invalid role. Must be 'A' or 'B'");
    }

    wchar_t exePath[MAX_PATH_LENGTH];
    if (!GetModuleFileNameW(nullptr, exePath, MAX_PATH_LENGTH)) {
        throw std::runtime_error("Failed to get executable path");
    }
    info.exe_path = fs::path(exePath);

    if (argc >= 4) {
        try {
            info.pid = std::stoul(argv[3]);
        } catch (...) {
            std::cerr << "[Warning] Invalid peer PID format\n";
            info.pid = 0;
        }
    }

    return info;
}

FILETIME Watcher::GetLastWriteTime(const fs::path& filePath) {
    // CreateFileW returns a raw HANDLE, we wrap it with HandleGuard
    HandleGuard hFile(CreateFileW(
        filePath.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    ));

    if (!hFile || hFile == INVALID_HANDLE_VALUE) {
        throw std::runtime_error("Failed to open file for timestamp check");
    }

    FILETIME ftWrite{};
    if (!GetFileTime(hFile, nullptr, nullptr, &ftWrite)) {
        throw std::runtime_error("Failed to get file timestamp");
    }

    return ftWrite;
}


bool Watcher::IsFileModified(const FILETIME& previous, const fs::path& filePath) {
    const FILETIME current = GetLastWriteTime(filePath);
    return CompareFileTime(&previous, &current) != 0;
}

bool Watcher::MonitorHostsFile(Blocker& blocker, const fs::path& hostsPath, FILETIME& lastWriteTime) {
    try {
        if (IsFileModified(lastWriteTime, hostsPath)) {
            std::wcout << L"[Watcher] Hosts file modification detected\n";
            
            if (!blocker.isBlocked() && !blocker.reapplyBlock()) {
                std::cerr << "[Error] Failed to restore block\n";
                return false;
            }

            lastWriteTime = GetLastWriteTime(hostsPath);
        }
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[Monitor Error] " << e.what() << '\n';
        return false;
    }
}

bool Watcher::RestartPeer(const ProcessInfo& info, const std::string& peerRole) {
    // Proper wide-string conversion
    const std::wstring wPeerRole(peerRole.begin(), peerRole.end());
    const std::wstring commandLine = CreateCommandLine(info.exe_path.wstring(), wPeerRole);

    STARTUPINFOW si = { sizeof(STARTUPINFOW) };
    ProcessGuard process;

    if (!CreateProcessW(
        nullptr,
        const_cast<wchar_t*>(commandLine.c_str()),
        nullptr,
        nullptr,
        FALSE,
        CREATE_NEW_CONSOLE,
        nullptr,
        nullptr,
        &si,
        &process.pi
    )) {
        std::cerr << "[Restart Error] CreateProcess failed (" << GetLastError() << ")\n";
        return false;
    }

    std::wcout << L"[Watcher] Successfully restarted peer " << wPeerRole 
              << L" (PID: " << process.pi.dwProcessId << L")\n";
    return true;
}

bool Watcher::MonitorPeerProcess(DWORD& peerPID, const ProcessInfo& info, int& restartCount) {
    if (peerPID == 0) {
        if (restartCount < MAX_RESTARTS && RestartPeer(info, info.role == "A" ? "B" : "A")) {
            restartCount++;
            std::this_thread::sleep_for(RESTART_COOLDOWN);
        }
        return true;
    }

    HandleGuard hProcess(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, peerPID));
    if (!hProcess || hProcess == INVALID_HANDLE_VALUE) {
        std::cerr << "[Peer Error] Process " << peerPID << " not found\n";
        peerPID = 0;
        return true;
    }
    
    DWORD exitCode = STILL_ACTIVE;
    if (!GetExitCodeProcess(hProcess, &exitCode) || exitCode != STILL_ACTIVE) {
        std::cerr << "[Peer Alert] Process " << peerPID << " terminated\n";
        peerPID = 0;
    }
    

    return true;
}

} // namespace utils