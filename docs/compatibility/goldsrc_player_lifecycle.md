# GoldSrc player lifecycle and signon progression

## Scope and selected evidence

This slice continues the verified Prompt 244 loopback path after stable
continuous snapshots. It targets the unmodified Half-Life 1.1.2.2 client,
Steam build 15961492, the installed Win32 `valve` Game DLL, and protocol 48.
All interoperability traffic is restricted to `127.0.0.1`.

The selected source baseline is
`91309f4678aa7cc54fc1503274f704a34d6d57e1` on
`codex/goldsrc-continuous-snapshot-slice`; the task branch is
`codex/goldsrc-player-lifecycle-slice`. The read-only Half-Life SDK is pinned
at `b1b5cf5892918535619b2937bb927e46cb097ba1`.

The contract was frozen before implementation from, in priority order:

1. two bounded pre-change runs of the stock client against the Prompt 244
   host;
2. the installed Win32 `valve` Game DLL export and callback table;
3. the locally installed reference `hlds.exe`;
4. the pinned local Half-Life SDK declarations and `client.cpp` behavior;
5. ReHLDS commit `0124d56c3d888d922eb045775f71c6682ad1226f`,
   especially `SV_ConnectClient`, `SV_New_f`, `SV_Spawn_f`,
   `SV_WriteSpawn`, `SV_SendEnts_f`, and `SV_ExtractFromUserinfo`.

The implementation is independent. Public reference code is behavioral
evidence only.

## Reproduced pre-change boundary

The pre-change stock client completed every Prompt 244 stage and remained
connected while sending valid `clc_move` keepalives. It acknowledged multiple
continuous server frames and the host reached
`continuous_snapshot_stable`. No `spawn`, `begin`, or `prespawn` string
command followed.

The reason is now identified: the Prompt 242 baseline bundle already ends in
`svc_signonnum 1`. A stock server sends that stage only from its validated
spawn response, after the Game DLL player lifecycle. Receiving it early causes
the client to send `sendents` and wait for the server-side player/view state;
it does not subsequently send the already-skipped `spawn` request.

The reproduced boundary is therefore
`premature_signonnum_1_requires_player_lifecycle_reconciliation`. The smallest
compatible correction preserves the already accepted Prompt 242--244 traffic,
performs the omitted lifecycle once at the first acknowledged stable snapshot
boundary, and exposes the authoritative player through the existing snapshot
stream. It deliberately does not replay `svc_setview`, `svc_setangle`, or
`svc_signonnum`: the stock client already acknowledged the immutable reliable
bootstrap containing `svc_setview 1` and `svc_signonnum 1`, and the final run
proved that no second signon bundle is required. New sessions use the exact
typed spawn grammar when the client supplies it; the reconciliation trigger
exists only for the established early-stage compatibility path.

## Reference lifecycle contract

The world is edict 0. Client slot 1 owns edict 1, and in general a one-based
client slot owns the same one-based reserved edict index. The binding is made
at network admission and remains stable for the network-session generation.
It does not imply Game DLL acceptance, player creation, signon completion, or
gameplay activation.

The ordered stock contract is:

1. Network admission binds and clears the reserved client edict. Validated
   userinfo is stored. The reference engine invokes
   `ClientUserInfoChanged` at this point; the selected Half-Life Game DLL
   returns immediately because player private data does not exist yet.
2. The reliable client command `new` causes serverinfo construction.
   `svc_setview` names `player_index + 1`, so slot 1 views edict 1. After
   building that response and before transmitting it, the engine calls
   `ClientConnect(edict, name, address, reject_reason)`.
3. `ClientConnect` receives the sanitized userinfo name, the loopback address
   text, and an initialized 128-byte rejection buffer. Acceptance is recorded
   exactly once. Rejection exposes no player, invokes no
   `ClientPutInServer`, and clears the session binding.
4. On an ordinary stock server, only the exact client string command
   `spawn <spawncount> <crc>` authorizes player creation. It has exactly two
   decimal arguments. The first must match the current nonzero server map
   generation. The second is a bounded 32-bit checksum token; this slice
   validates its grammar but does not expand consistency policy.
5. Before calling `ClientPutInServer`, the engine clears the client entvars,
   preserves the edict identity, assigns colormap and netname, and publishes
   the current server time through `globalvars_t`.
6. `ClientPutInServer` allocates the selected Game DLL's `CBasePlayer`
   private data through `pfnPvAllocEntPrivateData` and directly calls the
   Game DLL player's `Spawn`. Private data and a finite, valid player edict
   must both exist before the player becomes network-visible.
