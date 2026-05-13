# Qport/session Capture Shell CI Manifest And Drift Gate

Prompt: `HL-CL-20260504-314-dedicated-goldsrc-hlds-qport-session-capture-shell-ci-manifest-and-drift-gate`

Compatibility claim level: `diagnostic-qport-session-capture-shell-ci-manifest-drift-gate-only; no real Steam Half-Life or HLDS-compatible client compatibility claimed`

## Runtime Proof

All 18 required drift-gate scenarios passed. Happy path loaded the CI manifest and preserved blocked capture, socket, runtime, netchan, public/LAN, real-client, and compatibility-expansion fields.

## Happy Markers

- drift_gate_passed=1
- fixture_drift_detected=0
- policy_drift_detected=0
- dry_run_manifest_drift_detected=0
- wrapper_drift_detected=0
- shell_dependency_drift_detected=0
- capture_allowed_now=0
- capture_executed=0
- socket_open_attempted=0
- real_client_binary_invoked=0
