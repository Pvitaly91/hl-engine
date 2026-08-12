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
    [int]$TimeoutSeconds = 30,

    [switch]$ExerciseServerInfoRetransmitAndInvalidCommands,

    [switch]$SkipServerOutput
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
if (Test-Path Variable:PSNativeCommandUseErrorActionPreference) {
    $PSNativeCommandUseErrorActionPreference = $false
}
trap {
    Write-Host "goldsrc_serverinfo_probe: result=fail reason=proof_gate_failed"
    exit 1
}

$reliableToggleFlag = [uint32]2147483648
$fragmentFlag = [uint32]1073741824
$sequenceMask = [uint32]1073741823
$svcNop = [byte]0x01
$clcStringCmd = [byte]0x03
$svcServerInfo = [byte]0x0B
$svcSendExtraInfo = [byte]0x36

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

function Get-StableFieldValue {
    param(
        [string]$Line,
        [string]$Name
    )

    $pattern = '(?:^|[,\s])' + [regex]::Escape($Name) + '=(?<value>[^,\s]*)(?=$|[,\s])'
    $matches = [regex]::Matches(
        $Line,
        $pattern,
        [System.Text.RegularExpressions.RegexOptions]::CultureInvariant
    )
    if ($matches.Count -ne 1) {
        throw ("expected exactly one {0} field: {1}" -f $Name, $Line.Trim())
    }
    return $matches[0].Groups["value"].Value
}

function Get-StableUnsignedField {
    param(
        [string]$Line,
        [string]$Name
    )

    $text = Get-StableFieldValue -Line $Line -Name $Name
    $value = [uint64]0
    if (-not [uint64]::TryParse(
        $text,
        [Globalization.NumberStyles]::None,
        [Globalization.CultureInfo]::InvariantCulture,
        [ref]$value
    )) {
        throw ("{0} is not an unsigned integer: {1}" -f $Name, $Line.Trim())
    }
    return $value
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

    $maximumLinesPerStream = 32768
    $drainedLines = 0
    $stdoutBuilder = New-Object System.Text.StringBuilder
    while (-not $State.StdoutClosed -and
        $null -ne $State.StdoutTask -and
        $drainedLines -lt $maximumLinesPerStream) {
        if (-not $State.StdoutTask.IsCompleted) {
            try {
                [void]$State.StdoutTask.Wait(2)
            }
            catch {
                # The result access below reports the underlying stream failure.
            }
            if (-not $State.StdoutTask.IsCompleted) {
                break
            }
        }
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
            [void]$stdoutBuilder.AppendLine($line)
            $State.StdoutTask = $State.Process.StandardOutput.ReadLineAsync()
            $drainedLines++
        }
    }
    if ($stdoutBuilder.Length -gt 0) {
        [System.IO.File]::AppendAllText(
            $State.StdoutPath,
            $stdoutBuilder.ToString(),
            $State.Encoding
        )
    }

    $stderrLines = 0
    $stderrBuilder = New-Object System.Text.StringBuilder
    while (-not $State.StderrClosed -and
        $null -ne $State.StderrTask -and
        $stderrLines -lt $maximumLinesPerStream) {
        if (-not $State.StderrTask.IsCompleted) {
            try {
                [void]$State.StderrTask.Wait(2)
            }
            catch {
                # The result access below reports the underlying stream failure.
            }
            if (-not $State.StderrTask.IsCompleted) {
                break
            }
        }
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
            [void]$stderrBuilder.AppendLine($line)
            $State.StderrTask = $State.Process.StandardError.ReadLineAsync()
            $stderrLines++
        }
    }
    if ($stderrBuilder.Length -gt 0) {
        [System.IO.File]::AppendAllText(
            $State.StderrPath,
            $stderrBuilder.ToString(),
            $State.Encoding
        )
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
            Complete-ProcessOutputCapture -State $OutputCapture
            throw ("hlhost exited with code {0} before goldsrc_udp_ready" -f $Process.ExitCode)
        }
        $remainingMilliseconds = Get-RemainingMilliseconds -Deadline $Deadline
        if ($remainingMilliseconds -le 0) {
            throw "timeout waiting for goldsrc_udp_ready"
        }
        [void]$Process.WaitForExit([Math]::Min(50, $remainingMilliseconds))
    }
}

function Wait-ForStdoutToken {
    param(
        [System.Diagnostics.Process]$Process,
        $OutputCapture,
        [string]$StdoutPath,
        [DateTime]$Deadline,
        [string]$Token,
        [string]$Description
    )

    while ($true) {
        [void](Update-ProcessOutputCapture -State $OutputCapture)
        $stdout = Get-SharedFileText -Path $StdoutPath
        if ($stdout.IndexOf($Token, [StringComparison]::Ordinal) -ge 0) {
            return
        }
        if ($Process.HasExited) {
            Complete-ProcessOutputCapture -State $OutputCapture
            $stdout = Get-SharedFileText -Path $StdoutPath
            if ($stdout.IndexOf(
                    $Token,
                    [StringComparison]::Ordinal) -ge 0) {
                return
            }
            throw ("hlhost exited with code {0} while waiting for {1}" -f
                $Process.ExitCode,
                $Description)
        }
        $remainingMilliseconds = Get-RemainingMilliseconds -Deadline $Deadline
        if ($remainingMilliseconds -le 0) {
            throw ("timeout waiting for {0}" -f $Description)
        }
        [void]$Process.WaitForExit([Math]::Min(50, $remainingMilliseconds))
    }
}

function Wait-ForUdpPortOwnership {
    param(
        [System.Diagnostics.Process]$Process,
        [DateTime]$Deadline,
        [string]$ExpectedAddress,
        [int]$ExpectedPort,
        [int]$ExpectedProcessId
    )

    $udpEndpointCommand = Get-Command Get-NetUDPEndpoint -ErrorAction SilentlyContinue
    $netstatCommand = Get-Command netstat.exe -ErrorAction SilentlyContinue
    if ($null -eq $udpEndpointCommand -and $null -eq $netstatCommand) {
        throw "cannot verify UDP port ownership: Get-NetUDPEndpoint and netstat.exe are unavailable"
    }

    while ($true) {
        if ($null -ne $udpEndpointCommand) {
            try {
                $rows = @(Get-NetUDPEndpoint -LocalPort $ExpectedPort -ErrorAction Stop)
                foreach ($row in $rows) {
                    if ([int]$row.OwningProcess -eq $ExpectedProcessId -and
                        [string]$row.LocalAddress -eq $ExpectedAddress) {
                        return "pid={0},address={1},port={2}" -f
                            $ExpectedProcessId,
                            $ExpectedAddress,
                            $ExpectedPort
                    }
                }
            }
            catch {
                $udpEndpointCommand = $null
            }
        }

        if ($null -ne $netstatCommand) {
            $netstatLines = @(& $netstatCommand.Source -ano -p UDP 2>$null)
            foreach ($line in $netstatLines) {
                $match = [regex]::Match(
                    $line,
                    '^\s*UDP\s+(?<local>\S+)\s+\*:\*\s+(?<pid>[0-9]+)\s*$',
                    [System.Text.RegularExpressions.RegexOptions]::CultureInvariant
                )
                if ($match.Success -and
                    $match.Groups["local"].Value -eq ("{0}:{1}" -f $ExpectedAddress, $ExpectedPort) -and
                    [int]$match.Groups["pid"].Value -eq $ExpectedProcessId) {
                    return "pid={0},address={1},port={2}" -f
                        $ExpectedProcessId,
                        $ExpectedAddress,
                        $ExpectedPort
                }
            }
        }

        if ($Process.HasExited) {
            throw ("hlhost exited with code {0} before UDP ownership was verified" -f
                $Process.ExitCode)
        }
        $remainingMilliseconds = Get-RemainingMilliseconds -Deadline $Deadline
        if ($remainingMilliseconds -le 0) {
            throw ("timeout verifying that hlhost PID {0} owns {1}:{2}" -f
                $ExpectedProcessId,
                $ExpectedAddress,
                $ExpectedPort)
        }
        [void]$Process.WaitForExit([Math]::Min(50, $remainingMilliseconds))
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

    $previewLength = [Math]::Min($Bytes.Length, 96)
    $parts = New-Object System.Collections.Generic.List[string]
    for ($index = 0; $index -lt $previewLength; $index++) {
        $parts.Add($Bytes[$index].ToString("X2", [Globalization.CultureInfo]::InvariantCulture))
    }
    $suffix = if ($Bytes.Length -gt $previewLength) { "..." } else { "" }
    return (($parts -join " ") + $suffix)
}

function ConvertTo-CompactHex {
    param([byte[]]$Bytes)

    $builder = New-Object System.Text.StringBuilder($Bytes.Length * 2)
    foreach ($value in $Bytes) {
        [void]$builder.Append($value.ToString("x2", [Globalization.CultureInfo]::InvariantCulture))
    }
    return $builder.ToString()
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
            throw ("{0} byte mismatch at offset {1}: expected {2:X2}, received {3:X2}; bytes={4}" -f
                $Description,
                $index,
                $Expected[$index],
                $Actual[$index],
                (ConvertTo-HexPreview -Bytes $Actual))
        }
    }
}

