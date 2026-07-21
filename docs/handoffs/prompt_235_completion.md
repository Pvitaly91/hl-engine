# PROMPT 235 completion handoff

Date: 2026-07-21

## Baseline and branch

- Baseline commit: `1e65c5a48d505b6e0d89dcb4e6716281d8f7f4c3`
- Baseline subject: `feat(net): add minimal GoldSrc netchan reliable slice`
- Task branch: `codex/goldsrc-signon-serverinfo-slice`
- Repository remote: `https://github.com/Pvitaly91/hl-engine.git`
- The selected baseline is the existing netchan history. The separately preserved
  physics/Jolt work is not in this task branch ancestry and was not merged.

## Completion scope

The slice carries one bounded reliable client signon command through the existing
authoritative UDP session and netchan, emits one bounded reliable serverinfo
bootstrap, correlates its acknowledgement, and stops before gameplay admission.
The compatibility contract and reference provenance are recorded in
`docs/compatibility/goldsrc_signon_serverinfo.md`.

All 16 acceptance artifacts were audited after the final implementation:

| # | Artifact | Result |
|---:|---|---|
| 1 | Inbound client command decoder | PASS |
| 2 | Exactly-once delivery | PASS |
| 3 | Explicit signon phase | PASS |
| 4 | Complete serverinfo data model | PASS |
| 5 | Bounded serverinfo encoder | PASS |
| 6 | Checksum and client-library identity handling | PASS |
| 7 | Unit-test target | PASS |
| 8 | Codec golden tests | PASS |
| 9 | Signon state tests | PASS |
| 10 | External success proof | PASS |
| 11 | Retransmission and rejection proof | PASS |
| 12 | Compatibility documentation | PASS |
| 13 | CMake integration | PASS |
| 14 | Old handshake regression | PASS |
| 15 | Old netchan regression | PASS |
| 16 | Feature-off regression | PASS |

## Changed source and documentation files

- `CMakeLists.txt`
- `docs/compatibility/goldsrc_signon_serverinfo.md`
- `docs/handoffs/prompt_235_completion.md`
- `include/app/host_application.h`
- `include/app/launch_options.h`
- `include/game_api/hl_server_module.h`
- `include/network/goldsrc_netchan.h`
- `include/network/goldsrc_signon.h`
- `scripts/run_goldsrc_serverinfo_proof.ps1`
- `src/app/host_application.cpp`
- `src/app/launch_options.cpp`
- `src/game_api/goldsrc_udp_handshake_runtime.inc`
- `src/game_api/hl_server_module.cpp`
- `src/game_api/server_bootstrap.cpp`
- `src/game_api/server_bootstrap.h`
- `src/network/goldsrc_netchan.cpp`
- `src/network/goldsrc_signon.cpp`
- `src/tests/goldsrc_netchan_tests.cpp`
- `src/tests/goldsrc_signon_tests.cpp`

No generated binary or `build32` path belongs to this change set.

## Build commands

The documented canonical commands from the surrounding HLengine workspace are:

```powershell
cmake --preset vs2022-win32
cmake --build --preset vs2022-debug --target `
  hlhost goldsrc_connectionless_unit_tests goldsrc_netchan_unit_tests `
  goldsrc_signon_unit_tests
```

The original surrounding workspace had an empty `third_party/halflife-sdk`
checkout, so final validation used a disposable equivalent outer CMake workspace
outside this Git worktree, linked to this `host` source and the available local
SDK checkout. The exact configure and build forms were:

```powershell
cmake -S . -B build -G 'Visual Studio 17 2022' -A Win32
cmake --build build --config Debug --target `
  hlhost goldsrc_connectionless_tests goldsrc_netchan_tests goldsrc_signon_tests
```

Result: PASS. All requested targets built in Debug/Win32.

## Test and proof commands

Unit and integration registration:

```powershell
ctest --test-dir .\build\host -C Debug --output-on-failure
```

Result: PASS, 3/3 tests.

The commands below were run with `$HostExe` set to the validated Debug/Win32
`hlhost.exe` and `$ValveGameDir` set to the legally obtained deterministic local
`valve` fixture:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass `
  -File .\scripts\run_goldsrc_serverinfo_proof.ps1 `
  -ExecutablePath $HostExe -GameDir $ValveGameDir `
  -BindAddress 127.0.0.1 -Port 0 -TimeoutSeconds 30 -SkipServerOutput

