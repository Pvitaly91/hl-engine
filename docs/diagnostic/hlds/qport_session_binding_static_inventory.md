# HL-CL-20260504-302 Qport/Session Binding Static Inventory

Compatibility claim level: diagnostic-qport-session-binding-static-inventory-only; no real Steam Half-Life or HLDS-compatible client compatibility claimed

This inventory is static/report-only. It does not run runtime probes, capture packets, implement a wire builder/parser, open sockets, invoke Steam, invoke a real client binary, run getchallenge/connect/post-connect/signon paths, start auth, start netchan, start reliable/unreliable channels, emit resource baselines, enter signon state, or admit a client. Unknown byte-level behavior remains unknown.

The query/info boundary from prompts 287 through 296 remains closed and separate. Connectionless query/info is not qport/session proof, not post-connect serverinfo, not signon-time serverinfo, not netchan proof, and not real compatibility evidence.

## Scan Scope

| Item | Result |
| --- | --- |
| Source roots scanned | `include`, `src` |
| Source files scanned | 78 |
| Prior docs/artifacts consulted | prompts 269, 270, 271, 293, 296, 297, 298, 299, 300, 301 |
| Runtime executed | no |
| Sockets opened | no |
| Real clients invoked | no |

## Static Count Summary

| Pattern family | Matches | Files | Interpretation |
| --- | ---: | ---: | --- |
| `qport` | 0 | 0 | No local GoldSrc qport source symbol or byte contract found. |
| `client_port` | 20 | 1 | Diagnostic loopback UDP client port variables only. |
| `remote_address` | 22 | 2 | Diagnostic summary/provenance fields such as `remote_address_source`. |
| `endpoint` | 132 | 3 | Mostly diagnostic endpoint cache and loopback identity text. |
| `session` | 22896 | 6 | Very noisy; mostly prompt-era signon/session descriptors and logger session IDs, not qport. |
| `session_id` | 5803 | 5 | Logger/app run identity and prompt-era signon descriptors, not netchan session binding. |
| `challenge` | 1364 | 5 | Diagnostic challenge/cache/connect policy surfaces. |
| `challenge cache` | 6 | 1 | Source summary text for the address-scoped challenge cache. |
| `address-scoped` | 19 | 2 | Prompt 269 diagnostic cache naming. |
| `userinfo` | 790 | 5 | Diagnostic userinfo parsing and policy surfaces. |
| `netchan` | 123 | 2 | Blocker/report fields, not a real channel object. |
| exact `NAT` / `port remap` | 0 | 0 | No NAT/qport remap policy found. |

## Qport/Session Symbol Inventory

