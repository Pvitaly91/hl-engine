# Qport To Netchan Dependency Mapping

| Future dependency | Current local evidence | Qport/session dependency | Unresolved fields | Forbidden assumptions | Next safe action |
| --- | --- | --- | --- | --- | --- |
| Netchan channel init | `netchan_not_started` blocker only | needs remote address/qport/session binding | channel object, qport, sequence seed | do not invent channel init | no-client diagnostic capture design |
| Sequence/ack initialization | prompt 300 found no byte contract | likely depends on channel/session identity | initial sequence, ack, reliable bit | do not reuse pseudo sequence IDs | capture design or evidence fixture |
| Reliable/unreliable envelope selection | prompt 301 found no envelope | likely depends on established channel | reliable flag, channel placement | do not use callback destination labels as packets | envelope fixture only after evidence |
| Remote address binding | endpoint strings and `remote_address_source` summaries | address must bind to channel peer | exact key policy | do not treat diagnostic text as netchan key | qport/session capture design |
| NAT/client-port tolerance | no exact `NAT` or port remap evidence | qport may exist for this, but absent locally | qport field, remap rules | do not infer from UDP source port | external evidence/capture policy later |
| Connect acceptance | challenge/userinfo diagnostics | prerequisite before channel transition | connect-to-channel state | do not claim admission | no-client design later |
| Post-connect serverinfo delivery | unresolved fixtures | depends on channel/envelope after connect | first packet envelope | do not promote query/info or preview | post-connect evidence later |
| Signon serverinfo delivery | pseudo signon surfaces only | depends on channel/session/signon state | signon envelope and state | do not reuse pseudo signon | signon evidence later |
| Reject/disconnect messaging | rejection summaries | may depend on endpoint/session after connect | envelope and reason framing | do not infer post-connect reject bytes | reject/disconnect inventory later |
| Client admission / put-in-server | blocker field only | likely after signon/session completion | slot/index/admission rules | do not admit client | separate admission inventory |

qport_to_netchan_dependency_rows_count: 10
