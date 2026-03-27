#pragma once

#include <functional>
#include <string_view>

#include "path_arrival_calibrator.h"
#include "path_node_event_dispatcher.h"
#include "path_traversal_controller.h"

namespace hl::game_api::detail
{
struct PathAdvanceControllerConfig
{
    float default_path_speed = 100.0f;
    bool trace_movement = false;
};

struct PathAdvanceControllerHooks
{
    std::function<void(edict_t*, const Vector&)> set_velocity;
    std::function<void(edict_t*, const Vector&)> set_avelocity;
    std::function<void(PathMoverMutableState&, PathMoverStage, std::string_view)> set_stage;
    std::function<void(std::string_view)> log_info;
    std::function<void(std::string_view)> log_warn;
    PathNodeEventDispatcher* event_dispatcher = nullptr;
    PathNodeEventDispatcherHooks event_hooks;
};

class PathAdvanceController
{
public:
    void Configure(const PathAdvanceControllerConfig& config);
    PathTraversalFrameResult HandlePostCalibration(
        PathMoverMutableState& mover,
        edict_t* entity,
        const TrackPathResolver& resolver,
        const TrackPathNodeView* current_node,
        const TrackPathNodeView* next_node,
        const ScriptedMovementFrameContext& frame,
        const PathArrivalCalibrationResult& arrival,
        const PathAdvanceControllerHooks& hooks) const;

private:
    static void StopMoverMotion(edict_t* entity, const PathAdvanceControllerHooks& hooks);

    PathAdvanceControllerConfig config_{};
};
} // namespace hl::game_api::detail
