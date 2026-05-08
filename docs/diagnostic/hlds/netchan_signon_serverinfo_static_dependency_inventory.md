# HL-CL-20260504-298 Static Dependency Inventory

Prompt ID: HL-CL-20260504-298-dedicated-goldsrc-hlds-netchan-signon-serverinfo-static-dependency-inventory

Compatibility claim level: diagnostic-netchan-signon-serverinfo-static-dependency-inventory-only; no real Steam Half-Life or HLDS-compatible client compatibility claimed

This inventory is static and report-only. It does not capture packets, invoke Steam, invoke a real client binary, open sockets, run connect, run post-connect serverinfo, run signon serverinfo, start auth, start netchan, send resource baselines, enter signon state, or admit a client.

## Boundary Status

The query/info diagnostic boundary from prompts 287 through 296 is closed for diagnostic connectionless query/info only. It proves fixture-backed byte building/parsing, query/info path integration, loopback query/info response swap, diagnostic query-client smoke, regression acceptance, fixture drift gating, wrapper execution, operator documentation, and release-boundary documentation.

That closed boundary does not prove post-connect serverinfo, signon-time serverinfo, netchan sequencing, reliable/unreliable channels, resource/model/sound/event baselines, client admission, real query-client compatibility, real Steam Half-Life client compatibility, or public/LAN exposure.

## Scan Scope

Tracked files scanned in the selected static scope: 127.

Primary scan roots:

- `include/`
- `src/`
- `fixtures/diagnostic/hlds/`
- `docs/diagnostic/hlds/`
- `scripts/`
- selected prompt artifacts from 279, 285, and 297

The corrected static scan found 96,534 aggregate core-term matches across 73 tracked files in the core scope. The large count is mostly from diagnostic summary/report fields, repeated prompt artifacts, and broad words such as `message`, `signon`, and `reject`; it is not byte-level proof by itself.

Representative match counts:

| Term | Matches | Files | Static conclusion |
| --- | ---: | ---: | --- |
| `netchan` | 190 | 31 | Mostly diagnostic blocker fields such as `netchan_not_started`; no real netchan byte/framing proof found. |
| `signon` | 56,431 | 51 | Extensive diagnostic and historical signon surfaces exist; real signon-time serverinfo bytes remain unresolved. |
| `serverinfo` | 1,685 | 49 | Query/info and diagnostic serverinfo artifacts exist; real post-connect/signon byte evidence remains absent. |
| `svc_serverinfo` / `SVC_SERVERINFO` | 23 | 17 | Appears in docs/artifacts as an unknown target; no implementation-level real message id proof found. |
| `baseline` | 790 | 35 | Baseline/report surfaces exist; no byte-level baseline packet format proof found. |
| `resource` | 96 | 24 | Resource/precache scaffolding exists; no signon resource baseline serialization proof found. |
| `MSG_Write` / `SZ_Write` | 9 | 3 | Appears as missing/future evidence in artifacts; no reusable production MSG/SZ writer found. |
| `WriteByte` / `WriteShort` / `WriteLong` / `WriteString` | 89 | 8 | Diagnostic frame message callbacks and query/info byte helpers exist; not real serverinfo wire proof. |
| `getchallenge` | 370 | 9 | Diagnostic getchallenge and challenge-cache surfaces exist. |
| `userinfo` | 838 | 14 | Diagnostic userinfo policy exists; not real admission proof. |

## Static Symbol And File Inventory

