# Qport/Session Readiness Wrapper CI Boundary Risk Table

Prompt: HL-CL-20260504-342-dedicated-goldsrc-hlds-qport-session-capture-execution-readiness-wrapper-ci-release-summary

| Risk | Required control |
| --- | --- |
| Fixture drift | Keep fixture hashes and offline validator checks active. |
| Policy drift | Keep capture policy gate and readiness policy review required. |
| Dry-run manifest drift | Keep dry-run validator required before readiness conclusions. |
| Shell CI manifest drift | Keep shell boundary documented through wrapper/CI release artifacts. |
| Runtime skeleton CI manifest drift | Require runtime skeleton CI drift gate before readiness work. |
| Execution skeleton CI manifest drift | Require execution skeleton CI drift gate before readiness work. |
| Implementation CI manifest drift | Require execution implementation CI drift gate before readiness work. |
| Readiness CI manifest drift | Require readiness CI drift gate before socket/datagram policy discussion. |
| Wrapper drift | Keep wrapper plan/validate checks as prerequisite evidence. |
| Dependency drift | Fail readiness boundary if required docs, manifests, or wrappers are removed. |
| Readiness plan treated as execution permission | State that readiness plan is policy/plan-only and does not authorize execution. |
| Timeout/cleanup policy gap | Keep timeout_cleanup_policy_defined=1 before any future execution-readiness planning. |
| Artifact schema lock gap | Keep artifact_schema_lock_defined=1 before any future runtime artifact discussion. |
| Socket policy bypass | Keep socket_policy_review_required=1 and socket_open_allowed_now=0. |
| Datagram policy bypass | Keep datagram_policy_review_required=1 and datagram_send_allowed_now=0. |
| Capture accidentally enabled | Keep capture_allowed_now=0 and capture_blocked_by_policy=1. |
| Capture execution accidentally triggered | Keep capture_executed=0 and capture_execution_allowed_now=0. |
| Capture runtime accidentally executed | Keep capture_runtime_executed=0 and capture_runtime_allowed_now=0. |
| Socket accidentally opened | Keep socket_open_attempted=0 and socket_open_allowed_now=0. |
| Loopback socket accidentally opened | Keep loopback_udp_socket_opened=0 and loopback_socket_allowed_now=0. |
| Datagram send/receive accidentally enabled | Keep datagram_sent=0, datagram_received=0, and datagram allowed fields at zero. |
| Public/LAN overreach | Keep public_socket_opened=0 and lan_socket_opened=0. |
| Real client overreach | Keep real_client_binary_invoked=0 and real_steam_client_used=0. |
| Connect/post-connect/signon path creep | Keep connect_path_invoked=0, post_connect_serverinfo_path_invoked=0, and signon_serverinfo_path_invoked=0. |
| Netchan runtime creep | Keep netchan_runtime_started=0 and netchan_runtime_allowed_now=0. |
| Qport evidence promotion | Keep qport_session_byte_evidence_sufficient=0 and qport_evidence_promotion_allowed_now=0. |
| Address-scoped challenge overclaim | Keep address_scoped_challenge_reusable_as_real_netchan_proof=0. |
| Compatibility claim expansion | Keep compatibility_claim_expansion_allowed_now=0 and compatibility_claim_expanded=0. |

