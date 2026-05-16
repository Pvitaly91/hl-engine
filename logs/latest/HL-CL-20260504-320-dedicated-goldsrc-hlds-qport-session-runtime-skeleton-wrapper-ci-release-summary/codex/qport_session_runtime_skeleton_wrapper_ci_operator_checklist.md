# Qport/Session Runtime Skeleton Wrapper CI Operator Checklist

Compatibility claim level: diagnostic-qport-session-runtime-skeleton-wrapper-ci-release-summary-only; no real Steam Half-Life or HLDS-compatible client compatibility claimed

## Inspect The Boundary

- Inspect `fixtures/diagnostic/hlds/qport_session/qport_session_capture_runtime_skeleton_ci_manifest.json` before changing runtime skeleton, fixture, policy, dry-run, shell CI, wrapper, or release-boundary files.
- Inspect `fixtures/diagnostic/hlds/qport_session/qport_session_capture_shell_ci_manifest.json` before changing the shell boundary.
- Inspect `fixtures/diagnostic/hlds/qport_session/qport_session_offline_fixture_manifest_policy.json` and `fixtures/diagnostic/hlds/qport_session/qport_session_capture_preflight_dry_run_manifest.json` before changing fixtures or dry-run plans.
- Inspect `scripts/run_hlds_qport_session_capture_dry_run.ps1` before changing wrapper plan or validate behavior.
- Inspect prompt 316 final policy gate artifacts before any future capture execution policy discussion.

## Required One Fields

- `capture_blocked_by_policy=1`
- `runtime_skeleton_added=1`
- `skeleton_plan_created=1`
- `skeleton_plan_validated=1`
- `runtime_skeleton_ci_manifest_created=1`
- `runtime_skeleton_drift_gate_passed=1`
- `final_policy_gate_passed=1`
- `ci_drift_gate_passed=1`
- `offline_fixture_validator_passed=1`
- `capture_policy_gate_passed=1`
- `dry_run_validator_passed=1`
- `wrapper_validation_passed=1`
- `shell_boundary_validated=1`
- `address_scoped_challenge_reusable_as_diagnostic_prerequisite=1`

## Required Zero Fields

- `runtime_skeleton_drift_detected=0`
- `policy_drift_detected=0`
- `dry_run_manifest_drift_detected=0`
- `shell_ci_manifest_drift_detected=0`
- `wrapper_drift_detected=0`
- `dependency_drift_detected=0`
- `capture_allowed_now=0`
- `capture_implementation_added=0`
- `capture_executed=0`
- `capture_runtime_executed=0`
- `datagram_sent=0`
- `datagram_received=0`
- `qport_session_byte_evidence_sufficient=0`
- `byte_level_qport_session_evidence_sufficient=0`
- `address_scoped_challenge_reusable_as_real_netchan_proof=0`
- `compatibility_claim_expanded=0`
- `real_client_capture_allowed_now=0`
- `real_steam_client_used=0`
- `real_client_binary_invoked=0`
- `socket_open_attempted=0`
- `public_socket_opened=0`
- `lan_socket_opened=0`
- `loopback_udp_socket_opened=0`
- `connect_path_invoked=0`
- `post_connect_serverinfo_path_invoked=0`
- `signon_serverinfo_path_invoked=0`
- `netchan_runtime_started=0`
- `normal_host_behavior_changed=0`

## Drift Response

If any drift marker becomes `1`, stop and run a drift-hardening prompt before discussing capture execution. If any blocked runtime field becomes `1`, treat it as a boundary regression. Do not open sockets, send or receive datagrams, invoke real clients, run connect/post-connect/signon/netchan paths, promote qport byte evidence, or expand compatibility claims from this boundary.
