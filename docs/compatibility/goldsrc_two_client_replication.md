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
