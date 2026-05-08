# HL-CL-20260504-301 Reliable/Unreliable Envelope Static Inventory

Compatibility claim level: diagnostic-reliable-unreliable-envelope-static-inventory-only; no real Steam Half-Life or HLDS-compatible client compatibility claimed

This document is static/report-only. It does not run getchallenge, connect, post-connect serverinfo, signon serverinfo, auth, netchan, reliable channel, unreliable channel, resource baselines, signon state, admission, public/LAN sockets, Steam, a real Half-Life client, or any real client binary. Unknown byte-level behavior remains unknown.

The closed query/info boundary from prompts 287 through 296 remains separate: it proves only diagnostic connectionless query/info over loopback. It is not post-connect serverinfo, signon-time serverinfo, reliable/unreliable envelope evidence, netchan sequence/ack evidence, or real HLDS/Steam client compatibility evidence.

## Scan Scope

| Item | Result |
| --- | --- |
| Source roots scanned | `include`, `src` |
| Source files scanned | 78 |
| Prompt artifacts/docs consulted | prompts 293, 296, 297, 298, 299, 300 stable docs and artifacts |
| Fixture roots consulted | `fixtures/diagnostic/hlds/serverinfo`, `fixtures/diagnostic/hlds/query_info_regression` |
| Runtime executed | no |
| Sockets opened | no |
| Real clients invoked | no |

## Static Count Summary

| Pattern family | Matches | Files | Interpretation |
| --- | ---: | ---: | --- |
| `reliable` | 46 | 3 | Diagnostic summary/blocker fields and message destination labels; no reliable channel envelope bytes. |
| `unreliable` | 2 | 1 | `MSG_ONE_UNRELIABLE` destination label only; no unreliable datagram envelope bytes. |
| `envelope` | 1163 | 5 | Prompt-era diagnostic signon/envelope surfaces; not a real reliable/unreliable netchan packet envelope. |
| `reliable_message` / `unreliable_message` | 0 | 0 | No explicit reliable/unreliable message buffer symbol found. |
| `reliable_buf` / `unreliable_buf` | 0 | 0 | No reliable/unreliable buffer primitive found. |
| `reliable_length` / `unreliable_length` | 0 | 0 | No payload length field found. |
| `reliable_state` / `reliable_pending` / `reliable_payload` | 0 | 0 | No local reliable state machine fields found. |
| queue terms | 0 | 0 | No message/send/packet queue contract found by exact phrase searches. |

## Reliable/Unreliable Symbol Inventory

