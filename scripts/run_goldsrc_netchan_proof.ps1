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

    [switch]$ExerciseReliableRetransmit,

    [switch]$SkipServerOutput
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
if (Test-Path Variable:PSNativeCommandUseErrorActionPreference) {
    $PSNativeCommandUseErrorActionPreference = $false
}

$reliableFlag = [uint32]2147483648
$fragmentFlag = [uint32]1073741824
$sequenceMask = [uint32]1073741823

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

function Get-StableUnsignedField {
    param(
        [string]$Line,
        [string]$Name
    )

    $pattern = '(?:^|[,\s])' + [regex]::Escape($Name) + '=(?<value>[0-9]+)(?=$|[,\s])'
    $matches = [regex]::Matches(
        $Line,
        $pattern,
        [System.Text.RegularExpressions.RegexOptions]::CultureInvariant
    )
    if ($matches.Count -ne 1) {
        throw ("expected exactly one unsigned {0} field: {1}" -f $Name, $Line.Trim())
    }

    $value = [uint64]0
    if (-not [uint64]::TryParse(
        $matches[0].Groups["value"].Value,
        [Globalization.NumberStyles]::None,
        [Globalization.CultureInfo]::InvariantCulture,
        [ref]$value
    )) {
        throw ("{0} is outside uint64 range: {1}" -f $Name, $Line.Trim())
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
                # netstat remains a permission-independent fallback on Windows.
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
        $payload[$index] = 0x01
    }
    return ,$payload
}

function New-SequencedDatagram {
    param(
        [ValidateRange(0, 1073741823)]
        [uint32]$Sequence,
        [ValidateRange(0, 1073741823)]
        [uint32]$Acknowledgement,
        [switch]$ReliablePresent,
        [switch]$ReliableAcknowledgement,
        [switch]$FragmentPresent,
        [byte[]]$Payload = @()
    )

    $rawSequenceWide = [uint64]$Sequence
    if ($ReliablePresent) {
        $rawSequenceWide = $rawSequenceWide -bor [uint64]2147483648
    }
    if ($FragmentPresent) {
        $rawSequenceWide = $rawSequenceWide -bor [uint64]1073741824
    }

    $rawAckWide = [uint64]$Acknowledgement
    if ($ReliableAcknowledgement) {
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
        ReliablePresent = (([uint64]$rawSequence -band [uint64]$reliableFlag) -ne 0)
        ReliableAcknowledgement = (([uint64]$rawAck -band [uint64]$reliableFlag) -ne 0)
        FragmentPresent = (([uint64]$rawSequence -band [uint64]$fragmentFlag) -ne 0)
        Payload = $payload
    }
}

function Assert-SequencedDatagram {
    param(
        $Decoded,
        [uint32]$ExpectedSequence,
        [uint32]$ExpectedAcknowledgement,
        [bool]$ExpectedReliablePresent,
        [bool]$ExpectedReliableAcknowledgement,
        [string]$Description
    )

    if ($Decoded.Bytes.Length -ne 16) {
        throw ("{0} must be exactly 16 bytes, received {1}" -f
            $Description,
            $Decoded.Bytes.Length)
    }
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
    if ($Decoded.ReliablePresent -ne $ExpectedReliablePresent) {
        throw ("{0} reliable flag mismatch: expected {1}, received {2}" -f
            $Description,
            [int]$ExpectedReliablePresent,
            [int]$Decoded.ReliablePresent)
    }
    if ($Decoded.ReliableAcknowledgement -ne $ExpectedReliableAcknowledgement) {
        throw ("{0} reliable-ack flag mismatch: expected {1}, received {2}" -f
            $Description,
            [int]$ExpectedReliableAcknowledgement,
            [int]$Decoded.ReliableAcknowledgement)
    }
    if ($Decoded.FragmentPresent) {
        throw ("{0} unexpectedly carries the fragmentation flag" -f $Description)
    }
    [byte[]]$expectedPayload = New-NopPayload
    Assert-ExactBytes `
        -Actual $Decoded.Payload `
        -Expected $expectedPayload `
        -Description ("{0} decoded svc_nop payload" -f $Description)
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
            throw "timeout waiting for hlhost to exit after the netchan proof"
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

function Get-RepositoryLogFileSet {
    param([string]$RepositoryRoot)

    $set = New-Object 'System.Collections.Generic.HashSet[string]' ([StringComparer]::OrdinalIgnoreCase)
    $logFiles = @(
        Get-ChildItem -LiteralPath $RepositoryRoot -Recurse -File -Filter "*.log" `
            -ErrorAction SilentlyContinue
    )
    foreach ($logFile in $logFiles) {
        [void]$set.Add([System.IO.Path]::GetFullPath($logFile.FullName))
    }
    return ,$set
}

