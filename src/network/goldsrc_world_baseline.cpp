#include "network/goldsrc_world_baseline.h"

#include "network/goldsrc_bitstream.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>

namespace hl::network
{
namespace
{
constexpr std::uint32_t kEntityNormal = 1u;
constexpr std::uint32_t kEntityBeam = 2u;
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

bool IsZero(const GoldSrcDecodedDeltaValue& value) noexcept
{
    if (value.kind == GoldSrcDeltaValueKind::kString)
    {
        return value.string_size == 0u;
    }
    double numeric = 0.0;
    return NumericValue(value, &numeric) && numeric == 0.0;
}

enum class FieldEncodeResult
{
    kOk,
    kUnsupported,
    kOutOfRange,
    kCapacity,
};

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

GoldSrcBaselineEncodeStatus EncodeRecord(
    GoldSrcBitWriter* writer,
    const GoldSrcDeltaTable& table,
    const GoldSrcDecodedDeltaRecord& record,
    double time_base) noexcept
{
    if (writer == nullptr || table.fields.empty()
        || table.fields.size() > kGoldSrcMaximumDeltaFieldsPerTable
        || record.field_count != table.fields.size())
    {
        return GoldSrcBaselineEncodeStatus::kInvalidDeltaState;
    }

    std::array<std::uint8_t, 7> mask{};
    std::size_t mask_bytes = 0u;
    for (std::size_t index = 0; index < table.fields.size(); ++index)
    {
        if (!IsZero(record.values[index]))
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
        return GoldSrcBaselineEncodeStatus::kOutputCapacityExceeded;
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
            record.values[index],
            time_base))
        {
        case FieldEncodeResult::kOk:
            break;
        case FieldEncodeResult::kCapacity:
            return GoldSrcBaselineEncodeStatus::kOutputCapacityExceeded;
        case FieldEncodeResult::kOutOfRange:
            return GoldSrcBaselineEncodeStatus::kValueOutOfRange;
        case FieldEncodeResult::kUnsupported:
        default:
            return GoldSrcBaselineEncodeStatus::kUnsupportedFieldEncoding;
        }
    }
    return GoldSrcBaselineEncodeStatus::kOk;
}

GoldSrcBaselineDecodeStatus DecodeRecord(
    GoldSrcBitReader* reader,
    const GoldSrcDeltaTable& table,
    double time_base,
    GoldSrcDecodedDeltaRecord* state) noexcept
{
    GoldSrcDecodedDeltaRecord zero;
    zero.field_count = table.fields.size();
    const GoldSrcDeltaRecordDecodeResult decoded =
        DecodeGoldSrcDeltaRecord(
            reader,
            table,
            zero,
            time_base);
    if (!decoded.ok())
    {
        return GoldSrcBaselineDecodeStatus::kInvalidDelta;
    }
    *state = decoded.record;
    return GoldSrcBaselineDecodeStatus::kOk;
}
} // namespace

std::string_view ReasonFor(GoldSrcBaselineBuildStatus status) noexcept
{
    switch (status)
    {
    case GoldSrcBaselineBuildStatus::kOk: return "ok";
    case GoldSrcBaselineBuildStatus::kMissingWorld: return "missing_world";
    case GoldSrcBaselineBuildStatus::kInvalidWorld: return "invalid_world";
    case GoldSrcBaselineBuildStatus::kInvalidEntityIndex: return "invalid_entity_index";
    case GoldSrcBaselineBuildStatus::kInvalidModelIndex: return "invalid_model_index";
    case GoldSrcBaselineBuildStatus::kInvalidPlayerSlot: return "invalid_player_slot";
    case GoldSrcBaselineBuildStatus::kDuplicateEntityIndex: return "duplicate_entity_index";
    case GoldSrcBaselineBuildStatus::kDuplicateInstanceIdentity: return "duplicate_instance_identity";
    case GoldSrcBaselineBuildStatus::kBaselineCountExceeded: return "baseline_count_exceeded";
    case GoldSrcBaselineBuildStatus::kInstanceCountExceeded: return "instance_count_exceeded";
    case GoldSrcBaselineBuildStatus::kInvalidState:
    default: return "invalid_state";
    }
}

