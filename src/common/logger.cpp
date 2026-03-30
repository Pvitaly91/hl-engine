#include "common/logger.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <mutex>
#include <process.h>
#include <sstream>
#include <utility>

#include "common/text_encoding.h"

namespace
{
using hl::common::LogCategory;
using hl::common::LogLevel;
using hl::common::LoggerOptions;
using hl::common::LoggerSessionInfo;
using hl::common::LoggerStatistics;

constexpr std::uintmax_t kDefaultMaxFileSizeBytes = 10u * 1024u * 1024u;

bool IsLevelEnabled(LogLevel level, LogLevel minimum) noexcept
{
    return static_cast<int>(level) >= static_cast<int>(minimum);
}

bool ContainsCategory(
    const std::vector<LogCategory>& categories,
    LogCategory category) noexcept
{
    return std::find(categories.begin(), categories.end(), category) != categories.end();
}

std::string ToLowerCopy(std::string_view text)
{
    std::string lowered(text);
    std::transform(
        lowered.begin(),
        lowered.end(),
        lowered.begin(),
        [](unsigned char character)
        {
            return static_cast<char>(std::tolower(character));
        });
    return lowered;
}

std::string BuildTimestamp()
{
    const auto now = std::chrono::system_clock::now();
    const auto epoch_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    const auto milliseconds = static_cast<int>(epoch_ms % 1000);
    const std::time_t now_time = std::chrono::system_clock::to_time_t(now);

    std::tm local_time = {};
#if defined(_MSC_VER)
    localtime_s(&local_time, &now_time);
#else
    local_time = *std::localtime(&now_time);
#endif

    std::ostringstream stream;
    stream << std::put_time(&local_time, "%Y-%m-%d %H:%M:%S")
           << '.'
           << std::setw(3) << std::setfill('0') << milliseconds;
    return stream.str();
}

std::string BuildSessionTimestamp()
{
    const auto now = std::chrono::system_clock::now();
    const std::time_t now_time = std::chrono::system_clock::to_time_t(now);

    std::tm local_time = {};
#if defined(_MSC_VER)
    localtime_s(&local_time, &now_time);
#else
    local_time = *std::localtime(&now_time);
#endif

    std::ostringstream stream;
    stream << std::put_time(&local_time, "%Y%m%d_%H%M%S");
    return stream.str();
}

std::string FormatPartNumber(std::size_t part)
{
    std::ostringstream stream;
    stream << std::setw(2) << std::setfill('0') << part;
    return stream.str();
}

void AppendRunLabel(std::string* filename, std::string_view run_label)
{
    if (filename == nullptr || run_label.empty())
    {
        return;
    }

    filename->append("__");
    filename->append(run_label);
}

std::filesystem::path BuildRollingLogPath(
    const std::filesystem::path& directory,
    std::string_view session_prefix,
    std::string_view file_id,
    std::string_view run_label,
    std::string_view suffix,
    std::size_t part)
{
    std::string filename = std::string(session_prefix) + "_" + std::string(file_id);
    AppendRunLabel(&filename, run_label);
    if (!suffix.empty())
    {
        filename += "_" + std::string(suffix);
    }
    filename += "_part" + FormatPartNumber(part) + ".log";
    return directory / filename;
}

std::filesystem::path BuildSummaryLogPath(
    const std::filesystem::path& directory,
    std::string_view session_prefix,
    std::string_view file_id,
    std::string_view run_label)
{
    std::string filename = std::string(session_prefix) + "_" + std::string(file_id);
    AppendRunLabel(&filename, run_label);
    filename += "_summary.log";
    return directory / filename;
}

bool PathExists(const std::filesystem::path& path)
{
    std::error_code error;
    return std::filesystem::exists(path, error);
}

std::string MakeUniqueSessionId(
    const std::filesystem::path& directory,
    const LoggerOptions& options)
{
    const std::string base_session_id = BuildSessionTimestamp();
    std::string session_id = base_session_id;
    std::size_t suffix = 1;

    for (;;)
    {
        bool conflict = PathExists(BuildRollingLogPath(
            directory,
            options.session_prefix,
            session_id,
            options.run_label,
            "",
            1));
        if (!conflict && options.log_summary_file)
        {
            conflict = PathExists(BuildSummaryLogPath(
                directory,
                options.session_prefix,
                session_id,
                options.run_label));
        }
        if (!conflict)
        {
            for (LogCategory category : options.category_file_sinks)
            {
                if (PathExists(BuildRollingLogPath(
                        directory,
                        options.session_prefix,
                        session_id,
                        options.run_label,
                        hl::common::ToString(category),
                        1)))
                {
                    conflict = true;
                    break;
                }
            }
        }

        if (!conflict)
        {
            return session_id;
        }

        session_id = base_session_id + "_" + FormatPartNumber(suffix++);
    }
}

std::string BuildBaseRunInstanceId(std::string_view session_id)
{
    const auto now = std::chrono::system_clock::now();
    const auto epoch_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    const auto milliseconds = static_cast<int>(epoch_ms % 1000);

    std::ostringstream stream;
    stream << session_id
           << '_'
           << std::setw(3) << std::setfill('0') << milliseconds
           << "_pid"
           << _getpid();
    return stream.str();
}

std::string MakeUniqueRunInstanceId(
    const std::filesystem::path& directory,
    const LoggerOptions& options,
    std::string_view session_id)
{
    const std::string base_run_instance_id = BuildBaseRunInstanceId(session_id);
    std::string run_instance_id = base_run_instance_id;
    std::size_t suffix = 1;

    for (;;)
    {
        bool conflict = PathExists(BuildRollingLogPath(
            directory,
            options.session_prefix,
            run_instance_id,
            options.run_label,
            "",
            1));
        if (!conflict && options.log_summary_file)
        {
            conflict = PathExists(BuildSummaryLogPath(
                directory,
                options.session_prefix,
                run_instance_id,
                options.run_label));
        }
        if (!conflict)
        {
            for (LogCategory category : options.category_file_sinks)
            {
                if (PathExists(BuildRollingLogPath(
                        directory,
                        options.session_prefix,
                        run_instance_id,
                        options.run_label,
                        hl::common::ToString(category),
                        1)))
                {
                    conflict = true;
                    break;
                }
            }
        }

        if (!conflict)
        {
            return run_instance_id;
        }

        run_instance_id = base_run_instance_id + "_" + FormatPartNumber(suffix++);
    }
}

std::string BuildLineBody(
    LogCategory category,
    LogLevel level,
    std::string_view message)
{
    return "[" + std::string(hl::common::ToString(level))
        + "][" + std::string(hl::common::ToString(category)) + "] "
        + std::string(message);
}

std::string BuildTimestampedLine(std::string_view body)
{
    return "[" + BuildTimestamp() + "] " + std::string(body);
}

LogLevel EffectiveSinkLevel(
    const std::optional<LogLevel>& global_level,
    LogLevel sink_level) noexcept
{
    if (!global_level.has_value())
    {
        return sink_level;
    }

    return static_cast<int>(*global_level) > static_cast<int>(sink_level)
        ? *global_level
        : sink_level;
}

bool IsCategoryEnabled(
    const LoggerOptions& options,
    LogCategory category,
    LogLevel level) noexcept
{
    if (static_cast<int>(level) >= static_cast<int>(LogLevel::Warn))
    {
        return true;
    }

    if (ContainsCategory(options.disabled_categories, category))
    {
        return false;
    }

    if (!options.enabled_categories.empty()
        && !ContainsCategory(options.enabled_categories, category))
    {
        return false;
    }

    return true;
}

struct RepeatState
{
    std::string last_body;
    LogCategory last_category = LogCategory::General;
    LogLevel last_level = LogLevel::Info;
    std::uintmax_t repeat_count = 0;
    std::uintmax_t suppressed_repeat_count = 0;
};

struct ConsoleSink
{
    bool enabled = false;
    LogLevel level = LogLevel::Info;
    RepeatState repeats;
    std::uintmax_t bytes_written = 0;

