#include "path_node_event_semantics_controller.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <sstream>
#include <utility>

namespace
{
constexpr std::array<std::string_view, 4> kDefaultMessageCanaries = {
    "trainstop8",
    "trainstop8a",
    "trainstop9",
    "trainstop11",
};

std::string FormatNodeName(std::string_view value)
{
    return value.empty() ? std::string("<empty>") : std::string(value);
}

bool IsClassification(std::string_view value, std::string_view expected)
{
    if (value.size() != expected.size())
    {
        return false;
    }

    return std::equal(
        value.begin(),
        value.end(),
        expected.begin(),
        [](char left, char right)
        {
            return std::tolower(static_cast<unsigned char>(left))
                == std::tolower(static_cast<unsigned char>(right));
        });
}
} // namespace

namespace hl::game_api::detail
{
void PathNodeEventSemanticsController::Configure(const PathNodeEventSemanticsConfig& config)
{
    config_ = config;
    if (config_.history_limit == 0)
    {
        config_.history_limit = 64;
    }

    EnsureCanaryDefaults();
}

PathNodeEventSemanticsResult PathNodeEventSemanticsController::HandleArrival(
    PathMoverMutableState& mover,
    const TrackPathNodeView& node,
    const ScriptedMovementFrameContext& frame,
    const PathNodeEventSemanticsHooks& hooks)
{
    PathNodeEventSemanticsResult result;
    result.node_reached = true;

    summary_.deepest_node_reached = FormatNodeName(node.targetname);

    const float speed_before_arrival = mover.summary.last_speed_before_arrival;
    const float speed_after_arrival = mover.summary.last_speed_after_arrival > 0.0f
        ? mover.summary.last_speed_after_arrival
        : mover.summary.effective_speed;
    const bool speed_changed = mover.summary.last_speed_changed_on_arrival;

    auto build_trace_summary =
        [&](std::string_view classification, std::string_view dispatch_result)
        {
            return "frame=" + std::to_string(frame.frame_number)
                + " time=" + std::to_string(frame.time)
                + " mover="
                + (mover.summary.targetname.empty()
                    ? std::string("<empty>")
                    : mover.summary.targetname)
                + " stage=" + (mover.summary.stage.empty()
                    ? std::string("<unset>")
                    : mover.summary.stage)
                + " previous=" + (mover.summary.previous_node.empty()
                    ? std::string("<none>")
                    : mover.summary.previous_node)
                + " current=" + (mover.summary.current_node.empty()
                    ? std::string("<none>")
                    : mover.summary.current_node)
                + " next=" + (mover.summary.next_node.empty()
                    ? std::string("<none>")
                    : mover.summary.next_node)
                + " arrivals=" + std::to_string(mover.summary.node_arrivals)
                + " advances=" + std::to_string(mover.summary.node_advances)
                + " message=" + (node.message_target.empty()
                    ? std::string("<none>")
                    : node.message_target)
                + " classification=" + std::string(classification)
                + " nodeSpeed=" + std::to_string(node.speed)
                + " speedBefore=" + std::to_string(speed_before_arrival)
                + " speedAfter=" + std::to_string(speed_after_arrival)
                + " speedChanged=" + (speed_changed ? "yes" : "no")
                + " speedDecision=" + (mover.summary.last_speed_decision.empty()
                    ? std::string("<unset>")
                    : mover.summary.last_speed_decision)
                + " currentSpeed=" + std::to_string(mover.summary.effective_speed)
                + " dispatchAttempted="
                + (mover.summary.staged_message_dispatch_attempted ? "yes" : "no")
                + " dispatchResult=" + std::string(dispatch_result)
                + " blocked=" + (mover.summary.blocked_reason.empty()
                    ? std::string("<none>")
                    : mover.summary.blocked_reason)
                + " stopped=" + (mover.summary.stopped_reason.empty()
                    ? std::string("<none>")
                    : mover.summary.stopped_reason);
        };

    if (node.message_target.empty())
    {
        mover.summary.last_message.clear();
        mover.summary.last_message_node.clear();
        mover.summary.last_message_dispatch_result = "no-message";
        mover.summary.staged_message_dispatch_attempted = false;
        mover.summary.staged_message_dispatch_succeeded = false;
        mover.summary.staged_message_dispatch_detail = "arrival-node has no message";

        PathNodeEventDispatchFeedback no_message_feedback;
        NoteCanaryReach(
            node,
            frame,
            mover,
            no_message_feedback,
            "no-message");

        if (config_.trace_movement)
        {
            result.trace_summary = build_trace_summary("no-message", "no-message");
            AppendLimited(summary_.rolling_trace, result.trace_summary, config_.history_limit);
            if (hooks.log_info)
            {
                hooks.log_info("PathNodeEventSemanticsController: " + result.trace_summary);
            }
        }
        return result;
    }

    result.encountered = true;
    ++summary_.encountered;
    if (!summary_.first_message_bearing_node_reached)
    {
        summary_.first_message_bearing_node_reached = true;
        summary_.first_message_bearing_node = node.targetname;
        summary_.first_message_bearing_frame = frame.frame_number;
        summary_.first_message_bearing_time = frame.time;
    }
    AppendUnique(summary_.reached_message_nodes, node.targetname);

    mover.summary.last_message = node.message_target;
    mover.summary.last_message_node = node.targetname;

    result.encountered_summary =
        "frame=" + std::to_string(frame.frame_number)
        + " time=" + std::to_string(frame.time)
        + " mover="
        + (mover.summary.targetname.empty() ? std::string("<empty>") : mover.summary.targetname)
        + " node=" + FormatNodeName(node.targetname)
        + " message=" + node.message_target
        + " nodeSpeed=" + std::to_string(node.speed)
        + " speedBefore=" + std::to_string(speed_before_arrival)
        + " speedAfter=" + std::to_string(speed_after_arrival)
        + " speedChanged=" + (speed_changed ? std::string("yes") : "no")
        + " currentSpeed=" + std::to_string(mover.summary.effective_speed)
        + " speedDecision=" + (mover.summary.last_speed_decision.empty()
            ? std::string("<unset>")
            : mover.summary.last_speed_decision);
    AppendLimited(summary_.encountered_history, result.encountered_summary, config_.history_limit);

    PathNodeEventDispatchFeedback feedback;
    if (!LooksLikeTargetName(node.message_target))
    {
        feedback.outcome = PathNodeEventDispatchOutcome::kDeferred;
        feedback.classification_hint = "encountered-unsupported";
        feedback.detail = "message contains unsupported whitespace-delimited token payload";
    }
    else if (hooks.dispatch_target_event == nullptr)
    {
        feedback.outcome = PathNodeEventDispatchOutcome::kDeferred;
        feedback.classification_hint = "encountered-deferred";
        feedback.detail = "dispatch hook unavailable; staged bootstrap surfaced only";
    }
    else
    {
        PathNodeEventDispatchRequest request;
        request.mover_edict_index = mover.summary.edict_index;
        request.mover_classname = mover.summary.classname;
        request.mover_targetname = mover.summary.targetname;
        request.node_name = node.targetname;
        request.message = node.message_target;
        request.node_speed = node.speed;
        request.current_speed = mover.summary.effective_speed;
        request.speed_before_arrival = speed_before_arrival;
        request.speed_after_arrival = speed_after_arrival;
        request.speed_changed_on_arrival = speed_changed;
        request.speed_decision = mover.summary.last_speed_decision;
        request.speed_decision_detail = mover.summary.last_speed_decision_detail;
        request.frame_number = frame.frame_number;
        request.time = frame.time;
        feedback = hooks.dispatch_target_event(request);
    }

    result.staged_dispatch_attempted = feedback.attempted;
    result.dispatch_outcome = feedback.outcome;
    result.classification = ResolveClassification(node.message_target, feedback);

    mover.summary.staged_message_dispatch_attempted = feedback.attempted;
    mover.summary.staged_message_dispatch_succeeded =
        feedback.outcome == PathNodeEventDispatchOutcome::kSucceeded;
    mover.summary.last_message_dispatch_result = DispatchOutcomeLabel(feedback.outcome);
    mover.summary.staged_message_dispatch_detail =
        feedback.detail.empty()
        ? std::string("staged path-node event semantics")
        : feedback.detail;

    if (feedback.attempted)
    {
        ++summary_.staged_dispatch_attempts;
    }

    switch (feedback.outcome)
    {
    case PathNodeEventDispatchOutcome::kSucceeded:
        ++summary_.staged_dispatch_successes;
        break;
    case PathNodeEventDispatchOutcome::kDeferred:
        ++summary_.staged_dispatch_deferred;
        break;
    case PathNodeEventDispatchOutcome::kFailed:
        ++summary_.staged_dispatch_failures;
        break;
    case PathNodeEventDispatchOutcome::kUnresolvedTarget:
        ++summary_.unresolved_message_targets;
        break;
    case PathNodeEventDispatchOutcome::kNotAttempted:
    default:
        break;
    }

    NoteClassificationCount(result.classification);

    result.dispatch_attempt_summary =
        "frame=" + std::to_string(frame.frame_number)
        + " mover="
        + (mover.summary.targetname.empty() ? std::string("<empty>") : mover.summary.targetname)
        + " node=" + FormatNodeName(node.targetname)
        + " message=" + node.message_target
        + " stagedDispatchAttempted=" + (feedback.attempted ? std::string("yes") : "no")
        + " runtimeTargets=" + std::to_string(feedback.runtime_target_candidates)
        + " parsedTargets=" + std::to_string(feedback.parsed_target_candidates)
        + " resolvedTargets=" + std::to_string(feedback.resolved_targets)
        + " targetClassnames=" + JoinValues(feedback.target_classnames)
        + " resolvedTargetDetails=" + JoinValues(feedback.resolved_target_details)
        + " dispatchMode="
        + (feedback.dispatch_mode.empty() ? std::string("<none>") : feedback.dispatch_mode)
        + " fadeChannelAvailable="
        + (feedback.fade_channel_available ? std::string("yes") : "no")
        + " fadeChannelUsed=" + (feedback.fade_channel_used ? std::string("yes") : "no")
        + " messageChannelAvailable="
        + (feedback.message_channel_available ? std::string("yes") : "no")
        + " messageChannelUsed="
        + (feedback.message_channel_used ? std::string("yes") : "no")
        + " envMessageLinkageFound="
        + (feedback.env_message_linkage_found ? std::string("yes") : "no")
        + " envMessageLinkageUsed="
        + (feedback.env_message_linkage_used ? std::string("yes") : "no")
        + " summaryFallbackUsed="
        + (feedback.summary_only_fallback_used ? std::string("yes") : "no")
        + " presentationLinkage="
        + (feedback.presentation_linkage_detail.empty()
            ? std::string("<none>")
            : feedback.presentation_linkage_detail)
        + " pfnUseAttempted=" + (feedback.pfn_use_attempted ? std::string("yes") : "no")
        + " brushDoorAttempted="
        + (feedback.brush_door_handling_attempted ? std::string("yes") : "no")
        + " brushDoorPath="
        + (feedback.brush_door_dispatch_path.empty()
            ? std::string("<none>")
            : feedback.brush_door_dispatch_path)
        + " brushDoorState="
        + (feedback.brush_door_state.empty() ? std::string("<none>") : feedback.brush_door_state)
        + " requiredSubsystem="
        + (feedback.required_subsystem.empty()
            ? std::string("<none>")
            : feedback.required_subsystem)
        + (feedback.detail.empty() ? std::string() : " detail=" + feedback.detail);
    AppendLimited(
        summary_.dispatch_attempt_history,
        result.dispatch_attempt_summary,
        config_.history_limit);

    result.dispatch_summary =
        "frame=" + std::to_string(frame.frame_number)
        + " mover="
        + (mover.summary.targetname.empty() ? std::string("<empty>") : mover.summary.targetname)
        + " node=" + FormatNodeName(node.targetname)
        + " message=" + node.message_target
        + " classification=" + result.classification
        + " dispatchResult=" + DispatchOutcomeLabel(feedback.outcome)
        + " resolvedTargets=" + std::to_string(feedback.resolved_targets)
        + " success=" + std::to_string(feedback.successful_targets)
        + " deferred=" + std::to_string(feedback.deferred_targets)
        + " failed=" + std::to_string(feedback.failed_targets)
        + " targetClassnames=" + JoinValues(feedback.target_classnames)
        + " resolvedTargetDetails=" + JoinValues(feedback.resolved_target_details)
        + " dispatchMode="
        + (feedback.dispatch_mode.empty() ? std::string("<none>") : feedback.dispatch_mode)
        + " fadeChannelAvailable="
        + (feedback.fade_channel_available ? std::string("yes") : "no")
        + " fadeChannelUsed=" + (feedback.fade_channel_used ? std::string("yes") : "no")
        + " messageChannelAvailable="
        + (feedback.message_channel_available ? std::string("yes") : "no")
        + " messageChannelUsed="
        + (feedback.message_channel_used ? std::string("yes") : "no")
        + " envMessageLinkageFound="
        + (feedback.env_message_linkage_found ? std::string("yes") : "no")
        + " envMessageLinkageUsed="
        + (feedback.env_message_linkage_used ? std::string("yes") : "no")
        + " summaryFallbackUsed="
        + (feedback.summary_only_fallback_used ? std::string("yes") : "no")
        + " presentationLinkage="
        + (feedback.presentation_linkage_detail.empty()
            ? std::string("<none>")
            : feedback.presentation_linkage_detail)
        + " pfnUseAttempted=" + (feedback.pfn_use_attempted ? std::string("yes") : "no")
        + " brushDoorAttempted="
        + (feedback.brush_door_handling_attempted ? std::string("yes") : "no")
        + " brushDoorPath="
        + (feedback.brush_door_dispatch_path.empty()
            ? std::string("<none>")
            : feedback.brush_door_dispatch_path)
        + " brushDoorSupport="
        + (feedback.brush_door_support_state.empty()
            ? std::string("<none>")
            : feedback.brush_door_support_state)
        + " brushDoorState="
        + (feedback.brush_door_state.empty() ? std::string("<none>") : feedback.brush_door_state)
        + " brushDoorStarted="
        + (feedback.brush_door_movement_started ? std::string("yes") : "no")
        + " brushDoorCompleted="
        + (feedback.brush_door_movement_completed ? std::string("yes") : "no")
        + " visibleDownstreamProgression="
        + (feedback.visible_downstream_progression ? std::string("yes") : "no")
        + " downstream="
        + (feedback.downstream_summary.empty() ? std::string("<none>") : feedback.downstream_summary)
        + " requiredSubsystem="
        + (feedback.required_subsystem.empty()
            ? std::string("<none>")
            : feedback.required_subsystem)
        + " brushDoorAudit="
        + (feedback.brush_door_runtime_audit.empty()
            ? std::string("<none>")
            : feedback.brush_door_runtime_audit)
        + (feedback.detail.empty() ? std::string() : " detail=" + feedback.detail);
    AppendLimited(summary_.dispatch_history, result.dispatch_summary, config_.history_limit);

    NoteMessageRecord(
        node,
        frame,
        mover,
        feedback,
        result.classification);
    NotePresentationEvent(
        node,
        frame,
        feedback,
        result.classification);
    NoteCanaryReach(
        node,
        frame,
        mover,
        feedback,
        result.classification);

    result.trace_summary =
        build_trace_summary(result.classification, DispatchOutcomeLabel(feedback.outcome));
    AppendLimited(summary_.rolling_trace, result.trace_summary, config_.history_limit);

    if (hooks.log_info)
    {
        hooks.log_info("PathNodeEventSemanticsController: " + result.encountered_summary);
        hooks.log_info("PathNodeEventSemanticsController: " + result.dispatch_attempt_summary);
    }

    if (feedback.outcome == PathNodeEventDispatchOutcome::kFailed
        || feedback.outcome == PathNodeEventDispatchOutcome::kUnresolvedTarget
        || IsClassification(result.classification, "encountered-unsupported"))
    {
        if (hooks.log_warn)
        {
            hooks.log_warn("PathNodeEventSemanticsController: " + result.dispatch_summary);
        }
    }
    else if (hooks.log_info)
    {
        hooks.log_info("PathNodeEventSemanticsController: " + result.dispatch_summary);
    }

    if (hooks.log_info)
    {
        hooks.log_info("PathNodeEventSemanticsController: " + result.trace_summary);
    }

    return result;
}

const std::vector<std::string>& PathNodeEventSemanticsController::EncounteredHistory() const noexcept
{
    return summary_.encountered_history;
}

const std::vector<std::string>& PathNodeEventSemanticsController::DispatchAttemptHistory() const noexcept
{
    return summary_.dispatch_attempt_history;
}

const std::vector<std::string>& PathNodeEventSemanticsController::DispatchHistory() const noexcept
{
    return summary_.dispatch_history;
}

const std::vector<std::string>& PathNodeEventSemanticsController::RollingTrace() const noexcept
{
    return summary_.rolling_trace;
}

const hl::game_api::PathNodeMessageStateSummary& PathNodeEventSemanticsController::Summary() const noexcept
{
    return summary_;
}

std::size_t PathNodeEventSemanticsController::DispatchCount() const noexcept
{
    return summary_.staged_dispatch_attempts;
}

bool PathNodeEventSemanticsController::AnyDispatchSucceeded() const noexcept
{
    return summary_.staged_dispatch_successes > 0;
}

const char* PathNodeEventSemanticsController::DispatchOutcomeLabel(
    PathNodeEventDispatchOutcome outcome) noexcept
{
    switch (outcome)
    {
    case PathNodeEventDispatchOutcome::kSucceeded:
        return "succeeded";
    case PathNodeEventDispatchOutcome::kDeferred:
        return "deferred";
    case PathNodeEventDispatchOutcome::kFailed:
        return "failed";
    case PathNodeEventDispatchOutcome::kUnresolvedTarget:
        return "unresolved-target";
    case PathNodeEventDispatchOutcome::kNotAttempted:
    default:
        return "not-attempted";
    }
}

bool PathNodeEventSemanticsController::LooksLikeTargetName(std::string_view message) noexcept
{
    if (message.empty())
    {
        return false;
    }

    return std::find_if(
               message.begin(),
               message.end(),
               [](char character)
               {
                   return std::isspace(static_cast<unsigned char>(character)) != 0;
               })
        == message.end();
}

void PathNodeEventSemanticsController::AppendLimited(
    std::vector<std::string>& lines,
    std::string line,
    std::size_t limit)
{
    lines.push_back(std::move(line));
    while (lines.size() > limit)
    {
        lines.erase(lines.begin());
    }
}

bool PathNodeEventSemanticsController::EqualsIgnoreCase(
    std::string_view left,
    std::string_view right) noexcept
{
    if (left.size() != right.size())
    {
        return false;
    }

    return std::equal(
        left.begin(),
        left.end(),
        right.begin(),
        [](char lhs, char rhs)
        {
            return std::tolower(static_cast<unsigned char>(lhs))
                == std::tolower(static_cast<unsigned char>(rhs));
        });
}

bool PathNodeEventSemanticsController::StartsWithIgnoreCase(
    std::string_view value,
    std::string_view prefix) noexcept
{
    if (value.size() < prefix.size())
    {
        return false;
    }

    return std::equal(
        prefix.begin(),
        prefix.end(),
        value.begin(),
        [](char lhs, char rhs)
        {
            return std::tolower(static_cast<unsigned char>(lhs))
                == std::tolower(static_cast<unsigned char>(rhs));
        });
}

void PathNodeEventSemanticsController::AppendUnique(
    std::vector<std::string>& lines,
    std::string value)
{
    if (value.empty())
    {
        return;
    }

    if (std::find_if(
            lines.begin(),
            lines.end(),
            [&](std::string_view existing)
            {
                return EqualsIgnoreCase(existing, value);
            })
        != lines.end())
    {
        return;
    }

    lines.push_back(std::move(value));
}

std::string PathNodeEventSemanticsController::JoinValues(const std::vector<std::string>& values)
{
    if (values.empty())
    {
        return "<none>";
    }

    std::ostringstream stream;
    for (std::size_t index = 0; index < values.size(); ++index)
    {
        if (index != 0)
        {
            stream << ';';
        }
        stream << values[index];
    }
    return stream.str();
}

std::string PathNodeEventSemanticsController::ResolveClassification(
    std::string_view message,
    const PathNodeEventDispatchFeedback& feedback)
{
    if (!feedback.classification_hint.empty())
    {
        return feedback.classification_hint;
    }

    if (feedback.outcome == PathNodeEventDispatchOutcome::kSucceeded)
    {
        return "resolved-and-dispatched";
    }

    if (feedback.outcome == PathNodeEventDispatchOutcome::kUnresolvedTarget)
    {
        return feedback.parsed_target_candidates > 0
            ? "encountered-unresolved-target"
            : "encountered-no-target";
    }

    if (feedback.outcome == PathNodeEventDispatchOutcome::kDeferred)
    {
        return "encountered-deferred";
    }

    if (feedback.outcome == PathNodeEventDispatchOutcome::kFailed)
    {
        return "encountered-deferred";
    }

    return message.empty() ? "no-message" : "encountered-deferred";
}

hl::game_api::PathNodeMessageEncounterSummary*
PathNodeEventSemanticsController::FindMessageRecord(
    hl::game_api::PathNodeMessageStateSummary& summary,
    std::string_view node_name)
{
    auto it = std::find_if(
        summary.message_records.begin(),
        summary.message_records.end(),
        [&](const hl::game_api::PathNodeMessageEncounterSummary& record)
        {
            return EqualsIgnoreCase(record.node_name, node_name);
        });
    return it != summary.message_records.end() ? &(*it) : nullptr;
}

hl::game_api::PathNodePresentationEventSummary*
PathNodeEventSemanticsController::FindPresentationEvent(
    hl::game_api::PathNodeMessageStateSummary& summary,
    std::string_view node_name,
    std::string_view event_name)
{
    const auto it = std::find_if(
        summary.presentation_events.begin(),
        summary.presentation_events.end(),
        [&](const hl::game_api::PathNodePresentationEventSummary& record)
        {
            return EqualsIgnoreCase(record.source_node, node_name)
                && EqualsIgnoreCase(record.event_name, event_name);
        });
    return it != summary.presentation_events.end() ? &(*it) : nullptr;
}

void PathNodeEventSemanticsController::EnsureCanaryDefaults()
{
    if (config_.canary_nodes.empty())
    {
        config_.canary_nodes.assign(kDefaultMessageCanaries.begin(), kDefaultMessageCanaries.end());
    }

    summary_ = {};
    for (const std::string& canary_name : config_.canary_nodes)
    {
        hl::game_api::PathNodeMessageCanarySummary canary;
        canary.node_name = canary_name;
        canary.dispatch_result = "not-reached";
        summary_.canaries.push_back(std::move(canary));
    }
}

void PathNodeEventSemanticsController::NoteClassificationCount(std::string_view classification)
{
    if (StartsWithIgnoreCase(classification, "resolved-and-dispatched"))
    {
        ++summary_.resolved_and_dispatched;
    }
    else if (IsClassification(classification, "encountered-no-target"))
    {
        ++summary_.encountered_no_target;
    }
    else if (IsClassification(classification, "encountered-unresolved-target"))
    {
        ++summary_.encountered_unresolved_targets;
    }
    else if (IsClassification(classification, "encountered-unsupported"))
    {
        ++summary_.encountered_unsupported;
    }
}

void PathNodeEventSemanticsController::NoteMessageRecord(
    const TrackPathNodeView& node,
    const ScriptedMovementFrameContext& frame,
    const PathMoverMutableState& mover,
    const PathNodeEventDispatchFeedback& feedback,
    std::string_view classification)
{
    hl::game_api::PathNodeMessageEncounterSummary* record =
        FindMessageRecord(summary_, node.targetname);
    if (record == nullptr)
    {
        summary_.message_records.push_back({});
        record = &summary_.message_records.back();
        record->node_name = node.targetname;
        record->first_reached_frame = frame.frame_number;
        record->first_reached_time = frame.time;
    }

    record->message = node.message_target;
    record->node_speed_metadata = node.speed;
    record->mover_speed_at_encounter = mover.summary.effective_speed;
    record->mover_speed_before_encounter = mover.summary.last_speed_before_arrival;
    record->mover_speed_after_encounter = mover.summary.last_speed_after_arrival > 0.0f
        ? mover.summary.last_speed_after_arrival
        : mover.summary.effective_speed;
    record->mover_speed_changed = mover.summary.last_speed_changed_on_arrival;
    record->dispatch_attempted = feedback.attempted;
    record->dispatch_result = DispatchOutcomeLabel(feedback.outcome);
    record->classification = std::string(classification);
    record->dispatch_mode = feedback.dispatch_mode;
    record->fade_channel_available = feedback.fade_channel_available;
    record->fade_channel_used = feedback.fade_channel_used;
    record->message_channel_available = feedback.message_channel_available;
    record->message_channel_used = feedback.message_channel_used;
    record->env_message_linkage_found = feedback.env_message_linkage_found;
    record->env_message_linkage_used = feedback.env_message_linkage_used;
    record->summary_only_fallback_used = feedback.summary_only_fallback_used;
    record->resolved_targets = feedback.resolved_targets;
    record->runtime_target_candidates = feedback.runtime_target_candidates;
    record->parsed_target_candidates = feedback.parsed_target_candidates;
    record->pfn_use_attempted = feedback.pfn_use_attempted;
    record->visible_downstream_progression =
        record->visible_downstream_progression || feedback.visible_downstream_progression;
    record->target_classnames = feedback.target_classnames;
    record->resolved_target_details = feedback.resolved_target_details;
    record->downstream_summary = feedback.downstream_summary;
    record->presentation_linkage_detail = feedback.presentation_linkage_detail;
    record->dispatch_detail = feedback.detail;
    record->required_subsystem = feedback.required_subsystem;
    record->brush_door_handling_attempted = feedback.brush_door_handling_attempted;
    record->brush_door_use_succeeded = feedback.brush_door_use_succeeded;
    record->brush_door_state_changed = feedback.brush_door_state_changed;
    record->brush_door_movement_started = feedback.brush_door_movement_started;
    record->brush_door_movement_completed = feedback.brush_door_movement_completed;
    record->brush_door_native_use_attempted = feedback.brush_door_native_use_attempted;
    record->brush_door_native_use_succeeded = feedback.brush_door_native_use_succeeded;
    record->brush_door_dispatch_path = feedback.brush_door_dispatch_path;
    record->brush_door_support_state = feedback.brush_door_support_state;
    record->brush_door_state = feedback.brush_door_state;
    record->brush_door_blocked_reason = feedback.brush_door_blocked_reason;
    record->brush_door_runtime_audit = feedback.brush_door_runtime_audit;
    record->downstream_target_chains = feedback.downstream_target_chains;
    record->downstream_scheduled_actions = feedback.downstream_scheduled_actions;
    record->downstream_alert_callbacks = feedback.alert_callbacks;
    record->downstream_message_callbacks = feedback.message_callbacks;
}

void PathNodeEventSemanticsController::NotePresentationEvent(
    const TrackPathNodeView& node,
    const ScriptedMovementFrameContext& frame,
    const PathNodeEventDispatchFeedback& feedback,
    std::string_view classification)
{
    const bool presentation_event =
        !feedback.dispatch_mode.empty()
        || feedback.fade_channel_available
        || feedback.fade_channel_used
        || feedback.message_channel_available
        || feedback.message_channel_used
        || feedback.env_message_linkage_found
        || feedback.env_message_linkage_used
        || feedback.summary_only_fallback_used;
    if (!presentation_event || node.message_target.empty())
    {
        return;
    }

    hl::game_api::PathNodePresentationEventSummary* record =
        FindPresentationEvent(summary_, node.targetname, node.message_target);
    if (record == nullptr)
    {
        summary_.presentation_events.push_back({});
        record = &summary_.presentation_events.back();
        record->event_name = node.message_target;
        record->source_node = node.targetname;
    }

    record->frame_number = frame.frame_number;
    record->time = frame.time;
    record->dispatch_attempted = feedback.attempted;
    record->succeeded = feedback.outcome == PathNodeEventDispatchOutcome::kSucceeded;
    record->deferred = feedback.outcome == PathNodeEventDispatchOutcome::kDeferred;
    record->dispatch_mode = feedback.dispatch_mode;
    record->classification = std::string(classification);
    record->fade_channel_available = feedback.fade_channel_available;
    record->fade_channel_used = feedback.fade_channel_used;
    record->message_channel_available = feedback.message_channel_available;
    record->message_channel_used = feedback.message_channel_used;
    record->env_message_linkage_found = feedback.env_message_linkage_found;
    record->env_message_linkage_used = feedback.env_message_linkage_used;
    record->summary_only_fallback_used = feedback.summary_only_fallback_used;
    record->required_subsystem = feedback.required_subsystem;
    record->presentation_linkage_detail = feedback.presentation_linkage_detail;
    record->dispatch_detail = feedback.detail;
}

void PathNodeEventSemanticsController::NoteCanaryReach(
    const TrackPathNodeView& node,
    const ScriptedMovementFrameContext& frame,
    const PathMoverMutableState& mover,
    const PathNodeEventDispatchFeedback& feedback,
    std::string_view classification)
{
    for (hl::game_api::PathNodeMessageCanarySummary& canary : summary_.canaries)
    {
        if (!EqualsIgnoreCase(canary.node_name, node.targetname))
        {
            continue;
        }

        if (!canary.reached)
        {
            canary.reached = true;
            canary.first_reached_frame = frame.frame_number;
            canary.first_reached_time = frame.time;
        }

        if (!node.message_target.empty())
        {
            canary.message_encountered = true;
            canary.staged_dispatch_attempted = feedback.attempted;
            canary.dispatch_result = DispatchOutcomeLabel(feedback.outcome);
            canary.classification = std::string(classification);
            canary.dispatch_mode = feedback.dispatch_mode;
            canary.fade_channel_available = feedback.fade_channel_available;
            canary.fade_channel_used = feedback.fade_channel_used;
            canary.message_channel_available = feedback.message_channel_available;
            canary.message_channel_used = feedback.message_channel_used;
            canary.env_message_linkage_found = feedback.env_message_linkage_found;
            canary.env_message_linkage_used = feedback.env_message_linkage_used;
            canary.summary_only_fallback_used = feedback.summary_only_fallback_used;
            canary.node_speed_metadata = node.speed;
            canary.mover_speed_at_encounter = mover.summary.effective_speed;
            canary.mover_speed_before_encounter = mover.summary.last_speed_before_arrival;
            canary.mover_speed_after_encounter = mover.summary.last_speed_after_arrival > 0.0f
                ? mover.summary.last_speed_after_arrival
                : mover.summary.effective_speed;
            canary.mover_speed_changed = mover.summary.last_speed_changed_on_arrival;
            canary.message = mover.summary.last_message;
            canary.resolved_targets = feedback.resolved_targets;
            canary.runtime_target_candidates = feedback.runtime_target_candidates;
            canary.parsed_target_candidates = feedback.parsed_target_candidates;
            canary.pfn_use_attempted = feedback.pfn_use_attempted;
            canary.visible_downstream_progression = feedback.visible_downstream_progression;
            canary.target_classnames = feedback.target_classnames;
            canary.resolved_target_details = feedback.resolved_target_details;
            canary.downstream_summary = feedback.downstream_summary;
            canary.presentation_linkage_detail = feedback.presentation_linkage_detail;
            canary.presentation_semantics_summary = feedback.presentation_semantics_summary;
            canary.dispatch_detail = feedback.detail;
            canary.required_subsystem = feedback.required_subsystem;
            canary.brush_door_handling_attempted = feedback.brush_door_handling_attempted;
            canary.brush_door_use_succeeded = feedback.brush_door_use_succeeded;
            canary.brush_door_state_changed = feedback.brush_door_state_changed;
            canary.brush_door_movement_started = feedback.brush_door_movement_started;
            canary.brush_door_movement_completed = feedback.brush_door_movement_completed;
            canary.brush_door_native_use_attempted = feedback.brush_door_native_use_attempted;
            canary.brush_door_native_use_succeeded = feedback.brush_door_native_use_succeeded;
            canary.brush_door_dispatch_path = feedback.brush_door_dispatch_path;
            canary.brush_door_support_state = feedback.brush_door_support_state;
            canary.brush_door_state = feedback.brush_door_state;
            canary.brush_door_blocked_reason = feedback.brush_door_blocked_reason;
            canary.brush_door_runtime_audit = feedback.brush_door_runtime_audit;
            canary.downstream_target_chains = feedback.downstream_target_chains;
            canary.downstream_scheduled_actions = feedback.downstream_scheduled_actions;
            canary.downstream_alert_callbacks = feedback.alert_callbacks;
            canary.downstream_message_callbacks = feedback.message_callbacks;
        }
        else if (canary.dispatch_result.empty())
        {
            canary.dispatch_result = "reached-no-message";
        }
    }
}
} // namespace hl::game_api::detail
