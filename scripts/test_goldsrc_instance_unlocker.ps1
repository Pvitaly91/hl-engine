[CmdletBinding()]
param(
    [string]$UnlockerPath,
    [switch]$SkipBuild
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Resolve-CMakeExecutable {
    foreach ($name in @('cmake.exe', 'cmake')) {
        $command = Get-Command $name -ErrorAction SilentlyContinue
        if ($null -ne $command) {
            return $command.Source
        }
    }

    $candidates = @(
        (Join-Path $env:ProgramFiles 'CMake\bin\cmake.exe'),
        (Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'),
        (Join-Path $env:ProgramFiles 'Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'),
        (Join-Path $env:ProgramFiles 'Microsoft Visual Studio\2022\Professional\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'),
        (Join-Path $env:ProgramFiles 'Microsoft Visual Studio\2022\Enterprise\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe')
    )
    foreach ($candidate in $candidates) {
        if (Test-Path -LiteralPath $candidate -PathType Leaf) {
            return $candidate
        }
    }
    throw 'cmake.exe was not found.'
}

function Invoke-Unlocker {
    param(
        [Parameter(Mandatory = $true)][string]$Executable,
        [Parameter(Mandatory = $true)][string[]]$Arguments
    )

    $output = @(& $Executable @Arguments 2>&1)
    $exitCode = $LASTEXITCODE
    if ($output.Count -ne 1) {
        throw 'The unlocker did not emit exactly one bounded JSON result.'
    }
    try {
        $json = $output[0] | ConvertFrom-Json
    }
    catch {
        throw 'The unlocker emitted invalid JSON.'
    }
    return [pscustomobject]@{
        ExitCode = $exitCode
        Json = $json
    }
}

function Assert-Condition {
    param(
        [Parameter(Mandatory = $true)][bool]$Condition,
        [Parameter(Mandatory = $true)][string]$Message
    )
    if (-not $Condition) {
        throw $Message
    }
}

function Start-TestHolder {
    param(
        [Parameter(Mandatory = $true)][string]$Executable,
        [Parameter(Mandatory = $true)][string]$Root,
        [Parameter(Mandatory = $true)][int]$DuplicateCount
    )

    $token = [Guid]::NewGuid().ToString('N')
    $readyFile = Join-Path $Root ($token + '.ready.json')
    $shutdownFile = Join-Path $Root ($token + '.shutdown')
    $objectName = 'Local\HLHostUnlockerTest-' + $token
    $controlName = 'Local\HLHostUnlockerControl-' + $token
    $arguments = @(
        '--test-holder',
        '--object-name', $objectName,
        '--control-object-name', $controlName,
        '--duplicate-count', $DuplicateCount.ToString(),
        '--ready-file', $readyFile,
        '--shutdown-file', $shutdownFile
    )
    $process = Start-Process -FilePath $Executable -ArgumentList $arguments `
        -PassThru -WindowStyle Hidden

    $deadline = [DateTime]::UtcNow.AddSeconds(10)
    while ([DateTime]::UtcNow -lt $deadline) {
        if (Test-Path -LiteralPath $readyFile -PathType Leaf) {
            $ready = Get-Content -LiteralPath $readyFile -Raw | ConvertFrom-Json
            Assert-Condition ([int]$ready.pid -eq $process.Id) `
                'The holder ready record did not match its owned PID.'
            return [pscustomobject]@{
                Process = $process
                Ready = $ready
                ReadyFile = $readyFile
                ShutdownFile = $shutdownFile
                ObjectName = $objectName
                ControlName = $controlName
            }
        }
        if ($process.HasExited) {
            throw 'The test holder exited before becoming ready.'
        }
        Start-Sleep -Milliseconds 50
    }
    throw 'The test holder did not become ready within the bounded wait.'
}

function Stop-TestHolder {
    param([Parameter(Mandatory = $true)]$Holder)

    New-Item -ItemType File -Path $Holder.ShutdownFile -Force | Out-Null
    if (-not $Holder.Process.WaitForExit(5000)) {
        $current = Get-Process -Id $Holder.Process.Id -ErrorAction SilentlyContinue
        if ($null -ne $current -and
            $current.StartTime.ToFileTimeUtc() -eq
                [int64]$Holder.Ready.creation_time) {
            $current.Kill()
            $current.WaitForExit(5000) | Out-Null
        }
        else {
            throw 'The exact owned holder could not be verified for cleanup.'
        }
    }
}

