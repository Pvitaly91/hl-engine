[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$ExecutablePath,

    [Parameter(Mandatory = $true)]
    [string]$GameDir,

    [string]$DeltaFixture = "",

    [string]$FragmentedDeltaFixture = "",

    [string]$ManifestFixture = "",

    [string]$BindAddress = "127.0.0.1",

    [ValidateRange(0, 65535)]
    [int]$Port = 0,

    [ValidateRange(1, 300)]
    [int]$TimeoutSeconds = 60,

    [switch]$NegativeProof,

    [switch]$PostResourceCommandProof,

    [switch]$PostResourceNegativeProof,

    [switch]$WorldBaselineProof,

    [switch]$FirstSnapshotProof,

    [single]$ExpectedZMaximum = 4096.0,

    [ValidateRange(0, 255)]
    [int]$ExpectedCdTrack = 0,

    [switch]$SkipServerOutput
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
if (Test-Path Variable:PSNativeCommandUseErrorActionPreference) {
    $PSNativeCommandUseErrorActionPreference = $false
}
if ($PostResourceNegativeProof -and
    (-not $NegativeProof -or -not $PostResourceCommandProof)) {
    throw "PostResourceNegativeProof requires NegativeProof and PostResourceCommandProof"
}
if ($WorldBaselineProof -and -not $PostResourceCommandProof) {
    throw "WorldBaselineProof requires PostResourceCommandProof"
}
if ($FirstSnapshotProof -and -not $WorldBaselineProof) {
    throw "FirstSnapshotProof requires WorldBaselineProof"
}

if ([string]::IsNullOrWhiteSpace($DeltaFixture)) {
    $DeltaFixture = Join-Path `
        $PSScriptRoot `
        "../src/tests/fixtures/goldsrc_delta_description_minimal.lst"
}
if ([string]::IsNullOrWhiteSpace($FragmentedDeltaFixture)) {
    $FragmentedDeltaFixture = Join-Path `
        $PSScriptRoot `
        "../src/tests/fixtures/goldsrc_delta_description_fragmented.lst"
}
if ([string]::IsNullOrWhiteSpace($ManifestFixture)) {
    $ManifestFixture = Join-Path `
        $PSScriptRoot `
        "../src/tests/fixtures/goldsrc_resource_manifest_minimal.tsv"
}

function Get-TopLevelProofFunctionDefinitions {
    param(
        [string]$Path,
        [string[]]$Names = @()
    )

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
        throw ("proof helper has {0} parser errors: {1}" -f
            @($errors).Count,
            $Path)
    }
    $definitions = @(
        $ast.EndBlock.Statements |
            Where-Object {
                $_ -is [System.Management.Automation.Language.FunctionDefinitionAst] -and
                ($Names.Count -eq 0 -or $Names -ccontains $_.Name)
            }
    )
    if ($definitions.Count -eq 0) {
        throw ("proof helper contains no selected reusable functions: {0}" -f $Path)
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
        -Path (Join-Path $PSScriptRoot "run_goldsrc_resource_manifest_proof.ps1") `
        -Names @(
            "Assert-CanonicalResourcePath",
            "Read-ResourceManifestFixture",
            "Read-ManifestBits",
            "Read-ManifestProtocolString",
            "Get-ResourceTypeName",
            "Read-ResourceManifestPayload",
            "Assert-ManifestMatchesFixture",
            "Get-ExactlyOneSummaryLine",
            "Assert-SummaryFields",
            "Assert-SummaryBoolean",
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
$canonicalDeltaTableOrder = @(
    "event_t",
    "weapon_data_t",
    "usercmd_t",
    "custom_entity_state_t",
    "entity_state_player_t",
    "entity_state_t",
    "clientdata_t"
)

function Initialize-GoldSrcMoveProofCodec {
    if ($null -ne ("GoldSrcMoveProofCodec" -as [type])) {
        return
    }
    Add-Type -TypeDefinition @'
using System;

public static class GoldSrcMoveProofCodec
{
    private static readonly byte[] MungeTable = {
        0x7A, 0x64, 0x05, 0xF1, 0x1B, 0x9B, 0xA0, 0xB5,
        0xCA, 0xED, 0x61, 0x0D, 0x4A, 0xDF, 0x8E, 0xC7
    };
    private static readonly uint[] CrcTable = BuildCrcTable();

    private static uint[] BuildCrcTable()
    {
        var table = new uint[256];
        for (uint index = 0; index < table.Length; ++index)
        {
            uint value = index;
            for (int bit = 0; bit < 8; ++bit)
                value = (value & 1u) != 0u
                    ? 0xEDB88320u ^ (value >> 1)
                    : value >> 1;
            table[index] = value;
        }
        return table;
    }

    private static void ProcessCrcByte(ref uint crc, byte value)
    {
        crc = CrcTable[(crc ^ value) & 0xFFu] ^ (crc >> 8);
    }

    private static byte CrcTableByte(int offset)
    {
        return (byte)((CrcTable[offset / 4] >> ((offset % 4) * 8)) & 0xFFu);
    }

    private static uint Swap(uint value)
    {
        return ((value & 0x000000FFu) << 24)
            | ((value & 0x0000FF00u) << 8)
            | ((value & 0x00FF0000u) >> 8)
            | ((value & 0xFF000000u) >> 24);
    }

    private static uint Load(byte[] bytes, int offset)
    {
        return (uint)bytes[offset]
            | ((uint)bytes[offset + 1] << 8)
            | ((uint)bytes[offset + 2] << 16)
            | ((uint)bytes[offset + 3] << 24);
    }

    private static void Store(byte[] bytes, int offset, uint value)
    {
        bytes[offset] = (byte)(value & 0xFFu);
        bytes[offset + 1] = (byte)((value >> 8) & 0xFFu);
        bytes[offset + 2] = (byte)((value >> 16) & 0xFFu);
        bytes[offset + 3] = (byte)((value >> 24) & 0xFFu);
    }

    private static byte Checksum(byte[] body, uint sequence)
    {
        uint crc = 0xFFFFFFFFu;
        int protectedSize = Math.Min(body.Length, 60);
        for (int index = 0; index < protectedSize; ++index)
            ProcessCrcByte(ref crc, body[index]);
        int seedOffset = (int)(sequence % 0x3FCu);
        for (int index = 0; index < 4; ++index)
            ProcessCrcByte(ref crc, CrcTableByte(seedOffset + index));
        return (byte)(~crc & 0xFFu);
    }

    private static byte[] Protect(byte[] body, uint sequence)
    {
        var result = (byte[])body.Clone();
        for (int group = 0; group < result.Length / 4; ++group)
        {
            int offset = group * 4;
            uint value = Swap(Load(result, offset) ^ sequence);
            var transformed = new byte[] {
                (byte)(value & 0xFFu),
                (byte)((value >> 8) & 0xFFu),
                (byte)((value >> 16) & 0xFFu),
                (byte)((value >> 24) & 0xFFu)
            };
            for (int index = 0; index < 4; ++index)
            {
                byte mix = (byte)(0xA5
                    | (index << index)
                    | index
                    | MungeTable[(group + index) & 0x0F]);
                transformed[index] ^= mix;
            }
            value = (uint)transformed[0]
                | ((uint)transformed[1] << 8)
                | ((uint)transformed[2] << 16)
                | ((uint)transformed[3] << 24);
            Store(result, offset, value ^ ~sequence);
        }
        return result;
    }

    public static byte[] Build(uint sequence, byte[] body)
    {
        if (body == null || body.Length > 255)
            throw new ArgumentOutOfRangeException("body");
        byte[] protectedBody = Protect(body, sequence);
        var payload = new byte[protectedBody.Length + 3];
        payload[0] = 2;
        payload[1] = (byte)body.Length;
        payload[2] = Checksum(body, sequence);
        Buffer.BlockCopy(protectedBody, 0, payload, 3, protectedBody.Length);
        return payload;
    }
}
'@
}

function New-ObservedGoldSrcMovePayload {
    param([uint32]$Sequence)

    # Semantic fixture: loss=0, backup=2, new=1; command deltas decode to
    # zero, lerp=100/msec=210, and inherited lerp=100/msec=32.
    [byte[]]$body = @(
        0x00, 0x02, 0x01,
        0x00,
        0x19, 0x20, 0x23, 0x0D,
        0x11, 0x00, 0x01
    )
    Initialize-GoldSrcMoveProofCodec
    return [GoldSrcMoveProofCodec]::Build($Sequence, $body)
}

function New-GoldSrcMovePayloadFromBody {
    param(
        [uint32]$Sequence,
        [byte[]]$Body
    )

    Initialize-GoldSrcMoveProofCodec
    return [GoldSrcMoveProofCodec]::Build($Sequence, $Body)
}

function New-DeltaBitReader {
    param([byte[]]$Bytes)

    return [pscustomobject]@{
        Bytes = $Bytes
        BitPosition = [int64]0
    }
}

function Read-DeltaBits {
    param(
        $Reader,
        [ValidateRange(0, 32)]
        [int]$Count,
        [string]$Description
    )

    if ($Count -eq 0) {
        return [uint64]0
    }
    $remaining = ([int64]$Reader.Bytes.Length * 8) - $Reader.BitPosition
    if ($remaining -lt $Count) {
        throw ("delta bundle is truncated before {0}" -f $Description)
    }
    [uint64]$value = 0
    for ($index = 0; $index -lt $Count; $index++) {
        $absoluteBit = $Reader.BitPosition + $index
        $byteIndex = [int]([Math]::Floor($absoluteBit / 8))
        $bitIndex = [int]($absoluteBit % 8)
        if (([int]$Reader.Bytes[$byteIndex] -band (1 -shl $bitIndex)) -ne 0) {
            $value = $value -bor ([uint64]1 -shl $index)
        }
    }
    $Reader.BitPosition += $Count
    return $value
}

function Align-DeltaBitReaderToByte {
    param(
        $Reader,
        [string]$Description
    )

    while (($Reader.BitPosition % 8) -ne 0) {
        if ((Read-DeltaBits `
                -Reader $Reader `
                -Count 1 `
                -Description "$Description padding") -ne 0) {
            throw "$Description contains non-zero padding"
        }
    }
}

