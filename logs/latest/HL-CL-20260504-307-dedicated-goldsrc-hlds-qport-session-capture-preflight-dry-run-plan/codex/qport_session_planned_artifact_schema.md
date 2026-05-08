# Planned Future Capture Artifact Schema

Prompt ID: HL-CL-20260504-307-dedicated-goldsrc-hlds-qport-session-capture-preflight-dry-run-plan

These artifacts are planned names and schemas only. They were not produced as live capture outputs.

| Planned Artifact | Purpose | Required Safety Fields |
| --- | --- | --- |
| `qport_session_capture_plan.json` | Future non-executed capture command and precondition plan. | `capture_executed`, `socket_opened`, `real_client_used`, `reject_reason` |
| `qport_session_capture_preflight_summary.json` | Future deterministic preflight summary. | `capture_allowed_now`, `capture_blocked_by_policy`, `capture_block_reason` |
| `qport_session_planned_datagram_shapes.md` | Future description of planned datagram shapes without byte claims. | `unresolved_fields`, `safe_preview`, `reject_reason` |
| `qport_session_planned_guard_results.json` | Future guard matrix output. | `public_socket_opened`, `lan_socket_opened`, `connect_path_invoked`, `netchan_runtime_started` |
| `qport_session_planned_cleanup.md` | Future cleanup and rollback plan for dry-run artifacts only. | `capture_executed`, `socket_opened`, `normal_host_behavior_changed` |
| `qport_session_planned_unresolved_fields.md` | Future unresolved-field inventory. | `qport_width`, `qport_endian`, `endpoint_key`, `challenge_cache_key` |

## Required Data Fields

Future artifact schemas must include:

- `qport_observed`
- `qport_raw`
- `qport_width`
- `qport_endian`
- `client_udp_source_port`
- `remote_address`
- `endpoint_key`
- `challenge_value`
- `challenge_cache_key`
- `userinfo_identity`
- `connect_ready`
- `netchan_started`
- `signon_started`
- `capture_executed`
- `socket_opened`
- `public_socket_opened`
- `lan_socket_opened`
- `safe_preview`
- `unresolved_fields`
- `reject_reason`

## Current Evidence Boundary

The planned schema must preserve unknown byte-level behavior as unknown. Qport/session byte evidence remains insufficient, and address-scoped challenge remains a diagnostic prerequisite only.

