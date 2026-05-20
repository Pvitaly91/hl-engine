# Socket Policy Risk Review

Prompt: HL-CL-20260504-343-dedicated-goldsrc-hlds-qport-session-capture-execution-socket-policy-review

| Risk | Control |
| --- | --- |
| Socket policy treated as socket permission | Keep `future_socket_open_implementation_allowed_next=0` and `socket_open_allowed_now=0`. |
| Accidental socket open | Keep `socket_open_attempted=0`. |
| Accidental loopback socket open | Keep `loopback_udp_socket_opened=0`. |
| Public/LAN exposure | Keep public and LAN allowed/opened fields at zero. |
| Datagram send/receive creep | Keep datagram send/receive allowed and executed fields at zero. |
| Capture execution creep | Keep `capture_execution_allowed_now=0` and `capture_executed=0`. |
| Capture runtime creep | Keep `capture_runtime_allowed_now=0` and `capture_runtime_executed=0`. |
| Timeout/cleanup gap | Keep `future_timeout_cleanup_policy_required=1`. |
| Artifact schema drift | Keep `future_artifact_schema_lock_required=1`. |
| Readiness drift bypass | Require readiness CI drift gate before future socket policy work. |
| Wrapper bypass | Require wrapper validation before future socket policy work. |
| Real client overreach | Keep real client allowed/used fields at zero. |
| Connect/post-connect/signon creep | Keep all connect and signon runtime markers at zero. |
| Netchan runtime creep | Keep `netchan_runtime_started=0`. |
| Qport evidence overclaim | Keep byte-evidence sufficiency fields at zero. |
| Address-scoped challenge overclaim | Keep `address_scoped_challenge_reusable_as_real_netchan_proof=0`. |
| Compatibility overclaim | Keep compatibility expansion fields at zero. |

