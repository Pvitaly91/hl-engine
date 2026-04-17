param(
    [string]$RepoRoot,
    [string]$OuterWorkspaceRoot,
    [string]$BuildDir,
    [string]$ExePath,
    [switch]$NoBuild,
    [switch]$NoExecute,
    [switch]$UseExistingBinary,
    [ValidateSet("auto", "runtime", "workspace")]
    [string]$ProvenanceMode = "auto",
    [string]$MatrixPath,
    [string[]]$Surface,
    [string[]]$Group,
    [switch]$ListSurfaces,
    [string]$ArtifactOutputDir,
    [switch]$CollectArtifacts
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
if (Test-Path Variable:PSNativeCommandUseErrorActionPreference) {
    $PSNativeCommandUseErrorActionPreference = $false
}

$script:PromptId = "HL-CL-20260411-163-dedicated-goldsrc-signon-carried-checkpoint-claimed-checkpoint-resumed-denial-surface"
$script:SuiteName = "signon-neighbor-surfaces"
$script:MainRunnerPath = Join-Path $PSScriptRoot "verify_resumed_denial_main.ps1"
$script:DefaultMatrixFileName = "signon_regression_matrix.psd1"

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

function Resolve-MatrixPath {
    param(
        [string]$ResolvedRepoRoot,
        [string]$RequestedMatrixPath
    )

    if ([string]::IsNullOrWhiteSpace($RequestedMatrixPath)) {
        return [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot $script:DefaultMatrixFileName))
    }

    $basePath = if ([string]::IsNullOrWhiteSpace($ResolvedRepoRoot)) { $PSScriptRoot } else { $ResolvedRepoRoot }
    return Resolve-FullPath -PathValue $RequestedMatrixPath -BasePath $basePath
}

function Get-EntryGroupNames {
    param([object[]]$Entries)

    $groupNames = New-Object System.Collections.Generic.List[string]
    $seenGroups = New-Object System.Collections.Generic.HashSet[string]([System.StringComparer]::OrdinalIgnoreCase)
    foreach ($entry in @($Entries)) {
        foreach ($groupName in @(Get-OptionalPropertyValue -Object $entry -Name "Groups" -DefaultValue @())) {
            if ([string]::IsNullOrWhiteSpace($groupName)) {
                continue
            }

            if ($seenGroups.Add($groupName)) {
                $groupNames.Add($groupName)
            }
        }
    }

    return $groupNames.ToArray()
}

function Import-SignonRegressionMatrix {
    param([string]$ResolvedMatrixPath)

    if (-not (Test-Path -LiteralPath $ResolvedMatrixPath -PathType Leaf)) {
        throw ("Signon regression matrix file does not exist: {0}" -f $ResolvedMatrixPath)
    }

    $matrixData = Import-PowerShellDataFile -Path $ResolvedMatrixPath
    $entries = @(Get-OptionalPropertyValue -Object $matrixData -Name "Entries" -DefaultValue @())
    if ($entries.Count -eq 0) {
        throw ("Signon regression matrix {0} does not define any Entries." -f $ResolvedMatrixPath)
    }

    $seenNames = New-Object System.Collections.Generic.HashSet[string]([System.StringComparer]::OrdinalIgnoreCase)
    $normalizedEntries = New-Object System.Collections.Generic.List[object]
    foreach ($entry in $entries) {
        $name = [string](Get-OptionalPropertyValue -Object $entry -Name "Name" -DefaultValue "")
        if ([string]::IsNullOrWhiteSpace($name)) {
            throw ("Signon regression matrix {0} contains an entry with a blank Name." -f $ResolvedMatrixPath)
        }
        if (-not $seenNames.Add($name)) {
            throw ("Signon regression matrix {0} contains a duplicate entry name: {1}" -f $ResolvedMatrixPath, $name)
        }

        $groups = @(Get-TrimmedUniqueValues -Values @(Get-OptionalPropertyValue -Object $entry -Name "Groups" -DefaultValue @()))
        if ($groups.Count -eq 0) {
            throw ("Signon regression matrix entry {0} must declare at least one group." -f $name)
        }

        $surfaceNeedle = [string](Get-OptionalPropertyValue -Object $entry -Name "SurfaceNeedle" -DefaultValue "")
        $probeNeedle = [string](Get-OptionalPropertyValue -Object $entry -Name "ProbeNeedle" -DefaultValue "")
        if ([string]::IsNullOrWhiteSpace($surfaceNeedle) -or [string]::IsNullOrWhiteSpace($probeNeedle)) {
            throw ("Signon regression matrix entry {0} must define both SurfaceNeedle and ProbeNeedle." -f $name)
        }

        $runs = Get-OptionalPropertyValue -Object $entry -Name "Runs" -DefaultValue $null
        if ($null -eq $runs) {
            throw ("Signon regression matrix entry {0} is missing Runs metadata." -f $name)
        }

        foreach ($runName in @("happy", "gate")) {
            $runExpectation = Get-OptionalPropertyValue -Object $runs -Name $runName -DefaultValue $null
            if ($null -eq $runExpectation) {
                throw ("Signon regression matrix entry {0} is missing the {1} run definition." -f $name, $runName)
            }

            foreach ($tokenProperty in @("Surface", "Probe", "Key")) {
                $tokenValues = @(Get-OptionalPropertyValue -Object $runExpectation -Name $tokenProperty -DefaultValue @())
                if ($tokenValues.Count -eq 0) {
                    throw ("Signon regression matrix entry {0} run {1} is missing {2} tokens." -f $name, $runName, $tokenProperty)
                }
            }
        }

        $normalizedEntries.Add([ordered]@{
                Name = $name
                Groups = $groups
                EnabledByDefault = [bool](Get-OptionalPropertyValue -Object $entry -Name "EnabledByDefault" -DefaultValue $false)
                Mode = [string](Get-OptionalPropertyValue -Object $entry -Name "Mode" -DefaultValue "")
                Notes = [string](Get-OptionalPropertyValue -Object $entry -Name "Notes" -DefaultValue "")
                SurfaceNeedle = $surfaceNeedle
                ProbeNeedle = $probeNeedle
                Runs = $runs
            })
    }

    return [ordered]@{
        SuiteName = [string](Get-OptionalPropertyValue -Object $matrixData -Name "SuiteName" -DefaultValue $script:SuiteName)
        MatrixPath = $ResolvedMatrixPath
        Entries = $normalizedEntries.ToArray()
        KnownGroups = @(Get-EntryGroupNames -Entries $normalizedEntries.ToArray())
    }
}

