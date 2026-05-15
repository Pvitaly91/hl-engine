# Qport/session runtime skeleton boundary checklist

## Before running the skeleton probe

- Confirm branch codex/HL-CL-20260401-081-target-runtime-completion-state.
- Confirm ixtures/diagnostic/hlds/qport_session/qport_session_offline_fixture_manifest_policy.json exists.
- Confirm ixtures/diagnostic/hlds/qport_session/qport_session_capture_preflight_dry_run_manifest.json exists.
- Confirm ixtures/diagnostic/hlds/qport_session/qport_session_capture_shell_ci_manifest.json exists.
- Confirm scripts/run_hlds_qport_session_capture_dry_run.ps1 exists.
- Confirm the prompt does not ask to execute capture, open sockets, send or receive datagrams, invoke Steam, invoke a real client, or run runtime network stages.

## Required validators and gates

- final_policy_gate_passed=1
- ci_drift_gate_passed=1
- offline_fixture_validator_passed=1
- capture_policy_gate_passed=1
- dry_run_validator_passed=1
- wrapper_validation_passed=1
- shell_boundary_validated=1

## Expected happy fields

- runtime_skeleton_added=1
- runtime_skeleton_disabled_by_default=1
- skeleton_plan_created=1
- skeleton_plan_validated=1
- capture_allowed_now=0
- capture_blocked_by_policy=1
- capture_block_reason=capture_implementation_not_allowed_yet

## Fields that must remain zero

- capture_implementation_added
- capture_executed
- capture_runtime_executed
- datagram_sent
- datagram_received
- qport_session_byte_evidence_sufficient
- byte_level_qport_session_evidence_sufficient
- address_scoped_challenge_reusable_as_real_netchan_proof
- real_client_capture_allowed_now
- real_steam_client_used
- real_client_binary_invoked
- socket_open_attempted
- public_socket_opened
- lan_socket_opened
- loopback_udp_socket_opened
- connect_path_invoked
- post_connect_serverinfo_path_invoked
- signon_serverinfo_path_invoked
- netchan_runtime_started
- normal_host_behavior_changed

## Pass criteria

The boundary passes only when the skeleton plan is created and validated, all prerequisite gates pass, and every blocked runtime/capture/socket/datagram/client/netchan/evidence/compatibility marker remains blocked.

## Fail criteria and response

- If capture_executed becomes 1, stop and treat the skeleton as an implementation regression.
- If capture_runtime_executed becomes 1, stop and treat the skeleton as an implementation regression.
- If socket_open_attempted becomes 1, stop and remove the socket path before any further prompt.
- If datagram_sent or datagram_received becomes 1, stop and remove datagram behavior before any further prompt.
- If qport_session_byte_evidence_sufficient becomes 1 without a separate evidence prompt, stop and reject the evidence promotion.
- If compatibility_claim_expansion_allowed_now becomes 1, stop and reject the compatibility expansion.
