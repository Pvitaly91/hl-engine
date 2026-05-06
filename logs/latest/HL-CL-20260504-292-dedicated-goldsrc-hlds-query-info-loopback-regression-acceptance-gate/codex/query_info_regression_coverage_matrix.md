# Query/Info Regression Coverage Matrix

| scenario | proof | accepted | rejected | reason | acceptance | component gates | public | lan | connect_sent |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| happy | pass | 1 | 0 | <none> | 1 | 4/4 | 0 | 0 | 0 |
| gate_disabled_by_default | pass | 0 | 1 | query_info_regression_acceptance_disabled | 0 | 0/4 | 0 | 0 | 0 |
| gate_no_real_client_used | pass | 1 | 0 | <none> | 1 | 4/4 | 0 | 0 | 0 |
| gate_public_socket_blocked | pass | 0 | 1 | public_socket_blocked | 0 | 0/4 | 0 | 0 | 0 |
| gate_lan_socket_blocked | pass | 0 | 1 | lan_socket_blocked | 0 | 0/4 | 0 | 0 | 0 |
| gate_non_loopback_client_denied | pass | 0 | 1 | non_loopback_client_denied | 0 | 0/4 | 0 | 0 | 0 |
| gate_connect_attempt_blocked | pass | 0 | 1 | connect_not_allowed_in_query_info_acceptance | 0 | 0/4 | 0 | 0 | 0 |
| gate_post_connect_stage_confusion_rejected | pass | 0 | 1 | connectionless_query_not_post_connect_evidence | 0 | 0/4 | 0 | 0 | 0 |
| gate_signon_stage_confusion_rejected | pass | 0 | 1 | connectionless_query_not_signon_evidence | 0 | 0/4 | 0 | 0 | 0 |
| gate_unresolved_post_connect_rejected | pass | 0 | 1 | unresolved_post_connect_serverinfo_not_buildable | 0 | 0/4 | 0 | 0 | 0 |
| gate_unresolved_signon_rejected | pass | 0 | 1 | unresolved_signon_serverinfo_not_buildable | 0 | 0/4 | 0 | 0 | 0 |
| gate_real_compatibility_claim_rejected | pass | 0 | 1 | real_compatibility_claim_rejected | 0 | 0/4 | 0 | 0 | 0 |
| gate_query_info_builder_required | pass | 0 | 1 | query_info_builder_parser_required | 0 | 0/4 | 0 | 0 | 0 |
| gate_query_info_loopback_required | pass | 0 | 1 | query_info_loopback_swap_required | 0 | 2/4 | 0 | 0 | 0 |
| gate_query_client_smoke_required | pass | 0 | 1 | query_client_smoke_required | 0 | 3/4 | 0 | 0 | 0 |
| gate_shutdown_cleanup | pass | 1 | 0 | <none> | 1 | 4/4 | 0 | 0 | 0 |

All scenarios preserve the connectionless-only boundary and keep public/LAN sockets, real clients, connect, post-connect, signon, auth, netchan, resources, and admission blocked.
