# GoldSrc authoritative PM_Move execution

## Scope and evidence

This note records the Prompt 246 movement slice and its persistent-session
stabilization. The stabilization baseline is
`87504e22ff0c4fea146bbab85dba9aff643053e6`; the task branch is
`codex/manual-pmove-smooth-longrun-fix`. The SDK remains at
`b1b5cf5892918535619b2937bb927e46cb097ba1`.

The pre-change Half-Life 1.1.2.2 Steam build 15961492 observation used the
Prompt 245 `c0a0` configuration and an unmodified local client on
`127.0.0.1`.  The client completed the lifecycle, sent valid movement input,
and remained on the `movement_execution_required` boundary.  A representative
accepted move contained two backup commands, one new command, and 32 ms of new
command time.  Hundreds of valid move envelopes were decoded while the player
snapshot reported no player updates.  This distinguishes decoded client-side
input and prediction from authoritative movement.

The execution contract was cross-checked against:

- the public Valve HLSDK `usercmd_t`, `playermove_t`, `movevars_t`, and
  `pm_shared` declarations at the pinned local SDK revision;
- public ReHLDS `SV_ParseMove`, `SV_EstablishTimeBase`, and `SV_RunCmd`
  behavior at commit
  `0124d56c3d888d922eb045775f71c6682ad1226f`;
- the existing repository-owned move codec, netchan sequencing, lifecycle,
  snapshot, and deterministic proof fixtures.

No reference-engine implementation block is copied into the host.  The host
uses an independent bounded implementation.

## Command execution and replay

The move codec exposes commands in chronological wire order: explicit backup
history first and fresh commands last. Execution preserves that order so
PM_Move receives each selected command oldest-first.

The bounded contract is:

- backup count and new count are byte fields;
- their checked sum must not exceed 62;
- each individual count must not exceed 62;
- an invalid checksum or malformed batch executes nothing;
- duplicate and out-of-order outer netchan packets are rejected before the
  execution planner;
- new commands from an accepted packet execute once;
- raw outer-netchan gaps are diagnostic only and never synthesize movement;
- backups execute only when their explicit history extends the last committed
  user command;
- each available missing command is recovered once, oldest-first;
- gaps beyond supplied backups do not replay the last command;
- repeated backups in a later non-gap packet are ignored;
- disconnect and slot reuse clear all sequence and command history;
- a PM_Move failure neither commits player state nor advances the successful
  command cursor.

The synthetic replay limit is zero. Recovery is bounded by the explicit
backup commands carried in the validated packet, uses checked arithmetic, and
never allocates an unbounded command queue.

## Command time

`usercmd_t::msec` advances authoritative command time.  One decoded byte is in
the valid `1..255` range; zero is rejected for authoritative execution.
Commands over 50 ms are split into bounded subcommands no greater than 50 ms.
The independent splitter preserves the exact original duration and permits at
most eight subcommands for one byte-sized command.

One move packet may contribute at most 1000 ms and may not advance the client
command frontier more than 250 ms ahead of elapsed monotonic host time. The
first committed executable command establishes a per-session host epoch.
Committed command time advances only after a complete PM_Move batch succeeds.
A temporary lead advances the observed/validated packet frontiers but neither
the execution frontier nor authoritative state; later host-time catch-up and
explicit backups recover it without a replay cascade. Stable rejection
identifiers include
`excessive_backup_count`, `excessive_new_count`, `invalid_command_msec`,
`command_time_overflow`, `command_time_budget_exceeded`,
`duplicate_command`, `stale_command`, `invalid_checksum`, and
`truncated_usercmd`.

Planning is side-effect free apart from bounded diagnostics. Separate
observed, structurally validated, and successfully executed frontiers prevent
a temporary clock rejection from poisoning duplicate detection. Disconnect
and slot reuse reset all sequence and clock state.

The verified contract gates are:

    command_execution_contract_verified=yes
    command_replay_contract_verified=yes
    command_time_contract_verified=yes

