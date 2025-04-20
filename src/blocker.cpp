// blocker.cpp
#include "blocker.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <system_error>
#include <windows.h>
#include <aclapi.h>

namespace fs = std::filesystem;


// Debug logging helpers
void Blocker::debugLog(const std::string& message) const {
    if (m_debugMode) {
        std::cout << "[DEBUG] " << message << std::endl;
    }
}

void Blocker::debugLog(const std::wstring& message) const {
    if (m_debugMode) {
        std::wcout << L"[DEBUG] " << message << std::endl;
    }
}

// Constructor
Blocker::Blocker(const fs::path& hostsPath, const fs::path& backupPath, bool debugMode)
    : m_hostsPath(hostsPath), m_backupPath(backupPath), m_debugMode(debugMode) {
    debugLog("Blocker constructor called");
    debugLog(L"Hosts path: " + m_hostsPath.wstring());
    debugLog(L"Backup path: " + m_backupPath.wstring());
}

// Trim whitespace
std::string Blocker::trim(const std::string& str) const {
    debugLog("Trimming string: " + str);
    auto start = std::find_if_not(str.begin(), str.end(), [](unsigned char c) {
        return std::isspace(c);
    });
    auto end = std::find_if_not(str.rbegin(), str.rend(), [](unsigned char c) {
        return std::isspace(c);
    }).base();
    return (start < end) ? std::string(start, end) : "";
}

// Check admin privileges (Windows-specific)
bool Blocker::checkAdminPrivileges() const {
    debugLog("Checking admin privileges");
    BOOL isAdmin = FALSE;
    PSID adminGroup = NULL;
    SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;

    if (!AllocateAndInitializeSid(&NtAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID, 
                                 DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &adminGroup)) {
        debugLog("Failed to allocate and initialize SID");
        return false;
    }

    if (!CheckTokenMembership(NULL, adminGroup, &isAdmin)) {
        isAdmin = FALSE;
        debugLog("Failed to check token membership");
    }

    FreeSid(adminGroup);
    debugLog(isAdmin ? "User has admin privileges" : "User does not have admin privileges");
    return isAdmin == TRUE;
}

// New writing stuff.
bool Blocker::secureWrite(const fs::path& path, const std::string& content) const {
    debugLog("Starting secureWrite operation");
    fs::path tempPath = path;
    tempPath += ".tmp";
    debugLog(L"Temporary file path: " + tempPath.wstring());

    try {
        {
            debugLog("Creating temporary file");
            std::ofstream ofs(tempPath, std::ios::binary);
            if (!ofs) {
                debugLog("Failed to open temporary file");
                return false;
            }
            ofs.exceptions(std::ofstream::failbit | std::ofstream::badbit);
            ofs << content;
            debugLog("Content written to temporary file");
        }

        // Construct path to hostswriter.exe
        wchar_t exePath[MAX_PATH];
        GetModuleFileNameW(NULL, exePath, MAX_PATH);
        std::wstring exeDir = fs::path(exePath).parent_path();
        std::wstring writerPath = exeDir + L"\\hostswriter.exe";
        debugLog(L"hostswriter.exe path: " + writerPath);

        // Arguments: "<tempPath>" "<targetPath>"
        std::wstring args = L"\"" + tempPath.wstring() + L"\" \"" + path.wstring() + L"\"";
        debugLog(L"Process arguments: " + args);

        // Launch elevated process
        SHELLEXECUTEINFOW sei = { sizeof(sei) };
        sei.lpVerb = L"runas";
        sei.lpFile = writerPath.c_str();
        sei.lpParameters = args.c_str();
        sei.nShow = SW_HIDE;
        sei.fMask = SEE_MASK_NOCLOSEPROCESS;
        
        debugLog("Attempting to launch hostswriter.exe with elevation");
        if (!ShellExecuteExW(&sei) || !sei.hProcess) {
            debugLog("Failed to launch hostswriter.exe with elevation");
            std::wcerr << L"[Blocker] Failed to launch hostswriter.exe with elevation.\n";
            fs::remove(tempPath);
            return false;
        }
        
        debugLog("Waiting for hostswriter.exe to complete");
        WaitForSingleObject(sei.hProcess, INFINITE);

        DWORD exitCode = 1;
        GetExitCodeProcess(sei.hProcess, &exitCode);
        CloseHandle(sei.hProcess);
        debugLog(L"hostswriter.exe exit code: " + std::to_wstring(exitCode));

        // Always delete the temp file, regardless of success/failure
        std::error_code ec;
        fs::remove(tempPath, ec);
        if (ec) {
            debugLog("Failed to remove temporary file: " + std::string(ec.message()));
        }

        if (exitCode != 0) {
            debugLog("hostswriter.exe returned error");
            std::wcerr << L"[Blocker] hostswriter.exe returned error: " << exitCode << std::endl;
            return false;
        }

        if (!fs::exists(path)) {
            debugLog("Hosts file was not created after hostswriter execution");
            std::wcerr << L"[Blocker] Hosts file was not created after hostswriter execution.\n";
            return false;
        }

        debugLog("hostswriter.exe succeeded");
        std::wcout << L"[Blocker] hostswriter.exe succeeded.\n";
        return true;
    } catch (const std::exception& e) {
        debugLog("Exception in secureWrite: " + std::string(e.what()));
        std::cerr << "[Blocker] Secure write error: " << e.what() << std::endl;
        std::error_code ec;
        fs::remove(tempPath, ec);
        return false;
    }
}

