# Real-Client Smoke Policy Review

Prompt: HL-CL-20260504-278-dedicated-goldsrc-hlds-production-loopback-connectionless-socket-pump-real-client-smoke-planning-policy-review

Compatibility claim level: real-client-smoke-planning-only; no real Steam Half-Life or HLDS-compatible client compatibility claimed

## Policy boundary

This prompt is planning-only. It did not launch Steam, invoke any Half-Life client binary, depend on proprietary client files, open public sockets, bind non-loopback addresses, run runtime proofs, or change source. The artifacts are based on repo inspection and existing prompt summaries from the diagnostic sequence.

A future real-client smoke attempt is not allowed now. It must be requested by a separate prompt, explicitly opt in to any process invocation, use only local/loopback or otherwise separately reviewed local-only networking, and fail closed before opening sockets or starting a client if required policy fields are absent.

## What prompt 277 proved

Prompt 277 proved that a diagnostic localhost UDP client harness can talk to the explicitly enabled diagnostic frame-wired socket pump. The harness sent getchallenge-shaped and connect-shaped datagrams over localhost, received challenge-shaped and serverinfo-shaped diagnostic responses, observed prompt 276 frame-pump counters, and closed client/server sockets. It also proved that no real Steam client was used, no real client binary was invoked, public sockets stayed closed, and auth/netchan/reliable/signon/resource/admission paths stayed inactive.

This is useful because it exercises the diagnostic pump through actual localhost UDP client/server sockets under bounded frames. It is not sufficient for a real client because the response shape is diagnostic, not validated as a real HLDS wire contract, and the post-connection systems a real client expects are deliberately absent.

## Forbidden claims

The following claims remain forbidden:

- Real Steam Half-Life client compatibility.
- HLDS-compatible client admission.
- Production public networking readiness.
- Steam authentication support.
- Netchan sequencing/ack support.
- Reliable/unreliable channel support.
- Signon state support.
- Resource/model/sound/event baseline delivery.
- Put-in-server or gameplay admission support.
- Public server discovery, matchmaking, or internet exposure support.

## Future real-client smoke policy

Any future real-client smoke must satisfy these conditions before execution:

- It is requested by a dedicated prompt that explicitly permits real client invocation.
- A local client binary path is provided outside the prompt artifacts and validated before use.
- Steam/client process invocation is separately approved by that future prompt.
- Public network bind and public socket policies hard-fail before open.
- The server remains loopback-only or local-only under a separately reviewed bind policy.
- Runtime is bounded by a deterministic timeout and bounded packet/log preview budget.
- Expected failure stage is recorded before the run and confirmed after the run.
- Cleanup always closes server sockets, client sockets, and any spawned process handles.
- No persistent mutation outside prompt-local logs is allowed.
- The summary must preserve no-auth/no-netchan/no-signon/no-resource/no-admission fields unless those layers have separate prior proof.

## Expected failure if attempted now

A real client attempted now is expected to fail at or before `serverinfo_wire_format_or_post_connect_netchan_signon_transition`. The current diagnostic serverinfo response is not validated as a real GoldSrc/HLDS wire response. Even if a real client accepted the connectionless shape, the next required systems are missing: Steam auth or a reviewed no-auth LAN diagnostic mode, netchan sequencing/ack, reliable/unreliable channels, signon state, resource/model/sound/event baselines, and put-in-server admission.

## Why no real client should run next

Running a real client now would mostly produce an ambiguous timeout or rejection. The safest next implementation is to reduce the first concrete compatibility gap with a diagnostic-only real serverinfo wire-format inventory and compatibility skeleton. That gives a measurable contract before any process invocation, keeps public networking blocked, and still avoids Steam/client binaries.
