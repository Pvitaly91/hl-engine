# Stage Separation And Blocker Preservation

| Boundary | Status |
| --- | --- |
| Query/info boundary remains closed | yes |
| Connectionless query/info is qport/session proof | no |
| Getchallenge/connect diagnostics are real netchan proof | no |
| Post-connect byte evidence sufficient | no |
| Signon-time byte evidence sufficient | no |
| Reliable/unreliable envelope byte evidence sufficient | no |
| Sequence/ack byte evidence sufficient | no |
| Real post-connect builder complete | no |
| Real signon builder complete | no |
| Real wire builder complete | no |
| Real client capture allowed now | no |
| Public/LAN exposure allowed now | no |
| Netchan runtime started | no |
| Normal host behavior changed | no |

Safety values:
- query_info_boundary_closed=1
- post_connect_byte_evidence_sufficient=0
- signon_time_byte_evidence_sufficient=0
- real_post_connect_builder_complete=0
- real_signon_builder_complete=0
- real_wire_builder_complete=0
- real_client_capture_allowed_now=0
- public_socket_opened=0
- lan_socket_opened=0
- loopback_udp_socket_opened=0
- connect_path_invoked=0
- post_connect_serverinfo_path_invoked=0
- signon_serverinfo_path_invoked=0
- netchan_runtime_started=0
- normal_host_behavior_changed=0
