# Channel Placement Inventory

| Message type | Channel placement | Local evidence | Byte-level status | Fixture-backed now? | Builder-backed now? | Forbidden assumptions | Next safe action |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Connectionless query/info | connectionless | prompts 287-296 | sufficient for diagnostic query/info only | yes | yes, diagnostic-only | not post-connect/signon | keep closed boundary |
| getchallenge | connectionless diagnostic | earlier datagram summaries | diagnostic only | partial | diagnostic only | not netchan | keep separate |
| connect request | connectionless diagnostic/pre-netchan | connect summaries | diagnostic only | partial | diagnostic only | not post-connect serverinfo | qport/session inventory |
| Post-connect serverinfo | unknown | unresolved fixtures/preview | insufficient | no | no | do not infer from query/info | envelope evidence later |
| Signon-time serverinfo | unknown signon/netchan | signon descriptor surfaces | insufficient | no | no | pseudo signon is not real bytes | envelope evidence later |
| Resource list | unknown | blocker fields | insufficient | no | no | do not invent reliable placement | baseline/resource inventory |
| Model baseline | unknown | baseline readiness fields | insufficient | no | no | no model baseline bytes | baseline/resource inventory |
| Sound baseline | unknown | baseline readiness fields | insufficient | no | no | no sound baseline bytes | baseline/resource inventory |
| Event baseline | unknown | baseline readiness fields | insufficient | no | no | no event baseline bytes | baseline/resource inventory |
| Reject/disconnect | connectionless or netchan depending stage, unknown | rejection summaries | insufficient for post-connect | no | no | connectionless reject does not prove post-connect envelope | reject/disconnect inventory |
| Client command/usercmd | unknown netchan | no usercmd envelope found | insufficient | no | no | do not invent client channel | qport/session inventory |
| Heartbeat/status query | unknown/not in this boundary | no focused proof | insufficient | no | no | do not add public query behavior | separate policy prompt |

channel_placement_rows_count: 12
