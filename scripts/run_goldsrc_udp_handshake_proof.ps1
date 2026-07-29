[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$ExecutablePath,

    [Parameter(Mandatory = $true)]
    [string]$GameDir,

    [string]$BindAddress = "127.0.0.1",

    [ValidateRange(0, 65535)]
    [int]$Port = 0,

    [ValidateRange(1, 300)]
    [int]$TimeoutSeconds = 20,

    [switch]$ExerciseDisconnectedSlotReuse,

    [switch]$SkipServerOutput
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
if (Test-Path Variable:PSNativeCommandUseErrorActionPreference) {
    $PSNativeCommandUseErrorActionPreference = $false
}

function ConvertTo-WindowsCommandLineArgument {
    param([AllowEmptyString()][string]$Value)

    if ($Value.Length -gt 0 -and $Value -notmatch '[\s"]') {
        return $Value
    }

    $quote = [char]34
    $backslash = [char]92
    $builder = New-Object System.Text.StringBuilder
    [void]$builder.Append($quote)
    $pendingBackslashes = 0

    foreach ($character in $Value.ToCharArray()) {
        if ($character -eq $backslash) {
            $pendingBackslashes++
            continue
        }

        if ($character -eq $quote) {
            for ($index = 0; $index -lt (($pendingBackslashes * 2) + 1); $index++) {
                [void]$builder.Append($backslash)
            }
            [void]$builder.Append($quote)
            $pendingBackslashes = 0
            continue
        }

        for ($index = 0; $index -lt $pendingBackslashes; $index++) {
            [void]$builder.Append($backslash)
        }
        $pendingBackslashes = 0
        [void]$builder.Append($character)
    }

    # Backslashes immediately before the closing quote must be doubled.
    for ($index = 0; $index -lt ($pendingBackslashes * 2); $index++) {
        [void]$builder.Append($backslash)
    }
    [void]$builder.Append($quote)
    return $builder.ToString()
}

function Get-SharedFileText {
    param([string]$Path)

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        return ""
    }

    $stream = $null
    $reader = $null
    try {
        $stream = [System.IO.File]::Open(
            $Path,
            [System.IO.FileMode]::Open,
            [System.IO.FileAccess]::Read,
            [System.IO.FileShare]::ReadWrite
        )
        $reader = New-Object System.IO.StreamReader($stream, [System.Text.Encoding]::UTF8, $true)
        return $reader.ReadToEnd()
    }
    catch [System.IO.IOException] {
        return ""
    }
    finally {
        if ($null -ne $reader) {
            $reader.Dispose()
        } elseif ($null -ne $stream) {
            $stream.Dispose()
        }
    }
}

function Get-RemainingMilliseconds {
    param([DateTime]$Deadline)

    $remaining = [Math]::Ceiling(($Deadline - [DateTime]::UtcNow).TotalMilliseconds)
    if ($remaining -le 0) {
        return 0
    }
    if ($remaining -gt [int]::MaxValue) {
        return [int]::MaxValue
    }
    return [int]$remaining
}

function Test-StableField {
    param(
        [string]$Line,
        [string]$Name,
        [string]$ExpectedValue
    )

    $pattern = '(?:^|[,\s])' +
        [regex]::Escape($Name) + '=' +
        [regex]::Escape($ExpectedValue) +
        '(?=$|[,\s])'
    return [regex]::IsMatch(
        $Line,
        $pattern,
        [System.Text.RegularExpressions.RegexOptions]::CultureInvariant
    )
}

