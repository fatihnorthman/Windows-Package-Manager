#pragma once

#include "IPackageAdapter.h"
#include <string>

namespace pm {

class WingetAdapter : public IPackageAdapter {
public:
    PackageManager manager() const override { return PackageManager::Winget; }
    std::string    managerName() const override { return "winget"; }

    bool isAvailable() const override;

    void listInstalled(PackageListCallback cb) override;
    void listUpgradable(PackageListCallback cb) override;
    void search(const std::string& query, PackageListCallback cb) override;
    void performAction(const PackageInfo& pkg,
                       TaskAction action,
                       std::function<void(int percent)> progressCb,
                       ActionCallback done) override;
    void setMsStoreSearchEnabled(bool enabled) { msStoreSearchEnabled_ = enabled; }

private:
    bool msStoreSearchEnabled_ = false;
};

} // namespace pm
