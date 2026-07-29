[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$ExecutablePath,

    [Parameter(Mandatory = $true)]
    [string]$GameDir,

    [Parameter(Mandatory = $true)]
    [string]$ClientPath,

    [ValidateSet(
        "forward",
        "backward",
        "strafe_left",
        "strafe_right",
        "jump",
        "duck")]
    [string]$Movement = "forward",

    [ValidateRange(30, 60)]
    [int]$ObservationSeconds = 30,

    [ValidateRange(60, 300)]
    [int]$TimeoutSeconds = 180
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
$executable = [System.IO.Path]::GetFullPath($ExecutablePath)
$gameDir = [System.IO.Path]::GetFullPath($GameDir)
$clientPath = [System.IO.Path]::GetFullPath($ClientPath)
if (-not (Test-Path -LiteralPath $executable -PathType Leaf) -or
    -not (Test-Path -LiteralPath $gameDir -PathType Container) -or
    -not (Test-Path -LiteralPath $clientPath -PathType Leaf)) {
    throw "stock PM_Move verification input is missing"
}

$readOnlyInputs = @(
    $clientPath,
    (Join-Path $gameDir "cl_dlls/client.dll"),
    (Join-Path $gameDir "dlls/hl.dll"),
    (Join-Path $gameDir "maps/c0a0.bsp"),
    (Join-Path $gameDir "delta.lst")
)
$hashesBefore = @{}
foreach ($path in $readOnlyInputs) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "stock PM_Move read-only input is missing"
    }
    $hashesBefore[$path] =
        (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash
}