| Symbol or concept | File path | Category | Direction | Evidence type | Status | Byte-level evidence | Relation to sequence/ack | Relation to session/userinfo | Relation to signon/serverinfo/baseline | Safe future use | Notes |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `reliable_channel_not_started` | `include/game_api/hl_server_module.h`, `src/game_api/hl_server_module.cpp` | reliable envelope blocker | server->client future only | source code | diagnostic-only | no | proves reliable channel is not started | none | post-connect/signon/baseline blockers | yes as guard only | This is the strongest local reliable-channel fact: it is intentionally absent. |
| `netchan_not_started` | `include/game_api/hl_server_module.h`, `src/game_api/hl_server_module.cpp` | netchan blocker | both future directions | source code | diagnostic-only | no | sequence/ack cannot be active while netchan is blocked | none | all post-connect/signon wire stages blocked | yes as guard only | Confirms no runtime netchan state was entered by diagnostic probes. |
| `resource_baselines_not_sent` | `include/game_api/hl_server_module.h`, `src/game_api/hl_server_module.cpp` | baseline blocker | server->client future only | source code | diagnostic-only | no | unknown | none | baseline/resource stages blocked | yes as guard only | Baseline/resource messages remain unsent and cannot imply signon evidence. |
| `MSG_ONE_UNRELIABLE` | `src/game_api/server_frame_loop.cpp` | unreliable label | server callback intent | source code | diagnostic-only | no | no packet sequence or ack fields | none | game DLL message callback observation only | partial for observation | A destination label, not an unreliable datagram envelope. |
| `MSG_ONE`, `MSG_ALL`, `MSG_BROADCAST`, `MSG_INIT` | `src/game_api/server_frame_loop.cpp` | message destination labels | server callback intent | source code | diagnostic-only | no | no packet sequence or ack fields | none | callback observation, not wire placement | partial for observation | These labels help classify callback intent but do not define channel bytes. |
| `FrameMessageBuffer` callback observation | `src/game_api/server_frame_loop.cpp`, `src/game_api/hl_server_module.cpp` | message buffer | server callback observation | source code | diagnostic-only | partial local bytes only | no sequence/ack envelope | none | message callback summaries only | partial for future fixture observation | Useful for observing writes, not a network packet buffer. |
| `DedicatedSignonEnvelope*` summaries | `include/game_api/hl_server_module.h`, `src/game_api/hl_server_module.cpp`, `src/app/launch_options.cpp` | signon envelope surface | server->client future only | source code | diagnostic-only | pseudo/partial only | no real sequence/ack | signon session fields are prompt-era diagnostics | signon report surface only | no for real envelope | Prompt-era signon envelope surfaces are diagnostic descriptors, not real netchan envelopes. |
| `signon_envelope_ready` | `include/game_api/hl_server_module.h`, `src/game_api/hl_server_module.cpp` | signon readiness field | server->client future only | source code | diagnostic-only | no | no sequence/ack | readiness only | signon descriptor surfaces | guard only | Repeated summary field; not a byte-level envelope. |
| `client_port` | `src/game_api/hl_server_module.cpp` | session/address-adjacent field | client->server diagnostics | source code | diagnostic-only | no | no qport contract | local UDP diagnostic bind/target metadata | no signon envelope link | partial for later qport inventory | `qport` was not found as a source symbol. |
| `remote_address_source` | `include/game_api/hl_server_module.h`, `src/game_api/hl_server_module.cpp` | address source summary | both diagnostics | source code | diagnostic-only | no | no sequence/ack | address policy summaries | no signon envelope link | guard only | Useful for loopback/public/LAN policy reporting, not packet envelope evidence. |
| `connect_datagram_*`, `serverinfo_response_datagram_sent` | `include/game_api/hl_server_module.h`, `src/game_api/hl_server_module.cpp` | connectionless datagram diagnostics | client/server diagnostics | source code | diagnostic-only | connectionless only | no netchan sequence/ack | challenge/connect summaries | post-connect remains blocked | guard only | Datagram counters do not imply reliable or unreliable netchan envelope bytes. |
| `reliable_message`, `unreliable_message`, `reliable_buf`, `unreliable_buf` | not found | reliable/unreliable envelope | unknown | static search | not found | no | none | none | none | no | No explicit reliable/unreliable payload buffers found. |
| `reliable_sequence`, `reliable_ack`, `incoming_acknowledged`, `outgoing_reliable_sequence` | not found | reliable sequence/ack | unknown | static search | not found | no | none | none | none | no | No local reliable sequence/ack state contract found. |
| `fragment`, `split`, `packet_loss` | not found | fragment/flow envelope | unknown | static search | not found | no | none | none | none | no | Split/fragment packet behavior remains unknown. |

## Envelope Candidate Matrix

