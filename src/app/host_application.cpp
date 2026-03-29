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
    case hl::app::RegressionGuardProfile::kChangelevelLatchOnlyContinuation:
        return "changelevel-latch-only-continuation";
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

void ValidateChangelevelTargetValidation(
    const hl::game_api::HlServerModuleSummary& summary,
    std::vector<std::string>& failures)
{
    AddGuardFailure(
        failures,
        summary.changelevel_transition.target_validation.attempted,
        "expected changelevel_target_validation attempted=yes");
    AddGuardFailure(
        failures,
        summary.changelevel_transition.target_validation.current_map == "c0a0",
        "expected changelevel_target_validation currentMap=c0a0");
    AddGuardFailure(
        failures,
        summary.changelevel_transition.target_validation.requested_map == "c0a0a",
        "expected changelevel_target_validation requestedMap=c0a0a");
    AddGuardFailure(
        failures,
        summary.changelevel_transition.target_validation.landmark == "c0a0toa",
        "expected changelevel_target_validation landmark=c0a0toa");
    AddGuardFailure(
        failures,
        summary.changelevel_transition.target_validation.target_map_exists,
        "expected changelevel_target_validation targetMapExists=yes");
    AddGuardFailure(
        failures,
        summary.changelevel_transition.target_validation.current_landmark_found,
        "expected changelevel_target_validation currentLandmark=yes");
    AddGuardFailure(
        failures,
        summary.changelevel_transition.target_validation.target_landmark_found,
        "expected changelevel_target_validation targetLandmark=yes");
    AddGuardFailure(
        failures,
        summary.changelevel_transition.target_validation.entity_parse_succeeded,
        "expected changelevel_target_validation entityParse=ok");
    AddGuardFailure(
        failures,
        summary.changelevel_transition.target_validation.action == "no-op dry-run validation",
        "expected changelevel_target_validation action=no-op dry-run validation");
}

void ValidateChangelevelLifecycleGate(
    const hl::game_api::HlServerModuleSummary& summary,
    std::vector<std::string>& failures)
{
    AddGuardFailure(
        failures,
        summary.changelevel_transition.lifecycle_gate.attempted,
        "expected changelevel_lifecycle_gate attempted=yes");
    AddGuardFailure(
        failures,
        summary.changelevel_transition.lifecycle_gate.intent_consumed,
        "expected changelevel_lifecycle_gate intentConsumed=yes");
    AddGuardFailure(
        failures,
        summary.changelevel_transition.lifecycle_gate.target_validation_passed,
        "expected changelevel_lifecycle_gate targetValidation=yes");
    AddGuardFailure(
        failures,
        summary.changelevel_transition.lifecycle_gate.bootstrap_allowed,
        "expected changelevel_lifecycle_gate bootstrapAllowed=yes");
    AddGuardFailure(
        failures,
        summary.changelevel_transition.lifecycle_gate.requested_map == "c0a0a",
        "expected changelevel_lifecycle_gate requestedMap=c0a0a");
    AddGuardFailure(
        failures,
        summary.changelevel_transition.lifecycle_gate.landmark == "c0a0toa",
        "expected changelevel_lifecycle_gate landmark=c0a0toa");
    AddGuardFailure(
        failures,
        summary.changelevel_transition.lifecycle_gate.action == "no-op gated-ready",
        "expected changelevel_lifecycle_gate action=no-op gated-ready");
}

void ValidateChangelevelLifecycleEntry(
    const hl::game_api::HlServerModuleSummary& summary,
    bool expected_eligible,
    bool expected_blocked_by_stop_mode,
    std::string_view expected_action,
    std::vector<std::string>& failures)
{
    AddGuardFailure(
        failures,
        summary.changelevel_transition.lifecycle_entry.attempted,
        "expected changelevel_lifecycle_entry attempted=yes");
    AddGuardFailure(
        failures,
        summary.changelevel_transition.lifecycle_entry.gate_checked,
        "expected changelevel_lifecycle_entry gateChecked=yes");
    AddGuardFailure(
        failures,
        summary.changelevel_transition.lifecycle_entry.gate_passed,
        "expected changelevel_lifecycle_entry gatePassed=yes");
    AddGuardFailure(
        failures,
        summary.changelevel_transition.lifecycle_entry.eligible == expected_eligible,
        std::string("expected changelevel_lifecycle_entry eligible=")
            + (expected_eligible ? "yes" : "no"));
    AddGuardFailure(
        failures,
        summary.changelevel_transition.lifecycle_entry.blocked_by_stop_mode
            == expected_blocked_by_stop_mode,
        std::string("expected changelevel_lifecycle_entry blockedByStopMode=")
            + (expected_blocked_by_stop_mode ? "yes" : "no"));
    AddGuardFailure(
        failures,
        summary.changelevel_transition.lifecycle_entry.requested_map == "c0a0a",
        "expected changelevel_lifecycle_entry requestedMap=c0a0a");
    AddGuardFailure(
        failures,
        summary.changelevel_transition.lifecycle_entry.landmark == "c0a0toa",
        "expected changelevel_lifecycle_entry landmark=c0a0toa");
    AddGuardFailure(
        failures,
        summary.changelevel_transition.lifecycle_entry.action == expected_action,
        std::string("expected changelevel_lifecycle_entry action=") + std::string(expected_action));
}

