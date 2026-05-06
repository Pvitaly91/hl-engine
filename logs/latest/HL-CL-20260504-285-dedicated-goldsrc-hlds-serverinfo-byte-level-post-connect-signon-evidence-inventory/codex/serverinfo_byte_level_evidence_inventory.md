# Byte-Level Serverinfo Evidence Inventory

Prompt: HL-CL-20260504-285-dedicated-goldsrc-hlds-serverinfo-byte-level-post-connect-signon-evidence-inventory

Compatibility claim level: diagnostic-byte-level-serverinfo-evidence-inventory-only; no real Steam Half-Life or HLDS-compatible client compatibility claimed

## Finding

No local byte-level evidence was found for a real GoldSrc/HLDS post-connect serverinfo response or signon-time `svc_serverinfo`-style payload. The only byte-level serverinfo-like implementation found locally remains the connectionless query/info packet path:

- `BuildGoldSrcInfoRequest`
- `BuildGoldSrcInfoResponse`
- `ParseGoldSrcInfoResponse`
- `PumpOneLoopbackGoldSrcInfoQuery`

That path is scoped to server browser/query-style info and cannot be promoted to post-connect or signon-time serverinfo. Prompt 284 proved a diagnostic localhost client can receive a contract-backed diagnostic preview response, but that response remains `diagnostic_preview_builder_complete=1`, `byte_level_builder_complete=0`, and `real_wire_builder_complete=0`.

## Source Evidence

| Location | Symbol / artifact | Evidence type | Stage | Byte-level evidence | Inventory decision |
| --- | --- | --- | --- | --- | --- |
| `src/game_api/hl_server_module.cpp:127807` | `BuildGoldSrcInfoRequest` | code | connectionless query | yes | Builds `FF FF FF FF TSource Engine Query 00`; query-only. |
| `src/game_api/hl_server_module.cpp:127833` | `BuildGoldSrcInfoResponse` | code | connectionless query/info response | yes | Builds `FF FF FF FF m`, C strings, uint8 player/max/protocol/trailing fields; query-only. |
| `src/game_api/hl_server_module.cpp:127889` | `ParseGoldSrcInfoResponse` | code | connectionless query/info response | yes | Parses marker/tag and query fields; not post-connect or signon. |
| `src/game_api/hl_server_module.cpp:149695` | `PumpOneLoopbackGoldSrcInfoQuery` | code | connectionless loopback query proof harness | yes for query bytes | Uses query bytes with loopback sockets in earlier prompts; not run for this prompt. |
| `src/game_api/hl_server_module.cpp:128529` | `BuildHldsServerinfoDiagnosticResultFromConnect` | code | diagnostic post-connect/connect-response preview | no | Builds text safe preview `serverinfo protocol=48 ...`; not a real byte-level wire contract. |
| `src/game_api/hl_server_module.cpp:250143` through `250183` | `StubWriteByte`, `StubWriteShort`, `StubWriteLong`, `StubWriteString` | code | generic game DLL message callback stubs | no for serverinfo | Generic message buffer stubs; no local `svc_serverinfo`, `MSG_Write`, or serverinfo payload contract. |
| `include/game_api/hl_server_module.h:7356` through `7497` | contract-backed validator/builder/path/smoke summaries | code | diagnostic fixture/reporting | no | Summary surfaces report validator/builder/smoke state; not a real wire encoder. |
| `fixtures/diagnostic/hlds/serverinfo/fixtures/connectionless_query_info_candidate.json` | checked-in fixture | fixture | connectionless query | partial local byte fixture | Confirmed only as fixture-only connectionless query candidate. |
| `fixtures/diagnostic/hlds/serverinfo/fixtures/diagnostic_post_connect_serverinfo_current.json` | checked-in fixture | fixture | diagnostic current | no | Diagnostic text preview, not byte-level real post-connect evidence. |
| `fixtures/diagnostic/hlds/serverinfo/fixtures/post_connect_real_serverinfo_unresolved.json` | checked-in fixture | fixture | post_connect | no | Must remain unresolved. |
| `fixtures/diagnostic/hlds/serverinfo/fixtures/signon_time_serverinfo_unresolved.json` | checked-in fixture | fixture | signon_time | no | Must remain unresolved. |
| `logs/latest/HL-CL-20260504-279-.../serverinfo_wire_format_inventory.md` | prompt 279 inventory | artifact | all serverinfo concepts | no new byte evidence | Reconfirmed that only connectionless query/info has local byte-level code. |
| `logs/latest/HL-CL-20260504-279-.../serverinfo_candidate_matrix.md` | prompt 279 candidate matrix | artifact | all serverinfo concepts | no new byte evidence | Marks post-connect and signon real contracts as unknown. |
| `logs/latest/HL-CL-20260504-264-.../wire_contract_inventory.md` | prompt 264 wire inventory | artifact | signon | no | States signon byte builders are synthetic and real signon/netchan/baseline contracts remain missing. |
| `logs/latest/HL-CL-20260504-284-.../serverinfo_contract_backed_localhost_smoke_swap_summary.json` | prompt 284 summary | artifact | diagnostic localhost smoke | no real wire | Records contract-backed diagnostic preview success with `byte_level_builder_complete=0` and `real_wire_builder_complete=0`. |

## Negative Evidence

- Source search found no current source definitions for `svc_serverinfo`, `SVC_SERVERINFO`, `MSG_Write`, `SZ_Write`, `signon buffer`, `bitstream`, `server data`, or `client put-in-server`.
- `WriteByte`, `WriteShort`, `WriteLong`, and `WriteString` exist as generic engine shim callbacks, but they are not tied to a serverinfo opcode, field order, packet frame, or signon transition.
- Signon, baseline, resource, model, sound, event, and netchan mentions are diagnostics, summaries, or unrelated scaffolding for this inventory. They do not prove a post-connect or signon-time serverinfo contract.

## Answered Questions

| Question | Answer |
| --- | --- |
| Is there local byte-level post-connect serverinfo evidence? | No. The post-connect real fixture remains unresolved. |
| Is there local byte-level signon-time serverinfo evidence? | No. No local `svc_serverinfo` message id, payload order, packet frame, or netchan/signon context was found. |
| Can any unresolved fixture be promoted? | No. Promotion would invent a real wire contract from diagnostic preview text or query bytes. |
| Which existing fixture is byte-level? | `connectionless_query_info_candidate`, scoped to connectionless query/info only. |
| What exact gap blocks a real wire builder? | Missing post-connect/signon marker or message id, field order, numeric widths, string termination, packet framing, netchan/reliable dependency, baselines/resources linkage, and acceptance fixtures. |

## Safety Boundary

This prompt did not launch Steam, invoke any real client binary, open public sockets, open loopback sockets, run the frame pump, run the socket pump, start netchan, enter signon, emit resource/model/sound/event baselines, or put a client in server. It is report-only.
