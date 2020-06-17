#pragma once

#include "git2.h"
#include <string>
#include <stdexcept>
#include <filesystem>

namespace Git2 {
	class Exception : public std::runtime_error {
	public:
		[[nodiscard]] int Code() const noexcept { return mCode; }
		[[nodiscard]] int Klass() const noexcept { return mKlass; }
		[[noreturn]] static void Raise(int code);
	private:
		explicit Exception(const char* message) noexcept : runtime_error(message) {}
		int mCode{}, mKlass{};
    };

	struct UserSignature {
	    std::string Name;
	    std::string Email;
	};

	class Repository {
	public:
		static Repository Open(const std::filesystem::path& path);
		static Repository Create(const std::filesystem::path& path, bool isBare = false);
		static Repository Clone(const std::filesystem::path& path, const std::string& uri);
		void Fetch(const std::string& origin = "origin");
		void PullAuto(const UserSignature &sign, const std::string &origin = "origin");
	private:
		git_repository* mHandle;
	};
}