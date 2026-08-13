#pragma once

#include "network/goldsrc_pmove.h"
#include "network/goldsrc_combat.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string_view>

struct edict_s;
struct clientdata_s;
struct playermove_s;
struct usercmd_s;

namespace hl::game_api::detail
{
struct WorldModelContext;

// GoldSrc clears this engine-owned callback buffer before handing it to the
// Game DLL. This must be an explicit byte reset: the HLSDK Vector default
// constructor intentionally leaves its components uninitialized.
void PrepareGoldSrcClientDataForGameDll(clientdata_s* output) noexcept;

using GoldSrcPmInitCallback = void (*)(playermove_s*);
using GoldSrcPmMoveCallback = void (*)(playermove_s*, int);
using GoldSrcCmdStartCallback = void (*)(
    const edict_s*,
    const usercmd_s*,
    unsigned int);
using GoldSrcCmdEndCallback = void (*)(const edict_s*);
using GoldSrcPlayerThinkCallback = void (*)(edict_s*);

struct GoldSrcPmoveGameDllCallbacks final
{
    GoldSrcPmInitCallback pm_init = nullptr;
    GoldSrcPmMoveCallback pm_move = nullptr;
    GoldSrcCmdStartCallback cmd_start = nullptr;
    GoldSrcCmdEndCallback cmd_end = nullptr;
    GoldSrcPlayerThinkCallback player_pre_think = nullptr;
    GoldSrcPlayerThinkCallback player_post_think = nullptr;
    GoldSrcPlayerThinkCallback entity_think = nullptr;
    float* global_time = nullptr;
    float* global_frametime = nullptr;
    int* active_attack_postthink_slot = nullptr;
    bool combat_enabled = false;
};

struct GoldSrcWorldLineTrace final
{
    float fraction = 1.0f;
    float end_position[3]{};
    float plane_normal[3]{};
    float plane_distance = 0.0f;
    bool all_solid = false;
    bool start_solid = false;
    bool hit_world = false;
};

struct GoldSrcCombatClientDiagnostics final
{
    network::GoldSrcCombatPhase phase =
        network::GoldSrcCombatPhase::kDisabled;
    std::uint64_t attack_commands_received = 0u;
    std::uint64_t attack_commands_executed = 0u;
    std::uint64_t duplicate_attack_commands_suppressed = 0u;
    std::uint64_t unsupported_gameplay_inputs_masked = 0u;
    std::uint64_t player_prethink_calls = 0u;
    std::uint64_t player_postthink_calls = 0u;
    std::uint64_t entity_think_calls = 0u;
    std::uint64_t respawn_inputs_forwarded = 0u;
    std::uint64_t callback_failures = 0u;
    std::uint64_t gameplay_time_msec = 0u;
};

struct GoldSrcMovevarsConfig final
{
    float gravity = 800.0f;
    float stop_speed = 100.0f;
    float maximum_speed = 320.0f;
    float spectator_maximum_speed = 500.0f;
    float accelerate = 10.0f;
    float air_accelerate = 10.0f;
    float water_accelerate = 10.0f;
    float friction = 4.0f;
    float edge_friction = 2.0f;
    float water_friction = 1.0f;
    float entity_gravity = 1.0f;
    float bounce = 1.0f;
    float step_size = 18.0f;
    float maximum_velocity = 2000.0f;
    float z_maximum = 4096.0f;
    float wave_height = 0.0f;
    bool footsteps = true;
    float roll_angle = 0.0f;
    float roll_speed = 0.0f;
    bool multiplayer = false;
};

enum class GoldSrcPmoveExecutionStatus
{
    kOk,
    kInvalidClient,
    kNotReady,
    kWorldUnavailable,
    kCallbackMissing,
    kPmInitFailed,
    kCommandPlanRejected,
    kContextInvalid,
    kPmMoveFailed,
    kOutputInvalid,
};

std::string_view ReasonFor(GoldSrcPmoveExecutionStatus status) noexcept;

struct GoldSrcPmoveExecutionResult final
{
    GoldSrcPmoveExecutionStatus status =
        GoldSrcPmoveExecutionStatus::kNotReady;
    network::GoldSrcCommandPlanStatus command_status =
        network::GoldSrcCommandPlanStatus::kOk;
    std::size_t commands_executed = 0u;
    std::size_t subcommands_executed = 0u;
    std::size_t backups_replayed = 0u;
    std::size_t duplicates_suppressed = 0u;
    std::size_t fresh_commands = 0u;
    std::size_t backup_commands = 0u;
    std::size_t recovered_commands = 0u;
    std::size_t synthetic_replays = 0u;
    std::uint32_t raw_netchan_sequence_distance = 0u;
    std::uint32_t packet_command_msec = 0u;
    std::uint64_t host_elapsed_msec = 0u;
    std::uint64_t command_elapsed_msec = 0u;
    std::uint64_t proposed_command_elapsed_msec = 0u;
    std::uint32_t last_observed_move_sequence = 0u;
    std::uint32_t last_validated_move_sequence = 0u;
    std::uint32_t last_executed_move_sequence = 0u;
    std::string_view output_validation_reason = "not_checked";
    // Fixed semantic identifier for the first failing execution stage. This
    // is safe to surface in automation without exposing exception details.
    std::string_view gameplay_failure_stage = "none";
    bool authoritative_state_changed = false;
    bool grounded = false;
    bool ducked = false;
    bool clock_recovered = false;
    // A gameplay callback may have changed opaque Game-DLL state that the
    // engine cannot safely roll back. The runtime is fail-stopped until the
    // Game DLL callback surface is successfully reinitialized.
    bool gameplay_callback_failure = false;
    bool gameplay_fail_stop = false;
    // Pre-gameplay failures retain the legacy transactional rollback and may
    // be retried without reinitializing the Game DLL.
    bool recoverable_rollback = false;

