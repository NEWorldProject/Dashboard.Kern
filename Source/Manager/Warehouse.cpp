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
        mWorkspaces.erase(iter);
    }

    Workspace* Warehouse::GetWorkspace(const std::string& name) {
        const auto i = mWorkspaces.find(name);
        if (i==mWorkspaces.end()) return nullptr;
        return &i->second;
    }

    void Warehouse::CreateWorkspace(const Warehouse::CheckoutArgs& args) {
        ValidateName(args.Name);
        const auto ws = mHome/WarehouseDir/WarehouseWorkspaceDir/args.Name;
        std::filesystem::create_directories(ws);
        // collect the information of all requests
        std::unordered_map<std::string, bool> flags{};
        std::unordered_map<std::string, std::pair<std::string, Module*>> collected{};
        for (auto&&[name, flag] : args.Modules) {
            if (collected.find(name)!=collected.end()) {
                throw std::runtime_error("Args Corrupted: Duplicated Module Id");
            }
            if (name.find("::")==std::string::npos) {
                collected.insert_or_assign(name, ResolveModuleName(name));
            }
            else {
                collected.insert_or_assign(name, SearchNamespacedId(name));
            }
            flags[name] = flag;
        }
        nlohmann::json list{};
        auto& ref = list["linked"] = nlohmann::json::array();
        std::filesystem::create_directories(mHome/args.Name);
        for (auto&[k, v]: collected) {
            auto part = nlohmann::json::object();
            auto flag = flags[k];
            part["uri"] = v.first;
            part["flag"] = flag;
            ref.push_back(part);
            auto pt = v.first;
            std::filesystem::create_directory_symlink(
                    v.second->GetContentPath(),
                    (flag ? mHome/args.Name : ws)/k
            );
        }
    }

    void Warehouse::ReloadWorkspaces() {
        std::vector<std::nested_exception> exceptions{};
        for (auto& m : mWorkspaces) {
            try { m.second.Reload(); }
            catch (...) { exceptions.emplace_back(); }
        }
        if (!exceptions.empty()) throw Utils::AggregateException(std::move(exceptions));
    }

    std::pair<std::string, Module*> Warehouse::ResolveModuleName(const std::string& name) {
        Module* target = nullptr;
        std::string uri{};
        EnumerateCabinets([&target, name = &name, &uri](Cabinet& cab) {
            const auto search = cab.Get(*name);
            if (search) {
                const auto thisId = cab.Namespace()+"::"+search->Id();
                if (!target) {
                    target = search;
                    uri = thisId;
                }
                else {
                    throw std::runtime_error("conflict for name "+*name+"between "+uri+" and "+thisId); //NOLINT
                }
            }
        });
        if (!target) throw std::runtime_error("Module not found:"+name);
        return {std::move(uri), target};
    }

    std::pair<std::string, Module*> Warehouse::SearchNamespacedId(const std::string& name) {
        const auto pos = name.find("::");
        const auto ns = name.substr(0, pos);
        const auto id = name.substr(pos+2);
        if (const auto cab = GetCabinet(ns); cab) {
            if (const auto md = cab->Get(id); md) {
                return {name, md};
            }
        }
        throw std::runtime_error("Module not found:"+name);
    }
}