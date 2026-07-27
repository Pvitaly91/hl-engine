#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace hl::network
{
inline constexpr std::uint8_t kGoldSrcDeltaDescriptionOpcode = 14u;
inline constexpr std::size_t kGoldSrcMaximumDeltaDefinitionBytes = 65536u;
inline constexpr std::size_t kGoldSrcMaximumDeltaTables = 32u;
inline constexpr std::size_t kGoldSrcMaximumDeltaFieldsPerTable = 56u;
inline constexpr std::size_t kGoldSrcMaximumDeltaLayoutFieldsPerTable = 256u;
inline constexpr std::size_t kGoldSrcMaximumDeltaNameBytes = 31u;
inline constexpr std::size_t kGoldSrcMaximumDeltaBundleBytes = 65536u;
inline constexpr std::size_t kGoldSrcMaximumDeltaDescriptorBytes = 64u;
inline constexpr std::size_t kGoldSrcCanonicalDeltaTableCount = 7u;
inline constexpr std::uint32_t kGoldSrcDeltaTypeByte = 0x00000001u;
inline constexpr std::uint32_t kGoldSrcDeltaTypeShort = 0x00000002u;
inline constexpr std::uint32_t kGoldSrcDeltaTypeFloat = 0x00000004u;
inline constexpr std::uint32_t kGoldSrcDeltaTypeInteger = 0x00000008u;
inline constexpr std::uint32_t kGoldSrcDeltaTypeAngle = 0x00000010u;
inline constexpr std::uint32_t kGoldSrcDeltaTypeTimeWindow8 = 0x00000020u;
inline constexpr std::uint32_t kGoldSrcDeltaTypeTimeWindowBig = 0x00000040u;
inline constexpr std::uint32_t kGoldSrcDeltaTypeString = 0x00000080u;
inline constexpr std::uint32_t kGoldSrcDeltaTypeSigned = 0x80000000u;

inline constexpr std::array<
    std::string_view,
    kGoldSrcCanonicalDeltaTableCount>
    kGoldSrcCanonicalDeltaTableOrder = {
        "event_t",
        "weapon_data_t",
        "usercmd_t",
        "custom_entity_state_t",
        "entity_state_player_t",
        "entity_state_t",
        "clientdata_t",
    };

enum class GoldSrcDeltaConditionalEncoderKind
{
    kNone,
    kGameDll,
};

struct GoldSrcDeltaFieldLayout final
{
    std::string name;
    std::uint16_t offset = 0u;
    std::uint8_t size = 0u;
};

struct GoldSrcDeltaTableLayout final
{
    std::string name;
    std::uint32_t structure_size = 0u;
    std::vector<GoldSrcDeltaFieldLayout> fields;
};

struct GoldSrcDeltaLayoutRegistry final
{
    std::vector<GoldSrcDeltaTableLayout> tables;
};

struct GoldSrcDeltaField final
{
    std::uint32_t field_type = 0u;
    std::string name;
    std::uint16_t field_offset = 0u;
    std::uint8_t field_size = 0u;
    std::uint8_t significant_bits = 0u;
    double premultiply = 0.0;
    double postmultiply = 0.0;
};

struct GoldSrcDeltaTable final
{
    std::string name;
    GoldSrcDeltaConditionalEncoderKind conditional_encoder_kind =
        GoldSrcDeltaConditionalEncoderKind::kNone;
    std::string conditional_encoder_name;
    std::vector<GoldSrcDeltaField> fields;
};

struct GoldSrcDeltaRegistry final
{
    std::vector<GoldSrcDeltaTable> tables;
};

enum class GoldSrcDeltaParseStatus
{
    kOk,
    kEmptyInput,
    kInputTooLarge,
    kMissingTableName,
    kNameTooLong,
    kInvalidName,
    kUnsupportedConditionalEncoder,
    kMissingConditionalEncoderName,
    kMissingOpeningBrace,
    kMissingClosingBrace,
    kUnknownDirective,
    kIncompleteDeclaration,
    kUnknownFieldType,
    kDuplicateFieldType,
    kInvalidBitCount,
    kInvalidMultiplier,
    kNumericOverflow,
    kTableCountExceeded,
    kFieldCountExceeded,
    kDuplicateTable,
    kDuplicateField,
    kMissingTableLayout,
    kMissingFieldLayout,
    kInvalidLayout,
    kTrailingData,
};

std::string_view ReasonFor(GoldSrcDeltaParseStatus status) noexcept;

struct GoldSrcDeltaParseResult final
{
    GoldSrcDeltaParseStatus status = GoldSrcDeltaParseStatus::kEmptyInput;
    GoldSrcDeltaRegistry registry;
    std::size_t error_offset = 0u;

    bool ok() const noexcept
    {
        return status == GoldSrcDeltaParseStatus::kOk;
    }
};

GoldSrcDeltaParseResult ParseGoldSrcDeltaDefinitions(
    std::string_view definitions,
    const GoldSrcDeltaLayoutRegistry& layouts);

enum class GoldSrcDeltaEncodeStatus
{
    kOk,
    kMissingRequiredTable,
    kDuplicateRequiredTable,
    kUnexpectedTable,
    kEmptyTable,
    kFieldCountExceeded,
    kInvalidFieldType,
    kInvalidFieldName,
    kInvalidFieldLayout,
    kInvalidBitCount,
    kInvalidMultiplier,
    kMultiplierOutOfRange,
    kDuplicateField,
    kOutputCapacityExceeded,
};

std::string_view ReasonFor(GoldSrcDeltaEncodeStatus status) noexcept;

struct GoldSrcDeltaDescriptorPayload final
{
    std::array<std::uint8_t, kGoldSrcMaximumDeltaDescriptorBytes> bytes{};
    std::size_t size = 0u;
    std::size_t bit_count = 0u;
};

struct GoldSrcDeltaDescriptorEncodeResult final
{
    GoldSrcDeltaEncodeStatus status =
        GoldSrcDeltaEncodeStatus::kInvalidFieldType;
    GoldSrcDeltaDescriptorPayload payload;

    bool ok() const noexcept
    {
        return status == GoldSrcDeltaEncodeStatus::kOk;
    }
};

GoldSrcDeltaDescriptorEncodeResult EncodeGoldSrcDeltaFieldDescriptor(
    const GoldSrcDeltaField& field,
    std::size_t output_capacity =
        kGoldSrcMaximumDeltaDescriptorBytes) noexcept;

struct GoldSrcDeltaBundlePayload final
{
    std::array<std::uint8_t, kGoldSrcMaximumDeltaBundleBytes> bytes{};
    std::size_t size = 0u;
    std::size_t bit_count = 0u;
    std::size_t table_count = 0u;
    std::size_t field_count = 0u;
};

struct GoldSrcDeltaBundleEncodeResult final
{
    GoldSrcDeltaEncodeStatus status =
        GoldSrcDeltaEncodeStatus::kMissingRequiredTable;
    GoldSrcDeltaBundlePayload payload;

    bool ok() const noexcept
    {
        return status == GoldSrcDeltaEncodeStatus::kOk;
    }
};

GoldSrcDeltaBundleEncodeResult EncodeGoldSrcCanonicalDeltaBundle(
    const GoldSrcDeltaRegistry& registry,
    std::size_t output_capacity =
        kGoldSrcMaximumDeltaBundleBytes) noexcept;

enum class GoldSrcDeltaDecodeStatus
{
    kOk,
    kNullInput,
    kEmptyPayload,
    kPayloadTooLarge,
    kTruncatedPayload,
    kWrongOpcode,
    kUnexpectedTable,
    kNameTooLong,
    kInvalidName,
    kInvalidFieldCount,
    kInvalidMaskByteCount,
    kInvalidMask,
    kMissingFieldName,
    kInvalidFieldType,
    kInvalidFieldLayout,
    kInvalidBitCount,
    kInvalidMultiplier,
    kDuplicateField,
    kNonZeroPadding,
    kTrailingData,
};

std::string_view ReasonFor(GoldSrcDeltaDecodeStatus status) noexcept;

struct GoldSrcDeltaBundleDecodeResult final
{
    GoldSrcDeltaDecodeStatus status =
        GoldSrcDeltaDecodeStatus::kEmptyPayload;
    GoldSrcDeltaRegistry registry;
    std::size_t bytes_consumed = 0u;
    std::size_t bits_consumed = 0u;

    bool ok() const noexcept
    {
        return status == GoldSrcDeltaDecodeStatus::kOk;
    }
};

GoldSrcDeltaBundleDecodeResult DecodeGoldSrcCanonicalDeltaBundle(
    const std::uint8_t* bytes,
    std::size_t size);
} // namespace hl::network
