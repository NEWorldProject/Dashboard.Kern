#include "Manager.h"
#include "Json/Json.h"
#include "date/date.h"
#include "Git2/Repository.h"
#include <sstream>
#include <unordered_set>
#include "Utils/Exception.h"

namespace {
    // Path Notes
    constexpr std::string_view RepoPath{"Repo"};
    constexpr std::string_view InfoPath{"info.json"};
    constexpr std::string_view BuildPath{"BuildTree"};
    constexpr std::string_view ModulesPath{"Modules"};
    constexpr std::string_view WarehouseDir{".nwds"};
    constexpr std::string_view WarehouseTempDir{"Temp"};
    constexpr std::string_view WarehouseStockDir{"Stock"};
    // Messages
    constexpr std::string_view MsgCabinetCorrupted{"Cabinet Corrupted"};
    constexpr std::string_view MsgCabinetConflict{"Cabinet with the same name already exists"};
    constexpr std::string_view MsgModuleDirMissing{"Module Directory Missing"};
    constexpr std::string_view MsgModuleDirCorrupted{"Module Directory Corrupted"};
    constexpr std::string_view MsgModuleInfoCorrupted{"Module Internal Info File Corrupted"};
    // Id Key
    constexpr std::string_view KeyModuleInfoId{"id"};
    constexpr std::string_view KeyModuleInfoUri{"uri"};
    constexpr std::string_view KeyModuleInfoDisplay{"usr"};
    constexpr std::string_view KeyModuleInfoLPull{"lup"};
    constexpr std::string_view KeyModuleInfoLCommit{"lcm"};
    constexpr std::string_view KeyCabinetNamespace{"ns"};

    [[noreturn]] void Corruption(std::string_view message) { throw std::runtime_error(message.data()); }

    auto GetDate(const std::string& date) {
        std::istringstream in{date};
        date::sys_seconds tp;
        in >> date::parse("%a,-%d-%b-%Y-%T-%z", tp);
        return tp;
    }

    auto FromDate(const date::sys_seconds& date) {
        std::ostringstream out{};
        out << date::format("%a,-%d-%b-%Y-%T-%z", date);
        return out.str();
    }

    void MakeSureRepoExists(const std::filesystem::path& home, const std::string& uri) {
        if (std::filesystem::exists(home/RepoPath)) return;
        Git2::Repository::Clone(home/RepoPath, uri);
    }
}

namespace Configure::Manager {
    Module::Module(const std::filesystem::path& home)
            :mHome(std::filesystem::absolute(home)), mIsFull(false) {
        if (!std::filesystem::exists(home)) Corruption(MsgModuleDirMissing);
        if (!std::filesystem::exists(home/InfoPath)) Corruption(MsgModuleDirCorrupted);
        mIsFull = std::filesystem::exists(home/RepoPath);
        // Load Basic Info
        const auto waterfall = [&]() {
            const auto list = Json::Load(home/InfoPath);
            // Fields must exist
            if (const auto x = list.find(KeyModuleInfoId.data()); x!=list.end()) mId = *x; else return false;
            if (const auto x = list.find(KeyModuleInfoUri.data()); x!=list.end()) mUri = *x; else return false;
            if (const auto x = list.find(KeyModuleInfoDisplay.data()); x!=list.end()) mDisplay = *x; else return false;
            // Optional Fields
            if (const auto x = list.find(KeyModuleInfoLPull.data()); x!=list.end()) mLastUpdate = GetDate(*x);
            if (const auto x = list.find(KeyModuleInfoLCommit.data()); x!=list.end()) mLastCommit = GetDate(*x);
            return true;
        };
        if (!waterfall()) Corruption(MsgModuleInfoCorrupted);
    }

    std::string Module::LastUpdateUtc() const { return FromDate(LastUpdate()); }

    std::string Module::LastCommitUtc() const { return FromDate(LastUpdate()); }

    void Module::Update() {
        MakeSureRepoExists(mHome, mUri);
        auto repo = Git2::Repository::Open(mHome/RepoPath);
        repo.PullAuto({"DWVoid", "yshliu0321@icloud.com"}); // TODO: Add a config option for this
        mLastUpdate = std::chrono::time_point_cast<std::chrono::seconds>(std::chrono::system_clock::now());
    }

    void Module::Destruct() {
        if (std::filesystem::exists(mHome)) std::filesystem::remove_all(mHome);
    }

    static void ValidateName(std::string_view view) {
        static constexpr std::string_view charNotAllowed{"#<$+%>!`&*'\"|{?=}/\\: @"};
        if (const auto pos = view.find_first_of(charNotAllowed); pos!=std::string_view::npos) {
            throw std::runtime_error(std::string("Invalid character '")+view[pos]+"' found in string");
        }
    }

