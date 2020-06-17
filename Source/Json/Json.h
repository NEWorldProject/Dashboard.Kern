#pragma once

#include "nlohmann/json.hpp"
#include <fstream>

namespace Json {
	inline nlohmann::json Load(const std::filesystem::path& file) {
		nlohmann::json list{};
		std::ifstream stream{ file };
		stream >> list;
		return list;
	}

	inline void Save(const std::filesystem::path& file, const nlohmann::json& json) {
		std::ofstream stream{ file };
		stream << std::setw(4) << json << std::endl;
	}
}