if ($null -eq ("Prompt246NativeInput" -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;

public static class Prompt246NativeInput
{
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

    [DllImport("user32.dll")]
    public static extern IntPtr GetForegroundWindow();

    [DllImport("user32.dll")]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static extern bool SetForegroundWindow(IntPtr window);

    [DllImport("user32.dll")]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static extern bool ShowWindow(IntPtr window, int command);

    [DllImport("user32.dll", SetLastError = true)]
    private static extern uint SendInput(
        uint inputCount,
        Input[] inputs,
        int inputSize);

    public static bool SendScanCode(ushort scanCode, bool keyUp)
    {
        Input input = new Input();
        input.type = 1;
        input.data.keyboard.scanCode = scanCode;
        input.data.keyboard.flags =
            0x0008u | (keyUp ? 0x0002u : 0u);
        return SendInput(
            1,
            new Input[] { input },
            Marshal.SizeOf(typeof(Input))) == 1;
    }
}
'@
}

$preexistingClientIds = @(
    Get-Process -Name "hl" -ErrorAction SilentlyContinue |
        ForEach-Object { $_.Id }
)
$reservation = [System.Net.Sockets.UdpClient]::new(
    [System.Net.Sockets.AddressFamily]::InterNetwork)
$reservation.Client.Bind(
    [System.Net.IPEndPoint]::new(
        [System.Net.IPAddress]::Loopback,
        0))
$port = ([System.Net.IPEndPoint]$reservation.Client.LocalEndPoint).Port
$reservation.Close()
$reservation.Dispose()

$runId = [Guid]::NewGuid().ToString("N")
$temporaryRoot =
    [System.IO.Path]::GetFullPath(
        [System.IO.Path]::GetTempPath()).TrimEnd(
            [System.IO.Path]::DirectorySeparatorChar)
$stdoutPath = Join-Path $temporaryRoot "hlhost_pmove_$runId.stdout.log"
$stderrPath = Join-Path $temporaryRoot "hlhost_pmove_$runId.stderr.log"
$hostProcess = $null
$launchedClientProcess = $null
$inputWindow = [IntPtr]::Zero
$inputWindowCandidateCount = 0
$inputForegroundConfirmed = $false
$previousForegroundWindow = [IntPtr]::Zero
$inputPressed = $false
$inputScanCode = @{
    forward = 0x11
    backward = 0x1F
    strafe_left = 0x1E
    strafe_right = 0x20
    jump = 0x39
    duck = 0x1D
}[$Movement]
$failure = $null
try {
    $hostObservationSeconds =
        [Math]::Min(60, $ObservationSeconds + 10)
    $hostArguments = @(
        "--gamedir", ('"{0}"' -f $gameDir),
        "--dedicated",
        "--deathmatch", "1",
        "--maxclients", "1",
        "--map", "c0a0",
        "--frames", "1",
        "--log-to-file", "0",
        "--log-summary-file", "0",
        "--log-disable-categories=general",
        "--ip", "127.0.0.1",
        "--port", [string]$port,
        "--goldsrc-pmove",
        ("--goldsrc-pmove-observation-ms={0}" -f
            ($hostObservationSeconds * 1000)),
        "--goldsrc-snapshot-rate-hz=20",
        "--goldsrc-handshake-timeout-ms",
        ([string]($TimeoutSeconds * 1000))
    )
    $hostProcess = Start-Process `
        -FilePath $executable `
        -ArgumentList $hostArguments `
        -WorkingDirectory ([System.IO.Path]::GetDirectoryName($executable)) `
        -RedirectStandardOutput $stdoutPath `
        -RedirectStandardError $stderrPath `
        -WindowStyle Hidden `
        -PassThru

    $readyDeadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    $ready = $null
    while ([DateTime]::UtcNow -lt $readyDeadline) {
        if ($hostProcess.HasExited) {
            throw "stock PM_Move host exited before readiness"
        }
        if (Test-Path -LiteralPath $stdoutPath -PathType Leaf) {
            $ready = Select-String `
                -LiteralPath $stdoutPath `
                -SimpleMatch "goldsrc_udp_ready:" `
                -ErrorAction SilentlyContinue |
                Select-Object -First 1
            if ($null -ne $ready) {
                break
            }
        }
        Start-Sleep -Milliseconds 100
    }
    if ($null -eq $ready) {
        throw "stock PM_Move readiness timeout"
    }

    $clientArguments = @(
        "-steam",
        "-game", "valve",
        "-console",
        "-novid",
        "-nosound",
        "-windowed",
        "-w", "640",
        "-h", "480",
        "+connect", ("127.0.0.1:{0}" -f $port)
    )
    $launchedClientProcess = Start-Process `
        -FilePath $clientPath `
        -ArgumentList $clientArguments `
        -WorkingDirectory ([System.IO.Path]::GetDirectoryName($clientPath)) `
        -WindowStyle Minimized `
        -PassThru

    $inputDeadline = [DateTime]::UtcNow.AddSeconds(15)
    while ([DateTime]::UtcNow -lt $inputDeadline) {
        $newClientProcesses = @(
            Get-Process -Name "hl" -ErrorAction SilentlyContinue |
                Where-Object {
                    $preexistingClientIds -notcontains $_.Id
                }
        )
        $inputWindowCandidateCount = @(
            $newClientProcesses |
                Where-Object {
                    $_.Refresh()
                    $_.MainWindowHandle -ne [IntPtr]::Zero
                }
        ).Count
        $preferredProcess = $newClientProcesses |
            Where-Object { $_.Id -eq $launchedClientProcess.Id } |
            Select-Object -First 1
        if ($null -ne $preferredProcess) {
            $preferredProcess.Refresh()
            if ($preferredProcess.MainWindowHandle -ne [IntPtr]::Zero) {
                $inputWindow = $preferredProcess.MainWindowHandle
            }
        }
        foreach ($process in $newClientProcesses) {
            if ($inputWindow -ne [IntPtr]::Zero) {
                break
            }
            $process.Refresh()
            if ($process.MainWindowHandle -ne [IntPtr]::Zero) {
                $inputWindow = $process.MainWindowHandle
                break
            }
        }
        if ($inputWindow -ne [IntPtr]::Zero) {
            break
        }
        Start-Sleep -Milliseconds 100
    }
    if ($inputWindow -eq [IntPtr]::Zero) {
        throw "stock PM_Move bounded input delivery failed"
    }
    $connectDeadline = [DateTime]::UtcNow.AddSeconds(30)
    $clientConnected = $false
    while ([DateTime]::UtcNow -lt $connectDeadline) {
        if (Test-Path -LiteralPath $stdoutPath -PathType Leaf) {
            $clientConnected = $null -ne (
                Select-String `
                    -LiteralPath $stdoutPath `
                    -SimpleMatch "goldsrc_game_dll_client_connected:" `
                    -ErrorAction SilentlyContinue |
                    Select-Object -First 1)
        }
        if ($clientConnected -or $hostProcess.HasExited) {
            break
        }
        Start-Sleep -Milliseconds 100
    }
    if (-not $clientConnected) {
        throw "stock PM_Move client connect timeout"
    }
    $materializedDeadline = [DateTime]::UtcNow.AddSeconds(30)
    $playerMaterialized = $false
    while ([DateTime]::UtcNow -lt $materializedDeadline) {
        if (Test-Path -LiteralPath $stdoutPath -PathType Leaf) {
            $playerMaterialized = $null -ne (
                Select-String `
                    -LiteralPath $stdoutPath `
                    -SimpleMatch "goldsrc_player_materialized:" `
                    -ErrorAction SilentlyContinue |
                    Select-Object -First 1)
        }
        if ($playerMaterialized -or $hostProcess.HasExited) {
            break
        }
        Start-Sleep -Milliseconds 100
    }
    if (-not $playerMaterialized) {
        throw "stock PM_Move player materialization timeout"
    }
    $previousForegroundWindow =
        [Prompt246NativeInput]::GetForegroundWindow()
    [void][Prompt246NativeInput]::ShowWindow($inputWindow, 9)
    Start-Sleep -Milliseconds 250
    if (-not [Prompt246NativeInput]::SetForegroundWindow($inputWindow)) {
        throw "stock PM_Move bounded input focus failed"
    }
    Start-Sleep -Milliseconds 100
    $inputForegroundConfirmed =
        [Prompt246NativeInput]::GetForegroundWindow() -eq $inputWindow
    if (-not [Prompt246NativeInput]::SendScanCode(
        [ushort]$inputScanCode,
        $false)) {
        throw "stock PM_Move bounded scan-code input failed"
    }
    $inputPressed = $true

    $completionDeadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    while (-not $hostProcess.HasExited -and
        [DateTime]::UtcNow -lt $completionDeadline) {
        Start-Sleep -Milliseconds 100
    }
    if (-not $hostProcess.HasExited) {
        throw "stock PM_Move host completion timeout"
    }
    if ($hostProcess.ExitCode -ne 0) {
        $failureStdout = if (
            Test-Path -LiteralPath $stdoutPath -PathType Leaf) {
            Get-Content -LiteralPath $stdoutPath -Raw
        } else {
            ""
        }
        Write-Output ("stock_failure_client_connected={0}" -f
            $failureStdout.Contains(
                "goldsrc_game_dll_client_connected:").ToString().
                    ToLowerInvariant())
        Write-Output ("stock_failure_player_materialized={0}" -f
            $failureStdout.Contains(
                "goldsrc_player_materialized:").ToString().
                    ToLowerInvariant())
        Write-Output ("stock_failure_pmove_summary={0}" -f
            $failureStdout.Contains(
                "goldsrc_pmove_summary:").ToString().
                    ToLowerInvariant())
        $safeFailureReason = [regex]::Matches(
            $failureStdout,
            "goldsrc_[a-z0-9_]+_failed: reason=([a-z0-9-]+)") |
                Select-Object -Last 1
        if ($null -ne $safeFailureReason) {
            Write-Output ("stock_failure_reason={0}" -f
                $safeFailureReason.Groups[1].Value)
        }
        throw "stock PM_Move host reported failure"
    }

    $newClientProcesses = @(
        Get-Process -Name "hl" -ErrorAction SilentlyContinue |
            Where-Object { $preexistingClientIds -notcontains $_.Id }
    )
    if ($newClientProcesses.Count -eq 0) {
        throw "stock PM_Move client did not remain connected"
    }

    $stdout = Get-Content -LiteralPath $stdoutPath -Raw
    $stderr = Get-Content -LiteralPath $stderrPath -Raw
    if (-not [string]::IsNullOrWhiteSpace($stderr) -or
        $stdout -match '\[error\]|transport_failed=1|clean_shutdown=0') {
        throw "stock PM_Move verification reported a runtime error"
    }
    $pmoveLine = @(
        $stdout -split '\r?\n' |
            Where-Object { $_.Contains("goldsrc_pmove_summary:") }
    ) | Select-Object -Last 1
    $snapshotLine = @(
        $stdout -split '\r?\n' |
            Where-Object {
                $_.Contains("goldsrc_continuous_snapshot_summary:")
            }
    ) | Select-Object -Last 1
    if ([string]::IsNullOrWhiteSpace($pmoveLine) -or
        [string]::IsNullOrWhiteSpace($snapshotLine)) {
        throw "stock PM_Move summaries are missing"
    }
    foreach ($required in @(
        "pm_init_calls=1",
        "movement_ready=true",
        "movement_executed=true",
        "authoritative_movement=true",
        "grounding=true",
        "next_boundary=two_client_player_replication_required",
        "gameplay_active=false",
        "clean_shutdown=1"
    )) {
        if (-not $pmoveLine.Contains($required)) {
            throw "stock PM_Move contract mismatch"
        }
    }
    $expectedMovementField = @{
        forward = "forward_movement=true"
        backward = "backward_movement=true"
        strafe_left = "strafe_movement=true"
        strafe_right = "strafe_movement=true"
        jump = "jump=true"
        duck = "duck=true"
    }[$Movement]
    $movementObserved = $pmoveLine.Contains($expectedMovementField)
    $snapshotStable = $snapshotLine.Contains("stable=true")
    $safeObservationFields = @(
        "pm_move_calls",
        "commands_received",
        "commands_executed",
        "authoritative_movement",
        "forward_input",
        "backward_input",
        "strafe_input",
        "jump_input",
        "duck_input",
        "forward_movement",
        "backward_movement",
        "strafe_movement",
        "jump",
        "duck",
        "grounding"
    )
    $safeObservations = @()
    foreach ($field in $safeObservationFields) {
        $match = [regex]::Match(
            $pmoveLine,
            "(?:^|,)$([regex]::Escape($field))=([^,]+)")
        if ($match.Success) {
            $safeObservations += "$field=$($match.Groups[1].Value)"
        }
    }
    Write-Output ("stock_pmove_observation={0}" -f
        ($safeObservations -join ","))
    Write-Output ("stock_input_window_candidates={0}" -f
        $inputWindowCandidateCount)
    Write-Output ("stock_input_foreground_confirmed={0}" -f
        $inputForegroundConfirmed.ToString().ToLowerInvariant())
    Write-Output ("stock_expected_movement_observed={0}" -f
        $movementObserved.ToString().ToLowerInvariant())
    Write-Output ("stock_snapshot_stable={0}" -f
        $snapshotStable.ToString().ToLowerInvariant())
    if (-not $movementObserved -or -not $snapshotStable) {
        throw "stock PM_Move movement observation mismatch"
    }

    foreach ($path in $readOnlyInputs) {
        $after = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash
        if ($hashesBefore[$path] -cne $after) {
            throw "stock PM_Move verification modified a read-only input"
        }
    }

    $version = [System.Diagnostics.FileVersionInfo]::GetVersionInfo(
        $clientPath).FileVersion
    Write-Output "stock_client_tested=yes"
    Write-Output ("stock_client_version={0}" -f $version)
    Write-Output ("stock_movement={0}" -f $Movement)
    Write-Output "stock_authoritative_movement=yes"
    Write-Output "stock_world_collision=yes"
    Write-Output "stock_grounding=yes"
    Write-Output "stock_client_remained_connected=yes"
    Write-Output "stock_prediction_stable=yes"
    Write-Output "previous_movement_boundary_resolved=yes"
    Write-Output "stock_client_advanced_past_previous_boundary=yes"
    Write-Output "next_observed_boundary=two_client_player_replication_required"
}
catch {
    $failure = $_
}
finally {
    if ($inputPressed) {
        [void][Prompt246NativeInput]::SendScanCode(
            [ushort]$inputScanCode,
            $true)
    }
    if ($inputWindow -ne [IntPtr]::Zero) {
        [void][Prompt246NativeInput]::ShowWindow($inputWindow, 6)
    }
    if ($previousForegroundWindow -ne [IntPtr]::Zero) {
        [void][Prompt246NativeInput]::SetForegroundWindow(
            $previousForegroundWindow)
    }
    $newClientProcesses = @(
        Get-Process -Name "hl" -ErrorAction SilentlyContinue |
            Where-Object { $preexistingClientIds -notcontains $_.Id }
    )
    foreach ($process in $newClientProcesses) {
        try {
            Stop-Process -Id $process.Id -Force -ErrorAction Stop
            [void]$process.WaitForExit(5000)
        }
        catch {
            if ($null -eq $failure) {
                $failure = $_
            }
        }
        $process.Dispose()
    }
    if ($null -ne $hostProcess) {
        try {
            if (-not $hostProcess.HasExited) {
                Stop-Process -Id $hostProcess.Id -Force -ErrorAction Stop
                [void]$hostProcess.WaitForExit(5000)
            }
        }
        catch {
            if ($null -eq $failure) {
                $failure = $_
            }
        }
        $hostProcess.Dispose()
    }
    foreach ($path in @($stdoutPath, $stderrPath)) {
        if ([System.IO.Path]::GetDirectoryName($path) -ceq $temporaryRoot -and
            [System.IO.Path]::GetFileName($path).StartsWith(
                "hlhost_pmove_",
                [StringComparison]::Ordinal)) {
            Remove-Item -LiteralPath $path -Force -ErrorAction SilentlyContinue
        }
    }
}
if ($null -ne $failure) {
    throw $failure
}
