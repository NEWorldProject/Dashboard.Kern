#include "Utils/Exception.h"
#include "Json/Json.h"
#include <sstream>
#include <algorithm>
#include "InterOp.h"

using namespace Configure::Manager::InterOp;

namespace Configure::Manager {
    Warehouse::Warehouse(const std::filesystem::path& home)
            :mHome(home) {
        const auto base = home/WarehouseDir;
        std::filesystem::create_directories(base);
        std::filesystem::create_directories(base/WarehouseTempDir);
        std::filesystem::create_directories(base/WarehouseStockDir);
        std::filesystem::create_directories(base/WarehouseWorkspaceDir);
        for (auto&& x: std::filesystem::directory_iterator(base/WarehouseStockDir)) {
            if (!x.is_directory()) continue;
            try {
                Load(x);
            }
            catch (...) {
                //ignore
            }
        }
    }

    void Warehouse::ImportCabinet(const std::string& uri) {
        std::string ns{};
        const auto base = mHome/WarehouseDir;
        const auto temp = base/WarehouseTempDir;
        const auto stock = base/WarehouseStockDir;
        const auto tmpTarget = temp/FetchProgressionTempDir;
        // fetch the expand the target cabinet into a temp directory as we ho not know the name yet.
        // if this step succeeds, we are sure that the target cabinet is in good state
        try {
            const auto cab = Cabinet::Fetch(tmpTarget, uri);
            // scan the namespace name against the list. if there is conflict, throw a runtime error with message
            if (mCabinets.find(cab.Namespace())!=mCabinets.end()) Corruption(MsgCabinetConflict);
            // move the cab
            ns = cab.Namespace();
            std::filesystem::rename(tmpTarget, stock/ns);
        }
        catch (...) {
            // in-case of exception, drop the temp cab dir
            std::filesystem::remove_all(tmpTarget);
            throw;
        }
        // finally, load the cabinet into the list
        Load(stock/ns);
    }

    void Warehouse::RemoveCabinet(const std::string& name) {
        const auto iter = mCabinets.find(name);
        if (iter==mCabinets.end()) return;
        mCabinets.erase(iter);
    }

    Cabinet* Warehouse::GetCabinet(const std::string& name) {
        const auto i = mCabinets.find(name);
        if (i==mCabinets.end()) return nullptr;
        return &i->second;
    }

    void Warehouse::UpdateCabinet(const std::string& name) {
        if (const auto cab = GetCabinet(name); cab) {
            cab->UpdateUnsafe();
            ReloadWorkspaces();
        }
    }

    void Warehouse::UpdateCabinets() {
        std::vector<std::nested_exception> transaction{};
        try {
            std::vector<std::nested_exception> updateErr{};
            for (auto& x : mCabinets) {
                try { x.second.UpdateUnsafe(); }
                catch (...) { updateErr.emplace_back(); }
            }
            if (!updateErr.empty()) throw Utils::AggregateException(std::move(updateErr));
        }
        catch (...) {
            try { std::throw_with_nested(std::runtime_error("Failures during update:")); }
            catch (...) { transaction.emplace_back(); }
        }
        try { ReloadWorkspaces(); }
        catch (...) {
            try { std::throw_with_nested(std::runtime_error("Failures during reload:")); }
            catch (...) { transaction.emplace_back(); }
        }
        if (!transaction.empty()) throw Utils::AggregateException(std::move(transaction));
    }

    void Warehouse::RemoveWorkspace(const std::string& name) {
        const auto iter = mWorkspaces.find(name);
        if (iter==mWorkspaces.end()) return;
        iter->second.Destruct();
        mWorkspaces.erase(iter);
    }

    Workspace* Warehouse::GetWorkspace(const std::string& name) {
        const auto i = mWorkspaces.find(name);
        if (i==mWorkspaces.end()) return nullptr;
        return &i->second;
    }

    void Warehouse::CreateWorkspace(const Warehouse::CheckoutArgs& args) {
        auto create = WorkspaceCheckoutHelper(*this, mHome, args);
        mWorkspaces.insert_or_assign(args.Name, std::move(create));
    }

    void Warehouse::ReloadWorkspaces() {
        std::vector<std::nested_exception> exceptions{};
        for (auto& m : mWorkspaces) {
            try { m.second.Reload(); }
            catch (...) { exceptions.emplace_back(); }
        }
        if (!exceptions.empty()) throw Utils::AggregateException(std::move(exceptions));
    }
}