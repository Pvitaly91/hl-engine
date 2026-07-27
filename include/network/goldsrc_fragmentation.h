#pragma once

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace hl::network
{
inline constexpr std::size_t kGoldSrcFragmentStreamCount = 2u;
inline constexpr std::size_t kGoldSrcFragmentPresenceBytes = 1u;
inline constexpr std::size_t kGoldSrcFragmentDescriptorBytes = 9u;
inline constexpr std::size_t kGoldSrcNormalOnlyFragmentMetadataBytes = 10u;
inline constexpr std::size_t kGoldSrcMaximumFragmentBytes = 1024u;
inline constexpr std::size_t kGoldSrcMaximumFragmentTransferBytes = 65536u;
inline constexpr std::size_t kGoldSrcMaximumNormalFragmentCount =
    kGoldSrcMaximumFragmentTransferBytes / kGoldSrcMaximumFragmentBytes;
inline constexpr auto kGoldSrcFragmentTransferTimeout =
    std::chrono::seconds(30);

enum class GoldSrcFragmentStream : std::uint8_t
{
    kNormal = 0u,
    kFile = 1u,
};

enum class GoldSrcFragmentCodecStatus
{
    kOk,
    kNullInput,
    kTruncated,
    kInvalidPresence,
    kNoFragment,
    kInvalidFragmentIndex,
    kInvalidFragmentCount,
    kFragmentCountExceeded,
    kInvalidOffset,
    kInvalidLength,
    kFragmentOutsidePayload,
    kOverlappingStreams,
    kUnsupportedStream,
    kPayloadTooLarge,
};

std::string_view ReasonFor(GoldSrcFragmentCodecStatus status) noexcept;

struct GoldSrcFragmentDescriptor final
{
    GoldSrcFragmentStream stream = GoldSrcFragmentStream::kNormal;
    std::uint8_t raw_presence = 0u;
    std::uint32_t raw_fragment_id = 0u;
    std::uint16_t fragment_index = 0u;
    std::uint16_t fragment_count = 0u;
    std::uint16_t payload_offset = 0u;
    std::uint16_t payload_length = 0u;
};

struct GoldSrcFragmentMetadata final
{
    std::array<GoldSrcFragmentDescriptor, kGoldSrcFragmentStreamCount>
        descriptors{};
    std::array<bool, kGoldSrcFragmentStreamCount> present{};
    std::size_t encoded_size = 0u;
    std::size_t present_count = 0u;
};

struct GoldSrcFragmentDecodeResult final
{
    GoldSrcFragmentCodecStatus status =
        GoldSrcFragmentCodecStatus::kTruncated;
    GoldSrcFragmentMetadata metadata;

    bool ok() const noexcept
    {
        return status == GoldSrcFragmentCodecStatus::kOk;
    }
};

GoldSrcFragmentDecodeResult DecodeGoldSrcFragmentMetadata(
    const std::uint8_t* bytes,
    std::size_t size,
    std::size_t maximum_fragment_count =
        kGoldSrcMaximumNormalFragmentCount) noexcept;

GoldSrcFragmentCodecStatus EncodeGoldSrcNormalFragmentMetadata(
    const GoldSrcFragmentDescriptor& descriptor,
    std::uint8_t* bytes,
    std::size_t capacity,
    std::size_t* encoded_size) noexcept;

GoldSrcFragmentCodecStatus ValidateGoldSrcFragmentRanges(
    const GoldSrcFragmentMetadata& metadata,
    std::size_t payload_size) noexcept;

enum class GoldSrcFragmentPlanResult
{
    kNotRequired,
    kPlanned,
    kPayloadEmpty,
    kPayloadTooLarge,
    kFragmentCountExceeded,
    kArithmeticOverflow,
    kTransferBusy,
    kEncodingFailed,
};

std::string_view ReasonFor(GoldSrcFragmentPlanResult result) noexcept;

enum class GoldSrcFragmentTransferPhase
{
    kNone,
    kPlanned,
    kSending,
    kSentAwaitingAck,
    kCompleted,
    kFailed,
};

std::string_view NameFor(GoldSrcFragmentTransferPhase phase) noexcept;

struct GoldSrcFragmentPlanEntry final
{
    std::size_t source_offset = 0u;
    std::size_t payload_length = 0u;
    GoldSrcFragmentDescriptor descriptor;
};

struct GoldSrcFragmentTransferDiagnostics final
{
    std::uint64_t transfers_planned = 0u;
    std::uint64_t fragments_sent = 0u;
    std::uint64_t fragments_resent = 0u;
    std::uint64_t fragments_acknowledged = 0u;
    std::uint64_t transfers_completed = 0u;
    std::uint64_t transfers_expired = 0u;
};

class GoldSrcFragmentSender final
{
public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

    GoldSrcFragmentPlanResult Plan(
        const std::uint8_t* payload,
        std::size_t size,
        std::uint64_t transfer_generation,
        TimePoint now,
        std::size_t fragment_capacity = kGoldSrcMaximumFragmentBytes,
        std::size_t maximum_transfer_size =
            kGoldSrcMaximumFragmentTransferBytes,
        std::size_t maximum_fragment_count =
            kGoldSrcMaximumNormalFragmentCount) noexcept;

    void Reset() noexcept;
    void Fail() noexcept;
    bool Expire(TimePoint now, std::chrono::milliseconds timeout) noexcept;
    bool RecordSent(
        std::uint32_t outer_sequence,
        bool reliable_toggle,
        bool retransmission,
        TimePoint now) noexcept;
    bool AcknowledgeCurrent(TimePoint now) noexcept;

    bool active() const noexcept;
    bool needs_fragment_staging() const noexcept;
    bool awaiting_acknowledgement() const noexcept;
    GoldSrcFragmentTransferPhase phase() const noexcept;
    std::uint64_t transfer_generation() const noexcept;
    std::size_t total_payload_size() const noexcept;
    std::size_t fragment_capacity() const noexcept;
    std::size_t fragment_count() const noexcept;
    std::size_t current_fragment_index() const noexcept;
    const GoldSrcFragmentPlanEntry* fragment_at(
        std::size_t index) const noexcept;
    const GoldSrcFragmentPlanEntry* current_fragment() const noexcept;
    const std::uint8_t* current_fragment_data() const noexcept;
    std::uint32_t first_outer_sequence() const noexcept;
    std::uint32_t latest_outer_sequence() const noexcept;
    bool reliable_toggle() const noexcept;
    std::uint64_t send_count() const noexcept;
    std::uint64_t resend_count() const noexcept;
    bool completion_acknowledged() const noexcept;
    TimePoint created_at() const noexcept;
    TimePoint last_progress_at() const noexcept;
    const GoldSrcFragmentTransferDiagnostics& diagnostics() const noexcept;

private:
    void ClearPayload() noexcept;

    GoldSrcFragmentTransferPhase phase_ =
        GoldSrcFragmentTransferPhase::kNone;
    std::uint64_t transfer_generation_ = 0u;
    std::array<std::uint8_t, kGoldSrcMaximumFragmentTransferBytes>
        frozen_payload_{};
    std::size_t total_payload_size_ = 0u;
    std::size_t fragment_capacity_ = 0u;
    std::array<
        GoldSrcFragmentPlanEntry,
        kGoldSrcMaximumNormalFragmentCount>
        plan_{};
    std::size_t fragment_count_ = 0u;
    std::size_t current_fragment_index_ = 0u;
    std::uint32_t first_outer_sequence_ = 0u;
    std::uint32_t latest_outer_sequence_ = 0u;
    bool reliable_toggle_ = false;
    std::uint64_t send_count_ = 0u;
    std::uint64_t resend_count_ = 0u;
    bool completion_acknowledged_ = false;
    TimePoint created_at_{};
    TimePoint last_progress_at_{};
    GoldSrcFragmentTransferDiagnostics diagnostics_{};
};

enum class GoldSrcFragmentProcessResult
{
    kAccepted,
    kDuplicate,
    kCompleted,
    kTransferMismatch,
    kInconsistentTotalSize,
    kInvalidOffset,
    kInvalidLength,
    kConflictingDuplicate,
    kOverlapConflict,
    kFragmentCountExceeded,
    kPayloadTooLarge,
    kUnsupportedStream,
    kExpired,
    kMalformed,
};

std::string_view ReasonFor(GoldSrcFragmentProcessResult result) noexcept;

class GoldSrcFragmentReassembler final
{
public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

    explicit GoldSrcFragmentReassembler(
        std::size_t maximum_total_size =
            kGoldSrcMaximumFragmentTransferBytes,
        std::size_t maximum_fragment_count =
            kGoldSrcMaximumNormalFragmentCount,
        std::size_t maximum_fragment_size =
            kGoldSrcMaximumFragmentBytes,
        std::chrono::milliseconds timeout =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                kGoldSrcFragmentTransferTimeout)) noexcept;

    GoldSrcFragmentProcessResult Process(
        std::uint64_t transfer_generation,
        const GoldSrcFragmentDescriptor& descriptor,
        const std::uint8_t* packet_payload,
        std::size_t packet_payload_size,
        TimePoint now) noexcept;
    bool Expire(TimePoint now) noexcept;
    void Reset() noexcept;

    bool active() const noexcept;
    bool complete() const noexcept;
    std::uint64_t transfer_generation() const noexcept;
    std::size_t fragment_count() const noexcept;
    std::size_t received_fragment_count() const noexcept;
    const std::uint8_t* payload_data() const noexcept;
    std::size_t payload_size() const noexcept;

private:
    struct StoredFragment final
    {
        bool present = false;
        std::size_t length = 0u;
        std::array<std::uint8_t, kGoldSrcMaximumFragmentBytes> bytes{};
    };

    GoldSrcFragmentProcessResult FailWithoutMutation(
        GoldSrcFragmentProcessResult result) const noexcept;
    bool CompleteAssembly() noexcept;

    std::size_t maximum_total_size_ = 0u;
    std::size_t maximum_fragment_count_ = 0u;
    std::size_t maximum_fragment_size_ = 0u;
    std::chrono::milliseconds timeout_{};
    bool active_ = false;
    bool complete_ = false;
    std::uint64_t transfer_generation_ = 0u;
    std::size_t fragment_count_ = 0u;
    std::size_t received_fragment_count_ = 0u;
    std::size_t received_payload_bytes_ = 0u;
    std::array<StoredFragment, kGoldSrcMaximumNormalFragmentCount> fragments_{};
    std::array<std::uint8_t, kGoldSrcMaximumFragmentTransferBytes> payload_{};
    std::size_t payload_size_ = 0u;
    TimePoint created_at_{};
    TimePoint last_progress_at_{};
};
} // namespace hl::network
