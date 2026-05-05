# Future Real-Client Smoke Design

Prompt: HL-CL-20260504-278-dedicated-goldsrc-hlds-production-loopback-connectionless-socket-pump-real-client-smoke-planning-policy-review

Compatibility claim level: real-client-smoke-planning-only; no real Steam Half-Life or HLDS-compatible client compatibility claimed

This is a design for a future prompt. It was not executed here.

## Design goal

The smallest later real-client smoke should only answer whether a explicitly configured local Steam Half-Life client can reach the loopback-only diagnostic server far enough to produce an expected, bounded failure report. It must not claim real HLDS compatibility and must not attempt gameplay admission.

## Required launch policy for a future prompt

A future smoke surface should require all of the following before any socket open or process invocation:

- Explicit real-client-smoke launch flag.
- Explicit path to a local client binary or launcher contract.
- Explicit future-prompt permission to invoke Steam/client process.
- Explicit loopback-only or separately reviewed local-only bind policy.
- Explicit public-network denial gate.
- Explicit bounded run duration and deterministic timeout.
- Explicit safe-preview byte/log budget.
- Explicit expected failure stage field.
- Explicit cleanup owner for server socket, client socket/process handle, logs, and timeout cancellation.

## Hard fail conditions

The future harness must fail before opening sockets or invoking a process when:

- Client binary path is absent.
- Process invocation permission is absent.
- Steam/client invocation is requested by an unapproved prompt.
- Public bind or non-loopback target is requested.
- Packet capture budget is unbounded.
- Run duration is unbounded.
- Output path is outside prompt-local logs.
- The compatibility claim is anything stronger than diagnostic/planning until the missing systems have proof.

## Proposed future execution phases

1. Preflight policy check: validate explicit opt-in, local-only networking, bounded timeout, safe-preview budget, and expected failure stage.
2. Server setup: start only the diagnostic loopback socket pump path that has prior gates. Public bind remains blocked before open.
3. Client launch: invoke the explicitly configured local client only if the future prompt grants permission.
4. Observation window: capture bounded logs, process exit/timeout, and safe packet previews without raw sensitive values.
5. Expected failure classification: record whether failure occurred at serverinfo, auth, netchan, signon, resources, admission, timeout, or process launch.
6. Cleanup: terminate process if started, close sockets, flush prompt-local logs, and record cleanup counters.

## Expected failure stage field

The future summary should include `expected_failure_stage` and `observed_failure_stage`. For the current codebase, the expected stage remains `serverinfo_wire_format_or_post_connect_netchan_signon_transition` until real serverinfo and channel/sign-on planning is advanced.

## Non-goals

The future smoke design does not include public networking, matchmaking, Steam auth implementation, netchan implementation, signon implementation, resource/model/sound/event baselines, put-in-server admission, or gameplay transport. Those remain separate implementation areas with separate proof requirements.

## Next implementation prompt

Recommended next prompt: HL-CL-20260504-279-dedicated-goldsrc-hlds-real-serverinfo-wire-format-inventory-compatibility-skeleton

Recommended next task: inventory and validate the real GoldSrc/HLDS serverinfo response wire format in a diagnostic-only compatibility skeleton before any real client invocation.
