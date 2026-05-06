# Query/Info Next Step Decision

Prompt: HL-CL-20260504-290-dedicated-goldsrc-hlds-connectionless-query-info-byte-level-loopback-policy-review

## Recommended Next Prompt

$nextPrompt

## Recommended Task

implement a stricter opt-in diagnostic query-client smoke harness that exercises only connectionless query/info over loopback with the byte-level response, while keeping public/LAN exposure, real clients, connect, post-connect, signon, auth, netchan, resources, and admission blocked

## Decision Rationale

The next safest task is a stricter policy-gated loopback query-client smoke harness because prompts 287, 288, and 289 already proved the byte-level connectionless query/info fixture, in-memory diagnostic path, and localhost loopback response. The proof chain also already includes explicit public-socket and non-loopback bind denial, stage-confusion rejection, unresolved post-connect/signon rejection, real compatibility claim rejection, timeout bounding, and shutdown cleanup.

A public/LAN blocker prompt is not the next smallest step because prompt 289 already proved the blocker gates and this review keeps public/LAN exposure disallowed. A fixture-contract drift prompt is not the next smallest step because the validator and evidence-gap guard are still required in the active query/info path and no fixture drift was found during this report-only review. A post-connect/signon gap regression prompt is not the next smallest step because prompt 286 established the source-level evidence-gap guard and prompts 287-289 continued to pass the relevant stage-confusion gates.

The recommended prompt must remain opt-in, bounded, loopback-only, connectionless-query-only, and diagnostic-only. It must not run Steam, invoke a real client binary, expose public or LAN sockets, enter connect/post-connect/signon, start auth/netchan/resources, or put a client in server.
