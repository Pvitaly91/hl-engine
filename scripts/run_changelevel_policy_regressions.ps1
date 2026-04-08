param(
    [string]$ExecutablePath,
    [string]$GameDir,
    [string]$LogRoot,
    [string]$RunLabelPrefix,
    [string]$PromptId,
    [switch]$FinalizeHarvestIndexOnly,
    [string]$ContinuationBaselineWorkingBranch,
    [string]$ContinuationBaselineHeadBeforeChanges,
    [string]$ContinuationBaselineFinalSourceCommit,
    [string[]]$AdditionalHarvestIndexPaths
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

    & $script:ResolvedExecutablePath @CaseArguments | Out-Host
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

function Get-SingleMatchedFile {
    param(
        [string]$DirectoryPath,
        [string]$Description,
        [scriptblock]$Filter
    )

    if (-not (Test-Path -LiteralPath $DirectoryPath -PathType Container)) {
        throw ("Directory not found for {0}: {1}" -f $Description, $DirectoryPath)
    }

    $matches = @(Get-ChildItem -LiteralPath $DirectoryPath -File | Where-Object $Filter)
    if ($matches.Count -eq 0) {
        throw ("No {0} found in {1}." -f $Description, $DirectoryPath)
    }

    if ($matches.Count -ne 1) {
        throw ("Expected exactly one {0} in {1}, found {2}." -f $Description, $DirectoryPath, $matches.Count)
    }

    return $matches[0]
}

function Get-RequiredManifestStringValue {
    param(
        $Manifest,
        [string]$PropertyName
    )

    $property = $Manifest.PSObject.Properties[$PropertyName]
    if ($null -eq $property -or [string]::IsNullOrWhiteSpace([string]$property.Value)) {
        throw ("Manifest missing required string property '{0}'." -f $PropertyName)
    }

    return [string]$property.Value
}

function Get-RequiredManifestIntValue {
    param(
        $Manifest,
        [string]$PropertyName
    )

    $property = $Manifest.PSObject.Properties[$PropertyName]
    if ($null -eq $property) {
        throw ("Manifest missing required integer property '{0}'." -f $PropertyName)
    }

    return [int]$property.Value
}

function Assert-ManifestPathMatchesFileName {
    param(
        [string]$ManifestPath,
        [string]$ActualPath,
        [string]$Description
    )

    if ([string]::IsNullOrWhiteSpace($ManifestPath)) {
        throw ("Missing path for {0}." -f $Description)
    }

    $manifestFileName = [System.IO.Path]::GetFileName($ManifestPath)
    $actualFileName = [System.IO.Path]::GetFileName($ActualPath)
    if (-not [string]::Equals($manifestFileName, $actualFileName, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw ("Manifest path mismatch for {0}. Expected file name '{1}', found '{2}'." -f $Description, $actualFileName, $manifestFileName)
    }
}

function Get-RepositoryGitState {
    param([string]$RepositoryRoot)

    Push-Location $RepositoryRoot
    try {
        $workingBranch = (& git branch --show-current).Trim()
        if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($workingBranch)) {
            throw "Unable to resolve working branch."
        }

        $gitCommit = (& git rev-parse HEAD).Trim()
        if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($gitCommit)) {
            throw "Unable to resolve git commit."
        }

        return [pscustomobject]@{
            workingBranch = $workingBranch
            gitCommit = $gitCommit
        }
    }
    finally {
        Pop-Location
    }
}

function New-CanonicalHarvestIndexRunRecord {
    param($RunArtifacts)

    return [ordered]@{
        runLabel = $RunArtifacts.runLabel
        sessionId = $RunArtifacts.sessionId
        runInstanceId = $RunArtifacts.runInstanceId
        regressionGuard = $RunArtifacts.regressionGuard
        stopOnChangelevelRequest = $RunArtifacts.stopOnChangelevelRequest
        sourceRuntimeFile = $RunArtifacts.sourceRuntimeFile
        sourceSummaryFile = $RunArtifacts.sourceSummaryFile
        sourceManifestFile = $RunArtifacts.sourceManifestFile
        canonicalRuntimeFile = $RunArtifacts.canonicalRuntimeFile
        canonicalSummaryFile = $RunArtifacts.canonicalSummaryFile
        canonicalManifestFile = $RunArtifacts.canonicalManifestFile
    }
}