function Wait-ForServerReadiness {
    param(
        [System.Diagnostics.Process]$Process,
        $OutputCapture,
        [string]$StdoutPath,
        [DateTime]$Deadline,
        [string]$ExpectedAddress,
        [int]$ExpectedPort
    )

    while ($true) {
        [void](Update-ProcessOutputCapture -State $OutputCapture)
        $stdout = Get-SharedFileText -Path $StdoutPath
        $readyLines = @(
            $stdout -split "`r?`n" |
                Where-Object { $_.IndexOf("goldsrc_udp_ready:", [StringComparison]::Ordinal) -ge 0 }
        )
        foreach ($line in $readyLines) {
            if (-not (Test-StableField -Line $line -Name "address" -ExpectedValue $ExpectedAddress)) {
                throw ("goldsrc_udp_ready address mismatch: {0}" -f $line.Trim())
            }
            if (-not (Test-StableField -Line $line -Name "port" -ExpectedValue ([string]$ExpectedPort))) {
                throw ("goldsrc_udp_ready port mismatch: {0}" -f $line.Trim())
            }
            return $line
        }

        if ($Process.HasExited) {
            # Re-read once after process teardown so a final buffered line is visible.
            Complete-ProcessOutputCapture -State $OutputCapture
            $stdout = Get-SharedFileText -Path $StdoutPath
            $readyLines = @(
                $stdout -split "`r?`n" |
                    Where-Object { $_.IndexOf("goldsrc_udp_ready:", [StringComparison]::Ordinal) -ge 0 }
            )
            if ($readyLines.Count -eq 0) {
                throw ("hlhost exited with code {0} before goldsrc_udp_ready" -f $Process.ExitCode)
            }
            continue
        }

        $remainingMilliseconds = Get-RemainingMilliseconds -Deadline $Deadline
        if ($remainingMilliseconds -le 0) {
            throw "timeout waiting for goldsrc_udp_ready"
        }

        # This is a bounded readiness poll, not an arbitrary startup sleep.
        $pollMilliseconds = [Math]::Min(50, $remainingMilliseconds)
        [void]$Process.WaitForExit($pollMilliseconds)
    }
}

function New-ConnectionlessDatagram {
    param(
        [string]$Text,
        [byte[]]$Tail = @()
    )

    $textBytes = [System.Text.Encoding]::ASCII.GetBytes($Text)
    $packet = New-Object byte[] (4 + $textBytes.Length + $Tail.Length)
    for ($index = 0; $index -lt 4; $index++) {
        $packet[$index] = 0xFF
    }
    [Array]::Copy($textBytes, 0, $packet, 4, $textBytes.Length)
    if ($Tail.Length -gt 0) {
        [Array]::Copy($Tail, 0, $packet, 4 + $textBytes.Length, $Tail.Length)
    }
    return ,$packet
}

function Assert-ConnectionlessPrefix {
    param(
        [byte[]]$Packet,
        [string]$Description
    )

    if ($Packet.Length -lt 4) {
        throw ("{0} is truncated" -f $Description)
    }
    for ($index = 0; $index -lt 4; $index++) {
        if ($Packet[$index] -ne 0xFF) {
            throw ("{0} has a malformed connectionless prefix" -f $Description)
        }
    }
}

function ConvertTo-HexPreview {
    param([byte[]]$Bytes)

    $previewLength = [Math]::Min($Bytes.Length, 64)
    $parts = New-Object System.Collections.Generic.List[string]
    for ($index = 0; $index -lt $previewLength; $index++) {
        $parts.Add($Bytes[$index].ToString("X2", [Globalization.CultureInfo]::InvariantCulture))
    }
    $suffix = if ($Bytes.Length -gt $previewLength) { "..." } else { "" }
    return (($parts -join " ") + $suffix)
}

function Assert-ExactBytes {
    param(
        [byte[]]$Actual,
        [byte[]]$Expected,
        [string]$Description
    )

    if ($Actual.Length -ne $Expected.Length) {
        throw ("{0} length mismatch: expected {1}, received {2}; bytes={3}" -f
            $Description,
            $Expected.Length,
            $Actual.Length,
            (ConvertTo-HexPreview -Bytes $Actual))
    }

    for ($index = 0; $index -lt $Expected.Length; $index++) {
        if ($Actual[$index] -ne $Expected[$index]) {
            throw ("{0} byte mismatch at offset {1}; bytes={2}" -f
                $Description,
                $index,
                (ConvertTo-HexPreview -Bytes $Actual))
        }
    }
}

