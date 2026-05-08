# HL-CL-20260504-303 Qport/Session No-Client Diagnostic Capture Design

Compatibility claim level: diagnostic-qport-session-no-client-capture-design-only; no real Steam Half-Life or HLDS-compatible client compatibility claimed

This document is a design boundary only. It does not implement packet capture, add launch options, add runtime code, open sockets, bind loopback/public/LAN sockets, run getchallenge/connect/post-connect/signon paths, start netchan, start reliable or unreliable channels, start auth, emit resource baselines, enter signon state, admit a client, invoke Steam, invoke a real Half-Life client, or claim real HLDS compatibility.

The closed query/info boundary remains separate. Connectionless query/info is not qport/session evidence, not post-connect serverinfo, not signon-time serverinfo, not netchan evidence, and not real client compatibility evidence.

## Design Inputs

| Input | Result used by this design |
| --- | --- |
| Prompt 296 query/info release boundary | Query/info diagnostic boundary is closed and remains connectionless only. |
| Prompt 299 message-writing inventory | Diagnostic byte helpers exist, but no production network writer or central sizebuf/overflow policy is proven. |
| Prompt 300 sequence/ack inventory | No sufficient netchan sequence/ack byte contract exists. |
| Prompt 301 reliable/unreliable envelope inventory | No sufficient reliable/unreliable envelope byte contract exists. |
| Prompt 302 qport/session static inventory | No local `qport` symbol, no byte-level qport/session evidence, and no qport-to-netchan transition proof exists. |

## Objective

Define a future no-client diagnostic capture design that can acquire qport/session evidence without real clients and without public or LAN exposure. The design is intentionally not executable in this prompt. It describes the requirements a later implementation prompt must satisfy before any code, capture, or runtime proof is added.

The target evidence is narrow:

| Evidence target | Required observation | Why it matters |
| --- | --- | --- |
| Qport field presence | Whether a qport-like field exists in a candidate connect/session fixture. | Without field existence, no qport fixture contract can be defined. |
| Qport width and byte order | Exact width, byte order, and text/binary placement if present. | Sequence/ack and envelope work cannot rely on guessed session binding. |
| Client source port relation | Whether UDP source port and qport are distinct or identical in a fixture. | Prompt 302 proved `client_port` is only local diagnostic metadata. |
| Remote address binding | Exact diagnostic peer key and whether it is endpoint-only or session-bound. | Future netchan channel identity depends on peer binding. |
| Challenge/session linkage | Which challenge value, cache key, and replay state are allowed to feed a future channel. | Address-scoped challenge cache is only a diagnostic prerequisite. |
| Userinfo linkage | Which accepted userinfo fields can be carried into a future diagnostic session descriptor. | Userinfo validation is not admission proof. |
| Connect-to-netchan handoff | The minimal data needed before sequence/ack and reliable/unreliable envelope design can proceed. | No channel object or qport binding exists locally. |
| Reject/disconnect binding | Whether rejection remains endpoint-scoped or becomes session-scoped after connect. | Reject/disconnect bytes cannot be invented. |

## Non-Goals

The no-client design must not:

| Non-goal | Reason |
| --- | --- |
| Use a real Steam Half-Life client | Real client use requires a separate explicit policy prompt. |
| Invoke Steam or client binaries | This boundary is no-client by definition. |
| Bind public or LAN sockets | Public/LAN exposure remains blocked. |
| Start netchan or reliable/unreliable channels | The channel contract is the missing evidence target. |
| Run post-connect or signon serverinfo | Those stages remain blocked until byte-level evidence exists. |
| Treat query/info or getchallenge/connect diagnostics as real netchan proof | Stage confusion is the main risk this design prevents. |
| Implement packet capture in this prompt | This prompt is documentation/design only. |

## Proposed Future Harness Shape

The future harness should be an offline, fixture-fed diagnostic planner first. It should accept only checked-in or explicitly provided byte fixtures, never a live network peer. A later implementation may add a dry-run mode that validates fixture metadata and emits a capture plan, but it must remain disabled by default.

| Component | Future responsibility | Hard guard |
| --- | --- | --- |
| Fixture manifest reader | Load a qport/session fixture manifest and selected fixture id. | Fail if fixture claims real compatibility. |
| Byte fixture classifier | Classify fields as candidate qport, source port, challenge, userinfo, endpoint, session, or unknown. | Unknown bytes remain unknown. |
| Stage validator | Confirm fixture stage is connect/session planning only, not post-connect or signon. | Fail on query/info promotion or signon promotion. |
| Policy validator | Enforce no real client, no public/LAN, no netchan runtime, and no admission. | Fail closed on missing policy fields. |
| Evidence gap reporter | Emit field gaps needed before sequence/ack or envelope fixtures can be defined. | Never synthesize missing qport/session bytes. |
| Cleanup reporter | Prove no sockets or processes were opened because design and fixture validation are offline. | Required for every future proof. |

## Candidate Data Model

Any future qport/session fixture manifest should be machine-readable and small. The minimum fields should be:

| Field | Required value or policy |
| --- | --- |
| `fixture_id` | Stable id, not derived from query/info fixture ids. |
| `stage` | One of `connect_session_candidate` or `qport_session_candidate`; never `connectionless_query`, `post_connect`, or `signon`. |
| `compatibility_claim_level` | Diagnostic-only, with no real Steam/Half-Life or HLDS-compatible client compatibility claimed. |
| `source` | `checked_in_fixture`, `operator_supplied_fixture`, or `report_only`; never `real_client_runtime` for this boundary. |
| `qport_field_present` | `0`, `1`, or `unknown`; unknown cannot pass a fixture contract gate. |
| `qport_width_bits` | Explicit integer only when evidence exists; otherwise `unknown`. |
| `qport_endian` | `little`, `big`, `text`, or `unknown`; unknown cannot become a builder contract. |
| `udp_source_port_relation` | `distinct`, `same`, or `unknown`; must not assume source port equals qport. |
| `challenge_linkage` | Explicit linkage to diagnostic challenge value or `unknown`. |
| `userinfo_linkage` | Explicit linkage to accepted userinfo fields or `unknown`. |
| `netchan_started` | Must be `0` for this design boundary. |
| `real_client_used` | Must be `0`. |
| `public_socket_opened` | Must be `0`. |
| `lan_socket_opened` | Must be `0`. |

