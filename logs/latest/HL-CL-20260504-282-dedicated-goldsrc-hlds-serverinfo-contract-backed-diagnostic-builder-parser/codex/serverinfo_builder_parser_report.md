# Serverinfo Contract-Backed Diagnostic Builder/Parser Report

Prompt: HL-CL-20260504-282-dedicated-goldsrc-hlds-serverinfo-contract-backed-diagnostic-builder-parser

Compatibility claim level: diagnostic-serverinfo-contract-backed-builder-parser-only; no real Steam Half-Life or HLDS-compatible client compatibility claimed

## Scope

This prompt adds a diagnostic-only contract-backed serverinfo builder/parser probe. It reads the checked-in fixture corpus under fixtures/diagnostic/hlds/serverinfo, invokes the prompt 281 fixture validator first, builds only allowed diagnostic fixture previews, and parses those previews back against the declared fixture contract.

It does not implement real HLDS compatibility, does not invoke Steam or any real Half-Life client binary, does not open public or loopback sockets, and does not change normal host behavior.

## Launch Surface

- --hlds-serverinfo-contract-backed-diagnostic-builder-parser
- --hlds-serverinfo-contract-backed-diagnostic-builder-parser-probe
- --hlds-serverinfo-contract-backed-diagnostic-builder-parser-probe-scenario <scenario>

The probe is disabled by default. The base flag and probe flag both enable the same diagnostic-only summary path.

## Builder/Parser Behavior

- Validator dependency: prompt 281 validator is invoked for build/parse scenarios.
- Buildable fixture families: diagnostic_post_connect_serverinfo_current and connectionless_query_info_candidate.
- Selected happy fixture: diagnostic_post_connect_serverinfo_current.
- Selected stage: diagnostic_current.
- Diagnostic preview builder complete: 1.
- Byte-level builder complete: 0.
- Real wire builder complete: 0.
- Real compatibility claims count: 0.

Because the checked-in corpus still does not contain sufficient real post-connect or signon byte-level evidence, this builder remains diagnostic-preview-only and explicitly reports real_wire_builder_complete=0.

## Proof Results

| Scenario | Accepted | Rejected | Reason |
| --- | ---: | ---: | --- |
| happy | 1 | 0 | <none> |
| gate_disabled_by_default | 0 | 1 | serverinfo_builder_parser_disabled |
| gate_validator_required | 0 | 1 | fixture_validator_required |
| gate_unresolved_fixture_rejected | 0 | 1 | unresolved_serverinfo_fixture_not_buildable |
| gate_invalid_fixture_rejected | 0 | 1 | invalid_serverinfo_fixture_not_buildable |
| gate_real_compatibility_claim_rejected | 0 | 1 | real_compatibility_claim_rejected |
| gate_field_order_mismatch | 0 | 1 | serverinfo_field_order_mismatch |
| gate_missing_required_field | 0 | 1 | missing_serverinfo_required_field |
| gate_unsafe_string | 0 | 1 | unsafe_serverinfo_string |
| gate_overlong_response | 0 | 1 | serverinfo_response_too_large |
| gate_no_real_client_used | 1 | 0 | <none> |
| gate_public_socket_blocked | 0 | 1 | public_socket_blocked |

## Safety Results

Every runtime scenario reported:

- public_socket_opened=0
- loopback_udp_socket_opened=0
- socket_open_attempted=0
- real_steam_client_used=0
- real_client_binary_invoked=0
- normal_host_behavior_changed=0
- steam_auth_not_implemented=1
- netchan_not_started=1
- reliable_channel_not_started=1
- resource_baselines_not_sent=1
- signon_state_not_entered=1
- client_not_put_in_server=1

## Next Step

Recommended next prompt: HL-CL-20260504-283-dedicated-goldsrc-hlds-serverinfo-contract-backed-diagnostic-serverinfo-path-integration

Recommended next task: integrate the contract-backed diagnostic builder/parser into the explicit diagnostic serverinfo path without real clients or sockets
