# PROMPT 236 completion handoff

Date: 2026-07-22

## Baseline and task branch

- Baseline branch: `codex/goldsrc-signon-serverinfo-slice`
- Baseline commit: `2a8175a8fa9dce7f2c33048254a9dd2a2f5124e8`
- Baseline subject: `feat(net): add first GoldSrc serverinfo signon slice`
- Task branch: `codex/goldsrc-resource-manifest-slice`
- Repository remote: `https://github.com/Pvitaly91/hl-engine.git`
- The baseline commit is an ancestor of the task branch.

The pre-existing modified and untracked `build32` artifacts were preserved and
excluded from this change. No generated binary belongs to the Prompt 236 source
set.

## Completed slice

The opt-in localhost path now accepts only the typed next signon request needed
by this milestone, prepares one bounded resource/precache manifest, freezes it
in the existing one-payload reliable channel, correlates its carrier ACK, and
stops before gameplay admission.

The reference-observed request is the reliable client string command `sendres`.
It is permitted only after the serverinfo reliable has been acknowledged and
the session has entered `awaiting_resource_request`. Retransmitted delivery is
transport-acknowledged but cannot advance signon or rebuild preparation state.

Behavioral evidence was corroborated from the public Xash3D-FWGS client signon
flow and reliable command writer, and the public ReHLDS server `sendres`
handler. The evidence links and compatibility limits are recorded in
`docs/compatibility/goldsrc_resource_manifest.md`.

## State, model, and bounds

The explicit signon progression is:

1. `serverinfo_acknowledged`
2. `awaiting_resource_request`
3. `resource_manifest_queued`
4. `resource_manifest_sent_awaiting_ack`
5. `resource_manifest_acknowledged`

At every phase, `put_in_server=0`, `spawned=0`, and `active=0`.

The typed manifest preserves resource category, authoritative index, normalized
relative path, bounded download-size metadata, bounded flags, and optional
protocol extension metadata. Production ordering is deterministic: generic,
sound, model, decal, then event; the world model remains model index 1. Paths,
indices, duplicate ownership, counts, sizes, flags, ordering, and encoded
capacity are validated before queueing.

Production data comes from the authoritative runtime registries and world
context. A typed production-vs-explicit-test-fixture dispatcher invokes exactly
one provider; unit tests verify the selected provider and result propagation.
The deterministic repository TSV fixture is used only by Proof A.

The complete companion-plus-list response is bounded by the existing 1200-byte
reliable capacity. If a legal production manifest cannot fit, preparation
returns typed `requires_fragmentation`, emits no partial payload, queues no
reliable data, and leaves signon at `awaiting_resource_request`. That outcome is
cached per session, so repeated reliable delivery reuses it without rebuilding.
Disconnect and slot reuse clear the cache, frozen bytes, phase, and counters.

The manifest remains frozen until a normal netchan acknowledgement both covers
an actual manifest carrier and has the correct reliable state. Non-covering,
wrong-state, stale, or future ACKs cannot clear it. A correct ACK clears pending
bytes and advances the phase exactly once.

## Changed files

- `CMakeLists.txt`
- `docs/compatibility/goldsrc_resource_manifest.md`
- `docs/handoffs/prompt_236_completion.md`
- `include/app/launch_options.h`
- `include/game_api/hl_server_module.h`
- `include/network/goldsrc_netchan.h`
- `include/network/goldsrc_resource_manifest.h`
- `include/network/goldsrc_signon.h`
- `scripts/run_goldsrc_resource_manifest_proof.ps1`
- `src/app/host_application.cpp`
- `src/app/launch_options.cpp`
- `src/game_api/goldsrc_udp_handshake_runtime.inc`
- `src/game_api/hl_server_module.cpp`
- `src/network/goldsrc_netchan.cpp`
- `src/network/goldsrc_resource_manifest.cpp`
- `src/network/goldsrc_signon.cpp`
- `src/tests/fixtures/goldsrc_resource_manifest_minimal.tsv`
- `src/tests/goldsrc_netchan_tests.cpp`
- `src/tests/goldsrc_resource_manifest_tests.cpp`
- `src/tests/goldsrc_signon_tests.cpp`

## Build and test commands

The surrounding canonical workspace requires the Half-Life SDK checkout. To
avoid touching the preserved `build32` tree, validation used the established
disposable outer Win32 CMake workspace with this source tree and the available
local SDK checkout.

```powershell
cmake --build $ValidationRoot/build --config Release --target `
  hlhost `
  goldsrc_connectionless_tests `
  goldsrc_netchan_tests `
  goldsrc_signon_tests `
  goldsrc_resource_manifest_tests

ctest --test-dir $ValidationRoot/build/host `
  -C Release --output-on-failure
```

Result: build PASS; full CTest PASS, 4/4.

The external proofs used `$HostExe` for the validated Release/Win32 executable,
`$ValveGameDir` for legally obtained local `valve` data, a separate process and
UDP socket, and loopback-only binding:

