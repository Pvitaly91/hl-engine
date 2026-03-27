#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include "path_mover_runtime.h"
#include "scripted_movement_controller.h"
#include "track_path_resolver.h"

namespace hl::game_api::detail
{
struct PathNodeEventDispatchRequest
{
    int mover_edict_index = -1;
    std::string mover_classname;
    std::string mover_targetname;
    std::string node_name;
    std::string message;
    float node_speed = 0.0f;
    float current_speed = 0.0f;
    float speed_before_arrival = 0.0f;
    float speed_after_arrival = 0.0f;
    bool speed_changed_on_arrival = false;
    std::string speed_decision;
    std::string speed_decision_detail;
    int frame_number = 0;
    float time = 0.0f;
};

enum class PathNodeEventDispatchOutcome
{
    kNotAttempted,
    kSucceeded,
    kDeferred,
    kFailed,
    kUnresolvedTarget,
};

struct PathNodeEventDispatchFeedback
{
    bool attempted = false;
    PathNodeEventDispatchOutcome outcome = PathNodeEventDispatchOutcome::kNotAttempted;
    int resolved_targets = 0;
    int successful_targets = 0;
    int deferred_targets = 0;
    int failed_targets = 0;
    bool unresolved_target = false;
    int runtime_target_candidates = 0;
    int parsed_target_candidates = 0;
    bool pfn_use_attempted = false;
    bool visible_downstream_progression = false;
    int multi_manager_activity = 0;
    int scripted_sequence_activity = 0;
    int actor_state_changes = 0;
    int path_state_changes = 0;
    int alert_callbacks = 0;
    int message_callbacks = 0;
    std::vector<std::string> target_classnames;
    std::vector<std::string> resolved_target_details;
    std::string classification_hint;
    std::string downstream_summary;
    std::string detail;
    std::string required_subsystem;
};

struct PathNodeEventSemanticsConfig
{
    std::size_t history_limit = 64;
    bool trace_movement = false;
    std::vector<std::string> canary_nodes;
};

struct PathNodeEventSemanticsHooks
{
    std::function<PathNodeEventDispatchFeedback(const PathNodeEventDispatchRequest&)>
        dispatch_target_event;
    std::function<void(std::string_view)> log_info;
    std::function<void(std::string_view)> log_warn;
};

struct PathNodeEventSemanticsResult
{
    bool node_reached = false;
    bool encountered = false;
    bool staged_dispatch_attempted = false;
    PathNodeEventDispatchOutcome dispatch_outcome = PathNodeEventDispatchOutcome::kNotAttempted;
    std::string classification;
    std::string encountered_summary;
    std::string dispatch_attempt_summary;
    std::string dispatch_summary;
    std::string trace_summary;
};

class PathNodeEventSemanticsController
{
public:
    void Configure(const PathNodeEventSemanticsConfig& config);
    PathNodeEventSemanticsResult HandleArrival(
        PathMoverMutableState& mover,
        const TrackPathNodeView& node,
        const ScriptedMovementFrameContext& frame,
        const PathNodeEventSemanticsHooks& hooks);

    const std::vector<std::string>& EncounteredHistory() const noexcept;
    const std::vector<std::string>& DispatchAttemptHistory() const noexcept;
    const std::vector<std::string>& DispatchHistory() const noexcept;
    const std::vector<std::string>& RollingTrace() const noexcept;
    const hl::game_api::PathNodeMessageStateSummary& Summary() const noexcept;
    std::size_t DispatchCount() const noexcept;
    bool AnyDispatchSucceeded() const noexcept;

    static const char* DispatchOutcomeLabel(PathNodeEventDispatchOutcome outcome) noexcept;

private:
    static bool LooksLikeTargetName(std::string_view message) noexcept;
    static void AppendLimited(std::vector<std::string>& lines, std::string line, std::size_t limit);
    static bool EqualsIgnoreCase(std::string_view left, std::string_view right) noexcept;
    static void AppendUnique(std::vector<std::string>& lines, std::string value);
    static std::string JoinValues(const std::vector<std::string>& values);
    static std::string ResolveClassification(
        std::string_view message,
        const PathNodeEventDispatchFeedback& feedback);
    static hl::game_api::PathNodeMessageEncounterSummary* FindMessageRecord(
        hl::game_api::PathNodeMessageStateSummary& summary,
        std::string_view node_name);

    void EnsureCanaryDefaults();
    void NoteClassificationCount(std::string_view classification);
    void NoteMessageRecord(
        const TrackPathNodeView& node,
        const ScriptedMovementFrameContext& frame,
        const PathMoverMutableState& mover,
        const PathNodeEventDispatchFeedback& feedback,
        std::string_view classification);
    void NoteCanaryReach(
        const TrackPathNodeView& node,
        const ScriptedMovementFrameContext& frame,
        const PathMoverMutableState& mover,
        const PathNodeEventDispatchFeedback& feedback,
        std::string_view classification);

    PathNodeEventSemanticsConfig config_{};
    hl::game_api::PathNodeMessageStateSummary summary_{};
};

using PathNodeMessageRuntimeConfig = PathNodeEventSemanticsConfig;
using PathNodeMessageRuntimeHooks = PathNodeEventSemanticsHooks;
using PathNodeMessageRuntimeResult = PathNodeEventSemanticsResult;
using PathNodeMessageRuntime = PathNodeEventSemanticsController;
using PathNodeMessageController = PathNodeEventSemanticsController;
using PathNodeEventBootstrapConfig = PathNodeEventSemanticsConfig;
using PathNodeEventBootstrapHooks = PathNodeEventSemanticsHooks;
using PathNodeEventBootstrapResult = PathNodeEventSemanticsResult;
using PathNodeEventBootstrap = PathNodeEventSemanticsController;
using PathNodeEventDispatcherConfig = PathNodeEventSemanticsConfig;
using PathNodeEventDispatcherHooks = PathNodeEventSemanticsHooks;
using PathNodeEventDispatchResult = PathNodeEventSemanticsResult;
using PathNodeEventDispatcher = PathNodeEventSemanticsController;
} // namespace hl::game_api::detail
