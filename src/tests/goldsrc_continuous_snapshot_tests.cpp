#include "network/goldsrc_snapshot.h"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>

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

GoldSrcDeltaTable Table(const char* name, const char* field)
{
    GoldSrcDeltaTable table;
    table.name = name;
    table.fields.push_back(IntegerField(field));
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
        Table("entity_state_player_t", "modelindex"));
    registry.tables.push_back(Table("entity_state_t", "modelindex"));
    registry.tables.push_back(Table("clientdata_t", "health"));
    return registry;
}

GoldSrcDecodedDeltaRecord Record(std::uint32_t value)
{
    GoldSrcDecodedDeltaRecord record;
    record.field_count = 1u;
    record.values[0].kind =
        GoldSrcDeltaValueKind::kUnsignedInteger;
    record.values[0].unsigned_value = value;
    return record;
}

GoldSrcBaselineBundle Baselines()
{
    GoldSrcBaselineBundle baselines;
    baselines.maximum_clients = 1u;
    for (std::uint16_t index = 0u; index <= 6u; ++index)
    {
        GoldSrcEntityBaseline baseline;
        baseline.entity_index = index;
        baseline.kind = index == 0u
            ? GoldSrcBaselineKind::kWorld
            : index == 1u
                ? GoldSrcBaselineKind::kPlayer
                : index == 4u
                    ? GoldSrcBaselineKind::kCustomEntity
                    : GoldSrcBaselineKind::kEntity;
        baseline.model_index = index + 1u;
        baseline.state = Record(index + 1u);
        baselines.entities.push_back(baseline);
    }
    return baselines;
}

GoldSrcSnapshotEntityState Entity(
    std::uint16_t index,
    std::uint32_t value,
    GoldSrcBaselineKind kind = GoldSrcBaselineKind::kEntity)
{
    GoldSrcSnapshotEntityState entity;
    entity.entity_index = index;
    entity.kind = kind;
    entity.state = Record(value);
    return entity;
}

GoldSrcServerFrame Frame(std::uint32_t id, float time)
{
    GoldSrcServerFrame frame;
    frame.frame_id = id;
    frame.server_time = time;
    frame.clientdata.state = Record(0u);
    return frame;
}

void TestScheduler()
{
    GoldSrcSnapshotScheduler scheduler;
    assert(scheduler.CheckDue(0.0)
        == GoldSrcSnapshotDueResult::kDisabled);
    assert(!scheduler.Start(0.0, 9.0));
    assert(!scheduler.Start(0.0, 31.0));
    assert(scheduler.Start(10.0, 20.0));
    assert(std::abs(
        scheduler.state().snapshot_interval_seconds - 0.05) < 0.000001);
    assert(scheduler.CheckDue(10.049)
        == GoldSrcSnapshotDueResult::kNotDue);
    assert(scheduler.CheckDue(10.05)
        == GoldSrcSnapshotDueResult::kDue);
    assert(scheduler.CheckDue(10.05)
        == GoldSrcSnapshotDueResult::kNotDue);
    assert(scheduler.CheckDue(10.30)
        == GoldSrcSnapshotDueResult::kDue);
    assert(scheduler.state().skipped_snapshots >= 3u);
    assert(scheduler.CheckDue(10.20)
        == GoldSrcSnapshotDueResult::kNonMonotonicServerTime);
    scheduler.RecordGenerated(7u);
    scheduler.RecordSent(7u, GoldSrcSnapshotKind::kDelta);
    scheduler.RecordAcknowledged(7u);
    assert(scheduler.state().last_sent_frame == 7u);
    scheduler.Stop();
    assert(scheduler.CheckDue(20.0)
        == GoldSrcSnapshotDueResult::kDisabled);
    scheduler.Reset();
    assert(!scheduler.state().enabled);
    assert(!scheduler.state().last_sent_frame.has_value());
}

