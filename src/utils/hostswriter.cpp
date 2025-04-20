#include <windows.h>
#include <fstream>
#include <string>

int wmain(int argc, wchar_t* argv[]) {
    std::wofstream log(L"C:\\hostswriter.log", std::ios::app);
    log << L"=== hostswriter.exe START ===\n";

    if (argc != 3) {
        log << L"[hostswriter] Invalid arguments.\n";
        return 1;
    }

    const std::wstring source = argv[1];
    const std::wstring target = argv[2];

    log << L"[hostswriter] Copying from: " << source << L"\n";
    log << L"[hostswriter] Copying to:   " << target << L"\n";

    // Disable file system redirection if 32-bit
    void* oldValue = nullptr;
    BOOL redirDisabled = Wow64DisableWow64FsRedirection(&oldValue);

    BOOL result = CopyFileW(source.c_str(), target.c_str(), FALSE);

    if (redirDisabled)
        Wow64RevertWow64FsRedirection(oldValue);

    if (!result) {
        DWORD err = GetLastError();
        log << L"[hostswriter] Copy failed. WinError: " << err << L"\n";
        return 2;
    }

    log << L"[hostswriter] Copy successful.\n";
    return 0;
}