void ValidateChangelevelLifecycleDispatch(
    const hl::game_api::HlServerModuleSummary& summary,
    bool expected_allowed,
    bool expected_blocked,
    std::string_view expected_action,
    std::string_view expected_short_circuit_reason,
    std::vector<std::string>& failures)
{
    AddGuardFailure(
        failures,
        summary.changelevel_transition.lifecycle_dispatch.attempted,
        "expected changelevel_lifecycle_dispatch attempted=yes");
    AddGuardFailure(
        failures,
        summary.changelevel_transition.lifecycle_dispatch.dispatch_checked,
        "expected changelevel_lifecycle_dispatch dispatchChecked=yes");
    AddGuardFailure(
        failures,
        summary.changelevel_transition.lifecycle_dispatch.dispatch_allowed == expected_allowed,
        std::string("expected changelevel_lifecycle_dispatch dispatchAllowed=")
            + (expected_allowed ? "yes" : "no"));
    AddGuardFailure(
        failures,
        summary.changelevel_transition.lifecycle_dispatch.dispatch_blocked == expected_blocked,
        std::string("expected changelevel_lifecycle_dispatch dispatchBlocked=")
            + (expected_blocked ? "yes" : "no"));
    AddGuardFailure(
        failures,
        summary.changelevel_transition.lifecycle_dispatch.decision_source
            == "changelevel_lifecycle_entry",
        "expected changelevel_lifecycle_dispatch decisionSource=changelevel_lifecycle_entry");
    AddGuardFailure(
        failures,
        summary.changelevel_transition.lifecycle_dispatch.requested_map == "c0a0a",
        "expected changelevel_lifecycle_dispatch requestedMap=c0a0a");
    AddGuardFailure(
        failures,
        summary.changelevel_transition.lifecycle_dispatch.landmark == "c0a0toa",
        "expected changelevel_lifecycle_dispatch landmark=c0a0toa");
    AddGuardFailure(
        failures,
        summary.changelevel_transition.lifecycle_dispatch.action == expected_action,
        std::string("expected changelevel_lifecycle_dispatch action=")
            + std::string(expected_action));
    AddGuardFailure(
        failures,
        summary.changelevel_transition.lifecycle_dispatch.short_circuit_reason
            == expected_short_circuit_reason,
        std::string("expected changelevel_lifecycle_dispatch shortCircuitReason=")
            + (expected_short_circuit_reason.empty()
                ? std::string("<empty>")
                : std::string(expected_short_circuit_reason)));
}

void ValidateChangelevelLifecycleExecution(
    const hl::game_api::HlServerModuleSummary& summary,
    bool expected_armed,
    bool expected_skipped,
    std::string_view expected_action,
    std::string_view expected_short_circuit_reason,
    std::vector<std::string>& failures)
{
    AddGuardFailure(
        failures,
        summary.changelevel_transition.lifecycle_execution.attempted,
        "expected changelevel_lifecycle_execution attempted=yes");
    AddGuardFailure(
        failures,
        summary.changelevel_transition.lifecycle_execution.execution_checked,
        "expected changelevel_lifecycle_execution executionChecked=yes");
    AddGuardFailure(
        failures,
        summary.changelevel_transition.lifecycle_execution.execution_armed == expected_armed,
        std::string("expected changelevel_lifecycle_execution executionArmed=")
            + (expected_armed ? "yes" : "no"));
    AddGuardFailure(
        failures,
        summary.changelevel_transition.lifecycle_execution.execution_skipped == expected_skipped,
        std::string("expected changelevel_lifecycle_execution executionSkipped=")
            + (expected_skipped ? "yes" : "no"));
    AddGuardFailure(
        failures,
        summary.changelevel_transition.lifecycle_execution.decision_source
            == "changelevel_lifecycle_dispatch",
        "expected changelevel_lifecycle_execution decisionSource=changelevel_lifecycle_dispatch");
    AddGuardFailure(
        failures,
        summary.changelevel_transition.lifecycle_execution.requested_map == "c0a0a",
        "expected changelevel_lifecycle_execution requestedMap=c0a0a");
    AddGuardFailure(
        failures,
        summary.changelevel_transition.lifecycle_execution.landmark == "c0a0toa",
        "expected changelevel_lifecycle_execution landmark=c0a0toa");
    AddGuardFailure(
        failures,
        summary.changelevel_transition.lifecycle_execution.action == expected_action,
        std::string("expected changelevel_lifecycle_execution action=")
            + std::string(expected_action));
    AddGuardFailure(
        failures,
        summary.changelevel_transition.lifecycle_execution.short_circuit_reason
            == expected_short_circuit_reason,
        std::string("expected changelevel_lifecycle_execution shortCircuitReason=")
            + (expected_short_circuit_reason.empty()
                ? std::string("<empty>")
                : std::string(expected_short_circuit_reason)));
}

