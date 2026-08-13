#include "game_api/goldsrc_pmove_runtime.h"
#include "network/goldsrc_netchan.h"
#include "network/goldsrc_pmove.h"
#include "network/goldsrc_snapshot.h"
#include "server_bootstrap.h"
#include "world_bootstrap.h"

#include <array>
#include <cassert>
#include <cmath>
#include <cstring>
#include <cstdint>

#pragma warning(push, 0)
#include "extdll.h"
#include "entity_state.h"
#include "in_buttons.h"
#include "pm_defs.h"
#pragma warning(pop)

namespace
{
void TestClientDataCallbackBufferContract()
{
    clientdata_t clientdata;
    std::memset(&clientdata, 0x5a, sizeof(clientdata));

    hl::game_api::detail::PrepareGoldSrcClientDataForGameDll(&clientdata);
    const auto* const callback_bytes =
        reinterpret_cast<const unsigned char*>(&clientdata);
    for (std::size_t index = 0u; index < sizeof(clientdata); ++index)
    {
        assert(callback_bytes[index] == 0u);
    }

    // Model the stock Glock UpdateClientData path: the Game DLL owns these
    // weapon fields, while RPG-only extension components remain untouched.
    clientdata.m_iId = 2;
    clientdata.vuser1.x = 68.0f;
    clientdata.vuser2.x = 0.0f;
    clientdata.vuser4.x = 17.0f;

    assert(clientdata.m_iId == 2);
    assert(clientdata.vuser1.x == 68.0f);
    assert(clientdata.vuser2.y == 0.0f);
    assert(clientdata.vuser2.z == 0.0f);
    assert(clientdata.vuser3.x == 0.0f);
    assert(clientdata.vuser3.y == 0.0f);
    assert(clientdata.vuser4.x == 17.0f);
}

int g_pm_init_calls = 0;
int g_pm_move_calls = 0;
int g_cmd_start_calls = 0;
int g_cmd_end_calls = 0;
bool g_emit_invalid_output = false;

enum class CombatCallbackEvent
{
    kCmdStart,
    kPlayerPreThink,
    kPmMove,
    kPlayerPostThink,
    kCmdEnd,
};

struct CombatCallbackObservation final
{
    CombatCallbackEvent event = CombatCallbackEvent::kCmdStart;
    float global_time = 0.0f;
    float global_frametime = 0.0f;
    int active_attack_slot = 0;
    std::uint16_t buttons = 0u;
    float pmove_time_msec = 0.0f;
};

std::array<CombatCallbackObservation, 128> g_combat_observations{};
std::size_t g_combat_observation_count = 0u;
float* g_combat_global_time = nullptr;
float* g_combat_global_frametime = nullptr;
int* g_combat_active_attack_slot = nullptr;
bool g_combat_emit_invalid_output = false;
bool g_combat_fail_cmd_end = false;
bool g_combat_respawn_in_prethink = false;
edict_t* g_combat_side_effect_target = nullptr;
int* g_combat_private_weapon_state = nullptr;

void ObserveCombatCallback(
    CombatCallbackEvent event,
    std::uint16_t buttons) noexcept
{
    assert(g_combat_observation_count < g_combat_observations.size());
    CombatCallbackObservation& observation =
        g_combat_observations[g_combat_observation_count++];
    observation.event = event;
    observation.global_time = g_combat_global_time != nullptr
        ? *g_combat_global_time
        : 0.0f;
    observation.global_frametime = g_combat_global_frametime != nullptr
        ? *g_combat_global_frametime
        : 0.0f;
    observation.active_attack_slot = g_combat_active_attack_slot != nullptr
        ? *g_combat_active_attack_slot
        : 0;
    observation.buttons = buttons;
}

void MockCombatPmInit(playermove_t*)
{
}

void MockCombatPmMove(playermove_t* context, int server)
{
    assert(context != nullptr);
    assert(server == TRUE);
    ObserveCombatCallback(
        CombatCallbackEvent::kPmMove,
        context->cmd.buttons);
    g_combat_observations[g_combat_observation_count - 1u]
        .pmove_time_msec = context->time;
    if (g_combat_emit_invalid_output)
    {
        context->velocity[0] = 1000000.0f;
    }
}

void MockCombatCmdStart(
    const edict_t*,
    const usercmd_t* command,
    unsigned int)
{
    assert(command != nullptr);
    ObserveCombatCallback(
        CombatCallbackEvent::kCmdStart,
        command->buttons);
}

void MockCombatPlayerPreThink(edict_t* player)
{
    assert(player != nullptr);
    ObserveCombatCallback(
        CombatCallbackEvent::kPlayerPreThink,
        static_cast<std::uint16_t>(player->v.button));
    if (g_combat_respawn_in_prethink
        && player->v.deadflag != DEAD_NO
        && (player->v.button & IN_ATTACK) != 0)
    {
        player->v.button = 0;
        player->v.deadflag = DEAD_NO;
        player->v.health = 100.0f;
        player->v.movetype = MOVETYPE_WALK;
    }
}

void MockCombatPlayerPostThink(edict_t* player)
{
    assert(player != nullptr);
    ObserveCombatCallback(
        CombatCallbackEvent::kPlayerPostThink,
        static_cast<std::uint16_t>(player->v.button));
    if (g_combat_side_effect_target != nullptr)
    {
        g_combat_side_effect_target->v.health -= 10.0f;
        player->v.iuser4 += 1;
    }
    if (g_combat_private_weapon_state != nullptr)
    {
        --*g_combat_private_weapon_state;
    }
}

void MockCombatCmdEnd(const edict_t* player)
{
    assert(player != nullptr);
    ObserveCombatCallback(
        CombatCallbackEvent::kCmdEnd,
        static_cast<std::uint16_t>(player->v.button));
#if defined(_MSC_VER)
    if (g_combat_fail_cmd_end)
    {
        RaiseException(
            EXCEPTION_NONCONTINUABLE_EXCEPTION,
            0u,
            0u,
            nullptr);
    }
#endif
}

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

void MockDueEntityThinkSetsKillMe(edict_t* entity)
{
    assert(entity != nullptr);
    entity->v.flags |= FL_KILLME;
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
    plane.normal = Vector(1.0f, 0.0f, 0.0f);
    plane.distance = 64.0f;
    plane.type = 0;
    world.collision_planes.push_back(plane);
    BspCollisionClipnode node;
    node.plane_index = 0;
    node.children = {CONTENTS_SOLID, CONTENTS_EMPTY};
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

void TestPersistentGameDllHelperClassification()
{
    using hl::game_api::detail::EntityStateSnapshot;
    using hl::game_api::detail::EngineStringPool;
    using hl::game_api::detail::IsPersistentGameDllHelper;

    EngineStringPool strings;
    strings.Reset();
    assert(strings.KnowsIndex(0));
    const string_t owned = strings.Alloc("owned");
    assert(strings.KnowsIndex(owned));
    const char external[] = "external_bodyque";
    const string_t external_index = static_cast<string_t>(
        static_cast<std::uint32_t>(
            reinterpret_cast<std::uintptr_t>(external)
            - reinterpret_cast<std::uintptr_t>(strings.Base())));
    assert(!strings.KnowsIndex(external_index));
    assert(strings.Describe(external_index) == external);
    assert(strings.KnowsIndex(external_index));

    EntityStateSnapshot helper;
    helper.index = 3;
    helper.free_flag = false;
    helper.in_use = true;
    helper.removed = false;
    helper.parse_index = -1;
    helper.private_data_present = true;
    helper.private_data_owned = true;
    assert(IsPersistentGameDllHelper(helper, 2));

    helper.classname = "bodyque";
    assert(IsPersistentGameDllHelper(helper, 2));

    EntityStateSnapshot candidate = helper;
    candidate.index = 2;
    assert(!IsPersistentGameDllHelper(candidate, 2));
    candidate = helper;
    candidate.in_use = false;
    assert(!IsPersistentGameDllHelper(candidate, 2));
    candidate = helper;
    candidate.removed = true;
    assert(!IsPersistentGameDllHelper(candidate, 2));
    candidate = helper;
    candidate.parse_index = 0;
    assert(!IsPersistentGameDllHelper(candidate, 2));
    candidate = helper;
    candidate.private_data_present = false;
    assert(!IsPersistentGameDllHelper(candidate, 2));
    candidate = helper;
    candidate.private_data_owned = false;
    assert(!IsPersistentGameDllHelper(candidate, 2));
}

void TestEdictPrivateDataRetirementAndReuseBarrier()
{
    using hl::game_api::detail::EdictStore;

    EdictStore feature_off_store;
    feature_off_store.Reset(1u, 1u);
    edict_t* const immediately_removed = feature_off_store.CreateEntity();
    assert(immediately_removed != nullptr);
    feature_off_store.RemoveEntity(immediately_removed);
    assert(feature_off_store.CreateEntity() == immediately_removed);

    EdictStore store;
    store.Reset(1u, 3u);
    edict_t* const world = store.World();
    assert(world != nullptr);
    assert(store.EntityOfOffset(0) == world);
    assert(store.OffsetOf(world) == 0);
    assert(world->v.pContainingEntity == world);
    edict_t* const client = store.EntityOfIndex(1);
    assert(client != nullptr);
    store.SetInUse(client, true);
    auto* const retired_player = static_cast<unsigned char*>(
        store.AllocatePrivateData(client, 64u));
    assert(retired_player != nullptr);
    retired_player[0] = 0x5au;
    client->v.health = 37.0f;

    store.PrepareClientForPutInServer(client);
    assert(store.EntityOfIndex(1) == client);
    assert(client->pvPrivateData == nullptr);
    assert(client->v.health == 0.0f);
    assert(client->v.pContainingEntity == client);

    auto* const fresh_player = static_cast<unsigned char*>(
        store.AllocatePrivateData(client, 64u));
    assert(fresh_player != nullptr);
    assert(fresh_player != retired_player);
    assert(retired_player[0] == 0x5au);

    store.BeginFrameReuseBarrier();
    edict_t* const removed = store.CreateEntity();
    assert(removed != nullptr);
    const int removed_index = store.IndexOf(removed);
    auto* const retired_entity_data = static_cast<unsigned char*>(
        store.AllocatePrivateData(removed, 32u));
    assert(retired_entity_data != nullptr);
    retired_entity_data[0] = 0xa5u;
    removed->v.nextthink = 1.0f;
    const float frame_time = 1.0f;
    assert(removed->v.nextthink <= frame_time);
    removed->v.nextthink = 0.0f;
    MockDueEntityThinkSetsKillMe(removed);
    assert((removed->v.flags & FL_KILLME) != 0);
    store.RemoveEntity(removed);
    assert(removed->free == TRUE);
    assert(removed->pvPrivateData == nullptr);
    assert(retired_entity_data[0] == 0xa5u);

    edict_t* const same_frame = store.CreateEntity();
    assert(same_frame != nullptr);
    assert(store.IndexOf(same_frame) != removed_index);

    store.BeginFrameReuseBarrier();
    edict_t* const next_frame = store.CreateEntity();
    assert(next_frame == removed);
    assert(store.IndexOf(next_frame) == removed_index);
    auto* const next_frame_entity_data = static_cast<unsigned char*>(
        store.AllocatePrivateData(next_frame, 32u));
    assert(next_frame_entity_data != nullptr);
    assert(next_frame_entity_data != retired_entity_data);
    assert(retired_entity_data[0] == 0xa5u);
}

void TestTransientMuzzleFlashCleanup()
{
    using hl::game_api::detail::EdictStore;

    EdictStore store;
    store.Reset(2u, 1u);
    edict_t* const world = store.World();
    edict_t* const player_a = store.EntityOfIndex(1);
    edict_t* const player_b = store.EntityOfIndex(2);
    edict_t* const non_player = store.CreateEntity();
    assert(world != nullptr);
    assert(player_a != nullptr);
    assert(player_b != nullptr);
    assert(non_player != nullptr);

    store.SetInUse(player_a, true);
    store.SetInUse(player_b, true);
    world->v.effects = EF_MUZZLEFLASH;
    player_a->v.effects = EF_MUZZLEFLASH | EF_BRIGHTLIGHT | EF_NOINTERP;
    player_b->v.effects = EF_MUZZLEFLASH;
    non_player->v.effects = EF_MUZZLEFLASH | EF_DIMLIGHT;

    const int receiver_a_shot_frame_effects = player_a->v.effects;
    const int receiver_b_shot_frame_effects = player_a->v.effects;
    assert((receiver_a_shot_frame_effects & EF_MUZZLEFLASH) != 0);
    assert((receiver_b_shot_frame_effects & EF_MUZZLEFLASH) != 0);
    assert(store.ClearTransientMuzzleFlashEffects() == 3u);
    assert((world->v.effects & EF_MUZZLEFLASH) != 0);
    assert((player_a->v.effects & EF_MUZZLEFLASH) == 0);
    assert((player_a->v.effects & EF_BRIGHTLIGHT) != 0);
    assert((player_a->v.effects & EF_NOINTERP) != 0);
    assert((player_b->v.effects & EF_MUZZLEFLASH) == 0);
    assert((non_player->v.effects & EF_MUZZLEFLASH) == 0);
    assert((non_player->v.effects & EF_DIMLIGHT) != 0);
    assert(store.ClearTransientMuzzleFlashEffects() == 0u);

    player_a->v.effects |= EF_MUZZLEFLASH;
    assert((player_a->v.effects & EF_MUZZLEFLASH) != 0);
    assert(store.ClearTransientMuzzleFlashEffects() == 1u);
    assert((player_a->v.effects & EF_MUZZLEFLASH) == 0);
    assert((player_a->v.effects & EF_BRIGHTLIGHT) != 0);
    assert((player_a->v.effects & EF_NOINTERP) != 0);
}

void TestCombatCallbackOrderReadinessAndSplitTime()
{
    using namespace hl::game_api::detail;
    using namespace hl::network;

    GoldSrcPmoveRuntime runtime;
    assert(runtime.InitializeWorld(CollisionFixture()));

    float global_time = 0.0f;
    float global_frametime = 0.0f;
    int active_attack_slot = 0;
    g_combat_global_time = &global_time;
    g_combat_global_frametime = &global_frametime;
    g_combat_active_attack_slot = &active_attack_slot;
    g_combat_observation_count = 0u;

    GoldSrcPmoveGameDllCallbacks callbacks;
    callbacks.pm_init = &MockCombatPmInit;
    callbacks.pm_move = &MockCombatPmMove;
    callbacks.cmd_start = &MockCombatCmdStart;
    callbacks.cmd_end = &MockCombatCmdEnd;
    callbacks.player_pre_think = &MockCombatPlayerPreThink;
    callbacks.player_post_think = &MockCombatPlayerPostThink;
    callbacks.global_time = &global_time;
    callbacks.global_frametime = &global_frametime;
    callbacks.active_attack_postthink_slot = &active_attack_slot;
    callbacks.combat_enabled = true;
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
    player.v.view_ofs = Vector(0.0f, 0.0f, 28.0f);

    runtime.SetCombatPlayerReady(
        1u,
        true,
        kGoldSrcStockGlockWeaponId,
        true);
    assert(runtime.CombatDiagnostics(1u)->phase
        == GoldSrcCombatPhase::kCombatReady);

    auto attack = Move(0u, 1u);
    attack.commands[0] = Command(100u, 0.0f, 0.0f, IN_ATTACK);
    const GoldSrcPmoveSplitResult split =
        SplitGoldSrcPmoveCommand(attack.commands[0].msec);
    assert(split.valid);
    const GoldSrcPmoveExecutionResult attack_result =
        runtime.Execute(1u, attack, 1u, 1000u, &player, &world);
    assert(attack_result.ok());
    assert(!attack_result.gameplay_callback_failure);
    assert(attack_result.subcommands_executed == split.count);
    assert(g_combat_observation_count == split.count * 5u);

    std::uint64_t expected_time_msec = 1000u;
    float previous_prethink_time = 0.0f;
    for (std::size_t piece = 0u; piece < split.count; ++piece)
    {
        const std::size_t base = piece * 5u;
        assert(g_combat_observations[base].event
            == CombatCallbackEvent::kCmdStart);
        assert(g_combat_observations[base + 1u].event
            == CombatCallbackEvent::kPlayerPreThink);
        assert(g_combat_observations[base + 2u].event
            == CombatCallbackEvent::kPmMove);
        assert(g_combat_observations[base + 3u].event
            == CombatCallbackEvent::kPlayerPostThink);
        assert(g_combat_observations[base + 4u].event
            == CombatCallbackEvent::kCmdEnd);

        expected_time_msec += split.msec[piece];
        const float expected_time =
            static_cast<float>(expected_time_msec) / 1000.0f;
        const float expected_frametime =
            static_cast<float>(split.msec[piece]) / 1000.0f;
        const CombatCallbackObservation& prethink =
            g_combat_observations[base + 1u];
        const CombatCallbackObservation& pmove =
            g_combat_observations[base + 2u];
        const CombatCallbackObservation& postthink =
            g_combat_observations[base + 3u];
        assert(prethink.global_time > previous_prethink_time);
        assert(std::fabs(prethink.global_time - expected_time) < 0.0001f);
        assert(std::fabs(prethink.global_frametime - expected_frametime)
            < 0.0001f);
        assert(pmove.global_time == prethink.global_time);
        assert(std::fabs(
            pmove.pmove_time_msec
                - static_cast<float>(expected_time_msec)) < 0.0001f);
        assert(postthink.global_time == prethink.global_time);
        assert(postthink.active_attack_slot == 1);
        assert((g_combat_observations[base].buttons & IN_ATTACK) != 0u);
        previous_prethink_time = prethink.global_time;
    }
    const GoldSrcCombatClientDiagnostics attack_diagnostics =
        *runtime.CombatDiagnostics(1u);
    assert(attack_diagnostics.gameplay_time_msec == 1100u);
    assert(attack_diagnostics.player_prethink_calls == split.count);
    assert(attack_diagnostics.player_postthink_calls == split.count);
    assert(attack_diagnostics.attack_commands_executed == split.count);
    assert(attack_diagnostics.phase == GoldSrcCombatPhase::kCombatStable);
    assert(active_attack_slot == 0);

    const std::size_t callbacks_before_rejection =
        g_combat_observation_count;
    const float time_before_rejection = global_time;
    const GoldSrcPmoveExecutionResult duplicate =
        runtime.Execute(1u, attack, 1u, 1300u, &player, &world);
    assert(!duplicate.ok());
    assert(!duplicate.gameplay_callback_failure);
    assert(duplicate.status
        == GoldSrcPmoveExecutionStatus::kCommandPlanRejected);
    assert(g_combat_observation_count == callbacks_before_rejection);
    assert(global_time == time_before_rejection);
    assert(runtime.CombatDiagnostics(1u)->gameplay_time_msec == 1100u);

    edict_t player_two = player;
    runtime.SetCombatPlayerReady(
        2u,
        true,
        kGoldSrcStockGlockWeaponId,
        true);
    g_combat_observation_count = 0u;
    auto second_client_move = Move(0u, 1u);
    second_client_move.commands[0] = Command(10u, 0.0f);
    const GoldSrcPmoveExecutionResult second_client_result =
        runtime.Execute(
            2u,
            second_client_move,
            1u,
            1000u,
            &player_two,
            &world);
    assert(second_client_result.ok());
    assert(g_combat_observation_count == 5u);
    assert(std::fabs(g_combat_observations[1].global_time - 1.010f)
        < 0.0001f);
    assert(runtime.CombatDiagnostics(2u)->gameplay_time_msec == 1010u);

    runtime.ResetClient(1u);
    runtime.SetCombatPlayerReady(1u, true, 1, true);
    assert(runtime.CombatDiagnostics(1u)->phase
        == GoldSrcCombatPhase::kAwaitingWeaponState);
    g_combat_observation_count = 0u;
    auto masked_attack = Move(0u, 1u);
    masked_attack.commands[0] = Command(10u, 0.0f, 0.0f, IN_ATTACK);
    const GoldSrcPmoveExecutionResult masked_result =
        runtime.Execute(1u, masked_attack, 2u, 2000u, &player, &world);
    assert(masked_result.ok());
    assert(g_combat_observation_count == 5u);
    assert((g_combat_observations[0].buttons & IN_ATTACK) == 0u);
    assert(g_combat_observations[3].active_attack_slot == 0);
    const GoldSrcCombatClientDiagnostics* masked_diagnostics =
        runtime.CombatDiagnostics(1u);
    assert(masked_diagnostics->attack_commands_received == 1u);
    assert(masked_diagnostics->attack_commands_executed == 0u);
    assert(masked_diagnostics->phase
        == GoldSrcCombatPhase::kAwaitingWeaponState);

    edict_t untouched_target{};
    untouched_target.v.health = 77.0f;
    int untouched_private_weapon_state = 9;
    g_combat_side_effect_target = &untouched_target;
    g_combat_private_weapon_state = &untouched_private_weapon_state;
    g_combat_observation_count = 0u;
    g_combat_emit_invalid_output = true;
    const GoldSrcPmoveExecutionResult callback_failure =
        runtime.Execute(1u, masked_attack, 3u, 2020u, &player, &world);
    g_combat_emit_invalid_output = false;
    g_combat_side_effect_target = nullptr;
    g_combat_private_weapon_state = nullptr;
    assert(!callback_failure.ok());
    assert(callback_failure.status
        == GoldSrcPmoveExecutionStatus::kOutputInvalid);
    assert(callback_failure.gameplay_failure_stage
        == "output_validation");
    assert(callback_failure.gameplay_callback_failure);
    assert(callback_failure.gameplay_fail_stop);
    assert(!callback_failure.recoverable_rollback);
    assert(g_combat_observation_count == 4u);
    assert(g_combat_observations[0].event
        == CombatCallbackEvent::kCmdStart);
    assert(g_combat_observations[1].event
        == CombatCallbackEvent::kPlayerPreThink);
    assert(g_combat_observations[2].event
        == CombatCallbackEvent::kPmMove);
    assert(g_combat_observations[3].event
        == CombatCallbackEvent::kCmdEnd);
    assert(untouched_target.v.health == 77.0f);
    assert(untouched_private_weapon_state == 9);
    assert(runtime.CombatDiagnostics(1u)->gameplay_time_msec == 2030u);
    const GoldSrcCommandExecutionState* failed_state =
        runtime.CommandState(1u);
    assert(failed_state != nullptr);
    assert(failed_state->last_observed_packet_sequence() == 3u);
    assert(failed_state->last_validated_move_sequence() == 3u);
    assert(failed_state->last_executed_move_sequence() == 2u);

    const std::size_t callbacks_at_fail_stop =
        g_combat_observation_count;
    const GoldSrcPmoveExecutionResult retry_after_fail_stop =
        runtime.Execute(1u, masked_attack, 3u, 2030u, &player, &world);
    assert(!retry_after_fail_stop.ok());
    assert(retry_after_fail_stop.gameplay_failure_stage
        == "fail_stop_latched");
    assert(retry_after_fail_stop.gameplay_callback_failure);
    assert(retry_after_fail_stop.gameplay_fail_stop);
    assert(!retry_after_fail_stop.recoverable_rollback);
    const GoldSrcPmoveExecutionResult other_client_after_fail_stop =
        runtime.Execute(
            2u,
            second_client_move,
            2u,
            2030u,
            &player_two,
            &world);
    assert(!other_client_after_fail_stop.ok());
    assert(other_client_after_fail_stop.gameplay_callback_failure);
    assert(other_client_after_fail_stop.gameplay_fail_stop);
    assert(g_combat_observation_count == callbacks_at_fail_stop);

    runtime.SetCombatPlayerReady(
        1u,
        true,
        kGoldSrcStockGlockWeaponId,
        false);
    assert(runtime.CombatDiagnostics(1u)->phase
        == GoldSrcCombatPhase::kAwaitingWeaponState);
    runtime.SetCombatPlayerReady(
        1u,
        true,
        kGoldSrcStockGlockWeaponId,
        true);
    assert(runtime.CombatDiagnostics(1u)->phase
        == GoldSrcCombatPhase::kCombatReady);

    g_combat_global_time = nullptr;
    g_combat_global_frametime = nullptr;
    g_combat_active_attack_slot = nullptr;
}

void TestRepeatedDeadPlayerClickRespawn()
{
    using namespace hl::game_api::detail;
    using namespace hl::network;

    const auto initialize_runtime = [](
        GoldSrcPmoveRuntime& runtime,
        float* global_time,
        float* global_frametime,
        int* active_attack_slot)
    {
        assert(runtime.InitializeWorld(CollisionFixture()));
        GoldSrcPmoveGameDllCallbacks callbacks;
        callbacks.pm_init = &MockCombatPmInit;
        callbacks.pm_move = &MockCombatPmMove;
        callbacks.cmd_start = &MockCombatCmdStart;
        callbacks.cmd_end = &MockCombatCmdEnd;
        callbacks.player_pre_think = &MockCombatPlayerPreThink;
        callbacks.player_post_think = &MockCombatPlayerPostThink;
        callbacks.global_time = global_time;
        callbacks.global_frametime = global_frametime;
        callbacks.active_attack_postthink_slot = active_attack_slot;
        callbacks.combat_enabled = true;
        GoldSrcMovevarsConfig movevars;
        movevars.maximum_velocity = 2000.0f;
        assert(runtime.InitializeGameDll(callbacks, ".", movevars));
    };
    const auto initialize_player = [](edict_t& player)
    {
        std::memset(&player, 0, sizeof(player));
        player.free = FALSE;
        player.v.movetype = MOVETYPE_WALK;
        player.v.health = 100.0f;
        player.v.gravity = 1.0f;
        player.v.friction = 1.0f;
        player.v.maxspeed = 320.0f;
        player.v.flags = FL_ONGROUND;
        player.v.view_ofs = Vector(0.0f, 0.0f, 28.0f);
    };
    const auto mark_respawnable = [](edict_t& player)
    {
        player.v.deadflag = DEAD_RESPAWNABLE;
        player.v.health = -8.0f;
        player.v.movetype = MOVETYPE_NONE;
        player.v.button = 0;
        player.v.nextthink = 0.0f;
    };

    float global_time = 0.0f;
    float global_frametime = 0.0f;
    int active_attack_slot = 0;
    g_combat_global_time = &global_time;
    g_combat_global_frametime = &global_frametime;
    g_combat_active_attack_slot = &active_attack_slot;
    g_combat_respawn_in_prethink = true;

    GoldSrcPmoveRuntime runtime;
    initialize_runtime(
        runtime,
        &global_time,
        &global_frametime,
        &active_attack_slot);
    edict_t world{};
    world.free = FALSE;
    edict_t player{};
    initialize_player(player);

    // Establish the command clock before exercising the maximum legal msec
    // value; an initial 255 ms command is intentionally beyond the 250 ms
    // first-packet lead allowance.
    auto clock_seed = Move(0u, 1u);
    clock_seed.commands[0] = Command(10u, 0.0f, 0.0f, 0u);
    assert(runtime.Execute(
        1u,
        clock_seed,
        1u,
        1000u,
        &player,
        &world).ok());

    constexpr std::uint32_t kRespawnCycles = 32u;
    for (std::uint32_t cycle = 0u; cycle < kRespawnCycles; ++cycle)
    {
        mark_respawnable(player);
        if (cycle == 0u)
        {
            // A peer can kill this player before the readiness snapshot is
            // refreshed.  Even in that stale-ready state, the click is a
            // lifecycle input and IN_ATTACK2 remains the only masked bit.
            runtime.SetCombatPlayerReady(
                1u,
                true,
                kGoldSrcStockGlockWeaponId,
                true);
            assert(runtime.CombatDiagnostics(1u)->phase
                == GoldSrcCombatPhase::kCombatReady);
        }
        else
        {
            runtime.SetCombatPlayerReady(1u, true, 0, false);
            assert(runtime.CombatDiagnostics(1u)->phase
                == GoldSrcCombatPhase::kAwaitingWeaponState);
        }
        g_combat_observation_count = 0u;
        auto click = Move(0u, 1u);
        const std::uint16_t click_buttons = cycle == 0u
            ? static_cast<std::uint16_t>(IN_ATTACK | IN_ATTACK2)
            : static_cast<std::uint16_t>(IN_ATTACK);
        click.commands[0] = Command(
            255u,
            0.0f,
            0.0f,
            click_buttons);
        const GoldSrcPmoveSplitResult split =
            SplitGoldSrcPmoveCommand(click.commands[0].msec);
        assert(split.valid && split.count > 1u);
        const GoldSrcPmoveExecutionResult result = runtime.Execute(
            1u,
            click,
            cycle + 2u,
            1300u + static_cast<std::uint64_t>(cycle) * 300u,
            &player,
            &world);
        assert(result.ok());
        assert(result.subcommands_executed == split.count);
        assert(!result.gameplay_callback_failure);
        assert(player.v.deadflag == DEAD_NO);
        assert(player.v.health == 100.0f);
        assert((player.v.oldbuttons & IN_ATTACK) == 0);
        assert(runtime.CombatDiagnostics(1u)->phase
            == GoldSrcCombatPhase::kAwaitingWeaponState);
        assert(g_combat_observation_count == split.count * 5u);
        assert((g_combat_observations[0].buttons & IN_ATTACK) != 0u);
        assert((g_combat_observations[1].buttons & IN_ATTACK) != 0u);
        for (std::size_t piece = 0u; piece < split.count; ++piece)
        {
            const std::size_t base = piece * 5u;
            for (std::size_t event = 0u; event < 5u; ++event)
            {
                assert((g_combat_observations[base + event].buttons
                        & IN_ATTACK2) == 0u);
            }
            assert((g_combat_observations[base + 2u].buttons
                    & IN_ATTACK) == 0u);
            assert((g_combat_observations[base + 3u].buttons
                    & IN_ATTACK) == 0u);
            assert((g_combat_observations[base + 4u].buttons
                    & IN_ATTACK) == 0u);
            assert(g_combat_observations[base + 3u]
                .active_attack_slot == 0);
            if (piece > 0u)
            {
                assert((g_combat_observations[base].buttons
                        & IN_ATTACK) == 0u);
                assert((g_combat_observations[base + 1u].buttons
                        & IN_ATTACK) == 0u);
            }
        }
        assert(active_attack_slot == 0);
    }
    const GoldSrcCombatClientDiagnostics* forwarded =
        runtime.CombatDiagnostics(1u);
    assert(forwarded != nullptr);
    assert(forwarded->respawn_inputs_forwarded == kRespawnCycles);
    assert(forwarded->attack_commands_received == 0u);
    assert(forwarded->attack_commands_executed == 0u);
    assert(forwarded->duplicate_attack_commands_suppressed == 0u);
    assert(forwarded->unsupported_gameplay_inputs_masked == 1u);
    assert(forwarded->callback_failures == 0u);

    // The next packet repeats the raw respawn click as a backup.  It is
    // suppressed by command history and must not be reclassified as a
    // duplicate weapon attack now that the player is alive.
    auto release = Move(1u, 1u);
    release.commands[0] = Command(255u, 0.0f, 0.0f, IN_ATTACK);
    release.commands[1] = Command(10u, 0.0f, 0.0f, 0u);
    assert(runtime.Execute(
        1u,
        release,
        kRespawnCycles + 2u,
        11000u,
        &player,
        &world).ok());
    assert(runtime.CombatDiagnostics(1u)
        ->duplicate_attack_commands_suppressed == 0u);

    runtime.SetCombatPlayerReady(
        1u,
        true,
        kGoldSrcStockGlockWeaponId,
        true);
    assert(runtime.CombatDiagnostics(1u)->phase
        == GoldSrcCombatPhase::kCombatReady);
    edict_t target{};
    target.free = FALSE;
    target.v.health = 100.0f;
    int clip = 17;
    g_combat_side_effect_target = &target;
    g_combat_private_weapon_state = &clip;
    g_combat_observation_count = 0u;
    auto live_attack = Move(0u, 1u);
    live_attack.commands[0] = Command(10u, 0.0f, 0.0f, IN_ATTACK);
    const GoldSrcPmoveExecutionResult live_result = runtime.Execute(
        1u,
        live_attack,
        kRespawnCycles + 3u,
        11010u,
        &player,
        &world);
    assert(live_result.ok());
    assert(g_combat_observation_count == 5u);
    for (std::size_t index = 0u;
         index < g_combat_observation_count;
         ++index)
    {
        assert((g_combat_observations[index].buttons & IN_ATTACK) != 0u);
    }
    assert(g_combat_observations[3].active_attack_slot == 1);
    assert(target.v.health == 90.0f);
    assert(clip == 16);
    const GoldSrcCombatClientDiagnostics* live =
        runtime.CombatDiagnostics(1u);
    assert(live != nullptr);
    assert(live->respawn_inputs_forwarded == kRespawnCycles);
    assert(live->attack_commands_received == 1u);
    assert(live->attack_commands_executed == 1u);
    assert(live->phase == GoldSrcCombatPhase::kCombatStable);
    assert(live->callback_failures == 0u);
    assert(active_attack_slot == 0);

    g_combat_side_effect_target = nullptr;
    g_combat_private_weapon_state = nullptr;
    auto live_backup = Move(1u, 1u);
    live_backup.commands[0] = Command(
        10u,
        0.0f,
        0.0f,
        IN_ATTACK);
    live_backup.commands[1] = Command(10u, 0.0f, 0.0f, 0u);
    const GoldSrcPmoveExecutionResult live_backup_result = runtime.Execute(
        1u,
        live_backup,
        kRespawnCycles + 4u,
        11020u,
        &player,
        &world);
    assert(live_backup_result.ok());
    assert(live_backup_result.duplicates_suppressed == 1u);
    const GoldSrcCombatClientDiagnostics* after_live_backup =
        runtime.CombatDiagnostics(1u);
    assert(after_live_backup != nullptr);
    assert(after_live_backup->duplicate_attack_commands_suppressed == 1u);
    assert(after_live_backup->attack_commands_received == 1u);
    assert(after_live_backup->attack_commands_executed == 1u);
    assert(target.v.health == 90.0f);
    assert(clip == 16);

    // A stale raw packet is rejected before duplicate combat accounting.
    auto second_live_attack = Move(0u, 1u);
    second_live_attack.commands[0] = Command(
        10u,
        0.0f,
        0.0f,
        IN_ATTACK);
    const std::uint32_t second_attack_sequence =
        kRespawnCycles + 5u;
    assert(runtime.Execute(
        1u,
        second_live_attack,
        second_attack_sequence,
        11030u,
        &player,
        &world).ok());
    const GoldSrcPmoveExecutionResult stale_attack = runtime.Execute(
        1u,
        second_live_attack,
        second_attack_sequence,
        11030u,
        &player,
        &world);
    assert(!stale_attack.ok());
    assert(stale_attack.command_status
        == GoldSrcCommandPlanStatus::kStalePacket);
    assert(runtime.CombatDiagnostics(1u)
        ->duplicate_attack_commands_suppressed == 1u);

    // Multiple suppressed backups contain only one exact copy of the last
    // semantic attack, so combat accounting advances by exactly one.
    auto multiple_live_backups = Move(2u, 1u);
    multiple_live_backups.commands[0] = Command(
        10u,
        25.0f,
        0.0f,
        0u);
    multiple_live_backups.commands[1] =
        second_live_attack.commands[0];
    multiple_live_backups.commands[2] = Command(
        10u,
        0.0f,
        0.0f,
        0u);
    const GoldSrcPmoveExecutionResult multiple_backup_result =
        runtime.Execute(
            1u,
            multiple_live_backups,
            kRespawnCycles + 6u,
            11040u,
            &player,
            &world);
    assert(multiple_backup_result.ok());
    assert(multiple_backup_result.duplicates_suppressed == 2u);
    assert(runtime.CombatDiagnostics(1u)
        ->duplicate_attack_commands_suppressed == 2u);

    // A raw backup that carries IN_ATTACK but does not exactly equal the
    // last executed attack is discarded without changing the metric.
    auto third_live_attack = Move(0u, 1u);
    third_live_attack.commands[0] = Command(
        10u,
        0.0f,
        0.0f,
        IN_ATTACK);
    assert(runtime.Execute(
        1u,
        third_live_attack,
        kRespawnCycles + 7u,
        11050u,
        &player,
        &world).ok());
    auto nonmatching_attack_backup = Move(1u, 1u);
    nonmatching_attack_backup.commands[0] = Command(
        10u,
        1.0f,
        0.0f,
        IN_ATTACK);
    nonmatching_attack_backup.commands[1] = Command(
        10u,
        0.0f,
        0.0f,
        0u);
    const GoldSrcPmoveExecutionResult nonmatching_backup_result =
        runtime.Execute(
            1u,
            nonmatching_attack_backup,
            kRespawnCycles + 8u,
            11060u,
            &player,
            &world);
    assert(nonmatching_backup_result.ok());
    assert(nonmatching_backup_result.duplicates_suppressed == 1u);
    assert(runtime.CombatDiagnostics(1u)
        ->duplicate_attack_commands_suppressed == 2u);

    g_combat_respawn_in_prethink = false;
    g_combat_global_time = nullptr;
    g_combat_global_frametime = nullptr;
    g_combat_active_attack_slot = nullptr;
}

#if defined(_MSC_VER)
void TestCombatCmdEndFailureFailStopsRuntime()
{
    using namespace hl::game_api::detail;
    using namespace hl::network;

    GoldSrcPmoveRuntime runtime;
    assert(runtime.InitializeWorld(CollisionFixture()));

    float global_time = 0.0f;
    float global_frametime = 0.0f;
    int active_attack_slot = 0;
    g_combat_global_time = &global_time;
    g_combat_global_frametime = &global_frametime;
    g_combat_active_attack_slot = &active_attack_slot;
    g_combat_observation_count = 0u;

    GoldSrcPmoveGameDllCallbacks callbacks;
    callbacks.pm_init = &MockCombatPmInit;
    callbacks.pm_move = &MockCombatPmMove;
    callbacks.cmd_start = &MockCombatCmdStart;
    callbacks.cmd_end = &MockCombatCmdEnd;
    callbacks.player_pre_think = &MockCombatPlayerPreThink;
    callbacks.player_post_think = &MockCombatPlayerPostThink;
    callbacks.global_time = &global_time;
    callbacks.global_frametime = &global_frametime;
    callbacks.active_attack_postthink_slot = &active_attack_slot;
    callbacks.combat_enabled = true;
    GoldSrcMovevarsConfig movevars;
    movevars.maximum_velocity = 2000.0f;
    assert(runtime.InitializeGameDll(callbacks, ".", movevars));

    edict_t world{};
    world.free = FALSE;
    edict_t shooter{};
    shooter.free = FALSE;
    shooter.v.movetype = MOVETYPE_WALK;
    shooter.v.health = 100.0f;
    shooter.v.gravity = 1.0f;
    shooter.v.friction = 1.0f;
    shooter.v.maxspeed = 320.0f;
    shooter.v.flags = FL_ONGROUND;
    shooter.v.view_ofs = Vector(0.0f, 0.0f, 28.0f);
    edict_t other_player = shooter;
    edict_t target{};
    target.free = FALSE;
    target.v.health = 100.0f;
    int private_weapon_state = 12;

    runtime.SetCombatPlayerReady(
        1u,
        true,
        kGoldSrcStockGlockWeaponId,
        true);
    runtime.SetCombatPlayerReady(
        2u,
        true,
        kGoldSrcStockGlockWeaponId,
        true);
    g_combat_side_effect_target = &target;
    g_combat_private_weapon_state = &private_weapon_state;
    g_combat_fail_cmd_end = true;

    auto attack = Move(0u, 1u);
    attack.commands[0] = Command(10u, 0.0f, 0.0f, IN_ATTACK);
    const GoldSrcPmoveExecutionResult failed =
        runtime.Execute(1u, attack, 7u, 1000u, &shooter, &world);
    g_combat_fail_cmd_end = false;

    assert(!failed.ok());
    assert(failed.status == GoldSrcPmoveExecutionStatus::kPmMoveFailed);
    assert(failed.gameplay_failure_stage == "cmd_end");
    assert(failed.gameplay_callback_failure);
    assert(failed.gameplay_fail_stop);
    assert(!failed.recoverable_rollback);
    assert(g_combat_observation_count == 5u);
    assert(g_combat_observations[0].event
        == CombatCallbackEvent::kCmdStart);
    assert(g_combat_observations[1].event
        == CombatCallbackEvent::kPlayerPreThink);
    assert(g_combat_observations[2].event
        == CombatCallbackEvent::kPmMove);
    assert(g_combat_observations[3].event
        == CombatCallbackEvent::kPlayerPostThink);
    assert(g_combat_observations[4].event
        == CombatCallbackEvent::kCmdEnd);
    assert(target.v.health == 90.0f);
    assert(private_weapon_state == 11);
    assert(shooter.v.iuser4 == 1);
    const GoldSrcCombatClientDiagnostics* combat =
        runtime.CombatDiagnostics(1u);
    assert(combat != nullptr);
    assert(combat->player_postthink_calls == 1u);
    assert(combat->attack_commands_executed == 1u);
    assert(combat->callback_failures == 1u);
    assert(combat->phase == GoldSrcCombatPhase::kCombatStable);
    const GoldSrcCommandExecutionState* failed_state =
        runtime.CommandState(1u);
    assert(failed_state != nullptr);
    assert(failed_state->last_observed_packet_sequence() == 7u);
    assert(failed_state->last_validated_move_sequence() == 7u);
    assert(failed_state->last_executed_move_sequence() == 0u);
    assert(failed_state->diagnostics().move_packets_observed == 1u);

    const std::size_t callbacks_after_failure =
        g_combat_observation_count;
    const GoldSrcPmoveExecutionResult retry =
        runtime.Execute(1u, attack, 7u, 1010u, &shooter, &world);
    assert(!retry.ok());
    assert(retry.gameplay_failure_stage == "fail_stop_latched");
    assert(retry.gameplay_callback_failure);
    assert(retry.gameplay_fail_stop);
    const GoldSrcPmoveExecutionResult other_client =
        runtime.Execute(2u, attack, 1u, 1010u, &other_player, &world);
    assert(!other_client.ok());
    assert(other_client.gameplay_failure_stage == "fail_stop_latched");
    assert(other_client.gameplay_callback_failure);
    assert(other_client.gameplay_fail_stop);
    assert(g_combat_observation_count == callbacks_after_failure);
    assert(target.v.health == 90.0f);
    assert(private_weapon_state == 11);
    assert(shooter.v.iuser4 == 1);
    assert(other_player.v.iuser4 == 0);
    assert(!runtime.CommandState(2u)->initialized());

    runtime.ResetClient(1u);
    const GoldSrcPmoveExecutionResult after_client_reset =
        runtime.Execute(2u, attack, 1u, 1020u, &other_player, &world);
    assert(!after_client_reset.ok());
    assert(after_client_reset.gameplay_fail_stop);
    assert(g_combat_observation_count == callbacks_after_failure);

    runtime.ResetGameDll();
    const GoldSrcPmoveExecutionResult after_runtime_reset =
        runtime.Execute(2u, attack, 1u, 1030u, &other_player, &world);
    assert(!after_runtime_reset.ok());
    assert(after_runtime_reset.gameplay_fail_stop);
    assert(g_combat_observation_count == callbacks_after_failure);

    g_combat_side_effect_target = nullptr;
    g_combat_private_weapon_state = nullptr;
    assert(runtime.InitializeGameDll(callbacks, ".", movevars));
    runtime.SetCombatPlayerReady(
        2u,
        true,
        kGoldSrcStockGlockWeaponId,
        true);
    const GoldSrcPmoveExecutionResult after_reinitialize =
        runtime.Execute(2u, attack, 1u, 1040u, &other_player, &world);
    assert(after_reinitialize.ok());
    assert(g_combat_observation_count == callbacks_after_failure + 5u);

    g_combat_global_time = nullptr;
    g_combat_global_frametime = nullptr;
    g_combat_active_attack_slot = nullptr;
}
#endif
} // namespace

int main()
{
    TestClientDataCallbackBufferContract();
    using namespace hl::network;
    TestPersistentGameDllHelperClassification();
    TestEdictPrivateDataRetirementAndReuseBarrier();
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
        assert(!plan.suppressed_backup_matched_last_command);
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
        assert(recovery.suppressed_backup_matched_last_command);
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
        const auto stale = state.Plan(recovered, 12u, 1000u);
        assert(stale.status == GoldSrcCommandPlanStatus::kStalePacket);
        assert(!stale.suppressed_backup_matched_last_command);
        assert(
            state.Plan(recovered, 11u, 1000u).status
            == GoldSrcCommandPlanStatus::kOutOfOrderPacket);

        auto nonmatching = Move(1u, 1u);
        nonmatching.commands[0] = Command(20u, 777.0f);
        nonmatching.commands[1] = Command(20u, 130.0f);
        const auto nonmatching_plan = state.Plan(
            nonmatching,
            13u,
            1040u);
        assert(nonmatching_plan.ok());
        assert(nonmatching_plan.duplicate_backups_suppressed == 1u);
        assert(!nonmatching_plan.suppressed_backup_matched_last_command);

        auto multiple_suppressed = Move(2u, 1u);
        multiple_suppressed.commands[0] = Command(20u, 777.0f);
        multiple_suppressed.commands[1] = Command(20u, 120.0f);
        multiple_suppressed.commands[2] = Command(20u, 130.0f);
        const auto multiple_suppressed_plan = state.Plan(
            multiple_suppressed,
            13u,
            1040u);
        assert(multiple_suppressed_plan.ok());
        assert(multiple_suppressed_plan.duplicate_backups_suppressed == 2u);
        assert(multiple_suppressed_plan
            .suppressed_backup_matched_last_command);
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
    {
        const float start[3] = {0.0f, 0.0f, 0.0f};
        const float end[3] = {128.0f, 0.0f, 0.0f};
        GoldSrcWorldLineTrace trace{};
        assert(runtime.TraceWorldLine(start, end, &trace));
        assert(!trace.start_solid);
        assert(!trace.all_solid);
        assert(trace.hit_world);
        assert(trace.fraction > 0.0f && trace.fraction < 1.0f);
    }
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

    edict_t player_b = player;
    player_b.v.origin = Vector(0.0f, 0.0f, 0.0f);
    player_b.v.velocity = Vector(0.0f, 0.0f, 0.0f);
    auto first_move_b = Move(0u, 1u);
    first_move_b.commands[0] = Command(10u, 0.0f, 120.0f);
    const auto first_result_b =
        runtime.Execute(2u, first_move_b, 1u, 1000u, &player_b, &world);
    assert(first_result_b.ok());
    assert(first_result_b.commands_executed == 1u);
    assert(player_b.v.origin.y > 0.0f);

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
    const Vector player_b_before_invalid = player_b.v.origin;
    const auto* command_b_before_invalid = runtime.CommandState(2u);
    assert(command_b_before_invalid != nullptr);
    const std::uint64_t command_time_b_before_invalid =
        command_b_before_invalid->command_time_msec();
    auto invalid_output = Move(0u, 1u);
    invalid_output.commands[0] = Command(10u, 100.0f);
    g_emit_invalid_output = true;
    const auto rejected =
        runtime.Execute(1u, invalid_output, 4u, 1200u, &player, &world);
    g_emit_invalid_output = false;
    assert(!rejected.ok());
    assert(!rejected.gameplay_callback_failure);
    assert(!rejected.gameplay_fail_stop);
    assert(rejected.recoverable_rollback);
    assert(rejected.status == GoldSrcPmoveExecutionStatus::kOutputInvalid);
    assert(player.v.origin == before_invalid);
    assert(player_b.v.origin == player_b_before_invalid);
    assert(runtime.movement_executed(2u));
    assert(runtime.CommandState(2u)->command_time_msec()
        == command_time_b_before_invalid);
    auto continuing_move_b = Move(0u, 1u);
    continuing_move_b.commands[0] = Command(10u, 0.0f, 120.0f);
    const auto continuing_result_b = runtime.Execute(
        2u,
        continuing_move_b,
        2u,
        1210u,
        &player_b,
        &world);
    assert(continuing_result_b.ok());
    assert(player_b.v.origin.y > player_b_before_invalid.y);
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
    assert(runtime.movement_executed(2u));

    auto duck = Move(0u, 1u);
    duck.commands[0] = Command(10u, 0.0f, 0.0f, IN_DUCK);
    const auto second_slot =
        runtime.Execute(2u, duck, 3u, 22000u, &player_b, &world);
    assert(second_slot.ok());
    assert(second_slot.ducked);
    assert(runtime.movement_executed(2u));
    assert(runtime.diagnostics().pm_init_calls == 1u);
    assert(runtime.diagnostics().movement_rollbacks == 1u);
    assert(runtime.implemented_service_callback_count() == 25u);
    TestCombatCallbackOrderReadinessAndSplitTime();
    TestRepeatedDeadPlayerClickRespawn();
#if defined(_MSC_VER)
    TestCombatCmdEndFailureFailStopsRuntime();
#endif
    TestTransientMuzzleFlashCleanup();
    return 0;
}
