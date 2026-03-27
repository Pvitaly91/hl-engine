#include "path_traversal_controller.h"

#include <algorithm>

namespace hl::game_api::detail
{
void PathTraversalController::Configure(const PathTraversalControllerConfig& config)
{
    config_ = config;
    if (config_.default_path_speed <= 0.0f)
    {
        config_.default_path_speed = 100.0f;
    }
    if (config_.arrival_epsilon <= 0.0f)
    {
        config_.arrival_epsilon = 16.0f;
    }
}

float PathTraversalController::ResolveSegmentSpeed(
    PathMoverMutableState& mover,
    const TrackPathNodeView* current_node,
    const TrackPathNodeView* next_node,
    const PathTraversalControllerHooks& hooks)
{
    const float inherited_speed =
        mover.summary.speed > 0.0f
        ? mover.summary.speed
        : (mover.summary.base_speed > 0.0f ? mover.summary.base_speed : config_.default_path_speed);

    float selected_speed = inherited_speed;
    std::string policy = "inherited-mover-speed";
    std::string detail =
        "base=" + std::to_string(mover.summary.base_speed)
        + " inherited=" + std::to_string(inherited_speed)
        + " current="
        + (current_node != nullptr && !current_node->targetname.empty()
            ? current_node->targetname
            : std::string("<none>"))
        + " next="
        + (next_node != nullptr && !next_node->targetname.empty()
            ? next_node->targetname
            : std::string("<none>"));

    if (current_node != nullptr && current_node->speed > 0.0f)
    {
        selected_speed = current_node->speed;
        policy = "current-node-speed-override";
        detail += " override=" + std::to_string(current_node->speed)
            + " stagedPolicy=arrival-node-speed-applies-to-next-segment";
    }
    else if (selected_speed <= 0.0f)
    {
        selected_speed = config_.default_path_speed;
        policy = "default-path-speed";
        detail += " fallbackDefault=" + std::to_string(config_.default_path_speed);
    }

    mover.summary.speed = selected_speed;
    mover.summary.effective_speed = selected_speed;
    if (policy != mover.summary.speed_policy
        || detail != mover.summary.speed_policy_detail
        || config_.trace_movement)
    {
        mover.summary.speed_policy = policy;
        mover.summary.speed_policy_detail = detail;
        if (hooks.log_info)
        {
            hooks.log_info(
                "PathTraversalController: speed mover="
                + (mover.summary.targetname.empty()
                    ? std::string("<empty>")
                    : mover.summary.targetname)
                + " selected=" + std::to_string(selected_speed)
                + " policy=" + policy
                + " detail=" + detail);
        }
    }

    return selected_speed;
}
} // namespace hl::game_api::detail
