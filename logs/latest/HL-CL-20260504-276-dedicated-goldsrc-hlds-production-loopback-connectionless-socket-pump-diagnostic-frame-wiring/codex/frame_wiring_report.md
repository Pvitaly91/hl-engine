# HL-CL-20260504-276 Diagnostic Frame Wiring Report

## Status

status: pass
prompt_id: HL-CL-20260504-276-dedicated-goldsrc-hlds-production-loopback-connectionless-socket-pump-diagnostic-frame-wiring
branch: codex/HL-CL-20260401-081-target-runtime-completion-state
pre_change_head: d3691e7122a7e87a85552f2ed8efa707443d8822
source_final_commit: 5794bf6ea59319f28313b6d14827b3f6eeccc23e
compatibility_claim_level: diagnostic-bounded-frame-pump-wiring-only; no real Steam Half-Life or HLDS-compatible client compatibility claimed

## Scope

This prompt adds explicit diagnostic-only bounded frame-pump wiring for the production-style loopback connectionless socket pump introduced in prompt 273 and registered in prompt 275. The wiring is disabled by default and only runs when the new diagnostic frame-wiring probe or enable flag is provided.

This does not implement real HLDS compatibility, real Steam client compatibility, production public networking, Steam authentication, netchan, reliable channels, signon state, resource/model/sound/event baselines, or client admission.

## Launch Flags Added

- --hlds-production-loopback-connectionless-socket-pump-diagnostic-frame-wiring
- --hlds-production-loopback-connectionless-socket-pump-diagnostic-frame-wiring-probe
- --hlds-production-loopback-connectionless-socket-pump-diagnostic-frame-wiring-probe-scenario <happy|gate_disabled_by_default|gate_registration_required|gate_non_loopback_bind_denied|gate_public_socket_blocked|gate_frame_budget_enforced|gate_shutdown_cleanup|gate_repeated_start_stop|gate_bad_marker|gate_connect_without_cached_challenge|gate_wrong_protocol|gate_unsafe_userinfo>

The probe flag enables lifecycle registration as a dependency, then runs only the requested bounded diagnostic scenario. Registration-only mode remains socket-free.

## Frame Target

selected_integration_point: FinalizeServerBootstrapStep
frame_target: diagnostic bounded frame step adjacent to lifecycle registration
registered_component_name: hlds_production_loopback_connectionless_socket_pump_lifecycle_registration
registered_lifecycle_phase: FinalizeServerBootstrapStep
cleanup_owner: HlServerModule::EngineShimState diagnostic lifecycle registration
cleanup_registered: 1

The existing host/server frame loop is not changed. The new frame step is a deterministic proof surface adjacent to the registered lifecycle component and runs only in explicit diagnostic frame-wiring mode.

## Pump Policy

- frame_pump_disabled_by_default: 1
- frame_pump_max_datagrams_per_frame: 1
- frame_pump_budget_enforced: 1
- bind_policy: loopback_only_explicit_diagnostic
- bind_address_effective: 127.0.0.1
- public_socket_opened: 0
- loopback_udp_socket_opened in happy: 1
- sockets_closed: 1
- normal_host_behavior_changed: 0

Non-loopback and public bind requests are rejected before socket open. No background thread or unbounded loop is introduced.

## Runtime Proof Results

| Proof | Result | Key evidence |
| --- | --- | --- |
| happy | pass | accepted=1, frame_pump_wired=1, socket_pump_steps=2, datagrams=2/2, responses=2, sockets_closed=1 |
| gate_disabled_by_default | pass | rejected=1, lastRejectReason=diagnostic_frame_pump_disabled, socket_open_attempted=0 |
| gate_registration_required | pass | rejected=1, lastRejectReason=lifecycle_registration_required, registration_performed=0 |
| gate_non_loopback_bind_denied | pass | rejected=1, lastRejectReason=non_loopback_bind_denied, socket_open_attempted=0 |
| gate_public_socket_blocked | pass | rejected=1, lastRejectReason=public_socket_blocked, public_socket_opened=0 |
| gate_frame_budget_enforced | pass | datagrams_queued=3, processed_first_frame=1, left_after_first_frame=2, budget=1 |
| gate_shutdown_cleanup | pass | shutdown_cleanup_performed=1, sockets_closed=1, socket_pump_started_after_cleanup=0 |
| gate_repeated_start_stop | pass | cycles=2, repeated_start_stop_passed=1, sockets_closed=1 |
| gate_bad_marker | pass | rejected=1, lastRejectReason=bad_connectionless_marker, connect_ready=0, serverinfo_ready=0 |
| gate_connect_without_cached_challenge | pass | rejected=1, lastRejectReason=missing_cached_challenge, challenge_cache_hit=0 |
| gate_wrong_protocol | pass | rejected=1, lastRejectReason=unsupported_protocol_version, protocol_version_accepted=0 |
| gate_unsafe_userinfo | pass | rejected=1, lastRejectReason=unsafe_userinfo_value, userinfo_policy_passed=0 |

## Safety Gates Preserved

- disabled_by_default_preserved: true
- loopback_only_policy_preserved: true
- public_socket_opened: 0
- prompt273_gates_preserved: true
- prompt275_registration_guarantees_preserved: true
- steam_auth_not_implemented: 1
- netchan_not_started: 1
- reliable_channel_not_started: 1
- resource_baselines_not_sent: 1
- signon_state_not_entered: 1
- client_not_put_in_server: 1

## Build

MSBuild Rebuild of build32/host/hlhost.vcxproj Debug Win32 succeeded with 2 C4127 warnings and 0 errors. The C4127 warnings are in existing diagnostic conditional blocks in src/game_api/hl_server_module.cpp.

## Remaining Work Before Real Client Smoke

- production policy review
- durable challenge cache for real endpoints
- final protocol/version compatibility policy
- production userinfo policy
- real serverinfo wire format validation
- netchan sequencing/ack
- reliable/unreliable channel setup
- resource/model/sound/event baselines
- signon state machine
- client spawn / put-in-server path
- Steam auth or explicit no-auth LAN diagnostic mode

## Recommended Next Prompt

recommended_next_prompt_id: HL-CL-20260504-277-dedicated-goldsrc-hlds-production-loopback-connectionless-socket-pump-localhost-client-smoke-harness
recommended_next_task: add a diagnostic localhost client smoke harness after policy review while preserving no auth netchan signon baselines or client admission claims
