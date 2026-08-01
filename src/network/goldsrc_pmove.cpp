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

std::string_view NameFor(GoldSrcManualSessionPhase phase) noexcept
{
    switch (phase)
    {
    case GoldSrcManualSessionPhase::kWaitingForClient:
        return "waiting_for_client";
    case GoldSrcManualSessionPhase::kHandshakeInProgress:
        return "handshake_in_progress";
    case GoldSrcManualSessionPhase::kSessionEstablished:
        return "session_established";
    case GoldSrcManualSessionPhase::kPersistentManualSession:
        return "persistent_manual_session";
    case GoldSrcManualSessionPhase::kStopped:
        return "stopped";
    }
    return "waiting_for_client";
}

GoldSrcManualSessionLifecycle::GoldSrcManualSessionLifecycle(
    GoldSrcManualSessionConfig config) noexcept
    : config_(config)
{
}

void GoldSrcManualSessionLifecycle::Start(std::uint64_t now_ms) noexcept
{
    const GoldSrcManualSessionConfig config = config_;
    *this = GoldSrcManualSessionLifecycle(config);
    started_ = true;
    deadline_ms_ = now_ms > std::numeric_limits<std::uint64_t>::max()
            - config_.handshake_timeout_ms
        ? std::numeric_limits<std::uint64_t>::max()
        : now_ms + config_.handshake_timeout_ms;
}

bool GoldSrcManualSessionLifecycle::MarkClientAdmitted() noexcept
{
    if (!can_pump())
    {
        return false;
    }
    const bool reconnected = session_ever_established_ && !session_established_;
    if (reconnected)
    {
        ++reconnect_count_;
    }
    client_admitted_ = true;
    phase_ = GoldSrcManualSessionPhase::kHandshakeInProgress;
    return reconnected;
}

bool GoldSrcManualSessionLifecycle::MarkSessionEstablished(
    std::uint64_t now_ms) noexcept
{
    if (!can_pump() || !client_admitted_ || session_established_)
    {
        return false;
    }
    session_established_ = true;
    session_ever_established_ = true;
    phase_ = config_.persistent
        ? GoldSrcManualSessionPhase::kPersistentManualSession
        : GoldSrcManualSessionPhase::kSessionEstablished;
    next_heartbeat_ms_ = now_ms
        > std::numeric_limits<std::uint64_t>::max()
                - config_.heartbeat_interval_ms
        ? std::numeric_limits<std::uint64_t>::max()
        : now_ms + config_.heartbeat_interval_ms;
    return true;
}

void GoldSrcManualSessionLifecycle::RecordClcMove(
    std::uint64_t now_ms) noexcept
{
    if (!can_pump())
    {
        return;
    }
    if (!first_movement_recorded_)
    {
        first_movement_recorded_ = true;
        first_movement_ms_ = now_ms;
    }
    ++movement_commands_;
    if (movement_milestone_reached_)
    {
        ++clc_moves_after_milestone_;
    }
}

void GoldSrcManualSessionLifecycle::RecordMovementSnapshot() noexcept
{
    if (!can_pump())
    {
        return;
    }
    ++movement_snapshots_;
    if (movement_milestone_reached_)
    {
        ++snapshots_after_milestone_;
    }
}

GoldSrcManualSessionEvents GoldSrcManualSessionLifecycle::Advance(
    std::uint64_t now_ms,
    bool movement_snapshot_integrated) noexcept
{
    GoldSrcManualSessionEvents events;
    if (!can_pump())
    {
        return events;
    }
    if (!global_deadline_suppressed() && now_ms >= deadline_ms_)
    {
        runtime_complete_ = true;
        phase_ = GoldSrcManualSessionPhase::kStopped;
        events.handshake_timed_out = true;
        events.runtime_completed = true;
        return events;
    }
    if (session_established_
        && first_movement_recorded_
        && !movement_milestone_reached_
        && now_ms >= first_movement_ms_
        && now_ms - first_movement_ms_
            >= config_.movement_observation_ms
        && movement_commands_ > 0u
        && movement_snapshots_ >= 3u
        && movement_snapshot_integrated)
    {
        movement_milestone_reached_ = true;
        proof_complete_ = true;
        ++movement_milestone_count_;
        events.movement_milestone_reached = true;
        events.proof_completed = true;
        if (!config_.persistent)
        {
            runtime_complete_ = true;
            phase_ = GoldSrcManualSessionPhase::kStopped;
            events.runtime_completed = true;
            return events;
        }
    }
    if (config_.persistent
        && session_established_
        && config_.heartbeat_interval_ms > 0u
        && now_ms >= next_heartbeat_ms_)
    {
        events.heartbeat_due = true;
        next_heartbeat_ms_ = now_ms
            > std::numeric_limits<std::uint64_t>::max()
                    - config_.heartbeat_interval_ms
            ? std::numeric_limits<std::uint64_t>::max()
            : now_ms + config_.heartbeat_interval_ms;
    }
    return events;
}

bool GoldSrcManualSessionLifecycle::MarkClientDisconnected() noexcept
{
    if (!started_ || !client_admitted_)
    {
        return false;
    }
    const bool established = session_established_;
    if (established)
    {
        ++disconnect_count_;
    }
    client_admitted_ = false;
    session_established_ = false;
    first_movement_recorded_ = false;
    first_movement_ms_ = 0u;
    movement_commands_ = 0u;
    movement_snapshots_ = 0u;
    movement_milestone_reached_ = false;
    phase_ = GoldSrcManualSessionPhase::kWaitingForClient;
    return established;
}

void GoldSrcManualSessionLifecycle::RequestShutdown() noexcept
{
    shutdown_requested_ = true;
    runtime_complete_ = true;
    phase_ = GoldSrcManualSessionPhase::kStopped;
}

bool GoldSrcManualSessionLifecycle::can_pump() const noexcept
{
    return started_ && !runtime_complete_ && !shutdown_requested_;
}

bool GoldSrcManualSessionLifecycle::persistent() const noexcept
{
    return config_.persistent;
}

bool GoldSrcManualSessionLifecycle::movement_milestone_reached() const noexcept
{
    return movement_milestone_reached_;
}

bool GoldSrcManualSessionLifecycle::proof_complete() const noexcept
{
    return proof_complete_;
}

bool GoldSrcManualSessionLifecycle::runtime_complete() const noexcept
{
    return runtime_complete_;
}

bool GoldSrcManualSessionLifecycle::shutdown_requested() const noexcept
{
    return shutdown_requested_;
}

bool GoldSrcManualSessionLifecycle::global_deadline_suppressed() const noexcept
{
    return config_.persistent && session_ever_established_;
}

std::uint64_t
GoldSrcManualSessionLifecycle::clc_moves_after_milestone() const noexcept
{
    return clc_moves_after_milestone_;
}

std::uint64_t
GoldSrcManualSessionLifecycle::snapshots_after_milestone() const noexcept
{
    return snapshots_after_milestone_;
}

std::uint64_t
GoldSrcManualSessionLifecycle::movement_milestone_count() const noexcept
{
    return movement_milestone_count_;
}

std::uint64_t GoldSrcManualSessionLifecycle::disconnect_count() const noexcept
{
    return disconnect_count_;
}

std::uint64_t GoldSrcManualSessionLifecycle::reconnect_count() const noexcept
{
    return reconnect_count_;
}

GoldSrcManualSessionPhase GoldSrcManualSessionLifecycle::phase() const noexcept
{
    return phase_;
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
