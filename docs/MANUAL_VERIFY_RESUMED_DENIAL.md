# Resumed-Denial Verification

There are now four related verification lanes:

- Historical reviewed-target verification for audit and exact reviewed-commit reproduction.
- Current-main resumed-denial regression verification for repeatable local reruns on `main`.
- Current-main neighboring signon-surface verification for adjacent checkpoint/token/claimed-checkpoint surfaces on `main`.
- CI/self-hosted orchestration that can run the current-main resumed-denial lane alone or pair it with the neighboring-surface suite.

The resumed-denial and neighboring-surface current-main checks all target `HL-CL-20260411-163-dedicated-goldsrc-signon-carried-checkpoint-claimed-checkpoint-resumed-denial-surface`. They tolerate the same benign warnings as long as the PASS signatures still match:

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
- `-ProvenanceMode <auto|runtime|workspace>` selects how current-main verification proves the binary/workspace pairing. `auto` prefers runtime `codex_run_identity`, but can fall back to workspace/build provenance when runtime git identity is `<unknown>`.
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
powershell -ExecutionPolicy Bypass -File .\tools\verify_resumed_denial_main.ps1 -RepoRoot G:\DEV\СPP\HLengine\host -OuterWorkspaceRoot G:\DEV\СPP\HLengine -ProvenanceMode auto
powershell -ExecutionPolicy Bypass -File .\tools\verify_resumed_denial_main.ps1 -RepoRoot G:\DEV\СPP\HLengine\host -NoExecute
powershell -ExecutionPolicy Bypass -File .\tools\verify_resumed_denial_main.ps1 -RepoRoot G:\DEV\СPP\HLengine\host -NoBuild
powershell -ExecutionPolicy Bypass -File .\tools\verify_resumed_denial_main.ps1 -RepoRoot G:\DEV\СPP\HLengine\host -BuildDir G:\DEV\СPP\HLengine\build-main-win32-hlhost-regression
powershell -ExecutionPolicy Bypass -File .\tools\verify_resumed_denial_main.ps1 -RepoRoot G:\DEV\СPP\HLengine\host -ExePath G:\DEV\СPP\HLengine\build-main-win32-hlhost-regression\host\Debug\hlhost.exe -UseExistingBinary
powershell -ExecutionPolicy Bypass -File .\tools\verify_resumed_denial_main.ps1 -RepoRoot G:\DEV\СPP\HLengine\host -ExePath G:\DEV\СPP\HLengine\build-main-win32-hlhost-regression\host\Debug\hlhost.exe -UseExistingBinary -ProvenanceMode workspace
powershell -ExecutionPolicy Bypass -File .\tools\verify_resumed_denial_main.ps1 -RepoRoot G:\DEV\СPP\HLengine\host -ProvenanceMode runtime
powershell -ExecutionPolicy Bypass -File .\tools\verify_resumed_denial_main.ps1 -RepoRoot G:\DEV\СPP\HLengine\host -SkipRecovery
```

What the current-main runner does:

- resolves the current host repo and the outer workspace CMake root
- builds `hlhost` unless `-NoBuild`, `-UseExistingBinary`, or `-ExePath` says otherwise
- verifies that the binary still exposes the dedicated/probe CLI surface before any real launch
- prints the requested provenance mode, expected `HEAD`, executable path, expected build output path, and whether workspace provenance fallback is eligible
- discovers the prompt-scoped `runtime/valve-fixture` game dir and the generated `logs/latest/runtime` summary logs
- launches happy, gate, and optional recovery with the same dedicated recipe used during manual review
- checks lifecycle readiness, resumed-denial surface tokens, probe tokens, loopback port sharing, and per-run provenance
- exits `0` only if every enabled scenario passes, or exits non-zero on any failure

### Current-Main Provenance Modes

The historical reviewed-target verifier stays strict and still requires runtime `codex_run_identity` to name the reviewed commit. Only the current-main runner and CI wrapper support workspace/build provenance fallback.

Current-main provenance modes:

- `auto`: preferred for local reruns and CI. If runtime `codex_run_identity` reports the expected `gitCommit`, the runner uses `runtime` provenance. If runtime `gitCommit=<unknown>`, the runner can fall back to `workspace` provenance when the executable is the expected build output under the resolved build directory.
- `runtime`: strict current-main mode. Runtime `codex_run_identity` must report the expected `gitCommit` or the run fails.
- `workspace`: explicit current-main fallback mode. The runner still rejects a conflicting runtime `gitCommit`, but accepts `gitCommit=<unknown>` when the repo root, `HEAD`, build directory, and expected build output path are all known and the executable matches that expected build output.

How reviewers can tell which mode was used:

- before launch, the runner prints `Requested provenance mode: ...` plus `Workspace fallback eligible: yes|no`
- after each executed run, the runner prints `provenance: runtime` or `provenance: workspace`
- the per-run `provenance detail:` line explains whether the decision came from runtime `codex_run_identity` or from workspace/build fallback

## Current-Main Neighboring Surface Suite

Use `tools/verify_signon_neighbor_surfaces_main.ps1` when you want a focused neighboring regression suite around the same signon-message cursor / checkpoint / token / claimed-checkpoint state machine area without folding those checks into the resumed-denial verifier.

This suite intentionally reuses the current-main happy/gate execution recipe from `tools/verify_resumed_denial_main.ps1`, then checks adjacent surface and probe lines in the generated happy/gate summary logs through a declarative matrix file instead of a hardcoded surface list.

The declarative matrix lives at `tools/signon_regression_matrix.psd1`. Each entry records:

- the surface name
- one or more logical groups
- whether the entry is enabled by default
- the expected happy/gate token sets
- optional mode/notes metadata

The default enabled matrix entries currently cover these eight neighboring surfaces:

- `signon-message-cursor-carried-checkpoint-resume-token`
- `signon-message-cursor-carried-checkpoint-resume-token-claim`
- `signon-message-cursor-carried-checkpoint-claimed-resume-allow`
- `signon-message-cursor-carried-checkpoint-claimed-checkpoint-bridge`
- `signon-message-cursor-carried-checkpoint-claimed-checkpoint-resume-allow`
- `signon-message-cursor-carried-checkpoint-claimed-checkpoint-resume-range`
- `signon-message-cursor-carried-checkpoint-claimed-checkpoint-resume-eof`
- `signon-message-cursor-carried-checkpoint-claimed-checkpoint-resumed-denial`

The matrix also tags those entries into logical groups such as:

- `resume-token`
- `claimed-checkpoint`
- `claimed-checkpoint-bridge`
- `claimed-checkpoint-resume-allow`
- `claimed-checkpoint-resume-range`
- `claimed-checkpoint-resume-eof`
- `resumed-denial`

What this suite intentionally does not cover yet:

- unrelated signon-message cursor surfaces outside the checkpoint/token/claimed-checkpoint neighborhood
- a full audit of every signon surface
- a separate recovery lane; this suite currently targets happy and gate only

The neighboring-surface suite accepts these parameters:

- `-RepoRoot <path>` points at the local host repo. If omitted, the current working directory is used.
- `-OuterWorkspaceRoot <path>` points at the surrounding hl-engine workspace root.
- `-BuildDir <path>` overrides the build directory.
- `-ExePath <path>` points at an explicit `hlhost.exe`.
- `-NoBuild` skips a build when the suite delegates to the current-main resumed-denial runner.
- `-UseExistingBinary` reuses the configured or explicit binary.
- `-NoExecute` validates the delegated happy/gate recipe but does not launch the runtime scenarios.
- `-ProvenanceMode <auto|runtime|workspace>` forwards the current-main provenance policy.
- `-MatrixPath <path>` overrides the declarative matrix file. Relative paths resolve from the repo root.
- `-ListSurfaces` prints the known matrix entries, enabled-by-default state, and group membership without running verification.
- `-Group <name> [<name> ...]` limits the suite to entries in the requested logical groups.
- `-Surface <name> [<name> ...]` limits the suite to the requested matrix entries by exact name.
- `-CollectArtifacts` writes a stable suite artifact bundle.
- `-ArtifactOutputDir <path>` overrides the suite artifact directory. If omitted with `-CollectArtifacts`, the suite uses `artifacts\signon-neighbor-surfaces`.

Filter behavior:

- with no `-Group` or `-Surface`, the suite runs the matrix entries whose `EnabledByDefault` flag is `true`
- `-Group` narrows the run to entries that belong to one or more named groups
- `-Surface` narrows the run to one or more exact surface names
- when both `-Group` and `-Surface` are provided, an entry must match both filters

Typical usage:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\verify_signon_neighbor_surfaces_main.ps1 -RepoRoot . -ListSurfaces
powershell -ExecutionPolicy Bypass -File .\tools\verify_signon_neighbor_surfaces_main.ps1 -RepoRoot . -OuterWorkspaceRoot G:\DEV\СPP\HLengine -BuildDir G:\DEV\СPP\HLengine\build-main-win32-hlhost-regression -UseExistingBinary -ProvenanceMode auto
powershell -ExecutionPolicy Bypass -File .\tools\verify_signon_neighbor_surfaces_main.ps1 -RepoRoot . -Group claimed-checkpoint
powershell -ExecutionPolicy Bypass -File .\tools\verify_signon_neighbor_surfaces_main.ps1 -RepoRoot . -Surface signon-message-cursor-carried-checkpoint-claimed-checkpoint-resume-eof
powershell -ExecutionPolicy Bypass -File .\tools\verify_signon_neighbor_surfaces_main.ps1 -RepoRoot . -OuterWorkspaceRoot G:\DEV\СPP\HLengine -BuildDir G:\DEV\СPP\HLengine\build-main-win32-hlhost-regression -UseExistingBinary -ProvenanceMode auto -NoExecute
powershell -ExecutionPolicy Bypass -File .\tools\verify_signon_neighbor_surfaces_main.ps1 -RepoRoot . -OuterWorkspaceRoot G:\DEV\СPP\HLengine -BuildDir G:\DEV\СPP\HLengine\build-main-win32-hlhost-regression -UseExistingBinary -ProvenanceMode auto -CollectArtifacts -ArtifactOutputDir .\artifacts\signon-neighbors
```

