# Socket Policy Allowed And Forbidden Actions

Prompt: HL-CL-20260504-343-dedicated-goldsrc-hlds-qport-session-capture-execution-socket-policy-review

| Action | Allowed in this prompt | Allowed in next socket policy gate prompt | Still forbidden until separate policy | Notes |
| --- | --- | --- | --- | --- |
| Docs/report changes | Yes | Yes | No | Stable docs and artifacts only. |
| Socket policy review | Yes | Yes | No | Policy review only. |
| Loopback-only socket policy gate | No | Yes | No | Read-only gate; no socket open. |
| Socket open implementation | No | No | Yes | Requires later explicit implementation policy. |
| Loopback socket open | No | No | Yes | Policy discussion is not open permission. |
| Public socket | No | No | Yes | Public exposure remains denied. |
| LAN socket | No | No | Yes | LAN exposure remains denied. |
| Datagram send | No | No | Yes | Requires separate datagram policy. |
| Datagram receive | No | No | Yes | Requires separate datagram policy. |
| Capture execution | No | No | Yes | Requires separate capture execution policy. |
| Capture runtime | No | No | Yes | Requires separate capture runtime policy. |
| Packet capture | No | No | Yes | No packet capture is implemented or run. |
| Real client | No | No | Yes | Real client binaries remain denied. |
| Steam | No | No | Yes | Steam remains denied. |
| Connect runtime | No | No | Yes | Connect path remains blocked. |
| Post-connect serverinfo | No | No | Yes | Post-connect serverinfo remains blocked. |
| Signon serverinfo | No | No | Yes | Signon serverinfo remains blocked. |
| Netchan runtime | No | No | Yes | Netchan remains blocked. |
| Qport evidence promotion | No | No | Yes | Unknown byte-level behavior remains unknown. |
| Compatibility claim expansion | No | No | Yes | No real compatibility is claimed. |

