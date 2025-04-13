#include "service.h"
#include <windows.h>
#include <iostream>
#include <string>

// Global service status variables.
SERVICE_STATUS          g_ServiceStatus = {};
SERVICE_STATUS_HANDLE   g_StatusHandle = nullptr;
HANDLE                  g_ServiceStopEvent = NULL;

// Global PROCESS_INFORMATION for the two watchdog processes.
PROCESS_INFORMATION     g_ProcessAInfo = {};
PROCESS_INFORMATION     g_ProcessBInfo = {};

// Forward declarations for service functions.
void WINAPI ServiceMain(DWORD argc, LPWSTR *argv);
void WINAPI ServiceCtrlHandler(DWORD CtrlCode);
bool LaunchWatchdogProcess(const std::wstring &commandLine, PROCESS_INFORMATION &pi);
void MonitorWatchdogProcesses();
void CleanupWatchdogProcesses();

// Command lines for the two watchdog processes.
// These could be adjusted so that the same executable is launched with different parameters.
const std::wstring WATCHDOG_A_CMD = L"ChickenJockey.exe --watchdog A";
const std::wstring WATCHDOG_B_CMD = L"ChickenJockey.exe --watchdog B";

// ---------------------------------------------------------------------
// StartServiceHandler
// This function is called from main.cpp to start the Windows service.
// It registers the service entry point with the Service Control Manager.
bool StartServiceHandler() {
    SERVICE_TABLE_ENTRY ServiceTable[] =
    {
        { const_cast<LPWSTR>(L"ChickenJockeyService"), ServiceMain },
        { NULL, NULL }
    };

    if (!StartServiceCtrlDispatcher(ServiceTable)) {
        std::wcerr << L"[Error] StartServiceCtrlDispatcher failed with error: " 
                   << GetLastError() << std::endl;
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------
// ServiceMain
// The main function for the Windows Service. It registers the control
// handler, sets up initial status, launches watchdog processes, and
// enters the main loop that monitors child processes until a stop signal is received.
void WINAPI ServiceMain(DWORD argc, LPWSTR *argv)
{
    // Register the service control handler.
    g_StatusHandle = RegisterServiceCtrlHandler(L"ChickenJockeyService", ServiceCtrlHandler);
    if (g_StatusHandle == NULL) {
        std::wcerr << L"[Error] RegisterServiceCtrlHandler failed with error: " 
                   << GetLastError() << std::endl;
        return;
    }

    // Initialize service status structure.
    g_ServiceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    g_ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
    g_ServiceStatus.dwCurrentState = SERVICE_START_PENDING;
    g_ServiceStatus.dwWin32ExitCode = 0;
    g_ServiceStatus.dwServiceSpecificExitCode = 0;
    g_ServiceStatus.dwCheckPoint = 0;

    // Report initial status.
    if (!SetServiceStatus(g_StatusHandle, &g_ServiceStatus)) {
        std::wcerr << L"[Error] SetServiceStatus failed with error: " 
                   << GetLastError() << std::endl;
    }

    // Create an event to signal when the service should stop.
    g_ServiceStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (g_ServiceStopEvent == NULL) {
        g_ServiceStatus.dwControlsAccepted = 0;
        g_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
        g_ServiceStatus.dwWin32ExitCode = GetLastError();
        g_ServiceStatus.dwCheckPoint = 1;
        SetServiceStatus(g_StatusHandle, &g_ServiceStatus);
        return;
    }

    // Service is now running.
    g_ServiceStatus.dwCurrentState = SERVICE_RUNNING;
    if (!SetServiceStatus(g_StatusHandle, &g_ServiceStatus)) {
        std::wcerr << L"[Error] SetServiceStatus failed with error: " 
                   << GetLastError() << std::endl;
    }
    std::wcout << L"[Info] Chicken Jockey Service started." << std::endl;

    // Launch watchdog processes.
    if (!LaunchWatchdogProcess(WATCHDOG_A_CMD, g_ProcessAInfo)) {
        std::wcerr << L"[Error] Failed to launch Watchdog A." << std::endl;
    } else {
        std::wcout << L"[Info] Watchdog A launched." << std::endl;
    }

    if (!LaunchWatchdogProcess(WATCHDOG_B_CMD, g_ProcessBInfo)) {
        std::wcerr << L"[Error] Failed to launch Watchdog B." << std::endl;
    } else {
        std::wcout << L"[Info] Watchdog B launched." << std::endl;
    }

    // Main service loop: monitor watchdog processes every 5 seconds.
    while (WaitForSingleObject(g_ServiceStopEvent, 5000) == WAIT_TIMEOUT) {
        MonitorWatchdogProcesses();
    }

    // Cleanup: terminate watchdog processes and close handles.
    CleanupWatchdogProcesses();
    CloseHandle(g_ServiceStopEvent);

    // Report service stopped.
    g_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
    if (!SetServiceStatus(g_StatusHandle, &g_ServiceStatus)) {
        std::wcerr << L"[Error] SetServiceStatus during shutdown failed with error: " 
                   << GetLastError() << std::endl;
    }
    std::wcout << L"[Info] Chicken Jockey Service stopped." << std::endl;
}

// ---------------------------------------------------------------------
// ServiceCtrlHandler
// Handles service control events (like stop or shutdown).
void WINAPI ServiceCtrlHandler(DWORD CtrlCode)
{
    switch (CtrlCode)
    {
        case SERVICE_CONTROL_STOP:
        case SERVICE_CONTROL_SHUTDOWN:
            std::wcout << L"[Info] Received stop/shutdown control request." << std::endl;
            g_ServiceStatus.dwCurrentState = SERVICE_STOP_PENDING;
            SetServiceStatus(g_StatusHandle, &g_ServiceStatus);
            // Signal the service to stop.
            SetEvent(g_ServiceStopEvent);
            break;
        default:
            break;
    }
}

// ---------------------------------------------------------------------
// LaunchWatchdogProcess
// Launches a child process using CreateProcess with the specified command line.
bool LaunchWatchdogProcess(const std::wstring &commandLine, PROCESS_INFORMATION &pi)
{
    STARTUPINFO si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    // CreateProcess requires a modifiable command line buffer.
    wchar_t* cmdLineBuffer = new wchar_t[commandLine.size() + 1];
    wcscpy_s(cmdLineBuffer, commandLine.size() + 1, commandLine.c_str());

    BOOL result = CreateProcess(
        NULL,              // No module name; use command line.
        cmdLineBuffer,     // Command line.
        NULL,              // Process handle not inheritable.
        NULL,              // Thread handle not inheritable.
        FALSE,             // Do not inherit handles.
        0,                 // No creation flags.
        NULL,              // Use parent's environment block.
        NULL,              // Use parent's starting directory.
        &si,               // Pointer to STARTUPINFO.
        &pi                // Pointer to PROCESS_INFORMATION.
    );

    delete[] cmdLineBuffer;

    if (!result) {
        std::wcerr << L"[Error] CreateProcess failed for command: " 
                   << commandLine << L", error: " << GetLastError() << std::endl;
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------
// MonitorWatchdogProcesses
// Checks if either watchdog process has exited and restarts it if needed.
void MonitorWatchdogProcesses()
{
    DWORD exitCode = 0;
    // Monitor Process A.
    if (g_ProcessAInfo.hProcess != NULL) {
        if (WaitForSingleObject(g_ProcessAInfo.hProcess, 0) == WAIT_OBJECT_0) {
            GetExitCodeProcess(g_ProcessAInfo.hProcess, &exitCode);
            std::wcout << L"[Warning] Watchdog A terminated with exit code: " 
                       << exitCode << L". Restarting..." << std::endl;
            CloseHandle(g_ProcessAInfo.hProcess);
            CloseHandle(g_ProcessAInfo.hThread);
            LaunchWatchdogProcess(WATCHDOG_A_CMD, g_ProcessAInfo);
        }
    }
    // Monitor Process B.
    if (g_ProcessBInfo.hProcess != NULL) {
        if (WaitForSingleObject(g_ProcessBInfo.hProcess, 0) == WAIT_OBJECT_0) {
            GetExitCodeProcess(g_ProcessBInfo.hProcess, &exitCode);
            std::wcout << L"[Warning] Watchdog B terminated with exit code: " 
                       << exitCode << L". Restarting..." << std::endl;
            CloseHandle(g_ProcessBInfo.hProcess);
            CloseHandle(g_ProcessBInfo.hThread);
            LaunchWatchdogProcess(WATCHDOG_B_CMD, g_ProcessBInfo);
        }
    }
}

// ---------------------------------------------------------------------
// CleanupWatchdogProcesses
// Terminates any watchdog processes still running and cleans up handles.
void CleanupWatchdogProcesses()
{
    if (g_ProcessAInfo.hProcess != NULL) {
        TerminateProcess(g_ProcessAInfo.hProcess, 0);
        CloseHandle(g_ProcessAInfo.hProcess);
        CloseHandle(g_ProcessAInfo.hThread);
        g_ProcessAInfo.hProcess = NULL;
    }
    if (g_ProcessBInfo.hProcess != NULL) {
        TerminateProcess(g_ProcessBInfo.hProcess, 0);
        CloseHandle(g_ProcessBInfo.hProcess);
        CloseHandle(g_ProcessBInfo.hThread);
        g_ProcessBInfo.hProcess = NULL;
    }
}
