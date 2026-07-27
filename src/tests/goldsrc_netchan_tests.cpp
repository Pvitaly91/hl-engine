#include "network/goldsrc_connectionless.h"
#include "network/goldsrc_netchan.h"
#include "network/ipv4_endpoint.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace
{
using namespace hl::network;

std::vector<std::uint8_t> DatagramBytes(
    const GoldSrcNetchanDatagram& datagram)
{
    return std::vector<std::uint8_t>(
        datagram.bytes.begin(),
        datagram.bytes.begin()
            + static_cast<std::ptrdiff_t>(datagram.size));
}

GoldSrcNetchanPacket Packet(
    std::uint32_t sequence,
    std::uint32_t acknowledgement,
    bool reliable_present = false,
    bool reliable_acknowledgement = false,
    std::initializer_list<std::uint8_t> payload = {})
{
    assert(payload.size() <= kGoldSrcNetchanMaximumPayloadBytes);
    GoldSrcNetchanPacket packet{};
    packet.sequence = sequence & kGoldSrcNetchanSequenceMask;
    packet.acknowledgement =
        acknowledgement & kGoldSrcNetchanSequenceMask;
    packet.reliable_present = reliable_present;
    packet.reliable_acknowledgement = reliable_acknowledgement;
    packet.raw_sequence = packet.sequence
        | (reliable_present ? kGoldSrcNetchanReliableFlag : 0u);
    packet.raw_acknowledgement = packet.acknowledgement
        | (reliable_acknowledgement ? kGoldSrcNetchanReliableFlag : 0u);
    packet.payload_size = payload.size();
    std::copy(payload.begin(), payload.end(), packet.payload.begin());
    return packet;
}

struct StateSnapshot final
{
    bool initialized = false;
    Ipv4Endpoint endpoint{};
    std::optional<std::uint16_t> connect_qport;
    std::uint16_t channel_identifier = 0;
    std::uint32_t outgoing_sequence = 0;
    std::uint32_t highest_sequence_sent = 0;
    std::uint32_t incoming_sequence = 0;
    std::uint32_t highest_accepted_acknowledgement = 0;
    std::uint32_t last_reliable_sequence = 0;
    bool local_reliable_sequence = false;
    bool incoming_reliable_sequence = false;
    bool incoming_reliable_acknowledgement = false;
    bool reliable_pending = false;
    std::size_t reliable_pending_bytes = 0;
    std::array<std::uint8_t, kGoldSrcNetchanMaximumReliableBytes>
        reliable_payload{};
    GoldSrcNetchanReliablePayloadKind pending_reliable_kind =
        GoldSrcNetchanReliablePayloadKind::kNone;
    GoldSrcNetchanReliablePayloadKind last_acknowledged_reliable_kind =
        GoldSrcNetchanReliablePayloadKind::kNone;
    std::uint64_t reliable_acknowledgement_generation = 0;
    GoldSrcNetchanIncomingPayloadPolicy incoming_payload_policy =
        GoldSrcNetchanIncomingPayloadPolicy::kStrictClientNopOnly;
    bool has_accepted_activity = false;
    GoldSrcNetchanState::TimePoint last_accepted_at{};
    GoldSrcNetchanTransportPhase transport_phase =
        GoldSrcNetchanTransportPhase::kNone;
};

StateSnapshot Snapshot(const GoldSrcNetchanState& state)
{
    StateSnapshot snapshot{};
    snapshot.initialized = state.initialized();
    snapshot.endpoint = state.remote_endpoint();
    snapshot.connect_qport = state.connect_qport();
    snapshot.channel_identifier = state.channel_identifier();
    snapshot.outgoing_sequence = state.outgoing_sequence();
    snapshot.highest_sequence_sent = state.highest_sequence_sent();
    snapshot.incoming_sequence = state.incoming_sequence();
    snapshot.highest_accepted_acknowledgement =
        state.highest_accepted_acknowledgement();
    snapshot.last_reliable_sequence = state.last_reliable_sequence();
    snapshot.local_reliable_sequence = state.local_reliable_sequence();
    snapshot.incoming_reliable_sequence =
        state.incoming_reliable_sequence();
    snapshot.incoming_reliable_acknowledgement =
        state.incoming_reliable_acknowledgement();
    snapshot.reliable_pending = state.reliable_pending();
    snapshot.reliable_pending_bytes = state.reliable_pending_bytes();
    std::copy_n(
        state.reliable_payload_data(),
        snapshot.reliable_payload.size(),
        snapshot.reliable_payload.begin());
    snapshot.pending_reliable_kind = state.pending_reliable_kind();
    snapshot.last_acknowledged_reliable_kind =
        state.last_acknowledged_reliable_kind();
    snapshot.reliable_acknowledgement_generation =
        state.reliable_acknowledgement_generation();
    snapshot.incoming_payload_policy = state.incoming_payload_policy();
    snapshot.has_accepted_activity = state.has_accepted_activity();
    snapshot.last_accepted_at = state.last_accepted_at();
    snapshot.transport_phase = state.transport_phase();
    return snapshot;
}

void AssertMutationStateEquals(
    const StateSnapshot& expected,
    const GoldSrcNetchanState& actual)
{
    assert(actual.initialized() == expected.initialized);
    assert(actual.remote_endpoint() == expected.endpoint);
    assert(actual.connect_qport() == expected.connect_qport);
    assert(actual.channel_identifier() == expected.channel_identifier);
    assert(actual.outgoing_sequence() == expected.outgoing_sequence);
    assert(actual.highest_sequence_sent() == expected.highest_sequence_sent);
    assert(actual.incoming_sequence() == expected.incoming_sequence);
    assert(
        actual.highest_accepted_acknowledgement()
        == expected.highest_accepted_acknowledgement);
    assert(
        actual.last_reliable_sequence()
        == expected.last_reliable_sequence);
    assert(
        actual.local_reliable_sequence()
        == expected.local_reliable_sequence);
    assert(
        actual.incoming_reliable_sequence()
        == expected.incoming_reliable_sequence);
    assert(
        actual.incoming_reliable_acknowledgement()
        == expected.incoming_reliable_acknowledgement);
    assert(actual.reliable_pending() == expected.reliable_pending);
    assert(
        actual.reliable_pending_bytes()
        == expected.reliable_pending_bytes);
    assert(std::equal(
        expected.reliable_payload.begin(),
        expected.reliable_payload.end(),
        actual.reliable_payload_data()));
    assert(
        actual.pending_reliable_kind()
        == expected.pending_reliable_kind);
    assert(
        actual.last_acknowledged_reliable_kind()
        == expected.last_acknowledged_reliable_kind);
    assert(
        actual.reliable_acknowledgement_generation()
        == expected.reliable_acknowledgement_generation);
    assert(
        actual.incoming_payload_policy()
        == expected.incoming_payload_policy);
    assert(
        actual.has_accepted_activity()
        == expected.has_accepted_activity);
    assert(actual.last_accepted_at() == expected.last_accepted_at);
    assert(actual.transport_phase() == expected.transport_phase);
}

void AssertDiagnosticsAreZero(const GoldSrcNetchanDiagnostics& diagnostics)
{
    assert(diagnostics.sequenced_received == 0u);
    assert(diagnostics.sequenced_sent == 0u);
    assert(diagnostics.accepted == 0u);
    assert(diagnostics.malformed_header == 0u);
    assert(diagnostics.unsupported_fragment == 0u);
    assert(diagnostics.endpoint_mismatch == 0u);
    assert(diagnostics.qport_mismatch == 0u);
    assert(diagnostics.duplicate_sequence == 0u);
    assert(diagnostics.out_of_order_sequence == 0u);
    assert(diagnostics.sequence_gaps == 0u);
    assert(diagnostics.dropped_incoming_packets == 0u);
    assert(diagnostics.stale_ack == 0u);
    assert(diagnostics.future_ack == 0u);
    assert(diagnostics.payload_decode_failed == 0u);
    assert(diagnostics.unsupported_payload == 0u);
    assert(diagnostics.unsupported_reserved_flags == 0u);
    assert(diagnostics.reliable_queued == 0u);
    assert(diagnostics.reliable_sent == 0u);
    assert(diagnostics.reliable_resent == 0u);
    assert(diagnostics.reliable_acked == 0u);
    assert(diagnostics.reliable_ack_mismatch == 0u);
}

void AssertCleanState(const GoldSrcNetchanState& state)
{
    const Ipv4Endpoint empty_endpoint{};
    assert(!state.initialized());
    assert(state.remote_endpoint() == empty_endpoint);
    assert(!state.connect_qport().has_value());
    assert(state.channel_identifier() == 0u);
    assert(state.outgoing_sequence() == 1u);
    assert(state.highest_sequence_sent() == 0u);
    assert(state.incoming_sequence() == 0u);
    assert(state.highest_accepted_acknowledgement() == 0u);
    assert(state.last_reliable_sequence() == 0u);
    assert(!state.local_reliable_sequence());
    assert(!state.incoming_reliable_sequence());
    assert(!state.incoming_reliable_acknowledgement());
    assert(!state.reliable_pending());
    assert(state.reliable_pending_bytes() == 0u);
    assert(
        state.pending_reliable_kind()
        == GoldSrcNetchanReliablePayloadKind::kNone);
    assert(
        state.last_acknowledged_reliable_kind()
        == GoldSrcNetchanReliablePayloadKind::kNone);
    assert(state.reliable_acknowledgement_generation() == 0u);
    assert(
        state.incoming_payload_policy()
        == GoldSrcNetchanIncomingPayloadPolicy::kStrictClientNopOnly);
    for (std::size_t index = 0;
         index < kGoldSrcNetchanMaximumReliableBytes;
         ++index)
    {
        assert(state.reliable_payload_data()[index] == 0u);
    }
    assert(!state.has_accepted_activity());
    assert(state.last_accepted_at() == GoldSrcNetchanState::TimePoint{});
    assert(state.transport_phase() == GoldSrcNetchanTransportPhase::kNone);
    AssertDiagnosticsAreZero(state.diagnostics());
}

std::vector<std::uint8_t> Connectionless(std::string_view body)
{
    std::vector<std::uint8_t> packet = {0xFFu, 0xFFu, 0xFFu, 0xFFu};
    packet.insert(packet.end(), body.begin(), body.end());
    return packet;
}

std::vector<std::uint8_t> ConnectPacket(
    std::int32_t challenge,
    std::uint16_t qport)
{
    std::string line = "connect 48 ";
    line += std::to_string(challenge);
    line += " \"\\prot\\3\\raw\\steam\\qport\\";
    line += std::to_string(qport);
    line += "\\ext\\0\" \"\\name\\Netchan Test\\model\\gordon\"\n";
    return Connectionless(line);
}

void TestCodecBoundsFlagsAndDirections()
{
    assert(kGoldSrcNetchanBaseHeaderBytes == 8u);
    assert(kGoldSrcNetchanMinimumDatagramBytes == 16u);
    assert(kGoldSrcNetchanMaximumRouteableBytes == 1400u);
    assert(kGoldSrcNetchanMaximumPayloadBytes == 1392u);
    assert(kGoldSrcNetchanSequenceMask == 0x3FFFFFFFu);
    assert(kGoldSrcNetchanFragmentFlag == 0x40000000u);
    assert(kGoldSrcNetchanReliableFlag == 0x80000000u);

    std::array<std::uint8_t, kGoldSrcNetchanBaseHeaderBytes> short_bytes{};
    for (std::size_t size = 0u;
         size < kGoldSrcNetchanBaseHeaderBytes;
         ++size)
    {
        assert(
            DecodeGoldSrcNetchanDatagram(
                GoldSrcNetchanDirection::kClientToServer,
                short_bytes.data(),
                size).status
            == GoldSrcNetchanCodecStatus::kMalformedHeader);
    }
    assert(
        DecodeGoldSrcNetchanDatagram(
            GoldSrcNetchanDirection::kClientToServer,
            nullptr,
            kGoldSrcNetchanBaseHeaderBytes).status
        == GoldSrcNetchanCodecStatus::kMalformedHeader);

    GoldSrcNetchanDatagram masked{};
    assert(
        EncodeGoldSrcNetchanDatagram(
            GoldSrcNetchanDirection::kServerToClient,
            0xFFFFFFFFu,
            0xC1234567u,
            true,
            true,
            nullptr,
            0u,
            false,
            &masked)
        == GoldSrcNetchanCodecStatus::kOk);
    const std::vector<std::uint8_t> expected_masked = {
        0xFFu, 0xFFu, 0xFFu, 0xBFu,
        0x67u, 0x45u, 0x23u, 0x81u,
    };
    assert(DatagramBytes(masked) == expected_masked);

    const GoldSrcNetchanDecodeResult decoded_masked =
        DecodeGoldSrcNetchanDatagram(
            GoldSrcNetchanDirection::kServerToClient,
            masked.bytes.data(),
            masked.size);
    assert(decoded_masked.ok());
    assert(decoded_masked.packet.raw_sequence == 0xBFFFFFFFu);
    assert(decoded_masked.packet.raw_acknowledgement == 0x81234567u);
    assert(decoded_masked.packet.sequence == kGoldSrcNetchanSequenceMask);
    assert(decoded_masked.packet.acknowledgement == 0x01234567u);
    assert(decoded_masked.packet.reliable_present);
    assert(decoded_masked.packet.reliable_acknowledgement);
    assert(!decoded_masked.packet.fragment_present);

    const std::array<std::uint8_t, 8> fragment = {
        0x01u, 0x00u, 0x00u, 0x40u,
        0x00u, 0x00u, 0x00u, 0x00u,
    };
    const GoldSrcNetchanDecodeResult fragmented =
        DecodeGoldSrcNetchanDatagram(
            GoldSrcNetchanDirection::kClientToServer,
            fragment.data(),
            fragment.size());
    assert(fragmented.status == GoldSrcNetchanCodecStatus::kMalformedFragment);
    assert(fragmented.packet.raw_sequence == 0x40000001u);
    assert(fragmented.packet.sequence == 1u);
    assert(fragmented.packet.fragment_present);

    const std::array<std::uint8_t, 8> acknowledgement_reserved = {
        0x01u, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x00u, 0x40u,
    };
    const GoldSrcNetchanDecodeResult reserved =
        DecodeGoldSrcNetchanDatagram(
            GoldSrcNetchanDirection::kClientToServer,
            acknowledgement_reserved.data(),
            acknowledgement_reserved.size());
    assert(
        reserved.status
        == GoldSrcNetchanCodecStatus::kUnsupportedReservedFlags);
    assert(reserved.packet.raw_acknowledgement == 0x40000000u);
    assert(reserved.packet.acknowledgement == 0u);

    const std::vector<std::uint8_t> exact_base_header = {
        0x04u, 0x03u, 0x02u, 0x01u,
        0x08u, 0x07u, 0x06u, 0x05u,
    };
    for (const GoldSrcNetchanDirection direction : {
             GoldSrcNetchanDirection::kServerToClient,
             GoldSrcNetchanDirection::kClientToServer})
    {
        GoldSrcNetchanDatagram base_only{};
        assert(
            EncodeGoldSrcNetchanDatagram(
                direction,
                0x01020304u,
                0x05060708u,
                false,
                false,
                nullptr,
                0u,
                false,
                &base_only)
            == GoldSrcNetchanCodecStatus::kOk);
        assert(base_only.size == kGoldSrcNetchanBaseHeaderBytes);
        assert(DatagramBytes(base_only) == exact_base_header);

        const GoldSrcNetchanDecodeResult base_decoded =
            DecodeGoldSrcNetchanDatagram(
                direction,
                base_only.bytes.data(),
                base_only.size);
        assert(base_decoded.ok());
        assert(base_decoded.packet.sequence == 0x01020304u);
        assert(base_decoded.packet.acknowledgement == 0x05060708u);
        assert(base_decoded.packet.payload_size == 0u);
    }

    assert(ReasonFor(GoldSrcNetchanCodecStatus::kUnsupportedFragment)
        == "unsupported_fragment");
    assert(ReasonFor(GoldSrcNetchanCodecStatus::kUnsupportedReservedFlags)
        == "unsupported_reserved_flags");
}

void TestMunge2GoldenRoundTripAndCompleteGroups()
{
    std::array<std::uint8_t, 8> bytes = {
        0x00u, 0x01u, 0x02u, 0x03u,
        0x04u, 0x05u, 0x06u, 0x07u,
    };
    const std::array<std::uint8_t, 8> original = bytes;
    const std::array<std::uint8_t, 8> expected_munged = {
        0x21u, 0x1Au, 0x01u, 0x78u,
        0x65u, 0x06u, 0x15u, 0x3Cu,
    };
    MungeGoldSrcNetchanPayload(bytes.data(), bytes.size(), 0x12345678u);
    assert(bytes == expected_munged);
    UnmungeGoldSrcNetchanPayload(bytes.data(), bytes.size(), 0x12345678u);
    assert(bytes == original);

    std::array<std::uint8_t, 10> partial = {
        0x00u, 0x01u, 0x02u, 0x03u, 0x04u,
        0x05u, 0x06u, 0x07u, 0x08u, 0x09u,
    };
    const std::array<std::uint8_t, 10> partial_original = partial;
    MungeGoldSrcNetchanPayload(
        partial.data(),
        partial.size(),
        0x12345678u);
    assert(std::equal(
        expected_munged.begin(),
        expected_munged.end(),
        partial.begin()));
    assert(partial[8] == 0x08u);
    assert(partial[9] == 0x09u);
    UnmungeGoldSrcNetchanPayload(
        partial.data(),
        partial.size(),
        0x12345678u);
    assert(partial == partial_original);

    std::array<std::uint8_t, 3> incomplete = {0x11u, 0x22u, 0x33u};
    const std::array<std::uint8_t, 3> incomplete_original = incomplete;
    MungeGoldSrcNetchanPayload(
        incomplete.data(),
        incomplete.size(),
        0x78u);
    assert(incomplete == incomplete_original);
    UnmungeGoldSrcNetchanPayload(
        incomplete.data(),
        incomplete.size(),
        0x78u);
    assert(incomplete == incomplete_original);

    std::array<std::uint8_t, 8> low_byte_key = original;
    MungeGoldSrcNetchanPayload(
        low_byte_key.data(),
        low_byte_key.size(),
        0x78u);
    assert(low_byte_key == expected_munged);
    MungeGoldSrcNetchanPayload(nullptr, 8u, 1u);
    UnmungeGoldSrcNetchanPayload(nullptr, 8u, 1u);
}

void TestGoldenPacketsPaddingAndRouteableLimit()
{
    const std::uint8_t nop = kGoldSrcServerNop;
    GoldSrcNetchanDatagram server_first{};
    assert(
        EncodeGoldSrcNetchanDatagram(
            GoldSrcNetchanDirection::kServerToClient,
            1u,
            0u,
            true,
            false,
            &nop,
            1u,
            true,
            &server_first)
        == GoldSrcNetchanCodecStatus::kOk);
    const std::vector<std::uint8_t> expected_server_first = {
        0x01u, 0x00u, 0x00u, 0x80u,
        0x00u, 0x00u, 0x00u, 0x00u,
        0x5Au, 0x19u, 0x01u, 0x00u,
        0x1Au, 0x01u, 0x11u, 0x40u,
    };
    assert(server_first.size == kGoldSrcNetchanMinimumDatagramBytes);
    assert(DatagramBytes(server_first) == expected_server_first);

    const GoldSrcNetchanDecodeResult decoded_server_first =
        DecodeGoldSrcNetchanDatagram(
            GoldSrcNetchanDirection::kServerToClient,
            server_first.bytes.data(),
            server_first.size);
    assert(decoded_server_first.ok());
    assert(decoded_server_first.packet.sequence == 1u);
    assert(decoded_server_first.packet.acknowledgement == 0u);
    assert(decoded_server_first.packet.reliable_present);
    assert(!decoded_server_first.packet.reliable_acknowledgement);
    assert(!decoded_server_first.packet.fragment_present);
    assert(decoded_server_first.packet.payload_size == 8u);
    for (std::size_t index = 0;
         index < decoded_server_first.packet.payload_size;
         ++index)
    {
        assert(decoded_server_first.packet.payload[index] == kGoldSrcServerNop);
    }

    const std::uint8_t client_nop = kGoldSrcClientNop;
    GoldSrcNetchanDatagram correct_client_ack{};
    assert(
        EncodeGoldSrcNetchanDatagram(
            GoldSrcNetchanDirection::kClientToServer,
            1u,
            1u,
            false,
            true,
            &client_nop,
            1u,
            true,
            &correct_client_ack)
        == GoldSrcNetchanCodecStatus::kOk);
    const std::vector<std::uint8_t> expected_client_ack = {
        0x01u, 0x00u, 0x00u, 0x00u,
        0x01u, 0x00u, 0x00u, 0x80u,
        0x5Au, 0x19u, 0x01u, 0x00u,
        0x1Au, 0x01u, 0x11u, 0x40u,
    };
    assert(DatagramBytes(correct_client_ack) == expected_client_ack);

    const GoldSrcNetchanDecodeResult decoded_client_ack =
        DecodeGoldSrcNetchanDatagram(
            GoldSrcNetchanDirection::kClientToServer,
            correct_client_ack.bytes.data(),
            correct_client_ack.size);
    assert(decoded_client_ack.ok());
    assert(decoded_client_ack.packet.sequence == 1u);
    assert(decoded_client_ack.packet.acknowledgement == 1u);
    assert(!decoded_client_ack.packet.reliable_present);
    assert(decoded_client_ack.packet.reliable_acknowledgement);
    assert(decoded_client_ack.packet.payload_size == 8u);
    for (std::size_t index = 0;
         index < decoded_client_ack.packet.payload_size;
         ++index)
    {
        assert(decoded_client_ack.packet.payload[index] == kGoldSrcClientNop);
    }

    GoldSrcNetchanDatagram empty_padded{};
    assert(
        EncodeGoldSrcNetchanDatagram(
            GoldSrcNetchanDirection::kServerToClient,
            7u,
            4u,
            false,
            false,
            nullptr,
            0u,
            true,
            &empty_padded)
        == GoldSrcNetchanCodecStatus::kOk);
    assert(empty_padded.size == kGoldSrcNetchanMinimumDatagramBytes);
    const GoldSrcNetchanDecodeResult decoded_empty_padded =
        DecodeGoldSrcNetchanDatagram(
            GoldSrcNetchanDirection::kServerToClient,
            empty_padded.bytes.data(),
            empty_padded.size);
    assert(decoded_empty_padded.ok());
    assert(decoded_empty_padded.packet.payload_size == 8u);
    for (std::size_t index = 0;
         index < decoded_empty_padded.packet.payload_size;
         ++index)
    {
        assert(decoded_empty_padded.packet.payload[index] == kGoldSrcServerNop);
    }

    std::vector<std::uint8_t> maximum_payload(
        kGoldSrcNetchanMaximumPayloadBytes,
        kGoldSrcServerNop);
    GoldSrcNetchanDatagram maximum{};
    assert(
        EncodeGoldSrcNetchanDatagram(
            GoldSrcNetchanDirection::kServerToClient,
            17u,
            9u,
            false,
            false,
            maximum_payload.data(),
            maximum_payload.size(),
            true,
            &maximum)
        == GoldSrcNetchanCodecStatus::kOk);
    assert(maximum.size == kGoldSrcNetchanMaximumRouteableBytes);
    const GoldSrcNetchanDecodeResult decoded_maximum =
        DecodeGoldSrcNetchanDatagram(
            GoldSrcNetchanDirection::kServerToClient,
            maximum.bytes.data(),
            maximum.size);
    assert(decoded_maximum.ok());
    assert(decoded_maximum.packet.payload_size == maximum_payload.size());
    assert(std::equal(
        maximum_payload.begin(),
        maximum_payload.end(),
        decoded_maximum.packet.payload.begin()));

    std::vector<std::uint8_t> oversized_datagram(
        kGoldSrcNetchanMaximumRouteableBytes + 1u,
        0u);
    assert(
        DecodeGoldSrcNetchanDatagram(
            GoldSrcNetchanDirection::kClientToServer,
            oversized_datagram.data(),
            oversized_datagram.size()).status
        == GoldSrcNetchanCodecStatus::kOversized);

    std::vector<std::uint8_t> oversized_payload(
        kGoldSrcNetchanMaximumPayloadBytes + 1u,
        kGoldSrcServerNop);
    GoldSrcNetchanDatagram rejected{};
    assert(
        EncodeGoldSrcNetchanDatagram(
            GoldSrcNetchanDirection::kServerToClient,
            1u,
            0u,
            false,
            false,
            oversized_payload.data(),
            oversized_payload.size(),
            false,
            &rejected)
        == GoldSrcNetchanCodecStatus::kPayloadTooLarge);
    assert(
        EncodeGoldSrcNetchanDatagram(
            GoldSrcNetchanDirection::kServerToClient,
            1u,
            0u,
            false,
            false,
            nullptr,
            1u,
            false,
            &rejected)
        == GoldSrcNetchanCodecStatus::kMalformedHeader);
    assert(
        EncodeGoldSrcNetchanDatagram(
            GoldSrcNetchanDirection::kServerToClient,
            1u,
            0u,
            false,
            false,
            nullptr,
            0u,
            false,
            nullptr)
        == GoldSrcNetchanCodecStatus::kMalformedHeader);
}

void TestSequenceArithmetic()
{
    constexpr std::uint32_t maximum = kGoldSrcNetchanSequenceMask;
    constexpr std::uint32_t half_range = 0x20000000u;

    assert(GoldSrcNetchanSequenceDistance(7u, 4u) == 3u);
    assert(GoldSrcNetchanSequenceDistance(0u, maximum) == 1u);
    assert(GoldSrcNetchanSequenceDistance(2u, maximum) == 3u);
    assert(
        GoldSrcNetchanSequenceDistance(
            kGoldSrcNetchanReliableFlag | 1u,
            0u)
        == 1u);
    assert(NextGoldSrcNetchanSequence(0u) == 1u);
    assert(NextGoldSrcNetchanSequence(maximum) == 0u);
    assert(NextGoldSrcNetchanSequence(0xFFFFFFFFu) == 0u);

    assert(IsGoldSrcNetchanSequenceNewer(1u, 0u));
    assert(!IsGoldSrcNetchanSequenceNewer(0u, 0u));
    assert(IsGoldSrcNetchanSequenceNewer(0u, maximum));
    assert(!IsGoldSrcNetchanSequenceNewer(maximum, 0u));
    assert(IsGoldSrcNetchanSequenceNewer(half_range - 1u, 0u));
    assert(!IsGoldSrcNetchanSequenceNewer(half_range, 0u));
    assert(!IsGoldSrcNetchanSequenceNewer(0u, half_range));
    assert(!IsGoldSrcNetchanSequenceNewer(half_range + 1u, 0u));

    assert(!GoldSrcNetchanSentWindowContains(0u, 0u, 0u));
    assert(GoldSrcNetchanSentWindowContains(1u, 1u, 1u));
    assert(!GoldSrcNetchanSentWindowContains(0u, 1u, 1u));
    assert(GoldSrcNetchanSentWindowContains(1u, 2u, 2u));
    assert(!GoldSrcNetchanSentWindowContains(0u, 2u, 2u));

    // Immediately before wrap, sequence zero is a future sequence rather
    // than a sent packet identified by the initial ACK sentinel.
    assert(GoldSrcNetchanSentWindowContains(maximum, maximum, maximum));
    assert(!GoldSrcNetchanSentWindowContains(0u, maximum, maximum));

    // Once zero is actually sent, both it and the immediately preceding
    // sequence are in the unambiguous modular sent window.
    const std::uint64_t sent_through_zero =
        static_cast<std::uint64_t>(maximum) + 1u;
    assert(GoldSrcNetchanSentWindowContains(0u, 0u, sent_through_zero));
    assert(GoldSrcNetchanSentWindowContains(maximum, 0u, sent_through_zero));

    // A newly sent sequence remains valid beyond the old first-sequence
    // half-range boundary, while the exactly half-range-old value is not.
    assert(GoldSrcNetchanSentWindowContains(
        0x20000001u,
        0x20000001u,
        0x20000001ull));
    assert(!GoldSrcNetchanSentWindowContains(
        1u,
        0x20000001u,
        0x20000001ull));
}

void TestInitialStateFirstSequenceDuplicatesAndGaps()
{
    using namespace std::chrono_literals;
    const Ipv4Endpoint endpoint{{127u, 0u, 0u, 1u}, 27015u};
    const Ipv4Endpoint other_endpoint{{127u, 0u, 0u, 1u}, 27016u};
    const GoldSrcNetchanState::TimePoint start{};

    GoldSrcNetchanState state;
    AssertCleanState(state);
    const std::uint8_t nop = kGoldSrcClientNop;
    assert(
        state.QueueReliablePayload(&nop, 1u)
        == GoldSrcNetchanQueueResult::kNotInitialized);

    assert(state.Initialize(
        endpoint,
        std::uint16_t{777u},
        12u,
        start + 10s));
    assert(state.initialized());
    assert(state.remote_endpoint() == endpoint);
    assert(state.connect_qport().has_value());
    assert(*state.connect_qport() == 777u);
    assert(state.channel_identifier() == 12u);
    assert(state.outgoing_sequence() == 1u);
    assert(state.highest_sequence_sent() == 0u);
    assert(state.incoming_sequence() == 0u);
    assert(state.highest_accepted_acknowledgement() == 0u);
    assert(!state.has_accepted_activity());
    assert(state.last_accepted_at() == GoldSrcNetchanState::TimePoint{});
    assert(state.transport_phase() == GoldSrcNetchanTransportPhase::kNone);
    AssertDiagnosticsAreZero(state.diagnostics());

    const GoldSrcNetchanPacket first = Packet(1u, 0u, true, false, {nop});
    assert(
        state.ProcessIncomingDatagram(endpoint, first, start + 11s)
        == GoldSrcNetchanProcessResult::kAccepted);
    assert(state.incoming_sequence() == 1u);
    assert(state.highest_accepted_acknowledgement() == 0u);
    assert(state.incoming_reliable_sequence());
    assert(!state.incoming_reliable_acknowledgement());
    assert(state.has_accepted_activity());
    assert(state.last_accepted_at() == start + 11s);

    const StateSnapshot before_duplicate = Snapshot(state);
    assert(
        state.ProcessIncomingDatagram(endpoint, first, start + 12s)
        == GoldSrcNetchanProcessResult::kDuplicateSequence);
    AssertMutationStateEquals(before_duplicate, state);

    const StateSnapshot before_out_of_order = Snapshot(state);
    assert(
        state.ProcessIncomingDatagram(
            endpoint,
            Packet(0u, 0u, false, false, {nop}),
            start + 13s)
        == GoldSrcNetchanProcessResult::kOutOfOrderSequence);
    AssertMutationStateEquals(before_out_of_order, state);

    assert(
        state.ProcessIncomingDatagram(
            endpoint,
            Packet(4u, 0u, false, false, {nop, nop}),
            start + 14s)
        == GoldSrcNetchanProcessResult::kAccepted);
    assert(state.incoming_sequence() == 4u);
    assert(state.last_accepted_at() == start + 14s);
    assert(state.diagnostics().sequence_gaps == 1u);
    assert(state.diagnostics().dropped_incoming_packets == 2u);

    const StateSnapshot before_unsupported_payload = Snapshot(state);
    assert(
        state.ProcessIncomingDatagram(
            endpoint,
            Packet(5u, 0u, false, false, {0x02u}),
            start + 15s)
        == GoldSrcNetchanProcessResult::kUnsupportedPayload);
    AssertMutationStateEquals(before_unsupported_payload, state);

    const StateSnapshot before_endpoint_mismatch = Snapshot(state);
    assert(
        state.ProcessIncomingDatagram(
            other_endpoint,
            Packet(5u, 0u),
            start + 16s)
        == GoldSrcNetchanProcessResult::kEndpointMismatch);
    AssertMutationStateEquals(before_endpoint_mismatch, state);

    const StateSnapshot before_fragment_record = Snapshot(state);
    state.RecordRejected(GoldSrcNetchanProcessResult::kUnsupportedFragment);
    AssertMutationStateEquals(before_fragment_record, state);
    state.RecordRejected(GoldSrcNetchanProcessResult::kMalformedHeader);
    AssertMutationStateEquals(before_fragment_record, state);

    const GoldSrcNetchanDiagnostics& diagnostics = state.diagnostics();
    assert(diagnostics.sequenced_received == 6u);
    assert(diagnostics.accepted == 2u);
    assert(diagnostics.duplicate_sequence == 1u);
    assert(diagnostics.out_of_order_sequence == 1u);
    assert(diagnostics.unsupported_payload == 1u);
    assert(diagnostics.endpoint_mismatch == 1u);
    assert(diagnostics.unsupported_fragment == 1u);
    assert(diagnostics.malformed_header == 1u);
}

void TestStaleAndFutureAcknowledgementsAreAtomic()
{
    using namespace std::chrono_literals;
    const Ipv4Endpoint endpoint{{192u, 0u, 2u, 10u}, 27015u};
    const GoldSrcNetchanState::TimePoint start{};
    GoldSrcNetchanState state;
    assert(state.Initialize(endpoint, std::nullopt, 0u, start));

    GoldSrcNetchanDatagram first_outgoing{};
    GoldSrcNetchanDatagram second_outgoing{};
    assert(state.BuildOutgoingDatagram(&first_outgoing));
    assert(state.BuildOutgoingDatagram(&second_outgoing));
    assert(state.highest_sequence_sent() == 2u);
    assert(state.outgoing_sequence() == 3u);

    const StateSnapshot before_future = Snapshot(state);
    assert(
        state.ProcessIncomingDatagram(
            endpoint,
            Packet(1u, 3u),
            start + 1s)
        == GoldSrcNetchanProcessResult::kFutureAck);
    AssertMutationStateEquals(before_future, state);
    assert(!state.has_accepted_activity());

    assert(
        state.ProcessIncomingDatagram(
            endpoint,
            Packet(1u, 2u),
            start + 2s)
        == GoldSrcNetchanProcessResult::kAccepted);
    assert(state.incoming_sequence() == 1u);
    assert(state.highest_accepted_acknowledgement() == 2u);
    assert(state.last_accepted_at() == start + 2s);

    const StateSnapshot before_stale = Snapshot(state);
    assert(
        state.ProcessIncomingDatagram(
            endpoint,
            Packet(2u, 1u),
            start + 3s)
        == GoldSrcNetchanProcessResult::kStaleAck);
    AssertMutationStateEquals(before_stale, state);
    assert(state.highest_accepted_acknowledgement() == 2u);

    assert(
        state.ProcessIncomingDatagram(
            endpoint,
            Packet(2u, 2u),
            start + 4s)
        == GoldSrcNetchanProcessResult::kAccepted);
    assert(state.highest_accepted_acknowledgement() == 2u);

    const StateSnapshot before_zero_ack = Snapshot(state);
    assert(
        state.ProcessIncomingDatagram(
            endpoint,
            Packet(3u, 0u),
            start + 5s)
        == GoldSrcNetchanProcessResult::kStaleAck);
    AssertMutationStateEquals(before_zero_ack, state);
    assert(state.highest_accepted_acknowledgement() == 2u);
    assert(state.last_accepted_at() == start + 4s);

    assert(state.diagnostics().future_ack == 1u);
    assert(state.diagnostics().stale_ack == 2u);
    assert(state.diagnostics().accepted == 2u);
}

void TestReliableQueueBoundsAndSecondQueue()
{
    const Ipv4Endpoint endpoint{{203u, 0u, 113u, 7u}, 27015u};
    const GoldSrcNetchanState::TimePoint start{};
    const std::array<std::uint8_t, 3> original = {0x01u, 0x02u, 0x03u};
    const std::array<std::uint8_t, 1> second = {0x04u};

    GoldSrcNetchanState state;
    assert(
        state.QueueReliablePayload(original.data(), original.size())
        == GoldSrcNetchanQueueResult::kNotInitialized);
    assert(state.Initialize(endpoint, std::uint16_t{999u}, 1u, start));
    assert(
        state.QueueReliablePayload(nullptr, 1u)
        == GoldSrcNetchanQueueResult::kEmptyPayload);
    assert(
        state.QueueReliablePayload(original.data(), 0u)
        == GoldSrcNetchanQueueResult::kEmptyPayload);
    assert(
        state.QueueReliablePayload(
            original.data(),
            original.size(),
            GoldSrcNetchanReliablePayloadKind::kNone)
        == GoldSrcNetchanQueueResult::kInvalidPayloadKind);

    std::vector<std::uint8_t> too_large(
        kGoldSrcMaximumFragmentTransferBytes + 1u,
        0x01u);
    assert(
        state.QueueReliablePayload(too_large.data(), too_large.size())
        == GoldSrcNetchanQueueResult::kPayloadTooLarge);
    assert(!state.reliable_pending());
    assert(!state.local_reliable_sequence());

    GoldSrcNetchanState fragmented_state;
    assert(fragmented_state.Initialize(endpoint, std::nullopt, 3u, start));
    std::vector<std::uint8_t> fragmented(
        kGoldSrcNetchanMaximumReliableBytes + 1u,
        0x01u);
    assert(
        fragmented_state.QueueReliablePayload(
            fragmented.data(),
            fragmented.size())
        == GoldSrcNetchanQueueResult::kQueued);
    assert(!fragmented_state.reliable_pending());
    assert(fragmented_state.fragment_sender().active());
    assert(fragmented_state.next_datagram_will_include_reliable());
    assert(
        fragmented_state.QueueReliablePayload(second.data(), second.size())
        == GoldSrcNetchanQueueResult::kReliableAlreadyPending);

    assert(
        state.QueueReliablePayload(original.data(), original.size())
        == GoldSrcNetchanQueueResult::kQueued);
    assert(state.reliable_pending());
    assert(state.reliable_pending_bytes() == original.size());
    assert(
        state.pending_reliable_kind()
        == GoldSrcNetchanReliablePayloadKind::kTransportBootstrap);
    assert(state.local_reliable_sequence());
    assert(state.transport_phase()
        == GoldSrcNetchanTransportPhase::kAwaitingFirstReliableAck);
    assert(std::equal(
        original.begin(),
        original.end(),
        state.reliable_payload_data()));

    const StateSnapshot before_second_queue = Snapshot(state);
    assert(
        state.QueueReliablePayload(second.data(), second.size())
        == GoldSrcNetchanQueueResult::kReliableAlreadyPending);
    AssertMutationStateEquals(before_second_queue, state);
    assert(state.diagnostics().reliable_queued == 1u);

    GoldSrcNetchanState maximum_state;
    assert(maximum_state.Initialize(endpoint, std::nullopt, 2u, start));
    std::vector<std::uint8_t> maximum(
        kGoldSrcNetchanMaximumReliableBytes,
        kGoldSrcServerNop);
    assert(
        maximum_state.QueueReliablePayload(maximum.data(), maximum.size())
        == GoldSrcNetchanQueueResult::kQueued);
    assert(
        maximum_state.reliable_pending_bytes()
        == kGoldSrcNetchanMaximumReliableBytes);
}

void TestFirstReliableSendAndCorrectAcknowledgement()
{
    using namespace std::chrono_literals;
    const Ipv4Endpoint endpoint{{127u, 0u, 0u, 1u}, 28000u};
    const GoldSrcNetchanState::TimePoint start{};
    GoldSrcNetchanState state;
    assert(state.Initialize(endpoint, std::uint16_t{777u}, 0u, start));

    const std::uint8_t nop = kGoldSrcServerNop;
    assert(
        state.QueueReliablePayload(&nop, 1u)
        == GoldSrcNetchanQueueResult::kQueued);
    assert(state.local_reliable_sequence());

    GoldSrcNetchanDatagram first{};
    assert(state.BuildOutgoingDatagram(&first));
    const std::vector<std::uint8_t> expected = {
        0x01u, 0x00u, 0x00u, 0x80u,
        0x00u, 0x00u, 0x00u, 0x00u,
        0x5Au, 0x19u, 0x01u, 0x00u,
        0x1Au, 0x01u, 0x11u, 0x40u,
    };
    assert(DatagramBytes(first) == expected);
    assert(state.outgoing_sequence() == 2u);
    assert(state.highest_sequence_sent() == 1u);
    assert(state.last_reliable_sequence() == 1u);
    assert(state.reliable_pending());
    assert(state.reliable_pending_bytes() == 1u);
    assert(state.diagnostics().reliable_sent == 1u);
    assert(state.diagnostics().reliable_resent == 0u);

    assert(
        state.ProcessIncomingDatagram(
            endpoint,
            Packet(1u, 1u, false, true),
            start + 1s)
        == GoldSrcNetchanProcessResult::kAccepted);
    assert(!state.reliable_pending());
    assert(state.reliable_pending_bytes() == 0u);
    for (std::size_t index = 0;
         index < kGoldSrcNetchanMaximumReliableBytes;
         ++index)
    {
        assert(state.reliable_payload_data()[index] == 0u);
    }
    assert(state.local_reliable_sequence());
    assert(state.highest_accepted_acknowledgement() == 1u);
    assert(state.transport_phase() == GoldSrcNetchanTransportPhase::kEstablished);
    assert(
        state.last_acknowledged_reliable_kind()
        == GoldSrcNetchanReliablePayloadKind::kTransportBootstrap);
    assert(state.reliable_acknowledgement_generation() == 1u);
    assert(state.diagnostics().reliable_acked == 1u);
    assert(state.diagnostics().reliable_ack_mismatch == 0u);
}

void TestServerInfoReliableKindAckMetadataAndEstablishedPhase()
{
    using namespace std::chrono_literals;
    const Ipv4Endpoint endpoint{{127u, 0u, 0u, 1u}, 28003u};
    const GoldSrcNetchanState::TimePoint start{};
    GoldSrcNetchanState state;
    assert(state.Initialize(endpoint, std::uint16_t{780u}, 0u, start));

    const std::uint8_t nop = kGoldSrcServerNop;
    assert(
        state.QueueReliablePayload(&nop, 1u)
        == GoldSrcNetchanQueueResult::kQueued);
    assert(
        state.pending_reliable_kind()
        == GoldSrcNetchanReliablePayloadKind::kTransportBootstrap);
    GoldSrcNetchanDatagram bootstrap{};
    assert(state.BuildOutgoingDatagram(&bootstrap));

    const GoldSrcNetchanProcessOutcome bootstrap_ack =
        state.ProcessIncomingDatagramDetailed(
            endpoint,
            Packet(1u, 1u, false, state.local_reliable_sequence()),
            start + 1s);
    assert(bootstrap_ack.result == GoldSrcNetchanProcessResult::kAccepted);
    assert(bootstrap_ack.reliable_payload_was_acknowledged());
    assert(
        bootstrap_ack.acknowledged_reliable_kind
        == GoldSrcNetchanReliablePayloadKind::kTransportBootstrap);
    assert(bootstrap_ack.reliable_acknowledgement_generation == 1u);
    assert(state.reliable_acknowledgement_generation() == 1u);
    assert(state.transport_phase() == GoldSrcNetchanTransportPhase::kEstablished);

    const std::array<std::uint8_t, 4> server_info = {
        0x0Bu,
        0x30u,
        0x00u,
        0x00u,
    };
    assert(
        state.QueueReliablePayload(
            server_info.data(),
            server_info.size(),
            GoldSrcNetchanReliablePayloadKind::kServerInfo)
        == GoldSrcNetchanQueueResult::kQueued);
    assert(
        state.pending_reliable_kind()
        == GoldSrcNetchanReliablePayloadKind::kServerInfo);
    assert(state.transport_phase() == GoldSrcNetchanTransportPhase::kEstablished);
    const bool server_info_toggle = state.local_reliable_sequence();

    GoldSrcNetchanDatagram server_info_datagram{};
    assert(state.BuildOutgoingDatagram(&server_info_datagram));
    const GoldSrcNetchanDecodeResult server_info_decoded =
        DecodeGoldSrcNetchanDatagram(
            GoldSrcNetchanDirection::kServerToClient,
            server_info_datagram.bytes.data(),
            server_info_datagram.size);
    assert(server_info_decoded.ok());
    assert(server_info_decoded.packet.sequence == 2u);
    assert(server_info_decoded.packet.reliable_present);
    assert(std::equal(
        server_info.begin(),
        server_info.end(),
        server_info_decoded.packet.payload.begin()));

    const GoldSrcNetchanProcessOutcome future_ack =
        state.ProcessIncomingDatagramDetailed(
            endpoint,
            Packet(2u, 3u, false, server_info_toggle),
            start + 2s);
    assert(future_ack.result == GoldSrcNetchanProcessResult::kFutureAck);
    assert(!future_ack.reliable_payload_was_acknowledged());
    assert(future_ack.reliable_acknowledgement_generation == 0u);
    assert(state.reliable_pending());
    assert(state.reliable_acknowledgement_generation() == 1u);
    assert(state.transport_phase() == GoldSrcNetchanTransportPhase::kEstablished);

    const GoldSrcNetchanProcessOutcome wrong_toggle =
        state.ProcessIncomingDatagramDetailed(
            endpoint,
            Packet(2u, 2u, false, !server_info_toggle),
            start + 3s);
    assert(wrong_toggle.result == GoldSrcNetchanProcessResult::kAccepted);
    assert(!wrong_toggle.reliable_payload_was_acknowledged());
    assert(wrong_toggle.reliable_acknowledgement_generation == 0u);
    assert(state.reliable_pending());
    assert(
        state.pending_reliable_kind()
        == GoldSrcNetchanReliablePayloadKind::kServerInfo);
    assert(state.reliable_acknowledgement_generation() == 1u);
    assert(state.transport_phase() == GoldSrcNetchanTransportPhase::kEstablished);

    const GoldSrcNetchanProcessOutcome stale_ack =
        state.ProcessIncomingDatagramDetailed(
            endpoint,
            Packet(3u, 1u, false, server_info_toggle),
            start + 4s);
    assert(stale_ack.result == GoldSrcNetchanProcessResult::kStaleAck);
    assert(!stale_ack.reliable_payload_was_acknowledged());
    assert(stale_ack.reliable_acknowledgement_generation == 0u);
    assert(state.reliable_pending());
    assert(state.reliable_acknowledgement_generation() == 1u);

    GoldSrcNetchanDatagram ordinary_followup{};
    assert(state.BuildOutgoingDatagram(&ordinary_followup));
    const GoldSrcNetchanDecodeResult ordinary_followup_decoded =
        DecodeGoldSrcNetchanDatagram(
            GoldSrcNetchanDirection::kServerToClient,
            ordinary_followup.bytes.data(),
            ordinary_followup.size);
    assert(ordinary_followup_decoded.ok());
    assert(ordinary_followup_decoded.packet.sequence == 3u);
    assert(!ordinary_followup_decoded.packet.reliable_present);

    const GoldSrcNetchanProcessOutcome second_wrong_toggle =
        state.ProcessIncomingDatagramDetailed(
            endpoint,
            Packet(3u, 3u, false, !server_info_toggle),
            start + 5s);
    assert(second_wrong_toggle.result == GoldSrcNetchanProcessResult::kAccepted);
    assert(!second_wrong_toggle.reliable_payload_was_acknowledged());
    assert(state.reliable_pending());
    assert(
        state.pending_reliable_kind()
        == GoldSrcNetchanReliablePayloadKind::kServerInfo);

    GoldSrcNetchanDatagram retransmission{};
    assert(state.BuildOutgoingDatagram(&retransmission));
    const GoldSrcNetchanDecodeResult retransmission_decoded =
        DecodeGoldSrcNetchanDatagram(
            GoldSrcNetchanDirection::kServerToClient,
            retransmission.bytes.data(),
            retransmission.size);
    assert(retransmission_decoded.ok());
    assert(retransmission_decoded.packet.sequence == 4u);
    assert(retransmission_decoded.packet.reliable_present);
    assert(std::equal(
        server_info.begin(),
        server_info.end(),
        retransmission_decoded.packet.payload.begin()));
    assert(state.diagnostics().reliable_resent == 1u);

    const GoldSrcNetchanProcessOutcome ack_before_retransmission =
        state.ProcessIncomingDatagramDetailed(
            endpoint,
            Packet(4u, 3u, false, server_info_toggle),
            start + 6s);
    assert(
        ack_before_retransmission.result
        == GoldSrcNetchanProcessResult::kAccepted);
    assert(!ack_before_retransmission.reliable_payload_was_acknowledged());
    assert(state.reliable_pending());
    assert(state.reliable_acknowledgement_generation() == 1u);

    const GoldSrcNetchanProcessOutcome correct_ack =
        state.ProcessIncomingDatagramDetailed(
            endpoint,
            Packet(5u, 4u, false, server_info_toggle),
            start + 7s);
    assert(correct_ack.result == GoldSrcNetchanProcessResult::kAccepted);
    assert(correct_ack.reliable_payload_was_acknowledged());
    assert(
        correct_ack.acknowledged_reliable_kind
        == GoldSrcNetchanReliablePayloadKind::kServerInfo);
    assert(correct_ack.reliable_acknowledgement_generation == 2u);
    assert(!state.reliable_pending());
    assert(
        state.pending_reliable_kind()
        == GoldSrcNetchanReliablePayloadKind::kNone);
    assert(
        state.last_acknowledged_reliable_kind()
        == GoldSrcNetchanReliablePayloadKind::kServerInfo);
    assert(state.reliable_acknowledgement_generation() == 2u);
    assert(state.transport_phase() == GoldSrcNetchanTransportPhase::kEstablished);
    assert(state.diagnostics().reliable_acked == 2u);
    assert(state.diagnostics().reliable_ack_mismatch == 2u);

    state.Reset();
    AssertCleanState(state);
}

void TestResourceManifestReliableKindAckRetransmitAndReset()
{
    using namespace std::chrono_literals;
    const Ipv4Endpoint endpoint{{127u, 0u, 0u, 1u}, 28005u};
    const GoldSrcNetchanState::TimePoint start{};
    GoldSrcNetchanState state;
    assert(state.Initialize(endpoint, std::uint16_t{781u}, 0u, start));
    assert(
        NameFor(GoldSrcNetchanReliablePayloadKind::kResourceManifest)
        == "resource_manifest");

    const std::uint8_t nop = kGoldSrcServerNop;
    assert(
        state.QueueReliablePayload(&nop, 1u)
        == GoldSrcNetchanQueueResult::kQueued);
    GoldSrcNetchanDatagram bootstrap{};
    assert(state.BuildOutgoingDatagram(&bootstrap));
    const GoldSrcNetchanProcessOutcome bootstrap_ack =
        state.ProcessIncomingDatagramDetailed(
            endpoint,
            Packet(1u, 1u, false, state.local_reliable_sequence()),
            start + 1s);
    assert(bootstrap_ack.result == GoldSrcNetchanProcessResult::kAccepted);
    assert(
        bootstrap_ack.acknowledged_reliable_kind
        == GoldSrcNetchanReliablePayloadKind::kTransportBootstrap);
    assert(state.transport_phase() == GoldSrcNetchanTransportPhase::kEstablished);

    std::array<std::uint8_t, 5> resource_manifest = {
        0x2Bu, 0x01u, 0x02u, 0x03u, 0x04u,
    };
    const std::array<std::uint8_t, 5> frozen_manifest = resource_manifest;
    assert(
        state.QueueReliablePayload(
            resource_manifest.data(),
            resource_manifest.size(),
            GoldSrcNetchanReliablePayloadKind::kResourceManifest)
        == GoldSrcNetchanQueueResult::kQueued);
    assert(
        state.pending_reliable_kind()
        == GoldSrcNetchanReliablePayloadKind::kResourceManifest);
    const bool resource_toggle = state.local_reliable_sequence();

    resource_manifest.fill(0xFFu);
    assert(std::equal(
        frozen_manifest.begin(),
        frozen_manifest.end(),
        state.reliable_payload_data()));

    GoldSrcNetchanDatagram first_manifest{};
    assert(state.BuildOutgoingDatagram(&first_manifest));
    const GoldSrcNetchanDecodeResult first_decoded =
        DecodeGoldSrcNetchanDatagram(
            GoldSrcNetchanDirection::kServerToClient,
            first_manifest.bytes.data(),
            first_manifest.size);
    assert(first_decoded.ok());
    assert(first_decoded.packet.sequence == 2u);
    assert(first_decoded.packet.reliable_present);
    assert(std::equal(
        frozen_manifest.begin(),
        frozen_manifest.end(),
        first_decoded.packet.payload.begin()));

    const GoldSrcNetchanProcessOutcome wrong_toggle =
        state.ProcessIncomingDatagramDetailed(
            endpoint,
            Packet(2u, 2u, false, !resource_toggle),
            start + 2s);
    assert(wrong_toggle.result == GoldSrcNetchanProcessResult::kAccepted);
    assert(!wrong_toggle.reliable_payload_was_acknowledged());
    assert(state.reliable_pending());
    assert(
        state.pending_reliable_kind()
        == GoldSrcNetchanReliablePayloadKind::kResourceManifest);

    GoldSrcNetchanDatagram ordinary_followup{};
    assert(state.BuildOutgoingDatagram(&ordinary_followup));
    const GoldSrcNetchanDecodeResult ordinary_decoded =
        DecodeGoldSrcNetchanDatagram(
            GoldSrcNetchanDirection::kServerToClient,
            ordinary_followup.bytes.data(),
            ordinary_followup.size);
    assert(ordinary_decoded.ok());
    assert(ordinary_decoded.packet.sequence == 3u);
    assert(!ordinary_decoded.packet.reliable_present);

    const GoldSrcNetchanProcessOutcome covering_wrong_toggle =
        state.ProcessIncomingDatagramDetailed(
            endpoint,
            Packet(3u, 3u, false, !resource_toggle),
            start + 3s);
    assert(
        covering_wrong_toggle.result
        == GoldSrcNetchanProcessResult::kAccepted);
    assert(!covering_wrong_toggle.reliable_payload_was_acknowledged());
    assert(state.reliable_pending());

    GoldSrcNetchanDatagram retransmission{};
    assert(state.BuildOutgoingDatagram(&retransmission));
    const GoldSrcNetchanDecodeResult retransmission_decoded =
        DecodeGoldSrcNetchanDatagram(
            GoldSrcNetchanDirection::kServerToClient,
            retransmission.bytes.data(),
            retransmission.size);
    assert(retransmission_decoded.ok());
    assert(retransmission_decoded.packet.sequence == 4u);
    assert(retransmission_decoded.packet.reliable_present);
    assert(std::equal(
        frozen_manifest.begin(),
        frozen_manifest.end(),
        retransmission_decoded.packet.payload.begin()));
    assert(state.diagnostics().reliable_resent == 1u);

    const GoldSrcNetchanProcessOutcome non_covering_ack =
        state.ProcessIncomingDatagramDetailed(
            endpoint,
            Packet(4u, 3u, false, resource_toggle),
            start + 4s);
    assert(
        non_covering_ack.result
        == GoldSrcNetchanProcessResult::kAccepted);
    assert(!non_covering_ack.reliable_payload_was_acknowledged());
    assert(state.reliable_pending());
    assert(
        state.pending_reliable_kind()
        == GoldSrcNetchanReliablePayloadKind::kResourceManifest);

    const GoldSrcNetchanProcessOutcome correct_ack =
        state.ProcessIncomingDatagramDetailed(
            endpoint,
            Packet(5u, 4u, false, resource_toggle),
            start + 5s);
    assert(correct_ack.result == GoldSrcNetchanProcessResult::kAccepted);
    assert(correct_ack.reliable_payload_was_acknowledged());
    assert(
        correct_ack.acknowledged_reliable_kind
        == GoldSrcNetchanReliablePayloadKind::kResourceManifest);
    assert(correct_ack.reliable_acknowledgement_generation == 2u);
    assert(!state.reliable_pending());
    assert(
        state.pending_reliable_kind()
        == GoldSrcNetchanReliablePayloadKind::kNone);
    assert(
        state.last_acknowledged_reliable_kind()
        == GoldSrcNetchanReliablePayloadKind::kResourceManifest);
    assert(state.reliable_acknowledgement_generation() == 2u);
    assert(state.transport_phase() == GoldSrcNetchanTransportPhase::kEstablished);
    assert(state.diagnostics().reliable_acked == 2u);
    assert(state.diagnostics().reliable_ack_mismatch == 2u);

    state.Reset();
    AssertCleanState(state);
}

void TestSignonBootstrapReliableKindUnfragmentedAndFragmented()
{
    using namespace std::chrono_literals;
    const Ipv4Endpoint endpoint{{127u, 0u, 0u, 1u}, 28006u};
    const Ipv4Endpoint reused_endpoint{{127u, 0u, 0u, 1u}, 28007u};
    const GoldSrcNetchanState::TimePoint start{};
    GoldSrcNetchanState state;
    assert(state.Initialize(endpoint, std::uint16_t{782u}, 0u, start));
    assert(
        NameFor(GoldSrcNetchanReliablePayloadKind::kSignonBootstrap)
        == "signon_bootstrap");

    std::array<std::uint8_t, 8> bootstrap = {
        0x0Bu, 0x36u, 0x0Eu, 0x75u, 0x73u, 0x65u, 0x72u, 0x00u,
    };
    const std::array<std::uint8_t, 8> frozen_bootstrap = bootstrap;
    assert(
        state.QueueReliablePayload(
            bootstrap.data(),
            bootstrap.size(),
            GoldSrcNetchanReliablePayloadKind::kSignonBootstrap)
        == GoldSrcNetchanQueueResult::kQueued);
    assert(
        state.pending_reliable_kind()
        == GoldSrcNetchanReliablePayloadKind::kSignonBootstrap);
    assert(state.reliable_pending_bytes() == frozen_bootstrap.size());
    const bool bootstrap_toggle = state.local_reliable_sequence();
    bootstrap.fill(0xFFu);
    assert(std::equal(
        frozen_bootstrap.begin(),
        frozen_bootstrap.end(),
        state.reliable_payload_data()));

    GoldSrcNetchanDatagram first_bootstrap{};
    assert(state.BuildOutgoingDatagram(&first_bootstrap));
    const GoldSrcNetchanDecodeResult first_bootstrap_decoded =
        DecodeGoldSrcNetchanDatagram(
            GoldSrcNetchanDirection::kServerToClient,
            first_bootstrap.bytes.data(),
            first_bootstrap.size);
    assert(first_bootstrap_decoded.ok());
    assert(first_bootstrap_decoded.packet.sequence == 1u);
    assert(first_bootstrap_decoded.packet.reliable_present);
    assert(!first_bootstrap_decoded.packet.fragment_present);
    assert(
        first_bootstrap_decoded.packet.payload_size
        == frozen_bootstrap.size());
    assert(std::equal(
        frozen_bootstrap.begin(),
        frozen_bootstrap.end(),
        first_bootstrap_decoded.packet.payload.begin()));

    const GoldSrcNetchanProcessOutcome wrong_bootstrap_toggle =
        state.ProcessIncomingDatagramDetailed(
            endpoint,
            Packet(1u, 1u, false, !bootstrap_toggle),
            start + 1ms);
    assert(
        wrong_bootstrap_toggle.result
        == GoldSrcNetchanProcessResult::kAccepted);
    assert(!wrong_bootstrap_toggle.reliable_payload_was_acknowledged());
    assert(state.reliable_pending());

    GoldSrcNetchanDatagram bootstrap_followup{};
    assert(state.BuildOutgoingDatagram(&bootstrap_followup));
    const GoldSrcNetchanDecodeResult bootstrap_followup_decoded =
        DecodeGoldSrcNetchanDatagram(
            GoldSrcNetchanDirection::kServerToClient,
            bootstrap_followup.bytes.data(),
            bootstrap_followup.size);
    assert(bootstrap_followup_decoded.ok());
    assert(bootstrap_followup_decoded.packet.sequence == 2u);
    assert(!bootstrap_followup_decoded.packet.reliable_present);
    assert(!bootstrap_followup_decoded.packet.fragment_present);

    const GoldSrcNetchanProcessOutcome covering_wrong_bootstrap_toggle =
        state.ProcessIncomingDatagramDetailed(
            endpoint,
            Packet(2u, 2u, false, !bootstrap_toggle),
            start + 2ms);
    assert(
        covering_wrong_bootstrap_toggle.result
        == GoldSrcNetchanProcessResult::kAccepted);
    assert(
        !covering_wrong_bootstrap_toggle
             .reliable_payload_was_acknowledged());
    assert(state.reliable_pending());

    GoldSrcNetchanDatagram bootstrap_retransmission{};
    assert(state.BuildOutgoingDatagram(&bootstrap_retransmission));
    const GoldSrcNetchanDecodeResult bootstrap_retransmission_decoded =
        DecodeGoldSrcNetchanDatagram(
            GoldSrcNetchanDirection::kServerToClient,
            bootstrap_retransmission.bytes.data(),
            bootstrap_retransmission.size);
    assert(bootstrap_retransmission_decoded.ok());
    assert(bootstrap_retransmission_decoded.packet.sequence == 3u);
    assert(bootstrap_retransmission_decoded.packet.reliable_present);
    assert(!bootstrap_retransmission_decoded.packet.fragment_present);
    assert(
        bootstrap_retransmission_decoded.packet.payload_size
        == first_bootstrap_decoded.packet.payload_size);
    assert(std::equal(
        first_bootstrap_decoded.packet.payload.begin(),
        first_bootstrap_decoded.packet.payload.begin()
            + static_cast<std::ptrdiff_t>(
                first_bootstrap_decoded.packet.payload_size),
        bootstrap_retransmission_decoded.packet.payload.begin()));
    assert(state.local_reliable_sequence() == bootstrap_toggle);
    assert(state.diagnostics().reliable_resent == 1u);

    const GoldSrcNetchanProcessOutcome non_covering_bootstrap_ack =
        state.ProcessIncomingDatagramDetailed(
            endpoint,
            Packet(3u, 2u, false, bootstrap_toggle),
            start + 3ms);
    assert(
        non_covering_bootstrap_ack.result
        == GoldSrcNetchanProcessResult::kAccepted);
    assert(!non_covering_bootstrap_ack.reliable_payload_was_acknowledged());
    assert(state.reliable_pending());
    assert(state.reliable_acknowledgement_generation() == 0u);

    const GoldSrcNetchanProcessOutcome bootstrap_ack =
        state.ProcessIncomingDatagramDetailed(
            endpoint,
            Packet(4u, 3u, false, bootstrap_toggle),
            start + 4ms);
    assert(bootstrap_ack.result == GoldSrcNetchanProcessResult::kAccepted);
    assert(bootstrap_ack.reliable_payload_was_acknowledged());
    assert(
        bootstrap_ack.acknowledged_reliable_kind
        == GoldSrcNetchanReliablePayloadKind::kSignonBootstrap);
    assert(bootstrap_ack.reliable_acknowledgement_generation == 1u);
    assert(!state.reliable_pending());
    assert(
        state.last_acknowledged_reliable_kind()
        == GoldSrcNetchanReliablePayloadKind::kSignonBootstrap);
    assert(state.reliable_acknowledgement_generation() == 1u);
    assert(state.diagnostics().reliable_queued == 1u);
    assert(state.diagnostics().reliable_sent == 1u);
    assert(state.diagnostics().reliable_resent == 1u);
    assert(state.diagnostics().reliable_acked == 1u);
    assert(state.diagnostics().reliable_ack_mismatch == 2u);

    state.Reset();
    AssertCleanState(state);

    assert(
        state.Initialize(
            endpoint,
            std::uint16_t{782u},
            0u,
            start + 10ms));
    std::vector<std::uint8_t> fragmented_bootstrap(
        kGoldSrcNetchanMaximumReliableBytes + 137u);
    for (std::size_t index = 0u;
         index < fragmented_bootstrap.size();
         ++index)
    {
        fragmented_bootstrap[index] =
            static_cast<std::uint8_t>((index * 29u + 7u) & 0xFFu);
    }
    const std::vector<std::uint8_t> frozen_fragmented_bootstrap =
        fragmented_bootstrap;
    assert(
        state.QueueReliablePayload(
            fragmented_bootstrap.data(),
            fragmented_bootstrap.size(),
            GoldSrcNetchanReliablePayloadKind::kSignonBootstrap)
        == GoldSrcNetchanQueueResult::kQueued);
    assert(!state.reliable_pending());
    assert(state.fragment_sender().active());
    assert(state.fragment_sender().fragment_count() == 2u);
    assert(
        state.fragment_sender().total_payload_size()
        == frozen_fragmented_bootstrap.size());
    fragmented_bootstrap.assign(fragmented_bootstrap.size(), 0xEEu);

    GoldSrcNetchanDatagram first_fragment{};
    assert(state.BuildOutgoingDatagram(&first_fragment));
    const GoldSrcNetchanDecodeResult first_fragment_decoded =
        DecodeGoldSrcNetchanDatagram(
            GoldSrcNetchanDirection::kServerToClient,
            first_fragment.bytes.data(),
            first_fragment.size);
    assert(first_fragment_decoded.ok());
    assert(first_fragment_decoded.packet.sequence == 1u);
    assert(first_fragment_decoded.packet.reliable_present);
    assert(first_fragment_decoded.packet.fragment_present);
    assert(state.reliable_pending());
    assert(
        first_fragment_decoded.packet.fragment_metadata
            .descriptors[0]
            .fragment_index
        == 1u);
    assert(
        first_fragment_decoded.packet.fragment_metadata
            .descriptors[0]
            .fragment_count
        == 2u);
    assert(
        first_fragment_decoded.packet.payload_size
        == kGoldSrcMaximumFragmentBytes);
    assert(std::equal(
        frozen_fragmented_bootstrap.begin(),
        frozen_fragmented_bootstrap.begin()
            + static_cast<std::ptrdiff_t>(
                kGoldSrcMaximumFragmentBytes),
        first_fragment_decoded.packet.payload.begin()));
    assert(
        state.pending_reliable_kind()
        == GoldSrcNetchanReliablePayloadKind::kSignonBootstrap);
    const bool first_fragment_toggle = state.local_reliable_sequence();
    const GoldSrcFragmentDescriptor first_fragment_descriptor =
        first_fragment_decoded.packet.fragment_metadata.descriptors[0];

    const GoldSrcNetchanProcessOutcome wrong_first_fragment_toggle =
        state.ProcessIncomingDatagramDetailed(
            endpoint,
            Packet(1u, 1u, false, !first_fragment_toggle),
            start + 11ms);
    assert(
        wrong_first_fragment_toggle.result
        == GoldSrcNetchanProcessResult::kAccepted);
    assert(
        !wrong_first_fragment_toggle
             .reliable_payload_was_acknowledged());
    assert(state.fragment_sender().current_fragment_index() == 0u);
    assert(state.fragment_sender().awaiting_acknowledgement());

    GoldSrcNetchanDatagram first_fragment_followup{};
    assert(state.BuildOutgoingDatagram(&first_fragment_followup));
    const GoldSrcNetchanDecodeResult first_fragment_followup_decoded =
        DecodeGoldSrcNetchanDatagram(
            GoldSrcNetchanDirection::kServerToClient,
            first_fragment_followup.bytes.data(),
            first_fragment_followup.size);
    assert(first_fragment_followup_decoded.ok());
    assert(first_fragment_followup_decoded.packet.sequence == 2u);
    assert(!first_fragment_followup_decoded.packet.reliable_present);
    assert(!first_fragment_followup_decoded.packet.fragment_present);

    const GoldSrcNetchanProcessOutcome
        covering_wrong_first_fragment_toggle =
            state.ProcessIncomingDatagramDetailed(
                endpoint,
                Packet(2u, 2u, false, !first_fragment_toggle),
                start + 12ms);
    assert(
        covering_wrong_first_fragment_toggle.result
        == GoldSrcNetchanProcessResult::kAccepted);
    assert(
        !covering_wrong_first_fragment_toggle
             .reliable_payload_was_acknowledged());

    GoldSrcNetchanDatagram first_fragment_retransmission{};
    assert(state.BuildOutgoingDatagram(&first_fragment_retransmission));
    const GoldSrcNetchanDecodeResult first_fragment_retransmission_decoded =
        DecodeGoldSrcNetchanDatagram(
            GoldSrcNetchanDirection::kServerToClient,
            first_fragment_retransmission.bytes.data(),
            first_fragment_retransmission.size);
    assert(first_fragment_retransmission_decoded.ok());
    assert(first_fragment_retransmission_decoded.packet.sequence == 3u);
    assert(first_fragment_retransmission_decoded.packet.reliable_present);
    assert(first_fragment_retransmission_decoded.packet.fragment_present);
    assert(
        first_fragment_retransmission_decoded.packet.fragment_metadata
            .descriptors[0]
            .raw_fragment_id
        == first_fragment_descriptor.raw_fragment_id);
    assert(
        first_fragment_retransmission_decoded.packet.fragment_metadata
            .descriptors[0]
            .payload_offset
        == first_fragment_descriptor.payload_offset);
    assert(
        first_fragment_retransmission_decoded.packet.fragment_metadata
            .descriptors[0]
            .payload_length
        == first_fragment_descriptor.payload_length);
    assert(
        first_fragment_retransmission_decoded.packet.payload_size
        == first_fragment_decoded.packet.payload_size);
    assert(std::equal(
        first_fragment_decoded.packet.payload.begin(),
        first_fragment_decoded.packet.payload.begin()
            + static_cast<std::ptrdiff_t>(
                first_fragment_decoded.packet.payload_size),
        first_fragment_retransmission_decoded.packet.payload.begin()));
    assert(state.local_reliable_sequence() == first_fragment_toggle);
    assert(state.fragment_sender().resend_count() == 1u);

    const GoldSrcNetchanProcessOutcome
        non_covering_first_fragment_ack =
            state.ProcessIncomingDatagramDetailed(
                endpoint,
                Packet(3u, 2u, false, first_fragment_toggle),
                start + 13ms);
    assert(
        non_covering_first_fragment_ack.result
        == GoldSrcNetchanProcessResult::kAccepted);
    assert(
        !non_covering_first_fragment_ack
             .reliable_payload_was_acknowledged());
    assert(state.fragment_sender().current_fragment_index() == 0u);
    assert(state.reliable_acknowledgement_generation() == 0u);

    const GoldSrcNetchanProcessOutcome first_fragment_ack =
        state.ProcessIncomingDatagramDetailed(
            endpoint,
            Packet(4u, 3u, false, first_fragment_toggle),
            start + 14ms);
    assert(
        first_fragment_ack.result
        == GoldSrcNetchanProcessResult::kAccepted);
    assert(!first_fragment_ack.reliable_payload_was_acknowledged());
    assert(
        first_fragment_ack.acknowledged_reliable_kind
        == GoldSrcNetchanReliablePayloadKind::kNone);
    assert(first_fragment_ack.reliable_acknowledgement_generation == 0u);
    assert(state.reliable_acknowledgement_generation() == 0u);
    assert(state.fragment_sender().current_fragment_index() == 1u);
    assert(state.fragment_sender().needs_fragment_staging());

    GoldSrcNetchanDatagram final_fragment{};
    assert(state.BuildOutgoingDatagram(&final_fragment));
    const GoldSrcNetchanDecodeResult final_fragment_decoded =
        DecodeGoldSrcNetchanDatagram(
            GoldSrcNetchanDirection::kServerToClient,
            final_fragment.bytes.data(),
            final_fragment.size);
    assert(final_fragment_decoded.ok());
    assert(final_fragment_decoded.packet.sequence == 4u);
    assert(final_fragment_decoded.packet.reliable_present);
    assert(final_fragment_decoded.packet.fragment_present);
    assert(
        final_fragment_decoded.packet.fragment_metadata
            .descriptors[0]
            .fragment_index
        == 2u);
    assert(
        final_fragment_decoded.packet.fragment_metadata
            .descriptors[0]
            .fragment_count
        == 2u);
    assert(
        final_fragment_decoded.packet.payload_size
        == frozen_fragmented_bootstrap.size()
            - kGoldSrcMaximumFragmentBytes);
    assert(std::equal(
        frozen_fragmented_bootstrap.begin()
            + static_cast<std::ptrdiff_t>(
                kGoldSrcMaximumFragmentBytes),
        frozen_fragmented_bootstrap.end(),
        final_fragment_decoded.packet.payload.begin()));
    const bool final_fragment_toggle = state.local_reliable_sequence();
    assert(final_fragment_toggle != first_fragment_toggle);
    const GoldSrcFragmentDescriptor final_fragment_descriptor =
        final_fragment_decoded.packet.fragment_metadata.descriptors[0];

    const GoldSrcNetchanProcessOutcome wrong_final_fragment_toggle =
        state.ProcessIncomingDatagramDetailed(
            endpoint,
            Packet(5u, 4u, false, !final_fragment_toggle),
            start + 15ms);
    assert(
        wrong_final_fragment_toggle.result
        == GoldSrcNetchanProcessResult::kAccepted);
    assert(
        !wrong_final_fragment_toggle
             .reliable_payload_was_acknowledged());
    assert(!state.fragment_sender().completion_acknowledged());

    GoldSrcNetchanDatagram final_fragment_followup{};
    assert(state.BuildOutgoingDatagram(&final_fragment_followup));
    const GoldSrcNetchanDecodeResult final_fragment_followup_decoded =
        DecodeGoldSrcNetchanDatagram(
            GoldSrcNetchanDirection::kServerToClient,
            final_fragment_followup.bytes.data(),
            final_fragment_followup.size);
    assert(final_fragment_followup_decoded.ok());
    assert(final_fragment_followup_decoded.packet.sequence == 5u);
    assert(!final_fragment_followup_decoded.packet.reliable_present);
    assert(!final_fragment_followup_decoded.packet.fragment_present);

    const GoldSrcNetchanProcessOutcome
        covering_wrong_final_fragment_toggle =
            state.ProcessIncomingDatagramDetailed(
                endpoint,
                Packet(6u, 5u, false, !final_fragment_toggle),
                start + 16ms);
    assert(
        covering_wrong_final_fragment_toggle.result
        == GoldSrcNetchanProcessResult::kAccepted);
    assert(
        !covering_wrong_final_fragment_toggle
             .reliable_payload_was_acknowledged());

    GoldSrcNetchanDatagram final_fragment_retransmission{};
    assert(state.BuildOutgoingDatagram(&final_fragment_retransmission));
    const GoldSrcNetchanDecodeResult final_fragment_retransmission_decoded =
        DecodeGoldSrcNetchanDatagram(
            GoldSrcNetchanDirection::kServerToClient,
            final_fragment_retransmission.bytes.data(),
            final_fragment_retransmission.size);
    assert(final_fragment_retransmission_decoded.ok());
    assert(final_fragment_retransmission_decoded.packet.sequence == 6u);
    assert(final_fragment_retransmission_decoded.packet.reliable_present);
    assert(final_fragment_retransmission_decoded.packet.fragment_present);
    assert(
        final_fragment_retransmission_decoded.packet.fragment_metadata
            .descriptors[0]
            .raw_fragment_id
        == final_fragment_descriptor.raw_fragment_id);
    assert(
        final_fragment_retransmission_decoded.packet.fragment_metadata
            .descriptors[0]
            .payload_offset
        == final_fragment_descriptor.payload_offset);
    assert(
        final_fragment_retransmission_decoded.packet.fragment_metadata
            .descriptors[0]
            .payload_length
        == final_fragment_descriptor.payload_length);
    assert(
        final_fragment_retransmission_decoded.packet.payload_size
        == final_fragment_decoded.packet.payload_size);
    assert(std::equal(
        final_fragment_decoded.packet.payload.begin(),
        final_fragment_decoded.packet.payload.begin()
            + static_cast<std::ptrdiff_t>(
                final_fragment_decoded.packet.payload_size),
        final_fragment_retransmission_decoded.packet.payload.begin()));
    assert(state.local_reliable_sequence() == final_fragment_toggle);
    assert(state.fragment_sender().resend_count() == 2u);

    const GoldSrcNetchanProcessOutcome
        non_covering_final_fragment_ack =
            state.ProcessIncomingDatagramDetailed(
                endpoint,
                Packet(7u, 5u, false, final_fragment_toggle),
                start + 17ms);
    assert(
        non_covering_final_fragment_ack.result
        == GoldSrcNetchanProcessResult::kAccepted);
    assert(
        !non_covering_final_fragment_ack
             .reliable_payload_was_acknowledged());
    assert(!state.fragment_sender().completion_acknowledged());
    assert(state.reliable_acknowledgement_generation() == 0u);

    const GoldSrcNetchanProcessOutcome final_fragment_ack =
        state.ProcessIncomingDatagramDetailed(
            endpoint,
            Packet(8u, 6u, false, final_fragment_toggle),
            start + 18ms);
    assert(
        final_fragment_ack.result
        == GoldSrcNetchanProcessResult::kAccepted);
    assert(final_fragment_ack.reliable_payload_was_acknowledged());
    assert(
        final_fragment_ack.acknowledged_reliable_kind
        == GoldSrcNetchanReliablePayloadKind::kSignonBootstrap);
    assert(final_fragment_ack.reliable_acknowledgement_generation == 1u);
    assert(
        state.last_acknowledged_reliable_kind()
        == GoldSrcNetchanReliablePayloadKind::kSignonBootstrap);
    assert(state.reliable_acknowledgement_generation() == 1u);
    assert(state.fragment_sender().completion_acknowledged());
    assert(
        state.fragment_sender().phase()
        == GoldSrcFragmentTransferPhase::kCompleted);
    assert(!state.fragment_sender().active());
    assert(!state.reliable_pending());
    assert(state.diagnostics().reliable_queued == 1u);
    assert(state.diagnostics().reliable_sent == 2u);
    assert(state.diagnostics().reliable_resent == 2u);
    assert(state.diagnostics().reliable_acked == 2u);
    assert(state.diagnostics().reliable_ack_mismatch == 4u);
    assert(
        state.fragment_sender().diagnostics().transfers_planned == 1u);
    assert(
        state.fragment_sender().diagnostics().fragments_sent == 2u);
    assert(
        state.fragment_sender().diagnostics().fragments_resent == 2u);
    assert(
        state.fragment_sender().diagnostics().fragments_acknowledged == 2u);
    assert(
        state.fragment_sender().diagnostics().transfers_completed == 1u);

    state.Reset();
    AssertCleanState(state);
    assert(
        state.fragment_sender().phase()
        == GoldSrcFragmentTransferPhase::kNone);
    assert(state.fragment_sender().fragment_count() == 0u);
    assert(
        state.fragment_sender().diagnostics().transfers_planned == 0u);
    assert(
        state.Initialize(
            reused_endpoint,
            std::nullopt,
            1u,
            start + 20ms));
    assert(state.remote_endpoint() == reused_endpoint);
    assert(!state.reliable_pending());
    assert(
        state.pending_reliable_kind()
        == GoldSrcNetchanReliablePayloadKind::kNone);
    assert(
        state.last_acknowledged_reliable_kind()
        == GoldSrcNetchanReliablePayloadKind::kNone);
    assert(state.reliable_acknowledgement_generation() == 0u);
    assert(!state.fragment_sender().active());
    assert(
        state.fragment_sender().phase()
        == GoldSrcFragmentTransferPhase::kNone);
    AssertDiagnosticsAreZero(state.diagnostics());
}

void TestOptInBoundedApplicationPayloadPolicy()
{
    using namespace std::chrono_literals;
    const Ipv4Endpoint endpoint{{127u, 0u, 0u, 1u}, 28004u};
    const GoldSrcNetchanState::TimePoint start{};
    GoldSrcNetchanState state;
    assert(state.Initialize(endpoint, std::nullopt, 0u, start));
    assert(
        state.incoming_payload_policy()
        == GoldSrcNetchanIncomingPayloadPolicy::kStrictClientNopOnly);

    const GoldSrcNetchanPacket application = Packet(
        1u,
        0u,
        true,
        false,
        {0x04u, 0x6Eu, 0x65u, 0x77u, 0x00u});
    const StateSnapshot before_default_rejection = Snapshot(state);
    const GoldSrcNetchanProcessOutcome default_result =
        state.ProcessIncomingDatagramDetailed(
            endpoint,
            application,
            start + 1s);
    assert(
        default_result.result
        == GoldSrcNetchanProcessResult::kUnsupportedPayload);
    assert(!default_result.reliable_payload_was_acknowledged());
    AssertMutationStateEquals(before_default_rejection, state);

    state.SetIncomingPayloadPolicy(
        GoldSrcNetchanIncomingPayloadPolicy::
            kAcceptBoundedApplicationPayload);
    assert(
        state.incoming_payload_policy()
        == GoldSrcNetchanIncomingPayloadPolicy::
            kAcceptBoundedApplicationPayload);

    GoldSrcNetchanPacket oversized = application;
    oversized.payload_size = oversized.payload.size() + 1u;
    const StateSnapshot before_oversized_rejection = Snapshot(state);
    const GoldSrcNetchanProcessOutcome oversized_result =
        state.ProcessIncomingDatagramDetailed(
            endpoint,
            oversized,
            start + 2s);
    assert(
        oversized_result.result
        == GoldSrcNetchanProcessResult::kPayloadDecodeFailed);
    assert(!oversized_result.reliable_payload_was_acknowledged());
    AssertMutationStateEquals(before_oversized_rejection, state);

    const GoldSrcNetchanProcessOutcome accepted =
        state.ProcessIncomingDatagramDetailed(
            endpoint,
            application,
            start + 3s);
    assert(accepted.result == GoldSrcNetchanProcessResult::kAccepted);
    assert(!accepted.reliable_payload_was_acknowledged());
    assert(state.incoming_sequence() == 1u);
    assert(state.incoming_reliable_sequence());

    state.Reset();
    AssertCleanState(state);
}

void TestReliableCoverageWrongToggleAndReferenceResend()
{
    using namespace std::chrono_literals;
    const Ipv4Endpoint endpoint{{127u, 0u, 0u, 1u}, 28001u};
    const GoldSrcNetchanState::TimePoint start{};
    GoldSrcNetchanState state;
    assert(state.Initialize(endpoint, std::uint16_t{778u}, 0u, start));
    const std::uint8_t nop = kGoldSrcServerNop;
    assert(
        state.QueueReliablePayload(&nop, 1u)
        == GoldSrcNetchanQueueResult::kQueued);
    const bool reliable_toggle = state.local_reliable_sequence();

    GoldSrcNetchanDatagram first{};
    assert(state.BuildOutgoingDatagram(&first));
    const GoldSrcNetchanDecodeResult first_decoded =
        DecodeGoldSrcNetchanDatagram(
            GoldSrcNetchanDirection::kServerToClient,
            first.bytes.data(),
            first.size);
    assert(first_decoded.ok());
    assert(first_decoded.packet.reliable_present);
    assert(first_decoded.packet.sequence == 1u);

    // The reliable toggle alone is insufficient: ACK zero does not cover the
    // server sequence that carried this reliable payload.
    assert(
        state.ProcessIncomingDatagram(
            endpoint,
            Packet(1u, 0u, false, reliable_toggle),
            start + 1s)
        == GoldSrcNetchanProcessResult::kAccepted);
    assert(state.reliable_pending());
    assert(state.diagnostics().reliable_acked == 0u);

    GoldSrcNetchanDatagram second{};
    assert(state.BuildOutgoingDatagram(&second));
    const GoldSrcNetchanDecodeResult second_decoded =
        DecodeGoldSrcNetchanDatagram(
            GoldSrcNetchanDirection::kServerToClient,
            second.bytes.data(),
            second.size);
    assert(second_decoded.ok());
    assert(second_decoded.packet.sequence == 2u);
    assert(!second_decoded.packet.reliable_present);

    // An ACK that exactly covers the original reliable sequence but carries
    // the old toggle records a mismatch and must not clear or immediately
    // retransmit the reliable payload.
    assert(
        state.ProcessIncomingDatagram(
            endpoint,
            Packet(2u, 1u, false, !reliable_toggle),
            start + 2s)
        == GoldSrcNetchanProcessResult::kAccepted);
    assert(state.reliable_pending());
    assert(state.local_reliable_sequence() == reliable_toggle);
    assert(state.diagnostics().reliable_ack_mismatch == 1u);

    GoldSrcNetchanDatagram third{};
    assert(state.BuildOutgoingDatagram(&third));
    const GoldSrcNetchanDecodeResult third_decoded =
        DecodeGoldSrcNetchanDatagram(
            GoldSrcNetchanDirection::kServerToClient,
            third.bytes.data(),
            third.size);
    assert(third_decoded.ok());
    assert(third_decoded.packet.sequence == 3u);
    assert(!third_decoded.packet.reliable_present);
    assert(state.diagnostics().reliable_resent == 0u);

    // The reference resend condition is reached only after a later ordinary
    // server sequence is acknowledged while the reliable toggle still differs.
    assert(
        state.ProcessIncomingDatagram(
            endpoint,
            Packet(3u, 2u, false, !reliable_toggle),
            start + 3s)
        == GoldSrcNetchanProcessResult::kAccepted);
    assert(state.reliable_pending());
    assert(state.diagnostics().reliable_ack_mismatch == 2u);

    GoldSrcNetchanDatagram retransmission{};
    assert(state.BuildOutgoingDatagram(&retransmission));
    const GoldSrcNetchanDecodeResult retransmission_decoded =
        DecodeGoldSrcNetchanDatagram(
            GoldSrcNetchanDirection::kServerToClient,
            retransmission.bytes.data(),
            retransmission.size);
    assert(retransmission_decoded.ok());
    assert(retransmission_decoded.packet.sequence == 4u);
    assert(retransmission_decoded.packet.reliable_present);
    assert(state.last_reliable_sequence() == 4u);
    assert(state.local_reliable_sequence() == reliable_toggle);
    assert(state.diagnostics().reliable_sent == 1u);
    assert(state.diagnostics().reliable_resent == 1u);
    assert(
        retransmission_decoded.packet.payload_size
        == first_decoded.packet.payload_size);
    assert(std::equal(
        first_decoded.packet.payload.begin(),
        first_decoded.packet.payload.begin()
            + static_cast<std::ptrdiff_t>(first_decoded.packet.payload_size),
        retransmission_decoded.packet.payload.begin()));

    assert(
        state.ProcessIncomingDatagram(
            endpoint,
            Packet(4u, 4u, false, reliable_toggle),
            start + 4s)
        == GoldSrcNetchanProcessResult::kAccepted);
    assert(!state.reliable_pending());
    assert(state.transport_phase() == GoldSrcNetchanTransportPhase::kEstablished);
    assert(state.diagnostics().reliable_acked == 1u);
    assert(state.local_reliable_sequence() == reliable_toggle);
}

void TestForgedAckAndCodecRejectionsPreserveReliableState()
{
    using namespace std::chrono_literals;
    const Ipv4Endpoint endpoint{{127u, 0u, 0u, 1u}, 28002u};
    const GoldSrcNetchanState::TimePoint start{};
    GoldSrcNetchanState state;
    assert(state.Initialize(endpoint, std::uint16_t{779u}, 0u, start));
    const std::uint8_t nop = kGoldSrcServerNop;
    assert(
        state.QueueReliablePayload(&nop, 1u)
        == GoldSrcNetchanQueueResult::kQueued);
    GoldSrcNetchanDatagram first{};
    assert(state.BuildOutgoingDatagram(&first));

    const StateSnapshot before_future = Snapshot(state);
    assert(
        state.ProcessIncomingDatagram(
            endpoint,
            Packet(1u, 2u, false, true),
            start + 1s)
        == GoldSrcNetchanProcessResult::kFutureAck);
    AssertMutationStateEquals(before_future, state);
    assert(state.reliable_pending());
    assert(!state.has_accepted_activity());
    assert(state.transport_phase()
        == GoldSrcNetchanTransportPhase::kAwaitingFirstReliableAck);

    GoldSrcNetchanPacket impossible_payload = Packet(1u, 1u, false, true);
    impossible_payload.payload_size = impossible_payload.payload.size() + 1u;
    assert(
        state.ProcessIncomingDatagram(
            endpoint,
            impossible_payload,
            start + 2s)
        == GoldSrcNetchanProcessResult::kPayloadDecodeFailed);
    AssertMutationStateEquals(before_future, state);

    const std::array<std::uint8_t, 8> fragment = {
        0x01u, 0x00u, 0x00u, 0x40u,
        0x01u, 0x00u, 0x00u, 0x80u,
    };
    assert(
        DecodeGoldSrcNetchanDatagram(
            GoldSrcNetchanDirection::kClientToServer,
            fragment.data(),
            fragment.size()).status
        == GoldSrcNetchanCodecStatus::kMalformedFragment);
    state.RecordRejected(GoldSrcNetchanProcessResult::kUnsupportedFragment);
    AssertMutationStateEquals(before_future, state);

    const std::array<std::uint8_t, 7> malformed{};
    assert(
        DecodeGoldSrcNetchanDatagram(
            GoldSrcNetchanDirection::kClientToServer,
            malformed.data(),
            malformed.size()).status
        == GoldSrcNetchanCodecStatus::kMalformedHeader);
    state.RecordRejected(GoldSrcNetchanProcessResult::kMalformedHeader);
    AssertMutationStateEquals(before_future, state);

    const std::array<std::uint8_t, 8> reserved_ack = {
        0x01u, 0x00u, 0x00u, 0x00u,
        0x01u, 0x00u, 0x00u, 0x40u,
    };
    assert(
        DecodeGoldSrcNetchanDatagram(
            GoldSrcNetchanDirection::kClientToServer,
            reserved_ack.data(),
            reserved_ack.size()).status
        == GoldSrcNetchanCodecStatus::kUnsupportedReservedFlags);
    state.RecordRejected(
        GoldSrcNetchanProcessResult::kUnsupportedReservedFlags);
    AssertMutationStateEquals(before_future, state);

    assert(state.diagnostics().future_ack == 1u);
    assert(state.diagnostics().payload_decode_failed == 1u);
    assert(state.diagnostics().unsupported_fragment == 1u);
    assert(state.diagnostics().malformed_header == 1u);
    assert(state.diagnostics().unsupported_reserved_flags == 1u);

    assert(
        state.ProcessIncomingDatagram(
            endpoint,
            Packet(1u, 1u, false, false),
            start + 2s)
        == GoldSrcNetchanProcessResult::kAccepted);
    assert(state.reliable_pending());
    assert(state.highest_accepted_acknowledgement() == 1u);
    assert(state.last_accepted_at() == start + 2s);

    const StateSnapshot before_stale = Snapshot(state);
    assert(
        state.ProcessIncomingDatagram(
            endpoint,
            Packet(2u, 0u, false, true),
            start + 3s)
        == GoldSrcNetchanProcessResult::kStaleAck);
    AssertMutationStateEquals(before_stale, state);
    assert(state.reliable_pending());
    assert(state.highest_accepted_acknowledgement() == 1u);
    assert(state.last_accepted_at() == start + 2s);

    assert(
        state.ProcessIncomingDatagram(
            endpoint,
            Packet(2u, 1u, false, true),
            start + 4s)
        == GoldSrcNetchanProcessResult::kAccepted);
    assert(!state.reliable_pending());
    assert(state.transport_phase() == GoldSrcNetchanTransportPhase::kEstablished);
    assert(state.diagnostics().stale_ack == 1u);
}

void TestResetAndSlotReuse()
{
    using namespace std::chrono_literals;
    const Ipv4Endpoint first_endpoint{{10u, 0u, 0u, 1u}, 30000u};
    const Ipv4Endpoint reused_endpoint{{10u, 0u, 0u, 2u}, 30001u};
    const GoldSrcNetchanState::TimePoint start{};
    GoldSrcNetchanState state;
    assert(state.Initialize(
        first_endpoint,
        std::uint16_t{111u},
        4u,
        start + 1s));
    const std::uint8_t nop = kGoldSrcServerNop;
    assert(
        state.QueueReliablePayload(&nop, 1u)
        == GoldSrcNetchanQueueResult::kQueued);
    GoldSrcNetchanDatagram first{};
    assert(state.BuildOutgoingDatagram(&first));
    assert(
        state.ProcessIncomingDatagram(
            first_endpoint,
            Packet(1u, 1u, false, false),
            start + 2s)
        == GoldSrcNetchanProcessResult::kAccepted);
    assert(state.reliable_pending());
    assert(state.has_accepted_activity());

    state.Reset();
    AssertCleanState(state);

    assert(state.Initialize(
        reused_endpoint,
        std::nullopt,
        9u,
        start + 10s));
    assert(state.remote_endpoint() == reused_endpoint);
    assert(!state.connect_qport().has_value());
    assert(state.channel_identifier() == 9u);
    assert(state.outgoing_sequence() == 1u);
    assert(state.incoming_sequence() == 0u);
    assert(!state.reliable_pending());
    assert(!state.local_reliable_sequence());
    assert(!state.has_accepted_activity());
    assert(state.last_accepted_at() == GoldSrcNetchanState::TimePoint{});
    assert(state.transport_phase() == GoldSrcNetchanTransportPhase::kNone);
    AssertDiagnosticsAreZero(state.diagnostics());

    assert(
        state.QueueReliablePayload(&nop, 1u)
        == GoldSrcNetchanQueueResult::kQueued);
    GoldSrcNetchanDatagram reused_first{};
    assert(state.BuildOutgoingDatagram(&reused_first));
    const std::vector<std::uint8_t> expected_fresh_first = {
        0x01u, 0x00u, 0x00u, 0x80u,
        0x00u, 0x00u, 0x00u, 0x00u,
        0x5Au, 0x19u, 0x01u, 0x00u,
        0x1Au, 0x01u, 0x11u, 0x40u,
    };
    assert(DatagramBytes(reused_first) == expected_fresh_first);
}

void TestEndpointRouterAndSecondEndpointIsolation()
{
    const Ipv4Endpoint endpoint_a{{127u, 0u, 0u, 1u}, 31000u};
    const Ipv4Endpoint endpoint_b{{127u, 0u, 0u, 1u}, 31001u};
    const Ipv4Endpoint unknown{{127u, 0u, 0u, 1u}, 31002u};
    const GoldSrcNetchanState::TimePoint start{};
    GoldSrcNetchanState session_a;
    GoldSrcNetchanState session_b;
    GoldSrcNetchanState unused;
    assert(session_a.Initialize(
        endpoint_a,
        std::uint16_t{700u},
        0u,
        start));
    assert(session_b.Initialize(
        endpoint_b,
        std::uint16_t{701u},
        1u,
        start));

    std::array<GoldSrcNetchanState*, 4> sessions = {
        nullptr,
        &session_a,
        &unused,
        &session_b,
    };
    assert(
        FindGoldSrcNetchanSession(
            sessions.data(),
            sessions.size(),
            endpoint_a)
        == &session_a);
    assert(
        FindGoldSrcNetchanSession(
            sessions.data(),
            sessions.size(),
            endpoint_b)
        == &session_b);

    const StateSnapshot before_unknown_a = Snapshot(session_a);
    const StateSnapshot before_unknown_b = Snapshot(session_b);
    assert(
        FindGoldSrcNetchanSession(
            sessions.data(),
            sessions.size(),
            unknown)
        == nullptr);
    AssertMutationStateEquals(before_unknown_a, session_a);
    AssertMutationStateEquals(before_unknown_b, session_b);
    assert(FindGoldSrcNetchanSession(nullptr, 4u, endpoint_a) == nullptr);
    assert(
        FindGoldSrcNetchanSession(
            sessions.data(),
            0u,
            endpoint_a)
        == nullptr);

    const StateSnapshot before_hijack = Snapshot(session_a);
    assert(
        session_a.ProcessIncomingDatagram(
            endpoint_b,
            Packet(1u, 0u),
            start + std::chrono::seconds(1))
        == GoldSrcNetchanProcessResult::kEndpointMismatch);
    AssertMutationStateEquals(before_hijack, session_a);
    assert(session_a.diagnostics().endpoint_mismatch == 1u);
    assert(!session_a.has_accepted_activity());
}

void TestHandshakeCommitInitializesExactlyOnePreSignonState()
{
    using namespace std::chrono_literals;
    const Ipv4Endpoint endpoint{{127u, 0u, 0u, 1u}, 32000u};
    const ChallengeTable::TimePoint start{};
    constexpr std::uint16_t qport = 777u;
    std::int32_t generated = 606060;
    std::array<GoldSrcNetchanState, 2> sessions{};
    int commit_calls = 0;
    int initialization_calls = 0;
    bool connected = false;
    bool put_in_server = false;
    bool spawned = false;
    bool active = false;

    ChallengeTable challenges(4u, 10s, [&generated]() { return generated++; });
    HandshakeProcessor processor(
        challenges,
        [&](const AdmissionRequest& request)
        {
            assert(request.endpoint == endpoint);
            assert(request.connect.protocol == 48);
            assert(request.connect.qport.has_value());
            assert(*request.connect.qport == qport);
            AdmissionResult admission{};
            admission.accepted = true;
            admission.slot = 0;
            admission.session_identifier = "netchan-test-session";
            admission.session_state = "connected";
            admission.spawned = false;
            admission.active = false;
            return admission;
        },
        [&](const AdmissionRequest& request, const AdmissionResult& admission)
        {
            ++commit_calls;
            assert(admission.slot == 0);
            assert(!sessions[0].initialized());
            assert(!sessions[1].initialized());
            const bool initialized = sessions[0].Initialize(
                request.endpoint,
                request.connect.qport,
                static_cast<std::uint16_t>(admission.slot),
                start + 1s);
            if (!initialized)
            {
                return false;
            }
            ++initialization_calls;
            connected = true;
            return true;
        });

    const HandshakeResult challenge = processor.Process(
        endpoint,
        Connectionless("getchallenge steam\n"),
        start);
    assert(challenge.outcome == HandshakeOutcome::kChallengeIssued);
    assert(challenge.issued_challenge.has_value());
    assert(*challenge.issued_challenge == 606060);

    const std::vector<std::uint8_t> connect =
        ConnectPacket(*challenge.issued_challenge, qport);
    const HandshakeResult accepted =
        processor.Process(endpoint, connect, start + 1s);
    assert(accepted.outcome == HandshakeOutcome::kConnectAccepted);
    assert(accepted.admission.has_value());
    assert(accepted.admission->session_state == "connected");
    assert(!accepted.admission->spawned);
    assert(!accepted.admission->active);
    assert(commit_calls == 1);
    assert(initialization_calls == 1);
    assert(sessions[0].initialized());
    assert(!sessions[1].initialized());
    assert(sessions[0].remote_endpoint() == endpoint);
    assert(sessions[0].connect_qport().has_value());
    assert(*sessions[0].connect_qport() == qport);
    assert(sessions[0].outgoing_sequence() == 1u);
    assert(sessions[0].incoming_sequence() == 0u);
    assert(sessions[0].transport_phase() == GoldSrcNetchanTransportPhase::kNone);
    assert(!sessions[0].has_accepted_activity());
    assert(connected);
    assert(!put_in_server);
    assert(!spawned);
    assert(!active);

    const HandshakeResult replay =
        processor.Process(endpoint, connect, start + 2s);
    assert(replay.outcome == HandshakeOutcome::kConnectRejected);
    assert(replay.reason == "challenge_consumed");
    assert(commit_calls == 1);
    assert(initialization_calls == 1);

    std::int32_t rejected_generated = 707070;
    ChallengeTable rejected_challenges(
        4u,
        10s,
        [&rejected_generated]() { return rejected_generated++; });
    std::array<GoldSrcNetchanState, 2> rejected_sessions{};
    int rejected_commit_calls = 0;
    HandshakeProcessor rejecting_processor(
        rejected_challenges,
        [](const AdmissionRequest&)
        {
            AdmissionResult admission{};
            admission.rejection_reason = "server_full";
            return admission;
        },
        [&](const AdmissionRequest&, const AdmissionResult&)
        {
            ++rejected_commit_calls;
            return false;
        });
    const HandshakeResult rejected_challenge = rejecting_processor.Process(
        endpoint,
        Connectionless("getchallenge steam\n"),
        start);
    assert(rejected_challenge.issued_challenge.has_value());
    const HandshakeResult rejected = rejecting_processor.Process(
        endpoint,
        ConnectPacket(*rejected_challenge.issued_challenge, qport),
        start + 1s);
    assert(rejected.outcome == HandshakeOutcome::kConnectRejected);
    assert(rejected.reason == "server_full");
    assert(rejected_commit_calls == 0);
    assert(!rejected_sessions[0].initialized());
    assert(!rejected_sessions[1].initialized());

    std::int32_t commit_failure_generated = 808080;
    ChallengeTable commit_failure_challenges(
        4u,
        10s,
        [&commit_failure_generated]() { return commit_failure_generated++; });
    GoldSrcNetchanState commit_failure_state;
    HandshakeProcessor commit_failure_processor(
        commit_failure_challenges,
        [](const AdmissionRequest&)
        {
            AdmissionResult admission{};
            admission.accepted = true;
            admission.slot = 0;
            admission.session_identifier = "planned-only";
            admission.session_state = "connected";
            return admission;
        },
        [](const AdmissionRequest&, const AdmissionResult&)
        {
            return false;
        });
    const HandshakeResult commit_failure_challenge =
        commit_failure_processor.Process(
            endpoint,
            Connectionless("getchallenge steam\n"),
            start);
    assert(commit_failure_challenge.issued_challenge.has_value());
    const HandshakeResult commit_failure = commit_failure_processor.Process(
        endpoint,
        ConnectPacket(*commit_failure_challenge.issued_challenge, qport),
        start + 1s);
    assert(commit_failure.outcome == HandshakeOutcome::kConnectRejected);
    assert(commit_failure.reason == "admission_commit_failed");
    AssertCleanState(commit_failure_state);
}
} // namespace

int main()
{
    TestCodecBoundsFlagsAndDirections();
    TestMunge2GoldenRoundTripAndCompleteGroups();
    TestGoldenPacketsPaddingAndRouteableLimit();
    TestSequenceArithmetic();
    TestInitialStateFirstSequenceDuplicatesAndGaps();
    TestStaleAndFutureAcknowledgementsAreAtomic();
    TestReliableQueueBoundsAndSecondQueue();
    TestFirstReliableSendAndCorrectAcknowledgement();
    TestServerInfoReliableKindAckMetadataAndEstablishedPhase();
    TestResourceManifestReliableKindAckRetransmitAndReset();
    TestSignonBootstrapReliableKindUnfragmentedAndFragmented();
    TestOptInBoundedApplicationPayloadPolicy();
    TestReliableCoverageWrongToggleAndReferenceResend();
    TestForgedAckAndCodecRejectionsPreserveReliableState();
    TestResetAndSlotReuse();
    TestEndpointRouterAndSecondEndpointIsolation();
    TestHandshakeCommitInitializesExactlyOnePreSignonState();
    return 0;
}
