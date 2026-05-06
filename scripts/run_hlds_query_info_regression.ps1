param(
    [ValidateSet("all", "acceptance", "drift")]
    [string]$Mode = "all",
    [string]$OutDir,
    [int]$Frames = 1,
    [switch]$NoBuild,
    [switch]$Build,
    [switch]$DryRun,
    [string]$ExecutablePath,
    [string]$GameDir,
    [string]$RunLabelPrefix = "query-info-regression",
    [switch]$RequireDriftGate,
    [switch]$Public,
    [switch]$LAN,
    [switch]$RealClient,
    [switch]$Connect,
    [switch]$PostConnect,
    [switch]$Signon
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
if (Test-Path Variable:PSNativeCommandUseErrorActionPreference) {
    $PSNativeCommandUseErrorActionPreference = $false
}

$script:WrapperPromptId = "HL-CL-20260504-294-dedicated-goldsrc-hlds-query-info-regression-rerun-command-wrapper"
$script:AcceptancePromptId = "HL-CL-20260504-292-dedicated-goldsrc-hlds-query-info-loopback-regression-acceptance-gate"
$script:DriftPromptId = "HL-CL-20260504-293-dedicated-goldsrc-hlds-query-info-regression-ci-manifest-and-fixture-drift-gate"
$script:CompatibilityClaimLevel = "diagnostic-query-info-rerun-wrapper-only; no real Steam Half-Life or HLDS-compatible client compatibility claimed"
$script:SelectedFixtureId = "connectionless_query_info_candidate"
$script:SelectedFixtureStage = "connectionless_query"

function Resolve-RepoRoot {
    $scriptDirectory = Split-Path -Parent $PSCommandPath
    return [System.IO.Path]::GetFullPath((Join-Path $scriptDirectory ".."))
}

function Resolve-PathFromRepo {
    param(
        [string]$RepoRoot,
        [string]$PathValue,
        [string]$FallbackRelativePath
    )

    $candidate = if ([string]::IsNullOrWhiteSpace($PathValue)) {
        Join-Path $RepoRoot $FallbackRelativePath
    } else {
        $PathValue
    }

    return [System.IO.Path]::GetFullPath($candidate)
}

function Resolve-MsBuild {
    $candidates = @(
        "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe",
        "C:\Program Files\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe",
        "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe",
        "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe"
    )

    foreach ($candidate in $candidates) {
        if (Test-Path -LiteralPath $candidate -PathType Leaf) {
            return $candidate
        }
    }

    $command = Get-Command MSBuild.exe -ErrorAction SilentlyContinue
    if ($null -ne $command -and -not [string]::IsNullOrWhiteSpace($command.Source)) {
        return $command.Source
    }

    throw "MSBuild.exe was not found. Use -NoBuild with an existing executable or install Visual Studio build tools."
}

function Assert-SafeRequest {
    if ($Build -and $NoBuild) {
        throw "build_mode_conflict"
    }
    if ($Frames -lt 1 -or $Frames -gt 1000) {
        throw "invalid_frame_count"
    }
    if ($Public) {
        throw "public_socket_blocked"
    }
    if ($LAN) {
        throw "lan_socket_blocked"
    }
    if ($RealClient) {
        throw "real_client_mode_blocked"
    }
    if ($Connect -or $PostConnect -or $Signon) {
        throw "query_info_only_wrapper"
    }
    if ($RequireDriftGate -and $Mode -eq "acceptance") {
        throw "drift_gate_required"
    }
}

function New-Step {
    param(
        [string]$Name,
        [string]$PromptId,
        [string]$RunLabel,
        [string]$LogDir,
        [string[]]$Arguments,
        [string]$SummaryPrefix
    )

    return [ordered]@{
        name = $Name
        prompt_id = $PromptId
        run_label = $RunLabel
        log_dir = $LogDir
        command = @($script:ResolvedExecutablePath) + $Arguments
        summary_prefix = $SummaryPrefix
        exit_code = $null
        output_log = $null
        summary_log = $null
        accepted = $null
        rejected = $null
        passed = $false
    }
}

function Convert-FieldsFromSummaryLine {
    param(
        [string]$Line,
        [string]$Prefix
    )

    $marker = $Prefix + ": "
    $index = $Line.IndexOf($marker, [System.StringComparison]::Ordinal)
    if ($index -lt 0) {
        throw ("Summary line does not contain expected prefix: {0}" -f $Prefix)
    }

    $payload = $Line.Substring($index + $marker.Length)
    $fields = [ordered]@{}
    foreach ($part in ($payload -split ", ")) {
        $keyValue = $part -split "=", 2
        if ($keyValue.Count -eq 2) {
            $fields[$keyValue[0]] = $keyValue[1]
        }
    }

    return $fields
}

function Get-StepSummaryFields {
    param($Step)

    $matches = @(
        Get-ChildItem -LiteralPath $Step.log_dir -Filter ("*{0}*_summary.log" -f $Step.run_label) -File |
            Sort-Object LastWriteTime -Descending
    )
    if ($matches.Count -eq 0) {
        throw ("No summary log found for {0} in {1}." -f $Step.name, $Step.log_dir)
    }

    $summaryLog = $matches[0].FullName
    $summaryLine = (
        Select-String -LiteralPath $summaryLog -Pattern ([regex]::Escape($Step.summary_prefix + ":")) |
            Select-Object -Last 1
    ).Line
    if ([string]::IsNullOrWhiteSpace($summaryLine)) {
        throw ("No {0} summary line found in {1}." -f $Step.summary_prefix, $summaryLog)
    }

    $fields = Convert-FieldsFromSummaryLine -Line $summaryLine -Prefix $Step.summary_prefix
    return [pscustomobject]@{
        SummaryLog = $summaryLog
        Fields = $fields
    }
}

function Assert-RequiredField {
    param(
        $Fields,
        [string]$Name,
        [string]$Expected
    )

    if (-not $Fields.Contains($Name)) {
        throw ("Required field '{0}' is missing." -f $Name)
    }
    if ($Fields[$Name] -ne $Expected) {
        throw ("Required field '{0}' expected '{1}', found '{2}'." -f $Name, $Expected, $Fields[$Name])
    }
}

function Assert-AcceptanceFields {
    param($Fields)

    Assert-RequiredField $Fields "accepted" "1"
    Assert-RequiredField $Fields "rejected" "0"
    Assert-RequiredField $Fields "query_info_regression_acceptance_passed" "1"
    Assert-RequiredField $Fields "selected_fixture_id" $script:SelectedFixtureId
    Assert-RequiredField $Fields "selected_fixture_stage" $script:SelectedFixtureStage
    Assert-RequiredField $Fields "public_socket_opened" "0"
    Assert-RequiredField $Fields "lan_socket_opened" "0"
    Assert-RequiredField $Fields "real_query_client_used" "0"
    Assert-RequiredField $Fields "real_client_binary_invoked" "0"
    Assert-RequiredField $Fields "connect_datagram_sent" "0"
    Assert-RequiredField $Fields "connect_path_invoked" "0"
    Assert-RequiredField $Fields "post_connect_serverinfo_path_invoked" "0"
    Assert-RequiredField $Fields "signon_serverinfo_path_invoked" "0"
    Assert-RequiredField $Fields "normal_host_behavior_changed" "0"
}

function Assert-DriftFields {
    param($Fields)

    Assert-RequiredField $Fields "accepted" "1"
    Assert-RequiredField $Fields "rejected" "0"
    Assert-RequiredField $Fields "drift_gate_passed" "1"
    Assert-RequiredField $Fields "query_info_regression_boundary_intact" "1"
    Assert-RequiredField $Fields "selected_fixture_id" $script:SelectedFixtureId
    Assert-RequiredField $Fields "selected_fixture_stage" $script:SelectedFixtureStage
    Assert-RequiredField $Fields "fixture_drift_detected" "0"
    Assert-RequiredField $Fields "public_socket_opened" "0"
    Assert-RequiredField $Fields "loopback_udp_socket_opened" "0"
    Assert-RequiredField $Fields "real_client_binary_invoked" "0"
    Assert-RequiredField $Fields "connect_path_blocked" "1"
    Assert-RequiredField $Fields "post_connect_stage_blocked" "1"
    Assert-RequiredField $Fields "signon_stage_blocked" "1"
    Assert-RequiredField $Fields "normal_host_behavior_changed" "0"
}

function Write-JsonFile {
    param(
        [string]$Path,
        $Value
    )

    $json = $Value | ConvertTo-Json -Depth 20
    $encoding = [System.Text.UTF8Encoding]::new($false)
    [System.IO.File]::WriteAllText($Path, $json + [Environment]::NewLine, $encoding)
}

function Convert-BoolToInt {
    param([bool]$Value)

    if ($Value) {
        return 1
    }

    return 0
}

function Invoke-Step {
    param($Step)

    New-Item -ItemType Directory -Force -Path $Step.log_dir | Out-Null
    $outputPath = Join-Path $Step.log_dir ($Step.run_label + "_wrapper_stdout.log")
    $output = & $script:ResolvedExecutablePath @($Step.command[1..($Step.command.Count - 1)]) 2>&1
    $exitCode = $LASTEXITCODE
    $output | ForEach-Object { [string]$_ } | Set-Content -LiteralPath $outputPath -Encoding utf8

    $Step.exit_code = $exitCode
    $Step.output_log = $outputPath
    if ($exitCode -ne 0) {
        throw ("{0} failed with exit code {1}." -f $Step.name, $exitCode)
    }

    $summary = Get-StepSummaryFields -Step $Step
    $Step.summary_log = $summary.SummaryLog
    $Step.accepted = $summary.Fields["accepted"]
    $Step.rejected = $summary.Fields["rejected"]

    if ($Step.name -eq "acceptance") {
        Assert-AcceptanceFields -Fields $summary.Fields
    } elseif ($Step.name -eq "drift") {
        Assert-DriftFields -Fields $summary.Fields
    } else {
        throw ("Unknown step name: {0}" -f $Step.name)
    }

    $Step.passed = $true
    return $summary.Fields
}

Assert-SafeRequest

$repoRoot = Resolve-RepoRoot
$script:ResolvedExecutablePath = Resolve-PathFromRepo `
    -RepoRoot $repoRoot `
    -PathValue $ExecutablePath `
    -FallbackRelativePath "build32/host/Debug/hlhost.exe"
$resolvedGameDir = Resolve-PathFromRepo `
    -RepoRoot $repoRoot `
    -PathValue $GameDir `
    -FallbackRelativePath "logs/latest/HL-CL-20260411-160-dedicated-goldsrc-signon-carried-checkpoint-claimed-checkpoint-resume-allow-surface/runtime/valve-fixture"
$resolvedOutDir = Resolve-PathFromRepo `
    -RepoRoot $repoRoot `
    -PathValue $OutDir `
    -FallbackRelativePath "logs/latest/HL-CL-20260504-294-dedicated-goldsrc-hlds-query-info-regression-rerun-command-wrapper/wrapper"

New-Item -ItemType Directory -Force -Path $resolvedOutDir | Out-Null

if ($Build) {
    $msBuild = Resolve-MsBuild
    $project = Join-Path $repoRoot "build32/host/hlhost.vcxproj"
    & $msBuild $project "/p:Configuration=Debug" "/p:Platform=Win32" "/m:1" "/v:minimal" "/nologo"
    if ($LASTEXITCODE -ne 0) {
        throw ("MSBuild failed with exit code {0}." -f $LASTEXITCODE)
    }
}

if (-not (Test-Path -LiteralPath $script:ResolvedExecutablePath -PathType Leaf)) {
    throw ("hlhost executable not found: {0}. Run with -Build or provide -ExecutablePath." -f $script:ResolvedExecutablePath)
}
if (-not (Test-Path -LiteralPath $resolvedGameDir -PathType Container)) {
    throw ("diagnostic game directory not found: {0}" -f $resolvedGameDir)
}

$steps = New-Object System.Collections.Generic.List[object]
if ($Mode -eq "all" -or $Mode -eq "acceptance") {
    $acceptanceLogDir = Join-Path $resolvedOutDir "acceptance"
    $acceptanceRunLabel = $RunLabelPrefix + "-acceptance-happy"
    $acceptanceArgs = @(
        "--dedicated",
        "--gamedir", $resolvedGameDir,
        "--maxclients", "4",
        "--frames", ([string]$Frames),
        "--run-label", $acceptanceRunLabel,
        "--prompt-id", $script:AcceptancePromptId,
        "--log-dir", $acceptanceLogDir,
        "--log-summary-file", "1",
        "--log-console-level", "error",
        "--hlds-query-info-loopback-regression-acceptance-probe",
        "--hlds-query-info-loopback-regression-acceptance-probe-scenario", "happy"
    )
    $steps.Add((New-Step `
        -Name "acceptance" `
        -PromptId $script:AcceptancePromptId `
        -RunLabel $acceptanceRunLabel `
        -LogDir $acceptanceLogDir `
        -Arguments $acceptanceArgs `
        -SummaryPrefix "hlds_query_info_loopback_regression_acceptance"))
}
if ($Mode -eq "all" -or $Mode -eq "drift") {
    $driftLogDir = Join-Path $resolvedOutDir "drift"
    $driftRunLabel = $RunLabelPrefix + "-drift-happy"
    $driftArgs = @(
        "--dedicated",
        "--gamedir", $resolvedGameDir,
        "--maxclients", "4",
        "--frames", ([string]$Frames),
        "--run-label", $driftRunLabel,
        "--prompt-id", $script:DriftPromptId,
        "--log-dir", $driftLogDir,
        "--log-summary-file", "1",
        "--log-console-level", "error",
        "--hlds-query-info-regression-ci-manifest-drift-gate-probe",
        "--hlds-query-info-regression-ci-manifest-drift-gate-probe-scenario", "happy"
    )
    $steps.Add((New-Step `
        -Name "drift" `
        -PromptId $script:DriftPromptId `
        -RunLabel $driftRunLabel `
        -LogDir $driftLogDir `
        -Arguments $driftArgs `
        -SummaryPrefix "hlds_query_info_regression_ci_manifest_drift_gate"))
}

$dryRunFlag = Convert-BoolToInt -Value $DryRun.IsPresent
$fullRunFlag = Convert-BoolToInt -Value (-not $DryRun.IsPresent)
$acceptanceIncluded = Convert-BoolToInt -Value ($Mode -eq "all" -or $Mode -eq "acceptance")
$driftIncluded = Convert-BoolToInt -Value ($Mode -eq "all" -or $Mode -eq "drift")
$plannedSteps = @()
foreach ($step in $steps) {
    $plannedSteps += $step
}

$commandPlan = [ordered]@{
    prompt_id = $script:WrapperPromptId
    compatibility_claim_level = $script:CompatibilityClaimLevel
    mode = $Mode
    diagnostic_only = 1
    dry_run = $dryRunFlag
    wrapper_script = $PSCommandPath
    executable_path = $script:ResolvedExecutablePath
    game_dir = $resolvedGameDir
    out_dir = $resolvedOutDir
    frames = $Frames
    acceptance_gate_included = $acceptanceIncluded
    drift_gate_included = $driftIncluded
    selected_fixture_id = $script:SelectedFixtureId
    selected_fixture_stage = $script:SelectedFixtureStage
    unsafe_modes_blocked = @("Public", "LAN", "RealClient", "Connect", "PostConnect", "Signon")
    steps = $plannedSteps
}

$planPath = Join-Path $resolvedOutDir "query_info_rerun_command_plan.json"
Write-JsonFile -Path $planPath -Value $commandPlan

$summary = [ordered]@{
    prompt_id = $script:WrapperPromptId
    compatibility_claim_level = $script:CompatibilityClaimLevel
    mode = $Mode
    wrapper_script_created = 1
    wrapper_script_file = $PSCommandPath
    wrapper_documented = 1
    wrapper_diagnostic_only = 1
    wrapper_dry_run_supported = 1
    wrapper_dry_run_passed = $dryRunFlag
    wrapper_full_run_executed = $fullRunFlag
    wrapper_full_run_passed = 0
    not_run_with_reason = if ($DryRun) { "dry_run_only" } else { "<none>" }
    acceptance_gate_included = $acceptanceIncluded
    drift_gate_included = $driftIncluded
    selected_fixture_id = $script:SelectedFixtureId
    selected_fixture_stage = $script:SelectedFixtureStage
    public_socket_opened = 0
    lan_socket_opened = 0
    real_client_binary_invoked = 0
    connect_path_invoked = 0
    post_connect_serverinfo_path_invoked = 0
    signon_serverinfo_path_invoked = 0
    normal_host_behavior_changed = 0
    command_plan_file = $planPath
    steps = $plannedSteps
}

if (-not $DryRun) {
    foreach ($step in $steps) {
        $fields = Invoke-Step -Step $step
        if ($fields.Contains("public_socket_opened")) {
            $summary.public_socket_opened = [int]$fields["public_socket_opened"]
        }
        if ($fields.Contains("lan_socket_opened")) {
            $summary.lan_socket_opened = [int]$fields["lan_socket_opened"]
        }
        if ($fields.Contains("real_client_binary_invoked")) {
            $summary.real_client_binary_invoked = [int]$fields["real_client_binary_invoked"]
        }
        if ($fields.Contains("connect_path_invoked")) {
            $summary.connect_path_invoked = [int]$fields["connect_path_invoked"]
        }
        if ($fields.Contains("post_connect_serverinfo_path_invoked")) {
            $summary.post_connect_serverinfo_path_invoked = [int]$fields["post_connect_serverinfo_path_invoked"]
        }
        if ($fields.Contains("signon_serverinfo_path_invoked")) {
            $summary.signon_serverinfo_path_invoked = [int]$fields["signon_serverinfo_path_invoked"]
        }
        if ($fields.Contains("normal_host_behavior_changed")) {
            $summary.normal_host_behavior_changed = [int]$fields["normal_host_behavior_changed"]
        }
    }

    $summary["wrapper_full_run_passed"] =
        Convert-BoolToInt -Value (@($steps | Where-Object { -not $_.passed }).Count -eq 0)
}

$summaryPath = Join-Path $resolvedOutDir "query_info_rerun_wrapper_summary.json"
Write-JsonFile -Path $summaryPath -Value $summary

$commandLogPath = Join-Path $resolvedOutDir "query_info_rerun_command_log.txt"
$commandLogLines = New-Object System.Collections.Generic.List[string]
$commandLogLines.Add("prompt_id=$script:WrapperPromptId")
$commandLogLines.Add("compatibility_claim_level=$script:CompatibilityClaimLevel")
$commandLogLines.Add("mode=$Mode")
$commandLogLines.Add("dry_run=$([int]$DryRun.IsPresent)")
foreach ($step in $steps) {
    $commandLogLines.Add(("step={0}" -f $step.name))
    $commandLogLines.Add(("command={0}" -f ($step.command -join " ")))
    $commandLogLines.Add(("exit_code={0}" -f $step.exit_code))
    $commandLogLines.Add(("summary_log={0}" -f $step.summary_log))
}
$commandLogLines | Set-Content -LiteralPath $commandLogPath -Encoding utf8

Write-Host ("query/info regression wrapper summary: {0}" -f $summaryPath)
Write-Host ("query/info regression command plan: {0}" -f $planPath)
