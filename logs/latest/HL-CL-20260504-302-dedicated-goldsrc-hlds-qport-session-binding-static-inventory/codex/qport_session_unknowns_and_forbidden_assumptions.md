# Qport/Session Unknowns And Forbidden Assumptions

| Unknown | Why unknown | Local evidence if any | Why it must not be invented | Safe way to resolve later |
| --- | --- | --- | --- | --- |
| qport field existence in real connect packet | no `qport` symbol or fixture bytes | prompt 300/302 static searches | invented qport would corrupt connect/channel contract | no-client diagnostic capture design or fixture ingestion policy |
| qport field width | no field found | none | width affects parser/builder correctness | evidence-backed fixture |
| qport endianness | no field found | none | byte order cannot be guessed | evidence-backed fixture |
| qport placement/order | no field found | none | token/field order affects connect parsing | capture/evidence plan |
| qport relationship to UDP source port | only `client_port` source-port diagnostics | local loopback bind result | UDP source port is not qport | qport/session capture design |
| qport relationship to NAT/port remap | no exact NAT/remap evidence | none | NAT behavior is protocol-sensitive | real/reference evidence under later policy |
| qport relationship to challenge value | challenge cache does not use qport | endpoint key + challenge string | qport may or may not participate | evidence-backed contract |
| qport relationship to userinfo | userinfo connect text has no qport | diagnostic userinfo policy | userinfo identity is not qport | evidence-backed contract |
| qport relationship to client slot | no admission path | blocker fields only | admission is forbidden and unknown | later admission inventory |
| session id existence | logger/pseudo session only | `session_id` noisy hits | app log session is not network session | capture/evidence plan |
| session id width/placement | no network session field | none | cannot encode unknown session | evidence-backed fixture |
| remote address key policy | endpoint string diagnostic only | `AF_INET/127.0.0.1:<port>` | real netchan key may differ | no-client capture design |
| connect endpoint to netchan address transition | no channel init | `netchan_not_started` | transition semantics affect all packets | no-client design before runtime |
| reject/disconnect session binding | summaries only | rejection text/reasons | stage-specific envelope unknown | reject/disconnect inventory later |

unknowns_count: 14