void TestFrameReferenceResolutionAndWrap()
{
    GoldSrcClientFrameHistory history(4u);
    assert(history.Store(Frame(254u, 1.0f))
        == GoldSrcFrameStoreResult::kStored);
    assert(history.Store(Frame(255u, 2.0f))
        == GoldSrcFrameStoreResult::kStored);
    assert(history.Store(Frame(256u, 3.0f))
        == GoldSrcFrameStoreResult::kStored);
    assert(history.Acknowledge(0u)
        == GoldSrcFrameAcknowledgeResult::kAcknowledged);
    assert(history.last_acknowledged_frame() == 256u);
    assert(history.Acknowledge(0u)
        == GoldSrcFrameAcknowledgeResult::kDuplicate);
    assert(history.Acknowledge(255u)
        == GoldSrcFrameAcknowledgeResult::kStale);
    assert(history.Acknowledge(1u)
        == GoldSrcFrameAcknowledgeResult::kFuture);

    GoldSrcClientFrameHistory full_wrap(4u);
    assert(full_wrap.Store(
        Frame(kGoldSrcServerFrameMask - 1u, 1.0f))
        == GoldSrcFrameStoreResult::kStored);
    assert(full_wrap.Store(
        Frame(kGoldSrcServerFrameMask, 2.0f))
        == GoldSrcFrameStoreResult::kStored);
    assert(full_wrap.Store(Frame(0u, 3.0f))
        == GoldSrcFrameStoreResult::kStored);
    assert(full_wrap.Store(Frame(1u, 4.0f))
        == GoldSrcFrameStoreResult::kStored);
    assert(full_wrap.Acknowledge(0u)
        == GoldSrcFrameAcknowledgeResult::kAcknowledged);
    assert(full_wrap.Acknowledge(1u)
        == GoldSrcFrameAcknowledgeResult::kAcknowledged);

    GoldSrcClientFrameHistory ambiguous(4u);
    assert(ambiguous.Store(Frame(1u, 1.0f))
        == GoldSrcFrameStoreResult::kStored);
    assert(ambiguous.Store(Frame(257u, 2.0f))
        == GoldSrcFrameStoreResult::kStored);
    assert(ambiguous.Acknowledge(1u)
        == GoldSrcFrameAcknowledgeResult::kAmbiguous);
    assert(ambiguous.Acknowledge(1u, 1u)
        == GoldSrcFrameAcknowledgeResult::kAcknowledged);
    assert(ambiguous.last_acknowledged_frame() == 1u);

    GoldSrcClientFrameHistory delayed_alias(2u);
    assert(delayed_alias.Store(Frame(1u, 1.0f))
        == GoldSrcFrameStoreResult::kStored);
    assert(delayed_alias.Store(Frame(257u, 2.0f))
        == GoldSrcFrameStoreResult::kStored);
    assert(delayed_alias.Store(Frame(258u, 3.0f))
        == GoldSrcFrameStoreResult::kStored);
    assert(delayed_alias.Acknowledge(1u, 1u)
        == GoldSrcFrameAcknowledgeResult::kFuture);
    assert(!delayed_alias.last_acknowledged_frame().has_value());
    assert(delayed_alias.Acknowledge(1u, 257u)
        == GoldSrcFrameAcknowledgeResult::kAcknowledged);
    assert(delayed_alias.last_acknowledged_frame() == 257u);

    GoldSrcClientFrameHistory evicted(2u);
    assert(evicted.Store(Frame(10u, 1.0f))
        == GoldSrcFrameStoreResult::kStored);
    assert(evicted.Store(Frame(11u, 2.0f))
        == GoldSrcFrameStoreResult::kStored);
    assert(evicted.Store(Frame(12u, 3.0f))
        == GoldSrcFrameStoreResult::kStored);
    assert(evicted.Acknowledge(10u)
        == GoldSrcFrameAcknowledgeResult::kEvicted);
    evicted.Reset();
    assert(evicted.Acknowledge(12u)
        == GoldSrcFrameAcknowledgeResult::kUnknown);
}

