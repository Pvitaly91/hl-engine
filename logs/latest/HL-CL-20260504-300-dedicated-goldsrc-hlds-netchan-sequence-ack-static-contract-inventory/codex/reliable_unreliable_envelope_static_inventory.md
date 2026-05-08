@"
# Reliable/Unreliable Envelope Static Inventory

Source: docs/diagnostic/hlds/netchan_sequence_ack_static_contract_inventory.md

Envelope concepts checked:
- reliable server-to-client messages: not found beyond eliable_channel_not_started.
- unreliable server-to-client datagrams: only MSG_ONE_UNRELIABLE destination label found.
- client-to-server commands: no real channel found.
- ack-only packets: not found.
- split/fragmented packets: not found.
- message queue flush: no netchan queue found.
- channel init: not found beyond blockers.
- channel reset: not found.
- channel close/disconnect: no real envelope found.

Conclusion: envelope_candidates_count=9, but none provide enough byte-level evidence. The next prompt should inventory envelope concepts before any sequence/ack fixture contract.
