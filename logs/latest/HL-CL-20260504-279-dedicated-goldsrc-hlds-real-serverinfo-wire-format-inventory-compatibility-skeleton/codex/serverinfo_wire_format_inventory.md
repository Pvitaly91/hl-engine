# Serverinfo Wire-Format Inventory

Prompt: HL-CL-20260504-279-dedicated-goldsrc-hlds-real-serverinfo-wire-format-inventory-compatibility-skeleton

Compatibility claim level: diagnostic-real-serverinfo-wire-format-skeleton-only; no real Steam Half-Life or HLDS-compatible client compatibility claimed

## Concept Separation

| Concept | Local evidence | Scope | Current conclusion |
| --- | --- | --- | --- |
| Connectionless server query/info response | `BuildGoldSrcInfoRequest`, `BuildGoldSrcInfoResponse`, `ParseGoldSrcInfoResponse`, and `PumpOneLoopbackGoldSrcInfoQuery` in `src/game_api/hl_server_module.cpp` | Server browser/query-style loopback info path | Present as a byte-level local diagnostic/query candidate. It is not the prompt 267/277 post-connect diagnostic serverinfo response and is not evidence of client admission. |
| Post-connect/serverinfo-shaped diagnostic response | Prompt 267 report/summary and `BuildHldsServerinfoDiagnosticResultFromConnect`; prompt 277 client-observed response preview | Diagnostic response after getchallenge/connect-shaped diagnostic path | Present as connectionless text: `FF FF FF FF serverinfo protocol=48 hostname=... map=... game=... maxplayers=... slot=... 00`. It is not proven as a real HLDS wire contract. |
| Signon-time serverinfo service/message | Prompt 264 signon wire-contract inventory and source searches for `svc_serverinfo`, `SVC`, signon, baseline, netchan | Signon service/message layer after channel setup | No local byte-level `svc_serverinfo` contract or real signon serverinfo encoder was found. Existing signon proof stack is synthetic. |
| Real HLDS/GoldSrc serverinfo wire contract | Local code and artifacts only | Real compatibility target | No sufficient local evidence was found for exact real post-connect or signon serverinfo field order, opcode/message id, string/numeric encoding, or client acceptance criteria. |

## Source Evidence

- `src/game_api/hl_server_module.cpp:127280` builds a connectionless info request: `FF FF FF FF TSource Engine Query 00`.
- `src/game_api/hl_server_module.cpp:127306` builds a connectionless info response beginning `FF FF FF FF m`, followed by C strings for address, server name, map, mod, game description, then byte fields for players, max players, protocol `48`, and trailing bytes `d`, `w`, `0`, `0`, `0`.
- `src/game_api/hl_server_module.cpp:127362` parses that query response enough to validate marker/tag, C strings, player count, and max player count. It does not name or validate all trailing byte semantics.
- `src/game_api/hl_server_module.cpp:127398` builds generic connectionless text packets with `FF FF FF FF`, text, and trailing NUL.
- `src/game_api/hl_server_module.cpp:128002` through `128061` builds the prompt 267 diagnostic post-connect serverinfo result as text-shaped safe preview only.
- `src/game_api/hl_server_module.cpp:149908`, `150550`, `151241`, and `151948` send the diagnostic text serverinfo response through UDP/frame-pump paths.
- Prompt 277 summary records `client_serverinfo_response_received=1`, `client_serverinfo_shape_valid=1`, and safe preview `FF FF FF FF serverinfo protocol=48 hostname=HLengine_Diagnostic_Server map=crossfire game=valve maxplayers=4 slot=diagnostic-client-slot-1 00`.
- Prompt 264 inventory states that connect-time serverinfo/signon remains missing and that existing signon proof byte builders are synthetic.

## Byte-Level Encoder Status

| Encoder/parser | Byte-level | Stage | Suitable for real client smoke now | Notes |
| --- | --- | --- | --- | --- |
| `BuildGoldSrcInfoResponse` / `ParseGoldSrcInfoResponse` | yes | connectionless query/info | no | Useful candidate for server browser/query inventory, but not the post-connect serverinfo response prompt 277 exercises. |
| `BuildHldsServerinfoDiagnosticResultFromConnect` | no, text safe preview only | diagnostic post-connect/connect-response | no | Diagnostic fields are key/value text and include synthetic slot. |
| Signon-time serverinfo encoder/parser | no local evidence found | signon | no | `svc_serverinfo` or equivalent real payload contract is unresolved locally. |

## Evidence Sufficiency Decision

`wire_format_inventory_complete=1` for repo-local inspection. `wire_format_evidence_sufficient=0` for implementing a new real serverinfo compatibility skeleton because the only concrete byte-level candidate is connectionless query/info, while the needed real post-connect/signon serverinfo contract is not locally documented or implemented.

Adding a source builder in this prompt would risk freezing the diagnostic text response or query response as a false real-client compatibility claim. The selected promotion remains report-only with `skeleton_implemented=0`.
