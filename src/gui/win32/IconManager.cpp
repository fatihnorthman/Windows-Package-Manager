#include "IconManager.h"
#include <cwctype>
#include <shellapi.h>
#include <shlobj.h>
#include <vector>
#include <algorithm>
#include <filesystem>

namespace pm {

namespace {

std::wstring utf8ToWide(const std::string& s) {
    if (s.empty()) return {};
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    if (len <= 0) return {};
    std::wstring out(static_cast<size_t>(len - 1), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, out.data(), len);
    return out;
}

std::wstring cleanIconPath(const std::wstring& rawPath, int& outIndex) {
    outIndex = 0;
    if (rawPath.empty()) return {};

    std::wstring path = rawPath;
    
    // Remove surrounding quotes if they exist
    if (path.front() == L'"') {
        size_t nextQuote = path.find(L'"', 1);
        if (nextQuote != std::wstring::npos) {
            std::wstring indexPart = path.substr(nextQuote + 1);
            path = path.substr(1, nextQuote - 1);
            
            // Check for index suffix like ,1 or ,-1
            size_t comma = indexPart.find(L',');
            if (comma != std::wstring::npos) {
                try {
                    outIndex = std::stoi(indexPart.substr(comma + 1));
                } catch (...) {
                    outIndex = 0;
                }
            }
            return path;
        }
    }

    // Try to find a comma indicating index suffix, e.g. C:\path\app.exe,1 or C:\path\app.ico,0
    size_t lastComma = path.find_last_of(L',');
    if (lastComma != std::wstring::npos && lastComma > path.find_last_of(L'\\')) {
        std::wstring indexStr = path.substr(lastComma + 1);
        // Trim spaces
        indexStr.erase(0, indexStr.find_first_not_of(L" \t"));
        indexStr.erase(indexStr.find_last_not_of(L" \t") + 1);
        
        bool isNumber = !indexStr.empty();
        for (size_t i = 0; i < indexStr.size(); ++i) {
            if (i == 0 && indexStr[i] == L'-') continue;
            if (!std::iswdigit(indexStr[i])) {
                isNumber = false;
                break;
            }
        }
        if (isNumber) {
            try {
                outIndex = std::stoi(indexStr);
                path = path.substr(0, lastComma);
            } catch (...) {}
        }
    }

    // Trim trailing and leading quotes/spaces
    while (!path.empty() && (path.front() == L' ' || path.front() == L'"')) path.erase(0, 1);
    while (!path.empty() && (path.back() == L' ' || path.back() == L'"')) path.pop_back();

    return path;
}

} // anonymous namespace

IconManager& IconManager::instance() {
    static IconManager inst;
    return inst;
}

IconManager::IconManager() = default;
IconManager::~IconManager() = default;

std::wstring IconManager::normalizeName(const std::wstring& name) {
    std::wstring out;
    out.reserve(name.size());
    for (wchar_t c : name) {
        if (std::iswalnum(c)) {
            out += std::towlower(c);
        }
    }
    return out;
}

std::wstring IconManager::findIconPath(const std::string& packageId, const std::string& packageName) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!scanned_) {
        scanRegistry();
        scanned_ = true;
    }

    std::wstring idW = utf8ToWide(packageId);
    std::wstring nameW = utf8ToWide(packageName);

    std::wstring idNorm = normalizeName(idW);
    std::wstring nameNorm = normalizeName(nameW);

    // 1. Try to match exact ID or exact name
    auto it = idToIconPath_.find(idNorm);
    if (it != idToIconPath_.end()) return it->second;

    it = nameToIconPath_.find(nameNorm);
    if (it != nameToIconPath_.end()) return it->second;

    // 2. Try substring match on Registry DisplayNames
    for (const auto& pair : nameToIconPath_) {
        const std::wstring& regNorm = pair.first;
        if (!nameNorm.empty() && (regNorm.find(nameNorm) != std::wstring::npos || nameNorm.find(regNorm) != std::wstring::npos)) {
            return pair.second;
        }
    }

    // 3. Try matching package ID parts (e.g. last token after dot)
    size_t lastDot = idW.find_last_of(L'.');
    if (lastDot != std::wstring::npos && lastDot + 1 < idW.size()) {
        std::wstring part = idW.substr(lastDot + 1);
        std::wstring partNorm = normalizeName(part);
        it = nameToIconPath_.find(partNorm);
        if (it != nameToIconPath_.end()) return it->second;
        
        for (const auto& pair : nameToIconPath_) {
            const std::wstring& regNorm = pair.first;
            if (!partNorm.empty() && (regNorm.find(partNorm) != std::wstring::npos || partNorm.find(regNorm) != std::wstring::npos)) {
                return pair.second;
            }
        }
    }

    return L"";
}