function Receive-UdpDatagram {
    param(
        [System.Net.Sockets.UdpClient]$Client,
        [System.Diagnostics.Process]$ServerProcess,
        $OutputCapture,
        [DateTime]$Deadline,
        [string]$Description
    )

    while ($true) {
        [void](Update-ProcessOutputCapture -State $OutputCapture)
        $remainingMilliseconds = Get-RemainingMilliseconds -Deadline $Deadline
        if ($remainingMilliseconds -le 0) {
            throw ("timeout waiting for {0}" -f $Description)
        }

        $Client.Client.ReceiveTimeout = [Math]::Max(1, [Math]::Min(50, $remainingMilliseconds))
        $remoteEndpoint = New-Object System.Net.IPEndPoint([System.Net.IPAddress]::Any, 0)
        try {
            [byte[]]$received = $Client.Receive([ref]$remoteEndpoint)
            if ($received.Length -gt 2048) {
                throw ("{0} exceeds the 2048-byte proof limit" -f $Description)
            }
            return ,$received
        }
        catch [System.Net.Sockets.SocketException] {
            $socketError = $_.Exception.SocketErrorCode
            if ($socketError -eq [System.Net.Sockets.SocketError]::TimedOut -or
                $socketError -eq [System.Net.Sockets.SocketError]::WouldBlock) {
                # Try the socket before testing process exit: a valid final reply can
                # already be queued when the one-shot server exits successfully.
                if ($ServerProcess.HasExited) {
                    throw ("hlhost exited with code {0} while waiting for {1}" -f
                        $ServerProcess.ExitCode,
                        $Description)
                }
                continue
            }
            throw
        }
    }
}

function Wait-ForCleanServerExit {
    param(
        [System.Diagnostics.Process]$Process,
        $OutputCapture,
        [DateTime]$Deadline
    )

    while (-not $Process.HasExited) {
        [void](Update-ProcessOutputCapture -State $OutputCapture)
        $remainingMilliseconds = Get-RemainingMilliseconds -Deadline $Deadline
        if ($remainingMilliseconds -le 0) {
            throw "timeout waiting for hlhost to exit after accepting the connection"
        }
        [void]$Process.WaitForExit([Math]::Min(100, $remainingMilliseconds))
    }
    # Refresh the process object before reading ExitCode on Windows PowerShell 5.
    $Process.Refresh()
    Complete-ProcessOutputCapture -State $OutputCapture
}

function New-ProcessOutputCapture {
    param(
        [System.Diagnostics.Process]$Process,
        [string]$StdoutPath,
        [string]$StderrPath
    )

    $encoding = New-Object System.Text.UTF8Encoding($false)
    [System.IO.File]::WriteAllText($StdoutPath, "", $encoding)
    [System.IO.File]::WriteAllText($StderrPath, "", $encoding)

    return [pscustomobject]@{
        Process = $Process
        StdoutPath = $StdoutPath
        StderrPath = $StderrPath
        Encoding = $encoding
        StdoutTask = $Process.StandardOutput.ReadLineAsync()
        StderrTask = $Process.StandardError.ReadLineAsync()
        StdoutClosed = $false
        StderrClosed = $false
    }
}

