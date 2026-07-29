#include "network/goldsrc_player_lifecycle.h"
#include "network/goldsrc_netchan.h"
#include "network/goldsrc_snapshot.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

namespace
{
std::vector<std::uint8_t> Command(std::string_view text)
{
    std::vector<std::uint8_t> bytes = {
        hl::network::kGoldSrcClientNopOpcode,
        hl::network::kGoldSrcClientStringCommandOpcode,
    };
    bytes.insert(bytes.end(), text.begin(), text.end());
    bytes.push_back(0u);
    bytes.push_back(hl::network::kGoldSrcClientNopOpcode);
    return bytes;
}

hl::network::GoldSrcPlayerMaterialization ValidPlayer()
{
    hl::network::GoldSrcPlayerMaterialization player;
    player.private_data_ready = true;
    player.player_entity_ready = true;
    player.player_spawned = true;
    player.finite_network_state = true;
    player.model_index = 7u;
    return player;
}

hl::network::GoldSrcClientEdictBinding BindSlot(
    hl::network::GoldSrcClientEdictRegistry& registry,
    std::uint16_t slot,
    std::uint64_t session)
{
    hl::network::GoldSrcClientEdictBinding binding;
    assert(
        registry.Bind(slot, session, &binding)
        == hl::network::GoldSrcClientEdictBindingResult::kBound);
    return binding;
}

hl::network::GoldSrcNetchanPacket AckPacket(
    std::uint32_t sequence,
    std::uint32_t acknowledgement,
    bool reliable_acknowledgement)
{
    using namespace hl::network;
    GoldSrcNetchanPacket packet{};
    packet.sequence = sequence & kGoldSrcNetchanSequenceMask;
    packet.acknowledgement =
        acknowledgement & kGoldSrcNetchanSequenceMask;
    packet.reliable_acknowledgement = reliable_acknowledgement;
    packet.raw_sequence = packet.sequence;
    packet.raw_acknowledgement = packet.acknowledgement
        | (reliable_acknowledgement
            ? kGoldSrcNetchanReliableFlag
            : 0u);
    return packet;
}
} // namespace

