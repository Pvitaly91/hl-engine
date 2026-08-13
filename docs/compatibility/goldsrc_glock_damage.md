# Stock GoldSrc Glock damage compatibility slice

prompt_id=HL-ENGINE-20260809-250A-GOLDSRC-GLOCK-STOCK-ACCEPTANCE

This document records the boundary and verified contracts for the opt-in
`--goldsrc-combat` compatibility slice. The feature is disabled by default and
does not change the movement-only `--goldsrc-pmove` path.

## Baseline inventory before implementation

The inventory below is a source-audited record of baseline
`dba657e6bd5cba280f50f217e48319e021d7fc5f`. The exact-baseline executable is
prepared and smoke-tested, but the required real stock-client observation is
recorded separately and remains pending.

| Surface | Baseline behavior |
| --- | --- |
| Attack input | `IN_ATTACK` was removed by the movement-only input mask. |
| Command callback order | `CmdStart -> PM_Move -> validate/commit -> CmdEnd`. |
| `StartFrame` | Present in the loaded Game DLL table, but not called by the UDP compatibility runtime. |
| `PlayerPreThink` | Present, but not called for authoritative `clc_move` execution. |
| `PlayerPostThink` | Present, but not called; consequently stock `ItemPostFrame` did not process attacks. |
| Player `Think` | Not scheduled in the per-command compatibility path. |
| Weapon data codec | Full snapshots could encode `weapon_data_t` delta states, but the runtime supplied none and delta snapshots rejected non-empty weapon data. |
| `UpdateClientData` | Called with `sendweapons=0`; weapon/ammo extensions were then cleared. |
| `GetWeaponData` | Present in the Game DLL table, but not called by the snapshot runtime. |
| Combat trace | Engine `TraceLine` returned an unobstructed world trace and did not consider connected player edicts. |
| Damage visibility | Clientdata already carried health, but no authoritative Glock path could produce damage. |
| Sound/event behavior | Sound callbacks were bounded and observed; the general engine callback table did not supply `PlaybackEvent`. PM_Move had an isolated no-op playback service callback. |
| Missing Glock callbacks | Gameplay pre/post-think sequencing, per-frame `StartFrame`, player-aware world-occluded `TraceLine`, `PlaybackEvent`, weapondata capture, and the engine callbacks used when stock Glock idle processing evaluates autoaim. |

The first missing combat boundary is
`validated_attack_input_and_player_postthink_weapon_processing_required`.
The baseline Game DLL can create independent stock player inventory through its
normal spawn and `GiveNamedItem` path; no engine-owned or private-data-injected
weapon is required.

## Verified stock callback and time contract

The pinned Half-Life SDK and the public ReHLDS `SV_RunCmd`/`SV_Physics`
implementations establish the contract used by this slice:

1. A validated command enters `CmdStart`.
2. The engine advances that client's independent command time by the accepted
   command's `msec`, then binds buttons, impulse, view angles, command time,
   and frame time to the player/global state.
3. `PlayerPreThink` runs before movement.
4. A due player entity `Think` runs before movement.
5. `PM_Move` executes, its output is validated, and authoritative movement is
   committed and relinked.
6. `PlayerPostThink` runs after movement and drives stock `ItemPostFrame`.
7. `CmdEnd` closes the command scope.

Commands above GoldSrc's 50 ms bound are split. Each accepted split command
uses the same callback unit. A held attack may therefore be evaluated in more
than one split, while the Game DLL's authoritative weapon cooldown prevents an
extra shot. Packet replay protection remains in the command planner, so a
duplicate or already-consumed backup command never reaches gameplay callbacks.

`StartFrame` runs once per authoritative host frame, independent of connected
client count. The compatibility runtime supplies monotonic server time and a
bounded host-frame `frametime`; each client has an independent command
timebase, so one client's commands cannot advance another client's clock or
advance the host clock a second time.

The stock Glock primary path is:

`PlayerPostThink -> ItemPostFrame -> PrimaryAttack -> GlockFire ->
FireBulletsPlayer -> TraceLine -> TraceAttack -> ApplyMultiDamage`.

