@"
# Netchan Sequence/Ack Blocker Ranking

Source: docs/diagnostic/hlds/netchan_sequence_ack_static_contract_inventory.md

1. missing_reliable_unreliable_envelope_contract
2. missing_sequence_ack_byte_contract
3. missing_qport_session_binding
4. missing_signon_envelope
5. missing_post_connect_serverinfo_envelope
6. missing_baseline_resource_envelope
7. missing_message_writer_policy
8. real_client_capture_forbidden
9. public_lan_exposure_forbidden
10. real_compatibility_claim_forbidden

Top blocker: missing_reliable_unreliable_envelope_contract

No runtime, real client, or public/LAN exposure is needed for the next static inventory step.
