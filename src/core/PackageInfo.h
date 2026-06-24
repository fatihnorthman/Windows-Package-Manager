#pragma once

#include <string>
#include <string_view>

namespace pm {

enum class PackageManager {
    Unknown,
    Winget,
    Scoop,
    Chocolatey
};

enum class InstallState {
    Unknown,
    Queued,
    Installing,
    Updating,
    Installed,
    UpToDate,
    Failed
};

enum class TaskAction {
    Install,
    Upgrade,
    Uninstall
};

inline std::string_view toString(PackageManager m) {
    switch (m) {
        case PackageManager::Winget:     return "winget";
        case PackageManager::Scoop:      return "scoop";
        case PackageManager::Chocolatey: return "chocolatey";
        default:                          return "unknown";
    }
}

inline std::string_view toString(InstallState s) {
    switch (s) {
        case InstallState::Queued:     return "Queued";
        case InstallState::Installing: return "Installing";
        case InstallState::Updating:   return "Updating";
        case InstallState::Installed:  return "Installed";
        case InstallState::UpToDate:   return "UpToDate";
        case InstallState::Failed:     return "Failed";
        default:                        return "Unknown";
    }
}

inline std::string_view toString(TaskAction a) {
    switch (a) {
        case TaskAction::Install:   return "install";
        case TaskAction::Upgrade:   return "upgrade";
        case TaskAction::Uninstall: return "uninstall";
    }
    return "unknown";
}

inline PackageManager managerFromString(std::string_view s) {
    if (s == "winget" || s == "Winget")     return PackageManager::Winget;
    if (s == "scoop"  || s == "Scoop")      return PackageManager::Scoop;
    if (s == "chocolatey" || s == "Chocolatey" || s == "choco") return PackageManager::Chocolatey;
    return PackageManager::Unknown;
}

struct PackageInfo {
    std::string id;
    std::string name;
    std::string installedVersion;
    std::string availableVersion;
    PackageManager manager = PackageManager::Unknown;
    InstallState state      = InstallState::Unknown;
};

} // namespace pm
