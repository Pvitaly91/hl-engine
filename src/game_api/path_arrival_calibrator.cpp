#include "path_arrival_calibrator.h"

#include <cmath>

namespace hl::game_api::detail
{
namespace
{
bool VectorExactlyEquals(const Vector& left, const Vector& right) noexcept
{
    return left.x == right.x && left.y == right.y && left.z == right.z;
}
} // namespace

void PathArrivalCalibrator::Configure(const PathArrivalCalibratorConfig& config)
{
    config_ = config;
    if (config_.arrival_epsilon <= 0.0f)
    {
        config_.arrival_epsilon = 24.0f;
    }
}

const PathArrivalCalibratorConfig& PathArrivalCalibrator::Config() const noexcept
{
    return config_;
}

PathArrivalCalibrationResult PathArrivalCalibrator::Evaluate(
    PathMoverMutableState& mover,
    edict_t* entity,
    const TrackPathNodeView* current_node,
    const TrackPathNodeView* next_node,
    const ScriptedMovementFrameContext& frame,
    const PathArrivalCalibratorHooks& hooks) const
{
    PathArrivalCalibrationResult result;
    result.evaluated = true;
    result.epsilon = config_.arrival_epsilon;

    if (entity == nullptr || next_node == nullptr || !next_node->has_origin)
    {
        mover.summary.last_arrival_distance = 0.0f;
        mover.summary.last_arrival_decision =
            entity == nullptr ? "skipped-no-entity"
            : next_node == nullptr ? "skipped-no-next-node"
                                   : "skipped-next-node-missing-origin";
        mover.summary.last_arrival_trigger = "unavailable";
        mover.summary.last_arrival_origin_text =
            entity != nullptr ? FormatPathMoverVector(entity->v.origin) : std::string("<unset>");
        mover.summary.last_arrival_target_origin_text =
            next_node != nullptr && next_node->has_origin
            ? FormatPathMoverVector(next_node->origin)
            : std::string("<unset>");
        mover.summary.last_arrival_snap_applied = false;
        result.decision = mover.summary.last_arrival_decision;
        result.trigger = mover.summary.last_arrival_trigger;
        return result;
    }

    const Vector mover_origin = entity->v.origin;
    const Vector target_origin = next_node->origin;
    const Vector delta = target_origin - mover_origin;
    const float distance_squared =
        (delta.x * delta.x) + (delta.y * delta.y) + (delta.z * delta.z);
    const float epsilon_squared = config_.arrival_epsilon * config_.arrival_epsilon;

    result.distance = std::sqrt(distance_squared);
    result.exact_hit = distance_squared == 0.0f;
    result.within_epsilon = distance_squared <= epsilon_squared;
    result.arrived = result.within_epsilon;
    result.snap_applied =
        result.arrived
        && !result.exact_hit
        && config_.snap_to_node_on_arrival
        && hooks.set_origin != nullptr
        && !VectorExactlyEquals(mover_origin, target_origin);
    result.trigger = result.exact_hit
        ? "exact-hit"
        : result.within_epsilon ? "epsilon-threshold" : "distance-exceeded";
    result.decision = result.arrived
        ? (result.snap_applied ? "snap-to-node" : result.trigger)
        : "not-arrived";

    mover.summary.last_arrival_distance = result.distance;
    mover.summary.last_arrival_decision = result.decision;
    mover.summary.last_arrival_trigger = result.trigger;
    mover.summary.last_arrival_origin_text = FormatPathMoverVector(mover_origin);
    mover.summary.last_arrival_target_origin_text = FormatPathMoverVector(target_origin);
    mover.summary.last_arrival_snap_applied = result.snap_applied;

    if (result.snap_applied)
    {
        hooks.set_origin(entity, target_origin);
        mover.summary.any_arrival_snap_applied = true;
        mover.summary.last_arrival_origin_text = FormatPathMoverVector(entity->v.origin);
    }

    result.summary =
        "frame=" + std::to_string(frame.frame_number)
        + " mover="
        + (mover.summary.targetname.empty() ? std::string("<empty>") : mover.summary.targetname)
        + " current="
        + (current_node != nullptr && !current_node->targetname.empty()
            ? current_node->targetname
            : std::string("<none>"))
        + " next=" + (next_node->targetname.empty() ? std::string("<empty>") : next_node->targetname)
        + " moverOrigin=" + FormatPathMoverVector(mover_origin)
        + " nextOrigin=" + FormatPathMoverVector(target_origin)
        + " distance=" + std::to_string(result.distance)
        + " epsilon=" + std::to_string(config_.arrival_epsilon)
        + " decision=" + result.decision
        + " trigger=" + result.trigger
        + " snap=" + (result.snap_applied ? "yes" : "no");

    if (hooks.log_info && (config_.trace_movement || result.arrived))
    {
        hooks.log_info("PathArrivalCalibrator: " + result.summary);
    }

    return result;
}
} // namespace hl::game_api::detail
