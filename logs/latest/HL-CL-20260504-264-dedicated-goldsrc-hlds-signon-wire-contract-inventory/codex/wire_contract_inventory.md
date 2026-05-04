# HLDS Signon Wire-Contract Inventory

Prompt: HL-CL-20260504-264-dedicated-goldsrc-hlds-signon-wire-contract-inventory
Compatibility claim level: synthetic-loopback-inventory-only; no real Steam Half-Life or HLDS-compatible client compatibility is claimed.

## Scope
This is a read-only inspection report. No source code was changed. The goal is to map the current dedicated host/sign-on proof stack to the real GoldSrc/HLDS connect and signon lifecycle and identify the smallest safe next bridge.

## Current Synthetic Proof Stack Inventory

| Prompt | Surface family | Launch flags | What it proves | Synthetic-only fields/concepts |
|---|---|---|---|---|
| 259 | septenary positive resumed-range | `--signon-message-cursor-carried-checkpoint-claimed-checkpoint-successor-checkpoint-resume-token-claim-checkpoint-resume-token-claim-checkpoint-resume-token-claim-checkpoint-resume-token-claim-resume-range-surface`; `--signon-message-cursor-carried-checkpoint-claimed-checkpoint-successor-checkpoint-resume-token-claim-checkpoint-resume-token-claim-checkpoint-resume-token-claim-checkpoint-resume-token-claim-resume-range-probe`; `--signon-message-cursor-carried-checkpoint-claimed-checkpoint-successor-checkpoint-resume-token-claim-checkpoint-resume-token-claim-checkpoint-resume-token-claim-checkpoint-resume-token-claim-resume-range-probe-scenario <happy|gate>` | positive resumed range after resume-allow | septenary claimant chain, carried/claimed checkpoint, synthetic cursor/message indices |
| 260 | septenary positive resumed-EOF | `--signon-message-cursor-carried-checkpoint-claimed-checkpoint-successor-checkpoint-resume-token-claim-checkpoint-resume-token-claim-checkpoint-resume-token-claim-checkpoint-resume-token-claim-resume-eof-surface`; `--signon-message-cursor-carried-checkpoint-claimed-checkpoint-successor-checkpoint-resume-token-claim-checkpoint-resume-token-claim-checkpoint-resume-token-claim-checkpoint-resume-token-claim-resume-eof-probe`; `--signon-message-cursor-carried-checkpoint-claimed-checkpoint-successor-checkpoint-resume-token-claim-checkpoint-resume-token-claim-checkpoint-resume-token-claim-checkpoint-resume-token-claim-resume-eof-probe-scenario <happy|gate>` | EOF only after accepted resumed range | synthetic EOF descriptor and terminal cursor state |
| 261 | septenary single resume-denial | `--signon-message-cursor-carried-checkpoint-claimed-checkpoint-successor-checkpoint-resume-token-claim-checkpoint-resume-token-claim-checkpoint-resume-token-claim-checkpoint-resume-token-claim-resume-denial-surface`; `--signon-message-cursor-carried-checkpoint-claimed-checkpoint-successor-checkpoint-resume-token-claim-checkpoint-resume-token-claim-checkpoint-resume-token-claim-checkpoint-resume-token-claim-resume-denial-probe`; `--signon-message-cursor-carried-checkpoint-claimed-checkpoint-successor-checkpoint-resume-token-claim-checkpoint-resume-token-claim-checkpoint-resume-token-claim-checkpoint-resume-token-claim-resume-denial-probe-scenario <happy|gate>` | stale_cursor_after_eof denial without range/EOF mutation | synthetic denial scenario and deny reason |
| 262 | septenary resume-denial-matrix | `--signon-message-cursor-carried-checkpoint-claimed-checkpoint-successor-checkpoint-resume-token-claim-checkpoint-resume-token-claim-checkpoint-resume-token-claim-checkpoint-resume-token-claim-resume-denial-matrix-surface`; `--signon-message-cursor-carried-checkpoint-claimed-checkpoint-successor-checkpoint-resume-token-claim-checkpoint-resume-token-claim-checkpoint-resume-token-claim-checkpoint-resume-token-claim-resume-denial-matrix-probe`; `--signon-message-cursor-carried-checkpoint-claimed-checkpoint-successor-checkpoint-resume-token-claim-checkpoint-resume-token-claim-checkpoint-resume-token-claim-checkpoint-resume-token-claim-resume-denial-matrix-probe-scenario <happy|gate>` | five negative variants denied deterministically | denial matrix counters and synthetic post-terminal policy |
| 263 | resume lifecycle acceptance gate | `--signon-resume-lifecycle-acceptance-gate`; `--signon-resume-lifecycle-acceptance-probe`; `--signon-resume-lifecycle-acceptance-probe-scenario <happy|gate>` | range, EOF, single denial, and denial matrix as one regression contract | aggregate synthetic lifecycle counters |