void ValidateChangelevelBootstrapPlan(
    const hl::game_api::HlServerModuleSummary& summary,
    bool expected_prepared,
    bool expected_skipped,
    std::string_view expected_action,
    std::string_view expected_short_circuit_reason,
    std::vector<std::string>& failures)
{
    AddGuardFailure(
        failures,
        summary.changelevel_transition.changelevel_bootstrap_plan.attempted,
        "expected changelevel_bootstrap_plan attempted=yes");
    AddGuardFailure(
        failures,
        summary.changelevel_transition.changelevel_bootstrap_plan.prepared == expected_prepared,
        std::string("expected changelevel_bootstrap_plan prepared=")
            + (expected_prepared ? "yes" : "no"));
    AddGuardFailure(
        failures,
        summary.changelevel_transition.changelevel_bootstrap_plan.skipped == expected_skipped,
        std::string("expected changelevel_bootstrap_plan skipped=")
            + (expected_skipped ? "yes" : "no"));
    AddGuardFailure(
        failures,
        summary.changelevel_transition.changelevel_bootstrap_plan.decision_source
            == "changelevel_lifecycle_execution",
        "expected changelevel_bootstrap_plan decisionSource=changelevel_lifecycle_execution");
    AddGuardFailure(
        failures,
        summary.changelevel_transition.changelevel_bootstrap_plan.action == expected_action,
        std::string("expected changelevel_bootstrap_plan action=")
            + std::string(expected_action));
    AddGuardFailure(
        failures,
        summary.changelevel_transition.changelevel_bootstrap_plan.short_circuit_reason
            == expected_short_circuit_reason,
        std::string("expected changelevel_bootstrap_plan shortCircuitReason=")
            + (expected_short_circuit_reason.empty()
                ? std::string("<empty>")
                : std::string(expected_short_circuit_reason)));

    if (!expected_prepared)
    {
        return;
    }

    AddGuardFailure(
        failures,
        summary.changelevel_transition.changelevel_bootstrap_plan.current_map == "c0a0",
        "expected changelevel_bootstrap_plan currentMap=c0a0");
    AddGuardFailure(
        failures,
        summary.changelevel_transition.changelevel_bootstrap_plan.requested_map == "c0a0a",
        "expected changelevel_bootstrap_plan requestedMap=c0a0a");
    AddGuardFailure(
        failures,
        summary.changelevel_transition.changelevel_bootstrap_plan.landmark == "c0a0toa",
        "expected changelevel_bootstrap_plan landmark=c0a0toa");
    AddGuardFailure(
        failures,
        ContainsText(
            summary.changelevel_transition.changelevel_bootstrap_plan.target_bsp_path,
            "c0a0a.bsp"),
        "expected changelevel_bootstrap_plan targetBspPath to reference c0a0a.bsp");
    AddGuardFailure(
        failures,
        summary.changelevel_transition.changelevel_bootstrap_plan.target_worldspawn_present,
        "expected changelevel_bootstrap_plan targetWorldspawnPresent=yes");
    AddGuardFailure(
        failures,
        summary.changelevel_transition.changelevel_bootstrap_plan.target_entity_parse_ok,
        "expected changelevel_bootstrap_plan targetEntityParse=ok");
}

void ValidateChangelevelLandmarkTransform(
    const hl::game_api::HlServerModuleSummary& summary,
    bool expected_computed,
    bool expected_skipped,
    std::string_view expected_action,
    std::string_view expected_short_circuit_reason,
    std::vector<std::string>& failures)
{
    AddGuardFailure(
        failures,
        summary.changelevel_transition.changelevel_landmark_transform.attempted,
        "expected changelevel_landmark_transform attempted=yes");
    AddGuardFailure(
        failures,
        summary.changelevel_transition.changelevel_landmark_transform.transform_computed
            == expected_computed,
        std::string("expected changelevel_landmark_transform transformComputed=")
            + (expected_computed ? "yes" : "no"));
    AddGuardFailure(
        failures,
        summary.changelevel_transition.changelevel_landmark_transform.skipped == expected_skipped,
        std::string("expected changelevel_landmark_transform skipped=")
            + (expected_skipped ? "yes" : "no"));
    AddGuardFailure(
        failures,
        summary.changelevel_transition.changelevel_landmark_transform.decision_source
            == "changelevel_bootstrap_plan",
        "expected changelevel_landmark_transform decisionSource=changelevel_bootstrap_plan");
    AddGuardFailure(
        failures,
        summary.changelevel_transition.changelevel_landmark_transform.action == expected_action,
        std::string("expected changelevel_landmark_transform action=")
            + std::string(expected_action));
    AddGuardFailure(
        failures,
        summary.changelevel_transition.changelevel_landmark_transform.short_circuit_reason
            == expected_short_circuit_reason,
        std::string("expected changelevel_landmark_transform shortCircuitReason=")
            + (expected_short_circuit_reason.empty()
                ? std::string("<empty>")
                : std::string(expected_short_circuit_reason)));

    if (!expected_computed)
    {
        return;
    }

    AddGuardFailure(
        failures,
        summary.changelevel_transition.changelevel_landmark_transform.current_landmark_origin
            == "-2345 -2292 624",
        "expected changelevel_landmark_transform currentLandmarkOrigin=-2345 -2292 624");
    AddGuardFailure(
        failures,
        summary.changelevel_transition.changelevel_landmark_transform.target_landmark_origin
            == "-2345 -1476 175",
        "expected changelevel_landmark_transform targetLandmarkOrigin=-2345 -1476 175");
    AddGuardFailure(
        failures,
        summary.changelevel_transition.changelevel_landmark_transform.translation_delta
            == "0 816 -449",
        "expected changelevel_landmark_transform translationDelta=0 816 -449");
}

