#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "game_api/hl_server_module.h"
#include "scripted_movement_controller.h"

#pragma warning(push, 0)
#include "extdll.h"
#pragma warning(pop)

namespace hl::game_api::detail
{
struct BrushDoorBootstrapConfig
{
    float default_speed = 100.0f;
    float default_lip = 8.0f;
    float arrival_epsilon = 1.0f;
    std::size_t preview_limit = 8;
    std::size_t history_limit = 64;
};

struct BrushDoorEntityView
{
    int edict_index = -1;
    std::size_t parse_index = 0;
    std::string classname;
    std::string targetname;
    std::string model;
    int modelindex = 0;
    Vector origin = Vector(0.0f, 0.0f, 0.0f);
    bool has_origin = false;
    Vector angles = Vector(0.0f, 0.0f, 0.0f);
    bool has_angles = false;
    Vector movedir = Vector(0.0f, 0.0f, 0.0f);
    bool has_movedir = false;
    Vector mins = Vector(0.0f, 0.0f, 0.0f);
    Vector maxs = Vector(0.0f, 0.0f, 0.0f);
    bool has_size = false;
    float speed = 0.0f;
    float lip = 0.0f;
    float wait = 0.0f;
    int spawnflags = 0;
    float health = 0.0f;
    bool health_available = false;
    float damage = 0.0f;
    bool damage_available = false;
    bool in_use = false;
    bool removed = false;
    bool deferred = false;
    bool has_private_data = false;
    std::string lifecycle;
};

struct BrushDoorUseRequest
{
    BrushDoorEntityView entity;
    int source_edict_index = -1;
    std::string source_label;
    std::string reason;
    int frame_number = 0;
    float time = 0.0f;
    float frametime = 0.0f;
    bool native_use_attempted = false;
    bool native_use_succeeded = false;
    std::string native_use_detail;
};

struct BrushDoorBootstrapHooks
{
    std::function<edict_t*(int)> entity_by_index;
    std::function<void(edict_t*, const Vector&)> set_origin;
    std::function<void(edict_t*, const Vector&)> set_velocity;
    std::function<void(std::string_view)> log_info;
    std::function<void(std::string_view)> log_warn;
};

struct BrushDoorDispatchResult
{
    bool attempted = false;
    bool handled = false;
    bool deferred = false;
    bool failed = false;
    bool use_succeeded = false;
    bool state_changed = false;
    bool movement_started = false;
    bool movement_completed = false;
    bool native_use_attempted = false;
    bool native_use_succeeded = false;
    bool staged_bootstrap_attempted = false;
    bool staged_bootstrap_used = false;
    std::string dispatch_path;
    std::string support_state;
    std::string door_state;
    std::string blocked_reason;
    std::string audit_line;
    std::string detail;
};

class BrushDoorBootstrapController
{
public:
    BrushDoorBootstrapController();
    ~BrushDoorBootstrapController();

    BrushDoorBootstrapController(const BrushDoorBootstrapController&) = delete;
    BrushDoorBootstrapController& operator=(const BrushDoorBootstrapController&) = delete;

    void Configure(const BrushDoorBootstrapConfig& config);
    void BeginFrame(const ScriptedMovementFrameContext& context);
    BrushDoorDispatchResult HandleUse(
        const BrushDoorUseRequest& request,
        const BrushDoorBootstrapHooks& hooks);
    void RunFrame(
        const ScriptedMovementFrameContext& context,
        const std::vector<BrushDoorEntityView>& doors,
        const BrushDoorBootstrapHooks& hooks);

    const hl::game_api::BrushDoorBootstrapStateSummary& Summary() const noexcept;
    const hl::game_api::BrushDoorRuntimeSummary* FindDoorState(int edict_index) const noexcept;
    const hl::game_api::BrushDoorRuntimeSummary* FindDoorByTargetname(
        std::string_view targetname) const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace hl::game_api::detail
