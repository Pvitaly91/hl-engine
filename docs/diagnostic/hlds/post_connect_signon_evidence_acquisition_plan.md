# Post-Connect And Signon-Time Serverinfo Evidence Acquisition Plan

Prompt: HL-CL-20260504-297-dedicated-goldsrc-hlds-post-connect-signon-evidence-acquisition-plan

Compatibility claim level:

```text
diagnostic-post-connect-signon-evidence-acquisition-plan-only; no real Steam Half-Life or HLDS-compatible client compatibility claimed
```

This is a planning boundary only. It does not capture packets, run a real
Steam Half-Life client, launch Steam, invoke real client binaries, bind sockets,
open public or LAN networking, start auth, start netchan, enter signon, emit
baselines, or admit a client.

## Closed Query/Info Boundary

The diagnostic connectionless query/info boundary is closed and documented by
prompts 287 through 296:

| Prompt | Boundary contribution |
|---|---|
| 287 | Fixture-backed byte-level connectionless query/info builder/parser |
| 288 | Diagnostic query/info path integration |
| 289 | Localhost loopback query/info response swap |
| 290 | Query/info loopback policy review |
| 291 | Opt-in diagnostic query-client smoke |
| 292 | Query/info loopback regression acceptance gate |
| 293 | CI manifest and fixture drift gate |
| 294 | Rerun wrapper |
| 295 | Operator checklist and quickstart |
| 296 | Release boundary summary |

Stable query/info references:

- Selected fixture: `connectionless_query_info_candidate`
- Selected stage: `connectionless_query`
- CI manifest: `fixtures/diagnostic/hlds/query_info_regression/query_info_regression_ci_manifest.json`
- Wrapper: `scripts/run_hlds_query_info_regression.ps1`
- Operator checklist: `docs/diagnostic/hlds/query_info_regression_operator_checklist.md`
- Quickstart: `docs/diagnostic/hlds/query_info_regression_quickstart.md`
- Release summary: `docs/diagnostic/hlds/query_info_release_boundary_summary.md`

That boundary proves only connectionless query/info diagnostics. It is not
post-connect serverinfo. It is not signon-time serverinfo. It is not real HLDS
or Steam Half-Life client compatibility.

## Evidence Target Definition

Post-connect serverinfo and signon-time serverinfo need separate byte-level
evidence. The following properties must be known before either stage can be
promoted from unresolved fixture data to a buildable or parseable real-stage
wire contract:

| Target | Post-connect serverinfo requirement | Signon-time serverinfo requirement |
|---|---|---|
| Packet stage | Prove the exact stage after accepted connect and before any later transport transition | Prove the exact signon state and message window where serverinfo is sent |
| Direction | Server-to-client response or message direction must be explicit | Server-to-client signon stream direction must be explicit |
| Marker/header | Determine whether a connectionless marker, netchan header, or other frame header is used | Determine netchan/signon framing header and sequence context |
| Opcode/message id | Determine the real message id or opcode for serverinfo | Determine whether `svc_serverinfo` or another signon message id is used |
| Field order | Record byte-exact field ordering | Record signon payload ordering relative to other signon messages |
| Field types | Record every scalar/string field and any optional fields | Record every scalar/string field and signon-only fields |
| String encoding | Prove encoding, termination, escaping, and bounds | Prove encoding, termination, escaping, and signon stream bounds |
| Numeric encoding | Prove widths and byte order | Prove widths and byte order |
| Packet length/framing | Prove length limits and frame boundaries | Prove signon message length boundaries and packet splitting rules |
| Netchan dependency | Determine whether netchan must exist before this serverinfo | Determine sequence, ack, and channel state dependencies |
| Reliable/unreliable dependency | Determine if the payload is reliable, unreliable, or connectionless | Determine reliable stream placement and resend/ack behavior |
| Challenge/session linkage | Bind to challenge, client endpoint, and accepted connect state | Bind to session state and signon progression |
| Userinfo linkage | Determine which accepted userinfo fields affect output | Determine which userinfo/session fields affect output |
| Map/game linkage | Prove map name, game directory, server count, and spawn count sources | Prove map/game/server count/spawn count placement in signon |
| Baseline linkage | Identify resource/model/sound/event baseline prerequisites | Identify messages before and after serverinfo and baseline dependency |
| Reject/disconnect behavior | Prove byte-level unsupported/reject behavior | Prove disconnect/reject behavior if signon cannot continue |

## Local Evidence Sources

