# Prompt 248 completion handoff

## Identity and scope

- Workspace: `D:\DEV\CPP\HL-Engine`
- Baseline commit: `30c6a98accfc5bf62f4ee984950a67ec1a707bfa`
- Previous two-client commit: `20925a53944af7d8d89d3aaf9bbaea1170d44e25`
- Task branch: `codex/two-client-smoothness-longrun-fix`
- Valve HLSDK: `b1b5cf5892918535619b2937bb927e46cb097ba1`
- External backup patch:
  `C:\Users\admin\AppData\Local\Temp\hl-engine-prompt248a-20260802-174701.patch`
- Prompt 248B external backup patch:
  `C:\Users\admin\AppData\Local\Temp\hl-engine-prompt248b-20260802-213949.patch`
- Transport scope: `127.0.0.1` only
- Gameplay active: no

This change fixes the stock-client replication and validation boundary. It
does not add weapons, damage, death, respawn, scoreboard, lag compensation,
player collision parity, or complete multiplayer gameplay.

## Independently classified observations

The stepped remote player and the reported keyboard freeze had different
causes.

`remote_step_root_cause=permanent_spawn_ef_nointerp_replication`: the public
Game DLL marks the ordinary spawn transition with `EF_NOINTERP`. The engine
was copying that transition flag into every later receiver snapshot. The
stock client therefore had regular samples but was continually told not to
interpolate them.

`local_freeze_root_cause=script_input_left_pressed`: the old stock launcher
put persistent `+forward`, `+moveright`, or reconnect `+back` commands on the
client command line. Those artificial held buttons contaminated manual
W/A/S/D testing and could keep players pressed against map geometry. Mouse
look remained independent, which made process uptime look healthy without
proving authoritative keyboard movement.

`same_root_cause=no`. Normal localhost captures did not show outgoing
snapshot starvation as the cause of either observation. The scheduler was
still hardened so sustained two-client movement traffic cannot regress into
that failure mode.

The earlier autotest was a false positive because it accepted cumulative
`pmove_calls > 0` and client uptime. It did not require fresh receive,
validation, execution, PM_Move, origin, snapshot, frame-ACK, and remote-update
progress at each long-duration checkpoint, and it had no visual interpolation
gate.

## Runtime changes

For each receiver, the engine preserves `EF_NOINTERP` only for the first
materialized spawn/transition sample and clears it from subsequent network
samples without changing Game DLL entvars. Network `animtime` advances with
monotonic server time. Origin, velocity, sequence, frame, framerate, effects,
presence, and update intervals are recorded per receiver. Smoothness requires
consecutive remote presence, monotonic animtime, no permanent no-interpolate
state, coherent motion, no discontinuity, 80 ms p95 or better, and no ordinary
remote-update gap above 160 ms after cadence warm-up.

The old host-frame order could spend the shared outgoing budget on an empty
ordinary ACK for each received movement datagram before due snapshots were
scheduled. The final bounded priority is:

1. required reliable/fragment progress;
2. one due snapshot per active streaming client, using a rotating fair start;
3. an aged optional empty ACK only when no reliable or snapshot carrier has
   already acknowledged that client's newest incoming frontier.

Multiple incoming packets for one client coalesce into one pending empty ACK.
The datagram limit remains bounded; the solution does not raise it to hide
pressure. Movement is still received and executed before snapshot
construction, and one client cannot consume the other client's reserved
snapshot opportunity.

Per-client summaries now expose due/sent/deferred/starved snapshots, interval
percentiles, remote update percentiles, remote origin deltas and velocity,
animation/interpolation state, clc_move stages, movement snapshots,
command-clock rejection/recovery, ACK carrier counts, progress times, and
authoritative origin/velocity. Diagnostics are sampled rather than emitted
for every packet. The stock harness reads these bounded semantic server
summaries while the server is live; it does not copy raw datagrams or wait for
shutdown before deciding a checkpoint.

## Input and acceptance changes

The stock launcher no longer puts persistent movement commands on any launch
line. `InputMode None` leaves every movement control released and is required
for `ManualObservation`. The only automatic mode is `VerifiedSendInput`. It
finds a visible top-level window owned by the exact newly launched `hl.exe`
PID, verifies its executable path, activates it, and verifies that the
foreground window still belongs to that PID before sending input. Hardware
scan-code key events are repeated at a bounded cadence during each short hold
so the stock DirectInput path observes them. W/S and D/A use equal 1000 ms
reversible phases; Space and Left Ctrl use 100 ms press/release phases.