| Symbol or concept | File path | Category | Evidence type | Status | Byte-level evidence | Width/endian/order | Stage | Upstream dependencies | Downstream dependents | Safe future use | Notes |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `qport` | not found | qport | source search | not found | no | unknown | connect/netchan | unknown | sequence/ack, envelope | no | No local qport field exists to fixture yet. |
| `client_port` | `src/game_api/hl_server_module.cpp` | endpoint/client source port | source code | diagnostic-only | no | local `int`, host-side only | loopback diagnostics | loopback socket bind helpers | endpoint identity, challenge cache diagnostics | partial as diagnostic context | Local UDP bind result, not a GoldSrc qport field. |
| `LoopbackEndpointIdentityString` | `src/game_api/hl_server_module.cpp` | endpoint key formatter | source code | diagnostic-only | no | text `AF_INET/127.0.0.1:<port>` | getchallenge/connect diagnostics | sockaddr source port | address-scoped cache | partial as diagnostic prerequisite | Builds endpoint identity from observed source port. |
| `remote_address_source` | `include/game_api/hl_server_module.h`, `src/game_api/hl_server_module.cpp` | remote address provenance | source code | diagnostic-only | no | text summary only | connectionless diagnostics | loopback policy | summaries/reports | guard only | Useful for proof reporting, not a netchan remote address object. |
| `udp_client_address_source` | `include/game_api/hl_server_module.h`, `src/game_api/hl_server_module.cpp` | client endpoint provenance | source code | diagnostic-only | no | text summary only | loopback diagnostics | loopback socket bind helpers | address cache/userinfo diagnostics | partial | Records deterministic loopback client endpoint text. |
| `HldsAddressScopedChallengeCacheEntry::endpoint` | `src/game_api/hl_server_module.cpp` | endpoint-scoped challenge key | source code | diagnostic-only | no | string key | getchallenge/connect bridge | getchallenge challenge value | connect validation | yes as diagnostic prerequisite | Keyed by endpoint string, not by qport. |
| `HldsAddressScopedChallengeCacheEntry::value` | `src/game_api/hl_server_module.cpp` | challenge value | source code | diagnostic-only | connectionless text only | string token | getchallenge/connect bridge | getchallenge response | connect challenge validation | yes as diagnostic prerequisite | Does not prove real HLDS challenge encoding. |
| `kHldsAddressScopedChallengeCacheMaxEntries` | `src/game_api/hl_server_module.cpp` | challenge cache policy | source code | diagnostic-only | no | integer constant `8` | diagnostic cache | prompt 269 policy | cache boundedness | guard only | Diagnostic capacity policy. |
| `kHldsAddressScopedChallengeCacheTtlTicks` | `src/game_api/hl_server_module.cpp` | challenge TTL | source code | diagnostic-only | no | integer constant `3` | diagnostic cache | prompt 269 policy | expiration gate | guard only | Diagnostic tick model, not real time. |
| `challenge_one_shot` / `challenge_replay_detected` | `include/game_api/hl_server_module.h`, `src/game_api/hl_server_module.cpp` | replay/one-shot state | source code | diagnostic-only | no | booleans | connect validation | cache hit and consumed flag | replay rejection | guard only | Diagnostic one-shot proof, not netchan session proof. |
| `BuildHldsUserinfoValidationPolicyConnectInput` | `src/game_api/hl_server_module.cpp` | connect/userinfo text builder | source code | diagnostic-only | partial connectionless text | text tokens | connect diagnostic | challenge value | userinfo policy | partial as diagnostic precondition | Builds `connect protocol=48 challenge=... userinfo=...`; no qport token. |
| `HldsUserinfoValidationPolicy*` | `include/game_api/hl_server_module.h`, `src/game_api/hl_server_module.cpp` | userinfo policy | source code | diagnostic-only | no real admission bytes | text/userinfo limits only | connect diagnostic | challenge cache | connect readiness policy | yes as diagnostic prerequisite | Policy max bytes=256, max keys=16, key bytes=32, value bytes=64. |
| `session_id` | `src/app/main.cpp`, `src/common/logger.cpp`, prompt-era signon surfaces | session id | source code | app/logging or diagnostic-only | no | string/run identity | logging/pseudo signon | app run/logging | artifacts/log names | no for netchan | Not a network session id. |
| `netchan_not_started` | `include/game_api/hl_server_module.h`, `src/game_api/hl_server_module.cpp` | netchan blocker | source code | diagnostic-only | no | n/a | post-connect/signon blockers | connect/session future | sequence/ack/envelope work | guard only | Confirms no netchan runtime/session was started. |
| `client_not_put_in_server` | `include/game_api/hl_server_module.h`, `src/game_api/hl_server_module.cpp` | admission blocker | source code | diagnostic-only | no | n/a | admission | signon/admission future | gameplay transport | guard only | Confirms no client admission. |

## Qport/Session Candidate Matrix

