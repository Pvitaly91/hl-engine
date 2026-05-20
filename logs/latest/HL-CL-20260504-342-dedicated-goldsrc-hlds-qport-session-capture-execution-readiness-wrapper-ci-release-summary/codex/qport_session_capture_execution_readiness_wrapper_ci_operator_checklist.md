# Qport/Session Readiness Wrapper CI Operator Checklist

Prompt: HL-CL-20260504-342-dedicated-goldsrc-hlds-qport-session-capture-execution-readiness-wrapper-ci-release-summary

## Inspect Boundary Inputs

- Inspect readiness CI manifest: `fixtures/diagnostic/hlds/qport_session/qport_session_capture_execution_readiness_ci_manifest.json`
- Inspect readiness release boundary: `docs/diagnostic/hlds/qport_session_capture_execution_readiness_release_boundary_summary.md`
- Inspect execution implementation CI manifest: `fixtures/diagnostic/hlds/qport_session/qport_session_capture_execution_implementation_ci_manifest.json`
- Inspect readiness CI manifest/drift doc: `docs/diagnostic/hlds/qport_session_capture_execution_readiness_ci_manifest_and_drift_gate.md`
- Inspect timeout/cleanup policy sketch in prompt 339 artifacts.
- Inspect artifact schema lock sketch in prompt 339 artifacts.
- Inspect wrapper script: `scripts/run_hlds_qport_session_capture_dry_run.ps1`

## Safe Checks

- Run only the readiness CI drift gate happy probe or wrapper plan/validate mode when a proof rerun is needed.
- Do not run capture, sockets, datagrams, public/LAN paths, real clients, connect, post-connect, signon, or netchan runtime.
- Confirm socket policy review remains required.
- Confirm datagram policy review remains required.
- Confirm readiness drift, policy drift, timeout/cleanup drift, artifact schema drift, socket policy drift, datagram policy drift, wrapper drift, and dependency drift remain zero.

## Fields That Must Remain One

- `readiness_ci_manifest_created`
- `readiness_drift_gate_passed`
- `execution_implementation_ci_manifest_created`
- `execution_implementation_drift_gate_passed`
- `wrapper_plan_passed`
- `wrapper_validate_passed`
- `readiness_gate_passed`
- `readiness_plan_created`
- `readiness_plan_validated`
- `readiness_policy_review_passed`
- `timeout_cleanup_policy_defined`
- `artifact_schema_lock_defined`
- `socket_policy_review_required`
- `datagram_policy_review_required`
- `capture_blocked_by_policy`
- `address_scoped_challenge_reusable_as_diagnostic_prerequisite`

## Fields That Must Remain Zero

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

- If any readiness drift field becomes one, stop and run a readiness drift-hardening prompt.
- If any policy, fixture, manifest, or wrapper drift appears, stop before any socket/datagram policy discussion.
- If any blocked field changes to one, treat the boundary as failed and do not proceed to execution or socket work.
- If compatibility claim expansion appears, revert the claim through a docs/policy hardening prompt before any runtime work.

