# HL-CL-20260504-315 Qport/Session Capture Shell Wrapper And CI Release Summary

Compatibility claim level: diagnostic-qport-session-capture-shell-wrapper-ci-release-summary-only; no real Steam Half-Life or HLDS-compatible client compatibility claimed

This release summary closes the combined qport/session capture shell wrapper and CI drift boundary after prompts 304 through 314. It documents the checked-in offline policy, fixtures, validators, dry-run wrapper, disabled-by-default capture shell, CI manifest, and drift gate as a diagnostic-only boundary. It does not allow capture execution, socket opening, runtime networking, real clients, Steam, netchan startup, or compatibility claim expansion.

## Stable Boundary

| Item | Path or value |
| --- | --- |
| Branch | `codex/HL-CL-20260401-081-target-runtime-completion-state` |
| Covered prompt range | `HL-CL-20260504-304` through `HL-CL-20260504-314` |
| Related static/design prompts | `HL-CL-20260504-302`, `HL-CL-20260504-303` |
| Fixture root | `fixtures/diagnostic/hlds/qport_session` |
| Offline fixture policy | `fixtures/diagnostic/hlds/qport_session/qport_session_offline_fixture_manifest_policy.json` |
| Offline fixtures | `fixtures/diagnostic/hlds/qport_session/fixtures/*.json` |
| Dry-run manifest | `fixtures/diagnostic/hlds/qport_session/qport_session_capture_preflight_dry_run_manifest.json` |
| CI manifest | `fixtures/diagnostic/hlds/qport_session/qport_session_capture_shell_ci_manifest.json` |
| Dry-run wrapper | `scripts/run_hlds_qport_session_capture_dry_run.ps1` |
| Wrapper docs | `docs/diagnostic/hlds/qport_session_capture_dry_run_wrapper.md` |
| Shell release boundary docs | `docs/diagnostic/hlds/qport_session_capture_shell_release_boundary_summary.md` |
| CI manifest and drift gate docs | `docs/diagnostic/hlds/qport_session_capture_shell_ci_manifest_and_drift_gate.md` |

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
| 314 CI manifest and drift gate | `bf42fc0e59d67ab00aae3286eb1cef3aded6a5d3` |

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
| 314 CI manifest and drift gate | `0a6d8aeac8a3b99d8bcae84d268994324828f7f3` |

## Proven Contract

The positive contract for this boundary is:

1. Qport/session offline fixture policy exists.
2. Qport/session fixtures exist and are guarded by recorded hashes.
3. Offline fixture validator passes over the checked-in fixture set.
4. No-client capture policy gate passes while denying capture.
5. Capture preflight dry-run manifest exists.
6. Dry-run validator passes with safe planned actions.
7. Dry-run wrapper plan mode passes.
8. Dry-run wrapper validate mode passes.
9. Disabled-by-default capture shell exists.
10. Shell-only plan is created and validated.
11. CI manifest exists and records fixture, policy, dry-run manifest, wrapper, and docs hashes.
12. Drift gate passes with no fixture, policy, dry-run manifest, wrapper, or shell dependency drift.
13. All drift gate scenarios passed in prompt 314.
14. The shell boundary can be revalidated without capture.

## Blocked And Not Proven

The following remain blocked or unproven:

- qport/session byte evidence
- qport width, endian, order, placement, and UDP source-port relation
- capture execution
- packet capture
- socket opening
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
- reliable and unreliable runtime channel behavior
- auth, resource/baseline, and admission behavior
- real HLDS-compatible client compatibility
- compatibility claim expansion

## Operator Checklist

Inspect the CI manifest:

```powershell
Get-Content -Raw fixtures\diagnostic\hlds\qport_session\qport_session_capture_shell_ci_manifest.json | ConvertFrom-Json
```

Run the drift gate happy proof with the local diagnostic fixture game directory:

