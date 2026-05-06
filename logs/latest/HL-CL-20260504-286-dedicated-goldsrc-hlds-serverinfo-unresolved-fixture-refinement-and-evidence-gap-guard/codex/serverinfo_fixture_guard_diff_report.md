# Serverinfo Fixture Guard Diff Report

Prompt: HL-CL-20260504-286-dedicated-goldsrc-hlds-serverinfo-unresolved-fixture-refinement-and-evidence-gap-guard

## Contract Updates

ixtures/diagnostic/hlds/serverinfo/contract/serverinfo_fixture_contract.schema.json

- Added vidence_gap_guard to required fixture metadata.
- Added object validation for guard booleans, stage buildability flags, evidence status, forbidden promotions, required promotion evidence, and expected guard reject reason.

ixtures/diagnostic/hlds/serverinfo/contract/serverinfo_fixture_contract.md

- Added an Evidence Gap Guard section documenting that diagnostic previews and connectionless bytes are not post-connect or signon byte evidence.

ixtures/diagnostic/hlds/serverinfo/README.md

- Added a prompt 286 guard statement describing the enforced evidence gap and real-client smoke block.

## Fixture Updates

All 9 fixture files now include vidence_gap_guard metadata. Fixture counts did not change.

| Fixture | Evidence status | Real-stage buildable | Expected guard reject reason |
| --- | --- | --- | --- |
| connectionless_query_info_candidate | connectionless_only | false | connectionless_query_not_post_connect_evidence |
| diagnostic_post_connect_serverinfo_current | diagnostic_preview_only | false | diagnostic_preview_not_byte_level_evidence |
| post_connect_real_serverinfo_unresolved | insufficient | false | unresolved_post_connect_serverinfo_not_buildable |
| signon_time_serverinfo_unresolved | unresolved | false | unresolved_signon_serverinfo_not_buildable |
| invalid_missing_required_field | synthetic_placeholder | false | invalid_serverinfo_fixture_not_buildable |
| invalid_field_order_mismatch | synthetic_placeholder | false | invalid_serverinfo_fixture_not_buildable |
| invalid_unsafe_string | synthetic_placeholder | false | invalid_serverinfo_fixture_not_buildable |
| invalid_overlong_response | synthetic_placeholder | false | invalid_serverinfo_fixture_not_buildable |
| invalid_unknown_opcode_or_marker | synthetic_placeholder | false | invalid_serverinfo_fixture_not_buildable |

## Source Updates

- Added launch options for hlds_serverinfo_unresolved_fixture_evidence_gap_guard probe mode and scenarios.
- Added a diagnostic summary struct for the evidence-gap guard.
- Added fixture metadata validation and mutation-gate reporting in hl_server_module.cpp.
- Wired guard execution into diagnostic bootstrap without changing normal host behavior.

## Regression Position

The prompt 281 fixture validator and prompt 282 builder/parser still pass after metadata additions. The unresolved fixture builder gate remains blocked as expected.
