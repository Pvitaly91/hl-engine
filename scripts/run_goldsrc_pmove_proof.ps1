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
    [int]$TimeoutSeconds = 180,

    [switch]$NegativeProof,

    [switch]$SkipServerOutput
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
if (Test-Path Variable:PSNativeCommandUseErrorActionPreference) {
    $PSNativeCommandUseErrorActionPreference = $false
}
$script:goldsrcPmoveProofStage = "input_validation"
trap {
    $safeStage = if (@(
            "input_validation",
            "unit_validation",
            "external_proof") -ccontains $script:goldsrcPmoveProofStage) {
        $script:goldsrcPmoveProofStage
    } else {
        "unknown"
    }
    Write-Host (
        "goldsrc_pmove_probe: result=fail stage={0} reason=proof_gate_failed" -f
            $safeStage)
    exit 1
}
if ($BindAddress -cne "127.0.0.1") {
    throw "PM_Move proofs are restricted to 127.0.0.1"
}

$resolvedExecutable = [System.IO.Path]::GetFullPath($ExecutablePath)
if (-not (Test-Path -LiteralPath $resolvedExecutable -PathType Leaf)) {
    throw "PM_Move host executable is missing"
}
$pmoveTests = Join-Path `
    (Split-Path -Parent $resolvedExecutable) `
    "goldsrc_pmove_tests.exe"
if (-not (Test-Path -LiteralPath $pmoveTests -PathType Leaf)) {
    throw "PM_Move unit executable is missing"
}

# These deterministic tests cover replay ordering, duplicate suppression,
# long-command splitting, static BSP collision, slot isolation, and rollback.
$script:goldsrcPmoveProofStage = "unit_validation"
& $pmoveTests
if ($LASTEXITCODE -ne 0) {
    throw "PM_Move bounded unit validation failed"
}

$repoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$driver = Join-Path $PSScriptRoot "run_goldsrc_delta_description_proof.ps1"
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
    PmoveProof = $true
    ContinuousSnapshotMinimumCount = $(if ($NegativeProof) {
        50
    } else {
        30
    })
    SnapshotRateHz = 20.0
    ExpectedZMaximum = 6300.0
    ExpectedCdTrack = 3
    SkipServerOutput = [bool]$SkipServerOutput
}
$script:goldsrcPmoveProofStage = "external_proof"
& $driver @parameters
$driverExitCode = if ($null -eq $LASTEXITCODE) {
    0
} else {
    [int]$LASTEXITCODE
}
if ($driverExitCode -ne 0) {
    exit $driverExitCode
}
exit 0
