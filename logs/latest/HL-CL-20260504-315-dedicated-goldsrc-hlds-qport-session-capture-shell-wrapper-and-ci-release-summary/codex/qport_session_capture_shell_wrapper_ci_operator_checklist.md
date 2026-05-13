# HL-CL-20260504-315 Operator Checklist

Compatibility claim level: diagnostic-qport-session-capture-shell-wrapper-ci-release-summary-only; no real Steam Half-Life or HLDS-compatible client compatibility claimed

## Inspect CI Manifest

- Open ixtures/diagnostic/hlds/qport_session/qport_session_capture_shell_ci_manifest.json.
- Confirm covered_prompt_start=304 and covered_prompt_end=313 in the manifest.
- Confirm fixture, policy, dry-run manifest, wrapper, and docs hashes are present.

## Run Drift Gate

Use the checked-in, disabled-by-default drift gate with the happy scenario only for release-boundary verification:

`powershell
.\build32\host\Debug\hlhost.exe 
  --dedicated 
  --gamedir logs\latest\HL-CL-20260409-119-dedicated-goldsrc-signon-envelope-surface\runtime\valve-fixture 
  --maxclients 4 
  --frames 1 
  --log-summary-file 1 
  --log-console-level error 
  --prompt-id HL-CL-20260504-315-dedicated-goldsrc-hlds-qport-session-capture-shell-wrapper-and-ci-release-summary 
  --run-label p315-drift-happy 
  --log-dir logs\latest\runtime\p315-drift-gate-happy 
  --hlds-qport-session-capture-shell-ci-drift-gate-probe 
  --hlds-qport-session-capture-shell-ci-drift-gate-probe-scenario happy
`

## Run Wrapper Plan Mode

`powershell
.\scripts\run_hlds_qport_session_capture_dry_run.ps1 -Mode plan -OutDir logs\latest\HL-CL-20260504-315-dedicated-goldsrc-hlds-qport-session-capture-shell-wrapper-and-ci-release-summary\wrapper_plan -RunLabelPrefix p315-plan -Strict
`

## Run Wrapper Validate Mode

`powershell
.\scripts\run_hlds_qport_session_capture_dry_run.ps1 -Mode validate -NoBuild -OutDir logs\latest\HL-CL-20260504-315-dedicated-goldsrc-hlds-qport-session-capture-shell-wrapper-and-ci-release-summary\wrapper_validate -RunLabelPrefix p315-validate -Strict
`

## Required Ones

- capture_shell_added=1
- shell_plan_created=1
- shell_plan_validated=1
- ci_manifest_created=1
- drift_gate_passed=1
- capture_blocked_by_policy=1

## Required Zeroes

- ixture_drift_detected=0
- policy_drift_detected=0
- dry_run_manifest_drift_detected=0
- wrapper_drift_detected=0
- shell_dependency_drift_detected=0
- capture_allowed_now=0
- capture_executed=0
- capture_runtime_executed=0
- socket_open_attempted=0
- public_socket_opened=0
- lan_socket_opened=0
- loopback_udp_socket_opened=0
- eal_client_binary_invoked=0
- connect_path_invoked=0
- post_connect_serverinfo_path_invoked=0
- signon_serverinfo_path_invoked=0
- 
etchan_runtime_started=0
- compatibility_claim_expanded=0

If any required zero changes, stop and treat the boundary as failed. Do not continue to capture implementation.
