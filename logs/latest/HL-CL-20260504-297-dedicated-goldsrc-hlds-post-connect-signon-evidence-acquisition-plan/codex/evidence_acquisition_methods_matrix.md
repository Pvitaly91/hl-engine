# Evidence Acquisition Methods Matrix

Prompt: HL-CL-20260504-297-dedicated-goldsrc-hlds-post-connect-signon-evidence-acquisition-plan

| Method | Allowed now | Reason | Safety risk | Evidence value | Required preconditions | Required prompt before execution | Expected artifacts | Cleanup |
|---|---|---|---|---|---|---|---|---|
| Static repo/code inspection | yes | read-only checked-in evidence | low | medium dependency map | none beyond branch/ancestry checks | none for read-only inventory | source symbol map, dependency inventory | none |
| Fixture extension from local docs | conditional | safe only with explicit local byte evidence | overclaiming docs as wire evidence | medium | local doc path, hash, stage map | fixture ingestion policy | fixture candidate, schema validation | quarantine or revert bad fixture |
| No-client diagnostic capture harness design | plan only | design can be written without sockets | scope creep into runtime | medium future value | static inventory | no-client capture design prompt | design doc, rejected modes | none |
| Loopback no-auth diagnostic capture | no | would execute networking and stage logic | auth/netchan/signon/admission confusion | high if later policy-gated | loopback policy and no-auth policy | no-auth loopback policy review | bounded capture logs, cleanup proof | close sockets |
| Real Steam/Half-Life client capture | no | explicitly forbidden now | real client invocation and overclaim | high if later approved | explicit approval and isolated setup | real-client capture policy boundary | capture hashes, sanitized fixtures | terminate client and sanitize logs |
| Public/LAN query capture | no | public/LAN exposure forbidden | public networking exposure | not needed for next step | separate public/LAN policy | public/LAN exposure review | policy artifacts first | network teardown if ever allowed |
| HLDS reference capture if locally available | no | may invoke external binaries/reference server | external binary/provenance risk | high if controlled later | local path policy and provenance | reference fixture ingestion/capture policy | hashes, capture notes | terminate process and sanitize artifacts |