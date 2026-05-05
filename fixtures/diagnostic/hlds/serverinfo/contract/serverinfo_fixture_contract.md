# Serverinfo Fixture Contract

Prompt: HL-CL-20260504-280-dedicated-goldsrc-hlds-serverinfo-wire-format-fixture-corpus-contract-definition

Compatibility claim level: diagnostic-serverinfo-fixture-contract-only; no real Steam Half-Life or HLDS-compatible client compatibility claimed.

## Families

Allowed `family` values:

- `connectionless_query_info_candidate`
- `diagnostic_post_connect_serverinfo_current`
- `post_connect_real_serverinfo_unresolved`
- `signon_time_serverinfo_unresolved`
- `invalid_mutation`
- `unknown_unresolved`

## Stages

Allowed `stage` values:

- `connectionless_query`
- `post_connect`
- `signon_time`
- `diagnostic_current`
- `unresolved`

## Evidence Confidence

Allowed `evidence_confidence` values:

- `local_code`
- `local_artifact`
- `local_doc`
- `synthetic_placeholder`
- `unresolved`

## Compatibility Claims

Allowed `compatibility_claim` values:

- `diagnostic_only`
- `fixture_only`
- `unresolved`
- `not_real_client_compatible`

No fixture may claim real Steam Half-Life or HLDS-compatible client compatibility.

## Required Metadata

Each fixture must define:

- `fixture_id`
- `family`
- `stage`
- `evidence_source`
- `evidence_confidence`
- `compatibility_claim`
- `marker_or_header`
- `opcode_or_tag`
- `field_order`
- `fields`
- `string_encoding_policy`
- `numeric_encoding_policy`
- `length_policy`
- `safety_policy`
- `expected_parse_result`
- `expected_reject_reason`
- `unresolved_fields`
- `safe_preview`
- `notes`

## Unknown Field Policy

Unknown fields must be represented as `unknown`, `unresolved`, or explicit entries in `unresolved_fields`. Unknowns must not be inferred into compatibility claims.

## Safe Preview Policy

Safe previews are bounded, deterministic, and may contain either short hex token strings or sanitized text. No fixture should store raw sensitive userinfo, auth material, external endpoints, public IPs, or unbounded byte payloads.

## Socket And Client Policy

Fixtures are data only. They must not open sockets, bind public addresses, invoke Steam, invoke real client binaries, start auth, start netchan, enter signon state, emit baselines, or put clients in server.