## Future Scenarios

These scenarios are design requirements for a later implementation prompt. They are not executed here.

| Scenario | Expected result | Required proof fields |
| --- | --- | --- |
| `happy_fixture_manifest_only` | Accepts a checked-in diagnostic fixture manifest with no socket/client execution. | `accepted=1`, `fixture_manifest_loaded=1`, `qport_session_capture_executed=0`, `real_client_binary_invoked=0`, `public_socket_opened=0`, `lan_socket_opened=0`. |
| `gate_disabled_by_default` | Rejects when the planner is not explicitly enabled. | `accepted=0`, `rejected=1`, `last_reject_reason=qport_session_no_client_capture_disabled`. |
| `gate_real_client_requested` | Rejects any request to use Steam or a real client. | `real_client_binary_invoked=0`, `last_reject_reason=real_client_forbidden`. |
| `gate_public_lan_requested` | Rejects public or LAN socket exposure. | `public_socket_opened=0`, `lan_socket_opened=0`, `last_reject_reason=public_lan_forbidden`. |
| `gate_live_capture_requested` | Rejects runtime capture until a later approved implementation prompt. | `socket_open_attempted=0`, `last_reject_reason=live_capture_not_allowed`. |
| `gate_stage_confusion_query_info` | Rejects reuse of query/info fixture as qport/session proof. | `stage_confusion_detected=1`, `last_reject_reason=query_info_not_qport_session`. |
| `gate_stage_confusion_post_connect` | Rejects post-connect or signon promotion. | `post_connect_serverinfo_path_invoked=0`, `signon_serverinfo_path_invoked=0`. |
| `gate_missing_qport_contract` | Rejects builder contract creation when qport width/order are unknown. | `byte_level_qport_session_evidence_sufficient=0`, `real_wire_builder_complete=0`. |

## Safety Gates

Any future implementation must prove these gates before it can be used:

| Gate | Required value |
| --- | --- |
| Disabled by default | `qport_session_no_client_capture_disabled_by_default=1` |
| No real client | `real_steam_client_used=0`, `real_client_binary_invoked=0` |
| No sockets in manifest-only mode | `socket_open_attempted=0`, `public_socket_opened=0`, `lan_socket_opened=0`, `loopback_udp_socket_opened=0` |
| No live capture in this boundary | `qport_session_capture_executed=0` |
| No runtime stages | `connect_path_invoked=0`, `post_connect_serverinfo_path_invoked=0`, `signon_serverinfo_path_invoked=0`, `netchan_runtime_started=0` |
| No admission | `client_not_put_in_server=1`, `normal_host_behavior_changed=0` |
| No overclaim | compatibility claim remains diagnostic-only |

## Acquisition Stages

| Stage | Objective | Allowed now? | Required output | Exit condition |
| --- | --- | --- | --- | --- |
| A. Design and policy boundary | Document evidence targets and blockers. | yes, this document | Stable design doc and prompt artifacts | Design committed. |
| B. Offline manifest validator | Implement disabled-by-default manifest-only validation. | future prompt only | JSON summary and gate manifests | No sockets, no clients, fixture manifest validated. |
| C. Operator-supplied byte fixture ingestion | Add policy for importing a local fixture without executing clients. | future prompt only | Fixture manifest, hashes, drift gate | Fixture is diagnostic-only and stage-specific. |
| D. No-client loopback harness design | Design a harness that can be reviewed before execution. | future prompt only | Design doc, no runtime | Execution remains blocked. |
| E. No-client loopback harness implementation | Implement only after explicit approval and bounded socket policy. | future prompt only | Runtime proof if approved | Loopback-only and no real client. |
| F. Real-client capture policy | Consider real-client capture only after explicit policy boundary. | not allowed now | Separate approval artifact | Still no public/LAN unless separately approved. |

## Stage Separation

| Stage | Current status | This design allows? | Why |
| --- | --- | --- | --- |
| Connectionless query/info | Closed diagnostic boundary | no new work | It is already complete and separate. |
| getchallenge/connect diagnostics | Existing diagnostic prerequisites | read-only reference only | They are not qport/session byte proof. |
| qport/session fixture planning | Missing evidence target | design only | Static evidence is too weak for a contract. |
| Netchan sequence/ack | Blocked | no | Needs qport/session and envelope evidence first. |
| Reliable/unreliable envelope | Blocked | no | Needs qport/session and sequence/ack evidence. |
| Post-connect serverinfo | Blocked | no | Needs channel/envelope/field evidence. |
| Signon-time serverinfo | Blocked | no | Needs channel/envelope/baseline evidence. |
| Client admission | Blocked | no | Out of scope and explicitly forbidden. |

## Recommended Next Prompt

Recommended next prompt:

`HL-CL-20260504-304-dedicated-goldsrc-hlds-qport-session-offline-fixture-manifest-policy`

Rationale: the safest next step is an offline fixture manifest policy and validator design. Static evidence is too weak for a qport/session fixture contract, and live capture remains intentionally blocked. An offline manifest policy can define hashes, stage labels, required diagnostic-only fields, and fail-closed gates before any runtime harness is considered.
