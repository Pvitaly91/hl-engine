#include "game_api/cfg_executor.h"

#include <algorithm>
#include <cctype>
#include <sstream>

#include "common/logger.h"
#include "common/text_encoding.h"
#include "filesystem/file_system.h"
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

std::string StripLineComment(std::string_view line)
{
    const std::size_t comment_position = line.find("//");
    if (comment_position != std::string_view::npos)
    {
        line = line.substr(0, comment_position);
    }

    return TrimWhitespace(line);
}

std::string NormalizeValue(std::string_view value)
{
    std::string normalized = TrimWhitespace(value);
    if (normalized.size() >= 2 && normalized.front() == '"' && normalized.back() == '"')
    {
        normalized = normalized.substr(1, normalized.size() - 2);
    }

    return normalized;
}

bool IsQueueableConfigCommand(std::string_view verb)
{
    return verb == "exec";
}

void AppendUnique(std::vector<std::string>& values, std::string_view value)
{
    const std::string text(value);
    if (std::find(values.begin(), values.end(), text) == values.end())
    {
        values.push_back(text);
    }
}
} // namespace

namespace hl::game_api
{
CfgExecutor::CfgExecutor(const filesystem::FileSystem& file_system)
    : file_system_(file_system)
{
}

CfgExecutionResult CfgExecutor::Execute(
    const std::filesystem::path& game_directory,
    std::string_view filename,
    CvarRegistry& cvar_registry,
    ServerCommandBuffer* command_buffer) const
{
    CfgExecutionResult result;

    const std::filesystem::path requested_path = std::filesystem::path(std::string(filename));
    result.path = requested_path.is_absolute()
        ? requested_path
        : file_system_.AbsolutePath(game_directory / requested_path);

    const bool file_exists = file_system_.FileExists(result.path);
    common::Logger::Info(
        "CFG resolve: " + common::ToUtf8(result.path)
        + " (" + (file_exists ? std::string("exists") : std::string("missing")) + ")");

    const std::optional<std::string> file_contents = file_system_.ReadTextFile(result.path);
    if (!file_contents.has_value())
    {
        common::Logger::Warn("CFG file not found, skipping: " + common::ToUtf8(result.path));
        return result;
    }

    result.found = true;
    common::Logger::Info("Executing cfg file: " + common::ToUtf8(result.path));

    std::istringstream stream(*file_contents);
    std::string raw_line;
    std::size_t line_number = 0;
    while (std::getline(stream, raw_line))
    {
        ++line_number;

        const std::string line = StripLineComment(raw_line);
        if (line.empty())
        {
            continue;
        }

        const std::size_t separator = line.find_first_of(" \t");
        if (separator == std::string::npos)
        {
            common::Logger::Warn(
                "Unsupported cfg line in " + common::ToUtf8(result.path)
                + ":" + std::to_string(line_number) + ": " + line);
            continue;
        }

        const std::string name = line.substr(0, separator);
        if (IsQueueableConfigCommand(name))
        {
            if (command_buffer == nullptr)
            {
                common::Logger::Warn(
                    "CFG line requires command queue but none is available: " + line);
                continue;
            }

            command_buffer->Queue(line);
            ++result.queued_commands;
            AppendUnique(result.queued_command_lines, line);
            common::Logger::Info("Queued cfg command: " + line);
            continue;
        }

        const std::string value = NormalizeValue(std::string_view(line).substr(separator + 1));
        if (value.empty())
        {
            common::Logger::Warn(
                "Missing cfg value for '" + name + "' in " + common::ToUtf8(result.path)
                + ":" + std::to_string(line_number));
            continue;
        }

        CvarSetResult set_result;
        if (cvar_registry.SetValue(name, value))
        {
            set_result.updated = true;
        }
        else if (CvarRegistry::IsSafeAutoCreateName(name))
        {
            set_result = cvar_registry.SetValueOrCreate(name, value);
        }
        else
        {
            common::Logger::Warn(
                "CFG references unsupported command/cvar '" + name + "' in "
                + common::ToUtf8(result.path) + ":" + std::to_string(line_number)
                + "; skipping.");
            continue;
        }

        common::Logger::Info(
            "Applied cfg value: " + name + " = " + value
            + (set_result.created ? " (auto-created)" : ""));
        ++result.updated_cvars;
        AppendUnique(result.updated_names, name);
    }

    return result;
}
} // namespace hl::game_api