function Update-ProcessOutputCapture {
    param($State)

    $drainedLines = 0
    while (-not $State.StdoutClosed -and
        $null -ne $State.StdoutTask -and
        $State.StdoutTask.IsCompleted -and
        $drainedLines -lt 256) {
        try {
            $line = $State.StdoutTask.Result
        }
        catch {
            throw ("failed reading hlhost stdout: {0}" -f $_.Exception.GetBaseException().Message)
        }
        if ($null -eq $line) {
            $State.StdoutClosed = $true
            $State.StdoutTask = $null
        } else {
            [System.IO.File]::AppendAllText(
                $State.StdoutPath,
                $line + [Environment]::NewLine,
                $State.Encoding
            )
            $State.StdoutTask = $State.Process.StandardOutput.ReadLineAsync()
            $drainedLines++
        }
    }

    $stderrLines = 0
    while (-not $State.StderrClosed -and
        $null -ne $State.StderrTask -and
        $State.StderrTask.IsCompleted -and
        $stderrLines -lt 256) {
        try {
            $line = $State.StderrTask.Result
        }
        catch {
            throw ("failed reading hlhost stderr: {0}" -f $_.Exception.GetBaseException().Message)
        }
        if ($null -eq $line) {
            $State.StderrClosed = $true
            $State.StderrTask = $null
        } else {
            [System.IO.File]::AppendAllText(
                $State.StderrPath,
                $line + [Environment]::NewLine,
                $State.Encoding
            )
            $State.StderrTask = $State.Process.StandardError.ReadLineAsync()
            $stderrLines++
        }
    }

    return ($drainedLines + $stderrLines)
}

function Complete-ProcessOutputCapture {
    param($State)

    $drainDeadline = [DateTime]::UtcNow.AddSeconds(2)
    while (-not $State.StdoutClosed -or -not $State.StderrClosed) {
        [void](Update-ProcessOutputCapture -State $State)
        if ($State.StdoutClosed -and $State.StderrClosed) {
            return
        }
        if ([DateTime]::UtcNow -ge $drainDeadline) {
            throw "timeout draining redirected hlhost stdout/stderr"
        }

        $pendingTask = if (-not $State.StdoutClosed) {
            $State.StdoutTask
        } else {
            $State.StderrTask
        }
        try {
            [void]$pendingTask.Wait(25)
        }
        catch {
            # Update-ProcessOutputCapture reports the underlying stream failure.
        }
    }
}

function Test-ProcessIdAlive {
    param([int]$ProcessId)

    if ($ProcessId -le 0) {
        return $false
    }
    $processById = Get-Process -Id $ProcessId -ErrorAction SilentlyContinue
    if ($null -eq $processById) {
        return $false
    }
    $processById.Dispose()
    return $true
}

function Assert-SessionSummary {
    param(
        [string]$Stdout,
        [string]$ExpectedEndpoint,
        [int]$ExpectedChallenge,
        [int]$ExpectedChannelIdentifier,
        [int]$ExpectedSlot,
        [int]$ExpectedSessionCount
    )

    $summaryLines = @(
        $Stdout -split "`r?`n" |
            Where-Object { $_.IndexOf("goldsrc_udp_session:", [StringComparison]::Ordinal) -ge 0 }
    )
    if ($summaryLines.Count -eq 0) {
        throw "stdout is missing goldsrc_udp_session summary"
    }

    $expectedFields = [ordered]@{
        count = [string]$ExpectedSessionCount
        state = "connected"
        connected = "1"
        put_in_server = "0"
        spawned = "0"
        active = "0"
        slot = [string]$ExpectedSlot
        endpoint = $ExpectedEndpoint
        protocol = "48"
        qport = "0"
        channel_id = [string]$ExpectedChannelIdentifier
        challenge = [string]$ExpectedChallenge
        auth_protocol = "3"
        extensions = "0"
        steam_auth = "0"
        protocol_info = "present"
        user_info = "present"
    }

    foreach ($line in $summaryLines) {
        foreach ($field in $expectedFields.GetEnumerator()) {
            if (-not (Test-StableField -Line $line -Name $field.Key -ExpectedValue $field.Value)) {
                throw ("goldsrc_udp_session expected {0}={1}: {2}" -f
                    $field.Key,
                    $field.Value,
                    $line.Trim())
            }
        }
    }

    return ("count={0},state=connected,connected=1,put_in_server=0,spawned=0,active=0,protocol=48,metadata=present" -f
        $ExpectedSessionCount)
}

