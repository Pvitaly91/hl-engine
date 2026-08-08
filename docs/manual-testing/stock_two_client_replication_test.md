# Stock two-client replication test

This procedure validates Prompts 247 and 248 with two unmodified Half-Life 1.1.2.2
Steam build 15961492 clients on `127.0.0.1` and `crossfire`. It does not
modify either client, the installed `valve` directory, Steam credentials, or
the Game DLL.

## Automatic test

From `D:\DEV\CPP\HL-Engine`, run:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File .\scripts\run_stock_two_client_autotest.ps1 `
  -MultiInstanceMode MutexUnlock `
  -InputMode VerifiedSendInput `
  -FollowServerLog
```

The script discovers the stock `hl.exe` and launches it directly. It does not
use `steam.exe -applaunch`. Both clients receive `-steam -multirun -insecure`,
`-game valve`, console/window arguments, distinct names, and distinct
`+clientport` values. No client launch line contains a persistent movement
button:

- Client A: `27005` and `HL_Engine_Client_A`;
- Client B: `27006` and `HL_Engine_Client_B`;
- Client A reconnect: `27007` and `HL_Engine_Client_A_Reconnect`.

The launch delay defaults to three seconds, but a delay is never treated as
proof. The script requires two new owned `hl.exe` process identities, distinct
UDP ports, two server challenge/connect flows, slots/edicts 1 and 2, and two
materialized players. A pre-existing `hl.exe` is never adopted.

`InputMode VerifiedSendInput` is the default and only automatic input mode. It
uses hardware scan-code `SendInput` events against one exact owned client
window at a time. Before delivery it verifies the PID, executable path, visible
top-level window, foreground window, and foreground owner. W/S and D/A are
bounded reversible 1000 ms phases; jump and duck are 100 ms phases. The
server-observed input counters must advance for every pulse. Every path sends
key-up in `finally`, releases all six movement keys again, and verifies release
with `GetAsyncKeyState`. Blind `SendKeys` and console movement aliases are not
used.

Success requires fresh per-client progress at every active checkpoint: move
packets received, validated and executed; PM_Move calls; movement snapshots;
horizontal authoritative movement during the reversible phases; outgoing
snapshots; frame ACKs; and remote updates observed by the peer. Each pulse
records its own start/end origin and counter deltas. Jump or duck displacement,
cumulative activity, and process uptime are not accepted as proof.

After mutual replication, the script writes a one-shot external request file
that asks the localhost server to disconnect slot 1 through its normal
per-client lifecycle/Game DLL callback. It then closes only owned Client A,
verifies that B survives and records A's Remove, closes B's exact launcher
mutex, and relaunches A on port `27007`. The new PID/session, clean slot reuse,
remote re-Add, reconnect movement, and cross-client isolation are required.
Cleanup first requests graceful close and falls back only to `Stop-Process`
for the exact owned PID. Steam is never terminated.

## Multi-instance modes and exact mutex helper

The stock kernel object is exactly `ValveHalfLifeLauncherMutex`; Prompt 247B
uses `LauncherMutex` as its short descriptive name. The repository helper
does not use substring matching. It accepts only the exact requested name or
the same exact final object-manager path component, verifies object type
`Mutant`, and refuses zero or multiple matches.

The preferred backend is the repository-owned Windows x64 target:

```powershell
cmake -S . -B .\out\build\vs2022-x64-tools `
  -G 'Visual Studio 17 2022' -A x64
cmake --build .\out\build\vs2022-x64-tools `
  --config Release --target goldsrc_instance_unlocker
```

It is intentionally absent from the Win32 engine build. The helper verifies
the caller-supplied PID, exact executable image and process creation time,
enumerates only that PID's handles, filters to `Mutant`, and closes only one
exact match with `DUPLICATE_CLOSE_SOURCE`. It prints no unrelated handle
names. It performs no injection, memory patching, DLL loading into the client,
or client-file modification.

The PowerShell ownership gate additionally requires a new non-pre-existing
owned PID, a live exact image, the recorded creation time, the current
interactive user/session, the expected owned UDP port, a server association,
and completed spawn. The transient B process and all pre-existing processes
are ineligible.

Available modes are:

- `Auto` (default): attempt direct `-multirun`; if B is the bounded known
  single-instance transient and has no UDP/handshake, clean only that PID,
  unlock A's exact mutex, and retry B;
