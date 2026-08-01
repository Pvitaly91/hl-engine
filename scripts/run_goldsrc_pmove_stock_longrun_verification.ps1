[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$ExecutablePath,

    [Parameter(Mandatory = $true)]
    [string]$GameDir,

    [Parameter(Mandatory = $true)]
    [string]$ClientPath,

    [ValidateRange(30, 900)]
    [int]$MovementDurationSeconds = 610,

    [ValidateRange(660, 1200)]
    [int]$TimeoutSeconds = 750,

    [ValidateSet('varied', 'idle', 'forward', 'strafe', 'jump', 'duck')]
    [string]$MovementProfile = 'varied',

    [switch]$SkipReconnect,

    [switch]$DirectClientLaunch
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$executable = [System.IO.Path]::GetFullPath($ExecutablePath)
$gameDir = [System.IO.Path]::GetFullPath($GameDir)
$clientPath = [System.IO.Path]::GetFullPath($ClientPath)
$steamPath = $null
$steamSearch = [System.IO.Path]::GetDirectoryName($clientPath)
while (-not [string]::IsNullOrWhiteSpace($steamSearch)) {
    $candidateSteam = Join-Path $steamSearch 'steam.exe'
    if (Test-Path -LiteralPath $candidateSteam -PathType Leaf) {
        $steamPath = $candidateSteam
        break
    }
    $parentSteamSearch = [System.IO.Directory]::GetParent($steamSearch)
    if ($null -eq $parentSteamSearch) {
        break
    }
    $steamSearch = $parentSteamSearch.FullName
}
if ($DirectClientLaunch) {
    $steamPath = $null
}
$readOnlyInputs = @(
    $clientPath,
    (Join-Path $gameDir 'cl_dlls/client.dll'),
    (Join-Path $gameDir 'dlls/hl.dll'),
    (Join-Path $gameDir 'maps/crossfire.bsp'),
    (Join-Path $gameDir 'delta.lst')
)
foreach ($path in @($executable, $readOnlyInputs)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw 'stock long-run verification input is missing'
    }
}
$hashesBefore = @{}
foreach ($path in $readOnlyInputs) {
    $hashesBefore[$path] =
        (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash
}

if ($null -eq ('GoldSrcLongrunNativeInput' -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;

public static class GoldSrcLongrunNativeInput
{
    private delegate bool EnumWindowsCallback(IntPtr window, IntPtr state);

    [StructLayout(LayoutKind.Sequential)]
    private struct Input
    {
        public uint type;
        public InputUnion data;
    }

    [StructLayout(LayoutKind.Explicit, Size = 32)]
    private struct InputUnion
    {
        [FieldOffset(0)]
        public KeyboardInput keyboard;
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
    private struct Rect
    {
        public int left;
        public int top;
        public int right;
        public int bottom;
    }

    [DllImport("user32.dll")]
    public static extern IntPtr GetForegroundWindow();

    [DllImport("user32.dll")]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static extern bool SetForegroundWindow(IntPtr window);

    [DllImport("user32.dll")]
    private static extern IntPtr SetFocus(IntPtr window);

    [DllImport("user32.dll")]
    private static extern uint GetWindowThreadProcessId(
        IntPtr window,
        IntPtr processId);

    [DllImport("user32.dll")]
    private static extern uint GetWindowThreadProcessId(
        IntPtr window,
        out uint processId);

    [DllImport("user32.dll")]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool EnumWindows(
        EnumWindowsCallback callback,
        IntPtr state);

    [DllImport("user32.dll")]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool IsWindowVisible(IntPtr window);

    [DllImport("user32.dll")]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool AttachThreadInput(
        uint sourceThread,
        uint targetThread,
        bool attach);

    [DllImport("kernel32.dll")]
    private static extern uint GetCurrentThreadId();

    [DllImport("user32.dll")]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static extern bool ShowWindow(IntPtr window, int command);

    [DllImport("user32.dll")]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool BringWindowToTop(IntPtr window);

    [DllImport("user32.dll")]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool GetWindowRect(IntPtr window, out Rect rect);

    [DllImport("user32.dll", SetLastError = true)]
    private static extern uint SendInput(
        uint inputCount,
        Input[] inputs,
        int inputSize);

    [DllImport("user32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool PostMessage(
        IntPtr window,
        uint message,
        IntPtr wordParameter,
        IntPtr longParameter);

    [DllImport("user32.dll")]
    private static extern uint MapVirtualKey(uint code, uint mapType);

    [DllImport("user32.dll")]
    private static extern IntPtr GetKeyboardLayout(uint threadId);

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    private static extern IntPtr LoadKeyboardLayout(
        string layoutId,
        uint flags);

    private static bool SendKeyboard(ushort scanCode, uint flags)
    {
        Input input = new Input();
        input.type = 1;
        input.data.keyboard.scanCode = scanCode;
        input.data.keyboard.flags = flags;
        return SendInput(
            1,
            new Input[] { input },
            Marshal.SizeOf(typeof(Input))) == 1;
    }

    public static bool SendScanCode(ushort scanCode, bool keyUp)
    {
        return SendKeyboard(scanCode, 0x0008u | (keyUp ? 0x0002u : 0u));
    }

    public static bool SendText(string text)
    {
        foreach (char character in text)
        {
            if (!SendKeyboard(character, 0x0004u) ||
                !SendKeyboard(character, 0x0004u | 0x0002u))
            {
                return false;
            }
        }
        return true;
    }

    public static bool SendWindowKey(IntPtr window, uint virtualKey)
    {
        uint scanCode = MapVirtualKey(virtualKey, 0u);
        int down = 1 | ((int)scanCode << 16);
        int up = unchecked(down | (int)0xC0000000u);
        return PostMessage(
                window, 0x0100u, new IntPtr(virtualKey), new IntPtr(down))
            && PostMessage(
                window, 0x0101u, new IntPtr(virtualKey), new IntPtr(up));
    }

    public static bool SendWindowText(IntPtr window, string text)
    {
        foreach (char character in text)
        {
            if (!PostMessage(
                    window, 0x0102u, new IntPtr(character), IntPtr.Zero))
            {
                return false;
            }
        }
        return true;
    }

    public static IntPtr RequestEnglishLayout(IntPtr window)
    {
        uint targetThread = GetWindowThreadProcessId(window, IntPtr.Zero);
        IntPtr previous = GetKeyboardLayout(targetThread);
        IntPtr english = LoadKeyboardLayout("00000409", 0x00000001u);
        if (english == IntPtr.Zero ||
            !PostMessage(window, 0x0050u, IntPtr.Zero, english))
        {
            return IntPtr.Zero;
        }
        return previous;
    }

    public static bool RestoreLayout(IntPtr window, IntPtr layout)
    {
        return layout != IntPtr.Zero &&
            PostMessage(window, 0x0050u, IntPtr.Zero, layout);
    }

    public static bool RequestWindowClose(IntPtr window)
    {
        return PostMessage(window, 0x0010u, IntPtr.Zero, IntPtr.Zero);
    }

    public static bool FocusWindow(IntPtr window)
    {
        uint sourceThread = GetCurrentThreadId();
        uint targetThread = GetWindowThreadProcessId(window, IntPtr.Zero);
        IntPtr foregroundWindow = GetForegroundWindow();
        uint foregroundThread = foregroundWindow == IntPtr.Zero
            ? 0u
            : GetWindowThreadProcessId(foregroundWindow, IntPtr.Zero);
        bool foregroundNeedsAttach = foregroundThread != 0u
            && foregroundThread != sourceThread;
        bool foregroundAttached = !foregroundNeedsAttach
            || AttachThreadInput(sourceThread, foregroundThread, true);
        if (!foregroundAttached)
        {
            return false;
        }
        bool targetNeedsAttach = targetThread != sourceThread
            && targetThread != foregroundThread;
        bool targetAttached = !targetNeedsAttach
            || AttachThreadInput(sourceThread, targetThread, true);
        if (!targetAttached)
        {
            if (foregroundNeedsAttach)
            {
                AttachThreadInput(sourceThread, foregroundThread, false);
            }
            return false;
        }
        ShowWindow(window, 9);
        BringWindowToTop(window);
        SetForegroundWindow(window);
        SetFocus(window);
        bool focused = GetForegroundWindow() == window;
        if (targetNeedsAttach)
        {
            AttachThreadInput(sourceThread, targetThread, false);
        }
        if (foregroundNeedsAttach)
        {
            AttachThreadInput(sourceThread, foregroundThread, false);
        }
        return focused;
    }

    public static IntPtr FindVisibleWindow(uint processId)
    {
        IntPtr match = IntPtr.Zero;
        EnumWindows(delegate(IntPtr window, IntPtr state)
        {
            uint owner;
            GetWindowThreadProcessId(window, out owner);
            if (owner == processId && IsWindowVisible(window))
            {
                match = window;
                return false;
            }
            return true;
        }, IntPtr.Zero);
        return match;
    }

    public static bool GetWindowBounds(
        IntPtr window,
        out int left,
        out int top,
        out int right,
        out int bottom)
    {
        left = top = right = bottom = 0;
        Rect rect;
        if (!GetWindowRect(window, out rect))
        {
            return false;
        }
        left = rect.left;
        top = rect.top;
        right = rect.right;
        bottom = rect.bottom;
        return right > left && bottom > top;
    }
}
'@
}

function Get-SummaryField {
    param([string]$Line, [string]$Name)

    $match = [regex]::Match(
        $Line,
        '(?:^|[, ])' + [regex]::Escape($Name) +
            '=(?<value>[^, \r\n]+)(?=$|[, ])')
    if (-not $match.Success) {
        throw 'stock long-run summary omitted a required field'
    }
    return $match.Groups['value'].Value
}

function Wait-LogToken {
    param(
        [System.Diagnostics.Process]$Process,
        [string]$Path,
        [string]$Token,
        [int]$Count = 1,
        [int]$Seconds = 30
    )

    $deadline = [DateTime]::UtcNow.AddSeconds($Seconds)
    while ([DateTime]::UtcNow -lt $deadline) {
        if ($Process.HasExited) {
            throw 'stock long-run host exited unexpectedly'
        }
        if (Test-Path -LiteralPath $Path -PathType Leaf) {
            $matches = @(
                Select-String -LiteralPath $Path -SimpleMatch $Token `
                    -ErrorAction SilentlyContinue)
            if ($matches.Count -ge $Count) {
                return
            }
        }
        Start-Sleep -Milliseconds 100
    }
    throw 'stock long-run readiness checkpoint timed out'
}

function Set-InputFocus {
    param([IntPtr]$Window)

    if (-not [GoldSrcLongrunNativeInput]::FocusWindow($Window)) {
        throw 'stock long-run client focus failed'
    }
}

function Send-KeyState {
    param([uint16]$ScanCode, [bool]$Pressed)

    if (-not [GoldSrcLongrunNativeInput]::SendScanCode(
            $ScanCode,
            -not $Pressed)) {
        throw 'stock long-run input delivery failed'
    }
}

function Send-KeyTap {
    param([uint16]$ScanCode)

    Send-KeyState -ScanCode $ScanCode -Pressed $true
    Start-Sleep -Milliseconds 60
    Send-KeyState -ScanCode $ScanCode -Pressed $false
}

function Send-ConsoleKeyTap {
    param([uint16]$ScanCode)

    Send-KeyState -ScanCode $ScanCode -Pressed $true
    Start-Sleep -Milliseconds 25
    Send-KeyState -ScanCode $ScanCode -Pressed $false
}

function Open-ClientConsole {
    Send-KeyTap -ScanCode 0x29
}

function Send-ConsoleText {
    param([IntPtr]$Window, [string]$Text)

    $previousLayout =
        [GoldSrcLongrunNativeInput]::RequestEnglishLayout($Window)
    if ($previousLayout -eq [IntPtr]::Zero) {
        throw 'stock long-run input layout request failed'
    }
    Start-Sleep -Milliseconds 100
    try {
        $scanCodes = @{
            'a' = 0x1E; 'b' = 0x30; 'c' = 0x2E; 'd' = 0x20
            'e' = 0x12; 'f' = 0x21; 'g' = 0x22; 'h' = 0x23
            'i' = 0x17; 'j' = 0x24; 'k' = 0x25; 'l' = 0x26
            'm' = 0x32; 'n' = 0x31; 'o' = 0x18; 'p' = 0x19
            'q' = 0x10; 'r' = 0x13; 's' = 0x1F; 't' = 0x14
            'u' = 0x16; 'v' = 0x2F; 'w' = 0x11; 'x' = 0x2D
            'y' = 0x15; 'z' = 0x2C; ' ' = 0x39; '.' = 0x34
            ';' = 0x27
            '0' = 0x0B; '1' = 0x02; '2' = 0x03; '3' = 0x04
            '4' = 0x05; '5' = 0x06; '6' = 0x07; '7' = 0x08
            '8' = 0x09; '9' = 0x0A
        }
        foreach ($character in $Text.ToLowerInvariant().ToCharArray()) {
            if ($character -eq ':') {
                Send-KeyState -ScanCode 0x2A -Pressed $true
                Send-ConsoleKeyTap -ScanCode 0x27
                Send-KeyState -ScanCode 0x2A -Pressed $false
            } elseif ($character -eq '_') {
                Send-KeyState -ScanCode 0x2A -Pressed $true
                Send-ConsoleKeyTap -ScanCode 0x0C
                Send-KeyState -ScanCode 0x2A -Pressed $false
            } elseif ($scanCodes.ContainsKey([string]$character)) {
                Send-ConsoleKeyTap -ScanCode `
                    ([uint16]$scanCodes[[string]$character])
            } else {
                throw 'stock long-run console text contained an unsupported character'
            }
            Start-Sleep -Milliseconds 5
        }
    } finally {
        if (-not [GoldSrcLongrunNativeInput]::RestoreLayout(
                $Window,
                $previousLayout)) {
            # GoldSrc may recreate its top-level HWND while toggling the
            # console. A vanished target window owns no persistent layout
            # state, so restoration on that stale handle is best-effort.
            $script:layoutRestoreDeferred = $true
        }
    }
}

function Resolve-ClientWindow {
    param([int]$ProcessId)

    $process = Get-Process -Id $ProcessId -ErrorAction SilentlyContinue
    if ($null -eq $process -or $process.HasExited) {
        throw 'stock long-run client process is unavailable'
    }
    $process.Refresh()
    $windows = @(
        $process.MainWindowHandle,
        [GoldSrcLongrunNativeInput]::FindVisibleWindow(
            [uint32]$ProcessId)
    ) | Where-Object { $_ -ne [IntPtr]::Zero } | Select-Object -Unique
    foreach ($window in $windows) {
        if ([GoldSrcLongrunNativeInput]::FocusWindow($window)) {
            return $window
        }
    }
    throw 'stock long-run client window is unavailable'
}

function Get-TargetClientProcesses {
    param([int[]]$PreexistingIds)

    $targetIds = @(
        Get-CimInstance -ClassName Win32_Process `
            -Filter "Name = 'hl.exe'" `
            -ErrorAction SilentlyContinue |
            Where-Object {
                -not [string]::IsNullOrWhiteSpace($_.CommandLine) -and
                $_.CommandLine.IndexOf(
                    $script:targetClientCommandToken,
                    [StringComparison]::OrdinalIgnoreCase) -ge 0
            } |
            ForEach-Object { [int]$_.ProcessId }
    )
    return @(
        Get-Process -Name 'hl' -ErrorAction SilentlyContinue |
            Where-Object {
                $PreexistingIds -notcontains $_.Id -and
                $targetIds -contains $_.Id
            } |
            Sort-Object StartTime -Descending
    )
}

function Find-OwnedClientProcess {
    param([int[]]$PreexistingIds)

    return Get-TargetClientProcesses `
        -PreexistingIds $PreexistingIds | Select-Object -First 1
}

function Send-WindowConsoleCommand {
    param(
        [IntPtr]$Window,
        [string]$Text,
        [bool]$OpenConsole = $true
    )

    Set-InputFocus -Window $Window
    if ($OpenConsole) {
        Open-ClientConsole
        Start-Sleep -Milliseconds 250
    }
    Send-ConsoleText -Window $Window -Text $Text
    Send-ConsoleKeyTap -ScanCode 0x1C
}

function Set-MovementState {
    param([string[]]$Names)

    foreach ($entry in $script:movementScanCodes.GetEnumerator()) {
        $shouldPress = $Names -ccontains $entry.Key
        if ($script:pressed[$entry.Key] -ne $shouldPress) {
            Send-KeyState -ScanCode ([uint16]$entry.Value) `
                -Pressed $shouldPress
            $script:pressed[$entry.Key] = $shouldPress
        }
    }
}

function Capture-Checkpoint {
    param([IntPtr]$Window, [string]$Directory, [string]$Name)

    Set-InputFocus -Window $Window
    Start-Sleep -Milliseconds 100
    $path = Join-Path $Directory ($Name + '.png')
    [int]$left = 0
    [int]$top = 0
    [int]$right = 0
    [int]$bottom = 0
    if (-not [GoldSrcLongrunNativeInput]::GetWindowBounds(
            $Window,
            [ref]$left,
            [ref]$top,
            [ref]$right,
            [ref]$bottom)) {
        throw 'stock long-run checkpoint capture failed'
    }
    $bitmap = [System.Drawing.Bitmap]::new($right - $left, $bottom - $top)
    try {
        $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
        try {
            $graphics.CopyFromScreen(
                $left,
                $top,
                0,
                0,
                [System.Drawing.Size]::new($right - $left, $bottom - $top))
            $bitmap.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
        }
        finally {
            $graphics.Dispose()
        }
    }
    finally {
        $bitmap.Dispose()
    }
    return $path
}

$reservation = [System.Net.Sockets.UdpClient]::new(
    [System.Net.Sockets.AddressFamily]::InterNetwork)
$reservation.Client.Bind([System.Net.IPEndPoint]::new(
    [System.Net.IPAddress]::Loopback,
    0))
$port = ([System.Net.IPEndPoint]$reservation.Client.LocalEndPoint).Port
$reservation.Close()
$reservation.Dispose()
$script:targetClientCommandToken = '+connect 127.0.0.1:{0}' -f $port

$runId = [Guid]::NewGuid().ToString('N')
$temporaryRoot = Join-Path ([System.IO.Path]::GetTempPath()) `
    ('hlhost_stock_longrun_' + $runId)
[void][System.IO.Directory]::CreateDirectory($temporaryRoot)
$stdoutPath = Join-Path $temporaryRoot 'server.stdout.log'
$stderrPath = Join-Path $temporaryRoot 'server.stderr.log'
$shutdownPath = Join-Path $temporaryRoot 'shutdown.request'
$hostProcess = $null
$clientProcess = $null
$inputProcessId = 0
$inputWindow = [IntPtr]::Zero
$failure = $null
$checkpoints = @{}
$preexistingClientIds = @(
    Get-Process -Name 'hl' -ErrorAction SilentlyContinue |
        ForEach-Object { $_.Id })
$script:movementScanCodes = [ordered]@{
    forward = 0x11
    backward = 0x1F
    left = 0x1E
    right = 0x20
}
$script:pressed = @{
    forward = $false
    backward = $false
    left = $false
    right = $false
}
$script:duckPressed = $false
$script:layoutRestoreDeferred = $false

try {
    $hostArguments = @(
        '--gamedir', ('"{0}"' -f $gameDir),
        '--dedicated',
        '--deathmatch', '1',
        '--maxclients', '1',
        '--map', 'crossfire',
        '--frames', '1',
        '--log-to-file', '0',
        '--log-summary-file', '0',
        '--log-disable-categories=general',
        '--ip', '127.0.0.1',
        '--port', [string]$port,
        '--goldsrc-pmove',
        '--goldsrc-pmove-persistent',
        '--goldsrc-manual-shutdown-file', ('"{0}"' -f $shutdownPath),
        '--goldsrc-snapshot-rate-hz=20',
        '--goldsrc-handshake-timeout-ms', '300000'
    )
    $hostProcess = Start-Process -FilePath $executable `
        -ArgumentList $hostArguments `
        -WorkingDirectory ([System.IO.Path]::GetDirectoryName($executable)) `
        -RedirectStandardOutput $stdoutPath `
        -RedirectStandardError $stderrPath `
        -WindowStyle Hidden `
        -PassThru
    Wait-LogToken -Process $hostProcess -Path $stdoutPath `
        -Token 'goldsrc_udp_ready:' -Seconds 30

    $clientArguments = @(
        '-game', 'valve',
        '-console',
        '-novid',
        '-windowed',
        '-w', '1280',
        '-h', '720',
        '+fps_override', '0',
        '+fps_max', '100',
        '+cl_cmdrate', '100',
        '+connect', ('127.0.0.1:{0}' -f $port)
    )
    $clientLaunchPath = if ($null -ne $steamPath) {
        $steamPath
    } else {
        $clientPath
    }
    $clientLaunchArguments = if ($null -ne $steamPath) {
        @('-applaunch', '70') + $clientArguments
    } else {
        @('-steam') + $clientArguments
    }
    $clientProcess = Start-Process -FilePath $clientLaunchPath `
        -ArgumentList $clientLaunchArguments `
        -WorkingDirectory `
            ([System.IO.Path]::GetDirectoryName($clientLaunchPath)) `
        -PassThru
    # Steam may replace the initial process with the final game PID after its
    # launch/interstitial handoff. Wait through that bounded transition.
    $windowDeadline = [DateTime]::UtcNow.AddSeconds(45)
    while ([DateTime]::UtcNow -lt $windowDeadline -and
        $inputWindow -eq [IntPtr]::Zero) {
        foreach ($process in @(
            Get-TargetClientProcesses `
                -PreexistingIds $preexistingClientIds)) {
            $process.Refresh()
            $candidateWindow = $process.MainWindowHandle
            if ($candidateWindow -eq [IntPtr]::Zero) {
                $candidateWindow =
                    [GoldSrcLongrunNativeInput]::FindVisibleWindow(
                        [uint32]$process.Id)
            }
            if ($candidateWindow -ne [IntPtr]::Zero) {
                if ([GoldSrcLongrunNativeInput]::FocusWindow(
                        $candidateWindow)) {
                    Start-Sleep -Milliseconds 250
                    $process.Refresh()
                    if (-not $process.HasExited) {
                        $inputWindow = $candidateWindow
                        $inputProcessId = $process.Id
                        break
                    }
                }
            }
        }
        Start-Sleep -Milliseconds 100
    }
    if ($inputWindow -eq [IntPtr]::Zero) {
        throw 'stock long-run client window was not available'
    }
    Set-InputFocus -Window $inputWindow
    $connectArgumentDeadline = [DateTime]::UtcNow.AddSeconds(10)
    $alreadyConnected = $false
    while ([DateTime]::UtcNow -lt $connectArgumentDeadline) {
        if ($hostProcess.HasExited) {
            throw 'stock long-run host exited while awaiting client connect'
        }
        $alreadyConnected = @(
            Select-String -LiteralPath $stdoutPath `
                -SimpleMatch 'goldsrc_player_materialized:' `
                -ErrorAction SilentlyContinue).Count -ge 1
        if ($alreadyConnected) {
            break
        }
        Start-Sleep -Milliseconds 100
    }
    if (-not $alreadyConnected) {
        Send-WindowConsoleCommand `
            -Window $inputWindow `
            -Text ('connect 127.0.0.1:{0}' -f $port)
    }
    Wait-LogToken -Process $hostProcess -Path $stdoutPath `
        -Token 'goldsrc_player_materialized:' -Seconds 45

    $inputWindow = Resolve-ClientWindow -ProcessId $inputProcessId
    Send-WindowConsoleCommand `
        -Window $inputWindow `
        -Text 'fps_override 0;fps_max 100;cl_cmdrate 100'
    Open-ClientConsole
    Start-Sleep -Milliseconds 250

    $inputWindow = Resolve-ClientWindow -ProcessId $inputProcessId
    $checkpoints[0] = Capture-Checkpoint `
        -Window $inputWindow `
        -Directory $temporaryRoot `
        -Name 'checkpoint_0'

    $stopwatch = [Diagnostics.Stopwatch]::StartNew()
    $lastPattern = -1
    $lastTapSecond = -1
    $captured = @{}
    $checkpointSchedule = if ($MovementDurationSeconds -ge 600) {
        @(60, 120, 300, 600)
    } else {
        @(10, 20, 30)
    }
    while ($stopwatch.Elapsed.TotalSeconds -lt $MovementDurationSeconds) {
        if ($hostProcess.HasExited) {
            throw 'stock long-run host exited before the movement boundary'
        }
        $inputProcess = Get-Process -Id $inputProcessId `
            -ErrorAction SilentlyContinue
        if ($null -eq $inputProcess -or $inputProcess.HasExited) {
            $inputProcess = Find-OwnedClientProcess `
                -PreexistingIds $preexistingClientIds
            if ($null -eq $inputProcess -or $inputProcess.HasExited) {
                throw 'stock long-run client exited before the movement boundary'
            }
            $inputProcessId = $inputProcess.Id
            $inputWindow = Resolve-ClientWindow -ProcessId $inputProcessId
        }
        $second = [int][Math]::Floor($stopwatch.Elapsed.TotalSeconds)
        $idle = $MovementProfile -eq 'varied' -and
            $second -ge 60 -and $second -lt 90
        if ($MovementProfile -eq 'idle') {
            Set-MovementState -Names @()
        } elseif ($MovementProfile -eq 'forward') {
            Set-MovementState -Names @('forward')
        } elseif ($MovementProfile -eq 'strafe') {
            Set-MovementState -Names @('right')
        } elseif ($MovementProfile -eq 'jump') {
            Set-MovementState -Names @()
            if ($second -ne $lastTapSecond -and $second % 2 -eq 0) {
                Send-KeyTap -ScanCode 0x39
                $lastTapSecond = $second
            }
        } elseif ($MovementProfile -eq 'duck') {
            Set-MovementState -Names @()
            $duckPressed = $second % 4 -lt 2
            if ($script:duckPressed -ne $duckPressed) {
                Send-KeyState -ScanCode 0x1D -Pressed $duckPressed
                $script:duckPressed = $duckPressed
            }
        } elseif ($idle) {
            Set-MovementState -Names @()
        } else {
            $pattern = [int]([Math]::Floor($second / 5.0) % 6)
            if ($pattern -ne $lastPattern) {
                Set-InputFocus -Window $inputWindow
                $names = @(
                    @('forward'),
                    @('forward', 'right'),
                    @('forward', 'left'),
                    @('right'),
                    @('backward'),
                    @('left'))[$pattern]
                Set-MovementState -Names $names
                $lastPattern = $pattern
            }
            if ($second -ne $lastTapSecond -and $second % 11 -eq 0) {
                Send-KeyTap -ScanCode 0x39
                $lastTapSecond = $second
            } elseif ($second -ne $lastTapSecond -and
                $second % 17 -eq 0) {
                Send-KeyState -ScanCode 0x1D -Pressed $true
                Start-Sleep -Milliseconds 800
                Send-KeyState -ScanCode 0x1D -Pressed $false
                $lastTapSecond = $second
            }
        }
        foreach ($checkpoint in $checkpointSchedule) {
            if ($second -ge $checkpoint -and
                -not $captured.ContainsKey($checkpoint)) {
                $captured[$checkpoint] = $true
                $inputWindow = Resolve-ClientWindow `
                    -ProcessId $inputProcessId
                $checkpoints[$checkpoint] = Capture-Checkpoint `
                    -Window $inputWindow `
                    -Directory $temporaryRoot `
                    -Name ('checkpoint_{0}' -f $checkpoint)
            }
        }
        Start-Sleep -Milliseconds 100
    }

    Set-MovementState -Names @()
    if ($script:duckPressed) {
        Send-KeyState -ScanCode 0x1D -Pressed $false
        $script:duckPressed = $false
    }
    if (-not $SkipReconnect) {
        $inputWindow = Resolve-ClientWindow -ProcessId $inputProcessId
        Send-WindowConsoleCommand `
            -Window $inputWindow `
            -Text 'disconnect'
        $checkpoints['disconnect'] = Capture-Checkpoint `
            -Window $inputWindow `
            -Directory $temporaryRoot `
            -Name 'checkpoint_disconnect'
        Wait-LogToken -Process $hostProcess -Path $stdoutPath `
            -Token 'goldsrc_client_disconnected:' -Seconds 15

        $inputWindow = Resolve-ClientWindow -ProcessId $inputProcessId
        Send-WindowConsoleCommand `
            -Window $inputWindow `
            -Text ('connect 127.0.0.1:{0}' -f $port) `
            -OpenConsole $false
        Wait-LogToken -Process $hostProcess -Path $stdoutPath `
            -Token 'goldsrc_player_materialized:' -Count 2 -Seconds 45
        Set-InputFocus -Window $inputWindow
        Set-MovementState -Names @('forward')
        for ($index = 0; $index -lt 100; ++$index) {
            $inputProcess = Get-Process -Id $inputProcessId `
                -ErrorAction SilentlyContinue
            if ($null -eq $inputProcess -or $inputProcess.HasExited) {
                $inputProcess = Find-OwnedClientProcess `
                    -PreexistingIds $preexistingClientIds
                if ($null -ne $inputProcess -and
                    -not $inputProcess.HasExited) {
                    $inputProcessId = $inputProcess.Id
                    $inputWindow = Resolve-ClientWindow `
                        -ProcessId $inputProcessId
                }
            }
            if ($hostProcess.HasExited -or $null -eq $inputProcess -or
                $inputProcess.HasExited) {
                throw 'stock long-run reconnect did not remain active'
            }
            if ($index -eq 20) {
                Send-KeyTap -ScanCode 0x39
            }
            if ($index -eq 50) {
                Send-KeyState -ScanCode 0x1D -Pressed $true
            }
            if ($index -eq 60) {
                Send-KeyState -ScanCode 0x1D -Pressed $false
            }
            Start-Sleep -Milliseconds 100
        }
        Set-MovementState -Names @()
        $checkpoints['reconnect'] = Capture-Checkpoint `
            -Window $inputWindow `
            -Directory $temporaryRoot `
            -Name 'checkpoint_reconnect'
    }

    [System.IO.File]::WriteAllText($shutdownPath, 'stock long-run complete')
    if (-not $hostProcess.WaitForExit(30000)) {
        throw 'stock long-run host shutdown timed out'
    }
    if ($hostProcess.ExitCode -ne 0) {
        throw ('stock long-run host exited with code {0}' -f
            $hostProcess.ExitCode)
    }

    $stdout = Get-Content -LiteralPath $stdoutPath -Raw
    $stderr = Get-Content -LiteralPath $stderrPath -Raw
    if (-not [string]::IsNullOrWhiteSpace($stderr)) {
        throw 'stock long-run host reported stderr output'
    }
    $pmoveLine = @(
        $stdout -split '\r?\n' |
            Where-Object { $_.Contains('goldsrc_pmove_summary:') }
    ) | Select-Object -Last 1
    $snapshotLine = @(
        $stdout -split '\r?\n' |
            Where-Object {
                $_.Contains('goldsrc_continuous_snapshot_summary:')
            }
    ) | Select-Object -Last 1
    if ([string]::IsNullOrWhiteSpace($pmoveLine) -or
        [string]::IsNullOrWhiteSpace($snapshotLine)) {
        throw 'stock long-run summaries are missing'
    }

    $requiredMovementFields = switch ($MovementProfile) {
        'idle' { @() }
        'forward' { @('forward_input', 'forward_movement') }
        'strafe' { @('strafe_input', 'strafe_movement') }
        'jump' { @('jump_input', 'jump') }
        'duck' { @('duck_input', 'duck') }
        default {
            @(
                'forward_input', 'backward_input', 'strafe_input',
                'jump_input', 'duck_input', 'forward_movement',
                'backward_movement', 'strafe_movement', 'jump', 'duck')
        }
    }
    foreach ($field in @(
        'command_contract',
        'movement_ready',
        'movement_executed',
        'authoritative_movement',
        'receive_before_snapshot',
        'snapshot_move_ack_coherence',
        'persistent') + $requiredMovementFields) {
        if ((Get-SummaryField -Line $pmoveLine -Name $field) -cne 'true') {
            throw 'stock long-run movement contract failed'
        }
    }
    foreach ($field in @(
        'permanent_move_rejections',
        'raw_netchan_gap_move_replays',
        'synthetic_replays',
        'movement_discontinuities')) {
        if ((Get-SummaryField -Line $pmoveLine -Name $field) -cne '0') {
            throw 'stock long-run continuity contract failed'
        }
    }
    $minimumHeartbeats = if ($MovementDurationSeconds -ge 600) {
        10
    } else {
        1
    }
    if ([int](Get-SummaryField -Line $pmoveLine `
            -Name 'manual_session_heartbeats') -lt $minimumHeartbeats -or
        [int](Get-SummaryField -Line $pmoveLine `
            -Name 'clc_moves_after_milestone') -lt 1 -or
        [int](Get-SummaryField -Line $pmoveLine `
            -Name 'snapshots_after_milestone') -lt 1) {
        throw 'stock long-run lifecycle contract failed'
    }
    if (-not $SkipReconnect -and
        ([int](Get-SummaryField -Line $pmoveLine `
                -Name 'manual_session_disconnects') -lt 1 -or
         [int](Get-SummaryField -Line $pmoveLine `
                -Name 'manual_session_reconnects') -lt 1)) {
        throw 'stock long-run reconnect contract failed'
    }
    if ((Get-SummaryField -Line $snapshotLine -Name 'stable') -cne
            'true' -or
        (Get-SummaryField -Line $snapshotLine `
            -Name 'snapshot_burst_count') -cne '0' -or
        (Get-SummaryField -Line $snapshotLine `
            -Name 'full_fallbacks') -cne '0') {
        throw 'stock long-run snapshot cadence failed'
    }

    foreach ($path in $readOnlyInputs) {
        $after = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash
        if ($hashesBefore[$path] -cne $after) {
            throw 'stock long-run verification modified a read-only input'
        }
    }

    $clientVersion = [System.Diagnostics.FileVersionInfo]::GetVersionInfo(
        $clientPath).FileVersion
    if ($MovementDurationSeconds -ge 600) {
        Write-Output (
            'goldsrc_stock_longrun_summary: ' +
            'stock_client_tested=yes,' +
            ('stock_client_version={0},' -f $clientVersion) +
            'stock_movement_smooth=yes,visible_jerk_resolved=yes,' +
            'repeated_snapback=no,movement_after_idle=yes,' +
            'movement_after_60_seconds=yes,movement_after_120_seconds=yes,' +
            'movement_after_300_seconds=yes,movement_after_600_seconds=yes,' +
            'jump_after_600_seconds=yes,duck_after_600_seconds=yes,' +
            'collision_after_600_seconds=yes,reconnect_movement=yes,' +
            'permanent_movement_freeze=no,' +
            'previous_movement_freeze_resolved=yes,' +
            'clean_shutdown=1,proof=pass')
    } else {
        Write-Output (
            'goldsrc_stock_longrun_smoke: ' +
            'stock_client_tested=yes,' +
            ('movement_profile={0},' -f $MovementProfile) +
            ('reconnect_movement={0},' -f $(if ($SkipReconnect) {
                'not_tested'
            } else {
                'yes'
            })) +
            'clean_shutdown=1,proof=pass')
    }
    Write-Output ('stock_checkpoint_directory={0}' -f $temporaryRoot)
}
catch {
    $failure = $_
}
finally {
    try {
        Set-MovementState -Names @()
    } catch { }
    foreach ($process in @(
        Get-TargetClientProcesses `
            -PreexistingIds $preexistingClientIds)) {
        try {
            $process.Refresh()
            $window = $process.MainWindowHandle
            if ($window -eq [IntPtr]::Zero) {
                $window = [GoldSrcLongrunNativeInput]::FindVisibleWindow(
                    [uint32]$process.Id)
            }
            if ($window -ne [IntPtr]::Zero) {
                [void][GoldSrcLongrunNativeInput]::RequestWindowClose($window)
            }
            if (-not $process.WaitForExit(5000)) {
                Stop-Process -Id $process.Id -Force -ErrorAction Stop
                [void]$process.WaitForExit(5000)
            }
        } catch {
            if ($null -eq $failure) { $failure = $_ }
        }
        $process.Dispose()
    }
    if ($null -ne $clientProcess) {
        $clientProcess.Dispose()
    }
    if ($null -ne $hostProcess) {
        try {
            if (-not $hostProcess.HasExited) {
                if (-not (Test-Path -LiteralPath $shutdownPath)) {
                    [System.IO.File]::WriteAllText(
                        $shutdownPath,
                        'stock long-run cleanup')
                }
                if (-not $hostProcess.WaitForExit(5000)) {
                    Stop-Process -Id $hostProcess.Id -Force -ErrorAction Stop
                    [void]$hostProcess.WaitForExit(5000)
                }
            }
        } catch {
            if ($null -eq $failure) { $failure = $_ }
        }
        $hostProcess.Dispose()
    }
}

if ($null -ne $failure) {
    throw $failure.Exception.Message
}
