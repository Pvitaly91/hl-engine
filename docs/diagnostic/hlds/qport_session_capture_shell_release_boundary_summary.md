# HL-CL-20260504-313 Qport/Session Capture Shell Release Boundary

Compatibility claim level: diagnostic-qport-session-capture-shell-release-boundary-only; no real Steam Half-Life or HLDS-compatible client compatibility claimed

This boundary covers the diagnostic qport/session stack from prompts 304 through 312, with static and design prerequisites from prompts 302 and 303. It records that a disabled-by-default no-client capture shell now exists, but that shell is still only a gated diagnostic shell. It does not execute capture, open sockets, run runtime network paths, start netchan, invoke Steam, invoke a real client, or expand compatibility claims.

## Stable Boundary

| Item | Path or value |
| --- | --- |
| Branch | `codex/HL-CL-20260401-081-target-runtime-completion-state` |
| Covered prompt range | `HL-CL-20260504-304` through `HL-CL-20260504-312` |
| Related static/design prompts | `HL-CL-20260504-302`, `HL-CL-20260504-303` |
| Fixture root | `fixtures/diagnostic/hlds/qport_session` |
| Offline fixture policy | `fixtures/diagnostic/hlds/qport_session/qport_session_offline_fixture_manifest_policy.json` |
| Offline fixtures | `fixtures/diagnostic/hlds/qport_session/fixtures/*.json` |
| Dry-run manifest | `fixtures/diagnostic/hlds/qport_session/qport_session_capture_preflight_dry_run_manifest.json` |
| Dry-run wrapper | `scripts/run_hlds_qport_session_capture_dry_run.ps1` |
| Wrapper docs | `docs/diagnostic/hlds/qport_session_capture_dry_run_wrapper.md` |
| Capture shell source area | `include/app/launch_options.h`, `src/app/launch_options.cpp`, `src/app/host_application.cpp`, `include/game_api/hl_server_module.h`, `src/game_api/hl_server_module.cpp` |

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

## Prompt 312 Proof Summary

Prompt 312 implemented and proved the disabled-by-default capture shell probe. Its full proof matrix passed:

| Scenario | Result |
| --- | --- |
| `happy` | pass |
| `gate_disabled_by_default` | pass |
| `gate_policy_gate_required` | pass |
| `gate_offline_validator_required` | pass |
| `gate_dry_run_validator_required` | pass |
| `gate_wrapper_validation_required` | pass |
| `gate_capture_execution_blocked` | pass |
| `gate_socket_request_blocked` | pass |
| `gate_real_client_request_blocked` | pass |
| `gate_public_lan_request_blocked` | pass |
| `gate_connect_postconnect_signon_request_blocked` | pass |
| `gate_netchan_runtime_request_blocked` | pass |
| `gate_qport_evidence_promotion_blocked` | pass |
| `gate_address_scoped_challenge_real_netchan_claim_blocked` | pass |
| `gate_no_real_client_used` | pass |

Expected happy markers:

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

Expected blocked markers:

- `capture_allowed_now=0`
- `capture_blocked_by_policy=1`
- `capture_block_reason=capture_implementation_not_allowed_yet`
- `capture_implementation_added=0`
- `capture_execution_requested=0`
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
- `normal_host_behavior_changed=0`

## Proven Contract

The following positive contract is stable for the capture-shell boundary:

1. Offline qport/session policy exists.
2. Offline qport/session fixtures exist.
3. Offline fixture validator passes.
4. Capture policy gate passes while denying capture.
5. Preflight dry-run manifest exists.
6. Dry-run validator passes.
7. Dry-run wrapper plan and validate modes pass.
8. Implementation policy review permits only a disabled-by-default shell.
9. Disabled-by-default capture shell exists.
10. Shell-only plan is created and validated.
11. Shell requires offline validator, policy gate, dry-run validator, and wrapper validation.
12. All shell gates passed.

## Not Proven

These areas remain explicitly unproven and forbidden to claim from this boundary:

- qport/session byte evidence
- qport width, endian, order, placement, or relation to UDP source port
- capture execution
- packet capture
- socket behavior
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

## Shell Boundary Checklist

Before running the shell probe:

- Confirm the branch is `codex/HL-CL-20260401-081-target-runtime-completion-state`.
- Confirm the offline policy exists at `fixtures/diagnostic/hlds/qport_session/qport_session_offline_fixture_manifest_policy.json`.
- Confirm the dry-run manifest exists at `fixtures/diagnostic/hlds/qport_session/qport_session_capture_preflight_dry_run_manifest.json`.
- Confirm wrapper validation artifacts or wrapper validate mode remain available.
- Confirm no prompt asks to execute capture, open sockets, invoke Steam, invoke a real client, or run runtime network stages.

Required validators:

- Offline fixture validator must pass.
- Capture policy gate must pass while denying capture.
- Dry-run validator must pass with safe planned actions.
- Wrapper validation must pass.

Pass requires:

- `accepted=1`
- `capture_shell_added=1`
- `shell_plan_created=1`
- `shell_plan_validated=1`
- all required validators and wrapper validation reported as invoked or checked and passing
- `capture_allowed_now=0`
- `capture_blocked_by_policy=1`

Fail immediately if:

- `capture_allowed_now` becomes `1`
- `socket_open_attempted` becomes `1`
- `capture_executed` becomes `1`
- `capture_runtime_executed` becomes `1`
- `real_client_binary_invoked` becomes `1`
- `connect_path_invoked`, `post_connect_serverinfo_path_invoked`, `signon_serverinfo_path_invoked`, or `netchan_runtime_started` becomes `1`
- `qport_session_byte_evidence_sufficient` becomes `1` without a separate byte-evidence prompt

If any failure marker appears, stop and treat it as a policy or implementation regression. Do not continue to capture implementation.

## Boundary Risks

| Risk | Boundary guard | Required response |
| --- | --- | --- |
| Shell misuse | Disabled-by-default probe and scenario gates | Reject unsafe invocation. |
| Accidental capture execution | `capture_executed=0` and `capture_runtime_executed=0` required | Stop and regress the shell. |
| Accidental socket open | `socket_open_attempted=0` required | Stop and remove socket path. |
| Loopback capture overreach | No loopback capture execution allowed | Require separate policy before active loopback capture. |
| Public/LAN exposure | Public and LAN fields must stay `0` | Reject as critical regression. |
| Real client action | Real-client fields must stay `0` | Reject as critical regression. |
| Connect/post-connect/signon path creep | Runtime path fields must stay `0` | Reject shell expansion. |
| Netchan runtime creep | `netchan_runtime_started=0` required | Reject shell expansion. |
| Qport evidence promotion | Byte evidence fields must stay `0` | Require separate byte-evidence prompt. |
| Address-scoped challenge overclaim | Real-netchan proof field must stay `0` | Reject promotion. |
| Wrapper validation bypass | Wrapper validation required | Block shell plan creation. |
| Policy/fixture drift | Offline fixture validator required | Repair fixture policy first. |
| Stale docs | Stable docs name exact files and commits | Update docs with any boundary change. |
| Compatibility overclaim | Diagnostic-only claim string required | Reject release summary. |

## Next Prompt

Recommended next prompt:

`HL-CL-20260504-314-dedicated-goldsrc-hlds-qport-session-capture-shell-ci-manifest-and-drift-gate`

The shell boundary is closed. The next step should manifest the qport/session shell boundary and guard drift before any future implementation expansion.