GoldSrcBaselineBuildStatus ValidateGoldSrcBaselineBundle(
    const GoldSrcBaselineBundle& bundle) noexcept
{
    if (bundle.entities.empty()
        || bundle.entities.front().kind != GoldSrcBaselineKind::kWorld
        || bundle.entities.front().entity_index != 0u)
    {
        return GoldSrcBaselineBuildStatus::kMissingWorld;
    }
    if (bundle.entities.size() > kGoldSrcMaximumEntityBaselines)
    {
        return GoldSrcBaselineBuildStatus::kBaselineCountExceeded;
    }
    if (bundle.instances.size() > kGoldSrcMaximumInstancedBaselines)
    {
        return GoldSrcBaselineBuildStatus::kInstanceCountExceeded;
    }

    std::array<bool, kGoldSrcMaximumEntityIndex + 1u> seen{};
    std::uint16_t previous = 0u;
    bool first = true;
    for (const GoldSrcEntityBaseline& baseline : bundle.entities)
    {
        if (baseline.entity_index > kGoldSrcMaximumEntityIndex)
        {
            return GoldSrcBaselineBuildStatus::kInvalidEntityIndex;
        }
        if (seen[baseline.entity_index])
        {
            return GoldSrcBaselineBuildStatus::kDuplicateEntityIndex;
        }
        if (!first && baseline.entity_index <= previous)
        {
            return GoldSrcBaselineBuildStatus::kInvalidEntityIndex;
        }
        seen[baseline.entity_index] = true;
        previous = baseline.entity_index;
        first = false;
        if (baseline.model_index == 0u
            || baseline.model_index > kGoldSrcMaximumModelIndex)
        {
            return GoldSrcBaselineBuildStatus::kInvalidModelIndex;
        }
        if (baseline.kind == GoldSrcBaselineKind::kWorld
            && baseline.entity_index != 0u)
        {
            return GoldSrcBaselineBuildStatus::kInvalidWorld;
        }
        if (baseline.kind == GoldSrcBaselineKind::kPlayer
            && (baseline.entity_index == 0u
                || baseline.entity_index > bundle.maximum_clients))
        {
            return GoldSrcBaselineBuildStatus::kInvalidPlayerSlot;
        }
        if (baseline.state.field_count == 0u)
        {
            return GoldSrcBaselineBuildStatus::kInvalidState;
        }
    }
    for (std::size_t index = 0; index < bundle.instances.size(); ++index)
    {
        const GoldSrcEntityBaseline& instance = bundle.instances[index];
        if (instance.kind != GoldSrcBaselineKind::kInstanced
            || instance.model_index == 0u
            || instance.model_index > kGoldSrcMaximumModelIndex
            || instance.state.field_count == 0u)
        {
            return GoldSrcBaselineBuildStatus::kInvalidState;
        }
        for (std::size_t prior = 0; prior < index; ++prior)
        {
            if (bundle.instances[prior].entity_index
                == instance.entity_index)
            {
                return GoldSrcBaselineBuildStatus::
                    kDuplicateInstanceIdentity;
            }
        }
    }
    return GoldSrcBaselineBuildStatus::kOk;
}

std::string_view ReasonFor(GoldSrcBaselineEncodeStatus status) noexcept
{
    switch (status)
    {
    case GoldSrcBaselineEncodeStatus::kOk: return "ok";
    case GoldSrcBaselineEncodeStatus::kInvalidBundle: return "invalid_bundle";
    case GoldSrcBaselineEncodeStatus::kMissingDeltaTable: return "missing_delta_table";
    case GoldSrcBaselineEncodeStatus::kDuplicateDeltaTable: return "duplicate_delta_table";
    case GoldSrcBaselineEncodeStatus::kInvalidDeltaState: return "invalid_delta_state";
    case GoldSrcBaselineEncodeStatus::kUnsupportedFieldEncoding: return "unsupported_field_encoding";
    case GoldSrcBaselineEncodeStatus::kValueOutOfRange: return "value_out_of_range";
    case GoldSrcBaselineEncodeStatus::kOutputCapacityExceeded:
    default: return "output_capacity_exceeded";
    }
}

