# Prompt 246 completion handoff

## Identity and scope

- Repository: `https://github.com/Pvitaly91/hl-engine.git`
- Baseline commit: `4ea479a1b6f800df7e28025bff9e040431fff149`
- Task branch: `codex/goldsrc-pmove-slice`
- SDK HEAD: `b1b5cf5892918535619b2937bb927e46cb097ba1`
- Stock client: Half-Life 1.1.2.2, Steam build `15961492`
- Game DLL API: `GetEntityAPI2` version 140
- Previous boundary: `movement_execution_required`
- Next boundary: `two_client_player_replication_required`

All interoperability traffic remained on `127.0.0.1`.  The SDK, installed
client, client DLL, Game DLL, map, and game data remained read-only.  No
proprietary input, capture, SDK content, installed binary, or build artifact
is part of the source change.

## Command and callback contract

The validated `clc_move` payload is decoded without movement side effects.
The execution planner reconstructs its newest-first command array into
oldest-first execution, distinguishes new and backup commands, replays only
the backups needed for a bounded sequence gap, and suppresses stale or
duplicate history.

The accepted limits are 62 total decoded commands, 24 recovered missing
commands, 1000 ms per move packet, and 250 ms maximum command-time lead.
Commands over 50 ms are split into bounded pieces while preserving the exact
duration.  Invalid checksum, malformed or excessive counts, zero command
time, stale/out-of-order packets, and time-budget violations execute no
movement.

The movement-only callback order for every executable subcommand is:

1. mask gameplay inputs and bind the authoritative `usercmd_t`;
2. call `CmdStart`;
3. build a shadow `playermove_t`;
4. call the real Game DLL `PM_Move` with `server=true`;
5. validate the complete result;
6. atomically commit and relink the player;
7. call `CmdEnd`.

`PM_Init` runs exactly once for the loaded Game DLL.  Player think, weapon,
item, trigger, touch-gameplay, and post-think execution remain outside this
movement-only slice.

## Implementation

The implementation adds a bounded command execution state, an authoritative
PM_Move runtime, and a BSP30 collision adapter.  It reuses the existing Game
DLL loader, UDP/netchan path, usercmd decoder, cvar registry, edict store,
world bootstrap, player lifecycle, snapshot scheduler, delta encoder, and
64-frame history.

The PM_Move context is zero-initialized and populated from the authoritative
player edict and server cvars.  Twenty-five engine service callbacks provide
bounded info lookup, file helpers for PM initialization, deterministic random
helpers, model queries, point contents, hull contents, player/line/model
traces, texture lookup, and safe no-op movement effects.

The physent policy is one static BSP world physent.  The world loader validates
planes, clipnodes, nodes, leaves, model headnodes, indices, bounds, and trace
depth.  Point, standing, and duck hulls produce bounded traces with fraction,
end position, plane, start-solid, all-solid, contents, and hit semantics.
Dynamic brushes, platforms, doors, ladders, water movement, triggers, and
other players are not represented in this slice.

PM_Move output is validated for finite and bounded vectors, valid hull,
movement, water, ground, and touch state, and monotonic command time.  A
rejected output leaves both the edict and successful command cursor unchanged.
A successful result commits origin, velocity, base velocity, view angles,
view offset, punch angle, flags, movetype, ground entity, water state, duck
state, timers, friction, and old buttons.  Existing clientdata and entity
snapshot builders then replicate only that committed edict.

The feature is gated by `--goldsrc-pmove`; without it, normal host behavior is
unchanged.

## Deterministic verification

The final Release build passed and CTest passed 12/12.  The dedicated PM_Move
test covers:

- exact long-command splitting and duration preservation;
- new-command ordering, backup recovery, replay-once behavior, and duplicate
  suppression;
- excessive backup/new counts, packet time, command lead, stale packets, and
  out-of-order packets;
- actual `PM_Init`, `CmdStart`, `PM_Move`, and `CmdEnd` callback counts;
- 25 service callbacks;
- static BSP wall collision;
- invalid-output rollback and same-packet retry;
- duck hull selection;
- disconnect reset and slot isolation.

Proof A called the real installed Game DLL and completed with 29 PM_Move
calls, 29 executed commands, one recovered backup, one suppressed duplicate,
forward movement, friction stop, jump, gravity, landing, duck/unduck, wall
collision, no world penetration, snapshot integration, and clean shutdown.

Proof B passed invalid checksum/count/time cases, duplicate and out-of-order
handling, backup replay-once behavior, invalid velocity rollback, preserved
collision and unduck state, reliable-state preservation, disconnect cleanup,
stale-command rejection, slot reuse, fresh-session movement, and clean
shutdown.

All legacy handshake, netchan, serverinfo, resource manifest, fragmentation,
signon continuation, delta description, post-resource command, world
baseline, first snapshot, continuous snapshot, and player lifecycle proofs
passed.  The feature-off acceptance/drift wrapper passed with
`normal_host_behavior_changed=0`.

## Stock-client verification

Five separate observations used the unmodified stock client and held bounded
movement input for more than 30 server seconds after player materialization:
forward, backward, strafe left, jump, and duck.  Each run proved that the
expected real stock input reached decoded `usercmd_t`, accepted commands
executed through real PM_Move, authoritative state changed, grounding and
static-world collision remained valid, continuous snapshots stayed stable,
and the client remained connected.  Jump transitioned through airborne state
and grounding; duck selected the duck hull.

The client, client DLL, Game DLL, map, and delta-description hashes were
identical before and after every run.  The previous movement boundary is
resolved.  The next observed boundary is
`two_client_player_replication_required`; single-player authoritative movement
does not establish two-client replication.

## Staging audit

Only repository source, headers, tests, proof scripts, and documentation are
eligible for staging.  Generated `out/` files remain untracked and unstaged.
The SDK, installed Game DLL, installed client, proprietary game data, captures,
temporary logs, and generated binaries remain unstaged.

Authoritative PM_Move-based player movement does not mean that weapon
processing, item interaction, moving platforms, ladders, water movement,
player-to-player collision, damage, death, respawn, or complete multiplayer
gameplay are implemented.
