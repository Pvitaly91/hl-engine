#include "network/goldsrc_snapshot.h"

#include "network/goldsrc_bitstream.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <utility>

namespace hl::network
{
namespace
{
const GoldSrcDeltaTable* FindTable(
    const GoldSrcDeltaRegistry& registry,
    std::string_view name,
    bool* duplicate) noexcept
{
    const GoldSrcDeltaTable* found = nullptr;
    *duplicate = false;
    for (const GoldSrcDeltaTable& table : registry.tables)
    {
        if (table.name != name)
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

std::string_view TableNameFor(GoldSrcBaselineKind kind) noexcept
{
    switch (kind)
    {
    case GoldSrcBaselineKind::kPlayer:
        return "entity_state_player_t";
    case GoldSrcBaselineKind::kCustomEntity:
        return "custom_entity_state_t";
    case GoldSrcBaselineKind::kWorld:
    case GoldSrcBaselineKind::kEntity:
    case GoldSrcBaselineKind::kInstanced:
    default:
        return "entity_state_t";
    }
}

const GoldSrcEntityBaseline* FindEntityBaseline(
    const GoldSrcBaselineBundle& baselines,
    std::uint16_t entity_index) noexcept
{
    const auto iterator = std::lower_bound(
        baselines.entities.begin(),
        baselines.entities.end(),
        entity_index,
        [](const GoldSrcEntityBaseline& baseline, std::uint16_t value)
        {
            return baseline.entity_index < value;
        });
    return iterator != baselines.entities.end()
            && iterator->entity_index == entity_index
        ? &*iterator
        : nullptr;
}

bool NumericValue(
    const GoldSrcDecodedDeltaValue& value,
    double* output) noexcept
{
    if (output == nullptr)
    {
        return false;
    }
    switch (value.kind)
    {
    case GoldSrcDeltaValueKind::kUnsignedInteger:
        *output = value.unsigned_value;
        return true;
    case GoldSrcDeltaValueKind::kSignedInteger:
        *output = value.signed_value;
        return true;
    case GoldSrcDeltaValueKind::kFloatingPoint:
        *output = value.floating_value;
        return std::isfinite(*output);
    case GoldSrcDeltaValueKind::kString:
    default:
        return false;
    }
}

bool ValuesEqual(
    const GoldSrcDecodedDeltaValue& left,
    const GoldSrcDecodedDeltaValue& right) noexcept
{
    if (left.kind == GoldSrcDeltaValueKind::kString
        || right.kind == GoldSrcDeltaValueKind::kString)
    {
        return left.kind == GoldSrcDeltaValueKind::kString
            && right.kind == GoldSrcDeltaValueKind::kString
            && left.string_size == right.string_size
            && std::equal(
                left.string_value.begin(),
                left.string_value.begin()
                    + static_cast<std::ptrdiff_t>(left.string_size),
                right.string_value.begin());
    }
    double left_number = 0.0;
    double right_number = 0.0;
    return NumericValue(left, &left_number)
        && NumericValue(right, &right_number)
        && left_number == right_number;
}

bool HasExactlyOneBaseType(std::uint32_t type) noexcept
{
    constexpr std::uint32_t types =
        kGoldSrcDeltaTypeByte | kGoldSrcDeltaTypeShort
        | kGoldSrcDeltaTypeFloat | kGoldSrcDeltaTypeInteger
        | kGoldSrcDeltaTypeAngle | kGoldSrcDeltaTypeTimeWindow8
        | kGoldSrcDeltaTypeTimeWindowBig | kGoldSrcDeltaTypeString;
    const std::uint32_t base = type & types;
    return base != 0u && (base & (base - 1u)) == 0u
        && (type & ~(types | kGoldSrcDeltaTypeSigned)) == 0u;
}

enum class FieldEncodeResult
{
    kOk,
    kUnsupported,
    kOutOfRange,
    kCapacity,
};

FieldEncodeResult EncodeField(
    GoldSrcBitWriter* writer,
    const GoldSrcDeltaField& field,
    const GoldSrcDecodedDeltaValue& value,
    double time_base) noexcept
{
    if (writer == nullptr || field.significant_bits == 0u
        || field.significant_bits > 32u
        || !HasExactlyOneBaseType(field.field_type)
        || !std::isfinite(field.premultiply)
        || !std::isfinite(field.postmultiply)
        || field.premultiply <= 0.0 || field.postmultiply <= 0.0
        || !std::isfinite(time_base))
    {
        return FieldEncodeResult::kUnsupported;
    }

    const std::uint32_t base =
        field.field_type & ~kGoldSrcDeltaTypeSigned;
    if (base == kGoldSrcDeltaTypeString)
    {
        if (value.kind != GoldSrcDeltaValueKind::kString
            || value.string_size > field.field_size
            || value.string_size >= value.string_value.size())
        {
            return FieldEncodeResult::kOutOfRange;
        }
        return writer->WriteBytes(
                   reinterpret_cast<const std::uint8_t*>(
                       value.string_value.data()),
                   value.string_size)
                && writer->WriteBits(0u, 8u)
            ? FieldEncodeResult::kOk
            : FieldEncodeResult::kCapacity;
    }

    double numeric = 0.0;
    if (!NumericValue(value, &numeric))
    {
        return FieldEncodeResult::kUnsupported;
    }

    const bool is_signed =
        (field.field_type & kGoldSrcDeltaTypeSigned) != 0u;
    double transformed = 0.0;
    if (base == kGoldSrcDeltaTypeAngle)
    {
        const double turns = std::fmod(numeric, 360.0);
        transformed = (turns < 0.0 ? turns + 360.0 : turns)
            * std::ldexp(1.0, field.significant_bits) / 360.0;
    }
    else if (base == kGoldSrcDeltaTypeTimeWindow8
        || base == kGoldSrcDeltaTypeTimeWindowBig)
    {
        const double scale = base == kGoldSrcDeltaTypeTimeWindow8
            ? 100.0
            : field.premultiply;
        transformed =
            static_cast<double>(
                static_cast<std::int64_t>(time_base * scale)
                - static_cast<std::int64_t>(numeric * scale));
    }
    else
    {
        transformed = numeric * field.premultiply;
    }
    if (!std::isfinite(transformed)
        || transformed < static_cast<double>(
            std::numeric_limits<std::int64_t>::min())
        || transformed > static_cast<double>(
            std::numeric_limits<std::int64_t>::max()))
    {
        return FieldEncodeResult::kOutOfRange;
    }

    const std::int64_t integral =
        static_cast<std::int64_t>(transformed);
    const std::size_t wire_bits =
        base == kGoldSrcDeltaTypeTimeWindow8
        ? 8u
        : field.significant_bits;
    const std::uint64_t unsigned_max =
        wire_bits == 32u
        ? std::numeric_limits<std::uint32_t>::max()
        : (std::uint64_t{1} << wire_bits) - 1u;
    std::uint32_t raw = 0u;
    if (is_signed || base == kGoldSrcDeltaTypeTimeWindow8
        || base == kGoldSrcDeltaTypeTimeWindowBig)
    {
        const std::int64_t maximum =
            (std::int64_t{1} << (wire_bits - 1u)) - 1;
        if (integral < -maximum || integral > maximum)
        {
            return FieldEncodeResult::kOutOfRange;
        }
        const std::uint64_t magnitude = integral < 0
            ? static_cast<std::uint64_t>(-integral)
            : static_cast<std::uint64_t>(integral);
        raw = static_cast<std::uint32_t>(
            (magnitude << 1u) | (integral < 0 ? 1u : 0u));
    }
    else
    {
        if (integral < 0
            || static_cast<std::uint64_t>(integral) > unsigned_max)
        {
            return FieldEncodeResult::kOutOfRange;
        }
        raw = static_cast<std::uint32_t>(integral);
    }

    return writer->WriteBits(raw, wire_bits)
        ? FieldEncodeResult::kOk
        : FieldEncodeResult::kCapacity;
}

GoldSrcSnapshotCodecStatus EncodeDeltaRecord(
    GoldSrcBitWriter* writer,
    const GoldSrcDeltaTable& table,
    const GoldSrcDecodedDeltaRecord& previous,
    const GoldSrcDecodedDeltaRecord& current,
    double time_base) noexcept
{
    if (writer == nullptr || table.fields.empty()
        || table.fields.size() > kGoldSrcMaximumDeltaFieldsPerTable
        || previous.field_count != table.fields.size()
        || current.field_count != table.fields.size())
    {
        return GoldSrcSnapshotCodecStatus::kInvalidDeltaState;
    }

    std::array<std::uint8_t, 7> mask{};
    std::size_t mask_bytes = 0u;
    for (std::size_t index = 0; index < table.fields.size(); ++index)
    {
        if (!ValuesEqual(previous.values[index], current.values[index]))
        {
            mask[index / 8u] |= static_cast<std::uint8_t>(
                std::uint8_t{1} << (index % 8u));
            mask_bytes = index / 8u + 1u;
        }
    }
    if (!writer->WriteBits(
            static_cast<std::uint32_t>(mask_bytes),
            3u)
        || !writer->WriteBytes(mask.data(), mask_bytes))
    {
        return GoldSrcSnapshotCodecStatus::kOutputCapacityExceeded;
    }

    for (std::size_t index = 0; index < table.fields.size(); ++index)
    {
        if ((mask[index / 8u]
                & static_cast<std::uint8_t>(
                    std::uint8_t{1} << (index % 8u)))
            == 0u)
        {
            continue;
        }
        switch (EncodeField(
            writer,
            table.fields[index],
            current.values[index],
            time_base))
        {
        case FieldEncodeResult::kOk:
            break;
        case FieldEncodeResult::kCapacity:
            return GoldSrcSnapshotCodecStatus::kOutputCapacityExceeded;
        case FieldEncodeResult::kOutOfRange:
            return GoldSrcSnapshotCodecStatus::kValueOutOfRange;
        case FieldEncodeResult::kUnsupported:
        default:
            return GoldSrcSnapshotCodecStatus::kUnsupportedFieldEncoding;
        }
    }
    return GoldSrcSnapshotCodecStatus::kOk;
}

GoldSrcSnapshotCodecStatus DecodeDeltaRecord(
    GoldSrcBitReader* reader,
    const GoldSrcDeltaTable& table,
    const GoldSrcDecodedDeltaRecord& previous,
    double time_base,
    GoldSrcDecodedDeltaRecord* state) noexcept
{
    const GoldSrcDeltaRecordDecodeResult decoded =
        DecodeGoldSrcDeltaRecord(reader, table, previous, time_base);
    if (!decoded.ok())
    {
        return GoldSrcSnapshotCodecStatus::kInvalidDeltaState;
    }
    *state = decoded.record;
    return GoldSrcSnapshotCodecStatus::kOk;
}

bool WriteFloat(GoldSrcBitWriter* writer, float value) noexcept
{
    std::uint32_t raw = 0u;
    static_assert(sizeof(raw) == sizeof(value));
    std::memcpy(&raw, &value, sizeof(raw));
    return writer->WriteBits(raw, 32u);
}

bool ReadFloat(GoldSrcBitReader* reader, float* value) noexcept
{
    std::uint32_t raw = 0u;
    if (!reader->ReadBits(32u, &raw))
    {
        return false;
    }
    std::memcpy(value, &raw, sizeof(raw));
    return true;
}

bool FrameStructureValid(const GoldSrcServerFrame& frame) noexcept
{
    if (!std::isfinite(frame.server_time) || frame.server_time < 0.0f
        || frame.frame_id > 0x3FFFFFFFu
        || frame.weapons.size() > kGoldSrcMaximumSnapshotWeapons
        || frame.entities.size() > kGoldSrcMaximumSnapshotEntities)
    {
        return false;
    }
    std::uint8_t prior_weapon = 0u;
    bool first_weapon = true;
    for (std::size_t index = 0; index < frame.weapons.size(); ++index)
    {
        const std::uint8_t weapon = frame.weapons[index].weapon_index;
        if (weapon >= kGoldSrcMaximumSnapshotWeapons
            || (!first_weapon && weapon <= prior_weapon))
        {
            return false;
        }
        prior_weapon = weapon;
        first_weapon = false;
    }
    std::uint16_t prior_entity = 0u;
    bool first_entity = true;
    for (const GoldSrcSnapshotEntityState& entity : frame.entities)
    {
        if (entity.entity_index == 0u
            || entity.entity_index > kGoldSrcMaximumEntityIndex
            || (!first_entity && entity.entity_index <= prior_entity))
        {
            return false;
        }
        prior_entity = entity.entity_index;
        first_entity = false;
    }
    return true;
}

bool RecordsEqual(
    const GoldSrcDecodedDeltaRecord& left,
    const GoldSrcDecodedDeltaRecord& right) noexcept
{
    if (left.field_count != right.field_count)
    {
        return false;
    }
    for (std::size_t index = 0; index < left.field_count; ++index)
    {
        if (!ValuesEqual(left.values[index], right.values[index]))
        {
            return false;
        }
    }
    return true;
}

bool FramesEqual(
    const GoldSrcServerFrame& left,
    const GoldSrcServerFrame& right) noexcept
{
    if (left.frame_id != right.frame_id
        || left.server_time != right.server_time
        || left.weapons.size() != right.weapons.size()
        || left.entities.size() != right.entities.size()
        || !RecordsEqual(left.clientdata.state, right.clientdata.state))
    {
        return false;
    }
    for (std::size_t index = 0; index < left.weapons.size(); ++index)
    {
        if (left.weapons[index].weapon_index
                != right.weapons[index].weapon_index
            || !RecordsEqual(
                left.weapons[index].state,
                right.weapons[index].state))
        {
            return false;
        }
    }
    for (std::size_t index = 0; index < left.entities.size(); ++index)
    {
        if (left.entities[index].entity_index
                != right.entities[index].entity_index
            || left.entities[index].kind != right.entities[index].kind
            || !RecordsEqual(
                left.entities[index].state,
                right.entities[index].state))
        {
            return false;
        }
    }
    return true;
}
} // namespace

std::string_view ReasonFor(GoldSrcSnapshotCodecStatus status) noexcept
{
    switch (status)
    {
    case GoldSrcSnapshotCodecStatus::kOk: return "ok";
    case GoldSrcSnapshotCodecStatus::kNullInput: return "null_input";
    case GoldSrcSnapshotCodecStatus::kEmptyPayload: return "empty_payload";
    case GoldSrcSnapshotCodecStatus::kPayloadTooLarge: return "payload_too_large";
    case GoldSrcSnapshotCodecStatus::kInvalidFrame: return "invalid_frame";
    case GoldSrcSnapshotCodecStatus::kNonFiniteServerTime: return "non_finite_server_time";
    case GoldSrcSnapshotCodecStatus::kNonMonotonicServerTime: return "non_monotonic_server_time";
    case GoldSrcSnapshotCodecStatus::kMissingDeltaTable: return "missing_delta_table";
    case GoldSrcSnapshotCodecStatus::kDuplicateDeltaTable: return "duplicate_delta_table";
    case GoldSrcSnapshotCodecStatus::kInvalidDeltaState: return "invalid_delta_state";
    case GoldSrcSnapshotCodecStatus::kUnsupportedFieldEncoding: return "unsupported_field_encoding";
    case GoldSrcSnapshotCodecStatus::kValueOutOfRange: return "value_out_of_range";
    case GoldSrcSnapshotCodecStatus::kInvalidWeaponIndex: return "invalid_weapon_index";
    case GoldSrcSnapshotCodecStatus::kWeaponOrderInvalid: return "weapon_order_invalid";
    case GoldSrcSnapshotCodecStatus::kEntityCountExceeded: return "entity_count_exceeded";
    case GoldSrcSnapshotCodecStatus::kInvalidEntityIndex: return "invalid_entity_index";
    case GoldSrcSnapshotCodecStatus::kDuplicateEntity: return "duplicate_entity";
    case GoldSrcSnapshotCodecStatus::kEntityOrderInvalid: return "entity_order_invalid";
    case GoldSrcSnapshotCodecStatus::kMissingEntityBaseline: return "missing_entity_baseline";
    case GoldSrcSnapshotCodecStatus::kEntityKindMismatch: return "entity_kind_mismatch";
    case GoldSrcSnapshotCodecStatus::kWrongMessageOrder: return "wrong_message_order";
    case GoldSrcSnapshotCodecStatus::kUnexpectedPreviousFrame: return "unexpected_previous_frame";
    case GoldSrcSnapshotCodecStatus::kInvalidClientData: return "invalid_clientdata";
    case GoldSrcSnapshotCodecStatus::kInvalidWeaponData: return "invalid_weapon_data";
    case GoldSrcSnapshotCodecStatus::kInvalidPacketEntities: return "invalid_packet_entities";
    case GoldSrcSnapshotCodecStatus::kUnexpectedDeltaPacket: return "unexpected_delta_packet";
    case GoldSrcSnapshotCodecStatus::kMissingEntityTerminator: return "missing_entity_terminator";
    case GoldSrcSnapshotCodecStatus::kNonZeroPadding: return "non_zero_padding";
    case GoldSrcSnapshotCodecStatus::kTrailingData: return "trailing_data";
    case GoldSrcSnapshotCodecStatus::kOutputCapacityExceeded:
    default: return "output_capacity_exceeded";
    }
}

GoldSrcSnapshotEncodeResult EncodeGoldSrcFirstSnapshot(
    const GoldSrcServerFrame& frame,
    const GoldSrcBaselineBundle& baselines,
    const GoldSrcDeltaRegistry& registry,
    std::size_t output_capacity) noexcept
{
    GoldSrcSnapshotEncodeResult result;
    if (!FrameStructureValid(frame))
    {
        return result;
    }
    if (output_capacity == 0u
        || output_capacity > result.payload.bytes.size())
    {
        result.status =
            GoldSrcSnapshotCodecStatus::kOutputCapacityExceeded;
        return result;
    }

    bool duplicate = false;
    const GoldSrcDeltaTable* clientdata =
        FindTable(registry, "clientdata_t", &duplicate);
    if (duplicate || clientdata == nullptr)
    {
        result.status = duplicate
            ? GoldSrcSnapshotCodecStatus::kDuplicateDeltaTable
            : GoldSrcSnapshotCodecStatus::kMissingDeltaTable;
        return result;
    }
    GoldSrcDecodedDeltaRecord zero_clientdata;
    zero_clientdata.field_count = clientdata->fields.size();

    GoldSrcBitWriter writer(result.payload.bytes.data(), output_capacity);
    if (!writer.WriteBits(kGoldSrcServerTimeOpcode, 8u)
        || !WriteFloat(&writer, frame.server_time)
        || !writer.WriteBits(kGoldSrcClientDataOpcode, 8u)
        || !writer.WriteBits(0u, 1u))
    {
        result.status =
            GoldSrcSnapshotCodecStatus::kOutputCapacityExceeded;
        return result;
    }
    result.status = EncodeDeltaRecord(
        &writer,
        *clientdata,
        zero_clientdata,
        frame.clientdata.state,
        frame.server_time);
    if (result.status != GoldSrcSnapshotCodecStatus::kOk)
    {
        return result;
    }

    const GoldSrcDeltaTable* weapon_table = nullptr;
    if (!frame.weapons.empty())
    {
        weapon_table = FindTable(registry, "weapon_data_t", &duplicate);
        if (duplicate || weapon_table == nullptr)
        {
            result.status = duplicate
                ? GoldSrcSnapshotCodecStatus::kDuplicateDeltaTable
                : GoldSrcSnapshotCodecStatus::kMissingDeltaTable;
            return result;
        }
    }
    std::uint8_t prior_weapon = 0u;
    bool first_weapon = true;
    for (std::size_t index = 0; index < frame.weapons.size(); ++index)
    {
        const GoldSrcWeaponState& weapon = frame.weapons[index];
        if (weapon.weapon_index >= kGoldSrcMaximumSnapshotWeapons)
        {
            result.status = GoldSrcSnapshotCodecStatus::kInvalidWeaponIndex;
            return result;
        }
        if (!first_weapon && weapon.weapon_index <= prior_weapon)
        {
            result.status = GoldSrcSnapshotCodecStatus::kWeaponOrderInvalid;
            return result;
        }
        GoldSrcDecodedDeltaRecord zero_weapon;
        zero_weapon.field_count = weapon_table->fields.size();
        if (!writer.WriteBits(1u, 1u)
            || !writer.WriteBits(weapon.weapon_index, 6u))
        {
            result.status =
                GoldSrcSnapshotCodecStatus::kOutputCapacityExceeded;
            return result;
        }
        result.status = EncodeDeltaRecord(
            &writer,
            *weapon_table,
            zero_weapon,
            weapon.state,
            frame.server_time);
        if (result.status != GoldSrcSnapshotCodecStatus::kOk)
        {
            return result;
        }
        prior_weapon = weapon.weapon_index;
        first_weapon = false;
    }
    if (!writer.WriteBits(0u, 1u)
        || !writer.PadToByte()
        || !writer.WriteBits(kGoldSrcPacketEntitiesOpcode, 8u)
        || !writer.WriteBits(
            static_cast<std::uint32_t>(frame.entities.size()),
            16u))
    {
        result.status =
            GoldSrcSnapshotCodecStatus::kOutputCapacityExceeded;
        return result;
    }

    std::uint16_t number_base = 0u;
    for (const GoldSrcSnapshotEntityState& entity : frame.entities)
    {
        const GoldSrcEntityBaseline* baseline =
            FindEntityBaseline(baselines, entity.entity_index);
        if (baseline == nullptr)
        {
            result.status =
                GoldSrcSnapshotCodecStatus::kMissingEntityBaseline;
            return result;
        }
        if (baseline->kind != entity.kind)
        {
            result.status = GoldSrcSnapshotCodecStatus::kEntityKindMismatch;
            return result;
        }
        const std::uint16_t delta =
            static_cast<std::uint16_t>(entity.entity_index - number_base);
        if (delta == 1u)
        {
            if (!writer.WriteBits(1u, 1u))
            {
                result.status =
                    GoldSrcSnapshotCodecStatus::kOutputCapacityExceeded;
                return result;
            }
        }
        else
        {
            if (!writer.WriteBits(0u, 1u))
            {
                result.status =
                    GoldSrcSnapshotCodecStatus::kOutputCapacityExceeded;
                return result;
            }
            if (delta == 0u || delta > 63u)
            {
                if (!writer.WriteBits(1u, 1u)
                    || !writer.WriteBits(entity.entity_index, 11u))
                {
                    result.status =
                        GoldSrcSnapshotCodecStatus::kOutputCapacityExceeded;
                    return result;
                }
            }
            else if (!writer.WriteBits(0u, 1u)
                || !writer.WriteBits(delta, 6u))
            {
                result.status =
                    GoldSrcSnapshotCodecStatus::kOutputCapacityExceeded;
                return result;
            }
        }
        number_base = entity.entity_index;
        const bool custom =
            entity.kind == GoldSrcBaselineKind::kCustomEntity;
        if (!writer.WriteBits(custom ? 1u : 0u, 1u)
            || (!baselines.instances.empty()
                && !writer.WriteBits(0u, 1u))
            || !writer.WriteBits(0u, 1u))
        {
            result.status =
                GoldSrcSnapshotCodecStatus::kOutputCapacityExceeded;
            return result;
        }

        const GoldSrcDeltaTable* entity_table =
            FindTable(registry, TableNameFor(entity.kind), &duplicate);
        if (duplicate || entity_table == nullptr)
        {
            result.status = duplicate
                ? GoldSrcSnapshotCodecStatus::kDuplicateDeltaTable
                : GoldSrcSnapshotCodecStatus::kMissingDeltaTable;
            return result;
        }
        result.status = EncodeDeltaRecord(
            &writer,
            *entity_table,
            baseline->state,
            entity.state,
            frame.server_time);
        if (result.status != GoldSrcSnapshotCodecStatus::kOk)
        {
            return result;
        }
    }
    if (!writer.WriteBits(0u, 16u)
        || !writer.PadToByte())
    {
        result.status =
            GoldSrcSnapshotCodecStatus::kOutputCapacityExceeded;
        return result;
    }
    result.payload.size = writer.bytes_written();
    result.status = GoldSrcSnapshotCodecStatus::kOk;
    return result;
}

GoldSrcSnapshotDecodeResult DecodeGoldSrcFirstSnapshot(
    const std::uint8_t* bytes,
    std::size_t size,
    std::uint32_t frame_id,
    const GoldSrcBaselineBundle& baselines,
    const GoldSrcDeltaRegistry& registry) noexcept
{
    GoldSrcSnapshotDecodeResult result;
    result.frame.frame_id = frame_id;
    if (bytes == nullptr)
    {
        result.status = size == 0u
            ? GoldSrcSnapshotCodecStatus::kEmptyPayload
            : GoldSrcSnapshotCodecStatus::kNullInput;
        return result;
    }
    if (size == 0u)
    {
        return result;
    }
    if (size > kGoldSrcMaximumSnapshotBytes)
    {
        result.status = GoldSrcSnapshotCodecStatus::kPayloadTooLarge;
        return result;
    }

    bool duplicate = false;
    const GoldSrcDeltaTable* clientdata =
        FindTable(registry, "clientdata_t", &duplicate);
    if (duplicate || clientdata == nullptr)
    {
        result.status = duplicate
            ? GoldSrcSnapshotCodecStatus::kDuplicateDeltaTable
            : GoldSrcSnapshotCodecStatus::kMissingDeltaTable;
        return result;
    }

    GoldSrcBitReader reader(bytes, size);
    std::uint32_t value = 0u;
    if (!reader.ReadBits(8u, &value)
        || value != kGoldSrcServerTimeOpcode
        || !ReadFloat(&reader, &result.frame.server_time)
        || !std::isfinite(result.frame.server_time)
        || result.frame.server_time < 0.0f)
    {
        result.status = GoldSrcSnapshotCodecStatus::kNonFiniteServerTime;
        return result;
    }
    if (!reader.ReadBits(8u, &value)
        || value != kGoldSrcClientDataOpcode)
    {
        result.status = GoldSrcSnapshotCodecStatus::kWrongMessageOrder;
        return result;
    }
    if (!reader.ReadBits(1u, &value) || value != 0u)
    {
        result.status =
            GoldSrcSnapshotCodecStatus::kUnexpectedPreviousFrame;
        return result;
    }
    GoldSrcDecodedDeltaRecord zero_clientdata;
    zero_clientdata.field_count = clientdata->fields.size();
    result.status = DecodeDeltaRecord(
        &reader,
        *clientdata,
        zero_clientdata,
        result.frame.server_time,
        &result.frame.clientdata.state);
    if (result.status != GoldSrcSnapshotCodecStatus::kOk)
    {
        result.status = GoldSrcSnapshotCodecStatus::kInvalidClientData;
        return result;
    }

    const GoldSrcDeltaTable* weapon_table = nullptr;
    std::uint8_t prior_weapon = 0u;
    bool first_weapon = true;
    while (true)
    {
        if (!reader.ReadBits(1u, &value))
        {
            result.status = GoldSrcSnapshotCodecStatus::kInvalidWeaponData;
            return result;
        }
        if (value == 0u)
        {
            break;
        }
        if (result.frame.weapons.size() >= kGoldSrcMaximumSnapshotWeapons
            || !reader.ReadBits(6u, &value))
        {
            result.status = GoldSrcSnapshotCodecStatus::kInvalidWeaponData;
            return result;
        }
        const std::uint8_t weapon_index =
            static_cast<std::uint8_t>(value);
        if (!first_weapon && weapon_index <= prior_weapon)
        {
            result.status = GoldSrcSnapshotCodecStatus::kWeaponOrderInvalid;
            return result;
        }
        if (weapon_table == nullptr)
        {
            weapon_table =
                FindTable(registry, "weapon_data_t", &duplicate);
            if (duplicate || weapon_table == nullptr)
            {
                result.status = duplicate
                    ? GoldSrcSnapshotCodecStatus::kDuplicateDeltaTable
                    : GoldSrcSnapshotCodecStatus::kMissingDeltaTable;
                return result;
            }
        }
        GoldSrcWeaponState weapon;
        weapon.weapon_index = weapon_index;
        GoldSrcDecodedDeltaRecord zero_weapon;
        zero_weapon.field_count = weapon_table->fields.size();
        result.status = DecodeDeltaRecord(
            &reader,
            *weapon_table,
            zero_weapon,
            result.frame.server_time,
            &weapon.state);
        if (result.status != GoldSrcSnapshotCodecStatus::kOk)
        {
            result.status = GoldSrcSnapshotCodecStatus::kInvalidWeaponData;
            return result;
        }
        result.frame.weapons.push_back(std::move(weapon));
        prior_weapon = weapon_index;
        first_weapon = false;
    }
    if (!reader.AlignToByte(true))
    {
        result.status = GoldSrcSnapshotCodecStatus::kNonZeroPadding;
        return result;
    }

    if (!reader.ReadBits(8u, &value))
    {
        result.status = GoldSrcSnapshotCodecStatus::kInvalidPacketEntities;
        return result;
    }
    if (value == kGoldSrcDeltaPacketEntitiesOpcode)
    {
        result.status = GoldSrcSnapshotCodecStatus::kUnexpectedDeltaPacket;
        return result;
    }
    if (value != kGoldSrcPacketEntitiesOpcode)
    {
        result.status = GoldSrcSnapshotCodecStatus::kWrongMessageOrder;
        return result;
    }
    if (!reader.ReadBits(16u, &value)
        || value > kGoldSrcMaximumSnapshotEntities)
    {
        result.status = GoldSrcSnapshotCodecStatus::kEntityCountExceeded;
        return result;
    }
    const std::size_t entity_count = value;
    result.frame.entities.reserve(entity_count);
    std::uint16_t number_base = 0u;
    for (std::size_t index = 0; index < entity_count; ++index)
    {
        std::uint32_t sequential = 0u;
        if (!reader.ReadBits(1u, &sequential))
        {
            result.status =
                GoldSrcSnapshotCodecStatus::kInvalidPacketEntities;
            return result;
        }
        std::uint32_t entity_number = 0u;
        if (sequential != 0u)
        {
            entity_number = static_cast<std::uint32_t>(number_base) + 1u;
        }
        else
        {
            std::uint32_t absolute = 0u;
            if (!reader.ReadBits(1u, &absolute))
            {
                result.status =
                    GoldSrcSnapshotCodecStatus::kInvalidPacketEntities;
                return result;
            }
            if (absolute != 0u)
            {
                if (!reader.ReadBits(11u, &entity_number))
                {
                    result.status =
                        GoldSrcSnapshotCodecStatus::kInvalidPacketEntities;
                    return result;
                }
            }
            else
            {
                std::uint32_t entity_delta = 0u;
                if (!reader.ReadBits(6u, &entity_delta)
                    || entity_delta == 0u)
                {
                    result.status =
                        GoldSrcSnapshotCodecStatus::kInvalidPacketEntities;
                    return result;
                }
                entity_number =
                    static_cast<std::uint32_t>(number_base) + entity_delta;
            }
        }
        if (entity_number == 0u
            || entity_number > kGoldSrcMaximumEntityIndex
            || entity_number <= number_base)
        {
            result.status =
                entity_number == number_base
                ? GoldSrcSnapshotCodecStatus::kDuplicateEntity
                : GoldSrcSnapshotCodecStatus::kEntityOrderInvalid;
            return result;
        }
        number_base = static_cast<std::uint16_t>(entity_number);

        std::uint32_t custom = 0u;
        if (!reader.ReadBits(1u, &custom))
        {
            result.status =
                GoldSrcSnapshotCodecStatus::kInvalidPacketEntities;
            return result;
        }
        if (!baselines.instances.empty())
        {
            std::uint32_t new_baseline = 0u;
            if (!reader.ReadBits(1u, &new_baseline)
                || new_baseline != 0u)
            {
                result.status =
                    GoldSrcSnapshotCodecStatus::kInvalidPacketEntities;
                return result;
            }
        }
        std::uint32_t offset_baseline = 0u;
        if (!reader.ReadBits(1u, &offset_baseline)
            || offset_baseline != 0u)
        {
            result.status =
                GoldSrcSnapshotCodecStatus::kInvalidPacketEntities;
            return result;
        }

        const GoldSrcEntityBaseline* baseline =
            FindEntityBaseline(
                baselines,
                static_cast<std::uint16_t>(entity_number));
        if (baseline == nullptr)
        {
            result.status =
                GoldSrcSnapshotCodecStatus::kMissingEntityBaseline;
            return result;
        }
        GoldSrcBaselineKind kind =
            custom != 0u
            ? GoldSrcBaselineKind::kCustomEntity
            : entity_number <= baselines.maximum_clients
                ? GoldSrcBaselineKind::kPlayer
                : GoldSrcBaselineKind::kEntity;
        if (baseline->kind != kind)
        {
            result.status = GoldSrcSnapshotCodecStatus::kEntityKindMismatch;
            return result;
        }
        const GoldSrcDeltaTable* entity_table =
            FindTable(registry, TableNameFor(kind), &duplicate);
        if (duplicate || entity_table == nullptr)
        {
            result.status = duplicate
                ? GoldSrcSnapshotCodecStatus::kDuplicateDeltaTable
                : GoldSrcSnapshotCodecStatus::kMissingDeltaTable;
            return result;
        }
        GoldSrcSnapshotEntityState entity;
        entity.entity_index =
            static_cast<std::uint16_t>(entity_number);
        entity.kind = kind;
        result.status = DecodeDeltaRecord(
            &reader,
            *entity_table,
            baseline->state,
            result.frame.server_time,
            &entity.state);
        if (result.status != GoldSrcSnapshotCodecStatus::kOk)
        {
            result.status =
                GoldSrcSnapshotCodecStatus::kInvalidPacketEntities;
            return result;
        }
        result.frame.entities.push_back(std::move(entity));
    }
    if (!reader.ReadBits(16u, &value) || value != 0u)
    {
        result.status =
            GoldSrcSnapshotCodecStatus::kMissingEntityTerminator;
        return result;
    }
    if (!reader.AlignToByte(true))
    {
        result.status = GoldSrcSnapshotCodecStatus::kNonZeroPadding;
        return result;
    }
    if (reader.bits_remaining() != 0u)
    {
        result.status = GoldSrcSnapshotCodecStatus::kTrailingData;
        return result;
    }
    result.bytes_consumed = reader.bytes_read();
    result.status = GoldSrcSnapshotCodecStatus::kOk;
    return result;
}

GoldSrcSnapshotBuildResult BuildGoldSrcFirstSnapshot(
    std::uint32_t frame_id,
    float server_time,
    const GoldSrcBaselineBundle& baselines,
    const GoldSrcDeltaRegistry& registry,
    std::size_t output_capacity) noexcept
{
    GoldSrcSnapshotBuildResult result;
    if (!std::isfinite(server_time) || server_time < 0.0f)
    {
        result.status = GoldSrcSnapshotCodecStatus::kNonFiniteServerTime;
        return result;
    }
    result.bundle.frame.frame_id = frame_id;
    result.bundle.frame.server_time = server_time;

    bool duplicate = false;
    const GoldSrcDeltaTable* clientdata =
        FindTable(registry, "clientdata_t", &duplicate);
    if (duplicate || clientdata == nullptr)
    {
        result.status = duplicate
            ? GoldSrcSnapshotCodecStatus::kDuplicateDeltaTable
            : GoldSrcSnapshotCodecStatus::kMissingDeltaTable;
        return result;
    }
    result.bundle.frame.clientdata.state.field_count =
        clientdata->fields.size();

    for (const GoldSrcEntityBaseline& baseline : baselines.entities)
    {
        if (baseline.entity_index == 0u
            || baseline.entity_index <= baselines.maximum_clients)
        {
            continue;
        }
        if (result.bundle.frame.entities.size()
            >= kGoldSrcMaximumSnapshotEntities)
        {
            result.status =
                GoldSrcSnapshotCodecStatus::kEntityCountExceeded;
            return result;
        }
        GoldSrcSnapshotEntityState entity;
        entity.entity_index = baseline.entity_index;
        entity.kind = baseline.kind;
        entity.state = baseline.state;
        result.bundle.frame.entities.push_back(std::move(entity));
    }

    const GoldSrcSnapshotEncodeResult encoded =
        EncodeGoldSrcFirstSnapshot(
            result.bundle.frame,
            baselines,
            registry,
            output_capacity);
    if (!encoded.ok())
    {
        result.status = encoded.status;
        return result;
    }
    const GoldSrcSnapshotDecodeResult decoded =
        DecodeGoldSrcFirstSnapshot(
            encoded.payload.bytes.data(),
            encoded.payload.size,
            frame_id,
            baselines,
            registry);
    if (!decoded.ok() || !FramesEqual(result.bundle.frame, decoded.frame))
    {
        result.status = decoded.ok()
            ? GoldSrcSnapshotCodecStatus::kInvalidFrame
            : decoded.status;
        return result;
    }
    result.bundle.payload = encoded.payload;
    result.status = GoldSrcSnapshotCodecStatus::kOk;
    return result;
}

std::string_view ReasonFor(GoldSrcFrameStoreResult result) noexcept
{
    switch (result)
    {
    case GoldSrcFrameStoreResult::kStored: return "stored";
    case GoldSrcFrameStoreResult::kInvalidHistory: return "invalid_history";
    case GoldSrcFrameStoreResult::kInvalidFrame: return "invalid_frame";
    case GoldSrcFrameStoreResult::kNonMonotonicFrame:
    default: return "non_monotonic_frame";
    }
}

std::string_view ReasonFor(GoldSrcFrameAcknowledgeResult result) noexcept
{
    switch (result)
    {
    case GoldSrcFrameAcknowledgeResult::kAcknowledged: return "acknowledged";
    case GoldSrcFrameAcknowledgeResult::kDuplicate: return "duplicate";
    case GoldSrcFrameAcknowledgeResult::kUnknown: return "unknown";
    case GoldSrcFrameAcknowledgeResult::kFuture: return "future";
    case GoldSrcFrameAcknowledgeResult::kEvicted: return "evicted";
    case GoldSrcFrameAcknowledgeResult::kStale:
    default: return "stale";
    }
}

GoldSrcClientFrameHistory::GoldSrcClientFrameHistory(
    std::size_t capacity) noexcept
    : capacity_(capacity)
{
}

void GoldSrcClientFrameHistory::Reset() noexcept
{
    frames_.clear();
    last_acknowledged_frame_.reset();
}

GoldSrcFrameStoreResult GoldSrcClientFrameHistory::Store(
    const GoldSrcServerFrame& frame)
{
    if (!valid())
    {
        return GoldSrcFrameStoreResult::kInvalidHistory;
    }
    if (!FrameStructureValid(frame))
    {
        return GoldSrcFrameStoreResult::kInvalidFrame;
    }
    if (!frames_.empty())
    {
        const GoldSrcServerFrame& newest = frames_.back();
        if (frame.frame_id <= newest.frame_id
            || frame.server_time < newest.server_time)
        {
            return GoldSrcFrameStoreResult::kNonMonotonicFrame;
        }
    }

    if (frames_.size() == capacity_)
    {
        frames_.erase(frames_.begin());
    }
    frames_.push_back(frame);
    return GoldSrcFrameStoreResult::kStored;
}

GoldSrcFrameAcknowledgeResult GoldSrcClientFrameHistory::Acknowledge(
    std::uint8_t wire_frame_reference) noexcept
{
    if (frames_.empty())
    {
        return GoldSrcFrameAcknowledgeResult::kUnknown;
    }
    if (last_acknowledged_frame_.has_value()
        && static_cast<std::uint8_t>(
            *last_acknowledged_frame_ & 0xFFu)
            == wire_frame_reference)
    {
        return GoldSrcFrameAcknowledgeResult::kDuplicate;
    }

    const GoldSrcServerFrame* matched = nullptr;
    for (const GoldSrcServerFrame& frame : frames_)
    {
        if (static_cast<std::uint8_t>(frame.frame_id & 0xFFu)
            != wire_frame_reference)
        {
            continue;
        }
        if (matched != nullptr)
        {
            return GoldSrcFrameAcknowledgeResult::kUnknown;
        }
        matched = &frame;
    }
    if (matched != nullptr)
    {
        if (last_acknowledged_frame_.has_value()
            && matched->frame_id < *last_acknowledged_frame_)
        {
            return GoldSrcFrameAcknowledgeResult::kStale;
        }
        last_acknowledged_frame_ = matched->frame_id;
        return GoldSrcFrameAcknowledgeResult::kAcknowledged;
    }

    const GoldSrcServerFrame& oldest = frames_.front();
    const GoldSrcServerFrame& newest = frames_.back();
    std::int64_t candidate =
        static_cast<std::int64_t>(newest.frame_id & ~0xFFu)
        + wire_frame_reference;
    const std::int64_t latest =
        static_cast<std::int64_t>(newest.frame_id);
    if (candidate > latest + 127)
    {
        candidate -= 256;
    }
    else if (candidate + 128 < latest)
    {
        candidate += 256;
    }
    if (candidate > latest)
    {
        return GoldSrcFrameAcknowledgeResult::kFuture;
    }
    if (candidate < static_cast<std::int64_t>(oldest.frame_id))
    {
        return GoldSrcFrameAcknowledgeResult::kEvicted;
    }
    return GoldSrcFrameAcknowledgeResult::kUnknown;
}

bool GoldSrcClientFrameHistory::valid() const noexcept
{
    return capacity_ != 0u
        && capacity_ <= kGoldSrcClientFrameHistoryDepth;
}

std::size_t GoldSrcClientFrameHistory::capacity() const noexcept
{
    return capacity_;
}

std::size_t GoldSrcClientFrameHistory::size() const noexcept
{
    return frames_.size();
}

const GoldSrcServerFrame* GoldSrcClientFrameHistory::Find(
    std::uint32_t frame_id) const noexcept
{
    for (const GoldSrcServerFrame& frame : frames_)
    {
        if (frame.frame_id == frame_id)
        {
            return &frame;
        }
    }
    return nullptr;
}

const std::optional<std::uint32_t>&
GoldSrcClientFrameHistory::last_acknowledged_frame() const noexcept
{
    return last_acknowledged_frame_;
}

std::string_view NameFor(GoldSrcFirstSnapshotPhase phase) noexcept
{
    switch (phase)
    {
    case GoldSrcFirstSnapshotPhase::kAwaitingFirstSnapshot:
        return "awaiting_first_snapshot";
    case GoldSrcFirstSnapshotPhase::kFirstSnapshotPrepared:
        return "first_snapshot_prepared";
    case GoldSrcFirstSnapshotPhase::kFirstSnapshotSentAwaitingClientReference:
        return "first_snapshot_sent_awaiting_client_reference";
    case GoldSrcFirstSnapshotPhase::kFirstSnapshotAcknowledged:
        return "first_snapshot_acknowledged";
    case GoldSrcFirstSnapshotPhase::kNone:
    default:
        return "none";
    }
}

std::string_view ReasonFor(
    GoldSrcFirstSnapshotTransitionResult result) noexcept
{
    switch (result)
    {
    case GoldSrcFirstSnapshotTransitionResult::kAdvanced:
        return "advanced";
    case GoldSrcFirstSnapshotTransitionResult::kAlreadyApplied:
        return "already_applied";
    case GoldSrcFirstSnapshotTransitionResult::kInvalidPhase:
        return "invalid_phase";
    case GoldSrcFirstSnapshotTransitionResult::kInvalidFrame:
    default:
        return "invalid_frame";
    }
}

void GoldSrcFirstSnapshotSessionState::Reset() noexcept
{
    phase_ = GoldSrcFirstSnapshotPhase::kNone;
    prepared_.reset();
    history_.Reset();
}

GoldSrcFirstSnapshotTransitionResult
GoldSrcFirstSnapshotSessionState::EnterAwaiting() noexcept
{
    if (phase_ == GoldSrcFirstSnapshotPhase::kNone)
    {
        phase_ = GoldSrcFirstSnapshotPhase::kAwaitingFirstSnapshot;
        return GoldSrcFirstSnapshotTransitionResult::kAdvanced;
    }
    if (phase_ != GoldSrcFirstSnapshotPhase::kNone)
    {
        return GoldSrcFirstSnapshotTransitionResult::kAlreadyApplied;
    }
    return GoldSrcFirstSnapshotTransitionResult::kInvalidPhase;
}

GoldSrcFirstSnapshotTransitionResult
GoldSrcFirstSnapshotSessionState::Prepare(
    GoldSrcFirstSnapshotBundle bundle)
{
    if (phase_ == GoldSrcFirstSnapshotPhase::kFirstSnapshotPrepared
        || phase_
            == GoldSrcFirstSnapshotPhase::
                kFirstSnapshotSentAwaitingClientReference
        || phase_
            == GoldSrcFirstSnapshotPhase::kFirstSnapshotAcknowledged)
    {
        return GoldSrcFirstSnapshotTransitionResult::kAlreadyApplied;
    }
    if (phase_ != GoldSrcFirstSnapshotPhase::kAwaitingFirstSnapshot)
    {
        return GoldSrcFirstSnapshotTransitionResult::kInvalidPhase;
    }
    if (!FrameStructureValid(bundle.frame)
        || bundle.payload.size == 0u
        || bundle.payload.size > bundle.payload.bytes.size())
    {
        return GoldSrcFirstSnapshotTransitionResult::kInvalidFrame;
    }
    prepared_ = std::move(bundle);
    phase_ = GoldSrcFirstSnapshotPhase::kFirstSnapshotPrepared;
    return GoldSrcFirstSnapshotTransitionResult::kAdvanced;
}

GoldSrcFirstSnapshotTransitionResult
GoldSrcFirstSnapshotSessionState::MarkSent()
{
    if (phase_
        == GoldSrcFirstSnapshotPhase::
            kFirstSnapshotSentAwaitingClientReference
        || phase_
            == GoldSrcFirstSnapshotPhase::kFirstSnapshotAcknowledged)
    {
        return GoldSrcFirstSnapshotTransitionResult::kAlreadyApplied;
    }
    if (phase_ != GoldSrcFirstSnapshotPhase::kFirstSnapshotPrepared
        || !prepared_.has_value())
    {
        return GoldSrcFirstSnapshotTransitionResult::kInvalidPhase;
    }
    if (history_.Store(prepared_->frame)
        != GoldSrcFrameStoreResult::kStored)
    {
        return GoldSrcFirstSnapshotTransitionResult::kInvalidFrame;
    }
    phase_ =
        GoldSrcFirstSnapshotPhase::
            kFirstSnapshotSentAwaitingClientReference;
    return GoldSrcFirstSnapshotTransitionResult::kAdvanced;
}

GoldSrcFrameAcknowledgeResult
GoldSrcFirstSnapshotSessionState::Acknowledge(
    std::uint8_t wire_frame_reference) noexcept
{
    if (phase_
            != GoldSrcFirstSnapshotPhase::
                kFirstSnapshotSentAwaitingClientReference
        && phase_
            != GoldSrcFirstSnapshotPhase::kFirstSnapshotAcknowledged)
    {
        return GoldSrcFrameAcknowledgeResult::kUnknown;
    }
    const GoldSrcFrameAcknowledgeResult result =
        history_.Acknowledge(wire_frame_reference);
    if (result == GoldSrcFrameAcknowledgeResult::kAcknowledged)
    {
        phase_ =
            GoldSrcFirstSnapshotPhase::kFirstSnapshotAcknowledged;
    }
    return result;
}

GoldSrcFirstSnapshotPhase
GoldSrcFirstSnapshotSessionState::phase() const noexcept
{
    return phase_;
}

const std::optional<GoldSrcFirstSnapshotBundle>&
GoldSrcFirstSnapshotSessionState::prepared() const noexcept
{
    return prepared_;
}

const GoldSrcClientFrameHistory&
GoldSrcFirstSnapshotSessionState::history() const noexcept
{
    return history_;
}
} // namespace hl::network