void ValidateChangelevelProjectedCarriedOrigin(
    const hl::game_api::HlServerModuleSummary& summary,
    bool expected_projected,
    bool expected_skipped,
    std::string_view expected_action,
    std::string_view expected_short_circuit_reason,
    std::vector<std::string>& failures)
{
    AddGuardFailure(
        failures,
        summary.changelevel_transition.changelevel_projected_carried_origin.attempted,
        "expected changelevel_projected_carried_origin attempted=yes");
    AddGuardFailure(
        failures,
        summary.changelevel_transition.changelevel_projected_carried_origin.projected
            == expected_projected,
        std::string("expected changelevel_projected_carried_origin projected=")
            + (expected_projected ? "yes" : "no"));
    AddGuardFailure(
        failures,
        summary.changelevel_transition.changelevel_projected_carried_origin.skipped
            == expected_skipped,
        std::string("expected changelevel_projected_carried_origin skipped=")
            + (expected_skipped ? "yes" : "no"));
    AddGuardFailure(
        failures,
        summary.changelevel_transition.changelevel_projected_carried_origin.decision_source
            == "changelevel_landmark_transform",
        "expected changelevel_projected_carried_origin decisionSource=changelevel_landmark_transform");
    AddGuardFailure(
        failures,
        summary.changelevel_transition.changelevel_projected_carried_origin.action
            == expected_action,
        std::string("expected changelevel_projected_carried_origin action=")
            + std::string(expected_action));
    AddGuardFailure(
        failures,
        summary.changelevel_transition.changelevel_projected_carried_origin.short_circuit_reason
            == expected_short_circuit_reason,
        std::string("expected changelevel_projected_carried_origin shortCircuitReason=")
            + (expected_short_circuit_reason.empty()
                ? std::string("<empty>")
                : std::string(expected_short_circuit_reason)));

    if (!expected_projected)
    {
        return;
    }

    AddGuardFailure(
        failures,
        summary.changelevel_transition.changelevel_projected_carried_origin
            .current_carried_origin == "-2687 -2158.35 514",
        "expected changelevel_projected_carried_origin currentCarriedOrigin=-2687 -2158.35 514");
    AddGuardFailure(
        failures,
        summary.changelevel_transition.changelevel_projected_carried_origin
            .translation_delta == "0 816 -449",
        "expected changelevel_projected_carried_origin translationDelta=0 816 -449");
    AddGuardFailure(
        failures,
        summary.changelevel_transition.changelevel_projected_carried_origin
            .projected_target_origin == "-2687 -1342.35 65",
        "expected changelevel_projected_carried_origin projectedTargetOrigin=-2687 -1342.35 65");
}

void ValidateChangelevelProjectedCarriedOrientation(
    const hl::game_api::HlServerModuleSummary& summary,
    bool expected_projected,
    bool expected_skipped,
    std::string_view expected_action,
    std::string_view expected_short_circuit_reason,
    std::vector<std::string>& failures)
{
    AddGuardFailure(
        failures,
        summary.changelevel_transition.changelevel_projected_carried_orientation.attempted,
        "expected changelevel_projected_carried_orientation attempted=yes");
    AddGuardFailure(
        failures,
        summary.changelevel_transition.changelevel_projected_carried_orientation.projected
            == expected_projected,
        std::string("expected changelevel_projected_carried_orientation projected=")
            + (expected_projected ? "yes" : "no"));
    AddGuardFailure(
        failures,
        summary.changelevel_transition.changelevel_projected_carried_orientation.skipped
            == expected_skipped,
        std::string("expected changelevel_projected_carried_orientation skipped=")
            + (expected_skipped ? "yes" : "no"));
    AddGuardFailure(
        failures,
        summary.changelevel_transition.changelevel_projected_carried_orientation.decision_source
            == "changelevel_landmark_transform",
        "expected changelevel_projected_carried_orientation decisionSource=changelevel_landmark_transform");
    AddGuardFailure(
        failures,
        summary.changelevel_transition.changelevel_projected_carried_orientation.action
            == expected_action,
        std::string("expected changelevel_projected_carried_orientation action=")
            + std::string(expected_action));
    AddGuardFailure(
        failures,
        summary.changelevel_transition.changelevel_projected_carried_orientation.short_circuit_reason
            == expected_short_circuit_reason,
        std::string("expected changelevel_projected_carried_orientation shortCircuitReason=")
            + (expected_short_circuit_reason.empty()
                ? std::string("<empty>")
                : std::string(expected_short_circuit_reason)));

    if (!expected_projected)
    {
        return;
    }

    AddGuardFailure(
        failures,
        summary.changelevel_transition.changelevel_projected_carried_orientation
            .current_carried_yaw_available,
        "expected changelevel_projected_carried_orientation currentCarriedYaw available");
    AddGuardFailure(
        failures,
        summary.changelevel_transition.changelevel_projected_carried_orientation
            .current_landmark_angles_available,
        "expected changelevel_projected_carried_orientation currentLandmarkAngles available");
    AddGuardFailure(
        failures,
        summary.changelevel_transition.changelevel_projected_carried_orientation
            .target_landmark_angles_available,
        "expected changelevel_projected_carried_orientation targetLandmarkAngles available");
    AddGuardFailure(
        failures,
        !summary.changelevel_transition.changelevel_projected_carried_orientation.yaw_delta.empty(),
        "expected changelevel_projected_carried_orientation yawDelta to be present");
    AddGuardFailure(
        failures,
        !summary.changelevel_transition.changelevel_projected_carried_orientation
             .projected_target_yaw.empty(),
        "expected changelevel_projected_carried_orientation projectedTargetYaw to be present");
}

