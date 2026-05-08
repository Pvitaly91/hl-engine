# HL-CL-20260504-300 Netchan Sequence/Ack Static Contract Inventory

Prompt ID: HL-CL-20260504-300-dedicated-goldsrc-hlds-netchan-sequence-ack-static-contract-inventory

Compatibility claim level: diagnostic-netchan-sequence-ack-static-contract-inventory-only; no real Steam Half-Life or HLDS-compatible client compatibility claimed

This inventory is static and report-only. It does not run runtime probes, does not capture packets, does not open sockets, does not invoke Steam, does not invoke a real client binary, does not run getchallenge/connect/post-connect/signon runtime paths, and does not implement a wire builder or parser.

Unknown byte-level behavior remains unknown. The connectionless query/info diagnostic boundary remains closed and must not be reused as post-connect serverinfo, signon-time serverinfo, netchan sequence/ack evidence, reliable/unreliable envelope evidence, or real compatibility evidence.

## Scan Scope

Primary source scan scope:

- `include/`
- `src/`

Context-only scan scope:

- `fixtures/diagnostic/hlds/serverinfo/`
- `fixtures/diagnostic/hlds/query_info_regression/`
- `docs/diagnostic/hlds/`
- `scripts/run_hlds_query_info_regression.ps1`
- prompt 293, 296, 297, 298, and 299 artifacts

Static source scan observations:

| Category | Source matches | Files | Contract relevance |
| --- | ---: | ---: | --- |
| netchan terms | 246 | 2 | Mostly blocker/report fields such as `netchan_not_started`; no real channel object. |
| broad sequence terms | 1406 | 13 | Mostly bootstrap/checkpoint/signon descriptor text and unrelated local ordering fields. |
| exact ack terms | 0 | 0 | No source `ack`, `acknowledge`, `incoming_acknowledged`, or `reliable_ack` symbol was found with word-boundary search. |
| reliable sequence terms | 0 | 0 | No `incoming_reliable_sequence`, `outgoing_reliable_sequence`, or `reliable_sequence` symbol found. |
| unreliable terms | 2 | 1 | Message destination naming only. |
| qport/client port terms | 20 | 1 | Diagnostic loopback `client_port` variables only; no GoldSrc qport contract. |
| fragment/packet loss/split terms | 0 | 0 | No fragment/split-packet contract found. |
| flow terms | 332 | 1 | Prompt-era signon stream/checkpoint text, not netchan flow control. |

## Netchan Sequence/Ack Symbol Inventory

