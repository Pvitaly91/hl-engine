# HL-CL-20260504-307 Qport/Session Capture Preflight Dry-Run Plan

Compatibility claim level: diagnostic-qport-session-capture-preflight-dry-run-plan-only; no real Steam Half-Life or HLDS-compatible client compatibility claimed

This document defines a dry-run layer for future qport/session no-client capture work. It does not implement capture, execute capture, open sockets, run getchallenge/connect/post-connect/signon runtime paths, start netchan, invoke Steam, invoke a real Half-Life client binary, or expand compatibility claims.

## Objective

The dry run exists to show what a future qport/session no-client capture would do before any capture implementation exists. It must show planned inputs, fixture prerequisites, policy-gate checks, planned artifacts, stop conditions, cleanup expectations, and the current blocked decision.

The current decision remains blocked:

- `capture_allowed_now=0`
- `capture_blocked_by_policy=1`
- `capture_block_reason=capture_implementation_not_allowed_yet`
- `qport_session_byte_evidence_sufficient=0`
- `byte_level_qport_session_evidence_sufficient=0`
- `address_scoped_challenge_reusable_as_real_netchan_proof=0`

## Stable Manifest

The machine-readable dry-run manifest is:

`fixtures/diagnostic/hlds/qport_session/qport_session_capture_preflight_dry_run_manifest.json`

It records the dry-run stages, planned artifacts, forbidden actions, expected summary fields, and required preconditions before any future capture implementation can be considered.

## Dry-Run Stages

| Stage | Name | Input | Output | Allowed actions | Forbidden actions | Failure condition |
| --- | --- | --- | --- | --- | --- | --- |
| 0 | load offline fixture policy | `qport_session_offline_fixture_manifest_policy.json` | policy loaded | read checked-in JSON, validate JSON shape | socket open, capture, real client | policy missing or invalid |
| 1 | run offline fixture validator | offline policy and fixtures | validator pass/fail | explicitly run disabled validator, read fixtures | socket open, runtime path, qport promotion | validator fails |
| 2 | run capture policy gate | validator result | policy gate decision | evaluate policy only, reuse validator result | implement capture, execute capture, open socket | policy gate missing or allows capture unexpectedly |
| 3 | generate capture command plan only | policy gate decision | planned command preview | write text or JSON plan | execute command, bind sockets | command contains forbidden action |
| 4 | generate artifact schema preview only | planned command preview | planned artifact schema | document fields, unresolveds, guards | write live bytes, real client data, byte-evidence claim | schema omits safety fields |
| 5 | stop before socket/capture/runtime | schema preview | blocked dry-run stop | emit blocked decision | socket, connect, post-connect, signon, netchan | any runtime or socket field is nonzero |
| 6 | emit blocked decision | blocked stop | dry-run summary | write diagnostic summary, recommend validator prompt | expand compatibility claim, promote challenge to real netchan proof | summary claims capture or compatibility |

## Planned Future Artifacts

These artifacts are planned only. This prompt does not create live capture artifacts.

| Artifact | Purpose | Required fields |
| --- | --- | --- |
| `qport_session_capture_plan.json` | command and gate plan | `capture_allowed_now`, `capture_block_reason`, `planned_command`, `forbidden_actions` |
| `qport_session_capture_preflight_summary.json` | machine-readable preflight result | `qport_observed`, `qport_raw`, `qport_width`, `qport_endian`, `client_udp_source_port`, `remote_address`, `endpoint_key`, `challenge_value`, `challenge_cache_key`, `userinfo_identity`, `connect_ready`, `netchan_started`, `signon_started`, `capture_executed`, `socket_opened`, `public_socket_opened`, `lan_socket_opened`, `safe_preview`, `unresolved_fields`, `reject_reason` |
| `qport_session_planned_datagram_shapes.md` | planned datagram shape notes | unresolved qport, connect, and byte-contract fields |
| `qport_session_planned_guard_results.json` | planned policy/validator gate results | `validator_passed`, `policy_gate_passed`, `capture_allowed_now`, `reject_reason` |
| `qport_session_planned_cleanup.md` | cleanup and state review | temporary files, socket cleanup, runtime cleanup, no runtime state |
| `qport_session_planned_unresolved_fields.md` | unresolved qport/session fields | qport presence, width, endian, order, UDP source port relation, endpoint/session binding |

## Blocked Decision Logic

The dry run must block capture now because:

- capture implementation is not allowed in this prompt
- qport/session byte evidence is insufficient
- socket behavior is not allowed
- runtime stages are not allowed
- a real client is not allowed
- public or LAN exposure is not allowed
- address-scoped challenge is a diagnostic prerequisite only, not real netchan proof

## Guard Matrix

| Guard | Required value | Reason |
| --- | ---: | --- |
| no capture implementation | `capture_implementation_added=0` | This prompt is plan-only. |
| no capture execution | `capture_executed=0` | No capture runtime is permitted. |
| no capture runtime | `capture_runtime_executed=0` | Dry run stops before runtime. |
| no socket open | `socket_open_attempted=0` | Capture preflight cannot bind sockets. |
| no public socket | `public_socket_opened=0` | Public exposure remains forbidden. |
| no LAN socket | `lan_socket_opened=0` | LAN exposure remains forbidden. |
| no loopback socket | `loopback_udp_socket_opened=0` | Even loopback capture is not executed here. |
| no real client | `real_client_binary_invoked=0` | Real client evidence remains disallowed. |
| no Steam | `real_steam_client_used=0` | Steam invocation remains disallowed. |
| no connect path | `connect_path_invoked=0` | Connect runtime remains blocked. |
| no post-connect serverinfo | `post_connect_serverinfo_path_invoked=0` | Post-connect bytes remain unknown. |
| no signon serverinfo | `signon_serverinfo_path_invoked=0` | Signon-time bytes remain unknown. |
| no netchan runtime | `netchan_runtime_started=0` | Netchan contract remains unresolved. |
| no compatibility claim | diagnostic-only claim | No real Steam or HLDS compatibility is claimed. |
| no qport promotion | `byte_level_qport_session_evidence_sufficient=0` | Local qport byte evidence is still missing. |
| no challenge promotion | `address_scoped_challenge_reusable_as_real_netchan_proof=0` | Address-scoped challenge remains a prerequisite only. |

## Optional Future Validator

A later source-level dry-run validator can be added only if it remains disabled by default and read-only. It should check:

- dry-run manifest exists and is valid JSON
- offline fixture policy exists
- offline fixture validator passes
- capture policy gate denies capture
- planned command contains no socket, real-client, public/LAN, connect, post-connect, signon, or netchan action
- planned artifacts include required fields and preserve unresolved qport/session fields

It must still open no sockets, run no runtime path, invoke no real client, and execute no capture.

## Recommended Next Prompt

`HL-CL-20260504-308-dedicated-goldsrc-hlds-qport-session-capture-preflight-dry-run-validator`

The dry-run manifest and docs exist. The next safe task is a disabled-by-default read-only validator for the dry-run manifest and planned artifact schema.
