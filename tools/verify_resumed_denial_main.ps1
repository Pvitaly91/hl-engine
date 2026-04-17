param(
    [string]$RepoRoot,
    [string]$OuterWorkspaceRoot,
    [ValidateSet("auto", "runtime", "workspace")]
    [string]$ProvenanceMode = "auto",
    [switch]$NoBuild,
    [switch]$SkipRecovery,
    [switch]$NoExecute,
    [string]$ExePath,
    [string]$BuildDir,
    [switch]$UseExistingBinary
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
if (Test-Path Variable:PSNativeCommandUseErrorActionPreference) {
    $PSNativeCommandUseErrorActionPreference = $false
}

$script:PromptId = "HL-CL-20260411-163-dedicated-goldsrc-signon-carried-checkpoint-claimed-checkpoint-resumed-denial-surface"
$script:DefaultBuildDirName = "build-main-win32-hlhost-regression"
$script:RunTimeoutSeconds = 300
$script:RuntimeLogWaitSeconds = 20
$script:RequiredCliOptions = @(
    "--dedicated",
    "--query-surface",
    "--query-probe",
    "--connect-surface",
    "--connect-probe",
    "--activation-surface",
    "--activation-probe",
    "--run-label",
    "--prompt-id"
)
$script:ScenarioNames = @(
    "activation",
    "bootstrap",
    "bootstrap-sequence",
    "signon-catalog",
    "signon-template",
    "signon-template-completion",
    "signon-envelope",
    "signon-batch",
    "signon-wiremap",
    "signon-burst",
    "signon-stream",
    "signon-stream-window",
    "signon-message-catalog",
    "signon-message-fetch",
    "signon-multi-message-fetch",
    "signon-message-range-fetch",
    "signon-message-cursor",
    "signon-message-cursor-advance",
    "signon-message-cursor-eof",
    "signon-message-cursor-resume-denial",
    "signon-message-cursor-resume-allow",
    "signon-message-cursor-carryover",
    "signon-message-cursor-carried-range",
    "signon-message-cursor-carried-eof",
    "signon-message-cursor-carried-resume-allow",
    "signon-message-cursor-carried-checkpoint",
    "signon-message-cursor-carried-checkpoint-resume-allow",
    "signon-message-cursor-carried-checkpoint-resume-token",
    "signon-message-cursor-carried-checkpoint-resume-token-claim",
    "signon-message-cursor-carried-checkpoint-claimed-resume-allow",
    "signon-message-cursor-carried-checkpoint-claimed-checkpoint-bridge",
    "signon-message-cursor-carried-checkpoint-claimed-checkpoint-resume-allow",
    "signon-message-cursor-carried-checkpoint-claimed-checkpoint-resume-range",
    "signon-message-cursor-carried-checkpoint-claimed-checkpoint-resume-eof",
    "signon-message-cursor-carried-checkpoint-claimed-checkpoint-resumed-denial",
    "signon-message-cursor-carried-checkpoint-claimed-checkpoint-advance",
    "signon-message-cursor-carried-checkpoint-claimed-checkpoint-eof",
    "signon-message-cursor-carried-checkpoint-claimed-checkpoint-resume-denial",
    "signon-message-cursor-carried-checkpoint-claimed-resume-range",
    "signon-message-cursor-carried-checkpoint-claimed-resume-eof",
    "signon-message-cursor-carried-checkpoint-claimed-resumed-denial",
    "signon-message-cursor-carried-checkpoint-claimed-range",
    "signon-message-cursor-carried-checkpoint-claimed-eof",
    "signon-message-cursor-carried-checkpoint-claimed-resume-denial",
    "signon-message-cursor-carried-checkpoint-resume-range",
    "signon-message-cursor-carried-checkpoint-resume-eof",
    "signon-message-cursor-carried-checkpoint-resumed-denial",
    "signon-message-cursor-carried-checkpoint-advance",
    "signon-message-cursor-carried-checkpoint-eof",
    "signon-message-cursor-carried-checkpoint-resume-denial",
    "signon-message-cursor-carried-resume-denial"
)
$script:GateScenarioNames = @(
    "signon-message-cursor-carried-checkpoint-resume-token",
    "signon-message-cursor-carried-checkpoint-resume-token-claim",
    "signon-message-cursor-carried-checkpoint-claimed-resume-allow",
    "signon-message-cursor-carried-checkpoint-claimed-checkpoint-bridge",
    "signon-message-cursor-carried-checkpoint-claimed-checkpoint-resume-allow",
    "signon-message-cursor-carried-checkpoint-claimed-checkpoint-resume-range",
    "signon-message-cursor-carried-checkpoint-claimed-checkpoint-resume-eof",
    "signon-message-cursor-carried-checkpoint-claimed-checkpoint-resumed-denial",
    "signon-message-cursor-carried-checkpoint-claimed-checkpoint-advance",
    "signon-message-cursor-carried-checkpoint-claimed-checkpoint-eof",
    "signon-message-cursor-carried-checkpoint-claimed-checkpoint-resume-denial",
    "signon-message-cursor-carried-checkpoint-claimed-resume-range",
    "signon-message-cursor-carried-checkpoint-claimed-resume-eof",
    "signon-message-cursor-carried-checkpoint-claimed-resumed-denial",
    "signon-message-cursor-carried-checkpoint-claimed-range",
    "signon-message-cursor-carried-checkpoint-claimed-eof",
    "signon-message-cursor-carried-checkpoint-claimed-resume-denial"
)
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
$script:OptionalScenarioNames = @(
    "signon-message-cursor-carried-checkpoint-claimed-checkpoint-resume-token"
)
$script:OptionalGateScenarioNames = @(
    "signon-message-cursor-carried-checkpoint-claimed-checkpoint-resume-token"
)
$script:BenignWarningChecks = @(
    @{
        Name = "duplicate mp_defaultteam cvar registration"
        Predicate = { param([string[]]$Lines) $Lines | Where-Object { $_ -like "*Duplicate cvar registration detected for 'mp_defaultteam'.*" } | Select-Object -First 1 }
    },
    @{
        Name = "missing skill.cfg"
        Predicate = { param([string[]]$Lines) $Lines | Where-Object { $_ -like "*skill.cfg*" -and $_ -like "*missing*" } | Select-Object -First 1 }
    },
    @{
        Name = "missing game.cfg"
        Predicate = { param([string[]]$Lines) $Lines | Where-Object { $_ -like "*game.cfg*" -and $_ -like "*missing*" } | Select-Object -First 1 }
    },
    @{
        Name = "missing prompt-scoped valve-fixture sound assets"
        Predicate = { param([string[]]$Lines) $Lines | Where-Object { $_ -like "*Sound precache missing asset under valve/sound*" } | Select-Object -First 1 }
    }
)
$script:HardFailureChecks = @(
    @{
        Name = "Unhandled exception"
        Predicate = { param([string[]]$Lines) $Lines | Where-Object { $_ -match "(?i)unhandled exception" } | Select-Object -First 1 }
    },
    @{
        Name = "Structured exception handler signature"
        Predicate = { param([string[]]$Lines) $Lines | Where-Object { $_ -match "(?i)\bSEH\b" } | Select-Object -First 1 }
    },
    @{
        Name = "LoadLibraryW failure"
        Predicate = { param([string[]]$Lines) $Lines | Where-Object { $_ -match "(?i)LoadLibrary(?:Ex)?W failed" } | Select-Object -First 1 }
    }
)
$script:ConfigureCommandLine = ""
$script:BuildCommandLine = ""
$script:ResolvedExecutablePath = ""
$script:ResolvedGameDir = ""
$script:ResolvedRuntimeLogDir = ""
$script:ResolvedBuildDir = ""
$script:ResolvedRepoRoot = ""
$script:ResolvedWorkspaceRoot = ""
$script:ResolvedExpectedExecutablePath = ""
$script:ExpectedGitCommit = ""
$script:ExpectedGitBranch = ""
$script:ExecutableProvenanceSource = ""
$script:BuildWasPerformed = $false
$script:ProvenanceModeRequest = $ProvenanceMode

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

function Get-CheckoutState {
    $headCommit = Get-GitSingleLine @("rev-parse", "HEAD")
    $branchOutput = & git symbolic-ref --quiet --short HEAD
    $branchName = ""
    if ($LASTEXITCODE -eq 0 -and $null -ne $branchOutput) {
        $branchName = ($branchOutput | Select-Object -First 1).ToString().Trim()
    }

    $isDetached = ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($branchName))
    if ($isDetached) {
        $branchName = ""
    }

    return [pscustomobject]@{
        HeadCommit = $headCommit
        BranchName = $branchName
        IsDetached = $isDetached
    }
}

