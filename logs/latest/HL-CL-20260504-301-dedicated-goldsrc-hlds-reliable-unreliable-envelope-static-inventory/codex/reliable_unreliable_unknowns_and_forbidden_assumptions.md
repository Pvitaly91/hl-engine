# Unknowns And Forbidden Assumptions

| Unknown | Why unknown | Local evidence if any | Why it must not be invented | Safe way to resolve later |
| --- | --- | --- | --- | --- |
| Reliable envelope header/order | no real reliable packet writer | blocker fields only | wrong order corrupts all downstream packets | byte fixture or approved capture policy |
| Unreliable envelope header/order | destination label only | `MSG_ONE_UNRELIABLE` | callback destination is not wire routing | evidence-backed envelope contract |
| Ack field position | no ack source symbol | none | ack placement determines packet parse | sequence/ack evidence prompt |
| Reliable bit/flag position | no reliable sequence/ack fields | blocker fields only | flag position affects packet state | reliable envelope evidence |
| Fragment envelope format | no fragment/split symbols | none | split assumptions alter framing | fragment policy prompt |
| Split packet envelope format | no split packet symbols | none | split behavior may use separate markers | fragment/split inventory |
| First post-connect server packet envelope | no real bytes | preview/unresolved serverinfo | stage confusion risk | post-connect byte fixture acquisition |
| First signon serverinfo envelope | pseudo signon only | signon descriptor surfaces | pseudo bytes are not real signon bytes | signon byte fixture acquisition |
| Reject/disconnect envelope | connectionless text only | rejection summaries | post-connect disconnect may be netchan-wrapped | reject/disconnect inventory |
| Baseline/resource envelope | no baseline writer | `resource_baselines_not_sent` | baseline order/channel are stage-critical | baseline/resource envelope inventory |
| Qport/session binding | no `qport` symbol; local `client_port` only | diagnostic port/address summaries | qport affects packet acceptance | qport/session inventory |
| Overflow behavior | no central sizebuf/overflow contract | diagnostic observation only | overflow policy changes byte-contract safety | message-writing policy contract |

unknowns_count: 12
