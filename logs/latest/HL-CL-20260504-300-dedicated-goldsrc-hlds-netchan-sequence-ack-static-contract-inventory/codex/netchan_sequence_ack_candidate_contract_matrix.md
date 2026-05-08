@"
# Sequence/Ack Candidate Contract Matrix

Source: docs/diagnostic/hlds/netchan_sequence_ack_static_contract_inventory.md

Candidate rows covered: outgoing sequence, incoming sequence, incoming ack, reliable sequence, reliable ack, unreliable datagram, fragment state, qport/client port, remote address binding, challenge/session binding, signon state binding, reliable message buffer, unreliable message buffer.

Summary:
- No candidate has sufficient byte-level evidence for real netchan.
- No candidate has confirmed field width, endian, or ordering for real netchan.
- The only byte-shaped sequence candidate is diagnostic pseudo signon sequence_id, which is not safe for real netchan reuse.

sequence_ack_contract_candidate_count=13
byte_level_sequence_ack_evidence_sufficient=0

Safe next task: reliable/unreliable envelope static inventory.
