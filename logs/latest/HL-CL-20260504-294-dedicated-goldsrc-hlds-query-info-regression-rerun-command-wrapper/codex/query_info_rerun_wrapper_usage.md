# Query/Info Regression Rerun Wrapper Usage
Prompt: HL-CL-20260504-294-dedicated-goldsrc-hlds-query-info-regression-rerun-command-wrapper
Compatibility claim level:
`	ext
diagnostic-query-info-rerun-wrapper-only; no real Steam Half-Life or HLDS-compatible client compatibility claimed
`
Script:
`powershell
scripts/run_hlds_query_info_regression.ps1
`
The wrapper is diagnostic-only and runs only the query/info regression acceptance gate from prompt 292 and the CI manifest/fixture drift gate from prompt 293. It hard-fails before command execution for -Public, -LAN, -RealClient, -Connect, -PostConnect, or -Signon.
## Dry Run
`powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/run_hlds_query_info_regression.ps1 -Mode all -DryRun -NoBuild
`
Dry-run result for this prompt: pass. It generated a command plan without launching real clients, without opening public/LAN sockets, and without invoking connect/post-connect/signon paths.
## Full Diagnostic Run
`powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/run_hlds_query_info_regression.ps1 -Mode all -NoBuild
`
Full-run result for this prompt: pass. It ran the prompt 292 acceptance happy scenario and prompt 293 drift-gate happy scenario through the existing in-repo diagnostic surfaces.
## Narrow Modes
`powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/run_hlds_query_info_regression.ps1 -Mode acceptance -NoBuild
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/run_hlds_query_info_regression.ps1 -Mode drift -NoBuild
`
Automation that requires the complete boundary can add -RequireDriftGate; the wrapper rejects acceptance-only plans when that guard is set.
## Expected Boundary
- selected_fixture_id: connectionless_query_info_candidate
- selected_fixture_stage: connectionless_query
- acceptance_gate_included: 1
- drift_gate_included: 1
- public_socket_opened: 0
- lan_socket_opened: 0
- real_client_binary_invoked: 0
- connect_path_invoked: 0
- post_connect_serverinfo_path_invoked: 0
- signon_serverinfo_path_invoked: 0
- normal_host_behavior_changed: 0