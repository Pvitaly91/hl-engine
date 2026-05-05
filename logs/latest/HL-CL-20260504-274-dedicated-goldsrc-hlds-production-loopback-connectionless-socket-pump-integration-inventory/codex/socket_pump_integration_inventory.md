# HL-CL-20260504-274 Socket Pump Integration Inventory

Prompt ID: HL-CL-20260504-274-dedicated-goldsrc-hlds-production-loopback-connectionless-socket-pump-integration-inventory

Status: pass, report-only inventory.

Compatibility claim level: diagnostic-socket-pump-integration-inventory-only; no real Steam Half-Life or HLDS-compatible client compatibility claimed

## Scope Decision

Selected promotion decision: report_only_no_source_change.

Selected integration point: none added. The existing prompt 273 diagnostic surface remains anchored at `FinalizeServerBootstrapStep` and is only reached when the dedicated diagnostic surface flag is enabled.

Reason: prompt 273 already installed a launch-gated diagnostic surface and proof runner. The next safe promotion should be a tiny disabled-by-default host lifecycle registration stub that creates an explicit lifecycle owner without opening sockets. The current tree does not yet have a durable pump object, shutdown-owned socket lifetime, or normal per-frame receive boundary suitable for frame pump wiring.

No source files were changed for this prompt.

## Prompt 273 Source Inventory

Source commit: 7845839e1e30b170faca2ceaceab2faeab357dff.

Changed files:

| File | Prompt 273 additions |
| --- | --- |
| `include/app/launch_options.h` | Added disabled-by-default fields for `hlds_production_loopback_connectionless_socket_pump_diagnostic_surface_enabled`, `hlds_production_loopback_connectionless_socket_pump_diagnostic_probe_enabled`, and probe scenario. |
| `src/app/launch_options.cpp` | Added launch flags and scenario parsing for the production loopback socket pump diagnostic surface and probe. Probe implies surface. |
| `src/app/host_application.cpp` | Propagates the new launch options into `HlServerModuleInitOptions` and includes them in the engine shim option summary line. |
| `include/game_api/hl_server_module.h` | Added surface/probe summary structs, init option fields, and top-level summary fields. |
| `src/game_api/hl_server_module.cpp` | Added state, summary line builders, socket pump result helpers, bounded loopback UDP pump, result application, diagnostic surface runner, final bootstrap call, and probe pass/fail checks. |

Line references from current HEAD:

| Area | File/function | Lines | Inventory note |
| --- | --- | --- | --- |
| Launch defaults | `LaunchOptions` | `include/app/launch_options.h:66` | Surface/probe flags default false, preserving disabled by default. |
| Launch parse | `ParseLaunchOptions` | `src/app/launch_options.cpp:1741` | Adds surface flag. |
| Launch parse | `ParseLaunchOptions` | `src/app/launch_options.cpp:1765` | Adds probe flag. |
| Scenario parse | `ParseLaunchOptions` | `src/app/launch_options.cpp:1789` | Accepts only happy plus seven named gate scenarios. |
| Probe implies surface | post-parse normalization | `src/app/launch_options.cpp:15299` | Probe enables surface, but only after explicit probe flag. |
| App to module bridge | `HostApplication::RunServerEngineShim` | `src/app/host_application.cpp:22506` | Copies launch flags into module init options. |
| App init summary | `HostApplication::RunServerEngineShim` | `src/app/host_application.cpp:23895` | Emits stable option summary fields. |
| Module summary fields | `HlServerModuleSummary` | `include/game_api/hl_server_module.h:22524` | Top-level summary exposes surface and probe results. |
| Pump summary shape | `HldsProductionLoopbackConnectionlessSocketPumpDiagnosticSurfaceSummary` | `include/game_api/hl_server_module.h:7134` | Extends lifecycle gate summary with pump counters and bind policy fields. |
| Pump constants | `kHldsProductionLoopbackSocketPumpMaxDatagramsPerStep` | `src/game_api/hl_server_module.cpp:151358` | Max datagrams per step is bounded at 4. |
| Socket RAII | `ScopedUdpSocket` | `src/game_api/hl_server_module.cpp:148710` | Destructor closes any owned socket. |
| Loopback bind | `BindLoopbackQuerySocket` | `src/game_api/hl_server_module.cpp:148781` | Binds via `MakeLoopbackAddress`, which uses `INADDR_LOOPBACK`. |
| Pump step | `PumpHldsProductionLoopbackSocketPumpStep` | `src/game_api/hl_server_module.cpp:151456` | One bounded receive/dispatch pass, then response send when accepted. |
| Pump runner | `RunHldsProductionLoopbackConnectionlessSocketPumpDiagnostic` | `src/game_api/hl_server_module.cpp:151632` | Enforces disabled, non-loopback, public socket, packet, challenge, protocol, and userinfo scenarios. |
| Result apply | `ApplyHldsProductionLoopbackConnectionlessSocketPumpDiagnosticResult` | `src/game_api/hl_server_module.cpp:151989` | Copies result to stable machine-readable summary fields. |
| Surface runner | `PerformHldsProductionLoopbackConnectionlessSocketPumpDiagnosticSurface` | `src/game_api/hl_server_module.cpp:210372` | Runs only in dedicated mode and only when surface flag is enabled. |
| Bootstrap hook | `FinalizeServerBootstrapStep` | `src/game_api/hl_server_module.cpp:249554` | Current hook is after diagnostic lifecycle surfaces and before spawn-pipeline availability logging. |
| Probe validation | `HlServerModule::InitializeEngineShim` | `src/game_api/hl_server_module.cpp:255633` | Validates all prompt 273 proof scenarios. |

