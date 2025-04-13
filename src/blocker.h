#pragma once

#include <string>
#include <vector>

// Blocker module for system-level website blocking via the Windows hosts file.
class Blocker {
public:
    // Constructor allows custom paths for the hosts file and its backup.
    Blocker(const std::string& hostsPath = "C:\\Windows\\System32\\drivers\\etc\\hosts",
            const std::string& backupPath = "C:\\ProgramData\\ChickenJockey\\hosts_backup.txt");

    // Load a list of domains to block.
    bool loadDomains(const std::vector<std::string>& domains);

    // Load domains from a uBlock Origin-style hosts file.
    bool loadDomainsFromFile(const std::string& filePath);

    // Backup the current hosts file to the configured backup path.
    bool backupHosts();

    // Overwrite the hosts file so that each blocked domain points to 127.0.0.1.
    bool applyBlock();

    // Verify whether the hosts file currently contains the block markers.
    bool isBlocked();

    // Reapply the block if tampering is detected.
    bool reapplyBlock();

private:
    std::vector<std::string> m_domains;
    std::string m_hostsPath;
    std::string m_backupPath;

    // Markers for delineating the ChickenJockey block section.
    const std::string BLOCK_START_MARKER = "### ChickenJockey Block Start ###";
    const std::string BLOCK_END_MARKER = "### ChickenJockey Block End ###";

    // Utility function to trim whitespace from a string.
    std::string trim(const std::string& str);
};