| Symbol or term | File | Category | Direction | Evidence type | Status | Byte-level evidence | Field width | Endian | Stage | Upstream dependencies | Downstream dependents | Safe future use | Notes |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `netchan_not_started` | `include/game_api/hl_server_module.h`, `src/game_api/hl_server_module.cpp` | unknown/blocker | none | source code | diagnostic-only | no | n/a | n/a | post-connect/signon blockers | connect/session diagnostics | serverinfo, signon, baselines | yes as blocker | Marks that real netchan is intentionally absent. |
| `reliable_channel_not_started` | `include/game_api/hl_server_module.h`, `src/game_api/hl_server_module.cpp` | reliable/blocker | none | source code | diagnostic-only | no | n/a | n/a | post-connect/signon blockers | netchan evidence | signon messages, baselines | yes as blocker | No reliable sequence, reliable ack, resend, or buffer contract accompanies it. |
| `resource_baselines_not_sent` | `include/game_api/hl_server_module.h`, `src/game_api/hl_server_module.cpp` | baseline/blocker | server->client in future only | source code | diagnostic-only | no | n/a | n/a | signon/baseline blockers | signon envelope | client admission | yes as blocker | Baseline/resource messages remain unsent. |
| `remote_address_source` | `include/game_api/hl_server_module.h`, `src/game_api/hl_server_module.cpp` | address | unknown | source code | diagnostic-only | no | n/a | n/a | connectionless/connect diagnostics | diagnostic loopback inputs | challenge/session policy | partial | Useful for diagnostic provenance, not a real netchan remote address binding. |
| `client_port` | `src/game_api/hl_server_module.cpp` | address/qport-adjacent | diagnostic client->server | source code | diagnostic-only | no | local `int` | n/a | loopback diagnostics | loopback socket helpers | diagnostic client endpoint summaries | partial | This is a local UDP port variable, not a GoldSrc `qport` field. |
| `qport` | not found | qport | unknown | source search | not found | no | unknown | unknown | netchan/connect | unknown | sequence/ack binding | no | No local qport contract exists. |
| `DedicatedSignonWiremapSequenceId` / `sequence_id` | `src/game_api/hl_server_module.cpp` | sequence | server diagnostic report | source code | diagnostic-only | partial pseudo bytes | 16-bit in pseudo helper | little-endian via `AppendLittleEndianShort` | pseudo signon report | pseudo signon wiremap | pseudo signon report parsing | no for real netchan, partial for report history | This is not a real netchan sequence field. |
| `parsed_sequence_ids` | `src/game_api/hl_server_module.cpp` | sequence | diagnostic parser/report | source code | diagnostic-only | no | text/integer parse | n/a | pseudo signon report | pseudo signon wiremap text | pseudo signon validation | partial for report history | Parses pseudo diagnostic sequence ids only. |
| `MSG_ONE_UNRELIABLE` | `src/game_api/server_frame_loop.cpp` | unreliable label | server callback intent | source code | diagnostic-only | no | n/a | n/a | message callback observation | game DLL message callback | observed message summaries | partial for observation | A destination label, not an unreliable datagram envelope. |
| `MSG_ONE`, `MSG_ALL`, `MSG_BROADCAST`, `MSG_INIT` | `src/game_api/server_frame_loop.cpp` | message destination label | server callback intent | source code | diagnostic-only | no | n/a | n/a | message callback observation | game DLL message callback | observed message summaries | partial for observation | These labels do not establish channel sequence or ack behavior. |
| `MapLogicQueuedAction::sequence` / `next_sequence_` | `src/game_api/map_logic_dispatcher.h`, `src/game_api/map_logic_dispatcher.cpp` | sequence | internal ordering | source code | production-like local logic | no | `std::uint64_t` | n/a | map logic | local action queue | map logic dispatch order | no for netchan | Unrelated to network packet sequence. |
| `SpawnTrace::sequence` | `src/game_api/spawn_trace.h` | sequence | internal ordering | source code | diagnostic/trace | no | `std::size_t` | n/a | spawn trace | local spawn instrumentation | reports | no for netchan | Unrelated to network packet sequence. |
| message `channel` argument | `src/game_api/hl_server_module.cpp` | channel | callback/report | source code | diagnostic-only | no | local `int` | n/a | sound/message diagnostics | engine callback observation | report strings | no for netchan | Not a network channel object. |
| `fragment`, `packet_loss`, `split` | not found | fragment/flow | unknown | source search | not found | no | unknown | unknown | netchan | unknown | split/fragment handling | no | No local fragment or packet-loss contract found. |

## Sequence/Ack Candidate Contract Matrix

| Candidate field | Likely role | Local symbol/evidence | Byte-level evidence sufficient? | Field width known? | Endian known? | Ordering known? | Required for post-connect serverinfo? | Required for signon-time serverinfo? | Blocker | Smallest safe next task |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| outgoing sequence | server packet sequence | no local netchan symbol | no | no | no | no | likely yes | likely yes | no channel envelope | reliable/unreliable envelope static inventory |
| incoming sequence | client packet sequence | no local netchan symbol | no | no | no | no | likely yes | likely yes | no channel envelope | reliable/unreliable envelope static inventory |
| incoming ack | acknowledge received packets | no local source symbol | no | no | no | no | likely yes | likely yes | no ack field evidence | reliable/unreliable envelope static inventory |
| reliable sequence | reliable stream state | no local source symbol | no | no | no | no | maybe | likely yes | reliable channel absent | reliable/unreliable envelope static inventory |
| reliable ack | reliable stream acknowledgement | no local source symbol | no | no | no | no | maybe | likely yes | reliable ack absent | reliable/unreliable envelope static inventory |
| unreliable datagram | non-reliable payload envelope | `MSG_ONE_UNRELIABLE` label only | no | no | no | no | maybe | maybe | only callback destination name exists | reliable/unreliable envelope static inventory |
| fragment state | split packet handling | not found | no | no | no | no | unknown | unknown | fragment contract absent | envelope inventory, then fragment policy if needed |
| qport/client port | client port binding | diagnostic `client_port`, no `qport` | no | local int only | n/a | n/a | likely connect/netchan dependency | likely yes | qport absent | qport/session binding static inventory after envelope |
| remote address binding | endpoint binding | `remote_address_source` summaries | no | n/a | n/a | n/a | yes for real session | yes | diagnostic provenance only | qport/session binding static inventory |
| challenge/session binding | connect/session provenance | challenge cache diagnostics and session ids | no | n/a | n/a | partial diagnostic order | yes | yes | not a netchan proof | bridge audit plus qport/session inventory |
| signon state binding | staged signon report state | prompt-era signon descriptor surfaces | no real bytes | no | no | pseudo only | maybe | yes | pseudo signon not real signon | envelope inventory before field contract |
| reliable message buffer | reliable payload storage | `reliable_channel_not_started` only | no | no | no | no | maybe | yes | no reliable buffer | reliable/unreliable envelope static inventory |
| unreliable message buffer | unreliable payload storage | destination label only | no | no | no | no | maybe | maybe | no unreliable buffer | reliable/unreliable envelope static inventory |