- `MultirunOnly`: preserve the old direct attempt and never close a mutex;
- `MutexUnlock`: unlock owned A before the first B launch.

`-DisableLauncherMutexFallback` makes Auto stop at the confirmed old blocker.
`-InstanceUnlockerPath` selects an already-built x64 helper. An optional
preinstalled Sysinternals path may be supplied with `-HandleExecutablePath`,
but nothing is downloaded and the repository helper remains preferred.

If `PROCESS_DUP_HANDLE` access is denied, the script records that an elevated
PowerShell is required and stops without bypassing access controls. Run the
same command from an elevated PowerShell only when this stable access-denied
result is present; the engine and normal server do not require elevation.

## Results and logs

Each run writes outside the repository under:

```text
%TEMP%\HL-Engine\stock-two-client\<timestamp>
```

The bounded directory contains:

- `stock_two_client_autotest.json` — machine-readable contract result;
- `stock_two_client_autotest.txt` — concise `key=value` result;
- redirected server/build/test logs used by the gates.

Packet bytes, Steam credentials, and proprietary game data are not written to
the result files. `-FollowServerLog` reports bounded semantic progress only;
the result JSON remains the source of truth.

## Dry run

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File .\scripts\run_stock_two_client_autotest.ps1 `
  -DryRun `
  -SkipBuild
```

DryRun launches nothing. It prints sanitized server/A/B/reconnect commands and
validates `-multirun`, ports, names, ownership selection, refusal to own
pre-existing PIDs, result serialization, exact-cleanup policy, helper
architecture, exact mutex policy, and the absence of injection/patching.

Inspect without closing:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File .\scripts\run_stock_two_client_autotest.ps1 `
  -MultiInstanceMode Auto `
  -InspectLauncherMutexOnly `
  -SkipBuild
```

This launches only A, completes the same ownership/network/spawn gates,
inspects the exact mutex, records one `Mutant` match, closes nothing, and
stops only the owned A/server processes.

## Long run

Run only after the basic two-client lifecycle passes:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File .\scripts\run_stock_two_client_autotest.ps1 `
  -MultiInstanceMode MutexUnlock `
  -InputMode VerifiedSendInput `
  -AutoTestDurationSeconds 600 `
  -SkipReconnectTest `
  -FollowServerLog
```

The long run requires fresh movement, origin, snapshot, frame-ACK, and peer
update progress for both clients at 10, 30, 60, 120, 300, and 600 seconds. It
also enforces 80 ms snapshot/remote-update p95, 160 ms maximum ordinary remote
gaps,
zero starvation windows, no burst or continuous remove/re-add behavior,
monotonic remote animtime, and no permanent `EF_NOINTERP`. A fatal process,
endpoint, handshake, input-delivery, or progress failure ends the run instead
of accepting elapsed time.

## Ten-minute manual smoothness observation

Run this mode after the shorter lifecycle/reconnect test and the automatic
600-second test:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File .\scripts\run_stock_two_client_autotest.ps1 `
  -MultiInstanceMode MutexUnlock `
  -InputMode None `
  -ManualObservation `
  -ManualObservationSeconds 600 `
  -PromptForVisualConfirmation `
  -FollowServerLog
```

The script prints the loopback endpoint and the exact owned A/B PIDs, arranges
their windows side by side when possible, prints bounded semantic counters
every five seconds, performs no automatic input or disconnect, and keeps the
session active for the requested interval. In each client, verify local
W/A/S/D, jump, duck and mouse.
Observe each player from the other client while moving and confirm smooth
remote motion without low-rate stepping. Continue for at least ten minutes
and confirm that both clients remain keyboard-movable. Answer each final Y/N
question from direct observation; the harness does not infer visual success
from protocol counters.

`InputMode None` deliberately sends no scripted movement and leaves every
movement button released. Automatic failures distinguish missing/foreign
windows, foreground activation or PID mismatch, `SendInput`/key-up/sticky-key
failures, missing server-observed input, and later network/runtime movement
stages. Rerun from an interactive PowerShell; use elevation only when the
result explicitly identifies an access-denied mutex operation. Never count
client uptime as a substitute for verified movement.

## Historical Prompt 247 stock-build behavior and results

