# HLDS Serverinfo Diagnostic Fixture Corpus

Prompt: HL-CL-20260504-280-dedicated-goldsrc-hlds-serverinfo-wire-format-fixture-corpus-contract-definition

Compatibility claim level: diagnostic-serverinfo-fixture-contract-only; no real Steam Half-Life or HLDS-compatible client compatibility claimed.

This corpus is a diagnostic contract and fixture set. It does not prove real Steam Half-Life client compatibility and must not be used to launch a real client, open public sockets, start auth, start netchan, enter signon state, emit resource baselines, or put a client in server.

The fixtures deliberately separate:

- connectionless query/info serverinfo candidate
- current diagnostic post-connect serverinfo response from prompts 267 and 277
- unresolved real post-connect serverinfo candidate
- unresolved signon-time serverinfo candidate
- invalid/mutated parser-gate candidates

The only local byte-level candidate is the connectionless query/info response shape. The post-connect and signon-time real serverinfo shapes remain unresolved until local fixture evidence or docs are added.

Prompt HL-CL-20260504-286 adds an explicit evidence-gap guard to every fixture. The guard prevents connectionless query/info bytes, diagnostic preview text, synthetic placeholders, and unresolved fixtures from being treated as real post-connect or signon-time serverinfo evidence.

## Layout

- `contract/serverinfo_fixture_contract.schema.json`: machine-readable metadata schema for fixtures.
- `contract/serverinfo_fixture_contract.md`: contract definition and policy notes.
- `fixtures/*.json`: diagnostic fixtures.

## Safety Policy

Every fixture must state:

- `compatibility_claim` is not real-client compatible.
- `safety_policy.real_steam_client_used` is false.
- `safety_policy.real_client_binary_invoked` is false.
- `safety_policy.public_socket_opened` is false.
- `evidence_gap_guard.connectionless_query_not_post_connect` is true.
- `evidence_gap_guard.connectionless_query_not_signon` is true.
- `evidence_gap_guard.diagnostic_preview_not_byte_evidence` is true.
- `evidence_gap_guard.unresolved_real_stage_not_buildable` is true.
- unresolved fields are explicit when evidence is missing.

No fixture in this corpus claims real HLDS compatibility.
