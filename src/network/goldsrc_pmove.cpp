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

bool CommandsEqual(
    const GoldSrcDecodedUserCommand& left,
    const GoldSrcDecodedUserCommand& right) noexcept
{
    return left.lerp_msec == right.lerp_msec
        && left.msec == right.msec
        && left.viewangles == right.viewangles
        && left.forwardmove == right.forwardmove
        && left.sidemove == right.sidemove
        && left.upmove == right.upmove
        && left.lightlevel == right.lightlevel
        && left.buttons == right.buttons
        && left.impulse == right.impulse
        && left.impact_index == right.impact_index
        && left.impact_position == right.impact_position;
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
    case GoldSrcCommandPlanStatus::kTemporarilyAhead:
        return "temporarily_ahead";
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
    plan.host_time_msec = host_time_msec;
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
    else if (observed_initialized_
        && packet_sequence == last_observed_packet_sequence_)
    {
        plan.status = GoldSrcCommandPlanStatus::kStalePacket;
        diagnostics_.duplicate_commands_suppressed += fresh + backup;
    }
    else if (observed_initialized_
        && !IsGoldSrcNetchanSequenceNewer(
            packet_sequence,
            last_observed_packet_sequence_))
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

    if (observed_initialized_)
    {
        plan.raw_netchan_sequence_distance = GoldSrcNetchanSequenceDistance(
            packet_sequence,
            last_observed_packet_sequence_);
        if (plan.raw_netchan_sequence_distance == 0u)
        {
            plan.status = GoldSrcCommandPlanStatus::kOutOfOrderPacket;
            ++diagnostics_.malformed_commands_rejected;
            return plan;
        }
        if (plan.raw_netchan_sequence_distance > 1u)
        {
            ++diagnostics_.raw_netchan_sequence_gaps;
        }
    }

    std::size_t explicit_recovery_begin = backup;
    if (has_last_command_)
    {
        for (std::size_t backup_index = 0u;
             backup_index < backup;
             ++backup_index)
        {
            if (CommandsEqual(
                    move.commands[backup_index],
                    last_command_))
            {
                explicit_recovery_begin = backup_index + 1u;
                plan.suppressed_backup_matched_last_command = true;
                break;
            }
        }
    }
    const std::size_t explicit_recovery_count =
        explicit_recovery_begin < backup
        ? backup - explicit_recovery_begin
        : 0u;
    for (std::size_t command_index = explicit_recovery_begin;
         command_index < backup;
         ++command_index)
    {
        if (!Append(
                &plan,
                move.commands[command_index],
                packet_sequence,
                true,
                false))
        {
            plan.status = GoldSrcCommandPlanStatus::kPlanCapacityExceeded;
            ++diagnostics_.malformed_commands_rejected;
            return plan;
        }
        ++plan.recovered_backups;
    }
    plan.duplicate_backups_suppressed = backup - explicit_recovery_count;
    diagnostics_.duplicate_commands_suppressed +=
        plan.duplicate_backups_suppressed;

    for (std::size_t command_index = backup;
         command_index < backup + fresh;
         ++command_index)
    {
        if (!Append(
                &plan,
                move.commands[command_index],
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
            - plan.total_command_msec)
    {
        plan.status = GoldSrcCommandPlanStatus::kCommandTimeOverflow;
        ++diagnostics_.malformed_commands_rejected;
        return plan;
    }
    plan.command_elapsed_msec = command_time_msec_;
    plan.proposed_command_elapsed_msec =
        command_time_msec_ + plan.total_command_msec;
    plan.establishes_command_clock_epoch =
        !command_clock_epoch_initialized_;
    if (command_clock_epoch_initialized_)
    {
        if (host_time_msec < host_epoch_msec_)
        {
            plan.status = GoldSrcCommandPlanStatus::kCommandTimeOverflow;
            ++diagnostics_.malformed_commands_rejected;
            return plan;
        }
        plan.host_elapsed_msec = host_time_msec - host_epoch_msec_;
    }
    if (plan.proposed_command_elapsed_msec
        > plan.host_elapsed_msec + kGoldSrcMaximumCommandLeadMsec)
    {
        plan.status = GoldSrcCommandPlanStatus::kTemporarilyAhead;
        ++diagnostics_.move_packets_temporarily_rejected;
        ++diagnostics_.command_time_budget_rejections;
        return plan;
    }
    return plan;
}

void GoldSrcCommandExecutionState::CommitObservedMovePacket(
    std::uint32_t packet_sequence,
    bool structurally_valid,
    bool temporarily_ahead) noexcept
{
    if (!observed_initialized_
        || IsGoldSrcNetchanSequenceNewer(
            packet_sequence,
            last_observed_packet_sequence_))
    {
        observed_initialized_ = true;
        last_observed_packet_sequence_ = packet_sequence;
        ++diagnostics_.move_packets_observed;
    }
    if (structurally_valid
        && (!validated_initialized_
            || IsGoldSrcNetchanSequenceNewer(
                packet_sequence,
                last_validated_move_sequence_)))
    {
        validated_initialized_ = true;
        last_validated_move_sequence_ = packet_sequence;
        ++diagnostics_.move_packets_validated;
    }
    if (phase_ == GoldSrcMovementPhase::kPlayerSpawnedAwaitingMovement)
    {
        phase_ = GoldSrcMovementPhase::kMovementReady;
    }
    command_clock_wait_pending_ =
        command_clock_wait_pending_ || temporarily_ahead;
}

void GoldSrcCommandExecutionState::CommitExecutedBatch(
    const GoldSrcCommandExecutionPlan& plan,
    std::uint32_t packet_sequence,
    std::uint64_t host_time_msec) noexcept
{
    if (!plan.ok() || plan.command_count == 0u)
    {
        return;
    }
    if (!command_clock_epoch_initialized_)
    {
        command_clock_epoch_initialized_ = true;
        host_epoch_msec_ = host_time_msec;
        command_time_msec_ = 0u;
    }
    if (last_execution_host_time_msec_ != 0u
        && host_time_msec >= last_execution_host_time_msec_)
    {
        diagnostics_.maximum_move_execution_gap_msec = (std::max)(
            diagnostics_.maximum_move_execution_gap_msec,
            host_time_msec - last_execution_host_time_msec_);
    }
    last_execution_host_time_msec_ = host_time_msec;
    for (std::size_t index = 0u; index < plan.command_count; ++index)
    {
        const GoldSrcPlannedUserCommand& command = plan.commands[index];
        has_last_command_ = true;
        last_command_ = command.command;
        command_time_msec_ += command.command.msec;
        ++diagnostics_.commands_executed;
        if (command.recovered_backup)
        {
            ++diagnostics_.backup_commands_replayed;
        }
    }
    executed_initialized_ = true;
    last_executed_move_sequence_ = packet_sequence;
    ++diagnostics_.move_packets_executed;
    if (command_clock_wait_pending_)
    {
        command_clock_wait_pending_ = false;
        ++diagnostics_.command_clock_recoveries;
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
    return observed_initialized_;
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
    return last_executed_move_sequence_;
}

std::uint32_t
GoldSrcCommandExecutionState::last_observed_packet_sequence() const noexcept
{
    return last_observed_packet_sequence_;
}

std::uint32_t
GoldSrcCommandExecutionState::last_validated_move_sequence() const noexcept
{
    return last_validated_move_sequence_;
}

std::uint32_t
GoldSrcCommandExecutionState::last_executed_move_sequence() const noexcept
{
    return last_executed_move_sequence_;
}

std::uint64_t GoldSrcCommandExecutionState::command_time_msec() const noexcept
{
    return command_time_msec_;
}

std::uint64_t GoldSrcCommandExecutionState::host_epoch_msec() const noexcept
{
    return host_epoch_msec_;
}

bool GoldSrcCommandExecutionState::command_clock_epoch_initialized() const noexcept
{
    return command_clock_epoch_initialized_;
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
