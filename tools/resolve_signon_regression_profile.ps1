param(
    [string]$RepoRoot,
    [string]$DiffBase,
    [string]$DiffHead,
    [string[]]$ChangedPath,
    [switch]$PrintChangedFiles
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
if (Test-Path Variable:PSNativeCommandUseErrorActionPreference) {
    $PSNativeCommandUseErrorActionPreference = $false
}

$script:HighRiskRuntimePaths = @(
    "include/app/launch_options.h",
    "src/app/launch_options.cpp",
    "src/app/host_application.cpp",
    "include/game_api/hl_server_module.h",
    "src/game_api/hl_server_module.cpp"
)

function Write-Heading {
    param([string]$Text)

    Write-Host ""
    Write-Host ("== {0} ==" -f $Text)
}

function Resolve-ResolverRepoRoot {
    param([string]$RequestedRoot)

    if ([string]::IsNullOrWhiteSpace($RequestedRoot)) {
        throw "-RepoRoot is required."
    }

    try {
        return [System.IO.Path]::GetFullPath((Resolve-Path -LiteralPath $RequestedRoot).Path)
    }
    catch {
        throw ("Repo root does not exist: {0}" -f $RequestedRoot)
    }
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

function Invoke-GitInRepo {
    param(
        [string]$ResolvedRepoRoot,
        [string[]]$Arguments
    )

    $output = & git -C $ResolvedRepoRoot @Arguments 2>&1
    $exitCode = if ($null -eq $LASTEXITCODE) { 0 } else { $LASTEXITCODE }
    if ($exitCode -ne 0) {
        $message = ($output | ForEach-Object { [string]$_ } | Where-Object { -not [string]::IsNullOrWhiteSpace($_) }) -join [Environment]::NewLine
        if ([string]::IsNullOrWhiteSpace($message)) {
            $message = "git returned exit code $exitCode."
        }

        throw ("git {0} failed in {1}: {2}" -f ($Arguments -join " "), $ResolvedRepoRoot, $message)
    }

    return @($output | ForEach-Object { [string]$_ })
}

function Normalize-ChangedPathValue {
    param(
        [string]$PathValue,
        [string]$ResolvedRepoRoot
    )

    if ([string]::IsNullOrWhiteSpace($PathValue)) {
        return ""
    }

    $normalizedPath = $PathValue.Trim()
    if ([System.IO.Path]::IsPathRooted($normalizedPath)) {
        $absolutePath = [System.IO.Path]::GetFullPath($normalizedPath)
        $absolutePathForCompare = $absolutePath.Replace('\', '/')
        $repoRootForCompare = ([System.IO.Path]::GetFullPath($ResolvedRepoRoot)).Replace('\', '/')
        $repoRootPrefix = $repoRootForCompare.TrimEnd('/') + '/'
        if ($absolutePathForCompare.StartsWith($repoRootPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
            $normalizedPath = $absolutePathForCompare.Substring($repoRootPrefix.Length)
        }
        else {
            $normalizedPath = $absolutePathForCompare
        }
    }

    $normalizedPath = $normalizedPath.Replace('\', '/')
    while ($normalizedPath.StartsWith("./", [System.StringComparison]::Ordinal)) {
        $normalizedPath = $normalizedPath.Substring(2)
    }
    while ($normalizedPath.StartsWith("/", [System.StringComparison]::Ordinal)) {
        $normalizedPath = $normalizedPath.Substring(1)
    }

    return ($normalizedPath -replace '/+', '/')
}

function Get-ChangedPathsFromGitDiff {
    param(
        [string]$ResolvedRepoRoot,
        [string]$DiffBase,
        [string]$DiffHead
    )

    $effectiveDiffBase = if ([string]::IsNullOrWhiteSpace($DiffBase)) { "origin/main" } else { $DiffBase.Trim() }
    $effectiveDiffHead = if ([string]::IsNullOrWhiteSpace($DiffHead)) { "HEAD" } else { $DiffHead.Trim() }
    $diffArguments = @(
        "diff",
        "--name-only",
        "--diff-filter=ACDMRTUXB",
        $effectiveDiffBase,
        $effectiveDiffHead,
        "--"
    )
    $diffOutput = Invoke-GitInRepo -ResolvedRepoRoot $ResolvedRepoRoot -Arguments $diffArguments

    return [pscustomobject]@{
        DiffBase = $effectiveDiffBase
        DiffHead = $effectiveDiffHead
        ChangedPaths = @(Get-TrimmedUniqueValues -Values $diffOutput)
    }
}

function Get-ProfileRank {
    param([string]$ProfileName)

    switch ($ProfileName) {
        "default" { return 0 }
        "checkpoint-extended" { return 1 }
        "full-expanded" { return 2 }
        default { return -1 }
    }
}

function New-RuleMatch {
    param(
        [string]$Profile,
        [string]$RuleId,
        [string]$Description,
        [string]$ChangedPath
    )

    return [ordered]@{
        profile = $Profile
        id = $RuleId
        description = $Description
        matchedPaths = @($ChangedPath)
    }
}

function Get-RuleMatchForChangedPath {
    param([string]$NormalizedPath)

    $lowerPath = $NormalizedPath.ToLowerInvariant()

    if ($script:HighRiskRuntimePaths -contains $lowerPath) {
        return (New-RuleMatch `
                -Profile "full-expanded" `
                -RuleId "full-expanded-high-risk-runtime-hotspots" `
                -Description "High-risk signon/runtime hotspot files under include/ or src/ changed." `
                -ChangedPath $NormalizedPath)
    }

    if ($lowerPath.StartsWith("include/") -or $lowerPath.StartsWith("src/")) {
        return (New-RuleMatch `
                -Profile "full-expanded" `
                -RuleId "full-expanded-include-src-runtime" `
                -Description "Runtime-adjacent source or header files under include/ or src/ changed." `
                -ChangedPath $NormalizedPath)
    }

    $fileName = [System.IO.Path]::GetFileName($lowerPath)
    if ($lowerPath -eq ".github/workflows/resumed-denial-regression.yml" -or
        $lowerPath -eq "tools/signon_regression_matrix.psd1" -or
        $lowerPath -eq "tools/resolve_signon_regression_profile.ps1" -or
        $fileName -like "verify_resumed_denial*.ps1" -or
        $fileName -like "verify_signon*.ps1") {
        return (New-RuleMatch `
                -Profile "checkpoint-extended" `
                -RuleId "checkpoint-extended-signon-orchestration" `
                -Description "Signon regression orchestration tooling or workflow wiring changed." `
                -ChangedPath $NormalizedPath)
    }

    if ($lowerPath.StartsWith("docs/") -or $lowerPath.EndsWith(".md") -or $lowerPath.EndsWith(".txt")) {
        return (New-RuleMatch `
                -Profile "default" `
                -RuleId "default-safe-docs-reporting" `
                -Description "Docs or text/reporting-only files changed." `
                -ChangedPath $NormalizedPath)
    }

    if ($lowerPath.StartsWith(".github/")) {
        return (New-RuleMatch `
                -Profile "default" `
                -RuleId "default-safe-workflow-only" `
                -Description "GitHub workflow or metadata files changed outside the resumed-denial regression workflow." `
                -ChangedPath $NormalizedPath)
    }

    if ($lowerPath.StartsWith("tools/")) {
        return (New-RuleMatch `
                -Profile "default" `
                -RuleId "default-safe-tooling-only" `
                -Description "Tooling or reporting files changed outside the signon regression orchestration scripts." `
                -ChangedPath $NormalizedPath)
    }

    return (New-RuleMatch `
            -Profile "checkpoint-extended" `
            -RuleId "checkpoint-extended-ambiguous-nonruntime" `
            -Description "Changed files are outside the explicitly safe docs/workflow/tooling set, so the profile is escalated conservatively." `
            -ChangedPath $NormalizedPath)
}

function Get-SelectionReason {
    param(
        [string]$ResolvedProfile,
        [object[]]$MatchedRules,
        [int]$ChangedPathCount
    )

    if ($ChangedPathCount -eq 0) {
        return "No changed files were detected for the requested diff, so the default profile is sufficient."
    }

    $matchedRuleIds = @($MatchedRules | ForEach-Object { [string]$_.id })
    switch ($ResolvedProfile) {
        "full-expanded" {
            if ($matchedRuleIds -contains "full-expanded-high-risk-runtime-hotspots") {
                return "High-risk signon/runtime hotspot files changed under include/ or src/, so full-expanded is required."
            }

            return "Runtime-adjacent source or header files changed under include/ or src/, so full-expanded is selected conservatively."
        }
        "checkpoint-extended" {
            if ($matchedRuleIds -contains "checkpoint-extended-signon-orchestration") {
                return "Changed files touch signon regression orchestration tooling, so checkpoint-extended is the smallest safe profile."
            }

            return "Changed files fall outside the explicitly safe docs/workflow/tooling set, so checkpoint-extended is selected conservatively."
        }
        default {
            return "All changed files are docs, workflow, or tooling/reporting-only paths, so the default profile is sufficient."
        }
    }
}

function Resolve-SignonRegressionProfile {
    param(
        [Parameter(Mandatory = $true)]
        [string]$RepoRoot,
        [string]$DiffBase,
        [string]$DiffHead,
        [string[]]$ChangedPath
    )

    $resolvedRepoRoot = Resolve-ResolverRepoRoot -RequestedRoot $RepoRoot
    $normalizedChangedPath = @(Get-TrimmedUniqueValues -Values @($ChangedPath) | ForEach-Object {
            Normalize-ChangedPathValue -PathValue $_ -ResolvedRepoRoot $resolvedRepoRoot
        } | Where-Object { -not [string]::IsNullOrWhiteSpace($_) })

    $effectiveDiffBase = if ([string]::IsNullOrWhiteSpace($DiffBase)) { "" } else { $DiffBase.Trim() }
    $effectiveDiffHead = if ([string]::IsNullOrWhiteSpace($DiffHead)) { "" } else { $DiffHead.Trim() }
    $changedPathSource = "explicit"
    if ($normalizedChangedPath.Count -eq 0) {
        $diffResult = Get-ChangedPathsFromGitDiff -ResolvedRepoRoot $resolvedRepoRoot -DiffBase $DiffBase -DiffHead $DiffHead
        $normalizedChangedPath = @($diffResult.ChangedPaths | ForEach-Object {
                Normalize-ChangedPathValue -PathValue $_ -ResolvedRepoRoot $resolvedRepoRoot
            } | Where-Object { -not [string]::IsNullOrWhiteSpace($_) })
        $effectiveDiffBase = [string]$diffResult.DiffBase
        $effectiveDiffHead = [string]$diffResult.DiffHead
        $changedPathSource = "git-diff"
    }

    $normalizedChangedPath = @($normalizedChangedPath | Sort-Object -Unique)
    $groupedMatches = [ordered]@{}
    if ($normalizedChangedPath.Count -eq 0) {
        $groupedMatches["default-no-changes"] = [ordered]@{
            profile = "default"
            id = "default-no-changes"
            description = "No changed files were detected for the requested diff."
            matchedPaths = @()
        }
    }
    else {
        foreach ($path in $normalizedChangedPath) {
            $match = Get-RuleMatchForChangedPath -NormalizedPath $path
            $ruleId = [string]$match.id
            if (-not $groupedMatches.Contains($ruleId)) {
                $groupedMatches[$ruleId] = [ordered]@{
                    profile = [string]$match.profile
                    id = $ruleId
                    description = [string]$match.description
                    matchedPaths = New-Object System.Collections.Generic.List[string]
                }
            }

            foreach ($matchedPath in @($match.matchedPaths)) {
                if (-not [string]::IsNullOrWhiteSpace($matchedPath)) {
                    $groupedMatches[$ruleId].matchedPaths.Add([string]$matchedPath)
                }
            }
        }
    }

    $matchedRules = @($groupedMatches.Values | ForEach-Object {
            [ordered]@{
                profile = [string]$_.profile
                id = [string]$_.id
                description = [string]$_.description
                matchedPaths = @([string[]]$_.matchedPaths | Sort-Object -Unique)
            }
        } | Sort-Object @{ Expression = { Get-ProfileRank -ProfileName $_.profile }; Descending = $true }, @{ Expression = { [string]$_.id }; Descending = $false })

    $resolvedProfile = "default"
    foreach ($rule in $matchedRules) {
        if ((Get-ProfileRank -ProfileName ([string]$rule.profile)) -gt (Get-ProfileRank -ProfileName $resolvedProfile)) {
            $resolvedProfile = [string]$rule.profile
        }
    }

    return [pscustomobject]([ordered]@{
            resolvedProfile = $resolvedProfile
            reason = (Get-SelectionReason -ResolvedProfile $resolvedProfile -MatchedRules $matchedRules -ChangedPathCount $normalizedChangedPath.Count)
            matchedRuleIds = @($matchedRules | ForEach-Object { [string]$_.id })
            matchedRules = @($matchedRules)
            changedPaths = @($normalizedChangedPath)
            changedPathSource = $changedPathSource
            diffBase = $effectiveDiffBase
            diffHead = $effectiveDiffHead
            repoRoot = $resolvedRepoRoot
        })
}

function Write-SignonRegressionProfileReport {
    param(
        [Parameter(Mandatory = $true)]
        [object]$Resolution,
        [switch]$PrintChangedFiles
    )

    Write-Heading "Signon Regression Profile Selection"
    Write-Host ("repo root: {0}" -f [string]$Resolution.repoRoot)
    Write-Host ("changed path source: {0}" -f [string]$Resolution.changedPathSource)
    if (-not [string]::IsNullOrWhiteSpace([string]$Resolution.diffBase)) {
        Write-Host ("diff base: {0}" -f [string]$Resolution.diffBase)
    }
    if (-not [string]::IsNullOrWhiteSpace([string]$Resolution.diffHead)) {
        Write-Host ("diff head: {0}" -f [string]$Resolution.diffHead)
    }
    Write-Host ("resolved profile: {0}" -f [string]$Resolution.resolvedProfile)
    Write-Host ("reason: {0}" -f [string]$Resolution.reason)

    $matchedRuleIds = @($Resolution.matchedRuleIds | Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) })
    if ($matchedRuleIds.Count -gt 0) {
        Write-Host ("matched rules: {0}" -f ($matchedRuleIds -join ", "))
    }

    foreach ($rule in @($Resolution.matchedRules)) {
        $rulePaths = @($rule.matchedPaths | Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) })
        $line = "  - {0}: {1}" -f [string]$rule.id, [string]$rule.description
        if ($rulePaths.Count -gt 0) {
            $line += " [" + ($rulePaths -join ", ") + "]"
        }

        Write-Host $line
    }

    if ($PrintChangedFiles) {
        Write-Host "changed files:"
        if (@($Resolution.changedPaths).Count -eq 0) {
            Write-Host "  - <none>"
        }
        else {
            foreach ($path in @($Resolution.changedPaths)) {
                Write-Host ("  - {0}" -f [string]$path)
            }
        }
    }
}

if ($MyInvocation.InvocationName -ne ".") {
    $resolution = Resolve-SignonRegressionProfile `
        -RepoRoot $RepoRoot `
        -DiffBase $DiffBase `
        -DiffHead $DiffHead `
        -ChangedPath $ChangedPath
    Write-SignonRegressionProfileReport -Resolution $resolution -PrintChangedFiles:$PrintChangedFiles
}
