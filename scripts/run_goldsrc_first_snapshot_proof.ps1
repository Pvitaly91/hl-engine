[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$ExecutablePath,

    [Parameter(Mandatory = $true)]
    [string]$GameDir,

    [string]$BindAddress = "127.0.0.1",

    [ValidateRange(0, 65535)]
    [int]$Port = 0,

    [ValidateRange(1, 300)]
    [int]$TimeoutSeconds = 60,

    [single]$ExpectedZMaximum = 6300.0,

    [ValidateRange(0, 255)]
    [int]$ExpectedCdTrack = 3,

    [switch]$NegativeProof,

    [switch]$SkipServerOutput
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
if (Test-Path Variable:PSNativeCommandUseErrorActionPreference) {
    $PSNativeCommandUseErrorActionPreference = $false
}
if ($BindAddress -cne "127.0.0.1") {
    throw "first-snapshot proofs are restricted to 127.0.0.1"
}

$driver = Join-Path $PSScriptRoot "run_goldsrc_delta_description_proof.ps1"
if (-not (Test-Path -LiteralPath $driver -PathType Leaf)) {
    throw "first-snapshot proof driver is missing"
}
$resolvedExecutable = [System.IO.Path]::GetFullPath($ExecutablePath)
if (-not (Test-Path -LiteralPath $resolvedExecutable -PathType Leaf)) {
    throw "first-snapshot host executable is missing"
}

if ($NegativeProof) {
    $snapshotTests = Join-Path `
        (Split-Path -Parent $resolvedExecutable) `
        "goldsrc_snapshot_tests.exe"
    if (-not (Test-Path -LiteralPath $snapshotTests -PathType Leaf)) {
        throw "first-snapshot unit executable is missing"
    }
    & $snapshotTests
    if ($LASTEXITCODE -ne 0) {
        throw "first-snapshot bounded codec/history validation failed"
    }
}

$parameters = @{
    ExecutablePath = $resolvedExecutable
    GameDir = $GameDir
    BindAddress = $BindAddress
    Port = $Port
    TimeoutSeconds = $TimeoutSeconds
    NegativeProof = [bool]$NegativeProof
    PostResourceCommandProof = $true
    WorldBaselineProof = $true
    FirstSnapshotProof = $true
    ExpectedZMaximum = $ExpectedZMaximum
    ExpectedCdTrack = $ExpectedCdTrack
    SkipServerOutput = [bool]$SkipServerOutput
}
& $driver @parameters
if (-not $?) {
    throw "first-snapshot external proof driver failed"
}
