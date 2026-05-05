# Corpus Validation Comparison

Prompt: HL-CL-20260504-281-dedicated-goldsrc-hlds-serverinfo-fixture-contract-validator-diagnostic-probe

## Prompt 280 Baseline

Prompt 280 created the checked-in fixture corpus with 9 fixtures: 2 valid fixture-only cases, 5 invalid/mutated cases, and 2 unresolved real-contract placeholders. It reported manual validation as passing and no source validator.

## Prompt 281 Validator Result

The prompt 281 source validator loaded the same corpus and contract files in the happy proof and matched the prompt 280 counts exactly.

| Field | Prompt 280 baseline | Prompt 281 happy proof |
| --- | ---: | ---: |
| fixtures_total | 9 | 9 |
| fixtures_valid | 2 | 2 |
| fixtures_invalid_cases | 5 | 5 |
| fixtures_unresolved | 2 | 2 |
| known_byte_level_candidates_count | 1 | 1 |
| synthetic_placeholder_count | 5 | 5 |
| unresolved_fields_count | 26 | 26 |
| real_compatibility_claims_count | 0 | 0 |

## Concept Separation

The validator requires separate fixture identities for:

- connectionless_query_info_candidate
- diagnostic_post_connect_serverinfo_current
- post_connect_real_serverinfo_unresolved
- signon_time_serverinfo_unresolved
- invalid_mutation

The gate_family_mixing proof mutates one fixture across family/stage boundaries and rejects it with serverinfo_fixture_family_mixing.

## Safety Boundaries

The validator performs file/metadata validation only. The final proofs kept public_socket_opened=0, loopback_udp_socket_opened=0, eal_steam_client_used=0, and eal_client_binary_invoked=0.
