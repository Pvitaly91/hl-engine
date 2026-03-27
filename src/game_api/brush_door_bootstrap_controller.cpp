#include "brush_door_bootstrap_controller.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <sstream>
#include <unordered_map>
#include <utility>

namespace
{
using hl::game_api::BrushDoorBootstrapStateSummary;
using hl::game_api::BrushDoorRuntimeSummary;
using hl::game_api::detail::BrushDoorBootstrapConfig;
using hl::game_api::detail::BrushDoorBootstrapController;
using hl::game_api::detail::BrushDoorBootstrapHooks;
using hl::game_api::detail::BrushDoorDispatchResult;
using hl::game_api::detail::BrushDoorEntityView;
using hl::game_api::detail::BrushDoorUseRequest;
using hl::game_api::detail::ScriptedMovementFrameContext;

bool EqualsIgnoreCase(std::string_view left, std::string_view right)
{
    if (left.size() != right.size())
    {
        return false;
    }

    return std::equal(
        left.begin(),
        left.end(),
        right.begin(),
        [](char lhs, char rhs)
        {
            return std::tolower(static_cast<unsigned char>(lhs))
                == std::tolower(static_cast<unsigned char>(rhs));
        });
}

void AppendLimited(std::vector<std::string>& lines, std::string line, std::size_t limit)
{
    lines.push_back(std::move(line));
    while (lines.size() > limit)
    {
        lines.erase(lines.begin());
    }
}

void AppendUnique(std::vector<std::string>& values, std::string value)
{
    if (value.empty())
    {
        return;
    }

    const auto it = std::find_if(
        values.begin(),
        values.end(),
        [&](std::string_view existing)
        {
            return EqualsIgnoreCase(existing, value);
        });
    if (it == values.end())
    {
        values.push_back(std::move(value));
    }
}

std::string FormatVector(const Vector& value)
{
    std::ostringstream stream;
    stream << value.x << ' ' << value.y << ' ' << value.z;
    return stream.str();
}

float NormalizeVector(Vector* value)
{
    if (value == nullptr)
    {
        return 0.0f;
    }

    const float length = std::sqrt(
        (value->x * value->x)
        + (value->y * value->y)
        + (value->z * value->z));
    if (length <= 0.0001f)
    {
        return 0.0f;
    }

    value->x /= length;
    value->y /= length;
    value->z /= length;
    return length;
}

std::string BoolLabel(bool value)
{
    return value ? "yes" : "no";
}

std::string SafeText(std::string_view value, std::string_view fallback = "<none>")
{
    return value.empty() ? std::string(fallback) : std::string(value);
}

std::string BuildDoorLabel(const BrushDoorRuntimeSummary& summary)
{
    if (!summary.targetname.empty())
    {
        return summary.targetname;
    }

    return "edict#" + std::to_string(summary.edict_index);
}
} // namespace

namespace hl::game_api::detail
{
struct BrushDoorBootstrapController::Impl
{
    struct MutableDoorState
    {
        BrushDoorRuntimeSummary summary;
        Vector closed_origin = Vector(0.0f, 0.0f, 0.0f);
        Vector open_origin = Vector(0.0f, 0.0f, 0.0f);
        Vector movedir = Vector(0.0f, 0.0f, 0.0f);
        float travel_distance = 0.0f;
        bool motion_ready = false;
        int last_motion_frame = -1;
    };

    BrushDoorBootstrapConfig config{};
    ScriptedMovementFrameContext current_frame{};
    BrushDoorBootstrapStateSummary summary{};
    std::unordered_map<int, MutableDoorState> doors;

