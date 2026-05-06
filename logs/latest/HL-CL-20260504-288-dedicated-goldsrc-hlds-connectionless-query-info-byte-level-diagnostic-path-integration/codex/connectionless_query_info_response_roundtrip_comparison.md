# Connectionless Query/Info Response Roundtrip Comparison

## Request Recognition

The diagnostic path used an in-memory GoldSrc query/info-shaped request. No socket was opened. The request was accepted only when the connectionless marker/header and query command matched the expected diagnostic shape.

## Built Response

- Fixture: connectionless_query_info_candidate
- Stage: connectionless_query
- Bytes: 67
- Safe hex preview: FFFFFFFF6D3132372E302E302E313A3000484C656E67696E6520546573742053657276657200633061300076616C76650048616C662D4C696665000004306477000000
- Safe text preview: ....m127.0.0.1:0|HLengine Test Server|c0a0|valve|Half-Life||.0dw|||

## Parsed Response

- marker_or_header_valid=1
- opcode_or_tag_valid=1
- field_order_valid=1
- required_fields_present=1
- safe_string_policy_passed=1
- response_length_within_limit=1
- numeric_encoding_policy_checked=1
- string_encoding_policy_checked=1
- parse_succeeded=1
- roundtrip_validation_passed=1

## Stage Separation

Connectionless query/info bytes remain connectionless-only. They are not post-connect serverinfo evidence and not signon-time serverinfo evidence. The unresolved post-connect and signon fixtures remain non-buildable for real wire.