```powershell
pwsh -NoProfile -ExecutionPolicy Bypass `
  -File scripts/run_goldsrc_resource_manifest_proof.ps1 `
  -ExecutablePath $HostExe -GameDir $ValveGameDir `
  -BindAddress 127.0.0.1 -Port 0 -TimeoutSeconds 300 `
  -SkipServerOutput

pwsh -NoProfile -ExecutionPolicy Bypass `
  -File scripts/run_goldsrc_resource_manifest_proof.ps1 `
  -ExecutablePath $HostExe -GameDir $ValveGameDir `
  -BindAddress 127.0.0.1 -Port 0 -TimeoutSeconds 300 `
  -NegativeProof -SkipServerOutput

pwsh -NoProfile -ExecutionPolicy Bypass `
  -File scripts/run_goldsrc_udp_handshake_proof.ps1 `
  -ExecutablePath $HostExe -GameDir $ValveGameDir `
  -BindAddress 127.0.0.1 -Port 0 -TimeoutSeconds 300 `
  -SkipServerOutput

pwsh -NoProfile -ExecutionPolicy Bypass `
  -File scripts/run_goldsrc_udp_handshake_proof.ps1 `
  -ExecutablePath $HostExe -GameDir $ValveGameDir `
  -BindAddress 127.0.0.1 -Port 0 -TimeoutSeconds 300 `
  -ExerciseDisconnectedSlotReuse -SkipServerOutput

pwsh -NoProfile -ExecutionPolicy Bypass `
  -File scripts/run_goldsrc_netchan_proof.ps1 `
  -ExecutablePath $HostExe -GameDir $ValveGameDir `
  -BindAddress 127.0.0.1 -Port 0 -TimeoutSeconds 300 `
  -SkipServerOutput

pwsh -NoProfile -ExecutionPolicy Bypass `
  -File scripts/run_goldsrc_netchan_proof.ps1 `
  -ExecutablePath $HostExe -GameDir $ValveGameDir `
  -BindAddress 127.0.0.1 -Port 0 -TimeoutSeconds 300 `
  -ExerciseReliableRetransmit -SkipServerOutput

pwsh -NoProfile -ExecutionPolicy Bypass `
  -File scripts/run_goldsrc_serverinfo_proof.ps1 `
  -ExecutablePath $HostExe -GameDir $ValveGameDir `
  -BindAddress 127.0.0.1 -Port 0 -TimeoutSeconds 300 `
  -SkipServerOutput

pwsh -NoProfile -ExecutionPolicy Bypass `
  -File scripts/run_goldsrc_serverinfo_proof.ps1 `
  -ExecutablePath $HostExe -GameDir $ValveGameDir `
  -BindAddress 127.0.0.1 -Port 0 -TimeoutSeconds 300 `
  -ExerciseServerInfoRetransmitAndInvalidCommands -SkipServerOutput

pwsh -NoProfile -ExecutionPolicy Bypass `
  -File scripts/run_hlds_query_info_regression.ps1 `
  -Mode all -NoBuild -ExecutablePath $HostExe `
  -GameDir $ValveGameDir -OutDir $ExternalValidationOutput
```

## Sanitized validation results

| Check | Result |
|---|---|
| Release/Win32 build | PASS |
| Full CTest | PASS (4/4) |
| Resource-manifest Proof A | PASS |
| Resource-manifest Proof B | PASS |
| Existing handshake proof | PASS |
| Existing disconnected-slot-reuse proof | PASS |
| Existing normal netchan proof | PASS |
| Existing netchan retransmission proof | PASS |
| Existing serverinfo Proof A | PASS |
| Existing serverinfo Proof B | PASS |
| Feature-off query/info regression | PASS |
| Normal host behavior changed | 0 |
| PowerShell parser validation | PASS |
| Repository formatting / `git diff --check` | PASS |

Proof A externally decoded the complete deterministic manifest, verified order,
indices, world resource, bounds, lifecycle state, final ACK, clean process exit,
and no repository mutation. Proof B verified frozen retransmission, ACK
correlation, duplicate-request suppression, unsupported-request stability, the
authoritative production `requires_fragmentation` result, exactly one
preparation attempt with cached reuse, server responsiveness, clean shutdown,
and no repository mutation.

## Stock-client status and limitations

- `stock_client_tested=no`.
- No automated, reproducible harness is available that launches an unmodified,
  legally obtained Half-Life client, drives this exact localhost stage, captures
  only sanitized evidence, and proves deterministic process/artifact cleanup.
- Fragmentation and reassembly are intentionally not implemented.
- The slice supports one client and one in-flight reliable application payload.
- File transfer, consistency enforcement, later signon batches, delta
  descriptions, baselines, snapshots, spawn, and gameplay remain out of scope.
- The path remains opt-in and loopback-only; feature-off behavior was verified.

Resource-manifest acknowledgement does not mean that resource download,
consistency verification, delta descriptions, baselines, snapshots,
spawn, or gameplay are complete.

## Final Git status

- Prompt 236 source, tests, proof, compatibility documentation, and this handoff
  are complete for the requested single commit
  `feat(net): add GoldSrc resource manifest signon slice`.
- The Prompt 236 change set contains 20 source/documentation/test files.
- No `build32` path or generated binary is staged or belongs to the change set.
- The pre-existing `build32` modifications and untracked artifacts remain
  preserved, so the post-commit worktree classification is `source-clean`.
- No unrelated local change was reset, restored, deleted, or committed.
- The task branch contains the verified baseline and is intended for a normal,
  non-force push. No pull request is requested.