void ValidateChangelevelProjectedTransferSnapshot(
    const hl::game_api::HlServerModuleSummary& summary,
    bool expected_prepared,
    bool expected_skipped,
    bool expected_transfer_ready,
    std::string_view expected_action,
    std::string_view expected_short_circuit_reason,
    std::vector<std::string>& failures)
{
    AddGuardFailure(
        failures,
        summary.changelevel_transition.changelevel_projected_transfer_snapshot.attempted,
        "expected changelevel_projected_transfer_snapshot attempted=yes");
    AddGuardFailure(
        failures,
        summary.changelevel_transition.changelevel_projected_transfer_snapshot.prepared
            == expected_prepared,
        std::string("expected changelevel_projected_transfer_snapshot prepared=")
            + (expected_prepared ? "yes" : "no"));
    AddGuardFailure(
        failures,
        summary.changelevel_transition.changelevel_projected_transfer_snapshot.skipped
            == expected_skipped,
        std::string("expected changelevel_projected_transfer_snapshot skipped=")
            + (expected_skipped ? "yes" : "no"));
    const std::string_view expected_decision_source =
        expected_prepared ? std::string_view("projected-carried-artifacts") : std::string_view();
    AddGuardFailure(
        failures,
        summary.changelevel_transition.changelevel_projected_transfer_snapshot.decision_source
            == expected_decision_source,
        std::string("expected changelevel_projected_transfer_snapshot decisionSource=")
            + (expected_decision_source.empty()
                ? std::string("<empty>")
                : std::string(expected_decision_source)));
    AddGuardFailure(
        failures,
        summary.changelevel_transition.changelevel_projected_transfer_snapshot.transfer_ready
            == expected_transfer_ready,
        std::string("expected changelevel_projected_transfer_snapshot transferReady=")
            + (expected_transfer_ready ? "yes" : "no"));
    AddGuardFailure(
        failures,
        summary.changelevel_transition.changelevel_projected_transfer_snapshot.action
            == expected_action,
        std::string("expected changelevel_projected_transfer_snapshot action=")
            + std::string(expected_action));
    AddGuardFailure(
        failures,
        summary.changelevel_transition.changelevel_projected_transfer_snapshot
            .short_circuit_reason
            == expected_short_circuit_reason,
        std::string("expected changelevel_projected_transfer_snapshot shortCircuitReason=")
            + (expected_short_circuit_reason.empty()
                ? std::string("<empty>")
                : std::string(expected_short_circuit_reason)));

    if (!expected_transfer_ready)
    {
        return;
    }

    AddGuardFailure(
        failures,
        summary.changelevel_transition.changelevel_projected_transfer_snapshot.current_map
            == "c0a0",
        "expected changelevel_projected_transfer_snapshot currentMap=c0a0");
    AddGuardFailure(
        failures,
        summary.changelevel_transition.changelevel_projected_transfer_snapshot.requested_map
            == "c0a0a",
        "expected changelevel_projected_transfer_snapshot requestedMap=c0a0a");
    AddGuardFailure(
        failures,
        summary.changelevel_transition.changelevel_projected_transfer_snapshot.landmark
            == "c0a0toa",
        "expected changelevel_projected_transfer_snapshot landmark=c0a0toa");
    AddGuardFailure(
        failures,
        ContainsText(
            summary.changelevel_transition.changelevel_projected_transfer_snapshot
                .target_bsp_path,
            "c0a0a.bsp"),
        "expected changelevel_projected_transfer_snapshot targetBspPath to reference c0a0a.bsp");
    AddGuardFailure(
        failures,
        summary.changelevel_transition.changelevel_projected_transfer_snapshot
            .projected_target_origin
            == "-2687 -1342.35 65",
        "expected changelevel_projected_transfer_snapshot projectedTargetOrigin=-2687 -1342.35 65");
    AddGuardFailure(
        failures,
        summary.changelevel_transition.changelevel_projected_transfer_snapshot
            .projected_target_yaw
            == "321.622406",
        "expected changelevel_projected_transfer_snapshot projectedTargetYaw=321.622406");
    AddGuardFailure(
        failures,
        summary.changelevel_transition.changelevel_projected_transfer_snapshot
            .target_worldspawn_present,
        "expected changelevel_projected_transfer_snapshot targetWorldspawnPresent=yes");
    AddGuardFailure(
        failures,
        summary.changelevel_transition.changelevel_projected_transfer_snapshot
            .target_entity_parse_ok,
        "expected changelevel_projected_transfer_snapshot targetEntityParse=ok");
}

