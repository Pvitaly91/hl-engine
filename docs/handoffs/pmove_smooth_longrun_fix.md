# PM_Move smooth persistent-session fix

## Scope

The development baseline is
`87504e22ff0c4fea146bbab85dba9aff643053e6` and the task branch is
`codex/manual-pmove-smooth-longrun-fix`. The local HLSDK remains pinned at
`b1b5cf5892918535619b2937bb927e46cb097ba1` and is not modified. All runtime
interoperability tests use `127.0.0.1` and read the installed stock client,
Game DLL, delta descriptions, and map without changing them.

## Reproduction and classification

The stock-client baseline showed visible correction from the beginning and an
eventual movement-input freeze while the renderer and host could remain
active. The symptoms did not share one root cause.

The jerk was caused by a combination of incorrect backup/fresh command
partition semantics, synthetic replay inferred from raw netchan gaps, and a
snapshot being built before movement received in the same host frame. The
freeze was caused by a temporarily ahead command packet leaving the observed
frontier stalled, after which later packet sequences fed an increasing
gap/replay/rejection cascade.

## Final execution model

The decoded move array is chronological: explicit backup commands precede
fresh commands. The planner is side-effect free except for bounded diagnostic
counters. Separate observed, structurally validated, and successfully
executed packet frontiers are committed explicitly. Raw netchan gaps are
diagnostic and never create commands; recovery is limited to backup commands
actually carried by a validated move packet. The synthetic replay limit is
zero.

The first executable batch establishes a host-monotonic command-clock epoch.
Only a successful full batch advances committed command time and the executed
frontier. A temporary lead advances observed/validated state, executes no
movement, and remains recoverable after host-time catch-up. A PM_Move output
failure rolls back both authoritative player state and planner state.
Disconnect and slot reuse clear every frontier, command, clock, origin,
velocity, duck, and old-button value.

During established streaming, each bounded host pump receives datagrams,
executes and commits PM_Move, then builds at most one due snapshot. Its carrier
acknowledgement covers the client packet represented by the committed state.
Delayed host frames skip missed intervals instead of sending catch-up bursts.
The default remains 20 Hz.

## Prediction state

The signon path provides the stock view entity, movevars, clientdata, packet
entities, and continuous delta-frame acknowledgements. Snapshot state is
captured from the committed player edict. The Game DLL `Player_Encode`
conditional suppresses the local player's low-resolution entity origin, as in
the stock server, while the exact local origin and velocity are sent through
`clientdata_t`. Hull, flags, duck/water state, base velocity, view offset, and
movement timers are sourced coherently from the same commit.

## Verification design

Deterministic PM_Move tests cover a simulated ten-minute 100 Hz stream,
alternating 6/7 ms commands, idle/resume, unrelated packet-sequence gaps,
explicit backup recovery, temporary lead recovery, the former permanent
cascade, transactional rollback, same-frame movement/snapshot ordering,
20 Hz scheduler jitter, sequence wraps, and reconnect reset. The external
long-run proof starts the real `hlhost.exe` in a separate process, completes
the normal lifecycle over localhost UDP, injects bounded loss and timing
conditions, decodes continuous snapshots, and checks the 60/120/300/600
second movement boundaries.

The stock-client verifier launches the unmodified installed client through
Steam when available, keeps all traffic on localhost, verifies read-only input
hashes, applies 100 Hz test cvars only to the running process, captures bounded
visual checkpoints outside the repository, and exercises movement, idle,
jump, duck, collision, disconnect, and reconnect. It never edits client or
GameDir files.

## Known limitations

The authoritative slice remains world-BSP-only. Weapons, firing, damage,
items, moving platforms, ladders, complete water behavior, dynamic entity
touches, player-to-player collision, and multiplayer combat are outside this
milestone. Local temporary logs, screenshots, build artifacts, installed
client data, Game DLL files, map data, and SDK files are not commit artifacts.

## Final gate

The final Release build passed and CTest reported 12/12 passing. Legacy
PM_Move proofs A and B passed, including rollback, explicit backup recovery,
temporary clock-lead recovery, lifecycle reset, and clean shutdown. Launcher
DryRun, a five-second persistent server-only smoke, and feature-off normal
host startup also passed.

The independent external proof ran for 600.002 seconds. It recorded 12,008
server snapshots with scheduler median/p95/max intervals of 47/63/65 ms,
zero bursts, zero missed intervals, zero full-snapshot fallbacks, and zero
movement discontinuities. The maximum observed movement-execution gap was
2,375 ms during deliberate proof-side loss/timing injection. Four temporary
clock rejections recovered without a permanent cascade; clock
resynchronizations and synthetic/non-move-gap replays remained zero. Movement
continued across the 60/120/300/600-second boundaries and shutdown was clean.

The unmodified installed stock client completed a 610-second varied-input
run at approximately 100 commands per second. Its server-side snapshot
median/p95/max intervals were 47/62/64 ms with zero bursts, missed intervals,
or full fallbacks. The maximum movement-execution gap was 32 ms and movement
discontinuities were zero. Visual checkpoints showed an active player at
0/60/120/300/600 seconds, a disconnected state, and a fresh active player
after reconnect. Movement, idle/resume, jump, duck, static collision, and
reconnect passed; repeated snap-back and permanent freeze were not observed.
The final Game DLL `ServerDeactivate` callback completed before module unload,
and the host exited cleanly.

No build artifact, installed client/Game DLL/map data, SDK file, temporary log,
or checkpoint image is part of the commit. The commit and remote SHA are
verified after publication and reported by the task result because a commit
cannot embed its own final object ID.
