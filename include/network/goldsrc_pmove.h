#pragma once

#include "network/goldsrc_client_move.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace hl::network
{
inline constexpr std::uint8_t kGoldSrcMaximumCommandMsec = 255u;
inline constexpr std::uint8_t kGoldSrcPmoveSplitThresholdMsec = 50u;
inline constexpr std::size_t kGoldSrcMaximumPmoveSubcommands = 8u;
inline constexpr std::size_t kGoldSrcMaximumDroppedCommandRecovery = 24u;
inline constexpr std::size_t kGoldSrcSyntheticReplayLimit = 0u;
inline constexpr std::uint32_t kGoldSrcMaximumMovePacketTimeMsec = 1000u;
inline constexpr std::uint32_t kGoldSrcMaximumCommandLeadMsec = 250u;
inline constexpr std::size_t kGoldSrcMaximumPlannedCommands =
    kGoldSrcMaximumMoveCommands + kGoldSrcMaximumDroppedCommandRecovery;

enum class GoldSrcMovementPhase
{
    kPlayerSpawnedAwaitingMovement,
    kMovementReady,
    kMovementExecuting,
    kMovementStable,
};

std::string_view NameFor(GoldSrcMovementPhase phase) noexcept;

enum class GoldSrcManualSessionPhase
{
    kWaitingForClient,
    kHandshakeInProgress,
    kSessionEstablished,
    kPersistentManualSession,
    kStopped,
};

std::string_view NameFor(GoldSrcManualSessionPhase phase) noexcept;

struct GoldSrcManualSessionConfig final
{
    bool persistent = false;
    std::uint64_t handshake_timeout_ms = 10000u;
    std::uint64_t movement_observation_ms = 2000u;
    std::uint64_t heartbeat_interval_ms = 30000u;
};

struct GoldSrcManualSessionEvents final
{
    bool handshake_timed_out = false;
    bool movement_milestone_reached = false;
    bool proof_completed = false;
    bool runtime_completed = false;
    bool heartbeat_due = false;
};

class GoldSrcManualSessionLifecycle final
{
public:
    explicit GoldSrcManualSessionLifecycle(
        GoldSrcManualSessionConfig config = {}) noexcept;

    void Start(std::uint64_t now_ms) noexcept;
    bool MarkClientAdmitted() noexcept;
    bool MarkSessionEstablished(std::uint64_t now_ms) noexcept;
    void RecordClcMove(std::uint64_t now_ms) noexcept;
    void RecordMovementSnapshot() noexcept;
    GoldSrcManualSessionEvents Advance(
        std::uint64_t now_ms,
        bool movement_snapshot_integrated) noexcept;
    bool MarkClientDisconnected() noexcept;
    void RequestShutdown() noexcept;

    bool can_pump() const noexcept;
    bool persistent() const noexcept;
    bool movement_milestone_reached() const noexcept;
    bool proof_complete() const noexcept;
    bool runtime_complete() const noexcept;
    bool shutdown_requested() const noexcept;
    bool global_deadline_suppressed() const noexcept;
    std::uint64_t clc_moves_after_milestone() const noexcept;
    std::uint64_t snapshots_after_milestone() const noexcept;
    std::uint64_t movement_milestone_count() const noexcept;
    std::uint64_t disconnect_count() const noexcept;
    std::uint64_t reconnect_count() const noexcept;
    GoldSrcManualSessionPhase phase() const noexcept;

private:
    GoldSrcManualSessionConfig config_{};
    GoldSrcManualSessionPhase phase_ =
        GoldSrcManualSessionPhase::kWaitingForClient;
    std::uint64_t deadline_ms_ = 0u;
    std::uint64_t next_heartbeat_ms_ = 0u;
    std::uint64_t first_movement_ms_ = 0u;
    std::uint64_t movement_commands_ = 0u;
    std::uint64_t movement_snapshots_ = 0u;
    std::uint64_t clc_moves_after_milestone_ = 0u;
    std::uint64_t snapshots_after_milestone_ = 0u;
    std::uint64_t movement_milestone_count_ = 0u;
    std::uint64_t disconnect_count_ = 0u;
    std::uint64_t reconnect_count_ = 0u;
    bool started_ = false;
    bool client_admitted_ = false;
    bool session_established_ = false;
    bool session_ever_established_ = false;
    bool first_movement_recorded_ = false;
    bool movement_milestone_reached_ = false;
    bool proof_complete_ = false;
    bool runtime_complete_ = false;
    bool shutdown_requested_ = false;
};

enum class GoldSrcCommandPlanStatus
{
    kOk,
    kInvalidCount,
    kExcessiveBackupCount,
    kExcessiveNewCount,
    kStalePacket,
    kOutOfOrderPacket,
    kInvalidCommandMsec,
    kCommandTimeOverflow,
    kCommandTimeBudgetExceeded,
    kTemporarilyAhead,
    kPlanCapacityExceeded,
};

std::string_view ReasonFor(GoldSrcCommandPlanStatus status) noexcept;

struct GoldSrcPlannedUserCommand final
{
    GoldSrcDecodedUserCommand command;
    std::uint32_t source_packet_sequence = 0u;
    bool recovered_backup = false;
    bool replayed_last_command = false;
    bool runfuncs = true;
};

struct GoldSrcCommandExecutionPlan final
{
    GoldSrcCommandPlanStatus status = GoldSrcCommandPlanStatus::kInvalidCount;
    std::array<
        GoldSrcPlannedUserCommand,
        kGoldSrcMaximumPlannedCommands>
        commands{};
    std::size_t command_count = 0u;
    std::size_t new_commands = 0u;
    std::size_t recovered_backups = 0u;
    std::size_t replayed_last_commands = 0u;
    std::size_t duplicate_backups_suppressed = 0u;
    std::uint32_t total_command_msec = 0u;
    std::uint64_t host_time_msec = 0u;
    std::uint64_t host_elapsed_msec = 0u;
    std::uint64_t command_elapsed_msec = 0u;
    std::uint64_t proposed_command_elapsed_msec = 0u;
    std::uint32_t raw_netchan_sequence_distance = 0u;
    bool establishes_command_clock_epoch = false;

    bool ok() const noexcept
    {
        return status == GoldSrcCommandPlanStatus::kOk;
    }

    bool observation_committable() const noexcept
    {
        return status == GoldSrcCommandPlanStatus::kOk
            || status == GoldSrcCommandPlanStatus::kTemporarilyAhead;
    }
};

struct GoldSrcPmoveSplitResult final
{
    bool valid = false;
    std::array<std::uint8_t, kGoldSrcMaximumPmoveSubcommands> msec{};
    std::size_t count = 0u;
    std::uint32_t total_msec = 0u;
};

GoldSrcPmoveSplitResult SplitGoldSrcPmoveCommand(
    std::uint8_t msec) noexcept;

struct GoldSrcCommandExecutionDiagnostics final
{
    std::uint64_t packets_received = 0u;
    std::uint64_t commands_received = 0u;
    std::uint64_t commands_executed = 0u;
    std::uint64_t backup_commands_observed = 0u;
    std::uint64_t backup_commands_replayed = 0u;
    std::uint64_t duplicate_commands_suppressed = 0u;
    std::uint64_t malformed_commands_rejected = 0u;
    std::uint64_t command_time_budget_rejections = 0u;
    std::uint64_t move_packets_observed = 0u;
    std::uint64_t move_packets_validated = 0u;
    std::uint64_t move_packets_executed = 0u;
    std::uint64_t move_packets_temporarily_rejected = 0u;
    std::uint64_t command_clock_recoveries = 0u;
    std::uint64_t command_clock_resynchronizations = 0u;
    std::uint64_t raw_netchan_sequence_gaps = 0u;
    std::uint64_t raw_netchan_gap_move_replays = 0u;
    std::uint64_t synthetic_command_replays = 0u;
    std::uint64_t maximum_move_execution_gap_msec = 0u;
};

class GoldSrcCommandExecutionState final
{
public:
    void Reset() noexcept;

    GoldSrcCommandExecutionPlan Plan(
        const GoldSrcDecodedMoveCommand& move,
        std::uint32_t packet_sequence,
        std::uint64_t host_time_msec) noexcept;

    void CommitObservedMovePacket(
        std::uint32_t packet_sequence,
        bool structurally_valid,
        bool temporarily_ahead = false) noexcept;
    void CommitExecutedBatch(
        const GoldSrcCommandExecutionPlan& plan,
        std::uint32_t packet_sequence,
        std::uint64_t host_time_msec) noexcept;
    void RecordRollback() noexcept;

    bool initialized() const noexcept;
    bool has_last_command() const noexcept;
    const GoldSrcDecodedUserCommand& last_command() const noexcept;
    std::uint32_t last_accepted_packet_sequence() const noexcept;
    std::uint32_t last_observed_packet_sequence() const noexcept;
    std::uint32_t last_validated_move_sequence() const noexcept;
    std::uint32_t last_executed_move_sequence() const noexcept;
    std::uint64_t command_time_msec() const noexcept;
    std::uint64_t host_epoch_msec() const noexcept;
    bool command_clock_epoch_initialized() const noexcept;
    std::uint64_t rollback_count() const noexcept;
    GoldSrcMovementPhase phase() const noexcept;
    const GoldSrcCommandExecutionDiagnostics& diagnostics() const noexcept;

private:
    bool observed_initialized_ = false;
    bool validated_initialized_ = false;
    bool executed_initialized_ = false;
    bool command_clock_epoch_initialized_ = false;
    bool has_last_command_ = false;
    bool command_clock_wait_pending_ = false;
    std::uint32_t last_observed_packet_sequence_ = 0u;
    std::uint32_t last_validated_move_sequence_ = 0u;
    std::uint32_t last_executed_move_sequence_ = 0u;
    std::uint64_t host_epoch_msec_ = 0u;
    std::uint64_t command_time_msec_ = 0u;
    std::uint64_t last_execution_host_time_msec_ = 0u;
    std::uint64_t rollback_count_ = 0u;
    GoldSrcDecodedUserCommand last_command_{};
    GoldSrcMovementPhase phase_ =
        GoldSrcMovementPhase::kPlayerSpawnedAwaitingMovement;
    GoldSrcCommandExecutionDiagnostics diagnostics_{};
};
} // namespace hl::network
