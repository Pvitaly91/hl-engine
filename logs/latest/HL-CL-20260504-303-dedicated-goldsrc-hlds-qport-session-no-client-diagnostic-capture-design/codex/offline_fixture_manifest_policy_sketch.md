# Offline Fixture Manifest Policy Sketch

This is a policy sketch for the recommended next prompt. It is not implemented here.

## Minimum Manifest Fields

| Field | Policy |
| --- | --- |
| `prompt_id` | Must identify the fixture policy prompt that created it. |
| `fixture_id` | Stable and qport/session-specific. |
| `stage` | Must be `connect_session_candidate` or `qport_session_candidate`. |
| `compatibility_claim_level` | Diagnostic-only, no real client compatibility. |
| `source` | Checked-in fixture, operator-supplied fixture, or report-only. |
| `source_hash` | Required when bytes are present. |
| `qport_field_present` | `0`, `1`, or `unknown`. |
| `qport_width_bits` | Integer or `unknown`. |
| `qport_endian` | `little`, `big`, `text`, or `unknown`. |
| `qport_order_evidence` | Textual locator or `unknown`. |
| `udp_source_port_relation` | `same`, `distinct`, or `unknown`. |
| `challenge_linkage` | Explicit linkage or `unknown`. |
| `userinfo_linkage` | Explicit linkage or `unknown`. |
| `netchan_started` | Must be `0`. |
| `real_client_used` | Must be `0`. |
| `public_socket_opened` | Must be `0`. |
| `lan_socket_opened` | Must be `0`. |

## Drift Policy

| Drift | Required outcome |
| --- | --- |
| fixture id changes | fail |
| stage changes to query/info | fail |
| stage changes to post-connect/signon | fail |
| compatibility claim adds real client or HLDS compatibility | fail |
| public/LAN policy field removed | fail |
| real-client policy field removed | fail |
| unknown qport bytes promoted to contract | fail |
| diagnostic preview treated as real netchan evidence | fail |

## Next Prompt Recommendation

The next prompt should create this manifest policy and a disabled-by-default validator design before any runtime harness is considered.
