# Query/Info Regression Quickstart

Use this page when you only need the commands and the fields to check.

Wrapper:

```powershell
scripts/run_hlds_query_info_regression.ps1
```

## Commands

Dry run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/run_hlds_query_info_regression.ps1 -Mode all -DryRun -NoBuild
```

Full query/info regression:

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

## Logs

Default wrapper output:

```text
logs/latest/HL-CL-20260504-294-dedicated-goldsrc-hlds-query-info-regression-rerun-command-wrapper/wrapper
```

Primary files:

- `query_info_rerun_command_plan.json`
- `query_info_rerun_command_log.txt`
- `query_info_rerun_wrapper_summary.json`

## Required Summary Fields

For any successful run, check:

- `wrapper_diagnostic_only=1`
- `selected_fixture_id=connectionless_query_info_candidate`
- `selected_fixture_stage=connectionless_query`
- `public_socket_opened=0`
- `lan_socket_opened=0`
- `real_client_binary_invoked=0`
- `connect_path_invoked=0`
- `post_connect_serverinfo_path_invoked=0`
- `signon_serverinfo_path_invoked=0`
- `normal_host_behavior_changed=0`

For `-Mode all`, also check:

- `acceptance_gate_included=1`
- `drift_gate_included=1`
- `wrapper_full_run_passed=1` for a full run
- `wrapper_dry_run_passed=1` for a dry run

## Boundary Reminder

This is diagnostic connectionless query/info only. It is not post-connect
serverinfo, not signon-time serverinfo, and not real Steam Half-Life or
HLDS-compatible client compatibility evidence. It does not run a real client.

Do not add `-Public`, `-LAN`, `-RealClient`, `-Connect`, `-PostConnect`, or
`-Signon`.
