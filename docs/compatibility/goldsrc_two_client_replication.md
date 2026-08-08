# GoldSrc two-client replication

## Initial single-client inventory

This inventory was completed before the Prompt 247 runtime implementation.
The baseline is commit `981fd2dc1bc3b4fca0a1e27853a2bf6463dbfcee`.

### `global_single_client_assumptions`

- `GoldSrcUdpHandshakeRuntime::admitted_session_id_` names one session and is
  used by helper paths that select only one admitted slot.
- Host-frame fragment expiry, pre-receive snapshot scheduling, post-receive
  snapshot scheduling, lifecycle reconciliation, and completion evaluation
  call `FindAdmittedNetchanSlot()` once instead of iterating live slots.
- A newly accepted connection overwrites the admitted-session identifier; a
  disconnect clears it even when another slot remains connected.
- The initial netchan send and legacy negative-proof ordinary advance resolve
  their destination through the single admitted-session identifier.
- Continuous-stream start time, first-movement time, last counted frame ACK,
  last movement commit sequence/host frame, reconnect/reset markers, and
  several cadence/diagnostic cursors are runtime-global.
- The persistent manual-session lifecycle models one connected client.
- Completion and success predicates expect one accepted session and one
  selected lifecycle/snapshot stream.
- The snapshot capture path produces one player entity and that same player's
  clientdata; it has no receiver-specific remote-player candidate list.
- Snapshot builders accept at most one `GoldSrcPlayerSnapshotInput` and apply
  local-player conditional encoding to that single entity.
- Shutdown summaries and live summary synchronization describe one selected
  client even when authoritative slot storage contains more occupants.
- The existing manual launcher supplies `--maxclients 1`, and legacy proof
  assertions deliberately expect `session_count=1`.

### `per_slot_state_already_available`

- Deterministic dedicated slot number, session ID, player name, userinfo,
  endpoint, protocol, qport/channel identity, challenge, and authentication
  metadata.
- Independent `GoldSrcNetchanState`, including endpoint binding, sequences,
  reliable toggles, acknowledgements, fragmentation, and transport phase.
- Independent `GoldSrcSignonSessionState`, serverinfo payload/context, delta
  registry/bundle, resource manifest preparation, encoded responses, and
  signon bootstrap storage.
- Independent `GoldSrcFirstSnapshotSessionState`, which already owns bounded
  frame history, acknowledged-frame selection, first/continuous frame IDs,
  and cadence scheduling.
- Independent `GoldSrcPlayerLifecycleSession` and generation-checked edict
  binding. The shared edict registry already supports slot-to-edict bindings
  for all configured clients.
- PM_Move command execution state is indexed by client slot and already
  isolates command clocks, replay suppression, authoritative movement state,
  rollback, and reset.
- Admission planning already selects the first free bounded slot. Sequenced
  datagram lookup already scans live netchans for the exact remote endpoint.

### `state_that_must_become_per_client`

- All admitted-session lookup and initial/reliable send destinations.
- Snapshot-due evaluation, stream start time, first-movement time, counted ACK
  frontier, cadence diagnostics, and movement/snapshot coherence markers.
- Signon/lifecycle reconciliation and completion evaluation.
- Disconnect/reconnect generation markers and all cleanup that is currently
  global but semantically owned by one slot.
- Receiver-specific player entity selection, local clientdata, local-player
  conditional encoding, remote Add/Update/Remove accounting, and remote
  movement continuity metrics.
- Per-client snapshot, ACK, PM_Move, lifecycle, view entity, disconnect, and
  reconnect summaries, plus bounded fair send-scheduler state.

### `summary_only_single_client_fields`

The legacy flat summary fields for allocated slot/session/endpoint, signon
phase, lifecycle callbacks, player edict/view, snapshot counts, frame ACK/base,
cadence, netchan counters, PM_Move counters, movement milestones, disconnect,
and reset describe only the most recently synchronized slot. They remain a
single-client compatibility view for old proofs. Prompt 247 adds explicit
per-slot summaries and aggregate two-client fields instead of interpreting the
legacy view as authoritative multi-client state.

### `transport_routing_single_client_fields`

- `admitted_session_id_` and `FindAdmittedNetchanSlot()`.
- `SendInitialNetchanDatagram()` without an explicit slot.
- `SendPendingOrdinaryAdvance()` without an explicit slot.
- One boolean `negative_advance_pending_` shared by all sessions.
- One global fragment-expiry check and one global snapshot destination.
- Disconnect logic that clears the global admitted identity and scheduler
  state for every client.