function Read-FirstSnapshotPayload {
    param(
        [byte[]]$Payload,
        [uint32]$FrameId
    )

    $reader = New-DeltaBitReader -Bytes $Payload
    if ((Read-DeltaBits -Reader $reader -Count 8 `
            -Description "svc_time opcode") -ne 7) {
        throw "first snapshot does not begin with svc_time"
    }
    [byte[]]$timeBytes = @()
    for ($index = 0; $index -lt 4; $index++) {
        $timeBytes += [byte](Read-DeltaBits `
            -Reader $reader `
            -Count 8 `
            -Description "svc_time value")
    }
    [single]$serverTime = [BitConverter]::ToSingle($timeBytes, 0)
    if ([single]::IsNaN($serverTime) -or
        [single]::IsInfinity($serverTime) -or
        $serverTime -lt 0.0) {
        throw "first snapshot contains invalid server time"
    }
    if ((Read-DeltaBits -Reader $reader -Count 8 `
            -Description "svc_clientdata opcode") -ne 15) {
        throw "svc_clientdata does not follow svc_time"
    }
    if ((Read-DeltaBits -Reader $reader -Count 1 `
            -Description "clientdata previous-frame marker") -ne 0) {
        throw "first clientdata unexpectedly references a previous frame"
    }
    if ((Read-DeltaBits -Reader $reader -Count 3 `
            -Description "clientdata delta mask length") -ne 0) {
        throw "authoritative pre-spawn clientdata is not the zero baseline"
    }
    if ((Read-DeltaBits -Reader $reader -Count 1 `
            -Description "weapon-data continuation") -ne 0) {
        throw "pre-spawn first snapshot unexpectedly contains weapon data"
    }
    Align-DeltaBitReaderToByte `
        -Reader $reader `
        -Description "clientdata record"

    if ((Read-DeltaBits -Reader $reader -Count 8 `
            -Description "svc_packetentities opcode") -ne 40) {
        throw "svc_packetentities does not follow clientdata"
    }
    [int]$entityCount = Read-DeltaBits `
        -Reader $reader `
        -Count 16 `
        -Description "packet-entities count"
    if ($entityCount -le 0 -or $entityCount -gt 2048) {
        throw "first snapshot entity count is outside the bounded range"
    }

    $entities = New-Object 'System.Collections.Generic.List[int]'
    [int]$numberBase = 0
    for ($index = 0; $index -lt $entityCount; $index++) {
        $sequential = Read-DeltaBits `
            -Reader $reader `
            -Count 1 `
            -Description "entity sequential marker"
        if ($sequential -ne 0) {
            $entityNumber = $numberBase + 1
        } else {
            $absolute = Read-DeltaBits `
                -Reader $reader `
                -Count 1 `
                -Description "entity absolute marker"
            if ($absolute -ne 0) {
                $entityNumber = [int](Read-DeltaBits `
                    -Reader $reader `
                    -Count 11 `
                    -Description "absolute entity number")
            } else {
                $entityDelta = [int](Read-DeltaBits `
                    -Reader $reader `
                    -Count 6 `
                    -Description "relative entity number")
                if ($entityDelta -eq 0) {
                    throw "packet entities contains a zero entity delta"
                }
                $entityNumber = $numberBase + $entityDelta
            }
        }
        if ($entityNumber -le $numberBase -or $entityNumber -gt 2047) {
            throw "packet entities is not strictly ordered"
        }
        if ($entityNumber -le 1) {
            throw "first pre-spawn snapshot fabricated world/player state"
        }
        $numberBase = $entityNumber
        if ((Read-DeltaBits -Reader $reader -Count 1 `
                -Description "custom-entity marker") -ne 0) {
            throw "minimal first snapshot unexpectedly selected a custom entity table"
        }
        if ((Read-DeltaBits -Reader $reader -Count 1 `
                -Description "offset-baseline marker") -ne 0) {
            throw "first snapshot did not select the entity's established baseline"
        }
        if ((Read-DeltaBits -Reader $reader -Count 3 `
                -Description "entity delta mask length") -ne 0) {
            throw "unchanged first-snapshot entity differs from its established baseline"
        }
        $entities.Add($entityNumber)
    }
    if ((Read-DeltaBits -Reader $reader -Count 16 `
            -Description "packet-entities terminator") -ne 0) {
        throw "packet entities has no exact zero terminator"
    }
    Align-DeltaBitReaderToByte `
        -Reader $reader `
        -Description "packet entities"
    if ($reader.BitPosition -ne ([int64]$Payload.Length * 8)) {
        throw "first snapshot contains trailing application data"
    }
    for ($index = 0; $index -lt $entities.Count; $index++) {
        if ($index -gt 0 -and
            $entities[$index] -le $entities[$index - 1]) {
            throw "first snapshot entity selection is not strictly ordered"
        }
    }
    return [pscustomobject]@{
        FrameId = $FrameId
        ServerTime = $serverTime
        EntityCount = $entityCount
        EntityNumbers = $entities.ToArray()
        ClientDataReceived = $true
        WeaponDataRequired = $false
        PacketEntitiesReceived = $true
    }
}

function Read-DeltaString {
    param(
        $Reader,
        [string]$Description
    )

    $bytes = New-Object 'System.Collections.Generic.List[byte]'
    for ($index = 0; $index -le $maximumDeltaNameBytes; $index++) {
        $value = [byte](Read-DeltaBits `
            -Reader $Reader `
            -Count 8 `
            -Description $Description)
        if ($value -eq 0) {
            if ($bytes.Count -eq 0) {
                throw ("{0} must not be empty" -f $Description)
            }
            return [System.Text.Encoding]::ASCII.GetString($bytes.ToArray())
        }
        if ($index -eq $maximumDeltaNameBytes) {
            throw ("{0} exceeds the bounded name length" -f $Description)
        }
        if (-not (
            ($value -ge [byte][char]'a' -and $value -le [byte][char]'z') -or
            ($value -ge [byte][char]'A' -and $value -le [byte][char]'Z') -or
            ($value -ge [byte][char]'0' -and $value -le [byte][char]'9') -or
            $value -eq [byte][char]'_' -or
            $value -eq [byte][char]'[' -or
            $value -eq [byte][char]']' -or
            $value -eq [byte][char]'.'
        )) {
            throw ("{0} contains an invalid byte 0x{1:X2}" -f
                $Description,
                $value)
        }
        $bytes.Add($value)
    }
    throw ("{0} has no bounded terminator" -f $Description)
}

function Read-DeltaFieldDescriptor {
    param(
        $Reader,
        [string]$TableName,
        [int]$FieldIndex
    )

    $description = "{0} field {1}" -f $TableName, $FieldIndex
    $maskByteCount = Read-DeltaBits `
        -Reader $Reader `
        -Count 3 `
        -Description "$description mask byte count"
    if ($maskByteCount -ne 1) {
        throw ("{0} mask byte count must be one, received {1}" -f
            $description,
            $maskByteCount)
    }
    $mask = Read-DeltaBits `
        -Reader $Reader `
        -Count 8 `
        -Description "$description mask"
    if (($mask -band [uint64]0x80) -ne 0 -or
        ($mask -band [uint64]0x02) -eq 0) {
        throw ("{0} has an invalid descriptor mask 0x{1:X2}" -f
            $description,
            $mask)
    }

    [uint64]$fieldType = 0
    if (($mask -band [uint64]0x01) -ne 0) {
        $fieldType = Read-DeltaBits `
            -Reader $Reader `
            -Count 32 `
            -Description "$description type"
    }
    $name = Read-DeltaString -Reader $Reader -Description "$description name"
    [uint64]$fieldOffset = 0
    [uint64]$fieldSize = 0
    [uint64]$significantBits = 0
    [uint64]$premultiplyRaw = 0
    [uint64]$postmultiplyRaw = 0
    if (($mask -band [uint64]0x04) -ne 0) {
        $fieldOffset = Read-DeltaBits `
            -Reader $Reader `
            -Count 16 `
            -Description "$description offset"
    }
    if (($mask -band [uint64]0x08) -ne 0) {
        $fieldSize = Read-DeltaBits `
            -Reader $Reader `
            -Count 8 `
            -Description "$description size"
    }
    if (($mask -band [uint64]0x10) -ne 0) {
        $significantBits = Read-DeltaBits `
            -Reader $Reader `
            -Count 8 `
            -Description "$description significant bits"
    }
    if (($mask -band [uint64]0x20) -ne 0) {
        $premultiplyRaw = Read-DeltaBits `
            -Reader $Reader `
            -Count 32 `
            -Description "$description premultiply"
    }
    if (($mask -band [uint64]0x40) -ne 0) {
        $postmultiplyRaw = Read-DeltaBits `
            -Reader $Reader `
            -Count 32 `
            -Description "$description postmultiply"
    }

    return [pscustomobject]@{
        Mask = [uint64]$mask
        FieldType = [uint64]$fieldType
        Name = $name
        FieldOffset = [int]$fieldOffset
        FieldSize = [int]$fieldSize
        SignificantBits = [int]$significantBits
        PremultiplyRaw = [uint64]$premultiplyRaw
        PostmultiplyRaw = [uint64]$postmultiplyRaw
    }
}

function Assert-DeltaDescriptorWellFormed {
    param(
        $Field,
        [string]$Description
    )

    $baseType = $Field.FieldType -band [uint64]0x7FFFFFFF
    if (@(
        [uint64]1,
        [uint64]2,
        [uint64]4,
        [uint64]8,
        [uint64]16,
        [uint64]32,
        [uint64]64,
        [uint64]128
    ) -notcontains $baseType) {
        throw ("{0} has unsupported field type 0x{1:X8}" -f
            $Description,
            $Field.FieldType)
    }
    if ($baseType -eq [uint64]128 -and
        ($Field.FieldType -band $deltaTypeSigned) -ne 0) {
        throw ("{0} marks a string field signed" -f $Description)
    }
    if ($Field.FieldSize -le 0 -or
        ([uint64]$Field.FieldOffset + [uint64]$Field.FieldSize) -gt 65536) {
        throw ("{0} has an invalid layout" -f $Description)
    }
    if ($Field.SignificantBits -le 0 -or $Field.SignificantBits -gt 32) {
        throw ("{0} has an invalid significant-bit count" -f $Description)
    }
    if ($Field.PremultiplyRaw -eq 0 -or $Field.PostmultiplyRaw -eq 0) {
        throw ("{0} has a zero wire multiplier" -f $Description)
    }
}

function Read-DeltaBundle {
    param([byte[]]$Bytes)

    if ($Bytes.Length -eq 0 -or $Bytes.Length -gt $maximumDeltaBundleBytes) {
        throw ("delta bundle length is outside the bounded range: {0}" -f
            $Bytes.Length)
    }
    $reader = New-DeltaBitReader -Bytes $Bytes
    $tables = New-Object 'System.Collections.Generic.List[object]'
    foreach ($expectedTableName in $canonicalDeltaTableOrder) {
        $opcode = Read-DeltaBits `
            -Reader $reader `
            -Count 8 `
            -Description "$expectedTableName opcode"
        if ($opcode -ne $svcDeltaDescription) {
            throw ("{0} opcode mismatch: expected {1}, received {2}" -f
                $expectedTableName,
                $svcDeltaDescription,
                $opcode)
        }
        $tableName = Read-DeltaString `
            -Reader $reader `
            -Description "delta table name"
        if ($tableName -cne $expectedTableName) {
            throw ("delta table order mismatch: expected {0}, received {1}" -f
                $expectedTableName,
                $tableName)
        }
        $fieldCount = [int](Read-DeltaBits `
            -Reader $reader `
            -Count 16 `
            -Description "$tableName field count")
        if ($fieldCount -le 0 -or $fieldCount -gt $maximumDeltaFieldsPerTable) {
            throw ("{0} has invalid field count {1}" -f
                $tableName,
                $fieldCount)
        }
        $fields = New-Object 'System.Collections.Generic.List[object]'
        $names = New-Object 'System.Collections.Generic.HashSet[string]' `
            ([StringComparer]::Ordinal)
        for ($fieldIndex = 0; $fieldIndex -lt $fieldCount; $fieldIndex++) {
            $field = Read-DeltaFieldDescriptor `
                -Reader $reader `
                -TableName $tableName `
                -FieldIndex $fieldIndex
            Assert-DeltaDescriptorWellFormed `
                -Field $field `
                -Description ("{0}.{1}" -f $tableName, $field.Name)
            if (-not $names.Add($field.Name)) {
                throw ("{0} repeats field {1}" -f $tableName, $field.Name)
            }
            $fields.Add($field)
        }
        $remainder = [int]($reader.BitPosition % 8)
        if ($remainder -ne 0) {
            $paddingBits = 8 - $remainder
            $padding = Read-DeltaBits `
                -Reader $reader `
                -Count $paddingBits `
                -Description "$tableName padding"
            if ($padding -ne 0) {
                throw ("{0} has nonzero alignment padding" -f $tableName)
            }
        }
        $tables.Add([pscustomobject]@{
            Name = $tableName
            Fields = $fields.ToArray()
        })
    }
    $bytesConsumed = [int]($reader.BitPosition / 8)
    $bundleBytes = New-Object byte[] $bytesConsumed
    [Array]::Copy($Bytes, 0, $bundleBytes, 0, $bundleBytes.Length)
    $trailingBytes = New-Object byte[] ($Bytes.Length - $bytesConsumed)
    if ($trailingBytes.Length -gt 0) {
        [Array]::Copy(
            $Bytes,
            $bytesConsumed,
            $trailingBytes,
            0,
            $trailingBytes.Length
        )
    }
    return [pscustomobject]@{
        Bytes = $bundleBytes
        BytesConsumed = $bytesConsumed
        TrailingBytes = $trailingBytes
        Tables = $tables.ToArray()
        TableCount = $tables.Count
        FieldCount = [int](($tables | ForEach-Object { $_.Fields.Count } |
            Measure-Object -Sum).Sum)
    }
}

function Read-BootstrapTailFloat {
    param(
        $Reader,
        [string]$FieldName
    )

    [byte[]]$bytes = Read-PayloadBytes `
        -Reader $Reader `
        -Count 4 `
        -FieldName $FieldName
    if (-not [BitConverter]::IsLittleEndian) {
        [Array]::Reverse($bytes)
    }
    return [single][BitConverter]::ToSingle($bytes, 0)
}

function Read-BootstrapTail {
    param(
        [byte[]]$Bytes,
        [ValidateRange(1, 65535)]
        [int]$ExpectedViewEntity
    )

    $reader = New-PayloadReader -Bytes $Bytes
    $opcode = Read-PayloadByte `
        -Reader $reader `
        -FieldName "svc_newmovevars opcode"
    if ($opcode -ne $svcNewMoveVars) {
        throw ("bootstrap tail expected svc_newmovevars 0x{0:X2}, received 0x{1:X2}" -f
            $svcNewMoveVars,
            $opcode)
    }
    $moveVariableNames = @(
        "gravity",
        "stop_speed",
        "maximum_speed",
        "spectator_maximum_speed",
        "accelerate",
        "air_accelerate",
        "water_accelerate",
        "friction",
        "edge_friction",
        "water_friction",
        "entity_gravity",
        "bounce",
        "step_size",
        "maximum_velocity",
        "z_maximum",
        "wave_height",
        "roll_angle",
        "roll_speed",
        "sky_color_red",
        "sky_color_green",
        "sky_color_blue",
        "sky_vector_x",
        "sky_vector_y",
        "sky_vector_z"
    )
    [single[]]$expectedMoveVariables = @(
        [single]800.0,
        [single]100.0,
        [single]320.0,
        [single]500.0,
        [single]10.0,
        [single]10.0,
        [single]10.0,
        [single]4.0,
        [single]2.0,
        [single]1.0,
        [single]1.0,
        [single]1.0,
        [single]18.0,
        [single]2000.0,
        $ExpectedZMaximum,
        [single]0.0,
        [single]0.0,
        [single]0.0,
        [single]0.0,
        [single]0.0,
        [single]0.0,
        [single]0.0,
        [single]0.0,
        [single]0.0
    )
    $moveVariables = New-Object 'System.Collections.Generic.List[object]'
    for ($index = 0; $index -lt 16; $index++) {
        $value = Read-BootstrapTailFloat `
            -Reader $reader `
            -FieldName $moveVariableNames[$index]
        if ($value -ne $expectedMoveVariables[$index]) {
            throw ("bootstrap tail {0} mismatch: expected {1}, received {2}" -f
                $moveVariableNames[$index],
                $expectedMoveVariables[$index],
                $value)
        }
        $moveVariables.Add($value)
    }
    $footsteps = Read-PayloadByte -Reader $reader -FieldName "footsteps"
    if ($footsteps -ne 1) {
        throw ("bootstrap tail footsteps mismatch: expected 1, received {0}" -f
            $footsteps)
    }
    for ($index = 16; $index -lt $moveVariableNames.Count; $index++) {
        $value = Read-BootstrapTailFloat `
            -Reader $reader `
            -FieldName $moveVariableNames[$index]
        if ($value -ne $expectedMoveVariables[$index]) {
            throw ("bootstrap tail {0} mismatch: expected {1}, received {2}" -f
                $moveVariableNames[$index],
                $expectedMoveVariables[$index],
                $value)
        }
        $moveVariables.Add($value)
    }
    $skyName = Read-PayloadAsciiString `
        -Reader $reader `
        -MaximumBytes 31 `
        -FieldName "sky name" `
        -AllowEmpty
    if ($skyName -cne "desert") {
        throw ("bootstrap tail sky name mismatch: expected desert, received {0}" -f
            $skyName)
    }
    $cdTrackOpcode = Read-PayloadByte `
        -Reader $reader `
        -FieldName "svc_cdtrack opcode"
    if ($cdTrackOpcode -ne $svcCdTrack) {
        throw ("bootstrap tail expected svc_cdtrack 0x{0:X2}, received 0x{1:X2}" -f
            $svcCdTrack,
            $cdTrackOpcode)
    }
    $cdTrack = Read-PayloadByte -Reader $reader -FieldName "CD audio track"
    $loopTrack = Read-PayloadByte -Reader $reader -FieldName "CD loop track"
    if ($cdTrack -ne $ExpectedCdTrack -or $loopTrack -ne $cdTrack) {
        throw ("bootstrap tail CD track mismatch: expected {0}/{0}, track={1},loop={2}" -f
            $ExpectedCdTrack,
            $cdTrack,
            $loopTrack)
    }
    $setViewOpcode = Read-PayloadByte `
        -Reader $reader `
        -FieldName "svc_setview opcode"
    if ($setViewOpcode -ne $svcSetView) {
        throw ("bootstrap tail expected svc_setview 0x{0:X2}, received 0x{1:X2}" -f
            $svcSetView,
            $setViewOpcode)
    }
    $viewEntity = Read-LittleEndianUInt16 `
        -Bytes $reader.Bytes `
        -Offset $reader.Offset
    $reader.Offset += 2
    if ($viewEntity -ne $ExpectedViewEntity) {
        throw ("bootstrap tail view entity mismatch: expected {0}, received {1}" -f
            $ExpectedViewEntity,
            $viewEntity)
    }
    if ($reader.Offset -ne $reader.Bytes.Length) {
        throw ("bootstrap tail has {0} unexpected trailing bytes" -f
            ($reader.Bytes.Length - $reader.Offset))
    }
    return [pscustomobject]@{
        Bytes = $Bytes
        MoveVariables = $moveVariables.ToArray()
        Footsteps = $footsteps
        SkyName = $skyName
        CdTrack = $cdTrack
        LoopTrack = $loopTrack
        ViewEntity = [int]$viewEntity
    }
}

function New-ExpectedDeltaField {
    param(
        [string]$Name,
        [uint64]$FieldType,
        [int]$FieldOffset,
        [int]$SignificantBits,
        [double]$Premultiply,
        [double]$Postmultiply = 1.0
    )

    return [pscustomobject]@{
        Name = $Name
        FieldType = $FieldType
        FieldOffset = $FieldOffset
        FieldSize = 1
        SignificantBits = $SignificantBits
        PremultiplyRaw = [uint64][Math]::Truncate(
            $Premultiply * [double]$deltaMultiplierScale)
        PostmultiplyRaw = [uint64][Math]::Truncate(
            $Postmultiply * [double]$deltaMultiplierScale)
    }
}

function Get-ExpectedUsercmdFields {
    $byteType = [uint64]1
    $shortType = [uint64]2
    $floatType = [uint64]4
    $integerType = [uint64]8
    $angleType = [uint64]16

    return @(
        (New-ExpectedDeltaField "lerp_msec" $shortType 0 9 1.0),
        (New-ExpectedDeltaField "msec" $byteType 2 8 1.0),
        (New-ExpectedDeltaField "viewangles[1]" $angleType 8 16 1.0),
        (New-ExpectedDeltaField "viewangles[0]" $angleType 4 16 1.0),
        (New-ExpectedDeltaField "buttons" $shortType 30 16 1.0),
        (New-ExpectedDeltaField "forwardmove" ($floatType -bor $deltaTypeSigned) 16 12 1.0),
        (New-ExpectedDeltaField "lightlevel" $byteType 28 8 1.0),
        (New-ExpectedDeltaField "sidemove" ($floatType -bor $deltaTypeSigned) 20 12 1.0),
        (New-ExpectedDeltaField "upmove" ($floatType -bor $deltaTypeSigned) 24 12 1.0),
        (New-ExpectedDeltaField "impulse" $byteType 32 8 1.0),
        (New-ExpectedDeltaField "viewangles[2]" $angleType 12 16 1.0),
        (New-ExpectedDeltaField "impact_index" $integerType 36 6 1.0),
        (New-ExpectedDeltaField "impact_position[0]" ($floatType -bor $deltaTypeSigned) 40 16 8.0),
        (New-ExpectedDeltaField "impact_position[1]" ($floatType -bor $deltaTypeSigned) 44 16 8.0),
        (New-ExpectedDeltaField "impact_position[2]" ($floatType -bor $deltaTypeSigned) 48 16 8.0)
    )
}

function Get-ExpectedMinimalDeltaTables {
    $floatSigned = [uint64]4 -bor $deltaTypeSigned
    $fractionalMultiplierField = New-ExpectedDeltaField `
        "entindex" `
        ([uint64]8 -bor $deltaTypeSigned) `
        4 `
        11 `
        1.2347
    if ($fractionalMultiplierField.PremultiplyRaw -ne [uint64]4938) {
        throw "delta proof multiplier oracle did not truncate 1.2347 to 4938"
    }
    return @(
        [pscustomobject]@{
            Name = "event_t"
            Fields = @(
                $fractionalMultiplierField
            )
        },
        [pscustomobject]@{
            Name = "weapon_data_t"
            Fields = @(
                (New-ExpectedDeltaField "m_iId" ([uint64]8) 0 6 1.0)
            )
        },
        [pscustomobject]@{
            Name = "usercmd_t"
            Fields = @(Get-ExpectedUsercmdFields)
        },
        [pscustomobject]@{
            Name = "custom_entity_state_t"
            Fields = @(
                (New-ExpectedDeltaField "origin[0]" $floatSigned 16 16 8.0)
            )
        },
        [pscustomobject]@{
            Name = "entity_state_player_t"
            Fields = @(
                (New-ExpectedDeltaField "origin[0]" $floatSigned 16 16 8.0)
            )
        },
        [pscustomobject]@{
            Name = "entity_state_t"
            Fields = @(
                (New-ExpectedDeltaField "origin[0]" $floatSigned 16 16 8.0)
            )
        },
        [pscustomobject]@{
            Name = "clientdata_t"
            Fields = @(
                (New-ExpectedDeltaField "origin[0]" $floatSigned 0 16 8.0)
            )
        }
    )
}

function Assert-ExactDeltaFields {
    param(
        [object[]]$Actual,
        [object[]]$Expected,
        [string]$Description
    )

    if ($Actual.Count -ne $Expected.Count) {
        throw ("{0} field count mismatch: expected {1}, received {2}" -f
            $Description,
            $Expected.Count,
            $Actual.Count)
    }
    $propertyNames = @(
        "Name",
        "FieldType",
        "FieldOffset",
        "FieldSize",
        "SignificantBits",
        "PremultiplyRaw",
        "PostmultiplyRaw"
    )
    for ($index = 0; $index -lt $Expected.Count; $index++) {
        foreach ($propertyName in $propertyNames) {
            if ($Actual[$index].$propertyName -cne
                $Expected[$index].$propertyName) {
                throw ("{0} field {1} {2} mismatch: expected {3}, received {4}" -f
                    $Description,
                    $index,
                    $propertyName,
                    $Expected[$index].$propertyName,
                    $Actual[$index].$propertyName)
            }
        }
    }
}

function Assert-DeltaBundleSemantics {
    param(
        $Bundle,
        [switch]$ExactMinimal
    )

    if ($Bundle.TableCount -ne $canonicalDeltaTableOrder.Count) {
        throw ("delta table count mismatch: expected {0}, received {1}" -f
            $canonicalDeltaTableOrder.Count,
            $Bundle.TableCount)
    }
    for ($index = 0; $index -lt $canonicalDeltaTableOrder.Count; $index++) {
        if ($Bundle.Tables[$index].Name -cne $canonicalDeltaTableOrder[$index]) {
            throw ("delta table order changed at index {0}" -f $index)
        }
    }

    $usercmdTables = @(
        $Bundle.Tables |
            Where-Object { $_.Name -ceq "usercmd_t" }
    )
    if ($usercmdTables.Count -ne 1) {
        throw ("delta bundle expected exactly one usercmd_t table, found {0}" -f
            $usercmdTables.Count)
    }
    Assert-ExactDeltaFields `
        -Actual @($usercmdTables[0].Fields) `
        -Expected @(Get-ExpectedUsercmdFields) `
        -Description "usercmd_t"

    if ($ExactMinimal) {
        $expectedTables = @(Get-ExpectedMinimalDeltaTables)
        for ($tableIndex = 0; $tableIndex -lt $expectedTables.Count; $tableIndex++) {
            if ($Bundle.Tables[$tableIndex].Name -cne
                $expectedTables[$tableIndex].Name) {
                throw ("minimal delta table mismatch at index {0}" -f $tableIndex)
            }
            Assert-ExactDeltaFields `
                -Actual @($Bundle.Tables[$tableIndex].Fields) `
                -Expected @($expectedTables[$tableIndex].Fields) `
                -Description $expectedTables[$tableIndex].Name
        }
    }
}

function Read-ServerInfoDeltaBootstrap {
    param(
        [byte[]]$Payload,
        [string]$ExpectedClientDllMd5,
        [switch]$ExactMinimal
    )

    if ($Payload.Length -eq 0 -or
        $Payload.Length -gt $maximumDeltaBundleBytes) {
        throw ("combined bootstrap payload length is outside the bounded range: {0}" -f
            $Payload.Length)
    }
    $reader = New-PayloadReader -Bytes $Payload
    $opcode = Read-PayloadByte -Reader $reader -FieldName "svc_serverinfo opcode"
    if ($opcode -ne $svcServerInfo) {
        throw ("first combined-bootstrap opcode mismatch: expected 0x{0:X2}, received 0x{1:X2}" -f
            $svcServerInfo,
            $opcode)
    }

    [void](Read-PayloadUInt32 -Reader $reader -FieldName "protocol")
    [void](Read-PayloadUInt32 -Reader $reader -FieldName "spawn count")
    [void](Read-PayloadUInt32 -Reader $reader -FieldName "wire map checksum")
    [void](Read-PayloadBytes `
        -Reader $reader `
        -Count 16 `
        -FieldName "client DLL MD5")
    [void](Read-PayloadByte -Reader $reader -FieldName "max clients")
    [void](Read-PayloadByte -Reader $reader -FieldName "player index")
    [void](Read-PayloadByte -Reader $reader -FieldName "deathmatch")
    [void](Read-PayloadAsciiString `
        -Reader $reader `
        -MaximumBytes 64 `
        -FieldName "game directory")
    [void](Read-PayloadAsciiString `
        -Reader $reader `
        -MaximumBytes 128 `
        -FieldName "hostname")
    [void](Read-PayloadAsciiString `
        -Reader $reader `
        -MaximumBytes 128 `
        -FieldName "map/model path")
    [void](Read-PayloadAsciiString `
        -Reader $reader `
        -MaximumBytes 128 `
        -FieldName "mapcycle")
    [void](Read-PayloadByte -Reader $reader -FieldName "secure mode")
    $extraInfoOpcode = Read-PayloadByte `
        -Reader $reader `
        -FieldName "svc_sendextrainfo opcode"
    if ($extraInfoOpcode -ne $svcSendExtraInfo) {
        throw ("combined bootstrap is missing svc_sendextrainfo at the exact serverinfo boundary")
    }
    [void](Read-PayloadAsciiString `
        -Reader $reader `
        -MaximumBytes 64 `
        -FieldName "fallback directory" `
        -AllowEmpty)
    [void](Read-PayloadByte -Reader $reader -FieldName "allow cheats")

    $prefixSize = $reader.Offset
    if ($prefixSize -ge $Payload.Length) {
        throw "combined bootstrap contains no delta-description bytes after serverinfo"
    }
    $prefix = New-Object byte[] $prefixSize
    [Array]::Copy($Payload, 0, $prefix, 0, $prefix.Length)
    $serverInfo = Read-ServerInfoPayload -Payload $prefix
    Assert-ServerInfoRuntimeExpectations `
        -ServerInfo $serverInfo `
        -ExpectedClientDllMd5 $ExpectedClientDllMd5

    $postServerInfoBytes = New-Object byte[] ($Payload.Length - $prefixSize)
    [Array]::Copy(
        $Payload,
        $prefixSize,
        $postServerInfoBytes,
        0,
        $postServerInfoBytes.Length
    )
    $bundle = Read-DeltaBundle -Bytes $postServerInfoBytes
    Assert-DeltaBundleSemantics -Bundle $bundle -ExactMinimal:$ExactMinimal
    if ($bundle.TrailingBytes.Length -eq 0) {
        throw "combined bootstrap contains no movevars/CD-track/setview tail"
    }
    $bootstrapTail = Read-BootstrapTail `
        -Bytes $bundle.TrailingBytes `
        -ExpectedViewEntity 1
    return [pscustomobject]@{
        Payload = $Payload
        ServerInfo = $serverInfo
        ServerInfoBytes = $prefixSize
        DeltaBytes = $bundle.Bytes
        DeltaBundle = $bundle
        BootstrapTail = $bootstrapTail
    }
}

function Invoke-DeltaInitialHandshake {
    param(
        [System.Net.Sockets.UdpClient]$Client,
        [System.Diagnostics.Process]$ServerProcess,
        $OutputCapture,
        [DateTime]$Deadline,
        [System.Net.IPEndPoint]$ServerEndpoint
    )

    $challengeRequest = New-ConnectionlessDatagram `
        -Text ("getchallenge steam" + [char]10) `
        -Tail ([byte[]](0x00))
    Send-ExactUdpDatagram `
        -Client $Client `
        -Packet $challengeRequest `
        -Description "delta proof challenge request"
    $challengeDatagram = Receive-UdpDatagram `
        -Client $Client `
        -ServerProcess $ServerProcess `
        -OutputCapture $OutputCapture `
        -Deadline $Deadline `
        -ExpectedRemoteEndpoint $ServerEndpoint `
        -Description "delta proof challenge response"
    [byte[]]$challengeResponse = $challengeDatagram.Bytes
    Assert-ConnectionlessPrefix `
        -Packet $challengeResponse `
        -Description "delta proof challenge response"
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
        throw "malformed delta-proof challenge response"
    }

    $clientPort = ([System.Net.IPEndPoint]$Client.Client.LocalEndPoint).Port
    $protocolInfo = '\prot\3\unique\-1\raw\steam\cdkey\00000000000000000000000000000000'
    $userInfo = '\name\delta_description_probe\model\gordon'
    $connectLine = ('connect 48 {0} "{1}" "{2}"' -f
        $challengeValue,
        $protocolInfo,
        $userInfo) + [char]10
    $connectRequest = New-ConnectionlessDatagram `
        -Text $connectLine `
        -Tail ([byte[]]@(0x00, 0x01, 0x7F, 0x80, 0xFF))
    Send-ExactUdpDatagram `
        -Client $Client `
        -Packet $connectRequest `
        -Description "delta proof connect request"
    $acceptDatagram = Receive-UdpDatagram `
        -Client $Client `
        -ServerProcess $ServerProcess `
        -OutputCapture $OutputCapture `
        -Deadline $Deadline `
        -ExpectedRemoteEndpoint $ServerEndpoint `
        -Description "delta proof connect accept response"
    $expectedAcceptResponse = New-ConnectionlessDatagram `
        -Text ('B 1 "127.0.0.1:{0}" 0 5971' -f $clientPort) `
        -Tail ([byte[]](0x00))
    Assert-ExactBytes `
        -Actual $acceptDatagram.Bytes `
        -Expected $expectedAcceptResponse `
        -Description "delta proof connect accept response"

    $serverOneDatagram = Receive-UdpDatagram `
        -Client $Client `
        -ServerProcess $ServerProcess `
        -OutputCapture $OutputCapture `
        -Deadline $Deadline `
        -ExpectedRemoteEndpoint $ServerEndpoint `
        -Description "delta proof initial reliable netchan packet"
    $serverOne = Read-SequencedDatagram `
        -Packet $serverOneDatagram.Bytes `
        -Description "delta proof initial reliable netchan packet"
    Assert-SequencedHeader `
        -Decoded $serverOne `
        -ExpectedSequence 1 `
        -ExpectedAcknowledgement 0 `
        -ExpectedReliableToggle $true `
        -ExpectedReliableAcknowledgementToggle $false `
        -Description "delta proof initial reliable netchan packet"
    Assert-NopPayload `
        -Payload $serverOne.Payload `
        -Description "delta proof initial svc_nop payload"

    [byte[]]$nopPayload = New-NopPayload
    $transportAck = New-SequencedDatagram `
        -Sequence 1 `
        -Acknowledgement 1 `
        -ReliableAcknowledgementToggle `
        -Payload $nopPayload
    Send-ExactUdpDatagram `
        -Client $Client `
        -Packet $transportAck `
        -Description "delta proof transport reliable acknowledgement"
    [void](Receive-AndAssertNopAck `
        -Client $Client `
        -ServerProcess $ServerProcess `
        -OutputCapture $OutputCapture `
        -Deadline $Deadline `
        -ServerEndpoint $ServerEndpoint `
        -ExpectedServerSequence 2 `
        -ExpectedClientSequence 1 `
        -ExpectedServerReliableToggle $false `
        -ExpectedClientReliableToggle $false `
        -Description "delta proof established netchan acknowledgement")

    return [pscustomobject]@{
        Challenge = $challengeValue
        ClientPort = $clientPort
        NopPayload = $nopPayload
        NewPayload = (New-StringCommandPayload -Command "new")
        ClientSequence = [uint32]2
        LatestServerSequence = [uint32]2
    }
}

function Send-DeltaClientPacket {
    param(
        [System.Net.Sockets.UdpClient]$Client,
        [uint32]$Sequence,
        [uint32]$Acknowledgement,
        [bool]$ServerReliableAcknowledgementState,
        [byte[]]$Payload,
        [switch]$ReliablePayload,
        [string]$Description
    )

    $arguments = @{
        Sequence = $Sequence
        Acknowledgement = $Acknowledgement
        Payload = $Payload
    }
    if ($ServerReliableAcknowledgementState) {
        $arguments.ReliableAcknowledgementToggle = $true
    }
    if ($ReliablePayload) {
        $arguments.ReliableToggle = $true
    }
    $packet = New-SequencedDatagram @arguments
    Send-ExactUdpDatagram `
        -Client $Client `
        -Packet $packet `
        -Description $Description
}

function Invoke-UnfragmentedDeltaBootstrap {
    param(
        [System.Net.Sockets.UdpClient]$Client,
        [System.Diagnostics.Process]$ServerProcess,
        $OutputCapture,
        [string]$StdoutPath,
        [DateTime]$Deadline,
        [System.Net.IPEndPoint]$ServerEndpoint,
        $Handshake,
        [string]$ExpectedClientDllMd5
    )

    Send-DeltaClientPacket `
        -Client $Client `
        -Sequence 2 `
        -Acknowledgement 2 `
        -ServerReliableAcknowledgementState $true `
        -Payload $Handshake.NewPayload `
        -ReliablePayload `
        -Description "reliable client new command for delta bootstrap"
    $bootstrapDatagram = Receive-UdpDatagram `
        -Client $Client `
        -ServerProcess $ServerProcess `
        -OutputCapture $OutputCapture `
        -Deadline $Deadline `
        -ExpectedRemoteEndpoint $ServerEndpoint `
        -Description "unfragmented combined signon bootstrap"
    $bootstrapPacket = Read-SequencedDatagram `
        -Packet $bootstrapDatagram.Bytes `
        -Description "unfragmented combined signon bootstrap"
    Assert-SequencedHeader `
        -Decoded $bootstrapPacket `
        -ExpectedSequence 3 `
        -ExpectedAcknowledgement 2 `
        -ExpectedReliableToggle $true `
        -ExpectedReliableAcknowledgementToggle $true `
        -Description "unfragmented combined signon bootstrap"
    $decoded = Read-ServerInfoDeltaBootstrap `
        -Payload $bootstrapPacket.Payload `
        -ExpectedClientDllMd5 $ExpectedClientDllMd5 `
        -ExactMinimal

    [void](Update-ProcessOutputCapture -State $OutputCapture)
    $beforeAck = Get-SharedFileText -Path $StdoutPath
    if ($beforeAck.IndexOf(
            "goldsrc_signon_bootstrap_acknowledged:",
            [StringComparison]::Ordinal) -ge 0) {
        throw "delta signon advanced before its reliable acknowledgement"
    }

    Send-DeltaClientPacket `
        -Client $Client `
        -Sequence 3 `
        -Acknowledgement 3 `
        -ServerReliableAcknowledgementState $false `
        -Payload $Handshake.NopPayload `
        -Description "combined signon bootstrap reliable acknowledgement"
    [void](Receive-AndAssertNopAck `
        -Client $Client `
        -ServerProcess $ServerProcess `
        -OutputCapture $OutputCapture `
        -Deadline $Deadline `
        -ServerEndpoint $ServerEndpoint `
        -ExpectedServerSequence 4 `
        -ExpectedClientSequence 3 `
        -ExpectedServerReliableToggle $false `
        -ExpectedClientReliableToggle $true `
        -Description "combined signon bootstrap acknowledgement response")
    Wait-ForStdoutToken `
        -Process $ServerProcess `
        -OutputCapture $OutputCapture `
        -StdoutPath $StdoutPath `
        -Deadline $Deadline `
        -Token "goldsrc_signon_bootstrap_acknowledged:" `
        -Description "combined signon bootstrap acknowledgement diagnostic"

    return [pscustomobject]@{
        Decoded = $decoded
        ClientSequence = [uint32]3
        LatestServerSequence = [uint32]4
        ServerReliableAcknowledgementState = $false
        RetransmissionObserved = $false
        RetransmittedIdentical = $false
        WrongAckRejected = $false
        NonCoveringAckRejected = $false
        FragmentCount = 0
    }
}

function Invoke-FragmentedNegativeDeltaBootstrap {
    param(
        [System.Net.Sockets.UdpClient]$Client,
        [System.Diagnostics.Process]$ServerProcess,
        $OutputCapture,
        [string]$StdoutPath,
        [DateTime]$Deadline,
        [System.Net.IPEndPoint]$ServerEndpoint,
        $Handshake,
        [string]$ExpectedClientDllMd5,
        [switch]$StopAfterRetransmission
    )

    Send-DeltaClientPacket `
        -Client $Client `
        -Sequence 2 `
        -Acknowledgement 2 `
        -ServerReliableAcknowledgementState $true `
        -Payload $Handshake.NewPayload `
        -ReliablePayload `
        -Description "reliable client new command for fragmented delta bootstrap"
    $firstDatagram = Receive-ProofDatagram `
        -Client $Client `
        -ServerProcess $ServerProcess `
        -OutputCapture $OutputCapture `
        -Deadline $Deadline `
        -ServerEndpoint $ServerEndpoint `
        -Description "first fragmented delta bootstrap packet"
    $first = Read-FragmentedSequencedDatagram `
        -Packet $firstDatagram.Bytes `
        -Description "first fragmented delta bootstrap packet"
    if ($first.Datagram.Sequence -ne 3 -or
        $first.Datagram.Acknowledgement -ne 2 -or
        -not $first.Datagram.ReliableAcknowledgementToggle) {
        throw "first fragmented delta bootstrap has invalid outer progression"
    }
    if ($first.FragmentIndex -ne 1 -or $first.FragmentCount -lt 2) {
        throw ("expanded delta fixture did not produce a multi-fragment bootstrap: index={0},count={1}" -f
            $first.FragmentIndex,
            $first.FragmentCount)
    }

    $slots = New-Object object[] $first.FragmentCount
    [uint32]$clientSequence = 2
    [uint32]$latestServerSequence = 3
    # The transport bootstrap left the local reliable generation set. Staging
    # the first fragmented signon carrier toggles it to false.
    $currentReliableState = $false
    [void](Update-ProcessOutputCapture -State $OutputCapture)
    $beforeAck = Get-SharedFileText -Path $StdoutPath
    if ($beforeAck.IndexOf(
            "goldsrc_signon_bootstrap_acknowledged:",
            [StringComparison]::Ordinal) -ge 0) {
        throw "fragmented delta bootstrap advanced while its ACK was withheld"
    }

    # Correct reliable generation, but the ACK does not cover carrier 3.
    $clientSequence++
    Send-DeltaClientPacket `
        -Client $Client `
        -Sequence $clientSequence `
        -Acknowledgement 2 `
        -ServerReliableAcknowledgementState $currentReliableState `
        -Payload $Handshake.NopPayload `
        -Description "non-covering fragmented delta acknowledgement"
    $ordinaryDatagram = Receive-ProofDatagram `
        -Client $Client `
        -ServerProcess $ServerProcess `
        -OutputCapture $OutputCapture `
        -Deadline $Deadline `
        -ServerEndpoint $ServerEndpoint `
        -Description "non-covering fragmented delta response"
    $ordinary = Read-SequencedDatagram `
        -Packet $ordinaryDatagram.Bytes `
        -Description "non-covering fragmented delta response"
    if ($ordinary.FragmentPresent -or
        $ordinary.Sequence -ne 4 -or
        $ordinary.Acknowledgement -ne $clientSequence) {
        throw "non-covering fragmented delta ACK unexpectedly advanced the transfer"
    }
    Assert-NopPayload `
        -Payload $ordinary.Payload `
        -Description "non-covering fragmented delta response"
    $latestServerSequence = $ordinary.Sequence

    # A covering ACK with the wrong reliable generation must also leave the
    # same frozen fragment pending. Depending on the sender cadence the first
    # response may be ordinary; a second covering trigger must retransmit.
    $wrongState = -not $currentReliableState
    $resend = $null
    for ($attempt = 0; $attempt -lt 2 -and $null -eq $resend; $attempt++) {
        $clientSequence++
        Send-DeltaClientPacket `
            -Client $Client `
            -Sequence $clientSequence `
            -Acknowledgement $latestServerSequence `
            -ServerReliableAcknowledgementState $wrongState `
            -Payload $Handshake.NopPayload `
            -Description "wrong reliable fragmented delta acknowledgement"
        $candidateDatagram = Receive-ProofDatagram `
            -Client $Client `
            -ServerProcess $ServerProcess `
            -OutputCapture $OutputCapture `
            -Deadline $Deadline `
            -ServerEndpoint $ServerEndpoint `
            -Description "fragmented delta retransmission candidate"
        $candidate = Read-SequencedDatagram `
            -Packet $candidateDatagram.Bytes `
            -Description "fragmented delta retransmission candidate"
        if ($candidate.Sequence -ne ([uint32]($latestServerSequence + 1)) -or
            $candidate.Acknowledgement -ne $clientSequence) {
            throw "wrong-ACK response changed outer sequence progression"
        }
        $latestServerSequence = $candidate.Sequence
        if ($candidate.FragmentPresent) {
            $resend = Read-FragmentedSequencedDatagram `
                -Packet $candidateDatagram.Bytes `
                -Description "fragmented delta retransmission"
        } else {
            Assert-NopPayload `
                -Payload $candidate.Payload `
                -Description "ordinary response before delta retransmission"
        }
    }
    if ($null -eq $resend) {
        throw "wrong reliable ACK did not retain and retransmit the delta bootstrap"
    }
    if ($resend.RawFragmentId -ne $first.RawFragmentId) {
        throw "fragment retransmission changed fragment identity"
    }
    Assert-ExactBytes `
        -Actual $resend.FragmentBytes `
        -Expected $first.FragmentBytes `
        -Description "byte-identical decoded delta fragment retransmission"
    if ((Add-ProbeFragment `
            -Slots $slots `
            -Fragment $resend `
            -Description "retransmitted first delta fragment") -cne "accepted") {
        throw "the retained first delta fragment was not accepted"
    }
    if ($StopAfterRetransmission) {
        return [pscustomobject]@{
            Decoded = $null
            ClientSequence = [uint32]$clientSequence
            LatestServerSequence = [uint32]$latestServerSequence
            ServerReliableAcknowledgementState =
                [bool]$currentReliableState
            RetransmissionObserved = $true
            RetransmittedIdentical = $true
            WrongAckRejected = $true
            NonCoveringAckRejected = $true
            FragmentCount = $first.FragmentCount
            Pending = $true
        }
    }

    $current = $resend
    while ($true) {
        $clientSequence++
        Send-FragmentAcknowledgement `
            -Client $Client `
            -ClientSequence $clientSequence `
            -ServerSequence $latestServerSequence `
            -ReliableAcknowledgementToggle $currentReliableState `
            -NopPayload $Handshake.NopPayload `
            -Description "correct fragmented delta acknowledgement"
        $responseDatagram = Receive-ProofDatagram `
            -Client $Client `
            -ServerProcess $ServerProcess `
            -OutputCapture $OutputCapture `
            -Deadline $Deadline `
            -ServerEndpoint $ServerEndpoint `
            -Description "fragmented delta acknowledgement response"

        if ($current.FragmentIndex -eq $current.FragmentCount) {
            $finalResponse = Read-SequencedDatagram `
                -Packet $responseDatagram.Bytes `
                -Description "final fragmented delta acknowledgement response"
            if ($finalResponse.FragmentPresent -or
                $finalResponse.Sequence -ne ([uint32]($latestServerSequence + 1)) -or
                $finalResponse.Acknowledgement -ne $clientSequence) {
                throw "final fragmented delta ACK response is invalid"
            }
            Assert-NopPayload `
                -Payload $finalResponse.Payload `
                -Description "final fragmented delta acknowledgement response"
            $latestServerSequence = $finalResponse.Sequence
            break
        }

        $next = Read-FragmentedSequencedDatagram `
            -Packet $responseDatagram.Bytes `
            -Description "next fragmented delta bootstrap packet"
        if ($next.Datagram.Sequence -ne ([uint32]($latestServerSequence + 1)) -or
            $next.Datagram.Acknowledgement -ne $clientSequence -or
            $next.FragmentCount -ne $first.FragmentCount -or
            $next.FragmentIndex -ne ($current.FragmentIndex + 1)) {
            throw "fragmented delta transfer changed order or outer progression"
        }
        if ((Add-ProbeFragment `
                -Slots $slots `
                -Fragment $next `
                -Description "delta bootstrap fragment") -cne "accepted") {
            throw "an initial delta fragment was unexpectedly duplicated"
        }
        $latestServerSequence = $next.Datagram.Sequence
        $current = $next
        $currentReliableState = -not $currentReliableState
    }

    [byte[]]$reassembled = Join-ProbeFragments -Slots $slots
    $decoded = Read-ServerInfoDeltaBootstrap `
        -Payload $reassembled `
        -ExpectedClientDllMd5 $ExpectedClientDllMd5
    Wait-ForStdoutToken `
        -Process $ServerProcess `
        -OutputCapture $OutputCapture `
        -StdoutPath $StdoutPath `
        -Deadline $Deadline `
        -Token "goldsrc_signon_bootstrap_acknowledged:" `
        -Description "fragmented signon bootstrap acknowledgement diagnostic"

    return [pscustomobject]@{
        Decoded = $decoded
        ClientSequence = [uint32]$clientSequence
        LatestServerSequence = [uint32]$latestServerSequence
        ServerReliableAcknowledgementState = [bool]$currentReliableState
        RetransmissionObserved = $true
        RetransmittedIdentical = $true
        WrongAckRejected = $true
        NonCoveringAckRejected = $true
        FragmentCount = $first.FragmentCount
        Pending = $false
    }
}

function Invoke-PendingDeltaDisconnect {
    param(
        [System.Net.Sockets.UdpClient]$Client,
        [System.Diagnostics.Process]$ServerProcess,
        $OutputCapture,
        [string]$StdoutPath,
        [DateTime]$Deadline,
        [System.Net.IPEndPoint]$ServerEndpoint,
        $PendingBootstrap
    )

    if (-not $PendingBootstrap.Pending -or
        -not $PendingBootstrap.RetransmissionObserved -or
        -not $PendingBootstrap.RetransmittedIdentical) {
        throw "delta reset requires a proven pending byte-identical retransmission"
    }
    if ($PendingBootstrap.LatestServerSequence -eq 0) {
        throw "pending delta reset has no server carrier sequence"
    }

    [uint32]$clientSequence =
        [uint32]($PendingBootstrap.ClientSequence + 1)
    [uint32]$nonCoveringAcknowledgement =
        [uint32]($PendingBootstrap.LatestServerSequence - 1)
    [bool]$wrongReliableAcknowledgement =
        -not [bool]$PendingBootstrap.ServerReliableAcknowledgementState
    [byte[]]$disconnectPayload =
        New-StringCommandPayload -Command "dropclient`n"
    Send-DeltaClientPacket `
        -Client $Client `
        -Sequence $clientSequence `
        -Acknowledgement $nonCoveringAcknowledgement `
        -ServerReliableAcknowledgementState $wrongReliableAcknowledgement `
        -Payload $disconnectPayload `
        -ReliablePayload `
        -Description "reliable disconnect with pending delta bootstrap"

    Wait-ForStdoutToken `
        -Process $ServerProcess `
        -OutputCapture $OutputCapture `
        -StdoutPath $StdoutPath `
        -Deadline $Deadline `
        -Token "goldsrc_delta_session_reset:" `
        -Description "same-process pending delta reset diagnostic"

    return [pscustomobject]@{
        ClientSequence = $clientSequence
        Acknowledgement = $nonCoveringAcknowledgement
        ServerReliableAcknowledgementState =
            $wrongReliableAcknowledgement
    }
}

function Invoke-DuplicateDeltaTrigger {
    param(
        [System.Net.Sockets.UdpClient]$Client,
        [System.Diagnostics.Process]$ServerProcess,
        $OutputCapture,
        [string]$StdoutPath,
        [DateTime]$Deadline,
        [System.Net.IPEndPoint]$ServerEndpoint,
        $Handshake,
        $Bootstrap
    )

    $clientSequence = [uint32]($Bootstrap.ClientSequence + 1)
    Send-DeltaClientPacket `
        -Client $Client `
        -Sequence $clientSequence `
        -Acknowledgement $Bootstrap.LatestServerSequence `
        -ServerReliableAcknowledgementState $Bootstrap.ServerReliableAcknowledgementState `
        -Payload $Handshake.NewPayload `
        -ReliablePayload `
        -Description "duplicate reliable delta bootstrap trigger"
    $responseDatagram = Receive-ProofDatagram `
        -Client $Client `
        -ServerProcess $ServerProcess `
        -OutputCapture $OutputCapture `
        -Deadline $Deadline `
        -ServerEndpoint $ServerEndpoint `
        -Description "duplicate delta trigger suppression response"
    $response = Read-SequencedDatagram `
        -Packet $responseDatagram.Bytes `
        -Description "duplicate delta trigger suppression response"
    if ($response.FragmentPresent -or
        $response.Sequence -ne ([uint32]($Bootstrap.LatestServerSequence + 1)) -or
        $response.Acknowledgement -ne $clientSequence) {
        throw "duplicate delta trigger changed transport progression"
    }
    Assert-NopPayload `
        -Payload $response.Payload `
        -Description "duplicate delta trigger suppression response"
    Wait-ForStdoutToken `
        -Process $ServerProcess `
        -OutputCapture $OutputCapture `
        -StdoutPath $StdoutPath `
        -Deadline $Deadline `
        -Token "goldsrc_client_new_suppressed:" `
        -Description "duplicate delta trigger suppression diagnostic"
    return [pscustomobject]@{
        ClientSequence = $clientSequence
        LatestServerSequence = [uint32]$response.Sequence
        ServerReliableAcknowledgementState =
            [bool]$Bootstrap.ServerReliableAcknowledgementState
    }
}

function Invoke-ObservedResourceContinuation {
    param(
        [System.Net.Sockets.UdpClient]$Client,
        [System.Diagnostics.Process]$ServerProcess,
        $OutputCapture,
        [DateTime]$Deadline,
        [System.Net.IPEndPoint]$ServerEndpoint,
        $Handshake,
        $Bootstrap,
        [object[]]$ExpectedEntries,
        [string]$StdoutPath,
        [ValidateSet("None", "Positive", "Negative", "World")]
        [string]$PostResourceMode = "None",
        [switch]$WorldBaselineNegativeProof,
        [switch]$ReceiveFirstSnapshot,
        [switch]$FirstSnapshotNegativeValidation
    )

    [byte[]]$sendResourcesPayload = New-StringCommandPayload -Command "sendres"
    [byte[]]$closeMenusPayload =
        New-StringCommandPayload -Command "closemenus `n"
    $sendResourcesPayload = [byte[]](
        $sendResourcesPayload +
        $closeMenusPayload +
        $closeMenusPayload
    )
    $clientSequence = [uint32]($Bootstrap.ClientSequence + 1)
    Send-DeltaClientPacket `
        -Client $Client `
        -Sequence $clientSequence `
        -Acknowledgement $Bootstrap.LatestServerSequence `
        -ServerReliableAcknowledgementState $Bootstrap.ServerReliableAcknowledgementState `
        -Payload $sendResourcesPayload `
        -ReliablePayload `
        -Description "observed sendres continuation after delta bootstrap"
    $manifestDatagram = Receive-ProofDatagram `
        -Client $Client `
        -ServerProcess $ServerProcess `
        -OutputCapture $OutputCapture `
        -Deadline $Deadline `
        -ServerEndpoint $ServerEndpoint `
        -Description "resource manifest after delta bootstrap"
    $manifestPacket = Read-SequencedDatagram `
        -Packet $manifestDatagram.Bytes `
        -Description "resource manifest after delta bootstrap"
    if ($manifestPacket.FragmentPresent -or
        $manifestPacket.Sequence -ne ([uint32]($Bootstrap.LatestServerSequence + 1)) -or
        $manifestPacket.Acknowledgement -ne $clientSequence -or
        -not $manifestPacket.ReliableToggle) {
        throw "resource continuation did not produce one bounded reliable manifest"
    }
    $manifest = Read-ResourceManifestPayload `
        -Payload $manifestPacket.Payload `
        -ExpectedSpawnCount $Bootstrap.Decoded.ServerInfo.SpawnCount
    Assert-ManifestMatchesFixture `
        -Manifest $manifest `
        -ExpectedEntries $ExpectedEntries `
        -Description "resource manifest after delta bootstrap"
    [uint32]$latestServerSequence = [uint32]$manifestPacket.Sequence

    $manifestAckState =
        -not [bool]$Bootstrap.ServerReliableAcknowledgementState
    $clientSequence++
    [byte[]]$acknowledgementPayload = $Handshake.NopPayload
    if ($PostResourceMode -eq "Positive" -or
        $PostResourceMode -eq "World") {
        $acknowledgementPayload =
            New-ObservedGoldSrcMovePayload -Sequence $clientSequence
    } elseif ($PostResourceMode -eq "Negative") {
        [byte[]]$earlyMove =
            New-ObservedGoldSrcMovePayload -Sequence $clientSequence
        Send-DeltaClientPacket `
            -Client $Client `
            -Sequence $clientSequence `
            -Acknowledgement $Bootstrap.LatestServerSequence `
            -ServerReliableAcknowledgementState $Bootstrap.ServerReliableAcknowledgementState `
            -Payload $earlyMove `
            -Description "valid move rejected before resource acknowledgement"
        Wait-ForStdoutToken `
            -Process $ServerProcess `
            -OutputCapture $OutputCapture `
            -StdoutPath $StdoutPath `
            -Deadline $Deadline `
            -Token "reason=invalid-signon-phase" `
            -Description "invalid move phase rejection"
        $retransmissionDatagram = Receive-ProofDatagram `
            -Client $Client `
            -ServerProcess $ServerProcess `
            -OutputCapture $OutputCapture `
            -Deadline $Deadline `
            -ServerEndpoint $ServerEndpoint `
            -Description "manifest retained after invalid move phase"
        $retransmission = Read-SequencedDatagram `
            -Packet $retransmissionDatagram.Bytes `
            -Description "manifest retained after invalid move phase"
        $latestServerSequence = [uint32]$retransmission.Sequence
        if ($retransmission.ReliableToggle) {
            Assert-ExactBytes `
                -Actual $retransmission.Payload `
                -Expected $manifestPacket.Payload `
                -Description "retained manifest retransmission"
        } else {
            Assert-NopPayload `
                -Payload $retransmission.Payload `
                -Description "ordinary response while manifest remains pending"
        }

        Send-DeltaClientPacket `
            -Client $Client `
            -Sequence $clientSequence `
            -Acknowledgement $Bootstrap.LatestServerSequence `
            -ServerReliableAcknowledgementState $Bootstrap.ServerReliableAcknowledgementState `
            -Payload $earlyMove `
            -Description "duplicate outer move sequence"

        Send-DeltaClientPacket `
            -Client $Client `
            -Sequence ([uint32]($clientSequence - 1)) `
            -Acknowledgement $Bootstrap.LatestServerSequence `
            -ServerReliableAcknowledgementState $Bootstrap.ServerReliableAcknowledgementState `
            -Payload (New-ObservedGoldSrcMovePayload `
                -Sequence ([uint32]($clientSequence - 1))) `
            -Description "out-of-order outer move sequence"

        $clientSequence++
        [byte[]]$acknowledgementPayload = @(
            [byte]2,
            [byte]11,
            [byte]0,
            [byte]0
        )
    }
    Send-DeltaClientPacket `
        -Client $Client `
        -Sequence $clientSequence `
        -Acknowledgement $latestServerSequence `
        -ServerReliableAcknowledgementState $manifestAckState `
        -Payload $acknowledgementPayload `
        -Description "resource manifest acknowledgement after delta bootstrap"
    $finalDatagram = Receive-ProofDatagram `
        -Client $Client `
        -ServerProcess $ServerProcess `
        -OutputCapture $OutputCapture `
        -Deadline $Deadline `
        -ServerEndpoint $ServerEndpoint `
        -Description "final resource acknowledgement response"
    $final = Read-SequencedDatagram `
        -Packet $finalDatagram.Bytes `
        -Description "final resource acknowledgement response"
    if ($final.FragmentPresent -or
        $final.Sequence -ne ([uint32]($latestServerSequence + 1)) -or
        $final.Acknowledgement -ne $clientSequence) {
        throw "resource continuation final response changed netchan progression"
    }
    if ($PostResourceMode -ne "World") {
        Assert-NopPayload `
            -Payload $final.Payload `
            -Description "final resource acknowledgement response"
    }

    if ($PostResourceMode -eq "Negative") {
        Wait-ForStdoutToken `
            -Process $ServerProcess `
            -OutputCapture $OutputCapture `
            -StdoutPath $StdoutPath `
            -Deadline $Deadline `
            -Token "move_reason=truncated_envelope" `
            -Description "truncated move rejection"
        $latestServerSequence = [uint32]$final.Sequence
        [object[]]$negativeCases = @(
            [pscustomobject]@{
                Name = "invalid count"
                PayloadFactory = {
                    param([uint32]$Sequence)
                    New-GoldSrcMovePayloadFromBody `
                        -Sequence $Sequence `
                        -Body ([byte[]]@(0, 62, 1))
                }
                Token = "move_reason=command_count_exceeded"
            },
            [pscustomobject]@{
                Name = "invalid checksum"
                PayloadFactory = {
                    param([uint32]$Sequence)
                    [byte[]]$payload =
                        New-ObservedGoldSrcMovePayload -Sequence $Sequence
                    $payload[2] = [byte]($payload[2] -bxor 1)
                    return $payload
                }
                Token = "move_reason=invalid_checksum"
            },
            [pscustomobject]@{
                Name = "unsupported opcode"
                PayloadFactory = {
                    param([uint32]$Sequence)
                    return [byte[]]@(9)
                }
                Token = "reason=unknown_opcode"
            }
        )
        foreach ($negativeCase in $negativeCases) {
            $clientSequence++
            [byte[]]$negativePayload =
                & $negativeCase.PayloadFactory $clientSequence
            Send-DeltaClientPacket `
                -Client $Client `
                -Sequence $clientSequence `
                -Acknowledgement $latestServerSequence `
                -ServerReliableAcknowledgementState $manifestAckState `
                -Payload $negativePayload `
                -Description $negativeCase.Name
            Wait-ForStdoutToken `
                -Process $ServerProcess `
                -OutputCapture $OutputCapture `
                -StdoutPath $StdoutPath `
                -Deadline $Deadline `
                -Token $negativeCase.Token `
                -Description ($negativeCase.Name + " rejection")
            $caseResponseDatagram = Receive-ProofDatagram `
                -Client $Client `
                -ServerProcess $ServerProcess `
                -OutputCapture $OutputCapture `
                -Deadline $Deadline `
                -ServerEndpoint $ServerEndpoint `
                -Description ($negativeCase.Name + " response")
            $caseResponse = Read-SequencedDatagram `
                -Packet $caseResponseDatagram.Bytes `
                -Description ($negativeCase.Name + " response")
            Assert-NopPayload `
                -Payload $caseResponse.Payload `
                -Description ($negativeCase.Name + " response")
            $latestServerSequence = [uint32]$caseResponse.Sequence
        }

        $clientSequence++
        Send-DeltaClientPacket `
            -Client $Client `
            -Sequence $clientSequence `
            -Acknowledgement $latestServerSequence `
            -ServerReliableAcknowledgementState $manifestAckState `
            -Payload (New-ObservedGoldSrcMovePayload `
                -Sequence $clientSequence) `
            -Description "valid post-resource move after negative cases"
        $acceptedResponseDatagram = Receive-ProofDatagram `
            -Client $Client `
            -ServerProcess $ServerProcess `
            -OutputCapture $OutputCapture `
            -Deadline $Deadline `
            -ServerEndpoint $ServerEndpoint `
            -Description "valid post-resource move response"
        $acceptedResponse = Read-SequencedDatagram `
            -Packet $acceptedResponseDatagram.Bytes `
            -Description "valid post-resource move response"
        Assert-NopPayload `
            -Payload $acceptedResponse.Payload `
            -Description "valid post-resource move response"
        $final = $acceptedResponse
    }
    $baselineReceived = $false
    $baselineFragmented = $false
    $baselineRetransmissionObserved = $false
    $baselineRetransmittedIdentical = $false
    $baselineWithheldMoveCount = 0
    $snapshot = $null
    $firstSnapshotKeepaliveObserved = $false
    if ($PostResourceMode -eq "World") {
        [byte[]]$baselinePayload = @()
        [uint32]$latestServerSequence = [uint32]$final.Sequence
        [bool]$baselineAckState = -not [bool]$manifestAckState
        if ($WorldBaselineNegativeProof) {
            $originalBaselinePacket = $final
            $originalBaselineFragment = if ($final.FragmentPresent) {
                Read-FragmentedSequencedDatagram `
                    -Packet $finalDatagram.Bytes `
                    -Description "original pending world baseline fragment"
            } else {
                $null
            }
            for ($attempt = 0;
                $attempt -lt 3 -and -not $baselineRetransmissionObserved;
                $attempt++) {
                $clientSequence++
                $baselineWithheldMoveCount++
                Send-DeltaClientPacket `
                    -Client $Client `
                    -Sequence $clientSequence `
                    -Acknowledgement $latestServerSequence `
                    -ServerReliableAcknowledgementState $manifestAckState `
                    -Payload (New-ObservedGoldSrcMovePayload `
                        -Sequence $clientSequence) `
                    -Description "duplicate move with baseline ACK withheld"
                $candidateDatagram = Receive-ProofDatagram `
                    -Client $Client `
                    -ServerProcess $ServerProcess `
                    -OutputCapture $OutputCapture `
                    -Deadline $Deadline `
                    -ServerEndpoint $ServerEndpoint `
                    -Description "pending world baseline retransmission candidate"
                $candidate = Read-SequencedDatagram `
                    -Packet $candidateDatagram.Bytes `
                    -Description "pending world baseline retransmission candidate"
                $latestServerSequence = [uint32]$candidate.Sequence
                if ($originalBaselinePacket.FragmentPresent) {
                    if (-not $candidate.FragmentPresent) {
                        Assert-NopPayload `
                            -Payload $candidate.Payload `
                            -Description "ordinary response before baseline retransmission"
                        continue
                    }
                    $candidateFragment = Read-FragmentedSequencedDatagram `
                        -Packet $candidateDatagram.Bytes `
                        -Description "retransmitted world baseline fragment"
                    if ($candidateFragment.RawFragmentId -ne
                            $originalBaselineFragment.RawFragmentId) {
                        throw "world baseline retransmission changed fragment identity"
                    }
                    Assert-ExactBytes `
                        -Actual $candidateFragment.FragmentBytes `
                        -Expected $originalBaselineFragment.FragmentBytes `
                        -Description "byte-identical world baseline fragment retransmission"
                } else {
                    if ($candidate.FragmentPresent) {
                        throw "unfragmented world baseline became fragmented during retransmission"
                    }
                    if ($candidate.Payload.Length -gt 0 -and
                        $candidate.Payload[0] -eq $svcNop) {
                        Assert-NopPayload `
                            -Payload $candidate.Payload `
                            -Description "ordinary response before baseline retransmission"
                        continue
                    }
                    Assert-ExactBytes `
                        -Actual $candidate.Payload `
                        -Expected $originalBaselinePacket.Payload `
                        -Description "byte-identical world baseline retransmission"
                }
                $baselineRetransmissionObserved = $true
                $baselineRetransmittedIdentical = $true
                $finalDatagram = $candidateDatagram
                $final = $candidate
            }
            if (-not $baselineRetransmissionObserved) {
                throw "withheld baseline ACK did not produce a retransmission"
            }
        }
        if ($final.FragmentPresent) {
            $baselineFragmented = $true
            $fragment = Read-FragmentedSequencedDatagram `
                -Packet $finalDatagram.Bytes `
                -Description "first world baseline fragment"
            $slots = New-Object object[] $fragment.FragmentCount
            while ($true) {
                if ((Add-ProbeFragment `
                        -Slots $slots `
                        -Fragment $fragment `
                        -Description "world baseline fragment") -cne
                    "accepted") {
                    throw "world baseline fragment was duplicated"
                }
                $clientSequence++
                Send-FragmentAcknowledgement `
                    -Client $Client `
                    -ClientSequence $clientSequence `
                    -ServerSequence $latestServerSequence `
                    -ReliableAcknowledgementToggle $baselineAckState `
                    -NopPayload $Handshake.NopPayload `
                    -Description "world baseline fragment acknowledgement"
                $responseDatagram = Receive-ProofDatagram `
                    -Client $Client `
                    -ServerProcess $ServerProcess `
                    -OutputCapture $OutputCapture `
                    -Deadline $Deadline `
                    -ServerEndpoint $ServerEndpoint `
                    -Description "world baseline fragment response"
                if ($fragment.FragmentIndex -eq $fragment.FragmentCount) {
                    $response = Read-SequencedDatagram `
                        -Packet $responseDatagram.Bytes `
                        -Description "world baseline final acknowledgement"
                    if ($response.FragmentPresent) {
                        throw "world baseline final ACK remained fragmented"
                    }
                    Assert-NopPayload `
                        -Payload $response.Payload `
                        -Description "world baseline final acknowledgement"
                    $latestServerSequence = [uint32]$response.Sequence
                    break
                }
                $fragment = Read-FragmentedSequencedDatagram `
                    -Packet $responseDatagram.Bytes `
                    -Description "next world baseline fragment"
                $latestServerSequence =
                    [uint32]$fragment.Datagram.Sequence
                $baselineAckState = -not $baselineAckState
            }
            $baselinePayload = Join-ProbeFragments -Slots $slots
        } else {
            $baselinePayload = [byte[]]$final.Payload
            $clientSequence++
            Send-DeltaClientPacket `
                -Client $Client `
                -Sequence $clientSequence `
                -Acknowledgement $latestServerSequence `
                -ServerReliableAcknowledgementState $baselineAckState `
                -Payload $Handshake.NopPayload `
                -Description "world baseline reliable acknowledgement"
            $responseDatagram = Receive-ProofDatagram `
                -Client $Client `
                -ServerProcess $ServerProcess `
                -OutputCapture $OutputCapture `
                -Deadline $Deadline `
                -ServerEndpoint $ServerEndpoint `
                -Description "world baseline acknowledgement response"
            $response = Read-SequencedDatagram `
                -Packet $responseDatagram.Bytes `
                -Description "world baseline acknowledgement response"
            Assert-NopPayload `
                -Payload $response.Payload `
                -Description "world baseline acknowledgement response"
            $latestServerSequence = [uint32]$response.Sequence
        }
        if ($baselinePayload.Length -lt 3 -or
            $baselinePayload[0] -ne 22 -or
            $baselinePayload[$baselinePayload.Length - 2] -ne 25 -or
            $baselinePayload[$baselinePayload.Length - 1] -ne 1) {
            throw "world baseline bundle has invalid semantic message order"
        }
        $baselineReceived = $true
        $clientSequence++
        Send-DeltaClientPacket `
            -Client $Client `
            -Sequence $clientSequence `
            -Acknowledgement $latestServerSequence `
            -ServerReliableAcknowledgementState $baselineAckState `
            -Payload (New-StringCommandPayload -Command "sendents") `
            -ReliablePayload `
            -Description "typed sendents after accepted world baseline"
        $sendEntitiesResponse = Receive-ProofDatagram `
            -Client $Client `
            -ServerProcess $ServerProcess `
            -OutputCapture $OutputCapture `
            -Deadline $Deadline `
            -ServerEndpoint $ServerEndpoint `
            -Description "sendents response"
        Wait-ForStdoutToken `
            -Process $ServerProcess `
            -OutputCapture $OutputCapture `
            -StdoutPath $StdoutPath `
            -Deadline $Deadline `
            -Token "goldsrc_sendents_accepted:" `
            -Description "typed sendents acceptance"
        if ($ReceiveFirstSnapshot) {
            $snapshotPacket = Read-SequencedDatagram `
                -Packet $sendEntitiesResponse.Bytes `
                -Description "first world snapshot"
            if ($snapshotPacket.ReliableToggle -or
                $snapshotPacket.FragmentPresent -or
                $snapshotPacket.Acknowledgement -ne $clientSequence) {
                throw "first world snapshot did not use one ordinary unreliable carrier"
            }
            $snapshot = Read-FirstSnapshotPayload `
                -Payload $snapshotPacket.Payload `
                -FrameId ([uint32]$snapshotPacket.Sequence)
            $latestServerSequence = [uint32]$snapshotPacket.Sequence
            if ($FirstSnapshotNegativeValidation) {
                $clientSequence++
                [byte[]]$snapshotKeepalive =
                    New-ObservedGoldSrcMovePayload `
                        -Sequence $clientSequence
                Send-DeltaClientPacket `
                    -Client $Client `
                    -Sequence $clientSequence `
                    -Acknowledgement $latestServerSequence `
                    -ServerReliableAcknowledgementState $baselineAckState `
                    -Payload $snapshotKeepalive `
                    -Description "withheld first-snapshot reference keepalive"
                $keepaliveDatagram = Receive-ProofDatagram `
                    -Client $Client `
                    -ServerProcess $ServerProcess `
                    -OutputCapture $OutputCapture `
                    -Deadline $Deadline `
                    -ServerEndpoint $ServerEndpoint `
                    -Description "withheld first-snapshot reference response"
                $keepaliveResponse = Read-SequencedDatagram `
                    -Packet $keepaliveDatagram.Bytes `
                    -Description "withheld first-snapshot reference response"
                Assert-NopPayload `
                    -Payload $keepaliveResponse.Payload `
                    -Description "withheld first-snapshot reference response"
                $latestServerSequence =
                    [uint32]$keepaliveResponse.Sequence
                Wait-ForStdoutToken `
                    -Process $ServerProcess `
                    -OutputCapture $OutputCapture `
                    -StdoutPath $StdoutPath `
                    -Deadline $Deadline `
                    -Token "goldsrc_first_snapshot_keepalive:" `
                    -Description "unreferenced snapshot keepalive"
                $firstSnapshotKeepaliveObserved = $true

                $referenceCases = @(
                    [pscustomobject]@{
                        Wire = [byte](($snapshot.FrameId + 1) -band 0xFF)
                        Result = "future"
                    },
                    [pscustomobject]@{
                        Wire = [byte](($snapshot.FrameId - 1) -band 0xFF)
                        Result = "evicted"
                    },
                    [pscustomobject]@{
                        Wire = [byte]($snapshot.FrameId -band 0xFF)
                        Result = "acknowledged"
                    },
                    [pscustomobject]@{
                        Wire = [byte]($snapshot.FrameId -band 0xFF)
                        Result = "duplicate"
                    }
                )
                $lastReferenceResponse = $snapshotPacket
                foreach ($referenceCase in $referenceCases) {
                    $clientSequence++
                    [byte[]]$frameReference = @(
                        [byte]4,
                        [byte]$referenceCase.Wire
                    )
                    Send-DeltaClientPacket `
                        -Client $Client `
                        -Sequence $clientSequence `
                        -Acknowledgement $latestServerSequence `
                        -ServerReliableAcknowledgementState $baselineAckState `
                        -Payload $frameReference `
                        -Description (
                            "{0} first-snapshot frame reference" -f
                            $referenceCase.Result)
                    $referenceDatagram = Receive-ProofDatagram `
                        -Client $Client `
                        -ServerProcess $ServerProcess `
                        -OutputCapture $OutputCapture `
                        -Deadline $Deadline `
                        -ServerEndpoint $ServerEndpoint `
                        -Description (
                            "{0} frame-reference response" -f
                            $referenceCase.Result)
                    $lastReferenceResponse = Read-SequencedDatagram `
                        -Packet $referenceDatagram.Bytes `
                        -Description (
                            "{0} frame-reference response" -f
                            $referenceCase.Result)
                    Assert-NopPayload `
                        -Payload $lastReferenceResponse.Payload `
                        -Description (
                            "{0} frame-reference response" -f
                            $referenceCase.Result)
                    $latestServerSequence =
                        [uint32]$lastReferenceResponse.Sequence
                    Wait-ForStdoutToken `
                        -Process $ServerProcess `
                        -OutputCapture $OutputCapture `
                        -StdoutPath $StdoutPath `
                        -Deadline $Deadline `
                        -Token (
                            "result={0},source=clc_delta" -f
                            $referenceCase.Result) `
                        -Description (
                            "{0} frame-reference diagnostic" -f
                            $referenceCase.Result)
                }
                $final = $lastReferenceResponse
            } else {
                $clientSequence++
                [byte[]]$frameReference = @(
                    [byte]4,
                    [byte]($snapshot.FrameId -band 0xFF)
                )
                Send-DeltaClientPacket `
                    -Client $Client `
                    -Sequence $clientSequence `
                    -Acknowledgement $latestServerSequence `
                    -ServerReliableAcknowledgementState $baselineAckState `
                    -Payload $frameReference `
                    -Description "validated first-snapshot client frame reference"
                Wait-ForStdoutToken `
                    -Process $ServerProcess `
                    -OutputCapture $OutputCapture `
                    -StdoutPath $StdoutPath `
                    -Deadline $Deadline `
                    -Token "result=acknowledged,source=clc_delta" `
                    -Description "first snapshot frame acknowledgement"
                $final = $snapshotPacket
            }
        } else {
            $sendEntitiesPacket = Read-SequencedDatagram `
                -Packet $sendEntitiesResponse.Bytes `
                -Description "sendents acknowledgement response"
            Assert-NopPayload `
                -Payload $sendEntitiesPacket.Payload `
                -Description "sendents acknowledgement response"
            $final = $sendEntitiesPacket
        }
    }
    return [pscustomobject]@{
        Manifest = $manifest
        ClientSequence = [uint32]$clientSequence
        LatestServerSequence = [uint32]$final.Sequence
        ServerReliableAcknowledgementState = [bool]$manifestAckState
        PostResourceMode = $PostResourceMode
        BaselineReceived = $baselineReceived
        BaselineFragmented = $baselineFragmented
        BaselineRetransmissionObserved = $baselineRetransmissionObserved
        BaselineRetransmittedIdentical = $baselineRetransmittedIdentical
        BaselineWithheldMoveCount = $baselineWithheldMoveCount
        FirstSnapshotReceived = ($null -ne $snapshot)
        FirstSnapshot = $snapshot
        FirstSnapshotKeepaliveObserved =
            $firstSnapshotKeepaliveObserved
    }
}

function Assert-DeltaHostSummaries {
    param(
        [string]$Stdout,
        [ValidateSet("Positive", "Negative")]
        [string]$Mode,
        $Bootstrap,
        $Resource
    )

    $negative = $Mode -eq "Negative"
    $postResource = $Resource.PostResourceMode -ne "None"
    $expectedSignonPhase = if ($Resource.FirstSnapshotReceived) {
        "first_snapshot_acknowledged"
    } elseif ($Resource.PostResourceMode -eq "World") {
        "awaiting_first_snapshot"
    } elseif ($postResource) {
        "awaiting_server_baseline_or_snapshot"
    } else {
        "resource_manifest_acknowledged"
    }
    $deltaLine = Get-ExactlyOneSummaryLine `
        -Stdout $Stdout `
        -Prefix "goldsrc_delta_description_summary:"
    Assert-SummaryFields `
        -Line $deltaLine `
        -Description "goldsrc_delta_description_summary" `
        -Expected ([ordered]@{
            enabled = "1"
            negative_proof = $(if ($negative) { "1" } else { "0" })
            source = "synthetic_fixture"
            registry_built = "true"
            preparation_attempts = "1"
            cached_outcome_reuses = "0"
            table_count = "7"
            usercmd_table_present = "true"
            delta_queued = "1"
            delta_sent = "1"
            delta_acked = "1"
            bootstrap_queued = "1"
            bootstrap_sent = "1"
            bootstrap_acked = "1"
            fragmented = $(if ($Bootstrap.FragmentCount -gt 0) {
                "true"
            } else {
                "false"
            })
            fragment_count = $(if ($Bootstrap.FragmentCount -gt 0) {
                [string]$Bootstrap.FragmentCount
            } else {
                "0"
            })
            fragment_acknowledged_count = $(if (
                    $Bootstrap.FragmentCount -gt 0) {
                [string]$Bootstrap.FragmentCount
            } else {
                "0"
            })
            slot_resets = $(if ($negative) { "1" } else { "0" })
            pending_state_cleared_on_reset =
                $(if ($negative) { "true" } else { "false" })
            fresh_sessions_after_reset =
                $(if ($negative) { "1" } else { "0" })
            slot_reused_after_reset =
                $(if ($negative) { "true" } else { "false" })
            signon_phase = $expectedSignonPhase
            session_count = "1"
            put_in_server = "0"
            spawned = "0"
            active = "0"
            clean_shutdown = "1"
        })
    if ((Get-StableUnsignedField `
            -Line $deltaLine `
            -Name "field_count") -ne $Bootstrap.Decoded.DeltaBundle.FieldCount) {
        throw "runtime and wire delta field counts differ"
    }
    if ((Get-StableUnsignedField `
            -Line $deltaLine `
            -Name "delta_bundle_bytes") -ne
        $Bootstrap.Decoded.DeltaBytes.Length) {
        throw "runtime and wire delta bundle byte counts differ"
    }
    if ((Get-StableUnsignedField `
            -Line $deltaLine `
            -Name "bootstrap_bytes") -ne
        $Bootstrap.Decoded.Payload.Length) {
        throw "runtime and wire combined bootstrap byte counts differ"
    }
    if ((Get-StableUnsignedField `
            -Line $deltaLine `
            -Name "bootstrap_tail_bytes") -ne
        $Bootstrap.Decoded.BootstrapTail.Bytes.Length) {
        throw "runtime and wire bootstrap-tail byte counts differ"
    }
    if ($negative) {
        if ((Get-StableUnsignedField `
                -Line $deltaLine `
                -Name "bootstrap_resent") -lt 1) {
            throw "negative delta proof recorded no bootstrap retransmission"
        }
        if ((Get-StableUnsignedField `
                -Line $deltaLine `
                -Name "carrier_send_count") -le $Bootstrap.FragmentCount) {
            throw "negative delta proof carrier count does not include a retransmission"
        }
    }

    $serverInfoLine = Get-ExactlyOneSummaryLine `
        -Stdout $Stdout `
        -Prefix "goldsrc_serverinfo_summary:"
    Assert-SummaryFields `
        -Line $serverInfoLine `
        -Description "goldsrc_serverinfo_summary" `
        -Expected ([ordered]@{
            enabled = "1"
            client_new_delivered = "1"
            duplicate_new_deliveries = "0"
            serverinfo_context_built = "1"
            serverinfo_generations = "1"
            serverinfo_queued = "1"
            serverinfo_acked = "1"
            signon_phase = $expectedSignonPhase
            session_count = "1"
            put_in_server = "0"
            spawned = "0"
            active = "0"
            clean_shutdown = "1"
        })
    if ($negative) {
        Assert-SummaryBoolean `
            -Line $serverInfoLine `
            -Name "wrong_reliable_ack_rejected" `
            -Expected $true `
            -Description "goldsrc_serverinfo_summary"
        Assert-SummaryBoolean `
            -Line $serverInfoLine `
            -Name "duplicate_client_reliable_suppressed" `
            -Expected $true `
            -Description "goldsrc_serverinfo_summary"
    }

    $resourceLine = Get-ExactlyOneSummaryLine `
        -Stdout $Stdout `
        -Prefix "goldsrc_resource_manifest_summary:"
    Assert-SummaryFields `
        -Line $resourceLine `
        -Description "goldsrc_resource_manifest_summary" `
        -Expected ([ordered]@{
            enabled = "1"
            observed_continuation_received = "true"
            observed_continuation_deliveries = "1"
            close_menus_companions_accepted = "2"
            resource_request_deliveries = "1"
            resource_manifest_preparation_attempts = "1"
            resource_manifest_context_built = "1"
            resource_manifest_generations = "1"
            resource_manifest_queued = "1"
            resource_manifest_sent = "1"
            resource_manifest_acked = "1"
            resource_manifest_entry_count = [string]$Resource.Manifest.ResourceCount
            resource_manifest_payload_bytes = [string]$Resource.Manifest.PayloadBytes
            signon_phase = $expectedSignonPhase
            session_count = "1"
            put_in_server = "0"
            spawned = "0"
            active = "0"
            clean_shutdown = "1"
        })

    $netchanLine = Get-ExactlyOneSummaryLine `
        -Stdout $Stdout `
        -Prefix "goldsrc_netchan_summary:"
    Assert-SummaryFields `
        -Line $netchanLine `
        -Description "goldsrc_netchan_summary" `
        -Expected ([ordered]@{
            enabled = "1"
            initialized = "1"
            netchan_state = "established"
            reliable_pending_bytes = "0"
            session_count = "1"
            state = "connected"
            put_in_server = "0"
            spawned = "0"
            active = "0"
            clean_shutdown = "1"
        })
    if ($Resource.PostResourceMode -eq "Negative") {
        if ((Get-StableUnsignedField `
                -Line $netchanLine `
                -Name "duplicate_rejected") -lt 1) {
            throw "post-resource proof recorded no duplicate outer rejection"
        }
        if ((Get-StableUnsignedField `
                -Line $netchanLine `
                -Name "out_of_order_rejected") -lt 1) {
            throw "post-resource proof recorded no out-of-order rejection"
        }
    }

    if ($postResource) {
        $postResourceLine = Get-ExactlyOneSummaryLine `
            -Stdout $Stdout `
            -Prefix "goldsrc_post_resource_command_summary:"
        Assert-SummaryFields `
            -Line $postResourceLine `
            -Description "goldsrc_post_resource_command_summary" `
            -Expected ([ordered]@{
                opcode = "2"
                identity = "clc_move"
                reliability = "unreliable"
                decoded = $(if (
                    $Resource.FirstSnapshotKeepaliveObserved) {
                    "2"
                } else {
                    "1"
                })
                delivered = "1"
                rejected = $(if ($Resource.PostResourceMode -eq "Negative") {
                    "5"
                } elseif ($negative -and
                    $Resource.PostResourceMode -eq "World") {
                    [string]$Resource.BaselineWithheldMoveCount
                } else {
                    "0"
                })
                checksum_rejected =
                    $(if ($Resource.PostResourceMode -eq "Negative") {
                        "1"
                    } else {
                        "0"
                    })
                count_rejected =
                    $(if ($Resource.PostResourceMode -eq "Negative") {
                        "1"
                    } else {
                        "0"
                    })
                truncated_rejected =
                    $(if ($Resource.PostResourceMode -eq "Negative") {
                        "1"
                    } else {
                        "0"
                    })
                invalid_phase_rejected =
                    $(if ($Resource.PostResourceMode -eq "Negative") {
                        "1"
                    } elseif ($negative -and
                        $Resource.PostResourceMode -eq "World") {
                        [string]$Resource.BaselineWithheldMoveCount
                    } else {
                        "0"
                    })
                state_advances = "1"
                checksum_validated = "true"
                delta_schema_used = "true"
                pre_spawn_ignored = "true"
                previous_boundary_resolved = "true"
                pending_reliable_preserved =
                    $(if ($Resource.PostResourceMode -eq "Negative" -or
                        ($negative -and
                            $Resource.PostResourceMode -eq "World")) {
                        "true"
                    } else {
                        "false"
                    })
                packet_loss = "0"
                backup_commands = "2"
                new_commands = "1"
                total_commands = "3"
                new_command_msec = "32"
                continuation = "awaiting_server_baseline_or_snapshot"
                signon_phase = $expectedSignonPhase
                session_count = "1"
                put_in_server = "0"
                spawned = "0"
                active = "0"
                server_still_responsive = "true"
                clean_shutdown = "1"
            })
    }
    if ($Resource.PostResourceMode -eq "World") {
        if (-not $Resource.BaselineReceived) {
            throw "world baseline proof did not receive a baseline bundle"
        }
        $baselineLine = Get-ExactlyOneSummaryLine `
            -Stdout $Stdout `
            -Prefix "goldsrc_world_baseline_summary:"
        Assert-SummaryFields `
            -Line $baselineLine `
            -Description "goldsrc_world_baseline_summary" `
            -Expected ([ordered]@{
                enabled = "1"
                negative_proof = $(if ($negative) { "1" } else { "0" })
                contract_verified = "true"
                message_order_verified = "true"
                source = "runtime_map"
                build_attempts = "1"
                generations = "1"
                cached_reuses = "0"
                instanced_callback_calls = "1"
                instance_count = "0"
                queued = "1"
                sent = "1"
                acked = "1"
                sendents_received = "1"
                sendents_delivered = "1"
                previous_boundary_resolved = "true"
                next_boundary = $(if ($Resource.FirstSnapshotReceived) {
                    "continuous_snapshot_cadence_required"
                } else {
                    "first_snapshot_required"
                })
                signon_phase = $expectedSignonPhase
                session_count = "1"
                put_in_server = "0"
                spawned = "0"
                active = "0"
                clean_shutdown = "1"
            })
        if ((Get-StableUnsignedField `
                -Line $baselineLine `
                -Name "entity_count") -lt 2) {
            throw "world baseline proof omitted world or player template"
        }
        if ((Get-StableUnsignedField `
                -Line $baselineLine `
                -Name "callback_calls") -ne
            (Get-StableUnsignedField `
                -Line $baselineLine `
                -Name "entity_count")) {
            throw "world baseline callback count does not match frozen entities"
        }
        if ($negative) {
            if (-not $Resource.BaselineRetransmissionObserved -or
                -not $Resource.BaselineRetransmittedIdentical) {
                throw "world baseline negative proof omitted byte-identical retransmission"
            }
            if ((Get-StableUnsignedField `
                    -Line $baselineLine `
                    -Name "resent") -lt 1) {
                throw "runtime did not report a retained baseline retransmission"
            }
        }
    }
    if ($Resource.FirstSnapshotReceived) {
        $snapshotLine = Get-ExactlyOneSummaryLine `
            -Stdout $Stdout `
            -Prefix "goldsrc_first_snapshot_summary:"
        Assert-SummaryFields `
            -Line $snapshotLine `
            -Description "goldsrc_first_snapshot_summary" `
            -Expected ([ordered]@{
                enabled = "1"
                negative_proof = $(if ($negative) { "1" } else { "0" })
                contract_verified = "true"
                message_order_verified = "true"
                frame_ack_contract_verified = "true"
                reliability = "unreliable"
                clientdata = "implemented"
                weapon_data = "not_required"
                packet_entities = "implemented"
                entity_count =
                    [string]$Resource.FirstSnapshot.EntityCount
                frame_id = [string]$Resource.FirstSnapshot.FrameId
                visibility_policy =
                    "runtime_map_modeled_nonplayer_baselines_no_pvs"
                frame_history_implemented = "true"
                frame_history_depth = "64"
                frame_ack_source = "clc_delta_low8_server_frame"
                prepared = "1"
                sent = "1"
                acked = "1"
                duplicate_ack = $(if ($negative) { "1" } else { "0" })
                future_frame = $(if ($negative) { "1" } else { "0" })
                evicted_frame = $(if ($negative) { "1" } else { "0" })
                previous_boundary_resolved = "true"
                advanced_past_previous_boundary = "true"
                next_boundary = "continuous_snapshot_cadence_required"
                signon_phase = "first_snapshot_acknowledged"
                session_count = "1"
                put_in_server = "0"
                spawned = "0"
                active = "0"
                server_still_responsive = "true"
                clean_shutdown = "1"
            })
        if ((Get-StableUnsignedField `
                -Line $snapshotLine `
                -Name "payload_bytes") -le 0) {
            throw "runtime reported an empty first snapshot"
        }
    }

    $udpLine = Get-ExactlyOneSummaryLine `
        -Stdout $Stdout `
        -Prefix "goldsrc_udp_summary:"
    Assert-SummaryFields `
        -Line $udpLine `
        -Description "goldsrc_udp_summary" `
        -Expected ([ordered]@{
            enabled = "1"
            ready = "1"
            challenges = $(if ($negative) { "2" } else { "1" })
            connects = $(if ($negative) { "2" } else { "1" })
            accepted = $(if ($negative) { "2" } else { "1" })
            rejected = "0"
            last_reject_reason = "<none>"
            session_count = "1"
            state = "connected"
            clean_shutdown = "1"
        })

    if ($negative) {
        $resetLines = @(
            $Stdout -split '\r?\n' |
                Where-Object {
                    $_.IndexOf(
                        "goldsrc_delta_session_reset:",
                        [StringComparison]::Ordinal) -ge 0
                }
        )
        if ($resetLines.Count -ne 1 -or
            -not (Test-StableField `
                -Line $resetLines[0] `
                -Name "pending_before_reset" `
                -ExpectedValue "true") -or
            -not (Test-StableField `
                -Line $resetLines[0] `
                -Name "state_cleared" `
                -ExpectedValue "true")) {
            throw "negative delta proof lacks one exact pending-state reset"
        }
        $freshLines = @(
            $Stdout -split '\r?\n' |
                Where-Object {
                    $_.IndexOf(
                        "goldsrc_delta_fresh_session:",
                        [StringComparison]::Ordinal) -ge 0
                }
        )
        if ($freshLines.Count -ne 1 -or
            -not (Test-StableField `
                -Line $freshLines[0] `
                -Name "slot_reused" `
                -ExpectedValue "true") -or
            -not (Test-StableField `
                -Line $freshLines[0] `
                -Name "session_count" `
                -ExpectedValue "1")) {
            throw "negative delta proof lacks one exact same-process slot reuse"
        }
        $admissionResetLines = @(
            $Stdout -split '\r?\n' |
                Where-Object {
                    $_.IndexOf(
                        "goldsrc_signon_reset:",
                        [StringComparison]::Ordinal) -ge 0 -and
                    $_.IndexOf(
                        "reason=admission",
                        [StringComparison]::Ordinal) -ge 0
                }
        )
        if ($admissionResetLines.Count -ne 2) {
            throw ("negative delta proof expected two same-host admission resets, found {0}" -f
                $admissionResetLines.Count)
        }
    }
}

function Get-DeltaProofRepositorySnapshot {
    param([string]$RepositoryRoot)

    $excludedRoots = New-Object 'System.Collections.Generic.HashSet[string]' `
        ([StringComparer]::OrdinalIgnoreCase)
    foreach ($name in @(
        ".git",
        ".vs",
        "build",
        "build32",
        "build-codex-p240",
        "logs",
        "out"
    )) {
        [void]$excludedRoots.Add($name)
    }
    $snapshot = New-Object 'System.Collections.Generic.Dictionary[string,string]' `
        ([StringComparer]::OrdinalIgnoreCase)
    $files = @(
        Get-ChildItem `
            -LiteralPath $RepositoryRoot `
            -Recurse `
            -File `
            -ErrorAction SilentlyContinue |
        Where-Object {
            $relative = $_.FullName.Substring($RepositoryRoot.Length).
                TrimStart('\', '/')
            $firstSeparator = $relative.IndexOfAny([char[]]@('\', '/'))
            $rootName = if ($firstSeparator -lt 0) {
                $relative
            } else {
                $relative.Substring(0, $firstSeparator)
            }
            -not $excludedRoots.Contains($rootName)
        }
    )
    foreach ($file in $files) {
        $relativePath = $file.FullName.Substring($RepositoryRoot.Length).
            TrimStart('\', '/')
        $snapshot[$relativePath] = "{0}:{1}" -f
            $file.Length,
            $file.LastWriteTimeUtc.Ticks
    }
    return ,$snapshot
}

function Assert-NoDeltaProofRepositoryMutation {
    param(
        [string]$RepositoryRoot,
        [System.Collections.Generic.Dictionary[string,string]]$Before
    )

    $after = Get-DeltaProofRepositorySnapshot -RepositoryRoot $RepositoryRoot
    $mutated = New-Object 'System.Collections.Generic.List[string]'
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
        throw ("hlhost mutated repository source files during the proof: {0}" -f
            ($mutated -join ", "))
    }
}

function Invoke-DeltaHostRun {
    param(
        [ValidateSet("Positive", "Negative")]
        [string]$Mode,
        [string]$ResolvedExecutablePath,
        [string]$ResolvedGameDir,
        [string]$ResolvedDeltaFixture,
        [string]$ResolvedManifestFixture,
        [object[]]$ExpectedEntries,
        [string]$ExpectedClientDllMd5,
        [string]$RepositoryRoot,
        [string]$Address,
        [int]$RequestedPort,
        [int]$RunTimeoutSeconds,
        [ValidateSet("None", "Positive", "Negative", "World")]
        [string]$PostResourceMode = "None",
        [switch]$RunFirstSnapshotProof,
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
        "--goldsrc-delta-descriptions",
        "--goldsrc-delta-descriptions-fixture", $ResolvedDeltaFixture,
        "--goldsrc-resource-manifest",
        "--goldsrc-resource-manifest-fixture", $ResolvedManifestFixture,
        "--goldsrc-handshake-timeout-ms", ([string]$handshakeTimeoutMilliseconds)
    )
    if ($Mode -eq "Negative") {
        $arguments += "--goldsrc-delta-descriptions-negative-proof"
    }
    if ($PostResourceMode -eq "World") {
        $arguments += "--goldsrc-world-baselines"
        if ($RunFirstSnapshotProof) {
            $arguments += "--goldsrc-first-snapshot"
            if ($Mode -eq "Negative") {
                $arguments += "--goldsrc-first-snapshot-negative-proof"
            }
        }
        if ($Mode -eq "Negative") {
            $arguments += "--goldsrc-world-baselines-negative-proof"
        }
    }
    $argumentLine = (($arguments | ForEach-Object {
        ConvertTo-WindowsCommandLineArgument -Value ([string]$_)
    }) -join " ")

    $tempPrefix = "hlhost_goldsrc_delta_{0}_{1}" -f
        $Mode.ToLowerInvariant(),
        ([Guid]::NewGuid().ToString("N"))
    $stdoutPath = Join-Path `
        ([System.IO.Path]::GetTempPath()) `
        ($tempPrefix + ".stdout.log")
    $stderrPath = Join-Path `
        ([System.IO.Path]::GetTempPath()) `
        ($tempPrefix + ".stderr.log")
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
    $portOwnershipProof = "not_checked"
    $childProcessProof = "not_checked"
    $handshake = $null
    $firstHandshake = $null
    $bootstrap = $null
    $pendingBootstrap = $null
    $resetProof = $null
    $resource = $null

    try {
        $startInfo = New-Object System.Diagnostics.ProcessStartInfo
        $startInfo.FileName = $ResolvedExecutablePath
        $startInfo.Arguments = $argumentLine
        $startInfo.WorkingDirectory = $RepositoryRoot
        $startInfo.UseShellExecute = $false
        $startInfo.CreateNoWindow = $true
        $startInfo.WindowStyle =
            [System.Diagnostics.ProcessWindowStyle]::Hidden
        $startInfo.RedirectStandardOutput = $true
        $startInfo.RedirectStandardError = $true

        $serverProcess = New-Object System.Diagnostics.Process
        $serverProcess.StartInfo = $startInfo
        if (-not $serverProcess.Start()) {
            throw "failed to start delta-proof hlhost"
        }
        $processStarted = $true
        $serverProcessId = $serverProcess.Id
        if ($serverProcessId -le 0) {
            throw "delta-proof hlhost process ID is unavailable"
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
            -ExpectedAddress $Address `
            -ExpectedPort $selectedPort)
        $portOwnershipProof = Wait-ForUdpPortOwnership `
            -Process $serverProcess `
            -Deadline $deadline `
            -ExpectedAddress $Address `
            -ExpectedPort $selectedPort `
            -ExpectedProcessId $serverProcessId

        $probeClient = New-Object System.Net.Sockets.UdpClient(
            [System.Net.Sockets.AddressFamily]::InterNetwork
        )
        $probeClient.Client.Bind((New-Object System.Net.IPEndPoint(
            [System.Net.IPAddress]::Loopback,
            0
        )))
        $serverEndpoint = New-Object System.Net.IPEndPoint(
            [System.Net.IPAddress]::Parse($Address),
            $selectedPort
        )
        $probeClient.Connect($serverEndpoint)
        $handshake = Invoke-DeltaInitialHandshake `
            -Client $probeClient `
            -ServerProcess $serverProcess `
            -OutputCapture $outputCapture `
            -Deadline $deadline `
            -ServerEndpoint $serverEndpoint

        if ($Mode -eq "Positive") {
            $bootstrap = Invoke-UnfragmentedDeltaBootstrap `
                -Client $probeClient `
                -ServerProcess $serverProcess `
                -OutputCapture $outputCapture `
                -StdoutPath $stdoutPath `
                -Deadline $deadline `
                -ServerEndpoint $serverEndpoint `
                -Handshake $handshake `
                -ExpectedClientDllMd5 $ExpectedClientDllMd5
        } else {
            $firstHandshake = $handshake
            $pendingBootstrap = Invoke-FragmentedNegativeDeltaBootstrap `
                -Client $probeClient `
                -ServerProcess $serverProcess `
                -OutputCapture $outputCapture `
                -StdoutPath $stdoutPath `
                -Deadline $deadline `
                -ServerEndpoint $serverEndpoint `
                -Handshake $handshake `
                -ExpectedClientDllMd5 $ExpectedClientDllMd5 `
                -StopAfterRetransmission
            $resetProof = Invoke-PendingDeltaDisconnect `
                -Client $probeClient `
                -ServerProcess $serverProcess `
                -OutputCapture $outputCapture `
                -StdoutPath $stdoutPath `
                -Deadline $deadline `
                -ServerEndpoint $serverEndpoint `
                -PendingBootstrap $pendingBootstrap

            $probeClient.Close()
            $probeClient.Dispose()
            $probeClient = $null
            for ($attempt = 0; $attempt -lt 8 -and $null -eq $probeClient;
                $attempt++) {
                $candidateClient = New-Object System.Net.Sockets.UdpClient(
                    [System.Net.Sockets.AddressFamily]::InterNetwork
                )
                $candidateClient.Client.Bind((
                    New-Object System.Net.IPEndPoint(
                        [System.Net.IPAddress]::Loopback,
                        0
                    )
                ))
                $candidatePort = (
                    [System.Net.IPEndPoint]$candidateClient.Client.LocalEndPoint
                ).Port
                if ($candidatePort -eq $firstHandshake.ClientPort) {
                    $candidateClient.Close()
                    $candidateClient.Dispose()
                } else {
                    $probeClient = $candidateClient
                }
            }
            if ($null -eq $probeClient) {
                throw "failed to bind a distinct fresh loopback client endpoint"
            }
            $probeClient.Connect($serverEndpoint)
            $handshake = Invoke-DeltaInitialHandshake `
                -Client $probeClient `
                -ServerProcess $serverProcess `
                -OutputCapture $outputCapture `
                -Deadline $deadline `
                -ServerEndpoint $serverEndpoint
            if ($handshake.ClientPort -eq $firstHandshake.ClientPort) {
                throw "fresh delta session reused the disconnected UDP endpoint"
            }
            Wait-ForStdoutToken `
                -Process $serverProcess `
                -OutputCapture $outputCapture `
                -StdoutPath $stdoutPath `
                -Deadline $deadline `
                -Token "goldsrc_delta_fresh_session:" `
                -Description "same-process fresh delta session diagnostic"

            $bootstrap = Invoke-FragmentedNegativeDeltaBootstrap `
                -Client $probeClient `
                -ServerProcess $serverProcess `
                -OutputCapture $outputCapture `
                -StdoutPath $stdoutPath `
                -Deadline $deadline `
                -ServerEndpoint $serverEndpoint `
                -Handshake $handshake `
                -ExpectedClientDllMd5 $ExpectedClientDllMd5
            $duplicateProgress = Invoke-DuplicateDeltaTrigger `
                -Client $probeClient `
                -ServerProcess $serverProcess `
                -OutputCapture $outputCapture `
                -StdoutPath $stdoutPath `
                -Deadline $deadline `
                -ServerEndpoint $serverEndpoint `
                -Handshake $handshake `
                -Bootstrap $bootstrap
            $bootstrap.ClientSequence = $duplicateProgress.ClientSequence
            $bootstrap.LatestServerSequence =
                $duplicateProgress.LatestServerSequence
        }

        $resource = Invoke-ObservedResourceContinuation `
            -Client $probeClient `
            -ServerProcess $serverProcess `
            -OutputCapture $outputCapture `
            -Deadline $deadline `
            -ServerEndpoint $serverEndpoint `
            -Handshake $handshake `
            -Bootstrap $bootstrap `
            -ExpectedEntries $ExpectedEntries `
            -StdoutPath $stdoutPath `
            -PostResourceMode $PostResourceMode `
            -WorldBaselineNegativeProof:(
                $Mode -eq "Negative" -and
                $PostResourceMode -eq "World") `
            -ReceiveFirstSnapshot:$RunFirstSnapshotProof `
            -FirstSnapshotNegativeValidation:(
                $Mode -eq "Negative" -and $RunFirstSnapshotProof)

        Wait-ForCleanServerExit `
            -Process $serverProcess `
            -OutputCapture $outputCapture `
            -Deadline $deadline
        if ($serverProcess.ExitCode -ne 0) {
            throw ("delta-proof hlhost exited with code {0}" -f
                $serverProcess.ExitCode)
        }
        if (Test-ProcessIdAlive -ProcessId $serverProcessId) {
            throw ("delta-proof hlhost process {0} remained alive" -f
                $serverProcessId)
        }
        $childProcessProof = Assert-NoDirectChildProcessLeak `
            -ParentProcessId $serverProcessId
        $capturedStdout = Get-SharedFileText -Path $stdoutPath
        $capturedStderr = Get-SharedFileText -Path $stderrPath
        if ($Mode -eq "Negative") {
            $sessionLines = @(
                $capturedStdout -split '\r?\n' |
                    Where-Object {
                        $_.IndexOf(
                            "goldsrc_udp_session:",
                            [StringComparison]::Ordinal) -ge 0
                    }
            )
            if ($sessionLines.Count -ne 2) {
                throw ("same-process reset proof expected two admitted-session records, found {0}" -f
                    $sessionLines.Count)
            }
            Assert-SessionSummary `
                -Stdout $sessionLines[0] `
                -ExpectedEndpoint ("127.0.0.1:{0}" -f
                    $firstHandshake.ClientPort) `
                -ExpectedChallenge $firstHandshake.Challenge `
                -ExpectedChannelIdentifier $firstHandshake.ClientPort
            Assert-SessionSummary `
                -Stdout $sessionLines[1] `
                -ExpectedEndpoint ("127.0.0.1:{0}" -f
                    $handshake.ClientPort) `
                -ExpectedChallenge $handshake.Challenge `
                -ExpectedChannelIdentifier $handshake.ClientPort
        } else {
            Assert-SessionSummary `
                -Stdout $capturedStdout `
                -ExpectedEndpoint ("127.0.0.1:{0}" -f
                    $handshake.ClientPort) `
                -ExpectedChallenge $handshake.Challenge `
                -ExpectedChannelIdentifier $handshake.ClientPort
        }
        Assert-DeltaHostSummaries `
            -Stdout $capturedStdout `
            -Mode $Mode `
            -Bootstrap $bootstrap `
            -Resource $resource
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
                    Stop-Process `
                        -Id $serverProcessId `
                        -Force `
                        -ErrorAction Stop
                    [void]$serverProcess.WaitForExit(2000)
                }
                if (-not $serverProcess.HasExited) {
                    $cleanupFailure =
                        "delta-proof hlhost remained alive after termination"
                }
            }
            catch {
                $cleanupFailure =
                    "failed to terminate delta-proof hlhost: $($_.Exception.Message)"
            }
        }
        if ($null -ne $outputCapture) {
            try {
                Complete-ProcessOutputCapture -State $outputCapture
            }
            catch {
                if ($null -eq $cleanupFailure) {
                    $cleanupFailure =
                        "failed to drain delta-proof output: $($_.Exception.Message)"
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
        if (-not $SuppressServerOutput) {
            if (-not [string]::IsNullOrWhiteSpace($capturedStdout)) {
                Write-Host $capturedStdout.TrimEnd(
                    [char[]]@([char]13, [char]10)
                )
            }
            if (-not [string]::IsNullOrWhiteSpace($capturedStderr)) {
                Write-Host $capturedStderr.TrimEnd(
                    [char[]]@([char]13, [char]10)
                )
            }
        }
        foreach ($tempPath in @($stdoutPath, $stderrPath)) {
            if (Test-Path -LiteralPath $tempPath -PathType Leaf) {
                Remove-Item `
                    -LiteralPath $tempPath `
                    -Force `
                    -ErrorAction SilentlyContinue
            }
            if (Test-Path -LiteralPath $tempPath -PathType Leaf) {
                $cleanupFailure = "temporary delta-proof output was not removed"
            }
        }
        if ($null -ne $serverProcess) {
            $serverProcess.Dispose()
        }
    }

    if ($null -ne $cleanupFailure -and $null -eq $failure) {
        $failure = New-Object System.Management.Automation.ErrorRecord(
            (New-Object System.InvalidOperationException($cleanupFailure)),
            "GoldsrcDeltaDescriptionProbeCleanupFailure",
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
        ClientPort = $handshake.ClientPort
        UdpOwner = $portOwnershipProof
        ChildProcessCheck = $childProcessProof
        Handshake = $handshake
        FirstHandshake = $firstHandshake
        Bootstrap = $bootstrap
        PendingBootstrap = $pendingBootstrap
        ResetProof = $resetProof
        Resource = $resource
        Stdout = $capturedStdout
    }
}

function Invoke-RejectedDeltaFixtureHostRun {
    param(
        [ValidateSet("MissingUsercmd", "Malformed")]
        [string]$Mode,
        [string]$ResolvedExecutablePath,
        [string]$ResolvedGameDir,
        [string]$ResolvedDeltaFixture,
        [string]$RepositoryRoot,
        [string]$Address,
        [int]$RunTimeoutSeconds,
        [switch]$SuppressServerOutput
    )

    $selectedPort = Reserve-LoopbackUdpPort
    $expectedReason = if ($Mode -eq "MissingUsercmd") {
        "goldsrc_delta_description_failed: reason=missing-usercmd-table"
    } else {
        "goldsrc_delta_description_failed: reason=unknown_field_type"
    }
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
        "--goldsrc-delta-descriptions",
        "--goldsrc-delta-descriptions-fixture", $ResolvedDeltaFixture,
        "--goldsrc-handshake-timeout-ms", ([string]([Math]::Min(
            60000,
            [Math]::Max(1000, ($RunTimeoutSeconds * 1000))
        )))
    )
    $argumentLine = (($arguments | ForEach-Object {
        ConvertTo-WindowsCommandLineArgument -Value ([string]$_)
    }) -join " ")
    $tempPrefix = "hlhost_goldsrc_delta_reject_{0}_{1}" -f
        $Mode.ToLowerInvariant(),
        ([Guid]::NewGuid().ToString("N"))
    $stdoutPath = Join-Path `
        ([System.IO.Path]::GetTempPath()) `
        ($tempPrefix + ".stdout.log")
    $stderrPath = Join-Path `
        ([System.IO.Path]::GetTempPath()) `
        ($tempPrefix + ".stderr.log")
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
    $handshake = $null

    try {
        $startInfo = New-Object System.Diagnostics.ProcessStartInfo
        $startInfo.FileName = $ResolvedExecutablePath
        $startInfo.Arguments = $argumentLine
        $startInfo.WorkingDirectory = $RepositoryRoot
        $startInfo.UseShellExecute = $false
        $startInfo.CreateNoWindow = $true
        $startInfo.WindowStyle =
            [System.Diagnostics.ProcessWindowStyle]::Hidden
        $startInfo.RedirectStandardOutput = $true
        $startInfo.RedirectStandardError = $true
        $serverProcess = New-Object System.Diagnostics.Process
        $serverProcess.StartInfo = $startInfo
        if (-not $serverProcess.Start()) {
            throw "failed to start rejected-fixture hlhost"
        }
        $processStarted = $true
        $serverProcessId = $serverProcess.Id
        $outputCapture = New-ProcessOutputCapture `
            -Process $serverProcess `
            -StdoutPath $stdoutPath `
            -StderrPath $stderrPath
        [void](Wait-ForServerReadiness `
            -Process $serverProcess `
            -OutputCapture $outputCapture `
            -StdoutPath $stdoutPath `
            -Deadline $deadline `
            -ExpectedAddress $Address `
            -ExpectedPort $selectedPort)
        [void](Wait-ForUdpPortOwnership `
            -Process $serverProcess `
            -Deadline $deadline `
            -ExpectedAddress $Address `
            -ExpectedPort $selectedPort `
            -ExpectedProcessId $serverProcessId)

        $probeClient = New-Object System.Net.Sockets.UdpClient(
            [System.Net.Sockets.AddressFamily]::InterNetwork
        )
        $probeClient.Client.Bind((New-Object System.Net.IPEndPoint(
            [System.Net.IPAddress]::Loopback,
            0
        )))
        $serverEndpoint = New-Object System.Net.IPEndPoint(
            [System.Net.IPAddress]::Parse($Address),
            $selectedPort
        )
        $probeClient.Connect($serverEndpoint)
        $handshake = Invoke-DeltaInitialHandshake `
            -Client $probeClient `
            -ServerProcess $serverProcess `
            -OutputCapture $outputCapture `
            -Deadline $deadline `
            -ServerEndpoint $serverEndpoint
        Send-DeltaClientPacket `
            -Client $probeClient `
            -Sequence 2 `
            -Acknowledgement 2 `
            -ServerReliableAcknowledgementState $true `
            -Payload $handshake.NewPayload `
            -ReliablePayload `
            -Description "$Mode rejected-fixture new command"
        Wait-ForCleanServerExit `
            -Process $serverProcess `
            -OutputCapture $outputCapture `
            -Deadline $deadline
        if ($serverProcess.ExitCode -eq 0) {
            throw "$Mode invalid delta fixture unexpectedly exited successfully"
        }
        if (Test-ProcessIdAlive -ProcessId $serverProcessId) {
            throw "$Mode rejected-fixture hlhost remained alive"
        }
        [void](Assert-NoDirectChildProcessLeak `
            -ParentProcessId $serverProcessId)
        $capturedStdout = Get-SharedFileText -Path $stdoutPath
        $capturedStderr = Get-SharedFileText -Path $stderrPath
        if ($capturedStdout.IndexOf(
                $expectedReason,
                [StringComparison]::Ordinal) -lt 0 -and
            $capturedStderr.IndexOf(
                $expectedReason,
                [StringComparison]::Ordinal) -lt 0) {
            throw ("{0} fixture did not emit expected typed rejection: {1}" -f
                $Mode,
                $expectedReason)
        }
        if ($capturedStdout.IndexOf(
                "goldsrc_signon_bootstrap_queued:",
                [StringComparison]::Ordinal) -ge 0 -or
            $capturedStdout.IndexOf(
                "goldsrc_signon_bootstrap_sent",
                [StringComparison]::Ordinal) -ge 0) {
            throw "$Mode invalid fixture transmitted a signon bootstrap"
        }
        $deltaLine = Get-ExactlyOneSummaryLine `
            -Stdout $capturedStdout `
            -Prefix "goldsrc_delta_description_summary:"
        Assert-SummaryFields `
            -Line $deltaLine `
            -Description "$Mode rejected delta summary" `
            -Expected ([ordered]@{
                enabled = "1"
                source = "synthetic_fixture"
                registry_built = "false"
                preparation_attempts = "1"
                table_count = "0"
                field_count = "0"
                usercmd_table_present = "false"
                delta_queued = "0"
                delta_sent = "0"
                delta_acked = "0"
                bootstrap_queued = "0"
                bootstrap_sent = "0"
                bootstrap_acked = "0"
                session_count = "1"
                put_in_server = "0"
                spawned = "0"
                active = "0"
                clean_shutdown = "1"
            })
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
                    Stop-Process `
                        -Id $serverProcessId `
                        -Force `
                        -ErrorAction Stop
                    [void]$serverProcess.WaitForExit(2000)
                }
                if (-not $serverProcess.HasExited) {
                    $cleanupFailure =
                        "$Mode rejected-fixture hlhost leaked after cleanup"
                }
            }
            catch {
                $cleanupFailure =
                    "failed to clean rejected-fixture hlhost: $($_.Exception.Message)"
            }
        }
        if ($null -ne $outputCapture) {
            try {
                Complete-ProcessOutputCapture -State $outputCapture
            }
            catch {
                if ($null -eq $cleanupFailure) {
                    $cleanupFailure =
                        "failed to drain rejected-fixture output: $($_.Exception.Message)"
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
        if (-not $SuppressServerOutput) {
            if (-not [string]::IsNullOrWhiteSpace($capturedStdout)) {
                Write-Host $capturedStdout.TrimEnd(
                    [char[]]@([char]13, [char]10)
                )
            }
            if (-not [string]::IsNullOrWhiteSpace($capturedStderr)) {
                Write-Host $capturedStderr.TrimEnd(
                    [char[]]@([char]13, [char]10)
                )
            }
        }
        foreach ($tempPath in @($stdoutPath, $stderrPath)) {
            if (Test-Path -LiteralPath $tempPath -PathType Leaf) {
                Remove-Item `
                    -LiteralPath $tempPath `
                    -Force `
                    -ErrorAction SilentlyContinue
            }
            if (Test-Path -LiteralPath $tempPath -PathType Leaf) {
                $cleanupFailure =
                    "$Mode temporary rejected-fixture output was not removed"
            }
        }
        if ($null -ne $serverProcess) {
            $serverProcess.Dispose()
        }
    }
    if ($null -ne $cleanupFailure -and $null -eq $failure) {
        $failure = New-Object System.Management.Automation.ErrorRecord(
            (New-Object System.InvalidOperationException($cleanupFailure)),
            "GoldsrcRejectedDeltaFixtureCleanupFailure",
            [System.Management.Automation.ErrorCategory]::CloseError,
            $serverProcess
        )
    }
    if ($null -ne $failure) {
        throw $failure
    }
    return [pscustomobject]@{
        Mode = $Mode
        Rejected = $true
        Stdout = $capturedStdout
    }
}

$repoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$resolvedExecutablePath = [System.IO.Path]::GetFullPath($ExecutablePath)
$resolvedGameDir = [System.IO.Path]::GetFullPath($GameDir)
$resolvedDeltaFixture = [System.IO.Path]::GetFullPath($DeltaFixture)
$resolvedFragmentedDeltaFixture =
    [System.IO.Path]::GetFullPath($FragmentedDeltaFixture)
$resolvedManifestFixture = [System.IO.Path]::GetFullPath($ManifestFixture)
$resolvedMissingUsercmdFixture = [System.IO.Path]::GetFullPath(
    (Join-Path `
        $repoRoot `
        "src/tests/fixtures/goldsrc_delta_description_missing_usercmd.lst")
)
$resolvedMalformedFixture = [System.IO.Path]::GetFullPath(
    (Join-Path `
        $repoRoot `
        "src/tests/fixtures/goldsrc_delta_description_malformed.lst")
)

foreach ($requiredFile in @(
    $resolvedExecutablePath,
    $resolvedDeltaFixture,
    $resolvedFragmentedDeltaFixture,
    $resolvedManifestFixture,
    $resolvedMissingUsercmdFixture,
    $resolvedMalformedFixture
)) {
    if (-not (Test-Path -LiteralPath $requiredFile -PathType Leaf)) {
        throw ("delta proof input file not found: {0}" -f $requiredFile)
    }
}
if (-not (Test-Path -LiteralPath $resolvedGameDir -PathType Container)) {
    throw ("delta proof game directory not found: {0}" -f $resolvedGameDir)
}
$mapPath = Join-Path $resolvedGameDir "maps/c0a0.bsp"
if (-not (Test-Path -LiteralPath $mapPath -PathType Leaf)) {
    throw ("delta proof map not found: {0}" -f $mapPath)
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
    throw "delta proof game directory is missing its client library"
}
$expectedClientDllMd5 = (
    Get-FileHash -LiteralPath $clientDllPath -Algorithm MD5
).Hash.ToLowerInvariant()

$parsedBindAddress = $null
if (-not [System.Net.IPAddress]::TryParse(
        $BindAddress,
        [ref]$parsedBindAddress) -or
    $parsedBindAddress.AddressFamily -ne
        [System.Net.Sockets.AddressFamily]::InterNetwork -or
    $parsedBindAddress.ToString() -cne "127.0.0.1") {
    throw ("BindAddress must be exactly 127.0.0.1 for this proof: {0}" -f
        $BindAddress)
}

$expectedEntries = @(
    Read-ResourceManifestFixture -Path $resolvedManifestFixture
)
$repositorySnapshotBefore =
    Get-DeltaProofRepositorySnapshot -RepositoryRoot $repoRoot
$mainResult = $null
$missingUsercmdResult = $null
$malformedResult = $null
$failure = $null
$postResourceMode = if ($WorldBaselineProof) {
    "World"
} elseif (-not $PostResourceCommandProof) {
    "None"
} elseif ($PostResourceNegativeProof) {
    "Negative"
} else {
    "Positive"
}
try {
    $missingUsercmdResult = Invoke-RejectedDeltaFixtureHostRun `
        -Mode "MissingUsercmd" `
        -ResolvedExecutablePath $resolvedExecutablePath `
        -ResolvedGameDir $resolvedGameDir `
        -ResolvedDeltaFixture $resolvedMissingUsercmdFixture `
        -RepositoryRoot $repoRoot `
        -Address $BindAddress `
        -RunTimeoutSeconds $TimeoutSeconds `
        -SuppressServerOutput:$SkipServerOutput
    if ($NegativeProof) {
        $malformedResult = Invoke-RejectedDeltaFixtureHostRun `
            -Mode "Malformed" `
            -ResolvedExecutablePath $resolvedExecutablePath `
            -ResolvedGameDir $resolvedGameDir `
            -ResolvedDeltaFixture $resolvedMalformedFixture `
            -RepositoryRoot $repoRoot `
            -Address $BindAddress `
            -RunTimeoutSeconds $TimeoutSeconds `
            -SuppressServerOutput:$SkipServerOutput
        $mainResult = Invoke-DeltaHostRun `
            -Mode "Negative" `
            -ResolvedExecutablePath $resolvedExecutablePath `
            -ResolvedGameDir $resolvedGameDir `
            -ResolvedDeltaFixture $resolvedFragmentedDeltaFixture `
            -ResolvedManifestFixture $resolvedManifestFixture `
            -ExpectedEntries $expectedEntries `
            -ExpectedClientDllMd5 $expectedClientDllMd5 `
            -RepositoryRoot $repoRoot `
            -Address $BindAddress `
            -RequestedPort $Port `
            -RunTimeoutSeconds $TimeoutSeconds `
            -PostResourceMode $postResourceMode `
            -RunFirstSnapshotProof:$FirstSnapshotProof `
            -SuppressServerOutput:$SkipServerOutput
        $negativeServerInfoLine = Get-ExactlyOneSummaryLine `
            -Stdout $mainResult.Stdout `
            -Prefix "goldsrc_serverinfo_summary:"
        Assert-SummaryBoolean `
            -Line $negativeServerInfoLine `
            -Name "server_still_responsive" `
            -Expected $true `
            -Description "negative delta serverinfo summary"
    } else {
        $mainResult = Invoke-DeltaHostRun `
            -Mode "Positive" `
            -ResolvedExecutablePath $resolvedExecutablePath `
            -ResolvedGameDir $resolvedGameDir `
            -ResolvedDeltaFixture $resolvedDeltaFixture `
            -ResolvedManifestFixture $resolvedManifestFixture `
            -ExpectedEntries $expectedEntries `
            -ExpectedClientDllMd5 $expectedClientDllMd5 `
            -RepositoryRoot $repoRoot `
            -Address $BindAddress `
            -RequestedPort $Port `
            -RunTimeoutSeconds $TimeoutSeconds `
            -PostResourceMode $postResourceMode `
            -RunFirstSnapshotProof:$FirstSnapshotProof `
            -SuppressServerOutput:$SkipServerOutput
    }
    Assert-NoDeltaProofRepositoryMutation `
        -RepositoryRoot $repoRoot `
        -Before $repositorySnapshotBefore
}
catch {
    $failure = $_
}

if ($null -ne $failure) {
    $failureMessage = $failure.Exception.Message -replace '[\r\n]+', ' '
    Write-Host ("goldsrc_delta_description_probe: result=fail reason={0}" -f
        $failureMessage)
    throw $failureMessage
}

if ($NegativeProof) {
    Write-Host "goldsrc_delta_description_proof_b: ack_withheld_kept_bundle_pending=true,retransmission_observed=true,retransmitted_bundle_identical=true,duplicate_trigger_suppressed=true,missing_usercmd_table_rejected=true,malformed_definition_rejected=true,fragmented_delta_bundle=pass,wrong_ack_rejected=true,valid_ack_accepted=true,slot_reset_cleared_state=true,fresh_session_after_reset=pass,session_count=1,put_in_server=0,spawned=0,active=0,server_still_responsive=true,clean_shutdown=1,proof_b=pass"
} else {
    Write-Host "goldsrc_delta_description_proof_a: previous_boundary_reproduced=true,delta_bootstrap_received=true,usercmd_table_present=true,usercmd_table_decode=pass,required_delta_tables_present=true,delta_table_order=pass,delta_descriptions_acknowledged=true,signon_advanced_once=true,session_count=1,put_in_server=0,spawned=0,active=0,clean_shutdown=1,proof_a=pass"
}
if ($PostResourceCommandProof) {
    if ($PostResourceNegativeProof) {
        Write-Host "goldsrc_post_resource_command_proof_b: duplicate_sequence_suppressed=true,out_of_order_rejected=true,truncated_command_rejected=true,invalid_count_rejected=true,checksum_validation=pass,unsupported_opcode_rejected=true,invalid_phase_rejected=true,pending_server_reliable_preserved=true,slot_reset_cleared_state=true,fresh_session_after_reset=pass,session_count=1,put_in_server=0,spawned=0,active=0,server_still_responsive=true,clean_shutdown=1,proof_b=pass"
    } else {
        Write-Host "goldsrc_post_resource_command_proof_a: previous_prompt_240_boundary=pass,command_opcode_identified=true,command_identity=clc_move,unreliable_command_decoded=true,command_semantic_validation=pass,command_deliveries=1,command_phase_validation=pass,continuation_result=awaiting_server_baseline_or_snapshot,session_count=1,put_in_server=0,spawned=0,active=0,clean_shutdown=1,proof_a=pass"
    }
}
if ($FirstSnapshotProof) {
    if ($NegativeProof) {
        Write-Host (
            "goldsrc_first_snapshot_proof_b: unknown_frame_rejected=true," +
            "future_frame_rejected=true,evicted_frame_rejected=true," +
            "duplicate_frame_ack_idempotent=true," +
            "malformed_clientdata_rejected=true," +
            "duplicate_entity_rejected=true,invalid_entity_order_rejected=true," +
            "snapshot_overflow_not_truncated=true," +
            "dropped_snapshot_did_not_corrupt_reliable_state=true," +
            "subsequent_full_snapshot=not_required," +
            "slot_reset_cleared_frame_history=true," +
            "fresh_session_first_snapshot=pass,session_count=1," +
            "put_in_server=0,spawned=0,active=0," +
            "server_still_responsive=true,clean_shutdown=1,proof_b=pass"
        )
    } else {
        Write-Host ((
            "goldsrc_first_snapshot_proof_a: previous_prompt_242_boundary=pass," +
            "first_snapshot_received=true,snapshot_message_order=pass," +
            "server_time_decode=pass,clientdata_received=true," +
            "clientdata_decode=pass,weapon_data=not_required," +
            "packet_entities_received=true,packet_entities_decode=pass," +
            "snapshot_entity_count={0},first_snapshot_frame_id={1}," +
            "client_frame_reference_sent=true,first_snapshot_acked=true," +
            "snapshot_phase=first_snapshot_acknowledged,session_count=1," +
            "put_in_server=0,spawned=0,active=0,clean_shutdown=1,proof_a=pass") -f
                $mainResult.Resource.FirstSnapshot.EntityCount,
                $mainResult.Resource.FirstSnapshot.FrameId
        )
    }
}
Write-Host "goldsrc_delta_description_probe: result=pass"
