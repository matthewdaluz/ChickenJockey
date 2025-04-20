#include "blocker.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <filesystem>   // Requires C++17
#include <algorithm>
#include <cctype>

namespace fs = std::filesystem;

// Constructor: sets the paths for the hosts file and backup.
Blocker::Blocker(const std::string& hostsPath, const std::string& backupPath)
    : m_hostsPath(hostsPath), m_backupPath(backupPath) 
{
}

// Utility: trim whitespace from both ends of a string.
std::string Blocker::trim(const std::string& str) {
    std::string s = str;
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
        return !std::isspace(ch);
    }));
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
    }).base(), s.end());
    return s;
}

// Load domains from a vector of strings.
bool Blocker::loadDomains(const std::vector<std::string>& domains) {
    m_domains = domains;
    if (m_domains.empty()) {
        std::cerr << "[Error] Domain list is empty." << std::endl;
        return false;
    }
    std::cout << "[Info] Loaded " << m_domains.size() << " domain(s) to block." << std::endl;
    return true;
}

// Load domains from a uBlock Origin–style hosts file.
// Expects lines like "0.0.0.0 example.com" or "127.0.0.1 example.com".
bool Blocker::loadDomainsFromFile(const std::string& filePath) {
    std::ifstream inFile(filePath);
    if (!inFile.is_open()) {
        std::cerr << "[Error] Unable to open file: " << filePath << std::endl;
        return false;
    }
    
    m_domains.clear();
    std::string line;
    while (std::getline(inFile, line)) {
        line = trim(line);
        // Skip empty lines and comments.
        if (line.empty() || line[0] == '#')
            continue;
        
        std::istringstream iss(line);
        std::string ip, domain;
        if (!(iss >> ip >> domain)) {
            continue;  // Skip if parsing fails.
        }
        // Accept only entries with 0.0.0.0 or 127.0.0.1.
        if (ip == "0.0.0.0" || ip == "127.0.0.1") {
            m_domains.push_back(domain);
        }
    }
    inFile.close();
    
    if (m_domains.empty()) {
        std::cerr << "[Error] No domains loaded from file: " << filePath << std::endl;
        return false;
    }
    
    std::cout << "[Info] Loaded " << m_domains.size() << " domain(s) from file: " << filePath << std::endl;
    return true;
}

// Backup the current hosts file using std::filesystem.
bool Blocker::backupHosts() {
    try {
        // Ensure the backup directory exists.
        fs::path backupDir = fs::path(m_backupPath).parent_path();
        if (!fs::exists(backupDir)) {
            fs::create_directories(backupDir);
        }
        // Copy the hosts file to the backup location (overwrite if already exists).
        fs::copy_file(m_hostsPath, m_backupPath, fs::copy_options::overwrite_existing);
        std::cout << "[Info] Hosts file backed up to: " << m_backupPath << std::endl;
    } catch (const fs::filesystem_error& e) {
        std::cerr << "[Error] Backup failed: " << e.what() << std::endl;
        return false;
    }
    return true;
}

// Overwrite the hosts file with block entries for each domain.
bool Blocker::applyBlock() {
    if (m_domains.empty()) {
        std::cerr << "[Error] No domains loaded to block." << std::endl;
        return false;
    }

    if (!backupHosts()) {
        std::cerr << "[Error] Unable to backup hosts file. Aborting block application." << std::endl;
        return false;
    }

    // Step 1: Read existing hosts file and preserve all lines outside our markers.
    std::ifstream inFile(m_hostsPath);
    if (!inFile.is_open()) {
        std::cerr << "[Error] Failed to open hosts file for reading: " << m_hostsPath << std::endl;
        return false;
    }

    std::ostringstream preservedLines;
    std::string line;
    bool insideBlock = false;

    while (std::getline(inFile, line)) {
        if (line.find(BLOCK_START_MARKER) != std::string::npos) {
            insideBlock = true;
            continue;
        }
        if (line.find(BLOCK_END_MARKER) != std::string::npos) {
            insideBlock = false;
            continue;
        }
        if (!insideBlock) {
            preservedLines << line << "\n";
        }
    }

    inFile.close();

    // Step 2: Open hosts file for writing and inject new blocklist.
    std::ofstream outFile(m_hostsPath, std::ios::out | std::ios::trunc);
    if (!outFile.is_open()) {
        std::cerr << "[Error] Failed to open hosts file for writing: " << m_hostsPath << std::endl;
        return false;
    }

    outFile << preservedLines.str();
    outFile << "# This hosts file has been modified by ChickenJockey.\n";
    outFile << BLOCK_START_MARKER << "\n";

    for (const auto& domain : m_domains) {
        outFile << "127.0.0.1 " << domain << "\n";
    }

    outFile << BLOCK_END_MARKER << "\n";
    outFile.close();

    std::cout << "[Info] Hosts file updated with preserved entries and new block entries." << std::endl;
    return true;
}


// Check if the hosts file is in a blocked state by looking for our markers.
bool Blocker::isBlocked() {
    std::ifstream inFile(m_hostsPath);
    if (!inFile.is_open()) {
        std::cerr << "[Error] Unable to open hosts file for reading: " << m_hostsPath << std::endl;
        return false;
    }
    
    std::string line;
    bool foundStart = false;
    bool foundEnd = false;
    while (std::getline(inFile, line)) {
        if (line.find(BLOCK_START_MARKER) != std::string::npos) {
            foundStart = true;
        }
        if (line.find(BLOCK_END_MARKER) != std::string::npos) {
            foundEnd = true;
        }
        if (foundStart && foundEnd)
            break;
    }
    inFile.close();
    
    if (foundStart && foundEnd)
        std::cout << "[Info] Hosts file is currently in a blocked state." << std::endl;
    else
        std::cout << "[Info] Hosts file is NOT in a blocked state." << std::endl;
    
    return foundStart && foundEnd;
}

// Reapply the block if the existing block markers are missing.
bool Blocker::reapplyBlock() {
    if (!isBlocked()) {
        std::cout << "[Warning] Tampering detected. Reapplying block." << std::endl;
        return applyBlock();
    }
    std::cout << "[Info] No tampering detected. Block remains intact." << std::endl;
    return true;
}
