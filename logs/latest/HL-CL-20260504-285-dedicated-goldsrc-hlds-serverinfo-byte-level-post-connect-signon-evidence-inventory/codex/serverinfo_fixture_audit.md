# Serverinfo Fixture Audit

Prompt: HL-CL-20260504-285-dedicated-goldsrc-hlds-serverinfo-byte-level-post-connect-signon-evidence-inventory

Fixture root: `fixtures/diagnostic/hlds/serverinfo`

## Corpus Counts

| Metric | Value |
| --- | --- |
| fixtures_total | 9 |
| fixtures_valid | 2 |
| fixtures_invalid_cases | 5 |
| fixtures_unresolved | 2 |
| fixtures_promoted | 0 |
| fixtures_modified | 0 |
| fixtures_added | 0 |
| real_compatibility_claims_count | 0 |

## Fixture Audit Matrix

| Fixture | Family | Stage | Evidence confidence | Compatibility claim | Byte-level data exists | Local evidence backing | Unresolved fields | Decision |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `connectionless_query_info_candidate` | connectionless_query_info_candidate | connectionless_query | local_code | fixture_only | yes, query safe hex/text preview and code-backed byte order | `BuildGoldSrcInfoRequest`, `BuildGoldSrcInfoResponse`, `ParseGoldSrcInfoResponse` | 4 | Keep as confirmed byte-level connectionless query candidate; do not promote to post-connect/signon. |
| `diagnostic_post_connect_serverinfo_current` | diagnostic_post_connect_serverinfo_current | diagnostic_current | local_artifact | diagnostic_only | no, diagnostic safe preview only | prompts 267, 277, 282, 284 diagnostic preview path | 4 | Keep diagnostic-preview-only. |
| `invalid_field_order_mismatch` | invalid_mutation | diagnostic_current | synthetic_placeholder | not_real_client_compatible | no | future parser gate fixture | 1 | Keep invalid mutation fixture. |
| `invalid_missing_required_field` | invalid_mutation | diagnostic_current | synthetic_placeholder | not_real_client_compatible | no | prompt 267 missing-field gate shape | 1 | Keep invalid mutation fixture. |
| `invalid_overlong_response` | invalid_mutation | diagnostic_current | synthetic_placeholder | not_real_client_compatible | no | future parser length-policy gate | 1 | Keep invalid mutation fixture. |
| `invalid_unknown_opcode_or_marker` | invalid_mutation | unresolved | synthetic_placeholder | not_real_client_compatible | no | bad-marker/future parser gate shape | 2 | Keep invalid mutation fixture. |
| `invalid_unsafe_string` | invalid_mutation | diagnostic_current | synthetic_placeholder | not_real_client_compatible | no | safety-policy mutation gate | 1 | Keep invalid mutation fixture. |
| `post_connect_real_serverinfo_unresolved` | post_connect_real_serverinfo_unresolved | post_connect | unresolved | unresolved | no | prompt 279 inventory found no local byte-level contract | 8 | Keep unresolved. No promotion. |
| `signon_time_serverinfo_unresolved` | signon_time_serverinfo_unresolved | signon_time | unresolved | unresolved | no | prompt 264 and prompt 279 found no local byte-level signon serverinfo contract | 6 | Keep unresolved. No promotion. |

## Concept Separation

The fixture corpus still separates:

- connectionless query/info serverinfo-like response
- diagnostic post-connect serverinfo preview
- unresolved real post-connect serverinfo
- unresolved signon-time serverinfo
- invalid/mutated diagnostic cases

No fixture claims real Steam Half-Life or HLDS-compatible client compatibility.