| File path | Symbol or artifact | Category | Evidence type | Byte-level evidence exists? | Stage | Mode | Safe future use |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `fixtures/diagnostic/hlds/serverinfo/fixtures/connectionless_query_info_candidate.json` | `connectionless_query_info_candidate` | Query/info fixture | Fixture | Yes, for connectionless query/info only | Connectionless query | Fixture-only | Safe for query/info regression only. |
| `fixtures/diagnostic/hlds/serverinfo/fixtures/diagnostic_post_connect_serverinfo_current.json` | `diagnostic_post_connect_serverinfo_current` | Diagnostic preview | Fixture | No real byte evidence | Diagnostic post-connect preview | Fixture-only | Safe as a negative/reference preview, not a real builder input. |
| `fixtures/diagnostic/hlds/serverinfo/fixtures/post_connect_real_serverinfo_unresolved.json` | `post_connect_real_serverinfo_unresolved` | Real-stage placeholder | Fixture | No | Post-connect | Fixture-only, unresolved | Safe to define gaps; builder must remain blocked. |
| `fixtures/diagnostic/hlds/serverinfo/fixtures/signon_time_serverinfo_unresolved.json` | `signon_time_serverinfo_unresolved` | Real-stage placeholder | Fixture | No | Signon-time | Fixture-only, unresolved | Safe to define gaps; builder must remain blocked. |
| `fixtures/diagnostic/hlds/serverinfo/contract/serverinfo_fixture_contract.schema.json` | Serverinfo fixture contract | Fixture contract | Schema | Contractual only | Mixed diagnostic stages | Fixture-only | Safe for fixture validation and drift checks. |
| `fixtures/diagnostic/hlds/query_info_regression/query_info_regression_ci_manifest.json` | Query/info CI manifest | Query/info boundary | Manifest | Records query/info expectations only | Connectionless query | Report/diagnostic | Safe to guard query/info drift, not post-connect/signon. |
| `docs/diagnostic/hlds/query_info_release_boundary_summary.md` | Query/info release boundary | Query/info boundary | Doc | No new bytes | Connectionless query | Report-only | Safe as closed query/info context. |
| `docs/diagnostic/hlds/post_connect_signon_evidence_acquisition_plan.md` | Evidence acquisition plan | Planning | Doc | No | Post-connect/signon target | Report-only | Safe next-planning input. |
| `scripts/run_hlds_query_info_regression.ps1` | Query/info regression wrapper | Rerun wrapper | Script | Invokes query/info acceptance/drift gates | Connectionless query | Diagnostic wrapper | Safe for query/info regression only. |
| `include/app/launch_options.h` | Diagnostic launch flags | Host launch config | Source | No | Multiple diagnostic gates | Diagnostic-only | Safe for locating existing probes; no new flags added here. |
| `src/app/launch_options.cpp` | Launch option parser | Host launch config | Source | No | Multiple diagnostic gates | Diagnostic-only | Safe for locating existing probes; no new flags added here. |
| `src/app/host_application.cpp` | Probe dispatch | Host wiring | Source | No | Multiple diagnostic gates | Diagnostic-only | Safe for locating existing probe entrypoints. |
| `include/game_api/hl_server_module.h` | `HldsGetchallengeDiagnosticSurfaceSummary` | Challenge | Source | No | Connectionless getchallenge | Diagnostic-only | Reusable policy context, not real admission evidence. |
| `include/game_api/hl_server_module.h` | `HldsUserinfoValidationPolicyDiagnosticSurfaceSummary` | Userinfo | Source | No | Connect/userinfo policy | Diagnostic-only | Reusable diagnostic validation policy context. |
| `include/game_api/hl_server_module.h` | `netchan_not_started`, `resource_baselines_not_sent`, `signon_state_not_entered`, `client_not_put_in_server` | Forbidden stage blockers | Source | No | Post-connect/signon blockers | Diagnostic-only | Safe as gates that must remain true. |
| `include/game_api/hl_server_module.h` | `post_connect_byte_evidence_sufficient`, `signon_time_byte_evidence_sufficient` | Evidence gap | Source | No | Post-connect/signon | Diagnostic-only | Safe to preserve evidence-gap guard. |
| `src/game_api/hl_server_module.cpp` | `RunHldsServerinfoFixtureContractValidatorDiagnosticProbe` | Fixture validator | Source | Fixture validation only | Fixture stages | Diagnostic-only | Required validator dependency. |
| `src/game_api/hl_server_module.cpp` | `RunHldsServerinfoUnresolvedFixtureEvidenceGapGuard` | Evidence-gap guard | Source | No | Post-connect/signon blockers | Diagnostic-only | Required blocker for unresolved real stages. |
| `src/game_api/hl_server_module.cpp` | `RunHldsConnectionlessQueryInfoByteLevelBuilderParser` | Query/info byte builder/parser | Source | Yes, query/info only | Connectionless query | Diagnostic-only | Closed query/info boundary. |
| `src/game_api/hl_server_module.cpp` | `RunHldsQueryInfoLoopbackRegressionAcceptance` | Query/info acceptance gate | Source | Validates query/info gates | Connectionless query | Diagnostic-only | Closed query/info boundary. |
| `src/game_api/hl_server_module.cpp` | `RunHldsQueryInfoRegressionCiManifestDriftGate` | Drift gate | Source | Manifest validation only | Connectionless query | Diagnostic-only | Closed query/info drift guard. |
| `src/game_api/hl_server_module.cpp` | `RunHldsServerinfoContractBackedDiagnosticPathIntegration` | Diagnostic serverinfo path | Source | Contract-backed diagnostic preview only | Diagnostic post-connect preview | Diagnostic-only | Not real post-connect/signon evidence. |
| `src/game_api/server_frame_loop.cpp` | `FrameMessageBuffer::*` | Message observation buffer | Source | Records callback writes, not packet bytes | Frame/game callback | Diagnostic/host helper | Candidate for later static message-writing inventory. |
| `src/game_api/hl_server_module.cpp` | `StubWriteByte`, `StubWriteShort`, `StubWriteLong`, `StubWriteString` | Game DLL message callback stubs | Source | No real packet framing | Frame/game callback | Host shim | Candidate for later message-writing inventory, not serverinfo wire proof. |
| `src/game_api/server_bootstrap.cpp` | `PrecacheRegistry::*` | Model/sound precache registry | Source | No baseline packet bytes | Resource/precache | Host scaffolding | Candidate for baseline/resource linkage inventory. |
| `src/game_api/server_bootstrap.cpp` | `EdictStore::*` | Edict/entity storage | Source | No baseline packet bytes | Entity/spawn | Host scaffolding | Candidate for baseline/entity linkage inventory. |
| `logs/latest/HL-CL-20260504-269-...` | Address-scoped challenge cache artifacts | Challenge/session | Artifact | No | Connectionless/connect dependency | Diagnostic-only | Safe context for challenge/session linkage. |
| `logs/latest/HL-CL-20260504-270-...` | Userinfo policy artifacts | Userinfo | Artifact | No | Connect/userinfo dependency | Diagnostic-only | Safe context for validation policy. |
| `logs/latest/HL-CL-20260504-271-...` | Connectionless lifecycle artifacts | Lifecycle | Artifact | No | Connectionless/connect diagnostic | Diagnostic-only | Safe context; not admission proof. |
| `logs/latest/HL-CL-20260504-273-...` | Loopback socket pump artifacts | Socket pump | Artifact | No real public networking | Loopback-only diagnostic | Diagnostic-only | Safe policy context for loopback-only pump. |
| `logs/latest/HL-CL-20260504-276-...` | Frame wiring artifacts | Socket/frame wiring | Artifact | No | Loopback/frame diagnostic | Diagnostic-only | Safe policy context, not netchan proof. |
| `logs/latest/HL-CL-20260504-277-...` | Localhost client smoke artifacts | Diagnostic client | Artifact | No real client | Loopback diagnostic | Diagnostic-only | Safe context; not real client proof. |
| `logs/latest/HL-CL-20260504-279-...` | Real serverinfo inventory skeleton | Serverinfo inventory | Artifact | No sufficient real bytes | Unknown real stages | Report-only | Safe prior negative inventory. |
| `logs/latest/HL-CL-20260504-285-...` | Byte-level evidence inventory | Serverinfo evidence gap | Artifact | No sufficient real bytes | Post-connect/signon | Report-only | Safe prior evidence-gap basis. |
| `logs/latest/HL-CL-20260504-297-...` | Evidence acquisition plan | Planning | Artifact | No | Post-connect/signon | Report-only | Direct input to this inventory. |

