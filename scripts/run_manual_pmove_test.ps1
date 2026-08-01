[CmdletBinding()]
param(
    [string]$ServerExecutablePath,
    [string]$ServerGameDir,
    [string]$ClientExecutablePath,
    [string]$ClientGameDir,

    [ValidatePattern('^[A-Za-z0-9_][A-Za-z0-9_-]{0,63}$')]
    [string]$Map = 'crossfire',

    [string]$BindAddress = '127.0.0.1',

    [ValidateRange(0, 65535)]
    [int]$Port = 0,

    [ValidateSet('Debug', 'Release', 'RelWithDebInfo', 'MinSizeRel')]
    [string]$Configuration = 'Debug',

    [ValidateRange(1, 300)]
    [int]$ReadinessTimeoutSeconds = 30,

    [ValidateRange(0, 86400)]
    [int]$DurationSeconds = 0,

    [string]$LogDirectory,

    [ValidateRange(640, 7680)]
    [int]$WindowWidth = 1280,

    [ValidateRange(480, 4320)]
    [int]$WindowHeight = 720,

    [string[]]$AdditionalServerArguments = @(),
    [string[]]$AdditionalClientArguments = @(),
    [switch]$SkipBuild,
    [switch]$RunTests,
    [switch]$ServerOnly,
    [switch]$BoundedObservation,
    [switch]$DryRun,
    [switch]$KeepServerRunning,
    [switch]$StopClientOnExit,
    [switch]$Fullscreen,
    [switch]$FollowServerLog
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
if (Test-Path Variable:PSNativeCommandUseErrorActionPreference) {
    $PSNativeCommandUseErrorActionPreference = $false
}

function Resolve-RepositoryRoot {
    $root = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
    if (-not (Test-Path -LiteralPath (Join-Path $root 'CMakeLists.txt') -PathType Leaf) -or
        -not (Test-Path -LiteralPath (Join-Path $root '.git') -PathType Container)) {
        throw 'Repository root could not be resolved from the launcher location.'
    }
    return $root.TrimEnd('\', '/')
}

function Test-PathInsideRoot {
    param([string]$PathValue, [string]$Root)

    $path = [System.IO.Path]::GetFullPath($PathValue).TrimEnd('\', '/')
    $rootPath = [System.IO.Path]::GetFullPath($Root).TrimEnd('\', '/')
    return $path.Equals($rootPath, [StringComparison]::OrdinalIgnoreCase) -or
        $path.StartsWith(
            $rootPath + [System.IO.Path]::DirectorySeparatorChar,
            [StringComparison]::OrdinalIgnoreCase)
}

function Resolve-CMakeExecutable {
    foreach ($name in @('cmake.exe', 'cmake')) {
        $command = Get-Command $name -ErrorAction SilentlyContinue
        if ($null -ne $command -and
            -not [string]::IsNullOrWhiteSpace($command.Source)) {
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
        if (-not [string]::IsNullOrWhiteSpace($candidate) -and
            (Test-Path -LiteralPath $candidate -PathType Leaf)) {
            return [System.IO.Path]::GetFullPath($candidate)
        }
    }
    throw 'cmake.exe was not found in PATH or a standard Visual Studio installation.'
}

function Resolve-CTestExecutable {
    param([string]$CMakeExecutable)

    $sibling = Join-Path (Split-Path -Parent $CMakeExecutable) 'ctest.exe'
    if (Test-Path -LiteralPath $sibling -PathType Leaf) {
        return $sibling
    }
    $command = Get-Command ctest.exe -ErrorAction SilentlyContinue
    if ($null -ne $command) {
        return $command.Source
    }
    throw 'ctest.exe was not found beside cmake.exe or in PATH.'
}

function Test-CanonicalBuildTree {
    param([string]$BuildDirectory, [string]$RepositoryRoot)

    $cache = Join-Path $BuildDirectory 'CMakeCache.txt'
    if (-not (Test-Path -LiteralPath $cache -PathType Leaf)) {
        return $false
    }
    $text = Get-Content -LiteralPath $cache -Raw
    $normalizedRoot = $RepositoryRoot.Replace('\', '/')
    return $text.Contains('CMAKE_GENERATOR:INTERNAL=Visual Studio 17 2022') -and
        $text.Contains('CMAKE_GENERATOR_PLATFORM:INTERNAL=Win32') -and
        $text.Contains(('CMAKE_HOME_DIRECTORY:INTERNAL={0}' -f $normalizedRoot))
}

function Invoke-NativeLogged {
    param(
        [string]$Executable,
        [string[]]$Arguments,
        [string]$OutputPath,
        [string]$Description
    )

    & $Executable @Arguments 2>&1 |
        Out-File -LiteralPath $OutputPath -Append -Encoding utf8
    $exitCode = $LASTEXITCODE
    if ($exitCode -ne 0) {
        Write-Host ("{0} failed. Recent output:" -f $Description)
        Get-Content -LiteralPath $OutputPath -Tail 30 -ErrorAction SilentlyContinue |
            ForEach-Object { Write-Host $_ }
        throw ("{0} failed with exit code {1}." -f $Description, $exitCode)
    }
}

function Get-SteamInstallRoots {
    $roots = @()
    $registryLocations = @(
        @{ Path = 'HKCU:\Software\Valve\Steam'; Name = 'SteamPath' },
        @{ Path = 'HKLM:\SOFTWARE\WOW6432Node\Valve\Steam'; Name = 'InstallPath' },
        @{ Path = 'HKLM:\SOFTWARE\Valve\Steam'; Name = 'InstallPath' }
    )
    foreach ($location in $registryLocations) {
        try {
            $value = (Get-ItemProperty -LiteralPath $location.Path -ErrorAction Stop).($location.Name)
            if (-not [string]::IsNullOrWhiteSpace([string]$value)) {
                $roots += [System.IO.Path]::GetFullPath([string]$value)
            }
        }
        catch {
            # Registry discovery is best effort; explicit paths remain supported.
        }
    }
    foreach ($candidate in @(
        (Join-Path ${env:ProgramFiles(x86)} 'Steam'),
        (Join-Path $env:ProgramFiles 'Steam')
    )) {
        if (-not [string]::IsNullOrWhiteSpace($candidate) -and
            (Test-Path -LiteralPath $candidate -PathType Container)) {
            $roots += [System.IO.Path]::GetFullPath($candidate)
        }
    }
    return @($roots | Select-Object -Unique)
}

function Get-SteamLibraryRoots {
    $libraries = @()
    foreach ($steamRoot in @(Get-SteamInstallRoots)) {
        $libraries += $steamRoot
        $vdfPath = Join-Path $steamRoot 'steamapps\libraryfolders.vdf'
        if (-not (Test-Path -LiteralPath $vdfPath -PathType Leaf)) {
            continue
        }
        foreach ($line in Get-Content -LiteralPath $vdfPath) {
            $match = [regex]::Match($line, '^\s*"(?:path|[0-9]+)"\s*"([^"]+)"\s*$')
            if ($match.Success) {
                $candidate = $match.Groups[1].Value.Replace('\\', '\')
                if (Test-Path -LiteralPath $candidate -PathType Container) {
                    $libraries += [System.IO.Path]::GetFullPath($candidate)
                }
            }
        }
    }
    return @($libraries | Select-Object -Unique)
}

function Get-HalfLifeInstallCandidates {
    $installs = @()
    foreach ($library in @(Get-SteamLibraryRoots)) {
        $candidate = Join-Path $library 'steamapps\common\Half-Life'
        if (Test-Path -LiteralPath $candidate -PathType Container) {
            $installs += [System.IO.Path]::GetFullPath($candidate)
        }
    }
    return @($installs | Select-Object -Unique)
}

function Resolve-HalfLifeClient {
    param([string]$RequestedPath)

    if (-not [string]::IsNullOrWhiteSpace($RequestedPath)) {
        $resolved = [System.IO.Path]::GetFullPath($RequestedPath)
        if (-not (Test-Path -LiteralPath $resolved -PathType Leaf)) {
            throw 'The requested Half-Life client executable does not exist.'
        }
        if ((Split-Path -Leaf $resolved) -ine 'hl.exe' -or
            -not (Test-Win32PortableExecutable -PathValue $resolved)) {
            throw 'The requested client is not a Win32 hl.exe executable.'
        }
        return $resolved
    }
    foreach ($install in @(Get-HalfLifeInstallCandidates)) {
        $candidate = Join-Path $install 'hl.exe'
        if ((Test-Path -LiteralPath $candidate -PathType Leaf) -and
            (Test-Win32PortableExecutable -PathValue $candidate)) {
            return $candidate
        }
    }
    throw "Half-Life client was not found.`nPass -ClientExecutablePath '<path-to-hl.exe>'."
}

function Test-GameDirectory {
    param([string]$PathValue, [string]$MapName)

    return (Test-Path -LiteralPath $PathValue -PathType Container) -and
        (Test-Path -LiteralPath (Join-Path $PathValue ('maps\{0}.bsp' -f $MapName)) -PathType Leaf) -and
        (Test-Path -LiteralPath (Join-Path $PathValue 'dlls\hl.dll') -PathType Leaf) -and
        (Test-Path -LiteralPath (Join-Path $PathValue 'delta.lst') -PathType Leaf)
}

function Resolve-ServerGameDirectory {
    param(
        [string]$RequestedPath,
        [string]$MapName,
        [string]$ResolvedClientPath,
        [string]$RequestedClientGameDir
    )

    if (-not [string]::IsNullOrWhiteSpace($RequestedPath)) {
        $resolved = [System.IO.Path]::GetFullPath($RequestedPath).TrimEnd('\', '/')
        if (-not (Test-GameDirectory -PathValue $resolved -MapName $MapName)) {
            throw 'The requested server GameDir is missing the map or required valve runtime files.'
        }
        return $resolved
    }

    $candidates = @()
    if (-not [string]::IsNullOrWhiteSpace($env:HLENGINE_VALVE_DIR)) {
        $candidates += [System.IO.Path]::GetFullPath($env:HLENGINE_VALVE_DIR)
    }
    if (-not [string]::IsNullOrWhiteSpace($RequestedClientGameDir)) {
        $candidates += [System.IO.Path]::GetFullPath($RequestedClientGameDir)
    }
    if (-not [string]::IsNullOrWhiteSpace($ResolvedClientPath)) {
        $candidates += Join-Path (Split-Path -Parent $ResolvedClientPath) 'valve'
    }
    foreach ($install in @(Get-HalfLifeInstallCandidates)) {
        $candidates += Join-Path $install 'valve'
    }

    foreach ($candidate in @($candidates | Select-Object -Unique)) {
        $fullPath = [System.IO.Path]::GetFullPath($candidate).TrimEnd('\', '/')
        if (Test-GameDirectory -PathValue $fullPath -MapName $MapName) {
            return $fullPath
        }
    }
    throw "A usable server GameDir was not found for map '$MapName'.`nPass -ServerGameDir '<path-to-valve>'."
}

function Resolve-ClientGameDirectory {
    param([string]$RequestedPath, [string]$ClientPath, [string]$MapName)

    $resolved = if (-not [string]::IsNullOrWhiteSpace($RequestedPath)) {
        [System.IO.Path]::GetFullPath($RequestedPath)
    } else {
        Join-Path (Split-Path -Parent $ClientPath) 'valve'
    }
    if (-not (Test-GameDirectory -PathValue $resolved -MapName $MapName)) {
        throw 'The client GameDir is missing the requested map or required valve runtime files.'
    }
    $resolved = $resolved.TrimEnd('\', '/')
    $expected = (Join-Path (Split-Path -Parent $ClientPath) 'valve').TrimEnd('\', '/')
    if ((Split-Path -Leaf $resolved) -ine 'valve' -or
        -not $resolved.Equals($expected, [StringComparison]::OrdinalIgnoreCase)) {
        throw 'The client GameDir must be the valve directory beside the selected hl.exe.'
    }
    return [System.IO.Path]::GetFullPath($resolved)
}

function Test-Win32PortableExecutable {
    param([string]$PathValue)

    $stream = $null
    $reader = $null
    try {
        $stream = [System.IO.File]::Open(
            $PathValue,
            [System.IO.FileMode]::Open,
            [System.IO.FileAccess]::Read,
            [System.IO.FileShare]::ReadWrite)
        $reader = New-Object System.IO.BinaryReader($stream)
        if ($reader.ReadUInt16() -ne 0x5A4D) { return $false }
        [void]$stream.Seek(0x3C, [System.IO.SeekOrigin]::Begin)
        $peOffset = $reader.ReadInt32()
        if ($peOffset -lt 0 -or $peOffset -gt $stream.Length - 6) { return $false }
        [void]$stream.Seek($peOffset, [System.IO.SeekOrigin]::Begin)
        if ($reader.ReadUInt32() -ne 0x00004550) { return $false }
        return $reader.ReadUInt16() -eq 0x014C
    }
    catch {
        return $false
    }
    finally {
        if ($null -ne $reader) { $reader.Dispose() }
        elseif ($null -ne $stream) { $stream.Dispose() }
    }
}

function Resolve-HlhostExecutable {
    param(
        [string]$RequestedPath,
        [string]$RepositoryRoot,
        [string]$BuildDirectory,
        [string]$BuildConfiguration
    )

    $explicit = -not [string]::IsNullOrWhiteSpace($RequestedPath)
    $resolved = if ($explicit) {
        [System.IO.Path]::GetFullPath($RequestedPath)
    } else {
        Join-Path $BuildDirectory (Join-Path $BuildConfiguration 'hlhost.exe')
    }
    if (-not (Test-Path -LiteralPath $resolved -PathType Leaf)) {
        throw 'hlhost.exe was not found. Build it or pass -ServerExecutablePath.'
    }
    if (-not $explicit -and -not (Test-PathInsideRoot -PathValue $resolved -Root $RepositoryRoot)) {
        throw 'The discovered hlhost.exe does not belong to the current workspace.'
    }
    if (-not (Test-Win32PortableExecutable -PathValue $resolved)) {
        throw 'hlhost.exe is not the expected Win32 executable.'
    }
    return [System.IO.Path]::GetFullPath($resolved)
}

function Assert-SafeAdditionalArguments {
    param([string[]]$Arguments, [string[]]$ReservedPrefixes, [string]$Kind)

    foreach ($argument in $Arguments) {
        if ([string]::IsNullOrWhiteSpace($argument) -or
            $argument.IndexOfAny([char[]]@("`0", "`r", "`n", '"')) -ge 0) {
            throw ("Invalid {0} argument." -f $Kind)
        }
        foreach ($prefix in $ReservedPrefixes) {
            if ($argument.Equals($prefix, [StringComparison]::OrdinalIgnoreCase) -or
                $argument.StartsWith($prefix + '=', [StringComparison]::OrdinalIgnoreCase)) {
                throw ("{0} argument '{1}' is managed by this launcher." -f $Kind, $prefix)
            }
        }
    }
}

function Convert-AdditionalArguments {
    param([string[]]$Arguments)

    return @($Arguments | ForEach-Object {
        if ($_ -match '\s') { Quote-ProcessArgument $_ } else { $_ }
    })
}

function Get-AvailableUdpPort {
    $client = New-Object System.Net.Sockets.UdpClient(
        [System.Net.Sockets.AddressFamily]::InterNetwork)
    try {
        $endpoint = New-Object System.Net.IPEndPoint(
            [System.Net.IPAddress]::Loopback,
            0)
        $client.Client.Bind($endpoint)
        return ([System.Net.IPEndPoint]$client.Client.LocalEndPoint).Port
    }
    finally {
        $client.Close()
    }
}

function Test-UdpPortAvailable {
    param([int]$RequestedPort)

    $client = New-Object System.Net.Sockets.UdpClient(
        [System.Net.Sockets.AddressFamily]::InterNetwork)
    try {
        $endpoint = New-Object System.Net.IPEndPoint(
            [System.Net.IPAddress]::Loopback,
            $RequestedPort)
        $client.Client.Bind($endpoint)
        return $true
    }
    catch {
        return $false
    }
    finally {
        $client.Close()
    }
}

function Quote-ProcessArgument {
    param([string]$Value)
    if ($Value.Contains('"')) {
        throw 'A process argument contains an unsupported quote character.'
    }
    return '"{0}"' -f $Value
}

function New-ServerArguments {
    param(
        [string]$GameDirectory,
        [string]$MapName,
        [string]$Address,
        [int]$SelectedPort,
        [string]$ShutdownRequestPath)

    $arguments = @(
        '--gamedir', (Quote-ProcessArgument $GameDirectory),
        '--dedicated',
        '--deathmatch', '1',
        '--maxclients', '1',
        '--map', $MapName,
        '--frames', '1',
        '--log-to-file', '0',
        '--log-summary-file', '0',
        '--log-disable-categories=general',
        '--ip', $Address,
        '--port', [string]$SelectedPort,
        '--goldsrc-pmove',
        '--goldsrc-snapshot-rate-hz=20',
        '--goldsrc-handshake-timeout-ms', '300000'
    )
    if ($BoundedObservation) {
        $arguments += '--goldsrc-pmove-observation-ms=60000'
    } else {
        $arguments += @(
            '--goldsrc-pmove-persistent',
            '--goldsrc-manual-shutdown-file',
            (Quote-ProcessArgument $ShutdownRequestPath))
    }
    return @($arguments + @(Convert-AdditionalArguments $AdditionalServerArguments))
}

function New-ClientArguments {
    param([string]$Address, [int]$SelectedPort)

    $arguments = @('-steam', '-game', 'valve', '-console', '-novid')
    if (-not $Fullscreen) {
        $arguments += @(
            '-windowed', '-w', [string]$WindowWidth,
            '-h', [string]$WindowHeight)
    }
    $arguments += @('+connect', ('{0}:{1}' -f $Address, $SelectedPort))
    return @($arguments + @(Convert-AdditionalArguments $AdditionalClientArguments))
}

function Wait-HlhostReady {
    param(
        [System.Diagnostics.Process]$Process,
        [string]$StdoutPath,
        [string]$ExpectedAddress,
        [int]$ExpectedPort,
        [int]$TimeoutSeconds
    )

    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    while ([DateTime]::UtcNow -lt $deadline) {
        if ($Process.HasExited) {
            throw ("hlhost exited with code {0} before readiness." -f $Process.ExitCode)
        }
        if (Test-Path -LiteralPath $StdoutPath -PathType Leaf) {
            $line = Select-String -LiteralPath $StdoutPath `
                -SimpleMatch 'goldsrc_udp_ready:' -ErrorAction SilentlyContinue |
                Select-Object -Last 1
            if ($null -ne $line) {
                $match = [regex]::Match(
                    $line.Line,
                    'address=([^,]+),port=([0-9]+)')
                if (-not $match.Success -or
                    $match.Groups[1].Value -cne $ExpectedAddress -or
                    [int]$match.Groups[2].Value -ne $ExpectedPort) {
                    throw 'hlhost readiness endpoint did not match the requested loopback endpoint.'
                }
                return
            }
        }
        Start-Sleep -Milliseconds 100
    }
    throw 'Timed out waiting for the hlhost readiness marker.'
}

function Wait-UdpEndpointOwnership {
    param(
        [System.Diagnostics.Process]$Process,
        [string]$Address,
        [int]$SelectedPort,
        [int]$TimeoutSeconds
    )

    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    $udpCommand = Get-Command Get-NetUDPEndpoint -ErrorAction SilentlyContinue
    $netstatCommand = Get-Command netstat.exe -ErrorAction SilentlyContinue
    while ([DateTime]::UtcNow -lt $deadline) {
        if ($Process.HasExited) {
            throw 'hlhost exited while UDP endpoint ownership was being verified.'
        }
        if ($null -ne $udpCommand) {
            try {
                foreach ($row in @(Get-NetUDPEndpoint -LocalPort $SelectedPort -ErrorAction Stop)) {
                    if ([int]$row.OwningProcess -eq $Process.Id -and
                        [string]$row.LocalAddress -ceq $Address) {
                        return
                    }
                }
            }
            catch {
                $udpCommand = $null
            }
        }
        if ($null -ne $netstatCommand) {
            foreach ($line in @(& $netstatCommand.Source -ano -p UDP 2>$null)) {
                $match = [regex]::Match(
                    $line,
                    '^\s*UDP\s+(\S+)\s+\*:\*\s+([0-9]+)\s*$')
                if ($match.Success -and
                    $match.Groups[1].Value -ceq ('{0}:{1}' -f $Address, $SelectedPort) -and
                    [int]$match.Groups[2].Value -eq $Process.Id) {
                    return
                }
            }
        }
        Start-Sleep -Milliseconds 100
    }
    throw 'The launched hlhost process did not own the expected localhost UDP endpoint.'
}

function Stop-OwnedProcess {
    param(
        [System.Diagnostics.Process]$Process,
        [string]$Description,
        [string]$ShutdownRequestPath = '')

    if ($null -eq $Process) { return 'not_started' }
    try { $Process.Refresh() } catch { return 'already_exited' }
    if ($Process.HasExited) { return 'already_exited' }

    $shutdownRequested = $false
    try {
        if (-not [string]::IsNullOrWhiteSpace($ShutdownRequestPath)) {
            [System.IO.File]::WriteAllText($ShutdownRequestPath, 'shutdown')
            $shutdownRequested = $true
            if ($Process.WaitForExit(8000)) { return 'graceful_request' }
        }
        if ($Process.CloseMainWindow()) {
            if ($Process.WaitForExit(2000)) { return 'graceful' }
        }
        Stop-Process -Id $Process.Id -Force -ErrorAction Stop
        if (-not $Process.WaitForExit(5000)) {
            throw ("{0} did not exit after bounded cleanup." -f $Description)
        }
        return 'stopped'
    }
    catch {
        throw ("Failed to stop the owned {0} process: {1}" -f $Description, $_.Exception.Message)
    }
    finally {
        if ($shutdownRequested -and
            (Test-Path -LiteralPath $ShutdownRequestPath -PathType Leaf)) {
            [System.IO.File]::Delete($ShutdownRequestPath)
        }
    }
}

function Get-IncrementalLogLines {
    param([string]$PathValue, [hashtable]$State)

    if (-not (Test-Path -LiteralPath $PathValue -PathType Leaf)) { return @() }
    $text = ''
    $stream = [System.IO.File]::Open(
        $PathValue,
        [System.IO.FileMode]::Open,
        [System.IO.FileAccess]::Read,
        [System.IO.FileShare]::ReadWrite)
    try {
        if ([long]$State.Offset -gt $stream.Length) { $State.Offset = 0L }
        [void]$stream.Seek([long]$State.Offset, [System.IO.SeekOrigin]::Begin)
        $reader = New-Object System.IO.StreamReader($stream)
        try {
            $text = $reader.ReadToEnd()
            $State.Offset = $stream.Length
        }
        finally { $reader.Dispose() }
    }
    finally { $stream.Dispose() }
    if ([string]::IsNullOrWhiteSpace($text)) { return @() }
    return @(($text -split "`r?`n") |
        Where-Object { -not [string]::IsNullOrWhiteSpace($_) } |
        Select-Object -Last 20)
}

function Write-ManualChecklist {
    Write-Host ''
    Write-Host 'Manual PM_Move test'
    Write-Host '-------------------'
    Write-Host '1. Wait for the map and player to load.'
    Write-Host '2. Stand still for 10 seconds. Expected: grounded; no fall-through.'
    Write-Host '3. Press W and S. Expected: authoritative forward/backward movement.'
    Write-Host '4. Press A and D. Expected: authoritative strafe movement.'
    Write-Host '5. Release movement keys. Expected: Half-Life friction stops the player.'
    Write-Host '6. Press Space. Expected: jump, gravity, landing, grounding restored.'
    Write-Host '7. Hold/release Ctrl. Expected: duck/unduck with view and hull change.'
    Write-Host '8. Walk into a wall. Expected: static BSP blocks penetration.'
    Write-Host '9. Try a low ceiling. Expected: blocked unduck stays crouched.'
    Write-Host '10. Move for at least 30 seconds. Expected: no snap-back/disconnect.'
    Write-Host '11. Optional reconnect. Expected: no stale position or velocity.'
    Write-Host ''
    Write-Host 'Known limitations: no weapons/firing, damage, item pickup, moving'
    Write-Host 'platforms, ladders, complete water movement, player collision, or combat.'
}

function Write-SessionMetadata {
    param([System.Collections.IDictionary]$Session, [string]$Directory)

    $jsonPath = Join-Path $Directory 'session.json'
    $summaryPath = Join-Path $Directory 'session-summary.txt'
    $Session | ConvertTo-Json -Depth 4 |
        Set-Content -LiteralPath $jsonPath -Encoding UTF8
    @(
        'Manual PM_Move session',
        ('start_utc={0}' -f $Session.start_utc),
        ('end_utc={0}' -f $Session.end_utc),
        ('repository_head={0}' -f $Session.repository_head),
        ('server_executable={0}' -f $Session.server_executable),
        ('server_timestamp_utc={0}' -f $Session.server_timestamp_utc),
        ('map={0}' -f $Session.map),
        ('session_mode={0}' -f $Session.session_mode),
        ('address={0}:{1}' -f $Session.bind_address, $Session.port),
        ('server_pid={0}' -f $Session.server_pid),
        ('client_pid={0}' -f $Session.client_pid),
        ('client_version={0}' -f $Session.client_version),
        ('server_exit_code={0}' -f $Session.server_exit_code),
        ('client_exit_code={0}' -f $Session.client_exit_code),
        ('server_cleanup={0}' -f $Session.server_cleanup),
        ('client_cleanup={0}' -f $Session.client_cleanup),
        ('result={0}' -f $Session.result)
    ) | Set-Content -LiteralPath $summaryPath -Encoding UTF8
}

$repositoryRoot = Resolve-RepositoryRoot
if ($BindAddress -cne '127.0.0.1') {
    throw 'Manual PM_Move testing is restricted to 127.0.0.1.'
}
Assert-SafeAdditionalArguments -Arguments $AdditionalServerArguments `
    -ReservedPrefixes @(
        '--gamedir', '--dedicated', '--deathmatch', '--maxclients', '--map',
        '--frames', '--log-to-file', '--log-summary-file',
        '--log-disable-categories', '--ip', '-ip', '--port', '-port',
        '--goldsrc-pmove', '--goldsrc-pmove-observation-ms',
        '--goldsrc-pmove-persistent', '--goldsrc-manual-shutdown-file',
        '--goldsrc-snapshot-rate-hz', '--goldsrc-handshake-timeout-ms') `
    -Kind 'server'
Assert-SafeAdditionalArguments -Arguments $AdditionalClientArguments `
    -ReservedPrefixes @(
        '-steam', '-game', '-console', '-novid', '-windowed', '-w', '-h',
        '+connect') -Kind 'client'

$cmake = Resolve-CMakeExecutable
$ctest = if ($RunTests) { Resolve-CTestExecutable -CMakeExecutable $cmake } else { '' }
$buildDirectory = Join-Path $repositoryRoot 'out\build\vs2022-win32-reference-sdk'
$head = (& git -C $repositoryRoot rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0) { throw 'Unable to resolve repository HEAD.' }

$clientPath = ''
$clientGamePath = ''
if (-not $ServerOnly) {
    $clientPath = Resolve-HalfLifeClient -RequestedPath $ClientExecutablePath
    $clientGamePath = Resolve-ClientGameDirectory `
        -RequestedPath $ClientGameDir -ClientPath $clientPath -MapName $Map
}
$serverGamePath = Resolve-ServerGameDirectory `
    -RequestedPath $ServerGameDir -MapName $Map `
    -ResolvedClientPath $clientPath -RequestedClientGameDir $ClientGameDir

$automaticPort = $Port -eq 0
$selectedPort = if ($automaticPort) { Get-AvailableUdpPort } else { $Port }
if (-not $automaticPort -and -not (Test-UdpPortAvailable -RequestedPort $selectedPort)) {
    throw ("UDP port {0} is already in use on 127.0.0.1." -f $selectedPort)
}

$timestamp = [DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss-fff')
$logPath = if ([string]::IsNullOrWhiteSpace($LogDirectory)) {
    Join-Path ([System.IO.Path]::GetTempPath()) `
        (Join-Path 'HL-Engine\manual-pmove' ($timestamp + '-' + [Guid]::NewGuid().ToString('N').Substring(0, 8)))
} else {
    [System.IO.Path]::GetFullPath($LogDirectory)
}
if (Test-PathInsideRoot -PathValue $logPath -Root $repositoryRoot) {
    throw 'Manual session logs must be stored outside the repository.'
}
$shutdownRequestPath = Join-Path $logPath 'shutdown.request'
$sessionMode = if ($BoundedObservation) {
    'bounded_observation'
} else {
    'persistent'
}

$canonicalTreeValid = Test-CanonicalBuildTree `
    -BuildDirectory $buildDirectory -RepositoryRoot $repositoryRoot
$wouldConfigure = -not $canonicalTreeValid

if ($DryRun) {
    $serverPath = Resolve-HlhostExecutable `
        -RequestedPath $ServerExecutablePath -RepositoryRoot $repositoryRoot `
        -BuildDirectory $buildDirectory -BuildConfiguration $Configuration
    $serverArguments = New-ServerArguments `
        -GameDirectory $serverGamePath -MapName $Map `
        -Address $BindAddress -SelectedPort $selectedPort `
        -ShutdownRequestPath $shutdownRequestPath
    $clientArguments = if ($ServerOnly) { @() } else {
        New-ClientArguments -Address $BindAddress -SelectedPort $selectedPort
    }
    Write-Host 'Manual PM_Move dry run'
    Write-Host 'server_executable=<resolved-hlhost.exe>'
    Write-Host ('server_timestamp_utc={0:o}' -f (Get-Item -LiteralPath $serverPath).LastWriteTimeUtc)
    $displayServerArguments = $serverArguments `
        -replace [regex]::Escape($serverGamePath), '<server-gamedir>' `
        -replace [regex]::Escape($shutdownRequestPath), '<shutdown-request-file>'
    Write-Host ('server_arguments={0}' -f ($displayServerArguments -join ' '))
    if (-not $ServerOnly) {
        Write-Host 'client_executable=<resolved-hl.exe>'
        Write-Host ('client_arguments={0}' -f ($clientArguments -join ' '))
    }
    Write-Host ('address={0}:{1}' -f $BindAddress, $selectedPort)
    Write-Host ('map={0}' -f $Map)
    Write-Host ('session_mode={0}' -f $sessionMode)
    Write-Host ('logs={0}' -f $logPath)
    Write-Host ('configure_would_run={0}' -f $wouldConfigure.ToString().ToLowerInvariant())
    Write-Host ('build_would_run={0}' -f (-not $SkipBuild).ToString().ToLowerInvariant())
    Write-Host ('cleanup=owned-server-only; client={0}' -f $(if ($StopClientOnExit) { 'owned-direct-client' } else { 'leave-running' }))
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
        Invoke-NativeLogged -Executable $cmake `
            -Arguments @('-S', $repositoryRoot, '-B', $buildDirectory, '-G', 'Visual Studio 17 2022', '-A', 'Win32') `
            -OutputPath $buildLogPath -Description 'CMake configure'
    }
    Invoke-NativeLogged -Executable $cmake `
        -Arguments @('--build', $buildDirectory, '--config', $Configuration, '--target', 'hlhost', '--', '/m:1') `
        -OutputPath $buildLogPath -Description 'hlhost build'
}

$serverPath = Resolve-HlhostExecutable `
    -RequestedPath $ServerExecutablePath -RepositoryRoot $repositoryRoot `
    -BuildDirectory $buildDirectory -BuildConfiguration $Configuration
if ($RunTests) {
    Invoke-NativeLogged -Executable $ctest `
        -Arguments @('--test-dir', $buildDirectory, '-C', $Configuration, '--output-on-failure') `
        -OutputPath $testLogPath -Description 'CTest'
    Write-Host 'CTest passed.'
}

$session = [ordered]@{
    timestamp = $timestamp
    repository_head = $head
    server_executable = $serverPath
    server_timestamp_utc = (Get-Item -LiteralPath $serverPath).LastWriteTimeUtc.ToString('o')
    map = $Map
    session_mode = $sessionMode
    bind_address = $BindAddress
    port = $selectedPort
    server_pid = $null
    client_pid = $null
    client_version = if ($ServerOnly) { $null } else { [System.Diagnostics.FileVersionInfo]::GetVersionInfo($clientPath).FileVersion }
    start_utc = [DateTime]::UtcNow.ToString('o')
    end_utc = $null
    server_exit_code = $null
    client_exit_code = $null
    server_cleanup = 'not_started'
    client_cleanup = 'not_started'
    result = 'running'
}

$serverProcess = $null
$ownedClientProcess = $null
$preexistingClientIds = @(
    Get-Process -Name 'hl' -ErrorAction SilentlyContinue |
        ForEach-Object { $_.Id })
$failure = $null
$normalCompletion = $false
$followState = @{ Offset = 0L }

try {
    $launchAttempts = if ($automaticPort) { 2 } else { 1 }
    for ($attempt = 1; $attempt -le $launchAttempts; ++$attempt) {
        if ($attempt -gt 1) {
            $selectedPort = Get-AvailableUdpPort
            $session.port = $selectedPort
            [System.IO.File]::WriteAllText($stdoutPath, '')
            [System.IO.File]::WriteAllText($stderrPath, '')
        }
        $serverArguments = New-ServerArguments `
            -GameDirectory $serverGamePath -MapName $Map `
            -Address $BindAddress -SelectedPort $selectedPort `
            -ShutdownRequestPath $shutdownRequestPath
        try {
            $serverProcess = Start-Process -FilePath $serverPath `
                -ArgumentList $serverArguments `
                -WorkingDirectory (Split-Path -Parent $serverPath) `
                -RedirectStandardOutput $stdoutPath `
                -RedirectStandardError $stderrPath `
                -WindowStyle Hidden -PassThru
            $session.server_pid = $serverProcess.Id
            Wait-HlhostReady -Process $serverProcess -StdoutPath $stdoutPath `
                -ExpectedAddress $BindAddress -ExpectedPort $selectedPort `
                -TimeoutSeconds $ReadinessTimeoutSeconds
            Wait-UdpEndpointOwnership -Process $serverProcess `
                -Address $BindAddress -SelectedPort $selectedPort `
                -TimeoutSeconds $ReadinessTimeoutSeconds
            break
        }
        catch {
            if ($null -ne $serverProcess) {
                $session.server_cleanup = Stop-OwnedProcess `
                    -Process $serverProcess -Description 'server' `
                    -ShutdownRequestPath $(if ($BoundedObservation) {
                        ''
                    } else {
                        $shutdownRequestPath
                    })
                $serverProcess.Dispose()
                $serverProcess = $null
            }
            if ($attempt -ge $launchAttempts) { throw }
            Write-Host 'Initial automatic port launch failed; retrying once.'
        }
    }

    Write-Host 'Server ready'
    Write-Host ('Address: {0}:{1}' -f $BindAddress, $selectedPort)
    Write-Host ('Map: {0}' -f $Map)
    Write-Host ('PID: {0}' -f $serverProcess.Id)
    Write-Host ('Server build: {0:o}' -f (Get-Item -LiteralPath $serverPath).LastWriteTimeUtc)
    Write-Host ('Logs: {0}' -f $logPath)
    if ($BoundedObservation) {
        Write-Host 'Bounded PM_Move observation session'
    } else {
        Write-Host 'Persistent manual PM_Move session'
        Write-Host 'The server remains active until Enter or Ctrl+C.'
    }

    if (-not $ServerOnly) {
        $clientArguments = New-ClientArguments `
            -Address $BindAddress -SelectedPort $selectedPort
        try {
            $launchedClient = Start-Process -FilePath $clientPath `
                -ArgumentList $clientArguments `
                -WorkingDirectory (Split-Path -Parent $clientPath) `
                -PassThru
            Start-Sleep -Milliseconds 250
            try { $launchedClient.Refresh() } catch { }
            if (-not $launchedClient.HasExited -and
                $launchedClient.ProcessName -ieq 'hl' -and
                $preexistingClientIds -notcontains $launchedClient.Id) {
                $ownedClientProcess = $launchedClient
                $session.client_pid = $launchedClient.Id
            } else {
                $launchedClient.Dispose()
            }
            Write-Host 'Half-Life client launch requested.'
        }
        catch {
            Write-Warning 'Automatic client launch failed; the server remains available.'
            Write-Host ('In the Half-Life console: connect {0}:{1}' -f $BindAddress, $selectedPort)
            Write-Host "Retry with -ClientExecutablePath '<path-to-hl.exe>'."
        }
        Write-ManualChecklist
    }

    $deadline = if ($DurationSeconds -gt 0) {
        [DateTime]::UtcNow.AddSeconds($DurationSeconds)
    } else { [DateTime]::MaxValue }
    $readTask = $null
    if ($DurationSeconds -eq 0) {
        Write-Host ''
        Write-Host $(if ($BoundedObservation) {
            'Press Enter to stop the bounded server session.'
        } else {
            'Press Enter to stop the persistent server session.'
        })
        Write-Host 'Press Ctrl+C to abort and clean up.'
        $readTask = [Console]::In.ReadLineAsync()
    }

    while ([DateTime]::UtcNow -lt $deadline) {
        if ($serverProcess.HasExited) {
            throw ("The server exited unexpectedly with code {0}. See the bounded external logs." -f $serverProcess.ExitCode)
        }
        if ($DurationSeconds -eq 0 -and $readTask.IsCompleted) { break }
        if ($FollowServerLog) {
            foreach ($line in @(Get-IncrementalLogLines -PathValue $stdoutPath -State $followState)) {
                Write-Host $line
            }
        }
        Start-Sleep -Milliseconds 250
    }
    $normalCompletion = $true
    $session.result = 'pass'
}
catch {
    $failure = $_
    $session.result = 'fail'
    Write-Host 'Manual PM_Move launcher failed. Recent server output:'
    Get-Content -LiteralPath $stdoutPath -Tail 20 -ErrorAction SilentlyContinue |
        ForEach-Object { Write-Host $_ }
    Get-Content -LiteralPath $stderrPath -Tail 20 -ErrorAction SilentlyContinue |
        ForEach-Object { Write-Host $_ }
}
finally {
    if ($null -ne $ownedClientProcess) {
        if ($StopClientOnExit) {
            try {
                $session.client_cleanup = Stop-OwnedProcess `
                    -Process $ownedClientProcess -Description 'client'
            }
            catch {
                $session.client_cleanup = 'failed'
                if ($null -eq $failure) { $failure = $_ }
            }
        } else {
            $session.client_cleanup = 'left_running'
        }
        try {
            if ($ownedClientProcess.HasExited) {
                $session.client_exit_code = $ownedClientProcess.ExitCode
            }
        } catch { }
        $ownedClientProcess.Dispose()
    }

    if ($null -ne $serverProcess) {
        $keep = $KeepServerRunning -and $normalCompletion
        if ($keep) {
            $session.server_cleanup = 'kept_running'
        } else {
            try {
                $session.server_cleanup = Stop-OwnedProcess `
                    -Process $serverProcess -Description 'server' `
                    -ShutdownRequestPath $(if ($BoundedObservation) {
                        ''
                    } else {
                        $shutdownRequestPath
                    })
            }
            catch {
                $session.server_cleanup = 'failed'
                if ($null -eq $failure) { $failure = $_ }
            }
        }
        try {
            if ($serverProcess.HasExited) {
                $session.server_exit_code = $serverProcess.ExitCode
            }
        } catch { }
        $serverProcess.Dispose()
    }
    $session.end_utc = [DateTime]::UtcNow.ToString('o')
    try { Write-SessionMetadata -Session $session -Directory $logPath }
    catch {
        if ($null -eq $failure) { $failure = $_ }
    }
}

if ($null -ne $failure) {
    Write-Error $failure.Exception.Message
    exit 1
}
Write-Host ('Session complete. Logs: {0}' -f $logPath)
exit 0
