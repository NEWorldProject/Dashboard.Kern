#include "Manager.h"
#include "Json/Json.h"
#include "date/date.h"
#include "Git2/Repository.h"
#include <sstream>
#include <unordered_set>

namespace {
	// Path Notes
	constexpr std::string_view RepoPath{ "Repo" };
	constexpr std::string_view InfoPath { "info.json" };
	constexpr std::string_view BuildPath{ "BuildTree" };
    constexpr std::string_view ModulesPath{ "Modules" };
	// Messages
    constexpr std::string_view MsgCabinetCorrupted{ "Cabinet Corrupted" };
	constexpr std::string_view MsgModuleDirMissing{ "Module Directory Missing" };
	constexpr std::string_view MsgModuleDirCorrupted{ "Module Directory Corrupted" };
	constexpr std::string_view MsgModuleInfoCorrupted{ "Module Internal Info File Corrupted" };
	// Id Key
	constexpr std::string_view KeyModuleInfoId{ "id" };
	constexpr std::string_view KeyModuleInfoUri{ "uri" };
	constexpr std::string_view KeyModuleInfoDisplay{ "disp" };
	constexpr std::string_view KeyModuleInfoLPull{ "lup" };
	constexpr std::string_view KeyModuleInfoLCommit{ "lcm" };

	[[noreturn]] void Corruption(std::string_view message) { throw std::runtime_error(message.data()); }

	auto GetDate(const std::string& date) {
		std::istringstream in{ date };
		date::sys_seconds tp;
		in >> date::parse("%a,-%d-%b-%Y-%T-%z", tp);
		return tp;
	}

	auto FromDate(const date::sys_seconds& date) {
		std::ostringstream out {};
		out << date::format("%a,-%d-%b-%Y-%T-%z", date);
		return out.str();
	}

	void MakeSureRepoExists(const std::filesystem::path& home, const std::string& uri) {
		if (std::filesystem::exists(home / RepoPath)) return;
		Git2::Repository::Clone(home / RepoPath, uri);
	}
}

namespace Configure::Manager {
	Module::Module(const std::filesystem::path& home): mHome(std::filesystem::absolute(home)), mIsFull(false) {
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
	    static constexpr std::string_view charNotAllowed {"#<$+%>!`&*'\"|{?=}/\\: @"};
	    if (const auto pos = view.find_first_of(charNotAllowed); pos != std::string_view::npos) {
	        throw std::runtime_error(std::string("Invalid character '") + view[pos] + "' found in string");
	    }
	}

    void DoValidation(const nlohmann::json& list) {
        auto validate = [list = std::unordered_set<std::string>()](const std::string& val) mutable {
            if (list.find(val)!=list.end()) Corruption("Duplicated Module Name in Module List");
        };
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
            std::filesystem::create_directories(home / ModulesPath);
            for (auto&& x : list["modules"]) {
                const auto thisDir = home / ModulesPath / std::string(x[KeyModuleInfoId.data()]);
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
        Cabinet result {};
        result.mHome = home;
        for (auto&& x : list["modules"]) {
            const auto thisDir = home / ModulesPath / std::string(x[KeyModuleInfoId.data()]);
            result.mModules.insert_or_assign(std::string(x[KeyModuleInfoId.data()]), Module{thisDir});
        }
        return result;
    }

    void Cabinet::Add(const std::string& uri, const std::string& name, const std::string& display) {
	    ValidateName(name);
	    if (uri.empty()) throw std::runtime_error("Uri cannot be empty");
	    if (mModules.find(name) != mModules.end()) throw std::runtime_error("Name is already used");
	    const auto thisDir = mHome / ModulesPath / name;
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
        if (iter == mModules.end()) return;
        iter->second.Destruct();
        mModules.erase(iter);
    }

    Module* Cabinet::Get(const std::string& name) {
        const auto i = mModules.find(name);
        if (i == mModules.end()) return nullptr;
        return &i->second;
    }
}
