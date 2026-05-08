# Netchan Sequence/Ack Symbol Inventory

Source: docs/diagnostic/hlds/netchan_sequence_ack_static_contract_inventory.md

Relevant source evidence found:
- 
etchan_not_started: diagnostic blocker/report field.
- eliable_channel_not_started: diagnostic blocker/report field.
- esource_baselines_not_sent: diagnostic blocker/report field.
- emote_address_source: diagnostic provenance only.
- client_port: loopback diagnostic UDP port variable, not GoldSrc qport.
- DedicatedSignonWiremapSequenceId / sequence_id: pseudo signon diagnostic sequence id, partial byte-shaped evidence for pseudo reports only.
- parsed_sequence_ids: diagnostic pseudo signon report parsing.
- MSG_ONE_UNRELIABLE: message destination label, not an unreliable packet envelope.
- unrelated local sequence fields in map logic and spawn trace.

Relevant source evidence not found:
- incoming_sequence
- outgoing_sequence
- incoming_acknowledged
- reliable_ack
- incoming_reliable_sequence
- outgoing_reliable_sequence
- qport
- fragment
- packet_loss
- split packet

Conclusion: local sequence/ack evidence is not sufficient for a netchan contract.
