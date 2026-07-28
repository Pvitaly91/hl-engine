#pragma once

#include "network/goldsrc_bitstream.h"
#include "network/goldsrc_delta_description.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace hl::network
{
inline constexpr std::uint8_t kGoldSrcClientMoveOpcode = 2u;
inline constexpr std::size_t kGoldSrcMaximumMoveBodyBytes = 255u;
inline constexpr std::size_t kGoldSrcMaximumMoveCommands = 62u;
inline constexpr std::uint8_t kGoldSrcMaximumReportedPacketLoss = 100u;

enum class GoldSrcDeltaValueKind
{
    kUnsignedInteger,
    kSignedInteger,
    kFloatingPoint,
    kString,
};

struct GoldSrcDecodedDeltaValue final
{
    GoldSrcDeltaValueKind kind = GoldSrcDeltaValueKind::kUnsignedInteger;
    std::uint32_t unsigned_value = 0u;
    std::int32_t signed_value = 0;
    double floating_value = 0.0;
    std::array<char, 256> string_value{};
    std::size_t string_size = 0u;
};

struct GoldSrcDecodedDeltaRecord final
{
    std::array<
        GoldSrcDecodedDeltaValue,
        kGoldSrcMaximumDeltaFieldsPerTable>
        values{};
    std::size_t field_count = 0u;
};

enum class GoldSrcDeltaRecordDecodeStatus
{
    kOk,
    kInvalidSchema,
    kInvalidMask,
    kUnsupportedFieldEncoding,
    kInvalidMultiplier,
    kTruncatedBitstream,
    kStringTooLong,
};

std::string_view ReasonFor(GoldSrcDeltaRecordDecodeStatus status) noexcept;

struct GoldSrcDeltaRecordDecodeResult final
{
    GoldSrcDeltaRecordDecodeStatus status =
        GoldSrcDeltaRecordDecodeStatus::kInvalidSchema;
    GoldSrcDecodedDeltaRecord record;

    bool ok() const noexcept
    {
        return status == GoldSrcDeltaRecordDecodeStatus::kOk;
    }
};

GoldSrcDeltaRecordDecodeResult DecodeGoldSrcDeltaRecord(
    GoldSrcBitReader* reader,
    const GoldSrcDeltaTable& table,
    const GoldSrcDecodedDeltaRecord& previous) noexcept;

struct GoldSrcDecodedUserCommand final
{
    std::uint16_t lerp_msec = 0u;
    std::uint8_t msec = 0u;
    std::array<float, 3> viewangles{};
    std::uint16_t buttons = 0u;
    float forwardmove = 0.0f;
    std::uint8_t lightlevel = 0u;
    float sidemove = 0.0f;
    float upmove = 0.0f;
    std::uint8_t impulse = 0u;
    std::uint32_t impact_index = 0u;
    std::array<float, 3> impact_position{};
    std::uint32_t command_time_msec = 0u;
};

struct GoldSrcDecodedMoveCommand final
{
    std::uint8_t packet_loss = 0u;
    bool voice_loopback = false;
    std::uint8_t backup_command_count = 0u;
    std::uint8_t new_command_count = 0u;
    std::array<GoldSrcDecodedUserCommand, kGoldSrcMaximumMoveCommands> commands{};
    std::size_t command_count = 0u;
    std::uint32_t new_command_time_msec = 0u;
    std::size_t protected_body_size = 0u;
    std::size_t bytes_consumed = 0u;
};

enum class GoldSrcClientMoveDecodeStatus
{
    kOk,
    kNullInput,
    kEmptyPayload,
    kPayloadTooLarge,
    kWrongOpcode,
    kTruncatedEnvelope,
    kInvalidBodyLength,
    kInvalidChecksum,
    kMissingUsercmdSchema,
    kDuplicateUsercmdSchema,
    kInvalidUsercmdSchema,
    kPacketLossOutOfRange,
    kCommandCountExceeded,
    kTruncatedBitstream,
    kInvalidDeltaMask,
    kUnsupportedFieldEncoding,
    kInvalidMultiplier,
    kStringTooLong,
    kNonZeroPadding,
};

std::string_view ReasonFor(GoldSrcClientMoveDecodeStatus status) noexcept;

struct GoldSrcClientMoveDecodeResult final
{
    GoldSrcClientMoveDecodeStatus status =
        GoldSrcClientMoveDecodeStatus::kEmptyPayload;
    GoldSrcDecodedMoveCommand command;

    bool ok() const noexcept
    {
        return status == GoldSrcClientMoveDecodeStatus::kOk;
    }
};

void MungeGoldSrcMoveBody(
    std::uint8_t* bytes,
    std::size_t size,
    std::uint32_t sequence) noexcept;
void UnmungeGoldSrcMoveBody(
    std::uint8_t* bytes,
    std::size_t size,
    std::uint32_t sequence) noexcept;
std::uint8_t GoldSrcMoveChecksum(
    const std::uint8_t* bytes,
    std::size_t size,
    std::uint32_t sequence) noexcept;

GoldSrcClientMoveDecodeResult DecodeGoldSrcClientMoveCommand(
    const std::uint8_t* bytes,
    std::size_t size,
    std::uint32_t sequence,
    const GoldSrcDeltaRegistry& registry) noexcept;

enum class GoldSrcClientApplicationDecodeStatus
{
    kOk,
    kNullInput,
    kEmptyPayload,
    kPayloadTooLarge,
    kUnknownOpcode,
    kMultipleMoveCommands,
    kMalformedMoveCommand,
    kUnsupportedTrailingData,
};

std::string_view ReasonFor(GoldSrcClientApplicationDecodeStatus status) noexcept;

struct GoldSrcClientApplicationDecodeResult final
{
    GoldSrcClientApplicationDecodeStatus status =
        GoldSrcClientApplicationDecodeStatus::kEmptyPayload;
    GoldSrcClientMoveDecodeStatus move_status =
        GoldSrcClientMoveDecodeStatus::kEmptyPayload;
    GoldSrcDecodedMoveCommand move;
    bool move_present = false;
    std::size_t nop_count = 0u;
    std::size_t bytes_consumed = 0u;

    bool ok() const noexcept
    {
        return status == GoldSrcClientApplicationDecodeStatus::kOk;
    }
};

GoldSrcClientApplicationDecodeResult DecodeGoldSrcClientApplicationPayload(
    const std::uint8_t* bytes,
    std::size_t size,
    std::uint32_t sequence,
    const GoldSrcDeltaRegistry& registry) noexcept;
} // namespace hl::network
