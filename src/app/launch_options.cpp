#include "app/launch_options.h"

#include "common/text_encoding.h"

#include <array>
#include <algorithm>
#include <cmath>
#include <cwctype>
#include <limits>
#include <sstream>
#include <string_view>

namespace
{
struct ParseState
{
    bool dedicated_explicit = false;
    bool maxclients_explicit = false;
    bool deathmatch_explicit = false;
    bool coop_explicit = false;
    bool log_directory_explicit = false;
    bool log_console_level_explicit = false;
    bool log_file_level_explicit = false;
    bool log_level_explicit = false;
    bool log_frame_sample_explicit = false;
    bool log_state_changes_only_explicit = false;
    bool log_summary_file_explicit = false;
    bool log_suppress_repeats_explicit = false;
};

bool StartsWith(std::wstring_view value, std::wstring_view prefix)
{
    return value.size() >= prefix.size() && value.compare(0, prefix.size(), prefix) == 0;
}

std::wstring TrimCopy(std::wstring_view value)
{
    std::size_t begin = 0;
    while (begin < value.size() && std::iswspace(value[begin]) != 0)
    {
        ++begin;
    }

    std::size_t end = value.size();
    while (end > begin && std::iswspace(value[end - 1]) != 0)
    {
        --end;
    }

    return std::wstring(value.substr(begin, end - begin));
}

std::wstring ToLowerCopy(std::wstring_view value)
{
    std::wstring lowered = TrimCopy(value);
    std::transform(
        lowered.begin(),
        lowered.end(),
        lowered.begin(),
        [](wchar_t character)
        {
            return static_cast<wchar_t>(std::towlower(character));
        });
    return lowered;
}

std::string NarrowAscii(std::wstring_view value)
{
    std::string text;
    text.reserve(value.size());
    for (wchar_t character : value)
    {
        text.push_back(
            character >= 0 && character <= 0x7F
                ? static_cast<char>(character)
                : '?');
    }
    return text;
}

std::optional<std::string> SanitizeRunLabel(std::wstring_view value)
{
    const std::wstring trimmed = TrimCopy(value);
    if (trimmed.empty())
    {
        return std::nullopt;
    }

    std::string sanitized;
    sanitized.reserve(trimmed.size());

    const auto append_separator =
        [&sanitized](char separator)
        {
            if (sanitized.empty())
            {
                return;
            }

            const char last = sanitized.back();
            if (last == '-' || last == '_')
            {
                return;
            }

            sanitized.push_back(separator);
        };

    for (wchar_t character : trimmed)
    {
        if ((character >= L'0' && character <= L'9')
            || (character >= L'A' && character <= L'Z')
            || (character >= L'a' && character <= L'z'))
        {
            sanitized.push_back(static_cast<char>(character));
            continue;
        }

        if (character == L'-' || character == L'_')
        {
            append_separator(static_cast<char>(character));
            continue;
        }

        append_separator('-');
    }

    while (!sanitized.empty()
        && (sanitized.back() == '-' || sanitized.back() == '_'))
    {
        sanitized.pop_back();
    }

    if (sanitized.empty())
    {
        return std::nullopt;
    }

    return sanitized;
}

std::optional<std::string> ParsePromptId(std::wstring_view value)
{
    const std::wstring trimmed = TrimCopy(value);
    if (trimmed.empty())
    {
        return std::nullopt;
    }

    return hl::common::ToUtf8(trimmed);
}

bool TryParseInteger(std::wstring_view text, int* value)
{
    if (value == nullptr || text.empty())
    {
        return false;
    }

    try
    {
        std::size_t consumed = 0;
        const int parsed = std::stoi(std::wstring(text), &consumed, 10);
        if (consumed != text.size())
        {
            return false;
        }

        *value = parsed;
        return true;
    }
    catch (...)
    {
        return false;
    }
}

bool TryParseFloat(std::wstring_view text, float* value)
{
    if (value == nullptr || text.empty())
    {
        return false;
    }

    try
    {
        std::size_t consumed = 0;
        const float parsed = std::stof(std::wstring(text), &consumed);
        if (consumed != text.size() || !std::isfinite(parsed))
        {
            return false;
        }

        *value = parsed;
        return true;
    }
    catch (...)
    {
        return false;
    }
}

bool TryParseBoolFlag(std::wstring_view text, bool* value)
{
    if (value == nullptr)
    {
        return false;
    }

    const std::wstring normalized = TrimCopy(text);
    if (normalized == L"1" || normalized == L"true" || normalized == L"on" || normalized == L"yes")
    {
        *value = true;
        return true;
    }
    if (normalized == L"0" || normalized == L"false" || normalized == L"off" || normalized == L"no")
    {
        *value = false;
        return true;
    }

    return false;
}

bool TryParseLogLevel(std::wstring_view text, hl::common::LogLevel* value)
{
    if (value == nullptr)
    {
        return false;
    }

    const std::optional<hl::common::LogLevel> parsed =
        hl::common::ParseLogLevel(NarrowAscii(TrimCopy(text)));
    if (!parsed.has_value())
    {
        return false;
    }

    *value = *parsed;
    return true;
}

bool TryParseRegressionGuardProfile(
    std::wstring_view text,
    hl::app::RegressionGuardProfile* value)
{
    if (value == nullptr)
    {
        return false;
    }

    const std::wstring normalized = ToLowerCopy(text);
    if (normalized == L"trainstop26-terminal-probe"
        || normalized == L"trainstop26-terminal"
        || normalized == L"trainstop26-stop-probe")
    {
        *value = hl::app::RegressionGuardProfile::kTrainstop26TerminalProbe;
        return true;
    }

    if (normalized == L"trainstop26-baseline"
        || normalized == L"c0a0-trainstop26-baseline")
    {
        *value = hl::app::RegressionGuardProfile::kTrainstop26Baseline;
        return true;
    }

    if (normalized == L"changelevel-latch-only-continuation")
    {
        *value = hl::app::RegressionGuardProfile::kChangelevelLatchOnlyContinuation;
        return true;
    }

    if (normalized == L"changelevel-request-consumed"
        || normalized == L"trigger-changelevel-consumed")
    {
        *value = hl::app::RegressionGuardProfile::kChangelevelRequestConsumed;
        return true;
    }

    return false;
}

bool TryParseCategoryList(
    std::wstring_view text,
    std::vector<hl::common::LogCategory>* categories,
    std::optional<std::wstring>* error_message)
{
    if (categories == nullptr)
    {
        return false;
    }

    categories->clear();
    std::wstring remaining = TrimCopy(text);
    if (remaining.empty())
    {
        if (error_message != nullptr)
        {
            *error_message = L"Empty category list.";
        }
        return false;
    }

    while (!remaining.empty())
    {
        const std::size_t comma = remaining.find(L',');
        const std::wstring token =
            TrimCopy(comma == std::wstring::npos ? remaining : remaining.substr(0, comma));
        if (token.empty())
        {
            if (error_message != nullptr)
            {
                *error_message = L"Empty category token in list.";
            }
            return false;
        }

        const std::optional<hl::common::LogCategory> category =
            hl::common::ParseLogCategory(NarrowAscii(token));
        if (!category.has_value())
        {
            if (error_message != nullptr)
            {
                *error_message = L"Unknown log category: " + token;
            }
            return false;
        }

        if (std::find(categories->begin(), categories->end(), *category) == categories->end())
        {
            categories->push_back(*category);
        }

        if (comma == std::wstring::npos)
        {
            break;
        }

        remaining = remaining.substr(comma + 1);
    }

    return true;
}

bool ParseRequiredBoolOption(
    int argc,
    wchar_t* argv[],
    int* index,
    bool* value,
    std::optional<std::wstring>* error_message,
    std::wstring_view option_name)
{
    if (*index + 1 >= argc)
    {
        *error_message = L"Missing value for " + std::wstring(option_name) + L".";
        return false;
    }

    if (!TryParseBoolFlag(argv[++(*index)], value))
    {
        *error_message =
            L"Invalid value for " + std::wstring(option_name) + L". Expected 0 or 1.";
        return false;
    }

    return true;
}

bool ParseBoolValue(
    std::wstring_view value,
    bool* parsed_value,
    std::optional<std::wstring>* error_message,
    std::wstring_view option_name)
{
    if (!TryParseBoolFlag(value, parsed_value))
    {
        *error_message =
            L"Invalid value for " + std::wstring(option_name) + L". Expected 0 or 1.";
        return false;
    }

    return true;
}

bool PathExists(const std::filesystem::path& path)
{
    std::error_code error;
    return std::filesystem::exists(path, error);
}

bool LooksLikeHostRepositoryRoot(const std::filesystem::path& candidate)
{
    return PathExists(candidate / ".git")
        && PathExists(candidate / "CMakeLists.txt")
        && PathExists(candidate / "include")
        && PathExists(candidate / "src");
}

bool LooksLikeWorkspaceRoot(const std::filesystem::path& candidate)
{
    return PathExists(candidate / "CMakePresets.json")
        && LooksLikeHostRepositoryRoot(candidate / "host");
}

std::filesystem::path TryResolveHostRepositoryRootFromBase(std::filesystem::path base_path)
{
    if (base_path.empty())
    {
        return {};
    }

    std::error_code error;
    std::filesystem::path current = std::filesystem::absolute(base_path, error);
    if (error)
    {
        current = base_path;
        error.clear();
    }

    for (int depth = 0; depth < 8; ++depth)
    {
        if (LooksLikeHostRepositoryRoot(current))
        {
            return current;
        }

        if (LooksLikeWorkspaceRoot(current))
        {
            return current / "host";
        }

        const std::filesystem::path parent = current.parent_path();
        if (parent.empty() || parent == current)
        {
            break;
        }

        current = parent;
    }

    return {};
}

std::filesystem::path ResolveDefaultLogDirectory(int argc, wchar_t* argv[])
{
    std::error_code error;
    const std::filesystem::path current_working_directory = std::filesystem::current_path(error);
    error.clear();

    std::filesystem::path executable_directory;
    if (argc > 0 && argv != nullptr && argv[0] != nullptr)
    {
        executable_directory = std::filesystem::absolute(std::filesystem::path(argv[0]), error);
        if (!error)
        {
            executable_directory = executable_directory.parent_path();
        }
        else
        {
            executable_directory.clear();
            error.clear();
        }
    }

    for (const std::filesystem::path& candidate : std::array<std::filesystem::path, 2>{
             current_working_directory,
             executable_directory,
         })
    {
        const std::filesystem::path repository_root =
            TryResolveHostRepositoryRootFromBase(candidate);
        if (!repository_root.empty())
        {
            return repository_root / "logs" / "latest" / "runtime";
        }
    }

    return std::filesystem::path(L"logs/latest/runtime");
}

void ApplyLoggingDefaults(hl::app::LaunchOptions& options, const ParseState& parse_state)
{
    const bool long_run = options.frame_count >= 300;

    if (!parse_state.log_frame_sample_explicit)
    {
        options.log_frame_sample = long_run ? 10 : 1;
    }
    if (!parse_state.log_state_changes_only_explicit)
    {
        options.log_state_changes_only = long_run;
    }
    if (!parse_state.log_summary_file_explicit)
    {
        options.log_summary_file = true;
    }
    if (!parse_state.log_suppress_repeats_explicit)
    {
        options.log_suppress_repeats = true;
    }
    if (!parse_state.log_console_level_explicit)
    {
        options.log_console_level = hl::common::LogLevel::Info;
    }
    if (!parse_state.log_file_level_explicit)
    {
        options.log_file_level = hl::common::LogLevel::Info;
    }

    if (options.verbose)
    {
        options.trace_scripted = true;
        options.trace_movement = true;
        options.trace_think = true;
        options.trace_callbacks = true;
        if (!parse_state.log_console_level_explicit)
        {
            options.log_console_level = hl::common::LogLevel::Debug;
        }
        if (!parse_state.log_file_level_explicit)
        {
            options.log_file_level = hl::common::LogLevel::Debug;
        }
    }

    if (options.trace_scripted
        || options.trace_movement
        || options.trace_think
        || options.trace_callbacks)
    {
        if (!parse_state.log_frame_sample_explicit)
        {
            options.log_frame_sample = 1;
        }
        if (!parse_state.log_state_changes_only_explicit)
        {
            options.log_state_changes_only = false;
        }
    }
}

void ApplyRuntimeDefaults(hl::app::LaunchOptions& options, const ParseState& parse_state)
{
    if (options.runtime_mode == hl::app::RuntimeMode::kDedicated)
    {
        if (!parse_state.deathmatch_explicit)
        {
            options.deathmatch = 1;
        }
        if (!parse_state.coop_explicit)
        {
            options.coop = 0;
        }
        if (!parse_state.maxclients_explicit)
        {
            options.maxclients = 4;
        }
    }

    options.deathmatch = options.deathmatch != 0 ? 1 : 0;
    options.coop = options.coop != 0 ? 1 : 0;
    if (options.maxclients < 1)
    {
        options.maxclients = 1;
    }
    else if (options.maxclients > 32)
    {
        options.maxclients = 32;
    }
}
} // namespace

