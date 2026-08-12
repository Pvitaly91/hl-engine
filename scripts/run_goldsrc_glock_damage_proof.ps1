[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$ExecutablePath,

    [Parameter(Mandatory = $true)]
    [string]$GameDir,

    [string]$BindAddress = "127.0.0.1",

    [ValidateRange(0, 65535)]
    [int]$Port = 0,

    [ValidateRange(60, 900)]
    [int]$TimeoutSeconds = 300,

    [switch]$NegativeProof,

    [switch]$FeatureOffProof,

    [switch]$FallDamageProof,

    [switch]$LethalDeathProof,

    [switch]$SkipServerOutput
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
if ($BindAddress -cne "127.0.0.1") {
    throw "Glock damage proofs are restricted to 127.0.0.1"
}
$selectedProofModes = @(
    @(
        $NegativeProof
        $FeatureOffProof
        $FallDamageProof
        $LethalDeathProof) |
        Where-Object { [bool]$_ })
if ($selectedProofModes.Count -gt 1) {
    throw "Glock damage proof modes are mutually exclusive"
}

$arguments = @{
    ExecutablePath = $ExecutablePath
    GameDir = $GameDir
    BindAddress = $BindAddress
    Port = $Port
    TimeoutSeconds = $TimeoutSeconds
    CombatProof = $true
}
if ($NegativeProof) {
    $arguments.NegativeProof = $true
}
if ($FeatureOffProof) {
    $arguments.FeatureOffProof = $true
}
if ($FallDamageProof) {
    $arguments.FallDamageProof = $true
}
if ($LethalDeathProof) {
    $arguments.LethalDeathProof = $true
}
if ($SkipServerOutput) {
    $arguments.SkipServerOutput = $true
}

try {
    & (Join-Path $PSScriptRoot "run_goldsrc_two_client_replication_proof.ps1") `
        @arguments
    $proofExitCode = if ($null -eq $LASTEXITCODE) {
        0
    } else {
        [int]$LASTEXITCODE
    }
    if ($proofExitCode -ne 0) {
        exit $proofExitCode
    }
    exit 0
} catch {
    $message = [string]$_.Exception.Message
    if ($message -match
            '^two-client proof failed: stage=[a-z0-9_]+; reason=[a-z0-9_]+$') {
        Write-Host $message
    } else {
        Write-Host (
            'two-client proof failed: stage=wrapper; ' +
            'reason=proof_gate_failed')
    }
    exit 1
}