## Dependency Graph

| Node | Current proof status | Current implementation status | Current blocker | Upstream prerequisites | Downstream dependents | Stage boundary | Forbidden assumptions |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `getchallenge` | Diagnostic request/response proof exists from earlier prompts | Diagnostic-only | Not real HLDS proof | Connectionless marker/command policy | Challenge cache, connect diagnostics | Connectionless | Do not treat as admission or netchan setup. |
| Challenge cache | Address-scoped diagnostic proof exists | Diagnostic-only | No production admission policy | `getchallenge` | Connect diagnostics, userinfo policy | Connectionless/connect bridge | Do not infer real client challenge semantics. |
| Connect datagram | Diagnostic connect policy exists in prior artifacts | Diagnostic-only | No real admission, no netchan | Challenge cache, userinfo | Post-connect preview, later netchan | Connect | Do not run in query/info or this prompt. |
| Userinfo validation | Diagnostic policy exists | Diagnostic-only | No production userinfo policy | Connect datagram, challenge cache | Admission policy, post-connect planning | Connect | Do not infer real client acceptance. |
| Diagnostic serverinfo path | Contract-backed diagnostic preview exists | Diagnostic-only | Not byte-level real post-connect or signon evidence | Fixture validator, diagnostic preview fixture | Static planning only | Diagnostic post-connect preview | Do not promote preview to real evidence. |
| Query/info byte path | Closed positive proof from 287-296 | Diagnostic-only | None inside query/info boundary | Query/info fixture, validator, evidence guard | Query/info regression wrapper | Connectionless query | Do not promote query/info to post-connect or signon. |
| Loopback socket pump | Bounded loopback proof exists in previous prompts | Diagnostic-only | Not public/LAN, not real netchan | Loopback policy | Query/info loopback, connectionless diagnostics | Loopback diagnostic | Do not expose public/LAN. |
| Netchan | Only blocker fields and policy summaries found | Not implemented as real channel | Sequencing/framing unknown | Connect/session policy, message writer policy | Reliable/unreliable, signon | Post-connect/netchan | Do not assume channel sequence or ack behavior. |
| Reliable/unreliable channels | Blocker fields found | Not implemented | Channel split and ordering unknown | Netchan | Signon messages, resource baselines | Netchan/signon | Do not send signon messages without channel evidence. |
| Post-connect serverinfo | Unresolved fixture exists | Builder/parser blocked | Missing byte-level packet evidence | Connect, message writer, framing policy | Netchan/signon planning | Post-connect | Do not use diagnostic preview as real bytes. |
| Signon-time serverinfo | Unresolved fixture exists | Builder/parser blocked | Missing byte-level signon message evidence | Netchan, reliable/unreliable, message ids | Baselines, spawn/admission | Signon-time | Do not invent `svc_serverinfo` framing. |
| Resource baselines | `resource_baselines_not_sent` gates and precache scaffolding found | Not serialized | Baseline packet format unknown | Signon channel, model/sound/event registries | Client spawn/admission | Signon/baseline | Do not infer baseline bytes from registry state. |
| Model/sound/event baselines | Precache/entity scaffolding found | Not serialized | Field layout and ordering unknown | Resource baseline policy, edict/model/sound state | Client spawn/admission | Signon/baseline | Do not emit without byte evidence. |
| Client admission / put-in-server | Explicit blocker fields found | Blocked | Admission policy and signon completion unknown | Auth, netchan, signon, baselines | Gameplay client state | Admission | Do not put client in server. |