| Source | Status now | What it provides | Gap or restriction |
|---|---|---|---|
| `fixtures/diagnostic/hlds/serverinfo/fixtures/connectionless_query_info_candidate.json` | Available now | Byte-level connectionless query/info fixture | Connectionless-only; forbidden as post-connect or signon evidence |
| `fixtures/diagnostic/hlds/serverinfo/fixtures/diagnostic_post_connect_serverinfo_current.json` | Available now | Current diagnostic post-connect text preview from earlier prompts | Preview-only; not byte-level real post-connect evidence |
| `fixtures/diagnostic/hlds/serverinfo/fixtures/post_connect_real_serverinfo_unresolved.json` | Available now | Explicit post-connect evidence gap and required fields | Non-buildable and non-parseable until byte evidence exists |
| `fixtures/diagnostic/hlds/serverinfo/fixtures/signon_time_serverinfo_unresolved.json` | Available now | Explicit signon-time evidence gap and required fields | Non-buildable and non-parseable until signon byte evidence exists |
| Serverinfo fixture contract and schema | Available now | Metadata and evidence-gap guard rules | Contract does not itself provide real wire bytes |
| Query/info CI manifest | Available now | Drift guard for closed query/info boundary | Query/info-only; does not expand real-stage scope |
| Prompt 279 summary | Available now | Earlier inventory found no sufficient local post-connect/signon serverinfo wire contract | Confirms planning blocker remains |
| Prompt 285 summary | Available now | Classified one connectionless byte candidate, zero post-connect byte candidates, zero signon byte candidates | Confirms byte evidence is insufficient |
| Host launch options and server module code | Available now | Many diagnostic surfaces, signon state flags, challenge/connect/userinfo scaffolding, write helper symbols | Needs focused static dependency inventory before capture design |
| Existing logs/artifacts from prompts 287-296 | Available now | Query/info closure, wrapper, drift policy, no public/LAN/real-client guarantees | No post-connect/signon capture was performed |
| Real Steam/Half-Life client capture | Unsafe/not allowed now | Could eventually provide real compatibility evidence | Requires separate policy prompt and explicit approval before execution |
| Public/LAN capture | Unsafe/not allowed now | Could eventually expose network behavior | Forbidden now; requires separate policy prompt |
| Local byte fixture supplied by an operator | Requires separate policy prompt | Could provide offline byte evidence without executing a client | Needs ingestion policy, provenance, hashes, and stage guard |

## Acquisition Methods Matrix

| Method | Allowed now | Why | Safety risks | Evidence value | Required preconditions | Required prompt before execution | Expected artifacts | Cleanup |
|---|---|---|---|---|---|---|---|---|
| Static repo/code inspection | Yes | Reads checked-in files only | Low, if no runtime is executed | Medium for dependencies and candidate symbols | None beyond branch/ancestry checks | None for read-only inventory | Static dependency matrix, source symbol map | None |
| Fixture extension from local docs | Conditional | Safe only when local docs contain explicit byte evidence | Risk of overclaiming docs as real wire evidence | Medium if provenance is clear | Source doc path, hash, stage mapping | Fixture ingestion or fixture update prompt | Fixture diff, schema validation, guard proof | Revert or quarantine bad fixture |
| No-client local diagnostic capture harness design | Plan only now | Can design without executing sockets or clients | Scope creep into runtime capture | Medium for future engine-only captures | Static dependency inventory | No-client diagnostic capture design prompt | Harness design, forbidden-action checklist | None at design stage |
| Loopback no-auth diagnostic capture | No | Would execute networking and stage logic | Could blur auth, netchan, signon, and admission boundaries | Potentially high if policy-gated | No-auth policy, loopback-only review, capture limits | No-auth local loopback policy review | Bounded capture logs, socket cleanup proof | Close sockets, delete transient endpoints |
| Real Steam/Half-Life client capture | No | Explicitly forbidden now | Invokes real client and may create compatibility overclaim | High if eventually approved | Real-client policy, operator approval, isolated env | Real-client capture policy boundary | Capture files, provenance, compatibility caveats | Kill client, sanitize logs, verify no public exposure |
| Public/LAN query capture | No | Public/LAN exposure is forbidden | Public networking exposure | Not needed for post-connect/signon first step | Separate public/LAN policy | Public/LAN exposure blocker/review | Policy artifacts only before any execution | Network teardown proof if ever allowed |
| HLDS reference capture if locally available | No | Could invoke external binaries or reference server | External binary and licensing/provenance risk | High if provenance and capture are controlled | Local path policy, no public sockets, no real client unless approved | Reference fixture ingestion or capture policy prompt | Hashes, capture provenance, fixture candidate | Terminate reference process, sanitize artifacts |

## Safe Staged Plan

### Stage A: Static Dependency Inventory Hardening

Objective: build a focused static inventory for netchan, signon, serverinfo,
message writing, challenge/session linkage, userinfo linkage, and baseline
dependencies.

Forbidden actions: no runtime execution, no sockets, no packet capture, no real
clients, no public/LAN networking, no fixture promotion.

Required inputs: checked-in code, checked-in fixtures, existing prompt artifacts.

Expected artifacts: dependency inventory, unresolved field matrix, candidate
symbol map, next-stage risk table.

