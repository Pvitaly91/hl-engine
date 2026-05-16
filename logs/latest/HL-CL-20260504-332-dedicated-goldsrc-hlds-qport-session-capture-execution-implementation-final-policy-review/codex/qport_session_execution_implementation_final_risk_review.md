# HL-CL-20260504-332 Risk Review

Compatibility claim level: diagnostic-qport-session-capture-execution-implementation-final-policy-review-only; no real Steam Half-Life or HLDS-compatible client compatibility claimed

| Risk | Current guard | Required response |
| --- | --- | --- |
| Accidental capture execution | `capture_execution_allowed_now=0`, `capture_executed=0` | Treat as a blocking regression. |
| Socket open creep | `socket_open_allowed_now=0`, `socket_open_attempted=0` | Stop and require separate socket policy. |
| Loopback socket overreach | `loopback_socket_allowed_now=0`, `loopback_udp_socket_opened=0` | Stop and require separate loopback policy. |
| Datagram send/receive creep | Datagram allowed and completed fields remain `0` | Stop and require separate datagram policy. |
| Public/LAN exposure | Public and LAN allowed/opened fields remain `0` | Reject as out of scope. |
| Real client overreach | Steam and real client fields remain `0` | Reject as out of scope. |
| Connect/post-connect/signon creep | Runtime path allowed and invoked fields remain `0` | Reject as out of scope. |
| Netchan runtime creep | `netchan_runtime_allowed_now=0`, `netchan_runtime_started=0` | Reject as out of scope. |
| Qport evidence overclaim | `qport_evidence_promotion_allowed_now=0` | Require a separate byte-evidence prompt. |
| Address-scoped challenge overclaim | `address_scoped_challenge_reusable_as_real_netchan_proof=0` | Reject promotion to real netchan proof. |
| CI drift bypass | Implementation, execution, and runtime skeleton CI drift gates remain required | Stop and rerun drift validation. |
| Wrapper bypass | Wrapper plan and validate remain required | Stop and rerun wrapper validation. |
| Stale docs | Stable docs and artifacts must agree with manifests | Update docs in a report-only prompt. |
| Compatibility overclaim | Diagnostic-only compatibility claim required | Reject expanded claims. |
