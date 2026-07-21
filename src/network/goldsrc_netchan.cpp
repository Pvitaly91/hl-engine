#include "network/goldsrc_netchan.h"

#include <algorithm>
#include <cstring>

namespace hl::network
{
namespace
{
constexpr std::uint32_t kSequenceHalfRange = 0x20000000u;
constexpr std::array<std::uint8_t, 16> kMungeTable2 = {
    0x05u, 0x61u, 0x7Au, 0xEDu,
    0x1Bu, 0xCAu, 0x0Du, 0x9Bu,
    0x4Au, 0xF1u, 0x64u, 0xC7u,
    0xB5u, 0x8Eu, 0xDFu, 0xA0u,
};

std::uint32_t ReadLittleEndian32(const std::uint8_t* bytes) noexcept
{
    return static_cast<std::uint32_t>(bytes[0])
        | (static_cast<std::uint32_t>(bytes[1]) << 8u)
        | (static_cast<std::uint32_t>(bytes[2]) << 16u)
        | (static_cast<std::uint32_t>(bytes[3]) << 24u);
}

void WriteLittleEndian32(std::uint8_t* bytes, std::uint32_t value) noexcept
{
    bytes[0] = static_cast<std::uint8_t>(value & 0xFFu);
    bytes[1] = static_cast<std::uint8_t>((value >> 8u) & 0xFFu);
    bytes[2] = static_cast<std::uint8_t>((value >> 16u) & 0xFFu);
    bytes[3] = static_cast<std::uint8_t>((value >> 24u) & 0xFFu);
}

std::uint32_t ByteSwap32(std::uint32_t value) noexcept
{
    return ((value & 0x000000FFu) << 24u)
        | ((value & 0x0000FF00u) << 8u)
        | ((value & 0x00FF0000u) >> 8u)
        | ((value & 0xFF000000u) >> 24u);
}

void TransformPayload(
    std::uint8_t* bytes,
    std::size_t size,
    std::uint32_t sequence,
    bool reverse) noexcept
{
    if (bytes == nullptr)
    {
        return;
    }

    const std::uint32_t key = sequence & 0xFFu;
    const std::size_t block_count = size / 4u;
    for (std::size_t block_index = 0; block_index < block_count; ++block_index)
    {
        std::uint8_t* block = bytes + (block_index * 4u);
        std::uint32_t value = ReadLittleEndian32(block) ^ key;
        if (!reverse)
        {
            value = ByteSwap32(value);
        }

        std::array<std::uint8_t, 4> transformed{};
        WriteLittleEndian32(transformed.data(), value);
        for (std::size_t byte_index = 0; byte_index < transformed.size(); ++byte_index)
        {
            const std::uint32_t index = static_cast<std::uint32_t>(byte_index);
            const std::uint8_t mask = static_cast<std::uint8_t>(
                0xA5u
                | (index << index)
                | index
                | kMungeTable2[(block_index + byte_index) & 0x0Fu]);
            transformed[byte_index] ^= mask;
        }

        value = ReadLittleEndian32(transformed.data());
        if (reverse)
        {
            value = ByteSwap32(value);
        }
        value ^= ~key;
        WriteLittleEndian32(block, value);
    }
}
} // namespace

std::string_view ReasonFor(GoldSrcNetchanCodecStatus status) noexcept
{
    switch (status)
    {
    case GoldSrcNetchanCodecStatus::kOk:
        return "ok";
    case GoldSrcNetchanCodecStatus::kMalformedHeader:
        return "malformed_header";
    case GoldSrcNetchanCodecStatus::kOversized:
        return "oversized_packet";
    case GoldSrcNetchanCodecStatus::kUnsupportedReservedFlags:
        return "unsupported_reserved_flags";
    case GoldSrcNetchanCodecStatus::kUnsupportedFragment:
        return "unsupported_fragment";
    case GoldSrcNetchanCodecStatus::kPayloadTooLarge:
        return "payload_too_large";
    default:
        return "malformed_header";
    }
}

GoldSrcNetchanDecodeResult DecodeGoldSrcNetchanDatagram(
    GoldSrcNetchanDirection direction,
    const std::uint8_t* bytes,
    std::size_t size) noexcept
{
    (void)direction;
    GoldSrcNetchanDecodeResult result;
    if (bytes == nullptr || size < kGoldSrcNetchanBaseHeaderBytes)
    {
        result.status = GoldSrcNetchanCodecStatus::kMalformedHeader;
        return result;
    }
    if (size > kGoldSrcNetchanMaximumRouteableBytes)
    {
        result.status = GoldSrcNetchanCodecStatus::kOversized;
        return result;
    }

    result.packet.raw_sequence = ReadLittleEndian32(bytes);
    result.packet.raw_acknowledgement = ReadLittleEndian32(bytes + 4u);
    result.packet.sequence =
        result.packet.raw_sequence & kGoldSrcNetchanSequenceMask;
    result.packet.acknowledgement =
        result.packet.raw_acknowledgement & kGoldSrcNetchanSequenceMask;
    result.packet.reliable_present =
        (result.packet.raw_sequence & kGoldSrcNetchanReliableFlag) != 0u;
    result.packet.reliable_acknowledgement =
        (result.packet.raw_acknowledgement & kGoldSrcNetchanReliableFlag) != 0u;
    result.packet.fragment_present =
        (result.packet.raw_sequence & kGoldSrcNetchanFragmentFlag) != 0u;

    if (result.packet.fragment_present)
    {
        result.status = GoldSrcNetchanCodecStatus::kUnsupportedFragment;
        return result;
    }
    if ((result.packet.raw_acknowledgement & kGoldSrcNetchanFragmentFlag) != 0u)
    {
        result.status = GoldSrcNetchanCodecStatus::kUnsupportedReservedFlags;
        return result;
    }

    result.packet.payload_size = size - kGoldSrcNetchanBaseHeaderBytes;
    if (result.packet.payload_size > 0u)
    {
        std::memcpy(
            result.packet.payload.data(),
            bytes + kGoldSrcNetchanBaseHeaderBytes,
            result.packet.payload_size);
        UnmungeGoldSrcNetchanPayload(
            result.packet.payload.data(),
            result.packet.payload_size,
            result.packet.sequence);
    }
    result.status = GoldSrcNetchanCodecStatus::kOk;
    return result;
}

GoldSrcNetchanCodecStatus EncodeGoldSrcNetchanDatagram(
    GoldSrcNetchanDirection direction,
    std::uint32_t sequence,
    std::uint32_t acknowledgement,
    bool reliable_present,
    bool reliable_acknowledgement,
    const std::uint8_t* payload,
    std::size_t payload_size,
    bool pad_to_minimum,
    GoldSrcNetchanDatagram* datagram) noexcept
{
    if (datagram == nullptr || (payload == nullptr && payload_size != 0u))
    {
        return GoldSrcNetchanCodecStatus::kMalformedHeader;
    }
    if (payload_size > kGoldSrcNetchanMaximumPayloadBytes)
    {
        return GoldSrcNetchanCodecStatus::kPayloadTooLarge;
    }

    const std::size_t unpadded_size =
        kGoldSrcNetchanBaseHeaderBytes + payload_size;
    const std::size_t encoded_size = pad_to_minimum
        ? std::max(unpadded_size, kGoldSrcNetchanMinimumDatagramBytes)
        : unpadded_size;
    if (encoded_size > kGoldSrcNetchanMaximumRouteableBytes)
    {
        return GoldSrcNetchanCodecStatus::kOversized;
    }

    *datagram = {};
    const std::uint32_t masked_sequence = sequence & kGoldSrcNetchanSequenceMask;
    const std::uint32_t masked_acknowledgement =
        acknowledgement & kGoldSrcNetchanSequenceMask;
    const std::uint32_t raw_sequence = masked_sequence
        | (reliable_present ? kGoldSrcNetchanReliableFlag : 0u);
    const std::uint32_t raw_acknowledgement = masked_acknowledgement
        | (reliable_acknowledgement ? kGoldSrcNetchanReliableFlag : 0u);
    WriteLittleEndian32(datagram->bytes.data(), raw_sequence);
    WriteLittleEndian32(datagram->bytes.data() + 4u, raw_acknowledgement);
    if (payload_size > 0u)
    {
        std::memcpy(
            datagram->bytes.data() + kGoldSrcNetchanBaseHeaderBytes,
            payload,
            payload_size);
    }

    const std::uint8_t padding_opcode =
        direction == GoldSrcNetchanDirection::kServerToClient
        ? kGoldSrcServerNop
        : kGoldSrcClientNop;
    std::fill(
        datagram->bytes.begin() + static_cast<std::ptrdiff_t>(unpadded_size),
        datagram->bytes.begin() + static_cast<std::ptrdiff_t>(encoded_size),
        padding_opcode);
    datagram->size = encoded_size;
    MungeGoldSrcNetchanPayload(
        datagram->bytes.data() + kGoldSrcNetchanBaseHeaderBytes,
        datagram->size - kGoldSrcNetchanBaseHeaderBytes,
        masked_sequence);
    return GoldSrcNetchanCodecStatus::kOk;
}

void MungeGoldSrcNetchanPayload(
    std::uint8_t* bytes,
    std::size_t size,
    std::uint32_t sequence) noexcept
{
    TransformPayload(bytes, size, sequence, false);
}

void UnmungeGoldSrcNetchanPayload(
    std::uint8_t* bytes,
    std::size_t size,
    std::uint32_t sequence) noexcept
{
    TransformPayload(bytes, size, sequence, true);
}

bool IsGoldSrcNetchanSequenceNewer(
    std::uint32_t candidate,
    std::uint32_t baseline) noexcept
{
    const std::uint32_t distance =
        GoldSrcNetchanSequenceDistance(candidate, baseline);
    return distance != 0u && distance < kSequenceHalfRange;
}

std::uint32_t GoldSrcNetchanSequenceDistance(
    std::uint32_t newer,
    std::uint32_t older) noexcept
{
    return (newer - older) & kGoldSrcNetchanSequenceMask;
}

std::uint32_t NextGoldSrcNetchanSequence(std::uint32_t sequence) noexcept
{
    return (sequence + 1u) & kGoldSrcNetchanSequenceMask;
}

bool GoldSrcNetchanSentWindowContains(
    std::uint32_t sequence,
    std::uint32_t highest_sequence_sent,
    std::uint64_t sent_sequence_count) noexcept
{
    if (sent_sequence_count == 0u)
    {
        return false;
    }

    const std::uint32_t masked_sequence =
        sequence & kGoldSrcNetchanSequenceMask;
    const std::uint32_t masked_highest =
        highest_sequence_sent & kGoldSrcNetchanSequenceMask;
    if (masked_sequence == masked_highest)
    {
        return true;
    }
    if (!IsGoldSrcNetchanSequenceNewer(masked_highest, masked_sequence))
    {
        return false;
    }

    const std::uint32_t backward_distance =
        GoldSrcNetchanSequenceDistance(masked_highest, masked_sequence);
    return static_cast<std::uint64_t>(backward_distance) < sent_sequence_count;
}

std::string_view NameFor(GoldSrcNetchanTransportPhase phase) noexcept
{
    switch (phase)
    {
    case GoldSrcNetchanTransportPhase::kAwaitingFirstReliableAck:
        return "awaiting_first_reliable_ack";
    case GoldSrcNetchanTransportPhase::kEstablished:
        return "established";
    case GoldSrcNetchanTransportPhase::kNone:
    default:
        return "none";
    }
}

std::string_view NameFor(GoldSrcNetchanReliablePayloadKind kind) noexcept
{
    switch (kind)
    {
    case GoldSrcNetchanReliablePayloadKind::kTransportBootstrap:
        return "transport_bootstrap";
    case GoldSrcNetchanReliablePayloadKind::kServerInfo:
        return "server_info";
    case GoldSrcNetchanReliablePayloadKind::kNone:
    default:
        return "none";
    }
}

std::string_view ReasonFor(GoldSrcNetchanQueueResult result) noexcept
{
    switch (result)
    {
    case GoldSrcNetchanQueueResult::kQueued:
        return "queued";
    case GoldSrcNetchanQueueResult::kNotInitialized:
        return "not_initialized";
    case GoldSrcNetchanQueueResult::kEmptyPayload:
        return "empty_payload";
    case GoldSrcNetchanQueueResult::kPayloadTooLarge:
        return "payload_too_large";
    case GoldSrcNetchanQueueResult::kReliableAlreadyPending:
        return "reliable_already_pending";
    case GoldSrcNetchanQueueResult::kInvalidPayloadKind:
        return "invalid_payload_kind";
    default:
        return "not_initialized";
    }
}

std::string_view ReasonFor(GoldSrcNetchanProcessResult result) noexcept
{
    switch (result)
    {
    case GoldSrcNetchanProcessResult::kAccepted:
        return "accepted";
    case GoldSrcNetchanProcessResult::kDuplicateSequence:
        return "duplicate_sequence";
    case GoldSrcNetchanProcessResult::kOutOfOrderSequence:
        return "out_of_order_sequence";
    case GoldSrcNetchanProcessResult::kMalformedHeader:
        return "malformed_header";
    case GoldSrcNetchanProcessResult::kUnsupportedFragment:
        return "unsupported_fragment";
    case GoldSrcNetchanProcessResult::kEndpointMismatch:
        return "endpoint_mismatch";
    case GoldSrcNetchanProcessResult::kQportMismatch:
        return "qport_mismatch";
    case GoldSrcNetchanProcessResult::kStaleAck:
        return "stale_ack";
    case GoldSrcNetchanProcessResult::kFutureAck:
        return "future_ack";
    case GoldSrcNetchanProcessResult::kPayloadDecodeFailed:
        return "payload_decode_failed";
    case GoldSrcNetchanProcessResult::kUnsupportedPayload:
        return "unsupported_payload";
    case GoldSrcNetchanProcessResult::kUnsupportedReservedFlags:
        return "unsupported_reserved_flags";
    default:
        return "malformed_header";
    }
}

void GoldSrcNetchanState::Reset() noexcept
{
    *this = GoldSrcNetchanState{};
}

bool GoldSrcNetchanState::Initialize(
    const Ipv4Endpoint& endpoint,
    std::optional<std::uint16_t> connect_qport,
    std::uint16_t channel_identifier,
    TimePoint now) noexcept
{
    Reset();
    initialized_ = true;
    remote_endpoint_ = endpoint;
    connect_qport_ = connect_qport;
    channel_identifier_ = channel_identifier;
    outgoing_sequence_ = 1u;
    incoming_sequence_ = 0u;
    highest_accepted_acknowledgement_ = 0u;
    initialized_at_ = now;
    return true;
}

GoldSrcNetchanQueueResult GoldSrcNetchanState::QueueReliablePayload(
    const std::uint8_t* bytes,
    std::size_t size) noexcept
{
    return QueueReliablePayload(
        bytes,
        size,
        GoldSrcNetchanReliablePayloadKind::kTransportBootstrap);
}

GoldSrcNetchanQueueResult GoldSrcNetchanState::QueueReliablePayload(
    const std::uint8_t* bytes,
    std::size_t size,
    GoldSrcNetchanReliablePayloadKind kind) noexcept
{
    if (!initialized_)
    {
        return GoldSrcNetchanQueueResult::kNotInitialized;
    }
    if (bytes == nullptr || size == 0u)
    {
        return GoldSrcNetchanQueueResult::kEmptyPayload;
    }
    if (size > pending_reliable_.size())
    {
        return GoldSrcNetchanQueueResult::kPayloadTooLarge;
    }
    if (pending_reliable_size_ != 0u)
    {
        return GoldSrcNetchanQueueResult::kReliableAlreadyPending;
    }
    if (kind == GoldSrcNetchanReliablePayloadKind::kNone)
    {
        return GoldSrcNetchanQueueResult::kInvalidPayloadKind;
    }

    std::memcpy(pending_reliable_.data(), bytes, size);
    pending_reliable_size_ = size;
    pending_reliable_kind_ = kind;
    pending_has_been_sent_ = false;
    local_reliable_sequence_ = !local_reliable_sequence_;
    if (kind == GoldSrcNetchanReliablePayloadKind::kTransportBootstrap)
    {
        transport_phase_ =
            GoldSrcNetchanTransportPhase::kAwaitingFirstReliableAck;
    }
    ++diagnostics_.reliable_queued;
    return GoldSrcNetchanQueueResult::kQueued;
}

bool GoldSrcNetchanState::BuildOutgoingDatagram(
    GoldSrcNetchanDatagram* datagram) noexcept
{
    if (!initialized_ || datagram == nullptr)
    {
        return false;
    }

    const bool resend_reliable = pending_reliable_size_ != 0u
        && pending_has_been_sent_
        && AcknowledgementIdentifiesSentSequence(
            highest_accepted_acknowledgement_)
        && IsGoldSrcNetchanSequenceNewer(
            highest_accepted_acknowledgement_,
            last_reliable_sequence_)
        && incoming_reliable_acknowledgement_ != local_reliable_sequence_;
    const bool send_reliable = pending_reliable_size_ != 0u
        && (!pending_has_been_sent_ || resend_reliable);
    const std::uint8_t* payload = send_reliable
        ? pending_reliable_.data()
        : nullptr;
    const std::size_t payload_size = send_reliable
        ? pending_reliable_size_
        : 0u;
    const std::uint32_t sequence = outgoing_sequence_;
    const GoldSrcNetchanCodecStatus status = EncodeGoldSrcNetchanDatagram(
        GoldSrcNetchanDirection::kServerToClient,
        sequence,
        incoming_sequence_,
        send_reliable,
        incoming_reliable_sequence_,
        payload,
        payload_size,
        true,
        datagram);
    if (status != GoldSrcNetchanCodecStatus::kOk)
    {
        return false;
    }

    highest_sequence_sent_ = sequence;
    ++sent_sequence_count_;
    outgoing_sequence_ = NextGoldSrcNetchanSequence(outgoing_sequence_);
    ++diagnostics_.sequenced_sent;

    if (send_reliable)
    {
        if (pending_has_been_sent_)
        {
            ++diagnostics_.reliable_resent;
        }
        else
        {
            ++diagnostics_.reliable_sent;
        }
        pending_has_been_sent_ = true;
        last_reliable_sequence_ = sequence;
    }
    return true;
}

GoldSrcNetchanProcessResult GoldSrcNetchanState::ProcessIncomingDatagram(
    const Ipv4Endpoint& sender,
    const GoldSrcNetchanPacket& packet,
    TimePoint now) noexcept
{
    return ProcessIncomingDatagramDetailed(sender, packet, now).result;
}

GoldSrcNetchanProcessOutcome
GoldSrcNetchanState::ProcessIncomingDatagramDetailed(
    const Ipv4Endpoint& sender,
    const GoldSrcNetchanPacket& packet,
    TimePoint now) noexcept
{
    GoldSrcNetchanProcessOutcome outcome{};
    const auto reject = [this, &outcome](GoldSrcNetchanProcessResult result)
    {
        RecordRejected(result);
        outcome.result = result;
        return outcome;
    };

    ++diagnostics_.sequenced_received;
    if (!initialized_ || sender != remote_endpoint_)
    {
        return reject(GoldSrcNetchanProcessResult::kEndpointMismatch);
    }
    if (packet.payload_size > packet.payload.size())
    {
        return reject(GoldSrcNetchanProcessResult::kPayloadDecodeFailed);
    }
    if (!PayloadIsSupported(packet))
    {
        return reject(GoldSrcNetchanProcessResult::kUnsupportedPayload);
    }

    if (packet.sequence == incoming_sequence_)
    {
        return reject(GoldSrcNetchanProcessResult::kDuplicateSequence);
    }
    if (!IsGoldSrcNetchanSequenceNewer(packet.sequence, incoming_sequence_))
    {
        return reject(GoldSrcNetchanProcessResult::kOutOfOrderSequence);
    }

    if (!AcknowledgementWasSent(packet.acknowledgement))
    {
        const GoldSrcNetchanProcessResult result = sent_sequence_count_ == 0u
            || IsGoldSrcNetchanSequenceNewer(
                packet.acknowledgement,
                highest_sequence_sent_)
            ? GoldSrcNetchanProcessResult::kFutureAck
            : GoldSrcNetchanProcessResult::kStaleAck;
        return reject(result);
    }
    if (packet.acknowledgement != highest_accepted_acknowledgement_
        && IsGoldSrcNetchanSequenceNewer(
            highest_accepted_acknowledgement_,
            packet.acknowledgement))
    {
        return reject(GoldSrcNetchanProcessResult::kStaleAck);
    }

    const std::uint32_t sequence_distance =
        GoldSrcNetchanSequenceDistance(packet.sequence, incoming_sequence_);
    if (sequence_distance > 1u)
    {
        ++diagnostics_.sequence_gaps;
        diagnostics_.dropped_incoming_packets += sequence_distance - 1u;
    }

    incoming_sequence_ = packet.sequence;
    highest_accepted_acknowledgement_ = packet.acknowledgement;
    incoming_reliable_acknowledgement_ = packet.reliable_acknowledgement;
    if (packet.reliable_present)
    {
        incoming_reliable_sequence_ = !incoming_reliable_sequence_;
    }

    const bool acknowledgement_covers_reliable = pending_has_been_sent_
        && AcknowledgementIdentifiesSentSequence(packet.acknowledgement)
        && (packet.acknowledgement == last_reliable_sequence_
            || IsGoldSrcNetchanSequenceNewer(
                packet.acknowledgement,
                last_reliable_sequence_));
    if (pending_reliable_size_ != 0u && acknowledgement_covers_reliable)
    {
        if (packet.reliable_acknowledgement == local_reliable_sequence_)
        {
            const GoldSrcNetchanReliablePayloadKind acknowledged_kind =
                pending_reliable_kind_;
            std::fill(
                pending_reliable_.begin(),
                pending_reliable_.end(),
                std::uint8_t{0});
            pending_reliable_size_ = 0u;
            pending_reliable_kind_ =
                GoldSrcNetchanReliablePayloadKind::kNone;
            pending_has_been_sent_ = false;
            ++diagnostics_.reliable_acked;
            last_acknowledged_reliable_kind_ = acknowledged_kind;
            ++reliable_acknowledgement_generation_;
            outcome.acknowledged_reliable_kind = acknowledged_kind;
            outcome.reliable_acknowledgement_generation =
                reliable_acknowledgement_generation_;
            if (acknowledged_kind
                    == GoldSrcNetchanReliablePayloadKind::kTransportBootstrap
                && transport_phase_
                    == GoldSrcNetchanTransportPhase::kAwaitingFirstReliableAck)
            {
                transport_phase_ = GoldSrcNetchanTransportPhase::kEstablished;
            }
        }
        else
        {
            ++diagnostics_.reliable_ack_mismatch;
        }
    }

    has_accepted_activity_ = true;
    last_accepted_at_ = now;
    ++diagnostics_.accepted;
    outcome.result = GoldSrcNetchanProcessResult::kAccepted;
    return outcome;
}

void GoldSrcNetchanState::RecordRejected(
    GoldSrcNetchanProcessResult result) noexcept
{
    switch (result)
    {
    case GoldSrcNetchanProcessResult::kDuplicateSequence:
        ++diagnostics_.duplicate_sequence;
        break;
    case GoldSrcNetchanProcessResult::kOutOfOrderSequence:
        ++diagnostics_.out_of_order_sequence;
        break;
    case GoldSrcNetchanProcessResult::kMalformedHeader:
        ++diagnostics_.malformed_header;
        break;
    case GoldSrcNetchanProcessResult::kUnsupportedFragment:
        ++diagnostics_.unsupported_fragment;
        break;
    case GoldSrcNetchanProcessResult::kEndpointMismatch:
        ++diagnostics_.endpoint_mismatch;
        break;
    case GoldSrcNetchanProcessResult::kQportMismatch:
        ++diagnostics_.qport_mismatch;
        break;
    case GoldSrcNetchanProcessResult::kStaleAck:
        ++diagnostics_.stale_ack;
        break;
    case GoldSrcNetchanProcessResult::kFutureAck:
        ++diagnostics_.future_ack;
        break;
    case GoldSrcNetchanProcessResult::kPayloadDecodeFailed:
        ++diagnostics_.payload_decode_failed;
        break;
    case GoldSrcNetchanProcessResult::kUnsupportedPayload:
        ++diagnostics_.unsupported_payload;
        break;
    case GoldSrcNetchanProcessResult::kUnsupportedReservedFlags:
        ++diagnostics_.unsupported_reserved_flags;
        break;
    case GoldSrcNetchanProcessResult::kAccepted:
    default:
        break;
    }
}

void GoldSrcNetchanState::SetIncomingPayloadPolicy(
    GoldSrcNetchanIncomingPayloadPolicy policy) noexcept
{
    incoming_payload_policy_ = policy;
}

bool GoldSrcNetchanState::initialized() const noexcept
{
    return initialized_;
}

const Ipv4Endpoint& GoldSrcNetchanState::remote_endpoint() const noexcept
{
    return remote_endpoint_;
}

const std::optional<std::uint16_t>&
GoldSrcNetchanState::connect_qport() const noexcept
{
    return connect_qport_;
}

std::uint16_t GoldSrcNetchanState::channel_identifier() const noexcept
{
    return channel_identifier_;
}

std::uint32_t GoldSrcNetchanState::outgoing_sequence() const noexcept
{
    return outgoing_sequence_;
}

std::uint32_t GoldSrcNetchanState::highest_sequence_sent() const noexcept
{
    return highest_sequence_sent_;
}

std::uint32_t GoldSrcNetchanState::incoming_sequence() const noexcept
{
    return incoming_sequence_;
}

std::uint32_t
GoldSrcNetchanState::highest_accepted_acknowledgement() const noexcept
{
    return highest_accepted_acknowledgement_;
}

std::uint32_t GoldSrcNetchanState::last_reliable_sequence() const noexcept
{
    return last_reliable_sequence_;
}

bool GoldSrcNetchanState::local_reliable_sequence() const noexcept
{
    return local_reliable_sequence_;
}

bool GoldSrcNetchanState::incoming_reliable_sequence() const noexcept
{
    return incoming_reliable_sequence_;
}

bool GoldSrcNetchanState::incoming_reliable_acknowledgement() const noexcept
{
    return incoming_reliable_acknowledgement_;
}

bool GoldSrcNetchanState::reliable_pending() const noexcept
{
    return pending_reliable_size_ != 0u;
}

std::size_t GoldSrcNetchanState::reliable_pending_bytes() const noexcept
{
    return pending_reliable_size_;
}

const std::uint8_t* GoldSrcNetchanState::reliable_payload_data() const noexcept
{
    return pending_reliable_.data();
}

GoldSrcNetchanReliablePayloadKind
GoldSrcNetchanState::pending_reliable_kind() const noexcept
{
    return pending_reliable_kind_;
}

GoldSrcNetchanReliablePayloadKind
GoldSrcNetchanState::last_acknowledged_reliable_kind() const noexcept
{
    return last_acknowledged_reliable_kind_;
}

std::uint64_t
GoldSrcNetchanState::reliable_acknowledgement_generation() const noexcept
{
    return reliable_acknowledgement_generation_;
}

GoldSrcNetchanIncomingPayloadPolicy
GoldSrcNetchanState::incoming_payload_policy() const noexcept
{
    return incoming_payload_policy_;
}

bool GoldSrcNetchanState::has_accepted_activity() const noexcept
{
    return has_accepted_activity_;
}

GoldSrcNetchanState::TimePoint
GoldSrcNetchanState::last_accepted_at() const noexcept
{
    return last_accepted_at_;
}

GoldSrcNetchanTransportPhase
GoldSrcNetchanState::transport_phase() const noexcept
{
    return transport_phase_;
}

const GoldSrcNetchanDiagnostics&
GoldSrcNetchanState::diagnostics() const noexcept
{
    return diagnostics_;
}

bool GoldSrcNetchanState::AcknowledgementWasSent(
    std::uint32_t acknowledgement) const noexcept
{
    if (AcknowledgementIdentifiesSentSequence(acknowledgement))
    {
        return true;
    }
    return acknowledgement == 0u
        && highest_accepted_acknowledgement_ == 0u;
}

bool GoldSrcNetchanState::AcknowledgementIdentifiesSentSequence(
    std::uint32_t acknowledgement) const noexcept
{
    return GoldSrcNetchanSentWindowContains(
        acknowledgement,
        highest_sequence_sent_,
        sent_sequence_count_);
}

bool GoldSrcNetchanState::PayloadIsSupported(
    const GoldSrcNetchanPacket& packet) const noexcept
{
    if (incoming_payload_policy_
        == GoldSrcNetchanIncomingPayloadPolicy::
            kAcceptBoundedApplicationPayload)
    {
        return true;
    }

    for (std::size_t index = 0; index < packet.payload_size; ++index)
    {
        if (packet.payload[index] != kGoldSrcClientNop)
        {
            return false;
        }
    }
    return true;
}

GoldSrcNetchanState* FindGoldSrcNetchanSession(
    GoldSrcNetchanState* const* sessions,
    std::size_t session_count,
    const Ipv4Endpoint& sender) noexcept
{
    if (sessions == nullptr)
    {
        return nullptr;
    }

    for (std::size_t index = 0; index < session_count; ++index)
    {
        GoldSrcNetchanState* session = sessions[index];
        if (session != nullptr
            && session->initialized()
            && session->remote_endpoint() == sender)
        {
            return session;
        }
    }
    return nullptr;
}
} // namespace hl::network
