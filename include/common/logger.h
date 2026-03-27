#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace hl::common
{
enum class LogLevel
{
    Trace,
    Debug,
    Info,
    Warn,
    Error
};

enum class LogCategory
{
    General,
    Startup,
    Filesystem,
    Dll,
    Server,
    World,
    Entity,
    Think,
    Use,
    Scripted,
    Path,
    PathEvent,
    Summary,
    Perf
};

struct LoggerOptions
{
    std::filesystem::path log_directory = std::filesystem::path("logs");
    std::string session_prefix = "hlhost";
    bool log_to_console = true;
    bool log_to_file = true;
    bool log_summary_file = true;
    std::uintmax_t max_file_size_bytes = 10u * 1024u * 1024u;
    std::optional<LogLevel> global_level;
    LogLevel console_level = LogLevel::Info;
    LogLevel file_level = LogLevel::Info;
    LogLevel summary_level = LogLevel::Info;
    bool suppress_repeats = true;
    std::vector<LogCategory> enabled_categories;
    std::vector<LogCategory> disabled_categories;
    std::vector<LogCategory> category_file_sinks;
};

struct LoggerSessionInfo
{
    bool configured = false;
    std::filesystem::path log_directory;
    std::string session_id;
    std::uintmax_t max_file_size_bytes = 0;
    std::filesystem::path current_file_path;
    std::filesystem::path summary_file_path;
    std::vector<std::filesystem::path> log_files;
    std::vector<std::filesystem::path> category_log_files;
};

struct LoggerStatistics
{
    std::size_t total_log_files_created = 0;
    std::uintmax_t total_bytes_written = 0;
    std::uintmax_t suppressed_repeat_count = 0;
    std::vector<std::filesystem::path> final_log_paths;
};

std::string_view ToString(LogLevel level) noexcept;
std::string_view ToString(LogCategory category) noexcept;
std::optional<LogLevel> ParseLogLevel(std::string_view text);
std::optional<LogCategory> ParseLogCategory(std::string_view text);

class Logger
{
public:
    static void Configure(const LoggerOptions& options);
    static void Flush();
    static void Shutdown();

    static bool IsConfigured();
    static LoggerSessionInfo SessionInfo();
    static LoggerStatistics Statistics();

    static bool ShouldLog(LogCategory category, LogLevel level);
    static bool ShouldLogFrame(
        int frame_number,
        int sample,
        bool state_changed = false,
        bool important_event = false);
    static bool LogFrameSampled(
        LogCategory category,
        LogLevel level,
        int frame_number,
        int sample,
        std::string_view message,
        bool state_changed = false,
        bool important_event = false);
    static bool LogStateChange(
        LogCategory category,
        LogLevel level,
        std::string_view key,
        std::string_view state_value,
        std::string_view message);

    static void Log(LogCategory category, LogLevel level, std::string_view message);
    static void Log(LogCategory category, LogLevel level, std::wstring_view message);
    static void Logf(LogCategory category, LogLevel level, const char* format, ...);

    static void Trace(LogCategory category, std::string_view message);
    static void Trace(LogCategory category, std::wstring_view message);
    static void Debug(LogCategory category, std::string_view message);
    static void Debug(LogCategory category, std::wstring_view message);
    static void Info(LogCategory category, std::string_view message);
    static void Info(LogCategory category, std::wstring_view message);
    static void Warn(LogCategory category, std::string_view message);
    static void Warn(LogCategory category, std::wstring_view message);
    static void Error(LogCategory category, std::string_view message);
    static void Error(LogCategory category, std::wstring_view message);

    static void Info(std::string_view message);
    static void Info(std::wstring_view message);

    static void Warn(std::string_view message);
    static void Warn(std::wstring_view message);

    static void Error(std::string_view message);
    static void Error(std::wstring_view message);

private:
    static void Write(LogCategory category, LogLevel level, std::string_view message);
};
} // namespace hl::common
