#include <algorithm>
#include <filesystem>
#include <iostream>

#include "app/host_application.h"
#include "app/launch_options.h"
#include "common/logger.h"
#include "common/text_encoding.h"
#include "platform/environment.h"

namespace
{
hl::common::LoggerOptions BuildLoggerOptions(const hl::app::LaunchOptions& options)
{
    hl::common::LoggerOptions logger_options;
    logger_options.log_directory = options.log_directory;
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
    hl::common::Logger::Shutdown();
    return exit_code;
}
