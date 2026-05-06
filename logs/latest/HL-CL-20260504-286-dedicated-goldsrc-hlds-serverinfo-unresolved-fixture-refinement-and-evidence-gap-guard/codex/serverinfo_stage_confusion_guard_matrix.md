# Serverinfo Stage-Confusion Guard Matrix

Prompt: HL-CL-20260504-286-dedicated-goldsrc-hlds-serverinfo-unresolved-fixture-refinement-and-evidence-gap-guard

| Attempted confusion | Guard field | Expected result | Runtime proof |
| --- | --- | --- | --- |
| Use connectionless query/info bytes as post-connect serverinfo | connectionless_query_not_post_connect | reject | gate_connectionless_query_promoted_to_post_connect: pass |
| Use connectionless query/info bytes as signon-time serverinfo | connectionless_query_not_signon | reject | gate_connectionless_query_promoted_to_signon: pass |
| Use diagnostic preview text as real byte-level wire evidence | diagnostic_preview_not_byte_evidence | reject | gate_diagnostic_preview_promoted_to_real_wire: pass |
| Build real post-connect serverinfo from unresolved fixture | unresolved_real_stage_not_buildable | reject | gate_unresolved_post_connect_build_attempt: pass |
| Build real signon-time serverinfo from unresolved fixture | unresolved_real_stage_not_buildable | reject | gate_unresolved_signon_build_attempt: pass |
| Mark unresolved fixture as real compatible | real compatibility claim count | reject | gate_real_compatibility_claim_escalation: pass |
| Enable byte-level post-connect/signon builder while evidence is insufficient | byte_level_evidence_required_for_real_stage | reject | gate_byte_level_builder_blocked: pass |
| Invoke real client as part of guard proof | no_real_client_gate_passed | reject if attempted | gate_no_real_client_used: pass |
| Open public socket through guard mode | public_socket_opened | reject before socket open | gate_public_socket_blocked: pass |

## Current Evidence Boundary

- connectionless query/info: byte-level local candidate exists, stage-limited.
- diagnostic post-connect serverinfo: diagnostic preview only, not byte-level real wire.
- post-connect real serverinfo: unresolved, no sufficient local byte evidence.
- signon-time serverinfo: unresolved, no sufficient local byte evidence.

## Safe Next Action

Proceed only with a connectionless query/info byte-level builder/parser. Do not promote post-connect or signon serverinfo until local byte-level fixtures and field/opcode/encoding evidence exist.
