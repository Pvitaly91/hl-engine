#include "path_advance_controller.h"

#include <cmath>

namespace
{
std::string BuildTerminalPathCompletionReason(
    const hl::game_api::detail::TrackPathResolver& resolver,
    const hl::game_api::detail::TrackPathNodeView& node)
{
    if (resolver.HasTerminalDeadEndLink(node))
    {
        return "path completed at '" + node.targetname + "' (dead end '" + node.next_target + "')";
    }

    return "path completed at '" + node.targetname + "'";
}
} // namespace

namespace hl::game_api::detail
{
void PathAdvanceController::Configure(const PathAdvanceControllerConfig& config)
{
    config_ = config;
    if (config_.default_path_speed <= 0.0f)
    {
        config_.default_path_speed = 100.0f;
    }
}

PathTraversalFrameResult PathAdvanceController::HandlePostCalibration(
    PathMoverMutableState& mover,
    edict_t* entity,
    const TrackPathResolver& resolver,
    const TrackPathNodeView* current_node,
    const TrackPathNodeView* next_node,
    const ScriptedMovementFrameContext& frame,
    const PathArrivalCalibrationResult& arrival,
    const PathAdvanceControllerHooks& hooks) const
{
    PathTraversalFrameResult result;
    if (entity == nullptr || current_node == nullptr)
    {
        mover.summary.moving = false;
        mover.summary.blocked_reason = "path-bound mover missing runtime entity";
        mover.summary.stopped_reason.clear();
        if (hooks.set_stage)
        {
            hooks.set_stage(mover, PathMoverStage::kBlocked, mover.summary.blocked_reason);
        }
        result.blocked = true;
        return result;
    }

    if (next_node == nullptr)
    {
        mover.summary.moving = false;
        mover.summary.blocked_reason.clear();
        mover.summary.path_bound = false;
        StopMoverMotion(entity, hooks);
        if (resolver.IsTerminalNode(*current_node))
        {
            mover.summary.graph_valid = true;
            mover.summary.stopped_reason = BuildTerminalPathCompletionReason(resolver, *current_node);
            if (hooks.set_stage)
            {
                hooks.set_stage(mover, PathMoverStage::kCompleted, mover.summary.stopped_reason);
            }
            result.completed = true;
            result.stopped = true;
        }
        else
        {
            mover.summary.stopped_reason.clear();
            mover.summary.graph_valid = false;
            mover.summary.blocked_reason =
                "broken path link '" + mover.summary.current_node + "' -> '" + current_node->next_target
                + "'";
            if (hooks.set_stage)
            {
                hooks.set_stage(mover, PathMoverStage::kBlocked, mover.summary.blocked_reason);
            }
            result.blocked = true;
            result.broken_link_summary = mover.summary.blocked_reason;
        }
        mover.summary.next_node.clear();
        return result;
    }

    if (!next_node->has_origin)
    {
        mover.summary.moving = false;
        mover.summary.path_bound = false;
        mover.summary.graph_valid = false;
        mover.summary.stopped_reason.clear();
        mover.summary.blocked_reason =
            "next node '" + next_node->targetname + "' is missing an origin";
        StopMoverMotion(entity, hooks);
        if (hooks.set_stage)
        {
            hooks.set_stage(mover, PathMoverStage::kBlocked, mover.summary.blocked_reason);
        }
        result.blocked = true;
        return result;
    }

    if (!arrival.arrived)
    {
        return result;
    }

    StopMoverMotion(entity, hooks);

    mover.summary.arrived_this_frame = true;
    mover.summary.moving = false;
    mover.summary.previous_node = mover.summary.current_node;
    mover.summary.current_node = next_node->targetname;
    mover.summary.node_arrivals += 1;
    mover.summary.node_advances += 1;
    mover.summary.last_node_arrival_frame = frame.frame_number;
    mover.summary.last_node_arrival_time = frame.time;
    mover.summary.blocked_reason.clear();
    mover.summary.stopped_reason.clear();
    mover.summary.last_speed_before_arrival =
        mover.summary.effective_speed > 0.0f
        ? mover.summary.effective_speed
        : (mover.summary.speed > 0.0f
            ? mover.summary.speed
            : (mover.summary.base_speed > 0.0f ? mover.summary.base_speed : config_.default_path_speed));

    result.arrived = true;
    result.advanced = true;

    const TrackPathNodeView* following = resolver.ResolveNext(*next_node);
    mover.summary.next_node = following != nullptr ? following->targetname : std::string();

    if (next_node->speed > 0.0f)
    {
        mover.summary.speed = next_node->speed;
        mover.summary.effective_speed = next_node->speed;
        mover.summary.last_node_speed_metadata = next_node->speed;
        mover.summary.last_speed_decision = "apply-override";
        mover.summary.speed_policy = "arrival-node-speed-override";
        mover.summary.speed_policy_detail =
            "arrivedNode=" + next_node->targetname
            + " speed=" + std::to_string(next_node->speed)
            + " stagedPolicy=arrival-node-speed-applies-to-next-segment";
        mover.summary.last_speed_decision_detail =
            "arrivedNode=" + next_node->targetname
            + " speedMetadata=" + std::to_string(next_node->speed)
            + " speedBefore=" + std::to_string(mover.summary.last_speed_before_arrival)
            + " speedAfter=" + std::to_string(next_node->speed)
            + " policy=arrival-node-speed-override";
    }
    else
    {
        mover.summary.last_node_speed_metadata = 0.0f;
        mover.summary.last_speed_decision = "retain-current-speed";
        if (mover.summary.speed <= 0.0f)
        {
            mover.summary.speed =
                mover.summary.base_speed > 0.0f ? mover.summary.base_speed : config_.default_path_speed;
            mover.summary.effective_speed = mover.summary.speed;
        }
        mover.summary.last_speed_decision_detail =
            "arrivedNode=" + next_node->targetname
            + " speedMetadata=<none>"
            + " speedBefore=" + std::to_string(mover.summary.last_speed_before_arrival)
            + " speedAfter=" + std::to_string(mover.summary.effective_speed)
            + " policy=retain-current-speed";
    }

    mover.summary.last_speed_after_arrival = mover.summary.effective_speed;
    mover.summary.last_speed_changed_on_arrival =
        std::fabs(mover.summary.last_speed_after_arrival - mover.summary.last_speed_before_arrival)
        > 0.001f;

    if (hooks.log_info
        && (config_.trace_movement
            || PathMoverEqualsIgnoreCase(mover.summary.targetname, "ftruck_a")
            || next_node->speed > 0.0f))
    {
        hooks.log_info(
            "PathAdvanceController: speed mover="
            + (mover.summary.targetname.empty()
                ? std::string("<empty>")
                : mover.summary.targetname)
            + " arrivedNode=" + next_node->targetname
            + " decision="
            + (mover.summary.last_speed_decision.empty()
                ? std::string("<unset>")
                : mover.summary.last_speed_decision)
            + " nodeSpeed="
            + (next_node->speed > 0.0f
                ? std::to_string(next_node->speed)
                : std::string("<none>"))
            + " speedBefore=" + std::to_string(mover.summary.last_speed_before_arrival)
            + " speedAfter=" + std::to_string(mover.summary.last_speed_after_arrival)
            + " speedChanged=" + (mover.summary.last_speed_changed_on_arrival ? "yes" : "no"));
    }

    if (hooks.set_stage)
    {
        hooks.set_stage(mover, PathMoverStage::kArrivedAtNode, "arrived=" + mover.summary.current_node);
    }

    if (hooks.log_info)
    {
        hooks.log_info(
            "PathAdvanceController: advance mover="
            + (mover.summary.targetname.empty()
                ? std::string("<empty>")
                : mover.summary.targetname)
            + " previousCurrent="
            + (mover.summary.previous_node.empty()
                ? std::string("<none>")
                : mover.summary.previous_node)
            + " arrivedNode=" + mover.summary.current_node
            + " newNext="
            + (mover.summary.next_node.empty()
                ? std::string("<none>")
                : mover.summary.next_node)
            + " speedBeforeArrival=" + std::to_string(mover.summary.last_speed_before_arrival)
            + " speedAfterArrival=" + std::to_string(mover.summary.last_speed_after_arrival)
            + " speedChanged="
            + (mover.summary.last_speed_changed_on_arrival ? "yes" : "no"));
    }

    if (hooks.event_dispatcher != nullptr)
    {
        const PathNodeEventDispatchResult event_result =
            hooks.event_dispatcher->HandleArrival(mover, *next_node, frame, hooks.event_hooks);
        result.message_summary = event_result.encountered_summary;
        result.dispatch_summary = event_result.dispatch_summary;
        result.trace_summary = event_result.trace_summary;
    }

    if (following != nullptr)
    {
        return result;
    }

    mover.summary.path_bound = false;
    if (resolver.IsTerminalNode(*next_node))
    {
        mover.summary.graph_valid = true;
        mover.summary.stopped_reason = BuildTerminalPathCompletionReason(resolver, *next_node);
        if (hooks.set_stage)
        {
            hooks.set_stage(mover, PathMoverStage::kCompleted, mover.summary.stopped_reason);
        }
        result.completed = true;
        result.stopped = true;
    }
    else
    {
        mover.summary.graph_valid = false;
        mover.summary.blocked_reason =
            "broken path link '" + next_node->targetname + "' -> '" + next_node->next_target + "'";
        mover.summary.stopped_reason.clear();
        if (hooks.set_stage)
        {
            hooks.set_stage(mover, PathMoverStage::kBlocked, mover.summary.blocked_reason);
        }
        result.blocked = true;
        result.broken_link_summary = mover.summary.blocked_reason;
    }

    return result;
}

void PathAdvanceController::StopMoverMotion(
    edict_t* entity,
    const PathAdvanceControllerHooks& hooks)
{
    if (entity == nullptr)
    {
        return;
    }

    if (hooks.set_velocity)
    {
        hooks.set_velocity(entity, Vector(0.0f, 0.0f, 0.0f));
    }
    if (hooks.set_avelocity)
    {
        hooks.set_avelocity(entity, Vector(0.0f, 0.0f, 0.0f));
    }
}
} // namespace hl::game_api::detail
