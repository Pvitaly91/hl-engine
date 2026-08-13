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

    [switch]$CombatProof,

    [switch]$FeatureOffProof,

    [switch]$FallDamageProof,

    [switch]$LethalDeathProof,

    [switch]$SkipServerOutput
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
if (Test-Path Variable:PSNativeCommandUseErrorActionPreference) {
    $PSNativeCommandUseErrorActionPreference = $false
}
trap {
    Write-Host (
        'two-client proof failed: stage=preflight; ' +
        'reason=proof_gate_failed')
    exit 1
}
if ($BindAddress -cne "127.0.0.1") {
    throw "two-client proofs are restricted to 127.0.0.1"
}
if ($FeatureOffProof -and (-not $CombatProof -or $NegativeProof)) {
    throw "feature-off proof requires CombatProof without NegativeProof"
}
if ($FallDamageProof -and
    (-not $CombatProof -or $NegativeProof -or $FeatureOffProof)) {
    throw "fall damage proof requires only CombatProof"
}
if ($LethalDeathProof -and
    (-not $CombatProof -or $NegativeProof -or $FeatureOffProof -or
        $FallDamageProof)) {
    throw "lethal death proof requires only CombatProof"
}
$effectiveTimeoutSeconds = if ($LethalDeathProof -and
    -not $PSBoundParameters.ContainsKey('TimeoutSeconds')) {
    900
} else {
    $TimeoutSeconds
}
$script:goldsrcAllowWeaponData = [bool]($CombatProof -and -not $FeatureOffProof)
$script:twoClientMovementMsec = if ($LethalDeathProof) { 50 } else { 10 }
$script:goldsrcExpectedServerInfoMap = if ($CombatProof) {
    "maps/crossfire.bsp"
} else {
    "maps/c0a0.bsp"
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
$ExpectedZMaximum = if ($CombatProof) { [single]4096.0 } else { [single]6300.0 }
$ExpectedCdTrack = if ($CombatProof) { 0 } else { 3 }
$ExpectedSkyColorRed = if ($CombatProof) { [single]210.0 } else { [single]0.0 }
$ExpectedSkyColorGreen = if ($CombatProof) { [single]205.0 } else { [single]0.0 }
$ExpectedSkyColorBlue = if ($CombatProof) { [single]183.0 } else { [single]0.0 }
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
    $snapshotsByFrameId = @{}
    foreach ($candidate in $Connection.Resource.AcknowledgedSnapshotsByLow8.Values) {
        if ($null -ne $candidate) {
            $snapshotsByFrameId[[uint32]$candidate.FrameId] = $candidate
        }
    }
    if ($null -ne $latest) {
        $snapshotsByFrameId[[uint32]$latest.FrameId] = $latest
    }
    $frameReferencesByClientSequence = @{}
    $carriedReferences =
        $Connection.Resource.PSObject.Properties[
            'FrameReferencesByClientSequence']
    if ($null -ne $carriedReferences -and
        $null -ne $carriedReferences.Value) {
        foreach ($entry in $carriedReferences.Value.GetEnumerator()) {
            $frameReferencesByClientSequence[[uint32]$entry.Key] =
                [uint32]$entry.Value
        }
    }
    if ($null -ne $latest) {
        $frameReferencesByClientSequence[
            [uint32]$Connection.Resource.ClientSequence] =
                [uint32]$latest.FrameId
    }
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
        SnapshotsByFrameId = $snapshotsByFrameId
        PendingFrameReferences = $frameReferencesByClientSequence
        LatestServerAcknowledgement = $null
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
        [object[]]$ExpectedEntries,
        [switch]$ExactCombatBootstrap
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
        -ExpectedViewEntity $OwnEntity `
        -ExactCombat:$ExactCombatBootstrap
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

function Get-CombatSnapshotFrameEvidence {
    param(
        $OutputCapture,
        [string]$StdoutPath,
        [DateTime]$Deadline,
        [ValidateRange(1, 2)]
        [int]$Slot,
        [uint32]$FrameId
    )

    $pattern = 'goldsrc_combat_snapshot_frame: slot=' +
        [string]$Slot + ',frame_id=' + [string]$FrameId +
        ',base_frame=(?<base>none|[0-9]+),' +
        'health=(?<health>-?[0-9]+(?:\.[0-9]+)?),' +
        'clip=(?<clip>-?[0-9]+),' +
        'origin_x=(?<origin_x>-?[0-9]+(?:\.[0-9]+)?),' +
        'origin_y=(?<origin_y>-?[0-9]+(?:\.[0-9]+)?(?:e[-+]?[0-9]+)?)'
    if ([DateTime]::UtcNow -ge $Deadline) {
        throw "proof global budget exhausted"
    }
    $indexDeadline = [DateTime]::UtcNow.AddSeconds(5)
    if ($indexDeadline -gt $Deadline) {
        $indexDeadline = $Deadline
    }
    while ([DateTime]::UtcNow -lt $indexDeadline) {
        [void](Update-ProcessOutputCapture -State $OutputCapture)
        $frameKey = [string]$Slot + ':' + [string]$FrameId
        $line = if ($OutputCapture.CombatSnapshotFrameLines.ContainsKey(
                $frameKey)) {
            [string]$OutputCapture.CombatSnapshotFrameLines[$frameKey]
        } else {
            $null
        }
        $match = if ($null -ne $line) {
            [regex]::Match($line, $pattern)
        } else {
            $null
        }
        if ($null -ne $match -and $match.Success) {
            $base = $match.Groups['base'].Value
            return [pscustomobject]@{
                IsFull = $base -ceq 'none'
                BaseFrameId = $(if ($base -ceq 'none') {
                    [uint32]0
                } else {
                    [uint32]$base
                })
                Health = [double]$match.Groups['health'].Value
                Clip = [int]$match.Groups['clip'].Value
                OriginX = [double]$match.Groups['origin_x'].Value
                OriginY = [double]$match.Groups['origin_y'].Value
            }
        }
        Start-Sleep -Milliseconds 10
    }
    if ([DateTime]::UtcNow -ge $Deadline) {
        throw "proof global budget exhausted"
    }
    if ($Slot -eq 1) {
        throw "client a snapshot index idle timeout"
    }
    throw "client b snapshot index idle timeout"
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
        [int]$Buttons = 0,
        [single]$Yaw = 0.0,
        [switch]$DeferAcknowledgement,
        [byte[]]$DatagramBytes = $null
    )

    if ($ServerProcess.HasExited) {
        throw "two-client server unavailable"
    }
    if ([DateTime]::UtcNow -ge $Deadline) {
        throw "proof global budget exhausted"
    }
    $snapshotReceiveDeadline = [DateTime]::UtcNow.AddSeconds(5)
    if ($snapshotReceiveDeadline -gt $Deadline) {
        $snapshotReceiveDeadline = $Deadline
    }
    for ($attempt = 0; $attempt -lt 256; $attempt++) {
        if ($null -ne $DatagramBytes) {
            if ($attempt -ne 0) {
                throw "replayed two-client datagram was not a snapshot"
            }
            $datagram = [pscustomobject]@{
                Bytes = $DatagramBytes
            }
        } else {
            try {
                $datagram = Receive-ProofDatagram `
                    -Client $State.Client `
                    -ServerProcess $ServerProcess `
                    -OutputCapture $OutputCapture `
                    -Deadline $snapshotReceiveDeadline `
                    -ServerEndpoint $ServerEndpoint `
                    -Description "two-client continuous snapshot"
            } catch {
                if ($_.Exception.Message -ceq
                        'timeout waiting for two-client continuous snapshot') {
                    if ($ServerProcess.HasExited) {
                        throw "two-client server unavailable"
                    }
                    if ([DateTime]::UtcNow -ge $Deadline) {
                        throw "proof global budget exhausted"
                    }
                    if ($State.OwnEntity -eq 1) {
                        throw "client a snapshot idle timeout"
                    }
                    throw "client b snapshot idle timeout"
                }
                throw
            }
        }
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
        [uint32]$sequence = $packet.Sequence
        [uint32]$latestSequence = $State.LatestServerSequence
        if ($sequence -eq $latestSequence -or
            -not (Test-TwoClientSequenceNewer `
                -Candidate $sequence `
                -Baseline $latestSequence)) {
            if ($null -ne $DatagramBytes) {
                throw "replayed two-client snapshot is not newer than the local frontier"
            }
            continue
        }
        [uint32]$acknowledgement = $packet.Acknowledgement
        if ($null -ne $State.LatestServerAcknowledgement -and
            $acknowledgement -ne
                [uint32]$State.LatestServerAcknowledgement -and
            -not (Test-TwoClientSequenceNewer `
                -Candidate $acknowledgement `
                -Baseline ([uint32]$State.LatestServerAcknowledgement))) {
            throw "two-client server acknowledgement regressed"
        }
        $State.LatestServerAcknowledgement = $acknowledgement
        $State.LatestServerSequence = $sequence
        if ($packet.Payload.Length -eq 0 -or $packet.Payload[0] -eq $svcNop) {
            continue
        }
        if ($packet.ReliableToggle -or $packet.FragmentPresent -or
            $packet.Payload[0] -ne 7) {
            throw "two-client stream emitted an unexpected carrier"
        }

        $snapshot = $null
        $frameEvidence = $null
        try {
            $snapshot = Read-PlayerLifecycleFullSnapshotPayload `
                -Payload $packet.Payload `
                -FrameId ([uint32]$packet.Sequence) `
                -PreviousSnapshot $State.LatestSnapshot `
                -DeltaTables $script:twoClientDeltaTables `
                -MaximumClients 2 `
                -OwnEntity $State.OwnEntity
        } catch {
            $fullSnapshotFailure = $_
            try {
                $wireBase = Get-ContinuousSnapshotBaseLow8 -Payload $packet.Payload
            } catch {
                throw $fullSnapshotFailure
            }
            if ($script:goldsrcAllowWeaponData) {
                $frameEvidence = Get-CombatSnapshotFrameEvidence `
                    -OutputCapture $OutputCapture `
                    -StdoutPath $OutputCapture.StdoutPath `
                    -Deadline $Deadline `
                    -Slot $State.OwnEntity `
                    -FrameId ([uint32]$packet.Sequence)
                if ($frameEvidence.IsFull) {
                    throw "combat snapshot full decode fallback failed"
                }
                if (($frameEvidence.BaseFrameId -band [uint32]0xFF) -ne
                        [uint32]$wireBase) {
                    throw "combat snapshot base marker mismatch"
                }
                [uint32]$fullBaseFrameId =
                    Resolve-TwoClientAcknowledgedSnapshotBase `
                        -State $State `
                        -Acknowledgement $acknowledgement `
                        -CurrentFrameId $sequence `
                        -WireBaseLow8 ([byte]$wireBase) `
                        -ExpectedFrameId ([uint32]$frameEvidence.BaseFrameId)
            } else {
                [uint32]$fullBaseFrameId =
                    Resolve-TwoClientAcknowledgedSnapshotBase `
                        -State $State `
                        -Acknowledgement $acknowledgement `
                        -CurrentFrameId $sequence `
                        -WireBaseLow8 ([byte]$wireBase)
            }
            $snapshot = Read-PlayerLifecycleContinuousSnapshotPayload `
                -Payload $packet.Payload `
                -FrameId ([uint32]$packet.Sequence) `
                -BaseSnapshot $State.SnapshotsByFrameId[$fullBaseFrameId] `
                -DeltaTables $script:twoClientDeltaTables `
                -MaximumClients 2 `
                -OwnEntity $State.OwnEntity
        }
        if ($script:goldsrcAllowWeaponData) {
            if ($null -eq $frameEvidence) {
                $frameEvidence = Get-CombatSnapshotFrameEvidence `
                    -OutputCapture $OutputCapture `
                    -StdoutPath $OutputCapture.StdoutPath `
                    -Deadline $Deadline `
                    -Slot $State.OwnEntity `
                    -FrameId ([uint32]$packet.Sequence)
            }
            if (-not $snapshot.ClientDataValues.Contains('health') -or
                [Math]::Abs(
                    [double]$snapshot.ClientDataValues['health'] -
                        $frameEvidence.Health) -gt 0.01) {
                if ($snapshot.ClientDataChangedFields -contains 'health') {
                    throw "combat snapshot health changed-field decode mismatch"
                }
                throw "combat snapshot health base inheritance mismatch"
            }
            $snapshotGlockPresent =
                $snapshot.WeaponDataValues.Contains(2)
            if ($frameEvidence.Health -le 0.0) {
                $expectedDeadGlockPresent = $frameEvidence.Clip -ge 0
                if ($snapshotGlockPresent -ne $expectedDeadGlockPresent -or
                    ($expectedDeadGlockPresent -and
                        (-not $snapshot.WeaponDataValues[2].Contains('m_iClip') -or
                            [int]$snapshot.WeaponDataValues[2]['m_iClip'] -ne
                                $frameEvidence.Clip))) {
                    throw "combat death snapshot weapon removal mismatch"
                }
            } elseif (-not $snapshotGlockPresent -or
                -not $snapshot.WeaponDataValues[2].Contains('m_iClip') -or
                [int]$snapshot.WeaponDataValues[2]['m_iClip'] -ne
                    $frameEvidence.Clip) {
                if ($snapshot.WeaponDataChangedFields.Contains(2) -and
                    $snapshot.WeaponDataChangedFields[2] -contains
                        'm_iClip') {
                    throw "combat snapshot clip changed-field decode mismatch"
                }
                throw "combat snapshot clip base inheritance mismatch"
            }
            if (-not $snapshot.ClientDataValues.Contains('origin[0]') -or
                -not $snapshot.ClientDataValues.Contains('origin[1]') -or
                [Math]::Abs(
                    [double]$snapshot.ClientDataValues['origin[0]'] -
                        $frameEvidence.OriginX) -gt 0.13 -or
                [Math]::Abs(
                    [double]$snapshot.ClientDataValues['origin[1]'] -
                        $frameEvidence.OriginY) -gt 0.13) {
                throw "combat snapshot origin decode mismatch"
            }
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

        $State.LatestSnapshot = $snapshot
        $State.SnapshotsByLow8[
            [int]([uint32]$snapshot.FrameId -band 0xFF)] = $snapshot
        $State.SnapshotsByFrameId[[uint32]$snapshot.FrameId] = $snapshot
        if (-not $DeferAcknowledgement) {
            $State.ClientSequence = [uint32]($State.ClientSequence + 1)
            [byte[]]$frameReference = @(
                [byte]4,
                [byte]([uint32]$snapshot.FrameId -band 0xFF))
            [byte[]]$movement = New-GoldSrcMovementPayload `
                -Sequence $State.ClientSequence `
                -Msec $script:twoClientMovementMsec `
                -Forward $Forward `
                -Side $Side `
                -Buttons $Buttons `
                -Yaw $Yaw
            Send-DeltaClientPacket `
                -Client $State.Client `
                -Sequence $State.ClientSequence `
                -Acknowledgement $State.LatestServerSequence `
                -ServerReliableAcknowledgementState (
                    $State.ReliableAcknowledgementState) `
                -Payload ([byte[]]($frameReference + $movement)) `
                -Description "two-client frame acknowledgement and movement"
            $State.PendingFrameReferences[[uint32]$State.ClientSequence] =
                [uint32]$snapshot.FrameId
            $State.FrameAcknowledgements++
            $State.PmovePackets++
        }
        Trim-TwoClientStreamHistory -State $State
        return $snapshot
    }
    throw "two-client stream did not yield a snapshot"
}

function Test-TwoClientByteArraysEqual {
    param(
        [byte[]]$Left,
        [byte[]]$Right
    )

    if ($null -eq $Left -or $null -eq $Right -or
        $Left.Length -ne $Right.Length) {
        return $false
    }
    for ($index = 0; $index -lt $Left.Length; $index++) {
        if ($Left[$index] -ne $Right[$index]) {
            return $false
        }
    }
    return $true
}

function Get-TwoClientSequenceDistance {
    param(
        [uint32]$Candidate,
        [uint32]$Baseline
    )

    [uint64]$modulus = [uint64]$sequenceMask + 1
    [uint64]$candidateMasked =
        [uint64]$Candidate -band [uint64]$sequenceMask
    [uint64]$baselineMasked =
        [uint64]$Baseline -band [uint64]$sequenceMask
    return [uint64](
        ($candidateMasked + $modulus - $baselineMasked) % $modulus)
}

function Test-TwoClientSequenceNewer {
    param(
        [uint32]$Candidate,
        [uint32]$Baseline
    )

    [uint64]$halfRange = ([uint64]$sequenceMask + 1) / 2
    [uint64]$distance = Get-TwoClientSequenceDistance `
        -Candidate $Candidate `
        -Baseline $Baseline
    return $distance -gt 0 -and $distance -lt $halfRange
}

$script:twoClientHistoryWindow = 512

function Trim-TwoClientStreamHistory {
    param($State)

    if ($null -eq $State) {
        return
    }
    [uint32]$latestClientSequence = $State.ClientSequence
    foreach ($key in @($State.PendingFrameReferences.Keys)) {
        [uint32]$clientSequence = $key
        [uint64]$distance = Get-TwoClientSequenceDistance `
            -Candidate $latestClientSequence `
            -Baseline $clientSequence
        if ($distance -ge [uint64]$script:twoClientHistoryWindow) {
            [void]$State.PendingFrameReferences.Remove($key)
        }
    }

    $referencedFrames = @{}
    foreach ($frameId in $State.PendingFrameReferences.Values) {
        $referencedFrames[[uint32]$frameId] = $true
    }
    [uint32]$latestServerSequence = $State.LatestServerSequence
    foreach ($key in @($State.SnapshotsByFrameId.Keys)) {
        [uint32]$frameId = $key
        [uint64]$distance = Get-TwoClientSequenceDistance `
            -Candidate $latestServerSequence `
            -Baseline $frameId
        if ($distance -ge [uint64]$script:twoClientHistoryWindow -and
            -not $referencedFrames.ContainsKey($frameId)) {
            [void]$State.SnapshotsByFrameId.Remove($key)
        }
    }
}

function Assert-TwoClientStreamHistoryBound {
    $snapshots = @{}
    $references = @{}
    for ([uint32]$sequence = 1; $sequence -le 10000; $sequence++) {
        $snapshots[$sequence] = [pscustomobject]@{ FrameId = $sequence }
        $references[$sequence] = $sequence
    }
    $state = [pscustomobject]@{
        ClientSequence = [uint32]10000
        LatestServerSequence = [uint32]10000
        PendingFrameReferences = $references
        SnapshotsByFrameId = $snapshots
    }
    Trim-TwoClientStreamHistory -State $state
    if ($state.PendingFrameReferences.Count -gt
            $script:twoClientHistoryWindow -or
        $state.SnapshotsByFrameId.Count -gt
            ($script:twoClientHistoryWindow * 2) -or
        $state.PendingFrameReferences.ContainsKey([uint32]1) -or
        $state.SnapshotsByFrameId.ContainsKey([uint32]1)) {
        throw "two-client stream history bound validation failed"
    }
    [uint32]$expected = 9999
    [uint32]$resolved = Resolve-TwoClientAcknowledgedSnapshotBase `
        -State $state `
        -Acknowledgement ([uint32]10000) `
        -CurrentFrameId ([uint32]10001) `
        -WireBaseLow8 ([byte]($expected -band 0xFF)) `
        -ExpectedFrameId $expected
    if ($resolved -ne $expected) {
        throw "two-client stream history exact base validation failed"
    }
    $staleRejected = $false
    try {
        [void](Resolve-TwoClientAcknowledgedSnapshotBase `
            -State $state `
            -Acknowledgement ([uint32]10000) `
            -CurrentFrameId ([uint32]10001) `
            -WireBaseLow8 ([byte]1) `
            -ExpectedFrameId ([uint32]1))
    } catch {
        $staleRejected = (
            $_.Exception.Message -eq
                "combat snapshot exact base client reference not acknowledged")
        if (-not $staleRejected) {
            throw "two-client stream history stale base validation failed"
        }
    }
    if (-not $staleRejected) {
        throw "two-client stream history stale base validation failed"
    }
}

function Resolve-TwoClientAcknowledgedSnapshotBase {
    param(
        $State,
        [uint32]$Acknowledgement,
        [uint32]$CurrentFrameId,
        [byte]$WireBaseLow8,
        [object]$ExpectedFrameId = $null
    )

    $coveredFrameIds = @{}
    $knownFrameIds = @{}
    $matchingLow8FrameIds = @{}
    $candidateFrameIds = @{}
    foreach ($entry in $State.PendingFrameReferences.GetEnumerator()) {
        [uint32]$clientSequence = $entry.Key
        if ($clientSequence -ne $Acknowledgement -and
            -not (Test-TwoClientSequenceNewer `
                -Candidate $Acknowledgement `
                -Baseline $clientSequence)) {
            continue
        }
        [uint32]$frameId = $entry.Value
        $coveredFrameIds[$frameId] = $true
        if (-not $State.SnapshotsByFrameId.ContainsKey($frameId)) {
            continue
        }
        $knownFrameIds[$frameId] = $true
        if (($frameId -band [uint32]0xFF) -ne
                [uint32]$WireBaseLow8) {
            continue
        }
        $matchingLow8FrameIds[$frameId] = $true
        [uint64]$distance = Get-TwoClientSequenceDistance `
            -Candidate $CurrentFrameId `
            -Baseline $frameId
        if ($distance -eq 0 -or $distance -gt 255) {
            continue
        }
        $candidateFrameIds[$frameId] = $true
    }
    if ($null -ne $ExpectedFrameId) {
        [uint32]$expected = [uint32]$ExpectedFrameId
        if (-not $coveredFrameIds.ContainsKey($expected)) {
            throw "combat snapshot exact base client reference not acknowledged"
        }
        if (-not $knownFrameIds.ContainsKey($expected)) {
            throw "combat snapshot exact base is not locally materialized"
        }
        if (($expected -band [uint32]0xFF) -ne
                [uint32]$WireBaseLow8) {
            throw "combat snapshot exact base low8 mismatch"
        }
        if (-not (Test-TwoClientSequenceNewer `
                -Candidate $CurrentFrameId `
                -Baseline $expected)) {
            throw "combat snapshot exact base is not older"
        }
        return $expected
    }
    if ($candidateFrameIds.Count -ne 1) {
        $boundedCovered = [Math]::Min(255, $coveredFrameIds.Count)
        $boundedKnown = [Math]::Min(255, $knownFrameIds.Count)
        $boundedLow8 = [Math]::Min(255, $matchingLow8FrameIds.Count)
        $boundedCandidates = [Math]::Min(255, $candidateFrameIds.Count)
        throw ((
            'two-client delta selected no exact acknowledged frame ' +
            'covered{0} known{1} low8{2} bounded{3}') -f
                $boundedCovered,
                $boundedKnown,
                $boundedLow8,
                $boundedCandidates)
    }
    [uint32]$resolvedFrameId = @($candidateFrameIds.Keys)[0]
    return $resolvedFrameId
}

