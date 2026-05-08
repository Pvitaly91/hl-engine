# Qport/Session Capture Preflight Dry-Run Plan

Prompt ID: HL-CL-20260504-307-dedicated-goldsrc-hlds-qport-session-capture-preflight-dry-run-plan

Compatibility claim level: diagnostic-qport-session-capture-preflight-dry-run-plan-only; no real Steam Half-Life or HLDS-compatible client compatibility claimed

## Objective

The dry-run layer is a planning layer only. It shows what a future no-client qport/session capture would check, what inputs it would require, what artifacts it would plan to emit, and why capture remains blocked now.

It does not implement capture, execute capture, open sockets, invoke Steam or a real client, run getchallenge/connect/post-connect/signon paths, start netchan, or promote any qport/session byte evidence.

## Planned Inputs

| Input | Path or Source | Purpose |
| --- | --- | --- |
| Offline fixture root | `fixtures/diagnostic/hlds/qport_session` | Provides policy and fixtures created before any capture implementation. |
| Offline fixture policy | `fixtures/diagnostic/hlds/qport_session/qport_session_offline_fixture_manifest_policy.json` | Defines allowed fixture families, safety fields, unresolved field policy, and promotion guards. |
| Offline fixture validator | prompt 305 validator/probe | Must pass before any capture preflight planning can proceed. |
| Capture policy gate | prompt 306 policy gate/probe | Must deny capture until a later prompt explicitly permits implementation. |

## Dry-Run Stages

| Stage | Name | Allowed Output | Stop Condition |
| --- | --- | --- | --- |
| 0 | Load offline fixture policy | Policy presence and metadata summary | Missing or invalid policy. |
| 1 | Run offline fixture validator | Validator pass/fail summary | Invalid fixture or unsafe claim. |
| 2 | Run capture policy gate | Capture allowed/blocked decision | Policy gate missing or unsafe decision. |
| 3 | Generate capture command plan only | Non-executable command intent summary | Any socket, runtime, or real-client action appears. |
| 4 | Generate artifact schema preview only | Planned artifact field list | Missing safety or unresolved-field fields. |
| 5 | Stop before socket/capture/runtime | Explicit blocked stop summary | Any attempt to execute capture or runtime path. |
| 6 | Emit blocked decision | Deterministic summary fields | Capture remains denied for this prompt. |

## Current Decision

Capture remains blocked.

Reasons:
- Capture implementation is not allowed in this prompt.
- Qport/session byte evidence is insufficient.
- No socket behavior is allowed.
- No runtime network stage is allowed.
- No real client or Steam invocation is allowed.
- Public and LAN exposure remain forbidden.
- Address-scoped challenge evidence is a diagnostic prerequisite only, not real netchan proof.

## Stable Manifest

The stable manifest is:

`fixtures/diagnostic/hlds/qport_session/qport_session_capture_preflight_dry_run_manifest.json`

It records the dry-run stages, required validator and policy gate, planned artifacts, forbidden actions, expected summary fields, and required preconditions before a future capture implementation prompt could proceed.

## Recommended Next Prompt

HL-CL-20260504-308-dedicated-goldsrc-hlds-qport-session-capture-preflight-dry-run-validator

Recommended task: implement a disabled-by-default read-only validator for the qport/session capture preflight dry-run manifest and planned artifact schema without capture, sockets, runtime paths, real clients, or compatibility claim expansion.