Damage and cooldown values remain owned by the selected Game DLL and its normal
multiplayer rules/configuration. The engine compatibility layer must not
subtract health, manufacture ammo, alter spread, or inspect Game DLL private
data.

## Scope and safety constraints

Combat stays loopback-only for this milestone. Only `IN_ATTACK` is admitted
when a spawned client has valid Game DLL weapon state. `IN_ATTACK2`, reload,
use, arbitrary impulses, and unsupported weapon selection remain masked.

Movement collision remains BSP-only. Combat line traces separately consider
the static BSP and connected, spawned, damageable player bounds, ignore the
shooter, and select the nearest valid hit. World geometry wins when it is
closer. Hitgroup remains generic. This slice now carries the stock lethal
health/deadflag transition and death-aware snapshot continuity, and the
operator directly confirmed that the player respawns normally. It does not
claim complete corpse, death-camera, scoreboard, or death-message parity.
Studio hitboxes, headshots, moving brushes, monsters, and lag compensation
remain outside the slice.

## Implementation and acceptance record

### Identity and opt-in boundary

- Task branch: `codex/goldsrc-glock-damage-slice`.
- Baseline: `dba657e6bd5cba280f50f217e48319e021d7fc5f`.
- Pinned SDK: `b1b5cf5892918535619b2937bb927e46cb097ba1`.
- Stock client: Half-Life 1.1.2.2, Steam build 15961492.
- Game DLL API version: 140.
- Feature option: `--goldsrc-combat`; default: off.

The option closes the already ordered loopback compatibility dependencies:
PM_Move, player lifecycle, continuous and first snapshots, world baselines,
resource and delta descriptions, serverinfo, netchan, and UDP handshake. A
non-loopback bind, a missing dependency, or a missing required Game DLL
callback fails closed. The default-off and movement-only paths do not enable
combat input or weapondata replication.

### Authoritative inventory and weapon execution

`ClientPutInServer` and the stock Game DLL materialize each player's private
player object and inventory. The engine neither fabricates a Glock nor writes
Game DLL private fields. Combat readiness requires both an active weapon ID 2
and a valid Glock record returned by the Game DLL. Each connected client owns
an independent inventory, command clock, lifecycle generation, snapshot
history, and weapondata history.

Only a validated, fresh `IN_ATTACK` command may enter the combat callback
sequence. `IN_ATTACK2`, reload, use, impulses, unsupported weapon selection,
invalid checksums, malformed commands, stale generations, out-of-order
commands, exact duplicates, and backup replays do not produce a second shot.
The Game DLL owns Glock cooldown and ammunition consumption.

### Snapshot and wire contract

For each authoritative client snapshot the runtime calls
`UpdateClientData(..., sendweapons=1)` and then `GetWeaponData` for that
client. Health is carried by the stock `svc_clientdata` delta description;
Glock clip state is carried by the associated `weapon_data_t` delta records.
Full and delta frames retain semantic per-client health and weapon state, and
deltas are based only on the exact acknowledged frame. If an acknowledged
base has left the bounded history, the server emits a full fallback instead
of selecting an ambiguous frame with the same low eight bits.

Weapon presence is death-aware. While the target is alive, the decoded frame
must contain its stock Glock record. Once stock damage makes health
nonpositive and sets `deadflag`, `GetWeaponData` legitimately stops returning
that Glock. The first death frame therefore uses a full snapshot to establish
the two-phase `Glock present -> Glock absent` transition, after which
acknowledged deltas inherit the absent weapon set without inventing a stale
clip value. Frame IDs, server time, movement/callback counters, and both
clients' snapshot counts must continue monotonically through the transition.

The external proof reconstructs health and Glock `m_iClip` from the actual
snapshot byte stream. Server summary markers are used only as independent
cross-checks; they are never used to manufacture decoded state.

### Trace, damage, and lifecycle contract

The combat trace first evaluates static BSP geometry and then current,
connected, spawned, damageable player bounds. It validates lifecycle
generation, ignores the shooter, selects the nearest impact, and allows the
world to occlude a player. It does not add player solids to PM_Move, so remote
players cannot create movement sticking. Hitgroup is deliberately generic.

