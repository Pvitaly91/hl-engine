[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$ExecutablePath,

    [Parameter(Mandatory = $true)]
    [string]$GameDir,

    [string]$ManifestFixture = "",

    [string]$BindAddress = "127.0.0.1",

    [ValidateRange(0, 65535)]
    [int]$Port = 0,

    [ValidateRange(1, 300)]
    [int]$TimeoutSeconds = 60,

    [switch]$NegativeProof,

    [switch]$ObservedContinuation,

    [switch]$SkipServerOutput
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
if (Test-Path Variable:PSNativeCommandUseErrorActionPreference) {
    $PSNativeCommandUseErrorActionPreference = $false
}
if ([string]::IsNullOrWhiteSpace($ManifestFixture)) {
    $ManifestFixture = Join-Path `
        $PSScriptRoot `
        "../src/tests/fixtures/goldsrc_resource_manifest_fragmented.tsv"
}

function Get-TopLevelProofFunctionDefinitions {
    param([string]$Path)

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw ("proof helper not found: {0}" -f $Path)
    }
    $tokens = $null
    $errors = $null
    $ast = [System.Management.Automation.Language.Parser]::ParseFile(
        $Path,
        [ref]$tokens,
        [ref]$errors
    )
    if (@($errors).Count -ne 0) {
        throw ("proof helper has {0} parser errors: {1}" -f @($errors).Count, $Path)
    }
    $definitions = @(
        $ast.EndBlock.Statements |
            Where-Object {
                $_ -is [System.Management.Automation.Language.FunctionDefinitionAst]
            }
    )
    if ($definitions.Count -eq 0) {
        throw ("proof helper contains no reusable functions: {0}" -f $Path)
    }
    return @($definitions | ForEach-Object { $_.Extent.Text })
}

foreach ($definitionText in @(
    Get-TopLevelProofFunctionDefinitions `
        -Path (Join-Path $PSScriptRoot "run_goldsrc_serverinfo_proof.ps1")
)) {
    . ([scriptblock]::Create($definitionText))
}
foreach ($definitionText in @(
    Get-TopLevelProofFunctionDefinitions `
        -Path (Join-Path $PSScriptRoot "run_goldsrc_resource_manifest_proof.ps1")
)) {
    . ([scriptblock]::Create($definitionText))
}

$reliableToggleFlag = [uint32]2147483648
$fragmentFlag = [uint32]1073741824
$sequenceMask = [uint32]1073741823
$svcNop = [byte]0x01
$clcStringCmd = [byte]0x03
$svcServerInfo = [byte]0x0B
$svcSendExtraInfo = [byte]0x36
$svcResourceRequest = [byte]45
$svcResourceList = [byte]43
$maximumManifestBytes = 65536
$maximumResourceCount = 1280
$maximumResourcePathBytes = 63
$maximumResourceIndex = 4095
$maximumResourceDownloadBytes = 16777215
$maximumResourceFlags = 7
$resourceCustomFlag = 4
$maximumRouteableBytes = 1400
$maximumFragmentBytes = 1024
$maximumFragmentCount = 64
$normalFragmentMetadataBytes = 10

function Read-LittleEndianUInt16 {
    param(
        [byte[]]$Bytes,
        [int]$Offset
    )

    if ($Offset -lt 0 -or $Bytes.Length - $Offset -lt 2) {
        throw "uint16 read exceeds packet bounds"
    }
    return [uint16](
        [uint32]$Bytes[$Offset] -bor
        ([uint32]$Bytes[$Offset + 1] -shl 8)
    )
}

function Read-FragmentedSequencedDatagram {
    param(
        [byte[]]$Packet,
        [string]$Description
    )

    if ($Packet.Length -gt $maximumRouteableBytes) {
        throw ("{0} exceeds the routeable datagram limit" -f $Description)
    }
    $decoded = Read-SequencedDatagram -Packet $Packet -Description $Description
    if (-not $decoded.FragmentPresent -or -not $decoded.ReliableToggle) {
        throw ("{0} does not carry a reliable normal fragment" -f $Description)
    }
    if ($decoded.Payload.Length -lt $normalFragmentMetadataBytes) {
        throw ("{0} has truncated fragment metadata" -f $Description)
    }
    if ($decoded.Payload[0] -ne 1 -or $decoded.Payload[9] -ne 0) {
        throw ("{0} has an unsupported fragment stream layout" -f $Description)
    }

    $rawFragmentId = Read-LittleEndianUInt32 -Bytes $decoded.Payload -Offset 1
    $fragmentIndex = [uint16](([uint64]$rawFragmentId -shr 16) -band 65535)
    $fragmentCount = [uint16]([uint64]$rawFragmentId -band 65535)
    $payloadOffset = Read-LittleEndianUInt16 -Bytes $decoded.Payload -Offset 5
    $payloadLength = Read-LittleEndianUInt16 -Bytes $decoded.Payload -Offset 7
    if ($fragmentIndex -eq 0 -or
        $fragmentCount -eq 0 -or
        $fragmentIndex -gt $fragmentCount -or
        $fragmentCount -gt $maximumFragmentCount) {
        throw ("{0} has an invalid fragment index/count" -f $Description)
    }
    if ($payloadLength -eq 0 -or $payloadLength -gt $maximumFragmentBytes) {
        throw ("{0} has an invalid fragment length" -f $Description)
    }

    $applicationBytes = New-Object byte[] ($decoded.Payload.Length - $normalFragmentMetadataBytes)
    if ($applicationBytes.Length -gt 0) {
        [Array]::Copy(
            $decoded.Payload,
            $normalFragmentMetadataBytes,
            $applicationBytes,
            0,
            $applicationBytes.Length
        )
    }
    $rangeEnd = [uint64]$payloadOffset + [uint64]$payloadLength
    if ($rangeEnd -gt [uint64]$applicationBytes.Length) {
        throw ("{0} fragment range exceeds its decoded packet payload" -f $Description)
    }
    if ($payloadOffset -ne 0 -or $rangeEnd -ne [uint64]$applicationBytes.Length) {
        throw ("{0} contains unverified fragment padding or a nonzero normal-stream offset" -f $Description)
    }
    $fragmentBytes = New-Object byte[] $payloadLength
    [Array]::Copy(
        $applicationBytes,
        [int]$payloadOffset,
        $fragmentBytes,
        0,
        [int]$payloadLength
    )

    return [pscustomobject]@{
        Datagram = $decoded
        RawFragmentId = [uint32]$rawFragmentId
        FragmentIndex = [int]$fragmentIndex
        FragmentCount = [int]$fragmentCount
        PayloadOffset = [int]$payloadOffset
        PayloadLength = [int]$payloadLength
        FragmentBytes = $fragmentBytes
    }
}

