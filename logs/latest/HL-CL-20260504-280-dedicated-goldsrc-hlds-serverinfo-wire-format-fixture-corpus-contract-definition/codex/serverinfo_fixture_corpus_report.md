# Serverinfo Fixture Corpus Report

Prompt: HL-CL-20260504-280-dedicated-goldsrc-hlds-serverinfo-wire-format-fixture-corpus-contract-definition

Compatibility claim level: diagnostic-serverinfo-fixture-contract-only; no real Steam Half-Life or HLDS-compatible client compatibility claimed

## Corpus Location

Stable fixture root: `fixtures/diagnostic/hlds/serverinfo`

The corpus is checked into a stable repo path rather than prompt-local logs. Prompt-local artifacts reference the fixture files; they do not duplicate every fixture body.

## Corpus Files

Contract files:

- `fixtures/diagnostic/hlds/serverinfo/contract/serverinfo_fixture_contract.schema.json`
- `fixtures/diagnostic/hlds/serverinfo/contract/serverinfo_fixture_contract.md`
- `fixtures/diagnostic/hlds/serverinfo/README.md`

Fixture files:

- `fixtures/diagnostic/hlds/serverinfo/fixtures/connectionless_query_info_candidate.json`
- `fixtures/diagnostic/hlds/serverinfo/fixtures/diagnostic_post_connect_serverinfo_current.json`
- `fixtures/diagnostic/hlds/serverinfo/fixtures/post_connect_real_serverinfo_unresolved.json`
- `fixtures/diagnostic/hlds/serverinfo/fixtures/signon_time_serverinfo_unresolved.json`
- `fixtures/diagnostic/hlds/serverinfo/fixtures/invalid_missing_required_field.json`
- `fixtures/diagnostic/hlds/serverinfo/fixtures/invalid_field_order_mismatch.json`
- `fixtures/diagnostic/hlds/serverinfo/fixtures/invalid_unsafe_string.json`
- `fixtures/diagnostic/hlds/serverinfo/fixtures/invalid_overlong_response.json`
- `fixtures/diagnostic/hlds/serverinfo/fixtures/invalid_unknown_opcode_or_marker.json`

## Fixture Counts

| Metric | Count |
| --- | ---: |
| total fixtures | 9 |
| valid fixture-only cases | 2 |
| invalid/mutated cases | 5 |
| unresolved cases | 2 |
| families covered | 5 |
| known byte-level candidates | 1 |
| synthetic placeholder cases | 5 |
| real compatibility claims | 0 |

## Families Covered

- `connectionless_query_info_candidate`: local byte-level query/info candidate backed by `BuildGoldSrcInfoResponse` and `ParseGoldSrcInfoResponse`.
- `diagnostic_post_connect_serverinfo_current`: prompt 267/277 text-shaped diagnostic post-connect response.
- `post_connect_real_serverinfo_unresolved`: explicit unresolved placeholder for real post-connect serverinfo.
- `signon_time_serverinfo_unresolved`: explicit unresolved placeholder for signon-time serverinfo.
- `invalid_mutation`: future parser gate cases for missing field, field order mismatch, unsafe string, overlong response, and unknown marker/opcode.

## Sufficiency Decision

The corpus is sufficient for a next diagnostic fixture contract validator/probe. It is not a real-client compatibility proof and does not justify a real client smoke attempt.

Real-client smoke remains blocked because post-connect real serverinfo and signon-time serverinfo are still unresolved, and auth/netchan/reliable/signon/resource/admission layers remain absent.