## Game DLL callback order

The full reference path surrounds PM_Move with `CmdStart`, player think
callbacks, commit/link/touch processing, `PlayerPostThink`, and `CmdEnd`.
For the movement-only milestone, `PlayerPreThink`, entity `Think`,
`PlayerPostThink`, and gameplay touch dispatch are deliberately deferred
because they can run weapons, items, triggers, and other gameplay outside this
scope.

The selected verified movement-only order for each executable subcommand is:

1. bind the masked authoritative `usercmd_t`;
2. call `CmdStart` exactly once;
3. build a shadow `playermove_t`;
4. call the real Game DLL `PM_Move` once with the server flag;
5. validate the complete output;
6. atomically commit and relink the player;
7. call `CmdEnd` exactly once, including cleanup after a rejected output.

Rejected and duplicate commands call none of these callbacks.  `CmdEnd` is
never called without a matching `CmdStart`.  Primary/secondary attack, reload,
weapon selection, impulse, and use input are masked.  Jump and duck remain
movement input.

`PM_Init` is called exactly once for one loaded Game DLL before the first
PM_Move.  It receives a complete callback table, including bounded game-dir
file helpers for `sound/materials.txt`.

The verified callback-order gate is:

    game_dll_movement_callback_order_verified=yes

## PM_Move context and services

Each command gets an independent zero-initialized shadow context populated
from the authoritative player edict.  It includes player index, server and
multiplayer flags, time/frame time, origin, angles, velocity, base velocity,
view offset, punch angle, flags, movetype, dead state, ground state, water
state, gravity/friction, duck/fall/step state, old buttons, the masked command,
movevars, a world physent, touches, bounded physinfo, and the required callback
table.  No private CBasePlayer memory is inspected or copied.

The basic Half-Life `pm_shared` path requires bounded implementations of:

- info-value lookup;
- test-player-position;
- point and true-point contents;
- hull point contents;
- player and line traces;
- deterministic random helpers;
- model type and bounds;
- BSP hull selection;
- model trace;
- safe console diagnostics;
- PM_Init file size/load/free and memory-line reading;
- bounded no-op movement sound/event emission;
- texture lookup.

All callback pointers remain valid for the entire PM_Move call.  Unsupported
gameplay callbacks fail closed or record a bounded semantic event.

## Movevars

The authoritative defaults come from the existing runtime cvar registry and
the already implemented signon `svc_newmovevars` bundle.  The same values
populate the server PM_Move context:

    movevars_source=runtime_cvar_registry
    movevars_client_sync=verified

The model includes gravity, stop speed, maximum speed, spectator maximum
speed, accelerate, air/water accelerate, friction, edge/water friction,
entity gravity, bounce, step size, maximum velocity, z maximum, wave height,
footsteps, sky fields, and roll values.  Values are checked for finiteness and
safe positive ranges before use.

## BSP collision and physents

The runtime map loader owns one validated BSP30 collision representation made
from the already loaded map bytes.  It validates lump bounds, planes,
clipnodes, model headnodes, child indices, recursion depth, and trace bounds.
It supports world contents and swept standing, duck, and point hull traces
with correct fraction, end position, plane, start-solid, all-solid, and hit
physent semantics.

The minimum physent policy is:

    pmove_physent_policy=world_only
    movement_touch_policy=world_collision_only

Physent index zero is the static world.  Dynamic brushes, doors, platforms,
ladders, water movement, other players, monsters, and trigger gameplay remain
explicit extensions.  Jolt is not used for player-vs-BSP collision.

## Validation, commit, and snapshots

PM_Move output is validated for finite vectors, coordinate and velocity
bounds, valid movement/duck/water/ground state, bounded touches, valid physent
references, and monotonic command time.  The player edict is unchanged during
the callback.  Invalid output rolls back to the last authoritative state and
does not partially advance the execution cursor.

