#include "app/host_application.h"

#include <algorithm>
#include <exception>
#include <string>
#include <string_view>
#include <vector>

#include "common/logger.h"
#include "common/text_encoding.h"
#include "filesystem/valve_directory.h"
#include "game_api/hl_server_module.h"
#include "platform/environment.h"

namespace
{
std::wstring ToFoundMissing(bool found)
{
    return found ? L"found" : L"missing";
}

const char* ToLoadedFailed(bool loaded)
{
    return loaded ? "loaded" : "failed";
}

std::wstring DescribeValidationCheck(const hl::filesystem::ValidationCheckStatus& status)
{
    std::wstring message = status.label + L": " + ToFoundMissing(status.found);
    if (status.optional)
    {
        message += L" (optional)";
    }

    return message;
}

bool UsesFallbackPath(const hl::filesystem::ValidationCheckStatus& status)
{
    return status.found
        && status.checked_paths.size() > 1
        && status.resolved_path != status.checked_paths.front();
}

bool StartsWith(std::string_view value, std::string_view prefix)
{
    return value.size() >= prefix.size() && value.compare(0, prefix.size(), prefix) == 0;
}

bool ContainsText(std::string_view value, std::string_view token)
{
    return value.find(token) != std::string_view::npos;
}

std::string_view RegressionGuardProfileName(hl::app::RegressionGuardProfile profile)
{
    switch (profile)
    {
    case hl::app::RegressionGuardProfile::kTrainstop26TerminalProbe:
        return "trainstop26-terminal-probe";
    case hl::app::RegressionGuardProfile::kTrainstop26Baseline:
        return "trainstop26-baseline";
    case hl::app::RegressionGuardProfile::kChangelevelRequestConsumed:
        return "changelevel-request-consumed";
    }

    return "unknown";
}

void AddGuardFailure(
    std::vector<std::string>& failures,
    bool condition,
    std::string failure_text)
{
    if (!condition)
    {
        failures.push_back(std::move(failure_text));
    }
}

const hl::game_api::PathNodeMessageCanarySummary* FindPathNodeCanary(
    const hl::game_api::HlServerModuleSummary& summary,
    std::string_view node_name)
{
    const auto it = std::find_if(
        summary.scripted_movement.path_node_messages.canaries.begin(),
        summary.scripted_movement.path_node_messages.canaries.end(),
        [&](const hl::game_api::PathNodeMessageCanarySummary& canary)
        {
            return canary.node_name == node_name;
        });

    return it != summary.scripted_movement.path_node_messages.canaries.end() ? &(*it) : nullptr;
}

void ValidateTrainstop26TerminalState(
    const hl::game_api::HlServerModuleSummary& summary,
    std::vector<std::string>& failures)
{
    constexpr std::string_view kExpectedCurrentNode = "trainstop26";
    constexpr std::string_view kExpectedStoppedReason =
        "path completed at 'trainstop26' (dead end 'trainstop27')";

    AddGuardFailure(
        failures,
        summary.scripted_movement.ftruck_final_current == kExpectedCurrentNode,
        "expected ftruck_a final current=trainstop26");
    AddGuardFailure(
        failures,
        summary.scripted_movement.ftruck_final_next.empty(),
        "expected ftruck_a final next=<none>");
    AddGuardFailure(
        failures,
        StartsWith(summary.scripted_movement.delayed_ftruck_status, "completed "),
        "expected ftruck_a terminal status=completed");
    AddGuardFailure(
        failures,
        ContainsText(
            summary.scripted_movement.delayed_ftruck_status,
            std::string("stopped=") + std::string(kExpectedStoppedReason)),
        "expected ftruck_a stopped reason to remain terminal dead-end completion at trainstop26");
}

bool ValidateRegressionGuard(
    hl::app::RegressionGuardProfile profile,
    const hl::game_api::HlServerModuleSummary& summary)
{
    constexpr std::string_view kExpectedStoppedReason =
        "path completed at 'trainstop26' (dead end 'trainstop27')";

    std::vector<std::string> failures;
    switch (profile)
    {
    case hl::app::RegressionGuardProfile::kTrainstop26TerminalProbe:
        ValidateTrainstop26TerminalState(summary, failures);
        AddGuardFailure(
            failures,
            !summary.server_frame_loop.any_seh,
            "expected frame loop any_seh=no");
        AddGuardFailure(
            failures,
            summary.server_frame_loop.stopped_early,
            "expected stop-on-node probe to stop early");
        AddGuardFailure(
            failures,
            ContainsText(summary.server_frame_loop.stop_reason, "requested=trainstop26"),
            "expected frame loop stop reason to reference requested=trainstop26");
        AddGuardFailure(
            failures,
            ContainsText(summary.server_frame_loop.stop_reason, "status=completed"),
            "expected frame loop stop reason to report status=completed");
        AddGuardFailure(
            failures,
            ContainsText(
                summary.server_frame_loop.stop_reason,
                std::string("stopped=") + std::string(kExpectedStoppedReason)),
            "expected frame loop stop reason to report the terminal dead-end completion text");
        break;

    case hl::app::RegressionGuardProfile::kTrainstop26Baseline:
    {
        const hl::game_api::PathNodeMessageCanarySummary* execute_sci_canary =
            FindPathNodeCanary(summary, "trainstop9");
        const hl::game_api::PathNodeMessageCanarySummary* fade_out_canary =
            FindPathNodeCanary(summary, "trainstop11");

        ValidateTrainstop26TerminalState(summary, failures);
        AddGuardFailure(
            failures,
            summary.server_frame_loop.frames_requested == 1800,
            "expected baseline frames requested=1800");
        AddGuardFailure(
            failures,
            summary.server_frame_loop.frames_completed == 1800,
            "expected baseline frames completed=1800");
        AddGuardFailure(
            failures,
            !summary.server_frame_loop.stopped_early,
            "expected baseline to run through all requested frames");
        AddGuardFailure(
            failures,
            !summary.server_frame_loop.any_seh,
            "expected baseline any_seh=no");
        AddGuardFailure(
            failures,
            summary.scripted_movement.path_node_messages.deepest_node_reached == "trainstop26",
            "expected deepest reached node=trainstop26");
        AddGuardFailure(
            failures,
            execute_sci_canary != nullptr,
            "expected execute_sci canary at trainstop9 to remain present");
        AddGuardFailure(
            failures,
            execute_sci_canary != nullptr && execute_sci_canary->reached,
            "expected execute_sci reached=yes");
        AddGuardFailure(
            failures,
            execute_sci_canary != nullptr && execute_sci_canary->dispatch_result == "succeeded",
            "expected execute_sci dispatchResult=succeeded");
        AddGuardFailure(
            failures,
            fade_out_canary != nullptr,
            "expected fade_out canary at trainstop11 to remain present");
        AddGuardFailure(
            failures,
            fade_out_canary != nullptr && fade_out_canary->reached,
            "expected fade_out reached=yes");
        AddGuardFailure(
            failures,
            fade_out_canary != nullptr && fade_out_canary->dispatch_result == "succeeded",
            "expected fade_out dispatchResult=succeeded");
        AddGuardFailure(
            failures,
            fade_out_canary != nullptr && fade_out_canary->fade_channel_used,
            "expected fade_out to keep using staged ScreenFade semantics");
        AddGuardFailure(
            failures,
            fade_out_canary != nullptr
                && !fade_out_canary->presentation_semantics_summary.empty(),
            "expected fade_out semantics summary to remain present");
        AddGuardFailure(
            failures,
            fade_out_canary != nullptr
                && ContainsText(
                    fade_out_canary->presentation_semantics_summary,
                    "handled=server-side-presentation-semantics")
                && ContainsText(
                    fade_out_canary->presentation_semantics_summary,
                    "message=ScreenFade"),
            "expected fade_out semantics to remain the staged server-side ScreenFade path");
        break;
    }

    case hl::app::RegressionGuardProfile::kChangelevelRequestConsumed:
        AddGuardFailure(
            failures,
            !summary.server_frame_loop.any_seh,
            "expected changelevel consumed probe any_seh=no");
        AddGuardFailure(
            failures,
            summary.server_frame_loop.stopped_early,
            "expected changelevel consumed probe to stop early");
        AddGuardFailure(
            failures,
            ContainsText(
                summary.server_frame_loop.stop_reason,
                "stop-on-changelevel-request reached"),
            "expected frame loop stop reason to reference stop-on-changelevel-request");
        AddGuardFailure(
            failures,
            summary.changelevel_transition.pending_request_captured,
            "expected pending_changelevel_request capture to remain present");
        AddGuardFailure(
            failures,
            summary.changelevel_transition.transition_intent_captured,
            "expected changelevel_transition_intent captured=yes");
        AddGuardFailure(
            failures,
            summary.changelevel_transition.transition_intent_consumed,
            "expected changelevel_transition_intent consumed=yes");
        AddGuardFailure(
            failures,
            summary.changelevel_transition.target_map == "c0a0a",
            "expected changelevel requestedMap=c0a0a");
        AddGuardFailure(
            failures,
            summary.changelevel_transition.landmark == "c0a0toa",
            "expected changelevel landmark=c0a0toa");
        AddGuardFailure(
            failures,
            summary.changelevel_transition.transition_intent_request_frame >= 0,
            "expected changelevel requestFrame to be captured");
        AddGuardFailure(
            failures,
            summary.changelevel_transition.transition_intent_request_time > 0.0f,
            "expected changelevel requestTime to be captured");
        AddGuardFailure(
            failures,
            summary.changelevel_transition.transition_intent_action == "no-op transition stop",
            "expected changelevel action=no-op transition stop");
        AddGuardFailure(
            failures,
            summary.changelevel_transition.pre_changelevel_handoff.active,
            "expected pre_changelevel_handoff active=yes");
        AddGuardFailure(
            failures,
            summary.changelevel_transition.pre_changelevel_handoff.request_frame
                == summary.changelevel_transition.transition_intent_request_frame,
            "expected pre_changelevel_handoff requestFrame to match consumed intent");
        AddGuardFailure(
            failures,
            summary.changelevel_transition.pre_changelevel_handoff.request_time
                == summary.changelevel_transition.transition_intent_request_time,
            "expected pre_changelevel_handoff requestTime to match consumed intent");
        AddGuardFailure(
            failures,
            summary.changelevel_transition.pre_changelevel_handoff.world_state
                == "frozen|latched|handoff-ready",
            "expected pre_changelevel_handoff worldState=frozen|latched|handoff-ready");
        AddGuardFailure(
            failures,
            summary.changelevel_transition.pre_changelevel_handoff.action
                == "no-op handoff boundary",
            "expected pre_changelevel_handoff action=no-op handoff boundary");
        AddGuardFailure(
            failures,
            !summary.changelevel_transition.pre_changelevel_handoff.map_load_performed,
            "expected pre_changelevel_handoff mapLoad=no");
        break;
    }

    if (failures.empty())
    {
        hl::common::Logger::Info(
            hl::common::LogCategory::Summary,
            "Regression guard passed: " + std::string(RegressionGuardProfileName(profile)));
        return true;
    }

    hl::common::Logger::Error(
        hl::common::LogCategory::Summary,
        "Regression guard failed: " + std::string(RegressionGuardProfileName(profile)));
    for (const std::string& failure : failures)
    {
        hl::common::Logger::Error(hl::common::LogCategory::Summary, "  - " + failure);
    }

    return false;
}
} // namespace

