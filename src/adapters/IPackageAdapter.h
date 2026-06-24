#pragma once

#include "../core/PackageInfo.h"
#include <vector>
#include <functional>
#include <string>

namespace pm {

// Async callbacks for adapter operations. All invoked from worker threads.
using PackageListCallback = std::function<void(std::vector<PackageInfo> packages, std::string error)>;
using ActionCallback      = std::function<void(bool success, std::string message)>;

class IPackageAdapter {
public:
    virtual ~IPackageAdapter() = default;

    virtual PackageManager manager() const = 0;
    virtual std::string    managerName() const = 0;
    virtual bool           isAvailable() const = 0;

    // List installed packages.
    virtual void listInstalled(PackageListCallback cb) = 0;

    // List packages that have an available update.
    virtual void listUpgradable(PackageListCallback cb) = 0;

    // Search package index (e.g. "chrome").
    virtual void search(const std::string& query, PackageListCallback cb) = 0;

    // Run an action. progressCb is called with integer percent (0-100) on a worker thread.
    virtual void performAction(const PackageInfo& pkg,
                               TaskAction action,
                               std::function<void(int percent)> progressCb,
                               ActionCallback done) = 0;
};

} // namespace pm
