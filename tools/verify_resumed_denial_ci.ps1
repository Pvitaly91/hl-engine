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
    [switch]$CollectArtifacts,
    [string]$ArtifactOutputDir
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
if (Test-Path Variable:PSNativeCommandUseErrorActionPreference) {
    $PSNativeCommandUseErrorActionPreference = $false
}

$script:PromptId = "HL-CL-20260411-163-dedicated-goldsrc-signon-carried-checkpoint-claimed-checkpoint-resumed-denial-surface"
$script:DefaultBuildDirName = "build-main-win32-hlhost-regression"
$script:MainRunnerPath = Join-Path $PSScriptRoot "verify_resumed_denial_main.ps1"
$script:ArtifactCollectorPath = Join-Path $PSScriptRoot "collect_resumed_denial_artifacts.ps1"

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

function Resolve-ArtifactOutputDir {
    param(
        [string]$ResolvedRepoRoot,
        [string]$RequestedArtifactOutputDir
    )

    if ([string]::IsNullOrWhiteSpace($RequestedArtifactOutputDir)) {
        return [System.IO.Path]::GetFullPath((Join-Path $ResolvedRepoRoot "artifacts\resumed-denial"))
    }

    return Resolve-FullPath -PathValue $RequestedArtifactOutputDir -BasePath $ResolvedRepoRoot
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
        [object]$VerificationOutputMetadata
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

    $metadataObject | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $MetadataPath -Encoding UTF8
}

function Write-CiSummary {
    param(
        [string]$Result,
        [int]$ExitCode,
        [string]$ProvenanceMode,
        [string]$ResolvedRepoRoot,
        [string]$ResolvedWorkspaceRoot,
        [string]$WorkspaceSource,
        [string]$ResolvedBuildDir,
        [string]$ResolvedExecutablePath,
        [string]$VerificationCommandLine,
        [string]$ResolvedArtifactOutputDir,
        [string]$ArtifactTextSummaryPath,
        [string]$ArtifactJsonSummaryPath
    )

    Write-Heading "CI Summary"
    Write-Host ("result: {0}" -f $Result)
    Write-Host ("exit code: {0}" -f $ExitCode)
    Write-Host ("provenance mode: {0}" -f $ProvenanceMode)
    Write-Host ("repo root: {0}" -f $ResolvedRepoRoot)
    Write-Host ("outer workspace root: {0}" -f $ResolvedWorkspaceRoot)
    Write-Host ("outer workspace source: {0}" -f $WorkspaceSource)
    Write-Host ("build dir: {0}" -f $ResolvedBuildDir)
    if (-not [string]::IsNullOrWhiteSpace($ResolvedExecutablePath)) {
        Write-Host ("executable: {0}" -f $ResolvedExecutablePath)
    }
    if (-not [string]::IsNullOrWhiteSpace($VerificationCommandLine)) {
        Write-Host ("verification command: {0}" -f $VerificationCommandLine)
    }
    if (-not [string]::IsNullOrWhiteSpace($ResolvedArtifactOutputDir)) {
        Write-Host ("artifact output dir: {0}" -f $ResolvedArtifactOutputDir)
    }
    if (-not [string]::IsNullOrWhiteSpace($ArtifactTextSummaryPath)) {
        Write-Host ("artifact text summary: {0}" -f $ArtifactTextSummaryPath)
    }
    if (-not [string]::IsNullOrWhiteSpace($ArtifactJsonSummaryPath)) {
        Write-Host ("artifact json summary: {0}" -f $ArtifactJsonSummaryPath)
    }
}

$resolvedRepoRoot = ""
$resolvedOuterWorkspaceRoot = ""
$workspaceSource = ""
$resolvedBuildDir = ""
$resolvedExecutablePath = ""
$verificationCommandLine = ""
$resolvedArtifactOutputDir = ""
$artifactTextSummaryPath = ""
$artifactJsonSummaryPath = ""
$verificationOutputLogPath = ""
$artifactMetadataPath = ""
$verificationOutputMetadata = $null
$resultLabel = "FAIL"
$finalExitCode = 1

try {
    $resolvedRepoRoot = Get-RepoRootFromRequest -RequestedRoot $RepoRoot
    if ($CollectArtifacts) {
        Assert-PathExists -LiteralPath $script:ArtifactCollectorPath -Description "artifact collector helper" -ActionHint "Verify that tools\\collect_resumed_denial_artifacts.ps1 exists in this checkout"
        $resolvedArtifactOutputDir = Resolve-ArtifactOutputDir -ResolvedRepoRoot $resolvedRepoRoot -RequestedArtifactOutputDir $ArtifactOutputDir
        $artifactMetadataDirectory = Join-Path $resolvedArtifactOutputDir "metadata"
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
                -VerificationOutputMetadata $verificationOutputMetadata

            Write-Heading "Artifact Collection"
            Write-Host ("Output dir: {0}" -f $resolvedArtifactOutputDir)
            & $script:ArtifactCollectorPath `
                -RepoRoot $resolvedRepoRoot `
                -LogsRoot (Join-Path $resolvedRepoRoot "logs\latest") `
                -OutputDir $resolvedArtifactOutputDir `
                -RunLabelPattern ([string](Get-OptionalPropertyValue -Object $verificationOutputMetadata -Name "RunLabelPattern" -DefaultValue "verify-resumed-denial-main-*")) `
                -IncludeCodexArtifacts `
                -AllowMissingCodexArtifacts `
                -VerificationMetadataPath $artifactMetadataPath

            $artifactCollectorExitCode = if ($null -eq $LASTEXITCODE) { 0 } else { $LASTEXITCODE }
            if ($artifactCollectorExitCode -ne 0) {
                throw ("collect_resumed_denial_artifacts.ps1 failed with exit code {0}." -f $artifactCollectorExitCode)
            }

            $artifactTextSummaryPath = Join-Path $resolvedArtifactOutputDir "resumed_denial_summary.txt"
            $artifactJsonSummaryPath = Join-Path $resolvedArtifactOutputDir "resumed_denial_summary.json"
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
        -ResolvedRepoRoot $resolvedRepoRoot `
        -ResolvedWorkspaceRoot $resolvedOuterWorkspaceRoot `
        -WorkspaceSource $workspaceSource `
        -ResolvedBuildDir $resolvedBuildDir `
        -ResolvedExecutablePath $resolvedExecutablePath `
        -VerificationCommandLine $verificationCommandLine `
        -ResolvedArtifactOutputDir $resolvedArtifactOutputDir `
        -ArtifactTextSummaryPath $artifactTextSummaryPath `
        -ArtifactJsonSummaryPath $artifactJsonSummaryPath
}

exit $finalExitCode
