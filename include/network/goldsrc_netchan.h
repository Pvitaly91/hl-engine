#pragma once

#include "network/ipv4_endpoint.h"

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace hl::network
{
inline constexpr std::size_t kGoldSrcNetchanBaseHeaderBytes = 8;
inline constexpr std::size_t kGoldSrcNetchanMinimumDatagramBytes = 16;
inline constexpr std::size_t kGoldSrcNetchanMaximumRouteableBytes = 1400;
inline constexpr std::size_t kGoldSrcNetchanMaximumPayloadBytes =
    kGoldSrcNetchanMaximumRouteableBytes - kGoldSrcNetchanBaseHeaderBytes;
inline constexpr std::size_t kGoldSrcNetchanMaximumReliableBytes = 1200;
inline constexpr std::uint32_t kGoldSrcNetchanSequenceMask = 0x3FFFFFFFu;
inline constexpr std::uint32_t kGoldSrcNetchanFragmentFlag = 0x40000000u;
inline constexpr std::uint32_t kGoldSrcNetchanReliableFlag = 0x80000000u;
inline constexpr std::uint8_t kGoldSrcServerNop = 0x01u;
inline constexpr std::uint8_t kGoldSrcClientNop = 0x01u;

enum class GoldSrcNetchanDirection
{
    kServerToClient,
    kClientToServer,
};

enum class GoldSrcNetchanCodecStatus
{
    kOk,
    kMalformedHeader,
    kOversized,
    kUnsupportedReservedFlags,
    kUnsupportedFragment,
    kPayloadTooLarge,
};

std::string_view ReasonFor(GoldSrcNetchanCodecStatus status) noexcept;

struct GoldSrcNetchanPacket final
{
    std::uint32_t raw_sequence = 0;
    std::uint32_t raw_acknowledgement = 0;
    std::uint32_t sequence = 0;
    std::uint32_t acknowledgement = 0;
    bool reliable_present = false;
    bool reliable_acknowledgement = false;
    bool fragment_present = false;
    std::array<std::uint8_t, kGoldSrcNetchanMaximumPayloadBytes> payload{};
    std::size_t payload_size = 0;
};

struct GoldSrcNetchanDecodeResult final
{
    GoldSrcNetchanCodecStatus status = GoldSrcNetchanCodecStatus::kMalformedHeader;
    GoldSrcNetchanPacket packet;

    bool ok() const noexcept
    {
        return status == GoldSrcNetchanCodecStatus::kOk;
    }
};

struct GoldSrcNetchanDatagram final
{
    std::array<std::uint8_t, kGoldSrcNetchanMaximumRouteableBytes> bytes{};
    std::size_t size = 0;
};

GoldSrcNetchanDecodeResult DecodeGoldSrcNetchanDatagram(
    GoldSrcNetchanDirection direction,
    const std::uint8_t* bytes,
    std::size_t size) noexcept;

GoldSrcNetchanCodecStatus EncodeGoldSrcNetchanDatagram(
    GoldSrcNetchanDirection direction,
    std::uint32_t sequence,
    std::uint32_t acknowledgement,
    bool reliable_present,
    bool reliable_acknowledgement,
    const std::uint8_t* payload,
    std::size_t payload_size,
    bool pad_to_minimum,
    GoldSrcNetchanDatagram* datagram) noexcept;

// GoldSrc protocol 48 transforms only complete four-byte groups after the
// eight-byte netchan header. The key is the low byte of the outgoing sequence.
void MungeGoldSrcNetchanPayload(
    std::uint8_t* bytes,
    std::size_t size,
    std::uint32_t sequence) noexcept;
void UnmungeGoldSrcNetchanPayload(
    std::uint8_t* bytes,
    std::size_t size,
    std::uint32_t sequence) noexcept;

bool IsGoldSrcNetchanSequenceNewer(
    std::uint32_t candidate,
    std::uint32_t baseline) noexcept;
std::uint32_t GoldSrcNetchanSequenceDistance(
    std::uint32_t newer,
    std::uint32_t older) noexcept;
std::uint32_t NextGoldSrcNetchanSequence(std::uint32_t sequence) noexcept;
bool GoldSrcNetchanSentWindowContains(
    std::uint32_t sequence,
    std::uint32_t highest_sequence_sent,
    std::uint64_t sent_sequence_count) noexcept;

enum class GoldSrcNetchanTransportPhase
{
    kNone,
    kAwaitingFirstReliableAck,
    kEstablished,
};

std::string_view NameFor(GoldSrcNetchanTransportPhase phase) noexcept;

enum class GoldSrcNetchanQueueResult
{
    kQueued,
    kNotInitialized,
    kEmptyPayload,
    kPayloadTooLarge,
    kReliableAlreadyPending,
};

std::string_view ReasonFor(GoldSrcNetchanQueueResult result) noexcept;

enum class GoldSrcNetchanProcessResult
{
    kAccepted,
    kDuplicateSequence,
    kOutOfOrderSequence,
    kMalformedHeader,
    kUnsupportedFragment,
    kEndpointMismatch,
    kQportMismatch,
    kStaleAck,
    kFutureAck,
    kPayloadDecodeFailed,
    kUnsupportedPayload,
    kUnsupportedReservedFlags,
};

std::string_view ReasonFor(GoldSrcNetchanProcessResult result) noexcept;

struct GoldSrcNetchanDiagnostics final
{
    std::uint64_t sequenced_received = 0;
    std::uint64_t sequenced_sent = 0;
    std::uint64_t accepted = 0;
    std::uint64_t malformed_header = 0;
    std::uint64_t unsupported_fragment = 0;
    std::uint64_t endpoint_mismatch = 0;
    std::uint64_t qport_mismatch = 0;
    std::uint64_t duplicate_sequence = 0;
    std::uint64_t out_of_order_sequence = 0;
    std::uint64_t sequence_gaps = 0;
    std::uint64_t dropped_incoming_packets = 0;
    std::uint64_t stale_ack = 0;
    std::uint64_t future_ack = 0;
    std::uint64_t payload_decode_failed = 0;
    std::uint64_t unsupported_payload = 0;
    std::uint64_t unsupported_reserved_flags = 0;
    std::uint64_t reliable_queued = 0;
    std::uint64_t reliable_sent = 0;
    std::uint64_t reliable_resent = 0;
    std::uint64_t reliable_acked = 0;
    std::uint64_t reliable_ack_mismatch = 0;
};

class GoldSrcNetchanState final
{
public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

    void Reset() noexcept;
    bool Initialize(
        const Ipv4Endpoint& endpoint,
        std::optional<std::uint16_t> connect_qport,
        std::uint16_t channel_identifier,
        TimePoint now) noexcept;

    GoldSrcNetchanQueueResult QueueReliablePayload(
        const std::uint8_t* bytes,
        std::size_t size) noexcept;
    bool BuildOutgoingDatagram(GoldSrcNetchanDatagram* datagram) noexcept;
    GoldSrcNetchanProcessResult ProcessIncomingDatagram(
        const Ipv4Endpoint& sender,
        const GoldSrcNetchanPacket& packet,
        TimePoint now) noexcept;
    void RecordRejected(GoldSrcNetchanProcessResult result) noexcept;

    bool initialized() const noexcept;
    const Ipv4Endpoint& remote_endpoint() const noexcept;
    const std::optional<std::uint16_t>& connect_qport() const noexcept;
    std::uint16_t channel_identifier() const noexcept;
    std::uint32_t outgoing_sequence() const noexcept;
    std::uint32_t highest_sequence_sent() const noexcept;
    std::uint32_t incoming_sequence() const noexcept;
    std::uint32_t highest_accepted_acknowledgement() const noexcept;
    std::uint32_t last_reliable_sequence() const noexcept;
    bool local_reliable_sequence() const noexcept;
    bool incoming_reliable_sequence() const noexcept;
    bool incoming_reliable_acknowledgement() const noexcept;
    bool reliable_pending() const noexcept;
    std::size_t reliable_pending_bytes() const noexcept;
    const std::uint8_t* reliable_payload_data() const noexcept;
    bool has_accepted_activity() const noexcept;
    TimePoint last_accepted_at() const noexcept;
    GoldSrcNetchanTransportPhase transport_phase() const noexcept;
    const GoldSrcNetchanDiagnostics& diagnostics() const noexcept;

private:
    bool AcknowledgementWasSent(std::uint32_t acknowledgement) const noexcept;
    bool AcknowledgementIdentifiesSentSequence(
        std::uint32_t acknowledgement) const noexcept;
    bool PayloadIsSupported(const GoldSrcNetchanPacket& packet) const noexcept;

    bool initialized_ = false;
    Ipv4Endpoint remote_endpoint_{};
    std::optional<std::uint16_t> connect_qport_;
    std::uint16_t channel_identifier_ = 0;
    std::uint32_t outgoing_sequence_ = 1;
    std::uint32_t highest_sequence_sent_ = 0;
    std::uint64_t sent_sequence_count_ = 0;
    std::uint32_t incoming_sequence_ = 0;
    std::uint32_t highest_accepted_acknowledgement_ = 0;
    std::uint32_t last_reliable_sequence_ = 0;
    bool local_reliable_sequence_ = false;
    bool incoming_reliable_sequence_ = false;
    bool incoming_reliable_acknowledgement_ = false;
    bool pending_has_been_sent_ = false;
    std::array<std::uint8_t, kGoldSrcNetchanMaximumReliableBytes> pending_reliable_{};
    std::size_t pending_reliable_size_ = 0;
    bool has_accepted_activity_ = false;
    TimePoint initialized_at_{};
    TimePoint last_accepted_at_{};
    GoldSrcNetchanTransportPhase transport_phase_ =
        GoldSrcNetchanTransportPhase::kNone;
    GoldSrcNetchanDiagnostics diagnostics_{};
};

GoldSrcNetchanState* FindGoldSrcNetchanSession(
    GoldSrcNetchanState* const* sessions,
    std::size_t session_count,
    const Ipv4Endpoint& sender) noexcept;
} // namespace hl::network