GoldSrcBaselineEncodeResult EncodeGoldSrcBaselineBundle(
    const GoldSrcBaselineBundle& bundle,
    const GoldSrcDeltaRegistry& registry,
    std::size_t output_capacity,
    double time_base) noexcept
{
    GoldSrcBaselineEncodeResult result;
    if (!std::isfinite(time_base)
        || ValidateGoldSrcBaselineBundle(bundle)
        != GoldSrcBaselineBuildStatus::kOk)
    {
        return result;
    }
    if (output_capacity == 0u
        || output_capacity > result.payload.bytes.size())
    {
        result.status =
            GoldSrcBaselineEncodeStatus::kOutputCapacityExceeded;
        return result;
    }

    GoldSrcBitWriter writer(result.payload.bytes.data(), output_capacity);
    if (!writer.WriteBits(kGoldSrcSpawnBaselineOpcode, 8u))
    {
        result.status =
            GoldSrcBaselineEncodeStatus::kOutputCapacityExceeded;
        return result;
    }
    for (const GoldSrcEntityBaseline& baseline : bundle.entities)
    {
        bool duplicate = false;
        const GoldSrcDeltaTable* table =
            FindTable(registry, TableNameFor(baseline.kind), &duplicate);
        if (duplicate || table == nullptr)
        {
            result.status = duplicate
                ? GoldSrcBaselineEncodeStatus::kDuplicateDeltaTable
                : GoldSrcBaselineEncodeStatus::kMissingDeltaTable;
            return result;
        }
        const std::uint32_t entity_type =
            baseline.kind == GoldSrcBaselineKind::kCustomEntity
            ? kEntityBeam
            : kEntityNormal;
        if (!writer.WriteBits(baseline.entity_index, 11u)
            || !writer.WriteBits(entity_type, 2u))
        {
            result.status =
                GoldSrcBaselineEncodeStatus::kOutputCapacityExceeded;
            return result;
        }
        result.status =
            EncodeRecord(&writer, *table, baseline.state, time_base);
        if (result.status != GoldSrcBaselineEncodeStatus::kOk)
        {
            return result;
        }
    }
    if (!writer.WriteBits(kGoldSrcBaselineTerminator, 16u)
        || !writer.WriteBits(
            static_cast<std::uint32_t>(bundle.instances.size()),
            6u))
    {
        result.status =
            GoldSrcBaselineEncodeStatus::kOutputCapacityExceeded;
        return result;
    }
    bool duplicate = false;
    const GoldSrcDeltaTable* instance_table =
        FindTable(registry, "entity_state_t", &duplicate);
    if (!bundle.instances.empty() && (duplicate || instance_table == nullptr))
    {
        result.status = duplicate
            ? GoldSrcBaselineEncodeStatus::kDuplicateDeltaTable
            : GoldSrcBaselineEncodeStatus::kMissingDeltaTable;
        return result;
    }
    for (const GoldSrcEntityBaseline& instance : bundle.instances)
    {
        result.status =
            EncodeRecord(
                &writer,
                *instance_table,
                instance.state,
                time_base);
        if (result.status != GoldSrcBaselineEncodeStatus::kOk)
        {
            return result;
        }
    }
    if (!writer.PadToByte()
        || !writer.WriteBits(kGoldSrcSignonNumberOpcode, 8u)
        || !writer.WriteBits(kGoldSrcBaselineSignonNumber, 8u))
    {
        result.status =
            GoldSrcBaselineEncodeStatus::kOutputCapacityExceeded;
        return result;
    }

    result.payload.size = writer.bytes_written();
    result.payload.bit_count = writer.bit_position();
    result.payload.entity_count = bundle.entities.size();
    result.payload.instance_count = bundle.instances.size();
    result.status = GoldSrcBaselineEncodeStatus::kOk;
    return result;
}

