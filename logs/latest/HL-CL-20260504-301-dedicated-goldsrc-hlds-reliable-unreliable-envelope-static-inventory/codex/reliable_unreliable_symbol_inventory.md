# Reliable/Unreliable Symbol Inventory

Prompt: HL-CL-20260504-301-dedicated-goldsrc-hlds-reliable-unreliable-envelope-static-inventory

This inventory is static-only. It did not start netchan, reliable channel, unreliable channel, sockets, clients, post-connect, or signon runtime paths.

| Symbol or concept | File path | Category | Direction | Status | Byte-level evidence | Notes |
| --- | --- | --- | --- | --- | --- | --- |
| `reliable_channel_not_started` | `include/game_api/hl_server_module.h`, `src/game_api/hl_server_module.cpp` | reliable envelope blocker | server->client future only | diagnostic-only | no | Strong guard that reliable channel state is not entered. |
| `netchan_not_started` | `include/game_api/hl_server_module.h`, `src/game_api/hl_server_module.cpp` | netchan blocker | both future directions | diagnostic-only | no | Sequence/ack and reliable/unreliable runtime remain absent. |
| `resource_baselines_not_sent` | `include/game_api/hl_server_module.h`, `src/game_api/hl_server_module.cpp` | baseline blocker | server->client future only | diagnostic-only | no | Baseline/resource messages are still blocked. |
| `MSG_ONE_UNRELIABLE` | `src/game_api/server_frame_loop.cpp` | unreliable label | server callback intent | diagnostic-only | no | Destination label only, not an unreliable datagram envelope. |
| `MSG_ONE`, `MSG_ALL`, `MSG_BROADCAST`, `MSG_INIT` | `src/game_api/server_frame_loop.cpp` | message destination labels | server callback intent | diagnostic-only | no | Callback metadata only. |
| `FrameMessageBuffer` callback observation | `src/game_api/server_frame_loop.cpp`, `src/game_api/hl_server_module.cpp` | message buffer | callback observation | diagnostic-only | partial local observation only | Not a network packet buffer. |
| `DedicatedSignonEnvelope*` summaries | `include/game_api/hl_server_module.h`, `src/game_api/hl_server_module.cpp` | signon envelope surface | server->client future only | diagnostic-only | pseudo/partial only | Prompt-era diagnostic descriptors, not real netchan envelopes. |
| `client_port` | `src/game_api/hl_server_module.cpp` | session-adjacent field | client->server diagnostics | diagnostic-only | no | Useful for later qport/session inventory; no qport symbol found. |
| `remote_address_source` | `include/game_api/hl_server_module.h`, `src/game_api/hl_server_module.cpp` | address source summary | both diagnostics | diagnostic-only | no | Policy/reporting metadata only. |
| `reliable_message`, `unreliable_message`, `reliable_buf`, `unreliable_buf` | not found | reliable/unreliable payload buffer | unknown | not found | no | No explicit reliable/unreliable buffer primitive found. |
| `reliable_sequence`, `reliable_ack`, `incoming_acknowledged`, `outgoing_reliable_sequence` | not found | reliable sequence/ack | unknown | not found | no | No reliable sequence/ack contract found. |
| `fragment`, `split`, `packet_loss` | not found | fragment/flow envelope | unknown | not found | no | Split/fragment behavior remains unknown. |

Counts:
- source_files_scanned_count: 78
- reliable_symbols_found_count: 46
- unreliable_symbols_found_count: 2
- envelope_symbols_found_count: 1163

Result:
- byte_level_envelope_evidence_sufficient=0
- netchan_runtime_started=0