Synthetic proof-only concepts: resume token claim chain; septenary claimant chain; carried checkpoint; claimed checkpoint; successor checkpoint resume-token-claim cascade; resume range; resume EOF; denial matrix; lifecycle acceptance gate; synthetic signon byte markers `HLPB`/`HLWM`/`HLWB`/`HLWS`/`HLWW`; prompt semantic tags.

Potentially reusable concepts: bounded loopback runtime; deterministic summary/probe framework; slot/session/cursor accounting; accepted/rejected counters; stable reject/deny reasons; manifest/handoff discipline; dedicated launch option plumbing; loopback query/connect scaffolding as diagnostics only.

## Real GoldSrc/HLDS Signon/Connect Lifecycle Inventory

| Area | Current repo status | Evidence | Compatibility assessment |
|---|---|---|---|
| UDP connectionless packet boundary | Partial loopback-only WinSock helpers. | `src/game_api/hl_server_module.cpp`: `ScopedUdpSocket`, `BindLoopbackQuerySocket`, `SendConnectionlessText`, `ReceiveConnectionlessText`. | Useful diagnostic substrate; not a public HLDS packet pump. |
| A2S/server info query | Partial legacy-like query proof exists. | `BuildGoldSrcInfoRequest`, `IsGoldSrcInfoRequest`, `BuildGoldSrcInfoResponse`, `ParseGoldSrcInfoResponse`, `PumpOneLoopbackGoldSrcInfoQuery`. | Closer to real query shape than resume stack, but still local proof only. |
| Challenge request/response | Synthetic text `getchallenge`/`challenge <value>` proof exists. | `PumpOneLoopbackConnectAdmissionAttempt`. | Not a real HLDS getchallenge handler/table. |
| Client connect parsing | Synthetic `connect challenge=<challenge> name=<player>` proof exists. | `PumpOneLoopbackConnectAdmissionAttempt`, `ParseAcceptResponseText`. | Missing real packet grammar, protocol/version/userinfo/password/auth handling. |
| Protocol/version validation | Not materially implemented for HLDS wire contract. | no dedicated parser found beyond synthetic substring checks. | Missing required real reject path. |
| Userinfo parsing | Minimal `name=` substring proof only. | `PumpOneLoopbackConnectAdmissionAttempt`. | Missing backslash key/value parser and validation. |
| Serverinfo emission | Query response exists; connect-time serverinfo/signon missing. | `BuildGoldSrcInfoResponse`, query summaries. | Missing real connect/signon serverinfo contract. |
| Signon state transition | Synthetic signon state/cursor summaries exist. | `BuildDedicatedSignon*`, resume lifecycle summaries. | No real `svc_signonnum`/GoldSrc signon state machine proven. |
| Resource/model/sound/event baseline inventory | Synthetic semantic tags/internal registries only. | `BuildDedicatedSignon*` synthetic builders. | Missing real resource lists, baselines, events. |
| Reliable/unreliable channel setup | Missing. | no netchan-style channel setup found. | Required before real client compatibility. |
| Netchan sequencing/ack/fragmentation | Missing. | grep did not reveal real netchan implementation. | Required before real client compatibility. |
| Client spawn/put-in-server equivalent | Internal lifecycle counters exist. | `AdmitDedicatedLoopbackPreauthPlayer`, lifecycle summaries. | Not wired to real client protocol state. |
| Disconnect/reject paths | Synthetic reject reasons exist. | connect probe and resume denial probes. | Need real wire reject/disconnect packets. |
| Map/mod/game directory negotiation and Steam compatibility | Fixture/gamedir concepts exist; no real Steam/HL client negotiation. | launch options and query summaries. | High compatibility risk; no Steam client proof. |

