# Qport/Session Capture Execution Readiness Boundary Drift Matrix

Prompt: HL-CL-20260504-341-dedicated-goldsrc-hlds-qport-session-capture-execution-readiness-ci-manifest-and-drift-gate

| Scenario | Expected | Result | Reject reason |
| --- | --- | --- | --- |
| happy | accept | pass | `<none>` |
| gate_disabled_by_default | reject | pass | qport_session_capture_execution_readiness_ci_drift_gate_disabled |
| gate_missing_ci_manifest | reject | pass | qport_session_capture_execution_readiness_ci_manifest_missing |
| gate_readiness_drift | reject | pass | qport_session_capture_execution_readiness_drift_detected |
| gate_readiness_policy_drift | reject | pass | qport_session_capture_execution_readiness_policy_drift_detected |
| gate_execution_implementation_ci_manifest_drift | reject | pass | qport_session_capture_execution_implementation_ci_manifest_drift_detected |
| gate_timeout_cleanup_policy_drift | reject | pass | qport_session_capture_execution_timeout_cleanup_policy_drift_detected |
| gate_artifact_schema_lock_drift | reject | pass | qport_session_capture_execution_artifact_schema_lock_drift_detected |
| gate_socket_policy_review_drift | reject | pass | qport_session_capture_execution_socket_policy_review_drift_detected |
| gate_datagram_policy_review_drift | reject | pass | qport_session_capture_execution_datagram_policy_review_drift_detected |
| gate_wrapper_drift | reject | pass | qport_session_capture_wrapper_drift_detected |
| gate_dependency_removed | reject | pass | qport_session_capture_execution_readiness_dependency_removed |
| gate_capture_execution_allowed_now_drift | reject | pass | capture_execution_allowed_now_drift_detected |
| gate_capture_runtime_allowed_now_drift | reject | pass | capture_runtime_allowed_now_drift_detected |
| gate_socket_allowed_drift | reject | pass | socket_open_allowed_now_drift_detected |
| gate_loopback_socket_allowed_drift | reject | pass | loopback_socket_allowed_now_drift_detected |
| gate_datagram_allowed_drift | reject | pass | datagram_allowed_now_drift_detected |
| gate_capture_executed_drift | reject | pass | capture_executed_drift_detected |
| gate_datagram_executed_drift | reject | pass | datagram_executed_drift_detected |
| gate_real_client_allowed_drift | reject | pass | real_client_allowed_now_drift_detected |
| gate_connect_signon_allowed_drift | reject | pass | connect_signon_allowed_now_drift_detected |
| gate_netchan_runtime_drift | reject | pass | netchan_runtime_drift_detected |
| gate_qport_evidence_promoted_drift | reject | pass | qport_evidence_promotion_drift_detected |
| gate_address_scoped_challenge_real_netchan_drift | reject | pass | address_scoped_challenge_real_netchan_drift_detected |
| gate_compatibility_claim_expanded | reject | pass | compatibility_claim_expansion_drift_detected |
| gate_no_real_client_used | accept | pass | `<none>` |
| gate_public_socket_blocked | reject | pass | public_socket_blocked |