A successful output atomically commits origin, velocity, base velocity,
angles, view offset, punch angle, flags, movetype, ground entity, water state,
duck state, fall/step timers, friction, and old buttons. The clientdata and
player-entity snapshot builders then consume only that committed edict.

In the established streaming phase, bounded receive and PM_Move execution
occur before a due snapshot. The carrier acknowledgement therefore covers
the client packet represented by that snapshot. The scheduler retains the
20 Hz default, sends at most one scheduled snapshot per due interval, and
skips catch-up bursts after a late host frame.

The Game DLL `Player_Encode` conditional is honored for the local player:
low-resolution entity `origin[0..2]` stays at the baseline while the precise
committed origin is carried by `clientdata_t`. This matches stock prediction
reconciliation and avoids conflicting local-origin channels. Entity hull,
flags, base velocity, and clientdata movement fields remain coherent.

Authoritative PM_Move-based player movement does not mean that weapon
processing, item interaction, moving platforms, ladders, water movement,
player-to-player collision, damage, death, respawn, or complete multiplayer
gameplay are implemented.

## Persistent-session stabilization

The visible jerk and eventual movement freeze were separate defects. Jerk
combined three coherence faults: backup/fresh commands were partitioned with
the wrong semantic indexing, raw netchan gaps could cause synthetic command
replay, and a due snapshot could be built before same-frame movement. The
freeze began when a temporarily ahead packet left the observed frontier
stalled; later packet sequences were then interpreted through the wrong gap
history and formed a growing rejection/replay cascade.

The stabilized model uses explicit backup recovery, a zero synthetic-replay
limit, independent observed/validated/executed frontiers, a transactional
PM_Move batch commit, and receive-before-snapshot ordering in the established
stream. A rejected output restores the authoritative edict and planner state.
Sampled diagnostics report only semantic discontinuities and aggregate
cadence/clock counters; normal packets and snapshots are not logged one by
one.

Session shutdown follows the stock lifecycle order: the persistent network
runtime and client slot are drained first, the Game DLL receives exactly one
`ServerDeactivate` callback after successful activation, and only then is the
module unloaded. This prevents long-running map allocations from surviving
into `FreeLibrary` while preserving feature-off startup and shutdown.

Deterministic tests simulate ten minutes at 100 commands per second, 6/7 ms
high-FPS rounding, idle/resume, non-move sequence gaps, explicit backup
recovery, temporary clock lead, the former permanent cascade, rollback,
same-frame snapshot ordering, scheduler jitter, sequence wrap, and reconnect.
The external proof uses a separate localhost UDP client and the real host
process; it never calls movement internals directly.

## Implemented slice

The command planner, PM_Move runtime, world collision adapter, and snapshot
integration are active only with `--goldsrc-pmove`.  The implementation:

- reconstructs accepted backup and new commands into oldest-first execution
  order;
- suppresses duplicate and stale commands and bounds dropped-command
  recovery;
- splits long commands without changing their total duration;
- calls the installed Game DLL's real `PM_Init` once and `PM_Move` once per
  executable subcommand;
- binds 25 movement-service callbacks and exposes one static BSP world
  physent;
- supports point, standing, and duck hull traces against validated BSP30
  planes and clipnodes;
- validates all PM_Move output before atomically committing it to the
  authoritative player edict;
- retains the previous edict and command cursor when PM_Move output is
  rejected;
- feeds the committed player state into the existing clientdata, packet
  entities, delta snapshot, and frame-history paths.

The deterministic positive proof executes real Game DLL movement and verifies
forward movement, friction stop, jump, gravity, landing, duck/unduck, wall
collision, absence of world penetration, backup replay, duplicate
suppression, callback order, and clean shutdown.  The negative proof covers
invalid checksum, malformed and excessive command groups, stale/out-of-order
packets, command-time limits, invalid PM_Move output, rollback/retry, and slot
reset/isolation.  The Release build and all 12 CTest groups pass.
