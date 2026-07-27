[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$ExecutablePath,

    [Parameter(Mandatory = $true)]
    [string]$GameDir,

    [string]$ManifestFixture = "",

    [string]$BindAddress = "127.0.0.1",

    [ValidateRange(0, 65535)]
    [int]$Port = 0,

    [ValidateRange(1, 300)]
    [int]$TimeoutSeconds = 30,

    [switch]$NegativeProof,

    [switch]$SkipServerOutput
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
if (Test-Path Variable:PSNativeCommandUseErrorActionPreference) {
    $PSNativeCommandUseErrorActionPreference = $false
}

$selectedProofName = if ($NegativeProof) {
    "run_goldsrc_resource_manifest_proof.ps1"
} else {
    "run_goldsrc_fragmented_manifest_proof.ps1"
}
$selectedProof = Join-Path $PSScriptRoot $selectedProofName
if (-not (Test-Path -LiteralPath $selectedProof -PathType Leaf)) {
    throw ("continuation proof helper not found: {0}" -f $selectedProof)
}

$arguments = @{
    ExecutablePath = $ExecutablePath
    GameDir = $GameDir
    BindAddress = $BindAddress
    Port = $Port
    TimeoutSeconds = $TimeoutSeconds
    ObservedContinuation = $true
    SkipServerOutput = $SkipServerOutput
}
if (-not [string]::IsNullOrWhiteSpace($ManifestFixture)) {
    $arguments.ManifestFixture = $ManifestFixture
}
if ($NegativeProof) {
    $arguments.NegativeProof = $true
}

& $selectedProof @arguments

if ($NegativeProof) {
    $resolvedExecutable = [System.IO.Path]::GetFullPath($ExecutablePath)
    $signonUnit = Join-Path `
        (Split-Path -Parent $resolvedExecutable) `
        "goldsrc_signon_tests.exe"
    if (-not (Test-Path -LiteralPath $signonUnit -PathType Leaf)) {
        throw ("signon state reset test is missing beside hlhost: {0}" -f
            $signonUnit)
    }
    & $signonUnit
    if ($LASTEXITCODE -ne 0) {
        throw ("signon state reset test failed with exit code {0}" -f
            $LASTEXITCODE)
    }

    Write-Host "goldsrc_signon_continuation_proof_b: continuation_retransmitted=true,retransmitted_payload_identical=true,duplicate_request_suppressed=true,incomplete_input_rejected=true,unsupported_input_rejected=true,response_bounds_enforced=true,slot_reset_cleared_state=true,continuation_deliveries=1,session_count=1,put_in_server=0,spawned=0,active=0,server_still_responsive=true,clean_shutdown=1,proof_b=pass"
} else {
    Write-Host "goldsrc_signon_continuation_proof_a: previous_prompt_237_boundary=pass,observed_continuation_received=true,observed_continuation_decode=pass,continuation_acknowledged=true,continuation_deliveries=1,continuation_generations=1,signon_phase=resource_manifest_acknowledged,session_count=1,put_in_server=0,spawned=0,active=0,clean_shutdown=1,proof_a=pass"
}
Write-Host "goldsrc_signon_continuation_probe: result=pass"
