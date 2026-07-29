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

    bool ok() const noexcept
    {
        return status == GoldSrcCommandPlanStatus::kOk;
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
};

class GoldSrcCommandExecutionState final
{
public:
    void Reset() noexcept;

    GoldSrcCommandExecutionPlan Plan(
        const GoldSrcDecodedMoveCommand& move,
        std::uint32_t packet_sequence,
        std::uint64_t host_time_msec) noexcept;

    void CommitExecuted(
        const GoldSrcPlannedUserCommand& command) noexcept;
    void RecordRollback() noexcept;

    bool initialized() const noexcept;
    bool has_last_command() const noexcept;
    const GoldSrcDecodedUserCommand& last_command() const noexcept;
    std::uint32_t last_accepted_packet_sequence() const noexcept;
    std::uint64_t command_time_msec() const noexcept;
    std::uint64_t rollback_count() const noexcept;
    GoldSrcMovementPhase phase() const noexcept;
    const GoldSrcCommandExecutionDiagnostics& diagnostics() const noexcept;

private:
    bool initialized_ = false;
    bool has_last_command_ = false;
    std::uint32_t last_accepted_packet_sequence_ = 0u;
    std::uint64_t command_time_msec_ = 0u;
    std::uint64_t rollback_count_ = 0u;
    GoldSrcDecodedUserCommand last_command_{};
    GoldSrcMovementPhase phase_ =
        GoldSrcMovementPhase::kPlayerSpawnedAwaitingMovement;
    GoldSrcCommandExecutionDiagnostics diagnostics_{};
};
} // namespace hl::network
