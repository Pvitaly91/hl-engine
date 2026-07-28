#include "network/goldsrc_client_move.h"

#include "network/goldsrc_netchan.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>

namespace hl::network
{
namespace
{
constexpr std::array<std::uint8_t, 16> kMoveMungeTable = {
    0x7Au,
    0x64u,
    0x05u,
    0xF1u,
    0x1Bu,
    0x9Bu,
    0xA0u,
    0xB5u,
    0xCAu,
    0xEDu,
    0x61u,
    0x0Du,
    0x4Au,
    0xDFu,
    0x8Eu,
    0xC7u,
};

constexpr std::uint32_t SwapBytes(std::uint32_t value) noexcept
{
    return ((value & 0x000000FFu) << 24u)
        | ((value & 0x0000FF00u) << 8u)
        | ((value & 0x00FF0000u) >> 8u)
        | ((value & 0xFF000000u) >> 24u);
}

std::uint32_t LoadLittleEndian(const std::uint8_t* bytes) noexcept
{
    return static_cast<std::uint32_t>(bytes[0])
        | (static_cast<std::uint32_t>(bytes[1]) << 8u)
        | (static_cast<std::uint32_t>(bytes[2]) << 16u)
        | (static_cast<std::uint32_t>(bytes[3]) << 24u);
}

void StoreLittleEndian(std::uint8_t* bytes, std::uint32_t value) noexcept
{
    bytes[0] = static_cast<std::uint8_t>(value & 0xFFu);
    bytes[1] = static_cast<std::uint8_t>((value >> 8u) & 0xFFu);
    bytes[2] = static_cast<std::uint8_t>((value >> 16u) & 0xFFu);
    bytes[3] = static_cast<std::uint8_t>((value >> 24u) & 0xFFu);
}

void TransformMoveBody(
    std::uint8_t* bytes,
    std::size_t size,
    std::uint32_t sequence,
    bool reverse) noexcept
{
    if (bytes == nullptr && size != 0u)
    {
        return;
    }
    const std::size_t group_count = size / 4u;
    for (std::size_t group = 0; group < group_count; ++group)
    {
        std::uint32_t value = LoadLittleEndian(bytes + group * 4u);
        value ^= sequence;
        if (!reverse)
        {
            value = SwapBytes(value);
        }

        std::array<std::uint8_t, 4> transformed = {
            static_cast<std::uint8_t>(value & 0xFFu),
            static_cast<std::uint8_t>((value >> 8u) & 0xFFu),
            static_cast<std::uint8_t>((value >> 16u) & 0xFFu),
            static_cast<std::uint8_t>((value >> 24u) & 0xFFu),
        };
        for (std::size_t index = 0; index < transformed.size(); ++index)
        {
            const std::uint8_t mix = static_cast<std::uint8_t>(
                0xA5u
                | static_cast<std::uint8_t>(index << index)
                | static_cast<std::uint8_t>(index)
                | kMoveMungeTable[(group + index) & 0x0Fu]);
            transformed[index] ^= mix;
        }
        value = static_cast<std::uint32_t>(transformed[0])
            | (static_cast<std::uint32_t>(transformed[1]) << 8u)
            | (static_cast<std::uint32_t>(transformed[2]) << 16u)
            | (static_cast<std::uint32_t>(transformed[3]) << 24u);
        if (reverse)
        {
            value = SwapBytes(value);
        }
        value ^= ~sequence;
        StoreLittleEndian(bytes + group * 4u, value);
    }
}

constexpr std::array<std::uint32_t, 256> BuildCrc32Table() noexcept
{
    std::array<std::uint32_t, 256> table{};
    for (std::size_t index = 0; index < table.size(); ++index)
    {
        std::uint32_t value = static_cast<std::uint32_t>(index);
        for (std::size_t bit = 0; bit < 8u; ++bit)
        {
            value = (value & 1u) != 0u
                ? 0xEDB88320u ^ (value >> 1u)
                : value >> 1u;
        }
        table[index] = value;
    }
    return table;
}

constexpr auto kCrc32Table = BuildCrc32Table();

std::uint8_t CrcTableByte(std::size_t byte_offset) noexcept
{
    const std::uint32_t word = kCrc32Table[byte_offset / 4u];
    return static_cast<std::uint8_t>(
        (word >> ((byte_offset % 4u) * 8u)) & 0xFFu);
}

void ProcessCrcByte(std::uint32_t* crc, std::uint8_t value) noexcept
{
    *crc = kCrc32Table[(*crc ^ value) & 0xFFu] ^ (*crc >> 8u);
}

std::int32_t SignExtend(std::uint32_t value, std::size_t bits) noexcept
{
    if (bits == 32u)
    {
        return static_cast<std::int32_t>(value);
    }
    const std::uint32_t sign_bit = std::uint32_t{1} << (bits - 1u);
    if ((value & sign_bit) == 0u)
    {
        return static_cast<std::int32_t>(value);
    }
    const std::uint32_t mask = (std::uint32_t{1} << bits) - 1u;
    return static_cast<std::int32_t>(value | ~mask);
}

bool HasExactlyOneBaseType(std::uint32_t type) noexcept
{
    constexpr std::uint32_t kBaseTypes =
        kGoldSrcDeltaTypeByte | kGoldSrcDeltaTypeShort
        | kGoldSrcDeltaTypeFloat | kGoldSrcDeltaTypeInteger
        | kGoldSrcDeltaTypeAngle | kGoldSrcDeltaTypeTimeWindow8
        | kGoldSrcDeltaTypeTimeWindowBig | kGoldSrcDeltaTypeString;
    const std::uint32_t base = type & kBaseTypes;
    return base != 0u && (base & (base - 1u)) == 0u
        && (type & ~(kBaseTypes | kGoldSrcDeltaTypeSigned)) == 0u;
}

GoldSrcDeltaRecordDecodeStatus DecodeFieldValue(
    GoldSrcBitReader* reader,
    const GoldSrcDeltaField& field,
    GoldSrcDecodedDeltaValue* value) noexcept
{
    if (reader == nullptr || value == nullptr
        || field.significant_bits == 0u || field.significant_bits > 32u
        || !HasExactlyOneBaseType(field.field_type))
    {
        return GoldSrcDeltaRecordDecodeStatus::kUnsupportedFieldEncoding;
    }
    if (!std::isfinite(field.premultiply)
        || !std::isfinite(field.postmultiply)
        || field.premultiply <= 0.0 || field.postmultiply <= 0.0)
    {
        return GoldSrcDeltaRecordDecodeStatus::kInvalidMultiplier;
    }

    const bool is_signed =
        (field.field_type & kGoldSrcDeltaTypeSigned) != 0u;
    const std::uint32_t base_type =
        field.field_type & ~kGoldSrcDeltaTypeSigned;
    if (base_type == kGoldSrcDeltaTypeString)
    {
        const std::size_t maximum =
            std::min<std::size_t>(field.field_size, 255u);
        value->kind = GoldSrcDeltaValueKind::kString;
        value->string_size = 0u;
        for (std::size_t index = 0; index <= maximum; ++index)
        {
            std::uint32_t character = 0u;
            if (!reader->ReadBits(8u, &character))
            {
                return GoldSrcDeltaRecordDecodeStatus::kTruncatedBitstream;
            }
            if (character == 0u)
            {
                return GoldSrcDeltaRecordDecodeStatus::kOk;
            }
            if (index == maximum)
            {
                return GoldSrcDeltaRecordDecodeStatus::kStringTooLong;
            }
            value->string_value[index] = static_cast<char>(character);
            ++value->string_size;
        }
        return GoldSrcDeltaRecordDecodeStatus::kStringTooLong;
    }

    std::uint32_t raw = 0u;
    if (!reader->ReadBits(field.significant_bits, &raw))
    {
        return GoldSrcDeltaRecordDecodeStatus::kTruncatedBitstream;
    }
    const std::int32_t signed_raw =
        is_signed ? SignExtend(raw, field.significant_bits) : 0;
    if (base_type == kGoldSrcDeltaTypeAngle)
    {
        double angle =
            static_cast<double>(raw)
            * (360.0 / std::ldexp(1.0, field.significant_bits));
        if (angle < -180.0)
        {
            angle += 360.0;
        }
        else if (angle > 180.0)
        {
            angle -= 360.0;
        }
        value->kind = GoldSrcDeltaValueKind::kFloatingPoint;
        value->floating_value = angle;
        return GoldSrcDeltaRecordDecodeStatus::kOk;
    }
    if (base_type == kGoldSrcDeltaTypeFloat)
    {
        value->kind = GoldSrcDeltaValueKind::kFloatingPoint;
        value->floating_value =
            (is_signed ? static_cast<double>(signed_raw)
                       : static_cast<double>(raw))
            / field.premultiply * field.postmultiply;
        return GoldSrcDeltaRecordDecodeStatus::kOk;
    }
    if (base_type == kGoldSrcDeltaTypeTimeWindow8
        || base_type == kGoldSrcDeltaTypeTimeWindowBig)
    {
        const double scale = base_type == kGoldSrcDeltaTypeTimeWindow8
            ? 100.0
            : field.premultiply;
        value->kind = GoldSrcDeltaValueKind::kFloatingPoint;
        value->floating_value = -static_cast<double>(signed_raw) / scale;
        return GoldSrcDeltaRecordDecodeStatus::kOk;
    }

    if (is_signed)
    {
        value->kind = GoldSrcDeltaValueKind::kSignedInteger;
        value->signed_value = static_cast<std::int32_t>(
            static_cast<double>(signed_raw) / field.premultiply
            * field.postmultiply);
    }
    else
    {
        value->kind = GoldSrcDeltaValueKind::kUnsignedInteger;
        value->unsigned_value = static_cast<std::uint32_t>(
            static_cast<double>(raw) / field.premultiply
            * field.postmultiply);
    }
    return GoldSrcDeltaRecordDecodeStatus::kOk;
}

const GoldSrcDeltaTable* FindUserCommandTable(
    const GoldSrcDeltaRegistry& registry,
    bool* duplicate) noexcept
{
    const GoldSrcDeltaTable* found = nullptr;
    *duplicate = false;
    for (const GoldSrcDeltaTable& table : registry.tables)
    {
        if (table.name != "usercmd_t")
        {
            continue;
        }
        if (found != nullptr)
        {
            *duplicate = true;
            return nullptr;
        }
        found = &table;
    }
    return found;
}

bool ReadUnsigned(
    const GoldSrcDecodedDeltaValue& value,
    std::uint32_t maximum,
    std::uint32_t* output) noexcept
{
    if (output == nullptr)
    {
        return false;
    }
    if (value.kind == GoldSrcDeltaValueKind::kUnsignedInteger
        && value.unsigned_value <= maximum)
    {
        *output = value.unsigned_value;
        return true;
    }
    if (value.kind == GoldSrcDeltaValueKind::kSignedInteger
        && value.signed_value >= 0
        && static_cast<std::uint32_t>(value.signed_value) <= maximum)
    {
        *output = static_cast<std::uint32_t>(value.signed_value);
        return true;
    }
    return false;
}

bool ReadFloat(
    const GoldSrcDecodedDeltaValue& value,
    float* output) noexcept
{
    if (output == nullptr)
    {
        return false;
    }
    double numeric = 0.0;
    if (value.kind == GoldSrcDeltaValueKind::kFloatingPoint)
    {
        numeric = value.floating_value;
    }
    else if (value.kind == GoldSrcDeltaValueKind::kSignedInteger)
    {
        numeric = value.signed_value;
    }
    else if (value.kind == GoldSrcDeltaValueKind::kUnsignedInteger)
    {
        numeric = value.unsigned_value;
    }
    else
    {
        return false;
    }
    if (!std::isfinite(numeric)
        || numeric < -std::numeric_limits<float>::max()
        || numeric > std::numeric_limits<float>::max())
    {
        return false;
    }
    *output = static_cast<float>(numeric);
    return true;
}

bool ApplyUserCommandRecord(
    const GoldSrcDeltaTable& table,
    const GoldSrcDecodedDeltaRecord& record,
    GoldSrcDecodedUserCommand* command) noexcept
{
    if (command == nullptr || record.field_count != table.fields.size())
    {
        return false;
    }
    for (std::size_t index = 0; index < table.fields.size(); ++index)
    {
        const std::string& name = table.fields[index].name;
        const GoldSrcDecodedDeltaValue& value = record.values[index];
        std::uint32_t integer = 0u;
        if (name == "lerp_msec")
        {
            if (!ReadUnsigned(value, UINT16_MAX, &integer))
            {
                return false;
            }
            command->lerp_msec = static_cast<std::uint16_t>(integer);
        }
        else if (name == "msec")
        {
            if (!ReadUnsigned(value, UINT8_MAX, &integer))
            {
                return false;
            }
            command->msec = static_cast<std::uint8_t>(integer);
        }
        else if (name == "viewangles[0]")
        {
            if (!ReadFloat(value, &command->viewangles[0]))
            {
                return false;
            }
        }
        else if (name == "viewangles[1]")
        {
            if (!ReadFloat(value, &command->viewangles[1]))
            {
                return false;
            }
        }
        else if (name == "viewangles[2]")
        {
            if (!ReadFloat(value, &command->viewangles[2]))
            {
                return false;
            }
        }
        else if (name == "buttons")
        {
            if (!ReadUnsigned(value, UINT16_MAX, &integer))
            {
                return false;
            }
            command->buttons = static_cast<std::uint16_t>(integer);
        }
        else if (name == "forwardmove")
        {
            if (!ReadFloat(value, &command->forwardmove))
            {
                return false;
            }
        }
        else if (name == "lightlevel")
        {
            if (!ReadUnsigned(value, UINT8_MAX, &integer))
            {
                return false;
            }
            command->lightlevel = static_cast<std::uint8_t>(integer);
        }
        else if (name == "sidemove")
        {
            if (!ReadFloat(value, &command->sidemove))
            {
                return false;
            }
        }
        else if (name == "upmove")
        {
            if (!ReadFloat(value, &command->upmove))
            {
                return false;
            }
        }
        else if (name == "impulse")
        {
            if (!ReadUnsigned(value, UINT8_MAX, &integer))
            {
                return false;
            }
            command->impulse = static_cast<std::uint8_t>(integer);
        }
        else if (name == "impact_index")
        {
            if (!ReadUnsigned(value, UINT32_MAX, &integer))
            {
                return false;
            }
            command->impact_index = integer;
        }
        else if (name == "impact_position[0]")
        {
            if (!ReadFloat(value, &command->impact_position[0]))
            {
                return false;
            }
        }
        else if (name == "impact_position[1]")
        {
            if (!ReadFloat(value, &command->impact_position[1]))
            {
                return false;
            }
        }
        else if (name == "impact_position[2]")
        {
            if (!ReadFloat(value, &command->impact_position[2]))
            {
                return false;
            }
        }
        else
        {
            return false;
        }
    }
    return true;
}

GoldSrcClientMoveDecodeStatus TranslateRecordStatus(
    GoldSrcDeltaRecordDecodeStatus status) noexcept
{
    switch (status)
    {
    case GoldSrcDeltaRecordDecodeStatus::kInvalidSchema:
        return GoldSrcClientMoveDecodeStatus::kInvalidUsercmdSchema;
    case GoldSrcDeltaRecordDecodeStatus::kInvalidMask:
        return GoldSrcClientMoveDecodeStatus::kInvalidDeltaMask;
    case GoldSrcDeltaRecordDecodeStatus::kUnsupportedFieldEncoding:
        return GoldSrcClientMoveDecodeStatus::kUnsupportedFieldEncoding;
    case GoldSrcDeltaRecordDecodeStatus::kInvalidMultiplier:
        return GoldSrcClientMoveDecodeStatus::kInvalidMultiplier;
    case GoldSrcDeltaRecordDecodeStatus::kStringTooLong:
        return GoldSrcClientMoveDecodeStatus::kStringTooLong;
    case GoldSrcDeltaRecordDecodeStatus::kTruncatedBitstream:
    default:
        return GoldSrcClientMoveDecodeStatus::kTruncatedBitstream;
    }
}
} // namespace

std::string_view ReasonFor(GoldSrcDeltaRecordDecodeStatus status) noexcept
{
    switch (status)
    {
    case GoldSrcDeltaRecordDecodeStatus::kOk:
        return "ok";
    case GoldSrcDeltaRecordDecodeStatus::kInvalidSchema:
        return "invalid_schema";
    case GoldSrcDeltaRecordDecodeStatus::kInvalidMask:
        return "invalid_mask";
    case GoldSrcDeltaRecordDecodeStatus::kUnsupportedFieldEncoding:
        return "unsupported_field_encoding";
    case GoldSrcDeltaRecordDecodeStatus::kInvalidMultiplier:
        return "invalid_multiplier";
    case GoldSrcDeltaRecordDecodeStatus::kStringTooLong:
        return "string_too_long";
    case GoldSrcDeltaRecordDecodeStatus::kTruncatedBitstream:
    default:
        return "truncated_bitstream";
    }
}

GoldSrcDeltaRecordDecodeResult DecodeGoldSrcDeltaRecord(
    GoldSrcBitReader* reader,
    const GoldSrcDeltaTable& table,
    const GoldSrcDecodedDeltaRecord& previous) noexcept
{
    GoldSrcDeltaRecordDecodeResult result;
    if (reader == nullptr || table.fields.empty()
        || table.fields.size() > kGoldSrcMaximumDeltaFieldsPerTable
        || previous.field_count != table.fields.size())
    {
        return result;
    }

    std::uint32_t mask_byte_count = 0u;
    if (!reader->ReadBits(3u, &mask_byte_count))
    {
        result.status = GoldSrcDeltaRecordDecodeStatus::kTruncatedBitstream;
        return result;
    }
    const std::size_t required_mask_bytes =
        (table.fields.size() + 7u) / 8u;
    if (mask_byte_count > required_mask_bytes)
    {
        result.status = GoldSrcDeltaRecordDecodeStatus::kInvalidMask;
        return result;
    }

    std::array<std::uint8_t, 7> mask{};
    for (std::size_t index = 0; index < mask_byte_count; ++index)
    {
        std::uint32_t value = 0u;
        if (!reader->ReadBits(8u, &value))
        {
            result.status =
                GoldSrcDeltaRecordDecodeStatus::kTruncatedBitstream;
            return result;
        }
        mask[index] = static_cast<std::uint8_t>(value);
    }
    if (mask_byte_count == required_mask_bytes
        && table.fields.size() % 8u != 0u)
    {
        const std::uint8_t valid_mask = static_cast<std::uint8_t>(
            (std::uint16_t{1} << (table.fields.size() % 8u)) - 1u);
        if ((mask[mask_byte_count - 1u] & ~valid_mask) != 0u)
        {
            result.status = GoldSrcDeltaRecordDecodeStatus::kInvalidMask;
            return result;
        }
    }

    result.record = previous;
    for (std::size_t index = 0; index < table.fields.size(); ++index)
    {
        const bool changed = index / 8u < mask_byte_count
            && (mask[index / 8u]
                    & static_cast<std::uint8_t>(
                        std::uint8_t{1} << (index % 8u)))
                != 0u;
        if (!changed)
        {
            continue;
        }
        const GoldSrcDeltaRecordDecodeStatus field_status =
            DecodeFieldValue(
                reader,
                table.fields[index],
                &result.record.values[index]);
        if (field_status != GoldSrcDeltaRecordDecodeStatus::kOk)
        {
            result.status = field_status;
            return result;
        }
    }
    result.record.field_count = table.fields.size();
    result.status = GoldSrcDeltaRecordDecodeStatus::kOk;
    return result;
}

std::string_view ReasonFor(GoldSrcClientMoveDecodeStatus status) noexcept
{
    switch (status)
    {
    case GoldSrcClientMoveDecodeStatus::kOk:
        return "ok";
    case GoldSrcClientMoveDecodeStatus::kNullInput:
        return "null_input";
    case GoldSrcClientMoveDecodeStatus::kEmptyPayload:
        return "empty_payload";
    case GoldSrcClientMoveDecodeStatus::kPayloadTooLarge:
        return "payload_too_large";
    case GoldSrcClientMoveDecodeStatus::kWrongOpcode:
        return "wrong_opcode";
    case GoldSrcClientMoveDecodeStatus::kTruncatedEnvelope:
        return "truncated_envelope";
    case GoldSrcClientMoveDecodeStatus::kInvalidBodyLength:
        return "invalid_body_length";
    case GoldSrcClientMoveDecodeStatus::kInvalidChecksum:
        return "invalid_checksum";
    case GoldSrcClientMoveDecodeStatus::kMissingUsercmdSchema:
        return "missing_usercmd_schema";
    case GoldSrcClientMoveDecodeStatus::kDuplicateUsercmdSchema:
        return "duplicate_usercmd_schema";
    case GoldSrcClientMoveDecodeStatus::kInvalidUsercmdSchema:
        return "invalid_usercmd_schema";
    case GoldSrcClientMoveDecodeStatus::kPacketLossOutOfRange:
        return "packet_loss_out_of_range";
    case GoldSrcClientMoveDecodeStatus::kCommandCountExceeded:
        return "command_count_exceeded";
    case GoldSrcClientMoveDecodeStatus::kTruncatedBitstream:
        return "truncated_bitstream";
    case GoldSrcClientMoveDecodeStatus::kInvalidDeltaMask:
        return "invalid_delta_mask";
    case GoldSrcClientMoveDecodeStatus::kUnsupportedFieldEncoding:
        return "unsupported_field_encoding";
    case GoldSrcClientMoveDecodeStatus::kInvalidMultiplier:
        return "invalid_multiplier";
    case GoldSrcClientMoveDecodeStatus::kStringTooLong:
        return "string_too_long";
    case GoldSrcClientMoveDecodeStatus::kNonZeroPadding:
    default:
        return "non_zero_padding";
    }
}

void MungeGoldSrcMoveBody(
    std::uint8_t* bytes,
    std::size_t size,
    std::uint32_t sequence) noexcept
{
    TransformMoveBody(bytes, size, sequence, false);
}

void UnmungeGoldSrcMoveBody(
    std::uint8_t* bytes,
    std::size_t size,
    std::uint32_t sequence) noexcept
{
    TransformMoveBody(bytes, size, sequence, true);
}

std::uint8_t GoldSrcMoveChecksum(
    const std::uint8_t* bytes,
    std::size_t size,
    std::uint32_t sequence) noexcept
{
    if (bytes == nullptr && size != 0u)
    {
        return 0u;
    }
    std::uint32_t crc = 0xFFFFFFFFu;
    const std::size_t protected_size = std::min<std::size_t>(size, 60u);
    for (std::size_t index = 0; index < protected_size; ++index)
    {
        ProcessCrcByte(&crc, bytes[index]);
    }
    const std::size_t seed_offset = sequence % 0x3FCu;
    for (std::size_t index = 0; index < 4u; ++index)
    {
        ProcessCrcByte(&crc, CrcTableByte(seed_offset + index));
    }
    return static_cast<std::uint8_t>((~crc) & 0xFFu);
}

GoldSrcClientMoveDecodeResult DecodeGoldSrcClientMoveCommand(
    const std::uint8_t* bytes,
    std::size_t size,
    std::uint32_t sequence,
    const GoldSrcDeltaRegistry& registry) noexcept
{
    GoldSrcClientMoveDecodeResult result;
    if (bytes == nullptr)
    {
        result.status = size == 0u
            ? GoldSrcClientMoveDecodeStatus::kEmptyPayload
            : GoldSrcClientMoveDecodeStatus::kNullInput;
        return result;
    }
    if (size == 0u)
    {
        result.status = GoldSrcClientMoveDecodeStatus::kEmptyPayload;
        return result;
    }
    if (size > kGoldSrcNetchanMaximumPayloadBytes)
    {
        result.status = GoldSrcClientMoveDecodeStatus::kPayloadTooLarge;
        return result;
    }
    if (bytes[0] != kGoldSrcClientMoveOpcode)
    {
        result.status = GoldSrcClientMoveDecodeStatus::kWrongOpcode;
        return result;
    }
    if (size < 3u)
    {
        result.status = GoldSrcClientMoveDecodeStatus::kTruncatedEnvelope;
        return result;
    }

    const std::size_t body_size = bytes[1];
    if (body_size < 3u)
    {
        result.status = GoldSrcClientMoveDecodeStatus::kInvalidBodyLength;
        return result;
    }
    if (body_size > size - 3u)
    {
        result.status = GoldSrcClientMoveDecodeStatus::kTruncatedEnvelope;
        return result;
    }

    std::array<std::uint8_t, kGoldSrcMaximumMoveBodyBytes> body{};
    std::copy_n(bytes + 3u, body_size, body.begin());
    UnmungeGoldSrcMoveBody(body.data(), body_size, sequence);
    if (GoldSrcMoveChecksum(body.data(), body_size, sequence) != bytes[2])
    {
        result.status = GoldSrcClientMoveDecodeStatus::kInvalidChecksum;
        return result;
    }

    bool duplicate_schema = false;
    const GoldSrcDeltaTable* usercmd =
        FindUserCommandTable(registry, &duplicate_schema);
    if (duplicate_schema)
    {
        result.status =
            GoldSrcClientMoveDecodeStatus::kDuplicateUsercmdSchema;
        return result;
    }
    if (usercmd == nullptr)
    {
        result.status = GoldSrcClientMoveDecodeStatus::kMissingUsercmdSchema;
        return result;
    }
    if (usercmd->fields.empty()
        || usercmd->fields.size() > kGoldSrcMaximumDeltaFieldsPerTable)
    {
        result.status = GoldSrcClientMoveDecodeStatus::kInvalidUsercmdSchema;
        return result;
    }

    result.command.packet_loss = body[0] & 0x7Fu;
    result.command.voice_loopback = (body[0] & 0x80u) != 0u;
    result.command.backup_command_count = body[1];
    result.command.new_command_count = body[2];
    if (result.command.packet_loss > kGoldSrcMaximumReportedPacketLoss)
    {
        result.status = GoldSrcClientMoveDecodeStatus::kPacketLossOutOfRange;
        return result;
    }
    const std::size_t command_count =
        static_cast<std::size_t>(result.command.backup_command_count)
        + static_cast<std::size_t>(result.command.new_command_count);
    if (command_count > kGoldSrcMaximumMoveCommands)
    {
        result.status = GoldSrcClientMoveDecodeStatus::kCommandCountExceeded;
        return result;
    }

    GoldSrcDecodedDeltaRecord previous;
    previous.field_count = usercmd->fields.size();
    std::uint32_t command_time = 0u;
    std::size_t body_offset = 3u;
    for (std::size_t index = 0; index < command_count; ++index)
    {
        if (body_offset >= body_size)
        {
            result.status =
                GoldSrcClientMoveDecodeStatus::kTruncatedBitstream;
            return result;
        }
        GoldSrcBitReader reader(
            body.data() + body_offset,
            body_size - body_offset);
        const GoldSrcDeltaRecordDecodeResult decoded =
            DecodeGoldSrcDeltaRecord(&reader, *usercmd, previous);
        if (!decoded.ok())
        {
            result.status = TranslateRecordStatus(decoded.status);
            return result;
        }
        GoldSrcDecodedUserCommand command;
        if (!ApplyUserCommandRecord(*usercmd, decoded.record, &command))
        {
            result.status =
                GoldSrcClientMoveDecodeStatus::kInvalidUsercmdSchema;
            return result;
        }
        if (!reader.AlignToByte(true))
        {
            result.status = reader.valid()
                ? GoldSrcClientMoveDecodeStatus::kNonZeroPadding
                : GoldSrcClientMoveDecodeStatus::kTruncatedBitstream;
            return result;
        }
        const std::size_t record_bytes = reader.bytes_read();
        if (record_bytes == 0u || record_bytes > body_size - body_offset)
        {
            result.status =
                GoldSrcClientMoveDecodeStatus::kTruncatedBitstream;
            return result;
        }
        body_offset += record_bytes;
        command_time += command.msec;
        command.command_time_msec = command_time;
        result.command.commands[index] = command;
        previous = decoded.record;
    }

    if (body_offset != body_size)
    {
        result.status = GoldSrcClientMoveDecodeStatus::kNonZeroPadding;
        return result;
    }

    std::uint32_t new_command_time = 0u;
    const std::size_t first_new =
        command_count - result.command.new_command_count;
    for (std::size_t index = first_new; index < command_count; ++index)
    {
        new_command_time += result.command.commands[index].msec;
    }
    result.command.command_count = command_count;
    result.command.new_command_time_msec = new_command_time;
    result.command.protected_body_size = body_size;
    result.command.bytes_consumed = body_size + 3u;
    result.status = GoldSrcClientMoveDecodeStatus::kOk;
    return result;
}

std::string_view ReasonFor(
    GoldSrcClientApplicationDecodeStatus status) noexcept
{
    switch (status)
    {
    case GoldSrcClientApplicationDecodeStatus::kOk:
        return "ok";
    case GoldSrcClientApplicationDecodeStatus::kNullInput:
        return "null_input";
    case GoldSrcClientApplicationDecodeStatus::kEmptyPayload:
        return "empty_payload";
    case GoldSrcClientApplicationDecodeStatus::kPayloadTooLarge:
        return "payload_too_large";
    case GoldSrcClientApplicationDecodeStatus::kUnknownOpcode:
        return "unknown_opcode";
    case GoldSrcClientApplicationDecodeStatus::kMultipleMoveCommands:
        return "multiple_move_commands";
    case GoldSrcClientApplicationDecodeStatus::kMalformedMoveCommand:
        return "malformed_move_command";
    case GoldSrcClientApplicationDecodeStatus::kUnsupportedTrailingData:
    default:
        return "unsupported_trailing_data";
    }
}

GoldSrcClientApplicationDecodeResult DecodeGoldSrcClientApplicationPayload(
    const std::uint8_t* bytes,
    std::size_t size,
    std::uint32_t sequence,
    const GoldSrcDeltaRegistry& registry) noexcept
{
    GoldSrcClientApplicationDecodeResult result;
    if (bytes == nullptr)
    {
        result.status = size == 0u
            ? GoldSrcClientApplicationDecodeStatus::kEmptyPayload
            : GoldSrcClientApplicationDecodeStatus::kNullInput;
        return result;
    }
    if (size == 0u)
    {
        result.status = GoldSrcClientApplicationDecodeStatus::kEmptyPayload;
        return result;
    }
    if (size > kGoldSrcNetchanMaximumPayloadBytes)
    {
        result.status = GoldSrcClientApplicationDecodeStatus::kPayloadTooLarge;
        return result;
    }

    std::size_t cursor = 0u;
    while (cursor < size)
    {
        const std::uint8_t opcode = bytes[cursor];
        if (opcode == kGoldSrcClientNop)
        {
            ++result.nop_count;
            ++cursor;
            continue;
        }
        if (opcode != kGoldSrcClientMoveOpcode)
        {
            result.status = result.move_present
                ? GoldSrcClientApplicationDecodeStatus::
                    kUnsupportedTrailingData
                : GoldSrcClientApplicationDecodeStatus::kUnknownOpcode;
            result.bytes_consumed = cursor;
            return result;
        }
        if (result.move_present)
        {
            result.status =
                GoldSrcClientApplicationDecodeStatus::kMultipleMoveCommands;
            result.bytes_consumed = cursor;
            return result;
        }

        const GoldSrcClientMoveDecodeResult decoded =
            DecodeGoldSrcClientMoveCommand(
                bytes + cursor,
                size - cursor,
                sequence,
                registry);
        result.move_status = decoded.status;
        if (!decoded.ok())
        {
            result.status =
                GoldSrcClientApplicationDecodeStatus::kMalformedMoveCommand;
            result.bytes_consumed = cursor;
            return result;
        }
        result.move = decoded.command;
        result.move_present = true;
        cursor += decoded.command.bytes_consumed;
    }

    if (!result.move_present)
    {
        result.status = GoldSrcClientApplicationDecodeStatus::kUnknownOpcode;
        result.bytes_consumed = cursor;
        return result;
    }
    result.bytes_consumed = cursor;
    result.status = GoldSrcClientApplicationDecodeStatus::kOk;
    return result;
}
} // namespace hl::network