void TestEntityDiffAndDeltaCodec()
{
    const GoldSrcDeltaRegistry registry = Registry();
    const GoldSrcBaselineBundle baselines = Baselines();
    GoldSrcServerFrame base = Frame(40u, 1.0f);
    base.clientdata.state = Record(100u);
    base.entities.push_back(Entity(2u, 3u));
    base.entities.push_back(
        Entity(4u, 5u, GoldSrcBaselineKind::kCustomEntity));
    base.entities.push_back(Entity(6u, 7u));

    GoldSrcServerFrame current = Frame(41u, 1.05f);
    current.clientdata.state = Record(90u);
    current.entities.push_back(Entity(2u, 20u));
    current.entities.push_back(Entity(3u, 4u));
    current.entities.push_back(Entity(6u, 7u));

    const GoldSrcEntityDiffResult diff =
        CompareGoldSrcSnapshotEntities(base, current);
    assert(diff.ok());
    assert(diff.adds == 1u);
    assert(diff.updates == 1u);
    assert(diff.removes == 1u);
    assert(diff.unchanged == 1u);
    assert(diff.operations.size() == 4u);

    const GoldSrcSnapshotEncodeResult encoded =
        EncodeGoldSrcDeltaSnapshot(
            current,
            base,
            baselines,
            registry);
    assert(encoded.ok());
    assert(encoded.payload.bytes[0] == kGoldSrcServerTimeOpcode);
    const GoldSrcSnapshotDecodeResult decoded =
        DecodeGoldSrcDeltaSnapshot(
            encoded.payload.bytes.data(),
            encoded.payload.size,
            current.frame_id,
            base,
            baselines,
            registry);
    assert(decoded.ok());
    assert(decoded.frame.entities.size() == 3u);
    assert(decoded.frame.entities[0].entity_index == 2u);
    assert(
        decoded.frame.entities[0].state.values[0].unsigned_value
        == 20u);
    assert(decoded.frame.entities[1].entity_index == 3u);
    assert(decoded.frame.entities[2].entity_index == 6u);
    assert(
        decoded.frame.clientdata.state.values[0].unsigned_value
        == 90u);
    assert(
        DecodeGoldSrcDeltaSnapshot(
            encoded.payload.bytes.data(),
            encoded.payload.size - 1u,
            current.frame_id,
            base,
            baselines,
            registry).status
        != GoldSrcSnapshotCodecStatus::kOk);
    assert(
        EncodeGoldSrcDeltaSnapshot(
            current,
            base,
            baselines,
            registry,
            1u).status
        == GoldSrcSnapshotCodecStatus::kOutputCapacityExceeded);

    GoldSrcServerFrame incompatible = current;
    incompatible.entities[0].kind =
        GoldSrcBaselineKind::kCustomEntity;
    assert(
        CompareGoldSrcSnapshotEntities(base, incompatible).status
        == GoldSrcEntityDiffStatus::kIncompatibleEntityKind);
}

void TestBaseSelectionLossRecoveryAndEviction()
{
    const GoldSrcDeltaRegistry registry = Registry();
    const GoldSrcBaselineBundle baselines = Baselines();
    const GoldSrcSnapshotBuildResult first =
        BuildGoldSrcFirstSnapshot(100u, 1.0f, baselines, registry);
    assert(first.ok());

    GoldSrcClientFrameHistory history(3u);
    assert(history.Store(first.bundle.frame)
        == GoldSrcFrameStoreResult::kStored);
    assert(history.Acknowledge(100u)
        == GoldSrcFrameAcknowledgeResult::kAcknowledged);

    const GoldSrcContinuousSnapshotBuildResult lost =
        BuildGoldSrcContinuousSnapshot(
            101u,
            1.05f,
            history.last_acknowledged(),
            baselines,
            registry);
    assert(lost.ok());
    assert(lost.bundle.kind == GoldSrcSnapshotKind::kDelta);
    assert(lost.bundle.base_frame_id == 100u);
    assert(history.Store(
        lost.bundle.frame,
        lost.bundle.kind,
        lost.bundle.base_frame_id)
        == GoldSrcFrameStoreResult::kStored);

    const GoldSrcContinuousSnapshotBuildResult recovery =
        BuildGoldSrcContinuousSnapshot(
            102u,
            1.10f,
            history.last_acknowledged(),
            baselines,
            registry);
    assert(recovery.ok());
    assert(recovery.bundle.kind == GoldSrcSnapshotKind::kDelta);
    assert(recovery.bundle.base_frame_id == 100u);
    assert(history.Store(
        recovery.bundle.frame,
        recovery.bundle.kind,
        recovery.bundle.base_frame_id)
        == GoldSrcFrameStoreResult::kStored);
    assert(history.Acknowledge(102u)
        == GoldSrcFrameAcknowledgeResult::kAcknowledged);

    const GoldSrcContinuousSnapshotBuildResult later =
        BuildGoldSrcContinuousSnapshot(
            103u,
            1.15f,
            history.last_acknowledged(),
            baselines,
            registry);
    assert(later.ok());
    assert(later.bundle.base_frame_id == 102u);
    assert(history.Store(
        later.bundle.frame,
        later.bundle.kind,
        later.bundle.base_frame_id)
        == GoldSrcFrameStoreResult::kStored);
    assert(history.size() == history.capacity());

    GoldSrcClientFrameHistory fallback_history(2u);
    assert(fallback_history.Store(first.bundle.frame)
        == GoldSrcFrameStoreResult::kStored);
    assert(fallback_history.Acknowledge(100u)
        == GoldSrcFrameAcknowledgeResult::kAcknowledged);
    assert(fallback_history.Store(lost.bundle.frame)
        == GoldSrcFrameStoreResult::kStored);
    assert(fallback_history.Store(recovery.bundle.frame)
        == GoldSrcFrameStoreResult::kStored);
    assert(fallback_history.last_acknowledged() == nullptr);
    const GoldSrcContinuousSnapshotBuildResult fallback =
        BuildGoldSrcContinuousSnapshot(
            103u,
            1.15f,
            fallback_history.last_acknowledged(),
            baselines,
            registry);
    assert(fallback.ok());
    assert(fallback.bundle.kind == GoldSrcSnapshotKind::kFull);
    assert(!fallback.bundle.base_frame_id.has_value());
}

