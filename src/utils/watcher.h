#include "blocker.h"

#pragma once
#ifndef UTILS_WATCHER_H
#define UTILS_WATCHER_H

#include <windows.h>
#include <string>
#include <memory>
#include <filesystem>

namespace fs = std::filesystem;

namespace utils {

class Watcher {
public:
    static bool Initialize(); // <-- this was missing
    static int Run(int argc, char* argv[]);

private:
    Watcher() = delete;

    struct ProcessInfo {
        DWORD pid;
        std::string role;
        fs::path exe_path;
    };

    class HandleGuard {
    public:
        explicit HandleGuard(HANDLE h = nullptr) noexcept : handle(h) {}
        ~HandleGuard() { if (handle) CloseHandle(handle); }
        operator HANDLE() const noexcept { return handle; }
        HANDLE* operator&() noexcept { return &handle; }
        HandleGuard(const HandleGuard&) = delete;
        HandleGuard& operator=(const HandleGuard&) = delete;
    private:
        HANDLE handle;
    };

    static ProcessInfo ParseArguments(int argc, char* argv[]);
    static FILETIME GetLastWriteTime(const fs::path& filePath);
    static bool IsFileModified(const FILETIME& previous, const fs::path& filePath);
    static bool RestartPeer(const ProcessInfo& info, const std::string& peerRole);
    static bool MonitorHostsFile(Blocker& blocker, const fs::path& hostsPath, FILETIME& lastWriteTime);
    static bool MonitorPeerProcess(DWORD& peerPID, const ProcessInfo& info, int& restartCount);
};

} // namespace utils

#endif // UTILS_WATCHER_H
