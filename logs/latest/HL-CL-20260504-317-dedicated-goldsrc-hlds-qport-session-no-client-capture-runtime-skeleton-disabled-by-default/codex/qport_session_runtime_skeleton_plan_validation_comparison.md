# Qport/session runtime skeleton plan validation comparison

## Happy plan

| field | value |
| --- | --- |
| runtime_skeleton_enabled | 1 |
| runtime_skeleton_disabled_by_default | 1 |
| skeleton_plan_created | 1 |
| skeleton_plan_validated | 1 |
| capture_implementation_allowed_next | 1 |
| capture_runtime_allowed_now | 0 |
| socket_open_allowed_now | 0 |
| public_socket_allowed_now | 0 |
| lan_socket_allowed_now | 0 |
| real_client_allowed_now | 0 |
| compatibility_claim_expansion_allowed_now | 0 |

## Required dependency gates

| gate | happy value |
| --- | ---: |
| final_policy_gate_passed | 1 |
| ci_drift_gate_passed | 1 |
| offline_fixture_validator_passed | 1 |
| capture_policy_gate_passed | 1 |
| dry_run_validator_passed | 1 |
| shell_boundary_validated | 1 |
| wrapper_validation_passed | 1 |

## Unsafe request mutations

Each unsafe mutation scenario was rejected before plan creation or runtime action. The generated summaries preserve capture_executed=0, capture_runtime_executed=0, datagram_sent=0, datagram_received=0, socket_open_attempted=0, public_socket_opened=0, lan_socket_opened=0, loopback_udp_socket_opened=0, real_steam_client_used=0, real_client_binary_invoked=0, connect_path_invoked=0, post_connect_serverinfo_path_invoked=0, signon_serverinfo_path_invoked=0, and netchan_runtime_started=0.
