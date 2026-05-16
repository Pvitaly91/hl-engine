# HL-CL-20260504-331 Operator Checklist

Compatibility claim level: diagnostic-qport-session-execution-implementation-wrapper-ci-release-summary-only; no real Steam Half-Life or HLDS-compatible client compatibility claimed

## Inspect Manifests

- Inspect implementation skeleton CI manifest: `fixtures/diagnostic/hlds/qport_session/qport_session_capture_execution_implementation_skeleton_ci_manifest.json`
- Inspect execution skeleton CI manifest: `fixtures/diagnostic/hlds/qport_session/qport_session_capture_execution_skeleton_ci_manifest.json`
- Inspect runtime skeleton CI manifest: `fixtures/diagnostic/hlds/qport_session/qport_session_capture_runtime_skeleton_ci_manifest.json`
- Inspect shell CI manifest: `fixtures/diagnostic/hlds/qport_session/qport_session_capture_shell_ci_manifest.json`
- Inspect dry-run manifest: `fixtures/diagnostic/hlds/qport_session/qport_session_capture_preflight_dry_run_manifest.json`
- Inspect policy file: `fixtures/diagnostic/hlds/qport_session/qport_session_offline_fixture_manifest_policy.json`

## Run Diagnostic Checks Only

- Implementation skeleton drift gate happy proof may be run only as an explicit bounded diagnostic probe with `--hlds-qport-session-execution-implementation-skeleton-ci-drift-gate-probe` and scenario `happy`.
- Wrapper plan/validate may be run only through the existing diagnostic dry-run wrapper modes.
- No capture, socket, datagram, public/LAN, real-client, connect, post-connect, signon, or netchan runtime proof is part of this release summary.

## Fields That Must Remain One

- `execution_implementation_skeleton_ci_manifest_created`
- `execution_implementation_skeleton_drift_gate_passed`
- `execution_skeleton_ci_manifest_created`
- `execution_skeleton_drift_gate_passed`
- `runtime_skeleton_ci_manifest_created`
- `runtime_skeleton_drift_gate_passed`
- `wrapper_plan_passed`
- `wrapper_validate_passed`
- `policy_review_passed`
- `execution_implementation_skeleton_added`
- `implementation_execution_plan_created`
- `implementation_execution_plan_validated`
- `capture_blocked_by_policy`
- `address_scoped_challenge_reusable_as_diagnostic_prerequisite`
- `future_minimal_no_client_execution_implementation_allowed_next`

## Fields That Must Remain Zero

- `implementation_skeleton_drift_detected`
- `policy_review_drift_detected`
- `policy_drift_detected`
- `dry_run_manifest_drift_detected`
- `execution_skeleton_ci_manifest_drift_detected`
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

- If implementation skeleton drift appears, stop and harden prompt 330's manifest/gate.
- If policy, fixture, manifest, or wrapper drift appears, stop and repair that boundary before considering any execution implementation gate.
- If any blocked field becomes `1`, treat the boundary as failed.
- If `capture_executed`, `socket_open_attempted`, `datagram_sent`, or `datagram_received` becomes `1`, stop immediately and create a regression prompt.
- If qport/session byte evidence or compatibility expansion becomes allowed, stop and reject the release boundary.