## Launch Flags Added By Prompt 273

| Flag | Effect | Default |
| --- | --- | --- |
| `--hlds-production-loopback-connectionless-socket-pump-diagnostic-surface` | Enables the diagnostic surface summary in dedicated mode. Does not by itself run the socket pump. | disabled |
| `--hlds-production-loopback-connectionless-socket-pump-diagnostic-surface=<bool>` | Boolean form of the surface flag. | disabled |
| `--hlds-production-loopback-connectionless-socket-pump-diagnostic-probe` | Enables the probe and implicitly enables the surface. | disabled |
| `--hlds-production-loopback-connectionless-socket-pump-diagnostic-probe=<bool>` | Boolean form of the probe flag. | disabled |
| `--hlds-production-loopback-connectionless-socket-pump-diagnostic-probe-scenario <scenario>` | Selects happy, disabled-by-default, non-loopback, public socket, bad marker, missing challenge, wrong protocol, or unsafe userinfo proof. | happy |
| `--hlds-production-loopback-connectionless-socket-pump-diagnostic-probe-scenario=<scenario>` | Inline scenario form. | happy |

No prompt 274 launch options were added.

## Pump Initialization Path

1. `wmain` parses launch options and configures logging.
2. `HostApplication::Run` resolves and validates the Valve game directory, then calls `RunServerEngineShim`.
3. `HostApplication::RunServerEngineShim` loads `hl.dll`, builds `HlServerModuleInitOptions`, copies the prompt 273 diagnostic socket pump flags, and calls `HlServerModule::InitializeEngineShim`.
4. `HlServerModule::InitializeEngineShim` resets summary/shim state, records the pump surface/probe options, initializes the deterministic server bootstrap, and calls `FinalizeServerBootstrapStep`.
5. `FinalizeServerBootstrapStep` runs the existing diagnostic surfaces, then calls `PerformHldsProductionLoopbackConnectionlessSocketPumpDiagnosticSurface`.
6. The pump surface returns immediately unless `state.server_state.dedicated` and the surface flag are both true.
7. With surface only, it emits a ready summary with no socket open. With probe enabled, it runs `RunHldsProductionLoopbackConnectionlessSocketPumpDiagnostic`.

## Pump Step/Poll Model

The pump is not a long-running host tick integration. It is a bounded proof runner:

- opens loopback server/client UDP sockets only inside the probe runner;
- binds via `127.0.0.1:0` for ephemeral loopback ports;
- sends a deterministic client datagram from the local proof client;
- calls `PumpHldsProductionLoopbackSocketPumpStep` once per expected datagram;
- each step performs one `ReceiveUdpDiagnosticDatagram`, dispatches by parsed connectionless command, and sends at most one diagnostic response;
- max datagrams per step summary constant is 4, but current scenarios perform one or two explicit steps;
- socket ownership remains local RAII scope and is closed before summary completion.

## Bind Policy And Socket Safeguards