function Select-SignonRegressionEntries {
    param(
        [object[]]$Entries,
        [string[]]$RequestedSurfaceNames,
        [string[]]$RequestedGroupNames
    )

    $normalizedSurfaceNames = @(Get-TrimmedUniqueValues -Values $RequestedSurfaceNames)
    $normalizedGroupNames = @(Get-TrimmedUniqueValues -Values $RequestedGroupNames)
    $knownSurfaceNames = @($Entries | ForEach-Object { [string](Get-OptionalPropertyValue -Object $_ -Name "Name" -DefaultValue "") })
    $knownGroupNames = @(Get-EntryGroupNames -Entries $Entries)

    foreach ($surfaceName in $normalizedSurfaceNames) {
        if ($knownSurfaceNames -notcontains $surfaceName) {
            throw ("Unknown -Surface value '{0}'. Known surfaces: {1}" -f $surfaceName, ($knownSurfaceNames -join ", "))
        }
    }

    foreach ($groupName in $normalizedGroupNames) {
        if ($knownGroupNames -notcontains $groupName) {
            throw ("Unknown -Group value '{0}'. Known groups: {1}" -f $groupName, ($knownGroupNames -join ", "))
        }
    }

    $selectedEntries = New-Object System.Collections.Generic.List[object]
    foreach ($entry in @($Entries)) {
        $entryName = [string](Get-OptionalPropertyValue -Object $entry -Name "Name" -DefaultValue "")
        $entryGroups = @(Get-OptionalPropertyValue -Object $entry -Name "Groups" -DefaultValue @())
        $matchesSurface = ($normalizedSurfaceNames.Count -eq 0 -or $normalizedSurfaceNames -contains $entryName)
        $matchesGroup = ($normalizedGroupNames.Count -eq 0 -or @($entryGroups | Where-Object { $normalizedGroupNames -contains $_ }).Count -gt 0)
        $enabledByDefault = [bool](Get-OptionalPropertyValue -Object $entry -Name "EnabledByDefault" -DefaultValue $false)

        if ($normalizedSurfaceNames.Count -eq 0 -and $normalizedGroupNames.Count -eq 0) {
            if ($enabledByDefault) {
                $selectedEntries.Add($entry)
            }
            continue
        }

        if ($matchesSurface -and $matchesGroup) {
            $selectedEntries.Add($entry)
        }
    }

    if ($selectedEntries.Count -eq 0) {
        if ($normalizedSurfaceNames.Count -eq 0 -and $normalizedGroupNames.Count -eq 0) {
            throw "The signon regression matrix does not contain any entries with EnabledByDefault = true."
        }

        throw "The requested -Surface/-Group filter combination did not select any signon regression entries."
    }

    return [pscustomobject]@{
        Entries = $selectedEntries.ToArray()
        RequestedSurfaceNames = $normalizedSurfaceNames
        RequestedGroupNames = $normalizedGroupNames
        SelectedSurfaceNames = @($selectedEntries | ForEach-Object { [string](Get-OptionalPropertyValue -Object $_ -Name "Name" -DefaultValue "") })
        SelectedGroupNames = @(Get-EntryGroupNames -Entries $selectedEntries.ToArray())
    }
}

function Write-SignonRegressionMatrixListing {
    param([object]$MatrixData)

    Write-Heading "Signon Regression Matrix"
    Write-Host ("Matrix path: {0}" -f $MatrixData.MatrixPath)
    Write-Host ("Suite name: {0}" -f $MatrixData.SuiteName)
    Write-Host ("Entries: {0}" -f @($MatrixData.Entries).Count)

    Write-Heading "Groups"
    foreach ($groupName in @($MatrixData.KnownGroups)) {
        $matchingNames = @(
            $MatrixData.Entries |
                Where-Object { @(Get-OptionalPropertyValue -Object $_ -Name "Groups" -DefaultValue @()) -contains $groupName } |
                ForEach-Object { [string](Get-OptionalPropertyValue -Object $_ -Name "Name" -DefaultValue "") }
        )
        Write-Host ("{0}: {1}" -f $groupName, ($matchingNames -join ", "))
    }

    Write-Heading "Surfaces"
    foreach ($entry in @($MatrixData.Entries)) {
        $entryName = [string](Get-OptionalPropertyValue -Object $entry -Name "Name" -DefaultValue "")
        $enabledByDefault = [bool](Get-OptionalPropertyValue -Object $entry -Name "EnabledByDefault" -DefaultValue $false)
        $groupNames = @(Get-OptionalPropertyValue -Object $entry -Name "Groups" -DefaultValue @())
        $mode = [string](Get-OptionalPropertyValue -Object $entry -Name "Mode" -DefaultValue "")
        $notes = [string](Get-OptionalPropertyValue -Object $entry -Name "Notes" -DefaultValue "")
        Write-Host ("{0}" -f $entryName)
        Write-Host ("  enabledByDefault: {0}" -f $(if ($enabledByDefault) { "yes" } else { "no" }))
        Write-Host ("  groups: {0}" -f ($groupNames -join ", "))
        if (-not [string]::IsNullOrWhiteSpace($mode)) {
            Write-Host ("  mode: {0}" -f $mode)
        }
        if (-not [string]::IsNullOrWhiteSpace($notes)) {
            Write-Host ("  notes: {0}" -f $notes)
        }
    }
}

