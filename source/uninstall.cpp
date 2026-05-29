// Copyright (c) DVP-F/Carnx00 2026 <carnx@duck.com>
// License : GNU GPL 3.0 (https://www.gnu.org/licenses/gpl-3.0.html) supplied with the package under `LICENSES`
// Source code hosts:
// - GitHub: https://github.com/DVP-F/DDCL 
// See `NOTICE.txt` for further Licensing information

#include <filesystem>
#include <fstream>
#include <string>
#include <iostream>
#include <windows.h>

#define REGL L"SOFTWARE\\DDCL"

namespace fs = std::filesystem;

std::wstring GetInstallLocation() {
    const wchar_t* valueName = L"InstallPath";
    HKEY hKey{};
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, REGL, 0, KEY_READ, &hKey) != ERROR_SUCCESS) { return L""; }
    DWORD size = 0;
    RegQueryValueExW(hKey, valueName, nullptr, nullptr, nullptr, &size);
    std::wstring value(size / sizeof(wchar_t), L'\0');
    RegQueryValueExW(hKey, valueName, nullptr, nullptr, reinterpret_cast<LPBYTE>(value.data()), &size);
    RegCloseKey(hKey);
    value.resize(wcslen(value.c_str()));
    return value;
}

const std::wstring location = GetInstallLocation();

static int RMRegistryKey() {
    HKEY hKey{};
    // accessible? (also checks admin, bc of the kaypath)
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, REGL, 0, KEY_READ, &hKey) != ERROR_SUCCESS) { return 1; }
    RegCloseKey(hKey);
    if (RegDeleteKeyExW(HKEY_LOCAL_MACHINE, REGL, KEY_WOW64_64KEY, 0) != ERROR_SUCCESS) { return 1; }
    return 0; // success
}

static int RMFiles() {
    try {
        fs::remove_all(location);
        fs::remove_all(L"C:\\ProgramData\\Microsoft\\Windows\\Start Menu\\Programs\\DDCL");
        return 0;
    }
    catch (const fs::filesystem_error& ex) {
        return 1;
    }
}

static int RMPathEntry() {
    wchar_t REG_PATH[] = L"SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Environment";
    HKEY hKey{};
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, REG_PATH, 0, KEY_READ | KEY_WRITE, &hKey) != ERROR_SUCCESS) { return 1; }
    DWORD size = 0;
    RegQueryValueExW(hKey, L"Path", nullptr, nullptr, nullptr, &size);
    std::wstring path(size / sizeof(wchar_t), L'\0');
    RegQueryValueExW(hKey, L"Path", nullptr, nullptr, reinterpret_cast<LPBYTE>(path.data()), &size);
    path.resize(wcslen(path.c_str()));
    std::wstring token = location + L";";
    size_t pos = path.find(token);
    if (pos != std::wstring::npos) {
        path.erase(pos, token.size());
    } else {
        pos = path.find(location);
        if (pos != std::wstring::npos) {
            path.erase(pos, location.size());
        }
    }
    RegSetValueExW(hKey, L"Path", 0, REG_EXPAND_SZ, reinterpret_cast<const BYTE*>(path.c_str()), 
        (path.size() + 1) * sizeof(wchar_t));
    RegCloseKey(hKey);
    SendMessageTimeoutW(HWND_BROADCAST, WM_SETTINGCHANGE, 0, (LPARAM)REG_PATH, SMTO_ABORTIFHUNG, 1000, nullptr);
    return 0;
}

int main() {
    try {
        RMRegistryKey();
        RMFiles();
        RMPathEntry();
    }
    catch (const std::exception &ex) {
        return 1;
    }
    return 0;
}