| Candidate envelope | Local symbols/evidence | Byte-level evidence sufficient? | Sequence dependency | Ack dependency | Reliable/unreliable dependency | Qport/session dependency | Message writer dependency | Stage | Blocker | Smallest safe next task |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Reliable server message envelope | `reliable_channel_not_started`, signon descriptor surfaces | no | unknown outgoing sequence | unknown ack/reliable ack | reliable channel absent | qport/session unknown | field writers only diagnostic | netchan/signon | no real reliable envelope | qport/session binding inventory, then envelope contract only with evidence |
| Unreliable server datagram envelope | `MSG_ONE_UNRELIABLE` label only | no | unknown packet sequence | unknown ack | unreliable packet placement absent | qport/session unknown | destination labels only | netchan/unreliable | label is not packet bytes | qport/session binding inventory |
| Client command envelope | connect/userinfo diagnostic summaries only | no | unknown client sequence | unknown server ack | client command channel absent | challenge/session/qport unknown | no command writer | post-connect/netchan | no client command envelope | qport/session binding inventory |
| Ack-only envelope | no local source symbol | no | unknown | unknown | unknown | unknown | none | netchan | no ack field evidence | sequence/ack fixture contract only after envelope evidence |
| Split/fragment envelope | no fragment/split symbols | no | unknown fragment sequence | unknown | fragment channel absent | unknown | none | netchan | fragment format absent | fragment policy inventory after envelope basics |
| Signon message envelope | prompt-era signon envelope summaries | no | unknown | unknown | likely reliable but unproven | signon session unknown | pseudo/diagnostic helpers only | signon | pseudo signon is not real bytes | signon field contract only after envelope evidence |
| Post-connect serverinfo envelope | preview/unresolved serverinfo artifacts | no | unknown | unknown | unknown channel placement | challenge/connect/session unknown | no real serverinfo writer | post-connect | no first server packet envelope | evidence acquisition/capture policy later |
| Baseline/resource envelope | `resource_baselines_not_sent`, baseline readiness fields | no | unknown | unknown | likely reliable but unproven | signon/session unknown | no baseline writer | baseline/resource | baseline envelope absent | baseline/resource envelope inventory later |
| Reject/disconnect envelope | rejection reason summaries, connectionless text helpers | no | unknown post-connect envelope | unknown | may be connectionless or netchan depending stage | challenge/session dependent | string helper only in diagnostics | reject/disconnect | post-connect reject bytes absent | reject/disconnect contract inventory later |

## Channel Placement Inventory

| Message type | Channel placement | Local evidence | Byte-level evidence status | Fixture-backed now? | Builder-backed now? | Forbidden assumptions | Next safe action |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Connectionless query/info | connectionless | prompts 287-296, query/info CI manifest | sufficient for diagnostic query/info only | yes | yes, diagnostic-only | not post-connect/signon | keep closed boundary and drift gate |
| getchallenge | connectionless diagnostic | earlier prompt summaries and datagram counters | diagnostic only | partial | diagnostic only | not a netchan envelope | keep separate from envelope work |
| connect request | connectionless diagnostic/pre-netchan | connect datagram summaries | diagnostic only | partial | diagnostic only | not post-connect serverinfo | qport/session inventory before netchan assumptions |
| Post-connect serverinfo | unknown netchan placement | unresolved fixtures and preview summaries | insufficient | no | no | do not infer from query/info | envelope evidence acquisition later |
| Signon-time serverinfo | unknown signon/netchan placement | prompt-era signon descriptor surfaces | insufficient | no | no | do not treat pseudo signon as real bytes | signon field contract after envelope evidence |
| Resource list | unknown, likely signon/baseline stream but unproven | `resource_baselines_not_sent` | insufficient | no | no | do not invent reliable placement | baseline/resource envelope inventory later |
| Model baseline | unknown | baseline readiness fields | insufficient | no | no | no model baseline bytes locally proven | baseline/resource envelope inventory later |
| Sound baseline | unknown | baseline readiness fields | insufficient | no | no | no sound baseline bytes locally proven | baseline/resource envelope inventory later |
| Event baseline | unknown | baseline readiness fields | insufficient | no | no | no event baseline bytes locally proven | baseline/resource envelope inventory later |
| Reject/disconnect | connectionless or netchan depending stage, unknown | rejection summaries only | insufficient for post-connect | no | no for real stage | connectionless reject text does not prove post-connect envelope | reject/disconnect inventory later |
| Client command / usercmd | unknown netchan | no usercmd envelope found in focused search | insufficient | no | no | do not invent command channel | qport/session and sequence/ack inventory later |
| Heartbeat/status query | connectionless if present, not part of query/info boundary here | no focused local status/heartbeat envelope proof | insufficient | no | no | do not add public query behavior | separate policy prompt if needed |

## Envelope To Message-Writer Mapping

