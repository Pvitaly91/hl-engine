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
- `evidence_gap_guard`
- `expected_parse_result`
- `expected_reject_reason`
- `unresolved_fields`
- `safe_preview`
- `notes`

## Evidence Gap Guard

Every fixture must carry an `evidence_gap_guard` object. This guard is required because prompt HL-CL-20260504-285 found byte-level evidence only for the connectionless query/info candidate, not for real post-connect or signon-time serverinfo.

The guard must state:

- `evidence_gap_guard_enabled: true`
- `stage_confusion_guard_enabled: true`
- `diagnostic_preview_not_byte_evidence: true`
- `connectionless_query_not_post_connect: true`
- `connectionless_query_not_signon: true`
- `unresolved_real_stage_not_buildable: true`
- `byte_level_evidence_required_for_real_stage: true`
- `real_stage_buildable: false`
- `real_stage_parseable: false`
- `byte_level_evidence_status`
- `forbidden_promotions`
- `required_before_promotion`
- `expected_guard_reject_reason`

Connectionless query/info bytes must not be promoted into post-connect or signon-time evidence. Diagnostic preview text must not be promoted into real wire evidence. Unresolved real-stage fixtures must remain non-buildable until a local byte-level fixture, local field order evidence, local opcode/message-id evidence, string and numeric encoding evidence, and a validator promotion gate are all present.

## Unknown Field Policy

Unknown fields must be represented as `unknown`, `unresolved`, or explicit entries in `unresolved_fields`. Unknowns must not be inferred into compatibility claims.

## Safe Preview Policy

Safe previews are bounded, deterministic, and may contain either short hex token strings or sanitized text. No fixture should store raw sensitive userinfo, auth material, external endpoints, public IPs, or unbounded byte payloads.

## Socket And Client Policy

Fixtures are data only. They must not open sockets, bind public addresses, invoke Steam, invoke real client binaries, start auth, start netchan, enter signon state, emit baselines, or put clients in server.
