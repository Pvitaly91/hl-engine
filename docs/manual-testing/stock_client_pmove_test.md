# Manual stock-client PM_Move test

## Purpose and prerequisites

`scripts/run_manual_pmove_test.ps1` prepares the current Win32 `hlhost`,
starts it on an automatically selected `127.0.0.1` UDP port, waits for the
real readiness marker and verifies endpoint ownership, then launches an
unmodified local Half-Life client for manual movement testing.

Prerequisites:

- Windows PowerShell 5.1;
- Visual Studio 2022 with Win32 C++ support and CMake, or `cmake.exe` in PATH;
- the configured read-only Half-Life SDK available to this build;
- a legal local Half-Life installation with `hl.exe`, the `valve` Game DLL,
  `delta.lst`, and the selected map;
- Steam already authenticated when the local client requires it.

The launcher never automates Steam credentials, edits the client or GameDir,
or opens a non-loopback server. Its default manual mode is persistent: after
the stock client establishes the session, network pumping, snapshots,
`clc_move`, and authoritative PM_Move continue until the launcher requests a
clean shutdown. The five-minute deadline still bounds an initial abandoned
connection, but it does not end an established persistent session.

## Common usage

Default build and launch:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File .\scripts\run_manual_pmove_test.ps1
```

Explicit client, server GameDir, and map:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File .\scripts\run_manual_pmove_test.ps1 `
  -ClientExecutablePath '<path-to-hl.exe>' `
  -ServerGameDir '<path-to-valve>' `
  -Map crossfire
```

Resolve everything without building or launching a process:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File .\scripts\run_manual_pmove_test.ps1 `
  -DryRun `
  -SkipBuild
```

Run the old proof-style 60-second bounded observation:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File .\scripts\run_manual_pmove_test.ps1 `
  -BoundedObservation
```

Run a noninteractive server-only smoke test:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File .\scripts\run_manual_pmove_test.ps1 `
  -ServerOnly `
  -DurationSeconds 10
```

Run the canonical 12-test CTest suite before launch:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File .\scripts\run_manual_pmove_test.ps1 `
  -RunTests
```

Run the bounded automated stock-client scenario after the automated proofs:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File .\scripts\run_goldsrc_pmove_stock_longrun_verification.ps1 `
  -ExecutablePath .\out\build\vs2022-win32-reference-sdk\Release\hlhost.exe `
  -GameDir '<path-to-valve>' `
  -ClientPath '<path-to-hl.exe>' `
  -MovementDurationSeconds 610
```

Add `-DirectClientLaunch` when Steam's `-applaunch` handoff does not retain a
stable process/window handle. The same installed `hl.exe` is then started with
`-steam`; Steam must still be running and authenticated.

Window discovery is restricted to the newly launched process whose command
line contains the unique localhost endpoint. Foreground transfer temporarily
attaches only the verifier, current foreground, and owned client GUI threads;
cleanup never closes a pre-existing Half-Life process.

This verifier uses only `127.0.0.1`, validates the hashes of its read-only
client/GameDir inputs before and after the run, and writes screenshots and
logs only to a temporary directory outside the repository. It applies
`fps_override 0`, `fps_max 100`, and `cl_cmdrate 100` only to the running
process so the required approximately 100-command-per-second scenario is not
affected by a user's persistent high-FPS configuration. No config file is
written. `-MovementProfile` can select `varied`, `idle`, `forward`, `strafe`,
`jump`, or `duck`; `-SkipReconnect` is intended only for short smoke checks.

Use `-SkipBuild` to run an existing `hlhost.exe`, `-Configuration Release`
to select the Release output, `-Port <number>` for a fixed free local port,
`-DurationSeconds <seconds>` for a timed session, and `-Map <name>` for another
map that exists in the selected GameDir. `-ServerOnly` never launches the GUI
client. `-Fullscreen` omits the windowed width/height arguments.

`-FollowServerLog` shows only incremental bounded output. `-KeepServerRunning`
leaves both the owned server process and its active persistent network runtime
running after a successful launcher exit. `-DurationSeconds N` requests a
launcher-controlled clean shutdown after N seconds; without it, press Enter or
Ctrl+C to end the persistent session. `-BoundedObservation` restores the old
60-second compatibility milestone behavior for proof comparison.
`-StopClientOnExit` closes a client only when the launcher can positively
identify the new direct `hl.exe` process; Steam and pre-existing clients are
never stopped.

Path overrides are `-ServerExecutablePath`, `-ServerGameDir`,
`-ClientExecutablePath`, `-ClientGameDir`, and `-LogDirectory`. The client
GameDir must be the `valve` directory beside the selected stock `hl.exe`;
server assets may come from a separate valid `valve` directory. Advanced
controls include `-ReadinessTimeoutSeconds`, `-WindowWidth`, `-WindowHeight`,
`-AdditionalServerArguments`, and `-AdditionalClientArguments`. Additional
arguments cannot replace launcher-owned networking, lifecycle, logging,
PM_Move, persistence, timeout, shutdown, connection, or window arguments.

## Discovery and build behavior

