# Netchan/Signon Dependency Graph

Prompt ID: HL-CL-20260504-298-dedicated-goldsrc-hlds-netchan-signon-serverinfo-static-dependency-inventory

This graph is static and report-only. It does not run connect, post-connect serverinfo, signon serverinfo, netchan, resource baselines, sockets, Steam, or a real client binary.

| Node | Current proof status | Current implementation status | Current blocker | Upstream prerequisites | Downstream dependents | Stage boundary | Forbidden assumptions |
| --- | --- | --- | --- | --- | --- | --- | --- |
| getchallenge | Diagnostic request/response proof exists from earlier prompts | Diagnostic-only | Not real HLDS proof | Connectionless marker/command policy | Challenge cache, connect diagnostics | Connectionless | Do not treat as admission or netchan setup. |
| Challenge cache | Address-scoped diagnostic proof exists | Diagnostic-only | No production admission policy | getchallenge | Connect diagnostics, userinfo policy | Connectionless/connect bridge | Do not infer real client challenge semantics. |
| Connect datagram | Diagnostic connect policy exists in prior artifacts | Diagnostic-only | No real admission, no netchan | Challenge cache, userinfo | Post-connect preview, later netchan | Connect | Do not run in query/info or this prompt. |
| Userinfo validation | Diagnostic policy exists | Diagnostic-only | No production userinfo policy | Connect datagram, challenge cache | Admission policy, post-connect planning | Connect | Do not infer real client acceptance. |
| Diagnostic serverinfo path | Contract-backed diagnostic preview exists | Diagnostic-only | Not byte-level real post-connect or signon evidence | Fixture validator, diagnostic preview fixture | Static planning only | Diagnostic post-connect preview | Do not promote preview to real evidence. |
| Query/info byte path | Closed positive proof from 287-296 | Diagnostic-only | None inside query/info boundary | Query/info fixture, validator, evidence guard | Query/info regression wrapper | Connectionless query | Do not promote query/info to post-connect or signon. |
| Loopback socket pump | Bounded loopback proof exists in previous prompts | Diagnostic-only | Not public/LAN, not real netchan | Loopback policy | Query/info loopback, connectionless diagnostics | Loopback diagnostic | Do not expose public/LAN. |
| Netchan | Only blocker fields and policy summaries found | Not implemented as real channel | Sequencing/framing unknown | Connect/session policy, message writer policy | Reliable/unreliable, signon | Post-connect/netchan | Do not assume channel sequence or ack behavior. |
| Reliable/unreliable channels | Blocker fields found | Not implemented | Channel split and ordering unknown | Netchan | Signon messages, resource baselines | Netchan/signon | Do not send signon messages without channel evidence. |
| Post-connect serverinfo | Unresolved fixture exists | Builder/parser blocked | Missing byte-level packet evidence | Connect, message writer, framing policy | Netchan/signon planning | Post-connect | Do not use diagnostic preview as real bytes. |
| Signon-time serverinfo | Unresolved fixture exists | Builder/parser blocked | Missing byte-level signon message evidence | Netchan, reliable/unreliable, message ids | Baselines, spawn/admission | Signon-time | Do not invent svc_serverinfo framing. |
| Resource baselines | resource_baselines_not_sent gates and precache scaffolding found | Not serialized | Baseline packet format unknown | Signon channel, model/sound/event registries | Client spawn/admission | Signon/baseline | Do not infer baseline bytes from registry state. |
| Model/sound/event baselines | Precache/entity scaffolding found | Not serialized | Field layout and ordering unknown | Resource baseline policy, edict/model/sound state | Client spawn/admission | Signon/baseline | Do not emit without byte evidence. |
| Client admission / put-in-server | Explicit blocker fields found | Blocked | Admission policy and signon completion unknown | Auth, netchan, signon, baselines | Gameplay client state | Admission | Do not put client in server. |

Dependency nodes counted: 14. Dependency edges counted: 16.

Recommended next focus: HL-CL-20260504-299-dedicated-goldsrc-hlds-netchan-message-writing-static-inventory.