Dependency edges counted for this inventory: 16.

Key edges:

1. `getchallenge` -> challenge cache
2. Challenge cache -> connect datagram
3. Connect datagram -> userinfo validation
4. Userinfo validation -> diagnostic lifecycle acceptance
5. Diagnostic lifecycle acceptance -> diagnostic serverinfo preview
6. Diagnostic serverinfo preview -> unresolved real post-connect blocker
7. Unresolved real post-connect blocker -> post-connect serverinfo evidence acquisition
8. Connect/session policy -> netchan
9. Netchan -> reliable/unreliable channels
10. Reliable/unreliable channels -> signon-time serverinfo
11. Signon-time serverinfo -> resource baselines
12. Resource baselines -> model/sound/event baselines
13. Model/sound/event baselines -> client admission
14. Query/info byte path -> query/info regression wrapper
15. Evidence-gap guard -> unresolved post-connect/signon blockers
16. Loopback policy -> diagnostic-only query/info and connectionless proofs

## Message Writing And Byte Encoding Inventory

| Helper | File | Classification | Can support future diagnostic fixtures? | Safe for future serverinfo fixture work? | Missing before real serverinfo |
| --- | --- | --- | --- | --- | --- |
| `FrameMessageBuffer::Begin` | `src/game_api/server_frame_loop.cpp` | Diagnostic frame message observation | Yes, to record callback context | Possibly, for observations only | Real packet boundary/framing. |
| `FrameMessageBuffer::End` | `src/game_api/server_frame_loop.cpp` | Diagnostic frame message observation | Yes | Possibly, for observations only | Real packet flush semantics. |
| `FrameMessageBuffer::Abort` | `src/game_api/server_frame_loop.cpp` | Diagnostic frame message observation | Yes | Possibly, for negative observations | Real failure semantics. |
| `FrameMessageBuffer::WriteByte` | `src/game_api/server_frame_loop.cpp` | Diagnostic callback write record | Yes | Possibly, but not as wire proof | Opcode/message id evidence. |
| `FrameMessageBuffer::WriteChar` | `src/game_api/server_frame_loop.cpp` | Diagnostic callback write record | Yes | Possibly, but not as wire proof | Signedness and field contract. |
| `FrameMessageBuffer::WriteShort` | `src/game_api/server_frame_loop.cpp` | Diagnostic callback write record | Yes | Possibly, but not as wire proof | Endianness and field contract. |
| `FrameMessageBuffer::WriteLong` | `src/game_api/server_frame_loop.cpp` | Diagnostic callback write record | Yes | Possibly, but not as wire proof | Endianness and field contract. |
| `FrameMessageBuffer::WriteAngle` | `src/game_api/server_frame_loop.cpp` | Diagnostic callback write record | Yes | Maybe for entity/baseline observations | Real angle encoding. |
| `FrameMessageBuffer::WriteCoord` | `src/game_api/server_frame_loop.cpp` | Diagnostic callback write record | Yes | Maybe for entity/baseline observations | Real coord encoding. |
| `FrameMessageBuffer::WriteString` | `src/game_api/server_frame_loop.cpp` | Diagnostic callback write record | Yes | Possibly, but not as wire proof | String termination/encoding evidence. |
| `FrameMessageBuffer::WriteEntity` | `src/game_api/server_frame_loop.cpp` | Diagnostic callback write record | Yes | Maybe for entity observations | Entity index encoding. |
| `StubWriteByte` / `StubWriteShort` / `StubWriteLong` / `StubWriteString` | `src/game_api/hl_server_module.cpp` | Engine callback shim into frame message buffer | Yes, for game DLL callback observation | Possibly, but only after stage-specific capture policy | Real network packet context. |
| `AppendByteLengthPrefixedText` | `src/game_api/hl_server_module.cpp` | Query/info byte helper | Yes for query/info style fixtures | No, unless real serverinfo evidence proves length-prefixed text | Post-connect/signon string contract. |

