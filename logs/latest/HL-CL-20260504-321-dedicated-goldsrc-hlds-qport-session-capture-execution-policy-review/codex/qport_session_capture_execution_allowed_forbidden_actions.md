# Qport/Session Capture Execution Allowed And Forbidden Actions

Compatibility claim level: diagnostic-qport-session-capture-execution-policy-review-only; no real Steam Half-Life or HLDS-compatible client compatibility claimed

| Action | Allowed in this prompt | Allowed in next policy-gate prompt | Allowed in future execution prompt | Forbidden until separate policy | Notes |
| --- | --- | --- | --- | --- | --- |
| Docs/report changes | Yes | Yes | Yes | No | Report-only updates are allowed. |
| Final policy review | Yes | Yes | Yes | No | This prompt performs review only. |
| Capture execution policy gate | No | Yes | Yes | No | Next prompt may define a policy-only gate. |
| Minimal no-client loopback capture implementation | No | No | Conditional | Yes | Requires a passed execution final policy gate first. |
| Runtime skeleton changes | No | No | Conditional | Yes | Requires a separate implementation prompt. |
| Open loopback socket | No | No | Conditional | Yes | Requires separate socket approval. |
| Send synthetic datagram | No | No | Conditional | Yes | Requires separate datagram approval. |
| Receive synthetic datagram | No | No | Conditional | Yes | Requires separate datagram approval. |
| Public socket | No | No | No | Yes | Public socket behavior remains out of scope. |
| LAN socket | No | No | No | Yes | LAN socket behavior remains out of scope. |
| Real client | No | No | No | Yes | Real clients remain out of scope. |
| Steam | No | No | No | Yes | Steam remains out of scope. |
| Connect path | No | No | No | Yes | Runtime connect path remains blocked. |
| Post-connect serverinfo | No | No | No | Yes | Post-connect runtime remains blocked. |
| Signon serverinfo | No | No | No | Yes | Signon runtime remains blocked. |
| Netchan runtime | No | No | No | Yes | Netchan remains blocked. |
| Qport evidence promotion | No | No | No | Yes | Unknown byte-level behavior remains unknown. |
| Compatibility claim | No | No | No | Yes | Diagnostic-only claim is required. |
