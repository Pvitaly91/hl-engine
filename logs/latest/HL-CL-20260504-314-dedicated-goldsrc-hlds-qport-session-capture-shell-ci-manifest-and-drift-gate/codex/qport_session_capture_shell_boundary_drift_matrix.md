# Qport/session Capture Shell CI Manifest And Drift Gate

Prompt: `HL-CL-20260504-314-dedicated-goldsrc-hlds-qport-session-capture-shell-ci-manifest-and-drift-gate`

Compatibility claim level: `diagnostic-qport-session-capture-shell-ci-manifest-drift-gate-only; no real Steam Half-Life or HLDS-compatible client compatibility claimed`

| Scenario | Proof | Accepted | Rejected | Reject reason | Drift gate passed |
|---|---:|---:|---:|---|---:|
| happy | pass | 1 | 0 | <none> | 1 |
| gate_disabled_by_default | pass | 0 | 1 | qport_session_capture_shell_drift_gate_disabled | 0 |
| gate_missing_ci_manifest | pass | 0 | 1 | qport_session_capture_shell_ci_manifest_missing | 0 |
| gate_fixture_drift | pass | 0 | 1 | qport_session_fixture_drift_detected | 0 |
| gate_policy_drift | pass | 0 | 1 | qport_session_policy_drift_detected | 0 |
| gate_dry_run_manifest_drift | pass | 0 | 1 | qport_session_dry_run_manifest_drift_detected | 0 |
| gate_wrapper_drift | pass | 0 | 1 | qport_session_wrapper_drift_detected | 0 |
| gate_shell_dependency_removed | pass | 0 | 1 | qport_session_shell_dependency_removed | 0 |
| gate_capture_allowed_now_drift | pass | 0 | 1 | qport_session_capture_allowed_now_drift | 0 |
| gate_capture_executed_drift | pass | 0 | 1 | qport_session_capture_executed_drift | 0 |
| gate_socket_allowed_drift | pass | 0 | 1 | qport_session_socket_allowed_drift | 0 |
| gate_real_client_allowed_drift | pass | 0 | 1 | qport_session_real_client_allowed_drift | 0 |
| gate_connect_signon_allowed_drift | pass | 0 | 1 | qport_session_connect_signon_allowed_drift | 0 |
| gate_qport_evidence_promoted_drift | pass | 0 | 1 | qport_session_byte_evidence_promoted_drift | 0 |
| gate_address_scoped_challenge_real_netchan_drift | pass | 0 | 1 | address_scoped_challenge_real_netchan_drift | 0 |
| gate_compatibility_claim_expanded | pass | 0 | 1 | qport_session_compatibility_claim_expanded | 0 |
| gate_no_real_client_used | pass | 1 | 0 | <none> | 1 |
| gate_public_socket_blocked | pass | 0 | 1 | public_socket_blocked | 0 |