    void WriteImmediate(std::string_view body, LogLevel level_to_write);
    void FlushRepeats();
    void Write(
        LogCategory category,
        LogLevel level_to_write,
        std::string_view body,
        bool suppress_repeats);
    void Flush();
};

struct PlainFileSink
{
    bool enabled = false;
    std::filesystem::path path;
    std::ofstream stream;
    RepeatState repeats;
    std::uintmax_t bytes_written = 0;
    std::size_t files_created = 0;

    bool Open(const std::filesystem::path& target_path);
    void WriteImmediate(std::string_view body);
    void FlushRepeats();
    void Write(
        LogCategory category,
        LogLevel level_to_write,
        std::string_view body,
        bool suppress_repeats);
    void Flush();
    void Close();
};

struct RollingFileSink
{
    bool enabled = false;
    std::filesystem::path directory;
    std::string session_prefix;
    std::string run_instance_id;
    std::string run_label;
    std::string suffix;
    std::uintmax_t max_file_size_bytes = kDefaultMaxFileSizeBytes;
    std::size_t next_part = 1;
    std::uintmax_t current_size_bytes = 0;
    std::filesystem::path current_path;
    std::ofstream stream;
    std::vector<std::filesystem::path> files;
    RepeatState repeats;
    std::uintmax_t bytes_written = 0;