function Assert-FinalServerSummary {
    param(
        [string]$Stdout,
        [int]$ExpectedSessionCount
    )

    $summaryLines = @(
        $Stdout -split "`r?`n" |
            Where-Object { $_.IndexOf("goldsrc_udp_summary:", [StringComparison]::Ordinal) -ge 0 }
    )
    if ($summaryLines.Count -ne 1) {
        throw ("stdout expected exactly one goldsrc_udp_summary line, found {0}" -f
            $summaryLines.Count)
    }

    $line = $summaryLines[0]
    $expectedFields = [ordered]@{
        enabled = "1"
        ready = "1"
        clean_shutdown = "1"
        max_datagrams_per_frame = "8"
        datagrams = "2"
        challenges = "1"
        connects = "1"
        accepted = "1"
        rejected = "0"
        last_reject_reason = "<none>"
        session_count = [string]$ExpectedSessionCount
        state = "connected"
    }
    foreach ($field in $expectedFields.GetEnumerator()) {
        if (-not (Test-StableField -Line $line -Name $field.Key -ExpectedValue $field.Value)) {
            throw ("goldsrc_udp_summary expected {0}={1}: {2}" -f
                $field.Key,
                $field.Value,
                $line.Trim())
        }
    }

    $framesMatch = [regex]::Match(
        $line,
        '(?:^|[,s])frames=(?<frames>[1-9][0-9]*)(?=$|[,s])',
        [System.Text.RegularExpressions.RegexOptions]::CultureInvariant
    )
    if (-not $framesMatch.Success) {
        throw ("goldsrc_udp_summary expected a positive frames count: {0}" -f $line.Trim())
    }

    return ("frames={0},max_datagrams_per_frame=8,datagrams=2,challenges=1,connects=1,accepted=1,rejected=0,session_count={1},state=connected,clean_shutdown=1" -f
        $framesMatch.Groups["frames"].Value,
        $ExpectedSessionCount)
}

$repoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$resolvedExecutablePath = [System.IO.Path]::GetFullPath($ExecutablePath)
$resolvedGameDir = [System.IO.Path]::GetFullPath($GameDir)

if (-not (Test-Path -LiteralPath $resolvedExecutablePath -PathType Leaf)) {
    throw ("hlhost executable not found: {0}" -f $resolvedExecutablePath)
}
if (-not (Test-Path -LiteralPath $resolvedGameDir -PathType Container)) {
    throw ("game directory not found: {0}" -f $resolvedGameDir)
}

$parsedBindAddress = $null
if (-not [System.Net.IPAddress]::TryParse($BindAddress, [ref]$parsedBindAddress) -or
    $parsedBindAddress.AddressFamily -ne [System.Net.Sockets.AddressFamily]::InterNetwork) {
    throw ("BindAddress must be an IPv4 address: {0}" -f $BindAddress)
}
if (-not [System.Net.IPAddress]::IsLoopback($parsedBindAddress)) {
    throw ("BindAddress must be loopback for this external proof: {0}" -f $BindAddress)
}

$selectedPort = $Port
$portReservation = $null
if ($selectedPort -eq 0) {
    try {
        # The reservation is intentionally brief: hlhost receives the explicit port.
        $portReservation = New-Object System.Net.Sockets.UdpClient(0)
        $selectedPort = ([System.Net.IPEndPoint]$portReservation.Client.LocalEndPoint).Port
    }
    finally {
        if ($null -ne $portReservation) {
            $portReservation.Close()
            $portReservation.Dispose()
            $portReservation = $null
        }
    }
}

