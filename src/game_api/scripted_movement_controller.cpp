#include "scripted_movement_controller.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace
{
using hl::game_api::CanarySceneStateSummary;
using hl::game_api::NamedCountSummary;
using hl::game_api::ScriptedMovementFrameStateSummary;
using hl::game_api::ScriptedMovementStateSummary;
using hl::game_api::ScriptedSceneRuntimeSummary;
using hl::game_api::detail::ScriptedMovementControllerConfig;
using hl::game_api::detail::ScriptedMovementControllerHooks;
using hl::game_api::detail::ScriptedMovementEntityView;
using hl::game_api::detail::ScriptedMovementFrameContext;
using hl::game_api::detail::TrackPathResolver;

constexpr std::array<const char*, 4> kCanarySceneNames = {
    "walk1",
    "walk3",
    "room2_walk1",
    "ponderstart",
};

std::string ToLowerCopy(std::string value)
{
    std::transform(
        value.begin(),
        value.end(),
        value.begin(),
        [](unsigned char character)
        {
            return static_cast<char>(std::tolower(character));
        });
    return value;
}

bool EqualsIgnoreCase(std::string_view left, std::string_view right)
{
    if (left.size() != right.size())
    {
        return false;
    }

    for (std::size_t index = 0; index < left.size(); ++index)
    {
        if (std::tolower(static_cast<unsigned char>(left[index]))
            != std::tolower(static_cast<unsigned char>(right[index])))
        {
            return false;
        }
    }

    return true;
}

std::string FormatVector(const Vector& value)
{
    std::ostringstream stream;
    stream << value.x << " " << value.y << " " << value.z;
    return stream.str();
}

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

bool IsActorClassSafe(std::string_view classname)
{
    const std::string normalized = ToLowerCopy(std::string(classname));
    return normalized.rfind("monster_", 0) == 0
        || normalized == "monster_barney"
        || normalized == "monster_scientist"
        || normalized == "monster_sitting_scientist";
}

bool IsLifecycleReady(const ScriptedMovementEntityView& entity)
{
    return entity.in_use
        && !entity.removed
        && entity.has_private_data
        && (entity.spawned || entity.deferred)
        && (entity.flags & FL_KILLME) == 0;
}

bool IsSceneEngaged(const ScriptedMovementEntityView& scene)
{
    return scene.received_use_count > 0
        || scene.use_successes > 0
        || scene.internal_state_changed
        || scene.scheduled_follow_up
        || scene.emitted_targets
        || scene.pending_scheduled_outputs > 0
        || scene.scheduled_for_think
        || scene.progressed_this_run;
}

bool RequiresMoveToMark(int move_to) noexcept
{
    return move_to == 1 || move_to == 2 || move_to == 4;
}

const char* MoveModeLabel(int move_to) noexcept
{
    switch (move_to)
    {
    case 0:
        return "wait-current-location";
    case 1:
        return "walk-to-mark";
    case 2:
        return "run-to-mark";
    case 4:
        return "teleport-to-mark";
    default:
        return "unsupported";
    }
}

enum class SceneStage
{
    kUnresolved,
    kActorResolved,
    kMovePending,
    kMoving,
    kArrived,
    kAnimationReady,
    kCompleted,
    kDeferred,
};

const char* SceneStageLabel(SceneStage stage) noexcept
{
    switch (stage)
    {
    case SceneStage::kUnresolved:
        return "unresolved";
    case SceneStage::kActorResolved:
        return "actor-resolved";
    case SceneStage::kMovePending:
        return "move-pending";
    case SceneStage::kMoving:
        return "moving";
    case SceneStage::kArrived:
        return "arrived";
    case SceneStage::kAnimationReady:
        return "animation-ready";
    case SceneStage::kCompleted:
        return "completed";
    case SceneStage::kDeferred:
        return "deferred";
    default:
        return "unknown";
    }
}

int SceneStageRank(SceneStage stage) noexcept
{
    switch (stage)
    {
    case SceneStage::kUnresolved:
        return 0;
    case SceneStage::kActorResolved:
        return 1;
    case SceneStage::kMovePending:
        return 2;
    case SceneStage::kMoving:
        return 3;
    case SceneStage::kArrived:
        return 4;
    case SceneStage::kAnimationReady:
        return 5;
    case SceneStage::kCompleted:
        return 6;
    case SceneStage::kDeferred:
        return -1;
    default:
        return -1;
    }
}

bool IsSceneClass(std::string_view classname)
{
    return EqualsIgnoreCase(classname, "scripted_sequence");
}

} // namespace