    bool OpenNextPart();
    bool EnsureOpen();
    void WriteImmediate(std::string_view body);
    void FlushRepeats();
    void Write(
        LogCategory category,
        LogLevel level_to_write,
        std::string_view body,
        bool suppress_repeats);
    void Flush();
    void Close();
};

struct CategoryFileSink
{
    LogCategory category = LogCategory::General;
    RollingFileSink sink;
};

struct LoggerState
{
    std::mutex mutex;
    bool configured = false;
    LoggerOptions options;
    std::filesystem::path resolved_log_directory;
    std::string session_id;
    std::string run_instance_id;
    ConsoleSink console_sink;
    RollingFileSink main_file_sink;
    PlainFileSink summary_file_sink;
    std::vector<CategoryFileSink> category_file_sinks;
    std::map<std::string, std::string> state_values;
};

LoggerState& GetState()
{
    static LoggerState state;
    return state;
}

void ConsoleSink::WriteImmediate(std::string_view body, LogLevel level_to_write)
{
    const std::string line = BuildTimestampedLine(body);
    std::ostream& stream = level_to_write == LogLevel::Error ? std::cerr : std::cout;
    stream << line << '\n';
    stream.flush();
    bytes_written += static_cast<std::uintmax_t>(line.size() + 1);
}

void ConsoleSink::FlushRepeats()
{
    if (repeats.repeat_count == 0)
    {
        return;
    }

    repeats.suppressed_repeat_count += repeats.repeat_count;
    const std::string repeat_body =
        BuildLineBody(
            repeats.last_category,
            repeats.last_level,
            "previous line repeated " + std::to_string(repeats.repeat_count) + " times");
    WriteImmediate(repeat_body, repeats.last_level);
    repeats.repeat_count = 0;
    repeats.last_body.clear();
}

void ConsoleSink::Write(
    LogCategory category,
    LogLevel level_to_write,
    std::string_view body,
    bool suppress_repeats)
{
    if (!enabled)
    {
        return;
    }

    if (suppress_repeats)
    {
        if (body == repeats.last_body)
        {
            ++repeats.repeat_count;
            return;
        }

        FlushRepeats();
        repeats.last_body = std::string(body);
        repeats.last_category = category;
        repeats.last_level = level_to_write;
    }

    WriteImmediate(body, level_to_write);
}

void ConsoleSink::Flush()
{
    FlushRepeats();
    std::cout.flush();
    std::cerr.flush();
}

bool PlainFileSink::Open(const std::filesystem::path& target_path)
{
    path = target_path;
    stream.open(path, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!stream.is_open())
    {
        return false;
    }

    files_created = 1;
    return true;
}

void PlainFileSink::WriteImmediate(std::string_view body)
{
    if (!enabled || !stream.is_open())
    {
        return;
    }

    const std::string line = BuildTimestampedLine(body);
    stream << line << '\n';
    stream.flush();
    bytes_written += static_cast<std::uintmax_t>(line.size() + 1);
}

void PlainFileSink::FlushRepeats()
{
    if (repeats.repeat_count == 0)
    {
        return;
    }

    repeats.suppressed_repeat_count += repeats.repeat_count;
    const std::string repeat_body =
        BuildLineBody(
            repeats.last_category,
            repeats.last_level,
            "previous line repeated " + std::to_string(repeats.repeat_count) + " times");
    WriteImmediate(repeat_body);
    repeats.repeat_count = 0;
    repeats.last_body.clear();
}

void PlainFileSink::Write(
    LogCategory category,
    LogLevel level_to_write,
    std::string_view body,
    bool suppress_repeats)
{
    if (!enabled || !stream.is_open())
    {
        return;
    }

    if (suppress_repeats)
    {
        if (body == repeats.last_body)
        {
            ++repeats.repeat_count;
            return;
        }

        FlushRepeats();
        repeats.last_body = std::string(body);
        repeats.last_category = category;
        repeats.last_level = level_to_write;
    }

    WriteImmediate(body);
}

void PlainFileSink::Flush()
{
    FlushRepeats();
    if (stream.is_open())
    {
        stream.flush();
    }
}

void PlainFileSink::Close()
{
    Flush();
    if (stream.is_open())
    {
        stream.close();
    }
}

bool RollingFileSink::OpenNextPart()
{
    current_path = BuildRollingLogPath(
        directory,
        session_prefix,
        run_instance_id,
        run_label,
        suffix,
        next_part++);
    stream.open(current_path, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!stream.is_open())
    {
        current_path.clear();
        return false;
    }

    files.push_back(current_path);
    current_size_bytes = 0;
    return true;
}

bool RollingFileSink::EnsureOpen()
{
    return stream.is_open() || OpenNextPart();
}

void RollingFileSink::WriteImmediate(std::string_view body)
{
    if (!enabled)
    {
        return;
    }

    if (!EnsureOpen())
    {
        return;
    }

    const std::string line = BuildTimestampedLine(body);
    const std::uintmax_t bytes_needed =
        static_cast<std::uintmax_t>(line.size() + 1);
    if (current_size_bytes > 0
        && current_size_bytes + bytes_needed > max_file_size_bytes)
    {
        stream.flush();
        stream.close();
        if (!OpenNextPart())
        {
            return;
        }
    }

    stream << line << '\n';
    stream.flush();
    current_size_bytes += bytes_needed;
    bytes_written += bytes_needed;
}

void RollingFileSink::FlushRepeats()
{
    if (repeats.repeat_count == 0)
    {
        return;
    }

    repeats.suppressed_repeat_count += repeats.repeat_count;
    const std::string repeat_body =
        BuildLineBody(
            repeats.last_category,
            repeats.last_level,
            "previous line repeated " + std::to_string(repeats.repeat_count) + " times");
    WriteImmediate(repeat_body);
    repeats.repeat_count = 0;
    repeats.last_body.clear();
}

void RollingFileSink::Write(
    LogCategory category,
    LogLevel level_to_write,
    std::string_view body,
    bool suppress_repeats)
{
    if (!enabled)
    {
        return;
    }

    if (suppress_repeats)
    {
        if (body == repeats.last_body)
        {
            ++repeats.repeat_count;
            return;
        }

        FlushRepeats();
        repeats.last_body = std::string(body);
        repeats.last_category = category;
        repeats.last_level = level_to_write;
    }

    WriteImmediate(body);
}

void RollingFileSink::Flush()
{
    FlushRepeats();
    if (stream.is_open())
    {
        stream.flush();
    }
}

void RollingFileSink::Close()
{
    Flush();
    if (stream.is_open())
    {
        stream.close();
    }
}

void ResetStateNoLock(LoggerState& state)
{
    state.console_sink = {};
    state.main_file_sink.Close();
    state.main_file_sink = {};
    state.summary_file_sink.Close();
    state.summary_file_sink = {};
    for (CategoryFileSink& sink : state.category_file_sinks)
    {
        sink.sink.Close();
    }
    state.category_file_sinks.clear();
    state.state_values.clear();
    state.resolved_log_directory.clear();
    state.session_id.clear();
    state.run_instance_id.clear();
    state.configured = false;
}

void WriteFallbackNoLock(LogCategory category, LogLevel level, std::string_view message)
{
    const std::string body = BuildLineBody(category, level, message);
    const std::string line = BuildTimestampedLine(body);
    std::ostream& stream = level == LogLevel::Error ? std::cerr : std::cout;
    stream << line << '\n';
    stream.flush();
}

bool ShouldWriteSummary(LogCategory category, LogLevel level) noexcept
{
    return static_cast<int>(level) >= static_cast<int>(LogLevel::Warn)
        || category == LogCategory::Startup
        || category == LogCategory::Summary
        || category == LogCategory::Perf;
}

void WriteConfiguredNoLock(
    LoggerState& state,
    LogCategory category,
    LogLevel level,
    std::string_view message)
{
    if (!state.configured)
    {
        WriteFallbackNoLock(category, level, message);
        return;
    }

    const std::string body = BuildLineBody(category, level, message);
    const bool suppress_repeats = state.options.suppress_repeats;

    if (state.console_sink.enabled
        && IsCategoryEnabled(state.options, category, level)
        && IsLevelEnabled(
            level,
            EffectiveSinkLevel(state.options.global_level, state.console_sink.level)))
    {
        state.console_sink.Write(category, level, body, suppress_repeats);
    }

    if (state.main_file_sink.enabled
        && IsCategoryEnabled(state.options, category, level)
        && IsLevelEnabled(
            level,
            EffectiveSinkLevel(state.options.global_level, state.options.file_level)))
    {
        state.main_file_sink.Write(category, level, body, suppress_repeats);
    }

    if (state.summary_file_sink.enabled
        && ShouldWriteSummary(category, level)
        && IsLevelEnabled(
            level,
            EffectiveSinkLevel(state.options.global_level, state.options.summary_level)))
    {
        state.summary_file_sink.Write(category, level, body, suppress_repeats);
    }

    for (CategoryFileSink& sink : state.category_file_sinks)
    {
        if (sink.category == category
            && IsLevelEnabled(
                level,
                EffectiveSinkLevel(state.options.global_level, state.options.file_level)))
        {
            sink.sink.Write(category, level, body, suppress_repeats);
        }
    }
}

LoggerSessionInfo BuildSessionInfoNoLock(const LoggerState& state)
{
    LoggerSessionInfo info;
    info.configured = state.configured;
    info.log_directory = state.resolved_log_directory;
    info.session_id = state.session_id;
    info.run_instance_id = state.run_instance_id;
    info.run_label = state.options.run_label;
    info.max_file_size_bytes = state.options.max_file_size_bytes;
    info.current_file_path = state.main_file_sink.current_path;
    info.summary_file_path = state.summary_file_sink.path;
    info.log_files = state.main_file_sink.files;
    for (const CategoryFileSink& sink : state.category_file_sinks)
    {
        info.category_log_files.insert(
            info.category_log_files.end(),
            sink.sink.files.begin(),
            sink.sink.files.end());
    }
    return info;
}

LoggerStatistics BuildStatisticsNoLock(const LoggerState& state)
{
    LoggerStatistics statistics;
    statistics.total_log_files_created =
        state.main_file_sink.files.size()
        + (state.summary_file_sink.enabled && !state.summary_file_sink.path.empty() ? 1u : 0u);
    statistics.total_bytes_written =
        state.console_sink.bytes_written
        + state.main_file_sink.bytes_written
        + state.summary_file_sink.bytes_written;
    statistics.suppressed_repeat_count =
        state.console_sink.repeats.suppressed_repeat_count
        + state.console_sink.repeats.repeat_count
        + state.main_file_sink.repeats.suppressed_repeat_count
        + state.main_file_sink.repeats.repeat_count
        + state.summary_file_sink.repeats.suppressed_repeat_count
        + state.summary_file_sink.repeats.repeat_count;
    statistics.final_log_paths = state.main_file_sink.files;
    if (state.summary_file_sink.enabled && !state.summary_file_sink.path.empty())
    {
        statistics.final_log_paths.push_back(state.summary_file_sink.path);
    }

    for (const CategoryFileSink& sink : state.category_file_sinks)
    {
        statistics.total_log_files_created += sink.sink.files.size();
        statistics.total_bytes_written += sink.sink.bytes_written;
        statistics.suppressed_repeat_count +=
            sink.sink.repeats.suppressed_repeat_count
            + sink.sink.repeats.repeat_count;
        statistics.final_log_paths.insert(
            statistics.final_log_paths.end(),
            sink.sink.files.begin(),
            sink.sink.files.end());
    }

    return statistics;
}
} // namespace

