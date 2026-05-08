# Qport/session capture preflight dry-run validator report

Prompt: HL-CL-20260504-308-dedicated-goldsrc-hlds-qport-session-capture-preflight-dry-run-validator
Compatibility claim: diagnostic-qport-session-capture-preflight-dry-run-validator-only; no real Steam Half-Life or HLDS-compatible client compatibility claimed

## Scope
Implemented a disabled-by-default read-only validator probe for the qport/session capture preflight dry-run manifest. The probe reads checked-in manifest, policy, fixture, and documentation files; it does not implement capture, execute capture, open sockets, invoke Steam or real clients, run getchallenge/connect/post-connect/signon paths, or start netchan.

## Source changes
- Added launch options for `--hlds-qport-session-capture-preflight-dry-run-validator`, `--hlds-qport-session-capture-preflight-dry-run-validator-probe`, and the scenario option.
- Added `HldsQportSessionCapturePreflightDryRunValidatorSummary` and host summary wiring.
- Added manifest metadata checks, policy reference checks, validator/policy-gate precondition checks, planned artifact schema checks, planned-action safety scanning, and mutation gates for all requested scenarios.

## Proof result
- happy: pass (<none>)
- gate_disabled_by_default: pass (qport_session_dry_run_validator_disabled)
- gate_missing_dry_run_manifest: pass (qport_session_dry_run_manifest_missing)
- gate_missing_policy_reference: pass (qport_session_policy_reference_missing)
- gate_validator_not_required: pass (offline_fixture_validator_required)
- gate_policy_gate_not_required: pass (capture_policy_gate_required)
- gate_capture_allowed_claim: pass (qport_session_capture_not_allowed_now)
- gate_capture_executed_claim: pass (qport_session_capture_execution_not_allowed)
- gate_socket_action_in_plan: pass (socket_action_not_allowed_in_dry_run_plan)
- gate_real_client_action_in_plan: pass (real_client_action_not_allowed_in_dry_run_plan)
- gate_connect_or_signon_action_in_plan: pass (connect_or_signon_action_not_allowed_in_dry_run_plan)
- gate_missing_planned_artifact_schema: pass (missing_planned_capture_artifact_schema_field)
- gate_qport_byte_evidence_claim: pass (qport_byte_evidence_claim_rejected)
- gate_address_scoped_challenge_real_netchan_claim: pass (address_scoped_challenge_not_real_netchan_proof)
- gate_no_real_client_used: pass (<none>)
- gate_public_socket_blocked: pass (public_socket_blocked)

## Boundary preservation
- capture_allowed_now=0 in happy and all gated paths.
- capture_implementation_added=0, capture_executed=0, capture_runtime_executed=0.
- real_client_capture_allowed_now=0, real_steam_client_used=0, real_client_binary_invoked=0.
- socket_open_attempted=0, public_socket_opened=0, lan_socket_opened=0, loopback_udp_socket_opened=0.
- connect_path_invoked=0, post_connect_serverinfo_path_invoked=0, signon_serverinfo_path_invoked=0, netchan_runtime_started=0.