| Guard | Implementation evidence | Prompt 273 proof evidence |
| --- | --- | --- |
| Disabled by default | Launch option fields default false; disabled scenario rejects before opening socket. | `gate_disabled_by_default` accepted=0 rejected=1, socket opened=0. |
| Explicit diagnostic mode only | Pump surface requires dedicated mode plus surface/probe flags. | All prompt 273 runtime summaries report `diagnostic_only=1`. |
| Loopback-only bind | Runtime uses `BindLoopbackQuerySocket`, which binds `MakeLoopbackAddress`. | Happy bind effective `127.0.0.1`, public socket opened=0. |
| Non-loopback denied before open | Scenario sets requested `0.0.0.0`, effective `<none>`, returns before socket creation. | `gate_non_loopback_bind_denied` socket opened=0. |
| Public socket blocked before open | Scenario sets public block policy and returns before socket creation. | `gate_public_socket_blocked` socket opened=0. |
| Sockets closed | `ScopedUdpSocket` closes in destructor; reject path marks sockets closed after started scopes unwind. | Every prompt 273 proof reports `sockets_closed=1`. |

## Datagram Dispatch And Diagnostic Helper Path

The prompt 273 pump dispatch path intentionally stays inside diagnostic helpers:

- `ReceiveUdpDiagnosticDatagram` receives one datagram from the loopback socket.
- `CopyConnectionlessMarkerAndCommand` records packet shape.
- `getchallenge` dispatch calls `ParseHldsGetchallengeDiagnosticInput`, inserts a challenge into `HldsAddressScopedChallengeCache`, and sends a `challenge` response.
- `connect` dispatch calls address-scoped challenge validation, userinfo policy validation, `ParseHldsConnectDiagnosticInput`, and `BuildHldsServerinfoDiagnosticResultFromConnect`.
- The serverinfo response is a diagnostic text packet. It does not start netchan, reliable channel, signon, baselines, or client admission.

## Summary Fields And Counters

Prompt 273 added stable summary fields for:

- surface/probe enabled state;
- scenario, accepted/rejected, reject reason, compatibility claim;
- diagnostic-only and no-admission flags inherited from lifecycle summaries;
- loopback/public socket flags and socket close state;
- pump lifecycle flags: initialized, started, stopped;
- bounded pump counters: steps, max datagrams per step, received, dispatched, rejected, responses, bytes in/out;
- bind policy, requested bind address, effective bind address;
- inherited lifecycle gate fields for getchallenge, cache, connect, userinfo, serverinfo;
- recommended next prompt id/task.

Prompt 273 happy summary values:

| Field | Value |
| --- | --- |
| `production_style_socket_pump_used` | 1 |
| `diagnostic_harness_receive_path_used` | 0 |
| `public_socket_opened` | 0 |
| `loopback_udp_socket_opened` | 1 |
| `sockets_closed` | 1 |
| `bind_policy` | `loopback_only_explicit_diagnostic` |
| `bind_address_effective` | `127.0.0.1` |
| `socket_pump_steps` | 2 |
| `socket_pump_datagrams_received` | 2 |
| `socket_pump_datagrams_dispatched` | 2 |
| `socket_pump_responses_sent` | 2 |

## Prompt 273 Proof Scenarios

| Scenario | What it proves |
| --- | --- |
| `happy` | Loopback UDP socket pump can step getchallenge, challenge cache, connect, userinfo policy, and serverinfo diagnostic response with no public socket and no admission. |
| `gate_disabled_by_default` | The pump remains disabled without explicit diagnostic enablement and opens no socket. |
| `gate_non_loopback_bind_denied` | A non-loopback requested bind is denied before any socket opens. |
| `gate_public_socket_blocked` | Public socket policy is blocked before any socket opens. |
| `gate_bad_marker` | Malformed connectionless marker is rejected and does not reach connect readiness. |
| `gate_connect_without_cached_challenge` | Connect without cached address-scoped challenge is rejected. |
| `gate_wrong_protocol` | Unsupported protocol is rejected after challenge but before connect readiness. |
| `gate_unsafe_userinfo` | Unsafe userinfo is rejected after challenge but before connect/serverinfo readiness. |

## Coupling To HostApplication And HlServerModule

The current coupling is narrow but still bootstrap-bound:

- `HostApplication` only parses and propagates launch options into the module init options.
- `HlServerModule` owns the actual state, diagnostic summaries, and socket pump proof runner.
- The pump has no standalone lifecycle object, no persistent socket member, and no registered per-frame tick hook.
- The only socket lifetime is inside `RunHldsProductionLoopbackConnectionlessSocketPumpDiagnostic`.
- The current execution point is still a launch-time diagnostic proof path under `InitializeEngineShim`, not normal host startup or an ongoing server loop.

## Host Lifecycle Integration Map

Candidate totals: 10 inspected, 6 safe for their current limited responsibility, 4 risky or blocked for promotion without another prompt.

