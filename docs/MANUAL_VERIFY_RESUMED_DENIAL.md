# Manual Resumed-Denial Verification

`tools/manual_verify_resumed_denial.ps1` verifies the reviewed resumed-denial checkpoint surface around `HL-CL-20260411-163-dedicated-goldsrc-signon-carried-checkpoint-claimed-checkpoint-resumed-denial-surface`.

The helper is intentionally pinned to the reviewed commit `148e27743e7a3dd3dc5aed12faf854405b8bbb08`. It will not silently validate the moved tip of `codex/HL-CL-20260401-081-target-runtime-completion-state`. If the current `HEAD` is not the reviewed commit, rerun with `-CheckoutReviewedCommit` or manually detach to the reviewed commit first.

What it checks:

- The repo/worktree, reviewed branch, reviewed commit, and pre-change commit all exist and line up.
- `build32/host/Debug/hlhost.exe` and the prompt-scoped `runtime/valve-fixture` inputs exist.
- The exact happy run, gate run, and optional recovery run are launched with the reviewed recipe.
- The local `hlhost.exe` still exposes the reviewed dedicated/probe CLI required by that recipe before any real launch is attempted.
- The newest generated summary logs are found by pattern and `LastWriteTime`.
- The reviewed PASS signatures are still present for lifecycle readiness, the claimant checkpoint resumed-denial surface, and its probe line.
- The resumed-denial surface still binds `loopback` on a non-zero port that matches the dedicated query/connect/activation stack.
- Hard failures such as `LoadLibraryW failed`, `Unhandled exception`, or a hang produce a non-zero exit code.

Benign warnings are tolerated if the PASS markers still match:

- duplicate `mp_defaultteam` cvar registration
- missing `skill.cfg`
- missing `game.cfg`
- missing prompt-scoped `valve-fixture/sound` assets

Usage examples:

```powershell
tools\manual_verify_resumed_denial.cmd
powershell -ExecutionPolicy Bypass -File .\tools\manual_verify_resumed_denial.ps1 -CheckoutReviewedCommit
powershell -ExecutionPolicy Bypass -File .\tools\manual_verify_resumed_denial.ps1 -CheckoutReviewedCommit -SkipRecovery
powershell -ExecutionPolicy Bypass -File .\tools\manual_verify_resumed_denial.ps1 -CheckoutReviewedCommit -NoExecute
```

What PASS means:

- `happy` reports the reviewed claimant checkpoint resumed-denial surface with `accepted=1`, `rejected=0`, `resumeAllowed=no`, `denialReason=claimant-checkpoint-resumed-exhausted`, `eof=yes`, `exhausted=yes`, `nextStartMessageIndex=<none>`, `remainingMessageCount=0`, and the probe reports `attempts=1`, `accepted=1`, `rejected=0`, `lastRejectReason=<none>`, `parsedResumePolicy=claimant-checkpoint-resumed-exhausted-denied`.
- `gate` keeps the same truthful denial fields, but the surface reports `accepted=1`, `rejected=3` and the probe reports `attempts=4`, `accepted=1`, `rejected=3`, `lastRejectReason=already-claimed-checkpoint-resumed-denied`, `parsedResumePolicy=claimant-checkpoint-resumed-exhausted-denied`.
- `recovery`, when enabled, matches the same truthful terminal-denial state as `happy`.
- All executed runs retain `signonMessageCursorCarriedCheckpointClaimedCheckpointResumeEofReady=1`, `claimedCheckpointBridgeResumeExhausted=1`, `signonMessageCursorCarriedCheckpointClaimedCheckpointResumedDeniedReady=1`, and `claimedCheckpointBridgeResumedDenied=1`.

What FAIL means:

- The reviewed branch or either required commit is missing.
- `hlhost.exe` or the prompt-scoped `valve-fixture` path is missing.
- The helper is still on a moved branch tip and was not explicitly told to detach to the reviewed commit.
- A run exits unexpectedly, hangs, or fails to emit a new summary log.
- The happy, gate, or recovery signatures drift from the reviewed expectations.
- `LoadLibraryW failed`, `Unhandled exception`, or the resumed-denial loopback port no longer matches the dedicated probe stack.