function Resolve-WorkspaceRoot {
    param(
        [string]$ResolvedRepoRoot,
        [string]$RequestedWorkspaceRoot
    )

    $workspaceSource = "repo-parent"
    if ([string]::IsNullOrWhiteSpace($RequestedWorkspaceRoot)) {
        $workspaceRoot = Split-Path -Path $ResolvedRepoRoot -Parent
        if ([string]::IsNullOrWhiteSpace($workspaceRoot)) {
            throw ("Unable to derive the outer workspace root from repo root: {0}" -f $ResolvedRepoRoot)
        }
    }
    else {
        $workspaceRoot = Resolve-FullPath -PathValue $RequestedWorkspaceRoot -BasePath $ResolvedRepoRoot
        $workspaceSource = "explicit"
    }

    $resolvedWorkspaceRoot = [System.IO.Path]::GetFullPath($workspaceRoot)
    $cmakeListsPath = Join-Path $resolvedWorkspaceRoot "CMakeLists.txt"
    if (-not (Test-Path -LiteralPath $cmakeListsPath -PathType Leaf)) {
        throw ("The outer workspace CMake entry point was not found at {0}. This regression runner expects the host repo to live under an hl-engine workspace root." -f $cmakeListsPath)
    }

    $expectedRepoRoot = [System.IO.Path]::GetFullPath((Join-Path $resolvedWorkspaceRoot "host"))
    $canonicalExpectedRepoRoot = Try-Get-GitRepoRoot -CandidateRoot $expectedRepoRoot
    $expectedComparisonRoot = if ([string]::IsNullOrWhiteSpace($canonicalExpectedRepoRoot)) { $expectedRepoRoot } else { $canonicalExpectedRepoRoot }
    if (-not $expectedComparisonRoot.Equals($ResolvedRepoRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw ("Repo root {0} does not match the host checkout expected under outer workspace root {1}. Expected host path {2} (canonical repo root {3}). Use a matching -RepoRoot/-OuterWorkspaceRoot pair." -f $ResolvedRepoRoot, $resolvedWorkspaceRoot, $expectedRepoRoot, $(if ([string]::IsNullOrWhiteSpace($canonicalExpectedRepoRoot)) { "<unresolved>" } else { $canonicalExpectedRepoRoot }))
    }

    return [pscustomobject]@{
        Root = $resolvedWorkspaceRoot
        Source = $workspaceSource
    }
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

function Resolve-CMakeExecutable {
    $candidates = @(
        "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
        "C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
        "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
        "C:\Program Files\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
    )

    foreach ($candidate in $candidates) {
        if (Test-Path -LiteralPath $candidate -PathType Leaf) {
            return $candidate
        }
    }

    $command = Get-Command cmake.exe -ErrorAction SilentlyContinue
    if ($null -ne $command -and -not [string]::IsNullOrWhiteSpace($command.Source)) {
        return $command.Source
    }

    throw "Unable to locate cmake.exe. Install Visual Studio 2022 CMake support or add cmake.exe to PATH."
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

function Invoke-ExternalCommand {
    param(
        [string]$FilePath,
        [string[]]$Arguments,
        [string]$Description
    )

    $commandLine = Format-CommandLine -ExecutablePath $FilePath -Arguments $Arguments
    Write-Host ("{0} command:" -f $Description)
    Write-Host ("  {0}" -f $commandLine)

    if ($Description -eq "Configure") {
        $script:ConfigureCommandLine = $commandLine
    }
    elseif ($Description -eq "Build") {
        $script:BuildCommandLine = $commandLine
    }

    & $FilePath @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw ("{0} failed with exit code {1}." -f $Description, $LASTEXITCODE)
    }
}

function Get-ExpectedExecutablePath {
    param([string]$ResolvedBuildDir)

    return [System.IO.Path]::GetFullPath((Join-Path $ResolvedBuildDir "host\Debug\hlhost.exe"))
}

function Get-ScenarioModes {
    param([string]$DefaultMode)

    $modeByScenario = @{}
    foreach ($scenarioName in $script:ScenarioNames) {
        $modeByScenario[$scenarioName] = $DefaultMode
    }

    return $modeByScenario
}

function New-GateScenarioModes {
    $modeByScenario = Get-ScenarioModes -DefaultMode "happy"
    foreach ($scenarioName in $script:GateScenarioNames) {
        $modeByScenario[$scenarioName] = "gate"
    }

    return $modeByScenario
}

function Add-ScenarioArguments {
    param(
        [System.Collections.Generic.List[string]]$Arguments,
        [string]$ScenarioName,
        [string]$ProbeScenario
    )

    $Arguments.Add(("--{0}-surface" -f $ScenarioName))
    $Arguments.Add(("--{0}-probe" -f $ScenarioName))
    $Arguments.Add(("--{0}-probe-scenario" -f $ScenarioName))
    $Arguments.Add($ProbeScenario)
}

function New-LaunchArguments {
    param(
        [string]$RunLabel,
        [hashtable]$ScenarioModes
    )

    $arguments = New-Object System.Collections.Generic.List[string]
    $arguments.Add("--dedicated")
    $arguments.Add("--gamedir")
    $arguments.Add($script:ResolvedGameDir)
    $arguments.Add("--map")
    $arguments.Add("c0a0")
    $arguments.Add("--deathmatch")
    $arguments.Add("1")
    $arguments.Add("--coop")
    $arguments.Add("0")
    $arguments.Add("--maxclients")
    $arguments.Add("4")
    $arguments.Add("--synthetic-players")
    $arguments.Add("0")
    $arguments.Add("--query-surface")
    $arguments.Add("--query-probe")
    $arguments.Add("--query-port")
    $arguments.Add("0")
    $arguments.Add("--connect-surface")
    $arguments.Add("--connect-probe")
    $arguments.Add("--connect-probe-scenario")
    $arguments.Add("accept")
    foreach ($scenarioName in $script:ScenarioNames) {
        Add-ScenarioArguments -Arguments $arguments -ScenarioName $scenarioName -ProbeScenario $ScenarioModes[$scenarioName]
    }
    $arguments.Add("--run-label")
    $arguments.Add($RunLabel)
    $arguments.Add("--prompt-id")
    $arguments.Add($script:PromptId)
    $arguments.Add("--frames")
    $arguments.Add("1000")
    $arguments.Add("--frametime")
    $arguments.Add("0.05")
    $arguments.Add("--stop-on-changelevel-request")
    $arguments.Add("0")

    return $arguments.ToArray()
}

function Get-NewestGeneratedFile {
    param(
        [string]$DirectoryPath,
        [string]$Filter,
        [datetime]$StartedAt
    )

    $deadline = (Get-Date).AddSeconds($script:RuntimeLogWaitSeconds)
    do {
        $matches = @(
            Get-ChildItem -LiteralPath $DirectoryPath -Filter $Filter -File -ErrorAction SilentlyContinue |
                Where-Object { $_.LastWriteTime -ge $StartedAt.AddSeconds(-1) } |
                Sort-Object LastWriteTime -Descending
        )
        if ($matches.Count -gt 0) {
            return $matches[0]
        }

        Start-Sleep -Milliseconds 500
    }
    while ((Get-Date) -lt $deadline)

    throw ("No file matching '{0}' appeared under {1} after {2}." -f $Filter, $DirectoryPath, $StartedAt.ToString("s"))
}

function Get-GeneratedRuntimeLogs {
    param(
        [string]$RunLabel,
        [datetime]$StartedAt
    )

    $summaryFile = Get-NewestGeneratedFile -DirectoryPath $script:ResolvedRuntimeLogDir -Filter ("*__{0}_summary.log" -f $RunLabel) -StartedAt $StartedAt
    $runtimeFiles = @(
        Get-ChildItem -LiteralPath $script:ResolvedRuntimeLogDir -Filter ("*__{0}_part*.log" -f $RunLabel) -File -ErrorAction SilentlyContinue |
            Where-Object { $_.LastWriteTime -ge $StartedAt.AddSeconds(-1) } |
            Sort-Object LastWriteTime
    )
    if ($runtimeFiles.Count -eq 0) {
        throw ("No runtime part logs were produced for run label {0}." -f $RunLabel)
    }

    return [pscustomobject]@{
        SummaryFile = $summaryFile
        RuntimeFiles = $runtimeFiles
    }
}

function Get-FirstMatchingLine {
    param(
        [string[]]$Lines,
        [string]$Needle,
        [string]$Description
    )

    $matches = @($Lines | Where-Object { $_ -like ("*{0}*" -f $Needle) })
    if ($matches.Count -eq 0) {
        throw ("Missing {0}: {1}" -f $Description, $Needle)
    }

    return $matches[0]
}

function Assert-LineContainsAll {
    param(
        [string]$Line,
        [string]$Description,
        [string[]]$ExpectedTokens
    )

    foreach ($token in $ExpectedTokens) {
        if ($Line.IndexOf($token, [System.StringComparison]::Ordinal) -lt 0) {
            throw ("{0} missing expected token '{1}'." -f $Description, $token)
        }
    }
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

function Get-WorkspaceProvenanceState {
    $reasons = New-Object System.Collections.Generic.List[string]
    $pathMatchesExpected = $false

    if ([string]::IsNullOrWhiteSpace($script:ResolvedRepoRoot)) {
        $reasons.Add("repo root is unresolved")
    }

    if ([string]::IsNullOrWhiteSpace($script:ResolvedBuildDir)) {
        $reasons.Add("build dir is unresolved")
    }

    if ([string]::IsNullOrWhiteSpace($script:ResolvedExecutablePath)) {
        $reasons.Add("executable path is unresolved")
    }

    if ([string]::IsNullOrWhiteSpace($script:ResolvedExpectedExecutablePath)) {
        $reasons.Add("expected build output path is unresolved")
    }
    elseif (-not [string]::IsNullOrWhiteSpace($script:ResolvedExecutablePath)) {
        $pathMatchesExpected = $script:ResolvedExecutablePath.Equals($script:ResolvedExpectedExecutablePath, [System.StringComparison]::OrdinalIgnoreCase)
        if (-not $pathMatchesExpected) {
            $reasons.Add(("executable path {0} does not match expected build output {1}" -f $script:ResolvedExecutablePath, $script:ResolvedExpectedExecutablePath))
        }
    }

    if ([string]::IsNullOrWhiteSpace($script:ExpectedGitCommit)) {
        $reasons.Add("current HEAD commit is unresolved")
    }

    $detail = ("repoRoot={0}; workspaceRoot={1}; head={2}; branch={3}; buildDir={4}; executable={5}; expectedBuildOutput={6}; executableSource={7}" -f
        $script:ResolvedRepoRoot,
        $script:ResolvedWorkspaceRoot,
        $(if ([string]::IsNullOrWhiteSpace($script:ExpectedGitCommit)) { "<unknown>" } else { $script:ExpectedGitCommit }),
        $(if ([string]::IsNullOrWhiteSpace($script:ExpectedGitBranch)) { "<detached>" } else { $script:ExpectedGitBranch }),
        $script:ResolvedBuildDir,
        $script:ResolvedExecutablePath,
        $script:ResolvedExpectedExecutablePath,
        $(if ([string]::IsNullOrWhiteSpace($script:ExecutableProvenanceSource)) { "<unknown>" } else { $script:ExecutableProvenanceSource }))

    return [pscustomobject]@{
        Eligible = ($reasons.Count -eq 0)
        Detail = $detail
        FailureReason = ($reasons -join "; ")
        PathMatchesExpected = $pathMatchesExpected
        ExecutableSource = $script:ExecutableProvenanceSource
    }
}

function Resolve-CodexRunProvenance {
    param(
        [string]$RunName,
        [string[]]$SummaryLines,
        [string]$RunLabel
    )

    $identityLine = Get-FirstMatchingLine -Lines $SummaryLines -Needle "codex_run_identity:" -Description ("{0} codex_run_identity" -f $RunName)
    Assert-LineContainsAll -Line $identityLine -Description ("{0} codex_run_identity" -f $RunName) -ExpectedTokens @(
        ("runLabel={0}" -f $RunLabel),
        ("promptId={0}" -f $script:PromptId),
        "stopOnChangelevelRequest=0",
        "frames=1000",
        "frametime=0.050000"
    )

    $runtimeGitBranch = Get-CodexRunIdentityFieldValue -IdentityLine $identityLine -FieldName "gitBranch"
    $runtimeGitCommit = Get-CodexRunIdentityFieldValue -IdentityLine $identityLine -FieldName "gitCommit"
    $runtimeIdentityText = ("gitBranch={0}, gitCommit={1}" -f $runtimeGitBranch, $runtimeGitCommit)
    $runtimeCommitMatches = (
        -not [string]::IsNullOrWhiteSpace($script:ExpectedGitCommit) -and
        $runtimeGitCommit.Equals($script:ExpectedGitCommit, [System.StringComparison]::OrdinalIgnoreCase)
    )
    $runtimeCommitUnknown = ($runtimeGitCommit -eq "<unknown>")
    $workspaceState = Get-WorkspaceProvenanceState

    switch ($script:ProvenanceModeRequest) {
        "runtime" {
            if (-not $runtimeCommitMatches) {
                throw ("{0} runtime provenance required: expected gitCommit {1}, but codex_run_identity reported {2}." -f $RunName, $script:ExpectedGitCommit, $runtimeIdentityText)
            }

            return [pscustomobject]@{
                Mode = "runtime"
                Detail = ("runtime codex_run_identity matched expected HEAD {0}; {1}" -f $script:ExpectedGitCommit, $runtimeIdentityText)
            }
        }

        "workspace" {
            if ($runtimeGitCommit -eq "<missing>") {
                throw ("{0} workspace provenance requested, but codex_run_identity is missing gitCommit. Runtime identity must at least report the field before workspace provenance can take over." -f $RunName)
            }

            if ($runtimeCommitMatches) {
                return [pscustomobject]@{
                    Mode = "workspace"
                    Detail = ("workspace provenance selected; runtime codex_run_identity corroborated expected HEAD {0}; {1}; {2}" -f $script:ExpectedGitCommit, $runtimeIdentityText, $workspaceState.Detail)
                }
            }

            if (-not $runtimeCommitUnknown) {
                throw ("{0} workspace provenance rejected because runtime codex_run_identity reported a conflicting commit. Expected gitCommit {1}; runtime reported {2}." -f $RunName, $script:ExpectedGitCommit, $runtimeIdentityText)
            }

            if (-not $workspaceState.Eligible) {
                throw ("{0} workspace provenance fallback is unavailable: {1}." -f $RunName, $workspaceState.FailureReason)
            }

            return [pscustomobject]@{
                Mode = "workspace"
                Detail = ("workspace provenance fallback accepted because runtime codex_run_identity reported {0}; {1}" -f $runtimeIdentityText, $workspaceState.Detail)
            }
        }

        default {
            if ($runtimeCommitMatches) {
                return [pscustomobject]@{
                    Mode = "runtime"
                    Detail = ("runtime codex_run_identity matched expected HEAD {0}; {1}" -f $script:ExpectedGitCommit, $runtimeIdentityText)
                }
            }

            if ($runtimeGitCommit -eq "<missing>") {
                throw ("{0} codex_run_identity is missing gitCommit, so auto provenance cannot validate the binary. Expected gitCommit {1}." -f $RunName, $script:ExpectedGitCommit)
            }

            if (-not $runtimeCommitUnknown) {
                throw ("{0} runtime provenance mismatch: expected gitCommit {1}, but codex_run_identity reported {2}." -f $RunName, $script:ExpectedGitCommit, $runtimeIdentityText)
            }

            if (-not $workspaceState.Eligible) {
                throw ("{0} runtime codex_run_identity reported {1}, and workspace provenance fallback is unavailable: {2}." -f $RunName, $runtimeIdentityText, $workspaceState.FailureReason)
            }

            return [pscustomobject]@{
                Mode = "workspace"
                Detail = ("workspace provenance fallback accepted because runtime codex_run_identity reported {0}; {1}" -f $runtimeIdentityText, $workspaceState.Detail)
            }
        }
    }
}

function Get-BoundPortFromSummaryLine {
    param(
        [string]$Line,
        [string]$Description
    )

    $match = [regex]::Match($Line, "boundPort=(\d+)")
    if (-not $match.Success) {
        throw ("Unable to parse boundPort from {0}." -f $Description)
    }

    return [int]$match.Groups[1].Value
}

function Convert-ScenarioNameToStartupTokenPrefix {
    param([string]$ScenarioName)

    return ($ScenarioName -replace "-", "_")
}

function Assert-StartupConfigMatchesRecipe {
    param(
        [string]$RunName,
        [string[]]$RuntimeLines,
        [hashtable]$ScenarioModes
    )

    $configLine = Get-FirstMatchingLine -Lines $RuntimeLines -Needle "Runtime foundation config:" -Description ("{0} startup config" -f $RunName)
    $expectedTokens = New-Object System.Collections.Generic.List[string]
    $expectedTokens.Add("mode=dedicated")
    $expectedTokens.Add("deathmatch=1")
    $expectedTokens.Add("coop=0")
    $expectedTokens.Add("maxclients=4")
    $expectedTokens.Add("synthetic_players=0")
    $expectedTokens.Add("query_surface=1")
    $expectedTokens.Add("query_probe=1")
    $expectedTokens.Add("query_port=0")
    $expectedTokens.Add("connect_surface=1")
    $expectedTokens.Add("connect_probe=1")
    $expectedTokens.Add("connect_probe_scenario=accept")
    foreach ($scenarioName in $script:ScenarioNames) {
        $prefix = Convert-ScenarioNameToStartupTokenPrefix $scenarioName
        $expectedTokens.Add(("{0}_surface=1" -f $prefix))
        $expectedTokens.Add(("{0}_probe=1" -f $prefix))
        $expectedTokens.Add(("{0}_probe_scenario={1}" -f $prefix, $ScenarioModes[$scenarioName]))
    }

    Assert-LineContainsAll -Line $configLine -Description ("{0} startup config" -f $RunName) -ExpectedTokens $expectedTokens.ToArray()
}

function Assert-CodexRunIdentity {
    param(
        [string]$RunName,
        [string[]]$SummaryLines,
        [string]$RunLabel
    )

    $identityLine = Get-FirstMatchingLine -Lines $SummaryLines -Needle "codex_run_identity:" -Description ("{0} codex_run_identity" -f $RunName)
    Assert-LineContainsAll -Line $identityLine -Description ("{0} codex_run_identity" -f $RunName) -ExpectedTokens @(
        ("runLabel={0}" -f $RunLabel),
        ("promptId={0}" -f $script:PromptId),
        ("gitCommit={0}" -f $script:ExpectedGitCommit),
        "stopOnChangelevelRequest=0",
        "frames=1000",
        "frametime=0.050000"
    )
}

function Assert-PortSharing {
    param(
        [string]$RunName,
        [string[]]$SummaryLines,
        [string]$TargetSurfaceLine
    )

    Assert-LineContainsAll -Line $TargetSurfaceLine -Description ("{0} target surface" -f $RunName) -ExpectedTokens @(
        "mode=dedicated",
        "bind=loopback",
        "requestedPort=0"
    )

    $targetPort = Get-BoundPortFromSummaryLine -Line $TargetSurfaceLine -Description ("{0} target surface" -f $RunName)
    if ($targetPort -le 0) {
        throw ("{0} target surface boundPort must be non-zero, found {1}." -f $RunName, $targetPort)
    }

    $querySurfaceLine = Get-FirstMatchingLine -Lines $SummaryLines -Needle "dedicated_query_surface:" -Description ("{0} query surface" -f $RunName)
    $connectSurfaceLine = Get-FirstMatchingLine -Lines $SummaryLines -Needle "dedicated_connect_surface:" -Description ("{0} connect surface" -f $RunName)
    $activationSurfaceLine = Get-FirstMatchingLine -Lines $SummaryLines -Needle "dedicated_activation_surface:" -Description ("{0} activation surface" -f $RunName)
    foreach ($pair in @(
        @{ Name = "query"; Line = $querySurfaceLine },
        @{ Name = "connect"; Line = $connectSurfaceLine },
        @{ Name = "activation"; Line = $activationSurfaceLine }
    )) {
        Assert-LineContainsAll -Line $pair.Line -Description ("{0} {1} surface" -f $RunName, $pair.Name) -ExpectedTokens @("bind=loopback", "requestedPort=0")
        $port = Get-BoundPortFromSummaryLine -Line $pair.Line -Description ("{0} {1} surface" -f $RunName, $pair.Name)
        if ($port -ne $targetPort) {
            throw ("{0} resumed-denial surface boundPort {1} is not shared with the {2} surface boundPort {3}." -f $RunName, $targetPort, $pair.Name, $port)
        }
    }
}

function Get-ObservedBenignWarnings {
    param([string[]]$RuntimeLines)

    $observed = New-Object System.Collections.Generic.List[string]
    foreach ($check in $script:BenignWarningChecks) {
        $match = & $check.Predicate $RuntimeLines
        if ($null -ne $match) {
            $observed.Add([string]$check.Name)
        }
    }

    return @($observed | Sort-Object -Unique)
}

function Initialize-OptionalScenariosFromHelp {
    param([string]$HelpText)

    foreach ($scenarioName in $script:OptionalScenarioNames) {
        $requiredScenarioOptions = @(
            ("--{0}-surface" -f $scenarioName),
            ("--{0}-probe" -f $scenarioName),
            ("--{0}-probe-scenario" -f $scenarioName)
        )
        $scenarioPresent = $true
        foreach ($optionName in $requiredScenarioOptions) {
            if ($HelpText.IndexOf($optionName, [System.StringComparison]::Ordinal) -lt 0) {
                $scenarioPresent = $false
                break
            }
        }

        if (-not $scenarioPresent) {
            continue
        }

        if ($script:ScenarioNames -notcontains $scenarioName) {
            $script:ScenarioNames += $scenarioName
            Write-Host ("Detected optional current-main scenario: {0}" -f $scenarioName)
        }

        if (
            $script:OptionalGateScenarioNames -contains $scenarioName -and
            $script:GateScenarioNames -notcontains $scenarioName
        ) {
            $script:GateScenarioNames += $scenarioName
        }
    }
}

function Assert-ExecutableSupportsRecipe {
    $helpLines = & $script:ResolvedExecutablePath --help 2>&1
    if ($LASTEXITCODE -ne 0) {
        throw ("Unable to inspect {0} with --help (exit code {1})." -f $script:ResolvedExecutablePath, $LASTEXITCODE)
    }

    $helpText = $helpLines -join "`n"
    $missingOptions = @($script:RequiredCliOptions | Where-Object { $helpText.IndexOf($_, [System.StringComparison]::Ordinal) -lt 0 })
    if ($missingOptions.Count -gt 0) {
        throw ("{0} does not expose the dedicated/probe options required by the resumed-denial regression recipe: {1}. Rebuild the current-main hlhost.exe before running full verification." -f $script:ResolvedExecutablePath, ($missingOptions -join ", "))
    }

    Initialize-OptionalScenariosFromHelp -HelpText $helpText
}

function Assert-NoHardFailures {
    param(
        [string]$RunName,
        [string[]]$RuntimeLines
    )

    foreach ($check in $script:HardFailureChecks) {
        $match = & $check.Predicate $RuntimeLines
        if ($null -ne $match) {
            throw ("{0} runtime log contains hard failure '{1}': {2}" -f $RunName, $check.Name, $match)
        }
    }
}

function Invoke-NativeProcessWithTimeout {
    param(
        [string]$FilePath,
        [string[]]$Arguments,
        [int]$TimeoutSeconds
    )

    $startInfo = New-Object System.Diagnostics.ProcessStartInfo
    $startInfo.FileName = $FilePath
    $startInfo.Arguments = (($Arguments | ForEach-Object { Quote-Argument $_ }) -join " ")
    $startInfo.WorkingDirectory = (Get-Location).Path
    $startInfo.UseShellExecute = $false
    $startInfo.CreateNoWindow = $true

    $process = New-Object System.Diagnostics.Process
    $process.StartInfo = $startInfo

    if (-not $process.Start()) {
        throw ("Failed to start process: {0}" -f $FilePath)
    }

    if (-not $process.WaitForExit($TimeoutSeconds * 1000)) {
        try {
            $process.Kill()
        }
        catch {
        }

        throw ("Process exceeded timeout after {0} seconds: {1}" -f $TimeoutSeconds, $FilePath)
    }

    $process.WaitForExit()
    return $process.ExitCode
}

function Invoke-HlhostRun {
    param(
        [string]$RunName,
        [string]$RunLabel,
        [hashtable]$ScenarioModes
    )

    $arguments = New-LaunchArguments -RunLabel $RunLabel -ScenarioModes $ScenarioModes
    $commandLine = Format-CommandLine -ExecutablePath $script:ResolvedExecutablePath -Arguments $arguments
    Write-Host ("{0} command:" -f $RunName)
    Write-Host ("  {0}" -f $commandLine)

    if ($NoExecute) {
        return [pscustomobject]@{
            RunName = $RunName
            RunLabel = $RunLabel
            CommandLine = $commandLine
            SummaryPath = ""
            Status = "SKIPPED"
            BenignWarnings = @()
            ProvenanceMode = ""
            ProvenanceDetail = ""
        }
    }

    $startedAt = Get-Date
    $exitCode = Invoke-NativeProcessWithTimeout -FilePath $script:ResolvedExecutablePath -Arguments $arguments -TimeoutSeconds $script:RunTimeoutSeconds
    if ($exitCode -ne 0) {
        throw ("{0} exited unexpectedly with code {1}." -f $RunName, $exitCode)
    }

    $generatedFiles = Get-GeneratedRuntimeLogs -RunLabel $RunLabel -StartedAt $startedAt
    $summaryLines = Get-Content -LiteralPath $generatedFiles.SummaryFile.FullName
    $runtimeLines = @()
    foreach ($runtimeFile in $generatedFiles.RuntimeFiles) {
        $runtimeLines += Get-Content -LiteralPath $runtimeFile.FullName
    }

    Assert-NoHardFailures -RunName $RunName -RuntimeLines ($summaryLines + $runtimeLines)
    Assert-StartupConfigMatchesRecipe -RunName $RunName -RuntimeLines $runtimeLines -ScenarioModes $ScenarioModes
    $provenanceResult = Resolve-CodexRunProvenance -RunName $RunName -SummaryLines $summaryLines -RunLabel $RunLabel

    $lifecycleLine = Get-FirstMatchingLine -Lines $summaryLines -Needle "dedicated_player_lifecycle_foundation:" -Description ("{0} lifecycle summary" -f $RunName)
    Assert-LineContainsAll -Line $lifecycleLine -Description ("{0} lifecycle summary" -f $RunName) -ExpectedTokens $script:LifecycleMarkers

    $surfaceLine = Get-FirstMatchingLine -Lines $summaryLines -Needle "dedicated_signon_message_cursor_carried_checkpoint_claimed_checkpoint_resumed_denial_surface:" -Description ("{0} resumed-denial surface" -f $RunName)
    $probeLine = Get-FirstMatchingLine -Lines $summaryLines -Needle "dedicated_signon_message_cursor_carried_checkpoint_claimed_checkpoint_resumed_denial_probe:" -Description ("{0} resumed-denial probe" -f $RunName)
    if ($RunName -eq "gate") {
        Assert-LineContainsAll -Line $surfaceLine -Description ("{0} resumed-denial surface" -f $RunName) -ExpectedTokens $script:GateSurfaceMarkers
        Assert-LineContainsAll -Line $probeLine -Description ("{0} resumed-denial probe" -f $RunName) -ExpectedTokens $script:GateProbeMarkers
    }
    else {
        Assert-LineContainsAll -Line $surfaceLine -Description ("{0} resumed-denial surface" -f $RunName) -ExpectedTokens $script:HappySurfaceMarkers
        Assert-LineContainsAll -Line $probeLine -Description ("{0} resumed-denial probe" -f $RunName) -ExpectedTokens $script:HappyProbeMarkers
    }

    Assert-PortSharing -RunName $RunName -SummaryLines $summaryLines -TargetSurfaceLine $surfaceLine
    $benignWarnings = @(Get-ObservedBenignWarnings -RuntimeLines $runtimeLines)

    return [pscustomobject]@{
        RunName = $RunName
        RunLabel = $RunLabel
        CommandLine = $commandLine
        SummaryPath = $generatedFiles.SummaryFile.FullName
        Status = "PASS"
        BenignWarnings = $benignWarnings
        ProvenanceMode = $provenanceResult.Mode
        ProvenanceDetail = $provenanceResult.Detail
    }
}

function Resolve-GameDirPath {
    param([string]$ResolvedRepoRoot)

    $gameDirPath = Join-Path $ResolvedRepoRoot ("logs\latest\{0}\runtime\valve-fixture" -f $script:PromptId)
    if (-not (Test-Path -LiteralPath $gameDirPath -PathType Container)) {
        throw ("Prompt-scoped valve-fixture gameDir was not found at {0}." -f $gameDirPath)
    }

    return [System.IO.Path]::GetFullPath($gameDirPath)
}

function Resolve-RuntimeLogDirPath {
    param([string]$ResolvedRepoRoot)

    $runtimeLogDirPath = Join-Path $ResolvedRepoRoot "logs\latest\runtime"
    if (-not (Test-Path -LiteralPath $runtimeLogDirPath -PathType Container)) {
        New-Item -ItemType Directory -Path $runtimeLogDirPath -Force | Out-Null
    }

    return [System.IO.Path]::GetFullPath($runtimeLogDirPath)
}

function Build-HlhostIfNeeded {
    param(
        [string]$WorkspaceRoot,
        [string]$ResolvedBuildDir
    )

    $cmakeExecutable = Resolve-CMakeExecutable
    $configureArguments = @(
        "-S",
        $WorkspaceRoot,
        "-B",
        $ResolvedBuildDir,
        "-G",
        "Visual Studio 17 2022",
        "-A",
        "Win32"
    )
    $buildArguments = @(
        "--build",
        $ResolvedBuildDir,
        "--config",
        "Debug",
        "--target",
        "hlhost",
        "--clean-first"
    )

    Write-Heading "Build"
    Invoke-ExternalCommand -FilePath $cmakeExecutable -Arguments $configureArguments -Description "Configure"
    Invoke-ExternalCommand -FilePath $cmakeExecutable -Arguments $buildArguments -Description "Build"

    $expectedExecutablePath = Get-ExpectedExecutablePath -ResolvedBuildDir $ResolvedBuildDir
    if (-not (Test-Path -LiteralPath $expectedExecutablePath -PathType Leaf)) {
        throw ("Build completed without producing {0}." -f $expectedExecutablePath)
    }
}

function Resolve-ExecutablePath {
    param(
        [string]$ResolvedRepoRoot,
        [string]$ResolvedBuildDir,
        [string]$RequestedExecutablePath
    )

    if (-not [string]::IsNullOrWhiteSpace($RequestedExecutablePath)) {
        $explicitPath = Resolve-FullPath -PathValue $RequestedExecutablePath -BasePath $ResolvedRepoRoot
        if (-not (Test-Path -LiteralPath $explicitPath -PathType Leaf)) {
            throw ("Explicit hlhost.exe path does not exist: {0}" -f $explicitPath)
        }

        return $explicitPath
    }

    $expectedExecutablePath = Get-ExpectedExecutablePath -ResolvedBuildDir $ResolvedBuildDir
    if (-not (Test-Path -LiteralPath $expectedExecutablePath -PathType Leaf)) {
        throw ("Expected hlhost.exe was not found at {0}. Re-run without -NoBuild/-UseExistingBinary or pass -ExePath." -f $expectedExecutablePath)
    }

    return $expectedExecutablePath
}

$resolvedRepoRoot = $null
$scenarioStatuses = [ordered]@{
    happy = "PENDING"
    gate = "PENDING"
    recovery = if ($SkipRecovery) { "SKIPPED" } else { "PENDING" }
}
$runResults = New-Object System.Collections.Generic.List[object]

try {
    $resolvedRepoRoot = Get-RepoRootFromRequest -RequestedRoot $RepoRoot
    Push-Location $resolvedRepoRoot
    try {
        $checkoutState = Get-CheckoutState
        $workspaceResolution = Resolve-WorkspaceRoot -ResolvedRepoRoot $resolvedRepoRoot -RequestedWorkspaceRoot $OuterWorkspaceRoot
        $workspaceRoot = $workspaceResolution.Root
        $resolvedBuildDir = Resolve-BuildDirPath -WorkspaceRoot $workspaceRoot -RequestedBuildDir $BuildDir
        $script:ResolvedRepoRoot = $resolvedRepoRoot
        $script:ResolvedWorkspaceRoot = $workspaceRoot
        $script:ResolvedGameDir = Resolve-GameDirPath -ResolvedRepoRoot $resolvedRepoRoot
        $script:ResolvedRuntimeLogDir = Resolve-RuntimeLogDirPath -ResolvedRepoRoot $resolvedRepoRoot
        $script:ResolvedBuildDir = $resolvedBuildDir
        $script:ResolvedExpectedExecutablePath = Get-ExpectedExecutablePath -ResolvedBuildDir $resolvedBuildDir
        $script:ExpectedGitCommit = $checkoutState.HeadCommit
        $script:ExpectedGitBranch = if ($checkoutState.IsDetached) { "" } else { $checkoutState.BranchName }

        Write-Heading "Repository"
        Write-Host ("Repo root: {0}" -f $resolvedRepoRoot)
        Write-Host ("Workspace root: {0}" -f $workspaceRoot)
        Write-Host ("Workspace root source: {0}" -f $workspaceResolution.Source)
        Write-Host ("Branch: {0}" -f $(if ($checkoutState.IsDetached) { "detached" } else { $checkoutState.BranchName }))
        Write-Host ("HEAD: {0}" -f $checkoutState.HeadCommit)
        if (-not $checkoutState.IsDetached -and $checkoutState.BranchName -ne "main") {
            Write-Host ("Note: current checkout is '{0}'. This runner is intended for mainline regression and is validating the current workspace state." -f $checkoutState.BranchName)
        }

        Write-Heading "Inputs"
        Write-Host ("Prompt ID: {0}" -f $script:PromptId)
        Write-Host ("Requested provenance mode: {0}" -f $script:ProvenanceModeRequest)
        Write-Host ("Game dir: {0}" -f $script:ResolvedGameDir)
        Write-Host ("Runtime log dir: {0}" -f $script:ResolvedRuntimeLogDir)
        Write-Host ("Build dir: {0}" -f $resolvedBuildDir)
        Write-Host ("Expected build output: {0}" -f $script:ResolvedExpectedExecutablePath)

        if (-not [string]::IsNullOrWhiteSpace($ExePath)) {
            Write-Heading "Binary"
            Write-Host "Using explicit -ExePath override."
            $script:ResolvedExecutablePath = Resolve-ExecutablePath -ResolvedRepoRoot $resolvedRepoRoot -ResolvedBuildDir $resolvedBuildDir -RequestedExecutablePath $ExePath
            if ($script:ResolvedExecutablePath.Equals($script:ResolvedExpectedExecutablePath, [System.StringComparison]::OrdinalIgnoreCase)) {
                $script:ExecutableProvenanceSource = "explicit-expected-build-output"
            }
            else {
                $script:ExecutableProvenanceSource = "explicit-nonstandard-path"
            }
        }
        elseif ($NoBuild -or $UseExistingBinary) {
            Write-Heading "Binary"
            Write-Host "Using the existing binary in the configured build directory."
            $script:ResolvedExecutablePath = Resolve-ExecutablePath -ResolvedRepoRoot $resolvedRepoRoot -ResolvedBuildDir $resolvedBuildDir -RequestedExecutablePath ""
            $script:ExecutableProvenanceSource = "configured-build-output"
        }
        else {
            Build-HlhostIfNeeded -WorkspaceRoot $workspaceRoot -ResolvedBuildDir $resolvedBuildDir
            $script:ResolvedExecutablePath = Resolve-ExecutablePath -ResolvedRepoRoot $resolvedRepoRoot -ResolvedBuildDir $resolvedBuildDir -RequestedExecutablePath ""
            $script:BuildWasPerformed = $true
            $script:ExecutableProvenanceSource = "runner-built"
        }

        Write-Host ("Executable: {0}" -f $script:ResolvedExecutablePath)
        $workspaceProvenanceState = Get-WorkspaceProvenanceState
        Write-Heading "Provenance"
        Write-Host ("Requested mode: {0}" -f $script:ProvenanceModeRequest)
        Write-Host ("Expected HEAD: {0}" -f $script:ExpectedGitCommit)
        Write-Host ("Expected branch: {0}" -f $(if ([string]::IsNullOrWhiteSpace($script:ExpectedGitBranch)) { "<detached>" } else { $script:ExpectedGitBranch }))
        Write-Host ("Executable provenance source: {0}" -f $script:ExecutableProvenanceSource)
        Write-Host ("Workspace fallback eligible: {0}" -f $(if ($workspaceProvenanceState.Eligible) { "yes" } else { "no" }))
        if ($workspaceProvenanceState.Eligible) {
            Write-Host ("  detail: {0}" -f $workspaceProvenanceState.Detail)
        }
        else {
            Write-Host ("  reason: {0}" -f $workspaceProvenanceState.FailureReason)
        }
        Write-Heading "Executable Preflight"
        Assert-ExecutableSupportsRecipe
        Write-Host "Dedicated/probe runtime options: present"

        $runStamp = Get-Date -Format "yyyyMMdd_HHmmss"
        $happyRunLabel = "verify-resumed-denial-main-{0}-happy" -f $runStamp
        $gateRunLabel = "verify-resumed-denial-main-{0}-gate" -f $runStamp
        $recoveryRunLabel = "verify-resumed-denial-main-{0}-recovery" -f $runStamp
        $happyScenarioModes = Get-ScenarioModes -DefaultMode "happy"
        $gateScenarioModes = New-GateScenarioModes

        Write-Heading "Runs"
        $happyResult = Invoke-HlhostRun -RunName "happy" -RunLabel $happyRunLabel -ScenarioModes $happyScenarioModes
        $runResults.Add($happyResult) | Out-Null
        $scenarioStatuses.happy = $happyResult.Status
        Write-Host ("happy: {0}" -f $happyResult.Status)
        if ($happyResult.SummaryPath) {
            Write-Host ("  summary log: {0}" -f $happyResult.SummaryPath)
        }
        if (-not [string]::IsNullOrWhiteSpace($happyResult.ProvenanceMode)) {
            Write-Host ("  provenance: {0}" -f $happyResult.ProvenanceMode)
            Write-Host ("  provenance detail: {0}" -f $happyResult.ProvenanceDetail)
        }
        if ($happyResult.BenignWarnings.Count -gt 0) {
            Write-Host ("  benign warnings: {0}" -f ($happyResult.BenignWarnings -join ", "))
        }

        $gateResult = Invoke-HlhostRun -RunName "gate" -RunLabel $gateRunLabel -ScenarioModes $gateScenarioModes
        $runResults.Add($gateResult) | Out-Null
        $scenarioStatuses.gate = $gateResult.Status
        Write-Host ("gate: {0}" -f $gateResult.Status)
        if ($gateResult.SummaryPath) {
            Write-Host ("  summary log: {0}" -f $gateResult.SummaryPath)
        }
        if (-not [string]::IsNullOrWhiteSpace($gateResult.ProvenanceMode)) {
            Write-Host ("  provenance: {0}" -f $gateResult.ProvenanceMode)
            Write-Host ("  provenance detail: {0}" -f $gateResult.ProvenanceDetail)
        }
        if ($gateResult.BenignWarnings.Count -gt 0) {
            Write-Host ("  benign warnings: {0}" -f ($gateResult.BenignWarnings -join ", "))
        }

        if (-not $SkipRecovery) {
            $recoveryResult = Invoke-HlhostRun -RunName "recovery" -RunLabel $recoveryRunLabel -ScenarioModes $happyScenarioModes
            $runResults.Add($recoveryResult) | Out-Null
            $scenarioStatuses.recovery = $recoveryResult.Status
            Write-Host ("recovery: {0}" -f $recoveryResult.Status)
            if ($recoveryResult.SummaryPath) {
                Write-Host ("  summary log: {0}" -f $recoveryResult.SummaryPath)
            }
            if (-not [string]::IsNullOrWhiteSpace($recoveryResult.ProvenanceMode)) {
                Write-Host ("  provenance: {0}" -f $recoveryResult.ProvenanceMode)
                Write-Host ("  provenance detail: {0}" -f $recoveryResult.ProvenanceDetail)
            }
            if ($recoveryResult.BenignWarnings.Count -gt 0) {
                Write-Host ("  benign warnings: {0}" -f ($recoveryResult.BenignWarnings -join ", "))
            }
        }

        Write-Heading "Log Discovery"
        foreach ($runResult in $runResults) {
            if (-not [string]::IsNullOrWhiteSpace($runResult.SummaryPath)) {
                Write-Host ("{0}: {1}" -f $runResult.RunName, $runResult.SummaryPath)
            }
        }

        Write-Heading "Verdict"
        foreach ($entry in $scenarioStatuses.GetEnumerator()) {
            Write-Host ("{0}: {1}" -f $entry.Key, $entry.Value)
        }

        if ($NoExecute) {
            Write-Host "Final overall verdict: NOEXECUTE"
            exit 0
        }

        Write-Host "Final overall verdict: PASS"
        exit 0
    }
    finally {
        Pop-Location
    }
}
catch {
    if ($scenarioStatuses.happy -eq "PENDING") {
        $scenarioStatuses.happy = "FAIL"
    }
    elseif ($scenarioStatuses.gate -eq "PENDING") {
        $scenarioStatuses.gate = "FAIL"
    }
    elseif (-not $SkipRecovery -and $scenarioStatuses.recovery -eq "PENDING") {
        $scenarioStatuses.recovery = "FAIL"
    }

    Write-Heading "Failure"
    Write-Host $_.Exception.Message
    Write-Heading "Verdict"
    foreach ($entry in $scenarioStatuses.GetEnumerator()) {
        Write-Host ("{0}: {1}" -f $entry.Key, $entry.Value)
    }
    Write-Host "Final overall verdict: FAIL"
    exit 1
}