void TestContinuousSessionState()
{
    const GoldSrcDeltaRegistry registry = Registry();
    const GoldSrcBaselineBundle baselines = Baselines();
    GoldSrcSnapshotBuildResult first =
        BuildGoldSrcFirstSnapshot(20u, 1.0f, baselines, registry);
    assert(first.ok());
    GoldSrcFirstSnapshotSessionState session;
    assert(session.EnterAwaiting()
        == GoldSrcFirstSnapshotTransitionResult::kAdvanced);
    assert(session.Prepare(std::move(first.bundle))
        == GoldSrcFirstSnapshotTransitionResult::kAdvanced);
    assert(session.MarkSent()
        == GoldSrcFirstSnapshotTransitionResult::kAdvanced);
    assert(session.Acknowledge(20u)
        == GoldSrcFrameAcknowledgeResult::kAcknowledged);
    assert(session.StartContinuous(1.0, 20.0));
    assert(session.CheckContinuousDue(1.049)
        == GoldSrcSnapshotDueResult::kNotDue);
    assert(session.CheckContinuousDue(1.05)
        == GoldSrcSnapshotDueResult::kDue);

    GoldSrcContinuousSnapshotBuildResult second =
        BuildGoldSrcContinuousSnapshot(
            21u,
            1.05f,
            session.history().last_acknowledged(),
            baselines,
            registry);
    assert(second.ok());
    assert(session.MarkContinuousSent(second.bundle)
        == GoldSrcFrameStoreResult::kStored);
    assert(session.phase()
        == GoldSrcFirstSnapshotPhase::kContinuousSnapshotStreaming);
    assert(session.Acknowledge(21u)
        == GoldSrcFrameAcknowledgeResult::kAcknowledged);

    GoldSrcContinuousSnapshotBuildResult third =
        BuildGoldSrcContinuousSnapshot(
            22u,
            1.10f,
            session.history().last_acknowledged(),
            baselines,
            registry);
    assert(third.ok());
    assert(session.MarkContinuousSent(third.bundle)
        == GoldSrcFrameStoreResult::kStored);
    assert(session.Acknowledge(22u)
        == GoldSrcFrameAcknowledgeResult::kAcknowledged);
    assert(session.phase()
        == GoldSrcFirstSnapshotPhase::kContinuousSnapshotStable);
    session.Reset();
    assert(session.phase() == GoldSrcFirstSnapshotPhase::kNone);
    assert(!session.scheduler().state().enabled);
    assert(session.Acknowledge(22u)
        == GoldSrcFrameAcknowledgeResult::kInvalidPhase);
}
} // namespace

int main()
{
    TestScheduler();
    TestFrameReferenceResolutionAndWrap();
    TestEntityDiffAndDeltaCodec();
    TestBaseSelectionLossRecoveryAndEviction();
    TestContinuousSessionState();
    std::cout
        << "goldsrc_continuous_snapshot_tests: all checks passed\n";
    return 0;
}
