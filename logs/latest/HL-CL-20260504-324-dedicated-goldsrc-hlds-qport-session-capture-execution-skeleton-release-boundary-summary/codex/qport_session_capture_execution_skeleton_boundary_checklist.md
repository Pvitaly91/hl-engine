# Qport/Session Capture Execution Skeleton Boundary Checklist

## Before Running The Probe

- Confirm the prompt is diagnostic-only and report/boundary scoped.
- Confirm HEAD descends from prompt 323 source and artifact commits.
- Confirm `--frames 1`, summary logging, and an explicit execution skeleton probe are used for any bounded rerun.
- Confirm no request includes capture execution, sockets, loopback sockets, datagrams, runtime network paths, Steam, real clients, connect/post-connect/signon, or netchan.

## Required Files

- `fixtures/diagnostic/hlds/qport_session/qport_session_offline_fixture_manifest_policy.json`
- `fixtures/diagnostic/hlds/qport_session/qport_session_capture_preflight_dry_run_manifest.json`
- `fixtures/diagnostic/hlds/qport_session/qport_session_capture_shell_ci_manifest.json`
- `fixtures/diagnostic/hlds/qport_session/qport_session_capture_runtime_skeleton_ci_manifest.json`
- `scripts/run_hlds_qport_session_capture_dry_run.ps1`
- `docs/diagnostic/hlds/qport_session_capture_execution_skeleton_release_boundary_summary.md`

## Expected Happy Fields

- `execution_skeleton_added=1`
- `execution_plan_created=1`
- `execution_plan_validated=1`
- `final_execution_policy_gate_passed=1`
- `runtime_skeleton_ci_drift_gate_passed=1`
- `runtime_skeleton_boundary_validated=1`
- `offline_fixture_validator_passed=1`
- `capture_policy_gate_passed=1`
- `dry_run_validator_passed=1`
- `wrapper_validation_passed=1`

## Expected Blocked Fields

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
- `capture_executed=0`
- `capture_runtime_executed=0`
- `datagram_sent=0`
- `datagram_received=0`
- `socket_open_attempted=0`
- `real_client_binary_invoked=0`
- `netchan_runtime_started=0`

## Pass And Fail Criteria

Pass: all required gates pass, the execution-only plan is created and validated, and every forbidden action marker remains blocked or zero.

Fail: any required gate is missing, any unsafe request is accepted, or any blocked marker changes to an executing/opened/sent/received state.

## Required Response To Unsafe Markers

- If `capture_executed=1`, stop and treat it as a release blocker.
- If `socket_open_attempted=1`, stop and require socket policy hardening.
- If `datagram_sent=1` or `datagram_received=1`, stop and require datagram policy hardening.
- If `qport_session_byte_evidence_sufficient=1` appears without a separate evidence prompt, revert the claim and require a byte-evidence prompt.
- If `compatibility_claim_expansion_allowed_now=1`, revert the claim and restore diagnostic-only compatibility.
