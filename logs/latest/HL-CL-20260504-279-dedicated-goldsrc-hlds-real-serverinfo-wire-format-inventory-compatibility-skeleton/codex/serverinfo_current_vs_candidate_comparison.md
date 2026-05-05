# Current vs Candidate Serverinfo Comparison

Prompt: HL-CL-20260504-279-dedicated-goldsrc-hlds-real-serverinfo-wire-format-inventory-compatibility-skeleton

Compatibility claim level: diagnostic-real-serverinfo-wire-format-skeleton-only; no real Steam Half-Life or HLDS-compatible client compatibility claimed

## Compared Shapes

| Shape | Stage | Response shape | Current status |
| --- | --- | --- | --- |
| Prompt 267 diagnostic serverinfo | diagnostic connect-response | `FF FF FF FF serverinfo protocol=48 hostname=HLengine_Diagnostic_Server map=crossfire game=valve maxplayers=4 slot=diagnostic-client-slot-1 00` | Built as diagnostic text safe preview after diagnostic getchallenge/connect readiness. |
| Prompt 277 client-observed serverinfo | diagnostic connect-response over localhost client harness | Same diagnostic text preview as prompt 267 | Observed by a diagnostic localhost UDP client; not a real client. |
| Existing query/info candidate | connectionless query/info | `FF FF FF FF m address\0name\0map\0mod\0description\0 players maxplayers protocol d w 0 0 0` | Byte-level local builder/parser for query path only. |
| Real post-connect/signon serverinfo | connect-response or signon | unknown | No sufficient local contract found. |

## Fields Already Covered Conceptually

These fields are present in both the diagnostic text response and the existing query/info response candidate as concepts, though not with the same opcode, order, or encoding:

- protocol/version: diagnostic text `protocol=48`; query candidate byte `48`.
- hostname/server name: diagnostic text hostname; query candidate server name C string.
- map: diagnostic text map; query candidate map C string.
- game/mod directory: diagnostic text game; query candidate mod C string.
- maxplayers: diagnostic text maxplayers; query candidate max players byte.

`fields_already_covered_count=5`

## Missing or Unresolved Fields

- post-connect opcode or message id.
- signon-time serverinfo service/message id.
- exact post-connect field order.
- exact signon serverinfo payload order.
- real string encoding and termination policy for the needed stage.
- real numeric widths/endian policy for the needed stage.
- query response trailing byte semantics after protocol.
- response length limit accepted by a real client.
- real client expected serverinfo stage and transition point.
- fixture/capture evidence for client acceptance.
- reject/disconnect response shape if serverinfo cannot proceed.
- auth/netchan/signon transition contract after serverinfo.

`fields_missing_count=12`

## Synthetic-Only Fields

- Diagnostic text command token `serverinfo`.
- Key/value text field names such as `protocol=`, `hostname=`, `map=`, `game=`, `maxplayers=` in the diagnostic response.
- `slot=diagnostic-client-slot-1` placeholder.
- Diagnostic readiness fields and summary counters.

`synthetic_only_fields_count=4`

## Expected Failure If Current Response Reaches A Real Client

Expected failure stage: `serverinfo_wire_format_or_post_connect_netchan_signon_transition`

The current prompt 267/277 response is expected to fail because it is a diagnostic text response with no locally proven real HLDS client acceptance. Even if a real client tolerated the connectionless marker, the next required systems remain absent: Steam auth or reviewed no-auth LAN diagnostic mode, netchan sequencing/ack, reliable/unreliable channel setup, resource/model/sound/event baselines, signon state machine, and client admission.
