# Query/Info Fixture Drift Gate Report

prompt_id: HL-CL-20260504-293-dedicated-goldsrc-hlds-query-info-regression-ci-manifest-and-fixture-drift-gate
compatibility_claim_level: diagnostic-query-info-ci-manifest-fixture-drift-gate-only; no real Steam Half-Life or HLDS-compatible client compatibility claimed
source_commit: 17839836b97dea9c75050deb61a869fa60750bf2

## Happy Result
- ci_manifest_loaded=1
- drift_gate_passed=1
- fixture_files_checked=12
- fixture_hashes_recorded=1
- fixture_drift_detected=0
- query_info_regression_boundary_intact=1
- public_socket_opened=0
- loopback_udp_socket_opened=0
- real_client_binary_invoked=0

## Scenario Matrix
| scenario | proof | accepted | rejected | reason | drift_gate_passed | public_socket_opened | loopback_udp_socket_opened | real_client_binary_invoked |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| happy | pass | 1 | 0 | <none> | 1 | 0 | 0 | 0 |
| gate_disabled_by_default | pass | 0 | 1 | query_info_ci_drift_gate_disabled | 0 | 0 | 0 | 0 |
| gate_fixture_hash_drift | pass | 0 | 1 | query_info_fixture_drift_detected | 0 | 0 | 0 | 0 |
| gate_selected_fixture_changed | pass | 0 | 1 | query_info_selected_fixture_changed | 0 | 0 | 0 | 0 |
| gate_stage_changed | pass | 0 | 1 | query_info_stage_changed | 0 | 0 | 0 | 0 |
| gate_real_compatibility_claim_added | pass | 0 | 1 | real_compatibility_claim_rejected | 0 | 0 | 0 | 0 |
| gate_post_connect_unresolved_made_buildable | pass | 0 | 1 | post_connect_unresolved_made_buildable | 0 | 0 | 0 | 0 |
| gate_signon_unresolved_made_buildable | pass | 0 | 1 | signon_unresolved_made_buildable | 0 | 0 | 0 | 0 |
| gate_public_socket_policy_removed | pass | 0 | 1 | public_socket_policy_missing | 0 | 0 | 0 | 0 |
| gate_real_client_policy_removed | pass | 0 | 1 | real_client_policy_missing | 0 | 0 | 0 | 0 |
| gate_connect_path_allowed | pass | 0 | 1 | connect_path_allowed_in_query_info_boundary | 0 | 0 | 0 | 0 |
| gate_no_real_client_used | pass | 1 | 0 | <none> | 1 | 0 | 0 | 0 |
| gate_public_socket_blocked | pass | 0 | 1 | public_socket_blocked | 0 | 0 | 0 | 0 |
