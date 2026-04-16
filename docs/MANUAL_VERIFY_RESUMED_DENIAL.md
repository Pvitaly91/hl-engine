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

The new runner accepts these parameters:

- `-RepoRoot <path>` points at the local host repo. If omitted, the current working directory is used.
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

## CI Notes

The current-main runner is suitable for future self-hosted Windows CI wiring, but it assumes the same outer-workspace layout used locally:

- repo root at `...\HLengine\host`
- outer workspace CMake entry point at `...\HLengine\CMakeLists.txt`
- prompt-scoped `logs/latest/.../runtime/valve-fixture` assets already present

If CI is added later, keep the historical reviewed verifier for audit-only reruns and call `tools\verify_resumed_denial_main.ps1` from a self-hosted workspace that preserves that layout.
