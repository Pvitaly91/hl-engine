# Qport/Session Capture Dry-Run Release Gate Checklist

Compatibility claim level: diagnostic-qport-session-capture-dry-run-release-boundary-only; no real Steam Half-Life or HLDS-compatible client compatibility claimed

## Before Running

- Confirm branch: `codex/HL-CL-20260401-081-target-runtime-completion-state`.
- Confirm policy file: `fixtures/diagnostic/hlds/qport_session/qport_session_offline_fixture_manifest_policy.json`.
- Confirm dry-run manifest: `fixtures/diagnostic/hlds/qport_session/qport_session_capture_preflight_dry_run_manifest.json`.
- Confirm wrapper script: `scripts/run_hlds_qport_session_capture_dry_run.ps1`.
- Confirm wrapper docs: `docs/diagnostic/hlds/qport_session_capture_dry_run_wrapper.md`.
- Confirm the requested action is dry-run validation only.

## Commands

Plan mode:

```powershell
.\scripts\run_hlds_qport_session_capture_dry_run.ps1 -Mode plan -OutDir logs\latest\HL-CL-20260504-310-dedicated-goldsrc-hlds-qport-session-capture-dry-run-release-boundary-summary\wrapper_plan -RunLabelPrefix p310-plan -Strict
```

Validate mode:

```powershell
.\scripts\run_hlds_qport_session_capture_dry_run.ps1 -Mode validate -NoBuild -OutDir logs\latest\HL-CL-20260504-310-dedicated-goldsrc-hlds-qport-session-capture-dry-run-release-boundary-summary\wrapper_validate -RunLabelPrefix p310-validate -Strict
```

## Wrapper Summary Fields

Pass requires:

- `accepted=1`
- `wrapper_plan_passed=1`
- `wrapper_validate_passed=1` for validate mode
- `offline_fixture_validator_included=1`
- `capture_policy_gate_included=1`
- `dry_run_validator_included=1`
- `capture_allowed_now=0`
- `capture_blocked_by_policy=1`
- `capture_block_reason=capture_implementation_not_allowed_yet`
- `capture_implementation_added=0`
- `capture_executed=0`
- `capture_runtime_executed=0`
- `socket_open_attempted=0`
- `public_socket_opened=0`
- `lan_socket_opened=0`
- `loopback_udp_socket_opened=0`
- `real_client_binary_invoked=0`
- `connect_path_invoked=0`
- `post_connect_serverinfo_path_invoked=0`
- `signon_serverinfo_path_invoked=0`
- `netchan_runtime_started=0`

## Policy Gate Fields

Pass requires:

- `policy_gate_passed=1`
- `offline_fixture_validator_passed=1`
- `capture_allowed_now=0`
- `capture_blocked_by_policy=1`
- `capture_block_reason=capture_implementation_not_allowed_yet`

## Dry-Run Validator Fields

Pass requires:

- `dry_run_manifest_validated=1`
- `planned_artifact_schema_validated=1`
- `planned_actions_safe=1`
- `dry_run_only=1`
- `qport_session_byte_evidence_sufficient=0`
- `byte_level_qport_session_evidence_sufficient=0`
- `address_scoped_challenge_reusable_as_real_netchan_proof=0`

## Fail Conditions

- `capture_allowed_now=1`.
- Any socket, public, LAN, loopback, Steam, real-client, connect, post-connect, signon, netchan, auth, admission, or capture action appears in the command plan.
- Any wrapper unsafe option is accepted.
- Policy or fixture JSON fails to parse.
- Offline fixture validator, policy gate, or dry-run validator is omitted.
- Address-scoped challenge is promoted to real netchan proof.
- Qport/session byte evidence is promoted without local byte evidence.

## Response To Failure

Stop the boundary run, preserve artifacts, and harden the smallest failing layer first: fixture policy, dry-run manifest, validator, policy gate, or wrapper. Do not continue into capture implementation from this boundary.
