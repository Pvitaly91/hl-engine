#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "game_api/hl_server_module.h"
#include "track_path_resolver.h"

#pragma warning(push, 0)
#include "extdll.h"
#pragma warning(pop)

namespace hl::game_api::detail
{
struct ScriptedMovementEntityView
{
    int edict_index = -1;
    std::size_t parse_index = 0;
    std::string classname;
    std::string targetname;
    std::string target;
    std::string message;
    std::string actor_name;
    std::string play;
    std::string idle;
    int move_to = 0;
    float radius = 0.0f;
    float delay = 0.0f;
    float speed = 0.0f;
    float start_speed = 0.0f;
    int spawnflags = 0;
    Vector origin = Vector(0.0f, 0.0f, 0.0f);
    bool has_origin = false;
    Vector angles = Vector(0.0f, 0.0f, 0.0f);
    bool has_angles = false;
    std::string origin_text;
    std::string angles_text;
    std::string model;
    int modelindex = 0;
    bool in_use = false;
    bool removed = false;
    bool spawned = false;
    bool deferred = false;
    bool has_private_data = false;
    bool scheduled_for_think = false;
    int flags = 0;
    int solid = 0;
    int movetype = 0;
    int effects = 0;
    int received_use_count = 0;
    int use_successes = 0;
    int pending_scheduled_outputs = 0;
    int last_trigger_frame = -1;
    float last_trigger_time = 0.0f;
    std::string last_source_entity;
    bool internal_state_changed = false;
    bool scheduled_follow_up = false;
    bool emitted_targets = false;
    bool progressed_this_run = false;
};

struct ScriptedMovementFrameContext
{
    int frame_number = 0;
    std::uint64_t host_frame_index = 0;
    std::uint64_t server_frame_index = 0;
    float time = 0.0f;
    float frametime = 0.0f;
    int delayed_actions_due = 0;
    int delayed_actions_executed = 0;
    int delayed_actions_pending = 0;
};

struct ScriptedMovementControllerConfig
{
    float walk_speed = 64.0f;
    float run_speed = 160.0f;
    float default_path_speed = 100.0f;
    float arrival_epsilon = 12.0f;
    float nearest_path_radius = 1024.0f;
    bool trace_movement = false;
    bool bootstrap_no_collision = true;
    std::size_t preview_limit = 8;
    std::size_t frame_history_limit = 64;
    std::size_t canary_limit = 6;
};

struct ScriptedMovementControllerHooks
{
    std::function<edict_t*(int)> entity_by_index;
    std::function<void(edict_t*, const Vector&)> set_origin;
    std::function<void(edict_t*, const Vector&)> set_angles;
    std::function<void(edict_t*, const Vector&)> set_velocity;
    std::function<void(edict_t*, const Vector&)> set_avelocity;
    std::function<int(edict_t*, float, float, int)> walk_move;
    std::function<void(edict_t*)> change_yaw;
    std::function<void(edict_t*)> change_pitch;
    std::function<void(std::string_view)> log_info;
    std::function<void(std::string_view)> log_warn;
};

class ScriptedMovementController
{
public:
    ScriptedMovementController();
    ~ScriptedMovementController();

    ScriptedMovementController(const ScriptedMovementController&) = delete;
    ScriptedMovementController& operator=(const ScriptedMovementController&) = delete;

    void Configure(const ScriptedMovementControllerConfig& config);
    void BeginFrame(const ScriptedMovementFrameContext& context);
    void RunFrame(
        const ScriptedMovementFrameContext& context,
        const TrackPathResolver& path_resolver,
        const std::vector<ScriptedMovementEntityView>& entities,
        const ScriptedMovementControllerHooks& hooks);

    const ScriptedMovementStateSummary& Summary() const noexcept;
    const ScriptedSceneRuntimeSummary* FindSceneState(int edict_index) const noexcept;
    bool DidSceneMoveThisFrame(int edict_index) const noexcept;
    bool DidSceneArriveThisFrame(int edict_index) const noexcept;
    bool DidSceneStageChangeThisFrame(int edict_index) const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace hl::game_api::detail