## Reliable/Unreliable Envelope Static Inventory

| Envelope concept | Found? | Symbol/file | Byte-level evidence | Relation to sequence/ack | Relation to serverinfo/signon/baselines | Missing fields | Safe next action |
| --- | --- | --- | --- | --- | --- | --- | --- |
| reliable server-to-client messages | no | `reliable_channel_not_started` only | no | no sequence or reliable ack | likely required for signon/baselines | sequence, ack, reliable bit, buffer, resend | inventory reliable/unreliable envelope terms next. |
| unreliable server-to-client datagrams | partial label only | `MSG_ONE_UNRELIABLE` | no | no sequence/ack | maybe used for some game messages, not proven for serverinfo | packet envelope, payload placement | inventory envelope/static channel names. |
| client-to-server commands | no real channel found | diagnostic connect/userinfo summaries only | no | no incoming sequence | connect/userinfo diagnostics only | command envelope, qport, ack fields | qport/session binding after envelope. |
| ack-only packets | no | none | no | no ack field | unknown | ack field, packet type, empty payload behavior | envelope inventory. |
| split/fragmented packets | no | none | no | no fragment sequence | unknown | split markers, fragment index/count | envelope inventory, fragment policy later. |
| message queue flush | no | no netchan queue found | no | unknown | signon/baseline delivery blocked | queue semantics, flush order | envelope inventory. |
| channel init | no | `netchan_not_started` blockers | no | all unknown | post-connect and signon blocked | init order, initial sequence values | envelope inventory. |
| channel reset | no | none | no | unknown | disconnect/reconnect unknown | reset state, replay behavior | later policy. |
| channel close/disconnect | no real envelope | connectionless reject/diagnostic summaries only | no | unknown | reject/disconnect post-connect unknown | close packet bytes, reason string policy | later reject/disconnect contract. |

## Post-Connect/Signon Dependency Mapping

| Dependent area | Sequence/ack dependency | Reliable/unreliable dependency | Message writer dependency | Challenge/session/userinfo dependency | Current local evidence | Unresolved gaps | Next safe task |
| --- | --- | --- | --- | --- | --- | --- | --- |
| post-connect serverinfo evidence | first server packet envelope likely needed | unknown reliable/unreliable placement | message id and field writers unknown | challenge/connect/userinfo diagnostics exist | preview and unresolved fixtures only | packet header, sequence, ack, message id, field order | reliable/unreliable envelope static inventory |
| signon-time serverinfo evidence | signon envelope likely needed | reliable stream likely but unproven | `svc_serverinfo` field writers unknown | session linkage and signon state unknown | pseudo signon report surfaces only | real signon packet envelope and state transition | reliable/unreliable envelope static inventory |
| resource/model/sound/event baselines | likely after signon state sequencing | likely reliable/baseline stream | baseline writer absent | map/session/precache linkage exists but not wire | `resource_baselines_not_sent` blocker | baseline message ids and payload encoding | baseline/resource inventory after envelope |
| client spawn/put-in-server | depends on accepted session and signon progression | unknown | admission path not a wire proof | diagnostic put-in-server exists | diagnostics report admission is blocked for real client | real client admission sequence | no runtime until explicit policy |
| reject/disconnect messaging | may be connectionless or netchan depending stage | unknown post-connect envelope | string/reason writer unknown | challenge/session policy can reject preauth | connectionless text/reject summaries only | post-connect reject envelope and timeout behavior | reject/disconnect byte contract later |

## Challenge/Session/Userinfo Bridge Audit

| Diagnostic area | Local representation | Reusable as diagnostic prerequisite? | Reusable as real netchan proof? | Stage-confusion risk | Required guard |
| --- | --- | --- | --- | --- | --- |
| address-scoped challenge cache | challenge cache key, TTL, one-shot, replay detection summaries | yes | no | challenge acceptance can be mistaken for channel start | keep `netchan_not_started=1`. |
| challenge one-shot/replay protection | `challenge_consumed`, replay and endpoint mismatch summaries | yes | no | replay rejection can be mistaken for packet ack | keep ack unknown. |
| userinfo validation | userinfo policy diagnostics and unsafe input gates | yes | no | validated userinfo can be mistaken for post-connect readiness | keep post-connect builder blocked. |
| connect readiness | diagnostic connect accept/reject summaries | partial | no | connect diagnostics can be mistaken for real session channel | keep real client and netchan blocked. |
| serverinfo diagnostic readiness | preview/unresolved serverinfo summaries | report-only | no | preview can be mistaken for byte-level post-connect evidence | evidence-gap guard remains required. |
| query/info boundary | closed fixture-backed connectionless query/info | yes for connectionless query/info only | no | query/info response can be promoted to serverinfo incorrectly | query/info stage separation and drift gate remain required. |