Message writing helpers found: 13 helper groups.

No production `MSG_Write*`, `SZ_Write`, `sizebuf`, or reusable real network datagram writer was found in source during this inventory. The existing write helpers are diagnostic or query/info-specific and cannot define post-connect/signon serverinfo bytes without new evidence.

## Serverinfo Stage Separation Audit

| Stage | Current evidence | Current fixture | Byte-level evidence sufficient? | Builder allowed now? | Parser allowed now? | Real compatibility claim allowed? | Next safe action |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Connectionless query/info | Closed diagnostic byte-level boundary from prompts 287-296 | `connectionless_query_info_candidate` | Yes, for diagnostic query/info only | Yes, diagnostic-only | Yes, diagnostic-only | No | Preserve regression wrapper and drift gate. |
| Diagnostic post-connect preview | Contract-backed preview and artifacts | `diagnostic_post_connect_serverinfo_current` | No real byte evidence | Only diagnostic preview/path checks | Only diagnostic preview/path checks | No | Keep as negative/reference input. |
| Real post-connect serverinfo | Unresolved fixture and evidence-gap guard | `post_connect_real_serverinfo_unresolved` | No | No | No | No | Acquire byte-level field/framing evidence in a later approved prompt. |
| Signon-time serverinfo | Unresolved fixture and evidence-gap guard | `signon_time_serverinfo_unresolved` | No | No | No | No | Inventory netchan/message-writing dependencies first. |
| Netchan messages | Blocker fields, no real channel implementation | None | No | No | No | No | Static message-writing/netchan inventory. |
| Resource/model/sound/event baselines | Registry/scaffolding and blocker fields | None | No | No | No | No | Static baseline/resource linkage inventory. |

