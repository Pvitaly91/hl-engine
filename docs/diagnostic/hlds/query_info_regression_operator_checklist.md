# Query/Info Regression Operator Checklist

This checklist is for running the diagnostic connectionless query/info
regression wrapper added for the HLDS query/info diagnostic boundary.

Compatibility claim level:

```text
diagnostic-query-info-wrapper-operator-docs-only; no real Steam Half-Life or HLDS-compatible client compatibility claimed
```

Wrapper path:

```powershell
scripts/run_hlds_query_info_regression.ps1
```

## Scope

The wrapper reruns only the diagnostic query/info boundary:

- prompt 292 query/info loopback regression acceptance gate
- prompt 293 query/info CI manifest and fixture drift gate

The selected fixture remains `connectionless_query_info_candidate`, and the
selected stage remains `connectionless_query`.

Connectionless query/info is not post-connect serverinfo. It is not signon-time
serverinfo. Passing this wrapper does not prove real HLDS, real Steam
Half-Life, or HLDS-compatible client compatibility.

## Supported Modes

- `-Mode all`: runs acceptance and drift gate.
- `-Mode acceptance`: runs only the prompt 292 acceptance gate.
- `-Mode drift`: runs only the prompt 293 drift gate.

Use `-DryRun` to generate a command plan without running `hlhost.exe`. Use
`-NoBuild` when the existing Debug host build is already available. Use
`-Build` only when local MSBuild is available and a rebuild is intended.

## Commands

Dry-run command plan:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/run_hlds_query_info_regression.ps1 -Mode all -DryRun -NoBuild
```

Full diagnostic run:

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

## Expected Output

The wrapper writes these files under its output directory:

- `query_info_rerun_command_plan.json`
- `query_info_rerun_command_log.txt`
- `query_info_rerun_wrapper_summary.json`

For the default output directory, look under:

```text
logs/latest/HL-CL-20260504-294-dedicated-goldsrc-hlds-query-info-regression-rerun-command-wrapper/wrapper
```

Successful JSON summary markers:

- `wrapper_diagnostic_only=1`
- `acceptance_gate_included=1`
- `drift_gate_included=1`
- `selected_fixture_id=connectionless_query_info_candidate`
- `selected_fixture_stage=connectionless_query`
- `public_socket_opened=0`
- `lan_socket_opened=0`
- `real_client_binary_invoked=0`
- `connect_path_invoked=0`
- `post_connect_serverinfo_path_invoked=0`
- `signon_serverinfo_path_invoked=0`
- `normal_host_behavior_changed=0`

For a full run, also expect:

- `wrapper_full_run_executed=1`
- `wrapper_full_run_passed=1`
- `not_run_with_reason=<none>`

For a dry run, expect:

- `wrapper_dry_run_supported=1`
- `wrapper_dry_run_passed=1`
- `wrapper_full_run_executed=0`
- `not_run_with_reason=dry_run_only`

## Safety Checklist Before Running

- Confirm you are intentionally running only the diagnostic query/info wrapper.
- Confirm the command does not include `-Public`, `-LAN`, `-RealClient`,
  `-Connect`, `-PostConnect`, or `-Signon`.
- Confirm no real client, real Steam client, or Half-Life client binary is part
  of the command.
- Confirm the target fixture remains `connectionless_query_info_candidate`.
- Confirm the target stage remains `connectionless_query`.
- Prefer `-DryRun` first when changing output directories or build settings.

## Safety Checklist After Running

Open `query_info_rerun_wrapper_summary.json` and confirm:

- `public_socket_opened=0`
- `lan_socket_opened=0`
- `real_client_binary_invoked=0`
- `connect_path_invoked=0`
- `post_connect_serverinfo_path_invoked=0`
- `signon_serverinfo_path_invoked=0`
- `normal_host_behavior_changed=0`
- `acceptance_gate_included=1` when using `-Mode all`
- `drift_gate_included=1` when using `-Mode all`

If any blocked field is non-zero, treat the run as failed and do not use the
result as query/info boundary evidence.

## Forbidden Actions

Do not use this wrapper to run or justify:

- real Steam Half-Life clients
- real client modes
- real query client binaries
- Steam launch or Steam authentication
- public socket exposure
- LAN socket exposure
- non-loopback targets
- connect path checks
- post-connect serverinfo checks
- signon serverinfo checks
- auth, netchan, reliable channel, resources, baselines, or client admission

The wrapper intentionally hard-fails for the unsafe switches `-Public`, `-LAN`,
`-RealClient`, `-Connect`, `-PostConnect`, and `-Signon`.

## Troubleshooting

If `hlhost.exe` is missing, run a build separately or use:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/run_hlds_query_info_regression.ps1 -Mode all -Build
```

If `-Mode acceptance -RequireDriftGate` fails with `drift_gate_required`, use
`-Mode all` or remove `-RequireDriftGate` only for an intentionally
acceptance-only local check.

If the wrapper reports fixture drift, inspect:

```text
fixtures/diagnostic/hlds/query_info_regression/query_info_regression_ci_manifest.json
fixtures/diagnostic/hlds/serverinfo/
```

Do not update the manifest or fixtures to make a drift failure pass unless the
change is covered by a separate prompt that reviews the evidence and policy
boundary.

If a public, LAN, real-client, connect, post-connect, or signon switch is
rejected, the wrapper is behaving as designed. Use a separate policy prompt
before changing those blockers.
