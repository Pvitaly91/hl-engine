#include "network/goldsrc_connectionless.h"
#include "network/goldsrc_netchan.h"
#include "network/goldsrc_player_lifecycle.h"
#include "network/goldsrc_pmove.h"
#include "network/goldsrc_signon.h"
#include "network/goldsrc_snapshot.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace
{
using namespace hl::network;

GoldSrcDeltaField IntegerField(const char* name)
{
    GoldSrcDeltaField field;
    field.field_type = kGoldSrcDeltaTypeInteger;
    field.name = name;
    field.field_size = 4u;
    field.significant_bits = 16u;
    field.premultiply = 1.0;
    field.postmultiply = 1.0;
    return field;
}

GoldSrcDeltaTable Table(const char* name, const char* field_name)
{
    GoldSrcDeltaTable table;
    table.name = name;
    table.fields.push_back(IntegerField(field_name));
    return table;
}

GoldSrcDeltaRegistry Registry()
{
    GoldSrcDeltaRegistry registry;
    registry.tables.push_back(Table("event_t", "entindex"));
    registry.tables.push_back(Table("weapon_data_t", "clip"));
    registry.tables.push_back(Table("usercmd_t", "msec"));
    registry.tables.push_back(
        Table("custom_entity_state_t", "modelindex"));
    registry.tables.push_back(
        Table("entity_state_player_t", "origin[0]"));
    registry.tables.push_back(Table("entity_state_t", "modelindex"));
    registry.tables.push_back(Table("clientdata_t", "health"));
    return registry;
}

GoldSrcDecodedDeltaRecord Record(std::uint32_t value)
{
    GoldSrcDecodedDeltaRecord record;
    record.field_count = 1u;
    record.values[0].kind = GoldSrcDeltaValueKind::kUnsignedInteger;
    record.values[0].unsigned_value = value;
    return record;
}

GoldSrcBaselineBundle Baselines()
{
    GoldSrcBaselineBundle baselines;
    baselines.maximum_clients = 2u;
    for (std::uint16_t index = 0u; index <= 3u; ++index)
    {
        GoldSrcEntityBaseline baseline;
        baseline.entity_index = index;
        baseline.kind = index == 0u
            ? GoldSrcBaselineKind::kWorld
            : index <= 2u
                ? GoldSrcBaselineKind::kPlayer
                : GoldSrcBaselineKind::kEntity;
        baseline.model_index = index + 1u;
        baseline.state = Record(index + 1u);
        baselines.entities.push_back(baseline);
    }
    return baselines;
}

GoldSrcSnapshotEntityState Player(
    std::uint16_t entity_index,
    std::uint32_t position)
{
    GoldSrcSnapshotEntityState player;
    player.entity_index = entity_index;
    player.kind = GoldSrcBaselineKind::kPlayer;
    player.state = Record(position);
    return player;
}

GoldSrcPlayerSnapshotInput ReceiverSnapshot(
    std::uint16_t own_entity,
    std::uint32_t own_position,
    std::uint32_t own_health,
    std::optional<GoldSrcSnapshotEntityState> remote)
{
    GoldSrcPlayerSnapshotInput input;
    input.entity = Player(own_entity, own_position);
    input.clientdata.state = Record(own_health);
    if (remote.has_value())
    {
        input.remote_entities.push_back(*remote);
    }
    return input;
}

const GoldSrcSnapshotEntityState* FindEntity(
    const GoldSrcServerFrame& frame,
    std::uint16_t entity_index)
{
    const auto found = std::find_if(
        frame.entities.begin(),
        frame.entities.end(),
        [entity_index](const GoldSrcSnapshotEntityState& entity)
        {
            return entity.entity_index == entity_index;
        });
    return found == frame.entities.end() ? nullptr : &*found;
}

GoldSrcServerFrame Frame(std::uint32_t frame_id, float server_time)
{
    GoldSrcServerFrame frame;
    frame.frame_id = frame_id;
    frame.server_time = server_time;
    frame.clientdata.state = Record(0u);
    return frame;
}

GoldSrcNetchanPacket Packet(
    std::uint32_t sequence,
    std::uint32_t acknowledgement,
    bool reliable_acknowledgement)
{
    GoldSrcNetchanPacket packet{};
    packet.sequence = sequence;
    packet.acknowledgement = acknowledgement;
    packet.reliable_acknowledgement = reliable_acknowledgement;
    packet.raw_sequence = sequence;
    packet.raw_acknowledgement = acknowledgement
        | (reliable_acknowledgement ? kGoldSrcNetchanReliableFlag : 0u);
    return packet;
}

GoldSrcDecodedMoveCommand Move(float forward, float side = 0.0f)
{
    GoldSrcDecodedMoveCommand move;
    move.new_command_count = 1u;
    move.command_count = 1u;
    move.commands[0].msec = 10u;
    move.commands[0].forwardmove = forward;
    move.commands[0].sidemove = side;
    return move;
}

struct AdmissionSlot final
{
    bool connected = false;
    Ipv4Endpoint endpoint{};
    std::uint16_t qport = 0u;
    std::uint64_t generation = 0u;
};

class TwoClientAdmissionFixture final
{
public:
    int Admit(const Ipv4Endpoint& endpoint, std::uint16_t qport)
    {
        for (const AdmissionSlot& slot : slots_)
        {
            if (slot.connected && slot.endpoint == endpoint
                && slot.qport == qport)
            {
                return -2;
            }
        }
        for (std::size_t index = 0u; index < slots_.size(); ++index)
        {
            AdmissionSlot& slot = slots_[index];
            if (!slot.connected)
            {
                slot.connected = true;
                slot.endpoint = endpoint;
                slot.qport = qport;
                ++slot.generation;
                return static_cast<int>(index + 1u);
            }
        }
        return -1;
    }

    bool Disconnect(int slot_number, std::uint64_t generation)
    {
        if (slot_number <= 0
            || static_cast<std::size_t>(slot_number) > slots_.size())
        {
            return false;
        }
        AdmissionSlot& slot = slots_[static_cast<std::size_t>(slot_number - 1)];
        if (!slot.connected || slot.generation != generation)
        {
            return false;
        }
        slot.connected = false;
        slot.endpoint = {};
        slot.qport = 0u;
        return true;
    }

    const AdmissionSlot& slot(int slot_number) const
    {
        return slots_[static_cast<std::size_t>(slot_number - 1)];
    }

private:
    std::array<AdmissionSlot, 2u> slots_{};
};

void TestAdmissionEdictAndGenerationIsolation()
{
    const Ipv4Endpoint endpoint_a =
        *Ipv4Endpoint::Parse("127.0.0.1", 31001u);
    const Ipv4Endpoint endpoint_b =
        *Ipv4Endpoint::Parse("127.0.0.1", 31002u);
    const Ipv4Endpoint endpoint_c =
        *Ipv4Endpoint::Parse("127.0.0.1", 31003u);
    assert(endpoint_a.SameHost(endpoint_b));

    TwoClientAdmissionFixture admission;
    assert(admission.Admit(endpoint_a, 1001u) == 1);
    assert(admission.Admit(endpoint_b, 1002u) == 2);
    assert(admission.Admit(endpoint_a, 1001u) == -2);
    assert(admission.Admit(endpoint_c, 1003u) == -1);

    GoldSrcClientEdictRegistry edicts(2u);
    GoldSrcClientEdictBinding binding_a;
    GoldSrcClientEdictBinding binding_b;
    assert(edicts.Bind(1u, 11u, &binding_a)
        == GoldSrcClientEdictBindingResult::kBound);
    assert(edicts.Bind(2u, 22u, &binding_b)
        == GoldSrcClientEdictBindingResult::kBound);
    assert(binding_a.edict_index == 1u);
    assert(binding_b.edict_index == 2u);
    assert(binding_a.edict_generation != 0u);
    assert(binding_b.edict_generation != 0u);

    const std::uint64_t old_generation = admission.slot(1).generation;
    assert(admission.Disconnect(1, old_generation));
    assert(!admission.Disconnect(1, old_generation));
    assert(admission.slot(2).connected);
    assert(edicts.Disconnect(binding_a)
        == GoldSrcClientEdictBindingResult::kBound);
    assert(edicts.Validate(binding_b)
        == GoldSrcClientEdictBindingResult::kBound);
    assert(admission.Admit(endpoint_c, 1003u) == 1);
    assert(admission.slot(1).generation != old_generation);

    GoldSrcClientEdictBinding rebound_a;
    assert(edicts.Bind(1u, 33u, &rebound_a)
        == GoldSrcClientEdictBindingResult::kBound);
    assert(rebound_a.edict_index == 1u);
    assert(rebound_a.edict_generation != binding_a.edict_generation);
    assert(edicts.Validate(binding_a)
        == GoldSrcClientEdictBindingResult::kStaleSession);
    assert(edicts.Validate(binding_b)
        == GoldSrcClientEdictBindingResult::kBound);
}

void TestNetchanAndFrameAcknowledgementIsolation()
{
    const auto start = GoldSrcNetchanState::Clock::now();
    const Ipv4Endpoint endpoint_a =
        *Ipv4Endpoint::Parse("127.0.0.1", 32001u);
    const Ipv4Endpoint endpoint_b =
        *Ipv4Endpoint::Parse("127.0.0.1", 32002u);
    GoldSrcNetchanState channel_a;
    GoldSrcNetchanState channel_b;
    assert(channel_a.Initialize(
        endpoint_a, static_cast<std::uint16_t>(2001u), 1u, start));
    assert(channel_b.Initialize(
        endpoint_b, static_cast<std::uint16_t>(2002u), 2u, start));
    const std::array<std::uint8_t, 2u> reliable_a{1u, 2u};
    const std::array<std::uint8_t, 2u> reliable_b{3u, 4u};
    assert(channel_a.QueueReliablePayload(
        reliable_a.data(), reliable_a.size(),
        GoldSrcNetchanReliablePayloadKind::kServerInfo)
        == GoldSrcNetchanQueueResult::kQueued);
    assert(channel_b.QueueReliablePayload(
        reliable_b.data(), reliable_b.size(),
        GoldSrcNetchanReliablePayloadKind::kSignonBootstrap)
        == GoldSrcNetchanQueueResult::kQueued);
    GoldSrcNetchanDatagram sent_a;
    GoldSrcNetchanDatagram sent_b;
    assert(channel_a.BuildOutgoingDatagram(&sent_a));
    assert(channel_b.BuildOutgoingDatagram(&sent_b));
    const auto accepted_a = channel_a.ProcessIncomingDatagramDetailed(
        endpoint_a,
        Packet(1u, channel_a.highest_sequence_sent(), true),
        start + std::chrono::milliseconds(1));
    assert(accepted_a.result == GoldSrcNetchanProcessResult::kAccepted);
    assert(!channel_a.reliable_pending());
    assert(channel_b.reliable_pending());
    assert(channel_b.ProcessIncomingDatagram(
        endpoint_a,
        Packet(1u, channel_b.highest_sequence_sent(), true),
        start + std::chrono::milliseconds(1))
        == GoldSrcNetchanProcessResult::kEndpointMismatch);
    assert(channel_b.reliable_pending());

    GoldSrcClientFrameHistory history_a(4u);
    GoldSrcClientFrameHistory history_b(4u);
    assert(history_a.Store(Frame(10u, 1.0f))
        == GoldSrcFrameStoreResult::kStored);
    assert(history_b.Store(Frame(20u, 1.0f))
        == GoldSrcFrameStoreResult::kStored);
    assert(history_a.Acknowledge(10u)
        == GoldSrcFrameAcknowledgeResult::kAcknowledged);
    assert(history_a.last_acknowledged_frame() == 10u);
    assert(!history_b.last_acknowledged_frame().has_value());
    assert(history_b.Acknowledge(20u)
        == GoldSrcFrameAcknowledgeResult::kAcknowledged);
    history_a.Reset();
    assert(history_a.size() == 0u);
    assert(history_b.size() == 1u);
    assert(history_b.last_acknowledged_frame() == 20u);
}

void TestReceiverOwnedSnapshotsAndPlayerOperations()
{
    const GoldSrcDeltaRegistry registry = Registry();
    const GoldSrcBaselineBundle baselines = Baselines();
    const GoldSrcPlayerSnapshotInput receiver_a =
        ReceiverSnapshot(1u, 100u, 91u, Player(2u, 200u));
    const GoldSrcPlayerSnapshotInput receiver_b =
        ReceiverSnapshot(2u, 200u, 72u, Player(1u, 100u));
    const GoldSrcSnapshotBuildResult first_a = BuildGoldSrcFirstSnapshot(
        30u, 1.0f, baselines, registry,
        kGoldSrcMaximumSnapshotBytes, &receiver_a);
    const GoldSrcSnapshotBuildResult first_b = BuildGoldSrcFirstSnapshot(
        40u, 1.0f, baselines, registry,
        kGoldSrcMaximumSnapshotBytes, &receiver_b);
    assert(first_a.ok());
    assert(first_b.ok());
    assert(first_a.bundle.frame.entities.size() == 3u);
    assert(first_b.bundle.frame.entities.size() == 3u);
    assert(FindEntity(first_a.bundle.frame, 1u) != nullptr);
    assert(FindEntity(first_a.bundle.frame, 2u) != nullptr);
    assert(FindEntity(first_b.bundle.frame, 1u) != nullptr);
    assert(FindEntity(first_b.bundle.frame, 2u) != nullptr);
    assert(first_a.bundle.frame.clientdata.state.values[0].unsigned_value == 91u);
    assert(first_b.bundle.frame.clientdata.state.values[0].unsigned_value == 72u);

    GoldSrcClientFrameHistory history_a(8u);
    GoldSrcClientFrameHistory history_b(8u);
    assert(history_a.Store(first_a.bundle.frame)
        == GoldSrcFrameStoreResult::kStored);
    assert(history_b.Store(first_b.bundle.frame)
        == GoldSrcFrameStoreResult::kStored);
    assert(history_a.Acknowledge(30u)
        == GoldSrcFrameAcknowledgeResult::kAcknowledged);
    assert(history_b.Acknowledge(40u)
        == GoldSrcFrameAcknowledgeResult::kAcknowledged);

    const GoldSrcPlayerSnapshotInput moved_a =
        ReceiverSnapshot(1u, 110u, 90u, Player(2u, 220u));
    const GoldSrcPlayerSnapshotInput moved_b =
        ReceiverSnapshot(2u, 220u, 70u, Player(1u, 110u));
    const auto update_a = BuildGoldSrcContinuousSnapshot(
        31u, 1.05f, history_a.last_acknowledged(), baselines, registry,
        kGoldSrcMaximumSnapshotBytes, &moved_a);
    const auto update_b = BuildGoldSrcContinuousSnapshot(
        41u, 1.05f, history_b.last_acknowledged(), baselines, registry,
        kGoldSrcMaximumSnapshotBytes, &moved_b);
    assert(update_a.ok() && update_b.ok());
    assert(update_a.bundle.kind == GoldSrcSnapshotKind::kDelta);
    assert(update_b.bundle.kind == GoldSrcSnapshotKind::kDelta);
    assert(update_a.bundle.base_frame_id == 30u);
    assert(update_b.bundle.base_frame_id == 40u);
    assert(update_a.bundle.entity_diff.updates == 2u);
    assert(update_b.bundle.entity_diff.updates == 2u);

    const GoldSrcPlayerSnapshotInput b_without_a =
        ReceiverSnapshot(2u, 225u, 70u, std::nullopt);
    const auto remove_a = BuildGoldSrcContinuousSnapshot(
        42u, 1.10f, &update_b.bundle.frame, baselines, registry,
        kGoldSrcMaximumSnapshotBytes, &b_without_a);
    assert(remove_a.ok());
    assert(remove_a.bundle.entity_diff.removes == 1u);
    assert(FindEntity(remove_a.bundle.frame, 1u) == nullptr);
    assert(FindEntity(remove_a.bundle.frame, 2u) != nullptr);

    const GoldSrcPlayerSnapshotInput b_with_reconnected_a =
        ReceiverSnapshot(2u, 230u, 69u, Player(1u, 5u));
    const auto readd_a = BuildGoldSrcContinuousSnapshot(
        43u, 1.15f, &remove_a.bundle.frame, baselines, registry,
        kGoldSrcMaximumSnapshotBytes, &b_with_reconnected_a);
    assert(readd_a.ok());
    assert(readd_a.bundle.entity_diff.adds == 1u);
    assert(FindEntity(readd_a.bundle.frame, 1u) != nullptr);

    GoldSrcServerFrame duplicate = Frame(50u, 2.0f);
    duplicate.entities.push_back(Player(2u, 1u));
    assert(ApplyGoldSrcPlayerSnapshot(
        &duplicate, receiver_a, baselines.maximum_clients)
        == GoldSrcPlayerSnapshotApplyStatus::kDuplicateEntity);
}

void TestPacketEntityOmissionAndReentry()
{
    const GoldSrcDeltaRegistry registry = Registry();
    const GoldSrcBaselineBundle baselines = Baselines();

    GoldSrcPlayerSnapshotInput both_present =
        ReceiverSnapshot(1u, 100u, 91u, Player(2u, 200u));
    both_present.weapons.push_back({2u, Record(17u)});
    const auto initial = BuildGoldSrcFirstSnapshot(
        60u, 3.0f, baselines, registry,
        kGoldSrcMaximumSnapshotBytes, &both_present);
    assert(initial.ok());
    assert(FindEntity(initial.bundle.frame, 1u) != nullptr);
    assert(FindEntity(initial.bundle.frame, 2u) != nullptr);

    GoldSrcPlayerSnapshotInput local_omitted = both_present;
    local_omitted.include_local_entity = false;
    local_omitted.entity = {};
    local_omitted.clientdata.state = Record(80u);
    local_omitted.weapons[0].state = Record(16u);
    const auto remove_local = BuildGoldSrcContinuousSnapshot(
        61u, 3.05f, &initial.bundle.frame, baselines, registry,
        kGoldSrcMaximumSnapshotBytes, &local_omitted);
    assert(remove_local.ok());
    assert(remove_local.bundle.kind == GoldSrcSnapshotKind::kDelta);
    assert(remove_local.bundle.entity_diff.removes == 1u);
    assert(FindEntity(remove_local.bundle.frame, 1u) == nullptr);
    assert(FindEntity(remove_local.bundle.frame, 2u) != nullptr);
    assert(remove_local.bundle.frame.clientdata.state.values[0].unsigned_value
        == 80u);
    assert(remove_local.bundle.frame.weapons.size() == 1u);
    assert(remove_local.bundle.frame.weapons[0].state.values[0].unsigned_value
        == 16u);

    GoldSrcPlayerSnapshotInput remote_omitted =
        ReceiverSnapshot(1u, 105u, 79u, std::nullopt);
    remote_omitted.weapons.push_back({2u, Record(15u)});
    const auto swap_visibility = BuildGoldSrcContinuousSnapshot(
        62u, 3.10f, &remove_local.bundle.frame, baselines, registry,
        kGoldSrcMaximumSnapshotBytes, &remote_omitted);
    assert(swap_visibility.ok());
    assert(swap_visibility.bundle.kind == GoldSrcSnapshotKind::kDelta);
    assert(swap_visibility.bundle.entity_diff.adds == 1u);
    assert(swap_visibility.bundle.entity_diff.removes == 1u);
    assert(FindEntity(swap_visibility.bundle.frame, 1u) != nullptr);
    assert(FindEntity(swap_visibility.bundle.frame, 2u) == nullptr);

    GoldSrcPlayerSnapshotInput neither_present = remote_omitted;
    neither_present.include_local_entity = false;
    neither_present.entity = {};
    const auto remove_local_again = BuildGoldSrcContinuousSnapshot(
        63u, 3.15f, &swap_visibility.bundle.frame, baselines, registry,
        kGoldSrcMaximumSnapshotBytes, &neither_present);
    assert(remove_local_again.ok());
    assert(remove_local_again.bundle.entity_diff.removes == 1u);
    assert(FindEntity(remove_local_again.bundle.frame, 1u) == nullptr);
    assert(FindEntity(remove_local_again.bundle.frame, 2u) == nullptr);

    GoldSrcPlayerSnapshotInput both_reentered =
        ReceiverSnapshot(1u, 110u, 78u, Player(2u, 210u));
    both_reentered.weapons.push_back({2u, Record(14u)});
    const auto readd_both = BuildGoldSrcContinuousSnapshot(
        64u, 3.20f, &remove_local_again.bundle.frame, baselines, registry,
        kGoldSrcMaximumSnapshotBytes, &both_reentered);
    assert(readd_both.ok());
    assert(readd_both.bundle.kind == GoldSrcSnapshotKind::kDelta);
    assert(readd_both.bundle.entity_diff.adds == 2u);
    assert(FindEntity(readd_both.bundle.frame, 1u) != nullptr);
    assert(FindEntity(readd_both.bundle.frame, 2u) != nullptr);

    const auto full_omission = BuildGoldSrcFirstSnapshot(
        70u, 4.0f, baselines, registry,
        kGoldSrcMaximumSnapshotBytes, &neither_present);
    assert(full_omission.ok());
    assert(FindEntity(full_omission.bundle.frame, 1u) == nullptr);
    assert(FindEntity(full_omission.bundle.frame, 2u) == nullptr);
    assert(full_omission.bundle.frame.entities.size() == 1u);
    assert(full_omission.bundle.frame.clientdata.state.field_count == 1u);
    assert(full_omission.bundle.frame.weapons.size() == 1u);
}

void TestCommandClockAndSchedulerIsolation()
{
    GoldSrcCommandExecutionState command_a;
    GoldSrcCommandExecutionState command_b;
    const GoldSrcDecodedMoveCommand move_a = Move(100.0f);
    const GoldSrcDecodedMoveCommand move_b = Move(0.0f, 80.0f);
    const auto plan_a = command_a.Plan(move_a, 1u, 1000u);
    const auto plan_b = command_b.Plan(move_b, 1u, 1000u);
    assert(plan_a.ok() && plan_b.ok());
    command_a.CommitObservedMovePacket(1u, true);
    command_b.CommitObservedMovePacket(1u, true);
    command_a.CommitExecutedBatch(plan_a, 1u, 1000u);
    command_b.CommitExecutedBatch(plan_b, 1u, 1000u);
    assert(command_a.command_time_msec() == 10u);
    assert(command_b.command_time_msec() == 10u);
    command_a.RecordRollback();
    assert(command_a.rollback_count() == 1u);
    assert(command_b.rollback_count() == 0u);

    GoldSrcDecodedMoveCommand invalid_a = Move(100.0f);
    invalid_a.commands[0].msec = 0u;
    assert(command_a.Plan(invalid_a, 2u, 1010u).status
        == GoldSrcCommandPlanStatus::kInvalidCommandMsec);
    const auto second_b = command_b.Plan(move_b, 2u, 1010u);
    assert(second_b.ok());
    command_b.CommitObservedMovePacket(2u, true);
    command_b.CommitExecutedBatch(second_b, 2u, 1010u);
    assert(command_a.command_time_msec() == 10u);
    assert(command_b.command_time_msec() == 20u);

    command_a.Reset();
    assert(!command_a.command_clock_epoch_initialized());
    assert(command_b.command_clock_epoch_initialized());
    assert(command_b.command_time_msec() == 20u);

    GoldSrcSnapshotScheduler scheduler_a;
    GoldSrcSnapshotScheduler scheduler_b;
    assert(scheduler_a.Start(1.0, 20.0));
    assert(scheduler_b.Start(1.0, 20.0));
    for (std::uint32_t frame = 1u; frame <= 20u; ++frame)
    {
        const double server_time =
            1.000001 + static_cast<double>(frame) * 0.05;
        assert(scheduler_a.CheckDue(server_time) == GoldSrcSnapshotDueResult::kDue);
        assert(scheduler_b.CheckDue(server_time) == GoldSrcSnapshotDueResult::kDue);
        scheduler_a.RecordGenerated(frame);
        scheduler_a.RecordSent(frame, GoldSrcSnapshotKind::kDelta);
        scheduler_b.RecordGenerated(frame + 100u);
        scheduler_b.RecordSent(frame + 100u, GoldSrcSnapshotKind::kDelta);
    }
    assert(scheduler_a.state().due_snapshots == 20u);
    assert(scheduler_b.state().due_snapshots == 20u);
    scheduler_a.Stop();
    assert(scheduler_a.CheckDue(2.1) == GoldSrcSnapshotDueResult::kDisabled);
    assert(scheduler_b.CheckDue(2.051) == GoldSrcSnapshotDueResult::kDue);
}

void TestBoundedTwoClientOutgoingSaturation()
{
    std::size_t fair_index = 0u;
    std::array<std::uint32_t, 2u> snapshots{};
    std::array<std::uint32_t, 2u> reliable{};
    std::array<std::uint32_t, 2u> last_acknowledged{};
    std::uint64_t coalesced = 0u;
    for (std::uint32_t frame = 1u; frame <= 12000u; ++frame)
    {
        std::vector<GoldSrcOutgoingClientDemand> demands(2u);
        for (std::size_t client = 0u; client < demands.size(); ++client)
        {
            demands[client].active = true;
            demands[client].snapshot_due = true;
            demands[client].empty_acknowledgement_pending = true;
            demands[client].incoming_frontier =
                frame * 8u + static_cast<std::uint32_t>(client);
            demands[client].reliable_pending =
                frame % 97u == static_cast<std::uint32_t>(client);
        }
        const GoldSrcOutgoingScheduleResult schedule =
            BuildGoldSrcBoundedOutgoingSchedule(demands, 8u, fair_index);
        assert(schedule.actions.size() <= 8u);
        assert(schedule.snapshots_deferred == 0u);
        assert(schedule.empty_acknowledgements_coalesced == 2u);
        fair_index = schedule.next_fair_client_index;
        coalesced += schedule.empty_acknowledgements_coalesced;
        for (const GoldSrcOutgoingAction& action : schedule.actions)
        {
            assert(action.acknowledged_frontier
                == demands[action.client_index].incoming_frontier);
            last_acknowledged[action.client_index] =
                action.acknowledged_frontier;
            if (action.kind == GoldSrcOutgoingActionKind::kSnapshot)
            {
                ++snapshots[action.client_index];
            }
            else if (action.kind == GoldSrcOutgoingActionKind::kReliable)
            {
                ++reliable[action.client_index];
            }
        }
    }
    assert(fair_index == 0u);
    assert(snapshots[0] == 12000u && snapshots[1] == 12000u);
    assert(reliable[0] > 0u && reliable[1] > 0u);
    assert(last_acknowledged[0] > 0u && last_acknowledged[1] > 0u);
    assert(coalesced == 24000u);

    const std::vector<GoldSrcOutgoingClientDemand> constrained{
        {true, false, true, true, 100u},
        {true, false, true, true, 200u}};
    const auto first = BuildGoldSrcBoundedOutgoingSchedule(
        constrained, 1u, 0u);
    const auto second = BuildGoldSrcBoundedOutgoingSchedule(
        constrained, 1u, first.next_fair_client_index);
    assert(first.actions.size() == 1u && second.actions.size() == 1u);
    assert(first.actions[0].client_index != second.actions[0].client_index);
    assert(first.snapshots_deferred == 1u);
    assert(second.snapshots_deferred == 1u);
}

void TestRemoteInterpolationContract()
{
    GoldSrcRemoteInterpolationSample previous;
    previous.server_time = 10.0;
    previous.origin = {100.0f, 20.0f, 8.0f};
    previous.velocity = {200.0f, -40.0f, 0.0f};
    previous.animtime = 10.0f;
    previous.effects = kGoldSrcEffectNoInterpolation;
    previous.entity_present = true;
    GoldSrcRemoteInterpolationSample current = previous;
    current.server_time = 10.05;
    current.origin = {110.0f, 18.0f, 8.0f};
    current.animtime = 10.05f;
    current.effects = 0u;
    assert(ValidateGoldSrcRemoteInterpolationSamples(previous, current)
        == GoldSrcRemoteInterpolationStatus::kEligible);

    current.effects = kGoldSrcEffectNoInterpolation;
    assert(ValidateGoldSrcRemoteInterpolationSamples(previous, current)
        == GoldSrcRemoteInterpolationStatus::kPermanentNoInterpolation);
    current.effects = 0u;
    current.animtime = 9.0f;
    assert(ValidateGoldSrcRemoteInterpolationSamples(previous, current)
        == GoldSrcRemoteInterpolationStatus::kNonMonotonicAnimtime);
    current.animtime = 10.05f;
    current.origin[0] = 140.0f;
    assert(ValidateGoldSrcRemoteInterpolationSamples(previous, current)
        == GoldSrcRemoteInterpolationStatus::kIncoherentMotion);
}

void TestDeterministicTwoClientTenMinuteLongRun()
{
    struct Client final
    {
        GoldSrcCommandExecutionState commands;
        std::uint32_t sequence = 0u;
        std::uint64_t pmove_calls = 0u;
        std::uint64_t snapshots = 0u;
        std::uint64_t frame_acknowledgements = 0u;
        std::uint64_t remote_updates = 0u;
    };
    std::array<Client, 2u> clients{};
    std::array<std::uint64_t, 2u> checkpoint_pmove{};
    std::array<std::uint64_t, 2u> checkpoint_snapshots{};
    std::array<std::uint64_t, 2u> checkpoint_acks{};
    std::array<std::uint64_t, 2u> checkpoint_remote{};
    std::size_t fair_index = 0u;
    const std::array<std::uint32_t, 4u> checkpoints{
        60u, 120u, 300u, 600u};
    std::size_t checkpoint_index = 0u;

    for (std::uint32_t tick = 1u; tick <= 12000u; ++tick)
    {
        const std::uint32_t second = tick / 20u;
        std::vector<GoldSrcOutgoingClientDemand> demands(2u);
        for (std::size_t index = 0u; index < clients.size(); ++index)
        {
            Client& client = clients[index];
            const bool idle = index == 0u
                ? second % 90u >= 30u && second % 90u < 35u
                : second % 75u >= 45u && second % 75u < 49u;
            const bool packet_lost = index == 0u
                ? tick % 37u == 0u
                : tick % 43u == 0u;
            if (!idle && !packet_lost
                && tick % (index == 0u ? 2u : 3u) == 0u)
            {
                GoldSrcDecodedMoveCommand move = Move(
                    index == 0u ? 120.0f : -90.0f,
                    index == 0u ? 0.0f : 75.0f);
                move.packet_loss = static_cast<std::uint8_t>(
                    packet_lost ? 1u : 0u);
                if (tick % 101u == 0u)
                {
                    move.backup_command_count = 1u;
                    move.command_count = 2u;
                    move.commands[1] = move.commands[0];
                }
                ++client.sequence;
                const std::uint64_t host_time =
                    static_cast<std::uint64_t>(tick) * 50u;
                const GoldSrcCommandExecutionPlan plan =
                    client.commands.Plan(move, client.sequence, host_time);
                assert(plan.ok());
                client.commands.CommitObservedMovePacket(
                    client.sequence, true);
                client.commands.CommitExecutedBatch(
                    plan, client.sequence, host_time);
                ++client.pmove_calls;
            }
            if (tick == (index == 0u ? 3601u : 4801u))
            {
                ++client.sequence;
                client.commands.CommitObservedMovePacket(
                    client.sequence, true, true);
            }
            demands[index].active = true;
            demands[index].snapshot_due = true;
            demands[index].empty_acknowledgement_pending = true;
            demands[index].reliable_pending =
                tick % (index == 0u ? 997u : 991u) == 0u;
            demands[index].incoming_frontier = client.sequence;
        }
        const GoldSrcOutgoingScheduleResult schedule =
            BuildGoldSrcBoundedOutgoingSchedule(demands, 8u, fair_index);
        fair_index = schedule.next_fair_client_index;
        assert(schedule.snapshots_deferred == 0u);
        for (const GoldSrcOutgoingAction& action : schedule.actions)
        {
            if (action.kind != GoldSrcOutgoingActionKind::kSnapshot)
            {
                continue;
            }
            Client& receiver = clients[action.client_index];
            ++receiver.snapshots;
            ++receiver.remote_updates;
            if (tick % (action.client_index == 0u ? 11u : 13u) != 0u)
            {
                ++receiver.frame_acknowledgements;
            }
        }

        if (checkpoint_index < checkpoints.size()
            && tick == checkpoints[checkpoint_index] * 20u)
        {
            for (std::size_t index = 0u; index < clients.size(); ++index)
            {
                assert(clients[index].pmove_calls > checkpoint_pmove[index]);
                assert(clients[index].snapshots > checkpoint_snapshots[index]);
                assert(clients[index].frame_acknowledgements
                    > checkpoint_acks[index]);
                assert(clients[index].remote_updates
                    > checkpoint_remote[index]);
                checkpoint_pmove[index] = clients[index].pmove_calls;
                checkpoint_snapshots[index] = clients[index].snapshots;
                checkpoint_acks[index] =
                    clients[index].frame_acknowledgements;
                checkpoint_remote[index] = clients[index].remote_updates;
            }
            ++checkpoint_index;
        }
    }
    assert(checkpoint_index == checkpoints.size());
    assert(clients[0].commands.last_executed_move_sequence()
        != clients[1].commands.last_executed_move_sequence());
    assert(clients[0].commands.diagnostics().move_packets_executed > 0u);
    assert(clients[1].commands.diagnostics().move_packets_executed > 0u);
}
} // namespace

int main()
{
    TestAdmissionEdictAndGenerationIsolation();
    TestNetchanAndFrameAcknowledgementIsolation();
    TestReceiverOwnedSnapshotsAndPlayerOperations();
    TestPacketEntityOmissionAndReentry();
    TestCommandClockAndSchedulerIsolation();
    TestBoundedTwoClientOutgoingSaturation();
    TestRemoteInterpolationContract();
    TestDeterministicTwoClientTenMinuteLongRun();
    return 0;
}
