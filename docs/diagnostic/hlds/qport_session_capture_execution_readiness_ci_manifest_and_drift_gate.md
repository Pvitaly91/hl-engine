# Qport/Session Capture Execution Readiness CI Manifest And Drift Gate

Prompt: HL-CL-20260504-341-dedicated-goldsrc-hlds-qport-session-capture-execution-readiness-ci-manifest-and-drift-gate

Compatibility claim level: diagnostic-qport-session-capture-execution-readiness-ci-manifest-drift-gate-only; no real Steam Half-Life or HLDS-compatible client compatibility claimed.

This document records the read-only CI manifest and disabled-by-default drift gate for the qport/session capture execution readiness boundary. The boundary covers prompts 304 through 340 and focuses on the disabled-by-default readiness gate and readiness plan added by prompt 339, documented by prompt 340, and allowed by the prompt 338 readiness policy review.

## Stable Files

- Readiness CI manifest: `fixtures/diagnostic/hlds/qport_session/qport_session_capture_execution_readiness_ci_manifest.json`
- Readiness release boundary summary: `docs/diagnostic/hlds/qport_session_capture_execution_readiness_release_boundary_summary.md`
- Readiness policy review: `docs/diagnostic/hlds/qport_session_capture_execution_readiness_policy_review.md`
- Execution implementation CI manifest: `fixtures/diagnostic/hlds/qport_session/qport_session_capture_execution_implementation_ci_manifest.json`
- Execution implementation skeleton CI manifest: `fixtures/diagnostic/hlds/qport_session/qport_session_capture_execution_implementation_skeleton_ci_manifest.json`
- Execution skeleton CI manifest: `fixtures/diagnostic/hlds/qport_session/qport_session_capture_execution_skeleton_ci_manifest.json`
- Runtime skeleton CI manifest: `fixtures/diagnostic/hlds/qport_session/qport_session_capture_runtime_skeleton_ci_manifest.json`
- Shell CI manifest: `fixtures/diagnostic/hlds/qport_session/qport_session_capture_shell_ci_manifest.json`
- Offline fixture policy: `fixtures/diagnostic/hlds/qport_session/qport_session_offline_fixture_manifest_policy.json`
- Dry-run manifest: `fixtures/diagnostic/hlds/qport_session/qport_session_capture_preflight_dry_run_manifest.json`
- Wrapper script: `scripts/run_hlds_qport_session_capture_dry_run.ps1`

## Positive Contract

- The readiness gate remains disabled by default.
- The readiness CI manifest exists and is loaded by an explicit diagnostic probe.
- The readiness plan remains present and validated.
- Prompt 338 readiness policy review remains required and passed.
- Prompt 336 execution implementation CI drift gate remains required and passed.
- The execution implementation boundary, final implementation gate, execution skeleton CI gate, runtime skeleton CI gate, offline fixture validator, capture policy gate, dry-run validator, and wrapper validation remain required and passed.
- Timeout/cleanup policy and artifact schema lock requirements remain defined as plan data only.
- Socket policy review and datagram policy review remain required before any future socket or datagram work.

## Blocked Contract

The readiness CI drift gate does not execute capture, run capture runtime, implement packet capture, open sockets, open loopback sockets, open public sockets, open LAN sockets, send datagrams, receive datagrams, invoke Steam, invoke real client binaries, run getchallenge/connect/post-connect/signon runtime paths, start netchan, promote qport/session byte evidence, or expand compatibility claims.

Required blocked markers remain zero:

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

## Drift Gate Probe

Launch options:

- `--hlds-qport-session-capture-execution-readiness-ci-drift-gate`
- `--hlds-qport-session-capture-execution-readiness-ci-drift-gate-probe`
- `--hlds-qport-session-capture-execution-readiness-ci-drift-gate-probe-scenario <scenario>`

The happy scenario reads the readiness CI manifest and validates manifest markers, relevant file presence, recorded hash markers, readiness dependencies, positive fields, blocked fields, and safety invariants. Mutation scenarios deliberately flip one prerequisite or blocked field in the summary and must be rejected before any runtime network action.

## Pass And Fail Rules

Pass means the happy probe is accepted, `readiness_drift_gate_passed=1`, all drift markers remain zero, all positive readiness fields remain one, and all blocked runtime/capture/socket/datagram/client/evidence/compatibility fields remain zero.

Fail means any required manifest, policy, release doc, fixture, wrapper, prerequisite gate, timeout/cleanup policy, artifact schema lock, socket policy review, datagram policy review, or blocked marker drifts. A failure must stop at diagnostic summary output; it must not be treated as permission to run capture or network code.
