#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "app/host_application.h"
#include "app/launch_options.h"
#include "common/logger.h"
#include "common/text_encoding.h"
#include "platform/environment.h"

namespace
{
struct GitRevisionIdentity
{
    std::string branch = "<unknown>";
    std::string commit = "<unknown>";
};

struct CodexRunIdentity
{
    std::string session_id;
    std::string run_instance_id;
    std::string run_label;
    std::string prompt_id;
    std::string regression_guard = "<none>";
    bool stop_on_changelevel_request = false;
    int frames = 0;
    float frametime = 0.0f;
    std::string current_file_path = "<disabled>";
    std::string summary_file_path = "<disabled>";
    std::string git_branch = "<unknown>";
    std::string git_commit = "<unknown>";
};

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

std::filesystem::path TryResolveHostRepositoryRoot(std::filesystem::path base_path)
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
    }

    for (int depth = 0; depth < 8; ++depth)
    {
        if (LooksLikeHostRepositoryRoot(current))
        {
            return current;
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

std::string TrimAscii(std::string_view value)
{
    std::size_t begin = 0;
    while (begin < value.size()
        && (value[begin] == ' ' || value[begin] == '\r' || value[begin] == '\n'
            || value[begin] == '\t'))
    {
        ++begin;
    }

    std::size_t end = value.size();
    while (end > begin
        && (value[end - 1] == ' ' || value[end - 1] == '\r' || value[end - 1] == '\n'
            || value[end - 1] == '\t'))
    {
        --end;
    }

    return std::string(value.substr(begin, end - begin));
}

std::string ReadTextFile(const std::filesystem::path& path)
{
    std::ifstream stream(path, std::ios::in | std::ios::binary);
    if (!stream.is_open())
    {
        return {};
    }

    std::ostringstream content;
    content << stream.rdbuf();
    return content.str();
}

std::filesystem::path ResolveGitDirectory(const std::filesystem::path& repository_root)
{
    const std::filesystem::path git_path = repository_root / ".git";
    if (std::filesystem::is_directory(git_path))
    {
        return git_path;
    }

    if (!std::filesystem::is_regular_file(git_path))
    {
        return {};
    }

    const std::string git_file = TrimAscii(ReadTextFile(git_path));
    constexpr std::string_view kGitDirPrefix = "gitdir:";
    if (git_file.size() <= kGitDirPrefix.size()
        || git_file.compare(0, kGitDirPrefix.size(), kGitDirPrefix) != 0)
    {
        return {};
    }

    const std::filesystem::path resolved = TrimAscii(git_file.substr(kGitDirPrefix.size()));
    if (resolved.empty())
    {
        return {};
    }

    return resolved.is_absolute() ? resolved : repository_root / resolved;
}

std::string DisplayBranchName(std::string_view ref_name)
{
    constexpr std::string_view kHeadsPrefix = "refs/heads/";
    if (ref_name.compare(0, kHeadsPrefix.size(), kHeadsPrefix) == 0)
    {
        return std::string(ref_name.substr(kHeadsPrefix.size()));
    }

    return std::string(ref_name);
}

std::string FindPackedRefCommit(
    const std::filesystem::path& packed_refs_path,
    std::string_view ref_name)
{
    std::ifstream stream(packed_refs_path);
    if (!stream.is_open())
    {
        return {};
    }

    std::string line;
    while (std::getline(stream, line))
    {
        if (line.empty() || line[0] == '#' || line[0] == '^')
        {
            continue;
        }

        const std::size_t separator = line.find(' ');
        if (separator == std::string::npos)
        {
            continue;
        }

        const std::string ref = TrimAscii(line.substr(separator + 1));
        if (ref == ref_name)
        {
            return TrimAscii(line.substr(0, separator));
        }
    }

    return {};
}

GitRevisionIdentity ReadGitRevisionIdentity(const std::filesystem::path& repository_root)
{
    GitRevisionIdentity identity;
    if (repository_root.empty())
    {
        return identity;
    }

    const std::filesystem::path git_directory = ResolveGitDirectory(repository_root);
    if (git_directory.empty())
    {
        return identity;
    }

    const std::string head = TrimAscii(ReadTextFile(git_directory / "HEAD"));
    if (head.empty())
    {
        return identity;
    }

    constexpr std::string_view kRefPrefix = "ref:";
    if (head.compare(0, kRefPrefix.size(), kRefPrefix) == 0)
    {
        const std::string ref_name = TrimAscii(head.substr(kRefPrefix.size()));
        if (!ref_name.empty())
        {
            identity.branch = DisplayBranchName(ref_name);
            const std::filesystem::path ref_path = git_directory / std::filesystem::path(ref_name);
            std::string commit = TrimAscii(ReadTextFile(ref_path));
            if (commit.empty())
            {
                commit = FindPackedRefCommit(git_directory / "packed-refs", ref_name);
            }
            if (!commit.empty())
            {
                identity.commit = commit;
            }
        }
        return identity;
    }

    identity.branch = "<detached>";
    identity.commit = head;
    return identity;
}

std::filesystem::path ResolveRepositoryRootForRun(
    int argc,
    wchar_t* argv[],
    const hl::app::LaunchOptions& options)
{
    std::vector<std::filesystem::path> candidates;
    candidates.push_back(options.log_directory);

    std::error_code error;
    candidates.push_back(std::filesystem::current_path(error));

    if (argc > 0 && argv != nullptr && argv[0] != nullptr)
    {
        std::filesystem::path executable_path =
            std::filesystem::absolute(std::filesystem::path(argv[0]), error);
        if (!error)
        {
            candidates.push_back(executable_path);
            candidates.push_back(executable_path.parent_path());
        }
    }

    for (const std::filesystem::path& candidate : candidates)
    {
        const std::filesystem::path repository_root = TryResolveHostRepositoryRoot(candidate);
        if (!repository_root.empty())
        {
            return repository_root;
        }
    }

    return {};
}

std::string RegressionGuardProfileName(hl::app::RegressionGuardProfile profile)
{
    switch (profile)
    {
    case hl::app::RegressionGuardProfile::kTrainstop26TerminalProbe:
        return "trainstop26-terminal-probe";
    case hl::app::RegressionGuardProfile::kTrainstop26Baseline:
        return "trainstop26-baseline";
    case hl::app::RegressionGuardProfile::kChangelevelLatchOnlyContinuation:
        return "changelevel-latch-only-continuation";
    case hl::app::RegressionGuardProfile::kChangelevelRequestConsumed:
        return "changelevel-request-consumed";
    }

    return "unknown";
}

std::string DisplayPath(const std::filesystem::path& path)
{
    return path.empty() ? std::string("<disabled>") : hl::common::ToUtf8(path);
}

CodexRunIdentity BuildCodexRunIdentity(
    const hl::app::LaunchOptions& options,
    const hl::common::LoggerSessionInfo& session_info,
    const GitRevisionIdentity& git_identity)
{
    CodexRunIdentity identity;
    identity.session_id = session_info.session_id;
    identity.run_instance_id = session_info.run_instance_id;
    identity.run_label = options.run_label.value_or(std::string());
    identity.prompt_id = options.prompt_id.value_or(std::string());
    if (options.regression_guard.has_value())
    {
        identity.regression_guard = RegressionGuardProfileName(*options.regression_guard);
    }
    identity.stop_on_changelevel_request = options.stop_on_changelevel_request;
    identity.frames = options.frame_count;
    identity.frametime = options.frame_time;
    identity.current_file_path = DisplayPath(session_info.current_file_path);
    identity.summary_file_path = DisplayPath(session_info.summary_file_path);
    identity.git_branch = git_identity.branch;
    identity.git_commit = git_identity.commit;
    return identity;
}

std::string BuildCodexRunIdentityLine(const CodexRunIdentity& identity)
{
    return "codex_run_identity: sessionId=" + identity.session_id
        + ", runInstanceId=" + identity.run_instance_id
        + ", runLabel=" + identity.run_label
        + (identity.prompt_id.empty() ? std::string() : ", promptId=" + identity.prompt_id)
        + ", regressionGuard=" + identity.regression_guard
        + ", stopOnChangelevelRequest="
        + std::string(identity.stop_on_changelevel_request ? "1" : "0")
        + ", frames=" + std::to_string(identity.frames)
        + ", frametime=" + std::to_string(identity.frametime)
        + ", gitBranch=" + identity.git_branch
        + ", gitCommit=" + identity.git_commit
        + ", currentFile=" + identity.current_file_path
        + ", summaryFile=" + identity.summary_file_path
        + ", action=run identity recorded";
}

std::string EscapeJson(std::string_view value)
{
    std::string escaped;
    escaped.reserve(value.size());
    for (unsigned char character : value)
    {
        switch (character)
        {
        case '\\':
            escaped += "\\\\";
            break;
        case '"':
            escaped += "\\\"";
            break;
        case '\r':
            escaped += "\\r";
            break;
        case '\n':
            escaped += "\\n";
            break;
        case '\t':
            escaped += "\\t";
            break;
        default:
            escaped.push_back(static_cast<char>(character));
            break;
        }
    }

    return escaped;
}

std::optional<std::filesystem::path> WriteCodexRunManifest(
    const CodexRunIdentity& identity,
    const hl::common::LoggerSessionInfo& session_info)
{
    if (identity.run_label.empty() || identity.run_instance_id.empty())
    {
        return std::nullopt;
    }

    std::error_code error;
    const std::filesystem::path codex_directory =
        session_info.log_directory.parent_path() / "codex";
    std::filesystem::create_directories(codex_directory, error);
    if (error)
    {
        return std::nullopt;
    }

    const std::filesystem::path manifest_path =
        codex_directory
        / ("hlhost_" + identity.run_instance_id + "__" + identity.run_label + "_manifest.json");

    std::ofstream stream(manifest_path, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!stream.is_open())
    {
        return std::nullopt;
    }

    stream << "{\n";
    stream << "  \"sessionId\": \"" << EscapeJson(identity.session_id) << "\",\n";
    stream << "  \"runInstanceId\": \"" << EscapeJson(identity.run_instance_id) << "\",\n";
    stream << "  \"runLabel\": \"" << EscapeJson(identity.run_label) << "\",\n";
    if (!identity.prompt_id.empty())
    {
        stream << "  \"promptId\": \"" << EscapeJson(identity.prompt_id) << "\",\n";
    }
    stream << "  \"regressionGuard\": \"" << EscapeJson(identity.regression_guard) << "\",\n";
    stream << "  \"stopOnChangelevelRequest\": "
           << (identity.stop_on_changelevel_request ? "1" : "0") << ",\n";
    stream << "  \"frames\": " << identity.frames << ",\n";
    stream << "  \"frametime\": " << std::to_string(identity.frametime) << ",\n";
    stream << "  \"gitBranch\": \"" << EscapeJson(identity.git_branch) << "\",\n";
    stream << "  \"gitCommit\": \"" << EscapeJson(identity.git_commit) << "\",\n";
    stream << "  \"currentFilePath\": \"" << EscapeJson(identity.current_file_path) << "\",\n";
    stream << "  \"summaryFilePath\": \"" << EscapeJson(identity.summary_file_path) << "\",\n";
    stream << "  \"runtimeLogPaths\": [";
    for (std::size_t index = 0; index < session_info.log_files.size(); ++index)
    {
        if (index != 0)
        {
            stream << ", ";
        }
        stream << "\"" << EscapeJson(DisplayPath(session_info.log_files[index])) << "\"";
    }
    stream << "]\n";
    stream << "}\n";

    return manifest_path;
}

hl::common::LoggerOptions BuildLoggerOptions(const hl::app::LaunchOptions& options)
{
    hl::common::LoggerOptions logger_options;
    logger_options.log_directory = options.log_directory;
    logger_options.run_label = options.run_label.value_or(std::string());
    logger_options.log_to_console = true;
    logger_options.log_to_file = options.log_to_file;
    logger_options.log_summary_file = options.log_summary_file;
    logger_options.max_file_size_bytes =
        static_cast<std::uintmax_t>(std::max(options.log_max_mb, 1)) * 1024u * 1024u;
    logger_options.global_level = options.log_level;
    logger_options.console_level = options.log_console_level;
    logger_options.file_level = options.log_file_level;
    logger_options.summary_level = hl::common::LogLevel::Info;
    logger_options.suppress_repeats = options.log_suppress_repeats;
    logger_options.enabled_categories = options.log_categories;
    logger_options.disabled_categories = options.log_disable_categories;
    logger_options.category_file_sinks = options.log_category_files;
    return logger_options;
}
} // namespace

int wmain(int argc, wchar_t* argv[])
{
    hl::platform::ConfigureConsoleForUtf8();

    const std::filesystem::path executable_name =
        argc > 0 ? std::filesystem::path(argv[0]) : std::filesystem::path(L"hlhost.exe");

    const hl::app::LaunchOptionsParseResult parse_result = hl::app::ParseLaunchOptions(argc, argv);
    if (parse_result.error_message.has_value())
    {
        hl::common::Logger::Error(*parse_result.error_message);
        std::cout << hl::common::ToUtf8(hl::app::BuildUsageText(executable_name)) << '\n';
        return 1;
    }

    if (parse_result.options.show_help)
    {
        std::cout << hl::common::ToUtf8(hl::app::BuildUsageText(executable_name)) << '\n';
        return 0;
    }

    hl::common::Logger::Configure(BuildLoggerOptions(parse_result.options));

    const hl::app::HostApplication application;
    const int exit_code = application.Run(parse_result.options);

    if (parse_result.options.run_label.has_value())
    {
        const std::filesystem::path repository_root =
            ResolveRepositoryRootForRun(argc, argv, parse_result.options);
        const GitRevisionIdentity git_identity = ReadGitRevisionIdentity(repository_root);
        const hl::common::LoggerSessionInfo session_info = hl::common::Logger::SessionInfo();
        const CodexRunIdentity identity =
            BuildCodexRunIdentity(parse_result.options, session_info, git_identity);

        hl::common::Logger::Info(
            hl::common::LogCategory::Summary,
            BuildCodexRunIdentityLine(identity));
        hl::common::Logger::Flush();

        const hl::common::LoggerSessionInfo final_session_info = hl::common::Logger::SessionInfo();
        const CodexRunIdentity final_identity =
            BuildCodexRunIdentity(parse_result.options, final_session_info, git_identity);
        if (!WriteCodexRunManifest(final_identity, final_session_info).has_value())
        {
            hl::common::Logger::Warn(
                hl::common::LogCategory::Summary,
                "codex_run_manifest: write_failed");
        }
    }

    hl::common::Logger::Shutdown();
    return exit_code;
}
