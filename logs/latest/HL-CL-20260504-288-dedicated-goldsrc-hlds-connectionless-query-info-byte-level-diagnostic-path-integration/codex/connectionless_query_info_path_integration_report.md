# Connectionless Query/Info Byte-Level Diagnostic Path Integration

Prompt: HL-CL-20260504-288-dedicated-goldsrc-hlds-connectionless-query-info-byte-level-diagnostic-path-integration

Compatibility claim level: diagnostic-connectionless-query-info-byte-level-path-integration-only; no real Steam Half-Life or HLDS-compatible client compatibility claimed

## Result

The explicit diagnostic query/info path recognizes only the in-memory connectionless query/info request shape, invokes the fixture validator, invokes the unresolved evidence-gap guard, invokes the prompt 287 connectionless query/info byte-level builder/parser, and marks the response ready only after byte parse/roundtrip validation succeeds.

This path is disabled by default and does not enable sockets, localhost smoke, Steam, real client binaries, auth, netchan, signon, resource baselines, or client admission.

## Selected Fixture

- Fixture root: fixtures/diagnostic/hlds/serverinfo
- Selected fixture: connectionless_query_info_candidate
- Selected stage: connectionless_query
- Byte builder complete: 1
- Byte parser complete: 1
- Response bytes: 67
- Safe hex preview: FFFFFFFF6D3132372E302E302E313A3000484C656E67696E6520546573742053657276657200633061300076616C76650048616C662D4C696665000004306477000000
- Safe text preview: ....m127.0.0.1:0|HLengine Test Server|c0a0|valve|Half-Life||.0dw|||

## Guard Boundaries

- connectionless_query_not_post_connect=1
- connectionless_query_not_signon=1
- diagnostic_preview_not_byte_evidence=1
- post_connect_byte_evidence_sufficient=0
- signon_time_byte_evidence_sufficient=0
- real_post_connect_builder_complete=0
- real_signon_builder_complete=0
- real_wire_builder_complete=0

## Runtime Proofs

| Scenario | Proof | Accepted | Rejected | Last Reject Reason |
|---|---:|---:|---:|---|
| happy | pass | 1 | 0 | <none> |
| gate_disabled_by_default | pass | 0 | 1 | connectionless_query_info_path_integration_disabled |
| gate_query_info_mode_required | pass | 0 | 1 | connectionless_query_info_mode_required |
| gate_validator_required | pass | 0 | 1 | fixture_validator_required |
| gate_evidence_gap_guard_required | pass | 0 | 1 | serverinfo_evidence_gap_guard_required |
| gate_builder_roundtrip_required | pass | 0 | 1 | connectionless_query_info_builder_roundtrip_required |
| gate_wrong_command_or_query | pass | 0 | 1 | unsupported_connectionless_query_command |
| gate_bad_marker_or_header | pass | 0 | 1 | connectionless_query_missing_marker_or_header |
| gate_wrong_opcode_or_tag | pass | 0 | 1 | connectionless_query_wrong_opcode_or_tag |
| gate_missing_required_field | pass | 0 | 1 | connectionless_query_missing_required_field |
| gate_unsafe_string | pass | 0 | 1 | connectionless_query_unsafe_string |
| gate_overlong_response | pass | 0 | 1 | connectionless_query_response_too_large |
| gate_post_connect_stage_confusion_rejected | pass | 0 | 1 | connectionless_query_not_post_connect_evidence |
| gate_signon_stage_confusion_rejected | pass | 0 | 1 | connectionless_query_not_signon_evidence |
| gate_unresolved_post_connect_rejected | pass | 0 | 1 | unresolved_post_connect_serverinfo_not_buildable |
| gate_unresolved_signon_rejected | pass | 0 | 1 | unresolved_signon_serverinfo_not_buildable |
| gate_real_compatibility_claim_rejected | pass | 0 | 1 | real_compatibility_claim_rejected |
| gate_no_real_client_used | pass | 1 | 0 | <none> |
| gate_public_socket_blocked | pass | 0 | 1 | public_socket_blocked |

## Validation

- Fixture JSON validation: pass for all 9 checked-in fixture JSON files.
- Descendant gates: pass for all required local commits through d332394b52faedfec4e0be52499a4c59ce0be2da.
- Build: MSBuild Debug Win32 hlhost passed with 2 existing C4127 warnings and 0 errors.
- Runtime proofs: all 19 scenarios passed.