The repository root is derived from the script location. The launcher prefers
`cmake.exe` from PATH and then standard Visual Studio locations. This source
tree has no source-local `CMakePresets.json`, so the documented canonical
build tree is configured as Visual Studio 2022 Win32 under
`out/build/vs2022-win32-reference-sdk` when its cache is absent or invalid.
Only the `hlhost` target is built by default.

`--frames 1` remains intentional. It controls the bounded host/Game DLL
bootstrap frame count before the host enters its separately owned GoldSrc
network loop; it does not limit persistent UDP pumping, snapshot cadence, or
PM_Move processing.

Client discovery checks explicit parameters first, then configured Steam
install and library folders from the Windows registry and
`libraryfolders.vdf`. It does not recursively scan drives. Server GameDir
discovery checks `-ServerGameDir`, `HLENGINE_VALVE_DIR`, the explicit client
GameDir, and discovered Half-Life installations. Missing data is not
downloaded or copied.

## Logs and cleanup

Default session artifacts are created outside the repository under the user
temporary directory in `HL-Engine\manual-pmove\<session>`. Each run writes:

- `server.stdout.log`;
- `server.stderr.log`;
- `session.json`;
- `session-summary.txt`;
- optional `build.log` and `ctest.log`.

Persistent mode also uses a transient `shutdown.request` sentinel in the same
external session directory. The launcher removes it after the server confirms
shutdown, so it is not a retained session artifact.

An explicit `-LogDirectory` must also be outside the repository. Metadata is
bounded to session identity, selected endpoint, executable/version fields,
PIDs when positively known, timing, exit codes, and cleanup results. It does
not contain packets, credentials, environment dumps, or proprietary content.

Normal exit and failure cleanup target only the exact server `Process` object
started by the launcher. Persistent cleanup first requests host-owned runtime
and socket shutdown, waits for the process, and uses bounded force cleanup only
as a fallback. The client is left running by default. Press Enter to end an
interactive session; Ctrl+C normally enters the same `finally` cleanup path
supported by Windows PowerShell.

Inspect the latest retained session without hard-coding a machine path:

```powershell
$root = Join-Path ([IO.Path]::GetTempPath()) 'HL-Engine\manual-pmove'
$latest = Get-ChildItem -LiteralPath $root -Directory |
  Sort-Object LastWriteTimeUtc -Descending |
  Select-Object -First 1
Get-Content -LiteralPath (Join-Path $latest.FullName 'session-summary.txt')
Select-String -LiteralPath (Join-Path $latest.FullName 'server.stdout.log') `
  -Pattern 'goldsrc_manual_session_|goldsrc_movement_milestone_'
```

## Manual movement checklist

1. Wait for the map and player to load.
2. Stand still for 10 seconds. The player should remain grounded without
   falling through the map.
3. Press W and S. Forward and backward movement should be authoritative.
4. Press A and D. Strafe movement should be authoritative.
5. Release all movement keys. Half-Life friction should stop the player.
6. Press Space. Verify jump, gravity, landing, and restored grounding.
7. Hold and release Ctrl. Verify duck/unduck view and hull changes.
8. Walk into a wall. Static BSP collision should block penetration.
9. Under a low ceiling, a blocked unduck should keep the player crouched.
10. Move before and after 60 seconds, then remain connected past 120 seconds.
    There should be no repeated snap-back, stalled snapshots, or timeout.
11. Continue through 300 and 600 seconds. At both boundaries, verify that
    movement responds immediately and does not arrive in periodic bursts.
12. After 600 seconds, repeat jump, duck, stop/start, and static wall
    collision before performing the reconnect checklist.

Reconnect checklist: open the client console, run `disconnect`, then
`connect 127.0.0.1:<printed-port>` while the server is still active. Verify a
fresh player state with no reused position or velocity.

## Known limitations

This checkpoint does not implement weapons or firing, damage, item pickup,
moving platforms, ladders, complete water movement, player-to-player
collision, or multiplayer combat. Persistent manual mode keeps the implemented
post-bootstrap network and PM_Move slice active; it does not imply those
unimplemented gameplay systems are complete.

## Troubleshooting

- If the client is not found, pass
  `-ClientExecutablePath '<path-to-hl.exe>'`.
- If the map, Game DLL, or delta descriptions are missing, pass
  `-ServerGameDir '<path-to-valve>'` and verify `maps\<map>.bsp` exists.
- If a fixed port is occupied, choose another one or omit `-Port` for bounded
  automatic selection. The launcher never terminates the current owner.
- If readiness fails, inspect the last bounded terminal excerpt and the
  external `server.stdout.log` / `server.stderr.log` files.
- If a real client timeout occurs, confirm `goldsrc_manual_session_started`,
  `goldsrc_movement_milestone_reached`, and later
  `goldsrc_manual_session_still_active` markers are present. A milestone
  without a later active marker indicates that the persistent runtime did not
  remain established.
- If an old Debug binary is selected with `-SkipBuild`, build once without
  `-SkipBuild` or use the current Release configuration explicitly.
- If Steam redirects the direct launch, the launcher may be unable to prove
  client PID ownership. It still prints the console command and never kills
  an unowned client or Steam.
