#include "game_api/server_command_dispatcher.h"

#include <algorithm>
#include <cctype>
#include <utility>

#include "common/logger.h"
#include "common/text_encoding.h"
#include "game_api/cfg_executor.h"
#include "game_api/cvar_registry.h"
#include "game_api/server_command_buffer.h"

namespace
{
std::string TrimWhitespace(std::string_view text)
{
    std::size_t begin = 0;
    while (begin < text.size() && std::isspace(static_cast<unsigned char>(text[begin])) != 0)
    {
        ++begin;
    }

    std::size_t end = text.size();
    while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1])) != 0)
    {
        --end;
    }

    return std::string(text.substr(begin, end - begin));
}

void AppendUnique(std::vector<std::string>& values, std::string_view text)
{
    const std::string value(text);
    if (std::find(values.begin(), values.end(), value) == values.end())
    {
        values.push_back(value);
    }
}
} // namespace

namespace hl::game_api
{
ServerCommandDispatcher::ServerCommandDispatcher(
    const filesystem::FileSystem& file_system,
    std::filesystem::path game_directory,
    CvarRegistry& cvar_registry,
    ServerCommandBuffer& command_buffer)
    : file_system_(file_system)
    , game_directory_(std::move(game_directory))
    , cvar_registry_(cvar_registry)
    , command_buffer_(command_buffer)
{
}

void ServerCommandDispatcher::Dispatch(std::string_view command_text)
{
    const std::string command = TrimWhitespace(command_text);
    if (command.empty())
    {
        return;
    }

    ++stats_.executed_commands;
    common::Logger::Info("Executing server command: " + command);

    const std::size_t separator = command.find_first_of(" \t");
    const std::string verb = separator == std::string::npos
        ? command
        : command.substr(0, separator);
    const std::string arguments = separator == std::string::npos
        ? std::string()
        : TrimWhitespace(std::string_view(command).substr(separator + 1));

    if (verb == "exec")
    {
        if (arguments.empty())
        {
            common::Logger::Warn("Server command 'exec' is missing a filename.");
            return;
        }

        CfgExecutor cfg_executor(file_system_);
        const CfgExecutionResult result = cfg_executor.Execute(
            game_directory_,
            arguments,
            cvar_registry_,
            &command_buffer_);

        common::Logger::Info(
            "Server command 'exec' file "
            + std::string(result.found ? "found: " : "missing: ")
            + common::ToUtf8(result.path));

        if (result.found)
        {
            ++stats_.executed_cfg_files;
            stats_.updated_cvars_from_cfg += result.updated_cvars;
            AppendUnique(stats_.executed_cfg_paths, common::ToUtf8(result.path));

            for (const std::string& updated_name : result.updated_names)
            {
                AppendUnique(stats_.updated_cvar_names, updated_name);
            }
        }

        return;
    }

    common::Logger::Warn("Unknown server command: " + command);
}

void ServerCommandDispatcher::DrainPending(std::size_t max_commands_per_cycle)
{
    if (is_executing_)
    {
        common::Logger::Warn("Server command execute requested while a drain cycle is already active.");
        return;
    }

    is_executing_ = true;
    ++stats_.execute_cycles;

    std::size_t executed_this_cycle = 0;
    while (command_buffer_.HasPending())
    {
        if (executed_this_cycle >= max_commands_per_cycle)
        {
            stats_.hit_execute_limit = true;
            common::Logger::Warn(
                "Server command execute hit safety limit of "
                + std::to_string(max_commands_per_cycle)
                + " command(s); remaining commands stay queued.");
            break;
        }

        const ServerCommandBufferSnapshot before_snapshot = command_buffer_.Snapshot();
        const std::string command = command_buffer_.PopFront();
        if (command.empty())
        {
            continue;
        }

        Dispatch(command);
        ++executed_this_cycle;

        const ServerCommandBufferSnapshot after_snapshot = command_buffer_.Snapshot();
        if (after_snapshot.pending_total > before_snapshot.pending_total - 1)
        {
            const std::size_t queued_now =
                after_snapshot.pending_total - (before_snapshot.pending_total - 1);
            stats_.queued_during_execution += queued_now;
            common::Logger::Info(
                "Server command queued " + std::to_string(queued_now)
                + " additional command(s) during execution.");

            const std::size_t previous_tail_size = before_snapshot.pending_total > 0
                ? before_snapshot.pending_total - 1
                : 0;
            for (std::size_t index = previous_tail_size;
                 index < after_snapshot.pending_commands.size();
                 ++index)
            {
                common::Logger::Info(
                    "  queued during execution: " + after_snapshot.pending_commands[index]);
            }
        }
    }

    stats_.remaining_pending_commands = command_buffer_.PendingCount();
    common::Logger::Info(
        "Server command execute cycle complete: pending="
        + std::to_string(stats_.remaining_pending_commands));

    is_executing_ = false;
}

bool ServerCommandDispatcher::IsExecuting() const noexcept
{
    return is_executing_;
}

const ServerCommandDispatchStats& ServerCommandDispatcher::Stats() const noexcept
{
    return stats_;
}
} // namespace hl::game_api
