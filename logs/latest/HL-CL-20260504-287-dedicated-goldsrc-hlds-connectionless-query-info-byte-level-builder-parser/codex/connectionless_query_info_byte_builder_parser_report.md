# Connectionless Query/Info Byte Builder Parser Report

Prompt: HL-CL-20260504-287-dedicated-goldsrc-hlds-connectionless-query-info-byte-level-builder-parser

Status: pass

Compatibility claim level: diagnostic-connectionless-query-info-byte-level-builder-parser-only; no real Steam Half-Life or HLDS-compatible client compatibility claimed

## Scope

This implementation is diagnostic-only. It byte-builds and parses only the connectionless_query_info_candidate fixture. It does not treat connectionless query/info bytes as post-connect serverinfo or signon-time serverinfo evidence.

## Dependencies

- Fixture validator invoked: 1
- Fixture validation passed: 1
- Evidence-gap guard invoked: 1
- Evidence-gap guard passed: 1
- Fixture root: fixtures/diagnostic/hlds/serverinfo
- Fixture files loaded: 9

## Byte Build

- Selected fixture: connectionless_query_info_candidate
- Selected family: connectionless_query_info_candidate
- Selected stage: connectionless_query
- Output bytes: 67
- Hex preview: $(System.Collections.Specialized.OrderedDictionary['build_output_safe_hex_preview'])
- Text preview: $(System.Collections.Specialized.OrderedDictionary['build_output_safe_text_preview'])
- Builder complete: 1
- Parser complete: 1
- Missing byte fields: <none>

## Guard Results

- Connectionless query not post-connect: 1
- Connectionless query not signon: 1
- Diagnostic preview not byte evidence: 1
- Post-connect byte evidence sufficient: 0
- Signon-time byte evidence sufficient: 0
- Real post-connect builder complete: 0
- Real signon builder complete: 0
- Real wire builder complete: 0

## Runtime Proofs

- happy: pass
- gate_disabled_by_default: pass
- gate_validator_required: pass
- gate_evidence_gap_guard_required: pass
- gate_wrong_fixture_family_rejected: pass
- gate_post_connect_stage_confusion_rejected: pass
- gate_signon_stage_confusion_rejected: pass
- gate_unresolved_post_connect_rejected: pass
- gate_unresolved_signon_rejected: pass
- gate_missing_marker_or_header: pass
- gate_wrong_opcode_or_tag: pass
- gate_field_order_mismatch: pass
- gate_missing_required_field: pass
- gate_unsafe_string: pass
- gate_overlong_response: pass
- gate_real_compatibility_claim_rejected: pass
- gate_no_real_client_used: pass
- gate_public_socket_blocked: pass


## Safety

No sockets were opened by this probe. No real Steam Half-Life client was used. No real client binary was invoked. Auth, netchan, reliable channel, signon, resources, and client admission remain unstarted.
