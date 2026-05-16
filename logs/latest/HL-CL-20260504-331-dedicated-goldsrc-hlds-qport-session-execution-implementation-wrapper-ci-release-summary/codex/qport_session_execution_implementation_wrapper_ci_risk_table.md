# HL-CL-20260504-331 Boundary Risk Table

Compatibility claim level: diagnostic-qport-session-execution-implementation-wrapper-ci-release-summary-only; no real Steam Half-Life or HLDS-compatible client compatibility claimed

| Risk | Guard | Required response |
| --- | --- | --- |
| Fixture drift | Implementation skeleton CI manifest, file hashes, and offline fixture validator | Stop and review fixture policy. |
| Policy drift | Policy hash, policy review marker, and capture policy gate | Stop and review policy before any runtime discussion. |
| Dry-run manifest drift | Dry-run manifest hash and validator | Stop and harden the dry-run manifest. |
| Shell CI manifest drift | Shell CI manifest hash and shell drift gate | Stop and restore shell CI boundary. |
| Runtime skeleton CI manifest drift | Runtime skeleton CI manifest hash and drift gate | Stop and refresh manifest only through an explicit drift prompt. |
| Execution skeleton CI manifest drift | Execution skeleton CI manifest hash and drift gate | Stop and refresh manifest only through an explicit drift prompt. |
| Implementation skeleton CI manifest drift | Implementation skeleton CI manifest and drift gate | Stop and refresh manifest only through an explicit drift prompt. |
| Wrapper drift | Wrapper hash, plan marker, and validate marker | Stop and harden wrapper plan/validate behavior. |
| Dependency drift | Policy review, execution skeleton CI, runtime skeleton CI, validators, and wrapper markers | Restore required dependencies before continuing. |
| Capture accidentally enabled | `capture_allowed_now=0` and `capture_execution_allowed_now=0` | Treat as a blocking regression. |
| Capture execution accidentally triggered | `capture_executed=0` | Treat as a blocking regression. |
| Capture runtime accidentally executed | `capture_runtime_executed=0` | Treat as a blocking regression. |
| Socket accidentally opened | `socket_open_attempted=0` | Treat as a blocking regression. |
| Loopback socket accidentally opened | `loopback_udp_socket_opened=0` | Treat as a blocking regression requiring separate loopback policy. |
| Datagram send/receive accidentally enabled | Datagram allowed and completed fields remain `0` | Treat as a blocking regression. |
| Public/LAN overreach | Public and LAN allowed/opened fields remain `0` | Reject as out of scope. |
| Real client overreach | Steam and real client fields remain `0` | Reject as out of scope. |
| Connect/post-connect/signon path creep | Runtime path fields remain `0` | Reject as out of scope. |
| Netchan runtime creep | `netchan_runtime_started=0` | Reject as out of scope. |
| Qport evidence promotion | Byte evidence fields remain `0` | Require a separate evidence prompt. |
| Address-scoped challenge overclaim | Real-netchan proof field remains `0` | Reject promotion. |
| Compatibility claim expansion | Diagnostic-only claim string required | Reject release. |