function Assert-RemoteEndpoint {
    param(
        [System.Net.IPEndPoint]$Actual,
        [System.Net.IPEndPoint]$Expected,
        [string]$Description
    )

    if ($null -eq $Actual -or
        -not $Actual.Address.Equals($Expected.Address) -or
        $Actual.Port -ne $Expected.Port) {
        $actualText = if ($null -eq $Actual) { "<none>" } else { $Actual.ToString() }
        throw ("{0} source mismatch: expected {1}, received {2}" -f
            $Description,
            $Expected.ToString(),
            $actualText)
    }
}

function Receive-UdpDatagram {
    param(
        [System.Net.Sockets.UdpClient]$Client,
        [System.Diagnostics.Process]$ServerProcess,
        $OutputCapture,
        [DateTime]$Deadline,
        [System.Net.IPEndPoint]$ExpectedRemoteEndpoint,
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
            Assert-RemoteEndpoint `
                -Actual $remoteEndpoint `
                -Expected $ExpectedRemoteEndpoint `
                -Description $Description
            return [pscustomobject]@{
                Bytes = $received
                RemoteEndpoint = $remoteEndpoint
            }
        }
        catch [System.Net.Sockets.SocketException] {
            $socketError = $_.Exception.SocketErrorCode
            if ($socketError -eq [System.Net.Sockets.SocketError]::TimedOut -or
                $socketError -eq [System.Net.Sockets.SocketError]::WouldBlock) {
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

function Send-ExactUdpDatagram {
    param(
        [System.Net.Sockets.UdpClient]$Client,
        [byte[]]$Packet,
        [string]$Description
    )

    $sentLength = $Client.Send($Packet, $Packet.Length)
    if ($sentLength -ne $Packet.Length) {
        throw ("{0} send was short: expected {1}, sent {2}" -f
            $Description,
            $Packet.Length,
            $sentLength)
    }
}

function Assert-NoUdpDatagram {
    param(
        [System.Net.Sockets.UdpClient]$Client,
        [System.Diagnostics.Process]$ServerProcess,
        $OutputCapture,
        [ValidateRange(1, 5000)]
        [int]$DurationMilliseconds,
        [string]$Description
    )

    $silenceDeadline = [DateTime]::UtcNow.AddMilliseconds($DurationMilliseconds)
    while ([DateTime]::UtcNow -lt $silenceDeadline) {
        [void](Update-ProcessOutputCapture -State $OutputCapture)
        if ($ServerProcess.HasExited) {
            throw ("hlhost exited with code {0} while verifying {1}" -f
                $ServerProcess.ExitCode,
                $Description)
        }
        $remainingMilliseconds = [Math]::Max(
            1,
            [int][Math]::Ceiling(($silenceDeadline - [DateTime]::UtcNow).TotalMilliseconds)
        )
        $Client.Client.ReceiveTimeout = [Math]::Min(50, $remainingMilliseconds)
        $remoteEndpoint = New-Object System.Net.IPEndPoint([System.Net.IPAddress]::Any, 0)
        try {
            [byte[]]$received = $Client.Receive([ref]$remoteEndpoint)
            throw ("{0} unexpectedly received {1} bytes from {2}: {3}" -f
                $Description,
                $received.Length,
                $remoteEndpoint.ToString(),
                (ConvertTo-HexPreview -Bytes $received))
        }
        catch [System.Net.Sockets.SocketException] {
            $socketError = $_.Exception.SocketErrorCode
            if ($socketError -eq [System.Net.Sockets.SocketError]::TimedOut -or
                $socketError -eq [System.Net.Sockets.SocketError]::WouldBlock) {
                continue
            }
            throw
        }
    }
}

function Read-LittleEndianUInt32 {
    param(
        [byte[]]$Bytes,
        [int]$Offset
    )

    if ($Offset -lt 0 -or $Bytes.Length - $Offset -lt 4) {
        throw "uint32 read exceeds packet bounds"
    }
    $value = [uint64]$Bytes[$Offset]
    $value = $value -bor ([uint64]$Bytes[$Offset + 1] -shl 8)
    $value = $value -bor ([uint64]$Bytes[$Offset + 2] -shl 16)
    $value = $value -bor ([uint64]$Bytes[$Offset + 3] -shl 24)
    return [uint32]$value
}

function Set-LittleEndianUInt32 {
    param(
        [byte[]]$Bytes,
        [int]$Offset,
        [uint32]$Value
    )

    if ($Offset -lt 0 -or $Bytes.Length - $Offset -lt 4) {
        throw "uint32 write exceeds packet bounds"
    }
    $wide = [uint64]$Value
    $Bytes[$Offset] = [byte]($wide -band 255)
    $Bytes[$Offset + 1] = [byte](($wide -shr 8) -band 255)
    $Bytes[$Offset + 2] = [byte](($wide -shr 16) -band 255)
    $Bytes[$Offset + 3] = [byte](($wide -shr 24) -band 255)
}

function Get-NextNetchanSequence {
    param([uint32]$Sequence)

    if ($Sequence -ge $sequenceMask) {
        return [uint32]1
    }
    return [uint32]($Sequence + 1)
}

function Convert-GoldsrcMunge2 {
    param(
        [byte[]]$Bytes,
        [byte]$SequenceKey,
        [switch]$Reverse
    )

    [byte[]]$table = @(
        0x05, 0x61, 0x7A, 0xED, 0x1B, 0xCA, 0x0D, 0x9B,
        0x4A, 0xF1, 0x64, 0xC7, 0xB5, 0x8E, 0xDF, 0xA0
    )
    $result = New-Object byte[] $Bytes.Length
    if ($Bytes.Length -gt 0) {
        [Array]::Copy($Bytes, 0, $result, 0, $Bytes.Length)
    }

    [byte[]]$sequenceBytes = @($SequenceKey, 0, 0, 0)
    [byte[]]$inverseSequenceBytes = @(
        [byte]($SequenceKey -bxor 0xFF),
        0xFF,
        0xFF,
        0xFF
    )
    $blockCount = [Math]::Floor($Bytes.Length / 4)
    for ($block = 0; $block -lt $blockCount; $block++) {
        $offset = $block * 4
        $stage = New-Object byte[] 4
        for ($byteIndex = 0; $byteIndex -lt 4; $byteIndex++) {
            if ($Reverse) {
                $stage[$byteIndex] = [byte]($result[$offset + $byteIndex] -bxor
                    $sequenceBytes[$byteIndex])
            } else {
                $sourceIndex = 3 - $byteIndex
                $stage[$byteIndex] = [byte]($result[$offset + $sourceIndex] -bxor
                    $sequenceBytes[$sourceIndex])
            }

            $mask = 0xA5 -bor ($byteIndex -shl $byteIndex) -bor $byteIndex -bor
                $table[($block + $byteIndex) -band 0x0F]
            $stage[$byteIndex] = [byte]($stage[$byteIndex] -bxor $mask)
        }

        for ($byteIndex = 0; $byteIndex -lt 4; $byteIndex++) {
            $stageIndex = if ($Reverse) { 3 - $byteIndex } else { $byteIndex }
            $result[$offset + $byteIndex] = [byte]($stage[$stageIndex] -bxor
                $inverseSequenceBytes[$byteIndex])
        }
    }
    return ,$result
}

function New-NopPayload {
    $payload = New-Object byte[] 8
    for ($index = 0; $index -lt $payload.Length; $index++) {
        $payload[$index] = $svcNop
    }
    return ,$payload
}

function New-StringCommandPayload {
    param([string]$Command)

    $commandBytes = [System.Text.Encoding]::ASCII.GetBytes($Command)
    $payload = New-Object byte[] (2 + $commandBytes.Length)
    $payload[0] = $clcStringCmd
    if ($commandBytes.Length -gt 0) {
        [Array]::Copy($commandBytes, 0, $payload, 1, $commandBytes.Length)
    }
    $payload[$payload.Length - 1] = 0
    return ,$payload
}

function New-SequencedDatagram {
    param(
        [ValidateRange(0, 1073741823)]
        [uint32]$Sequence,
        [ValidateRange(0, 1073741823)]
        [uint32]$Acknowledgement,
        [switch]$ReliableToggle,
        [switch]$ReliableAcknowledgementToggle,
        [switch]$FragmentPresent,
        [byte[]]$Payload = @()
    )

    $rawSequenceWide = [uint64]$Sequence
    if ($ReliableToggle) {
        $rawSequenceWide = $rawSequenceWide -bor [uint64]2147483648
    }
    if ($FragmentPresent) {
        $rawSequenceWide = $rawSequenceWide -bor [uint64]1073741824
    }

    $rawAckWide = [uint64]$Acknowledgement
    if ($ReliableAcknowledgementToggle) {
        $rawAckWide = $rawAckWide -bor [uint64]2147483648
    }

    $packet = New-Object byte[] (8 + $Payload.Length)
    Set-LittleEndianUInt32 -Bytes $packet -Offset 0 -Value ([uint32]$rawSequenceWide)
    Set-LittleEndianUInt32 -Bytes $packet -Offset 4 -Value ([uint32]$rawAckWide)
    if ($Payload.Length -gt 0) {
        [byte[]]$transformed = Convert-GoldsrcMunge2 `
            -Bytes $Payload `
            -SequenceKey ([byte]($Sequence -band 0xFF))
        [Array]::Copy($transformed, 0, $packet, 8, $transformed.Length)
    }
    return ,$packet
}

function Read-SequencedDatagram {
    param(
        [byte[]]$Packet,
        [string]$Description
    )

    if ($Packet.Length -lt 8) {
        throw ("{0} is shorter than the 8-byte GoldSrc netchan header" -f $Description)
    }
    if ($Packet.Length -gt 2048) {
        throw ("{0} exceeds the 2048-byte proof limit" -f $Description)
    }
    if ($Packet.Length -ge 4 -and
        $Packet[0] -eq 0xFF -and
        $Packet[1] -eq 0xFF -and
        $Packet[2] -eq 0xFF -and
        $Packet[3] -eq 0xFF) {
        throw ("{0} is connectionless, not a sequenced packet" -f $Description)
    }

    $rawSequence = Read-LittleEndianUInt32 -Bytes $Packet -Offset 0
    $rawAck = Read-LittleEndianUInt32 -Bytes $Packet -Offset 4
    if (([uint64]$rawAck -band [uint64]$fragmentFlag) -ne 0) {
        throw ("{0} uses an unsupported acknowledgement flag: raw_ack=0x{1:X8}" -f
            $Description,
            $rawAck)
    }

    $sequence = [uint32]([uint64]$rawSequence -band [uint64]$sequenceMask)
    $ack = [uint32]([uint64]$rawAck -band [uint64]$sequenceMask)
    $payloadLength = $Packet.Length - 8
    $payload = New-Object byte[] $payloadLength
    if ($payloadLength -gt 0) {
        [Array]::Copy($Packet, 8, $payload, 0, $payloadLength)
        [byte[]]$payload = Convert-GoldsrcMunge2 `
            -Bytes $payload `
            -SequenceKey ([byte]($sequence -band 0xFF)) `
            -Reverse
    }

    return [pscustomobject]@{
        Bytes = $Packet
        RawSequence = $rawSequence
        RawAcknowledgement = $rawAck
        Sequence = $sequence
        Acknowledgement = $ack
        ReliableToggle = (([uint64]$rawSequence -band [uint64]$reliableToggleFlag) -ne 0)
        ReliableAcknowledgementToggle = (([uint64]$rawAck -band [uint64]$reliableToggleFlag) -ne 0)
        FragmentPresent = (([uint64]$rawSequence -band [uint64]$fragmentFlag) -ne 0)
        Payload = $payload
    }
}

