param(
    [string]$RepoRoot,
    [string]$LogsRoot,
    [string]$OutputDir,
    [string]$RunLabelPattern = "verify-resumed-denial-main-*",
    [switch]$IncludeCodexArtifacts,
    [switch]$AllowMissingCodexArtifacts,
    [string]$VerificationMetadataPath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
if (Test-Path Variable:PSNativeCommandUseErrorActionPreference) {
    $PSNativeCommandUseErrorActionPreference = $false
}

$script:PromptId = "HL-CL-20260411-163-dedicated-goldsrc-signon-carried-checkpoint-claimed-checkpoint-resumed-denial-surface"
$script:ScenarioNames = @("happy", "gate", "recovery")
$script:LifecycleMarkers = @(
    "signonMessageCursorCarriedCheckpointClaimedCheckpointResumeEofReady=1",
    "claimedCheckpointBridgeResumeExhausted=1",
    "signonMessageCursorCarriedCheckpointClaimedCheckpointResumedDeniedReady=1",
    "claimedCheckpointBridgeResumedDenied=1"
)
$script:HappySurfaceMarkers = @(
    "accepted=1",
    "rejected=0",
    "resumeAllowed=no",
    "denialReason=claimant-checkpoint-resumed-exhausted",
    "eof=yes",
    "exhausted=yes",
    "nextStartMessageIndex=<none>",
    "remainingMessageCount=0"
)
$script:GateSurfaceMarkers = @(
    "accepted=1",
    "rejected=3",
    "resumeAllowed=no",
    "denialReason=claimant-checkpoint-resumed-exhausted",
    "eof=yes",
    "exhausted=yes",
    "nextStartMessageIndex=<none>",
    "remainingMessageCount=0"
)
$script:HappyProbeMarkers = @(
    "attempts=1",
    "accepted=1",
    "rejected=0",
    "lastRejectReason=<none>",
    "parsedResumePolicy=claimant-checkpoint-resumed-exhausted-denied"
)
$script:GateProbeMarkers = @(
    "attempts=4",
    "accepted=1",
    "rejected=3",
    "lastRejectReason=already-claimed-checkpoint-resumed-denied",
    "parsedResumePolicy=claimant-checkpoint-resumed-exhausted-denied"
)

function Write-Heading {
    param([string]$Text)

    Write-Host ""
    Write-Host ("== {0} ==" -f $Text)
}

function Resolve-FullPath {
    param(
        [string]$PathValue,
        [string]$BasePath = (Get-Location).Path
    )

    if ([string]::IsNullOrWhiteSpace($PathValue)) {
        return [System.IO.Path]::GetFullPath($BasePath)
    }

    if ([System.IO.Path]::IsPathRooted($PathValue)) {
        return [System.IO.Path]::GetFullPath($PathValue)
    }

    return [System.IO.Path]::GetFullPath((Join-Path $BasePath $PathValue))
}

function Get-GitSingleLine {
    param([string[]]$Arguments)

    $output = & git @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw ("git {0} failed with exit code {1}." -f ($Arguments -join " "), $LASTEXITCODE)
    }

    return ($output | Select-Object -First 1).Trim()
}

function Get-RepoRootFromRequest {
    param([string]$RequestedRoot)

    $startingPath = Resolve-FullPath $RequestedRoot
    if (-not (Test-Path -LiteralPath $startingPath -PathType Container)) {
        throw ("Repo root candidate does not exist: {0}" -f $startingPath)
    }

    Push-Location $startingPath
    try {
        $topLevel = Get-GitSingleLine @("rev-parse", "--show-toplevel")
        return [System.IO.Path]::GetFullPath($topLevel)
    }
    finally {
        Pop-Location
    }
}

function Ensure-Directory {
    param([string]$LiteralPath)

    if (-not (Test-Path -LiteralPath $LiteralPath -PathType Container)) {
        New-Item -ItemType Directory -Path $LiteralPath -Force | Out-Null
    }

    return [System.IO.Path]::GetFullPath($LiteralPath)
}

function Get-OptionalPropertyValue {
    param(
        [object]$Object,
        [string]$Name,
        $DefaultValue = $null
    )

    if ($null -eq $Object) {
        return $DefaultValue
    }

    if ($Object -is [System.Collections.IDictionary]) {
        if ($Object.Contains($Name)) {
            return $Object[$Name]
        }

        return $DefaultValue
    }

    $property = $Object.PSObject.Properties[$Name]
    if ($null -eq $property) {
        return $DefaultValue
    }

    return $property.Value
}

