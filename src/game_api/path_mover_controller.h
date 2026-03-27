#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "game_api/hl_server_module.h"
#include "path_arrival_calibrator.h"
#include "path_advance_controller.h"
#include "path_node_event_dispatcher.h"
#include "scripted_movement_controller.h"
#include "track_path_resolver.h"

#pragma warning(push, 0)
#include "extdll.h"
#pragma warning(pop)

namespace hl::game_api::detail
{
struct PathMoverControllerConfig
{
    float default_path_speed = 100.0f;
    float arrival_epsilon = 24.0f;
    float start_bind_radius = 2048.0f;
    bool trace_movement = false;
    std::size_t preview_limit = 8;
    std::size_t frame_history_limit = 128;
    std::size_t similar_name_limit = 4;
};

struct PathMoverControllerHooks
{
    std::function<edict_t*(int)> entity_by_index;
    std::function<void(edict_t*, const Vector&)> set_origin;
    std::function<void(edict_t*, const Vector&)> set_angles;
    std::function<void(edict_t*, const Vector&)> set_velocity;
    std::function<void(edict_t*, const Vector&)> set_avelocity;
    std::function<void(edict_t*)> change_yaw;
    std::function<PathNodeEventDispatchFeedback(const PathNodeEventDispatchRequest&)> dispatch_target_event;
    std::function<void(std::string_view)> log_info;
    std::function<void(std::string_view)> log_warn;
};

struct PathMoverFrameState
{
    int frame_number = 0;
    std::uint64_t host_frame_index = 0;
    std::uint64_t server_frame_index = 0;
    float time = 0.0f;
    float frametime = 0.0f;
    int active_movers = 0;
    int moving_movers = 0;
    int blocked_movers = 0;
    int stopped_movers = 0;
    int node_arrivals = 0;
    int path_advances = 0;
    std::string ftruck_status;
    std::vector<std::string> active_movers_preview;
    std::vector<std::string> messages_this_frame;
    std::vector<std::string> events;
};

struct PathMoverControllerStateSummary
{
    bool configured = false;
    bool controller_ran = false;
    int frames_attempted = 0;
    int frames_completed = 0;
    TrackPathGraphSummary path_graph;
    PathMoverAggregateSummary path_movers;
    float effective_arrival_epsilon = 0.0f;
    bool snap_to_node_occurred = false;
    bool flatbedstart_resolved = false;
    std::string delayed_ftruck_status;
    std::string ftruck_final_current;
    std::string ftruck_final_next;
    bool ftruck_advanced_beyond_trainstop1a = false;
    PathNodeMessageStateSummary path_node_messages;
    std::vector<std::string> messages_encountered;
    std::size_t message_dispatch_count = 0;
    bool message_dispatch_triggered = false;
    std::vector<std::string> message_dispatches;
    std::vector<std::string> broken_link_events;
    std::vector<PathMoverRuntimeSummary> movers_preview;
    std::vector<PathMoverFrameState> frames;
    std::string readiness;
};

class PathMoverController
{
public:
    PathMoverController();
    ~PathMoverController();

    PathMoverController(const PathMoverController&) = delete;
    PathMoverController& operator=(const PathMoverController&) = delete;

    void Configure(const PathMoverControllerConfig& config);
    void BeginFrame(const ScriptedMovementFrameContext& context);
    void RequestActivation(
        int edict_index,
        std::string targetname,
        float request_time,
        std::string reason,
        std::string use_source);
    void RunFrame(
        const ScriptedMovementFrameContext& context,
        const TrackPathResolver& path_resolver,
        const std::vector<ScriptedMovementEntityView>& entities,
        const PathMoverControllerHooks& hooks);

    const PathMoverControllerStateSummary& Summary() const noexcept;
    const PathMoverRuntimeSummary* FindMoverState(int edict_index) const noexcept;
    const PathMoverRuntimeSummary* FindMoverByTargetname(std::string_view targetname) const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace hl::game_api::detail
