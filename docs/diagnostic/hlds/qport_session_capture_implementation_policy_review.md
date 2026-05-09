# HL-CL-20260504-311 Qport/Session Capture Implementation Policy Review

Compatibility claim level: diagnostic-qport-session-capture-implementation-policy-review-only; no real Steam Half-Life or HLDS-compatible client compatibility claimed

This review sits after the closed qport/session dry-run boundary from prompts 304 through 310. It decides whether the next prompt may move from dry-run documentation into a minimal disabled-by-default capture implementation shell. This review does not implement capture, execute capture, open sockets, run runtime network paths, invoke Steam, invoke a real client, or expand compatibility claims.

## Closed Dry-Run Boundary Reviewed

The current qport/session boundary includes:

| Layer | Stable evidence |
| --- | --- |
| Offline fixture manifest policy | `fixtures/diagnostic/hlds/qport_session/qport_session_offline_fixture_manifest_policy.json` |
| Offline fixture set | `fixtures/diagnostic/hlds/qport_session/fixtures/*.json` |
| Offline fixture validator | prompt 305 validator probe, passing over the checked-in fixtures |
| Capture policy gate | prompt 306 gate, passing while denying capture |
| Preflight dry-run manifest | `fixtures/diagnostic/hlds/qport_session/qport_session_capture_preflight_dry_run_manifest.json` |
| Dry-run validator | prompt 308 validator, passing with safe planned actions |
| Dry-run wrapper | `scripts/run_hlds_qport_session_capture_dry_run.ps1`, plan and validate modes passing |
| Release boundary summary | `docs/diagnostic/hlds/qport_session_capture_dry_run_release_boundary_summary.md` |

The boundary proves only dry-run safety and policy wiring. It does not prove qport/session bytes, packet capture, runtime networking, netchan, real client behavior, or HLDS-compatible client behavior.

## Policy Decision

| Decision field | Value | Meaning |
| --- | ---: | --- |
| `capture_implementation_allowed_next` | `1` | A future prompt may add only a disabled-by-default no-client capture shell/harness with policy gates. |
| `capture_runtime_allowed_now` | `0` | This prompt and the current boundary still cannot run capture. |
| `socket_open_allowed_now` | `0` | No socket may be opened now. |
| `public_socket_allowed_now` | `0` | Public exposure remains forbidden. |
| `lan_socket_allowed_now` | `0` | LAN exposure remains forbidden. |
| `real_client_allowed_now` | `0` | Real client and Steam use remain forbidden. |
| `compatibility_claim_expansion_allowed_now` | `0` | Compatibility claims remain diagnostic-only. |

`capture_implementation_allowed_next=1` is narrow. It permits the next prompt to add a no-client, disabled-by-default implementation shell that is still policy-gated and safe by default. It does not permit capture execution, public or LAN sockets, real clients, Steam, connect/post-connect/signon/netchan runtime, qport byte-evidence promotion, or real compatibility claims.

## Blockers Still Present

- Qport/session byte evidence remains insufficient.
- Qport width, endian, order, placement, and UDP source-port relation remain unknown.
- Address-scoped challenge remains a diagnostic prerequisite only, not real netchan proof.
- Real netchan sequence/ack and reliable/unreliable byte contracts remain unresolved.
- Post-connect and signon serverinfo byte evidence remains insufficient.
- Capture execution is still disallowed.
- Socket opening is still disallowed now.
- Public/LAN exposure and real clients remain forbidden.

## Minimum Scope For Future Implementation Prompt

Any future implementation prompt must be limited to:

- disabled-by-default behavior
- explicit diagnostic-only flag
- no capture execution by default
- no socket open by default
- no public or LAN exposure
- no real client or Steam
- no connect/post-connect/signon runtime
- no netchan, auth, resource/baseline, reliable/unreliable, or admission startup
- required offline fixture validator
- required capture policy gate
- required dry-run validator
- required wrapper plan/validate success
- loopback-only guard if a socket capability is introduced later
- structured artifact-only summaries
- deterministic timeout and cleanup policy
- safe preview only
- no qport/session evidence promotion
- no compatibility claim expansion

