#pragma once

#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include "path_mover_runtime.h"
#include "path_node_event_dispatcher.h"
#include "scripted_movement_controller.h"
#include "track_path_resolver.h"

namespace hl::game_api::detail
{
struct PathTraversalControllerConfig
{
    float default_path_speed = 100.0f;
    float arrival_epsilon = 16.0f;
    bool snap_to_node_on_arrival = true;
    bool trace_movement = false;
};

struct PathTraversalControllerHooks
{
    std::function<void(edict_t*, const Vector&)> set_origin;
    std::function<void(edict_t*, const Vector&)> set_velocity;
    std::function<void(edict_t*, const Vector&)> set_avelocity;
    std::function<void(PathMoverMutableState&, PathMoverStage, std::string_view)> set_stage;
    std::function<void(std::string_view)> log_info;
    std::function<void(std::string_view)> log_warn;
    PathNodeEventDispatcher* event_dispatcher = nullptr;
    PathNodeEventDispatcherHooks event_hooks;
};

struct PathTraversalFrameResult
{
    bool arrived = false;
    bool advanced = false;
    bool stopped = false;
    bool completed = false;
    bool blocked = false;
    std::string message_summary;
    std::string dispatch_summary;
    std::string trace_summary;
    std::string broken_link_summary;
};

class PathTraversalController
{
public:
    void Configure(const PathTraversalControllerConfig& config);
    float ResolveSegmentSpeed(
        PathMoverMutableState& mover,
        const TrackPathNodeView* current_node,
        const TrackPathNodeView* next_node,
        const PathTraversalControllerHooks& hooks);

private:
    PathTraversalControllerConfig config_{};
};
} // namespace hl::game_api::detail