// Load domains - single combined implementation
bool Blocker::loadDomains(const std::vector<std::string>& domains) {
    debugLog("Loading domains from vector");
    if (domains.empty()) {
        debugLog("Domain list is empty");
        std::cerr << "[Error] Domain list is empty." << std::endl;
        return false;
    }

    m_domains = domains;
    debugLog("Loaded " + std::to_string(m_domains.size()) + " domain(s)");
    std::cout << "[Info] Loaded " << m_domains.size() << " domain(s).\n";
    return true;
}


// Backup hosts file
bool Blocker::backupHosts() {
    if (!checkAdminPrivileges()) {
        std::cerr << "[Error] Admin rights required for backup." << std::endl;
        return false;
    }

    try {
        fs::create_directories(m_backupPath.parent_path());
        fs::copy_file(m_hostsPath, m_backupPath, fs::copy_options::overwrite_existing);
        fs::permissions(m_backupPath,
            fs::perms::owner_read | fs::perms::owner_write,
            fs::perm_options::replace);
        
        std::cout << "[Info] Backup created: " << m_backupPath << std::endl;
        return true;
    } catch (const fs::filesystem_error& e) {
        std::cerr << "[Error] Backup failed: " << e.what() << std::endl;
        return false;
    }
}

// Apply block
bool Blocker::applyBlock() {
    if (!checkAdminPrivileges()) {
        std::cerr << "[Error] Admin rights required to modify hosts file." << std::endl;
        return false;
    }

    if (m_domains.empty()) {
        std::cerr << "[Error] No domains to block." << std::endl;
        return false;
    }

    // Automatically create backup if it doesn't exist
    try {
        if (!fs::exists(m_backupPath)) {
            fs::create_directories(m_backupPath.parent_path());
            fs::copy_file(m_hostsPath, m_backupPath, fs::copy_options::overwrite_existing);
            fs::permissions(m_backupPath,
                fs::perms::owner_read | fs::perms::owner_write,
                fs::perm_options::replace);
            std::cout << "[Info] Auto-backup created: " << m_backupPath << std::endl;
        }
    } catch (const fs::filesystem_error& e) {
        std::cerr << "[Warning] Failed to auto-create backup: " << e.what() << std::endl;
        // Continue anyway — not critical unless factory reset happens
    }

    // Read existing content
    std::ifstream inFile(m_hostsPath);
    if (!inFile) {
        std::cerr << "[Error] Can't read hosts file." << std::endl;
        return false;
    }

    std::ostringstream content;
    bool insideBlock = false;
    std::string line;

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
            content << line << '\n';
        }
    }

    // Build new content
    std::ostringstream newContent;
    newContent << content.str()
               << "# Managed by ChickenJockey\n"
               << BLOCK_START_MARKER << '\n';
    
    for (const auto& domain : m_domains) {
        newContent << "127.0.0.1 " << domain << '\n';
    }
    
    newContent << BLOCK_END_MARKER << '\n';

    // Atomic write
    if (!secureWrite(m_hostsPath, newContent.str())) {
        std::cerr << "[Error] Failed to update hosts file." << std::endl;
        return false;
    }

    std::cout << "[Info] Hosts file updated successfully.\n";
    return true;
}


// Check block status
bool Blocker::isBlocked() {
    std::ifstream inFile(m_hostsPath);
    if (!inFile) return false;

    bool foundStart = false, foundEnd = false;
    std::string line;

    while (std::getline(inFile, line)) {
        if (line.find(BLOCK_START_MARKER) != std::string::npos) foundStart = true;
        if (line.find(BLOCK_END_MARKER) != std::string::npos) foundEnd = true;
        if (foundStart && foundEnd) break;
    }

    return foundStart && foundEnd;
}

// Reapply block
bool Blocker::reapplyBlock() {
    if (!isBlocked()) {
        std::cout << "[Warning] Block compromised - reapplying.\n";
        return applyBlock();
    }
    std::cout << "[Info] Block integrity verified.\n";
    return true;
}