## File and Function Map

| File/function | Current role | Bucket | Risk | Recommended next action |
|---|---|---|---|---|
| `include/app/launch_options.h` | dedicated proof and acceptance launch option fields | shared plumbing / synthetic proof | medium | add only compact diagnostic flags |
| `src/app/launch_options.cpp` | flag parsing and dependency propagation | shared plumbing / synthetic proof | medium | keep next additions scoped to getchallenge diagnostic |
| `src/app/host_application.cpp` | transfers launch options into init options | shared host path | medium | touch only to pass read-only diagnostic config |
| `include/game_api/hl_server_module.h` | init options and summary/probe structs | shared infrastructure | high | add small diagnostic structs only |
| `src/game_api/hl_server_module.cpp` | slot state, loopback sockets, query/connect proofs, synthetic signon/resume stack | shared plus synthetic proof | high | isolate next parser/helper; do not rewrite net flow |
| `BindLoopbackQuerySocket` | loopback UDP bind and port record | diagnostic substrate | medium | reuse for getchallenge diagnostic |
| `PumpOneLoopbackConnectAdmissionAttempt` | synthetic getchallenge/connect proof | synthetic connect proof | high | evidence only; do not treat as real parser |
| `BuildConnectionlessTextPacket` / `ExtractConnectionlessText` | bounded connectionless text helpers | diagnostic substrate | medium | reuse for first diagnostic before binary expansion |
| `BuildGoldSrcInfoRequest/Response` | legacy-like A2S info proof | query diagnostic | medium | keep separate from connect |
| `AdmitDedicatedLoopbackPreauthPlayer` | synthetic slot admission | shared slot state | high | do not touch for read-only getchallenge diagnostic |
| `BuildDedicatedSignon*` helpers | synthetic proof byte builders | synthetic signon proof | high | do not extend for real compatibility claims |
| `src/game_api/server_bootstrap.*` | server/bootstrap scaffolding | shared host infrastructure | medium | inspect later; no immediate change |
| `src/game_api/server_frame_loop.*` | frame loop scaffolding | shared host infrastructure | medium | leave untouched unless pump hook is needed |
| `src/game_api/server_command_*` | command buffer/dispatcher | shared infrastructure | medium | leave untouched for next diagnostic |
| `docs/MANUAL_VERIFY_RESUMED_DENIAL.md` | prior synthetic denial docs | docs | low | no next action |

## Smallest Safe Next Implementation Step

Recommended next prompt id: `HL-CL-20260504-265-dedicated-goldsrc-hlds-getchallenge-diagnostic-surface`

Recommended next task: implement a read-only HLDS-style connectionless `getchallenge` / `challenge` diagnostic surface that parses actual connectionless packet bytes and emits deterministic summary/reject fields, without accepting real clients yet.

Why: current code already has a synthetic `getchallenge` text exchange, so this is adjacent and low blast-radius. Real connect parsing, userinfo, signon state, netchan, and baselines all depend on a stable challenge boundary. A read-only diagnostic does not claim Steam/HLDS compatibility and avoids a broad networking rewrite.

Likely next files: `include/app/launch_options.h`, `src/app/launch_options.cpp`, `src/app/host_application.cpp`, `include/game_api/hl_server_module.h`, `src/game_api/hl_server_module.cpp`.

Keep untouched next: Half-Life client code, `client.dll`, HUD/renderer/gameplay systems, existing synthetic resume suffix implementations except for reference/reuse, netchan/replication until connectionless/challenge diagnostics are explicit.
