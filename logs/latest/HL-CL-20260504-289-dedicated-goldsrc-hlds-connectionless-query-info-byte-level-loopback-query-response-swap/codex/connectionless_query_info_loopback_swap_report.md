# Connectionless Query/Info Loopback Swap Report

Prompt: HL-CL-20260504-289-dedicated-goldsrc-hlds-connectionless-query-info-byte-level-loopback-query-response-swap

Status: pass

Compatibility claim: diagnostic-connectionless-query-info-byte-level-loopback-response-only; no real Steam Half-Life or HLDS-compatible client compatibility claimed

This is diagnostic-only. The loopback swap routes only a deterministic connectionless query/info datagram through the prompt 288 byte-level query/info path integration and sends the byte-level query/info response back over localhost. It is not post-connect serverinfo, not signon-time serverinfo, not a real HLDS query server, and not real Steam Half-Life client compatibility.

## Dependencies

- Fixture root: fixtures/diagnostic/hlds/serverinfo
- Selected fixture: connectionless_query_info_candidate
- Validator dependency: prompt 281 fixture contract validator, invoked before byte builder use
- Evidence-gap guard dependency: prompt 286 unresolved fixture evidence-gap guard
- Byte builder/parser dependency: prompt 287 connectionless query/info byte-level builder/parser
- Path integration dependency: prompt 288 connectionless query/info byte-level diagnostic path integration
- Loopback bind policy: localhost-only UDP sockets bound through BindLoopbackQuerySocket on 127.0.0.1; no public socket path is allowed

## Happy Path Evidence

- query_info_path_integration_invoked: 1
- query_info_request_datagram_received: 1
- query_info_response_datagram_sent: 1
- client_query_info_response_received: 1
- client_query_info_response_shape_valid: 1
- build_output_bytes: 67
- safe hex preview: FFFFFFFF6D3132372E302E302E313A3000484C656E67696E6520546573742053657276657200633061300076616C76650048616C662D4C696665000004306477000000
- safe text preview: ....m127.0.0.1:0|HLengine Test Server|c0a0|valve|Half-Life||.0dw|||

## Gate Matrix

``text

scenario                                   proof accepted rejected reason
--------                                   ----- -------- -------- ------                                              
happy                                      pass  1        0        <none>                                              
gate_disabled_by_default                   pass  0        1        connectionless_query_info_loopback_swap_disabled    
gate_query_info_path_required              pass  0        1        connectionless_query_info_path_required             
gate_validator_required                    pass  0        1        fixture_validator_required                          
gate_evidence_gap_guard_required           pass  0        1        serverinfo_evidence_gap_guard_required              
gate_builder_roundtrip_required            pass  0        1        connectionless_query_info_builder_roundtrip_required
gate_wrong_command_or_query                pass  0        1        unsupported_connectionless_query_command            
gate_bad_marker_or_header                  pass  0        1        connectionless_query_missing_marker_or_header       
gate_wrong_opcode_or_tag                   pass  0        1        connectionless_query_wrong_opcode_or_tag            
gate_missing_required_field                pass  0        1        connectionless_query_missing_required_field         
gate_unsafe_string                         pass  0        1        connectionless_query_unsafe_string                  
gate_overlong_response                     pass  0        1        connectionless_query_response_too_large             
gate_post_connect_stage_confusion_rejected pass  0        1        connectionless_query_not_post_connect_evidence      
gate_signon_stage_confusion_rejected       pass  0        1        connectionless_query_not_signon_evidence            
gate_unresolved_post_connect_rejected      pass  0        1        unresolved_post_connect_serverinfo_not_buildable    
gate_unresolved_signon_rejected            pass  0        1        unresolved_signon_serverinfo_not_buildable          
gate_real_compatibility_claim_rejected     pass  0        1        real_compatibility_claim_rejected                   
gate_no_real_client_used                   pass  1        0        <none>                                              
gate_non_loopback_bind_denied              pass  0        1        non_loopback_bind_denied                            
gate_public_socket_blocked                 pass  0        1        public_socket_blocked                               
gate_client_timeout_bounded                pass  0        1        client_timeout_bounded                              
gate_shutdown_cleanup                      pass  1        0        <none>                                              


``

## Boundaries Preserved

- no real Steam Half-Life client compatibility claimed
- no real client binary invoked
- public sockets not opened
- only loopback UDP sockets opened in runtime scenarios that require the diagnostic exchange
- query/info response is not post-connect serverinfo
- query/info response is not signon-time serverinfo
- real post-connect and signon builders remain blocked
- Steam auth, netchan, reliable channel, resources, signon, and put-in-server paths remain not started