| ID | File/function | Current responsibility | Dedicated mode? | Tests/proofs only? | Safe for diagnostic-only integration? | Public socket blocked before open? | Non-admission lifecycle preserved? | Risk | Recommended action |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| C1 | `src/app/launch_options.cpp:580`, `:1741`, `:15299` | Parse CLI flags and normalize implied surfaces. | All modes. | No. | Yes, for flags only. | Yes, no socket exists here. | Yes. | Low | Keep as flag parser; no new prompt 274 flags. |
| C2 | `src/app/host_application.cpp:22220` | Main host run: resolve game dir, smoke test, initialize shim. | All modes. | No. | No for pump execution; yes for pass-through only. | Not applicable here. | Yes if no pump added. | Medium | Do not open or register sockets here. |
| C3 | `src/app/host_application.cpp:22426`, `:22506`, `:25362` | Build `HlServerModuleInitOptions` and call module init. | All modes, maps dedicated flag. | No. | Yes, for option bridge only. | Yes, no socket exists here. | Yes. | Low | Keep bridge-only. |
| C4 | `src/game_api/hl_server_module.cpp:251441`, `:251845` | Reset summary/shim state and copy init flags. | All modes. | No. | Yes, for read-only registration state. | Yes, if it remains state-only. | Yes. | Low | Best next place for a disabled-by-default lifecycle registration stub. |
| C5 | `src/game_api/hl_server_module.cpp:249554`, `:249584` | Finalize deterministic bootstrap and run launch-gated diagnostic surfaces. | Yes after server state finalization. | Not test-only, but proof surfaces are flag-gated. | Already safe for launch-only diagnostic proof. | Yes, existing prompt 273 gates prove it. | Yes. | Low | Keep current hook; do not promote to normal pump yet. |
| C6 | `src/game_api/server_frame_loop.cpp:367`, `src/game_api/hl_server_module.cpp:247416`, `:249574` | Deterministic frame bootstrap and `pfnStartFrame` loop when frames > 0. | Yes after activation. | Proof/development oriented. | Not yet; needs lifecycle owner, per-frame budget, and shutdown cleanup first. | Unknown until a disabled registration stub owns bind policy. | Yes only if pump remains non-admission. | Medium | Future frame-pump prompt after registration stub. |
| C7 | `src/game_api/hl_server_module.cpp:210423` | Existing dedicated query/connect/signon diagnostic surface aggregation. | Dedicated only. | Flag-gated diagnostic surfaces. | Risky for socket pump promotion because this area also aggregates admission-adjacent surfaces. | Possible but would need explicit socket owner and bind policy. | Must be proven. | Medium | Do not merge pump with query/connect surface yet. |
| C8 | `src/game_api/hl_server_module.cpp:251387`, `:148710`, `src/common/logger.cpp:1163` | Module/log cleanup and socket RAII. | All modes. | No. | Yes for cleanup-only ownership. | Not a bind point. | Yes. | Low | Next stub should declare shutdown cleanup expectations. |
| C9 | `src/game_api/hl_server_module.cpp:255633`, prompt 273 runtime artifacts | Probe validation and machine-readable summaries. | Dedicated proof runs. | Yes. | Yes for proof checks only. | Yes, prompt 273 gates prove it. | Yes. | Low | Keep as regression proof surface. |
| C10 | Future production receive path boundary | Real connectionless receive path reported absent by prompt 272. | Not present. | Not present. | No, blocked until implemented. | Not implemented. | Not implemented. | High | Reduce risk with registration stub before any real receive-path attempt. |

## Promotion Decision

Decision: report_only_no_source_change.

Rationale:

- The current proof runner is already launch gated and safe in its current bootstrap-finalization proof role.
- Moving it into a frame loop now would require a persistent socket owner and shutdown behavior that do not exist yet.
- Wiring into the existing dedicated query/connect surface risks coupling diagnostic connectionless packet handling with admission-adjacent state before the no-admission boundary is made explicit.
- A read-only or disabled registration stub is a better next step than frame wiring because it can name the intended lifecycle owner, bind policy, per-frame budget, summary fields, and cleanup contract without opening sockets.

## Recommended Next Prompt

Recommended next prompt id: HL-CL-20260504-275-dedicated-goldsrc-hlds-production-loopback-connectionless-socket-pump-lifecycle-registration-stub

Recommended next task: add a disabled-by-default host lifecycle registration stub for the production loopback connectionless socket pump, with read-only registration summary fields, no socket opens, no frame pump, preserved prompt 273 gates, and a documented cleanup owner.