    MutableDoorState& EnsureDoor(const BrushDoorEntityView& view)
    {
        MutableDoorState& door = doors[view.edict_index];
        BrushDoorRuntimeSummary& runtime = door.summary;
        runtime.edict_index = view.edict_index;
        runtime.parse_index = view.parse_index;
        runtime.classname = view.classname;
        runtime.targetname = view.targetname;
        runtime.model = view.model;
        runtime.modelindex = view.modelindex;
        runtime.origin_text = view.has_origin ? FormatVector(view.origin) : std::string("<none>");
        runtime.angles_text = view.has_angles ? FormatVector(view.angles) : std::string("<none>");
        runtime.movedir_text = view.has_movedir ? FormatVector(view.movedir) : std::string("<none>");
        runtime.speed = view.speed;
        runtime.lip = view.lip;
        runtime.wait = view.wait;
        runtime.spawnflags = view.spawnflags;
        runtime.health = view.health;
        runtime.health_available = view.health_available;
        runtime.damage = view.damage;
        runtime.damage_available = view.damage_available;
        runtime.lifecycle = view.lifecycle;
        runtime.resolved_runtime_target = view.edict_index >= 0;
        runtime.active = view.in_use && !view.removed;
        if (runtime.support_state.empty())
        {
            runtime.support_state = "passive-recipient-only";
        }
        if (runtime.movement_state.empty())
        {
            runtime.movement_state = "closed";
        }
        if (!runtime.movement_started && view.has_origin)
        {
            door.closed_origin = view.origin;
        }
        return door;
    }

    void LogTransition(
        MutableDoorState& door,
        const ScriptedMovementFrameContext& frame,
        std::string message,
        const BrushDoorBootstrapHooks& hooks,
        bool warn = false)
    {
        AppendLimited(summary.transition_history, std::move(message), config.history_limit);
        door.summary.last_state_change_frame = frame.frame_number;
        door.summary.last_state_change_time = frame.time;
        const std::string& line = summary.transition_history.back();
        if (warn)
        {
            if (hooks.log_warn)
            {
                hooks.log_warn(line);
            }
            return;
        }

        if (hooks.log_info)
        {
            hooks.log_info(line);
        }
    }

    void SetSupportAndState(
        MutableDoorState& door,
        const ScriptedMovementFrameContext& frame,
        std::string support_state,
        std::string movement_state,
        std::string blocked_reason,
        const BrushDoorBootstrapHooks& hooks,
        BrushDoorDispatchResult* dispatch_result,
        bool warn_on_change)
    {
        const bool state_changed =
            door.summary.support_state != support_state
            || door.summary.movement_state != movement_state
            || door.summary.blocked_reason != blocked_reason;
        door.summary.support_state = std::move(support_state);
        door.summary.movement_state = std::move(movement_state);
        door.summary.blocked_reason = std::move(blocked_reason);
        if (!state_changed)
        {
            return;
        }

        if (dispatch_result != nullptr)
        {
            dispatch_result->state_changed = true;
        }

        LogTransition(
            door,
            frame,
            "BrushDoorBootstrapController: " + BuildDoorLabel(door.summary)
                + " support=" + door.summary.support_state
                + " state=" + door.summary.movement_state
                + " blocked=" + SafeText(door.summary.blocked_reason),
            hooks,
            warn_on_change);
    }

    std::string BuildAuditLine(const BrushDoorEntityView& view) const
    {
        return "edict#" + std::to_string(view.edict_index)
            + " parse#" + std::to_string(view.parse_index)
            + " classname=" + SafeText(view.classname, "<empty>")
            + " targetname=" + SafeText(view.targetname)
            + " live=" + BoolLabel(view.in_use && !view.removed)
            + " removed=" + BoolLabel(view.removed)
            + " deferred=" + BoolLabel(view.deferred)
            + " private=" + BoolLabel(view.has_private_data)
            + " model=" + SafeText(view.model)
            + " modelindex=" + std::to_string(view.modelindex)
            + " origin=" + (view.has_origin ? FormatVector(view.origin) : std::string("<none>"))
            + " angles=" + (view.has_angles ? FormatVector(view.angles) : std::string("<none>"))
            + " movedir=" + (view.has_movedir ? FormatVector(view.movedir) : std::string("<none>"))
            + " speed=" + std::to_string(view.speed)
            + " lip=" + std::to_string(view.lip)
            + " wait=" + std::to_string(view.wait)
            + " spawnflags=" + std::to_string(view.spawnflags)
            + " lifecycle=" + SafeText(view.lifecycle);
    }

