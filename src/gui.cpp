// gui.cpp

#include "gui.h"
#include "blocker.h"
#include <windows.h>
#include <commdlg.h>
#include <dwmapi.h>
#include <uxtheme.h>
#include <versionhelpers.h>
#include <string>
#include <vector>
#include <sstream>
#include <iostream>
#include <regex>
#include <shlwapi.h>
#include <shlobj.h>
#include <shellapi.h>
#include <fstream>
#pragma comment(lib, "shlwapi.lib")


#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "uxtheme.lib")

// Modern UI constants
#define WINDOW_WIDTH 680
#define WINDOW_HEIGHT 520
#define COLOR_BACKGROUND RGB(0xF8, 0xF9, 0xFA)
#define COLOR_BUTTON RGB(0xE9, 0xEC, 0xEF)
#define COLOR_TEXT RGB(0x21, 0x25, 0x29)

enum CustomControls {
    ID_EDIT_INPUT = 1001,
    ID_BUTTON_BROWSE,
    ID_BUTTON_APPLY,
    ID_STATUS_BAR
};

HINSTANCE g_hInst = GetModuleHandle(NULL);
HWND g_hEdit = nullptr;
HWND g_hStatusBar = nullptr;
HWND hButtonBrowse = nullptr;
HWND hButtonApply = nullptr;
bool g_darkMode = false;

// Forward declarations
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
void InitializeControls(HWND hwnd);
void UpdateLayout(HWND hwnd);
bool LoadFileToEditControl(HWND hEdit);
HFONT CreateCustomFont(int size, bool bold = false);
void ApplyDarkTheme(HWND hwnd, bool enable);
void ShowErrorMessage(HWND hwnd, const wchar_t* message);

bool AddStartupEntry(const std::wstring& name, const std::wstring& command) {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                      L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run",
                      0, KEY_SET_VALUE, &hKey) != ERROR_SUCCESS) {
        return false;
    }

    LONG result = RegSetValueExW(
        hKey, name.c_str(), 0, REG_SZ,
        reinterpret_cast<const BYTE*>(command.c_str()),
        static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t))
    );

    RegCloseKey(hKey);
    return result == ERROR_SUCCESS;
}



int RunGUI() {
    Blocker checkBlock;
    if (checkBlock.isBlocked()) {
        MessageBoxW(NULL,
            L"Chicken Jockey is already active.\nUse Task Manager or watchdogs will restart it.",
            L"Access Denied", MB_ICONWARNING | MB_OK);
        return 1;
    }

    const wchar_t CLASS_NAME[] = L"ChickenJockeyClass";

    WNDCLASSW wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = g_hInst;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(COLOR_BACKGROUND);
    wc.style = CS_HREDRAW | CS_VREDRAW;

    if (!RegisterClassW(&wc)) return 1;

    RECT rc = { 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT };
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
    
    HWND hwnd = CreateWindowW(
        CLASS_NAME, L"Chicken Jockey - Website Blocker",
        WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX & ~WS_THICKFRAME,
        CW_USEDEFAULT, CW_USEDEFAULT,
        rc.right - rc.left, rc.bottom - rc.top,
        NULL, NULL, g_hInst, NULL
    );

    if (!hwnd) return 1;

    // Initialize dark mode if supported
    if (IsWindows10OrGreater()) {
        BOOL darkMode = TRUE;
        DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &darkMode, sizeof(darkMode));
        g_darkMode = true;
    }

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return static_cast<int>(msg.wParam);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE:
            InitializeControls(hwnd);
            ApplyDarkTheme(hwnd, g_darkMode);
            break;

        case WM_SIZE:
            UpdateLayout(hwnd);
            break;

        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORSTATIC: {
            HDC hdc = (HDC)wParam;
            SetBkColor(hdc, COLOR_BACKGROUND);
            SetTextColor(hdc, COLOR_TEXT);
            return (LRESULT)CreateSolidBrush(COLOR_BACKGROUND);
        }

        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case ID_BUTTON_BROWSE:
                    if (LoadFileToEditControl(g_hEdit)) {
                        SendMessage(g_hStatusBar, SB_SETTEXT, 0, (LPARAM)L"File loaded successfully");
                    }
                    break;

                case ID_BUTTON_APPLY: {
                    int response = MessageBox(hwnd,
                        L"Once applied, blocks cannot be removed through this UI. Proceed?",
                        L"Confirm Block", MB_YESNO | MB_ICONWARNING);
                    if (response != IDYES) break;

                    // Retrieve and process blocklist
                    int len = GetWindowTextLengthW(g_hEdit);
                    std::wstring buffer(len + 1, L'\0');
                    GetWindowTextW(g_hEdit, &buffer[0], len + 1);

                    // Convert to UTF-8
                    int utf8Size = WideCharToMultiByte(CP_UTF8, 0, buffer.c_str(), -1, nullptr, 0, nullptr, nullptr);
                    std::string utf8BlockList(utf8Size, 0);
                    WideCharToMultiByte(CP_UTF8, 0, buffer.c_str(), -1, &utf8BlockList[0], utf8Size, nullptr, nullptr);

                    // Parse domains
                    std::istringstream stream(utf8BlockList);
                    std::string line;
                    std::vector<std::string> domains;
                    std::regex hostPattern(R"((?:0\.0\.0\.0|127\.0\.0\.1)?\s*([a-zA-Z0-9\.\-_]+))");

                    while (std::getline(stream, line)) {
                        if (line.empty() || line[0] == '#') continue;
                        std::smatch match;
                        if (std::regex_search(line, match, hostPattern)) {
                            domains.push_back(match[1].str());
                        }
                    }

                    Blocker blocker;
if (!blocker.loadDomains(domains) || !blocker.applyBlock()) {
    ShowErrorMessage(hwnd, L"Failed to apply blocklist.");
} else {
    MessageBoxW(hwnd, 
        L"Blocklist applied successfully. You can now run Chicken Jockey in watchdog mode.",
        L"Block Applied", MB_OK | MB_ICONINFORMATION);

    // Disable UI controls
    EnableWindow(g_hEdit, FALSE);
    EnableWindow(hButtonBrowse, FALSE);
    EnableWindow(hButtonApply, FALSE);

    PostMessage(hwnd, WM_CLOSE, 0, 0);
                    }
                    break;
                }
            }
            break;

        case WM_DESTROY:
            PostQuitMessage(0);
            break;

        default:
            return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    return 0;
}

