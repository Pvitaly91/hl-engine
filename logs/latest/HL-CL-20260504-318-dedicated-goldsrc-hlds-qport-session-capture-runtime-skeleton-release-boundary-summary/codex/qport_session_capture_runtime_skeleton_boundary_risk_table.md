# Qport/session runtime skeleton boundary risk table

| Risk | Guard | Required response |
| --- | --- | --- |
| Skeleton misuse | Disabled-by-default probe and explicit scenario gates | Reject unsafe invocation. |
| Accidental capture execution | capture_executed=0 | Stop and regress the skeleton. |
| Accidental capture runtime execution | capture_runtime_executed=0 | Stop and regress the skeleton. |
| Accidental socket open | socket_open_attempted=0 | Stop and remove socket behavior. |
| Accidental datagram send/receive | datagram_sent=0 and datagram_received=0 | Stop and remove datagram behavior. |
| Loopback capture overreach | loopback_udp_socket_opened=0 and no capture execution | Require separate policy before active loopback capture. |
| Public/LAN exposure | public_socket_opened=0 and lan_socket_opened=0 | Reject as critical regression. |
| Real client action | real_steam_client_used=0 and real_client_binary_invoked=0 | Reject as critical regression. |
| Connect/post-connect/signon path creep | connect_path_invoked=0, post_connect_serverinfo_path_invoked=0, signon_serverinfo_path_invoked=0 | Reject skeleton expansion. |
| Netchan runtime creep | netchan_runtime_started=0 | Reject skeleton expansion. |
| Qport evidence promotion | qport evidence fields remain 0 | Require separate evidence prompt. |
| Address-scoped challenge real-netchan overclaim | address_scoped_challenge_reusable_as_real_netchan_proof=0 | Reject promotion. |
| CI drift gate bypass | ci_drift_gate_passed=1 required | Block skeleton plan creation. |
| Wrapper validation bypass | wrapper_validation_passed=1 required | Block skeleton plan creation. |
| Stale docs | Stable doc tracks files and commits | Update docs with any boundary change. |
| Compatibility overclaim | Diagnostic-only compatibility claim | Reject release. |