HICON IconManager::extractIcon(const std::wstring& iconPath) {
    if (iconPath.empty()) return nullptr;
    
    int index = 0;
    std::wstring cleanPath = cleanIconPath(iconPath, index);
    if (cleanPath.empty()) return nullptr;

    HICON hIcon = nullptr;
    UINT iconId = 0;
    // Attempt high-quality extraction of 32x32 icon
    UINT extracted = PrivateExtractIconsW(
        cleanPath.c_str(),
        index,
        32, 32,
        &hIcon,
        &iconId,
        1,
        LR_DEFAULTCOLOR
    );

    // Fallback using SHGetFileInfo if PrivateExtractIcons fails (e.g. for general file types or folders)
    if (!hIcon || extracted == 0) {
        SHFILEINFOW sfi = {};
        DWORD_PTR hr = SHGetFileInfoW(
            cleanPath.c_str(),
            0,
            &sfi,
            sizeof(sfi),
            SHGFI_ICON | SHGFI_LARGEICON
        );
        if (hr && sfi.hIcon) {
            hIcon = sfi.hIcon;
        }
    }

    return hIcon;
}

void IconManager::scanRegistry() {
    scanRegistryKey(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall", false);
    scanRegistryKey(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall", true);
    scanRegistryKey(HKEY_CURRENT_USER, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall", false);
}

void IconManager::scanRegistryKey(HKEY hRootKey, const wchar_t* subKeyPath, bool isWow64) {
    HKEY hKey = nullptr;
    REGSAM samDesired = KEY_READ;
    if (isWow64) {
        samDesired |= KEY_WOW64_32KEY;
    } else {
        samDesired |= KEY_WOW64_64KEY;
    }

    if (RegOpenKeyExW(hRootKey, subKeyPath, 0, samDesired, &hKey) != ERROR_SUCCESS) {
        return;
    }

    DWORD subKeysCount = 0;
    DWORD maxSubKeyLen = 0;
    if (RegQueryInfoKeyW(hKey, nullptr, nullptr, nullptr, &subKeysCount, &maxSubKeyLen, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS) {
        RegCloseKey(hKey);
        return;
    }

    std::vector<wchar_t> subKeyName(maxSubKeyLen + 1);

    for (DWORD i = 0; i < subKeysCount; ++i) {
        DWORD nameLen = maxSubKeyLen + 1;
        FILETIME ftLastWriteTime;
        if (RegEnumKeyExW(hKey, i, subKeyName.data(), &nameLen, nullptr, nullptr, nullptr, &ftLastWriteTime) == ERROR_SUCCESS) {
            HKEY hSubKey = nullptr;
            if (RegOpenKeyExW(hKey, subKeyName.data(), 0, samDesired, &hSubKey) == ERROR_SUCCESS) {
                wchar_t displayName[512] = {0};
                wchar_t displayIcon[1024] = {0};
                DWORD valType = 0;
                DWORD dataSize = sizeof(displayName);
                
                LSTATUS status = RegQueryValueExW(hSubKey, L"DisplayName", nullptr, &valType, reinterpret_cast<BYTE*>(displayName), &dataSize);
                if (status == ERROR_SUCCESS) {
                    dataSize = sizeof(displayIcon);
                    status = RegQueryValueExW(hSubKey, L"DisplayIcon", nullptr, &valType, reinterpret_cast<BYTE*>(displayIcon), &dataSize);
                    if (status == ERROR_SUCCESS && displayIcon[0] != L'\0') {
                        std::wstring nameW = displayName;
                        std::wstring iconW = displayIcon;
                        std::wstring nameNorm = normalizeName(nameW);
                        if (!nameNorm.empty()) {
                            nameToIconPath_[nameNorm] = iconW;
                        }
                        
                        std::wstring subKeyW = subKeyName.data();
                        std::wstring subKeyNorm = normalizeName(subKeyW);
                        if (!subKeyNorm.empty()) {
                            idToIconPath_[subKeyNorm] = iconW;
                        }
                    }
                }
                RegCloseKey(hSubKey);
            }
        }
    }

    RegCloseKey(hKey);
}

} // namespace pm