void InitializeControls(HWND hwnd) {
    HFONT hFont = CreateCustomFont(14);
    
    // Header
    CreateWindowW(L"STATIC", L"Website Block List Manager",
        WS_CHILD | WS_VISIBLE | SS_CENTER,
        20, 20, WINDOW_WIDTH - 40, 40, hwnd, NULL, g_hInst, NULL);

    // Edit control
    g_hEdit = CreateWindowW(L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE |
        ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_WANTRETURN | WS_VSCROLL,
        20, 80, WINDOW_WIDTH - 40, WINDOW_HEIGHT - 200,
        hwnd, (HMENU)ID_EDIT_INPUT, g_hInst, NULL);
    SendMessageW(g_hEdit, WM_SETFONT, (WPARAM)hFont, TRUE);

    // Buttons
    hButtonBrowse = CreateWindowW(L"BUTTON", L"Import Hosts File",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_FLAT,
        20, WINDOW_HEIGHT - 100, 200, 40,
        hwnd, (HMENU)ID_BUTTON_BROWSE, g_hInst, NULL);

    hButtonApply = CreateWindowW(L"BUTTON", L"Apply Website Blocks",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_FLAT,
        240, WINDOW_HEIGHT - 100, 200, 40,
        hwnd, (HMENU)ID_BUTTON_APPLY, g_hInst, NULL);

    SendMessageW(hButtonBrowse, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessageW(hButtonApply, WM_SETFONT, (WPARAM)hFont, TRUE);

    // Status bar
    g_hStatusBar = CreateWindowExW(0, STATUSCLASSNAMEW, NULL,
        WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
        0, 0, 0, 0, hwnd, (HMENU)ID_STATUS_BAR, g_hInst, NULL);
}

void UpdateLayout(HWND hwnd) {
    RECT rc;
    GetClientRect(hwnd, &rc);
    
    SetWindowPos(g_hEdit, NULL, 20, 80, 
        rc.right - 40, rc.bottom - 200, SWP_NOZORDER);
    
    int buttonY = rc.bottom - 90;
    SetWindowPos(hButtonBrowse, NULL, 20, buttonY, 200, 40, SWP_NOZORDER);
    SetWindowPos(hButtonApply, NULL, 240, buttonY, 200, 40, SWP_NOZORDER);

    SendMessageW(g_hStatusBar, WM_SIZE, 0, 0);
}

HFONT CreateCustomFont(int size, bool bold) {
    LOGFONTW lf = {};
    SystemParametersInfoW(SPI_GETICONTITLELOGFONT, sizeof(lf), &lf, 0);
    lf.lfHeight = -MulDiv(size, GetDeviceCaps(GetDC(NULL), LOGPIXELSY), 72);
    lf.lfWeight = bold ? FW_BOLD : FW_NORMAL;
    wcscpy_s(lf.lfFaceName, L"Segoe UI");
    return CreateFontIndirectW(&lf);
}

void ApplyDarkTheme(HWND hwnd, bool enable) {
    if (IsWindows10OrGreater()) {
        BOOL darkMode = enable;
        DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &darkMode, sizeof(darkMode));
    }
}

bool LoadFileToEditControl(HWND hEdit) {
    OPENFILENAMEW ofn = { sizeof(OPENFILENAMEW) };
    wchar_t szFileName[MAX_PATH] = L"";

    ofn.hwndOwner = hEdit;
    ofn.lpstrFilter = L"Hosts Files (*.txt;*.hosts)\0*.txt;*.hosts\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = szFileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST;
    ofn.lpstrDefExt = L"txt";

    if (!GetOpenFileNameW(&ofn)) return false;

    HANDLE hFile = CreateFileW(szFileName, GENERIC_READ, FILE_SHARE_READ, NULL,
                               OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return false;

    DWORD fileSize = GetFileSize(hFile, NULL);
    std::string buffer(fileSize + 1, 0);
    DWORD bytesRead;
    ReadFile(hFile, buffer.data(), fileSize, &bytesRead, NULL);
    CloseHandle(hFile);

    int wideSize = MultiByteToWideChar(CP_UTF8, 0, buffer.data(), bytesRead, NULL, 0);
    std::wstring wbuffer(wideSize, 0);
    MultiByteToWideChar(CP_UTF8, 0, buffer.data(), bytesRead, &wbuffer[0], wideSize);

    // 🔧 Normalize to CRLF line endings for multiline display
    std::wstring normalized;
    for (wchar_t ch : wbuffer) {
        if (ch == L'\n') {
            if (normalized.empty() || normalized.back() != L'\r') {
                normalized += L'\r';
            }
        }
        normalized += ch;
    }

    SetWindowTextW(hEdit, normalized.c_str());
    return true;
}


void ShowErrorMessage(HWND hwnd, const wchar_t* message) {
    MessageBoxW(hwnd, message, L"Error", MB_OK | MB_ICONERROR);
}