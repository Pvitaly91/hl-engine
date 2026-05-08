# Qport/Session Evidence Target Matrix

| Evidence target | Current local evidence | Sufficient now? | Required before fixture contract | Safe next action |
| --- | --- | --- | --- | --- |
| qport field presence | No `qport` symbol in prompt 302 static inventory. | no | Fixture or manifest evidence that qport exists or is absent. | Offline fixture manifest policy. |
| qport field width | No field found. | no | Exact width from fixture. | Offline fixture metadata field. |
| qport endian | No field found. | no | Explicit endian or text encoding. | Offline fixture metadata field. |
| qport field order | No field found. | no | Position relative to challenge/userinfo/connect tokens. | Fixture manifest and later byte fixture. |
| UDP source port relation | `client_port` is diagnostic source-port metadata. | no | Relation to qport marked same, distinct, or unknown. | Do not equate source port to qport. |
| remote address key | Diagnostic loopback endpoint strings. | diagnostic-only | Peer/session key policy. | Manifest policy with diagnostic prerequisite guard. |
| challenge linkage | Address-scoped challenge cache. | diagnostic prerequisite only | Explicit challenge/session transition rule. | Fixture manifest policy. |
| userinfo linkage | Diagnostic userinfo validation policy. | diagnostic prerequisite only | Explicit accepted fields carried into session descriptor. | Fixture manifest policy. |
| connect-to-netchan handoff | `netchan_not_started=1`. | no | Channel/session binding evidence. | Keep runtime blocked. |
| reject/disconnect binding | Summary reasons only. | no | Stage-specific envelope/session rule. | Later reject/disconnect inventory. |

## Fixture Contract Is Still Blocked

The target matrix keeps `byte_level_qport_session_evidence_sufficient=0`. A future fixture contract cannot pass while qport presence, width, endian, order, and qport-to-session linkage are unknown.