function Assert-NoNewRepositoryLogFiles {
    param(
        [string]$RepositoryRoot,
        [System.Collections.Generic.HashSet[string]]$Before
    )

    $after = Get-RepositoryLogFileSet -RepositoryRoot $RepositoryRoot
    $created = New-Object System.Collections.Generic.List[string]
    foreach ($path in $after) {
        if (-not $Before.Contains($path)) {
            $created.Add($path)
        }
    }
    if ($created.Count -gt 0) {
        throw ("hlhost created repository log files despite --log-to-file 0 and --log-summary-file 0: {0}" -f
            ($created -join ", "))
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

    $frames = Get-StableUnsignedField -Line $line -Name "frames"
    if ($frames -eq 0) {
        throw ("goldsrc_udp_summary expected a positive frames count: {0}" -f $line.Trim())
    }
}

function Assert-NetchanSummary {
    param(
        [string]$Stdout,
        [bool]$RetransmitMode
    )

    $summaryLines = @(
        $Stdout -split "`r?`n" |
            Where-Object { $_.IndexOf("goldsrc_netchan_summary:", [StringComparison]::Ordinal) -ge 0 }
    )
    if ($summaryLines.Count -ne 1) {
        throw ("stdout expected exactly one goldsrc_netchan_summary line, found {0}" -f
            $summaryLines.Count)
    }
    $line = $summaryLines[0]

    $expectedReceived = if ($RetransmitMode) { "9" } else { "1" }
    $expectedSent = if ($RetransmitMode) { "4" } else { "1" }
    $expectedResent = if ($RetransmitMode) { "1" } else { "0" }
    $expectedNegative = if ($RetransmitMode) { "1" } else { "0" }
    $expectedResponsive = if ($RetransmitMode) { "1" } else { "0" }
    $expectedFields = [ordered]@{
        enabled = "1"
        initialized = "1"
        netchan_state = "established"
        server_initial_sequence = "1"
        server_initial_ack = "0"
        reliable_present = "1"
        fragment_present = "0"
        payload_transform = "pass"
        first_payload = "svc_nop"
        reliable_queued = "1"
        reliable_sent = "1"
        reliable_resent = $expectedResent
        reliable_acked = "1"
        reliable_pending_bytes = "0"
        sequenced_received = $expectedReceived
        sequenced_sent = $expectedSent
        duplicate_rejected = $expectedNegative
        out_of_order_rejected = $expectedNegative
        malformed_rejected = $expectedNegative
        fragment_rejected = $expectedNegative
        future_ack_rejected = $expectedNegative
        endpoint_hijack_rejected = $expectedNegative
        duplicate_sessions = "0"
        partial_sessions = "0"
        server_still_responsive = $expectedResponsive
        session_count = "1"
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

    # The same summary intentionally reports both the channel phase and the
    # authoritative lifecycle state using stable state fields.
    if (-not (Test-StableField -Line $line -Name "state" -ExpectedValue "connected")) {
        throw ("goldsrc_netchan_summary expected authoritative state=connected: {0}" -f
            $line.Trim())
    }

    $ackMismatch = Get-StableUnsignedField -Line $line -Name "reliable_ack_mismatch"
    if ($RetransmitMode) {
        if ($ackMismatch -lt 1) {
            throw ("goldsrc_netchan_summary expected reliable_ack_mismatch>=1: {0}" -f
                $line.Trim())
        }
    } elseif ($ackMismatch -ne 0) {
        throw ("goldsrc_netchan_summary expected reliable_ack_mismatch=0: {0}" -f
            $line.Trim())
    }
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
$arguments = @(
    "--gamedir", $resolvedGameDir,
    "--dedicated",
    "--deathmatch", "1",
    "--maxclients", "1",
    "--frames", "1",
    "--log-to-file", "0",
    "--log-summary-file", "0",
    "--ip", $BindAddress,
    "--port", ([string]$selectedPort),
    "--goldsrc-handshake",
    "--goldsrc-netchan",
    "--goldsrc-handshake-timeout-ms", ([string]$handshakeTimeoutMilliseconds)
)
if ($ExerciseReliableRetransmit) {
    $arguments += "--goldsrc-netchan-negative-proof"
}
$argumentLine = (($arguments | ForEach-Object {
    ConvertTo-WindowsCommandLineArgument -Value ([string]$_)
}) -join " ")
$tempPrefix = "hlhost_goldsrc_netchan_{0}" -f ([Guid]::NewGuid().ToString("N"))
$stdoutPath = Join-Path ([System.IO.Path]::GetTempPath()) ($tempPrefix + ".stdout.log")
$stderrPath = Join-Path ([System.IO.Path]::GetTempPath()) ($tempPrefix + ".stderr.log")
$deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
$repositoryLogsBefore = Get-RepositoryLogFileSet -RepositoryRoot $repoRoot

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
$portOwnershipProof = ""
$childProcessProof = "not_checked"

[byte[]]$nopPayload = New-NopPayload
[byte[]]$goldenServerSequence1 = @(
    0x01, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00,
    0x5A, 0x19, 0x01, 0x00, 0x1A, 0x01, 0x11, 0x40
)
[byte[]]$goldenServerSequence2 = @(
    0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x59, 0x19, 0x01, 0x03, 0x19, 0x01, 0x11, 0x43
)
[byte[]]$goldenClientSequence1Ack2 = @(
    0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00,
    0x5A, 0x19, 0x01, 0x00, 0x1A, 0x01, 0x11, 0x40
)
[byte[]]$goldenServerSequence3 = @(
    0x03, 0x00, 0x00, 0x80, 0x01, 0x00, 0x00, 0x00,
    0x58, 0x19, 0x01, 0x02, 0x18, 0x01, 0x11, 0x42
)
[byte[]]$goldenClientSequence2Ack3 = @(
    0x02, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x80,
    0x59, 0x19, 0x01, 0x03, 0x19, 0x01, 0x11, 0x43
)

try {
    [byte[]]$generatedServerSequence1 = New-SequencedDatagram `
        -Sequence 1 `
        -Acknowledgement 0 `
        -ReliablePresent `
        -Payload $nopPayload
    Assert-ExactBytes `
        -Actual $generatedServerSequence1 `
        -Expected $goldenServerSequence1 `
        -Description "COM_Munge2 golden vector"

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
    $userInfo = '\name\netchan_probe\model\gordon'
    $connectLine = ('connect 48 {0} "{1}" "{2}"' -f
        $challengeValue,
        $protocolInfo,
        $userInfo) + "`n"
    [byte[]]$authTail = @(0x00, 0x01, 0x7F, 0x80, 0xFF)
    [byte[]]$connectRequest = New-ConnectionlessDatagram -Text $connectLine -Tail $authTail
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
    [byte[]]$acceptResponse = $acceptDatagram.Bytes
    [byte[]]$expectedAcceptResponse = New-ConnectionlessDatagram `
        -Text ('B 1 "127.0.0.1:{0}" 0 5971' -f $clientPort) `
        -Tail ([byte[]](0x00))
    Assert-ExactBytes `
        -Actual $acceptResponse `
        -Expected $expectedAcceptResponse `
        -Description "connect accept response"

    $serverOneDatagram = Receive-UdpDatagram `
        -Client $probeClient `
        -ServerProcess $serverProcess `
        -OutputCapture $outputCapture `
        -Deadline $deadline `
        -ExpectedRemoteEndpoint $serverEndpoint `
        -Description "initial reliable server netchan datagram"
    Assert-ExactBytes `
        -Actual $serverOneDatagram.Bytes `
        -Expected $goldenServerSequence1 `
        -Description "initial reliable server netchan datagram golden fixture"
    $serverOne = Read-SequencedDatagram `
        -Packet $serverOneDatagram.Bytes `
        -Description "initial reliable server netchan datagram"
    Assert-SequencedDatagram `
        -Decoded $serverOne `
        -ExpectedSequence 1 `
        -ExpectedAcknowledgement 0 `
        -ExpectedReliablePresent $true `
        -ExpectedReliableAcknowledgement $false `
        -Description "initial reliable server netchan datagram"

    if (-not $ExerciseReliableRetransmit) {
        [byte[]]$clientAck = New-SequencedDatagram `
            -Sequence 1 `
            -Acknowledgement 1 `
            -ReliableAcknowledgement `
            -Payload $nopPayload
        [byte[]]$expectedClientAck = @(
            0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x80,
            0x5A, 0x19, 0x01, 0x00, 0x1A, 0x01, 0x11, 0x40
        )
        Assert-ExactBytes `
            -Actual $clientAck `
            -Expected $expectedClientAck `
            -Description "normal client reliable acknowledgement golden fixture"
        Send-ExactUdpDatagram `
            -Client $probeClient `
            -Packet $clientAck `
            -Description "normal client reliable acknowledgement"
    } else {
        # S2 is an ordinary server transmit while S1 remains pending. Receiving
        # it, rather than sleeping, synchronizes the documented resend condition.
        $serverTwoDatagram = Receive-UdpDatagram `
            -Client $probeClient `
            -ServerProcess $serverProcess `
            -OutputCapture $outputCapture `
            -Deadline $deadline `
            -ExpectedRemoteEndpoint $serverEndpoint `
            -Description "ordinary server netchan datagram before reliable resend"
        Assert-ExactBytes `
            -Actual $serverTwoDatagram.Bytes `
            -Expected $goldenServerSequence2 `
            -Description "ordinary server netchan datagram golden fixture"
        $serverTwo = Read-SequencedDatagram `
            -Packet $serverTwoDatagram.Bytes `
            -Description "ordinary server netchan datagram before reliable resend"
        Assert-SequencedDatagram `
            -Decoded $serverTwo `
            -ExpectedSequence 2 `
            -ExpectedAcknowledgement 0 `
            -ExpectedReliablePresent $false `
            -ExpectedReliableAcknowledgement $false `
            -Description "ordinary server netchan datagram before reliable resend"

        [byte[]]$wrongReliableAck = New-SequencedDatagram `
            -Sequence 1 `
            -Acknowledgement 2 `
            -Payload $nopPayload
        Assert-ExactBytes `
            -Actual $wrongReliableAck `
            -Expected $goldenClientSequence1Ack2 `
            -Description "client wrong-toggle acknowledgement golden fixture"
        Send-ExactUdpDatagram `
            -Client $probeClient `
            -Packet $wrongReliableAck `
            -Description "client wrong-toggle acknowledgement"

        $serverThreeDatagram = Receive-UdpDatagram `
            -Client $probeClient `
            -ServerProcess $serverProcess `
            -OutputCapture $outputCapture `
            -Deadline $deadline `
            -ExpectedRemoteEndpoint $serverEndpoint `
            -Description "reliable server resend"
        Assert-ExactBytes `
            -Actual $serverThreeDatagram.Bytes `
            -Expected $goldenServerSequence3 `
            -Description "reliable server resend golden fixture"
        $serverThree = Read-SequencedDatagram `
            -Packet $serverThreeDatagram.Bytes `
            -Description "reliable server resend"
        Assert-SequencedDatagram `
            -Decoded $serverThree `
            -ExpectedSequence 3 `
            -ExpectedAcknowledgement 1 `
            -ExpectedReliablePresent $true `
            -ExpectedReliableAcknowledgement $false `
            -Description "reliable server resend"
        Assert-ExactBytes `
            -Actual $serverThree.Payload `
            -Expected $serverOne.Payload `
            -Description "decoded reliable resend payload identity"
        if ($serverThree.ReliablePresent -ne $serverOne.ReliablePresent) {
            throw "reliable resend changed the reliable sequence toggle"
        }

        [byte[]]$correctReliableAck = New-SequencedDatagram `
            -Sequence 2 `
            -Acknowledgement 3 `
            -ReliableAcknowledgement `
            -Payload $nopPayload
        Assert-ExactBytes `
            -Actual $correctReliableAck `
            -Expected $goldenClientSequence2Ack3 `
            -Description "client final reliable acknowledgement golden fixture"
        Send-ExactUdpDatagram `
            -Client $probeClient `
            -Packet $correctReliableAck `
            -Description "client final reliable acknowledgement"

        # Six negative categories must leave incoming sequence 2 intact.
        Send-ExactUdpDatagram `
            -Client $probeClient `
            -Packet $correctReliableAck `
            -Description "duplicate client sequence"
        Send-ExactUdpDatagram `
            -Client $probeClient `
            -Packet $wrongReliableAck `
            -Description "older out-of-order client sequence"
        [byte[]]$malformedPacket = @(0x03, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00)
        Send-ExactUdpDatagram `
            -Client $probeClient `
            -Packet $malformedPacket `
            -Description "malformed short client packet"
        [byte[]]$fragmentPacket = New-SequencedDatagram `
            -Sequence 3 `
            -Acknowledgement 3 `
            -ReliableAcknowledgement `
            -FragmentPresent `
            -Payload $nopPayload
        Send-ExactUdpDatagram `
            -Client $probeClient `
            -Packet $fragmentPacket `
            -Description "unsupported fragmented client packet"
        [byte[]]$futureAckPacket = New-SequencedDatagram `
            -Sequence 3 `
            -Acknowledgement 100 `
            -ReliableAcknowledgement `
            -Payload $nopPayload
        Send-ExactUdpDatagram `
            -Client $probeClient `
            -Packet $futureAckPacket `
            -Description "forged future acknowledgement"

        $hijackClient = New-Object System.Net.Sockets.UdpClient(
            [System.Net.Sockets.AddressFamily]::InterNetwork
        )
        $hijackClient.Client.Bind((New-Object System.Net.IPEndPoint([System.Net.IPAddress]::Loopback, 0)))
        $hijackPort = ([System.Net.IPEndPoint]$hijackClient.Client.LocalEndPoint).Port
        if ($hijackPort -eq $clientPort) {
            throw "second UDP socket unexpectedly reused the primary client endpoint"
        }
        $hijackClient.Connect($serverEndpoint)
        [byte[]]$validSequenceThree = New-SequencedDatagram `
            -Sequence 3 `
            -Acknowledgement 3 `
            -ReliableAcknowledgement `
            -Payload $nopPayload
        Send-ExactUdpDatagram `
            -Client $hijackClient `
            -Packet $validSequenceThree `
            -Description "second-endpoint session hijack attempt"
        Wait-ForStdoutToken `
            -Process $serverProcess `
            -OutputCapture $outputCapture `
            -StdoutPath $stdoutPath `
            -Deadline $deadline `
            -Token ("goldsrc_netchan_unknown_endpoint: endpoint=127.0.0.1:{0}" -f $hijackPort) `
            -Description "second-endpoint rejection diagnostic"
        Assert-NoUdpDatagram `
            -Client $hijackClient `
            -ServerProcess $serverProcess `
            -OutputCapture $outputCapture `
            -DurationMilliseconds 250 `
            -Description "second-endpoint no-amplification response"
        Send-ExactUdpDatagram `
            -Client $probeClient `
            -Packet $validSequenceThree `
            -Description "valid client packet after negative cases"

        $serverFourDatagram = Receive-UdpDatagram `
            -Client $probeClient `
            -ServerProcess $serverProcess `
            -OutputCapture $outputCapture `
            -Deadline $deadline `
            -ExpectedRemoteEndpoint $serverEndpoint `
            -Description "final ack-only server netchan response"
        $serverFour = Read-SequencedDatagram `
            -Packet $serverFourDatagram.Bytes `
            -Description "final ack-only server netchan response"
        Assert-SequencedDatagram `
            -Decoded $serverFour `
            -ExpectedSequence 4 `
            -ExpectedAcknowledgement 3 `
            -ExpectedReliablePresent $false `
            -ExpectedReliableAcknowledgement $false `
            -Description "final ack-only server netchan response"
        [byte[]]$expectedServerFour = New-SequencedDatagram `
            -Sequence 4 `
            -Acknowledgement 3 `
            -Payload $nopPayload
        Assert-ExactBytes `
            -Actual $serverFourDatagram.Bytes `
            -Expected $expectedServerFour `
            -Description "final ack-only server netchan response fixture"
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
    $expectedUdpDatagrams = if ($ExerciseReliableRetransmit) { 11 } else { 3 }
    Assert-UdpSummary -Stdout $capturedStdout -ExpectedDatagrams $expectedUdpDatagrams
    Assert-NetchanSummary `
        -Stdout $capturedStdout `
        -RetransmitMode ([bool]$ExerciseReliableRetransmit)
    Assert-NoNewRepositoryLogFiles `
        -RepositoryRoot $repoRoot `
        -Before $repositoryLogsBefore
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
                try {
                    if ($serverProcessId -gt 0) {
                        Stop-Process -Id $serverProcessId -Force -ErrorAction Stop
                    } else {
                        $serverProcess.Kill()
                    }
                }
                catch {
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
        if (Test-Path -LiteralPath $tempPath -PathType Leaf) {
            $tempCleanupFailure = ("temporary output file still exists after cleanup: {0}" -f $tempPath)
            if ($null -eq $cleanupFailure) {
                $cleanupFailure = $tempCleanupFailure
            } else {
                $cleanupFailure += "; $tempCleanupFailure"
            }
        }
    }
}

if ($null -ne $cleanupFailure -and $null -eq $failure) {
    $failure = New-Object System.Management.Automation.ErrorRecord(
        (New-Object System.InvalidOperationException($cleanupFailure)),
        "GoldsrcNetchanProbeCleanupFailure",
        [System.Management.Automation.ErrorCategory]::CloseError,
        $serverProcess
    )
}

if ($null -ne $failure) {
    $failureMessage = $failure.Exception.Message -replace '[\r\n]+', ' '
    Write-Host ("goldsrc_netchan_probe: result=fail reason={0}" -f $failureMessage)
    throw $failure
}

Write-Host ("goldsrc_netchan_probe: port={0},client_endpoint=127.0.0.1:{1}" -f
    $selectedPort,
    $clientPort)
Write-Host ("goldsrc_netchan_probe: udp_owner={0},child_process_check={1},repo_log_files_created=0,temp_files_cleaned=1" -f
    $portOwnershipProof,
    $childProcessProof)
if ($ExerciseReliableRetransmit) {
    Write-Host "goldsrc_netchan_proof_b: reliable_ack_mismatch_detected=true,reliable_resent=true,reliable_payload_identical=true,final_reliable_ack=true,duplicate_rejected=true,out_of_order_rejected=true,malformed_rejected=true,fragment_rejected=true,future_ack_rejected=true,endpoint_hijack_rejected=true,duplicate_sessions=0,partial_sessions=0,server_still_responsive=true,state=connected,put_in_server=0,spawned=0,active=0,clean_shutdown=1,proof_b=pass"
} else {
    Write-Host "goldsrc_netchan_proof_a: transport=udp,external_datagrams=true,protocol=48,connectionless_handshake=pass,netchan_initialized=true,server_initial_sequence=1,server_initial_ack=0,reliable_present=true,fragment_present=false,payload_transform=pass,first_payload=svc_nop,reliable_acked=true,reliable_pending_bytes=0,session_count=1,state=connected,netchan_state=established,put_in_server=0,spawned=0,active=0,clean_shutdown=1,proof_a=pass"
}
Write-Host "goldsrc_netchan_probe: result=pass"
