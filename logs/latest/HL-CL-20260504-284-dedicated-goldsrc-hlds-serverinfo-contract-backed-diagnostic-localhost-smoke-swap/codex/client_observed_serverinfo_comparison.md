# Client-Observed Serverinfo Comparison

Prompt: HL-CL-20260504-284-dedicated-goldsrc-hlds-serverinfo-contract-backed-diagnostic-localhost-smoke-swap

## Old Diagnostic Skeleton Path

Prompt 277 used the existing diagnostic serverinfo text shape directly from the localhost socket pump. That path remains preserved and unchanged outside explicit smoke-swap mode.

## Contract-Backed Smoke-Swap Path

Prompt 284 uses the same diagnostic text preview selected from the checked-in fixture diagnostic_post_connect_serverinfo_current, but it is now produced through fixture validation, diagnostic preview build, parser roundtrip validation, serverinfo path integration, and localhost socket pump response send.

## Client Observation

- client_serverinfo_response_received=1
- client_serverinfo_shape_valid=1
- client_serverinfo_contract_backed=1
- response_bytes_or_text_safe_preview=serverinfo protocol=48 hostname=HLengine_Diagnostic_Server map=crossfire game=valve maxplayers=4 slot=diagnostic-client-slot-1

## Compatibility Boundary

This comparison proves only a diagnostic localhost UDP client can receive the contract-backed diagnostic preview. It does not prove byte-level real GoldSrc/HLDS post-connect or signon serverinfo compatibility.
