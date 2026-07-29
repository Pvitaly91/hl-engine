[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$ExecutablePath,

    [Parameter(Mandatory = $true)]
    [string]$GameDir,

    [string]$BindAddress = "127.0.0.1",

    [ValidateRange(0, 65535)]
    [int]$Port = 0,

    [ValidateRange(30, 300)]
    [int]$TimeoutSeconds = 120,

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
    throw "player-lifecycle proofs are restricted to 127.0.0.1"
}

$driver = Join-Path $PSScriptRoot "run_goldsrc_delta_description_proof.ps1"
if (-not (Test-Path -LiteralPath $driver -PathType Leaf)) {
    throw "player-lifecycle proof driver is missing"
}
$resolvedExecutable = [System.IO.Path]::GetFullPath($ExecutablePath)
if (-not (Test-Path -LiteralPath $resolvedExecutable -PathType Leaf)) {
    throw "player-lifecycle host executable is missing"
}
$lifecycleTests = Join-Path `
    (Split-Path -Parent $resolvedExecutable) `
    "goldsrc_player_lifecycle_tests.exe"
if (-not (Test-Path -LiteralPath $lifecycleTests -PathType Leaf)) {
    throw "player-lifecycle unit executable is missing"
}

# The bounded fixture validates rejection, duplicate, rollback, view reset,
# private-data reset, and two-slot ownership before the external host run.
& $lifecycleTests
if ($LASTEXITCODE -ne 0) {
    throw "player-lifecycle bounded unit validation failed"
}

$repoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$parameters = @{
    ExecutablePath = $resolvedExecutable
    GameDir = $GameDir
    DeltaFixture = Join-Path `
        $repoRoot `
        "src/tests/fixtures/goldsrc_delta_description_minimal.lst"
    FragmentedDeltaFixture = Join-Path `
        $repoRoot `
        "src/tests/fixtures/goldsrc_delta_description_fragmented.lst"
    ManifestFixture = Join-Path `
        $repoRoot `
        "src/tests/fixtures/goldsrc_resource_manifest_minimal.tsv"
    BindAddress = $BindAddress
    Port = $Port
    TimeoutSeconds = $TimeoutSeconds
    NegativeProof = [bool]$NegativeProof
    PostResourceCommandProof = $true
    WorldBaselineProof = $true
    FirstSnapshotProof = $true
    ContinuousSnapshotProof = $true
    PlayerLifecycleProof = $true
    ContinuousSnapshotMinimumCount = $(if ($NegativeProof) {
        [Math]::Max(300, $SnapshotCount)
    } else {
        $SnapshotCount
    })
    SnapshotRateHz = $SnapshotRateHz
    ExpectedZMaximum = $ExpectedZMaximum
    ExpectedCdTrack = $ExpectedCdTrack
    SkipServerOutput = [bool]$SkipServerOutput
}
& $driver @parameters
if (-not $?) {
    throw "player-lifecycle external proof driver failed"
}
