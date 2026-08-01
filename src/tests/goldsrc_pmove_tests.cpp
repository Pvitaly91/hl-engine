#include "game_api/goldsrc_pmove_runtime.h"
#include "network/goldsrc_netchan.h"
#include "network/goldsrc_pmove.h"
#include "network/goldsrc_snapshot.h"
#include "world_bootstrap.h"

#include <cassert>
#include <cstdint>

#pragma warning(push, 0)
#include "extdll.h"
#include "in_buttons.h"
#include "pm_defs.h"
#pragma warning(pop)

namespace
{
int g_pm_init_calls = 0;
int g_pm_move_calls = 0;
int g_cmd_start_calls = 0;
int g_cmd_end_calls = 0;
bool g_emit_invalid_output = false;

void MockPmInit(playermove_t*)
{
    ++g_pm_init_calls;
}

void MockPmMove(playermove_t* context, int server)
{
    assert(context != nullptr);
    assert(server == TRUE);
    ++g_pm_move_calls;
    if (g_emit_invalid_output)
    {
        context->velocity[0] = 1000000.0f;
        return;
    }

    const float seconds = static_cast<float>(context->cmd.msec) / 1000.0f;
    float end[3] = {
        context->origin[0] + context->cmd.forwardmove * seconds,
        context->origin[1] + context->cmd.sidemove * seconds,
        context->origin[2],
    };
    const pmtrace_t trace =
        context->PM_PlayerTrace(context->origin, end, PM_NORMAL, -1);
    float previous[3] = {
        context->origin[0],
        context->origin[1],
        context->origin[2],
    };
    for (int axis = 0; axis < 3; ++axis)
    {
        context->origin[axis] = trace.endpos[axis];
        context->velocity[axis] =
            (trace.endpos[axis] - previous[axis])
            / (seconds > 0.0f ? seconds : 1.0f);
    }
    context->usehull =
        (context->cmd.buttons & IN_DUCK) != 0 ? 1 : 0;
    context->onground = 0;
}

void MockCmdStart(const edict_t*, const usercmd_t*, unsigned int)
{
    ++g_cmd_start_calls;
}

void MockCmdEnd(const edict_t*)
{
    ++g_cmd_end_calls;
}

hl::game_api::detail::WorldModelContext CollisionFixture()
{
    using namespace hl::game_api::detail;
    WorldModelContext world;
    world.model_path = "maps/pmove_fixture.bsp";
    world.bsp_loaded = true;
    world.prepared = true;
    world.collision_loaded = true;
    BspCollisionPlane plane;
    plane.normal = Vector(-1.0f, 0.0f, 0.0f);
    plane.distance = -64.0f;
    plane.type = 0;
    world.collision_planes.push_back(plane);
    BspCollisionClipnode node;
    node.plane_index = 0;
    node.children = {CONTENTS_EMPTY, CONTENTS_SOLID};
    world.collision_clipnodes.push_back(node);
    world.point_hull_nodes.push_back(node);
    BspInlineModelBounds model;
    model.valid = true;
    model.mins = Vector(-256.0f, -256.0f, -256.0f);
    model.maxs = Vector(256.0f, 256.0f, 256.0f);
    model.headnodes = {0, 0, 0, 0};
    world.inline_models.push_back(model);
    return world;
}

hl::network::GoldSrcDecodedUserCommand Command(
    std::uint8_t msec,
    float forward,
    float side = 0.0f,
    std::uint16_t buttons = 0u)
{
    hl::network::GoldSrcDecodedUserCommand command;
    command.msec = msec;
    command.forwardmove = forward;
    command.sidemove = side;
    command.buttons = buttons;
    return command;
}

hl::network::GoldSrcDecodedMoveCommand Move(
    std::uint8_t backups,
    std::uint8_t fresh)
{
    hl::network::GoldSrcDecodedMoveCommand move;
    move.backup_command_count = backups;
    move.new_command_count = fresh;
    move.command_count = static_cast<std::size_t>(backups + fresh);
    return move;
}
} // namespace

