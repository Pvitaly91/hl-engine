# Qport/Session Design Blocker Ranking

| Rank | Blocker | Severity | Reason | Smallest safe next task | Runtime needed? | Real client needed? | Public/LAN needed? |
| ---: | --- | --- | --- | --- | --- | --- | --- |
| 1 | missing offline fixture manifest policy | critical | No qport/session fixture can be accepted without stage, hash, compatibility, and safety fields. | qport/session offline fixture manifest policy | no | no | no |
| 2 | missing qport/session byte contract | critical | Prompt 302 found no `qport` symbol and no width/endian/order evidence. | manifest policy, then fixture ingestion policy | no | no | no |
| 3 | missing qport-to-netchan binding | critical | `netchan_not_started=1`; no channel object or remote binding exists. | manifest policy with explicit netchan blockers | no | no | no |
| 4 | missing reliable/unreliable envelope byte contract | critical | Prompt 301 found only diagnostic blockers and labels. | offline policy before envelope contract | no | no | no |
| 5 | missing sequence/ack byte contract | critical | Prompt 300 found no sufficient sequence/ack bytes. | qport/session policy before sequence/ack fixture | no | no | no |
| 6 | missing remote address/session key policy | high | Endpoint strings are diagnostic-only. | fixture manifest policy | no | no | no |
| 7 | missing post-connect serverinfo byte contract | high | Post-connect evidence remains insufficient. | keep blocked until channel evidence | no | no | no |
| 8 | missing signon serverinfo byte contract | high | Signon evidence remains insufficient. | keep blocked until channel/baseline evidence | no | no | no |
| 9 | real client capture forbidden | policy blocker | Real client use is outside this boundary. | separate policy prompt only | later | later | no |
| 10 | public/LAN exposure forbidden | policy blocker | Public/LAN sockets remain blocked. | keep fail-closed policy | no | no | no |

## Top Blocker

`missing_offline_fixture_manifest_policy`

The design cannot move directly to capture or fixture contracts. A manifest policy must exist first so any future evidence is stage-specific, hashed, diagnostic-only, and fail-closed.
