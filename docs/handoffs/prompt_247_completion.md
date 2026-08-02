# Prompt 247 completion handoff

## Identity and scope

- Workspace: `D:\DEV\CPP\HL-Engine`
- Baseline branch: `codex/manual-pmove-smooth-longrun-fix`
- Baseline commit: `981fd2dc1bc3b4fca0a1e27853a2bf6463dbfcee`
- Task branch: `codex/goldsrc-two-client-replication-slice`
- Valve HLSDK: `b1b5cf5892918535619b2937bb927e46cb097ba1`
- Stock target: Half-Life 1.1.2.2, Steam build 15961492
- Original boundary: `two_client_player_replication_required`
- Original blocker:
  `admitted_session_id_single_slot_snapshot_scheduler`
- Local interoperability scope: `127.0.0.1` only
- Gameplay active: no

The original server already issued a second challenge, admitted a distinct
second loopback endpoint, allocated slot/edict 2, and kept client A alive. The
blocking ownership was downstream: a single global admitted-session selector
and single-slot snapshot scheduler routed continuous lifecycle, ACK, movement,
and snapshot work through only one session.

## Delivered result

The runtime now routes two live endpoint-bound netchans to independent slot
generations. A maps to slot/edict/view 1 and serverinfo player index 0; B maps
to slot/edict/view 2 and player index 1. Sign-on state, reliable state, frame
history, acknowledged delta base, player lifecycle, private data, command
clock, PM_Move context, scheduler timing, disconnect cleanup, and diagnostics
are per client.

Each receiver gets its own clientdata and both connected player entities under
the `all_connected_players_plus_existing_modeled_entities_no_pvs` policy. The
Game DLL is called with the candidate and actual receiver. Add, Update,
Remove, and reconnect Add are represented in the receiver's packet-entity
stream. Commands for all ready clients are processed before bounded fair
round-robin snapshot sends.

Disconnect A cleans only A. B continues receiving snapshots and executing
PM_Move, observes A's removal, and remains eligible for movement. Reconnect A
creates a fresh slot generation, private-data lifecycle, frame history,
command epoch, and PM_Move state; B observes the re-Add. Stale A endpoint
traffic is rejected.

## Files and test surfaces

- Compatibility design and limits:
  `docs/compatibility/goldsrc_two_client_replication.md`
- External two-socket Proof A/B:
  `scripts/run_goldsrc_two_client_replication_proof.ps1`
- Required unit target: `goldsrc_two_client_replication_unit_tests`
- Stock manual launcher: `scripts/run_manual_two_client_test.ps1`
- Stock automatic launcher: `scripts/run_stock_two_client_autotest.ps1`
- Stock procedure:
  `docs/manual-testing/stock_two_client_replication_test.md`

The unit target covers admission, generations/edicts, netchan ACK isolation,
receiver clientdata, player ordering, Add/Update/Remove/re-Add, per-client
command clocks, rollback counters, and scheduler fairness. The PM_Move runtime
test additionally injects invalid A output while B is live and proves that B's
origin, clock, and later movement are not rolled back.

## Automated evidence

- Release `hlhost` build: pass
- `goldsrc_two_client_replication_unit_tests`: pass
- PM_Move rollback isolation unit: pass
- External localhost Proof A: pass
- External localhost Proof B: pass
- Two-client launcher DryRun: pass
- Two-client launcher `ServerOnly` smoke on `crossfire`: pass

Proof A observed two connected/spawned clients, slots/edicts 1 and 2,
independent movement, mutual visibility, remote Add/Update/Remove,
per-client frame history and delta bases, simultaneous input, B movement after
A disconnect, clean A reconnect/slot reuse, and clean shutdown. Proof B
observed duplicate and third-client rejection, endpoint/reliable/frame ACK
isolation, malformed-client isolation, command-clock/rollback isolation,
stale-packet rejection, surviving-client responsiveness, clean reuse, no
cross-client state leak, and clean shutdown.

The final Proof A rerun recorded 310/345 snapshots, 172/244 PM_Move calls, and
159/56 acknowledged frames for A/B respectively. The final CTest rerun passed
13/13 tests.

## Prompt 247A automatic stock-client result

- Previous blocker:
  `stock_goldsrc_single_instance_rejected_client_b_before_network_handshake`
- Previous working launcher found under the permitted DEV roots: no
- Previous working launcher path: missing
- Previous working multi-instance method: missing
- Implemented method:
  `direct_hl_exe_steam_multirun_distinct_clientport_and_name`
- Launcher mutex fallback used: no
- External safety patch:
  `C:\Users\admin\AppData\Local\Temp\hl-engine-prompt247-before-dual-client-launcher-20260802-090200.patch`
- Primary external result:
  `C:\Users\admin\AppData\Local\Temp\HL-Engine\stock-two-client\20260802-093336-662-3ec5a157\stock_two_client_autotest.json`

Before the 247A change, both launch requests returned distinct PIDs, but only
Client A created a Half-Life game window and network endpoint. Client B
remained a stock single-instance error process for the bounded 40-second
launcher run and never issued a server challenge. The pre-fix argument set was
`-steam -game valve -console -novid -windowed -w 960 -h 540 +connect`; it had
no multi-instance flag, unique name, or explicit client port.

