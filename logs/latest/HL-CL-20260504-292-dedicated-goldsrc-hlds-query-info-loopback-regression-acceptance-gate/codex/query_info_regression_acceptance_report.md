# Query/Info Loopback Regression Acceptance Report

Prompt: HL-CL-20260504-292-dedicated-goldsrc-hlds-query-info-loopback-regression-acceptance-gate

Compatibility claim level: diagnostic-query-info-loopback-regression-acceptance-only; no real Steam Half-Life or HLDS-compatible client compatibility claimed

## Boundary

This is diagnostic-only. The gate consolidates prompts 287-291 as the connectionless query/info regression boundary. It selects only `connectionless_query_info_candidate`, verifies the byte-level builder/parser, validates the diagnostic path integration, exercises the loopback response swap, and proves the in-repo diagnostic query-client smoke path.

## Proven

- fixture-backed byte-level query/info builder/parser works
- query/info diagnostic path integration works
- loopback query/info response swap works
- diagnostic query-client smoke works over loopback only
- public and LAN socket exposure remain blocked
- real query clients, Steam clients, and real client binaries remain unused
- connect, post-connect serverinfo, signon serverinfo, auth, netchan, resources, baselines, and admission remain untouched

## Not Claimed

No real Steam Half-Life client compatibility is claimed. No real HLDS-compatible client compatibility is claimed. Query/info remains connectionless-only and is not post-connect or signon serverinfo.

## Happy Summary

| field | value |
| --- | --- |
| query_info_regression_acceptance_enabled | 1 |
| query_info_regression_acceptance_disabled_by_default | 1 |
| query_info_regression_acceptance_passed | 1 |
| query_info_regression_gates_total | 4 |
| query_info_regression_gates_passed | 4 |
| query_info_regression_gates_failed | 0 |
| query_info_builder_parser_passed | 1 |
| query_info_path_integration_passed | 1 |
| query_info_loopback_swap_passed | 1 |
| query_client_smoke_passed | 1 |
| selected_fixture_id | connectionless_query_info_candidate |
| selected_fixture_stage | connectionless_query |
| byte_level_connectionless_query_builder_complete | 1 |
| byte_level_connectionless_query_parser_complete | 1 |
| client_query_info_response_received | 1 |
| client_query_info_response_shape_valid | 1 |
| connectionless_query_not_post_connect | 1 |
| connectionless_query_not_signon | 1 |
| post_connect_byte_evidence_sufficient | 0 |
| signon_time_byte_evidence_sufficient | 0 |
| real_post_connect_builder_complete | 0 |
| real_signon_builder_complete | 0 |
| real_wire_builder_complete | 0 |
| diagnostic_query_client_used | 1 |
| real_query_client_used | 0 |
| real_steam_client_used | 0 |
| real_client_binary_invoked | 0 |
| public_socket_opened | 0 |
| query_client_public_socket_opened | 0 |
| server_public_socket_opened | 0 |
| lan_socket_opened | 0 |
| loopback_udp_socket_opened | 1 |
| sockets_closed | 1 |
| connect_datagram_sent | 0 |
| connect_path_invoked | 0 |
| post_connect_serverinfo_path_invoked | 0 |
| signon_serverinfo_path_invoked | 0 |
| normal_host_behavior_changed | 0 |
| real_client_smoke_allowed_now | 0 |
| real_query_client_allowed_now | 0 |
| public_socket_exposure_allowed_now | 0 |
| lan_socket_exposure_allowed_now | 0 |
| steam_auth_not_implemented | 1 |
| netchan_not_started | 1 |
| reliable_channel_not_started | 1 |
| resource_baselines_not_sent | 1 |
| signon_state_not_entered | 1 |
| client_not_put_in_server | 1 |
| auth_not_started | 1 |