function Resolve-LogsLayout {
    param(
        [string]$ResolvedRepoRoot,
        [string]$RequestedLogsRoot
    )

    $candidateRoot = if ([string]::IsNullOrWhiteSpace($RequestedLogsRoot)) {
        Join-Path $ResolvedRepoRoot "logs\latest"
    }
    else {
        Resolve-FullPath -PathValue $RequestedLogsRoot -BasePath $ResolvedRepoRoot
    }

    if (-not (Test-Path -LiteralPath $candidateRoot -PathType Container)) {
        throw ("Logs root does not exist: {0}" -f $candidateRoot)
    }

    $latestRoot = $candidateRoot
    if (-not (Test-Path -LiteralPath (Join-Path $latestRoot "runtime") -PathType Container)) {
        $nestedLatestRoot = Join-Path $candidateRoot "latest"
        if (Test-Path -LiteralPath (Join-Path $nestedLatestRoot "runtime") -PathType Container) {
            $latestRoot = $nestedLatestRoot
        }
    }

    $runtimeRoot = Join-Path $latestRoot "runtime"
    if (-not (Test-Path -LiteralPath $runtimeRoot -PathType Container)) {
        throw ("Runtime logs directory was not found under {0}." -f $latestRoot)
    }

    $codexRoot = Join-Path $latestRoot "codex"
    if (-not (Test-Path -LiteralPath $codexRoot -PathType Container)) {
        $codexRoot = ""
    }

    return [pscustomobject]@{
        LatestRoot = [System.IO.Path]::GetFullPath($latestRoot)
        RuntimeRoot = [System.IO.Path]::GetFullPath($runtimeRoot)
        CodexRoot = if ([string]::IsNullOrWhiteSpace($codexRoot)) { "" } else { [System.IO.Path]::GetFullPath($codexRoot) }
    }
}

function Get-NormalizedScenarioStatus {
    param([string]$Value)

    if ([string]::IsNullOrWhiteSpace($Value)) {
        return "UNKNOWN"
    }

    $normalized = $Value.Trim().ToUpperInvariant()
    switch ($normalized) {
        "PENDING" { return "NOT_RUN" }
        default { return $normalized }
    }
}

function Read-VerificationMetadata {
    param([string]$MetadataPath)

    if ([string]::IsNullOrWhiteSpace($MetadataPath)) {
        return $null
    }

    $resolvedPath = Resolve-FullPath $MetadataPath
    if (-not (Test-Path -LiteralPath $resolvedPath -PathType Leaf)) {
        throw ("Verification metadata path does not exist: {0}" -f $resolvedPath)
    }

    $rawJson = Get-Content -LiteralPath $resolvedPath -Raw
    $metadata = $rawJson | ConvertFrom-Json
    $metadata | Add-Member -NotePropertyName "__path" -NotePropertyValue $resolvedPath -Force
    return $metadata
}

function Get-RunLabelInfo {
    param([string]$RunLabel)

    $match = [regex]::Match($RunLabel, '^(.*)-(happy|gate|recovery)$')
    if (-not $match.Success) {
        return $null
    }

    return [pscustomobject]@{
        BaseLabel = $match.Groups[1].Value
        Scenario = $match.Groups[2].Value
    }
}

function Get-RunLabelFromFileName {
    param(
        [string]$FileName,
        [string]$SuffixPattern
    )

    $match = [regex]::Match($FileName, ('__(.+)_{0}$' -f $SuffixPattern))
    if (-not $match.Success) {
        return ""
    }

    return $match.Groups[1].Value
}