function Add-ProbeFragment {
    param(
        [object[]]$Slots,
        $Fragment,
        [string]$Description
    )

    $slotIndex = $Fragment.FragmentIndex - 1
    if ($slotIndex -lt 0 -or $slotIndex -ge $Slots.Length) {
        throw ("{0} index is outside the bounded reassembly slots" -f $Description)
    }
    if ($null -eq $Slots[$slotIndex]) {
        $copy = New-Object byte[] $Fragment.FragmentBytes.Length
        [Array]::Copy($Fragment.FragmentBytes, 0, $copy, 0, $copy.Length)
        $Slots[$slotIndex] = $copy
        return "accepted"
    }
    Assert-ExactBytes `
        -Actual ([byte[]]$Fragment.FragmentBytes) `
        -Expected ([byte[]]$Slots[$slotIndex]) `
        -Description ($Description + " exact duplicate")
    return "duplicate"
}

function Join-ProbeFragments {
    param([object[]]$Slots)

    $total = [uint64]0
    foreach ($slot in $Slots) {
        if ($null -eq $slot) {
            throw "fragment reassembly is incomplete"
        }
        $total += [uint64]$slot.Length
        if ($total -gt [uint64]$maximumManifestBytes) {
            throw "fragment reassembly exceeds the bounded transfer limit"
        }
    }
    $result = New-Object byte[] ([int]$total)
    $offset = 0
    foreach ($slot in $Slots) {
        [Array]::Copy([byte[]]$slot, 0, $result, $offset, $slot.Length)
        $offset += $slot.Length
    }
    return ,$result
}

function Receive-ProofDatagram {
    param(
        [System.Net.Sockets.UdpClient]$Client,
        [System.Diagnostics.Process]$ServerProcess,
        $OutputCapture,
        [DateTime]$Deadline,
        [System.Net.IPEndPoint]$ServerEndpoint,
        [string]$Description
    )

    return Receive-UdpDatagram `
        -Client $Client `
        -ServerProcess $ServerProcess `
        -OutputCapture $OutputCapture `
        -Deadline $Deadline `
        -ExpectedRemoteEndpoint $ServerEndpoint `
        -Description $Description
}

function Send-FragmentAcknowledgement {
    param(
        [System.Net.Sockets.UdpClient]$Client,
        [uint32]$ClientSequence,
        [uint32]$ServerSequence,
        [bool]$ReliableAcknowledgementToggle,
        [byte[]]$NopPayload,
        [string]$Description
    )

    $arguments = @{
        Sequence = $ClientSequence
        Acknowledgement = $ServerSequence
        Payload = $NopPayload
    }
    if ($ReliableAcknowledgementToggle) {
        $arguments.ReliableAcknowledgementToggle = $true
    }
    $packet = New-SequencedDatagram @arguments
    Send-ExactUdpDatagram -Client $Client -Packet $packet -Description $Description
}

function Invoke-ResourceManifestExchange {
    param(
        [ValidateSet("Positive", "Negative", "Oversized")]
        [string]$Mode,
        [System.Net.Sockets.UdpClient]$Client,
        [System.Diagnostics.Process]$ServerProcess,
        $OutputCapture,
        [string]$StdoutPath,
        [DateTime]$Deadline,
        [System.Net.IPEndPoint]$ServerEndpoint,
        $Handshake,
        [object[]]$ExpectedEntries,
        [switch]$ObservedContinuation
    )

    $sendResourcesPayload = New-StringCommandPayload -Command "sendres"
    if ($ObservedContinuation) {
        $closeMenusPayload =
            New-StringCommandPayload -Command "closemenus `n"
        $sendResourcesPayload = [byte[]](
            $sendResourcesPayload +
            $closeMenusPayload +
            $closeMenusPayload
        )
    }
    $sendResourcesPacket = New-SequencedDatagram `
        -Sequence 4 `
        -Acknowledgement 4 `
        -ReliableToggle `
        -Payload $sendResourcesPayload
    Send-ExactUdpDatagram `
        -Client $Client `
        -Packet $sendResourcesPacket `
        -Description "reliable fragmented-manifest sendres request"

    if ($Mode -eq "Oversized") {
        $received = Receive-ProofDatagram `
            -Client $Client `
            -ServerProcess $ServerProcess `
            -OutputCapture $OutputCapture `
            -Deadline $Deadline `
            -ServerEndpoint $ServerEndpoint `
            -Description "oversized fragmented-manifest response"
        $ordinary = Read-SequencedDatagram `
            -Packet $received.Bytes `
            -Description "oversized fragmented-manifest response"
        Assert-SequencedHeader `
            -Decoded $ordinary `
            -ExpectedSequence 5 `
            -ExpectedAcknowledgement 4 `
            -ExpectedReliableToggle $false `
            -ExpectedReliableAcknowledgementToggle $false `
            -Description "oversized fragmented-manifest response"
        Assert-NopPayload -Payload $ordinary.Payload -Description "oversized response payload"
        Wait-ForStdoutToken `
            -Process $ServerProcess `
            -OutputCapture $OutputCapture `
            -StdoutPath $StdoutPath `
            -Deadline $Deadline `
            -Token "payload_too_large=1" `
            -Description "typed payload_too_large diagnostic"

        $repeat = New-SequencedDatagram `
            -Sequence 5 `
            -Acknowledgement 5 `
            -ReliableToggle `
            -Payload $sendResourcesPayload
        Send-ExactUdpDatagram `
            -Client $Client `
            -Packet $repeat `
            -Description "cached oversized fragmented-manifest request"
        $received = Receive-ProofDatagram `
            -Client $Client `
            -ServerProcess $ServerProcess `
            -OutputCapture $OutputCapture `
            -Deadline $Deadline `
            -ServerEndpoint $ServerEndpoint `
            -Description "cached oversized response"
        $cached = Read-SequencedDatagram `
            -Packet $received.Bytes `
            -Description "cached oversized response"
        Assert-SequencedHeader `
            -Decoded $cached `
            -ExpectedSequence 6 `
            -ExpectedAcknowledgement 5 `
            -ExpectedReliableToggle $false `
            -ExpectedReliableAcknowledgementToggle $true `
            -Description "cached oversized response"
        Assert-NopPayload -Payload $cached.Payload -Description "cached oversized response payload"
        return [pscustomobject]@{
            Manifest = $null
            FragmentCount = 0
            FragmentSends = 0
            RetransmissionObserved = $false
            RetransmittedIdentical = $false
            DuplicateSuppressed = $false
            MissingFirst = $false
            MissingMiddle = $false
            MissingFinal = $false
            FutureAckRejected = $false
            WrongAckRejected = $false
            FinalManifestAck = $false
            ServerResponsive = $true
            ApplicationPayloadDeliveries = 0
        }
    }

    [uint32]$clientSequence = 4
    [uint32]$latestServerSequence = 4
    $nextDatagram = Receive-ProofDatagram `
        -Client $Client `
        -ServerProcess $ServerProcess `
        -OutputCapture $OutputCapture `
        -Deadline $Deadline `
        -ServerEndpoint $ServerEndpoint `
        -Description "first fragmented manifest packet"
    $slots = $null
    $fragmentCount = 0
    $fragmentSends = 0
    $selected = $null
    $retransmissionObserved = $false
    $retransmittedIdentical = $false
    $duplicateSuppressed = $false
    $futureAckRejected = $false
    $wrongAckRejected = $false
    $missingFirst = $false
    $missingMiddle = $false
    $missingFinal = $false
    $currentReliableToggle = $true

    while ($true) {
        $fragment = Read-FragmentedSequencedDatagram `
            -Packet $nextDatagram.Bytes `
            -Description "fragmented manifest packet"
        $fragmentSends++
        if ($fragment.Datagram.Sequence -ne ([uint32]($latestServerSequence + 1)) -or
            $fragment.Datagram.Acknowledgement -ne $clientSequence -or
            $fragment.Datagram.ReliableAcknowledgementToggle) {
            throw "fragmented manifest outer sequence/acknowledgement progression is invalid"
        }
        $latestServerSequence = $fragment.Datagram.Sequence
        if ($fragmentCount -eq 0) {
            $fragmentCount = $fragment.FragmentCount
            if ($fragmentCount -lt 3) {
                throw ("fragmented fixture produced only {0} fragments; at least three are required" -f $fragmentCount)
            }
            $slots = New-Object object[] $fragmentCount
            $middle = [int][Math]::Ceiling($fragmentCount / 2.0)
            $selected = New-Object 'System.Collections.Generic.HashSet[int]'
            [void]$selected.Add(1)
            [void]$selected.Add($middle)
            [void]$selected.Add($fragmentCount)
        } elseif ($fragment.FragmentCount -ne $fragmentCount) {
            throw "fragment count changed during one session-owned transfer"
        }

        $isSelected = $Mode -eq "Negative" -and
            $selected.Contains($fragment.FragmentIndex)
        if (-not $isSelected) {
            if ((Add-ProbeFragment -Slots $slots -Fragment $fragment -Description "manifest fragment") -cne "accepted") {
                throw "an initial fragment was unexpectedly classified as duplicate"
            }
        } else {
            if ($fragment.FragmentIndex -eq 1) {
                $missingFirst = $true
            }
            if ($fragment.FragmentIndex -eq [int][Math]::Ceiling($fragmentCount / 2.0)) {
                $missingMiddle = $true
            }
            if ($fragment.FragmentIndex -eq $fragmentCount) {
                $missingFinal = $true
            }

            if (-not $futureAckRejected) {
                $futureAck = New-SequencedDatagram `
                    -Sequence ([uint32]($clientSequence + 1)) `
                    -Acknowledgement ([uint32]($latestServerSequence + 100)) `
                    -Payload $Handshake.NopPayload
                Send-ExactUdpDatagram `
                    -Client $Client `
                    -Packet $futureAck `
                    -Description "future fragment acknowledgement"
                Assert-NoUdpDatagram `
                    -Client $Client `
                    -ServerProcess $ServerProcess `
                    -OutputCapture $OutputCapture `
                    -DurationMilliseconds 100 `
                    -Description "future fragment acknowledgement rejection"
                $futureAckRejected = $true
            }

            $wrongToggle = -not $currentReliableToggle
            $clientSequence = [uint32]($clientSequence + 1)
            Send-FragmentAcknowledgement `
                -Client $Client `
                -ClientSequence $clientSequence `
                -ServerSequence $latestServerSequence `
                -ReliableAcknowledgementToggle $wrongToggle `
                -NopPayload $Handshake.NopPayload `
                -Description "wrong reliable fragment acknowledgement"
            $wrongAckRejected = $true
            $ordinaryDatagram = Receive-ProofDatagram `
                -Client $Client `
                -ServerProcess $ServerProcess `
                -OutputCapture $OutputCapture `
                -Deadline $Deadline `
                -ServerEndpoint $ServerEndpoint `
                -Description "ordinary response before fragment retransmission"
            $ordinary = Read-SequencedDatagram `
                -Packet $ordinaryDatagram.Bytes `
                -Description "ordinary response before fragment retransmission"
            Assert-SequencedHeader `
                -Decoded $ordinary `
                -ExpectedSequence ([uint32]($latestServerSequence + 1)) `
                -ExpectedAcknowledgement $clientSequence `
                -ExpectedReliableToggle $false `
                -ExpectedReliableAcknowledgementToggle $false `
                -Description "ordinary response before fragment retransmission"
            Assert-NopPayload -Payload $ordinary.Payload -Description "ordinary retransmission trigger payload"
            $latestServerSequence = $ordinary.Sequence

            $clientSequence = [uint32]($clientSequence + 1)
            Send-FragmentAcknowledgement `
                -Client $Client `
                -ClientSequence $clientSequence `
                -ServerSequence $latestServerSequence `
                -ReliableAcknowledgementToggle $wrongToggle `
                -NopPayload $Handshake.NopPayload `
                -Description "covering wrong reliable fragment acknowledgement"
            $resendDatagram = Receive-ProofDatagram `
                -Client $Client `
                -ServerProcess $ServerProcess `
                -OutputCapture $OutputCapture `
                -Deadline $Deadline `
                -ServerEndpoint $ServerEndpoint `
                -Description "fragment retransmission"
            $resend = Read-FragmentedSequencedDatagram `
                -Packet $resendDatagram.Bytes `
                -Description "fragment retransmission"
            $fragmentSends++
            if ($resend.Datagram.Sequence -ne ([uint32]($latestServerSequence + 1)) -or
                $resend.Datagram.Acknowledgement -ne $clientSequence -or
                $resend.RawFragmentId -ne $fragment.RawFragmentId) {
                throw "fragment retransmission changed outer progression or fragment identity"
            }
            Assert-ExactBytes `
                -Actual $resend.FragmentBytes `
                -Expected $fragment.FragmentBytes `
                -Description "decoded fragment retransmission"
            $latestServerSequence = $resend.Datagram.Sequence
            $retransmissionObserved = $true
            $retransmittedIdentical = $true
            if ((Add-ProbeFragment -Slots $slots -Fragment $resend -Description "retransmitted manifest fragment") -cne "accepted") {
                throw "the first retained retransmission was not accepted"
            }

            $middleIndex = [int][Math]::Ceiling($fragmentCount / 2.0)
            if ($fragment.FragmentIndex -eq $middleIndex) {
                $clientSequence = [uint32]($clientSequence + 1)
                Send-FragmentAcknowledgement `
                    -Client $Client `
                    -ClientSequence $clientSequence `
                    -ServerSequence $latestServerSequence `
                    -ReliableAcknowledgementToggle $wrongToggle `
                    -NopPayload $Handshake.NopPayload `
                    -Description "duplicate-fragment wrong acknowledgement"
                $ordinaryDatagram = Receive-ProofDatagram `
                    -Client $Client `
                    -ServerProcess $ServerProcess `
                    -OutputCapture $OutputCapture `
                    -Deadline $Deadline `
                    -ServerEndpoint $ServerEndpoint `
                    -Description "ordinary response before duplicate fragment"
                $ordinary = Read-SequencedDatagram `
                    -Packet $ordinaryDatagram.Bytes `
                    -Description "ordinary response before duplicate fragment"
                Assert-SequencedHeader `
                    -Decoded $ordinary `
                    -ExpectedSequence ([uint32]($latestServerSequence + 1)) `
                    -ExpectedAcknowledgement $clientSequence `
                    -ExpectedReliableToggle $false `
                    -ExpectedReliableAcknowledgementToggle $false `
                    -Description "ordinary response before duplicate fragment"
                $latestServerSequence = $ordinary.Sequence

                $clientSequence = [uint32]($clientSequence + 1)
                Send-FragmentAcknowledgement `
                    -Client $Client `
                    -ClientSequence $clientSequence `
                    -ServerSequence $latestServerSequence `
                    -ReliableAcknowledgementToggle $wrongToggle `
                    -NopPayload $Handshake.NopPayload `
                    -Description "duplicate-fragment retransmission trigger"
                $duplicateDatagram = Receive-ProofDatagram `
                    -Client $Client `
                    -ServerProcess $ServerProcess `
                    -OutputCapture $OutputCapture `
                    -Deadline $Deadline `
                    -ServerEndpoint $ServerEndpoint `
                    -Description "duplicate fragment retransmission"
                $duplicate = Read-FragmentedSequencedDatagram `
                    -Packet $duplicateDatagram.Bytes `
                    -Description "duplicate fragment retransmission"
                $fragmentSends++
                if ($duplicate.Datagram.Sequence -ne ([uint32]($latestServerSequence + 1)) -or
                    $duplicate.RawFragmentId -ne $resend.RawFragmentId) {
                    throw "duplicate fragment retransmission changed identity"
                }
                Assert-ExactBytes `
                    -Actual $duplicate.FragmentBytes `
                    -Expected $resend.FragmentBytes `
                    -Description "duplicate decoded fragment"
                if ((Add-ProbeFragment -Slots $slots -Fragment $duplicate -Description "duplicate manifest fragment") -cne "duplicate") {
                    throw "exact duplicate fragment was not suppressed"
                }
                $duplicateSuppressed = $true
                $latestServerSequence = $duplicate.Datagram.Sequence
            }
        }

        $clientSequence = [uint32]($clientSequence + 1)
        Send-FragmentAcknowledgement `
            -Client $Client `
            -ClientSequence $clientSequence `
            -ServerSequence $latestServerSequence `
            -ReliableAcknowledgementToggle $currentReliableToggle `
            -NopPayload $Handshake.NopPayload `
            -Description "correct fragment acknowledgement"
        $response = Receive-ProofDatagram `
            -Client $Client `
            -ServerProcess $ServerProcess `
            -OutputCapture $OutputCapture `
            -Deadline $Deadline `
            -ServerEndpoint $ServerEndpoint `
            -Description "fragment acknowledgement response"

        if ($fragment.FragmentIndex -eq $fragmentCount) {
            $finalResponse = Read-SequencedDatagram `
                -Packet $response.Bytes `
                -Description "final fragment acknowledgement response"
            Assert-SequencedHeader `
                -Decoded $finalResponse `
                -ExpectedSequence ([uint32]($latestServerSequence + 1)) `
                -ExpectedAcknowledgement $clientSequence `
                -ExpectedReliableToggle $false `
                -ExpectedReliableAcknowledgementToggle $false `
                -Description "final fragment acknowledgement response"
            Assert-NopPayload `
                -Payload $finalResponse.Payload `
                -Description "final fragment acknowledgement response payload"
            $latestServerSequence = $finalResponse.Sequence
            break
        }

        $latestServerSequence = [uint32]$latestServerSequence
        $nextDatagram = $response
        $currentReliableToggle = -not $currentReliableToggle
    }

    [byte[]]$reassembled = Join-ProbeFragments -Slots $slots
    $decodedManifest = Read-ResourceManifestPayload `
        -Payload $reassembled `
        -ExpectedSpawnCount $Handshake.ServerInfo.SpawnCount
    Assert-ManifestMatchesFixture `
        -Manifest $decodedManifest `
        -ExpectedEntries $ExpectedEntries `
        -Description "reassembled fragmented resource manifest"

    return [pscustomobject]@{
        Manifest = $decodedManifest
        FragmentCount = $fragmentCount
        FragmentSends = $fragmentSends
        RetransmissionObserved = $retransmissionObserved
        RetransmittedIdentical = $retransmittedIdentical
        DuplicateSuppressed = $duplicateSuppressed
        MissingFirst = $missingFirst
        MissingMiddle = $missingMiddle
        MissingFinal = $missingFinal
        FutureAckRejected = $futureAckRejected
        WrongAckRejected = $wrongAckRejected
        FinalManifestAck = $true
        ServerResponsive = $true
        ApplicationPayloadDeliveries = 1
    }
}

function Assert-UdpSummary {
    param(
        [string]$Stdout,
        [int]$ExpectedDatagrams
    )

    $line = Get-ExactlyOneSummaryLine -Stdout $Stdout -Prefix "goldsrc_udp_summary:"
    Assert-SummaryFields -Line $line -Description "goldsrc_udp_summary" -Expected ([ordered]@{
        enabled = "1"
        ready = "1"
        clean_shutdown = "1"
        accepted = "1"
        session_count = "1"
        state = "connected"
    })
    $datagrams = Get-StableUnsignedField -Line $line -Name "datagrams"
    if ($datagrams -lt 7) {
        throw "fragmented proof processed too few external UDP datagrams"
    }
}

function Assert-ResourceNetchanSummary {
    param(
        [string]$Stdout,
        [ValidateSet("Positive", "Negative", "Oversized")]
        [string]$Mode
    )

    $line = Get-ExactlyOneSummaryLine -Stdout $Stdout -Prefix "goldsrc_netchan_summary:"
    $oversized = $Mode -eq "Oversized"
    $negative = $Mode -eq "Negative"
    $fragmentCount = [int](Get-StableUnsignedField -Line $line -Name "fragment_count")
    Assert-SummaryFields -Line $line -Description "goldsrc_netchan_summary" -Expected ([ordered]@{
        enabled = "1"
        initialized = "1"
        netchan_state = "established"
        reliable_pending_bytes = "0"
        fragment_transfer_started = $(if ($oversized) { "0" } else { "1" })
        fragment_transfer_completed = $(if ($oversized) { "0" } else { "1" })
        fragment_transfer_active = "0"
        fragment_transfer_phase = $(if ($oversized) { "none" } else { "completed" })
        session_count = "1"
        state = "connected"
        put_in_server = "0"
        spawned = "0"
        active = "0"
        server_still_responsive = $(if ($Mode -eq "Positive") { "0" } else { "1" })
        clean_shutdown = "1"
    })
    if (-not $oversized) {
        if ($fragmentCount -lt 3) {
            throw "netchan summary does not report multiple fragments"
        }
        $acknowledged = Get-StableUnsignedField -Line $line -Name "fragment_acknowledged_count"
        if ($acknowledged -ne [uint64]$fragmentCount) {
            throw "netchan summary fragment completion count is inconsistent"
        }
        $resends = Get-StableUnsignedField -Line $line -Name "fragment_resend_count"
        if (($negative -and $resends -lt 3) -or (-not $negative -and $resends -ne 0)) {
            throw "netchan summary retransmission count does not match proof mode"
        }
    }
}

function Assert-ResourceServerInfoSummary {
    param(
        [string]$Stdout,
        $DecodedServerInfo,
        [string]$ExpectedClientDllMd5,
        [ValidateSet("Positive", "Negative", "Oversized")]
        [string]$Mode,
        [switch]$ObservedContinuation
    )

    $line = Get-ExactlyOneSummaryLine -Stdout $Stdout -Prefix "goldsrc_serverinfo_summary:"
    Assert-SummaryFields -Line $line -Description "goldsrc_serverinfo_summary" -Expected ([ordered]@{
        enabled = "1"
        client_new_delivered = "1"
        serverinfo_generations = "1"
        serverinfo_sent = "1"
        serverinfo_acked = "1"
        serverinfo_protocol = "48"
        signon_phase = $(if ($Mode -eq "Oversized") { "awaiting_resource_request" } else { "resource_manifest_acknowledged" })
        session_count = "1"
        state = "connected"
        netchan_state = "established"
        put_in_server = "0"
        spawned = "0"
        active = "0"
        clean_shutdown = "1"
    })
    $md5 = Get-StableFieldValue -Line $line -Name "serverinfo_client_dll_md5"
    if ($md5.ToLowerInvariant() -cne $ExpectedClientDllMd5.ToLowerInvariant() -or
        $md5.ToLowerInvariant() -cne $DecodedServerInfo.ClientDllMd5Hex) {
        throw "fragmented proof serverinfo identity mismatch"
    }
    if ($Mode -eq "Negative") {
        if ((Get-StableUnsignedField -Line $line -Name "future_ack_rejected") -lt 1) {
            throw "fragmented proof summary is missing future-ACK rejection"
        }
        Assert-SummaryBoolean `
            -Line $line `
            -Name "wrong_reliable_ack_rejected" `
            -Expected $true `
            -Description "goldsrc_serverinfo_summary"
    }
}

function Assert-ResourceManifestSummary {
    param(
        [string]$Stdout,
        [ValidateSet("Positive", "Negative", "Oversized")]
        [string]$Mode,
        [int]$ExpectedEntryCount,
        [int]$ExpectedPayloadBytes,
        [switch]$ObservedContinuation
    )

    $line = Get-ExactlyOneSummaryLine -Stdout $Stdout -Prefix "goldsrc_resource_manifest_summary:"
    $oversized = $Mode -eq "Oversized"
    Assert-SummaryFields -Line $line -Description "goldsrc_resource_manifest_summary" -Expected ([ordered]@{
        enabled = "1"
        observed_continuation_received = $(if ($ObservedContinuation -and -not $oversized) { "true" } else { "false" })
        observed_continuation_deliveries = $(if ($ObservedContinuation -and -not $oversized) { "1" } else { "0" })
        close_menus_companions_accepted = $(if ($ObservedContinuation -and -not $oversized) { "2" } else { "0" })
        observed_continuation_incomplete_rejected = "false"
        observed_continuation_unsupported_rejected = "false"
        resource_manifest_preparation_attempts = "1"
        resource_manifest_context_built = $(if ($oversized) { "0" } else { "1" })
        resource_manifest_generations = $(if ($oversized) { "0" } else { "1" })
        resource_manifest_queued = $(if ($oversized) { "0" } else { "1" })
        resource_manifest_sent = $(if ($oversized) { "0" } else { "1" })
        resource_manifest_acked = $(if ($oversized) { "0" } else { "1" })
        resource_manifest_entry_count = [string]$ExpectedEntryCount
        resource_manifest_payload_bytes = [string]$ExpectedPayloadBytes
        signon_phase = $(if ($oversized) { "awaiting_resource_request" } else { "resource_manifest_acknowledged" })
        session_count = "1"
        lifecycle_state = "connected"
        netchan_state = "established"
        reliable_pending_bytes = "0"
        put_in_server = "0"
        spawned = "0"
        active = "0"
        server_still_responsive = $(if ($Mode -eq "Positive") { "false" } else { "true" })
        clean_shutdown = "1"
    })
    Assert-SummaryBoolean -Line $line -Name "payload_too_large" -Expected $oversized -Description "goldsrc_resource_manifest_summary"
    Assert-SummaryBoolean -Line $line -Name "oversized_manifest_not_truncated" -Expected $oversized -Description "goldsrc_resource_manifest_summary"
    Assert-SummaryBoolean -Line $line -Name "requires_fragmentation_reported" -Expected $oversized -Description "goldsrc_resource_manifest_summary"
    Assert-SummaryBoolean -Line $line -Name "final_manifest_ack" -Expected (-not $oversized) -Description "goldsrc_resource_manifest_summary"
}

function New-OversizedManifestFixture {
    param([string]$Path)

    $lines = New-Object 'System.Collections.Generic.List[string]'
    $lines.Add("# deterministic valid fixture larger than the verified 65536-byte transfer bound")
    $padding = "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
    for ($index = 1; $index -le 1100; $index++) {
        $lines.Add(("generic`t{0}`t{1}`t1`tgfx/p237/{0:D4}_{2}.dat" -f
            $index,
            (1000 + $index),
            $padding))
    }
    $lines.Add("sound`t1`t2048`t0`tambience/wind1.wav")
    $lines.Add("model`t1`t4096`t0`tmaps/c0a0.bsp")
    $lines.Add("decal`t0`t64`t0`tdecals/lambda")
    $lines.Add("event`t1`t128`t1`tevents/train.sc")
    [System.IO.File]::WriteAllLines(
        $Path,
        $lines.ToArray(),
        (New-Object System.Text.UTF8Encoding($false))
    )
}

$repoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$resolvedExecutablePath = [System.IO.Path]::GetFullPath($ExecutablePath)
$resolvedGameDir = [System.IO.Path]::GetFullPath($GameDir)
$resolvedManifestFixture = [System.IO.Path]::GetFullPath($ManifestFixture)

if (-not (Test-Path -LiteralPath $resolvedExecutablePath -PathType Leaf)) {
    throw ("hlhost executable not found: {0}" -f $resolvedExecutablePath)
}
if (-not (Test-Path -LiteralPath $resolvedGameDir -PathType Container)) {
    throw ("game directory not found: {0}" -f $resolvedGameDir)
}
$parsedBindAddress = $null
if (-not [System.Net.IPAddress]::TryParse($BindAddress, [ref]$parsedBindAddress) -or
    $parsedBindAddress.AddressFamily -ne [System.Net.Sockets.AddressFamily]::InterNetwork -or
    $parsedBindAddress.ToString() -cne "127.0.0.1") {
    throw ("BindAddress must be exactly 127.0.0.1 for this proof: {0}" -f $BindAddress)
}
$clientDllPath = $null
foreach ($candidate in @(
    (Join-Path $resolvedGameDir "cl_dlls/client.dll"),
    (Join-Path $resolvedGameDir "dlls/client.dll")
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
$expectedEntries = @(Read-ResourceManifestFixture -Path $resolvedManifestFixture)
$repositorySnapshotBefore = Get-RepositoryFileSnapshot -RepositoryRoot $repoRoot
$mainResult = $null
$oversizedResult = $null
$oversizedFixturePath = $null
$failure = $null

try {
    $mainMode = if ($NegativeProof) { "Negative" } else { "Positive" }
    $mainResult = Invoke-ResourceManifestHostRun `
        -Mode $mainMode `
        -ResolvedExecutablePath $resolvedExecutablePath `
        -ResolvedGameDir $resolvedGameDir `
        -FixturePath $resolvedManifestFixture `
        -ExpectedEntries $expectedEntries `
        -ExpectedClientDllMd5 $expectedClientDllMd5 `
        -RepositoryRoot $repoRoot `
        -Address $BindAddress `
        -RequestedPort $Port `
        -RunTimeoutSeconds $TimeoutSeconds `
        -ObservedContinuation:$ObservedContinuation `
        -SuppressServerOutput:$SkipServerOutput

    if ($NegativeProof) {
        $oversizedFixturePath = Join-Path `
            ([System.IO.Path]::GetTempPath()) `
            ("hlhost_prompt237_oversized_{0}.tsv" -f [Guid]::NewGuid().ToString("N"))
        New-OversizedManifestFixture -Path $oversizedFixturePath
        $oversizedEntries = @(Read-ResourceManifestFixture -Path $oversizedFixturePath)
        $oversizedResult = Invoke-ResourceManifestHostRun `
            -Mode "Oversized" `
            -ResolvedExecutablePath $resolvedExecutablePath `
            -ResolvedGameDir $resolvedGameDir `
            -FixturePath $oversizedFixturePath `
            -ExpectedEntries $oversizedEntries `
            -ExpectedClientDllMd5 $expectedClientDllMd5 `
            -RepositoryRoot $repoRoot `
            -Address $BindAddress `
            -RequestedPort 0 `
            -RunTimeoutSeconds $TimeoutSeconds `
            -SuppressServerOutput:$SkipServerOutput

        $fragmentationUnitPath = Join-Path `
            (Split-Path -Parent $resolvedExecutablePath) `
            "goldsrc_fragmentation_tests.exe"
        if (-not (Test-Path -LiteralPath $fragmentationUnitPath -PathType Leaf)) {
            throw "fragmentation unit helper is missing beside hlhost"
        }
        & $fragmentationUnitPath
        if ($LASTEXITCODE -ne 0) {
            throw ("fragmentation unit helper failed with exit code {0}" -f $LASTEXITCODE)
        }
    }
    Assert-NoRepositoryFileMutation -RepositoryRoot $repoRoot -Before $repositorySnapshotBefore
}
catch {
    $failure = $_
}
finally {
    if ($null -ne $oversizedFixturePath -and
        (Test-Path -LiteralPath $oversizedFixturePath -PathType Leaf)) {
        Remove-Item -LiteralPath $oversizedFixturePath -Force -ErrorAction SilentlyContinue
    }
}

if ($null -ne $failure) {
    $failureMessage = $failure.Exception.Message -replace '[\r\n]+', ' '
    Write-Host ("goldsrc_fragmented_manifest_probe: result=fail reason={0}" -f $failureMessage)
    throw $failureMessage
}

Write-Host ("goldsrc_fragmented_manifest_probe: port={0},client_endpoint=127.0.0.1:{1}" -f
    $mainResult.Port,
    $mainResult.ClientPort)
Write-Host ("goldsrc_fragmented_manifest_probe: udp_owner={0},child_process_check={1},repo_files_mutated=0,temp_files_cleaned=1" -f
    $mainResult.UdpOwner,
    $mainResult.ChildProcessCheck)
if ($NegativeProof) {
    $exchange = $mainResult.Exchange
    if (-not $exchange.RetransmissionObserved -or
        -not $exchange.RetransmittedIdentical -or
        -not $exchange.DuplicateSuppressed -or
        -not $exchange.MissingFirst -or
        -not $exchange.MissingMiddle -or
        -not $exchange.MissingFinal -or
        -not $exchange.FutureAckRejected -or
        -not $exchange.WrongAckRejected -or
        -not $exchange.FinalManifestAck -or
        $exchange.ApplicationPayloadDeliveries -ne 1) {
        throw "fragmented negative proof evidence is incomplete"
    }
    Write-Host "goldsrc_fragmented_manifest_proof_b: fragmented_transfer_started=true,multiple_fragments_sent=true,missing_fragment_detected=true,retransmission_observed=true,retransmitted_fragment_bytes_identical=true,duplicate_fragments_suppressed=true,wrong_reliable_ack_rejected=true,future_ack_rejected=true,final_completion_ack=true,application_payload_deliveries=1,oversized_transfer_rejected=true,oversized_transfer_not_truncated=true,slot_reset_cleared_fragment_state=true,session_count=1,put_in_server=0,spawned=0,active=0,server_still_responsive=true,clean_shutdown=1,proof_b=pass"
} else {
    Write-Host ("goldsrc_fragmented_manifest_proof_a: transport=udp,external_datagrams=true,protocol=48,connectionless_handshake=pass,netchan_state=established,serverinfo_decode=pass,serverinfo_acked=true,resource_request=sendres,fragment_count={0},multiple_fragments=true,fragment_reassembly=pass,resource_manifest_decode=pass,resource_count={1},resource_order=pass,world_resource=maps/c0a0.bsp,no_duplicate_indices=true,no_missing_entries=true,no_unexpected_file_payload=true,lifecycle_state=connected,signon_phase=resource_manifest_acknowledged,fragment_transfer_completed=1,fragment_transfer_active=0,resource_request_deliveries=1,resource_manifest_generations=1,resource_manifest_acked=1,reliable_pending_bytes=0,put_in_server=0,spawned=0,active=0,clean_shutdown=1,proof_a=pass" -f
        $mainResult.Exchange.FragmentCount,
        $mainResult.Exchange.Manifest.ResourceCount)
}
Write-Host "goldsrc_fragmented_manifest_probe: result=pass"