The compatibility layer returns the selected `edict_t` to the Game DLL. The
stock `TraceAttack`/`ApplyMultiDamage` path owns damage; the host does not
subtract health. Disconnect retains the client edict for stock callback
semantics, retires obsolete private storage until safe teardown, creates a
fresh player object on reconnect, dispatches due non-client `Think` callbacks,
and prevents same-frame reuse of a removed non-client edict.

`AddToFullPack` has three explicit outcomes. A positive result includes the
entity, a stock zero result merely omits it from that receiver's packet, and
only an exception or invalid invocation is a callback fault. A deliberate
omission, including one that occurs around a death transition, no longer
triggers the global gameplay fail-stop.

If a structured movement validation fails before gameplay callbacks begin,
normal rollback remains available. A failure after combat callbacks begin is
fatal for the combat runtime because opaque Game DLL side effects can span the
weapon, shooter, and target and cannot be safely rolled back piecemeal. The
accepted packet is consumed once, later gameplay callbacks are rejected, and
the UDP runtime terminates fail-closed rather than replaying a possibly
committed shot.

Stock `UTIL_Remove` remains deferred. During a combat host frame, the runtime
performs a compile-time-bounded sweep of non-client edicts marked
`FL_KILLME` before dispatch, excludes dormant or removal-marked entities from
the due-think set, retires an entity marked by its own callback after that
callback unwinds, and performs one final bounded sweep while the frame's reuse
barrier is active. This prevents an unbounded cleanup pass, a callback into a
retired entity, and same-frame reuse of removed private state.

Game DLL combat callbacks execute behind the platform exception boundary. On
Windows the host also disables interactive process error dialogs at startup.
A callback fault is therefore converted to a fixed semantic failure rather
than exposing a GUI crash dialog, address, module, stack, or raw process
output. Once gameplay callbacks have begun, the command is consumed once, the
global gameplay fail-stop is latched, later combat callbacks are refused, and
the transport terminates fail-closed.

The external proof harness follows the same safe-output rule. Direct and
wrapper negative checks confirm that only fixed semantic failure classes are
reported; raw process output, exception details, and private installation
paths are not printed. Per-client frame/reference retention uses a 512-frame
window and rejects an expired exact base. The indexed snapshot capture is
also bounded, while the temporary text capture remains exact for final
summary validation without forcing a physical-media synchronization for every
captured line.

### Stock Glock autoaim callback boundary

The first manual current-build attempt after the launcher lifetime correction
showed a distinct gameplay failure: merely aiming near the other player caused
the stock client to report a connection problem. This was not launcher
cleanup. The server had entered its fixed `PlayerPostThink` gameplay
fail-stop.

The exact path is stock Glock `WeaponIdle -> GetAutoaimVector`. When another
player entered the autoaim cone, the Game DLL required `pfnVecToAngles` and
then `pfnCrosshairAngle`; both entries were absent from the populated engine
callback table. The host now provides stock-compatible vector-to-angle
conversion and a bounded crosshair-angle callback. The mandatory headless aim
phase exercises target acquisition with zero attack buttons and verifies that
both callbacks run while health and ammunition remain unchanged and both
clients remain responsive.

This correction does not yet claim stock `svc_crosshairangle` visual delivery.
The callback is accepted safely, but client-side crosshair-deflection delivery
is a separate compatibility boundary. It is also separate from local and
remote Glock fire animation, sound, and muzzle-flash feedback, which still
require direct manual observation.

### Stock lethal-death snapshot boundary

The next manual run confirmed wall decals and the local view-model firing
animation, then exposed a separate failure after repeated player hits. The
target's final authoritative transition was health `4 -> -8`. Stock
`GetWeaponData` correctly stopped reporting the dead target's Glock, but the
snapshot path still required the prior alive-frame weapon set. That mismatch
caused a controlled snapshot fail-stop and the clients then displayed a
connection warning; it was not launcher cleanup and no raw crash detail is
needed to diagnose it.

The corrected snapshot path treats life and death as two explicit weapon-set
phases, emits a full fallback at the presence-to-absence boundary, and accepts
stock `AddToFullPack` omission separately from a callback fault. The dedicated
lethal proof first records the ordinary nonlethal control at health `100 ->
88`, then executes eight one-shot lethal death/respawn cycles. Across the
control and lethal phases, exactly nine attacks execute, nine rounds are
consumed, nine player-hit shots are recorded, and 18 player-trace callbacks
run: 16 for client A and two for client B, with zero world hits.

