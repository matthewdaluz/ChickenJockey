// blocker.h
#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;

class Blocker {
public:
    Blocker(const fs::path& hostsPath = fs::path(R"(C:\Windows\System32\drivers\etc\hosts)"),
            const fs::path& backupPath = fs::path(R"(C:\ProgramData\ChickenJockey\hosts_backup.txt)"),
            bool debugMode = false);
    
    bool loadDomains(const std::vector<std::string>& domains);
    bool loadDomainsFromFile(const fs::path& filePath);
    bool backupHosts();
    bool applyBlock();
    bool isBlocked();
    bool reapplyBlock();
    bool checkAdminPrivileges() const;  // Moved to public
    bool secureWrite(const fs::path& path, const std::string& content) const;  // Moved to public

    // Getters
    const fs::path& getHostsPath() const { return m_hostsPath; }
    const fs::path& getBackupPath() const { return m_backupPath; }
    void setDebugMode(bool debug) { m_debugMode = debug; }

private:
    static constexpr const char* BLOCK_START_MARKER = "### ChickenJockey Block Start ###";
    static constexpr const char* BLOCK_END_MARKER = "### ChickenJockey Block End ###";

    std::vector<std::string> m_domains;
    fs::path m_hostsPath;
    fs::path m_backupPath;
    bool m_debugMode;

    void debugLog(const std::string& message) const;
    void debugLog(const std::wstring& message) const;
    std::string trim(const std::string& str) const;
};