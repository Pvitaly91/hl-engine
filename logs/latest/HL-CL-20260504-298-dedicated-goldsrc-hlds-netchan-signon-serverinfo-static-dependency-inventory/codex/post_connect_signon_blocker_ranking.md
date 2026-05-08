# Post-Connect And Signon Blocker Ranking

Prompt ID: HL-CL-20260504-298-dedicated-goldsrc-hlds-netchan-signon-serverinfo-static-dependency-inventory

| Rank | Blocker | Severity | Reason | Smallest safe next task | Runtime needed? | Real client needed? | Public/LAN needed? |
| ---: | --- | --- | --- | --- | --- | --- | --- |
| 1 | Missing byte-level post-connect serverinfo | Critical | Real post-connect fixture remains unresolved and builder/parser remain blocked. | Static message-writing and serverinfo field contract inventory. | No | No | No |
| 2 | Missing byte-level signon-time serverinfo | Critical | Signon-time fixture remains unresolved and svc_serverinfo message id/framing is unknown. | Static netchan/message-writing inventory. | No | No | No |
| 3 | Netchan sequencing unknown | Critical | No real netchan sequencing/ack/framing proof found. | Focused netchan static inventory. | No | No | No |
| 4 | Reliable/unreliable channel unknown | High | Signon delivery path cannot be inferred from query/info or callback stubs. | Static channel dependency inventory. | No | No | No |
| 5 | Message writing policy unknown | High | Diagnostic FrameMessageBuffer is not a real network writer. | Focused message-writing inventory. | No | No | No |
| 6 | Baseline/resource linkage unknown | High | Precache/edict scaffolding exists without packet serialization evidence. | Baseline/resource linkage static inventory. | No | No | No |
| 7 | Client admission forbidden | High | client_not_put_in_server and related gates remain true by design. | Admission policy inventory only after signon evidence exists. | No | No | No |
| 8 | Real client capture forbidden | High | Current policy forbids Steam/client binaries. | Separate real-client policy boundary before any use. | No | Later only | No |
| 9 | Public/LAN exposure forbidden | High | Query/info and loopback policies explicitly block public/LAN. | Separate public/LAN policy boundary before any exposure. | No | No | Later only |
| 10 | Fixture evidence-gap guard active | Medium | Prevents unsafe promotion of unresolved fixtures, as intended. | Preserve guard and add focused inventories before fixture promotion. | No | No | No |

Top blocker: missing_byte_level_post_connect_serverinfo.

Recommended next prompt: HL-CL-20260504-299-dedicated-goldsrc-hlds-netchan-message-writing-static-inventory.
