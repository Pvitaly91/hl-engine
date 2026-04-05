param(
    [string]$ExecutablePath,
    [string]$GameDir,
    [string]$LogRoot,
    [string]$RunLabelPrefix
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
if (Test-Path Variable:PSNativeCommandUseErrorActionPreference) {
    $PSNativeCommandUseErrorActionPreference = $false
}

function Get-FullPathOrDefault {
    param(
        [string]$Candidate,
        [string]$Fallback
    )

    $value = if ([string]::IsNullOrWhiteSpace($Candidate)) { $Fallback } else { $Candidate }
    return [System.IO.Path]::GetFullPath($value)
}

function Get-ResolvedGameDir {
    param([string]$RequestedGameDir)

    if (-not [string]::IsNullOrWhiteSpace($RequestedGameDir)) {
        return [System.IO.Path]::GetFullPath($RequestedGameDir)
    }

    if (-not [string]::IsNullOrWhiteSpace($env:HLENGINE_VALVE_DIR)) {
        return [System.IO.Path]::GetFullPath($env:HLENGINE_VALVE_DIR)
    }

    $defaultGameDir = "D:\Steam\steamapps\common\Half-Life\valve"
    return [System.IO.Path]::GetFullPath($defaultGameDir)
}

function Invoke-RegressionCase {
    param(
        [string]$CaseName,
        [string]$CaseLogDir,
        [string[]]$CaseArguments,
        [string]$CaseRunLabel
    )

    New-Item -ItemType Directory -Force -Path $CaseLogDir | Out-Null

    Write-Host ("==> {0}" -f $CaseName)
    Write-Host ("    log dir: {0}" -f $CaseLogDir)

    & $script:ResolvedExecutablePath @CaseArguments
    if ($LASTEXITCODE -ne 0) {
        throw ("{0} failed with exit code {1}." -f $CaseName, $LASTEXITCODE)
    }

    Copy-RegressionArtifactsToCanonicalLatest `
        -CaseLogDir $CaseLogDir `
        -RunLabel $CaseRunLabel
}

function Get-RunLabelArguments {
    param([string]$RunLabel)

    if ([string]::IsNullOrWhiteSpace($RunLabel)) {
        return @()
    }

    return @("--run-label", $RunLabel)
}

function Copy-RegressionArtifactsToCanonicalLatest {
    param(
        [string]$CaseLogDir,
        [string]$RunLabel
    )

    if ([string]::IsNullOrWhiteSpace($RunLabel)) {
        return
    }

    $runtimeFiles = @(Get-ChildItem -LiteralPath $CaseLogDir -File | Where-Object {
        $_.Name -like ("*__{0}_part*.log" -f $RunLabel) -or
        $_.Name -like ("*__{0}_summary.log" -f $RunLabel)
    })
    if ($runtimeFiles.Count -eq 0) {
        throw ("No runtime files found for run label '{0}' in {1}." -f $RunLabel, $CaseLogDir)
    }

    $codexSourceDir = Join-Path (Split-Path -Parent $CaseLogDir) "codex"
    $manifestFiles = @()
    if (Test-Path -LiteralPath $codexSourceDir -PathType Container) {
        $manifestFiles = @(Get-ChildItem -LiteralPath $codexSourceDir -File | Where-Object {
            $_.Name -like ("*__{0}_manifest.json" -f $RunLabel)
        })
    }
    if ($manifestFiles.Count -eq 0) {
        throw ("No manifest files found for run label '{0}' in {1}." -f $RunLabel, $codexSourceDir)
    }

    New-Item -ItemType Directory -Force -Path $script:CanonicalRuntimeLogDir | Out-Null
    New-Item -ItemType Directory -Force -Path $script:CanonicalCodexLogDir | Out-Null

    foreach ($runtimeFile in $runtimeFiles) {
        Copy-Item -LiteralPath $runtimeFile.FullName `
            -Destination (Join-Path $script:CanonicalRuntimeLogDir $runtimeFile.Name) `
            -Force
    }

    foreach ($manifestFile in $manifestFiles) {
        Copy-Item -LiteralPath $manifestFile.FullName `
            -Destination (Join-Path $script:CanonicalCodexLogDir $manifestFile.Name) `
            -Force
    }
}

$repositoryRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$workspaceRoot = [System.IO.Path]::GetFullPath((Join-Path $repositoryRoot ".."))
$CanonicalRuntimeLogDir = Join-Path $repositoryRoot "logs\latest\runtime"
$CanonicalCodexLogDir = Join-Path $repositoryRoot "logs\latest\codex"
$ResolvedExecutablePath = Get-FullPathOrDefault `
    -Candidate $ExecutablePath `
    -Fallback (Join-Path $workspaceRoot "out\build\vs2022-win32\host\Debug\hlhost.exe")
$ResolvedGameDir = Get-ResolvedGameDir -RequestedGameDir $GameDir
$ResolvedLogRoot = Get-FullPathOrDefault `
    -Candidate $LogRoot `
    -Fallback (Join-Path $repositoryRoot "logs\latest\changelevel_policy_regressions")

if (-not (Test-Path -LiteralPath $ResolvedExecutablePath -PathType Leaf)) {
    throw ("hlhost executable not found: {0}" -f $ResolvedExecutablePath)
}

if (-not (Test-Path -LiteralPath $ResolvedGameDir -PathType Container)) {
    throw ("Half-Life valve directory not found: {0}`nSet HLENGINE_VALVE_DIR or pass -GameDir." -f $ResolvedGameDir)
}

$commonArguments = @(
    "--gamedir", $ResolvedGameDir,
    "--frames", "1800",
    "--frametime", "0.05",
    "--path-arrival-epsilon", "24",
    "--log-to-file", "1",
    "--log-max-mb", "10",
    "--log-summary-file", "1",
    "--log-frame-sample", "10",
    "--log-state-changes-only", "1",
    "--log-suppress-repeats", "1"
)
$baselineRunLabel = $null
$stopRunLabel = $null
if (-not [string]::IsNullOrWhiteSpace($RunLabelPrefix)) {
    $baselineRunLabel = "{0}-baseline" -f $RunLabelPrefix
    $stopRunLabel = "{0}-stop" -f $RunLabelPrefix
}
$baselineRunLabelArguments = Get-RunLabelArguments -RunLabel $baselineRunLabel
$stopRunLabelArguments = Get-RunLabelArguments -RunLabel $stopRunLabel

Invoke-RegressionCase `
    -CaseName "latch-only continuation" `
    -CaseLogDir (Join-Path $ResolvedLogRoot "latch_only_continuation") `
    -CaseRunLabel $baselineRunLabel `
    -CaseArguments ($commonArguments + @(
        "--log-dir", (Join-Path $ResolvedLogRoot "latch_only_continuation"),
        "--stop-on-changelevel-request", "0",
        "--regression-guard", "changelevel-latch-only-continuation"
    ) + $baselineRunLabelArguments)

Invoke-RegressionCase `
    -CaseName "focused stop verification" `
    -CaseLogDir (Join-Path $ResolvedLogRoot "focused_stop_verification") `
    -CaseRunLabel $stopRunLabel `
    -CaseArguments ($commonArguments + @(
        "--log-dir", (Join-Path $ResolvedLogRoot "focused_stop_verification"),
        "--stop-on-changelevel-request", "1",
        "--regression-guard", "changelevel-request-consumed"
    ) + $stopRunLabelArguments)

Write-Host "Accepted changelevel policy regressions passed."
exit 0