std::string_view ReasonFor(GoldSrcBaselineDecodeStatus status) noexcept
{
    switch (status)
    {
    case GoldSrcBaselineDecodeStatus::kOk: return "ok";
    case GoldSrcBaselineDecodeStatus::kNullInput: return "null_input";
    case GoldSrcBaselineDecodeStatus::kEmptyPayload: return "empty_payload";
    case GoldSrcBaselineDecodeStatus::kPayloadTooLarge: return "payload_too_large";
    case GoldSrcBaselineDecodeStatus::kWrongOpcode: return "wrong_opcode";
    case GoldSrcBaselineDecodeStatus::kInvalidEntityIndex: return "invalid_entity_index";
    case GoldSrcBaselineDecodeStatus::kInvalidEntityType: return "invalid_entity_type";
    case GoldSrcBaselineDecodeStatus::kDuplicateEntityIndex: return "duplicate_entity_index";
    case GoldSrcBaselineDecodeStatus::kMissingDeltaTable: return "missing_delta_table";
    case GoldSrcBaselineDecodeStatus::kDuplicateDeltaTable: return "duplicate_delta_table";
    case GoldSrcBaselineDecodeStatus::kInvalidDelta: return "invalid_delta";
    case GoldSrcBaselineDecodeStatus::kMissingTerminator: return "missing_terminator";
    case GoldSrcBaselineDecodeStatus::kMissingSignonMarker: return "missing_signon_marker";
    case GoldSrcBaselineDecodeStatus::kNonZeroPadding: return "non_zero_padding";
    case GoldSrcBaselineDecodeStatus::kTrailingData:
    default: return "trailing_data";
    }
}

