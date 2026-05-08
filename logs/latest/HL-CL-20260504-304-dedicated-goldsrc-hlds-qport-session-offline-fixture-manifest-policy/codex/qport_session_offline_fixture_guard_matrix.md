# Qport/Session Offline Fixture Guard Matrix

| Guard | Stable policy field | Required value | Failure reason |
| --- | --- | --- | --- |
| No real client | `real_client_used` | false | `real_client_claim_rejected` |
| No Steam | `compatibility_claim` and simulated violation fields | no real Steam claim | `real_client_claim_rejected` |
| No public socket | `public_socket_opened` | false | `public_socket_claim_rejected` |
| No LAN socket | `lan_socket_opened` | false | `lan_socket_claim_rejected` |
| No capture execution | `capture_executed` | false | `capture_execution_forbidden` |
| No socket at all | `socket_opened` | false | `socket_claim_rejected` |
| No qport promotion without bytes | `byte_level_evidence_sufficient` | false | `qport_promoted_without_byte_evidence` |
| No address challenge to real netchan proof | family/stage/promotion policy | diagnostic prerequisite only | `address_challenge_not_real_netchan_proof` |
| No netchan runtime | `netchan_started` | false | `netchan_runtime_forbidden` |
| No signon runtime | `signon_started` | false | `signon_runtime_forbidden` |
| No connect/post-connect runtime | stage policy | no runtime stages | `runtime_stage_forbidden` |
| No real compatibility claim | `compatibility_claim` | allowed enum only | `real_compatibility_claim_rejected` |
| No query/info stage confusion | `forbidden_stages` | query/info forbidden for qport/session | `query_info_not_qport_session` |

## Required Always-Zero Runtime Fields

| Field | Value |
| --- | --- |
| `capture_executed` | 0 |
| `real_client_binary_invoked` | 0 |
| `socket_open_attempted` | 0 |
| `public_socket_opened` | 0 |
| `lan_socket_opened` | 0 |
| `loopback_udp_socket_opened` | 0 |
| `connect_path_invoked` | 0 |
| `post_connect_serverinfo_path_invoked` | 0 |
| `signon_serverinfo_path_invoked` | 0 |
| `netchan_runtime_started` | 0 |