What PASS means for this suite:

- the delegated current-main happy/gate recipe itself still passes
- each selected surface emits both its dedicated surface line and probe line
- the selected happy/gate assertions still match the stable accepted/rejected/policy markers derived from current runtime behavior
- the grouped summary also stays green for every selected logical group

What FAIL means for this suite:

- the delegated happy/gate recipe fails
- a selected neighboring surface line or probe line is missing
- a selected surface drifts away from the expected accepted/rejected/policy markers for happy or gate
- any selected logical group contains a failing surface

## CI / Self-Hosted Wrapper

Use `tools/verify_resumed_denial_ci.ps1` when you want a CI-friendly or automation-friendly entry point for the same current-main regression. This wrapper keeps the current-main logic in `tools/verify_resumed_denial_main.ps1`, but makes the outer workspace assumptions explicit and safe for self-hosted Windows runners.

How it differs from the other lanes:

- `tools/manual_verify_resumed_denial.ps1` is the historical reviewed-target audit lane. It is pinned to the reviewed commit and branch and exists to reproduce the reviewed target exactly.
- `tools/verify_resumed_denial_main.ps1` is the direct current-main regression lane. It performs the build, preflight, and runtime verification for the current checked-out workspace state.
- `tools/verify_signon_neighbor_surfaces_main.ps1` is the direct current-main neighboring-surface lane. It reuses the current-main happy/gate recipe, then checks adjacent checkpoint/token/claimed-checkpoint surfaces.
- `tools/verify_resumed_denial_ci.ps1` is the CI/self-hosted entry lane. It resolves the host repo plus outer workspace pair, fails early if the layout is wrong or incomplete, prints the exact delegated verification command, and then calls the direct current-main resumed-denial runner, with an optional neighboring-surface follow-up lane.

