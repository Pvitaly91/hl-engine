# Socket Pump Safety Gate Matrix

Prompt ID: HL-CL-20260504-274-dedicated-goldsrc-hlds-production-loopback-connectionless-socket-pump-integration-inventory

Compatibility claim level: diagnostic-socket-pump-integration-inventory-only; no real Steam Half-Life or HLDS-compatible client compatibility claimed

## Legend

- P: Preserved by current code or safe for that integration point.
- S: Safe only as state/summary/pass-through, with no socket open.
- R: Risky or blocked for promotion without another prompt.
- N/A: Not applicable because no socket or datagram dispatch can happen there.

Candidate IDs:

| ID | Integration point |
| --- | --- |
| C1 | Launch option parse/init |
| C2 | Host application startup |
| C3 | `RunServerEngineShim` option bridge |
| C4 | `InitializeEngineShim` state setup |
| C5 | Current `FinalizeServerBootstrapStep` diagnostic surface |
| C6 | Server frame loop |
| C7 | Dedicated query/connect diagnostic surface boundary |
| C8 | Shutdown/cleanup |
| C9 | Runtime proof entrypoints |
| C10 | Future production receive path boundary |

## Gate Preservation Matrix

| Gate | C1 | C2 | C3 | C4 | C5 | C6 | C7 | C8 | C9 | C10 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Disabled by default | P | S | P | P | P | R | R | S | P | R |
| Explicit diagnostic enable only | P | S | P | P | P | R | R | N/A | P | R |
| Dedicated mode only | S | S | S | P | P | R | P | N/A | P | R |
| Non-loopback bind denied | N/A | N/A | N/A | S | P | R | R | N/A | P | R |
| Public socket blocked before open | N/A | N/A | N/A | S | P | R | R | N/A | P | R |
| Malformed marker rejected | N/A | N/A | N/A | N/A | P | R | R | N/A | P | R |
| Connect without cached challenge rejected | N/A | N/A | N/A | N/A | P | R | R | N/A | P | R |
| Wrong protocol rejected | N/A | N/A | N/A | N/A | P | R | R | N/A | P | R |
| Unsafe userinfo rejected | N/A | N/A | N/A | N/A | P | R | R | N/A | P | R |
| Deterministic bounded pump | N/A | N/A | N/A | S | P | R | R | N/A | P | R |
| Sockets closed | N/A | N/A | N/A | S | P | R | R | P | P | R |
| No Steam auth | P | P | P | P | P | R | R | P | P | R |
| No netchan | P | P | P | P | P | R | R | P | P | R |
| No reliable channel | P | P | P | P | P | R | R | P | P | R |
| No signon state | P | P | P | P | P | R | R | P | P | R |
| No resource/model/sound/event baselines | P | P | P | P | P | R | R | P | P | R |
| No put-in-server/client admission | P | P | P | P | P | R | R | P | P | R |
| Stable machine-readable summaries | S | S | S | P | P | R | R | S | P | R |

## Gate Notes

Prompt 273 proof surface C5 preserves every required safety gate when enabled explicitly. The report-only decision keeps C5 unchanged and adds no new code path.

C6 and C7 are marked risky for promotion, not because they currently weaken gates, but because wiring a pump there would create a new runtime behavior requiring a persistent owner, per-frame budget, bind-policy enforcement before open, and shutdown cleanup proof.

C10 remains blocked because prompt 272 found no real production receive path boundary. Any future receive path must preserve the diagnostic claim boundary and must not be represented as real Steam Half-Life or HLDS client compatibility.

## Prompt 273 Gate Proofs Preserved

| Prompt 273 proof | Status used for this inventory |
| --- | --- |
| happy | pass |
| gate_disabled_by_default | pass |
| gate_non_loopback_bind_denied | pass |
| gate_public_socket_blocked | pass |
| gate_bad_marker | pass |
| gate_connect_without_cached_challenge | pass |
| gate_wrong_protocol | pass |
| gate_unsafe_userinfo | pass |

No prompt 274 source changes were made, so no source path could weaken these proof results.
