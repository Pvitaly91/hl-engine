# HL-CL-20260504-311 Qport/Session Capture Implementation Policy Review

Compatibility claim level: diagnostic-qport-session-capture-implementation-policy-review-only; no real Steam Half-Life or HLDS-compatible client compatibility claimed

## Boundary Reviewed

The closed dry-run boundary contains the offline fixture manifest policy, offline fixture validator, capture policy gate, preflight dry-run manifest, dry-run validator, dry-run wrapper, and release boundary summary from prompts 304 through 310. The wrapper plan and validate modes still pass, and capture remains blocked by policy.

The boundary does not implement capture, execute capture, open sockets, invoke Steam or a real client, run connect/post-connect/signon, start netchan, or provide qport/session byte evidence.

## Policy Decision

| Field | Value |
| --- | ---: |
| `capture_implementation_allowed_next` | `1` |
| `capture_runtime_allowed_now` | `0` |
| `socket_open_allowed_now` | `0` |
| `public_socket_allowed_now` | `0` |
| `lan_socket_allowed_now` | `0` |
| `real_client_allowed_now` | `0` |
| `compatibility_claim_expansion_allowed_now` | `0` |

The next prompt may add only a disabled-by-default no-client capture shell/harness with explicit diagnostic gates. It must not execute capture by default, open public/LAN sockets, invoke real clients, use Steam, run runtime network paths, promote qport evidence, or claim compatibility.

## Remaining Blockers

- Qport/session byte evidence is insufficient.
- Qport width, endian, order, placement, and source-port relation are unknown.
- Address-scoped challenge remains diagnostic prerequisite only.
- Real netchan proof does not exist.
- Post-connect and signon serverinfo byte evidence remains insufficient.
- Capture execution and socket opening remain disallowed now.

## Minimum Future Scope

Future implementation is limited to a disabled-by-default shell with validator, policy gate, dry-run validator, and wrapper gates required. It may define rejected paths, summaries, timeout/cleanup policy, and artifact-only preview fields. It may not make capture active without a later explicit policy prompt.

Recommended next prompt: `HL-CL-20260504-312-dedicated-goldsrc-hlds-qport-session-no-client-capture-shell-disabled-by-default`.