Pass/fail: pass only if the inventory explicitly separates post-connect and
signon-time requirements and preserves the query/info evidence-gap guard.

Suggested next prompt:
`HL-CL-20260504-298-dedicated-goldsrc-hlds-netchan-signon-serverinfo-static-dependency-inventory`.

### Stage B: No-Client Diagnostic Capture Harness Design

Objective: design an engine-only diagnostic capture harness that can later
record locally generated stage bytes without real clients.

Forbidden actions: no capture execution, no client binary, no Steam, no public
or LAN socket, no admission.

Required inputs: Stage A dependency inventory and existing fixture contract.

Expected artifacts: design doc, state machine boundary, proposed summary
fields, cleanup plan, rejected unsafe modes.

Pass/fail: pass only if the design remains disabled by default and cannot be
confused with real compatibility.

### Stage C: No-Auth Local Loopback Policy Review

Objective: decide whether a no-auth loopback-only diagnostic capture can be
allowed without claiming real compatibility.

Forbidden actions: no real client, no public/LAN socket, no Steam, no auth
bypass presented as production behavior.

Required inputs: Stage A and B artifacts.

Expected artifacts: policy matrix, loopback bind policy, timeout policy,
cleanup policy, compatibility claim limits.

Pass/fail: pass only if public/LAN/real-client/connect admission boundaries
remain explicit.

### Stage D: Reference Fixture Ingestion Policy

Objective: define how an operator-provided byte fixture can be ingested without
executing external binaries.

Forbidden actions: no capture, no binary invocation, no fixture promotion
without provenance and validator gates.

Required inputs: byte payload, provenance statement, hash, stage label, field
notes.

Expected artifacts: ingestion checklist, quarantine fixture, schema validation,
evidence-gap guard decision.

Pass/fail: pass only if selected stage and compatibility claim remain
diagnostic until separate promotion proof exists.

### Stage E: Real-Client Smoke Planning Only

Objective: plan the conditions for a future real-client capture without running
it.

Forbidden actions: no Steam, no real client, no public/LAN socket, no capture.

Required inputs: prior stages, isolated environment plan, legal/provenance
policy, rollback plan.

Expected artifacts: real-client policy checklist and explicit approval gates.

Pass/fail: pass only if execution remains blocked until a dedicated approval
prompt.

### Stage F: Eventual Real-Client Capture

Objective: only after explicit approval, capture real client/server bytes under
controlled local conditions.

Forbidden actions before approval: all execution remains forbidden.

Required inputs: all prior stages, approval prompt, isolated loopback setup,
sanitization plan.

Expected artifacts if ever allowed: raw capture hashes, sanitized byte fixtures,
field decode notes, cleanup proof, compatibility limitations.

Pass/fail: pass only if evidence is stage-correct and does not bypass auth,
netchan, baseline, signon, or admission requirements.

## Blocker Table

| Blocker | Current state | Why it blocks real compatibility | Safe next action |
|---|---|---|---|
| Post-connect byte evidence missing | `post_connect_byte_evidence_sufficient=0` | No marker/header, opcode, field order, or encoding proof | Stage A static inventory |
| Signon byte evidence missing | `signon_time_byte_evidence_sufficient=0` | No signon message id, payload order, or framing proof | Stage A static inventory |
| Netchan sequencing unknown | Netchan symbols exist, but real sequence contract is not proven | Serverinfo may depend on sequence/ack state | Inventory netchan dependencies |
| Reliable/unreliable channel unknown | Reliable markers exist, but stage placement is unresolved | Signon may depend on reliable stream behavior | Inventory channel dependency |
| Baseline/resource messages unknown | Baseline/resource terms exist, but byte ordering is unresolved | Client signon may require model/sound/event/resource baselines | Inventory baseline dependencies |
| Real client invocation forbidden | Real client markers remain `0` | No real-client compatibility can be claimed without a future policy | Keep blocked until explicit policy |
| Public/LAN exposure forbidden | Public/LAN markers remain `0` | Network exposure is outside diagnostic scope | Keep blocked |
| Real compatibility claim forbidden | Fixture contract rejects real compatibility claims | Existing evidence is diagnostic-only | Keep claim limit explicit |
| Query/info stage confusion risk | Guarded by prompts 286 and 293 | Query/info bytes cannot prove post-connect/signon | Keep drift gate active |
| Fixture drift | Controlled by query/info CI manifest | Drift could weaken closed boundary | Continue drift gate for query/info |

## Recommendation

Recommended next prompt:

```text
HL-CL-20260504-298-dedicated-goldsrc-hlds-netchan-signon-serverinfo-static-dependency-inventory
```

Recommended next task:

```text
Create a focused static dependency inventory for netchan, signon, post-connect serverinfo, signon-time serverinfo, message writing, baseline/resource linkage, challenge/session linkage, and userinfo linkage before any capture design or runtime execution.
```