7. The spawn response contains the established world/entity signon data,
   `svc_time`, client updates and light styles, `svc_setangle`, initial
   Game-DLL-derived `svc_clientdata`, then `svc_signonnum 1`. The selected
   Prompt 245 reconciliation reuses the already accepted world signon data
   and sends only the missing bounded player/clientdata snapshot semantics; it
   does not replay the world bootstrap or emit a duplicate signon stage.
8. The client answers stage 1 with `sendents`. That command is the indirect
   acknowledgement of the reliable signon stage. Prompt 245 marks its bounded
   player transition complete only after a later `clc_delta` reference covers
   the first snapshot containing the real player. There is no verified
   `begin` or `prespawn` command in this protocol-48 flow.

The serverinfo-time `svc_setview` already identifies edict 1 before the player
exists. The binding becomes authoritative only after a valid Game DLL player
is ready. Prompt 245 records the same entity index in typed lifecycle state
and verifies it through the accepted player snapshot and its later client
frame reference. `svc_setangle` was not required in the successful repair.

The typed lifecycle control codec and netchan tests still cover exact
`svc_setview`, `svc_setangle`, `svc_signonnum 1` ordering, immutable queued
bytes, retransmission identity, wrong and non-covering acknowledgements,
exact acknowledgement, duplicate queue suppression, and reset. That codec is
a bounded transport contract, not a reason to send an unobserved duplicate in
the stock-client runtime path.

## State separation and idempotence

Player lifecycle is independent from network, netchan, signon, snapshot
scheduling, and gameplay state. Its phases are:

`no_client_edict -> client_edict_allocated -> awaiting_game_dll_connect ->
game_dll_connected -> awaiting_put_in_server -> put_in_server ->
player_entity_ready -> awaiting_signon_progression ->
player_view_established -> pre_movement_ready`.

`rejected` and `disconnected` are terminal for one session generation.
`ClientConnect` and `ClientPutInServer` each execute at most once. Duplicate
commands and retransmitted outer packets do not repeat callbacks or regenerate
reliable lifecycle bytes. A wrong spawn count, wrong phase, malformed numeric
field, trailing argument, separator, embedded NUL, or control character
cannot mutate lifecycle state.

The Prompt 244 `active` label meant network/snapshot streaming, not Game DLL
or gameplay activation. Prompt 245 keeps explicit values for
`network_connected`, `signon_complete`, `game_dll_connected`,
`put_in_server`, `player_entity_ready`, `player_spawned`, and
`gameplay_active`. `gameplay_active` remains false.

## Player state, clientdata, and snapshots

The authoritative player is the bound Game DLL edict. Network state is mapped
field by field; no SDK object or host pointer is serialized. The player uses
the `entity_state_player_t` delta table, its one-based edict number, and
finite Game-DLL-produced origin, angles, velocity, model, movement, solidity,
sequence, frame, effects, render, and gait fields when valid.

`pfnUpdateClientData` is available in the selected callback table. It is
called with the authoritative edict and a zero-initialized `clientdata_t`.
The result is copied into the protocol-independent frame model and validated;
private object memory is never inspected. Pre-player zero clientdata remains
the fallback before the lifecycle boundary.

The existing 20 Hz scheduler continues independently of reliable transport.
The first valid post-lifecycle frame adds edict 1 and uses a full snapshot
when no acknowledged base contains it. Later frames use the existing
Add/Update/Remove delta and 64-frame history behavior. `clc_move` remains
decoded keepalive input only and never changes player state.

Initial user messages emitted inside `ClientPutInServer` are accepted through
the existing bounded Game DLL message callbacks but are not treated as proof
of gameplay. Only messages inseparable from successful player construction
are retained for lifecycle acceptance.

## Rejection, rollback, disconnect, and reuse

Game DLL rejection clears the edict binding without exposing a player or
calling `ClientPutInServer`. Failure during `ClientPutInServer` clears any
private data through the engine allocator contract and leaves no
network-visible player.

For a Game-DLL-connected client, disconnect calls `ClientDisconnect` exactly
once before private data and entvars are cleared. It then clears the view
binding, pending lifecycle reliable bytes, frame history, userinfo, session
generation, and reserved edict. A rejected pre-connect client does not receive
`ClientDisconnect`. Slot reuse receives a new generation and cannot inherit
private data, pointer fields, view, frame history, reliable state, or stale
packet authority.

Creating the Game DLL player entity and establishing the stock-client view
does not mean that clc_move execution, PM_Move, prediction, weapons,
damage, death, respawn, or complete multiplayer gameplay are implemented.
