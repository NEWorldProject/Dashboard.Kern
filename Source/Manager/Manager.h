#pragma once

#include <chrono>
#include <string>
#include <vector>
#include <filesystem>
#include <unordered_map>
#include <exception>

namespace Configure::Manager {
	using SysSec = std::chrono::time_point<std::chrono::system_clock, std::chrono::seconds>;

	class Module {
	public:
		explicit Module(const std::filesystem::path& home);
		[[nodiscard]] auto& Id() const noexcept { return mId; }
		[[nodiscard]] auto& Uri() const noexcept { return mUri; }
		[[nodiscard]] auto& Display() const noexcept { return mDisplay; }
		[[nodiscard]] auto IsFull() const noexcept { return mIsFull; }
		[[nodiscard]] std::string LastUpdateUtc() const;
		[[nodiscard]] std::string LastCommitUtc() const;
		[[nodiscard]] SysSec LastUpdate() const noexcept { return mLastUpdate; }
		[[nodiscard]] SysSec LastCommit() const noexcept { return mLastCommit; }
		void Update();
		void Destruct();
		[[nodiscard]] std::filesystem::path GetContentPath() const;
	private:
		bool mIsFull;
		std::string mId, mUri, mDisplay;
		SysSec mLastUpdate, mLastCommit;
		std::filesystem::path mHome;
	};

	class Cabinet {
	public:
	    static Cabinet Fetch(const std::filesystem::path& home, const std::string& uri);
	    static Cabinet Open(const std::filesystem::path& home);
	    [[nodiscard]] auto& Namespace() const noexcept { return mNs; }
	    void Add(const std::string& uri, const std::string& name, const std::string& display);
	    void Remove(const std::string& name);
	    [[nodiscard]] Module* Get(const std::string& name);
	    template <class Fn> void Enumerate(Fn fn) { for (auto&& [_, x]: mModules) fn(x); }
	    // Internal API
        void UpdateUnsafe();
	private:
	    std::string mNs;
	    std::filesystem::path mHome;
	    std::unordered_map<std::string, Module> mModules;
    };

	class Workspace {
	public:
	    Workspace(const std::filesystem::path& home);
	    void Reload();
	    void Update();
	private:
	    std::vector<std::tuple<std::string, Module*, bool>> mList;
	};

	class Warehouse {
    public:
        struct CheckoutArgs {
            struct ModuleArgs {
                std::string Name;
                bool InTree = true;
            };
            std::string Name;
            std::vector<ModuleArgs> Modules;
        };

        explicit Warehouse(const std::filesystem::path& home);
        void ImportCabinet(const std::string& uri);
        void RemoveCabinet(const std::string& name);
        [[nodiscard]] Cabinet* GetCabinet(const std::string& name);
        void UpdateCabinet(const std::string& name);
        void UpdateCabinets();
        template <class Fn> void EnumerateCabinets(Fn fn) { for (auto&& [_, x]: mCabinets) fn(x); }
        void CreateWorkspace(const CheckoutArgs& args);
        void RemoveWorkspace(const std::string& name);
        [[nodiscard]] Workspace* GetWorkspace(const std::string& name);
        template <class Fn> void EnumerateWorkspaces(Fn fn) { for (auto&& [_, x]: mWorkspaces) fn(x); }
	private:
	    void Load(const std::filesystem::path& path);
        void ReloadWorkspaces();
        std::filesystem::path mHome;
        std::unordered_map<std::string, Cabinet> mCabinets;
        std::unordered_map<std::string, Workspace> mWorkspaces;
        std::pair<std::string, Module*> ResolveModuleName(const std::string& name);
        std::pair<std::string, Module*> SearchNamespacedId(const std::string& name);
    };
}
