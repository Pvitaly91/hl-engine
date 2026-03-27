#include "path_mover_controller.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <unordered_map>
#include <utility>

#include "path_arrival_calibrator.h"
#include "path_advance_controller.h"
#include "path_mover_runtime.h"
#include "path_traversal_controller.h"

namespace
{
using hl::game_api::PathMoverAggregateSummary;
using hl::game_api::detail::PathMoverControllerConfig;
using hl::game_api::detail::PathMoverControllerHooks;
using hl::game_api::detail::PathMoverControllerStateSummary;
using hl::game_api::detail::PathMoverFrameState;
using hl::game_api::detail::PathMoverMutableState;
using hl::game_api::detail::PathMoverStage;
using hl::game_api::detail::PathAdvanceController;
using hl::game_api::detail::PathAdvanceControllerConfig;
using hl::game_api::detail::PathAdvanceControllerHooks;
using hl::game_api::detail::PathArrivalCalibrator;
using hl::game_api::detail::PathArrivalCalibratorConfig;
using hl::game_api::detail::PathArrivalCalibratorHooks;
using hl::game_api::detail::PathArrivalCalibrationResult;
using hl::game_api::detail::PathTraversalController;
using hl::game_api::detail::PathTraversalControllerConfig;
using hl::game_api::detail::PathTraversalControllerHooks;
using hl::game_api::detail::PathTraversalFrameResult;
using hl::game_api::detail::ScriptedMovementEntityView;
using hl::game_api::detail::ScriptedMovementFrameContext;
using hl::game_api::detail::TrackPathLookupResult;
using hl::game_api::detail::TrackPathLookupState;
using hl::game_api::detail::TrackPathNodeView;
using hl::game_api::detail::TrackPathResolver;

constexpr std::string_view kFtruckName = "ftruck_a";
constexpr std::string_view kFlatbedStartName = "flatbedstart";
constexpr std::array<std::string_view, 3> kFtruckCanaryNodes = {
    "trainstop1",
    "trainstop1a",
    "trainstop1b",
};

float AngleMod(float value)
{
    while (value < 0.0f)
    {
        value += 360.0f;
    }
    while (value >= 360.0f)
    {
        value -= 360.0f;
    }
    return value;
}

float VecToYaw(const Vector& direction)
{
    if (direction.x == 0.0f && direction.y == 0.0f)
    {
        return 0.0f;
    }

    constexpr float kRadiansToDegrees = 180.0f / 3.14159265358979323846f;
    return AngleMod(std::atan2(direction.y, direction.x) * kRadiansToDegrees);
}

bool IsCanaryNode(std::string_view node_name)
{
    return std::find_if(
               kFtruckCanaryNodes.begin(),
               kFtruckCanaryNodes.end(),
               [&](std::string_view candidate)
               {
                   return hl::game_api::detail::PathMoverEqualsIgnoreCase(node_name, candidate);
               })
        != kFtruckCanaryNodes.end();
}
} // namespace

namespace hl::game_api::detail
{
struct PathMoverController::Impl
{
    struct ActivationRequest
    {
        int edict_index = -1;
        std::string targetname;
        float request_time = 0.0f;
        std::string reason;
        std::string use_source;
    };

    PathMoverControllerConfig config{};
    PathMoverControllerStateSummary summary{};
    ScriptedMovementFrameContext current_frame{};
    std::vector<ActivationRequest> activation_requests;
    std::unordered_map<int, PathMoverMutableState> movers;
    PathTraversalController traversal_controller;
    PathArrivalCalibrator arrival_calibrator;
    PathAdvanceController advance_controller;
    PathNodeEventDispatcher event_dispatcher;

