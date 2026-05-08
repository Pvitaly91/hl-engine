# Reliable/Unreliable Blocker Ranking

| Rank | Blocker | Severity | Reason | Smallest safe next task | Runtime needed? | Real client needed? | Public/LAN needed? |
| ---: | --- | --- | --- | --- | --- | --- | --- |
| 1 | missing reliable/unreliable envelope byte contract | critical | no real reliable/unreliable packet envelope found | qport/session binding static inventory | no | no | no |
| 2 | missing sequence/ack byte contract | critical | no outgoing/incoming sequence or ack contract exists | sequence/ack fixture contract only after envelope/session evidence | no | no | no |
| 3 | missing qport/session binding | high | diagnostic `client_port` exists, but no `qport` byte contract | qport/session binding static inventory | no | no | no |
| 4 | missing post-connect serverinfo envelope | high | unresolved post-connect fixtures are not buildable | post-connect envelope contract after channel evidence | no | no | no |
| 5 | missing signon serverinfo envelope | high | pseudo signon surfaces are not real signon bytes | signon envelope/field contract after channel evidence | no | no | no |
| 6 | missing baseline/resource envelope | high | baseline/resource sending remains blocked | baseline/resource envelope inventory | no | no | no |
| 7 | missing message writer policy | medium | diagnostic writers exist, but no real network sizebuf/overflow policy | message-writing policy fixture contract | no | no | no |
| 8 | missing reject/disconnect envelope | medium | rejection summaries do not define post-connect disconnect bytes | reject/disconnect static contract inventory | no | no | no |
| 9 | real client capture forbidden | policy blocker | prompt forbids Steam/client execution | separate explicit policy boundary only | yes later | yes later | no |
| 10 | public/LAN exposure forbidden | policy blocker | prompt forbids non-loopback/public/LAN sockets | keep blocked | no | no | no |

blockers_count: 10
top_blocker: missing_reliable_unreliable_envelope_byte_contract

Recommended next prompt:
HL-CL-20260504-302-dedicated-goldsrc-hlds-qport-session-binding-static-inventory