function Get-SelectedRunLabels {
    param(
        [string]$RuntimeRoot,
        [string]$RunLabelPattern,
        [object]$Metadata
    )

    $labelsFromMetadata = [ordered]@{}
    $metadataRunLabels = Get-OptionalPropertyValue -Object $Metadata -Name "RunLabels"
    if ($null -ne $metadataRunLabels) {
        foreach ($scenarioName in $script:ScenarioNames) {
            $label = [string](Get-OptionalPropertyValue -Object $metadataRunLabels -Name $scenarioName -DefaultValue "")
            if (-not [string]::IsNullOrWhiteSpace($label)) {
                $labelsFromMetadata[$scenarioName] = $label
            }
        }
    }

    if ($labelsFromMetadata.Count -gt 0) {
        $baseLabel = ""
        foreach ($label in $labelsFromMetadata.Values) {
            $info = Get-RunLabelInfo -RunLabel $label
            if ($null -ne $info) {
                $baseLabel = $info.BaseLabel
                break
            }
        }

        return [pscustomobject]@{
            Source = "metadata"
            BaseLabel = $baseLabel
            RunLabels = $labelsFromMetadata
            RunLabelPattern = $RunLabelPattern
        }
    }

    if ($null -ne $Metadata) {
        return [pscustomobject]@{
            Source = "metadata-no-run-labels"
            BaseLabel = ""
            RunLabels = [ordered]@{}
            RunLabelPattern = $RunLabelPattern
        }
    }

    $groupByBaseLabel = @{}
    $summaryFiles = @(Get-ChildItem -LiteralPath $RuntimeRoot -Filter "*_summary.log" -File -ErrorAction SilentlyContinue)
    foreach ($summaryFile in $summaryFiles) {
        $runLabel = Get-RunLabelFromFileName -FileName $summaryFile.Name -SuffixPattern 'summary\.log'
        if ([string]::IsNullOrWhiteSpace($runLabel) -or $runLabel -notlike $RunLabelPattern) {
            continue
        }

        $info = Get-RunLabelInfo -RunLabel $runLabel
        if ($null -eq $info) {
            continue
        }

        if (-not $groupByBaseLabel.ContainsKey($info.BaseLabel)) {
            $groupByBaseLabel[$info.BaseLabel] = [pscustomobject]@{
                BaseLabel = $info.BaseLabel
                LastWriteTime = $summaryFile.LastWriteTime
                RunLabels = [ordered]@{}
            }
        }

        $groupByBaseLabel[$info.BaseLabel].RunLabels[$info.Scenario] = $runLabel
        if ($summaryFile.LastWriteTime -gt $groupByBaseLabel[$info.BaseLabel].LastWriteTime) {
            $groupByBaseLabel[$info.BaseLabel].LastWriteTime = $summaryFile.LastWriteTime
        }
    }

    if ($groupByBaseLabel.Count -eq 0) {
        throw ("No runtime summary logs under {0} matched run label pattern '{1}'." -f $RuntimeRoot, $RunLabelPattern)
    }

    $selectedGroup = $groupByBaseLabel.Values | Sort-Object LastWriteTime -Descending | Select-Object -First 1
    return [pscustomobject]@{
        Source = "runtime-summary-scan"
        BaseLabel = $selectedGroup.BaseLabel
        RunLabels = $selectedGroup.RunLabels
        RunLabelPattern = $RunLabelPattern
    }
}

function Get-FirstMatchingLine {
    param(
        [string[]]$Lines,
        [string]$Needle
    )

    $matches = @($Lines | Where-Object { $_ -like ("*{0}*" -f $Needle) })
    if ($matches.Count -eq 0) {
        return ""
    }

    return $matches[0]
}

function Test-LineContainsAll {
    param(
        [string]$Line,
        [string[]]$ExpectedTokens
    )

    if ([string]::IsNullOrWhiteSpace($Line)) {
        return $false
    }

    foreach ($token in $ExpectedTokens) {
        if ($Line.IndexOf($token, [System.StringComparison]::Ordinal) -lt 0) {
            return $false
        }
    }

    return $true
}

function Get-CodexRunIdentityFieldValue {
    param(
        [string]$IdentityLine,
        [string]$FieldName
    )

    $match = [regex]::Match($IdentityLine, ('\b{0}=([^,]+)' -f [regex]::Escape($FieldName)))
    if (-not $match.Success) {
        return "<missing>"
    }

    return $match.Groups[1].Value.Trim()
}

function Get-ScenarioMarkers {
    param([string]$ScenarioName)

    if ($ScenarioName -eq "gate") {
        return [pscustomobject]@{
            Surface = $script:GateSurfaceMarkers
            Probe = $script:GateProbeMarkers
        }
    }

    return [pscustomobject]@{
        Surface = $script:HappySurfaceMarkers
        Probe = $script:HappyProbeMarkers
    }
}

function Parse-ScenarioSummaryFile {
    param(
        [string]$SummaryFilePath,
        [string]$ScenarioName
    )

    $result = [ordered]@{
        Status = "FAIL"
        FailureReason = ""
        RuntimeGitBranch = "<unknown>"
        RuntimeGitCommit = "<unknown>"
        SurfaceLine = ""
        ProbeLine = ""
        LifecycleLine = ""
    }

    if ([string]::IsNullOrWhiteSpace($SummaryFilePath) -or -not (Test-Path -LiteralPath $SummaryFilePath -PathType Leaf)) {
        $result.FailureReason = "summary log missing"
        return [pscustomobject]$result
    }

    $summaryLines = @(Get-Content -LiteralPath $SummaryFilePath)
    $lifecycleLine = Get-FirstMatchingLine -Lines $summaryLines -Needle "dedicated_player_lifecycle_foundation:"
    $surfaceLine = Get-FirstMatchingLine -Lines $summaryLines -Needle "dedicated_signon_message_cursor_carried_checkpoint_claimed_checkpoint_resumed_denial_surface:"
    $probeLine = Get-FirstMatchingLine -Lines $summaryLines -Needle "dedicated_signon_message_cursor_carried_checkpoint_claimed_checkpoint_resumed_denial_probe:"
    $identityLine = Get-FirstMatchingLine -Lines $summaryLines -Needle "codex_run_identity:"

    $markers = Get-ScenarioMarkers -ScenarioName $ScenarioName
    $hasLifecycle = Test-LineContainsAll -Line $lifecycleLine -ExpectedTokens $script:LifecycleMarkers
    $hasSurface = Test-LineContainsAll -Line $surfaceLine -ExpectedTokens $markers.Surface
    $hasProbe = Test-LineContainsAll -Line $probeLine -ExpectedTokens $markers.Probe

    $result.RuntimeGitBranch = Get-CodexRunIdentityFieldValue -IdentityLine $identityLine -FieldName "gitBranch"
    $result.RuntimeGitCommit = Get-CodexRunIdentityFieldValue -IdentityLine $identityLine -FieldName "gitCommit"
    $result.SurfaceLine = $surfaceLine
    $result.ProbeLine = $probeLine
    $result.LifecycleLine = $lifecycleLine

    if ($hasLifecycle -and $hasSurface -and $hasProbe) {
        $result.Status = "PASS"
        return [pscustomobject]$result
    }

    $missingChecks = New-Object System.Collections.Generic.List[string]
    if (-not $hasLifecycle) {
        $missingChecks.Add("lifecycle markers")
    }
    if (-not $hasSurface) {
        $missingChecks.Add("surface markers")
    }
    if (-not $hasProbe) {
        $missingChecks.Add("probe markers")
    }

    $result.FailureReason = ("summary log did not satisfy {0}" -f ($missingChecks -join ", "))
    return [pscustomobject]$result
}

