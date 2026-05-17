# HL-CL-20260504-335 Execution Implementation Boundary Checklist

Compatibility claim level: diagnostic-qport-session-capture-execution-implementation-release-boundary-only; no real Steam Half-Life or HLDS-compatible client compatibility claimed

## Before Running The Implementation Probe

- Confirm the probe is explicitly enabled and remains disabled by default outside that invocation.
- Confirm prompt 333 final gate is invoked and passed.
- Confirm prompt 330 implementation skeleton CI drift gate is invoked and passed.
- Confirm prompt 332 final implementation policy review is loaded and passed.
- Confirm prompt 325 execution skeleton CI drift gate is invoked and passed.
- Confirm prompt 319 runtime skeleton CI drift gate is invoked and passed.
- Confirm prompt 305 offline fixture validator is invoked and passed.
- Confirm prompt 306 capture policy gate is invoked and passed while denying capture.
- Confirm prompt 308 dry-run validator is invoked and passed.
- Confirm wrapper validation is checked and passed.

## Required Files

- `fixtures/diagnostic/hlds/qport_session/qport_session_offline_fixture_manifest_policy.json`
- `fixtures/diagnostic/hlds/qport_session/qport_session_capture_preflight_dry_run_manifest.json`
- `fixtures/diagnostic/hlds/qport_session/qport_session_capture_shell_ci_manifest.json`
- `fixtures/diagnostic/hlds/qport_session/qport_session_capture_runtime_skeleton_ci_manifest.json`
- `fixtures/diagnostic/hlds/qport_session/qport_session_capture_execution_skeleton_ci_manifest.json`
- `fixtures/diagnostic/hlds/qport_session/qport_session_capture_execution_implementation_skeleton_ci_manifest.json`
- `scripts/run_hlds_qport_session_capture_dry_run.ps1`
- `docs/diagnostic/hlds/qport_session_execution_implementation_wrapper_ci_release_summary.md`
- `docs/diagnostic/hlds/qport_session_capture_execution_implementation_final_policy_review.md`
- `docs/diagnostic/hlds/qport_session_capture_execution_implementation_release_boundary_summary.md`

## Expected Happy Fields

- `execution_implementation_enabled=1`
- `execution_implementation_disabled_by_default=1`
- `execution_implementation_surface_added=1`
- `implementation_plan_created=1`
- `implementation_plan_validated=1`
- `final_gate_invoked=1`
- `final_gate_passed=1`
- `implementation_skeleton_ci_drift_gate_invoked=1`
- `implementation_skeleton_ci_drift_gate_passed=1`
- `policy_review_loaded=1`
- `policy_review_passed=1`
- `execution_skeleton_ci_drift_gate_invoked=1`
- `execution_skeleton_ci_drift_gate_passed=1`
- `runtime_skeleton_ci_drift_gate_invoked=1`
- `runtime_skeleton_ci_drift_gate_passed=1`
- `offline_fixture_validator_invoked=1`
- `offline_fixture_validator_passed=1`
- `capture_policy_gate_invoked=1`
- `capture_policy_gate_passed=1`
- `dry_run_validator_invoked=1`
- `dry_run_validator_passed=1`
- `wrapper_validation_checked=1`
- `wrapper_validation_passed=1`
- `future_capture_execution_implementation_prompt_allowed_next=1`

## Expected Blocked Fields

- `capture_blocked_by_policy=1`
- `capture_block_reason=capture_implementation_not_allowed_yet`
- `capture_execution_allowed_now=0`
- `capture_runtime_allowed_now=0`
- `socket_open_allowed_now=0`
- `loopback_socket_allowed_now=0`
- `public_socket_allowed_now=0`
- `lan_socket_allowed_now=0`
- `datagram_send_allowed_now=0`
- `datagram_receive_allowed_now=0`
- `real_client_allowed_now=0`
- `connect_path_allowed_now=0`
- `post_connect_serverinfo_allowed_now=0`
- `signon_serverinfo_allowed_now=0`
- `netchan_runtime_allowed_now=0`
- `qport_evidence_promotion_allowed_now=0`
- `compatibility_claim_expansion_allowed_now=0`
- `capture_allowed_now=0`

## Fields That Must Remain Zero

- `capture_implementation_added`
- `capture_executed`
- `capture_runtime_executed`
- `datagram_sent`
- `datagram_received`
- `qport_session_byte_evidence_sufficient`
- `byte_level_qport_session_evidence_sufficient`
- `address_scoped_challenge_reusable_as_real_netchan_proof`
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

## Pass And Fail Criteria

Pass requires all prerequisite gates to pass, the implementation plan to be created and validated, and every blocked runtime/capture/socket/datagram/real-client/evidence/compatibility field to remain at its blocked value.

Fail if any required gate is missing, any blocked field flips to `1`, `capture_blocked_by_policy` flips to `0`, `capture_block_reason` is empty, or the compatibility claim expands beyond the diagnostic-only claim.

## Regression Responses

If `capture_executed` becomes `1`, stop and treat it as a boundary regression. If `socket_open_attempted` becomes `1`, stop and require a separate socket policy prompt. If `loopback_udp_socket_opened` becomes `1`, stop and require a separate loopback socket policy prompt. If `datagram_sent` or `datagram_received` becomes `1`, stop and require separate datagram policy. If `qport_session_byte_evidence_sufficient` becomes `1` without a separate evidence prompt, reject the result as an evidence overclaim. If `compatibility_claim_expansion_allowed_now` becomes `1`, reject the result as a compatibility overclaim.
