# No-Client Capture Safety Policy

## Required Policy Defaults

| Policy | Required value |
| --- | --- |
| Explicit opt-in | required |
| Disabled by default | yes |
| Real client allowed | no |
| Steam allowed | no |
| Public socket allowed | no |
| LAN socket allowed | no |
| Live capture allowed in prompt 303 | no |
| Runtime stage execution allowed in prompt 303 | no |
| Compatibility claim | diagnostic-only |

## Required Future Proof Fields

| Field | Required value |
| --- | --- |
| `qport_session_no_client_capture_disabled_by_default` | 1 |
| `qport_session_capture_executed` | 0 in design/manifest-only mode |
| `socket_open_attempted` | 0 in manifest-only mode |
| `real_steam_client_used` | 0 |
| `real_client_binary_invoked` | 0 |
| `public_socket_opened` | 0 |
| `lan_socket_opened` | 0 |
| `loopback_udp_socket_opened` | 0 in manifest-only mode |
| `connect_path_invoked` | 0 |
| `post_connect_serverinfo_path_invoked` | 0 |
| `signon_serverinfo_path_invoked` | 0 |
| `netchan_runtime_started` | 0 |
| `normal_host_behavior_changed` | 0 |

## Fail-Closed Conditions

| Condition | Reason |
| --- | --- |
| Missing compatibility claim | Prevents accidental overclaim. |
| Missing stage field | Prevents query/info, post-connect, or signon confusion. |
| Missing real-client blocker | Prevents accidental Steam/client execution. |
| Missing public/LAN blocker | Prevents exposure outside local diagnostic scope. |
| Unknown qport width/order in builder contract | Prevents invented bytes. |
| Any live socket in manifest-only mode | Violates no-client design boundary. |
