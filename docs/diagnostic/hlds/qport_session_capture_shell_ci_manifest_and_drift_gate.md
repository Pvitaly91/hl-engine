# Qport/session Capture Shell CI Manifest And Drift Gate

Prompt: `HL-CL-20260504-314-dedicated-goldsrc-hlds-qport-session-capture-shell-ci-manifest-and-drift-gate`

Compatibility claim level:
`diagnostic-qport-session-capture-shell-ci-manifest-drift-gate-only; no real Steam Half-Life or HLDS-compatible client compatibility claimed`

## Scope

This document defines the CI-style manifest and disabled-by-default drift gate for the qport/session capture shell boundary closed in prompts 304 through 313.

The gate is read-only. It validates checked-in files, manifest hash records, wrapper blockers, and shell boundary summary fields. It does not implement capture, execute capture, open sockets, run getchallenge/connect/post-connect/signon paths, start netchan, invoke Steam, invoke real clients, or expand compatibility claims.

## Stable Inputs

- Fixture root: `fixtures/diagnostic/hlds/qport_session`
- CI manifest: `fixtures/diagnostic/hlds/qport_session/qport_session_capture_shell_ci_manifest.json`
- Offline fixture policy: `fixtures/diagnostic/hlds/qport_session/qport_session_offline_fixture_manifest_policy.json`
- Dry-run manifest: `fixtures/diagnostic/hlds/qport_session/qport_session_capture_preflight_dry_run_manifest.json`
- Dry-run wrapper: `scripts/run_hlds_qport_session_capture_dry_run.ps1`
- Wrapper docs: `docs/diagnostic/hlds/qport_session_capture_dry_run_wrapper.md`
- Shell release boundary docs: `docs/diagnostic/hlds/qport_session_capture_shell_release_boundary_summary.md`

## Drift Gate Command

The gate is disabled by default and must be explicitly requested:

```powershell
.\build\install\bin\hl-engine.exe `
  --dedicated `
  --prompt-id HL-CL-20260504-314-dedicated-goldsrc-hlds-qport-session-capture-shell-ci-manifest-and-drift-gate `
  --hlds-qport-session-capture-shell-ci-drift-gate-probe `
  --hlds-qport-session-capture-shell-ci-drift-gate-probe-scenario happy
```

## Pass Markers

- `ci_manifest_loaded=1`
- `drift_gate_enabled=1`
- `drift_gate_disabled_by_default=1`
- `drift_gate_passed=1`
- `fixture_files_checked>=5`
- `fixture_drift_detected=0`
- `policy_drift_detected=0`
- `dry_run_manifest_drift_detected=0`
- `wrapper_drift_detected=0`
- `shell_dependency_drift_detected=0`

## Required Positive Boundary Fields

- `capture_shell_added=1`
- `shell_plan_created=1`
- `shell_plan_validated=1`
- `offline_fixture_validator_required=1`
- `capture_policy_gate_required=1`
- `dry_run_validator_required=1`
- `wrapper_validation_required=1`

## Required Blocked Fields

- `capture_allowed_now=0`
- `capture_implementation_added=0`
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

## Failure Handling

Any drift marker, capture permission, socket/runtime marker, real-client marker, qport evidence promotion, address-scoped challenge promotion, or compatibility claim expansion fails the gate. Treat the failure as a policy review requirement before any future qport/session capture-shell expansion.