namespace hl::game_api::detail
{
struct ScriptedMovementController::Impl
{
    struct SceneState
    {
        ScriptedSceneRuntimeSummary summary;
        SceneStage stage = SceneStage::kUnresolved;
        bool engaged = false;
        bool pending_animation_ready = false;
        bool ever_arrived = false;
        bool progressed_further_than_before = false;
    };

    ScriptedMovementControllerConfig config{};
    ScriptedMovementStateSummary summary{};
    ScriptedMovementFrameContext current_frame{};
    std::unordered_map<int, SceneState> scenes;

    static const ScriptedMovementEntityView* ResolveActor(
        const ScriptedMovementEntityView& scene,
        const std::unordered_map<std::string, std::vector<const ScriptedMovementEntityView*>>& by_targetname,
        const std::unordered_map<std::string, std::vector<const ScriptedMovementEntityView*>>& by_classname,
        bool* used_classname_fallback)
    {
        if (used_classname_fallback != nullptr)
        {
            *used_classname_fallback = false;
        }

        if (scene.actor_name.empty())
        {
            return nullptr;
        }

        const std::string key = ToLowerCopy(scene.actor_name);
        const auto by_name = by_targetname.find(key);
        if (by_name != by_targetname.end())
        {
            for (const ScriptedMovementEntityView* candidate : by_name->second)
            {
                if (candidate != nullptr && candidate->in_use && !candidate->removed)
                {
                    return candidate;
                }
            }
        }

        const auto by_class = by_classname.find(key);
        if (by_class != by_classname.end())
        {
            for (const ScriptedMovementEntityView* candidate : by_class->second)
            {
                if (candidate != nullptr && candidate->in_use && !candidate->removed)
                {
                    if (used_classname_fallback != nullptr)
                    {
                        *used_classname_fallback = true;
                    }
                    return candidate;
                }
            }
        }

        return nullptr;
    }

