# Two stock-client Glock damage acceptance test

prompt_id=HL-ENGINE-20260809-250A-GOLDSRC-GLOCK-STOCK-ACCEPTANCE

This procedure validates the opt-in `--goldsrc-combat` compatibility slice
with two unmodified Half-Life 1.1.2.2 Steam build 15961492 clients. It is a
loopback-only manual interoperability check on `crossfire`; it does not claim
full multiplayer gameplay parity.

## Preconditions

- Build the Win32 `Release` configuration and pass the mandatory zero-button
  autoaim phase, Glock Proof A, Glock Proof B, the dedicated lethal death
  proof, and the 3/3 stock fall proof.
- Close unrelated Half-Life windows so the launcher owns both test clients.
- Do not hold synthetic movement keys. All observation and firing input in this
  phase must be manual.
- Keep both clients connected for at least 120 seconds.

## Exact-baseline comparison

Before the combat run, launch the separately prepared exact-baseline binary
without `-GoldSrcCombat`:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File '.\scripts\run_stock_two_client_autotest.ps1' `
  -AcceptancePhase ManualObservation `
  -MultiInstanceMode Auto `
  -InputMode None `
  -ManualObservationSeconds 120 `
  -PromptForVisualConfirmation `
  -Map crossfire `
  -ServerExecutablePath '<isolated-baseline-worktree>\out\build\vs2022-win32-reference-sdk\Release\hlhost.exe' `
  -ServerGameDir '<stock-half-life-dir>\valve' `
  -ClientExecutablePath '<stock-half-life-dir>\hl.exe' `
  -SkipBuild
```

With both unmodified clients spawned, record whether the stock inventory and
Glock are visible, the active weapon and ammunition values, one attempted
primary attack, and the target's health before and after. This observation is
paired with the source-audited baseline callback/input record; do not infer a
callback count from the HUD. Required baseline observations are:

```text
baseline_stock_two_clients_tested=yes
baseline_stock_inventory_after_spawn=game_dll_inventory_present
baseline_stock_glock_available=yes
baseline_stock_glock_active=yes
baseline_primary_input_attempted=yes
baseline_combat_option=off
baseline_attack_input_observed=yes
baseline_attack_masked=yes
baseline_shot_executed=no
baseline_ammo_changed=no
baseline_target_health_changed=no
baseline_shooter_ammo_changed=no
baseline_clients_remained_connected=yes
```

## Launch command

Run from the repository root:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File '.\scripts\run_stock_two_client_autotest.ps1' `
  -AcceptancePhase ManualObservation `
  -MultiInstanceMode Auto `
  -InputMode None `
  -ManualObservationSeconds 120 `
  -PromptForVisualConfirmation `
  -GoldSrcCombat `
  -Map crossfire `
  -ServerExecutablePath '.\out\build\vs2022-win32-reference-sdk\Release\hlhost.exe' `
  -ServerGameDir '<stock-half-life-dir>\valve' `
  -ClientExecutablePath '<stock-half-life-dir>\hl.exe' `
  -SkipBuild
