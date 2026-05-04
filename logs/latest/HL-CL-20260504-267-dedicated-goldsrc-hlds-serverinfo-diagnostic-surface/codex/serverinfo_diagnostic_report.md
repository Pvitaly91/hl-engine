# HLDS Serverinfo Diagnostic Surface

PROMPT-ID: HL-CL-20260504-267-dedicated-goldsrc-hlds-serverinfo-diagnostic-surface

## Scope

Implemented a bounded diagnostic-only `hlds_serverinfo_diagnostic` surface/probe after the existing diagnostic getchallenge and connect parser surfaces. This is a local loopback serverinfo-shaped skeleton and does not emit a real HLDS serverinfo admission flow.

Compatibility claim level: diagnostic-serverinfo-skeleton-only; no real Steam Half-Life or HLDS-compatible client compatibility claimed.

## Launch Options

- `--hlds-serverinfo-diagnostic-surface`
- `--hlds-serverinfo-diagnostic-probe`
- `--hlds-serverinfo-diagnostic-probe-scenario <happy|gate_missing_connect|gate_wrong_protocol|gate_missing_serverinfo_field>`

Probe enables surface. The serverinfo happy path internally reuses deterministic getchallenge and connect diagnostic helpers; it does not depend on synthetic resume lifecycle surfaces.

## Diagnostic Packet/Response Shape

Prerequisites are diagnostic getchallenge + diagnostic connect only. The happy serverinfo response safe preview is:

`FF FF FF FF serverinfo protocol=48 hostname=HLengine_Diagnostic_Server map=crossfire game=valve maxplayers=4 slot=diagnostic-client-slot-1 00`

Diagnostic fields emitted:

- protocol/version-shaped field: `48`
- hostname-shaped field: `HLengine Diagnostic Server`
- map name-shaped field: `crossfire`
- game directory-shaped field: `valve`
- maxplayers-shaped field: `4`
- player slot placeholder: `diagnostic-client-slot-1`

## Proof Results

- happy: pass, accepted=1, serverinfo_response_ready=1
- gate_missing_connect: pass, rejected=1, lastRejectReason=missing_connect_diagnostic
- gate_wrong_protocol: pass, rejected=1, lastRejectReason=unsupported_protocol_version
- gate_missing_serverinfo_field: pass, rejected=1, lastRejectReason=missing_serverinfo_field

## Safety Boundaries

- public_socket_opened=0
- bounded_loopback=1
- steam_auth_not_implemented=1
- netchan_not_started=1
- reliable_channel_not_started=1
- resource_baselines_not_sent=1
- signon_state_not_entered=1
- client_not_put_in_server=1

## Remaining Before Real Client Attempts

- real connectionless socket receive path
- real challenge cache keyed by remote address
- protocol/version compatibility policy
- userinfo validation policy
- real serverinfo wire emission
- netchan sequencing/ack
- resource/model/sound/event baselines
- signon state machine
- client spawn / put-in-server path
- Steam auth or explicit no-auth LAN diagnostic mode