function Restore-TwoClientSnapshotBacklog {
    param(
        $State,
        [hashtable]$Packets,
        [uint32]$TargetFrameId,
        [System.Diagnostics.Process]$ServerProcess,
        $OutputCapture,
        [string]$StdoutPath,
        [DateTime]$Deadline,
        [System.Net.IPEndPoint]$ServerEndpoint
    )

    if ($Packets.Count -eq 0 -or
        -not $Packets.ContainsKey([uint32]$TargetFrameId)) {
        throw "two-client backlog target packet is unavailable"
    }
    if (-not $script:goldsrcAllowWeaponData) {
        [byte[]]$targetBytes = $Packets[$TargetFrameId]
        $targetPacket = Read-SequencedDatagram `
            -Packet $targetBytes `
            -Description "two-client noncombat backlog target"
        if ([uint32]$targetPacket.Sequence -ne $TargetFrameId -or
            $targetPacket.ReliableToggle -or
            $targetPacket.FragmentPresent -or
            $targetPacket.Payload.Length -eq 0 -or
            $targetPacket.Payload[0] -ne 7) {
            throw "two-client noncombat backlog target was not a snapshot"
        }

        $targetDecodable = $false
        try {
            [void](Read-PlayerLifecycleFullSnapshotPayload `
                -Payload $targetPacket.Payload `
                -FrameId $TargetFrameId `
                -PreviousSnapshot $State.LatestSnapshot `
                -DeltaTables $script:twoClientDeltaTables `
                -MaximumClients 2 `
                -OwnEntity $State.OwnEntity)
            $targetDecodable = $true
        } catch {
            $targetFullFailure = $_
            try {
                [byte]$wireBase = Get-ContinuousSnapshotBaseLow8 `
                    -Payload $targetPacket.Payload
            } catch {
                throw $targetFullFailure
            }

            [uint32]$fullBaseFrameId = 0
            $baseResolved = $false
            try {
                $fullBaseFrameId =
                    Resolve-TwoClientAcknowledgedSnapshotBase `
                        -State $State `
                        -Acknowledgement (
                            [uint32]$targetPacket.Acknowledgement) `
                        -CurrentFrameId $TargetFrameId `
                        -WireBaseLow8 $wireBase
                $baseResolved = $true
            } catch {
                $baseResolved = $false
            }
            if ($baseResolved) {
                [void](Read-PlayerLifecycleContinuousSnapshotPayload `
                    -Payload $targetPacket.Payload `
                    -FrameId $TargetFrameId `
                    -BaseSnapshot (
                        $State.SnapshotsByFrameId[$fullBaseFrameId]) `
                    -DeltaTables $script:twoClientDeltaTables `
                    -MaximumClients 2 `
                    -OwnEntity $State.OwnEntity)
                $targetDecodable = $true
            }
        }

        [uint32]$restoreFrameId = $TargetFrameId
        if (-not $targetDecodable) {
            $newestFullFrameId = $null
            foreach ($candidate in $Packets.GetEnumerator()) {
                [uint32]$candidateFrameId = $candidate.Key
                if ($candidateFrameId -eq
                        [uint32]$State.LatestServerSequence -or
                    -not (Test-TwoClientSequenceNewer `
                        -Candidate $candidateFrameId `
                        -Baseline ([uint32]$State.LatestServerSequence))) {
                    continue
                }
                if ($null -ne $newestFullFrameId -and
                    -not (Test-TwoClientSequenceNewer `
                        -Candidate $candidateFrameId `
                        -Baseline ([uint32]$newestFullFrameId))) {
                    continue
                }
                $candidatePacket = Read-SequencedDatagram `
                    -Packet ([byte[]]$candidate.Value) `
                    -Description "two-client noncombat backlog full candidate"
                if ([uint32]$candidatePacket.Sequence -ne $candidateFrameId -or
                    $candidatePacket.ReliableToggle -or
                    $candidatePacket.FragmentPresent -or
                    $candidatePacket.Payload.Length -eq 0 -or
                    $candidatePacket.Payload[0] -ne 7) {
                    throw "two-client noncombat backlog candidate was not a snapshot"
                }
                try {
                    [void](Read-PlayerLifecycleFullSnapshotPayload `
                        -Payload $candidatePacket.Payload `
                        -FrameId $candidateFrameId `
                        -PreviousSnapshot $State.LatestSnapshot `
                        -DeltaTables $script:twoClientDeltaTables `
                        -MaximumClients 2 `
                        -OwnEntity $State.OwnEntity)
                    $newestFullFrameId = $candidateFrameId
                } catch {
                    continue
                }
            }
            if ($null -eq $newestFullFrameId) {
                throw "two-client noncombat backlog has no recoverable full snapshot"
            }
            $restoreFrameId = [uint32]$newestFullFrameId
        }

        [void](Receive-TwoClientSnapshot `
            -State $State `
            -ServerProcess $ServerProcess `
            -OutputCapture $OutputCapture `
            -Deadline $Deadline `
            -ServerEndpoint $ServerEndpoint `
            -DatagramBytes ([byte[]]$Packets[$restoreFrameId]))
        if ([uint32]$State.LatestSnapshot.FrameId -ne $restoreFrameId) {
            throw "two-client backlog target was not materialized"
        }
        return
    }
    [uint32]$currentFrameId = $TargetFrameId
    $decodeChain = New-Object 'System.Collections.Generic.List[uint32]'
    $visited = New-Object 'System.Collections.Generic.HashSet[uint32]'
    $resolved = $false
    for ($depth = 0; $depth -lt 64; $depth++) {
        if ($State.SnapshotsByFrameId.ContainsKey($currentFrameId)) {
            $resolved = $true
            break
        }
        if (-not $visited.Add($currentFrameId)) {
            throw "two-client backlog base chain contains a cycle"
        }
        if (-not $Packets.ContainsKey($currentFrameId)) {
            throw "two-client backlog base packet is unavailable"
        }
        $decodeChain.Add($currentFrameId)
        $evidence = Get-CombatSnapshotFrameEvidence `
            -OutputCapture $OutputCapture `
            -StdoutPath $StdoutPath `
            -Deadline $Deadline `
            -Slot $State.OwnEntity `
            -FrameId $currentFrameId
        if ($evidence.IsFull) {
            $resolved = $true
            break
        }
        $currentFrameId = [uint32]$evidence.BaseFrameId
    }
    if (-not $resolved) {
        throw "two-client backlog base chain exceeded its bound"
    }
    for ($index = $decodeChain.Count - 1; $index -ge 0; $index--) {
        [uint32]$frameId = $decodeChain[$index]
        [void](Receive-TwoClientSnapshot `
            -State $State `
            -ServerProcess $ServerProcess `
            -OutputCapture $OutputCapture `
            -Deadline $Deadline `
            -ServerEndpoint $ServerEndpoint `
            -DeferAcknowledgement `
            -DatagramBytes ([byte[]]$Packets[$frameId]))
    }
    if ([uint32]$State.LatestSnapshot.FrameId -ne $TargetFrameId) {
        if (-not $State.SnapshotsByFrameId.ContainsKey($TargetFrameId)) {
            throw "two-client backlog target was not materialized"
        }
        $State.LatestSnapshot = $State.SnapshotsByFrameId[$TargetFrameId]
    }
}

function Drain-TwoClientSnapshotBacklog {
    param(
        $StateA,
        $StateB,
        [System.Diagnostics.Process]$ServerProcess,
        $OutputCapture,
        [string]$StdoutPath,
        [DateTime]$Deadline,
        [System.Net.IPEndPoint]$ServerEndpoint
    )

    $packetsA = @{}
    $packetsB = @{}
    $targetFrameA = $null
    $targetFrameB = $null
    $lastSequenceA = if ($null -eq $StateA) {
        $null
    } else {
        [uint32]$StateA.LatestServerSequence
    }
    $lastSequenceB = if ($null -eq $StateB) {
        $null
    } else {
        [uint32]$StateB.LatestServerSequence
    }
    $drainComplete = $false
    for ($iteration = 0; $iteration -lt 4096; $iteration++) {
        if ([DateTime]::UtcNow -ge $Deadline) {
            throw "two-client snapshot backlog drain timed out"
        }
        if ($ServerProcess.HasExited) {
            throw "two-client snapshot backlog server exited"
        }
        if (($iteration -band 63) -eq 0) {
            [void](Update-ProcessOutputCapture -State $OutputCapture)
        }
        $drained = $false
        for ($stateIndex = 0; $stateIndex -lt 2; $stateIndex++) {
            $state = if ($stateIndex -eq 0) { $StateA } else { $StateB }
            $packets = if ($stateIndex -eq 0) { $packetsA } else { $packetsB }
            if ($null -eq $state) {
                continue
            }
            if ($state.Client.Available -le 0) {
                continue
            }
            $sender = New-Object System.Net.IPEndPoint(
                [System.Net.IPAddress]::Any,
                0)
            [byte[]]$bytes = $state.Client.Receive([ref]$sender)
            if (-not $sender.Address.Equals($ServerEndpoint.Address) -or
                $sender.Port -ne $ServerEndpoint.Port) {
                throw "two-client backlog source isolation failed"
            }
            $drained = $true
            if ($bytes.Length -gt 2048) {
                throw "two-client backlog datagram exceeded its bound"
            }
            if ($bytes.Length -ge 4 -and
                $bytes[0] -eq 0xFF -and
                $bytes[1] -eq 0xFF -and
                $bytes[2] -eq 0xFF -and
                $bytes[3] -eq 0xFF) {
                continue
            }
            $packet = Read-SequencedDatagram `
                -Packet $bytes `
                -Description "two-client queued snapshot"
            [uint32]$sequence = $packet.Sequence
            if ($stateIndex -eq 0) {
                if ($null -eq $lastSequenceA -or
                    (Test-TwoClientSequenceNewer `
                        -Candidate $sequence `
                        -Baseline ([uint32]$lastSequenceA))) {
                    $lastSequenceA = $sequence
                }
            } else {
                if ($null -eq $lastSequenceB -or
                    (Test-TwoClientSequenceNewer `
                        -Candidate $sequence `
                        -Baseline ([uint32]$lastSequenceB))) {
                    $lastSequenceB = $sequence
                }
            }
            if ($packet.Payload.Length -eq 0 -or
                $packet.Payload[0] -eq $svcNop) {
                continue
            }
            if ($packet.ReliableToggle -or $packet.FragmentPresent -or
                $packet.Payload[0] -ne 7) {
                throw "two-client backlog emitted an unexpected carrier"
            }
            if ($packets.ContainsKey($sequence) -and
                -not (Test-TwoClientByteArraysEqual `
                    -Left ([byte[]]$packets[$sequence]) `
                    -Right $bytes)) {
                throw "two-client backlog sequence had conflicting bytes"
            }
            $packets[$sequence] = $bytes
            if ($stateIndex -eq 0) {
                $currentFrame = if ($null -eq $targetFrameA) {
                    $StateA.LatestSnapshot
                } else {
                    $null
                }
                $baselineFrameId = if ($null -ne $targetFrameA) {
                    [uint32]$targetFrameA
                } elseif ($null -ne $currentFrame) {
                    [uint32]$currentFrame.FrameId
                } else {
                    $null
                }
                if ($null -eq $baselineFrameId -or
                    (Test-TwoClientSequenceNewer `
                        -Candidate $sequence `
                        -Baseline ([uint32]$baselineFrameId))) {
                    $targetFrameA = $sequence
                }
            } else {
                $currentFrame = if ($null -eq $targetFrameB) {
                    $StateB.LatestSnapshot
                } else {
                    $null
                }
                $baselineFrameId = if ($null -ne $targetFrameB) {
                    [uint32]$targetFrameB
                } elseif ($null -ne $currentFrame) {
                    [uint32]$currentFrame.FrameId
                } else {
                    $null
                }
                if ($null -eq $baselineFrameId -or
                    (Test-TwoClientSequenceNewer `
                        -Candidate $sequence `
                        -Baseline ([uint32]$baselineFrameId))) {
                    $targetFrameB = $sequence
                }
            }
        }
        if (-not $drained) {
            $drainComplete = $true
            break
        }
    }
    if (-not $drainComplete) {
        throw "two-client snapshot backlog exceeded the bounded drain"
    }
    if ($null -ne $targetFrameA) {
        Restore-TwoClientSnapshotBacklog `
            -State $StateA `
            -Packets $packetsA `
            -TargetFrameId ([uint32]$targetFrameA) `
            -ServerProcess $ServerProcess `
            -OutputCapture $OutputCapture `
            -StdoutPath $StdoutPath `
            -Deadline $Deadline `
            -ServerEndpoint $ServerEndpoint
    }
    if ($null -ne $StateB -and $null -ne $targetFrameB) {
        Restore-TwoClientSnapshotBacklog `
            -State $StateB `
            -Packets $packetsB `
            -TargetFrameId ([uint32]$targetFrameB) `
            -ServerProcess $ServerProcess `
            -OutputCapture $OutputCapture `
            -StdoutPath $StdoutPath `
            -Deadline $Deadline `
            -ServerEndpoint $ServerEndpoint
    }
    if ($null -ne $lastSequenceA) {
        $StateA.LatestServerSequence = [uint32]$lastSequenceA
    }
    if ($null -ne $StateB -and $null -ne $lastSequenceB) {
        $StateB.LatestServerSequence = [uint32]$lastSequenceB
    }
}

function Send-TwoClientSimultaneousMove {
    param(
        $State,
        [int]$Forward,
        [int]$Side
    )

    $State.ClientSequence = [uint32]($State.ClientSequence + 1)
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
    $State.PendingFrameReferences[[uint32]$State.ClientSequence] =
        [uint32]$State.LatestSnapshot.FrameId
    $State.FrameAcknowledgements++
    $State.PmovePackets++
    Trim-TwoClientStreamHistory -State $State
}

function Send-TwoClientCombatMove {
    param(
        $State,
        [single]$Yaw,
        [ValidateRange(0, 65535)]
        [int]$Buttons,
        [ValidateRange(1, 255)]
        [byte]$Msec = 10,
        [ValidateRange(0, 61)]
        [byte]$Backups = 0,
        [ValidateRange(1, 62)]
        [byte]$Fresh = 1,
        [switch]$CorruptChecksum,
        [switch]$Malformed
    )

    $State.ClientSequence = [uint32]($State.ClientSequence + 1)
    [byte[]]$frameReference = @(
        [byte]4,
        [byte]([uint32]$State.LatestSnapshot.FrameId -band 0xFF))
    [byte[]]$movement = if ($Malformed) {
        [byte[]]@(2, 255, 0)
    } else {
        New-GoldSrcMovementPayload `
            -Sequence $State.ClientSequence `
            -Msec $Msec `
            -Buttons $Buttons `
            -Yaw $Yaw `
            -Backups $Backups `
            -Fresh $Fresh
    }
    if ($CorruptChecksum -and $movement.Length -ge 3) {
        $movement[2] = [byte]($movement[2] -bxor 0x5A)
    }
    [byte[]]$payload = [byte[]]($frameReference + $movement)
    $packetArguments = @{
        Sequence = $State.ClientSequence
        Acknowledgement = $State.LatestServerSequence
        Payload = $payload
    }
    if ($State.ReliableAcknowledgementState) {
        $packetArguments.ReliableAcknowledgementToggle = $true
    }
    [byte[]]$packet = New-SequencedDatagram @packetArguments
    Send-ExactUdpDatagram `
        -Client $State.Client `
        -Packet $packet `
        -Description "two-client combat movement"
    $State.PendingFrameReferences[[uint32]$State.ClientSequence] =
        [uint32]$State.LatestSnapshot.FrameId
    $State.FrameAcknowledgements++
    $State.PmovePackets++
    Trim-TwoClientStreamHistory -State $State
    return ,$packet
}

function Send-TwoClientCombatRecoveryMove {
    param(
        $State,
        [ValidateRange(0, 65535)]
        [int]$LastButtons,
        [ValidateRange(0, 65535)]
        [int]$FreshButtons = 0
    )

    $State.ClientSequence = [uint32]($State.ClientSequence + 1)
    [byte[]]$frameReference = @(
        [byte]4,
        [byte]([uint32]$State.LatestSnapshot.FrameId -band 0xFF))
    [byte[]]$movement = New-GoldSrcRecoveryMovementPayload `
        -Sequence $State.ClientSequence `
        -LastForward 0 `
        -LastSide 0 `
        -LastButtons ([uint16]$LastButtons) `
        -FreshForward 0 `
        -FreshSide 0 `
        -FreshButtons ([uint16]$FreshButtons)
    $packetArguments = @{
        Sequence = $State.ClientSequence
        Acknowledgement = $State.LatestServerSequence
        Payload = [byte[]]($frameReference + $movement)
    }
    if ($State.ReliableAcknowledgementState) {
        $packetArguments.ReliableAcknowledgementToggle = $true
    }
    Send-ExactUdpDatagram `
        -Client $State.Client `
        -Packet (New-SequencedDatagram @packetArguments) `
        -Description "two-client combat recovery movement"
    $State.PendingFrameReferences[[uint32]$State.ClientSequence] =
        [uint32]$State.LatestSnapshot.FrameId
    $State.FrameAcknowledgements++
    $State.PmovePackets++
    Trim-TwoClientStreamHistory -State $State
}

function Send-TwoClientExactAttackBackupMove {
    param(
        $State,
        [single]$Yaw,
        [ValidateRange(1, 255)]
        [byte]$Msec = 10
    )

    $State.ClientSequence = [uint32]($State.ClientSequence + 1)
    [double]$normalizedYaw = [double]$Yaw % 360.0
    if ($normalizedYaw -lt 0.0) {
        $normalizedYaw += 360.0
    }
    [uint16]$yawBits = [uint16][Math]::Floor(
        $normalizedYaw * 65536.0 / 360.0)
    [byte[]]$frameReference = @(
        [byte]4,
        [byte]([uint32]$State.LatestSnapshot.FrameId -band 0xFF))
    [byte[]]$movement = [GoldSrcMoveProofCodec]::BuildExactAttackBackup(
        $State.ClientSequence,
        $Msec,
        [uint16]1,
        $yawBits)
    $packetArguments = @{
        Sequence = $State.ClientSequence
        Acknowledgement = $State.LatestServerSequence
        Payload = [byte[]]($frameReference + $movement)
    }
    if ($State.ReliableAcknowledgementState) {
        $packetArguments.ReliableAcknowledgementToggle = $true
    }
    Send-ExactUdpDatagram `
        -Client $State.Client `
        -Packet (New-SequencedDatagram @packetArguments) `
        -Description "exact combat attack backup"
    $State.PendingFrameReferences[[uint32]$State.ClientSequence] =
        [uint32]$State.LatestSnapshot.FrameId
    $State.FrameAcknowledgements++
    $State.PmovePackets++
    Trim-TwoClientStreamHistory -State $State
}

function Get-CombatWireState {
    param($State)

    $snapshot = $State.LatestSnapshot
    if ($null -eq $snapshot -or
        $null -eq $snapshot.PSObject.Properties['ClientDataValues'] -or
        $null -eq $snapshot.PSObject.Properties['WeaponDataValues']) {
        throw "combat semantic snapshot evidence is unavailable"
    }
    $clientData = $snapshot.ClientDataValues
    if ($null -eq $clientData -or -not $clientData.Contains('health') -or
        -not $clientData.Contains('origin[0]') -or
        -not $clientData.Contains('origin[1]')) {
        throw "combat clientdata health evidence is unavailable"
    }
    $weaponData = $snapshot.WeaponDataValues
    if ($null -eq $weaponData -or -not $weaponData.Contains(2)) {
        throw "combat Glock weapondata evidence is unavailable"
    }
    $glockData = $weaponData[2]
    if ($null -eq $glockData -or -not $glockData.Contains('m_iClip')) {
        throw "combat Glock clip evidence is unavailable"
    }
    [double]$health = $clientData['health']
    [double]$originX = $clientData['origin[0]']
    [double]$originY = $clientData['origin[1]']
    [double]$clipValue = $glockData['m_iClip']
    if ([double]::IsNaN($health) -or [double]::IsInfinity($health) -or
        [double]::IsNaN($originX) -or [double]::IsInfinity($originX) -or
        [double]::IsNaN($originY) -or [double]::IsInfinity($originY) -or
        [double]::IsNaN($clipValue) -or
        [double]::IsInfinity($clipValue) -or
        $clipValue -ne [Math]::Truncate($clipValue)) {
        throw "combat semantic snapshot evidence is invalid"
    }
    return [pscustomobject]@{
        Health = $health
        OriginX = $originX
        OriginY = $originY
        Clip = [int]$clipValue
        FrameId = [uint32]$snapshot.FrameId
    }
}

function Get-CombatLifeWireState {
    param($State)

    $snapshot = $State.LatestSnapshot
    if ($null -eq $snapshot -or
        $null -eq $snapshot.PSObject.Properties['ClientDataValues'] -or
        $null -eq $snapshot.PSObject.Properties['WeaponDataValues']) {
        throw "combat life snapshot evidence is unavailable"
    }
    $clientData = $snapshot.ClientDataValues
    if ($null -eq $clientData -or -not $clientData.Contains('health') -or
        -not $clientData.Contains('origin[0]') -or
        -not $clientData.Contains('origin[1]')) {
        throw "combat life clientdata evidence is unavailable"
    }
    [double]$health = $clientData['health']
    [double]$originX = $clientData['origin[0]']
    [double]$originY = $clientData['origin[1]']
    if ([double]::IsNaN($health) -or [double]::IsInfinity($health) -or
        [double]::IsNaN($originX) -or [double]::IsInfinity($originX) -or
        [double]::IsNaN($originY) -or [double]::IsInfinity($originY)) {
        throw "combat life snapshot evidence is invalid"
    }
    return [pscustomobject]@{
        Health = $health
        OriginX = $originX
        OriginY = $originY
        GlockPresent = $snapshot.WeaponDataValues.Contains(2)
        FrameId = [uint32]$snapshot.FrameId
    }
}

function Get-FeatureOffWireState {
    param($State)

    $snapshot = $State.LatestSnapshot
    if ($null -eq $snapshot -or
        $null -eq $snapshot.PSObject.Properties['ClientDataValues'] -or
        $null -eq $snapshot.PSObject.Properties['WeaponDataValues'] -or
        -not $snapshot.ClientDataValues.Contains('health')) {
        throw "feature-off semantic snapshot evidence is unavailable"
    }
    [double]$health = $snapshot.ClientDataValues['health']
    if ([double]::IsNaN($health) -or [double]::IsInfinity($health)) {
        throw "feature-off clientdata health evidence is invalid"
    }
    return [pscustomobject]@{
        Health = $health
        WeaponDataPresent = ($snapshot.WeaponDataValues.Count -ne 0)
        FrameId = [uint32]$snapshot.FrameId
    }
}

function Get-LatestCombatProgress {
    param(
        $OutputCapture,
        [string]$StdoutPath,
        [ValidateRange(1, 2)]
        [int]$Slot
    )

    [void](Update-ProcessOutputCapture -State $OutputCapture)
    if (-not $OutputCapture.LatestGoldSrcProgressBySlot.ContainsKey($Slot)) {
        throw "combat movement progress evidence is unavailable"
    }
    $line = [string]$OutputCapture.LatestGoldSrcProgressBySlot[$Slot]
    $prefix = 'goldsrc_client_progress:'
    $offset = $line.IndexOf($prefix, [StringComparison]::Ordinal)
    if ($offset -lt 0) {
        throw "combat movement progress evidence is malformed"
    }
    $fields = @{}
    foreach ($part in $line.Substring($offset + $prefix.Length).Split(',')) {
        $separator = $part.IndexOf('=')
        if ($separator -gt 0) {
            $fields[$part.Substring(0, $separator).Trim()] =
                $part.Substring($separator + 1).Trim()
        }
    }
    foreach ($required in @(
            'connected',
            'signon_complete',
            'spawned',
            'clc_move_received',
            'clc_move_validated',
            'clc_move_executed',
            'pmove_calls',
            'forward_input_packets',
            'strafe_input_packets',
            'server_time_ms',
            'combat_attack_received',
            'combat_attack_executed',
            'combat_prethink',
            'combat_postthink',
            'combat_callback_failures',
            'combat_respawn_inputs_forwarded',
            'combat_duplicate_attack_suppressed',
            'combat_active_weapon',
            'combat_glock_present',
            'combat_shot_traces',
            'combat_shooter_ignored',
            'combat_shot_player_hits',
            'combat_shot_world_hits',
            'combat_last_target',
            'flags')) {
        if (-not $fields.ContainsKey($required) -or
            $fields[$required] -notmatch '^[0-9]+$') {
            throw "combat movement progress evidence is incomplete"
        }
    }
    if (-not $fields.ContainsKey('combat_glock_clip') -or
        $fields['combat_glock_clip'] -notmatch '^-?[0-9]+$' -or
        -not $fields.ContainsKey('health') -or
        $fields['health'] -notmatch '^-?[0-9]+(?:\.[0-9]+)?$' -or
        -not $fields.ContainsKey('combat_clientdata_health') -or
        $fields['combat_clientdata_health'] -notmatch
            '^-?[0-9]+(?:\.[0-9]+)?$' -or
        -not $fields.ContainsKey('deadflag') -or
        $fields['deadflag'] -notmatch '^-?[0-9]+$' -or
        -not $fields.ContainsKey('combat_phase') -or
        $fields['combat_phase'] -notmatch '^[a-z_]+$' -or
        -not $fields.ContainsKey('origin_x') -or
        $fields['origin_x'] -notmatch '^-?[0-9]+(?:\.[0-9]+)?$' -or
        -not $fields.ContainsKey('origin_y') -or
        $fields['origin_y'] -notmatch '^-?[0-9]+(?:\.[0-9]+)?$' -or
        -not $fields.ContainsKey('origin_z') -or
        $fields['origin_z'] -notmatch '^-?[0-9]+(?:\.[0-9]+)?$') {
        throw "combat progress state evidence is incomplete"
    }
    return [pscustomobject]@{
        Connected = [int]$fields['connected'] -eq 1
        SignonComplete = [int]$fields['signon_complete'] -eq 1
        Spawned = [int]$fields['spawned'] -eq 1
        MoveReceived = [uint64]$fields['clc_move_received']
        MoveValidated = [uint64]$fields['clc_move_validated']
        MoveExecuted = [uint64]$fields['clc_move_executed']
        PmoveCalls = [uint64]$fields['pmove_calls']
        ForwardInputs = [uint64]$fields['forward_input_packets']
        StrafeInputs = [uint64]$fields['strafe_input_packets']
        ServerTimeMs = [uint64]$fields['server_time_ms']
        AttackReceived = [uint64]$fields['combat_attack_received']
        AttackExecuted = [uint64]$fields['combat_attack_executed']
        PreThink = [uint64]$fields['combat_prethink']
        PostThink = [uint64]$fields['combat_postthink']
        CallbackFailures = [uint64]$fields['combat_callback_failures']
        RespawnInputsForwarded =
            [uint64]$fields['combat_respawn_inputs_forwarded']
        DuplicateAttackSuppressed =
            [uint64]$fields['combat_duplicate_attack_suppressed']
        ActiveWeapon = [int]$fields['combat_active_weapon']
        GlockPresent = [int]$fields['combat_glock_present'] -eq 1
        GlockClip = [int]$fields['combat_glock_clip']
        ShotTraces = [uint64]$fields['combat_shot_traces']
        ShooterIgnored = [uint64]$fields['combat_shooter_ignored']
        ShotPlayerHits = [uint64]$fields['combat_shot_player_hits']
        ShotWorldHits = [uint64]$fields['combat_shot_world_hits']
        LastTarget = [int]$fields['combat_last_target']
        Health = [double]$fields['health']
        ClientDataHealth = [double]$fields['combat_clientdata_health']
        Deadflag = [int]$fields['deadflag']
        CombatPhase = [string]$fields['combat_phase']
        OriginX = [double]$fields['origin_x']
        OriginY = [double]$fields['origin_y']
        OriginZ = [double]$fields['origin_z']
        Grounded = (([int]$fields['flags'] -band 512) -ne 0)
    }
}

function Invoke-CombatSnapshotPump {
    param(
        $StateA,
        $StateB,
        [System.Diagnostics.Process]$ServerProcess,
        $OutputCapture,
        [string]$StdoutPath,
        [DateTime]$Deadline,
        [System.Net.IPEndPoint]$ServerEndpoint,
        [ValidateRange(1, 256)]
        [int]$Count,
        [int]$ForwardA = 0,
        [int]$ForwardB = 0,
        [int]$SideB = 0,
        [single]$YawA = 0.0,
        [single]$YawB = 0.0
    )

    for ($index = 0; $index -lt $Count; $index++) {
        [void](Receive-TwoClientSnapshot `
            -State $StateA `
            -ServerProcess $ServerProcess `
            -OutputCapture $OutputCapture `
            -Deadline $Deadline `
            -ServerEndpoint $ServerEndpoint `
            -Forward $ForwardA `
            -Yaw $YawA)
        [void](Receive-TwoClientSnapshot `
            -State $StateB `
            -ServerProcess $ServerProcess `
            -OutputCapture $OutputCapture `
            -Deadline $Deadline `
            -ServerEndpoint $ServerEndpoint `
            -Forward $ForwardB `
            -Side $SideB `
            -Yaw $YawB)
    }
}

function Wait-CombatZeroInputExecution {
    param(
        $StateA,
        $StateB,
        [ValidateRange(1, 2)]
        [int]$ShooterSlot,
        [System.Diagnostics.Process]$ServerProcess,
        $OutputCapture,
        [string]$StdoutPath,
        [DateTime]$Deadline,
        [System.Net.IPEndPoint]$ServerEndpoint,
        [single]$YawA = 0.0,
        [single]$YawB = 0.0
    )

    $before = Get-LatestCombatProgress `
        -OutputCapture $OutputCapture `
        -StdoutPath $StdoutPath `
        -Slot $ShooterSlot
    for ($index = 0; $index -lt 96; $index++) {
        Invoke-CombatSnapshotPump `
            -StateA $StateA `
            -StateB $StateB `
            -ServerProcess $ServerProcess `
            -OutputCapture $OutputCapture `
            -StdoutPath $StdoutPath `
            -Deadline $Deadline `
            -ServerEndpoint $ServerEndpoint `
            -Count 1 `
            -YawA $YawA `
            -YawB $YawB
        $after = Get-LatestCombatProgress `
            -OutputCapture $OutputCapture `
            -StdoutPath $StdoutPath `
            -Slot $ShooterSlot
        if ($after.AttackReceived -ne $before.AttackReceived -or
            $after.AttackExecuted -ne $before.AttackExecuted) {
            throw "zero-input release changed weapon attack counters"
        }
        if ($after.MoveExecuted -ge $before.MoveExecuted + 8 -and
            $after.PreThink -ge $before.PreThink + 8 -and
            $after.PostThink -ge $before.PostThink + 8 -and
            $after.CallbackFailures -eq 0) {
            return
        }
    }
    throw "zero-input release execution was not confirmed"
}

function Wait-CombatHealthSnapshotWithoutMove {
    param(
        $StateA,
        $StateB,
        $TargetState,
        [double]$ExpectedHealth,
        [System.Diagnostics.Process]$ServerProcess,
        $OutputCapture,
        [DateTime]$Deadline,
        [System.Net.IPEndPoint]$ServerEndpoint
    )

    for ($index = 0; $index -lt 64; $index++) {
        [void](Receive-TwoClientSnapshot `
            -State $StateA `
            -ServerProcess $ServerProcess `
            -OutputCapture $OutputCapture `
            -Deadline $Deadline `
            -ServerEndpoint $ServerEndpoint `
            -DeferAcknowledgement)
        [void](Receive-TwoClientSnapshot `
            -State $StateB `
            -ServerProcess $ServerProcess `
            -OutputCapture $OutputCapture `
            -Deadline $Deadline `
            -ServerEndpoint $ServerEndpoint `
            -DeferAcknowledgement)
        $life = Get-CombatLifeWireState -State $TargetState
        if ([Math]::Abs($life.Health - $ExpectedHealth) -le 0.01) {
            return
        }
    }
    throw "fixture health snapshot was not observed"
}

function Sync-TwoClientSnapshotFrontier {
    param(
        $StateA,
        $StateB,
        [System.Diagnostics.Process]$ServerProcess,
        $OutputCapture,
        [string]$StdoutPath,
        [DateTime]$Deadline,
        [System.Net.IPEndPoint]$ServerEndpoint
    )

    Drain-TwoClientSnapshotBacklog `
        -StateA $StateA `
        -StateB $StateB `
        -ServerProcess $ServerProcess `
        -OutputCapture $OutputCapture `
        -StdoutPath $StdoutPath `
        -Deadline $Deadline `
        -ServerEndpoint $ServerEndpoint
    Invoke-CombatSnapshotPump `
        -StateA $StateA `
        -StateB $StateB `
        -ServerProcess $ServerProcess `
        -OutputCapture $OutputCapture `
        -Deadline $Deadline `
        -ServerEndpoint $ServerEndpoint `
        -Count 2
}

function Sync-OneClientSnapshotFrontier {
    param(
        $State,
        [System.Diagnostics.Process]$ServerProcess,
        $OutputCapture,
        [string]$StdoutPath,
        [DateTime]$Deadline,
        [System.Net.IPEndPoint]$ServerEndpoint
    )

    Drain-TwoClientSnapshotBacklog `
        -StateA $State `
        -StateB $null `
        -ServerProcess $ServerProcess `
        -OutputCapture $OutputCapture `
        -StdoutPath $StdoutPath `
        -Deadline $Deadline `
        -ServerEndpoint $ServerEndpoint
    for ($index = 0; $index -lt 2; $index++) {
        [void](Receive-TwoClientSnapshot `
            -State $State `
            -ServerProcess $ServerProcess `
            -OutputCapture $OutputCapture `
            -Deadline $Deadline `
            -ServerEndpoint $ServerEndpoint)
    }
}

function Assert-CombatWireUnchanged {
    param(
        $BeforeA,
        $AfterA,
        $BeforeB,
        $AfterB,
        [string]$Description
    )

    if ($AfterA.Health -ne $BeforeA.Health -or
        $AfterA.Clip -ne $BeforeA.Clip -or
        $AfterB.Health -ne $BeforeB.Health -or
        $AfterB.Clip -ne $BeforeB.Clip) {
        throw "$Description changed combat wire state"
    }
}

function Get-StockCombatAnchorGenerations {
    param(
        $OutputCapture,
        [string]$StdoutPath,
        [DateTime]$Deadline
    )

    while ([DateTime]::UtcNow -lt $Deadline) {
        [void](Update-ProcessOutputCapture -State $OutputCapture)
        $text = Get-SharedFileTailText -Path $StdoutPath
        $result = @{}
        foreach ($match in [regex]::Matches(
            $text,
            'goldsrc_stock_test_anchor: slot=(?<slot>[12]),session_generation=(?<generation>[0-9]+),')) {
            $result[[int]$match.Groups['slot'].Value] =
                [uint64]$match.Groups['generation'].Value
        }
        if ($result.ContainsKey(1) -and $result.ContainsKey(2)) {
            return $result
        }
        Start-Sleep -Milliseconds 25
    }
    throw "stock combat spawn anchors were not observed"
}

