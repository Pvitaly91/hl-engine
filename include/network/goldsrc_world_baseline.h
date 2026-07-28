#pragma once

#include "network/goldsrc_client_move.h"
#include "network/goldsrc_delta_description.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace hl::network
{
inline constexpr std::uint8_t kGoldSrcSpawnBaselineOpcode = 22u;
inline constexpr std::uint8_t kGoldSrcSignonNumberOpcode = 25u;
inline constexpr std::uint8_t kGoldSrcBaselineSignonNumber = 1u;
inline constexpr std::uint16_t kGoldSrcBaselineTerminator = 0xFFFFu;
inline constexpr std::uint16_t kGoldSrcMaximumEntityIndex = 2047u;
inline constexpr std::uint16_t kGoldSrcMaximumModelIndex = 1023u;
inline constexpr std::size_t kGoldSrcMaximumEntityBaselines = 2048u;
inline constexpr std::size_t kGoldSrcMaximumInstancedBaselines = 63u;
inline constexpr std::size_t kGoldSrcMaximumBaselineBundleBytes = 65536u;

enum class GoldSrcBaselineKind
{
    kWorld,
    kPlayer,
    kEntity,
    kCustomEntity,
    kInstanced,
};

enum class GoldSrcBaselineProvenance
{
    kRuntimeMap,
    kSyntheticFixture,
};

struct GoldSrcEntityBaseline final
{
    std::uint16_t entity_index = 0u;
    GoldSrcBaselineKind kind = GoldSrcBaselineKind::kEntity;
    std::uint16_t model_index = 0u;
    GoldSrcDecodedDeltaRecord state;
    GoldSrcBaselineProvenance provenance =
        GoldSrcBaselineProvenance::kRuntimeMap;
};

struct GoldSrcBaselineBundle final
{
    std::vector<GoldSrcEntityBaseline> entities;
    std::vector<GoldSrcEntityBaseline> instances;
    std::uint16_t maximum_clients = 0u;
};

enum class GoldSrcBaselineBuildStatus
{
    kOk,
    kMissingWorld,
    kInvalidWorld,
    kInvalidEntityIndex,
    kInvalidModelIndex,
    kInvalidPlayerSlot,
    kDuplicateEntityIndex,
    kDuplicateInstanceIdentity,
    kBaselineCountExceeded,
    kInstanceCountExceeded,
    kInvalidState,
};

std::string_view ReasonFor(GoldSrcBaselineBuildStatus status) noexcept;

struct GoldSrcBaselineBuildResult final
{
    GoldSrcBaselineBuildStatus status =
        GoldSrcBaselineBuildStatus::kMissingWorld;
    GoldSrcBaselineBundle bundle;

    bool ok() const noexcept
    {
        return status == GoldSrcBaselineBuildStatus::kOk;
    }
};

GoldSrcBaselineBuildStatus ValidateGoldSrcBaselineBundle(
    const GoldSrcBaselineBundle& bundle) noexcept;

enum class GoldSrcBaselineEncodeStatus
{
    kOk,
    kInvalidBundle,
    kMissingDeltaTable,
    kDuplicateDeltaTable,
    kInvalidDeltaState,
    kUnsupportedFieldEncoding,
    kValueOutOfRange,
    kOutputCapacityExceeded,
};

std::string_view ReasonFor(GoldSrcBaselineEncodeStatus status) noexcept;

struct GoldSrcBaselinePayload final
{
    std::array<std::uint8_t, kGoldSrcMaximumBaselineBundleBytes> bytes{};
    std::size_t size = 0u;
    std::size_t bit_count = 0u;
    std::size_t entity_count = 0u;
    std::size_t instance_count = 0u;
};

struct GoldSrcBaselineEncodeResult final
{
    GoldSrcBaselineEncodeStatus status =
        GoldSrcBaselineEncodeStatus::kInvalidBundle;
    GoldSrcBaselinePayload payload;

    bool ok() const noexcept
    {
        return status == GoldSrcBaselineEncodeStatus::kOk;
    }
};

GoldSrcBaselineEncodeResult EncodeGoldSrcBaselineBundle(
    const GoldSrcBaselineBundle& bundle,
    const GoldSrcDeltaRegistry& registry,
    std::size_t output_capacity =
        kGoldSrcMaximumBaselineBundleBytes,
    double time_base = 1.0) noexcept;

enum class GoldSrcBaselineDecodeStatus
{
    kOk,
    kNullInput,
    kEmptyPayload,
    kPayloadTooLarge,
    kWrongOpcode,
    kInvalidEntityIndex,
    kInvalidEntityType,
    kDuplicateEntityIndex,
    kMissingDeltaTable,
    kDuplicateDeltaTable,
    kInvalidDelta,
    kMissingTerminator,
    kMissingSignonMarker,
    kNonZeroPadding,
    kTrailingData,
};

std::string_view ReasonFor(GoldSrcBaselineDecodeStatus status) noexcept;

struct GoldSrcBaselineDecodeResult final
{
    GoldSrcBaselineDecodeStatus status =
        GoldSrcBaselineDecodeStatus::kEmptyPayload;
    GoldSrcBaselineBundle bundle;
    std::size_t bytes_consumed = 0u;

    bool ok() const noexcept
    {
        return status == GoldSrcBaselineDecodeStatus::kOk;
    }
};

GoldSrcBaselineDecodeResult DecodeGoldSrcBaselineBundle(
    const std::uint8_t* bytes,
    std::size_t size,
    std::uint16_t maximum_clients,
    const GoldSrcDeltaRegistry& registry,
    double time_base = 1.0) noexcept;
} // namespace hl::network
