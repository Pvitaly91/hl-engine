# Address-Scoped Challenge Linkage Audit

| Linkage item | Local evidence | Reusable as diagnostic prerequisite? | Reusable as real netchan proof? | Stage-specific notes | Guard needed |
| --- | --- | --- | --- | --- | --- |
| Challenge cache key | `challenge_cache_key=remote_loopback_endpoint` | yes | no | Diagnostic endpoint key only. | Keep qport/session claim blocked. |
| Endpoint matching | `FindEndpoint(endpoint)` with `AF_INET/127.0.0.1:<port>` | yes | no | Uses observed loopback source endpoint. | Do not infer qport/NAT behavior. |
| Challenge value matching | `connect_challenge_value == endpoint_entry->value` | yes | no | Text token in diagnostic connect. | Do not infer real packet field width/order. |
| Wrong endpoint reuse | `gate_wrong_endpoint_reuse`, client B socket | yes | no | Diagnostic proof of endpoint mismatch. | Keep loopback-only and no public/LAN. |
| Expiration | TTL ticks=3, created tick=10 | yes | no | Synthetic tick model. | Do not infer real timeout. |
| One-shot/replay | `consumed`, `challenge_replay_detected` | yes | no | Diagnostic replay gate. | Do not infer real challenge lifecycle. |
| Connect readiness | challenge and userinfo diagnostics feed preview serverinfo | partial | no | Connectionless/connect diagnostic only. | Do not start netchan. |
| Userinfo policy | minimal diagnostic policy | yes | no | Validates text fields before diagnostic acceptance. | Do not infer real admission. |
| Lifecycle acceptance | prompt 271 artifacts | yes as policy history | no | Connectionless lifecycle only. | Keep post-connect/signon blocked. |
| Loopback pump/client smoke | prompts 273/276/277 artifacts | yes as loopback policy history | no | Earlier prompt socket proofs only; prompt 302 opened none. | Keep no runtime in this prompt. |

address_scoped_challenge_reusable_as_diagnostic_prerequisite: 1
address_scoped_challenge_reusable_as_real_netchan_proof: 0
