# Qport/Session Capture Execution Risk Review

Compatibility claim level: diagnostic-qport-session-capture-execution-policy-review-only; no real Steam Half-Life or HLDS-compatible client compatibility claimed

| Risk | Current guard | Required response |
| --- | --- | --- |
| Accidental execution | `capture_execution_allowed_now=0`, `capture_executed=0` | Treat as a blocking regression. |
| Socket open creep | `socket_open_allowed_now=0`, `socket_open_attempted=0` | Stop and require separate socket policy. |
| Datagram send/receive creep | `datagram_send_allowed_now=0`, `datagram_receive_allowed_now=0` | Stop and require separate datagram policy. |
| Public/LAN exposure | `public_socket_allowed_now=0`, `lan_socket_allowed_now=0` | Reject as out of scope. |
| Real client overreach | `real_client_allowed_now=0`, `real_client_binary_invoked=0` | Reject as out of scope. |
| Netchan runtime creep | `netchan_runtime_allowed_now=0`, `netchan_runtime_started=0` | Reject as out of scope. |
| Connect/post-connect/signon creep | Runtime path allowed and invoked fields remain `0` | Reject as out of scope. |
| Qport evidence overclaim | `qport_evidence_promotion_allowed_now=0` | Require a separate byte-evidence prompt. |
| Address-scoped challenge overclaim | `address_scoped_challenge_reusable_as_real_netchan_proof=0` | Reject promotion to real netchan proof. |
| CI drift bypass | Runtime skeleton CI drift gate remains required | Stop and rerun drift validation. |
| Wrapper bypass | Wrapper plan and validate remain required | Stop and rerun wrapper validation. |
| Cleanup or timeout missing | Future preconditions require cleanup and timeout policy | Deny execution policy until locked. |
| Compatibility overclaim | Diagnostic-only compatibility claim required | Reject expanded claims. |
