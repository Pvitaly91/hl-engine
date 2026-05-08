# Reliable/Unreliable Envelope Candidate Matrix

| Candidate envelope | Local symbols/evidence | Byte-level evidence sufficient? | Sequence dependency | Ack dependency | Reliable/unreliable dependency | Qport/session dependency | Stage | Blocker | Smallest safe next task |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Reliable server message envelope | `reliable_channel_not_started`, signon descriptors | no | unknown | unknown | channel absent | unknown | netchan/signon | no real reliable envelope | qport/session binding inventory |
| Unreliable server datagram envelope | `MSG_ONE_UNRELIABLE` label | no | unknown | unknown | packet placement absent | unknown | netchan/unreliable | label is not packet bytes | qport/session binding inventory |
| Client command envelope | connect/userinfo summaries | no | unknown client sequence | unknown server ack | client channel absent | unknown | post-connect/netchan | no command envelope | qport/session binding inventory |
| Ack-only envelope | none | no | unknown | unknown | unknown | unknown | netchan | no ack field | sequence/ack evidence later |
| Split/fragment envelope | none | no | unknown | unknown | fragment absent | unknown | netchan | fragment format absent | fragment policy later |
| Signon message envelope | diagnostic signon envelope summaries | no | unknown | unknown | likely reliable but unproven | unknown | signon | pseudo signon only | field contract after envelope evidence |
| Post-connect serverinfo envelope | preview/unresolved serverinfo artifacts | no | unknown | unknown | unknown | challenge/session unknown | post-connect | first server packet envelope absent | evidence acquisition later |
| Baseline/resource envelope | `resource_baselines_not_sent` | no | unknown | unknown | likely reliable but unproven | unknown | baseline/resource | baseline envelope absent | baseline/resource inventory later |
| Reject/disconnect envelope | rejection summaries and connectionless text helpers | no | unknown | unknown | stage-dependent | challenge/session dependent | reject/disconnect | post-connect reject bytes absent | reject/disconnect inventory later |

Counts:
- reliable_envelope_candidates_count: 1
- unreliable_envelope_candidates_count: 1
- reliable_unreliable_contract_candidate_count: 9