| Envelope candidate | Required primitives | Helper found? | Helper confidence | Missing primitive | Missing policy | Risk level |
| --- | --- | --- | --- | --- | --- | --- |
| Reliable server message envelope | sequence/ack fields, reliable flag, payload length, byte/string writers | payload writers partially observed only | low | sequence, ack, reliable bit, length, resend buffer | ordering, width, overflow, retransmit | critical |
| Unreliable server datagram envelope | sequence/ack fields, unreliable payload placement, byte/string writers | `MSG_ONE_UNRELIABLE` label only | low | packet header, payload boundary | ordering, width, overflow | critical |
| Client command envelope | client sequence, qport/session, command payload writers | no real helper | low | qport, command payload, ack relation | client command routing | critical |
| Ack-only envelope | sequence/ack header only | no | low | ack field and empty payload rule | empty-packet policy | high |
| Split/fragment envelope | fragment id/count/offset, payload chunk writer | no | low | all fragment fields | split threshold/reassembly | high |
| Signon message envelope | reliable envelope plus `svc_serverinfo` fields | pseudo helpers only | low | real message id/framing | reliable placement, field ordering | critical |
| Post-connect serverinfo envelope | first server packet envelope plus serverinfo fields | preview only | low | first packet header and message id | stage placement | critical |
| Baseline/resource envelope | baseline/resource message ids and payload writers | no | low | baseline ids and field writers | reliable/baseline ordering | high |
| Reject/disconnect envelope | reason string writer plus stage envelope | connectionless text helper only | medium for text, low for envelope | post-connect reject envelope | stage-specific disconnect policy | high |

## Envelope Dependency On Sequence/Ack

| Dependency | Local evidence | Field known? | Order known? | Width known? | Byte-level known? | Safe next action |
| --- | --- | --- | --- | --- | --- | --- |
| Outgoing sequence | no real netchan symbol | no | no | no | no | do not define until envelope/session evidence exists |
| Incoming sequence | no real netchan symbol | no | no | no | no | do not define until client command envelope evidence exists |
| Incoming ack | no exact ack source symbol | no | no | no | no | keep unknown; avoid pseudo ack fields |
| Reliable sequence | no `reliable_sequence` symbol | no | no | no | no | inventory qport/session then decide fixture feasibility |
| Reliable ack | no `reliable_ack` symbol | no | no | no | no | inventory qport/session then decide fixture feasibility |
| Fragment counters | no fragment/split symbols | no | no | no | no | leave out of any initial fixture contract |
| Qport/client port | diagnostic `client_port`, no `qport` | partial local port only | no | local int only | no | qport/session binding static inventory |
| Challenge/session binding | challenge/cache/userinfo diagnostics exist | partial conceptual | no | no | no | bridge diagnostics without claiming real netchan proof |

## Stage-Specific Blocking Review

| Stage | Current evidence | Current status | Byte-level evidence sufficient? | Builder allowed now? | Parser allowed now? | Real compatibility claim allowed? | Blocking result |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Connectionless query/info | prompts 287-296 | closed diagnostic boundary | yes for query/info only | yes, diagnostic-only | yes, diagnostic-only | no | separate from netchan envelope work |
| getchallenge/connect diagnostics | earlier diagnostic prompts | diagnostic-only | partial and stage-limited | diagnostic-only | diagnostic-only | no | not a reliable/unreliable envelope |
| Post-connect serverinfo | preview/unresolved fixtures | blocked | no | no | no | no | `post_connect_byte_evidence_sufficient=0` |
| Signon-time serverinfo | pseudo signon descriptors | blocked | no | no | no | no | `signon_time_byte_evidence_sufficient=0` |
| Reliable/unreliable channel | blocker fields only | not started | no | no | no | no | `reliable_channel_not_started=1`, no envelope bytes |
| Baseline/resource | readiness/blocker fields only | not sent | no | no | no | no | `resource_baselines_not_sent=1` |
| Real client capture | forbidden by prompt policy | blocked | no | no | no | no | `real_client_capture_allowed_now=0` |
| Public/LAN exposure | forbidden by prompt policy | blocked | no | no | no | no | no sockets opened |

## Unknowns And Forbidden Assumptions

