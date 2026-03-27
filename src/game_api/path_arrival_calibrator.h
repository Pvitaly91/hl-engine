#pragma once

#include <functional>
#include <string>
#include <string_view>

#include "path_mover_runtime.h"
#include "scripted_movement_controller.h"
#include "track_path_resolver.h"

namespace hl::game_api::detail
{
struct PathArrivalCalibratorConfig
{
    float arrival_epsilon = 24.0f;
    bool snap_to_node_on_arrival = true;
    bool trace_movement = false;
};

struct PathArrivalCalibratorHooks
{
    std::function<void(edict_t*, const Vector&)> set_origin;
    std::function<void(std::string_view)> log_info;
    std::function<void(std::string_view)> log_warn;
};

struct PathArrivalCalibrationResult
{
    bool evaluated = false;
    bool arrived = false;
    bool exact_hit = false;
    bool within_epsilon = false;
    bool snap_applied = false;
    float distance = 0.0f;
    float epsilon = 0.0f;
    std::string decision;
    std::string trigger;
    std::string summary;
};

class PathArrivalCalibrator
{
public:
    void Configure(const PathArrivalCalibratorConfig& config);
    const PathArrivalCalibratorConfig& Config() const noexcept;

    PathArrivalCalibrationResult Evaluate(
        PathMoverMutableState& mover,
        edict_t* entity,
        const TrackPathNodeView* current_node,
        const TrackPathNodeView* next_node,
        const ScriptedMovementFrameContext& frame,
        const PathArrivalCalibratorHooks& hooks) const;

private:
    PathArrivalCalibratorConfig config_{};
};
} // namespace hl::game_api::detail
