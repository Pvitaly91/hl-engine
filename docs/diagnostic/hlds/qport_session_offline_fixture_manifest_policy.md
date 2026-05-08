# HL-CL-20260504-304 Qport/Session Offline Fixture Manifest Policy

Compatibility claim level: diagnostic-qport-session-offline-fixture-manifest-policy-only; no real Steam Half-Life or HLDS-compatible client compatibility claimed

This policy defines how future qport/session evidence fixtures must be represented before any capture harness, socket use, real-client use, netchan runtime, post-connect serverinfo, signon serverinfo, or compatibility claim expansion. It does not implement capture, run capture, open sockets, run getchallenge/connect/post-connect/signon paths, start auth, start netchan, start reliable/unreliable channels, emit resource baselines, enter signon state, admit clients, invoke Steam, or invoke real Half-Life binaries.

## Stable Files

| File | Purpose |
| --- | --- |
| `fixtures/diagnostic/hlds/qport_session/qport_session_offline_fixture_manifest_policy.json` | Machine-readable fixture policy and schema-like contract. |
| `fixtures/diagnostic/hlds/qport_session/README.md` | Operator-facing fixture directory overview. |
| `fixtures/diagnostic/hlds/qport_session/fixtures/qport_session_unresolved.json` | Positive unresolved qport/session policy fixture. |
| `fixtures/diagnostic/hlds/qport_session/fixtures/diagnostic_endpoint_challenge_prerequisite.json` | Positive diagnostic prerequisite fixture. |
| `fixtures/diagnostic/hlds/qport_session/fixtures/invalid_real_client_claim.json` | Negative policy fixture for simulated real-client overclaim. |
| `fixtures/diagnostic/hlds/qport_session/fixtures/invalid_public_socket_claim.json` | Negative policy fixture for simulated public socket overclaim. |
| `fixtures/diagnostic/hlds/qport_session/fixtures/invalid_qport_promoted_without_byte_evidence.json` | Negative policy fixture for simulated qport promotion without bytes. |

## Required Fixture Fields

Every future fixture must include:

| Field group | Required fields |
| --- | --- |
| Identity | `fixture_id`, `family`, `stage`, `evidence_type`, `evidence_source`, `evidence_confidence` |
| Claim policy | `compatibility_claim`, `diagnostic_only` |
| Safety | `real_client_used`, `socket_opened`, `public_socket_opened`, `lan_socket_opened`, `capture_executed` |
| Qport | `qport_observed`, `qport_raw`, `qport_width`, `qport_endian`, `qport_source` |
| Endpoint/session | `client_udp_source_port`, `remote_address`, `endpoint_key` |
| Challenge/userinfo | `challenge_value`, `challenge_cache_key`, `challenge_one_shot`, `challenge_replay_detected`, `userinfo_identity`, `connect_ready` |
| Runtime blockers | `netchan_started`, `signon_started`, `byte_level_evidence_sufficient` |
| Promotion controls | `unresolved_fields`, `forbidden_promotions`, `required_before_promotion`, `expected_guard_reject_reason` |
| Review | `safe_preview`, `notes` |

## Safety Invariants

| Field | Required value |
| --- | --- |
| `diagnostic_only` | `true` |
| `real_client_used` | `false` |
| `socket_opened` | `false` |
| `public_socket_opened` | `false` |
| `lan_socket_opened` | `false` |
| `capture_executed` | `false` |
| `netchan_started` | `false` |
| `signon_started` | `false` |
| `byte_level_evidence_sufficient` | `false` until a separate evidence prompt supplies local bytes |

## Promotion Policy

Unresolved qport/session evidence cannot become a candidate byte contract until all of the following exist:

- local byte-level fixture
- qport presence evidence
- qport width evidence
- qport endian evidence
- qport field order evidence
- endpoint or session binding evidence
- offline validator gate

The policy forbids promoting:

- address-scoped challenge cache to real netchan proof
- query/info evidence to qport/session proof
- qport/session policy fixtures to post-connect or signon serverinfo evidence
- diagnostic fixtures to real Steam or HLDS compatibility

## Validator Design

The validator is intentionally not implemented in this prompt. The next prompt should add a disabled-by-default, read-only manifest validator that opens no sockets, runs no capture, invokes no real clients, and checks the policy fixtures plus negative expected reject reasons.

## Relation To Current Diagnostics

| Source | Reusable as diagnostic prerequisite? | Reusable as real proof? | Required guard |
| --- | --- | --- | --- |
| Prompt 269 address-scoped challenge cache | yes | no | `address_scoped_challenge_reusable_as_real_netchan_proof=0` |
| Prompt 270 userinfo validation | yes | no | userinfo is not admission |
| Prompt 271 lifecycle acceptance | yes as policy history | no | lifecycle is not netchan |
| Prompt 296 query/info boundary | separate closed boundary | no | query/info is not qport/session |
| Prompt 302 qport/session inventory | yes as static gap proof | no | `byte_level_qport_session_evidence_sufficient=0` |
| Prompt 303 no-client design | yes as design input | no | no capture execution |

## Recommended Next Prompt

`HL-CL-20260504-305-dedicated-goldsrc-hlds-qport-session-offline-fixture-validator-probe`

The policy and fixtures exist, but the validator is not implemented. The next safe task is a disabled-by-default read-only validator probe that checks these files and negative gates without opening sockets or invoking real clients.
