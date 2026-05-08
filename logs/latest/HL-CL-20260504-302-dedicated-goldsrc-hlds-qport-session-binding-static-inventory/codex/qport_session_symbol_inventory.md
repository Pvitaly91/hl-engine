# Qport/Session Symbol Inventory

Prompt: HL-CL-20260504-302-dedicated-goldsrc-hlds-qport-session-binding-static-inventory

This inventory is static-only. It did not run getchallenge, connect, post-connect, signon, netchan, sockets, public/LAN networking, Steam, or real clients.

| Symbol or concept | File path | Category | Evidence type | Status | Byte-level evidence | Width/endian/order | Stage | Safe future use | Notes |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `qport` | not found | qport | source search | not found | no | unknown | connect/netchan | no | No local qport field exists to fixture. |
| `client_port` | `src/game_api/hl_server_module.cpp` | endpoint/client source port | source code | diagnostic-only | no | local `int`, host-side only | loopback diagnostics | partial diagnostic context | Local UDP bind result, not a GoldSrc qport field. |
| `LoopbackEndpointIdentityString` | `src/game_api/hl_server_module.cpp` | endpoint key formatter | source code | diagnostic-only | no | text `AF_INET/127.0.0.1:<port>` | getchallenge/connect diagnostics | partial diagnostic prerequisite | Builds endpoint identity from observed source port. |
| `remote_address_source` | `include/game_api/hl_server_module.h`, `src/game_api/hl_server_module.cpp` | remote address provenance | source code | diagnostic-only | no | text summary only | connectionless diagnostics | guard only | Report/provenance field, not a netchan remote address object. |
| `udp_client_address_source` | `include/game_api/hl_server_module.h`, `src/game_api/hl_server_module.cpp` | client endpoint provenance | source code | diagnostic-only | no | text summary only | loopback diagnostics | partial | Records deterministic loopback client endpoint text. |
| `HldsAddressScopedChallengeCacheEntry::endpoint` | `src/game_api/hl_server_module.cpp` | endpoint-scoped challenge key | source code | diagnostic-only | no | string key | getchallenge/connect bridge | yes as diagnostic prerequisite | Keyed by endpoint string, not by qport. |
| `HldsAddressScopedChallengeCacheEntry::value` | `src/game_api/hl_server_module.cpp` | challenge value | source code | diagnostic-only | connectionless text only | string token | getchallenge/connect bridge | yes as diagnostic prerequisite | Does not prove real challenge encoding. |
| `kHldsAddressScopedChallengeCacheMaxEntries` | `src/game_api/hl_server_module.cpp` | challenge cache policy | source code | diagnostic-only | no | integer constant `8` | diagnostic cache | guard only | Diagnostic capacity policy. |
| `kHldsAddressScopedChallengeCacheTtlTicks` | `src/game_api/hl_server_module.cpp` | challenge TTL | source code | diagnostic-only | no | integer constant `3` | diagnostic cache | guard only | Diagnostic tick model, not real time. |
| `challenge_one_shot`, `challenge_replay_detected` | `include/game_api/hl_server_module.h`, `src/game_api/hl_server_module.cpp` | replay/one-shot state | source code | diagnostic-only | no | booleans | connect validation | guard only | Diagnostic one-shot proof, not netchan session proof. |
| `BuildHldsUserinfoValidationPolicyConnectInput` | `src/game_api/hl_server_module.cpp` | connect/userinfo text builder | source code | diagnostic-only | partial connectionless text | text tokens | connect diagnostic | partial diagnostic precondition | Builds `connect protocol=48 challenge=... userinfo=...`; no qport token. |
| `HldsUserinfoValidationPolicy*` | `include/game_api/hl_server_module.h`, `src/game_api/hl_server_module.cpp` | userinfo policy | source code | diagnostic-only | no real admission bytes | text/userinfo limits only | connect diagnostic | yes as diagnostic prerequisite | Policy max bytes=256, max keys=16, key bytes=32, value bytes=64. |
| `session_id` | `src/app/main.cpp`, `src/common/logger.cpp`, prompt-era signon surfaces | session id | source code | app/logging or diagnostic-only | no | string/run identity | logging/pseudo signon | no for netchan | Not a network session id. |
| `netchan_not_started` | `include/game_api/hl_server_module.h`, `src/game_api/hl_server_module.cpp` | netchan blocker | source code | diagnostic-only | no | n/a | post-connect/signon blockers | guard only | Confirms no netchan runtime/session started. |
| `client_not_put_in_server` | `include/game_api/hl_server_module.h`, `src/game_api/hl_server_module.cpp` | admission blocker | source code | diagnostic-only | no | n/a | admission | guard only | Confirms no client admission. |

Counts:
- source_files_scanned_count: 78
- qport_symbols_found_count: 0
- client_port_symbols_found_count: 20
- remote_address_symbols_found_count: 22
- session_symbols_found_count: 22896
- challenge_link_symbols_found_count: 1364
- userinfo_link_symbols_found_count: 790
- netchan_binding_symbols_found_count: 123
