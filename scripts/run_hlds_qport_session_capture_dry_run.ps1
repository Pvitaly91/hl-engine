param(
    [ValidateSet("plan", "validate")]
    [string]$Mode = "plan",
    [string]$OutDir,
    [int]$Frames = 1,
    [switch]$NoBuild,
    [switch]$Build,
    [string]$ExecutablePath,
    [string]$GameDir,
    [string]$RunLabelPrefix = "qport-session-capture-dry-run",
    [switch]$Strict,
    [switch]$OmitDryRunValidator,
    [switch]$Public,
    [switch]$LAN,
    [switch]$RealClient,
    [switch]$Steam,
    [switch]$Capture,
    [switch]$Socket,
    [switch]$Connect,
    [switch]$PostConnect,
    [switch]$Signon,
    [switch]$Netchan,
    [switch]$Auth,
    [switch]$Admission
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
if (Test-Path Variable:PSNativeCommandUseErrorActionPreference) {
    $PSNativeCommandUseErrorActionPreference = $false
}

$script:PromptId = "HL-CL-20260504-309-dedicated-goldsrc-hlds-qport-session-no-client-capture-dry-run-wrapper"
$script:OfflineFixtureValidatorPromptId = "HL-CL-20260504-305-dedicated-goldsrc-hlds-qport-session-offline-fixture-validator-probe"
$script:CapturePolicyGatePromptId = "HL-CL-20260504-306-dedicated-goldsrc-hlds-qport-session-no-client-capture-policy-gate"
$script:DryRunValidatorPromptId = "HL-CL-20260504-308-dedicated-goldsrc-hlds-qport-session-capture-preflight-dry-run-validator"
$script:CompatibilityClaimLevel = "diagnostic-qport-session-capture-dry-run-wrapper-only; no real Steam Half-Life or HLDS-compatible client compatibility claimed"
$script:DryRunManifestPath = "fixtures/diagnostic/hlds/qport_session/qport_session_capture_preflight_dry_run_manifest.json"
$script:PolicyFilePath = "fixtures/diagnostic/hlds/qport_session/qport_session_offline_fixture_manifest_policy.json"
$script:FixtureRoot = "fixtures/diagnostic/hlds/qport_session"
$script:BlockedReason = "capture_implementation_not_allowed_yet"

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

function Convert-BoolToInt {
    param([bool]$Value)

    if ($Value) {
        return 1
    }

    return 0
}

function Write-JsonFile {
    param(
        [string]$Path,
        $Value
    )

    $json = $Value | ConvertTo-Json -Depth 30
    $encoding = [System.Text.UTF8Encoding]::new($false)
    [System.IO.File]::WriteAllText($Path, $json + [Environment]::NewLine, $encoding)
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

function Assert-CommonSafetyFields {
    param($Fields)

    Assert-RequiredField $Fields "real_client_binary_invoked" "0"
    Assert-RequiredField $Fields "socket_open_attempted" "0"
    Assert-RequiredField $Fields "public_socket_opened" "0"
    Assert-RequiredField $Fields "lan_socket_opened" "0"
    Assert-RequiredField $Fields "loopback_udp_socket_opened" "0"
    Assert-RequiredField $Fields "connect_path_invoked" "0"
    Assert-RequiredField $Fields "post_connect_serverinfo_path_invoked" "0"
    Assert-RequiredField $Fields "signon_serverinfo_path_invoked" "0"
    Assert-RequiredField $Fields "netchan_runtime_started" "0"
    Assert-RequiredField $Fields "normal_host_behavior_changed" "0"
}

function Assert-OfflineFixtureValidatorFields {
    param($Fields)

    Assert-RequiredField $Fields "accepted" "1"
    Assert-RequiredField $Fields "rejected" "0"
    Assert-RequiredField $Fields "fixture_validation_passed" "1"
    Assert-RequiredField $Fields "qport_session_byte_evidence_sufficient" "0"
    Assert-RequiredField $Fields "byte_level_qport_session_evidence_sufficient" "0"
    Assert-RequiredField $Fields "capture_executed" "0"
    Assert-CommonSafetyFields -Fields $Fields
}

function Assert-CapturePolicyGateFields {
    param($Fields)

    Assert-RequiredField $Fields "accepted" "1"
    Assert-RequiredField $Fields "rejected" "0"
    Assert-RequiredField $Fields "policy_gate_passed" "1"
    Assert-RequiredField $Fields "offline_fixture_validator_invoked" "1"
    Assert-RequiredField $Fields "offline_fixture_validator_passed" "1"
    Assert-RequiredField $Fields "capture_allowed_now" "0"
    Assert-RequiredField $Fields "capture_blocked_by_policy" "1"
    Assert-RequiredField $Fields "capture_block_reason" $script:BlockedReason
    Assert-RequiredField $Fields "capture_implementation_added" "0"
    Assert-RequiredField $Fields "capture_executed" "0"
    Assert-RequiredField $Fields "capture_runtime_executed" "0"
    Assert-RequiredField $Fields "qport_session_byte_evidence_sufficient" "0"
    Assert-RequiredField $Fields "byte_level_qport_session_evidence_sufficient" "0"
    Assert-CommonSafetyFields -Fields $Fields
}

function Assert-DryRunValidatorFields {
    param($Fields)

    Assert-RequiredField $Fields "accepted" "1"
    Assert-RequiredField $Fields "rejected" "0"
    Assert-RequiredField $Fields "dry_run_manifest_validated" "1"
    Assert-RequiredField $Fields "planned_artifact_schema_validated" "1"
    Assert-RequiredField $Fields "planned_actions_safe" "1"
    Assert-RequiredField $Fields "capture_allowed_now" "0"
    Assert-RequiredField $Fields "capture_blocked_by_policy" "1"
    Assert-RequiredField $Fields "capture_block_reason" $script:BlockedReason
    Assert-RequiredField $Fields "capture_implementation_added" "0"
    Assert-RequiredField $Fields "capture_executed" "0"
    Assert-RequiredField $Fields "capture_runtime_executed" "0"
    Assert-RequiredField $Fields "dry_run_only" "1"
    Assert-RequiredField $Fields "qport_session_byte_evidence_sufficient" "0"
    Assert-RequiredField $Fields "byte_level_qport_session_evidence_sufficient" "0"
    Assert-CommonSafetyFields -Fields $Fields
}

function Get-UnsafeRejectReason {
    if ($Public) { return "public_option_blocked" }
    if ($LAN) { return "lan_option_blocked" }
    if ($RealClient) { return "real_client_option_blocked" }
    if ($Steam) { return "steam_option_blocked" }
    if ($Capture) { return "capture_option_blocked" }
    if ($Socket) { return "socket_option_blocked" }
    if ($Connect) { return "connect_option_blocked" }
    if ($PostConnect) { return "post_connect_option_blocked" }
    if ($Signon) { return "signon_option_blocked" }
    if ($Netchan) { return "netchan_option_blocked" }
    if ($Auth) { return "auth_option_blocked" }
    if ($Admission) { return "admission_option_blocked" }
    if ($OmitDryRunValidator) { return "dry_run_validator_required" }
    return $null
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

    if ($Step.name -eq "offline_fixture_validator") {
        Assert-OfflineFixtureValidatorFields -Fields $summary.Fields
    } elseif ($Step.name -eq "capture_policy_gate") {
        Assert-CapturePolicyGateFields -Fields $summary.Fields
    } elseif ($Step.name -eq "dry_run_validator") {
        Assert-DryRunValidatorFields -Fields $summary.Fields
    } else {
        throw ("Unknown step name: {0}" -f $Step.name)
    }

    $Step.passed = $true
    return $summary.Fields
}

function Read-DryRunManifest {
    param([string]$RepoRoot)

    $manifestPath = Join-Path $RepoRoot $script:DryRunManifestPath
    if (-not (Test-Path -LiteralPath $manifestPath -PathType Leaf)) {
        throw ("dry-run manifest not found: {0}" -f $manifestPath)
    }

    return Get-Content -Raw -LiteralPath $manifestPath | ConvertFrom-Json
}

function New-WrapperSummary {
    param(
        [string]$ModeValue,
        [string]$ResolvedOutDir,
        [string]$PlanPath,
        [int]$PlanGenerated,
        [int]$PlanPassed,
        [int]$ValidateExecuted,
        [int]$ValidatePassed,
        [int]$UnsafeOptionsBlocked,
        [string]$RejectReason
    )

    return [ordered]@{
        prompt_id = $script:PromptId
        branch = $null
        pre_change_head = $null
        source_changed = 1
        source_commit = "pending"
        artifact_commit = "pending"
        compatibility_claim_level = $script:CompatibilityClaimLevel
        mode = $ModeValue
        accepted = if ([string]::IsNullOrWhiteSpace($RejectReason)) { 1 } else { 0 }
        rejected = if ([string]::IsNullOrWhiteSpace($RejectReason)) { 0 } else { 1 }
        last_reject_reason = if ([string]::IsNullOrWhiteSpace($RejectReason)) { "<none>" } else { $RejectReason }
        wrapper_script_created = 1
        wrapper_script_file = $PSCommandPath
        wrapper_documented = 1
        wrapper_usage_file = "docs/diagnostic/hlds/qport_session_capture_dry_run_wrapper.md"
        wrapper_diagnostic_only = 1
        wrapper_plan_mode_supported = 1
        wrapper_plan_generated = $PlanGenerated
        wrapper_plan_passed = $PlanPassed
        wrapper_validate_mode_supported = 1
        wrapper_validate_executed = $ValidateExecuted
        wrapper_validate_passed = $ValidatePassed
        offline_fixture_validator_included = 1
        capture_policy_gate_included = 1
        dry_run_validator_included = if ($OmitDryRunValidator) { 0 } else { 1 }
        dry_run_manifest_path = $script:DryRunManifestPath
        policy_file_path = $script:PolicyFilePath
        capture_allowed_now = 0
        capture_blocked_by_policy = 1
        capture_block_reason = $script:BlockedReason
        capture_implementation_added = 0
        capture_executed = 0
        capture_runtime_executed = 0
        qport_session_byte_evidence_sufficient = 0
        byte_level_qport_session_evidence_sufficient = 0
        real_client_capture_allowed_now = 0
        real_steam_client_used = 0
        real_client_binary_invoked = 0
        socket_open_attempted = 0
        public_socket_opened = 0
        lan_socket_opened = 0
        loopback_udp_socket_opened = 0
        connect_path_invoked = 0
        post_connect_serverinfo_path_invoked = 0
        signon_serverinfo_path_invoked = 0
        netchan_runtime_started = 0
        normal_host_behavior_changed = 0
        unsafe_options_blocked = $UnsafeOptionsBlocked
        command_plan_file = $PlanPath
        out_dir = $ResolvedOutDir
        recommended_next_prompt_id = "HL-CL-20260504-310-dedicated-goldsrc-hlds-qport-session-capture-dry-run-release-boundary-summary"
        recommended_next_task = "summarize the qport/session no-client capture dry-run release boundary after wrapper plan and validate modes pass while capture remains blocked"
        changed_files = @(
            "scripts/run_hlds_qport_session_capture_dry_run.ps1",
            "docs/diagnostic/hlds/qport_session_capture_dry_run_wrapper.md"
        )
        commands_run = @()
    }
}

if ($Build -and $NoBuild) {
    throw "build_mode_conflict"
}
if ($Frames -lt 1 -or $Frames -gt 1000) {
    throw "invalid_frame_count"
}

$repoRoot = Resolve-RepoRoot
$script:ResolvedExecutablePath = Resolve-PathFromRepo `
    -RepoRoot $repoRoot `
    -PathValue $ExecutablePath `
    -FallbackRelativePath "build32/host/Debug/hlhost.exe"
$resolvedGameDir = Resolve-PathFromRepo `
    -RepoRoot $repoRoot `
    -PathValue $GameDir `
    -FallbackRelativePath "logs/latest/HL-CL-20260409-119-dedicated-goldsrc-signon-envelope-surface/runtime/valve-fixture"
$resolvedOutDir = Resolve-PathFromRepo `
    -RepoRoot $repoRoot `
    -PathValue $OutDir `
    -FallbackRelativePath "logs/latest/HL-CL-20260504-309-dedicated-goldsrc-hlds-qport-session-no-client-capture-dry-run-wrapper/wrapper"
$resolvedRuntimeLogRoot = Resolve-PathFromRepo `
    -RepoRoot $repoRoot `
    -PathValue "" `
    -FallbackRelativePath "logs/latest/runtime"

New-Item -ItemType Directory -Force -Path $resolvedOutDir | Out-Null
New-Item -ItemType Directory -Force -Path $resolvedRuntimeLogRoot | Out-Null

$offlineValidatorLogDir = Join-Path $resolvedRuntimeLogRoot ($RunLabelPrefix + "-offline-fixture-validator")
$policyGateLogDir = Join-Path $resolvedRuntimeLogRoot ($RunLabelPrefix + "-capture-policy-gate")
$dryRunValidatorLogDir = Join-Path $resolvedRuntimeLogRoot ($RunLabelPrefix + "-dry-run-validator")
$offlineValidatorRunLabel = $RunLabelPrefix + "-offline-fixture-validator-happy"
$policyGateRunLabel = $RunLabelPrefix + "-capture-policy-gate-happy"
$dryRunValidatorRunLabel = $RunLabelPrefix + "-dry-run-validator-happy"

$commonHostArgs = @(
    "--dedicated",
    "--gamedir", $resolvedGameDir,
    "--maxclients", "4",
    "--frames", ([string]$Frames),
    "--log-summary-file", "1",
    "--log-console-level", "error"
)

$steps = New-Object System.Collections.Generic.List[object]
$steps.Add((New-Step `
    -Name "offline_fixture_validator" `
    -PromptId $script:OfflineFixtureValidatorPromptId `
    -RunLabel $offlineValidatorRunLabel `
    -LogDir $offlineValidatorLogDir `
    -Arguments (@($commonHostArgs) + @(
        "--run-label", $offlineValidatorRunLabel,
        "--prompt-id", $script:OfflineFixtureValidatorPromptId,
        "--log-dir", $offlineValidatorLogDir,
        "--hlds-qport-session-offline-fixture-validator-probe",
        "--hlds-qport-session-offline-fixture-validator-probe-scenario", "happy"
    )) `
    -SummaryPrefix "hlds_qport_session_offline_fixture_validator"))
$steps.Add((New-Step `
    -Name "capture_policy_gate" `
    -PromptId $script:CapturePolicyGatePromptId `
    -RunLabel $policyGateRunLabel `
    -LogDir $policyGateLogDir `
    -Arguments (@($commonHostArgs) + @(
        "--run-label", $policyGateRunLabel,
        "--prompt-id", $script:CapturePolicyGatePromptId,
        "--log-dir", $policyGateLogDir,
        "--hlds-qport-session-no-client-capture-policy-gate-probe",
        "--hlds-qport-session-no-client-capture-policy-gate-probe-scenario", "happy"
    )) `
    -SummaryPrefix "hlds_qport_session_no_client_capture_policy_gate"))
if (-not $OmitDryRunValidator) {
    $steps.Add((New-Step `
        -Name "dry_run_validator" `
        -PromptId $script:DryRunValidatorPromptId `
        -RunLabel $dryRunValidatorRunLabel `
        -LogDir $dryRunValidatorLogDir `
        -Arguments (@($commonHostArgs) + @(
            "--run-label", $dryRunValidatorRunLabel,
            "--prompt-id", $script:DryRunValidatorPromptId,
            "--log-dir", $dryRunValidatorLogDir,
            "--hlds-qport-session-capture-preflight-dry-run-validator-probe",
            "--hlds-qport-session-capture-preflight-dry-run-validator-probe-scenario", "happy"
        )) `
        -SummaryPrefix "hlds_qport_session_capture_preflight_dry_run_validator"))
}

$plannedSteps = @()
foreach ($step in $steps) {
    $plannedSteps += $step
}

$commandPlanPath = Join-Path $resolvedOutDir "qport_session_capture_dry_run_command_plan.json"
$commandPlan = [ordered]@{
    prompt_id = $script:PromptId
    compatibility_claim_level = $script:CompatibilityClaimLevel
    mode = $Mode
    diagnostic_only = 1
    wrapper_script = $PSCommandPath
    executable_path = $script:ResolvedExecutablePath
    game_dir = $resolvedGameDir
    out_dir = $resolvedOutDir
    frames = $Frames
    dry_run_manifest_path = $script:DryRunManifestPath
    policy_file_path = $script:PolicyFilePath
    fixture_root = $script:FixtureRoot
    offline_fixture_validator_included = 1
    capture_policy_gate_included = 1
    dry_run_validator_included = if ($OmitDryRunValidator) { 0 } else { 1 }
    capture_allowed_now = 0
    capture_blocked_by_policy = 1
    capture_block_reason = $script:BlockedReason
    forbidden_options = @(
        "Public",
        "LAN",
        "RealClient",
        "Steam",
        "Capture",
        "Socket",
        "Connect",
        "PostConnect",
        "Signon",
        "Netchan",
        "Auth",
        "Admission"
    )
    steps = $plannedSteps
}
Write-JsonFile -Path $commandPlanPath -Value $commandPlan

$unsafeRejectReason = Get-UnsafeRejectReason
$summary = New-WrapperSummary `
    -ModeValue $Mode `
    -ResolvedOutDir $resolvedOutDir `
    -PlanPath $commandPlanPath `
    -PlanGenerated 1 `
    -PlanPassed 0 `
    -ValidateExecuted 0 `
    -ValidatePassed 0 `
    -UnsafeOptionsBlocked 1 `
    -RejectReason $unsafeRejectReason

$commandLogPath = Join-Path $resolvedOutDir "qport_session_capture_dry_run_command_log.txt"
$summaryPath = Join-Path $resolvedOutDir "qport_session_capture_dry_run_wrapper_summary.json"

if (-not [string]::IsNullOrWhiteSpace($unsafeRejectReason)) {
    $summary["wrapper_plan_passed"] = 0
    $summary["commands_run"] = @("refused_before_execution:$unsafeRejectReason")
    Write-JsonFile -Path $summaryPath -Value $summary
    @(
        "prompt_id=$script:PromptId",
        "mode=$Mode",
        "rejected=1",
        "last_reject_reason=$unsafeRejectReason",
        "command_plan_file=$commandPlanPath",
        "summary_file=$summaryPath"
    ) | Set-Content -LiteralPath $commandLogPath -Encoding utf8
    throw $unsafeRejectReason
}

$manifest = Read-DryRunManifest -RepoRoot $repoRoot
if ($manifest.capture_allowed_now -ne $false) {
    throw "qport_session_capture_not_allowed_now"
}
if ($manifest.capture_blocked_by_policy -ne $true) {
    throw "qport_session_capture_must_remain_blocked"
}

$summary["wrapper_plan_passed"] = 1
$summary["commands_run"] = @("plan_generated")

if ($Mode -eq "validate") {
    if ($Build) {
        $msBuild = Resolve-MsBuild
        $project = Join-Path $repoRoot "build32/host/hlhost.vcxproj"
        & $msBuild $project "/p:Configuration=Debug" "/p:Platform=Win32" "/m:1" "/nr:false" "/v:minimal" "/nologo"
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

    $summary["wrapper_validate_executed"] = 1
    foreach ($step in $steps) {
        $fields = Invoke-Step -Step $step
        if ($fields.Contains("capture_allowed_now") -and $fields["capture_allowed_now"] -ne "0") {
            throw "qport_session_capture_not_allowed_now"
        }
        foreach ($fieldName in @(
            "capture_allowed_now",
            "capture_blocked_by_policy",
            "capture_implementation_added",
            "capture_executed",
            "capture_runtime_executed",
            "qport_session_byte_evidence_sufficient",
            "byte_level_qport_session_evidence_sufficient"
        )) {
            if ($fields.Contains($fieldName)) {
                $summary[$fieldName] = [int]$fields[$fieldName]
            }
        }
        foreach ($fieldName in @(
            "real_client_binary_invoked",
            "socket_open_attempted",
            "public_socket_opened",
            "lan_socket_opened",
            "loopback_udp_socket_opened",
            "connect_path_invoked",
            "post_connect_serverinfo_path_invoked",
            "signon_serverinfo_path_invoked",
            "netchan_runtime_started",
            "normal_host_behavior_changed"
        )) {
            if ($fields.Contains($fieldName)) {
                $summary[$fieldName] = [int]$fields[$fieldName]
            }
        }
    }

    $failedSteps = @($steps | Where-Object { -not $_.passed })
    $summary["wrapper_validate_passed"] = Convert-BoolToInt -Value ($failedSteps.Count -eq 0)
    $summary["commands_run"] = @("plan_generated") + @($steps | ForEach-Object { "executed_step:$($_.name)" })
}

if ($Strict) {
    foreach ($requiredPath in @(
        $script:DryRunManifestPath,
        $script:PolicyFilePath,
        "docs/diagnostic/hlds/qport_session_capture_preflight_dry_run_plan.md",
        "docs/diagnostic/hlds/qport_session_capture_dry_run_wrapper.md"
    )) {
        if (-not (Test-Path -LiteralPath (Join-Path $repoRoot $requiredPath))) {
            throw ("required path missing: {0}" -f $requiredPath)
        }
    }
}

Write-JsonFile -Path $summaryPath -Value $summary

$commandLogLines = New-Object System.Collections.Generic.List[string]
$commandLogLines.Add("prompt_id=$script:PromptId")
$commandLogLines.Add("compatibility_claim_level=$script:CompatibilityClaimLevel")
$commandLogLines.Add("mode=$Mode")
$commandLogLines.Add("command_plan_file=$commandPlanPath")
$commandLogLines.Add("summary_file=$summaryPath")
foreach ($step in $steps) {
    $commandLogLines.Add(("step={0}" -f $step.name))
    $commandLogLines.Add(("command={0}" -f ($step.command -join " ")))
    $commandLogLines.Add(("exit_code={0}" -f $step.exit_code))
    $commandLogLines.Add(("summary_log={0}" -f $step.summary_log))
}
$commandLogLines | Set-Content -LiteralPath $commandLogPath -Encoding utf8

Write-Host ("qport/session capture dry-run wrapper summary: {0}" -f $summaryPath)
Write-Host ("qport/session capture dry-run command plan: {0}" -f $commandPlanPath)
