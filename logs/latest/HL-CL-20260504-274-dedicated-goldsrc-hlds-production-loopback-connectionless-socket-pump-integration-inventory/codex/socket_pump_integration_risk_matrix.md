# Socket Pump Integration Risk Matrix

Prompt ID: HL-CL-20260504-274-dedicated-goldsrc-hlds-production-loopback-connectionless-socket-pump-integration-inventory

Compatibility claim level: diagnostic-socket-pump-integration-inventory-only; no real Steam Half-Life or HLDS-compatible client compatibility claimed

## Candidate Risk Summary

| ID | Candidate | Risk | Main risk | Safe current use | Promotion action |
| --- | --- | --- | --- | --- | --- |
| C1 | Launch option parse/init | Low | Flag sprawl could imply socket behavior if names are too broad. | Parse diagnostic flags only; no socket open. | Keep unchanged. |
| C2 | Host application startup | Medium | Runs in normal host startup before server-module lifecycle state is available. | Pass-through only. | Do not open/register sockets here. |
| C3 | `RunServerEngineShim` option bridge | Low | Incorrect option propagation could enable diagnostics unexpectedly. | Copy explicit flags to module init options. | Keep bridge-only. |
| C4 | `InitializeEngineShim` state setup | Low | Registration fields could be mistaken for runtime enablement. | State-only registration and disabled defaults. | Best next prompt target for registration stub. |
| C5 | `FinalizeServerBootstrapStep` existing diagnostic surface | Low | It is a launch-time proof hook, not a durable host pump. | Current prompt 273 proof surface with explicit flags. | Keep current hook until registration exists. |
| C6 | Server frame loop | Medium | Per-frame pump would need bounded scheduling, persistent socket lifetime, and cleanup ownership. | Not used for socket pump today. | Defer until after registration stub. |
| C7 | Dedicated query/connect diagnostic surface boundary | Medium | This area is admission-adjacent and aggregates many query/connect/signon surfaces. | Keep separate from pump. | Do not merge with socket pump yet. |
| C8 | Shutdown/cleanup path | Low | Persistent sockets would need deterministic close semantics. | RAII closes current proof sockets; module destructor clears active state. | Include cleanup owner in next stub. |
| C9 | Runtime proof entrypoints | Low | Proof-only checks can mask lack of normal lifecycle integration. | Preserve as regression proof. | Keep as proof, not runtime wiring. |
| C10 | Future production receive path boundary | High | Prompt 272 found no real production receive path to hook. | None. | Do not claim or wire real receive path yet. |

## Promotion Outcome Comparison

| Option | Outcome | Risk | Decision |
| --- | --- | --- | --- |
| `report_only_no_source_change` | Produces integration inventory and plan only. | Low | Selected. |
| `add_read_only_introspection_helper` | Adds source just to report existing state. | Low | Not needed because existing summaries and source are enough. |
| `add_disabled_by_default_host_lifecycle_registration_stub` | Adds state-only registration, no socket open. | Low to medium | Recommended next prompt, not this inventory prompt. |
| `add_frame_pump_wiring_in_diagnostic_mode_only` | Adds per-frame diagnostic pump. | Medium | Too early without registration owner and cleanup contract. |

## Blockers Before Frame Pump Wiring

| Blocker | Evidence | Required reduction |
| --- | --- | --- |
| No persistent pump owner | Prompt 273 pump sockets are local to `RunHldsProductionLoopbackConnectionlessSocketPumpDiagnostic`. | Add a disabled registration record and owner field before opening sockets outside the proof runner. |
| No shutdown-owned socket lifetime | Current closure is RAII in the proof function; no host lifecycle close hook owns a persistent pump. | Define cleanup owner and closed summary fields before frame loop use. |
| No real production receive path | Prompt 272 reported `production_receive_path_available=0` and `guarded_hook_installed=0`. | Keep diagnostic loopback-only pump separate from real receive compatibility claims. |
| Admission-adjacent surfaces nearby | Dedicated query/connect/signon surfaces share summary neighborhoods and some query socket concepts. | Keep pump boundary explicit and no-admission until auth/netchan/signon/baselines are separately proven. |
| Frame loop is not always active | `RunServerFrameLoop` returns disabled when frames <= 0. | Registration stub should state whether frame pumping depends on `--frames` or a new bounded diagnostic tick. |

## Risk Controls To Preserve

| Control | Required state |
| --- | --- |
| Default state | Disabled, no socket allocation, no registration side effects beyond summary fields. |
| Mode gate | Dedicated diagnostic only. |
| Bind gate | Reject all non-loopback/public bind requests before socket open. |
| Pump budget | Bounded datagram count per step and bounded steps per proof/tick. |
| Dispatch boundary | Connectionless getchallenge/connect/serverinfo diagnostics only. |
| Admission boundary | No Steam auth, no netchan, no reliable channel, no signon state, no baselines, no put-in-server. |
| Cleanup | All sockets closed and reported closed. |
| Reporting | Stable machine-readable summary fields remain present. |

## Recommended Next Prompt

Recommended next prompt id: HL-CL-20260504-275-dedicated-goldsrc-hlds-production-loopback-connectionless-socket-pump-lifecycle-registration-stub

Recommended next task: add a disabled-by-default host lifecycle registration stub for the production loopback connectionless socket pump, with read-only registration summary fields, no socket opens, no frame pump, preserved prompt 273 gates, and a documented cleanup owner.
