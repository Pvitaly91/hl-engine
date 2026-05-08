# Challenge, Session, And Userinfo Linkage Inventory

Prompt ID: HL-CL-20260504-298-dedicated-goldsrc-hlds-netchan-signon-serverinfo-static-dependency-inventory

| Prior surface | What can be reused safely | Still diagnostic-only | Must not be reused as real compatibility evidence | Needed before post-connect/signon evidence can be trusted |
| --- | --- | --- | --- | --- |
| Prompt 269 address-scoped challenge cache | Challenge issuance, TTL, one-shot/replay, endpoint gating as diagnostic policy | All challenge cache behavior | Real HLDS challenge semantics | Stage-specific challenge/session linkage evidence. |
| Prompt 270 userinfo validation policy | Userinfo parsing limits and unsafe-value guards as diagnostic policy | All userinfo acceptance behavior | Production userinfo/admission policy | Final production userinfo policy and real client expectations. |
| Prompt 271 connectionless lifecycle acceptance | Sequencing gates for getchallenge/connect/userinfo diagnostics | Lifecycle acceptance as diagnostic harness | Client admission or netchan start | Real transition evidence from connect to post-connect packets. |
| Prompts 273-277 loopback pump/frame/client smoke | Loopback-only socket policy, cleanup expectations, public-socket blockers | Loopback client/pump harnesses | Public/LAN or real-client compatibility | Separate policy before any capture/run beyond static inventory. |
| Prompts 287-296 query/info boundary | Query/info fixture/gates/wrapper and drift protections | Connectionless query/info only | Post-connect/signon serverinfo proof | Maintain as closed boundary; do not cross-promote stages. |

Challenge/session links found: 1,808 aggregate getchallenge plus challenge matches in the selected scan. Userinfo links found: 838 aggregate userinfo matches. These counts show useful diagnostic policy context, not real session/admission proof.
