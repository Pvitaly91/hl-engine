# HL-CL-20260504-321 Qport/Session Capture Execution Policy Review

Compatibility claim level: diagnostic-qport-session-capture-execution-policy-review-only; no real Steam Half-Life or HLDS-compatible client compatibility claimed

This policy review evaluates whether any future no-client qport/session capture execution can be considered after the runtime skeleton and drift boundaries were closed. It is a report-only boundary. It does not implement capture, execute capture, open sockets, send or receive datagrams, run getchallenge/connect/post-connect/signon paths, start netchan, invoke Steam, invoke a real client, promote qport/session byte evidence, or expand compatibility claims.

## Closed Boundary Reviewed

The reviewed boundary includes:

| Boundary item | Path or source |
| --- | --- |
| Offline fixture policy | `fixtures/diagnostic/hlds/qport_session/qport_session_offline_fixture_manifest_policy.json` |
| Offline fixture validator | Prompt `HL-CL-20260504-305` |
| Capture policy gate | Prompt `HL-CL-20260504-306` |
| Dry-run manifest | `fixtures/diagnostic/hlds/qport_session/qport_session_capture_preflight_dry_run_manifest.json` |
| Dry-run validator | Prompt `HL-CL-20260504-308` |
| Dry-run wrapper | `scripts/run_hlds_qport_session_capture_dry_run.ps1` |
| Capture shell | Prompt `HL-CL-20260504-312` |
| Shell CI drift gate | `fixtures/diagnostic/hlds/qport_session/qport_session_capture_shell_ci_manifest.json` and prompt `HL-CL-20260504-314` |
| Final policy gate | Prompt `HL-CL-20260504-316` |
| Runtime skeleton | Prompt `HL-CL-20260504-317` |
| Runtime skeleton release boundary | Prompt `HL-CL-20260504-318` |
| Runtime skeleton CI drift gate | `fixtures/diagnostic/hlds/qport_session/qport_session_capture_runtime_skeleton_ci_manifest.json` and prompt `HL-CL-20260504-319` |
| Wrapper and CI release summary | `docs/diagnostic/hlds/qport_session_runtime_skeleton_wrapper_ci_release_summary.md` and prompt `HL-CL-20260504-320` |

The reviewed boundary is closed for diagnostic review only:

- `qport_session_runtime_skeleton_wrapper_ci_boundary_closed=1`
- `runtime_skeleton_drift_gate_passed=1`
- `runtime_skeleton_drift_detected=0`
- `dependency_drift_detected=0`
- `capture_allowed_now=0`
- `capture_executed=0`
- `capture_runtime_executed=0`
- `datagram_sent=0`
- `datagram_received=0`
- `socket_open_attempted=0`
- `real_client_binary_invoked=0`
- `connect_path_invoked=0`
- `signon_serverinfo_path_invoked=0`
- `netchan_runtime_started=0`

## Policy Decision

Capture execution is not allowed now. A future no-client capture execution policy gate is allowed next only if it remains policy-only and read-only. A future minimal no-client loopback capture implementation is not allowed next.

| Decision field | Value | Reason |
| --- | --- | --- |
| `capture_execution_allowed_now` | `0` | The boundary is still policy and skeleton only. |
| `capture_runtime_allowed_now` | `0` | No capture runtime execution has been approved. |
| `socket_open_allowed_now` | `0` | No socket approval exists for this boundary. |
| `loopback_socket_allowed_now` | `0` | Loopback sockets still require a separate execution policy gate. |
| `public_socket_allowed_now` | `0` | Public sockets remain out of scope. |
| `lan_socket_allowed_now` | `0` | LAN sockets remain out of scope. |
| `datagram_send_allowed_now` | `0` | Datagram send remains blocked. |
| `datagram_receive_allowed_now` | `0` | Datagram receive remains blocked. |
| `real_client_allowed_now` | `0` | Real clients and Steam remain out of scope. |
| `connect_path_allowed_now` | `0` | Connect runtime remains blocked. |
| `post_connect_serverinfo_allowed_now` | `0` | Post-connect serverinfo runtime remains blocked. |
| `signon_serverinfo_allowed_now` | `0` | Signon serverinfo runtime remains blocked. |
| `netchan_runtime_allowed_now` | `0` | Netchan runtime remains blocked. |
| `qport_evidence_promotion_allowed_now` | `0` | Unknown byte-level behavior must remain unknown. |
| `compatibility_claim_expansion_allowed_now` | `0` | No real client or HLDS-compatible compatibility is claimed. |

Explicit answers:

- Is capture execution allowed now? No.
- Is a future no-client capture execution policy gate allowed next? Yes, only if it remains policy-only, read-only, diagnostic-only, and does not open sockets or send/receive datagrams.
- Is a future minimal no-client loopback capture implementation allowed next? No, not yet.
- What blockers must be resolved before any execution attempt? A final execution policy gate, loopback-only policy, socket/datagram approvals, bounded timeout and cleanup policy, artifact schema, stop-before-capture proof, and immediate drift/wrapper validation all need to exist first.
- What gates must exist before execution could ever be considered? Offline fixture validator, capture policy gate, dry-run validator, dry-run wrapper plan/validate, shell CI drift gate, final policy gate, runtime skeleton CI drift gate, and a new execution-specific final policy gate.

