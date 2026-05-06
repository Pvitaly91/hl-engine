# Query/Info Regression Rerun Wrapper

This wrapper reruns only the diagnostic connectionless query/info regression
boundary. It does not run a real Steam Half-Life client, does not invoke real
client binaries, and does not enable public, LAN, connect, post-connect, signon,
auth, netchan, resource, baseline, or admission paths.

Script:

```powershell
scripts/run_hlds_query_info_regression.ps1
```

Default behavior is diagnostic-only. The wrapper emits a command plan, command
log, and machine-readable summary under its output directory.

Operator docs:

- `docs/diagnostic/hlds/query_info_regression_operator_checklist.md`
- `docs/diagnostic/hlds/query_info_regression_quickstart.md`

## Common Usage

Generate a command plan without running hlhost:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/run_hlds_query_info_regression.ps1 -Mode all -DryRun -NoBuild
```

Run the prompt 292 query/info regression acceptance and prompt 293 fixture drift
gate using an existing Debug hlhost build:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/run_hlds_query_info_regression.ps1 -Mode all -NoBuild
```

Run only the acceptance gate:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/run_hlds_query_info_regression.ps1 -Mode acceptance -NoBuild
```

Run only the drift gate:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/run_hlds_query_info_regression.ps1 -Mode drift -NoBuild
```

Build before running when local MSBuild is available:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/run_hlds_query_info_regression.ps1 -Mode all -Build
```

## Policy Blocks

The wrapper hard-fails before command execution if any unsafe mode is requested:

- `-Public`
- `-LAN`
- `-RealClient`
- `-Connect`
- `-PostConnect`
- `-Signon`

`-RequireDriftGate` may be used by automation to reject an acceptance-only plan
that omits the fixture drift gate.

## Expected Boundary

The selected fixture remains:

- `selected_fixture_id=connectionless_query_info_candidate`
- `selected_fixture_stage=connectionless_query`

Expected blocked behaviors remain:

- `public_socket_opened=0`
- `lan_socket_opened=0`
- `real_client_binary_invoked=0`
- `connect_path_invoked=0`
- `post_connect_serverinfo_path_invoked=0`
- `signon_serverinfo_path_invoked=0`
- `normal_host_behavior_changed=0`

This wrapper preserves the compatibility claim level:

```text
diagnostic-query-info-rerun-wrapper-only; no real Steam Half-Life or HLDS-compatible client compatibility claimed
```
