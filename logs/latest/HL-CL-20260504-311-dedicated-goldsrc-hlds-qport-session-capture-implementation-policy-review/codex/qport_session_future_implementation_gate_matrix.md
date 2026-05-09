# Qport/Session Future Implementation Gate Matrix

Compatibility claim level: diagnostic-qport-session-capture-implementation-policy-review-only; no real Steam Half-Life or HLDS-compatible client compatibility claimed

| Gate | Required state | Failure response |
| --- | --- | --- |
| Disabled by default | Required | Reject implementation. |
| Explicit diagnostic flag | Required | Reject implementation. |
| Offline fixture validator | Required and passing | Reject implementation. |
| Capture policy gate | Required and authoritative | Reject implementation. |
| Dry-run validator | Required and passing | Reject implementation. |
| Wrapper validation | Required and passing | Reject implementation. |
| No real client | Required | Reject implementation. |
| No public socket | Required | Reject implementation. |
| No LAN socket | Required | Reject implementation. |
| No non-loopback | Required if sockets ever exist | Reject implementation. |
| No capture execution unless explicitly allowed later | Required | Reject implementation. |
| No qport evidence promotion | Required | Reject implementation. |
| No address-scoped challenge real netchan proof | Required | Reject implementation. |
| No connect/post-connect/signon runtime | Required | Reject implementation. |
| No netchan runtime | Required | Reject implementation. |
| No auth/resources/admission | Required | Reject implementation. |
| Bounded timeout | Required for any future active path | Reject implementation. |
| Deterministic cleanup | Required for any future active path | Reject implementation. |
| Artifact schema complete | Required | Reject implementation. |
