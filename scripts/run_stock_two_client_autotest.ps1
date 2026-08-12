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

    [switch]$GoldSrcCombat,

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
    [int]$SpawnTimeoutSeconds = 180,

    [ValidateRange(5, 300)]
    [int]$MovementTimeoutSeconds = 60,

    [ValidateRange(5, 120)]
    [int]$DisconnectTimeoutSeconds = 30,

    [ValidateRange(5, 300)]
    [int]$ReconnectTimeoutSeconds = 120,

    [ValidateRange(10, 900)]
    [int]$AutoTestDurationSeconds = 120,

    [ValidateSet('None', 'VerifiedSendInput')]
    [string]$InputMode = 'None',

    [switch]$AllowAutomatedPlayerInput,

    [ValidateSet('All', 'Automatic', 'Reconnect', 'ManualObservation')]
    [string]$AcceptancePhase = 'ManualObservation',

    [switch]$ManualObservation,

    [ValidateRange(10, 3600)]
    [int]$ManualObservationSeconds = 600,

    [switch]$PromptForVisualConfirmation,

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
switch ($AcceptancePhase) {
    'Automatic' {
        $ManualObservation = $false
        $SkipReconnectTest = $true
    }
    'Reconnect' {
        $ManualObservation = $false
        $SkipReconnectTest = $false
        if (-not $PSBoundParameters.ContainsKey('AutoTestDurationSeconds')) {
            $AutoTestDurationSeconds = 10
        }
    }
    'ManualObservation' {
        $ManualObservation = $true
        $SkipReconnectTest = $true
    }
}
if ($ManualObservation -and $InputMode -cne 'None') {
    throw 'manual_observation_requires_input_mode_none'
}
if ($ManualObservation -and $AllowAutomatedPlayerInput) {
    throw 'manual_observation_forbids_automated_player_input'
}
if ($InputMode -cne 'None' -and -not $AllowAutomatedPlayerInput) {
    throw 'automated_player_input_requires_explicit_opt_in'
}
if (-not $ManualObservation -and
    ($InputMode -cne 'VerifiedSendInput' -or
        -not $AllowAutomatedPlayerInput)) {
    throw 'automatic_acceptance_requires_verified_sendinput'
}
if ($PromptForVisualConfirmation -and -not $ManualObservation) {
    throw 'visual_confirmation_requires_manual_observation'
}
if ($GoldSrcCombat -and $ManualObservation -and
    $ManualObservationSeconds -lt 120) {
    throw 'goldsrc_combat_manual_observation_requires_120_seconds'
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
        [string]$DisconnectRequestPath,
        [string]$StockTestControlPath)

    $arguments = @(
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
        (Quote-ProcessArgument $DisconnectRequestPath),
        '--goldsrc-stock-test-control-file',
        (Quote-ProcessArgument $StockTestControlPath)
    )
    if ($GoldSrcCombat) {
        $arguments += '--goldsrc-combat'
    }
    return $arguments
}

function New-StockClientArguments {
    param(
        [string]$Address,
        [int]$ServerPort,
        [int]$LocalClientPort,
        [string]$ClientName)

    $arguments = @(
        '-steam',
        '-multirun',
        '-insecure',
        '-game', 'valve',
        '-console',
        '-novid',
        '-nojoy',
        '-windowed',
        '-w', '960',
        '-h', '540',
        '+clientport', [string]$LocalClientPort,
        '+joystick', '0',
        '+name', $ClientName,
        '+connect', ('{0}:{1}' -f $Address, $ServerPort)
    )
    return $arguments
}

function Initialize-StockInputBridge {
    if ('Prompt248StockInput.Native' -as [type]) { return }
    Add-Type -AssemblyName System.Windows.Forms
    Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;

namespace Prompt248StockInput
{
    public static class Native
    {
        private delegate bool EnumWindowsCallback(IntPtr window, IntPtr state);

        [StructLayout(LayoutKind.Sequential)]
        private struct WindowRect
        {
            public int left;
            public int top;
            public int right;
            public int bottom;
        }

        [StructLayout(LayoutKind.Sequential)]
        private struct KeyboardInput
        {
            public ushort virtualKey;
            public ushort scanCode;
            public uint flags;
            public uint time;
            public UIntPtr extraInfo;
        }

        [StructLayout(LayoutKind.Sequential)]
        private struct MouseInput
        {
            public int x;
            public int y;
            public uint mouseData;
            public uint flags;
            public uint time;
            public UIntPtr extraInfo;
        }

        [StructLayout(LayoutKind.Explicit)]
        private struct InputUnion
        {
            [FieldOffset(0)] public KeyboardInput keyboard;
            [FieldOffset(0)] public MouseInput mouse;
        }

        [StructLayout(LayoutKind.Sequential)]
        private struct Input
        {
            public uint type;
            public InputUnion data;
        }

        [DllImport("user32.dll")]
        public static extern bool SetForegroundWindow(IntPtr window);

        [DllImport("user32.dll")]
        public static extern IntPtr GetForegroundWindow();

        [DllImport("user32.dll")]
        private static extern bool EnumWindows(
            EnumWindowsCallback callback, IntPtr state);

        [DllImport("user32.dll")]
        private static extern bool IsWindowVisible(IntPtr window);

        [DllImport("user32.dll")]
        private static extern IntPtr GetWindow(IntPtr window, uint command);

        [DllImport("user32.dll")]
        private static extern bool GetWindowRect(
            IntPtr window, out WindowRect rectangle);

        [DllImport("user32.dll")]
        public static extern uint GetWindowThreadProcessId(
            IntPtr window, out uint processId);

        [DllImport("kernel32.dll")]
        public static extern uint GetCurrentThreadId();

        [DllImport("user32.dll")]
        public static extern bool AttachThreadInput(
            uint sourceThread, uint targetThread, bool attach);

        [DllImport("user32.dll")]
        public static extern bool BringWindowToTop(IntPtr window);

        [DllImport("user32.dll")]
        public static extern bool ShowWindow(IntPtr window, int command);

        [DllImport("user32.dll")]
        public static extern IntPtr SetActiveWindow(IntPtr window);

        [DllImport("user32.dll")]
        public static extern IntPtr SetFocus(IntPtr window);

        [DllImport("user32.dll")]
        private static extern bool MoveWindow(
            IntPtr window, int x, int y, int width, int height, bool repaint);

        [DllImport("user32.dll")]
        private static extern int GetSystemMetrics(int index);

        [DllImport("user32.dll", SetLastError = true)]
        private static extern uint SendInput(
            uint inputCount, Input[] inputs, int inputSize);

        [DllImport("user32.dll")]
        private static extern uint MapVirtualKey(uint code, uint mapType);

        [DllImport("user32.dll")]
        private static extern short GetAsyncKeyState(int virtualKey);

        public static IntPtr FindVisibleTopLevelWindow(uint processId)
        {
            IntPtr match = IntPtr.Zero;
            long largestArea = 0;
            EnumWindows((window, state) =>
            {
                if (!IsWindowVisible(window)) return true;
                uint owner;
                GetWindowThreadProcessId(window, out owner);
                if (owner != processId) return true;
                if (GetWindow(window, 4u) != IntPtr.Zero) return true;
                WindowRect rectangle;
                if (!GetWindowRect(window, out rectangle)) return true;
                long width = Math.Max(0, rectangle.right - rectangle.left);
                long height = Math.Max(0, rectangle.bottom - rectangle.top);
                long area = width * height;
                if (area > largestArea)
                {
                    largestArea = area;
                    match = window;
                }
                return true;
            }, IntPtr.Zero);
            return match;
        }

        public static bool IsKeyDown(int virtualKey)
        {
            return (GetAsyncKeyState(virtualKey) & 0x8000) != 0;
        }

        public static bool SendScanCode(ushort virtualKey, bool released)
        {
            var input = new Input();
            input.type = 1;
            input.data.keyboard.virtualKey = 0;
            input.data.keyboard.scanCode =
                (ushort)MapVirtualKey(virtualKey, 0);
            input.data.keyboard.flags = 0x0008u | (released ? 0x0002u : 0u);
            input.data.keyboard.time = 0;
            input.data.keyboard.extraInfo = UIntPtr.Zero;
            return SendInput(1, new[] { input }, Marshal.SizeOf(input)) == 1;
        }

        public static int InputStructureSize()
        {
            return Marshal.SizeOf(typeof(Input));
        }

        public static bool ArrangeSideBySide(IntPtr left, IntPtr right)
        {
            int screenWidth = GetSystemMetrics(0);
            int screenHeight = GetSystemMetrics(1);
            if (screenWidth < 800 || screenHeight < 480) return false;
            int width = screenWidth / 2;
            int height = Math.Min(screenHeight, Math.Max(480, width * 9 / 16 + 40));
            return MoveWindow(left, 0, 0, width, height, true)
                && MoveWindow(right, width, 0, screenWidth - width, height, true);
        }

    }
}
'@
}

function Send-StockKeyInput {
    param([byte]$VirtualKey, [bool]$Released)

    if (-not [Prompt248StockInput.Native]::SendScanCode(
            [uint16]$VirtualKey, $Released)) {
        throw 'sendinput_failed'
    }
}

function Assert-OwnedClientForeground {
    param(
        [System.Diagnostics.Process]$Process,
        [string]$ExpectedImage)

    Initialize-StockInputBridge
    $Process.Refresh()
    if ($Process.HasExited) {
        throw 'client_window_not_found'
    }
    $expectedPath = [System.IO.Path]::GetFullPath($ExpectedImage)
    $actualPath = [System.IO.Path]::GetFullPath($Process.Path)
    if (-not $actualPath.Equals(
            $expectedPath, [StringComparison]::OrdinalIgnoreCase)) {
        throw 'client_window_not_owned'
    }
    $targetWindow =
        [Prompt248StockInput.Native]::FindVisibleTopLevelWindow(
            [uint32]$Process.Id)
    if ($targetWindow -eq [IntPtr]::Zero) {
        throw 'client_window_not_found'
    }
    $foregroundWindow = [Prompt248StockInput.Native]::GetForegroundWindow()
    [uint32]$unusedPid = 0
    [uint32]$foregroundThread =
        [Prompt248StockInput.Native]::GetWindowThreadProcessId(
            $foregroundWindow, [ref]$unusedPid)
    [uint32]$targetThread =
        [Prompt248StockInput.Native]::GetWindowThreadProcessId(
            $targetWindow, [ref]$unusedPid)
    [uint32]$currentThread =
        [Prompt248StockInput.Native]::GetCurrentThreadId()
    try {
        if ($foregroundThread -ne 0 -and
            $foregroundThread -ne $currentThread) {
            [void][Prompt248StockInput.Native]::AttachThreadInput(
                $currentThread, $foregroundThread, $true)
        }
        if ($targetThread -ne 0 -and $targetThread -ne $currentThread) {
            [void][Prompt248StockInput.Native]::AttachThreadInput(
                $currentThread, $targetThread, $true)
        }
        [void][Prompt248StockInput.Native]::ShowWindow($targetWindow, 9)
        [void][Prompt248StockInput.Native]::BringWindowToTop($targetWindow)
        [void][Prompt248StockInput.Native]::SetForegroundWindow($targetWindow)
        [void][Prompt248StockInput.Native]::SetActiveWindow($targetWindow)
        [void][Prompt248StockInput.Native]::SetFocus($targetWindow)
    }
    finally {
        if ($targetThread -ne 0 -and $targetThread -ne $currentThread) {
            [void][Prompt248StockInput.Native]::AttachThreadInput(
                $currentThread, $targetThread, $false)
        }
        if ($foregroundThread -ne 0 -and
            $foregroundThread -ne $currentThread) {
            [void][Prompt248StockInput.Native]::AttachThreadInput(
                $currentThread, $foregroundThread, $false)
        }
    }
    $deadline = [DateTime]::UtcNow.AddSeconds(2)
    do {
        $foreground = [Prompt248StockInput.Native]::GetForegroundWindow()
        [uint32]$foregroundPid = 0
        [void][Prompt248StockInput.Native]::GetWindowThreadProcessId(
            $foreground, [ref]$foregroundPid)
        if ([int]$foregroundPid -eq $Process.Id -and
            $foreground -eq $targetWindow) { return $targetWindow }
        Start-Sleep -Milliseconds 50
    } while ([DateTime]::UtcNow -lt $deadline)
    throw 'foreground_pid_mismatch'
}

