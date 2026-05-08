# Qport/Session Candidate Matrix

| Candidate field/binding | Likely role | Local symbols/evidence | Byte-level evidence sufficient? | Field width known? | Endian known? | Order known? | Stage | Required for netchan? | Required for post-connect serverinfo? | Required for signon-time serverinfo? | Blocker | Smallest safe next task |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| qport field | client port remap/NAT aid | no `qport` symbol | no | no | no | no | connect/netchan | likely | likely | likely | absent field | no-client diagnostic capture design |
| client source port | observed UDP source port | `client_port`, sockaddr source port | no | local int only | n/a | n/a | connectionless diagnostics | maybe | maybe | maybe | not qport | capture design or evidence fixture later |
| remote address | endpoint identity/provenance | `remote_address_source`, sockaddr, loopback endpoint strings | no | text only | n/a | n/a | connectionless diagnostics | yes for real channel | yes | yes | no netchan remote binding | qport/session capture design |
| endpoint key | challenge cache lookup key | `AF_INET/127.0.0.1:<port>` | no | text only | n/a | n/a | getchallenge/connect bridge | diagnostic prerequisite only | maybe | maybe | not real session key | explicit diagnostic contract if later needed |
| address-scoped challenge key | bind challenge to endpoint | `challenge_cache_key=remote_loopback_endpoint` | no | text label only | n/a | n/a | getchallenge/connect bridge | prerequisite only | maybe | maybe | no real proof | keep as prerequisite guard |
| session id | durable network identity | logger `session_id`, pseudo signon descriptors | no | string only | n/a | n/a | logging/pseudo signon | unknown | unknown | unknown | not network session | do not reuse |
| challenge value | connect authorization token | diagnostic challenge value string | connectionless diagnostic only | no real width | n/a | diagnostic text order only | getchallenge/connect | prerequisite only | maybe | maybe | not netchan proof | fixture ingestion/capture later |
| challenge TTL | challenge expiration | TTL ticks=3 | no | local int only | n/a | n/a | diagnostic cache | no direct proof | no | no | diagnostic tick model | keep guard |
| challenge one-shot/replay state | prevent reused challenge | consumed flag, replay gate | no | booleans only | n/a | n/a | diagnostic cache/connect | prerequisite only | maybe | maybe | not real proof | keep guard |
| userinfo identity | name/model/rate fields | userinfo policy and parser | diagnostic text only | bytes limit known for policy | n/a | diagnostic text order only | connect | prerequisite only | maybe | maybe | not admission proof | keep policy guard |
| client slot/client index | future admission target | sparse client slot/player slot text | no | no | no | no | admission | maybe after netchan | maybe | yes | admission forbidden | separate admission inventory |
| netchan remote address binding | channel peer binding | `netchan_not_started` only | no | no | no | no | netchan | yes | yes | yes | no channel object | no-client capture design |
| netchan qport binding | qport tied to channel | not found | no | no | no | no | netchan | yes | yes | yes | absent | no-client capture design |
| connect-to-netchan transition binding | accepted connect creates channel/session | diagnostic connect readiness only | no | no | no | no | transition | yes | yes | yes | transition unknown | no-client capture design |
| reject/disconnect session binding | reject after failed session/connect | rejection summaries | no | no | no | no | reject/disconnect | maybe | maybe | maybe | envelope unknown | reject/disconnect inventory later |

qport_session_contract_candidate_count: 15
byte_level_qport_session_evidence_sufficient: 0
