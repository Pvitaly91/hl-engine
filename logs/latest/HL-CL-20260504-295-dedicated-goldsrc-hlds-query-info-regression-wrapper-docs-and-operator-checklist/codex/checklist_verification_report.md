# Checklist Verification Report
Prompt: HL-CL-20260504-295-dedicated-goldsrc-hlds-query-info-regression-wrapper-docs-and-operator-checklist
## Wrapper Verification
Dry-run executed: 1
Dry-run passed: 1
Full-run executed: 1
Full-run passed: 1
Not-run reason: <none>
## Required Fields
- wrapper_diagnostic_only: 1
- acceptance_gate_included: 1
- drift_gate_included: 1
- selected_fixture_id: connectionless_query_info_candidate
- selected_fixture_stage: connectionless_query
- public_socket_opened: 0
- lan_socket_opened: 0
- real_client_binary_invoked: 0
- connect_path_invoked: 0
- post_connect_serverinfo_path_invoked: 0
- signon_serverinfo_path_invoked: 0
- normal_host_behavior_changed: 0
## Validation Notes
- Descendant gates passed for prompt 287 through 294 commits, including prompt 294 source and artifact commits.
- g could not be launched from the Codex app path on this Windows install due to access denied, so required searches used Get-ChildItem and Select-String.
- JSON validation passed for the prompt 294 wrapper plan/summary and prompt 295 dry/full wrapper outputs.
- PowerShell syntax check passed for $wrapperScript.
- No real-client runtime proof was run by policy.
- No public/LAN socket runtime proof was run by policy.