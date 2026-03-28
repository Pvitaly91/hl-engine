param(
    [switch]$ProbeOnly,
    [string]$ValveDir,
    [string]$WorkspaceRoot,
    [string]$CMakePath,
    [string]$ConfigurePreset = "vs2022-win32",
    [string]$BuildPreset = "vs2022-debug",
    [string]$Target = "changelevel_policy_regressions",
    [string]$GitHubOutputPath,
    [string]$GitHubSummaryPath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
if (Test-Path Variable:PSNativeCommandUseErrorActionPreference) {
    $PSNativeCommandUseErrorActionPreference = $false
}

function Get-FullPathOrEmpty {
    param([string]$PathValue)

    if ([string]::IsNullOrWhiteSpace($PathValue)) {
        return ""
    }

    return [System.IO.Path]::GetFullPath($PathValue)
}

function Write-GitHubOutputValue {
    param(
        [string]$Name,
        [string]$Value
    )

    if ([string]::IsNullOrWhiteSpace($script:ResolvedGitHubOutputPath)) {
        return
    }

    Add-Content -LiteralPath $script:ResolvedGitHubOutputPath -Value ("{0}={1}" -f $Name, $Value)
}

function Append-GitHubSummary {
    param([string[]]$Lines)

    if ([string]::IsNullOrWhiteSpace($script:ResolvedGitHubSummaryPath)) {
        return
    }

    Add-Content -LiteralPath $script:ResolvedGitHubSummaryPath -Value $Lines
}

function Resolve-CMakeExecutable {
    param([string]$RequestedCMakePath)

    if (-not [string]::IsNullOrWhiteSpace($RequestedCMakePath)) {
        $resolvedRequestedPath = Get-FullPathOrEmpty $RequestedCMakePath
        if (-not (Test-Path -LiteralPath $resolvedRequestedPath -PathType Leaf)) {
            throw ("Requested cmake executable not found: {0}" -f $resolvedRequestedPath)
        }

        return $resolvedRequestedPath
    }

    $command = Get-Command cmake.exe -ErrorAction SilentlyContinue
    if ($null -ne $command -and -not [string]::IsNullOrWhiteSpace($command.Source)) {
        return $command.Source
    }

    $fallbackCandidates = @(
        "C:\Program Files\CMake\bin\cmake.exe",
        "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
        "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
        "C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
        "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
    )

    foreach ($candidate in $fallbackCandidates) {
        if (Test-Path -LiteralPath $candidate -PathType Leaf) {
            return $candidate
        }
    }

    throw "cmake.exe was not found in PATH or standard Visual Studio/CMake locations."
}

function Resolve-HLengineWorkspaceRoot {
    param(
        [string]$RequestedWorkspaceRoot,
        [string]$StartingDirectory
    )

    if (-not [string]::IsNullOrWhiteSpace($RequestedWorkspaceRoot)) {
        return [pscustomobject]@{
            Source = "explicit"
            Root = [System.IO.Path]::GetFullPath($RequestedWorkspaceRoot)
        }
    }

    $current = [System.IO.Path]::GetFullPath($StartingDirectory)
    for ($depth = 0; $depth -lt 6; ++$depth) {
        $presetPath = Join-Path $current "CMakePresets.json"
        $hostCMakePath = Join-Path $current "host\CMakeLists.txt"
        $hasPreset = Test-Path -LiteralPath $presetPath -PathType Leaf
        $hasHostCMake = Test-Path -LiteralPath $hostCMakePath -PathType Leaf
        if ($hasPreset -and $hasHostCMake) {
            return [pscustomobject]@{
                Source = "auto-discovered"
                Root = $current
            }
        }

        $parent = Split-Path -Path $current -Parent
        if ([string]::IsNullOrWhiteSpace($parent) -or $parent -eq $current) {
            break
        }

        $current = $parent
    }

    $defaultParent = Split-Path -Path ([System.IO.Path]::GetFullPath($StartingDirectory)) -Parent
    if (-not [string]::IsNullOrWhiteSpace($defaultParent)) {
        return [pscustomobject]@{
            Source = "default-parent"
            Root = [System.IO.Path]::GetFullPath($defaultParent)
        }
    }

    return [pscustomobject]@{
        Source = "missing"
        Root = ""
    }
}

function Test-WorkspaceLayout {
    param(
        [string]$RequestedWorkspaceRoot,
        [string]$RepositoryRoot
    )

    $workspaceResolution = Resolve-HLengineWorkspaceRoot `
        -RequestedWorkspaceRoot $RequestedWorkspaceRoot `
        -StartingDirectory $RepositoryRoot

    $checks = New-Object System.Collections.Generic.List[string]
    $missing = New-Object System.Collections.Generic.List[string]

    if ([string]::IsNullOrWhiteSpace($workspaceResolution.Root)) {
        $checks.Add("workspace root: missing")
        return [pscustomobject]@{
            Runnable = $false
            ResolvedWorkspaceRoot = ""
            WorkspaceSource = $workspaceResolution.Source
            Reason = "Workspace layout unavailable: HLengine workspace root was not found. Set HLENGINE_WORKSPACE_ROOT or provision the surrounding workspace layout."
            Checks = $checks.ToArray()
        }
    }

    $resolvedWorkspaceRoot = $workspaceResolution.Root
    $workspaceExists = Test-Path -LiteralPath $resolvedWorkspaceRoot -PathType Container
    $workspaceLabel = if ($workspaceExists) { "found" } else { "missing" }
    $checks.Add(
        ("workspace root: {0} (`{1}`, source={2})" -f $workspaceLabel, $resolvedWorkspaceRoot, $workspaceResolution.Source))
    if (-not $workspaceExists) {
        $missing.Add("workspace root")
    }

    $presetPath = Join-Path $resolvedWorkspaceRoot "CMakePresets.json"
    $presetExists = Test-Path -LiteralPath $presetPath -PathType Leaf
    $presetLabel = if ($presetExists) { "found" } else { "missing" }
    $checks.Add(("CMakePresets.json: {0} (`{1}`)" -f $presetLabel, $presetPath))
    if (-not $presetExists) {
        $missing.Add("CMakePresets.json")
    }

    $thirdPartyPath = Join-Path $resolvedWorkspaceRoot "third_party"
    $thirdPartyExists = Test-Path -LiteralPath $thirdPartyPath -PathType Container
    $thirdPartyLabel = if ($thirdPartyExists) { "found" } else { "missing" }
    $checks.Add(("third_party/: {0} (`{1}`)" -f $thirdPartyLabel, $thirdPartyPath))
    if (-not $thirdPartyExists) {
        $missing.Add("third_party/")
    }

    $hostCMakePath = Join-Path $resolvedWorkspaceRoot "host\CMakeLists.txt"
    $hostCMakeExists = Test-Path -LiteralPath $hostCMakePath -PathType Leaf
    $hostCMakeLabel = if ($hostCMakeExists) { "found" } else { "missing" }
    $checks.Add(("host/CMakeLists.txt: {0} (`{1}`)" -f $hostCMakeLabel, $hostCMakePath))
    if (-not $hostCMakeExists) {
        $missing.Add("host/CMakeLists.txt")
    }

    $reason = if ($missing.Count -eq 0) {
        "Workspace layout is available."
    } else {
        "Workspace layout unavailable: missing " + ($missing -join ", ") + "."
    }

    return [pscustomobject]@{
        Runnable = ($missing.Count -eq 0)
        ResolvedWorkspaceRoot = $resolvedWorkspaceRoot
        WorkspaceSource = $workspaceResolution.Source
        Reason = $reason
        Checks = $checks.ToArray()
    }
}

function Test-ChangelevelPolicyEnvironment {
    param(
        [string]$RequestedValveDir,
        [string]$RequestedWorkspaceRoot,
        [string]$RepositoryRoot
    )

    $resolvedValveDir = Get-FullPathOrEmpty $RequestedValveDir
    if ([string]::IsNullOrWhiteSpace($resolvedValveDir)) {
        $resolvedValveDir = Get-FullPathOrEmpty $env:HLENGINE_VALVE_DIR
    }

    $checks = New-Object System.Collections.Generic.List[string]
    $missing = New-Object System.Collections.Generic.List[string]

    if ([string]::IsNullOrWhiteSpace($resolvedValveDir)) {
        $checks.Add("HLENGINE_VALVE_DIR: missing")
        return [pscustomobject]@{
            Runnable = $false
            FailureKind = "assets"
            ResolvedValveDir = ""
            ResolvedWorkspaceRoot = ""
            WorkspaceSource = ""
            Reason = "HLENGINE_VALVE_DIR is not set on this runner."
            Checks = $checks.ToArray()
        }
    }

    $valveDirectoryExists = Test-Path -LiteralPath $resolvedValveDir -PathType Container
    $valveDirectoryLabel = if ($valveDirectoryExists) { "found" } else { "missing" }
    $checks.Add(
        ("valve directory: {0} (`{1}`)" -f $valveDirectoryLabel, $resolvedValveDir))
    if (-not $valveDirectoryExists) {
        $missing.Add("valve directory")
    }

    $hlDllPath = Join-Path $resolvedValveDir "dlls\hl.dll"
    $hlDllExists = Test-Path -LiteralPath $hlDllPath -PathType Leaf
    $hlDllLabel = if ($hlDllExists) { "found" } else { "missing" }
    $checks.Add(("hl.dll: {0} (`{1}`)" -f $hlDllLabel, $hlDllPath))
    if (-not $hlDllExists) {
        $missing.Add("dlls/hl.dll")
    }

    $clientCandidates = @(
        (Join-Path $resolvedValveDir "cl_dlls\client.dll"),
        (Join-Path $resolvedValveDir "dlls\client.dll")
    )
    $resolvedClientDll = ""
    foreach ($candidate in $clientCandidates) {
        if (Test-Path -LiteralPath $candidate -PathType Leaf) {
            $resolvedClientDll = $candidate
            break
        }
    }

    if ([string]::IsNullOrWhiteSpace($resolvedClientDll)) {
        $checks.Add(
            ("client.dll: missing (checked `{0}`, fallback `{1}`)" -f $clientCandidates[0], $clientCandidates[1]))
        $missing.Add("client.dll")
    } else {
        $clientLabel = if ($resolvedClientDll -eq $clientCandidates[0]) {
            "found"
        } else {
            "found via fallback"
        }
        $checks.Add(("client.dll: {0} (`{1}`)" -f $clientLabel, $resolvedClientDll))
    }

    $bootstrapMapPath = Join-Path $resolvedValveDir "maps\c0a0.bsp"
    $bootstrapMapExists = Test-Path -LiteralPath $bootstrapMapPath -PathType Leaf
    $bootstrapMapLabel = if ($bootstrapMapExists) { "found" } else { "missing" }
    $checks.Add(
        ("maps/c0a0.bsp: {0} (`{1}`)" -f $bootstrapMapLabel, $bootstrapMapPath))
    if (-not $bootstrapMapExists) {
        $missing.Add("maps/c0a0.bsp")
    }

    $assetReason = if ($missing.Count -eq 0) {
        "Licensed Half-Life valve assets are available."
    } else {
        "Licensed Half-Life valve assets unavailable: missing " + ($missing -join ", ") + "."
    }

    if ($missing.Count -ne 0) {
        return [pscustomobject]@{
            Runnable = $false
            FailureKind = "assets"
            ResolvedValveDir = $resolvedValveDir
            ResolvedWorkspaceRoot = ""
            WorkspaceSource = ""
            Reason = $assetReason
            Checks = $checks.ToArray()
        }
    }

    $workspaceStatus = Test-WorkspaceLayout `
        -RequestedWorkspaceRoot $RequestedWorkspaceRoot `
        -RepositoryRoot $RepositoryRoot
    foreach ($workspaceCheck in $workspaceStatus.Checks) {
        $checks.Add($workspaceCheck)
    }

    if (-not $workspaceStatus.Runnable) {
        return [pscustomobject]@{
            Runnable = $false
            FailureKind = "workspace-layout"
            ResolvedValveDir = $resolvedValveDir
            ResolvedWorkspaceRoot = $workspaceStatus.ResolvedWorkspaceRoot
            WorkspaceSource = $workspaceStatus.WorkspaceSource
            Reason = $workspaceStatus.Reason
            Checks = $checks.ToArray()
        }
    }

    return [pscustomobject]@{
        Runnable = $true
        FailureKind = ""
        ResolvedValveDir = $resolvedValveDir
        ResolvedWorkspaceRoot = $workspaceStatus.ResolvedWorkspaceRoot
        WorkspaceSource = $workspaceStatus.WorkspaceSource
        Reason = "Licensed Half-Life valve assets and workspace layout are available."
        Checks = $checks.ToArray()
    }
}

function Publish-EnvironmentStatus {
    param([pscustomobject]$EnvironmentStatus)

    $runnableText = if ($EnvironmentStatus.Runnable) { "true" } else { "false" }
    $summaryRunnableText = if ($EnvironmentStatus.Runnable) { "yes" } else { "no" }
    $summaryValveDir = if ([string]::IsNullOrWhiteSpace($EnvironmentStatus.ResolvedValveDir)) {
        "<unset>"
    } else {
        $EnvironmentStatus.ResolvedValveDir
    }
    $summaryWorkspaceRoot = if ([string]::IsNullOrWhiteSpace($EnvironmentStatus.ResolvedWorkspaceRoot)) {
        "<unset>"
    } else {
        $EnvironmentStatus.ResolvedWorkspaceRoot
    }

    Write-GitHubOutputValue -Name "runnable" -Value $runnableText
    Write-GitHubOutputValue -Name "reason" -Value $EnvironmentStatus.Reason
    Write-GitHubOutputValue -Name "valve_dir" -Value $EnvironmentStatus.ResolvedValveDir
    Write-GitHubOutputValue -Name "workspace_root" -Value $EnvironmentStatus.ResolvedWorkspaceRoot
    Write-GitHubOutputValue -Name "failure_kind" -Value $EnvironmentStatus.FailureKind

    $summaryLines = @(
        "## Changelevel Policy Self-Hosted",
        "",
        ('- runnable: ' + $summaryRunnableText),
        ('- failure kind: ' + $(if ([string]::IsNullOrWhiteSpace($EnvironmentStatus.FailureKind)) { "<none>" } else { $EnvironmentStatus.FailureKind })),
        ('- valve dir: `' + $summaryValveDir + '`'),
        ('- workspace root: `' + $summaryWorkspaceRoot + '`'),
        ('- reason: ' + $EnvironmentStatus.Reason),
        "- checks:"
    )

    foreach ($check in $EnvironmentStatus.Checks) {
        $summaryLines += ('  - ' + $check)
    }
    $summaryLines += ""

    Append-GitHubSummary -Lines $summaryLines

    if ($EnvironmentStatus.Runnable) {
        Write-Host ("Environment ready: {0}" -f $EnvironmentStatus.ResolvedValveDir)
        return
    }

    Write-Host ("Changelevel policy self-hosted run not runnable: {0}" -f $EnvironmentStatus.Reason)
    if ($env:GITHUB_ACTIONS -eq "true") {
        Write-Host ("::warning title=Changelevel policy regressions skipped::{0}" -f $EnvironmentStatus.Reason)
    }
}

$script:ResolvedGitHubOutputPath = Get-FullPathOrEmpty $GitHubOutputPath
$script:ResolvedGitHubSummaryPath = Get-FullPathOrEmpty $GitHubSummaryPath
$repositoryRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$requestedWorkspaceRoot = Get-FullPathOrEmpty $WorkspaceRoot
if ([string]::IsNullOrWhiteSpace($requestedWorkspaceRoot)) {
    $requestedWorkspaceRoot = Get-FullPathOrEmpty $env:HLENGINE_WORKSPACE_ROOT
}
$environmentStatus = Test-ChangelevelPolicyEnvironment `
    -RequestedValveDir $ValveDir `
    -RequestedWorkspaceRoot $requestedWorkspaceRoot `
    -RepositoryRoot $repositoryRoot

Publish-EnvironmentStatus -EnvironmentStatus $environmentStatus

if ($ProbeOnly) {
    exit 0
}

if (-not $environmentStatus.Runnable) {
    throw ("Changelevel policy self-hosted run is not runnable: {0}" -f $environmentStatus.Reason)
}

$env:HLENGINE_VALVE_DIR = $environmentStatus.ResolvedValveDir
$cmakeExecutable = Resolve-CMakeExecutable -RequestedCMakePath $CMakePath
$workspaceRoot = $environmentStatus.ResolvedWorkspaceRoot

Push-Location $workspaceRoot
try {
    Write-Host ("Using cmake executable: {0}" -f $cmakeExecutable)
    Write-Host ("Using workspace root: {0}" -f $workspaceRoot)
    Write-Host ("==> cmake --preset {0}" -f $ConfigurePreset)
    & $cmakeExecutable --preset $ConfigurePreset
    if ($LASTEXITCODE -ne 0) {
        throw ("cmake --preset {0} failed with exit code {1}." -f $ConfigurePreset, $LASTEXITCODE)
    }

    Write-Host ("==> cmake --build --preset {0} --target {1}" -f $BuildPreset, $Target)
    & $cmakeExecutable --build --preset $BuildPreset --target $Target
    if ($LASTEXITCODE -ne 0) {
        throw (
            "cmake --build --preset {0} --target {1} failed with exit code {2}." -f
                $BuildPreset,
                $Target,
                $LASTEXITCODE)
    }
}
finally {
    Pop-Location
}

Append-GitHubSummary -Lines @(
    "## Changelevel Policy Self-Hosted Result",
    "",
    ('- command: `cmake --build --preset ' + $BuildPreset + ' --target ' + $Target + '`'),
    ('- valve dir: `' + $environmentStatus.ResolvedValveDir + '`'),
    ('- workspace root: `' + $environmentStatus.ResolvedWorkspaceRoot + '`'),
    "- result: passed",
    ""
)

Write-Host "Accepted changelevel policy self-hosted run passed."
exit 0
