# Qport/session offline fixture validator proof comparison

| Scenario | Proof | Accepted | Rejected | Reject reason | Fixture validation | No real client | No public socket | No LAN socket | No netchan runtime |
| --- | --- | ---: | ---: | --- | ---: | ---: | ---: | ---: | ---: |
| happy | pass | 1 | 0 | <none> | 1 | True | True | True | True |
| gate_disabled_by_default | pass | 0 | 1 | qport_session_offline_fixture_validator_disabled | 0 | True | True | True | True |
| gate_missing_policy_file | pass | 0 | 1 | qport_session_policy_file_missing | 0 | True | True | True | True |
| gate_missing_required_metadata | pass | 0 | 1 | missing_required_qport_fixture_metadata | 0 | True | True | True | True |
| gate_real_client_claim | pass | 0 | 1 | qport_fixture_real_client_claim_rejected | 0 | True | True | True | True |
| gate_public_socket_claim | pass | 0 | 1 | qport_fixture_public_socket_claim_rejected | 0 | True | True | True | True |
| gate_lan_socket_claim | pass | 0 | 1 | qport_fixture_lan_socket_claim_rejected | 0 | True | True | True | True |
| gate_capture_executed_claim | pass | 0 | 1 | qport_fixture_capture_executed_claim_rejected | 0 | True | True | True | True |
| gate_qport_promoted_without_byte_evidence | pass | 0 | 1 | qport_promoted_without_byte_evidence | 0 | True | True | True | True |
| gate_address_scoped_challenge_promoted_to_real_netchan | pass | 0 | 1 | address_scoped_challenge_not_real_netchan_proof | 0 | True | True | True | True |
| gate_invalid_fixture_missing_reject_reason | pass | 0 | 1 | invalid_qport_fixture_missing_reject_reason | 0 | True | True | True | True |
| gate_no_real_client_used | pass | 1 | 0 | <none> | 1 | True | True | True | True |
| gate_public_socket_blocked | pass | 0 | 1 | public_socket_blocked | 0 | True | True | True | True |
