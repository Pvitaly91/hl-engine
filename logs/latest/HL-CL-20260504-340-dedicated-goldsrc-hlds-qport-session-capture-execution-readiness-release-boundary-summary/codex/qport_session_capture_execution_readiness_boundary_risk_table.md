# Qport/session Capture Execution Readiness Boundary Risk Table

| Risk | Current guard | Required response |
| --- | --- | --- |
| Readiness gate misuse | Gate remains disabled by default and probe-only | Reject use as execution permission. |
| Readiness plan treated as execution permission | Plan fields define policy requirements only | Reject and restate that execution remains blocked. |
| Accidental capture execution | `capture_execution_allowed_now=0`, `capture_executed=0` | Treat as a blocking regression. |
| Accidental capture runtime execution | `capture_runtime_allowed_now=0`, `capture_runtime_executed=0` | Treat as a blocking regression. |
| Accidental socket open | `socket_open_allowed_now=0`, `socket_open_attempted=0` | Stop and require separate socket policy. |
| Accidental loopback socket open | `loopback_socket_allowed_now=0`, `loopback_udp_socket_opened=0` | Stop and require separate loopback policy. |
| Accidental datagram send/receive | Datagram allowed and completed fields remain `0` | Stop and require separate datagram policy. |
| Public/LAN exposure | Public and LAN allowed/opened fields remain `0` | Reject as out of scope. |
| Real client action | Steam and real client invocation fields remain `0` | Reject as out of scope. |
| Connect/post-connect/signon path creep | Runtime path allowed and invoked fields remain `0` | Reject as out of scope. |
| Netchan runtime creep | `netchan_runtime_allowed_now=0`, `netchan_runtime_started=0` | Reject as out of scope. |
| Qport evidence promotion | Byte evidence fields remain `0` | Require a separate evidence prompt. |
| Address-scoped challenge real-netchan overclaim | `address_scoped_challenge_reusable_as_real_netchan_proof=0` | Reject promotion to real netchan proof. |
| Timeout/cleanup policy gap | `timeout_cleanup_policy_defined=1` | Stop and restore readiness plan requirements. |
| Artifact schema lock gap | `artifact_schema_lock_defined=1` | Stop and restore artifact schema requirements. |
| Socket/datagram policy bypass | `socket_policy_review_required=1`, `datagram_policy_review_required=1` | Reject socket/datagram work. |
| Wrapper validation bypass | `wrapper_validation_passed=1` | Stop and rerun wrapper validation in bounded diagnostic mode only if separately approved. |
| Stale docs | Stable docs and prompt artifacts must agree | Update docs in a report-only prompt. |
| Compatibility overclaim | Diagnostic-only compatibility claim required | Reject expanded claims. |