GoldSrcBaselineDecodeResult DecodeGoldSrcBaselineBundle(
    const std::uint8_t* bytes,
    std::size_t size,
    std::uint16_t maximum_clients,
    const GoldSrcDeltaRegistry& registry,
    double time_base) noexcept
{
    GoldSrcBaselineDecodeResult result;
    result.bundle.maximum_clients = maximum_clients;
    if (!std::isfinite(time_base))
    {
        result.status = GoldSrcBaselineDecodeStatus::kInvalidDelta;
        return result;
    }
    if (bytes == nullptr)
    {
        result.status = GoldSrcBaselineDecodeStatus::kNullInput;
        return result;
    }
    if (size == 0u)
    {
        return result;
    }
    if (size > kGoldSrcMaximumBaselineBundleBytes)
    {
        result.status = GoldSrcBaselineDecodeStatus::kPayloadTooLarge;
        return result;
    }

    GoldSrcBitReader reader(bytes, size);
    std::uint32_t value = 0u;
    if (!reader.ReadBits(8u, &value)
        || value != kGoldSrcSpawnBaselineOpcode)
    {
        result.status = GoldSrcBaselineDecodeStatus::kWrongOpcode;
        return result;
    }
    std::array<bool, kGoldSrcMaximumEntityIndex + 1u> seen{};
    bool terminated = false;
    while (result.bundle.entities.size()
        < kGoldSrcMaximumEntityBaselines)
    {
        GoldSrcBitReader peek = reader;
        if (!peek.ReadBits(16u, &value))
        {
            result.status =
                GoldSrcBaselineDecodeStatus::kMissingTerminator;
            return result;
        }
        if (value == kGoldSrcBaselineTerminator)
        {
            reader = peek;
            terminated = true;
            break;
        }

        std::uint32_t entity_index = 0u;
        std::uint32_t entity_type = 0u;
        if (!reader.ReadBits(11u, &entity_index)
            || !reader.ReadBits(2u, &entity_type)
            || entity_index > kGoldSrcMaximumEntityIndex)
        {
            result.status =
                GoldSrcBaselineDecodeStatus::kInvalidEntityIndex;
            return result;
        }
        if (entity_type != kEntityNormal && entity_type != kEntityBeam)
        {
            result.status =
                GoldSrcBaselineDecodeStatus::kInvalidEntityType;
            return result;
        }
        if (seen[entity_index])
        {
            result.status =
                GoldSrcBaselineDecodeStatus::kDuplicateEntityIndex;
            return result;
        }
        seen[entity_index] = true;

        GoldSrcEntityBaseline baseline;
        baseline.entity_index =
            static_cast<std::uint16_t>(entity_index);
        baseline.kind = entity_type == kEntityBeam
            ? GoldSrcBaselineKind::kCustomEntity
            : entity_index == 0u
                ? GoldSrcBaselineKind::kWorld
                : entity_index <= maximum_clients
                    ? GoldSrcBaselineKind::kPlayer
                    : GoldSrcBaselineKind::kEntity;
        bool duplicate = false;
        const GoldSrcDeltaTable* table =
            FindTable(registry, TableNameFor(baseline.kind), &duplicate);
        if (duplicate || table == nullptr)
        {
            result.status = duplicate
                ? GoldSrcBaselineDecodeStatus::kDuplicateDeltaTable
                : GoldSrcBaselineDecodeStatus::kMissingDeltaTable;
            return result;
        }
        result.status =
            DecodeRecord(
                &reader,
                *table,
                time_base,
                &baseline.state);
        if (result.status != GoldSrcBaselineDecodeStatus::kOk)
        {
            return result;
        }
        for (std::size_t index = 0; index < table->fields.size(); ++index)
        {
            if (table->fields[index].name != "modelindex")
            {
                continue;
            }
            const GoldSrcDecodedDeltaValue& model =
                baseline.state.values[index];
            baseline.model_index = static_cast<std::uint16_t>(
                model.kind == GoldSrcDeltaValueKind::kUnsignedInteger
                ? model.unsigned_value
                : model.kind == GoldSrcDeltaValueKind::kSignedInteger
                    && model.signed_value >= 0
                    ? static_cast<std::uint32_t>(model.signed_value)
                    : 0u);
        }
        result.bundle.entities.push_back(std::move(baseline));
    }
    if (!terminated)
    {
        result.status = GoldSrcBaselineDecodeStatus::kMissingTerminator;
        return result;
    }

    std::uint32_t instance_count = 0u;
    if (!reader.ReadBits(6u, &instance_count))
    {
        result.status = GoldSrcBaselineDecodeStatus::kInvalidDelta;
        return result;
    }
    bool duplicate = false;
    const GoldSrcDeltaTable* instance_table =
        FindTable(registry, "entity_state_t", &duplicate);
    if (instance_count != 0u && (duplicate || instance_table == nullptr))
    {
        result.status = duplicate
            ? GoldSrcBaselineDecodeStatus::kDuplicateDeltaTable
            : GoldSrcBaselineDecodeStatus::kMissingDeltaTable;
        return result;
    }
    for (std::uint32_t index = 0; index < instance_count; ++index)
    {
        GoldSrcEntityBaseline instance;
        instance.kind = GoldSrcBaselineKind::kInstanced;
        instance.entity_index = static_cast<std::uint16_t>(index);
        result.status =
            DecodeRecord(
                &reader,
                *instance_table,
                time_base,
                &instance.state);
        if (result.status != GoldSrcBaselineDecodeStatus::kOk)
        {
            return result;
        }
        result.bundle.instances.push_back(std::move(instance));
    }
    if (!reader.AlignToByte(true))
    {
        result.status = GoldSrcBaselineDecodeStatus::kNonZeroPadding;
        return result;
    }
    std::uint32_t opcode = 0u;
    std::uint32_t signon = 0u;
    if (!reader.ReadBits(8u, &opcode)
        || !reader.ReadBits(8u, &signon)
        || opcode != kGoldSrcSignonNumberOpcode
        || signon != kGoldSrcBaselineSignonNumber)
    {
        result.status =
            GoldSrcBaselineDecodeStatus::kMissingSignonMarker;
        return result;
    }
    if (reader.bits_remaining() != 0u)
    {
        result.status = GoldSrcBaselineDecodeStatus::kTrailingData;
        return result;
    }
    result.bytes_consumed = reader.bytes_read();
    result.status = GoldSrcBaselineDecodeStatus::kOk;
    return result;
}
} // namespace hl::network