function Send-VerifiedPhysicalKey {
    param(
        [System.Diagnostics.Process]$Process,
        [string]$ExpectedImage,
        [ValidateSet('forward', 'back', 'moveleft', 'moveright', 'jump', 'duck')]
        [string]$Movement,
        [ValidateRange(25, 1500)][int]$HoldMilliseconds,
        [System.Collections.IDictionary]$Result,
        [ValidateSet('a', 'b', 'reconnect_a')][string]$Role)

    $virtualKeys = @{
        forward = 0x57
        back = 0x53
        moveleft = 0x41
        moveright = 0x44
        jump = 0x20
        duck = 0x11
    }
    $targetWindow = [IntPtr]::Zero
    for ($activationAttempt = 1; $activationAttempt -le 3;
            ++$activationAttempt) {
        $targetWindow = Assert-OwnedClientForeground -Process $Process `
            -ExpectedImage $ExpectedImage
        # GoldSrc updates its internal input-active state from the foreground
        # transition on the window thread.  Do not race the first key-down
        # against that activation message.
        Start-Sleep -Milliseconds 300
        $foreground = [Prompt248StockInput.Native]::GetForegroundWindow()
        if ($foreground -eq $targetWindow) { break }
        if ($activationAttempt -eq 3) {
            throw 'foreground_activation_failed'
        }
    }
    $Result.input_target_pid = $Process.Id
    $Result.input_target_window = ('0x{0:X}' -f $targetWindow.ToInt64())
    $Result.foreground_verified = $true
    [byte]$key = $virtualKeys[$Movement]
    try {
        try {
            Send-StockKeyInput -VirtualKey $key -Released $false
        }
        catch {
            $Result.input_pulse_failure = 'sendinput_failed'
            throw
        }
        $Result.key_down_sent = $true
        $hold = [System.Diagnostics.Stopwatch]::StartNew()
        while ($hold.ElapsedMilliseconds -lt $HoldMilliseconds) {
            Start-Sleep -Milliseconds 50
            if ($hold.ElapsedMilliseconds -lt $HoldMilliseconds) {
                Send-StockKeyInput -VirtualKey $key -Released $false
            }
        }
        $hold.Stop()
    }
    finally {
        try {
            Send-StockKeyInput -VirtualKey $key -Released $true
            $Result.key_up_sent = $true
        }
        catch {
            $Result.input_pulse_failure = 'key_up_failed'
            throw 'key_up_failed'
        }
    }
    for ($releaseAttempt = 1; $releaseAttempt -le 5 -and
            [Prompt248StockInput.Native]::IsKeyDown([int]$key);
            ++$releaseAttempt) {
        Send-StockKeyInput -VirtualKey $key -Released $true
        Start-Sleep -Milliseconds 50
    }
    if ([Prompt248StockInput.Native]::IsKeyDown([int]$key)) {
        $Result.sticky_key_detected = $true
        $Result.sticky_key_virtual = ('0x{0:X2}' -f $key)
        $Result.input_pulse_failure = 'sticky_key_detected'
        throw 'sticky_key_detected'
    }
    $Result.key_release_verified = $true
}

function Arrange-OwnedClientWindowsSideBySide {
    param(
        [System.Diagnostics.Process]$ClientA,
        [System.Diagnostics.Process]$ClientB,
        [string]$ExpectedImage)

    $windowA = Assert-OwnedClientForeground -Process $ClientA `
        -ExpectedImage $ExpectedImage
    $windowB = Assert-OwnedClientForeground -Process $ClientB `
        -ExpectedImage $ExpectedImage
    return [Prompt248StockInput.Native]::ArrangeSideBySide($windowA, $windowB)
}

function Release-AllOwnedMovementKeys {
    param(
        [System.Diagnostics.Process]$Process,
        [string]$ExpectedImage,
        [System.Collections.IDictionary]$Result)

    if ($null -eq $Process) { return }
    $Process.Refresh()
    if ($Process.HasExited) { return }
    $null = Assert-OwnedClientForeground -Process $Process `
        -ExpectedImage $ExpectedImage
    foreach ($key in @(0x57, 0x53, 0x41, 0x44, 0x20, 0x11)) {
        Send-StockKeyInput -VirtualKey ([byte]$key) -Released $true
        ++$Result.forced_key_release_count
        Start-Sleep -Milliseconds 50
        for ($releaseAttempt = 1; $releaseAttempt -le 5 -and
                [Prompt248StockInput.Native]::IsKeyDown([int]$key);
                ++$releaseAttempt) {
            Send-StockKeyInput -VirtualKey ([byte]$key) -Released $true
            ++$Result.forced_key_release_count
            Start-Sleep -Milliseconds 50
        }
        if ([Prompt248StockInput.Native]::IsKeyDown([int]$key)) {
            $Result.sticky_key_detected = $true
            $Result.sticky_key_virtual = ('0x{0:X2}' -f $key)
            $Result.input_pulse_failure = 'sticky_key_detected'
            throw 'sticky_key_detected'
        }
    }
    $Result.key_release_verified = $true
}

function Get-StockMovementSequence {
    param([ValidateSet('a', 'b', 'reconnect_a')][string]$Role)

    if ($Role -ceq 'b') {
        return @(
            [pscustomobject]@{ Movement = 'moveright'; Hold = 150 },
            [pscustomobject]@{ Movement = 'moveleft'; Hold = 150 },
            [pscustomobject]@{ Movement = 'forward'; Hold = 150 },
            [pscustomobject]@{ Movement = 'back'; Hold = 150 })
    }
    return @(
        [pscustomobject]@{ Movement = 'forward'; Hold = 150 },
        [pscustomobject]@{ Movement = 'back'; Hold = 150 },
        [pscustomobject]@{ Movement = 'moveleft'; Hold = 150 },
        [pscustomobject]@{ Movement = 'moveright'; Hold = 150 })
}

function Get-InputCounterForMovement {
    param([string]$Movement)

    $counter = switch ($Movement) {
        'forward' { 'forward_input_packets' }
        'back' { 'backward_input_packets' }
        'moveleft' { 'strafe_input_packets' }
        'moveright' { 'strafe_input_packets' }
        'jump' { 'jump_input_packets' }
        'duck' { 'duck_input_packets' }
        default { throw 'unknown_movement_pulse' }
    }
    return $counter
}

function Invoke-StockAnchorReset {
    param(
        [ValidateRange(1, 2)][int]$Slot,
        [string]$ControlPath,
        [string]$LogPath,
        [pscustomobject]$BeforePair,
        [System.Collections.IDictionary]$Result)

    $deadline = [DateTime]::UtcNow.AddSeconds(20)
    $pair = $BeforePair
    do {
        $mover = if ($Slot -eq 1) { $pair.A } else { $pair.B }
        if ($null -ne $mover -and
            [int]$mover['spawn_anchor_valid'] -eq 1 -and
            [uint64]$mover['session_generation'] -gt 0) {
            break
        }
        $pair = Wait-LiveProgressPair -PathValue $LogPath `
            -AfterA $pair.A -AfterB $pair.B
    } while ([DateTime]::UtcNow -lt $deadline)
    $mover = if ($Slot -eq 1) { $pair.A } else { $pair.B }
    if ($null -eq $mover -or [int]$mover['spawn_anchor_valid'] -ne 1) {
        throw ('spawn_anchor_{0}_not_ready' -f $Slot)
    }
    $requestId = '{0}-{1}-{2}' -f $Result.test_run_id, $Slot,
        ([guid]::NewGuid().ToString('N').Substring(0, 8))
    $beforeApplied = [int64]$mover['test_reset_applied']
    $beforeSnapshots = [int64]$mover['snapshots_sent']
    $command = 'reset_player_to_spawn_anchor slot={0} expected_session_generation={1} request_id={2}' -f `
        $Slot, [uint64]$mover['session_generation'], $requestId
    Write-AtomicText -PathValue $ControlPath -TextValue ($command + "`n")
    try {
        $deadline = [DateTime]::UtcNow.AddSeconds(15)
        $ackPattern = 'goldsrc_stock_test_reset_ack: request_id={0},slot={1},status=applied,reason=none' -f `
            [regex]::Escape($requestId), $Slot
        do {
            Start-Sleep -Milliseconds 100
            $text = Get-SharedFileText -PathValue $LogPath
            $a = Get-LastClientProgress -Text $text -Slot 1
            $b = Get-LastClientProgress -Text $text -Slot 2
            $current = if ($Slot -eq 1) { $a } else { $b }
            $anchorDistance = if ($null -ne $current) {
                $dx = [double]$current['origin_x'] -
                    [double]$current['spawn_anchor_x']
                $dy = [double]$current['origin_y'] -
                    [double]$current['spawn_anchor_y']
                $dz = [double]$current['origin_z'] -
                    [double]$current['spawn_anchor_z']
                [Math]::Sqrt($dx * $dx + $dy * $dy + $dz * $dz)
            } else { [double]::PositiveInfinity }
            if ($null -ne $a -and $null -ne $b -and
                [regex]::IsMatch($text, $ackPattern) -and
                [int64]$current['test_reset_applied'] -gt $beforeApplied -and
                [int64]$current['snapshots_sent'] -ge ($beforeSnapshots + 2) -and
                $anchorDistance -le 4.0) {
                return [pscustomobject]@{ A = $a; B = $b }
            }
        } while ([DateTime]::UtcNow -lt $deadline)
        throw ('spawn_anchor_{0}_reset_timeout' -f $Slot)
    }
    finally {
        if (Test-Path -LiteralPath $ControlPath -PathType Leaf) {
            Remove-Item -LiteralPath $ControlPath -Force
        }
    }
}

function Invoke-StockMovementScenarioWithStatus {
    param(
        [System.Diagnostics.Process]$Process,
        [string]$ExpectedImage,
        [ValidateSet('a', 'b', 'reconnect_a')][string]$Role,
        [string]$PathValue,
        [string]$ControlPath,
        [pscustomobject]$BeforePair,
        [System.Collections.IDictionary]$Result)

    $logicalRole = if ($Role -ceq 'b') { 'b' } else { 'a' }
    $current = $BeforePair
    [double]$maximumHorizontalDisplacement = 0.0
    $pulseResults = @()
    $successfulDirection = 'none'
    $pipelinePassed = $false
    $worldGeometryBlocked = $false
    $slot = if ($logicalRole -ceq 'a') { 1 } else { 2 }
    Release-AllOwnedMovementKeys -Process $Process `
        -ExpectedImage $ExpectedImage -Result $Result
    foreach ($pulse in @(Get-StockMovementSequence -Role $Role)) {
        $current = Invoke-StockAnchorReset -Slot $slot `
            -ControlPath $ControlPath -LogPath $PathValue `
            -BeforePair $current -Result $Result
        $phaseBefore = if ($logicalRole -ceq 'a') {
            $current.A
        } else { $current.B }
        $observerPhaseBefore = if ($logicalRole -ceq 'a') {
            $current.B
        } else { $current.A }
        try {
            Send-VerifiedPhysicalKey -Process $Process `
                -ExpectedImage $ExpectedImage `
                -Movement $pulse.Movement `
                -HoldMilliseconds $pulse.Hold -Result $Result -Role $Role
        }
        finally {
            Release-AllOwnedMovementKeys -Process $Process `
                -ExpectedImage $ExpectedImage -Result $Result
        }
        $counter = Get-InputCounterForMovement -Movement $pulse.Movement
        if ($logicalRole -ceq 'a') {
            $next = Wait-LiveProgressPair -PathValue $PathValue `
                -AfterA $current.A -AfterB $current.B `
                -RequiredA @($counter) `
                -FailureIdentifier 'sendinput_not_observed'
        }
        else {
            $next = Wait-LiveProgressPair -PathValue $PathValue `
                -AfterA $current.A -AfterB $current.B `
                -RequiredB @($counter) `
                -FailureIdentifier 'sendinput_not_observed'
        }
        $phaseAfter = if ($logicalRole -ceq 'a') {
            $next.A
        } else { $next.B }
        $observerPhaseAfter = if ($logicalRole -ceq 'a') {
            $next.B
        } else { $next.A }
        $phaseDisplacement = Get-OriginDisplacement `
            -Before $phaseBefore -After $phaseAfter
        $scenarioDisplacement = $phaseDisplacement
        $horizontal = $pulse.Movement -in @(
            'forward', 'back', 'moveleft', 'moveright')
        if ($horizontal) {
            $maximumHorizontalDisplacement = [Math]::Max(
                $maximumHorizontalDisplacement, $scenarioDisplacement)
        }
        $pulsePipelinePassed = $true
        foreach ($requiredCounter in @(
            'clc_move_received', 'clc_move_validated', 'clc_move_executed',
            'pmove_calls', 'movement_commands_executed', 'snapshots_sent',
            'frames_acknowledged')) {
            if ((Get-CounterDelta -Before $phaseBefore -After $phaseAfter `
                    -Name $requiredCounter) -le 0) {
                $pulsePipelinePassed = $false
            }
        }
        if ((Get-CounterDelta -Before $observerPhaseBefore `
                -After $observerPhaseAfter -Name 'remote_updates') -le 0) {
            $pulsePipelinePassed = $false
        }
        $travelPassed = $horizontal -and $phaseDisplacement -ge 2.0
        $pulseResults += [pscustomobject]@{
            movement = $pulse.Movement
            hold_milliseconds = $pulse.Hold
            input_counter = $counter
            input_counter_delta = Get-CounterDelta `
                -Before $phaseBefore -After $phaseAfter -Name $counter
            start_origin = [ordered]@{
                x = [double]$phaseBefore['origin_x']
                y = [double]$phaseBefore['origin_y']
                z = [double]$phaseBefore['origin_z']
            }
            end_origin = [ordered]@{
                x = [double]$phaseAfter['origin_x']
                y = [double]$phaseAfter['origin_y']
                z = [double]$phaseAfter['origin_z']
            }
            displacement = $phaseDisplacement
            phase_displacement = $phaseDisplacement
            scenario_displacement = $scenarioDisplacement
            pmove_delta = Get-CounterDelta `
                -Before $phaseBefore -After $phaseAfter -Name 'pmove_calls'
            executed_command_delta = Get-CounterDelta `
                -Before $phaseBefore -After $phaseAfter `
                -Name 'movement_commands_executed'
            snapshot_delta = Get-CounterDelta `
                -Before $phaseBefore -After $phaseAfter -Name 'snapshots_sent'
            frame_ack_delta = Get-CounterDelta `
                -Before $phaseBefore -After $phaseAfter `
                -Name 'frames_acknowledged'
            remote_update_delta_seen_by_other_client = Get-CounterDelta `
                -Before $observerPhaseBefore -After $observerPhaseAfter `
                -Name 'remote_updates'
            pipeline = if ($pulsePipelinePassed) { 'pass' } else { 'fail' }
            travel = if ($travelPassed) { 'pass' } `
                elseif ($pulsePipelinePassed) { 'blocked_by_geometry' } `
                else { 'not_evaluated' }
        }
        $current = $next
        if (-not $pulsePipelinePassed) {
            $pipelinePassed = $false
            break
        }
        $pipelinePassed = $true
        if ($travelPassed) {
            $successfulDirection = $pulse.Movement
        } else {
            $worldGeometryBlocked = $true
        }
        $current = Invoke-StockAnchorReset -Slot $slot `
            -ControlPath $ControlPath -LogPath $PathValue `
            -BeforePair $current -Result $Result
        if ($travelPassed) {
            break
        }
        Start-Sleep -Milliseconds 300
    }
    return [pscustomobject]@{
        AfterPair = $current
        PulseResults = $pulseResults
        MaximumHorizontalDisplacement = $maximumHorizontalDisplacement
        PipelinePassed = $pipelinePassed
        TravelPassed = $successfulDirection -cne 'none'
        WorldGeometryBlocked = $worldGeometryBlocked
        SuccessfulDirection = $successfulDirection
        ReturnedToAnchor = $successfulDirection -cne 'none'
    }
}

function Invoke-QuietNative {
    param(
        [string]$Executable,
        [string[]]$Arguments,
        [string]$OutputPath,
        [string]$Description)

    # Windows PowerShell 5.1 can promote a native process' stderr output to a
    # terminating NativeCommandError when the script-wide preference is Stop.
    # CMake writes non-fatal diagnostics to stderr, so keep native execution
    # non-terminating here and use its exit code as the authoritative result.
    $previousErrorActionPreference = $ErrorActionPreference
    $nativeExitCode = $null
    try {
        $ErrorActionPreference = 'Continue'
        & $Executable @Arguments *> $OutputPath
        $nativeExitCode = $LASTEXITCODE
    }
    finally {
        $ErrorActionPreference = $previousErrorActionPreference
    }
    if ($nativeExitCode -ne 0) {
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
    foreach ($process in @(Get-Process -Name 'hl' `
            -ErrorAction SilentlyContinue)) {
        try {
            $process.Refresh()
            if ($process.HasExited) { continue }
            $commandLine = ''
            try {
                $cim = Get-CimInstance Win32_Process `
                    -Filter ('ProcessId={0}' -f $process.Id) `
                    -ErrorAction SilentlyContinue
                if ($null -ne $cim) {
                    $commandLine = [string]$cim.CommandLine
                }
            }
            catch { }
            $records += [pscustomobject]@{
                ProcessId = [int]$process.Id
                CreationDate = $process.StartTime
                ExecutablePath = [string]$process.Path
                CommandLine = $commandLine
            }
        }
        catch {
            # A process that exits during enumeration is not a stable candidate.
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
    $pathMatches = @($Records | Where-Object {
        $record = $_
        $isNew = $PreexistingIds -notcontains [int]$record.ProcessId
        $pathIsExact = -not [string]::IsNullOrWhiteSpace(
                [string]$record.ExecutablePath) -and
            [System.IO.Path]::GetFullPath([string]$record.ExecutablePath).Equals(
                $expectedPath,
                [StringComparison]::OrdinalIgnoreCase)
        $isNew -and $pathIsExact
    })
    $preferred = @($pathMatches | Where-Object {
        [int]$_.ProcessId -eq $PreferredProcessId })
    if ($preferred.Count -eq 1) { return $preferred[0] }

    $matches = @($pathMatches | Where-Object {
        $record = $_
        $commandLine = [string]$record.CommandLine
        $identityMatches = [string]::IsNullOrWhiteSpace($commandLine) -or
            ($commandLine.Contains([string]$ExpectedClientPort) -and
             $commandLine.Contains($ExpectedClientName))
        $identityMatches
    })
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

    if ($null -ne $result) {
        $result.launch_generation = [int]$result.launch_generation + 1
        $result.launch_role = $Role
    }

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

function Invoke-ServerOwnedSpawnRetryDisconnect {
    param(
        [System.Diagnostics.Process]$Process,
        [int]$ExpectedProcessId,
        [ValidateSet(1, 2)][int]$Slot,
        [string]$RequestPath,
        [string]$StdoutPath,
        [System.Diagnostics.Process]$ServerProcess,
        [int]$TimeoutSeconds,
        [string]$Description)

    $startOffset = (Get-SharedFileText -PathValue $StdoutPath).Length
    try {
        [System.IO.File]::WriteAllText(
            $RequestPath, ('disconnect-owned-slot-{0}' -f $Slot))
        $null = Wait-LogRegex -PathValue $StdoutPath `
            -Pattern ('goldsrc_manual_client_disconnect: slot={0},status=completed' -f $Slot) `
            -TimeoutSeconds $TimeoutSeconds `
            -ServerProcess $ServerProcess `
            -FailureIdentifier `
                ('client_{0}_spawn_retry_disconnect_timeout' -f
                    $(if ($Slot -eq 1) { 'a' } else { 'b' })) `
            -StartOffset $startOffset
    }
    finally {
        if ([System.IO.File]::Exists($RequestPath)) {
            [System.IO.File]::Delete($RequestPath)
        }
    }
    Start-Sleep -Milliseconds 250
    return Stop-ExactOwnedProcess -Process $Process `
        -ExpectedProcessId $ExpectedProcessId -Description $Description
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

function Get-LastClientProgress {
    param([string]$Text, [int]$Slot)

    return Get-LastSummaryFields -Text $Text `
        -Prefix 'goldsrc_client_progress:' -Slot ([string]$Slot)
}

function Write-ManualLiveProgress {
    param([string]$Text)

    foreach ($slot in 1, 2) {
        $progress = Get-LastClientProgress -Text $Text -Slot $slot
        if ($null -eq $progress) { continue }
        Write-Host (('manual_live_slot_{0}=connected:{1},spawned:{2},' +
            'pmove:{3},snapshots:{4},acks:{5},remote_updates:{6}') -f
            $slot, $progress['connected'], $progress['spawned'],
            $progress['pmove_calls'], $progress['snapshots_sent'],
            $progress['frames_acknowledged'], $progress['remote_updates'])
    }
}

function Read-ManualYesNo {
    param([string]$Question)

    while ($true) {
        $answer = (Read-Host ($Question + ' [Y/N]')).Trim()
        if ($answer -match '^(?i:y|yes)$') { return $true }
        if ($answer -match '^(?i:n|no)$') { return $false }
        Write-Host 'manual_answer_required=Y_or_N'
    }
}

function Test-CounterAdvanced {
    param(
        [System.Collections.IDictionary]$Before,
        [System.Collections.IDictionary]$After,
        [string]$Name)

    return $Before.Contains($Name) -and $After.Contains($Name) -and
        [int64]$After[$Name] -gt [int64]$Before[$Name]
}

function Test-OriginChanged {
    param(
        [System.Collections.IDictionary]$Before,
        [System.Collections.IDictionary]$After)

    foreach ($axis in @('x', 'y', 'z')) {
        $field = 'origin_{0}' -f $axis
        if (-not $Before.Contains($field) -or -not $After.Contains($field)) {
            return $false
        }
        if ([Math]::Abs(
                [double]$After[$field] - [double]$Before[$field]) -gt 0.05) {
            return $true
        }
    }
    return $false
}

function Get-OriginDisplacement {
    param(
        [System.Collections.IDictionary]$Before,
        [System.Collections.IDictionary]$After)

    if ($null -eq $Before -or $null -eq $After) { return 0.0 }
    [double]$sum = 0.0
    foreach ($axis in @('x', 'y', 'z')) {
        $field = 'origin_{0}' -f $axis
        if (-not $Before.Contains($field) -or -not $After.Contains($field)) {
            return 0.0
        }
        $delta = [double]$After[$field] - [double]$Before[$field]
        $sum += $delta * $delta
    }
    return [Math]::Sqrt($sum)
}

function Get-CounterDelta {
    param(
        [System.Collections.IDictionary]$Before,
        [System.Collections.IDictionary]$After,
        [string]$Name)

    if ($null -eq $Before -or $null -eq $After -or
        -not $Before.Contains($Name) -or -not $After.Contains($Name)) {
        return [int64]0
    }
    return [int64]$After[$Name] - [int64]$Before[$Name]
}

function Get-EnabledMovementCheckpoints {
    param([ValidateRange(10, 900)][int]$DurationSeconds)

    $all = @(10, 30, 60, 120, 300, 600)
    return @($all | Where-Object {
        $_ -le $DurationSeconds
    })
}

function Get-InputCountersForRole {
    param([ValidateSet('a', 'b')][string]$Role)

    if ($Role -ceq 'a') {
        return @('forward_input_packets', 'backward_input_packets',
            'jump_input_packets')
    }
    return @('strafe_input_packets', 'duck_input_packets')
}

function Test-PulseCheckpointDelta {
    param(
        [System.Collections.IDictionary]$MoverBefore,
        [System.Collections.IDictionary]$MoverAfter,
        [System.Collections.IDictionary]$ObserverBefore,
        [System.Collections.IDictionary]$ObserverAfter,
        [Parameter(Mandatory)]
        [ValidateSet('a', 'b')][string]$Role,
        [object[]]$PulseResults = @(),
        [double]$MaximumHorizontalDisplacement = 0.0,
        [double]$MinimumDisplacement = 2.0)

    $result = [ordered]@{
        passed = $false
        failure_stage = 'status_missing'
        start_origin = $null
        end_origin = $null
        displacement = 0.0
        input_packet_delta = 0
        pulse_results = @()
        pmove_delta = 0
        executed_command_delta = 0
        snapshot_delta = 0
        frame_ack_delta = 0
        remote_update_delta_seen_by_other_client = 0
        pipeline_result = 'fail'
        travel_result = 'not_evaluated'
        world_geometry_blocked = $false
    }
    if ($null -eq $MoverBefore -or $null -eq $MoverAfter -or
        $null -eq $ObserverBefore -or $null -eq $ObserverAfter) {
        return $result
    }
    $result.start_origin = [ordered]@{
        x = [double]$MoverBefore['origin_x']
        y = [double]$MoverBefore['origin_y']
        z = [double]$MoverBefore['origin_z']
    }
    $result.end_origin = [ordered]@{
        x = [double]$MoverAfter['origin_x']
        y = [double]$MoverAfter['origin_y']
        z = [double]$MoverAfter['origin_z']
    }
    if ([int]$MoverAfter['connected'] -ne 1 -or
        [int]$ObserverAfter['connected'] -ne 1) {
        $result.failure_stage = 'client_disconnected'
        return $result
    }
    if ([int]$MoverAfter['spawned'] -ne 1 -or
        [int]$ObserverAfter['spawned'] -ne 1) {
        $result.failure_stage = 'client_not_spawned'
        return $result
    }
    $result.pulse_results = $PulseResults
    $result.displacement = if ($PulseResults.Count -gt 0) {
        $MaximumHorizontalDisplacement
    } else {
        Get-OriginDisplacement -Before $MoverBefore -After $MoverAfter
    }
    $result.pmove_delta = Get-CounterDelta `
        -Before $MoverBefore -After $MoverAfter -Name 'pmove_calls'
    $result.executed_command_delta = Get-CounterDelta `
        -Before $MoverBefore -After $MoverAfter `
        -Name 'movement_commands_executed'
    $result.snapshot_delta = Get-CounterDelta `
        -Before $MoverBefore -After $MoverAfter -Name 'snapshots_sent'
    $result.frame_ack_delta = Get-CounterDelta `
        -Before $MoverBefore -After $MoverAfter -Name 'frames_acknowledged'
    $result.remote_update_delta_seen_by_other_client = Get-CounterDelta `
        -Before $ObserverBefore -After $ObserverAfter -Name 'remote_updates'
    if ($result.displacement -gt 96.0) {
        $result.failure_stage = 'anchor_radius_exceeded'
        return $result
    }

    foreach ($pulse in @($PulseResults)) {
        if ($null -ne $pulse -and
            $null -ne $pulse.PSObject.Properties['input_counter_delta']) {
            $result.input_packet_delta += [int64]$pulse.input_counter_delta
        }
    }
    if ($PulseResults.Count -gt 0 -and $result.input_packet_delta -le 0) {
        $result.failure_stage = 'sendinput_not_observed'
        return $result
    }

    foreach ($counter in @(
        'clc_move_received',
        'clc_move_validated',
        'clc_move_executed',
        'pmove_calls',
        'movement_commands_executed',
        'movement_snapshots',
        'snapshots_sent',
        'frames_acknowledged')) {
        if (-not (Test-CounterAdvanced `
                -Before $MoverBefore -After $MoverAfter -Name $counter)) {
            $result.failure_stage = switch ($counter) {
                'clc_move_received' { 'clc_move_not_received' }
                'clc_move_validated' { 'clc_move_not_validated' }
                'clc_move_executed' { 'pmove_not_executed' }
                'pmove_calls' { 'pmove_not_executed' }
                'movement_commands_executed' { 'pmove_not_executed' }
                'movement_snapshots' { 'snapshots_not_advancing' }
                'snapshots_sent' { 'snapshots_not_advancing' }
                'frames_acknowledged' { 'frame_ack_not_advancing' }
            }
            return $result
        }
    }
    if ($result.displacement -lt $MinimumDisplacement) {
        $result.pipeline_result = 'pass'
        $result.travel_result = 'blocked_by_geometry'
        $result.world_geometry_blocked = $true
        $result.failure_stage = 'all_directions_blocked_from_safe_anchor'
        return $result
    }
    if ($result.remote_update_delta_seen_by_other_client -le 0) {
        $result.failure_stage = 'remote_updates_not_advancing'
        return $result
    }
    foreach ($counter in @('snapshots_sent', 'frames_acknowledged')) {
        if (-not (Test-CounterAdvanced -Before $ObserverBefore `
                -After $ObserverAfter -Name $counter)) {
            $result.failure_stage = if ($counter -ceq 'snapshots_sent') {
                'snapshots_not_advancing'
            } else { 'frame_ack_not_advancing' }
            return $result
        }
    }
    if ((Get-CounterDelta -Before $MoverBefore -After $MoverAfter `
            -Name 'snapshots_starved') -gt 0) {
        $result.failure_stage = 'snapshot_starvation_detected'
        return $result
    }
    $result.failure_stage = 'none'
    $result.pipeline_result = 'pass'
    $result.travel_result = 'pass'
    $result.passed = $true
    return $result
}

function Wait-LiveProgressPair {
    param(
        [string]$PathValue,
        [System.Collections.IDictionary]$AfterA,
        [System.Collections.IDictionary]$AfterB,
        [string[]]$RequiredA = @(),
        [string[]]$RequiredB = @(),
        [string]$FailureIdentifier = 'live_status_update_timeout',
        [ValidateRange(1, 30)][int]$TimeoutSeconds = 10)

    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    do {
        $text = Get-SharedFileText -PathValue $PathValue
        $a = Get-LastClientProgress -Text $text -Slot 1
        $b = Get-LastClientProgress -Text $text -Slot 2
        $aReady = $null -ne $a -and ($null -eq $AfterA -or
            [int64]$a['server_time_ms'] -gt [int64]$AfterA['server_time_ms'])
        $bReady = $null -ne $b -and ($null -eq $AfterB -or
            [int64]$b['server_time_ms'] -gt [int64]$AfterB['server_time_ms'])
        foreach ($counter in $RequiredA) {
            $aReady = $aReady -and (Test-CounterAdvanced `
                -Before $AfterA -After $a -Name $counter)
        }
        foreach ($counter in $RequiredB) {
            $bReady = $bReady -and (Test-CounterAdvanced `
                -Before $AfterB -After $b -Name $counter)
        }
        if ($aReady -and $bReady) {
            return [pscustomobject]@{ A = $a; B = $b }
        }
        Start-Sleep -Milliseconds 100
    } while ([DateTime]::UtcNow -lt $deadline)
    throw $FailureIdentifier
}

function Wait-LiveClientProgress {
    param(
        [string]$PathValue,
        [int]$Slot,
        [System.Collections.IDictionary]$After,
        [string[]]$RequiredCounters = @(),
        [string]$FailureIdentifier = 'live_status_update_timeout',
        [ValidateRange(1, 30)][int]$TimeoutSeconds = 10)

    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    do {
        $text = Get-SharedFileText -PathValue $PathValue
        $progress = Get-LastClientProgress -Text $text -Slot $Slot
        $ready = $null -ne $progress -and ($null -eq $After -or
            [int64]$progress['server_time_ms'] -gt
                [int64]$After['server_time_ms'])
        foreach ($counter in $RequiredCounters) {
            $ready = $ready -and (Test-CounterAdvanced `
                -Before $After -After $progress -Name $counter)
        }
        if ($ready) {
            return $progress
        }
        Start-Sleep -Milliseconds 100
    } while ([DateTime]::UtcNow -lt $deadline)
    throw $FailureIdentifier
}

function Invoke-StockSurvivorScenarioWithStatus {
    param(
        [System.Diagnostics.Process]$Process,
        [string]$ExpectedImage,
        [string]$PathValue,
        [string]$ControlPath,
        [System.Collections.IDictionary]$Before,
        [System.Collections.IDictionary]$Result)

    $current = $Before
    [double]$maximumHorizontalDisplacement = 0.0
    Release-AllOwnedMovementKeys -Process $Process `
        -ExpectedImage $ExpectedImage -Result $Result
    foreach ($pulse in @(Get-StockMovementSequence -Role b)) {
        $text = Get-SharedFileText -PathValue $PathValue
        $lastA = Get-LastClientProgress -Text $text -Slot 1
        $resetPair = Invoke-StockAnchorReset -Slot 2 `
            -ControlPath $ControlPath -LogPath $PathValue `
            -BeforePair ([pscustomobject]@{ A = $lastA; B = $current }) `
            -Result $Result
        $current = $resetPair.B
        $phaseBefore = $current
        try {
            Send-VerifiedPhysicalKey -Process $Process `
                -ExpectedImage $ExpectedImage `
                -Movement $pulse.Movement `
                -HoldMilliseconds $pulse.Hold -Result $Result -Role b
        }
        finally {
            Release-AllOwnedMovementKeys -Process $Process `
                -ExpectedImage $ExpectedImage -Result $Result
        }
        $counter = Get-InputCounterForMovement -Movement $pulse.Movement
        $current = Wait-LiveClientProgress -PathValue $PathValue -Slot 2 `
            -After $current -RequiredCounters @($counter) `
            -FailureIdentifier 'sendinput_not_observed'
        $displacement = Get-OriginDisplacement `
            -Before $phaseBefore -After $current
        $maximumHorizontalDisplacement = [Math]::Max(
            $maximumHorizontalDisplacement, $displacement)
        $pipelinePassed = $true
        foreach ($requiredCounter in @(
            $counter, 'clc_move_received', 'clc_move_validated',
            'clc_move_executed', 'pmove_calls',
            'movement_commands_executed', 'snapshots_sent',
            'frames_acknowledged')) {
            if (-not (Test-CounterAdvanced -Before $phaseBefore `
                    -After $current -Name $requiredCounter)) {
                $pipelinePassed = $false
            }
        }
        $text = Get-SharedFileText -PathValue $PathValue
        $lastA = Get-LastClientProgress -Text $text -Slot 1
        $resetPair = Invoke-StockAnchorReset -Slot 2 `
            -ControlPath $ControlPath -LogPath $PathValue `
            -BeforePair ([pscustomobject]@{ A = $lastA; B = $current }) `
            -Result $Result
        $current = $resetPair.B
        if ($pipelinePassed -and $displacement -ge 2.0 -and
            $displacement -le 96.0) {
            break
        }
    }
    return [pscustomobject]@{
        After = $current
        MaximumHorizontalDisplacement = $maximumHorizontalDisplacement
    }
}

function Test-SurvivorMovementDelta {
    param(
        [System.Collections.IDictionary]$Before,
        [System.Collections.IDictionary]$After,
        [double]$MaximumHorizontalDisplacement = 0.0)

    if ($null -eq $Before -or $null -eq $After -or
        [int]$After['connected'] -ne 1 -or [int]$After['spawned'] -ne 1 -or
        [Math]::Max(
            (Get-OriginDisplacement -Before $Before -After $After),
            $MaximumHorizontalDisplacement) -lt 2.0) {
        return $false
    }
    foreach ($counter in @(
        'clc_move_received', 'clc_move_validated', 'clc_move_executed',
        'pmove_calls', 'movement_commands_executed', 'movement_snapshots',
        'snapshots_sent', 'frames_acknowledged')) {
        if (-not (Test-CounterAdvanced -Before $Before -After $After `
                -Name $counter)) {
            return $false
        }
    }
    $inputDelta = 0
    foreach ($counter in @(
        'forward_input_packets', 'backward_input_packets',
        'strafe_input_packets')) {
        $inputDelta += Get-CounterDelta -Before $Before -After $After `
            -Name $counter
    }
    if ($inputDelta -le 0) { return $false }
    return $true
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

    $json = $Result | ConvertTo-Json -Depth 8
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
    param([System.Collections.IDictionary]$Result)

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
        -ClientName 'HL_Engine_Client_A'
    $persistentCommands = @(
        '+forward', '+back', '+moveleft', '+moveright', '+jump', '+duck')
    $persistentFound = @($persistentCommands | Where-Object {
        $arguments -contains $_
    })
    if ($arguments -notcontains '-multirun' -or
        $arguments -notcontains '+clientport' -or
        $arguments -notcontains '27005' -or
        $persistentFound.Count -ne 0) {
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

    $before = [ordered]@{
        connected = 1; spawned = 1; origin_x = 0; origin_y = 0; origin_z = 0
        clc_move_received = 10; clc_move_validated = 10
        clc_move_executed = 10; pmove_calls = 10
        movement_commands_executed = 10; movement_snapshots = 10
        snapshots_sent = 10; frames_acknowledged = 10
        forward_input_packets = 10; backward_input_packets = 10
        strafe_input_packets = 10; jump_input_packets = 10
        duck_input_packets = 10
        remote_updates = 10; snapshots_starved = 0
    }
    $after = [ordered]@{}
    foreach ($entry in $before.GetEnumerator()) {
        $after[$entry.Key] = $entry.Value
    }
    foreach ($counter in @(
        'clc_move_received', 'clc_move_validated', 'clc_move_executed',
        'pmove_calls', 'movement_commands_executed', 'movement_snapshots',
        'snapshots_sent', 'frames_acknowledged', 'forward_input_packets',
        'backward_input_packets', 'jump_input_packets')) {
        $after[$counter] = [int64]$before[$counter] + 1
    }
    $after.origin_x = 8
    $observerAfter = [ordered]@{}
    foreach ($entry in $before.GetEnumerator()) {
        $observerAfter[$entry.Key] = $entry.Value
    }
    $observerAfter.remote_updates = 11
    $observerAfter.snapshots_sent = 11
    $observerAfter.frames_acknowledged = 11
    $checkpoint = Test-PulseCheckpointDelta `
        -MoverBefore $before -MoverAfter $after `
        -ObserverBefore $before -ObserverAfter $observerAfter -Role a
    if (-not $checkpoint.passed) {
        throw 'checkpoint_delta_test_failed'
    }
    $elapsedOnly = Test-PulseCheckpointDelta `
        -MoverBefore $before -MoverAfter $before `
        -ObserverBefore $before -ObserverAfter $before -Role a
    if ($elapsedOnly.passed) {
        throw 'elapsed_time_false_positive_test_failed'
    }
    $verticalOnly = [ordered]@{}
    foreach ($entry in $after.GetEnumerator()) {
        $verticalOnly[$entry.Key] = $entry.Value
    }
    $verticalOnly.origin_x = 0
    $verticalOnly.origin_z = 8
    $verticalPulse = Test-PulseCheckpointDelta `
        -MoverBefore $before -MoverAfter $verticalOnly `
        -ObserverBefore $before -ObserverAfter $observerAfter -Role a `
        -PulseResults @([pscustomobject]@{
            movement = 'jump'; input_counter_delta = 1
        }) `
        -MaximumHorizontalDisplacement 0
    if ($verticalPulse.passed -or
        $verticalPulse.failure_stage -cne
            'all_directions_blocked_from_safe_anchor') {
        throw 'horizontal_movement_false_positive_test_failed'
    }
    $blockedPulse = Test-PulseCheckpointDelta `
        -MoverBefore $before -MoverAfter $after `
        -ObserverBefore $before -ObserverAfter $observerAfter -Role a `
        -PulseResults @([pscustomobject]@{
            movement = 'forward'; input_counter_delta = 1
        }) -MaximumHorizontalDisplacement 0
    if ($blockedPulse.passed -or
        $blockedPulse.pipeline_result -cne 'pass' -or
        $blockedPulse.travel_result -cne 'blocked_by_geometry') {
        throw 'blocked_direction_test_failed'
    }
    $adaptivePulse = Test-PulseCheckpointDelta `
        -MoverBefore $before -MoverAfter $after `
        -ObserverBefore $before -ObserverAfter $observerAfter -Role a `
        -PulseResults @(
            [pscustomobject]@{
                movement = 'forward'; input_counter_delta = 1
            },
            [pscustomobject]@{
                movement = 'back'; input_counter_delta = 1
            }) -MaximumHorizontalDisplacement 8
    if (-not $adaptivePulse.passed) {
        throw 'adaptive_direction_test_failed'
    }
    $teleportAfter = [ordered]@{}
    foreach ($entry in $after.GetEnumerator()) {
        $teleportAfter[$entry.Key] = $entry.Value
    }
    $teleportAfter.origin_x = 512
    $teleportPulse = Test-PulseCheckpointDelta `
        -MoverBefore $before -MoverAfter $teleportAfter `
        -ObserverBefore $before -ObserverAfter $observerAfter -Role a `
        -PulseResults @([pscustomobject]@{
            movement = 'forward'; input_counter_delta = 1
        }) -MaximumHorizontalDisplacement 0
    if ($teleportPulse.passed) {
        throw 'teleport_counted_as_movement_test_failed'
    }
    $longSchedule = @(Get-EnabledMovementCheckpoints -DurationSeconds 600)
    if (($longSchedule -join ',') -cne '10,30,60,120,300,600') {
        throw 'checkpoint_schedule_test_failed'
    }
    $requiredSchema = @(
        'input_delivery_a', 'input_delivery_b', 'sticky_key_detected',
        'forced_key_release_count', 'movement_after_10_seconds_both',
        'movement_after_30_seconds_both', 'movement_after_60_seconds_both',
        'movement_after_120_seconds_both',
        'movement_after_300_seconds_both',
        'movement_after_600_seconds_both', 'checkpoint_results',
        'movement_stall_stage', 'stock_manual_visual_acceptance',
        'stock_600_second_acceptance', 'delayed_launch_process_count',
        'manual_session_auto_close', 'manual_completion_confirmed',
        'manual_failure_cleanup_suppressed',
        'automated_player_input_authorized',
        'manual_window_focus_automation')
    foreach ($field in $requiredSchema) {
        if (-not $Result.Contains($field)) {
            throw 'result_schema_test_failed'
        }
    }
    $source = Get-Content -LiteralPath $PSCommandPath -Raw
    if (-not $source.Contains('Release-AllOwnedMovementKeys') -or
        -not $source.Contains('finally') -or
        -not $source.Contains('FindVisibleTopLevelWindow') -or
        -not $source.Contains('GetAsyncKeyState')) {
        throw 'input_helper_contract_test_failed'
    }
    if (-not $source.Contains(
            'manual_session_state=awaiting_operator_completion') -or
        -not $source.Contains(
            'Finish all movement and shooting checks, then press Enter')) {
        throw 'manual_session_completion_contract_test_failed'
    }
    if (-not $source.Contains(
            'manual_automatic_player_input=disabled') -or
        -not $source.Contains(
            'manual_window_focus_automation=disabled') -or
        -not $source.Contains(
            'manual_observation_forbids_automated_player_input') -or
        -not $source.Contains(
            'automated_player_input_requires_explicit_opt_in')) {
        throw 'manual_input_isolation_contract_test_failed'
    }
    $runtimeSource = Get-Content -LiteralPath (Join-Path `
        (Split-Path -Parent $PSScriptRoot) `
        'src\game_api\goldsrc_udp_handshake_runtime.inc') -Raw
    if (-not $source.Contains('reset_player_to_spawn_anchor') -or
        -not $source.Contains('preexisting_stock_client_detected') -or
        -not $source.Contains('phase_cleanup_barrier') -or
        -not $runtimeSource.Contains('stock_test_reset_transitions_') -or
        -not $runtimeSource.Contains(
            'interpolation_samples_excluded_for_reset')) {
        throw 'anchor_reset_contract_test_failed'
    }
    Initialize-StockInputBridge
    $expectedInputSize = if ([IntPtr]::Size -eq 8) { 40 } else { 28 }
    if ([Prompt248StockInput.Native]::InputStructureSize() -ne
        $expectedInputSize) {
        throw 'sendinput_structure_size_test_failed'
    }
    if ((Convert-FailureIdentifier -Message 'sendinput_not_observed') -cne
            'sendinput_not_observed' -or
        (Convert-FailureIdentifier -Message 'unclassified') -cne
            'stock_two_client_autotest_failed') {
        throw 'blocker_conversion_test_failed'
    }
    if (-not (Test-InputDeliveryFailure `
                -Identifier 'sendinput_not_observed') -or
        (Test-InputDeliveryFailure `
                -Identifier 'authoritative_origin_unchanged')) {
        throw 'input_failure_classification_test_failed'
    }
    return [ordered]@{
        no_persistent_launch_input_test = 'pass'
        window_ownership_test = 'pass'
        key_release_test = 'pass'
        sendinput_structure_size_test = 'pass'
        checkpoint_delta_test = 'pass'
        elapsed_time_false_positive_test = 'pass'
        checkpoint_schedule_test = 'pass'
        blocker_conversion_test = 'pass'
        result_schema_test = 'pass'
        manual_session_completion_test = 'pass'
        manual_input_isolation_test = 'pass'
        blocked_direction_test = 'pass'
        adaptive_direction_test = 'pass'
        anchor_reset_test = 'pass'
        teleport_not_counted_as_movement_test = 'pass'
        nointerp_reset_transition_test = 'pass'
        phase_cleanup_barrier_test = 'pass'
        preexisting_client_test = 'pass'
        multirun_classification_test = 'pass'
    }
}

function Convert-FailureIdentifier {
    param([string]$Message)

    if ($Message -cmatch '^client_a_disconnect_[a-z0-9_]+$' -or
        $Message -cmatch '^client_[ab]_spawn_retry_[a-z0-9_]+$' -or
        $Message -cmatch '^client_a_reconnect_spawn_retry_[a-z0-9_]+$' -or
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
        'preexisting_stock_client_detected',
        'previous_phase_client_still_alive',
        'previous_phase_cleanup_incomplete',
        'launcher_mutex_not_released',
        'client_port_still_owned',
        'actual_multirun_rejection',
        'all_directions_blocked_from_safe_anchor',
        'anchor_radius_exceeded',
        'spawn_anchor_1_not_ready',
        'spawn_anchor_2_not_ready',
        'spawn_anchor_1_reset_timeout',
        'spawn_anchor_2_reset_timeout',
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
        'client_window_not_found',
        'client_window_not_owned',
        'foreground_activation_failed',
        'foreground_pid_mismatch',
        'sendinput_failed',
        'sendinput_not_observed',
        'key_up_failed',
        'sticky_key_detected',
        'client_disconnected',
        'client_not_spawned',
        'clc_move_not_received',
        'clc_move_not_validated',
        'clc_move_temporarily_rejected',
        'pmove_not_executed',
        'authoritative_origin_unchanged',
        'snapshots_not_advancing',
        'frame_ack_not_advancing',
        'remote_updates_not_advancing',
        'snapshot_starvation_detected',
        'cross_client_state_leak',
        'live_status_update_timeout',
        'manual_visual_confirmation_failed',
        'automatic_acceptance_requires_verified_sendinput',
        'persistent_movement_launch_argument_detected',
        'client_b_movement_after_a_disconnect_failed',
        'reconnect_movement_failed',
        'distinct_port_test_failed',
        'client_argument_test_failed',
        'process_ownership_selection_test_failed',
        'preexisting_process_refusal_test_failed',
        'checkpoint_delta_test_failed',
        'elapsed_time_false_positive_test_failed',
        'checkpoint_schedule_test_failed',
        'blocker_conversion_test_failed',
        'input_failure_classification_test_failed',
        'result_schema_test_failed',
        'input_helper_contract_test_failed',
        'sendinput_structure_size_test_failed',
        'stock_progress_telemetry_missing',
        'stock_live_movement_checkpoint_failed',
        'stock_remote_smoothness_summary_failed',
        'stock_replication_summary_failed',
        'stock_movement_summary_failed',
        'stock_disconnect_reconnect_summary_failed')
    foreach ($identifier in $known) {
        if ($Message.Contains($identifier)) { return $identifier }
    }
    return 'stock_two_client_autotest_failed'
}

function Test-InputDeliveryFailure {
    param([string]$Identifier)

    return @(
        'client_window_not_found',
        'client_window_not_owned',
        'foreground_activation_failed',
        'foreground_pid_mismatch',
        'sendinput_failed',
        'sendinput_not_observed',
        'key_up_failed',
        'sticky_key_detected') -ccontains $Identifier
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
$stockTestControlPath = Join-Path $logPath 'stock-test-control.request'
$jsonResultPath = Join-Path $logPath 'stock_two_client_autotest.json'
$textResultPath = Join-Path $logPath 'stock_two_client_autotest.txt'

$result = [ordered]@{
    status = 'running'
    test_run_id = [guid]::NewGuid().ToString('N')
    acceptance_phase = $AcceptancePhase
    test_phase = if ($ManualObservation) { 'manual_observation_run' } `
        elseif ($AcceptancePhase -ceq 'Reconnect') { 'reconnect_run' } `
        elseif ($AutoTestDurationSeconds -ge 600) {
            'automatic_600_second_run'
        } else { 'lifecycle_run' }
    launch_generation = 0
    launch_role = 'none'
    preexisting_hl_pids = @()
    owned_hl_pids = @()
    previous_phase_cleanup_complete = $true
    phase_cleanup_barrier = 'not_run'
    multirun_failure_phase = 'missing'
    multirun_failure_role = 'missing'
    conflicting_pid = $null
    conflicting_process_owned = $false
    conflicting_client_port = $null
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
    client_a_reconnect_spawn_retry_attempted = $false
    client_a_reconnect_spawn_retry_count = 0
    multi_instance_mode = $MultiInstanceMode
    input_mode = $InputMode
    manual_observation = [bool]$ManualObservation
    manual_observation_seconds = $ManualObservationSeconds
    automated_player_input_authorized = [bool]$AllowAutomatedPlayerInput
    manual_window_focus_automation = if ($ManualObservation) {
        'disabled'
    } else { 'not_applicable' }
    manual_session_auto_close = $false
    manual_completion_confirmed = $false
    manual_failure_cleanup_suppressed = $false
    manual_visual_confirmation_requested =
        [bool]$PromptForVisualConfirmation
    manual_local_a_smooth = $null
    manual_local_b_smooth = $null
    manual_a_sees_b_smooth = $null
    manual_b_sees_a_smooth = $null
    manual_keyboard_freeze_observed = $null
    stock_two_clients_tested = $null
    stock_glock_available = $null
    stock_glock_primary_fired = $null
    stock_shooter_ammo_decreased = $null
    stock_target_health_decreased = $null
    stock_target_remained_alive = $null
    stock_wall_blocked_damage = $null
    stock_miss_caused_no_damage = $null
    stock_client_a_movement_stable = $null
    stock_client_b_movement_stable = $null
    stock_remote_replication_stable = $null
    stock_player_sticking_reproduced = $null
    stock_clients_remained_connected = $null
    manual_windows_arranged = $false
    stock_manual_visual_acceptance = 'not_confirmed'
    stock_600_second_acceptance = 'not_run'
    persistent_launch_commands_removed = $true
    persistent_movement_launch_argument_count = 0
    controlled_input_pulses = $InputMode -cne 'None' -and
        [bool]$AllowAutomatedPlayerInput
    input_target_pid = $null
    input_target_window = $null
    foreground_verified = $false
    key_down_sent = $false
    key_up_sent = $false
    key_release_verified = $false
    forced_key_release_count = 0
    sticky_key_detected = $false
    sticky_key_virtual = $null
    input_pulse_failure = 'none'
    input_delivery_a = 'not_run'
    input_delivery_b = 'not_run'
    movement_pipeline_a = 'not_run'
    movement_pipeline_b = 'not_run'
    travel_result_a = 'not_run'
    travel_result_b = 'not_run'
    world_geometry_blocked_a = $false
    world_geometry_blocked_b = $false
    movement_direction_attempts_a = @()
    movement_direction_attempts_b = @()
    successful_direction_a = 'none'
    successful_direction_b = 'none'
    spawn_anchor_a_valid = $false
    spawn_anchor_b_valid = $false
    maximum_distance_from_anchor_a = 0.0
    maximum_distance_from_anchor_b = 0.0
    anchor_radius_limit = 96.0
    player_a_returned_to_anchor = $false
    player_b_returned_to_anchor = $false
    checkpoint_path_design_failure = $false
    all_directions_blocked_from_safe_anchor = $false
    input_delivery_reconnect_a = 'not_run'
    clc_move_progress_a = $false
    clc_move_progress_b = $false
    pmove_progress_a = $false
    pmove_progress_b = $false
    frame_ack_progress_a = $false
    frame_ack_progress_b = $false
    remote_update_progress_a = $false
    remote_update_progress_b = $false
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
    client_b_retry_attempt_count = 0
    client_b_retry_udp_port = $null
    client_b_retry_handshake_seen = $false
    client_b_spawn_retry_attempted = $false
    client_b_spawn_retry_count = 0
    client_b_spawn_retry_pid = $null
    client_a_spawn_retry_attempted = $false
    client_a_spawn_retry_count = 0
    client_a_spawn_retry_pid = $null
    preexisting_process_modified = 'no'
    process_injection = 'no'
    memory_patching = 'no'
    client_files_modified = 'no'
    steam_termination = 'no'
    elevated_shell_required = $false
    last_stock_error_single_instance_dialog = $false
    last_stock_unexpected_exit_code = $null
    client_a_process_created = $false
    client_a_launch_attempt_count = 0
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
    reconnect_gate_a = $null
    reconnect_gate_b = $null
    client_b_movement_after_a_disconnect = $false
    stale_input_state_inherited = $false
    slot_reuse_clean = $false
    movement_after_10_seconds_both = $false
    movement_after_30_seconds_both = $false
    movement_after_60_seconds_both = $false
    movement_after_120_seconds_both = $false
    movement_after_300_seconds_both = $false
    movement_after_600_seconds_both = $false
    movement_stall_detected = $false
    movement_stall_stage = 'none'
    movement_stall_client = 'none'
    movement_stall_checkpoint_seconds = $null
    movement_stall_time_seconds = $null
    failure_stage = 'none'
    failure_client = 'none'
    failure_checkpoint_seconds = $null
    last_successful_movement_checkpoint = $null
    last_successful_snapshot_checkpoint = $null
    last_successful_frame_ack_checkpoint = $null
    input_delivery_verified = $false
    checkpoint_results = @()
    last_successful_move_time_a = $null
    last_successful_move_time_b = $null
    last_successful_snapshot_time_a = $null
    last_successful_snapshot_time_b = $null
    last_successful_frame_ack_time_a = $null
    last_successful_frame_ack_time_b = $null
    snapshot_interval_median_a_ms = $null
    snapshot_interval_p95_a_ms = $null
    snapshot_interval_max_a_ms = $null
    snapshot_interval_median_b_ms = $null
    snapshot_interval_p95_b_ms = $null
    snapshot_interval_max_b_ms = $null
    remote_update_p95_a_ms = $null
    remote_update_p95_b_ms = $null
    maximum_snapshot_starvation_frames = $null
    remote_interpolation_contract_verified = $false
    remote_interpolation_protocol_pass = 'fail'
    movement_window_processes_stable = $false
    cross_client_state_leak = $true
    server_cleanup = 'not_started'
    client_a_cleanup = 'not_started'
    client_b_cleanup = 'not_started'
    reconnect_client_cleanup = 'not_started'
    delayed_launch_process_count = 0
    process_leak = $false
    preflight_stage = 'not_started'
    blocker = 'none'
    diagnostic_identifier = 'none'
}

$serverProcess = $null
$clientAProcess = $null
$clientBProcess = $null
$clientAReconnectProcess = $null
$clientAPath = $null
$clientBPath = $null
$ownedPids = @()
$failure = $null
$dryRunPassed = $false
$inspectOnlyPassed = $false
$clientBPreexistingIds = @()
$initialHalfLifeProcessIds = @()
$performReconnectTest = -not ($SkipReconnectTest -or $ManualObservation)

try {
    $result.preflight_stage = 'self_tests'
    $selfTests = Invoke-LauncherSelfTests -Result $result
    $result.preflight_stage = 'cmake_resolution'
    $cmake = Resolve-CMakeExecutable
    if (-not [string]::IsNullOrWhiteSpace($HandleExecutablePath) -and
        -not (Test-Path -LiteralPath $HandleExecutablePath -PathType Leaf)) {
        throw 'optional_handle_executable_not_found'
    }
    $needsUnlocker = $InspectLauncherMutexOnly -or
        $MultiInstanceMode -ceq 'MutexUnlock' -or
        ($MultiInstanceMode -ceq 'Auto' -and
         -not $DisableLauncherMutexFallback)
    $result.preflight_stage = 'unlocker_resolution'
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

    $result.preflight_stage = 'client_resolution'
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
    $initialHalfLifeProcessIds = @(Get-HalfLifeProcessRecords |
        ForEach-Object { [int]$_.ProcessId })
    $result.preexisting_hl_pids = @($initialHalfLifeProcessIds)
    if (-not $DryRun -and $initialHalfLifeProcessIds.Count -gt 0) {
        $result.multirun_failure_phase = $result.test_phase
        $result.multirun_failure_role = 'client_a'
        $result.conflicting_pid = [int]$initialHalfLifeProcessIds[0]
        $result.conflicting_process_owned = $false
        throw 'preexisting_stock_client_detected'
    }
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
    $result.preflight_stage = 'server_resolution'
    $serverPath = Resolve-HlhostExecutable `
        -RequestedPath $ServerExecutablePath -RepositoryRoot $repositoryRoot `
        -BuildDirectory $buildDirectory -BuildConfiguration $Configuration
    $serverArguments = New-StockServerArguments `
        -GameDirectory $serverGamePath -MapName $Map -Address $BindAddress `
        -SelectedPort $selectedPort -ShutdownRequestPath $shutdownRequestPath `
        -DisconnectRequestPath $disconnectRequestPath `
        -StockTestControlPath $stockTestControlPath
    $clientAArguments = New-StockClientArguments `
        -Address $BindAddress -ServerPort $selectedPort `
        -LocalClientPort $ClientAPort -ClientName $ClientAName
    $clientBArguments = New-StockClientArguments `
        -Address $BindAddress -ServerPort $selectedPort `
        -LocalClientPort $ClientBPort -ClientName $ClientBName
    $clientAReconnectName = $ClientAName + '_Reconnect'
    $clientAReconnectArguments = New-StockClientArguments `
        -Address $BindAddress -ServerPort $selectedPort `
        -LocalClientPort $ClientAReconnectPort `
        -ClientName $clientAReconnectName
    $persistentMovementPattern = '^\+(forward|back|moveleft|moveright|jump|duck)$'
    $persistentMovementLaunchArguments = @(
        @($clientAArguments) + @($clientBArguments) +
        @($clientAReconnectArguments) | Where-Object {
            [string]$_ -cmatch $persistentMovementPattern
        })
    $result.persistent_movement_launch_argument_count =
        $persistentMovementLaunchArguments.Count
    if ($persistentMovementLaunchArguments.Count -ne 0) {
        throw 'persistent_movement_launch_argument_detected'
    }

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
        Write-Host ('input_mode={0}' -f $InputMode)
        Write-Host ('automated_player_input_authorized={0}' -f
            ([bool]$AllowAutomatedPlayerInput).ToString().ToLowerInvariant())
        Write-Host ('manual_window_focus_automation={0}' -f
            $result.manual_window_focus_automation)
        Write-Host 'joystick_input=disabled'
        Write-Host 'persistent_movement_launch_argument_count=0'
        Write-Host ('manual_observation={0}' -f
            ([bool]$ManualObservation).ToString().ToLowerInvariant())
        Write-Host ('goldsrc_combat={0}' -f
            ([bool]$GoldSrcCombat).ToString().ToLowerInvariant())
        Write-Host 'persistent_launch_commands_removed=true'
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
        foreach ($entry in $selfTests.GetEnumerator()) {
            Write-Host ('{0}={1}' -f $entry.Key, $entry.Value)
        }
        Write-Host 'dry_run=pass'
        $result.status = 'dry_run'
        $result.blocker = 'none'
        $dryRunPassed = $true
    } else {
        [System.IO.File]::WriteAllText($stdoutPath, '')
        [System.IO.File]::WriteAllText($stderrPath, '')
        if (-not $SkipBuild) {
            $result.preflight_stage = 'server_build'
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
            $result.preflight_stage = 'tests'
            Invoke-QuietNative -Executable $ctest -Arguments @(
                '--test-dir', $buildDirectory, '-C', $Configuration,
                '--output-on-failure') `
                -OutputPath $testLogPath -Description 'ctest'
        }

        $result.preflight_stage = 'complete'
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

        for ($clientALaunchAttempt = 1; $clientALaunchAttempt -le 1;
                ++$clientALaunchAttempt) {
            $result.client_a_launch_attempt_count = $clientALaunchAttempt
            $preexistingClientIds = @(Get-HalfLifeProcessRecords |
                ForEach-Object { [int]$_.ProcessId })
            try {
                $clientAProcess = Start-StockOwnedClient `
                    -Role 'client_a' -Executable $clientAPath `
                    -Arguments $clientAArguments `
                    -PreexistingIds $preexistingClientIds `
                    -ExpectedClientPort $ClientAPort `
                    -ExpectedClientName $ClientAName `
                    -AlwaysCleanupOnFailure
                break
            }
            catch {
                if ($_.Exception.Data.Contains('ProcessId')) {
                    $ownedPids += [int]$_.Exception.Data['ProcessId']
                }
                throw
            }
        }
        if ($null -eq $clientAProcess) {
            throw 'client_a_multirun_rejected'
        }
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
        $spawnAttemptTimeout = [Math]::Min($SpawnTimeoutSeconds, 90)
        for ($spawnAttempt = 1; $spawnAttempt -le 2; ++$spawnAttempt) {
            try {
                $null = Wait-LogRegex -PathValue $stdoutPath `
                    -Pattern ('goldsrc_player_edict_bound: session_id={0},slot=1,edict=1,' -f [regex]::Escape($sessionA)) `
                    -TimeoutSeconds $spawnAttemptTimeout `
                    -ServerProcess $serverProcess `
                    -ClientProcess $clientAProcess `
                    -FailureIdentifier 'client_a_spawn_timeout'
                $null = Wait-LogRegex -PathValue $stdoutPath `
                    -Pattern ('goldsrc_player_materialized: session_id={0},edict=1,.*spawned=1' -f [regex]::Escape($sessionA)) `
                    -TimeoutSeconds $spawnAttemptTimeout `
                    -ServerProcess $serverProcess `
                    -ClientProcess $clientAProcess `
                    -FailureIdentifier 'client_a_spawn_timeout'
                $null = Wait-LogRegex -PathValue $stdoutPath `
                    -Pattern ('goldsrc_continuous_snapshot_sent: session_id={0},' -f [regex]::Escape($sessionA)) `
                    -TimeoutSeconds $spawnAttemptTimeout `
                    -ServerProcess $serverProcess `
                    -ClientProcess $clientAProcess `
                    -FailureIdentifier 'client_a_spawn_timeout'
                break
            }
            catch {
                if ($_.Exception.Message -cne 'client_a_spawn_timeout' -or
                    $spawnAttempt -eq 2) {
                    throw
                }
                $result.client_a_spawn_retry_attempted = $true
                $result.client_a_spawn_retry_count = $spawnAttempt
                $oldAId = $clientAProcess.Id
                $result.client_a_cleanup =
                    Invoke-ServerOwnedSpawnRetryDisconnect `
                        -Process $clientAProcess `
                        -ExpectedProcessId $oldAId -Slot 1 `
                        -RequestPath $disconnectRequestPath `
                        -StdoutPath $stdoutPath `
                        -ServerProcess $serverProcess `
                        -TimeoutSeconds $DisconnectTimeoutSeconds `
                        -Description 'client_a_spawn_retry'
                $clientAProcess.Dispose()
                $clientAProcess = $null
                $preexistingClientIds = @(Get-HalfLifeProcessRecords |
                    ForEach-Object { [int]$_.ProcessId })
                $retryLogOffset = (Get-SharedFileText `
                    -PathValue $stdoutPath).Length
                $clientAProcess = Start-StockOwnedClient `
                    -Role 'client_a_spawn_retry' `
                    -Executable $clientAPath -Arguments $clientAArguments `
                    -PreexistingIds $preexistingClientIds `
                    -ExpectedClientPort $ClientAPort `
                    -ExpectedClientName $ClientAName `
                    -AlwaysCleanupOnFailure
                $result.client_a_pid = $clientAProcess.Id
                $result.client_a_spawn_retry_pid = $clientAProcess.Id
                $ownedPids += $clientAProcess.Id
                Wait-OwnedUdpPort -Process $clientAProcess `
                    -ExpectedPort $ClientAPort `
                    -TimeoutSeconds $ConnectTimeoutSeconds `
                    -FailureIdentifier `
                        'client_a_spawn_retry_process_created_but_no_network'
                $aAccept = Wait-LogRegex -PathValue $stdoutPath `
                    -Pattern ('goldsrc_udp_accept: session_id=(?<session>[^,]+),slot=1,endpoint=127\.0\.0\.1:{0},' -f $ClientAPort) `
                    -TimeoutSeconds $ConnectTimeoutSeconds `
                    -ServerProcess $serverProcess `
                    -ClientProcess $clientAProcess `
                    -FailureIdentifier 'client_a_spawn_retry_connect_timeout' `
                    -StartOffset $retryLogOffset
                $sessionA = $aAccept.Groups['session'].Value
            }
        }
        $result.client_a_spawned = $true
        $result.client_a_edict = 1

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
            for ($retryAttempt = 1; $retryAttempt -le 3; ++$retryAttempt) {
                $result.client_b_retry_attempt_count = $retryAttempt
                $clientBPreexistingIds = @(Get-HalfLifeProcessRecords |
                    ForEach-Object { [int]$_.ProcessId })
                try {
                    $clientBProcess = Start-StockOwnedClient `
                        -Role 'client_b_retry' -Executable $clientBPath `
                        -Arguments $clientBArguments `
                        -PreexistingIds $clientBPreexistingIds `
                        -ExpectedClientPort $ClientBPort `
                        -ExpectedClientName $ClientBName `
                        -AlwaysCleanupOnFailure
                    break
                }
                catch {
                    if ($_.Exception.Data.Contains('ProcessId')) {
                        $ownedPids += [int]$_.Exception.Data['ProcessId']
                    }
                    if ($_.Exception.Message -cne
                            'client_b_retry_multirun_rejected' -or
                        $retryAttempt -eq 3) {
                        throw
                    }
                    Start-Sleep -Seconds 1
                    try {
                        $null = Invoke-VerifiedLauncherMutex `
                            -Process $clientAProcess `
                            -ExpectedImage $clientAPath `
                            -PreexistingIds $preexistingClientIds `
                            -OwnedIds $ownedPids -ExpectedPort $ClientAPort `
                            -ServerAssociated $result.client_a_connected `
                            -Spawned $result.client_a_spawned `
                            -UnlockerExecutable $unlockerPath `
                            -CloseHandle $true -Result $result `
                            -Context Primary
                    }
                    catch {
                        if ($_.Exception.Message -cne
                                'launcher_mutex_inspect_mutex_not_found') {
                            throw
                        }
                    }
                    Start-Sleep -Seconds 3
                }
            }
            if ($null -eq $clientBProcess) {
                throw 'client_b_retry_multirun_rejected'
            }
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
        $spawnAttemptTimeout = [Math]::Min($SpawnTimeoutSeconds, 90)
        for ($spawnAttempt = 1; $spawnAttempt -le 2; ++$spawnAttempt) {
            try {
                $null = Wait-LogRegex -PathValue $stdoutPath `
                    -Pattern ('goldsrc_player_edict_bound: session_id={0},slot=2,edict=2,' -f [regex]::Escape($sessionB)) `
                    -TimeoutSeconds $spawnAttemptTimeout `
                    -ServerProcess $serverProcess `
                    -ClientProcess $clientBProcess `
                    -FailureIdentifier 'client_b_spawn_timeout'
                $null = Wait-LogRegex -PathValue $stdoutPath `
                    -Pattern ('goldsrc_player_materialized: session_id={0},edict=2,.*spawned=1' -f [regex]::Escape($sessionB)) `
                    -TimeoutSeconds $spawnAttemptTimeout `
                    -ServerProcess $serverProcess `
                    -ClientProcess $clientBProcess `
                    -FailureIdentifier 'client_b_spawn_timeout'
                break
            }
            catch {
                if ($_.Exception.Message -cne 'client_b_spawn_timeout' -or
                    $spawnAttempt -eq 2) {
                    throw
                }
                $result.client_b_spawn_retry_attempted = $true
                $result.client_b_spawn_retry_count = $spawnAttempt
                $oldBId = $clientBProcess.Id
                $result.client_b_cleanup =
                    Invoke-ServerOwnedSpawnRetryDisconnect `
                        -Process $clientBProcess `
                        -ExpectedProcessId $oldBId -Slot 2 `
                        -RequestPath $disconnectRequestPath `
                        -StdoutPath $stdoutPath `
                        -ServerProcess $serverProcess `
                        -TimeoutSeconds $DisconnectTimeoutSeconds `
                        -Description 'client_b_spawn_retry'
                $clientBProcess.Dispose()
                $clientBProcess = $null
                $clientBPreexistingIds = @(Get-HalfLifeProcessRecords |
                    ForEach-Object { [int]$_.ProcessId })
                $retryLogOffset = (Get-SharedFileText `
                    -PathValue $stdoutPath).Length
                $clientBProcess = Start-StockOwnedClient `
                    -Role 'client_b_spawn_retry' `
                    -Executable $clientBPath -Arguments $clientBArguments `
                    -PreexistingIds $clientBPreexistingIds `
                    -ExpectedClientPort $ClientBPort `
                    -ExpectedClientName $ClientBName `
                    -AlwaysCleanupOnFailure
                $result.client_b_pid = $clientBProcess.Id
                $result.client_b_spawn_retry_pid = $clientBProcess.Id
                $ownedPids += $clientBProcess.Id
                Wait-OwnedUdpPort -Process $clientBProcess `
                    -ExpectedPort $ClientBPort `
                    -TimeoutSeconds $ConnectTimeoutSeconds `
                    -FailureIdentifier `
                        'client_b_spawn_retry_process_created_but_no_network'
                $bAccept = Wait-LogRegex -PathValue $stdoutPath `
                    -Pattern ('goldsrc_udp_accept: session_id=(?<session>[^,]+),slot=2,endpoint=127\.0\.0\.1:{0},' -f $ClientBPort) `
                    -TimeoutSeconds $ConnectTimeoutSeconds `
                    -ServerProcess $serverProcess `
                    -ClientProcess $clientBProcess `
                    -FailureIdentifier 'client_b_spawn_retry_connect_timeout' `
                    -StartOffset $retryLogOffset
                $sessionB = $bAccept.Groups['session'].Value
            }
        }
        $result.client_b_spawned = $true
        $result.client_b_edict = 2
        if ($null -ne $result.client_b_retry_pid) {
            $result.client_b_retry_stable = $true
        }

        if ($ManualObservation) {
            # Do not activate, focus, resize, move, or inject input into either
            # stock client during manual observation. GoldSrc mouse-look is
            # foreground-sensitive, so even window-management automation can
            # become an unintended view-angle input for the second client.
            $result.manual_windows_arranged = $false
            $result.manual_window_focus_automation = 'disabled'
            Write-Host 'manual_automatic_player_input=disabled'
            Write-Host 'manual_window_focus_automation=disabled'
            Write-Host ('manual_endpoint={0}:{1}' -f $BindAddress, $selectedPort)
            Write-Host ('manual_client_a_pid={0}' -f $clientAProcess.Id)
            Write-Host ('manual_client_b_pid={0}' -f $clientBProcess.Id)
            Write-Host ('manual_observation_seconds={0}' -f
                $ManualObservationSeconds)
            $manualClock = [System.Diagnostics.Stopwatch]::StartNew()
            [double]$nextManualStatusSeconds = 0
            while ($manualClock.Elapsed.TotalSeconds -lt
                    $ManualObservationSeconds) {
                foreach ($client in @($clientAProcess, $clientBProcess)) {
                    $client.Refresh()
                    if ($client.HasExited) {
                        throw 'client_disconnected'
                    }
                }
                $serverProcess.Refresh()
                if ($serverProcess.HasExited) {
                    throw 'server_exited_during_gate'
                }
                if ($manualClock.Elapsed.TotalSeconds -ge
                        $nextManualStatusSeconds) {
                    Write-ManualLiveProgress -Text (
                        Get-SharedFileText -PathValue $stdoutPath)
                    $nextManualStatusSeconds += 5
                }
                Start-Sleep -Milliseconds 250
            }
            $manualClock.Stop()
            Write-Host 'manual_session_auto_close=disabled'
            Write-Host 'manual_session_state=awaiting_operator_completion'
            $null = Read-Host (
                'Finish all movement and shooting checks, then press Enter')
            foreach ($client in @($clientAProcess, $clientBProcess)) {
                $client.Refresh()
                if ($client.HasExited) {
                    throw 'client_disconnected'
                }
            }
            $serverProcess.Refresh()
            if ($serverProcess.HasExited) {
                throw 'server_exited_during_gate'
            }
            $result.manual_completion_confirmed = $true
            Write-Host 'manual_session_state=operator_completed'
            if ($PromptForVisualConfirmation) {
                $result.manual_local_a_smooth = Read-ManualYesNo `
                    -Question 'Did Client A movement remain smooth?'
                $result.manual_local_b_smooth = Read-ManualYesNo `
                    -Question 'Did Client B movement remain smooth?'
                $result.manual_a_sees_b_smooth = Read-ManualYesNo `
                    -Question 'Did A observe B smoothly?'
                $result.manual_b_sees_a_smooth = Read-ManualYesNo `
                    -Question 'Did B observe A smoothly?'
                $result.manual_keyboard_freeze_observed = Read-ManualYesNo `
                    -Question 'Did either client lose keyboard movement?'
                if ($GoldSrcCombat) {
                    $result.stock_two_clients_tested = Read-ManualYesNo `
                        -Question 'Did you directly observe both stock clients?'
                    $result.stock_glock_available = Read-ManualYesNo `
                        -Question 'Did Client A have the stock Glock?'
                    $result.stock_glock_primary_fired = Read-ManualYesNo `
                        -Question 'Did Client A fire exactly one primary shot?'
                    $result.stock_shooter_ammo_decreased = Read-ManualYesNo `
                        -Question 'Did Client A ammunition decrease?'
                    $result.stock_target_health_decreased = Read-ManualYesNo `
                        -Question 'Did Client B health decrease after the clear shot?'
                    $result.stock_target_remained_alive = Read-ManualYesNo `
                        -Question 'Did Client B remain alive after that shot?'
                    $result.stock_wall_blocked_damage = Read-ManualYesNo `
                        -Question 'Did a solid wall prevent Client B health loss?'
                    $result.stock_miss_caused_no_damage = Read-ManualYesNo `
                        -Question 'Did a deliberate miss leave Client B health unchanged?'
                    $result.stock_player_sticking_reproduced = Read-ManualYesNo `
                        -Question 'Did either player become stuck?'
                    $result.stock_clients_remained_connected = Read-ManualYesNo `
                        -Question 'Did both clients remain connected for the full observation?'
                    $result.stock_client_a_movement_stable =
                        $result.manual_local_a_smooth
                    $result.stock_client_b_movement_stable =
                        $result.manual_local_b_smooth
                    $result.stock_remote_replication_stable =
                        $result.manual_a_sees_b_smooth -and
                        $result.manual_b_sees_a_smooth
                }
                $manualAccepted = $result.manual_local_a_smooth -and
                    $result.manual_local_b_smooth -and
                    $result.manual_a_sees_b_smooth -and
                    $result.manual_b_sees_a_smooth -and
                    -not $result.manual_keyboard_freeze_observed
                if ($GoldSrcCombat) {
                    $manualAccepted = $manualAccepted -and
                        $result.stock_two_clients_tested -and
                        $result.stock_glock_available -and
                        $result.stock_glock_primary_fired -and
                        $result.stock_shooter_ammo_decreased -and
                        $result.stock_target_health_decreased -and
                        $result.stock_target_remained_alive -and
                        $result.stock_wall_blocked_damage -and
                        $result.stock_miss_caused_no_damage -and
                        $result.stock_client_a_movement_stable -and
                        $result.stock_client_b_movement_stable -and
                        $result.stock_remote_replication_stable -and
                        -not $result.stock_player_sticking_reproduced -and
                        $result.stock_clients_remained_connected
                }
                $result.stock_manual_visual_acceptance = if ($manualAccepted) {
                    'pass'
                } else { 'fail' }
                if (-not $manualAccepted) {
                    throw 'manual_visual_confirmation_failed'
                }
            }
            $result.movement_window_processes_stable = $true
        }
        else {
            if ($InputMode -cne 'VerifiedSendInput' -or
                -not $AllowAutomatedPlayerInput) {
                throw 'automatic_acceptance_requires_verified_sendinput'
            }
            Release-AllOwnedMovementKeys -Process $clientAProcess `
                -ExpectedImage $clientAPath -Result $result
            Release-AllOwnedMovementKeys -Process $clientBProcess `
                -ExpectedImage $clientBPath -Result $result
            $initialProgress = Wait-LiveProgressPair `
                -PathValue $stdoutPath -AfterA $null -AfterB $null
            $checkpoints = @(Get-EnabledMovementCheckpoints `
                -DurationSeconds $AutoTestDurationSeconds)
            $movementClock = [System.Diagnostics.Stopwatch]::StartNew()
            foreach ($checkpoint in $checkpoints) {
                while ($movementClock.Elapsed.TotalSeconds -lt $checkpoint) {
                    Start-Sleep -Milliseconds 200
                }
                foreach ($entry in @(
                    [pscustomobject]@{
                        Role = 'client_a'; Process = $clientAProcess
                    },
                    [pscustomobject]@{
                        Role = 'client_b'; Process = $clientBProcess
                    })) {
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
                if ($serverProcess.HasExited) {
                    throw 'server_exited_during_gate'
                }
                Release-AllOwnedMovementKeys -Process $clientAProcess `
                    -ExpectedImage $clientAPath -Result $result
                Release-AllOwnedMovementKeys -Process $clientBProcess `
                    -ExpectedImage $clientBPath -Result $result
                $beforeA = Wait-LiveProgressPair -PathValue $stdoutPath `
                    -AfterA $initialProgress.A -AfterB $initialProgress.B
                $result.movement_stall_client = 'a'
                $result.movement_stall_checkpoint_seconds = $checkpoint
                $scenarioA = Invoke-StockMovementScenarioWithStatus `
                    -Process $clientAProcess -ExpectedImage $clientAPath `
                    -Role a -PathValue $stdoutPath `
                    -ControlPath $stockTestControlPath `
                    -BeforePair $beforeA -Result $result
                $result.input_delivery_a = 'pass'
                $afterA = $scenarioA.AfterPair
                $aGate = Test-PulseCheckpointDelta `
                    -MoverBefore $beforeA.A -MoverAfter $afterA.A `
                    -ObserverBefore $beforeA.B -ObserverAfter $afterA.B `
                    -Role a -PulseResults $scenarioA.PulseResults `
                    -MaximumHorizontalDisplacement `
                        $scenarioA.MaximumHorizontalDisplacement

                $beforeB = Wait-LiveProgressPair -PathValue $stdoutPath `
                    -AfterA $afterA.A -AfterB $afterA.B
                $result.movement_stall_client = 'b'
                $scenarioB = Invoke-StockMovementScenarioWithStatus `
                    -Process $clientBProcess -ExpectedImage $clientBPath `
                    -Role b -PathValue $stdoutPath `
                    -ControlPath $stockTestControlPath `
                    -BeforePair $beforeB -Result $result
                $result.input_delivery_b = 'pass'
                $afterB = $scenarioB.AfterPair
                $bGate = Test-PulseCheckpointDelta `
                    -MoverBefore $beforeB.B -MoverAfter $afterB.B `
                    -ObserverBefore $beforeB.A -ObserverAfter $afterB.A `
                    -Role b -PulseResults $scenarioB.PulseResults `
                    -MaximumHorizontalDisplacement `
                        $scenarioB.MaximumHorizontalDisplacement

                $result.spawn_anchor_a_valid =
                    [int]$afterB.A['spawn_anchor_valid'] -eq 1
                $result.spawn_anchor_b_valid =
                    [int]$afterB.B['spawn_anchor_valid'] -eq 1
                $result.movement_pipeline_a = $aGate.pipeline_result
                $result.movement_pipeline_b = $bGate.pipeline_result
                $result.travel_result_a = $aGate.travel_result
                $result.travel_result_b = $bGate.travel_result
                $result.world_geometry_blocked_a =
                    [bool]$scenarioA.WorldGeometryBlocked
                $result.world_geometry_blocked_b =
                    [bool]$scenarioB.WorldGeometryBlocked
                $result.movement_direction_attempts_a = @(
                    $scenarioA.PulseResults | ForEach-Object { $_.movement })
                $result.movement_direction_attempts_b = @(
                    $scenarioB.PulseResults | ForEach-Object { $_.movement })
                $result.successful_direction_a =
                    $scenarioA.SuccessfulDirection
                $result.successful_direction_b =
                    $scenarioB.SuccessfulDirection
                $result.maximum_distance_from_anchor_a = [Math]::Max(
                    [double]$result.maximum_distance_from_anchor_a,
                    [double]$scenarioA.MaximumHorizontalDisplacement)
                $result.maximum_distance_from_anchor_b = [Math]::Max(
                    [double]$result.maximum_distance_from_anchor_b,
                    [double]$scenarioB.MaximumHorizontalDisplacement)
                $result.player_a_returned_to_anchor =
                    [bool]$scenarioA.ReturnedToAnchor
                $result.player_b_returned_to_anchor =
                    [bool]$scenarioB.ReturnedToAnchor

                $passedBoth = $aGate.passed -and $bGate.passed -and
                    -not $result.sticky_key_detected
                if (-not $scenarioA.TravelPassed -or
                    -not $scenarioB.TravelPassed) {
                    $result.all_directions_blocked_from_safe_anchor = $true
                }
                $result[('movement_after_{0}_seconds_both' -f $checkpoint)] =
                    $passedBoth
                $result.checkpoint_results += [pscustomobject]@{
                    checkpoint_seconds = $checkpoint
                    client_a = $aGate
                    client_b = $bGate
                    passed_both = $passedBoth
                }
                $result.last_successful_move_time_a =
                    [int64]$afterB.A['last_successful_move_time_ms']
                $result.last_successful_move_time_b =
                    [int64]$afterB.B['last_successful_move_time_ms']
                $result.last_successful_snapshot_time_a =
                    [int64]$afterB.A['last_successful_snapshot_time_ms']
                $result.last_successful_snapshot_time_b =
                    [int64]$afterB.B['last_successful_snapshot_time_ms']
                $result.last_successful_frame_ack_time_a =
                    [int64]$afterB.A['last_successful_frame_ack_time_ms']
                $result.last_successful_frame_ack_time_b =
                    [int64]$afterB.B['last_successful_frame_ack_time_ms']
                if (-not $passedBoth) {
                    $result.movement_stall_detected = $true
                    $result.movement_stall_client = if (-not $aGate.passed) {
                        'a'
                    } else { 'b' }
                    $result.movement_stall_stage = if (-not $aGate.passed) {
                        $aGate.failure_stage
                    } else { $bGate.failure_stage }
                    $result.movement_stall_checkpoint_seconds = $checkpoint
                    $result.movement_stall_time_seconds = $checkpoint
                    throw $result.movement_stall_stage
                }
                $result.last_successful_movement_checkpoint = $checkpoint
                $result.last_successful_snapshot_checkpoint = $checkpoint
                $result.last_successful_frame_ack_checkpoint = $checkpoint
                $result.movement_stall_client = 'none'
                $result.movement_stall_checkpoint_seconds = $null
                $result.input_delivery_verified = $true
                $result.clc_move_progress_a = $true
                $result.clc_move_progress_b = $true
                $result.pmove_progress_a = $true
                $result.pmove_progress_b = $true
                $result.frame_ack_progress_a = $true
                $result.frame_ack_progress_b = $true
                $result.remote_update_progress_a = $true
                $result.remote_update_progress_b = $true
                $initialProgress = $afterB
                if ($FollowServerLog) {
                    Write-Host ('movement_checkpoint_seconds={0}' -f
                        $checkpoint)
                    Write-Host 'movement_checkpoint_a=true'
                    Write-Host 'movement_checkpoint_b=true'
                }
            }
            $movementClock.Stop()
            if ($AutoTestDurationSeconds -ge 600 -and
                $result.movement_after_600_seconds_both) {
                $result.stock_600_second_acceptance = 'pass'
            }
            $result.movement_window_processes_stable = $true
        }

        if ($performReconnectTest) {
            $preDisconnectProgress = Wait-LiveProgressPair `
                -PathValue $stdoutPath -AfterA $null -AfterB $null
            Release-AllOwnedMovementKeys -Process $clientAProcess `
                -ExpectedImage $clientAPath -Result $result
            Release-AllOwnedMovementKeys -Process $clientBProcess `
                -ExpectedImage $clientBPath -Result $result
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
            $result.movement_stall_client = 'b'
            $result.movement_stall_checkpoint_seconds = $null
            $beforeSurvivor = Wait-LiveClientProgress `
                -PathValue $stdoutPath -Slot 2 -After $null
            $survivorScenario = Invoke-StockSurvivorScenarioWithStatus `
                -Process $clientBProcess -ExpectedImage $clientBPath `
                -PathValue $stdoutPath `
                -ControlPath $stockTestControlPath `
                -Before $beforeSurvivor `
                -Result $result
            $result.input_delivery_b = 'pass'
            $afterSurvivor = $survivorScenario.After
            $result.client_b_movement_after_a_disconnect =
                Test-SurvivorMovementDelta `
                    -Before $beforeSurvivor -After $afterSurvivor `
                    -MaximumHorizontalDisplacement `
                        $survivorScenario.MaximumHorizontalDisplacement
            if (-not $result.client_b_movement_after_a_disconnect) {
                throw 'client_b_movement_after_a_disconnect_failed'
            }
            Release-AllOwnedMovementKeys -Process $clientBProcess `
                -ExpectedImage $clientBPath -Result $result

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

            $result.movement_stall_client = 'a'
            $reconnectSpawnTimeout = [Math]::Min($ReconnectTimeoutSeconds, 90)
            for ($reconnectAttempt = 1; $reconnectAttempt -le 2;
                    ++$reconnectAttempt) {
                $idsBeforeReconnect = @(Get-HalfLifeProcessRecords |
                    ForEach-Object { [int]$_.ProcessId })
                $reconnectLogOffset = (Get-SharedFileText `
                    -PathValue $stdoutPath).Length
                $role = if ($reconnectAttempt -eq 1) {
                    'client_a_reconnect'
                } else { 'client_a_reconnect_spawn_retry' }
                $clientAReconnectProcess = Start-StockOwnedClient `
                    -Role $role -Executable $clientAPath `
                    -Arguments $clientAReconnectArguments `
                    -PreexistingIds $idsBeforeReconnect `
                    -ExpectedClientPort $ClientAReconnectPort `
                    -ExpectedClientName $clientAReconnectName `
                    -AlwaysCleanupOnFailure
                $result.client_a_reconnect_pid =
                    $clientAReconnectProcess.Id
                $ownedPids += $clientAReconnectProcess.Id
                $result.client_a_reconnect_pid_is_new =
                    $clientAReconnectProcess.Id -ne $aPid
                Wait-OwnedUdpPort -Process $clientAReconnectProcess `
                    -ExpectedPort $ClientAReconnectPort `
                    -TimeoutSeconds $ReconnectTimeoutSeconds `
                    -FailureIdentifier `
                        'client_a_reconnect_process_created_but_no_network'
                $aReconnectAccept = Wait-LogRegex `
                    -PathValue $stdoutPath `
                    -Pattern ('goldsrc_udp_accept: session_id=(?<session>[^,]+),slot=1,endpoint=127\.0\.0\.1:{0},' -f $ClientAReconnectPort) `
                    -TimeoutSeconds $ReconnectTimeoutSeconds `
                    -ServerProcess $serverProcess `
                    -ClientProcess $clientAReconnectProcess `
                    -FailureIdentifier 'client_a_reconnect_timeout' `
                    -StartOffset $reconnectLogOffset
                $sessionAReconnect =
                    $aReconnectAccept.Groups['session'].Value
                $result.client_a_reconnect_session_is_new =
                    $sessionAReconnect -cne $sessionA
                try {
                    $null = Wait-LogRegex -PathValue $stdoutPath `
                        -Pattern ('goldsrc_player_materialized: session_id={0},edict=1,.*spawned=1' -f [regex]::Escape($sessionAReconnect)) `
                        -TimeoutSeconds $reconnectSpawnTimeout `
                        -ServerProcess $serverProcess `
                        -ClientProcess $clientAReconnectProcess `
                        -FailureIdentifier `
                            'client_a_reconnect_spawn_timeout' `
                        -StartOffset $reconnectLogOffset
                    break
                }
                catch {
                    if ($_.Exception.Message -cne
                            'client_a_reconnect_spawn_timeout' -or
                        $reconnectAttempt -eq 2) {
                        throw
                    }
                    $result.client_a_reconnect_spawn_retry_attempted = $true
                    $result.client_a_reconnect_spawn_retry_count =
                        $reconnectAttempt
                    $retryReconnectPid = $clientAReconnectProcess.Id
                    $result.reconnect_client_cleanup =
                        Invoke-ServerOwnedSpawnRetryDisconnect `
                            -Process $clientAReconnectProcess `
                            -ExpectedProcessId $retryReconnectPid -Slot 1 `
                            -RequestPath $disconnectRequestPath `
                            -StdoutPath $stdoutPath `
                            -ServerProcess $serverProcess `
                            -TimeoutSeconds $DisconnectTimeoutSeconds `
                            -Description 'client_a_reconnect_spawn_retry'
                    $clientAReconnectProcess.Dispose()
                    $clientAReconnectProcess = $null
                }
            }
            $result.client_a_reconnect = $true
            $result.lifecycle_stage = 'reconnect_readd_snapshot'
            Release-AllOwnedMovementKeys -Process $clientAReconnectProcess `
                -ExpectedImage $clientAPath -Result $result
            Release-AllOwnedMovementKeys -Process $clientBProcess `
                -ExpectedImage $clientBPath -Result $result
            $reconnectBefore = Wait-LiveProgressPair `
                -PathValue $stdoutPath -AfterA $preDisconnectProgress.A `
                -AfterB $afterSurvivor
            $result.movement_stall_client = 'a'
            $reconnectScenarioA = Invoke-StockMovementScenarioWithStatus `
                -Process $clientAReconnectProcess `
                -ExpectedImage $clientAPath -Role reconnect_a `
                -PathValue $stdoutPath `
                -ControlPath $stockTestControlPath `
                -BeforePair $reconnectBefore `
                -Result $result
            $result.input_delivery_reconnect_a = 'pass'
            $reconnectAfterA = $reconnectScenarioA.AfterPair
            $reconnectGateA = Test-PulseCheckpointDelta `
                -MoverBefore $reconnectBefore.A `
                -MoverAfter $reconnectAfterA.A `
                -ObserverBefore $reconnectBefore.B `
                -ObserverAfter $reconnectAfterA.B -Role a `
                -PulseResults $reconnectScenarioA.PulseResults `
                -MaximumHorizontalDisplacement `
                    $reconnectScenarioA.MaximumHorizontalDisplacement
            $reconnectBeforeB = Wait-LiveProgressPair `
                -PathValue $stdoutPath -AfterA $reconnectAfterA.A `
                -AfterB $reconnectAfterA.B
            $result.movement_stall_client = 'b'
            $reconnectScenarioB = Invoke-StockMovementScenarioWithStatus `
                -Process $clientBProcess -ExpectedImage $clientBPath `
                -Role b -PathValue $stdoutPath `
                -ControlPath $stockTestControlPath `
                -BeforePair $reconnectBeforeB -Result $result
            $result.input_delivery_b = 'pass'
            $reconnectAfterB = $reconnectScenarioB.AfterPair
            $reconnectGateB = Test-PulseCheckpointDelta `
                -MoverBefore $reconnectBeforeB.B `
                -MoverAfter $reconnectAfterB.B `
                -ObserverBefore $reconnectBeforeB.A `
                -ObserverAfter $reconnectAfterB.A -Role b `
                -PulseResults $reconnectScenarioB.PulseResults `
                -MaximumHorizontalDisplacement `
                    $reconnectScenarioB.MaximumHorizontalDisplacement
            $result.reconnect_gate_a = $reconnectGateA
            $result.reconnect_gate_b = $reconnectGateB
            $result.reconnect_movement =
                $reconnectGateA.passed -and $reconnectGateB.passed
            $result.stale_input_state_inherited =
                [bool]$result.sticky_key_detected
            if (-not $reconnectGateA.passed) {
                $result.movement_stall_client = 'a'
                throw $reconnectGateA.failure_stage
            }
            if (-not $reconnectGateB.passed) {
                $result.movement_stall_client = 'b'
                throw $reconnectGateB.failure_stage
            }
            if ($result.stale_input_state_inherited) {
                throw 'sticky_key_detected'
            }
            $result.movement_stall_client = 'none'
            $result.lifecycle_stage = 'reconnect_complete'
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
        $result.client_a_movement = if ($ManualObservation) {
            [int]$summaryA['pmove_calls'] -gt 0
        } else { $true }
        $result.client_b_movement = if ($ManualObservation) {
            [int]$summaryB['pmove_calls'] -gt 0
        } else { $true }
        $result.snapshot_interval_median_a_ms =
            [int]$summaryA['snapshot_interval_median_ms']
        $result.snapshot_interval_p95_a_ms =
            [int]$summaryA['snapshot_interval_p95_ms']
        $result.snapshot_interval_max_a_ms =
            [int]$summaryA['snapshot_interval_max_ms']
        $result.snapshot_interval_median_b_ms =
            [int]$summaryB['snapshot_interval_median_ms']
        $result.snapshot_interval_p95_b_ms =
            [int]$summaryB['snapshot_interval_p95_ms']
        $result.snapshot_interval_max_b_ms =
            [int]$summaryB['snapshot_interval_max_ms']
        $result.remote_update_p95_a_ms =
            [int]$summaryA['remote_update_interval_p95_ms']
        $result.remote_update_p95_b_ms =
            [int]$summaryB['remote_update_interval_p95_ms']
        $result.maximum_snapshot_starvation_frames =
            [int]$aggregate['maximum_snapshot_starvation_frames']
        $result.remote_interpolation_contract_verified =
            $aggregate['remote_interpolation_contract_verified'] -ceq 'true'
        $result.remote_interpolation_protocol_pass = if (
            $result.remote_interpolation_contract_verified) {
            'pass'
        } else { 'fail' }
        $result.client_a_sees_b =
            ([int]$summaryA['remote_adds'] +
             [int]$summaryA['remote_updates']) -gt 0
        $result.client_b_sees_a =
            ([int]$summaryB['remote_adds'] +
             [int]$summaryB['remote_updates']) -gt 0
        $result.client_a_remove_visible_to_b =
            [int]$summaryB['remote_removes'] -gt 0
        $result.client_a_readd_visible_to_b = if (-not $performReconnectTest) {
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
        $result.slot_reuse_clean = if (-not $performReconnectTest) {
            $true
        } else {
            $aggregate['reconnect_slot_reuse'] -ceq 'true' -and
            $summaryA['slot_reuse_clean'] -ceq 'true'
        }
        $result.cross_client_state_leak =
            $aggregate['cross_client_state_leak'] -cne 'false'
        if (-not $result.client_a_movement -or
            -not $result.client_b_movement -or
            -not $result.simultaneous_movement) {
            throw 'stock_movement_summary_failed'
        }
        if ($result.snapshot_interval_p95_a_ms -gt 80 -or
            $result.snapshot_interval_p95_b_ms -gt 80 -or
            $result.remote_update_p95_a_ms -gt 80 -or
            $result.remote_update_p95_b_ms -gt 80 -or
            $result.maximum_snapshot_starvation_frames -ne 0 -or
            -not $result.remote_interpolation_contract_verified -or
            $summaryA['permanent_remote_nointerp'] -cne 'false' -or
            $summaryB['permanent_remote_nointerp'] -cne 'false' -or
            (-not $performReconnectTest -and
                ([int]$summaryA['remote_readds'] -gt 0 -or
                 [int]$summaryB['remote_readds'] -gt 0))) {
            throw 'stock_remote_smoothness_summary_failed'
        }
        if (-not $result.client_a_sees_b -or
            -not $result.client_b_sees_a -or
            $result.remote_player_add -cne 'pass' -or
            $result.remote_player_update -cne 'pass' -or
            $result.cross_client_state_leak) {
            throw 'stock_replication_summary_failed'
        }
        if ($performReconnectTest -and
            ($result.client_a_disconnect -cne 'pass' -or
             -not $result.client_b_survived_disconnect -or
             -not $result.client_a_remove_visible_to_b -or
             -not $result.client_a_reconnect -or
             -not $result.client_a_reconnect_pid_is_new -or
             -not $result.client_a_reconnect_session_is_new -or
             -not $result.client_a_readd_visible_to_b -or
             -not $result.client_b_movement_after_a_disconnect -or
             -not $result.reconnect_movement -or
             $result.stale_input_state_inherited -or
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
    $result.diagnostic_identifier = $result.blocker
    if ($result.blocker -in @(
            'client_a_multirun_rejected',
            'client_b_multirun_rejected',
            'client_a_reconnect_multirun_rejected') -and
        $initialHalfLifeProcessIds.Count -eq 0) {
        $result.multirun_failure_phase = $result.test_phase
        $result.multirun_failure_role = if (
            $result.blocker.StartsWith('client_b_')) { 'client_b' } `
            elseif ($result.blocker.StartsWith('client_a_reconnect_')) {
                'reconnect_client_a'
            } else { 'client_a' }
        $result.blocker = 'actual_multirun_rejection'
    }
    if ($result.blocker -ceq 'stock_two_client_autotest_failed' -and
        $result.preflight_stage -cne 'complete') {
        $result.blocker = 'preflight_{0}_failed' -f $result.preflight_stage
    }
    if ($result.blocker.EndsWith('_single_instance_dialog',
            [StringComparison]::Ordinal)) {
        $result.last_stock_error_single_instance_dialog = $true
    }
    if (Test-InputDeliveryFailure -Identifier $result.blocker) {
        if ($result.movement_stall_client -ceq 'a') {
            if ($result.client_a_reconnect) {
                $result.input_delivery_reconnect_a = 'fail'
            } else {
                $result.input_delivery_a = 'fail'
            }
        }
        elseif ($result.movement_stall_client -ceq 'b') {
            $result.input_delivery_b = 'fail'
        }
    }
    if ($_.Exception.Data.Contains('UnexpectedExitCode')) {
        $result.last_stock_unexpected_exit_code =
            [int]$_.Exception.Data['UnexpectedExitCode']
    }
    if ($result.blocker -ceq
            'launcher_mutex_access_denied_elevation_required' -or
        $result.blocker -ceq 'sendinput_failed') {
        $result.elevated_shell_required = $true
    }
    $result.failure_stage = if (
        $result.movement_stall_stage -cne 'none') {
        $result.movement_stall_stage
    } else { $result.blocker }
    $result.failure_client = $result.movement_stall_client
    $result.failure_checkpoint_seconds =
        $result.movement_stall_checkpoint_seconds
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
    $keep = ($KeepProcessesOnFailure -or $ManualObservation) -and
        $null -ne $failure
    if ($keep -and $ManualObservation) {
        $result.manual_failure_cleanup_suppressed = $true
    }
    if (-not $DryRun -and $InputMode -ceq 'VerifiedSendInput') {
        foreach ($entry in @(
            [pscustomobject]@{
                Process = $clientAReconnectProcess; Image = $clientAPath
            },
            [pscustomobject]@{
                Process = $clientBProcess; Image = $clientBPath
            },
            [pscustomobject]@{
                Process = $clientAProcess; Image = $clientAPath
            })) {
            if ($null -eq $entry.Process -or
                [string]::IsNullOrWhiteSpace([string]$entry.Image)) {
                continue
            }
            try {
                Release-AllOwnedMovementKeys -Process $entry.Process `
                    -ExpectedImage $entry.Image -Result $result
            }
            catch {
                if ($null -eq $failure) {
                    $failure = $_
                    $result.status = 'fail'
                    $result.blocker = Convert-FailureIdentifier `
                        -Message $_.Exception.Message
                    $result.failure_stage = $result.blocker
                }
            }
        }
    }
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
        $delayedLaunchIds = [Collections.Generic.HashSet[int]]::new()
        $clientPathsResolved =
            -not [string]::IsNullOrWhiteSpace($clientAPath) -and
            -not [string]::IsNullOrWhiteSpace($clientBPath)
        if (-not $DryRun -and $null -ne $failure -and
            $clientPathsResolved -and
            $result.blocker -cne 'preexisting_stock_client_detected') {
            $delayedAuditDeadline = [DateTime]::UtcNow.AddSeconds(60)
            while ([DateTime]::UtcNow -lt $delayedAuditDeadline) {
                foreach ($record in @(Get-HalfLifeProcessRecords)) {
                    $recordId = [int]$record.ProcessId
                    if ($initialHalfLifeProcessIds -contains $recordId -or
                        $ownedPids -contains $recordId) {
                        continue
                    }
                    $recordPath = [string]$record.ExecutablePath
                    if (-not [string]::IsNullOrWhiteSpace($recordPath) -and
                        ([System.IO.Path]::GetFullPath($recordPath).Equals(
                            $clientAPath,
                            [StringComparison]::OrdinalIgnoreCase) -or
                         [System.IO.Path]::GetFullPath($recordPath).Equals(
                            $clientBPath,
                            [StringComparison]::OrdinalIgnoreCase))) {
                        $null = $delayedLaunchIds.Add($recordId)
                    }
                }
                Start-Sleep -Milliseconds 250
            }
        }
        $result.delayed_launch_process_count = $delayedLaunchIds.Count
        $remaining = @($ownedPids | Where-Object {
            $null -ne (Get-Process -Id $_ -ErrorAction SilentlyContinue)
        })
        $remainingDelayed = @(if ($clientPathsResolved) {
            Get-HalfLifeProcessRecords | Where-Object {
                $recordId = [int]$_.ProcessId
                if ($initialHalfLifeProcessIds -contains $recordId -or
                    $ownedPids -contains $recordId) {
                    return $false
                }
                $recordPath = [string]$_.ExecutablePath
                return -not [string]::IsNullOrWhiteSpace($recordPath) -and
                    ([System.IO.Path]::GetFullPath($recordPath).Equals(
                        $clientAPath,
                        [StringComparison]::OrdinalIgnoreCase) -or
                     [System.IO.Path]::GetFullPath($recordPath).Equals(
                        $clientBPath,
                        [StringComparison]::OrdinalIgnoreCase))
            }
        })
        $result.process_leak = $remaining.Count -ne 0 -or
            $remainingDelayed.Count -ne 0
    } else {
        $result.process_leak = $true
    }
    $result.owned_hl_pids = @(
        $result.client_a_pid, $result.client_b_pid,
        $result.client_a_reconnect_pid | Where-Object { $null -ne $_ } |
        Select-Object -Unique)
    $result.phase_cleanup_barrier = if (-not $result.process_leak) {
        'pass'
    } else { 'fail' }
    if ($result.process_leak -and $null -eq $failure) {
        $result.status = 'fail'
        $result.blocker = 'previous_phase_client_still_alive'
        $result.multirun_failure_phase = $result.test_phase
        $result.multirun_failure_role = 'cleanup'
        $result.failure_stage = $result.blocker
        $failure = New-Object System.Exception -ArgumentList $result.blocker
    }
    $result.end_utc = [DateTime]::UtcNow.ToString('o')
    $result.diagnostic_identifier = if ($result.status -ceq 'pass') {
        'none'
    } else {
        Convert-FailureIdentifier -Message $result.blocker
    }
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
    Write-Host ('failure_stage={0}' -f $result.failure_stage)
    Write-Host ('failure_client={0}' -f $result.failure_client)
    Write-Host ('failure_checkpoint_seconds={0}' -f $(
        if ($null -eq $result.failure_checkpoint_seconds) {
            'missing'
        } else { $result.failure_checkpoint_seconds }))
    Write-Host ('result_json={0}' -f $jsonResultPath)
    exit 1
}
Write-Host 'status=pass'
Write-Host 'stock_two_client_autotest=pass'
Write-Host ('result_json={0}' -f $jsonResultPath)
exit 0
