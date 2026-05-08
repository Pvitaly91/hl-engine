# HL-CL-20260504-309 Qport/Session Capture Dry-Run Wrapper

Compatibility claim level: diagnostic-qport-session-capture-dry-run-wrapper-only; no real Steam Half-Life or HLDS-compatible client compatibility claimed

## Scope

`scripts/run_hlds_qport_session_capture_dry_run.ps1` is a diagnostic-only wrapper for the qport/session no-client capture preflight chain. It creates a deterministic command plan and, in validate mode, invokes only the checked-in read-only validation probes from earlier prompts:

- prompt 305 offline qport/session fixture validator
- prompt 306 qport/session no-client capture policy gate
- prompt 308 qport/session capture preflight dry-run validator

The wrapper does not implement capture, execute capture, open sockets, invoke Steam, invoke a real client binary, run connect/post-connect/signon paths, start netchan, start admission, or promote any qport/session byte evidence.

## Stable Inputs

| Input | Path |
| --- | --- |
| dry-run manifest | `fixtures/diagnostic/hlds/qport_session/qport_session_capture_preflight_dry_run_manifest.json` |
| offline fixture policy | `fixtures/diagnostic/hlds/qport_session/qport_session_offline_fixture_manifest_policy.json` |
| fixture root | `fixtures/diagnostic/hlds/qport_session` |
| dry-run plan doc | `docs/diagnostic/hlds/qport_session_capture_preflight_dry_run_plan.md` |

The wrapper requires the dry-run manifest to keep `capture_allowed_now=false`, `capture_blocked_by_policy=true`, and `capture_block_reason=capture_implementation_not_allowed_yet`.

## Modes

| Mode | Behavior |
| --- | --- |
| `plan` | Writes a command plan and wrapper summary only. No host command is invoked. |
| `validate` | Runs the three read-only diagnostic probes with happy scenarios and validates their summary fields. |

`plan` is the default mode. `validate` remains bounded to local checked-in policy and fixture validation surfaces.

## Example Commands

Plan only:

```powershell
.\scripts\run_hlds_qport_session_capture_dry_run.ps1 -Mode plan -OutDir logs\latest\HL-CL-20260504-309-dedicated-goldsrc-hlds-qport-session-no-client-capture-dry-run-wrapper\wrapper_plan
```

Validate read-only probes:

```powershell
.\scripts\run_hlds_qport_session_capture_dry_run.ps1 -Mode validate -NoBuild -OutDir logs\latest\HL-CL-20260504-309-dedicated-goldsrc-hlds-qport-session-no-client-capture-dry-run-wrapper\wrapper_validate
```

Use `-Build` only when a local rebuild is intentionally required. The wrapper does not require build by default and validates against `build32/host/Debug/hlhost.exe` when `-NoBuild` is used.

## Summary Fields

The wrapper summary JSON includes:

- `wrapper_script_created`
- `wrapper_diagnostic_only`
- `wrapper_plan_mode_supported`
- `wrapper_plan_generated`
- `wrapper_plan_passed`
- `wrapper_validate_mode_supported`
- `wrapper_validate_executed`
- `wrapper_validate_passed`
- `offline_fixture_validator_included`
- `capture_policy_gate_included`
- `dry_run_validator_included`
- `capture_allowed_now`
- `capture_blocked_by_policy`
- `capture_block_reason`
- `capture_implementation_added`
- `capture_executed`
- `capture_runtime_executed`
- `qport_session_byte_evidence_sufficient`
- `byte_level_qport_session_evidence_sufficient`
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
- `unsafe_options_blocked`

Expected safe values keep capture blocked and all socket, client, runtime, netchan, and compatibility-expansion fields at zero.

## Forbidden Options

The wrapper rejects these options before host execution:

- `-Public`
- `-LAN`
- `-RealClient`
- `-Steam`
- `-Capture`
- `-Socket`
- `-Connect`
- `-PostConnect`
- `-Signon`
- `-Netchan`
- `-Auth`
- `-Admission`

`-OmitDryRunValidator` exists only as a diagnostic gate input and is rejected with `dry_run_validator_required`.

## Blocked Decision

Capture remains blocked because:

- qport/session byte evidence is still insufficient
- capture implementation is not allowed here
- capture execution is not allowed here
- socket behavior is not allowed here
- real clients and Steam are not allowed here
- connect/post-connect/signon/netchan runtime paths are not allowed here
- address-scoped challenge remains a diagnostic prerequisite only, not real netchan proof

## Troubleshooting

If validate mode fails with `hlhost executable not found`, either run a prior build or use `-Build`. If it fails with `diagnostic game directory not found`, pass `-GameDir` pointing to an existing local diagnostic fixture directory. If a probe rejects, inspect the per-step `_wrapper_stdout.log` and summary log paths recorded in `qport_session_capture_dry_run_command_log.txt`.

## Next Boundary

When both plan and validate modes pass while capture remains blocked, the safe next step is a release-boundary summary for the dry-run wrapper. That summary still must not permit capture, sockets, runtime network paths, real clients, or compatibility claim expansion.
