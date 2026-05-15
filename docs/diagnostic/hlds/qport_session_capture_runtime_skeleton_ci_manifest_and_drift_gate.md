# Qport/Session Capture Runtime Skeleton CI Manifest And Drift Gate

Prompt `HL-CL-20260504-319-dedicated-goldsrc-hlds-qport-session-capture-runtime-skeleton-ci-manifest-and-drift-gate` adds a machine-readable CI manifest and disabled-by-default drift gate for the qport/session no-client capture runtime skeleton boundary.

Compatibility claim level:

`diagnostic-qport-session-runtime-skeleton-ci-manifest-drift-gate-only; no real Steam Half-Life or HLDS-compatible client compatibility claimed`

## Boundary

- Covered prompts: 304 through 318.
- Focused runtime skeleton prompt: 317.
- Runtime skeleton source commit: `5c48e0070132c0415125cef656795aabe69dde03`.
- Runtime skeleton artifact commit: `9958e7b8bf099b788eb7ad301757521393b820eb`.
- Final policy gate source commit: `624052e71f3901883f837e4220db120e9b5433a0`.
- Shell CI manifest source commit: `bf42fc0e59d67ab00aae3286eb1cef3aded6a5d3`.
- Runtime skeleton CI manifest: `fixtures/diagnostic/hlds/qport_session/qport_session_capture_runtime_skeleton_ci_manifest.json`.
- Shell CI manifest: `fixtures/diagnostic/hlds/qport_session/qport_session_capture_shell_ci_manifest.json`.
- Fixture root: `fixtures/diagnostic/hlds/qport_session`.
- Policy file: `fixtures/diagnostic/hlds/qport_session/qport_session_offline_fixture_manifest_policy.json`.
- Dry-run manifest: `fixtures/diagnostic/hlds/qport_session/qport_session_capture_preflight_dry_run_manifest.json`.
- Wrapper script: `scripts/run_hlds_qport_session_capture_dry_run.ps1`.
- Runtime skeleton release boundary doc: `docs/diagnostic/hlds/qport_session_capture_runtime_skeleton_release_boundary_summary.md`.

## Drift Gate

The runtime skeleton CI drift gate is read-only and disabled by default. It is only active through explicit diagnostic probe options:

```text
--hlds-qport-session-runtime-skeleton-ci-drift-gate
--hlds-qport-session-runtime-skeleton-ci-drift-gate-probe
--hlds-qport-session-runtime-skeleton-ci-drift-gate-probe-scenario <scenario>
```

The gate loads the runtime skeleton CI manifest, validates required field markers, checks fixture/policy/dry-run/wrapper hashes where existing local gates already record them, confirms the shell CI manifest and runtime skeleton release files are present, then reuses the prompt 317 runtime skeleton happy path. It stops before capture execution, socket opening, datagram send or receive, real client use, connect/post-connect/signon paths, netchan runtime, qport byte evidence promotion, and compatibility expansion.

## Positive Contract

- `runtime_skeleton_added=1`
- `skeleton_plan_created=1`
- `skeleton_plan_validated=1`
- `final_policy_gate_passed=1`
- `ci_drift_gate_passed=1`
- `offline_fixture_validator_passed=1`
- `capture_policy_gate_passed=1`
- `dry_run_validator_passed=1`
- `wrapper_validation_passed=1`
- `shell_boundary_validated=1`

## Blocked Contract

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

## Proof Scenarios

The drift gate defines these scenarios:

`happy`, `gate_disabled_by_default`, `gate_missing_ci_manifest`, `gate_runtime_skeleton_drift`, `gate_policy_drift`, `gate_dry_run_manifest_drift`, `gate_shell_ci_manifest_drift`, `gate_wrapper_drift`, `gate_dependency_removed`, `gate_capture_allowed_now_drift`, `gate_capture_executed_drift`, `gate_capture_runtime_executed_drift`, `gate_datagram_drift`, `gate_socket_allowed_drift`, `gate_real_client_allowed_drift`, `gate_connect_signon_allowed_drift`, `gate_netchan_runtime_drift`, `gate_qport_evidence_promoted_drift`, `gate_address_scoped_challenge_real_netchan_drift`, `gate_compatibility_claim_expanded`, `gate_no_real_client_used`, and `gate_public_socket_blocked`.

## Operator Checks

- Inspect `fixtures/diagnostic/hlds/qport_session/qport_session_capture_runtime_skeleton_ci_manifest.json` before changing fixture, policy, dry-run, shell CI, wrapper, runtime skeleton, or release-boundary files.
- Run the happy drift gate proof before discussing future capture execution prompts.
- Treat any blocked field changing from `0` to `1` as a boundary failure unless a later explicit prompt updates the policy and evidence chain.
- Treat qport/session byte evidence promotion, address-scoped challenge promotion, socket opening, datagram activity, real client use, and compatibility claim expansion as out of scope for this boundary.

Recommended next prompt:

`HL-CL-20260504-320-dedicated-goldsrc-hlds-qport-session-runtime-skeleton-wrapper-ci-release-summary`
