# Qport/Session Capture Implementation Risk Review

Compatibility claim level: diagnostic-qport-session-capture-implementation-policy-review-only; no real Steam Half-Life or HLDS-compatible client compatibility claimed

| Risk | Severity | Required guard |
| --- | --- | --- |
| Accidental socket open | critical | Disabled by default; socket open remains disallowed now. |
| Accidental capture execution | critical | Capture execution denied until separate policy changes the gate. |
| Real client overreach | critical | Real client and Steam rejected. |
| Public/LAN exposure | critical | Public/LAN hard-deny. |
| Qport byte evidence overclaim | high | Byte-level qport evidence remains insufficient. |
| Address-scoped challenge overclaim | high | Cannot become real netchan proof. |
| Connect/post-connect/signon path creep | high | Runtime path fields must remain zero. |
| Netchan/auth/resources/admission creep | high | No runtime startup allowed. |
| Fixture/policy drift | high | Offline fixture validator and JSON validation required. |
| Wrapper bypass | high | Wrapper plan/validate success required. |
| Stale docs | medium | Update stable docs with every policy/path change. |
| Normal host behavior changed | critical | Shell must be disabled by default and preserve normal behavior. |