function Assert-SequencedHeader {
    param(
        $Decoded,
        [uint32]$ExpectedSequence,
        [uint32]$ExpectedAcknowledgement,
        [bool]$ExpectedReliableToggle,
        [bool]$ExpectedReliableAcknowledgementToggle,
        [string]$Description
    )

    if ($Decoded.Sequence -ne $ExpectedSequence) {
        throw ("{0} sequence mismatch: expected {1}, received {2}" -f
            $Description,
            $ExpectedSequence,
            $Decoded.Sequence)
    }
    if ($Decoded.Acknowledgement -ne $ExpectedAcknowledgement) {
        throw ("{0} acknowledgement mismatch: expected {1}, received {2}" -f
            $Description,
            $ExpectedAcknowledgement,
            $Decoded.Acknowledgement)
    }
    if ($Decoded.ReliableToggle -ne $ExpectedReliableToggle) {
        throw ("{0} raw reliable-toggle mismatch: expected {1}, received {2}" -f
            $Description,
            [int]$ExpectedReliableToggle,
            [int]$Decoded.ReliableToggle)
    }
    if ($Decoded.ReliableAcknowledgementToggle -ne $ExpectedReliableAcknowledgementToggle) {
        throw ("{0} raw reliable-ack toggle mismatch: expected {1}, received {2}" -f
            $Description,
            [int]$ExpectedReliableAcknowledgementToggle,
            [int]$Decoded.ReliableAcknowledgementToggle)
    }
    if ($Decoded.FragmentPresent) {
        throw ("{0} unexpectedly carries the fragmentation flag" -f $Description)
    }
}

function Assert-NopPayload {
    param(
        [byte[]]$Payload,
        [string]$Description
    )

    [byte[]]$expected = New-NopPayload
    Assert-ExactBytes -Actual $Payload -Expected $expected -Description $Description
}

function New-PayloadReader {
    param([byte[]]$Bytes)

    return [pscustomobject]@{
        Bytes = $Bytes
        Offset = 0
    }
}

function Read-PayloadByte {
    param(
        $Reader,
        [string]$FieldName
    )

    if ($Reader.Offset -ge $Reader.Bytes.Length) {
        throw ("svc_serverinfo is truncated before {0}" -f $FieldName)
    }
    $value = $Reader.Bytes[$Reader.Offset]
    $Reader.Offset++
    return [byte]$value
}

function Read-PayloadUInt32 {
    param(
        $Reader,
        [string]$FieldName
    )

    if ($Reader.Bytes.Length - $Reader.Offset -lt 4) {
        throw ("svc_serverinfo is truncated before {0}" -f $FieldName)
    }
    $value = Read-LittleEndianUInt32 -Bytes $Reader.Bytes -Offset $Reader.Offset
    $Reader.Offset += 4
    return $value
}

function Read-PayloadBytes {
    param(
        $Reader,
        [ValidateRange(0, 2048)]
        [int]$Count,
        [string]$FieldName
    )

    if ($Reader.Bytes.Length - $Reader.Offset -lt $Count) {
        throw ("svc_serverinfo is truncated before {0}" -f $FieldName)
    }
    $result = New-Object byte[] $Count
    if ($Count -gt 0) {
        [Array]::Copy($Reader.Bytes, $Reader.Offset, $result, 0, $Count)
    }
    $Reader.Offset += $Count
    return ,$result
}

function Read-PayloadAsciiString {
    param(
        $Reader,
        [ValidateRange(0, 1024)]
        [int]$MaximumBytes,
        [string]$FieldName,
        [switch]$AllowEmpty
    )

    $start = $Reader.Offset
    $terminator = -1
    while ($Reader.Offset -lt $Reader.Bytes.Length -and
        ($Reader.Offset - $start) -le $MaximumBytes) {
        $value = $Reader.Bytes[$Reader.Offset]
        $Reader.Offset++
        if ($value -eq 0) {
            $terminator = $Reader.Offset - 1
            break
        }
        if ($value -lt 0x20 -or $value -gt 0x7E) {
            throw ("svc_serverinfo {0} contains non-printable byte 0x{1:X2}" -f
                $FieldName,
                $value)
        }
    }
    if ($terminator -lt 0) {
        throw ("svc_serverinfo {0} is missing a bounded NUL terminator" -f $FieldName)
    }
    $length = $terminator - $start
    if ($length -gt $MaximumBytes) {
        throw ("svc_serverinfo {0} exceeds {1} bytes" -f $FieldName, $MaximumBytes)
    }
    if ($length -eq 0 -and -not $AllowEmpty) {
        throw ("svc_serverinfo {0} must not be empty" -f $FieldName)
    }
    if ($length -eq 0) {
        return ""
    }
    return [System.Text.Encoding]::ASCII.GetString($Reader.Bytes, $start, $length)
}

