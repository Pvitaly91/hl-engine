@"
# Challenge/Session/Userinfo Bridge Audit

Source: docs/diagnostic/hlds/netchan_sequence_ack_static_contract_inventory.md

Bridge areas audited:
- address-scoped challenge cache
- challenge one-shot/replay protection
- userinfo validation
- connect readiness
- serverinfo diagnostic readiness
- closed query/info boundary

Reusable as diagnostic prerequisite:
- challenge cache policy
- challenge replay/endpoint checks
- userinfo validation policy
- query/info stage separation guard

Not reusable as real netchan proof:
- any challenge/cache/userinfo/connect diagnostic summary
- any query/info byte response
- any post-connect preview serverinfo
- any pseudo signon sequence id

Required guards:
- keep 
etchan_not_started=1
- keep post-connect/signon builders blocked
- keep byte-level evidence gap guard active
- keep public/LAN and real-client blockers active
