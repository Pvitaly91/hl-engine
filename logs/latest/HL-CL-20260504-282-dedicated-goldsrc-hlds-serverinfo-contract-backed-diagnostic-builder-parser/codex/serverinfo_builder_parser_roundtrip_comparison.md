# Serverinfo Builder/Parser Roundtrip Comparison

## Allowed Diagnostic Roundtrip

The happy scenario selected diagnostic_post_connect_serverinfo_current from fixtures/diagnostic/hlds/serverinfo and built this diagnostic safe preview:

```text
serverinfo protocol=48 hostname=HLengine_Diagnostic_Server map=crossfire game=valve maxplayers=4 slot=diagnostic-client-slot-1
```

The parser validated the preview against the fixture contract with:

- marker/header valid: 1
- opcode/tag valid: 1
- field order valid: 1
- required fields present: 1
- safe string policy passed: 1
- response length within limit: 1
- numeric encoding policy checked: 1
- string encoding policy checked: 1

## Rejected Candidate Classes

- Unresolved real post-connect fixture rejected: 1.
- Invalid fixture rejected: 1.
- Real compatibility claim escalation rejected: 1.
- Field order mutation rejected with serverinfo_field_order_mismatch.
- Missing required field mutation rejected with missing_serverinfo_required_field.
- Unsafe string mutation rejected with unsafe_serverinfo_string.
- Overlong response mutation rejected with serverinfo_response_too_large.

## Compatibility Boundary

The roundtrip is contract-backed and diagnostic-preview-only. It does not prove real GoldSrc/HLDS post-connect or signon-time serverinfo wire compatibility, because those fixture families remain unresolved and are not buildable.