The dedicated autotest now launches `hl.exe` directly with
`-steam -multirun -insecure`, distinct names, and client ports 27005/27006
(27007 for reconnect). It validates exact new-PID ownership, UDP ownership,
server handshake/spawn markers, automatic stock input, remote replication,
disconnect/reconnect, clean slot reuse, external JSON/TXT output, and exact
owned-PID cleanup. DryRun and its ownership/cleanup self-tests pass under
Windows PowerShell 5.1.

The final primary real run created Client A PID 11664 and Client B PID 29016. Client
A owned UDP 27005, connected, and spawned. Client B was classified as
`client_b_multirun_rejected` after 3252 ms, before UDP 27006 or any Client B
challenge. Both clients and the server were cleaned; `process_leak=false`.
The same process-level outcome was reproduced for shared and separate stock
executable paths, both `-steam`/`-multirun` orderings, and a no-`-steam`
comparison. No arbitrary handle or mutex manipulation was attempted.

## Prompt 247B exact stock launcher-mutex completion

- Previous blocker:
  `stock_goldsrc_single_instance_rejected_client_b_before_network_handshake`
- Confirmed failure: `client_b_multirun_rejected`
- Resolution:
  `exact_launcher_mutex_handle_closed_in_owned_stock_client`
- Backend: repository-owned `goldsrc_instance_unlocker` x64 helper
- Actual exact kernel object name: `ValveHalfLifeLauncherMutex`
- Prompt shorthand: `LauncherMutex`
- Object type: `Mutant`
- Process injection: no
- Memory patching: no
- Client files modified: no
- Steam terminated: no
- Elevated PowerShell required on this machine: no
- External Prompt 247B safety patch:
  `C:\Users\admin\AppData\Local\Temp\hl-engine-prompt247b-20260802-101841.patch`

The x64 helper verifies exact target PID, canonical image and recorded process
creation time. It enumerates only that PID, filters handles to the kernel
`Mutant` type, and accepts exactly one exact object-name match. Close mode uses
`DUPLICATE_CLOSE_SOURCE` only after revalidating the same type/name. It emits
no unrelated handle names. Unit tests pass for inspect, exact close, wrong
PID/image/creation/name, ambiguous matches, repeated close, and preservation
of a separate control mutex; `unrelated_handles_closed=0`.

The PowerShell layer additionally verifies that the target is a new owned
non-pre-existing `hl.exe`, is alive, belongs to the current interactive
user/session, owns its expected UDP port, is associated with the expected
server slot, and has spawned. The transient B process is never eligible.

Auto mode first reproduced a 1450 ms stock single-instance B process. It
opened no UDP endpoint and produced no server handshake; only that exact PID
was cleaned. A held exactly one verified launcher mutex. After closing it,
retry B became a persistent independent process on UDP 27006, connected as
slot/edict 2, spawned and moved concurrently with A.

The stock client did not reliably send a protocol disconnect when its window
was closed, and sampled snapshot diagnostics were not valid progress gates.
The final harness therefore uses a one-shot external localhost server request
file for the normal per-client slot-1 disconnect/Game DLL callback, then
closes only owned A. Remove and Re-Add are verified from exact per-receiver
shutdown counters rather than sampled diagnostic lines. B survives, its exact
launcher mutex is closed before A reconnect, and the fresh A session reuses
the slot without cross-client state.

Primary Auto lifecycle result:
`C:\Users\admin\AppData\Local\Temp\HL-Engine\stock-two-client\20260802-114835-603-5cd8c8eb\stock_two_client_autotest.json`.
It passed two distinct processes/ports, both handshakes/spawns/movement,
mutual Add/Update, simultaneous movement, 60/120-second checkpoints,
disconnect/Remove, reconnect/Re-Add, clean reuse, no cross-client state leak,
and `process_leak=false`.

The 600-second MutexUnlock result is:
`C:\Users\admin\AppData\Local\Temp\HL-Engine\stock-two-client\20260802-115155-188-4576dce0\stock_two_client_autotest.json`.
It passed the 60, 120, 300 and 600-second two-client checkpoints with both
clients moving and no state/process leak.

Final verification includes Release build PASS, CTest 13/13, x64 helper tests,
PowerShell parser/DryRun, mutex inspect-only, two-client Probe A/B, PM_Move
Proof A/B, the prior 600-second persistent PM_Move proof, stock Auto lifecycle,
stock 600-second run, and feature-off acceptance/drift with
`normal_host_behavior_changed=0`.

## Known limits and next boundary

Two-client movement and mutual player visibility do not mean that
player-to-player collision, weapons, firing, damage, death, respawn,
scoreboard, lag compensation, or complete Half-Life Deathmatch gameplay are
implemented.

The next observed boundary must be assigned after the two-client stock run;
combat is outside Prompt 247.

## Repository hygiene

The Half-Life SDK, installed Game DLL, stock clients, maps, `valve` data,
runtime logs, captures, build trees, and generated binaries are excluded from
the task commit. The pre-existing `src/game_api/dll_module.cpp` worktree
metadata change and pre-existing log wrapper directory are user-owned and are
not staged.
