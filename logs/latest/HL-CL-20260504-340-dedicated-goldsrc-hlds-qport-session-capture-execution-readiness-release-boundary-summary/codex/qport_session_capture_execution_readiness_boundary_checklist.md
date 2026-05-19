# Qport/session Capture Execution Readiness Boundary Checklist

## Before Running Readiness Gate Probe

- Confirm branch `codex/HL-CL-20260401-081-target-runtime-completion-state`.
- Confirm prompt 339 source `e22b56166a3b2ea161a9776602b96513827f878c` and artifact `6c5fb6502b2bc176f3a982948a6959ae34ea27c5` are ancestors of HEAD.
- Confirm this is diagnostic-only readiness work, not capture execution.
- Confirm no prompt requests socket open, loopback socket, public/LAN socket, datagram send/receive, real client, Steam, connect/post-connect/signon, netchan, qport evidence promotion, or compatibility expansion.

## Required Files

- `fixtures/diagnostic/hlds/qport_session/qport_session_offline_fixture_manifest_policy.json`
- `fixtures/diagnostic/hlds/qport_session/qport_session_capture_preflight_dry_run_manifest.json`
- `fixtures/diagnostic/hlds/qport_session/qport_session_capture_shell_ci_manifest.json`
- `fixtures/diagnostic/hlds/qport_session/qport_session_capture_runtime_skeleton_ci_manifest.json`
- `fixtures/diagnostic/hlds/qport_session/qport_session_capture_execution_skeleton_ci_manifest.json`
- `fixtures/diagnostic/hlds/qport_session/qport_session_capture_execution_implementation_skeleton_ci_manifest.json`
- `fixtures/diagnostic/hlds/qport_session/qport_session_capture_execution_implementation_ci_manifest.json`
- `scripts/run_hlds_qport_session_capture_dry_run.ps1`

## Required Gates

- readiness policy review passed
- execution implementation CI drift gate passed
- execution implementation boundary validated
- final implementation gate passed
- execution skeleton CI drift gate passed
- runtime skeleton CI drift gate passed
- offline fixture validator passed
- capture policy gate passed while denying capture
- dry-run validator passed
- wrapper validation passed

## Expected Happy Fields

- `readiness_gate_passed=1`
- `readiness_plan_created=1`
- `readiness_plan_validated=1`
- `readiness_policy_review_passed=1`
- `execution_implementation_ci_drift_gate_passed=1`
- `execution_implementation_boundary_validated=1`
- `final_gate_passed=1`
- `timeout_cleanup_policy_defined=1`
- `artifact_schema_lock_defined=1`
- `socket_policy_review_required=1`
- `datagram_policy_review_required=1`

## Expected Blocked Fields

- capture, runtime, socket, loopback, public, LAN, datagram, real-client, connect, post-connect, signon, netchan, qport evidence, and compatibility allowed-now fields stay `0`
- capture executed, runtime executed, datagram sent/received, sockets opened, real client invoked, connect/signon/netchan runtime fields stay `0`
- `capture_blocked_by_policy=1`
- `address_scoped_challenge_reusable_as_diagnostic_prerequisite=1`
- `address_scoped_challenge_reusable_as_real_netchan_proof=0`

## Pass And Fail

Pass means every required readiness and implementation gate remains passed, the readiness plan exists and validates, timeout/cleanup and artifact schema lock requirements exist, socket/datagram policy review remains required, and every blocked action remains blocked.

Fail means any required gate is missing, any readiness policy requirement is absent, or any blocked action becomes allowed or executed.

## Stop Conditions

- If `capture_executed=1`, stop immediately and treat it as a boundary regression.
- If `socket_open_attempted=1` or `loopback_udp_socket_opened=1`, stop and require separate socket/loopback policy review.
- If `datagram_sent=1` or `datagram_received=1`, stop and require separate datagram policy review.
- If `timeout_cleanup_policy_defined=0`, stop and restore readiness plan requirements.
- If `artifact_schema_lock_defined=0`, stop and restore schema-lock requirements.
- If `qport_session_byte_evidence_sufficient=1` without a separate evidence prompt, reject the result as evidence overclaim.
- If `compatibility_claim_expansion_allowed_now=1`, reject the result as out of scope.
