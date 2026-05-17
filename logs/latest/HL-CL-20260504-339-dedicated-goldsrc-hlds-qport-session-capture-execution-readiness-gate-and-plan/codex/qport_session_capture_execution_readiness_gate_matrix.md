# Qport/session Capture Execution Readiness Gate Matrix

| Scenario | Expected acceptance | Required decision |
| --- | --- | --- |
| `happy` | accepted=1 rejected=0 | accept only if safety proof passes |
| `gate_disabled_by_default` | accepted=0 rejected=1 | reject and keep readiness plan uncreated or invalid |
| `gate_readiness_policy_required` | accepted=0 rejected=1 | reject and keep readiness plan uncreated or invalid |
| `gate_execution_implementation_ci_required` | accepted=0 rejected=1 | reject and keep readiness plan uncreated or invalid |
| `gate_execution_implementation_boundary_required` | accepted=0 rejected=1 | reject and keep readiness plan uncreated or invalid |
| `gate_final_gate_required` | accepted=0 rejected=1 | reject and keep readiness plan uncreated or invalid |
| `gate_execution_skeleton_ci_required` | accepted=0 rejected=1 | reject and keep readiness plan uncreated or invalid |
| `gate_runtime_skeleton_ci_required` | accepted=0 rejected=1 | reject and keep readiness plan uncreated or invalid |
| `gate_offline_validator_required` | accepted=0 rejected=1 | reject and keep readiness plan uncreated or invalid |
| `gate_capture_policy_gate_required` | accepted=0 rejected=1 | reject and keep readiness plan uncreated or invalid |
| `gate_dry_run_validator_required` | accepted=0 rejected=1 | reject and keep readiness plan uncreated or invalid |
| `gate_wrapper_validation_required` | accepted=0 rejected=1 | reject and keep readiness plan uncreated or invalid |
| `gate_timeout_cleanup_policy_required` | accepted=0 rejected=1 | reject and keep readiness plan uncreated or invalid |
| `gate_artifact_schema_lock_required` | accepted=0 rejected=1 | reject and keep readiness plan uncreated or invalid |
| `gate_socket_policy_review_required` | accepted=0 rejected=1 | reject and keep readiness plan uncreated or invalid |
| `gate_datagram_policy_review_required` | accepted=0 rejected=1 | reject and keep readiness plan uncreated or invalid |
| `gate_capture_execution_blocked` | accepted=0 rejected=1 | reject and keep readiness plan uncreated or invalid |
| `gate_capture_runtime_blocked` | accepted=0 rejected=1 | reject and keep readiness plan uncreated or invalid |
| `gate_socket_open_blocked` | accepted=0 rejected=1 | reject and keep readiness plan uncreated or invalid |
| `gate_loopback_socket_blocked` | accepted=0 rejected=1 | reject and keep readiness plan uncreated or invalid |
| `gate_public_lan_blocked` | accepted=0 rejected=1 | reject and keep readiness plan uncreated or invalid |
| `gate_datagram_send_blocked` | accepted=0 rejected=1 | reject and keep readiness plan uncreated or invalid |
| `gate_datagram_receive_blocked` | accepted=0 rejected=1 | reject and keep readiness plan uncreated or invalid |
| `gate_real_client_blocked` | accepted=0 rejected=1 | reject and keep readiness plan uncreated or invalid |
| `gate_connect_signon_blocked` | accepted=0 rejected=1 | reject and keep readiness plan uncreated or invalid |
| `gate_netchan_runtime_blocked` | accepted=0 rejected=1 | reject and keep readiness plan uncreated or invalid |
| `gate_qport_evidence_promotion_blocked` | accepted=0 rejected=1 | reject and keep readiness plan uncreated or invalid |
| `gate_compatibility_claim_blocked` | accepted=0 rejected=1 | reject and keep readiness plan uncreated or invalid |
| `gate_no_real_client_used` | accepted=1 rejected=0 | accept only if safety proof passes |
| `gate_public_socket_blocked` | accepted=0 rejected=1 | reject and keep readiness plan uncreated or invalid |
