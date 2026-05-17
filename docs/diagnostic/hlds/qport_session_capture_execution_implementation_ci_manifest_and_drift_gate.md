# Qport/Session Capture Execution Implementation CI Manifest And Drift Gate

Prompt: `HL-CL-20260504-336-dedicated-goldsrc-hlds-qport-session-capture-execution-implementation-ci-manifest-and-drift-gate`

## Boundary

This document records the CI manifest and disabled-by-default drift gate for the qport/session capture execution implementation boundary introduced by prompt 334 and summarized by prompt 335.

Compatibility claim:

`diagnostic-qport-session-capture-execution-implementation-ci-manifest-drift-gate-only; no real Steam Half-Life or HLDS-compatible client compatibility claimed`

Covered prompt range: 304 through 335.

Focused implementation prompt: 334.

Execution implementation source commit: `2a1bcaed03bffa02d068b33a6f6e18f333e98714`.

Execution implementation artifact commit: `7b65f1e5d767bf8102275e5237ef5ab1dd229f2b`.

Final gate source commit: `580c7b760a3e5f4088125c65c51f59db1b15b472`.

Implementation boundary source commit: `3d6b5ebb8b144a230a0d631c485cf219cbbbec4e`.

## Stable Inputs

- Fixture root: `fixtures/diagnostic/hlds/qport_session`
- Policy file: `fixtures/diagnostic/hlds/qport_session/qport_session_offline_fixture_manifest_policy.json`
- Dry-run manifest: `fixtures/diagnostic/hlds/qport_session/qport_session_capture_preflight_dry_run_manifest.json`
- Shell CI manifest: `fixtures/diagnostic/hlds/qport_session/qport_session_capture_shell_ci_manifest.json`
- Runtime skeleton CI manifest: `fixtures/diagnostic/hlds/qport_session/qport_session_capture_runtime_skeleton_ci_manifest.json`
- Execution skeleton CI manifest: `fixtures/diagnostic/hlds/qport_session/qport_session_capture_execution_skeleton_ci_manifest.json`
- Execution implementation skeleton CI manifest: `fixtures/diagnostic/hlds/qport_session/qport_session_capture_execution_implementation_skeleton_ci_manifest.json`
- Execution implementation CI manifest: `fixtures/diagnostic/hlds/qport_session/qport_session_capture_execution_implementation_ci_manifest.json`
- Wrapper script: `scripts/run_hlds_qport_session_capture_dry_run.ps1`
- Release boundary doc: `docs/diagnostic/hlds/qport_session_capture_execution_implementation_release_boundary_summary.md`

## Proven Positive Contract

The manifest requires these positive fields to remain `1`:

- `execution_implementation_surface_added`
- `implementation_plan_created`
- `implementation_plan_validated`
- `final_gate_passed`
- `implementation_skeleton_ci_drift_gate_passed`
- `policy_review_passed`
- `execution_skeleton_ci_drift_gate_passed`
- `runtime_skeleton_ci_drift_gate_passed`
- `offline_fixture_validator_passed`
- `capture_policy_gate_passed`
- `dry_run_validator_passed`
- `wrapper_validation_passed`

The drift gate validates these fields from checked-in manifests, policy/review docs, prior prompt artifacts, and the prompt 334 implementation probe. The gate is diagnostic-only and disabled by default.

## Required Blocked Contract

The manifest requires these fields to remain `0`:

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

## Drift Gate Behavior

The disabled-by-default probe loads `qport_session_capture_execution_implementation_ci_manifest.json`, checks required fields, confirms the stable input files are present, records whether file hashes are available, reuses the prompt 334 implementation surface in read-only happy mode, and rejects drift scenarios without executing network behavior.

The gate rejects:

- missing implementation CI manifest
- execution implementation drift
- final gate drift
- implementation skeleton CI manifest drift
- policy review drift
- execution skeleton CI manifest drift
- runtime skeleton CI manifest drift
- wrapper drift
- dependency removal
- any blocked field becoming allowed or executed
- qport/session byte evidence promotion
- address-scoped challenge overclaim
- compatibility claim expansion

## Explicit Non-Scope

This CI manifest and drift gate do not execute capture, run capture runtime, open sockets, open loopback sockets, open public or LAN sockets, send datagrams, receive datagrams, run getchallenge/connect/post-connect/signon paths, start netchan, invoke Steam, invoke real client binaries, collect byte evidence, promote qport/session evidence, or expand compatibility claims.

Unknown byte-level qport/session behavior remains unknown.

## Probe Options

- `--hlds-qport-session-capture-execution-implementation-ci-drift-gate`
- `--hlds-qport-session-capture-execution-implementation-ci-drift-gate-probe`
- `--hlds-qport-session-capture-execution-implementation-ci-drift-gate-probe-scenario <scenario>`

Supported scenarios are listed in the stable manifest. The `happy` scenario must pass with `execution_implementation_drift_gate_passed=1`; all drift and unsafe-action scenarios must reject while preserving the no-capture, no-socket, no-datagram, no-real-client, no-runtime boundary.

## Next Prompt

Recommended next prompt:

`HL-CL-20260504-337-dedicated-goldsrc-hlds-qport-session-execution-implementation-wrapper-ci-release-summary`

Recommended task: summarize the qport/session execution implementation wrapper and CI drift gate boundary after manifest drift checks pass.