    static void AppendLimited(
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

    void SetStage(
        PathMoverMutableState& mover,
        PathMoverStage new_stage,
        const ScriptedMovementFrameContext& frame,
        std::string_view reason,
        const PathMoverControllerHooks& hooks)
    {
        const bool changed = mover.stage != new_stage;
        mover.stage = new_stage;
        mover.summary.stage = PathMoverStageLabel(new_stage);
        mover.summary.stage_changed_this_frame = changed;
        if (changed)
        {
            mover.summary.last_progress_frame = frame.frame_number;
            mover.summary.last_progress_time = frame.time;
            if (hooks.log_info)
            {
                hooks.log_info(
                    "PathMoverController: "
                    + (mover.summary.targetname.empty()
                        ? std::string("<empty>")
                        : mover.summary.targetname)
                    + " stage=" + mover.summary.stage
                    + (reason.empty() ? std::string() : " reason=" + std::string(reason)));
            }
        }
    }

    void UpdateOriginText(PathMoverMutableState& mover, edict_t* entity) const
    {
        if (entity != nullptr)
        {
            mover.summary.origin_text = FormatPathMoverVector(entity->v.origin);
        }
    }

    void ApplyTraversalResult(
        PathMoverMutableState& mover,
        const PathTraversalFrameResult& result,
        PathMoverFrameState& frame_state)
    {
        if (result.arrived)
        {
            ++frame_state.node_arrivals;
        }
        if (result.advanced)
        {
            ++frame_state.path_advances;
        }
        if (result.stopped || result.completed)
        {
            ++frame_state.stopped_movers;
        }
        if (result.blocked)
        {
            ++frame_state.blocked_movers;
        }
        if (!result.message_summary.empty())
        {
            frame_state.messages_this_frame.push_back(result.message_summary);
            frame_state.events.push_back(result.message_summary);
            AppendLimited(summary.messages_encountered, result.message_summary, config.frame_history_limit);
        }
        if (!result.dispatch_summary.empty())
        {
            frame_state.events.push_back(result.dispatch_summary);
            AppendLimited(summary.message_dispatches, result.dispatch_summary, config.frame_history_limit);
        }
        if (!result.trace_summary.empty())
        {
            frame_state.events.push_back(result.trace_summary);
        }
        if (!result.broken_link_summary.empty())
        {
            frame_state.events.push_back(result.broken_link_summary);
            AppendLimited(summary.broken_link_events, result.broken_link_summary, config.frame_history_limit);
        }

        (void)mover;
    }

    std::string BuildFtruckStatus() const
    {
        for (const auto& [edict_index, mover] : movers)
        {
            (void)edict_index;
            if (!PathMoverEqualsIgnoreCase(mover.summary.targetname, kFtruckName))
            {
                continue;
            }

            return mover.summary.stage
                + " previous="
                + (mover.summary.previous_node.empty()
                    ? std::string("<none>")
                    : mover.summary.previous_node)
                + " current="
                + (mover.summary.current_node.empty()
                    ? std::string("<none>")
                    : mover.summary.current_node)
                + " next="
                + (mover.summary.next_node.empty()
                    ? std::string("<none>")
                    : mover.summary.next_node)
                + " arrivals=" + std::to_string(mover.summary.node_arrivals)
                + " advances=" + std::to_string(mover.summary.node_advances)
                + " lastMessage="
                + (mover.summary.last_message.empty()
                    ? std::string("<none>")
                    : mover.summary.last_message)
                + " dispatch="
                + (mover.summary.last_message_dispatch_result.empty()
                    ? std::string("<none>")
                    : mover.summary.last_message_dispatch_result)
                + " blocked="
                + (mover.summary.blocked_reason.empty()
                    ? std::string("<none>")
                    : mover.summary.blocked_reason)
                + " stopped="
                + (mover.summary.stopped_reason.empty()
                    ? std::string("<none>")
                    : mover.summary.stopped_reason);
        }

        return "unresolved";
    }

    bool ShouldEmitFtruckCanary(const PathMoverMutableState& mover) const
    {
        if (!PathMoverEqualsIgnoreCase(mover.summary.targetname, kFtruckName))
        {
            return false;
        }

        return IsCanaryNode(mover.summary.previous_node)
            || IsCanaryNode(mover.summary.current_node)
            || IsCanaryNode(mover.summary.next_node)
            || IsCanaryNode(mover.summary.resolved_start_node);
    }

    void AppendFrameEvent(PathMoverFrameState& frame_state, std::string event) const
    {
        frame_state.events.push_back(std::move(event));
    }

    void EmitCanaryTrace(
        const PathMoverMutableState& mover,
        std::string_view stage_label,
        PathMoverFrameState& frame_state) const
    {
        if (!ShouldEmitFtruckCanary(mover)
            || (!config.trace_movement
                && !mover.summary.arrived_this_frame
                && !mover.summary.stage_changed_this_frame
                && mover.summary.last_message.empty()))
        {
            return;
        }

        AppendFrameEvent(
            frame_state,
            "canary stage=" + std::string(stage_label)
                + " mover="
                + (mover.summary.targetname.empty()
                    ? std::string("<empty>")
                    : mover.summary.targetname)
                + " previous="
                + (mover.summary.previous_node.empty()
                    ? std::string("<none>")
                    : mover.summary.previous_node)
                + " current="
                + (mover.summary.current_node.empty()
                    ? std::string("<none>")
                    : mover.summary.current_node)
                + " next="
                + (mover.summary.next_node.empty()
                    ? std::string("<none>")
                    : mover.summary.next_node)
                + " origin="
                + (mover.summary.origin_text.empty()
                    ? std::string("<unset>")
                    : mover.summary.origin_text)
                + " distanceToNext=" + std::to_string(mover.summary.last_arrival_distance)
                + " arrivalDecision="
                + (mover.summary.last_arrival_decision.empty()
                    ? std::string("<unset>")
                    : mover.summary.last_arrival_decision)
                + " pathAdvances=" + std::to_string(mover.summary.node_advances)
                + " lastMessage="
                + (mover.summary.last_message.empty()
                    ? std::string("<none>")
                    : mover.summary.last_message)
                + " speed=" + std::to_string(mover.summary.effective_speed)
                + " speedDecision="
                + (mover.summary.last_speed_decision.empty()
                    ? std::string("<unset>")
                    : mover.summary.last_speed_decision)
                + " speedBeforeArrival="
                + std::to_string(mover.summary.last_speed_before_arrival)
                + " speedAfterArrival="
                + std::to_string(mover.summary.last_speed_after_arrival)
                + " speedChanged="
                + (mover.summary.last_speed_changed_on_arrival ? "yes" : "no")
                + " dispatch="
                + (mover.summary.last_message_dispatch_result.empty()
                    ? std::string("<none>")
                    : mover.summary.last_message_dispatch_result));
    }
};

PathMoverController::PathMoverController()
    : impl_(std::make_unique<Impl>())
{
}

PathMoverController::~PathMoverController() = default;

void PathMoverController::Configure(const PathMoverControllerConfig& config)
{
    impl_->config = config;
    if (impl_->config.arrival_epsilon <= 0.0f)
    {
        impl_->config.arrival_epsilon = 24.0f;
    }
    if (impl_->config.preview_limit == 0)
    {
        impl_->config.preview_limit = 8;
    }
    if (impl_->config.frame_history_limit == 0)
    {
        impl_->config.frame_history_limit = 128;
    }
    if (impl_->config.similar_name_limit == 0)
    {
        impl_->config.similar_name_limit = 4;
    }

    PathTraversalControllerConfig traversal_config;
    traversal_config.default_path_speed = impl_->config.default_path_speed;
    traversal_config.arrival_epsilon = impl_->config.arrival_epsilon;
    traversal_config.snap_to_node_on_arrival = true;
    traversal_config.trace_movement = impl_->config.trace_movement;
    impl_->traversal_controller.Configure(traversal_config);

    PathArrivalCalibratorConfig arrival_config;
    arrival_config.arrival_epsilon = impl_->config.arrival_epsilon;
    arrival_config.snap_to_node_on_arrival = true;
    arrival_config.trace_movement = impl_->config.trace_movement;
    impl_->arrival_calibrator.Configure(arrival_config);

    PathAdvanceControllerConfig advance_config;
    advance_config.default_path_speed = impl_->config.default_path_speed;
    advance_config.trace_movement = impl_->config.trace_movement;
    impl_->advance_controller.Configure(advance_config);

    PathNodeEventDispatcherConfig event_config;
    event_config.history_limit = impl_->config.frame_history_limit;
    event_config.trace_movement = impl_->config.trace_movement;
    impl_->event_dispatcher.Configure(event_config);

    impl_->summary = {};
    impl_->summary.configured = true;
    impl_->summary.effective_arrival_epsilon = impl_->config.arrival_epsilon;
    impl_->activation_requests.clear();
    impl_->movers.clear();
}

void PathMoverController::BeginFrame(const ScriptedMovementFrameContext& context)
{
    impl_->current_frame = context;
    ++impl_->summary.frames_attempted;

    for (auto& [edict_index, mover] : impl_->movers)
    {
        (void)edict_index;
        mover.summary.stage_changed_this_frame = false;
        mover.summary.arrived_this_frame = false;
        mover.summary.moving = false;
        mover.summary.last_arrival_snap_applied = false;
    }
}

void PathMoverController::RequestActivation(
    int edict_index,
    std::string targetname,
    float request_time,
    std::string reason,
    std::string use_source)
{
    impl_->activation_requests.push_back({
        edict_index,
        std::move(targetname),
        request_time,
        std::move(reason),
        std::move(use_source),
    });
}

void PathMoverController::RunFrame(
    const ScriptedMovementFrameContext& context,
    const TrackPathResolver& path_resolver,
    const std::vector<ScriptedMovementEntityView>& entities,
    const PathMoverControllerHooks& hooks)
{
    PathMoverFrameState frame;
    frame.frame_number = context.frame_number;
    frame.host_frame_index = context.host_frame_index;
    frame.server_frame_index = context.server_frame_index;
    frame.time = context.time;
    frame.frametime = context.frametime;

    PathTraversalControllerHooks traversal_hooks;
    traversal_hooks.set_origin = hooks.set_origin;
    traversal_hooks.set_velocity = hooks.set_velocity;
    traversal_hooks.set_avelocity = hooks.set_avelocity;
    traversal_hooks.set_stage =
        [&](PathMoverMutableState& mover, PathMoverStage stage, std::string_view reason)
        {
            impl_->SetStage(mover, stage, context, reason, hooks);
        };
    traversal_hooks.log_info = hooks.log_info;
    traversal_hooks.log_warn = hooks.log_warn;
    traversal_hooks.event_dispatcher = &impl_->event_dispatcher;
    traversal_hooks.event_hooks.dispatch_target_event = hooks.dispatch_target_event;
    traversal_hooks.event_hooks.log_info = hooks.log_info;
    traversal_hooks.event_hooks.log_warn = hooks.log_warn;

    PathArrivalCalibratorHooks arrival_hooks;
    arrival_hooks.set_origin = hooks.set_origin;
    arrival_hooks.log_info = hooks.log_info;
    arrival_hooks.log_warn = hooks.log_warn;

    PathAdvanceControllerHooks advance_hooks;
    advance_hooks.set_velocity = hooks.set_velocity;
    advance_hooks.set_avelocity = hooks.set_avelocity;
    advance_hooks.set_stage =
        [&](PathMoverMutableState& mover, PathMoverStage stage, std::string_view reason)
        {
            impl_->SetStage(mover, stage, context, reason, hooks);
        };
    advance_hooks.log_info = hooks.log_info;
    advance_hooks.log_warn = hooks.log_warn;
    advance_hooks.event_dispatcher = &impl_->event_dispatcher;
    advance_hooks.event_hooks.dispatch_target_event = hooks.dispatch_target_event;
    advance_hooks.event_hooks.log_info = hooks.log_info;
    advance_hooks.event_hooks.log_warn = hooks.log_warn;

    std::unordered_map<int, std::vector<Impl::ActivationRequest>> requests_by_edict;
    for (const Impl::ActivationRequest& request : impl_->activation_requests)
    {
        requests_by_edict[request.edict_index].push_back(request);
    }

    for (const ScriptedMovementEntityView& mover_view : entities)
    {
        if (!PathMoverEqualsIgnoreCase(mover_view.classname, "func_tracktrain"))
        {
            continue;
        }
        if (mover_view.edict_index < 0 || !mover_view.in_use || mover_view.removed)
        {
            continue;
        }

        PathMoverMutableState& mover = impl_->movers[mover_view.edict_index];
        mover.summary.edict_index = mover_view.edict_index;
        mover.summary.parse_index = mover_view.parse_index;
        mover.summary.classname = mover_view.classname;
        mover.summary.targetname = mover_view.targetname;
        mover.summary.model = mover_view.model;
        mover.summary.modelindex = mover_view.modelindex;
        mover.summary.requested_start_node = mover_view.target;
        mover.summary.arrival_epsilon = impl_->config.arrival_epsilon;
        if (mover.summary.base_speed <= 0.0f)
        {
            mover.summary.base_speed =
                mover_view.start_speed > 0.0f
                ? mover_view.start_speed
                : (mover_view.speed > 0.0f ? mover_view.speed : impl_->config.default_path_speed);
        }
        if (mover.summary.speed <= 0.0f)
        {
            mover.summary.speed = mover.summary.base_speed;
        }

        edict_t* mover_entity =
            hooks.entity_by_index ? hooks.entity_by_index(mover_view.edict_index) : nullptr;
        if (mover_entity != nullptr)
        {
            impl_->UpdateOriginText(mover, mover_entity);
        }
        else if (!mover_view.origin_text.empty())
        {
            mover.summary.origin_text = mover_view.origin_text;
        }

        bool activation_requested_this_frame = false;
        if (const auto request_it = requests_by_edict.find(mover_view.edict_index);
            request_it != requests_by_edict.end())
        {
            activation_requested_this_frame = true;
            mover.summary.activated = true;
            mover.summary.last_use_source = request_it->second.back().use_source;
        }

        const Vector* bind_origin = nullptr;
        Vector fallback_origin(0.0f, 0.0f, 0.0f);
        if (mover_entity != nullptr)
        {
            fallback_origin = mover_entity->v.origin;
            bind_origin = &fallback_origin;
        }
        else if (mover_view.has_origin)
        {
            fallback_origin = mover_view.origin;
            bind_origin = &fallback_origin;
        }

        if (!mover.summary.start_target_resolved || !mover.summary.path_bound
            || activation_requested_this_frame)
        {
            const TrackPathLookupResult lookup = path_resolver.ResolveStartNode(
                mover.summary.requested_start_node,
                bind_origin,
                impl_->config.start_bind_radius,
                impl_->config.similar_name_limit);

            mover.summary.resolution_mode = lookup.resolution_mode;
            mover.summary.resolution_detail = lookup.detail;
            if (!lookup.similar_names.empty())
            {
                mover.summary.resolution_detail += " similar=";
                bool first = true;
                for (const std::string& candidate : lookup.similar_names)
                {
                    if (!first)
                    {
                        mover.summary.resolution_detail += ",";
                    }
                    mover.summary.resolution_detail += candidate;
                    first = false;
                }
            }

            if (lookup.state == TrackPathLookupState::kNoNodeFound)
            {
                mover.summary.start_target_resolved = false;
                mover.summary.graph_valid = false;
                mover.summary.path_bound = false;
                mover.summary.resolved_start_node.clear();
                if (mover.summary.current_node.empty())
                {
                    mover.summary.next_node.clear();
                }
                mover.summary.blocked_reason =
                    "track start '" + mover.summary.requested_start_node + "' not resolved";
                mover.summary.stopped_reason.clear();
                impl_->SetStage(
                    mover,
                    activation_requested_this_frame || mover.summary.activated
                        ? PathMoverStage::kBlocked
                        : PathMoverStage::kUnresolved,
                    context,
                    mover.summary.blocked_reason,
                    hooks);
            }
            else
            {
                mover.summary.start_target_resolved = true;
                mover.summary.resolved_start_node =
                    lookup.node != nullptr ? lookup.node->targetname : std::string();
                impl_->SetStage(
                    mover,
                    PathMoverStage::kStartResolved,
                    context,
                    mover.summary.resolution_detail,
                    hooks);

                if (lookup.node == nullptr || !path_resolver.IsGraphValidStartNode(*lookup.node))
                {
                    mover.summary.graph_valid = false;
                    mover.summary.path_bound = false;
                    mover.summary.blocked_reason =
                        "track start '" + mover.summary.requested_start_node
                        + "' resolved to '"
                        + (mover.summary.resolved_start_node.empty()
                            ? std::string("<empty>")
                            : mover.summary.resolved_start_node)
                        + "' but graph is broken";
                    mover.summary.stopped_reason.clear();
                    impl_->SetStage(
                        mover,
                        activation_requested_this_frame || mover.summary.activated
                            ? PathMoverStage::kBlocked
                            : PathMoverStage::kStartResolved,
                        context,
                        mover.summary.blocked_reason,
                        hooks);
                }
                else
                {
                    mover.summary.graph_valid = true;
                    mover.summary.path_bound = true;
                    mover.summary.blocked_reason.clear();
                    if (mover.summary.current_node.empty())
                    {
                        mover.summary.current_node = lookup.node->targetname;
                        if (const TrackPathNodeView* next = path_resolver.ResolveNext(*lookup.node);
                            next != nullptr)
                        {
                            mover.summary.next_node = next->targetname;
                        }
                    }

                    if (!mover.snapped_to_start && mover_entity != nullptr
                        && hooks.set_origin != nullptr && lookup.node->has_origin)
                    {
                        hooks.set_origin(mover_entity, lookup.node->origin);
                        if (hooks.set_velocity)
                        {
                            hooks.set_velocity(mover_entity, Vector(0.0f, 0.0f, 0.0f));
                        }
                        if (hooks.set_avelocity)
                        {
                            hooks.set_avelocity(mover_entity, Vector(0.0f, 0.0f, 0.0f));
                        }
                        mover.snapped_to_start = true;
                        impl_->UpdateOriginText(mover, mover_entity);
                        if (hooks.log_info)
                        {
                            hooks.log_info(
                                "PathMoverController: "
                                + (mover.summary.targetname.empty()
                                    ? std::string("<empty>")
                                    : mover.summary.targetname)
                                + " snappedToStart node="
                                + lookup.node->targetname
                                + " origin=" + mover.summary.origin_text);
                        }
                    }

                    impl_->SetStage(
                        mover,
                        PathMoverStage::kGraphBound,
                        context,
                        mover.summary.resolution_detail,
                        hooks);
                }
            }

            if (activation_requested_this_frame && hooks.log_info)
            {
                hooks.log_info(
                    "PathMoverController: activation mover="
                    + (mover.summary.targetname.empty()
                        ? std::string("<empty>")
                        : mover.summary.targetname)
                    + " requestedStart="
                    + (mover.summary.requested_start_node.empty()
                        ? std::string("<empty>")
                        : mover.summary.requested_start_node)
                    + " resolvedStart="
                    + (mover.summary.resolved_start_node.empty()
                        ? std::string("<empty>")
                        : mover.summary.resolved_start_node)
                    + " stage=" + mover.summary.stage
                    + " useSource="
                    + (mover.summary.last_use_source.empty()
                        ? std::string("<unknown>")
                        : mover.summary.last_use_source));
            }
        }

        if (!mover.summary.activated || !mover.summary.path_bound)
        {
            if (!mover.summary.blocked_reason.empty()
                && mover.summary.stage == PathMoverStageLabel(PathMoverStage::kBlocked))
            {
                ++frame.blocked_movers;
            }
            continue;
        }

        ++frame.active_movers;
        if (mover.stage != PathMoverStage::kMoving
            && mover.stage != PathMoverStage::kArrivedAtNode
            && mover.stage != PathMoverStage::kCompleted
            && mover.stage != PathMoverStage::kStopped)
        {
            impl_->SetStage(mover, PathMoverStage::kActivated, context, "ready-to-move", hooks);
        }

        const TrackPathNodeView* current_node =
            path_resolver.FindNodeByName(mover.summary.current_node);
        const TrackPathNodeView* next_node =
            !mover.summary.next_node.empty()
            ? path_resolver.FindNodeByName(mover.summary.next_node)
            : (current_node != nullptr ? path_resolver.ResolveNext(*current_node) : nullptr);
        mover.summary.next_node = next_node != nullptr ? next_node->targetname : std::string();

        if (mover_entity == nullptr || current_node == nullptr)
        {
            mover.summary.path_bound = false;
            mover.summary.graph_valid = false;
            mover.summary.blocked_reason = current_node == nullptr
                ? "current node '" + mover.summary.current_node + "' is missing from the graph"
                : "path-bound mover missing runtime entity";
            mover.summary.stopped_reason.clear();
            impl_->SetStage(mover, PathMoverStage::kBlocked, context, mover.summary.blocked_reason, hooks);
            ++frame.blocked_movers;
            continue;
        }

        mover.summary.blocked_reason.clear();
        mover.summary.stopped_reason.clear();

        if (impl_->config.trace_movement)
        {
            impl_->AppendFrameEvent(
                frame,
                "pipeline mover="
                    + (mover.summary.targetname.empty()
                        ? std::string("<empty>")
                        : mover.summary.targetname)
                    + " order=movement-update -> arrival-calibrator -> snap-to-node -> path-advance -> node-event-dispatch");
        }

        if (next_node != nullptr && next_node->has_origin)
        {
            const float speed =
                impl_->traversal_controller.ResolveSegmentSpeed(
                    mover,
                    current_node,
                    next_node,
                    traversal_hooks);

            const Vector current_origin = mover_entity->v.origin;
            mover.previous_origin = current_origin;
            mover.previous_origin_valid = true;

            const Vector delta = next_node->origin - current_origin;
            const float distance = delta.Length();
            if (distance > 0.0f)
            {
                mover_entity->v.ideal_yaw = VecToYaw(delta);
                if (mover_entity->v.yaw_speed <= 0.0f)
                {
                    mover_entity->v.yaw_speed = 180.0f;
                }
                if (hooks.change_yaw)
                {
                    hooks.change_yaw(mover_entity);
                }

                const float step = std::min(speed * context.frametime, distance);
                const Vector new_origin =
                    step >= distance ? next_node->origin : current_origin + (delta * (step / distance));
                if (hooks.set_origin)
                {
                    hooks.set_origin(mover_entity, new_origin);
                }
                if (hooks.set_velocity)
                {
                    hooks.set_velocity(
                        mover_entity,
                        step >= distance ? Vector(0.0f, 0.0f, 0.0f) : delta * (speed / distance));
                }
                if (hooks.set_avelocity)
                {
                    hooks.set_avelocity(mover_entity, Vector(0.0f, 0.0f, 0.0f));
                }
                mover.summary.moving = true;
                impl_->UpdateOriginText(mover, mover_entity);
                if ((next_node->origin - mover_entity->v.origin).Length() > impl_->config.arrival_epsilon)
                {
                    impl_->SetStage(
                        mover,
                        PathMoverStage::kMoving,
                        context,
                        "toward=" + mover.summary.next_node,
                        hooks);
                }
            }
        }

        const PathArrivalCalibrationResult arrival_result =
            impl_->arrival_calibrator.Evaluate(
                mover,
                mover_entity,
                current_node,
                next_node,
                context,
                arrival_hooks);
        const PathTraversalFrameResult traversal_result =
            impl_->advance_controller.HandlePostCalibration(
                mover,
                mover_entity,
                path_resolver,
                current_node,
                next_node,
                context,
                arrival_result,
                advance_hooks);
        impl_->UpdateOriginText(mover, mover_entity);
        impl_->ApplyTraversalResult(mover, traversal_result, frame);
        impl_->EmitCanaryTrace(
            mover,
            traversal_result.arrived ? "post-advance" : "post-move",
            frame);

        if (mover.summary.moving)
        {
            ++frame.moving_movers;
        }
    }

    impl_->summary.path_graph = path_resolver.Summary();
    impl_->summary.path_movers = PathMoverAggregateSummary{};
    impl_->summary.effective_arrival_epsilon = impl_->config.arrival_epsilon;
    impl_->summary.snap_to_node_occurred = false;
    impl_->summary.flatbedstart_resolved = false;
    impl_->summary.movers_preview.clear();
    impl_->summary.path_node_messages = impl_->event_dispatcher.Summary();
    impl_->summary.messages_encountered = impl_->summary.path_node_messages.encountered_history;
    impl_->summary.message_dispatch_count =
        impl_->summary.path_node_messages.staged_dispatch_attempts;
    impl_->summary.message_dispatch_triggered =
        impl_->summary.path_node_messages.staged_dispatch_successes > 0;
    impl_->summary.message_dispatches = impl_->summary.path_node_messages.dispatch_history;
    impl_->summary.path_movers.staged_message_dispatches =
        impl_->summary.path_node_messages.staged_dispatch_attempts;

    std::vector<const PathMoverMutableState*> ordered_movers;
    ordered_movers.reserve(impl_->movers.size());
    for (const auto& [edict_index, mover] : impl_->movers)
    {
        (void)edict_index;
        ordered_movers.push_back(&mover);
        ++impl_->summary.path_movers.func_tracktrain_resolved;
        if (mover.summary.start_target_resolved)
        {
            ++impl_->summary.path_movers.start_targets_resolved;
        }
        if (mover.summary.path_bound)
        {
            ++impl_->summary.path_movers.track_bound;
        }
        if (mover.summary.activated)
        {
            ++impl_->summary.path_movers.activated;
        }
        if (mover.summary.moving)
        {
            ++impl_->summary.path_movers.moving;
        }
        if (mover.summary.any_arrival_snap_applied)
        {
            impl_->summary.snap_to_node_occurred = true;
        }
        if (mover.summary.node_arrivals > 0)
        {
            ++impl_->summary.path_movers.arrived_at_node;
        }
        if (mover.stage == PathMoverStage::kStopped || mover.stage == PathMoverStage::kCompleted)
        {
            ++impl_->summary.path_movers.stopped;
        }
        if (mover.stage == PathMoverStage::kCompleted)
        {
            ++impl_->summary.path_movers.completed;
        }
        if (mover.stage == PathMoverStage::kBlocked || !mover.summary.blocked_reason.empty())
        {
            ++impl_->summary.path_movers.blocked;
        }
        impl_->summary.path_movers.node_arrivals +=
            static_cast<std::size_t>(mover.summary.node_arrivals);
        impl_->summary.path_movers.path_advances +=
            static_cast<std::size_t>(mover.summary.node_advances);
        if (!mover.summary.blocked_reason.empty()
            && mover.summary.blocked_reason.find("broken path link") != std::string::npos)
        {
            ++impl_->summary.path_movers.broken_link_hits;
        }
        if (PathMoverEqualsIgnoreCase(mover.summary.requested_start_node, kFlatbedStartName)
            && mover.summary.start_target_resolved)
        {
            impl_->summary.flatbedstart_resolved = true;
        }
    }

    std::sort(
        ordered_movers.begin(),
        ordered_movers.end(),
        [](const PathMoverMutableState* left, const PathMoverMutableState* right)
        {
            if (PathMoverStageRank(left->stage) != PathMoverStageRank(right->stage))
            {
                return PathMoverStageRank(left->stage) > PathMoverStageRank(right->stage);
            }
            return left->summary.targetname < right->summary.targetname;
        });

    for (const PathMoverMutableState* mover : ordered_movers)
    {
        if (impl_->summary.movers_preview.size() < impl_->config.preview_limit)
        {
            impl_->summary.movers_preview.push_back(mover->summary);
        }
        if ((mover->summary.activated || mover->summary.path_bound
                || PathMoverEqualsIgnoreCase(mover->summary.targetname, kFtruckName))
            && frame.active_movers_preview.size() < impl_->config.preview_limit)
        {
            frame.active_movers_preview.push_back(
                (mover->summary.targetname.empty()
                    ? std::string("<empty>")
                    : mover->summary.targetname)
                + " stage=" + mover->summary.stage
                + " previous=" + (mover->summary.previous_node.empty()
                    ? std::string("<none>")
                    : mover->summary.previous_node)
                + " current=" + (mover->summary.current_node.empty()
                    ? std::string("<none>")
                    : mover->summary.current_node)
                + " next=" + (mover->summary.next_node.empty()
                    ? std::string("<none>")
                    : mover->summary.next_node)
                + " origin=" + (mover->summary.origin_text.empty()
                    ? std::string("<unset>")
                    : mover->summary.origin_text)
                + " distance=" + std::to_string(mover->summary.last_arrival_distance)
                + " arrivalDecision=" + (mover->summary.last_arrival_decision.empty()
                    ? std::string("<unset>")
                    : mover->summary.last_arrival_decision)
                + " speed=" + std::to_string(mover->summary.effective_speed)
                + " speedDecision=" + (mover->summary.last_speed_decision.empty()
                    ? std::string("<unset>")
                    : mover->summary.last_speed_decision)
                + " speedBeforeArrival="
                + std::to_string(mover->summary.last_speed_before_arrival)
                + " speedAfterArrival="
                + std::to_string(mover->summary.last_speed_after_arrival)
                + " speedChanged="
                + (mover->summary.last_speed_changed_on_arrival ? "yes" : "no")
                + " arrivals=" + std::to_string(mover->summary.node_arrivals)
                + " advances=" + std::to_string(mover->summary.node_advances)
                + " message=" + (mover->summary.last_message.empty()
                    ? std::string("<none>")
                    : mover->summary.last_message)
                + " dispatch=" + (mover->summary.last_message_dispatch_result.empty()
                    ? std::string("<none>")
                    : mover->summary.last_message_dispatch_result)
                + " blocked=" + (mover->summary.blocked_reason.empty()
                    ? std::string("<none>")
                    : mover->summary.blocked_reason)
                + " stopped=" + (mover->summary.stopped_reason.empty()
                    ? std::string("<none>")
                    : mover->summary.stopped_reason));
        }
    }

    impl_->summary.delayed_ftruck_status = impl_->BuildFtruckStatus();
    if (const PathMoverRuntimeSummary* ftruck = FindMoverByTargetname(kFtruckName);
        ftruck != nullptr)
    {
        impl_->summary.ftruck_final_current = ftruck->current_node;
        impl_->summary.ftruck_final_next = ftruck->next_node;
        impl_->summary.ftruck_advanced_beyond_trainstop1a =
            !PathMoverEqualsIgnoreCase(ftruck->current_node, "trainstop1")
            || !PathMoverEqualsIgnoreCase(ftruck->next_node, "trainstop1a");
    }
    else
    {
        impl_->summary.ftruck_final_current.clear();
        impl_->summary.ftruck_final_next.clear();
        impl_->summary.ftruck_advanced_beyond_trainstop1a = false;
    }
    frame.ftruck_status = impl_->summary.delayed_ftruck_status;

    if (impl_->summary.path_node_messages.staged_dispatch_successes > 0
        && impl_->summary.path_node_messages.first_message_bearing_node_reached)
    {
        impl_->summary.readiness =
            "ready for deeper path-node event semantics, passive actor/server groundwork, and later controlled client bootstrap groundwork";
    }
    else if (impl_->summary.path_node_messages.encountered > 0)
    {
        impl_->summary.readiness =
            "path-node messages are surfacing; deepen staged dispatch semantics and controlled event-chain expansion next";
    }
    else if (impl_->summary.path_movers.path_advances > 0)
    {
        impl_->summary.readiness =
            "path traversal is stable; deepen path-node event semantics and passive actor/server groundwork next";
    }
    else if (impl_->summary.path_movers.moving > 0)
    {
        impl_->summary.readiness =
            "path mover traversal is active; longer deterministic runs can probe arrivals, events, and broken-link handling";
    }
    else if (impl_->summary.path_movers.blocked > 0)
    {
        impl_->summary.readiness =
            "path mover traversal is diagnosable; resolve the top bind or broken-chain blocker next";
    }
    else
    {
        impl_->summary.readiness =
            "path mover traversal is configured; delayed activation or longer deterministic runs are next";
    }

    impl_->summary.frames.push_back(frame);
    while (impl_->summary.frames.size() > impl_->config.frame_history_limit)
    {
        impl_->summary.frames.erase(impl_->summary.frames.begin());
    }
    ++impl_->summary.frames_completed;
    impl_->summary.controller_ran = true;
    impl_->activation_requests.clear();
}

const PathMoverControllerStateSummary& PathMoverController::Summary() const noexcept
{
    return impl_->summary;
}

const PathMoverRuntimeSummary* PathMoverController::FindMoverState(int edict_index) const noexcept
{
    const auto it = impl_->movers.find(edict_index);
    return it != impl_->movers.end() ? &it->second.summary : nullptr;
}

const PathMoverRuntimeSummary* PathMoverController::FindMoverByTargetname(
    std::string_view targetname) const noexcept
{
    for (const auto& [edict_index, mover] : impl_->movers)
    {
        (void)edict_index;
        if (PathMoverEqualsIgnoreCase(mover.summary.targetname, targetname))
        {
            return &mover.summary;
        }
    }

    return nullptr;
}
} // namespace hl::game_api::detail