```

The launcher starts two separate stock-client processes with independent local
ports and a loopback server. Its mutex helper is limited to the launcher mutex;
it does not modify either client binary or game data.

Manual observation disables every scripted player-control path.
`InputMode None` is the default input mode, and
`AcceptancePhase ManualObservation` is the default phase. The launcher does
not activate, focus, resize, or move either client window after connection,
and joystick input is disabled for the launched test clients.
Select the client window yourself before controlling it. `VerifiedSendInput`
is reserved for explicitly opted-in automatic runs and cannot be enabled in
`ManualObservation`.

`ManualObservationSeconds` is a minimum observation interval, not an automatic
session deadline. After the interval, the launcher leaves the server and both
owned clients open and waits indefinitely at
`manual_session_state=awaiting_operator_completion`. Complete every movement,
fall, and firing check first; then return to PowerShell and press Enter. Only
after that confirmation does the launcher ask the visual questions and clean
up its owned processes. If setup or validation fails during a manual run, the
launcher also suppresses automatic cleanup so any still-running owned clients
remain available for inspection; close them manually before starting the next
run.

## Observation sequence

1. Confirm that both clients connect, spawn, remain connected, and can move
   smoothly without sticking or correction bursts. On the central `crossfire`
   ramp, explicitly test ascent, descent, toe and crest transitions, diagonal
   approach, stop/resume, reverse and strafe escape, the reported wall/window
   ledge escape, and jump/duck near the ramp. Repeat with the second client
   still independently movable.
2. With client B kept at full health for the later Glock target check, have
   client A jump from the reported `crossfire` balcony. Confirm that the
   landing completes, client A remains connected, local control resumes
   immediately, client B continues to observe A, and no connection warning or
   snapshot stall appears.
3. In client A, confirm the stock Glock is available and active. Record the
   visible ammunition count.
4. Without firing, slowly move client A's aim across and near client B. Confirm
   that both clients remain responsive and connected and that no connection
   warning or snapshot stall appears. This specifically revalidates the stock
   Glock idle-autoaim callback correction.
5. Aim at client B's torso and fire exactly one primary shot. Confirm client
   A's ammunition decreases by one, client B's health decreases, and client B
   remains alive.
6. Put solid world geometry between the clients and fire a clearly blocked
   primary shot. Confirm client B takes no damage.
7. Aim clearly away from client B and fire one deliberate miss. Confirm client
   B takes no damage.
8. Restore client B to full health if needed, then fire separated torso shots
   until stock damage kills client B. Confirm the death transition completes
   without a connection warning, frozen snapshots, or either process closing.
   Leave the clients running briefly and verify client A still moves and client
   B continues receiving frames. Use the normal stock respawn action in client
   B, then confirm a fresh live spawn, restored movement, visibility from both
   clients, and continued connectivity. Reverse the roles: client B must kill
   client A, client A must respawn, and both clients must still be alive,
   movable, mutually visible, and connected after this second cycle.
9. Separately record local shooter feedback (animation, muzzle flash,
   recoil/punch, sound and clip HUD decrement) and remote observer feedback
   (remote muzzle flash, sound and animation). For the remote muzzle flash,
   fire once, verify that the glow appears briefly and disappears immediately,
   wait at least ten seconds with no attack held, then fire once more and
   verify that the effect is re-armed for only that shot. Any glow that remains
   on the player model is a failure. Remote feedback remains a separate pending
   compatibility boundary if any part is missing.
10. Move both clients again, including one more ramp/step traversal, and observe
   each client's view of the other. Confirm local movement and remote
   replication remain stable through the full 120-second interval.
11. Answer every launcher confirmation from direct visual observation. Stop the
   test if any condition is uncertain.

## Required result record

Completion requires this exact manual evidence set:

```text
stock_two_clients_tested=yes
stock_glock_available=yes
stock_glock_primary_fired=yes
stock_shooter_ammo_decreased=yes
stock_target_health_decreased=yes
stock_target_remained_alive=yes
stock_wall_blocked_damage=yes
stock_miss_caused_no_damage=yes
stock_client_a_movement_stable=yes
stock_client_b_movement_stable=yes
stock_remote_replication_stable=yes
stock_player_sticking_reproduced=no
stock_clients_remained_connected=yes
stock_aim_near_player_tested=yes
stock_aim_near_player_connection_stable=yes
stock_aim_near_player_clients_responsive=yes

stock_lethal_sequence_tested=yes
stock_target_death_observed=yes
stock_death_connection_stable=yes
stock_post_death_snapshots_advanced=yes
stock_post_death_shooter_movement_stable=yes
stock_respawn_attempted=yes
stock_respawn_completed=yes
stock_respawn_connection_stable=yes
stock_respawn_movement_stable=yes
stock_respawn_remote_visibility_stable=yes

stock_remote_muzzleflash_first_shot_visible=yes
stock_remote_muzzleflash_cleared_after_shot=yes
stock_remote_muzzleflash_rearmed_on_next_shot=yes
stock_remote_muzzleflash_persistent_glow_reproduced=no

stock_balcony_fall_tested=yes
stock_balcony_landing_completed=yes
stock_balcony_connection_stable=yes
stock_balcony_post_landing_movement_stable=yes
stock_connection_problem_reproduced=no

current_combat_build_ramp_tested=yes
central_ramp_up=pass
central_ramp_down=pass
ramp_transitions=pass
reverse_escape=pass
strafe_escape=pass
ledge_escape=pass
jump_duck_near_ramp=pass
two_client_ramp_isolation=pass
player_sticking_reproduced=no

