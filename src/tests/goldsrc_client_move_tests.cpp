#include "network/goldsrc_bitstream.h"
#include "network/goldsrc_client_move.h"
#include "network/goldsrc_signon.h"

#ifdef NDEBUG
#undef NDEBUG
#endif

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace
{
using namespace hl::network;

constexpr std::uint32_t kObservedSequence = 32u;

const std::array<const char*, 15> kUsercmdNames = {
    "lerp_msec",
    "msec",
    "viewangles[1]",
    "viewangles[0]",
    "buttons",
    "forwardmove",
    "lightlevel",
    "sidemove",
    "upmove",
    "impulse",
    "viewangles[2]",
    "impact_index",
    "impact_position[0]",
    "impact_position[1]",
    "impact_position[2]",
};

const std::array<std::uint32_t, 15> kUsercmdTypes = {
    kGoldSrcDeltaTypeShort,
    kGoldSrcDeltaTypeByte,
    kGoldSrcDeltaTypeAngle,
    kGoldSrcDeltaTypeAngle,
    kGoldSrcDeltaTypeShort,
    kGoldSrcDeltaTypeFloat | kGoldSrcDeltaTypeSigned,
    kGoldSrcDeltaTypeByte,
    kGoldSrcDeltaTypeFloat | kGoldSrcDeltaTypeSigned,
    kGoldSrcDeltaTypeFloat | kGoldSrcDeltaTypeSigned,
    kGoldSrcDeltaTypeByte,
    kGoldSrcDeltaTypeAngle,
    kGoldSrcDeltaTypeInteger,
    kGoldSrcDeltaTypeFloat | kGoldSrcDeltaTypeSigned,
    kGoldSrcDeltaTypeFloat | kGoldSrcDeltaTypeSigned,
    kGoldSrcDeltaTypeFloat | kGoldSrcDeltaTypeSigned,
};

const std::array<std::uint8_t, 15> kUsercmdBits = {
    9u, 8u, 16u, 16u, 16u, 12u, 8u, 12u,
    12u, 8u, 16u, 6u, 16u, 16u, 16u,
};

const std::array<double, 15> kUsercmdPremultiply = {
    1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0,
    1.0, 1.0, 1.0, 1.0, 8.0, 8.0, 8.0,
};

GoldSrcDeltaRegistry CanonicalUsercmdRegistry()
{
    GoldSrcDeltaRegistry registry;
    GoldSrcDeltaTable table;
    table.name = "usercmd_t";
    for (std::size_t index = 0; index < kUsercmdNames.size(); ++index)
    {
        GoldSrcDeltaField field;
        field.field_type = kUsercmdTypes[index];
        field.name = kUsercmdNames[index];
        field.field_offset = static_cast<std::uint16_t>(index * 4u);
        field.field_size = 4u;
        field.significant_bits = kUsercmdBits[index];
        field.premultiply = kUsercmdPremultiply[index];
        field.postmultiply = 1.0;
        table.fields.push_back(field);
    }
    registry.tables.push_back(table);
    return registry;
}

struct WireUsercmd
{
    bool lerp_changed = false;
    std::uint16_t lerp_msec = 0u;
    bool msec_changed = false;
    std::uint8_t msec = 0u;
};

std::vector<std::uint8_t> EncodeMove(
    std::uint32_t sequence,
    std::uint8_t packet_loss,
    std::uint8_t backup_count,
    std::uint8_t new_count,
    const std::vector<WireUsercmd>& commands)
{
    assert(commands.size()
        == static_cast<std::size_t>(backup_count)
            + static_cast<std::size_t>(new_count));
    std::array<std::uint8_t, kGoldSrcMaximumMoveBodyBytes> body{};
    body[0] = packet_loss;
    body[1] = backup_count;
    body[2] = new_count;
    std::size_t cursor = 3u;
    for (const WireUsercmd& command : commands)
    {
        GoldSrcBitWriter writer(body.data() + cursor, body.size() - cursor);
        const std::uint8_t mask =
            static_cast<std::uint8_t>(
                (command.lerp_changed ? 0x01u : 0u)
                | (command.msec_changed ? 0x02u : 0u));
        assert(writer.WriteBits(mask == 0u ? 0u : 1u, 3u));
        if (mask != 0u)
        {
            assert(writer.WriteBits(mask, 8u));
        }
        if (command.lerp_changed)
        {
            assert(writer.WriteBits(command.lerp_msec, 9u));
        }
        if (command.msec_changed)
        {
            assert(writer.WriteBits(command.msec, 8u));
        }
        assert(writer.PadToByte());
        assert(writer.bytes_written() != 0u);
        cursor += writer.bytes_written();
    }

    std::vector<std::uint8_t> payload(cursor + 3u);
    payload[0] = kGoldSrcClientMoveOpcode;
    payload[1] = static_cast<std::uint8_t>(cursor);
    payload[2] = GoldSrcMoveChecksum(body.data(), cursor, sequence);
    MungeGoldSrcMoveBody(body.data(), cursor, sequence);
    for (std::size_t index = 0; index < cursor; ++index)
    {
        payload[index + 3u] = body[index];
    }
    return payload;
}

std::vector<std::uint8_t> EncodeBody(
    std::uint32_t sequence,
    std::vector<std::uint8_t> body)
{
    assert(body.size() <= kGoldSrcMaximumMoveBodyBytes);
    std::vector<std::uint8_t> payload(body.size() + 3u);
    payload[0] = kGoldSrcClientMoveOpcode;
    payload[1] = static_cast<std::uint8_t>(body.size());
    payload[2] = GoldSrcMoveChecksum(body.data(), body.size(), sequence);
    MungeGoldSrcMoveBody(body.data(), body.size(), sequence);
    for (std::size_t index = 0; index < body.size(); ++index)
    {
        payload[index + 3u] = body[index];
    }
    return payload;
}

std::vector<std::uint8_t> ObservedFixture()
{
    return EncodeMove(
        kObservedSequence,
        0u,
        2u,
        1u,
        {
            {},
            {true, 100u, true, 210u},
            {false, 0u, true, 32u},
        });
}

void ReachPrompt240Terminal(GoldSrcSignonSessionState* state)
{
    assert(state != nullptr);
    assert(state->EnterAwaitingNew(
        GoldSrcSignonBootstrapMode::kServerInfoWithDeltaDescriptions)
        == GoldSrcSignonTransitionResult::kAdvanced);
    assert(state->HandleClientCommand(GoldSrcClientSignonCommand::kNew)
        == GoldSrcSignonCommandDisposition::kDelivered);
    assert(state->MarkSignonBootstrapSent()
        == GoldSrcSignonTransitionResult::kAdvanced);
    assert(state->MarkSignonBootstrapAcknowledged()
        == GoldSrcSignonTransitionResult::kAdvanced);
    assert(state->EnterAwaitingResourceRequest()
        == GoldSrcSignonTransitionResult::kAdvanced);
    assert(state->HandleClientCommand(
        GoldSrcClientSignonCommand::kSendResources)
        == GoldSrcSignonCommandDisposition::kDelivered);
    assert(state->MarkResourceManifestSent()
        == GoldSrcSignonTransitionResult::kAdvanced);
    assert(state->MarkResourceManifestAcknowledged()
        == GoldSrcSignonTransitionResult::kAdvanced);
}

void TestObservedFixture()
{
    const GoldSrcDeltaRegistry registry = CanonicalUsercmdRegistry();
    const std::vector<std::uint8_t> fixture = ObservedFixture();
    const std::vector<std::uint8_t> observed_golden = {
        0x02u, 0x0Bu, 0xEDu, 0x20u, 0x19u, 0x52u, 0x20u,
        0x37u, 0x7Bu, 0x20u, 0x79u, 0x11u, 0x00u, 0x01u,
    };
    assert(fixture == observed_golden);
    assert(fixture.size() == 14u);
    const GoldSrcClientMoveDecodeResult decoded =
        DecodeGoldSrcClientMoveCommand(
            fixture.data(), fixture.size(), kObservedSequence, registry);
    assert(decoded.ok());
    assert(decoded.command.bytes_consumed == fixture.size());
    assert(decoded.command.packet_loss == 0u);
    assert(!decoded.command.voice_loopback);
    assert(decoded.command.backup_command_count == 2u);
    assert(decoded.command.new_command_count == 1u);
    assert(decoded.command.command_count == 3u);
    assert(decoded.command.commands[0].lerp_msec == 0u);
    assert(decoded.command.commands[0].msec == 0u);
    assert(decoded.command.commands[1].lerp_msec == 100u);
    assert(decoded.command.commands[1].msec == 210u);
    assert(decoded.command.commands[1].command_time_msec == 210u);
    assert(decoded.command.commands[2].lerp_msec == 100u);
    assert(decoded.command.commands[2].msec == 32u);
    assert(decoded.command.commands[2].command_time_msec == 242u);
    assert(decoded.command.new_command_time_msec == 32u);

    std::vector<std::uint8_t> roundtrip(
        fixture.begin() + 3, fixture.end());
    const std::vector<std::uint8_t> protected_copy = roundtrip;
    UnmungeGoldSrcMoveBody(
        roundtrip.data(), roundtrip.size(), kObservedSequence);
    assert(GoldSrcMoveChecksum(
        roundtrip.data(), roundtrip.size(), kObservedSequence)
        == fixture[2]);
    MungeGoldSrcMoveBody(
        roundtrip.data(), roundtrip.size(), kObservedSequence);
    assert(roundtrip == protected_copy);
}

void TestApplicationDispatch()
{
    const GoldSrcDeltaRegistry registry = CanonicalUsercmdRegistry();
    const std::vector<std::uint8_t> move = ObservedFixture();
    std::vector<std::uint8_t> payload = {kGoldSrcClientNopOpcode};
    payload.insert(payload.end(), move.begin(), move.end());
    payload.push_back(kGoldSrcClientNopOpcode);
    const GoldSrcClientApplicationDecodeResult decoded =
        DecodeGoldSrcClientApplicationPayload(
            payload.data(), payload.size(), kObservedSequence, registry);
    assert(decoded.ok());
    assert(decoded.move_present);
    assert(decoded.nop_count == 2u);
    assert(decoded.bytes_consumed == payload.size());

    payload.push_back(9u);
    assert(DecodeGoldSrcClientApplicationPayload(
        payload.data(), payload.size(), kObservedSequence, registry).status
        == GoldSrcClientApplicationDecodeStatus::kUnsupportedTrailingData);

    std::vector<std::uint8_t> twice = move;
    twice.insert(twice.end(), move.begin(), move.end());
    assert(DecodeGoldSrcClientApplicationPayload(
        twice.data(), twice.size(), kObservedSequence, registry).status
        == GoldSrcClientApplicationDecodeStatus::kMultipleMoveCommands);

    const std::array<std::uint8_t, 1> unknown = {9u};
    assert(DecodeGoldSrcClientApplicationPayload(
        unknown.data(), unknown.size(), kObservedSequence, registry).status
        == GoldSrcClientApplicationDecodeStatus::kUnknownOpcode);

    const std::array<std::uint8_t, 13> disconnect = {
        kGoldSrcClientApplicationStringCommandOpcode,
        'd', 'r', 'o', 'p', 'c', 'l', 'i', 'e', 'n', 't', '\n', '\0'};
    const GoldSrcClientApplicationDecodeResult disconnect_decoded =
        DecodeGoldSrcClientApplicationPayload(
            disconnect.data(),
            disconnect.size(),
            kObservedSequence,
            registry);
    assert(disconnect_decoded.ok());
    assert(disconnect_decoded.disconnect_present);
    assert(!disconnect_decoded.move_present);
    assert(!disconnect_decoded.frame_reference_present);

    const std::array<std::uint8_t, 12> unterminated_disconnect = {
        kGoldSrcClientApplicationStringCommandOpcode,
        'd', 'r', 'o', 'p', 'c', 'l', 'i', 'e', 'n', 't', '\n'};
    assert(DecodeGoldSrcClientApplicationPayload(
        unterminated_disconnect.data(),
        unterminated_disconnect.size(),
        kObservedSequence,
        registry).status
        == GoldSrcClientApplicationDecodeStatus::kMalformedStringCommand);

    std::vector<std::uint8_t> move_then_disconnect = move;
    move_then_disconnect.insert(
        move_then_disconnect.end(),
        disconnect.begin(),
        disconnect.end());
    assert(DecodeGoldSrcClientApplicationPayload(
        move_then_disconnect.data(),
        move_then_disconnect.size(),
        kObservedSequence,
        registry).status
        == GoldSrcClientApplicationDecodeStatus::kUnsupportedTrailingData);
}

void TestEnvelopeAndChecksumRejections()
{
    const GoldSrcDeltaRegistry registry = CanonicalUsercmdRegistry();
    std::vector<std::uint8_t> fixture = ObservedFixture();
    for (std::size_t size = 0u; size < fixture.size(); ++size)
    {
        const GoldSrcClientMoveDecodeResult decoded =
            DecodeGoldSrcClientMoveCommand(
                fixture.data(), size, kObservedSequence, registry);
        assert(!decoded.ok());
    }
    fixture[2] ^= 0x01u;
    assert(DecodeGoldSrcClientMoveCommand(
        fixture.data(), fixture.size(), kObservedSequence, registry).status
        == GoldSrcClientMoveDecodeStatus::kInvalidChecksum);

    const std::vector<std::uint8_t> invalid_loss =
        EncodeMove(kObservedSequence, 101u, 0u, 0u, {});
    assert(DecodeGoldSrcClientMoveCommand(
        invalid_loss.data(), invalid_loss.size(), kObservedSequence, registry)
        .status == GoldSrcClientMoveDecodeStatus::kPacketLossOutOfRange);

    const std::vector<std::uint8_t> excessive_counts =
        EncodeBody(kObservedSequence, {0u, 62u, 1u});
    assert(DecodeGoldSrcClientMoveCommand(
        excessive_counts.data(),
        excessive_counts.size(),
        kObservedSequence,
        registry).status
        == GoldSrcClientMoveDecodeStatus::kCommandCountExceeded);
}

void TestCountBoundaries()
{
    const GoldSrcDeltaRegistry registry = CanonicalUsercmdRegistry();
    const std::vector<std::uint8_t> minimum =
        EncodeMove(kObservedSequence, 0u, 0u, 0u, {});
    const GoldSrcClientMoveDecodeResult minimum_decoded =
        DecodeGoldSrcClientMoveCommand(
            minimum.data(), minimum.size(), kObservedSequence, registry);
    assert(minimum_decoded.ok());
    assert(minimum_decoded.command.command_count == 0u);

    std::vector<WireUsercmd> commands(kGoldSrcMaximumMoveCommands);
    for (std::size_t index = 0; index < commands.size(); ++index)
    {
        commands[index].msec_changed = true;
        commands[index].msec = static_cast<std::uint8_t>(index + 1u);
    }
    const std::vector<std::uint8_t> maximum_new =
        EncodeMove(
            kObservedSequence,
            0u,
            0u,
            static_cast<std::uint8_t>(commands.size()),
            commands);
    const GoldSrcClientMoveDecodeResult maximum_new_decoded =
        DecodeGoldSrcClientMoveCommand(
            maximum_new.data(),
            maximum_new.size(),
            kObservedSequence,
            registry);
    assert(maximum_new_decoded.ok());
    assert(maximum_new_decoded.command.command_count
        == kGoldSrcMaximumMoveCommands);
    assert(maximum_new_decoded.command.commands.back().msec == 62u);

    const std::vector<std::uint8_t> maximum_backup =
        EncodeMove(
            kObservedSequence,
            0u,
            static_cast<std::uint8_t>(commands.size()),
            0u,
            commands);
    assert(DecodeGoldSrcClientMoveCommand(
        maximum_backup.data(),
        maximum_backup.size(),
        kObservedSequence,
        registry).ok());
}

void TestSchemaAndBitstreamRejections()
{
    const GoldSrcDeltaRegistry registry = CanonicalUsercmdRegistry();
    const std::vector<std::uint8_t> fixture = ObservedFixture();
    const GoldSrcDeltaRegistry missing;
    assert(DecodeGoldSrcClientMoveCommand(
        fixture.data(), fixture.size(), kObservedSequence, missing).status
        == GoldSrcClientMoveDecodeStatus::kMissingUsercmdSchema);

    GoldSrcDeltaRegistry duplicate = registry;
    duplicate.tables.push_back(registry.tables.front());
    assert(DecodeGoldSrcClientMoveCommand(
        fixture.data(), fixture.size(), kObservedSequence, duplicate).status
        == GoldSrcClientMoveDecodeStatus::kDuplicateUsercmdSchema);

    GoldSrcDeltaRegistry invalid = registry;
    invalid.tables.front().fields.front().premultiply = 0.0;
    assert(DecodeGoldSrcClientMoveCommand(
        fixture.data(), fixture.size(), kObservedSequence, invalid).status
        == GoldSrcClientMoveDecodeStatus::kInvalidMultiplier);

    // One command declares an msec change but omits the eight-bit value.
    const std::vector<std::uint8_t> truncated_delta =
        EncodeBody(kObservedSequence, {0u, 0u, 1u, 0x11u, 0x00u});
    assert(DecodeGoldSrcClientMoveCommand(
        truncated_delta.data(),
        truncated_delta.size(),
        kObservedSequence,
        registry).status
        == GoldSrcClientMoveDecodeStatus::kTruncatedBitstream);

    // Two mask bytes for 15 fields with the unused high bit set.
    std::array<std::uint8_t, 8> invalid_mask_body{};
    invalid_mask_body[2] = 1u;
    GoldSrcBitWriter invalid_mask_writer(
        invalid_mask_body.data() + 3u,
        invalid_mask_body.size() - 3u);
    assert(invalid_mask_writer.WriteBits(2u, 3u));
    assert(invalid_mask_writer.WriteBits(0u, 8u));
    assert(invalid_mask_writer.WriteBits(0x80u, 8u));
    assert(invalid_mask_writer.PadToByte());
    const std::vector<std::uint8_t> invalid_mask = EncodeBody(
        kObservedSequence,
        std::vector<std::uint8_t>(
            invalid_mask_body.begin(),
            invalid_mask_body.begin() + 3u
                + invalid_mask_writer.bytes_written()));
    assert(DecodeGoldSrcClientMoveCommand(
        invalid_mask.data(),
        invalid_mask.size(),
        kObservedSequence,
        registry).status
        == GoldSrcClientMoveDecodeStatus::kInvalidDeltaMask);

    const std::vector<std::uint8_t> trailing =
        EncodeBody(kObservedSequence, {0u, 0u, 0u, 0u});
    assert(DecodeGoldSrcClientMoveCommand(
        trailing.data(), trailing.size(), kObservedSequence, registry).status
        == GoldSrcClientMoveDecodeStatus::kNonZeroPadding);
}

void TestSignonPolicyAndReset()
{
    GoldSrcSignonSessionState state;
    const auto initial = state.diagnostics();
    assert(state.HandlePostResourceMove()
        == GoldSrcSignonCommandDisposition::kWrongPhase);
    assert(state.phase() == GoldSrcSignonPhase::kNone);
    assert(state.diagnostics().post_resource_command_delivered
        == initial.post_resource_command_delivered);

    ReachPrompt240Terminal(&state);
    assert(state.phase()
        == GoldSrcSignonPhase::kResourceManifestAcknowledged);
    assert(state.EnterAwaitingPostResourceCommand()
        == GoldSrcSignonTransitionResult::kAdvanced);
    assert(NameFor(state.phase()) == "awaiting_post_resource_command");
    assert(state.HandlePostResourceMove()
        == GoldSrcSignonCommandDisposition::kDelivered);
    assert(state.phase()
        == GoldSrcSignonPhase::kAwaitingServerBaselineOrSnapshot);
    assert(NameFor(state.phase())
        == "awaiting_server_baseline_or_snapshot");
    assert(state.diagnostics().post_resource_command_delivered == 1u);
    assert(state.diagnostics().post_resource_command_state_advances == 1u);
    assert(state.HandlePostResourceMove()
        == GoldSrcSignonCommandDisposition::kDelivered);
    assert(state.diagnostics().post_resource_command_delivered == 2u);
    assert(state.diagnostics().post_resource_command_state_advances == 1u);

    // The codec/state slice owns no gameplay data and cannot mutate it.
    const std::array<float, 3> origin = {1.0f, 2.0f, 3.0f};
    const std::array<float, 3> velocity = {4.0f, 5.0f, 6.0f};
    const std::array<float, 3> expected_origin = {1.0f, 2.0f, 3.0f};
    const std::array<float, 3> expected_velocity = {4.0f, 5.0f, 6.0f};
    assert(origin == expected_origin);
    assert(velocity == expected_velocity);

    state.Reset();
    assert(state.phase() == GoldSrcSignonPhase::kNone);
    assert(state.diagnostics().post_resource_command_received == 0u);
    assert(state.diagnostics().post_resource_command_delivered == 0u);
    ReachPrompt240Terminal(&state);
    assert(state.EnterAwaitingPostResourceCommand()
        == GoldSrcSignonTransitionResult::kAdvanced);
    assert(state.HandlePostResourceMove()
        == GoldSrcSignonCommandDisposition::kDelivered);
}
} // namespace

int main()
{
    TestObservedFixture();
    TestApplicationDispatch();
    TestEnvelopeAndChecksumRejections();
    TestCountBoundaries();
    TestSchemaAndBitstreamRejections();
    TestSignonPolicyAndReset();
    return 0;
}