The CI wrapper accepts these parameters:

- `-RepoRoot <path>` points at the local host repo. If omitted, the current working directory is used.
- `-OuterWorkspaceRoot <path>` points at the surrounding hl-engine workspace root. If omitted, the wrapper tries, in order: `-OuterWorkspaceRoot`, `HLHOST_OUTER_WORKSPACE_ROOT`, `HLENGINE_WORKSPACE_ROOT`, and the repo parent.
- `-ProvenanceMode <auto|runtime|workspace>` forwards the requested provenance policy to `tools/verify_resumed_denial_main.ps1`. The wrapper default is `auto`.
- `-BuildDir <path>` overrides the build directory. Relative paths resolve from the resolved outer workspace root.
- `-ExePath <path>` points at an explicit `hlhost.exe`. Relative paths resolve from the resolved outer workspace root.
- `-NoBuild` skips the build and expects an existing binary in the configured build directory.
- `-UseExistingBinary` also skips the build and reuses the configured build output path.
- `-SkipRecovery` runs only happy and gate.
- `-NoExecute` prints the delegated build and verification commands, performs binary preflight, and does not launch the runtime scenarios.
- `-RunNeighborSurfaceSuite` keeps resumed-denial as the primary suite, then also runs `tools/verify_signon_neighbor_surfaces_main.ps1` against the same repo/workspace/build/provenance context using the already-built binary.
- `-NeighborSurfaceGroup <name> [<name> ...]` forwards neighboring-surface group filters to the matrix-backed runner. These filters require `-RunNeighborSurfaceSuite`.
- `-NeighborSurface <name> [<name> ...]` forwards exact neighboring-surface names to the matrix-backed runner. These filters require `-RunNeighborSurfaceSuite`.
- `-CollectArtifacts` enables post-run artifact bundling and summary generation.
- `-ArtifactOutputDir <path>` overrides the artifact bundle output directory. Relative paths resolve from the resolved repo root. If omitted with `-CollectArtifacts`, the wrapper uses `artifacts\resumed-denial` for resumed-denial-only runs, or `artifacts\signon-regressions` when `-RunNeighborSurfaceSuite` is enabled.

