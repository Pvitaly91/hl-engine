# HLDS userinfo validation policy diagnostic

Prompt: `HL-CL-20260504-270-dedicated-goldsrc-hlds-userinfo-validation-policy-diagnostic-surface`
Compatibility claim level: `diagnostic-userinfo-validation-policy-only; no real Steam Half-Life or HLDS-compatible client compatibility claimed`

## Scope
This surface is diagnostic-only. It validates a GoldSrc/HLDS-style userinfo-shaped payload after a valid address-scoped diagnostic challenge and before diagnostic connect/serverinfo readiness. It does not implement Steam auth, netchan, reliable channel state, real signon, resource baselines, or client admission.

## Policy
- userinfo_policy_enabled=1
- userinfo_policy_name=diagnostic_goldsrc_userinfo_minimal_policy
- userinfo_required_name=1
- userinfo_max_bytes=256
- userinfo_max_keys=16
- userinfo_max_key_bytes=32
- userinfo_max_value_bytes=64
- userinfo_duplicate_policy=reject_duplicate_protected_keys
- userinfo_sanitized_preview_only=1
- userinfo_policy_diagnostic_only=1

## Proof Results
| scenario | proof | accepted | rejected | last_reject_reason | policy_passed | connect_ready | serverinfo_ready | summary |
| --- | --- | ---: | ---: | --- | ---: | ---: | ---: | --- |
| `happy` | pass | 1 | 0 | `<none>` | 1 | 1 | 1 | `logs/latest/HL-CL-20260504-270-dedicated-goldsrc-hlds-userinfo-validation-policy-diagnostic-surface/runtime/hlhost_20260504_234436_676_pid41068__codex-dedicated-goldsrc-hlds-userinfo-validation-policy-diagnostic-happy_summary.log` |
| `gate_missing_required_name` | pass | 0 | 1 | `missing_required_userinfo_name` | 0 | 0 | 0 | `logs/latest/HL-CL-20260504-270-dedicated-goldsrc-hlds-userinfo-validation-policy-diagnostic-surface/runtime/hlhost_20260504_234524_550_pid17268__codex-dedicated-goldsrc-hlds-userinfo-validation-policy-diagnostic-gate-missing-required-name_summary.log` |
| `gate_malformed_userinfo_policy` | pass | 0 | 1 | `malformed_userinfo` | 0 | 0 | 0 | `logs/latest/HL-CL-20260504-270-dedicated-goldsrc-hlds-userinfo-validation-policy-diagnostic-surface/runtime/hlhost_20260504_234614_611_pid15724__codex-dedicated-goldsrc-hlds-userinfo-validation-policy-diagnostic-gate-malformed-userinfo-policy_summary.log` |
| `gate_overlong_userinfo` | pass | 0 | 1 | `userinfo_value_too_large` | 0 | 0 | 0 | `logs/latest/HL-CL-20260504-270-dedicated-goldsrc-hlds-userinfo-validation-policy-diagnostic-surface/runtime/hlhost_20260504_234705_495_pid24836__codex-dedicated-goldsrc-hlds-userinfo-validation-policy-diagnostic-gate-overlong-userinfo_summary.log` |
| `gate_duplicate_protected_key` | pass | 0 | 1 | `duplicate_userinfo_key` | 0 | 0 | 0 | `logs/latest/HL-CL-20260504-270-dedicated-goldsrc-hlds-userinfo-validation-policy-diagnostic-surface/runtime/hlhost_20260504_234754_498_pid3908__codex-dedicated-goldsrc-hlds-userinfo-validation-policy-diagnostic-gate-duplicate-protected-key_summary.log` |
| `gate_control_character_value` | pass | 0 | 1 | `unsafe_userinfo_value` | 0 | 0 | 0 | `logs/latest/HL-CL-20260504-270-dedicated-goldsrc-hlds-userinfo-validation-policy-diagnostic-surface/runtime/hlhost_20260504_234845_158_pid41352__codex-dedicated-goldsrc-hlds-userinfo-validation-policy-diagnostic-gate-control-character-value_summary.log` |
| `gate_challenge_mismatch_still_precedes_userinfo_acceptance` | pass | 0 | 1 | `challenge_value_mismatch` | 0 | 0 | 0 | `logs/latest/HL-CL-20260504-270-dedicated-goldsrc-hlds-userinfo-validation-policy-diagnostic-surface/runtime/hlhost_20260504_234931_230_pid10204__codex-dedicated-goldsrc-hlds-userinfo-validation-policy-diagnostic-gate-challenge-mismatch-still-precedes-userinfo-acceptance_summary.log` |

## Transport and Safety
- public_socket_opened=0
- loopback_udp_socket_opened=1
- bounded_loopback=1
- sockets_closed=1
- udp_bind_address=127.0.0.1
- challenge_cache_key=remote_loopback_endpoint
- challenge_one_shot=1

## Remaining Before Real Client Attempts
- production connectionless receive path
- durable challenge cache integrated with real remote address handling
- final protocol/version compatibility policy
- production userinfo validation policy
- real serverinfo wire emission
- netchan sequencing/ack
- reliable/unreliable channel setup
- resource/model/sound/event baselines
- signon state machine
- client spawn / put-in-server path
- Steam auth or explicit no-auth LAN diagnostic mode