The evidence-gap guard remains the control point that prevents diagnostic previews or query/info bytes from being treated as post-connect or signon-time serverinfo evidence.

## Baseline And Resource Linkage Inventory

| Linkage | Static references found | Classification | Required before signon-time serverinfo? | Byte-level known? | Candidate next inventory? |
| --- | --- | --- | --- | --- | --- |
| Model precache | `PrecacheRegistry::PrecacheModel`, `ModelIndex`, `EnsureModelIndex`, `ModelName`, `ModelCount` | Host scaffolding | Likely yes for resource/model baselines | No | Yes |
| Sound precache | `PrecacheRegistry::PrecacheSound`, `SoundName`, `SoundCount`, `SoundPrecacheRegistry` references | Host scaffolding | Likely yes | No | Yes |
| Event baseline | Broad `event` symbols exist, but no clear signon event-baseline serializer found | Unknown/diagnostic | Likely yes | No | Yes |
| Resource list | `resource_baselines_not_sent` gates and resource path normalization | Diagnostic blocker plus host utility | Likely yes | No | Yes |
| Entity/edict baseline | `EdictStore::*`, entity snapshots, edict indices/offsets | Host scaffolding | Likely yes | No | Yes |
| Map CRC/checksum | Not established by this scan as serverinfo byte evidence | Not found/unknown | Unknown but likely needed | No | Yes |
| Server spawn count/server count | Summary fields such as `spawn_count` exist | Diagnostic/report state | Likely relevant | No | Yes |
| Client slot/index | Some client-slot references; no real admission serializer | Diagnostic/report state | Likely relevant | No | Yes |

Baseline/resource symbols found: 886 aggregate `baseline` plus `resource` matches in the selected scan, but this is not byte-level baseline evidence.

## Challenge, Session, And Userinfo Linkage