    void SetSceneStage(
        SceneState& state,
        SceneStage new_stage,
        const ScriptedMovementFrameContext& frame)
    {
        const bool changed = state.stage != new_stage;
        state.stage = new_stage;
        state.summary.stage = SceneStageLabel(new_stage);
        state.summary.stage_changed_this_frame = changed;
        if (changed)
        {
            state.summary.last_progress_frame = frame.frame_number;
            state.summary.last_progress_time = frame.time;
        }
        if (SceneStageRank(new_stage) >= SceneStageRank(SceneStage::kMoving))
        {
            state.progressed_further_than_before = true;
        }
    }
};

ScriptedMovementController::ScriptedMovementController()
    : impl_(std::make_unique<Impl>())
{
}

ScriptedMovementController::~ScriptedMovementController() = default;

void ScriptedMovementController::Configure(const ScriptedMovementControllerConfig& config)
{
    impl_->config = config;
    if (impl_->config.preview_limit == 0)
    {
        impl_->config.preview_limit = 8;
    }
    if (impl_->config.frame_history_limit == 0)
    {
        impl_->config.frame_history_limit = 64;
    }
    if (impl_->config.canary_limit == 0)
    {
        impl_->config.canary_limit = 6;
    }

    impl_->summary = {};
    impl_->summary.configured = true;
    impl_->summary.trace_movement = impl_->config.trace_movement;
    impl_->summary.bootstrap_mode =
        impl_->config.bootstrap_no_collision
        ? "direct-kinematic-no-collision bootstrap"
        : "direct-kinematic bootstrap";
    impl_->scenes.clear();
}

void ScriptedMovementController::BeginFrame(const ScriptedMovementFrameContext& context)
{
    impl_->current_frame = context;
    ++impl_->summary.frames_attempted;
    impl_->summary.attempted = true;

    for (auto& [edict_index, scene] : impl_->scenes)
    {
        (void)edict_index;
        scene.summary.stage_changed_this_frame = false;
        scene.summary.moving_this_frame = false;
        scene.summary.arrived_this_frame = false;
    }
}

void ScriptedMovementController::RunFrame(
    const ScriptedMovementFrameContext& context,
    const TrackPathResolver& path_resolver,
    const std::vector<ScriptedMovementEntityView>& entities,
    const ScriptedMovementControllerHooks& hooks)
{
    (void)path_resolver;
    std::unordered_map<std::string, std::vector<const ScriptedMovementEntityView*>> by_targetname;
    std::unordered_map<std::string, std::vector<const ScriptedMovementEntityView*>> by_classname;
    by_targetname.reserve(entities.size());
    by_classname.reserve(entities.size());

    for (const ScriptedMovementEntityView& entity : entities)
    {
        if (!entity.in_use || entity.removed)
        {
            continue;
        }

        if (!entity.targetname.empty())
        {
            by_targetname[ToLowerCopy(entity.targetname)].push_back(&entity);
        }
        if (!entity.classname.empty())
        {
            by_classname[ToLowerCopy(entity.classname)].push_back(&entity);
        }
    }

    ScriptedMovementFrameStateSummary frame_summary;
    frame_summary.frame_number = context.frame_number;
    frame_summary.host_frame_index = context.host_frame_index;
    frame_summary.server_frame_index = context.server_frame_index;
    frame_summary.time = context.time;
    frame_summary.frametime = context.frametime;
    frame_summary.delayed_actions_due = context.delayed_actions_due;
    frame_summary.delayed_actions_executed = context.delayed_actions_executed;
    frame_summary.delayed_actions_pending = context.delayed_actions_pending;

    for (const ScriptedMovementEntityView& scene_view : entities)
    {
        if (!IsSceneClass(scene_view.classname))
        {
            continue;
        }

        Impl::SceneState& state = impl_->scenes[scene_view.edict_index];
        state.summary.edict_index = scene_view.edict_index;
        state.summary.parse_index = scene_view.parse_index;
        state.summary.targetname = scene_view.targetname;
        state.summary.classname = scene_view.classname;
        state.summary.entity_name = scene_view.actor_name;
        state.summary.play = scene_view.play;
        state.summary.idle = scene_view.idle;
        state.summary.move_to = scene_view.move_to;
        state.summary.radius = scene_view.radius;
        state.summary.delay = scene_view.delay;
        state.summary.origin_text =
            !scene_view.origin_text.empty() ? scene_view.origin_text : FormatVector(scene_view.origin);
        state.summary.angles_text =
            !scene_view.angles_text.empty() ? scene_view.angles_text : FormatVector(scene_view.angles);
        state.summary.mark_origin_text = state.summary.origin_text;
        state.summary.mark_yaw = scene_view.has_angles ? scene_view.angles.y : 0.0f;
        state.summary.arrival_epsilon = impl_->config.arrival_epsilon;
        state.summary.movement_mode = MoveModeLabel(scene_view.move_to);
        state.summary.requires_move_to_mark = RequiresMoveToMark(scene_view.move_to);
        state.summary.bootstrap_no_collision = impl_->config.bootstrap_no_collision;
        state.summary.actor_resolved = false;
        state.summary.actor_edict_index = -1;
        state.summary.actor_classname.clear();
        state.summary.blocked_reason.clear();
        state.summary.support_state.clear();
        state.engaged = IsSceneEngaged(scene_view);

        bool used_classname_fallback = false;
        const ScriptedMovementEntityView* actor = Impl::ResolveActor(
            scene_view,
            by_targetname,
            by_classname,
            &used_classname_fallback);

        if (actor == nullptr)
        {
            state.summary.support_state = "actor-resolution-pending";
            state.summary.blocked_reason =
                "actor '" + scene_view.actor_name + "' not found";
            impl_->SetSceneStage(state, SceneStage::kUnresolved, context);
            ++frame_summary.blocked_scenes;
            continue;
        }

        if (!IsActorClassSafe(actor->classname))
        {
            state.summary.support_state = "actor-invalid";
            state.summary.blocked_reason =
                "actor wrong classname/type: " + actor->classname;
            impl_->SetSceneStage(state, SceneStage::kDeferred, context);
            ++frame_summary.blocked_scenes;
            continue;
        }

        state.summary.actor_resolved = true;
        state.summary.actor_edict_index = actor->edict_index;
        state.summary.actor_classname = actor->classname;

        const bool uses_deferred_actor_bootstrap = actor->deferred && !actor->spawned;

        if (!IsLifecycleReady(*actor))
        {
            state.summary.support_state = uses_deferred_actor_bootstrap
                ? "actor-bootstrap-runtime-missing"
                : "actor-not-ready";
            state.summary.blocked_reason = "actor not yet lifecycle-ready";
            impl_->SetSceneStage(state, SceneStage::kDeferred, context);
            ++frame_summary.blocked_scenes;
            continue;
        }

        std::string support_state =
            scene_view.move_to == 4
            ? "bootstrap-teleport"
            : scene_view.move_to == 1 || scene_view.move_to == 2
            ? "bootstrap-direct-kinematic"
            : "bootstrap-wait";
        if (uses_deferred_actor_bootstrap)
        {
            support_state += "-deferred-actor";
        }
        state.summary.support_state = std::move(support_state);

        if (used_classname_fallback && hooks.log_info)
        {
            hooks.log_info(
                "ScriptedMovementController: actor resolution for "
                + (scene_view.targetname.empty() ? std::string("<empty>") : scene_view.targetname)
                + " used classname fallback '" + scene_view.actor_name + "'.");
        }

        if (!state.engaged)
        {
            impl_->SetSceneStage(state, SceneStage::kActorResolved, context);
            continue;
        }

        if (scene_view.move_to == 0)
        {
            impl_->SetSceneStage(state, SceneStage::kAnimationReady, context);
            if (scene_view.emitted_targets)
            {
                impl_->SetSceneStage(state, SceneStage::kCompleted, context);
            }
            continue;
        }

        if (scene_view.move_to == 4)
        {
            edict_t* actor_entity =
                hooks.entity_by_index ? hooks.entity_by_index(actor->edict_index) : nullptr;
            if (actor_entity == nullptr || hooks.set_origin == nullptr || hooks.set_angles == nullptr)
            {
                state.summary.blocked_reason = "actor exists but movement support missing";
                impl_->SetSceneStage(state, SceneStage::kDeferred, context);
                ++frame_summary.blocked_scenes;
                continue;
            }

            if (state.pending_animation_ready)
            {
                state.pending_animation_ready = false;
                impl_->SetSceneStage(state, SceneStage::kAnimationReady, context);
                continue;
            }

            hooks.set_origin(actor_entity, scene_view.origin);
            Vector angles = actor_entity->v.angles;
            angles.y = state.summary.mark_yaw;
            hooks.set_angles(actor_entity, angles);
            if (hooks.set_velocity)
            {
                hooks.set_velocity(actor_entity, Vector(0.0f, 0.0f, 0.0f));
            }
            if (hooks.set_avelocity)
            {
                hooks.set_avelocity(actor_entity, Vector(0.0f, 0.0f, 0.0f));
            }

            state.summary.arrived_this_frame = true;
            state.pending_animation_ready = true;
            state.ever_arrived = true;
            ++frame_summary.scene_arrivals;
            impl_->SetSceneStage(state, SceneStage::kArrived, context);
            continue;
        }

        if (scene_view.move_to == 1 || scene_view.move_to == 2)
        {
            edict_t* actor_entity =
                hooks.entity_by_index ? hooks.entity_by_index(actor->edict_index) : nullptr;
            if (actor_entity == nullptr || hooks.set_origin == nullptr)
            {
                state.summary.blocked_reason = "actor exists but movement support missing";
                impl_->SetSceneStage(state, SceneStage::kDeferred, context);
                ++frame_summary.blocked_scenes;
                continue;
            }

            if (state.pending_animation_ready)
            {
                state.pending_animation_ready = false;
                impl_->SetSceneStage(state, SceneStage::kAnimationReady, context);
                continue;
            }

            const Vector current_origin = actor_entity->v.origin;
            const Vector delta = scene_view.origin - current_origin;
            const float distance = delta.Length();
            if (distance <= impl_->config.arrival_epsilon)
            {
                state.summary.arrived_this_frame = true;
                state.pending_animation_ready = true;
                state.ever_arrived = true;
                ++frame_summary.scene_arrivals;
                impl_->SetSceneStage(state, SceneStage::kArrived, context);
                continue;
            }

            const float speed =
                scene_view.move_to == 2 ? impl_->config.run_speed : impl_->config.walk_speed;
            const float step = std::min(speed * context.frametime, distance);
            const float desired_yaw = VecToYaw(delta);
            actor_entity->v.ideal_yaw = desired_yaw;
            if (actor_entity->v.yaw_speed <= 0.0f)
            {
                actor_entity->v.yaw_speed = scene_view.move_to == 2 ? 120.0f : 90.0f;
            }
            if (hooks.change_yaw)
            {
                hooks.change_yaw(actor_entity);
            }
            else if (hooks.set_angles)
            {
                Vector actor_angles = actor_entity->v.angles;
                actor_angles.y = desired_yaw;
                hooks.set_angles(actor_entity, actor_angles);
            }

            Vector new_origin = current_origin;
            if (hooks.walk_move != nullptr)
            {
                const float horizontal_distance =
                    std::sqrt((delta.x * delta.x) + (delta.y * delta.y));
                const float step_2d = std::min(step, horizontal_distance);
                if (step_2d > 0.0f)
                {
                    hooks.walk_move(actor_entity, desired_yaw, step_2d, 0);
                    new_origin = actor_entity->v.origin;
                }
            }
            else
            {
                new_origin = current_origin + (delta * (step / distance));
            }

            new_origin.z = current_origin.z + (delta.z * (step / distance));
            if (step >= distance)
            {
                new_origin = scene_view.origin;
            }

            hooks.set_origin(actor_entity, new_origin);
            if (hooks.set_velocity)
            {
                hooks.set_velocity(
                    actor_entity,
                    step >= distance ? Vector(0.0f, 0.0f, 0.0f) : delta * (speed / distance));
            }
            if (hooks.set_avelocity)
            {
                hooks.set_avelocity(actor_entity, Vector(0.0f, 0.0f, 0.0f));
            }

            state.summary.moving_this_frame = true;
            ++frame_summary.scenes_moving;
            impl_->SetSceneStage(state, SceneStage::kMoving, context);
            continue;
        }

        state.summary.support_state = "unsupported-move-mode";
        state.summary.blocked_reason =
            "m_fMoveTo=" + std::to_string(scene_view.move_to)
            + " unsupported by bootstrap movement layer";
        impl_->SetSceneStage(state, SceneStage::kDeferred, context);
        ++frame_summary.blocked_scenes;
    }

    std::unordered_map<std::string, std::size_t> blocked_reason_counts;
    std::unordered_map<std::string, std::size_t> stage_counts;
    impl_->summary.actor_resolutions_attempted = 0;
    impl_->summary.actor_resolutions_succeeded = 0;
    impl_->summary.actor_resolutions_failed = 0;
    impl_->summary.scene_movement_attempts = 0;
    impl_->summary.scene_movement_successes = 0;
    impl_->summary.scene_movement_blocked = 0;
    impl_->summary.scenes_preview.clear();
    impl_->summary.canaries.clear();

    std::vector<const Impl::SceneState*> ordered_scenes;
    ordered_scenes.reserve(impl_->scenes.size());
    for (const auto& [edict_index, scene] : impl_->scenes)
    {
        (void)edict_index;
        ordered_scenes.push_back(&scene);
        if (!scene.summary.entity_name.empty())
        {
            ++impl_->summary.actor_resolutions_attempted;
            if (scene.summary.actor_resolved)
            {
                ++impl_->summary.actor_resolutions_succeeded;
            }
            else
            {
                ++impl_->summary.actor_resolutions_failed;
            }
        }
        if (scene.summary.requires_move_to_mark && scene.engaged)
        {
            ++impl_->summary.scene_movement_attempts;
            if (scene.ever_arrived
                || scene.stage == SceneStage::kAnimationReady
                || scene.stage == SceneStage::kCompleted)
            {
                ++impl_->summary.scene_movement_successes;
            }
            else if (!scene.summary.blocked_reason.empty())
            {
                ++impl_->summary.scene_movement_blocked;
            }
        }
        ++stage_counts[scene.summary.stage];
        if (!scene.summary.blocked_reason.empty())
        {
            ++blocked_reason_counts[scene.summary.blocked_reason];
        }
    }

    std::sort(
        ordered_scenes.begin(),
        ordered_scenes.end(),
        [](const Impl::SceneState* left, const Impl::SceneState* right)
        {
            if (left->progressed_further_than_before != right->progressed_further_than_before)
            {
                return left->progressed_further_than_before > right->progressed_further_than_before;
            }
            if (SceneStageRank(left->stage) != SceneStageRank(right->stage))
            {
                return SceneStageRank(left->stage) > SceneStageRank(right->stage);
            }
            return left->summary.targetname < right->summary.targetname;
        });
    for (const Impl::SceneState* scene : ordered_scenes)
    {
        if (impl_->summary.scenes_preview.size() >= impl_->config.preview_limit)
        {
            break;
        }
        impl_->summary.scenes_preview.push_back(scene->summary);
    }

    impl_->summary.movement_blocked_reasons.clear();
    impl_->summary.scene_stage_summary.clear();
    frame_summary.blocked_by_reason.clear();
    for (const auto& [name, count] : blocked_reason_counts)
    {
        impl_->summary.movement_blocked_reasons.push_back({name, count});
        frame_summary.blocked_by_reason.push_back({name, count});
    }
    for (const auto& [name, count] : stage_counts)
    {
        impl_->summary.scene_stage_summary.push_back({name, count});
    }
    std::sort(
        impl_->summary.movement_blocked_reasons.begin(),
        impl_->summary.movement_blocked_reasons.end(),
        [](const NamedCountSummary& left, const NamedCountSummary& right)
        {
            if (left.count != right.count)
            {
                return left.count > right.count;
            }
            return left.name < right.name;
        });
    std::sort(
        impl_->summary.scene_stage_summary.begin(),
        impl_->summary.scene_stage_summary.end(),
        [](const NamedCountSummary& left, const NamedCountSummary& right)
        {
            if (left.count != right.count)
            {
                return left.count > right.count;
            }
            return left.name < right.name;
        });
    std::sort(
        frame_summary.blocked_by_reason.begin(),
        frame_summary.blocked_by_reason.end(),
        [](const NamedCountSummary& left, const NamedCountSummary& right)
        {
            if (left.count != right.count)
            {
                return left.count > right.count;
            }
            return left.name < right.name;
        });

    frame_summary.canary_status.clear();
    for (const char* canary_name : kCanarySceneNames)
    {
        CanarySceneStateSummary canary;
        canary.canary_name = canary_name;
        for (const auto& [edict_index, scene] : impl_->scenes)
        {
            (void)edict_index;
            if (!EqualsIgnoreCase(scene.summary.targetname, canary_name))
            {
                continue;
            }
            canary.actor_name = scene.summary.entity_name;
            canary.stage = scene.summary.stage;
            canary.progressed_further_than_before = scene.progressed_further_than_before;
            canary.status =
                scene.summary.stage + " actor="
                + (scene.summary.entity_name.empty() ? std::string("<none>") : scene.summary.entity_name)
                + " blocked="
                + (scene.summary.blocked_reason.empty()
                    ? std::string("<none>")
                    : scene.summary.blocked_reason);
            break;
        }
        if (canary.stage.empty())
        {
            canary.stage = "unresolved";
            canary.status = "scene not resolved";
        }
        impl_->summary.canaries.push_back(canary);
        frame_summary.canary_status.push_back(std::string(canary_name) + ": " + canary.status);
    }

    impl_->summary.readiness =
        impl_->summary.scene_movement_successes > 0
        ? "ready for movement-driven scripted scene completion; path mover runtime can layer on next"
        : !impl_->summary.movement_blocked_reasons.empty()
        ? "movement bootstrap is diagnosable; resolve the top blocked reason next"
        : "movement bootstrap is configured; longer deterministic runs can probe delayed movers";

    impl_->summary.frames.push_back(frame_summary);
    while (impl_->summary.frames.size() > impl_->config.frame_history_limit)
    {
        impl_->summary.frames.erase(impl_->summary.frames.begin());
    }
    ++impl_->summary.frames_completed;
    impl_->summary.controller_ran = true;

    if (hooks.log_info && impl_->config.trace_movement)
    {
        hooks.log_info(
            "SceneMovementTrace: frame " + std::to_string(frame_summary.frame_number)
            + " scenesMoving=" + std::to_string(frame_summary.scenes_moving)
            + " arrivals=" + std::to_string(frame_summary.scene_arrivals)
            + " blocked=" + std::to_string(frame_summary.blocked_scenes)
            + " delayed due/executed/pending="
            + std::to_string(frame_summary.delayed_actions_due) + "/"
            + std::to_string(frame_summary.delayed_actions_executed) + "/"
            + std::to_string(frame_summary.delayed_actions_pending));
        for (const std::string& line : frame_summary.canary_status)
        {
            hooks.log_info("  - " + line);
        }
    }
}

const ScriptedMovementStateSummary& ScriptedMovementController::Summary() const noexcept
{
    return impl_->summary;
}

const ScriptedSceneRuntimeSummary* ScriptedMovementController::FindSceneState(int edict_index) const noexcept
{
    const auto it = impl_->scenes.find(edict_index);
    return it != impl_->scenes.end() ? &it->second.summary : nullptr;
}

bool ScriptedMovementController::DidSceneMoveThisFrame(int edict_index) const noexcept
{
    const auto it = impl_->scenes.find(edict_index);
    return it != impl_->scenes.end() && it->second.summary.moving_this_frame;
}

bool ScriptedMovementController::DidSceneArriveThisFrame(int edict_index) const noexcept
{
    const auto it = impl_->scenes.find(edict_index);
    return it != impl_->scenes.end() && it->second.summary.arrived_this_frame;
}

bool ScriptedMovementController::DidSceneStageChangeThisFrame(int edict_index) const noexcept
{
    const auto it = impl_->scenes.find(edict_index);
    return it != impl_->scenes.end() && it->second.summary.stage_changed_this_frame;
}
} // namespace hl::game_api::detail
