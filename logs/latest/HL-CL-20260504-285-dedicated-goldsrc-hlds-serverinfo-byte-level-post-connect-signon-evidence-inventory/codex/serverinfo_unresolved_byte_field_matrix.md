# Unresolved Byte Field Matrix

Prompt: HL-CL-20260504-285-dedicated-goldsrc-hlds-serverinfo-byte-level-post-connect-signon-evidence-inventory

## Matrix

| Field or byte area | Stage | Current status | Current fixture value | Local evidence source | Evidence confidence | Unknown / risk | Impact on real client smoke | Smallest safe next action |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| connectionless marker/header | connectionless_query | known for query only | `FF FF FF FF` | `BuildGoldSrcInfoRequest`, `BuildGoldSrcInfoResponse` | local_code | Relation to post-connect/signon unresolved | Cannot prove real post-connect/signon acceptance | Keep query candidate separate. |
| post-connect response marker/tag | post_connect | unknown | unresolved | `post_connect_real_serverinfo_unresolved` | unresolved | Could be wrong frame, text command, or no separate response | Real client may ignore/reject immediately | Add evidence-gap guard/refinement fixture. |
| signon message opcode/id | signon_time | unknown | unresolved | prompt 264 and source search | unresolved | `svc_serverinfo` or equivalent id not locally proven | Cannot enter real signon state safely | Inventory netchan/signon dependency or guard unresolved fixture. |
| protocol field | post_connect/signon_time | diagnostic text only | `48` as diagnostic text | prompt 267/277/282/284 summaries | local_artifact | Real width, order, signedness, and stage unknown | Real client may reject protocol field shape | Keep text diagnostic separate. |
| server spawn count / server count | signon_time | unknown | unresolved | prompt 264 inventory | unresolved | Potential signon-state field missing | Signon may desync or reject | Do not synthesize; inventory signon contract. |
| map name | post_connect/signon_time | diagnostic text only | `crossfire` in diagnostic preview | prompt 267 fixture | local_artifact | Real string placement/termination unknown | Real client may reject or parse wrong map | Keep unresolved for real stages. |
| game directory | post_connect/signon_time | diagnostic text only | `valve` in diagnostic preview | prompt 267 fixture | local_artifact | Real field order and encoding unknown | Mod/game mismatch risk | Keep unresolved for real stages. |
| hostname | post_connect/signon_time | diagnostic text only | `HLengine_Diagnostic_Server` | prompt 267 fixture | local_artifact | Real field name/order unknown | Client UI/serverdata may not populate | Keep unresolved for real stages. |
| max clients / player slot | post_connect/signon_time | diagnostic text and query uint8 differ | `4` text in diagnostic, uint8 in query | query code and diagnostic fixture | mixed | Real width and slot/admission linkage unknown | Admission/signon may fail after serverinfo | Keep diagnostic/query distinctions. |
| checksum / map CRC | signon_time | unknown | unresolved | prompt 264 inventory | unresolved | Required checksum or map CRC may be absent | Real client may reject map/resource stage | Add unresolved guard; no builder. |
| resource/model/sound/event baselines linkage | signon_time | absent | unresolved | prompt 278/284 summaries `resource_baselines_not_sent=1` | local_artifact | Baseline packets not implemented | Client cannot progress through signon/resources | Inventory dependencies before smoke. |
| client entity / edict / spawn linkage | signon_time/post_connect | absent | unresolved | prompt 278/284 summaries `client_not_put_in_server=1` | local_artifact | No admission/spawn path | Client cannot enter server | Keep smoke blocked. |
| string termination / encoding | post_connect/signon_time | unknown | unresolved | fixture unresolved policies | unresolved | Text preview may not match real C strings or bitstream | Parser/builder could freeze wrong contract | Do not promote. |
| byte order / numeric widths | post_connect/signon_time | unknown | unresolved | fixture unresolved policies | unresolved | Width/endian incorrect for real client | Client parse failure or desync | Do not invent widths. |
| packet framing / netchan dependency | signon_time | unknown | unresolved | prompt 264/278 summaries `netchan_not_started=1` | local_artifact | Signon serverinfo may be inside netchan/reliable frames | Raw connectionless response would be wrong | Inventory netchan/signon dependency. |
| reliable/unreliable channel dependency | signon_time | absent | unresolved | prompt 278/284 summaries `reliable_channel_not_started=1` | local_artifact | Wrong channel ordering/acks | Client stalls or disconnects | Keep real smoke blocked. |

## Count

`unresolved_byte_areas_count=16` for the post-connect and signon-time areas above. The query marker/header is listed only to document the concept separation; it does not reduce the unresolved post-connect/signon count.
