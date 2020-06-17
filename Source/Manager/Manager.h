#pragma once

#include <chrono>
#include <string>
#include <optional>
#include <filesystem>
#include <unordered_map>

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
	    void Add(const std::string& uri, const std::string& name, const std::string& display);
	    void Remove(const std::string& name);
	    Module* Get(const std::string& name);
	    template <class Fn>
	    void Enumerate(Fn fn) { for (auto&& [_, x]: mModules) fn(x); }
	private:
	    std::filesystem::path mHome;
	    std::unordered_map<std::string, Module> mModules;
    };
}