namespace hl::app
{
LaunchOptionsParseResult ParseLaunchOptions(int argc, wchar_t* argv[])
{
    LaunchOptionsParseResult result;
    ParseState parse_state;

    for (int index = 1; index < argc; ++index)
    {
        const std::wstring_view argument = argv[index];

        if (argument == L"--help" || argument == L"-h")
        {
            result.options.show_help = true;
            continue;
        }

        if (argument == L"--verbose")
        {
            result.options.verbose = true;
            continue;
        }

        if (argument == L"--dedicated" || argument == L"-dedicated")
        {
            result.options.runtime_mode = hl::app::RuntimeMode::kDedicated;
            parse_state.dedicated_explicit = true;
            continue;
        }

        constexpr std::wstring_view dedicated_prefix = L"--dedicated=";
        if (StartsWith(argument, dedicated_prefix))
        {
            bool dedicated = false;
            if (!ParseBoolValue(
                    argument.substr(dedicated_prefix.size()),
                    &dedicated,
                    &result.error_message,
                    L"--dedicated"))
            {
                return result;
            }

            result.options.runtime_mode =
                dedicated ? hl::app::RuntimeMode::kDedicated : hl::app::RuntimeMode::kListen;
            parse_state.dedicated_explicit = true;
            continue;
        }

        if (argument == L"--gamedir")
        {
            if (index + 1 >= argc)
            {
                result.error_message = L"Missing value for --gamedir.";
                return result;
            }

            result.options.game_directory = std::filesystem::path(argv[++index]);
            continue;
        }

        constexpr std::wstring_view game_dir_prefix = L"--gamedir=";
        if (StartsWith(argument, game_dir_prefix))
        {
            const std::wstring_view value = argument.substr(game_dir_prefix.size());
            if (value.empty())
            {
                result.error_message = L"Empty value for --gamedir.";
                return result;
            }

            result.options.game_directory = std::filesystem::path(value);
            continue;
        }

        if (argument == L"--map")
        {
            if (index + 1 >= argc)
            {
                result.error_message = L"Missing value for --map.";
                return result;
            }

            result.options.map_name = std::wstring(argv[++index]);
            continue;
        }

        constexpr std::wstring_view map_prefix = L"--map=";
        if (StartsWith(argument, map_prefix))
        {
            const std::wstring_view value = argument.substr(map_prefix.size());
            if (value.empty())
            {
                result.error_message = L"Empty value for --map.";
                return result;
            }

            result.options.map_name = std::wstring(value);
            continue;
        }

        if (argument == L"--regression-guard")
        {
            if (index + 1 >= argc)
            {
                result.error_message = L"Missing value for --regression-guard.";
                return result;
            }

            hl::app::RegressionGuardProfile profile{};
            if (!TryParseRegressionGuardProfile(argv[++index], &profile))
            {
                result.error_message =
                    L"Invalid value for --regression-guard. Expected trainstop26-terminal-probe, trainstop26-baseline, changelevel-latch-only-continuation, or changelevel-request-consumed.";
                return result;
            }

            result.options.regression_guard = profile;
            continue;
        }

        constexpr std::wstring_view regression_guard_prefix = L"--regression-guard=";
        if (StartsWith(argument, regression_guard_prefix))
        {
            hl::app::RegressionGuardProfile profile{};
            if (!TryParseRegressionGuardProfile(
                    argument.substr(regression_guard_prefix.size()),
                    &profile))
            {
                result.error_message =
                    L"Invalid value for --regression-guard. Expected trainstop26-terminal-probe, trainstop26-baseline, changelevel-latch-only-continuation, or changelevel-request-consumed.";
                return result;
            }

            result.options.regression_guard = profile;
            continue;
        }

        if (argument == L"--run-label")
        {
            if (index + 1 >= argc)
            {
                result.error_message = L"Missing value for --run-label.";
                return result;
            }

            const std::optional<std::string> sanitized = SanitizeRunLabel(argv[++index]);
            if (!sanitized.has_value())
            {
                result.error_message =
                    L"Invalid value for --run-label. Expected at least one filesystem-safe ASCII character.";
                return result;
            }

            result.options.run_label = *sanitized;
            continue;
        }

        constexpr std::wstring_view run_label_prefix = L"--run-label=";
        if (StartsWith(argument, run_label_prefix))
        {
            const std::optional<std::string> sanitized =
                SanitizeRunLabel(argument.substr(run_label_prefix.size()));
            if (!sanitized.has_value())
            {
                result.error_message =
                    L"Invalid value for --run-label. Expected at least one filesystem-safe ASCII character.";
                return result;
            }

            result.options.run_label = *sanitized;
            continue;
        }

        if (argument == L"--prompt-id")
        {
            if (index + 1 >= argc)
            {
                result.error_message = L"Missing value for --prompt-id.";
                return result;
            }

            const std::optional<std::string> prompt_id = ParsePromptId(argv[++index]);
            if (!prompt_id.has_value())
            {
                result.error_message = L"Empty value for --prompt-id.";
                return result;
            }

            result.options.prompt_id = *prompt_id;
            continue;
        }

        constexpr std::wstring_view prompt_id_prefix = L"--prompt-id=";
        if (StartsWith(argument, prompt_id_prefix))
        {
            const std::optional<std::string> prompt_id =
                ParsePromptId(argument.substr(prompt_id_prefix.size()));
            if (!prompt_id.has_value())
            {
                result.error_message = L"Empty value for --prompt-id.";
                return result;
            }

            result.options.prompt_id = *prompt_id;
            continue;
        }

        if (argument == L"--maxclients")
        {
            if (index + 1 >= argc)
            {
                result.error_message = L"Missing value for --maxclients.";
                return result;
            }

            int maxclients = 0;
            if (!TryParseInteger(argv[++index], &maxclients) || maxclients <= 0 || maxclients > 32)
            {
                result.error_message =
                    L"Invalid value for --maxclients. Expected an integer in the range 1..32.";
                return result;
            }

            result.options.maxclients = maxclients;
            parse_state.maxclients_explicit = true;
            continue;
        }

        constexpr std::wstring_view maxclients_prefix = L"--maxclients=";
        if (StartsWith(argument, maxclients_prefix))
        {
            int maxclients = 0;
            if (!TryParseInteger(argument.substr(maxclients_prefix.size()), &maxclients)
                || maxclients <= 0 || maxclients > 32)
            {
                result.error_message =
                    L"Invalid value for --maxclients. Expected an integer in the range 1..32.";
                return result;
            }

            result.options.maxclients = maxclients;
            parse_state.maxclients_explicit = true;
            continue;
        }

        if (argument == L"--deathmatch")
        {
            bool enabled = false;
            if (!ParseRequiredBoolOption(
                    argc,
                    argv,
                    &index,
                    &enabled,
                    &result.error_message,
                    argument))
            {
                return result;
            }

            result.options.deathmatch = enabled ? 1 : 0;
            parse_state.deathmatch_explicit = true;
            continue;
        }

        constexpr std::wstring_view deathmatch_prefix = L"--deathmatch=";
        if (StartsWith(argument, deathmatch_prefix))
        {
            bool enabled = false;
            if (!ParseBoolValue(
                    argument.substr(deathmatch_prefix.size()),
                    &enabled,
                    &result.error_message,
                    L"--deathmatch"))
            {
                return result;
            }

            result.options.deathmatch = enabled ? 1 : 0;
            parse_state.deathmatch_explicit = true;
            continue;
        }

        if (argument == L"--coop")
        {
            bool enabled = false;
            if (!ParseRequiredBoolOption(
                    argc,
                    argv,
                    &index,
                    &enabled,
                    &result.error_message,
                    argument))
            {
                return result;
            }

            result.options.coop = enabled ? 1 : 0;
            parse_state.coop_explicit = true;
            continue;
        }

        constexpr std::wstring_view coop_prefix = L"--coop=";
        if (StartsWith(argument, coop_prefix))
        {
            bool enabled = false;
            if (!ParseBoolValue(
                    argument.substr(coop_prefix.size()),
                    &enabled,
                    &result.error_message,
                    L"--coop"))
            {
                return result;
            }

            result.options.coop = enabled ? 1 : 0;
            parse_state.coop_explicit = true;
            continue;
        }

        if (argument == L"--synthetic-players")
        {
            if (index + 1 >= argc)
            {
                result.error_message = L"Missing value for --synthetic-players.";
                return result;
            }

            int synthetic_players = 0;
            if (!TryParseInteger(argv[++index], &synthetic_players)
                || (synthetic_players != 0 && synthetic_players != 2))
            {
                result.error_message =
                    L"Invalid value for --synthetic-players. Expected 0 or 2.";
                return result;
            }

            result.options.synthetic_players = synthetic_players;
            continue;
        }

        constexpr std::wstring_view synthetic_players_prefix = L"--synthetic-players=";
        if (StartsWith(argument, synthetic_players_prefix))
        {
            int synthetic_players = 0;
            if (!TryParseInteger(
                    argument.substr(synthetic_players_prefix.size()),
                    &synthetic_players)
                || (synthetic_players != 0 && synthetic_players != 2))
            {
                result.error_message =
                    L"Invalid value for --synthetic-players. Expected 0 or 2.";
                return result;
            }

            result.options.synthetic_players = synthetic_players;
            continue;
        }

        if (argument == L"--query-surface")
        {
            result.options.query_surface_enabled = true;
            continue;
        }

        constexpr std::wstring_view query_surface_prefix = L"--query-surface=";
        if (StartsWith(argument, query_surface_prefix))
        {
            if (!ParseBoolValue(
                    argument.substr(query_surface_prefix.size()),
                    &result.options.query_surface_enabled,
                    &result.error_message,
                    L"--query-surface"))
            {
                return result;
            }
            continue;
        }

        if (argument == L"--query-probe")
        {
            result.options.query_probe_enabled = true;
            continue;
        }

        constexpr std::wstring_view query_probe_prefix = L"--query-probe=";
        if (StartsWith(argument, query_probe_prefix))
        {
            if (!ParseBoolValue(
                    argument.substr(query_probe_prefix.size()),
                    &result.options.query_probe_enabled,
                    &result.error_message,
                    L"--query-probe"))
            {
                return result;
            }
            continue;
        }

        if (argument == L"--query-port")
        {
            if (index + 1 >= argc)
            {
                result.error_message = L"Missing value for --query-port.";
                return result;
            }

            int query_port = 0;
            if (!TryParseInteger(argv[++index], &query_port) || query_port < 0 || query_port > 65535)
            {
                result.error_message =
                    L"Invalid value for --query-port. Expected an integer in the range 0..65535.";
                return result;
            }

            result.options.query_port = query_port;
            continue;
        }

        constexpr std::wstring_view query_port_prefix = L"--query-port=";
        if (StartsWith(argument, query_port_prefix))
        {
            int query_port = 0;
            if (!TryParseInteger(argument.substr(query_port_prefix.size()), &query_port)
                || query_port < 0 || query_port > 65535)
            {
                result.error_message =
                    L"Invalid value for --query-port. Expected an integer in the range 0..65535.";
                return result;
            }

            result.options.query_port = query_port;
            continue;
        }

        if (argument == L"--frames")
        {
            if (index + 1 >= argc)
            {
                result.error_message = L"Missing value for --frames.";
                return result;
            }

            int frames = 0;
            if (!TryParseInteger(argv[++index], &frames) || frames < 0)
            {
                result.error_message = L"Invalid value for --frames. Expected a non-negative integer.";
                return result;
            }

            result.options.frame_count = frames;
            continue;
        }

        constexpr std::wstring_view frames_prefix = L"--frames=";
        if (StartsWith(argument, frames_prefix))
        {
            const std::wstring_view value = argument.substr(frames_prefix.size());
            int frames = 0;
            if (!TryParseInteger(value, &frames) || frames < 0)
            {
                result.error_message = L"Invalid value for --frames. Expected a non-negative integer.";
                return result;
            }

            result.options.frame_count = frames;
            continue;
        }

        if (argument == L"--frametime")
        {
            if (index + 1 >= argc)
            {
                result.error_message = L"Missing value for --frametime.";
                return result;
            }

            float frametime = 0.0f;
            if (!TryParseFloat(argv[++index], &frametime) || frametime <= 0.0f)
            {
                result.error_message = L"Invalid value for --frametime. Expected a positive number.";
                return result;
            }

            result.options.frame_time = frametime;
            continue;
        }

        constexpr std::wstring_view frametime_prefix = L"--frametime=";
        if (StartsWith(argument, frametime_prefix))
        {
            const std::wstring_view value = argument.substr(frametime_prefix.size());
            float frametime = 0.0f;
            if (!TryParseFloat(value, &frametime) || frametime <= 0.0f)
            {
                result.error_message = L"Invalid value for --frametime. Expected a positive number.";
                return result;
            }

            result.options.frame_time = frametime;
            continue;
        }

        if (argument == L"--think-limit")
        {
            if (index + 1 >= argc)
            {
                result.error_message = L"Missing value for --think-limit.";
                return result;
            }

            int think_limit = 0;
            if (!TryParseInteger(argv[++index], &think_limit) || think_limit <= 0)
            {
                result.error_message = L"Invalid value for --think-limit. Expected a positive integer.";
                return result;
            }

            result.options.think_limit = think_limit;
            continue;
        }

        constexpr std::wstring_view think_limit_prefix = L"--think-limit=";
        if (StartsWith(argument, think_limit_prefix))
        {
            const std::wstring_view value = argument.substr(think_limit_prefix.size());
            int think_limit = 0;
            if (!TryParseInteger(value, &think_limit) || think_limit <= 0)
            {
                result.error_message = L"Invalid value for --think-limit. Expected a positive integer.";
                return result;
            }

            result.options.think_limit = think_limit;
            continue;
        }

        if (argument == L"--use-limit")
        {
            if (index + 1 >= argc)
            {
                result.error_message = L"Missing value for --use-limit.";
                return result;
            }

            int use_limit = 0;
            if (!TryParseInteger(argv[++index], &use_limit) || use_limit <= 0)
            {
                result.error_message = L"Invalid value for --use-limit. Expected a positive integer.";
                return result;
            }

            result.options.use_limit = use_limit;
            continue;
        }

        constexpr std::wstring_view use_limit_prefix = L"--use-limit=";
        if (StartsWith(argument, use_limit_prefix))
        {
            const std::wstring_view value = argument.substr(use_limit_prefix.size());
            int use_limit = 0;
            if (!TryParseInteger(value, &use_limit) || use_limit <= 0)
            {
                result.error_message = L"Invalid value for --use-limit. Expected a positive integer.";
                return result;
            }

            result.options.use_limit = use_limit;
            continue;
        }

        if (argument == L"--scheduled-use-limit")
        {
            if (index + 1 >= argc)
            {
                result.error_message = L"Missing value for --scheduled-use-limit.";
                return result;
            }

            int scheduled_use_limit = 0;
            if (!TryParseInteger(argv[++index], &scheduled_use_limit) || scheduled_use_limit <= 0)
            {
                result.error_message = L"Invalid value for --scheduled-use-limit. Expected a positive integer.";
                return result;
            }

            result.options.scheduled_use_limit = scheduled_use_limit;
            continue;
        }

        constexpr std::wstring_view scheduled_use_limit_prefix = L"--scheduled-use-limit=";
        if (StartsWith(argument, scheduled_use_limit_prefix))
        {
            const std::wstring_view value = argument.substr(scheduled_use_limit_prefix.size());
            int scheduled_use_limit = 0;
            if (!TryParseInteger(value, &scheduled_use_limit) || scheduled_use_limit <= 0)
            {
                result.error_message = L"Invalid value for --scheduled-use-limit. Expected a positive integer.";
                return result;
            }

            result.options.scheduled_use_limit = scheduled_use_limit;
            continue;
        }

        if (argument == L"--path-arrival-epsilon")
        {
            if (index + 1 >= argc)
            {
                result.error_message = L"Missing value for --path-arrival-epsilon.";
                return result;
            }

            float arrival_epsilon = 0.0f;
            if (!TryParseFloat(argv[++index], &arrival_epsilon) || arrival_epsilon <= 0.0f)
            {
                result.error_message = L"Invalid value for --path-arrival-epsilon. Expected a positive number.";
                return result;
            }

            result.options.path_arrival_epsilon = arrival_epsilon;
            continue;
        }

        constexpr std::wstring_view path_arrival_epsilon_prefix = L"--path-arrival-epsilon=";
        if (StartsWith(argument, path_arrival_epsilon_prefix))
        {
            const std::wstring_view value = argument.substr(path_arrival_epsilon_prefix.size());
            float arrival_epsilon = 0.0f;
            if (!TryParseFloat(value, &arrival_epsilon) || arrival_epsilon <= 0.0f)
            {
                result.error_message = L"Invalid value for --path-arrival-epsilon. Expected a positive number.";
                return result;
            }

            result.options.path_arrival_epsilon = arrival_epsilon;
            continue;
        }

        if (argument == L"--trace-scripted" || argument == L"--log-scripted-trace")
        {
            if (!ParseRequiredBoolOption(
                    argc,
                    argv,
                    &index,
                    &result.options.trace_scripted,
                    &result.error_message,
                    argument))
            {
                return result;
            }
            continue;
        }

        constexpr std::wstring_view trace_scripted_prefix = L"--trace-scripted=";
        constexpr std::wstring_view log_scripted_trace_prefix = L"--log-scripted-trace=";
        if (StartsWith(argument, trace_scripted_prefix)
            || StartsWith(argument, log_scripted_trace_prefix))
        {
            const std::wstring_view prefix = StartsWith(argument, trace_scripted_prefix)
                ? trace_scripted_prefix
                : log_scripted_trace_prefix;
            if (!ParseBoolValue(
                    argument.substr(prefix.size()),
                    &result.options.trace_scripted,
                    &result.error_message,
                    prefix.substr(0, prefix.size() - 1)))
            {
                return result;
            }
            continue;
        }

        if (argument == L"--movement-trace" || argument == L"--path-trace"
            || argument == L"--trace-path" || argument == L"--log-path-trace")
        {
            if (!ParseRequiredBoolOption(
                    argc,
                    argv,
                    &index,
                    &result.options.trace_movement,
                    &result.error_message,
                    argument))
            {
                return result;
            }
            continue;
        }

        constexpr std::wstring_view movement_trace_prefix = L"--movement-trace=";
        constexpr std::wstring_view path_trace_prefix = L"--path-trace=";
        constexpr std::wstring_view trace_path_prefix = L"--trace-path=";
        constexpr std::wstring_view log_path_trace_prefix = L"--log-path-trace=";
        if (StartsWith(argument, movement_trace_prefix)
            || StartsWith(argument, path_trace_prefix)
            || StartsWith(argument, trace_path_prefix)
            || StartsWith(argument, log_path_trace_prefix))
        {
            std::wstring_view prefix = movement_trace_prefix;
            if (StartsWith(argument, path_trace_prefix))
            {
                prefix = path_trace_prefix;
            }
            else if (StartsWith(argument, trace_path_prefix))
            {
                prefix = trace_path_prefix;
            }
            else if (StartsWith(argument, log_path_trace_prefix))
            {
                prefix = log_path_trace_prefix;
            }

            if (!ParseBoolValue(
                    argument.substr(prefix.size()),
                    &result.options.trace_movement,
                    &result.error_message,
                    prefix.substr(0, prefix.size() - 1)))
            {
                return result;
            }
            continue;
        }

        if (argument == L"--trace-think")
        {
            if (!ParseRequiredBoolOption(
                    argc,
                    argv,
                    &index,
                    &result.options.trace_think,
                    &result.error_message,
                    argument))
            {
                return result;
            }
            continue;
        }

        constexpr std::wstring_view trace_think_prefix = L"--trace-think=";
        if (StartsWith(argument, trace_think_prefix))
        {
            if (!ParseBoolValue(
                    argument.substr(trace_think_prefix.size()),
                    &result.options.trace_think,
                    &result.error_message,
                    L"--trace-think"))
            {
                return result;
            }
            continue;
        }

        if (argument == L"--trace-callbacks")
        {
            if (!ParseRequiredBoolOption(
                    argc,
                    argv,
                    &index,
                    &result.options.trace_callbacks,
                    &result.error_message,
                    argument))
            {
                return result;
            }
            continue;
        }

        constexpr std::wstring_view trace_callbacks_prefix = L"--trace-callbacks=";
        if (StartsWith(argument, trace_callbacks_prefix))
        {
            if (!ParseBoolValue(
                    argument.substr(trace_callbacks_prefix.size()),
                    &result.options.trace_callbacks,
                    &result.error_message,
                    L"--trace-callbacks"))
            {
                return result;
            }
            continue;
        }

        if (argument == L"--stop-on-first-message")
        {
            if (!ParseRequiredBoolOption(
                    argc,
                    argv,
                    &index,
                    &result.options.stop_on_first_message,
                    &result.error_message,
                    argument))
            {
                return result;
            }
            continue;
        }

        constexpr std::wstring_view stop_on_first_message_prefix = L"--stop-on-first-message=";
        if (StartsWith(argument, stop_on_first_message_prefix))
        {
            if (!ParseBoolValue(
                    argument.substr(stop_on_first_message_prefix.size()),
                    &result.options.stop_on_first_message,
                    &result.error_message,
                    L"--stop-on-first-message"))
            {
                return result;
            }
            continue;
        }

        if (argument == L"--stop-on-changelevel-request")
        {
            if (index + 1 >= argc)
            {
                result.error_message = L"Missing value for --stop-on-changelevel-request.";
                return result;
            }

            if (!ParseBoolValue(
                    argv[++index],
                    &result.options.stop_on_changelevel_request,
                    &result.error_message,
                    L"--stop-on-changelevel-request"))
            {
                return result;
            }
            continue;
        }

        constexpr std::wstring_view stop_on_changelevel_request_prefix =
            L"--stop-on-changelevel-request=";
        if (StartsWith(argument, stop_on_changelevel_request_prefix))
        {
            if (!ParseBoolValue(
                    argument.substr(stop_on_changelevel_request_prefix.size()),
                    &result.options.stop_on_changelevel_request,
                    &result.error_message,
                    L"--stop-on-changelevel-request"))
            {
                return result;
            }
            continue;
        }

        if (argument == L"--stop-on-node")
        {
            if (index + 1 >= argc)
            {
                result.error_message = L"Missing value for --stop-on-node.";
                return result;
            }

            const std::wstring value = argv[++index];
            if (value.empty())
            {
                result.error_message = L"Empty value for --stop-on-node.";
                return result;
            }

            result.options.stop_on_node = value;
            continue;
        }

        constexpr std::wstring_view stop_on_node_prefix = L"--stop-on-node=";
        if (StartsWith(argument, stop_on_node_prefix))
        {
            const std::wstring_view value = argument.substr(stop_on_node_prefix.size());
            if (value.empty())
            {
                result.error_message = L"Empty value for --stop-on-node.";
                return result;
            }

            result.options.stop_on_node = std::wstring(value);
            continue;
        }

        if (argument == L"--log-dir")
        {
            if (index + 1 >= argc)
            {
                result.error_message = L"Missing value for --log-dir.";
                return result;
            }

            result.options.log_directory = std::filesystem::path(argv[++index]);
            parse_state.log_directory_explicit = true;
            result.options.log_directory_explicit = true;
            continue;
        }

        constexpr std::wstring_view log_dir_prefix = L"--log-dir=";
        if (StartsWith(argument, log_dir_prefix))
        {
            const std::wstring_view value = argument.substr(log_dir_prefix.size());
            if (value.empty())
            {
                result.error_message = L"Empty value for --log-dir.";
                return result;
            }

            result.options.log_directory = std::filesystem::path(value);
            parse_state.log_directory_explicit = true;
            result.options.log_directory_explicit = true;
            continue;
        }

        if (argument == L"--log-to-file")
        {
            if (!ParseRequiredBoolOption(
                    argc,
                    argv,
                    &index,
                    &result.options.log_to_file,
                    &result.error_message,
                    argument))
            {
                return result;
            }
            continue;
        }

        constexpr std::wstring_view log_to_file_prefix = L"--log-to-file=";
        if (StartsWith(argument, log_to_file_prefix))
        {
            if (!ParseBoolValue(
                    argument.substr(log_to_file_prefix.size()),
                    &result.options.log_to_file,
                    &result.error_message,
                    L"--log-to-file"))
            {
                return result;
            }
            continue;
        }

        if (argument == L"--log-max-mb")
        {
            if (index + 1 >= argc)
            {
                result.error_message = L"Missing value for --log-max-mb.";
                return result;
            }

            int log_max_mb = 0;
            if (!TryParseInteger(argv[++index], &log_max_mb) || log_max_mb <= 0)
            {
                result.error_message = L"Invalid value for --log-max-mb. Expected a positive integer.";
                return result;
            }

            result.options.log_max_mb = log_max_mb;
            continue;
        }

        constexpr std::wstring_view log_max_mb_prefix = L"--log-max-mb=";
        if (StartsWith(argument, log_max_mb_prefix))
        {
            const std::wstring_view value = argument.substr(log_max_mb_prefix.size());
            int log_max_mb = 0;
            if (!TryParseInteger(value, &log_max_mb) || log_max_mb <= 0)
            {
                result.error_message = L"Invalid value for --log-max-mb. Expected a positive integer.";
                return result;
            }

            result.options.log_max_mb = log_max_mb;
            continue;
        }

        if (argument == L"--log-level")
        {
            if (index + 1 >= argc)
            {
                result.error_message = L"Missing value for --log-level.";
                return result;
            }

            hl::common::LogLevel level = hl::common::LogLevel::Info;
            if (!TryParseLogLevel(argv[++index], &level))
            {
                result.error_message =
                    L"Invalid value for --log-level. Expected error, warn, info, debug, or trace.";
                return result;
            }

            result.options.log_level = level;
            parse_state.log_level_explicit = true;
            continue;
        }

        constexpr std::wstring_view log_level_prefix = L"--log-level=";
        if (StartsWith(argument, log_level_prefix))
        {
            hl::common::LogLevel level = hl::common::LogLevel::Info;
            if (!TryParseLogLevel(argument.substr(log_level_prefix.size()), &level))
            {
                result.error_message =
                    L"Invalid value for --log-level. Expected error, warn, info, debug, or trace.";
                return result;
            }

            result.options.log_level = level;
            parse_state.log_level_explicit = true;
            continue;
        }

        if (argument == L"--log-console-level")
        {
            if (index + 1 >= argc)
            {
                result.error_message = L"Missing value for --log-console-level.";
                return result;
            }

            hl::common::LogLevel level = hl::common::LogLevel::Info;
            if (!TryParseLogLevel(argv[++index], &level))
            {
                result.error_message =
                    L"Invalid value for --log-console-level. Expected error, warn, info, debug, or trace.";
                return result;
            }

            result.options.log_console_level = level;
            parse_state.log_console_level_explicit = true;
            continue;
        }

        constexpr std::wstring_view log_console_level_prefix = L"--log-console-level=";
        if (StartsWith(argument, log_console_level_prefix))
        {
            hl::common::LogLevel level = hl::common::LogLevel::Info;
            if (!TryParseLogLevel(argument.substr(log_console_level_prefix.size()), &level))
            {
                result.error_message =
                    L"Invalid value for --log-console-level. Expected error, warn, info, debug, or trace.";
                return result;
            }

            result.options.log_console_level = level;
            parse_state.log_console_level_explicit = true;
            continue;
        }

        if (argument == L"--log-file-level")
        {
            if (index + 1 >= argc)
            {
                result.error_message = L"Missing value for --log-file-level.";
                return result;
            }

            hl::common::LogLevel level = hl::common::LogLevel::Info;
            if (!TryParseLogLevel(argv[++index], &level))
            {
                result.error_message =
                    L"Invalid value for --log-file-level. Expected error, warn, info, debug, or trace.";
                return result;
            }

            result.options.log_file_level = level;
            parse_state.log_file_level_explicit = true;
            continue;
        }

        constexpr std::wstring_view log_file_level_prefix = L"--log-file-level=";
        if (StartsWith(argument, log_file_level_prefix))
        {
            hl::common::LogLevel level = hl::common::LogLevel::Info;
            if (!TryParseLogLevel(argument.substr(log_file_level_prefix.size()), &level))
            {
                result.error_message =
                    L"Invalid value for --log-file-level. Expected error, warn, info, debug, or trace.";
                return result;
            }

            result.options.log_file_level = level;
            parse_state.log_file_level_explicit = true;
            continue;
        }

        if (argument == L"--log-categories")
        {
            if (index + 1 >= argc)
            {
                result.error_message = L"Missing value for --log-categories.";
                return result;
            }

            if (!TryParseCategoryList(
                    argv[++index],
                    &result.options.log_categories,
                    &result.error_message))
            {
                return result;
            }
            continue;
        }

        constexpr std::wstring_view log_categories_prefix = L"--log-categories=";
        if (StartsWith(argument, log_categories_prefix))
        {
            if (!TryParseCategoryList(
                    argument.substr(log_categories_prefix.size()),
                    &result.options.log_categories,
                    &result.error_message))
            {
                return result;
            }
            continue;
        }

        if (argument == L"--log-disable-categories")
        {
            if (index + 1 >= argc)
            {
                result.error_message = L"Missing value for --log-disable-categories.";
                return result;
            }

            if (!TryParseCategoryList(
                    argv[++index],
                    &result.options.log_disable_categories,
                    &result.error_message))
            {
                return result;
            }
            continue;
        }

        constexpr std::wstring_view log_disable_categories_prefix = L"--log-disable-categories=";
        if (StartsWith(argument, log_disable_categories_prefix))
        {
            if (!TryParseCategoryList(
                    argument.substr(log_disable_categories_prefix.size()),
                    &result.options.log_disable_categories,
                    &result.error_message))
            {
                return result;
            }
            continue;
        }

        if (argument == L"--log-category-files")
        {
            if (index + 1 >= argc)
            {
                result.error_message = L"Missing value for --log-category-files.";
                return result;
            }

            if (!TryParseCategoryList(
                    argv[++index],
                    &result.options.log_category_files,
                    &result.error_message))
            {
                return result;
            }
            continue;
        }

        constexpr std::wstring_view log_category_files_prefix = L"--log-category-files=";
        if (StartsWith(argument, log_category_files_prefix))
        {
            if (!TryParseCategoryList(
                    argument.substr(log_category_files_prefix.size()),
                    &result.options.log_category_files,
                    &result.error_message))
            {
                return result;
            }
            continue;
        }

        if (argument == L"--log-frame-sample")
        {
            if (index + 1 >= argc)
            {
                result.error_message = L"Missing value for --log-frame-sample.";
                return result;
            }

            int sample = 0;
            if (!TryParseInteger(argv[++index], &sample) || sample <= 0)
            {
                result.error_message = L"Invalid value for --log-frame-sample. Expected a positive integer.";
                return result;
            }

            result.options.log_frame_sample = sample;
            parse_state.log_frame_sample_explicit = true;
            continue;
        }

        constexpr std::wstring_view log_frame_sample_prefix = L"--log-frame-sample=";
        if (StartsWith(argument, log_frame_sample_prefix))
        {
            int sample = 0;
            if (!TryParseInteger(argument.substr(log_frame_sample_prefix.size()), &sample)
                || sample <= 0)
            {
                result.error_message = L"Invalid value for --log-frame-sample. Expected a positive integer.";
                return result;
            }

            result.options.log_frame_sample = sample;
            parse_state.log_frame_sample_explicit = true;
            continue;
        }

        if (argument == L"--log-state-changes-only")
        {
            if (!ParseRequiredBoolOption(
                    argc,
                    argv,
                    &index,
                    &result.options.log_state_changes_only,
                    &result.error_message,
                    argument))
            {
                return result;
            }
            parse_state.log_state_changes_only_explicit = true;
            continue;
        }

        constexpr std::wstring_view log_state_changes_only_prefix = L"--log-state-changes-only=";
        if (StartsWith(argument, log_state_changes_only_prefix))
        {
            if (!ParseBoolValue(
                    argument.substr(log_state_changes_only_prefix.size()),
                    &result.options.log_state_changes_only,
                    &result.error_message,
                    L"--log-state-changes-only"))
            {
                return result;
            }
            parse_state.log_state_changes_only_explicit = true;
            continue;
        }

        if (argument == L"--log-summary-file")
        {
            if (!ParseRequiredBoolOption(
                    argc,
                    argv,
                    &index,
                    &result.options.log_summary_file,
                    &result.error_message,
                    argument))
            {
                return result;
            }
            parse_state.log_summary_file_explicit = true;
            continue;
        }

        constexpr std::wstring_view log_summary_file_prefix = L"--log-summary-file=";
        if (StartsWith(argument, log_summary_file_prefix))
        {
            if (!ParseBoolValue(
                    argument.substr(log_summary_file_prefix.size()),
                    &result.options.log_summary_file,
                    &result.error_message,
                    L"--log-summary-file"))
            {
                return result;
            }
            parse_state.log_summary_file_explicit = true;
            continue;
        }

        if (argument == L"--log-suppress-repeats")
        {
            if (!ParseRequiredBoolOption(
                    argc,
                    argv,
                    &index,
                    &result.options.log_suppress_repeats,
                    &result.error_message,
                    argument))
            {
                return result;
            }
            parse_state.log_suppress_repeats_explicit = true;
            continue;
        }

        constexpr std::wstring_view log_suppress_repeats_prefix = L"--log-suppress-repeats=";
        if (StartsWith(argument, log_suppress_repeats_prefix))
        {
            if (!ParseBoolValue(
                    argument.substr(log_suppress_repeats_prefix.size()),
                    &result.options.log_suppress_repeats,
                    &result.error_message,
                    L"--log-suppress-repeats"))
            {
                return result;
            }
            parse_state.log_suppress_repeats_explicit = true;
            continue;
        }

        result.error_message = L"Unknown command line option: " + std::wstring(argument);
        return result;
    }

    ApplyRuntimeDefaults(result.options, parse_state);
    if (result.options.query_probe_enabled)
    {
        result.options.query_surface_enabled = true;
    }
    ApplyLoggingDefaults(result.options, parse_state);
    if (!parse_state.log_directory_explicit)
    {
        result.options.log_directory = ResolveDefaultLogDirectory(argc, argv);
    }
    return result;
}

std::wstring BuildUsageText(const std::filesystem::path& executable_path)
{
    const std::wstring executable_name =
        executable_path.filename().empty() ? L"hlhost.exe" : executable_path.filename().wstring();

    return L"Usage:\n"
           L"  "
           + executable_name
           + L" [--dedicated] [--gamedir <path>] [--map <name>] [--deathmatch <0|1>] [--coop <0|1>] [--maxclients <count>] [--synthetic-players <0|2>] [--query-surface] [--query-probe] [--query-port <0..65535>] [--regression-guard <profile>] [--run-label <label>] [--prompt-id <id>] [--frames <count>] [--frametime <seconds>] [--think-limit <count>] [--use-limit <count>] [--scheduled-use-limit <count>] [--path-arrival-epsilon <distance>]\n"
             L"    [--trace-scripted <0|1>] [--trace-path <0|1>] [--trace-think <0|1>] [--trace-callbacks <0|1>] [--verbose]\n"
             L"    [--log-dir <path>] [--log-to-file <0|1>] [--log-max-mb <n>] [--log-level <level>]\n"
             L"    [--log-console-level <level>] [--log-file-level <level>] [--log-categories <csv>]\n"
             L"    [--log-disable-categories <csv>] [--log-category-files <csv>] [--log-frame-sample <n>]\n"
             L"    [--log-state-changes-only <0|1>] [--log-summary-file <0|1>] [--log-suppress-repeats <0|1>]\n"
             L"    [--stop-on-first-message <0|1>] [--stop-on-changelevel-request <0|1>] [--stop-on-node <name>]\n\n"
             L"Options:\n"
             L"  --dedicated                  Enable dedicated HLDM-oriented runtime defaults (deathmatch=1, coop=0, maxclients=4 unless overridden)\n"
             L"  --gamedir <path>               Use an explicit Half-Life game directory (typically ...\\valve)\n"
             L"  --map <name>                   Set the bootstrap map name (default: c0a0)\n"
             L"  --deathmatch <0|1>            Override the host-side deathmatch cvar seed\n"
             L"  --coop <0|1>                  Override the host-side coop cvar seed\n"
             L"  --maxclients <n>              Set the authoritative reserved client slot count (default: 1, dedicated default: 4)\n"
             L"  --synthetic-players <0|2>     Run the bounded host-only dedicated lifecycle harness with 0 or 2 synthetic players\n"
             L"  --query-surface               Enable the bounded loopback dedicated UDP info-query surface\n"
             L"  --query-probe                 Run one deterministic loopback info-query probe against the enabled query surface\n"
             L"  --query-port <0..65535>       Requested loopback query UDP port (0 requests a dynamic OS-assigned port)\n"
             L"  --regression-guard <profile>   Run a narrow acceptance guard after summary capture\n"
             L"                                 Profiles: trainstop26-terminal-probe, trainstop26-baseline, changelevel-latch-only-continuation, changelevel-request-consumed\n"
             L"  --run-label <label>            Optional Codex trace label; sanitized for filesystem-safe log and manifest names\n"
             L"  --prompt-id <id>               Optional current prompt id for runtime identity and manifest traceability\n"
             L"  --frames <count>               Run a finite deterministic post-activation frame loop (default: 1000)\n"
             L"  --frametime <s>                Fixed frame time for the bootstrap loop (default: 0.05)\n"
             L"  --think-limit <n>              Maximum due thinks executed per frame (default: 32)\n"
             L"  --use-limit <n>                Maximum use dispatch attempts per frame (default: 64)\n"
             L"  --scheduled-use-limit <n>      Maximum queued map-logic actions processed per frame (default: 128)\n"
             L"  --path-arrival-epsilon <d>     Path-node arrival tolerance for func_tracktrain traversal (default: 24)\n"
             L"  --trace-scripted <0|1>         Enable explicit verbose scripted diagnostics\n"
             L"  --trace-path <0|1>             Enable explicit verbose path/movement diagnostics\n"
             L"  --trace-think <0|1>            Enable explicit verbose think-scheduler diagnostics\n"
             L"  --trace-callbacks <0|1>        Enable explicit verbose per-callback runtime diagnostics\n"
             L"  --movement-trace <0|1>         Alias for --trace-path\n"
             L"  --path-trace <0|1>             Alias for --trace-path\n"
             L"  --log-scripted-trace <0|1>     Alias for --trace-scripted\n"
             L"  --log-path-trace <0|1>         Alias for --trace-path\n"
             L"  --verbose                      Turn on broader debug-oriented logging defaults\n"
             L"  --log-dir <path>               Directory for session log files (default: <repo>\\logs\\latest\\runtime when detected, otherwise logs\\latest\\runtime)\n"
             L"  --log-to-file <0|1>            Enable or disable main file logging (default: 1)\n"
             L"  --log-max-mb <n>               Rotate main file log after n megabytes (default: 10)\n"
             L"  --log-level <level>            Global minimum level across sinks\n"
             L"  --log-console-level <level>    Console minimum level (error|warn|info|debug|trace)\n"
             L"  --log-file-level <level>       Main file minimum level (error|warn|info|debug|trace)\n"
             L"  --log-categories <csv>         Allow only selected info/debug/trace categories\n"
             L"  --log-disable-categories <csv> Suppress selected info/debug/trace categories\n"
             L"  --log-category-files <csv>     Mirror selected categories into separate rotating files\n"
             L"  --log-frame-sample <n>         Log noisy frame summaries every n-th frame unless important\n"
             L"  --log-state-changes-only <0|1> Emit noisy movement/path/scripted summaries only on state changes\n"
             L"  --log-summary-file <0|1>       Enable compact summary log output (default: 1)\n"
             L"  --log-suppress-repeats <0|1>   Collapse repeated consecutive log lines (default: 1)\n"
             L"  --stop-on-first-message <0|1>  Stop cleanly after the first message-bearing path node (default: 0)\n"
             L"  --stop-on-changelevel-request <0|1>\n"
             L"                                 Stop cleanly after a staged-safe changelevel intent is consumed (default: 0)\n"
             L"  --stop-on-node <name>          Stop cleanly after reaching a specific path node\n"
             L"  --help                         Show this help message\n";
}
} // namespace hl::app