int main()
{
    using namespace hl::network;
    using namespace hl::game_api::detail;

    {
        GoldSrcManualSessionConfig config;
        config.persistent = false;
        config.handshake_timeout_ms = 300000u;
        config.movement_observation_ms = 60000u;
        GoldSrcManualSessionLifecycle bounded(config);
        bounded.Start(0u);
        assert(!bounded.persistent());
        assert(!bounded.MarkClientAdmitted());
        assert(bounded.MarkSessionEstablished(10u));
        bounded.RecordClcMove(1000u);
        bounded.RecordMovementSnapshot();
        bounded.RecordMovementSnapshot();
        bounded.RecordMovementSnapshot();
        assert(!bounded.Advance(60999u, true).movement_milestone_reached);
        const auto completed = bounded.Advance(61000u, true);
        assert(completed.movement_milestone_reached);
        assert(completed.proof_completed);
        assert(completed.runtime_completed);
        assert(bounded.movement_milestone_reached());
        assert(bounded.proof_complete());
        assert(bounded.runtime_complete());
        assert(!bounded.can_pump());
    }

    {
        GoldSrcManualSessionConfig config;
        config.persistent = true;
        config.handshake_timeout_ms = 300000u;
        config.movement_observation_ms = 60000u;
        config.heartbeat_interval_ms = 30000u;
        GoldSrcManualSessionLifecycle persistent(config);
        persistent.Start(0u);
        assert(persistent.persistent());
        assert(!persistent.MarkClientAdmitted());
        assert(persistent.MarkSessionEstablished(10u));
        persistent.RecordClcMove(1000u);
        persistent.RecordMovementSnapshot();
        persistent.RecordMovementSnapshot();
        persistent.RecordMovementSnapshot();
        const auto milestone = persistent.Advance(61000u, true);
        assert(milestone.movement_milestone_reached);
        assert(milestone.proof_completed);
        assert(!milestone.runtime_completed);
        assert(milestone.heartbeat_due);
        assert(persistent.can_pump());
        assert(persistent.global_deadline_suppressed());

        persistent.RecordClcMove(61001u);
        persistent.RecordMovementSnapshot();
        const auto after_deadline = persistent.Advance(600000u, true);
        assert(!after_deadline.handshake_timed_out);
        assert(!after_deadline.runtime_completed);
        assert(after_deadline.heartbeat_due);
        assert(!persistent.Advance(600000u, true).heartbeat_due);
        assert(persistent.clc_moves_after_milestone() == 1u);
        assert(persistent.snapshots_after_milestone() == 1u);
        assert(persistent.can_pump());

        assert(persistent.MarkClientDisconnected());
        assert(persistent.disconnect_count() == 1u);
        assert(persistent.global_deadline_suppressed());
        assert(persistent.MarkClientAdmitted());
        assert(persistent.reconnect_count() == 1u);
        assert(persistent.MarkSessionEstablished(600010u));
        persistent.RecordClcMove(600020u);
        persistent.RecordMovementSnapshot();
        persistent.RecordMovementSnapshot();
        persistent.RecordMovementSnapshot();
        assert(
            persistent.Advance(660020u, true)
                .movement_milestone_reached);
        assert(persistent.movement_milestone_count() == 2u);
        assert(persistent.can_pump());

        persistent.RequestShutdown();
        assert(persistent.shutdown_requested());
        assert(persistent.runtime_complete());
        assert(!persistent.can_pump());
        assert(
            persistent.phase()
            == GoldSrcManualSessionPhase::kStopped);
    }

    {
        GoldSrcManualSessionConfig config;
        config.persistent = true;
        config.handshake_timeout_ms = 300000u;
        GoldSrcManualSessionLifecycle unconnected(config);
        unconnected.Start(0u);
        const auto timeout = unconnected.Advance(300000u, false);
        assert(timeout.handshake_timed_out);
        assert(timeout.runtime_completed);
        assert(!unconnected.global_deadline_suppressed());
        assert(!unconnected.can_pump());
    }

    {
        const auto split = SplitGoldSrcPmoveCommand(255u);
        assert(split.valid);
        assert(split.count == 6u);
        assert(split.total_msec == 255u);
        for (std::size_t index = 0; index < split.count; ++index)
        {
            assert(split.msec[index] > 0u);
            assert(split.msec[index] <= kGoldSrcPmoveSplitThresholdMsec);
        }
        assert(!SplitGoldSrcPmoveCommand(0u).valid);
    }

    {
        GoldSrcCommandExecutionState state;
        auto first = Move(2u, 1u);
        first.commands[0] = Command(20u, 80.0f);
        first.commands[1] = Command(20u, 90.0f);
        first.commands[2] = Command(20u, 100.0f);
        const auto plan = state.Plan(first, 10u, 1000u);
        assert(plan.ok());
        assert(plan.command_count == 1u);
        assert(plan.commands[0].command.forwardmove == 100.0f);
        assert(plan.duplicate_backups_suppressed == 2u);
        assert(!state.initialized());
        state.CommitObservedMovePacket(10u, true);
        state.CommitExecutedBatch(plan, 10u, 1000u);
        assert(state.last_observed_packet_sequence() == 10u);
        assert(state.last_validated_move_sequence() == 10u);
        assert(state.last_executed_move_sequence() == 10u);
        assert(state.command_clock_epoch_initialized());
        assert(state.host_epoch_msec() == 1000u);

        auto recovered = Move(2u, 1u);
        recovered.commands[0] = Command(20u, 100.0f);
        recovered.commands[1] = Command(20u, 110.0f);
        recovered.commands[2] = Command(20u, 120.0f);
        const auto recovery = state.Plan(recovered, 12u, 1020u);
        assert(recovery.ok());
        assert(recovery.recovered_backups == 1u);
        assert(recovery.duplicate_backups_suppressed == 1u);
        assert(recovery.commands[0].recovered_backup);
        assert(!recovery.commands[1].recovered_backup);
        assert(recovery.commands[0].command.forwardmove == 110.0f);
        assert(recovery.commands[1].command.forwardmove == 120.0f);
        state.CommitObservedMovePacket(12u, true);
        state.CommitExecutedBatch(recovery, 12u, 1020u);
        assert(state.diagnostics().raw_netchan_sequence_gaps == 1u);
        assert(state.diagnostics().raw_netchan_gap_move_replays == 0u);
        assert(state.diagnostics().synthetic_command_replays == 0u);

        auto invalid = Move(0u, 1u);
        invalid.commands[0] = Command(0u, 100.0f);
        assert(
            state.Plan(invalid, 13u, 1000u).status
            == GoldSrcCommandPlanStatus::kInvalidCommandMsec);
        assert(
            state.Plan(recovered, 12u, 1000u).status
            == GoldSrcCommandPlanStatus::kStalePacket);
        assert(
            state.Plan(recovered, 11u, 1000u).status
            == GoldSrcCommandPlanStatus::kOutOfOrderPacket);
    }

    {
        GoldSrcCommandExecutionState state;
        const auto excessive_backups = Move(63u, 0u);
        assert(
            state.Plan(excessive_backups, 1u, 1000u).status
            == GoldSrcCommandPlanStatus::kExcessiveBackupCount);
        const auto excessive_new = Move(0u, 63u);
        assert(
            state.Plan(excessive_new, 1u, 1000u).status
            == GoldSrcCommandPlanStatus::kExcessiveNewCount);

        auto packet_time_overflow = Move(0u, 5u);
        for (std::size_t index = 0;
             index < packet_time_overflow.command_count;
             ++index)
        {
            packet_time_overflow.commands[index] =
                Command(255u, 100.0f);
        }
        assert(
            state.Plan(packet_time_overflow, 1u, 10000u).status
            == GoldSrcCommandPlanStatus::kCommandTimeBudgetExceeded);

        auto command_lead = Move(0u, 1u);
        command_lead.commands[0] = Command(251u, 100.0f);
        assert(
            state.Plan(command_lead, 1u, 0u).status
            == GoldSrcCommandPlanStatus::kTemporarilyAhead);
        assert(!state.initialized());
    }

    {
        GoldSrcCommandExecutionState longrun;
        std::uint32_t sequence = 1u;
        std::uint64_t host_time = 10000u;
        bool executed_after_60 = false;
        bool executed_after_120 = false;
        bool executed_after_300 = false;
        bool executed_after_600 = false;
        for (std::uint32_t index = 0u; index < 60000u; ++index)
        {
            if (index != 0u && index % 1000u == 0u)
            {
                sequence = NextGoldSrcNetchanSequence(sequence);
            }
            auto move = Move(0u, 1u);
            move.commands[0] = Command(10u, 200.0f);
            const auto plan = longrun.Plan(move, sequence, host_time);
            assert(plan.ok());
            assert(plan.command_count == 1u);
            assert(plan.replayed_last_commands == 0u);
            longrun.CommitObservedMovePacket(sequence, true);
            longrun.CommitExecutedBatch(plan, sequence, host_time);
            const std::uint32_t elapsed_seconds = (index + 1u) / 100u;
            executed_after_60 = executed_after_60 || elapsed_seconds >= 60u;
            executed_after_120 = executed_after_120 || elapsed_seconds >= 120u;
            executed_after_300 = executed_after_300 || elapsed_seconds >= 300u;
            executed_after_600 = executed_after_600 || elapsed_seconds >= 600u;
            host_time += 10u;
            sequence = NextGoldSrcNetchanSequence(sequence);
        }
        assert(executed_after_60);
        assert(executed_after_120);
        assert(executed_after_300);
        assert(executed_after_600);
        assert(longrun.command_time_msec() == 600000u);
        assert(longrun.diagnostics().commands_executed == 60000u);
        assert(longrun.diagnostics().raw_netchan_sequence_gaps > 0u);
        assert(longrun.diagnostics().raw_netchan_gap_move_replays == 0u);
        assert(longrun.diagnostics().synthetic_command_replays == 0u);
        assert(longrun.diagnostics().command_time_budget_rejections == 0u);
        assert(longrun.diagnostics().command_clock_resynchronizations == 0u);
    }

    {
        GoldSrcCommandExecutionState high_fps;
        std::uint32_t sequence = 1u;
        std::uint64_t host_time = 5000u;
        std::uint64_t elapsed = 0u;
        std::size_t command_index = 0u;
        while (elapsed < 600000u)
        {
            const std::uint8_t msec =
                command_index % 2u == 0u ? 6u : 7u;
            auto move = Move(0u, 1u);
            move.commands[0] = Command(msec, 180.0f);
            const auto plan = high_fps.Plan(move, sequence, host_time);
            assert(plan.ok());
            high_fps.CommitObservedMovePacket(sequence, true);
            high_fps.CommitExecutedBatch(plan, sequence, host_time);
            elapsed += msec;
            host_time += msec;
            sequence = NextGoldSrcNetchanSequence(sequence);
            ++command_index;
        }
        assert(high_fps.diagnostics().command_time_budget_rejections == 0u);
        assert(high_fps.command_time_msec() == elapsed);
    }

    {
        GoldSrcCommandExecutionState recovery;
        auto first = Move(0u, 1u);
        first.commands[0] = Command(10u, 100.0f);
        const auto first_plan = recovery.Plan(first, 1u, 1000u);
        assert(first_plan.ok());
        recovery.CommitObservedMovePacket(1u, true);
        recovery.CommitExecutedBatch(first_plan, 1u, 1000u);

        auto ahead = Move(0u, 1u);
        ahead.commands[0] = Command(255u, 110.0f);
        const auto ahead_plan = recovery.Plan(ahead, 2u, 1000u);
        assert(ahead_plan.status == GoldSrcCommandPlanStatus::kTemporarilyAhead);
        assert(recovery.last_observed_packet_sequence() == 1u);
        assert(recovery.last_executed_move_sequence() == 1u);
        recovery.CommitObservedMovePacket(2u, true, true);
        assert(recovery.last_observed_packet_sequence() == 2u);
        assert(recovery.last_executed_move_sequence() == 1u);

        auto catchup = Move(2u, 1u);
        catchup.commands[0] = first.commands[0];
        catchup.commands[1] = ahead.commands[0];
        catchup.commands[2] = Command(10u, 120.0f);
        const auto catchup_plan = recovery.Plan(catchup, 3u, 1300u);
        assert(catchup_plan.ok());
        assert(catchup_plan.recovered_backups == 1u);
        assert(catchup_plan.replayed_last_commands == 0u);
        recovery.CommitObservedMovePacket(3u, true);
        recovery.CommitExecutedBatch(catchup_plan, 3u, 1300u);
        assert(recovery.diagnostics().command_clock_recoveries == 1u);
        assert(recovery.last_executed_move_sequence() == 3u);

        const std::uint64_t resume_host_time = 31300u;
        auto resumed = Move(0u, 1u);
        resumed.commands[0] = Command(10u, 130.0f);
        const auto resumed_plan = recovery.Plan(
            resumed,
            100u,
            resume_host_time);
        assert(resumed_plan.ok());
        assert(resumed_plan.command_count == 1u);
        assert(resumed_plan.replayed_last_commands == 0u);
        recovery.CommitObservedMovePacket(100u, true);
        recovery.CommitExecutedBatch(
            resumed_plan,
            100u,
            resume_host_time);
        assert(recovery.diagnostics().raw_netchan_gap_move_replays == 0u);
    }

    {
        GoldSrcCommandExecutionState wrapped;
        const std::uint32_t first_sequence =
            kGoldSrcNetchanSequenceMask - 1u;
        auto move = Move(0u, 1u);
        move.commands[0] = Command(10u, 50.0f);
        auto plan = wrapped.Plan(move, first_sequence, 1000u);
        assert(plan.ok());
        wrapped.CommitObservedMovePacket(first_sequence, true);
        wrapped.CommitExecutedBatch(plan, first_sequence, 1000u);
        const std::uint32_t second_sequence =
            NextGoldSrcNetchanSequence(first_sequence);
        plan = wrapped.Plan(move, second_sequence, 1010u);
        assert(plan.ok());
        wrapped.CommitObservedMovePacket(second_sequence, true);
        wrapped.CommitExecutedBatch(plan, second_sequence, 1010u);
        const std::uint32_t wrapped_sequence =
            NextGoldSrcNetchanSequence(second_sequence);
        plan = wrapped.Plan(move, wrapped_sequence, 1020u);
        assert(plan.ok());
        wrapped.CommitObservedMovePacket(wrapped_sequence, true);
        wrapped.CommitExecutedBatch(plan, wrapped_sequence, 1020u);
        assert(wrapped.last_executed_move_sequence() == wrapped_sequence);
    }

    {
        GoldSrcSnapshotScheduler scheduler;
        assert(scheduler.Start(0.0, 20.0));
        assert(scheduler.CheckDue(0.05) == GoldSrcSnapshotDueResult::kDue);
        assert(scheduler.CheckDue(0.55) == GoldSrcSnapshotDueResult::kDue);
        assert(scheduler.CheckDue(0.5501) == GoldSrcSnapshotDueResult::kNotDue);
        double now = 0.55;
        for (int index = 0; index < 60000; ++index)
        {
            now += index % 3 == 0 ? 0.007 : index % 3 == 1 ? 0.013 : 0.010;
            scheduler.CheckDue(now);
        }
        assert(now >= 600.0);
        assert(scheduler.state().snapshot_burst_count == 0u);
        assert(scheduler.state().skipped_snapshots >= 9u);
        assert(scheduler.state().maximum_due_interval_seconds >= 0.05);
        assert(scheduler.state().due_interval_samples > 1000u);
        assert(scheduler.state().median_due_interval_msec >= 49u);
        assert(scheduler.state().median_due_interval_msec <= 51u);
        assert(scheduler.state().p95_due_interval_msec >= 49u);
        assert(scheduler.state().p95_due_interval_msec <= 60u);
    }

    {
        GoldSrcSnapshotScheduler delayed_scheduler;
        assert(delayed_scheduler.Start(0.0, 20.0));
        assert(delayed_scheduler.CheckDue(0.099)
            == GoldSrcSnapshotDueResult::kDue);
        assert(delayed_scheduler.CheckDue(0.100)
            == GoldSrcSnapshotDueResult::kNotDue);
        assert(delayed_scheduler.CheckDue(0.149)
            == GoldSrcSnapshotDueResult::kNotDue);
        assert(delayed_scheduler.CheckDue(0.151)
            == GoldSrcSnapshotDueResult::kDue);
        assert(delayed_scheduler.state().skipped_snapshots == 1u);
        assert(delayed_scheduler.state().snapshot_burst_count == 0u);
    }

    GoldSrcPmoveRuntime runtime;
    const auto fixture = CollisionFixture();
    assert(runtime.InitializeWorld(fixture));
    GoldSrcPmoveGameDllCallbacks callbacks;
    callbacks.pm_init = &MockPmInit;
    callbacks.pm_move = &MockPmMove;
    callbacks.cmd_start = &MockCmdStart;
    callbacks.cmd_end = &MockCmdEnd;
    GoldSrcMovevarsConfig movevars;
    movevars.maximum_velocity = 2000.0f;
    assert(runtime.InitializeGameDll(callbacks, ".", movevars));

    edict_t world{};
    world.free = FALSE;
    edict_t player{};
    player.free = FALSE;
    player.v.movetype = MOVETYPE_WALK;
    player.v.health = 100.0f;
    player.v.gravity = 1.0f;
    player.v.friction = 1.0f;
    player.v.maxspeed = 320.0f;
    player.v.flags = FL_ONGROUND;
    player.v.origin = Vector(0.0f, 0.0f, 0.0f);
    player.v.velocity = Vector(0.0f, 0.0f, 0.0f);
    player.v.v_angle = Vector(0.0f, 0.0f, 0.0f);
    player.v.punchangle = Vector(0.0f, 0.0f, 0.0f);
    player.v.basevelocity = Vector(0.0f, 0.0f, 0.0f);
    player.v.movedir = Vector(0.0f, 0.0f, 0.0f);
    player.v.view_ofs = Vector(0.0f, 0.0f, 28.0f);

    auto first_move = Move(0u, 1u);
    first_move.commands[0] = Command(50u, 400.0f);
    const auto first_result =
        runtime.Execute(1u, first_move, 1u, 1000u, &player, &world);
    assert(first_result.ok());
    assert(first_result.commands_executed == 1u);
    assert(first_result.authoritative_state_changed);
    assert(player.v.origin.x > 0.0f);
    assert(player.v.origin.x < 64.1f);
    assert(g_pm_init_calls == 1);
    assert(g_pm_move_calls == 1);
    assert(g_cmd_start_calls == 1);
    assert(g_cmd_end_calls == 1);

    const auto duplicate =
        runtime.Execute(1u, first_move, 1u, 1000u, &player, &world);
    assert(!duplicate.ok());
    assert(
        duplicate.command_status
        == GoldSrcCommandPlanStatus::kStalePacket);

    auto recovered = Move(2u, 1u);
    recovered.commands[0] = first_move.commands[0];
    recovered.commands[1] = Command(20u, 300.0f);
    recovered.commands[2] = Command(20u, 200.0f);
    const auto recovered_result =
        runtime.Execute(1u, recovered, 3u, 1100u, &player, &world);
    assert(recovered_result.ok());
    assert(recovered_result.commands_executed == 2u);
    assert(recovered_result.backups_replayed == 1u);

    const Vector before_invalid = player.v.origin;
    auto invalid_output = Move(0u, 1u);
    invalid_output.commands[0] = Command(10u, 100.0f);
    g_emit_invalid_output = true;
    const auto rejected =
        runtime.Execute(1u, invalid_output, 4u, 1200u, &player, &world);
    g_emit_invalid_output = false;
    assert(!rejected.ok());
    assert(rejected.status == GoldSrcPmoveExecutionStatus::kOutputInvalid);
    assert(player.v.origin == before_invalid);
    const auto retry =
        runtime.Execute(1u, invalid_output, 4u, 1200u, &player, &world);
    assert(retry.ok());

    for (std::uint32_t index = 0u; index < 2000u; ++index)
    {
        auto continuity = Move(0u, 1u);
        continuity.commands[0] = Command(
            10u,
            index % 2u == 0u ? 200.0f : -200.0f);
        const auto continuity_result = runtime.Execute(
            1u,
            continuity,
            5u + index,
            1210u + static_cast<std::uint64_t>(index) * 10u,
            &player,
            &world);
        assert(continuity_result.ok());
    }
    const auto* continuity_state = runtime.CommandState(1u);
    assert(continuity_state != nullptr);
    assert(continuity_state->diagnostics().commands_executed >= 2004u);
    assert(continuity_state->diagnostics().synthetic_command_replays == 0u);

    runtime.ResetClient(1u);
    assert(!runtime.movement_executed(1u));
    assert(!runtime.movement_executed(2u));

    auto duck = Move(0u, 1u);
    duck.commands[0] = Command(10u, 0.0f, 0.0f, IN_DUCK);
    const auto second_slot =
        runtime.Execute(2u, duck, 1u, 1000u, &player, &world);
    assert(second_slot.ok());
    assert(second_slot.ducked);
    assert(runtime.movement_executed(2u));
    assert(runtime.diagnostics().pm_init_calls == 1u);
    assert(runtime.diagnostics().movement_rollbacks == 1u);
    assert(runtime.implemented_service_callback_count() == 25u);
    return 0;
}