function Get-ScenarioArtifactFiles {
    param(
        [string]$RuntimeRoot,
        [string]$CodexRoot,
        [string]$RunLabel
    )

    if ([string]::IsNullOrWhiteSpace($RunLabel)) {
        return [pscustomobject]@{
            SummaryFile = ""
            RuntimePartFiles = @()
            CodexManifest = ""
        }
    }

    $summaryFile = @(
        Get-ChildItem -LiteralPath $RuntimeRoot -Filter ("*__{0}_summary.log" -f $RunLabel) -File -ErrorAction SilentlyContinue |
            Sort-Object LastWriteTime -Descending
    ) | Select-Object -First 1

    $runtimePartFiles = @(
        Get-ChildItem -LiteralPath $RuntimeRoot -Filter ("*__{0}_part*.log" -f $RunLabel) -File -ErrorAction SilentlyContinue |
            Sort-Object Name
    )

    $codexManifest = ""
    if (-not [string]::IsNullOrWhiteSpace($CodexRoot)) {
        $codexManifestFile = @(
            Get-ChildItem -LiteralPath $CodexRoot -Filter ("*__{0}_manifest.json" -f $RunLabel) -File -ErrorAction SilentlyContinue |
                Sort-Object LastWriteTime -Descending
        ) | Select-Object -First 1
        if ($null -ne $codexManifestFile) {
            $codexManifest = $codexManifestFile.FullName
        }
    }

    return [pscustomobject]@{
        SummaryFile = if ($null -eq $summaryFile) { "" } else { $summaryFile.FullName }
        RuntimePartFiles = @($runtimePartFiles | ForEach-Object { $_.FullName })
        CodexManifest = $codexManifest
    }
}

function Copy-FileIfDifferent {
    param(
        [string]$SourcePath,
        [string]$DestinationPath
    )

    if ([string]::IsNullOrWhiteSpace($SourcePath) -or -not (Test-Path -LiteralPath $SourcePath -PathType Leaf)) {
        return ""
    }

    $resolvedSourcePath = [System.IO.Path]::GetFullPath($SourcePath)
    $resolvedDestinationPath = [System.IO.Path]::GetFullPath($DestinationPath)
    Ensure-Directory -LiteralPath (Split-Path -Path $resolvedDestinationPath -Parent) | Out-Null
    if (-not $resolvedSourcePath.Equals($resolvedDestinationPath, [System.StringComparison]::OrdinalIgnoreCase)) {
        Copy-Item -LiteralPath $resolvedSourcePath -Destination $resolvedDestinationPath -Force
    }

    return $resolvedDestinationPath
}

function Copy-SupplementalMetadataFiles {
    param(
        [object]$Metadata,
        [string]$ResolvedOutputDir
    )

    $metadataDirectory = Ensure-Directory -LiteralPath (Join-Path $ResolvedOutputDir "metadata")
    $copiedFiles = [ordered]@{}

    $metadataPath = [string](Get-OptionalPropertyValue -Object $Metadata -Name "__path" -DefaultValue "")
    if (-not [string]::IsNullOrWhiteSpace($metadataPath)) {
        $copiedFiles.VerificationMetadata = Copy-FileIfDifferent -SourcePath $metadataPath -DestinationPath (Join-Path $metadataDirectory "verification_metadata.json")
    }

    $verificationOutputLog = [string](Get-OptionalPropertyValue -Object $Metadata -Name "VerificationOutputLog" -DefaultValue "")
    if (-not [string]::IsNullOrWhiteSpace($verificationOutputLog)) {
        $copiedFiles.VerificationOutputLog = Copy-FileIfDifferent -SourcePath $verificationOutputLog -DestinationPath (Join-Path $metadataDirectory "verify_resumed_denial_ci_output.log")
    }

    return [pscustomobject]$copiedFiles
}