The CI wrapper refuses ambiguous layouts. The resolved outer workspace must contain `CMakeLists.txt`, and `<outer-workspace>\host` must resolve back to the requested `-RepoRoot`. That prevents automation from building one checkout while verifying another.

Typical usage:

```powershell
tools\verify_resumed_denial_ci.cmd
powershell -ExecutionPolicy Bypass -File .\tools\verify_resumed_denial_ci.ps1 -RepoRoot . -OuterWorkspaceRoot G:\DEV\СPP\HLengine
powershell -ExecutionPolicy Bypass -File .\tools\verify_resumed_denial_ci.ps1 -RepoRoot . -OuterWorkspaceRoot G:\DEV\СPP\HLengine -ProvenanceMode auto
powershell -ExecutionPolicy Bypass -File .\tools\verify_resumed_denial_ci.ps1 -RepoRoot . -OuterWorkspaceRoot G:\DEV\СPP\HLengine -NoExecute
powershell -ExecutionPolicy Bypass -File .\tools\verify_resumed_denial_ci.ps1 -RepoRoot . -OuterWorkspaceRoot G:\DEV\СPP\HLengine -UseExistingBinary -ProvenanceMode auto
powershell -ExecutionPolicy Bypass -File .\tools\verify_resumed_denial_ci.ps1 -RepoRoot . -OuterWorkspaceRoot G:\DEV\СPP\HLengine -BuildDir build-main-win32-hlhost-regression
powershell -ExecutionPolicy Bypass -File .\tools\verify_resumed_denial_ci.ps1 -RepoRoot . -ExePath build-main-win32-hlhost-regression\host\Debug\hlhost.exe -UseExistingBinary -ProvenanceMode workspace
powershell -ExecutionPolicy Bypass -File .\tools\verify_resumed_denial_ci.ps1 -RepoRoot . -OuterWorkspaceRoot G:\DEV\СPP\HLengine -BuildDir G:\DEV\СPP\HLengine\build-main-win32-hlhost-regression -UseExistingBinary -ProvenanceMode auto -CollectArtifacts -ArtifactOutputDir .\artifacts\resumed-denial
powershell -ExecutionPolicy Bypass -File .\tools\verify_resumed_denial_ci.ps1 -RepoRoot . -OuterWorkspaceRoot G:\DEV\СPP\HLengine -BuildDir G:\DEV\СPP\HLengine\build-main-win32-hlhost-regression -UseExistingBinary -ProvenanceMode auto -RunNeighborSurfaceSuite -CollectArtifacts -ArtifactOutputDir .\artifacts\signon-neighbors
powershell -ExecutionPolicy Bypass -File .\tools\verify_resumed_denial_ci.ps1 -RepoRoot . -OuterWorkspaceRoot G:\DEV\СPP\HLengine -BuildDir G:\DEV\СPP\HLengine\build-main-win32-hlhost-regression -UseExistingBinary -RunNeighborSurfaceSuite -NeighborSurfaceGroup claimed-checkpoint -CollectArtifacts -ArtifactOutputDir .\artifacts\claimed-checkpoint-suite
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
- prints the requested provenance mode alongside the resolved repo/workspace/build inputs
- prints the exact `powershell ... verify_resumed_denial_main.ps1 ...` command that it delegates to
- when `-RunNeighborSurfaceSuite` is enabled, prints the exact `powershell ... verify_signon_neighbor_surfaces_main.ps1 ...` command too and reuses the already-built or already-resolved `hlhost.exe`
- when neighboring-surface filters are supplied, forwards the selected groups and/or surface names into the matrix-backed runner
- when `-CollectArtifacts` is enabled, captures the delegated runner output, writes wrapper metadata, and calls `tools/collect_resumed_denial_artifacts.ps1`
- when `-RunNeighborSurfaceSuite` and `-CollectArtifacts` are both enabled, keeps the resumed-denial bundle under `resumed-denial\`, keeps the neighboring-surface bundle under `neighbor-surfaces\`, and writes a root `verification_suite_summary.txt` plus `verification_suite_summary.json`
- propagates the delegated runner exit code for CI
- emits a concise `CI Summary` block with `PASS`, `NOEXECUTE`, or `FAIL` plus the collected artifact summary paths when available

## Artifact Collection

Use `tools/collect_resumed_denial_artifacts.ps1` when you want to turn the newest resumed-denial logs into a stable artifact bundle and a compact txt/json summary.

The resumed-denial collector stays focused on the resumed-denial lane. When `-RunNeighborSurfaceSuite` is enabled, the neighboring suite writes its own stable bundle directly and the CI wrapper adds the root `verification_suite_summary.txt/json` files that point at both bundles.

The collector accepts these parameters:

- `-RepoRoot <path>` points at the host repo whose `logs\latest` tree should be searched.
- `-LogsRoot <path>` optionally points at the logs root to search. It can be either `logs` or `logs\latest`; if omitted, the collector uses `<RepoRoot>\logs\latest`.
- `-OutputDir <path>` selects the bundle output directory.
- `-RunLabelPattern <pattern>` narrows the selected run label set. The default is `verify-resumed-denial-main-*`.
- `-IncludeCodexArtifacts` copies matching `logs\latest\codex\*_manifest.json` files into the bundle when they exist.
- `-AllowMissingCodexArtifacts` keeps the collector from failing when the codex-side manifests are unavailable.

Typical usage:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\collect_resumed_denial_artifacts.ps1 -RepoRoot . -OutputDir .\artifacts\resumed-denial -AllowMissingCodexArtifacts
powershell -ExecutionPolicy Bypass -File .\tools\collect_resumed_denial_artifacts.ps1 -RepoRoot . -OutputDir .\artifacts\resumed-denial -RunLabelPattern verify-resumed-denial-main-* -IncludeCodexArtifacts -AllowMissingCodexArtifacts
```

