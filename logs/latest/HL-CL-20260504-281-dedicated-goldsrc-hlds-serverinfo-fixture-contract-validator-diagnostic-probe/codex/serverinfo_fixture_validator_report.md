# Serverinfo Fixture Contract Validator Report

Prompt: HL-CL-20260504-281-dedicated-goldsrc-hlds-serverinfo-fixture-contract-validator-diagnostic-probe
Compatibility claim level: diagnostic-serverinfo-fixture-contract-validator-only; no real Steam Half-Life or HLDS-compatible client compatibility claimed
Source commit: e79942c657fc88b3a3fdd6419eccefe49d55a187

## Scope

This prompt adds a diagnostic-only validator/probe for the checked-in serverinfo fixture corpus at ixtures/diagnostic/hlds/serverinfo. The probe reads fixture metadata and contract files only. It does not open sockets, invoke Steam, invoke a real client binary, start auth, start netchan, enter signon state, emit baselines, or put a client in server.

## Implementation

- Launch flags added:
  - --hlds-serverinfo-fixture-contract-validator-diagnostic-probe
  - --hlds-serverinfo-fixture-contract-validator-diagnostic-probe-scenario <scenario>
- Host lifecycle point: existing diagnostic bootstrap summary path via FinalizeServerBootstrapStep.
- Fixture root: $(@{enabled=1; mode=dedicated; scenario=happy; accepted=1; rejected=0; lastRejectReason=<none>; compatibility_claim_level=diagnostic-serverinfo-fixture-contract-validator-only; no real Steam Half-Life or HLDS-compatible client compatibility claimed; diagnostic_only=1; validator_probe_enabled=1; validator_disabled_by_default=1; fixture_contract_loaded=1; fixture_contract_path=fixtures/diagnostic/hlds/serverinfo/contract/serverinfo_fixture_contract.schema.json; fixture_root=fixtures/diagnostic/hlds/serverinfo; fixture_files_loaded=9; fixture_files_expected=9; fixture_validation_attempted=1; fixture_validation_passed=1; fixture_validation_error_count=0; fixtures_total=9; fixtures_valid=2; fixtures_invalid_cases=5; fixtures_unresolved=2; fixture_families=connectionless_query_info_candidate|diagnostic_post_connect_serverinfo_current|post_connect_real_serverinfo_unresolved|signon_time_serverinfo_unresolved|invalid_mutation; serverinfo_concepts_separated=1; connectionless_query_fixture_present=1; diagnostic_post_connect_fixture_present=1; post_connect_real_fixture_present=1; signon_time_fixture_present=1; required_metadata_present=1; missing_metadata_field=<none>; invalid_fixtures_have_reject_reasons=1; unresolved_fixtures_claim_compatibility=0; real_compatibility_claims_count=0; safe_preview_policy_passed=1; safe_preview_max_bytes=240; safe_preview_unsafe_chars_detected=0; safe_preview_overlong_detected=0; known_byte_level_candidates_count=1; synthetic_placeholder_count=5; unresolved_fields_count=26; corpus_sufficient_for_next_diagnostic_builder=1; validator_mutation_mode=0; mutation_case=<none>; no_real_client_gate_passed=0; real_client_smoke_allowed_now=0; real_steam_client_used=0; real_client_binary_invoked=0; public_socket_opened=0; loopback_udp_socket_opened=0; socket_open_attempted=0; normal_host_behavior_changed=0; steam_auth_not_implemented=1; netchan_not_started=1; reliable_channel_not_started=1; resource_baselines_not_sent=1; signon_state_not_entered=1; client_not_put_in_server=1; recommended_next_prompt_id=HL-CL-20260504-282-dedicated-goldsrc-hlds-serverinfo-contract-backed-diagnostic-builder-parser; recommended_next_task=add a contract-backed diagnostic serverinfo builder/parser using the checked-in fixture corpus without real clients or sockets; auth=not_implemented; signon=not_entered; gameplay_transport=not_started; detail=diagnostic-only serverinfo fixture contract validator read checked-in corpus; no sockets or real client}.fixture_root)
- Contract path: $(@{enabled=1; mode=dedicated; scenario=happy; accepted=1; rejected=0; lastRejectReason=<none>; compatibility_claim_level=diagnostic-serverinfo-fixture-contract-validator-only; no real Steam Half-Life or HLDS-compatible client compatibility claimed; diagnostic_only=1; validator_probe_enabled=1; validator_disabled_by_default=1; fixture_contract_loaded=1; fixture_contract_path=fixtures/diagnostic/hlds/serverinfo/contract/serverinfo_fixture_contract.schema.json; fixture_root=fixtures/diagnostic/hlds/serverinfo; fixture_files_loaded=9; fixture_files_expected=9; fixture_validation_attempted=1; fixture_validation_passed=1; fixture_validation_error_count=0; fixtures_total=9; fixtures_valid=2; fixtures_invalid_cases=5; fixtures_unresolved=2; fixture_families=connectionless_query_info_candidate|diagnostic_post_connect_serverinfo_current|post_connect_real_serverinfo_unresolved|signon_time_serverinfo_unresolved|invalid_mutation; serverinfo_concepts_separated=1; connectionless_query_fixture_present=1; diagnostic_post_connect_fixture_present=1; post_connect_real_fixture_present=1; signon_time_fixture_present=1; required_metadata_present=1; missing_metadata_field=<none>; invalid_fixtures_have_reject_reasons=1; unresolved_fixtures_claim_compatibility=0; real_compatibility_claims_count=0; safe_preview_policy_passed=1; safe_preview_max_bytes=240; safe_preview_unsafe_chars_detected=0; safe_preview_overlong_detected=0; known_byte_level_candidates_count=1; synthetic_placeholder_count=5; unresolved_fields_count=26; corpus_sufficient_for_next_diagnostic_builder=1; validator_mutation_mode=0; mutation_case=<none>; no_real_client_gate_passed=0; real_client_smoke_allowed_now=0; real_steam_client_used=0; real_client_binary_invoked=0; public_socket_opened=0; loopback_udp_socket_opened=0; socket_open_attempted=0; normal_host_behavior_changed=0; steam_auth_not_implemented=1; netchan_not_started=1; reliable_channel_not_started=1; resource_baselines_not_sent=1; signon_state_not_entered=1; client_not_put_in_server=1; recommended_next_prompt_id=HL-CL-20260504-282-dedicated-goldsrc-hlds-serverinfo-contract-backed-diagnostic-builder-parser; recommended_next_task=add a contract-backed diagnostic serverinfo builder/parser using the checked-in fixture corpus without real clients or sockets; auth=not_implemented; signon=not_entered; gameplay_transport=not_started; detail=diagnostic-only serverinfo fixture contract validator read checked-in corpus; no sockets or real client}.fixture_contract_path)
- Fixture files loaded in happy proof: 9 / 9