namespace hl::app
{
int HostApplication::Run(const LaunchOptions& options) const
{
    try
    {
        common::Logger::Info(common::LogCategory::Startup, L"hlhost startup initiated.");

        const std::filesystem::path working_directory = platform::GetCurrentWorkingDirectory();
        const std::filesystem::path executable_path = platform::GetExecutablePath();

        common::Logger::Info(
            common::LogCategory::Startup,
            "Current working directory: " + common::ToUtf8(working_directory));
        common::Logger::Info(
            common::LogCategory::Startup,
            "Executable path: " + common::ToUtf8(executable_path));

        std::vector<std::filesystem::path> checked_candidates;
        const std::optional<std::filesystem::path> resolved_game_directory = filesystem::ResolveValveDirectory(
            file_system_,
            options.game_directory,
            executable_path,
            working_directory,
            &checked_candidates);

        if (!resolved_game_directory.has_value())
        {
            if (options.game_directory.has_value())
            {
                common::Logger::Error(
                    common::LogCategory::Filesystem,
                    "Provided --gamedir does not exist or is not a directory: "
                    + common::ToUtf8(file_system_.AbsolutePath(*options.game_directory)));
            }
            else
            {
                common::Logger::Error(
                    common::LogCategory::Filesystem,
                    L"Could not locate a valid 'valve' directory near the executable or project root.");
                LogCheckedCandidates(checked_candidates);
            }

            return 1;
        }

        const std::filesystem::path absolute_game_directory =
            file_system_.AbsolutePath(*resolved_game_directory);

        common::Logger::Info(
            common::LogCategory::Filesystem,
            "Using game directory: " + common::ToUtf8(absolute_game_directory));

        const filesystem::ValveDirectory valve_directory(absolute_game_directory);
        const filesystem::ValveDirectoryValidationResult validation = valve_directory.Validate(file_system_);
        LogValveValidationStatus(validation);

        if (!validation.IsValid())
        {
            common::Logger::Error(common::LogCategory::Filesystem, L"Valve directory validation failed.");
            for (const filesystem::ValidationIssue& issue : validation.issues)
            {
                common::Logger::Error(common::LogCategory::Filesystem, issue.message);
            }

            return 2;
        }

        common::Logger::Info(common::LogCategory::Filesystem, L"Valve directory validation succeeded.");
        if (!RunDllSmokeTest(absolute_game_directory))
        {
            common::Logger::Error(common::LogCategory::Dll, L"DLL smoke test failed.");
            return 3;
        }

        common::Logger::Info(common::LogCategory::Dll, L"DLL smoke test succeeded.");
        if (!RunServerEngineShim(absolute_game_directory, options))
        {
            common::Logger::Error(common::LogCategory::Server, L"hl.dll engine shim initialization failed.");
            return 4;
        }

        common::Logger::Info(common::LogCategory::Server, L"hl.dll engine shim initialization succeeded.");
        common::Logger::Info(
            common::LogCategory::Summary,
            L"Host foundation reached deterministic post-activation frame bootstrap stage.");

        return 0;
    }
    catch (const std::exception& error)
    {
        common::Logger::Error(
            common::LogCategory::Startup,
            std::string("Fatal startup error: ") + error.what());
        return 10;
    }
}

void HostApplication::LogValveValidationStatus(
    const filesystem::ValveDirectoryValidationResult& validation) const
{
    common::Logger::Info(common::LogCategory::Filesystem, DescribeValidationCheck(validation.valve_directory));
    common::Logger::Info(common::LogCategory::Filesystem, DescribeValidationCheck(validation.hl_dll));
    common::Logger::Info(common::LogCategory::Filesystem, DescribeValidationCheck(validation.client_dll));
    common::Logger::Info(common::LogCategory::Filesystem, DescribeValidationCheck(validation.pak0_pak));

    if (UsesFallbackPath(validation.client_dll))
    {
        common::Logger::Info(
            common::LogCategory::Filesystem,
            "client.dll fallback path selected: "
            + common::ToUtf8(validation.client_dll.resolved_path));
    }

    if (validation.pak0_pak.found)
    {
        common::Logger::Info(
            common::LogCategory::Filesystem,
            "pak0.pak detected but ignored for current validation: "
            + common::ToUtf8(validation.pak0_pak.resolved_path));
    }
}

void HostApplication::LogCheckedCandidates(const std::vector<std::filesystem::path>& candidates) const
{
    if (candidates.empty())
    {
        common::Logger::Warn(common::LogCategory::Filesystem, L"No automatic 'valve' candidates were produced.");
        return;
    }

    common::Logger::Warn(common::LogCategory::Filesystem, L"Checked candidate directories:");
    for (const std::filesystem::path& candidate : candidates)
    {
        common::Logger::Warn(common::LogCategory::Filesystem, "  - " + common::ToUtf8(candidate));
    }
}

bool HostApplication::RunDllSmokeTest(const std::filesystem::path& game_directory) const
{
    common::Logger::Info(common::LogCategory::Dll, L"Running DLL smoke test.");

    std::vector<game_api::GameModuleSmokeTestResult> results;
    results.reserve(game_api::GetDefaultGameModuleDescriptors().size());

    bool all_modules_ok = true;
    for (const game_api::GameModuleDescriptor& descriptor : game_api::GetDefaultGameModuleDescriptors())
    {
        results.push_back(module_loader_.SmokeTestModule(descriptor, game_directory));
        const game_api::GameModuleSmokeTestResult& result = results.back();
        const std::string display_name = common::ToUtf8(descriptor.display_name);

        std::string module_status_line = display_name + ": " + ToLoadedFailed(result.loaded);
        if (result.used_fallback)
        {
            module_status_line += " (fallback)";
        }

        common::Logger::Info(common::LogCategory::Dll, module_status_line);
        common::Logger::Info(
            common::LogCategory::Dll,
            display_name + " exports: "
            + std::to_string(result.FoundExportCount()) + "/"
            + std::to_string(result.RequiredExportCount()));

        if (!result.loaded_path.empty())
        {
            const std::string loaded_path_label = result.loaded
                ? display_name + " loaded path: "
                : display_name + " last attempted path: ";
            common::Logger::Info(common::LogCategory::Dll, loaded_path_label + common::ToUtf8(result.loaded_path));
        }

        if (!result.loaded && result.load_error_code != 0)
        {
            common::Logger::Warn(
                common::LogCategory::Dll,
                display_name + " load error: code "
                + std::to_string(result.load_error_code) + ", message: "
                + common::ToUtf8(result.load_error_message));
        }

        all_modules_ok = all_modules_ok && result.IsSuccessful();
    }

    common::Logger::Info(common::LogCategory::Summary, L"DLL smoke test summary:");
    common::Logger::Info(common::LogCategory::Summary, L"Valve directory: OK");
    for (const game_api::GameModuleSmokeTestResult& result : results)
    {
        const std::string display_name = common::ToUtf8(result.descriptor.display_name);

        std::string module_status_line = display_name + ": " + ToLoadedFailed(result.loaded);
        if (result.used_fallback)
        {
            module_status_line += " (fallback)";
        }

        common::Logger::Info(common::LogCategory::Summary, module_status_line);
        common::Logger::Info(
            common::LogCategory::Summary,
            display_name + " exports: "
            + std::to_string(result.FoundExportCount()) + "/"
            + std::to_string(result.RequiredExportCount()));
    }

    return all_modules_ok;
}

bool HostApplication::RunServerEngineShim(
    const std::filesystem::path& game_directory,
    const LaunchOptions& options) const
{
    common::Logger::Info(common::LogCategory::Server, L"Running hl.dll minimal engine shim.");

    game_api::HlServerModule server_module;
    const std::filesystem::path hl_dll_path = game_directory / L"dlls/hl.dll";
    if (!server_module.Load(hl_dll_path))
    {
        common::Logger::Error(
            common::LogCategory::Server,
            "Failed to load hl.dll for engine shim: " + common::ToUtf8(hl_dll_path));
        return false;
    }

    game_api::HlServerModuleInitOptions init_options;
    init_options.game_directory = game_directory;
    init_options.mod_name = "valve";
    init_options.map_name = options.map_name.has_value()
        ? common::ToUtf8(*options.map_name)
        : "c0a0";
    init_options.hostname = "HLengine Test Server";
    init_options.maxclients = 1;
    init_options.frame_bootstrap.frames = options.frame_count;
    init_options.frame_bootstrap.frametime = options.frame_time;
    init_options.frame_bootstrap.think_limit = options.think_limit;
    init_options.frame_bootstrap.use_limit = options.use_limit;
    init_options.frame_bootstrap.scheduled_use_limit = options.scheduled_use_limit;
    init_options.frame_bootstrap.path_arrival_epsilon = options.path_arrival_epsilon;
    init_options.frame_bootstrap.trace_scripted = options.trace_scripted;
    init_options.frame_bootstrap.trace_movement = options.trace_movement;
    init_options.frame_bootstrap.trace_think = options.trace_think;
    init_options.frame_bootstrap.trace_callbacks = options.trace_callbacks;
    init_options.frame_bootstrap.log_frame_sample = options.log_frame_sample;
    init_options.frame_bootstrap.log_state_changes_only = options.log_state_changes_only;
    init_options.frame_bootstrap.stop_on_first_message = options.stop_on_first_message;
    init_options.frame_bootstrap.stop_on_changelevel_request =
        options.stop_on_changelevel_request;
    init_options.frame_bootstrap.stop_on_node =
        options.stop_on_node.has_value() ? common::ToUtf8(*options.stop_on_node) : std::string();

    common::Logger::Info(
        common::LogCategory::Startup,
        "Frame bootstrap config: frames=" + std::to_string(init_options.frame_bootstrap.frames)
        + ", frametime=" + std::to_string(init_options.frame_bootstrap.frametime)
        + ", think_limit=" + std::to_string(init_options.frame_bootstrap.think_limit)
        + ", use_limit=" + std::to_string(init_options.frame_bootstrap.use_limit)
        + ", scheduled_use_limit="
        + std::to_string(init_options.frame_bootstrap.scheduled_use_limit)
        + ", path_arrival_epsilon="
        + std::to_string(init_options.frame_bootstrap.path_arrival_epsilon)
        + ", trace_scripted="
        + std::string(init_options.frame_bootstrap.trace_scripted ? "1" : "0")
        + ", trace_movement="
        + std::string(init_options.frame_bootstrap.trace_movement ? "1" : "0")
        + ", trace_think="
        + std::string(init_options.frame_bootstrap.trace_think ? "1" : "0")
        + ", trace_callbacks="
        + std::string(init_options.frame_bootstrap.trace_callbacks ? "1" : "0")
        + ", log_frame_sample="
        + std::to_string(init_options.frame_bootstrap.log_frame_sample)
        + ", log_state_changes_only="
        + std::string(init_options.frame_bootstrap.log_state_changes_only ? "1" : "0")
        + ", stop_on_first_message="
        + std::string(init_options.frame_bootstrap.stop_on_first_message ? "1" : "0")
        + ", stop_on_changelevel_request="
        + std::string(init_options.frame_bootstrap.stop_on_changelevel_request ? "1" : "0")
        + ", stop_on_node="
        + (init_options.frame_bootstrap.stop_on_node.empty()
            ? std::string("<none>")
            : init_options.frame_bootstrap.stop_on_node));

    if (!server_module.InitializeEngineShim(init_options))
    {
        return false;
    }

    if (options.regression_guard.has_value()
        && !ValidateRegressionGuard(*options.regression_guard, server_module.Summary()))
    {
        return false;
    }

    return true;
}
} // namespace hl::app
