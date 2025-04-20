#include "gui.h"
#include "blocker.h"  // Added for integration
#include <windows.h>
#include <commdlg.h>
#include <string>
#include <vector>
#include <sstream>
#include <iostream>

// Global instance handle and control IDs.
HINSTANCE g_hInst = GetModuleHandle(NULL);
HWND g_hEdit = nullptr;

#define ID_EDIT_INPUT   101
#define ID_BUTTON_BROWSE 102
#define ID_BUTTON_APPLY  103

// Forward declaration for the window procedure.
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Helper: Open a file dialog to import a uBlock Origin–style hosts file,
// read its content, and load it into the edit control.
bool LoadFileToEditControl(HWND hEdit) {
    OPENFILENAME ofn = { 0 };
    wchar_t szFileName[MAX_PATH] = L"";

    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.hwndOwner = hEdit;
    ofn.lpstrFilter = L"Hosts Files (*.txt;*.hosts)\0*.txt;*.hosts\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = szFileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST;
    ofn.lpstrDefExt = L"txt";

    if (GetOpenFileName(&ofn)) {
        HANDLE hFile = CreateFile(szFileName, GENERIC_READ, FILE_SHARE_READ, NULL,
                                  OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile == INVALID_HANDLE_VALUE) {
            MessageBox(NULL, L"Failed to open file.", L"Error", MB_ICONERROR);
            return false;
        }
        DWORD fileSize = GetFileSize(hFile, NULL);
        if (fileSize == INVALID_FILE_SIZE || fileSize == 0) {
            CloseHandle(hFile);
            MessageBox(NULL, L"File size invalid or empty.", L"Error", MB_ICONERROR);
            return false;
        }
        std::vector<wchar_t> buffer((fileSize / sizeof(wchar_t)) + 1, 0);
        DWORD bytesRead = 0;
        if (!ReadFile(hFile, buffer.data(), fileSize, &bytesRead, NULL)) {
            CloseHandle(hFile);
            MessageBox(NULL, L"Failed to read file.", L"Error", MB_ICONERROR);
            return false;
        }
        CloseHandle(hFile);
        std::wstring fileContent(buffer.data());
        SetWindowText(hEdit, fileContent.c_str());
        return true;
    }
    return false;
}

int RunGUI() {
    const wchar_t CLASS_NAME[] = L"ChickenJockeySetupClass";

    WNDCLASS wc = { };
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = g_hInst;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);

    if (!RegisterClass(&wc)) {
        MessageBox(NULL, L"Failed to register window class.", L"Error", MB_ICONERROR);
        return 1;
    }

    HWND hwnd = CreateWindowEx(
        0,
        CLASS_NAME,
        L"Chicken Jockey Setup",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT, 500, 400,
        NULL,
        NULL,
        g_hInst,
        NULL
    );

    if (!hwnd) {
        MessageBox(NULL, L"Failed to create window.", L"Error", MB_ICONERROR);
        return 1;
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
    static HWND hButtonBrowse, hButtonApply;

    switch (msg) {
        case WM_CREATE:
        {
            CreateWindowW(L"STATIC", L"Enter websites to block (one per line):",
                          WS_CHILD | WS_VISIBLE,
                          20, 20, 450, 20, hwnd, NULL, g_hInst, NULL);

            g_hEdit = CreateWindowW(L"EDIT", L"",
                          WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN,
                          20, 50, 450, 200, hwnd, (HMENU)ID_EDIT_INPUT, g_hInst, NULL);

            hButtonBrowse = CreateWindowW(L"BUTTON", L"Import File",
                          WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                          20, 270, 120, 30, hwnd, (HMENU)ID_BUTTON_BROWSE, g_hInst, NULL);

            hButtonApply = CreateWindowW(L"BUTTON", L"Apply Block",
                          WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                          160, 270, 120, 30, hwnd, (HMENU)ID_BUTTON_APPLY, g_hInst, NULL);
            break;
        }
        case WM_COMMAND:
        {
            switch (LOWORD(wParam)) {
                case ID_BUTTON_BROWSE:
                    LoadFileToEditControl(g_hEdit);
                    break;
                case ID_BUTTON_APPLY:
                {
                    int response = MessageBox(hwnd,
                        L"Warning: Once applied, the block is irreversible and cannot be disabled through this interface.\nDo you wish to continue?",
                        L"Confirm Activation", MB_YESNO | MB_ICONWARNING);
                    if (response == IDYES) {
                        int len = GetWindowTextLength(g_hEdit);
                        std::wstring blockList(len + 1, L'\0');
                        GetWindowText(g_hEdit, &blockList[0], len + 1);
                        blockList.resize(wcslen(blockList.c_str()));

                        // Safe wchar_t to char (ASCII only) conversion to avoid compiler warning
std::string utf8BlockList;
utf8BlockList.reserve(blockList.size());
for (wchar_t wc : blockList) {
    if (wc <= 0x7F) utf8BlockList += static_cast<char>(wc);
}

std::istringstream stream(utf8BlockList);
std::string line;
std::vector<std::string> domains;
while (std::getline(stream, line)) {
    if (!line.empty()) domains.push_back(line);
}


                        Blocker blocker;
                        if (!blocker.loadDomains(domains) || !blocker.applyBlock()) {
                            MessageBox(hwnd, L"Failed to apply blocklist.", L"Error", MB_ICONERROR);
                        } else {
                            MessageBox(hwnd, L"Blocklist has been applied and is now active.", L"Block Applied", MB_OK | MB_ICONINFORMATION);

                            EnableWindow(g_hEdit, FALSE);
                            EnableWindow(GetDlgItem(hwnd, ID_BUTTON_BROWSE), FALSE);
                            EnableWindow(GetDlgItem(hwnd, ID_BUTTON_APPLY), FALSE);
                            PostMessage(hwnd, WM_CLOSE, 0, 0);
                        }
                    }
                    break;
                }
            }
            break;
        }
        case WM_DESTROY:
            PostQuitMessage(0);
            break;
        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }

    return 0;
}