| Candidate field/binding | Likely role | Local symbols/evidence | Byte-level evidence sufficient? | Field width known? | Endian known? | Order known? | Stage | Required for netchan? | Required for post-connect serverinfo? | Required for signon-time serverinfo? | Blocker | Smallest safe next task |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| qport field | client port remap/NAT aid in connect/netchan | no `qport` symbol | no | no | no | no | connect/netchan | likely | likely | likely | absent field | no-client diagnostic capture design |
| client source port | observed UDP source port | `client_port`, sockaddr source port | no | local int only | n/a | n/a | connectionless diagnostics | maybe | maybe | maybe | not qport | capture design or evidence fixture later |
| remote address | endpoint identity/provenance | `remote_address_source`, sockaddr, loopback endpoint strings | no | text only | n/a | n/a | connectionless diagnostics | yes for real channel | yes | yes | no netchan remote binding | qport/session capture design |
| endpoint key | challenge cache lookup key | `AF_INET/127.0.0.1:<port>` | no | text only | n/a | n/a | getchallenge/connect bridge | diagnostic prerequisite only | maybe | maybe | not real session key | explicit diagnostic contract if later needed |
| address-scoped challenge key | bind challenge to endpoint | `challenge_cache_key=remote_loopback_endpoint` | no | text label only | n/a | n/a | getchallenge/connect bridge | prerequisite only | maybe | maybe | no real HLDS proof | keep as prerequisite guard |
| session id | durable channel/session identity | logger `session_id`, pseudo signon session descriptors | no | string only | n/a | n/a | logging/pseudo signon | unknown | unknown | unknown | not network session | do not reuse |
| challenge value | connect authorization token | diagnostic challenge value string | connectionless diagnostic only | no real width | n/a | text token order known only in diagnostic connect | getchallenge/connect | prerequisite only | maybe | maybe | not netchan proof | fixture ingestion/capture later |
| challenge TTL | challenge expiration | TTL ticks=3 | no | local int only | n/a | n/a | diagnostic cache | no direct netchan proof | no | no | diagnostic tick model | keep guard |
| challenge one-shot/replay state | prevent reused challenge | consumed flag, replay gate | no | booleans only | n/a | n/a | diagnostic cache/connect | prerequisite only | maybe | maybe | not real HLDS proof | keep guard |
| userinfo identity | name/model/rate fields | userinfo policy and parser | diagnostic text only | bytes limit known for policy | n/a | diagnostic text order only | connect | prerequisite only | maybe | maybe | not admission proof | keep policy guard |
| client slot/index | future admission target | sparse client slot/player slot text | no | no | no | no | admission | maybe after netchan | maybe | yes | admission forbidden | separate admission inventory |
| netchan remote address binding | channel peer binding | `netchan_not_started` only | no | no | no | no | netchan | yes | yes | yes | no channel object | no-client capture design |
| netchan qport binding | qport tied to channel | not found | no | no | no | no | netchan | yes | yes | yes | absent | no-client capture design |
| connect-to-netchan transition binding | accepted connect creates channel/session | diagnostic connect readiness only | no | no | no | no | transition | yes | yes | yes | transition unknown | no-client capture design |
| reject/disconnect session binding | reject after failed session/connect | rejection summaries | no | no | no | no | reject/disconnect | maybe | maybe | maybe | envelope unknown | reject/disconnect inventory later |

## Address-Scoped Challenge Linkage Audit

| Linkage item | Local evidence | Reusable as diagnostic prerequisite? | Reusable as real netchan proof? | Stage-specific notes | Guard needed |
| --- | --- | --- | --- | --- | --- |
| Challenge cache key | `challenge_cache_key=remote_loopback_endpoint` | yes | no | Diagnostic endpoint key only. | Keep qport/session claim blocked. |
| Endpoint matching | `FindEndpoint(endpoint)` using `AF_INET/127.0.0.1:<port>` | yes | no | Uses observed loopback source endpoint. | Do not infer qport/NAT behavior. |
| Challenge value matching | `connect_challenge_value == endpoint_entry->value` | yes | no | Text token in diagnostic connect. | Do not infer real packet field width/order. |
| Wrong endpoint reuse | `gate_wrong_endpoint_reuse`, client B socket | yes | no | Diagnostic proof of endpoint mismatch. | Keep loopback-only and no public/LAN. |
| Expiration | TTL ticks=3, created tick=10 | yes | no | Synthetic tick model. | Do not infer real timeout. |
| One-shot/replay | `consumed`, `challenge_replay_detected` | yes | no | Diagnostic replay gate. | Do not infer real challenge lifecycle. |
| Connect readiness | challenge and userinfo diagnostics feed serverinfo preview | partial | no | Connectionless/connect diagnostic only. | Do not start netchan. |
| Userinfo policy | minimal diagnostic policy | yes | no | Validates text fields before diagnostic acceptance. | Do not infer real admission. |
| Lifecycle acceptance | prompt 271 artifacts | yes as policy history | no | Connectionless lifecycle only. | Keep post-connect/signon blocked. |
| Loopback pump/client smoke | prompts 273/276/277 artifacts | yes as loopback policy history | no | Socket proofs are earlier prompt scope only; this prompt opens none. | Keep no runtime in this prompt. |