namespace hl::common
{
std::string_view ToString(LogLevel level) noexcept
{
    switch (level)
    {
    case LogLevel::Trace:
        return "trace";
    case LogLevel::Debug:
        return "debug";
    case LogLevel::Info:
        return "info";
    case LogLevel::Warn:
        return "warn";
    case LogLevel::Error:
        return "error";
    default:
        return "unknown";
    }
}

std::string_view ToString(LogCategory category) noexcept
{
    switch (category)
    {
    case LogCategory::General:
        return "general";
    case LogCategory::Startup:
        return "startup";
    case LogCategory::Filesystem:
        return "filesystem";
    case LogCategory::Dll:
        return "dll";
    case LogCategory::Server:
        return "server";
    case LogCategory::World:
        return "world";
    case LogCategory::Entity:
        return "entity";
    case LogCategory::Think:
        return "think";
    case LogCategory::Use:
        return "use";
    case LogCategory::Scripted:
        return "scripted";
    case LogCategory::Path:
        return "path";
    case LogCategory::PathEvent:
        return "path_event";
    case LogCategory::Summary:
        return "summary";
    case LogCategory::Perf:
        return "perf";
    default:
        return "unknown";
    }
}

std::optional<LogLevel> ParseLogLevel(std::string_view text)
{
    const std::string lowered = ToLowerCopy(text);
    if (lowered == "trace")
    {
        return LogLevel::Trace;
    }
    if (lowered == "debug")
    {
        return LogLevel::Debug;
    }
    if (lowered == "info")
    {
        return LogLevel::Info;
    }
    if (lowered == "warn" || lowered == "warning")
    {
        return LogLevel::Warn;
    }
    if (lowered == "error")
    {
        return LogLevel::Error;
    }
    return std::nullopt;
}

std::optional<LogCategory> ParseLogCategory(std::string_view text)
{
    const std::string lowered = ToLowerCopy(text);
    if (lowered == "general")
    {
        return LogCategory::General;
    }
    if (lowered == "startup")
    {
        return LogCategory::Startup;
    }
    if (lowered == "filesystem" || lowered == "fs")
    {
        return LogCategory::Filesystem;
    }
    if (lowered == "dll")
    {
        return LogCategory::Dll;
    }
    if (lowered == "server")
    {
        return LogCategory::Server;
    }
    if (lowered == "world")
    {
        return LogCategory::World;
    }
    if (lowered == "entity")
    {
        return LogCategory::Entity;
    }
    if (lowered == "think")
    {
        return LogCategory::Think;
    }
    if (lowered == "use")
    {
        return LogCategory::Use;
    }
    if (lowered == "scripted")
    {
        return LogCategory::Scripted;
    }
    if (lowered == "path")
    {
        return LogCategory::Path;
    }
    if (lowered == "path_event" || lowered == "path-event" || lowered == "pathevent")
    {
        return LogCategory::PathEvent;
    }
    if (lowered == "summary")
    {
        return LogCategory::Summary;
    }
    if (lowered == "perf" || lowered == "performance")
    {
        return LogCategory::Perf;
    }
    return std::nullopt;
}

void Logger::Configure(const LoggerOptions& options)
{
    LoggerState& state = GetState();
    std::lock_guard<std::mutex> lock(state.mutex);

    ResetStateNoLock(state);

    state.options = options;
    if (state.options.max_file_size_bytes == 0)
    {
        state.options.max_file_size_bytes = kDefaultMaxFileSizeBytes;
    }
    if (state.options.log_directory.empty())
    {
        state.options.log_directory = std::filesystem::path("logs");
    }
    if (state.options.session_prefix.empty())
    {
        state.options.session_prefix = "hlhost";
    }

    state.console_sink.enabled = state.options.log_to_console;
    state.console_sink.level = state.options.console_level;

    std::error_code error;
    state.resolved_log_directory = std::filesystem::absolute(state.options.log_directory, error);
    if (error)
    {
        state.resolved_log_directory = state.options.log_directory;
        error.clear();
    }

    if (state.options.log_to_file || state.options.log_summary_file
        || !state.options.category_file_sinks.empty())
    {
        std::filesystem::create_directories(state.resolved_log_directory, error);
        if (error)
        {
            state.options.log_to_file = false;
            state.options.log_summary_file = false;
            state.options.category_file_sinks.clear();
            WriteFallbackNoLock(
                LogCategory::Startup,
                LogLevel::Warn,
                "Logger could not create log directory '"
                    + ToUtf8(state.resolved_log_directory) + "'. File logging disabled.");
            error.clear();
        }
    }

    state.session_id = MakeUniqueSessionId(state.resolved_log_directory, state.options);
    state.run_instance_id =
        MakeUniqueRunInstanceId(
            state.resolved_log_directory,
            state.options,
            state.session_id);

    state.main_file_sink.enabled = state.options.log_to_file;
    state.main_file_sink.directory = state.resolved_log_directory;
    state.main_file_sink.session_prefix = state.options.session_prefix;
    state.main_file_sink.run_instance_id = state.run_instance_id;
    state.main_file_sink.run_label = state.options.run_label;
    state.main_file_sink.max_file_size_bytes = state.options.max_file_size_bytes;
    if (state.main_file_sink.enabled && !state.main_file_sink.OpenNextPart())
    {
        state.main_file_sink.enabled = false;
        WriteFallbackNoLock(
            LogCategory::Startup,
            LogLevel::Warn,
            "Logger could not open the main session log file. File logging disabled.");
    }

    state.summary_file_sink.enabled = state.options.log_summary_file;
    if (state.summary_file_sink.enabled
        && !state.summary_file_sink.Open(BuildSummaryLogPath(
            state.resolved_log_directory,
            state.options.session_prefix,
            state.run_instance_id,
            state.options.run_label)))
    {
        state.summary_file_sink.enabled = false;
        WriteFallbackNoLock(
            LogCategory::Startup,
            LogLevel::Warn,
            "Logger could not open the summary log file. Summary logging disabled.");
    }

    for (LogCategory category : state.options.category_file_sinks)
    {
        CategoryFileSink category_sink;
        category_sink.category = category;
        category_sink.sink.enabled = true;
        category_sink.sink.directory = state.resolved_log_directory;
        category_sink.sink.session_prefix = state.options.session_prefix;
        category_sink.sink.run_instance_id = state.run_instance_id;
        category_sink.sink.run_label = state.options.run_label;
        category_sink.sink.suffix = std::string(ToString(category));
        category_sink.sink.max_file_size_bytes = state.options.max_file_size_bytes;
        if (category_sink.sink.OpenNextPart())
        {
            state.category_file_sinks.push_back(std::move(category_sink));
        }
    }

    state.configured = true;

    WriteConfiguredNoLock(state, LogCategory::Startup, LogLevel::Info, "Logger session started.");
    WriteConfiguredNoLock(
        state,
        LogCategory::Startup,
        LogLevel::Info,
        "Log directory: " + ToUtf8(state.resolved_log_directory));
    WriteConfiguredNoLock(
        state,
        LogCategory::Startup,
        LogLevel::Info,
        "Session id: " + state.session_id);
    WriteConfiguredNoLock(
        state,
        LogCategory::Startup,
        LogLevel::Info,
        "Run instance id: " + state.run_instance_id);
    WriteConfiguredNoLock(
        state,
        LogCategory::Startup,
        LogLevel::Info,
        "Max file size: " + std::to_string(state.options.max_file_size_bytes) + " bytes");
    WriteConfiguredNoLock(
        state,
        LogCategory::Startup,
        LogLevel::Info,
        "Current file path: "
            + (state.main_file_sink.current_path.empty()
                ? std::string("<disabled>")
                : ToUtf8(state.main_file_sink.current_path)));
    if (state.summary_file_sink.enabled)
    {
        WriteConfiguredNoLock(
            state,
            LogCategory::Startup,
            LogLevel::Info,
            "Summary file path: " + ToUtf8(state.summary_file_sink.path));
    }
}

void Logger::Flush()
{
    LoggerState& state = GetState();
    std::lock_guard<std::mutex> lock(state.mutex);

    if (!state.configured)
    {
        return;
    }

    state.console_sink.Flush();
    state.main_file_sink.Flush();
    state.summary_file_sink.Flush();
    for (CategoryFileSink& sink : state.category_file_sinks)
    {
        sink.sink.Flush();
    }
}

void Logger::Shutdown()
{
    LoggerState& state = GetState();
    std::lock_guard<std::mutex> lock(state.mutex);

    if (!state.configured)
    {
        return;
    }

    state.console_sink.Flush();
    state.main_file_sink.Flush();
    state.summary_file_sink.Flush();
    for (CategoryFileSink& sink : state.category_file_sinks)
    {
        sink.sink.Flush();
    }

    const LoggerStatistics statistics = BuildStatisticsNoLock(state);
    std::ostringstream final_paths;
    for (std::size_t index = 0; index < statistics.final_log_paths.size(); ++index)
    {
        if (index != 0)
        {
            final_paths << "; ";
        }
        final_paths << ToUtf8(statistics.final_log_paths[index]);
    }

    WriteConfiguredNoLock(
        state,
        LogCategory::Summary,
        LogLevel::Info,
        "Logger shutdown: total log files created="
            + std::to_string(statistics.total_log_files_created)
            + ", total bytes written="
            + std::to_string(statistics.total_bytes_written)
            + ", repeat suppressions="
            + std::to_string(statistics.suppressed_repeat_count));
    WriteConfiguredNoLock(
        state,
        LogCategory::Summary,
        LogLevel::Info,
        "Logger shutdown paths: "
            + (statistics.final_log_paths.empty()
                ? std::string("<none>")
                : final_paths.str()));

    state.console_sink.Flush();
    state.main_file_sink.Close();
    state.summary_file_sink.Close();
    for (CategoryFileSink& sink : state.category_file_sinks)
    {
        sink.sink.Close();
    }

    ResetStateNoLock(state);
}

bool Logger::IsConfigured()
{
    LoggerState& state = GetState();
    std::lock_guard<std::mutex> lock(state.mutex);
    return state.configured;
}

LoggerSessionInfo Logger::SessionInfo()
{
    LoggerState& state = GetState();
    std::lock_guard<std::mutex> lock(state.mutex);
    return BuildSessionInfoNoLock(state);
}

LoggerStatistics Logger::Statistics()
{
    LoggerState& state = GetState();
    std::lock_guard<std::mutex> lock(state.mutex);
    return BuildStatisticsNoLock(state);
}

bool Logger::ShouldLog(LogCategory category, LogLevel level)
{
    LoggerState& state = GetState();
    std::lock_guard<std::mutex> lock(state.mutex);

    if (!state.configured)
    {
        return true;
    }

    const bool console_enabled =
        state.console_sink.enabled
        && IsCategoryEnabled(state.options, category, level)
        && IsLevelEnabled(
            level,
            EffectiveSinkLevel(state.options.global_level, state.console_sink.level));
    const bool file_enabled =
        state.main_file_sink.enabled
        && IsCategoryEnabled(state.options, category, level)
        && IsLevelEnabled(
            level,
            EffectiveSinkLevel(state.options.global_level, state.options.file_level));
    const bool summary_enabled =
        state.summary_file_sink.enabled
        && ShouldWriteSummary(category, level)
        && IsLevelEnabled(
            level,
            EffectiveSinkLevel(state.options.global_level, state.options.summary_level));

    if (console_enabled || file_enabled || summary_enabled)
    {
        return true;
    }

    return std::any_of(
        state.category_file_sinks.begin(),
        state.category_file_sinks.end(),
        [&](const CategoryFileSink& sink)
        {
            return sink.category == category
                && IsLevelEnabled(
                    level,
                    EffectiveSinkLevel(state.options.global_level, state.options.file_level));
        });
}

bool Logger::ShouldLogFrame(
    int frame_number,
    int sample,
    bool state_changed,
    bool important_event)
{
    if (important_event || state_changed || sample <= 1 || frame_number <= 0)
    {
        return true;
    }

    return (frame_number % sample) == 0;
}

bool Logger::LogFrameSampled(
    LogCategory category,
    LogLevel level,
    int frame_number,
    int sample,
    std::string_view message,
    bool state_changed,
    bool important_event)
{
    if (!ShouldLogFrame(frame_number, sample, state_changed, important_event))
    {
        return false;
    }

    Log(category, level, message);
    return true;
}

bool Logger::LogStateChange(
    LogCategory category,
    LogLevel level,
    std::string_view key,
    std::string_view state_value,
    std::string_view message)
{
    LoggerState& state = GetState();
    std::lock_guard<std::mutex> lock(state.mutex);

    const std::string compound_key =
        std::string(ToString(category)) + ":" + std::string(key);
    const auto it = state.state_values.find(compound_key);
    if (it != state.state_values.end() && it->second == state_value)
    {
        return false;
    }

    state.state_values[compound_key] = std::string(state_value);
    WriteConfiguredNoLock(state, category, level, message);
    return true;
}

void Logger::Log(LogCategory category, LogLevel level, std::string_view message)
{
    Write(category, level, message);
}

void Logger::Log(LogCategory category, LogLevel level, std::wstring_view message)
{
    Write(category, level, ToUtf8(message));
}

void Logger::Logf(LogCategory category, LogLevel level, const char* format, ...)
{
    if (format == nullptr)
    {
        return;
    }

    va_list args;
    va_start(args, format);

    va_list args_copy;
    va_copy(args_copy, args);
    const int required_size = std::vsnprintf(nullptr, 0, format, args_copy);
    va_end(args_copy);

    if (required_size < 0)
    {
        va_end(args);
        return;
    }

    std::string buffer(static_cast<std::size_t>(required_size) + 1u, '\0');
    std::vsnprintf(buffer.data(), buffer.size(), format, args);
    va_end(args);
    buffer.resize(static_cast<std::size_t>(required_size));

    Write(category, level, buffer);
}

void Logger::Trace(LogCategory category, std::string_view message)
{
    Write(category, LogLevel::Trace, message);
}

void Logger::Trace(LogCategory category, std::wstring_view message)
{
    Write(category, LogLevel::Trace, ToUtf8(message));
}

void Logger::Debug(LogCategory category, std::string_view message)
{
    Write(category, LogLevel::Debug, message);
}

void Logger::Debug(LogCategory category, std::wstring_view message)
{
    Write(category, LogLevel::Debug, ToUtf8(message));
}

void Logger::Info(LogCategory category, std::string_view message)
{
    Write(category, LogLevel::Info, message);
}

void Logger::Info(LogCategory category, std::wstring_view message)
{
    Write(category, LogLevel::Info, ToUtf8(message));
}

void Logger::Warn(LogCategory category, std::string_view message)
{
    Write(category, LogLevel::Warn, message);
}

void Logger::Warn(LogCategory category, std::wstring_view message)
{
    Write(category, LogLevel::Warn, ToUtf8(message));
}

void Logger::Error(LogCategory category, std::string_view message)
{
    Write(category, LogLevel::Error, message);
}

void Logger::Error(LogCategory category, std::wstring_view message)
{
    Write(category, LogLevel::Error, ToUtf8(message));
}

void Logger::Info(std::string_view message)
{
    Write(LogCategory::General, LogLevel::Info, message);
}

void Logger::Info(std::wstring_view message)
{
    Write(LogCategory::General, LogLevel::Info, ToUtf8(message));
}

void Logger::Warn(std::string_view message)
{
    Write(LogCategory::General, LogLevel::Warn, message);
}

void Logger::Warn(std::wstring_view message)
{
    Write(LogCategory::General, LogLevel::Warn, ToUtf8(message));
}

void Logger::Error(std::string_view message)
{
    Write(LogCategory::General, LogLevel::Error, message);
}

void Logger::Error(std::wstring_view message)
{
    Write(LogCategory::General, LogLevel::Error, ToUtf8(message));
}

void Logger::Write(LogCategory category, LogLevel level, std::string_view message)
{
    LoggerState& state = GetState();
    std::lock_guard<std::mutex> lock(state.mutex);
    WriteConfiguredNoLock(state, category, level, message);
}
} // namespace hl::common
