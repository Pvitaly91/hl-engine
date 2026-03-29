param(
    [string]$ExecutablePath,
    [string]$GameDir,
    [string]$LogRoot
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
        [string[]]$CaseArguments
    )

    New-Item -ItemType Directory -Force -Path $CaseLogDir | Out-Null

    Write-Host ("==> {0}" -f $CaseName)
    Write-Host ("    log dir: {0}" -f $CaseLogDir)

    & $script:ResolvedExecutablePath @CaseArguments
    if ($LASTEXITCODE -ne 0) {
        throw ("{0} failed with exit code {1}." -f $CaseName, $LASTEXITCODE)
    }
}

$repositoryRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$workspaceRoot = [System.IO.Path]::GetFullPath((Join-Path $repositoryRoot ".."))
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

Invoke-RegressionCase `
    -CaseName "latch-only continuation" `
    -CaseLogDir (Join-Path $ResolvedLogRoot "latch_only_continuation") `
    -CaseArguments ($commonArguments + @(
        "--log-dir", (Join-Path $ResolvedLogRoot "latch_only_continuation"),
        "--stop-on-changelevel-request", "0",
        "--regression-guard", "changelevel-latch-only-continuation"
    ))

Invoke-RegressionCase `
    -CaseName "focused stop verification" `
    -CaseLogDir (Join-Path $ResolvedLogRoot "focused_stop_verification") `
    -CaseArguments ($commonArguments + @(
        "--log-dir", (Join-Path $ResolvedLogRoot "focused_stop_verification"),
        "--stop-on-changelevel-request", "1",
        "--regression-guard", "changelevel-request-consumed"
    ))

Write-Host "Accepted changelevel policy regressions passed."
exit 0