void ValidateChangelevelPlayerTransferApplyPlan(
    const hl::game_api::HlServerModuleSummary& summary,
    bool expected_prepared,
    bool expected_skipped,
    bool expected_apply_ready,
    std::string_view expected_action,
    std::string_view expected_short_circuit_reason,
    std::vector<std::string>& failures)
{
    AddGuardFailure(
        failures,
        summary.changelevel_transition.changelevel_player_transfer_apply_plan.attempted,
        "expected changelevel_player_transfer_apply_plan attempted=yes");
    AddGuardFailure(
        failures,
        summary.changelevel_transition.changelevel_player_transfer_apply_plan.prepared
            == expected_prepared,
        std::string("expected changelevel_player_transfer_apply_plan prepared=")
            + (expected_prepared ? "yes" : "no"));
    AddGuardFailure(
        failures,
        summary.changelevel_transition.changelevel_player_transfer_apply_plan.skipped
            == expected_skipped,
        std::string("expected changelevel_player_transfer_apply_plan skipped=")
            + (expected_skipped ? "yes" : "no"));
    const std::string_view expected_decision_source =
        expected_prepared ? std::string_view("projected-transfer-snapshot")
                          : std::string_view();
    AddGuardFailure(
        failures,
        summary.changelevel_transition.changelevel_player_transfer_apply_plan.decision_source
            == expected_decision_source,
        std::string("expected changelevel_player_transfer_apply_plan decisionSource=")
            + (expected_decision_source.empty()
                ? std::string("<empty>")
                : std::string(expected_decision_source)));
    const std::string_view expected_apply_target =
        expected_prepared ? std::string_view("player") : std::string_view();
    AddGuardFailure(
        failures,
        summary.changelevel_transition.changelevel_player_transfer_apply_plan.apply_target
            == expected_apply_target,
        std::string("expected changelevel_player_transfer_apply_plan applyTarget=")
            + (expected_apply_target.empty()
                ? std::string("<empty>")
                : std::string(expected_apply_target)));
    AddGuardFailure(
        failures,
        summary.changelevel_transition.changelevel_player_transfer_apply_plan.apply_ready
            == expected_apply_ready,
        std::string("expected changelevel_player_transfer_apply_plan applyReady=")
            + (expected_apply_ready ? "yes" : "no"));
    AddGuardFailure(
        failures,
        summary.changelevel_transition.changelevel_player_transfer_apply_plan.action
            == expected_action,
        std::string("expected changelevel_player_transfer_apply_plan action=")
            + std::string(expected_action));
    AddGuardFailure(
        failures,
        summary.changelevel_transition.changelevel_player_transfer_apply_plan
            .short_circuit_reason
            == expected_short_circuit_reason,
        std::string("expected changelevel_player_transfer_apply_plan shortCircuitReason=")
            + (expected_short_circuit_reason.empty()
                ? std::string("<empty>")
                : std::string(expected_short_circuit_reason)));

    if (!expected_apply_ready)
    {
        return;
    }

    AddGuardFailure(
        failures,
        summary.changelevel_transition.changelevel_player_transfer_apply_plan.current_map
            == "c0a0",
        "expected changelevel_player_transfer_apply_plan currentMap=c0a0");
    AddGuardFailure(
        failures,
        summary.changelevel_transition.changelevel_player_transfer_apply_plan.requested_map
            == "c0a0a",
        "expected changelevel_player_transfer_apply_plan requestedMap=c0a0a");
    AddGuardFailure(
        failures,
        summary.changelevel_transition.changelevel_player_transfer_apply_plan.landmark
            == "c0a0toa",
        "expected changelevel_player_transfer_apply_plan landmark=c0a0toa");
    AddGuardFailure(
        failures,
        ContainsText(
            summary.changelevel_transition.changelevel_player_transfer_apply_plan
                .target_bsp_path,
            "c0a0a.bsp"),
        "expected changelevel_player_transfer_apply_plan targetBspPath to reference c0a0a.bsp");
    AddGuardFailure(
        failures,
        summary.changelevel_transition.changelevel_player_transfer_apply_plan
            .target_player_origin
            == "-2687 -1342.35 65",
        "expected changelevel_player_transfer_apply_plan targetPlayerOrigin=-2687 -1342.35 65");
    AddGuardFailure(
        failures,
        summary.changelevel_transition.changelevel_player_transfer_apply_plan
            .target_player_yaw
            == "321.622406",
        "expected changelevel_player_transfer_apply_plan targetPlayerYaw=321.622406");
    AddGuardFailure(
        failures,
        summary.changelevel_transition.changelevel_player_transfer_apply_plan
            .origin_write_prepared,
        "expected changelevel_player_transfer_apply_plan originWritePrepared=yes");
    AddGuardFailure(
        failures,
        summary.changelevel_transition.changelevel_player_transfer_apply_plan
            .yaw_write_prepared,
        "expected changelevel_player_transfer_apply_plan yawWritePrepared=yes");
    AddGuardFailure(
        failures,
        !summary.changelevel_transition.changelevel_player_transfer_apply_plan
             .inventory_write_prepared,
        "expected changelevel_player_transfer_apply_plan inventoryWritePrepared=no");
    AddGuardFailure(
        failures,
        !summary.changelevel_transition.changelevel_player_transfer_apply_plan
             .velocity_write_prepared,
        "expected changelevel_player_transfer_apply_plan velocityWritePrepared=no");
    AddGuardFailure(
        failures,
        summary.changelevel_transition.changelevel_player_transfer_apply_plan
            .target_worldspawn_present,
        "expected changelevel_player_transfer_apply_plan targetWorldspawnPresent=yes");
    AddGuardFailure(
        failures,
        summary.changelevel_transition.changelevel_player_transfer_apply_plan
            .target_entity_parse_ok,
        "expected changelevel_player_transfer_apply_plan targetEntityParse=ok");
}

