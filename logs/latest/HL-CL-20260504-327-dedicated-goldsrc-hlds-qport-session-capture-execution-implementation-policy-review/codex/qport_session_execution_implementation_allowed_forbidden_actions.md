# HL-CL-20260504-327 Allowed And Forbidden Actions

Compatibility claim level: diagnostic-qport-session-capture-execution-implementation-policy-review-only; no real Steam Half-Life or HLDS-compatible client compatibility claimed

| Action | Allowed in this prompt | Allowed in the next implementation prompt | Still forbidden until separate policy | Notes |
| --- | --- | --- | --- | --- |
| Docs/report changes | Yes | Yes | No | This prompt only changes docs and artifacts. |
| Implementation shell changes | No | Yes | No | Next prompt may add disabled-by-default implementation skeleton surfaces. |
| Execution implementation skeleton | No | Yes | No | Must remain disabled by default and diagnostic-only. |
| Capture execution | No | No | Yes | Still requires separate execution policy. |
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
