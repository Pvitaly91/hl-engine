param(
    [switch]$CheckoutReviewedCommit,
    [switch]$SkipRecovery,
    [switch]$LeaveDetached,
    [switch]$NoExecute,
    [string]$RepoRoot,
    [string]$ReviewedCommit = "148e27743e7a3dd3dc5aed12faf854405b8bbb08",
    [string]$PreChangeCommit = "673be202082f057533585a8fdb866cfb92542f56"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
if (Test-Path Variable:PSNativeCommandUseErrorActionPreference) {
    $PSNativeCommandUseErrorActionPreference = $false
}

$script:ReviewedBranch = "codex/HL-CL-20260401-081-target-runtime-completion-state"
$script:PromptId = "HL-CL-20260411-163-dedicated-goldsrc-signon-carried-checkpoint-claimed-checkpoint-resumed-denial-surface"
$script:RunTimeoutSeconds = 300
$script:RuntimeLogWaitSeconds = 20
$script:RelayEnvironmentVariable = "HLHOST_MANUAL_VERIFY_RELAY"
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

function Write-Heading {
    param([string]$Text)

    Write-Host ""
    Write-Host ("== {0} ==" -f $Text)
}

function Resolve-FullPath {
    param([string]$PathValue)

    if ([string]::IsNullOrWhiteSpace($PathValue)) {
        return [System.IO.Path]::GetFullPath((Get-Location).Path)
    }

    return [System.IO.Path]::GetFullPath($PathValue)
}

function Normalize-RelativePath {
    param([string]$PathValue)

    return ($PathValue -replace "\\", "/")
}

function Get-GitSingleLine {
    param([string[]]$Arguments)

    $output = & git @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw ("git {0} failed with exit code {1}." -f ($Arguments -join " "), $LASTEXITCODE)
    }

    return ($output | Select-Object -First 1).Trim()
}

function Invoke-GitQuiet {
    param([string[]]$Arguments)

    & git @Arguments | Out-Null
    if ($LASTEXITCODE -ne 0) {
        throw ("git {0} failed with exit code {1}." -f ($Arguments -join " "), $LASTEXITCODE)
    }
}