| Unknown | Why unknown | Local evidence if any | Why it must not be invented | Safe way to resolve later |
| --- | --- | --- | --- | --- |
| Reliable envelope header/order | no real reliable packet writer | blocker fields only | wrong order corrupts all downstream packets | byte fixture or approved capture policy |
| Unreliable envelope header/order | `MSG_ONE_UNRELIABLE` is only a label | destination label | callback destination is not wire routing | envelope fixture contract only with evidence |
| Ack field position | no ack source symbol | none | ack placement determines packet parse | sequence/ack evidence prompt |
| Reliable bit/flag position | no reliable sequence/ack fields | blocker fields only | flag position affects packet state | reliable envelope evidence |
| Fragment envelope format | no fragment/split symbols | none | split packet assumptions can alter framing | fragment policy prompt |
| Split packet envelope format | no split packet symbols | none | split behavior may use separate markers | fragment/split inventory |
| First post-connect server packet envelope | no real bytes | preview/unresolved serverinfo only | stage confusion risk | post-connect byte fixture acquisition |
| First signon serverinfo envelope | pseudo signon only | signon descriptor surfaces | pseudo bytes are not real signon bytes | signon byte fixture acquisition |
| Reject/disconnect envelope | connectionless text only | rejection summaries | post-connect disconnect may be netchan-wrapped | reject/disconnect contract inventory |
| Baseline/resource envelope | no baseline writer | `resource_baselines_not_sent` | baseline order and channel are stage-critical | baseline/resource envelope inventory |
| Qport/session binding | no `qport` symbol; local `client_port` only | diagnostic address/port summaries | qport affects client/server packet acceptance | qport/session binding static inventory |
| Overflow behavior | no central sizebuf/overflow contract | diagnostic buffer observation only | overflow policy changes byte contract safety | message-writing policy fixture contract |

## Blocker Ranking

| Rank | Blocker | Severity | Reason | Smallest safe next task | Runtime needed? | Real client needed? | Public/LAN needed? |
| ---: | --- | --- | --- | --- | --- | --- | --- |
| 1 | missing reliable/unreliable envelope byte contract | critical | no real reliable or unreliable channel packet envelope was found | qport/session binding static inventory as the next narrow dependency | no | no | no |
| 2 | missing sequence/ack byte contract | critical | no outgoing/incoming sequence or ack field contract exists | sequence/ack fixture contract only after envelope/session evidence | no | no | no |
| 3 | missing qport/session binding | high | only diagnostic `client_port` and address summaries exist; no `qport` | qport/session binding static inventory | no | no | no |
| 4 | missing post-connect serverinfo envelope | high | unresolved post-connect fixtures remain not buildable | post-connect envelope contract after channel evidence | no | no | no |
| 5 | missing signon serverinfo envelope | high | pseudo signon surfaces are not real signon bytes | signon envelope/field contract after channel evidence | no | no | no |
| 6 | missing baseline/resource envelope | high | baseline/resource messages remain unsent | baseline/resource envelope inventory | no | no | no |
| 7 | missing message writer policy | medium | diagnostic writers exist but no real network sizebuf/overflow policy | message-writing policy fixture contract | no | no | no |
| 8 | missing reject/disconnect envelope | medium | rejection summaries do not define post-connect disconnect bytes | reject/disconnect static contract inventory | no | no | no |
| 9 | real client capture forbidden | policy blocker | prompt forbids Steam/client execution | separate explicit policy boundary only | yes later | yes later | no |
| 10 | public/LAN exposure forbidden | policy blocker | prompt forbids non-loopback/public/LAN sockets | keep blocked; no public/LAN needed for static work | no | no | no |

## Recommended Next Prompt

Recommended next prompt:

`HL-CL-20260504-302-dedicated-goldsrc-hlds-qport-session-binding-static-inventory`

Rationale: this inventory found reliable/unreliable envelope evidence is still insufficient, and the most concrete narrow dependency now visible is qport/session binding. Local code has diagnostic `client_port`, address-source, challenge, and userinfo summaries, but no `qport` source symbol or byte-level session binding. Inventorying that bridge is safer than defining sequence/ack or envelope fixture contracts from missing packet bytes.
