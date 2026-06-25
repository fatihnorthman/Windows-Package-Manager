#pragma once

#include <windows.h>
#include <string>
#include <unordered_map>
#include <mutex>

namespace pm {

class IconManager {
public:
    static IconManager& instance();

    // Prevent copying
    IconManager(const IconManager&) = delete;
    IconManager& operator=(const IconManager&) = delete;

    std::wstring findIconPath(const std::string& packageId, const std::string& packageName);
    HICON extractIcon(const std::wstring& iconPath);

private:
    IconManager();
    ~IconManager();

    void scanRegistry();
    void scanRegistryKey(HKEY hRootKey, const wchar_t* subKeyPath, bool isWow64 = false);
    
    std::wstring normalizeName(const std::wstring& name);

    std::unordered_map<std::wstring, std::wstring> nameToIconPath_;
    std::unordered_map<std::wstring, std::wstring> idToIconPath_;
    
    std::mutex mutex_;
    bool scanned_ = false;
};

} // namespace pm