```powershell
.\build32\host\Debug\hlhost.exe `
  --dedicated `
  --gamedir logs\latest\HL-CL-20260409-119-dedicated-goldsrc-signon-envelope-surface\runtime\valve-fixture `
  --maxclients 4 `
  --frames 1 `
  --log-summary-file 1 `
  --log-console-level error `
  --prompt-id HL-CL-20260504-315-dedicated-goldsrc-hlds-qport-session-capture-shell-wrapper-and-ci-release-summary `
  --run-label p315-drift-happy `
  --log-dir logs\latest\runtime\p315-drift-gate-happy `
  --hlds-qport-session-capture-shell-ci-drift-gate-probe `
  --hlds-qport-session-capture-shell-ci-drift-gate-probe-scenario happy
```

Run wrapper plan mode:

```powershell
.\scripts\run_hlds_qport_session_capture_dry_run.ps1 -Mode plan -OutDir logs\latest\HL-CL-20260504-315-dedicated-goldsrc-hlds-qport-session-capture-shell-wrapper-and-ci-release-summary\wrapper_plan -RunLabelPrefix p315-plan -Strict
```

Run wrapper validate mode only when a bounded read-only validator rerun is needed:

```powershell
.\scripts\run_hlds_qport_session_capture_dry_run.ps1 -Mode validate -NoBuild -OutDir logs\latest\HL-CL-20260504-315-dedicated-goldsrc-hlds-qport-session-capture-shell-wrapper-and-ci-release-summary\wrapper_validate -RunLabelPrefix p315-validate -Strict
```

Fields that must remain `1`:

- `capture_shell_added`
- `shell_plan_created`
- `shell_plan_validated`
- `ci_manifest_created`
- `drift_gate_passed`
- `capture_blocked_by_policy`

Fields that must remain `0`:

- `fixture_drift_detected`
- `policy_drift_detected`
- `dry_run_manifest_drift_detected`
- `wrapper_drift_detected`
- `shell_dependency_drift_detected`
- `capture_allowed_now`
- `capture_implementation_added`
- `capture_executed`
- `capture_runtime_executed`
- `qport_session_byte_evidence_sufficient`
- `byte_level_qport_session_evidence_sufficient`
- `address_scoped_challenge_reusable_as_real_netchan_proof`
- `compatibility_claim_expanded`
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

Detect fixture, policy, dry-run manifest, wrapper, and shell dependency drift through the drift gate markers. If any drift marker becomes `1`, stop and run a policy or drift-hardening prompt before any future implementation expansion.

If any blocked field changes, do not continue to capture implementation. Treat it as a release boundary regression.

## Boundary Risk Table

| Risk | Guard | Required response |
| --- | --- | --- |
| Fixture drift | CI manifest fixture hashes and offline validator | Stop and review fixture policy. |
| Policy drift | Policy hash and policy validator expectations | Stop and review policy. |
| Dry-run manifest drift | Dry-run manifest hash and validator | Stop and harden dry-run manifest. |
| Wrapper drift | Wrapper hash and unsafe-option blockers | Stop and harden wrapper. |
| Shell dependency drift | Drift gate dependency checks | Restore offline validator, policy gate, dry-run validator, and wrapper validation requirements. |
| Capture accidentally enabled | `capture_allowed_now=0` | Treat as a blocking regression. |
| Capture accidentally executed | `capture_executed=0`, `capture_runtime_executed=0` | Treat as a blocking regression. |
| Socket accidentally opened | `socket_open_attempted=0` | Treat as a blocking regression. |
| Public/LAN overreach | Public and LAN socket fields remain `0` | Reject as out of scope. |
| Real client overreach | Real client and Steam fields remain `0` | Reject as out of scope. |
| Connect/post-connect/signon path creep | Runtime path fields remain `0` | Reject as out of scope. |
| Netchan runtime creep | `netchan_runtime_started=0` | Reject as out of scope. |
| Qport evidence promotion | Byte evidence fields remain `0` | Require separate evidence prompt. |
| Address-scoped challenge overclaim | Real-netchan proof field remains `0` | Reject promotion. |
| Compatibility claim expansion | Diagnostic-only claim string required | Reject release. |

## Next Prompt

Recommended next prompt:

`HL-CL-20260504-316-dedicated-goldsrc-hlds-qport-session-capture-implementation-final-policy-gate`

This boundary is clean, but a final explicit go/no-go policy gate is still required before any future capture implementation attempt.
