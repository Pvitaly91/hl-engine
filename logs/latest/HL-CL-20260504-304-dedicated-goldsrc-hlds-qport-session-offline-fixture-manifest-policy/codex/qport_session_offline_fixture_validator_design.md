# Qport/Session Offline Fixture Validator Design

Validator implemented: 0

Validator design created: 1

## Purpose

The future validator should be disabled by default and read only the policy JSON plus fixture JSON files. It must not open sockets, run capture, invoke a real client, run runtime stages, or mutate normal host behavior.

## Required Future Checks

| Check | Required behavior |
| --- | --- |
| Policy file present | Load `qport_session_offline_fixture_manifest_policy.json`. |
| Fixture allowlist present | Load exactly the expected fixture files unless explicitly updated by a policy prompt. |
| Required metadata | Fail if any required field is missing. |
| Diagnostic-only | Require `diagnostic_only=true`. |
| No real client | Require `real_client_used=false`. |
| No sockets | Require `socket_opened=false`, `public_socket_opened=false`, `lan_socket_opened=false`. |
| No capture | Require `capture_executed=false`. |
| No runtime | Require `netchan_started=false`, `signon_started=false`. |
| No qport byte promotion | Require `byte_level_evidence_sufficient=false` until separate evidence prompt. |
| Negative fixtures | Verify expected reject reasons for simulated violations. |

## Required Future Scenarios

| Scenario | Expected result |
| --- | --- |
| `happy_policy_fixtures_load` | positive fixtures accepted as policy fixtures |
| `gate_real_client_claim_rejected` | `invalid_real_client_claim` rejected |
| `gate_public_socket_claim_rejected` | `invalid_public_socket_claim` rejected |
| `gate_qport_promotion_without_byte_evidence_rejected` | qport promotion fixture rejected |
| `gate_query_info_stage_confusion_rejected` | query/info cannot be qport/session evidence |
| `gate_address_challenge_not_real_netchan_proof` | diagnostic challenge cache cannot be real netchan proof |

## Non-Implementation Rationale

This prompt creates policy and fixtures only. A validator probe is the next smallest safe implementation, because the policy now provides stable files and expected reject reasons.