$handshakeTimeoutMilliseconds = [Math]::Min(
    60000,
    [Math]::Max(1000, ($TimeoutSeconds * 1000))
)
$expectedSlot = if ($ExerciseDisconnectedSlotReuse) { 2 } else { 1 }
$expectedSessionCount = if ($ExerciseDisconnectedSlotReuse) { 2 } else { 1 }
$maximumClients = if ($ExerciseDisconnectedSlotReuse) { 2 } else { 1 }
$arguments = @(
    "--gamedir", $resolvedGameDir,
    "--dedicated",
    "--deathmatch", "1",
    "--maxclients", ([string]$maximumClients),
    "--frames", "1",
    "--log-to-file", "0",
    "--log-summary-file", "0",
    "--log-disable-categories=general",
    "--ip", $BindAddress,
    "--port", ([string]$selectedPort),
    "--goldsrc-handshake",
    "--goldsrc-handshake-timeout-ms", ([string]$handshakeTimeoutMilliseconds)
)
if ($ExerciseDisconnectedSlotReuse) {
    $arguments += @("--synthetic-players", "2")
}
$argumentLine = (($arguments | ForEach-Object {
    ConvertTo-WindowsCommandLineArgument -Value ([string]$_)
}) -join " ")
$tempPrefix = "hlhost_goldsrc_udp_{0}" -f ([Guid]::NewGuid().ToString("N"))
$stdoutPath = Join-Path ([System.IO.Path]::GetTempPath()) ($tempPrefix + ".stdout.log")
$stderrPath = Join-Path ([System.IO.Path]::GetTempPath()) ($tempPrefix + ".stderr.log")
$deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)

$serverProcess = $null
$serverProcessId = 0
$processStarted = $false
$outputCapture = $null
$probeClient = $null
$capturedStdout = ""
$capturedStderr = ""
$failure = $null
$cleanupFailure = $null
$challengeValue = 0
$clientPort = 0
$sessionProof = ""
$serverProof = ""

