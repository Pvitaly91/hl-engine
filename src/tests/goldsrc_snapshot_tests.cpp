#include "network/goldsrc_snapshot.h"
#include "network/goldsrc_netchan.h"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <utility>

namespace
{
using namespace hl::network;

GoldSrcDeltaField MakeIntegerField(
    const char* name,
    std::uint8_t bits = 16u,
    bool signed_value = false)
{
    GoldSrcDeltaField field;
    field.field_type = kGoldSrcDeltaTypeInteger
        | (signed_value ? kGoldSrcDeltaTypeSigned : 0u);
    field.name = name;
    field.field_size = 4u;
    field.significant_bits = bits;
    field.premultiply = 1.0;
    field.postmultiply = 1.0;
    return field;
}

GoldSrcDeltaTable MakeTable(
    const char* name,
    const char* field_name = "modelindex")
{
    GoldSrcDeltaTable table;
    table.name = name;
    table.fields.push_back(MakeIntegerField(field_name, 16u, false));
    return table;
}

GoldSrcDeltaRegistry MakeRegistry()
{
    GoldSrcDeltaRegistry registry;
    registry.tables.push_back(MakeTable("event_t", "entindex"));
    registry.tables.push_back(MakeTable("weapon_data_t", "m_iClip"));
    registry.tables.push_back(MakeTable("usercmd_t", "msec"));
    registry.tables.push_back(
        MakeTable("custom_entity_state_t", "modelindex"));
    registry.tables.push_back(
        MakeTable("entity_state_player_t", "modelindex"));
    registry.tables.push_back(MakeTable("entity_state_t", "modelindex"));
    registry.tables.push_back(MakeTable("clientdata_t", "health"));
    return registry;
}

GoldSrcDecodedDeltaRecord MakeRecord(std::uint32_t value)
{
    GoldSrcDecodedDeltaRecord record;
    record.field_count = 1u;
    record.values[0].kind = GoldSrcDeltaValueKind::kUnsignedInteger;
    record.values[0].unsigned_value = value;
    return record;
}

GoldSrcBaselineBundle MakeBaselines()
{
    GoldSrcBaselineBundle baselines;
    baselines.maximum_clients = 1u;

    GoldSrcEntityBaseline world;
    world.entity_index = 0u;
    world.kind = GoldSrcBaselineKind::kWorld;
    world.model_index = 1u;
    world.state = MakeRecord(1u);
    baselines.entities.push_back(world);

    GoldSrcEntityBaseline player;
    player.entity_index = 1u;
    player.kind = GoldSrcBaselineKind::kPlayer;
    player.model_index = 1u;
    player.state = MakeRecord(1u);
    baselines.entities.push_back(player);

    GoldSrcEntityBaseline normal;
    normal.entity_index = 2u;
    normal.kind = GoldSrcBaselineKind::kEntity;
    normal.model_index = 2u;
    normal.state = MakeRecord(2u);
    baselines.entities.push_back(normal);

    GoldSrcEntityBaseline custom;
    custom.entity_index = 4u;
    custom.kind = GoldSrcBaselineKind::kCustomEntity;
    custom.model_index = 3u;
    custom.state = MakeRecord(3u);
    baselines.entities.push_back(custom);
    return baselines;
}

GoldSrcServerFrame MakeFrame(std::uint32_t frame_id, float time)
{
    GoldSrcServerFrame frame;
    frame.frame_id = frame_id;
    frame.server_time = time;
    frame.clientdata.state = MakeRecord(0u);

    GoldSrcSnapshotEntityState first;
    first.entity_index = 2u;
    first.kind = GoldSrcBaselineKind::kEntity;
    first.state = MakeRecord(2u);
    frame.entities.push_back(first);

    GoldSrcSnapshotEntityState second;
    second.entity_index = 4u;
    second.kind = GoldSrcBaselineKind::kCustomEntity;
    second.state = MakeRecord(3u);
    frame.entities.push_back(second);
    return frame;
}

void TestFirstSnapshotBuildAndSemanticRoundTrip()
{
    const GoldSrcDeltaRegistry registry = MakeRegistry();
    const GoldSrcBaselineBundle baselines = MakeBaselines();
    const GoldSrcSnapshotBuildResult built =
        BuildGoldSrcFirstSnapshot(37u, 12.5f, baselines, registry);
    assert(built.ok());
    assert(built.bundle.payload.size > 0u);
    assert(built.bundle.frame.frame_id == 37u);
    assert(built.bundle.frame.server_time == 12.5f);
    assert(built.bundle.frame.weapons.empty());
    assert(built.bundle.frame.entities.size() == 2u);
    assert(built.bundle.frame.entities[0].entity_index == 2u);
    assert(built.bundle.frame.entities[1].entity_index == 4u);

    const GoldSrcSnapshotDecodeResult decoded =
        DecodeGoldSrcFirstSnapshot(
            built.bundle.payload.bytes.data(),
            built.bundle.payload.size,
            37u,
            baselines,
            registry);
    assert(decoded.ok());
    assert(decoded.bytes_consumed == built.bundle.payload.size);
    assert(decoded.frame.frame_id == 37u);
    assert(decoded.frame.server_time == 12.5f);
    assert(decoded.frame.clientdata.state.field_count == 1u);
    assert(decoded.frame.weapons.empty());
    assert(decoded.frame.entities.size() == 2u);
    assert(decoded.frame.entities[0].kind == GoldSrcBaselineKind::kEntity);
    assert(
        decoded.frame.entities[1].kind
        == GoldSrcBaselineKind::kCustomEntity);
}

void TestMessageOrderAndBaselineRelativeDelta()
{
    const GoldSrcDeltaRegistry registry = MakeRegistry();
    const GoldSrcBaselineBundle baselines = MakeBaselines();
    GoldSrcServerFrame frame = MakeFrame(8u, 3.0f);
    frame.clientdata.state.values[0].unsigned_value = 75u;
    frame.entities[0].state.values[0].unsigned_value = 9u;

    const GoldSrcSnapshotEncodeResult encoded =
        EncodeGoldSrcFirstSnapshot(frame, baselines, registry);
    assert(encoded.ok());
    assert(encoded.payload.bytes[0] == kGoldSrcServerTimeOpcode);
    assert(encoded.payload.bytes[5] == kGoldSrcClientDataOpcode);

    const GoldSrcSnapshotDecodeResult decoded =
        DecodeGoldSrcFirstSnapshot(
            encoded.payload.bytes.data(),
            encoded.payload.size,
            frame.frame_id,
            baselines,
            registry);
    assert(decoded.ok());
    assert(
        decoded.frame.clientdata.state.values[0].unsigned_value == 75u);
    assert(
        decoded.frame.entities[0].state.values[0].unsigned_value == 9u);
}

void TestMalformedAndBoundedCodecFailures()
{
    const GoldSrcDeltaRegistry registry = MakeRegistry();
    const GoldSrcBaselineBundle baselines = MakeBaselines();
    const GoldSrcSnapshotBuildResult built =
        BuildGoldSrcFirstSnapshot(9u, 4.0f, baselines, registry);
    assert(built.ok());

    GoldSrcFirstSnapshotPayload malformed = built.bundle.payload;
    malformed.bytes[5] = 0xFFu;
    assert(
        DecodeGoldSrcFirstSnapshot(
            malformed.bytes.data(),
            malformed.size,
            9u,
            baselines,
            registry).status
        == GoldSrcSnapshotCodecStatus::kWrongMessageOrder);

    GoldSrcFirstSnapshotPayload malformed_clientdata =
        built.bundle.payload;
    malformed_clientdata.bytes[6] = 0x02u;
    assert(
        DecodeGoldSrcFirstSnapshot(
            malformed_clientdata.bytes.data(),
            7u,
            9u,
            baselines,
            registry).status
        == GoldSrcSnapshotCodecStatus::kInvalidClientData);

    assert(
        EncodeGoldSrcFirstSnapshot(
            built.bundle.frame,
            baselines,
            registry,
            1u).status
        == GoldSrcSnapshotCodecStatus::kOutputCapacityExceeded);

    GoldSrcServerFrame duplicate = built.bundle.frame;
    duplicate.entities.push_back(duplicate.entities.back());
    assert(
        EncodeGoldSrcFirstSnapshot(
            duplicate,
            baselines,
            registry).status
        != GoldSrcSnapshotCodecStatus::kOk);

    GoldSrcServerFrame reversed = built.bundle.frame;
    std::swap(reversed.entities[0], reversed.entities[1]);
    assert(
        EncodeGoldSrcFirstSnapshot(
            reversed,
            baselines,
            registry).status
        != GoldSrcSnapshotCodecStatus::kOk);

    GoldSrcDeltaRegistry missing = registry;
    missing.tables.pop_back();
    assert(
        BuildGoldSrcFirstSnapshot(
            10u,
            5.0f,
            baselines,
            missing).status
        == GoldSrcSnapshotCodecStatus::kMissingDeltaTable);

    assert(
        BuildGoldSrcFirstSnapshot(
            10u,
            std::numeric_limits<float>::infinity(),
            baselines,
            registry).status
        == GoldSrcSnapshotCodecStatus::kNonFiniteServerTime);
}

void TestFrameHistoryAcknowledgementAndEviction()
{
    GoldSrcClientFrameHistory history(3u);
    assert(history.valid());
    assert(history.capacity() == 3u);
    assert(history.Store(MakeFrame(100u, 1.0f))
        == GoldSrcFrameStoreResult::kStored);
    assert(history.Store(MakeFrame(101u, 2.0f))
        == GoldSrcFrameStoreResult::kStored);
    assert(history.Store(MakeFrame(102u, 3.0f))
        == GoldSrcFrameStoreResult::kStored);
    assert(history.Acknowledge(102u)
        == GoldSrcFrameAcknowledgeResult::kAcknowledged);
    assert(history.Acknowledge(102u)
        == GoldSrcFrameAcknowledgeResult::kDuplicate);
    assert(history.Acknowledge(101u)
        == GoldSrcFrameAcknowledgeResult::kStale);
    assert(history.Acknowledge(103u)
        == GoldSrcFrameAcknowledgeResult::kFuture);
    assert(history.Store(MakeFrame(103u, 4.0f))
        == GoldSrcFrameStoreResult::kStored);
    assert(history.size() == 3u);
    assert(history.Find(100u) == nullptr);
    assert(history.Acknowledge(100u)
        == GoldSrcFrameAcknowledgeResult::kEvicted);
    assert(history.Store(MakeFrame(103u, 5.0f))
        == GoldSrcFrameStoreResult::kNonMonotonicFrame);
    history.Reset();
    assert(history.size() == 0u);
    assert(!history.last_acknowledged_frame().has_value());
    assert(history.Acknowledge(103u)
        == GoldSrcFrameAcknowledgeResult::kUnknown);

    GoldSrcClientFrameHistory gap(3u);
    assert(gap.Store(MakeFrame(200u, 1.0f))
        == GoldSrcFrameStoreResult::kStored);
    assert(gap.Store(MakeFrame(202u, 2.0f))
        == GoldSrcFrameStoreResult::kStored);
    assert(gap.Acknowledge(201u)
        == GoldSrcFrameAcknowledgeResult::kUnknown);

    GoldSrcClientFrameHistory invalid(0u);
    assert(!invalid.valid());
    assert(invalid.Store(MakeFrame(1u, 1.0f))
        == GoldSrcFrameStoreResult::kInvalidHistory);
}

void TestSnapshotSessionState()
{
    const GoldSrcDeltaRegistry registry = MakeRegistry();
    const GoldSrcBaselineBundle baselines = MakeBaselines();
    GoldSrcSnapshotBuildResult built =
        BuildGoldSrcFirstSnapshot(44u, 7.0f, baselines, registry);
    assert(built.ok());

    GoldSrcFirstSnapshotSessionState session;
    assert(session.phase() == GoldSrcFirstSnapshotPhase::kNone);
    assert(session.EnterAwaiting()
        == GoldSrcFirstSnapshotTransitionResult::kAdvanced);
    assert(session.Prepare(std::move(built.bundle))
        == GoldSrcFirstSnapshotTransitionResult::kAdvanced);
    assert(
        session.phase()
        == GoldSrcFirstSnapshotPhase::kFirstSnapshotPrepared);
    assert(session.MarkSent()
        == GoldSrcFirstSnapshotTransitionResult::kAdvanced);
    assert(
        session.phase()
        == GoldSrcFirstSnapshotPhase::
            kFirstSnapshotSentAwaitingClientReference);
    assert(session.Acknowledge(44u)
        == GoldSrcFrameAcknowledgeResult::kAcknowledged);
    assert(
        session.phase()
        == GoldSrcFirstSnapshotPhase::kFirstSnapshotAcknowledged);
    assert(session.Acknowledge(44u)
        == GoldSrcFrameAcknowledgeResult::kDuplicate);
    session.Reset();
    assert(session.phase() == GoldSrcFirstSnapshotPhase::kNone);
    assert(session.history().size() == 0u);
    assert(!session.prepared().has_value());
}

void TestClientFrameReferenceApplicationDecode()
{
    const GoldSrcDeltaRegistry registry = MakeRegistry();
    const std::array<std::uint8_t, 2> reference = {
        kGoldSrcClientFrameReferenceOpcode,
        44u,
    };
    const GoldSrcClientApplicationDecodeResult decoded =
        DecodeGoldSrcClientApplicationPayload(
            reference.data(),
            reference.size(),
            1u,
            registry);
    assert(decoded.ok());
    assert(!decoded.move_present);
    assert(decoded.frame_reference_present);
    assert(decoded.frame_reference == 44u);
    assert(decoded.frame_reference_count == 1u);

    const std::array<std::uint8_t, 1> truncated = {
        kGoldSrcClientFrameReferenceOpcode,
    };
    assert(
        DecodeGoldSrcClientApplicationPayload(
            truncated.data(),
            truncated.size(),
            1u,
            registry).status
        == GoldSrcClientApplicationDecodeStatus::kTruncatedFrameReference);

    const std::array<std::uint8_t, 4> duplicate = {
        kGoldSrcClientFrameReferenceOpcode,
        44u,
        kGoldSrcClientFrameReferenceOpcode,
        44u,
    };
    assert(
        DecodeGoldSrcClientApplicationPayload(
            duplicate.data(),
            duplicate.size(),
            1u,
            registry).status
        == GoldSrcClientApplicationDecodeStatus::kMultipleFrameReferences);
}

void TestSnapshotUsesOrdinaryUnreliableNetchan()
{
    const auto endpoint = Ipv4Endpoint::Parse("127.0.0.1", 27015u);
    assert(endpoint.has_value());
    GoldSrcNetchanState netchan;
    assert(netchan.Initialize(
        *endpoint,
        static_cast<std::uint16_t>(27005u),
        1u,
        GoldSrcNetchanState::Clock::now()));
    const std::array<std::uint8_t, 2> payload = {
        kGoldSrcServerTimeOpcode,
        0u,
    };
    const std::uint32_t frame_id = netchan.outgoing_sequence();
    GoldSrcNetchanDatagram datagram;
    assert(netchan.BuildOutgoingUnreliableDatagram(
        payload.data(),
        payload.size(),
        &datagram));
    const GoldSrcNetchanDecodeResult decoded =
        DecodeGoldSrcNetchanDatagram(
            GoldSrcNetchanDirection::kServerToClient,
            datagram.bytes.data(),
            datagram.size);
    assert(decoded.ok());
    assert(decoded.packet.sequence == frame_id);
    assert(!decoded.packet.reliable_present);
    assert(decoded.packet.payload_size >= payload.size());
    assert(decoded.packet.payload[0] == payload[0]);
    assert(decoded.packet.payload[1] == payload[1]);
}
} // namespace

int main()
{
    TestFirstSnapshotBuildAndSemanticRoundTrip();
    TestMessageOrderAndBaselineRelativeDelta();
    TestMalformedAndBoundedCodecFailures();
    TestFrameHistoryAcknowledgementAndEviction();
    TestSnapshotSessionState();
    TestClientFrameReferenceApplicationDecode();
    TestSnapshotUsesOrdinaryUnreliableNetchan();
    std::cout << "goldsrc_snapshot_tests: pass\n";
    return 0;
}
