# Qport/Session Capture Dry-Run Boundary Risk Table

Compatibility claim level: diagnostic-qport-session-capture-dry-run-release-boundary-only; no real Steam Half-Life or HLDS-compatible client compatibility claimed

| Risk | Severity | Boundary guard | Required response |
| --- | --- | --- | --- |
| Fixture or policy drift | high | Offline fixture validator plus JSON parse check | Stop and repair the fixture policy or fixture set. |
| Dry-run manifest drift | high | Dry-run manifest validator | Stop and repair manifest fields or planned schema. |
| Wrapper option misuse | high | Unsafe option rejection before host invocation | Stop and treat accepted unsafe option as wrapper regression. |
| Accidental capture enablement | critical | `capture_allowed_now=0` required | Stop; no capture can proceed from this boundary. |
| Socket action in plan | critical | `socket_open_attempted=0`, `planned_actions_safe=1` | Reject the plan. |
| Public or LAN exposure | critical | `public_socket_opened=0`, `lan_socket_opened=0` | Reject the plan. |
| Real client or Steam action | critical | `real_steam_client_used=0`, `real_client_binary_invoked=0` | Reject the plan. |
| Connect/post-connect/signon action | high | Runtime path fields remain `0` | Reject the plan. |
| Netchan runtime action | high | `netchan_runtime_started=0` | Reject the plan. |
| Address-scoped challenge promoted to real netchan proof | high | `address_scoped_challenge_reusable_as_real_netchan_proof=0` | Reject promotion and return to policy hardening. |
| Qport/session evidence overclaim | high | `byte_level_qport_session_evidence_sufficient=0` | Reject promotion and require byte-level evidence policy. |
| Stale docs | medium | Stable docs name exact paths and commands | Update docs before use. |
| Future capture implementation overreach | critical | Separate policy-review prompt required | Do not implement capture under this boundary. |

Residual risk: the boundary proves dry-run wiring and blocking behavior only. It does not prove qport/session byte layout, capture feasibility, real client compatibility, or HLDS compatibility.
