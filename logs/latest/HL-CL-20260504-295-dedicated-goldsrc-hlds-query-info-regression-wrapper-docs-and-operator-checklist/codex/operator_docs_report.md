# Operator Docs Report
Prompt: HL-CL-20260504-295-dedicated-goldsrc-hlds-query-info-regression-wrapper-docs-and-operator-checklist
Compatibility claim level: diagnostic-query-info-wrapper-operator-docs-only; no real Steam Half-Life or HLDS-compatible client compatibility claimed
## Result
Created stable operator documentation for the query/info diagnostic regression wrapper.
- Operator checklist: $operatorDoc
- Quickstart: $quickstartDoc
- Wrapper reference updated: $wrapperDoc
- Wrapper script: $wrapperScript
## Coverage
The checklist documents:
- what the wrapper does and does not do
- supported modes: ll, cceptance, and drift
- dry-run, full-run, acceptance-only, and drift-only commands
- expected output files and JSON markers
- safety checklist before running
- safety checklist after running
- troubleshooting for missing builds, drift failures, and intentionally blocked unsafe switches
- explicit forbidden actions for real clients, Steam, public sockets, LAN sockets, connect, post-connect serverinfo, signon serverinfo, auth, netchan, resources, baselines, and client admission
The quickstart provides the short command set and the summary fields operators must check.
## Boundary Statement
The docs state that connectionless query/info is not post-connect serverinfo, is not signon-time serverinfo, and is not real Steam Half-Life or HLDS-compatible client compatibility evidence.