function Get-CanonicalHarvestIndexPath {
    param([string]$RunLabelPrefix)

    if ([string]::IsNullOrWhiteSpace($RunLabelPrefix)) {
        return $null
    }

    return Join-Path $script:CanonicalCodexLogDir ("changelevel_policy_regressions__{0}_canonical_harvest_index.json" -f $RunLabelPrefix)
}

function New-ContinuationBaselineMetadata {
    param(
        [string]$WorkingBranch,
        [string]$HeadBeforeChanges,
        [string]$FinalSourceCommit
    )

    return [pscustomobject][ordered]@{
        workingBranch = $WorkingBranch
        headBeforeChanges = $HeadBeforeChanges
        finalSourceCommit = $FinalSourceCommit
    }
}

function Get-CopiedManifestPathsFromHarvestIndex {
    param(
        $HarvestIndexPayload,
        [string]$IndexPath
    )

    $copiedManifestPaths = @()
    $indexDirectory = Split-Path -Parent $IndexPath
    foreach ($runRecordName in @("baseline", "stop")) {
        $runRecordProperty = $HarvestIndexPayload.PSObject.Properties[$runRecordName]
        if ($null -eq $runRecordProperty -or $null -eq $runRecordProperty.Value) {
            continue
        }

        $canonicalManifestProperty = $runRecordProperty.Value.PSObject.Properties["canonicalManifestFile"]
        if ($null -eq $canonicalManifestProperty -or [string]::IsNullOrWhiteSpace([string]$canonicalManifestProperty.Value)) {
            continue
        }

        $resolvedCanonicalManifestPath = [System.IO.Path]::GetFullPath([string]$canonicalManifestProperty.Value)
        $copiedManifestPaths += $resolvedCanonicalManifestPath

        $siblingCopiedManifestPath = Join-Path $indexDirectory ([System.IO.Path]::GetFileName($resolvedCanonicalManifestPath))
        $copiedManifestPaths += [System.IO.Path]::GetFullPath($siblingCopiedManifestPath)
    }

    return @($copiedManifestPaths | Sort-Object -Unique)
}

function Set-CopiedManifestContinuationBaseline {
    param(
        [string[]]$ManifestPaths,
        $ContinuationBaseline
    )

    foreach ($manifestPath in $ManifestPaths) {
        if ([string]::IsNullOrWhiteSpace($manifestPath)) {
            continue
        }

        $resolvedManifestPath = [System.IO.Path]::GetFullPath($manifestPath)
        if (-not (Test-Path -LiteralPath $resolvedManifestPath -PathType Leaf)) {
            throw ("Copied manifest not found for continuation baseline update: {0}" -f $resolvedManifestPath)
        }

        $manifestPayload = Get-Content -LiteralPath $resolvedManifestPath -Raw | ConvertFrom-Json
        $existingContinuationBaseline = $manifestPayload.PSObject.Properties["continuationBaseline"]
        if ($null -ne $existingContinuationBaseline) {
            $existingContinuationBaseline.Value = $ContinuationBaseline
        }
        else {
            $manifestPayload | Add-Member -NotePropertyName "continuationBaseline" -NotePropertyValue $ContinuationBaseline
        }

        $manifestPayload | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $resolvedManifestPath -Encoding utf8
    }
}