## Happy Proof

The happy proof validated the checked-in contract and all nine checked-in fixtures.

- fixture_validation_passed: 1
- fixtures_total: 9
- fixtures_valid: 2
- fixtures_invalid_cases: 5
- fixtures_unresolved: 2
- serverinfo_concepts_separated: 1
- required_metadata_present: 1
- invalid_fixtures_have_reject_reasons: 1
- unresolved_fixtures_claim_compatibility: 0
- real_compatibility_claims_count: 0
- safe_preview_policy_passed: 1

## Mutation Gates

| Scenario | Result | Stable reason |
| --- | --- | --- |
| gate_disabled_by_default | pass | fixture_validator_disabled |
| gate_no_real_client_used | pass | no real client binary invoked |
| gate_unresolved_claims_compatible | pass | unresolved_fixture_claims_compatibility |
| gate_missing_required_metadata | pass | missing_required_fixture_metadata |
| gate_invalid_fixture_missing_reject_reason | pass | invalid_fixture_missing_reject_reason |
| gate_family_mixing | pass | serverinfo_fixture_family_mixing |
| gate_unsafe_safe_preview | pass | unsafe_fixture_safe_preview |
| gate_public_socket_blocked | pass | public_socket_blocked |

## Safety Results

- real_steam_client_used: 0
- real_client_binary_invoked: 0
- public_socket_opened: 0
- loopback_udp_socket_opened: 0
- normal_host_behavior_changed: 0
- steam_auth_not_implemented: 1
- netchan_not_started: 1
- reliable_channel_not_started: 1
- resource_baselines_not_sent: 1
- signon_state_not_entered: 1
- client_not_put_in_server: 1

## Conclusion

The checked-in corpus is now guarded by a source-level diagnostic validator/probe. This still does not imply real HLDS or Steam client compatibility; it only validates fixture metadata, safety policy, concept separation, and mutation rejection over local diagnostic data.
