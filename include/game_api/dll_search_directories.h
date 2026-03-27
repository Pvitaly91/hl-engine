#pragma once

#include <filesystem>
#include <vector>

namespace hl::game_api
{
std::vector<std::filesystem::path> BuildDllSearchDirectories(
    const std::filesystem::path& game_directory,
    const std::filesystem::path& module_path);
} // namespace hl::game_api