function Set-StockCombatFixture {
    param(
        [string]$ControlPath,
        [int]$ShooterSlot,
        [uint64]$ShooterGeneration,
        [int]$TargetSlot,
        [uint64]$TargetGeneration,
        [ValidateSet(
            'clear', 'miss', 'blocked', 'aim', 'lethal', 'lethal_full')]
        [string]$Mode,
        $OutputCapture,
        [string]$StdoutPath,
        [DateTime]$Deadline
    )

    $requestId = [Guid]::NewGuid().ToString('N')
    $request = (
        'arrange_combat_pair slot={0} expected_session_generation={1} ' +
        'target_slot={2} expected_target_session_generation={3} ' +
        'mode={4} request_id={5}') -f
        $ShooterSlot,
        $ShooterGeneration,
        $TargetSlot,
        $TargetGeneration,
        $Mode,
        $requestId
    [IO.File]::WriteAllText($ControlPath, $request)
    try {
        while ([DateTime]::UtcNow -lt $Deadline) {
            [void](Update-ProcessOutputCapture -State $OutputCapture)
            $text = Get-SharedFileTailText -Path $StdoutPath
            $match = [regex]::Match(
                $text,
                ('goldsrc_stock_test_combat_ack: request_id={0},' +
                 'shooter_slot=(?<shooter>[0-9]+),target_slot=(?<target>[0-9]+),' +
                 'mode=(?<mode>[^,]+),status=(?<status>[^,]+),' +
                 'reason=(?<reason>[^,]+),aim_yaw=(?<yaw>[-0-9.]+),' +
                 'world_fraction=(?<fraction>[-0-9.]+)') -f
                    [regex]::Escape($requestId))
            if ($match.Success) {
                if ($match.Groups['status'].Value -cne 'applied') {
                    throw ("combat fixture rejected: " +
                        $match.Groups['reason'].Value)
                }
                return [pscustomobject]@{
                    Yaw = [single]$match.Groups['yaw'].Value
                    WorldFraction = [single]$match.Groups['fraction'].Value
                    Mode = $match.Groups['mode'].Value
                }
            }
            Start-Sleep -Milliseconds 25
        }
        throw "combat fixture acknowledgement timed out"
    } finally {
        Remove-Item -LiteralPath $ControlPath -Force -ErrorAction SilentlyContinue
        Start-Sleep -Milliseconds 150
    }
}

function Set-StockFallFixture {
    param(
        [string]$ControlPath,
        [ValidateRange(1, 2)]
        [int]$Slot,
        [uint64]$Generation,
        [ValidateSet('landing', 'damage')]
        [string]$Mode,
        $OutputCapture,
        [string]$StdoutPath,
        [DateTime]$Deadline
    )

    $requestId = [Guid]::NewGuid().ToString('N')
    $request = (
        'arrange_fall_landing slot={0} expected_session_generation={1} ' +
        'mode={2} request_id={3}') -f
        $Slot, $Generation, $Mode, $requestId
    [IO.File]::WriteAllText($ControlPath, $request)
    try {
        while ([DateTime]::UtcNow -lt $Deadline) {
            [void](Update-ProcessOutputCapture -State $OutputCapture)
            $text = Get-SharedFileTailText -Path $StdoutPath
            $match = [regex]::Match(
                $text,
                ('goldsrc_stock_test_fall_ack: request_id={0},' +
                 'slot=(?<slot>[0-9]+),mode=(?<mode>landing|damage),' +
                 'status=(?<status>[a-z]+),' +
                 'reason=(?<reason>[a-z_]+)') -f
                    [regex]::Escape($requestId))
            if ($match.Success) {
                if ($match.Groups['status'].Value -cne 'applied' -or
                    [int]$match.Groups['slot'].Value -ne $Slot -or
                    $match.Groups['mode'].Value -cne $Mode -or
                    $match.Groups['reason'].Value -cne 'none') {
                    throw "fall fixture rejected"
                }
                return $true
            }
            Start-Sleep -Milliseconds 25
        }
        throw "fall fixture acknowledgement timed out"
    } finally {
        Remove-Item -LiteralPath $ControlPath -Force -ErrorAction SilentlyContinue
        Start-Sleep -Milliseconds 150
    }
}

function Test-StockFallObservation {
    param(
        $OutputCapture,
        [string]$StdoutPath,
        [ValidateRange(1, 2)]
        [int]$Slot,
        [uint64]$Generation,
        [ValidateSet('landing', 'damage')]
        [string]$Mode
    )

    [void](Update-ProcessOutputCapture -State $OutputCapture)
    $text = Get-SharedFileTailText -Path $StdoutPath
    $pattern = 'goldsrc_stock_test_fall_observation: slot=' +
        [string]$Slot + ',session_generation=' + [string]$Generation +
        ',mode=' + [regex]::Escape($Mode) +
        ',status=(?<status>pass|failed),reason=(?<reason>[a-z_]+)'
    $match = [regex]::Match(
        $text,
        $pattern)
    if (-not $match.Success) {
        return $false
    }
    if ($match.Groups['status'].Value -cne 'pass' -or
        $match.Groups['reason'].Value -cne 'none') {
        throw "fall observation rejected"
    }
    return $true
}

function Test-StockFallResponsiveness {
    param(
        $OutputCapture,
        [string]$StdoutPath
    )

    [void](Update-ProcessOutputCapture -State $OutputCapture)
    $text = Get-SharedFileTailText -Path $StdoutPath
    $match = [regex]::Match(
        $text,
        ('goldsrc_stock_test_fall_responsiveness: ' +
         'status=(?<status>pass|failed),reason=(?<reason>[a-z_]+)'))
    if (-not $match.Success) {
        return $false
    }
    if ($match.Groups['status'].Value -cne 'pass' -or
        $match.Groups['reason'].Value -cne 'none') {
        throw "fall responsiveness rejected"
    }
    return $true
}

