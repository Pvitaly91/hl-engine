param(
    [string]$RepoRoot,
    [string]$OuterWorkspaceRoot,
    [ValidateSet("auto", "runtime", "workspace")]
    [string]$ProvenanceMode = "auto",
    [string]$BuildDir,
    [string]$ExePath,
    [switch]$NoBuild,
    [switch]$SkipRecovery,
    [switch]$NoExecute,
    [switch]$UseExistingBinary,
    [switch]$RunNeighborSurfaceSuite,
    [ValidateSet("auto", "default", "checkpoint-extended", "full-expanded")]
    [string]$NeighborSurfaceProfile,
    [string[]]$NeighborSurfaceGroup,
    [string[]]$NeighborSurface,
    [switch]$FullNeighborMatrix,
    [string]$DiffBase,
    [string]$DiffHead,
    [string[]]$ChangedPath,
    [string]$JUnitOutputPath,
    [switch]$CollectArtifacts,
    [string]$ArtifactOutputDir,
    [string]$ExecutionMode,
    [string]$ExecutionModeReason
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
if (Test-Path Variable:PSNativeCommandUseErrorActionPreference) {
    $PSNativeCommandUseErrorActionPreference = $false
}

$script:PromptId = "HL-CL-20260411-163-dedicated-goldsrc-signon-carried-checkpoint-claimed-checkpoint-resumed-denial-surface"
$script:DefaultBuildDirName = "build-main-win32-hlhost-regression"
$script:MainRunnerPath = Join-Path $PSScriptRoot "verify_resumed_denial_main.ps1"
$script:NeighborSuiteRunnerPath = Join-Path $PSScriptRoot "verify_signon_neighbor_surfaces_main.ps1"
$script:ArtifactCollectorPath = Join-Path $PSScriptRoot "collect_resumed_denial_artifacts.ps1"
$script:ProfileResolverPath = Join-Path $PSScriptRoot "resolve_signon_regression_profile.ps1"
$script:DefaultNeighborJUnitFileName = "signon_neighbor_surface_junit.xml"

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

function Try-Get-GitRepoRoot {
    param([string]$CandidateRoot)

    if ([string]::IsNullOrWhiteSpace($CandidateRoot) -or -not (Test-Path -LiteralPath $CandidateRoot -PathType Container)) {
        return ""
    }

    Push-Location $CandidateRoot
    try {
        $topLevel = & git rev-parse --show-toplevel 2>$null
        if ($LASTEXITCODE -ne 0 -or $null -eq $topLevel) {
            return ""
        }

        return [System.IO.Path]::GetFullPath(($topLevel | Select-Object -First 1).Trim())
    }
    finally {
        Pop-Location
    }
}

function Quote-Argument {
    param([string]$Value)

    if ([string]::IsNullOrEmpty($Value)) {
        return '""'
    }

    if ($Value -notmatch '[\s"]') {
        return $Value
    }

    return '"' + ($Value -replace '"', '\"') + '"'
}

function Format-CommandLine {
    param(
        [string]$ExecutablePath,
        [string[]]$Arguments
    )

    $segments = New-Object System.Collections.Generic.List[string]
    $segments.Add((Quote-Argument $ExecutablePath))
    foreach ($argument in $Arguments) {
        $segments.Add((Quote-Argument $argument))
    }

    return ($segments -join " ")
}

function Get-CurrentHostExecutable {
    $processPath = (Get-Process -Id $PID).Path
    if (-not [string]::IsNullOrWhiteSpace($processPath) -and (Test-Path -LiteralPath $processPath -PathType Leaf)) {
        return $processPath
    }

    foreach ($candidate in @(
        (Join-Path $PSHOME "pwsh.exe"),
        (Join-Path $PSHOME "powershell.exe"),
        "pwsh.exe",
        "powershell.exe"
    )) {
        if (Test-Path -LiteralPath $candidate -PathType Leaf) {
            return $candidate
        }

        $command = Get-Command $candidate -ErrorAction SilentlyContinue
        if ($null -ne $command -and -not [string]::IsNullOrWhiteSpace($command.Source)) {
            return $command.Source
        }
    }

    throw "Unable to locate a PowerShell host executable."
}

function New-WorkspaceCandidate {
    param(
        [string]$Root,
        [string]$Source
    )

    return [pscustomobject]@{
        Root = $Root
        Source = $Source
    }
}

function Resolve-OuterWorkspaceRoot {
    param(
        [string]$ResolvedRepoRoot,
        [string]$RequestedWorkspaceRoot
    )

    $candidates = New-Object System.Collections.Generic.List[object]
    if (-not [string]::IsNullOrWhiteSpace($RequestedWorkspaceRoot)) {
        $candidates.Add((New-WorkspaceCandidate -Root (Resolve-FullPath -PathValue $RequestedWorkspaceRoot -BasePath $ResolvedRepoRoot) -Source "explicit"))
    }

    foreach ($envCandidate in @(
        @{ Value = $env:HLHOST_OUTER_WORKSPACE_ROOT; Source = "env:HLHOST_OUTER_WORKSPACE_ROOT" },
        @{ Value = $env:HLENGINE_WORKSPACE_ROOT; Source = "env:HLENGINE_WORKSPACE_ROOT" }
    )) {
        if (-not [string]::IsNullOrWhiteSpace($envCandidate.Value)) {
            $candidates.Add((New-WorkspaceCandidate -Root (Resolve-FullPath -PathValue $envCandidate.Value -BasePath $ResolvedRepoRoot) -Source $envCandidate.Source))
        }
    }

    $parentRoot = Split-Path -Path $ResolvedRepoRoot -Parent
    if (-not [string]::IsNullOrWhiteSpace($parentRoot)) {
        $candidates.Add((New-WorkspaceCandidate -Root ([System.IO.Path]::GetFullPath($parentRoot)) -Source "repo-parent"))
    }

    $visitedRoots = New-Object System.Collections.Generic.HashSet[string]([System.StringComparer]::OrdinalIgnoreCase)
    $attempts = New-Object System.Collections.Generic.List[string]
    foreach ($candidate in $candidates) {
        if ([string]::IsNullOrWhiteSpace($candidate.Root)) {
            continue
        }

        $resolvedCandidateRoot = [System.IO.Path]::GetFullPath($candidate.Root)
        if (-not $visitedRoots.Add($resolvedCandidateRoot)) {
            continue
        }

        $cmakeListsPath = Join-Path $resolvedCandidateRoot "CMakeLists.txt"
        $expectedRepoRoot = [System.IO.Path]::GetFullPath((Join-Path $resolvedCandidateRoot "host"))
        $canonicalExpectedRepoRoot = Try-Get-GitRepoRoot -CandidateRoot $expectedRepoRoot
        $expectedComparisonRoot = if ([string]::IsNullOrWhiteSpace($canonicalExpectedRepoRoot)) { $expectedRepoRoot } else { $canonicalExpectedRepoRoot }
        $workspaceExists = Test-Path -LiteralPath $resolvedCandidateRoot -PathType Container
        $cmakeExists = Test-Path -LiteralPath $cmakeListsPath -PathType Leaf
        $repoMatches = $expectedComparisonRoot.Equals($ResolvedRepoRoot, [System.StringComparison]::OrdinalIgnoreCase)
        $attempts.Add(
            ("{0}: root={1}; exists={2}; cmake={3}; expectedHostRepo={4}; canonicalHostRepo={5}; repoMatches={6}" -f
                $candidate.Source,
                $resolvedCandidateRoot,
                $(if ($workspaceExists) { "yes" } else { "no" }),
                $(if ($cmakeExists) { "yes" } else { "no" }),
                $expectedRepoRoot,
                $(if ([string]::IsNullOrWhiteSpace($canonicalExpectedRepoRoot)) { "<unresolved>" } else { $canonicalExpectedRepoRoot }),
                $(if ($repoMatches) { "yes" } else { "no" })))

        if ($workspaceExists -and $cmakeExists -and $repoMatches) {
            return [pscustomobject]@{
                Root = $resolvedCandidateRoot
                Source = $candidate.Source
                ExpectedRepoRoot = $expectedRepoRoot
                Attempts = $attempts.ToArray()
            }
        }
    }

    $reason = "Unable to resolve an outer workspace root for the host repo."
    if (-not [string]::IsNullOrWhiteSpace($RequestedWorkspaceRoot)) {
        $reason += " The explicit -OuterWorkspaceRoot did not point at an hl-engine workspace whose expected host checkout matched -RepoRoot."
    }
    else {
        $reason += " Checked explicit environment overrides and the repo parent."
    }

    $reason += " Provide -OuterWorkspaceRoot <path> or set HLHOST_OUTER_WORKSPACE_ROOT so that <outer-workspace>\\host resolves to the requested repo root."
    if ($attempts.Count -gt 0) {
        $reason += " Attempts: " + ($attempts -join " | ")
    }

    throw $reason
}

function Resolve-BuildDirPath {
    param(
        [string]$WorkspaceRoot,
        [string]$RequestedBuildDir
    )

    if ([string]::IsNullOrWhiteSpace($RequestedBuildDir)) {
        return [System.IO.Path]::GetFullPath((Join-Path $WorkspaceRoot $script:DefaultBuildDirName))
    }

    return Resolve-FullPath -PathValue $RequestedBuildDir -BasePath $WorkspaceRoot
}

function Resolve-ExecutablePath {
    param(
        [string]$WorkspaceRoot,
        [string]$RequestedExecutablePath
    )

    if ([string]::IsNullOrWhiteSpace($RequestedExecutablePath)) {
        return ""
    }

    return Resolve-FullPath -PathValue $RequestedExecutablePath -BasePath $WorkspaceRoot
}

function Resolve-JUnitOutputPath {
    param(
        [string]$ResolvedRepoRoot,
        [string]$RequestedJUnitOutputPath
    )

    if ([string]::IsNullOrWhiteSpace($RequestedJUnitOutputPath)) {
        return ""
    }

    return Resolve-FullPath -PathValue $RequestedJUnitOutputPath -BasePath $ResolvedRepoRoot
}

function Get-ExpectedExecutablePath {
    param([string]$ResolvedBuildDir)

    return [System.IO.Path]::GetFullPath((Join-Path $ResolvedBuildDir "host\Debug\hlhost.exe"))
}

function Assert-PathExists {
    param(
        [string]$LiteralPath,
        [string]$Description,
        [string]$ActionHint
    )

    if (Test-Path -LiteralPath $LiteralPath) {
        return
    }

    $message = ("Missing {0}: {1}" -f $Description, $LiteralPath)
    if (-not [string]::IsNullOrWhiteSpace($ActionHint)) {
        $message += ". " + $ActionHint
    }

    throw $message
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

function Get-TrimmedUniqueValues {
    param([string[]]$Values)

    $seenValues = New-Object System.Collections.Generic.HashSet[string]([System.StringComparer]::OrdinalIgnoreCase)
    $normalizedValues = New-Object System.Collections.Generic.List[string]
    foreach ($value in @($Values)) {
        if ($null -eq $value) {
            continue
        }

        $trimmedValue = $value.Trim()
        if ([string]::IsNullOrWhiteSpace($trimmedValue)) {
            continue
        }

        if ($seenValues.Add($trimmedValue)) {
            $normalizedValues.Add($trimmedValue)
        }
    }

    return $normalizedValues.ToArray()
}

function Resolve-NeighborProfileSelectionContext {
    param(
        [string]$ResolvedRepoRoot,
        [string]$RequestedProfile,
        [string[]]$NormalizedNeighborSurfaceGroups,
        [string[]]$NormalizedNeighborSurfaces,
        [switch]$FullNeighborMatrixRequested,
        [string]$DiffBase,
        [string]$DiffHead,
        [string[]]$ChangedPaths
    )

    if ($RequestedProfile -eq "auto") {
        Assert-PathExists -LiteralPath $script:ProfileResolverPath -Description "neighbor profile resolver" -ActionHint "Verify that tools\\resolve_signon_regression_profile.ps1 exists in this checkout"
        $resolverResult = & {
            param(
                [string]$ScriptPath,
                [string]$ResolvedRepoRoot,
                [string]$DiffBase,
                [string]$DiffHead,
                [string[]]$ChangedPaths
            )

            . $ScriptPath
            $resolution = Resolve-SignonRegressionProfile `
                -RepoRoot $ResolvedRepoRoot `
                -DiffBase $DiffBase `
                -DiffHead $DiffHead `
                -ChangedPath $ChangedPaths
            Write-SignonRegressionProfileReport -Resolution $resolution -PrintChangedFiles
            return $resolution
        } $script:ProfileResolverPath $ResolvedRepoRoot $DiffBase $DiffHead $ChangedPaths

        return [pscustomobject]@{
            RequestedProfileMode = "auto"
            RequestedProfile = "auto"
            RunnerProfile = [string]$resolverResult.resolvedProfile
            ResolvedProfile = [string]$resolverResult.resolvedProfile
            SelectionOrigin = "auto-profile"
            SelectionReason = [string]$resolverResult.reason
            MatchedRuleIds = @($resolverResult.matchedRuleIds)
            MatchedRules = @($resolverResult.matchedRules)
            DiffBase = [string]$resolverResult.diffBase
            DiffHead = [string]$resolverResult.diffHead
            ChangedPaths = @($resolverResult.changedPaths)
            ChangedPathSource = [string]$resolverResult.changedPathSource
        }
    }

    if (-not [string]::IsNullOrWhiteSpace($RequestedProfile)) {
        return [pscustomobject]@{
            RequestedProfileMode = "explicit"
            RequestedProfile = $RequestedProfile
            RunnerProfile = $RequestedProfile
            ResolvedProfile = $RequestedProfile
            SelectionOrigin = "profile"
            SelectionReason = ("Explicit neighboring-surface profile override requested: {0}." -f $RequestedProfile)
            MatchedRuleIds = @()
            MatchedRules = @()
            DiffBase = ""
            DiffHead = ""
            ChangedPaths = @()
            ChangedPathSource = ""
        }
    }

    if ($FullNeighborMatrixRequested -or $NormalizedNeighborSurfaceGroups.Count -gt 0 -or $NormalizedNeighborSurfaces.Count -gt 0) {
        return [pscustomobject]@{
            RequestedProfileMode = "explicit"
            RequestedProfile = ""
            RunnerProfile = ""
            ResolvedProfile = ""
            SelectionOrigin = "explicit"
            SelectionReason = "Explicit neighboring-surface groups, surfaces, or full-matrix selection requested; auto profile selection was not used."
            MatchedRuleIds = @()
            MatchedRules = @()
            DiffBase = ""
            DiffHead = ""
            ChangedPaths = @()
            ChangedPathSource = ""
        }
    }

    return [pscustomobject]@{
        RequestedProfileMode = "implicit-default"
        RequestedProfile = ""
        RunnerProfile = ""
        ResolvedProfile = ""
        SelectionOrigin = "default"
        SelectionReason = "No neighboring-surface profile or filters were requested; preserving the existing default-enabled matrix semantics."
        MatchedRuleIds = @()
        MatchedRules = @()
        DiffBase = ""
        DiffHead = ""
        ChangedPaths = @()
        ChangedPathSource = ""
    }
}

function Resolve-ArtifactOutputDir {
    param(
        [string]$ResolvedRepoRoot,
        [string]$RequestedArtifactOutputDir,
        [switch]$IncludeNeighborSurfaceSuite
    )

    if ([string]::IsNullOrWhiteSpace($RequestedArtifactOutputDir)) {
        $defaultDirectoryName = if ($IncludeNeighborSurfaceSuite) { "signon-regressions" } else { "resumed-denial" }
        return [System.IO.Path]::GetFullPath((Join-Path $ResolvedRepoRoot ("artifacts\{0}" -f $defaultDirectoryName)))
    }

    return Resolve-FullPath -PathValue $RequestedArtifactOutputDir -BasePath $ResolvedRepoRoot
}

function Resolve-ArtifactLayout {
    param(
        [string]$ResolvedRepoRoot,
        [string]$RequestedArtifactOutputDir,
        [switch]$IncludeNeighborSurfaceSuite
    )

    $rootOutputDir = Resolve-ArtifactOutputDir `
        -ResolvedRepoRoot $ResolvedRepoRoot `
        -RequestedArtifactOutputDir $RequestedArtifactOutputDir `
        -IncludeNeighborSurfaceSuite:$IncludeNeighborSurfaceSuite

    $resumedOutputDir = $rootOutputDir
    $neighborOutputDir = ""
    $combinedTextSummaryPath = ""
    $combinedJsonSummaryPath = ""

    if ($IncludeNeighborSurfaceSuite) {
        $resumedOutputDir = Join-Path $rootOutputDir "resumed-denial"
        $neighborOutputDir = Join-Path $rootOutputDir "neighbor-surfaces"
        $combinedTextSummaryPath = Join-Path $rootOutputDir "verification_suite_summary.txt"
        $combinedJsonSummaryPath = Join-Path $rootOutputDir "verification_suite_summary.json"
    }

    return [pscustomobject]@{
        RootOutputDir = $rootOutputDir
        ResumedDenialOutputDir = $resumedOutputDir
        NeighborSurfaceOutputDir = $neighborOutputDir
        CombinedTextSummaryPath = $combinedTextSummaryPath
        CombinedJsonSummaryPath = $combinedJsonSummaryPath
    }
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

function Get-VerificationOutputMetadata {
    param(
        [string]$OutputLogPath,
        [switch]$NoExecuteRequested,
        [switch]$SkipRecoveryRequested
    )

    $scenarioStatuses = [ordered]@{
        happy = "UNKNOWN"
        gate = "UNKNOWN"
        recovery = if ($SkipRecoveryRequested) { "SKIPPED" } else { "UNKNOWN" }
    }
    $runLabels = [ordered]@{}
    $scenarioProvenance = [ordered]@{
        happy = ""
        gate = ""
        recovery = ""
    }
    $currentScenario = ""
    $overallResult = if ($NoExecuteRequested) { "NOEXECUTE" } else { "UNKNOWN" }

    if ([string]::IsNullOrWhiteSpace($OutputLogPath) -or -not (Test-Path -LiteralPath $OutputLogPath -PathType Leaf)) {
        return [pscustomobject]@{
            ScenarioStatuses = $scenarioStatuses
            RunLabels = $runLabels
            ScenarioProvenance = $scenarioProvenance
            OverallResult = $overallResult
        }
    }

    foreach ($line in @(Get-Content -LiteralPath $OutputLogPath)) {
        $trimmedLine = $line.Trim()

        if ($trimmedLine -match '^(happy|gate|recovery) command:$') {
            $currentScenario = $matches[1]
            continue
        }

        if ($line -match '--run-label\s+([^\s"]+)') {
            $runLabel = $matches[1]
            $runLabelInfo = Get-RunLabelInfo -RunLabel $runLabel
            if ($null -ne $runLabelInfo) {
                $runLabels[$runLabelInfo.Scenario] = $runLabel
            }
        }

        if ($trimmedLine -match '^(happy|gate|recovery):\s+(PASS|FAIL|SKIPPED|PENDING)$') {
            $scenarioStatuses[$matches[1]] = Get-NormalizedScenarioStatus -Value $matches[2]
            continue
        }

        if ($trimmedLine -match '^provenance:\s+([A-Za-z0-9_,.-]+)$' -and -not [string]::IsNullOrWhiteSpace($currentScenario)) {
            $scenarioProvenance[$currentScenario] = $matches[1]
            continue
        }

        if ($trimmedLine -match '^Final overall verdict:\s+([A-Za-z]+)$') {
            $overallResult = Get-NormalizedScenarioStatus -Value $matches[1]
        }
    }

    return [pscustomobject]@{
        ScenarioStatuses = $scenarioStatuses
        RunLabels = $runLabels
        ScenarioProvenance = $scenarioProvenance
        OverallResult = $overallResult
    }
}

function Write-ArtifactMetadataFile {
    param(
        [string]$MetadataPath,
        [string]$ResultLabel,
        [string]$ResolvedRepoRoot,
        [string]$ResolvedWorkspaceRoot,
        [string]$ResolvedBuildDir,
        [string]$ResolvedExecutablePath,
        [string]$RequestedProvenanceMode,
        [string]$VerificationCommandLine,
        [string]$VerificationOutputLogPath,
        [object]$VerificationOutputMetadata,
        [object]$NeighborSurfaceSelection,
        [string]$ExecutionMode,
        [string]$ExecutionModeReason
    )

    $metadataDirectory = Split-Path -Path $MetadataPath -Parent
    if (-not [string]::IsNullOrWhiteSpace($metadataDirectory)) {
        New-Item -ItemType Directory -Path $metadataDirectory -Force | Out-Null
    }

    $runLabels = [ordered]@{
        happy = [string](Get-OptionalPropertyValue -Object $VerificationOutputMetadata.RunLabels -Name "happy" -DefaultValue "")
        gate = [string](Get-OptionalPropertyValue -Object $VerificationOutputMetadata.RunLabels -Name "gate" -DefaultValue "")
        recovery = [string](Get-OptionalPropertyValue -Object $VerificationOutputMetadata.RunLabels -Name "recovery" -DefaultValue "")
    }

    $runLabelPattern = "verify-resumed-denial-main-*"
    foreach ($runLabel in $runLabels.Values) {
        $runLabelInfo = Get-RunLabelInfo -RunLabel $runLabel
        if ($null -ne $runLabelInfo) {
            $runLabelPattern = ("{0}-*" -f $runLabelInfo.BaseLabel)
            break
        }
    }

    $metadataObject = [ordered]@{
        generatedAt = (Get-Date).ToString("o")
        promptId = $script:PromptId
        repoRoot = $ResolvedRepoRoot
        outerWorkspaceRoot = $ResolvedWorkspaceRoot
        buildDir = $ResolvedBuildDir
        executablePath = $ResolvedExecutablePath
        requestedProvenanceMode = $RequestedProvenanceMode
        executionMode = $ExecutionMode
        executionModeReason = $ExecutionModeReason
        overallResult = $ResultLabel
        verificationCommandLine = $VerificationCommandLine
        verificationOutputLog = $VerificationOutputLogPath
        runLabelPattern = $runLabelPattern
        runLabels = $runLabels
        scenarioStatuses = [ordered]@{
            happy = [string](Get-OptionalPropertyValue -Object $VerificationOutputMetadata.ScenarioStatuses -Name "happy" -DefaultValue "UNKNOWN")
            gate = [string](Get-OptionalPropertyValue -Object $VerificationOutputMetadata.ScenarioStatuses -Name "gate" -DefaultValue "UNKNOWN")
            recovery = [string](Get-OptionalPropertyValue -Object $VerificationOutputMetadata.ScenarioStatuses -Name "recovery" -DefaultValue "UNKNOWN")
        }
        scenarioProvenance = [ordered]@{
            happy = [string](Get-OptionalPropertyValue -Object $VerificationOutputMetadata.ScenarioProvenance -Name "happy" -DefaultValue "")
            gate = [string](Get-OptionalPropertyValue -Object $VerificationOutputMetadata.ScenarioProvenance -Name "gate" -DefaultValue "")
            recovery = [string](Get-OptionalPropertyValue -Object $VerificationOutputMetadata.ScenarioProvenance -Name "recovery" -DefaultValue "")
        }
    }

    if ($null -ne $NeighborSurfaceSelection) {
        $metadataObject.neighborSurfaceSelection = [ordered]@{
            requestedProfileMode = [string](Get-OptionalPropertyValue -Object $NeighborSurfaceSelection -Name "RequestedProfileMode" -DefaultValue "")
            requestedProfile = [string](Get-OptionalPropertyValue -Object $NeighborSurfaceSelection -Name "RequestedProfile" -DefaultValue "")
            resolvedProfile = [string](Get-OptionalPropertyValue -Object $NeighborSurfaceSelection -Name "ResolvedProfile" -DefaultValue "")
            selectionOrigin = [string](Get-OptionalPropertyValue -Object $NeighborSurfaceSelection -Name "SelectionOrigin" -DefaultValue "")
            selectionReason = [string](Get-OptionalPropertyValue -Object $NeighborSurfaceSelection -Name "SelectionReason" -DefaultValue "")
            matchedRuleIds = @((Get-OptionalPropertyValue -Object $NeighborSurfaceSelection -Name "MatchedRuleIds" -DefaultValue @()))
            matchedRules = @((Get-OptionalPropertyValue -Object $NeighborSurfaceSelection -Name "MatchedRules" -DefaultValue @()))
            diffBase = [string](Get-OptionalPropertyValue -Object $NeighborSurfaceSelection -Name "DiffBase" -DefaultValue "")
            diffHead = [string](Get-OptionalPropertyValue -Object $NeighborSurfaceSelection -Name "DiffHead" -DefaultValue "")
            changedPathSource = [string](Get-OptionalPropertyValue -Object $NeighborSurfaceSelection -Name "ChangedPathSource" -DefaultValue "")
            changedPaths = @((Get-OptionalPropertyValue -Object $NeighborSurfaceSelection -Name "ChangedPaths" -DefaultValue @()))
        }
    }

    $metadataObject | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $MetadataPath -Encoding UTF8
}

function Read-JsonFileIfExists {
    param([string]$JsonPath)

    if ([string]::IsNullOrWhiteSpace($JsonPath) -or -not (Test-Path -LiteralPath $JsonPath -PathType Leaf)) {
        return $null
    }

    return (Get-Content -LiteralPath $JsonPath -Raw | ConvertFrom-Json)
}

function Get-ObservedProvenanceMode {
    param(
        [object]$ScenarioProvenance,
        [string[]]$ScenarioNames = @("happy", "gate", "recovery")
    )

    $values = New-Object System.Collections.Generic.List[string]
    foreach ($scenarioName in $ScenarioNames) {
        $value = [string](Get-OptionalPropertyValue -Object $ScenarioProvenance -Name $scenarioName -DefaultValue "")
        if (-not [string]::IsNullOrWhiteSpace($value)) {
            $values.Add($value)
        }
    }

    $uniqueValues = @($values | Sort-Object -Unique)
    if ($uniqueValues.Count -eq 0) {
        return ""
    }
    if ($uniqueValues.Count -eq 1) {
        return $uniqueValues[0]
    }

    return "mixed"
}

function Get-CombinedResult {
    param(
        [string]$PrimaryResult,
        [string]$SecondaryResult
    )

    $results = @(
        @($PrimaryResult, $SecondaryResult) |
            Where-Object { -not [string]::IsNullOrWhiteSpace($_) -and $_ -ne "NOT_REQUESTED" }
    )

    if ($results.Count -eq 0) {
        return "UNKNOWN"
    }
    if ($results -contains "FAIL") {
        return "FAIL"
    }
    if ((@($results | Where-Object { $_ -eq "NOEXECUTE" })).Count -eq $results.Count) {
        return "NOEXECUTE"
    }
    if ((@($results | Where-Object { $_ -eq "PASS" })).Count -eq $results.Count) {
        return "PASS"
    }

    return "FAIL"
}

function Get-ScenarioValueMapFromSummary {
    param(
        [object]$SummaryObject,
        [string]$ValuePropertyName,
        [hashtable]$FallbackValues
    )

    $valueMap = [ordered]@{
        happy = [string](Get-OptionalPropertyValue -Object $FallbackValues -Name "happy" -DefaultValue "UNKNOWN")
        gate = [string](Get-OptionalPropertyValue -Object $FallbackValues -Name "gate" -DefaultValue "UNKNOWN")
        recovery = [string](Get-OptionalPropertyValue -Object $FallbackValues -Name "recovery" -DefaultValue "UNKNOWN")
    }

    if ($null -eq $SummaryObject -or -not $SummaryObject.PSObject.Properties["scenarios"]) {
        return $valueMap
    }

    foreach ($scenario in @($SummaryObject.scenarios)) {
        $scenarioName = [string](Get-OptionalPropertyValue -Object $scenario -Name "name" -DefaultValue "")
        if ([string]::IsNullOrWhiteSpace($scenarioName) -or -not $valueMap.Contains($scenarioName)) {
            continue
        }

        $valueMap[$scenarioName] = [string](Get-OptionalPropertyValue -Object $scenario -Name $ValuePropertyName -DefaultValue $valueMap[$scenarioName])
    }

    return $valueMap
}

function Write-CombinedSuiteSummaryFiles {
    param(
        [string]$TextSummaryPath,
        [string]$JsonSummaryPath,
        [object]$SummaryObject
    )

    $textDirectory = Split-Path -Path $TextSummaryPath -Parent
    $jsonDirectory = Split-Path -Path $JsonSummaryPath -Parent
    if (-not [string]::IsNullOrWhiteSpace($textDirectory)) {
        New-Item -ItemType Directory -Path $textDirectory -Force | Out-Null
    }
    if (-not [string]::IsNullOrWhiteSpace($jsonDirectory)) {
        New-Item -ItemType Directory -Path $jsonDirectory -Force | Out-Null
    }

    $textLines = New-Object System.Collections.Generic.List[string]
    $textLines.Add(("overall: {0}" -f $SummaryObject.overallResult))
    $textLines.Add(("requested provenance: {0}" -f $SummaryObject.requestedProvenanceMode))
    if (-not [string]::IsNullOrWhiteSpace([string](Get-OptionalPropertyValue -Object $SummaryObject -Name "executionMode" -DefaultValue ""))) {
        $textLines.Add(("execution mode: {0}" -f [string]$SummaryObject.executionMode))
    }
    if (-not [string]::IsNullOrWhiteSpace([string](Get-OptionalPropertyValue -Object $SummaryObject -Name "executionModeReason" -DefaultValue ""))) {
        $textLines.Add(("execution mode reason: {0}" -f [string]$SummaryObject.executionModeReason))
    }
    $textLines.Add(("repo root: {0}" -f $SummaryObject.repoRoot))
    $textLines.Add(("outer workspace root: {0}" -f $SummaryObject.outerWorkspaceRoot))
    $textLines.Add(("build dir: {0}" -f $SummaryObject.buildDir))
    $textLines.Add(("executable: {0}" -f $SummaryObject.executablePath))
    $textLines.Add(("artifact root: {0}" -f $SummaryObject.artifactRoot))
    $textLines.Add("")
    $textLines.Add("suites:")
    foreach ($suite in @($SummaryObject.suites)) {
        $textLines.Add(("  {0}: {1}" -f $suite.name, $suite.result))
        if (-not [string]::IsNullOrWhiteSpace($suite.observedProvenanceMode)) {
            $textLines.Add(("    observed provenance: {0}" -f $suite.observedProvenanceMode))
        }
        if (-not [string]::IsNullOrWhiteSpace($suite.artifactOutputDir)) {
            $textLines.Add(("    artifact dir: {0}" -f $suite.artifactOutputDir))
        }
        if (-not [string]::IsNullOrWhiteSpace($suite.textSummaryPath)) {
            $textLines.Add(("    text summary: {0}" -f $suite.textSummaryPath))
        }
    }
    $textLines.Add("")
    $textLines.Add("resumed-denial scenarios:")
    foreach ($scenarioName in @("happy", "gate", "recovery")) {
        $scenarioStatus = [string](Get-OptionalPropertyValue -Object $SummaryObject.resumedDenial.scenarioStatuses -Name $scenarioName -DefaultValue "UNKNOWN")
        $scenarioProvenance = [string](Get-OptionalPropertyValue -Object $SummaryObject.resumedDenial.scenarioProvenance -Name $scenarioName -DefaultValue "")
        $line = "  {0}: {1}" -f $scenarioName, $scenarioStatus
        if (-not [string]::IsNullOrWhiteSpace($scenarioProvenance)) {
            $line += " (" + $scenarioProvenance + ")"
        }
        $textLines.Add($line)
    }

    if ($SummaryObject.runNeighborSurfaceSuite) {
        $textLines.Add("")
        $textLines.Add("neighbor-surfaces:")
        if (-not [string]::IsNullOrWhiteSpace([string](Get-OptionalPropertyValue -Object $SummaryObject.neighborSurfaces -Name "requestedProfileMode" -DefaultValue ""))) {
            $textLines.Add(("  requested profile mode: {0}" -f [string]$SummaryObject.neighborSurfaces.requestedProfileMode))
        }
        if (-not [string]::IsNullOrWhiteSpace([string]$SummaryObject.neighborSurfaces.requestedProfile)) {
            $textLines.Add(("  requested profile: {0}" -f [string]$SummaryObject.neighborSurfaces.requestedProfile))
        }
        if (-not [string]::IsNullOrWhiteSpace([string]$SummaryObject.neighborSurfaces.resolvedProfile)) {
            $textLines.Add(("  resolved profile: {0}" -f [string]$SummaryObject.neighborSurfaces.resolvedProfile))
        }
        if (-not [string]::IsNullOrWhiteSpace([string]$SummaryObject.neighborSurfaces.selectionOrigin)) {
            $textLines.Add(("  selection origin: {0}" -f [string]$SummaryObject.neighborSurfaces.selectionOrigin))
        }
        if (-not [string]::IsNullOrWhiteSpace([string](Get-OptionalPropertyValue -Object $SummaryObject.neighborSurfaces -Name "profileSelectionReason" -DefaultValue ""))) {
            $textLines.Add(("  profile selection reason: {0}" -f [string]$SummaryObject.neighborSurfaces.profileSelectionReason))
        }
        if (@((Get-OptionalPropertyValue -Object $SummaryObject.neighborSurfaces -Name "profileSelectionRuleIds" -DefaultValue @())).Count -gt 0) {
            $textLines.Add(("  matched profile rules: {0}" -f (@($SummaryObject.neighborSurfaces.profileSelectionRuleIds) -join ", ")))
        }
        if (-not [string]::IsNullOrWhiteSpace([string](Get-OptionalPropertyValue -Object $SummaryObject.neighborSurfaces -Name "profileSelectionDiffBase" -DefaultValue ""))) {
            $textLines.Add(("  diff base: {0}" -f [string]$SummaryObject.neighborSurfaces.profileSelectionDiffBase))
        }
        if (-not [string]::IsNullOrWhiteSpace([string](Get-OptionalPropertyValue -Object $SummaryObject.neighborSurfaces -Name "profileSelectionDiffHead" -DefaultValue ""))) {
            $textLines.Add(("  diff head: {0}" -f [string]$SummaryObject.neighborSurfaces.profileSelectionDiffHead))
        }
        if (-not [string]::IsNullOrWhiteSpace([string](Get-OptionalPropertyValue -Object $SummaryObject.neighborSurfaces -Name "profileSelectionChangedPathSource" -DefaultValue ""))) {
            $textLines.Add(("  changed path source: {0}" -f [string]$SummaryObject.neighborSurfaces.profileSelectionChangedPathSource))
        }
        if (@((Get-OptionalPropertyValue -Object $SummaryObject.neighborSurfaces -Name "profileSelectionChangedPaths" -DefaultValue @())).Count -gt 0) {
            $textLines.Add(("  changed paths: {0}" -f (@($SummaryObject.neighborSurfaces.profileSelectionChangedPaths) -join ", ")))
        }
        if (@($SummaryObject.neighborSurfaces.selectedGroupNames).Count -gt 0) {
            $textLines.Add(("  selected groups: {0}" -f (@($SummaryObject.neighborSurfaces.selectedGroupNames) -join ", ")))
        }
        if (@($SummaryObject.neighborSurfaces.selectedSurfaceNames).Count -gt 0) {
            $textLines.Add(("  selected surfaces: {0}" -f (@($SummaryObject.neighborSurfaces.selectedSurfaceNames) -join ", ")))
        }
        $textLines.Add(("  passing surfaces: {0}" -f $SummaryObject.neighborSurfaces.passingSurfaceCount))
        $textLines.Add(("  failing surfaces: {0}" -f $SummaryObject.neighborSurfaces.failingSurfaceCount))
        if (-not [string]::IsNullOrWhiteSpace([string]$SummaryObject.neighborSurfaces.junitOutputPath)) {
            $textLines.Add(("  junit output: {0}" -f [string]$SummaryObject.neighborSurfaces.junitOutputPath))
        }
        foreach ($groupResult in @($SummaryObject.neighborSurfaces.groups)) {
            $textLines.Add(("  group {0}: {1}" -f $groupResult.name, $groupResult.overallStatus))
        }
        foreach ($surface in @($SummaryObject.neighborSurfaces.surfaces)) {
            $textLines.Add(("  {0}: {1}" -f $surface.name, $surface.overallStatus))
        }
        if (@($SummaryObject.neighborSurfaces.failedSurfaces).Count -gt 0) {
            $textLines.Add("  failed surfaces:")
            foreach ($failedSurface in @($SummaryObject.neighborSurfaces.failedSurfaces)) {
                $textLines.Add(("    {0}" -f $failedSurface.name))
                $textLines.Add(("      profile: {0}" -f $(if ([string]::IsNullOrWhiteSpace([string]$failedSurface.profile)) { "<none>" } else { [string]$failedSurface.profile })))
                $textLines.Add(("      groups: {0}" -f $(if (@($failedSurface.groups).Count -gt 0) { (@($failedSurface.groups) -join ", ") } else { "<none>" })))
                $textLines.Add(("      reason: {0}" -f [string]$failedSurface.conciseReason))
                if (-not [string]::IsNullOrWhiteSpace([string]$failedSurface.summaryPath)) {
                    $textLines.Add(("      summary: {0}" -f [string]$failedSurface.summaryPath))
                }
            }
        }
    }

    $textLines | Set-Content -LiteralPath $TextSummaryPath -Encoding UTF8
    $SummaryObject | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $JsonSummaryPath -Encoding UTF8
}

function Write-CiSummary {
    param(
        [string]$Result,
        [int]$ExitCode,
        [string]$ProvenanceMode,
        [string]$ExecutionMode,
        [string]$ExecutionModeReason,
        [string]$ResolvedRepoRoot,
        [string]$ResolvedWorkspaceRoot,
        [string]$WorkspaceSource,
        [string]$ResolvedBuildDir,
        [string]$ResolvedExecutablePath,
        [string]$VerificationCommandLine,
        [string]$NeighborVerificationCommandLine,
        [string]$ResolvedArtifactOutputDir,
        [string]$ArtifactTextSummaryPath,
        [string]$ArtifactJsonSummaryPath,
        [string]$NeighborArtifactTextSummaryPath,
        [string]$NeighborArtifactJsonSummaryPath,
        [string]$CombinedTextSummaryPath,
        [string]$CombinedJsonSummaryPath,
        [string]$ResumedDenialResult,
        [string]$NeighborSurfaceResult,
        [string]$NeighborSurfaceProfile,
        [object]$NeighborSurfaceSelection,
        [string[]]$NeighborSurfaceGroups,
        [string[]]$NeighborSurfaces,
        [string]$NeighborJUnitOutputPath,
        [switch]$FullNeighborMatrixRequested,
        [switch]$NeighborSurfaceSuiteEnabled
    )

    Write-Heading "CI Summary"
    Write-Host ("result: {0}" -f $Result)
    Write-Host ("exit code: {0}" -f $ExitCode)
    Write-Host ("provenance mode: {0}" -f $ProvenanceMode)
    if (-not [string]::IsNullOrWhiteSpace($ExecutionMode)) {
        Write-Host ("execution mode: {0}" -f $ExecutionMode)
    }
    if (-not [string]::IsNullOrWhiteSpace($ExecutionModeReason)) {
        Write-Host ("execution mode reason: {0}" -f $ExecutionModeReason)
    }
    Write-Host ("repo root: {0}" -f $ResolvedRepoRoot)
    Write-Host ("outer workspace root: {0}" -f $ResolvedWorkspaceRoot)
    Write-Host ("outer workspace source: {0}" -f $WorkspaceSource)
    Write-Host ("build dir: {0}" -f $ResolvedBuildDir)
    if (-not [string]::IsNullOrWhiteSpace($ResolvedExecutablePath)) {
        Write-Host ("executable: {0}" -f $ResolvedExecutablePath)
    }
    if (-not [string]::IsNullOrWhiteSpace($VerificationCommandLine)) {
        Write-Host ("resumed-denial command: {0}" -f $VerificationCommandLine)
    }
    if (-not [string]::IsNullOrWhiteSpace($NeighborVerificationCommandLine)) {
        Write-Host ("neighbor-surface command: {0}" -f $NeighborVerificationCommandLine)
    }
    if ($NeighborSurfaceSuiteEnabled) {
        if ($null -ne $NeighborSurfaceSelection) {
            $requestedProfileMode = [string](Get-OptionalPropertyValue -Object $NeighborSurfaceSelection -Name "RequestedProfileMode" -DefaultValue "")
            $requestedProfile = [string](Get-OptionalPropertyValue -Object $NeighborSurfaceSelection -Name "RequestedProfile" -DefaultValue "")
            $resolvedProfile = [string](Get-OptionalPropertyValue -Object $NeighborSurfaceSelection -Name "ResolvedProfile" -DefaultValue "")
            $selectionReason = [string](Get-OptionalPropertyValue -Object $NeighborSurfaceSelection -Name "SelectionReason" -DefaultValue "")
            $matchedRuleIds = @((Get-OptionalPropertyValue -Object $NeighborSurfaceSelection -Name "MatchedRuleIds" -DefaultValue @()))
            $selectionDiffBase = [string](Get-OptionalPropertyValue -Object $NeighborSurfaceSelection -Name "DiffBase" -DefaultValue "")
            $selectionDiffHead = [string](Get-OptionalPropertyValue -Object $NeighborSurfaceSelection -Name "DiffHead" -DefaultValue "")
            $changedPathSource = [string](Get-OptionalPropertyValue -Object $NeighborSurfaceSelection -Name "ChangedPathSource" -DefaultValue "")

            if (-not [string]::IsNullOrWhiteSpace($requestedProfileMode)) {
                Write-Host ("neighbor-surface profile mode: {0}" -f $requestedProfileMode)
            }
            if (-not [string]::IsNullOrWhiteSpace($requestedProfile)) {
                Write-Host ("neighbor-surface requested profile: {0}" -f $requestedProfile)
            }
            if (-not [string]::IsNullOrWhiteSpace($resolvedProfile)) {
                Write-Host ("neighbor-surface resolved profile: {0}" -f $resolvedProfile)
            }
            if (-not [string]::IsNullOrWhiteSpace($selectionReason)) {
                Write-Host ("neighbor-surface selection reason: {0}" -f $selectionReason)
            }
            if ($matchedRuleIds.Count -gt 0) {
                Write-Host ("neighbor-surface matched rules: {0}" -f ($matchedRuleIds -join ", "))
            }
            if (-not [string]::IsNullOrWhiteSpace($selectionDiffBase)) {
                Write-Host ("neighbor-surface diff base: {0}" -f $selectionDiffBase)
            }
            if (-not [string]::IsNullOrWhiteSpace($selectionDiffHead)) {
                Write-Host ("neighbor-surface diff head: {0}" -f $selectionDiffHead)
            }
            if (-not [string]::IsNullOrWhiteSpace($changedPathSource)) {
                Write-Host ("neighbor-surface changed path source: {0}" -f $changedPathSource)
            }
        }
        if (-not [string]::IsNullOrWhiteSpace($NeighborSurfaceProfile)) {
            Write-Host ("neighbor-surface profile: {0}" -f $NeighborSurfaceProfile)
        }
        if (@($NeighborSurfaceGroups).Count -gt 0) {
            Write-Host ("neighbor-surface groups: {0}" -f (@($NeighborSurfaceGroups) -join ", "))
        }
        if (@($NeighborSurfaces).Count -gt 0) {
            Write-Host ("neighbor-surfaces: {0}" -f (@($NeighborSurfaces) -join ", "))
        }
        if ($FullNeighborMatrixRequested) {
            Write-Host "neighbor-surface full matrix: yes"
        }
        if (-not [string]::IsNullOrWhiteSpace($NeighborJUnitOutputPath)) {
            Write-Host ("neighbor-surface junit: {0}" -f $NeighborJUnitOutputPath)
        }
    }
    if (-not [string]::IsNullOrWhiteSpace($ResolvedArtifactOutputDir)) {
        Write-Host ("artifact root dir: {0}" -f $ResolvedArtifactOutputDir)
    }
    if (-not [string]::IsNullOrWhiteSpace($ArtifactTextSummaryPath)) {
        Write-Host ("resumed-denial text summary: {0}" -f $ArtifactTextSummaryPath)
    }
    if (-not [string]::IsNullOrWhiteSpace($ArtifactJsonSummaryPath)) {
        Write-Host ("resumed-denial json summary: {0}" -f $ArtifactJsonSummaryPath)
    }
    if (-not [string]::IsNullOrWhiteSpace($NeighborArtifactTextSummaryPath)) {
        Write-Host ("neighbor-surface text summary: {0}" -f $NeighborArtifactTextSummaryPath)
    }
    if (-not [string]::IsNullOrWhiteSpace($NeighborArtifactJsonSummaryPath)) {
        Write-Host ("neighbor-surface json summary: {0}" -f $NeighborArtifactJsonSummaryPath)
    }
    if (-not [string]::IsNullOrWhiteSpace($CombinedTextSummaryPath)) {
        Write-Host ("combined suite text summary: {0}" -f $CombinedTextSummaryPath)
    }
    if (-not [string]::IsNullOrWhiteSpace($CombinedJsonSummaryPath)) {
        Write-Host ("combined suite json summary: {0}" -f $CombinedJsonSummaryPath)
    }

    Write-Host ("resumed-denial suite: {0}" -f $ResumedDenialResult)
    if ($NeighborSurfaceSuiteEnabled) {
        Write-Host ("neighbor-surface suite: {0}" -f $NeighborSurfaceResult)
    }
}

$resolvedRepoRoot = ""
$resolvedOuterWorkspaceRoot = ""
$workspaceSource = ""
$resolvedBuildDir = ""
$resolvedExecutablePath = ""
$resolvedNeighborJUnitOutputPath = ""
$verificationCommandLine = ""
$neighborVerificationCommandLine = ""
$resolvedArtifactOutputDir = ""
$resumedArtifactOutputDir = ""
$neighborArtifactOutputDir = ""
$artifactTextSummaryPath = ""
$artifactJsonSummaryPath = ""
$neighborArtifactTextSummaryPath = ""
$neighborArtifactJsonSummaryPath = ""
$combinedTextSummaryPath = ""
$combinedJsonSummaryPath = ""
$verificationOutputLogPath = ""
$artifactMetadataPath = ""
$verificationOutputMetadata = $null
$artifactLayout = $null
$normalizedNeighborSurfaceGroups = @(Get-TrimmedUniqueValues -Values $NeighborSurfaceGroup)
$normalizedNeighborSurfaces = @(Get-TrimmedUniqueValues -Values $NeighborSurface)
$normalizedChangedPaths = @(Get-TrimmedUniqueValues -Values $ChangedPath)
$normalizedExecutionMode = if ([string]::IsNullOrWhiteSpace($ExecutionMode)) { "" } else { $ExecutionMode.Trim() }
$normalizedExecutionModeReason = if ([string]::IsNullOrWhiteSpace($ExecutionModeReason)) { "" } else { $ExecutionModeReason.Trim() }
$neighborSurfaceSelection = $null
$effectiveNeighborSurfaceProfile = $NeighborSurfaceProfile
$resumedDenialResult = "UNKNOWN"
$neighborSurfaceResult = if ($RunNeighborSurfaceSuite) { "NOT_RUN" } else { "NOT_REQUESTED" }
$resultLabel = "FAIL"
$finalExitCode = 1

$validExecutionModes = @(
    "auto-diff",
    "explicit-manual",
    "scheduled-full",
    "not-requested"
)
if (-not [string]::IsNullOrWhiteSpace($normalizedExecutionMode) -and $validExecutionModes -notcontains $normalizedExecutionMode) {
    throw ("Unsupported -ExecutionMode '{0}'. Expected one of: {1}" -f $normalizedExecutionMode, ($validExecutionModes -join ", "))
}

try {
    $resolvedRepoRoot = Get-RepoRootFromRequest -RequestedRoot $RepoRoot
    if (-not $RunNeighborSurfaceSuite -and ($normalizedNeighborSurfaceGroups.Count -gt 0 -or $normalizedNeighborSurfaces.Count -gt 0 -or $FullNeighborMatrix -or -not [string]::IsNullOrWhiteSpace($NeighborSurfaceProfile) -or -not [string]::IsNullOrWhiteSpace($DiffBase) -or -not [string]::IsNullOrWhiteSpace($DiffHead) -or $normalizedChangedPaths.Count -gt 0 -or -not [string]::IsNullOrWhiteSpace($JUnitOutputPath))) {
        throw "Neighbor surface filters, -NeighborSurfaceProfile, -DiffBase, -DiffHead, -ChangedPath, -JUnitOutputPath, and -FullNeighborMatrix require -RunNeighborSurfaceSuite."
    }
    if ($FullNeighborMatrix -and ($normalizedNeighborSurfaceGroups.Count -gt 0 -or $normalizedNeighborSurfaces.Count -gt 0)) {
        throw "-FullNeighborMatrix cannot be combined with -NeighborSurfaceGroup or -NeighborSurface."
    }
    if (-not [string]::IsNullOrWhiteSpace($NeighborSurfaceProfile) -and ($normalizedNeighborSurfaceGroups.Count -gt 0 -or $normalizedNeighborSurfaces.Count -gt 0 -or $FullNeighborMatrix)) {
        throw "-NeighborSurfaceProfile cannot be combined with -NeighborSurfaceGroup, -NeighborSurface, or -FullNeighborMatrix."
    }
    if ($RunNeighborSurfaceSuite) {
        $neighborSurfaceSelection = Resolve-NeighborProfileSelectionContext `
            -ResolvedRepoRoot $resolvedRepoRoot `
            -RequestedProfile $NeighborSurfaceProfile `
            -NormalizedNeighborSurfaceGroups $normalizedNeighborSurfaceGroups `
            -NormalizedNeighborSurfaces $normalizedNeighborSurfaces `
            -FullNeighborMatrixRequested:$FullNeighborMatrix `
            -DiffBase $DiffBase `
            -DiffHead $DiffHead `
            -ChangedPaths $normalizedChangedPaths
        $effectiveNeighborSurfaceProfile = [string](Get-OptionalPropertyValue -Object $neighborSurfaceSelection -Name "RunnerProfile" -DefaultValue "")
    }
    if ([string]::IsNullOrWhiteSpace($normalizedExecutionMode)) {
        if (-not $RunNeighborSurfaceSuite) {
            $normalizedExecutionMode = "not-requested"
        }
        elseif ($null -ne $neighborSurfaceSelection) {
            $requestedProfileMode = [string](Get-OptionalPropertyValue -Object $neighborSurfaceSelection -Name "RequestedProfileMode" -DefaultValue "")
            if ($requestedProfileMode -eq "auto") {
                $normalizedExecutionMode = "auto-diff"
            }
            elseif ($requestedProfileMode -eq "explicit") {
                $normalizedExecutionMode = "explicit-manual"
            }
        }
    }
    if ([string]::IsNullOrWhiteSpace($normalizedExecutionModeReason)) {
        $normalizedExecutionModeReason = switch ($normalizedExecutionMode) {
            "auto-diff" {
                [string](Get-OptionalPropertyValue -Object $neighborSurfaceSelection -Name "SelectionReason" -DefaultValue "Diff-aware neighboring-surface profile selection was requested.")
            }
            "explicit-manual" {
                [string](Get-OptionalPropertyValue -Object $neighborSurfaceSelection -Name "SelectionReason" -DefaultValue "Explicit neighboring-surface selection was requested.")
            }
            "scheduled-full" {
                "Scheduled broad neighboring-surface coverage requested the full-expanded profile."
            }
            default {
                "Neighbor-surface suite was not requested."
            }
        }
    }
    $resolvedNeighborJUnitOutputPath = Resolve-JUnitOutputPath -ResolvedRepoRoot $resolvedRepoRoot -RequestedJUnitOutputPath $JUnitOutputPath
    if ($CollectArtifacts) {
        Assert-PathExists -LiteralPath $script:ArtifactCollectorPath -Description "artifact collector helper" -ActionHint "Verify that tools\\collect_resumed_denial_artifacts.ps1 exists in this checkout"
        $artifactLayout = Resolve-ArtifactLayout `
            -ResolvedRepoRoot $resolvedRepoRoot `
            -RequestedArtifactOutputDir $ArtifactOutputDir `
            -IncludeNeighborSurfaceSuite:$RunNeighborSurfaceSuite
        $resolvedArtifactOutputDir = $artifactLayout.RootOutputDir
        $resumedArtifactOutputDir = $artifactLayout.ResumedDenialOutputDir
        $neighborArtifactOutputDir = $artifactLayout.NeighborSurfaceOutputDir
        $combinedTextSummaryPath = $artifactLayout.CombinedTextSummaryPath
        $combinedJsonSummaryPath = $artifactLayout.CombinedJsonSummaryPath
        $artifactMetadataDirectory = Join-Path $resumedArtifactOutputDir "metadata"
        New-Item -ItemType Directory -Path $artifactMetadataDirectory -Force | Out-Null
        $verificationOutputLogPath = Join-Path $artifactMetadataDirectory "verify_resumed_denial_ci_output.log"
        $artifactMetadataPath = Join-Path $artifactMetadataDirectory "verification_metadata.json"
    }

    $workspaceResolution = Resolve-OuterWorkspaceRoot -ResolvedRepoRoot $resolvedRepoRoot -RequestedWorkspaceRoot $OuterWorkspaceRoot
    $resolvedOuterWorkspaceRoot = $workspaceResolution.Root
    $workspaceSource = $workspaceResolution.Source
    $resolvedBuildDir = Resolve-BuildDirPath -WorkspaceRoot $resolvedOuterWorkspaceRoot -RequestedBuildDir $BuildDir
    $resolvedExecutablePath = Resolve-ExecutablePath -WorkspaceRoot $resolvedOuterWorkspaceRoot -RequestedExecutablePath $ExePath

    Assert-PathExists -LiteralPath $script:MainRunnerPath -Description "current-main regression runner" -ActionHint "Verify that tools\\verify_resumed_denial_main.ps1 exists in this checkout"

    $gameDirPath = Join-Path $resolvedRepoRoot ("logs\latest\{0}\runtime\valve-fixture" -f $script:PromptId)
    Assert-PathExists -LiteralPath $gameDirPath -Description "prompt-scoped valve-fixture inputs" -ActionHint "Provision the resumed-denial runtime fixture under logs\\latest before running the CI wrapper"

    if (-not [string]::IsNullOrWhiteSpace($resolvedExecutablePath)) {
        Assert-PathExists -LiteralPath $resolvedExecutablePath -Description "explicit hlhost.exe" -ActionHint "Pass a valid -ExePath or omit it so the runner can build or reuse the configured build output"
    }
    elseif ($NoBuild -or $UseExistingBinary) {
        $resolvedExecutablePath = Get-ExpectedExecutablePath -ResolvedBuildDir $resolvedBuildDir
        Assert-PathExists -LiteralPath $resolvedExecutablePath -Description "existing hlhost.exe" -ActionHint "Re-run without -NoBuild/-UseExistingBinary, change -BuildDir, or pass -ExePath"
    }

    Write-Heading "Resolved Inputs"
    Write-Host ("Repo root: {0}" -f $resolvedRepoRoot)
    Write-Host ("Outer workspace root: {0}" -f $resolvedOuterWorkspaceRoot)
    Write-Host ("Outer workspace source: {0}" -f $workspaceSource)
    Write-Host ("Provenance mode: {0}" -f $ProvenanceMode)
    Write-Host ("Build dir: {0}" -f $resolvedBuildDir)
    Write-Host ("Game dir: {0}" -f $gameDirPath)
    if ($RunNeighborSurfaceSuite -and $null -ne $neighborSurfaceSelection) {
        $requestedProfileMode = [string](Get-OptionalPropertyValue -Object $neighborSurfaceSelection -Name "RequestedProfileMode" -DefaultValue "")
        $requestedProfile = [string](Get-OptionalPropertyValue -Object $neighborSurfaceSelection -Name "RequestedProfile" -DefaultValue "")
        $resolvedProfile = [string](Get-OptionalPropertyValue -Object $neighborSurfaceSelection -Name "ResolvedProfile" -DefaultValue "")
        $selectionReason = [string](Get-OptionalPropertyValue -Object $neighborSurfaceSelection -Name "SelectionReason" -DefaultValue "")
        $matchedRuleIds = @((Get-OptionalPropertyValue -Object $neighborSurfaceSelection -Name "MatchedRuleIds" -DefaultValue @()))
        $selectionDiffBase = [string](Get-OptionalPropertyValue -Object $neighborSurfaceSelection -Name "DiffBase" -DefaultValue "")
        $selectionDiffHead = [string](Get-OptionalPropertyValue -Object $neighborSurfaceSelection -Name "DiffHead" -DefaultValue "")

        if (-not [string]::IsNullOrWhiteSpace($requestedProfileMode)) {
            Write-Host ("Neighbor-surface profile mode: {0}" -f $requestedProfileMode)
        }
        if (-not [string]::IsNullOrWhiteSpace($requestedProfile)) {
            Write-Host ("Neighbor-surface requested profile: {0}" -f $requestedProfile)
        }
        if (-not [string]::IsNullOrWhiteSpace($resolvedProfile)) {
            Write-Host ("Neighbor-surface resolved profile: {0}" -f $resolvedProfile)
        }
        if (-not [string]::IsNullOrWhiteSpace($selectionReason)) {
            Write-Host ("Neighbor-surface selection reason: {0}" -f $selectionReason)
        }
        if ($matchedRuleIds.Count -gt 0) {
            Write-Host ("Neighbor-surface matched rules: {0}" -f ($matchedRuleIds -join ", "))
        }
        if (-not [string]::IsNullOrWhiteSpace($selectionDiffBase)) {
            Write-Host ("Neighbor-surface diff base: {0}" -f $selectionDiffBase)
        }
        if (-not [string]::IsNullOrWhiteSpace($selectionDiffHead)) {
            Write-Host ("Neighbor-surface diff head: {0}" -f $selectionDiffHead)
        }
    }
    if (-not [string]::IsNullOrWhiteSpace($effectiveNeighborSurfaceProfile)) {
        Write-Host ("Neighbor-surface profile: {0}" -f $effectiveNeighborSurfaceProfile)
    }
    if ($normalizedNeighborSurfaceGroups.Count -gt 0) {
        Write-Host ("Neighbor-surface groups: {0}" -f ($normalizedNeighborSurfaceGroups -join ", "))
    }
    if ($normalizedNeighborSurfaces.Count -gt 0) {
        Write-Host ("Neighbor-surfaces: {0}" -f ($normalizedNeighborSurfaces -join ", "))
    }
    if ($FullNeighborMatrix) {
        Write-Host "Neighbor-surface full matrix: yes"
    }
    if (-not [string]::IsNullOrWhiteSpace($resolvedNeighborJUnitOutputPath)) {
        Write-Host ("Neighbor-surface JUnit: {0}" -f $resolvedNeighborJUnitOutputPath)
    }
    if (-not [string]::IsNullOrWhiteSpace($resolvedExecutablePath)) {
        Write-Host ("Executable: {0}" -f $resolvedExecutablePath)
    }
    else {
        Write-Host "Executable: <resolved by verify_resumed_denial_main.ps1 after build>"
    }

    $hostExecutable = Get-CurrentHostExecutable
    $verificationArguments = New-Object System.Collections.Generic.List[string]
    $verificationArguments.Add("-NoLogo")
    $verificationArguments.Add("-NoProfile")
    $verificationArguments.Add("-ExecutionPolicy")
    $verificationArguments.Add("Bypass")
    $verificationArguments.Add("-File")
    $verificationArguments.Add($script:MainRunnerPath)
    $verificationArguments.Add("-RepoRoot")
    $verificationArguments.Add($resolvedRepoRoot)
    $verificationArguments.Add("-OuterWorkspaceRoot")
    $verificationArguments.Add($resolvedOuterWorkspaceRoot)
    $verificationArguments.Add("-ProvenanceMode")
    $verificationArguments.Add($ProvenanceMode)
    $verificationArguments.Add("-BuildDir")
    $verificationArguments.Add($resolvedBuildDir)
    if (-not [string]::IsNullOrWhiteSpace($resolvedExecutablePath)) {
        $verificationArguments.Add("-ExePath")
        $verificationArguments.Add($resolvedExecutablePath)
    }
    if ($NoBuild) {
        $verificationArguments.Add("-NoBuild")
    }
    if ($SkipRecovery) {
        $verificationArguments.Add("-SkipRecovery")
    }
    if ($NoExecute) {
        $verificationArguments.Add("-NoExecute")
    }
    if ($UseExistingBinary) {
        $verificationArguments.Add("-UseExistingBinary")
    }

    $verificationCommandLine = Format-CommandLine -ExecutablePath $hostExecutable -Arguments $verificationArguments.ToArray()

    Write-Heading "Verification"
    Write-Host "Delegating to the current-main regression runner."
    Write-Host ("  {0}" -f $verificationCommandLine)

    if ($CollectArtifacts) {
        & $hostExecutable @($verificationArguments.ToArray()) 2>&1 | Tee-Object -FilePath $verificationOutputLogPath
        $finalExitCode = if ($null -eq $LASTEXITCODE) { 0 } else { $LASTEXITCODE }
        $verificationOutputMetadata = Get-VerificationOutputMetadata `
            -OutputLogPath $verificationOutputLogPath `
            -NoExecuteRequested:$NoExecute `
            -SkipRecoveryRequested:$SkipRecovery
    }
    else {
        & $hostExecutable @($verificationArguments.ToArray())
        $finalExitCode = if ($null -eq $LASTEXITCODE) { 0 } else { $LASTEXITCODE }
    }

    if ($finalExitCode -ne 0) {
        throw ("verify_resumed_denial_main.ps1 failed with exit code {0}." -f $finalExitCode)
    }

    if ($CollectArtifacts -and $null -ne $verificationOutputMetadata -and $verificationOutputMetadata.OverallResult -ne "UNKNOWN") {
        $resultLabel = $verificationOutputMetadata.OverallResult
    }
    else {
        $resultLabel = if ($NoExecute) { "NOEXECUTE" } else { "PASS" }
    }
    $resumedDenialResult = $resultLabel

    if ([string]::IsNullOrWhiteSpace($resolvedExecutablePath)) {
        $resolvedExecutablePath = Get-ExpectedExecutablePath -ResolvedBuildDir $resolvedBuildDir
    }

    if ($RunNeighborSurfaceSuite) {
        Assert-PathExists -LiteralPath $script:NeighborSuiteRunnerPath -Description "neighbor-surface suite runner" -ActionHint "Verify that tools\\verify_signon_neighbor_surfaces_main.ps1 exists in this checkout"
        Assert-PathExists -LiteralPath $resolvedExecutablePath -Description "resolved hlhost.exe for the neighbor-surface suite" -ActionHint "Ensure the resumed-denial suite built or resolved the expected current-main binary before running the neighbor-surface suite"

        $neighborArguments = New-Object System.Collections.Generic.List[string]
        $neighborArguments.Add("-NoLogo")
        $neighborArguments.Add("-NoProfile")
        $neighborArguments.Add("-ExecutionPolicy")
        $neighborArguments.Add("Bypass")
        $neighborArguments.Add("-File")
        $neighborArguments.Add($script:NeighborSuiteRunnerPath)
        $neighborArguments.Add("-RepoRoot")
        $neighborArguments.Add($resolvedRepoRoot)
        $neighborArguments.Add("-OuterWorkspaceRoot")
        $neighborArguments.Add($resolvedOuterWorkspaceRoot)
        $neighborArguments.Add("-BuildDir")
        $neighborArguments.Add($resolvedBuildDir)
        $neighborArguments.Add("-ExePath")
        $neighborArguments.Add($resolvedExecutablePath)
        $neighborArguments.Add("-UseExistingBinary")
        $neighborArguments.Add("-ProvenanceMode")
        $neighborArguments.Add($ProvenanceMode)
        if (-not [string]::IsNullOrWhiteSpace($effectiveNeighborSurfaceProfile)) {
            $neighborArguments.Add("-Profile")
            $neighborArguments.Add($effectiveNeighborSurfaceProfile)
        }
        if ($normalizedNeighborSurfaceGroups.Count -gt 0) {
            $neighborArguments.Add("-Group")
            foreach ($groupName in $normalizedNeighborSurfaceGroups) {
                $neighborArguments.Add($groupName)
            }
        }
        if ($normalizedNeighborSurfaces.Count -gt 0) {
            $neighborArguments.Add("-Surface")
            foreach ($surfaceName in $normalizedNeighborSurfaces) {
                $neighborArguments.Add($surfaceName)
            }
        }
        if ($FullNeighborMatrix) {
            $neighborArguments.Add("-FullMatrix")
        }
        if (-not [string]::IsNullOrWhiteSpace($resolvedNeighborJUnitOutputPath)) {
            $neighborArguments.Add("-JUnitOutputPath")
            $neighborArguments.Add($resolvedNeighborJUnitOutputPath)
        }
        if ($NoExecute) {
            $neighborArguments.Add("-NoExecute")
        }
        if ($CollectArtifacts) {
            $neighborArguments.Add("-CollectArtifacts")
            $neighborArguments.Add("-ArtifactOutputDir")
            $neighborArguments.Add($neighborArtifactOutputDir)
        }

        $neighborVerificationCommandLine = Format-CommandLine -ExecutablePath $hostExecutable -Arguments $neighborArguments.ToArray()
        Write-Heading "Neighbor Surface Suite"
        Write-Host "Running the opt-in neighboring signon surface suite."
        Write-Host ("  {0}" -f $neighborVerificationCommandLine)

        & $hostExecutable @($neighborArguments.ToArray())
        $neighborExitCode = if ($null -eq $LASTEXITCODE) { 0 } else { $LASTEXITCODE }
        if ($neighborExitCode -eq 0) {
            $neighborSurfaceResult = if ($NoExecute) { "NOEXECUTE" } else { "PASS" }
            $resultLabel = Get-CombinedResult -PrimaryResult $resumedDenialResult -SecondaryResult $neighborSurfaceResult
        }
        else {
            $neighborSurfaceResult = "FAIL"
            $resultLabel = "FAIL"
            $finalExitCode = $neighborExitCode
            throw ("verify_signon_neighbor_surfaces_main.ps1 failed with exit code {0}." -f $neighborExitCode)
        }
    }

    $finalExitCode = 0
}
catch {
    if ($finalExitCode -eq 0) {
        $finalExitCode = 1
    }

    Write-Heading "Failure"
    Write-Host $_.Exception.Message
    $resultLabel = "FAIL"
}
finally {
    if ($CollectArtifacts -and -not [string]::IsNullOrWhiteSpace($resolvedRepoRoot) -and -not [string]::IsNullOrWhiteSpace($resolvedArtifactOutputDir)) {
        try {
            if ($null -eq $verificationOutputMetadata) {
                $verificationOutputMetadata = Get-VerificationOutputMetadata `
                    -OutputLogPath $verificationOutputLogPath `
                    -NoExecuteRequested:$NoExecute `
                    -SkipRecoveryRequested:$SkipRecovery
            }

            Write-ArtifactMetadataFile `
                -MetadataPath $artifactMetadataPath `
                -ResultLabel $resultLabel `
                -ResolvedRepoRoot $resolvedRepoRoot `
                -ResolvedWorkspaceRoot $resolvedOuterWorkspaceRoot `
                -ResolvedBuildDir $resolvedBuildDir `
                -ResolvedExecutablePath $resolvedExecutablePath `
                -RequestedProvenanceMode $ProvenanceMode `
                -VerificationCommandLine $verificationCommandLine `
                -VerificationOutputLogPath $verificationOutputLogPath `
                -VerificationOutputMetadata $verificationOutputMetadata `
                -NeighborSurfaceSelection $neighborSurfaceSelection `
                -ExecutionMode $normalizedExecutionMode `
                -ExecutionModeReason $normalizedExecutionModeReason

            Write-Heading "Artifact Collection"
            Write-Host ("Resumed-denial output dir: {0}" -f $resumedArtifactOutputDir)
            & $script:ArtifactCollectorPath `
                -RepoRoot $resolvedRepoRoot `
                -LogsRoot (Join-Path $resolvedRepoRoot "logs\latest") `
                -OutputDir $resumedArtifactOutputDir `
                -RunLabelPattern ([string](Get-OptionalPropertyValue -Object $verificationOutputMetadata -Name "RunLabelPattern" -DefaultValue "verify-resumed-denial-main-*")) `
                -IncludeCodexArtifacts `
                -AllowMissingCodexArtifacts `
                -VerificationMetadataPath $artifactMetadataPath

            $artifactCollectorExitCode = if ($null -eq $LASTEXITCODE) { 0 } else { $LASTEXITCODE }
            if ($artifactCollectorExitCode -ne 0) {
                throw ("collect_resumed_denial_artifacts.ps1 failed with exit code {0}." -f $artifactCollectorExitCode)
            }

            $artifactTextSummaryPath = Join-Path $resumedArtifactOutputDir "resumed_denial_summary.txt"
            $artifactJsonSummaryPath = Join-Path $resumedArtifactOutputDir "resumed_denial_summary.json"

            if ($RunNeighborSurfaceSuite) {
                $neighborArtifactTextSummaryPath = Join-Path $neighborArtifactOutputDir "signon_neighbor_surface_summary.txt"
                $neighborArtifactJsonSummaryPath = Join-Path $neighborArtifactOutputDir "signon_neighbor_surface_summary.json"

                $resumedArtifactSummary = Read-JsonFileIfExists -JsonPath $artifactJsonSummaryPath
                $neighborArtifactSummary = Read-JsonFileIfExists -JsonPath $neighborArtifactJsonSummaryPath

                $resumedScenarioStatuses = if ($null -ne $resumedArtifactSummary) {
                    Get-ScenarioValueMapFromSummary `
                        -SummaryObject $resumedArtifactSummary `
                        -ValuePropertyName "status" `
                        -FallbackValues ([ordered]@{
                            happy = [string](Get-OptionalPropertyValue -Object $verificationOutputMetadata.ScenarioStatuses -Name "happy" -DefaultValue "UNKNOWN")
                            gate = [string](Get-OptionalPropertyValue -Object $verificationOutputMetadata.ScenarioStatuses -Name "gate" -DefaultValue "UNKNOWN")
                            recovery = [string](Get-OptionalPropertyValue -Object $verificationOutputMetadata.ScenarioStatuses -Name "recovery" -DefaultValue "UNKNOWN")
                        })
                }
                else {
                    [ordered]@{
                        happy = [string](Get-OptionalPropertyValue -Object $verificationOutputMetadata.ScenarioStatuses -Name "happy" -DefaultValue "UNKNOWN")
                        gate = [string](Get-OptionalPropertyValue -Object $verificationOutputMetadata.ScenarioStatuses -Name "gate" -DefaultValue "UNKNOWN")
                        recovery = [string](Get-OptionalPropertyValue -Object $verificationOutputMetadata.ScenarioStatuses -Name "recovery" -DefaultValue "UNKNOWN")
                    }
                }

                $resumedScenarioProvenance = if ($null -ne $resumedArtifactSummary) {
                    Get-ScenarioValueMapFromSummary `
                        -SummaryObject $resumedArtifactSummary `
                        -ValuePropertyName "provenanceMode" `
                        -FallbackValues ([ordered]@{
                            happy = [string](Get-OptionalPropertyValue -Object $verificationOutputMetadata.ScenarioProvenance -Name "happy" -DefaultValue "")
                            gate = [string](Get-OptionalPropertyValue -Object $verificationOutputMetadata.ScenarioProvenance -Name "gate" -DefaultValue "")
                            recovery = [string](Get-OptionalPropertyValue -Object $verificationOutputMetadata.ScenarioProvenance -Name "recovery" -DefaultValue "")
                        })
                }
                else {
                    [ordered]@{
                        happy = [string](Get-OptionalPropertyValue -Object $verificationOutputMetadata.ScenarioProvenance -Name "happy" -DefaultValue "")
                        gate = [string](Get-OptionalPropertyValue -Object $verificationOutputMetadata.ScenarioProvenance -Name "gate" -DefaultValue "")
                        recovery = [string](Get-OptionalPropertyValue -Object $verificationOutputMetadata.ScenarioProvenance -Name "recovery" -DefaultValue "")
                    }
                }

                $neighborSurfaceItems = @()
                $neighborGroupItems = @()
                $neighborFailedSurfaceItems = @()
                if ($null -ne $neighborArtifactSummary) {
                    $neighborSurfaceItems = @($neighborArtifactSummary.surfaces | ForEach-Object {
                            [ordered]@{
                                name = [string]$_.name
                                groups = @((Get-OptionalPropertyValue -Object $_ -Name "groups" -DefaultValue @()))
                                overallStatus = [string]$_.overallStatus
                            }
                        })
                    $neighborGroupItems = @($neighborArtifactSummary.groups | ForEach-Object {
                            [ordered]@{
                                name = [string]$_.name
                                overallStatus = [string]$_.overallStatus
                                surfaceNames = @((Get-OptionalPropertyValue -Object $_ -Name "surfaceNames" -DefaultValue @()))
                                passingSurfaceCount = [int](Get-OptionalPropertyValue -Object $_ -Name "passingSurfaceCount" -DefaultValue 0)
                                failingSurfaceCount = [int](Get-OptionalPropertyValue -Object $_ -Name "failingSurfaceCount" -DefaultValue 0)
                                notRunSurfaceCount = [int](Get-OptionalPropertyValue -Object $_ -Name "notRunSurfaceCount" -DefaultValue 0)
                            }
                        })
                    $neighborFailedSurfaceItems = @($neighborArtifactSummary.failedSurfaces | ForEach-Object {
                            [ordered]@{
                                name = [string]$_.name
                                groups = @((Get-OptionalPropertyValue -Object $_ -Name "groups" -DefaultValue @()))
                                profile = [string](Get-OptionalPropertyValue -Object $_ -Name "profile" -DefaultValue "")
                                conciseReason = [string](Get-OptionalPropertyValue -Object $_ -Name "conciseReason" -DefaultValue "")
                                summaryPath = [string](Get-OptionalPropertyValue -Object $_ -Name "summaryPath" -DefaultValue "")
                            }
                        })
                }

                $neighborSelectionMode = if ($null -ne $neighborArtifactSummary) {
                    [string](Get-OptionalPropertyValue -Object $neighborArtifactSummary -Name "selectionMode" -DefaultValue "")
                }
                elseif ($effectiveNeighborSurfaceProfile -eq "full-expanded") {
                    "full-matrix"
                }
                elseif ($effectiveNeighborSurfaceProfile -eq "checkpoint-extended") {
                    "filtered"
                }
                elseif ($FullNeighborMatrix) {
                    "full-matrix"
                }
                elseif ($normalizedNeighborSurfaceGroups.Count -gt 0 -or $normalizedNeighborSurfaces.Count -gt 0) {
                    "filtered"
                }
                else {
                    "default-enabled"
                }
                $neighborPassingSurfaceCount = @($neighborSurfaceItems | Where-Object { $_.overallStatus -eq "PASS" }).Count
                $neighborFailingSurfaceCount = @($neighborSurfaceItems | Where-Object { $_.overallStatus -eq "FAIL" }).Count
                $neighborRequestedProfileMode = [string](Get-OptionalPropertyValue -Object $neighborSurfaceSelection -Name "RequestedProfileMode" -DefaultValue "")
                $neighborRequestedProfile = [string](Get-OptionalPropertyValue -Object $neighborSurfaceSelection -Name "RequestedProfile" -DefaultValue "")
                $neighborResolvedProfile = [string](Get-OptionalPropertyValue -Object $neighborSurfaceSelection -Name "ResolvedProfile" -DefaultValue "")
                $neighborSelectionOrigin = [string](Get-OptionalPropertyValue -Object $neighborSurfaceSelection -Name "SelectionOrigin" -DefaultValue "")
                if ([string]::IsNullOrWhiteSpace($neighborSelectionOrigin)) {
                    $neighborSelectionOrigin = if ($null -ne $neighborArtifactSummary) { [string](Get-OptionalPropertyValue -Object $neighborArtifactSummary -Name "selectionOrigin" -DefaultValue "") } else { $(if (-not [string]::IsNullOrWhiteSpace($effectiveNeighborSurfaceProfile)) { "profile" } elseif ($FullNeighborMatrix -or $normalizedNeighborSurfaceGroups.Count -gt 0 -or $normalizedNeighborSurfaces.Count -gt 0) { "explicit" } else { "default" }) }
                }
                $neighborSelectionReason = [string](Get-OptionalPropertyValue -Object $neighborSurfaceSelection -Name "SelectionReason" -DefaultValue "")
                $neighborProfileSelectionRuleIds = @((Get-OptionalPropertyValue -Object $neighborSurfaceSelection -Name "MatchedRuleIds" -DefaultValue @()))
                $neighborProfileSelectionRules = @((Get-OptionalPropertyValue -Object $neighborSurfaceSelection -Name "MatchedRules" -DefaultValue @()))
                $neighborProfileSelectionDiffBase = [string](Get-OptionalPropertyValue -Object $neighborSurfaceSelection -Name "DiffBase" -DefaultValue "")
                $neighborProfileSelectionDiffHead = [string](Get-OptionalPropertyValue -Object $neighborSurfaceSelection -Name "DiffHead" -DefaultValue "")
                $neighborProfileSelectionChangedPathSource = [string](Get-OptionalPropertyValue -Object $neighborSurfaceSelection -Name "ChangedPathSource" -DefaultValue "")
                $neighborProfileSelectionChangedPaths = @((Get-OptionalPropertyValue -Object $neighborSurfaceSelection -Name "ChangedPaths" -DefaultValue @()))

                $combinedSummaryObject = [ordered]@{
                    generatedAt = (Get-Date).ToString("o")
                    overallResult = $resultLabel
                    requestedProvenanceMode = $ProvenanceMode
                    executionMode = $normalizedExecutionMode
                    executionModeReason = $normalizedExecutionModeReason
                    repoRoot = $resolvedRepoRoot
                    outerWorkspaceRoot = $resolvedOuterWorkspaceRoot
                    buildDir = $resolvedBuildDir
                    executablePath = $resolvedExecutablePath
                    artifactRoot = $resolvedArtifactOutputDir
                    runNeighborSurfaceSuite = $true
                    suites = @(
                        [ordered]@{
                            name = "resumed-denial"
                            result = if ($null -ne $resumedArtifactSummary) { [string]$resumedArtifactSummary.overallResult } else { $resumedDenialResult }
                            observedProvenanceMode = if ($null -ne $resumedArtifactSummary) { [string]$resumedArtifactSummary.observedProvenanceMode } else { Get-ObservedProvenanceMode -ScenarioProvenance $resumedScenarioProvenance }
                            artifactOutputDir = $resumedArtifactOutputDir
                            textSummaryPath = $artifactTextSummaryPath
                            jsonSummaryPath = $artifactJsonSummaryPath
                        }
                        [ordered]@{
                            name = "neighbor-surfaces"
                            result = if ($null -ne $neighborArtifactSummary) { [string]$neighborArtifactSummary.overallResult } else { $neighborSurfaceResult }
                            observedProvenanceMode = if ($null -ne $neighborArtifactSummary) { [string]$neighborArtifactSummary.observedProvenanceMode } else { "" }
                            artifactOutputDir = $neighborArtifactOutputDir
                            textSummaryPath = if (Test-Path -LiteralPath $neighborArtifactTextSummaryPath -PathType Leaf) { $neighborArtifactTextSummaryPath } else { "" }
                            jsonSummaryPath = if (Test-Path -LiteralPath $neighborArtifactJsonSummaryPath -PathType Leaf) { $neighborArtifactJsonSummaryPath } else { "" }
                            junitOutputPath = if ($null -ne $neighborArtifactSummary) { [string](Get-OptionalPropertyValue -Object $neighborArtifactSummary -Name "junitOutputPath" -DefaultValue "") } else { $resolvedNeighborJUnitOutputPath }
                        }
                    )
                    resumedDenial = [ordered]@{
                        overallResult = if ($null -ne $resumedArtifactSummary) { [string]$resumedArtifactSummary.overallResult } else { $resumedDenialResult }
                        observedProvenanceMode = if ($null -ne $resumedArtifactSummary) { [string]$resumedArtifactSummary.observedProvenanceMode } else { Get-ObservedProvenanceMode -ScenarioProvenance $resumedScenarioProvenance }
                        scenarioStatuses = $resumedScenarioStatuses
                        scenarioProvenance = $resumedScenarioProvenance
                        textSummaryPath = $artifactTextSummaryPath
                        jsonSummaryPath = $artifactJsonSummaryPath
                    }
                    neighborSurfaces = [ordered]@{
                        overallResult = if ($null -ne $neighborArtifactSummary) { [string]$neighborArtifactSummary.overallResult } else { $neighborSurfaceResult }
                        observedProvenanceMode = if ($null -ne $neighborArtifactSummary) { [string]$neighborArtifactSummary.observedProvenanceMode } else { "" }
                        requestedFullMatrix = if ($null -ne $neighborArtifactSummary) { [bool](Get-OptionalPropertyValue -Object $neighborArtifactSummary -Name "requestedFullMatrix" -DefaultValue $false) } else { [bool]($FullNeighborMatrix -or $effectiveNeighborSurfaceProfile -eq "full-expanded") }
                        selectionMode = $neighborSelectionMode
                        selectionOrigin = $neighborSelectionOrigin
                        requestedProfileMode = $neighborRequestedProfileMode
                        requestedProfile = $neighborRequestedProfile
                        resolvedProfile = $neighborResolvedProfile
                        profileSelectionReason = $neighborSelectionReason
                        profileSelectionRuleIds = $neighborProfileSelectionRuleIds
                        profileSelectionRules = $neighborProfileSelectionRules
                        profileSelectionDiffBase = $neighborProfileSelectionDiffBase
                        profileSelectionDiffHead = $neighborProfileSelectionDiffHead
                        profileSelectionChangedPathSource = $neighborProfileSelectionChangedPathSource
                        profileSelectionChangedPaths = $neighborProfileSelectionChangedPaths
                        selectedGroupNames = if ($null -ne $neighborArtifactSummary) { @($neighborArtifactSummary.selectedGroupNames) } else { $(if ($effectiveNeighborSurfaceProfile -eq "checkpoint-extended") { @("checkpoint-extended") } else { @($normalizedNeighborSurfaceGroups) }) }
                        selectedSurfaceNames = if ($null -ne $neighborArtifactSummary) { @($neighborArtifactSummary.selectedSurfaceNames) } else { @() }
                        passingSurfaceCount = $neighborPassingSurfaceCount
                        failingSurfaceCount = $neighborFailingSurfaceCount
                        groups = $neighborGroupItems
                        surfaces = $neighborSurfaceItems
                        failedSurfaces = $neighborFailedSurfaceItems
                        junitOutputPath = if ($null -ne $neighborArtifactSummary) { [string](Get-OptionalPropertyValue -Object $neighborArtifactSummary -Name "junitOutputPath" -DefaultValue "") } else { $resolvedNeighborJUnitOutputPath }
                        textSummaryPath = if (Test-Path -LiteralPath $neighborArtifactTextSummaryPath -PathType Leaf) { $neighborArtifactTextSummaryPath } else { "" }
                        jsonSummaryPath = if (Test-Path -LiteralPath $neighborArtifactJsonSummaryPath -PathType Leaf) { $neighborArtifactJsonSummaryPath } else { "" }
                    }
                }

                Write-CombinedSuiteSummaryFiles `
                    -TextSummaryPath $combinedTextSummaryPath `
                    -JsonSummaryPath $combinedJsonSummaryPath `
                    -SummaryObject $combinedSummaryObject
            }
        }
        catch {
            Write-Heading "Artifact Collection Failure"
            Write-Host $_.Exception.Message
            if ($finalExitCode -eq 0) {
                $finalExitCode = 1
                $resultLabel = "FAIL"
            }
        }
    }

    Write-CiSummary `
        -Result $resultLabel `
        -ExitCode $finalExitCode `
        -ProvenanceMode $ProvenanceMode `
        -ExecutionMode $normalizedExecutionMode `
        -ExecutionModeReason $normalizedExecutionModeReason `
        -ResolvedRepoRoot $resolvedRepoRoot `
        -ResolvedWorkspaceRoot $resolvedOuterWorkspaceRoot `
        -WorkspaceSource $workspaceSource `
        -ResolvedBuildDir $resolvedBuildDir `
        -ResolvedExecutablePath $resolvedExecutablePath `
        -VerificationCommandLine $verificationCommandLine `
        -NeighborVerificationCommandLine $neighborVerificationCommandLine `
        -ResolvedArtifactOutputDir $resolvedArtifactOutputDir `
        -ArtifactTextSummaryPath $artifactTextSummaryPath `
        -ArtifactJsonSummaryPath $artifactJsonSummaryPath `
        -NeighborArtifactTextSummaryPath $neighborArtifactTextSummaryPath `
        -NeighborArtifactJsonSummaryPath $neighborArtifactJsonSummaryPath `
        -CombinedTextSummaryPath $combinedTextSummaryPath `
        -CombinedJsonSummaryPath $combinedJsonSummaryPath `
        -ResumedDenialResult $resumedDenialResult `
        -NeighborSurfaceResult $neighborSurfaceResult `
        -NeighborSurfaceProfile $effectiveNeighborSurfaceProfile `
        -NeighborSurfaceSelection $neighborSurfaceSelection `
        -NeighborSurfaceGroups $normalizedNeighborSurfaceGroups `
        -NeighborSurfaces $normalizedNeighborSurfaces `
        -NeighborJUnitOutputPath $resolvedNeighborJUnitOutputPath `
        -FullNeighborMatrixRequested:$FullNeighborMatrix `
        -NeighborSurfaceSuiteEnabled:$RunNeighborSurfaceSuite
}

exit $finalExitCode