All eight death/respawn cycles complete. The same victim dies seven times,
both players are alive after the eighth respawn, eight respawn clicks reach
the lifecycle path (A=1, B=7), and none is counted as a weapon attack. New
body-queue telemetry directly observes eight corpse-copy calls across four
distinct queue nodes, two completed queue rotations, and a valid sequence.
Both callback-failure counters remain zero and the server remains responsive
through clean shutdown. This automated body-queue evidence does not claim
complete corpse or death-camera visual parity. The operator's earlier direct
observation confirms that stock death and respawn work, but the remaining
manual combat record is still pending.

### One-frame muzzle-flash boundary

The same post-fix stock-client run exposed a visual lifetime defect: after the
first shot, the remote player model retained its muzzle glow. Glock correctly
sets `EF_MUZZLEFLASH`, but the host had not implemented the stock end-of-frame
cleanup, so the one-frame bit remained in later entity snapshots.

The runtime now preserves `EF_MUZZLEFLASH` through the complete per-client
snapshot-send sweep and clears it once afterward on every non-world edict.
Only that transient bit is cleared; decals, playback events, view-model firing
animation, damage, death, respawn, and persistent entity effects are
unchanged. The focused cleanup regression models two receiver reads while the
shot-frame bit is still set, then verifies next-frame clearing, repeated-shot
re-arming, preservation of persistent effect bits and the existing
receiver-local `EF_NOINTERP` state, and exclusion of the world edict. A fresh Release
build, CTest 15/15, normal two-client Glock Proof A, and all 13 Proof B gates
pass. Direct visual confirmation that the remote glow now disappears after
each shot is pending.

### Automated proof commands

All commands run from the repository root with the canonical Win32 Release
binary. `<stock-valve-dir>` denotes an installed, unmodified Half-Life
`valve` directory and is never staged.

```powershell
$exe = '.\out\build\vs2022-win32-reference-sdk\Release\hlhost.exe'
$game = '<stock-valve-dir>'

powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File '.\scripts\run_goldsrc_glock_damage_proof.ps1' `
  -ExecutablePath $exe -GameDir $game -SkipServerOutput

powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File '.\scripts\run_goldsrc_glock_damage_proof.ps1' `
  -ExecutablePath $exe -GameDir $game -NegativeProof -SkipServerOutput

powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File '.\scripts\run_goldsrc_glock_damage_proof.ps1' `
  -ExecutablePath $exe -GameDir $game -FeatureOffProof -SkipServerOutput

powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File '.\scripts\run_goldsrc_glock_damage_proof.ps1' `
  -ExecutablePath $exe -GameDir $game -FallDamageProof -SkipServerOutput

powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File '.\scripts\run_goldsrc_glock_damage_proof.ps1' `
  -ExecutablePath $exe -GameDir $game -LethalDeathProof -SkipServerOutput
