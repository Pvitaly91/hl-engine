# Localhost Client Smoke Harness Report

PROMPT-ID: HL-CL-20260504-277-dedicated-goldsrc-hlds-production-loopback-connectionless-socket-pump-localhost-client-smoke-harness

## Scope

This prompt adds a diagnostic localhost UDP client smoke harness around the prompt 276 explicit diagnostic frame-wired production-style loopback socket pump. It does not use a real Steam Half-Life client and does not claim HLDS-compatible client compatibility.

Compatibility claim level: diagnostic-localhost-client-smoke-harness-only; no real Steam Half-Life or HLDS-compatible client compatibility claimed

## Source Changes

- include/app/launch_options.h: adds the disabled-by-default harness enable/probe/scenario launch fields.
- src/app/launch_options.cpp: parses the harness flags and makes the probe enable only the harness and lifecycle registration dependency.
- src/app/host_application.cpp: propagates harness launch fields into HlServerModuleInitOptions and logs them in startup config.
- include/game_api/hl_server_module.h: adds harness summary/probe summary fields for client socket, real-client, frame-pump, and safety reporting.
- src/game_api/hl_server_module.cpp: adds the diagnostic harness runner, summary serialization, validation gates, and lifecycle call in FinalizeServerBootstrapStep adjacent to prompt 276 frame wiring.

## Launch Flags Added

- --hlds-production-loopback-connectionless-socket-pump-localhost-client-smoke-harness
- --hlds-production-loopback-connectionless-socket-pump-localhost-client-smoke-harness-probe
- --hlds-production-loopback-connectionless-socket-pump-localhost-client-smoke-harness-probe-scenario <happy|gate_disabled_by_default|gate_no_real_client_used|gate_non_loopback_client_denied|gate_public_socket_blocked|gate_client_timeout_bounded|gate_wrong_challenge|gate_wrong_protocol|gate_unsafe_userinfo|gate_shutdown_cleanup>

## Harness Behavior

The happy probe starts the registered prompt 276 diagnostic frame-wired pump, opens a loopback-only diagnostic client socket, sends getchallenge, pumps bounded frames until the diagnostic challenge response is observed, sends connect with the observed challenge and safe userinfo, then pumps bounded frames until the diagnostic serverinfo response is observed.

The harness verifies diagnostic response shape only. It does not validate real GoldSrc wire compatibility and does not start auth, netchan, signon, baselines, or client admission.

## Safety Results

- disabled by default: 1
- diagnostic client used in happy path: 1
- real Steam client used: 0
- real client binary invoked: 0
- public socket opened: 0
- client public socket opened: 0
- server public socket opened: 0
- client socket loopback only: 1
- sockets closed: 1
- normal host behavior changed: 0

## Runtime Proofs

| Scenario | Result | Accepted | Rejected | Last reject reason |
| --- | --- | --- | --- | --- |
| happy | pass | 1 | 0 | <none> |
| gate_disabled_by_default | pass | 0 | 1 | localhost_client_smoke_harness_disabled |
| gate_no_real_client_used | pass | 1 | 0 | <none> |
| gate_non_loopback_client_denied | pass | 0 | 1 | non_loopback_client_denied |
| gate_public_socket_blocked | pass | 0 | 1 | public_socket_blocked |
| gate_client_timeout_bounded | pass | 0 | 1 | client_timeout_bounded |
| gate_wrong_challenge | pass | 0 | 1 | challenge_value_mismatch |
| gate_wrong_protocol | pass | 0 | 1 | unsupported_protocol_version |
| gate_unsafe_userinfo | pass | 0 | 1 | unsafe_userinfo_value |
| gate_shutdown_cleanup | pass | 1 | 0 | <none> |

## Why This Is Still Not Real Compatibility

The harness uses only an internal diagnostic localhost UDP client. It does not launch Steam, does not use a Half-Life client binary, does not perform authentication, does not create netchan or reliable channel state, does not enter signon state, does not emit resource/model/sound/event baselines, and does not put any client in server.

## Remaining Work Before Real Client Smoke

- Explicit production policy review.
- Durable challenge cache for real endpoints.
- Final protocol/version compatibility policy.
- Production userinfo policy.
- Real serverinfo wire format validation against known GoldSrc expectations.
- Netchan sequencing/ack.
- Reliable/unreliable channel setup.
- Resource/model/sound/event baselines.
- Signon state machine.
- Client spawn and put-in-server path.
- Steam auth or explicit no-auth LAN diagnostic mode.
- Separate real-client-smoke planning prompt before running any real client.