The authoritative routing contract for Prompt 247 is exact live endpoint plus
the endpoint-bound netchan's qport/channel identity and current slot session
generation. The visibility policy is
`all_connected_players_plus_existing_modeled_entities_no_pvs`; it is a
deterministic loopback compatibility policy, not full PVS/PAS parity.

## Baseline revalidation note

The unchanged baseline passed the Release build, CTest 12/12, handshake,
netchan, serverinfo, delta/resource/fragmentation, world baseline, first
snapshot, positive continuous snapshot, positive player lifecycle, PM_Move
Proof A/B, the 600-second PM_Move long-run, launcher DryRun, persistent
server-only smoke, and feature-off host run. The standalone negative
continuous-snapshot wrapper and the lifecycle wrapper mode that embeds it
exited early at their stale-frame checkpoint on the unchanged baseline; this
was recorded before C++ changes and its assertions will not be weakened.

## Implemented architecture

The task branch is `codex/goldsrc-two-client-replication-slice`. The local
Valve HLSDK remains read-only at
`b1b5cf5892918535619b2937bb927e46cb097ba1`.

The global `admitted_session_id_` routing owner is removed. Connectionless
admission chooses the first free bounded slot, while sequenced traffic is
routed through the exact live endpoint-bound netchan. The slot/session
generation, endpoint, challenge metadata, netchan sequences and reliable
state, sign-on state, encoded bootstrap state, frame history, acknowledged
delta base, lifecycle, edict binding, userinfo, command clock, PM_Move context,
snapshot scheduler, and diagnostics remain independently owned by each slot.

With `maxclients=2`, the deterministic mapping is:

| Client | Slot | Serverinfo player index | Edict/view entity |
| --- | ---: | ---: | ---: |
| A | 1 | 0 | 1 |
| B | 2 | 1 | 2 |

The same loopback address is not treated as a duplicate. Distinct source
ports/endpoints can own A and B concurrently. A duplicate connected endpoint
is rejected, and a third endpoint receives the stable server-full outcome.
Host-frame receive work is bounded and scans all ready datagrams; outgoing
work uses bounded round-robin slot selection. Commands received in a frame are
executed before receiver-specific snapshots are built.

## Receiver-specific snapshots

Each receiver snapshot contains its own player plus every other connected,
materialized player accepted by the Game DLL's `AddToFullPack`. The callback
receives the candidate entity and the actual receiver as host. The engine shim
implements `pfnCheckVisibility` for this compatibility slice so that a null
visibility set follows the documented no-PVS policy. Other Game DLL FullPack
filters remain authoritative.

The selected visibility policy is
`all_connected_players_plus_existing_modeled_entities_no_pvs`. Player records
use `entity_state_player_t`, remain strictly ordered by edict number, and are
deduplicated before encoding. Only the receiver's own `clientdata_t` is
attached to its frame; remote clientdata is never copied into another
receiver's stream.

Each client's acknowledged frame selects only that client's delta base.
Joining players produce Add, movement produces Update, disconnect produces
Remove, and a fresh occupant of the reused slot produces a new Add. A full
`packetentities` fallback also semantically replaces the receiver's entity set,
so a previously visible player omitted from a full frame is treated as a
removal.

The runtime records per-receiver snapshot gaps and remote-origin continuity.
At 20 Hz, no-PVS remote position changes are carried in the same authoritative
movement state committed before snapshot construction.

## Lifecycle, movement, and cleanup

ClientConnect, userinfo application, ClientPutInServer, private data, spawn,
view entity, and ClientDisconnect are evaluated per slot. Edict 1 and edict 2
hold distinct private-data allocations. Disconnect resets only the affected
netchan, sign-on state, reliable fragments, frame history, delta base,
lifecycle generation, command state, PM_Move state, and per-session timing.
The surviving client's snapshot and movement schedulers continue running.

Slot reuse constructs a fresh runtime slot before admission commit, binds a
new lifecycle generation, and rejects traffic for the stale disconnected
endpoint. No frame ACK, reliable ACK, clientdata, view, position, command
clock, PM_Move rollback, or private-data identity crosses between clients or
generations.

## Verification surfaces

`goldsrc_two_client_replication_unit_tests` covers deterministic admission,
slot/generation/edict mapping, netchan reliable and frame-ACK isolation,
receiver-local clientdata, two-player ordering, Add/Update/Remove/re-Add,
independent command clocks, rollback counters, and scheduler fairness. The
PM_Move runtime test deliberately returns an invalid output for A while B is
active and verifies that A rolls back without changing B's origin, command
clock, or subsequent movement.

