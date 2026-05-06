# Query/Info Boundary Risk Table

| Risk | Current mitigation | Pass evidence | Action if it fails |
|---|---|---|---|
| Fixture drift | CI manifest hashes and drift gate | `fixture_drift_detected=0` | Review fixture change under a drift prompt |
| Accidental stage confusion | Evidence-gap guard and regression gates | query/info not post-connect/signon markers remain true | Stop and harden stage-confusion gates |
| Query/info overclaim as post-connect/signon | Release docs and guard summaries | post-connect/signon allowed markers stay `0` | Revert claim and require evidence prompt |
| Public/LAN exposure | Wrapper unsafe switches and drift policy | public/LAN markers stay `0` | Treat as release blocker |
| Real-client overclaim | Compatibility claim limit and wrapper checks | real client markers stay `0` | Treat as release blocker |
| Connect path accidentally enabled | Wrapper blocks `-Connect` and acceptance checks | `connect_path_invoked=0` | Stop and isolate connect path regression |
| Post-connect/signon path accidentally enabled | Wrapper blocks modes and acceptance checks | path markers stay `0` | Stop and isolate serverinfo stage regression |
| Stale wrapper docs | Operator checklist and quickstart | docs point to current wrapper and markers | Update docs without changing runtime scope |
| CI manifest drift | Prompt 293 manifest and drift gate | `drift_gate_passed=1` | Review manifest separately |
| Operator misuse | Explicit forbidden switch list | unsafe switches reject before execution | Improve checklist and wrapper messages |