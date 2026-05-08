# Qport/Session No-Client Diagnostic Capture Design

Compatibility claim level: diagnostic-qport-session-no-client-capture-design-only; no real Steam Half-Life or HLDS-compatible client compatibility claimed

This is a design artifact only. No capture was implemented or run.

## Design Position

Prompt 302 closed the static inventory with `qport_symbols_found_count=0` and `byte_level_qport_session_evidence_sufficient=0`. That makes a qport/session fixture contract premature. The next useful boundary is an offline, no-client capture design that defines what evidence would be accepted later.

## No-Client Shape

| Part | Design requirement |
| --- | --- |
| Input | Checked-in or explicitly provided byte fixture metadata only. |
| Execution | Manifest validation and command-plan generation only in the first implementation prompt. |
| Network | No socket open in manifest-only mode. |
| Client | No Steam, no Half-Life binary, no real client process. |
| Stage | Connect/session candidate planning only. |
| Output | JSON summary, field-gap report, safety gate report, and fixture drift inputs. |

## Evidence Targets

| Target | Required later evidence |
| --- | --- |
| qport presence | Explicit `present`, `absent`, or `unknown`, with unknown failing contract gates. |
| qport width | Exact width only from fixture evidence. |
| qport endian/order | Explicit endian and byte/order evidence. |
| UDP source port relation | Explicit relation between source port and qport; no equality assumption. |
| remote address/session key | Explicit peer key policy and session binding policy. |
| challenge linkage | Linkage to diagnostic challenge value and cache key, or unknown. |
| userinfo linkage | Accepted userinfo fields and whether they carry into session metadata. |
| connect-to-netchan handoff | Minimal fields required before sequence/ack work can proceed. |

## Rejection Rules

| Request | Required rejection |
| --- | --- |
| Real client capture | `real_client_forbidden` |
| Steam invocation | `real_client_forbidden` |
| Public/LAN sockets | `public_lan_forbidden` |
| Live capture in first implementation | `live_capture_not_allowed` |
| Query/info fixture reuse | `query_info_not_qport_session` |
| Post-connect/signon promotion | `stage_confusion_detected` |
| Builder contract with unknown qport bytes | `qport_session_byte_contract_missing` |

## Non-Proofs

This design still does not prove:

| Area | Status |
| --- | --- |
| Real HLDS compatibility | not proven |
| Real Steam Half-Life client compatibility | not proven |
| Post-connect serverinfo bytes | not sufficient |
| Signon-time serverinfo bytes | not sufficient |
| Reliable/unreliable envelope bytes | not sufficient |
| Netchan sequence/ack bytes | not sufficient |
| qport/session byte contract | not sufficient |
| Client admission | forbidden |
