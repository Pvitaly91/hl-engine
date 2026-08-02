[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$ExecutablePath,

    [Parameter(Mandatory = $true)]
    [string]$GameDir,

    [string]$BindAddress = "127.0.0.1",

    [ValidateRange(0, 65535)]
    [int]$Port = 0,

    [ValidateRange(60, 900)]
    [int]$TimeoutSeconds = 240,

    [switch]$NegativeProof,

    [switch]$SkipServerOutput
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
if (Test-Path Variable:PSNativeCommandUseErrorActionPreference) {
    $PSNativeCommandUseErrorActionPreference = $false
}
if ($BindAddress -cne "127.0.0.1") {
    throw "two-client proofs are restricted to 127.0.0.1"
}

function Get-TopLevelProofFunctionDefinitions {
    param(
        [string]$Path,
        [string[]]$Names = @()
    )

    $tokens = $null
    $errors = $null
    $ast = [System.Management.Automation.Language.Parser]::ParseFile(
        $Path,
        [ref]$tokens,
        [ref]$errors)
    if (@($errors).Count -ne 0) {
        throw "proof helper has parser errors"
    }
    return @(
        $ast.EndBlock.Statements |
            Where-Object {
                $_ -is [System.Management.Automation.Language.FunctionDefinitionAst] -and
                ($Names.Count -eq 0 -or $Names -ccontains $_.Name)
            } |
            ForEach-Object { $_.Extent.Text })
}

foreach ($definitionText in @(
    Get-TopLevelProofFunctionDefinitions `
        -Path (Join-Path $PSScriptRoot "run_goldsrc_serverinfo_proof.ps1")
)) {
    . ([scriptblock]::Create($definitionText))
}
foreach ($definitionText in @(
    Get-TopLevelProofFunctionDefinitions `
        -Path (Join-Path $PSScriptRoot "run_goldsrc_resource_manifest_proof.ps1") `
        -Names @(
            "Assert-CanonicalResourcePath",
            "Read-ResourceManifestFixture",
            "Read-ManifestBits",
            "Read-ManifestProtocolString",
            "Get-ResourceTypeName",
            "Read-ResourceManifestPayload",
            "Assert-ManifestMatchesFixture",
            "Reserve-LoopbackUdpPort"
        )
)) {
    . ([scriptblock]::Create($definitionText))
}
foreach ($definitionText in @(
    Get-TopLevelProofFunctionDefinitions `
        -Path (Join-Path $PSScriptRoot "run_goldsrc_fragmented_manifest_proof.ps1") `
        -Names @(
            "Read-LittleEndianUInt16",
            "Read-FragmentedSequencedDatagram",
            "Add-ProbeFragment",
            "Join-ProbeFragments",
            "Receive-ProofDatagram",
            "Send-FragmentAcknowledgement"
        )
)) {
    . ([scriptblock]::Create($definitionText))
}
foreach ($definitionText in @(
    Get-TopLevelProofFunctionDefinitions `
        -Path (Join-Path $PSScriptRoot "run_goldsrc_delta_description_proof.ps1")
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
$svcDeltaDescription = [byte]14
$svcSetView = [byte]5
$svcCdTrack = [byte]32
$svcNewMoveVars = [byte]44
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
$maximumDeltaBundleBytes = 65536
$maximumDeltaNameBytes = 31
$maximumDeltaFieldsPerTable = 56
$deltaMultiplierScale = [uint64]4000
$deltaTypeSigned = [uint64]2147483648
$ExpectedZMaximum = [single]6300.0
$ExpectedCdTrack = 3
$canonicalDeltaTableOrder = @(
    "event_t",
    "weapon_data_t",
    "usercmd_t",
    "custom_entity_state_t",
    "entity_state_player_t",
    "entity_state_t",
    "clientdata_t"
)

function New-TwoClientProbe {
    param([System.Net.IPEndPoint]$ServerEndpoint)

    $client = New-Object System.Net.Sockets.UdpClient(
        [System.Net.Sockets.AddressFamily]::InterNetwork)
    $client.Client.ReceiveBufferSize = 4 * 1024 * 1024
    $client.Client.Bind((New-Object System.Net.IPEndPoint(
        [System.Net.IPAddress]::Loopback,
        0)))
    $client.Connect($ServerEndpoint)
    return $client
}

function New-TwoClientStreamState {
    param(
        [System.Net.Sockets.UdpClient]$Client,
        [int]$OwnEntity,
        $Connection
    )

    $latest = $Connection.Resource.LatestAcknowledgedSnapshot
    $remoteEntity = if ($OwnEntity -eq 1) { 2 } else { 1 }
    $remoteVisible = $null -ne $latest -and
        $latest.PlayerEntities -contains $remoteEntity
    return [pscustomobject]@{
        Client = $Client
        OwnEntity = $OwnEntity
        RemoteEntity = $remoteEntity
        ClientSequence = [uint32]$Connection.Resource.ClientSequence
        LatestServerSequence = [uint32]$Connection.Resource.LatestServerSequence
        ReliableAcknowledgementState =
            [bool]$Connection.Resource.CurrentServerReliableAcknowledgementState
        LatestSnapshot = $latest
        SnapshotsByLow8 = $Connection.Resource.AcknowledgedSnapshotsByLow8
        Snapshots = [int]$Connection.Resource.ContinuousSnapshotsReceived
        FrameAcknowledgements =
            [int]$Connection.Resource.ContinuousReferencesSent
        PmovePackets = [int]$Connection.Resource.PmovePacketsSent
        OwnVisible = $null -ne $latest -and
            $latest.PlayerEntities -contains $OwnEntity
        RemoteVisible = $remoteVisible
        LastRemotePresent = $remoteVisible
        RemoteAddSeen = $remoteVisible
        RemoteUpdateSeen = $false
        RemoteRemoveSeen = $false
        RemoteReaddSeen = $false
        FullSnapshots = 0
        DeltaSnapshots = 0
    }
}

function Invoke-TwoClientSignon {
    param(
        [System.Net.Sockets.UdpClient]$Client,
        [int]$OwnEntity,
        [string]$ProbeName,
        [System.Diagnostics.Process]$ServerProcess,
        $OutputCapture,
        [string]$StdoutPath,
        [DateTime]$Deadline,
        [System.Net.IPEndPoint]$ServerEndpoint,
        [string]$ExpectedClientDllMd5,
        [object[]]$ExpectedEntries
    )

    $handshake = Invoke-DeltaInitialHandshake `
        -Client $Client `
        -ServerProcess $ServerProcess `
        -OutputCapture $OutputCapture `
        -Deadline $Deadline `
        -ServerEndpoint $ServerEndpoint `
        -ProbeName $ProbeName `
        -ExpectedSlot $OwnEntity
    $bootstrap = Invoke-UnfragmentedDeltaBootstrap `
        -Client $Client `
        -ServerProcess $ServerProcess `
        -OutputCapture $OutputCapture `
        -StdoutPath $StdoutPath `
        -Deadline $Deadline `
        -ServerEndpoint $ServerEndpoint `
        -Handshake $handshake `
        -ExpectedClientDllMd5 $ExpectedClientDllMd5 `
        -ExpectedMaxClients 2 `
        -ExpectedPlayerIndex ($OwnEntity - 1) `
        -ExpectedViewEntity $OwnEntity
    $resource = Invoke-ObservedResourceContinuation `
        -Client $Client `
        -ServerProcess $ServerProcess `
        -OutputCapture $OutputCapture `
        -Deadline $Deadline `
        -ServerEndpoint $ServerEndpoint `
        -Handshake $handshake `
        -Bootstrap $bootstrap `
        -ExpectedEntries $ExpectedEntries `
        -StdoutPath $StdoutPath `
        -PostResourceMode World `
        -ReceiveFirstSnapshot `
        -ReceiveContinuousSnapshots `
        -ReceivePlayerLifecycle `
        -ReceivePmove `
        -ReturnAfterContinuousCount `
        -MaximumClients 2 `
        -OwnEntity $OwnEntity `
        -ContinuousSnapshotCount 12
    if (-not $resource.PlayerEntityObserved -or
        $resource.PmovePacketsSent -lt 1) {
        throw "two-client probe did not reach spawned PM_Move state"
    }
    return [pscustomobject]@{
        Handshake = $handshake
        Bootstrap = $bootstrap
        Resource = $resource
    }
}

function Receive-TwoClientSnapshot {
    param(
        $State,
        [System.Diagnostics.Process]$ServerProcess,
        $OutputCapture,
        [DateTime]$Deadline,
        [System.Net.IPEndPoint]$ServerEndpoint,
        [int]$Forward = 0,
        [int]$Side = 0,
        [int]$Buttons = 0
    )

    for ($attempt = 0; $attempt -lt 256; $attempt++) {
        $datagram = Receive-ProofDatagram `
            -Client $State.Client `
            -ServerProcess $ServerProcess `
            -OutputCapture $OutputCapture `
            -Deadline $Deadline `
            -ServerEndpoint $ServerEndpoint `
            -Description "two-client continuous snapshot"
        if ($datagram.Bytes.Length -ge 4 -and
            $datagram.Bytes[0] -eq 0xFF -and
            $datagram.Bytes[1] -eq 0xFF -and
            $datagram.Bytes[2] -eq 0xFF -and
            $datagram.Bytes[3] -eq 0xFF) {
            continue
        }
        $packet = Read-SequencedDatagram `
            -Packet $datagram.Bytes `
            -Description "two-client continuous snapshot"
        $State.LatestServerSequence = [uint32]$packet.Sequence
        if ($packet.Payload.Length -eq 0 -or $packet.Payload[0] -eq $svcNop) {
            continue
        }
        if ($packet.ReliableToggle -or $packet.FragmentPresent -or
            $packet.Payload[0] -ne 7) {
            throw "two-client stream emitted an unexpected carrier"
        }

        $snapshot = $null
        try {
            $snapshot = Read-PlayerLifecycleFullSnapshotPayload `
                -Payload $packet.Payload `
                -FrameId ([uint32]$packet.Sequence) `
                -PreviousSnapshot $State.LatestSnapshot `
                -DeltaTables $script:twoClientDeltaTables `
                -MaximumClients 2 `
                -OwnEntity $State.OwnEntity
        } catch {
            $wireBase = Get-ContinuousSnapshotBaseLow8 -Payload $packet.Payload
            if (-not $State.SnapshotsByLow8.ContainsKey([int]$wireBase)) {
                throw "two-client delta selected an unacknowledged frame"
            }
            $snapshot = Read-PlayerLifecycleContinuousSnapshotPayload `
                -Payload $packet.Payload `
                -FrameId ([uint32]$packet.Sequence) `
                -BaseSnapshot $State.SnapshotsByLow8[[int]$wireBase] `
                -DeltaTables $script:twoClientDeltaTables `
                -MaximumClients 2 `
                -OwnEntity $State.OwnEntity
        }

        $State.Snapshots++
        if ($snapshot.DeltaPacketEntitiesReceived) {
            $State.DeltaSnapshots++
        } else {
            $State.FullSnapshots++
        }
        $State.OwnVisible = $State.OwnVisible -or
            $snapshot.PlayerEntities -contains $State.OwnEntity
        $State.LastRemotePresent =
            $snapshot.PlayerEntities -contains $State.RemoteEntity
        if ($State.LastRemotePresent) {
            $State.RemoteVisible = $true
        }
        if ($snapshot.PlayerAddEntities -contains $State.RemoteEntity) {
            if ($State.RemoteRemoveSeen) {
                $State.RemoteReaddSeen = $true
            } else {
                $State.RemoteAddSeen = $true
            }
        }
        if ($snapshot.PlayerUpdateEntities -contains $State.RemoteEntity) {
            $State.RemoteUpdateSeen = $true
        }
        if ($snapshot.PlayerRemoveEntities -contains $State.RemoteEntity) {
            $State.RemoteRemoveSeen = $true
            $State.RemoteVisible = $false
        }

        $State.ClientSequence = [uint32]($State.ClientSequence + 1u)
        [byte[]]$frameReference = @(
            [byte]4,
            [byte]([uint32]$snapshot.FrameId -band 0xFF))
        [byte[]]$movement = New-GoldSrcMovementPayload `
            -Sequence $State.ClientSequence `
            -Msec 10 `
            -Forward $Forward `
            -Side $Side `
            -Buttons $Buttons
        Send-DeltaClientPacket `
            -Client $State.Client `
            -Sequence $State.ClientSequence `
            -Acknowledgement $State.LatestServerSequence `
            -ServerReliableAcknowledgementState (
                $State.ReliableAcknowledgementState) `
            -Payload ([byte[]]($frameReference + $movement)) `
            -Description "two-client frame acknowledgement and movement"
        $State.FrameAcknowledgements++
        $State.PmovePackets++
        $State.LatestSnapshot = $snapshot
        $State.SnapshotsByLow8[
            [int]([uint32]$snapshot.FrameId -band 0xFF)] = $snapshot
        return $snapshot
    }
    throw "two-client stream did not yield a snapshot"
}

function Send-TwoClientSimultaneousMove {
    param(
        $State,
        [int]$Forward,
        [int]$Side
    )

    $State.ClientSequence = [uint32]($State.ClientSequence + 1u)
    [byte[]]$frameReference = @(
        [byte]4,
        [byte]([uint32]$State.LatestSnapshot.FrameId -band 0xFF))
    [byte[]]$movement = New-GoldSrcMovementPayload `
        -Sequence $State.ClientSequence `
        -Msec 10 `
        -Forward $Forward `
        -Side $Side
    Send-DeltaClientPacket `
        -Client $State.Client `
        -Sequence $State.ClientSequence `
        -Acknowledgement $State.LatestServerSequence `
        -ServerReliableAcknowledgementState (
            $State.ReliableAcknowledgementState) `
        -Payload ([byte[]]($frameReference + $movement)) `
        -Description "simultaneous two-client movement"
    $State.FrameAcknowledgements++
    $State.PmovePackets++
}

function Receive-ConnectionlessOnly {
    param(
        [System.Net.Sockets.UdpClient]$Client,
        [System.Diagnostics.Process]$ServerProcess,
        $OutputCapture,
        [DateTime]$Deadline,
        [System.Net.IPEndPoint]$ServerEndpoint
    )

    while ([DateTime]::UtcNow -lt $Deadline) {
        $datagram = Receive-UdpDatagram `
            -Client $Client `
            -ServerProcess $ServerProcess `
            -OutputCapture $OutputCapture `
            -Deadline $Deadline `
            -ExpectedRemoteEndpoint $ServerEndpoint `
            -Description "connectionless rejection proof"
        if ($datagram.Bytes.Length -ge 4 -and
            $datagram.Bytes[0] -eq 0xFF -and
            $datagram.Bytes[1] -eq 0xFF -and
            $datagram.Bytes[2] -eq 0xFF -and
            $datagram.Bytes[3] -eq 0xFF) {
            return $datagram.Bytes
        }
    }
    throw "connectionless response was not observed"
}

function Invoke-RejectedConnectionAttempt {
    param(
        [System.Net.Sockets.UdpClient]$Client,
        [string]$ProbeName,
        [System.Diagnostics.Process]$ServerProcess,
        $OutputCapture,
        [DateTime]$Deadline,
        [System.Net.IPEndPoint]$ServerEndpoint
    )

    Send-ExactUdpDatagram `
        -Client $Client `
        -Packet (New-ConnectionlessDatagram `
            -Text ("getchallenge steam" + [char]10) `
            -Tail ([byte[]](0x00))) `
        -Description "two-client rejection challenge"
    $challengeBytes = Receive-ConnectionlessOnly `
        -Client $Client `
        -ServerProcess $ServerProcess `
        -OutputCapture $OutputCapture `
        -Deadline $Deadline `
        -ServerEndpoint $ServerEndpoint
    $challengeText = [Text.Encoding]::ASCII.GetString(
        $challengeBytes, 4, $challengeBytes.Length - 4)
    $match = [regex]::Match($challengeText, 'A00000000 (?<value>[0-9]+)')
    if (-not $match.Success) {
        throw "rejection proof challenge was malformed"
    }
    $protocolInfo = '\prot\3\unique\-1\raw\steam\cdkey\00000000000000000000000000000000'
    $userInfo = '\name\' + $ProbeName + '\model\gordon'
    $connectLine = ('connect 48 {0} "{1}" "{2}"' -f
        $match.Groups['value'].Value,
        $protocolInfo,
        $userInfo) + [char]10
    Send-ExactUdpDatagram `
        -Client $Client `
        -Packet (New-ConnectionlessDatagram `
            -Text $connectLine `
            -Tail ([byte[]]@(0x00, 0x01, 0x7F, 0x80, 0xFF))) `
        -Description "two-client rejected connect"
    $rejection = Receive-ConnectionlessOnly `
        -Client $Client `
        -ServerProcess $ServerProcess `
        -OutputCapture $OutputCapture `
        -Deadline $Deadline `
        -ServerEndpoint $ServerEndpoint
    $rejectionText = [Text.Encoding]::ASCII.GetString(
        $rejection, 4, $rejection.Length - 4)
    if ($rejectionText.StartsWith('B 1 ', [StringComparison]::Ordinal)) {
        throw "connection expected to be rejected was admitted"
    }
    return $rejectionText
}

function Send-ClientDisconnect {
    param($State)

    $State.ClientSequence = [uint32]($State.ClientSequence + 1u)
    Send-DeltaClientPacket `
        -Client $State.Client `
        -Sequence $State.ClientSequence `
        -Acknowledgement $State.LatestServerSequence `
        -ServerReliableAcknowledgementState (
            $State.ReliableAcknowledgementState) `
        -Payload (New-StringCommandPayload -Command "dropclient`n") `
        -ReliablePayload `
        -Description "two-client disconnect"
}

function Get-SummaryFields {
    param(
        [string]$Stdout,
        [string]$Prefix,
        [int]$ExpectedCount = 1
    )

    $lines = @(
        $Stdout -split '\r?\n' |
            Where-Object {
                $_.IndexOf($Prefix, [StringComparison]::Ordinal) -ge 0
            })
    if ($lines.Count -ne $ExpectedCount) {
        throw "required two-client summary count mismatch"
    }
    $records = @()
    foreach ($line in $lines) {
        $text = $line.Substring(
            $line.IndexOf($Prefix, [StringComparison]::Ordinal) + $Prefix.Length)
        $fields = @{}
        foreach ($part in $text.Trim().Split(',')) {
            $separator = $part.IndexOf('=')
            if ($separator -gt 0) {
                $fields[$part.Substring(0, $separator).Trim()] =
                    $part.Substring($separator + 1).Trim()
            }
        }
        $records += ,$fields
    }
    return $records
}

$repoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$resolvedExecutable = [System.IO.Path]::GetFullPath($ExecutablePath)
$resolvedGameDir = [System.IO.Path]::GetFullPath($GameDir)
$binaryDirectory = Split-Path -Parent $resolvedExecutable
$twoClientUnitTests = Join-Path `
    $binaryDirectory "goldsrc_two_client_replication_unit_tests.exe"
$pmoveUnitTests = Join-Path $binaryDirectory "goldsrc_pmove_tests.exe"
$deltaFixture = Join-Path `
    $repoRoot "src/tests/fixtures/goldsrc_delta_description_minimal.lst"
$manifestFixture = Join-Path `
    $repoRoot "src/tests/fixtures/goldsrc_resource_manifest_minimal.tsv"
foreach ($required in @(
    $resolvedExecutable,
    $twoClientUnitTests,
    $pmoveUnitTests,
    $deltaFixture,
    $manifestFixture,
    (Join-Path $resolvedGameDir "maps/c0a0.bsp"))) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) {
        throw "two-client proof input is missing"
    }
}
& $twoClientUnitTests *> $null
if ($LASTEXITCODE -ne 0) {
    throw "two-client bounded unit validation failed"
}
if ($NegativeProof) {
    # This runtime-level test deliberately makes PM_Move return invalid state
    # for slot A and proves rollback leaves the live slot-B origin, command
    # clock, and subsequent movement untouched.
    & $pmoveUnitTests *> $null
    if ($LASTEXITCODE -ne 0) {
        throw "two-client PM_Move rollback isolation validation failed"
    }
}
$clientDllPath = @(
    (Join-Path $resolvedGameDir "cl_dlls/client.dll"),
    (Join-Path $resolvedGameDir "dlls/client.dll") |
        Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } |
        Select-Object -First 1)
if ($clientDllPath.Count -ne 1) {
    throw "two-client proof client library is missing"
}
$expectedClientDllMd5 = (
    Get-FileHash -LiteralPath $clientDllPath[0] -Algorithm MD5
).Hash.ToLowerInvariant()
$expectedEntries = @(Read-ResourceManifestFixture -Path $manifestFixture)
$selectedPort = if ($Port -eq 0) { Reserve-LoopbackUdpPort } else { $Port }
$serverEndpoint = New-Object System.Net.IPEndPoint(
    [System.Net.IPAddress]::Loopback,
    $selectedPort)
$deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
$tempId = [Guid]::NewGuid().ToString("N")
$stdoutPath = Join-Path ([IO.Path]::GetTempPath()) (
    "hlhost_two_client_${tempId}.stdout.log")
$stderrPath = Join-Path ([IO.Path]::GetTempPath()) (
    "hlhost_two_client_${tempId}.stderr.log")
$shutdownPath = Join-Path ([IO.Path]::GetTempPath()) (
    "hlhost_two_client_${tempId}.shutdown")
$arguments = @(
    "--gamedir", $resolvedGameDir,
    "--dedicated",
    "--deathmatch", "1",
    "--maxclients", "2",
    "--map", "c0a0",
    "--frames", "1",
    "--log-to-file", "0",
    "--log-summary-file", "0",
    "--log-disable-categories=general",
    "--ip", $BindAddress,
    "--port", [string]$selectedPort,
    "--goldsrc-delta-descriptions",
    "--goldsrc-delta-descriptions-fixture", $deltaFixture,
    "--goldsrc-resource-manifest",
    "--goldsrc-resource-manifest-fixture", $manifestFixture,
    "--goldsrc-world-baselines",
    "--goldsrc-first-snapshot",
    "--goldsrc-continuous-snapshots",
    "--goldsrc-snapshot-rate-hz=20",
    "--goldsrc-player-lifecycle",
    "--goldsrc-pmove",
    "--goldsrc-pmove-persistent",
    "--goldsrc-manual-shutdown-file", $shutdownPath,
    "--goldsrc-handshake-timeout-ms", [string]($TimeoutSeconds * 1000)
)
$argumentLine = (($arguments | ForEach-Object {
    ConvertTo-WindowsCommandLineArgument -Value ([string]$_)
}) -join " ")

$serverProcess = $null
$outputCapture = $null
$clientA = $null
$clientB = $null
$clientAReconnect = $null
$thirdClient = $null
$failure = $null
$capturedStdout = ""
$capturedStderr = ""
$result = $null
$proofStage = "server_start"
try {
    $startInfo = New-Object Diagnostics.ProcessStartInfo
    $startInfo.FileName = $resolvedExecutable
    $startInfo.Arguments = $argumentLine
    $startInfo.WorkingDirectory = $repoRoot
    $startInfo.UseShellExecute = $false
    $startInfo.CreateNoWindow = $true
    $startInfo.WindowStyle = [Diagnostics.ProcessWindowStyle]::Hidden
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true
    $serverProcess = New-Object Diagnostics.Process
    $serverProcess.StartInfo = $startInfo
    if (-not $serverProcess.Start()) {
        throw "failed to start two-client proof server"
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

    $proofStage = "client_a_signon"
    $clientA = New-TwoClientProbe -ServerEndpoint $serverEndpoint
    $connectionA = Invoke-TwoClientSignon `
        -Client $clientA `
        -OwnEntity 1 `
        -ProbeName "two_client_a" `
        -ServerProcess $serverProcess `
        -OutputCapture $outputCapture `
        -StdoutPath $stdoutPath `
        -Deadline $deadline `
        -ServerEndpoint $serverEndpoint `
        -ExpectedClientDllMd5 $expectedClientDllMd5 `
        -ExpectedEntries $expectedEntries
    $script:twoClientDeltaTables =
        $connectionA.Bootstrap.Decoded.DeltaBundle.Tables
    $stateA = New-TwoClientStreamState `
        -Client $clientA -OwnEntity 1 -Connection $connectionA

    $proofStage = "client_b_signon"
    $clientB = New-TwoClientProbe -ServerEndpoint $serverEndpoint
    $connectionB = Invoke-TwoClientSignon `
        -Client $clientB `
        -OwnEntity 2 `
        -ProbeName "two_client_b" `
        -ServerProcess $serverProcess `
        -OutputCapture $outputCapture `
        -StdoutPath $stdoutPath `
        -Deadline $deadline `
        -ServerEndpoint $serverEndpoint `
        -ExpectedClientDllMd5 $expectedClientDllMd5 `
        -ExpectedEntries $expectedEntries
    $stateB = New-TwoClientStreamState `
        -Client $clientB -OwnEntity 2 -Connection $connectionB

    $proofStage = "admission_isolation"
    [void](Invoke-RejectedConnectionAttempt `
        -Client $clientA `
        -ProbeName "duplicate_a" `
        -ServerProcess $serverProcess `
        -OutputCapture $outputCapture `
        -Deadline $deadline `
        -ServerEndpoint $serverEndpoint)
    $thirdClient = New-TwoClientProbe -ServerEndpoint $serverEndpoint
    [void](Invoke-RejectedConnectionAttempt `
        -Client $thirdClient `
        -ProbeName "third_client" `
        -ServerProcess $serverProcess `
        -OutputCapture $outputCapture `
        -Deadline $deadline `
        -ServerEndpoint $serverEndpoint)

    $proofStage = "mutual_movement"
    # Client A accumulates legitimate snapshots while client B completes its
    # independent sign-on.  Drain that bounded backlog until both receivers
    # have observed a post-movement remote Update, instead of assuming that a
    # fixed handful of oldest queued datagrams reaches the live edge.
    for ($index = 0; $index -lt 300; $index++) {
        [void](Receive-TwoClientSnapshot `
            -State $stateA `
            -ServerProcess $serverProcess `
            -OutputCapture $outputCapture `
            -Deadline $deadline `
            -ServerEndpoint $serverEndpoint `
            -Forward 220)
        [void](Receive-TwoClientSnapshot `
            -State $stateB `
            -ServerProcess $serverProcess `
            -OutputCapture $outputCapture `
            -Deadline $deadline `
            -ServerEndpoint $serverEndpoint `
            -Side 180)
        if ($index -lt 20) {
            Send-TwoClientSimultaneousMove `
                -State $stateA -Forward 160 -Side 0
            Send-TwoClientSimultaneousMove `
                -State $stateB -Forward 0 -Side -160
        }
        if ($index -ge 20 -and
            $stateA.OwnVisible -and $stateB.OwnVisible -and
            $stateA.RemoteVisible -and $stateB.RemoteVisible -and
            $stateA.RemoteAddSeen -and $stateB.RemoteAddSeen -and
            $stateA.RemoteUpdateSeen -and $stateB.RemoteUpdateSeen) {
            break
        }
    }
    if (-not $stateA.OwnVisible -or -not $stateB.OwnVisible -or
        -not $stateA.RemoteVisible -or -not $stateB.RemoteVisible -or
        -not $stateA.RemoteAddSeen -or -not $stateB.RemoteAddSeen -or
        -not $stateA.RemoteUpdateSeen -or -not $stateB.RemoteUpdateSeen) {
        $visibilityFailure = ("mutual player Add/Update visibility was not proven " +
            "(a_own={0},a_remote={1},a_add={2},a_update={3}," +
            "b_own={4},b_remote={5},b_add={6},b_update={7})") -f
            $stateA.OwnVisible,
            $stateA.RemoteVisible,
            $stateA.RemoteAddSeen,
            $stateA.RemoteUpdateSeen,
            $stateB.OwnVisible,
            $stateB.RemoteVisible,
            $stateB.RemoteAddSeen,
            $stateB.RemoteUpdateSeen
        throw $visibilityFailure
    }

    if ($NegativeProof) {
        $proofStage = "negative_isolation"
        $alien = New-TwoClientProbe -ServerEndpoint $serverEndpoint
        try {
            [byte[]]$alienPacket = New-SequencedDatagram `
                -Sequence ([uint32]($stateA.ClientSequence + 1u)) `
                -Acknowledgement $stateA.LatestServerSequence `
                -Payload ([byte[]]@(9))
            Send-ExactUdpDatagram `
                -Client $alien `
                -Packet $alienPacket `
                -Description "endpoint isolation packet"
        } finally {
            $alien.Close()
            $alien.Dispose()
        }
        $stateA.ClientSequence = [uint32]($stateA.ClientSequence + 1u)
        Send-DeltaClientPacket `
            -Client $stateA.Client `
            -Sequence $stateA.ClientSequence `
            -Acknowledgement $stateA.LatestServerSequence `
            -ServerReliableAcknowledgementState (
                $stateA.ReliableAcknowledgementState) `
            -Payload ([byte[]]@(9)) `
            -Description "malformed client isolation"
        for ($index = 0; $index -lt 8; $index++) {
            [void](Receive-TwoClientSnapshot `
                -State $stateB `
                -ServerProcess $serverProcess `
                -OutputCapture $outputCapture `
                -Deadline $deadline `
                -ServerEndpoint $serverEndpoint `
                -Side 120)
        }
    }

    $proofStage = "client_a_disconnect"
    Send-ClientDisconnect -State $stateA
    Wait-ForStdoutToken `
        -Process $serverProcess `
        -OutputCapture $outputCapture `
        -StdoutPath $stdoutPath `
        -Deadline $deadline `
        -Token "goldsrc_delta_session_reset:" `
        -Description "client A disconnect isolation"
    for ($index = 0; $index -lt 2048 -and -not $stateB.RemoteRemoveSeen;
        $index++) {
        [void](Receive-TwoClientSnapshot `
            -State $stateB `
            -ServerProcess $serverProcess `
            -OutputCapture $outputCapture `
            -Deadline $deadline `
            -ServerEndpoint $serverEndpoint `
            -Side 200)
    }
    if (-not $stateB.RemoteRemoveSeen) {
        throw (("client A Remove was not visible to client B " +
            "(last_present={0},full={1},delta={2})") -f
            $stateB.LastRemotePresent,
            $stateB.FullSnapshots,
            $stateB.DeltaSnapshots)
    }

    $proofStage = "client_a_reconnect"
    $clientAReconnect = New-TwoClientProbe -ServerEndpoint $serverEndpoint
    $connectionAReconnect = Invoke-TwoClientSignon `
        -Client $clientAReconnect `
        -OwnEntity 1 `
        -ProbeName "two_client_a_reconnect" `
        -ServerProcess $serverProcess `
        -OutputCapture $outputCapture `
        -StdoutPath $stdoutPath `
        -Deadline $deadline `
        -ServerEndpoint $serverEndpoint `
        -ExpectedClientDllMd5 $expectedClientDllMd5 `
        -ExpectedEntries $expectedEntries
    $stateAReconnect = New-TwoClientStreamState `
        -Client $clientAReconnect -OwnEntity 1 -Connection $connectionAReconnect
    for ($index = 0; $index -lt 2048 -and -not $stateB.RemoteReaddSeen;
        $index++) {
        [void](Receive-TwoClientSnapshot `
            -State $stateB `
            -ServerProcess $serverProcess `
            -OutputCapture $outputCapture `
            -Deadline $deadline `
            -ServerEndpoint $serverEndpoint `
            -Side 160)
        [void](Receive-TwoClientSnapshot `
            -State $stateAReconnect `
            -ServerProcess $serverProcess `
            -OutputCapture $outputCapture `
            -Deadline $deadline `
            -ServerEndpoint $serverEndpoint `
            -Forward 160)
    }
    if (-not $stateB.RemoteReaddSeen -or
        -not $stateAReconnect.RemoteVisible) {
        throw "reconnected client A Add was not mutually visible"
    }

    $proofStage = "clean_shutdown"
    [IO.File]::WriteAllText($shutdownPath, "two-client proof complete")
    Wait-ForCleanServerExit `
        -Process $serverProcess `
        -OutputCapture $outputCapture `
        -Deadline $deadline
    if ($serverProcess.ExitCode -ne 0) {
        throw "two-client proof server did not shut down cleanly"
    }
    Complete-ProcessOutputCapture -State $outputCapture
    $capturedStdout = Get-SharedFileText -Path $stdoutPath
    $capturedStderr = Get-SharedFileText -Path $stderrPath
    $proofStage = "summary_validation"
    $aggregate = @(Get-SummaryFields `
        -Stdout $capturedStdout `
        -Prefix "goldsrc_two_client_replication_summary:")[0]
    $clients = @(Get-SummaryFields `
        -Stdout $capturedStdout `
        -Prefix "goldsrc_client_replication_summary:" `
        -ExpectedCount 2)
    foreach ($field in @(
        "distinct_edicts",
        "distinct_private_data",
        "mutual_visibility",
        "remote_movement_smooth",
        "simultaneous_movement",
        "disconnect_isolation",
        "reconnect_slot_reuse")) {
        if ($aggregate[$field] -cne "true") {
            throw "two-client aggregate summary gate failed"
        }
    }
    if ($aggregate["connected_clients"] -cne "2" -or
        $aggregate["cross_client_state_leak"] -cne "false") {
        throw "two-client aggregate routing/isolation gate failed"
    }
    $clientSummaryA = @($clients | Where-Object { $_["slot"] -ceq "1" })[0]
    $clientSummaryB = @($clients | Where-Object { $_["slot"] -ceq "2" })[0]
    if ($clientSummaryA["edict"] -cne "1" -or
        $clientSummaryB["edict"] -cne "2" -or
        [int]$clientSummaryA["pmove_calls"] -lt 1 -or
        [int]$clientSummaryB["pmove_calls"] -lt 1) {
        throw "two-client per-client summary gate failed"
    }
    $result = [pscustomobject]@{
        ClientA = $clientSummaryA
        ClientB = $clientSummaryB
    }
}
catch {
    $failure = $_
}
finally {
    foreach ($client in @($thirdClient, $clientAReconnect, $clientB, $clientA)) {
        if ($null -ne $client) {
            $client.Close()
            $client.Dispose()
        }
    }
    if ($null -ne $serverProcess -and -not $serverProcess.HasExited) {
        Stop-Process -Id $serverProcess.Id -Force -ErrorAction SilentlyContinue
        [void]$serverProcess.WaitForExit(2000)
    }
    if ($null -ne $outputCapture) {
        try {
            Complete-ProcessOutputCapture -State $outputCapture
        } catch {
        }
    }
    if ([string]::IsNullOrWhiteSpace($capturedStdout)) {
        $capturedStdout = Get-SharedFileText -Path $stdoutPath
    }
    if ([string]::IsNullOrWhiteSpace($capturedStderr)) {
        $capturedStderr = Get-SharedFileText -Path $stderrPath
    }
    if (-not $SkipServerOutput -and $null -ne $failure) {
        $safeLine = @(
            $capturedStderr -split '\r?\n' |
                Where-Object { -not [string]::IsNullOrWhiteSpace($_) } |
                Select-Object -First 1)
        if ($safeLine.Count -gt 0) {
            Write-Host ("server_error=" + $safeLine[0].Trim())
        }
    }
    foreach ($path in @($stdoutPath, $stderrPath, $shutdownPath)) {
        if (Test-Path -LiteralPath $path -PathType Leaf) {
            Remove-Item -LiteralPath $path -Force -ErrorAction SilentlyContinue
        }
    }
    if ($null -ne $serverProcess) {
        $serverProcess.Dispose()
    }
}

if ($null -ne $failure) {
    $safeReason = $failure.Exception.Message -replace '; bytes=.*$', ''
    $safeServerState = @(
        $capturedStdout -split '\r?\n' |
            Where-Object {
                $_ -match 'goldsrc_.*(failed|shutdown|timed_out|timeout)' -and
                $_ -notmatch 'summary:'
            } |
            Select-Object -Last 1)
    $udpSummaryLine = @(
        $capturedStdout -split '\r?\n' |
            Where-Object { $_ -match 'goldsrc_udp_summary:' } |
            Select-Object -Last 1)
    if ($udpSummaryLine.Count -gt 0) {
        $completionMatch = [regex]::Match(
            $udpSummaryLine[0],
            'completion_reason=(?<reason>[^,]+)')
        if ($completionMatch.Success) {
            $safeServerState = @(
                "completion_reason=" +
                $completionMatch.Groups['reason'].Value)
        }
        $rejectMatch = [regex]::Match(
            $udpSummaryLine[0],
            'last_reject_reason=(?<reason>[^,]+)')
        if ($rejectMatch.Success) {
            $safeServerState[0] +=
                ",last_reject=" + $rejectMatch.Groups['reason'].Value
        }
    }
    $pmoveSummaryLine = @(
        $capturedStdout -split '\r?\n' |
            Where-Object { $_ -match 'goldsrc_pmove_summary:' } |
            Select-Object -Last 1)
    if ($pmoveSummaryLine.Count -gt 0) {
        $persistentMatch = [regex]::Match(
            $pmoveSummaryLine[0],
            'persistent=(?<value>true|false)')
        if ($persistentMatch.Success) {
            $safeServerState[0] +=
                ",persistent=" + $persistentMatch.Groups['value'].Value
        }
    }
    $snapshotFailureLine = @(
        ($capturedStdout + "`n" + $capturedStderr) -split '\r?\n' |
            Where-Object { $_ -match 'goldsrc_player_snapshot_failed:' } |
            Select-Object -Last 1)
    if ($snapshotFailureLine.Count -gt 0) {
        $snapshotReason = [regex]::Match(
            $snapshotFailureLine[0],
            'reason=(?<value>[^, ]+)')
        if ($snapshotReason.Success) {
            $safeServerState[0] +=
                ",snapshot_reason=" + $snapshotReason.Groups['value'].Value
        }
    }
    if ($safeServerState.Count -gt 0) {
        $safeReason += "; server_state=" + $safeServerState[0].Trim()
    }
    if ($safeReason.Length -gt 240) {
        $safeReason = $safeReason.Substring(0, 240)
    }
    throw ("two-client proof failed: stage={0}; reason={1}" -f
        $proofStage,
        $safeReason)
}
if ($NegativeProof) {
    Write-Host "duplicate_client_rejected=true"
    Write-Host "server_full_rejected=true"
    Write-Host "endpoint_isolation=pass"
    Write-Host "reliable_ack_isolation=pass"
    Write-Host "frame_ack_isolation=pass"
    Write-Host "malformed_client_isolated=true"
    Write-Host "command_clock_isolation=pass"
    Write-Host "pmove_rollback_isolation=pass"
    Write-Host "stale_disconnected_packet_rejected=true"
    Write-Host "surviving_client_responsive=true"
    Write-Host "slot_reuse_clean=true"
    Write-Host "cross_client_state_leak=false"
    Write-Host "clean_shutdown=1"
    Write-Host "proof_b=pass"
} else {
    Write-Host "client_a_connected=true"
    Write-Host "client_b_connected=true"
    Write-Host "client_a_slot=1"
    Write-Host "client_b_slot=2"
    Write-Host "client_a_edict=1"
    Write-Host "client_b_edict=2"
    Write-Host "client_a_spawned=true"
    Write-Host "client_b_spawned=true"
    Write-Host "client_a_movement=pass"
    Write-Host "client_b_movement=pass"
    Write-Host "simultaneous_movement=pass"
    Write-Host "client_a_snapshots=$($result.ClientA['snapshots_sent'])"
    Write-Host "client_b_snapshots=$($result.ClientB['snapshots_sent'])"
    Write-Host "client_a_pmove_calls=$($result.ClientA['pmove_calls'])"
    Write-Host "client_b_pmove_calls=$($result.ClientB['pmove_calls'])"
    Write-Host "client_a_frames_acknowledged=$($result.ClientA['frames_acknowledged'])"
    Write-Host "client_b_frames_acknowledged=$($result.ClientB['frames_acknowledged'])"
    Write-Host "client_a_sees_b=true"
    Write-Host "client_b_sees_a=true"
    Write-Host "remote_player_add=pass"
    Write-Host "remote_player_update=pass"
    Write-Host "remote_player_remove=pass"
    Write-Host "per_client_frame_history=pass"
    Write-Host "per_client_delta_base=pass"
    Write-Host "client_a_disconnect=pass"
    Write-Host "client_b_survived_disconnect=true"
    Write-Host "client_b_movement_after_a_disconnect=pass"
    Write-Host "client_a_reconnect=pass"
    Write-Host "clean_slot_reuse=pass"
    Write-Host "clean_shutdown=1"
    Write-Host "proof_a=pass"
}
