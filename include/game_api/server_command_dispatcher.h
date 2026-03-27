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

struct ServerCommandDispatchStats
{
    std::size_t execute_cycles = 0;
    std::size_t executed_commands = 0;
    std::size_t executed_cfg_files = 0;
    std::size_t updated_cvars_from_cfg = 0;
    std::size_t queued_during_execution = 0;
    std::size_t remaining_pending_commands = 0;
    bool hit_execute_limit = false;
    std::vector<std::string> updated_cvar_names;
    std::vector<std::string> executed_cfg_paths;
};

class ServerCommandDispatcher
{
public:
    ServerCommandDispatcher(
        const filesystem::FileSystem& file_system,
        std::filesystem::path game_directory,
        CvarRegistry& cvar_registry,
        ServerCommandBuffer& command_buffer);

    void Dispatch(std::string_view command_text);
    void DrainPending(std::size_t max_commands_per_cycle = 128);
    bool IsExecuting() const noexcept;

    const ServerCommandDispatchStats& Stats() const noexcept;

private:
    const filesystem::FileSystem& file_system_;
    std::filesystem::path game_directory_;
    CvarRegistry& cvar_registry_;
    ServerCommandBuffer& command_buffer_;
    bool is_executing_ = false;
    ServerCommandDispatchStats stats_;
};
} // namespace hl::game_api
