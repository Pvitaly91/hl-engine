# Query/Info Regression Rerun Wrapper Report

Prompt: HL-CL-20260504-294-dedicated-goldsrc-hlds-query-info-regression-rerun-command-wrapper

Compatibility claim level: diagnostic-query-info-rerun-wrapper-only; no real Steam Half-Life or HLDS-compatible client compatibility claimed

## Result

Added a small PowerShell wrapper and documentation for rerunning the diagnostic query/info regression boundary.

- wrapper_script_file: scripts/run_hlds_query_info_regression.ps1
- wrapper_documentation_file: docs/diagnostic/hlds/query_info_regression_rerun_wrapper.md
- wrapper_diagnostic_only: 1
- wrapper_dry_run_supported: 1
- wrapper_dry_run_passed: 1
- wrapper_full_run_executed: 1
- wrapper_full_run_passed: 1

## Included Gates

- acceptance_gate_included: 1
- drift_gate_included: 1
- selected_fixture_id: connectionless_query_info_candidate
- selected_fixture_stage: connectionless_query

## Proofs

| Proof | Result |
|---|---:|
| happy | pass |
| public socket blocked | pass |
| LAN socket blocked | pass |
| real client blocked | pass |
| connect/post-connect/signon mode blocked | pass |
| drift gate required | pass |

## Full Run Steps

- acceptance exit_code: 0; accepted: 1; rejected: 0; passed: True
- drift exit_code: 0; accepted: 1; rejected: 0; passed: True

## Boundary Preservation

- public_socket_opened: 0
- lan_socket_opened: 0
- real_client_binary_invoked: 0
- connect_path_invoked: 0
- post_connect_serverinfo_path_invoked: 0
- signon_serverinfo_path_invoked: 0
- normal_host_behavior_changed: 0

The wrapper does not add host launch options and does not change normal host behavior. Unsafe public/LAN/real-client/connect/post-connect/signon modes are rejected before command execution.
