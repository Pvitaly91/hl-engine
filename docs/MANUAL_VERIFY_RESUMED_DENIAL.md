# Resumed-Denial Verification

There are now two separate resumed-denial verification lanes:

- Historical reviewed-target verification for audit and exact reviewed-commit reproduction.
- Current-main regression verification for repeatable local reruns and future CI wiring.

Both lanes target `HL-CL-20260411-163-dedicated-goldsrc-signon-carried-checkpoint-claimed-checkpoint-resumed-denial-surface`. Both lanes tolerate the same benign warnings as long as the PASS signatures still match:

- duplicate `mp_defaultteam` cvar registration
- missing `skill.cfg`
- missing `game.cfg`
- missing prompt-scoped `valve-fixture/sound` assets

## Historical Reviewed-Target Verification

Use `tools/manual_verify_resumed_denial.ps1` when you need to reproduce the reviewed audit target around commit `148e27743e7a3dd3dc5aed12faf854405b8bbb08`.

This helper remains intentionally pinned to the reviewed commit and reviewed branch. It will not silently validate the moved tip of `codex/HL-CL-20260401-081-target-runtime-completion-state`. If the current `HEAD` is not the reviewed commit, rerun with `-CheckoutReviewedCommit` or manually detach to the reviewed commit first.

If you need to preserve the current tooling worktree or build the reviewed binary in an isolated checkout, point the helper at that reviewed checkout with `-RepoRoot`. The reviewed checkout still needs its own `build32/host/Debug/hlhost.exe` and prompt-scoped `logs/latest/.../runtime/valve-fixture` inputs before the helper will run.

Typical usage:

```powershell
tools\manual_verify_resumed_denial.cmd
powershell -ExecutionPolicy Bypass -File .\tools\manual_verify_resumed_denial.ps1 -CheckoutReviewedCommit
powershell -ExecutionPolicy Bypass -File .\tools\manual_verify_resumed_denial.ps1 -RepoRoot C:\h148 -CheckoutReviewedCommit
powershell -ExecutionPolicy Bypass -File .\tools\manual_verify_resumed_denial.ps1 -CheckoutReviewedCommit -SkipRecovery
powershell -ExecutionPolicy Bypass -File .\tools\manual_verify_resumed_denial.ps1 -CheckoutReviewedCommit -NoExecute
```

## Current-Main Regression Verification

Use `tools/verify_resumed_denial_main.ps1` for repeatable regression checks against the current workspace state on `main`. This runner does not detach to the reviewed commit and does not weaken the historical reviewed-commit guardrails.

The direct current-main runner accepts these parameters:

- `-RepoRoot <path>` points at the local host repo. If omitted, the current working directory is used.
- `-OuterWorkspaceRoot <path>` overrides the default outer workspace root. If omitted, the runner uses the repo parent and requires that `<outer-workspace>\host` resolves back to `-RepoRoot`.
- `-NoBuild` skips the CMake build and expects an existing binary in the configured build directory.
- `-UseExistingBinary` also skips the build and reuses the existing binary in the configured build directory.
- `-ExePath <path>` points at an explicit `hlhost.exe` and bypasses the default build output path.
- `-BuildDir <path>` overrides the default current-main regression build directory.
- `-SkipRecovery` runs only happy and gate.
- `-NoExecute` prints the configure/build/run commands and performs binary preflight without launching the scenarios.

By default the runner uses the canonical outer-workspace CMake entry point one level above the host repo:

```powershell
"C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" -S G:\DEV\СPP\HLengine -B G:\DEV\СPP\HLengine\build-main-win32-hlhost-regression -G "Visual Studio 17 2022" -A Win32
"C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" --build G:\DEV\СPP\HLengine\build-main-win32-hlhost-regression --config Debug --target hlhost --clean-first
```

That produces the default current-main regression binary at:

```text
G:\DEV\СPP\HLengine\build-main-win32-hlhost-regression\host\Debug\hlhost.exe
```

Typical usage:

