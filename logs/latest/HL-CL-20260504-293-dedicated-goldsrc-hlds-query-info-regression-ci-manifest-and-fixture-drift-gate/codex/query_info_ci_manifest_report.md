# Query/Info CI Manifest Report

prompt_id: HL-CL-20260504-293-dedicated-goldsrc-hlds-query-info-regression-ci-manifest-and-fixture-drift-gate
compatibility_claim_level: diagnostic-query-info-ci-manifest-fixture-drift-gate-only; no real Steam Half-Life or HLDS-compatible client compatibility claimed
branch: codex/HL-CL-20260401-081-target-runtime-completion-state
pre_change_head: 8c2b3493a96c9bc57cf85e54a95d4eace36bf4a4
source_commit: 17839836b97dea9c75050deb61a869fa60750bf2
manifest_path: fixtures/diagnostic/hlds/query_info_regression/query_info_regression_ci_manifest.json
fixture_root: fixtures/diagnostic/hlds/serverinfo
selected_fixture_id: connectionless_query_info_candidate
selected_fixture_stage: connectionless_query

## Manifest Policy
- The query/info diagnostic boundary covers prompts 287 through 292.
- The manifest records SHA-256 hashes for fixture review and FNV1a64 hashes for the in-process drift gate.
- The drift policy fails selected fixture id or stage changes, real compatibility claims, unresolved post-connect/signon buildability, public/LAN or real-client policy removal, connect path allowance, and query/info promotion to post-connect or signon.
- No sockets or real client binaries are used by this drift gate.

## Required Positive Behaviors
- query_info_builder_parser_passed=1
- query_info_path_integration_passed=1
- query_info_loopback_swap_passed=1
- query_client_smoke_passed=1
- byte_level_connectionless_query_builder_complete=1
- byte_level_connectionless_query_parser_complete=1
- client_query_info_response_received=1
- client_query_info_response_shape_valid=1

## Required Blocked Behaviors
- real_query_client_used=0
- real_steam_client_used=0
- real_client_binary_invoked=0
- public_socket_opened=0
- lan_socket_opened=0
- connect_datagram_sent=0
- connect_path_invoked=0
- post_connect_serverinfo_path_invoked=0
- signon_serverinfo_path_invoked=0
- real_post_connect_builder_complete=0
- real_signon_builder_complete=0
- real_wire_builder_complete=0
