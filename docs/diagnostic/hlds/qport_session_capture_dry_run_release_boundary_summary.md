# HL-CL-20260504-310 Qport/Session Capture Dry-Run Release Boundary

Compatibility claim level: diagnostic-qport-session-capture-dry-run-release-boundary-only; no real Steam Half-Life or HLDS-compatible client compatibility claimed

This boundary covers the qport/session dry-run stack from prompts 304 through 309, with static and design prerequisites from prompts 302 and 303. It closes only a diagnostic no-client dry-run boundary. It does not allow capture, socket use, runtime networking, real clients, Steam, netchan startup, or real HLDS compatibility claims.

## Stable Boundary

| Item | Path or value |
| --- | --- |
| Branch | `codex/HL-CL-20260401-081-target-runtime-completion-state` |
| Covered prompt range | `HL-CL-20260504-304` through `HL-CL-20260504-309` |
| Related static/design prompts | `HL-CL-20260504-302`, `HL-CL-20260504-303` |
| Fixture root | `fixtures/diagnostic/hlds/qport_session` |
| Offline fixture policy | `fixtures/diagnostic/hlds/qport_session/qport_session_offline_fixture_manifest_policy.json` |
| Offline fixtures | `fixtures/diagnostic/hlds/qport_session/fixtures/*.json` |
| Dry-run manifest | `fixtures/diagnostic/hlds/qport_session/qport_session_capture_preflight_dry_run_manifest.json` |
| Dry-run wrapper | `scripts/run_hlds_qport_session_capture_dry_run.ps1` |
| Wrapper docs | `docs/diagnostic/hlds/qport_session_capture_dry_run_wrapper.md` |

Relevant source commits:

| Prompt | Source commit |
| --- | --- |
| 304 manifest policy | `71e89a6ea848767c94119773ef36be56cc86239a` |
| 305 offline fixture validator | `70c4a99b2eace8c549f8379e4f60af4af034d626` |
| 306 capture policy gate | `7ce0541babd4a2f3a7e7e41e0e2352f6f4022c3d` |
| 307 dry-run plan | `7db1ba1e0b1235d50933bb99d50f7a18d909e6f8` |
| 308 dry-run validator | `1bc41c55242f71447e707f182e652e8bf2f7f3a9` |
| 309 dry-run wrapper | `9bebafe858211606689e326a90637c4455875400` |

Relevant artifact commits:

| Prompt | Artifact commit |
| --- | --- |
| 304 manifest policy | `c830498ceba33c4bd38342edf91217bfb304722d` |
| 305 offline fixture validator | `1333be64e43babf251d22825986fdc4304b98cdf` |
| 306 capture policy gate | `eca41a4b2f5a48666fe6fd9bc02d1480ec970a16` |
| 307 dry-run plan | `53ccbdf3d12a9fca0ef1a6b31c780367369578a6` |
| 308 dry-run validator | `dc885cbc0c7e222adb00570557b01708da1e1eaa` |
| 309 dry-run wrapper | `6dd0ced69d0018fd548f41e3da5fe56e4586dc77` |

## Rerun Commands

Plan mode writes a command plan and summary only:

```powershell
.\scripts\run_hlds_qport_session_capture_dry_run.ps1 -Mode plan -OutDir logs\latest\HL-CL-20260504-310-dedicated-goldsrc-hlds-qport-session-capture-dry-run-release-boundary-summary\wrapper_plan -RunLabelPrefix p310-plan -Strict
```

Validate mode runs only the read-only validators and policy gate:

```powershell
.\scripts\run_hlds_qport_session_capture_dry_run.ps1 -Mode validate -NoBuild -OutDir logs\latest\HL-CL-20260504-310-dedicated-goldsrc-hlds-qport-session-capture-dry-run-release-boundary-summary\wrapper_validate -RunLabelPrefix p310-validate -Strict
```

Expected success markers:

- `accepted=1`
- `wrapper_plan_passed=1`
- `wrapper_validate_passed=1` in validate mode
- `offline_fixture_validator_included=1`
- `capture_policy_gate_included=1`
- `dry_run_validator_included=1`