function Set-HarvestIndexContinuationBaseline {
    param(
        [string[]]$IndexPaths,
        [string]$WorkingBranch,
        [string]$HeadBeforeChanges,
        [string]$FinalSourceCommit
    )

    if ([string]::IsNullOrWhiteSpace($WorkingBranch)) {
        throw "Continuation baseline working branch is required."
    }

    if ([string]::IsNullOrWhiteSpace($HeadBeforeChanges)) {
        throw "Continuation baseline headBeforeChanges is required."
    }

    if ([string]::IsNullOrWhiteSpace($FinalSourceCommit)) {
        throw "Continuation baseline finalSourceCommit is required."
    }

    $continuationBaseline = New-ContinuationBaselineMetadata `
        -WorkingBranch $WorkingBranch `
        -HeadBeforeChanges $HeadBeforeChanges `
        -FinalSourceCommit $FinalSourceCommit

    foreach ($indexPath in $IndexPaths) {
        if ([string]::IsNullOrWhiteSpace($indexPath)) {
            continue
        }

        $resolvedIndexPath = [System.IO.Path]::GetFullPath($indexPath)
        if (-not (Test-Path -LiteralPath $resolvedIndexPath -PathType Leaf)) {
            throw ("Harvest index not found for continuation baseline update: {0}" -f $resolvedIndexPath)
        }

        $payload = Get-Content -LiteralPath $resolvedIndexPath -Raw | ConvertFrom-Json
        $existingContinuationBaseline = $payload.PSObject.Properties["continuationBaseline"]
        if ($null -ne $existingContinuationBaseline) {
            $existingContinuationBaseline.Value = $continuationBaseline
        }
        else {
            $payload | Add-Member -NotePropertyName "continuationBaseline" -NotePropertyValue $continuationBaseline
        }

        $copiedManifestPaths = Get-CopiedManifestPathsFromHarvestIndex `
            -HarvestIndexPayload $payload `
            -IndexPath $resolvedIndexPath

        $payload | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $resolvedIndexPath -Encoding utf8
        Set-CopiedManifestContinuationBaseline `
            -ManifestPaths $copiedManifestPaths `
            -ContinuationBaseline $continuationBaseline
    }
}

function Copy-RegressionArtifactsToCanonicalLatest {
    param(
        [string]$CaseLogDir,
        [string]$RunLabel
    )

    if ([string]::IsNullOrWhiteSpace($RunLabel)) {
        return $null
    }

    $runtimeLogFile = Get-SingleMatchedFile `
        -DirectoryPath $CaseLogDir `
        -Description ("runtime part log for run label '{0}'" -f $RunLabel) `
        -Filter { $_.Name -like ("*__{0}_part*.log" -f $RunLabel) }
    $summaryFile = Get-SingleMatchedFile `
        -DirectoryPath $CaseLogDir `
        -Description ("summary log for run label '{0}'" -f $RunLabel) `
        -Filter { $_.Name -like ("*__{0}_summary.log" -f $RunLabel) }
    $codexSourceDir = Join-Path (Split-Path -Parent $CaseLogDir) "codex"
    $manifestFile = Get-SingleMatchedFile `
        -DirectoryPath $codexSourceDir `
        -Description ("manifest file for run label '{0}'" -f $RunLabel) `
        -Filter { $_.Name -like ("*__{0}_manifest.json" -f $RunLabel) }
    $manifest = Get-Content -LiteralPath $manifestFile.FullName -Raw | ConvertFrom-Json
    $manifestRunLabel = Get-RequiredManifestStringValue -Manifest $manifest -PropertyName "runLabel"
    if (-not [string]::Equals($manifestRunLabel, $RunLabel, [System.StringComparison]::Ordinal)) {
        throw ("Manifest run label mismatch. Expected '{0}', found '{1}'." -f $RunLabel, $manifestRunLabel)
    }

    $manifestRuntimeFile = Get-RequiredManifestStringValue -Manifest $manifest -PropertyName "currentFilePath"
    $manifestSummaryFile = Get-RequiredManifestStringValue -Manifest $manifest -PropertyName "summaryFilePath"
    $sourceRuntimeFile = [System.IO.Path]::GetFullPath($runtimeLogFile.FullName)
    $sourceSummaryFile = [System.IO.Path]::GetFullPath($summaryFile.FullName)
    $sourceManifestFile = $manifestFile.FullName

    if (-not (Test-Path -LiteralPath $sourceRuntimeFile -PathType Leaf)) {
        throw ("Source runtime log missing for run label '{0}': {1}" -f $RunLabel, $sourceRuntimeFile)
    }

    if (-not (Test-Path -LiteralPath $sourceSummaryFile -PathType Leaf)) {
        throw ("Source summary log missing for run label '{0}': {1}" -f $RunLabel, $sourceSummaryFile)
    }

    Assert-ManifestPathMatchesFileName `
        -ManifestPath $manifestRuntimeFile `
        -ActualPath $sourceRuntimeFile `
        -Description ("runtime part log for run label '{0}'" -f $RunLabel)
    Assert-ManifestPathMatchesFileName `
        -ManifestPath $manifestSummaryFile `
        -ActualPath $sourceSummaryFile `
        -Description ("summary log for run label '{0}'" -f $RunLabel)

    New-Item -ItemType Directory -Force -Path $script:CanonicalRuntimeLogDir | Out-Null
    New-Item -ItemType Directory -Force -Path $script:CanonicalCodexLogDir | Out-Null

    $canonicalRuntimeFile = Join-Path $script:CanonicalRuntimeLogDir $runtimeLogFile.Name
    $canonicalSummaryFile = Join-Path $script:CanonicalRuntimeLogDir $summaryFile.Name
    $canonicalManifestFile = Join-Path $script:CanonicalCodexLogDir $manifestFile.Name

    foreach ($copyOperation in @(
        @{ Source = $sourceRuntimeFile; Destination = $canonicalRuntimeFile },
        @{ Source = $sourceSummaryFile; Destination = $canonicalSummaryFile },
        @{ Source = $sourceManifestFile; Destination = $canonicalManifestFile }
    )) {
        Copy-Item -LiteralPath $copyOperation.Source `
            -Destination $copyOperation.Destination `
            -Force
        if (-not (Test-Path -LiteralPath $copyOperation.Destination -PathType Leaf)) {
            throw ("Canonical harvest copy missing after copy operation for run label '{0}': {1}" -f $RunLabel, $copyOperation.Destination)
        }
    }

    return [pscustomobject]@{
        runLabel = $manifestRunLabel
        sessionId = Get-RequiredManifestStringValue -Manifest $manifest -PropertyName "sessionId"
        runInstanceId = Get-RequiredManifestStringValue -Manifest $manifest -PropertyName "runInstanceId"
        regressionGuard = Get-RequiredManifestStringValue -Manifest $manifest -PropertyName "regressionGuard"
        stopOnChangelevelRequest = Get-RequiredManifestIntValue -Manifest $manifest -PropertyName "stopOnChangelevelRequest"
        sourceRuntimeFile = $sourceRuntimeFile
        sourceSummaryFile = $sourceSummaryFile
        sourceManifestFile = $sourceManifestFile
        canonicalRuntimeFile = $canonicalRuntimeFile
        canonicalSummaryFile = $canonicalSummaryFile
        canonicalManifestFile = $canonicalManifestFile
    }
}