    bool ok() const noexcept
    {
        return status == GoldSrcPmoveExecutionStatus::kOk;
    }
};

struct GoldSrcPmoveDiagnostics final
{
    std::uint64_t pm_init_calls = 0u;
    std::uint64_t pm_move_calls = 0u;
    std::uint64_t cmd_start_calls = 0u;
    std::uint64_t cmd_end_calls = 0u;
    std::uint64_t contexts_built = 0u;
    std::uint64_t movement_commits = 0u;
    std::uint64_t movement_rollbacks = 0u;
    std::uint64_t movement_snapshots_sent = 0u;
    std::uint64_t player_origin_updates = 0u;
    std::uint64_t player_velocity_updates = 0u;
    std::uint64_t player_ground_state_updates = 0u;
    std::uint64_t player_hull_updates = 0u;
    std::uint64_t service_callback_calls = 0u;
    std::uint64_t player_trace_calls = 0u;
    std::uint64_t test_position_calls = 0u;
    std::uint64_t point_contents_calls = 0u;
    std::uint64_t hull_contents_calls = 0u;
    std::uint64_t model_service_calls = 0u;
    std::uint64_t random_calls = 0u;
    std::uint64_t file_service_calls = 0u;
    bool pm_init_complete = false;
    bool world_collision_ready = false;
    bool callback_table_complete = false;
};

class GoldSrcPmoveRuntime final
{
public:
    GoldSrcPmoveRuntime();
    ~GoldSrcPmoveRuntime();
    GoldSrcPmoveRuntime(GoldSrcPmoveRuntime&&) noexcept;
    GoldSrcPmoveRuntime& operator=(GoldSrcPmoveRuntime&&) noexcept;
    GoldSrcPmoveRuntime(const GoldSrcPmoveRuntime&) = delete;
    GoldSrcPmoveRuntime& operator=(const GoldSrcPmoveRuntime&) = delete;

    bool InitializeWorld(const WorldModelContext& world);
    bool InitializeGameDll(
        const GoldSrcPmoveGameDllCallbacks& callbacks,
        const std::filesystem::path& game_directory,
        const GoldSrcMovevarsConfig& movevars);
    void ResetClient(std::size_t client_slot) noexcept;
    void ResetGameDll() noexcept;

    GoldSrcPmoveExecutionResult Execute(
        std::size_t client_slot,
        const network::GoldSrcDecodedMoveCommand& move,
        std::uint32_t packet_sequence,
        std::uint64_t host_time_msec,
        edict_s* player,
        edict_s* world) noexcept;

    const network::GoldSrcCommandExecutionState* CommandState(
        std::size_t client_slot) const noexcept;
    const GoldSrcPmoveDiagnostics& diagnostics() const noexcept;
    std::size_t implemented_service_callback_count() const noexcept;
    bool movement_ready(std::size_t client_slot) const noexcept;
    bool movement_executed(std::size_t client_slot) const noexcept;
    bool IsPlayerPositionValid(
        const float* origin,
        int use_hull) const noexcept;
    bool IsWorldPointOpen(const float* point) const noexcept;
    bool TraceWorldLine(
        const float* start,
        const float* end,
        GoldSrcWorldLineTrace* output) const noexcept;
    bool TraceWorldHull(
        const float* start,
        const float* end,
        int use_hull,
        GoldSrcWorldLineTrace* output) const noexcept;
    void SetCombatPlayerReady(
        std::size_t client_slot,
        bool player_ready,
        int active_weapon_id,
        bool glock_state_ready) noexcept;
    const GoldSrcCombatClientDiagnostics* CombatDiagnostics(
        std::size_t client_slot) const noexcept;
    void RecordMovementSnapshot(std::size_t client_slot) noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace hl::game_api::detail
