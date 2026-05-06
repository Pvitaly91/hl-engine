# Query/Info Loopback Policy Review

Prompt: HL-CL-20260504-290-dedicated-goldsrc-hlds-connectionless-query-info-byte-level-loopback-policy-review

Compatibility claim level: diagnostic-query-info-loopback-policy-review-only; no real Steam Half-Life or HLDS-compatible client compatibility claimed

Status: pass, report-only.

## Scope

This policy review covers the diagnostic connectionless query/info byte-level loopback response path proven by prompts 287, 288, and 289. It does not add launch options, source behavior, fixture data, sockets, client runtime behavior, or normal host changes.

The reviewed path remains diagnostic-only and connectionless-query-only. It is not a real HLDS query server, not a public socket policy, not LAN exposure, not a real query-client compatibility claim, not post-connect serverinfo, and not signon-time serverinfo.

## Policy Decisions

| Area | Decision | Basis |
| --- | --- | --- |
| Diagnostic status | Keep diagnostic-only and disabled by default. | Prompts 287-289 all require explicit probes and carry no real compatibility claim. |
| Fixture scope | Keep selected fixture $selectedFixture only. | Prompt 287 builder/parser and prompt 288 path integration selected only the connectionless query fixture. |
| Stage separation | Query/info cannot be used as post-connect or signon serverinfo. | Prompt 286 evidence-gap guard and prompts 287-289 stage-confusion gates passed. |
| Real post-connect/signon builders | Keep blocked. | Prompt 285 found no sufficient post-connect or signon-time byte evidence; prompt 286 enforces non-buildability. |
| Real client smoke | Keep blocked. | No Steam or real client binary has been invoked; no real query client evidence exists. |
| Public socket exposure | Keep blocked. | Prompt 289 proved public socket gate rejection, but public exposure has not had a policy acceptance prompt. |
| LAN exposure | Keep blocked. | Prompt 289 proved non-loopback bind denial; LAN threat model and compatibility have not been reviewed. |
| Future query-client smoke | Allow only as a new opt-in, bounded, loopback-only diagnostic prompt. | The loopback byte response is proven, so the next safe step is a stricter diagnostic query-client harness that does not enter connect/post-connect/signon. |
| Future public exposure | Requires a separate policy prompt and is not allowed now. | Current proof only covers disabled-by-default localhost diagnostics and explicit public-socket rejection. |

## Boundary Rules For Next Work

- Connectionless query/info byte response is fixture-backed and diagnostic-only.
- Connectionless query/info is not post-connect serverinfo.
- Connectionless query/info is not signon-time serverinfo.
- Real post-connect and signon byte builders remain blocked.
- Real client smoke remains blocked.
- Public socket exposure remains blocked.
- LAN exposure remains blocked unless separately reviewed.
- Any future query-client smoke must be opt-in, bounded, loopback-only, and connectionless-query-only unless a separate policy prompt changes that boundary.
- Any future public socket exposure must require a separate policy prompt and is not allowed now.

## Decision

The next safest prompt is $nextPrompt.

Reason: prompt 289 already proves a diagnostic localhost loopback query/info response and the public/non-loopback blockers. The next smallest useful step is a stricter policy-gated query-client smoke harness that exercises only the connectionless query/info path over loopback. It should not use Steam, real client binaries, public sockets, LAN sockets, connect, post-connect, signon, auth, netchan, resources, or admission.
