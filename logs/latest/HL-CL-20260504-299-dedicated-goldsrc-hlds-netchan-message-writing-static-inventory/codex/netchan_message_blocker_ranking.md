# Netchan Message Blocker Ranking

Source: docs/diagnostic/hlds/netchan_message_writing_static_inventory.md

1. missing_netchan_sequence_ack_contract
2. missing_reliable_unreliable_envelope_contract
3. missing_post_connect_serverinfo_byte_field_contract
4. missing_signon_time_serverinfo_byte_field_contract
5. missing_message_writer_policy_for_strings_numeric_fields
6. missing_baseline_resource_message_contract
7. missing_reject_disconnect_byte_contract
8. unknown_central_buffer_overflow_policy
9. unknown_real_endian_policy
10. real_client_capture_forbidden

Top blocker: missing_netchan_sequence_ack_contract

None of these blockers requires a real client, public socket, LAN socket, or runtime execution for the next static inventory step.