What the collector writes:

- `resumed_denial_summary.txt`
- `resumed_denial_summary.json`
- `metadata\verification_metadata.json` when the CI wrapper invoked the collector
- `metadata\verify_resumed_denial_ci_output.log` when the CI wrapper invoked the collector
- `runtime\happy\summary.log`, `runtime\gate\summary.log`, and `runtime\recovery\summary.log` when those runs produced logs
- matching `runtime\<scenario>\*_part*.log` files
- matching `codex\<scenario>\manifest.json` files when requested and available

What the neighboring-surface suite writes when `-CollectArtifacts` is enabled:

- `signon_neighbor_surface_summary.txt`
- `signon_neighbor_surface_summary.json`
- `metadata\verify_signon_neighbor_surfaces_output.log`
- `metadata\delegated_verify_resumed_denial_metadata.json`
- `runtime\happy\summary.log` and `runtime\gate\summary.log`
- matching `runtime\happy\*_part*.log` and `runtime\gate\*_part*.log`
- matching `codex\happy\manifest.json` and `codex\gate\manifest.json` when they were available
- selected group names plus per-group PASS/FAIL counts inside `signon_neighbor_surface_summary.txt/json`

What the combined wrapper bundle adds when `-RunNeighborSurfaceSuite` is enabled:

- `verification_suite_summary.txt`
- `verification_suite_summary.json`
- `resumed-denial\...` resumed-denial bundle files
- `neighbor-surfaces\...` neighboring-surface bundle files

The txt/json summaries record:

- happy, gate, and recovery result states
- requested and observed provenance mode when known
- repo root, build dir, executable path, and selected run label pattern
- source and copied summary-log paths
- whether codex-side artifacts were found
- overall `PASS`, `FAIL`, or `NOEXECUTE`

The combined wrapper summary txt/json records:

- resumed-denial overall result plus happy/gate/recovery scenario results
- neighboring-surface overall result plus the selected group list, per-group PASS/FAIL, and per-surface PASS/FAIL
- requested provenance mode
- repo root, outer workspace root, build dir, executable path, and artifact root
- the per-suite artifact directories and summary file paths

## PASS / FAIL Semantics

PASS means:

- `happy` reports `accepted=1`, `rejected=0`, `resumeAllowed=no`, `denialReason=claimant-checkpoint-resumed-exhausted`, `eof=yes`, `exhausted=yes`, `nextStartMessageIndex=<none>`, `remainingMessageCount=0`, and the probe reports `attempts=1`, `accepted=1`, `rejected=0`, `lastRejectReason=<none>`, `parsedResumePolicy=claimant-checkpoint-resumed-exhausted-denied`.
- `gate` keeps the same truthful denial fields, but the surface reports `accepted=1`, `rejected=3` and the probe reports `attempts=4`, `accepted=1`, `rejected=3`, `lastRejectReason=already-claimed-checkpoint-resumed-denied`, `parsedResumePolicy=claimant-checkpoint-resumed-exhausted-denied`.
- `recovery`, when enabled, matches the same truthful terminal-denial state as `happy`.
- All executed runs retain `signonMessageCursorCarriedCheckpointClaimedCheckpointResumeEofReady=1`, `claimedCheckpointBridgeResumeExhausted=1`, `signonMessageCursorCarriedCheckpointClaimedCheckpointResumedDeniedReady=1`, and `claimedCheckpointBridgeResumedDenied=1`.
- each executed run prints whether provenance came from `runtime` or `workspace`

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

For the neighboring-surface suite specifically:

- PASS means the delegated happy/gate recipe still passed and all selected adjacent checkpoint/token/claimed-checkpoint assertions matched.
- FAIL means the delegated happy/gate recipe failed or any selected adjacent surface drifted from its expected happy/gate markers.
- `NOEXECUTE` means the suite validated the delegated happy/gate recipe shape and binary path but intentionally did not launch the runtime scenarios.

## GitHub Actions Workflow

The repo now includes `.github/workflows/resumed-denial-regression.yml`.

Workflow notes:

- it is manual-only via `workflow_dispatch`
- it targets self-hosted Windows runners with `self-hosted` and `windows` labels
- it checks out the repo to `host/` and then runs `tools\verify_resumed_denial_ci.ps1`
- it exposes optional workflow inputs for `outer_workspace_root`, `build_dir`, `exe_path`, `provenance_mode`, `no_build`, `use_existing_binary`, `skip_recovery`, `no_execute`, `run_neighbor_surface_suite`, `neighbor_surface_group`, and `neighbor_surface`
- it always enables `-CollectArtifacts` for the workflow run, uploads the resulting bundle with `actions/upload-artifact`, and writes either the resumed-denial summary or the combined suite summary to `GITHUB_STEP_SUMMARY`
- when `run_neighbor_surface_suite=true`, the uploaded artifact bundle is rooted at `signon-regression-...` and includes both `resumed-denial\` and `neighbor-surfaces\` bundles plus `verification_suite_summary.txt/json`
- when `neighbor_surface_group` or `neighbor_surface` is supplied, those comma- or newline-separated filters are forwarded to the matrix-backed neighboring-surface runner
- it does not auto-trigger on every push because the surrounding hl-engine workspace remains runner-specific and missing prerequisites should fail clearly instead of pretending the regression is universally runnable
