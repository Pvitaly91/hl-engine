[CmdletBinding()]
param(
    [string]$ServerExecutablePath,
    [string]$ServerGameDir,
    [string]$ClientAExecutablePath,
    [string]$ClientBExecutablePath,
    [string]$ClientGameDir,

    [ValidatePattern('^[A-Za-z0-9_][A-Za-z0-9_-]{0,63}$')]
    [string]$Map = 'crossfire',

    [string]$BindAddress = '127.0.0.1',

    [ValidateRange(0, 65535)]
    [int]$Port = 0,

    [ValidateSet('Debug', 'Release', 'RelWithDebInfo', 'MinSizeRel')]
    [string]$Configuration = 'Release',

    [ValidateRange(1, 300)]
    [int]$ReadinessTimeoutSeconds = 30,

    [ValidateRange(0, 86400)]
    [int]$DurationSeconds = 0,

    [string]$LogDirectory,

    [string[]]$AdditionalServerArguments = @(),
    [string[]]$AdditionalClientAArguments = @(),
    [string[]]$AdditionalClientBArguments = @(),
    [switch]$SkipBuild,
    [switch]$RunTests,
    [switch]$ServerOnly,
    [switch]$ManualClientB,
    [switch]$DryRun,
    [switch]$KeepServerRunning,
    [switch]$StopClientsOnExit
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
if (Test-Path Variable:PSNativeCommandUseErrorActionPreference) {
    $PSNativeCommandUseErrorActionPreference = $false
}
if ($BindAddress -cne '127.0.0.1') {
    throw 'Two-client manual testing is restricted to 127.0.0.1.'
}

function Get-ManualLauncherFunctionDefinitions {
    param([string]$PathValue)

    $tokens = $null
    $errors = $null
    $ast = [System.Management.Automation.Language.Parser]::ParseFile(
        $PathValue,
        [ref]$tokens,
        [ref]$errors)
    if (@($errors).Count -ne 0) {
        throw 'The shared manual launcher has parser errors.'
    }
    return @($ast.EndBlock.Statements |
        Where-Object {
            $_ -is [System.Management.Automation.Language.FunctionDefinitionAst]
        } |
        ForEach-Object { $_.Extent.Text })
}

foreach ($definitionText in @(Get-ManualLauncherFunctionDefinitions `
        -PathValue (Join-Path $PSScriptRoot 'run_manual_pmove_test.ps1'))) {
    . ([scriptblock]::Create($definitionText))
}

function New-TwoClientServerArguments {
    param(
        [string]$GameDirectory,
        [string]$MapName,
        [string]$Address,
        [int]$SelectedPort,
        [string]$ShutdownRequestPath)

    return @(
        '--gamedir', (Quote-ProcessArgument $GameDirectory),
        '--dedicated',
        '--deathmatch', '1',
        '--maxclients', '2',
        '--map', $MapName,
        '--frames', '1',
        '--log-to-file', '0',
        '--log-summary-file', '0',
        '--log-disable-categories=general',
        '--ip', $Address,
        '--port', [string]$SelectedPort,
        '--goldsrc-pmove',
        '--goldsrc-snapshot-rate-hz=20',
        '--goldsrc-handshake-timeout-ms', '300000',
        '--goldsrc-pmove-persistent',
        '--goldsrc-manual-shutdown-file',
        (Quote-ProcessArgument $ShutdownRequestPath)
    ) + @(Convert-AdditionalArguments $AdditionalServerArguments)
}

function New-TwoClientClientArguments {
    param(
        [string]$Address,
        [int]$SelectedPort,
        [string[]]$AdditionalArguments)

    return @(
        '-steam', '-game', 'valve', '-console', '-novid', '-nojoy',
        '-windowed', '-w', '960', '-h', '540',
        '+joystick', '0',
        '+connect', ('{0}:{1}' -f $Address, $SelectedPort)
    ) + @(Convert-AdditionalArguments $AdditionalArguments)
}

function Invoke-QuietNative {
    param(
        [string]$Executable,
        [string[]]$Arguments,
        [string]$OutputPath,
        [string]$Description)

    & $Executable @Arguments *> $OutputPath
    if ($LASTEXITCODE -ne 0) {
        throw ('{0} failed; inspect the bounded external log.' -f $Description)
    }
}

function Start-OwnedHalfLifeClient {
    param(
        [string]$Executable,
        [string[]]$Arguments)

    $before = @(Get-Process -Name hl -ErrorAction SilentlyContinue |
        ForEach-Object { $_.Id })
    $launched = Start-Process -FilePath $Executable `
        -ArgumentList $Arguments `
        -WorkingDirectory (Split-Path -Parent $Executable) `
        -PassThru
    Start-Sleep -Milliseconds 750
    try { $launched.Refresh() } catch { }
    if (-not $launched.HasExited -and
        $launched.ProcessName -ieq 'hl' -and
        $before -notcontains $launched.Id) {
        return $launched
    }
    $launched.Dispose()
    return $null
}

function Write-TwoClientChecklist {
    param([string]$Address, [int]$SelectedPort)

    Write-Host 'Two-client stock Half-Life test'
    Write-Host ('Endpoint: {0}:{1}' -f $Address, $SelectedPort)
    Write-Host '1. Spawn A, then connect/spawn B while A is moving.'
    Write-Host '2. Check W/A/S/D, mouse look, jump, and duck in both clients.'
    Write-Host '3. Confirm A sees B and B sees A moving without freezes or snap-back.'
    Write-Host '4. Move both simultaneously; verify independent local views.'
    Write-Host '5. Keep both connected through 60, 120, 300, and 600 seconds.'
    Write-Host '6. Disconnect A; verify B moves and A disappears from B.'
    Write-Host '7. Reconnect A; verify clean spawn, reappearance, and movement.'
    Write-Host 'Console fallback for either client:'
    Write-Host ('connect {0}:{1}' -f $Address, $SelectedPort)
}

$repositoryRoot = [System.IO.Path]::GetFullPath(
    (Join-Path $PSScriptRoot '..')).TrimEnd('\', '/')
if (-not (Test-Path -LiteralPath (
        Join-Path $repositoryRoot 'CMakeLists.txt') -PathType Leaf) -or
    -not (Test-Path -LiteralPath (
        Join-Path $repositoryRoot '.git') -PathType Container)) {
    throw 'Repository root could not be resolved from the launcher location.'
}
Assert-SafeAdditionalArguments -Arguments $AdditionalServerArguments `
    -ReservedPrefixes @(
        '--gamedir', '--dedicated', '--deathmatch', '--maxclients', '--map',
        '--frames', '--log-to-file', '--log-summary-file',
        '--log-disable-categories', '--ip', '-ip', '--port', '-port',
        '--goldsrc-pmove', '--goldsrc-pmove-persistent',
        '--goldsrc-manual-shutdown-file', '--goldsrc-snapshot-rate-hz',
        '--goldsrc-handshake-timeout-ms') -Kind 'server'
$reservedClientPrefixes = @(
    '-steam', '-game', '-console', '-novid', '-nojoy', '-windowed',
    '-w', '-h', '+joystick', '+connect')
Assert-SafeAdditionalArguments -Arguments $AdditionalClientAArguments `
    -ReservedPrefixes $reservedClientPrefixes -Kind 'client A'
Assert-SafeAdditionalArguments -Arguments $AdditionalClientBArguments `
    -ReservedPrefixes $reservedClientPrefixes -Kind 'client B'

$cmake = Resolve-CMakeExecutable
$ctest = if ($RunTests) {
    Resolve-CTestExecutable -CMakeExecutable $cmake
} else { '' }
$buildDirectory = Join-Path `
    $repositoryRoot 'out\build\vs2022-win32-reference-sdk'
$canonicalTreeValid = Test-CanonicalBuildTree `
    -BuildDirectory $buildDirectory -RepositoryRoot $repositoryRoot

$clientAPath = ''
$clientBPath = ''
$clientGamePath = ''
if (-not $ServerOnly) {
    $clientAPath = Resolve-HalfLifeClient `
        -RequestedPath $ClientAExecutablePath
    $clientBPath = if ([string]::IsNullOrWhiteSpace(
            $ClientBExecutablePath)) {
        $clientAPath
    } else {
        Resolve-HalfLifeClient -RequestedPath $ClientBExecutablePath
    }
    $clientGamePath = Resolve-ClientGameDirectory `
        -RequestedPath $ClientGameDir -ClientPath $clientAPath -MapName $Map
}
$serverGamePath = Resolve-ServerGameDirectory `
    -RequestedPath $ServerGameDir -MapName $Map `
    -ResolvedClientPath $clientAPath -RequestedClientGameDir $clientGamePath

$automaticPort = $Port -eq 0
$selectedPort = if ($automaticPort) { Get-AvailableUdpPort } else { $Port }
if (-not $automaticPort -and
    -not (Test-UdpPortAvailable -RequestedPort $selectedPort)) {
    throw 'The requested localhost UDP port is already in use.'
}

$timestamp = [DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss-fff')
$logPath = if ([string]::IsNullOrWhiteSpace($LogDirectory)) {
    Join-Path ([System.IO.Path]::GetTempPath()) (
        Join-Path 'HL-Engine\manual-two-client' (
            $timestamp + '-' + [Guid]::NewGuid().ToString('N').Substring(0, 8)))
} else {
    [System.IO.Path]::GetFullPath($LogDirectory)
}
if (Test-PathInsideRoot -PathValue $logPath -Root $repositoryRoot) {
    throw 'Manual session logs must be stored outside the repository.'
}
$shutdownRequestPath = Join-Path $logPath 'shutdown.request'

if ($DryRun) {
    $serverPath = Resolve-HlhostExecutable `
        -RequestedPath $ServerExecutablePath -RepositoryRoot $repositoryRoot `
        -BuildDirectory $buildDirectory -BuildConfiguration $Configuration
    $serverArguments = New-TwoClientServerArguments `
        -GameDirectory $serverGamePath -MapName $Map -Address $BindAddress `
        -SelectedPort $selectedPort -ShutdownRequestPath $shutdownRequestPath
    Write-Host 'Manual two-client dry run'
    Write-Host 'server_executable=<resolved-hlhost.exe>'
    Write-Host 'server_arguments=<validated-local-two-client-arguments>'
    if (-not $ServerOnly) {
        Write-Host 'client_a_executable=<resolved-hl.exe>'
        Write-Host $(if ($ManualClientB) {
            'client_b_launch=manual'
        } else {
            'client_b_executable=<resolved-hl.exe>'
        })
    }
    Write-Host ('address={0}:{1}' -f $BindAddress, $selectedPort)
    Write-Host ('map={0}' -f $Map)
    Write-Host 'max_clients=2'
    Write-Host 'persistent=true'
    Write-Host ('configure_would_run={0}' -f (
        (-not $canonicalTreeValid).ToString().ToLowerInvariant()))
    Write-Host ('build_would_run={0}' -f (
        (-not $SkipBuild).ToString().ToLowerInvariant()))
    Write-Host 'cleanup=owned-processes-only'
    Write-Host 'dry_run=pass'
    exit 0
}

[void][System.IO.Directory]::CreateDirectory($logPath)
$stdoutPath = Join-Path $logPath 'server.stdout.log'
$stderrPath = Join-Path $logPath 'server.stderr.log'
$buildLogPath = Join-Path $logPath 'build.log'
$testLogPath = Join-Path $logPath 'ctest.log'
[System.IO.File]::WriteAllText($stdoutPath, '')
[System.IO.File]::WriteAllText($stderrPath, '')

if (-not $SkipBuild) {
    if (-not $canonicalTreeValid) {
        Invoke-QuietNative -Executable $cmake -Arguments @(
            '-S', $repositoryRoot, '-B', $buildDirectory,
            '-G', 'Visual Studio 17 2022', '-A', 'Win32') `
            -OutputPath $buildLogPath -Description 'CMake configure'
    }
    Invoke-QuietNative -Executable $cmake -Arguments @(
        '--build', $buildDirectory, '--config', $Configuration,
        '--target', 'hlhost', '--', '/m:1') `
        -OutputPath $buildLogPath -Description 'hlhost build'
}
$serverPath = Resolve-HlhostExecutable `
    -RequestedPath $ServerExecutablePath -RepositoryRoot $repositoryRoot `
    -BuildDirectory $buildDirectory -BuildConfiguration $Configuration
if ($RunTests) {
    Invoke-QuietNative -Executable $ctest -Arguments @(
        '--test-dir', $buildDirectory, '-C', $Configuration,
        '--output-on-failure') -OutputPath $testLogPath -Description 'CTest'
}

$serverProcess = $null
$clientAProcess = $null
$clientBProcess = $null
$failure = $null
$normalCompletion = $false
try {
    $serverArguments = New-TwoClientServerArguments `
        -GameDirectory $serverGamePath -MapName $Map -Address $BindAddress `
        -SelectedPort $selectedPort -ShutdownRequestPath $shutdownRequestPath
    $serverProcess = Start-Process -FilePath $serverPath `
        -ArgumentList $serverArguments `
        -WorkingDirectory (Split-Path -Parent $serverPath) `
        -RedirectStandardOutput $stdoutPath `
        -RedirectStandardError $stderrPath `
        -WindowStyle Hidden -PassThru
    Wait-HlhostReady -Process $serverProcess -StdoutPath $stdoutPath `
        -ExpectedAddress $BindAddress -ExpectedPort $selectedPort `
        -TimeoutSeconds $ReadinessTimeoutSeconds
    Wait-UdpEndpointOwnership -Process $serverProcess `
        -Address $BindAddress -SelectedPort $selectedPort `
        -TimeoutSeconds $ReadinessTimeoutSeconds

    Write-Host 'Two-client server ready'
    Write-Host ('Address: {0}:{1}' -f $BindAddress, $selectedPort)
    Write-Host ('Map: {0}' -f $Map)
    Write-Host ('External logs: {0}' -f $logPath)

    if (-not $ServerOnly) {
        $clientAProcess = Start-OwnedHalfLifeClient `
            -Executable $clientAPath `
            -Arguments (New-TwoClientClientArguments `
                -Address $BindAddress -SelectedPort $selectedPort `
                -AdditionalArguments $AdditionalClientAArguments)
        if ($null -eq $clientAProcess) {
            Write-Warning 'Client A needs manual launch; server remains active.'
        } else {
            Write-Host 'Client A launch requested.'
        }
        if (-not $ManualClientB) {
            $clientBProcess = Start-OwnedHalfLifeClient `
                -Executable $clientBPath `
                -Arguments (New-TwoClientClientArguments `
                    -Address $BindAddress -SelectedPort $selectedPort `
                    -AdditionalArguments $AdditionalClientBArguments)
            if ($null -eq $clientBProcess) {
                Write-Warning 'Steam did not expose a second owned process; launch B manually.'
            } else {
                Write-Host 'Client B launch requested.'
            }
        } else {
            Write-Host 'Client B is configured for manual launch.'
        }
        Write-TwoClientChecklist -Address $BindAddress -SelectedPort $selectedPort
    }

    $deadline = if ($DurationSeconds -gt 0) {
        [DateTime]::UtcNow.AddSeconds($DurationSeconds)
    } else { [DateTime]::MaxValue }
    $readTask = $null
    if ($DurationSeconds -eq 0) {
        Write-Host 'Press Enter to stop the persistent server; Ctrl+C also cleans it up.'
        $readTask = [Console]::In.ReadLineAsync()
    }
    while ([DateTime]::UtcNow -lt $deadline) {
        if ($serverProcess.HasExited) {
            throw 'The two-client server exited unexpectedly; inspect external logs.'
        }
        if ($DurationSeconds -eq 0 -and $readTask.IsCompleted) { break }
        Start-Sleep -Milliseconds 250
    }
    $normalCompletion = $true
}
catch {
    $failure = $_
}
finally {
    if ($StopClientsOnExit) {
        foreach ($owned in @($clientBProcess, $clientAProcess)) {
            if ($null -ne $owned) {
                try { [void](Stop-OwnedProcess -Process $owned -Description 'client') }
                catch { if ($null -eq $failure) { $failure = $_ } }
            }
        }
    }
    foreach ($owned in @($clientBProcess, $clientAProcess)) {
        if ($null -ne $owned) { $owned.Dispose() }
    }
    if ($null -ne $serverProcess) {
        if (-not ($KeepServerRunning -and $normalCompletion)) {
            try {
                [void](Stop-OwnedProcess -Process $serverProcess `
                    -Description 'server' `
                    -ShutdownRequestPath $shutdownRequestPath)
            } catch { if ($null -eq $failure) { $failure = $_ } }
        }
        $serverProcess.Dispose()
    }
}

if ($null -ne $failure) {
    Write-Error $failure.Exception.Message
    exit 1
}
Write-Host 'manual_two_client_launcher=pass'
exit 0