function Get-MetadataScenarioStatuses {
    param([object]$Metadata)

    $statuses = [ordered]@{
        happy = "UNKNOWN"
        gate = "UNKNOWN"
        recovery = "UNKNOWN"
    }

    $metadataStatuses = Get-OptionalPropertyValue -Object $Metadata -Name "ScenarioStatuses"
    if ($null -eq $metadataStatuses) {
        return $statuses
    }

    foreach ($scenarioName in $script:ScenarioNames) {
        $statuses[$scenarioName] = Get-NormalizedScenarioStatus -Value ([string](Get-OptionalPropertyValue -Object $metadataStatuses -Name $scenarioName -DefaultValue "UNKNOWN"))
    }

    return $statuses
}

function Get-MetadataScenarioProvenance {
    param([object]$Metadata)

    $provenance = [ordered]@{
        happy = ""
        gate = ""
        recovery = ""
    }

    $metadataProvenance = Get-OptionalPropertyValue -Object $Metadata -Name "ScenarioProvenance"
    if ($null -ne $metadataProvenance) {
        foreach ($scenarioName in $script:ScenarioNames) {
            $provenance[$scenarioName] = [string](Get-OptionalPropertyValue -Object $metadataProvenance -Name $scenarioName -DefaultValue "")
        }
    }

    return $provenance
}

function Get-MetadataOverallResult {
    param([object]$Metadata)

    return Get-NormalizedScenarioStatus -Value ([string](Get-OptionalPropertyValue -Object $Metadata -Name "OverallResult" -DefaultValue "UNKNOWN"))
}

function Build-ScenarioArtifactRecord {
    param(
        [string]$ScenarioName,
        [string]$RunLabel,
        [pscustomobject]$ArtifactFiles,
        [pscustomobject]$SummaryParse,
        [string]$MetadataStatus,
        [string]$MetadataProvenance,
        [string]$ResolvedOutputDir,
        [switch]$IncludeCodexArtifacts
    )

    $runtimeScenarioDir = Ensure-Directory -LiteralPath (Join-Path (Join-Path $ResolvedOutputDir "runtime") $ScenarioName)
    $artifactSummaryPath = ""
    if (-not [string]::IsNullOrWhiteSpace($ArtifactFiles.SummaryFile)) {
        $artifactSummaryPath = Copy-FileIfDifferent -SourcePath $ArtifactFiles.SummaryFile -DestinationPath (Join-Path $runtimeScenarioDir "summary.log")
    }

    $artifactPartPaths = New-Object System.Collections.Generic.List[string]
    foreach ($partFile in $ArtifactFiles.RuntimePartFiles) {
        $partName = Split-Path -Path $partFile -Leaf
        $destinationPath = Join-Path $runtimeScenarioDir $partName
        $copiedPartPath = Copy-FileIfDifferent -SourcePath $partFile -DestinationPath $destinationPath
        if (-not [string]::IsNullOrWhiteSpace($copiedPartPath)) {
            $artifactPartPaths.Add($copiedPartPath)
        }
    }

    $artifactManifestPath = ""
    if ($IncludeCodexArtifacts -and -not [string]::IsNullOrWhiteSpace($ArtifactFiles.CodexManifest)) {
        $codexScenarioDir = Ensure-Directory -LiteralPath (Join-Path (Join-Path $ResolvedOutputDir "codex") $ScenarioName)
        $artifactManifestPath = Copy-FileIfDifferent -SourcePath $ArtifactFiles.CodexManifest -DestinationPath (Join-Path $codexScenarioDir "manifest.json")
    }

    $resolvedStatus = $SummaryParse.Status
    if ($MetadataStatus -eq "SKIPPED" -or $MetadataStatus -eq "NOEXECUTE") {
        $resolvedStatus = "SKIPPED"
    }
    elseif ([string]::IsNullOrWhiteSpace($ArtifactFiles.SummaryFile)) {
        if ($MetadataStatus -eq "FAIL") {
            $resolvedStatus = "FAIL"
        }
        elseif ($MetadataStatus -eq "NOT_RUN") {
            $resolvedStatus = "NOT_RUN"
        }
        else {
            $resolvedStatus = "MISSING"
        }
    }

    return [pscustomobject]@{
        Name = $ScenarioName
        RunLabel = $RunLabel
        Status = $resolvedStatus
        MetadataStatus = $MetadataStatus
        ProvenanceMode = if ([string]::IsNullOrWhiteSpace($MetadataProvenance)) { "unknown" } else { $MetadataProvenance }
        SummarySourcePath = $ArtifactFiles.SummaryFile
        SummaryArtifactPath = $artifactSummaryPath
        RuntimePartSourcePaths = @($ArtifactFiles.RuntimePartFiles)
        RuntimePartArtifactPaths = @($artifactPartPaths)
        CodexManifestSourcePath = $ArtifactFiles.CodexManifest
        CodexManifestArtifactPath = $artifactManifestPath
        CodexManifestFound = (-not [string]::IsNullOrWhiteSpace($ArtifactFiles.CodexManifest))
        RuntimeGitBranch = $SummaryParse.RuntimeGitBranch
        RuntimeGitCommit = $SummaryParse.RuntimeGitCommit
        FailureReason = $SummaryParse.FailureReason
    }
}

