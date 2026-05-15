# qport/session Runtime Skeleton CI Manifest Report

Prompt: HL-CL-20260504-319-dedicated-goldsrc-hlds-qport-session-capture-runtime-skeleton-ci-manifest-and-drift-gate

Compatibility claim level: diagnostic-qport-session-runtime-skeleton-ci-manifest-drift-gate-only; no real Steam Half-Life or HLDS-compatible client compatibility claimed

Source commit: 5804ca4961ef3a843a2604deb28c95e9439a6112

## Stable Manifest

Manifest path: fixtures/diagnostic/hlds/qport_session/qport_session_capture_runtime_skeleton_ci_manifest.json
Covered prompts: 304 through 318
Focused runtime skeleton prompt: 317
Runtime skeleton source commit: 5c48e0070132c0415125cef656795aabe69dde03
Runtime skeleton artifact commit: 9958e7b8bf099b788eb7ad301757521393b820eb
Final policy gate source commit: 624052e71f3901883f837e4220db120e9b5433a0
Shell CI manifest source commit: bf42fc0e59d67ab00aae3286eb1cef3aded6a5d3

## Files Guarded

Fixture root: fixtures/diagnostic/hlds/qport_session
Policy file: fixtures/diagnostic/hlds/qport_session/qport_session_offline_fixture_manifest_policy.json
Dry-run manifest: fixtures/diagnostic/hlds/qport_session/qport_session_capture_preflight_dry_run_manifest.json
Shell CI manifest: fixtures/diagnostic/hlds/qport_session/qport_session_capture_shell_ci_manifest.json
Wrapper script: scripts/run_hlds_qport_session_capture_dry_run.ps1
Runtime skeleton release doc: docs/diagnostic/hlds/qport_session_capture_runtime_skeleton_release_boundary_summary.md

## Happy Manifest Checks

runtime_skeleton_ci_manifest_loaded: 1
fixture_files_checked: 5
fixture_hashes_recorded: 1
policy_hash_recorded: 1
dry_run_manifest_hash_recorded: 1
shell_ci_manifest_hash_recorded: 1
wrapper_hash_recorded: 1

## Scope

The manifest is diagnostic-only. It records and guards the runtime skeleton boundary without enabling capture execution, socket opening, datagram send/receive, real client invocation, connect/post-connect/signon paths, netchan runtime, qport byte evidence promotion, or compatibility claim expansion.
