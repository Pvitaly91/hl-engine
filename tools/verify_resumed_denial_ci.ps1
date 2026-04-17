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
    [switch]$UseExistingBinary
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
if (Test-Path Variable:PSNativeCommandUseErrorActionPreference) {
    $PSNativeCommandUseErrorActionPreference = $false
}

$script:PromptId = "HL-CL-20260411-163-dedicated-goldsrc-signon-carried-checkpoint-claimed-checkpoint-resumed-denial-surface"
$script:DefaultBuildDirName = "build-main-win32-hlhost-regression"
$script:MainRunnerPath = Join-Path $PSScriptRoot "verify_resumed_denial_main.ps1"

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
        [string]$VerificationCommandLine
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
}

$resolvedRepoRoot = ""
$resolvedOuterWorkspaceRoot = ""
$workspaceSource = ""
$resolvedBuildDir = ""
$resolvedExecutablePath = ""
$verificationCommandLine = ""
$resultLabel = "FAIL"
$finalExitCode = 1

try {
    $resolvedRepoRoot = Get-RepoRootFromRequest -RequestedRoot $RepoRoot
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

    & $hostExecutable @($verificationArguments.ToArray())
    $finalExitCode = $LASTEXITCODE
    if ($finalExitCode -ne 0) {
        throw ("verify_resumed_denial_main.ps1 failed with exit code {0}." -f $finalExitCode)
    }

    $resultLabel = if ($NoExecute) { "NOEXECUTE" } else { "PASS" }
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
    Write-CiSummary `
        -Result $resultLabel `
        -ExitCode $finalExitCode `
        -ProvenanceMode $ProvenanceMode `
        -ResolvedRepoRoot $resolvedRepoRoot `
        -ResolvedWorkspaceRoot $resolvedOuterWorkspaceRoot `
        -WorkspaceSource $workspaceSource `
        -ResolvedBuildDir $resolvedBuildDir `
        -ResolvedExecutablePath $resolvedExecutablePath `
        -VerificationCommandLine $verificationCommandLine
}

exit $finalExitCode
