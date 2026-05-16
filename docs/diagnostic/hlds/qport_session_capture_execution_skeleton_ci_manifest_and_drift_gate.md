# Qport/Session Capture Execution Skeleton CI Manifest and Drift Gate

Prompt ID: HL-CL-20260504-325-dedicated-goldsrc-hlds-qport-session-capture-execution-skeleton-ci-manifest-and-drift-gate

Compatibility claim level: diagnostic-qport-session-execution-skeleton-ci-manifest-drift-gate-only; no real Steam Half-Life or HLDS-compatible client compatibility claimed

## Boundary

This report documents the CI-style manifest and disabled-by-default drift gate for the qport/session no-client capture execution skeleton boundary. It covers prompts 304 through 324, focuses on prompt 323, and depends on the prompt 322 final execution policy gate plus the prompt 319 runtime skeleton CI drift gate.

Stable manifest:

- fixtures/diagnostic/hlds/qport_session/qport_session_capture_execution_skeleton_ci_manifest.json

Required checked-in inputs:

- fixtures/diagnostic/hlds/qport_session
- fixtures/diagnostic/hlds/qport_session/qport_session_offline_fixture_manifest_policy.json
- fixtures/diagnostic/hlds/qport_session/qport_session_capture_preflight_dry_run_manifest.json
- fixtures/diagnostic/hlds/qport_session/qport_session_capture_shell_ci_manifest.json
- fixtures/diagnostic/hlds/qport_session/qport_session_capture_runtime_skeleton_ci_manifest.json
- scripts/run_hlds_qport_session_capture_dry_run.ps1
- docs/diagnostic/hlds/qport_session_capture_execution_skeleton_release_boundary_summary.md
- logs/latest/HL-CL-20260504-323-dedicated-goldsrc-hlds-qport-session-no-client-capture-execution-skeleton-disabled-by-default/codex/qport_session_capture_execution_skeleton_summary.json

Relevant commits:

- Execution skeleton source commit: 8d85c47e73758a0970c7242fa8eecf621f3fd9ce
- Execution skeleton artifact commit: c08bdd9b3091f4186c2115cf46f4449bae55def5
- Final execution policy gate source commit: 700a7380d7570588532c129d625d88177b692632
- Runtime skeleton CI source commit: 5804ca4961ef3a843a2604deb28c95e9439a6112

## Drift Gate

The drift gate is disabled by default and runs only when explicitly requested:

```text
--hlds-qport-session-execution-skeleton-ci-drift-gate-probe
--hlds-qport-session-execution-skeleton-ci-drift-gate-probe-scenario <scenario>
```

It loads the execution skeleton CI manifest, validates required fields, checks required files and recorded hashes where present, invokes the prompt 323 execution skeleton happy path, and rejects drift in policy, dry-run, runtime skeleton CI manifest, wrapper, dependency, or execution skeleton boundary surfaces.

The gate is read-only. It does not implement capture, execute capture, open sockets, open loopback sockets, send datagrams, receive datagrams, invoke real clients, run connect/post-connect/signon paths, start netchan, promote qport/session byte evidence, or expand compatibility claims.

## Required Positive Contract

- execution_skeleton_added=1
- execution_plan_created=1
- execution_plan_validated=1
- final_execution_policy_gate_passed=1
- runtime_skeleton_ci_drift_gate_passed=1
- runtime_skeleton_boundary_validated=1
- offline_fixture_validator_passed=1
- capture_policy_gate_passed=1
- dry_run_validator_passed=1
- wrapper_validation_passed=1

## Required Blocked Contract

- capture_execution_allowed_now=0
- capture_runtime_allowed_now=0
- socket_open_allowed_now=0
- loopback_socket_allowed_now=0
- public_socket_allowed_now=0
- lan_socket_allowed_now=0
- datagram_send_allowed_now=0
- datagram_receive_allowed_now=0
- real_client_allowed_now=0
- connect_path_allowed_now=0
- post_connect_serverinfo_allowed_now=0
- signon_serverinfo_allowed_now=0
- netchan_runtime_allowed_now=0
- qport_evidence_promotion_allowed_now=0
- compatibility_claim_expansion_allowed_now=0
- capture_allowed_now=0
- capture_implementation_added=0
- capture_executed=0
- capture_runtime_executed=0
- datagram_sent=0
- datagram_received=0
- qport_session_byte_evidence_sufficient=0
- byte_level_qport_session_evidence_sufficient=0
- address_scoped_challenge_reusable_as_real_netchan_proof=0
- real_steam_client_used=0
- real_client_binary_invoked=0
- socket_open_attempted=0
- public_socket_opened=0
- lan_socket_opened=0
- loopback_udp_socket_opened=0
- connect_path_invoked=0
- post_connect_serverinfo_path_invoked=0
- signon_serverinfo_path_invoked=0
- netchan_runtime_started=0
- normal_host_behavior_changed=0

## Proof Scenarios

The prompt 325 proof matrix is:

- happy
- gate_disabled_by_default
- gate_missing_ci_manifest
- gate_execution_skeleton_drift
- gate_policy_drift
- gate_dry_run_manifest_drift
- gate_runtime_skeleton_ci_manifest_drift
- gate_wrapper_drift
- gate_dependency_removed
- gate_capture_execution_allowed_now_drift
- gate_capture_runtime_allowed_now_drift
- gate_socket_allowed_drift
- gate_loopback_socket_allowed_drift
- gate_datagram_allowed_drift
- gate_capture_executed_drift
- gate_datagram_executed_drift
- gate_real_client_allowed_drift
- gate_connect_signon_allowed_drift
- gate_netchan_runtime_drift
- gate_qport_evidence_promoted_drift
- gate_address_scoped_challenge_real_netchan_drift
- gate_compatibility_claim_expanded
- gate_no_real_client_used
- gate_public_socket_blocked

## Pass And Fail Conditions

Pass requires the happy scenario to load the manifest, pass the execution skeleton drift gate, preserve all positive fields, preserve all blocked fields, and report zero drift for policy, dry-run manifest, runtime skeleton CI manifest, wrapper, dependency, and execution skeleton surfaces.

Fail includes any capture execution, capture runtime execution, socket open attempt, loopback/public/LAN socket open, datagram send/receive, real client invocation, connect/post-connect/signon path invocation, netchan runtime start, qport/session evidence promotion, address-scoped challenge promotion to real netchan proof, or compatibility claim expansion.

Recommended next prompt:

HL-CL-20260504-326-dedicated-goldsrc-hlds-qport-session-execution-skeleton-wrapper-ci-release-summary

Recommended next task:

Summarize the qport/session execution skeleton wrapper and CI drift gate boundary after manifest drift checks pass.