## Future Execution Preconditions

Before any capture execution could ever be considered, these preconditions must be satisfied by a separate explicit prompt:

1. An explicit future execution final policy gate exists and passes.
2. The gate remains no-client and diagnostic-only.
3. Execution policy is loopback-only.
4. Execution policy has a bounded frame or step count.
5. Public and LAN socket behavior remains forbidden.
6. Real clients and Steam remain forbidden.
7. Connect, post-connect, signon, and netchan runtime remain forbidden unless separately approved.
8. Qport/session byte evidence promotion remains forbidden during the execution policy gate.
9. Compatibility claim expansion remains forbidden.
10. Dry-run wrapper plan and validate modes pass immediately before any execution discussion.
11. Runtime skeleton CI drift gate passes immediately before any execution discussion.
12. Cleanup proof is required before any future execution implementation.
13. No socket opens are allowed unless separately approved.
14. No datagram send or receive is allowed unless separately approved.
15. Artifact schema is locked before any future execution implementation.
16. Timeout policy is locked and a stop-before-capture proof exists before any future execution implementation.

## Allowed And Forbidden Actions

| Action | Allowed in this prompt | Allowed in next policy-gate prompt | Allowed in future execution prompt | Forbidden until separate policy | Notes |
| --- | --- | --- | --- | --- | --- |
| Docs/report changes | Yes | Yes | Yes | No | Report-only updates are allowed. |
| Final policy review | Yes | Yes | Yes | No | This prompt performs review only. |
| Capture execution policy gate | No | Yes | Yes | No | Next prompt may define a policy-only gate. |
| Minimal no-client loopback capture implementation | No | No | Conditional | Yes | Requires a passed execution final policy gate first. |
| Runtime skeleton changes | No | No | Conditional | Yes | Requires a separate implementation prompt. |
| Open loopback socket | No | No | Conditional | Yes | Requires separate socket approval. |
| Send synthetic datagram | No | No | Conditional | Yes | Requires separate datagram approval. |
| Receive synthetic datagram | No | No | Conditional | Yes | Requires separate datagram approval. |
| Public socket | No | No | No | Yes | Public socket behavior remains out of scope. |
| LAN socket | No | No | No | Yes | LAN socket behavior remains out of scope. |
| Real client | No | No | No | Yes | Real clients remain out of scope. |
| Steam | No | No | No | Yes | Steam remains out of scope. |
| Connect path | No | No | No | Yes | Runtime connect path remains blocked. |
| Post-connect serverinfo | No | No | No | Yes | Post-connect runtime remains blocked. |
| Signon serverinfo | No | No | No | Yes | Signon runtime remains blocked. |
| Netchan runtime | No | No | No | Yes | Netchan remains blocked. |
| Qport evidence promotion | No | No | No | Yes | Unknown byte-level behavior remains unknown. |
| Compatibility claim | No | No | No | Yes | Diagnostic-only claim is required. |

## Risk Review

| Risk | Current guard | Required response |
| --- | --- | --- |
| Accidental execution | `capture_execution_allowed_now=0`, `capture_executed=0` | Treat as a blocking regression. |
| Socket open creep | `socket_open_allowed_now=0`, `socket_open_attempted=0` | Stop and require separate socket policy. |
| Datagram send/receive creep | `datagram_send_allowed_now=0`, `datagram_receive_allowed_now=0` | Stop and require separate datagram policy. |
| Public/LAN exposure | `public_socket_allowed_now=0`, `lan_socket_allowed_now=0` | Reject as out of scope. |
| Real client overreach | `real_client_allowed_now=0`, `real_client_binary_invoked=0` | Reject as out of scope. |
| Netchan runtime creep | `netchan_runtime_allowed_now=0`, `netchan_runtime_started=0` | Reject as out of scope. |
| Connect/post-connect/signon creep | Runtime path allowed and invoked fields remain `0` | Reject as out of scope. |
| Qport evidence overclaim | `qport_evidence_promotion_allowed_now=0` | Require a separate byte-evidence prompt. |
| Address-scoped challenge overclaim | `address_scoped_challenge_reusable_as_real_netchan_proof=0` | Reject promotion to real netchan proof. |
| CI drift bypass | Runtime skeleton CI drift gate remains required | Stop and rerun drift validation. |
| Wrapper bypass | Wrapper plan and validate remain required | Stop and rerun wrapper validation. |
| Cleanup or timeout missing | Future preconditions require cleanup and timeout policy | Deny execution policy until locked. |
| Compatibility overclaim | Diagnostic-only compatibility claim required | Reject expanded claims. |

## Next Prompt

Recommended next prompt:

`HL-CL-20260504-322-dedicated-goldsrc-hlds-qport-session-capture-execution-final-policy-gate`

Recommended next task: create a final policy-only gate that decides whether a later no-client capture execution implementation prompt may be considered, while capture execution, sockets, datagrams, real clients, runtime paths, qport evidence promotion, and compatibility expansion remain blocked now.