void ValidateChangelevelPlayerTransferWriteSet(
    const hl::game_api::HlServerModuleSummary& summary,
    bool expected_prepared,
    bool expected_skipped,
    bool expected_write_set_ready,
    std::string_view expected_action,
    std::string_view expected_short_circuit_reason,
    std::vector<std::string>& failures)
{
    AddGuardFailure(
        failures,
        summary.changelevel_transition.changelevel_player_transfer_write_set.attempted,
        "expected changelevel_player_transfer_write_set attempted=yes");
    AddGuardFailure(
        failures,
        summary.changelevel_transition.changelevel_player_transfer_write_set.prepared
            == expected_prepared,
        std::string("expected changelevel_player_transfer_write_set prepared=")
            + (expected_prepared ? "yes" : "no"));
    AddGuardFailure(
        failures,
        summary.changelevel_transition.changelevel_player_transfer_write_set.skipped
            == expected_skipped,
        std::string("expected changelevel_player_transfer_write_set skipped=")
            + (expected_skipped ? "yes" : "no"));
    const std::string_view expected_decision_source =
        expected_write_set_ready ? std::string_view("player-transfer-apply-plan")
                                 : std::string_view();
    AddGuardFailure(
        failures,
        summary.changelevel_transition.changelevel_player_transfer_write_set.decision_source
            == expected_decision_source,
        std::string("expected changelevel_player_transfer_write_set decisionSource=")
            + (expected_decision_source.empty()
                ? std::string("<empty>")
                : std::string(expected_decision_source)));
    const std::string_view expected_apply_target =
        expected_write_set_ready ? std::string_view("player") : std::string_view();
    AddGuardFailure(
        failures,
        summary.changelevel_transition.changelevel_player_transfer_write_set.apply_target
            == expected_apply_target,
        std::string("expected changelevel_player_transfer_write_set applyTarget=")
            + (expected_apply_target.empty()
                ? std::string("<empty>")
                : std::string(expected_apply_target)));
    AddGuardFailure(
        failures,
        summary.changelevel_transition.changelevel_player_transfer_write_set.write_set_ready
            == expected_write_set_ready,
        std::string("expected changelevel_player_transfer_write_set writeSetReady=")
            + (expected_write_set_ready ? "yes" : "no"));
    AddGuardFailure(
        failures,
        summary.changelevel_transition.changelevel_player_transfer_write_set.action
            == expected_action,
        std::string("expected changelevel_player_transfer_write_set action=")
            + std::string(expected_action));
    AddGuardFailure(
        failures,
        summary.changelevel_transition.changelevel_player_transfer_write_set
            .short_circuit_reason
            == expected_short_circuit_reason,
        std::string("expected changelevel_player_transfer_write_set shortCircuitReason=")
            + (expected_short_circuit_reason.empty()
                ? std::string("<empty>")
                : std::string(expected_short_circuit_reason)));

    if (!expected_write_set_ready)
    {
        return;
    }

    AddGuardFailure(
        failures,
        summary.changelevel_transition.changelevel_player_transfer_write_set.current_map
            == "c0a0",
        "expected changelevel_player_transfer_write_set currentMap=c0a0");
    AddGuardFailure(
        failures,
        summary.changelevel_transition.changelevel_player_transfer_write_set.requested_map
            == "c0a0a",
        "expected changelevel_player_transfer_write_set requestedMap=c0a0a");
    AddGuardFailure(
        failures,
        summary.changelevel_transition.changelevel_player_transfer_write_set.landmark
            == "c0a0toa",
        "expected changelevel_player_transfer_write_set landmark=c0a0toa");
    AddGuardFailure(
        failures,
        ContainsText(
            summary.changelevel_transition.changelevel_player_transfer_write_set.target_bsp_path,
            "c0a0a.bsp"),
        "expected changelevel_player_transfer_write_set targetBspPath to reference c0a0a.bsp");
    AddGuardFailure(
        failures,
        summary.changelevel_transition.changelevel_player_transfer_write_set
            .target_player_origin
            == "-2687 -1342.35 65",
        "expected changelevel_player_transfer_write_set targetPlayerOrigin=-2687 -1342.35 65");
    AddGuardFailure(
        failures,
        summary.changelevel_transition.changelevel_player_transfer_write_set.target_player_yaw
            == "321.622406",
        "expected changelevel_player_transfer_write_set targetPlayerYaw=321.622406");
    AddGuardFailure(
        failures,
        summary.changelevel_transition.changelevel_player_transfer_write_set.write_origin,
        "expected changelevel_player_transfer_write_set writeOrigin=yes");
    AddGuardFailure(
        failures,
        summary.changelevel_transition.changelevel_player_transfer_write_set.write_yaw,
        "expected changelevel_player_transfer_write_set writeYaw=yes");
    AddGuardFailure(
        failures,
        !summary.changelevel_transition.changelevel_player_transfer_write_set.write_inventory,
        "expected changelevel_player_transfer_write_set writeInventory=no");
    AddGuardFailure(
        failures,
        !summary.changelevel_transition.changelevel_player_transfer_write_set.write_velocity,
        "expected changelevel_player_transfer_write_set writeVelocity=no");
    AddGuardFailure(
        failures,
        summary.changelevel_transition.changelevel_player_transfer_write_set.write_count == 2,
        "expected changelevel_player_transfer_write_set writeCount=2");
    AddGuardFailure(
        failures,
        summary.changelevel_transition.changelevel_player_transfer_write_set
            .runtime_write_suppressed,
        "expected changelevel_player_transfer_write_set runtimeWriteSuppressed=yes");
    AddGuardFailure(
        failures,
        summary.changelevel_transition.changelevel_player_transfer_write_set
            .target_worldspawn_present,
        "expected changelevel_player_transfer_write_set targetWorldspawnPresent=yes");
    AddGuardFailure(
        failures,
        summary.changelevel_transition.changelevel_player_transfer_write_set
            .target_entity_parse_ok,
        "expected changelevel_player_transfer_write_set targetEntityParse=ok");
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

    case hl::app::RegressionGuardProfile::kChangelevelLatchOnlyContinuation:
        ValidateTrainstop26TerminalState(summary, failures);
        ValidateChangelevelTargetValidation(summary, failures);
        ValidateChangelevelLifecycleGate(summary, failures);
        ValidateChangelevelLifecycleEntry(
            summary,
            true,
            false,
            "no-op entry armed",
            failures);
        ValidateChangelevelLifecycleDispatch(
            summary,
            true,
            false,
            "no-op dispatch armed",
            "",
            failures);
        ValidateChangelevelLifecycleExecution(
            summary,
            true,
            false,
            "no-op execution armed",
            "",
            failures);
        ValidateChangelevelBootstrapPlan(
            summary,
            true,
            false,
            "prepared-no-load",
            "",
            failures);
        ValidateChangelevelLandmarkTransform(
            summary,
            true,
            false,
            "no-op transform prepared",
            "",
            failures);
        ValidateChangelevelProjectedCarriedOrigin(
            summary,
            true,
            false,
            "no-op carried-origin projection",
            "",
            failures);
        ValidateChangelevelProjectedCarriedOrientation(
            summary,
            true,
            false,
            "no-op carried-orientation projection",
            "",
            failures);
        ValidateChangelevelProjectedTransferSnapshot(
            summary,
            true,
            false,
            true,
            "no-op transfer snapshot prepared",
            "",
            failures);
        ValidateChangelevelPlayerTransferApplyPlan(
            summary,
            true,
            false,
            true,
            "no-op player apply plan prepared",
            "",
            failures);
        ValidateChangelevelPlayerTransferWriteSet(
            summary,
            true,
            false,
            true,
            "no-op player transfer write set prepared",
            "",
            failures);
        AddGuardFailure(
            failures,
            !summary.server_frame_loop.any_seh,
            "expected latch-only continuation any_seh=no");
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
            summary.changelevel_transition.pre_changelevel_handoff.handoff_latched,
            "expected changelevel_transition_intent handoffLatched=yes");
        AddGuardFailure(
            failures,
            !summary.changelevel_transition.pre_changelevel_handoff.world_frozen,
            "expected changelevel_transition_intent worldFrozen=no");
        AddGuardFailure(
            failures,
            !summary.changelevel_transition.pre_changelevel_handoff.stop_requested,
            "expected changelevel_transition_intent stopRequested=no");
        AddGuardFailure(
            failures,
            summary.changelevel_transition.transition_intent_action == "no-op handoff boundary",
            "expected changelevel action=no-op handoff boundary");
        AddGuardFailure(
            failures,
            summary.changelevel_transition.pre_changelevel_handoff.active,
            "expected pre_changelevel_handoff active=yes");
        AddGuardFailure(
            failures,
            summary.changelevel_transition.pre_changelevel_handoff.handoff_latched,
            "expected pre_changelevel_handoff handoffLatched=yes");
        AddGuardFailure(
            failures,
            !summary.changelevel_transition.pre_changelevel_handoff.world_frozen,
            "expected pre_changelevel_handoff worldFrozen=no");
        AddGuardFailure(
            failures,
            !summary.changelevel_transition.pre_changelevel_handoff.stop_requested,
            "expected pre_changelevel_handoff stopRequested=no");
        AddGuardFailure(
            failures,
            !summary.changelevel_transition.pre_changelevel_handoff.map_load_performed,
            "expected pre_changelevel_handoff mapLoad=no");
        AddGuardFailure(
            failures,
            summary.changelevel_transition.post_handoff_activity.measured,
            "expected post_handoff_activity measured=yes");
        AddGuardFailure(
            failures,
            summary.changelevel_transition.post_handoff_activity.scheduled_executed == 0,
            "expected postHandoffScheduledExecuted=0");
        AddGuardFailure(
            failures,
            summary.changelevel_transition.post_handoff_activity.dispatch_attempts == 0,
            "expected postHandoffDispatchAttempts=0");
        AddGuardFailure(
            failures,
            summary.changelevel_transition.post_handoff_activity.dispatch_successes == 0,
            "expected postHandoffDispatchSuccesses=0");
        AddGuardFailure(
            failures,
            summary.changelevel_transition.post_handoff_activity.messages == 0,
            "expected postHandoffMessages=0");
        break;

    case hl::app::RegressionGuardProfile::kChangelevelRequestConsumed:
        ValidateChangelevelTargetValidation(summary, failures);
        ValidateChangelevelLifecycleGate(summary, failures);
        ValidateChangelevelLifecycleEntry(
            summary,
            false,
            true,
            "no-op entry skipped by stop mode",
            failures);
        ValidateChangelevelLifecycleDispatch(
            summary,
            false,
            true,
            "no-op dispatch skipped",
            "stop-on-changelevel-request",
            failures);
        ValidateChangelevelLifecycleExecution(
            summary,
            false,
            true,
            "no-op execution skipped",
            "stop-on-changelevel-request",
            failures);
        ValidateChangelevelBootstrapPlan(
            summary,
            false,
            true,
            "plan skipped",
            "stop-on-changelevel-request",
            failures);
        ValidateChangelevelLandmarkTransform(
            summary,
            false,
            true,
            "transform skipped",
            "stop-on-changelevel-request",
            failures);
        ValidateChangelevelProjectedCarriedOrigin(
            summary,
            false,
            true,
            "projection skipped",
            "stop-on-changelevel-request",
            failures);
        ValidateChangelevelProjectedCarriedOrientation(
            summary,
            false,
            true,
            "orientation projection skipped",
            "stop-on-changelevel-request",
            failures);
        ValidateChangelevelProjectedTransferSnapshot(
            summary,
            false,
            true,
            false,
            "transfer snapshot skipped",
            "stop-on-changelevel-request",
            failures);
        ValidateChangelevelPlayerTransferApplyPlan(
            summary,
            false,
            true,
            false,
            "player apply plan skipped",
            "stop-on-changelevel-request",
            failures);
        ValidateChangelevelPlayerTransferWriteSet(
            summary,
            false,
            true,
            false,
            "player transfer write set skipped",
            "stop-on-changelevel-request",
            failures);
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
            summary.changelevel_transition.pre_changelevel_handoff.handoff_latched,
            "expected pre_changelevel_handoff handoffLatched=yes");
        AddGuardFailure(
            failures,
            summary.changelevel_transition.pre_changelevel_handoff.world_frozen,
            "expected pre_changelevel_handoff worldFrozen=yes");
        AddGuardFailure(
            failures,
            summary.changelevel_transition.pre_changelevel_handoff.stop_requested,
            "expected pre_changelevel_handoff stopRequested=yes");
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
            summary.changelevel_transition.pre_changelevel_handoff.action
                == "no-op transition stop",
            "expected pre_changelevel_handoff action=no-op transition stop");
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
