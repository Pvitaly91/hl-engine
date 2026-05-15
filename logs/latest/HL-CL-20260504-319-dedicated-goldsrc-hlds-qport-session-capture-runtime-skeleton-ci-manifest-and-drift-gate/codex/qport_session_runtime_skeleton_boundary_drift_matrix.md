# qport/session Runtime Skeleton Boundary Drift Matrix

| Scenario | Proof | Reject reason / note |
| --- | --- | --- |
| happy | pass | <none> |
| gate_disabled_by_default | pass | qport_session_runtime_skeleton_drift_gate_disabled |
| gate_missing_ci_manifest | pass | qport_session_runtime_skeleton_ci_manifest_missing |
| gate_runtime_skeleton_drift | pass | qport_session_runtime_skeleton_drift_detected |
| gate_policy_drift | pass | qport_session_policy_drift_detected |
| gate_dry_run_manifest_drift | pass | qport_session_dry_run_manifest_drift_detected |
| gate_shell_ci_manifest_drift | pass | qport_session_shell_ci_manifest_drift_detected |
| gate_wrapper_drift | pass | qport_session_wrapper_drift_detected |
| gate_dependency_removed | pass | qport_session_runtime_skeleton_dependency_removed |
| gate_capture_allowed_now_drift | pass | qport_session_capture_allowed_now_drift |
| gate_capture_executed_drift | pass | qport_session_capture_executed_drift |
| gate_capture_runtime_executed_drift | pass | qport_session_capture_runtime_executed_drift |
| gate_datagram_drift | pass | qport_session_datagram_drift_detected |
| gate_socket_allowed_drift | pass | qport_session_socket_allowed_drift |
| gate_real_client_allowed_drift | pass | qport_session_real_client_allowed_drift |
| gate_connect_signon_allowed_drift | pass | qport_session_connect_signon_allowed_drift |
| gate_netchan_runtime_drift | pass | qport_session_netchan_runtime_drift |
| gate_qport_evidence_promoted_drift | pass | qport_session_byte_evidence_promoted_drift |
| gate_address_scoped_challenge_real_netchan_drift | pass | address_scoped_challenge_real_netchan_drift |
| gate_compatibility_claim_expanded | pass | qport_session_compatibility_claim_expanded |
| gate_no_real_client_used | pass | <none> |
| gate_public_socket_blocked | pass | public_socket_blocked |
