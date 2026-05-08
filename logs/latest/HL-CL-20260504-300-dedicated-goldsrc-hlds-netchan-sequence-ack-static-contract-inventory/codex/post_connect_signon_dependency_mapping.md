@"
# Post-Connect/Signon Dependency Mapping

Source: docs/diagnostic/hlds/netchan_sequence_ack_static_contract_inventory.md

Rows covered:
1. post-connect serverinfo evidence
2. signon-time serverinfo evidence
3. resource/model/sound/event baselines
4. client spawn/put-in-server
5. reject/disconnect messaging

Every row remains blocked by missing sequence/ack or reliable/unreliable envelope evidence. Challenge, session, userinfo, and query/info diagnostics remain useful prerequisites but are not real netchan proof and must not be promoted to post-connect or signon compatibility evidence.