| Prior surface | What can be reused safely | Still diagnostic-only | Must not be reused as real compatibility evidence | Needed before post-connect/signon evidence can be trusted |
| --- | --- | --- | --- | --- |
| Prompt 269 address-scoped challenge cache | Challenge issuance, TTL, one-shot/replay, endpoint gating as diagnostic policy | All challenge cache behavior | Real HLDS challenge semantics | Stage-specific challenge/session linkage evidence. |
| Prompt 270 userinfo validation policy | Userinfo parsing limits and unsafe-value guards as diagnostic policy | All userinfo acceptance behavior | Production userinfo/admission policy | Final production userinfo policy and real client expectations. |
| Prompt 271 connectionless lifecycle acceptance | Sequencing gates for getchallenge/connect/userinfo diagnostics | Lifecycle acceptance as diagnostic harness | Client admission or netchan start | Real transition evidence from connect to post-connect packets. |
| Prompts 273-277 loopback pump/frame/client smoke | Loopback-only socket policy, cleanup expectations, public-socket blockers | Loopback client/pump harnesses | Public/LAN or real-client compatibility | Separate policy before any capture/run beyond static inventory. |
| Prompts 287-296 query/info boundary | Query/info fixture/gates/wrapper and drift protections | Connectionless query/info only | Post-connect/signon serverinfo proof | Maintain as closed boundary; do not cross-promote stages. |

Challenge/session links found: 1,808 aggregate `getchallenge` plus `challenge` matches in the selected scan. Userinfo links found: 838 aggregate `userinfo` matches. These counts show useful diagnostic policy context, not real session/admission proof.

## Ranked Blocker List

| Rank | Blocker | Severity | Reason | Smallest safe next task | Runtime needed? | Real client needed? | Public/LAN needed? |
| ---: | --- | --- | --- | --- | --- | --- | --- |
| 1 | Missing byte-level post-connect serverinfo | Critical | Real post-connect fixture remains unresolved and builder/parser remain blocked. | Static message-writing and serverinfo field contract inventory. | No | No | No |
| 2 | Missing byte-level signon-time serverinfo | Critical | Signon-time fixture remains unresolved and `svc_serverinfo` message id/framing is unknown. | Static netchan/message-writing inventory. | No | No | No |
| 3 | Netchan sequencing unknown | Critical | No real netchan sequencing/ack/framing proof found. | Focused netchan static inventory. | No | No | No |
| 4 | Reliable/unreliable channel unknown | High | Signon delivery path cannot be inferred from query/info or callback stubs. | Static channel dependency inventory. | No | No | No |
| 5 | Message writing policy unknown | High | Diagnostic `FrameMessageBuffer` is not a real network writer. | Focused message-writing inventory. | No | No | No |
| 6 | Baseline/resource linkage unknown | High | Precache/edict scaffolding exists without packet serialization evidence. | Baseline/resource linkage static inventory. | No | No | No |
| 7 | Client admission forbidden | High | `client_not_put_in_server` and related gates remain true by design. | Admission policy inventory only after signon evidence exists. | No | No | No |
| 8 | Real client capture forbidden | High | Current policy forbids Steam/client binaries. | Separate real-client policy boundary before any use. | No | Later only | No |
| 9 | Public/LAN exposure forbidden | High | Query/info and loopback policies explicitly block public/LAN. | Separate public/LAN policy boundary before any exposure. | No | No | Later only |
| 10 | Fixture evidence-gap guard active | Medium | Prevents unsafe promotion of unresolved fixtures, as intended. | Preserve guard and add focused inventories before fixture promotion. | No | No | No |

Top blocker: missing_byte_level_post_connect_serverinfo.

## Recommended Next Prompt

Recommended next prompt:

`HL-CL-20260504-299-dedicated-goldsrc-hlds-netchan-message-writing-static-inventory`

Reason: this scan found message-writing observation helpers and many netchan/signon references, but no reusable real `MSG_Write`/`SZ_Write`/`sizebuf` writer or netchan packet framing proof. A narrower static inventory of netchan and message-writing dependencies is the smallest safe next task before any serverinfo field contract inventory or capture design.

