# Qport/Session Blocker Ranking

| Rank | Blocker | Severity | Reason | Smallest safe next task | Runtime needed? | Real client needed? | Public/LAN needed? |
| ---: | --- | --- | --- | --- | --- | --- | --- |
| 1 | missing qport/session byte contract | critical | no qport symbol, field width, endian, or order found | no-client diagnostic capture design | no for design | no | no |
| 2 | missing qport-to-netchan binding | critical | no channel object or remote binding exists | no-client diagnostic capture design | no for design | no | no |
| 3 | missing reliable/unreliable envelope byte contract | critical | prompt 301 found only diagnostic blockers and labels | envelope fixture only after session evidence | no | no | no |
| 4 | missing sequence/ack byte contract | critical | prompt 300 found no sufficient sequence/ack contract | sequence/ack fixture only after session/envelope evidence | no | no | no |
| 5 | missing remote address/session key policy | high | endpoint strings are diagnostic-only | no-client diagnostic capture design | no for design | no | no |
| 6 | missing post-connect serverinfo byte contract | high | unresolved fixtures remain blocked | post-connect evidence acquisition later | later | no now | no |
| 7 | missing signon serverinfo byte contract | high | pseudo signon is not real signon | signon evidence acquisition later | later | no now | no |
| 8 | missing reject/disconnect session binding | medium | rejection summaries do not define post-connect session envelope | reject/disconnect static inventory later | no | no | no |
| 9 | real client capture forbidden | policy blocker | prompt forbids Steam/client execution | separate explicit policy boundary only | yes later | yes later | no |
| 10 | public/LAN exposure forbidden | policy blocker | prompt forbids public/LAN sockets | keep blocked | no | no | no |

blockers_count: 10
top_blocker: missing_qport_session_byte_contract

Recommended next prompt:
HL-CL-20260504-303-dedicated-goldsrc-hlds-qport-session-no-client-diagnostic-capture-design