function Resolve-ArtifactOutputDir {
    param(
        [string]$ResolvedRepoRoot,
        [string]$RequestedArtifactOutputDir
    )

    if ([string]::IsNullOrWhiteSpace($RequestedArtifactOutputDir)) {
        return [System.IO.Path]::GetFullPath((Join-Path $ResolvedRepoRoot "artifacts\signon-neighbor-surfaces"))
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

function Get-DelegatedRunnerContext {
    param([string]$OutputLogPath)

    $context = [ordered]@{
        RepoRoot = ""
        WorkspaceRoot = ""
        Branch = ""
        HeadCommit = ""
        RequestedProvenanceMode = ""
        BuildDir = ""
        ExecutablePath = ""
    }

    if ([string]::IsNullOrWhiteSpace($OutputLogPath) -or -not (Test-Path -LiteralPath $OutputLogPath -PathType Leaf)) {
        return [pscustomobject]$context
    }

    foreach ($line in @(Get-Content -LiteralPath $OutputLogPath)) {
        $trimmedLine = $line.Trim()
        if ($trimmedLine -match '^Repo root:\s+(.+)$') {
            $context.RepoRoot = $matches[1]
            continue
        }
        if ($trimmedLine -match '^Workspace root:\s+(.+)$') {
            $context.WorkspaceRoot = $matches[1]
            continue
        }
        if ($trimmedLine -match '^Branch:\s+(.+)$') {
            $context.Branch = $matches[1]
            continue
        }
        if ($trimmedLine -match '^HEAD:\s+(.+)$') {
            $context.HeadCommit = $matches[1]
            continue
        }
        if ($trimmedLine -match '^Requested provenance mode:\s+(.+)$') {
            $context.RequestedProvenanceMode = $matches[1]
            continue
        }
        if ($trimmedLine -match '^Build dir:\s+(.+)$') {
            $context.BuildDir = $matches[1]
            continue
        }
        if ($trimmedLine -match '^Executable:\s+(.+)$') {
            $context.ExecutablePath = $matches[1]
            continue
        }
    }

    return [pscustomobject]$context
}

function Get-VerificationOutputMetadata {
    param(
        [string]$OutputLogPath,
        [switch]$NoExecuteRequested
    )

    $scenarioStatuses = [ordered]@{
        happy = "UNKNOWN"
        gate = "UNKNOWN"
    }
    $runLabels = [ordered]@{}
    $scenarioProvenance = [ordered]@{
        happy = ""
        gate = ""
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

        if ($trimmedLine -match '^(happy|gate) command:$') {
            $currentScenario = $matches[1]
            continue
        }

        if ($line -match '--run-label\s+([^\s"]+)') {
            $runLabel = $matches[1]
            $runLabelInfo = Get-RunLabelInfo -RunLabel $runLabel
            if ($null -ne $runLabelInfo -and ($runLabelInfo.Scenario -eq "happy" -or $runLabelInfo.Scenario -eq "gate")) {
                $runLabels[$runLabelInfo.Scenario] = $runLabel
            }
        }

        if ($trimmedLine -match '^(happy|gate):\s+(PASS|FAIL|SKIPPED|PENDING)$') {
            $scenarioStatuses[$matches[1]] = $matches[2].Trim().ToUpperInvariant()
            continue
        }

        if ($trimmedLine -match '^provenance:\s+([A-Za-z0-9_,.-]+)$' -and -not [string]::IsNullOrWhiteSpace($currentScenario)) {
            $scenarioProvenance[$currentScenario] = $matches[1]
            continue
        }

        if ($trimmedLine -match '^Final overall verdict:\s+([A-Za-z]+)$') {
            $overallResult = $matches[1].Trim().ToUpperInvariant()
        }
    }

    return [pscustomobject]@{
        ScenarioStatuses = $scenarioStatuses
        RunLabels = $runLabels
        ScenarioProvenance = $scenarioProvenance
        OverallResult = $overallResult
    }
}

function Get-NewestMatchingFile {
    param(
        [string]$DirectoryPath,
        [string]$Filter,
        [string]$Description
    )

    $match = Get-ChildItem -LiteralPath $DirectoryPath -Filter $Filter -File -ErrorAction SilentlyContinue |
        Sort-Object LastWriteTime -Descending |
        Select-Object -First 1

    if ($null -eq $match) {
        throw ("Missing {0} under {1} for filter {2}." -f $Description, $DirectoryPath, $Filter)
    }

    return $match
}

function Get-RunArtifacts {
    param(
        [string]$RuntimeRoot,
        [string]$CodexRoot,
        [string]$RunLabel
    )

    $summaryFile = Get-NewestMatchingFile -DirectoryPath $RuntimeRoot -Filter ("*__{0}_summary.log" -f $RunLabel) -Description ("summary log for {0}" -f $RunLabel)
    $runtimeFiles = @(
        Get-ChildItem -LiteralPath $RuntimeRoot -Filter ("*__{0}_part*.log" -f $RunLabel) -File -ErrorAction SilentlyContinue |
            Sort-Object LastWriteTime
    )
    if ($runtimeFiles.Count -eq 0) {
        throw ("No runtime part logs were produced for run label {0}." -f $RunLabel)
    }

    $codexManifest = $null
    if (-not [string]::IsNullOrWhiteSpace($CodexRoot) -and (Test-Path -LiteralPath $CodexRoot -PathType Container)) {
        $codexManifest = Get-ChildItem -LiteralPath $CodexRoot -Filter ("*__{0}_manifest.json" -f $RunLabel) -File -ErrorAction SilentlyContinue |
            Sort-Object LastWriteTime -Descending |
            Select-Object -First 1
    }

    return [pscustomobject]@{
        SummaryFile = $summaryFile
        RuntimeFiles = $runtimeFiles
        CodexManifest = $codexManifest
    }
}

function Find-FirstMatchingLine {
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

function Get-TokenMatchReport {
    param(
        [string]$Line,
        [string[]]$ExpectedTokens
    )

    $matchedTokens = New-Object System.Collections.Generic.List[string]
    $missingTokens = New-Object System.Collections.Generic.List[string]

    foreach ($token in $ExpectedTokens) {
        if (-not [string]::IsNullOrWhiteSpace($Line) -and $Line.IndexOf($token, [System.StringComparison]::Ordinal) -ge 0) {
            $matchedTokens.Add($token)
        }
        else {
            $missingTokens.Add($token)
        }
    }

    return [pscustomobject]@{
        MatchedTokens = $matchedTokens.ToArray()
        MissingTokens = $missingTokens.ToArray()
    }
}

function Test-SurfaceCheckForRun {
    param(
        [object]$Definition,
        [string]$RunName,
        [string[]]$SummaryLines
    )

    $expected = $Definition.Runs[$RunName]
    if ($null -eq $expected) {
        throw ("Surface definition {0} does not define expectations for run {1}." -f $Definition.Name, $RunName)
    }

    $surfaceLine = Find-FirstMatchingLine -Lines $SummaryLines -Needle $Definition.SurfaceNeedle
    $probeLine = Find-FirstMatchingLine -Lines $SummaryLines -Needle $Definition.ProbeNeedle
    $surfaceTokens = @("mode=dedicated", "bind=loopback", "requestedPort=0") + @($expected.Surface)
    $probeTokens = @("mode=dedicated", "probe=loopback") + @($expected.Probe)
    $surfaceReport = Get-TokenMatchReport -Line $surfaceLine -ExpectedTokens $surfaceTokens
    $probeReport = Get-TokenMatchReport -Line $probeLine -ExpectedTokens $probeTokens
    $status = if ($surfaceReport.MissingTokens.Count -eq 0 -and $probeReport.MissingTokens.Count -eq 0) { "PASS" } else { "FAIL" }

    if ($status -eq "PASS") {
        $reason = ("matched key tokens: {0}" -f ($expected.Key -join ", "))
    }
    else {
        $problemSegments = New-Object System.Collections.Generic.List[string]
        if ([string]::IsNullOrWhiteSpace($surfaceLine)) {
            $problemSegments.Add("missing surface summary line")
        }
        if ([string]::IsNullOrWhiteSpace($probeLine)) {
            $problemSegments.Add("missing probe summary line")
        }
        if ($surfaceReport.MissingTokens.Count -gt 0) {
            $problemSegments.Add(("surface missing: {0}" -f ($surfaceReport.MissingTokens -join ", ")))
        }
        if ($probeReport.MissingTokens.Count -gt 0) {
            $problemSegments.Add(("probe missing: {0}" -f ($probeReport.MissingTokens -join ", ")))
        }
        $reason = $problemSegments -join "; "
    }

    return [pscustomobject]@{
        runName = $RunName
        status = $status
        reason = $reason
        surfaceLine = $surfaceLine
        probeLine = $probeLine
        matchedSurfaceTokens = $surfaceReport.MatchedTokens
        missingSurfaceTokens = $surfaceReport.MissingTokens
        matchedProbeTokens = $probeReport.MatchedTokens
        missingProbeTokens = $probeReport.MissingTokens
        keyTokens = @($expected.Key)
    }
}

function Get-ObservedProvenanceMode {
    param([object]$VerificationMetadata)

    $provenanceValues = New-Object System.Collections.Generic.List[string]
    foreach ($scenarioName in @("happy", "gate")) {
        $value = [string](Get-OptionalPropertyValue -Object $VerificationMetadata.ScenarioProvenance -Name $scenarioName -DefaultValue "")
        if (-not [string]::IsNullOrWhiteSpace($value)) {
            $provenanceValues.Add($value)
        }
    }

    $uniqueValues = @($provenanceValues | Sort-Object -Unique)
    if ($uniqueValues.Count -eq 0) {
        return ""
    }
    if ($uniqueValues.Count -eq 1) {
        return $uniqueValues[0]
    }

    return "mixed"
}

function Get-GroupedOverallStatus {
    param([string[]]$Statuses)

    if (@($Statuses).Count -eq 0) {
        return "NOT_SELECTED"
    }
    if (@($Statuses | Where-Object { $_ -eq "FAIL" }).Count -gt 0) {
        return "FAIL"
    }
    if (@($Statuses | Where-Object { $_ -eq "PASS" }).Count -eq @($Statuses).Count) {
        return "PASS"
    }
    if (@($Statuses | Where-Object { $_ -eq "NOT_RUN" }).Count -eq @($Statuses).Count) {
        return "NOT_RUN"
    }
    if (@($Statuses | Where-Object { $_ -eq "SKIPPED" }).Count -eq @($Statuses).Count) {
        return "SKIPPED"
    }

    return "MIXED"
}

function Build-GroupResults {
    param(
        [object[]]$SelectedEntries,
        [object[]]$SurfaceResults
    )

    $surfaceResultsByName = @{}
    foreach ($surfaceResult in @($SurfaceResults)) {
        $surfaceResultsByName[[string]$surfaceResult.name] = $surfaceResult
    }

    $groupResults = New-Object System.Collections.Generic.List[object]
    foreach ($groupName in @(Get-EntryGroupNames -Entries $SelectedEntries)) {
        $groupSurfaceNames = @(
            $SelectedEntries |
                Where-Object { @(Get-OptionalPropertyValue -Object $_ -Name "Groups" -DefaultValue @()) -contains $groupName } |
                ForEach-Object { [string](Get-OptionalPropertyValue -Object $_ -Name "Name" -DefaultValue "") }
        )
        $groupSurfaceResults = @($groupSurfaceNames | ForEach-Object { $surfaceResultsByName[$_] } | Where-Object { $null -ne $_ })
        $groupStatuses = @($groupSurfaceResults | ForEach-Object { [string]$_.overallStatus })
        $groupResults.Add([ordered]@{
                name = $groupName
                overallStatus = Get-GroupedOverallStatus -Statuses $groupStatuses
                surfaceCount = $groupSurfaceNames.Count
                passingSurfaceCount = @($groupSurfaceResults | Where-Object { $_.overallStatus -eq "PASS" }).Count
                failingSurfaceCount = @($groupSurfaceResults | Where-Object { $_.overallStatus -eq "FAIL" }).Count
                notRunSurfaceCount = @($groupSurfaceResults | Where-Object { $_.overallStatus -eq "NOT_RUN" }).Count
                surfaceNames = $groupSurfaceNames
            }) | Out-Null
    }

    return $groupResults.ToArray()
}

function Copy-RunArtifactsToBundle {
    param(
        [object]$RunRecord,
        [string]$OutputDir
    )

    if ([string]::IsNullOrWhiteSpace($RunRecord.sourceSummaryPath)) {
        return
    }

    $runtimeScenarioDirectory = Ensure-Directory (Join-Path $OutputDir ("runtime\{0}" -f $RunRecord.name))
    $summaryArtifactPath = Join-Path $runtimeScenarioDirectory "summary.log"
    Copy-Item -LiteralPath $RunRecord.sourceSummaryPath -Destination $summaryArtifactPath -Force
    $RunRecord.artifactSummaryPath = $summaryArtifactPath

    $runtimeArtifactPaths = New-Object System.Collections.Generic.List[string]
    foreach ($runtimePath in @($RunRecord.sourceRuntimePaths)) {
        $destinationPath = Join-Path $runtimeScenarioDirectory (Split-Path -Path $runtimePath -Leaf)
        Copy-Item -LiteralPath $runtimePath -Destination $destinationPath -Force
        $runtimeArtifactPaths.Add($destinationPath)
    }
    $RunRecord.artifactRuntimePaths = $runtimeArtifactPaths.ToArray()

    if (-not [string]::IsNullOrWhiteSpace($RunRecord.sourceCodexManifestPath)) {
        $codexScenarioDirectory = Ensure-Directory (Join-Path $OutputDir ("codex\{0}" -f $RunRecord.name))
        $codexArtifactPath = Join-Path $codexScenarioDirectory "manifest.json"
        Copy-Item -LiteralPath $RunRecord.sourceCodexManifestPath -Destination $codexArtifactPath -Force
        $RunRecord.artifactCodexManifestPath = $codexArtifactPath
    }
}

function Write-SuiteSummaryFiles {
    param(
        [string]$OutputDir,
        [object]$SummaryObject,
        [string]$DelegatedOutputLogPath,
        [object]$VerificationMetadata
    )

    $resolvedOutputDir = Ensure-Directory $OutputDir
    $metadataDirectory = Ensure-Directory (Join-Path $resolvedOutputDir "metadata")
    $delegatedOutputArtifactPath = Join-Path $metadataDirectory "verify_signon_neighbor_surfaces_output.log"
    if (-not [string]::IsNullOrWhiteSpace($DelegatedOutputLogPath) -and (Test-Path -LiteralPath $DelegatedOutputLogPath -PathType Leaf)) {
        $resolvedDelegatedOutputLogPath = [System.IO.Path]::GetFullPath($DelegatedOutputLogPath)
        $resolvedDelegatedOutputArtifactPath = [System.IO.Path]::GetFullPath($delegatedOutputArtifactPath)
        if (-not $resolvedDelegatedOutputLogPath.Equals($resolvedDelegatedOutputArtifactPath, [System.StringComparison]::OrdinalIgnoreCase)) {
            Copy-Item -LiteralPath $resolvedDelegatedOutputLogPath -Destination $resolvedDelegatedOutputArtifactPath -Force
        }
    }

    $delegatedMetadataArtifactPath = Join-Path $metadataDirectory "delegated_verify_resumed_denial_metadata.json"
    if ($null -ne $VerificationMetadata) {
        $VerificationMetadata | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $delegatedMetadataArtifactPath -Encoding UTF8
    }

    foreach ($runRecord in @($SummaryObject.runs)) {
        Copy-RunArtifactsToBundle -RunRecord $runRecord -OutputDir $resolvedOutputDir
    }

    $SummaryObject.metadata = [ordered]@{
        delegatedOutputLog = if (Test-Path -LiteralPath $delegatedOutputArtifactPath -PathType Leaf) { $delegatedOutputArtifactPath } else { "" }
        delegatedVerificationMetadata = if (Test-Path -LiteralPath $delegatedMetadataArtifactPath -PathType Leaf) { $delegatedMetadataArtifactPath } else { "" }
    }

    $textLines = New-Object System.Collections.Generic.List[string]
    $textLines.Add(("suite: {0}" -f $SummaryObject.suiteName))
    $textLines.Add(("overall: {0}" -f $SummaryObject.overallResult))
    $textLines.Add(("requested provenance: {0}" -f $SummaryObject.requestedProvenanceMode))
    $textLines.Add(("observed provenance: {0}" -f $SummaryObject.observedProvenanceMode))
    $textLines.Add(("repo root: {0}" -f $SummaryObject.repoRoot))
    $textLines.Add(("outer workspace root: {0}" -f $SummaryObject.outerWorkspaceRoot))
    $textLines.Add(("build dir: {0}" -f $SummaryObject.buildDir))
    $textLines.Add(("executable: {0}" -f $SummaryObject.executablePath))
    $textLines.Add(("matrix path: {0}" -f $SummaryObject.matrixPath))
    $textLines.Add(("delegated verification command: {0}" -f $SummaryObject.delegatedVerificationCommandLine))
    if (@($SummaryObject.requestedGroupNames).Count -gt 0) {
        $textLines.Add(("requested groups: {0}" -f (@($SummaryObject.requestedGroupNames) -join ", ")))
    }
    if (@($SummaryObject.requestedSurfaceNames).Count -gt 0) {
        $textLines.Add(("requested surfaces: {0}" -f (@($SummaryObject.requestedSurfaceNames) -join ", ")))
    }
    $textLines.Add(("selected groups: {0}" -f (@($SummaryObject.selectedGroupNames) -join ", ")))
    $textLines.Add(("selected surfaces: {0}" -f (@($SummaryObject.selectedSurfaceNames) -join ", ")))
    $textLines.Add("")
    $textLines.Add("run results:")
    foreach ($runRecord in @($SummaryObject.runs)) {
        $textLines.Add(("  {0}: {1}" -f $runRecord.name, $runRecord.status))
        if (-not [string]::IsNullOrWhiteSpace($runRecord.provenanceMode)) {
            $textLines.Add(("    provenance: {0}" -f $runRecord.provenanceMode))
        }
        if (-not [string]::IsNullOrWhiteSpace($runRecord.artifactSummaryPath)) {
            $textLines.Add(("    summary log: {0}" -f $runRecord.artifactSummaryPath))
        }
    }
    $textLines.Add("")
    $textLines.Add("group results:")
    foreach ($groupResult in @($SummaryObject.groups)) {
        $textLines.Add(("  {0}: {1}" -f $groupResult.name, $groupResult.overallStatus))
        $textLines.Add(("    surfaces: {0}" -f ($groupResult.surfaceNames -join ", ")))
        $textLines.Add(("    counts: pass={0}, fail={1}, not-run={2}" -f $groupResult.passingSurfaceCount, $groupResult.failingSurfaceCount, $groupResult.notRunSurfaceCount))
    }
    $textLines.Add("")
    $textLines.Add("surface results:")
    foreach ($surfaceResult in @($SummaryObject.surfaces)) {
        $textLines.Add(("  {0}: {1}" -f $surfaceResult.name, $surfaceResult.overallStatus))
        if (@($surfaceResult.groups).Count -gt 0) {
            $textLines.Add(("    groups: {0}" -f (@($surfaceResult.groups) -join ", ")))
        }
        foreach ($runCheck in @($surfaceResult.runs)) {
            $textLines.Add(("    {0}: {1} - {2}" -f $runCheck.runName, $runCheck.status, $runCheck.reason))
        }
    }

    $textSummaryPath = Join-Path $resolvedOutputDir "signon_neighbor_surface_summary.txt"
    $jsonSummaryPath = Join-Path $resolvedOutputDir "signon_neighbor_surface_summary.json"
    $textLines | Set-Content -LiteralPath $textSummaryPath -Encoding UTF8
    $SummaryObject | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $jsonSummaryPath -Encoding UTF8

    return [pscustomobject]@{
        TextSummaryPath = $textSummaryPath
        JsonSummaryPath = $jsonSummaryPath
    }
}

$resolvedRepoRoot = ""
$resolvedMatrixPath = ""
$resolvedArtifactOutputDir = ""
$matrixData = $null
$selectedMatrix = $null
$delegatedOutputLogPath = ""
$delegatedCommandLine = ""
$delegatedRunnerContext = [pscustomobject]@{}
$verificationMetadata = $null
$runRecords = @()
$groupResults = @()
$surfaceResults = @()
$resultLabel = "FAIL"
$finalExitCode = 1
$cleanupOutputLogPath = ""

try {
    $resolvedRepoRoot = Get-RepoRootFromRequest -RequestedRoot $RepoRoot
    $resolvedMatrixPath = Resolve-MatrixPath -ResolvedRepoRoot $resolvedRepoRoot -RequestedMatrixPath $MatrixPath
    $matrixData = Import-SignonRegressionMatrix -ResolvedMatrixPath $resolvedMatrixPath

    if ($ListSurfaces) {
        Write-SignonRegressionMatrixListing -MatrixData $matrixData
        $resultLabel = "LISTED"
        $finalExitCode = 0
    }
    else {
        $selectedMatrix = Select-SignonRegressionEntries -Entries $matrixData.Entries -RequestedSurfaceNames $Surface -RequestedGroupNames $Group
        $selectedEntries = @($selectedMatrix.Entries)

        Write-Heading "Selected Matrix Entries"
        Write-Host ("Matrix path: {0}" -f $resolvedMatrixPath)
        if (@($selectedMatrix.RequestedGroupNames).Count -gt 0) {
            Write-Host ("Requested groups: {0}" -f (@($selectedMatrix.RequestedGroupNames) -join ", "))
        }
        if (@($selectedMatrix.RequestedSurfaceNames).Count -gt 0) {
            Write-Host ("Requested surfaces: {0}" -f (@($selectedMatrix.RequestedSurfaceNames) -join ", "))
        }
        Write-Host ("Selected groups: {0}" -f (@($selectedMatrix.SelectedGroupNames) -join ", "))
        Write-Host ("Selected surfaces ({0}): {1}" -f $selectedMatrix.SelectedSurfaceNames.Count, (@($selectedMatrix.SelectedSurfaceNames) -join ", "))

        if ($CollectArtifacts -or -not [string]::IsNullOrWhiteSpace($ArtifactOutputDir)) {
            $resolvedArtifactOutputDir = Resolve-ArtifactOutputDir -ResolvedRepoRoot $resolvedRepoRoot -RequestedArtifactOutputDir $ArtifactOutputDir
            $metadataDirectory = Ensure-Directory (Join-Path $resolvedArtifactOutputDir "metadata")
            $delegatedOutputLogPath = Join-Path $metadataDirectory "verify_signon_neighbor_surfaces_output.log"
        }
        else {
            $cleanupOutputLogPath = [System.IO.Path]::GetTempFileName()
            $delegatedOutputLogPath = $cleanupOutputLogPath
        }

        if (-not (Test-Path -LiteralPath $script:MainRunnerPath -PathType Leaf)) {
            throw ("Current-main resumed-denial runner is missing: {0}" -f $script:MainRunnerPath)
        }

        $hostExecutable = Get-CurrentHostExecutable
        $delegatedArguments = New-Object System.Collections.Generic.List[string]
        $delegatedArguments.Add("-NoLogo")
        $delegatedArguments.Add("-NoProfile")
        $delegatedArguments.Add("-ExecutionPolicy")
        $delegatedArguments.Add("Bypass")
        $delegatedArguments.Add("-File")
        $delegatedArguments.Add($script:MainRunnerPath)
        $delegatedArguments.Add("-RepoRoot")
        $delegatedArguments.Add($resolvedRepoRoot)
        if (-not [string]::IsNullOrWhiteSpace($OuterWorkspaceRoot)) {
            $delegatedArguments.Add("-OuterWorkspaceRoot")
            $delegatedArguments.Add($OuterWorkspaceRoot)
        }
        if (-not [string]::IsNullOrWhiteSpace($BuildDir)) {
            $delegatedArguments.Add("-BuildDir")
            $delegatedArguments.Add($BuildDir)
        }
        if (-not [string]::IsNullOrWhiteSpace($ExePath)) {
            $delegatedArguments.Add("-ExePath")
            $delegatedArguments.Add($ExePath)
        }
        if ($NoBuild) {
            $delegatedArguments.Add("-NoBuild")
        }
        if ($NoExecute) {
            $delegatedArguments.Add("-NoExecute")
        }
        if ($UseExistingBinary) {
            $delegatedArguments.Add("-UseExistingBinary")
        }
        $delegatedArguments.Add("-ProvenanceMode")
        $delegatedArguments.Add($ProvenanceMode)
        $delegatedArguments.Add("-SkipRecovery")

        $delegatedCommandLine = Format-CommandLine -ExecutablePath $hostExecutable -Arguments $delegatedArguments.ToArray()
        Write-Heading "Delegated Verification"
        Write-Host "Reusing tools\\verify_resumed_denial_main.ps1 for the current-main happy/gate execution recipe."
        Write-Host ("  {0}" -f $delegatedCommandLine)

        & $hostExecutable @($delegatedArguments.ToArray()) 2>&1 | Tee-Object -FilePath $delegatedOutputLogPath
        $delegatedExitCode = if ($null -eq $LASTEXITCODE) { 0 } else { $LASTEXITCODE }
        $verificationMetadata = Get-VerificationOutputMetadata -OutputLogPath $delegatedOutputLogPath -NoExecuteRequested:$NoExecute
        $delegatedRunnerContext = Get-DelegatedRunnerContext -OutputLogPath $delegatedOutputLogPath

        if ($delegatedExitCode -ne 0) {
            throw ("verify_resumed_denial_main.ps1 failed with exit code {0}." -f $delegatedExitCode)
        }

        Write-Heading "Delegated Run Status"
        Write-Host ("happy: {0}" -f $verificationMetadata.ScenarioStatuses.happy)
        Write-Host ("gate: {0}" -f $verificationMetadata.ScenarioStatuses.gate)

        if ($NoExecute -or $verificationMetadata.OverallResult -eq "NOEXECUTE") {
            foreach ($definition in $selectedEntries) {
                $surfaceResults += [pscustomobject]@{
                    name = $definition.Name
                    groups = @($definition.Groups)
                    overallStatus = "NOT_RUN"
                    runs = @(
                        [pscustomobject]@{ runName = "happy"; status = "NOT_RUN"; reason = "NoExecute requested; delegated runner validated the recipe but did not launch happy." }
                        [pscustomobject]@{ runName = "gate"; status = "NOT_RUN"; reason = "NoExecute requested; delegated runner validated the recipe but did not launch gate." }
                    )
                }
            }

            $resultLabel = "NOEXECUTE"
            $finalExitCode = 0
        }
        else {
            foreach ($scenarioName in @("happy", "gate")) {
                $scenarioStatus = [string](Get-OptionalPropertyValue -Object $verificationMetadata.ScenarioStatuses -Name $scenarioName -DefaultValue "UNKNOWN")
                if ($scenarioStatus -ne "PASS") {
                    throw ("Delegated resumed-denial recipe did not leave {0} in PASS state; found {1}." -f $scenarioName, $scenarioStatus)
                }
            }

            $runtimeRoot = Join-Path $resolvedRepoRoot "logs\\latest\\runtime"
            $codexRoot = Join-Path $resolvedRepoRoot "logs\\latest\\codex"
            foreach ($scenarioName in @("happy", "gate")) {
                $runLabel = [string](Get-OptionalPropertyValue -Object $verificationMetadata.RunLabels -Name $scenarioName -DefaultValue "")
                if ([string]::IsNullOrWhiteSpace($runLabel)) {
                    throw ("Delegated runner did not emit a run label for {0}." -f $scenarioName)
                }

                $runArtifacts = Get-RunArtifacts -RuntimeRoot $runtimeRoot -CodexRoot $codexRoot -RunLabel $runLabel
                $runRecords += [pscustomobject]@{
                    name = $scenarioName
                    status = [string](Get-OptionalPropertyValue -Object $verificationMetadata.ScenarioStatuses -Name $scenarioName -DefaultValue "UNKNOWN")
                    runLabel = $runLabel
                    provenanceMode = [string](Get-OptionalPropertyValue -Object $verificationMetadata.ScenarioProvenance -Name $scenarioName -DefaultValue "")
                    sourceSummaryPath = $runArtifacts.SummaryFile.FullName
                    sourceRuntimePaths = @($runArtifacts.RuntimeFiles | ForEach-Object { $_.FullName })
                    sourceCodexManifestPath = if ($null -eq $runArtifacts.CodexManifest) { "" } else { $runArtifacts.CodexManifest.FullName }
                    artifactSummaryPath = ""
                    artifactRuntimePaths = @()
                    artifactCodexManifestPath = ""
                }
            }

            $runSummaryLines = @{}
            foreach ($runRecord in $runRecords) {
                $runSummaryLines[$runRecord.name] = @(Get-Content -LiteralPath $runRecord.sourceSummaryPath)
            }

            Write-Heading "Neighbor Surface Checks"
            $suiteFailed = $false
            foreach ($definition in $selectedEntries) {
                $happyCheck = Test-SurfaceCheckForRun -Definition $definition -RunName "happy" -SummaryLines $runSummaryLines["happy"]
                $gateCheck = Test-SurfaceCheckForRun -Definition $definition -RunName "gate" -SummaryLines $runSummaryLines["gate"]
                $overallStatus = if ($happyCheck.status -eq "PASS" -and $gateCheck.status -eq "PASS") { "PASS" } else { "FAIL" }
                if ($overallStatus -ne "PASS") {
                    $suiteFailed = $true
                }

                $surfaceResult = [pscustomobject]@{
                    name = $definition.Name
                    groups = @($definition.Groups)
                    overallStatus = $overallStatus
                    runs = @($happyCheck, $gateCheck)
                }
                $surfaceResults += $surfaceResult

                Write-Host ("{0}: {1}" -f $definition.Name, $overallStatus)
                Write-Host ("  groups: {0}" -f (@($definition.Groups) -join ", "))
                Write-Host ("  happy: {0}" -f $happyCheck.reason)
                Write-Host ("  gate: {0}" -f $gateCheck.reason)
            }

            $resultLabel = if ($suiteFailed) { "FAIL" } else { "PASS" }
            $finalExitCode = if ($suiteFailed) { 1 } else { 0 }
        }

        $groupResults = @(Build-GroupResults -SelectedEntries $selectedEntries -SurfaceResults $surfaceResults)
        Write-Heading "Group Summary"
        foreach ($groupResult in $groupResults) {
            Write-Host ("{0}: {1}" -f $groupResult.name, $groupResult.overallStatus)
            Write-Host ("  surfaces: {0}" -f (@($groupResult.surfaceNames) -join ", "))
            Write-Host ("  counts: pass={0}, fail={1}, not-run={2}" -f $groupResult.passingSurfaceCount, $groupResult.failingSurfaceCount, $groupResult.notRunSurfaceCount)
        }

        $summaryObject = [ordered]@{
            generatedAt = (Get-Date).ToString("o")
            suiteName = $matrixData.SuiteName
            promptId = $script:PromptId
            overallResult = $resultLabel
            requestedProvenanceMode = $ProvenanceMode
            observedProvenanceMode = Get-ObservedProvenanceMode -VerificationMetadata $verificationMetadata
            repoRoot = $resolvedRepoRoot
            outerWorkspaceRoot = [string](Get-OptionalPropertyValue -Object $delegatedRunnerContext -Name "WorkspaceRoot" -DefaultValue "")
            buildDir = [string](Get-OptionalPropertyValue -Object $delegatedRunnerContext -Name "BuildDir" -DefaultValue $BuildDir)
            executablePath = [string](Get-OptionalPropertyValue -Object $delegatedRunnerContext -Name "ExecutablePath" -DefaultValue $ExePath)
            headCommit = [string](Get-OptionalPropertyValue -Object $delegatedRunnerContext -Name "HeadCommit" -DefaultValue "")
            branch = [string](Get-OptionalPropertyValue -Object $delegatedRunnerContext -Name "Branch" -DefaultValue "")
            matrixPath = $resolvedMatrixPath
            requestedSurfaceNames = @($selectedMatrix.RequestedSurfaceNames)
            requestedGroupNames = @($selectedMatrix.RequestedGroupNames)
            selectedSurfaceNames = @($selectedMatrix.SelectedSurfaceNames)
            selectedGroupNames = @($selectedMatrix.SelectedGroupNames)
            delegatedVerificationCommandLine = $delegatedCommandLine
            runs = @($runRecords)
            groups = @($groupResults)
            surfaces = @($surfaceResults)
            metadata = [ordered]@{}
        }

        if ($CollectArtifacts -or -not [string]::IsNullOrWhiteSpace($ArtifactOutputDir)) {
            $summaryPaths = Write-SuiteSummaryFiles -OutputDir $resolvedArtifactOutputDir -SummaryObject $summaryObject -DelegatedOutputLogPath $delegatedOutputLogPath -VerificationMetadata $verificationMetadata
            Write-Heading "Artifact Bundle"
            Write-Host ("Output dir: {0}" -f $resolvedArtifactOutputDir)
            Write-Host ("Text summary: {0}" -f $summaryPaths.TextSummaryPath)
            Write-Host ("JSON summary: {0}" -f $summaryPaths.JsonSummaryPath)
        }

        Write-Heading "Verdict"
        Write-Host ("happy: {0}" -f [string](Get-OptionalPropertyValue -Object $verificationMetadata.ScenarioStatuses -Name "happy" -DefaultValue "UNKNOWN"))
        Write-Host ("gate: {0}" -f [string](Get-OptionalPropertyValue -Object $verificationMetadata.ScenarioStatuses -Name "gate" -DefaultValue "UNKNOWN"))
        Write-Host ("neighbor-surface suite: {0}" -f $resultLabel)
    }
}
catch {
    Write-Heading "Failure"
    Write-Host $_.Exception.Message
    $resultLabel = "FAIL"
    if ($finalExitCode -eq 0) {
        $finalExitCode = 1
    }
}
finally {
    if (-not [string]::IsNullOrWhiteSpace($cleanupOutputLogPath) -and (Test-Path -LiteralPath $cleanupOutputLogPath -PathType Leaf)) {
        Remove-Item -LiteralPath $cleanupOutputLogPath -Force -ErrorAction SilentlyContinue
    }
}

Write-Host ("Final overall verdict: {0}" -f $resultLabel)
exit $finalExitCode
