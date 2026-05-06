# Byte-Level Candidate Classification

Prompt: HL-CL-20260504-285-dedicated-goldsrc-hlds-serverinfo-byte-level-post-connect-signon-evidence-inventory

## Classification Summary

| Class | Count | Candidates |
| --- | ---: | --- |
| confirmed_byte_level_connectionless_query | 1 | `connectionless_query_info_candidate` |
| candidate_byte_level_post_connect | 0 | none |
| candidate_byte_level_signon_time | 0 | none |
| diagnostic_preview_only | 1 | `diagnostic_post_connect_serverinfo_current` |
| synthetic_placeholder | 5 | `invalid_field_order_mismatch`, `invalid_missing_required_field`, `invalid_overlong_response`, `invalid_unknown_opcode_or_marker`, `invalid_unsafe_string` |
| unresolved_no_byte_evidence | 2 | `post_connect_real_serverinfo_unresolved`, `signon_time_serverinfo_unresolved` |
| invalid_mutation_fixture | 5 | same five invalid fixtures |

## Candidate Details

| Candidate | Stage | Byte evidence confidence | Marker/header | Opcode/message id/tag | Field order | Numeric encoding | String encoding | Length policy | Known bytes / safe preview | Expected buildability | Safe next action |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `connectionless_query_info_candidate` | connectionless_query | medium for local code, low for real compatibility | `FF FF FF FF` | request `TSource Engine Query`, response tag `m` | address, server_name, map, mod, game_description, players, max_players, protocol, server_type, os, password, mod_running, secure | uint8 fields, no endian issue for parsed fields | ASCII C strings, NUL terminated | builder reserves 192; protocol limit unresolved | `ff ff ff ff 6d`; text preview `m 127.0.0.1:<port> ... protocol=48` | buildable only as query fixture; not post-connect/signon | If desired, implement a connectionless query/info byte-level builder/parser, not a real client path. |
| `diagnostic_post_connect_serverinfo_current` | diagnostic_current | high for diagnostic behavior, low for real compatibility | `FF FF FF FF` diagnostic text preview | text command `serverinfo` | protocol, hostname, map, game, maxplayers, slot | decimal text | ASCII text, trailing NUL after text packet | diagnostic max 256 | `serverinfo protocol=48 hostname=...` | buildable only as diagnostic preview; already covered by prompts 282-284 | Keep diagnostic only; do not promote. |
| `post_connect_real_serverinfo_unresolved` | post_connect | none | unknown | unknown | unknown | unknown | unknown | unknown | none | not buildable | Keep unresolved; add guard/refinement rather than builder. |
| `signon_time_serverinfo_unresolved` | signon_time | none | requires netchan/signon context | `svc_serverinfo` or equivalent not locally proven | unknown | unknown | unknown | unknown | none | not buildable | Keep unresolved; inventory netchan/signon dependency or add evidence-gap guard. |
| invalid mutation fixtures | diagnostic_current/unresolved | synthetic placeholders | varied | varied | intentionally invalid | diagnostic mutation only | diagnostic mutation only | diagnostic mutation only | bounded safe previews | not buildable | Keep as parser gates. |

## Build Permission Decision

`byte_level_builder_allowed_next=0` for post-connect and signon-time serverinfo. A builder for those stages would require byte-level evidence that is not present locally. A query-only builder/parser could be safe later, but it would not reduce the post-connect/signon real-client gap.
