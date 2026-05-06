# Diagnostic Query/Info Loopback Query-Client Smoke

Prompt: HL-CL-20260504-291-dedicated-goldsrc-hlds-connectionless-query-info-byte-level-loopback-query-client-policy-gated-smoke

Status: pass

Compatibility claim level: diagnostic-query-info-loopback-query-client-smoke-only; no real Steam Half-Life or HLDS-compatible client compatibility claimed

## Scope

This source change adds a disabled-by-default diagnostic query-client smoke harness for connectionless query/info only. It wraps the prompt 289 loopback query/info response swap and keeps the response path constrained to the checked-in connectionless_query_info_candidate fixture.

No real Steam Half-Life client, real query client binary, public socket, LAN socket, connect path, post-connect serverinfo path, signon serverinfo path, auth, netchan, resource baseline, or client admission path is used.

## Happy Path Evidence

- diagnostic_query_client_used: 1
- real_query_client_used: 0
- real_steam_client_used: 0
- real_client_binary_invoked: 0
- query_client_socket_opened: 1
- query_client_loopback_only: 1
- public_socket_opened: 0
- lan_socket_opened: 0
- loopback_udp_socket_opened: 1
- client_query_info_request_sent: 1
- client_query_info_response_received: 1
- client_query_info_response_shape_valid: 1
- selected_fixture_id: connectionless_query_info_candidate
- selected_fixture_stage: connectionless_query
- byte_level_connectionless_query_builder_complete: 1
- byte_level_connectionless_query_parser_complete: 1

## Boundary Evidence

- connectionless_query_not_post_connect: 1
- connectionless_query_not_signon: 1
- connect_datagram_sent: 0
- connect_path_invoked: 0
- post_connect_serverinfo_path_invoked: 0
- signon_serverinfo_path_invoked: 0
- challenge_cache_required: 0
- real_post_connect_builder_complete: 0
- real_signon_builder_complete: 0
- real_wire_builder_complete: 0

## Runtime Proofs

- happy: pass
- gate_disabled_by_default: pass
- gate_no_real_client_used: pass
- gate_public_socket_blocked: pass
- gate_lan_socket_blocked: pass
- gate_non_loopback_client_denied: pass
- gate_wrong_query_command: pass
- gate_bad_marker_or_header: pass
- gate_wrong_opcode_or_tag: pass
- gate_response_timeout_bounded: pass
- gate_connect_attempt_blocked: pass
- gate_post_connect_stage_confusion_rejected: pass
- gate_signon_stage_confusion_rejected: pass
- gate_unresolved_post_connect_rejected: pass
- gate_unresolved_signon_rejected: pass
- gate_real_compatibility_claim_rejected: pass
- gate_shutdown_cleanup: pass

