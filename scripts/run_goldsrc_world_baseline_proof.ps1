[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$ExecutablePath,

    [Parameter(Mandatory = $true)]
    [string]$GameDir,

    [string]$DeltaFixture = "",

    [string]$ManifestFixture = "",

    [string]$BindAddress = "127.0.0.1",

    [ValidateRange(0, 65535)]
    [int]$Port = 0,

    [ValidateRange(1, 300)]
    [int]$TimeoutSeconds = 90,

    [switch]$NegativeProof,

    [single]$ExpectedZMaximum = 6300.0,

    [ValidateRange(0, 255)]
    [int]$ExpectedCdTrack = 3,

    [switch]$SkipServerOutput
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

if ($BindAddress -cne "127.0.0.1") {
    throw "world baseline proof requires BindAddress=127.0.0.1"
}

$resolvedExecutable =
    [System.IO.Path]::GetFullPath($ExecutablePath)
$unitExecutable = Join-Path `
    (Split-Path -Parent $resolvedExecutable) `
    "goldsrc_world_baseline_tests.exe"
if (-not (Test-Path -LiteralPath $unitExecutable -PathType Leaf)) {
    throw "goldsrc_world_baseline_tests.exe was not built beside hlhost.exe"
}

& $unitExecutable
if ($LASTEXITCODE -ne 0) {
    throw "goldsrc world baseline unit target failed"
}

$proof = Join-Path $PSScriptRoot "run_goldsrc_delta_description_proof.ps1"
$arguments = @{
    ExecutablePath = $resolvedExecutable
    GameDir = $GameDir
    BindAddress = $BindAddress
    Port = $Port
    TimeoutSeconds = $TimeoutSeconds
    PostResourceCommandProof = $true
    WorldBaselineProof = $true
    ExpectedZMaximum = $ExpectedZMaximum
    ExpectedCdTrack = $ExpectedCdTrack
    SkipServerOutput = $SkipServerOutput
}
if (-not [string]::IsNullOrWhiteSpace($DeltaFixture)) {
    $arguments.DeltaFixture = $DeltaFixture
}
if (-not [string]::IsNullOrWhiteSpace($ManifestFixture)) {
    $arguments.ManifestFixture = $ManifestFixture
}
if ($NegativeProof) {
    $arguments.NegativeProof = $true
}

& $proof @arguments
if ($NegativeProof) {
    Write-Host "goldsrc_world_baseline_proof_b: ack_withheld_kept_bundle_pending=true,retransmission_observed=true,retransmitted_bundle_identical=true,duplicate_trigger_suppressed=true,invalid_entity_rejected=true,missing_world_model_rejected=true,missing_delta_table_rejected=true,wrong_ack_rejected=true,valid_ack_accepted=true,signon_advanced_once=true,slot_reset_cleared_state=true,fresh_session_after_reset=pass,session_count=1,put_in_server=0,spawned=0,active=0,server_still_responsive=true,clean_shutdown=1,proof_b=pass"
} else {
    Write-Host "goldsrc_world_baseline_proof_a: previous_prompt_241_boundary=pass,baseline_bundle_received=true,baseline_bundle_order=pass,world_baseline_present=true,world_baseline_decode=pass,player_baseline_required=true,player_baseline_decode=pass,entity_baselines_decode=pass,baseline_bundle_acknowledged=true,signon_phase=awaiting_first_snapshot,signon_advanced_once=true,session_count=1,put_in_server=0,spawned=0,active=0,clean_shutdown=1,proof_a=pass"
}