int main()
{
    using namespace hl::network;

    {
        const std::vector<std::uint8_t> bytes = Command("spawn 42 123456");
        const auto decoded =
            DecodeGoldSrcClientLifecycleCommand(bytes.data(), bytes.size());
        assert(decoded.ok());
        assert(decoded.command.kind == GoldSrcClientLifecycleCommandKind::kSpawn);
        assert(decoded.command.spawn_count == 42u);
        assert(decoded.command.checksum_token == 123456u);

        for (const std::string_view invalid : {
                 "spawn",
                 "spawn 42",
                 "spawn 42 1 extra",
                 "spawn -1 1",
                 "spawn 1 -1",
                 "spawn  1 2",
                 "begin 1",
                 "prespawn 1",
                 "spawn 1 2;quit",
                 "spawn 4294967296 1",
                 "spawn 1 4294967296",
             })
        {
            const std::vector<std::uint8_t> candidate = Command(invalid);
            assert(!DecodeGoldSrcClientLifecycleCommand(
                        candidate.data(), candidate.size()).ok());
        }
        std::vector<std::uint8_t> missing = Command("spawn 1 2");
        missing.pop_back();
        missing.pop_back();
        assert(
            DecodeGoldSrcClientLifecycleCommand(missing.data(), missing.size())
                .status
            == GoldSrcClientLifecycleCommandStatus::kMissingTerminator);
        std::vector<std::uint8_t> trailing = Command("spawn 1 2");
        trailing.push_back(9u);
        assert(
            DecodeGoldSrcClientLifecycleCommand(
                trailing.data(), trailing.size()).status
            == GoldSrcClientLifecycleCommandStatus::kUnsupportedTrailingData);
        std::vector<std::uint8_t> control = Command("spawn 1 2");
        control[4] = '\n';
        assert(
            DecodeGoldSrcClientLifecycleCommand(
                control.data(), control.size()).status
            == GoldSrcClientLifecycleCommandStatus::kInvalidControlCharacter);
        assert(
            DecodeGoldSrcClientLifecycleCommand(nullptr, 1u).status
            == GoldSrcClientLifecycleCommandStatus::kNullInput);
    }

    {
        GoldSrcClientEdictRegistry registry(4u);
        const GoldSrcClientEdictBinding one = BindSlot(registry, 1u, 100u);
        const GoldSrcClientEdictBinding two = BindSlot(registry, 2u, 101u);
        assert(one.edict_index == 1u);
        assert(two.edict_index == 2u);
        assert(one.edict_index != 0u && two.edict_index != 0u);
        assert(
            registry.Validate(one)
            == GoldSrcClientEdictBindingResult::kBound);
        GoldSrcClientEdictBinding duplicate;
        assert(
            registry.Bind(1u, 100u, &duplicate)
            == GoldSrcClientEdictBindingResult::kDuplicateSession);
        assert(duplicate.edict_generation == one.edict_generation);
        assert(
            registry.Disconnect(one)
            == GoldSrcClientEdictBindingResult::kBound);
        assert(
            registry.Validate(one)
            == GoldSrcClientEdictBindingResult::kNotBound);
        const GoldSrcClientEdictBinding reused =
            BindSlot(registry, 1u, 200u);
        assert(reused.edict_index == 1u);
        assert(reused.edict_generation != one.edict_generation);
        assert(
            registry.Validate(one)
            == GoldSrcClientEdictBindingResult::kStaleSession);
        assert(
            registry.Validate(two)
            == GoldSrcClientEdictBindingResult::kBound);
        assert(
            registry.Bind(0u, 300u)
            == GoldSrcClientEdictBindingResult::kSlotOutOfRange);
        assert(
            registry.Bind(5u, 300u)
            == GoldSrcClientEdictBindingResult::kSlotOutOfRange);
    }

    {
        GoldSrcClientEdictRegistry registry(2u);
        const GoldSrcClientEdictBinding binding =
            BindSlot(registry, 1u, 1u);
        GoldSrcPlayerLifecycleSession lifecycle;
        assert(
            lifecycle.BindClientEdict(binding)
            == GoldSrcPlayerLifecycleResult::kAdvanced);
        assert(
            lifecycle.RecordInitialUserInfo("player")
            == GoldSrcPlayerLifecycleResult::kAdvanced);
        int connect_calls = 0;
        const GoldSrcClientConnectCallback accept =
            [&connect_calls](
                const GoldSrcClientEdictBinding& callback_binding,
                std::string_view name,
                std::string_view address,
                std::array<char, kGoldSrcClientRejectReasonBytes>&)
            {
                ++connect_calls;
                return callback_binding.edict_index == 1u
                    && name == "player"
                    && address == "127.0.0.1:27015";
            };
        assert(
            lifecycle.ConnectGameDll(
                "player", "127.0.0.1:27015", accept)
            == GoldSrcPlayerLifecycleResult::kAdvanced);
        assert(connect_calls == 1);
        assert(
            lifecycle.ConnectGameDll(
                "player", "127.0.0.1:27015", accept)
            == GoldSrcPlayerLifecycleResult::kAlreadyApplied);
        assert(connect_calls == 1);
        assert(
            lifecycle.EnterAwaitingPutInServer()
            == GoldSrcPlayerLifecycleResult::kAdvanced);

        int put_calls = 0;
        const GoldSrcClientPutInServerCallback put =
            [&put_calls](const GoldSrcClientEdictBinding& callback_binding)
            {
                ++put_calls;
                assert(callback_binding.edict_index == 1u);
                return ValidPlayer();
            };
        GoldSrcClientLifecycleCommand spawn;
        spawn.kind = GoldSrcClientLifecycleCommandKind::kSpawn;
        spawn.spawn_count = 7u;
        spawn.checksum_token = 11u;
        assert(
            lifecycle.PutInServer(8u, spawn, put)
            == GoldSrcPlayerLifecycleResult::kSpawnCountMismatch);
        assert(put_calls == 0);
        assert(
            lifecycle.PutInServer(7u, spawn, put)
            == GoldSrcPlayerLifecycleResult::kAdvanced);
        assert(put_calls == 1);
        assert(lifecycle.player().private_data_ready);
        assert(lifecycle.player().player_spawned);
        assert(!lifecycle.gameplay_active());
        assert(
            lifecycle.PutInServer(7u, spawn, put)
            == GoldSrcPlayerLifecycleResult::kAlreadyApplied);
        assert(put_calls == 1);

        assert(
            lifecycle.EstablishView(2u)
            == GoldSrcPlayerLifecycleResult::kInvalidViewEntity);
        assert(
            lifecycle.EstablishView(1u)
            == GoldSrcPlayerLifecycleResult::kAdvanced);
        assert(
            lifecycle.MarkSignonComplete()
            == GoldSrcPlayerLifecycleResult::kInvalidPhase);
        assert(
            lifecycle.MarkPlayerSnapshot(123u)
            == GoldSrcPlayerLifecycleResult::kAdvanced);
        assert(
            lifecycle.MarkPlayerSnapshot(124u)
            == GoldSrcPlayerLifecycleResult::kAlreadyApplied);
        assert(
            lifecycle.MarkSignonComplete()
            == GoldSrcPlayerLifecycleResult::kAdvanced);
        assert(lifecycle.signon_complete());
        assert(lifecycle.view_entity_index() == 1u);
        assert(lifecycle.first_player_snapshot_frame_id() == 123u);

        int disconnect_calls = 0;
        const GoldSrcClientDisconnectCallback disconnect =
            [&disconnect_calls](const GoldSrcClientEdictBinding& disconnected)
            {
                ++disconnect_calls;
                assert(disconnected.edict_index == 1u);
            };
        assert(
            lifecycle.Disconnect(disconnect)
            == GoldSrcPlayerLifecycleResult::kAdvanced);
        assert(disconnect_calls == 1);
        assert(
            lifecycle.Disconnect(disconnect)
            == GoldSrcPlayerLifecycleResult::kAlreadyApplied);
        assert(disconnect_calls == 1);
        assert(lifecycle.view_entity_index() == 0u);
        assert(!lifecycle.first_player_snapshot_frame_id().has_value());
        assert(!lifecycle.game_dll_connected());
        assert(!lifecycle.put_in_server());
    }

    {
        GoldSrcClientEdictRegistry registry(1u);
        GoldSrcPlayerLifecycleSession rejected;
        assert(
            rejected.BindClientEdict(BindSlot(registry, 1u, 1u))
            == GoldSrcPlayerLifecycleResult::kAdvanced);
        assert(
            rejected.RecordInitialUserInfo("reject")
            == GoldSrcPlayerLifecycleResult::kAdvanced);
        int rejection_calls = 0;
        const auto reject =
            [&rejection_calls](
                const GoldSrcClientEdictBinding&,
                std::string_view,
                std::string_view,
                std::array<char, kGoldSrcClientRejectReasonBytes>& reason)
            {
                ++rejection_calls;
                reason.fill('R');
                return false;
            };
        assert(
            rejected.ConnectGameDll("reject", "127.0.0.1:1", reject)
            == GoldSrcPlayerLifecycleResult::kGameDllRejected);
        assert(rejection_calls == 1);
        assert(rejected.reject_reason().size() == 127u);
        assert(!rejected.game_dll_connected());
        assert(
            rejected.EnterAwaitingPutInServer()
            == GoldSrcPlayerLifecycleResult::kInvalidPhase);
        int disconnect_calls = 0;
        assert(
            rejected.Disconnect(
                [&disconnect_calls](const GoldSrcClientEdictBinding&)
                {
                    ++disconnect_calls;
                })
            == GoldSrcPlayerLifecycleResult::kAdvanced);
        assert(disconnect_calls == 0);
    }

    {
        GoldSrcClientEdictRegistry registry(1u);
        GoldSrcPlayerLifecycleSession failed;
        assert(
            failed.BindClientEdict(BindSlot(registry, 1u, 1u))
            == GoldSrcPlayerLifecycleResult::kAdvanced);
        assert(
            failed.RecordInitialUserInfo("player")
            == GoldSrcPlayerLifecycleResult::kAdvanced);
        assert(
            failed.ConnectGameDll(
                "player",
                "127.0.0.1:2",
                [](const auto&, auto, auto, auto&) { return true; })
            == GoldSrcPlayerLifecycleResult::kAdvanced);
        assert(
            failed.EnterAwaitingPutInServer()
            == GoldSrcPlayerLifecycleResult::kAdvanced);
        GoldSrcPlayerMaterialization missing_private = ValidPlayer();
        missing_private.private_data_ready = false;
        assert(
            failed.PutInServerFromReconciliation(
                [missing_private](const auto&) { return missing_private; })
            == GoldSrcPlayerLifecycleResult::kPlayerMaterializationFailed);
        assert(!failed.put_in_server());
        assert(!failed.player().player_entity_ready);
        assert(!failed.gameplay_active());
    }

    {
        GoldSrcPlayerLifecycleControl control;
        control.view_entity_index = 1u;
        control.view_angles = {12.5f, 270.0f, 0.0f};
        const auto encoded = EncodeGoldSrcPlayerLifecycleControl(control);
        assert(encoded.ok());
        assert(encoded.payload.size == kGoldSrcPlayerLifecycleControlBytes);
        const auto decoded = DecodeGoldSrcPlayerLifecycleControl(
            encoded.payload.bytes.data(), encoded.payload.size);
        assert(decoded.ok());
        assert(decoded.control.view_entity_index == 1u);
        assert(std::fabs(decoded.control.view_angles[0] - 12.5f) < 0.01f);
        assert(std::fabs(decoded.control.view_angles[1] - 270.0f) < 0.01f);
        assert(
            decoded.control.signon_number
            == kGoldSrcPlayerLifecycleSignonNumber);

        GoldSrcPlayerLifecycleControl invalid = control;
        invalid.view_entity_index = 0u;
        assert(!EncodeGoldSrcPlayerLifecycleControl(invalid).ok());
        invalid = control;
        invalid.view_angles[0] =
            std::numeric_limits<float>::quiet_NaN();
        assert(!EncodeGoldSrcPlayerLifecycleControl(invalid).ok());
        auto corrupted = encoded.payload;
        corrupted.bytes[11] = 2u;
        assert(
            DecodeGoldSrcPlayerLifecycleControl(
                corrupted.bytes.data(), corrupted.size).status
            == GoldSrcPlayerLifecycleControlStatus::kWrongSignonNumber);

        using namespace std::chrono_literals;
        const Ipv4Endpoint endpoint{{127u, 0u, 0u, 1u}, 27015u};
        const GoldSrcNetchanState::TimePoint start{};
        GoldSrcNetchanState netchan;
        assert(netchan.Initialize(endpoint, std::uint16_t{27015u}, 0u, start));
        assert(
            NameFor(
                GoldSrcNetchanReliablePayloadKind::kPlayerLifecycleControl)
            == "player_lifecycle_control");
        assert(
            netchan.QueueReliablePayload(
                encoded.payload.bytes.data(),
                encoded.payload.size,
                GoldSrcNetchanReliablePayloadKind::kPlayerLifecycleControl)
            == GoldSrcNetchanQueueResult::kQueued);
        assert(
            netchan.QueueReliablePayload(
                encoded.payload.bytes.data(),
                encoded.payload.size,
                GoldSrcNetchanReliablePayloadKind::kPlayerLifecycleControl)
            == GoldSrcNetchanQueueResult::kReliableAlreadyPending);
        const auto frozen = encoded.payload;
        const bool reliable_toggle = netchan.local_reliable_sequence();

        GoldSrcNetchanDatagram first{};
        assert(netchan.BuildOutgoingDatagram(&first));
        const auto first_decoded = DecodeGoldSrcNetchanDatagram(
            GoldSrcNetchanDirection::kServerToClient,
            first.bytes.data(),
            first.size);
        assert(first_decoded.ok());
        assert(first_decoded.packet.reliable_present);
        assert(std::equal(
            frozen.bytes.begin(),
            frozen.bytes.begin()
                + static_cast<std::ptrdiff_t>(frozen.size),
            first_decoded.packet.payload.begin()));

        const auto wrong_ack = netchan.ProcessIncomingDatagramDetailed(
            endpoint,
            AckPacket(1u, 1u, !reliable_toggle),
            start + 1s);
        assert(wrong_ack.result == GoldSrcNetchanProcessResult::kAccepted);
        assert(!wrong_ack.reliable_payload_was_acknowledged());
        assert(netchan.reliable_pending());

        GoldSrcNetchanDatagram ordinary{};
        assert(netchan.BuildOutgoingDatagram(&ordinary));
        const auto ordinary_decoded = DecodeGoldSrcNetchanDatagram(
            GoldSrcNetchanDirection::kServerToClient,
            ordinary.bytes.data(),
            ordinary.size);
        assert(ordinary_decoded.ok());
        assert(!ordinary_decoded.packet.reliable_present);

        const auto covering_wrong_toggle =
            netchan.ProcessIncomingDatagramDetailed(
                endpoint,
                AckPacket(2u, 2u, !reliable_toggle),
                start + 2s);
        assert(
            covering_wrong_toggle.result
            == GoldSrcNetchanProcessResult::kAccepted);
        assert(!covering_wrong_toggle.reliable_payload_was_acknowledged());
        assert(netchan.reliable_pending());

        GoldSrcNetchanDatagram retransmission{};
        assert(netchan.BuildOutgoingDatagram(&retransmission));
        const auto retransmission_decoded =
            DecodeGoldSrcNetchanDatagram(
                GoldSrcNetchanDirection::kServerToClient,
                retransmission.bytes.data(),
                retransmission.size);
        assert(retransmission_decoded.ok());
        assert(retransmission_decoded.packet.reliable_present);
        assert(std::equal(
            frozen.bytes.begin(),
            frozen.bytes.begin()
                + static_cast<std::ptrdiff_t>(frozen.size),
            retransmission_decoded.packet.payload.begin()));

        const auto non_covering_ack =
            netchan.ProcessIncomingDatagramDetailed(
                endpoint,
                AckPacket(3u, 2u, reliable_toggle),
                start + 3s);
        assert(
            non_covering_ack.result
            == GoldSrcNetchanProcessResult::kAccepted);
        assert(!non_covering_ack.reliable_payload_was_acknowledged());
        assert(netchan.reliable_pending());

        const auto covering_ack =
            netchan.ProcessIncomingDatagramDetailed(
                endpoint,
                AckPacket(4u, 3u, reliable_toggle),
                start + 4s);
        assert(covering_ack.result == GoldSrcNetchanProcessResult::kAccepted);
        assert(covering_ack.reliable_payload_was_acknowledged());
        assert(
            covering_ack.acknowledged_reliable_kind
            == GoldSrcNetchanReliablePayloadKind::kPlayerLifecycleControl);
        assert(!netchan.reliable_pending());

        netchan.Reset();
        assert(!netchan.initialized());
        assert(!netchan.reliable_pending());
        assert(netchan.reliable_pending_bytes() == 0u);
        assert(
            netchan.pending_reliable_kind()
            == GoldSrcNetchanReliablePayloadKind::kNone);
    }

    {
        GoldSrcServerFrame before;
        before.frame_id = 10u;
        before.server_time = 1.0f;
        before.clientdata.state.field_count = 1u;
        before.clientdata.state.values[0].kind =
            GoldSrcDeltaValueKind::kFloatingPoint;
        before.clientdata.state.values[0].floating_value = 0.0;
        GoldSrcSnapshotEntityState world_entity;
        world_entity.entity_index = 3u;
        world_entity.kind = GoldSrcBaselineKind::kEntity;
        world_entity.state.field_count = 1u;
        before.entities.push_back(world_entity);

        GoldSrcPlayerSnapshotInput player;
        player.entity.entity_index = 1u;
        player.entity.kind = GoldSrcBaselineKind::kPlayer;
        player.entity.state.field_count = 2u;
        player.entity.state.values[0].kind =
            GoldSrcDeltaValueKind::kUnsignedInteger;
        player.entity.state.values[0].unsigned_value = 7u;
        player.entity.state.values[1].kind =
            GoldSrcDeltaValueKind::kFloatingPoint;
        player.entity.state.values[1].floating_value = 64.0;
        player.clientdata.state.field_count = 1u;
        player.clientdata.state.values[0].kind =
            GoldSrcDeltaValueKind::kFloatingPoint;
        player.clientdata.state.values[0].floating_value = 100.0;

        GoldSrcServerFrame after = before;
        after.frame_id = 11u;
        assert(
            ApplyGoldSrcPlayerSnapshot(&after, player, 2u)
            == GoldSrcPlayerSnapshotApplyStatus::kApplied);
        assert(after.entities.size() == 2u);
        assert(after.entities.front().entity_index == 1u);
        assert(after.entities.front().kind == GoldSrcBaselineKind::kPlayer);
        assert(after.clientdata.state.values[0].floating_value == 100.0);
        const GoldSrcEntityDiffResult added =
            CompareGoldSrcSnapshotEntities(before, after);
        assert(added.ok());
        assert(added.adds == 1u);

        GoldSrcServerFrame updated = after;
        updated.frame_id = 12u;
        updated.entities.front().state.values[1].floating_value = 65.0;
        const GoldSrcEntityDiffResult changed =
            CompareGoldSrcSnapshotEntities(after, updated);
        assert(changed.ok());
        assert(changed.updates == 1u);

        GoldSrcServerFrame removed = updated;
        removed.frame_id = 13u;
        removed.entities.erase(removed.entities.begin());
        const GoldSrcEntityDiffResult erased =
            CompareGoldSrcSnapshotEntities(updated, removed);
        assert(erased.ok());
        assert(erased.removes == 1u);

        assert(
            ApplyGoldSrcPlayerSnapshot(&after, player, 2u)
            == GoldSrcPlayerSnapshotApplyStatus::kDuplicateEntity);
        player.entity.entity_index = 3u;
        assert(
            ApplyGoldSrcPlayerSnapshot(&before, player, 2u)
            == GoldSrcPlayerSnapshotApplyStatus::kInvalidEntityIndex);
        player.entity.entity_index = 1u;
        player.entity.state.values[1].floating_value =
            std::numeric_limits<double>::quiet_NaN();
        assert(
            ApplyGoldSrcPlayerSnapshot(&before, player, 2u)
            == GoldSrcPlayerSnapshotApplyStatus::kInvalidPlayerState);
    }

    {
        GoldSrcDeltaField entity_field;
        entity_field.field_type = kGoldSrcDeltaTypeInteger;
        entity_field.name = "modelindex";
        entity_field.field_size = 1u;
        entity_field.significant_bits = 12u;
        entity_field.premultiply = 1.0;
        entity_field.postmultiply = 1.0;

        GoldSrcDeltaField physinfo_field;
        physinfo_field.field_type = kGoldSrcDeltaTypeString;
        physinfo_field.name = "physinfo";
        physinfo_field.field_size = 1u;
        physinfo_field.significant_bits = 1u;
        physinfo_field.premultiply = 1.0;
        physinfo_field.postmultiply = 1.0;

        GoldSrcDeltaRegistry registry;
        GoldSrcDeltaTable clientdata_table;
        clientdata_table.name = "clientdata_t";
        clientdata_table.fields.push_back(physinfo_field);
        registry.tables.push_back(clientdata_table);
        GoldSrcDeltaTable player_table;
        player_table.name = "entity_state_player_t";
        player_table.fields.push_back(entity_field);
        registry.tables.push_back(player_table);

        GoldSrcBaselineBundle baselines;
        baselines.maximum_clients = 1u;
        GoldSrcEntityBaseline player_baseline;
        player_baseline.entity_index = 1u;
        player_baseline.kind = GoldSrcBaselineKind::kPlayer;
        player_baseline.state.field_count = 1u;
        player_baseline.state.values[0].kind =
            GoldSrcDeltaValueKind::kUnsignedInteger;
        baselines.entities.push_back(player_baseline);

        GoldSrcPlayerSnapshotInput player;
        player.entity.entity_index = 1u;
        player.entity.kind = GoldSrcBaselineKind::kPlayer;
        player.entity.state.field_count = 1u;
        player.entity.state.values[0].kind =
            GoldSrcDeltaValueKind::kUnsignedInteger;
        player.clientdata.state.field_count = 1u;
        player.clientdata.state.values[0].kind =
            GoldSrcDeltaValueKind::kString;
        player.clientdata.state.values[0].string_size = 0u;

        const GoldSrcSnapshotBuildResult built =
            BuildGoldSrcFirstSnapshot(
                14u,
                2.0f,
                baselines,
                registry,
                kGoldSrcMaximumSnapshotBytes,
                &player);
        assert(built.ok());
        assert(
            built.bundle.frame.clientdata.state.values[0].kind
            == GoldSrcDeltaValueKind::kString);
        assert(
            built.bundle.frame.clientdata.state.values[0].string_size
            == 0u);
    }

    return 0;
}