`scripts/run_goldsrc_two_client_replication_proof.ps1` runs one real built
server and two independent localhost UDP sockets. Proof A covers independent
sign-on, lifecycle, PM_Move, mutual Add/Update visibility, simultaneous input,
per-client history/base selection, A disconnect, B survival and movement,
Remove, clean A reconnect/re-Add, and shutdown. Proof B covers duplicate/full
admission, endpoint/reliable/frame isolation, malformed input isolation,
command-clock and PM_Move rollback isolation, stale disconnected traffic,
slot reuse, and state-leak rejection.

`scripts/run_manual_two_client_test.ps1` provides DryRun, ServerOnly,
automatic or manual B launch, explicit A/B executable paths, a persistent
duration-zero session, bounded-duration smoke, external temporary logs, and
owned-process-only cleanup. The required stock-client observation procedure is
in `docs/manual-testing/stock_two_client_replication_test.md`.

## Current limits

Two-client movement and mutual player visibility do not mean that
player-to-player collision, weapons, firing, damage, death, respawn,
scoreboard, lag compensation, or complete Half-Life Deathmatch gameplay are
implemented. `gameplay_active` remains `no`.

## Prompt 248 smoothness and long-run contract

The two reported stock-client symptoms were independent. Remote stepping was
`permanent_spawn_ef_nointerp_replication`: the ordinary Game DLL spawn flag
was copied into every later receiver sample. The engine now retains
`EF_NOINTERP` only for the first materialized transition sample and clears it
from subsequent network state without mutating Game DLL entvars. Remote
`animtime` follows monotonic server time, and receiver diagnostics cover
origin, velocity, sequence, frame, framerate, effects, interpolation state,
consecutive presence, gaps, and deltas.

The keyboard symptom was `script_input_left_pressed`: persistent launch-time
`+forward`, `+moveright`, and reconnect `+back` commands contaminated manual
input and could press a player continuously into geometry. All launch-time
movement holds are removed. Manual observation uses `InputMode None`; the
automatic mode uses bounded physical scan-code W/S or D/A press/release pulses
plus a short jump or duck. It resolves the visible window for the exact new
PID, verifies its executable and foreground ownership, repeats key-down only
during the bounded hold, releases all six movement keys in `finally`, verifies
release, and fails if server-observed per-key input counters do not advance.

Outgoing work is now bounded and ordered as required reliable/fragment
progress, one due snapshot per active client with rotating fairness, then an
optional coalesced empty ACK when no reliable or snapshot carrier already
acknowledges the current incoming frontier. Movement remains receive-before-
snapshot coherent. Normal stock observation after the fix recorded A
snapshot median/p95/max gaps of 47/63/80 ms, B gaps of 47/63/79 ms, remote
update p95 of 63 ms in both directions, and zero snapshot starvation.

The stock acceptance gate no longer treats lifetime or cumulative
`pmove_calls > 0` as continued movement. At each 10/30/60/120/300/600-second
checkpoint it requires fresh clc_move receive/validate/execute, PM_Move,
movement snapshot, horizontal authoritative displacement, outgoing snapshot,
frame-ACK, and remote update progress for both clients. Per-pulse start/end
origins and counter deltas are retained, so a jump/duck Z change cannot satisfy
the reversible horizontal gate. The harness consumes live bounded semantic
summaries and never depends on shutdown-only output or raw packet logging.
Deterministic regressions additionally cover
shared-budget saturation, interpolation eligibility, and 600 simulated
seconds with independent clocks, loss, backup commands, idle/resume, bursts,
reliable traffic, and ACK patterns.

Protocol interpolation acceptance is deliberately separate from visual
acceptance. The protocol gate requires no permanent remote `EF_NOINTERP`,
monotonic remote animtime, no re-Add during an uninterrupted connection,
remote-update p95 at most 80 ms, ordinary maximum at most 160 ms, snapshot p95
at most 80 ms, advancing ACKs, and zero snapshot-starvation frames after
startup warm-up. Only the user's five explicit answers from the 600-second
`InputMode None` observation can establish visual smoothness and keyboard
control.

Prompt 248B removes map-route dependence from the generic long-run gate. A
stable, valid spawn anchor is stored per slot and network session generation.
The disabled-by-default test control is accepted only on `127.0.0.1` with
persistent PM_Move and an external path. A reset preserves the connection,
Game DLL private data, lifecycle generation, netchan, frame history and PM_Move
counters. It is represented by at most one no-interpolation transition and is
excluded from steady-state interpolation and movement-distance samples.

Movement pipeline health (verified input plus advancing clc_move, PM_Move,
snapshots and ACKs) is independent of travel. Solid BSP can block one
direction while the pipeline remains healthy; adaptive bounded directions
then establish liveness without traversing `crossfire`. Specific ramp/step
behavior remains a separate diagnostic and is not a prerequisite for the
generic 600-second network test.
