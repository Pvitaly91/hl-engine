[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$ExecutablePath,

    [Parameter(Mandatory = $true)]
    [string]$GameDir,

    [string]$ManifestFixture = (Join-Path $PSScriptRoot "../src/tests/fixtures/goldsrc_resource_manifest_minimal.tsv"),

    [string]$BindAddress = "127.0.0.1",

    [ValidateRange(0, 65535)]
    [int]$Port = 0,

    [ValidateRange(1, 300)]
    [int]$TimeoutSeconds = 30,

    [switch]$NegativeProof,

    [switch]$SkipServerOutput
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
if (Test-Path Variable:PSNativeCommandUseErrorActionPreference) {
    $PSNativeCommandUseErrorActionPreference = $false
}

# Keep process, UDP, netchan, repository-mutation, and cleanup mechanics in
# lockstep with the previous externally observed serverinfo proof. Only the
# top-level helper definitions are imported; that script's probe body is never
# executed.
$serverInfoProofPath = Join-Path $PSScriptRoot "run_goldsrc_serverinfo_proof.ps1"
if (-not (Test-Path -LiteralPath $serverInfoProofPath -PathType Leaf)) {
    throw ("serverinfo proof helper not found: {0}" -f $serverInfoProofPath)
}
$helperTokens = $null
$helperErrors = $null
$helperAst = [System.Management.Automation.Language.Parser]::ParseFile(
    $serverInfoProofPath,
    [ref]$helperTokens,
    [ref]$helperErrors
)
if (@($helperErrors).Count -ne 0) {
    throw ("serverinfo proof helper has {0} parser errors" -f @($helperErrors).Count)
}
$helperDefinitions = @(
    $helperAst.EndBlock.Statements |
        Where-Object {
            $_ -is [System.Management.Automation.Language.FunctionDefinitionAst]
        }
)
if ($helperDefinitions.Count -eq 0) {
    throw "serverinfo proof helper contains no reusable function definitions"
}
foreach ($definition in $helperDefinitions) {
    . ([scriptblock]::Create($definition.Extent.Text))
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
$maximumManifestBytes = 1200
$maximumResourceCount = 1280
$maximumResourcePathBytes = 63
$maximumResourceIndex = 4095
$maximumResourceDownloadBytes = 16777215
$maximumResourceFlags = 7
$resourceCustomFlag = 4

function Assert-CanonicalResourcePath {
    param(
        [string]$Path,
        [string]$Description
    )

    if ([string]::IsNullOrEmpty($Path)) {
        throw ("{0} path must not be empty" -f $Description)
    }
    if ([System.Text.Encoding]::UTF8.GetByteCount($Path) -gt $maximumResourcePathBytes) {
        throw ("{0} path exceeds {1} bytes" -f $Description, $maximumResourcePathBytes)
    }
    if ($Path -cne $Path.ToLowerInvariant()) {
        throw ("{0} path must already be lower-case canonical text" -f $Description)
    }
    if ($Path[0] -eq '/' -or $Path[0] -eq [char]92) {
        throw ("{0} path must be relative" -f $Description)
    }
    if ($Path.IndexOf(':') -ge 0 -or $Path.IndexOf([char]92) -ge 0) {
        throw ("{0} path contains a drive, colon, or backslash" -f $Description)
    }
    foreach ($character in $Path.ToCharArray()) {
        $value = [int]$character
        if ($value -lt 0x20 -or $value -gt 0x7E) {
            throw ("{0} path contains a non-printable or non-ASCII byte" -f $Description)
        }
    }
    $segments = $Path.Split([char]'/')
    foreach ($segment in $segments) {
        if ($segment.Length -eq 0 -or $segment -ceq "." -or $segment -ceq "..") {
            throw ("{0} path contains an empty, dot, or parent segment" -f $Description)
        }
    }
}

function Read-ResourceManifestFixture {
    param([string]$Path)

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw ("resource manifest fixture not found: {0}" -f $Path)
    }

    $typeMetadata = @{
        generic = [pscustomobject]@{ WireType = [byte]4; Rank = 0 }
        sound = [pscustomobject]@{ WireType = [byte]0; Rank = 1 }
        model = [pscustomobject]@{ WireType = [byte]2; Rank = 2 }
        decal = [pscustomobject]@{ WireType = [byte]3; Rank = 3 }
        event = [pscustomobject]@{ WireType = [byte]5; Rank = 4 }
    }
    $entries = New-Object System.Collections.Generic.List[object]
    $ownedIndices = New-Object 'System.Collections.Generic.HashSet[string]' ([StringComparer]::Ordinal)
    $seenCategories = New-Object 'System.Collections.Generic.HashSet[string]' ([StringComparer]::Ordinal)
    $worldCount = 0
    $previousRank = -1
    [uint32]$previousIndex = 0
    $previousPath = ""
    $lineNumber = 0

    foreach ($line in [System.IO.File]::ReadAllLines($Path, [System.Text.Encoding]::UTF8)) {
        $lineNumber++
        if ($line.Length -eq 0 -or $line[0] -eq '#') {
            continue
        }
        $fields = [regex]::Split($line, [string][char]9)
        if ($fields.Count -ne 5) {
            throw ("fixture line {0} must contain exactly five tab-separated fields" -f $lineNumber)
        }

        $typeName = $fields[0]
        if ($typeName -cne $typeName.ToLowerInvariant() -or
            -not $typeMetadata.ContainsKey($typeName)) {
            throw ("fixture line {0} has an unsupported resource type" -f $lineNumber)
        }
        $metadata = $typeMetadata[$typeName]

        [uint32]$index = 0
        if (-not [uint32]::TryParse(
                $fields[1],
                [Globalization.NumberStyles]::None,
                [Globalization.CultureInfo]::InvariantCulture,
                [ref]$index) -or
            $index -gt $maximumResourceIndex) {
            throw ("fixture line {0} has an invalid decimal resource index" -f $lineNumber)
        }

        [uint32]$downloadSize = 0
        if (-not [uint32]::TryParse(
                $fields[2],
                [Globalization.NumberStyles]::None,
                [Globalization.CultureInfo]::InvariantCulture,
                [ref]$downloadSize) -or
            $downloadSize -gt $maximumResourceDownloadBytes) {
            throw ("fixture line {0} has an invalid decimal download size" -f $lineNumber)
        }

        [uint32]$flags = 0
        if (-not [uint32]::TryParse(
                $fields[3],
                [Globalization.NumberStyles]::None,
                [Globalization.CultureInfo]::InvariantCulture,
                [ref]$flags) -or
            $flags -gt $maximumResourceFlags) {
            throw ("fixture line {0} has invalid decimal flags" -f $lineNumber)
        }

        $resourcePath = $fields[4]
        Assert-CanonicalResourcePath -Path $resourcePath -Description ("fixture line {0}" -f $lineNumber)

        $ownership = "{0}:{1}" -f $typeName, $index
        if (-not $ownedIndices.Add($ownership)) {
            throw ("fixture line {0} duplicates resource ownership {1}" -f $lineNumber, $ownership)
        }
        [void]$seenCategories.Add($typeName)

        if ($metadata.Rank -lt $previousRank -or
            ($metadata.Rank -eq $previousRank -and
                ($index -lt $previousIndex -or
                    ($index -eq $previousIndex -and
                        [StringComparer]::Ordinal.Compare($resourcePath, $previousPath) -le 0)))) {
            throw ("fixture line {0} violates deterministic category/index/path ordering" -f $lineNumber)
        }
        $previousRank = $metadata.Rank
        $previousIndex = $index
        $previousPath = $resourcePath

        if ($typeName -ceq "model" -and $index -eq 1) {
            if ($resourcePath -cne "maps/c0a0.bsp") {
                throw "fixture model index 1 must be maps/c0a0.bsp"
            }
            $worldCount++
        }

        $entries.Add([pscustomobject]@{
            TypeName = $typeName
            WireType = [byte]$metadata.WireType
            Index = [uint16]$index
            DownloadSize = [uint32]$downloadSize
            Flags = [byte]$flags
            Path = $resourcePath
        })
    }

    if ($entries.Count -eq 0 -or $entries.Count -gt $maximumResourceCount) {
        throw ("fixture resource count is outside 1..{0}" -f $maximumResourceCount)
    }
    foreach ($requiredCategory in @("generic", "sound", "model", "decal", "event")) {
        if (-not $seenCategories.Contains($requiredCategory)) {
            throw ("fixture is missing required category {0}" -f $requiredCategory)
        }
    }
    if ($worldCount -ne 1) {
        throw "fixture must contain exactly one model index 1 world resource"
    }
    return $entries.ToArray()
}

function Read-ManifestBits {
    param(
        $Reader,
        [ValidateRange(1, 32)]
        [int]$Count,
        [string]$FieldName
    )

    $availableBits = ($Reader.Bytes.Length * 8) - [int]$Reader.BitOffset
    if ($Count -gt $availableBits) {
        throw ("resource manifest is truncated while reading {0}" -f $FieldName)
    }
    [uint64]$value = 0
    for ($bitIndex = 0; $bitIndex -lt $Count; $bitIndex++) {
        $source = $Reader.Bytes[[Math]::Floor($Reader.BitOffset / 8)]
        $mask = 1 -shl ($Reader.BitOffset % 8)
        if (($source -band $mask) -ne 0) {
            $value = $value -bor (([uint64]1) -shl $bitIndex)
        }
        $Reader.BitOffset = [int]$Reader.BitOffset + 1
    }
    return [uint32]$value
}

function Read-ManifestProtocolString {
    param(
        $Reader,
        [string]$FieldName
    )

    $bytes = New-Object System.Collections.Generic.List[byte]
    for ($index = 0; $index -le $maximumResourcePathBytes; $index++) {
        $value = Read-ManifestBits -Reader $Reader -Count 8 -FieldName $FieldName
        if ($value -eq 0) {
            if ($bytes.Count -eq 0) {
                throw ("resource manifest {0} is empty" -f $FieldName)
            }
            $text = [System.Text.Encoding]::ASCII.GetString($bytes.ToArray())
            Assert-CanonicalResourcePath -Path $text -Description ("resource manifest {0}" -f $FieldName)
            return $text
        }
        if ($index -eq $maximumResourcePathBytes) {
            throw ("resource manifest {0} exceeds the bounded path length" -f $FieldName)
        }
        if ($value -lt 0x20 -or $value -gt 0x7E) {
            throw ("resource manifest {0} contains a non-printable byte" -f $FieldName)
        }
        $bytes.Add([byte]$value)
    }
    throw ("resource manifest {0} has no bounded terminator" -f $FieldName)
}

function Get-ResourceTypeName {
    param([uint32]$WireType)

    switch ($WireType) {
        0 { return "sound" }
        2 { return "model" }
        3 { return "decal" }
        4 { return "generic" }
        5 { return "event" }
        default { throw ("resource manifest contains unsupported type {0}" -f $WireType) }
    }
}

function Read-ResourceManifestPayload {
    param(
        [byte[]]$Payload,
        [uint32]$ExpectedSpawnCount
    )

    if ($Payload.Length -le 9 -or $Payload.Length -gt $maximumManifestBytes) {
        throw ("resource manifest response length is outside 10..{0}" -f $maximumManifestBytes)
    }
    if ($Payload[0] -ne $svcResourceRequest) {
        throw "resource manifest companion opcode is not the observed request companion"
    }
    $spawnCount = Read-LittleEndianUInt32 -Bytes $Payload -Offset 1
    $reserved = Read-LittleEndianUInt32 -Bytes $Payload -Offset 5
    if ($spawnCount -ne $ExpectedSpawnCount) {
        throw ("resource manifest companion spawn count differs from serverinfo: {0} != {1}" -f
            $spawnCount,
            $ExpectedSpawnCount)
    }
    if ($reserved -ne 0) {
        throw "resource manifest companion reserved value is nonzero"
    }

    $manifestBytes = New-Object byte[] ($Payload.Length - 9)
    [Array]::Copy($Payload, 9, $manifestBytes, 0, $manifestBytes.Length)
    $reader = [pscustomobject]@{
        Bytes = $manifestBytes
        BitOffset = 0
    }
    $opcode = Read-ManifestBits -Reader $reader -Count 8 -FieldName "svc_resourcelist opcode"
    if ($opcode -ne $svcResourceList) {
        throw "resource manifest list opcode is not svc_resourcelist"
    }
    $resourceCount = Read-ManifestBits -Reader $reader -Count 12 -FieldName "resource count"
    if ($resourceCount -eq 0 -or $resourceCount -gt $maximumResourceCount) {
        throw ("resource manifest count is outside 1..{0}" -f $maximumResourceCount)
    }

    $entries = New-Object System.Collections.Generic.List[object]
    for ($entryIndex = 0; $entryIndex -lt $resourceCount; $entryIndex++) {
        $wireType = Read-ManifestBits -Reader $reader -Count 4 -FieldName ("resource {0} type" -f $entryIndex)
        $typeName = Get-ResourceTypeName -WireType $wireType
        $resourcePath = Read-ManifestProtocolString -Reader $reader -FieldName ("resource {0} path" -f $entryIndex)
        $index = Read-ManifestBits -Reader $reader -Count 12 -FieldName ("resource {0} index" -f $entryIndex)
        $downloadSize = Read-ManifestBits -Reader $reader -Count 24 -FieldName ("resource {0} download size" -f $entryIndex)
        $flags = Read-ManifestBits -Reader $reader -Count 3 -FieldName ("resource {0} flags" -f $entryIndex)

        $md5 = New-Object byte[] 0
        if (($flags -band $resourceCustomFlag) -ne 0) {
            $md5 = New-Object byte[] 16
            for ($digestIndex = 0; $digestIndex -lt $md5.Length; $digestIndex++) {
                $md5[$digestIndex] = [byte](Read-ManifestBits -Reader $reader -Count 8 -FieldName ("resource {0} digest" -f $entryIndex))
            }
        }

        $extraInfoPresent = (Read-ManifestBits -Reader $reader -Count 1 -FieldName ("resource {0} extra-info marker" -f $entryIndex)) -ne 0
        $extraInfo = New-Object byte[] 0
        if ($extraInfoPresent) {
            $extraInfo = New-Object byte[] 32
            for ($extraIndex = 0; $extraIndex -lt $extraInfo.Length; $extraIndex++) {
                $extraInfo[$extraIndex] = [byte](Read-ManifestBits -Reader $reader -Count 8 -FieldName ("resource {0} extra info" -f $entryIndex))
            }
        }

        $entries.Add([pscustomobject]@{
            TypeName = $typeName
            WireType = [byte]$wireType
            Index = [uint16]$index
            DownloadSize = [uint32]$downloadSize
            Flags = [byte]$flags
            Path = $resourcePath
            Md5 = $md5
            ExtraInfoPresent = $extraInfoPresent
            ExtraInfo = $extraInfo
        })
    }

    $consistencyPresent = Read-ManifestBits -Reader $reader -Count 1 -FieldName "consistency marker"
    if ($consistencyPresent -ne 0) {
        throw "resource manifest unexpectedly contains consistency data"
    }
    while (($reader.BitOffset % 8) -ne 0) {
        $padding = Read-ManifestBits -Reader $reader -Count 1 -FieldName "terminal padding"
        if ($padding -ne 0) {
            throw "resource manifest contains nonzero terminal padding"
        }
    }
    if ($reader.BitOffset -ne ($manifestBytes.Length * 8)) {
        throw "resource manifest contains unexpected trailing data"
    }

    return [pscustomobject]@{
        Bytes = $Payload
        SpawnCount = [uint32]$spawnCount
        Entries = $entries.ToArray()
        ResourceCount = [int]$entries.Count
        PayloadBytes = [int]$Payload.Length
        ListBytes = [int]$manifestBytes.Length
        ConsistencyPresent = $false
        Fragmented = $false
    }
}

function Assert-ManifestMatchesFixture {
    param(
        $Manifest,
        [object[]]$ExpectedEntries,
        [string]$Description
    )

    if ($Manifest.ResourceCount -ne $ExpectedEntries.Count) {
        throw ("{0} resource count mismatch: expected {1}, received {2}" -f
            $Description,
            $ExpectedEntries.Count,
            $Manifest.ResourceCount)
    }
    $ownedIndices = New-Object 'System.Collections.Generic.HashSet[string]' ([StringComparer]::Ordinal)
    $worldCount = 0
    for ($index = 0; $index -lt $ExpectedEntries.Count; $index++) {
        $actual = $Manifest.Entries[$index]
        $expected = $ExpectedEntries[$index]
        foreach ($field in @("TypeName", "Index", "DownloadSize", "Flags", "Path")) {
            if ([string]$actual.($field) -cne [string]$expected.($field)) {
                throw ("{0} entry {1} field {2} mismatch" -f $Description, $index, $field)
            }
        }
        $ownership = "{0}:{1}" -f $actual.TypeName, $actual.Index
        if (-not $ownedIndices.Add($ownership)) {
            throw ("{0} contains duplicate ownership {1}" -f $Description, $ownership)
        }
        if ($actual.TypeName -ceq "model" -and $actual.Index -eq 1) {
            if ($actual.Path -cne "maps/c0a0.bsp") {
                throw ("{0} world resource is not maps/c0a0.bsp" -f $Description)
            }
            $worldCount++
        }
        if (($actual.Flags -band $resourceCustomFlag) -eq 0 -and $actual.Md5.Length -ne 0) {
            throw ("{0} entry {1} has an unexpected digest" -f $Description, $index)
        }
        if (($actual.Flags -band $resourceCustomFlag) -ne 0) {
            if ($actual.Md5.Length -ne 16 -or
                @($actual.Md5 | Where-Object { $_ -ne 0 }).Count -ne 0) {
                throw ("{0} entry {1} custom digest differs from the deterministic fixture default" -f
                    $Description,
                    $index)
            }
        }
        if ($actual.ExtraInfoPresent -or $actual.ExtraInfo.Length -ne 0) {
            throw ("{0} entry {1} has unexpected extra metadata" -f $Description, $index)
        }
    }
    if ($worldCount -ne 1 -or $Manifest.ConsistencyPresent -or $Manifest.Fragmented) {
        throw ("{0} world/consistency/fragmentation invariants failed" -f $Description)
    }
}

function Get-ExactlyOneSummaryLine {
    param(
        [string]$Stdout,
        [string]$Prefix
    )

    $lines = @(
        $Stdout -split '\r?\n' |
            Where-Object { $_.IndexOf($Prefix, [StringComparison]::Ordinal) -ge 0 }
    )
    if ($lines.Count -ne 1) {
        throw ("stdout expected exactly one {0} line, found {1}" -f $Prefix, $lines.Count)
    }
    return $lines[0]
}

function Assert-SummaryFields {
    param(
        [string]$Line,
        [System.Collections.IDictionary]$Expected,
        [string]$Description
    )

    foreach ($field in $Expected.GetEnumerator()) {
        if (-not (Test-StableField -Line $Line -Name $field.Key -ExpectedValue ([string]$field.Value))) {
            throw ("{0} expected {1}={2}: {3}" -f
                $Description,
                $field.Key,
                $field.Value,
                $Line.Trim())
        }
    }
}

function Assert-SummaryBoolean {
    param(
        [string]$Line,
        [string]$Name,
        [bool]$Expected,
        [string]$Description
    )

    $text = Get-StableFieldValue -Line $Line -Name $Name
    $actual = $null
    if ($text -ceq "true" -or $text -ceq "1") {
        $actual = $true
    } elseif ($text -ceq "false" -or $text -ceq "0") {
        $actual = $false
    } else {
        throw ("{0} field {1} is not a canonical boolean: {2}" -f $Description, $Name, $text)
    }
    if ($actual -ne $Expected) {
        throw ("{0} expected {1}={2}: {3}" -f
            $Description,
            $Name,
            $Expected.ToString().ToLowerInvariant(),
            $Line.Trim())
    }
}

function Assert-ResourceNetchanSummary {
    param(
        [string]$Stdout,
        [ValidateSet("Positive", "Negative", "Oversized")]
        [string]$Mode
    )

    $line = Get-ExactlyOneSummaryLine -Stdout $Stdout -Prefix "goldsrc_netchan_summary:"
    $expectedReliableCount = if ($Mode -eq "Oversized") { 2 } else { 3 }
    $expectedResponsive = if ($Mode -eq "Positive") { "0" } else { "1" }
    Assert-SummaryFields -Line $line -Description "goldsrc_netchan_summary" -Expected ([ordered]@{
        enabled = "1"
        initialized = "1"
        netchan_state = "established"
        server_initial_sequence = "1"
        server_initial_ack = "0"
        fragment_present = "0"
        payload_transform = "pass"
        first_payload = "svc_nop"
        reliable_queued = [string]$expectedReliableCount
        reliable_sent = [string]$expectedReliableCount
        reliable_acked = [string]$expectedReliableCount
        reliable_pending_bytes = "0"
        session_count = "1"
        state = "connected"
        put_in_server = "0"
        spawned = "0"
        active = "0"
        server_still_responsive = $expectedResponsive
        clean_shutdown = "1"
    })
    $resent = Get-StableUnsignedField -Line $line -Name "reliable_resent"
    $mismatch = Get-StableUnsignedField -Line $line -Name "reliable_ack_mismatch"
    if ($Mode -eq "Negative") {
        if ($resent -lt 2 -or $mismatch -lt 1) {
            throw "negative netchan summary is missing manifest retransmission/ACK-mismatch evidence"
        }
    } elseif ($resent -ne 0 -or $mismatch -ne 0) {
        throw "non-negative netchan summary unexpectedly contains retransmission evidence"
    }
}

function Assert-ResourceServerInfoSummary {
    param(
        [string]$Stdout,
        $DecodedServerInfo,
        [string]$ExpectedClientDllMd5,
        [ValidateSet("Positive", "Negative", "Oversized")]
        [string]$Mode
    )

    $line = Get-ExactlyOneSummaryLine -Stdout $Stdout -Prefix "goldsrc_serverinfo_summary:"
    $expectedPhase = if ($Mode -eq "Oversized") {
        "awaiting_resource_request"
    } else {
        "resource_manifest_acknowledged"
    }
    $expectedResponsive = if ($Mode -eq "Positive") { "0" } else { "1" }
    $expectedUnsupported = if ($Mode -eq "Negative") { "1" } else { "0" }
    $expectedWrongAck = if ($Mode -eq "Negative") { "1" } else { "0" }
    Assert-SummaryFields -Line $line -Description "goldsrc_serverinfo_summary" -Expected ([ordered]@{
        enabled = "1"
        negative_proof = "0"
        client_new_received = "1"
        client_new_delivered = "1"
        duplicate_new_deliveries = "0"
        malformed_stringcmd_rejected = "0"
        command_injection_rejected = "0"
        unsupported_command_rejected = $expectedUnsupported
        unsupported_opcode_rejected = "0"
        serverinfo_context_built = "1"
        serverinfo_generations = "1"
        serverinfo_queued = "1"
        serverinfo_sent = "1"
        serverinfo_resent = "0"
        serverinfo_acked = "1"
        serverinfo_first_carrier_sequence = "3"
        serverinfo_latest_carrier_sequence = "3"
        serverinfo_send_count = "1"
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
        wrong_reliable_ack_rejected = $expectedWrongAck
        future_ack_rejected = "0"
        duplicate_client_reliable_suppressed = "0"
        signon_phase = $expectedPhase
        session_count = "1"
        state = "connected"
        netchan_state = "established"
        reliable_pending_bytes = "0"
        put_in_server = "0"
        spawned = "0"
        active = "0"
        server_still_responsive = $expectedResponsive
        clean_shutdown = "1"
    })

    $spawnCount = Get-StableUnsignedField -Line $line -Name "serverinfo_spawn_count"
    $payloadBytes = Get-StableUnsignedField -Line $line -Name "serverinfo_payload_bytes"
    if ($spawnCount -ne [uint64]$DecodedServerInfo.SpawnCount -or
        $payloadBytes -ne [uint64]$DecodedServerInfo.PayloadBytes) {
        throw "serverinfo summary differs from the externally decoded serverinfo"
    }
    $checksum = Get-StableFieldValue -Line $line -Name "serverinfo_checksum"
    $md5 = Get-StableFieldValue -Line $line -Name "serverinfo_client_dll_md5"
    if ($checksum -notmatch '\A[0-9A-Fa-f]{8}\z' -or
        $checksum.ToLowerInvariant() -cne $DecodedServerInfo.ChecksumHex -or
        $md5 -notmatch '\A[0-9A-Fa-f]{32}\z' -or
        $md5.ToLowerInvariant() -cne $DecodedServerInfo.ClientDllMd5Hex -or
        $md5.ToLowerInvariant() -cne $ExpectedClientDllMd5.ToLowerInvariant()) {
        throw "serverinfo summary checksum/client identity differs from external/runtime evidence"
    }
}

function Assert-ResourceManifestSummary {
    param(
        [string]$Stdout,
        [ValidateSet("Positive", "Negative", "Oversized")]
        [string]$Mode,
        [int]$ExpectedEntryCount,
        [int]$ExpectedPayloadBytes
    )

    $line = Get-ExactlyOneSummaryLine -Stdout $Stdout -Prefix "goldsrc_resource_manifest_summary:"
    $negativeValue = if ($Mode -eq "Positive") { "0" } else { "1" }
    $requestReceived = if ($Mode -eq "Negative") { 2 } elseif ($Mode -eq "Positive") { 1 } else { 0 }
    $requestDelivered = if ($Mode -eq "Oversized") { 0 } else { 1 }
    $duplicates = if ($Mode -eq "Negative") { 1 } else { 0 }
    $preparationAttempts = 1
    $cachedOutcomeReuses = if ($Mode -eq "Oversized") { 1 } else { 0 }
    $contextBuilt = if ($Mode -eq "Oversized") { 0 } else { 1 }
    $generationCount = if ($Mode -eq "Oversized") { 0 } else { 1 }
    $queuedCount = if ($Mode -eq "Oversized") { 0 } else { 1 }
    $sentCount = if ($Mode -eq "Oversized") { 0 } else { 1 }
    $resentCount = if ($Mode -eq "Negative") { 2 } else { 0 }
    $ackedCount = if ($Mode -eq "Oversized") { 0 } else { 1 }
    $firstCarrier = if ($Mode -eq "Oversized") { 0 } else { 5 }
    $latestCarrier = if ($Mode -eq "Negative") { 9 } elseif ($Mode -eq "Positive") { 5 } else { 0 }
    $sendCount = if ($Mode -eq "Negative") { 3 } elseif ($Mode -eq "Positive") { 1 } else { 0 }
    $expectedPhase = if ($Mode -eq "Oversized") {
        "awaiting_resource_request"
    } else {
        "resource_manifest_acknowledged"
    }

    Assert-SummaryFields -Line $line -Description "goldsrc_resource_manifest_summary" -Expected ([ordered]@{
        enabled = "1"
        negative_proof = $negativeValue
        resource_request_received = [string]$requestReceived
        resource_request_deliveries = [string]$requestDelivered
        duplicate_resource_requests_suppressed = [string]$duplicates
        resource_manifest_preparation_attempts = [string]$preparationAttempts
        resource_manifest_cached_outcome_reuses = [string]$cachedOutcomeReuses
        resource_manifest_context_built = [string]$contextBuilt
        resource_manifest_generations = [string]$generationCount
        resource_manifest_queued = [string]$queuedCount
        resource_manifest_sent = [string]$sentCount
        resource_manifest_resent = [string]$resentCount
        resource_manifest_acked = [string]$ackedCount
        resource_manifest_entry_count = [string]$ExpectedEntryCount
        resource_manifest_payload_bytes = [string]$ExpectedPayloadBytes
        resource_manifest_first_carrier_sequence = [string]$firstCarrier
        resource_manifest_latest_carrier_sequence = [string]$latestCarrier
        resource_manifest_send_count = [string]$sendCount
        signon_phase = $expectedPhase
        session_count = "1"
        lifecycle_state = "connected"
        netchan_state = "established"
        reliable_pending_bytes = "0"
        put_in_server = "0"
        spawned = "0"
        active = "0"
        clean_shutdown = "1"
    })

    $negativeMode = $Mode -eq "Negative"
    $oversizedMode = $Mode -eq "Oversized"
    Assert-SummaryBoolean -Line $line -Name "resource_manifest_retransmitted" -Expected $negativeMode -Description "goldsrc_resource_manifest_summary"
    Assert-SummaryBoolean -Line $line -Name "duplicate_request_suppressed" -Expected $negativeMode -Description "goldsrc_resource_manifest_summary"
    Assert-SummaryBoolean -Line $line -Name "oversized_manifest_not_truncated" -Expected $oversizedMode -Description "goldsrc_resource_manifest_summary"
    Assert-SummaryBoolean -Line $line -Name "requires_fragmentation_reported" -Expected $oversizedMode -Description "goldsrc_resource_manifest_summary"
    Assert-SummaryBoolean -Line $line -Name "final_manifest_ack" -Expected (-not $oversizedMode) -Description "goldsrc_resource_manifest_summary"
    Assert-SummaryBoolean -Line $line -Name "wrong_reliable_ack_rejected" -Expected $negativeMode -Description "goldsrc_resource_manifest_summary"
    Assert-SummaryBoolean -Line $line -Name "server_still_responsive" -Expected ($Mode -ne "Positive") -Description "goldsrc_resource_manifest_summary"
}

function Reserve-LoopbackUdpPort {
    $reservation = $null
    try {
        $reservation = New-Object System.Net.Sockets.UdpClient(0)
        return ([System.Net.IPEndPoint]$reservation.Client.LocalEndPoint).Port
    }
    finally {
        if ($null -ne $reservation) {
            $reservation.Close()
            $reservation.Dispose()
        }
    }
}

function Invoke-HandshakeThroughServerInfo {
    param(
        [System.Net.Sockets.UdpClient]$Client,
        [System.Diagnostics.Process]$ServerProcess,
        $OutputCapture,
        [DateTime]$Deadline,
        [System.Net.IPEndPoint]$ServerEndpoint,
        [string]$ExpectedClientDllMd5
    )

    $challengeText = "getchallenge steam" + [char]10
    $challengeRequest = New-ConnectionlessDatagram -Text $challengeText -Tail ([byte[]](0x00))
    Send-ExactUdpDatagram -Client $Client -Packet $challengeRequest -Description "challenge request"
    $receiveArguments = @{
        Client = $Client
        ServerProcess = $ServerProcess
        OutputCapture = $OutputCapture
        Deadline = $Deadline
        ExpectedRemoteEndpoint = $ServerEndpoint
        Description = "challenge response"
    }
    $challengeDatagram = Receive-UdpDatagram @receiveArguments
    $challengeResponse = [byte[]]$challengeDatagram.Bytes
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
    [int]$challengeValue = 0
    if (-not $challengeMatch.Success -or
        -not [int]::TryParse(
            $challengeMatch.Groups["challenge"].Value,
            [Globalization.NumberStyles]::None,
            [Globalization.CultureInfo]::InvariantCulture,
            [ref]$challengeValue)) {
        throw "malformed challenge response"
    }

    $clientPort = ([System.Net.IPEndPoint]$Client.Client.LocalEndPoint).Port
    $protocolInfo = '\prot\3\unique\-1\raw\steam\cdkey\00000000000000000000000000000000'
    $userInfo = '\name\resource_manifest_probe\model\gordon'
    $connectLine = ('connect 48 {0} "{1}" "{2}"' -f
        $challengeValue,
        $protocolInfo,
        $userInfo) + [char]10
    $connectRequest = New-ConnectionlessDatagram -Text $connectLine -Tail ([byte[]]@(0x00, 0x01, 0x7F, 0x80, 0xFF))
    Send-ExactUdpDatagram -Client $Client -Packet $connectRequest -Description "connect request"
    $receiveArguments.Description = "connect accept response"
    $acceptDatagram = Receive-UdpDatagram @receiveArguments
    $expectedAcceptResponse = New-ConnectionlessDatagram -Text ('B 1 "127.0.0.1:{0}" 0 5971' -f $clientPort) -Tail ([byte[]](0x00))
    Assert-ExactBytes -Actual $acceptDatagram.Bytes -Expected $expectedAcceptResponse -Description "connect accept response"

    $receiveArguments.Description = "initial reliable netchan packet"
    $serverOneDatagram = Receive-UdpDatagram @receiveArguments
    $serverOne = Read-SequencedDatagram -Packet $serverOneDatagram.Bytes -Description "initial reliable netchan packet"
    Assert-SequencedHeader -Decoded $serverOne -ExpectedSequence 1 -ExpectedAcknowledgement 0 -ExpectedReliableToggle $true -ExpectedReliableAcknowledgementToggle $false -Description "initial reliable netchan packet"
    Assert-NopPayload -Payload $serverOne.Payload -Description "initial svc_nop payload"

    $nopPayload = New-NopPayload
    $transportAck = New-SequencedDatagram -Sequence 1 -Acknowledgement 1 -ReliableAcknowledgementToggle -Payload $nopPayload
    Send-ExactUdpDatagram -Client $Client -Packet $transportAck -Description "transport reliable acknowledgement"
    $nopArguments = @{
        Client = $Client
        ServerProcess = $ServerProcess
        OutputCapture = $OutputCapture
        Deadline = $Deadline
        ServerEndpoint = $ServerEndpoint
        ExpectedServerSequence = [uint32]2
        ExpectedClientSequence = [uint32]1
        ExpectedServerReliableToggle = $false
        ExpectedClientReliableToggle = $false
        Description = "established netchan acknowledgement"
    }
    [void](Receive-AndAssertNopAck @nopArguments)

    $newPayload = New-StringCommandPayload -Command "new"
    $clientNew = New-SequencedDatagram -Sequence 2 -Acknowledgement 2 -ReliableToggle -ReliableAcknowledgementToggle -Payload $newPayload
    Send-ExactUdpDatagram -Client $Client -Packet $clientNew -Description "reliable client new command"
    $receiveArguments.Description = "reliable serverinfo packet"
    $serverInfoDatagram = Receive-UdpDatagram @receiveArguments
    $serverInfoPacket = Read-SequencedDatagram -Packet $serverInfoDatagram.Bytes -Description "reliable serverinfo packet"
    Assert-SequencedHeader -Decoded $serverInfoPacket -ExpectedSequence 3 -ExpectedAcknowledgement 2 -ExpectedReliableToggle $true -ExpectedReliableAcknowledgementToggle $true -Description "reliable serverinfo packet"
    $decodedServerInfo = Read-ServerInfoPayload -Payload $serverInfoPacket.Payload
    Assert-ServerInfoRuntimeExpectations -ServerInfo $decodedServerInfo -ExpectedClientDllMd5 $ExpectedClientDllMd5

    $serverInfoAck = New-SequencedDatagram -Sequence 3 -Acknowledgement 3 -Payload $nopPayload
    Send-ExactUdpDatagram -Client $Client -Packet $serverInfoAck -Description "serverinfo reliable acknowledgement"
    $nopArguments.ExpectedServerSequence = [uint32]4
    $nopArguments.ExpectedClientSequence = [uint32]3
    $nopArguments.ExpectedServerReliableToggle = $false
    $nopArguments.ExpectedClientReliableToggle = $true
    $nopArguments.Description = "serverinfo acknowledgement response"
    [void](Receive-AndAssertNopAck @nopArguments)

    return [pscustomobject]@{
        Challenge = $challengeValue
        ClientPort = $clientPort
        ServerInfo = $decodedServerInfo
        NopPayload = $nopPayload
        NextClientSequence = [uint32]4
        LatestServerSequence = [uint32]4
    }
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
        [object[]]$ExpectedEntries
    )

    $sendResourcesPayload = New-StringCommandPayload -Command "sendres"
    $sendResourcesPacket = New-SequencedDatagram -Sequence 4 -Acknowledgement 4 -ReliableToggle -Payload $sendResourcesPayload
    Send-ExactUdpDatagram -Client $Client -Packet $sendResourcesPacket -Description "reliable client sendres request"

    if ($Mode -eq "Oversized") {
        $nopArguments = @{
            Client = $Client
            ServerProcess = $ServerProcess
            OutputCapture = $OutputCapture
            Deadline = $Deadline
            ServerEndpoint = $ServerEndpoint
            ExpectedServerSequence = [uint32]5
            ExpectedClientSequence = [uint32]4
            ExpectedServerReliableToggle = $false
            ExpectedClientReliableToggle = $false
            Description = "oversized manifest atomic ordinary response"
        }
        [void](Receive-AndAssertNopAck @nopArguments)
        Wait-ForStdoutToken -Process $ServerProcess -OutputCapture $OutputCapture -StdoutPath $StdoutPath -Deadline $Deadline -Token "goldsrc_resource_manifest_not_queued: reason=requires_fragmentation" -Description "typed requires_fragmentation diagnostic"
        Wait-ForStdoutToken -Process $ServerProcess -OutputCapture $OutputCapture -StdoutPath $StdoutPath -Deadline $Deadline -Token "source=authoritative-runtime" -Description "authoritative runtime manifest diagnostic"

        $repeatedSendResources = New-SequencedDatagram -Sequence 5 -Acknowledgement 5 -ReliableToggle -Payload $sendResourcesPayload
        Send-ExactUdpDatagram -Client $Client -Packet $repeatedSendResources -Description "repeated oversized reliable sendres request"
        $nopArguments.ExpectedServerSequence = [uint32]6
        $nopArguments.ExpectedClientSequence = [uint32]5
        $nopArguments.ExpectedServerReliableToggle = $false
        $nopArguments.ExpectedClientReliableToggle = $true
        $nopArguments.Description = "cached oversized manifest outcome response"
        [void](Receive-AndAssertNopAck @nopArguments)
        return [pscustomobject]@{
            Manifest = $null
            RetransmittedIdentical = $false
            FinalManifestAck = $false
            ServerResponsive = $true
        }
    }

    $receiveArguments = @{
        Client = $Client
        ServerProcess = $ServerProcess
        OutputCapture = $OutputCapture
        Deadline = $Deadline
        ExpectedRemoteEndpoint = $ServerEndpoint
        Description = "reliable resource manifest packet"
    }
    $manifestDatagram = Receive-UdpDatagram @receiveArguments
    $manifestPacket = Read-SequencedDatagram -Packet $manifestDatagram.Bytes -Description "reliable resource manifest packet"
    Assert-SequencedHeader -Decoded $manifestPacket -ExpectedSequence 5 -ExpectedAcknowledgement 4 -ExpectedReliableToggle $true -ExpectedReliableAcknowledgementToggle $false -Description "reliable resource manifest packet"
    $decodedManifest = Read-ResourceManifestPayload -Payload $manifestPacket.Payload -ExpectedSpawnCount $Handshake.ServerInfo.SpawnCount
    Assert-ManifestMatchesFixture -Manifest $decodedManifest -ExpectedEntries $ExpectedEntries -Description "resource manifest"

    if ($Mode -eq "Positive") {
        $manifestAck = New-SequencedDatagram -Sequence 5 -Acknowledgement 5 -ReliableAcknowledgementToggle -Payload $Handshake.NopPayload
        Send-ExactUdpDatagram -Client $Client -Packet $manifestAck -Description "resource manifest reliable acknowledgement"
        $nopArguments = @{
            Client = $Client
            ServerProcess = $ServerProcess
            OutputCapture = $OutputCapture
            Deadline = $Deadline
            ServerEndpoint = $ServerEndpoint
            ExpectedServerSequence = [uint32]6
            ExpectedClientSequence = [uint32]5
            ExpectedServerReliableToggle = $false
            ExpectedClientReliableToggle = $false
            Description = "resource manifest acknowledgement response"
        }
        [void](Receive-AndAssertNopAck @nopArguments)
        return [pscustomobject]@{
            Manifest = $decodedManifest
            RetransmittedIdentical = $false
            FinalManifestAck = $true
            ServerResponsive = $true
        }
    }

    # Correct reliable state but an ACK that does not cover carrier sequence 5.
    $nonCoveringAck = New-SequencedDatagram -Sequence 5 -Acknowledgement 4 -ReliableAcknowledgementToggle -Payload $Handshake.NopPayload
    Send-ExactUdpDatagram -Client $Client -Packet $nonCoveringAck -Description "non-covering resource manifest acknowledgement"
    $nopArguments = @{
        Client = $Client
        ServerProcess = $ServerProcess
        OutputCapture = $OutputCapture
        Deadline = $Deadline
        ServerEndpoint = $ServerEndpoint
        ExpectedServerSequence = [uint32]6
        ExpectedClientSequence = [uint32]5
        ExpectedServerReliableToggle = $false
        ExpectedClientReliableToggle = $false
        Description = "ordinary response after non-covering acknowledgement"
    }
    [void](Receive-AndAssertNopAck @nopArguments)

    # The covering ACK carries the wrong reliable state, forcing the existing
    # one-payload netchan to retransmit the still-frozen manifest.
    $wrongReliableAck = New-SequencedDatagram -Sequence 6 -Acknowledgement 6 -Payload $Handshake.NopPayload
    Send-ExactUdpDatagram -Client $Client -Packet $wrongReliableAck -Description "wrong resource manifest reliable acknowledgement"
    $receiveArguments.Description = "resource manifest retransmission"
    $resendDatagram = Receive-UdpDatagram @receiveArguments
    $resendPacket = Read-SequencedDatagram -Packet $resendDatagram.Bytes -Description "resource manifest retransmission"
    Assert-SequencedHeader -Decoded $resendPacket -ExpectedSequence 7 -ExpectedAcknowledgement 6 -ExpectedReliableToggle $true -ExpectedReliableAcknowledgementToggle $false -Description "resource manifest retransmission"
    Assert-ExactBytes -Actual $resendPacket.Payload -Expected $manifestPacket.Payload -Description "frozen resource manifest retransmission"
    $decodedResend = Read-ResourceManifestPayload -Payload $resendPacket.Payload -ExpectedSpawnCount $Handshake.ServerInfo.SpawnCount
    Assert-ManifestMatchesFixture -Manifest $decodedResend -ExpectedEntries $ExpectedEntries -Description "retransmitted resource manifest"

    $duplicateRequest = New-SequencedDatagram -Sequence 7 -Acknowledgement 7 -ReliableToggle -Payload $sendResourcesPayload
    Send-ExactUdpDatagram -Client $Client -Packet $duplicateRequest -Description "duplicate reliable sendres request"
    $nopArguments.ExpectedServerSequence = [uint32]8
    $nopArguments.ExpectedClientSequence = [uint32]7
    $nopArguments.ExpectedServerReliableToggle = $false
    $nopArguments.ExpectedClientReliableToggle = $true
    $nopArguments.Description = "duplicate sendres suppression response"
    [void](Receive-AndAssertNopAck @nopArguments)
    Wait-ForStdoutToken -Process $ServerProcess -OutputCapture $OutputCapture -StdoutPath $StdoutPath -Deadline $Deadline -Token "goldsrc_resource_request_suppressed:" -Description "duplicate sendres suppression diagnostic"

    $unsupportedPayload = New-StringCommandPayload -Command "status"
    $unsupportedRequest = New-SequencedDatagram -Sequence 8 -Acknowledgement 8 -ReliableToggle -Payload $unsupportedPayload
    Send-ExactUdpDatagram -Client $Client -Packet $unsupportedRequest -Description "unsupported reliable signon request"
    $receiveArguments.Description = "resource manifest retransmission after unsupported request"
    $secondResendDatagram = Receive-UdpDatagram @receiveArguments
    $secondResendPacket = Read-SequencedDatagram -Packet $secondResendDatagram.Bytes -Description "resource manifest retransmission after unsupported request"
    Assert-SequencedHeader -Decoded $secondResendPacket -ExpectedSequence 9 -ExpectedAcknowledgement 8 -ExpectedReliableToggle $true -ExpectedReliableAcknowledgementToggle $false -Description "resource manifest retransmission after unsupported request"
    Assert-ExactBytes -Actual $secondResendPacket.Payload -Expected $manifestPacket.Payload -Description "resource manifest after unsupported request"
    $decodedSecondResend = Read-ResourceManifestPayload -Payload $secondResendPacket.Payload -ExpectedSpawnCount $Handshake.ServerInfo.SpawnCount
    Assert-ManifestMatchesFixture -Manifest $decodedSecondResend -ExpectedEntries $ExpectedEntries -Description "resource manifest after unsupported request"
    Wait-ForStdoutToken -Process $ServerProcess -OutputCapture $OutputCapture -StdoutPath $StdoutPath -Deadline $Deadline -Token "goldsrc_client_signon_rejected: reason=unsupported_command" -Description "unsupported signon request rejection"

    [void](Update-ProcessOutputCapture -State $OutputCapture)
    $beforeCorrectAck = Get-SharedFileText -Path $StdoutPath
    if ($beforeCorrectAck.IndexOf(
            "goldsrc_resource_manifest_acknowledged:",
            [StringComparison]::Ordinal) -ge 0) {
        throw "resource manifest advanced before the correct reliable acknowledgement"
    }

    $correctAck = New-SequencedDatagram -Sequence 9 -Acknowledgement 9 -ReliableAcknowledgementToggle -Payload $Handshake.NopPayload
    Send-ExactUdpDatagram -Client $Client -Packet $correctAck -Description "correct resource manifest reliable acknowledgement"
    $nopArguments.ExpectedServerSequence = [uint32]10
    $nopArguments.ExpectedClientSequence = [uint32]9
    $nopArguments.ExpectedServerReliableToggle = $false
    $nopArguments.ExpectedClientReliableToggle = $false
    $nopArguments.Description = "responsive final manifest acknowledgement response"
    [void](Receive-AndAssertNopAck @nopArguments)

    return [pscustomobject]@{
        Manifest = $decodedManifest
        RetransmittedIdentical = $true
        FinalManifestAck = $true
        ServerResponsive = $true
    }
}

