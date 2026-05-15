# HL-CL-20260504-318 Qport/Session Capture Runtime Skeleton Release Boundary

Compatibility claim level: diagnostic-qport-session-capture-runtime-skeleton-release-boundary-only; no real Steam Half-Life or HLDS-compatible client compatibility claimed

This release boundary covers the diagnostic qport/session stack from prompts 304 through 317, with the focused runtime-skeleton implementation in prompt 317 and the final policy gate in prompt 316. It records that a disabled-by-default no-client capture runtime skeleton exists, but the skeleton remains a diagnostic plan-only boundary. It does not execute capture, open sockets, send or receive datagrams, run runtime network paths, start netchan, invoke Steam, invoke a real client, promote qport/session byte evidence, or expand compatibility claims.

## Stable Boundary

| Item | Path or value |
| --- | --- |
| Branch | `codex/HL-CL-20260401-081-target-runtime-completion-state` |
| Covered prompt range | `HL-CL-20260504-304` through `HL-CL-20260504-317` |
| Focused runtime-skeleton prompt | `HL-CL-20260504-317` |
| Related final policy prompt | `HL-CL-20260504-316` |
| Fixture root | `fixtures/diagnostic/hlds/qport_session` |
| Offline fixture policy | `fixtures/diagnostic/hlds/qport_session/qport_session_offline_fixture_manifest_policy.json` |
| Offline fixtures | `fixtures/diagnostic/hlds/qport_session/fixtures/*.json` |
| Dry-run manifest | `fixtures/diagnostic/hlds/qport_session/qport_session_capture_preflight_dry_run_manifest.json` |
| CI manifest | `fixtures/diagnostic/hlds/qport_session/qport_session_capture_shell_ci_manifest.json` |
| Dry-run wrapper | `scripts/run_hlds_qport_session_capture_dry_run.ps1` |
| Runtime skeleton source area | `include/app/launch_options.h`, `src/app/launch_options.cpp`, `src/app/host_application.cpp`, `include/game_api/hl_server_module.h`, `src/game_api/hl_server_module.cpp` |
| Runtime skeleton source commit | `5c48e0070132c0415125cef656795aabe69dde03` |

Relevant source commits:

| Prompt | Source commit |
| --- | --- |
| 304 offline fixture manifest policy | `71e89a6ea848767c94119773ef36be56cc86239a` |
| 305 offline fixture validator | `70c4a99b2eace8c549f8379e4f60af4af034d626` |
| 306 capture policy gate | `7ce0541babd4a2f3a7e7e41e0e2352f6f4022c3d` |
| 307 preflight dry-run plan | `7db1ba1e0b1235d50933bb99d50f7a18d909e6f8` |
| 308 dry-run validator | `1bc41c55242f71447e707f182e652e8bf2f7f3a9` |
| 309 dry-run wrapper | `9bebafe858211606689e326a90637c4455875400` |
| 310 dry-run release boundary | `3392efdfde566494636cef6a398a49851f1e42a9` |
| 311 implementation policy review | `707aa1c47555691c6231fccbdb7b4289cf2fcbfe` |
| 312 capture shell | `89131aa1b019bd18705ebf5cb32bff4e079a325d` |
| 313 shell release boundary | `9812166cf4dc8877569db0dadfbab0a46782432a` |
| 314 shell CI manifest and drift gate | `bf42fc0e59d67ab00aae3286eb1cef3aded6a5d3` |
| 315 shell wrapper and CI release summary | `d6aaa61f38205ae377046fe165dfaf39116a4d5b` |
| 316 final policy gate | `624052e71f3901883f837e4220db120e9b5433a0` |
| 317 runtime skeleton | `5c48e0070132c0415125cef656795aabe69dde03` |

Relevant artifact commits:

| Prompt | Artifact commit |
| --- | --- |
| 304 offline fixture manifest policy | `c830498ceba33c4bd38342edf91217bfb304722d` |
| 305 offline fixture validator | `1333be64e43babf251d22825986fdc4304b98cdf` |
| 306 capture policy gate | `eca41a4b2f5a48666fe6fd9bc02d1480ec970a16` |
| 307 preflight dry-run plan | `53ccbdf3d12a9fca0ef1a6b31c780367369578a6` |
| 308 dry-run validator | `dc885cbc0c7e222adb00570557b01708da1e1eaa` |
| 309 dry-run wrapper | `6dd0ced69d0018fd548f41e3da5fe56e4586dc77` |
| 310 dry-run release boundary | `ad3ff5383f6f456e877ca43f1e4d2d3299662822` |
| 311 implementation policy review | `534c14b4aa01e2360bf0c0e808df2da67d87e834` |
| 312 capture shell | `f2018ebf61bb7d271580ef3709428fdbb7acc398` |
| 313 shell release boundary | `a3cb74aa0205366c1f8abcb8f0c842956e56c8cf` |
| 314 shell CI manifest and drift gate | `0a6d8aeac8a3b99d8bcae84d268994324828f7f3` |
| 315 shell wrapper and CI release summary | `2102da76495a4c2135d2340449f44fb15756cfe9` |
| 316 final policy gate | `25e60e9bbef65034fee5a9de165b248762784c52` |
| 317 runtime skeleton | `9958e7b8bf099b788eb7ad301757521393b820eb` |

## Prompt 317 Proof Summary

Prompt 317 added and proved the disabled-by-default runtime skeleton probe. Its full proof matrix passed:

| Scenario | Result |
| --- | --- |
| `happy` | pass |
| `gate_disabled_by_default` | pass |
| `gate_final_policy_gate_required` | pass |
| `gate_ci_drift_gate_required` | pass |
| `gate_offline_validator_required` | pass |
| `gate_capture_policy_gate_required` | pass |
| `gate_dry_run_validator_required` | pass |
| `gate_shell_boundary_required` | pass |
| `gate_wrapper_validation_required` | pass |
| `gate_capture_execution_blocked` | pass |
| `gate_socket_open_blocked` | pass |
| `gate_datagram_send_blocked` | pass |
| `gate_datagram_receive_blocked` | pass |
| `gate_real_client_blocked` | pass |
| `gate_public_lan_blocked` | pass |
| `gate_connect_postconnect_signon_blocked` | pass |
| `gate_netchan_runtime_blocked` | pass |
| `gate_qport_evidence_promotion_blocked` | pass |
| `gate_compatibility_claim_expansion_blocked` | pass |
| `gate_no_real_client_used` | pass |
| `gate_public_socket_blocked` | pass |

Expected happy markers:

- `runtime_skeleton_added=1`
- `runtime_skeleton_disabled_by_default=1`
- `skeleton_plan_created=1`
- `skeleton_plan_validated=1`
- `final_policy_gate_passed=1`
- `ci_drift_gate_passed=1`
- `offline_fixture_validator_passed=1`
- `capture_policy_gate_passed=1`
- `dry_run_validator_passed=1`
- `wrapper_validation_passed=1`
- `shell_boundary_validated=1`

Expected blocked markers:

- `capture_allowed_now=0`
- `capture_blocked_by_policy=1`
- `capture_block_reason=capture_implementation_not_allowed_yet`
- `capture_implementation_added=0`
- `capture_executed=0`
- `capture_runtime_executed=0`
- `datagram_sent=0`
- `datagram_received=0`
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

## Proven Contract

The following positive contract is stable for the runtime-skeleton boundary:

1. Offline qport/session policy exists.
2. Offline qport/session fixtures exist and are guarded.
3. Offline fixture validator passes.
4. Capture policy gate passes while denying capture.
5. Preflight dry-run manifest exists.
6. Dry-run validator passes.
7. Dry-run wrapper plan and validate modes pass.
8. CI manifest and drift gate pass.
9. Final policy gate passes.
10. Disabled-by-default capture shell exists.
11. Disabled-by-default runtime skeleton exists.
12. Skeleton-only runtime plan is created and validated.
13. All runtime skeleton gates passed.

## Not Proven

These areas remain explicitly unproven and forbidden to claim from this boundary:

- qport/session byte evidence
- qport width, endian, order, placement, or relation to UDP source port
- capture execution
- capture runtime execution
- packet capture
- socket opening
- datagram send
- datagram receive
- public socket exposure
- LAN socket exposure
- loopback capture execution
- Steam use
- real Half-Life client execution
- real client binary invocation
- getchallenge/connect runtime execution
- post-connect serverinfo bytes
- signon serverinfo bytes
- netchan runtime
- reliable or unreliable runtime channel behavior
- auth, resource/baseline, or admission behavior
- real HLDS-compatible client compatibility
- compatibility claim expansion

