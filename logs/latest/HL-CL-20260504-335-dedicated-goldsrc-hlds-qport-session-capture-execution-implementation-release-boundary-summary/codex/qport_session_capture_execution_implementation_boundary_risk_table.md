# HL-CL-20260504-335 Execution Implementation Boundary Risk Table

Compatibility claim level: diagnostic-qport-session-capture-execution-implementation-release-boundary-only; no real Steam Half-Life or HLDS-compatible client compatibility claimed

| Risk | Guard | Required response |
| --- | --- | --- |
| Implementation surface misuse | Disabled-by-default probe and deterministic plan-only behavior | Stop and restore probe-only scope. |
| Accidental capture execution | `capture_execution_allowed_now=0`, `capture_executed=0` | Treat as blocking regression. |
| Accidental capture runtime execution | `capture_runtime_allowed_now=0`, `capture_runtime_executed=0` | Treat as blocking regression. |
| Accidental socket open | `socket_open_allowed_now=0`, `socket_open_attempted=0` | Require separate socket policy. |
| Accidental loopback socket open | `loopback_socket_allowed_now=0`, `loopback_udp_socket_opened=0` | Require separate loopback policy. |
| Accidental datagram send/receive | Datagram allowed and completed fields remain `0` | Require separate datagram policy. |
| Public/LAN exposure | Public and LAN allowed/opened fields remain `0` | Reject as out of scope. |
| Real client action | Steam and real-client fields remain `0` | Reject as out of scope. |
| Connect/post-connect/signon path creep | Runtime path fields remain `0` | Reject as out of scope. |
| Netchan runtime creep | `netchan_runtime_started=0` | Reject as out of scope. |
| Qport evidence promotion | Byte evidence fields remain `0` | Require separate byte-evidence prompt. |
| Address-scoped challenge real-netchan overclaim | `address_scoped_challenge_reusable_as_real_netchan_proof=0` | Reject promotion to real netchan proof. |
| Final gate bypass | `final_gate_invoked=1`, `final_gate_passed=1` | Stop and rerun final gate validation. |
| Implementation skeleton CI drift gate bypass | `implementation_skeleton_ci_drift_gate_passed=1` | Stop and rerun drift validation. |
| Wrapper validation bypass | `wrapper_validation_checked=1`, `wrapper_validation_passed=1` | Stop and rerun wrapper validation. |
| Stale docs | Stable docs and prompt artifacts must match manifests | Update in a report-only prompt. |
| Compatibility overclaim | Diagnostic-only compatibility string required | Reject expanded claims. |
