# Relation To Current Diagnostics

| Source | Reusable as diagnostic prerequisite? | Reusable as real proof? | Required policy guard | Risk |
| --- | --- | --- | --- | --- |
| Prompt 269 address-scoped challenge cache | yes | no | `address_scoped_challenge_reusable_as_real_netchan_proof=0` | Challenge cache can be mistaken for session proof. |
| Prompt 270 userinfo validation | yes | no | Userinfo remains pre-admission diagnostic data. | Userinfo can be mistaken for admission. |
| Prompt 271 lifecycle acceptance | yes as policy history | no | Lifecycle remains connectionless diagnostic. | Lifecycle can be mistaken for netchan start. |
| Prompt 296 query/info boundary | no for qport/session | no | Query/info stage is forbidden for qport/session fixtures. | Query/info can be over-promoted. |
| Prompt 302 qport/session inventory | yes as gap evidence | no | `byte_level_qport_session_evidence_sufficient=0` | Static inventory can be mistaken for byte evidence. |
| Prompt 303 no-client design | yes as design input | no | `capture_executed=0` | Design can be mistaken for implementation. |

## Preserved Assertions

| Assertion | Value |
| --- | --- |
| Query/info boundary closed | 1 |
| Qport/session byte evidence sufficient | 0 |
| Address-scoped challenge reusable as diagnostic prerequisite | 1 |
| Address-scoped challenge reusable as real netchan proof | 0 |
| Capture allowed now | 0 |
| Real client capture allowed now | 0 |
| Netchan runtime started | 0 |