## Runtime Skeleton Boundary Checklist

Before running the skeleton probe:

- Confirm the branch is `codex/HL-CL-20260401-081-target-runtime-completion-state`.
- Confirm the offline policy exists at `fixtures/diagnostic/hlds/qport_session/qport_session_offline_fixture_manifest_policy.json`.
- Confirm the dry-run manifest exists at `fixtures/diagnostic/hlds/qport_session/qport_session_capture_preflight_dry_run_manifest.json`.
- Confirm the CI manifest exists at `fixtures/diagnostic/hlds/qport_session/qport_session_capture_shell_ci_manifest.json`.
- Confirm the wrapper exists at `scripts/run_hlds_qport_session_capture_dry_run.ps1`.
- Confirm no prompt asks to execute capture, open sockets, send or receive datagrams, invoke Steam, invoke a real client, or run runtime network stages.

Required gates:

- Final policy gate must pass.
- CI drift gate must pass.
- Offline fixture validator must pass.
- Capture policy gate must pass while denying capture.
- Dry-run validator must pass.
- Wrapper validation must pass.
- Shell boundary must be validated.

Pass requires:

- `accepted=1`
- `runtime_skeleton_added=1`
- `skeleton_plan_created=1`
- `skeleton_plan_validated=1`
- all required gates reported as passing
- `capture_allowed_now=0`
- `capture_blocked_by_policy=1`

Fail immediately if:

- `capture_executed` becomes `1`
- `capture_runtime_executed` becomes `1`
- `socket_open_attempted` becomes `1`
- `datagram_sent` becomes `1`
- `datagram_received` becomes `1`
- `real_client_binary_invoked` becomes `1`
- `connect_path_invoked`, `post_connect_serverinfo_path_invoked`, `signon_serverinfo_path_invoked`, or `netchan_runtime_started` becomes `1`
- `qport_session_byte_evidence_sufficient` becomes `1` without a separate byte-evidence prompt
- `compatibility_claim_expansion_allowed_now` becomes `1`

If any failure marker appears, stop and treat it as a policy or implementation regression. Do not continue to capture execution or compatibility-claim work.

## Boundary Risks

| Risk | Boundary guard | Required response |
| --- | --- | --- |
| Skeleton misuse | Disabled-by-default probe and scenario gates | Reject unsafe invocation. |
| Accidental capture execution | `capture_executed=0` required | Stop and regress the skeleton. |
| Accidental capture runtime execution | `capture_runtime_executed=0` required | Stop and regress the skeleton. |
| Accidental socket open | `socket_open_attempted=0` required | Stop and remove socket path. |
| Accidental datagram send/receive | `datagram_sent=0` and `datagram_received=0` required | Stop and remove datagram path. |
| Loopback capture overreach | No loopback capture execution allowed | Require separate policy before active loopback capture. |
| Public/LAN exposure | Public and LAN fields must stay `0` | Reject as critical regression. |
| Real client action | Real-client and Steam fields must stay `0` | Reject as critical regression. |
| Connect/post-connect/signon path creep | Runtime path fields must stay `0` | Reject skeleton expansion. |
| Netchan runtime creep | `netchan_runtime_started=0` required | Reject skeleton expansion. |
| Qport evidence promotion | Byte evidence fields must stay `0` | Require separate byte-evidence prompt. |
| Address-scoped challenge real-netchan overclaim | Real-netchan proof field must stay `0` | Reject promotion. |
| CI drift gate bypass | CI drift gate required in happy path | Block skeleton plan creation. |
| Wrapper validation bypass | Wrapper validation required | Block skeleton plan creation. |
| Stale docs | Stable docs name exact files and commits | Update docs with any boundary change. |
| Compatibility overclaim | Diagnostic-only claim string required | Reject release. |

## Next Prompt

Recommended next prompt:

`HL-CL-20260504-319-dedicated-goldsrc-hlds-qport-session-capture-runtime-skeleton-ci-manifest-and-drift-gate`

The runtime skeleton boundary is clean. The next step should manifest the qport/session runtime skeleton boundary and guard drift before any future capture execution discussion.
