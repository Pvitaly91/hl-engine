# Qport/session dry-run validator proof comparison

| Scenario | Accepted | Rejected | Validated | Schema | Safe actions | Reject reason | Proof |
|---|---:|---:|---:|---:|---:|---|---|
| happy | 1 | 0 | 1 | 1 | 1 | <none> | pass |
| gate_disabled_by_default | 0 | 1 | 0 | 0 | 0 | qport_session_dry_run_validator_disabled | pass |
| gate_missing_dry_run_manifest | 0 | 1 | 0 | 0 | 0 | qport_session_dry_run_manifest_missing | pass |
| gate_missing_policy_reference | 0 | 1 | 0 | 1 | 1 | qport_session_policy_reference_missing | pass |
| gate_validator_not_required | 0 | 1 | 0 | 1 | 1 | offline_fixture_validator_required | pass |
| gate_policy_gate_not_required | 0 | 1 | 0 | 1 | 1 | capture_policy_gate_required | pass |
| gate_capture_allowed_claim | 0 | 1 | 0 | 1 | 1 | qport_session_capture_not_allowed_now | pass |
| gate_capture_executed_claim | 0 | 1 | 0 | 1 | 1 | qport_session_capture_execution_not_allowed | pass |
| gate_socket_action_in_plan | 0 | 1 | 0 | 1 | 0 | socket_action_not_allowed_in_dry_run_plan | pass |
| gate_real_client_action_in_plan | 0 | 1 | 0 | 1 | 0 | real_client_action_not_allowed_in_dry_run_plan | pass |
| gate_connect_or_signon_action_in_plan | 0 | 1 | 0 | 1 | 0 | connect_or_signon_action_not_allowed_in_dry_run_plan | pass |
| gate_missing_planned_artifact_schema | 0 | 1 | 0 | 0 | 1 | missing_planned_capture_artifact_schema_field | pass |
| gate_qport_byte_evidence_claim | 0 | 1 | 0 | 1 | 1 | qport_byte_evidence_claim_rejected | pass |
| gate_address_scoped_challenge_real_netchan_claim | 0 | 1 | 0 | 1 | 1 | address_scoped_challenge_not_real_netchan_proof | pass |
| gate_no_real_client_used | 1 | 0 | 1 | 1 | 1 | <none> | pass |
| gate_public_socket_blocked | 0 | 1 | 0 | 0 | 0 | public_socket_blocked | pass |