## Qport To Netchan Dependency Mapping

| Future dependency | Current local evidence | Qport/session dependency | Unresolved fields | Forbidden assumptions | Next safe action |
| --- | --- | --- | --- | --- | --- |
| Netchan channel init | `netchan_not_started` blocker only | needs remote address/qport/session binding | channel object, qport, sequence seed | do not invent channel init | no-client diagnostic capture design |
| Sequence/ack initialization | prompt 300 found no byte contract | likely depends on channel/session identity | initial sequence, ack, reliable bit | do not reuse pseudo sequence IDs | capture design or evidence fixture |
| Reliable/unreliable envelope selection | prompt 301 found no envelope | likely depends on established channel | reliable flag, channel placement | do not use callback destination labels as packets | envelope fixture only after evidence |
| Remote address binding | endpoint strings and `remote_address_source` summaries | address must be bound to channel peer | exact key policy | do not treat diagnostic text as netchan key | qport/session capture design |
| NAT/client-port tolerance | no exact `NAT`/port remap evidence | qport may exist for this, but absent locally | qport field, remap rules | do not infer from UDP source port | external evidence/capture policy later |
| Connect acceptance | challenge/userinfo diagnostics | prerequisite before channel transition | connect-to-channel state | do not claim admission | no-client design later |
| Post-connect serverinfo delivery | unresolved fixtures | depends on channel/envelope after connect | first packet envelope | do not promote query/info or preview | post-connect evidence later |
| Signon serverinfo delivery | pseudo signon surfaces only | depends on channel/session/signon state | signon envelope and state | do not reuse pseudo signon | signon evidence later |
| Reject/disconnect messaging | rejection summaries | may depend on endpoint/session after connect | envelope and reason framing | do not infer post-connect reject bytes | reject/disconnect inventory later |
| Client admission / put-in-server | blocker field only | likely after signon/session completion | slot/index/admission rules | do not admit client | separate admission inventory |

## Qport/Session Relation To Message Writing And Envelope

| Area | Local evidence | Qport/session appears? | Interpretation | Needed before fixture contract? |
| --- | --- | --- | --- | --- |
| Message writing helpers | prompt 299 callback/query-info/pseudo helpers | no real qport writer | Helpers do not encode qport/session. | yes |
| Reliable/unreliable envelope candidates | prompt 301 matrix | qport/session marked unknown dependency | Envelope cannot be placed without session binding evidence. | yes |
| Sequence/ack candidates | prompt 300 matrix | qport/client port row only, no qport bytes | Sequence/ack cannot be safely fixed to a peer without session binding. | yes |
| Address-scoped challenge cache | prompt 269/source | endpoint key exists | Reusable as diagnostic prerequisite, not real proof. | yes, as prerequisite guard |
| Userinfo validation policy | prompt 270/source | no qport, but connect text identity exists | Userinfo is a diagnostic connect prerequisite only. | yes, as policy guard |
| Query/info boundary | prompts 287-296 | no qport/session dependency | Connectionless query/info stays separate. | no for query/info, yes for post-connect/signon |

## Stage Separation And Blocker Preservation

| Boundary | Status |
| --- | --- |
| Query/info boundary remains closed | yes |
| Connectionless query/info is qport/session proof | no |
| getchallenge/connect diagnostics are real netchan proof | no |
| post-connect byte evidence sufficient | no |
| signon-time byte evidence sufficient | no |
| reliable/unreliable envelope byte evidence sufficient | no |
| sequence/ack byte evidence sufficient | no |
| real post-connect builder complete | no |
| real signon builder complete | no |
| real wire builder complete | no |
| real client capture allowed now | no |
| public/LAN exposure allowed now | no |
| netchan runtime started | no |

## Unknowns And Forbidden Assumptions