```powershell
tools\verify_resumed_denial_main.cmd
powershell -ExecutionPolicy Bypass -File .\tools\verify_resumed_denial_main.ps1 -RepoRoot G:\DEV\СPP\HLengine\host
powershell -ExecutionPolicy Bypass -File .\tools\verify_resumed_denial_main.ps1 -RepoRoot G:\DEV\СPP\HLengine\host -OuterWorkspaceRoot G:\DEV\СPP\HLengine
powershell -ExecutionPolicy Bypass -File .\tools\verify_resumed_denial_main.ps1 -RepoRoot G:\DEV\СPP\HLengine\host -NoExecute
powershell -ExecutionPolicy Bypass -File .\tools\verify_resumed_denial_main.ps1 -RepoRoot G:\DEV\СPP\HLengine\host -NoBuild
powershell -ExecutionPolicy Bypass -File .\tools\verify_resumed_denial_main.ps1 -RepoRoot G:\DEV\СPP\HLengine\host -BuildDir G:\DEV\СPP\HLengine\build-main-win32-hlhost-regression
powershell -ExecutionPolicy Bypass -File .\tools\verify_resumed_denial_main.ps1 -RepoRoot G:\DEV\СPP\HLengine\host -ExePath G:\DEV\СPP\HLengine\build-main-win32-hlhost-regression\host\Debug\hlhost.exe -UseExistingBinary
powershell -ExecutionPolicy Bypass -File .\tools\verify_resumed_denial_main.ps1 -RepoRoot G:\DEV\СPP\HLengine\host -SkipRecovery
```

What the current-main runner does:

- resolves the current host repo and the outer workspace CMake root
- builds `hlhost` unless `-NoBuild`, `-UseExistingBinary`, or `-ExePath` says otherwise
- verifies that the binary still exposes the dedicated/probe CLI surface before any real launch
- discovers the prompt-scoped `runtime/valve-fixture` game dir and the generated `logs/latest/runtime` summary logs
- launches happy, gate, and optional recovery with the same dedicated recipe used during manual review
- checks lifecycle readiness, resumed-denial surface tokens, probe tokens, and loopback port sharing
- exits `0` only if every enabled scenario passes, or exits non-zero on any failure

## CI / Self-Hosted Wrapper

Use `tools/verify_resumed_denial_ci.ps1` when you want a CI-friendly or automation-friendly entry point for the same current-main regression. This wrapper keeps the current-main logic in `tools/verify_resumed_denial_main.ps1`, but makes the outer workspace assumptions explicit and safe for self-hosted Windows runners.

How it differs from the other lanes:

- `tools/manual_verify_resumed_denial.ps1` is the historical reviewed-target audit lane. It is pinned to the reviewed commit and branch and exists to reproduce the reviewed target exactly.
- `tools/verify_resumed_denial_main.ps1` is the direct current-main regression lane. It performs the build, preflight, and runtime verification for the current checked-out workspace state.
- `tools/verify_resumed_denial_ci.ps1` is the CI/self-hosted entry lane. It resolves the host repo plus outer workspace pair, fails early if the layout is wrong or incomplete, prints the exact delegated verification command, and then calls the direct current-main runner.

The CI wrapper accepts these parameters:

- `-RepoRoot <path>` points at the local host repo. If omitted, the current working directory is used.
- `-OuterWorkspaceRoot <path>` points at the surrounding hl-engine workspace root. If omitted, the wrapper tries, in order: `-OuterWorkspaceRoot`, `HLHOST_OUTER_WORKSPACE_ROOT`, `HLENGINE_WORKSPACE_ROOT`, and the repo parent.
- `-BuildDir <path>` overrides the build directory. Relative paths resolve from the resolved outer workspace root.
- `-ExePath <path>` points at an explicit `hlhost.exe`. Relative paths resolve from the resolved outer workspace root.
- `-NoBuild` skips the build and expects an existing binary in the configured build directory.
- `-UseExistingBinary` also skips the build and reuses the configured build output path.
- `-SkipRecovery` runs only happy and gate.
- `-NoExecute` prints the delegated build and verification commands, performs binary preflight, and does not launch the runtime scenarios.

The CI wrapper refuses ambiguous layouts. The resolved outer workspace must contain `CMakeLists.txt`, and `<outer-workspace>\host` must resolve back to the requested `-RepoRoot`. That prevents automation from building one checkout while verifying another.

Typical usage:

