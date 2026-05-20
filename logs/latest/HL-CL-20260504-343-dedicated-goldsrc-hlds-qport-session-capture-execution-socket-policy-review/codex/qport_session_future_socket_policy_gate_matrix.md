# Future Socket Policy Gate Matrix

Prompt: HL-CL-20260504-343-dedicated-goldsrc-hlds-qport-session-capture-execution-socket-policy-review

| Gate | Required | Pass condition |
| --- | --- | --- |
| Disabled by default | Yes | Gate is inert unless explicitly enabled. |
| Readiness CI drift gate | Yes | Prompt 341 readiness drift gate remains passed. |
| Execution implementation CI drift gate | Yes | Prompt 336 execution implementation drift gate remains passed. |
| Readiness boundary | Yes | Prompt 342 readiness wrapper/CI boundary remains closed. |
| Wrapper validation | Yes | Wrapper plan/validate remains passed. |
| Timeout/cleanup policy | Yes | `timeout_cleanup_policy_defined=1`. |
| Artifact schema lock | Yes | `artifact_schema_lock_defined=1`. |
| Loopback-only policy | Yes | Future discussion is constrained to loopback-only policy. |
| Public/LAN denied | Yes | Public and LAN allowed/opened fields remain zero. |
| Datagram send denied | Yes | `datagram_send_allowed_now=0` and `datagram_sent=0`. |
| Datagram receive denied | Yes | `datagram_receive_allowed_now=0` and `datagram_received=0`. |
| Capture execution denied | Yes | `capture_execution_allowed_now=0` and `capture_executed=0`. |
| Capture runtime denied | Yes | `capture_runtime_allowed_now=0` and `capture_runtime_executed=0`. |
| Real client denied | Yes | Real client allowed/used fields remain zero. |
| Connect/post-connect/signon denied | Yes | Connect, post-connect, and signon markers remain zero. |
| Netchan runtime denied | Yes | `netchan_runtime_allowed_now=0` and `netchan_runtime_started=0`. |
| Qport evidence promotion denied | Yes | Qport byte-evidence and promotion fields remain zero. |
| Compatibility expansion denied | Yes | Compatibility expansion fields remain zero. |
| No socket open in policy gate | Yes | `socket_open_attempted=0` and `loopback_udp_socket_opened=0`. |

