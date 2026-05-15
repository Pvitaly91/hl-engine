# qport/session capture final policy decision matrix

Prompt: $promptId

Compatibility claim level: $compat

| Scenario | Proof passed | Accepted | Rejected | Decision/reason |
| --- | ---: | ---: | ---: | --- |
| happy | 1 | 1 | 0 | <none> |
| gate_disabled_by_default | 1 | 0 | 1 | qport_session_final_policy_gate_disabled |
| gate_ci_drift_gate_required | 1 | 0 | 1 | qport_session_ci_drift_gate_required |
| gate_offline_validator_required | 1 | 0 | 1 | offline_fixture_validator_required |
| gate_capture_policy_gate_required | 1 | 0 | 1 | capture_policy_gate_required |
| gate_dry_run_validator_required | 1 | 0 | 1 | dry_run_validator_required |
| gate_wrapper_validation_required | 1 | 0 | 1 | wrapper_validation_required |
| gate_shell_boundary_required | 1 | 0 | 1 | capture_shell_boundary_required |
| gate_capture_runtime_requested_blocked | 1 | 0 | 1 | capture_runtime_not_allowed_now |
| gate_socket_open_requested_blocked | 1 | 0 | 1 | socket_open_not_allowed_now |
| gate_public_lan_requested_blocked | 1 | 0 | 1 | public_lan_not_allowed_now |
| gate_real_client_requested_blocked | 1 | 0 | 1 | real_client_not_allowed_now |
| gate_connect_signon_requested_blocked | 1 | 0 | 1 | connect_postconnect_signon_not_allowed_now |
| gate_netchan_runtime_requested_blocked | 1 | 0 | 1 | netchan_runtime_not_allowed_now |
| gate_compatibility_claim_requested_blocked | 1 | 0 | 1 | compatibility_claim_expansion_not_allowed_now |
| gate_qport_evidence_promotion_requested_blocked | 1 | 0 | 1 | qport_evidence_promotion_not_allowed_now |
| gate_no_real_client_used | 1 | 1 | 0 | <none> |
| gate_public_socket_blocked | 1 | 0 | 1 | public_socket_blocked |

## Final decision

The final policy gate passes only for happy and gate_no_real_client_used. The happy decision sets capture_implementation_allowed_next=1 solely for minimal_disabled_by_default_no_client_diagnostic_shell_or_runtime_skeleton.

All runtime authority remains denied in this prompt: capture_runtime_allowed_now=0, socket_open_allowed_now=0, public_socket_allowed_now=0, lan_socket_allowed_now=0, eal_client_allowed_now=0, and compatibility_claim_expansion_allowed_now=0.