Expected blocked markers:

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

## Proven Contract

The following positive contract is stable for this boundary:

1. Offline fixture manifest policy exists.
2. Offline qport/session fixtures exist, including expected invalid-policy fixtures.
3. Offline fixture validator passes over the checked-in fixture set.
4. No-client capture policy gate passes while denying capture.
5. Preflight dry-run manifest exists.
6. Dry-run manifest validator passes.
7. Dry-run wrapper supports plan mode.
8. Dry-run wrapper supports validate mode.
9. Wrapper blocks unsafe options before host invocation.
10. Wrapper includes the offline validator, capture policy gate, and dry-run validator.
11. No sockets, runtime stages, capture, real client, Steam, or compatibility expansion occurred.

## Not Proven

These areas remain explicitly unproven and forbidden to claim from this boundary:

- qport/session byte evidence
- qport width, endian, order, or placement
- qport relationship to UDP source port, NAT, or session key
- capture execution
- packet capture
- socket behavior, including loopback capture
- public socket exposure
- LAN socket exposure
- Steam use
- real Half-Life client execution
- getchallenge/connect runtime execution
- post-connect serverinfo bytes
- signon serverinfo bytes
- netchan runtime
- reliable or unreliable runtime channel behavior
- auth, resource/baseline, or client admission behavior
- real HLDS-compatible client compatibility

## Release Gate Checklist

Before running the wrapper:

- Confirm the branch is `codex/HL-CL-20260401-081-target-runtime-completion-state`.
- Confirm the offline policy and dry-run manifest exist at the stable paths above.
- Confirm the wrapper script and docs exist.
- Confirm no prompt asks to execute capture, open sockets, invoke Steam, or invoke a real client.

Inspect the wrapper summary:

- Pass requires `accepted=1`, `wrapper_plan_passed=1`, and, for validate mode, `wrapper_validate_passed=1`.
- Pass requires `capture_allowed_now=0` and `capture_blocked_by_policy=1`.
- Pass requires all socket/client/runtime fields to remain `0`.
- Fail if `capture_allowed_now` becomes `1`; stop and treat it as a policy regression.
- Fail if any plan or summary contains public, LAN, real-client, Steam, connect, post-connect, signon, netchan, auth, admission, capture, or socket action.

Inspect policy gate and dry-run validator summaries:

- Policy gate must report capture blocked by policy.
- Dry-run validator must report `planned_actions_safe=1`.
- Address-scoped challenge must remain diagnostic prerequisite only.
- Qport/session byte evidence must remain insufficient.

If policy or fixture drift is detected, stop and harden the offline fixture policy before any implementation prompt.

## Boundary Risks

| Risk | Boundary guard | Required response |
| --- | --- | --- |
| Fixture or policy drift | JSON validation plus offline validator | Stop and repair fixture policy. |
| Dry-run manifest drift | Dry-run validator | Stop and repair manifest/schema. |
| Wrapper option misuse | Unsafe option rejection | Stop before host invocation. |
| Accidental capture enablement | `capture_allowed_now=0` required | Treat as regression. |
| Socket action in plan | `socket_open_attempted=0` and safe planned actions | Reject plan. |
| Public or LAN exposure | Public/LAN fields must stay `0` | Reject plan. |
| Real client or Steam action | Real-client fields must stay `0` | Reject plan. |
| Connect/post-connect/signon action | Runtime fields must stay `0` | Reject plan. |
| Address-scoped challenge promoted to real netchan proof | `address_scoped_challenge_reusable_as_real_netchan_proof=0` | Reject promotion. |
| Qport/session evidence overclaim | `byte_level_qport_session_evidence_sufficient=0` | Reject promotion. |
| Stale docs | Stable docs name exact paths and commands | Update docs with any path change. |
| Future capture implementation overreach | Separate policy review required | Do not implement capture in this boundary. |

## Next Prompt

Recommended next prompt:

`HL-CL-20260504-311-dedicated-goldsrc-hlds-qport-session-capture-implementation-policy-review`

The dry-run boundary is closed, but any future capture implementation still requires an explicit policy review before socket, runtime, or capture work can be considered.
