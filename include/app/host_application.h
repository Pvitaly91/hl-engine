#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "app/launch_options.h"
#include "filesystem/file_system.h"
#include "filesystem/valve_directory.h"
#include "game_api/game_module_loader.h"

namespace hl::app
{
class HostApplication
{
public:
    int Run(const LaunchOptions& options) const;

private:
    void LogCheckedCandidates(const std::vector<std::filesystem::path>& candidates) const;
    void LogValveValidationStatus(
        const filesystem::ValveDirectoryValidationResult& validation) const;
    bool RunDllSmokeTest(const std::filesystem::path& game_directory) const;
    bool RunServerEngineShim(
        const std::filesystem::path& game_directory,
        const std::filesystem::path& client_dll_path,
        const LaunchOptions& options) const;

    filesystem::FileSystem file_system_;
    game_api::Win32GameModuleLoader module_loader_;
};
} // namespace hl::app
