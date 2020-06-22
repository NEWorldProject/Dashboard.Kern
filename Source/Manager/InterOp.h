#pragma once

#include "Manager.h"

namespace Configure::Manager::InterOp {
    // Path Notes
    constexpr std::string_view RepoPath{"Repo"};
    constexpr std::string_view InfoPath{"info.json"};
    constexpr std::string_view BuildPath{"BuildTree"};
    constexpr std::string_view ModulesPath{"Modules"};
    constexpr std::string_view WarehouseDir{".nwds"};
    constexpr std::string_view WarehouseTempDir{"Temp"};
    constexpr std::string_view WarehouseStockDir{"Stock"};
    constexpr std::string_view WarehouseWorkspaceDir{"Ws"};
    constexpr std::string_view FetchProgressionTempDir{"FetchProgress"};
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

    inline void ValidateName(std::string_view view) {
        static constexpr std::string_view charNotAllowed{"#<$+%>!`&*'\"|{?=}/\\: @"};
        if (const auto pos = view.find_first_of(charNotAllowed); pos!=std::string_view::npos) {
            throw std::runtime_error(std::string("Invalid character '")+view[pos]+"' found in string");
        }
    }

    [[noreturn]] inline void Corruption(std::string_view message) { throw std::runtime_error(message.data()); }

    Workspace WorkspaceCheckoutHelper(
            Warehouse& warehouse,
            const std::filesystem::path& home,
            const Warehouse::CheckoutArgs& args
    );
}
