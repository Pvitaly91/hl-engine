# Qport/session Capture Execution Readiness Plan

This plan is policy-only and read-only. It creates no capture runtime and grants no socket, datagram, real-client, netchan, qport-evidence, or compatibility permissions.

## Plan Requirements

- Readiness gate remains disabled by default.
- Explicit diagnostic readiness-gate probe mode is required.
- Prompt 338 readiness policy review must load and pass.
- Prompt 336 execution implementation CI drift gate must pass.
- Prompt 337 implementation wrapper/CI release boundary must validate.
- Prompt 333 final implementation gate must pass.
- Execution and runtime skeleton CI gates must pass.
- Offline fixture validator, capture policy gate, dry-run validator, and wrapper validation must pass.
- Timeout and cleanup policy requirements must be defined before any future execution readiness expansion.
- Artifact schema lock requirements must be defined before any future execution readiness expansion.
- Separate socket and datagram policy reviews remain required before any open/send/receive behavior.

## Explicit Non-Permissions

- No capture execution.
- No capture runtime.
- No packet capture.
- No socket open.
- No loopback socket open.
- No public or LAN socket behavior.
- No datagram send or receive.
- No real client or Steam invocation.
- No connect/post-connect/signon runtime path.
- No netchan runtime.
- No qport/session byte evidence promotion.
- No compatibility claim expansion.

## Pass Criteria

- `readiness_gate_passed=1`
- `readiness_plan_created=1`
- `readiness_plan_validated=1`
- all prerequisite gate loaded/invoked/passed fields remain `1`
- all allowed-now and executed/opened/sent/received/runtime/client/evidence/compatibility fields remain `0`
- `address_scoped_challenge_reusable_as_diagnostic_prerequisite=1`
- `address_scoped_challenge_reusable_as_real_netchan_proof=0`
