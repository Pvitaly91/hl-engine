# Qport/Session Capture Execution Skeleton Boundary Risk Table

| Risk | Guard | Required response |
| --- | --- | --- |
| execution skeleton misuse | disabled-by-default explicit probe plus plan-only summary | reject any use as execution permission |
| accidental capture execution | `capture_execution_allowed_now=0`, `capture_executed=0` | stop as release blocker |
| accidental capture runtime execution | `capture_runtime_allowed_now=0`, `capture_runtime_executed=0` | stop and harden runtime policy |
| accidental socket open | `socket_open_allowed_now=0`, `socket_open_attempted=0` | stop and require socket policy |
| accidental loopback socket open | `loopback_socket_allowed_now=0`, `loopback_udp_socket_opened=0` | stop and require loopback policy |
| accidental datagram send/receive | datagram allowed and completed fields remain `0` | stop and require datagram policy |
| public/LAN exposure | public and LAN allowed/opened fields remain `0` | reject as out of scope |
| real client action | real client allowed/used/invoked fields remain `0` | reject as out of scope |
| connect/post-connect/signon path creep | path allowed and invoked fields remain `0` | reject as out of scope |
| netchan runtime creep | `netchan_runtime_allowed_now=0`, `netchan_runtime_started=0` | reject as out of scope |
| qport evidence promotion | evidence promotion and sufficiency fields remain `0` | require separate byte-evidence prompt |
| address-scoped challenge real-netchan overclaim | `address_scoped_challenge_reusable_as_real_netchan_proof=0` | reject real-netchan proof reuse |
| final execution policy gate bypass | final execution policy gate must pass before plan creation | stop and restore dependency |
| runtime skeleton CI drift gate bypass | runtime skeleton CI drift gate must pass before plan creation | stop and rerun/harden drift validation |
| wrapper validation bypass | wrapper validation must pass before plan creation | stop and restore wrapper validation |
| stale docs | stable docs must match prompt artifacts | update docs in report-only prompt |
| compatibility overclaim | fixed diagnostic-only compatibility claim | reject expanded claims |