```powershell
tools\verify_resumed_denial_ci.cmd
powershell -ExecutionPolicy Bypass -File .\tools\verify_resumed_denial_ci.ps1 -RepoRoot . -OuterWorkspaceRoot G:\DEV\СPP\HLengine
powershell -ExecutionPolicy Bypass -File .\tools\verify_resumed_denial_ci.ps1 -RepoRoot . -OuterWorkspaceRoot G:\DEV\СPP\HLengine -NoExecute
powershell -ExecutionPolicy Bypass -File .\tools\verify_resumed_denial_ci.ps1 -RepoRoot . -OuterWorkspaceRoot G:\DEV\СPP\HLengine -UseExistingBinary
powershell -ExecutionPolicy Bypass -File .\tools\verify_resumed_denial_ci.ps1 -RepoRoot . -OuterWorkspaceRoot G:\DEV\СPP\HLengine -BuildDir build-main-win32-hlhost-regression
powershell -ExecutionPolicy Bypass -File .\tools\verify_resumed_denial_ci.ps1 -RepoRoot . -ExePath build-main-win32-hlhost-regression\host\Debug\hlhost.exe -UseExistingBinary
```

Required prerequisites for self-hosted execution:

- the checked-out host repo must be available at `-RepoRoot`
- the prompt-scoped fixture must exist under `logs\latest\HL-CL-20260411-163-dedicated-goldsrc-signon-carried-checkpoint-claimed-checkpoint-resumed-denial-surface\runtime\valve-fixture`
- the resolved outer workspace root must contain `CMakeLists.txt`
- the resolved outer workspace root must treat the checked-out repo as `<outer-workspace>\host`
- Visual Studio 2022 CMake support or `cmake.exe` in `PATH` must be available when a build is requested
- if `-NoBuild`, `-UseExistingBinary`, or `-ExePath` is used, the expected `hlhost.exe` must already exist

What the CI wrapper does:

- resolves and validates the repo root plus outer workspace root pair
- fails early with actionable diagnostics if the outer workspace layout, fixture inputs, or requested binary path is missing
- prints the exact `powershell ... verify_resumed_denial_main.ps1 ...` command that it delegates to
- propagates the delegated runner exit code for CI
- emits a concise `CI Summary` block with `PASS`, `NOEXECUTE`, or `FAIL`

## PASS / FAIL Semantics

PASS means:

- `happy` reports `accepted=1`, `rejected=0`, `resumeAllowed=no`, `denialReason=claimant-checkpoint-resumed-exhausted`, `eof=yes`, `exhausted=yes`, `nextStartMessageIndex=<none>`, `remainingMessageCount=0`, and the probe reports `attempts=1`, `accepted=1`, `rejected=0`, `lastRejectReason=<none>`, `parsedResumePolicy=claimant-checkpoint-resumed-exhausted-denied`.
- `gate` keeps the same truthful denial fields, but the surface reports `accepted=1`, `rejected=3` and the probe reports `attempts=4`, `accepted=1`, `rejected=3`, `lastRejectReason=already-claimed-checkpoint-resumed-denied`, `parsedResumePolicy=claimant-checkpoint-resumed-exhausted-denied`.
- `recovery`, when enabled, matches the same truthful terminal-denial state as `happy`.
- All executed runs retain `signonMessageCursorCarriedCheckpointClaimedCheckpointResumeEofReady=1`, `claimedCheckpointBridgeResumeExhausted=1`, `signonMessageCursorCarriedCheckpointClaimedCheckpointResumedDeniedReady=1`, and `claimedCheckpointBridgeResumedDenied=1`.

FAIL means:

- the outer workspace `CMakeLists.txt`, prompt-scoped `valve-fixture`, or expected binary path is missing
- the binary no longer exposes the dedicated/probe CLI required by the recipe
- a run exits unexpectedly, hangs, or fails to emit a new summary log
- the happy, gate, or recovery signatures drift from the expected resumed-denial markers
- `LoadLibraryW failed`, `Unhandled exception`, or the resumed-denial loopback port no longer matches the dedicated probe stack

`NOEXECUTE` means:

- the wrapper or direct current-main runner successfully resolved the repo/workspace/binary inputs
- any requested build and binary preflight succeeded
- the happy, gate, and recovery runtime scenarios were intentionally not launched

## GitHub Actions Workflow

The repo now includes `.github/workflows/resumed-denial-regression.yml`.

Workflow notes:

- it is manual-only via `workflow_dispatch`
- it targets self-hosted Windows runners with `self-hosted` and `windows` labels
- it checks out the repo to `host/` and then runs `tools\verify_resumed_denial_ci.ps1`
- it exposes optional workflow inputs for `outer_workspace_root`, `build_dir`, `exe_path`, `no_build`, `use_existing_binary`, `skip_recovery`, and `no_execute`
- it does not auto-trigger on every push because the surrounding hl-engine workspace remains runner-specific and missing prerequisites should fail clearly instead of pretending the regression is universally runnable
