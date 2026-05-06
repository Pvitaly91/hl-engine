# Query/Info Diagnostic Release Boundary

Boundary name: `diagnostic_connectionless_query_info_regression_boundary`

Compatibility claim level:

```text
diagnostic-query-info-wrapper-release-boundary-only; no real Steam Half-Life or HLDS-compatible client compatibility claimed
```

This boundary covers prompts 287 through 295. It closes the diagnostic
connectionless query/info stack as a rerunnable, fixture-backed, loopback-only
diagnostic boundary.

It does not prove real HLDS compatibility. It does not prove real Steam
Half-Life client compatibility. It does not prove real query client
compatibility.

## Boundary Definition

- Selected fixture: `connectionless_query_info_candidate`
- Selected stage: `connectionless_query`
- Wrapper path: `scripts/run_hlds_query_info_regression.ps1`
- CI manifest: `fixtures/diagnostic/hlds/query_info_regression/query_info_regression_ci_manifest.json`
- Operator checklist: `docs/diagnostic/hlds/query_info_regression_operator_checklist.md`
- Quickstart: `docs/diagnostic/hlds/query_info_regression_quickstart.md`
- Branch: `codex/HL-CL-20260401-081-target-runtime-completion-state`

Connectionless query/info is not post-connect serverinfo. Connectionless
query/info is not signon-time serverinfo.

## Defining Commits

| Prompt | Source commit | Artifact commit | Boundary role |
|---|---|---|---|
| 287 | `ef7d69a929b16f5f55b03f932ed46fd0594adbb4` | `d332394b52faedfec4e0be52499a4c59ce0be2da` | byte-level builder/parser |
| 288 | `ddd65ea5886d28cf0f51ffd5fdbc19e4be3b74cd` | `9866d3e1e3f6ccfe00120b75bc89081d66606cdf` | diagnostic path integration |
| 289 | `b02c8d81946d827a740353c332aba8bd584d0ffc` | `f397488a5e47419f510930635285a46c75945e3d` | loopback query/response swap |
| 290 | source unchanged | `a250e0defc59894a26c00e96d81aff160c20a61e` | policy review |
| 291 | `9e9027b381c6813c63d33e243de1f86ad27d03c2` | `b11c45537492c5f0e87ab891522a9375853cf6a5` | diagnostic query client smoke |
| 292 | `4b286595663162008f80d63945529c04c6d16aed` | `8c2b3493a96c9bc57cf85e54a95d4eace36bf4a4` | regression acceptance gate |
| 293 | `17839836b97dea9c75050deb61a869fa60750bf2` | `22a1133f59c195066a8f55fdbac495cf8eb607f6` | CI manifest and fixture drift gate |
| 294 | `424ee9da370f22fc3d85956c7931450f6ac010c0` | `63a8e670ee0f6b64d3965cef6c84a8001afdb1ab` | rerun wrapper |
| 295 | `9f659b68ee53a9af8f2c325631c8ebe3b5298efd` | `ae4bfe410e5f7b82314bc05ce76f5027df60fb64` | operator checklist and quickstart |

## Rerun Commands

Dry-run command plan:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/run_hlds_query_info_regression.ps1 -Mode all -DryRun -NoBuild
```

Full diagnostic boundary run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/run_hlds_query_info_regression.ps1 -Mode all -NoBuild
```

Acceptance only:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/run_hlds_query_info_regression.ps1 -Mode acceptance -NoBuild
```

Drift only:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/run_hlds_query_info_regression.ps1 -Mode drift -NoBuild
```

## Expected Success Markers

In `query_info_rerun_wrapper_summary.json`, a passing full boundary run has:

- `wrapper_diagnostic_only=1`
- `wrapper_full_run_executed=1`
- `wrapper_full_run_passed=1`
- `acceptance_gate_included=1`
- `drift_gate_included=1`
- `selected_fixture_id=connectionless_query_info_candidate`
- `selected_fixture_stage=connectionless_query`

Expected blocked markers:

- `public_socket_opened=0`
- `lan_socket_opened=0`
- `real_client_binary_invoked=0`
- `connect_path_invoked=0`
- `post_connect_serverinfo_path_invoked=0`
- `signon_serverinfo_path_invoked=0`
- `normal_host_behavior_changed=0`

## What Is Proven

- Fixture-backed byte-level connectionless query/info response builder works.
- Parser and roundtrip validation work for the selected fixture.
- Diagnostic query/info path integration works.
- Loopback query/info response swap works.
- Opt-in diagnostic query client smoke works.
- Query/info regression acceptance gate works.
- CI manifest and fixture drift gate work.
- The rerun wrapper dry-run and full-run paths work.
- Operator checklist and quickstart docs exist for safe reruns.

## What Is Not Proven

