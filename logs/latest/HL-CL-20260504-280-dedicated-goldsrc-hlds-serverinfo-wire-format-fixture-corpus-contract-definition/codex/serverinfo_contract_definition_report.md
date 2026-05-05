# Serverinfo Contract Definition Report

Prompt: HL-CL-20260504-280-dedicated-goldsrc-hlds-serverinfo-wire-format-fixture-corpus-contract-definition

Compatibility claim level: diagnostic-serverinfo-fixture-contract-only; no real Steam Half-Life or HLDS-compatible client compatibility claimed

## Contract Location

- Schema: `fixtures/diagnostic/hlds/serverinfo/contract/serverinfo_fixture_contract.schema.json`
- Policy notes: `fixtures/diagnostic/hlds/serverinfo/contract/serverinfo_fixture_contract.md`

## Required Conceptual Separation

The contract requires fixtures to use explicit families and stages so these concepts stay separate:

1. connectionless query/info serverinfo candidate
2. current diagnostic post-connect serverinfo response
3. signon-time serverinfo candidate
4. unknown/unresolved serverinfo-like shapes

No fixture may merge the query/info `m` packet with the diagnostic post-connect text response or with an unresolved signon-time service message.

## Taxonomies

Allowed fixture families:

- `connectionless_query_info_candidate`
- `diagnostic_post_connect_serverinfo_current`
- `post_connect_real_serverinfo_unresolved`
- `signon_time_serverinfo_unresolved`
- `invalid_mutation`
- `unknown_unresolved`

Allowed stages:

- `connectionless_query`
- `post_connect`
- `signon_time`
- `diagnostic_current`
- `unresolved`

Allowed evidence confidence values:

- `local_code`
- `local_artifact`
- `local_doc`
- `synthetic_placeholder`
- `unresolved`

Allowed compatibility claims:

- `diagnostic_only`
- `fixture_only`
- `unresolved`
- `not_real_client_compatible`

Real-client compatibility is intentionally not an allowed compatibility claim.

## Safety Policy

Each fixture requires safety fields proving:

- `real_steam_client_used=false`
- `real_client_binary_invoked=false`
- `public_socket_opened=false`
- `normal_host_behavior_changed=false`
- `real_client_smoke_allowed_now=false`

The contract is data-only. It does not open sockets, bind public addresses, invoke Steam, invoke real clients, start auth, start netchan, enter signon, emit baselines, or put clients in server.

## Unresolved Field Policy

Unknown fields must remain explicit in `unresolved_fields` or be represented as unknown/unresolved policy values. Unknown fields must not be inferred into compatibility claims or silently converted into builder behavior.
