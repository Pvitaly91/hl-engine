# Qport/session capture shell boundary checklist

## Before running the shell probe

- Confirm branch `codex/HL-CL-20260401-081-target-runtime-completion-state`.
- Confirm `fixtures/diagnostic/hlds/qport_session/qport_session_offline_fixture_manifest_policy.json` exists.
- Confirm `fixtures/diagnostic/hlds/qport_session/qport_session_capture_preflight_dry_run_manifest.json` exists.
- Confirm `scripts/run_hlds_qport_session_capture_dry_run.ps1` exists.
- Confirm `docs/diagnostic/hlds/qport_session_capture_dry_run_wrapper.md` exists.
- Confirm no request asks to execute capture, open sockets, run runtime networking, invoke Steam, or invoke a real client.

## Required validators

- Offline fixture validator must pass.
- Capture policy gate must pass while denying capture.
- Dry-run validator must pass with safe planned actions.
- Wrapper validation must pass.

## Expected happy fields

- `accepted=1`
- `capture_shell_added=1`
- `shell_plan_created=1`
- `shell_plan_validated=1`
- `offline_fixture_validator_invoked=1`
- `offline_fixture_validator_passed=1`
- `capture_policy_gate_invoked=1`
- `capture_policy_gate_passed=1`
- `dry_run_validator_invoked=1`
- `dry_run_validator_passed=1`
- `wrapper_validation_required=1`
- `wrapper_validation_checked=1`
- `wrapper_validation_passed=1`

## Expected blocked fields

- `capture_allowed_now=0`
- `capture_blocked_by_policy=1`
- `capture_block_reason=capture_implementation_not_allowed_yet`
- `capture_implementation_added=0`
- `capture_execution_requested=0`
- `capture_executed=0`
- `capture_runtime_executed=0`
- `qport_session_byte_evidence_sufficient=0`
- `byte_level_qport_session_evidence_sufficient=0`
- `address_scoped_challenge_reusable_as_real_netchan_proof=0`
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

## Pass

Pass means the shell probe accepts only the happy diagnostic shell path, creates a shell-only plan, validates it, and leaves capture, sockets, runtime paths, real clients, and compatibility expansion blocked.

## Fail

Fail if `capture_allowed_now`, `capture_executed`, `capture_runtime_executed`, `socket_open_attempted`, `real_client_binary_invoked`, `connect_path_invoked`, `post_connect_serverinfo_path_invoked`, `signon_serverinfo_path_invoked`, or `netchan_runtime_started` becomes `1`.

If `qport_session_byte_evidence_sufficient` becomes `1` without a separate byte-evidence prompt, stop and treat it as evidence overclaim.