function Write-SummaryFiles {
    param(
        [string]$ResolvedOutputDir,
        [object]$SummaryObject
    )

    $textSummaryPath = Join-Path $ResolvedOutputDir "resumed_denial_summary.txt"
    $jsonSummaryPath = Join-Path $ResolvedOutputDir "resumed_denial_summary.json"

    $lines = New-Object System.Collections.Generic.List[string]
    $lines.Add(("overall: {0}" -f $SummaryObject.overallResult))
    $lines.Add(("repoRoot: {0}" -f $SummaryObject.repoRoot))
    $lines.Add(("logsRoot: {0}" -f $SummaryObject.logsRoot))
    $lines.Add(("artifactOutputDir: {0}" -f $SummaryObject.artifactOutputDir))
    $lines.Add(("runLabelPattern: {0}" -f $SummaryObject.runLabelPattern))
    $lines.Add(("selectedRunSource: {0}" -f $SummaryObject.selectedRunSource))
    $lines.Add(("selectedBaseLabel: {0}" -f $SummaryObject.selectedBaseLabel))
    $lines.Add(("requestedProvenanceMode: {0}" -f $SummaryObject.requestedProvenanceMode))
    $lines.Add(("observedProvenanceMode: {0}" -f $SummaryObject.observedProvenanceMode))
    $lines.Add(("buildDir: {0}" -f $SummaryObject.buildDir))
    $lines.Add(("executablePath: {0}" -f $SummaryObject.executablePath))
    $lines.Add(("codexArtifactsRequested: {0}" -f $(if ($SummaryObject.codexArtifacts.requested) { "yes" } else { "no" })))
    $lines.Add(("codexArtifactsFound: {0}" -f $(if ($SummaryObject.codexArtifacts.found) { "yes" } else { "no" })))
    foreach ($scenarioRecord in $SummaryObject.scenarios) {
        $lines.Add(("{0}: {1}" -f $scenarioRecord.name, $scenarioRecord.status))
        $lines.Add(("  runLabel: {0}" -f $scenarioRecord.runLabel))
        $lines.Add(("  provenance: {0}" -f $scenarioRecord.provenanceMode))
        $lines.Add(("  summarySource: {0}" -f $scenarioRecord.summaryLog.sourcePath))
        $lines.Add(("  summaryArtifact: {0}" -f $scenarioRecord.summaryLog.artifactPath))
        $lines.Add(("  codexManifestFound: {0}" -f $(if ($scenarioRecord.codexManifest.found) { "yes" } else { "no" })))
        if (-not [string]::IsNullOrWhiteSpace($scenarioRecord.failureReason)) {
            $lines.Add(("  failureReason: {0}" -f $scenarioRecord.failureReason))
        }
    }

    Set-Content -LiteralPath $textSummaryPath -Value $lines -Encoding UTF8
    $SummaryObject | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $jsonSummaryPath -Encoding UTF8

    return [pscustomobject]@{
        TextSummaryPath = $textSummaryPath
        JsonSummaryPath = $jsonSummaryPath
    }
}

$resolvedRepoRoot = ""
$resolvedLogsRoot = ""
$resolvedOutputDir = ""
$summaryPaths = $null