- No real HLDS compatibility is proven.
- No real Steam Half-Life client compatibility is proven.
- No real query client compatibility is proven.
- Public socket exposure remains unproven and forbidden.
- LAN socket exposure remains unproven and forbidden.
- Connect path behavior is outside this boundary.
- Post-connect serverinfo is outside this boundary.
- Signon serverinfo is outside this boundary.
- Steam auth and no-auth LAN policy are outside this boundary.
- Netchan, reliable channels, resource/model/sound/event baselines, signon
  state, client spawn, put-in-server, and admission remain outside this
  boundary.

## Release Gate Checklist

Before running:

- Confirm the command uses only `scripts/run_hlds_query_info_regression.ps1`.
- Confirm the command does not include `-Public`, `-LAN`, `-RealClient`,
  `-Connect`, `-PostConnect`, or `-Signon`.
- Confirm the intended mode is `all`, `acceptance`, or `drift`.
- Prefer a dry-run first for changed output paths or build settings.

Run dry-run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/run_hlds_query_info_regression.ps1 -Mode all -DryRun -NoBuild
```

Run full boundary:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/run_hlds_query_info_regression.ps1 -Mode all -NoBuild
```

Inspect wrapper output:

- `wrapper_diagnostic_only=1`
- `wrapper_full_run_passed=1`
- `acceptance_gate_included=1`
- `drift_gate_included=1`
- `selected_fixture_id=connectionless_query_info_candidate`
- `selected_fixture_stage=connectionless_query`
- blocked markers all remain `0`

Inspect the acceptance summary fields parsed by the wrapper:

- `query_info_regression_acceptance_passed=1`
- `query_info_builder_parser_passed=1`
- `query_info_path_integration_passed=1`
- `query_info_loopback_swap_passed=1`
- `query_client_smoke_passed=1`
- `client_query_info_response_received=1`
- `client_query_info_response_shape_valid=1`
- `public_socket_opened=0`
- `lan_socket_opened=0`
- `real_client_binary_invoked=0`
- `connect_path_invoked=0`
- `post_connect_serverinfo_path_invoked=0`
- `signon_serverinfo_path_invoked=0`

Inspect the drift gate summary fields parsed by the wrapper:

- `drift_gate_passed=1`
- `query_info_regression_boundary_intact=1`
- `required_gates_present=1`
- `required_blocked_behaviors_present=1`
- `public_socket_policy_preserved=1`
- `lan_socket_policy_preserved=1`
- `real_client_policy_preserved=1`
- `connect_path_blocked=1`
- `post_connect_stage_blocked=1`
- `signon_stage_blocked=1`
- `fixture_drift_detected=0`
- `compatibility_claim_drift_detected=0`

Pass means both acceptance and drift pass with all blocked markers still zero.
Fail means either gate fails, fixture drift is detected, compatibility claim
drift is detected, or any public/LAN/real-client/connect/post-connect/signon
marker is non-zero.

If fixture drift is detected, do not update the manifest casually. Review the
fixture change under a separate prompt. If a public/LAN/real-client blocker
fails, treat the boundary as broken and do not use the run as release evidence.

## Boundary Risk Table

| Risk | Current mitigation | Pass evidence | Next action if it fails |
|---|---|---|---|
| Fixture drift | CI manifest hashes and drift gate | `fixture_drift_detected=0` | Review fixture change under a drift prompt |
| Accidental stage confusion | Evidence-gap guard and regression gates | query/info not post-connect/signon markers remain true | Stop and harden stage-confusion gates |
| Query/info overclaim as post-connect/signon | Release docs and guard summaries | post-connect/signon allowed markers stay `0` | Revert claim and require evidence prompt |
| Public/LAN exposure | Wrapper unsafe switches and drift policy | public/LAN markers stay `0` | Treat as release blocker |
| Real-client overclaim | Compatibility claim limit and wrapper checks | real client markers stay `0` | Treat as release blocker |
| Connect path accidentally enabled | Wrapper blocks `-Connect` and acceptance checks | `connect_path_invoked=0` | Stop and isolate connect path regression |
| Post-connect/signon path accidentally enabled | Wrapper blocks modes and acceptance checks | both path markers stay `0` | Stop and isolate serverinfo stage regression |
| Stale wrapper docs | Operator checklist and quickstart | docs point to current wrapper and markers | Update docs without changing runtime scope |
| CI manifest drift | Prompt 293 manifest and drift gate | `drift_gate_passed=1` | Review manifest separately |
| Operator misuse | Explicit forbidden switch list | unsafe switches reject before execution | Improve checklist and wrapper messages |

## Recommendation

Recommended next prompt:

```text
HL-CL-20260504-297-dedicated-goldsrc-hlds-post-connect-signon-evidence-acquisition-plan
```

Recommended next task:

```text
Plan post-connect and signon-time serverinfo byte-level evidence acquisition, because the connectionless query/info diagnostic boundary is closed and the next larger blocker remains missing post-connect/signon byte-level evidence.
```
