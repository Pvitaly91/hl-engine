# Client Observed Query/Info Response Comparison

Selected fixture: connectionless_query_info_candidate
Stage: connectionless_query

The diagnostic query client observed the byte-level response produced by the prompt 289 loopback query/info response path. The client-side shape validation passed and the response remained connectionless query/info only.

- response_received: 1
- response_shape_valid: 1
- response_bytes: 67
- safe_hex_preview: `FFFFFFFF6D3132372E302E302E313A3000484C656E67696E6520546573742053657276657200633061300076616C76650048616C662D4C696665000004306477000000`
- safe_text_preview: `....m127.0.0.1:0|HLengine Test Server|c0a0|valve|Half-Life||.0dw|||`
- marker_or_header_valid: 1
- opcode_or_tag_valid: 1
- field_order_valid: 1
- required_fields_present: 1
- safe_string_policy_passed: 1
- response_length_within_limit: 1

This is not post-connect serverinfo and not signon-time serverinfo.

