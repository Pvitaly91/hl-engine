@"
# Netchan Sequence/Ack Unknowns And Forbidden Assumptions

Source: docs/diagnostic/hlds/netchan_sequence_ack_static_contract_inventory.md

Unknowns count: 12

Unknowns:
- exact sequence field width
- exact ack field width
- reliable bit/flag location
- qport handling
- fragment handling
- channel init order
- first post-connect server packet envelope
- first signon serverinfo envelope
- reliable/unreliable split
- overflow behavior
- timeout/disconnect behavior
- reject message envelope

These must not be invented from query/info, callback observation, or pseudo signon diagnostics. Safe resolution requires later envelope inventory, fixture/evidence ingestion policy, or explicit capture policy prompts.
