# Contract-Backed Localhost Smoke-Swap Report

Prompt: HL-CL-20260504-284-dedicated-goldsrc-hlds-serverinfo-contract-backed-diagnostic-localhost-smoke-swap

Status: pass

Compatibility claim level: diagnostic-contract-backed-localhost-smoke-swap-only; no real Steam Half-Life or HLDS-compatible client compatibility claimed

## Scope

This prompt adds an explicit diagnostic localhost smoke-swap mode. It keeps normal host behavior unchanged and only runs when --hlds-serverinfo-contract-backed-diagnostic-localhost-smoke-swap-probe is supplied.

The smoke-swap composes the prompt 277 diagnostic localhost UDP client harness with the prompt 283 contract-backed diagnostic serverinfo path. The response source is the checked-in fixture-backed path: fixture validator, builder/parser, and roundtrip validation. It remains diagnostic-preview-only: byte_level_builder_complete=0 and real_wire_builder_complete=0.

## Happy Proof

The happy scenario started the loopback-only frame-wired pump, created a diagnostic localhost UDP client socket, sent getchallenge/connect-shaped datagrams, invoked the contract-backed serverinfo path, and received a serverinfo-shaped diagnostic preview.

Key happy values:

- contract_backed_response_selected=1
- client_serverinfo_contract_backed=1
- selected_fixture_id=diagnostic_post_connect_serverinfo_current
- fixture_validator_invoked=1
- fixture_validation_passed=1
- builder_parser_invoked=1
- build_succeeded=1
- parse_succeeded=1
- roundtrip_validation_passed=1
- client_serverinfo_response_received=1
- public_socket_opened=0
- real_steam_client_used=0
- real_client_binary_invoked=0

## Runtime Gates

All required prompt 284 gates passed:
- happy: pass
- gate_disabled_by_default: pass
- gate_smoke_harness_required: pass
- gate_contract_path_required: pass
- gate_validator_required: pass
- gate_builder_roundtrip_required: pass
- gate_client_timeout_bounded: pass
- gate_wrong_challenge: pass
- gate_wrong_protocol: pass
- gate_unsafe_userinfo: pass
- gate_unresolved_fixture_rejected: pass
- gate_invalid_fixture_rejected: pass
- gate_real_compatibility_claim_rejected: pass
- gate_no_real_client_used: pass
- gate_non_loopback_client_denied: pass
- gate_public_socket_blocked: pass
- gate_shutdown_cleanup: pass

## Boundaries Preserved

No real Steam Half-Life client was launched. No real client binary was invoked. Public sockets were blocked. Non-loopback client policy was rejected before socket open. Auth, netchan, reliable channels, signon, resources, baselines, and client admission remain unimplemented/not-started in this diagnostic path.

## Remaining Work

Before any real client smoke, the project still needs byte-level post-connect/signon serverinfo evidence, a real wire builder, final protocol/version compatibility policy, production userinfo policy, netchan sequencing/ack, reliable/unreliable channel setup, resource/model/sound/event baselines, signon state machine, client spawn/put-in-server, and Steam auth or explicit no-auth LAN diagnostic mode.