function Read-ServerInfoPayload {
    param([byte[]]$Payload)

    if ($Payload.Length -eq 0 -or $Payload.Length -gt 1400) {
        throw ("svc_serverinfo payload length is outside the bounded routeable range: {0}" -f
            $Payload.Length)
    }
    $reader = New-PayloadReader -Bytes $Payload
    $opcode = Read-PayloadByte -Reader $reader -FieldName "svc_serverinfo opcode"
    if ($opcode -ne $svcServerInfo) {
        throw ("first bootstrap opcode mismatch: expected svc_serverinfo 0x{0:X2}, received 0x{1:X2}" -f
            $svcServerInfo,
            $opcode)
    }

    $protocol = Read-PayloadUInt32 -Reader $reader -FieldName "protocol"
    $spawnCount = Read-PayloadUInt32 -Reader $reader -FieldName "spawn count"
    $checksum = Read-PayloadUInt32 -Reader $reader -FieldName "wire map checksum"
    [byte[]]$clientDllMd5Bytes = Read-PayloadBytes `
        -Reader $reader `
        -Count 16 `
        -FieldName "client DLL MD5"
    $maxClients = Read-PayloadByte -Reader $reader -FieldName "max clients"
    $playerIndex = Read-PayloadByte -Reader $reader -FieldName "player index"
    $deathmatch = Read-PayloadByte -Reader $reader -FieldName "deathmatch"
    $gameDir = Read-PayloadAsciiString `
        -Reader $reader `
        -MaximumBytes 64 `
        -FieldName "game directory"
    $hostname = Read-PayloadAsciiString `
        -Reader $reader `
        -MaximumBytes 128 `
        -FieldName "hostname"
    $map = Read-PayloadAsciiString `
        -Reader $reader `
        -MaximumBytes 128 `
        -FieldName "map/model path"
    $mapcycle = Read-PayloadAsciiString `
        -Reader $reader `
        -MaximumBytes 128 `
        -FieldName "mapcycle"
    $secure = Read-PayloadByte -Reader $reader -FieldName "secure mode"
    $extraInfoOpcode = Read-PayloadByte -Reader $reader -FieldName "svc_sendextrainfo opcode"
    if ($extraInfoOpcode -ne $svcSendExtraInfo) {
        throw ("svc_sendextrainfo companion opcode mismatch: expected 0x{0:X2}, received 0x{1:X2}" -f
            $svcSendExtraInfo,
            $extraInfoOpcode)
    }
    $fallbackDir = Read-PayloadAsciiString `
        -Reader $reader `
        -MaximumBytes 64 `
        -FieldName "fallback directory" `
        -AllowEmpty
    $allowCheats = Read-PayloadByte -Reader $reader -FieldName "allow cheats"
    if ($reader.Offset -ne $Payload.Length) {
        throw ("svc_serverinfo payload has {0} unexpected trailing bytes at offset {1}: {2}" -f
            ($Payload.Length - $reader.Offset),
            $reader.Offset,
            (ConvertTo-HexPreview -Bytes $Payload))
    }

    return [pscustomobject]@{
        Bytes = $Payload
        Protocol = $protocol
        SpawnCount = $spawnCount
        Checksum = $checksum
        ChecksumHex = $checksum.ToString("x8", [Globalization.CultureInfo]::InvariantCulture)
        ClientDllMd5Bytes = $clientDllMd5Bytes
        ClientDllMd5Hex = ConvertTo-CompactHex -Bytes $clientDllMd5Bytes
        MaxClients = $maxClients
        PlayerIndex = $playerIndex
        Deathmatch = $deathmatch
        GameDir = $gameDir
        Hostname = $hostname
        Map = $map
        Mapcycle = $mapcycle
        Secure = $secure
        ExtraInfoOpcode = $extraInfoOpcode
        FallbackDir = $fallbackDir
        AllowCheats = $allowCheats
        PayloadBytes = $Payload.Length
    }
}

function Assert-ServerInfoRuntimeExpectations {
    param(
        $ServerInfo,
        [string]$ExpectedClientDllMd5,
        [ValidateRange(1, 255)]
        [int]$ExpectedMaxClients = 1,
        [ValidateRange(0, 254)]
        [int]$ExpectedPlayerIndex = 0
    )

    if ($ServerInfo.Protocol -ne 48) {
        throw ("svc_serverinfo protocol mismatch: expected 48, received {0}" -f
            $ServerInfo.Protocol)
    }
    if ($ServerInfo.SpawnCount -eq 0) {
        throw "svc_serverinfo production spawn count must not be zero"
    }
    if ($ServerInfo.Checksum -eq 0) {
        throw "svc_serverinfo production wire map checksum must not be zero"
    }
    if ($ServerInfo.ClientDllMd5Hex -ne $ExpectedClientDllMd5.ToLowerInvariant()) {
        throw ("svc_serverinfo client DLL MD5 mismatch: expected {0}, received {1}" -f
            $ExpectedClientDllMd5.ToLowerInvariant(),
            $ServerInfo.ClientDllMd5Hex)
    }
    if ($ServerInfo.MaxClients -ne $ExpectedMaxClients) {
        throw ("svc_serverinfo max clients mismatch: expected {0}, received {1}" -f
            $ExpectedMaxClients,
            $ServerInfo.MaxClients)
    }
    if ($ServerInfo.PlayerIndex -ne $ExpectedPlayerIndex) {
        throw ("svc_serverinfo zero-based player index mismatch: expected {0}, received {1}" -f
            $ExpectedPlayerIndex,
            $ServerInfo.PlayerIndex)
    }
    if ($ServerInfo.Deathmatch -ne 1) {
        throw ("svc_serverinfo deathmatch mismatch: expected 1, received {0}" -f
            $ServerInfo.Deathmatch)
    }

    [string]$expectedMap = Get-Variable `
        -Scope Script `
        -Name goldsrcExpectedServerInfoMap `
        -ValueOnly `
        -ErrorAction SilentlyContinue
    if ([string]::IsNullOrWhiteSpace($expectedMap)) {
        $expectedMap = "maps/c0a0.bsp"
    }
    $expectedStrings = [ordered]@{
        GameDir = "valve"
        Hostname = "HLengine Test Server"
        Map = $expectedMap
        Mapcycle = "mapcycle.txt"
        FallbackDir = ""
    }
    foreach ($field in $expectedStrings.GetEnumerator()) {
        if ([string]$ServerInfo.($field.Key) -cne $field.Value) {
            throw ("svc_serverinfo {0} mismatch: expected '{1}', received '{2}'" -f
                $field.Key,
                $field.Value,
                [string]$ServerInfo.($field.Key))
        }
    }
    if ($ServerInfo.Secure -ne 0) {
        throw ("svc_serverinfo secure mode mismatch: expected 0, received {0}" -f
            $ServerInfo.Secure)
    }
    if ($ServerInfo.ExtraInfoOpcode -ne $svcSendExtraInfo) {
        throw "svc_serverinfo is missing the required svc_sendextrainfo companion"
    }
    if ($ServerInfo.AllowCheats -ne 0) {
        throw ("svc_sendextrainfo allow-cheats mismatch: expected 0, received {0}" -f
            $ServerInfo.AllowCheats)
    }
}

function Receive-AndAssertNopAck {
    param(
        [System.Net.Sockets.UdpClient]$Client,
        [System.Diagnostics.Process]$ServerProcess,
        $OutputCapture,
        [DateTime]$Deadline,
        [System.Net.IPEndPoint]$ServerEndpoint,
        [uint32]$ExpectedServerSequence,
        [uint32]$ExpectedClientSequence,
        [bool]$ExpectedServerReliableToggle,
        [bool]$ExpectedClientReliableToggle,
        [string]$Description
    )

    $datagram = Receive-UdpDatagram `
        -Client $Client `
        -ServerProcess $ServerProcess `
        -OutputCapture $OutputCapture `
        -Deadline $Deadline `
        -ExpectedRemoteEndpoint $ServerEndpoint `
        -Description $Description
    $decoded = Read-SequencedDatagram -Packet $datagram.Bytes -Description $Description
    Assert-SequencedHeader `
        -Decoded $decoded `
        -ExpectedSequence $ExpectedServerSequence `
        -ExpectedAcknowledgement $ExpectedClientSequence `
        -ExpectedReliableToggle $ExpectedServerReliableToggle `
        -ExpectedReliableAcknowledgementToggle $ExpectedClientReliableToggle `
        -Description $Description
    if ($decoded.Bytes.Length -ne 16) {
        throw ("{0} must be a 16-byte padded ACK, received {1} bytes" -f
            $Description,
            $decoded.Bytes.Length)
    }
    Assert-NopPayload -Payload $decoded.Payload -Description ("{0} svc_nop padding" -f $Description)
    return $decoded
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
            throw "timeout waiting for hlhost to exit after the serverinfo proof"
        }
        [void]$Process.WaitForExit([Math]::Min(100, $remainingMilliseconds))
    }
    $Process.Refresh()
    Complete-ProcessOutputCapture -State $OutputCapture
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

function Assert-NoDirectChildProcessLeak {
    param([int]$ParentProcessId)

    $cimCommand = Get-Command Get-CimInstance -ErrorAction SilentlyContinue
    if ($null -eq $cimCommand) {
        throw "cannot verify child-process cleanup because Get-CimInstance is unavailable"
    }

    try {
        $children = @(
            Get-CimInstance Win32_Process `
                -Filter ("ParentProcessId={0}" -f $ParentProcessId) `
                -ErrorAction Stop
        )
    }
    catch {
        throw ("cannot verify child-process cleanup: {0}" -f
            $_.Exception.GetBaseException().Message)
    }

    $aliveIds = @(
        $children |
            ForEach-Object { [int]$_.ProcessId } |
            Where-Object { Test-ProcessIdAlive -ProcessId $_ }
    )
    if ($aliveIds.Count -gt 0) {
        throw ("hlhost leaked direct child process IDs: {0}" -f ($aliveIds -join ","))
    }
    return "pass"
}

function Get-RepositoryFileSnapshot {
    param([string]$RepositoryRoot)

    $snapshot = New-Object 'System.Collections.Generic.Dictionary[string,string]' `
        ([StringComparer]::OrdinalIgnoreCase)
    $files = @(
        Get-ChildItem -LiteralPath $RepositoryRoot -Recurse -File -ErrorAction SilentlyContinue
    )
    foreach ($file in $files) {
        $fullPath = [System.IO.Path]::GetFullPath($file.FullName)
        $relativePath = $fullPath.Substring($RepositoryRoot.Length).TrimStart('\', '/')
        $snapshot[$relativePath] = "{0}:{1}" -f
            $file.Length,
            $file.LastWriteTimeUtc.Ticks
    }
    return ,$snapshot
}

function Assert-NoRepositoryFileMutation {
    param(
        [string]$RepositoryRoot,
        [System.Collections.Generic.Dictionary[string,string]]$Before
    )

    $after = Get-RepositoryFileSnapshot -RepositoryRoot $RepositoryRoot
    $mutated = New-Object System.Collections.Generic.List[string]
    foreach ($entry in $after.GetEnumerator()) {
        if (-not $Before.ContainsKey($entry.Key)) {
            $mutated.Add(("created:{0}" -f $entry.Key))
        } elseif ($Before[$entry.Key] -ne $entry.Value) {
            $mutated.Add(("modified:{0}" -f $entry.Key))
        }
    }
    foreach ($entry in $Before.GetEnumerator()) {
        if (-not $after.ContainsKey($entry.Key)) {
            $mutated.Add(("removed:{0}" -f $entry.Key))
        }
    }
    if ($mutated.Count -gt 0) {
        throw ("hlhost mutated repository files during the proof: {0}" -f
            ($mutated -join ", "))
    }
}

function Assert-SessionSummary {
    param(
        [string]$Stdout,
        [string]$ExpectedEndpoint,
        [int]$ExpectedChallenge,
        [int]$ExpectedChannelIdentifier
    )

    $summaryLines = @(
        $Stdout -split "`r?`n" |
            Where-Object { $_.IndexOf("goldsrc_udp_session:", [StringComparison]::Ordinal) -ge 0 }
    )
    if ($summaryLines.Count -eq 0) {
        throw "stdout is missing goldsrc_udp_session summary"
    }

    $expectedFields = [ordered]@{
        count = "1"
        state = "connected"
        connected = "1"
        put_in_server = "0"
        spawned = "0"
        active = "0"
        slot = "1"
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
}

function Assert-UdpSummary {
    param(
        [string]$Stdout,
        [int]$ExpectedDatagrams
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
        datagrams = [string]$ExpectedDatagrams
        challenges = "1"
        connects = "1"
        accepted = "1"
        rejected = "0"
        last_reject_reason = "<none>"
        session_count = "1"
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
    if ((Get-StableUnsignedField -Line $line -Name "frames") -eq 0) {
        throw ("goldsrc_udp_summary expected a positive frames count: {0}" -f $line.Trim())
    }
}

function Assert-NetchanSummaryCore {
    param([string]$Stdout)

    $summaryLines = @(
        $Stdout -split "`r?`n" |
            Where-Object { $_.IndexOf("goldsrc_netchan_summary:", [StringComparison]::Ordinal) -ge 0 }
    )
    if ($summaryLines.Count -ne 1) {
        throw ("stdout expected exactly one goldsrc_netchan_summary line, found {0}" -f
            $summaryLines.Count)
    }
    $line = $summaryLines[0]
    $expectedFields = [ordered]@{
        enabled = "1"
        initialized = "1"
        netchan_state = "established"
        server_initial_sequence = "1"
        server_initial_ack = "0"
        fragment_present = "0"
        payload_transform = "pass"
        first_payload = "svc_nop"
        reliable_pending_bytes = "0"
        session_count = "1"
        state = "connected"
        put_in_server = "0"
        spawned = "0"
        active = "0"
        clean_shutdown = "1"
    }
    foreach ($field in $expectedFields.GetEnumerator()) {
        if (-not (Test-StableField -Line $line -Name $field.Key -ExpectedValue $field.Value)) {
            throw ("goldsrc_netchan_summary expected {0}={1}: {2}" -f
                $field.Key,
                $field.Value,
                $line.Trim())
        }
    }
}

function Assert-ServerInfoSummary {
    param(
        [string]$Stdout,
        $DecodedServerInfo,
        [string]$ExpectedClientDllMd5,
        [bool]$NegativeProof
    )

    $summaryLines = @(
        $Stdout -split "`r?`n" |
            Where-Object { $_.IndexOf("goldsrc_serverinfo_summary:", [StringComparison]::Ordinal) -ge 0 }
    )
    if ($summaryLines.Count -ne 1) {
        throw ("stdout expected exactly one goldsrc_serverinfo_summary line, found {0}" -f
            $summaryLines.Count)
    }
    $line = $summaryLines[0]
    $negativeValue = if ($NegativeProof) { "1" } else { "0" }
    $expectedFields = [ordered]@{
        enabled = "1"
        negative_proof = $negativeValue
        client_new_delivered = "1"
        duplicate_new_deliveries = "0"
        serverinfo_context_built = "1"
        serverinfo_generations = "1"
        serverinfo_queued = "1"
        serverinfo_acked = "1"
        serverinfo_protocol = "48"
        serverinfo_maxclients = "1"
        serverinfo_player_index = "0"
        serverinfo_deathmatch = "1"
        serverinfo_game_dir = "valve"
        serverinfo_hostname = "HLengine_Test_Server"
        serverinfo_map = "maps/c0a0.bsp"
        serverinfo_mapcycle = "mapcycle.txt"
        serverinfo_secure = "0"
        serverinfo_fallback_dir = "<empty>"
        serverinfo_allow_cheats = "0"
        signon_phase = "serverinfo_acknowledged"
        session_count = "1"
        state = "connected"
        netchan_state = "established"
        reliable_pending_bytes = "0"
        put_in_server = "0"
        spawned = "0"
        active = "0"
        server_still_responsive = if ($NegativeProof) { "1" } else { "0" }
        clean_shutdown = "1"
    }
    foreach ($field in $expectedFields.GetEnumerator()) {
        if (-not (Test-StableField -Line $line -Name $field.Key -ExpectedValue $field.Value)) {
            throw ("goldsrc_serverinfo_summary expected {0}={1}: {2}" -f
                $field.Key,
                $field.Value,
                $line.Trim())
        }
    }

    $spawnCount = Get-StableUnsignedField -Line $line -Name "serverinfo_spawn_count"
    if ($spawnCount -ne [uint64]$DecodedServerInfo.SpawnCount) {
        throw ("summary/wire serverinfo spawn-count mismatch: summary={0}, wire={1}" -f
            $spawnCount,
            $DecodedServerInfo.SpawnCount)
    }
    $checksum = Get-StableFieldValue -Line $line -Name "serverinfo_checksum"
    if ($checksum -notmatch '\A[0-9A-Fa-f]{8}\z' -or
        $checksum.ToLowerInvariant() -cne $DecodedServerInfo.ChecksumHex) {
        throw ("summary/wire serverinfo checksum mismatch: summary={0}, wire={1}" -f
            $checksum,
            $DecodedServerInfo.ChecksumHex)
    }
    $md5 = Get-StableFieldValue -Line $line -Name "serverinfo_client_dll_md5"
    if ($md5 -notmatch '\A[0-9A-Fa-f]{32}\z' -or
        $md5.ToLowerInvariant() -cne $DecodedServerInfo.ClientDllMd5Hex -or
        $md5.ToLowerInvariant() -cne $ExpectedClientDllMd5.ToLowerInvariant()) {
        throw ("summary/wire/runtime client DLL MD5 mismatch: summary={0}, wire={1}, runtime={2}" -f
            $md5,
            $DecodedServerInfo.ClientDllMd5Hex,
            $ExpectedClientDllMd5.ToLowerInvariant())
    }
    $payloadBytes = Get-StableUnsignedField -Line $line -Name "serverinfo_payload_bytes"
    if ($payloadBytes -ne [uint64]$DecodedServerInfo.PayloadBytes) {
        throw ("summary/wire serverinfo payload-size mismatch: summary={0}, wire={1}" -f
            $payloadBytes,
            $DecodedServerInfo.PayloadBytes)
    }

    $newReceived = Get-StableUnsignedField -Line $line -Name "client_new_received"
    if ($newReceived -lt 1 -or $newReceived -gt 2) {
        throw ("goldsrc_serverinfo_summary expected 1..2 client_new_received: {0}" -f
            $line.Trim())
    }
    if ((Get-StableUnsignedField -Line $line -Name "serverinfo_sent") -lt 1) {
        throw ("goldsrc_serverinfo_summary expected serverinfo_sent>=1: {0}" -f $line.Trim())
    }
    $firstCarrier = Get-StableUnsignedField `
        -Line $line `
        -Name "serverinfo_first_carrier_sequence"
    $latestCarrier = Get-StableUnsignedField `
        -Line $line `
        -Name "serverinfo_latest_carrier_sequence"
    $sendCount = Get-StableUnsignedField `
        -Line $line `
        -Name "serverinfo_send_count"
    if ($firstCarrier -ne 3) {
        throw ("goldsrc_serverinfo_summary expected first carrier sequence 3: {0}" -f
            $line.Trim())
    }
    if ($NegativeProof) {
        if ($latestCarrier -ne 5 -or $sendCount -ne 2) {
            throw ("goldsrc_serverinfo_summary expected two carriers ending at sequence 5: {0}" -f
                $line.Trim())
        }
    } elseif ($latestCarrier -ne 3 -or $sendCount -ne 1) {
        throw ("goldsrc_serverinfo_summary expected one carrier at sequence 3: {0}" -f
            $line.Trim())
    }

    $resent = Get-StableUnsignedField -Line $line -Name "serverinfo_resent"
    $malformed = Get-StableUnsignedField -Line $line -Name "malformed_stringcmd_rejected"
    $injection = Get-StableUnsignedField -Line $line -Name "command_injection_rejected"
    $unsupported = Get-StableUnsignedField -Line $line -Name "unsupported_command_rejected"
    $unsupportedOpcode = Get-StableUnsignedField `
        -Line $line `
        -Name "unsupported_opcode_rejected"
    $wrongAck = Get-StableUnsignedField -Line $line -Name "wrong_reliable_ack_rejected"
    $futureAck = Get-StableUnsignedField -Line $line -Name "future_ack_rejected"
    $duplicateReliable = Get-StableUnsignedField `
        -Line $line `
        -Name "duplicate_client_reliable_suppressed"
    if ($NegativeProof) {
        if ($resent -lt 1 -or
            $malformed -lt 3 -or
            $injection -lt 2 -or
            $unsupported -lt 1 -or
            $unsupportedOpcode -lt 1 -or
            $wrongAck -lt 1 -or
            $futureAck -lt 1 -or
            $duplicateReliable -lt 1) {
            throw ("goldsrc_serverinfo_summary negative counters are incomplete: {0}" -f
                $line.Trim())
        }
    } elseif ($resent -ne 0 -or
        $malformed -ne 0 -or
        $injection -ne 0 -or
        $unsupported -ne 0 -or
        $wrongAck -ne 0 -or
        $futureAck -ne 0 -or
        $duplicateReliable -ne 0 -or
        $unsupportedOpcode -ne 0) {
        throw ("goldsrc_serverinfo_summary Proof A contains negative evidence: {0}" -f
            $line.Trim())
    }
    return $line
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
$mapPath = Join-Path $resolvedGameDir "maps\c0a0.bsp"
if (-not (Test-Path -LiteralPath $mapPath -PathType Leaf)) {
    throw ("proof map not found: {0}" -f $mapPath)
}
$clientDllPath = $null
foreach ($candidate in @(
    (Join-Path $resolvedGameDir "cl_dlls\client.dll"),
    (Join-Path $resolvedGameDir "dlls\client.dll")
)) {
    if (Test-Path -LiteralPath $candidate -PathType Leaf) {
        $clientDllPath = $candidate
        break
    }
}
if ($null -eq $clientDllPath) {
    throw "proof game directory is missing the target client library"
}
$expectedClientDllMd5 = (Get-FileHash -LiteralPath $clientDllPath -Algorithm MD5).Hash.ToLowerInvariant()

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
        $portReservation = New-Object System.Net.Sockets.UdpClient(0)
        $selectedPort = ([System.Net.IPEndPoint]$portReservation.Client.LocalEndPoint).Port
    }
    finally {
        if ($null -ne $portReservation) {
            $portReservation.Close()
            $portReservation.Dispose()
        }
    }
}

$handshakeTimeoutMilliseconds = [Math]::Min(
    60000,
    [Math]::Max(1000, ($TimeoutSeconds * 1000))
)
$arguments = @(
    "--gamedir", $resolvedGameDir,
    "--dedicated",
    "--deathmatch", "1",
    "--maxclients", "1",
    "--map", "c0a0",
    "--frames", "1",
    "--log-to-file", "0",
    "--log-summary-file", "0",
    "--log-disable-categories=general",
    "--ip", $BindAddress,
    "--port", ([string]$selectedPort),
    "--goldsrc-serverinfo",
    "--goldsrc-handshake-timeout-ms", ([string]$handshakeTimeoutMilliseconds)
)
if ($ExerciseServerInfoRetransmitAndInvalidCommands) {
    $arguments += "--goldsrc-serverinfo-negative-proof"
}
$argumentLine = (($arguments | ForEach-Object {
    ConvertTo-WindowsCommandLineArgument -Value ([string]$_)
}) -join " ")
$tempPrefix = "hlhost_goldsrc_serverinfo_{0}" -f ([Guid]::NewGuid().ToString("N"))
$stdoutPath = Join-Path ([System.IO.Path]::GetTempPath()) ($tempPrefix + ".stdout.log")
$stderrPath = Join-Path ([System.IO.Path]::GetTempPath()) ($tempPrefix + ".stderr.log")
$deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
$repositorySnapshotBefore = Get-RepositoryFileSnapshot -RepositoryRoot $repoRoot

$serverProcess = $null
$serverProcessId = 0
$processStarted = $false
$outputCapture = $null
$probeClient = $null
$hijackClient = $null
$capturedStdout = ""
$capturedStderr = ""
$failure = $null
$cleanupFailure = $null
$challengeValue = 0
$clientPort = 0
$portOwnershipProof = "not_checked"
$childProcessProof = "not_checked"
$decodedServerInfo = $null

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
    $portOwnershipProof = Wait-ForUdpPortOwnership `
        -Process $serverProcess `
        -Deadline $deadline `
        -ExpectedAddress $BindAddress `
        -ExpectedPort $selectedPort `
        -ExpectedProcessId $serverProcessId

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
    Send-ExactUdpDatagram `
        -Client $probeClient `
        -Packet $challengeRequest `
        -Description "challenge request"
    $challengeDatagram = Receive-UdpDatagram `
        -Client $probeClient `
        -ServerProcess $serverProcess `
        -OutputCapture $outputCapture `
        -Deadline $deadline `
        -ExpectedRemoteEndpoint $serverEndpoint `
        -Description "challenge response"
    [byte[]]$challengeResponse = $challengeDatagram.Bytes
    Assert-ConnectionlessPrefix -Packet $challengeResponse -Description "challenge response"
    $challengeBody = [System.Text.Encoding]::ASCII.GetString(
        $challengeResponse,
        4,
        $challengeResponse.Length - 4
    )
    $challengeMatch = [regex]::Match(
        $challengeBody,
        '\AA00000000 (?<challenge>0|[1-9][0-9]*) 3 0 0\n\x00\z',
        [System.Text.RegularExpressions.RegexOptions]::CultureInvariant
    )
    if (-not $challengeMatch.Success -or -not [int]::TryParse(
            $challengeMatch.Groups["challenge"].Value,
            [Globalization.NumberStyles]::None,
            [Globalization.CultureInfo]::InvariantCulture,
            [ref]$challengeValue)) {
        throw "malformed challenge response"
    }

    $protocolInfo = '\prot\3\unique\-1\raw\steam\cdkey\00000000000000000000000000000000'
    $userInfo = '\name\serverinfo_probe\model\gordon'
    $connectLine = ('connect 48 {0} "{1}" "{2}"' -f
        $challengeValue,
        $protocolInfo,
        $userInfo) + "`n"
    [byte[]]$connectRequest = New-ConnectionlessDatagram `
        -Text $connectLine `
        -Tail ([byte[]]@(0x00, 0x01, 0x7F, 0x80, 0xFF))
    Send-ExactUdpDatagram `
        -Client $probeClient `
        -Packet $connectRequest `
        -Description "connect request"
    $acceptDatagram = Receive-UdpDatagram `
        -Client $probeClient `
        -ServerProcess $serverProcess `
        -OutputCapture $outputCapture `
        -Deadline $deadline `
        -ExpectedRemoteEndpoint $serverEndpoint `
        -Description "connect accept response"
    [byte[]]$expectedAcceptResponse = New-ConnectionlessDatagram `
        -Text ('B 1 "127.0.0.1:{0}" 0 5971' -f $clientPort) `
        -Tail ([byte[]](0x00))
    Assert-ExactBytes `
        -Actual $acceptDatagram.Bytes `
        -Expected $expectedAcceptResponse `
        -Description "connect accept response"

    $serverOneDatagram = Receive-UdpDatagram `
        -Client $probeClient `
        -ServerProcess $serverProcess `
        -OutputCapture $outputCapture `
        -Deadline $deadline `
        -ExpectedRemoteEndpoint $serverEndpoint `
        -Description "initial reliable netchan packet"
    $serverOne = Read-SequencedDatagram `
        -Packet $serverOneDatagram.Bytes `
        -Description "initial reliable netchan packet"
    Assert-SequencedHeader `
        -Decoded $serverOne `
        -ExpectedSequence 1 `
        -ExpectedAcknowledgement 0 `
        -ExpectedReliableToggle $true `
        -ExpectedReliableAcknowledgementToggle $false `
        -Description "initial reliable netchan packet"
    Assert-NopPayload -Payload $serverOne.Payload -Description "initial svc_nop payload"

    [byte[]]$nopPayload = New-NopPayload
    [byte[]]$transportAck = New-SequencedDatagram `
        -Sequence 1 `
        -Acknowledgement 1 `
        -ReliableAcknowledgementToggle `
        -Payload $nopPayload
    Send-ExactUdpDatagram `
        -Client $probeClient `
        -Packet $transportAck `
        -Description "transport reliable acknowledgement"
    [void](Receive-AndAssertNopAck `
        -Client $probeClient `
        -ServerProcess $serverProcess `
        -OutputCapture $outputCapture `
        -Deadline $deadline `
        -ServerEndpoint $serverEndpoint `
        -ExpectedServerSequence 2 `
        -ExpectedClientSequence 1 `
        -ExpectedServerReliableToggle $false `
        -ExpectedClientReliableToggle $false `
        -Description "established netchan acknowledgement")

    [byte[]]$newPayload = New-StringCommandPayload -Command "new"
    [byte[]]$clientNew = New-SequencedDatagram `
        -Sequence 2 `
        -Acknowledgement 2 `
        -ReliableToggle `
        -ReliableAcknowledgementToggle `
        -Payload $newPayload
    Send-ExactUdpDatagram `
        -Client $probeClient `
        -Packet $clientNew `
        -Description "reliable client new command"
    $serverInfoDatagram = Receive-UdpDatagram `
        -Client $probeClient `
        -ServerProcess $serverProcess `
        -OutputCapture $outputCapture `
        -Deadline $deadline `
        -ExpectedRemoteEndpoint $serverEndpoint `
        -Description "reliable serverinfo packet"
    $serverInfoPacket = Read-SequencedDatagram `
        -Packet $serverInfoDatagram.Bytes `
        -Description "reliable serverinfo packet"
    Assert-SequencedHeader `
        -Decoded $serverInfoPacket `
        -ExpectedSequence 3 `
        -ExpectedAcknowledgement 2 `
        -ExpectedReliableToggle $true `
        -ExpectedReliableAcknowledgementToggle $true `
        -Description "reliable serverinfo packet"
    $decodedServerInfo = Read-ServerInfoPayload -Payload $serverInfoPacket.Payload
    Assert-ServerInfoRuntimeExpectations `
        -ServerInfo $decodedServerInfo `
        -ExpectedClientDllMd5 $expectedClientDllMd5

    if (-not $ExerciseServerInfoRetransmitAndInvalidCommands) {
        [byte[]]$serverInfoAck = New-SequencedDatagram `
            -Sequence 3 `
            -Acknowledgement 3 `
            -Payload $nopPayload
        Send-ExactUdpDatagram `
            -Client $probeClient `
            -Packet $serverInfoAck `
            -Description "serverinfo reliable acknowledgement"
        [void](Receive-AndAssertNopAck `
            -Client $probeClient `
            -ServerProcess $serverProcess `
            -OutputCapture $outputCapture `
            -Deadline $deadline `
            -ServerEndpoint $serverEndpoint `
            -ExpectedServerSequence 4 `
            -ExpectedClientSequence 3 `
            -ExpectedServerReliableToggle $false `
            -ExpectedClientReliableToggle $true `
            -Description "final serverinfo acknowledgement response")
    }
    else {
        [byte[]]$wrongAckAtCarrier = New-SequencedDatagram `
            -Sequence 3 `
            -Acknowledgement 3 `
            -ReliableAcknowledgementToggle `
            -Payload $nopPayload
        Send-ExactUdpDatagram `
            -Client $probeClient `
            -Packet $wrongAckAtCarrier `
            -Description "wrong serverinfo reliable acknowledgement"
        [void](Receive-AndAssertNopAck `
            -Client $probeClient `
            -ServerProcess $serverProcess `
            -OutputCapture $outputCapture `
            -Deadline $deadline `
            -ServerEndpoint $serverEndpoint `
            -ExpectedServerSequence 4 `
            -ExpectedClientSequence 3 `
            -ExpectedServerReliableToggle $false `
            -ExpectedClientReliableToggle $true `
            -Description "ordinary packet before serverinfo resend")

        [byte[]]$wrongAckAfterAdvance = New-SequencedDatagram `
            -Sequence 4 `
            -Acknowledgement 4 `
            -ReliableAcknowledgementToggle `
            -Payload $nopPayload
        Send-ExactUdpDatagram `
            -Client $probeClient `
            -Packet $wrongAckAfterAdvance `
            -Description "wrong acknowledgement after ordinary advance"
        $resendDatagram = Receive-UdpDatagram `
            -Client $probeClient `
            -ServerProcess $serverProcess `
            -OutputCapture $outputCapture `
            -Deadline $deadline `
            -ExpectedRemoteEndpoint $serverEndpoint `
            -Description "serverinfo retransmission"
        $resendPacket = Read-SequencedDatagram `
            -Packet $resendDatagram.Bytes `
            -Description "serverinfo retransmission"
        Assert-SequencedHeader `
            -Decoded $resendPacket `
            -ExpectedSequence 5 `
            -ExpectedAcknowledgement 4 `
            -ExpectedReliableToggle $true `
            -ExpectedReliableAcknowledgementToggle $true `
            -Description "serverinfo retransmission"
        Assert-ExactBytes `
            -Actual $resendPacket.Payload `
            -Expected $serverInfoPacket.Payload `
            -Description "decoded serverinfo retransmission"
        $decodedResend = Read-ServerInfoPayload -Payload $resendPacket.Payload
        Assert-ServerInfoRuntimeExpectations `
            -ServerInfo $decodedResend `
            -ExpectedClientDllMd5 $expectedClientDllMd5

        [void](Update-ProcessOutputCapture -State $outputCapture)
        $beforeCorrectAck = Get-SharedFileText -Path $stdoutPath
        if ($beforeCorrectAck.IndexOf(
                "goldsrc_serverinfo_acknowledged:",
                [StringComparison]::Ordinal) -ge 0) {
            throw "serverinfo advanced before the correct reliable acknowledgement"
        }

        [byte[]]$futureAck = New-SequencedDatagram `
            -Sequence 5 `
            -Acknowledgement 100 `
            -Payload $nopPayload
        Send-ExactUdpDatagram `
            -Client $probeClient `
            -Packet $futureAck `
            -Description "future serverinfo acknowledgement"
        Wait-ForStdoutToken `
            -Process $serverProcess `
            -OutputCapture $outputCapture `
            -StdoutPath $stdoutPath `
            -Deadline $deadline `
            -Token "reason=future_ack" `
            -Description "future acknowledgement rejection"
        Assert-NoUdpDatagram `
            -Client $probeClient `
            -ServerProcess $serverProcess `
            -OutputCapture $outputCapture `
            -DurationMilliseconds 150 `
            -Description "future acknowledgement no-response check"

        [byte[]]$correctAck = New-SequencedDatagram `
            -Sequence 5 `
            -Acknowledgement 5 `
            -Payload $nopPayload
        Send-ExactUdpDatagram `
            -Client $probeClient `
            -Packet $correctAck `
            -Description "correct serverinfo acknowledgement"
        [void](Receive-AndAssertNopAck `
            -Client $probeClient `
            -ServerProcess $serverProcess `
            -OutputCapture $outputCapture `
            -Deadline $deadline `
            -ServerEndpoint $serverEndpoint `
            -ExpectedServerSequence 6 `
            -ExpectedClientSequence 5 `
            -ExpectedServerReliableToggle $false `
            -ExpectedClientReliableToggle $true `
            -Description "serverinfo acknowledged response")

        [byte[]]$duplicateNew = New-SequencedDatagram `
            -Sequence 6 `
            -Acknowledgement 6 `
            -ReliableToggle `
            -Payload $newPayload
        Send-ExactUdpDatagram `
            -Client $probeClient `
            -Packet $duplicateNew `
            -Description "duplicate reliable new command"
        [void](Receive-AndAssertNopAck `
            -Client $probeClient `
            -ServerProcess $serverProcess `
            -OutputCapture $outputCapture `
            -Deadline $deadline `
            -ServerEndpoint $serverEndpoint `
            -ExpectedServerSequence 7 `
            -ExpectedClientSequence 6 `
            -ExpectedServerReliableToggle $false `
            -ExpectedClientReliableToggle $false `
            -Description "duplicate new suppression response")
        Wait-ForStdoutToken `
            -Process $serverProcess `
            -OutputCapture $outputCapture `
            -StdoutPath $stdoutPath `
            -Deadline $deadline `
            -Token "goldsrc_client_new_suppressed:" `
            -Description "duplicate new suppression diagnostic"

        Send-ExactUdpDatagram `
            -Client $probeClient `
            -Packet $duplicateNew `
            -Description "duplicate outer sequence"
        Wait-ForStdoutToken `
            -Process $serverProcess `
            -OutputCapture $outputCapture `
            -StdoutPath $stdoutPath `
            -Deadline $deadline `
            -Token "reason=duplicate_sequence" `
            -Description "duplicate outer-sequence rejection"
        Assert-NoUdpDatagram `
            -Client $probeClient `
            -ServerProcess $serverProcess `
            -OutputCapture $outputCapture `
            -DurationMilliseconds 150 `
            -Description "duplicate outer-sequence no-response check"

        $invalidPayloads = New-Object 'System.Collections.Generic.List[byte[]]'
        $invalidPayloads.Add([byte[]]@($clcStringCmd))
        $invalidPayloads.Add([byte[]]@($clcStringCmd, 0x6E, 0x65, 0x77))
        $oversizedCommand = New-Object byte[] 67
        $oversizedCommand[0] = $clcStringCmd
        for ($index = 1; $index -le 65; $index++) {
            $oversizedCommand[$index] = [byte][char]'a'
        }
        $oversizedCommand[66] = 0
        $invalidPayloads.Add($oversizedCommand)
        $invalidPayloads.Add((New-StringCommandPayload -Command "new extra"))
        $invalidPayloads.Add((New-StringCommandPayload -Command "new;quit"))
        $invalidPayloads.Add((New-StringCommandPayload -Command "status"))
        $invalidPayloads.Add([byte[]]@(0x7F))

        [uint32]$nextClientSequence = 7
        [uint32]$latestServerSequence = 7
        $expectedClientReliableToggle = $false
        foreach ($invalidPayload in $invalidPayloads) {
            $expectedClientReliableToggle = -not $expectedClientReliableToggle
            [byte[]]$invalidPacket = New-SequencedDatagram `
                -Sequence $nextClientSequence `
                -Acknowledgement $latestServerSequence `
                -ReliableToggle `
                -Payload $invalidPayload
            Send-ExactUdpDatagram `
                -Client $probeClient `
                -Packet $invalidPacket `
                -Description "bounded invalid client application packet"
            $latestServerSequence = Get-NextNetchanSequence -Sequence $latestServerSequence
            [void](Receive-AndAssertNopAck `
                -Client $probeClient `
                -ServerProcess $serverProcess `
                -OutputCapture $outputCapture `
                -Deadline $deadline `
                -ServerEndpoint $serverEndpoint `
                -ExpectedServerSequence $latestServerSequence `
                -ExpectedClientSequence $nextClientSequence `
                -ExpectedServerReliableToggle $false `
                -ExpectedClientReliableToggle $expectedClientReliableToggle `
                -Description "rejected client application response")
            $nextClientSequence = Get-NextNetchanSequence -Sequence $nextClientSequence
        }

        $hijackClient = New-Object System.Net.Sockets.UdpClient(
            [System.Net.Sockets.AddressFamily]::InterNetwork
        )
        $hijackClient.Client.Bind((New-Object System.Net.IPEndPoint([System.Net.IPAddress]::Loopback, 0)))
        $hijackPort = ([System.Net.IPEndPoint]$hijackClient.Client.LocalEndPoint).Port
        if ($hijackPort -eq $clientPort) {
            throw "second UDP socket unexpectedly reused the primary endpoint"
        }
        $hijackClient.Connect($serverEndpoint)
        [byte[]]$finalClientPacket = New-SequencedDatagram `
            -Sequence $nextClientSequence `
            -Acknowledgement $latestServerSequence `
            -Payload $nopPayload
        Send-ExactUdpDatagram `
            -Client $hijackClient `
            -Packet $finalClientPacket `
            -Description "second-endpoint hijack attempt"
        Wait-ForStdoutToken `
            -Process $serverProcess `
            -OutputCapture $outputCapture `
            -StdoutPath $stdoutPath `
            -Deadline $deadline `
            -Token ("goldsrc_netchan_unknown_endpoint: endpoint=127.0.0.1:{0}" -f $hijackPort) `
            -Description "second-endpoint rejection"
        Assert-NoUdpDatagram `
            -Client $hijackClient `
            -ServerProcess $serverProcess `
            -OutputCapture $outputCapture `
            -DurationMilliseconds 200 `
            -Description "second-endpoint no-amplification check"

        Send-ExactUdpDatagram `
            -Client $probeClient `
            -Packet $finalClientPacket `
            -Description "valid primary endpoint packet after rejections"
        $finalServerSequence = Get-NextNetchanSequence -Sequence $latestServerSequence
        [void](Receive-AndAssertNopAck `
            -Client $probeClient `
            -ServerProcess $serverProcess `
            -OutputCapture $outputCapture `
            -Deadline $deadline `
            -ServerEndpoint $serverEndpoint `
            -ExpectedServerSequence $finalServerSequence `
            -ExpectedClientSequence $nextClientSequence `
            -ExpectedServerReliableToggle $false `
            -ExpectedClientReliableToggle $expectedClientReliableToggle `
            -Description "final responsive server packet")
    }

    Wait-ForCleanServerExit `
        -Process $serverProcess `
        -OutputCapture $outputCapture `
        -Deadline $deadline
    if ($serverProcess.ExitCode -ne 0) {
        throw ("hlhost exited with code {0}" -f $serverProcess.ExitCode)
    }
    if (Test-ProcessIdAlive -ProcessId $serverProcessId) {
        throw ("hlhost process ID {0} is still alive after clean exit" -f $serverProcessId)
    }
    $childProcessProof = Assert-NoDirectChildProcessLeak -ParentProcessId $serverProcessId
    $capturedStdout = Get-SharedFileText -Path $stdoutPath
    $capturedStderr = Get-SharedFileText -Path $stderrPath
    Assert-SessionSummary `
        -Stdout $capturedStdout `
        -ExpectedEndpoint ("127.0.0.1:{0}" -f $clientPort) `
        -ExpectedChallenge $challengeValue `
        -ExpectedChannelIdentifier $clientPort
    $expectedDatagrams = if ($ExerciseServerInfoRetransmitAndInvalidCommands) { 19 } else { 5 }
    Assert-UdpSummary -Stdout $capturedStdout -ExpectedDatagrams $expectedDatagrams
    Assert-NetchanSummaryCore -Stdout $capturedStdout
    [void](Assert-ServerInfoSummary `
        -Stdout $capturedStdout `
        -DecodedServerInfo $decodedServerInfo `
        -ExpectedClientDllMd5 $expectedClientDllMd5 `
        -NegativeProof ([bool]$ExerciseServerInfoRetransmitAndInvalidCommands))
    Assert-NoRepositoryFileMutation `
        -RepositoryRoot $repoRoot `
        -Before $repositorySnapshotBefore
}
catch {
    $failure = $_
}
finally {
    if ($null -ne $hijackClient) {
        $hijackClient.Close()
        $hijackClient.Dispose()
    }
    if ($null -ne $probeClient) {
        $probeClient.Close()
        $probeClient.Dispose()
    }
    if ($processStarted -and $null -ne $serverProcess) {
        try {
            if (-not $serverProcess.HasExited) {
                if ($serverProcessId -gt 0) {
                    Stop-Process -Id $serverProcessId -Force -ErrorAction Stop
                }
                else {
                    $serverProcess.Kill()
                }
                [void]$serverProcess.WaitForExit(2000)
            }
            if (-not $serverProcess.HasExited) {
                $cleanupFailure = "cleanup_failed"
            }
        }
        catch {
            $cleanupFailure = "cleanup_failed"
        }
    }
    if ($null -ne $outputCapture) {
        try {
            Complete-ProcessOutputCapture -State $outputCapture
        }
        catch {
            if ($null -eq $cleanupFailure) {
                $cleanupFailure = "cleanup_failed"
            }
        }
    }
    if ($processStarted -and $serverProcessId -gt 0 -and
        (Test-ProcessIdAlive -ProcessId $serverProcessId)) {
        $cleanupFailure = "cleanup_failed"
    }
    $latestStdout = Get-SharedFileText -Path $stdoutPath
    $latestStderr = Get-SharedFileText -Path $stderrPath
    if ($latestStdout.Length -gt 0) {
        $capturedStdout = $latestStdout
    }
    if ($latestStderr.Length -gt 0) {
        $capturedStderr = $latestStderr
    }
    foreach ($tempPath in @($stdoutPath, $stderrPath)) {
        if (Test-Path -LiteralPath $tempPath -PathType Leaf) {
            Remove-Item -LiteralPath $tempPath -Force -ErrorAction SilentlyContinue
        }
        if (Test-Path -LiteralPath $tempPath -PathType Leaf) {
            $cleanupFailure = "cleanup_failed"
        }
    }
}

if ($null -ne $cleanupFailure -and $null -eq $failure) {
    $failure = New-Object System.Management.Automation.ErrorRecord(
        (New-Object System.InvalidOperationException($cleanupFailure)),
        "GoldsrcServerInfoProbeCleanupFailure",
        [System.Management.Automation.ErrorCategory]::CloseError,
        $serverProcess
    )
}
if ($null -ne $failure) {
    $failureReason = if ($cleanupFailure -ceq "cleanup_failed") {
        "cleanup_failed"
    } else {
        "proof_gate_failed"
    }
    Write-Host ("goldsrc_serverinfo_probe: result=fail reason={0}" -f
        $failureReason)
    exit 1
}

Write-Host ("goldsrc_serverinfo_probe: port={0},client_endpoint=127.0.0.1:{1}" -f
    $selectedPort,
    $clientPort)
Write-Host ("goldsrc_serverinfo_probe: udp_owner={0},child_process_check={1},repo_files_mutated=0,temp_files_cleaned=1" -f
    $portOwnershipProof,
    $childProcessProof)
if ($ExerciseServerInfoRetransmitAndInvalidCommands) {
    Write-Host "goldsrc_serverinfo_proof_b: serverinfo_retransmitted=true,retransmitted_payload_identical=true,wrong_reliable_ack_rejected=true,future_ack_rejected=true,duplicate_client_reliable_suppressed=true,duplicate_outer_sequence_rejected=true,malformed_client_message_rejected=true,unsupported_command_rejected=true,command_injection_rejected=true,endpoint_hijack_rejected=true,client_new_deliveries=1,serverinfo_generations=1,session_count=1,put_in_server=0,spawned=0,active=0,server_still_responsive=true,clean_shutdown=1,proof_b=pass"
}
else {
    Write-Host ("goldsrc_serverinfo_proof_a: transport=udp,external_datagrams=true,protocol=48,connectionless_handshake=pass,netchan_established=true,client_new_reliable=true,client_new_delivered=1,client_new_reliable_acked=true,serverinfo_received=true,serverinfo_decode=pass,serverinfo_protocol=48,serverinfo_player_index={0},serverinfo_map={1},serverinfo_game_dir=valve,serverinfo_reliable=true,serverinfo_acked=true,signon_phase=serverinfo_acknowledged,session_count=1,put_in_server=0,spawned=0,active=0,clean_shutdown=1,proof_a=pass" -f
        $decodedServerInfo.PlayerIndex,
        $decodedServerInfo.Map)
}
Write-Host "goldsrc_serverinfo_probe: result=pass"
