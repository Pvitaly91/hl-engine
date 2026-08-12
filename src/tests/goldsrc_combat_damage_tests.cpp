#include "network/goldsrc_combat.h"
#include "network/goldsrc_pmove.h"
#include "network/goldsrc_snapshot.h"

#include <cassert>
#include <array>
#include <cstdint>
#include <iostream>
#include <limits>

namespace
{
using namespace hl::network;

constexpr std::uint16_t kAttack = 1u << 0u;
constexpr std::uint16_t kJump = 1u << 1u;
constexpr std::uint16_t kDuck = 1u << 2u;
constexpr std::uint16_t kUse = 1u << 5u;
constexpr std::uint16_t kAttack2 = 1u << 11u;
constexpr std::uint16_t kReload = 1u << 13u;
constexpr std::uint8_t kGlockWeaponIndex = 2u;
constexpr int kFixtureGlockDamage = 12;
constexpr std::uint64_t kFixtureGlockCooldownMsec = 300u;

// This is a deterministic Game DLL callback fixture, not engine-side weapon
// behavior. Production combat continues to consume the inventory, damage and
// cooldown values produced by the installed Game DLL.
struct FixtureGlockInventory final
{
    std::uint8_t active_weapon = kGlockWeaponIndex;
    int clip = 17;
    int reserve_ammo = 68;
    std::uint64_t next_primary_attack_msec = 0u;
};

struct FixtureCombatPlayer final
{
    int slot = 0;
    std::uint64_t generation = 0u;
    bool connected = true;
    int health = 100;
    FixtureGlockInventory inventory;
};

enum class FixtureShotOutcome
{
    kPlayerHit,
    kMiss,
    kWorldBlocked,
    kCooldown,
    kShooterDisconnected,
    kInvalidWeapon,
    kEmptyClip,
};

struct FixtureShotObservation final
{
    FixtureShotOutcome outcome = FixtureShotOutcome::kMiss;
    bool fired = false;
    bool damage_applied = false;
    int clip_before = 0;
    int clip_after = 0;
    int target_health_before = 0;
    int target_health_after = 0;
};

FixtureCombatPlayer FixturePlayer(
    int slot,
    std::uint64_t generation,
    int clip,
    int reserve_ammo)
{
    FixtureCombatPlayer player;
    player.slot = slot;
    player.generation = generation;
    player.inventory.clip = clip;
    player.inventory.reserve_ammo = reserve_ammo;
    return player;
}

FixtureShotObservation ObserveFixturePrimaryAttack(
    FixtureCombatPlayer* shooter,
    FixtureCombatPlayer* target,
    const GoldSrcCombatTraceSelection& trace,
    std::uint64_t command_time_msec)
{
    assert(shooter != nullptr);
    FixtureShotObservation observation;
    observation.clip_before = shooter->inventory.clip;
    observation.clip_after = shooter->inventory.clip;
    if (target != nullptr)
    {
        observation.target_health_before = target->health;
        observation.target_health_after = target->health;
    }
    if (!shooter->connected)
    {
        observation.outcome = FixtureShotOutcome::kShooterDisconnected;
        return observation;
    }
    if (shooter->inventory.active_weapon != kGlockWeaponIndex)
    {
        observation.outcome = FixtureShotOutcome::kInvalidWeapon;
        return observation;
    }
    if (command_time_msec < shooter->inventory.next_primary_attack_msec)
    {
        observation.outcome = FixtureShotOutcome::kCooldown;
        return observation;
    }
    if (shooter->inventory.clip <= 0)
    {
        observation.outcome = FixtureShotOutcome::kEmptyClip;
        return observation;
    }

    observation.fired = true;
    --shooter->inventory.clip;
    shooter->inventory.next_primary_attack_msec =
        command_time_msec + kFixtureGlockCooldownMsec;
    observation.clip_after = shooter->inventory.clip;

    if (trace.hit_type == GoldSrcCombatTraceHitType::kWorld)
    {
        observation.outcome = FixtureShotOutcome::kWorldBlocked;
        return observation;
    }
    if (trace.hit_type != GoldSrcCombatTraceHitType::kPlayer
        || target == nullptr
        || !target->connected
        || target == shooter
        || trace.player_slot != target->slot
        || trace.player_generation != target->generation)
    {
        observation.outcome = FixtureShotOutcome::kMiss;
        return observation;
    }

    assert(target->health > kFixtureGlockDamage);
    target->health -= kFixtureGlockDamage;
    observation.outcome = FixtureShotOutcome::kPlayerHit;
    observation.damage_applied = true;
    observation.target_health_after = target->health;
    return observation;
}

GoldSrcDeltaField IntegerField(const char* name, std::uint8_t bits = 16u)
{
    GoldSrcDeltaField field;
    field.field_type = kGoldSrcDeltaTypeInteger;
    field.name = name;
    field.field_size = 4u;
    field.significant_bits = bits;
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
    registry.tables.push_back(Table("weapon_data_t", "m_iClip"));
    registry.tables.push_back(Table("usercmd_t", "msec"));
    registry.tables.push_back(Table("custom_entity_state_t", "modelindex"));
    registry.tables.push_back(Table("entity_state_player_t", "modelindex"));
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
    for (std::uint16_t index = 0u; index <= 2u; ++index)
    {
        GoldSrcEntityBaseline entity;
        entity.entity_index = index;
        entity.kind = index == 0u
            ? GoldSrcBaselineKind::kWorld
            : GoldSrcBaselineKind::kPlayer;
        entity.model_index = 1u;
        entity.state = Record(1u);
        baselines.entities.push_back(entity);
    }
    return baselines;
}

GoldSrcServerFrame Frame(
    std::uint32_t frame_id,
    float time,
    std::uint32_t health,
    std::uint32_t clip)
{
    GoldSrcServerFrame frame;
    frame.frame_id = frame_id;
    frame.server_time = time;
    frame.clientdata.state = Record(health);
    GoldSrcWeaponState glock;
    glock.weapon_index = 2u;
    glock.state = Record(clip);
    frame.weapons.push_back(glock);
    for (std::uint16_t index = 1u; index <= 2u; ++index)
    {
        GoldSrcSnapshotEntityState player;
        player.entity_index = index;
        player.kind = GoldSrcBaselineKind::kPlayer;
        player.state = Record(1u);
        frame.entities.push_back(player);
    }
    return frame;
}

GoldSrcCombatTraceCandidate Candidate(
    int slot,
    std::uint64_t generation,
    float minimum_x,
    float maximum_x)
{
    GoldSrcCombatTraceCandidate candidate;
    candidate.slot = slot;
    candidate.generation = generation;
    candidate.authoritative_generation = generation;
    candidate.connected = true;
    candidate.spawned = true;
    candidate.damageable = true;
    candidate.minimum = {{minimum_x, -4.0f, -18.0f}};
    candidate.maximum = {{maximum_x, 4.0f, 18.0f}};
    return candidate;
}

GoldSrcDecodedMoveCommand AttackMove()
{
    GoldSrcDecodedMoveCommand move;
    move.new_command_count = 1u;
    move.command_count = 1u;
    move.commands[0].msec = 10u;
    move.commands[0].buttons = kAttack;
    return move;
}

GoldSrcDecodedMoveCommand MovementMove(
    std::uint16_t buttons,
    float forward,
    float side)
{
    GoldSrcDecodedMoveCommand move;
    move.new_command_count = 1u;
    move.command_count = 1u;
    move.commands[0].msec = 10u;
    move.commands[0].buttons = buttons;
    move.commands[0].forwardmove = forward;
    move.commands[0].sidemove = side;
    return move;
}

GoldSrcPlayerSnapshotInput SnapshotInput(
    std::uint16_t local_slot,
    std::uint32_t health,
    std::uint32_t clip,
    std::uint16_t remote_slot)
{
    GoldSrcPlayerSnapshotInput input;
    input.entity.entity_index = local_slot;
    input.entity.kind = GoldSrcBaselineKind::kPlayer;
    input.entity.state = Record(local_slot);
    input.clientdata.state = Record(health);
    input.weapons.push_back({kGlockWeaponIndex, Record(clip)});

    GoldSrcSnapshotEntityState remote;
    remote.entity_index = remote_slot;
    remote.kind = GoldSrcBaselineKind::kPlayer;
    remote.state = Record(remote_slot);
    input.remote_entities.push_back(remote);
    return input;
}

void TestInputAndFreshCommandGate()
{
    const auto disabled = FilterGoldSrcCombatInput(
        kAttack | kJump | kAttack2 | kReload | kUse,
        false,
        GoldSrcCombatPhase::kDisabled);
    assert(disabled.attack_received);
    assert(!disabled.attack_enabled);
    assert((disabled.buttons & kJump) != 0u);
    assert((disabled.buttons & (kAttack | kAttack2 | kReload | kUse)) == 0u);
    assert(disabled.attack2_masked);

    const auto waiting = FilterGoldSrcCombatInput(
        kAttack,
        true,
        GoldSrcCombatPhase::kAwaitingWeaponState);
    assert(!waiting.attack_enabled);
    const auto ready = FilterGoldSrcCombatInput(
        kAttack | kAttack2,
        true,
        GoldSrcCombatPhase::kCombatReady);
    assert(ready.attack_enabled);
    assert((ready.buttons & kAttack2) == 0u);

    GoldSrcCommandExecutionState state;
    auto move = AttackMove();
    const auto first = state.Plan(move, 10u, 1000u);
    assert(first.ok() && first.command_count == 1u);
    state.CommitObservedMovePacket(10u, true);
    state.CommitExecutedBatch(first, 10u, 1000u);
    assert(state.Plan(move, 10u, 1010u).status
        == GoldSrcCommandPlanStatus::kStalePacket);

    GoldSrcDecodedMoveCommand backup = move;
    backup.backup_command_count = 1u;
    backup.new_command_count = 1u;
    backup.command_count = 2u;
    backup.commands[1] = MovementMove(kJump, 200.0f, 25.0f).commands[0];
    const auto replay = state.Plan(backup, 11u, 1010u);
    assert(replay.ok());
    assert(replay.command_count == 1u);
    assert(replay.duplicate_backups_suppressed == 1u);
    assert((replay.commands[0].command.buttons & kAttack) == 0u);
    assert((replay.commands[0].command.buttons & kJump) != 0u);
    state.CommitObservedMovePacket(11u, true);
    state.CommitExecutedBatch(replay, 11u, 1010u);

    // Disconnect/slot reuse must clear the accepted packet frontier and the
    // last held buttons before the replacement player submits a command.
    state.Reset();
    assert(!state.initialized());
    assert(!state.has_last_command());
    assert(state.command_time_msec() == 0u);
    const auto after_reconnect_move = MovementMove(kJump, 180.0f, -20.0f);
    const auto after_reconnect = state.Plan(after_reconnect_move, 1u, 2000u);
    assert(after_reconnect.ok());
    assert(after_reconnect.command_count == 1u);
    assert((after_reconnect.commands[0].command.buttons & kAttack) == 0u);
    assert(after_reconnect.commands[0].command.forwardmove == 180.0f);
    state.CommitObservedMovePacket(1u, true);
    state.CommitExecutedBatch(after_reconnect, 1u, 2000u);
    assert(state.initialized());
    assert((state.last_command().buttons & kAttack) == 0u);
}

void TestStartFrameCadence()
{
    GoldSrcCombatFrameCadence cadence;
    assert(!cadence.BeginFrame(false, 1u));
    assert(cadence.BeginFrame(true, 1u));
    assert(!cadence.BeginFrame(true, 1u));
    assert(cadence.BeginFrame(true, 2u));
    assert(!cadence.BeginFrame(true, 2u));
    assert(cadence.BeginFrame(true, 25u));
    assert(cadence.call_count() == 3u);
    cadence.Reset();
    assert(cadence.call_count() == 0u);
    assert(cadence.BeginFrame(true, 25u));
}

void TestPlayerAwareTraceSelection()
{
    const std::array<float, 3> start{{0.0f, 0.0f, 0.0f}};
    const std::array<float, 3> end{{100.0f, 0.0f, 0.0f}};
    auto shooter = Candidate(1, 7u, -2.0f, 2.0f);
    auto target = Candidate(2, 9u, 20.0f, 24.0f);
    auto farther = Candidate(3, 11u, 40.0f, 44.0f);

    auto clear = SelectGoldSrcCombatLineHit(
        start, end, 1.0f, {shooter, target}, 1, 7u);
    assert(clear.hit_type == GoldSrcCombatTraceHitType::kPlayer);
    assert(clear.player_slot == 2);
    assert(clear.shooter_ignored);

    auto nearest = SelectGoldSrcCombatLineHit(
        start, end, 1.0f, {farther, target}, 0, 0u);
    assert(nearest.player_slot == 2);
    auto before_wall = SelectGoldSrcCombatLineHit(
        start, end, 0.30f, {target}, 0, 0u);
    assert(before_wall.hit_type == GoldSrcCombatTraceHitType::kPlayer);
    auto behind_wall = SelectGoldSrcCombatLineHit(
        start, end, 0.10f, {target}, 0, 0u);
    assert(behind_wall.hit_type == GoldSrcCombatTraceHitType::kWorld);
    assert(behind_wall.world_occluded);

    target.minimum[1] = 10.0f;
    target.maximum[1] = 14.0f;
    assert(SelectGoldSrcCombatLineHit(
        start, end, 1.0f, {target}, 0, 0u).hit_type
        == GoldSrcCombatTraceHitType::kNone);
    target = Candidate(2, 9u, 20.0f, 24.0f);
    target.connected = false;
    assert(SelectGoldSrcCombatLineHit(
        start, end, 1.0f, {target}, 0, 0u).hit_type
        == GoldSrcCombatTraceHitType::kNone);
    target.connected = true;
    target.authoritative_generation = 10u;
    assert(SelectGoldSrcCombatLineHit(
        start, end, 1.0f, {target}, 0, 0u).hit_type
        == GoldSrcCombatTraceHitType::kNone);

    auto ducked = Candidate(2, 12u, 20.0f, 24.0f);
    ducked.minimum[2] = -18.0f;
    ducked.maximum[2] = 18.0f;
    const std::array<float, 3> high_start{{0.0f, 0.0f, 24.0f}};
    const std::array<float, 3> high_end{{100.0f, 0.0f, 24.0f}};
    assert(SelectGoldSrcCombatLineHit(
        high_start, high_end, 1.0f, {ducked}, 0, 0u).hit_type
        == GoldSrcCombatTraceHitType::kNone);

    const auto isolated_a = SelectGoldSrcCombatLineHit(
        start, end, 1.0f, {Candidate(2, 20u, 10.0f, 12.0f)}, 1, 19u);
    const auto isolated_b = SelectGoldSrcCombatLineHit(
        start, end, 1.0f, {Candidate(1, 19u, 30.0f, 32.0f)}, 2, 20u);
    assert(isolated_a.player_slot == 2);
    assert(isolated_b.player_slot == 1);
}

void TestGlockStateAndDamageObservations()
{
    const std::array<float, 3> start{{0.0f, 0.0f, 0.0f}};
    const std::array<float, 3> end{{100.0f, 0.0f, 0.0f}};
    FixtureCombatPlayer player_a = FixturePlayer(1, 40u, 17, 68);
    FixtureCombatPlayer player_b = FixturePlayer(2, 50u, 13, 51);
    assert(player_a.inventory.active_weapon == kGlockWeaponIndex);
    assert(player_b.inventory.active_weapon == kGlockWeaponIndex);
    assert(player_a.inventory.clip != player_b.inventory.clip);
    assert(player_a.inventory.reserve_ammo != player_b.inventory.reserve_ammo);

    const auto clear_hit = SelectGoldSrcCombatLineHit(
        start,
        end,
        1.0f,
        {Candidate(1, 40u, -2.0f, 2.0f),
         Candidate(2, 50u, 20.0f, 24.0f)},
        1,
        40u);
    const FixtureShotObservation shot = ObserveFixturePrimaryAttack(
        &player_a, &player_b, clear_hit, 1000u);
    assert(shot.outcome == FixtureShotOutcome::kPlayerHit);
    assert(shot.fired && shot.damage_applied);
    assert(shot.clip_before == 17 && shot.clip_after == 16);
    assert(shot.target_health_before == 100);
    assert(shot.target_health_after == 88);
    assert(player_a.inventory.clip == 16);
    assert(player_a.inventory.reserve_ammo == 68);
    assert(player_a.inventory.next_primary_attack_msec == 1300u);
    assert(player_a.health == 100);
    assert(player_b.health == 88);
    assert(player_b.inventory.clip == 13);
    assert(player_b.inventory.reserve_ammo == 51);

    const FixtureShotObservation cooldown = ObserveFixturePrimaryAttack(
        &player_a, &player_b, clear_hit, 1100u);
    assert(cooldown.outcome == FixtureShotOutcome::kCooldown);
    assert(!cooldown.fired && !cooldown.damage_applied);
    assert(player_a.inventory.clip == 16);
    assert(player_b.health == 88);

    FixtureCombatPlayer miss_shooter = FixturePlayer(1, 60u, 17, 68);
    FixtureCombatPlayer miss_target = FixturePlayer(2, 61u, 13, 51);
    const auto miss_trace = SelectGoldSrcCombatLineHit(
        start, end, 1.0f, {}, 1, 60u);
    const auto miss = ObserveFixturePrimaryAttack(
        &miss_shooter, &miss_target, miss_trace, 2000u);
    assert(miss.outcome == FixtureShotOutcome::kMiss);
    assert(miss.fired && !miss.damage_applied);
    assert(miss_shooter.inventory.clip == 16);
    assert(miss_target.health == 100);

    FixtureCombatPlayer wall_shooter = FixturePlayer(1, 70u, 17, 68);
    FixtureCombatPlayer wall_target = FixturePlayer(2, 71u, 13, 51);
    const auto wall_trace = SelectGoldSrcCombatLineHit(
        start,
        end,
        0.10f,
        {Candidate(2, 71u, 20.0f, 24.0f)},
        1,
        70u);
    const auto wall = ObserveFixturePrimaryAttack(
        &wall_shooter, &wall_target, wall_trace, 3000u);
    assert(wall.outcome == FixtureShotOutcome::kWorldBlocked);
    assert(wall.fired && !wall.damage_applied);
    assert(wall_shooter.inventory.clip == 16);
    assert(wall_target.health == 100);

    FixtureCombatPlayer self = FixturePlayer(1, 80u, 17, 68);
    const auto self_trace = SelectGoldSrcCombatLineHit(
        start,
        end,
        1.0f,
        {Candidate(1, 80u, 20.0f, 24.0f)},
        1,
        80u);
    assert(self_trace.hit_type == GoldSrcCombatTraceHitType::kNone);
    assert(self_trace.shooter_ignored);
    const auto self_shot = ObserveFixturePrimaryAttack(
        &self, &self, self_trace, 4000u);
    assert(self_shot.outcome == FixtureShotOutcome::kMiss);
    assert(self_shot.fired && !self_shot.damage_applied);
    assert(self.health == 100);

    FixtureCombatPlayer disconnected_shooter =
        FixturePlayer(1, 90u, 17, 68);
    FixtureCombatPlayer disconnected_target =
        FixturePlayer(2, 91u, 13, 51);
    const auto pending_target_trace = SelectGoldSrcCombatLineHit(
        start,
        end,
        1.0f,
        {Candidate(2, 91u, 20.0f, 24.0f)},
        1,
        90u);
    assert(pending_target_trace.hit_type
        == GoldSrcCombatTraceHitType::kPlayer);
    disconnected_target.connected = false;
    const auto disconnected = ObserveFixturePrimaryAttack(
        &disconnected_shooter,
        &disconnected_target,
        pending_target_trace,
        5000u);
    assert(disconnected.outcome == FixtureShotOutcome::kMiss);
    assert(disconnected.fired && !disconnected.damage_applied);
    assert(disconnected_target.health == 100);

    disconnected_shooter.connected = false;
    const int disconnected_clip = disconnected_shooter.inventory.clip;
    const auto stale_attacker = ObserveFixturePrimaryAttack(
        &disconnected_shooter, &player_b, clear_hit, 6000u);
    assert(stale_attacker.outcome
        == FixtureShotOutcome::kShooterDisconnected);
    assert(!stale_attacker.fired);
    assert(disconnected_shooter.inventory.clip == disconnected_clip);

    // A replacement generation receives a fresh Game DLL-owned inventory and
    // no inherited cooldown, while B's state remains untouched.
    player_a = FixturePlayer(1, 41u, 17, 68);
    assert(player_a.inventory.clip == 17);
    assert(player_a.inventory.next_primary_attack_msec == 0u);
    assert(player_b.inventory.clip == 13);
    assert(player_b.health == 88);

    FixtureCombatPlayer invalid_weapon = FixturePlayer(1, 100u, 17, 68);
    invalid_weapon.inventory.active_weapon = 3u;
    const auto invalid = ObserveFixturePrimaryAttack(
        &invalid_weapon, &player_b, clear_hit, 7000u);
    assert(invalid.outcome == FixtureShotOutcome::kInvalidWeapon);
    assert(!invalid.fired);
    assert(invalid_weapon.inventory.clip == 17);
}

void TestWeaponAndHealthSnapshotRoundTrip()
{
    const auto registry = Registry();
    const auto baselines = Baselines();
    const GoldSrcServerFrame base = Frame(100u, 10.0f, 100u, 17u);
    const auto full = EncodeGoldSrcFirstSnapshot(base, baselines, registry);
    assert(full.ok());
    const auto decoded_full = DecodeGoldSrcFirstSnapshot(
        full.payload.bytes.data(), full.payload.size, 100u, baselines, registry);
    assert(decoded_full.ok());
    assert(decoded_full.frame.weapons.size() == 1u);
    assert(decoded_full.frame.weapons[0].weapon_index == 2u);
    assert(decoded_full.frame.weapons[0].state.values[0].unsigned_value == 17u);

    const GoldSrcServerFrame current = Frame(101u, 10.05f, 88u, 16u);
    const auto delta = EncodeGoldSrcDeltaSnapshot(
        current, base, baselines, registry);
    assert(delta.ok());
    const auto reconstructed = DecodeGoldSrcDeltaSnapshot(
        delta.payload.bytes.data(), delta.payload.size, 101u,
        base, baselines, registry);
    assert(reconstructed.ok());
    assert(reconstructed.frame.clientdata.state.values[0].unsigned_value == 88u);
    assert(reconstructed.frame.weapons.size() == 1u);
    assert(reconstructed.frame.weapons[0].state.values[0].unsigned_value == 16u);

    const GoldSrcServerFrame dying = Frame(102u, 10.10f, 0u, 15u);
    const auto dying_delta = EncodeGoldSrcDeltaSnapshot(
        dying, current, baselines, registry);
    assert(dying_delta.ok());
    const auto decoded_dying = DecodeGoldSrcDeltaSnapshot(
        dying_delta.payload.bytes.data(),
        dying_delta.payload.size,
        102u,
        current,
        baselines,
        registry);
    assert(decoded_dying.ok());
    assert(decoded_dying.frame.clientdata.state.values[0].unsigned_value == 0u);
    assert(decoded_dying.frame.weapons.size() == 1u);
    assert(decoded_dying.frame.weapons[0].state.values[0].unsigned_value == 15u);

    auto dead_player = SnapshotInput(1u, 0u, 0u, 2u);
    dead_player.weapons.clear();
    const auto death_transition = BuildGoldSrcContinuousSnapshot(
        103u,
        10.15f,
        &dying,
        baselines,
        registry,
        kGoldSrcMaximumSnapshotBytes,
        &dead_player);
    assert(death_transition.ok());
    assert(death_transition.bundle.kind == GoldSrcSnapshotKind::kFull);
    assert(!death_transition.bundle.base_frame_id.has_value());
    const auto decoded_death = DecodeGoldSrcFirstSnapshot(
        death_transition.bundle.payload.bytes.data(),
        death_transition.bundle.payload.size,
        103u,
        baselines,
        registry);
    assert(decoded_death.ok());
    assert(decoded_death.frame.clientdata.state.values[0].unsigned_value == 0u);
    assert(decoded_death.frame.weapons.empty());

    GoldSrcClientFrameHistory client_a(4u);
    GoldSrcClientFrameHistory client_b(4u);
    assert(client_a.Store(base) == GoldSrcFrameStoreResult::kStored);
    assert(client_b.Store(Frame(200u, 20.0f, 100u, 13u))
        == GoldSrcFrameStoreResult::kStored);
    assert(client_a.Find(100u) != nullptr);
    assert(client_a.Find(200u) == nullptr);
    assert(client_b.Find(200u)->weapons[0].state.values[0].unsigned_value
        == 13u);

    GoldSrcServerFrame applied;
    GoldSrcPlayerSnapshotInput player;
    player.entity.entity_index = 1u;
    player.entity.kind = GoldSrcBaselineKind::kPlayer;
    player.entity.state = Record(1u);
    player.clientdata.state = Record(100u);
    player.weapons.push_back({2u, Record(17u)});
    player.weapons.push_back({2u, Record(16u)});
    assert(ApplyGoldSrcPlayerSnapshot(&applied, player, 2u)
        == GoldSrcPlayerSnapshotApplyStatus::kInvalidWeaponData);
}

void TestPerClientReplicationLossRecoveryAndAckIsolation()
{
    const auto registry = Registry();
    const auto baselines = Baselines();
    const auto player_a_initial = SnapshotInput(1u, 100u, 17u, 2u);
    const auto player_b_initial = SnapshotInput(2u, 100u, 13u, 1u);
    const auto first_a = BuildGoldSrcFirstSnapshot(
        100u,
        10.0f,
        baselines,
        registry,
        kGoldSrcMaximumSnapshotBytes,
        &player_a_initial);
    const auto first_b = BuildGoldSrcFirstSnapshot(
        200u,
        20.0f,
        baselines,
        registry,
        kGoldSrcMaximumSnapshotBytes,
        &player_b_initial);
    assert(first_a.ok() && first_b.ok());
    assert(first_a.bundle.frame.weapons.size() == 1u);
    assert(first_b.bundle.frame.weapons.size() == 1u);
    assert(first_a.bundle.frame.weapons[0].state.values[0].unsigned_value
        == 17u);
    assert(first_b.bundle.frame.weapons[0].state.values[0].unsigned_value
        == 13u);

    GoldSrcClientFrameHistory history_a(4u);
    GoldSrcClientFrameHistory history_b(4u);
    assert(history_a.Store(first_a.bundle.frame)
        == GoldSrcFrameStoreResult::kStored);
    assert(history_b.Store(first_b.bundle.frame)
        == GoldSrcFrameStoreResult::kStored);
    assert(history_a.Acknowledge(static_cast<std::uint8_t>(100u))
        == GoldSrcFrameAcknowledgeResult::kAcknowledged);
    assert(history_b.Acknowledge(static_cast<std::uint8_t>(200u))
        == GoldSrcFrameAcknowledgeResult::kAcknowledged);
    assert(history_a.last_acknowledged_frame() == 100u);
    assert(history_b.last_acknowledged_frame() == 200u);

    const auto player_a_after_shot = SnapshotInput(1u, 100u, 16u, 2u);
    const auto player_b_after_hit = SnapshotInput(2u, 88u, 13u, 1u);
    const auto delta_a = BuildGoldSrcContinuousSnapshot(
        101u,
        10.05f,
        history_a.last_acknowledged(),
        baselines,
        registry,
        kGoldSrcMaximumSnapshotBytes,
        &player_a_after_shot);
    const auto delta_b = BuildGoldSrcContinuousSnapshot(
        201u,
        20.05f,
        history_b.last_acknowledged(),
        baselines,
        registry,
        kGoldSrcMaximumSnapshotBytes,
        &player_b_after_hit);
    assert(delta_a.ok() && delta_b.ok());
    assert(delta_a.bundle.kind == GoldSrcSnapshotKind::kDelta);
    assert(delta_b.bundle.kind == GoldSrcSnapshotKind::kDelta);
    assert(delta_a.bundle.base_frame_id == 100u);
    assert(delta_b.bundle.base_frame_id == 200u);

    const auto decoded_a = DecodeGoldSrcDeltaSnapshot(
        delta_a.bundle.payload.bytes.data(),
        delta_a.bundle.payload.size,
        101u,
        first_a.bundle.frame,
        baselines,
        registry);
    const auto decoded_b = DecodeGoldSrcDeltaSnapshot(
        delta_b.bundle.payload.bytes.data(),
        delta_b.bundle.payload.size,
        201u,
        first_b.bundle.frame,
        baselines,
        registry);
    assert(decoded_a.ok() && decoded_b.ok());
    assert(decoded_a.frame.clientdata.state.values[0].unsigned_value == 100u);
    assert(decoded_b.frame.clientdata.state.values[0].unsigned_value == 88u);
    assert(decoded_a.frame.weapons.size() == 1u);
    assert(decoded_b.frame.weapons.size() == 1u);
    assert(decoded_a.frame.weapons[0].state.values[0].unsigned_value == 16u);
    assert(decoded_b.frame.weapons[0].state.values[0].unsigned_value == 13u);

    assert(history_a.Store(
        delta_a.bundle.frame,
        delta_a.bundle.kind,
        delta_a.bundle.base_frame_id) == GoldSrcFrameStoreResult::kStored);
    assert(history_b.Store(
        delta_b.bundle.frame,
        delta_b.bundle.kind,
        delta_b.bundle.base_frame_id) == GoldSrcFrameStoreResult::kStored);
    assert(history_a.Find(201u) == nullptr);
    assert(history_b.Find(101u) == nullptr);

    // A's unacknowledged frame is treated as lost, so its next delta remains
    // based on A's last acknowledged frame rather than B's newer ACK.
    assert(history_b.Acknowledge(static_cast<std::uint8_t>(201u))
        == GoldSrcFrameAcknowledgeResult::kAcknowledged);
    assert(history_a.last_acknowledged_frame() == 100u);
    assert(history_b.last_acknowledged_frame() == 201u);
    const auto player_a_after_loss = SnapshotInput(1u, 100u, 15u, 2u);
    const auto loss_recovery = BuildGoldSrcContinuousSnapshot(
        102u,
        10.10f,
        history_a.last_acknowledged(),
        baselines,
        registry,
        kGoldSrcMaximumSnapshotBytes,
        &player_a_after_loss);
    assert(loss_recovery.ok());
    assert(loss_recovery.bundle.kind == GoldSrcSnapshotKind::kDelta);
    assert(loss_recovery.bundle.base_frame_id == 100u);
    const auto recovered = DecodeGoldSrcDeltaSnapshot(
        loss_recovery.bundle.payload.bytes.data(),
        loss_recovery.bundle.payload.size,
        102u,
        first_a.bundle.frame,
        baselines,
        registry);
    assert(recovered.ok());
    assert(recovered.frame.clientdata.state.values[0].unsigned_value == 100u);
    assert(recovered.frame.weapons[0].state.values[0].unsigned_value == 15u);

    GoldSrcClientFrameHistory fallback_history(2u);
    assert(fallback_history.Store(first_a.bundle.frame)
        == GoldSrcFrameStoreResult::kStored);
    assert(fallback_history.Acknowledge(static_cast<std::uint8_t>(100u))
        == GoldSrcFrameAcknowledgeResult::kAcknowledged);
    assert(fallback_history.Store(
        delta_a.bundle.frame,
        delta_a.bundle.kind,
        delta_a.bundle.base_frame_id) == GoldSrcFrameStoreResult::kStored);
    assert(fallback_history.Store(
        loss_recovery.bundle.frame,
        loss_recovery.bundle.kind,
        loss_recovery.bundle.base_frame_id)
        == GoldSrcFrameStoreResult::kStored);
    assert(fallback_history.last_acknowledged() == nullptr);

    const auto fallback_input = SnapshotInput(1u, 100u, 15u, 2u);
    const auto fallback = BuildGoldSrcContinuousSnapshot(
        103u,
        10.15f,
        fallback_history.last_acknowledged(),
        baselines,
        registry,
        kGoldSrcMaximumSnapshotBytes,
        &fallback_input);
    assert(fallback.ok());
    assert(fallback.bundle.kind == GoldSrcSnapshotKind::kFull);
    assert(!fallback.bundle.base_frame_id.has_value());
    const auto decoded_fallback = DecodeGoldSrcFirstSnapshot(
        fallback.bundle.payload.bytes.data(),
        fallback.bundle.payload.size,
        103u,
        baselines,
        registry);
    assert(decoded_fallback.ok());
    assert(decoded_fallback.frame.clientdata.state.values[0].unsigned_value
        == 100u);
    assert(decoded_fallback.frame.weapons[0].state.values[0].unsigned_value
        == 15u);

    assert(history_a.Acknowledge(static_cast<std::uint8_t>(101u))
        == GoldSrcFrameAcknowledgeResult::kAcknowledged);
    assert(history_a.last_acknowledged_frame() == 101u);
    assert(history_b.last_acknowledged_frame() == 201u);
}

void TestMovementAndRemoteInterpolationRegression()
{
    const auto combat_move_input = FilterGoldSrcCombatInput(
        kAttack | kAttack2 | kJump | kDuck,
        true,
        GoldSrcCombatPhase::kCombatReady);
    assert(combat_move_input.attack_enabled);
    assert((combat_move_input.buttons & kAttack) != 0u);
    assert((combat_move_input.buttons & kJump) != 0u);
    assert((combat_move_input.buttons & kDuck) != 0u);
    assert((combat_move_input.buttons & kAttack2) == 0u);

    const auto feature_off_input = FilterGoldSrcCombatInput(
        kAttack | kJump | kDuck,
        false,
        GoldSrcCombatPhase::kDisabled);
    assert(!feature_off_input.attack_enabled);
    assert((feature_off_input.buttons & kAttack) == 0u);
    assert((feature_off_input.buttons & kJump) != 0u);
    assert((feature_off_input.buttons & kDuck) != 0u);

    GoldSrcCommandExecutionState commands_a;
    GoldSrcCommandExecutionState commands_b;
    const auto movement_a = MovementMove(
        combat_move_input.buttons, 240.0f, -35.0f);
    const auto movement_b = MovementMove(
        feature_off_input.buttons, 180.0f, 45.0f);
    const auto plan_a = commands_a.Plan(movement_a, 10u, 1000u);
    const auto plan_b = commands_b.Plan(movement_b, 20u, 1000u);
    assert(plan_a.ok() && plan_b.ok());
    assert(plan_a.commands[0].command.forwardmove == 240.0f);
    assert(plan_a.commands[0].command.sidemove == -35.0f);
    assert(plan_b.commands[0].command.forwardmove == 180.0f);
    assert(plan_b.commands[0].command.sidemove == 45.0f);
    assert((plan_b.commands[0].command.buttons & (kJump | kDuck))
        == (kJump | kDuck));
    commands_a.CommitObservedMovePacket(10u, true);
    commands_a.CommitExecutedBatch(plan_a, 10u, 1000u);
    commands_b.CommitObservedMovePacket(20u, true);
    commands_b.CommitExecutedBatch(plan_b, 20u, 1000u);
    assert(commands_a.command_time_msec() == 10u);
    assert(commands_b.command_time_msec() == 10u);
    commands_a.Reset();
    assert(!commands_a.initialized());
    assert(commands_b.initialized());
    assert(commands_b.last_accepted_packet_sequence() == 20u);

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

void TestDeathAwareWeaponPresence()
{
    assert(GoldSrcCombatWeaponPresenceMatches(100.0, true, true));
    assert(!GoldSrcCombatWeaponPresenceMatches(100.0, false, false));
    assert(!GoldSrcCombatWeaponPresenceMatches(100.0, true, false));
    assert(!GoldSrcCombatWeaponPresenceMatches(100.0, false, true));

    assert(GoldSrcCombatWeaponPresenceMatches(0.0, false, false));
    assert(GoldSrcCombatWeaponPresenceMatches(-8.0, false, false));
    assert(GoldSrcCombatWeaponPresenceMatches(-8.0, true, true));
    assert(!GoldSrcCombatWeaponPresenceMatches(-8.0, true, false));
    assert(!GoldSrcCombatWeaponPresenceMatches(-8.0, false, true));
    assert(!GoldSrcCombatWeaponPresenceMatches(
        std::numeric_limits<double>::quiet_NaN(),
        false,
        false));
}
} // namespace

int main()
{
    TestInputAndFreshCommandGate();
    TestStartFrameCadence();
    TestPlayerAwareTraceSelection();
    TestGlockStateAndDamageObservations();
    TestWeaponAndHealthSnapshotRoundTrip();
    TestPerClientReplicationLossRecoveryAndAckIsolation();
    TestMovementAndRemoteInterpolationRegression();
    TestDeathAwareWeaponPresence();
    std::cout << "goldsrc_combat_damage_unit_tests: pass\n";
    return 0;
}