The results in this section established multi-instance launch and replication,
but Prompt 248 invalidated their movement-continuity acceptance logic. They
used cumulative movement and uptime checkpoints, did not prove fresh live
progress at every checkpoint, and had no manual interpolation gate. Do not use
them as current smoothness or 600-second movement acceptance.

On the tested Windows installation, Client A starts, owns UDP `27005`, reaches
the server, and spawns. The initial Auto-mode B process presents the stock
single-instance dialog, exits after a bounded lifetime, never owns UDP
`27006`, and never reaches the server. The primary acceptance recorded a
1450 ms transient lifetime, no UDP, no handshake, and exact transient cleanup.

The same result was reproduced with:

- one shared stock executable path;
- two explicit Steam library executable paths;
- `-steam -multirun` ordering;
- `-multirun -steam` ordering;
- `-multirun` without `-steam`.

After exact owned mutex close, retry B becomes a persistent independent
`hl.exe`, owns UDP `27006`, connects as slot/edict 2, spawns, moves, and
replicates mutually with A. The 120-second Auto lifecycle/reconnect run passed,
including A Remove, exact B mutex close, fresh A reconnect/Re-Add and clean
slot reuse. The historical 600-second MutexUnlock run was reported as passing
at 60, 120, 300 and 600 seconds. Under the corrected Prompt 248 criteria that
result is a confirmed false positive and is retained only as launch/lifecycle
evidence.

Primary result:

```text
%TEMP%\HL-Engine\stock-two-client\20260802-114835-603-5cd8c8eb\stock_two_client_autotest.json
```

Long-run result:

```text
%TEMP%\HL-Engine\stock-two-client\20260802-115155-188-4576dce0\stock_two_client_autotest.json
```

Do not patch the client, automate Steam credentials, terminate Steam, or
close arbitrary process handles.

## Prompt 248B anchor-isolated checkpoints

Automatic mode captures a generation-bound safe spawn anchor for each client
and uses an external loopback-only control file to return the tested player
before and after every bounded pulse. It waits for the exact reset request
acknowledgment and at least two snapshots before sending input. Reset
displacement never counts as movement, and the deliberate teleport sample is
excluded from interpolation statistics.

Pipeline health and travel are separate. A pulse into a wall may report
`movement_pipeline=pass` and `travel_result=blocked_by_geometry`; the harness
then resets and tries the next direction. Only missing clc_move/PM_Move
progress, or failure of every direction from a verified anchor, is a movement
failure. The local radius is 96 units and the current 150-ms pulse provides a
safe margin on the tested stock client.

Before any initial A launch, the harness enumerates `hl.exe`. An existing
unowned client is never closed or modified and produces
`preexisting_stock_client_detected`. Close that client yourself before
starting the required manual observation. Manual mode sends no input and does
not reset either player unless an explicit reset command is requested.

Troubleshooting is intentionally classified by layer:

- an error window or early exit is `client_b_multirun_rejected` or an owned
  process-creation failure;
- a valid second process without UDP `27006` is
  `client_b_process_created_but_no_network`;
- a UDP endpoint without a server connect marker is
  `client_b_connect_timeout`;
- a connect without materialization is `client_b_spawn_timeout`.
- zero exact mutex matches are `launcher_mutex_inspect_mutex_not_found`;
- multiple matches are refused as `ambiguous_mutex_matches`;
- access denied is reported as requiring elevation, never bypassed.

## Manual launcher

The interactive launcher remains available:

```powershell
& '.\scripts\run_manual_two_client_test.ps1' `
  -ServerExecutablePath '.\out\build\vs2022-win32-reference-sdk\Release\hlhost.exe' `
  -ServerGameDir 'D:\Steam\steamapps\common\Half-Life\valve' `
  -ClientAExecutablePath 'D:\Steam\steamapps\common\Half-Life\hl.exe' `
  -ClientBExecutablePath 'D:\Steam\steamapps\common\Half-Life\hl.exe' `
  -ClientGameDir 'D:\Steam\steamapps\common\Half-Life\valve' `
  -Configuration Release `
  -SkipBuild
```

Use `-ServerOnly` or `-ManualClientB` when an independently managed stock
client session is available. Manual observation remains supplementary to the
automatic machine-readable gate.

## Scope boundary

Two-client movement and mutual player visibility do not mean that
player-to-player collision, weapons, firing, damage, death, respawn,
scoreboard, lag compensation, or complete Half-Life Deathmatch gameplay are
implemented.
