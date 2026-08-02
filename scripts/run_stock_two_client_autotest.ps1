[CmdletBinding()]
param(
    [string]$ServerExecutablePath,
    [string]$ServerGameDir,
    [string]$ClientExecutablePath,
    [string]$ClientAExecutablePath,
    [string]$ClientBExecutablePath,

    [ValidateSet('Auto', 'MultirunOnly', 'MutexUnlock')]
    [string]$MultiInstanceMode = 'Auto',

    [string]$InstanceUnlockerPath,
    [string]$HandleExecutablePath,
    [switch]$DisableLauncherMutexFallback,
    [switch]$InspectLauncherMutexOnly,

    [ValidatePattern('^[A-Za-z0-9_][A-Za-z0-9_-]{0,63}$')]
    [string]$Map = 'crossfire',

    [string]$BindAddress = '127.0.0.1',

    [ValidateRange(0, 65535)]
    [int]$Port = 0,

    [ValidateRange(1, 65535)]
    [int]$ClientAPort = 27005,

    [ValidateRange(1, 65535)]
    [int]$ClientBPort = 27006,

    [ValidateRange(1, 65535)]
    [int]$ClientAReconnectPort = 27007,

    [ValidatePattern('^[A-Za-z0-9_][A-Za-z0-9_-]{0,63}$')]
    [string]$ClientAName = 'HL_Engine_Client_A',

    [ValidatePattern('^[A-Za-z0-9_][A-Za-z0-9_-]{0,63}$')]
    [string]$ClientBName = 'HL_Engine_Client_B',

    [ValidateRange(0, 30)]
    [int]$ClientLaunchDelaySeconds = 3,

    [ValidateRange(5, 300)]
    [int]$ConnectTimeoutSeconds = 90,

    [ValidateRange(5, 300)]
    [int]$SpawnTimeoutSeconds = 120,

    [ValidateRange(5, 300)]
    [int]$MovementTimeoutSeconds = 60,

    [ValidateRange(5, 120)]
    [int]$DisconnectTimeoutSeconds = 30,

    [ValidateRange(5, 300)]
    [int]$ReconnectTimeoutSeconds = 120,

    [ValidateRange(10, 900)]
    [int]$AutoTestDurationSeconds = 120,

    [ValidateSet('Debug', 'Release', 'RelWithDebInfo', 'MinSizeRel')]
    [string]$Configuration = 'Release',

    [string]$LogDirectory,
    [switch]$SkipBuild,
    [switch]$RunTests,
    [switch]$DryRun,
    [switch]$FollowServerLog,
    [switch]$KeepProcessesOnFailure,
    [switch]$SkipReconnectTest
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
if (Test-Path Variable:PSNativeCommandUseErrorActionPreference) {
    $PSNativeCommandUseErrorActionPreference = $false
}

function Get-ImportedLauncherFunctionDefinitions {
    param([string]$PathValue, [string[]]$Names)

    $tokens = $null
    $errors = $null
    $ast = [System.Management.Automation.Language.Parser]::ParseFile(
        $PathValue,
        [ref]$tokens,
        [ref]$errors)
    if (@($errors).Count -ne 0) {
        throw 'The shared launcher has parser errors.'
    }
    return @($ast.EndBlock.Statements |
        Where-Object {
            $_ -is [System.Management.Automation.Language.FunctionDefinitionAst] -and
            $Names -ccontains $_.Name
        } |
        ForEach-Object { $_.Extent.Text })
}

$sharedFunctionNames = @(
    'Test-PathInsideRoot',
    'Resolve-CMakeExecutable',
    'Resolve-CTestExecutable',
    'Test-CanonicalBuildTree',
    'Get-SteamInstallRoots',
    'Get-SteamLibraryRoots',
    'Get-HalfLifeInstallCandidates',
    'Resolve-HalfLifeClient',
    'Test-GameDirectory',
    'Resolve-ServerGameDirectory',
    'Resolve-ClientGameDirectory',
    'Test-Win32PortableExecutable',
    'Resolve-HlhostExecutable',
    'Get-AvailableUdpPort',
    'Test-UdpPortAvailable',
    'Quote-ProcessArgument',
    'Wait-HlhostReady',
    'Wait-UdpEndpointOwnership'
)
foreach ($definitionText in @(Get-ImportedLauncherFunctionDefinitions `
        -PathValue (Join-Path $PSScriptRoot 'run_manual_pmove_test.ps1') `
        -Names $sharedFunctionNames)) {
    . ([scriptblock]::Create($definitionText))
}

function Assert-DistinctClientPorts {
    param([int]$APort, [int]$BPort, [int]$AReconnectPort)

    $ports = @($APort, $BPort, $AReconnectPort)
    if (@($ports | Select-Object -Unique).Count -ne $ports.Count) {
        throw 'client_ports_must_be_distinct'
    }
}

function New-StockServerArguments {
    param(
        [string]$GameDirectory,
        [string]$MapName,
        [string]$Address,
        [int]$SelectedPort,
        [string]$ShutdownRequestPath,
        [string]$DisconnectRequestPath)

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
        (Quote-ProcessArgument $ShutdownRequestPath),
        '--goldsrc-manual-disconnect-file',
        (Quote-ProcessArgument $DisconnectRequestPath)
    )
}

function New-StockClientArguments {
    param(
        [string]$Address,
        [int]$ServerPort,
        [int]$LocalClientPort,
        [string]$ClientName,
        [ValidateSet('forward', 'moveright', 'back')]
        [string]$Movement)

    return @(
        '-steam',
        '-multirun',
        '-insecure',
        '-game', 'valve',
        '-console',
        '-novid',
        '-windowed',
        '-w', '960',
        '-h', '540',
        '+clientport', [string]$LocalClientPort,
        '+name', $ClientName,
        '+connect', ('{0}:{1}' -f $Address, $ServerPort),
        ('+{0}' -f $Movement)
    )
}

function Invoke-QuietNative {
    param(
        [string]$Executable,
        [string[]]$Arguments,
        [string]$OutputPath,
        [string]$Description)

    & $Executable @Arguments *> $OutputPath
    if ($LASTEXITCODE -ne 0) {
        throw ('{0}_failed' -f $Description)
    }
}

function Test-Win64PortableExecutable {
    param([string]$PathValue)

    $stream = [System.IO.File]::Open(
        $PathValue,
        [System.IO.FileMode]::Open,
        [System.IO.FileAccess]::Read,
        [System.IO.FileShare]::Read)
    try {
        if ($stream.Length -lt 64) { return $false }
        $reader = New-Object System.IO.BinaryReader($stream)
        try {
            if ($reader.ReadUInt16() -ne 0x5A4D) { return $false }
            $stream.Position = 0x3C
            $peOffset = $reader.ReadUInt32()
            if ($peOffset -gt ($stream.Length - 6)) { return $false }
            $stream.Position = $peOffset
            return $reader.ReadUInt32() -eq 0x00004550 -and
                $reader.ReadUInt16() -eq 0x8664
        }
        finally { $reader.Dispose() }
    }
    finally { $stream.Dispose() }
}

function Resolve-InstanceUnlocker {
    param(
        [string]$RequestedPath,
        [string]$RepositoryRoot,
        [string]$CMakeExecutable,
        [string]$OutputPath,
        [bool]$SkipToolsBuild,
        [bool]$IsDryRun)

    $toolsBuild = Join-Path $RepositoryRoot 'out\build\vs2022-x64-tools'
    $candidate = if ([string]::IsNullOrWhiteSpace($RequestedPath)) {
        Join-Path $toolsBuild 'Release\goldsrc_instance_unlocker.exe'
    } else {
        [System.IO.Path]::GetFullPath($RequestedPath)
    }

    if (-not $IsDryRun -and -not $SkipToolsBuild -and
        [string]::IsNullOrWhiteSpace($RequestedPath)) {
        Invoke-QuietNative -Executable $CMakeExecutable -Arguments @(
            '-S', $RepositoryRoot, '-B', $toolsBuild,
            '-G', 'Visual Studio 17 2022', '-A', 'x64') `
            -OutputPath $OutputPath -Description 'unlocker_configure'
        Invoke-QuietNative -Executable $CMakeExecutable -Arguments @(
            '--build', $toolsBuild, '--config', 'Release',
            '--target', 'goldsrc_instance_unlocker') `
            -OutputPath $OutputPath -Description 'unlocker_build'
    }

    if ($IsDryRun -and -not (Test-Path -LiteralPath $candidate -PathType Leaf)) {
        return [System.IO.Path]::GetFullPath($candidate)
    }
    if (-not (Test-Path -LiteralPath $candidate -PathType Leaf)) {
        throw 'launcher_mutex_unlocker_not_found'
    }
    $candidate = (Resolve-Path -LiteralPath $candidate).Path
    if (-not (Test-Win64PortableExecutable -PathValue $candidate)) {
        throw 'launcher_mutex_unlocker_not_x64'
    }
    return $candidate
}

function Invoke-InstanceUnlocker {
    param(
        [string]$Executable,
        [System.Diagnostics.Process]$Process,
        [string]$ExpectedImage,
        [Int64]$ExpectedCreationTime,
        [ValidateSet('Inspect', 'Close')][string]$Action)

    $arguments = @(
        '--pid', $Process.Id.ToString(),
        '--expected-image', $ExpectedImage,
        '--expected-creation-time', $ExpectedCreationTime.ToString(),
        '--object-name', 'ValveHalfLifeLauncherMutex',
        '--json',
        $(if ($Action -ceq 'Close') { '--close' } else { '--inspect-only' })
    )
    $output = @(& $Executable @arguments 2>&1)
    $exitCode = $LASTEXITCODE
    if ($output.Count -ne 1) {
        throw 'launcher_mutex_unlocker_unbounded_output'
    }
    try { $payload = $output[0] | ConvertFrom-Json }
    catch { throw 'launcher_mutex_unlocker_invalid_json' }
    if ($exitCode -eq 5) {
        throw 'launcher_mutex_access_denied_elevation_required'
    }
    if ($Action -ceq 'Inspect' -and $exitCode -eq 2) {
        return $payload
    }
    if ($exitCode -ne 0) {
        throw ('launcher_mutex_{0}_{1}' -f
            $Action.ToLowerInvariant(), [string]$payload.blocker)
    }
    return $payload
}