powershell -NoProfile -ExecutionPolicy Bypass `
  -File .\scripts\run_goldsrc_serverinfo_proof.ps1 `
  -ExecutablePath $HostExe -GameDir $ValveGameDir `
  -BindAddress 127.0.0.1 -Port 0 -TimeoutSeconds 30 `
  -ExerciseServerInfoRetransmitAndInvalidCommands -SkipServerOutput

powershell -NoProfile -ExecutionPolicy Bypass `
  -File .\scripts\run_goldsrc_udp_handshake_proof.ps1 `
  -ExecutablePath $HostExe -GameDir $ValveGameDir `
  -BindAddress 127.0.0.1 -Port 0 -TimeoutSeconds 30 -SkipServerOutput

powershell -NoProfile -ExecutionPolicy Bypass `
  -File .\scripts\run_goldsrc_udp_handshake_proof.ps1 `
  -ExecutablePath $HostExe -GameDir $ValveGameDir `
  -BindAddress 127.0.0.1 -Port 0 -TimeoutSeconds 30 `
  -ExerciseDisconnectedSlotReuse -SkipServerOutput

powershell -NoProfile -ExecutionPolicy Bypass `
  -File .\scripts\run_goldsrc_netchan_proof.ps1 `
  -ExecutablePath $HostExe -GameDir $ValveGameDir `
  -BindAddress 127.0.0.1 -Port 0 -TimeoutSeconds 30 -SkipServerOutput

powershell -NoProfile -ExecutionPolicy Bypass `
  -File .\scripts\run_goldsrc_netchan_proof.ps1 `
  -ExecutablePath $HostExe -GameDir $ValveGameDir `
  -BindAddress 127.0.0.1 -Port 0 -TimeoutSeconds 30 `
  -ExerciseReliableRetransmit -SkipServerOutput

powershell -NoProfile -ExecutionPolicy Bypass `
  -File .\scripts\run_hlds_query_info_regression.ps1 `
  -Mode all -NoBuild -ExecutablePath $HostExe -GameDir $ValveGameDir `
  -OutDir $ExternalValidationOutput
```

## Concise validation results

| Check | Result |
|---|---|
| Debug/Win32 build | PASS |
| Full CTest | PASS (3/3) |
| Serverinfo external Proof A | PASS |
| Serverinfo retransmission/rejection Proof B | PASS |
| Existing handshake proof | PASS |
| Existing disconnected-slot-reuse proof | PASS |
| Existing netchan proof | PASS |
| Existing netchan retransmission/negative proof | PASS |
| Feature-off query/info regression | PASS |
| Normal host behavior changed | 0 |
| PowerShell parser check | PASS |
| `git diff --check` | PASS |

Both new proofs used the actual `hlhost` executable in a separate process, a
separate external UDP socket, the authoritative session, and the production
netchan/signon path. They also verified bounded completion, process cleanup, and
no repository-file mutation.

## Known limitations

- An unmodified stock Half-Life client was not tested. The exact blocker is the
  absence of an automated, reproducible harness that launches a legally obtained
  client, drives this stage on isolated loopback, records a sanitized result, and
  proves deterministic process and artifact cleanup.
- The slice supports one client and one in-flight reliable server payload. It
  intentionally does not add fragmentation or a general reliable queue.
- Later signon batches, resource exchange, baselines, snapshots, spawn, movement,
  and gameplay remain outside this milestone.
- The feature remains opt-in; feature-off behavior was regression-tested.
- Physics/Jolt validation is not applicable because those commits are not in the
  selected signon branch ancestry.

Serverinfo acknowledgement does not mean that resources, baselines, snapshots,
spawn, or gameplay are complete.

## Final Git status

- Task source and documentation are complete and ready for the single requested
  commit `feat(net): add first GoldSrc serverinfo signon slice`.
- After that commit, intended source/documentation state is clean.
- Previously generated `build32` files remain modified/untracked exactly as found,
  so the overall worktree is classified as `source-clean`, not fully clean.
- No `build32` path or generated binary is staged.
- No unrelated local change was reset, restored, deleted, or committed.
