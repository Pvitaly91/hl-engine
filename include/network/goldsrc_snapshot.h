#pragma once

#include "network/goldsrc_world_baseline.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace hl::network
{
inline constexpr std::uint8_t kGoldSrcServerTimeOpcode = 7u;
inline constexpr std::uint8_t kGoldSrcClientDataOpcode = 15u;
inline constexpr std::uint8_t kGoldSrcPacketEntitiesOpcode = 40u;
inline constexpr std::uint8_t kGoldSrcDeltaPacketEntitiesOpcode = 41u;
inline constexpr std::uint8_t kGoldSrcClientDeltaOpcode = 4u;
inline constexpr std::size_t kGoldSrcMaximumSnapshotBytes = 1200u;
inline constexpr std::size_t kGoldSrcMaximumSnapshotEntities = 2048u;
inline constexpr std::size_t kGoldSrcMaximumSnapshotWeapons = 64u;
inline constexpr std::size_t kGoldSrcClientFrameHistoryDepth = 64u;

struct GoldSrcClientDataState final
{
    GoldSrcDecodedDeltaRecord state;
};

struct GoldSrcWeaponState final
{
    std::uint8_t weapon_index = 0u;
    GoldSrcDecodedDeltaRecord state;
};

struct GoldSrcSnapshotEntityState final
{
    std::uint16_t entity_index = 0u;
    GoldSrcBaselineKind kind = GoldSrcBaselineKind::kEntity;
    GoldSrcDecodedDeltaRecord state;
};

struct GoldSrcServerFrame final
{
    std::uint32_t frame_id = 0u;
    float server_time = 0.0f;
    GoldSrcClientDataState clientdata;
    std::vector<GoldSrcWeaponState> weapons;
    std::vector<GoldSrcSnapshotEntityState> entities;
};

struct GoldSrcFirstSnapshotPayload final
{
    std::array<std::uint8_t, kGoldSrcMaximumSnapshotBytes> bytes{};
    std::size_t size = 0u;
};

struct GoldSrcFirstSnapshotBundle final
{
    GoldSrcServerFrame frame;
    GoldSrcFirstSnapshotPayload payload;
};

enum class GoldSrcSnapshotCodecStatus
{
    kOk,
    kNullInput,
    kEmptyPayload,
    kPayloadTooLarge,
    kInvalidFrame,
    kNonFiniteServerTime,
    kNonMonotonicServerTime,
    kMissingDeltaTable,
    kDuplicateDeltaTable,
    kInvalidDeltaState,
    kUnsupportedFieldEncoding,
    kValueOutOfRange,
    kInvalidWeaponIndex,
    kWeaponOrderInvalid,
    kEntityCountExceeded,
    kInvalidEntityIndex,
    kDuplicateEntity,
    kEntityOrderInvalid,
    kMissingEntityBaseline,
    kEntityKindMismatch,
    kWrongMessageOrder,
    kUnexpectedPreviousFrame,
    kInvalidClientData,
    kInvalidWeaponData,
    kInvalidPacketEntities,
    kUnexpectedDeltaPacket,
    kMissingEntityTerminator,
    kNonZeroPadding,
    kTrailingData,
    kOutputCapacityExceeded,
};

std::string_view ReasonFor(GoldSrcSnapshotCodecStatus status) noexcept;

struct GoldSrcSnapshotEncodeResult final
{
    GoldSrcSnapshotCodecStatus status =
        GoldSrcSnapshotCodecStatus::kInvalidFrame;
    GoldSrcFirstSnapshotPayload payload;

    bool ok() const noexcept
    {
        return status == GoldSrcSnapshotCodecStatus::kOk;
    }
};

struct GoldSrcSnapshotDecodeResult final
{
    GoldSrcSnapshotCodecStatus status =
        GoldSrcSnapshotCodecStatus::kEmptyPayload;
    GoldSrcServerFrame frame;
    std::size_t bytes_consumed = 0u;

    bool ok() const noexcept
    {
        return status == GoldSrcSnapshotCodecStatus::kOk;
    }
};

struct GoldSrcSnapshotBuildResult final
{
    GoldSrcSnapshotCodecStatus status =
        GoldSrcSnapshotCodecStatus::kInvalidFrame;
    GoldSrcFirstSnapshotBundle bundle;

    bool ok() const noexcept
    {
        return status == GoldSrcSnapshotCodecStatus::kOk;
    }
};

GoldSrcSnapshotEncodeResult EncodeGoldSrcFirstSnapshot(
    const GoldSrcServerFrame& frame,
    const GoldSrcBaselineBundle& baselines,
    const GoldSrcDeltaRegistry& registry,
    std::size_t output_capacity = kGoldSrcMaximumSnapshotBytes) noexcept;

GoldSrcSnapshotDecodeResult DecodeGoldSrcFirstSnapshot(
    const std::uint8_t* bytes,
    std::size_t size,
    std::uint32_t frame_id,
    const GoldSrcBaselineBundle& baselines,
    const GoldSrcDeltaRegistry& registry) noexcept;

GoldSrcSnapshotBuildResult BuildGoldSrcFirstSnapshot(
    std::uint32_t frame_id,
    float server_time,
    const GoldSrcBaselineBundle& baselines,
    const GoldSrcDeltaRegistry& registry,
    std::size_t output_capacity = kGoldSrcMaximumSnapshotBytes) noexcept;

enum class GoldSrcFrameStoreResult
{
    kStored,
    kInvalidHistory,
    kInvalidFrame,
    kNonMonotonicFrame,
};

std::string_view ReasonFor(GoldSrcFrameStoreResult result) noexcept;

enum class GoldSrcFrameAcknowledgeResult
{
    kAcknowledged,
    kDuplicate,
    kUnknown,
    kFuture,
    kEvicted,
    kStale,
};

std::string_view ReasonFor(GoldSrcFrameAcknowledgeResult result) noexcept;

class GoldSrcClientFrameHistory final
{
public:
    explicit GoldSrcClientFrameHistory(
        std::size_t capacity = kGoldSrcClientFrameHistoryDepth) noexcept;

    void Reset() noexcept;
    GoldSrcFrameStoreResult Store(const GoldSrcServerFrame& frame);
    GoldSrcFrameAcknowledgeResult Acknowledge(
        std::uint8_t wire_frame_reference) noexcept;

    bool valid() const noexcept;
    std::size_t capacity() const noexcept;
    std::size_t size() const noexcept;
    const GoldSrcServerFrame* Find(std::uint32_t frame_id) const noexcept;
    const std::optional<std::uint32_t>&
        last_acknowledged_frame() const noexcept;

private:
    std::vector<GoldSrcServerFrame> frames_;
    std::size_t capacity_ = 0u;
    std::optional<std::uint32_t> last_acknowledged_frame_;
};

enum class GoldSrcFirstSnapshotPhase
{
    kNone,
    kAwaitingFirstSnapshot,
    kFirstSnapshotPrepared,
    kFirstSnapshotSentAwaitingClientReference,
    kFirstSnapshotAcknowledged,
};

std::string_view NameFor(GoldSrcFirstSnapshotPhase phase) noexcept;

enum class GoldSrcFirstSnapshotTransitionResult
{
    kAdvanced,
    kAlreadyApplied,
    kInvalidPhase,
    kInvalidFrame,
};

std::string_view ReasonFor(
    GoldSrcFirstSnapshotTransitionResult result) noexcept;

class GoldSrcFirstSnapshotSessionState final
{
public:
    void Reset() noexcept;
    GoldSrcFirstSnapshotTransitionResult EnterAwaiting() noexcept;
    GoldSrcFirstSnapshotTransitionResult Prepare(
        GoldSrcFirstSnapshotBundle bundle);
    GoldSrcFirstSnapshotTransitionResult MarkSent();
    GoldSrcFrameAcknowledgeResult Acknowledge(
        std::uint8_t wire_frame_reference) noexcept;

    GoldSrcFirstSnapshotPhase phase() const noexcept;
    const std::optional<GoldSrcFirstSnapshotBundle>& prepared() const noexcept;
    const GoldSrcClientFrameHistory& history() const noexcept;

private:
    GoldSrcFirstSnapshotPhase phase_ = GoldSrcFirstSnapshotPhase::kNone;
    std::optional<GoldSrcFirstSnapshotBundle> prepared_;
    GoldSrcClientFrameHistory history_;
};
} // namespace hl::network