Every key-down has a key-up in `finally`. Before and after each phase, before
disconnect/reconnect, and in the outer cleanup, the harness releases W, A, S,
D, Space, and Left Ctrl again. `GetAsyncKeyState` verifies release; a sticky
key, foreground mismatch, missing owned window, failed `SendInput`, or input
counter that does not advance is a precise hard failure. Input is never sent
to an unrelated process.

Automatic checkpoints at 10, 30, 60, 120, 300, and 600 seconds compare live
per-client counter snapshots. Every active movement window requires fresh
received, validated and executed moves, PM_Move calls, movement snapshots,
horizontal authoritative displacement, outgoing snapshots, acknowledged
frames, and remote updates. Each individual pulse records start/end origin,
displacement, PM_Move, executed-command, snapshot, frame-ACK, and peer remote
update deltas. Vertical jump/duck motion cannot satisfy the horizontal
movement gate, and a process remaining alive is not sufficient.

Manual observation launches two stock clients with no scripted buttons,
arranges their exact owned windows side by side when possible, prints their
PIDs and loopback endpoint, and emits safe per-client counters every five
seconds. At the end it asks five independent Y/N questions for local A, local
B, A observing B, B observing A, and any keyboard loss. It never synthesizes
positive answers and cleans only its owned processes.

## Deterministic and stock evidence

The saturation regression drives both clients to the receive limit for many
host frames with periodic reliable demand. It verifies bounded sends, reliable
completion, correct ACK frontiers, empty-ACK coalescing, rotating fairness,
and zero snapshot starvation.

The interpolation regression accepts coherent monotonic samples and rejects
permanent `EF_NOINTERP`, non-monotonic `animtime`, and incoherent motion. The
600-second simulated regression uses independent client cadences, simultaneous
bursts, idle/resume periods, bounded loss, backup commands, temporary clock
lead, reliable demand, and independent frame ACKs. Both clients continue
PM_Move, snapshots, ACK progress and mutual remote updates at 60, 120, 300 and
600 simulated seconds without deadlock, starvation, or cross-client state
leak.

A real two-stock-client network observation before the final Prompt 248A
acceptance recorded
snapshot median/p95/max intervals of 47/63/80 ms for A and 47/63/79 ms for B.
Both remote-update p95 values were 63 ms, maximum snapshot starvation was
zero, remote animtime was monotonic, consecutive presence held, and permanent
remote `EF_NOINTERP` was absent. Final automatic 600-second and manual visual
acceptance results must be recorded here only after those runs complete.

Prompt 248A helper syntax/DryRun and multiple shorter stock runs passed while
the harness was being hardened. The required final 600-second run remains
unaccepted until a complete checkpoint result exists. The latest environment
then stopped before sign-on: the exact stock `hl.exe` loaded `hw.dll` and the
Steam API but produced neither a client window nor its requested loopback UDP
endpoint; a normal Steam `-applaunch 70` attempt also produced no new client.
Steam was not terminated or modified. This external launcher state is not
reported as an engine movement failure, and no commit is permitted while the
automatic and manual gates remain incomplete.

Further observation showed that Steam queued several launch requests and
materialized new windowless `hl.exe` PIDs tens of seconds after the requesting
harness invocation had already failed. The harness now sends only one initial
A launch request, records the complete pre-test PID set, observes a 60-second
post-failure stabilization window, counts delayed exact-path PIDs, and reports
any remaining new PID as `process_leak=true`. It does not terminate a delayed
PID whose unique test identity cannot be proven. The current external session
showed this delayed queue during the final audit; the last PID subsequently
exited naturally and the final `hl.exe`/`hlhost.exe` count was zero. The failed
run is still not accepted, and future runs now report the delayed-PID evidence
instead of claiming an immediate clean result.

The final one-attempt smoke result is
`C:\Users\admin\AppData\Local\Temp\HL-Engine\stock-two-client\20260802-211404-398-9fe38d9a\stock_two_client_autotest.json`.
It reports `client_a_multirun_rejected`, one delayed exact-path PID observed
during the stabilization window, `process_leak=false` after that PID exited,
and zero remaining `hl.exe`/`hlhost.exe` processes.