local_fire_feedback=<verified/missing>
remote_fire_feedback=<verified/missing>
next_observed_boundary=<none/stock_one_frame_muzzleflash_manual_acceptance_required/remote_fire_event_replication_required>
```

Until every required observation in this record is explicitly confirmed,
Prompt 250 remains incomplete and manual acceptance must not be marked passed.

## Current recorded result

The automated prerequisites currently recorded for this procedure are:

- Win32 Release build: pass.
- CTest: 15/15 passed.
- One-frame muzzle-flash cleanup regression: pass. The bit remains set through
  two modeled receiver reads, clears after the complete send sweep, is re-armed
  by a later shot, and leaves unrelated persistent effects intact.
- Mandatory zero-button aim phase: pass. The direct ray missed while the
  autoaim cone acquired the target, both `pfnVecToAngles` and
  `pfnCrosshairAngle` were invoked, health and ammunition were unchanged, and
  both clients remained responsive.
- Glock Proof A: pass. The mandatory aim phase passed, one authoritative
  attack executed once, `StartFrame` calls were 2125, and pre/post-think counts
  were A=107/107 and B=105/105. Clip changed 17 -> 16, reserve ammunition
  stayed 68 -> 68, target health changed 100 -> 88, post-shot movement passed
  for both clients, and shutdown was clean.
- Glock Proof B: pass for all 13 negative/isolation gates.
- Repeated lethal death proof: pass. After the nonlethal `100 -> 88` control,
  eight one-shot lethal death/respawn cycles completed. Totals including the
  control were nine attacks received/executed, nine rounds consumed, nine
  player-hit shots, 18 traces (A=16, B=2), and zero world hits. The same victim
  died seven times. Respawn inputs were A=1/B=7; all eight clicks reached the
  lifecycle path and none was counted as a weapon attack. Both players were
  alive after respawn eight and callback failures stayed zero.
- Body-queue telemetry directly observed eight corpse-copy calls across four
  distinct nodes, two completed queue rotations, and a valid sequence. The
  server remained responsive through clean shutdown. This is automated
  lifecycle evidence and does not replace visual corpse/death-camera checks.
- Stock fall proof: pass, 3/3. The 400-unit landing was grounded and caused no
  damage; the 600-unit landing was grounded, caused exactly 10 damage, and
  left the player alive; gameplay callback failures remained zero and both
  clients passed authoritative and decoded-wire XY movement after landing.
- Combat feature-off proof: pass.
- Safe-output negative checks: direct and wrapper runs pass with fixed semantic
  failure classifications only. Proof frame/reference history uses a bounded
  512-frame window, the indexed capture is bounded, and exact final summary
  validation passes without printing raw process output or private paths.
- The stock `UpdateClientData` pre-callback byte-zero contract is restored;
  its poisoned-buffer regression passes, snapshot range validation remains
  strict, and fresh combat Proof A/B plus feature-off runs pass.
- Noncombat two-client Probe A/B: pass/pass.
- Fresh frozen-tree PM_Move wrapper Proof A/B: pass/pass, including positive
  movement/landing/collision behavior and negative rollback, reconnect, and
  slot-reuse cases; both runs shut down cleanly.
- Player lifecycle and continuous snapshot Proof A/B: pass/pass.
- All earlier external handshake, netchan, serverinfo, resource,
  fragmentation, signon, delta, post-resource, world, first-snapshot,
  continuous, lifecycle, PM_Move, and two-client A/B groups: pass/pass.
- Default-off query gate: pass with `normal_host_behavior_changed=0`.
- Mutex unlocker, launcher DryRun, and launcher smoke checks: pass.
- Persistent PM_Move long-run: pass with 610.098 seconds observed, 12,204
  server snapshots sent and 7,789 received, 7,787 movement commands sent and
  7,783 executed, 7 temporary clock rejections and 2 recoveries. Cadence
  passed with a 96 ms maximum interval, 0 bursts, 2 missed intervals, 0
  movement discontinuities, every 60/120/300/600-second checkpoint, and clean
  shutdown.
- The detached exact-baseline worktree is clean at `dba657e`, its canonical
  Win32 Release executable was rebuilt successfully, and its headless
  two-client Probe A passed connect, spawn, simultaneous movement,
  replication, reconnect and clean shutdown. Its real stock-client attack
  observation is still pending.

The exact-baseline observation, visible current-build two-stock-client run,
all thirteen launcher-backed stock-client fields, detailed ramp/step checks,
balcony-fall and post-landing connectivity checks, post-fix one-frame
muzzle-flash lifetime, and complete local/remote fire feedback remain pending.
Do not copy automated values into the manual result record. Prompt 250 remains
incomplete until those direct observations pass; no completion claim is made
by the automated results above.

The latest pre-fix manual attempt established one additional fact: aiming near
the other player caused a connection warning because the server entered its
`PlayerPostThink` gameplay fail-stop. It was not launcher auto-close. Stock
Glock idle autoaim required missing `pfnVecToAngles` and then
`pfnCrosshairAngle` callbacks. The host now supplies stock-compatible
vector-to-angle semantics and a bounded crosshair-angle callback, and the
mandatory headless aim phase passes. The two-client aim-near-player sequence
above must still be repeated manually.

The following pre-fix run confirmed wall decals and the local view-model firing
animation, then exposed a controlled snapshot fail-stop when repeated hits
changed the target from health `4` to `-8`. The dead target's stock Glock was
correctly absent, but the snapshot path still required the prior alive weapon
set. The death-aware fix now uses a full fallback for the two-phase
Glock-present to Glock-absent transition and treats a stock `AddToFullPack`
omission separately from a callback fault. The lethal automated proof and fresh
normal Proof A/B pass. The operator has since confirmed that stock death works
and the player respawns normally. That same run exposed a persistent remote
model muzzle glow after the first shot; the host now performs the stock
post-send `EF_MUZZLEFLASH` cleanup. Steps 9-11 remain the required post-fix
manual check.

This fix does not claim visual `svc_crosshairangle` delivery. That client-side
crosshair deflection is a separate pending boundary and must not be confused
with local or remote fire animation, sound, recoil, or muzzle-flash feedback.

```text
manual_baseline_observation=pending
manual_current_combat_observation=pending
manual_ramp_observation=pending
manual_balcony_fall_observation=pending
manual_aim_near_player_observation=pending
manual_post_fix_death_observation=pass
manual_post_fix_respawn_observation=pass
manual_post_fix_muzzleflash_observation=pending
local_fire_feedback=missing
remote_fire_feedback=missing
```
