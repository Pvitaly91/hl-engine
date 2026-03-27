#include "game_api/dll_search_directories.h"

#include <algorithm>

namespace
{
std::filesystem::path MakeAbsolutePath(const std::filesystem::path& path)
{
    std::error_code error_code;
    const std::filesystem::path absolute_path = std::filesystem::absolute(path, error_code);
    return error_code ? path : absolute_path;
}

void AddUniquePath(
    const std::filesystem::path& path,
    std::vector<std::filesystem::path>& directories)
{
    if (path.empty())
    {
        return;
    }

    const std::filesystem::path absolute_path = MakeAbsolutePath(path);
    const auto existing = std::find(directories.begin(), directories.end(), absolute_path);
    if (existing == directories.end())
    {
        directories.push_back(absolute_path);
    }
}
} // namespace

namespace hl::game_api
{
std::vector<std::filesystem::path> BuildDllSearchDirectories(
    const std::filesystem::path& game_directory,
    const std::filesystem::path& module_path)
{
    std::vector<std::filesystem::path> directories;

    const std::filesystem::path absolute_game_directory = MakeAbsolutePath(game_directory);
    const std::filesystem::path game_root = absolute_game_directory.parent_path();
    const std::filesystem::path module_directory = MakeAbsolutePath(module_path).parent_path();

    AddUniquePath(game_root, directories);
    AddUniquePath(absolute_game_directory, directories);
    AddUniquePath(module_directory, directories);

    return directories;
}
} // namespace hl::game_api
