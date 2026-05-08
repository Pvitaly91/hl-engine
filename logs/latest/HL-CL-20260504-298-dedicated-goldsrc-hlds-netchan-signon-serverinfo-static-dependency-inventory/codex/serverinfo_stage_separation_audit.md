# Serverinfo Stage Separation Audit

Prompt ID: HL-CL-20260504-298-dedicated-goldsrc-hlds-netchan-signon-serverinfo-static-dependency-inventory

| Stage | Current evidence | Current fixture | Byte-level evidence sufficient? | Builder allowed now? | Parser allowed now? | Real compatibility claim allowed? | Next safe action |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Connectionless query/info | Closed diagnostic byte-level boundary from prompts 287-296 | connectionless_query_info_candidate | Yes, for diagnostic query/info only | Yes, diagnostic-only | Yes, diagnostic-only | No | Preserve regression wrapper and drift gate. |
| Diagnostic post-connect preview | Contract-backed preview and artifacts | diagnostic_post_connect_serverinfo_current | No real byte evidence | Only diagnostic preview/path checks | Only diagnostic preview/path checks | No | Keep as negative/reference input. |
| Real post-connect serverinfo | Unresolved fixture and evidence-gap guard | post_connect_real_serverinfo_unresolved | No | No | No | No | Acquire byte-level field/framing evidence in a later approved prompt. |
| Signon-time serverinfo | Unresolved fixture and evidence-gap guard | signon_time_serverinfo_unresolved | No | No | No | No | Inventory netchan/message-writing dependencies first. |
| Netchan messages | Blocker fields, no real channel implementation | None | No | No | No | No | Static message-writing/netchan inventory. |
| Resource/model/sound/event baselines | Registry/scaffolding and blocker fields | None | No | No | No | No | Static baseline/resource linkage inventory. |

Stage-separation conclusion: query/info remains connectionless-only. It is not post-connect serverinfo and not signon-time serverinfo. Diagnostic preview serverinfo remains preview-only and cannot be promoted to real bytes. Real post-connect and signon builders remain blocked.
