# Qport/Session Capture Execution Readiness CI Manifest Report

Prompt: HL-CL-20260504-341-dedicated-goldsrc-hlds-qport-session-capture-execution-readiness-ci-manifest-and-drift-gate

Compatibility claim level: diagnostic-qport-session-capture-execution-readiness-ci-manifest-drift-gate-only; no real Steam Half-Life or HLDS-compatible client compatibility claimed.

## Manifest

- Manifest created: 1
- Manifest loaded in happy proof: 1
- Manifest path: `fixtures/diagnostic/hlds/qport_session/qport_session_capture_execution_readiness_ci_manifest.json`
- Covered prompt start: 304
- Covered prompt end: 340
- Focused readiness prompt: 339
- Readiness gate source commit: `e22b56166a3b2ea161a9776602b96513827f878c`
- Readiness gate artifact commit: `6c5fb6502b2bc176f3a982948a6959ae34ea27c5`
- Readiness release boundary source commit: `4c9419037978724ca888b687bf15707f4eecc258`
- Readiness policy review source commit: `2443255002e9bb43c20e0846296c2194d7ad2630`
- Execution implementation CI source commit: `b6681508343ad915e4d779737bec45df44128761`

## Checked Inputs

- Fixture root: `fixtures/diagnostic/hlds/qport_session`
- Policy file: `fixtures/diagnostic/hlds/qport_session/qport_session_offline_fixture_manifest_policy.json`
- Dry-run manifest: `fixtures/diagnostic/hlds/qport_session/qport_session_capture_preflight_dry_run_manifest.json`
- Shell CI manifest: `fixtures/diagnostic/hlds/qport_session/qport_session_capture_shell_ci_manifest.json`
- Runtime skeleton CI manifest: `fixtures/diagnostic/hlds/qport_session/qport_session_capture_runtime_skeleton_ci_manifest.json`
- Execution skeleton CI manifest: `fixtures/diagnostic/hlds/qport_session/qport_session_capture_execution_skeleton_ci_manifest.json`
- Execution implementation skeleton CI manifest: `fixtures/diagnostic/hlds/qport_session/qport_session_capture_execution_implementation_skeleton_ci_manifest.json`
- Execution implementation CI manifest: `fixtures/diagnostic/hlds/qport_session/qport_session_capture_execution_implementation_ci_manifest.json`
- Wrapper script: `scripts/run_hlds_qport_session_capture_dry_run.ps1`
- Readiness release doc: `docs/diagnostic/hlds/qport_session_capture_execution_readiness_release_boundary_summary.md`
- Timeout cleanup policy sketch: `logs/latest/HL-CL-20260504-339-dedicated-goldsrc-hlds-qport-session-capture-execution-readiness-gate-and-plan/codex/qport_session_timeout_cleanup_policy_sketch.md`
- Artifact schema lock sketch: `logs/latest/HL-CL-20260504-339-dedicated-goldsrc-hlds-qport-session-capture-execution-readiness-gate-and-plan/codex/qport_session_artifact_schema_lock_sketch.md`

## Happy Result

- readiness_ci_manifest_created=1
- readiness_ci_manifest_loaded=1
- fixture_files_checked=5
- fixture_hashes_recorded=1
- policy_hash_recorded=1
- dry_run_manifest_hash_recorded=1
- execution_implementation_ci_manifest_hash_recorded=1
- wrapper_hash_recorded=1
- readiness_drift_detected=0
- readiness_policy_drift_detected=0
- execution_implementation_ci_manifest_drift_detected=0
- timeout_cleanup_policy_drift_detected=0
- artifact_schema_lock_drift_detected=0
- socket_policy_review_drift_detected=0
- datagram_policy_review_drift_detected=0
- dependency_drift_detected=0
