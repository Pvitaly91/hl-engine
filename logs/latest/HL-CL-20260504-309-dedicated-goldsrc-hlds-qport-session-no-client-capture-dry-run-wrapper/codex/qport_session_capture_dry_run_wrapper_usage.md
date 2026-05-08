# HL-CL-20260504-309 Wrapper Usage Artifact

Stable documentation: docs/diagnostic/hlds/qport_session_capture_dry_run_wrapper.md

Plan mode:

`powershell
.\scripts\run_hlds_qport_session_capture_dry_run.ps1 -Mode plan -OutDir logs/latest/HL-CL-20260504-309-dedicated-goldsrc-hlds-qport-session-no-client-capture-dry-run-wrapper/wrapper_plan
`

Validate mode:

`powershell
.\scripts\run_hlds_qport_session_capture_dry_run.ps1 -Mode validate -NoBuild -OutDir logs/latest/HL-CL-20260504-309-dedicated-goldsrc-hlds-qport-session-no-client-capture-dry-run-wrapper/wrapper_validate
`

Forbidden options rejected before execution: -Public, -LAN, -RealClient, -Steam, -Capture, -Socket, -Connect, -PostConnect, -Signon, -Netchan, -Auth, and -Admission.