function Test-GitQuiet {
    param([string[]]$Arguments)

    & git @Arguments | Out-Null
    return ($LASTEXITCODE -eq 0)
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

function Assert-CommitExists {
    param(
        [string]$Commit,
        [string]$Description
    )

    if (-not (Test-GitQuiet @("cat-file", "-e", ("{0}^{{commit}}" -f $Commit)))) {
        throw ("Missing {0} commit: {1}" -f $Description, $Commit)
    }
}

function Assert-ReviewedBranchExists {
    if (-not (Test-GitQuiet @("show-ref", "--verify", "--quiet", ("refs/heads/{0}" -f $script:ReviewedBranch)))) {
        throw ("Missing reviewed branch: {0}" -f $script:ReviewedBranch)
    }
}

function Assert-PreChangeCommitIsAncestor {
    param(
        [string]$AncestorCommit,
        [string]$DescendantCommit
    )

    & git merge-base --is-ancestor $AncestorCommit $DescendantCommit | Out-Null
    if ($LASTEXITCODE -ne 0) {
        throw ("Expected pre-change commit {0} to be an ancestor of reviewed commit {1}." -f $AncestorCommit, $DescendantCommit)
    }
}

function Get-BranchTipCommit {
    param([string]$BranchName)

    return Get-GitSingleLine @("rev-parse", $BranchName)
}

function Get-DirtyTrackedPaths {
    $lines = & git status --porcelain=v1 --untracked-files=no
    if ($LASTEXITCODE -ne 0) {
        throw "Unable to inspect tracked worktree state."
    }

    $paths = New-Object System.Collections.Generic.List[string]
    foreach ($line in $lines) {
        if ([string]::IsNullOrWhiteSpace($line) -or $line.Length -lt 4) {
            continue
        }

        $pathText = $line.Substring(3)
        if ($pathText -like "* -> *") {
            $pathText = ($pathText -split " -> ", 2)[1]
        }

        $paths.Add((Normalize-RelativePath $pathText))
    }

    return @($paths | Sort-Object -Unique)
}

function Get-PathsChangedBetweenRefs {
    param(
        [string]$RefA,
        [string]$RefB
    )

    $lines = & git diff --name-only --relative $RefA $RefB
    if ($LASTEXITCODE -ne 0) {
        throw ("Unable to diff {0} against {1}." -f $RefA, $RefB)
    }

    return @($lines | Where-Object { -not [string]::IsNullOrWhiteSpace($_) } | ForEach-Object { Normalize-RelativePath $_ } | Sort-Object -Unique)
}

function Assert-CheckoutIsSafe {
    param([string]$TargetCommit)

    $dirtyTrackedPaths = @(Get-DirtyTrackedPaths)
    if ($dirtyTrackedPaths.Count -eq 0) {
        return
    }

    $changedPaths = @(Get-PathsChangedBetweenRefs -RefA "HEAD" -RefB $TargetCommit)
    $blockingPaths = @($dirtyTrackedPaths | Where-Object { $changedPaths -contains $_ })
    if ($blockingPaths.Count -gt 0) {
        throw ("Refusing detached checkout because tracked local changes overlap files that differ at {0}: {1}. Use a clean worktree or auxiliary worktree for -CheckoutReviewedCommit." -f $TargetCommit, ($blockingPaths -join ", "))
    }
}

function Restore-CheckoutState {
    param($State)

    if ($null -eq $State) {
        return
    }

    if ($State.IsDetached) {
        Invoke-GitQuiet @("switch", "--detach", $State.HeadCommit)
        return
    }

    Invoke-GitQuiet @("switch", $State.BranchName)
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

    $summaryFile = Get-NewestGeneratedFile -DirectoryPath $script:RuntimeLogDir -Filter ("*__{0}_summary.log" -f $RunLabel) -StartedAt $StartedAt
    $runtimeFiles = @(
        Get-ChildItem -LiteralPath $script:RuntimeLogDir -Filter ("*__{0}_part*.log" -f $RunLabel) -File -ErrorAction SilentlyContinue |
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
        ("gitCommit={0}" -f $ReviewedCommit),
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

function Assert-ExecutableSupportsReviewedRecipe {
    $helpLines = & $script:ResolvedExecutablePath --help 2>&1
    if ($LASTEXITCODE -ne 0) {
        throw ("Unable to inspect {0} with --help (exit code {1})." -f $script:ResolvedExecutablePath, $LASTEXITCODE)
    }

    $helpText = $helpLines -join "`n"
    $requiredOptions = @(
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
    $missingOptions = @($requiredOptions | Where-Object { $helpText.IndexOf($_, [System.StringComparison]::Ordinal) -lt 0 })
    if ($missingOptions.Count -gt 0) {
        throw ("{0} does not expose the reviewed runtime options required by this recipe: {1}. Rebuild the reviewed hlhost.exe before running full verification." -f $script:ResolvedExecutablePath, ($missingOptions -join ", "))
    }
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
        }
    }

    $startedAt = Get-Date
    $process = Start-Process -FilePath $script:ResolvedExecutablePath -ArgumentList $arguments -NoNewWindow -PassThru
    if (-not $process.WaitForExit($script:RunTimeoutSeconds * 1000)) {
        Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
        throw ("{0} hung beyond {1} seconds and was terminated." -f $RunName, $script:RunTimeoutSeconds)
    }

    $exitCode = $process.ExitCode
    if ($null -eq $exitCode) {
        throw ("{0} exited unexpectedly before an exit code was available." -f $RunName)
    }
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
    Assert-CodexRunIdentity -RunName $RunName -SummaryLines $summaryLines -RunLabel $RunLabel

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
    }
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

    throw "Unable to locate the current PowerShell host executable for relay."
}

function Invoke-RelayedSelf {
    param(
        [string]$ResolvedRepoRoot,
        [string]$OriginalHeadCommit
    )

    if ([string]::IsNullOrWhiteSpace($PSCommandPath) -or -not (Test-Path -LiteralPath $PSCommandPath -PathType Leaf)) {
        throw "Unable to relay because the current script path is not available."
    }

    Write-Heading "Relay"
    Write-Host ("Current HEAD {0} is not the reviewed commit {1}; relaunching from a temporary copy before detached checkout." -f $OriginalHeadCommit, $ReviewedCommit)

    $tempScriptPath = Join-Path ([System.IO.Path]::GetTempPath()) ("manual_verify_resumed_denial_{0}.ps1" -f [guid]::NewGuid().ToString("N"))
    Copy-Item -LiteralPath $PSCommandPath -Destination $tempScriptPath -Force
    $hostExecutable = Get-CurrentHostExecutable
    $relayArguments = New-Object System.Collections.Generic.List[string]
    $relayArguments.Add("-NoLogo")
    $relayArguments.Add("-NoProfile")
    $relayArguments.Add("-ExecutionPolicy")
    $relayArguments.Add("Bypass")
    $relayArguments.Add("-File")
    $relayArguments.Add($tempScriptPath)
    $relayArguments.Add("-RepoRoot")
    $relayArguments.Add($ResolvedRepoRoot)
    $relayArguments.Add("-ReviewedCommit")
    $relayArguments.Add($ReviewedCommit)
    $relayArguments.Add("-PreChangeCommit")
    $relayArguments.Add($PreChangeCommit)
    if ($CheckoutReviewedCommit) {
        $relayArguments.Add("-CheckoutReviewedCommit")
    }
    if ($SkipRecovery) {
        $relayArguments.Add("-SkipRecovery")
    }
    if ($LeaveDetached) {
        $relayArguments.Add("-LeaveDetached")
    }
    if ($NoExecute) {
        $relayArguments.Add("-NoExecute")
    }

    $previousRelayValue = [System.Environment]::GetEnvironmentVariable($script:RelayEnvironmentVariable)
    try {
        [System.Environment]::SetEnvironmentVariable($script:RelayEnvironmentVariable, "1")
        $relayProcess = Start-Process -FilePath $hostExecutable -ArgumentList $relayArguments -NoNewWindow -PassThru -Wait
        exit $relayProcess.ExitCode
    }
    finally {
        if ($null -eq $previousRelayValue) {
            [System.Environment]::SetEnvironmentVariable($script:RelayEnvironmentVariable, $null)
        }
        else {
            [System.Environment]::SetEnvironmentVariable($script:RelayEnvironmentVariable, $previousRelayValue)
        }

        Remove-Item -LiteralPath $tempScriptPath -Force -ErrorAction SilentlyContinue
    }
}

$resolvedRepoRoot = $null
$originalState = $null
$didCheckoutReviewedCommit = $false
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
        Assert-ReviewedBranchExists
        Assert-CommitExists -Commit $PreChangeCommit -Description "pre-change"
        Assert-CommitExists -Commit $ReviewedCommit -Description "reviewed"
        Assert-PreChangeCommitIsAncestor -AncestorCommit $PreChangeCommit -DescendantCommit $ReviewedCommit

        $originalState = Get-CheckoutState
        $reviewedBranchTip = Get-BranchTipCommit -BranchName $script:ReviewedBranch

        if (
            $CheckoutReviewedCommit -and
            $originalState.HeadCommit -ne $ReviewedCommit -and
            [string]::IsNullOrWhiteSpace([System.Environment]::GetEnvironmentVariable($script:RelayEnvironmentVariable))
        ) {
            Invoke-RelayedSelf -ResolvedRepoRoot $resolvedRepoRoot -OriginalHeadCommit $originalState.HeadCommit
        }

        Write-Heading "Repository"
        Write-Host ("Repo root: {0}" -f $resolvedRepoRoot)
        Write-Host ("Reviewed branch: {0}" -f $script:ReviewedBranch)
        Write-Host ("Reviewed branch tip: {0}" -f $reviewedBranchTip)
        Write-Host ("Original HEAD: {0}" -f $originalState.HeadCommit)
        Write-Host ("Original branch state: {0}" -f $(if ($originalState.IsDetached) { "detached" } else { $originalState.BranchName }))
        Write-Host ("Reviewed commit: {0}" -f $ReviewedCommit)
        Write-Host ("Pre-change commit: {0}" -f $PreChangeCommit)

        if ($CheckoutReviewedCommit -and $originalState.HeadCommit -ne $ReviewedCommit) {
            Assert-CheckoutIsSafe -TargetCommit $ReviewedCommit
            Write-Heading "Checkout"
            Write-Host ("Detaching to reviewed commit: {0}" -f $ReviewedCommit)
            Invoke-GitQuiet @("switch", "--detach", $ReviewedCommit)
            $didCheckoutReviewedCommit = $true
        }

        $postCheckoutState = Get-CheckoutState
        Write-Host ("Post-checkout HEAD: {0}" -f $postCheckoutState.HeadCommit)
        Write-Host ("Checked out reviewed commit: {0}" -f $(if ($didCheckoutReviewedCommit) { "yes" } else { "no" }))

        if ($postCheckoutState.HeadCommit -ne $ReviewedCommit) {
            throw ("Refusing to validate current HEAD {0}. The reviewed target is {1}. Pass -CheckoutReviewedCommit or manually checkout the reviewed commit first." -f $postCheckoutState.HeadCommit, $ReviewedCommit)
        }

        $script:ResolvedExecutablePath = (Resolve-Path "build32/host/Debug/hlhost.exe").Path
        $script:ResolvedGameDir = (Resolve-Path "logs/latest/HL-CL-20260411-163-dedicated-goldsrc-signon-carried-checkpoint-claimed-checkpoint-resumed-denial-surface/runtime/valve-fixture").Path
        $script:RuntimeLogDir = (Resolve-Path "logs/latest/runtime").Path
        Write-Heading "Inputs"
        Write-Host ("Executable: {0}" -f $script:ResolvedExecutablePath)
        Write-Host ("Game dir: {0}" -f $script:ResolvedGameDir)
        Write-Host ("Prompt ID: {0}" -f $script:PromptId)
        if (-not $NoExecute) {
            Write-Heading "Executable Preflight"
            Assert-ExecutableSupportsReviewedRecipe
            Write-Host "Reviewed runtime options: present"
        }

        $runStamp = Get-Date -Format "yyyyMMdd_HHmmss"
        $happyRunLabel = "manual-resumed-denial-{0}-happy" -f $runStamp
        $gateRunLabel = "manual-resumed-denial-{0}-gate" -f $runStamp
        $recoveryRunLabel = "manual-resumed-denial-{0}-recovery" -f $runStamp
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
        if ($CheckoutReviewedCommit -and $didCheckoutReviewedCommit -and -not $LeaveDetached -and $null -ne $originalState) {
            Write-Heading "Restore"
            Write-Host ("Restoring original checkout state: {0}" -f $(if ($originalState.IsDetached) { $originalState.HeadCommit } else { $originalState.BranchName }))
            Restore-CheckoutState -State $originalState
        }

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
