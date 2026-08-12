[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$ExecutablePath,

    [Parameter(Mandatory = $true)]
    [string]$GameDir,

    [ValidateRange(60, 900)]
    [int]$TimeoutSeconds = 780,

    [ValidateRange(12000, 20000)]
    [int]$SnapshotCount = 12000,

    [ValidateRange(10, 30)]
    [single]$SnapshotRateHz = 20.0
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
if (Test-Path Variable:PSNativeCommandUseErrorActionPreference) {
    $PSNativeCommandUseErrorActionPreference = $false
}

$script:pmoveLongrunStage = 'input_validation'
$script:pmoveLongrunReason = 'proof_gate_failed'

function Stop-GoldsrcPmoveLongrunProofSafely {
    param(
        [string]$Stage,
        [string]$Reason
    )

    $allowedStages = @(
        'input_validation',
        'external_proof',
        'child_input_validation',
        'child_fixture_validation',
        'child_missing_usercmd_fixture',
        'child_malformed_fixture',
        'child_positive_host_run',
        'child_negative_host_run',
        'child_repository_integrity',
        'child_result_validation',
        'child_unknown',
        'summary_validation',
        'duration_validation',
        'cadence_validation'
    )
    $allowedReasons = @(
        'dependency_unavailable',
        'external_proof_failed',
        'cleanup_failed',
        'timeout',
        'host_lifecycle_failed',
        'required_input_unavailable',
        'repository_integrity_failed',
        'summary_gate_failed',
        'resource_gate_failed',
        'snapshot_gate_failed',
        'movement_gate_failed',
        'bootstrap_gate_failed',
        'transport_gate_failed',
        'shutdown_failed',
        'proof_gate_failed',
        'summary_missing_or_duplicate',
        'summary_malformed',
        'semantic_gate_failed',
        'duration_gate_failed',
        'cadence_gate_failed'
    )
    $safeStage = if ($allowedStages -ccontains $Stage) {
        $Stage
    } else {
        'unknown'
    }
    $safeReason = if ($allowedReasons -ccontains $Reason) {
        $Reason
    } else {
        'proof_gate_failed'
    }
    Write-Host (
        'goldsrc_pmove_longrun_probe: result=fail stage={0} reason={1}' -f
            $safeStage,
            $safeReason)
    exit 1
}

trap {
    Stop-GoldsrcPmoveLongrunProofSafely `
        -Stage $script:pmoveLongrunStage `
        -Reason $script:pmoveLongrunReason
}

function Get-LongrunField {
    param(
        [string]$Line,
        [string]$Name
    )

    $pattern = '(?:^|[, ])' + [regex]::Escape($Name) +
        '=(?<value>[^, \r\n]+)(?=$|[, ])'
    $matches = [regex]::Matches(
        $Line,
        $pattern,
        [System.Text.RegularExpressions.RegexOptions]::CultureInvariant)
    if ($matches.Count -ne 1) {
        Stop-GoldsrcPmoveLongrunProofSafely `
            -Stage $script:pmoveLongrunStage `
            -Reason $script:pmoveLongrunReason
    }
    return $matches[0].Groups['value'].Value
}

$proofScript = Join-Path $PSScriptRoot 'run_goldsrc_delta_description_proof.ps1'
if (-not (Test-Path -LiteralPath $proofScript -PathType Leaf)) {
    Stop-GoldsrcPmoveLongrunProofSafely `
        -Stage 'input_validation' `
        -Reason 'dependency_unavailable'
}
$requiredDurationSeconds = 600
$childGuardSeconds = 10
$childMinimumDurationSeconds =
    $requiredDurationSeconds + $childGuardSeconds
[int]$requiredServerSnapshotCount = [Math]::Max(
    $SnapshotCount,
    [Math]::Floor(
        $requiredDurationSeconds * [double]$SnapshotRateHz))
[int]$childMinimumSnapshotCount = [Math]::Max(
    $SnapshotCount + [Math]::Ceiling(
        $childGuardSeconds * [double]$SnapshotRateHz),
    [Math]::Ceiling(
        $childMinimumDurationSeconds * [double]$SnapshotRateHz) + 1)
if ($childMinimumSnapshotCount -gt 20000) {
    Stop-GoldsrcPmoveLongrunProofSafely `
        -Stage 'input_validation' `
        -Reason 'proof_gate_failed'
}
$script:pmoveLongrunReason = 'dependency_unavailable'
$windowsPowerShell = (Get-Command powershell.exe -ErrorAction Stop).Source
$proofArguments = @(
    '-NoProfile',
    '-ExecutionPolicy', 'Bypass',
    '-File', $proofScript,
    '-ExecutablePath', $ExecutablePath,
    '-GameDir', $GameDir,
    '-TimeoutSeconds', [string]$TimeoutSeconds,
    '-PostResourceCommandProof',
    '-WorldBaselineProof',
    '-FirstSnapshotProof',
    '-ContinuousSnapshotProof',
    '-PlayerLifecycleProof',
    '-PmoveProof',
    '-PersistentPmoveProof',
    # The child stops when either its duration or decoded-frame target is
    # reached, while this wrapper intentionally requires both minimums.  Give
    # the child a bounded margin so an exact-boundary stop cannot undercut the
    # unchanged 600-second/server-cadence acceptance gates below.
    '-PersistentPmoveDurationSeconds',
        [string]$childMinimumDurationSeconds,
    '-ContinuousSnapshotMinimumCount',
        [string]$childMinimumSnapshotCount,
    '-SnapshotRateHz', $SnapshotRateHz.ToString(
        [Globalization.CultureInfo]::InvariantCulture),
    '-ExpectedZMaximum', '6300',
    '-ExpectedCdTrack', '3',
    '-SkipServerOutput'
)

$script:pmoveLongrunStage = 'external_proof'
$script:pmoveLongrunReason = 'external_proof_failed'
$proofOutput = & $windowsPowerShell @proofArguments 2>&1 | Out-String
if ($LASTEXITCODE -ne 0) {
    $safeChildFailures = @(
        [regex]::Matches(
            $proofOutput,
            '(?m)^goldsrc_delta_description_probe: result=fail ' +
                'stage=(?<stage>input_validation|fixture_validation|' +
                'missing_usercmd_fixture|malformed_fixture|' +
                'positive_host_run|negative_host_run|' +
                'repository_integrity|result_validation|unknown) ' +
                'reason=(?<reason>cleanup_failed|timeout|' +
                'host_lifecycle_failed|required_input_unavailable|' +
                'repository_integrity_failed|summary_gate_failed|' +
                'resource_gate_failed|snapshot_gate_failed|' +
                'movement_gate_failed|bootstrap_gate_failed|' +
                'transport_gate_failed|shutdown_failed|' +
                'proof_gate_failed)\r?$')
    )
    if ($safeChildFailures.Count -eq 1) {
        Stop-GoldsrcPmoveLongrunProofSafely `
            -Stage ('child_' +
                $safeChildFailures[0].Groups['stage'].Value) `
            -Reason $safeChildFailures[0].Groups['reason'].Value
    }
    Stop-GoldsrcPmoveLongrunProofSafely `
        -Stage 'external_proof' `
        -Reason 'external_proof_failed'
}
$script:pmoveLongrunStage = 'summary_validation'
$script:pmoveLongrunReason = 'summary_missing_or_duplicate'
$observationLines = @(
    $proofOutput -split '[\r\n]+' |
        Where-Object {
            $_ -like 'goldsrc_pmove_longrun_observation:*'
        })
if ($observationLines.Count -ne 1) {
    Stop-GoldsrcPmoveLongrunProofSafely `
        -Stage 'summary_validation' `
        -Reason 'summary_missing_or_duplicate'
}
$observation = $observationLines[0]

$required = [ordered]@{
    permanent_rejection_cascade = 'false'
    non_move_sequence_gap_replays = '0'
    backup_recovery = 'pass'
    snapshot_cadence = 'pass'
    movement_discontinuities = '0'
    command_execution_after_60_seconds = 'true'
    command_execution_after_120_seconds = 'true'
    command_execution_after_300_seconds = 'true'
    command_execution_after_600_seconds = 'true'
    clean_shutdown = '1'
    proof = 'pass'
}
foreach ($entry in $required.GetEnumerator()) {
    $script:pmoveLongrunReason = 'summary_malformed'
    if ((Get-LongrunField -Line $observation -Name $entry.Key) -cne
        $entry.Value) {
        Stop-GoldsrcPmoveLongrunProofSafely `
            -Stage 'summary_validation' `
            -Reason 'semantic_gate_failed'
    }
}
$script:pmoveLongrunStage = 'duration_validation'
$script:pmoveLongrunReason = 'summary_malformed'
$durationText = Get-LongrunField `
    -Line $observation `
    -Name 'longrun_duration_seconds'
[double]$duration = 0.0
if (-not [double]::TryParse(
        $durationText,
        [Globalization.NumberStyles]::Float,
        [Globalization.CultureInfo]::InvariantCulture,
        [ref]$duration)) {
    Stop-GoldsrcPmoveLongrunProofSafely `
        -Stage 'duration_validation' `
        -Reason 'summary_malformed'
}
if ($duration -lt [double]$requiredDurationSeconds) {
    Stop-GoldsrcPmoveLongrunProofSafely `
        -Stage 'duration_validation' `
        -Reason 'duration_gate_failed'
}
$script:pmoveLongrunStage = 'cadence_validation'
$script:pmoveLongrunReason = 'summary_malformed'
$serverSnapshotsText = Get-LongrunField `
    -Line $observation `
    -Name 'server_snapshots_sent'
[int]$serverSnapshotsSent = 0
if (-not [int]::TryParse(
        $serverSnapshotsText,
        [Globalization.NumberStyles]::Integer,
        [Globalization.CultureInfo]::InvariantCulture,
        [ref]$serverSnapshotsSent)) {
    Stop-GoldsrcPmoveLongrunProofSafely `
        -Stage 'cadence_validation' `
        -Reason 'summary_malformed'
}
if ($serverSnapshotsSent -lt $requiredServerSnapshotCount) {
    Stop-GoldsrcPmoveLongrunProofSafely `
        -Stage 'cadence_validation' `
        -Reason 'cadence_gate_failed'
}

$payload = $observation.Substring(
    'goldsrc_pmove_longrun_observation:'.Length).Trim()
Write-Host "goldsrc_pmove_longrun_summary: $payload"