    void DoValidation(const nlohmann::json& list) {
        auto validate = [list = std::unordered_set<std::string>()](const std::string& val) mutable {
            if (list.find(val)!=list.end()) Corruption("Duplicated Module Name in Module List");
        };
        validate(list["ns"]);
        for (auto&& x : list["modules"]) validate(std::string(x[KeyModuleInfoId.data()]));
    }

    Cabinet Cabinet::Fetch(const std::filesystem::path& home, const std::string& uri) {
        try {
            std::filesystem::create_directories(home);
            MakeSureRepoExists(home, uri);
            const auto infoFilePath = home/RepoPath/InfoPath;
            if (!std::filesystem::exists(infoFilePath)) Corruption(MsgCabinetCorrupted);
            const auto list = Json::Load(infoFilePath);
            DoValidation(list);
            std::filesystem::create_directories(home/ModulesPath);
            for (auto&& x : list["modules"]) {
                const auto thisDir = home/ModulesPath/std::string(x[KeyModuleInfoId.data()]);
                std::filesystem::create_directories(thisDir);
                Json::Save(thisDir/InfoPath, x);
            }
        }
        catch (...) {
            std::filesystem::remove_all(home);
        }
        return Open(home);
    }

    Cabinet Cabinet::Open(const std::filesystem::path& home) {
        const auto infoFilePath = home/RepoPath/InfoPath;
        if (!std::filesystem::exists(infoFilePath)) Corruption(MsgCabinetCorrupted);
        const auto list = Json::Load(infoFilePath);
        DoValidation(list);
        Cabinet result{};
        result.mNs = list[KeyCabinetNamespace.data()];
        result.mHome = home;
        for (auto&& x : list["modules"]) {
            const auto thisDir = home/ModulesPath/std::string(x[KeyModuleInfoId.data()]);
            result.mModules.insert_or_assign(std::string(x[KeyModuleInfoId.data()]), Module{thisDir});
        }
        return result;
    }

    void Cabinet::UpdateUnsafe() {
        auto repo = Git2::Repository::Open(mHome/RepoPath);
        repo.PullAuto({"DWVoid", "yshliu0321@icloud.com"}); // TODO: Add a config option for this
    }

    void Cabinet::Add(const std::string& uri, const std::string& name, const std::string& display) {
        ValidateName(name);
        if (uri.empty()) throw std::runtime_error("Uri cannot be empty");
        if (mModules.find(name)!=mModules.end()) throw std::runtime_error("Name is already used");
        const auto thisDir = mHome/ModulesPath/name;
        std::filesystem::create_directories(thisDir);
        nlohmann::json json{};
        json[KeyModuleInfoId.data()] = uri;
        json[KeyModuleInfoUri.data()] = name;
        json[KeyModuleInfoDisplay.data()] = display;
        Json::Save(thisDir/InfoPath, json);
        mModules.insert_or_assign(name, Module{thisDir});
    }

    void Cabinet::Remove(const std::string& name) {
        const auto iter = mModules.find(name);
        if (iter==mModules.end()) return;
        iter->second.Destruct();
        mModules.erase(iter);
    }

    Module* Cabinet::Get(const std::string& name) {
        const auto i = mModules.find(name);
        if (i==mModules.end()) return nullptr;
        return &i->second;
    }

    Warehouse::Warehouse(const std::filesystem::path& home) : mHome(home) {
        const auto base = home/WarehouseDir;
        std::filesystem::create_directories(base);
        std::filesystem::create_directories(base/WarehouseTempDir);
        std::filesystem::create_directories(base/WarehouseStockDir);
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
        const auto tmpTarget = temp/"FetchProgress";
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

    void Warehouse::Load(const std::filesystem::path& path) {
        auto cab = Cabinet::Open(path);
        mCabinets.insert_or_assign(cab.Namespace(), std::move(cab));
    }

    void Warehouse::ReloadWorkspaces() {
        std::vector<std::nested_exception> exceptions{};
        for (auto& m : mWorkspaces) {
            try { m.second.Reload(); }
            catch (...) { exceptions.emplace_back(); }
        }
        if (!exceptions.empty()) throw Utils::AggregateException(std::move(exceptions));
    }

    void Workspace::Reload() {

    }

    void Workspace::Update() {
        std::vector<std::nested_exception> exceptions{};
        for (auto& m : mList) {
            try { std::get<1>(m)->Update(); }
            catch (...) { exceptions.emplace_back(); }
        }
        if (!exceptions.empty()) throw Utils::AggregateException(std::move(exceptions));
    }
}