function Reset-StockPlayerToAnchor {
    param(
        [string]$ControlPath,
        [ValidateRange(1, 2)]
        [int]$Slot,
        [uint64]$Generation,
        $OutputCapture,
        [string]$StdoutPath,
        [DateTime]$Deadline
    )

    $requestId = [Guid]::NewGuid().ToString('N')
    $request = (
        'reset_player_to_spawn_anchor slot={0} ' +
        'expected_session_generation={1} request_id={2}') -f
        $Slot, $Generation, $requestId
    [IO.File]::WriteAllText($ControlPath, $request)
    try {
        while ([DateTime]::UtcNow -lt $Deadline) {
            [void](Update-ProcessOutputCapture -State $OutputCapture)
            $text = Get-SharedFileTailText -Path $StdoutPath
            $match = [regex]::Match(
                $text,
                ('goldsrc_stock_test_reset_ack: request_id={0},' +
                 'slot=(?<slot>[0-9]+),status=(?<status>[a-z]+),' +
                 'reason=(?<reason>[a-z_]+)') -f
                    [regex]::Escape($requestId))
            if ($match.Success) {
                if ($match.Groups['status'].Value -cne 'applied' -or
                    [int]$match.Groups['slot'].Value -ne $Slot -or
                    $match.Groups['reason'].Value -cne 'none') {
                    throw "player reset fixture rejected"
                }
                return $true
            }
            Start-Sleep -Milliseconds 25
        }
        throw "player reset fixture acknowledgement timed out"
    } finally {
        Remove-Item -LiteralPath $ControlPath -Force -ErrorAction SilentlyContinue
        Start-Sleep -Milliseconds 150
    }
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

    $State.ClientSequence = [uint32]($State.ClientSequence + 1)
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
$combatUnitTests = Join-Path `
    $binaryDirectory "goldsrc_combat_damage_unit_tests.exe"
$deltaFixture = Join-Path $repoRoot $(if ($CombatProof) {
    "src/tests/fixtures/goldsrc_delta_description_combat.lst"
} else {
    "src/tests/fixtures/goldsrc_delta_description_minimal.lst"
})
$manifestFixture = Join-Path `
    $repoRoot "src/tests/fixtures/goldsrc_resource_manifest_minimal.tsv"
$proofMap = if ($CombatProof) { "crossfire" } else { "c0a0" }
$requiredInputs = @(
    $resolvedExecutable,
    $twoClientUnitTests,
    $pmoveUnitTests,
    $deltaFixture,
    $manifestFixture,
    (Join-Path $resolvedGameDir ("maps/{0}.bsp" -f $proofMap)))
if ($CombatProof) {
    $requiredInputs += $combatUnitTests
}
foreach ($required in $requiredInputs) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) {
        throw "two-client proof input is missing"
    }
}
& $twoClientUnitTests *> $null
if ($LASTEXITCODE -ne 0) {
    throw "two-client bounded unit validation failed"
}
if ($CombatProof) {
    & $combatUnitTests *> $null
    if ($LASTEXITCODE -ne 0) {
        throw "combat bounded unit validation failed"
    }
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
$deadline = [DateTime]::UtcNow.AddSeconds($effectiveTimeoutSeconds)
$tempId = [Guid]::NewGuid().ToString("N")
$stdoutPath = Join-Path ([IO.Path]::GetTempPath()) (
    "hlhost_two_client_${tempId}.stdout.log")
$stderrPath = Join-Path ([IO.Path]::GetTempPath()) (
    "hlhost_two_client_${tempId}.stderr.log")
$shutdownPath = Join-Path ([IO.Path]::GetTempPath()) (
    "hlhost_two_client_${tempId}.shutdown")
$controlPath = Join-Path ([IO.Path]::GetTempPath()) (
    "hlhost_two_client_${tempId}.control")
$bootstrapFrameCount = if ($CombatProof) { "0" } else { "1" }
$arguments = @(
    "--gamedir", $resolvedGameDir,
    "--dedicated",
    "--deathmatch", "1",
    "--maxclients", "2",
    "--map", $proofMap,
    "--frames", $bootstrapFrameCount,
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
    "--goldsrc-handshake-timeout-ms", [string]((
        [Math]::Min($effectiveTimeoutSeconds, 300)) * 1000)
)
if ($CombatProof -and -not $FeatureOffProof) {
    $arguments += @(
        "--goldsrc-combat",
        "--goldsrc-stock-test-control-file", $controlPath
    )
    if ($FallDamageProof) {
        $arguments += "--goldsrc-fall-damage-proof"
    }
}
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
$combatEvidence = $null
$aimPhaseEvidence = $null
$proofStage = "server_start"
$fallDiagnosticStage = "none"
try {
    $proofStage = "history_validation"
    Assert-TwoClientStreamHistoryBound
    $proofStage = "server_start"
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
        -ExpectedEntries $expectedEntries `
        -ExactCombatBootstrap:$CombatProof
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
        -ExpectedEntries $expectedEntries `
        -ExactCombatBootstrap:$CombatProof
    $stateB = New-TwoClientStreamState `
        -Client $clientB -OwnEntity 2 -Connection $connectionB

    Sync-OneClientSnapshotFrontier `
        -State $stateA `
        -ServerProcess $serverProcess `
        -OutputCapture $outputCapture `
        -StdoutPath $stdoutPath `
        -Deadline $deadline `
        -ServerEndpoint $serverEndpoint

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
                -Sequence ([uint32]($stateA.ClientSequence + 1)) `
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
        $stateA.ClientSequence = [uint32]($stateA.ClientSequence + 1)
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

    if ($CombatProof) {
        if (-not $FeatureOffProof) {
            $proofStage = "combat_frontier_sync"
            Sync-TwoClientSnapshotFrontier `
                -StateA $stateA `
                -StateB $stateB `
                -ServerProcess $serverProcess `
                -OutputCapture $outputCapture `
                -StdoutPath $stdoutPath `
                -Deadline $deadline `
                -ServerEndpoint $serverEndpoint
        }
        if ($FeatureOffProof) {
            $proofStage = "feature_off_attack_boundary"
            $featureBeforeA = Get-FeatureOffWireState -State $stateA
            $featureBeforeB = Get-FeatureOffWireState -State $stateB
            if ($featureBeforeA.WeaponDataPresent -or
                $featureBeforeB.WeaponDataPresent) {
                throw "feature-off unexpectedly exposed weapondata"
            }
            $featureProgressBeforeA = Get-LatestCombatProgress `
                -OutputCapture $outputCapture `
                -StdoutPath $stdoutPath `
                -Slot 1
            $featureProgressBeforeB = Get-LatestCombatProgress `
                -OutputCapture $outputCapture `
                -StdoutPath $stdoutPath `
                -Slot 2
            [void](Send-TwoClientCombatMove `
                -State $stateA `
                -Yaw 0.0 `
                -Buttons 1)
            $featureMovementAdvanced = $false
            for ($index = 0; $index -lt 80; $index++) {
                Invoke-CombatSnapshotPump `
                    -StateA $stateA `
                    -StateB $stateB `
                    -ServerProcess $serverProcess `
                    -OutputCapture $outputCapture `
                    -Deadline $deadline `
                    -ServerEndpoint $serverEndpoint `
                    -Count 1 `
                    -ForwardA 140 `
                    -SideB 140
                if ($index -lt 10) {
                    continue
                }
                $featureProgressAfterA = Get-LatestCombatProgress `
                    -OutputCapture $outputCapture `
                    -StdoutPath $stdoutPath `
                    -Slot 1
                $featureProgressAfterB = Get-LatestCombatProgress `
                    -OutputCapture $outputCapture `
                    -StdoutPath $stdoutPath `
                    -Slot 2
                if ($featureProgressAfterA.MoveReceived -gt
                        $featureProgressBeforeA.MoveReceived -and
                    $featureProgressAfterA.MoveValidated -gt
                        $featureProgressBeforeA.MoveValidated -and
                    $featureProgressAfterA.MoveExecuted -gt
                        $featureProgressBeforeA.MoveExecuted -and
                    $featureProgressAfterA.PmoveCalls -gt
                        $featureProgressBeforeA.PmoveCalls -and
                    $featureProgressAfterA.ForwardInputs -gt
                        $featureProgressBeforeA.ForwardInputs -and
                    $featureProgressAfterB.PmoveCalls -gt
                        $featureProgressBeforeB.PmoveCalls -and
                    $featureProgressAfterB.StrafeInputs -gt
                        $featureProgressBeforeB.StrafeInputs) {
                    $featureMovementAdvanced = $true
                    break
                }
            }
            $featureAfterA = Get-FeatureOffWireState -State $stateA
            $featureAfterB = Get-FeatureOffWireState -State $stateB
            if (-not $featureMovementAdvanced -or
                $featureAfterA.Health -ne $featureBeforeA.Health -or
                $featureAfterB.Health -ne $featureBeforeB.Health -or
                $featureAfterA.WeaponDataPresent -or
                $featureAfterB.WeaponDataPresent) {
                throw "feature-off attack boundary gate failed"
            }
            $combatEvidence = [pscustomobject]@{
                AttackMoveReceived = $true
                AttackMoveValidated = $true
                AttackMoveExecuted = $true
                AttackMasked = $true
                GameplayCallbacksAbsent = $true
                HealthUnchanged = $true
                WeaponDataAbsent = $true
                MovementStable = $true
            }
        } else {
        $proofStage = "combat_anchor_stabilization"
        for ($index = 0; $index -lt 120; $index++) {
            [void](Receive-TwoClientSnapshot `
                -State $stateA `
                -ServerProcess $serverProcess `
                -OutputCapture $outputCapture `
                -Deadline $deadline `
                -ServerEndpoint $serverEndpoint)
            [void](Receive-TwoClientSnapshot `
                -State $stateB `
                -ServerProcess $serverProcess `
                -OutputCapture $outputCapture `
                -Deadline $deadline `
                -ServerEndpoint $serverEndpoint)
            [void](Update-ProcessOutputCapture -State $outputCapture)
            $anchorText = Get-SharedFileText -Path $stdoutPath
            if ($anchorText -match 'goldsrc_stock_test_anchor: slot=1,' -and
                $anchorText -match 'goldsrc_stock_test_anchor: slot=2,') {
                break
            }
        }
        $generations = Get-StockCombatAnchorGenerations `
            -OutputCapture $outputCapture `
            -StdoutPath $stdoutPath `
            -Deadline $deadline

        if ($FallDamageProof) {
            $proofStage = "fall_frontier_sync"
            Sync-TwoClientSnapshotFrontier `
                -StateA $stateA `
                -StateB $stateB `
                -ServerProcess $serverProcess `
                -OutputCapture $outputCapture `
                -StdoutPath $stdoutPath `
                -Deadline $deadline `
                -ServerEndpoint $serverEndpoint
            $fallWireBeforeA = Get-CombatWireState -State $stateA
            $fallWireBeforeB = Get-CombatWireState -State $stateB
            $fallProgressBeforeA = Get-LatestCombatProgress `
                -OutputCapture $outputCapture `
                -StdoutPath $stdoutPath `
                -Slot 1
            $fallProgressBeforeB = Get-LatestCombatProgress `
                -OutputCapture $outputCapture `
                -StdoutPath $stdoutPath `
                -Slot 2
            if ($fallWireBeforeA.Health -le 10.0 -or
                $fallProgressBeforeA.Deadflag -ne 0 -or
                $fallProgressBeforeA.CallbackFailures -ne 0 -or
                $fallProgressBeforeB.CallbackFailures -ne 0 -or
                [Math]::Abs(
                    $fallWireBeforeA.Health -
                        $fallProgressBeforeA.Health) -gt 0.01 -or
                [Math]::Abs(
                    $fallWireBeforeA.Health -
                        $fallProgressBeforeA.ClientDataHealth) -gt 0.01 -or
                [Math]::Abs(
                    $fallWireBeforeB.Health -
                        $fallProgressBeforeB.Health) -gt 0.01) {
                throw "fall precondition gate failed"
            }

            $proofStage = "fall_landing_fixture"
            [void](Set-StockFallFixture `
                -ControlPath $controlPath `
                -Slot 1 `
                -Generation $generations[1] `
                -Mode landing `
                -OutputCapture $outputCapture `
                -StdoutPath $stdoutPath `
                -Deadline $deadline)

            $proofStage = "fall_landing_transition"
            [void](Send-TwoClientCombatMove `
                -State $stateA `
                -Yaw 0.0 `
                -Buttons 0)
            $landingObserved = $false
            $landingGroundedObserved = $false
            $landingWireAfterA = $fallWireBeforeA
            $landingWireAfterB = $fallWireBeforeB
            $landingProgressAfterA = $fallProgressBeforeA
            $landingProgressAfterB = $fallProgressBeforeB
            for ($index = 0; $index -lt 96; $index++) {
                $fallDiagnosticStage = "landing_snapshot_pump"
                Invoke-CombatSnapshotPump `
                    -StateA $stateA `
                    -StateB $stateB `
                    -ServerProcess $serverProcess `
                    -OutputCapture $outputCapture `
                    -StdoutPath $stdoutPath `
                    -Deadline $deadline `
                    -ServerEndpoint $serverEndpoint `
                    -Count 1
                $fallDiagnosticStage = "landing_wire_a"
                $landingWireAfterA = Get-CombatWireState -State $stateA
                $fallDiagnosticStage = "landing_wire_b"
                $landingWireAfterB = Get-CombatWireState -State $stateB
                if ($index -lt 10) {
                    continue
                }
                $fallDiagnosticStage = "landing_progress_a"
                $landingProgressAfterA = Get-LatestCombatProgress `
                    -OutputCapture $outputCapture `
                    -StdoutPath $stdoutPath `
                    -Slot 1
                $fallDiagnosticStage = "landing_progress_b"
                $landingProgressAfterB = Get-LatestCombatProgress `
                    -OutputCapture $outputCapture `
                    -StdoutPath $stdoutPath `
                    -Slot 2
                $fallDiagnosticStage = "landing_observation"
                $landingGroundedObserved = Test-StockFallObservation `
                    -OutputCapture $outputCapture `
                    -StdoutPath $stdoutPath `
                    -Slot 1 `
                    -Generation $generations[1] `
                    -Mode landing
                if ($landingProgressAfterA.MoveExecuted -gt
                        $fallProgressBeforeA.MoveExecuted -and
                    $landingProgressAfterA.PmoveCalls -gt
                        $fallProgressBeforeA.PmoveCalls -and
                    $landingProgressAfterA.PreThink -gt
                        $fallProgressBeforeA.PreThink -and
                    $landingProgressAfterA.PostThink -gt
                        $fallProgressBeforeA.PostThink -and
                    $landingProgressAfterA.Grounded -and
                    $landingGroundedObserved) {
                    $landingObserved = $true
                    break
                }
            }
            if (-not $landingObserved -or -not $landingGroundedObserved) {
                throw "fall landing transition unconfirmed"
            }
            if ($serverProcess.HasExited) {
                throw "fall landing server unavailable"
            }
            if ($landingProgressAfterA.CallbackFailures -ne 0 -or
                $landingProgressAfterB.CallbackFailures -ne 0) {
                throw "fall landing callback failure"
            }
            if (
                [Math]::Abs(
                    $landingWireAfterA.Health -
                        $fallWireBeforeA.Health) -gt 0.01 -or
                [Math]::Abs(
                    $landingProgressAfterA.Health -
                        $fallProgressBeforeA.Health) -gt 0.01 -or
                [Math]::Abs(
                    $landingProgressAfterA.ClientDataHealth -
                        $fallProgressBeforeA.ClientDataHealth) -gt 0.01 -or
                $landingProgressAfterA.Deadflag -ne 0) {
                throw "fall landing health transition failed"
            }
            if ($landingWireAfterA.Clip -ne $fallWireBeforeA.Clip -or
                $landingProgressAfterA.AttackReceived -ne
                    $fallProgressBeforeA.AttackReceived -or
                $landingProgressAfterA.AttackExecuted -ne
                    $fallProgressBeforeA.AttackExecuted -or
                [Math]::Abs(
                    $landingWireAfterB.Health -
                        $fallWireBeforeB.Health) -gt 0.01 -or
                $landingWireAfterB.Clip -ne $fallWireBeforeB.Clip) {
                throw "fall landing isolation failed"
            }

            $proofStage = "fall_between_modes_reset"
            [void](Reset-StockPlayerToAnchor `
                -ControlPath $controlPath `
                -Slot 1 `
                -Generation $generations[1] `
                -OutputCapture $outputCapture `
                -StdoutPath $stdoutPath `
                -Deadline $deadline)
            Sync-TwoClientSnapshotFrontier `
                -StateA $stateA `
                -StateB $stateB `
                -ServerProcess $serverProcess `
                -OutputCapture $outputCapture `
                -StdoutPath $stdoutPath `
                -Deadline $deadline `
                -ServerEndpoint $serverEndpoint
            $fallWireBeforeA = Get-CombatWireState -State $stateA
            $fallWireBeforeB = Get-CombatWireState -State $stateB
            $fallProgressBeforeA = Get-LatestCombatProgress `
                -OutputCapture $outputCapture `
                -StdoutPath $stdoutPath `
                -Slot 1
            $fallProgressBeforeB = Get-LatestCombatProgress `
                -OutputCapture $outputCapture `
                -StdoutPath $stdoutPath `
                -Slot 2
            if ([Math]::Abs(
                    $fallWireBeforeA.Health -
                        $landingWireAfterA.Health) -gt 0.01 -or
                $fallWireBeforeA.Clip -ne $landingWireAfterA.Clip -or
                [Math]::Abs(
                    $fallWireBeforeB.Health -
                        $landingWireAfterB.Health) -gt 0.01 -or
                $fallWireBeforeB.Clip -ne $landingWireAfterB.Clip -or
                $fallProgressBeforeA.CallbackFailures -ne 0 -or
                $fallProgressBeforeB.CallbackFailures -ne 0) {
                throw "fall inter-mode reset gate failed"
            }

            $proofStage = "fall_damage_fixture"
            [void](Set-StockFallFixture `
                -ControlPath $controlPath `
                -Slot 1 `
                -Generation $generations[1] `
                -Mode damage `
                -OutputCapture $outputCapture `
                -StdoutPath $stdoutPath `
                -Deadline $deadline)

            $proofStage = "fall_damage_landing"
            [void](Send-TwoClientCombatMove `
                -State $stateA `
                -Yaw 0.0 `
                -Buttons 0)
            $fallObserved = $false
            $damageGroundedObserved = $false
            $fallWireAfterA = $fallWireBeforeA
            $fallWireAfterB = $fallWireBeforeB
            $fallProgressAfterA = $fallProgressBeforeA
            $fallProgressAfterB = $fallProgressBeforeB
            for ($index = 0; $index -lt 96; $index++) {
                Invoke-CombatSnapshotPump `
                    -StateA $stateA `
                    -StateB $stateB `
                    -ServerProcess $serverProcess `
                    -OutputCapture $outputCapture `
                    -StdoutPath $stdoutPath `
                    -Deadline $deadline `
                    -ServerEndpoint $serverEndpoint `
                    -Count 1
                $fallWireAfterA = Get-CombatWireState -State $stateA
                $fallWireAfterB = Get-CombatWireState -State $stateB
                if ($index -lt 10) {
                    continue
                }
                $fallProgressAfterA = Get-LatestCombatProgress `
                    -OutputCapture $outputCapture `
                    -StdoutPath $stdoutPath `
                    -Slot 1
                $fallProgressAfterB = Get-LatestCombatProgress `
                    -OutputCapture $outputCapture `
                    -StdoutPath $stdoutPath `
                    -Slot 2
                $damageGroundedObserved = Test-StockFallObservation `
                    -OutputCapture $outputCapture `
                    -StdoutPath $stdoutPath `
                    -Slot 1 `
                    -Generation $generations[1] `
                    -Mode damage
                if ([Math]::Abs(
                        $fallWireAfterA.Health -
                            ($fallWireBeforeA.Health - 10.0)) -le 0.01 -and
                    $fallProgressAfterA.MoveReceived -gt
                        $fallProgressBeforeA.MoveReceived -and
                    $fallProgressAfterA.MoveValidated -gt
                        $fallProgressBeforeA.MoveValidated -and
                    $fallProgressAfterA.MoveExecuted -gt
                        $fallProgressBeforeA.MoveExecuted -and
                    $fallProgressAfterA.PmoveCalls -gt
                        $fallProgressBeforeA.PmoveCalls -and
                    $fallProgressAfterA.PreThink -gt
                        $fallProgressBeforeA.PreThink -and
                    $fallProgressAfterA.PostThink -gt
                        $fallProgressBeforeA.PostThink -and
                    $fallProgressAfterA.Grounded -and
                    $damageGroundedObserved) {
                    $fallObserved = $true
                    break
                }
            }
            if (-not $fallObserved -or -not $damageGroundedObserved -or
                $serverProcess.HasExited) {
                throw "fall landing callback gate failed"
            }
            if ($fallProgressAfterA.CallbackFailures -ne 0 -or
                $fallProgressAfterA.CallbackFailures -ne
                    $fallProgressBeforeA.CallbackFailures) {
                throw "fall callback failure gate failed"
            }
            if ([Math]::Abs(
                    $fallProgressAfterA.Health -
                        ($fallProgressBeforeA.Health - 10.0)) -gt 0.01) {
                throw "fall authoritative damage gate failed"
            }
            if ([Math]::Abs(
                    $fallProgressAfterA.ClientDataHealth -
                        $fallProgressAfterA.Health) -gt 0.01 -or
                [Math]::Abs(
                    $fallWireAfterA.Health -
                        $fallProgressAfterA.Health) -gt 0.01) {
                throw "fall clientdata replication gate failed"
            }
            if ($fallProgressAfterA.Deadflag -ne 0 -or
                $fallProgressAfterA.Health -le 0.0) {
                throw "fall nonlethal gate failed"
            }
            if ($fallWireAfterA.Clip -ne $fallWireBeforeA.Clip -or
                $fallProgressAfterA.AttackReceived -ne
                    $fallProgressBeforeA.AttackReceived -or
                $fallProgressAfterA.AttackExecuted -ne
                    $fallProgressBeforeA.AttackExecuted) {
                throw "fall weapon isolation gate failed"
            }
            if ([Math]::Abs(
                    $fallWireAfterB.Health -
                        $fallWireBeforeB.Health) -gt 0.01 -or
                $fallWireAfterB.Clip -ne $fallWireBeforeB.Clip -or
                $fallProgressAfterB.CallbackFailures -ne 0) {
                throw "fall peer isolation gate failed"
            }

            $proofStage = "fall_post_landing"
            $fallDiagnosticStage = "responsive_progress_before_a"
            $responsiveBeforeA = Get-LatestCombatProgress `
                -OutputCapture $outputCapture `
                -StdoutPath $stdoutPath `
                -Slot 1
            $fallDiagnosticStage = "responsive_progress_before_b"
            $responsiveBeforeB = Get-LatestCombatProgress `
                -OutputCapture $outputCapture `
                -StdoutPath $stdoutPath `
                -Slot 2
            $fallDiagnosticStage = "responsive_wire_before_a"
            $responsiveWireBeforeA = Get-CombatWireState -State $stateA
            $fallDiagnosticStage = "responsive_wire_before_b"
            $responsiveWireBeforeB = Get-CombatWireState -State $stateB
            $responsiveSnapshotsA = $stateA.Snapshots
            $responsiveSnapshotsB = $stateB.Snapshots
            $postLandingResponsive = $false
            $authoritativeMovedA = $false
            $authoritativeMovedB = $false
            $wireMovedA = $false
            $wireMovedB = $false
            $responsiveAfterA = $responsiveBeforeA
            $responsiveAfterB = $responsiveBeforeB
            for ($index = 0; $index -lt 96; $index++) {
                $directionA = if ($authoritativeMovedA) {
                    0
                } elseif ($index -lt 48) {
                    200
                } else {
                    -200
                }
                $directionB = if ($authoritativeMovedB) {
                    0
                } elseif ($index -lt 48) {
                    -200
                } else {
                    200
                }
                $fallDiagnosticStage = "responsive_snapshot_pump"
                Invoke-CombatSnapshotPump `
                    -StateA $stateA `
                    -StateB $stateB `
                    -ServerProcess $serverProcess `
                    -OutputCapture $outputCapture `
                    -StdoutPath $stdoutPath `
                    -Deadline $deadline `
                    -ServerEndpoint $serverEndpoint `
                    -Count 1 `
                    -ForwardA $directionA `
                    -ForwardB $directionB
                if ($index -lt 10) {
                    continue
                }
                $fallDiagnosticStage = "responsive_progress_after_a"
                $responsiveAfterA = Get-LatestCombatProgress `
                    -OutputCapture $outputCapture `
                    -StdoutPath $stdoutPath `
                    -Slot 1
                $fallDiagnosticStage = "responsive_progress_after_b"
                $responsiveAfterB = Get-LatestCombatProgress `
                    -OutputCapture $outputCapture `
                    -StdoutPath $stdoutPath `
                    -Slot 2
                $fallDiagnosticStage = "responsive_wire_after_a"
                $responsiveWireAfterA = Get-CombatWireState -State $stateA
                $fallDiagnosticStage = "responsive_wire_after_b"
                $responsiveWireAfterB = Get-CombatWireState -State $stateB
                $authoritativeDeltaAX = $responsiveAfterA.OriginX -
                    $responsiveBeforeA.OriginX
                $authoritativeDeltaAY = $responsiveAfterA.OriginY -
                    $responsiveBeforeA.OriginY
                $authoritativeDeltaBX = $responsiveAfterB.OriginX -
                    $responsiveBeforeB.OriginX
                $authoritativeDeltaBY = $responsiveAfterB.OriginY -
                    $responsiveBeforeB.OriginY
                $wireDeltaAX = $responsiveWireAfterA.OriginX -
                    $responsiveWireBeforeA.OriginX
                $wireDeltaAY = $responsiveWireAfterA.OriginY -
                    $responsiveWireBeforeA.OriginY
                $wireDeltaBX = $responsiveWireAfterB.OriginX -
                    $responsiveWireBeforeB.OriginX
                $wireDeltaBY = $responsiveWireAfterB.OriginY -
                    $responsiveWireBeforeB.OriginY
                $authoritativeMovedA = $authoritativeMovedA -or
                    ($authoritativeDeltaAX * $authoritativeDeltaAX +
                        $authoritativeDeltaAY * $authoritativeDeltaAY) -gt 1.0
                $authoritativeMovedB = $authoritativeMovedB -or
                    ($authoritativeDeltaBX * $authoritativeDeltaBX +
                        $authoritativeDeltaBY * $authoritativeDeltaBY) -gt 1.0
                $wireMovedA = $wireMovedA -or
                    ($wireDeltaAX * $wireDeltaAX +
                        $wireDeltaAY * $wireDeltaAY) -gt 0.25
                $wireMovedB = $wireMovedB -or
                    ($wireDeltaBX * $wireDeltaBX +
                        $wireDeltaBY * $wireDeltaBY) -gt 0.25
                $fallDiagnosticStage = "responsive_observation"
                $serverResponsivenessObserved = Test-StockFallResponsiveness `
                    -OutputCapture $outputCapture `
                    -StdoutPath $stdoutPath
                if ($responsiveAfterA.MoveExecuted -gt
                        $responsiveBeforeA.MoveExecuted -and
                    $responsiveAfterB.MoveExecuted -gt
                        $responsiveBeforeB.MoveExecuted -and
                    $authoritativeMovedA -and $authoritativeMovedB -and
                    $serverResponsivenessObserved -and
                    $stateA.Snapshots -gt $responsiveSnapshotsA -and
                    $stateB.Snapshots -gt $responsiveSnapshotsB) {
                    $postLandingResponsive = $true
                    break
                }
            }
            if ($postLandingResponsive) {
                $fallDiagnosticStage = "responsive_frontier_sync"
                Sync-TwoClientSnapshotFrontier `
                    -StateA $stateA `
                    -StateB $stateB `
                    -ServerProcess $serverProcess `
                    -OutputCapture $outputCapture `
                    -StdoutPath $stdoutPath `
                    -Deadline $deadline `
                    -ServerEndpoint $serverEndpoint
                $fallDiagnosticStage = "responsive_wire_frontier_a"
                $responsiveWireAfterA = Get-CombatWireState -State $stateA
                $fallDiagnosticStage = "responsive_wire_frontier_b"
                $responsiveWireAfterB = Get-CombatWireState -State $stateB
                $wireDeltaAX = $responsiveWireAfterA.OriginX -
                    $responsiveWireBeforeA.OriginX
                $wireDeltaAY = $responsiveWireAfterA.OriginY -
                    $responsiveWireBeforeA.OriginY
                $wireDeltaBX = $responsiveWireAfterB.OriginX -
                    $responsiveWireBeforeB.OriginX
                $wireDeltaBY = $responsiveWireAfterB.OriginY -
                    $responsiveWireBeforeB.OriginY
                $wireMovedA = $wireMovedA -or
                    ($wireDeltaAX * $wireDeltaAX +
                        $wireDeltaAY * $wireDeltaAY) -gt 0.25
                $wireMovedB = $wireMovedB -or
                    ($wireDeltaBX * $wireDeltaBX +
                        $wireDeltaBY * $wireDeltaBY) -gt 0.25
            }
            $fallFinalWireA = Get-CombatWireState -State $stateA
            $fallFinalWireB = Get-CombatWireState -State $stateB
            $fallDiagnosticStage = "responsive_gate"
            if ($serverProcess.HasExited -or
                $responsiveAfterA.CallbackFailures -ne 0 -or
                $responsiveAfterB.CallbackFailures -ne 0) {
                throw "fall responsiveness runtime failed"
            }
            if (-not $authoritativeMovedA -or -not $authoritativeMovedB) {
                throw "fall authoritative movement missing"
            }
            if (-not $wireMovedA -and -not $wireMovedB) {
                throw "fall wire movement missing both"
            }
            if (-not $wireMovedA) {
                throw "fall wire movement missing a"
            }
            if (-not $wireMovedB) {
                throw "fall wire movement missing b"
            }
            if (-not $postLandingResponsive) {
                throw "fall responsiveness contract missing"
            }
            if (
                [Math]::Abs(
                    $fallFinalWireA.Health -
                        $fallWireAfterA.Health) -gt 0.01 -or
                $fallFinalWireA.Clip -ne $fallWireAfterA.Clip -or
                [Math]::Abs(
                    $fallFinalWireB.Health -
                        $fallWireBeforeB.Health) -gt 0.01 -or
                $fallFinalWireB.Clip -ne $fallWireBeforeB.Clip) {
                throw "fall post landing state changed"
            }
            $combatEvidence = [pscustomobject]@{
                FixtureApplied = $true
                LandingModeGrounded = $landingGroundedObserved
                LandingModeHealthUnchanged = $true
                LandingModePostThinkAdvanced = $true
                HealthBefore = $fallWireBeforeA.Health
                HealthAfter = $fallWireAfterA.Health
                DamageAmount = 10.0
                DamageModeGrounded = $damageGroundedObserved
                TargetAlive = $true
                PlayerPostThinkAdvanced = $true
                GameplayCallbackFailures = 0
                ClientDataHealthUpdated = $true
                PeerStateUnchanged = $true
                PostLandingMovement = $true
                ServerStillResponsive = $true
            }
        } elseif (-not $NegativeProof) {
            $proofStage = "aim_only_fixture"
            $aimFixture = Set-StockCombatFixture `
                -ControlPath $controlPath `
                -ShooterSlot 1 `
                -ShooterGeneration $generations[1] `
                -TargetSlot 2 `
                -TargetGeneration $generations[2] `
                -Mode aim `
                -OutputCapture $outputCapture `
                -StdoutPath $stdoutPath `
                -Deadline $deadline
            if ($aimFixture.Mode -cne 'aim' -or
                $aimFixture.WorldFraction -lt 0.999) {
                throw "aim-only combat fixture semantic gate failed"
            }
            [single]$aimYaw = $aimFixture.Yaw

            Sync-TwoClientSnapshotFrontier `
                -StateA $stateA `
                -StateB $stateB `
                -ServerProcess $serverProcess `
                -OutputCapture $outputCapture `
                -StdoutPath $stdoutPath `
                -Deadline $deadline `
                -ServerEndpoint $serverEndpoint
            $aimWireBeforeA = Get-CombatWireState -State $stateA
            $aimWireBeforeB = Get-CombatWireState -State $stateB
            $aimProgressBeforeA = Get-LatestCombatProgress `
                -OutputCapture $outputCapture `
                -StdoutPath $stdoutPath `
                -Slot 1
            $aimProgressBeforeB = Get-LatestCombatProgress `
                -OutputCapture $outputCapture `
                -StdoutPath $stdoutPath `
                -Slot 2
            if ($aimProgressBeforeA.CallbackFailures -ne 0 -or
                $aimProgressBeforeB.CallbackFailures -ne 0 -or
                $aimProgressBeforeA.ActiveWeapon -ne 2 -or
                $aimProgressBeforeB.ActiveWeapon -ne 2 -or
                -not $aimProgressBeforeA.GlockPresent -or
                -not $aimProgressBeforeB.GlockPresent) {
                throw "aim-only precondition gate failed"
            }

            $proofStage = "aim_only_weapon_idle"
            $aimSnapshotsBeforeA = $stateA.Snapshots
            $aimSnapshotsBeforeB = $stateB.Snapshots
            $aimProgressAfterA = $aimProgressBeforeA
            $aimProgressAfterB = $aimProgressBeforeB
            $aimCallbacksAdvanced = $false
            for ($index = 0; $index -lt 80; $index++) {
                Invoke-CombatSnapshotPump `
                    -StateA $stateA `
                    -StateB $stateB `
                    -ServerProcess $serverProcess `
                    -OutputCapture $outputCapture `
                    -StdoutPath $stdoutPath `
                    -Deadline $deadline `
                    -ServerEndpoint $serverEndpoint `
                    -Count 1 `
                    -YawA $aimYaw
                if ($index -lt 10) {
                    continue
                }
                $aimProgressAfterA = Get-LatestCombatProgress `
                    -OutputCapture $outputCapture `
                    -StdoutPath $stdoutPath `
                    -Slot 1
                $aimProgressAfterB = Get-LatestCombatProgress `
                    -OutputCapture $outputCapture `
                    -StdoutPath $stdoutPath `
                    -Slot 2
                if ($aimProgressAfterA.MoveReceived -gt
                        $aimProgressBeforeA.MoveReceived -and
                    $aimProgressAfterA.MoveValidated -gt
                        $aimProgressBeforeA.MoveValidated -and
                    $aimProgressAfterA.MoveExecuted -gt
                        $aimProgressBeforeA.MoveExecuted -and
                    $aimProgressAfterA.PmoveCalls -gt
                        $aimProgressBeforeA.PmoveCalls -and
                    $aimProgressAfterA.PreThink -gt
                        $aimProgressBeforeA.PreThink -and
                    $aimProgressAfterA.PostThink -gt
                        $aimProgressBeforeA.PostThink -and
                    $aimProgressAfterA.ServerTimeMs -gt
                        $aimProgressBeforeA.ServerTimeMs -and
                    $aimProgressAfterB.MoveReceived -gt
                        $aimProgressBeforeB.MoveReceived -and
                    $aimProgressAfterB.MoveValidated -gt
                        $aimProgressBeforeB.MoveValidated -and
                    $aimProgressAfterB.MoveExecuted -gt
                        $aimProgressBeforeB.MoveExecuted -and
                    $aimProgressAfterB.PmoveCalls -gt
                        $aimProgressBeforeB.PmoveCalls -and
                    $aimProgressAfterB.PreThink -gt
                        $aimProgressBeforeB.PreThink -and
                    $aimProgressAfterB.PostThink -gt
                        $aimProgressBeforeB.PostThink -and
                    $aimProgressAfterB.ServerTimeMs -gt
                        $aimProgressBeforeB.ServerTimeMs -and
                    $stateA.Snapshots -gt $aimSnapshotsBeforeA -and
                    $stateB.Snapshots -gt $aimSnapshotsBeforeB) {
                    $aimCallbacksAdvanced = $true
                    break
                }
            }
            $aimWireAfterA = Get-CombatWireState -State $stateA
            $aimWireAfterB = Get-CombatWireState -State $stateB
            if (-not $aimCallbacksAdvanced -or $serverProcess.HasExited) {
                throw "aim-only responsiveness gate failed"
            }
            if ($aimProgressAfterA.CallbackFailures -ne 0 -or
                $aimProgressAfterB.CallbackFailures -ne 0) {
                throw "aim-only callback gate failed"
            }
            Assert-CombatWireUnchanged `
                -BeforeA $aimWireBeforeA `
                -AfterA $aimWireAfterA `
                -BeforeB $aimWireBeforeB `
                -AfterB $aimWireAfterB `
                -Description "aim-only idle"
            if ($aimProgressAfterA.AttackReceived -ne 0 -or
                $aimProgressAfterA.AttackExecuted -ne 0 -or
                $aimProgressAfterB.AttackReceived -ne 0 -or
                $aimProgressAfterB.AttackExecuted -ne 0 -or
                $aimProgressAfterA.GlockClip -ne
                    $aimProgressBeforeA.GlockClip -or
                $aimProgressAfterB.GlockClip -ne
                    $aimProgressBeforeB.GlockClip -or
                $aimProgressAfterA.Health -ne
                    $aimProgressBeforeA.Health -or
                $aimProgressAfterB.Health -ne
                    $aimProgressBeforeB.Health -or
                $aimProgressAfterA.ClientDataHealth -ne
                    $aimProgressBeforeA.ClientDataHealth -or
                $aimProgressAfterB.ClientDataHealth -ne
                    $aimProgressBeforeB.ClientDataHealth -or
                $aimProgressAfterA.ShotTraces -ne
                    $aimProgressBeforeA.ShotTraces -or
                $aimProgressAfterB.ShotTraces -ne
                    $aimProgressBeforeB.ShotTraces) {
                throw "aim-only combat state isolation gate failed"
            }
            $aimPhaseEvidence = [pscustomobject]@{
                FixtureApplied = $true
                TargetInsideAutoaimCone = $true
                AimOffsetNonzero = $true
                ButtonsZero = $true
                ClientACallbacksAdvanced = $true
                ClientBCallbacksAdvanced = $true
                AttackStateUnchanged = $true
                WireStateUnchanged = $true
                ServerStillResponsive = $true
            }
            $proofStage = "combat_clear_fixture"
            $clearFixtureMode = if ($LethalDeathProof) {
                'lethal_full'
            } else {
                'clear'
            }
            $fixture = Set-StockCombatFixture `
                -ControlPath $controlPath `
                -ShooterSlot 1 `
                -ShooterGeneration $generations[1] `
                -TargetSlot 2 `
                -TargetGeneration $generations[2] `
                -Mode $clearFixtureMode `
                -OutputCapture $outputCapture `
                -StdoutPath $stdoutPath `
                -Deadline $deadline
            if ($fixture.Mode -cne $clearFixtureMode -or
                $fixture.WorldFraction -lt 0.999) {
                throw "clear combat fixture semantic gate failed"
            }
            Sync-TwoClientSnapshotFrontier `
                -StateA $stateA `
                -StateB $stateB `
                -ServerProcess $serverProcess `
                -OutputCapture $outputCapture `
                -StdoutPath $stdoutPath `
                -Deadline $deadline `
                -ServerEndpoint $serverEndpoint
            $wireBeforeA = Get-CombatWireState -State $stateA
            $wireBeforeB = Get-CombatWireState -State $stateB
            $progressBeforeA = Get-LatestCombatProgress `
                -OutputCapture $outputCapture `
                -StdoutPath $stdoutPath `
                -Slot 1
            $progressBeforeB = Get-LatestCombatProgress `
                -OutputCapture $outputCapture `
                -StdoutPath $stdoutPath `
                -Slot 2
            if ($wireBeforeA.Clip -ne $progressBeforeA.GlockClip -or
                $wireBeforeB.Clip -ne $progressBeforeB.GlockClip) {
                throw "combat initial weapondata wire gate failed"
            }
            if ([Math]::Abs(
                    $wireBeforeA.Health -
                        $progressBeforeA.ClientDataHealth) -gt 0.01 -or
                [Math]::Abs(
                    $wireBeforeB.Health -
                        $progressBeforeB.ClientDataHealth) -gt 0.01) {
                throw "combat initial clientdata wire gate failed"
            }
            $proofStage = "combat_positive_shot"
            $controlSnapshotsBeforeA = $stateA.Snapshots
            $controlSnapshotsBeforeB = $stateB.Snapshots
            [void](Send-TwoClientCombatMove `
                -State $stateA -Yaw $fixture.Yaw -Buttons 1)
            $movementAdvanced = $false
            $wireTransitionObserved = $false
            $wireAfterA = $wireBeforeA
            $wireAfterB = $wireBeforeB
            $healthDeltaFieldObserved = $false
            $clipDeltaFieldObserved = $false
            [double]$minimumDecodedTargetHealth = $wireBeforeB.Health
            [int]$minimumDecodedShooterClip = $wireBeforeA.Clip
            [double]$minimumHealthDeltaValue = [double]::PositiveInfinity
            [double]$minimumClipDeltaValue = [double]::PositiveInfinity
            for ($index = 0; $index -lt 80; $index++) {
                Invoke-CombatSnapshotPump `
                    -StateA $stateA `
                    -StateB $stateB `
                    -ServerProcess $serverProcess `
                    -OutputCapture $outputCapture `
                    -Deadline $deadline `
                    -ServerEndpoint $serverEndpoint `
                    -Count 1 `
                    -ForwardA 140 `
                    -SideB 140
                $snapshotA = $stateA.LatestSnapshot
                $snapshotB = $stateB.LatestSnapshot
                $currentClipDeltaField = $null -ne
                    $snapshotA.PSObject.Properties[
                        'WeaponDataChangedFields'] -and
                    $snapshotA.WeaponDataChangedFields.Contains(2) -and
                    $snapshotA.WeaponDataChangedFields[2] -contains
                        'm_iClip'
                if ($currentClipDeltaField) {
                    $clipDeltaFieldObserved = $true
                }
                $currentHealthDeltaField = $null -ne
                    $snapshotB.PSObject.Properties[
                        'ClientDataChangedFields'] -and
                    $snapshotB.ClientDataChangedFields -contains 'health'
                if ($currentHealthDeltaField) {
                    $healthDeltaFieldObserved = $true
                }
                $observedWireA = Get-CombatWireState -State $stateA
                $observedWireB = Get-CombatWireState -State $stateB
                $minimumDecodedShooterClip = [Math]::Min(
                    $minimumDecodedShooterClip,
                    $observedWireA.Clip)
                $minimumDecodedTargetHealth = [Math]::Min(
                    $minimumDecodedTargetHealth,
                    $observedWireB.Health)
                if ($currentClipDeltaField) {
                    $minimumClipDeltaValue = [Math]::Min(
                        $minimumClipDeltaValue,
                        [double]$observedWireA.Clip)
                }
                if ($currentHealthDeltaField) {
                    $minimumHealthDeltaValue = [Math]::Min(
                        $minimumHealthDeltaValue,
                        $observedWireB.Health)
                }
                if ($index -lt 10) {
                    continue
                }
                $progressAfterA = Get-LatestCombatProgress `
                    -OutputCapture $outputCapture `
                    -StdoutPath $stdoutPath `
                    -Slot 1
                $progressAfterB = Get-LatestCombatProgress `
                    -OutputCapture $outputCapture `
                    -StdoutPath $stdoutPath `
                    -Slot 2
                if ($progressAfterA.PmoveCalls -gt
                        $progressBeforeA.PmoveCalls -and
                    $progressAfterA.ForwardInputs -gt
                        $progressBeforeA.ForwardInputs -and
                    $progressAfterA.ServerTimeMs -gt
                        $progressBeforeA.ServerTimeMs -and
                    $progressAfterB.PmoveCalls -gt
                        $progressBeforeB.PmoveCalls -and
                    $progressAfterB.StrafeInputs -gt
                        $progressBeforeB.StrafeInputs -and
                    $progressAfterB.ServerTimeMs -gt
                        $progressBeforeB.ServerTimeMs) {
                    $movementAdvanced = $true
                    $wireAfterA = Get-CombatWireState -State $stateA
                    $wireAfterB = Get-CombatWireState -State $stateB
                    if ($wireBeforeA.Clip - $wireAfterA.Clip -eq 1 -and
                        $wireAfterB.Health -lt $wireBeforeB.Health) {
                        $wireTransitionObserved = $true
                        break
                    }
                }
            }
            if (-not $movementAdvanced) {
                throw "post-shot movement progression gate failed"
            }
            if ($progressAfterA.AttackReceived -le
                    $progressBeforeA.AttackReceived) {
                throw "positive combat attack receipt gate failed"
            }
            if ($progressAfterA.ActiveWeapon -ne 2 -or
                -not $progressAfterA.GlockPresent) {
                throw "positive combat Glock readiness gate failed"
            }
            if ($progressAfterA.AttackExecuted -le
                    $progressBeforeA.AttackExecuted) {
                throw "positive combat attack execution gate failed"
            }
            if (-not $wireTransitionObserved) {
                $wireAfterA = Get-CombatWireState -State $stateA
                $wireAfterB = Get-CombatWireState -State $stateB
            }
            if ($progressAfterB.Health -ge $progressBeforeB.Health) {
                if ($progressAfterA.ShotTraces -le
                        $progressBeforeA.ShotTraces) {
                    throw "positive combat weapon trace gate failed"
                }
                if ($progressAfterA.ShotPlayerHits -le
                        $progressBeforeA.ShotPlayerHits) {
                    if ($progressAfterA.ShotWorldHits -gt
                            $progressBeforeA.ShotWorldHits) {
                        throw "positive combat unexpected world hit gate failed"
                    }
                    throw "positive combat player trace hit gate failed"
                }
                if ($progressAfterA.LastTarget -ne 2) {
                    throw "positive combat trace target routing gate failed"
                }
                throw "positive combat authoritative damage gate failed"
            }
            if ($progressAfterB.ClientDataHealth -ge
                    $progressBeforeB.ClientDataHealth) {
                throw "positive combat target UpdateClientData health gate failed"
            }
            if ($wireAfterB.Health -ge $wireBeforeB.Health) {
                if (-not $healthDeltaFieldObserved) {
                    throw "positive combat target health delta mask gate failed"
                }
                if ($minimumDecodedTargetHealth -lt $wireBeforeB.Health) {
                    throw "positive combat target health history reversion gate failed"
                }
                if (-not [double]::IsNaN($minimumHealthDeltaValue) -and
                    -not [double]::IsInfinity($minimumHealthDeltaValue) -and
                    [Math]::Abs(
                        $minimumHealthDeltaValue -
                            (2.0 * $progressAfterB.ClientDataHealth)) -le
                        0.01) {
                    throw "positive combat target health signed decode gate failed"
                }
                if ([Math]::Abs(
                        $minimumHealthDeltaValue - $wireBeforeB.Health) -le
                    0.01) {
                    throw "positive combat target health selected unchanged gate failed"
                }
                if ($minimumHealthDeltaValue -gt $wireBeforeB.Health) {
                    throw "positive combat target health selected increase gate failed"
                }
                throw "positive combat target health field decode gate failed"
            }
            $shooterClipDelta = $wireBeforeA.Clip - $wireAfterA.Clip
            if ($shooterClipDelta -eq 0) {
                if ($progressBeforeA.GlockClip -
                        $progressAfterA.GlockClip -eq 1) {
                    if (-not $clipDeltaFieldObserved) {
                        throw "positive combat shooter clip delta mask gate failed"
                    }
                    if ($minimumDecodedShooterClip -lt $wireBeforeA.Clip) {
                        throw "positive combat shooter clip history reversion gate failed"
                    }
                    if (-not [double]::IsNaN($minimumClipDeltaValue) -and
                        -not [double]::IsInfinity($minimumClipDeltaValue) -and
                        [Math]::Abs(
                            $minimumClipDeltaValue -
                                (2.0 * $progressAfterA.GlockClip)) -le 0.01) {
                        throw "positive combat shooter clip signed decode gate failed"
                    }
                    throw "positive combat shooter clip field decode gate failed"
                }
                throw "positive combat authoritative Glock clip unchanged gate failed"
            }
            if ($shooterClipDelta -ne 1) {
                throw "positive combat shooter clip exact decrement gate failed"
            }
            if ($wireAfterA.Health -ne $wireBeforeA.Health) {
                throw "positive combat shooter health isolation gate failed"
            }
            if ($wireAfterB.Clip -ne $wireBeforeB.Clip) {
                throw "positive combat target clip isolation gate failed"
            }
            if ($wireAfterB.Health -ge $wireBeforeB.Health) {
                if ($progressAfterA.ShotTraces -le
                        $progressBeforeA.ShotTraces) {
                    throw "positive combat weapon trace gate failed"
                }
                if ($progressAfterA.ShotPlayerHits -le
                        $progressBeforeA.ShotPlayerHits) {
                    if ($progressAfterA.ShotWorldHits -gt
                            $progressBeforeA.ShotWorldHits) {
                        throw "positive combat unexpected world hit gate failed"
                    }
                    throw "positive combat player trace hit gate failed"
                }
                if ($progressAfterA.LastTarget -ne 2) {
                    throw "positive combat trace target routing gate failed"
                }
                throw "positive combat authoritative damage gate failed"
            }
            if ($wireAfterB.Health -le 0.0) {
                throw "positive combat nonlethal target gate failed"
            }
            $combatEvidence = [pscustomobject]@{
                ShooterHealthBefore = $wireBeforeA.Health
                ShooterHealthAfter = $wireAfterA.Health
                ShooterClipBefore = $wireBeforeA.Clip
                ShooterClipAfter = $wireAfterA.Clip
                TargetHealthBefore = $wireBeforeB.Health
                TargetHealthAfter = $wireAfterB.Health
                TargetClipBefore = $wireBeforeB.Clip
                TargetClipAfter = $wireAfterB.Clip
                TargetClientDataUpdated = $true
                ShooterWeaponDataUpdated = $true
                ClientAMovementAfterShot = $true
                ClientBMovementAfterShot = $true
                WallOccluded = $false
            }
            if ($LethalDeathProof) {
                $proofStage = "lethal_control_validation"
                [double]$lethalDamagePerShot =
                    $wireBeforeB.Health - $wireAfterB.Health
                if ([Math]::Abs($wireBeforeB.Health - 100.0) -gt 0.01 -or
                    [Math]::Abs($wireAfterB.Health - 88.0) -gt 0.01 -or
                    [Math]::Abs($lethalDamagePerShot - 12.0) -gt 0.01 -or
                    $wireBeforeA.Clip - $wireAfterA.Clip -ne 1 -or
                    $progressAfterA.AttackReceived -
                        $progressBeforeA.AttackReceived -ne 1 -or
                    $progressAfterA.AttackExecuted -
                        $progressBeforeA.AttackExecuted -ne 1 -or
                    $progressAfterA.ShotTraces -
                        $progressBeforeA.ShotTraces -ne 2 -or
                    $progressAfterA.ShotPlayerHits -
                        $progressBeforeA.ShotPlayerHits -ne 2 -or
                    $progressAfterA.ShotWorldHits -
                        $progressBeforeA.ShotWorldHits -ne 0 -or
                    $progressAfterA.LastTarget -ne 2 -or
                    $progressAfterB.Deadflag -ne 0 -or
                    -not $progressAfterB.GlockPresent -or
                    $progressAfterA.CallbackFailures -ne 0 -or
                    $progressAfterB.CallbackFailures -ne 0 -or
                    $progressAfterA.PreThink -le $progressBeforeA.PreThink -or
                    $progressAfterA.PostThink -le $progressBeforeA.PostThink -or
                    $progressAfterB.PreThink -le $progressBeforeB.PreThink -or
                    $progressAfterB.PostThink -le $progressBeforeB.PostThink -or
                    $stateA.Snapshots -le $controlSnapshotsBeforeA -or
                    $stateB.Snapshots -le $controlSnapshotsBeforeB) {
                    throw "lethal death nonlethal control gate failed"
                }

                $lethalWireAfterA = $wireAfterA
                $lethalWireAfterB = Get-CombatLifeWireState -State $stateB
                $lethalProgressAfterA = $progressAfterA
                $lethalProgressAfterB = $progressAfterB
                for ($shotNumber = 1; $shotNumber -le 1; $shotNumber++) {
                    $proofStage = "lethal_cooldown_$shotNumber"
                    Invoke-CombatSnapshotPump `
                        -StateA $stateA `
                        -StateB $stateB `
                        -ServerProcess $serverProcess `
                        -OutputCapture $outputCapture `
                        -StdoutPath $stdoutPath `
                        -Deadline $deadline `
                        -ServerEndpoint $serverEndpoint `
                        -Count 40 `
                        -YawA $fixture.Yaw
                    if ($serverProcess.HasExited) {
                        throw "lethal death cooldown server unavailable"
                    }
                    Wait-CombatZeroInputExecution `
                        -StateA $stateA `
                        -StateB $stateB `
                        -ShooterSlot 1 `
                        -ServerProcess $serverProcess `
                        -OutputCapture $outputCapture `
                        -StdoutPath $stdoutPath `
                        -Deadline $deadline `
                        -ServerEndpoint $serverEndpoint `
                        -YawA $fixture.Yaw
                    Sync-TwoClientSnapshotFrontier `
                        -StateA $stateA `
                        -StateB $stateB `
                        -ServerProcess $serverProcess `
                        -OutputCapture $outputCapture `
                        -StdoutPath $stdoutPath `
                        -Deadline $deadline `
                        -ServerEndpoint $serverEndpoint

                    $proofStage = "lethal_fixture_$shotNumber"
                    $fixture = Set-StockCombatFixture `
                        -ControlPath $controlPath `
                        -ShooterSlot 1 `
                        -ShooterGeneration $generations[1] `
                        -TargetSlot 2 `
                        -TargetGeneration $generations[2] `
                        -Mode lethal `
                        -OutputCapture $outputCapture `
                        -StdoutPath $stdoutPath `
                        -Deadline $deadline
                    if ($fixture.Mode -cne 'lethal' -or
                        $fixture.WorldFraction -lt 0.999) {
                        throw "lethal death clear fixture gate failed"
                    }
                    Wait-CombatHealthSnapshotWithoutMove `
                        -StateA $stateA `
                        -StateB $stateB `
                        -TargetState $stateB `
                        -ExpectedHealth 4.0 `
                        -ServerProcess $serverProcess `
                        -OutputCapture $outputCapture `
                        -Deadline $deadline `
                        -ServerEndpoint $serverEndpoint

                    $lethalWireBeforeA = Get-CombatWireState -State $stateA
                    $lethalWireBeforeB = Get-CombatLifeWireState -State $stateB
                    $lethalProgressBeforeA = Get-LatestCombatProgress `
                        -OutputCapture $outputCapture `
                        -StdoutPath $stdoutPath `
                        -Slot 1
                    $lethalProgressBeforeB = Get-LatestCombatProgress `
                        -OutputCapture $outputCapture `
                        -StdoutPath $stdoutPath `
                        -Slot 2
                    $lethalSnapshotsBeforeA = $stateA.Snapshots
                    $lethalSnapshotsBeforeB = $stateB.Snapshots
                    [double]$expectedHealthBefore = 4.0
                    if ([Math]::Abs(
                            $lethalWireBeforeB.Health -
                                $expectedHealthBefore) -gt 0.01 -or
                        [Math]::Abs($lethalWireBeforeA.Health - 100.0) -gt
                            0.01 -or
                        -not $lethalWireBeforeB.GlockPresent -or
                        [Math]::Abs(
                            $lethalProgressBeforeB.Health - 4.0) -gt 0.01 -or
                        [Math]::Abs(
                            $lethalProgressBeforeB.ClientDataHealth - 4.0
                        ) -gt 0.01 -or
                        $lethalProgressBeforeA.Deadflag -ne 0 -or
                        -not $lethalProgressBeforeA.GlockPresent -or
                        $lethalProgressBeforeB.Deadflag -ne 0 -or
                        -not $lethalProgressBeforeB.GlockPresent -or
                        $lethalProgressBeforeA.CallbackFailures -ne 0 -or
                        $lethalProgressBeforeB.CallbackFailures -ne 0) {
                        throw "lethal death pre-shot state gate failed"
                    }

                    $proofStage = "lethal_first_shot_send"
                    [void](Send-TwoClientCombatMove `
                        -State $stateA `
                        -Yaw $fixture.Yaw `
                        -Buttons 1)
                    $proofStage = "lethal_first_shot_transition"
                    $lethalTransitionObserved = $false
                    $lethalWireAfterA = $lethalWireBeforeA
                    $lethalWireAfterB = $lethalWireBeforeB
                    $lethalProgressAfterA = $lethalProgressBeforeA
                    $lethalProgressAfterB = $lethalProgressBeforeB
                    for ($index = 0; $index -lt 80; $index++) {
                        Invoke-CombatSnapshotPump `
                            -StateA $stateA `
                            -StateB $stateB `
                            -ServerProcess $serverProcess `
                            -OutputCapture $outputCapture `
                            -StdoutPath $stdoutPath `
                            -Deadline $deadline `
                            -ServerEndpoint $serverEndpoint `
                            -Count 1 `
                            -YawA $fixture.Yaw
                        $lethalWireAfterA =
                            Get-CombatWireState -State $stateA
                        $lethalWireAfterB =
                            Get-CombatLifeWireState -State $stateB
                        if ($index -lt 10) {
                            continue
                        }
                        $lethalProgressAfterA = Get-LatestCombatProgress `
                            -OutputCapture $outputCapture `
                            -StdoutPath $stdoutPath `
                            -Slot 1
                        $lethalProgressAfterB = Get-LatestCombatProgress `
                            -OutputCapture $outputCapture `
                            -StdoutPath $stdoutPath `
                            -Slot 2
                        $terminalStateReady =
                            $lethalProgressAfterB.Health -le 0.0 -and
                            $lethalProgressAfterB.Deadflag -ne 0 -and
                            -not $lethalProgressAfterB.GlockPresent
                        if ($lethalProgressAfterA.AttackReceived -eq
                                $lethalProgressBeforeA.AttackReceived + 1 -and
                            $lethalProgressAfterA.AttackExecuted -eq
                                $lethalProgressBeforeA.AttackExecuted + 1 -and
                            $lethalProgressAfterA.GlockClip -eq
                                $lethalProgressBeforeA.GlockClip - 1 -and
                            $lethalProgressAfterB.Health -lt
                                $lethalProgressBeforeB.Health -and
                            $lethalProgressAfterA.PreThink -gt
                                $lethalProgressBeforeA.PreThink -and
                            $lethalProgressAfterA.PostThink -gt
                                $lethalProgressBeforeA.PostThink -and
                            $lethalProgressAfterB.PreThink -gt
                                $lethalProgressBeforeB.PreThink -and
                            $lethalProgressAfterB.PostThink -gt
                                $lethalProgressBeforeB.PostThink -and
                            $stateA.Snapshots -gt $lethalSnapshotsBeforeA -and
                            $stateB.Snapshots -gt $lethalSnapshotsBeforeB -and
                            $terminalStateReady) {
                            $lethalTransitionObserved = $true
                            break
                        }
                    }
                    $proofStage = "lethal_shot_transition_$shotNumber"
                    if (-not $lethalTransitionObserved -or
                        $serverProcess.HasExited) {
                        if ($serverProcess.HasExited) {
                            $proofStage = "lethal_transition_server_unavailable"
                        } elseif ($lethalProgressAfterA.AttackReceived -ne
                                $lethalProgressBeforeA.AttackReceived + 1) {
                            $proofStage = "lethal_transition_attack_not_received"
                        } elseif ($lethalProgressAfterA.AttackExecuted -ne
                                $lethalProgressBeforeA.AttackExecuted + 1) {
                            $proofStage = "lethal_transition_attack_not_executed"
                        } elseif ($lethalProgressAfterA.GlockClip -ne
                                $lethalProgressBeforeA.GlockClip - 1) {
                            $proofStage = "lethal_transition_clip_unchanged"
                        } elseif ($lethalProgressAfterB.Health -ge
                                $lethalProgressBeforeB.Health) {
                            if ($lethalProgressAfterA.ShotTraces -ne
                                    $lethalProgressBeforeA.ShotTraces + 2) {
                                $proofStage = "lethal_transition_trace_missing"
                            } elseif ($lethalProgressAfterA.ShotPlayerHits -ne
                                    $lethalProgressBeforeA.ShotPlayerHits + 2) {
                                if ($lethalProgressAfterA.ShotWorldHits -gt
                                        $lethalProgressBeforeA.ShotWorldHits) {
                                    $proofStage = "lethal_transition_world_hit"
                                } else {
                                    $proofStage = "lethal_transition_player_missed"
                                }
                            } else {
                                $proofStage = "lethal_transition_damage_missing"
                            }
                        } elseif ($lethalProgressAfterB.Deadflag -eq 0) {
                            $proofStage = "lethal_transition_deadflag_missing"
                        } elseif ($lethalWireAfterB.GlockPresent -or
                                $lethalProgressAfterB.GlockPresent) {
                            $proofStage = "lethal_transition_inventory_present"
                        } elseif ($lethalProgressAfterA.PreThink -le
                                $lethalProgressBeforeA.PreThink -or
                            $lethalProgressAfterA.PostThink -le
                                $lethalProgressBeforeA.PostThink -or
                            $lethalProgressAfterB.PreThink -le
                                $lethalProgressBeforeB.PreThink -or
                            $lethalProgressAfterB.PostThink -le
                                $lethalProgressBeforeB.PostThink) {
                            $proofStage = "lethal_transition_callbacks_not_advanced"
                        } elseif ($stateA.Snapshots -le
                                $lethalSnapshotsBeforeA -or
                            $stateB.Snapshots -le $lethalSnapshotsBeforeB) {
                            $proofStage = "lethal_transition_snapshots_not_advanced"
                        } else {
                            $proofStage = "lethal_transition_unclassified"
                        }
                        throw "lethal death shot transition unconfirmed"
                    }

                    $proofStage = "lethal_first_shot_frontier"
                    Sync-TwoClientSnapshotFrontier `
                        -StateA $stateA `
                        -StateB $stateB `
                        -ServerProcess $serverProcess `
                        -OutputCapture $outputCapture `
                        -StdoutPath $stdoutPath `
                        -Deadline $deadline `
                        -ServerEndpoint $serverEndpoint
                    $lethalWireAfterA = Get-CombatWireState -State $stateA
                    $lethalWireAfterB =
                        Get-CombatLifeWireState -State $stateB
                    $lethalProgressAfterA = Get-LatestCombatProgress `
                        -OutputCapture $outputCapture `
                        -StdoutPath $stdoutPath `
                        -Slot 1
                    $lethalProgressAfterB = Get-LatestCombatProgress `
                        -OutputCapture $outputCapture `
                        -StdoutPath $stdoutPath `
                        -Slot 2
                    $proofStage = "lethal_first_shot_exact"
                    [double]$expectedHealthAfter = -8.0
                    $proofStage = "lethal_shot_exact_$shotNumber"
                    if ([Math]::Abs(
                            $lethalWireAfterB.Health -
                                $expectedHealthAfter) -gt 0.01 -or
                        [Math]::Abs(
                            $lethalProgressAfterB.Health -
                                $expectedHealthAfter) -gt 0.01 -or
                        [Math]::Abs(
                            $lethalProgressAfterB.ClientDataHealth -
                                $expectedHealthAfter) -gt 0.01 -or
                        $lethalWireBeforeA.Clip -
                            $lethalWireAfterA.Clip -ne 1 -or
                        $lethalProgressAfterA.AttackReceived -
                            $lethalProgressBeforeA.AttackReceived -ne 1 -or
                        $lethalProgressAfterA.AttackExecuted -
                            $lethalProgressBeforeA.AttackExecuted -ne 1 -or
                        $lethalProgressAfterA.ShotTraces -
                            $lethalProgressBeforeA.ShotTraces -ne 2 -or
                        $lethalProgressAfterA.ShotPlayerHits -
                            $lethalProgressBeforeA.ShotPlayerHits -ne 2 -or
                        $lethalProgressAfterA.ShotWorldHits -
                            $lethalProgressBeforeA.ShotWorldHits -ne 0 -or
                        $lethalProgressAfterA.LastTarget -ne 2 -or
                        $lethalProgressAfterA.CallbackFailures -ne 0 -or
                        $lethalProgressAfterB.CallbackFailures -ne 0 -or
                        $lethalWireAfterA.Health -ne
                            $lethalWireBeforeA.Health) {
                        throw "lethal death exact shot gate failed"
                    }
                    $proofStage = "lethal_first_shot_terminal"
                    $proofStage = "lethal_shot_terminal_$shotNumber"
                    if ($lethalWireAfterB.Health -gt 0.0 -or
                        $lethalWireAfterB.GlockPresent -or
                        $lethalProgressAfterB.Deadflag -eq 0 -or
                        $lethalProgressAfterB.GlockPresent) {
                        throw "lethal death terminal transition gate failed"
                    }
                }

                $proofStage = "lethal_post_death_progression"
                $deathWireA = $lethalWireAfterA
                $deathWireB = $lethalWireAfterB
                $deathProgressA = $lethalProgressAfterA
                $deathProgressB = $lethalProgressAfterB
                $deathSnapshotsA = $stateA.Snapshots
                $deathSnapshotsB = $stateB.Snapshots
                Invoke-CombatSnapshotPump `
                    -StateA $stateA `
                    -StateB $stateB `
                    -ServerProcess $serverProcess `
                    -OutputCapture $outputCapture `
                    -StdoutPath $stdoutPath `
                    -Deadline $deadline `
                    -ServerEndpoint $serverEndpoint `
                    -Count 32 `
                    -YawA $fixture.Yaw
                $postDeathWireA = Get-CombatWireState -State $stateA
                $postDeathWireB = Get-CombatLifeWireState -State $stateB
                $postDeathProgressA = Get-LatestCombatProgress `
                    -OutputCapture $outputCapture `
                    -StdoutPath $stdoutPath `
                    -Slot 1
                $postDeathProgressB = Get-LatestCombatProgress `
                    -OutputCapture $outputCapture `
                    -StdoutPath $stdoutPath `
                    -Slot 2
                if ($serverProcess.HasExited -or
                    -not $postDeathProgressA.Connected -or
                    -not $postDeathProgressA.SignonComplete -or
                    -not $postDeathProgressA.Spawned -or
                    -not $postDeathProgressB.Connected -or
                    -not $postDeathProgressB.SignonComplete -or
                    -not $postDeathProgressB.Spawned -or
                    $stateA.Snapshots -lt $deathSnapshotsA + 32 -or
                    $stateB.Snapshots -lt $deathSnapshotsB + 32 -or
                    $postDeathWireA.FrameId -le $deathWireA.FrameId -or
                    $postDeathWireB.FrameId -le $deathWireB.FrameId -or
                    $postDeathProgressA.ServerTimeMs -le
                        $deathProgressA.ServerTimeMs -or
                    $postDeathProgressB.ServerTimeMs -le
                        $deathProgressB.ServerTimeMs -or
                    $postDeathProgressA.MoveReceived -le
                        $deathProgressA.MoveReceived -or
                    $postDeathProgressA.MoveValidated -le
                        $deathProgressA.MoveValidated -or
                    $postDeathProgressA.MoveExecuted -le
                        $deathProgressA.MoveExecuted -or
                    $postDeathProgressA.PmoveCalls -le
                        $deathProgressA.PmoveCalls -or
                    $postDeathProgressA.PreThink -le
                        $deathProgressA.PreThink -or
                    $postDeathProgressA.PostThink -le
                        $deathProgressA.PostThink -or
                    $postDeathProgressB.MoveReceived -le
                        $deathProgressB.MoveReceived -or
                    $postDeathProgressB.MoveValidated -le
                        $deathProgressB.MoveValidated -or
                    $postDeathProgressB.MoveExecuted -le
                        $deathProgressB.MoveExecuted -or
                    $postDeathProgressB.PmoveCalls -le
                        $deathProgressB.PmoveCalls -or
                    $postDeathProgressB.PreThink -le
                        $deathProgressB.PreThink -or
                    $postDeathProgressB.PostThink -le
                        $deathProgressB.PostThink -or
                    $postDeathProgressA.CallbackFailures -ne 0 -or
                    $postDeathProgressB.CallbackFailures -ne 0) {
                    throw "lethal death post-death progression gate failed"
                }
                if ([Math]::Abs(
                        $postDeathWireB.Health - $deathWireB.Health) -gt
                            0.01 -or
                    $postDeathWireB.Health -gt 0.0 -or
                    $postDeathWireB.GlockPresent -or
                    $postDeathProgressB.Deadflag -eq 0 -or
                    $postDeathProgressB.GlockPresent -or
                    $postDeathProgressA.AttackReceived -ne
                        $deathProgressA.AttackReceived -or
                    $postDeathProgressA.AttackExecuted -ne
                        $deathProgressA.AttackExecuted -or
                    $postDeathWireA.Health -ne $deathWireA.Health -or
                    $postDeathWireA.Clip -ne $deathWireA.Clip) {
                    throw "lethal death stable terminal state gate failed"
                }

                $proofStage = "lethal_first_respawn_ready"
                $firstRespawnReady = $false
                for ($index = 0; $index -lt 520; $index++) {
                    Invoke-CombatSnapshotPump `
                        -StateA $stateA `
                        -StateB $stateB `
                        -ServerProcess $serverProcess `
                        -OutputCapture $outputCapture `
                        -StdoutPath $stdoutPath `
                        -Deadline $deadline `
                        -ServerEndpoint $serverEndpoint `
                        -Count 1 `
                        -YawA $fixture.Yaw
                    $firstRespawnProgressB = Get-LatestCombatProgress `
                        -OutputCapture $outputCapture `
                        -StdoutPath $stdoutPath `
                        -Slot 2
                    if ($firstRespawnProgressB.Deadflag -eq 3) {
                        $firstRespawnReady = $true
                        break
                    }
                }
                if (-not $firstRespawnReady -or $serverProcess.HasExited) {
                    throw "lethal first respawn readiness gate failed"
                }

                $proofStage = "lethal_first_death_camera_dwell"
                Invoke-CombatSnapshotPump `
                    -StateA $stateA `
                    -StateB $stateB `
                    -ServerProcess $serverProcess `
                    -OutputCapture $outputCapture `
                    -StdoutPath $stdoutPath `
                    -Deadline $deadline `
                    -ServerEndpoint $serverEndpoint `
                    -Count 160 `
                    -YawA $fixture.Yaw
                $firstDeathCameraProgressB = Get-LatestCombatProgress `
                    -OutputCapture $outputCapture `
                    -StdoutPath $stdoutPath `
                    -Slot 2
                if ($serverProcess.HasExited -or
                    $firstDeathCameraProgressB.Deadflag -ne 3 -or
                    $firstDeathCameraProgressB.CallbackFailures -ne 0) {
                    throw "lethal first death camera dwell gate failed"
                }

                $proofStage = "lethal_first_respawn"
                $firstRespawnAttackBeforeB =
                    $firstDeathCameraProgressB.AttackReceived
                $firstRespawnExecutedBeforeB =
                    $firstDeathCameraProgressB.AttackExecuted
                $firstRespawnInputsBeforeB =
                    $firstDeathCameraProgressB.RespawnInputsForwarded
                [void](Send-TwoClientCombatMove `
                    -State $stateB `
                    -Yaw 0.0 `
                    -Buttons 1 `
                    -Msec 100)
                $firstRespawnObserved = $false
                for ($index = 0; $index -lt 520; $index++) {
                    Invoke-CombatSnapshotPump `
                        -StateA $stateA `
                        -StateB $stateB `
                        -ServerProcess $serverProcess `
                        -OutputCapture $outputCapture `
                        -StdoutPath $stdoutPath `
                        -Deadline $deadline `
                        -ServerEndpoint $serverEndpoint `
                        -Count 1
                    $firstRespawnProgressB = Get-LatestCombatProgress `
                        -OutputCapture $outputCapture `
                        -StdoutPath $stdoutPath `
                        -Slot 2
                    if ($firstRespawnProgressB.Deadflag -eq 0 -and
                        $firstRespawnProgressB.GlockPresent -and
                        $firstRespawnProgressB.RespawnInputsForwarded -eq
                            $firstRespawnInputsBeforeB + 1 -and
                        $firstRespawnProgressB.AttackReceived -eq
                            $firstRespawnAttackBeforeB -and
                        $firstRespawnProgressB.AttackExecuted -eq
                            $firstRespawnExecutedBeforeB -and
                        [Math]::Abs($firstRespawnProgressB.Health - 100.0) -le
                            0.01) {
                        $firstRespawnObserved = $true
                        break
                    }
                }
                if (-not $firstRespawnObserved -or $serverProcess.HasExited) {
                    throw "lethal first respawn transition gate failed"
                }
                Sync-TwoClientSnapshotFrontier `
                    -StateA $stateA `
                    -StateB $stateB `
                    -ServerProcess $serverProcess `
                    -OutputCapture $outputCapture `
                    -StdoutPath $stdoutPath `
                    -Deadline $deadline `
                    -ServerEndpoint $serverEndpoint
                $firstRespawnWireB = Get-CombatWireState -State $stateB
                if ([Math]::Abs($firstRespawnWireB.Health - 100.0) -gt 0.01 -or
                    $firstRespawnWireB.Clip -ne 17) {
                    throw "lethal first respawn inventory gate failed"
                }
                Invoke-CombatSnapshotPump `
                    -StateA $stateA `
                    -StateB $stateB `
                    -ServerProcess $serverProcess `
                    -OutputCapture $outputCapture `
                    -StdoutPath $stdoutPath `
                    -Deadline $deadline `
                    -ServerEndpoint $serverEndpoint `
                    -Count 100

                $secondDeathWireA = $null
                $secondDeathWireB = $null
                $secondDeathProgressA = $null
                $secondDeathProgressB = $null
                for ($shotNumber = 1; $shotNumber -le 1; $shotNumber++) {
                    $proofStage = "lethal_second_cooldown_$shotNumber"
                    Invoke-CombatSnapshotPump `
                        -StateA $stateA `
                        -StateB $stateB `
                        -ServerProcess $serverProcess `
                        -OutputCapture $outputCapture `
                        -StdoutPath $stdoutPath `
                        -Deadline $deadline `
                        -ServerEndpoint $serverEndpoint `
                        -Count 40
                    Wait-CombatZeroInputExecution `
                        -StateA $stateA `
                        -StateB $stateB `
                        -ShooterSlot 2 `
                        -ServerProcess $serverProcess `
                        -OutputCapture $outputCapture `
                        -StdoutPath $stdoutPath `
                        -Deadline $deadline `
                        -ServerEndpoint $serverEndpoint
                    Sync-TwoClientSnapshotFrontier `
                        -StateA $stateA `
                        -StateB $stateB `
                        -ServerProcess $serverProcess `
                        -OutputCapture $outputCapture `
                        -StdoutPath $stdoutPath `
                        -Deadline $deadline `
                        -ServerEndpoint $serverEndpoint

                    $proofStage = "lethal_second_fixture_$shotNumber"
                    $secondFixture = Set-StockCombatFixture `
                        -ControlPath $controlPath `
                        -ShooterSlot 2 `
                        -ShooterGeneration $generations[2] `
                        -TargetSlot 1 `
                        -TargetGeneration $generations[1] `
                        -Mode lethal `
                        -OutputCapture $outputCapture `
                        -StdoutPath $stdoutPath `
                        -Deadline $deadline
                    if ($secondFixture.Mode -cne 'lethal' -or
                        $secondFixture.WorldFraction -lt 0.999) {
                        throw "lethal second clear fixture gate failed"
                    }
                    Wait-CombatHealthSnapshotWithoutMove `
                        -StateA $stateA `
                        -StateB $stateB `
                        -TargetState $stateA `
                        -ExpectedHealth 4.0 `
                        -ServerProcess $serverProcess `
                        -OutputCapture $outputCapture `
                        -Deadline $deadline `
                        -ServerEndpoint $serverEndpoint

                    $secondWireBeforeA = Get-CombatLifeWireState -State $stateA
                    $secondWireBeforeB = Get-CombatWireState -State $stateB
                    $secondProgressBeforeA = Get-LatestCombatProgress `
                        -OutputCapture $outputCapture `
                        -StdoutPath $stdoutPath `
                        -Slot 1
                    $secondProgressBeforeB = Get-LatestCombatProgress `
                        -OutputCapture $outputCapture `
                        -StdoutPath $stdoutPath `
                        -Slot 2
                    [double]$secondExpectedBefore = 4.0
                    if ([Math]::Abs(
                            $secondWireBeforeA.Health -
                                $secondExpectedBefore) -gt 0.01 -or
                        [Math]::Abs($secondWireBeforeB.Health - 100.0) -gt
                            0.01 -or
                        [Math]::Abs(
                            $secondProgressBeforeA.Health - 4.0) -gt 0.01 -or
                        [Math]::Abs(
                            $secondProgressBeforeA.ClientDataHealth - 4.0
                        ) -gt 0.01 -or
                        -not $secondWireBeforeA.GlockPresent -or
                        $secondProgressBeforeA.Deadflag -ne 0 -or
                        -not $secondProgressBeforeA.GlockPresent -or
                        $secondProgressBeforeB.Deadflag -ne 0 -or
                        -not $secondProgressBeforeB.GlockPresent -or
                        $secondProgressBeforeA.CallbackFailures -ne 0 -or
                        $secondProgressBeforeB.CallbackFailures -ne 0) {
                        throw "lethal second pre-shot state gate failed"
                    }

                    $proofStage = "lethal_second_shot_$shotNumber"
                    [void](Send-TwoClientCombatMove `
                        -State $stateB `
                        -Yaw $secondFixture.Yaw `
                        -Buttons 1)
                    $secondTransitionObserved = $false
                    for ($index = 0; $index -lt 80; $index++) {
                        Invoke-CombatSnapshotPump `
                            -StateA $stateA `
                            -StateB $stateB `
                            -ServerProcess $serverProcess `
                            -OutputCapture $outputCapture `
                            -StdoutPath $stdoutPath `
                            -Deadline $deadline `
                            -ServerEndpoint $serverEndpoint `
                            -Count 1 `
                            -YawB $secondFixture.Yaw
                        $secondDeathWireA =
                            Get-CombatLifeWireState -State $stateA
                        $secondDeathWireB =
                            Get-CombatWireState -State $stateB
                        if ($index -lt 10) {
                            continue
                        }
                        $secondDeathProgressA = Get-LatestCombatProgress `
                            -OutputCapture $outputCapture `
                            -StdoutPath $stdoutPath `
                            -Slot 1
                        $secondDeathProgressB = Get-LatestCombatProgress `
                            -OutputCapture $outputCapture `
                            -StdoutPath $stdoutPath `
                            -Slot 2
                        $secondTerminalReady =
                            $secondDeathProgressA.Health -le 0.0 -and
                            $secondDeathProgressA.Deadflag -ne 0 -and
                            -not $secondDeathProgressA.GlockPresent
                        if ($secondDeathProgressB.AttackExecuted -eq
                                $secondProgressBeforeB.AttackExecuted + 1 -and
                            $secondDeathProgressB.AttackReceived -eq
                                $secondProgressBeforeB.AttackReceived + 1 -and
                            $secondDeathProgressB.ShotTraces -eq
                                $secondProgressBeforeB.ShotTraces + 2 -and
                            $secondDeathProgressB.ShotPlayerHits -eq
                                $secondProgressBeforeB.ShotPlayerHits + 2 -and
                            $secondDeathProgressB.ShotWorldHits -eq
                                $secondProgressBeforeB.ShotWorldHits -and
                            $secondDeathProgressB.LastTarget -eq 1 -and
                            $secondDeathProgressB.GlockClip -eq
                                $secondProgressBeforeB.GlockClip - 1 -and
                            $secondDeathProgressA.Health -lt
                                $secondProgressBeforeA.Health -and
                            $secondTerminalReady) {
                            $secondTransitionObserved = $true
                            break
                        }
                    }
                    $proofStage = "lethal_second_shot_transition_$shotNumber"
                    if (-not $secondTransitionObserved -or
                        $serverProcess.HasExited) {
                        throw "lethal second shot transition unconfirmed"
                    }
                    Sync-TwoClientSnapshotFrontier `
                        -StateA $stateA `
                        -StateB $stateB `
                        -ServerProcess $serverProcess `
                        -OutputCapture $outputCapture `
                        -StdoutPath $stdoutPath `
                        -Deadline $deadline `
                        -ServerEndpoint $serverEndpoint
                    $secondDeathWireA =
                        Get-CombatLifeWireState -State $stateA
                    $secondDeathWireB = Get-CombatWireState -State $stateB
                    $secondDeathProgressA = Get-LatestCombatProgress `
                        -OutputCapture $outputCapture `
                        -StdoutPath $stdoutPath `
                        -Slot 1
                    $secondDeathProgressB = Get-LatestCombatProgress `
                        -OutputCapture $outputCapture `
                        -StdoutPath $stdoutPath `
                        -Slot 2
                    [double]$secondExpectedAfter = -8.0
                    $proofStage = "lethal_second_shot_exact_$shotNumber"
                    if ([Math]::Abs(
                            $secondDeathWireA.Health -
                                $secondExpectedAfter) -gt 0.01 -or
                        [Math]::Abs(
                            $secondDeathProgressA.Health -
                                $secondExpectedAfter) -gt 0.01 -or
                        [Math]::Abs(
                            $secondDeathProgressA.ClientDataHealth -
                                $secondExpectedAfter) -gt 0.01 -or
                        $secondDeathProgressA.Deadflag -eq 0 -or
                        $secondDeathWireA.GlockPresent -or
                        $secondDeathProgressA.GlockPresent -or
                        $secondDeathProgressA.CallbackFailures -ne 0 -or
                        $secondDeathProgressB.CallbackFailures -ne 0) {
                        throw "lethal second exact shot gate failed"
                    }
                }

                $proofStage = "lethal_second_respawn_ready"
                $secondRespawnReady = $false
                for ($index = 0; $index -lt 520; $index++) {
                    Invoke-CombatSnapshotPump `
                        -StateA $stateA `
                        -StateB $stateB `
                        -ServerProcess $serverProcess `
                        -OutputCapture $outputCapture `
                        -StdoutPath $stdoutPath `
                        -Deadline $deadline `
                        -ServerEndpoint $serverEndpoint `
                        -Count 1 `
                        -YawB $secondFixture.Yaw
                    $secondRespawnProgressA = Get-LatestCombatProgress `
                        -OutputCapture $outputCapture `
                        -StdoutPath $stdoutPath `
                        -Slot 1
                    if ($secondRespawnProgressA.Deadflag -eq 3) {
                        $secondRespawnReady = $true
                        break
                    }
                }
                if (-not $secondRespawnReady -or $serverProcess.HasExited) {
                    throw "lethal second respawn readiness gate failed"
                }

                $proofStage = "lethal_second_death_camera_dwell"
                Invoke-CombatSnapshotPump `
                    -StateA $stateA `
                    -StateB $stateB `
                    -ServerProcess $serverProcess `
                    -OutputCapture $outputCapture `
                    -StdoutPath $stdoutPath `
                    -Deadline $deadline `
                    -ServerEndpoint $serverEndpoint `
                    -Count 160 `
                    -YawB $secondFixture.Yaw
                $secondDeathCameraProgressA = Get-LatestCombatProgress `
                    -OutputCapture $outputCapture `
                    -StdoutPath $stdoutPath `
                    -Slot 1
                if ($serverProcess.HasExited -or
                    $secondDeathCameraProgressA.Deadflag -ne 3 -or
                    $secondDeathCameraProgressA.CallbackFailures -ne 0) {
                    throw "lethal second death camera dwell gate failed"
                }

                $proofStage = "lethal_second_respawn"
                $secondRespawnAttackBeforeA =
                    $secondDeathCameraProgressA.AttackReceived
                $secondRespawnExecutedBeforeA =
                    $secondDeathCameraProgressA.AttackExecuted
                $secondRespawnInputsBeforeA =
                    $secondDeathCameraProgressA.RespawnInputsForwarded
                [void](Send-TwoClientCombatMove `
                    -State $stateA `
                    -Yaw 0.0 `
                    -Buttons 1 `
                    -Msec 100)
                $secondRespawnObserved = $false
                for ($index = 0; $index -lt 520; $index++) {
                    Invoke-CombatSnapshotPump `
                        -StateA $stateA `
                        -StateB $stateB `
                        -ServerProcess $serverProcess `
                        -OutputCapture $outputCapture `
                        -StdoutPath $stdoutPath `
                        -Deadline $deadline `
                        -ServerEndpoint $serverEndpoint `
                        -Count 1
                    $secondRespawnProgressA = Get-LatestCombatProgress `
                        -OutputCapture $outputCapture `
                        -StdoutPath $stdoutPath `
                        -Slot 1
                    if ($secondRespawnProgressA.Deadflag -eq 0 -and
                        $secondRespawnProgressA.GlockPresent -and
                        $secondRespawnProgressA.RespawnInputsForwarded -eq
                            $secondRespawnInputsBeforeA + 1 -and
                        $secondRespawnProgressA.AttackReceived -eq
                            $secondRespawnAttackBeforeA -and
                        $secondRespawnProgressA.AttackExecuted -eq
                            $secondRespawnExecutedBeforeA -and
                        [Math]::Abs($secondRespawnProgressA.Health - 100.0) -le
                            0.01) {
                        $secondRespawnObserved = $true
                        break
                    }
                }
                if (-not $secondRespawnObserved -or $serverProcess.HasExited) {
                    throw "lethal second respawn transition gate failed"
                }
                Sync-TwoClientSnapshotFrontier `
                    -StateA $stateA `
                    -StateB $stateB `
                    -ServerProcess $serverProcess `
                    -OutputCapture $outputCapture `
                    -StdoutPath $stdoutPath `
                    -Deadline $deadline `
                    -ServerEndpoint $serverEndpoint
                $secondRespawnWireA = Get-CombatWireState -State $stateA
                $secondRespawnWireB = Get-CombatWireState -State $stateB
                if ([Math]::Abs($secondRespawnWireA.Health - 100.0) -gt 0.01 -or
                    [Math]::Abs($secondRespawnWireB.Health - 100.0) -gt 0.01 -or
                    $secondRespawnWireA.Clip -ne 17 -or
                    $secondRespawnWireB.Clip -ne 16) {
                    throw "lethal second respawn inventory gate failed"
                }

                $repeatedDeathCycles = 2
                $repeatedSameVictimDeaths = 1
                for ($deathCycle = 3; $deathCycle -le 8; $deathCycle++) {
                    $proofStage = "lethal_repeated_release_$deathCycle"
                    Wait-CombatZeroInputExecution `
                        -StateA $stateA `
                        -StateB $stateB `
                        -ShooterSlot 1 `
                        -ServerProcess $serverProcess `
                        -OutputCapture $outputCapture `
                        -StdoutPath $stdoutPath `
                        -Deadline $deadline `
                        -ServerEndpoint $serverEndpoint
                    Sync-TwoClientSnapshotFrontier `
                        -StateA $stateA `
                        -StateB $stateB `
                        -ServerProcess $serverProcess `
                        -OutputCapture $outputCapture `
                        -StdoutPath $stdoutPath `
                        -Deadline $deadline `
                        -ServerEndpoint $serverEndpoint
                    $proofStage = "lethal_repeated_fixture_$deathCycle"
                    $repeatedFixture = Set-StockCombatFixture `
                        -ControlPath $controlPath `
                        -ShooterSlot 1 `
                        -ShooterGeneration $generations[1] `
                        -TargetSlot 2 `
                        -TargetGeneration $generations[2] `
                        -Mode lethal `
                        -OutputCapture $outputCapture `
                        -StdoutPath $stdoutPath `
                        -Deadline $deadline
                    if ($repeatedFixture.Mode -cne 'lethal' -or
                        $repeatedFixture.WorldFraction -lt 0.999) {
                        throw "lethal repeated fixture gate failed"
                    }
                    Wait-CombatHealthSnapshotWithoutMove `
                        -StateA $stateA `
                        -StateB $stateB `
                        -TargetState $stateB `
                        -ExpectedHealth 4.0 `
                        -ServerProcess $serverProcess `
                        -OutputCapture $outputCapture `
                        -Deadline $deadline `
                        -ServerEndpoint $serverEndpoint

                    $repeatedWireBeforeA = Get-CombatWireState -State $stateA
                    $repeatedWireBeforeB = Get-CombatWireState -State $stateB
                    $repeatedProgressBeforeA = Get-LatestCombatProgress `
                        -OutputCapture $outputCapture `
                        -StdoutPath $stdoutPath `
                        -Slot 1
                    $repeatedProgressBeforeB = Get-LatestCombatProgress `
                        -OutputCapture $outputCapture `
                        -StdoutPath $stdoutPath `
                        -Slot 2
                    $expectedRepeatedClipBefore = 20 - $deathCycle
                    $expectedRepeatedTargetClipBefore = if ($deathCycle -eq 3) {
                        16
                    } else {
                        17
                    }
                    if ([Math]::Abs($repeatedWireBeforeA.Health - 100.0) -gt
                            0.01 -or
                        [Math]::Abs($repeatedWireBeforeB.Health - 4.0) -gt
                            0.01 -or
                        $repeatedWireBeforeA.Clip -ne
                            $expectedRepeatedClipBefore -or
                        $repeatedWireBeforeB.Clip -ne
                            $expectedRepeatedTargetClipBefore -or
                        $repeatedProgressBeforeA.Deadflag -ne 0 -or
                        $repeatedProgressBeforeB.Deadflag -ne 0 -or
                        -not $repeatedProgressBeforeA.GlockPresent -or
                        -not $repeatedProgressBeforeB.GlockPresent -or
                        [Math]::Abs($repeatedProgressBeforeB.Health - 4.0) -gt
                            0.01 -or
                        [Math]::Abs(
                            $repeatedProgressBeforeB.ClientDataHealth - 4.0
                        ) -gt 0.01 -or
                        $repeatedProgressBeforeA.CallbackFailures -ne 0 -or
                        $repeatedProgressBeforeB.CallbackFailures -ne 0) {
                        throw "lethal repeated pre-shot state gate failed"
                    }

                    $proofStage = "lethal_repeated_shot_$deathCycle"
                    [void](Send-TwoClientCombatMove `
                        -State $stateA `
                        -Yaw $repeatedFixture.Yaw `
                        -Buttons 1)
                    $repeatedDeathObserved = $false
                    for ($index = 0; $index -lt 120; $index++) {
                        Invoke-CombatSnapshotPump `
                            -StateA $stateA `
                            -StateB $stateB `
                            -ServerProcess $serverProcess `
                            -OutputCapture $outputCapture `
                            -StdoutPath $stdoutPath `
                            -Deadline $deadline `
                            -ServerEndpoint $serverEndpoint `
                            -Count 1 `
                            -YawA $repeatedFixture.Yaw
                        $repeatedDeathWireA =
                            Get-CombatWireState -State $stateA
                        $repeatedDeathWireB =
                            Get-CombatLifeWireState -State $stateB
                        if ($index -lt 10) {
                            continue
                        }
                        $repeatedDeathProgressA = Get-LatestCombatProgress `
                            -OutputCapture $outputCapture `
                            -StdoutPath $stdoutPath `
                            -Slot 1
                        $repeatedDeathProgressB = Get-LatestCombatProgress `
                            -OutputCapture $outputCapture `
                            -StdoutPath $stdoutPath `
                            -Slot 2
                        if ($repeatedDeathProgressA.AttackReceived -eq
                                $repeatedProgressBeforeA.AttackReceived + 1 -and
                            $repeatedDeathProgressA.AttackExecuted -eq
                                $repeatedProgressBeforeA.AttackExecuted + 1 -and
                            $repeatedDeathProgressA.ShotTraces -eq
                                $repeatedProgressBeforeA.ShotTraces + 2 -and
                            $repeatedDeathProgressA.ShotPlayerHits -eq
                                $repeatedProgressBeforeA.ShotPlayerHits + 2 -and
                            $repeatedDeathProgressA.ShotWorldHits -eq
                                $repeatedProgressBeforeA.ShotWorldHits -and
                            $repeatedDeathProgressA.LastTarget -eq 2 -and
                            $repeatedDeathProgressA.GlockClip -eq
                                $repeatedProgressBeforeA.GlockClip - 1 -and
                            $repeatedDeathProgressB.Health -le 0.0 -and
                            $repeatedDeathProgressB.Deadflag -ne 0 -and
                            -not $repeatedDeathProgressB.GlockPresent) {
                            $repeatedDeathObserved = $true
                            break
                        }
                    }
                    if (-not $repeatedDeathObserved -or
                        $serverProcess.HasExited) {
                        throw "lethal repeated death transition gate failed"
                    }
                    Sync-TwoClientSnapshotFrontier `
                        -StateA $stateA `
                        -StateB $stateB `
                        -ServerProcess $serverProcess `
                        -OutputCapture $outputCapture `
                        -StdoutPath $stdoutPath `
                        -Deadline $deadline `
                        -ServerEndpoint $serverEndpoint
                    $repeatedDeathWireA =
                        Get-CombatWireState -State $stateA
                    $repeatedDeathWireB =
                        Get-CombatLifeWireState -State $stateB
                    $repeatedDeathProgressA = Get-LatestCombatProgress `
                        -OutputCapture $outputCapture `
                        -StdoutPath $stdoutPath `
                        -Slot 1
                    $repeatedDeathProgressB = Get-LatestCombatProgress `
                        -OutputCapture $outputCapture `
                        -StdoutPath $stdoutPath `
                        -Slot 2
                    if (
                        [Math]::Abs($repeatedDeathWireB.Health + 8.0) -gt
                            0.01 -or
                        [Math]::Abs($repeatedDeathProgressB.Health + 8.0) -gt
                            0.01 -or
                        $repeatedDeathProgressA.CallbackFailures -ne 0 -or
                        $repeatedDeathProgressB.CallbackFailures -ne 0) {
                        throw "lethal repeated death transition gate failed"
                    }
                    $proofStage = "lethal_repeated_respawn_ready_$deathCycle"
                    $repeatedRespawnReadySnapshotsA = $stateA.Snapshots
                    $repeatedRespawnReadySnapshotsB = $stateB.Snapshots
                    $repeatedRespawnReadyServerTimeB =
                        $repeatedDeathProgressB.ServerTimeMs
                    $repeatedRespawnReady = $false
                    for ($index = 0; $index -lt 520; $index++) {
                        Invoke-CombatSnapshotPump `
                            -StateA $stateA `
                            -StateB $stateB `
                            -ServerProcess $serverProcess `
                            -OutputCapture $outputCapture `
                            -StdoutPath $stdoutPath `
                            -Deadline $deadline `
                            -ServerEndpoint $serverEndpoint `
                            -Count 1 `
                            -YawA $repeatedFixture.Yaw
                        $repeatedRespawnProgressB =
                            Get-LatestCombatProgress `
                                -OutputCapture $outputCapture `
                                -StdoutPath $stdoutPath `
                                -Slot 2
                        if ($stateA.Snapshots -gt
                                $repeatedRespawnReadySnapshotsA -and
                            $stateB.Snapshots -gt
                                $repeatedRespawnReadySnapshotsB -and
                            $repeatedRespawnProgressB.ServerTimeMs -gt
                                $repeatedRespawnReadyServerTimeB -and
                            $repeatedRespawnProgressB.Deadflag -eq 3) {
                            $repeatedRespawnReady = $true
                            break
                        }
                    }
                    if (-not $repeatedRespawnReady) {
                        throw "lethal repeated respawn ready frame bound exhausted"
                    }
                    if ($serverProcess.HasExited -or
                        $repeatedRespawnProgressB.CallbackFailures -ne 0) {
                        throw "lethal repeated respawn readiness gate failed"
                    }

                    $proofStage = "lethal_repeated_respawn_$deathCycle"
                    $repeatedRespawnAttackBeforeB =
                        $repeatedRespawnProgressB.AttackReceived
                    $repeatedRespawnExecutedBeforeB =
                        $repeatedRespawnProgressB.AttackExecuted
                    $repeatedRespawnInputsBeforeB =
                        $repeatedRespawnProgressB.RespawnInputsForwarded
                    $repeatedRespawnSnapshotsBeforeA = $stateA.Snapshots
                    $repeatedRespawnSnapshotsBeforeB = $stateB.Snapshots
                    $repeatedRespawnServerTimeBeforeB =
                        $repeatedRespawnProgressB.ServerTimeMs
                    [void](Send-TwoClientCombatMove `
                        -State $stateB `
                        -Yaw 0.0 `
                        -Buttons 1 `
                        -Msec 100)
                    $repeatedRespawnObserved = $false
                    for ($index = 0; $index -lt 520; $index++) {
                        Invoke-CombatSnapshotPump `
                            -StateA $stateA `
                            -StateB $stateB `
                            -ServerProcess $serverProcess `
                            -OutputCapture $outputCapture `
                            -StdoutPath $stdoutPath `
                            -Deadline $deadline `
                            -ServerEndpoint $serverEndpoint `
                            -Count 1
                        $repeatedRespawnProgressB =
                            Get-LatestCombatProgress `
                                -OutputCapture $outputCapture `
                                -StdoutPath $stdoutPath `
                                -Slot 2
                        if ($stateA.Snapshots -gt
                                $repeatedRespawnSnapshotsBeforeA -and
                            $stateB.Snapshots -gt
                                $repeatedRespawnSnapshotsBeforeB -and
                            $repeatedRespawnProgressB.ServerTimeMs -gt
                                $repeatedRespawnServerTimeBeforeB -and
                            $repeatedRespawnProgressB.Deadflag -eq 0 -and
                            $repeatedRespawnProgressB.GlockPresent -and
                            [Math]::Abs(
                                $repeatedRespawnProgressB.Health - 100.0
                            ) -le 0.01 -and
                            $repeatedRespawnProgressB.RespawnInputsForwarded -eq
                                $repeatedRespawnInputsBeforeB + 1 -and
                            $repeatedRespawnProgressB.AttackReceived -eq
                                $repeatedRespawnAttackBeforeB -and
                            $repeatedRespawnProgressB.AttackExecuted -eq
                                $repeatedRespawnExecutedBeforeB) {
                            $repeatedRespawnObserved = $true
                            break
                        }
                    }
                    if (-not $repeatedRespawnObserved) {
                        throw "lethal repeated respawn frame bound exhausted"
                    }
                    if ($serverProcess.HasExited -or
                        $repeatedRespawnProgressB.CallbackFailures -ne 0) {
                        throw "lethal repeated respawn transition gate failed"
                    }
                    Sync-TwoClientSnapshotFrontier `
                        -StateA $stateA `
                        -StateB $stateB `
                        -ServerProcess $serverProcess `
                        -OutputCapture $outputCapture `
                        -StdoutPath $stdoutPath `
                        -Deadline $deadline `
                        -ServerEndpoint $serverEndpoint
                    $repeatedRespawnWireA =
                        Get-CombatWireState -State $stateA
                    $repeatedRespawnWireB =
                        Get-CombatWireState -State $stateB
                    $expectedRepeatedClipAfter = 19 - $deathCycle
                    if ([Math]::Abs(
                            $repeatedRespawnWireA.Health - 100.0
                        ) -gt 0.01 -or
                        [Math]::Abs(
                            $repeatedRespawnWireB.Health - 100.0
                        ) -gt 0.01 -or
                        $repeatedRespawnWireA.Clip -ne
                            $expectedRepeatedClipAfter -or
                        $repeatedRespawnWireB.Clip -ne 17) {
                        throw "lethal repeated respawn inventory gate failed"
                    }
                    $repeatedDeathCycles++
                    $repeatedSameVictimDeaths++
                }
                $combatEvidence = [pscustomobject]@{
                    ShooterHealthBefore = $wireBeforeA.Health
                    ShooterHealthAfter = $wireAfterA.Health
                    ShooterClipBefore = $wireBeforeA.Clip
                    ShooterClipAfter = $wireAfterA.Clip
                    TargetHealthBefore = $wireBeforeB.Health
                    TargetHealthAfter = $wireAfterB.Health
                    TargetClipBefore = $wireBeforeB.Clip
                    TargetClipAfter = $wireAfterB.Clip
                    TargetClientDataUpdated = $true
                    ShooterWeaponDataUpdated = $true
                    ClientAMovementAfterShot = $true
                    ClientBMovementAfterShot = $true
                    WallOccluded = $false
                    LethalDamagePerShot = $lethalDamagePerShot
                    LethalShotCount = 8
                    LethalShooterClipAfter = $postDeathWireA.Clip
                    LethalTargetHealthAfter = $postDeathWireB.Health
                    LethalTargetDeadflag = $postDeathProgressB.Deadflag
                    LethalTargetGlockAbsent =
                        (-not $postDeathWireB.GlockPresent -and
                            -not $postDeathProgressB.GlockPresent)
                    PostDeathFramesA =
                        $stateA.Snapshots - $deathSnapshotsA
                    PostDeathFramesB =
                        $stateB.Snapshots - $deathSnapshotsB
                    PostDeathClientAProgressed = $true
                    PostDeathClientBProgressed = $true
                    FirstRespawnPassed = $true
                    FirstDeathCameraDwellPassed = $true
                    SecondDeathPassed = $true
                    SecondRespawnPassed = $true
                    SecondDeathCameraDwellPassed = $true
                    SecondShooterClipAfter = $secondRespawnWireB.Clip
                    DeathRespawnCycles = $repeatedDeathCycles
                    SameVictimDeaths = $repeatedSameVictimDeaths
                    RepeatedCycleGatePassed =
                        ($repeatedDeathCycles -eq 8 -and
                            $repeatedSameVictimDeaths -eq 7)
                    ClientARespawnInputsForwarded =
                        $secondRespawnProgressA.RespawnInputsForwarded
                    ClientBRespawnInputsForwarded =
                        $repeatedRespawnProgressB.RespawnInputsForwarded
                    ServerStillResponsive = $true
                }
            }
        } else {
            $wireInitialA = Get-CombatWireState -State $stateA
            $wireInitialB = Get-CombatWireState -State $stateB

            $proofStage = "combat_miss_and_exact_duplicate"
            $missFixture = Set-StockCombatFixture `
                -ControlPath $controlPath `
                -ShooterSlot 1 `
                -ShooterGeneration $generations[1] `
                -TargetSlot 2 `
                -TargetGeneration $generations[2] `
                -Mode miss `
                -OutputCapture $outputCapture `
                -StdoutPath $stdoutPath `
                -Deadline $deadline
            if ($missFixture.Mode -cne 'miss' -or
                $missFixture.WorldFraction -lt 0.999) {
                throw "miss combat fixture semantic gate failed"
            }
            Sync-TwoClientSnapshotFrontier `
                -StateA $stateA `
                -StateB $stateB `
                -ServerProcess $serverProcess `
                -OutputCapture $outputCapture `
                -StdoutPath $stdoutPath `
                -Deadline $deadline `
                -ServerEndpoint $serverEndpoint
            $missBeforeA = Get-CombatWireState -State $stateA
            $missBeforeB = Get-CombatWireState -State $stateB
            $missProgressBeforeA = Get-LatestCombatProgress `
                -OutputCapture $outputCapture `
                -StdoutPath $stdoutPath `
                -Slot 1
            [byte[]]$firstAttackPacket = Send-TwoClientCombatMove `
                -State $stateA -Yaw $missFixture.Yaw -Buttons 1
            Send-ExactUdpDatagram `
                -Client $stateA.Client `
                -Packet $firstAttackPacket `
                -Description "duplicate combat attack packet"
            Send-TwoClientExactAttackBackupMove `
                -State $stateA `
                -Yaw $missFixture.Yaw
            Invoke-CombatSnapshotPump `
                -StateA $stateA `
                -StateB $stateB `
                -ServerProcess $serverProcess `
                -OutputCapture $outputCapture `
                -StdoutPath $stdoutPath `
                -Deadline $deadline `
                -ServerEndpoint $serverEndpoint `
                -Count 12
            Sync-TwoClientSnapshotFrontier `
                -StateA $stateA `
                -StateB $stateB `
                -ServerProcess $serverProcess `
                -OutputCapture $outputCapture `
                -StdoutPath $stdoutPath `
                -Deadline $deadline `
                -ServerEndpoint $serverEndpoint
            $missAfterA = Get-CombatWireState -State $stateA
            $missAfterB = Get-CombatWireState -State $stateB
            $missProgressAfterA = Get-LatestCombatProgress `
                -OutputCapture $outputCapture `
                -StdoutPath $stdoutPath `
                -Slot 1
            if ($missBeforeA.Clip - $missAfterA.Clip -ne 1) {
                $authoritativeRounds =
                    $missProgressBeforeA.GlockClip -
                        $missProgressAfterA.GlockClip
                if ($authoritativeRounds -eq 0) {
                    throw "miss authoritative round was not consumed"
                }
                if ($authoritativeRounds -eq 1) {
                    throw "miss wire round transition gate failed"
                }
                throw "miss duplicate round consumption gate failed"
            }
            if ($missAfterA.Health -ne $missBeforeA.Health) {
                throw "miss shooter health isolation gate failed"
            }
            if ($missAfterB.Health -ne $missBeforeB.Health) {
                throw "miss target health isolation gate failed"
            }
            if ($missAfterB.Clip -ne $missBeforeB.Clip) {
                throw "miss target weapon isolation gate failed"
            }
            if ($missProgressAfterA.ShotPlayerHits -ne
                    $missProgressBeforeA.ShotPlayerHits) {
                throw "miss unexpectedly selected a player trace"
            }
            if ($missProgressAfterA.DuplicateAttackSuppressed -ne
                    $missProgressBeforeA.DuplicateAttackSuppressed + 1 -or
                $missProgressAfterA.RespawnInputsForwarded -ne
                    $missProgressBeforeA.RespawnInputsForwarded -or
                $missProgressAfterA.AttackReceived -ne
                    $missProgressBeforeA.AttackReceived + 1 -or
                $missProgressAfterA.AttackExecuted -ne
                    $missProgressBeforeA.AttackExecuted + 1) {
                throw "miss exact attack backup accounting gate failed"
            }
            $selfHitPrevented =
                $missProgressAfterA.ShooterIgnored -gt
                    $missProgressBeforeA.ShooterIgnored
            if (-not $selfHitPrevented) {
                throw "miss shooter-ignore trace gate failed"
            }

            Invoke-CombatSnapshotPump `
                -StateA $stateA `
                -StateB $stateB `
                -ServerProcess $serverProcess `
                -OutputCapture $outputCapture `
                -StdoutPath $stdoutPath `
                -Deadline $deadline `
                -ServerEndpoint $serverEndpoint `
                -Count 40
            $proofStage = "combat_cooldown"
            $cooldownBeforeA = Get-CombatWireState -State $stateA
            $cooldownBeforeB = Get-CombatWireState -State $stateB
            $cooldownProgressBeforeA = Get-LatestCombatProgress `
                -OutputCapture $outputCapture `
                -StdoutPath $stdoutPath `
                -Slot 1
            [void](Send-TwoClientCombatMove `
                -State $stateA -Yaw $missFixture.Yaw -Buttons 1)
            [void](Send-TwoClientCombatMove `
                -State $stateA -Yaw $missFixture.Yaw -Buttons 0)
            [void](Send-TwoClientCombatMove `
                -State $stateA -Yaw $missFixture.Yaw -Buttons 1)
            Invoke-CombatSnapshotPump `
                -StateA $stateA `
                -StateB $stateB `
                -ServerProcess $serverProcess `
                -OutputCapture $outputCapture `
                -Deadline $deadline `
                -ServerEndpoint $serverEndpoint `
                -Count 12
            Sync-TwoClientSnapshotFrontier `
                -StateA $stateA `
                -StateB $stateB `
                -ServerProcess $serverProcess `
                -OutputCapture $outputCapture `
                -StdoutPath $stdoutPath `
                -Deadline $deadline `
                -ServerEndpoint $serverEndpoint
            $cooldownAfterA = Get-CombatWireState -State $stateA
            $cooldownAfterB = Get-CombatWireState -State $stateB
            $cooldownProgressAfterA = Get-LatestCombatProgress `
                -OutputCapture $outputCapture `
                -StdoutPath $stdoutPath `
                -Slot 1
            if ($cooldownBeforeA.Clip - $cooldownAfterA.Clip -ne 1) {
                $authoritativeRounds =
                    $cooldownProgressBeforeA.GlockClip -
                        $cooldownProgressAfterA.GlockClip
                if ($authoritativeRounds -eq 0) {
                    throw "cooldown authoritative round was not consumed"
                }
                if ($authoritativeRounds -eq 1) {
                    throw "cooldown wire round transition gate failed"
                }
                throw "cooldown duplicate round consumption gate failed"
            }
            if ($cooldownAfterA.Health -ne $cooldownBeforeA.Health) {
                throw "cooldown shooter health isolation gate failed"
            }
            if ($cooldownAfterB.Health -ne $cooldownBeforeB.Health) {
                throw "cooldown target health isolation gate failed"
            }
            if ($cooldownAfterB.Clip -ne $cooldownBeforeB.Clip) {
                throw "cooldown target weapon isolation gate failed"
            }

            $proofStage = "combat_backup_replay"
            $backupBeforeA = Get-CombatWireState -State $stateA
            $backupBeforeB = Get-CombatWireState -State $stateB
            $backupProgressBeforeA = Get-LatestCombatProgress `
                -OutputCapture $outputCapture `
                -StdoutPath $stdoutPath `
                -Slot 1
            Send-TwoClientCombatRecoveryMove `
                -State $stateA `
                -LastButtons 1 `
                -FreshButtons 0
            Invoke-CombatSnapshotPump `
                -StateA $stateA `
                -StateB $stateB `
                -ServerProcess $serverProcess `
                -OutputCapture $outputCapture `
                -Deadline $deadline `
                -ServerEndpoint $serverEndpoint `
                -Count 4
            Assert-CombatWireUnchanged `
                -BeforeA $backupBeforeA `
                -AfterA (Get-CombatWireState -State $stateA) `
                -BeforeB $backupBeforeB `
                -AfterB (Get-CombatWireState -State $stateB) `
                -Description "backup replay"
            $backupProgressAfterA = Get-LatestCombatProgress `
                -OutputCapture $outputCapture `
                -StdoutPath $stdoutPath `
                -Slot 1
            if ($backupProgressAfterA.DuplicateAttackSuppressed -ne
                    $backupProgressBeforeA.DuplicateAttackSuppressed -or
                $backupProgressAfterA.AttackReceived -ne
                    $backupProgressBeforeA.AttackReceived -or
                $backupProgressAfterA.AttackExecuted -ne
                    $backupProgressBeforeA.AttackExecuted -or
                $backupProgressAfterA.RespawnInputsForwarded -ne
                    $backupProgressBeforeA.RespawnInputsForwarded) {
                throw "stale backup semantic accounting gate failed"
            }

            $proofStage = "combat_invalid_checksum"
            $checksumBeforeA = Get-CombatWireState -State $stateA
            $checksumBeforeB = Get-CombatWireState -State $stateB
            [void](Send-TwoClientCombatMove `
                -State $stateA `
                -Yaw $missFixture.Yaw `
                -Buttons 1 `
                -CorruptChecksum)
            Invoke-CombatSnapshotPump `
                -StateA $stateA `
                -StateB $stateB `
                -ServerProcess $serverProcess `
                -OutputCapture $outputCapture `
                -Deadline $deadline `
                -ServerEndpoint $serverEndpoint `
                -Count 4
            Assert-CombatWireUnchanged `
                -BeforeA $checksumBeforeA `
                -AfterA (Get-CombatWireState -State $stateA) `
                -BeforeB $checksumBeforeB `
                -AfterB (Get-CombatWireState -State $stateB) `
                -Description "invalid checksum"

            $proofStage = "combat_malformed_move"
            $malformedBeforeA = Get-CombatWireState -State $stateA
            $malformedBeforeB = Get-CombatWireState -State $stateB
            [void](Send-TwoClientCombatMove `
                -State $stateA `
                -Yaw $missFixture.Yaw `
                -Buttons 1 `
                -Malformed)
            Invoke-CombatSnapshotPump `
                -StateA $stateA `
                -StateB $stateB `
                -ServerProcess $serverProcess `
                -OutputCapture $outputCapture `
                -Deadline $deadline `
                -ServerEndpoint $serverEndpoint `
                -Count 4
            Assert-CombatWireUnchanged `
                -BeforeA $malformedBeforeA `
                -AfterA (Get-CombatWireState -State $stateA) `
                -BeforeB $malformedBeforeB `
                -AfterB (Get-CombatWireState -State $stateB) `
                -Description "malformed movement"

            $proofStage = "combat_attack2_mask"
            $attack2BeforeA = Get-CombatWireState -State $stateA
            $attack2BeforeB = Get-CombatWireState -State $stateB
            [void](Send-TwoClientCombatMove `
                -State $stateA -Yaw $missFixture.Yaw -Buttons 2048)
            Invoke-CombatSnapshotPump `
                -StateA $stateA `
                -StateB $stateB `
                -ServerProcess $serverProcess `
                -OutputCapture $outputCapture `
                -Deadline $deadline `
                -ServerEndpoint $serverEndpoint `
                -Count 4
            Assert-CombatWireUnchanged `
                -BeforeA $attack2BeforeA `
                -AfterA (Get-CombatWireState -State $stateA) `
                -BeforeB $attack2BeforeB `
                -AfterB (Get-CombatWireState -State $stateB) `
                -Description "attack2 mask"

            $proofStage = "combat_wall_occlusion"
            Invoke-CombatSnapshotPump `
                -StateA $stateA `
                -StateB $stateB `
                -ServerProcess $serverProcess `
                -OutputCapture $outputCapture `
                -Deadline $deadline `
                -ServerEndpoint $serverEndpoint `
                -Count 40
            $blockedFixture = Set-StockCombatFixture `
                -ControlPath $controlPath `
                -ShooterSlot 1 `
                -ShooterGeneration $generations[1] `
                -TargetSlot 2 `
                -TargetGeneration $generations[2] `
                -Mode blocked `
                -OutputCapture $outputCapture `
                -StdoutPath $stdoutPath `
                -Deadline $deadline
            if ($blockedFixture.Mode -cne 'blocked' -or
                $blockedFixture.WorldFraction -le 0.01 -or
                $blockedFixture.WorldFraction -ge 0.999) {
                throw "blocked combat fixture semantic gate failed"
            }
            Sync-TwoClientSnapshotFrontier `
                -StateA $stateA `
                -StateB $stateB `
                -ServerProcess $serverProcess `
                -OutputCapture $outputCapture `
                -StdoutPath $stdoutPath `
                -Deadline $deadline `
                -ServerEndpoint $serverEndpoint
            $blockedBeforeA = Get-CombatWireState -State $stateA
            $blockedBeforeB = Get-CombatWireState -State $stateB
            $blockedProgressBeforeA = Get-LatestCombatProgress `
                -OutputCapture $outputCapture `
                -StdoutPath $stdoutPath `
                -Slot 1
            $blockedProgressBeforeB = Get-LatestCombatProgress `
                -OutputCapture $outputCapture `
                -StdoutPath $stdoutPath `
                -Slot 2
            [void](Send-TwoClientCombatMove `
                -State $stateA -Yaw $blockedFixture.Yaw -Buttons 1)
            Invoke-CombatSnapshotPump `
                -StateA $stateA `
                -StateB $stateB `
                -ServerProcess $serverProcess `
                -OutputCapture $outputCapture `
                -Deadline $deadline `
                -ServerEndpoint $serverEndpoint `
                -Count 12
            Sync-TwoClientSnapshotFrontier `
                -StateA $stateA `
                -StateB $stateB `
                -ServerProcess $serverProcess `
                -OutputCapture $outputCapture `
                -StdoutPath $stdoutPath `
                -Deadline $deadline `
                -ServerEndpoint $serverEndpoint
            $blockedAfterA = Get-CombatWireState -State $stateA
            $blockedAfterB = Get-CombatWireState -State $stateB
            $blockedProgressAfterA = Get-LatestCombatProgress `
                -OutputCapture $outputCapture `
                -StdoutPath $stdoutPath `
                -Slot 1
            $blockedProgressAfterB = Get-LatestCombatProgress `
                -OutputCapture $outputCapture `
                -StdoutPath $stdoutPath `
                -Slot 2
            if ($blockedBeforeA.Clip - $blockedAfterA.Clip -ne 1) {
                throw "wall occlusion exact round consumption gate failed"
            }
            if ($blockedAfterA.Health -ne $blockedBeforeA.Health) {
                throw "wall occlusion shooter health isolation gate failed"
            }
            if ($blockedAfterB.Health -ne $blockedBeforeB.Health) {
                Write-Host ((
                    'two_client_phase_diagnostic: phase=wall_occlusion,' +
                    'prior_phase_stable=true,round_consumed=true,' +
                    'target_health_decreased={0},' +
                    'shooter_player_hit_advanced={1},' +
                    'shooter_world_hit_advanced={2},' +
                    'target_runtime_health_changed={3},' +
                    'target_clientdata_health_changed={4},' +
                    'target_snapshot_advanced={5}') -f
                    ($blockedAfterB.Health -lt $blockedBeforeB.Health).
                        ToString().ToLowerInvariant(),
                    ($blockedProgressAfterA.ShotPlayerHits -gt
                        $blockedProgressBeforeA.ShotPlayerHits).
                        ToString().ToLowerInvariant(),
                    ($blockedProgressAfterA.ShotWorldHits -gt
                        $blockedProgressBeforeA.ShotWorldHits).
                        ToString().ToLowerInvariant(),
                    ($blockedProgressAfterB.Health -ne
                        $blockedProgressBeforeB.Health).
                        ToString().ToLowerInvariant(),
                    ($blockedProgressAfterB.ClientDataHealth -ne
                        $blockedProgressBeforeB.ClientDataHealth).
                        ToString().ToLowerInvariant(),
                    ($blockedAfterB.FrameId -ne $blockedBeforeB.FrameId).
                        ToString().ToLowerInvariant())
                throw "wall occlusion target health isolation gate failed"
            }
            if ($blockedAfterB.Clip -ne $blockedBeforeB.Clip) {
                throw "wall occlusion target weapon isolation gate failed"
            }
            if ($blockedProgressAfterA.ShotPlayerHits -ne
                    $blockedProgressBeforeA.ShotPlayerHits) {
                throw "wall occlusion actual shot selected a player"
            }
            if ($blockedProgressAfterA.ShotWorldHits -le
                    $blockedProgressBeforeA.ShotWorldHits) {
                throw "wall occlusion actual shot did not select world"
            }

            $proofStage = "combat_cross_client_isolation"
            Invoke-CombatSnapshotPump `
                -StateA $stateA `
                -StateB $stateB `
                -ServerProcess $serverProcess `
                -OutputCapture $outputCapture `
                -Deadline $deadline `
                -ServerEndpoint $serverEndpoint `
                -Count 8
            $bMissFixture = Set-StockCombatFixture `
                -ControlPath $controlPath `
                -ShooterSlot 2 `
                -ShooterGeneration $generations[2] `
                -TargetSlot 1 `
                -TargetGeneration $generations[1] `
                -Mode miss `
                -OutputCapture $outputCapture `
                -StdoutPath $stdoutPath `
                -Deadline $deadline
            if ($bMissFixture.Mode -cne 'miss' -or
                $bMissFixture.WorldFraction -lt 0.999) {
                throw "cross-client fixture semantic gate failed"
            }
            Sync-TwoClientSnapshotFrontier `
                -StateA $stateA `
                -StateB $stateB `
                -ServerProcess $serverProcess `
                -OutputCapture $outputCapture `
                -StdoutPath $stdoutPath `
                -Deadline $deadline `
                -ServerEndpoint $serverEndpoint
            $isolationBeforeA = Get-CombatWireState -State $stateA
            $isolationBeforeB = Get-CombatWireState -State $stateB
            $isolationProgressBeforeB = Get-LatestCombatProgress `
                -OutputCapture $outputCapture `
                -StdoutPath $stdoutPath `
                -Slot 2
            [byte[]]$staleBPacket = Send-TwoClientCombatMove `
                -State $stateB -Yaw $bMissFixture.Yaw -Buttons 1
            Invoke-CombatSnapshotPump `
                -StateA $stateA `
                -StateB $stateB `
                -ServerProcess $serverProcess `
                -OutputCapture $outputCapture `
                -Deadline $deadline `
                -ServerEndpoint $serverEndpoint `
                -Count 12
            Sync-TwoClientSnapshotFrontier `
                -StateA $stateA `
                -StateB $stateB `
                -ServerProcess $serverProcess `
                -OutputCapture $outputCapture `
                -StdoutPath $stdoutPath `
                -Deadline $deadline `
                -ServerEndpoint $serverEndpoint
            $isolationAfterA = Get-CombatWireState -State $stateA
            $isolationAfterB = Get-CombatWireState -State $stateB
            $isolationProgressAfterB = Get-LatestCombatProgress `
                -OutputCapture $outputCapture `
                -StdoutPath $stdoutPath `
                -Slot 2
            if ($isolationAfterA.Health -ne $isolationBeforeA.Health) {
                throw "cross-client target health isolation gate failed"
            }
            if ($isolationAfterA.Clip -ne $isolationBeforeA.Clip) {
                throw "cross-client target weapon isolation gate failed"
            }
            if ($isolationAfterB.Health -ne $isolationBeforeB.Health) {
                throw "cross-client shooter health isolation gate failed"
            }
            if ($isolationBeforeB.Clip - $isolationAfterB.Clip -ne 1) {
                throw "cross-client shooter round consumption gate failed"
            }
            if ($isolationProgressAfterB.ShotPlayerHits -ne
                    $isolationProgressBeforeB.ShotPlayerHits) {
                throw "cross-client miss selected a player trace"
            }

            $proofStage = "combat_disconnected_target"
            $disconnectBeforeA = Get-CombatWireState -State $stateA
            Send-ClientDisconnect -State $stateB
            Wait-ForStdoutToken `
                -Process $serverProcess `
                -OutputCapture $outputCapture `
                -StdoutPath $stdoutPath `
                -Deadline $deadline `
                -Token "goldsrc_delta_session_reset:" `
                -Description "combat target disconnect"
            Send-ExactUdpDatagram `
                -Client $stateB.Client `
                -Packet $staleBPacket `
                -Description "stale disconnected combat packet"
            for ($index = 0; $index -lt 20; $index++) {
                [void](Receive-TwoClientSnapshot `
                    -State $stateA `
                    -ServerProcess $serverProcess `
                    -OutputCapture $outputCapture `
                    -Deadline $deadline `
                    -ServerEndpoint $serverEndpoint)
            }
            $staleAfterA = Get-CombatWireState -State $stateA
            if ($staleAfterA.Health -ne $disconnectBeforeA.Health -or
                $staleAfterA.Clip -ne $disconnectBeforeA.Clip) {
                throw "stale disconnected attacker isolation gate failed"
            }
            [void](Send-TwoClientCombatMove `
                -State $stateA -Yaw $blockedFixture.Yaw -Buttons 1)
            for ($index = 0; $index -lt 10; $index++) {
                [void](Receive-TwoClientSnapshot `
                    -State $stateA `
                    -ServerProcess $serverProcess `
                    -OutputCapture $outputCapture `
                    -Deadline $deadline `
                    -ServerEndpoint $serverEndpoint)
            }
            Sync-OneClientSnapshotFrontier `
                -State $stateA `
                -ServerProcess $serverProcess `
                -OutputCapture $outputCapture `
                -StdoutPath $stdoutPath `
                -Deadline $deadline `
                -ServerEndpoint $serverEndpoint
            $responsiveAfterA = Get-CombatWireState -State $stateA
            if ($staleAfterA.Clip - $responsiveAfterA.Clip -ne 1) {
                throw "disconnected-target round consumption gate failed"
            }
            if ($responsiveAfterA.Health -ne $staleAfterA.Health) {
                throw "disconnected-target shooter health isolation gate failed"
            }

            $proofStage = "combat_slot_reuse"
            $clientAReconnect = New-TwoClientProbe -ServerEndpoint $serverEndpoint
            $connectionBReconnect = Invoke-TwoClientSignon `
                -Client $clientAReconnect `
                -OwnEntity 2 `
                -ProbeName "combat_b_reconnect" `
                -ServerProcess $serverProcess `
                -OutputCapture $outputCapture `
                -StdoutPath $stdoutPath `
                -Deadline $deadline `
                -ServerEndpoint $serverEndpoint `
                -ExpectedClientDllMd5 $expectedClientDllMd5 `
                -ExpectedEntries $expectedEntries `
                -ExactCombatBootstrap:$CombatProof
            $stateBReconnect = New-TwoClientStreamState `
                -Client $clientAReconnect `
                -OwnEntity 2 `
                -Connection $connectionBReconnect
            for ($index = 0; $index -lt 30; $index++) {
                [void](Receive-TwoClientSnapshot `
                    -State $stateA `
                    -ServerProcess $serverProcess `
                    -OutputCapture $outputCapture `
                    -Deadline $deadline `
                    -ServerEndpoint $serverEndpoint)
                [void](Receive-TwoClientSnapshot `
                    -State $stateBReconnect `
                    -ServerProcess $serverProcess `
                    -OutputCapture $outputCapture `
                    -Deadline $deadline `
                    -ServerEndpoint $serverEndpoint)
            }
            $reconnectAfterA = Get-CombatWireState -State $stateA
            $reconnectWireB = Get-CombatWireState -State $stateBReconnect
            if ($reconnectAfterA.Health -ne $responsiveAfterA.Health -or
                $reconnectAfterA.Clip -ne $responsiveAfterA.Clip -or
                $reconnectWireB.Health -ne $wireInitialB.Health -or
                $reconnectWireB.Clip -ne $wireInitialB.Clip) {
                throw "combat slot-reuse wire-state gate failed"
            }
            $combatEvidence = [pscustomobject]@{
                MissNoDamage = $true
                ExactDuplicateSuppressed = $true
                BackupReplaySuppressed = $true
                CooldownEnforced = $true
                InvalidChecksumNoFire = $true
                MalformedMoveNoFire = $true
                Attack2Masked = $true
                WallOcclusion = $true
                SelfHitPrevented = $selfHitPrevented
                CrossClientIsolation = $true
                DisconnectedTargetSafe = $true
                StaleAttackerPacketRejected = $true
                SlotReuseClean = $true
                ServerStillResponsive = $true
            }
        }
        }

        $proofStage = "combat_clean_shutdown"
        [IO.File]::WriteAllText($shutdownPath, "combat proof complete")
        Wait-ForCleanServerExit `
            -Process $serverProcess `
            -OutputCapture $outputCapture `
            -Deadline $deadline
        Complete-ProcessOutputCapture -State $outputCapture
        $capturedStdout = Get-SharedFileText -Path $stdoutPath
        $capturedStderr = Get-SharedFileText -Path $stderrPath
        if ($serverProcess.ExitCode -ne 0) {
            $capturedDiagnosticText =
                $capturedStdout + [Environment]::NewLine + $capturedStderr
            $failureMatches = [regex]::Matches(
                $capturedDiagnosticText,
                ('goldsrc_combat_failed: reason=gameplay-callback,' +
                 'stage=(?<stage>fail_stop_latched|cmd_start|' +
                 'player_prethink|entity_think|context_build|pm_move|' +
                 'output_validation|player_postthink|cmd_end|unknown)'))
            if ($failureMatches.Count -eq 0) {
                $failureMatches = [regex]::Matches(
                    $capturedDiagnosticText,
                    ('last_reject_reason=' +
                     'combat-gameplay-callback-failed-' +
                     '(?<stage>fail_stop_latched|cmd_start|' +
                     'player_prethink|entity_think|context_build|pm_move|' +
                     'output_validation|player_postthink|cmd_end|unknown)'))
            }
            if ($failureMatches.Count -gt 0) {
                $reportedStage =
                    $failureMatches[$failureMatches.Count - 1].Groups[
                        'stage'].Value
                $proofStage = $(if ($FallDamageProof) {
                    'fall_gameplay_' + $reportedStage
                } else {
                    'combat_gameplay_' + $reportedStage
                })
                throw "server gameplay stage failed"
            }
            $shutdownFailureMatch = [regex]::Match(
                $capturedDiagnosticText,
                ('goldsrc_shutdown_failed: stage=' +
                 '(?<stage>runtime_success_contract|server_deactivate)'))
            if ($shutdownFailureMatch.Success) {
                $proofStage = $(if ($FallDamageProof) {
                    'fall_shutdown_' +
                        $shutdownFailureMatch.Groups['stage'].Value
                } else {
                    'combat_shutdown_' +
                        $shutdownFailureMatch.Groups['stage'].Value
                })
                throw "server shutdown stage failed"
            }
            if ($capturedDiagnosticText.Contains(
                    'last_reject_reason=' +
                    'player-lifecycle-shutdown-disconnect-failed')) {
                $proofStage = $(if ($FallDamageProof) {
                    'fall_shutdown_player_disconnect'
                } else {
                    'combat_shutdown_player_disconnect'
                })
                throw "server shutdown lifecycle failed"
            }
            if ($capturedDiagnosticText.Contains(
                    'completion_reason=manual_shutdown_request')) {
                $noRejectRecorded = $capturedDiagnosticText -match
                    'last_reject_reason=(none|not-applicable),'
                $rejectMatch = [regex]::Match(
                    $capturedDiagnosticText,
                    'last_reject_reason=(?<reason>[a-z0-9_-]{1,96}),')
                $rejectCategory = if (-not $rejectMatch.Success) {
                    'unknown'
                } elseif ($rejectMatch.Groups['reason'].Value -match
                        'snapshot|clientdata|weapondata') {
                    'snapshot'
                } elseif ($rejectMatch.Groups['reason'].Value -match
                        'delta') {
                    'delta'
                } elseif ($rejectMatch.Groups['reason'].Value -match
                        'netchan|ack|sequence') {
                    'netchan'
                } elseif ($rejectMatch.Groups['reason'].Value -match
                        'player-lifecycle|client-disconnect') {
                    'player_lifecycle'
                } elseif ($rejectMatch.Groups['reason'].Value -match
                        'pmove|movement') {
                    'pmove'
                } elseif ($rejectMatch.Groups['reason'].Value -match
                        'combat') {
                    'combat'
                } else {
                    'semantic_' + $rejectMatch.Groups['reason'].Value.Replace(
                        '-', '_')
                }
                $proofStage = $(if ($FallDamageProof -and
                        $noRejectRecorded) {
                    'fall_shutdown_success_contract'
                } elseif ($FallDamageProof) {
                    'fall_shutdown_' + $rejectCategory
                } else {
                    'combat_shutdown_' + $rejectCategory
                })
                throw "server shutdown status failed"
            }
            throw (
                "combat proof server did not shut down cleanly (exit code {0})" -f
                $serverProcess.ExitCode)
        }
        if ($FeatureOffProof) {
            $proofStage = "feature_off_summary_validation"
            if ($capturedStdout -match
                'goldsrc_combat_(summary|observation|shot|failed):') {
                throw "feature-off emitted combat activity"
            }
            $pmoveSummary = @(Get-SummaryFields `
                -Stdout $capturedStdout `
                -Prefix "goldsrc_pmove_summary:")[0]
            $clients = @(Get-SummaryFields `
                -Stdout $capturedStdout `
                -Prefix "goldsrc_client_replication_summary:" `
                -ExpectedCount 2)
            $clientSummaryA = @(
                $clients | Where-Object { $_["slot"] -ceq "1" })[0]
            $clientSummaryB = @(
                $clients | Where-Object { $_["slot"] -ceq "2" })[0]
            if ($null -eq $combatEvidence -or
                -not $combatEvidence.AttackMoveReceived -or
                -not $combatEvidence.AttackMoveValidated -or
                -not $combatEvidence.AttackMoveExecuted -or
                -not $combatEvidence.AttackMasked -or
                -not $combatEvidence.GameplayCallbacksAbsent -or
                -not $combatEvidence.HealthUnchanged -or
                -not $combatEvidence.WeaponDataAbsent -or
                -not $combatEvidence.MovementStable -or
                $pmoveSummary["command_contract"] -cne "true" -or
                $pmoveSummary["movement_ready"] -cne "true" -or
                $pmoveSummary["movement_executed"] -cne "true" -or
                [int]$clientSummaryA["pmove_calls"] -lt 1 -or
                [int]$clientSummaryB["pmove_calls"] -lt 1) {
                throw "feature-off summary gate failed"
            }
            $result = [pscustomobject]@{
                ClientA = $clientSummaryA
                ClientB = $clientSummaryB
                Pmove = $pmoveSummary
                Evidence = $combatEvidence
            }
        } elseif ($FallDamageProof) {
            $proofStage = "fall_summary_validation"
            $combatSummary = @(Get-SummaryFields `
                -Stdout $capturedStdout `
                -Prefix "goldsrc_combat_summary:")[0]
            $clients = @(Get-SummaryFields `
                -Stdout $capturedStdout `
                -Prefix "goldsrc_client_replication_summary:" `
                -ExpectedCount 2)
            $clientSummaryA = @(
                $clients | Where-Object { $_["slot"] -ceq "1" })[0]
            $clientSummaryB = @(
                $clients | Where-Object { $_["slot"] -ceq "2" })[0]
            if ($null -eq $combatEvidence -or
                -not $combatEvidence.FixtureApplied -or
                -not $combatEvidence.LandingModeGrounded -or
                -not $combatEvidence.LandingModeHealthUnchanged -or
                -not $combatEvidence.LandingModePostThinkAdvanced -or
                -not $combatEvidence.TargetAlive -or
                -not $combatEvidence.PlayerPostThinkAdvanced -or
                -not $combatEvidence.DamageModeGrounded -or
                -not $combatEvidence.ClientDataHealthUpdated -or
                -not $combatEvidence.PeerStateUnchanged -or
                -not $combatEvidence.PostLandingMovement -or
                -not $combatEvidence.ServerStillResponsive -or
                [Math]::Abs(
                    ($combatEvidence.HealthBefore -
                        $combatEvidence.HealthAfter) - 10.0) -gt 0.01 -or
                [int]$combatSummary["attack_received"] -ne 0 -or
                [int]$combatSummary["attack_executed"] -ne 0 -or
                [int]$combatSummary["a_prethink"] -lt 1 -or
                [int]$combatSummary["a_postthink"] -lt 1 -or
                [int]$combatSummary["a_callback_failures"] -ne 0 -or
                [int]$combatSummary["b_callback_failures"] -ne 0 -or
                [Math]::Abs(
                    [double]$combatSummary["a_health_after"] -
                        $combatEvidence.HealthAfter) -gt 0.01 -or
                [Math]::Abs(
                    [double]$combatSummary["a_clientdata_health_after"] -
                        $combatEvidence.HealthAfter) -gt 0.01 -or
                [int]$clientSummaryA["pmove_calls"] -lt 1 -or
                [int]$clientSummaryB["pmove_calls"] -lt 1) {
                throw "fall damage summary gate failed"
            }
            $result = [pscustomobject]@{
                ClientA = $clientSummaryA
                ClientB = $clientSummaryB
                Combat = $combatSummary
                Evidence = $combatEvidence
            }
        } else {
        $proofStage = "combat_summary_validation"
        $combatSummary = @(Get-SummaryFields `
            -Stdout $capturedStdout `
            -Prefix "goldsrc_combat_summary:")[0]
        $clients = @(Get-SummaryFields `
            -Stdout $capturedStdout `
            -Prefix "goldsrc_client_replication_summary:" `
            -ExpectedCount 2)
        $clientSummaryA = @($clients | Where-Object { $_["slot"] -ceq "1" })[0]
        $clientSummaryB = @($clients | Where-Object { $_["slot"] -ceq "2" })[0]
        if ($null -eq $combatEvidence -or
            $combatSummary["gameplay_active"] -cne "true" -or
            $combatSummary["callback_order"] -cne
                "cmdstart_prethink_think_pmove_commit_postthink_cmdend" -or
            $combatSummary["trace_policy"] -cne
                "nearest_player_before_static_world" -or
            $combatSummary["callback_order_verified"] -cne "true" -or
            $combatSummary["time_contract_verified"] -cne "true" -or
            $combatSummary["weapon_postframe_verified"] -cne "true" -or
            $combatSummary["weapon_replication_verified"] -cne "true" -or
            $combatSummary["player_aware_trace"] -cne "true" -or
            $combatSummary["per_client_weapondata"] -cne "true" -or
            $combatSummary["frame_history_weapondata"] -cne "true" -or
            [int]$combatSummary["start_frame_calls"] -lt 1 -or
            [int]$combatSummary["a_prethink"] -lt 1 -or
            [int]$combatSummary["a_postthink"] -lt 1 -or
            [int]$combatSummary["b_prethink"] -lt 1 -or
            [int]$combatSummary["b_postthink"] -lt 1 -or
            [int]$combatSummary["a_active_weapon"] -ne 2 -or
            $combatSummary["a_glock_present"] -cne "true" -or
            [int]$combatSummary["a_weapondata_frames"] -lt 1 -or
            [int]$combatSummary["b_weapondata_frames"] -lt 1) {
            throw "combat callback or inventory summary gate failed"
        }
        if ($LethalDeathProof) {
            if ($combatSummary["b_glock_present"] -cne "true" -or
                [int]$combatSummary["b_deadflag"] -ne 0) {
                throw "lethal death inventory summary gate failed"
            }
        } elseif ([int]$combatSummary["b_active_weapon"] -ne 2 -or
            $combatSummary["b_glock_present"] -cne "true") {
            throw "combat target inventory summary gate failed"
        }
        if (-not $NegativeProof) {
            if ($null -eq $aimPhaseEvidence -or
                -not $aimPhaseEvidence.FixtureApplied -or
                -not $aimPhaseEvidence.TargetInsideAutoaimCone -or
                -not $aimPhaseEvidence.AimOffsetNonzero -or
                -not $aimPhaseEvidence.ButtonsZero -or
                -not $aimPhaseEvidence.ClientACallbacksAdvanced -or
                -not $aimPhaseEvidence.ClientBCallbacksAdvanced -or
                -not $aimPhaseEvidence.AttackStateUnchanged -or
                -not $aimPhaseEvidence.WireStateUnchanged -or
                -not $aimPhaseEvidence.ServerStillResponsive) {
                throw "positive aim regression phase gate failed"
            }
            $positiveSummaryGates = if ($LethalDeathProof) {
                [ordered]@{
                    vec_to_angles_callback =
                        [int]$combatSummary["vec_to_angles_calls"] -ge 1
                    crosshair_angle_callback =
                        [int]$combatSummary["crosshair_angle_calls"] -ge 1
                    attack_received =
                        [int]$combatSummary["attack_received"] -eq 9
                    attack_executed =
                        [int]$combatSummary["attack_executed"] -eq 9
                    client_a_respawn_health =
                        [Math]::Abs(
                            [double]$combatSummary["a_health_after"] -
                            100.0) -le 0.01
                    client_a_respawn_clientdata =
                        [Math]::Abs(
                            [double]$combatSummary[
                                "a_clientdata_health_after"] -
                            100.0) -le 0.01
                    client_b_respawn_health =
                        [Math]::Abs(
                            [double]$combatSummary["b_health_after"] -
                            100.0) -le 0.01
                    client_b_respawn_clientdata =
                        [Math]::Abs(
                            [double]$combatSummary[
                                "b_clientdata_health_after"] -
                            100.0) -le 0.01
                    client_a_respawned =
                        [int]$combatSummary["a_deadflag"] -eq 0 -and
                            $combatSummary["a_glock_present"] -ceq "true"
                    client_b_respawned =
                        [int]$combatSummary["b_deadflag"] -eq 0 -and
                            $combatSummary["b_glock_present"] -ceq "true"
                    client_a_exact_traces =
                        [int]$combatSummary["a_shot_traces"] -eq 16 -and
                            [int]$combatSummary["a_shot_player_hits"] -eq 16
                    client_b_exact_traces =
                        [int]$combatSummary["b_shot_traces"] -eq 2 -and
                            [int]$combatSummary["b_shot_player_hits"] -eq 2
                    client_a_world_hit_count =
                        [int]$combatSummary["a_shot_world_hits"] -eq 0
                    client_b_world_hit_count =
                        [int]$combatSummary["b_shot_world_hits"] -eq 0
                    callback_failures_a =
                        [int]$combatSummary["a_callback_failures"] -eq 0
                    callback_failures_b =
                        [int]$combatSummary["b_callback_failures"] -eq 0
                    post_death_frames_a =
                        $combatEvidence.PostDeathFramesA -ge 32
                    post_death_frames_b =
                        $combatEvidence.PostDeathFramesB -ge 32
                    post_death_client_a_progressed =
                        $combatEvidence.PostDeathClientAProgressed
                    post_death_client_b_progressed =
                        $combatEvidence.PostDeathClientBProgressed
                    first_respawn =
                        $combatEvidence.FirstRespawnPassed
                    first_death_camera_dwell =
                        $combatEvidence.FirstDeathCameraDwellPassed
                    second_death =
                        $combatEvidence.SecondDeathPassed
                    second_respawn =
                        $combatEvidence.SecondRespawnPassed
                    second_death_camera_dwell =
                        $combatEvidence.SecondDeathCameraDwellPassed
                    repeated_death_respawn_cycles =
                        $combatEvidence.DeathRespawnCycles -eq 8 -and
                            $combatEvidence.RepeatedCycleGatePassed
                    same_victim_body_queue_wrap =
                        $combatEvidence.SameVictimDeaths -eq 7 -and
                            [int]$combatSummary[
                                "body_queue_copy_calls"] -eq 8 -and
                            [int]$combatSummary[
                                "body_queue_distinct_nodes"] -eq 4 -and
                            $combatSummary[
                                "body_queue_sequence_valid"] -ceq "true" -and
                            [int]$combatSummary[
                                "body_queue_completed_cycles"] -eq 2
                    duplicate_attack_suppressed =
                        [int]$combatSummary[
                            "duplicate_attack_suppressed"] -eq 0
                    client_a_respawn_inputs =
                        [int]$combatSummary[
                            "a_respawn_inputs_forwarded"] -eq 1 -and
                            $combatEvidence.ClientARespawnInputsForwarded -eq 1
                    client_b_respawn_inputs =
                        [int]$combatSummary[
                            "b_respawn_inputs_forwarded"] -eq 7 -and
                            $combatEvidence.ClientBRespawnInputsForwarded -eq 7
                    server_still_responsive =
                        $combatEvidence.ServerStillResponsive
                }
            } else {
                [ordered]@{
                    vec_to_angles_callback =
                        [int]$combatSummary["vec_to_angles_calls"] -ge 1
                    crosshair_angle_callback =
                        [int]$combatSummary["crosshair_angle_calls"] -ge 1
                    shooter_reserve_unchanged =
                        [int]$combatSummary["a_reserve_before"] -eq
                            [int]$combatSummary["a_reserve_after"]
                    attack_received =
                        [int]$combatSummary["attack_received"] -eq 1
                    attack_executed =
                        [int]$combatSummary["attack_executed"] -eq 1
                    duplicate_attack_suppressed =
                        [int]$combatSummary[
                            "duplicate_attack_suppressed"] -eq 0
                    shooter_clip_before =
                        [int]$combatSummary["a_clip_before"] -eq
                            $combatEvidence.ShooterClipBefore
                    shooter_clip_after =
                        [int]$combatSummary["a_clip_after"] -eq
                            $combatEvidence.ShooterClipAfter
                    shooter_health_after =
                        [Math]::Abs(
                            [double]$combatSummary["a_health_after"] -
                            $combatEvidence.ShooterHealthAfter) -le 0.01
                    shooter_clientdata_health =
                        [Math]::Abs(
                            [double]$combatSummary[
                                "a_clientdata_health_after"] -
                            $combatEvidence.ShooterHealthAfter) -le 0.01
                    target_health_before =
                        [Math]::Abs(
                            [double]$combatSummary["b_health_before"] -
                            $combatEvidence.TargetHealthBefore) -le 0.01
                    target_health_after =
                        [Math]::Abs(
                            [double]$combatSummary["b_health_after"] -
                            $combatEvidence.TargetHealthAfter) -le 0.01
                    target_clientdata_health =
                        [Math]::Abs(
                            [double]$combatSummary[
                                "b_clientdata_health_after"] -
                            $combatEvidence.TargetHealthAfter) -le 0.01
                    target_alive =
                        [int]$combatSummary["b_deadflag"] -eq 0
                    shot_trace_present =
                        [int]$combatSummary["a_shot_traces"] -ge 1
                    shooter_ignored =
                        [int]$combatSummary["a_shooter_ignored"] -ge 1
                    player_hit_present =
                        [int]$combatSummary["a_shot_player_hits"] -ge 1
                    world_hit_count =
                        [int]$combatSummary["a_shot_world_hits"] -eq 0
                    world_occlusion_count =
                        [int]$combatSummary["a_world_occlusions"] -eq 0
                    target_identity =
                        [int]$combatSummary["a_last_target"] -eq 2
                    playback_event =
                        [int]$combatSummary["a_playback_events"] -ge 1
                }
            }
            foreach ($gate in $positiveSummaryGates.GetEnumerator()) {
                if (-not [bool]$gate.Value) {
                    throw ("positive Glock damage summary {0} gate failed" -f
                        $gate.Key)
                }
            }
        } else {
            $negativeSummaryGates = [ordered]@{
                shooter_health_unchanged =
                    [double]$combatSummary["a_health_before"] -eq
                        [double]$combatSummary["a_health_after"]
                target_health_min_unchanged =
                    [double]$combatSummary["b_health_before"] -eq
                        [double]$combatSummary["b_health_min"]
                target_health_after_unchanged =
                    [double]$combatSummary["b_health_before"] -eq
                        [double]$combatSummary["b_health_after"]
                attack_receipt_count =
                    [int]$combatSummary["attack_received"] -ge 6
                attack_execution_pairing =
                    [int]$combatSummary["attack_executed"] -eq
                        [int]$combatSummary["attack_received"]
                shooter_trace_count =
                    [int]$combatSummary["a_shot_traces"] -ge 4
                shooter_ignored =
                    [int]$combatSummary["a_shooter_ignored"] -ge 1
                shooter_world_hit_present =
                    [int]$combatSummary["a_shot_world_hits"] -ge 1
                shooter_world_occlusion_present =
                    [int]$combatSummary["a_world_occlusions"] -ge 1
                aggregate_world_occlusion_present =
                    [int]$combatSummary["world_occlusions"] -ge 1
                second_client_trace_present =
                    [int]$combatSummary["b_shot_traces"] -ge 1
                second_client_shooter_ignored =
                    [int]$combatSummary["b_shooter_ignored"] -ge 1
                duplicate_attack_accounted =
                    [int]$combatSummary["duplicate_attack_suppressed"] -eq 1
                shooter_rounds_consumed =
                    [int]$combatSummary["a_clip_before"] -
                        [int]$combatSummary["a_clip_min"] -ge 4
                second_client_round_consumed =
                    [int]$combatSummary["b_clip_before"] -
                        [int]$combatSummary["b_clip_min"] -ge 1
                reconnect_count = [int]$clientSummaryB["reconnects"] -ge 1
                slot_reuse_clean =
                    $clientSummaryB["slot_reuse_clean"] -ceq "true"
            }
            foreach ($gate in $negativeSummaryGates.GetEnumerator()) {
                if (-not [bool]$gate.Value) {
                    throw ("negative Glock isolation summary {0} gate failed" -f
                        $gate.Key)
                }
            }
        }
        $result = [pscustomobject]@{
            ClientA = $clientSummaryA
            ClientB = $clientSummaryB
            Combat = $combatSummary
            Evidence = $combatEvidence
        }
        }
    }

    if (-not $CombatProof) {
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
        -ExpectedEntries $expectedEntries `
        -ExactCombatBootstrap:$CombatProof
    $stateAReconnect = New-TwoClientStreamState `
        -Client $clientAReconnect -OwnEntity 1 -Connection $connectionAReconnect
    Sync-OneClientSnapshotFrontier `
        -State $stateB `
        -ServerProcess $serverProcess `
        -OutputCapture $outputCapture `
        -StdoutPath $stdoutPath `
        -Deadline $deadline `
        -ServerEndpoint $serverEndpoint
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
    foreach ($path in @($stdoutPath, $stderrPath, $shutdownPath, $controlPath)) {
        if (Test-Path -LiteralPath $path -PathType Leaf) {
            Remove-Item -LiteralPath $path -Force -ErrorAction SilentlyContinue
        }
    }
    if ($null -ne $serverProcess) {
        $serverProcess.Dispose()
    }
}

if ($null -ne $failure) {
    $safeStage = if ($proofStage -match '^[a-z0-9_]{1,64}$') {
        $proofStage
    } else {
        'unknown'
    }
    $safeReason = if ($failure.Exception.Message -ceq
            'proof global budget exhausted') {
        'global_budget_exhausted'
    } elseif ($failure.Exception.Message -ceq
            'client a snapshot idle timeout') {
        'snapshot_idle_client_a'
    } elseif ($failure.Exception.Message -ceq
            'client b snapshot idle timeout') {
        'snapshot_idle_client_b'
    } elseif ($failure.Exception.Message -ceq
            'client a snapshot index idle timeout') {
        'snapshot_index_idle_client_a'
    } elseif ($failure.Exception.Message -ceq
            'client b snapshot index idle timeout') {
        'snapshot_index_idle_client_b'
    } elseif ($failure.Exception.Message -match
            '^lethal repeated respawn( ready)? frame bound exhausted$') {
        'phase_semantic_frame_bound_exhausted'
    } elseif ($failure.Exception.Message -ceq
            'two-client server unavailable') {
        'server_unavailable'
    } elseif ($safeStage -match '^lethal_.*fixture' -and
        $failure.Exception.Message -match
            '^combat fixture rejected: (edict_unavailable|shooter_anchor_not_ready|target_anchor_not_ready|lethal_player_not_alive|lethal_target_not_damageable|forced_respawn_cvar_unavailable|forced_respawn_cvar_not_fixed|clear_fixture_not_found)$') {
        'combat_fixture_' + $Matches[1]
    } elseif ($safeStage -match '^lethal_.*fixture' -and
        $failure.Exception.Message -ceq
            'combat fixture acknowledgement timed out') {
        'combat_fixture_acknowledgement_unavailable'
    } elseif ($safeStage -match '^fall_' -and
        $capturedStdout -match
            'goldsrc_stock_test_fall_observation: slot=[12],session_generation=[0-9]+,mode=(landing|damage),status=failed,reason=world_ground_required') {
        'fall_world_ground_required'
    } elseif ($safeStage -match '^fall_' -and
        $capturedStdout -match
            'goldsrc_stock_test_fall_observation: slot=[12],session_generation=[0-9]+,mode=(landing|damage),status=failed,reason=health_transition_invalid') {
        'fall_health_transition_invalid'
    } elseif ($safeStage -match '^fall_' -and
        $capturedStdout -match
            'goldsrc_stock_test_fall_observation: slot=[12],session_generation=[0-9]+,mode=(landing|damage),status=failed,reason=peer_not_ready') {
        'fall_peer_not_ready'
    } elseif ($safeStage -match '^fall_' -and
        $failure.Exception.Message -ceq 'fall observation rejected') {
        'fall_observation_failed'
    } elseif ($safeStage -match '^fall_' -and
        $failure.Exception.Message -ceq 'fall responsiveness rejected') {
        'fall_responsiveness_observation_failed'
    } elseif ($safeStage -ceq 'fall_landing_transition' -and
        $capturedStdout -notmatch
            'goldsrc_stock_test_fall_observation: slot=1,session_generation=[0-9]+,mode=landing,status=pass,reason=none') {
        'fall_landing_grounded_marker_missing'
    } elseif ($safeStage -ceq 'fall_damage_landing' -and
        $capturedStdout -notmatch
            'goldsrc_stock_test_fall_observation: slot=1,session_generation=[0-9]+,mode=damage,status=pass,reason=none') {
        'fall_damage_grounded_marker_missing'
    } elseif ($safeStage -match '^fall_' -and
        $failure.Exception.Message -ceq 'fall landing transition unconfirmed') {
        'fall_landing_transition_unconfirmed'
    } elseif ($safeStage -match '^fall_' -and
        $failure.Exception.Message -ceq 'fall landing server unavailable') {
        'fall_landing_server_unavailable'
    } elseif ($safeStage -match '^fall_' -and
        $failure.Exception.Message -ceq 'fall landing callback failure') {
        'fall_landing_callback_failed'
    } elseif ($safeStage -match '^fall_' -and
        $failure.Exception.Message -ceq 'fall landing health transition failed') {
        'fall_landing_health_transition_failed'
    } elseif ($safeStage -match '^fall_' -and
        $failure.Exception.Message -ceq 'fall landing isolation failed') {
        'fall_landing_isolation_failed'
    } elseif ($safeStage -match '^fall_' -and
        $failure.Exception.Message -ceq
            'combat clientdata health evidence is unavailable') {
        'fall_wire_clientdata_incomplete'
    } elseif ($safeStage -match '^fall_' -and
        $failure.Exception.Message -ceq
            'combat movement progress evidence is incomplete') {
        'fall_progress_incomplete'
    } elseif ($safeStage -match '^fall_' -and
        $failure.Exception.Message -ceq
            'combat progress state evidence is incomplete') {
        'fall_progress_state_incomplete'
    } elseif ($safeStage -match '^fall_' -and
        $failure.Exception.Message -match
            '^combat (semantic snapshot|Glock weapondata|Glock clip) evidence is (unavailable|invalid)$') {
        'fall_wire_snapshot_incomplete'
    } elseif ($safeStage -match '^fall_' -and
        $failure.Exception.Message -ceq 'fall responsiveness runtime failed') {
        'fall_responsiveness_runtime_failed'
    } elseif ($safeStage -match '^fall_' -and
        $failure.Exception.Message -ceq 'fall authoritative movement missing') {
        'fall_authoritative_movement_missing'
    } elseif ($safeStage -match '^fall_' -and
        $failure.Exception.Message -ceq 'fall wire movement missing both') {
        'fall_wire_movement_missing_both'
    } elseif ($safeStage -match '^fall_' -and
        $failure.Exception.Message -ceq 'fall wire movement missing a') {
        'fall_wire_movement_missing_a'
    } elseif ($safeStage -match '^fall_' -and
        $failure.Exception.Message -ceq 'fall wire movement missing b') {
        'fall_wire_movement_missing_b'
    } elseif ($safeStage -match '^fall_' -and
        $failure.Exception.Message -ceq 'fall responsiveness contract missing') {
        'fall_responsiveness_contract_missing'
    } elseif ($safeStage -match '^fall_' -and
        $failure.Exception.Message -ceq 'fall post landing state changed') {
        'fall_post_landing_state_changed'
    } elseif ($safeStage -match '^fall_' -and
        $fallDiagnosticStage -match
            '^(landing|damage|responsive)_[a-z0-9_]{1,48}$') {
        'fall_' + $fallDiagnosticStage + '_failed'
    } elseif ($safeStage -match '^fall_') {
        'fall_observation_unconfirmed'
    } elseif ($failure.Exception.Message -match
            'bootstrap tail z_maximum mismatch') {
        'bootstrap_zmax_mismatch'
    } elseif ($failure.Exception.Message -match
            '^bootstrap tail sky_color_(red|green|blue) mismatch: expected [-+0-9.eE]+, received ([-+0-9.eE]+)$') {
        ('bootstrap_sky_color_{0}_received_{1}' -f $Matches[1], $Matches[2])
    } elseif ($failure.Exception.Message -match
            'bootstrap tail sky_vector_(x|y|z) mismatch') {
        'bootstrap_sky_vector_mismatch'
    } elseif ($failure.Exception.Message -match
            'bootstrap tail sky name mismatch') {
        'bootstrap_sky_name_mismatch'
    } elseif ($failure.Exception.Message -match
            'bootstrap tail CD track mismatch') {
        'bootstrap_cd_track_mismatch'
    } elseif ($failure.Exception.Message -match
            'semantic snapshot evidence') {
        'semantic_snapshot_evidence_failed'
    } elseif ($failure.Exception.Message -match
            '^negative Glock isolation summary ([a-z0-9_]+) gate failed$') {
        ('negative_summary_{0}_failed' -f $Matches[1])
    } elseif ($safeStage -eq 'combat_wall_occlusion' -and
            $failure.Exception.Message -ceq
                'wall occlusion exact round consumption gate failed') {
        'wall_round_consumption_failed'
    } elseif ($safeStage -eq 'combat_wall_occlusion' -and
            $failure.Exception.Message -ceq
                'wall occlusion shooter health isolation gate failed') {
        'wall_shooter_health_isolation_failed'
    } elseif ($safeStage -eq 'combat_wall_occlusion' -and
            $failure.Exception.Message -ceq
                'wall occlusion target health isolation gate failed') {
        'wall_target_health_isolation_failed'
    } elseif ($safeStage -eq 'combat_wall_occlusion' -and
            $failure.Exception.Message -ceq
                'wall occlusion target weapon isolation gate failed') {
        'wall_target_weapon_isolation_failed'
    } elseif ($safeStage -eq 'combat_wall_occlusion' -and
            $failure.Exception.Message -ceq
                'wall occlusion actual shot selected a player') {
        'wall_player_occlusion_failed'
    } elseif ($safeStage -eq 'combat_wall_occlusion' -and
            $failure.Exception.Message -ceq
                'wall occlusion actual shot did not select world') {
        'wall_world_trace_failed'
    } elseif ($failure.Exception.Message -match
            '(lifecycle|clientdata|weapon-data|weapon data|snapshot|delta selected)') {
        'snapshot_semantic_failed'
    } elseif ($failure.Exception.Message -match
            'spawned PM_Move state') {
        'spawned_pmove_state_failed'
    } elseif ($failure.Exception.Message -match 'bootstrap') {
        'bootstrap_semantic_gate_failed'
    } elseif ($failure.Exception.Message -match 'resource|manifest') {
        'resource_semantic_gate_failed'
    } elseif ($failure.Exception.Message -match 'pmove|PM_Move|movement') {
        'pmove_semantic_gate_failed'
    } elseif ($failure.Exception.Message -match 'player|lifecycle') {
        'player_semantic_gate_failed'
    } elseif ($failure.Exception.Message -match 'unexpected|carrier|opcode') {
        'protocol_order_gate_failed'
    } elseif ($failure.Exception.Message -match
            'movement progress') {
        'movement_progress_evidence_failed'
    } elseif ($failure.Exception.Message -match
            "Cannot bind argument to parameter '([A-Za-z0-9_]+)'") {
        'proof_parameter_binding_failed'
    } elseif ($failure.Exception.Message -match
            "The property '([A-Za-z0-9_]+)' cannot be found") {
        'proof_property_missing'
    } elseif ($failure.Exception.Message -match '^Cannot convert value') {
        'proof_value_conversion_failed'
    } elseif ($failure.Exception.Message -match '^Method invocation failed') {
        'proof_method_invocation_failed'
    } elseif ($failure.Exception.Message -match
            '^Exception calling "([A-Za-z0-9_]+)"') {
        'proof_method_failed'
    } elseif ($failure.Exception.Message -match '^A parameter cannot be found') {
        'proof_parameter_missing'
    } elseif ($failure.Exception.Message -match
            "^The term '([A-Za-z0-9_-]+)' is not recognized") {
        'proof_command_missing'
    } elseif ($failure.Exception.Message -match '^The term ') {
        'proof_command_resolution_failed'
    } elseif ($safeStage -match 'clean_shutdown$') {
        'clean_shutdown_failed'
    } elseif ($safeStage -match 'summary_validation$') {
        'summary_gate_failed'
    } elseif ($safeStage -eq 'server_start') {
        'server_start_failed'
    } else {
        'proof_gate_failed'
    }
    Write-Host ("two-client proof failed: stage={0}; reason={1}" -f
        $safeStage,
        $safeReason)
    exit 1
}
if ($FeatureOffProof) {
    $evidence = $result.Evidence
    Write-Host ("feature_off_attack_received=" + $evidence.AttackMoveReceived.ToString().ToLowerInvariant())
    Write-Host ("feature_off_attack_validated=" + $evidence.AttackMoveValidated.ToString().ToLowerInvariant())
    Write-Host ("feature_off_attack_masked=" + $evidence.AttackMasked.ToString().ToLowerInvariant())
    Write-Host ("feature_off_gameplay_callbacks_absent=" + $evidence.GameplayCallbacksAbsent.ToString().ToLowerInvariant())
    Write-Host ("feature_off_firing_absent=" + $evidence.HealthUnchanged.ToString().ToLowerInvariant())
    Write-Host ("feature_off_health_unchanged=" + $evidence.HealthUnchanged.ToString().ToLowerInvariant())
    Write-Host "feature_off_ammo_observation=unavailable"
    Write-Host ("feature_off_weapondata_absent=" + $evidence.WeaponDataAbsent.ToString().ToLowerInvariant())
    Write-Host ("feature_off_movement_after_attack=" + $(if ($evidence.MovementStable) { 'pass' } else { 'fail' }))
    Write-Host "clean_shutdown=1"
    Write-Host "feature_off_proof=pass"
} elseif ($FallDamageProof) {
    $evidence = $result.Evidence
    Write-Host "fall_fixture=applied"
    Write-Host "landing_mode_velocity=400"
    Write-Host "landing_mode_grounded=true"
    Write-Host "landing_mode_health_unchanged=true"
    Write-Host "landing_mode_postthink_advanced=true"
    Write-Host "damage_mode_velocity=600"
    Write-Host "damage_mode_grounded=true"
    Write-Host "fall_damage_amount=10"
    Write-Host ("fall_target_alive=" +
        $evidence.TargetAlive.ToString().ToLowerInvariant())
    Write-Host ("player_postthink_advanced=" +
        $evidence.PlayerPostThinkAdvanced.ToString().ToLowerInvariant())
    Write-Host "gameplay_callback_failures=0"
    Write-Host ("clientdata_health_updated=" +
        $evidence.ClientDataHealthUpdated.ToString().ToLowerInvariant())
    Write-Host ("peer_state_unchanged=" +
        $evidence.PeerStateUnchanged.ToString().ToLowerInvariant())
    Write-Host ("post_landing_movement=" + $(if (
        $evidence.PostLandingMovement) { 'pass' } else { 'fail' }))
    Write-Host ("server_still_responsive=" +
        $evidence.ServerStillResponsive.ToString().ToLowerInvariant())
    Write-Host "clean_shutdown=1"
    Write-Host "fall_damage_proof=pass"
} elseif ($CombatProof -and $NegativeProof) {
    $evidence = $result.Evidence
    Write-Host ("miss_no_damage=" + $(if ($evidence.MissNoDamage) { 'pass' } else { 'fail' }))
    Write-Host ("wall_occlusion=" + $(if ($evidence.WallOcclusion) { 'pass' } else { 'fail' }))
    Write-Host ("self_hit_prevented=" + $evidence.SelfHitPrevented.ToString().ToLowerInvariant())
    Write-Host ("duplicate_attack_suppressed=" + $evidence.ExactDuplicateSuppressed.ToString().ToLowerInvariant())
    Write-Host ("duplicate_backup_attack_suppressed=" + $evidence.BackupReplaySuppressed.ToString().ToLowerInvariant())
    Write-Host ("cooldown_enforced=" + $evidence.CooldownEnforced.ToString().ToLowerInvariant())
    Write-Host ("invalid_checksum_no_fire=" + $evidence.InvalidChecksumNoFire.ToString().ToLowerInvariant())
    Write-Host ("malformed_command_no_fire=" + $evidence.MalformedMoveNoFire.ToString().ToLowerInvariant())
    Write-Host ("attack2_masked=" + $evidence.Attack2Masked.ToString().ToLowerInvariant())
    Write-Host ("disconnected_target_safe=" + $evidence.DisconnectedTargetSafe.ToString().ToLowerInvariant())
    Write-Host ("stale_attacker_packet_rejected=" + $evidence.StaleAttackerPacketRejected.ToString().ToLowerInvariant())
    Write-Host ("slot_reuse_clean=" + $evidence.SlotReuseClean.ToString().ToLowerInvariant())
    Write-Host ("cross_client_weapon_state_leak=" + (-not $evidence.CrossClientIsolation).ToString().ToLowerInvariant())
    Write-Host ("server_still_responsive=" + $evidence.ServerStillResponsive.ToString().ToLowerInvariant())
    Write-Host "clean_shutdown=1"
    Write-Host "proof_b=pass"
} elseif ($CombatProof -and $LethalDeathProof) {
    $evidence = $result.Evidence
    Write-Host "client_a_connected=true"
    Write-Host "client_b_connected=true"
    Write-Host "client_a_spawned=true"
    Write-Host "client_b_spawned=true"
    Write-Host "aim_regression_phase=pass"
    Write-Host "nonlethal_control=pass"
    Write-Host "nonlethal_control_health=100_to_88"
    Write-Host "lethal_damage_per_shot=12"
    Write-Host "lethal_shots_total=8"
    Write-Host "first_lethal_transition=pass"
    Write-Host "first_death_camera_dwell=pass"
    Write-Host "first_respawn=pass"
    Write-Host "second_lethal_transition=pass"
    Write-Host "second_death_camera_dwell=pass"
    Write-Host "second_respawn=pass"
    Write-Host "death_respawn_cycles=8"
    Write-Host "same_victim_deaths=7"
    Write-Host "body_queue_copy_calls=8"
    Write-Host "body_queue_distinct_nodes=4"
    Write-Host "body_queue_completed_cycles=2"
    Write-Host "body_queue_sequence_valid=true"
    Write-Host "repeated_death_respawn_gate=pass"
    Write-Host "both_players_alive_after_eighth_respawn=true"
    Write-Host "respawn_clicks_forwarded=8"
    Write-Host "respawn_clicks_counted_as_weapon_attacks=0"
    Write-Host "attack_commands_received=9"
    Write-Host "attack_commands_executed=9"
    Write-Host "rounds_consumed=9"
    Write-Host "player_hit_shots=9"
    Write-Host "player_trace_callbacks=18"
    Write-Host "world_hit_traces=0"
    Write-Host ("post_death_frames_a=" + $evidence.PostDeathFramesA)
    Write-Host ("post_death_frames_b=" + $evidence.PostDeathFramesB)
    Write-Host "post_death_client_a_progression=pass"
    Write-Host "post_death_client_b_progression=pass"
    Write-Host "gameplay_callback_failures=0"
    Write-Host "server_still_responsive=true"
    Write-Host "clean_shutdown=1"
    Write-Host "repeated_lethal_death_proof=pass"
} elseif ($CombatProof) {
    $evidence = $result.Evidence
    $combat = $result.Combat
    [double]$targetHealthBefore = $evidence.TargetHealthBefore
    [double]$targetHealthAfter = $evidence.TargetHealthAfter
    [int]$shooterAmmoBefore = $evidence.ShooterClipBefore
    [int]$shooterAmmoAfter = $evidence.ShooterClipAfter
    Write-Host "client_a_connected=true"
    Write-Host "client_b_connected=true"
    Write-Host "client_a_spawned=true"
    Write-Host "client_b_spawned=true"
    Write-Host "glock_available=true"
    Write-Host "glock_active=true"
    Write-Host "glock_weapon_id=2"
    Write-Host "aim_regression_phase=pass"
    Write-Host "aim_fixture=applied"
    Write-Host "aim_target_inside_autoaim_cone=true"
    Write-Host "aim_direct_ray_misses_target=true"
    Write-Host "aim_buttons_zero=true"
    Write-Host "aim_phase_attack_commands=0"
    Write-Host "aim_phase_ammo_unchanged=true"
    Write-Host "aim_phase_health_unchanged=true"
    Write-Host "aim_phase_client_a_callbacks=pass"
    Write-Host "aim_phase_client_b_callbacks=pass"
    Write-Host "aim_phase_server_responsive=true"
    Write-Host "vec_to_angles_called=true"
    Write-Host "crosshair_angle_called=true"
    Write-Host ("combat_callback_order_verified=" +
        $combat["callback_order_verified"])
    Write-Host ("combat_time_contract_verified=" +
        $combat["time_contract_verified"])
    Write-Host ("start_frame_call_count=" +
        $combat["start_frame_calls"])
    Write-Host ("player_prethink_calls_a=" + $combat["a_prethink"])
    Write-Host ("player_prethink_calls_b=" + $combat["b_prethink"])
    Write-Host ("player_postthink_calls_a=" + $combat["a_postthink"])
    Write-Host ("player_postthink_calls_b=" + $combat["b_postthink"])
    Write-Host "attack_command_delivered=true"
    Write-Host "attack_command_executed_once=true"
    Write-Host ("attack_commands_received=" +
        $combat["attack_received"])
    Write-Host ("attack_commands_executed=" +
        $combat["attack_executed"])
    Write-Host ("duplicate_attack_commands_suppressed=" +
        $combat["duplicate_attack_suppressed"])
    Write-Host "player_trace_hit_b=true"
    Write-Host ("player_aware_trace=" + $combat["player_aware_trace"])
    Write-Host ("shooter_ignored=" + $(if (
        [int]$combat["a_shooter_ignored"] -ge 1) { 'true' } else { 'false' }))
    Write-Host ("wall_occluded=" + $evidence.WallOccluded.ToString().ToLowerInvariant())
    Write-Host "target_health_before=$targetHealthBefore"
    Write-Host "target_health_after=$targetHealthAfter"
    Write-Host "damage_amount_observed=$($targetHealthBefore - $targetHealthAfter)"
    Write-Host "target_alive=true"
    Write-Host ("target_deadflag=" + $combat["b_deadflag"])
    Write-Host ("shooter_health_before=" + $combat["a_health_before"])
    Write-Host ("shooter_health_after=" + $combat["a_health_after"])
    Write-Host "shooter_ammo_before=$shooterAmmoBefore"
    Write-Host "shooter_ammo_after=$shooterAmmoAfter"
    Write-Host ("shooter_reserve_ammo_before=" +
        $combat["a_reserve_before"])
    Write-Host ("shooter_reserve_ammo_after=" +
        $combat["a_reserve_after"])
    Write-Host "rounds_consumed=$($shooterAmmoBefore - $shooterAmmoAfter)"
    Write-Host ("target_clientdata_updated=" + $evidence.TargetClientDataUpdated.ToString().ToLowerInvariant())
    Write-Host ("shooter_weapondata_updated=" + $evidence.ShooterWeaponDataUpdated.ToString().ToLowerInvariant())
    Write-Host ("per_client_weapondata=" +
        $combat["per_client_weapondata"])
    Write-Host ("frame_history_weapondata=" +
        $combat["frame_history_weapondata"])
    Write-Host ("playback_event_calls=" +
        $combat["a_playback_events"])
    Write-Host ("client_a_movement_after_shot=" + $(if ($evidence.ClientAMovementAfterShot) { 'pass' } else { 'fail' }))
    Write-Host ("client_b_movement_after_shot=" + $(if ($evidence.ClientBMovementAfterShot) { 'pass' } else { 'fail' }))
    Write-Host "clean_shutdown=1"
    Write-Host "proof_a=pass"
} elseif ($NegativeProof) {
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