The final independent regression pass on this working tree recorded: Release
build pass, CTest 13/13, PM_Move Proof A/B pass, two-client Probe Proof A/B
pass, x64 LauncherMutex helper tests pass, feature-off host pass with
`normal_host_behavior_changed=0`, and the 600.007-second PM_Move long-run pass.
The long-run executed 11,602 movement commands, delivered 11,609 snapshots,
had scheduler snapshot p95/max gaps of 62/78 ms, passed movement at
60/120/300/600 seconds, and shut down cleanly. The two-client unit executable
also covers bounded snapshot saturation, zero deferral with normal capacity,
remote interpolation rejection cases, and its deterministic simulated
ten-minute two-client checkpoint run.

## Remaining boundary

The next boundary is `multiplayer_gameplay_and_combat`. Prompt 248 deliberately
stops before combat or full Half-Life Deathmatch behavior.

## Prompt 248B anchor-isolated acceptance

The failed 120-second stock observation was a checkpoint-route design failure,
not a keyboard or PM_Move freeze. A had reached the lower concrete ramp and B
the second-floor window ledge/wall in `crossfire`. Both continued to advance
clc_move receive/validate/execute, PM_Move, snapshots, frame ACKs, and peer
remote updates while the fixed input direction pointed into solid BSP. Zero
travel in that condition is now classified separately as
`pipeline=pass, travel=blocked_by_geometry`.

The loopback-only `--goldsrc-stock-test-control-file` option accepts atomic,
generation-checked `reset_player_to_spawn_anchor` requests from an external
path. A safe anchor is captured only for a connected, signed-on, spawned,
stable player at a valid BSP position. Reset restores origin, angles, hull and
ground state; clears velocity, basevelocity, buttons and oldbuttons; and
preserves lifecycle, private data, netchan, frame history, PM_Move state and
counters. Every acknowledgment contains a sanitized request ID, slot, status
and reason. The launcher waits for the exact request ID, two later snapshots,
and an origin within four units of the anchor before delivering input.

Reset teleports are one receiver-local `EF_NOINTERP` transition. That sample
is excluded from cadence/discontinuity statistics and the next ordinary
snapshot must clear no-interpolation. The successful long-run recorded
`nointerp_cleared_after_reset=1` for both slots and retained
`permanent_remote_nointerp=no`.

Checkpoint pulses are 150 ms on this installation. Longer nominal holds were
measured at 103-133 units and exceeded the 96-unit test radius; 150 ms kept the
observed maximum at 67.36 units for A and 66.93 for B while remaining far
above the two-unit liveness threshold. Each attempt begins and ends at the
anchor. Directions are adaptive (A: W/S/A/D; B: D/A/W/S), and a blocked first
direction is not reported as a keyboard freeze when the pipeline advances and
another direction travels.

The completed automatic result is
`C:\Users\admin\AppData\Local\Temp\HL-Engine\stock-two-client\20260802-223247-082-caafbc53\stock_two_client_autotest.json`.
It passed movement at 10/30/60/120/300/600 seconds with the same two client
processes, zero snapshot starvation, no sticky key, protocol interpolation
pass, and `process_leak=false`.

The lifecycle result is
`C:\Users\admin\AppData\Local\Temp\HL-Engine\stock-two-client\20260802-224512-664-46132e50\stock_two_client_autotest.json`.
Disconnect A, survivor B movement, reconnect A, re-Add, reconnect movement and
clean slot reuse all passed. A late unowned `hl.exe` appeared after owned
cleanup, so this result correctly remains unacceptable for the cleanup gate.
The next manual invocation returned
`blocker=preexisting_stock_client_detected` without launching or modifying it.
Manual visual acceptance therefore remains unconfirmed and no commit is
permitted yet.

A later 600-second `InputMode None` observation started both owned stock
clients with automatic input and reset disabled, but the PowerShell process
terminated after about 108 seconds with the machine reporting paging-file
exhaustion. Exact owned cleanup completed with `process_leak=false` and the
phase cleanup barrier passed, but the observation duration and user answers
were not completed. A subsequent lifecycle retry encountered a stock-launcher
B transient before UDP ownership; another retry was interrupted by the same
system paging-file condition and its exact current-run client/server PIDs were
verified and removed. Final process counts were zero. These environmental
failures are not accepted as reconnect or manual-visual evidence.

External working-tree backups:

- `C:\Users\admin\AppData\Local\Temp\hl-engine-prompt248a-20260802-174701.patch`
- `C:\Users\admin\AppData\Local\Temp\hl-engine-prompt248b-20260802-213949.patch`
