#include "network/goldsrc_pmove.h"

#include "network/goldsrc_netchan.h"

#include <algorithm>
#include <limits>

namespace hl::network
{
namespace
{
bool CheckedAdd(std::uint32_t value, std::uint32_t* total) noexcept
{
    if (total == nullptr
        || value > std::numeric_limits<std::uint32_t>::max() - *total)
    {
        return false;
    }
    *total += value;
    return true;
}

bool Append(
    GoldSrcCommandExecutionPlan* plan,
    const GoldSrcDecodedUserCommand& command,
    std::uint32_t packet_sequence,
    bool recovered,
    bool replayed) noexcept
{
    if (plan == nullptr || plan->command_count >= plan->commands.size())
    {
        return false;
    }
    GoldSrcPlannedUserCommand& output =
        plan->commands[plan->command_count++];
    output.command = command;
    output.source_packet_sequence = packet_sequence;
    output.recovered_backup = recovered;
    output.replayed_last_command = replayed;
    output.runfuncs = true;
    return true;
}
} // namespace

std::string_view NameFor(GoldSrcMovementPhase phase) noexcept
{
    switch (phase)
    {
    case GoldSrcMovementPhase::kPlayerSpawnedAwaitingMovement:
        return "player_spawned_awaiting_movement";
    case GoldSrcMovementPhase::kMovementReady:
        return "movement_ready";
    case GoldSrcMovementPhase::kMovementExecuting:
        return "movement_executing";
    case GoldSrcMovementPhase::kMovementStable:
        return "movement_stable";
    }
    return "player_spawned_awaiting_movement";
}

std::string_view ReasonFor(GoldSrcCommandPlanStatus status) noexcept
{
    switch (status)
    {
    case GoldSrcCommandPlanStatus::kOk:
        return "ok";
    case GoldSrcCommandPlanStatus::kInvalidCount:
        return "invalid_command_count";
    case GoldSrcCommandPlanStatus::kExcessiveBackupCount:
        return "excessive_backup_count";
    case GoldSrcCommandPlanStatus::kExcessiveNewCount:
        return "excessive_new_count";
    case GoldSrcCommandPlanStatus::kStalePacket:
        return "stale_command";
    case GoldSrcCommandPlanStatus::kOutOfOrderPacket:
        return "out_of_order_packet";
    case GoldSrcCommandPlanStatus::kInvalidCommandMsec:
        return "invalid_command_msec";
    case GoldSrcCommandPlanStatus::kCommandTimeOverflow:
        return "command_time_overflow";
    case GoldSrcCommandPlanStatus::kCommandTimeBudgetExceeded:
        return "command_time_budget_exceeded";
    case GoldSrcCommandPlanStatus::kPlanCapacityExceeded:
        return "command_plan_capacity_exceeded";
    }
    return "invalid_command_count";
}

GoldSrcPmoveSplitResult SplitGoldSrcPmoveCommand(
    std::uint8_t msec) noexcept
{
    GoldSrcPmoveSplitResult result;
    if (msec == 0u)
    {
        return result;
    }

    std::uint32_t remaining = msec;
    while (remaining > 0u)
    {
        if (result.count >= result.msec.size())
        {
            return {};
        }
        const std::uint32_t pieces_left =
            (remaining + kGoldSrcPmoveSplitThresholdMsec - 1u)
            / kGoldSrcPmoveSplitThresholdMsec;
        const std::uint32_t piece =
            (remaining + pieces_left - 1u) / pieces_left;
        if (piece == 0u || piece > kGoldSrcPmoveSplitThresholdMsec)
        {
            return {};
        }
        result.msec[result.count++] = static_cast<std::uint8_t>(piece);
        result.total_msec += piece;
        remaining -= piece;
    }
    result.valid = result.total_msec == msec;
    return result;
}

void GoldSrcCommandExecutionState::Reset() noexcept
{
    *this = {};
}

GoldSrcCommandExecutionPlan GoldSrcCommandExecutionState::Plan(
    const GoldSrcDecodedMoveCommand& move,
    std::uint32_t packet_sequence,
    std::uint64_t host_time_msec) noexcept
{
    GoldSrcCommandExecutionPlan plan;
    ++diagnostics_.packets_received;
    diagnostics_.commands_received += move.command_count;
    diagnostics_.backup_commands_observed += move.backup_command_count;

    const std::size_t backup = move.backup_command_count;
    const std::size_t fresh = move.new_command_count;
    if (backup > kGoldSrcMaximumMoveCommands)
    {
        plan.status = GoldSrcCommandPlanStatus::kExcessiveBackupCount;
    }
    else if (fresh > kGoldSrcMaximumMoveCommands)
    {
        plan.status = GoldSrcCommandPlanStatus::kExcessiveNewCount;
    }
    else if (backup + fresh != move.command_count
        || move.command_count > kGoldSrcMaximumMoveCommands)
    {
        plan.status = GoldSrcCommandPlanStatus::kInvalidCount;
    }
    else if (initialized_
        && packet_sequence == last_accepted_packet_sequence_)
    {
        plan.status = GoldSrcCommandPlanStatus::kStalePacket;
        diagnostics_.duplicate_commands_suppressed += fresh + backup;
    }
    else if (initialized_
        && !IsGoldSrcNetchanSequenceNewer(
            packet_sequence,
            last_accepted_packet_sequence_))
    {
        plan.status = GoldSrcCommandPlanStatus::kOutOfOrderPacket;
        diagnostics_.duplicate_commands_suppressed += fresh + backup;
    }
    else
    {
        plan.status = GoldSrcCommandPlanStatus::kOk;
    }
    if (!plan.ok())
    {
        ++diagnostics_.malformed_commands_rejected;
        return plan;
    }

    for (std::size_t index = 0; index < move.command_count; ++index)
    {
        if (move.commands[index].msec == 0u)
        {
            plan.status = GoldSrcCommandPlanStatus::kInvalidCommandMsec;
            ++diagnostics_.malformed_commands_rejected;
            return plan;
        }
    }

    std::size_t dropped = 0u;
    if (initialized_)
    {
        const std::uint32_t distance = GoldSrcNetchanSequenceDistance(
            packet_sequence,
            last_accepted_packet_sequence_);
        if (distance == 0u)
        {
            plan.status = GoldSrcCommandPlanStatus::kOutOfOrderPacket;
            ++diagnostics_.malformed_commands_rejected;
            return plan;
        }
        dropped = std::min<std::size_t>(
            distance - 1u,
            kGoldSrcMaximumDroppedCommandRecovery);
    }

    if (dropped == 0u)
    {
        plan.duplicate_backups_suppressed = backup;
        diagnostics_.duplicate_commands_suppressed += backup;
    }
    else
    {
        const std::size_t available = std::min(dropped, backup);
        const std::size_t missing = dropped - available;
        if (missing > 0u && has_last_command_)
        {
            for (std::size_t index = 0; index < missing; ++index)
            {
                if (!Append(
                        &plan,
                        last_command_,
                        packet_sequence,
                        true,
                        true))
                {
                    plan.status =
                        GoldSrcCommandPlanStatus::kPlanCapacityExceeded;
                    ++diagnostics_.malformed_commands_rejected;
                    return plan;
                }
                ++plan.replayed_last_commands;
            }
        }
        for (std::size_t offset = available; offset > 0u; --offset)
        {
            const std::size_t command_index = fresh + offset - 1u;
            if (!Append(
                    &plan,
                    move.commands[command_index],
                    packet_sequence,
                    true,
                    false))
            {
                plan.status =
                    GoldSrcCommandPlanStatus::kPlanCapacityExceeded;
                ++diagnostics_.malformed_commands_rejected;
                return plan;
            }
            ++plan.recovered_backups;
        }
        if (backup > available)
        {
            plan.duplicate_backups_suppressed = backup - available;
            diagnostics_.duplicate_commands_suppressed += backup - available;
        }
    }

    for (std::size_t offset = fresh; offset > 0u; --offset)
    {
        if (!Append(
                &plan,
                move.commands[offset - 1u],
                packet_sequence,
                false,
                false))
        {
            plan.status = GoldSrcCommandPlanStatus::kPlanCapacityExceeded;
            ++diagnostics_.malformed_commands_rejected;
            return plan;
        }
        ++plan.new_commands;
    }

    for (std::size_t index = 0; index < plan.command_count; ++index)
    {
        if (!CheckedAdd(plan.commands[index].command.msec, &plan.total_command_msec))
        {
            plan.status = GoldSrcCommandPlanStatus::kCommandTimeOverflow;
            ++diagnostics_.malformed_commands_rejected;
            return plan;
        }
    }
    if (plan.total_command_msec > kGoldSrcMaximumMovePacketTimeMsec)
    {
        plan.status = GoldSrcCommandPlanStatus::kCommandTimeBudgetExceeded;
        ++diagnostics_.command_time_budget_rejections;
        return plan;
    }
    if (command_time_msec_ > std::numeric_limits<std::uint64_t>::max()
            - plan.total_command_msec
        || command_time_msec_ + plan.total_command_msec
            > host_time_msec + kGoldSrcMaximumCommandLeadMsec)
    {
        plan.status = GoldSrcCommandPlanStatus::kCommandTimeBudgetExceeded;
        ++diagnostics_.command_time_budget_rejections;
        return plan;
    }

    initialized_ = true;
    last_accepted_packet_sequence_ = packet_sequence;
    if (phase_ == GoldSrcMovementPhase::kPlayerSpawnedAwaitingMovement)
    {
        phase_ = GoldSrcMovementPhase::kMovementReady;
    }
    return plan;
}

void GoldSrcCommandExecutionState::CommitExecuted(
    const GoldSrcPlannedUserCommand& command) noexcept
{
    has_last_command_ = true;
    last_command_ = command.command;
    command_time_msec_ += command.command.msec;
    ++diagnostics_.commands_executed;
    if (command.recovered_backup)
    {
        ++diagnostics_.backup_commands_replayed;
    }
    phase_ = diagnostics_.commands_executed > 1u
        ? GoldSrcMovementPhase::kMovementStable
        : GoldSrcMovementPhase::kMovementExecuting;
}

void GoldSrcCommandExecutionState::RecordRollback() noexcept
{
    ++rollback_count_;
}

bool GoldSrcCommandExecutionState::initialized() const noexcept
{
    return initialized_;
}

bool GoldSrcCommandExecutionState::has_last_command() const noexcept
{
    return has_last_command_;
}

const GoldSrcDecodedUserCommand&
GoldSrcCommandExecutionState::last_command() const noexcept
{
    return last_command_;
}

std::uint32_t
GoldSrcCommandExecutionState::last_accepted_packet_sequence() const noexcept
{
    return last_accepted_packet_sequence_;
}

std::uint64_t GoldSrcCommandExecutionState::command_time_msec() const noexcept
{
    return command_time_msec_;
}

std::uint64_t GoldSrcCommandExecutionState::rollback_count() const noexcept
{
    return rollback_count_;
}

GoldSrcMovementPhase GoldSrcCommandExecutionState::phase() const noexcept
{
    return phase_;
}

const GoldSrcCommandExecutionDiagnostics&
GoldSrcCommandExecutionState::diagnostics() const noexcept
{
    return diagnostics_;
}
} // namespace hl::network
