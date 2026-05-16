# qport/session capture execution final policy decision matrix

| Scenario | Accepted | Rejected | Proof | Last reject reason |
|---|---:|---:|---|---|
| happy | 1 | 0 | pass | <none> |
| gate_disabled_by_default | 0 | 1 | pass | qport_session_capture_execution_final_policy_gate_disabled |
| gate_runtime_skeleton_ci_required | 0 | 1 | pass | runtime_skeleton_ci_drift_gate_required |
| gate_runtime_skeleton_boundary_required | 0 | 1 | pass | runtime_skeleton_boundary_required |
| gate_final_policy_gate_required | 0 | 1 | pass | final_policy_gate_required |
| gate_offline_validator_required | 0 | 1 | pass | offline_fixture_validator_required |
| gate_capture_policy_gate_required | 0 | 1 | pass | capture_policy_gate_required |
| gate_dry_run_validator_required | 0 | 1 | pass | dry_run_validator_required |
| gate_wrapper_validation_required | 0 | 1 | pass | wrapper_validation_required |
| gate_execution_requested_blocked | 0 | 1 | pass | capture_execution_not_allowed_now |
| gate_capture_runtime_requested_blocked | 0 | 1 | pass | capture_runtime_not_allowed_now |
| gate_socket_open_requested_blocked | 0 | 1 | pass | socket_open_not_allowed_now |
| gate_loopback_socket_requested_blocked | 0 | 1 | pass | loopback_socket_not_allowed_now |
| gate_public_lan_requested_blocked | 0 | 1 | pass | public_lan_not_allowed_now |
| gate_datagram_send_requested_blocked | 0 | 1 | pass | datagram_send_not_allowed_now |
| gate_datagram_receive_requested_blocked | 0 | 1 | pass | datagram_receive_not_allowed_now |
| gate_real_client_requested_blocked | 0 | 1 | pass | real_client_not_allowed_now |
| gate_connect_signon_requested_blocked | 0 | 1 | pass | connect_postconnect_signon_not_allowed_now |
| gate_netchan_runtime_requested_blocked | 0 | 1 | pass | netchan_runtime_not_allowed_now |
| gate_qport_evidence_promotion_requested_blocked | 0 | 1 | pass | qport_evidence_promotion_not_allowed_now |
| gate_compatibility_claim_requested_blocked | 0 | 1 | pass | compatibility_claim_expansion_not_allowed_now |
| gate_no_real_client_used | 1 | 0 | pass | <none> |
| gate_public_socket_blocked | 0 | 1 | pass | public_socket_blocked |