function Invoke-ResourceManifestHostRun {
    param(
        [ValidateSet("Positive", "Negative", "Oversized")]
        [string]$Mode,
        [string]$ResolvedExecutablePath,
        [string]$ResolvedGameDir,
        [string]$FixturePath,
        [object[]]$ExpectedEntries,
        [string]$ExpectedClientDllMd5,
        [string]$RepositoryRoot,
        [string]$Address,
        [int]$RequestedPort,
        [int]$RunTimeoutSeconds,
        [switch]$SuppressServerOutput
    )

    $selectedPort = if ($RequestedPort -eq 0) {
        Reserve-LoopbackUdpPort
    } else {
        $RequestedPort
    }
    $handshakeTimeoutMilliseconds = [Math]::Min(
        60000,
        [Math]::Max(1000, ($RunTimeoutSeconds * 1000))
    )
    $arguments = @(
        "--gamedir", $ResolvedGameDir,
        "--dedicated",
        "--deathmatch", "1",
        "--maxclients", "1",
        "--map", "c0a0",
        "--frames", "1",
        "--log-to-file", "0",
        "--log-summary-file", "0",
        "--ip", $Address,
        "--port", ([string]$selectedPort),
        "--goldsrc-resource-manifest",
        "--goldsrc-handshake-timeout-ms", ([string]$handshakeTimeoutMilliseconds)
    )
    if (-not [string]::IsNullOrWhiteSpace($FixturePath)) {
        $arguments += @("--goldsrc-resource-manifest-fixture", $FixturePath)
    }
    if ($Mode -ne "Positive") {
        $arguments += "--goldsrc-resource-manifest-negative-proof"
    }
    $argumentLine = (($arguments | ForEach-Object {
        ConvertTo-WindowsCommandLineArgument -Value ([string]$_)
    }) -join " ")

    $tempPrefix = "hlhost_goldsrc_resource_manifest_{0}_{1}" -f
        $Mode.ToLowerInvariant(),
        ([Guid]::NewGuid().ToString("N"))
    $stdoutPath = Join-Path ([System.IO.Path]::GetTempPath()) ($tempPrefix + ".stdout.log")
    $stderrPath = Join-Path ([System.IO.Path]::GetTempPath()) ($tempPrefix + ".stderr.log")
    $deadline = [DateTime]::UtcNow.AddSeconds($RunTimeoutSeconds)
    $serverProcess = $null
    $serverProcessId = 0
    $processStarted = $false
    $outputCapture = $null
    $probeClient = $null
    $capturedStdout = ""
    $capturedStderr = ""
    $failure = $null
    $cleanupFailure = $null
    $clientPort = 0
    $challengeValue = 0
    $portOwnershipProof = "not_checked"
    $childProcessProof = "not_checked"
    $handshake = $null
    $exchange = $null

    try {
        $startInfo = New-Object System.Diagnostics.ProcessStartInfo
        $startInfo.FileName = $ResolvedExecutablePath
        $startInfo.Arguments = $argumentLine
        $startInfo.WorkingDirectory = $RepositoryRoot
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
        $outputCapture = New-ProcessOutputCapture -Process $serverProcess -StdoutPath $stdoutPath -StderrPath $stderrPath

        $readinessArguments = @{
            Process = $serverProcess
            OutputCapture = $outputCapture
            StdoutPath = $stdoutPath
            Deadline = $deadline
            ExpectedAddress = $Address
            ExpectedPort = $selectedPort
        }
        [void](Wait-ForServerReadiness @readinessArguments)
        $portOwnershipProof = Wait-ForUdpPortOwnership -Process $serverProcess -Deadline $deadline -ExpectedAddress $Address -ExpectedPort $selectedPort -ExpectedProcessId $serverProcessId

        $probeClient = New-Object System.Net.Sockets.UdpClient(
            [System.Net.Sockets.AddressFamily]::InterNetwork
        )
        $probeClient.Client.Bind((New-Object System.Net.IPEndPoint([System.Net.IPAddress]::Loopback, 0)))
        $clientPort = ([System.Net.IPEndPoint]$probeClient.Client.LocalEndPoint).Port
        $parsedAddress = [System.Net.IPAddress]::Parse($Address)
        $serverEndpoint = New-Object System.Net.IPEndPoint($parsedAddress, $selectedPort)
        $probeClient.Connect($serverEndpoint)

        $handshake = Invoke-HandshakeThroughServerInfo -Client $probeClient -ServerProcess $serverProcess -OutputCapture $outputCapture -Deadline $deadline -ServerEndpoint $serverEndpoint -ExpectedClientDllMd5 $ExpectedClientDllMd5
        $challengeValue = $handshake.Challenge
        $exchange = Invoke-ResourceManifestExchange -Mode $Mode -Client $probeClient -ServerProcess $serverProcess -OutputCapture $outputCapture -StdoutPath $stdoutPath -Deadline $deadline -ServerEndpoint $serverEndpoint -Handshake $handshake -ExpectedEntries $ExpectedEntries

        Wait-ForCleanServerExit -Process $serverProcess -OutputCapture $outputCapture -Deadline $deadline
        if ($serverProcess.ExitCode -ne 0) {
            throw ("hlhost exited with code {0}" -f $serverProcess.ExitCode)
        }
        if (Test-ProcessIdAlive -ProcessId $serverProcessId) {
            throw ("hlhost process ID {0} is still alive after clean exit" -f $serverProcessId)
        }
        $childProcessProof = Assert-NoDirectChildProcessLeak -ParentProcessId $serverProcessId
        $capturedStdout = Get-SharedFileText -Path $stdoutPath
        $capturedStderr = Get-SharedFileText -Path $stderrPath

        Assert-SessionSummary -Stdout $capturedStdout -ExpectedEndpoint ("127.0.0.1:{0}" -f $clientPort) -ExpectedChallenge $challengeValue -ExpectedChannelIdentifier $clientPort
        $expectedDatagrams = if ($Mode -eq "Positive") { 7 } elseif ($Mode -eq "Negative") { 11 } else { 7 }
        Assert-UdpSummary -Stdout $capturedStdout -ExpectedDatagrams $expectedDatagrams
        Assert-ResourceNetchanSummary -Stdout $capturedStdout -Mode $Mode
        Assert-ResourceServerInfoSummary -Stdout $capturedStdout -DecodedServerInfo $handshake.ServerInfo -ExpectedClientDllMd5 $ExpectedClientDllMd5 -Mode $Mode
        if ($Mode -eq "Oversized") {
            Assert-ResourceManifestSummary -Stdout $capturedStdout -Mode $Mode -ExpectedEntryCount 0 -ExpectedPayloadBytes 0
        } else {
            Assert-ResourceManifestSummary -Stdout $capturedStdout -Mode $Mode -ExpectedEntryCount $ExpectedEntries.Count -ExpectedPayloadBytes $exchange.Manifest.PayloadBytes
        }
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
                    if ($serverProcessId -gt 0) {
                        Stop-Process -Id $serverProcessId -Force -ErrorAction Stop
                    } else {
                        $serverProcess.Kill()
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
                if ($null -eq $cleanupFailure) {
                    $cleanupFailure = "failed to drain hlhost output: $($_.Exception.Message)"
                }
            }
        }
        if ($processStarted -and $serverProcessId -gt 0 -and
            (Test-ProcessIdAlive -ProcessId $serverProcessId)) {
            $cleanupFailure = "hlhost process remained alive after cleanup"
        }
        $latestStdout = Get-SharedFileText -Path $stdoutPath
        $latestStderr = Get-SharedFileText -Path $stderrPath
        if ($latestStdout.Length -gt 0) {
            $capturedStdout = $latestStdout
        }
        if ($latestStderr.Length -gt 0) {
            $capturedStderr = $latestStderr
        }
        if (-not $SuppressServerOutput) {
            if (-not [string]::IsNullOrWhiteSpace($capturedStdout)) {
                Write-Host $capturedStdout.TrimEnd([char[]]@([char]13, [char]10))
            }
            if (-not [string]::IsNullOrWhiteSpace($capturedStderr)) {
                Write-Host $capturedStderr.TrimEnd([char[]]@([char]13, [char]10))
            }
        }
        foreach ($tempPath in @($stdoutPath, $stderrPath)) {
            if (Test-Path -LiteralPath $tempPath -PathType Leaf) {
                Remove-Item -LiteralPath $tempPath -Force -ErrorAction SilentlyContinue
            }
            if (Test-Path -LiteralPath $tempPath -PathType Leaf) {
                $cleanupFailure = "temporary proof output was not removed"
            }
        }
        if ($null -ne $serverProcess) {
            $serverProcess.Dispose()
        }
    }

    if ($null -ne $cleanupFailure -and $null -eq $failure) {
        $failure = New-Object System.Management.Automation.ErrorRecord(
            (New-Object System.InvalidOperationException($cleanupFailure)),
            "GoldsrcResourceManifestProbeCleanupFailure",
            [System.Management.Automation.ErrorCategory]::CloseError,
            $serverProcess
        )
    }
    if ($null -ne $failure) {
        throw $failure
    }

    return [pscustomobject]@{
        Mode = $Mode
        Port = $selectedPort
        ClientPort = $clientPort
        UdpOwner = $portOwnershipProof
        ChildProcessCheck = $childProcessProof
        Handshake = $handshake
        Exchange = $exchange
        Stdout = $capturedStdout
    }
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
$mapPath = Join-Path $resolvedGameDir "maps/c0a0.bsp"
if (-not (Test-Path -LiteralPath $mapPath -PathType Leaf)) {
    throw ("proof map not found: {0}" -f $mapPath)
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

$parsedBindAddress = $null
if (-not [System.Net.IPAddress]::TryParse($BindAddress, [ref]$parsedBindAddress) -or
    $parsedBindAddress.AddressFamily -ne [System.Net.Sockets.AddressFamily]::InterNetwork -or
    $parsedBindAddress.ToString() -cne "127.0.0.1") {
    throw ("BindAddress must be exactly 127.0.0.1 for this proof: {0}" -f $BindAddress)
}

$expectedEntries = @(Read-ResourceManifestFixture -Path $resolvedManifestFixture)
$repositorySnapshotBefore = Get-RepositoryFileSnapshot -RepositoryRoot $repoRoot
$mainResult = $null
$oversizedResult = $null
$failure = $null

try {
    if ($NegativeProof) {
        $mainResult = Invoke-ResourceManifestHostRun -Mode "Negative" -ResolvedExecutablePath $resolvedExecutablePath -ResolvedGameDir $resolvedGameDir -FixturePath $resolvedManifestFixture -ExpectedEntries $expectedEntries -ExpectedClientDllMd5 $expectedClientDllMd5 -RepositoryRoot $repoRoot -Address $BindAddress -RequestedPort $Port -RunTimeoutSeconds $TimeoutSeconds -SuppressServerOutput:$SkipServerOutput

        $oversizedResult = Invoke-ResourceManifestHostRun -Mode "Oversized" -ResolvedExecutablePath $resolvedExecutablePath -ResolvedGameDir $resolvedGameDir -FixturePath "" -ExpectedEntries @() -ExpectedClientDllMd5 $expectedClientDllMd5 -RepositoryRoot $repoRoot -Address $BindAddress -RequestedPort 0 -RunTimeoutSeconds $TimeoutSeconds -SuppressServerOutput:$SkipServerOutput
    } else {
        $mainResult = Invoke-ResourceManifestHostRun -Mode "Positive" -ResolvedExecutablePath $resolvedExecutablePath -ResolvedGameDir $resolvedGameDir -FixturePath $resolvedManifestFixture -ExpectedEntries $expectedEntries -ExpectedClientDllMd5 $expectedClientDllMd5 -RepositoryRoot $repoRoot -Address $BindAddress -RequestedPort $Port -RunTimeoutSeconds $TimeoutSeconds -SuppressServerOutput:$SkipServerOutput
    }
    Assert-NoRepositoryFileMutation -RepositoryRoot $repoRoot -Before $repositorySnapshotBefore
}
catch {
    $failure = $_
}

if ($null -ne $failure) {
    $failureMessage = $failure.Exception.Message -replace '[\r\n]+', ' '
    Write-Host ("goldsrc_resource_manifest_probe: result=fail reason={0}" -f $failureMessage)
    throw $failureMessage
}

Write-Host ("goldsrc_resource_manifest_probe: port={0},client_endpoint=127.0.0.1:{1}" -f
    $mainResult.Port,
    $mainResult.ClientPort)
Write-Host ("goldsrc_resource_manifest_probe: udp_owner={0},child_process_check={1},repo_files_mutated=0,temp_files_cleaned=1" -f
    $mainResult.UdpOwner,
    $mainResult.ChildProcessCheck)
if ($NegativeProof) {
    Write-Host "goldsrc_resource_manifest_proof_b: resource_manifest_retransmitted=true,retransmitted_manifest_identical=true,duplicate_request_suppressed=true,oversized_manifest_not_truncated=true,requires_fragmentation_reported=true,final_manifest_ack=true,resource_request_deliveries=1,resource_manifest_generations=1,session_count=1,put_in_server=0,spawned=0,active=0,server_still_responsive=true,clean_shutdown=1,proof_b=pass"
} else {
    Write-Host ("goldsrc_resource_manifest_proof_a: transport=udp,external_datagrams=true,protocol=48,connectionless_handshake=pass,netchan_state=established,serverinfo_decode=pass,serverinfo_acked=true,resource_request=sendres,resource_request_deliveries=1,resource_manifest_decode=pass,resource_count={0},resource_order=pass,world_resource=maps/c0a0.bsp,no_duplicate_indices=true,no_unexpected_file_payload=true,fragmentation_flag=false,lifecycle_state=connected,signon_phase=resource_manifest_acknowledged,resource_manifest_generations=1,resource_manifest_acked=1,reliable_pending_bytes=0,put_in_server=0,spawned=0,active=0,clean_shutdown=1,proof_a=pass" -f
        $mainResult.Exchange.Manifest.ResourceCount)
}
Write-Host "goldsrc_resource_manifest_probe: result=pass"