    bool PrepareMotion(
        MutableDoorState& door,
        const BrushDoorEntityView& view,
        std::string* blocked_reason)
    {
        if (blocked_reason != nullptr)
        {
            blocked_reason->clear();
        }

        if (view.edict_index < 0 || !view.in_use || view.removed)
        {
            if (blocked_reason != nullptr)
            {
                *blocked_reason = "runtime entity inactive or removed";
            }
            door.motion_ready = false;
            return false;
        }

        if (!view.has_origin)
        {
            if (blocked_reason != nullptr)
            {
                *blocked_reason = "missing runtime origin";
            }
            door.motion_ready = false;
            return false;
        }

        if (!view.has_size)
        {
            if (blocked_reason != nullptr)
            {
                *blocked_reason = "missing brush bounds";
            }
            door.motion_ready = false;
            return false;
        }

        if (view.model.empty())
        {
            if (blocked_reason != nullptr)
            {
                *blocked_reason = "missing brush model";
            }
            door.motion_ready = false;
            return false;
        }

        Vector movedir = view.movedir;
        if (!view.has_movedir || NormalizeVector(&movedir) <= 0.0001f)
        {
            if (blocked_reason != nullptr)
            {
                *blocked_reason = "missing brush movedir";
            }
            door.motion_ready = false;
            return false;
        }

        const Vector size = view.maxs - view.mins;
        // Keep the authored brush-door lip as-is, including negative values that encode
        // longer staged travel for map-specific doors such as room2train. Only fall back
        // when the parsed value is not finite.
        const float lip = std::isfinite(view.lip) ? view.lip : config.default_lip;
        const float raw_distance =
            std::fabs(movedir.x * size.x)
            + std::fabs(movedir.y * size.y)
            + std::fabs(movedir.z * size.z)
            - lip;
        if (raw_distance <= config.arrival_epsilon)
        {
            if (blocked_reason != nullptr)
            {
                *blocked_reason = "non-positive staged travel distance";
            }
            door.motion_ready = false;
            return false;
        }

        if (!door.summary.movement_started)
        {
            door.closed_origin = view.origin;
        }
        door.movedir = movedir;
        door.travel_distance = raw_distance;
        door.open_origin = door.closed_origin + (door.movedir * door.travel_distance);
        door.motion_ready = true;
        return true;
    }

    void MarkArrival(
        MutableDoorState& door,
        edict_t* entity,
        const ScriptedMovementFrameContext& frame,
        const BrushDoorBootstrapHooks& hooks,
        BrushDoorDispatchResult* dispatch_result)
    {
        if (entity != nullptr && hooks.set_velocity)
        {
            hooks.set_velocity(entity, Vector(0.0f, 0.0f, 0.0f));
        }
        if (entity != nullptr && hooks.set_origin)
        {
            hooks.set_origin(entity, door.open_origin);
            AppendUnique(summary.exercised_callbacks, "pfnSetOrigin");
        }

        door.summary.origin_text = FormatVector(door.open_origin);
        door.summary.movement_completed = true;
        SetSupportAndState(
            door,
            frame,
            "opened",
            "open",
            {},
            hooks,
            dispatch_result,
            false);
        if (dispatch_result != nullptr)
        {
            dispatch_result->movement_completed = true;
        }
    }

