#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "common/logger.h"

namespace hl::app
{
enum class RegressionGuardProfile
{
    kTrainstop26TerminalProbe,
    kTrainstop26Baseline,
    kChangelevelLatchOnlyContinuation,
    kChangelevelRequestConsumed,
};

struct LaunchOptions
{
    std::optional<std::filesystem::path> game_directory;
    std::optional<std::wstring> map_name;
    std::optional<RegressionGuardProfile> regression_guard;
    std::optional<std::string> run_label;
    int frame_count = 1000;
    float frame_time = 0.05f;
    int think_limit = 32;
    int use_limit = 64;
    int scheduled_use_limit = 128;
    float path_arrival_epsilon = 24.0f;
    bool trace_scripted = false;
    bool trace_movement = false;
    bool trace_think = false;
    bool trace_callbacks = false;
    bool verbose = false;
    bool stop_on_first_message = false;
    bool stop_on_changelevel_request = false;
    std::optional<std::wstring> stop_on_node;
    std::filesystem::path log_directory = std::filesystem::path(L"logs/latest/runtime");
    bool log_directory_explicit = false;
    bool log_to_file = true;
    int log_max_mb = 10;
    std::optional<common::LogLevel> log_level;
    common::LogLevel log_console_level = common::LogLevel::Info;
    common::LogLevel log_file_level = common::LogLevel::Info;
    std::vector<common::LogCategory> log_categories;
    std::vector<common::LogCategory> log_disable_categories;
    std::vector<common::LogCategory> log_category_files;
    int log_frame_sample = 10;
    bool log_state_changes_only = true;
    bool log_summary_file = true;
    bool log_suppress_repeats = true;
    bool show_help = false;
};

struct LaunchOptionsParseResult
{
    LaunchOptions options;
    std::optional<std::wstring> error_message;
};

LaunchOptionsParseResult ParseLaunchOptions(int argc, wchar_t* argv[]);
std::wstring BuildUsageText(const std::filesystem::path& executable_path);
} // namespace hl::app