try {
    $resolvedRepoRoot = Get-RepoRootFromRequest -RequestedRoot $RepoRoot
    $logsLayout = Resolve-LogsLayout -ResolvedRepoRoot $resolvedRepoRoot -RequestedLogsRoot $LogsRoot
    $resolvedLogsRoot = $logsLayout.LatestRoot
    $resolvedOutputDir = Ensure-Directory -LiteralPath (Resolve-FullPath -PathValue $OutputDir -BasePath $resolvedRepoRoot)
    $metadata = Read-VerificationMetadata -MetadataPath $VerificationMetadataPath

    Write-Heading "Resolved Inputs"
    Write-Host ("Repo root: {0}" -f $resolvedRepoRoot)
    Write-Host ("Logs root: {0}" -f $resolvedLogsRoot)
    Write-Host ("Runtime logs: {0}" -f $logsLayout.RuntimeRoot)
    Write-Host ("Codex logs: {0}" -f $(if ([string]::IsNullOrWhiteSpace($logsLayout.CodexRoot)) { "<missing>" } else { $logsLayout.CodexRoot }))
    Write-Host ("Output dir: {0}" -f $resolvedOutputDir)
    Write-Host ("Run label pattern: {0}" -f $RunLabelPattern)
    if ($null -ne $metadata) {
        Write-Host ("Verification metadata: {0}" -f $metadata.__path)
    }

    $selectedRunLabels = Get-SelectedRunLabels -RuntimeRoot $logsLayout.RuntimeRoot -RunLabelPattern $RunLabelPattern -Metadata $metadata
    $metadataStatuses = Get-MetadataScenarioStatuses -Metadata $metadata
    $metadataScenarioProvenance = Get-MetadataScenarioProvenance -Metadata $metadata
    $metadataOverallResult = Get-MetadataOverallResult -Metadata $metadata
    $supplementalFiles = Copy-SupplementalMetadataFiles -Metadata $metadata -ResolvedOutputDir $resolvedOutputDir
    $expectedHeadCommit = Get-GitSingleLine @("rev-parse", "HEAD")

    $scenarioRecords = New-Object System.Collections.Generic.List[object]
    $failureMessages = New-Object System.Collections.Generic.List[string]
    $codexArtifactsFound = $false
    $observedProvenanceModes = New-Object System.Collections.Generic.List[string]

    foreach ($scenarioName in $script:ScenarioNames) {
        $runLabel = [string](Get-OptionalPropertyValue -Object $selectedRunLabels.RunLabels -Name $scenarioName -DefaultValue "")
        $artifactFiles = Get-ScenarioArtifactFiles -RuntimeRoot $logsLayout.RuntimeRoot -CodexRoot $logsLayout.CodexRoot -RunLabel $runLabel
        $summaryParse = Parse-ScenarioSummaryFile -SummaryFilePath $artifactFiles.SummaryFile -ScenarioName $scenarioName
        $scenarioRecord = Build-ScenarioArtifactRecord `
            -ScenarioName $scenarioName `
            -RunLabel $runLabel `
            -ArtifactFiles $artifactFiles `
            -SummaryParse $summaryParse `
            -MetadataStatus $metadataStatuses[$scenarioName] `
            -MetadataProvenance $metadataScenarioProvenance[$scenarioName] `
            -ResolvedOutputDir $resolvedOutputDir `
            -IncludeCodexArtifacts:$IncludeCodexArtifacts
        $scenarioRecords.Add($scenarioRecord) | Out-Null

        if ($scenarioRecord.CodexManifestFound) {
            $codexArtifactsFound = $true
        }
        if ($scenarioRecord.ProvenanceMode -ne "unknown") {
            $observedProvenanceModes.Add($scenarioRecord.ProvenanceMode)
        }

        $summaryRequired = $false
        if ($metadataOverallResult -eq "PASS") {
            $summaryRequired = $true
        }
        elseif ($scenarioRecord.MetadataStatus -eq "PASS") {
            $summaryRequired = $true
        }
        elseif ($null -eq $metadata -and ($scenarioName -eq "happy" -or $scenarioName -eq "gate")) {
            $summaryRequired = $true
        }

        if ($summaryRequired -and [string]::IsNullOrWhiteSpace($scenarioRecord.SummarySourcePath)) {
            $failureMessages.Add(("{0} summary log is missing for run label {1}" -f $scenarioName, $(if ([string]::IsNullOrWhiteSpace($runLabel)) { "<unknown>" } else { $runLabel })))
        }

        if ($scenarioRecord.Status -eq "FAIL") {
            $failureMessages.Add(("{0} summary validation failed: {1}" -f $scenarioName, $(if ([string]::IsNullOrWhiteSpace($scenarioRecord.FailureReason)) { "see collected logs" } else { $scenarioRecord.FailureReason })))
        }

        if (
            $IncludeCodexArtifacts -and
            -not $AllowMissingCodexArtifacts -and
            $scenarioRecord.Status -ne "SKIPPED" -and
            $scenarioRecord.Status -ne "NOT_RUN" -and
            -not $scenarioRecord.CodexManifestFound
        ) {
            $failureMessages.Add(("{0} codex manifest is missing for run label {1}" -f $scenarioName, $(if ([string]::IsNullOrWhiteSpace($runLabel)) { "<unknown>" } else { $runLabel })))
        }
    }

    $observedProvenanceMode = "unknown"
    $uniqueProvenanceModes = @($observedProvenanceModes | Sort-Object -Unique)
    if ($uniqueProvenanceModes.Count -eq 1) {
        $observedProvenanceMode = $uniqueProvenanceModes[0]
    }
    elseif ($uniqueProvenanceModes.Count -gt 1) {
        $observedProvenanceMode = ($uniqueProvenanceModes -join ",")
    }

    $overallResult = "PASS"
    if ($metadataOverallResult -eq "NOEXECUTE") {
        $overallResult = "NOEXECUTE"
    }
    elseif ($metadataOverallResult -eq "FAIL") {
        $overallResult = "FAIL"
    }

    foreach ($scenarioRecord in $scenarioRecords) {
        if ($scenarioRecord.Status -eq "FAIL" -or $scenarioRecord.Status -eq "MISSING") {
            $overallResult = "FAIL"
            break
        }
    }

    if ($metadataOverallResult -eq "UNKNOWN" -and $overallResult -ne "FAIL") {
        $requiredPassScenarios = @($scenarioRecords | Where-Object { $_.Status -ne "SKIPPED" -and $_.Status -ne "NOT_RUN" })
        if ($requiredPassScenarios.Count -eq 0) {
            $overallResult = "NOEXECUTE"
        }
    }

    $summaryObject = [ordered]@{
        generatedAt = (Get-Date).ToString("o")
        repoRoot = $resolvedRepoRoot
        logsRoot = $resolvedLogsRoot
        artifactOutputDir = $resolvedOutputDir
        runLabelPattern = $selectedRunLabels.RunLabelPattern
        selectedRunSource = $selectedRunLabels.Source
        selectedBaseLabel = $selectedRunLabels.BaseLabel
        promptId = $script:PromptId
        requestedProvenanceMode = [string](Get-OptionalPropertyValue -Object $metadata -Name "RequestedProvenanceMode" -DefaultValue "unknown")
        observedProvenanceMode = $observedProvenanceMode
        expectedHeadCommit = $expectedHeadCommit
        buildDir = [string](Get-OptionalPropertyValue -Object $metadata -Name "BuildDir" -DefaultValue "")
        executablePath = [string](Get-OptionalPropertyValue -Object $metadata -Name "ExecutablePath" -DefaultValue "")
        overallResult = $overallResult
        verificationCommandLine = [string](Get-OptionalPropertyValue -Object $metadata -Name "VerificationCommandLine" -DefaultValue "")
        codexArtifacts = [ordered]@{
            requested = [bool]$IncludeCodexArtifacts
            allowMissing = [bool]$AllowMissingCodexArtifacts
            found = $codexArtifactsFound
        }
        supplementalFiles = [ordered]@{
            verificationMetadata = [string](Get-OptionalPropertyValue -Object $supplementalFiles -Name "VerificationMetadata" -DefaultValue "")
            verificationOutputLog = [string](Get-OptionalPropertyValue -Object $supplementalFiles -Name "VerificationOutputLog" -DefaultValue "")
        }
        scenarios = @(
            foreach ($scenarioRecord in $scenarioRecords) {
                [ordered]@{
                    name = $scenarioRecord.Name
                    runLabel = $scenarioRecord.RunLabel
                    status = $scenarioRecord.Status
                    metadataStatus = $scenarioRecord.MetadataStatus
                    provenanceMode = $scenarioRecord.ProvenanceMode
                    failureReason = $scenarioRecord.FailureReason
                    runtimeGitBranch = $scenarioRecord.RuntimeGitBranch
                    runtimeGitCommit = $scenarioRecord.RuntimeGitCommit
                    summaryLog = [ordered]@{
                        sourcePath = $scenarioRecord.SummarySourcePath
                        artifactPath = $scenarioRecord.SummaryArtifactPath
                    }
                    runtimeParts = [ordered]@{
                        sourcePaths = @($scenarioRecord.RuntimePartSourcePaths)
                        artifactPaths = @($scenarioRecord.RuntimePartArtifactPaths)
                    }
                    codexManifest = [ordered]@{
                        found = $scenarioRecord.CodexManifestFound
                        sourcePath = $scenarioRecord.CodexManifestSourcePath
                        artifactPath = $scenarioRecord.CodexManifestArtifactPath
                    }
                }
            }
        )
    }

    $summaryPaths = Write-SummaryFiles -ResolvedOutputDir $resolvedOutputDir -SummaryObject $summaryObject

    Write-Heading "Artifact Bundle"
    Write-Host ("Text summary: {0}" -f $summaryPaths.TextSummaryPath)
    Write-Host ("JSON summary: {0}" -f $summaryPaths.JsonSummaryPath)
    Write-Host ("Runtime artifacts: {0}" -f (Join-Path $resolvedOutputDir "runtime"))
    if ($IncludeCodexArtifacts) {
        Write-Host ("Codex artifacts: {0}" -f (Join-Path $resolvedOutputDir "codex"))
    }

    if ($failureMessages.Count -gt 0) {
        throw ($failureMessages -join " | ")
    }
}
catch {
    Write-Heading "Failure"
    Write-Host $_.Exception.Message
    if ($null -ne $summaryPaths) {
        Write-Host ("Partial text summary: {0}" -f $summaryPaths.TextSummaryPath)
        Write-Host ("Partial JSON summary: {0}" -f $summaryPaths.JsonSummaryPath)
    }
    exit 1
}

exit 0