```

Proof A first requires the mandatory zero-button autoaim phase to acquire the
other player in the autoaim cone while the direct ray misses, invoke both
engine callbacks, preserve health and ammunition, and keep both clients
responsive. It then requires one nonlethal stock Glock hit, one consumed
round, decoded target-health and shooter-clip updates, per-client acknowledged
history, stable post-shot movement for both probes, and clean shutdown. Proof B independently
gates miss, wall occlusion, shooter exclusion, exact duplicate, backup replay,
cooldown, bad checksum, malformed movement, secondary attack, disconnected
target, stale attacker, reconnect slot reuse, and cross-client weapon-state
isolation. The feature-off proof requires movement progression while attack is
masked and no combat callbacks, weapondata, or damage are emitted. The fall
proof independently gates a 400-unit landing without damage, a grounded
600-unit landing with exactly 10 points of nonlethal stock fall damage, zero
gameplay callback failures, and post-landing authoritative and decoded-wire XY
movement for both clients. The repeated-lethal proof preserves the ordinary
nonlethal `100 -> 88` control and then requires eight one-shot lethal
death/respawn cycles. Including the control, it requires exactly nine attacks,
nine executed attacks, nine consumed rounds, nine player-hit shots, 18 trace
callbacks split A=16/B=2, and zero world hits. It also requires all eight
respawn clicks to reach the lifecycle path without becoming weapon attacks,
both players alive after the eighth respawn, a valid two-rotation four-node
body-queue sequence observed directly through telemetry, zero callback
failures, continued responsiveness, and clean shutdown.

The complete regression matrix also includes the full CTest suite, every
earlier protocol proof, PM_Move Proof A/B, continuous-snapshot low-eight-bit
wrap and full-fallback coverage, two-client Probe A/B, the long-running PM_Move
proof, and the default-off query/info drift gate.

### Final automated verification record

The established matrix below was recorded from the fresh canonical Win32
Release artifacts. The post-muzzle-flash build and proof reruns used an
equivalent Release build directory because the canonical executable remained
open in the operator's live manual session; that session was not terminated.
These are automated evidence only and do not substitute for the thirteen
direct stock-client observations in the manual procedure.

| Gate | Recorded result |
| --- | --- |
| Release build | Pass. |
| CTest | 15/15 passed. |
| One-frame muzzle-flash cleanup | Focused regression passed: the shot-frame bit remains available through two modeled receiver reads, is cleared after the send sweep, can be re-armed by a later shot, and does not clear persistent effects or the world edict. |
| Mandatory autoaim phase | Pass: attack buttons stayed zero, the direct ray missed while the autoaim cone acquired the target, `pfnVecToAngles` and `pfnCrosshairAngle` were both called, health and ammunition were unchanged, and both clients remained responsive. |
| Glock Proof A | Pass: the mandatory aim phase passed and one attack was received and executed once; `StartFrame` calls = 2125; client A pre/post-think calls = 107/107; client B pre/post-think calls = 105/105. |
| Proof A weapon state | Glock clip 17 -> 16; reserve ammunition 68 -> 68; exactly one round consumed. |
| Proof A damage state | Target health 100 -> 88; 12 points of nonlethal damage; target remained alive. |
| Proof A continuity | Post-shot movement passed for clients A and B; clean shutdown passed. |
| Glock Proof B | Pass: all 13 negative/isolation gates passed. |
| Repeated lethal death Proof | Pass: nonlethal control `100 -> 88`, then 8/8 one-shot lethal death/respawn cycles. Totals including control are 9 attacks received/executed, 9 rounds, 9 player-hit shots, 18 traces (A=16, B=2), and 0 world hits. Same-victim deaths = 7; respawn inputs A=1/B=7; 8 clicks were forwarded and 0 became weapon attacks. Direct body-queue telemetry reports 8 copy calls, 4 distinct nodes, 2 completed rotations, and a valid sequence. Both players were alive after respawn eight; callback failures = 0; responsiveness and clean shutdown passed. |
| Stock fall Proof | Pass, 3/3: the 400-unit landing was grounded with no damage; the 600-unit landing was grounded with exactly 10 points of damage and the player alive; zero gameplay callback failures and post-landing authoritative plus decoded-wire XY movement passed for both clients. |
| Combat feature-off proof | Pass. |
| Noncombat two-client Probe A/B | Pass/pass. |
| PM_Move Proof A/B | Fresh frozen-tree wrapper reruns pass/pass. Proof A covers movement, friction, jump, gravity, landing, duck/unduck, wall collision, and snapshot progression; Proof B covers malformed, duplicate, stale, rollback, reconnect, and slot-reuse cases. Both shut down cleanly. |
| Player lifecycle Proof A/B | Pass/pass. |
| Continuous snapshot Proof A/B | Pass/pass. |
| Earlier external protocol matrix | Handshake, netchan, serverinfo, resource, fragmentation, signon, delta description, post-resource, world baseline, first snapshot, continuous snapshot, player lifecycle, PM_Move, and two-client A/B groups all passed. |
| Persistent PM_Move long-run | Pass: 610.098 seconds; 12,204 server snapshots sent and 7,789 received; 7,787 movement commands sent and 7,783 executed; 7 temporary clock rejections and 2 recoveries; cadence passed with a 96 ms maximum interval, 0 bursts, 2 missed intervals, 0 movement discontinuities, every 60/120/300/600-second checkpoint, and clean shutdown. |
| Default-off query gate | Pass; `normal_host_behavior_changed=0`. |
| Launcher tooling | Mutex unlocker, DryRun, and launcher smoke checks passed. |
| Exact-baseline executable | Prepared and smoke-tested successfully; real stock-client baseline observation remains pending. |

The first real stock-client combat acceptance attempt exposed a producer-side
snapshot defect before any attack was processed: the engine-owned
`clientdata_t` callback buffer was not byte-cleared before
`UpdateClientData`. HLSDK `Vector` default construction leaves components
untouched, so Glock-unowned RPG extension components could contain finite
nonsemantic data. The runtime now restores the stock engine contract by
clearing the complete buffer before the callback. A poisoned-buffer regression
test verifies full pre-callback zeroing and the untouched Glock/RPG extension
fields. Snapshot range validation remains strict; no value clamping or schema
relaxation was added. Fresh Glock Proof A, Proof B, and feature-off runs pass.

The final snapshot proof also applies an exact full-ACK ceiling while resolving
the stock low-eight-bit `clc_delta` reference. A delayed reference cannot alias
to a newer same-low8 frame generation; the focused alias regression and the
strong exact-ACK Proof B both pass.

A later manual attempt exposed the stock Glock idle-autoaim callback gap
described above. After that correction the operator confirmed shots, wall
decals, and the local view-model firing animation, but repeated player hits
exposed the lethal snapshot boundary at health `4 -> -8`. After the
death-aware correction, the operator confirmed that death completes and the
player respawns normally, then reported that the remote model's first-shot
muzzle glow remained visible. The fresh Release build, CTest 15/15, mandatory
zero-button aim phase, normal Glock Proof A, dedicated lethal death proof, all
13 Proof B gates, feature-off proof, stock fall proof, and old two-client A/B
probes pass. The one-frame muzzle-flash cleanup regression and a post-fix
normal Glock Proof A also pass. The manual baseline/current comparison, ramp
traversal, aim-near-player behavior, balcony-fall connectivity, post-fix
muzzle-flash lifetime, and complete local/remote fire feedback must still be
observed directly in the two stock clients.

Proof B's thirteen passing gates cover deliberate miss, wall occlusion,
shooter exclusion, exact duplicate suppression, backup replay suppression,
weapon cooldown, invalid checksum, malformed movement, secondary-attack
masking, disconnected-target safety, stale-attacker rejection, clean slot
reuse, and cross-client weapon-state isolation.

### Acceptance and publication state

The implementation and the automated results listed above are present, and
the older external A/B regression matrix is green, but Prompt 250 is not
complete. The two-stock-client manual combat procedure, ramp/step collision
revalidation, balcony-fall and post-landing connectivity observation,
post-fix one-frame muzzle-flash observation, complete fire-feedback
observation, and real stock-client observation against the prepared
exact-baseline executable are still pending.
No manual `yes` value may be
inferred from an automated probe or launcher smoke check. The current pending
record is maintained in
`docs/handoffs/prompt_250_completion.md`.

```text
manual_baseline_observation=pending
manual_current_combat_observation=pending
manual_ramp_revalidation=pending
manual_balcony_fall_observation=pending
manual_aim_near_player_observation=pending
manual_post_fix_death_observation=pass
manual_post_fix_respawn_observation=pass
manual_post_fix_muzzleflash_observation=pending
local_fire_feedback=missing
remote_fire_feedback=missing
next_observed_boundary=stock_one_frame_muzzleflash_manual_acceptance_required
```

The immediate observed boundary is one-frame remote-model muzzle-flash
lifetime. After that visual check passes, remote stock fire-event replication
remains a separate pending boundary: the host observes the Game DLL playback
callback but does not yet claim a complete remote audiovisual event packet.

Verified nonlethal and lethal generic Glock hits plus the observed respawn do
not mean that studio hitgroups, headshots, complete corpse/death-camera parity,
other weapons, projectiles, explosions, item pickups, scoreboard, lag
compensation, or
complete Half-Life Deathmatch combat are implemented.
