[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$ExecutablePath,

    [Parameter(Mandatory = $true)]
    [string]$GameDir,

    [string]$BindAddress = "127.0.0.1",

    [ValidateRange(0, 65535)]
    [int]$Port = 0,

    [ValidateRange(15, 300)]
    [int]$TimeoutSeconds = 90,

    [ValidateRange(30, 512)]
    [int]$SnapshotCount = 30,

    [ValidateRange(10, 30)]
    [single]$SnapshotRateHz = 20.0,

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
    throw "continuous-snapshot proofs are restricted to 127.0.0.1"
}

$driver = Join-Path $PSScriptRoot "run_goldsrc_delta_description_proof.ps1"
if (-not (Test-Path -LiteralPath $driver -PathType Leaf)) {
    throw "continuous-snapshot proof driver is missing"
}
$resolvedExecutable = [System.IO.Path]::GetFullPath($ExecutablePath)
if (-not (Test-Path -LiteralPath $resolvedExecutable -PathType Leaf)) {
    throw "continuous-snapshot host executable is missing"
}
$snapshotTests = Join-Path `
    (Split-Path -Parent $resolvedExecutable) `
    "goldsrc_continuous_snapshot_tests.exe"
if (-not (Test-Path -LiteralPath $snapshotTests -PathType Leaf)) {
    throw "continuous-snapshot unit executable is missing"
}
& $snapshotTests
if ($LASTEXITCODE -ne 0) {
    throw "continuous-snapshot bounded unit validation failed"
}

$repoRoot = [System.IO.Path]::GetFullPath(
    (Join-Path $PSScriptRoot "..")
)
$deltaFixture = Join-Path `
    $repoRoot `
    "src/tests/fixtures/goldsrc_delta_description_minimal.lst"
$fragmentedDeltaFixture = Join-Path `
    $repoRoot `
    "src/tests/fixtures/goldsrc_delta_description_fragmented.lst"
$manifestFixture = Join-Path `
    $repoRoot `
    "src/tests/fixtures/goldsrc_resource_manifest_minimal.tsv"

$parameters = @{
    ExecutablePath = $resolvedExecutable
    GameDir = $GameDir
    DeltaFixture = $deltaFixture
    FragmentedDeltaFixture = $fragmentedDeltaFixture
    ManifestFixture = $manifestFixture
    BindAddress = $BindAddress
    Port = $Port
    TimeoutSeconds = $TimeoutSeconds
    NegativeProof = [bool]$NegativeProof
    PostResourceCommandProof = $true
    WorldBaselineProof = $true
    FirstSnapshotProof = $true
    ContinuousSnapshotProof = $true
    ContinuousSnapshotMinimumCount = $SnapshotCount
    SnapshotRateHz = $SnapshotRateHz
    ExpectedZMaximum = $ExpectedZMaximum
    ExpectedCdTrack = $ExpectedCdTrack
    SkipServerOutput = [bool]$SkipServerOutput
}
& $driver @parameters
if (-not $?) {
    throw "continuous-snapshot external proof driver failed"
}