$repositoryRoot = Split-Path -Parent $PSScriptRoot
$buildDirectory = Join-Path $repositoryRoot 'out\build\vs2022-x64-tools'
if ([string]::IsNullOrWhiteSpace($UnlockerPath)) {
    $UnlockerPath = Join-Path $buildDirectory `
        'Release\goldsrc_instance_unlocker.exe'
}

if (-not $SkipBuild) {
    $cmake = Resolve-CMakeExecutable
    & $cmake -S $repositoryRoot -B $buildDirectory `
        -G 'Visual Studio 17 2022' -A x64
    if ($LASTEXITCODE -ne 0) {
        throw 'The isolated x64 tools configure failed.'
    }
    & $cmake --build $buildDirectory --config Release `
        --target goldsrc_instance_unlocker
    if ($LASTEXITCODE -ne 0) {
        throw 'The isolated x64 unlocker build failed.'
    }
}

$UnlockerPath = [IO.Path]::GetFullPath($UnlockerPath)
if (-not (Test-Path -LiteralPath $UnlockerPath -PathType Leaf)) {
    throw 'The x64 unlocker executable was not found.'
}

$artifactRoot = Join-Path ([IO.Path]::GetTempPath()) `
    ('HL-Engine\instance-unlocker-tests\' +
        [DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss-fff') + '-' +
        [Guid]::NewGuid().ToString('N').Substring(0, 8))
New-Item -ItemType Directory -Path $artifactRoot -Force | Out-Null

$holder = $null
$ambiguousHolder = $null
$checks = [ordered]@{}
try {
    $holder = Start-TestHolder -Executable $UnlockerPath `
        -Root $artifactRoot -DuplicateCount 1
    $base = @(
        '--pid', $holder.Process.Id.ToString(),
        '--expected-image', $UnlockerPath,
        '--expected-creation-time',
            ([string]$holder.Ready.creation_time),
        '--object-name', $holder.ObjectName,
        '--json'
    )

    $inspect = Invoke-Unlocker $UnlockerPath ($base + '--inspect-only')
    $checks.inspect = $inspect.ExitCode -eq 0 -and
        $inspect.Json.status -eq 'inspected' -and
        [int]$inspect.Json.matched_handle_count -eq 1 -and
        $inspect.Json.matched_object_type -eq 'Mutant' -and
        -not [bool]$inspect.Json.handle_closed
    Assert-Condition $checks.inspect 'Exact inspect-only verification failed.'

    $wrongImage = Invoke-Unlocker $UnlockerPath @(
        '--pid', $holder.Process.Id.ToString(),
        '--expected-image', (Join-Path $env:SystemRoot 'System32\cmd.exe'),
        '--expected-creation-time', ([string]$holder.Ready.creation_time),
        '--object-name', $holder.ObjectName,
        '--inspect-only', '--json'
    )
    $checks.wrong_image = $wrongImage.ExitCode -eq 3 -and
        $wrongImage.Json.blocker -eq 'target_image_mismatch'
    Assert-Condition $checks.wrong_image 'Wrong-image rejection failed.'

    $wrongCreation = Invoke-Unlocker $UnlockerPath @(
        '--pid', $holder.Process.Id.ToString(),
        '--expected-image', $UnlockerPath,
        '--expected-creation-time',
            ([string]([int64]$holder.Ready.creation_time + 1)),
        '--object-name', $holder.ObjectName,
        '--inspect-only', '--json'
    )
    $checks.wrong_creation = $wrongCreation.ExitCode -eq 3 -and
        $wrongCreation.Json.blocker -eq 'target_creation_time_mismatch'
    Assert-Condition $checks.wrong_creation `
        'Wrong-creation-time rejection failed.'

    $wrongName = Invoke-Unlocker $UnlockerPath @(
        '--pid', $holder.Process.Id.ToString(),
        '--expected-image', $UnlockerPath,
        '--expected-creation-time', ([string]$holder.Ready.creation_time),
        '--object-name', ($holder.ObjectName + '-wrong'),
        '--inspect-only', '--json'
    )
    $checks.wrong_name = $wrongName.ExitCode -eq 2 -and
        [int]$wrongName.Json.matched_handle_count -eq 0
    Assert-Condition $checks.wrong_name 'Wrong-name rejection failed.'

    $close = Invoke-Unlocker $UnlockerPath ($base + '--close')
    $checks.exact_close = $close.ExitCode -eq 0 -and
        $close.Json.status -eq 'closed' -and
        [bool]$close.Json.handle_closed -and
        [int]$close.Json.matched_handle_count -eq 1
    Assert-Condition $checks.exact_close 'Exact mutex close failed.'

    $repeat = Invoke-Unlocker $UnlockerPath ($base + '--inspect-only')
    $checks.repeated_close = $repeat.ExitCode -eq 2 -and
        [int]$repeat.Json.matched_handle_count -eq 0
    Assert-Condition $checks.repeated_close 'Repeated-close safety check failed.'

    $control = Invoke-Unlocker $UnlockerPath @(
        '--pid', $holder.Process.Id.ToString(),
        '--expected-image', $UnlockerPath,
        '--expected-creation-time', ([string]$holder.Ready.creation_time),
        '--object-name', $holder.ControlName,
        '--inspect-only', '--json'
    )
    $checks.unrelated_handle_preserved = $control.ExitCode -eq 0 -and
        [int]$control.Json.matched_handle_count -eq 1
    Assert-Condition $checks.unrelated_handle_preserved `
        'An unrelated control mutex was affected.'

    $deadPid = $holder.Process.Id
    $deadCreation = [string]$holder.Ready.creation_time
    Stop-TestHolder $holder
    $holder = $null
    $wrongPid = Invoke-Unlocker $UnlockerPath @(
        '--pid', $deadPid.ToString(),
        '--expected-image', $UnlockerPath,
        '--expected-creation-time', $deadCreation,
        '--object-name', 'LauncherMutex',
        '--inspect-only', '--json'
    )
    $checks.wrong_pid = $wrongPid.ExitCode -eq 3
    Assert-Condition $checks.wrong_pid 'Dead/wrong-PID rejection failed.'

    $ambiguousHolder = Start-TestHolder -Executable $UnlockerPath `
        -Root $artifactRoot -DuplicateCount 2
    $ambiguous = Invoke-Unlocker $UnlockerPath @(
        '--pid', $ambiguousHolder.Process.Id.ToString(),
        '--expected-image', $UnlockerPath,
        '--expected-creation-time',
            ([string]$ambiguousHolder.Ready.creation_time),
        '--object-name', $ambiguousHolder.ObjectName,
        '--inspect-only', '--json'
    )
    $checks.ambiguous = $ambiguous.ExitCode -eq 4 -and
        [int]$ambiguous.Json.matched_handle_count -eq 2
    Assert-Condition $checks.ambiguous 'Ambiguous-match rejection failed.'

    Stop-TestHolder $ambiguousHolder
    $ambiguousHolder = $null

    $result = [ordered]@{
        status = 'pass'
        inspect = 'pass'
        exact_close = 'pass'
        wrong_pid = 'pass'
        wrong_image = 'pass'
        wrong_creation_time = 'pass'
        wrong_name = 'pass'
        ambiguous = 'pass'
        repeated_close = 'pass'
        unlocker_inspect_test = 'pass'
        unlocker_exact_close_test = 'pass'
        unlocker_wrong_pid_test = 'pass'
        unlocker_wrong_image_test = 'pass'
        unlocker_wrong_creation_time_test = 'pass'
        unlocker_wrong_name_test = 'pass'
        unlocker_ambiguous_match_test = 'pass'
        unlocker_repeated_close_test = 'pass'
        unrelated_handles_closed = 0
        process_injection = 'no'
        memory_patching = 'no'
    }
    $resultPath = Join-Path $artifactRoot 'instance_unlocker_tests.json'
    $result | ConvertTo-Json | Set-Content -LiteralPath $resultPath `
        -Encoding UTF8
    Write-Output 'unlocker_tests=pass'
    Write-Output 'unlocker_inspect_test=pass'
    Write-Output 'unlocker_exact_close_test=pass'
    Write-Output 'unlocker_wrong_pid_test=pass'
    Write-Output 'unlocker_wrong_image_test=pass'
    Write-Output 'unlocker_wrong_creation_time_test=pass'
    Write-Output 'unlocker_wrong_name_test=pass'
    Write-Output 'unlocker_ambiguous_match_test=pass'
    Write-Output 'unlocker_repeated_close_test=pass'
    Write-Output 'unrelated_handles_closed=0'
    Write-Output ('result_json=' + $resultPath)
}
finally {
    if ($null -ne $holder) {
        Stop-TestHolder $holder
    }
    if ($null -ne $ambiguousHolder) {
        Stop-TestHolder $ambiguousHolder
    }
}
$global:LASTEXITCODE = 0
