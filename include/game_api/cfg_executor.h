#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace hl::filesystem
{
class FileSystem;
}

namespace hl::game_api
{
class CvarRegistry;
class ServerCommandBuffer;

struct CfgExecutionResult
{
    std::filesystem::path path;
    bool found = false;
    std::size_t updated_cvars = 0;
    std::size_t queued_commands = 0;
    std::vector<std::string> updated_names;
    std::vector<std::string> queued_command_lines;
};

class CfgExecutor
{
public:
    explicit CfgExecutor(const filesystem::FileSystem& file_system);

    CfgExecutionResult Execute(
        const std::filesystem::path& game_directory,
        std::string_view filename,
        CvarRegistry& cvar_registry,
        ServerCommandBuffer* command_buffer = nullptr) const;

private:
    const filesystem::FileSystem& file_system_;
};
} // namespace hl::game_api