function Write-CanonicalHarvestIndex {
    param(
        [string]$RunLabelPrefix,
        [string]$PromptId,
        $BaselineRunArtifacts,
        $StopRunArtifacts
    )

    if ([string]::IsNullOrWhiteSpace($RunLabelPrefix)) {
        return $null
    }

    if ($null -eq $BaselineRunArtifacts) {
        throw ("Missing baseline canonical harvest artifacts for run label prefix '{0}'." -f $RunLabelPrefix)
    }

    if ($null -eq $StopRunArtifacts) {
        throw ("Missing stop canonical harvest artifacts for run label prefix '{0}'." -f $RunLabelPrefix)
    }

    $gitState = Get-RepositoryGitState -RepositoryRoot $script:RepositoryRoot
    $indexPath = Get-CanonicalHarvestIndexPath -RunLabelPrefix $RunLabelPrefix
    $payload = [ordered]@{
        runLabelPrefix = $RunLabelPrefix
        generatedByPromptId = $script:CanonicalHarvestPromptId
    }
    if (-not [string]::IsNullOrWhiteSpace($PromptId)) {
        $payload.generatedForPromptId = $PromptId
    }
    $payload.workingBranch = $gitState.workingBranch
    $payload.gitCommit = $gitState.gitCommit
    $payload.baseline = New-CanonicalHarvestIndexRunRecord -RunArtifacts $BaselineRunArtifacts
    $payload.stop = New-CanonicalHarvestIndexRunRecord -RunArtifacts $StopRunArtifacts

    $payload | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $indexPath -Encoding utf8
    if (-not (Test-Path -LiteralPath $indexPath -PathType Leaf)) {
        throw ("Canonical harvest index was not written: {0}" -f $indexPath)
    }

    return $indexPath
}

$repositoryRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$script:RepositoryRoot = $repositoryRoot
$workspaceRoot = [System.IO.Path]::GetFullPath((Join-Path $repositoryRoot ".."))
$CanonicalRuntimeLogDir = Join-Path $repositoryRoot "logs\latest\runtime"
$CanonicalCodexLogDir = Join-Path $repositoryRoot "logs\latest\codex"
$script:CanonicalHarvestPromptId = "HL-CL-20260405-094-canonical-harvest-index"
$canonicalHarvestIndexPath = Get-CanonicalHarvestIndexPath -RunLabelPrefix $RunLabelPrefix

if ($FinalizeHarvestIndexOnly) {
    if ([string]::IsNullOrWhiteSpace($RunLabelPrefix)) {
        throw "RunLabelPrefix is required when -FinalizeHarvestIndexOnly is used."
    }

    $indexPaths = @()
    if (-not [string]::IsNullOrWhiteSpace($canonicalHarvestIndexPath)) {
        $indexPaths += $canonicalHarvestIndexPath
    }

    foreach ($additionalHarvestIndexPath in $AdditionalHarvestIndexPaths) {
        if (-not [string]::IsNullOrWhiteSpace($additionalHarvestIndexPath)) {
            $indexPaths += $additionalHarvestIndexPath
        }
    }

    if ($indexPaths.Count -eq 0) {
        throw ("No harvest index paths resolved for run label prefix '{0}'." -f $RunLabelPrefix)
    }

    Set-HarvestIndexContinuationBaseline `
        -IndexPaths $indexPaths `
        -WorkingBranch $ContinuationBaselineWorkingBranch `
        -HeadBeforeChanges $ContinuationBaselineHeadBeforeChanges `
        -FinalSourceCommit $ContinuationBaselineFinalSourceCommit

    Write-Host ("Canonical harvest continuation baseline updated for run label prefix '{0}'." -f $RunLabelPrefix)
    exit 0
}

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
$promptIdArguments = @()
if (-not [string]::IsNullOrWhiteSpace($PromptId)) {
    $promptIdArguments = @("--prompt-id", $PromptId)
}

$baselineRunArtifacts = Invoke-RegressionCase `
    -CaseName "latch-only continuation" `
    -CaseLogDir (Join-Path $ResolvedLogRoot "latch_only_continuation") `
    -CaseRunLabel $baselineRunLabel `
    -CaseArguments ($commonArguments + @(
        "--log-dir", (Join-Path $ResolvedLogRoot "latch_only_continuation"),
        "--stop-on-changelevel-request", "0",
        "--regression-guard", "changelevel-latch-only-continuation"
    ) + $baselineRunLabelArguments + $promptIdArguments)

$stopRunArtifacts = Invoke-RegressionCase `
    -CaseName "focused stop verification" `
    -CaseLogDir (Join-Path $ResolvedLogRoot "focused_stop_verification") `
    -CaseRunLabel $stopRunLabel `
    -CaseArguments ($commonArguments + @(
        "--log-dir", (Join-Path $ResolvedLogRoot "focused_stop_verification"),
        "--stop-on-changelevel-request", "1",
        "--regression-guard", "changelevel-request-consumed"
    ) + $stopRunLabelArguments + $promptIdArguments)

$canonicalHarvestIndexPath = Write-CanonicalHarvestIndex `
    -RunLabelPrefix $RunLabelPrefix `
    -PromptId $PromptId `
    -BaselineRunArtifacts $baselineRunArtifacts `
    -StopRunArtifacts $stopRunArtifacts

if ($null -ne $canonicalHarvestIndexPath) {
    Write-Host ("Canonical harvest index: {0}" -f $canonicalHarvestIndexPath)
}

Write-Host "Accepted changelevel policy regressions passed."
exit 0
