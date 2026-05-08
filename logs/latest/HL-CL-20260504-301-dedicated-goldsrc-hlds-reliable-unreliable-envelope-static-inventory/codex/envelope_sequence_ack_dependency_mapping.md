# Envelope Sequence/Ack Dependency Mapping

| Dependency | Local evidence | Field known? | Order known? | Width known? | Byte-level known? | Safe next action |
| --- | --- | --- | --- | --- | --- | --- |
| Outgoing sequence | no real netchan symbol | no | no | no | no | do not define until envelope/session evidence exists |
| Incoming sequence | no real netchan symbol | no | no | no | no | do not define until client command evidence exists |
| Incoming ack | no exact ack source symbol | no | no | no | no | keep unknown |
| Reliable sequence | no `reliable_sequence` symbol | no | no | no | no | inventory qport/session before fixture feasibility |
| Reliable ack | no `reliable_ack` symbol | no | no | no | no | inventory qport/session before fixture feasibility |
| Fragment counters | no fragment/split symbols | no | no | no | no | leave out of initial contract |
| Qport/client port | diagnostic `client_port`, no `qport` | partial local port only | no | local int only | no | qport/session binding static inventory |
| Challenge/session binding | challenge/cache/userinfo diagnostics | partial conceptual | no | no | no | bridge diagnostics without claiming real netchan proof |

sequence_ack_dependency_rows_count: 8
