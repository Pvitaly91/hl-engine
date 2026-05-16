# HL-CL-20260504-326 Operator Checklist

Compatibility claim level: diagnostic-qport-session-execution-skeleton-wrapper-ci-release-summary-only; no real Steam Half-Life or HLDS-compatible client compatibility claimed

## Inspect Manifests

- Inspect execution skeleton CI manifest: `Get-Content -Raw fixtures/diagnostic/hlds/qport_session/qport_session_capture_execution_skeleton_ci_manifest.json | ConvertFrom-Json`.
- Inspect runtime skeleton CI manifest: `Get-Content -Raw fixtures/diagnostic/hlds/qport_session/qport_session_capture_runtime_skeleton_ci_manifest.json | ConvertFrom-Json`.
- Inspect policy, dry-run manifest, shell CI manifest, and wrapper paths before any proof rerun.

## Safe Bounded Checks

- Execution skeleton drift gate happy proof is optional and must remain explicit, bounded, diagnostic-only, and `--frames 1`.
- Wrapper plan/validate checks may be inspected through prompt 315/320/325 summaries or rerun only in diagnostic dry-run modes.
- Final execution policy status is sourced from prompt 322 and prompt 325 drift summaries.

## Fields That Must Remain 1

- `capture_blocked_by_policy`
- `address_scoped_challenge_reusable_as_diagnostic_prerequisite`
- `execution_skeleton_ci_manifest_created`
- `execution_skeleton_drift_gate_passed`
- `runtime_skeleton_ci_manifest_created`
- `runtime_skeleton_drift_gate_passed`
- `wrapper_plan_passed`
- `wrapper_validate_passed`
- `final_execution_policy_gate_passed`
- `execution_skeleton_added`
- `execution_plan_created`
- `execution_plan_validated`
- `offline_fixture_validator_passed`
- `capture_policy_gate_passed`
- `dry_run_validator_passed`
- `wrapper_validation_passed`

## Fields That Must Remain 0

- `execution_skeleton_drift_detected`
- `policy_drift_detected`
- `dry_run_manifest_drift_detected`
- `runtime_skeleton_ci_manifest_drift_detected`
- `wrapper_drift_detected`
- `dependency_drift_detected`
- `capture_execution_allowed_now`
- `capture_runtime_allowed_now`
- `socket_open_allowed_now`
- `loopback_socket_allowed_now`
- `public_socket_allowed_now`
- `lan_socket_allowed_now`
- `datagram_send_allowed_now`
- `datagram_receive_allowed_now`
- `real_client_allowed_now`
- `connect_path_allowed_now`
- `post_connect_serverinfo_allowed_now`
- `signon_serverinfo_allowed_now`
- `netchan_runtime_allowed_now`
- `qport_evidence_promotion_allowed_now`
- `compatibility_claim_expansion_allowed_now`
- `capture_allowed_now`
- `capture_implementation_added`
- `capture_executed`
- `capture_runtime_executed`
- `datagram_sent`
- `datagram_received`
- `qport_session_byte_evidence_sufficient`
- `byte_level_qport_session_evidence_sufficient`
- `address_scoped_challenge_reusable_as_real_netchan_proof`
- `compatibility_claim_expanded`
- `real_steam_client_used`
- `real_client_binary_invoked`
- `socket_open_attempted`
- `public_socket_opened`
- `lan_socket_opened`
- `loopback_udp_socket_opened`
- `connect_path_invoked`
- `post_connect_serverinfo_path_invoked`
- `signon_serverinfo_path_invoked`
- `netchan_runtime_started`
- `normal_host_behavior_changed`

## Drift Response

If any blocked field changes to `1`, stop. Treat it as a boundary regression and route to a separate hardening prompt before any execution, socket, datagram, real-client, runtime-stage, evidence, or compatibility discussion.
