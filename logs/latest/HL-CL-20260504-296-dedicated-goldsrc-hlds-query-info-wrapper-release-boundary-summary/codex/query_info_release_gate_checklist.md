# Query/Info Release Gate Checklist

## Before Running

- Confirm the command uses only `scripts/run_hlds_query_info_regression.ps1`.
- Confirm the command does not include `-Public`, `-LAN`, `-RealClient`, `-Connect`, `-PostConnect`, or `-Signon`.
- Confirm the mode is `all`, `acceptance`, or `drift`.
- Run a dry-run first when changing output paths or build settings.

## Commands

Dry-run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/run_hlds_query_info_regression.ps1 -Mode all -DryRun -NoBuild
```

Full boundary:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/run_hlds_query_info_regression.ps1 -Mode all -NoBuild
```

## Wrapper Summary Fields

- `wrapper_diagnostic_only=1`
- `wrapper_full_run_passed=1` for a full run
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

## Acceptance Summary Fields

- `query_info_regression_acceptance_passed=1`
- `query_info_builder_parser_passed=1`
- `query_info_path_integration_passed=1`
- `query_info_loopback_swap_passed=1`
- `query_client_smoke_passed=1`
- `client_query_info_response_received=1`
- `client_query_info_response_shape_valid=1`
- blocked public/LAN/real-client/connect/post-connect/signon markers remain `0`

## Drift Summary Fields

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

Pass means both acceptance and drift pass with all blocked markers still zero. Fail means either gate fails, fixture/compatibility drift is detected, or any forbidden marker becomes non-zero.