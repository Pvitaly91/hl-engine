# Qport/Session Execution Implementation Wrapper CI Operator Checklist

Compatibility claim level: diagnostic-qport-session-execution-implementation-wrapper-ci-release-summary-only; no real Steam Half-Life or HLDS-compatible client compatibility claimed

## Inspect Manifests

Inspect the execution implementation CI manifest:

```powershell
Get-Content -Raw fixtures\diagnostic\hlds\qport_session\qport_session_capture_execution_implementation_ci_manifest.json | ConvertFrom-Json
```

Inspect the implementation skeleton CI manifest:

```powershell
Get-Content -Raw fixtures\diagnostic\hlds\qport_session\qport_session_capture_execution_implementation_skeleton_ci_manifest.json | ConvertFrom-Json
```

Inspect the execution skeleton CI manifest:

```powershell
Get-Content -Raw fixtures\diagnostic\hlds\qport_session\qport_session_capture_execution_skeleton_ci_manifest.json | ConvertFrom-Json
```

Inspect the runtime skeleton CI manifest:

```powershell
Get-Content -Raw fixtures\diagnostic\hlds\qport_session\qport_session_capture_runtime_skeleton_ci_manifest.json | ConvertFrom-Json
```

## Bounded Diagnostic Reruns

Run the execution implementation drift gate only in explicit probe mode with `--hlds-qport-session-capture-execution-implementation-ci-drift-gate-probe` and scenario `happy`.

Run wrapper plan/validate only through the existing dry-run wrapper diagnostic modes. Wrapper plan and validate do not permit capture, sockets, datagrams, real clients, runtime network paths, qport/session byte evidence promotion, or compatibility claim expansion.

## Fields That Must Remain 1

- `capture_blocked_by_policy`
- `address_scoped_challenge_reusable_as_diagnostic_prerequisite`
- `execution_implementation_ci_manifest_created`
- `execution_implementation_drift_gate_passed`
- `execution_implementation_skeleton_ci_manifest_created`
- `execution_skeleton_ci_manifest_created`
- `runtime_skeleton_ci_manifest_created`
- `wrapper_plan_passed`
- `wrapper_validate_passed`
- `final_gate_passed`
- `execution_implementation_surface_added`
- `implementation_plan_created`
- `implementation_plan_validated`
- `future_capture_execution_implementation_prompt_allowed_next`

## Fields That Must Remain 0

- `execution_implementation_drift_detected`
- `implementation_skeleton_drift_detected`
- `policy_drift_detected`
- `dry_run_manifest_drift_detected`
- `execution_implementation_skeleton_ci_manifest_drift_detected`
- `execution_skeleton_ci_manifest_drift_detected`
- `runtime_skeleton_ci_manifest_drift_detected`
- `wrapper_drift_detected`
- `dependency_drift_detected`
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
- `compatibility_claim_expanded`
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

## Drift Response

If any blocked field changes from `0` to `1`, stop and treat the result as a boundary regression. Do not proceed to capture execution, capture runtime, socket work, datagram work, real-client work, connect/post-connect/signon/netchan work, qport evidence promotion, or compatibility claim expansion until a separate hardening prompt resolves the drift.