try {
    $startInfo = New-Object System.Diagnostics.ProcessStartInfo
    $startInfo.FileName = $resolvedExecutablePath
    $startInfo.Arguments = $argumentLine
    $startInfo.WorkingDirectory = $repoRoot
    $startInfo.UseShellExecute = $false
    $startInfo.CreateNoWindow = $true
    $startInfo.WindowStyle = [System.Diagnostics.ProcessWindowStyle]::Hidden
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true

    $serverProcess = New-Object System.Diagnostics.Process
    $serverProcess.StartInfo = $startInfo
    if (-not $serverProcess.Start()) {
        throw "failed to start hlhost"
    }
    $processStarted = $true
    $serverProcessId = $serverProcess.Id
    if ($serverProcessId -le 0) {
        throw "hlhost started but its process ID could not be identified"
    }
    $outputCapture = New-ProcessOutputCapture `
        -Process $serverProcess `
        -StdoutPath $stdoutPath `
        -StderrPath $stderrPath

    [void](Wait-ForServerReadiness `
        -Process $serverProcess `
        -OutputCapture $outputCapture `
        -StdoutPath $stdoutPath `
        -Deadline $deadline `
        -ExpectedAddress $BindAddress `
        -ExpectedPort $selectedPort)

    $probeClient = New-Object System.Net.Sockets.UdpClient(
        [System.Net.Sockets.AddressFamily]::InterNetwork
    )
    $probeClient.Client.Bind((New-Object System.Net.IPEndPoint([System.Net.IPAddress]::Loopback, 0)))
    $clientPort = ([System.Net.IPEndPoint]$probeClient.Client.LocalEndPoint).Port
    $serverEndpoint = New-Object System.Net.IPEndPoint($parsedBindAddress, $selectedPort)
    $probeClient.Connect($serverEndpoint)

    [byte[]]$challengeRequest = New-ConnectionlessDatagram `
        -Text "getchallenge steam`n" `
        -Tail ([byte[]](0x00))
    $sentLength = $probeClient.Send($challengeRequest, $challengeRequest.Length)
    if ($sentLength -ne $challengeRequest.Length) {
        throw ("challenge request send was short: expected {0}, sent {1}" -f
            $challengeRequest.Length,
            $sentLength)
    }

    [byte[]]$challengeResponse = Receive-UdpDatagram `
        -Client $probeClient `
        -ServerProcess $serverProcess `
        -OutputCapture $outputCapture `
        -Deadline $deadline `
        -Description "challenge response"
    Assert-ConnectionlessPrefix -Packet $challengeResponse -Description "challenge response"

    $challengeBodyLength = $challengeResponse.Length - 4
    $challengeBody = [System.Text.Encoding]::ASCII.GetString(
        $challengeResponse,
        4,
        $challengeBodyLength
    )
    $challengeMatch = [regex]::Match(
        $challengeBody,
        '\AA00000000 (?<challenge>0|[1-9][0-9]*) 3 0 0\n\x00\z',
        [System.Text.RegularExpressions.RegexOptions]::CultureInvariant
    )
    if (-not $challengeMatch.Success) {
        throw ("malformed challenge response: bytes={0}" -f
            (ConvertTo-HexPreview -Bytes $challengeResponse))
    }

    $parsedChallenge = 0
    if (-not [int]::TryParse(
        $challengeMatch.Groups["challenge"].Value,
        [Globalization.NumberStyles]::None,
        [Globalization.CultureInfo]::InvariantCulture,
        [ref]$parsedChallenge
    )) {
        throw "challenge response value is outside signed int32 range"
    }
    $challengeValue = $parsedChallenge

    [byte[]]$expectedChallengeResponse = New-ConnectionlessDatagram `
        -Text ("A00000000 {0} 3 0 0`n" -f $challengeValue) `
        -Tail ([byte[]](0x00))
    Assert-ExactBytes `
        -Actual $challengeResponse `
        -Expected $expectedChallengeResponse `
        -Description "challenge response"

    $protocolInfo = '\prot\3\unique\-1\raw\steam\cdkey\00000000000000000000000000000000'
    $userInfo = '\name\udp_probe\model\gordon'
    $connectLine = ('connect 48 {0} "{1}" "{2}"' -f
        $challengeValue,
        $protocolInfo,
        $userInfo) + "`n"
    [byte[]]$authTail = @(0x00, 0x01, 0x7F, 0x80, 0xFF)
    [byte[]]$connectRequest = New-ConnectionlessDatagram -Text $connectLine -Tail $authTail
    $sentLength = $probeClient.Send($connectRequest, $connectRequest.Length)
    if ($sentLength -ne $connectRequest.Length) {
        throw ("connect request send was short: expected {0}, sent {1}" -f
            $connectRequest.Length,
            $sentLength)
    }

    [byte[]]$acceptResponse = Receive-UdpDatagram `
        -Client $probeClient `
        -ServerProcess $serverProcess `
        -OutputCapture $outputCapture `
        -Deadline $deadline `
        -Description "connect accept response"
    [byte[]]$expectedAcceptResponse = New-ConnectionlessDatagram `
        -Text ('B {0} "127.0.0.1:{1}" 0 5971' -f $expectedSlot, $clientPort) `
        -Tail ([byte[]](0x00))
    Assert-ExactBytes `
        -Actual $acceptResponse `
        -Expected $expectedAcceptResponse `
        -Description "connect accept response"

    Wait-ForCleanServerExit `
        -Process $serverProcess `
        -OutputCapture $outputCapture `
        -Deadline $deadline
    if ($serverProcess.ExitCode -ne 0) {
        throw ("hlhost exited with code {0}" -f $serverProcess.ExitCode)
    }

    $capturedStdout = Get-SharedFileText -Path $stdoutPath
    $capturedStderr = Get-SharedFileText -Path $stderrPath
    $sessionProof = Assert-SessionSummary `
        -Stdout $capturedStdout `
        -ExpectedEndpoint ("127.0.0.1:{0}" -f $clientPort) `
        -ExpectedChallenge $challengeValue `
        -ExpectedChannelIdentifier $clientPort `
        -ExpectedSlot $expectedSlot `
        -ExpectedSessionCount $expectedSessionCount
    $serverProof = Assert-FinalServerSummary `
        -Stdout $capturedStdout `
        -ExpectedSessionCount $expectedSessionCount
}
catch {
    $failure = $_
}
finally {
    if ($null -ne $probeClient) {
        $probeClient.Close()
        $probeClient.Dispose()
    }

    if ($processStarted -and $null -ne $serverProcess) {
        try {
            if (-not $serverProcess.HasExited) {
                try {
                    if ($serverProcessId -gt 0) {
                        Stop-Process -Id $serverProcessId -Force -ErrorAction Stop
                    } else {
                        $serverProcess.Kill()
                    }
                }
                catch {
                    # A natural exit can race the termination request.
                    $serverProcess.Refresh()
                    if (-not $serverProcess.HasExited) {
                        throw
                    }
                }
                [void]$serverProcess.WaitForExit(2000)
            }
            if (-not $serverProcess.HasExited) {
                $cleanupFailure = "leaked directly owned hlhost process after forced termination"
            }
        }
        catch {
            $cleanupFailure = "failed to terminate leftover hlhost process: $($_.Exception.Message)"
        }
    }

    if ($null -ne $outputCapture) {
        try {
            Complete-ProcessOutputCapture -State $outputCapture
        }
        catch {
            $captureCleanupFailure = "failed to drain hlhost output: $($_.Exception.Message)"
            if ($null -eq $cleanupFailure) {
                $cleanupFailure = $captureCleanupFailure
            } else {
                $cleanupFailure += "; $captureCleanupFailure"
            }
        }
    }

    if ($processStarted) {
        if ($serverProcessId -le 0) {
            $pidCleanupFailure = "started hlhost process had no verifiable process ID"
            if ($null -eq $cleanupFailure) {
                $cleanupFailure = $pidCleanupFailure
            } else {
                $cleanupFailure += "; $pidCleanupFailure"
            }
        } elseif (Test-ProcessIdAlive -ProcessId $serverProcessId) {
            $pidCleanupFailure = ("hlhost process ID {0} is still alive after cleanup" -f $serverProcessId)
            if ($null -eq $cleanupFailure) {
                $cleanupFailure = $pidCleanupFailure
            } else {
                $cleanupFailure += "; $pidCleanupFailure"
            }
        }
    }

    $latestStdout = Get-SharedFileText -Path $stdoutPath
    $latestStderr = Get-SharedFileText -Path $stderrPath
    if ($latestStdout.Length -gt 0) {
        $capturedStdout = $latestStdout
    }
    if ($latestStderr.Length -gt 0) {
        $capturedStderr = $latestStderr
    }

    if (-not $SkipServerOutput) {
        if (-not [string]::IsNullOrWhiteSpace($capturedStdout)) {
            Write-Host $capturedStdout.TrimEnd([char[]]@("`r", "`n"))
        }
        if (-not [string]::IsNullOrWhiteSpace($capturedStderr)) {
            Write-Host $capturedStderr.TrimEnd([char[]]@("`r", "`n"))
        }
    }

    foreach ($tempPath in @($stdoutPath, $stderrPath)) {
        if (Test-Path -LiteralPath $tempPath -PathType Leaf) {
            Remove-Item -LiteralPath $tempPath -Force -ErrorAction SilentlyContinue
        }
    }
}

if ($null -ne $cleanupFailure) {
    if ($null -eq $failure) {
        $failure = New-Object System.Management.Automation.ErrorRecord(
            (New-Object System.InvalidOperationException($cleanupFailure)),
            "GoldsrcUdpProbeCleanupFailure",
            [System.Management.Automation.ErrorCategory]::CloseError,
            $serverProcess
        )
    }
}

if ($null -ne $failure) {
    $failureMessage = $failure.Exception.Message -replace '[\r\n]+', ' '
    Write-Host ("goldsrc_udp_probe: result=fail reason={0}" -f $failureMessage)
    throw $failure
}

Write-Host ("goldsrc_udp_probe: port={0}" -f $selectedPort)
Write-Host ("goldsrc_udp_probe: challenge={0}" -f $challengeValue)
Write-Host ("goldsrc_udp_probe: response=accept endpoint=127.0.0.1:{0}" -f $clientPort)
Write-Host ("goldsrc_udp_probe: session={0}" -f $sessionProof)
Write-Host ("goldsrc_udp_probe: server={0}" -f $serverProof)
Write-Host "goldsrc_udp_probe: result=pass"
