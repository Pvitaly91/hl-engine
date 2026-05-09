# Qport/Session Allowed And Forbidden Actions

Compatibility claim level: diagnostic-qport-session-capture-implementation-policy-review-only; no real Steam Half-Life or HLDS-compatible client compatibility claimed

| Action | Allowed in this prompt | Allowed in next implementation prompt | Allowed eventually after separate policy | Notes |
| --- | --- | --- | --- | --- |
| Edit docs | yes | yes | yes | Report and policy docs only here. |
| Edit wrapper safety docs | yes | yes | yes | Wrapper hardening remains diagnostic. |
| Add disabled-by-default capture shell | no | yes | yes | Shell only; no default capture. |
| Open loopback socket | no | no | yes | Requires later explicit policy. |
| Send synthetic qport/session datagram | no | no | yes | Requires later explicit policy. |
| Execute capture | no | no | yes | Requires separate policy and proof scope. |
| Public socket | no | no | no | Forbidden. |
| LAN socket | no | no | no | Forbidden. |
| Real client | no | no | no | Forbidden. |
| Steam | no | no | no | Forbidden. |
| Connect path | no | no | no | Out of scope. |
| Post-connect serverinfo | no | no | no | Out of scope. |
| Signon serverinfo | no | no | no | Out of scope. |
| Netchan runtime | no | no | no | Out of scope. |
| Real compatibility claim | no | no | no | Diagnostic-only claim remains required. |
