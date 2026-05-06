# Existing vs Contract-Backed Diagnostic Serverinfo Comparison

## Existing Prompt 267 Path

The existing diagnostic serverinfo path still uses the synthetic diagnostic serverinfo skeleton when the contract-backed integration flag is not enabled. Prompt 267 gates are preserved by prompt 283:

- missing connect remains rejected before response ready
- unsupported protocol remains rejected before response ready
- missing serverinfo field remains rejected before response ready

## Contract-Backed Prompt 283 Path

The contract-backed path is explicit diagnostic mode only. It reuses the prompt 281 validator and prompt 282 builder/parser, then selects diagnostic_post_connect_serverinfo_current from fixtures/diagnostic/hlds/serverinfo.

## Comparison

| Area | Existing diagnostic path | Contract-backed diagnostic path |
| --- | --- | --- |
| Enablement | Explicit serverinfo diagnostic surface | Explicit prompt 283 path integration probe |
| Fixture validator | Not required | Required before builder/parser |
| Response source | Prompt 267 diagnostic skeleton | Prompt 282 fixture-backed diagnostic preview |
| Selected fixture | n/a | diagnostic_post_connect_serverinfo_current |
| Roundtrip validation | n/a | Required before response-ready |
| Byte-level real wire format | Not claimed | Not complete, byte_level_builder_complete=0 |
| Real client compatibility | Not claimed | Not claimed |
| Sockets | None | None |
| Auth/netchan/signon/resources/admission | Not started | Not started |

The integration does not imply real HLDS or Steam client compatibility because it still uses a diagnostic preview contract, has no byte-level post-connect/signon real-wire evidence, does not start netchan or signon, and never admits a client.
