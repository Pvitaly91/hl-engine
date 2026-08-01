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
        throw "long-run proof omitted a required semantic field"
    }
    return $matches[0].Groups['value'].Value
}

$proofScript = Join-Path $PSScriptRoot 'run_goldsrc_delta_description_proof.ps1'
if (-not (Test-Path -LiteralPath $proofScript -PathType Leaf)) {
    throw "long-run proof dependency is missing"
}
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
    '-PersistentPmoveDurationSeconds', '600',
    '-ContinuousSnapshotMinimumCount', [string]$SnapshotCount,
    '-SnapshotRateHz', $SnapshotRateHz.ToString(
        [Globalization.CultureInfo]::InvariantCulture),
    '-ExpectedZMaximum', '6300',
    '-ExpectedCdTrack', '3',
    '-SkipServerOutput'
)

$proofOutput = & $windowsPowerShell @proofArguments 2>&1 | Out-String
if ($LASTEXITCODE -ne 0) {
    throw "localhost long-run PM_Move proof failed"
}
$observationLines = @(
    $proofOutput -split '[\r\n]+' |
        Where-Object {
            $_ -like 'goldsrc_pmove_longrun_observation:*'
        })
if ($observationLines.Count -ne 1) {
    throw "localhost long-run PM_Move proof returned no unique summary"
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
    if ((Get-LongrunField -Line $observation -Name $entry.Key) -cne
        $entry.Value) {
        throw "localhost long-run PM_Move proof failed a semantic gate"
    }
}
$duration = [double]::Parse(
    (Get-LongrunField -Line $observation -Name 'longrun_duration_seconds'),
    [Globalization.CultureInfo]::InvariantCulture)
if ($duration -lt 600.0) {
    throw "localhost long-run PM_Move proof ended before 600 seconds"
}
$serverSnapshotsSent = [int]::Parse(
    (Get-LongrunField -Line $observation -Name 'server_snapshots_sent'),
    [Globalization.CultureInfo]::InvariantCulture)
if ($serverSnapshotsSent -lt [Math]::Floor(600.0 * $SnapshotRateHz)) {
    throw "localhost long-run PM_Move proof missed the server cadence target"
}

$payload = $observation.Substring(
    'goldsrc_pmove_longrun_observation:'.Length).Trim()
Write-Host "goldsrc_pmove_longrun_summary: $payload"
