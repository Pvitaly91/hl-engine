# Qport/Session Offline Fixture Schema Report

The schema-like contract is encoded in `qport_session_offline_fixture_manifest_policy.json` under `schema_contract` and enforced by future validator design.

## Required Fields

| Group | Fields |
| --- | --- |
| Identity | `fixture_id`, `family`, `stage`, `evidence_type`, `evidence_source`, `evidence_confidence` |
| Claims | `compatibility_claim`, `diagnostic_only` |
| Safety | `real_client_used`, `socket_opened`, `public_socket_opened`, `lan_socket_opened`, `capture_executed` |
| Qport | `qport_observed`, `qport_raw`, `qport_width`, `qport_endian`, `qport_source` |
| Endpoint/session | `client_udp_source_port`, `remote_address`, `endpoint_key` |
| Challenge/userinfo | `challenge_value`, `challenge_cache_key`, `challenge_one_shot`, `challenge_replay_detected`, `userinfo_identity`, `connect_ready` |
| Runtime blockers | `netchan_started`, `signon_started`, `byte_level_evidence_sufficient` |
| Promotion | `unresolved_fields`, `forbidden_promotions`, `required_before_promotion`, `expected_guard_reject_reason` |
| Review | `safe_preview`, `notes` |

## Required Constants

| Field | Required value until a separate evidence prompt |
| --- | --- |
| `diagnostic_only` | true |
| `real_client_used` | false |
| `socket_opened` | false |
| `public_socket_opened` | false |
| `lan_socket_opened` | false |
| `capture_executed` | false |
| `netchan_started` | false |
| `signon_started` | false |
| `byte_level_evidence_sufficient` | false |

## Unknown Representation

Unknown qport/session byte behavior must be represented as `unknown` and must remain listed in `unresolved_fields`. Unknowns may not be promoted into byte contracts.

## Promotion Requirements

Promotion from unresolved fixture to qport/session byte contract requires:

- local byte-level fixture
- qport presence evidence
- qport width evidence
- qport endian evidence
- qport field order evidence
- endpoint or session binding evidence
- offline validator gate