    void AdvanceOpening(
        MutableDoorState& door,
        const ScriptedMovementFrameContext& frame,
        const BrushDoorBootstrapHooks& hooks,
        BrushDoorDispatchResult* dispatch_result)
    {
        if (!door.motion_ready
            || !EqualsIgnoreCase(door.summary.movement_state, "opening")
            || door.last_motion_frame == frame.frame_number)
        {
            return;
        }

        edict_t* entity = hooks.entity_by_index ? hooks.entity_by_index(door.summary.edict_index) : nullptr;
        if (entity == nullptr)
        {
            SetSupportAndState(
                door,
                frame,
                "blocked",
                "blocked",
                "door edict disappeared before staged move",
                hooks,
                dispatch_result,
                true);
            return;
        }

        Vector current_origin = entity->v.origin;
        const Vector remaining = door.open_origin - current_origin;
        const float distance = remaining.Length();
        if (distance <= config.arrival_epsilon)
        {
            door.last_motion_frame = frame.frame_number;
            MarkArrival(door, entity, frame, hooks, dispatch_result);
            return;
        }

        const float speed = door.summary.speed > 0.0f ? door.summary.speed : config.default_speed;
        const float step = std::max(speed * std::max(frame.frametime, 0.0f), config.arrival_epsilon);
        Vector delta = remaining;
        NormalizeVector(&delta);
        const Vector next_origin =
            step >= distance ? door.open_origin : current_origin + (delta * step);

        if (hooks.set_velocity)
        {
            hooks.set_velocity(entity, Vector(0.0f, 0.0f, 0.0f));
        }
        if (hooks.set_origin)
        {
            hooks.set_origin(entity, next_origin);
            AppendUnique(summary.exercised_callbacks, "pfnSetOrigin");
        }

        door.summary.origin_text = FormatVector(next_origin);
        door.last_motion_frame = frame.frame_number;
        if ((door.open_origin - next_origin).Length() <= config.arrival_epsilon)
        {
            MarkArrival(door, entity, frame, hooks, dispatch_result);
        }
    }

