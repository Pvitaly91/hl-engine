[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$ExecutablePath,

    [Parameter(Mandatory = $true)]
    [string]$GameDir,

    [string]$DeltaFixture = "",

    [string]$FragmentedDeltaFixture = "",

    [string]$ManifestFixture = "",

    [string]$BindAddress = "127.0.0.1",

    [ValidateRange(0, 65535)]
    [int]$Port = 0,

    [ValidateRange(1, 300)]
    [int]$TimeoutSeconds = 60,

    [single]$ExpectedZMaximum = 4096.0,

    [ValidateRange(0, 255)]
    [int]$ExpectedCdTrack = 0,

    [switch]$NegativeProof,

    [switch]$SkipServerOutput
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$proof = Join-Path $PSScriptRoot "run_goldsrc_delta_description_proof.ps1"
$arguments = @{
    ExecutablePath = $ExecutablePath
    GameDir = $GameDir
    BindAddress = $BindAddress
    Port = $Port
    TimeoutSeconds = $TimeoutSeconds
    PostResourceCommandProof = $true
    ExpectedZMaximum = $ExpectedZMaximum
    ExpectedCdTrack = $ExpectedCdTrack
    SkipServerOutput = $SkipServerOutput
}
if (-not [string]::IsNullOrWhiteSpace($DeltaFixture)) {
    $arguments.DeltaFixture = $DeltaFixture
}
if (-not [string]::IsNullOrWhiteSpace($FragmentedDeltaFixture)) {
    $arguments.FragmentedDeltaFixture = $FragmentedDeltaFixture
}
if (-not [string]::IsNullOrWhiteSpace($ManifestFixture)) {
    $arguments.ManifestFixture = $ManifestFixture
}
if ($NegativeProof) {
    $arguments.NegativeProof = $true
    $arguments.PostResourceNegativeProof = $true
}

& $proof @arguments