## Unknowns And Forbidden Assumptions

| Unknown | Why unknown | Local evidence | Why it must not be invented | Safe way to resolve later |
| --- | --- | --- | --- | --- |
| exact sequence field width | no real netchan sequence symbol or packet capture | pseudo `sequence_id` is 16-bit only for diagnostic wiremap | pseudo width may not match real netchan | envelope inventory, then explicit evidence prompt. |
| exact ack field width | no ack source symbol | no exact ack token in source | wrong width corrupts packet contract | envelope inventory or external fixture ingestion policy. |
| reliable bit/flag location | no reliable envelope | `reliable_channel_not_started` only | flag position affects every packet | reliable/unreliable envelope inventory. |
| qport handling | no `qport` source symbol | loopback `client_port` variables only | local UDP port is not a qport proof | qport/session binding static inventory. |
| fragment handling | no fragment/split terms | none | fragment policy changes packet format | fragment policy inventory after envelope. |
| channel init order | no real channel init | blockers say netchan not started | start order affects first post-connect packet | no-client design only after envelope contract. |
| first post-connect server packet envelope | no real bytes | preview/unresolved fixtures only | stage confusion risk | byte fixture/evidence prompt. |
| first signon serverinfo envelope | no real bytes | pseudo signon only | pseudo report markers are not real signon | byte fixture/evidence prompt. |
| reliable/unreliable split | only destination labels | `MSG_ONE_UNRELIABLE` label | callback destination is not packet routing | envelope inventory. |
| overflow behavior | no central sizebuf/overflow | prompt 299 inventory | overflow policy affects packet safety | message-writing policy fixture contract. |
| timeout/disconnect behavior | no real channel runtime | connectionless diagnostics only | timeouts differ by stage | later policy and bounded diagnostic design. |
| reject message envelope | no post-connect reject bytes | connectionless text helpers only | wrong envelope changes client behavior | reject/disconnect contract inventory. |

## Blocker Ranking

| Rank | Blocker | Severity | Reason | Smallest safe next task | Runtime needed? | Real client needed? | Public/LAN exposure needed? |
| ---: | --- | --- | --- | --- | --- | --- | --- |
| 1 | missing reliable/unreliable envelope contract | critical | no local channel envelope exists, so sequence/ack placement cannot be defined | reliable/unreliable envelope static inventory | no | no | no |
| 2 | missing sequence/ack byte contract | critical | no real outgoing/incoming sequence or ack fields found | envelope inventory, then fixture contract only if evidence exists | no | no | no |
| 3 | missing qport/session binding | high | no `qport`; only diagnostic `client_port` and challenge/session summaries | qport/session binding static inventory | no | no | no |
| 4 | missing signon envelope | high | pseudo signon helpers are diagnostic-only | signon envelope inventory after channel envelope | no | no | no |
| 5 | missing post-connect serverinfo envelope | high | preview/unresolved serverinfo is not byte evidence | serverinfo envelope inventory after channel envelope | no | no | no |
| 6 | missing baseline/resource envelope | high | baseline/resource sending remains blocked | baseline/resource message static inventory | no | no | no |
| 7 | missing message writer policy | medium-high | no central sizebuf/overflow/endian policy | message-writing policy fixture contract | no | no | no |
| 8 | real client capture forbidden | policy critical | no approval for Steam or real client | capture policy prompt only later | not now | later only | no |
| 9 | public/LAN exposure forbidden | policy critical | diagnostic scope remains loopback/report-only | public/LAN blocker if ever needed | no | no | no |
| 10 | real compatibility claim forbidden | policy critical | no real HLDS or Steam client proof exists | maintain diagnostic claim limit | no | no | no |

## Recommendation

Recommended next prompt:

HL-CL-20260504-301-dedicated-goldsrc-hlds-reliable-unreliable-envelope-static-inventory

Rationale: local source does not contain enough sequence/ack evidence to define even a diagnostic sequence/ack fixture contract. The next safe narrowing step is to inventory reliable/unreliable envelope concepts and channel placement, because that determines where sequence and ack fields would live before post-connect or signon-time serverinfo contracts can be designed.