    void RebuildSummary()
    {
        summary.tracked_doors = doors.size();
        summary.use_supported = 0;
        summary.moving = 0;
        summary.opened = 0;
        summary.blocked = 0;
        summary.deferred = 0;
        summary.doors_preview.clear();

        std::vector<const BrushDoorRuntimeSummary*> ordered;
        ordered.reserve(doors.size());
        for (const auto& [edict_index, door] : doors)
        {
            (void)edict_index;
            ordered.push_back(&door.summary);
            if (EqualsIgnoreCase(door.summary.support_state, "use-supported"))
            {
                ++summary.use_supported;
            }
            else if (EqualsIgnoreCase(door.summary.support_state, "moving"))
            {
                ++summary.moving;
            }
            else if (EqualsIgnoreCase(door.summary.support_state, "opened"))
            {
                ++summary.opened;
            }
            else if (EqualsIgnoreCase(door.summary.support_state, "blocked"))
            {
                ++summary.blocked;
            }
            else if (EqualsIgnoreCase(door.summary.support_state, "deferred"))
            {
                ++summary.deferred;
            }
        }

        std::sort(
            ordered.begin(),
            ordered.end(),
            [](const BrushDoorRuntimeSummary* left, const BrushDoorRuntimeSummary* right)
            {
                if (left == nullptr || right == nullptr)
                {
                    return left < right;
                }
                if (left->targetname != right->targetname)
                {
                    return left->targetname < right->targetname;
                }
                return left->edict_index < right->edict_index;
            });

        for (const BrushDoorRuntimeSummary* runtime : ordered)
        {
            if (runtime == nullptr || summary.doors_preview.size() >= config.preview_limit)
            {
                continue;
            }

            summary.doors_preview.push_back(*runtime);
        }

        summary.readiness =
            summary.opened > 0 || summary.moving > 0
            ? "staged-safe brush door bootstrap is active"
            : summary.use_supported > 0
            ? "brush-door bootstrap is ready for func_door path-node dispatch"
            : summary.tracked_doors > 0
            ? "brush-door bootstrap is tracking runtime func_door entities"
            : "brush-door bootstrap is waiting for runtime func_door targets";
    }
};

BrushDoorBootstrapController::BrushDoorBootstrapController()
    : impl_(std::make_unique<Impl>())
{
}

BrushDoorBootstrapController::~BrushDoorBootstrapController() = default;

void BrushDoorBootstrapController::Configure(const BrushDoorBootstrapConfig& config)
{
    impl_->config = config;
    if (impl_->config.default_speed <= 0.0f)
    {
        impl_->config.default_speed = 100.0f;
    }
    if (impl_->config.default_lip < 0.0f)
    {
        impl_->config.default_lip = 8.0f;
    }
    if (impl_->config.arrival_epsilon <= 0.0f)
    {
        impl_->config.arrival_epsilon = 1.0f;
    }
    if (impl_->config.preview_limit == 0)
    {
        impl_->config.preview_limit = 8;
    }
    if (impl_->config.history_limit == 0)
    {
        impl_->config.history_limit = 64;
    }

    impl_->current_frame = {};
    impl_->summary = {};
    impl_->summary.configured = true;
    impl_->doors.clear();
}

void BrushDoorBootstrapController::BeginFrame(const ScriptedMovementFrameContext& context)
{
    impl_->current_frame = context;
    impl_->summary.controller_ran = true;
    ++impl_->summary.frames_attempted;
}

BrushDoorDispatchResult BrushDoorBootstrapController::HandleUse(
    const BrushDoorUseRequest& request,
    const BrushDoorBootstrapHooks& hooks)
{
    BrushDoorDispatchResult result;
    result.attempted = true;
    result.native_use_attempted = request.native_use_attempted;
    result.native_use_succeeded = request.native_use_succeeded;
    result.staged_bootstrap_attempted = true;

    Impl::MutableDoorState& door = impl_->EnsureDoor(request.entity);
    door.summary.audit_line = impl_->BuildAuditLine(request.entity);
    door.summary.last_source_event = SafeText(request.source_label);
    door.summary.last_use_frame = request.frame_number;
    door.summary.last_use_time = request.time;
    door.summary.native_use_attempted = request.native_use_attempted;
    door.summary.native_use_succeeded = request.native_use_succeeded;
    door.summary.staged_bootstrap_attempted = true;
    result.audit_line = door.summary.audit_line;

    std::string blocked_reason;
    if (!impl_->PrepareMotion(door, request.entity, &blocked_reason))
    {
        door.summary.dispatch_path = "fallback-deferred";
        door.summary.staged_bootstrap_used = false;
        impl_->SetSupportAndState(
            door,
            impl_->current_frame,
            request.entity.in_use && !request.entity.removed ? "deferred" : "blocked",
            request.entity.in_use && !request.entity.removed ? "deferred" : "blocked",
            blocked_reason,
            hooks,
            &result,
            true);

        result.deferred = request.entity.in_use && !request.entity.removed;
        result.failed = !result.deferred;
        result.dispatch_path = door.summary.dispatch_path;
        result.support_state = door.summary.support_state;
        result.door_state = door.summary.movement_state;
        result.blocked_reason = door.summary.blocked_reason;
        result.detail =
            "staged-safe brush door bootstrap deferred"
            " path=" + door.summary.dispatch_path
            + " reason=" + SafeText(door.summary.blocked_reason)
            + " audit=" + door.summary.audit_line;
        impl_->RebuildSummary();
        return result;
    }

    door.summary.dispatch_path = "staged-bootstrap";
    door.summary.staged_bootstrap_used = true;
    result.staged_bootstrap_used = true;
    result.handled = true;
    result.use_succeeded = true;
    result.dispatch_path = door.summary.dispatch_path;

    if (EqualsIgnoreCase(door.summary.movement_state, "open"))
    {
        impl_->SetSupportAndState(
            door,
            impl_->current_frame,
            "opened",
            "open",
            {},
            hooks,
            &result,
            false);
    }
    else
    {
        const bool was_started = door.summary.movement_started;
        door.summary.movement_started = true;
        if (!was_started)
        {
            result.movement_started = true;
        }
        impl_->SetSupportAndState(
            door,
            impl_->current_frame,
            "moving",
            "opening",
            {},
            hooks,
            &result,
            false);
        impl_->AdvanceOpening(door, impl_->current_frame, hooks, &result);
    }

    result.support_state = door.summary.support_state;
    result.door_state = door.summary.movement_state;
    result.blocked_reason = door.summary.blocked_reason;
    result.detail =
        "staged-safe brush door bootstrap handled"
        " path=" + door.summary.dispatch_path
        + " state=" + SafeText(door.summary.movement_state)
        + " support=" + SafeText(door.summary.support_state)
        + " nativeAttempted=" + BoolLabel(request.native_use_attempted)
        + (request.native_use_detail.empty() ? std::string() : " native=" + request.native_use_detail)
        + " audit=" + door.summary.audit_line;

    impl_->RebuildSummary();
    return result;
}

void BrushDoorBootstrapController::RunFrame(
    const ScriptedMovementFrameContext& context,
    const std::vector<BrushDoorEntityView>& doors,
    const BrushDoorBootstrapHooks& hooks)
{
    impl_->current_frame = context;

    for (const BrushDoorEntityView& view : doors)
    {
        Impl::MutableDoorState& door = impl_->EnsureDoor(view);
        door.summary.audit_line = impl_->BuildAuditLine(view);

        if (!view.in_use || view.removed)
        {
            impl_->SetSupportAndState(
                door,
                context,
                "blocked",
                "blocked",
                "runtime entity inactive or removed",
                hooks,
                nullptr,
                true);
            continue;
        }

        if (EqualsIgnoreCase(door.summary.movement_state, "opening"))
        {
            std::string blocked_reason;
            if (!impl_->PrepareMotion(door, view, &blocked_reason))
            {
                impl_->SetSupportAndState(
                    door,
                    context,
                    "deferred",
                    "deferred",
                    blocked_reason,
                    hooks,
                    nullptr,
                    true);
                continue;
            }

            impl_->AdvanceOpening(door, context, hooks, nullptr);
        }
        else if (EqualsIgnoreCase(door.summary.movement_state, "open"))
        {
            impl_->SetSupportAndState(door, context, "opened", "open", {}, hooks, nullptr, false);
        }
        else
        {
            std::string blocked_reason;
            if (impl_->PrepareMotion(door, view, &blocked_reason))
            {
                impl_->SetSupportAndState(
                    door,
                    context,
                    "use-supported",
                    "closed",
                    {},
                    hooks,
                    nullptr,
                    false);
            }
            else
            {
                const bool preserve_deferred =
                    door.summary.staged_bootstrap_attempted
                    || EqualsIgnoreCase(door.summary.dispatch_path, "fallback-deferred");
                impl_->SetSupportAndState(
                    door,
                    context,
                    preserve_deferred ? std::string("deferred")
                                      : std::string("passive-recipient-only"),
                    preserve_deferred ? std::string("deferred") : std::string("closed"),
                    blocked_reason,
                    hooks,
                    nullptr,
                    false);
            }
        }
    }

    ++impl_->summary.frames_completed;
    impl_->RebuildSummary();
}

const BrushDoorBootstrapStateSummary& BrushDoorBootstrapController::Summary() const noexcept
{
    return impl_->summary;
}

const BrushDoorRuntimeSummary* BrushDoorBootstrapController::FindDoorState(int edict_index) const noexcept
{
    const auto it = impl_->doors.find(edict_index);
    return it != impl_->doors.end() ? &it->second.summary : nullptr;
}

const BrushDoorRuntimeSummary* BrushDoorBootstrapController::FindDoorByTargetname(
    std::string_view targetname) const noexcept
{
    for (const auto& [edict_index, door] : impl_->doors)
    {
        (void)edict_index;
        if (EqualsIgnoreCase(door.summary.targetname, targetname))
        {
            return &door.summary;
        }
    }

    return nullptr;
}
} // namespace hl::game_api::detail
