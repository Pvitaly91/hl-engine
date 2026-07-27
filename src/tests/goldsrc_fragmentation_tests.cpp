#include "network/goldsrc_fragmentation.h"
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
#include <limits>
#include <vector>

namespace
{
using namespace hl::network;
using namespace std::chrono_literals;

GoldSrcFragmentDescriptor Descriptor(
    std::uint16_t index,
    std::uint16_t count,
    std::uint16_t length,
    std::uint16_t offset = 0u)
{
    GoldSrcFragmentDescriptor descriptor{};
    descriptor.stream = GoldSrcFragmentStream::kNormal;
    descriptor.raw_presence = 1u;
    descriptor.raw_fragment_id =
        (static_cast<std::uint32_t>(index) << 16u) | count;
    descriptor.fragment_index = index;
    descriptor.fragment_count = count;
    descriptor.payload_offset = offset;
    descriptor.payload_length = length;
    return descriptor;
}

GoldSrcNetchanPacket Ack(
    std::uint32_t sequence,
    std::uint32_t acknowledgement,
    bool reliable_acknowledgement)
{
    GoldSrcNetchanPacket packet{};
    packet.sequence = sequence & kGoldSrcNetchanSequenceMask;
    packet.acknowledgement =
        acknowledgement & kGoldSrcNetchanSequenceMask;
    packet.reliable_acknowledgement = reliable_acknowledgement;
    packet.raw_sequence = packet.sequence;
    packet.raw_acknowledgement = packet.acknowledgement
        | (reliable_acknowledgement
                ? kGoldSrcNetchanReliableFlag
                : 0u);
    return packet;
}

std::vector<std::uint8_t> DecodedFragmentBytes(
    const GoldSrcNetchanDatagram& datagram)
{
    const GoldSrcNetchanDecodeResult decoded =
        DecodeGoldSrcNetchanDatagram(
            GoldSrcNetchanDirection::kServerToClient,
            datagram.bytes.data(),
            datagram.size);
    assert(decoded.ok());
    assert(decoded.packet.fragment_present);
    return std::vector<std::uint8_t>(
        decoded.packet.payload.begin(),
        decoded.packet.payload.begin()
            + static_cast<std::ptrdiff_t>(decoded.packet.payload_size));
}

void TestStableConstantsAndReasons()
{
    static_assert(kGoldSrcFragmentStreamCount == 2u);
    static_assert(kGoldSrcNormalOnlyFragmentMetadataBytes == 10u);
    static_assert(kGoldSrcMaximumFragmentBytes == 1024u);
    static_assert(kGoldSrcNetchanFragmentPayloadCapacity == 1024u);
    static_assert(kGoldSrcMaximumFragmentTransferBytes == 65536u);
    static_assert(kGoldSrcMaximumNormalFragmentCount == 64u);

    assert(ReasonFor(GoldSrcFragmentCodecStatus::kOk) == "ok");
    assert(
        ReasonFor(GoldSrcFragmentPlanResult::kTransferBusy)
        == "transfer_busy");
    assert(
        ReasonFor(GoldSrcFragmentProcessResult::kTransferMismatch)
        == "transfer_mismatch");
    assert(
        NameFor(GoldSrcFragmentTransferPhase::kSentAwaitingAck)
        == "sent_awaiting_ack");
}

void TestFragmentMetadataCodecAndBounds()
{
    const GoldSrcFragmentDescriptor descriptor = Descriptor(2u, 3u, 4u);
    std::array<std::uint8_t, kGoldSrcNormalOnlyFragmentMetadataBytes>
        encoded{};
    std::size_t encoded_size = 0u;
    assert(
        EncodeGoldSrcNormalFragmentMetadata(
            descriptor,
            encoded.data(),
            encoded.size(),
            &encoded_size)
        == GoldSrcFragmentCodecStatus::kOk);
    const std::array<
        std::uint8_t,
        kGoldSrcNormalOnlyFragmentMetadataBytes> expected = {
        0x01u,
        0x03u, 0x00u, 0x02u, 0x00u,
        0x00u, 0x00u,
        0x04u, 0x00u,
        0x00u,
    };
    assert(encoded_size == expected.size());
    assert(encoded == expected);

    const GoldSrcFragmentDecodeResult decoded =
        DecodeGoldSrcFragmentMetadata(encoded.data(), encoded.size());
    assert(decoded.ok());
    assert(decoded.metadata.encoded_size == encoded.size());
    assert(decoded.metadata.present_count == 1u);
    assert(decoded.metadata.present[0]);
    assert(!decoded.metadata.present[1]);
    const GoldSrcFragmentDescriptor& round_trip =
        decoded.metadata.descriptors[0];
    assert(round_trip.raw_fragment_id == 0x00020003u);
    assert(round_trip.fragment_index == 2u);
    assert(round_trip.fragment_count == 3u);
    assert(round_trip.payload_offset == 0u);
    assert(round_trip.payload_length == 4u);
    assert(
        ValidateGoldSrcFragmentRanges(decoded.metadata, 4u)
        == GoldSrcFragmentCodecStatus::kOk);

    for (std::size_t boundary = 0u;
         boundary < expected.size();
         ++boundary)
    {
        assert(
            DecodeGoldSrcFragmentMetadata(
                expected.data(),
                boundary).status
            == GoldSrcFragmentCodecStatus::kTruncated);
    }

    std::array<std::uint8_t, 10u> invalid = expected;
    invalid[0] = 2u;
    assert(
        DecodeGoldSrcFragmentMetadata(
            invalid.data(),
            invalid.size()).status
        == GoldSrcFragmentCodecStatus::kInvalidPresence);

    invalid = expected;
    invalid[7] = 0u;
    invalid[8] = 0u;
    assert(
        DecodeGoldSrcFragmentMetadata(
            invalid.data(),
            invalid.size()).status
        == GoldSrcFragmentCodecStatus::kInvalidLength);

    invalid = expected;
    invalid[7] = 0x01u;
    invalid[8] = 0x04u;
    assert(
        DecodeGoldSrcFragmentMetadata(
            invalid.data(),
            invalid.size()).status
        == GoldSrcFragmentCodecStatus::kInvalidLength);

    std::array<std::uint8_t, 10u> outside = expected;
    outside[5] = 0xFFu;
    outside[6] = 0xFFu;
    outside[7] = 0x02u;
    outside[8] = 0x00u;
    const GoldSrcFragmentDecodeResult outside_decoded =
        DecodeGoldSrcFragmentMetadata(outside.data(), outside.size());
    assert(outside_decoded.ok());
    assert(
        ValidateGoldSrcFragmentRanges(outside_decoded.metadata, 16u)
        == GoldSrcFragmentCodecStatus::kFragmentOutsidePayload);

    const GoldSrcFragmentDescriptor maximum =
        Descriptor(64u, 64u, 1024u);
    assert(
        EncodeGoldSrcNormalFragmentMetadata(
            maximum,
            encoded.data(),
            encoded.size(),
            &encoded_size)
        == GoldSrcFragmentCodecStatus::kOk);
    assert(
        DecodeGoldSrcFragmentMetadata(
            encoded.data(),
            encoded.size()).ok());

    GoldSrcFragmentDescriptor one_over = maximum;
    one_over.payload_length = 1025u;
    assert(
        EncodeGoldSrcNormalFragmentMetadata(
            one_over,
            encoded.data(),
            encoded.size(),
            &encoded_size)
        == GoldSrcFragmentCodecStatus::kInvalidLength);
    assert(
        EncodeGoldSrcNormalFragmentMetadata(
            descriptor,
            nullptr,
            encoded.size(),
            &encoded_size)
        == GoldSrcFragmentCodecStatus::kNullInput);
}

void TestFragmentDatagramFlagsTransformAndGoldenShape()
{
    const GoldSrcFragmentDescriptor descriptor = Descriptor(1u, 2u, 3u);
    const std::array<std::uint8_t, 3u> payload = {0x2Bu, 0x02u, 0x00u};
    GoldSrcNetchanDatagram datagram{};
    assert(
        EncodeGoldSrcFragmentDatagram(
            GoldSrcNetchanDirection::kServerToClient,
            0x01020304u,
            0x05060708u,
            true,
            descriptor,
            payload.data(),
            payload.size(),
            &datagram)
        == GoldSrcNetchanCodecStatus::kOk);
    assert(datagram.size == 21u);
    assert(datagram.bytes[0] == 0x04u);
    assert(datagram.bytes[1] == 0x03u);
    assert(datagram.bytes[2] == 0x02u);
    assert(datagram.bytes[3] == 0xC1u);
    assert(datagram.bytes[4] == 0x08u);
    assert(datagram.bytes[5] == 0x07u);
    assert(datagram.bytes[6] == 0x06u);
    assert(datagram.bytes[7] == 0x85u);
    const std::array<std::uint8_t, 21u> golden_datagram = {
        0x04u, 0x03u, 0x02u, 0xC1u,
        0x08u, 0x07u, 0x06u, 0x85u,
        0x5Fu, 0x18u, 0x02u, 0x05u,
        0x1Du, 0x00u, 0x10u, 0x44u,
        0x06u, 0x3Bu, 0x40u, 0x04u,
        0x00u,
    };
    assert(std::equal(
        golden_datagram.begin(),
        golden_datagram.end(),
        datagram.bytes.begin()));

    const GoldSrcNetchanDecodeResult decoded =
        DecodeGoldSrcNetchanDatagram(
            GoldSrcNetchanDirection::kServerToClient,
            datagram.bytes.data(),
            datagram.size);
    assert(decoded.ok());
    assert(decoded.packet.fragment_present);
    assert(decoded.packet.reliable_present);
    assert(decoded.packet.reliable_acknowledgement);
    assert(decoded.packet.fragment_metadata_size == 10u);
    assert(
        decoded.packet.fragment_metadata.descriptors[0].raw_fragment_id
        == 0x00010002u);
    assert(decoded.packet.payload_size == payload.size());
    assert(std::equal(
        payload.begin(),
        payload.end(),
        decoded.packet.payload.begin()));

    std::array<std::uint8_t, 13u> transformed{};
    std::copy_n(datagram.bytes.begin() + 8, transformed.size(), transformed.begin());
    assert(transformed.back() == payload.back());
    UnmungeGoldSrcNetchanPayload(
        transformed.data(),
        transformed.size(),
        0x01020304u);
    const std::array<std::uint8_t, 13u> expected_decoded = {
        0x01u,
        0x02u, 0x00u, 0x01u, 0x00u,
        0x00u, 0x00u,
        0x03u, 0x00u,
        0x00u,
        0x2Bu, 0x02u, 0x00u,
    };
    assert(transformed == expected_decoded);

    GoldSrcNetchanDatagram second{};
    assert(
        EncodeGoldSrcFragmentDatagram(
            GoldSrcNetchanDirection::kServerToClient,
            0x01020304u,
            0x05060708u,
            true,
            descriptor,
            payload.data(),
            payload.size(),
            &second)
        == GoldSrcNetchanCodecStatus::kOk);
    assert(second.bytes == datagram.bytes);
    assert(second.size == datagram.size);
    assert(std::all_of(
        datagram.bytes.begin()
            + static_cast<std::ptrdiff_t>(datagram.size),
        datagram.bytes.end(),
        [](std::uint8_t byte)
        {
            return byte == 0u;
        }));

    std::array<std::uint8_t, 8u> reserved_ack = {
        0x01u, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x00u, 0x40u,
    };
    assert(
        DecodeGoldSrcNetchanDatagram(
            GoldSrcNetchanDirection::kClientToServer,
            reserved_ack.data(),
            reserved_ack.size()).status
        == GoldSrcNetchanCodecStatus::kUnsupportedReservedFlags);

    std::array<std::uint8_t, 8u> fragment_without_reliable = {
        0x01u, 0x00u, 0x00u, 0x40u,
        0x00u, 0x00u, 0x00u, 0x00u,
    };
    assert(
        DecodeGoldSrcNetchanDatagram(
            GoldSrcNetchanDirection::kClientToServer,
            fragment_without_reliable.data(),
            fragment_without_reliable.size()).status
        == GoldSrcNetchanCodecStatus::kMalformedFragment);
}

void AssertPlanCoversPayload(
    const GoldSrcFragmentSender& sender,
    std::size_t expected_size)
{
    assert(sender.total_payload_size() == expected_size);
    std::size_t expected_offset = 0u;
    for (std::size_t index = 0u;
         index < sender.fragment_count();
         ++index)
    {
        const GoldSrcFragmentPlanEntry* entry = sender.fragment_at(index);
        assert(entry != nullptr);
        assert(entry->source_offset == expected_offset);
        const std::size_t expected_length = std::min(
            sender.fragment_capacity(),
            expected_size - expected_offset);
        assert(entry->payload_length == expected_length);
        assert(entry->descriptor.fragment_index == index + 1u);
        assert(
            entry->descriptor.fragment_count
            == sender.fragment_count());
        assert(entry->descriptor.payload_offset == 0u);
        assert(
            entry->descriptor.payload_length
            == entry->payload_length);
        expected_offset += expected_length;
    }
    assert(expected_offset == expected_size);
}

void TestFragmentPlanner()
{
    const GoldSrcFragmentSender::TimePoint start{};
    std::vector<std::uint8_t> small(
        kGoldSrcNetchanMaximumReliableBytes,
        0x11u);
    GoldSrcFragmentSender sender;
    assert(
        sender.Plan(small.data(), small.size(), 1u, start)
        == GoldSrcFragmentPlanResult::kNotRequired);
    assert(sender.phase() == GoldSrcFragmentTransferPhase::kNone);
    assert(
        sender.Plan(nullptr, 0u, 1u, start)
        == GoldSrcFragmentPlanResult::kPayloadEmpty);

    std::vector<std::uint8_t> first_fragmenting(
        kGoldSrcNetchanMaximumReliableBytes + 1u);
    for (std::size_t index = 0u;
         index < first_fragmenting.size();
         ++index)
    {
        first_fragmenting[index] =
            static_cast<std::uint8_t>(index & 0xFFu);
    }
    assert(
        sender.Plan(
            first_fragmenting.data(),
            first_fragmenting.size(),
            7u,
            start)
        == GoldSrcFragmentPlanResult::kPlanned);
    assert(sender.fragment_count() == 2u);
    assert(sender.current_fragment()->source_offset == 0u);
    assert(sender.current_fragment()->payload_length == 1024u);
    const std::uint8_t frozen_first = sender.current_fragment_data()[0];
    first_fragmenting[0] ^= 0xFFu;
    assert(sender.current_fragment_data()[0] == frozen_first);
    assert(
        sender.Plan(
            first_fragmenting.data(),
            first_fragmenting.size(),
            8u,
            start)
        == GoldSrcFragmentPlanResult::kTransferBusy);
    assert(sender.transfer_generation() == 7u);
    assert(sender.current_fragment_data()[0] == frozen_first);
    AssertPlanCoversPayload(sender, first_fragmenting.size());

    {
        GoldSrcFragmentSender exact;
        std::vector<std::uint8_t> payload(2048u, 0x22u);
        assert(
            exact.Plan(payload.data(), payload.size(), 1u, start)
            == GoldSrcFragmentPlanResult::kPlanned);
        assert(exact.fragment_count() == 2u);
        assert(exact.current_fragment()->payload_length == 1024u);
        assert(exact.AcknowledgeCurrent(start) == false);
    }
    {
        GoldSrcFragmentSender final_short;
        std::vector<std::uint8_t> payload(2049u, 0x33u);
        assert(
            final_short.Plan(payload.data(), payload.size(), 1u, start)
            == GoldSrcFragmentPlanResult::kPlanned);
        assert(final_short.fragment_count() == 3u);
        assert(final_short.current_fragment()->payload_length == 1024u);
        assert(final_short.fragment_at(2u)->payload_length == 1u);
    }
    {
        GoldSrcFragmentSender maximum;
        std::vector<std::uint8_t> payload(
            kGoldSrcMaximumFragmentTransferBytes,
            0x44u);
        assert(
            maximum.Plan(payload.data(), payload.size(), 1u, start)
            == GoldSrcFragmentPlanResult::kPlanned);
        assert(
            maximum.fragment_count()
            == kGoldSrcMaximumNormalFragmentCount);
        AssertPlanCoversPayload(maximum, payload.size());
    }
    {
        GoldSrcFragmentSender over;
        const std::array<std::uint8_t, 1u> byte = {0x55u};
        assert(
            over.Plan(
                byte.data(),
                kGoldSrcMaximumFragmentTransferBytes + 1u,
                1u,
                start)
            == GoldSrcFragmentPlanResult::kPayloadTooLarge);
        assert(
            over.Plan(
                byte.data(),
                std::numeric_limits<std::size_t>::max(),
                1u,
                start,
                kGoldSrcMaximumFragmentBytes,
                std::numeric_limits<std::size_t>::max())
            == GoldSrcFragmentPlanResult::kArithmeticOverflow);
    }
    {
        GoldSrcFragmentSender limited;
        std::vector<std::uint8_t> payload(2049u, 0x66u);
        assert(
            limited.Plan(
                payload.data(),
                payload.size(),
                1u,
                start,
                1024u,
                payload.size(),
                2u)
            == GoldSrcFragmentPlanResult::kFragmentCountExceeded);
    }
    {
        std::vector<std::uint8_t> payload(2500u, 0x77u);
        GoldSrcFragmentSender first;
        GoldSrcFragmentSender second;
        assert(
            first.Plan(payload.data(), payload.size(), 19u, start)
            == GoldSrcFragmentPlanResult::kPlanned);
        assert(
            second.Plan(payload.data(), payload.size(), 19u, start)
            == GoldSrcFragmentPlanResult::kPlanned);
        assert(first.fragment_count() == second.fragment_count());
        assert(
            first.current_fragment()->descriptor.raw_fragment_id
            == second.current_fragment()->descriptor.raw_fragment_id);
    }
}

void TestOrderedOutOfOrderAndDuplicateReassembly()
{
    const GoldSrcFragmentReassembler::TimePoint start{};
    const std::array<std::uint8_t, 4u> first = {1u, 2u, 3u, 4u};
    const std::array<std::uint8_t, 3u> second = {5u, 6u, 7u};
    {
        GoldSrcFragmentReassembler reassembler;
        assert(
            reassembler.Process(
                9u,
                Descriptor(1u, 2u, 4u),
                first.data(),
                first.size(),
                start)
            == GoldSrcFragmentProcessResult::kAccepted);
        assert(reassembler.received_fragment_count() == 1u);
        assert(
            reassembler.Process(
                9u,
                Descriptor(1u, 2u, 4u),
                first.data(),
                first.size(),
                start + 1ms)
            == GoldSrcFragmentProcessResult::kDuplicate);
        assert(reassembler.received_fragment_count() == 1u);
        assert(
            reassembler.Process(
                9u,
                Descriptor(2u, 2u, 3u),
                second.data(),
                second.size(),
                start + 2ms)
            == GoldSrcFragmentProcessResult::kCompleted);
        const std::array<std::uint8_t, 7u> expected = {
            1u, 2u, 3u, 4u, 5u, 6u, 7u,
        };
        assert(reassembler.complete());
        assert(reassembler.payload_size() == expected.size());
        assert(std::equal(
            expected.begin(),
            expected.end(),
            reassembler.payload_data()));
        assert(
            reassembler.Process(
                9u,
                Descriptor(2u, 2u, 3u),
                second.data(),
                second.size(),
                start + 3ms)
            == GoldSrcFragmentProcessResult::kDuplicate);
        reassembler.Reset();
        assert(!reassembler.active());
        assert(!reassembler.complete());
        assert(reassembler.payload_size() == 0u);
    }
    {
        GoldSrcFragmentReassembler reassembler;
        assert(
            reassembler.Process(
                12u,
                Descriptor(2u, 2u, 3u),
                second.data(),
                second.size(),
                start)
            == GoldSrcFragmentProcessResult::kAccepted);
        assert(
            reassembler.Process(
                12u,
                Descriptor(1u, 2u, 4u),
                first.data(),
                first.size(),
                start + 1ms)
            == GoldSrcFragmentProcessResult::kCompleted);
        assert(reassembler.payload_size() == 7u);
    }
}

void TestReassemblerMissingConflictIdentityAndLimits()
{
    const GoldSrcFragmentReassembler::TimePoint start{};
    const std::array<std::uint8_t, 2u> aa = {0xAAu, 0xAAu};
    const std::array<std::uint8_t, 2u> bb = {0xBBu, 0xBBu};
    const std::array<std::uint8_t, 3u> bbb = {
        0xBBu, 0xBBu, 0xBBu,
    };
    for (const std::size_t missing : {0u, 1u, 2u})
    {
        GoldSrcFragmentReassembler reassembler;
        for (std::size_t index = 0u; index < 3u; ++index)
        {
            if (index == missing)
            {
                continue;
            }
            assert(
                reassembler.Process(
                    1u,
                    Descriptor(
                        static_cast<std::uint16_t>(index + 1u),
                        3u,
                        2u),
                    aa.data(),
                    aa.size(),
                    start + std::chrono::milliseconds(index))
                == GoldSrcFragmentProcessResult::kAccepted);
        }
        assert(reassembler.active());
        assert(!reassembler.complete());
        assert(reassembler.received_fragment_count() == 2u);
    }
    {
        GoldSrcFragmentReassembler reassembler;
        assert(
            reassembler.Process(
                5u,
                Descriptor(1u, 2u, 2u),
                aa.data(),
                aa.size(),
                start)
            == GoldSrcFragmentProcessResult::kAccepted);
        assert(
            reassembler.Process(
                6u,
                Descriptor(2u, 2u, 2u),
                bb.data(),
                bb.size(),
                start)
            == GoldSrcFragmentProcessResult::kTransferMismatch);
        assert(
            reassembler.Process(
                5u,
                Descriptor(2u, 3u, 2u),
                bb.data(),
                bb.size(),
                start)
            == GoldSrcFragmentProcessResult::kInconsistentTotalSize);
        assert(reassembler.received_fragment_count() == 1u);
    }
    {
        GoldSrcFragmentReassembler reassembler;
        assert(
            reassembler.Process(
                7u,
                Descriptor(1u, 2u, 2u),
                aa.data(),
                aa.size(),
                start)
            == GoldSrcFragmentProcessResult::kAccepted);
        assert(
            reassembler.Process(
                7u,
                Descriptor(1u, 2u, 2u),
                bb.data(),
                bb.size(),
                start)
            == GoldSrcFragmentProcessResult::kConflictingDuplicate);
        assert(
            reassembler.Process(
                7u,
                Descriptor(1u, 2u, 3u),
                bbb.data(),
                bbb.size(),
                start)
            == GoldSrcFragmentProcessResult::kOverlapConflict);
        assert(reassembler.received_fragment_count() == 1u);
    }
    {
        GoldSrcFragmentReassembler reassembler;
        assert(
            reassembler.Process(
                1u,
                Descriptor(1u, 1u, 2u, 1u),
                aa.data(),
                aa.size(),
                start)
            == GoldSrcFragmentProcessResult::kInvalidOffset);
        GoldSrcFragmentDescriptor invalid_length =
            Descriptor(1u, 1u, 1u);
        invalid_length.payload_length = 0u;
        assert(
            reassembler.Process(
                1u,
                invalid_length,
                aa.data(),
                aa.size(),
                start)
            == GoldSrcFragmentProcessResult::kInvalidLength);
        GoldSrcFragmentDescriptor file =
            Descriptor(1u, 1u, 2u);
        file.stream = GoldSrcFragmentStream::kFile;
        assert(
            reassembler.Process(
                1u,
                file,
                aa.data(),
                aa.size(),
                start)
            == GoldSrcFragmentProcessResult::kUnsupportedStream);
        assert(!reassembler.active());
    }
    {
        GoldSrcFragmentReassembler limited(3u, 2u, 2u, 1s);
        assert(
            limited.Process(
                1u,
                Descriptor(1u, 2u, 2u),
                aa.data(),
                aa.size(),
                start)
            == GoldSrcFragmentProcessResult::kAccepted);
        assert(
            limited.Process(
                1u,
                Descriptor(2u, 2u, 2u),
                bb.data(),
                bb.size(),
                start)
            == GoldSrcFragmentProcessResult::kPayloadTooLarge);
        limited.Reset();
        assert(!limited.active());
    }
    {
        GoldSrcFragmentReassembler limited(8u, 2u, 2u, 1s);
        assert(
            limited.Process(
                1u,
                Descriptor(1u, 3u, 2u),
                aa.data(),
                aa.size(),
                start)
            == GoldSrcFragmentProcessResult::kFragmentCountExceeded);
    }
    {
        GoldSrcFragmentReassembler expiring(8u, 2u, 2u, 10ms);
        assert(
            expiring.Process(
                1u,
                Descriptor(1u, 2u, 2u),
                aa.data(),
                aa.size(),
                start)
            == GoldSrcFragmentProcessResult::kAccepted);
        assert(!expiring.Expire(start + 9ms));
        assert(expiring.Expire(start + 10ms));
        assert(!expiring.active());
        assert(expiring.received_fragment_count() == 0u);
    }
}

void TestNetchanFragmentSessionRetransmissionAndReset()
{
    const Ipv4Endpoint endpoint{{127u, 0u, 0u, 1u}, 27015u};
    const GoldSrcNetchanState::TimePoint start{};
    std::vector<std::uint8_t> payload(
        kGoldSrcNetchanMaximumReliableBytes + 1u);
    for (std::size_t index = 0u; index < payload.size(); ++index)
    {
        payload[index] = static_cast<std::uint8_t>((index * 17u) & 0xFFu);
    }
    const std::vector<std::uint8_t> original = payload;

    GoldSrcNetchanState state;
    assert(state.Initialize(endpoint, std::nullopt, 0u, start));
    assert(
        state.QueueReliablePayload(
            payload.data(),
            payload.size(),
            GoldSrcNetchanReliablePayloadKind::kResourceManifest)
        == GoldSrcNetchanQueueResult::kQueued);
    assert(state.fragment_sender().active());
    assert(state.fragment_sender().fragment_count() == 2u);
    payload.assign(payload.size(), 0xEEu);
    assert(
        state.QueueReliablePayload(
            original.data(),
            original.size(),
            GoldSrcNetchanReliablePayloadKind::kResourceManifest)
        == GoldSrcNetchanQueueResult::kReliableAlreadyPending);

    GoldSrcNetchanDatagram first{};
    assert(state.BuildOutgoingDatagram(&first));
    const GoldSrcNetchanDecodeResult first_decoded =
        DecodeGoldSrcNetchanDatagram(
            GoldSrcNetchanDirection::kServerToClient,
            first.bytes.data(),
            first.size);
    assert(first_decoded.ok());
    assert(first_decoded.packet.fragment_present);
    assert(first_decoded.packet.reliable_present);
    assert(first_decoded.packet.sequence == 1u);
    assert(first_decoded.packet.payload_size == 1024u);
    assert(std::equal(
        original.begin(),
        original.begin() + 1024,
        first_decoded.packet.payload.begin()));
    const std::vector<std::uint8_t> first_bytes =
        DecodedFragmentBytes(first);
    const std::uint32_t first_identity =
        first_decoded.packet.fragment_metadata
            .descriptors[0]
            .raw_fragment_id;
    const bool first_toggle = state.local_reliable_sequence();

    const auto before_future = state.fragment_sender().current_fragment_index();
    assert(
        state.ProcessIncomingDatagram(
            endpoint,
            Ack(1u, 50u, first_toggle),
            start + 1ms)
        == GoldSrcNetchanProcessResult::kFutureAck);
    assert(
        state.fragment_sender().current_fragment_index()
        == before_future);
    assert(state.fragment_sender().awaiting_acknowledgement());

    const GoldSrcNetchanProcessOutcome wrong =
        state.ProcessIncomingDatagramDetailed(
            endpoint,
            Ack(1u, 1u, !first_toggle),
            start + 2ms);
    assert(wrong.result == GoldSrcNetchanProcessResult::kAccepted);
    assert(!wrong.reliable_payload_was_acknowledged());
    assert(state.fragment_sender().awaiting_acknowledgement());
    assert(state.diagnostics().reliable_ack_mismatch == 1u);

    GoldSrcNetchanDatagram ordinary{};
    assert(state.BuildOutgoingDatagram(&ordinary));
    const GoldSrcNetchanDecodeResult ordinary_decoded =
        DecodeGoldSrcNetchanDatagram(
            GoldSrcNetchanDirection::kServerToClient,
            ordinary.bytes.data(),
            ordinary.size);
    assert(ordinary_decoded.ok());
    assert(!ordinary_decoded.packet.fragment_present);
    assert(!ordinary_decoded.packet.reliable_present);
    assert(ordinary_decoded.packet.sequence == 2u);

    assert(
        state.ProcessIncomingDatagram(
            endpoint,
            Ack(2u, 2u, !first_toggle),
            start + 3ms)
        == GoldSrcNetchanProcessResult::kAccepted);
    GoldSrcNetchanDatagram retransmitted{};
    assert(state.BuildOutgoingDatagram(&retransmitted));
    const GoldSrcNetchanDecodeResult retransmitted_decoded =
        DecodeGoldSrcNetchanDatagram(
            GoldSrcNetchanDirection::kServerToClient,
            retransmitted.bytes.data(),
            retransmitted.size);
    assert(retransmitted_decoded.ok());
    assert(retransmitted_decoded.packet.fragment_present);
    assert(retransmitted_decoded.packet.sequence == 3u);
    assert(
        retransmitted_decoded.packet.fragment_metadata
            .descriptors[0]
            .raw_fragment_id
        == first_identity);
    assert(DecodedFragmentBytes(retransmitted) == first_bytes);
    assert(state.local_reliable_sequence() == first_toggle);
    assert(state.fragment_sender().resend_count() == 1u);

    const GoldSrcNetchanProcessOutcome early =
        state.ProcessIncomingDatagramDetailed(
            endpoint,
            Ack(3u, 3u, first_toggle),
            start + 4ms);
    assert(early.result == GoldSrcNetchanProcessResult::kAccepted);
    assert(!early.reliable_payload_was_acknowledged());
    assert(state.fragment_sender().current_fragment_index() == 1u);
    assert(state.fragment_sender().needs_fragment_staging());

    GoldSrcNetchanDatagram final_fragment{};
    assert(state.BuildOutgoingDatagram(&final_fragment));
    const GoldSrcNetchanDecodeResult final_decoded =
        DecodeGoldSrcNetchanDatagram(
            GoldSrcNetchanDirection::kServerToClient,
            final_fragment.bytes.data(),
            final_fragment.size);
    assert(final_decoded.ok());
    assert(final_decoded.packet.fragment_present);
    assert(final_decoded.packet.sequence == 4u);
    assert(
        final_decoded.packet.fragment_metadata
            .descriptors[0]
            .fragment_index
        == 2u);
    assert(final_decoded.packet.payload_size == original.size() - 1024u);
    assert(state.local_reliable_sequence() != first_toggle);

    const GoldSrcNetchanProcessOutcome completed =
        state.ProcessIncomingDatagramDetailed(
            endpoint,
            Ack(4u, 4u, state.local_reliable_sequence()),
            start + 5ms);
    assert(completed.result == GoldSrcNetchanProcessResult::kAccepted);
    assert(completed.reliable_payload_was_acknowledged());
    assert(
        completed.acknowledged_reliable_kind
        == GoldSrcNetchanReliablePayloadKind::kResourceManifest);
    assert(completed.reliable_acknowledgement_generation == 1u);
    assert(state.fragment_sender().completion_acknowledged());
    assert(
        state.fragment_sender().phase()
        == GoldSrcFragmentTransferPhase::kCompleted);
    assert(!state.fragment_sender().active());
    assert(!state.reliable_pending());
    assert(state.diagnostics().reliable_acked == 2u);

    state.Reset();
    assert(!state.initialized());
    assert(
        state.fragment_sender().phase()
        == GoldSrcFragmentTransferPhase::kNone);
    assert(state.fragment_sender().fragment_count() == 0u);
    assert(state.fragment_sender().diagnostics().transfers_planned == 0u);
    assert(state.Initialize(endpoint, std::nullopt, 0u, start + 6ms));
    assert(!state.fragment_sender().active());
    assert(state.pending_reliable_kind()
        == GoldSrcNetchanReliablePayloadKind::kNone);
}

void TestTimeoutInboundRejectionAndSequenceWrap()
{
    const Ipv4Endpoint endpoint{{127u, 0u, 0u, 1u}, 27016u};
    const GoldSrcNetchanState::TimePoint start{};
    std::vector<std::uint8_t> payload(2048u, 0xA5u);
    GoldSrcNetchanState state;
    assert(state.Initialize(endpoint, std::nullopt, 0u, start));
    assert(
        state.QueueReliablePayload(
            payload.data(),
            payload.size(),
            GoldSrcNetchanReliablePayloadKind::kResourceManifest)
        == GoldSrcNetchanQueueResult::kQueued);
    GoldSrcNetchanDatagram first{};
    assert(state.BuildOutgoingDatagram(&first));
    const auto last_activity = state.last_accepted_at();

    GoldSrcNetchanPacket inbound_fragment =
        Ack(1u, 1u, state.local_reliable_sequence());
    inbound_fragment.fragment_present = true;
    inbound_fragment.reliable_present = true;
    assert(
        state.ProcessIncomingDatagram(
            endpoint,
            inbound_fragment,
            start + 1s)
        == GoldSrcNetchanProcessResult::kUnsupportedFragment);
    assert(state.last_accepted_at() == last_activity);
    assert(state.fragment_sender().awaiting_acknowledgement());

    const auto expiry_baseline =
        GoldSrcNetchanState::Clock::now();
    assert(!state.ExpireFragmentTransfer(expiry_baseline + 29s));
    assert(state.ExpireFragmentTransfer(expiry_baseline + 31s));
    assert(
        state.fragment_sender().phase()
        == GoldSrcFragmentTransferPhase::kFailed);
    assert(!state.reliable_pending());
    assert(state.fragment_sender().fragment_count() == 0u);
    assert(state.fragment_sender().transfer_generation() == 0u);
    assert(state.fragment_sender().diagnostics().transfers_expired == 1u);

    GoldSrcFragmentSender sender;
    assert(
        sender.Plan(payload.data(), payload.size(), 4u, start)
        == GoldSrcFragmentPlanResult::kPlanned);
    assert(sender.RecordSent(
        kGoldSrcNetchanSequenceMask,
        true,
        false,
        start));
    assert(sender.AcknowledgeCurrent(start + 1ms));
    assert(sender.RecordSent(0u, false, false, start + 2ms));
    assert(sender.first_outer_sequence() == kGoldSrcNetchanSequenceMask);
    assert(sender.latest_outer_sequence() == 0u);
    assert(!sender.reliable_toggle());
    assert(
        NextGoldSrcNetchanSequence(kGoldSrcNetchanSequenceMask)
        == 0u);
    assert(
        GoldSrcNetchanSequenceDistance(
            0u,
            kGoldSrcNetchanSequenceMask)
        == 1u);
    assert(IsGoldSrcNetchanSequenceNewer(
        0u,
        kGoldSrcNetchanSequenceMask));

    GoldSrcNetchanDatagram before_wrap{};
    GoldSrcNetchanDatagram after_wrap{};
    const GoldSrcFragmentDescriptor first_descriptor =
        Descriptor(1u, 2u, 1024u);
    const GoldSrcFragmentDescriptor second_descriptor =
        Descriptor(2u, 2u, 1024u);
    assert(
        EncodeGoldSrcFragmentDatagram(
            GoldSrcNetchanDirection::kServerToClient,
            kGoldSrcNetchanSequenceMask,
            kGoldSrcNetchanSequenceMask - 1u,
            true,
            first_descriptor,
            payload.data(),
            1024u,
            &before_wrap)
        == GoldSrcNetchanCodecStatus::kOk);
    assert(
        EncodeGoldSrcFragmentDatagram(
            GoldSrcNetchanDirection::kServerToClient,
            0u,
            kGoldSrcNetchanSequenceMask,
            false,
            second_descriptor,
            payload.data() + 1024u,
            1024u,
            &after_wrap)
        == GoldSrcNetchanCodecStatus::kOk);
    assert(
        DecodeGoldSrcNetchanDatagram(
            GoldSrcNetchanDirection::kServerToClient,
            before_wrap.bytes.data(),
            before_wrap.size).packet.sequence
        == kGoldSrcNetchanSequenceMask);
    assert(
        DecodeGoldSrcNetchanDatagram(
            GoldSrcNetchanDirection::kServerToClient,
            after_wrap.bytes.data(),
            after_wrap.size).packet.sequence
        == 0u);

    GoldSrcNetchanState active_reset;
    assert(active_reset.Initialize(endpoint, std::nullopt, 9u, start));
    assert(
        active_reset.QueueReliablePayload(
            payload.data(),
            payload.size(),
            GoldSrcNetchanReliablePayloadKind::kResourceManifest)
        == GoldSrcNetchanQueueResult::kQueued);
    GoldSrcNetchanDatagram active_fragment{};
    assert(active_reset.BuildOutgoingDatagram(&active_fragment));
    assert(active_reset.fragment_sender().active());
    assert(active_reset.reliable_pending());
    active_reset.Reset();
    assert(!active_reset.initialized());
    assert(!active_reset.fragment_sender().active());
    assert(
        active_reset.fragment_sender().phase()
        == GoldSrcFragmentTransferPhase::kNone);
    assert(active_reset.fragment_sender().fragment_count() == 0u);
    assert(active_reset.reliable_pending_bytes() == 0u);
    assert(
        active_reset.pending_reliable_kind()
        == GoldSrcNetchanReliablePayloadKind::kNone);
    assert(active_reset.Initialize(endpoint, std::nullopt, 10u, start));
    assert(!active_reset.fragment_sender().active());
}
} // namespace

int main()
{
    TestStableConstantsAndReasons();
    TestFragmentMetadataCodecAndBounds();
    TestFragmentDatagramFlagsTransformAndGoldenShape();
    TestFragmentPlanner();
    TestOrderedOutOfOrderAndDuplicateReassembly();
    TestReassemblerMissingConflictIdentityAndLimits();
    TestNetchanFragmentSessionRetransmissionAndReset();
    TestTimeoutInboundRejectionAndSequenceWrap();
    return 0;
}
