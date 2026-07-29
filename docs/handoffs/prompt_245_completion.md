# Prompt 245 completion handoff

## Identity and scope

- Repository: `https://github.com/Pvitaly91/hl-engine.git`
- Baseline branch: `codex/goldsrc-continuous-snapshot-slice`
- Baseline commit: `91309f4678aa7cc54fc1503274f704a34d6d57e1`
- Task branch: `codex/goldsrc-player-lifecycle-slice`
- SDK HEAD: `b1b5cf5892918535619b2937bb927e46cb097ba1`
- Stock client: Half-Life 1.1.2.2, Steam build `15961492`
- Game DLL API: installed Win32 `valve` Game DLL, `GetEntityAPI2` version 140
- Previous boundary: `player_lifecycle_or_signon_progression_required`
- Lifecycle trigger:
  `stable_snapshot_ack_reconciliation_after_premature_signonnum_1`

All interoperability traffic remained on `127.0.0.1`. The SDK, installed
client, installed Game DLL, and installed game data remained read-only.

## Observed contract

The Prompt 244 client had already accepted the reliable server bootstrap,
including `svc_setview 1`, followed later by the baseline bundle ending in
`svc_signonnum 1`. It sent `sendents`, continued `clc_move` keepalives and
`clc_delta` references, but did not send a later `spawn`, `begin`, or
`prespawn` string command. The missing operation was the skipped server-side
player lifecycle, not another client command.

The selected compatibility path therefore reconciles the omitted lifecycle
once after stable acknowledged snapshots:

1. network admission binds slot 1 to reserved edict 1 for the current session
   generation;
2. validated userinfo is stored and `ClientUserInfoChanged` is called once;
3. the accepted `new` command causes `ClientConnect` to be called once with
   the sanitized name, loopback endpoint text, and a bounded 128-byte rejection
   buffer;
4. after the established early `svc_signonnum 1` path becomes stable,
   `ClientPutInServer` is called once;
5. the Game DLL allocates private player data and runs its player spawn path;
6. the resulting finite authoritative player edict is validated, marked
   spawned, and recorded as view entity 1;
7. later 20 Hz snapshots add edict 1 using `entity_state_player_t` and build
   `clientdata_t` only through `pfnUpdateClientData`;
8. a later `clc_delta` reference covering the first player snapshot completes
   the bounded signon transition;
9. `ClientDisconnect` runs exactly once during clean shutdown before private
   data and the binding are cleared.

The successful stock run proved that replaying `svc_setview`,
`svc_setangle`, or `svc_signonnum 1` is unnecessary and would duplicate an
already acknowledged transition. The typed lifecycle control codec and
netchan tests nevertheless cover exact message order, frozen bytes,
retransmission identity, duplicate queue suppression, wrong and non-covering
ACKs, exact ACK, and reset semantics.

## Implementation

The new pure player-lifecycle module provides:

- one-based client-edict ownership with generation validation;
- strict typed decoding for `spawn <spawncount> <checksum>`;
- separate network, Game DLL, put-in-server, player, view, signon, and
  gameplay states;
- exactly-once connect, put-in-server, and disconnect behavior;
- bounded rejection, rollback, stale-session, and slot-reuse handling;
- deterministic view/signon control encode/decode;
- no console, shell, or generic unvalidated command execution.

The existing UDP session, netchan, edict store, string pool, signon state,
baseline registry, snapshot scheduler, and 64-frame history are reused. There
is no second loader, transport, client registry, entity registry, edict pool,
or signon state machine.

The runtime integrates the real Game DLL callbacks and maps the authoritative
player into existing semantic snapshots. Canonical `clientdata_t` string
fields are captured with bounded NUL termination. Inactive prediction and
ammo extension channels are neutralized because this slice deliberately does
not run `PlayerPreThink`, `PM_Move`, prediction, or weapon simulation.
Non-finite network-visible values are rejected or neutralized only in those
inactive clientdata extension channels.

Proof wrappers now suppress only the high-volume `general` callback category.
Structured server, protocol, proof, and summary records remain enabled. This
keeps the bounded proof output deterministic without weakening any assertion.

## Verification

The final Release build passed. CTest passed 11/11:

- connectionless;
- netchan;
- fragmentation;
- signon;
- resource manifest;
- delta descriptions;
- client move;
- world baseline;
- first/continuous snapshot;
- player lifecycle.

Lifecycle Proof A used the real built host in a separate process and a
separate loopback UDP client. It verified exactly one `ClientConnect`, one
`ClientPutInServer`, private data, a spawned player on edict 1, view entity 1,
player-derived clientdata, a player snapshot and its frame acknowledgement,
signon progression, no movement, and clean shutdown.

Lifecycle Proof B verified bounded Game DLL rejection, rejection reason,
duplicate suppression, wrong spawn count and phase, put-in-server rollback,
invalid-player exclusion, exact disconnect cleanup, stale-session rejection,
clean slot reuse, and no inherited view, private data, frame history, or
pending reliable state. Movement and gameplay remained disabled.

The complete post-change legacy matrix passed:

| Gate | Result |
|---|---|
| handshake and disconnected-slot reuse | PASS |
| netchan normal and retransmission | PASS |
| serverinfo A/B | PASS |
| resource manifest A/B | PASS |
| fragmentation A/B | PASS |
| signon continuation A/B | PASS |
| delta-description A/B | PASS |
| post-resource command A/B | PASS |
| world baseline A/B | PASS |
| first snapshot A/B | PASS |
| continuous snapshot A/B | PASS |
| feature-off acceptance/drift | PASS |
| normal host behavior changed | 0 |

`git diff --check` passed. Generated `out/` files remain untracked and
unstaged. No SDK, installed Game DLL, installed client, proprietary asset,
capture, temporary stock log, or generated binary belongs in the commit.

## Final stock-client result

The final run used the unmodified installed Half-Life client, production
`delta.lst`, production resource manifest, `c0a0`, and a direct loopback
connection. The installed client and delta-description hashes were identical
before and after the run.

The host completed with:

- `ClientUserInfoChanged=1`;
- `ClientConnect=1`;
- `ClientPutInServer=1`;
- authoritative edict and view entity 1;
- private player data ready;
- player entity spawned;
- 160 continuous snapshots sent;
- 114 continuous snapshot acknowledgements;
- 115 distinct client frame references;
- 157 snapshots containing the player;
- first stock player frame ID 162;
- player-derived clientdata accepted;
- bounded signon state `pre_movement_ready`;
- `gameplay_active=false`;
- clean shutdown.

The stock client remained alive through the bounded observation and accepted
the player/view/clientdata transition. The previous boundary is resolved and
the next observed boundary is:

`movement_execution_required`

## Limitation

Creating the Game DLL player entity and establishing the stock-client view
does not mean that clc_move execution, PM_Move, prediction, weapons,
damage, death, respawn, or complete multiplayer gameplay are implemented.
