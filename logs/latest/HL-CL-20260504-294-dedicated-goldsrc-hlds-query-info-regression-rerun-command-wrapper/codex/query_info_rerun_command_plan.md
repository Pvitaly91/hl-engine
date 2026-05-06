# Query/Info Rerun Command Plan
Mode: all
Diagnostic only: 1
Dry run: 0
Executable: G:\DEV\РЎPP\HLengine\host\build32\host\Debug\hlhost.exe
Game dir: G:\DEV\РЎPP\HLengine\host\logs\latest\HL-CL-20260411-160-dedicated-goldsrc-signon-carried-checkpoint-claimed-checkpoint-resume-allow-surface\runtime\valve-fixture
Frames: 1
Acceptance gate included: 1
Drift gate included: 1
Selected fixture: connectionless_query_info_candidate
Selected stage: connectionless_query
## Steps### acceptance

Prompt: HL-CL-20260504-292-dedicated-goldsrc-hlds-query-info-loopback-regression-acceptance-gate

Run label: p294-full-acceptance-happy

Command:

`	ext
G:\DEV\РЎPP\HLengine\host\build32\host\Debug\hlhost.exe --dedicated --gamedir G:\DEV\РЎPP\HLengine\host\logs\latest\HL-CL-20260411-160-dedicated-goldsrc-signon-carried-checkpoint-claimed-checkpoint-resume-allow-surface\runtime\valve-fixture --maxclients 4 --frames 1 --run-label p294-full-acceptance-happy --prompt-id HL-CL-20260504-292-dedicated-goldsrc-hlds-query-info-loopback-regression-acceptance-gate --log-dir G:\DEV\РЎPP\HLengine\host\logs\latest\HL-CL-20260504-294-dedicated-goldsrc-hlds-query-info-regression-rerun-command-wrapper\wrapper_full_run\acceptance --log-summary-file 1 --log-console-level error --hlds-query-info-loopback-regression-acceptance-probe --hlds-query-info-loopback-regression-acceptance-probe-scenario happy
`

### drift

Prompt: HL-CL-20260504-293-dedicated-goldsrc-hlds-query-info-regression-ci-manifest-and-fixture-drift-gate

Run label: p294-full-drift-happy

Command:

`	ext
G:\DEV\РЎPP\HLengine\host\build32\host\Debug\hlhost.exe --dedicated --gamedir G:\DEV\РЎPP\HLengine\host\logs\latest\HL-CL-20260411-160-dedicated-goldsrc-signon-carried-checkpoint-claimed-checkpoint-resume-allow-surface\runtime\valve-fixture --maxclients 4 --frames 1 --run-label p294-full-drift-happy --prompt-id HL-CL-20260504-293-dedicated-goldsrc-hlds-query-info-regression-ci-manifest-and-fixture-drift-gate --log-dir G:\DEV\РЎPP\HLengine\host\logs\latest\HL-CL-20260504-294-dedicated-goldsrc-hlds-query-info-regression-rerun-command-wrapper\wrapper_full_run\drift --log-summary-file 1 --log-console-level error --hlds-query-info-regression-ci-manifest-drift-gate-probe --hlds-query-info-regression-ci-manifest-drift-gate-probe-scenario happy
`

