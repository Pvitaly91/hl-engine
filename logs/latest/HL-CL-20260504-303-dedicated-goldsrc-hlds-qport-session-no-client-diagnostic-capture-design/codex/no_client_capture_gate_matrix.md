# No-Client Capture Gate Matrix

These gates are design targets for a later implementation. They were not executed in prompt 303.

| Gate | Intent | Required outcome |
| --- | --- | --- |
| happy_fixture_manifest_only | Validate manifest-only plan. | accept only if no sockets/clients/runtime stages occur. |
| gate_disabled_by_default | Prove opt-in requirement. | reject with `qport_session_no_client_capture_disabled`. |
| gate_real_client_requested | Reject Steam/client use. | reject with `real_client_forbidden`, `real_client_binary_invoked=0`. |
| gate_public_lan_requested | Reject public/LAN sockets. | reject with `public_lan_forbidden`, sockets remain closed. |
| gate_live_capture_requested | Reject live capture until explicitly approved. | reject with `live_capture_not_allowed`. |
| gate_stage_confusion_query_info | Reject query/info fixture as qport/session proof. | reject with `query_info_not_qport_session`. |
| gate_stage_confusion_post_connect | Reject post-connect/signon promotion. | reject and leave those paths uninvoked. |
| gate_missing_qport_contract | Reject unknown qport bytes as a builder contract. | reject with `qport_session_byte_contract_missing`. |

## Required Always-Zero Fields

| Field | Value |
| --- | --- |
| `real_steam_client_used` | 0 |
| `real_client_binary_invoked` | 0 |
| `public_socket_opened` | 0 |
| `lan_socket_opened` | 0 |
| `connect_path_invoked` | 0 |
| `post_connect_serverinfo_path_invoked` | 0 |
| `signon_serverinfo_path_invoked` | 0 |
| `netchan_runtime_started` | 0 |
| `normal_host_behavior_changed` | 0 |
