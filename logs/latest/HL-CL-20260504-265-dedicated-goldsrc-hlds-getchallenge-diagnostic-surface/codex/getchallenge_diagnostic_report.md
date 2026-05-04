# HLDS getchallenge diagnostic report

Prompt: HL-CL-20260504-265-dedicated-goldsrc-hlds-getchallenge-diagnostic-surface

## Claim
compatibility_claim_level: diagnostic-connectionless-getchallenge-only; no real Steam Half-Life or HLDS-compatible client compatibility claimed

This is a bounded diagnostic bridge. It recognizes a local diagnostic GoldSrc/HLDS-style connectionless marker (FF FF FF FF) plus the getchallenge command and emits a deterministic challenge-shaped summary. It does not implement real connect, auth, signon, netchan, Steam validation, or public socket service.

## Implementation

Changed source files:
- include/app/launch_options.h
- src/app/launch_options.cpp
- src/app/host_application.cpp
- include/game_api/hl_server_module.h
- src/game_api/hl_server_module.cpp

Launch options added:
- --hlds-getchallenge-diagnostic-surface
- --hlds-getchallenge-diagnostic-probe
- --hlds-getchallenge-diagnostic-probe-scenario <happy|gate_bad_marker|gate_wrong_command>

Dependency behavior: enabling probe auto-enables surface. The diagnostic is independent from the synthetic resume lifecycle acceptance gate.

## Packet shapes

happy input: FF FF FF FF getchallenge 00
response shape: FF FF FF FF challenge 265042650 00

gate_bad_marker input: missing/malformed connectionless marker carrying getchallenge text.

gate_wrong_command input: FF FF FF FF status 00

## Proof summaries

happy_summary: logs\latest\HL-CL-20260504-265-dedicated-goldsrc-hlds-getchallenge-diagnostic-surface\runtime\hlhost_20260504_151922_650_pid28204__codex-dedicated-goldsrc-hlds-getchallenge-diagnostic-happy_summary.log
bad_marker_summary: logs\latest\HL-CL-20260504-265-dedicated-goldsrc-hlds-getchallenge-diagnostic-surface\runtime\hlhost_20260504_152035_228_pid39568__codex-dedicated-goldsrc-hlds-getchallenge-diagnostic-gate-bad-marker_summary.log
wrong_command_summary: logs\latest\HL-CL-20260504-265-dedicated-goldsrc-hlds-getchallenge-diagnostic-surface\runtime\hlhost_20260504_152144_589_pid29612__codex-dedicated-goldsrc-hlds-getchallenge-diagnostic-gate-wrong-command_summary.log

happy_probe_line:
[2026-05-04 15:20:34.971] [info][summary] hlds_getchallenge_diagnostic_probe: enabled=1, mode=dedicated, scenario=happy, accepted=1, rejected=0, lastRejectReason=<none>, compatibility_claim_level=diagnostic-connectionless-getchallenge-only, diagnostic_only=1, connectionless_marker_seen=1, connectionless_marker_valid=1, command_raw=getchallenge, command_normalized=getchallenge, getchallenge_detected=1, challenge_generated=1, challenge_response_ready=1, challenge_value=265042650, response_shape=connectionless-text:challenge <deterministic>, response_bytes_or_text_safe_preview=FF FF FF FF challenge 265042650 00, remote_address_source=diagnostic-loopback-input, bounded_loopback=1, public_socket_opened=0, auth=none-diagnostic-only, signon=not-started, gameplay_transport=no, detail=diagnostic-only HLDS-style getchallenge request accepted; real client compatibility not claimed

bad_marker_probe_line:
[2026-05-04 15:21:44.367] [info][summary] hlds_getchallenge_diagnostic_probe: enabled=1, mode=dedicated, scenario=gate_bad_marker, accepted=0, rejected=1, lastRejectReason=bad_connectionless_marker, compatibility_claim_level=diagnostic-connectionless-getchallenge-only, diagnostic_only=1, connectionless_marker_seen=1, connectionless_marker_valid=0, command_raw=<none>, command_normalized=<none>, getchallenge_detected=0, challenge_generated=0, challenge_response_ready=0, challenge_value=<none>, response_shape=connectionless-text:challenge, response_bytes_or_text_safe_preview=<none>, remote_address_source=diagnostic-loopback-input, bounded_loopback=1, public_socket_opened=0, auth=none-diagnostic-only, signon=not-started, gameplay_transport=no, detail=diagnostic getchallenge rejected before command parse: bad connectionless marker

wrong_command_probe_line:
[2026-05-04 15:23:00.078] [info][summary] hlds_getchallenge_diagnostic_probe: enabled=1, mode=dedicated, scenario=gate_wrong_command, accepted=0, rejected=1, lastRejectReason=unsupported_connectionless_command, compatibility_claim_level=diagnostic-connectionless-getchallenge-only, diagnostic_only=1, connectionless_marker_seen=1, connectionless_marker_valid=1, command_raw=status, command_normalized=status, getchallenge_detected=0, challenge_generated=0, challenge_response_ready=0, challenge_value=<none>, response_shape=connectionless-text:challenge, response_bytes_or_text_safe_preview=<none>, remote_address_source=diagnostic-loopback-input, bounded_loopback=1, public_socket_opened=0, auth=none-diagnostic-only, signon=not-started, gameplay_transport=no, detail=diagnostic getchallenge rejected: unsupported connectionless command

## Build and runtime

Build command: & 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' build32\host\hlhost.vcxproj /p:Configuration=Debug /p:Platform=Win32 /m
Build result: pass, 0 errors, existing warnings C4127 observed in src/game_api/hl_server_module.cpp around legacy constant-condition sites.

Runtime proofs: happy, gate_bad_marker, gate_wrong_command all exited 0.
public_socket_opened: 0
bounded_loopback: 1

## Remaining work before real client attempts

- persistent challenge table and expiry/address binding
- real HLDS connect packet grammar and protocol/version reject path
- userinfo parser
- serverinfo/signon byte contract
- reliable/unreliable channel and netchan sequencing
- resource/baseline inventory
- real client smoke harness without compatibility overclaim
