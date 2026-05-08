# Netchan Static Inventory

Source: docs/diagnostic/hlds/netchan_message_writing_static_inventory.md

Found netchan-related source symbols are blocker/report fields rather than a real netchan implementation:
- 
etchan_not_started
- eliable_channel_not_started
- esource_baselines_not_sent
- message destination labels such as MSG_ONE_UNRELIABLE
- pseudo diagnostic sequence/counter fields in dedicated signon wiremap helpers

Not found:
- real netchan channel object
- 
et_chan implementation
- real sequence/ack encoder
- reliable channel envelope
- unreliable datagram envelope
- fragment/split packet handling
- packet loss/flow implementation
- client/server channel send/receive functions

Counts:
- netchan_symbols_found_count=246
- reliable_symbols_found_count=46
- unreliable_symbols_found_count=2

Conclusion: netchan sequence/ack remains the top blocker before any post-connect or signon-time serverinfo field contract can be safely designed.