The implementation shell may define names, guards, summaries, and rejected paths. It must not make capture available without a later explicit policy prompt that changes the relevant gates.

## Future Implementation Gate Matrix

| Gate | Required state |
| --- | --- |
| Disabled by default | Required |
| Explicit diagnostic flag | Required |
| Offline fixture validator | Required and passing |
| Capture policy gate | Required and still authoritative |
| Dry-run validator | Required and passing |
| Wrapper validation | Required and passing |
| No real client | Required |
| No public socket | Required |
| No LAN socket | Required |
| No non-loopback socket | Required if sockets are ever introduced |
| No capture execution unless separately allowed | Required |
| No qport evidence promotion | Required |
| No address-scoped challenge real netchan proof | Required |
| No connect/post-connect/signon runtime | Required |
| No netchan runtime | Required |
| No auth/resources/admission | Required |
| Bounded timeout | Required for any future active path |
| Deterministic cleanup | Required for any future active path |
| Artifact schema complete | Required |

## Allowed And Forbidden Actions

| Action | Allowed in this prompt | Allowed in next implementation prompt | Allowed eventually after separate policy | Notes |
| --- | --- | --- | --- | --- |
| Edit docs | yes | yes | yes | Report and policy docs only here. |
| Edit wrapper safety docs | yes | yes | yes | Wrapper behavior can be hardened without capture. |
| Add disabled-by-default capture shell | no | yes | yes | Shell only; no default execution. |
| Open loopback socket | no | no | yes | Requires later explicit policy and loopback-only guard. |
| Send synthetic qport/session datagram | no | no | yes | Requires later explicit capture policy. |
| Execute capture | no | no | yes | Requires separate policy change and proof scope. |
| Public socket | no | no | no | Remains forbidden. |
| LAN socket | no | no | no | Remains forbidden. |
| Real client | no | no | no | Remains forbidden. |
| Steam | no | no | no | Remains forbidden. |
| Connect path | no | no | no | Remains out of scope. |
| Post-connect serverinfo | no | no | no | Remains out of scope. |
| Signon serverinfo | no | no | no | Remains out of scope. |
| Netchan runtime | no | no | no | Remains out of scope. |
| Real compatibility claim | no | no | no | Diagnostic-only claim remains required. |

## Risk Review

| Risk | Severity | Required guard |
| --- | --- | --- |
| Accidental socket open | critical | Default disabled, no socket open allowed now, future loopback-only guard. |
| Accidental capture execution | critical | Capture execution denied until separate policy changes the gate. |
| Real client overreach | critical | Real client and Steam flags rejected. |
| Public/LAN exposure | critical | Public/LAN gates remain hard-deny. |
| Qport byte evidence overclaim | high | `byte_level_qport_session_evidence_sufficient=0` remains required. |
| Address-scoped challenge overclaim | high | Cannot be promoted to real netchan proof. |
| Connect/post-connect/signon creep | high | Runtime stage fields must remain zero. |
| Netchan/auth/resources/admission creep | high | No runtime startup allowed. |
| Fixture/policy drift | high | Offline fixture validator and JSON validation required. |
| Wrapper bypass | high | Wrapper validation required before any implementation shell. |
| Stale docs or summaries | medium | Stable docs and artifacts must be updated with any path or policy change. |
| Normal host behavior changed | critical | Shell must remain disabled by default and preserve normal behavior. |

## Recommended Next Prompt

`HL-CL-20260504-312-dedicated-goldsrc-hlds-qport-session-no-client-capture-shell-disabled-by-default`

This next prompt may add only a disabled-by-default, diagnostic-only no-client capture shell with hard policy gates. It must still not execute capture, open sockets, invoke real clients, run runtime stages, or expand compatibility claims.
