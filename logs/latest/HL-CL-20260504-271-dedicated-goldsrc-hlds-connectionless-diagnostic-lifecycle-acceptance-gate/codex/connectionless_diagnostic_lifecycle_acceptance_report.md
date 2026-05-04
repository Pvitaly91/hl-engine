# HLDS Connectionless Diagnostic Lifecycle Acceptance Gate

Prompt: HL-CL-20260504-271-dedicated-goldsrc-hlds-connectionless-diagnostic-lifecycle-acceptance-gate

Compatibility claim level: diagnostic-connectionless-lifecycle-acceptance-only; no real Steam Half-Life or HLDS-compatible client compatibility claimed

## Scope

This is a diagnostic-only regression boundary for prompts 265 through 270. It orchestrates the existing getchallenge, localhost UDP, address-scoped challenge cache, connect parser, userinfo validation policy, and serverinfo skeleton diagnostics. It does not implement full HLDS, Steam auth, netchan, real signon state, resource baselines, or client admission.

## Accepted Lifecycle Contract

- getchallenge-shaped connectionless request recognized: 1
- diagnostic challenge generated and response ready: 1
- localhost-only UDP diagnostic transport used: 1
- challenge cache key: remote_loopback_endpoint
- challenge one-shot: 1
- connect accepted only with matching endpoint/value: endpoint=1, value=1
- protocol/version accepted: 1
- userinfo policy passed: 1
- serverinfo skeleton response ready: 1
- public socket opened: 0
- sockets closed: 1
- real auth/netchan/signon/baseline/admission: steam_auth_not_implemented=1, netchan_not_started=1, reliable_channel_not_started=1, resource_baselines_not_sent=1, signon_state_not_entered=1, client_not_put_in_server=1

## Proof Matrix

``text

scenario                              pass accepted rejected last_reject_reason             summary
--------                              ---- -------- -------- ------------------             -------                     
happy                                 True 1        0        <none>                         logs/latest/HL-CL-20260504-…
gate_bad_marker_udp                   True 0        1        bad_connectionless_marker      logs/latest/HL-CL-20260504-…
gate_connect_without_cached_challenge True 0        1        missing_cached_challenge       logs/latest/HL-CL-20260504-…
gate_wrong_endpoint_reuse             True 0        1        challenge_endpoint_mismatch    logs/latest/HL-CL-20260504-…
gate_expired_or_replayed_challenge    True 0        1        challenge_replay               logs/latest/HL-CL-20260504-…
gate_wrong_protocol                   True 0        1        unsupported_protocol_version   logs/latest/HL-CL-20260504-…
gate_missing_required_userinfo_name   True 0        1        missing_required_userinfo_name logs/latest/HL-CL-20260504-…
gate_unsafe_userinfo                  True 0        1        unsafe_userinfo_value          logs/latest/HL-CL-20260504-…
gate_non_loopback_bind_blocked        True 0        1        non_loopback_bind_denied       logs/latest/HL-CL-20260504-…
``

Aggregate gates: total=8, passed=8, failed=0

## Remaining Before Real Client Connection Attempts

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
