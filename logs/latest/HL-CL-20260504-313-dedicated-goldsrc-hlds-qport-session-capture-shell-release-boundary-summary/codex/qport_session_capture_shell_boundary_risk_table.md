# Qport/session capture shell boundary risk table

| Risk | Guard | Required response |
| --- | --- | --- |
| Shell misuse | Disabled-by-default probe and explicit scenario gates | Reject unsafe invocation. |
| Accidental capture execution | `capture_executed=0` and `capture_runtime_executed=0` | Stop and fix shell gate. |
| Accidental socket open | `socket_open_attempted=0` | Stop and remove socket path. |
| Loopback capture overreach | No loopback capture execution allowed | Require separate policy before active loopback capture. |
| Public/LAN exposure | `public_socket_opened=0` and `lan_socket_opened=0` | Reject as critical regression. |
| Real client action | `real_client_binary_invoked=0` and `real_steam_client_used=0` | Reject as critical regression. |
| Connect/post-connect/signon path creep | Runtime path fields remain `0` | Reject shell expansion. |
| Netchan runtime creep | `netchan_runtime_started=0` | Reject shell expansion. |
| Qport evidence promotion | Byte evidence fields remain `0` | Require a separate evidence prompt. |
| Address-scoped challenge real-netchan overclaim | `address_scoped_challenge_reusable_as_real_netchan_proof=0` | Reject promotion. |
| Wrapper validation bypass | `wrapper_validation_required=1` and `wrapper_validation_passed=1` | Block shell plan creation. |
| Policy/fixture drift | Offline validator required | Repair policy or fixtures first. |
| Stale docs | Stable docs list exact files, commits, and blocked fields | Update docs with any boundary change. |
| Compatibility overclaim | Diagnostic-only compatibility claim required | Reject summary or release. |
