#include "network/goldsrc_fragmentation.h"
#include "network/goldsrc_netchan.h"

#include <algorithm>
#include <cstring>
#include <limits>

namespace hl::network
{
namespace
{
std::uint16_t ReadLittleEndian16(const std::uint8_t* bytes) noexcept
{
    return static_cast<std::uint16_t>(bytes[0])
        | static_cast<std::uint16_t>(
            static_cast<std::uint16_t>(bytes[1]) << 8u);
}

std::uint32_t ReadLittleEndian32(const std::uint8_t* bytes) noexcept
{
    return static_cast<std::uint32_t>(bytes[0])
        | (static_cast<std::uint32_t>(bytes[1]) << 8u)
        | (static_cast<std::uint32_t>(bytes[2]) << 16u)
        | (static_cast<std::uint32_t>(bytes[3]) << 24u);
}

void WriteLittleEndian16(std::uint8_t* bytes, std::uint16_t value) noexcept
{
    bytes[0] = static_cast<std::uint8_t>(value & 0xFFu);
    bytes[1] = static_cast<std::uint8_t>((value >> 8u) & 0xFFu);
}

void WriteLittleEndian32(std::uint8_t* bytes, std::uint32_t value) noexcept
{
    bytes[0] = static_cast<std::uint8_t>(value & 0xFFu);
    bytes[1] = static_cast<std::uint8_t>((value >> 8u) & 0xFFu);
    bytes[2] = static_cast<std::uint8_t>((value >> 16u) & 0xFFu);
    bytes[3] = static_cast<std::uint8_t>((value >> 24u) & 0xFFu);
}

bool CheckedAdd(
    std::size_t left,
    std::size_t right,
    std::size_t* result) noexcept
{
    if (result == nullptr
        || left > std::numeric_limits<std::size_t>::max() - right)
    {
        return false;
    }
    *result = left + right;
    return true;
}

bool IsValidStream(GoldSrcFragmentStream stream) noexcept
{
    return stream == GoldSrcFragmentStream::kNormal
        || stream == GoldSrcFragmentStream::kFile;
}
} // namespace

std::string_view ReasonFor(GoldSrcFragmentCodecStatus status) noexcept
{
    switch (status)
    {
    case GoldSrcFragmentCodecStatus::kOk:
        return "ok";
    case GoldSrcFragmentCodecStatus::kNullInput:
        return "null_input";
    case GoldSrcFragmentCodecStatus::kTruncated:
        return "truncated";
    case GoldSrcFragmentCodecStatus::kInvalidPresence:
        return "invalid_presence";
    case GoldSrcFragmentCodecStatus::kNoFragment:
        return "no_fragment";
    case GoldSrcFragmentCodecStatus::kInvalidFragmentIndex:
        return "invalid_fragment_index";
    case GoldSrcFragmentCodecStatus::kInvalidFragmentCount:
        return "invalid_fragment_count";
    case GoldSrcFragmentCodecStatus::kFragmentCountExceeded:
        return "fragment_count_exceeded";
    case GoldSrcFragmentCodecStatus::kInvalidOffset:
        return "invalid_offset";
    case GoldSrcFragmentCodecStatus::kInvalidLength:
        return "invalid_length";
    case GoldSrcFragmentCodecStatus::kFragmentOutsidePayload:
        return "fragment_outside_payload";
    case GoldSrcFragmentCodecStatus::kOverlappingStreams:
        return "overlapping_streams";
    case GoldSrcFragmentCodecStatus::kUnsupportedStream:
        return "unsupported_stream";
    case GoldSrcFragmentCodecStatus::kPayloadTooLarge:
        return "payload_too_large";
    default:
        return "truncated";
    }
}

GoldSrcFragmentDecodeResult DecodeGoldSrcFragmentMetadata(
    const std::uint8_t* bytes,
    std::size_t size,
    std::size_t maximum_fragment_count) noexcept
{
    GoldSrcFragmentDecodeResult result{};
    if (bytes == nullptr)
    {
        result.status = GoldSrcFragmentCodecStatus::kNullInput;
        return result;
    }
    if (maximum_fragment_count == 0u
        || maximum_fragment_count
            > std::numeric_limits<std::uint16_t>::max())
    {
        result.status = GoldSrcFragmentCodecStatus::kFragmentCountExceeded;
        return result;
    }

    std::size_t cursor = 0u;
    for (std::size_t stream_index = 0u;
         stream_index < kGoldSrcFragmentStreamCount;
         ++stream_index)
    {
        if (cursor >= size)
        {
            result.status = GoldSrcFragmentCodecStatus::kTruncated;
            return result;
        }

        const std::uint8_t presence = bytes[cursor++];
        if (presence > 1u)
        {
            result.status = GoldSrcFragmentCodecStatus::kInvalidPresence;
            return result;
        }
        if (presence == 0u)
        {
            continue;
        }
        if (size - cursor < kGoldSrcFragmentDescriptorBytes - 1u)
        {
            result.status = GoldSrcFragmentCodecStatus::kTruncated;
            return result;
        }

        GoldSrcFragmentDescriptor descriptor{};
        descriptor.stream = static_cast<GoldSrcFragmentStream>(stream_index);
        descriptor.raw_presence = presence;
        descriptor.raw_fragment_id = ReadLittleEndian32(bytes + cursor);
        cursor += 4u;
        descriptor.fragment_index = static_cast<std::uint16_t>(
            (descriptor.raw_fragment_id >> 16u) & 0xFFFFu);
        descriptor.fragment_count = static_cast<std::uint16_t>(
            descriptor.raw_fragment_id & 0xFFFFu);
        descriptor.payload_offset = ReadLittleEndian16(bytes + cursor);
        cursor += 2u;
        descriptor.payload_length = ReadLittleEndian16(bytes + cursor);
        cursor += 2u;

        if (descriptor.fragment_index == 0u)
        {
            result.status =
                GoldSrcFragmentCodecStatus::kInvalidFragmentIndex;
            return result;
        }
        if (descriptor.fragment_count == 0u
            || descriptor.fragment_index > descriptor.fragment_count)
        {
            result.status =
                GoldSrcFragmentCodecStatus::kInvalidFragmentCount;
            return result;
        }
        if (descriptor.fragment_count > maximum_fragment_count)
        {
            result.status =
                GoldSrcFragmentCodecStatus::kFragmentCountExceeded;
            return result;
        }
        if (descriptor.payload_length == 0u
            || descriptor.payload_length > kGoldSrcMaximumFragmentBytes)
        {
            result.status = GoldSrcFragmentCodecStatus::kInvalidLength;
            return result;
        }

        result.metadata.present[stream_index] = true;
        result.metadata.descriptors[stream_index] = descriptor;
        ++result.metadata.present_count;
    }

    result.metadata.encoded_size = cursor;
    if (result.metadata.present_count == 0u)
    {
        result.status = GoldSrcFragmentCodecStatus::kNoFragment;
        return result;
    }
    result.status = GoldSrcFragmentCodecStatus::kOk;
    return result;
}

GoldSrcFragmentCodecStatus EncodeGoldSrcNormalFragmentMetadata(
    const GoldSrcFragmentDescriptor& descriptor,
    std::uint8_t* bytes,
    std::size_t capacity,
    std::size_t* encoded_size) noexcept
{
    if (bytes == nullptr || encoded_size == nullptr)
    {
        return GoldSrcFragmentCodecStatus::kNullInput;
    }
    *encoded_size = 0u;
    if (!IsValidStream(descriptor.stream)
        || descriptor.stream != GoldSrcFragmentStream::kNormal)
    {
        return GoldSrcFragmentCodecStatus::kUnsupportedStream;
    }
    if (descriptor.fragment_index == 0u)
    {
        return GoldSrcFragmentCodecStatus::kInvalidFragmentIndex;
    }
    if (descriptor.fragment_count == 0u
        || descriptor.fragment_index > descriptor.fragment_count)
    {
        return GoldSrcFragmentCodecStatus::kInvalidFragmentCount;
    }
    if (descriptor.fragment_count > kGoldSrcMaximumNormalFragmentCount)
    {
        return GoldSrcFragmentCodecStatus::kFragmentCountExceeded;
    }
    if (descriptor.payload_length == 0u
        || descriptor.payload_length > kGoldSrcMaximumFragmentBytes)
    {
        return GoldSrcFragmentCodecStatus::kInvalidLength;
    }
    if (capacity < kGoldSrcNormalOnlyFragmentMetadataBytes)
    {
        return GoldSrcFragmentCodecStatus::kTruncated;
    }

    std::fill_n(bytes, kGoldSrcNormalOnlyFragmentMetadataBytes, std::uint8_t{0});
    bytes[0] = 1u;
    const std::uint32_t raw_fragment_id =
        (static_cast<std::uint32_t>(descriptor.fragment_index) << 16u)
        | static_cast<std::uint32_t>(descriptor.fragment_count);
    WriteLittleEndian32(bytes + 1u, raw_fragment_id);
    WriteLittleEndian16(bytes + 5u, descriptor.payload_offset);
    WriteLittleEndian16(bytes + 7u, descriptor.payload_length);
    bytes[9] = 0u;
    *encoded_size = kGoldSrcNormalOnlyFragmentMetadataBytes;
    return GoldSrcFragmentCodecStatus::kOk;
}

GoldSrcFragmentCodecStatus ValidateGoldSrcFragmentRanges(
    const GoldSrcFragmentMetadata& metadata,
    std::size_t payload_size) noexcept
{
    if (metadata.present_count == 0u)
    {
        return GoldSrcFragmentCodecStatus::kNoFragment;
    }

    std::array<std::size_t, kGoldSrcFragmentStreamCount> starts{};
    std::array<std::size_t, kGoldSrcFragmentStreamCount> ends{};
    for (std::size_t index = 0u;
         index < kGoldSrcFragmentStreamCount;
         ++index)
    {
        if (!metadata.present[index])
        {
            continue;
        }
        const GoldSrcFragmentDescriptor& descriptor =
            metadata.descriptors[index];
        starts[index] = descriptor.payload_offset;
        if (!CheckedAdd(
                starts[index],
                descriptor.payload_length,
                &ends[index]))
        {
            return GoldSrcFragmentCodecStatus::kInvalidOffset;
        }
        if (ends[index] > payload_size)
        {
            return GoldSrcFragmentCodecStatus::kFragmentOutsidePayload;
        }
    }

    if (metadata.present[0] && metadata.present[1])
    {
        const bool disjoint =
            ends[0] <= starts[1] || ends[1] <= starts[0];
        if (!disjoint)
        {
            return GoldSrcFragmentCodecStatus::kOverlappingStreams;
        }
    }
    return GoldSrcFragmentCodecStatus::kOk;
}

std::string_view ReasonFor(GoldSrcFragmentPlanResult result) noexcept
{
    switch (result)
    {
    case GoldSrcFragmentPlanResult::kNotRequired:
        return "not_required";
    case GoldSrcFragmentPlanResult::kPlanned:
        return "planned";
    case GoldSrcFragmentPlanResult::kPayloadEmpty:
        return "payload_empty";
    case GoldSrcFragmentPlanResult::kPayloadTooLarge:
        return "payload_too_large";
    case GoldSrcFragmentPlanResult::kFragmentCountExceeded:
        return "fragment_count_exceeded";
    case GoldSrcFragmentPlanResult::kArithmeticOverflow:
        return "arithmetic_overflow";
    case GoldSrcFragmentPlanResult::kTransferBusy:
        return "transfer_busy";
    case GoldSrcFragmentPlanResult::kEncodingFailed:
        return "encoding_failed";
    default:
        return "encoding_failed";
    }
}

std::string_view NameFor(GoldSrcFragmentTransferPhase phase) noexcept
{
    switch (phase)
    {
    case GoldSrcFragmentTransferPhase::kPlanned:
        return "planned";
    case GoldSrcFragmentTransferPhase::kSending:
        return "sending";
    case GoldSrcFragmentTransferPhase::kSentAwaitingAck:
        return "sent_awaiting_ack";
    case GoldSrcFragmentTransferPhase::kCompleted:
        return "completed";
    case GoldSrcFragmentTransferPhase::kFailed:
        return "failed";
    case GoldSrcFragmentTransferPhase::kNone:
    default:
        return "none";
    }
}

GoldSrcFragmentPlanResult GoldSrcFragmentSender::Plan(
    const std::uint8_t* payload,
    std::size_t size,
    std::uint64_t transfer_generation,
    TimePoint now,
    std::size_t fragment_capacity,
    std::size_t maximum_transfer_size,
    std::size_t maximum_fragment_count) noexcept
{
    if (active())
    {
        return GoldSrcFragmentPlanResult::kTransferBusy;
    }
    if (payload == nullptr || size == 0u)
    {
        return GoldSrcFragmentPlanResult::kPayloadEmpty;
    }
    if (size <= kGoldSrcNetchanMaximumReliableBytes)
    {
        return GoldSrcFragmentPlanResult::kNotRequired;
    }
    if (fragment_capacity == 0u
        || fragment_capacity > kGoldSrcMaximumFragmentBytes
        || fragment_capacity
            > std::numeric_limits<std::uint16_t>::max())
    {
        return GoldSrcFragmentPlanResult::kEncodingFailed;
    }
    if (maximum_fragment_count == 0u
        || maximum_fragment_count > plan_.size())
    {
        return GoldSrcFragmentPlanResult::kFragmentCountExceeded;
    }
    if (size > std::numeric_limits<std::size_t>::max()
            - (fragment_capacity - 1u))
    {
        return GoldSrcFragmentPlanResult::kArithmeticOverflow;
    }
    if (maximum_transfer_size == 0u
        || maximum_transfer_size > frozen_payload_.size()
        || size > maximum_transfer_size)
    {
        return GoldSrcFragmentPlanResult::kPayloadTooLarge;
    }

    const std::size_t count =
        (size + fragment_capacity - 1u) / fragment_capacity;
    if (count == 0u || count > maximum_fragment_count
        || count > std::numeric_limits<std::uint16_t>::max())
    {
        return GoldSrcFragmentPlanResult::kFragmentCountExceeded;
    }

    const GoldSrcFragmentTransferDiagnostics stable_diagnostics =
        diagnostics_;
    Reset();
    diagnostics_ = stable_diagnostics;
    std::memcpy(frozen_payload_.data(), payload, size);
    transfer_generation_ = transfer_generation;
    total_payload_size_ = size;
    fragment_capacity_ = fragment_capacity;
    fragment_count_ = count;
    created_at_ = now;
    last_progress_at_ = now;
    for (std::size_t index = 0u; index < count; ++index)
    {
        if (index > std::numeric_limits<std::size_t>::max()
                / fragment_capacity)
        {
            Reset();
            return GoldSrcFragmentPlanResult::kArithmeticOverflow;
        }
        const std::size_t offset = index * fragment_capacity;
        if (offset >= size)
        {
            Reset();
            return GoldSrcFragmentPlanResult::kEncodingFailed;
        }
        const std::size_t length =
            std::min(fragment_capacity, size - offset);
        GoldSrcFragmentPlanEntry& entry = plan_[index];
        entry.source_offset = offset;
        entry.payload_length = length;
        entry.descriptor.stream = GoldSrcFragmentStream::kNormal;
        entry.descriptor.raw_presence = 1u;
        entry.descriptor.fragment_index =
            static_cast<std::uint16_t>(index + 1u);
        entry.descriptor.fragment_count =
            static_cast<std::uint16_t>(count);
        entry.descriptor.raw_fragment_id =
            (static_cast<std::uint32_t>(entry.descriptor.fragment_index)
                << 16u)
            | entry.descriptor.fragment_count;
        entry.descriptor.payload_offset = 0u;
        entry.descriptor.payload_length =
            static_cast<std::uint16_t>(length);
    }
    phase_ = GoldSrcFragmentTransferPhase::kPlanned;
    ++diagnostics_.transfers_planned;
    return GoldSrcFragmentPlanResult::kPlanned;
}

void GoldSrcFragmentSender::Reset() noexcept
{
    *this = GoldSrcFragmentSender{};
}

void GoldSrcFragmentSender::ClearPayload() noexcept
{
    std::fill(frozen_payload_.begin(), frozen_payload_.end(), std::uint8_t{0});
}

void GoldSrcFragmentSender::Fail() noexcept
{
    ClearPayload();
    transfer_generation_ = 0u;
    total_payload_size_ = 0u;
    fragment_capacity_ = 0u;
    plan_.fill(GoldSrcFragmentPlanEntry{});
    fragment_count_ = 0u;
    current_fragment_index_ = 0u;
    first_outer_sequence_ = 0u;
    latest_outer_sequence_ = 0u;
    reliable_toggle_ = false;
    send_count_ = 0u;
    resend_count_ = 0u;
    phase_ = GoldSrcFragmentTransferPhase::kFailed;
    completion_acknowledged_ = false;
    created_at_ = {};
    last_progress_at_ = {};
}

bool GoldSrcFragmentSender::Expire(
    TimePoint now,
    std::chrono::milliseconds timeout) noexcept
{
    if (!active() || timeout.count() < 0 || now < last_progress_at_
        || now - last_progress_at_ < timeout)
    {
        return false;
    }
    ++diagnostics_.transfers_expired;
    Fail();
    return true;
}

bool GoldSrcFragmentSender::RecordSent(
    std::uint32_t outer_sequence,
    bool reliable_toggle,
    bool retransmission,
    TimePoint now) noexcept
{
    if (!active() || current_fragment() == nullptr)
    {
        return false;
    }
    if (first_outer_sequence_ == 0u && send_count_ == 0u)
    {
        first_outer_sequence_ = outer_sequence;
    }
    latest_outer_sequence_ = outer_sequence;
    reliable_toggle_ = reliable_toggle;
    ++send_count_;
    if (retransmission)
    {
        ++resend_count_;
        ++diagnostics_.fragments_resent;
    }
    else
    {
        ++diagnostics_.fragments_sent;
    }
    last_progress_at_ = now;
    phase_ = GoldSrcFragmentTransferPhase::kSentAwaitingAck;
    return true;
}

bool GoldSrcFragmentSender::AcknowledgeCurrent(TimePoint now) noexcept
{
    if (phase_ != GoldSrcFragmentTransferPhase::kSentAwaitingAck
        || current_fragment_index_ >= fragment_count_)
    {
        return false;
    }
    ++diagnostics_.fragments_acknowledged;
    ++current_fragment_index_;
    last_progress_at_ = now;
    if (current_fragment_index_ == fragment_count_)
    {
        completion_acknowledged_ = true;
        phase_ = GoldSrcFragmentTransferPhase::kCompleted;
        ++diagnostics_.transfers_completed;
        ClearPayload();
    }
    else
    {
        phase_ = GoldSrcFragmentTransferPhase::kSending;
    }
    return true;
}

bool GoldSrcFragmentSender::active() const noexcept
{
    return phase_ == GoldSrcFragmentTransferPhase::kPlanned
        || phase_ == GoldSrcFragmentTransferPhase::kSending
        || phase_ == GoldSrcFragmentTransferPhase::kSentAwaitingAck;
}

bool GoldSrcFragmentSender::needs_fragment_staging() const noexcept
{
    return phase_ == GoldSrcFragmentTransferPhase::kPlanned
        || phase_ == GoldSrcFragmentTransferPhase::kSending;
}

bool GoldSrcFragmentSender::awaiting_acknowledgement() const noexcept
{
    return phase_ == GoldSrcFragmentTransferPhase::kSentAwaitingAck;
}

GoldSrcFragmentTransferPhase GoldSrcFragmentSender::phase() const noexcept
{
    return phase_;
}

std::uint64_t GoldSrcFragmentSender::transfer_generation() const noexcept
{
    return transfer_generation_;
}

std::size_t GoldSrcFragmentSender::total_payload_size() const noexcept
{
    return total_payload_size_;
}

std::size_t GoldSrcFragmentSender::fragment_capacity() const noexcept
{
    return fragment_capacity_;
}

std::size_t GoldSrcFragmentSender::fragment_count() const noexcept
{
    return fragment_count_;
}

std::size_t GoldSrcFragmentSender::current_fragment_index() const noexcept
{
    return current_fragment_index_;
}

const GoldSrcFragmentPlanEntry* GoldSrcFragmentSender::fragment_at(
    std::size_t index) const noexcept
{
    return index < fragment_count_ ? &plan_[index] : nullptr;
}

const GoldSrcFragmentPlanEntry*
GoldSrcFragmentSender::current_fragment() const noexcept
{
    return fragment_at(current_fragment_index_);
}

const std::uint8_t* GoldSrcFragmentSender::current_fragment_data() const noexcept
{
    const GoldSrcFragmentPlanEntry* entry = current_fragment();
    return entry == nullptr
        ? nullptr
        : frozen_payload_.data() + entry->source_offset;
}

std::uint32_t GoldSrcFragmentSender::first_outer_sequence() const noexcept
{
    return first_outer_sequence_;
}

std::uint32_t GoldSrcFragmentSender::latest_outer_sequence() const noexcept
{
    return latest_outer_sequence_;
}

bool GoldSrcFragmentSender::reliable_toggle() const noexcept
{
    return reliable_toggle_;
}

std::uint64_t GoldSrcFragmentSender::send_count() const noexcept
{
    return send_count_;
}

std::uint64_t GoldSrcFragmentSender::resend_count() const noexcept
{
    return resend_count_;
}

bool GoldSrcFragmentSender::completion_acknowledged() const noexcept
{
    return completion_acknowledged_;
}

GoldSrcFragmentSender::TimePoint GoldSrcFragmentSender::created_at() const noexcept
{
    return created_at_;
}

GoldSrcFragmentSender::TimePoint
GoldSrcFragmentSender::last_progress_at() const noexcept
{
    return last_progress_at_;
}

const GoldSrcFragmentTransferDiagnostics&
GoldSrcFragmentSender::diagnostics() const noexcept
{
    return diagnostics_;
}

std::string_view ReasonFor(GoldSrcFragmentProcessResult result) noexcept
{
    switch (result)
    {
    case GoldSrcFragmentProcessResult::kAccepted:
        return "accepted";
    case GoldSrcFragmentProcessResult::kDuplicate:
        return "duplicate";
    case GoldSrcFragmentProcessResult::kCompleted:
        return "completed";
    case GoldSrcFragmentProcessResult::kTransferMismatch:
        return "transfer_mismatch";
    case GoldSrcFragmentProcessResult::kInconsistentTotalSize:
        return "inconsistent_total_size";
    case GoldSrcFragmentProcessResult::kInvalidOffset:
        return "invalid_offset";
    case GoldSrcFragmentProcessResult::kInvalidLength:
        return "invalid_length";
    case GoldSrcFragmentProcessResult::kConflictingDuplicate:
        return "conflicting_duplicate";
    case GoldSrcFragmentProcessResult::kOverlapConflict:
        return "overlap_conflict";
    case GoldSrcFragmentProcessResult::kFragmentCountExceeded:
        return "fragment_count_exceeded";
    case GoldSrcFragmentProcessResult::kPayloadTooLarge:
        return "payload_too_large";
    case GoldSrcFragmentProcessResult::kUnsupportedStream:
        return "unsupported_stream";
    case GoldSrcFragmentProcessResult::kExpired:
        return "expired";
    case GoldSrcFragmentProcessResult::kMalformed:
    default:
        return "malformed";
    }
}

GoldSrcFragmentReassembler::GoldSrcFragmentReassembler(
    std::size_t maximum_total_size,
    std::size_t maximum_fragment_count,
    std::size_t maximum_fragment_size,
    std::chrono::milliseconds timeout) noexcept
    : maximum_total_size_(std::min(maximum_total_size, payload_.size())),
      maximum_fragment_count_(
          std::min(maximum_fragment_count, fragments_.size())),
      maximum_fragment_size_(
          std::min(maximum_fragment_size, fragments_[0].bytes.size())),
      timeout_(timeout)
{
}

GoldSrcFragmentProcessResult GoldSrcFragmentReassembler::FailWithoutMutation(
    GoldSrcFragmentProcessResult result) const noexcept
{
    return result;
}

GoldSrcFragmentProcessResult GoldSrcFragmentReassembler::Process(
    std::uint64_t transfer_generation,
    const GoldSrcFragmentDescriptor& descriptor,
    const std::uint8_t* packet_payload,
    std::size_t packet_payload_size,
    TimePoint now) noexcept
{
    if (Expire(now))
    {
        return GoldSrcFragmentProcessResult::kExpired;
    }
    if (descriptor.stream != GoldSrcFragmentStream::kNormal)
    {
        return FailWithoutMutation(
            GoldSrcFragmentProcessResult::kUnsupportedStream);
    }
    if (packet_payload == nullptr)
    {
        return FailWithoutMutation(GoldSrcFragmentProcessResult::kMalformed);
    }
    if (descriptor.fragment_count == 0u
        || descriptor.fragment_count > maximum_fragment_count_)
    {
        return FailWithoutMutation(
            GoldSrcFragmentProcessResult::kFragmentCountExceeded);
    }
    if (descriptor.fragment_index == 0u
        || descriptor.fragment_index > descriptor.fragment_count)
    {
        return FailWithoutMutation(GoldSrcFragmentProcessResult::kMalformed);
    }
    if (descriptor.payload_length == 0u
        || descriptor.payload_length > maximum_fragment_size_)
    {
        return FailWithoutMutation(
            GoldSrcFragmentProcessResult::kInvalidLength);
    }
    std::size_t payload_end = 0u;
    if (!CheckedAdd(
            descriptor.payload_offset,
            descriptor.payload_length,
            &payload_end))
    {
        return FailWithoutMutation(
            GoldSrcFragmentProcessResult::kInvalidOffset);
    }
    if (payload_end > packet_payload_size)
    {
        return FailWithoutMutation(
            GoldSrcFragmentProcessResult::kInvalidOffset);
    }

    if (!active_ && !complete_)
    {
        active_ = true;
        transfer_generation_ = transfer_generation;
        fragment_count_ = descriptor.fragment_count;
        created_at_ = now;
        last_progress_at_ = now;
    }
    else
    {
        if (transfer_generation != transfer_generation_)
        {
            return FailWithoutMutation(
                GoldSrcFragmentProcessResult::kTransferMismatch);
        }
        if (descriptor.fragment_count != fragment_count_)
        {
            return FailWithoutMutation(
                GoldSrcFragmentProcessResult::kInconsistentTotalSize);
        }
    }

    StoredFragment& stored = fragments_[descriptor.fragment_index - 1u];
    const std::uint8_t* fragment_bytes =
        packet_payload + descriptor.payload_offset;
    if (stored.present)
    {
        if (stored.length != descriptor.payload_length)
        {
            return FailWithoutMutation(
                GoldSrcFragmentProcessResult::kOverlapConflict);
        }
        if (!std::equal(
                stored.bytes.begin(),
                stored.bytes.begin()
                    + static_cast<std::ptrdiff_t>(stored.length),
                fragment_bytes))
        {
            return FailWithoutMutation(
                GoldSrcFragmentProcessResult::kConflictingDuplicate);
        }
        return GoldSrcFragmentProcessResult::kDuplicate;
    }
    if (complete_)
    {
        return FailWithoutMutation(GoldSrcFragmentProcessResult::kMalformed);
    }

    std::size_t new_total = 0u;
    if (!CheckedAdd(
            received_payload_bytes_,
            descriptor.payload_length,
            &new_total)
        || new_total > maximum_total_size_)
    {
        return FailWithoutMutation(
            GoldSrcFragmentProcessResult::kPayloadTooLarge);
    }

    std::copy_n(
        fragment_bytes,
        descriptor.payload_length,
        stored.bytes.begin());
    stored.length = descriptor.payload_length;
    stored.present = true;
    received_payload_bytes_ = new_total;
    ++received_fragment_count_;
    last_progress_at_ = now;

    if (received_fragment_count_ != fragment_count_)
    {
        return GoldSrcFragmentProcessResult::kAccepted;
    }
    if (!CompleteAssembly())
    {
        return GoldSrcFragmentProcessResult::kMalformed;
    }
    active_ = false;
    complete_ = true;
    return GoldSrcFragmentProcessResult::kCompleted;
}

bool GoldSrcFragmentReassembler::CompleteAssembly() noexcept
{
    std::size_t cursor = 0u;
    for (std::size_t index = 0u; index < fragment_count_; ++index)
    {
        const StoredFragment& stored = fragments_[index];
        if (!stored.present)
        {
            return false;
        }
        std::size_t next = 0u;
        if (!CheckedAdd(cursor, stored.length, &next)
            || next > maximum_total_size_)
        {
            return false;
        }
        std::copy_n(
            stored.bytes.begin(),
            stored.length,
            payload_.begin() + static_cast<std::ptrdiff_t>(cursor));
        cursor = next;
    }
    payload_size_ = cursor;
    return payload_size_ == received_payload_bytes_;
}

bool GoldSrcFragmentReassembler::Expire(TimePoint now) noexcept
{
    if (!active_ || timeout_.count() < 0 || now < last_progress_at_
        || now - last_progress_at_ < timeout_)
    {
        return false;
    }
    Reset();
    return true;
}

void GoldSrcFragmentReassembler::Reset() noexcept
{
    const std::size_t maximum_total_size = maximum_total_size_;
    const std::size_t maximum_fragment_count = maximum_fragment_count_;
    const std::size_t maximum_fragment_size = maximum_fragment_size_;
    const std::chrono::milliseconds timeout = timeout_;
    *this = GoldSrcFragmentReassembler(
        maximum_total_size,
        maximum_fragment_count,
        maximum_fragment_size,
        timeout);
}

bool GoldSrcFragmentReassembler::active() const noexcept
{
    return active_;
}

bool GoldSrcFragmentReassembler::complete() const noexcept
{
    return complete_;
}

std::uint64_t GoldSrcFragmentReassembler::transfer_generation() const noexcept
{
    return transfer_generation_;
}

std::size_t GoldSrcFragmentReassembler::fragment_count() const noexcept
{
    return fragment_count_;
}

std::size_t
GoldSrcFragmentReassembler::received_fragment_count() const noexcept
{
    return received_fragment_count_;
}

const std::uint8_t* GoldSrcFragmentReassembler::payload_data() const noexcept
{
    return payload_.data();
}

std::size_t GoldSrcFragmentReassembler::payload_size() const noexcept
{
    return payload_size_;
}
} // namespace hl::network
