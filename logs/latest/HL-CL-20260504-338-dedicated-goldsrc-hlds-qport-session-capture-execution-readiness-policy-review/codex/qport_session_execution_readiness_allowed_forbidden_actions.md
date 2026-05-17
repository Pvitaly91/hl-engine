# Qport/Session Execution Readiness Allowed And Forbidden Actions

Compatibility claim level: diagnostic-qport-session-capture-execution-readiness-policy-review-only; no real Steam Half-Life or HLDS-compatible client compatibility claimed

| Action | Allowed in this prompt | Allowed in next readiness prompt | Still forbidden until separate policy | Notes |
| --- | --- | --- | --- | --- |
| Docs/report changes | Yes | Yes | No | This prompt changes docs and artifacts only. |
| Readiness gate/plan | No | Yes | No | Next prompt may define only a disabled-by-default readiness gate and plan. |
| Timeout/cleanup policy definition | Yes | Yes | No | Definition only; no runtime cleanup execution. |
| Artifact schema definition | Yes | Yes | No | Schema only; no capture artifacts from runtime execution. |
| Socket policy review | Yes | Yes | No | Review may be planned; socket open remains forbidden. |
| Datagram policy review | Yes | Yes | No | Review may be planned; datagram send/receive remains forbidden. |
| Capture execution | No | No | Yes | Requires separate execution approval after readiness work. |
| Capture runtime | No | No | Yes | Runtime capture remains blocked. |
| Socket open | No | No | Yes | Requires separate socket policy. |
| Loopback socket | No | No | Yes | Requires separate loopback socket policy. |
| Datagram send | No | No | Yes | Requires separate datagram-send policy. |
| Datagram receive | No | No | Yes | Requires separate datagram-receive policy. |
| Public socket | No | No | Yes | Out of scope. |
| LAN socket | No | No | Yes | Out of scope. |
| Real client | No | No | Yes | Out of scope. |
| Steam | No | No | Yes | Out of scope. |
| Connect | No | No | Yes | Runtime connect path remains blocked. |
| Post-connect serverinfo | No | No | Yes | Runtime post-connect path remains blocked. |
| Signon serverinfo | No | No | Yes | Runtime signon path remains blocked. |
| Netchan runtime | No | No | Yes | Netchan remains blocked. |
| Qport evidence promotion | No | No | Yes | Unknown byte-level behavior remains unknown. |
| Compatibility claim | No | No | Yes | Diagnostic-only compatibility claim is required. |