| Unknown | Why unknown | Local evidence if any | Why it must not be invented | Safe way to resolve later |
| --- | --- | --- | --- | --- |
| qport field existence in real connect packet | no `qport` symbol or fixture bytes | prompt 300/302 static searches | invented qport would corrupt connect/channel contract | no-client diagnostic capture design or fixture ingestion policy |
| qport field width | no field found | none | width affects parser/builder correctness | evidence-backed fixture |
| qport endianness | no field found | none | byte order cannot be guessed | evidence-backed fixture |
| qport placement/order | no field found | none | token/field order affects connect parsing | capture/evidence plan |
| qport relationship to UDP source port | only `client_port` source-port diagnostics | local loopback bind result | UDP source port is not qport | qport/session capture design |
| qport relationship to NAT/port remap | no exact NAT/remap evidence | none | NAT behavior is protocol-sensitive | real/reference evidence under later policy |
| qport relationship to challenge value | challenge cache does not use qport | endpoint key + challenge string | qport may or may not participate | evidence-backed contract |
| qport relationship to userinfo | userinfo connect text has no qport | diagnostic userinfo policy | userinfo identity is not qport | evidence-backed contract |
| qport relationship to client slot | no admission path | blocker fields only | admission is forbidden and unknown | later admission inventory |
| session id existence | logger/pseudo session only | `session_id` noisy hits | app log session is not network session | capture/evidence plan |
| session id width/placement | no network session field | none | cannot encode unknown session | evidence-backed fixture |
| remote address key policy | endpoint string diagnostic only | `AF_INET/127.0.0.1:<port>` | real netchan key may differ | no-client capture design |
| connect endpoint to netchan address transition | no channel init | `netchan_not_started` | transition semantics affect all packets | no-client design before runtime |
| reject/disconnect session binding | summaries only | rejection text/reasons | stage-specific envelope unknown | reject/disconnect inventory later |

## Blocker Ranking

| Rank | Blocker | Severity | Reason | Smallest safe next task | Runtime needed? | Real client needed? | Public/LAN needed? |
| ---: | --- | --- | --- | --- | --- | --- | --- |
| 1 | missing qport/session byte contract | critical | no qport symbol, field width, endian, or order found | no-client diagnostic capture design | no for design | no | no |
| 2 | missing qport-to-netchan binding | critical | no channel object or remote binding exists | no-client diagnostic capture design | no for design | no | no |
| 3 | missing reliable/unreliable envelope byte contract | critical | prompt 301 found only diagnostic blockers and labels | envelope fixture only after session evidence | no | no | no |
| 4 | missing sequence/ack byte contract | critical | prompt 300 found no sufficient sequence/ack contract | sequence/ack fixture only after session/envelope evidence | no | no | no |
| 5 | missing remote address/session key policy | high | endpoint strings are diagnostic-only | no-client diagnostic capture design | no for design | no | no |
| 6 | missing post-connect serverinfo byte contract | high | unresolved fixtures remain blocked | post-connect evidence acquisition later | later | no now | no |
| 7 | missing signon serverinfo byte contract | high | pseudo signon is not real signon | signon evidence acquisition later | later | no now | no |
| 8 | missing reject/disconnect session binding | medium | rejection summaries do not define post-connect session envelope | reject/disconnect static inventory later | no | no | no |
| 9 | real client capture forbidden | policy blocker | prompt forbids Steam/client execution | separate explicit policy boundary only | yes later | yes later | no |
| 10 | public/LAN exposure forbidden | policy blocker | prompt forbids public/LAN sockets | keep blocked | no | no | no |

## Recommended Next Prompt

Recommended next prompt:

`HL-CL-20260504-303-dedicated-goldsrc-hlds-qport-session-no-client-diagnostic-capture-design`

Rationale: static qport/session evidence is too weak for a fixture contract. The repo has a useful diagnostic address-scoped challenge prerequisite, but no qport field, no network session id, no qport-to-netchan transition, and no byte-level remote binding evidence. The next safe step is a design-only no-client diagnostic capture plan that remains loopback/local, does not invoke real clients, and does not execute capture until a later explicit prompt.
