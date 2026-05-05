# Serverinfo Fixture Validation Report

PROMPT-ID: HL-CL-20260504-280-dedicated-goldsrc-hlds-serverinfo-wire-format-fixture-corpus-contract-definition

## Result

Validation status: pass

This prompt added a checked-in diagnostic fixture corpus and contract definition only. No source-code validator, socket behavior, client process invocation, public bind path, host lifecycle hook, or runtime networking proof was added.

## Validation Coverage

| Check | Result | Notes |
| --- | --- | --- |
| JSON parse validation | pass | Contract schema plus all fixture JSON files parse with PowerShell ConvertFrom-Json. |
| Required metadata fields | pass | All nine fixture files include fixture_id, family, stage, evidence_source, evidence_confidence, compatibility_claim, marker_or_header, opcode_or_tag, field_order, fields, string_encoding_policy, numeric_encoding_policy, length_policy, safety_policy, expected_parse_result, expected_reject_reason, unresolved_fields, safe_preview, and notes. |
| Conceptual separation | pass | Fixtures keep connectionless_query_info_candidate, diagnostic_post_connect_serverinfo_current, post_connect_real_serverinfo_unresolved, signon_time_serverinfo_unresolved, and invalid_mutation families separate. |
| Stage taxonomy | pass | Fixtures use only connectionless_query, diagnostic_current, post_connect, signon_time, or unresolved. |
| Evidence taxonomy | pass | Fixtures use local_code, local_artifact, synthetic_placeholder, or unresolved. |
| Compatibility taxonomy | pass | Fixtures use diagnostic_only, fixture_only, unresolved, or not_real_client_compatible. |
| Real compatibility claims | pass | Count is 0. No fixture claims real Steam Half-Life or HLDS-compatible client compatibility. |
| Safety policy | pass | Every fixture records real_steam_client_used=0, real_client_binary_invoked=0, public_socket_opened=0, normal_host_behavior_changed=0, and real_client_smoke_allowed_now=0. |
| Invalid reject reasons | pass | Each invalid fixture records a stable expected reject reason. |
| Safe preview policy | pass | Fixture previews are short deterministic text/hex previews and are not packet captures from a real client. |

## Invalid Fixture Gates Reserved For Future Parser

| Fixture | Expected parse result | Expected reject reason |
| --- | --- | --- |
| invalid_missing_required_field | reject | missing_serverinfo_required_field |
| invalid_field_order_mismatch | reject | serverinfo_field_order_mismatch |
| invalid_unsafe_string | reject | unsafe_serverinfo_string |
| invalid_overlong_response | reject | serverinfo_response_too_large |
| invalid_unknown_opcode_or_marker | reject | unknown_serverinfo_marker_or_opcode |

## Validator Implementation Decision

fixture_validator_implemented: 0

A compiled validator/probe was not added here because the prompt target was the fixture corpus and contract definition. The next smallest safe implementation step is a diagnostic-only validator/probe that reads this corpus, validates the metadata and safety policy mechanically, emits bounded summaries, and still opens no sockets.

## Build And Runtime

- Lightweight build: not run, because no compiled source files changed.
- Runtime proof: not run, because no runtime behavior was added.
- Real client proof: not run and still prohibited.
- Public socket proof: not run, because no socket behavior was added.
