# Diagnostic HLDS Qport/Session Offline Fixture Policy

This directory contains offline policy fixtures for qport/session planning. These files are not packet captures, not real client evidence, not netchan proof, and not HLDS compatibility evidence.

Compatibility claim level:
diagnostic-qport-session-offline-fixture-manifest-policy-only; no real Steam Half-Life or HLDS-compatible client compatibility claimed

## Files

| Path | Purpose |
| --- | --- |
| `qport_session_offline_fixture_manifest_policy.json` | Machine-readable policy for future qport/session offline fixture manifests. |
| `fixtures/qport_session_unresolved.json` | Positive unresolved policy fixture. It keeps qport/session byte evidence insufficient. |
| `fixtures/diagnostic_endpoint_challenge_prerequisite.json` | Positive diagnostic prerequisite fixture for endpoint/challenge/userinfo linkage. |
| `fixtures/invalid_real_client_claim.json` | Negative policy fixture with a simulated forbidden real-client claim mutation. |
| `fixtures/invalid_public_socket_claim.json` | Negative policy fixture with a simulated forbidden public socket mutation. |
| `fixtures/invalid_qport_promoted_without_byte_evidence.json` | Negative policy fixture for attempted qport promotion without bytes. |

## Safety Boundary

Every fixture in this directory must keep:

- `diagnostic_only=true`
- `real_client_used=false`
- `socket_opened=false`
- `public_socket_opened=false`
- `lan_socket_opened=false`
- `capture_executed=false`
- `netchan_started=false`
- `signon_started=false`
- `byte_level_evidence_sufficient=false`

Negative fixtures model expected validator rejections through `simulated_violation` and `expected_guard_reject_reason`. They are still offline policy fixtures and must not be treated as evidence that a real client, public socket, or qport byte contract exists.

## Promotion Rule

No qport/session fixture may be promoted to a byte contract until a later prompt supplies local byte-level evidence for qport presence, width, endian, order, endpoint/session binding, and a validator gate.