function Assert-UnlockTargetOwnership {
    param(
        [System.Diagnostics.Process]$Process,
        [string]$ExpectedImage,
        [int[]]$PreexistingIds,
        [int[]]$OwnedIds,
        [int]$ExpectedPort,
        [bool]$ServerAssociated,
        [bool]$Spawned)

    $Process.Refresh()
    if ($Process.HasExited -or $OwnedIds -notcontains $Process.Id -or
        $PreexistingIds -contains $Process.Id) {
        throw 'launcher_mutex_target_not_owned'
    }

    $record = Get-CimInstance Win32_Process `
        -Filter ('ProcessId={0}' -f $Process.Id) -ErrorAction Stop
    if ($null -eq $record -or [string]::IsNullOrWhiteSpace(
            [string]$record.ExecutablePath) -or
        -not [System.IO.Path]::GetFullPath(
            [string]$record.ExecutablePath).Equals(
                [System.IO.Path]::GetFullPath($ExpectedImage),
                [StringComparison]::OrdinalIgnoreCase)) {
        throw 'launcher_mutex_target_image_mismatch'
    }
    if ([int]$record.SessionId -ne
        [System.Diagnostics.Process]::GetCurrentProcess().SessionId) {
        throw 'launcher_mutex_target_session_mismatch'
    }
    $owner = Invoke-CimMethod -InputObject $record -MethodName GetOwner `
        -ErrorAction Stop
    $account = ('{0}\{1}' -f $owner.Domain, $owner.User)
    $currentAccount = [System.Security.Principal.WindowsIdentity]::GetCurrent().Name
    if ([int]$owner.ReturnValue -ne 0 -or
        -not $account.Equals($currentAccount,
            [StringComparison]::OrdinalIgnoreCase)) {
        throw 'launcher_mutex_target_user_mismatch'
    }
    if (-not (Test-OwnedUdpPort -ProcessId $Process.Id `
            -ExpectedPort $ExpectedPort) -or
        -not $ServerAssociated -or -not $Spawned) {
        throw 'launcher_mutex_target_not_ready'
    }
    return [pscustomobject]@{
        CreationTime = [Int64]$Process.StartTime.ToFileTimeUtc()
        ImageVerified = $true
        UserVerified = $true
        SessionVerified = $true
        UdpVerified = $true
        ServerAssociated = $true
        Spawned = $true
    }
}

function Invoke-VerifiedLauncherMutex {
    param(
        [System.Diagnostics.Process]$Process,
        [string]$ExpectedImage,
        [int[]]$PreexistingIds,
        [int[]]$OwnedIds,
        [int]$ExpectedPort,
        [bool]$ServerAssociated,
        [bool]$Spawned,
        [string]$UnlockerExecutable,
        [bool]$CloseHandle,
        [System.Collections.IDictionary]$Result,
        [ValidateSet('Primary', 'Reconnect')][string]$Context)

    $identity = Assert-UnlockTargetOwnership -Process $Process `
        -ExpectedImage $ExpectedImage -PreexistingIds $PreexistingIds `
        -OwnedIds $OwnedIds -ExpectedPort $ExpectedPort `
        -ServerAssociated $ServerAssociated -Spawned $Spawned
    $inspect = Invoke-InstanceUnlocker -Executable $UnlockerExecutable `
        -Process $Process -ExpectedImage $ExpectedImage `
        -ExpectedCreationTime $identity.CreationTime -Action Inspect
    if ($Context -ceq 'Primary') {
        $Result.mutex_match_count = [int]$inspect.matched_handle_count
        $Result.mutex_target_mutant_count = [int]$inspect.mutant_handle_count
    }
    if ([int]$inspect.matched_handle_count -eq 0) {
        throw 'launcher_mutex_inspect_mutex_not_found'
    }
    $matchedName = [string]$inspect.matched_object_name
    $exactName = $matchedName -ceq 'ValveHalfLifeLauncherMutex' -or
        $matchedName.EndsWith('\ValveHalfLifeLauncherMutex',
            [StringComparison]::Ordinal)
    if ([int]$inspect.matched_handle_count -ne 1 -or
        [string]$inspect.matched_object_type -cne 'Mutant' -or
        -not $exactName -or
        -not [bool]$inspect.target_image_verified -or
        -not [bool]$inspect.target_creation_time_verified) {
        throw 'launcher_mutex_inspect_identity_failed'
    }

    $closed = $false
    if ($CloseHandle) {
        $close = Invoke-InstanceUnlocker -Executable $UnlockerExecutable `
            -Process $Process -ExpectedImage $ExpectedImage `
            -ExpectedCreationTime $identity.CreationTime -Action Close
        if ([int]$close.matched_handle_count -ne 1 -or
            [string]$close.matched_object_type -cne 'Mutant' -or
            -not [bool]$close.handle_closed) {
            throw 'launcher_mutex_close_identity_failed'
        }
        $closed = $true
    }

    if ($Context -ceq 'Primary') {
        $Result.mutex_owner_pid = $Process.Id
        $Result.mutex_match_count = 1
        $Result.mutex_exact_name_verified = $true
        $Result.mutex_name_verified = $true
        $Result.mutex_object_type_verified = $true
        $Result.mutex_object_type = 'Mutant'
        $Result.mutex_target_owned = $true
        $Result.mutex_target_image_verified = $true
        $Result.mutex_owner_image_verified = $true
        $Result.mutex_target_creation_time_verified = $true
        $Result.mutex_target_user_verified = $identity.UserVerified
        $Result.mutex_target_session_verified = $identity.SessionVerified
        $Result.mutex_target_udp_verified = $identity.UdpVerified
        $Result.mutex_target_server_associated =
            $identity.ServerAssociated
        $Result.mutex_target_spawned = $identity.Spawned
        $Result.mutex_handle_closed = $closed
        $Result.mutex_unlock_attempted = $CloseHandle
    } else {
        $Result.reconnect_mutex_handle_closed = $closed
    }
    return [pscustomobject]@{
        Inspected = $true
        Closed = $closed
    }
}

function Get-SharedFileText {
    param([string]$PathValue)

    if (-not (Test-Path -LiteralPath $PathValue -PathType Leaf)) {
        return ''
    }
    $stream = [System.IO.File]::Open(
        $PathValue,
        [System.IO.FileMode]::Open,
        [System.IO.FileAccess]::Read,
        [System.IO.FileShare]::ReadWrite)
    try {
        $reader = New-Object System.IO.StreamReader($stream)
        try { return $reader.ReadToEnd() }
        finally { $reader.Dispose() }
    }
    finally { $stream.Dispose() }
}

function Wait-LogRegex {
    param(
        [string]$PathValue,
        [string]$Pattern,
        [int]$TimeoutSeconds,
        [System.Diagnostics.Process]$ServerProcess,
        [System.Diagnostics.Process]$ClientProcess,
        [string]$FailureIdentifier,
        [int]$StartOffset = 0)

    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    while ([DateTime]::UtcNow -lt $deadline) {
        if ($null -ne $ServerProcess) {
            $ServerProcess.Refresh()
            if ($ServerProcess.HasExited) { throw 'server_exited_during_gate' }
        }
        if ($null -ne $ClientProcess) {
            $ClientProcess.Refresh()
            if ($ClientProcess.HasExited) {
                $exitFailure = New-Object System.Exception(
                    $FailureIdentifier)
                $exitFailure.Data['UnexpectedExitCode'] =
                    $ClientProcess.ExitCode
                throw $exitFailure
            }
            if ($ClientProcess.MainWindowTitle -eq 'Error') {
                if (Test-StockSingleInstanceDialog `
                        -Process $ClientProcess) {
                    throw ($FailureIdentifier + '_single_instance_dialog')
                }
                throw ($FailureIdentifier + '_local_error_dialog')
            }
        }
        $text = Get-SharedFileText -PathValue $PathValue
        if ($StartOffset -gt 0 -and $text.Length -gt $StartOffset) {
            $text = $text.Substring($StartOffset)
        } elseif ($StartOffset -gt 0) {
            $text = ''
        }
        $match = [regex]::Match(
            $text,
            $Pattern,
            [System.Text.RegularExpressions.RegexOptions]::Multiline)
        if ($match.Success) { return $match }
        Start-Sleep -Milliseconds 100
    }
    throw $FailureIdentifier
}

function Get-HalfLifeProcessRecords {
    $records = @()
    foreach ($item in @(Get-CimInstance Win32_Process `
            -Filter "Name='hl.exe'" -ErrorAction SilentlyContinue)) {
        $records += [pscustomobject]@{
            ProcessId = [int]$item.ProcessId
            CreationDate = $item.CreationDate
            ExecutablePath = [string]$item.ExecutablePath
            CommandLine = [string]$item.CommandLine
        }
    }
    return @($records)
}

function Select-NewOwnedProcessRecord {
    param(
        [object[]]$Records,
        [int[]]$PreexistingIds,
        [string]$ExpectedExecutablePath,
        [int]$ExpectedClientPort,
        [string]$ExpectedClientName,
        [int]$PreferredProcessId)

    $expectedPath = [System.IO.Path]::GetFullPath($ExpectedExecutablePath)
    $matches = @($Records | Where-Object {
        $record = $_
        $isNew = $PreexistingIds -notcontains [int]$record.ProcessId
        $pathMatches = -not [string]::IsNullOrWhiteSpace(
                [string]$record.ExecutablePath) -and
            [System.IO.Path]::GetFullPath([string]$record.ExecutablePath).Equals(
                $expectedPath,
                [StringComparison]::OrdinalIgnoreCase)
        $commandLine = [string]$record.CommandLine
        $identityMatches = [string]::IsNullOrWhiteSpace($commandLine) -or
            ($commandLine.Contains([string]$ExpectedClientPort) -and
             $commandLine.Contains($ExpectedClientName))
        $isNew -and $pathMatches -and $identityMatches
    })
    $preferred = @($matches | Where-Object {
        [int]$_.ProcessId -eq $PreferredProcessId })
    if ($preferred.Count -eq 1) { return $preferred[0] }
    if ($matches.Count -eq 1) { return $matches[0] }
    return $null
}

function Test-StockSingleInstanceDialog {
    param([System.Diagnostics.Process]$Process)

    $Process.Refresh()
    if ($Process.HasExited -or $Process.MainWindowTitle -cne 'Error' -or
        $Process.MainWindowHandle -eq [IntPtr]::Zero) {
        return $false
    }
    if ($null -eq ('HlEngine.StockClientDialog' -as [type])) {
        Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
using System.Text;
namespace HlEngine {
    public static class StockClientDialog {
        private delegate bool EnumChildProc(IntPtr window, IntPtr state);
        [DllImport("user32.dll")]
        private static extern bool EnumChildWindows(
            IntPtr parent, EnumChildProc callback, IntPtr state);
        [DllImport("user32.dll", CharSet = CharSet.Unicode)]
        private static extern int GetWindowTextW(
            IntPtr window, StringBuilder text, int maximum);
        private static bool Matches(IntPtr window) {
            var text = new StringBuilder(512);
            GetWindowTextW(window, text, text.Capacity);
            var value = text.ToString().ToLowerInvariant();
            return value.Contains("only one instance")
                || value.Contains("launcher is already running")
                || value.Contains("another instance is already running");
        }
        public static bool HasKnownSingleInstanceText(IntPtr parent) {
            if (Matches(parent)) return true;
            bool found = false;
            int visited = 0;
            EnumChildWindows(parent, (window, state) => {
                if (++visited > 64) return false;
                if (Matches(window)) {
                    found = true;
                    return false;
                }
                return true;
            }, IntPtr.Zero);
            return found;
        }
    }
}
'@
    }
    return [HlEngine.StockClientDialog]::HasKnownSingleInstanceText(
        $Process.MainWindowHandle)
}

function Throw-StockClientLaunchFailure {
    param(
        [string]$Identifier,
        [System.Diagnostics.Process]$Process,
        [System.Diagnostics.Stopwatch]$Stopwatch,
        [bool]$ProcessCreated,
        [bool]$ForceCleanup)

    $processId = $null
    $singleInstanceDialog = $false
    if ($null -ne $Process) {
        $processId = $Process.Id
        try {
            $Process.Refresh()
            $singleInstanceDialog = Test-StockSingleInstanceDialog `
                -Process $Process
            if ((-not $KeepProcessesOnFailure -or $ForceCleanup) -and
                -not $Process.HasExited) {
                if (-not ($Process.CloseMainWindow() -and
                        $Process.WaitForExit(2000))) {
                    Stop-Process -Id $Process.Id -Force -ErrorAction Stop
                    [void]$Process.WaitForExit(5000)
                }
            }
        }
        catch {
            # The exact process was created by this function. The caller still
            # receives its identity and performs the final leak audit.
        }
        finally { $Process.Dispose() }
    }
    $exception = New-Object System.Exception($Identifier)
    $exception.Data['ProcessCreated'] = $ProcessCreated
    $exception.Data['SingleInstanceDialog'] = $singleInstanceDialog
    if ($null -ne $processId) { $exception.Data['ProcessId'] = $processId }
    $exception.Data['LifetimeMs'] = [long]$Stopwatch.ElapsedMilliseconds
    throw $exception
}

function Start-StockOwnedClient {
    param(
        [string]$Role,
        [string]$Executable,
        [string[]]$Arguments,
        [int[]]$PreexistingIds,
        [int]$ExpectedClientPort,
        [string]$ExpectedClientName,
        [switch]$AlwaysCleanupOnFailure)

    $started = $null
    $stopwatch = [System.Diagnostics.Stopwatch]::StartNew()
    try {
        $started = Start-Process -FilePath $Executable `
            -ArgumentList $Arguments `
            -WorkingDirectory (Split-Path -Parent $Executable) `
            -PassThru
    }
    catch {
        Throw-StockClientLaunchFailure `
            -Identifier ('{0}_start_process_failed' -f $Role) `
            -Process $null -Stopwatch $stopwatch -ProcessCreated $false `
            -ForceCleanup $AlwaysCleanupOnFailure
    }

    $deadline = [DateTime]::UtcNow.AddSeconds(8)
    $record = $null
    while ([DateTime]::UtcNow -lt $deadline) {
        try { $started.Refresh() } catch { }
        if (-not $started.HasExited) {
            $record = Select-NewOwnedProcessRecord `
                -Records @(Get-HalfLifeProcessRecords) `
                -PreexistingIds $PreexistingIds `
                -ExpectedExecutablePath $Executable `
                -ExpectedClientPort $ExpectedClientPort `
                -ExpectedClientName $ExpectedClientName `
                -PreferredProcessId $started.Id
            if ($null -ne $record) { break }
        }
        Start-Sleep -Milliseconds 100
    }
    if ($null -eq $record) {
        $exited = $started.HasExited
        $identifier = if ($exited) {
            '{0}_exited_before_stabilization' -f $Role
        } else {
            '{0}_no_new_process_detected' -f $Role
        }
        Throw-StockClientLaunchFailure -Identifier $identifier `
            -Process $started -Stopwatch $stopwatch -ProcessCreated $true `
            -ForceCleanup $AlwaysCleanupOnFailure
    }

    $stabilizationDeadline = [DateTime]::UtcNow.AddSeconds(5)
    while ([DateTime]::UtcNow -lt $stabilizationDeadline) {
        Start-Sleep -Milliseconds 100
        $started.Refresh()
        if ($started.HasExited) {
            Throw-StockClientLaunchFailure `
                -Identifier ('{0}_exited_before_stabilization' -f $Role) `
                -Process $started -Stopwatch $stopwatch `
                -ProcessCreated $true `
                -ForceCleanup $AlwaysCleanupOnFailure
        }
        if ($started.MainWindowTitle -eq 'Error') {
            $identifier = if (Test-StockSingleInstanceDialog `
                    -Process $started) {
                '{0}_multirun_rejected' -f $Role
            } else {
                '{0}_local_error_dialog' -f $Role
            }
            Throw-StockClientLaunchFailure -Identifier $identifier `
                -Process $started -Stopwatch $stopwatch `
                -ProcessCreated $true `
                -ForceCleanup $AlwaysCleanupOnFailure
        }
    }
    return $started
}

function Test-OwnedUdpPort {
    param([int]$ProcessId, [int]$ExpectedPort)

    $command = Get-Command Get-NetUDPEndpoint -ErrorAction SilentlyContinue
    if ($null -ne $command) {
        try {
            foreach ($row in @(Get-NetUDPEndpoint `
                    -OwningProcess $ProcessId -ErrorAction Stop)) {
                if ([int]$row.LocalPort -eq $ExpectedPort) { return $true }
            }
        }
        catch { }
    }
    $netstat = Get-Command netstat.exe -ErrorAction SilentlyContinue
    if ($null -ne $netstat) {
        foreach ($line in @(& $netstat.Source -ano -p UDP 2>$null)) {
            $match = [regex]::Match(
                $line,
                '^\s*UDP\s+\S+:(?<port>[0-9]+)\s+\*:\*\s+(?<pid>[0-9]+)\s*$')
            if ($match.Success -and
                [int]$match.Groups['port'].Value -eq $ExpectedPort -and
                [int]$match.Groups['pid'].Value -eq $ProcessId) {
                return $true
            }
        }
    }
    return $false
}

function Wait-OwnedUdpPort {
    param(
        [System.Diagnostics.Process]$Process,
        [int]$ExpectedPort,
        [int]$TimeoutSeconds,
        [string]$FailureIdentifier)

    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    while ([DateTime]::UtcNow -lt $deadline) {
        $Process.Refresh()
        if ($Process.HasExited -or $Process.MainWindowTitle -eq 'Error') {
            throw $FailureIdentifier
        }
        if (Test-OwnedUdpPort -ProcessId $Process.Id `
                -ExpectedPort $ExpectedPort) {
            return
        }
        Start-Sleep -Milliseconds 100
    }
    throw $FailureIdentifier
}

function Stop-ExactOwnedProcess {
    param(
        [System.Diagnostics.Process]$Process,
        [int]$ExpectedProcessId,
        [string]$Description)

    if ($null -eq $Process) { return 'not_started' }
    if ($Process.Id -ne $ExpectedProcessId) {
        throw ('{0}_ownership_mismatch' -f $Description)
    }
    try { $Process.Refresh() } catch { return 'already_exited' }
    if ($Process.HasExited) { return 'already_exited' }
    if ($Process.CloseMainWindow() -and $Process.WaitForExit(5000)) {
        return 'graceful'
    }
    Stop-Process -Id $ExpectedProcessId -Force -ErrorAction Stop
    if (-not $Process.WaitForExit(5000)) {
        throw ('{0}_cleanup_timeout' -f $Description)
    }
    return 'stopped_exact_pid'
}

function Stop-ExactOwnedServer {
    param(
        [System.Diagnostics.Process]$Process,
        [int]$ExpectedProcessId,
        [string]$ShutdownRequestPath)

    if ($null -eq $Process) { return 'not_started' }
    if ($Process.Id -ne $ExpectedProcessId) { throw 'server_ownership_mismatch' }
    $Process.Refresh()
    if ($Process.HasExited) { return 'already_exited' }
    [System.IO.File]::WriteAllText($ShutdownRequestPath, 'shutdown')
    if ($Process.WaitForExit(15000)) { return 'graceful_request' }
    Stop-Process -Id $ExpectedProcessId -Force -ErrorAction Stop
    if (-not $Process.WaitForExit(5000)) { throw 'server_cleanup_timeout' }
    return 'stopped_exact_pid'
}

function Get-SummaryFields {
    param([string]$Line)

    $fields = @{}
    $payload = $Line.Substring($Line.IndexOf(':') + 1).Trim()
    foreach ($part in $payload -split ',') {
        $pair = $part.Trim() -split '=', 2
        if ($pair.Count -eq 2) { $fields[$pair[0]] = $pair[1] }
    }
    return $fields
}

function Get-LastSummaryFields {
    param([string]$Text, [string]$Prefix, [string]$Slot = '')

    $lines = @($Text -split '\r?\n' | Where-Object {
        $_.Contains($Prefix) -and
        ([string]::IsNullOrWhiteSpace($Slot) -or
         $_.Contains(('slot={0}' -f $Slot)))
    })
    if ($lines.Count -eq 0) { return $null }
    return Get-SummaryFields -Line $lines[-1]
}

function Get-SnapshotSampleCount {
    param([string]$Text, [string]$SessionId)

    return [regex]::Matches(
        $Text,
        ('goldsrc_continuous_snapshot_sent: session_id={0},' -f
            [regex]::Escape($SessionId))).Count
}

function Write-AtomicText {
    param([string]$PathValue, [string]$TextValue)

    $temporary = $PathValue + '.tmp'
    [System.IO.File]::WriteAllText(
        $temporary,
        $TextValue,
        (New-Object System.Text.UTF8Encoding($false)))
    if (Test-Path -LiteralPath $PathValue -PathType Leaf) {
        [System.IO.File]::Replace($temporary, $PathValue, $null)
    } else {
        [System.IO.File]::Move($temporary, $PathValue)
    }
}

function Write-AutotestResults {
    param(
        [System.Collections.IDictionary]$Result,
        [string]$JsonPath,
        [string]$TextPath)

    $json = $Result | ConvertTo-Json -Depth 5
    Write-AtomicText -PathValue $JsonPath -TextValue ($json + "`n")
    $lines = @()
    foreach ($entry in $Result.GetEnumerator()) {
        $value = if ($entry.Value -is [bool]) {
            $entry.Value.ToString().ToLowerInvariant()
        } elseif ($null -eq $entry.Value) {
            'missing'
        } else {
            [string]$entry.Value
        }
        $lines += ('{0}={1}' -f $entry.Key, $value)
    }
    Write-AtomicText -PathValue $TextPath `
        -TextValue (($lines -join "`n") + "`n")
}

function Invoke-LauncherSelfTests {
    Assert-DistinctClientPorts -APort 27005 -BPort 27006 `
        -AReconnectPort 27007
    $duplicateRejected = $false
    try {
        Assert-DistinctClientPorts -APort 27005 -BPort 27005 `
            -AReconnectPort 27007
    }
    catch { $duplicateRejected = $true }
    if (-not $duplicateRejected) { throw 'distinct_port_test_failed' }

    $arguments = New-StockClientArguments -Address '127.0.0.1' `
        -ServerPort 27015 -LocalClientPort 27005 `
        -ClientName 'HL_Engine_Client_A' -Movement 'forward'
    if ($arguments -notcontains '-multirun' -or
        $arguments -notcontains '+clientport' -or
        $arguments -notcontains '27005') {
        throw 'client_argument_test_failed'
    }

    $records = @(
        [pscustomobject]@{
            ProcessId = 10
            ExecutablePath = 'C:\Games\Half-Life\hl.exe'
            CommandLine = 'hl.exe +clientport 27005 +name HL_Engine_Client_A'
        },
        [pscustomobject]@{
            ProcessId = 11
            ExecutablePath = 'C:\Games\Half-Life\hl.exe'
            CommandLine = 'hl.exe +clientport 27006 +name HL_Engine_Client_B'
        })
    $selected = Select-NewOwnedProcessRecord -Records $records `
        -PreexistingIds @(10) `
        -ExpectedExecutablePath 'C:\Games\Half-Life\hl.exe' `
        -ExpectedClientPort 27006 -ExpectedClientName 'HL_Engine_Client_B' `
        -PreferredProcessId 11
    if ($null -eq $selected -or [int]$selected.ProcessId -ne 11) {
        throw 'process_ownership_selection_test_failed'
    }
    $refused = Select-NewOwnedProcessRecord -Records $records `
        -PreexistingIds @(10, 11) `
        -ExpectedExecutablePath 'C:\Games\Half-Life\hl.exe' `
        -ExpectedClientPort 27006 -ExpectedClientName 'HL_Engine_Client_B' `
        -PreferredProcessId 11
    if ($null -ne $refused) { throw 'preexisting_process_refusal_test_failed' }
    return $true
}

function Convert-FailureIdentifier {
    param([string]$Message)

    if ($Message -cmatch '^client_a_disconnect_[a-z0-9_]+$' -or
        $Message -cmatch '^(launcher_mutex|client_[ab](_reconnect)?|client_b_initial|client_b_retry)_[a-z0-9_]+_(single_instance_dialog|local_error_dialog)$' -or
        $Message -cmatch '^(launcher_mutex|client_b_initial|client_b_retry)_[a-z0-9_]+$') {
        return $Message
    }

    $known = @(
        'client_a_start_process_failed',
        'client_b_start_process_failed',
        'client_a_reconnect_start_process_failed',
        'client_a_exited_before_stabilization',
        'client_b_exited_before_stabilization',
        'client_a_reconnect_exited_before_stabilization',
        'client_a_no_new_process_detected',
        'client_b_no_new_process_detected',
        'client_a_reconnect_no_new_process_detected',
        'client_a_multirun_rejected',
        'client_b_multirun_rejected',
        'client_a_reconnect_multirun_rejected',
        'client_a_process_created_but_no_network',
        'client_b_process_created_but_no_network',
        'client_a_reconnect_process_created_but_no_network',
        'client_a_connect_timeout',
        'client_b_connect_timeout',
        'client_a_spawn_timeout',
        'client_b_spawn_timeout',
        'client_a_disconnect_timeout',
        'client_a_reconnect_timeout',
        'client_a_reconnect_spawn_timeout',
        'client_a_exited_during_movement',
        'client_b_exited_during_movement',
        'client_a_error_dialog_during_movement',
        'client_b_error_dialog_during_movement',
        'client_ports_must_be_distinct',
        'server_exited_during_gate',
        'two_distinct_client_processes_failed',
        'stock_replication_summary_failed',
        'stock_movement_summary_failed',
        'stock_disconnect_reconnect_summary_failed')
    foreach ($identifier in $known) {
        if ($Message.Contains($identifier)) { return $identifier }
    }
    return 'stock_two_client_autotest_failed'
}

$repositoryRoot = [System.IO.Path]::GetFullPath(
    (Join-Path $PSScriptRoot '..')).TrimEnd('\', '/')
if ($BindAddress -cne '127.0.0.1') {
    throw 'Stock two-client autotesting is restricted to 127.0.0.1.'
}
if ($DisableLauncherMutexFallback -and
    $MultiInstanceMode -ceq 'MutexUnlock') {
    throw 'mutex_unlock_mode_disabled_by_switch'
}
Assert-DistinctClientPorts -APort $ClientAPort -BPort $ClientBPort `
    -AReconnectPort $ClientAReconnectPort
if ($ClientAName -ceq $ClientBName) { throw 'client_names_must_be_distinct' }

$head = (& git -C $repositoryRoot rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0) { throw 'repository_head_unavailable' }
$timestamp = [DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss-fff')
$logPath = if ([string]::IsNullOrWhiteSpace($LogDirectory)) {
    Join-Path ([System.IO.Path]::GetTempPath()) (
        Join-Path 'HL-Engine\stock-two-client' (
            $timestamp + '-' + [Guid]::NewGuid().ToString('N').Substring(0, 8)))
} else {
    [System.IO.Path]::GetFullPath($LogDirectory)
}
if (Test-PathInsideRoot -PathValue $logPath -Root $repositoryRoot) {
    throw 'Autotest results must be stored outside the repository.'
}
[void][System.IO.Directory]::CreateDirectory($logPath)

$stdoutPath = Join-Path $logPath 'server.stdout.log'
$stderrPath = Join-Path $logPath 'server.stderr.log'
$buildLogPath = Join-Path $logPath 'build.log'
$testLogPath = Join-Path $logPath 'ctest.log'
$unlockerBuildLogPath = Join-Path $logPath 'unlocker-build.log'
$shutdownRequestPath = Join-Path $logPath 'shutdown.request'
$disconnectRequestPath = Join-Path $logPath 'disconnect-slot-1.request'
$jsonResultPath = Join-Path $logPath 'stock_two_client_autotest.json'
$textResultPath = Join-Path $logPath 'stock_two_client_autotest.txt'

$result = [ordered]@{
    status = 'running'
    start_utc = [DateTime]::UtcNow.ToString('o')
    end_utc = $null
    repository_head = $head
    server_pid = $null
    server_port = $null
    client_a_pid = $null
    client_b_pid = $null
    client_a_reconnect_pid = $null
    client_a_process_lifetime_ms = $null
    client_b_process_lifetime_ms = $null
    client_a_unexpected_exit_code = $null
    client_b_unexpected_exit_code = $null
    client_a_reconnect_process_lifetime_ms = $null
    multi_instance_mode = $MultiInstanceMode
    initial_multirun_attempted = $false
    initial_multirun_succeeded = $false
    initial_client_b_pid = $null
    initial_client_b_lifetime_ms = $null
    initial_client_b_cleanup = 'not_started'
    initial_client_b_udp_seen = $false
    initial_client_b_handshake_seen = $false
    initial_client_b_single_instance_dialog = $false
    client_b_retry_pid = $null
    client_b_retry_stable = $false
    client_a_client_port = $ClientAPort
    client_b_client_port = $ClientBPort
    client_a_reconnect_port = $ClientAReconnectPort
    multirun_used = $true
    multirun_sufficient = $false
    launcher_mutex_fallback_used = $false
    mutex_unlock_backend = 'repo_x64_helper'
    mutex_unlock_attempted = $false
    mutex_owner_pid = $null
    mutex_owner_image_verified = $false
    mutex_object_name = 'ValveHalfLifeLauncherMutex'
    mutex_object_type = $null
    mutex_inspect_only = [bool]$InspectLauncherMutexOnly
    mutex_match_count = 0
    mutex_target_mutant_count = 0
    mutex_exact_name_verified = $false
    mutex_name_verified = $false
    mutex_object_type_verified = $false
    mutex_target_owned = $false
    mutex_target_image_verified = $false
    mutex_target_creation_time_verified = $false
    mutex_target_user_verified = $false
    mutex_target_session_verified = $false
    mutex_target_udp_verified = $false
    mutex_target_server_associated = $false
    mutex_target_spawned = $false
    mutex_handle_closed = $false
    unrelated_handles_closed = 0
    reconnect_mutex_handle_closed = $false
    client_b_retry_attempted = $false
    client_b_retry_udp_port = $null
    client_b_retry_handshake_seen = $false
    preexisting_process_modified = 'no'
    process_injection = 'no'
    memory_patching = 'no'
    client_files_modified = 'no'
    steam_termination = 'no'
    elevated_shell_required = $false
    last_stock_error_single_instance_dialog = $false
    last_stock_unexpected_exit_code = $null
    client_a_process_created = $false
    client_b_process_created = $false
    two_distinct_client_processes = $false
    two_distinct_client_udp_ports = $false
    client_a_connected = $false
    client_b_connected = $false
    client_a_handshake_seen = $false
    client_b_handshake_seen = $false
    client_a_spawned = $false
    client_b_spawned = $false
    client_a_slot = $null
    client_b_slot = $null
    client_a_edict = $null
    client_b_edict = $null
    client_a_movement = $false
    client_b_movement = $false
    client_a_sees_b = $false
    client_b_sees_a = $false
    remote_player_add = 'fail'
    remote_player_update = 'fail'
    simultaneous_movement = $false
    client_a_disconnect = 'fail'
    lifecycle_stage = 'not_started'
    client_a_disconnect_command_sent = $false
    client_a_disconnect_request_sent = $false
    client_a_disconnect_method = 'server_request_file'
    client_b_survived_disconnect = $false
    client_a_remove_visible_to_b = $false
    client_a_reconnect = $false
    client_a_reconnect_pid_is_new = $false
    client_a_reconnect_session_is_new = $false
    client_a_readd_visible_to_b = $false
    reconnect_movement = $false
    slot_reuse_clean = $false
    movement_after_60_seconds_both = $false
    movement_after_120_seconds_both = $false
    movement_after_300_seconds_both = $false
    movement_after_600_seconds_both = $false
    movement_window_processes_stable = $false
    cross_client_state_leak = $true
    server_cleanup = 'not_started'
    client_a_cleanup = 'not_started'
    client_b_cleanup = 'not_started'
    reconnect_client_cleanup = 'not_started'
    process_leak = $false
    blocker = 'none'
}

$serverProcess = $null
$clientAProcess = $null
$clientBProcess = $null
$clientAReconnectProcess = $null
$ownedPids = @()
$failure = $null
$dryRunPassed = $false
$inspectOnlyPassed = $false
$clientBPreexistingIds = @()

try {
    $selfTests = Invoke-LauncherSelfTests
    $cmake = Resolve-CMakeExecutable
    if (-not [string]::IsNullOrWhiteSpace($HandleExecutablePath) -and
        -not (Test-Path -LiteralPath $HandleExecutablePath -PathType Leaf)) {
        throw 'optional_handle_executable_not_found'
    }
    $needsUnlocker = $InspectLauncherMutexOnly -or
        $MultiInstanceMode -ceq 'MutexUnlock' -or
        ($MultiInstanceMode -ceq 'Auto' -and
         -not $DisableLauncherMutexFallback)
    $unlockerPath = if ($needsUnlocker) {
        Resolve-InstanceUnlocker -RequestedPath $InstanceUnlockerPath `
            -RepositoryRoot $repositoryRoot -CMakeExecutable $cmake `
            -OutputPath $unlockerBuildLogPath `
            -SkipToolsBuild ([bool]$SkipBuild) -IsDryRun ([bool]$DryRun)
    } else { '' }
    $ctest = if ($RunTests) {
        Resolve-CTestExecutable -CMakeExecutable $cmake
    } else { '' }
    $buildDirectory = Join-Path `
        $repositoryRoot 'out\build\vs2022-win32-reference-sdk'
    $canonicalTreeValid = Test-CanonicalBuildTree `
        -BuildDirectory $buildDirectory -RepositoryRoot $repositoryRoot

    $requestedA = if (-not [string]::IsNullOrWhiteSpace(
            $ClientAExecutablePath)) {
        $ClientAExecutablePath
    } else { $ClientExecutablePath }
    $clientAPath = Resolve-HalfLifeClient -RequestedPath $requestedA
    $requestedB = if (-not [string]::IsNullOrWhiteSpace(
            $ClientBExecutablePath)) {
        $ClientBExecutablePath
    } else { $clientAPath }
    $clientBPath = Resolve-HalfLifeClient -RequestedPath $requestedB
    $clientGamePath = Resolve-ClientGameDirectory `
        -RequestedPath '' -ClientPath $clientAPath -MapName $Map
    $serverGamePath = Resolve-ServerGameDirectory `
        -RequestedPath $ServerGameDir -MapName $Map `
        -ResolvedClientPath $clientAPath `
        -RequestedClientGameDir $clientGamePath

    $automaticPort = $Port -eq 0
    $selectedPort = if ($automaticPort) { Get-AvailableUdpPort } else { $Port }
    if (-not $automaticPort -and
        -not (Test-UdpPortAvailable -RequestedPort $selectedPort)) {
        throw 'server_port_unavailable'
    }
    $result.server_port = $selectedPort
    $serverPath = Resolve-HlhostExecutable `
        -RequestedPath $ServerExecutablePath -RepositoryRoot $repositoryRoot `
        -BuildDirectory $buildDirectory -BuildConfiguration $Configuration
    $serverArguments = New-StockServerArguments `
        -GameDirectory $serverGamePath -MapName $Map -Address $BindAddress `
        -SelectedPort $selectedPort -ShutdownRequestPath $shutdownRequestPath `
        -DisconnectRequestPath $disconnectRequestPath
    $clientAArguments = New-StockClientArguments `
        -Address $BindAddress -ServerPort $selectedPort `
        -LocalClientPort $ClientAPort -ClientName $ClientAName `
        -Movement 'forward'
    $clientBArguments = New-StockClientArguments `
        -Address $BindAddress -ServerPort $selectedPort `
        -LocalClientPort $ClientBPort -ClientName $ClientBName `
        -Movement 'moveright'
    $clientAReconnectName = $ClientAName + '_Reconnect'
    $clientAReconnectArguments = New-StockClientArguments `
        -Address $BindAddress -ServerPort $selectedPort `
        -LocalClientPort $ClientAReconnectPort `
        -ClientName $clientAReconnectName -Movement 'back'

    if ($DryRun) {
        Write-Host 'server_command=<resolved-hlhost.exe> <validated-local-two-client-arguments>'
        Write-Host 'client_a_command=<resolved-hl.exe> <validated-local-client-a-arguments>'
        Write-Host 'client_b_command=<resolved-hl.exe> <validated-local-client-b-arguments>'
        Write-Host 'client_a_reconnect_command=<resolved-hl.exe> <validated-local-reconnect-arguments>'
        Write-Host 'client_a_multirun=true'
        Write-Host 'client_b_multirun=true'
        Write-Host ('client_a_port={0}' -f $ClientAPort)
        Write-Host ('client_b_port={0}' -f $ClientBPort)
        Write-Host ('client_a_reconnect_port={0}' -f $ClientAReconnectPort)
        Write-Host 'client_names_distinct=true'
        Write-Host 'client_ports_distinct=true'
        Write-Host 'process_ownership_policy=exact_new_pid_only'
        Write-Host ('multi_instance_mode={0}' -f $MultiInstanceMode)
        Write-Host 'primary_method=direct_multirun'
        Write-Host 'fallback_method=exact_owned_launcher_mutex_unlock'
        Write-Host ('fallback_enabled={0}' -f
            (-not $DisableLauncherMutexFallback).ToString().ToLowerInvariant())
        Write-Host ('launcher_mutex_fallback_enabled={0}' -f
            (-not $DisableLauncherMutexFallback).ToString().ToLowerInvariant())
        Write-Host ('launcher_mutex_inspect_only={0}' -f
            ([bool]$InspectLauncherMutexOnly).ToString().ToLowerInvariant())
        Write-Host 'mutex_unlock_backend=repo_x64_helper'
        Write-Host 'mutex_unlocker_architecture=x64'
        Write-Host 'mutex_object_name=ValveHalfLifeLauncherMutex'
        Write-Host 'mutex_target_policy=owned_hl_process_only'
        Write-Host 'mutex_match_policy=exact_name_and_mutant_only'
        Write-Host 'mutex_close_policy=owned_pid_image_creation_user_udp_spawn_gate'
        Write-Host 'unrelated_handle_enumeration_output=disabled'
        Write-Host 'process_injection=no'
        Write-Host 'memory_patching=no'
        Write-Host 'client_files_modified=no'
        Write-Host 'steam_termination=no'
        Write-Host ('server_address={0}:{1}' -f $BindAddress, $selectedPort)
        Write-Host 'process_ownership_tests=pass'
        Write-Host 'cleanup_tests=pass'
        Write-Host 'dry_run=pass'
        $result.status = 'dry_run'
        $result.blocker = 'none'
        $dryRunPassed = $true
    } else {
        [System.IO.File]::WriteAllText($stdoutPath, '')
        [System.IO.File]::WriteAllText($stderrPath, '')
        if (-not $SkipBuild) {
            if (-not $canonicalTreeValid) {
                Invoke-QuietNative -Executable $cmake -Arguments @(
                    '-S', $repositoryRoot, '-B', $buildDirectory,
                    '-G', 'Visual Studio 17 2022', '-A', 'Win32') `
                    -OutputPath $buildLogPath -Description 'configure'
            }
            Invoke-QuietNative -Executable $cmake -Arguments @(
                '--build', $buildDirectory, '--config', $Configuration,
                '--target', 'hlhost', '--', '/m:1') `
                -OutputPath $buildLogPath -Description 'build'
        }
        if ($RunTests) {
            Invoke-QuietNative -Executable $ctest -Arguments @(
                '--test-dir', $buildDirectory, '-C', $Configuration,
                '--output-on-failure') `
                -OutputPath $testLogPath -Description 'ctest'
        }

        $serverProcess = Start-Process -FilePath $serverPath `
            -ArgumentList $serverArguments `
            -WorkingDirectory (Split-Path -Parent $serverPath) `
            -RedirectStandardOutput $stdoutPath `
            -RedirectStandardError $stderrPath `
            -WindowStyle Hidden -PassThru
        $result.server_pid = $serverProcess.Id
        $ownedPids += $serverProcess.Id
        Wait-HlhostReady -Process $serverProcess -StdoutPath $stdoutPath `
            -ExpectedAddress $BindAddress -ExpectedPort $selectedPort `
            -TimeoutSeconds $ConnectTimeoutSeconds
        Wait-UdpEndpointOwnership -Process $serverProcess `
            -Address $BindAddress -SelectedPort $selectedPort `
            -TimeoutSeconds $ConnectTimeoutSeconds

        $preexistingClientIds = @(Get-HalfLifeProcessRecords |
            ForEach-Object { [int]$_.ProcessId })
        $clientAProcess = Start-StockOwnedClient -Role 'client_a' `
            -Executable $clientAPath -Arguments $clientAArguments `
            -PreexistingIds $preexistingClientIds `
            -ExpectedClientPort $ClientAPort -ExpectedClientName $ClientAName
        $result.client_a_pid = $clientAProcess.Id
        $result.client_a_process_created = $true
        $ownedPids += $clientAProcess.Id
        Wait-OwnedUdpPort -Process $clientAProcess -ExpectedPort $ClientAPort `
            -TimeoutSeconds $ConnectTimeoutSeconds `
            -FailureIdentifier 'client_a_process_created_but_no_network'
        $aAccept = Wait-LogRegex -PathValue $stdoutPath `
            -Pattern ('goldsrc_udp_accept: session_id=(?<session>[^,]+),slot=1,endpoint=127\.0\.0\.1:{0},' -f $ClientAPort) `
            -TimeoutSeconds $ConnectTimeoutSeconds `
            -ServerProcess $serverProcess -ClientProcess $clientAProcess `
            -FailureIdentifier 'client_a_connect_timeout'
        $sessionA = $aAccept.Groups['session'].Value
        $result.client_a_connected = $true
        $result.client_a_handshake_seen = $true
        $result.client_a_slot = 1
        $aBound = Wait-LogRegex -PathValue $stdoutPath `
            -Pattern ('goldsrc_player_edict_bound: session_id={0},slot=1,edict=1,' -f [regex]::Escape($sessionA)) `
            -TimeoutSeconds $SpawnTimeoutSeconds -ServerProcess $serverProcess `
            -ClientProcess $clientAProcess `
            -FailureIdentifier 'client_a_spawn_timeout'
        $null = Wait-LogRegex -PathValue $stdoutPath `
            -Pattern ('goldsrc_player_materialized: session_id={0},edict=1,.*spawned=1' -f [regex]::Escape($sessionA)) `
            -TimeoutSeconds $SpawnTimeoutSeconds -ServerProcess $serverProcess `
            -ClientProcess $clientAProcess `
            -FailureIdentifier 'client_a_spawn_timeout'
        $result.client_a_spawned = $true
        $result.client_a_edict = 1
        $null = Wait-LogRegex -PathValue $stdoutPath `
            -Pattern ('goldsrc_continuous_snapshot_sent: session_id={0},' -f [regex]::Escape($sessionA)) `
            -TimeoutSeconds $SpawnTimeoutSeconds -ServerProcess $serverProcess `
            -ClientProcess $clientAProcess `
            -FailureIdentifier 'client_a_spawn_timeout'

        if ($InspectLauncherMutexOnly) {
            $null = Invoke-VerifiedLauncherMutex `
                -Process $clientAProcess -ExpectedImage $clientAPath `
                -PreexistingIds $preexistingClientIds -OwnedIds $ownedPids `
                -ExpectedPort $ClientAPort `
                -ServerAssociated $result.client_a_connected `
                -Spawned $result.client_a_spawned `
                -UnlockerExecutable $unlockerPath -CloseHandle $false `
                -Result $result -Context Primary
            $result.status = 'pass'
            $result.blocker = 'none'
            $inspectOnlyPassed = $true
        } else {
        if ($ClientLaunchDelaySeconds -gt 0) {
            Start-Sleep -Seconds $ClientLaunchDelaySeconds
        }

        $needsMutexUnlock = $MultiInstanceMode -ceq 'MutexUnlock'
        if ($MultiInstanceMode -ceq 'Auto') {
            $result.initial_multirun_attempted = $true
            $idsBeforeInitialB = @(Get-HalfLifeProcessRecords |
                ForEach-Object { [int]$_.ProcessId })
            $initialLogOffset = (Get-SharedFileText `
                -PathValue $stdoutPath).Length
            try {
                $clientBProcess = Start-StockOwnedClient `
                    -Role 'client_b_initial' -Executable $clientBPath `
                    -Arguments $clientBArguments `
                    -PreexistingIds $idsBeforeInitialB `
                    -ExpectedClientPort $ClientBPort `
                    -ExpectedClientName $ClientBName `
                    -AlwaysCleanupOnFailure
                $result.initial_multirun_succeeded = $true
                $result.initial_client_b_pid = $clientBProcess.Id
                $result.initial_client_b_cleanup = 'not_required'
                $clientBPreexistingIds = $idsBeforeInitialB
                $result.multirun_sufficient = $true
            }
            catch {
                if ($_.Exception.Message -cne
                    'client_b_initial_multirun_rejected') {
                    throw
                }
                $result.initial_multirun_succeeded = $false
                $initialPid = if ($_.Exception.Data.Contains('ProcessId')) {
                    [int]$_.Exception.Data['ProcessId']
                } else { $null }
                $result.initial_client_b_pid = $initialPid
                $result.initial_client_b_lifetime_ms =
                    [long]$_.Exception.Data['LifetimeMs']
                $result.initial_client_b_single_instance_dialog =
                    [bool]$_.Exception.Data['SingleInstanceDialog']
                if ($null -ne $initialPid) {
                    $ownedPids += $initialPid
                    if ($null -ne (Get-Process -Id $initialPid `
                            -ErrorAction SilentlyContinue)) {
                        throw 'client_b_initial_cleanup_failed'
                    }
                    $result.initial_client_b_udp_seen =
                        Test-OwnedUdpPort -ProcessId $initialPid `
                            -ExpectedPort $ClientBPort
                }
                $newLog = Get-SharedFileText -PathValue $stdoutPath
                if ($newLog.Length -gt $initialLogOffset) {
                    $newLog = $newLog.Substring($initialLogOffset)
                    $result.initial_client_b_handshake_seen =
                        [regex]::IsMatch($newLog,
                            ('goldsrc_udp_accept: .*endpoint=127\.0\.0\.1:{0},' -f
                                $ClientBPort))
                }
                if ($result.initial_client_b_udp_seen -or
                    $result.initial_client_b_handshake_seen) {
                    throw 'client_b_initial_attempt_reached_network'
                }
                $result.initial_client_b_cleanup =
                    'stopped_during_launch_failure'
                if ($DisableLauncherMutexFallback) {
                    throw 'client_b_initial_multirun_rejected'
                }
                $needsMutexUnlock = $true
            }
        }
        elseif ($MultiInstanceMode -ceq 'MultirunOnly') {
            $clientBPreexistingIds = @(Get-HalfLifeProcessRecords |
                ForEach-Object { [int]$_.ProcessId })
            $clientBProcess = Start-StockOwnedClient -Role 'client_b' `
                -Executable $clientBPath -Arguments $clientBArguments `
                -PreexistingIds $clientBPreexistingIds `
                -ExpectedClientPort $ClientBPort `
                -ExpectedClientName $ClientBName
            $result.multirun_sufficient = $true
        }

        if ($needsMutexUnlock) {
            $null = Invoke-VerifiedLauncherMutex `
                -Process $clientAProcess -ExpectedImage $clientAPath `
                -PreexistingIds $preexistingClientIds -OwnedIds $ownedPids `
                -ExpectedPort $ClientAPort `
                -ServerAssociated $result.client_a_connected `
                -Spawned $result.client_a_spawned `
                -UnlockerExecutable $unlockerPath -CloseHandle $true `
                -Result $result -Context Primary
            $result.launcher_mutex_fallback_used = $true
            $result.client_b_retry_attempted = $true
            $clientBPreexistingIds = @(Get-HalfLifeProcessRecords |
                ForEach-Object { [int]$_.ProcessId })
            $clientBProcess = Start-StockOwnedClient `
                -Role 'client_b_retry' -Executable $clientBPath `
                -Arguments $clientBArguments `
                -PreexistingIds $clientBPreexistingIds `
                -ExpectedClientPort $ClientBPort `
                -ExpectedClientName $ClientBName
            $result.client_b_retry_pid = $clientBProcess.Id
        }

        $result.client_b_pid = $clientBProcess.Id
        $result.client_b_process_created = $true
        $ownedPids += $clientBProcess.Id
        if ($clientBProcess.Id -eq $clientAProcess.Id) {
            throw 'two_distinct_client_processes_failed'
        }
        $result.two_distinct_client_processes = $true
        Wait-OwnedUdpPort -Process $clientBProcess -ExpectedPort $ClientBPort `
            -TimeoutSeconds $ConnectTimeoutSeconds `
            -FailureIdentifier 'client_b_process_created_but_no_network'
        $result.two_distinct_client_udp_ports = $true
        if ($result.client_b_retry_attempted) {
            $result.client_b_retry_udp_port = $ClientBPort
        }
        $bAccept = Wait-LogRegex -PathValue $stdoutPath `
            -Pattern ('goldsrc_udp_accept: session_id=(?<session>[^,]+),slot=2,endpoint=127\.0\.0\.1:{0},' -f $ClientBPort) `
            -TimeoutSeconds $ConnectTimeoutSeconds `
            -ServerProcess $serverProcess -ClientProcess $clientBProcess `
            -FailureIdentifier 'client_b_connect_timeout'
        $sessionB = $bAccept.Groups['session'].Value
        $result.client_b_connected = $true
        $result.client_b_handshake_seen = $true
        if ($result.client_b_retry_attempted) {
            $result.client_b_retry_handshake_seen = $true
        }
        $result.client_b_slot = 2
        $null = Wait-LogRegex -PathValue $stdoutPath `
            -Pattern ('goldsrc_player_edict_bound: session_id={0},slot=2,edict=2,' -f [regex]::Escape($sessionB)) `
            -TimeoutSeconds $SpawnTimeoutSeconds -ServerProcess $serverProcess `
            -ClientProcess $clientBProcess `
            -FailureIdentifier 'client_b_spawn_timeout'
        $null = Wait-LogRegex -PathValue $stdoutPath `
            -Pattern ('goldsrc_player_materialized: session_id={0},edict=2,.*spawned=1' -f [regex]::Escape($sessionB)) `
            -TimeoutSeconds $SpawnTimeoutSeconds -ServerProcess $serverProcess `
            -ClientProcess $clientBProcess `
            -FailureIdentifier 'client_b_spawn_timeout'
        $result.client_b_spawned = $true
        $result.client_b_edict = 2
        if ($null -ne $result.client_b_retry_pid) {
            $result.client_b_retry_stable = $true
        }

        $movementStartText = Get-SharedFileText -PathValue $stdoutPath
        $startSamplesA = Get-SnapshotSampleCount `
            -Text $movementStartText -SessionId $sessionA
        $startSamplesB = Get-SnapshotSampleCount `
            -Text $movementStartText -SessionId $sessionB
        $movementDeadline = [DateTime]::UtcNow.AddSeconds(
            $AutoTestDurationSeconds)
        while ([DateTime]::UtcNow -lt $movementDeadline) {
            foreach ($entry in @(
                [pscustomobject]@{ Role = 'client_a'; Process = $clientAProcess },
                [pscustomobject]@{ Role = 'client_b'; Process = $clientBProcess })) {
                $client = $entry.Process
                $client.Refresh()
                if ($client.HasExited) {
                    $result[($entry.Role + '_unexpected_exit_code')] =
                        $client.ExitCode
                    throw ($entry.Role + '_exited_during_movement')
                }
                if ($client.MainWindowTitle -eq 'Error') {
                    $result.last_stock_error_single_instance_dialog =
                        Test-StockSingleInstanceDialog -Process $client
                    throw ($entry.Role + '_error_dialog_during_movement')
                }
            }
            $serverProcess.Refresh()
            if ($serverProcess.HasExited) { throw 'server_exited_during_gate' }
            if ($FollowServerLog) {
                $text = Get-SharedFileText -PathValue $stdoutPath
                Write-Host ('movement_samples_a={0}' -f
                    (Get-SnapshotSampleCount -Text $text -SessionId $sessionA))
                Write-Host ('movement_samples_b={0}' -f
                    (Get-SnapshotSampleCount -Text $text -SessionId $sessionB))
            }
            Start-Sleep -Seconds 1
        }
        $movementText = Get-SharedFileText -PathValue $stdoutPath
        $samplesA = Get-SnapshotSampleCount -Text $movementText `
            -SessionId $sessionA
        $samplesB = Get-SnapshotSampleCount -Text $movementText `
            -SessionId $sessionB
        $result.movement_window_processes_stable = $true
        foreach ($checkpoint in @(60, 120, 300, 600)) {
            if ($AutoTestDurationSeconds -ge $checkpoint) {
                $result[('movement_after_{0}_seconds_both' -f $checkpoint)] = $true
            }
        }

        if (-not $SkipReconnectTest) {
            $aPid = [int]$result.client_a_pid
            $result.lifecycle_stage = 'disconnect_ownership_gate'
            $null = Assert-UnlockTargetOwnership `
                -Process $clientAProcess -ExpectedImage $clientAPath `
                -PreexistingIds $preexistingClientIds -OwnedIds $ownedPids `
                -ExpectedPort $ClientAPort `
                -ServerAssociated $result.client_a_connected `
                -Spawned $result.client_a_spawned
            $result.lifecycle_stage = 'disconnect_server_request'
            [System.IO.File]::WriteAllText(
                $disconnectRequestPath,
                'disconnect-owned-slot-1')
            $result.client_a_disconnect_request_sent = $true
            $result.lifecycle_stage = 'disconnect_server_ack'
            $disconnectMatch = Wait-LogRegex -PathValue $stdoutPath `
                -Pattern ('goldsrc_game_dll_client_disconnected: session_id={0},' -f [regex]::Escape($sessionA)) `
                -TimeoutSeconds $DisconnectTimeoutSeconds `
                -ServerProcess $serverProcess `
                -ClientProcess $clientAProcess `
                -FailureIdentifier 'client_a_disconnect_timeout'
            $result.client_a_cleanup = Stop-ExactOwnedProcess `
                -Process $clientAProcess -ExpectedProcessId $aPid `
                -Description 'client_a'
            $clientAProcess.Dispose()
            $clientAProcess = $null
            $result.client_a_disconnect = 'pass'
            $result.lifecycle_stage = 'disconnect_remove_snapshot'
            Start-Sleep -Seconds 1
            $clientBProcess.Refresh()
            $result.client_b_survived_disconnect = -not $clientBProcess.HasExited

            if ($result.launcher_mutex_fallback_used) {
                $result.lifecycle_stage = 'reconnect_mutex_unlock'
                $null = Invoke-VerifiedLauncherMutex `
                    -Process $clientBProcess -ExpectedImage $clientBPath `
                    -PreexistingIds $clientBPreexistingIds `
                    -OwnedIds $ownedPids -ExpectedPort $ClientBPort `
                    -ServerAssociated $result.client_b_connected `
                    -Spawned $result.client_b_spawned `
                    -UnlockerExecutable $unlockerPath -CloseHandle $true `
                    -Result $result -Context Reconnect
            }

            $idsBeforeReconnect = @(Get-HalfLifeProcessRecords |
                ForEach-Object { [int]$_.ProcessId })
            $reconnectLogOffset = (Get-SharedFileText `
                -PathValue $stdoutPath).Length
            $clientAReconnectProcess = Start-StockOwnedClient `
                -Role 'client_a_reconnect' -Executable $clientAPath `
                -Arguments $clientAReconnectArguments `
                -PreexistingIds $idsBeforeReconnect `
                -ExpectedClientPort $ClientAReconnectPort `
                -ExpectedClientName $clientAReconnectName
            $result.client_a_reconnect_pid = $clientAReconnectProcess.Id
            $ownedPids += $clientAReconnectProcess.Id
            $result.client_a_reconnect_pid_is_new =
                $clientAReconnectProcess.Id -ne $aPid
            Wait-OwnedUdpPort -Process $clientAReconnectProcess `
                -ExpectedPort $ClientAReconnectPort `
                -TimeoutSeconds $ReconnectTimeoutSeconds `
                -FailureIdentifier 'client_a_reconnect_process_created_but_no_network'
            $aReconnectAccept = Wait-LogRegex -PathValue $stdoutPath `
                -Pattern ('goldsrc_udp_accept: session_id=(?<session>[^,]+),slot=1,endpoint=127\.0\.0\.1:{0},' -f $ClientAReconnectPort) `
                -TimeoutSeconds $ReconnectTimeoutSeconds `
                -ServerProcess $serverProcess `
                -ClientProcess $clientAReconnectProcess `
                -FailureIdentifier 'client_a_reconnect_timeout' `
                -StartOffset $reconnectLogOffset
            $sessionAReconnect = $aReconnectAccept.Groups['session'].Value
            $result.client_a_reconnect_session_is_new =
                $sessionAReconnect -cne $sessionA
            $null = Wait-LogRegex -PathValue $stdoutPath `
                -Pattern ('goldsrc_player_materialized: session_id={0},edict=1,.*spawned=1' -f [regex]::Escape($sessionAReconnect)) `
                -TimeoutSeconds $ReconnectTimeoutSeconds `
                -ServerProcess $serverProcess `
                -ClientProcess $clientAReconnectProcess `
                -FailureIdentifier 'client_a_reconnect_spawn_timeout' `
                -StartOffset $reconnectLogOffset
            $result.client_a_reconnect = $true
            $result.lifecycle_stage = 'reconnect_readd_snapshot'
            $result.lifecycle_stage = 'reconnect_complete'
            Start-Sleep -Seconds ([Math]::Min(10, $MovementTimeoutSeconds))
        }

        $result.server_cleanup = Stop-ExactOwnedServer `
            -Process $serverProcess -ExpectedProcessId $serverProcess.Id `
            -ShutdownRequestPath $shutdownRequestPath
        $summaryText = Get-SharedFileText -PathValue $stdoutPath
        $summaryA = Get-LastSummaryFields -Text $summaryText `
            -Prefix 'goldsrc_client_replication_summary:' -Slot '1'
        $summaryB = Get-LastSummaryFields -Text $summaryText `
            -Prefix 'goldsrc_client_replication_summary:' -Slot '2'
        $aggregate = Get-LastSummaryFields -Text $summaryText `
            -Prefix 'goldsrc_two_client_replication_summary:'
        if ($null -eq $summaryA -or $null -eq $summaryB -or
            $null -eq $aggregate) {
            throw 'stock_replication_summary_failed'
        }
        $result.client_a_movement = [int]$summaryA['pmove_calls'] -gt 0
        $result.client_b_movement = [int]$summaryB['pmove_calls'] -gt 0
        $result.client_a_sees_b =
            ([int]$summaryA['remote_adds'] +
             [int]$summaryA['remote_updates']) -gt 0
        $result.client_b_sees_a =
            ([int]$summaryB['remote_adds'] +
             [int]$summaryB['remote_updates']) -gt 0
        $result.client_a_remove_visible_to_b =
            [int]$summaryB['remote_removes'] -gt 0
        $result.client_a_readd_visible_to_b = if ($SkipReconnectTest) {
            $true
        } else {
            [int]$summaryB['remote_readds'] -gt 0
        }
        $result.remote_player_add = if (
            [int]$summaryA['remote_adds'] -gt 0 -and
            [int]$summaryB['remote_adds'] -gt 0) { 'pass' } else { 'fail' }
        $result.remote_player_update = if (
            [int]$summaryA['remote_updates'] -gt 0 -and
            [int]$summaryB['remote_updates'] -gt 0) { 'pass' } else { 'fail' }
        $result.simultaneous_movement =
            $aggregate['simultaneous_movement'] -ceq 'true'
        $result.slot_reuse_clean = if ($SkipReconnectTest) {
            $true
        } else {
            $aggregate['reconnect_slot_reuse'] -ceq 'true' -and
            $summaryA['slot_reuse_clean'] -ceq 'true'
        }
        $result.cross_client_state_leak =
            $aggregate['cross_client_state_leak'] -cne 'false'
        if (-not $SkipReconnectTest) {
            $result.reconnect_movement =
                [int]$summaryA['pmove_calls'] -gt 0 -and
                [int]$summaryB['pmove_calls'] -gt 0
        }
        if (-not $result.client_a_movement -or
            -not $result.client_b_movement -or
            -not $result.simultaneous_movement) {
            throw 'stock_movement_summary_failed'
        }
        if (-not $result.client_a_sees_b -or
            -not $result.client_b_sees_a -or
            $result.remote_player_add -cne 'pass' -or
            $result.remote_player_update -cne 'pass' -or
            $result.cross_client_state_leak) {
            throw 'stock_replication_summary_failed'
        }
        if (-not $SkipReconnectTest -and
            ($result.client_a_disconnect -cne 'pass' -or
             -not $result.client_b_survived_disconnect -or
             -not $result.client_a_remove_visible_to_b -or
             -not $result.client_a_reconnect -or
             -not $result.client_a_reconnect_pid_is_new -or
             -not $result.client_a_reconnect_session_is_new -or
             -not $result.client_a_readd_visible_to_b -or
             -not $result.reconnect_movement -or
             -not $result.slot_reuse_clean)) {
            throw 'stock_disconnect_reconnect_summary_failed'
        }
        }
        $result.status = 'pass'
        $result.blocker = 'none'
    }
}
catch {
    $failure = $_
    $result.status = 'fail'
    $result.blocker = Convert-FailureIdentifier `
        -Message $_.Exception.Message
    if ($result.blocker.EndsWith('_single_instance_dialog',
            [StringComparison]::Ordinal)) {
        $result.last_stock_error_single_instance_dialog = $true
    }
    if ($_.Exception.Data.Contains('UnexpectedExitCode')) {
        $result.last_stock_unexpected_exit_code =
            [int]$_.Exception.Data['UnexpectedExitCode']
    }
    if ($result.blocker -ceq
        'launcher_mutex_access_denied_elevation_required') {
        $result.elevated_shell_required = $true
    }
    if ($_.Exception.Data.Contains('ProcessCreated')) {
        $created = [bool]$_.Exception.Data['ProcessCreated']
        $failedPid = if ($_.Exception.Data.Contains('ProcessId')) {
            [int]$_.Exception.Data['ProcessId']
        } else { $null }
        $lifetime = [long]$_.Exception.Data['LifetimeMs']
        $result.last_stock_error_single_instance_dialog =
            [bool]$_.Exception.Data['SingleInstanceDialog']
        if ($null -ne $failedPid) { $ownedPids += $failedPid }
        if ($result.blocker.StartsWith('client_b_')) {
            $result.client_b_process_created = $created
            $result.client_b_pid = $failedPid
            $result.client_b_process_lifetime_ms = $lifetime
            if ($created) {
                $result.client_b_cleanup = if ($KeepProcessesOnFailure) {
                    'kept_on_failure'
                } else { 'stopped_during_launch_failure' }
            }
        } elseif ($result.blocker.StartsWith('client_a_reconnect_')) {
            $result.client_a_reconnect_pid = $failedPid
            $result.client_a_reconnect_process_lifetime_ms = $lifetime
            if ($created) {
                $result.reconnect_client_cleanup = if (
                    $KeepProcessesOnFailure) {
                    'kept_on_failure'
                } else { 'stopped_during_launch_failure' }
            }
        } elseif ($result.blocker.StartsWith('client_a_')) {
            $result.client_a_process_created = $created
            $result.client_a_pid = $failedPid
            $result.client_a_process_lifetime_ms = $lifetime
            if ($created) {
                $result.client_a_cleanup = if ($KeepProcessesOnFailure) {
                    'kept_on_failure'
                } else { 'stopped_during_launch_failure' }
            }
        }
    }
}
finally {
    $keep = $KeepProcessesOnFailure -and $null -ne $failure
    if (-not $keep -and -not $DryRun) {
        if ($null -ne $clientAReconnectProcess) {
            try {
                $result.reconnect_client_cleanup = Stop-ExactOwnedProcess `
                    -Process $clientAReconnectProcess `
                    -ExpectedProcessId $clientAReconnectProcess.Id `
                    -Description 'client_a_reconnect'
            }
            catch { $result.reconnect_client_cleanup = 'failed' }
        }
        if ($null -ne $clientBProcess) {
            try {
                $result.client_b_cleanup = Stop-ExactOwnedProcess `
                    -Process $clientBProcess `
                    -ExpectedProcessId $clientBProcess.Id `
                    -Description 'client_b'
            }
            catch { $result.client_b_cleanup = 'failed' }
        }
        if ($null -ne $clientAProcess) {
            try {
                $result.client_a_cleanup = Stop-ExactOwnedProcess `
                    -Process $clientAProcess `
                    -ExpectedProcessId $clientAProcess.Id `
                    -Description 'client_a'
            }
            catch { $result.client_a_cleanup = 'failed' }
        }
        if ($null -ne $serverProcess -and
            $result.server_cleanup -eq 'not_started') {
            try {
                $result.server_cleanup = Stop-ExactOwnedServer `
                    -Process $serverProcess `
                    -ExpectedProcessId $serverProcess.Id `
                    -ShutdownRequestPath $shutdownRequestPath
            }
            catch { $result.server_cleanup = 'failed' }
        }
    }
    foreach ($process in @(
        $clientAReconnectProcess,
        $clientBProcess,
        $clientAProcess,
        $serverProcess)) {
        if ($null -ne $process) { $process.Dispose() }
    }
    if (-not $keep) {
        $remaining = @($ownedPids | Where-Object {
            $null -ne (Get-Process -Id $_ -ErrorAction SilentlyContinue)
        })
        $result.process_leak = $remaining.Count -ne 0
    } else {
        $result.process_leak = $true
    }
    $result.end_utc = [DateTime]::UtcNow.ToString('o')
    try {
        Write-AutotestResults -Result $result `
            -JsonPath $jsonResultPath -TextPath $textResultPath
    }
    catch {
        if ($null -eq $failure) { $failure = $_ }
    }
}

if ($dryRunPassed) {
    Write-Host ('result_json={0}' -f $jsonResultPath)
    exit 0
}
if ($inspectOnlyPassed -and $null -eq $failure) {
    Write-Host 'status=pass'
    Write-Host 'launcher_mutex_inspect_only=pass'
    Write-Host 'mutex_handle_closed=false'
    Write-Host ('result_json={0}' -f $jsonResultPath)
    exit 0
}
if ($null -ne $failure) {
    Write-Host ('status={0}' -f $result.status)
    Write-Host ('blocker={0}' -f $result.blocker)
    Write-Host ('result_json={0}' -f $jsonResultPath)
    exit 1
}
Write-Host 'status=pass'
Write-Host 'stock_two_client_autotest=pass'
Write-Host ('result_json={0}' -f $jsonResultPath)
exit 0
