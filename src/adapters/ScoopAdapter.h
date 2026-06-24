#pragma once

#include "IPackageAdapter.h"

namespace pm {

class ScoopAdapter : public IPackageAdapter {
public:
    PackageManager manager() const override { return PackageManager::Scoop; }
    std::string    managerName() const override { return "scoop"; }

    bool isAvailable() const override;

    void listInstalled(PackageListCallback cb) override;
    void listUpgradable(PackageListCallback cb) override;
    void search(const std::string& query, PackageListCallback cb) override;
    void performAction(const PackageInfo& pkg,
                       TaskAction action,
                       std::function<void(int percent)> progressCb,
                       ActionCallback done) override;
};

} // namespace pm