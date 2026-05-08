# Stage-Specific Reuse Assessment

Source: docs/diagnostic/hlds/netchan_message_writing_static_inventory.md

| Stage | Reusable now | Diagnostic-only | Requires new byte evidence | Risk |
| --- | --- | --- | --- | --- |
| connectionless query/info | yes, only inside closed boundary | yes | no for existing boundary | low if scoped |
| post-connect serverinfo | no | previews only | yes | high |
| signon-time serverinfo | no | pseudo reports only | yes | high |
| netchan reliable messages | no | no real implementation found | yes | high |
| netchan unreliable messages | no | no real implementation found | yes | high |
| baseline/resource messages | no | blocker/report only | yes | high |
| reject/disconnect messages | partial connectionless diagnostics only | yes | yes for post-connect/signon | medium |

No real Steam Half-Life client compatibility is claimed. No real HLDS-compatible client compatibility is claimed.
