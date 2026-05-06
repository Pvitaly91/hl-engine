# Fixture Promotion Decision

Prompt: HL-CL-20260504-285-dedicated-goldsrc-hlds-serverinfo-byte-level-post-connect-signon-evidence-inventory

## Decision Summary

No fixture promotion, split, rename, retirement, or metadata update is justified by local evidence in this prompt.

| Fixture | Decision | Reason |
| --- | --- | --- |
| `connectionless_query_info_candidate` | keep unchanged | It is byte-level but scoped to connectionless query/info. Promoting it to post-connect or signon would merge distinct concepts. |
| `diagnostic_post_connect_serverinfo_current` | keep unchanged | It is diagnostic-preview-only and already powers contract-backed diagnostic preview flow. It is not real byte-level post-connect evidence. |
| `post_connect_real_serverinfo_unresolved` | keep unresolved | No local marker/tag, field order, numeric/string encoding, length policy, or acceptance fixture was found. |
| `signon_time_serverinfo_unresolved` | keep unresolved | No local `svc_serverinfo` id, signon payload order, netchan/reliable frame dependency, or baseline/resource linkage was found. |
| `invalid_field_order_mismatch` | keep invalid mutation fixture | Useful parser gate; not a compatibility candidate. |
| `invalid_missing_required_field` | keep invalid mutation fixture | Useful parser gate; not a compatibility candidate. |
| `invalid_overlong_response` | keep invalid mutation fixture | Useful parser gate; not a compatibility candidate. |
| `invalid_unknown_opcode_or_marker` | keep invalid mutation fixture | Useful parser gate; not a compatibility candidate. |
| `invalid_unsafe_string` | keep invalid mutation fixture | Useful parser gate; not a compatibility candidate. |

## Promotion Counts

| Metric | Value |
| --- | ---: |
| fixtures_promoted | 0 |
| fixtures_kept_unresolved | 2 |
| fixtures_added | 0 |
| fixtures_modified | 0 |
| fixtures_retired | 0 |

## Next Prompt Recommendation

Recommended next prompt:

`HL-CL-20260504-286-dedicated-goldsrc-hlds-serverinfo-unresolved-fixture-refinement-and-evidence-gap-guard`

Recommended task:

Add a diagnostic guard around unresolved post-connect and signon-time serverinfo fixtures so future builders/parsers cannot accidentally treat diagnostic preview text or connectionless query bytes as real post-connect/signon evidence.

Why this is the smallest safe next step:

- It directly addresses the only actionable finding from this inventory.
- It avoids inventing byte-level formats.
- It preserves the prompt 280-284 diagnostic builder/parser/smoke chain.
- It keeps real-client smoke blocked until real byte-level evidence exists.
