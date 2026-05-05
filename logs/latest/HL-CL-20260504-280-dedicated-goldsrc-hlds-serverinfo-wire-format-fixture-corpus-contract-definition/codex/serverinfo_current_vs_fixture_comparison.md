# Current Diagnostic Serverinfo Vs Fixture Corpus

PROMPT-ID: HL-CL-20260504-280-dedicated-goldsrc-hlds-serverinfo-wire-format-fixture-corpus-contract-definition

## Mapping

| Current evidence | Fixture mapping | Status |
| --- | --- | --- |
| Prompt 267 diagnostic serverinfo skeleton | diagnostic_post_connect_serverinfo_current.json | Covered as diagnostic_current, not real client compatible. |
| Prompt 277 client-observed diagnostic serverinfo response | diagnostic_post_connect_serverinfo_current.json | Covered as diagnostic_current, not real client compatible. |
| Prompt 279 byte-level connectionless query/info candidate | connectionless_query_info_candidate.json | Covered as local_code evidence for connectionless_query only. |
| Prompt 279 unresolved post-connect real serverinfo contract | post_connect_real_serverinfo_unresolved.json | Preserved as unresolved. |
| Prompt 279 unresolved signon-time serverinfo contract | signon_time_serverinfo_unresolved.json | Preserved as unresolved. |

## Concept Separation

Connectionless query/info serverinfo is treated as a byte-level candidate backed by local code evidence. It is not promoted to post-connect or signon-time compatibility evidence.

Current diagnostic post-connect serverinfo remains a text-shaped diagnostic response used by prompts 267 and 277. It is useful for diagnostic lifecycle and localhost harness proofs, but it is not a real GoldSrc/HLDS serverinfo wire contract.

Signon-time serverinfo remains unresolved in the local evidence reviewed for prompt 279 and this prompt. The fixture corpus records this gap explicitly instead of inventing a byte contract.

Unknown or unresolved serverinfo-like shapes are represented through unresolved fixtures and invalid/mutated fixtures. Future parser work should reject or report these shapes deterministically.

## Fields Already Covered

- Diagnostic response command/tag text in the current post-connect skeleton.
- Diagnostic protocol field for current prompt 267/277 flow.
- Diagnostic hostname, map, game directory, maxplayers, and slot fields.
- Local-code-backed connectionless query/info marker/header and opcode/tag candidate.
- Safe preview metadata for all fixtures.

## Synthetic-Only Fields

- diagnostic_post_connect_serverinfo_current.slot
- diagnostic_post_connect_serverinfo_current.command tag
- diagnostic text key/value shape used by the current diagnostic response
- invalid mutation payloads used to reserve parser gate behavior

## Unresolved Fields

- real post-connect marker/header
- real post-connect opcode/tag
- real post-connect field order
- real post-connect string encoding and termination
- real post-connect numeric widths and endian policy
- real post-connect response length policy
- signon-time message id/opcode
- signon-time bitstream/message framing
- signon-time netchan and reliable channel dependencies
- signon-time baseline/resource dependencies
- real client acceptance behavior for either post-connect or signon-time serverinfo

## Expected Failure If A Real Client Is Attempted Now

expected_failure_stage_if_real_client_attempted_now: serverinfo_wire_format_or_post_connect_netchan_signon_transition

A real client smoke attempt is still blocked. The current diagnostic response shape can prove diagnostic request/response flow, but it cannot prove real client acceptance, signon transition, netchan setup, resource baseline delivery, Steam authentication behavior, or client admission.

## Future Parser Gate Inputs

The invalid fixtures should become the first diagnostic parser gates in the next probe:

- missing required field
- field order mismatch
- unsafe string
- overlong response
- unknown opcode or marker

## Next Builder/Parser Decision

The corpus is sufficient for a diagnostic fixture contract validator/probe. It is not sufficient for a real-client-compatible serverinfo builder. A later builder/parser must remain contract-backed and diagnostic-only until byte-level post-connect or signon-time evidence exists locally.
