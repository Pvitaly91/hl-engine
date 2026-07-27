#include "network/goldsrc_signon.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace
{
using namespace hl::network;

void WriteLittleEndian32(
    std::vector<std::uint8_t>* bytes,
    std::size_t offset,
    std::uint32_t value)
{
    assert(bytes != nullptr);
    assert(offset <= bytes->size());
    assert(bytes->size() - offset >= 4u);
    (*bytes)[offset] = static_cast<std::uint8_t>(value & 0xFFu);
    (*bytes)[offset + 1u] =
        static_cast<std::uint8_t>((value >> 8u) & 0xFFu);
    (*bytes)[offset + 2u] =
        static_cast<std::uint8_t>((value >> 16u) & 0xFFu);
    (*bytes)[offset + 3u] =
        static_cast<std::uint8_t>((value >> 24u) & 0xFFu);
}

std::vector<std::uint8_t> Bytes(std::string_view value)
{
    return std::vector<std::uint8_t>(value.begin(), value.end());
}

std::filesystem::path UniqueTemporaryPath(std::string_view extension)
{
    const auto serial = std::chrono::high_resolution_clock::now()
                            .time_since_epoch()
                            .count();
    return std::filesystem::temp_directory_path()
        / ("hlhost_goldsrc_signon_" + std::to_string(serial)
            + std::string(extension));
}

GoldSrcServerInfoContext GoldenContext()
{
    GoldSrcServerInfoContext context{};
    context.protocol_version = 48u;
    context.spawn_count = 7u;
    context.canonical_map_checksum = 0x12345678u;
    context.client_dll_md5 = std::array<std::uint8_t, 16>{
        0xE9u, 0x70u, 0xC0u, 0xD0u,
        0x7Eu, 0x35u, 0x5Du, 0x80u,
        0x4Fu, 0x4Au, 0x42u, 0x65u,
        0x88u, 0x24u, 0x0Cu, 0x0Bu,
    };
    context.max_clients = 2u;
    context.player_index = 0u;
    context.deathmatch = true;
    context.game_directory = "valve";
    context.hostname = "HL Test";
    context.map_model_path = "maps/c1a0.bsp";
    context.mapcycle.clear();
    context.secure = false;
    context.fallback_game_directory.clear();
    context.cheats = false;
    return context;
}

// This fixture is deliberately spelled out independently of the encoder. It
// is the protocol-48 svc_serverinfo grammar followed by svc_sendextrainfo.
std::vector<std::uint8_t> GoldenServerInfoFixture()
{
    return {
        0x0Bu,
        0x30u, 0x00u, 0x00u, 0x00u,
        0x07u, 0x00u, 0x00u, 0x00u,
        0xB7u, 0x6Cu, 0x16u, 0x87u,
        0xE9u, 0x70u, 0xC0u, 0xD0u,
        0x7Eu, 0x35u, 0x5Du, 0x80u,
        0x4Fu, 0x4Au, 0x42u, 0x65u,
        0x88u, 0x24u, 0x0Cu, 0x0Bu,
        0x02u, 0x00u, 0x01u,
        'v', 'a', 'l', 'v', 'e', 0x00u,
        'H', 'L', ' ', 'T', 'e', 's', 't', 0x00u,
        'm', 'a', 'p', 's', '/', 'c', '1', 'a', '0', '.', 'b', 's', 'p', 0x00u,
        0x00u,
        0x00u,
        0x36u, 0x00u, 0x00u,
    };
}

void TestStrictClientNewDecoder()
{
    constexpr std::array<std::uint8_t, 5> exact = {
        0x03u, 0x6Eu, 0x65u, 0x77u, 0x00u,
    };
    const GoldSrcClientSignonDecodeResult decoded =
        DecodeGoldSrcClientSignonPayload(exact.data(), exact.size());
    assert(decoded.ok());
    assert(decoded.command == GoldSrcClientSignonCommand::kNew);
    assert(
        decoded.companion_command
        == GoldSrcClientSignonCompanionCommand::kNone);
    assert(decoded.companion_count == 0u);
    assert(decoded.leading_nop_count == 0u);
    assert(decoded.trailing_nop_count == 0u);

    constexpr std::array<std::uint8_t, 9> padded = {
        0x01u, 0x01u,
        0x03u, 0x6Eu, 0x65u, 0x77u, 0x00u,
        0x01u, 0x01u,
    };
    const GoldSrcClientSignonDecodeResult decoded_padded =
        DecodeGoldSrcClientSignonPayload(padded.data(), padded.size());
    assert(decoded_padded.ok());
    assert(decoded_padded.leading_nop_count == 2u);
    assert(decoded_padded.trailing_nop_count == 2u);

    constexpr std::array<std::uint8_t, 9> exact_send_resources = {
        0x03u, 's', 'e', 'n', 'd', 'r', 'e', 's', 0x00u,
    };
    const GoldSrcClientSignonDecodeResult decoded_send_resources =
        DecodeGoldSrcClientSignonPayload(
            exact_send_resources.data(),
            exact_send_resources.size());
    assert(decoded_send_resources.ok());
    assert(
        decoded_send_resources.command
        == GoldSrcClientSignonCommand::kSendResources);
    assert(decoded_send_resources.leading_nop_count == 0u);
    assert(decoded_send_resources.trailing_nop_count == 0u);
    assert(
        decoded_send_resources.companion_command
        == GoldSrcClientSignonCompanionCommand::kNone);
    assert(decoded_send_resources.companion_count == 0u);

    constexpr std::array<std::uint8_t, 11> padded_send_resources = {
        0x01u,
        0x03u, 's', 'e', 'n', 'd', 'r', 'e', 's', 0x00u,
        0x01u,
    };
    const GoldSrcClientSignonDecodeResult decoded_padded_send_resources =
        DecodeGoldSrcClientSignonPayload(
            padded_send_resources.data(),
            padded_send_resources.size());
    assert(decoded_padded_send_resources.ok());
    assert(
        decoded_padded_send_resources.command
        == GoldSrcClientSignonCommand::kSendResources);
    assert(decoded_padded_send_resources.leading_nop_count == 1u);
    assert(decoded_padded_send_resources.trailing_nop_count == 1u);
    assert(decoded_padded_send_resources.companion_count == 0u);

    constexpr std::array<std::uint8_t, 13> exact_disconnect = {
        0x03u,
        'd', 'r', 'o', 'p', 'c', 'l', 'i', 'e', 'n', 't', '\n',
        0x00u,
    };
    const GoldSrcClientSignonDecodeResult decoded_disconnect =
        DecodeGoldSrcClientSignonPayload(
            exact_disconnect.data(),
            exact_disconnect.size());
    assert(decoded_disconnect.ok());
    assert(
        decoded_disconnect.command
        == GoldSrcClientSignonCommand::kDisconnect);
    assert(decoded_disconnect.companion_count == 0u);

    constexpr std::array<std::uint8_t, 12> missing_disconnect_line_feed = {
        0x03u,
        'd', 'r', 'o', 'p', 'c', 'l', 'i', 'e', 'n', 't',
        0x00u,
    };
    assert(
        DecodeGoldSrcClientSignonPayload(
            missing_disconnect_line_feed.data(),
            missing_disconnect_line_feed.size()).status
        == GoldSrcClientSignonDecodeStatus::kUnsupportedCommand);
    constexpr std::array<std::uint8_t, 12> invented_disconnect = {
        0x03u,
        'd', 'i', 's', 'c', 'o', 'n', 'n', 'e', 'c', 't',
        0x00u,
    };
    assert(
        DecodeGoldSrcClientSignonPayload(
            invented_disconnect.data(),
            invented_disconnect.size()).status
        == GoldSrcClientSignonDecodeStatus::kUnsupportedCommand);

    constexpr std::array<std::uint8_t, 37> observed_send_resources = {
        0x03u, 's', 'e', 'n', 'd', 'r', 'e', 's', 0x00u,
        0x03u, 'c', 'l', 'o', 's', 'e', 'm', 'e', 'n', 'u', 's',
            ' ', '\n', 0x00u,
        0x03u, 'c', 'l', 'o', 's', 'e', 'm', 'e', 'n', 'u', 's',
            ' ', '\n', 0x00u,
    };
    const GoldSrcClientSignonDecodeResult decoded_observed =
        DecodeGoldSrcClientSignonPayload(
            observed_send_resources.data(),
            observed_send_resources.size());
    assert(decoded_observed.ok());
    assert(
        decoded_observed.command
        == GoldSrcClientSignonCommand::kSendResources);
    assert(
        decoded_observed.companion_command
        == GoldSrcClientSignonCompanionCommand::kCloseMenus);
    assert(decoded_observed.companion_count == 2u);

    constexpr std::array<std::uint8_t, 41> observed_with_nops = {
        0x01u,
        0x03u, 's', 'e', 'n', 'd', 'r', 'e', 's', 0x00u,
        0x01u,
        0x03u, 'c', 'l', 'o', 's', 'e', 'm', 'e', 'n', 'u', 's',
            ' ', '\n', 0x00u,
        0x01u,
        0x03u, 'c', 'l', 'o', 's', 'e', 'm', 'e', 'n', 'u', 's',
            ' ', '\n', 0x00u,
        0x01u,
    };
    const GoldSrcClientSignonDecodeResult decoded_observed_with_nops =
        DecodeGoldSrcClientSignonPayload(
            observed_with_nops.data(),
            observed_with_nops.size());
    assert(decoded_observed_with_nops.ok());
    assert(decoded_observed_with_nops.leading_nop_count == 1u);
    assert(decoded_observed_with_nops.trailing_nop_count == 3u);
    assert(decoded_observed_with_nops.companion_count == 2u);

    constexpr std::array<std::uint8_t, 23> one_observed_companion = {
        0x03u, 's', 'e', 'n', 'd', 'r', 'e', 's', 0x00u,
        0x03u, 'c', 'l', 'o', 's', 'e', 'm', 'e', 'n', 'u', 's',
            ' ', '\n', 0x00u,
    };
    assert(
        DecodeGoldSrcClientSignonPayload(
            one_observed_companion.data(),
            one_observed_companion.size()).status
        == GoldSrcClientSignonDecodeStatus::kUnsupportedCompanionCount);

    assert(
        DecodeGoldSrcClientSignonPayload(nullptr, 1u).status
        == GoldSrcClientSignonDecodeStatus::kNullInput);
    assert(
        DecodeGoldSrcClientSignonPayload(nullptr, 0u).status
        == GoldSrcClientSignonDecodeStatus::kEmptyPayload);

    constexpr std::array<std::uint8_t, 2> only_nops = {1u, 1u};
    assert(
        DecodeGoldSrcClientSignonPayload(
            only_nops.data(),
            only_nops.size()).status
        == GoldSrcClientSignonDecodeStatus::kMissingCommand);

    constexpr std::array<std::uint8_t, 1> unknown_opcode = {2u};
    assert(
        DecodeGoldSrcClientSignonPayload(
            unknown_opcode.data(),
            unknown_opcode.size()).status
        == GoldSrcClientSignonDecodeStatus::kUnsupportedOpcode);

    constexpr std::array<std::uint8_t, 4> missing_nul = {
        3u, 'n', 'e', 'w',
    };
    assert(
        DecodeGoldSrcClientSignonPayload(
            missing_nul.data(),
            missing_nul.size()).status
        == GoldSrcClientSignonDecodeStatus::kMissingStringTerminator);

    constexpr std::array<std::uint8_t, 6> newline = {
        3u, 'n', 'e', 'w', '\n', 0u,
    };
    assert(
        DecodeGoldSrcClientSignonPayload(newline.data(), newline.size()).status
        == GoldSrcClientSignonDecodeStatus::kInvalidControlByte);

    constexpr std::array<std::uint8_t, 5> empty = {
        1u, 3u, 0u, 1u, 1u,
    };
    assert(
        DecodeGoldSrcClientSignonPayload(empty.data(), empty.size()).status
        == GoldSrcClientSignonDecodeStatus::kEmptyCommand);

    constexpr std::array<std::uint8_t, 10> injected = {
        3u, 'n', 'e', 'w', ';', 'q', 'u', 'i', 't', 0u,
    };
    assert(
        DecodeGoldSrcClientSignonPayload(
            injected.data(),
            injected.size()).status
        == GoldSrcClientSignonDecodeStatus::kUnsupportedCommand);

    constexpr std::array<std::uint8_t, 11> new_with_argument = {
        3u, 'n', 'e', 'w', ' ', 'e', 'x', 't', 'r', 'a', 0u,
    };
    assert(
        DecodeGoldSrcClientSignonPayload(
            new_with_argument.data(),
            new_with_argument.size()).status
        == GoldSrcClientSignonDecodeStatus::kUnsupportedCommand);

    constexpr std::array<std::uint8_t, 8> send_resources_missing_nul = {
        3u, 's', 'e', 'n', 'd', 'r', 'e', 's',
    };
    assert(
        DecodeGoldSrcClientSignonPayload(
            send_resources_missing_nul.data(),
            send_resources_missing_nul.size()).status
        == GoldSrcClientSignonDecodeStatus::kMissingStringTerminator);

    constexpr std::array<std::uint8_t, 10> send_resources_suffix = {
        3u, 's', 'e', 'n', 'd', 'r', 'e', 's', 'x', 0u,
    };
    assert(
        DecodeGoldSrcClientSignonPayload(
            send_resources_suffix.data(),
            send_resources_suffix.size()).status
        == GoldSrcClientSignonDecodeStatus::kUnsupportedCommand);

    constexpr std::array<std::uint8_t, 10> send_resources_argument = {
        3u, 's', 'e', 'n', 'd', 'r', 'e', 's', ' ', 0u,
    };
    assert(
        DecodeGoldSrcClientSignonPayload(
            send_resources_argument.data(),
            send_resources_argument.size()).status
        == GoldSrcClientSignonDecodeStatus::kUnsupportedCommand);

    constexpr std::array<std::uint8_t, 9> send_resources_wrong_case = {
        3u, 'S', 'e', 'n', 'd', 'r', 'e', 's', 0u,
    };
    assert(
        DecodeGoldSrcClientSignonPayload(
            send_resources_wrong_case.data(),
            send_resources_wrong_case.size()).status
        == GoldSrcClientSignonDecodeStatus::kUnsupportedCommand);

    constexpr std::array<std::uint8_t, 8> unrelated_command = {
        3u, 's', 't', 'a', 't', 'u', 's', 0u,
    };
    assert(
        DecodeGoldSrcClientSignonPayload(
            unrelated_command.data(),
            unrelated_command.size()).status
        == GoldSrcClientSignonDecodeStatus::kUnsupportedCommand);

    constexpr std::array<std::uint8_t, 10> two_commands = {
        3u, 'n', 'e', 'w', 0u,
        3u, 'n', 'e', 'w', 0u,
    };
    assert(
        DecodeGoldSrcClientSignonPayload(
            two_commands.data(),
            two_commands.size()).status
        == GoldSrcClientSignonDecodeStatus::kMultipleCommands);

    constexpr std::array<std::uint8_t, 20>
        send_resources_unsupported_companion = {
            3u, 's', 'e', 'n', 'd', 'r', 'e', 's', 0u,
            3u, 'c', 'l', 'o', 's', 'e', 'm', 'e', 'n', 'u', 0u,
        };
    assert(
        DecodeGoldSrcClientSignonPayload(
            send_resources_unsupported_companion.data(),
            send_resources_unsupported_companion.size()).status
        == GoldSrcClientSignonDecodeStatus::kUnsupportedCompanionCommand);

    constexpr std::array<std::uint8_t, 36>
        send_resources_incomplete_companion = {
            3u, 's', 'e', 'n', 'd', 'r', 'e', 's', 0u,
            3u, 'c', 'l', 'o', 's', 'e', 'm', 'e', 'n', 'u', 's',
                ' ', '\n', 0u,
            3u, 'c', 'l', 'o', 's', 'e', 'm', 'e', 'n', 'u', 's',
                ' ', '\n',
        };
    assert(
        DecodeGoldSrcClientSignonPayload(
            send_resources_incomplete_companion.data(),
            send_resources_incomplete_companion.size()).status
        == GoldSrcClientSignonDecodeStatus::kMissingStringTerminator);

    constexpr std::array<std::uint8_t, 51>
        send_resources_too_many_companions = {
            3u, 's', 'e', 'n', 'd', 'r', 'e', 's', 0u,
            3u, 'c', 'l', 'o', 's', 'e', 'm', 'e', 'n', 'u', 's',
                ' ', '\n', 0u,
            3u, 'c', 'l', 'o', 's', 'e', 'm', 'e', 'n', 'u', 's',
                ' ', '\n', 0u,
            3u, 'c', 'l', 'o', 's', 'e', 'm', 'e', 'n', 'u', 's',
                ' ', '\n', 0u,
        };
    assert(
        DecodeGoldSrcClientSignonPayload(
            send_resources_too_many_companions.data(),
            send_resources_too_many_companions.size()).status
        == GoldSrcClientSignonDecodeStatus::kTooManyCompanionCommands);

    constexpr std::array<std::uint8_t, 22>
        close_menus_with_argument = {
            3u, 's', 'e', 'n', 'd', 'r', 'e', 's', 0u,
            3u, 'c', 'l', 'o', 's', 'e', 'm', 'e', 'n', 'u', 's', ' ', 0u,
        };
    assert(
        DecodeGoldSrcClientSignonPayload(
            close_menus_with_argument.data(),
            close_menus_with_argument.size()).status
        == GoldSrcClientSignonDecodeStatus::kUnsupportedCompanionCommand);

    constexpr std::array<std::uint8_t, 21> close_menus_without_suffix = {
        3u, 's', 'e', 'n', 'd', 'r', 'e', 's', 0u,
        3u, 'c', 'l', 'o', 's', 'e', 'm', 'e', 'n', 'u', 's', 0u,
    };
    assert(
        DecodeGoldSrcClientSignonPayload(
            close_menus_without_suffix.data(),
            close_menus_without_suffix.size()).status
        == GoldSrcClientSignonDecodeStatus::kUnsupportedCompanionCommand);

    constexpr std::array<std::uint8_t, 21>
        close_menus_before_send_resources = {
            3u, 'c', 'l', 'o', 's', 'e', 'm', 'e', 'n', 'u', 's', 0u,
            3u, 's', 'e', 'n', 'd', 'r', 'e', 's', 0u,
        };
    assert(
        DecodeGoldSrcClientSignonPayload(
            close_menus_before_send_resources.data(),
            close_menus_before_send_resources.size()).status
        == GoldSrcClientSignonDecodeStatus::kUnsupportedCommand);

    constexpr std::array<std::uint8_t, 7> trailing_data = {
        3u, 'n', 'e', 'w', 0u, 1u, 0xFFu,
    };
    assert(
        DecodeGoldSrcClientSignonPayload(
            trailing_data.data(),
            trailing_data.size()).status
        == GoldSrcClientSignonDecodeStatus::kUnsupportedTrailingData);

    std::vector<std::uint8_t> oversized(
        kGoldSrcMaximumSignonPayloadBytes + 1u,
        1u);
    assert(
        DecodeGoldSrcClientSignonPayload(
            oversized.data(),
            oversized.size()).status
        == GoldSrcClientSignonDecodeStatus::kPayloadTooLarge);

    std::vector<std::uint8_t> overlong = {3u};
    overlong.insert(
        overlong.end(),
        kGoldSrcMaximumClientCommandBytes + 1u,
        static_cast<std::uint8_t>('a'));
    overlong.push_back(0u);
    assert(
        DecodeGoldSrcClientSignonPayload(
            overlong.data(),
            overlong.size()).status
        == GoldSrcClientSignonDecodeStatus::kCommandTooLong);
}

void TestSignonStateExactOnceAndReset()
{
    GoldSrcSignonSessionState state;
    assert(state.phase() == GoldSrcSignonPhase::kNone);
    assert(
        state.HandleClientCommand(GoldSrcClientSignonCommand::kNone)
        == GoldSrcSignonCommandDisposition::kUnsupported);
    assert(state.phase() == GoldSrcSignonPhase::kNone);
    assert(state.diagnostics().client_new_received == 0u);
    assert(
        state.MarkServerInfoSent()
        == GoldSrcSignonTransitionResult::kInvalidPhase);
    assert(
        state.MarkServerInfoAcknowledged()
        == GoldSrcSignonTransitionResult::kInvalidPhase);
    assert(
        state.EnterAwaitingResourceRequest()
        == GoldSrcSignonTransitionResult::kInvalidPhase);
    assert(
        state.MarkResourceManifestSent()
        == GoldSrcSignonTransitionResult::kInvalidPhase);
    assert(
        state.MarkResourceManifestAcknowledged()
        == GoldSrcSignonTransitionResult::kInvalidPhase);
    assert(
        state.HandleClientCommand(GoldSrcClientSignonCommand::kNew)
        == GoldSrcSignonCommandDisposition::kWrongPhase);
    assert(state.diagnostics().client_new_received == 1u);
    assert(state.diagnostics().client_new_delivered == 0u);

    state.Reset();
    assert(
        state.EnterAwaitingNew()
        == GoldSrcSignonTransitionResult::kAdvanced);
    assert(
        state.EnterAwaitingNew()
        == GoldSrcSignonTransitionResult::kAlreadyApplied);
    assert(state.phase() == GoldSrcSignonPhase::kAwaitingNew);

    assert(
        state.HandleClientCommand(GoldSrcClientSignonCommand::kNew)
        == GoldSrcSignonCommandDisposition::kDelivered);
    assert(state.phase() == GoldSrcSignonPhase::kServerInfoQueued);
    assert(state.diagnostics().client_new_delivered == 1u);
    assert(state.diagnostics().serverinfo_queued == 1u);
    assert(
        state.HandleClientCommand(GoldSrcClientSignonCommand::kNew)
        == GoldSrcSignonCommandDisposition::kDuplicateSuppressed);
    assert(state.diagnostics().client_new_delivered == 1u);
    assert(state.diagnostics().duplicate_new_suppressed == 1u);

    assert(
        state.MarkServerInfoAcknowledged()
        == GoldSrcSignonTransitionResult::kInvalidPhase);
    assert(state.phase() == GoldSrcSignonPhase::kServerInfoQueued);
    assert(
        state.MarkServerInfoSent()
        == GoldSrcSignonTransitionResult::kAdvanced);
    assert(state.phase() == GoldSrcSignonPhase::kServerInfoSentAwaitingAck);
    assert(
        state.MarkServerInfoSent()
        == GoldSrcSignonTransitionResult::kAlreadyApplied);
    assert(
        state.HandleClientCommand(GoldSrcClientSignonCommand::kNew)
        == GoldSrcSignonCommandDisposition::kDuplicateSuppressed);
    assert(state.diagnostics().client_new_delivered == 1u);
    assert(state.diagnostics().duplicate_new_suppressed == 2u);
    assert(
        state.MarkServerInfoAcknowledged()
        == GoldSrcSignonTransitionResult::kAdvanced);
    assert(state.phase() == GoldSrcSignonPhase::kServerInfoAcknowledged);
    assert(
        state.MarkServerInfoAcknowledged()
        == GoldSrcSignonTransitionResult::kAlreadyApplied);
    assert(state.diagnostics().serverinfo_acknowledged == 1u);
    assert(
        state.HandleClientCommand(GoldSrcClientSignonCommand::kNew)
        == GoldSrcSignonCommandDisposition::kDuplicateSuppressed);
    assert(state.diagnostics().client_new_delivered == 1u);
    assert(state.diagnostics().duplicate_new_suppressed == 3u);
    assert(
        state.EnterAwaitingNew()
        == GoldSrcSignonTransitionResult::kInvalidPhase);

    assert(
        state.HandleClientCommand(GoldSrcClientSignonCommand::kSendResources)
        == GoldSrcSignonCommandDisposition::kWrongPhase);
    assert(state.phase() == GoldSrcSignonPhase::kServerInfoAcknowledged);
    assert(state.diagnostics().resource_request_received == 1u);
    assert(state.diagnostics().resource_request_delivered == 0u);
    assert(state.diagnostics().resource_request_wrong_phase == 1u);

    assert(
        state.EnterAwaitingResourceRequest()
        == GoldSrcSignonTransitionResult::kAdvanced);
    assert(state.phase() == GoldSrcSignonPhase::kAwaitingResourceRequest);
    assert(NameFor(state.phase()) == "awaiting_resource_request");
    assert(
        state.EnterAwaitingResourceRequest()
        == GoldSrcSignonTransitionResult::kAlreadyApplied);
    assert(
        state.MarkResourceManifestSent()
        == GoldSrcSignonTransitionResult::kInvalidPhase);

    assert(
        state.HandleClientCommand(GoldSrcClientSignonCommand::kSendResources)
        == GoldSrcSignonCommandDisposition::kDelivered);
    assert(state.phase() == GoldSrcSignonPhase::kResourceManifestQueued);
    assert(NameFor(state.phase()) == "resource_manifest_queued");
    assert(state.diagnostics().resource_request_received == 2u);
    assert(state.diagnostics().resource_request_delivered == 1u);
    assert(state.diagnostics().resource_manifest_queued == 1u);
    assert(
        state.HandleClientCommand(GoldSrcClientSignonCommand::kSendResources)
        == GoldSrcSignonCommandDisposition::kDuplicateSuppressed);
    assert(state.diagnostics().resource_request_received == 3u);
    assert(state.diagnostics().resource_request_delivered == 1u);
    assert(
        state.diagnostics().duplicate_resource_request_suppressed == 1u);
    assert(
        state.MarkResourceManifestAcknowledged()
        == GoldSrcSignonTransitionResult::kInvalidPhase);

    assert(
        state.MarkResourceManifestSent()
        == GoldSrcSignonTransitionResult::kAdvanced);
    assert(
        state.phase()
        == GoldSrcSignonPhase::kResourceManifestSentAwaitingAck);
    assert(NameFor(state.phase()) == "resource_manifest_sent_awaiting_ack");
    assert(state.diagnostics().resource_manifest_sent == 1u);
    assert(
        state.MarkResourceManifestSent()
        == GoldSrcSignonTransitionResult::kAlreadyApplied);
    assert(
        state.HandleClientCommand(GoldSrcClientSignonCommand::kSendResources)
        == GoldSrcSignonCommandDisposition::kDuplicateSuppressed);
    assert(state.diagnostics().resource_request_received == 4u);
    assert(state.diagnostics().resource_request_delivered == 1u);
    assert(
        state.diagnostics().duplicate_resource_request_suppressed == 2u);

    assert(
        state.MarkResourceManifestAcknowledged()
        == GoldSrcSignonTransitionResult::kAdvanced);
    assert(state.phase() == GoldSrcSignonPhase::kResourceManifestAcknowledged);
    assert(NameFor(state.phase()) == "resource_manifest_acknowledged");
    assert(state.diagnostics().resource_manifest_acknowledged == 1u);
    assert(
        state.MarkResourceManifestAcknowledged()
        == GoldSrcSignonTransitionResult::kAlreadyApplied);
    assert(
        state.EnterAwaitingResourceRequest()
        == GoldSrcSignonTransitionResult::kAlreadyApplied);
    assert(
        state.MarkServerInfoAcknowledged()
        == GoldSrcSignonTransitionResult::kAlreadyApplied);
    assert(
        state.HandleClientCommand(GoldSrcClientSignonCommand::kSendResources)
        == GoldSrcSignonCommandDisposition::kDuplicateSuppressed);
    assert(state.diagnostics().resource_request_received == 5u);
    assert(state.diagnostics().resource_request_delivered == 1u);
    assert(
        state.diagnostics().duplicate_resource_request_suppressed == 3u);
    assert(
        state.HandleClientCommand(GoldSrcClientSignonCommand::kNew)
        == GoldSrcSignonCommandDisposition::kDuplicateSuppressed);
    assert(state.diagnostics().client_new_delivered == 1u);
    assert(state.diagnostics().duplicate_new_suppressed == 4u);

    state.Reset();
    assert(state.phase() == GoldSrcSignonPhase::kNone);
    assert(state.diagnostics().client_new_received == 0u);
    assert(state.diagnostics().client_new_delivered == 0u);
    assert(state.diagnostics().duplicate_new_suppressed == 0u);
    assert(state.diagnostics().serverinfo_queued == 0u);
    assert(state.diagnostics().serverinfo_sent == 0u);
    assert(state.diagnostics().serverinfo_acknowledged == 0u);
    assert(state.diagnostics().resource_request_received == 0u);
    assert(state.diagnostics().resource_request_delivered == 0u);
    assert(
        state.diagnostics().duplicate_resource_request_suppressed == 0u);
    assert(state.diagnostics().resource_request_wrong_phase == 0u);
    assert(state.diagnostics().resource_manifest_queued == 0u);
    assert(state.diagnostics().resource_manifest_sent == 0u);
    assert(state.diagnostics().resource_manifest_acknowledged == 0u);
}

void TestCombinedSignonBootstrapExactOnceAndReset()
{
    GoldSrcSignonSessionState state;

    assert(
        state.EnterAwaitingNew(
            GoldSrcSignonBootstrapMode::
                kServerInfoWithDeltaDescriptions)
        == GoldSrcSignonTransitionResult::kAdvanced);
    assert(
        state.EnterAwaitingNew(
            GoldSrcSignonBootstrapMode::
                kServerInfoWithDeltaDescriptions)
        == GoldSrcSignonTransitionResult::kAlreadyApplied);
    assert(
        state.EnterAwaitingNew(
            GoldSrcSignonBootstrapMode::kServerInfoOnly)
        == GoldSrcSignonTransitionResult::kInvalidPhase);
    assert(state.phase() == GoldSrcSignonPhase::kAwaitingNew);

    assert(
        state.HandleClientCommand(GoldSrcClientSignonCommand::kNew)
        == GoldSrcSignonCommandDisposition::kDelivered);
    assert(state.phase() == GoldSrcSignonPhase::kSignonBootstrapQueued);
    assert(NameFor(state.phase()) == "signon_bootstrap_queued");
    assert(state.diagnostics().client_new_received == 1u);
    assert(state.diagnostics().client_new_delivered == 1u);
    assert(state.diagnostics().serverinfo_queued == 1u);
    assert(state.diagnostics().delta_descriptions_queued == 1u);
    assert(state.diagnostics().signon_bootstrap_queued == 1u);

    assert(
        state.HandleClientCommand(GoldSrcClientSignonCommand::kNew)
        == GoldSrcSignonCommandDisposition::kDuplicateSuppressed);
    assert(state.phase() == GoldSrcSignonPhase::kSignonBootstrapQueued);
    assert(state.diagnostics().client_new_received == 2u);
    assert(state.diagnostics().client_new_delivered == 1u);
    assert(state.diagnostics().duplicate_new_suppressed == 1u);
    assert(state.diagnostics().serverinfo_queued == 1u);
    assert(state.diagnostics().delta_descriptions_queued == 1u);
    assert(state.diagnostics().signon_bootstrap_queued == 1u);

    assert(
        state.MarkServerInfoSent()
        == GoldSrcSignonTransitionResult::kInvalidPhase);
    assert(
        state.MarkServerInfoAcknowledged()
        == GoldSrcSignonTransitionResult::kInvalidPhase);
    assert(
        state.MarkSignonBootstrapAcknowledged()
        == GoldSrcSignonTransitionResult::kInvalidPhase);
    assert(
        state.EnterAwaitingResourceRequest()
        == GoldSrcSignonTransitionResult::kInvalidPhase);
    assert(state.phase() == GoldSrcSignonPhase::kSignonBootstrapQueued);

    assert(
        state.MarkSignonBootstrapSent()
        == GoldSrcSignonTransitionResult::kAdvanced);
    assert(
        state.phase()
        == GoldSrcSignonPhase::kSignonBootstrapSentAwaitingAck);
    assert(NameFor(state.phase()) == "signon_bootstrap_sent_awaiting_ack");
    assert(
        state.MarkSignonBootstrapSent()
        == GoldSrcSignonTransitionResult::kAlreadyApplied);
    assert(
        state.MarkServerInfoSent()
        == GoldSrcSignonTransitionResult::kInvalidPhase);
    assert(
        state.MarkServerInfoAcknowledged()
        == GoldSrcSignonTransitionResult::kInvalidPhase);
    assert(state.diagnostics().serverinfo_sent == 1u);
    assert(state.diagnostics().delta_descriptions_sent == 1u);
    assert(state.diagnostics().signon_bootstrap_sent == 1u);

    assert(
        state.MarkSignonBootstrapAcknowledged()
        == GoldSrcSignonTransitionResult::kAdvanced);
    assert(state.phase() == GoldSrcSignonPhase::kSignonBootstrapAcknowledged);
    assert(NameFor(state.phase()) == "signon_bootstrap_acknowledged");
    assert(
        state.MarkSignonBootstrapAcknowledged()
        == GoldSrcSignonTransitionResult::kAlreadyApplied);
    assert(
        state.MarkServerInfoAcknowledged()
        == GoldSrcSignonTransitionResult::kInvalidPhase);
    assert(state.diagnostics().serverinfo_acknowledged == 1u);
    assert(state.diagnostics().delta_descriptions_acknowledged == 1u);
    assert(state.diagnostics().signon_bootstrap_acknowledged == 1u);

    assert(
        state.EnterAwaitingResourceRequest()
        == GoldSrcSignonTransitionResult::kAdvanced);
    assert(state.phase() == GoldSrcSignonPhase::kAwaitingResourceRequest);
    assert(
        state.EnterAwaitingResourceRequest()
        == GoldSrcSignonTransitionResult::kAlreadyApplied);
    assert(
        state.MarkSignonBootstrapSent()
        == GoldSrcSignonTransitionResult::kAlreadyApplied);
    assert(
        state.MarkSignonBootstrapAcknowledged()
        == GoldSrcSignonTransitionResult::kAlreadyApplied);
    assert(state.diagnostics().serverinfo_queued == 1u);
    assert(state.diagnostics().serverinfo_sent == 1u);
    assert(state.diagnostics().serverinfo_acknowledged == 1u);
    assert(state.diagnostics().delta_descriptions_queued == 1u);
    assert(state.diagnostics().delta_descriptions_sent == 1u);
    assert(state.diagnostics().delta_descriptions_acknowledged == 1u);
    assert(state.diagnostics().signon_bootstrap_queued == 1u);
    assert(state.diagnostics().signon_bootstrap_sent == 1u);
    assert(state.diagnostics().signon_bootstrap_acknowledged == 1u);
    state.Reset();
    assert(state.phase() == GoldSrcSignonPhase::kNone);
    assert(state.diagnostics().client_new_received == 0u);
    assert(state.diagnostics().client_new_delivered == 0u);
    assert(state.diagnostics().duplicate_new_suppressed == 0u);
    assert(state.diagnostics().serverinfo_queued == 0u);
    assert(state.diagnostics().serverinfo_sent == 0u);
    assert(state.diagnostics().serverinfo_acknowledged == 0u);
    assert(state.diagnostics().delta_descriptions_queued == 0u);
    assert(state.diagnostics().delta_descriptions_sent == 0u);
    assert(state.diagnostics().delta_descriptions_acknowledged == 0u);
    assert(state.diagnostics().signon_bootstrap_queued == 0u);
    assert(state.diagnostics().signon_bootstrap_sent == 0u);
    assert(state.diagnostics().signon_bootstrap_acknowledged == 0u);

    assert(
        state.EnterAwaitingNew(
            GoldSrcSignonBootstrapMode::kServerInfoOnly)
        == GoldSrcSignonTransitionResult::kAdvanced);
    assert(
        state.EnterAwaitingNew(
            GoldSrcSignonBootstrapMode::
                kServerInfoWithDeltaDescriptions)
        == GoldSrcSignonTransitionResult::kInvalidPhase);
    assert(
        state.HandleClientCommand(GoldSrcClientSignonCommand::kNew)
        == GoldSrcSignonCommandDisposition::kDelivered);
    assert(state.phase() == GoldSrcSignonPhase::kServerInfoQueued);
    assert(
        state.MarkSignonBootstrapSent()
        == GoldSrcSignonTransitionResult::kInvalidPhase);
    assert(
        state.MarkSignonBootstrapAcknowledged()
        == GoldSrcSignonTransitionResult::kInvalidPhase);
    assert(state.diagnostics().serverinfo_queued == 1u);
    assert(state.diagnostics().delta_descriptions_queued == 0u);
    assert(state.diagnostics().signon_bootstrap_queued == 0u);

    state.Reset();
    assert(
        state.EnterAwaitingNew(
            GoldSrcSignonBootstrapMode::
                kServerInfoWithDeltaDescriptions)
        == GoldSrcSignonTransitionResult::kAdvanced);
    assert(
        state.HandleClientCommand(GoldSrcClientSignonCommand::kNew)
        == GoldSrcSignonCommandDisposition::kDelivered);
    assert(state.phase() == GoldSrcSignonPhase::kSignonBootstrapQueued);
    assert(state.diagnostics().client_new_received == 1u);
    assert(state.diagnostics().client_new_delivered == 1u);
    assert(state.diagnostics().serverinfo_queued == 1u);
    assert(state.diagnostics().delta_descriptions_queued == 1u);
    assert(state.diagnostics().signon_bootstrap_queued == 1u);
}

void TestMunge3AndServerInfoGoldenBytes()
{
    assert(MungeGoldSrcServerInfoChecksum(0x12345678u, 0u) == 0x87166CB7u);
    assert(MungeGoldSrcServerInfoChecksum(0x12345678u, 1u) == 0x86166CB6u);
    assert(MungeGoldSrcServerInfoChecksum(0x49C30432u, 0u) == 0xCD449BECu);
    assert(MungeGoldSrcServerInfoChecksum(0x49C30432u, 1u) == 0xCC449BEDu);
    assert(UnmungeGoldSrcServerInfoChecksum(0x87166CB7u, 0u) == 0x12345678u);
    assert(UnmungeGoldSrcServerInfoChecksum(0x86166CB6u, 1u) == 0x12345678u);
    assert(UnmungeGoldSrcServerInfoChecksum(0xCD449BECu, 0u) == 0x49C30432u);
    assert(UnmungeGoldSrcServerInfoChecksum(0xCC449BEDu, 1u) == 0x49C30432u);

    const GoldSrcServerInfoContext context = GoldenContext();
    const GoldSrcServerInfoEncodeResult encoded =
        EncodeGoldSrcServerInfo(context);
    assert(encoded.ok());

    const std::vector<std::uint8_t> expected = GoldenServerInfoFixture();
    assert(encoded.payload.size == expected.size());
    assert(std::equal(
        expected.begin(),
        expected.end(),
        encoded.payload.bytes.begin()));

    GoldSrcServerInfoContext second_player = context;
    second_player.player_index = 1u;
    const GoldSrcServerInfoEncodeResult encoded_second =
        EncodeGoldSrcServerInfo(second_player);
    assert(encoded_second.ok());
    assert(encoded_second.payload.bytes[9] == 0xB6u);
    assert(encoded_second.payload.bytes[10] == 0x6Cu);
    assert(encoded_second.payload.bytes[11] == 0x16u);
    assert(encoded_second.payload.bytes[12] == 0x86u);
    assert(encoded_second.payload.bytes[30] == 1u);
}

void TestStandaloneSendExtraInfoGoldenAndBoundaries()
{
    const GoldSrcServerInfoEncodeResult combined =
        EncodeGoldSrcServerInfo(GoldenContext());
    const GoldSrcSendExtraInfoEncodeResult empty =
        EncodeGoldSrcSendExtraInfo({}, false);
    assert(combined.ok());
    assert(empty.ok());
    assert(empty.payload.size == 3u);
    assert(empty.payload.size <= kGoldSrcMaximumSendExtraInfoPayloadBytes);
    assert(std::equal(
        empty.payload.bytes.begin(),
        empty.payload.bytes.begin()
            + static_cast<std::ptrdiff_t>(empty.payload.size),
        combined.payload.bytes.begin()
            + static_cast<std::ptrdiff_t>(
                combined.payload.size - empty.payload.size)));

    const std::string maximum_fallback(
        kGoldSrcMaximumFallbackDirectoryBytes,
        'f');
    const GoldSrcSendExtraInfoEncodeResult maximum =
        EncodeGoldSrcSendExtraInfo(maximum_fallback, false);
    assert(maximum.ok());
    assert(maximum.payload.size == kGoldSrcMaximumSendExtraInfoPayloadBytes);

    const auto assert_rejected_with_empty_payload = [](
        const GoldSrcSendExtraInfoEncodeResult& rejected,
        GoldSrcServerInfoCodecStatus expected_status)
    {
        assert(rejected.status == expected_status);
        assert(rejected.payload.size == 0u);
    };

    assert_rejected_with_empty_payload(
        EncodeGoldSrcSendExtraInfo(
            std::string(kGoldSrcMaximumFallbackDirectoryBytes + 1u, 'f'),
            false),
        GoldSrcServerInfoCodecStatus::kStringTooLong);
    assert_rejected_with_empty_payload(
        EncodeGoldSrcSendExtraInfo(std::string("val\0ve", 6u), false),
        GoldSrcServerInfoCodecStatus::kEmbeddedNul);
    assert_rejected_with_empty_payload(
        EncodeGoldSrcSendExtraInfo("bad\nname", false),
        GoldSrcServerInfoCodecStatus::kInvalidControlByte);
    assert_rejected_with_empty_payload(
        EncodeGoldSrcSendExtraInfo({}, true),
        GoldSrcServerInfoCodecStatus::kCheatsUnsupported);
}

void AssertGoldenServerInfoDecoded(
    const GoldSrcServerInfoDecodeResult& decoded)
{
    assert(decoded.ok());
    assert(decoded.server_info.protocol_version == 48u);
    assert(decoded.server_info.spawn_count == 7u);
    assert(decoded.server_info.wire_map_checksum == 0x87166CB7u);
    assert(decoded.server_info.canonical_map_checksum == 0x12345678u);
    const std::array<std::uint8_t, 16> expected_digest = {
        0xE9u, 0x70u, 0xC0u, 0xD0u,
        0x7Eu, 0x35u, 0x5Du, 0x80u,
        0x4Fu, 0x4Au, 0x42u, 0x65u,
        0x88u, 0x24u, 0x0Cu, 0x0Bu,
    };
    assert(decoded.server_info.client_dll_md5 == expected_digest);
    assert(decoded.server_info.max_clients == 2u);
    assert(decoded.server_info.player_index == 0u);
    assert(decoded.server_info.deathmatch);
    assert(decoded.server_info.game_directory.view() == "valve");
    assert(decoded.server_info.hostname.view() == "HL Test");
    assert(decoded.server_info.map_model_path.view() == "maps/c1a0.bsp");
    assert(decoded.server_info.mapcycle.view().empty());
    assert(!decoded.server_info.secure);
    assert(decoded.server_info.fallback_game_directory.view().empty());
    assert(!decoded.server_info.cheats);
}

void TestServerInfoDecoderGoldenRoundTripAndNoMutation()
{
    std::vector<std::uint8_t> fixture = GoldenServerInfoFixture();
    const std::vector<std::uint8_t> original_fixture = fixture;
    const GoldSrcServerInfoDecodeResult fixture_decoded =
        DecodeGoldSrcServerInfo(fixture.data(), fixture.size());
    AssertGoldenServerInfoDecoded(fixture_decoded);
    assert(fixture == original_fixture);

    GoldSrcServerInfoContext context = GoldenContext();
    const GoldSrcServerInfoContext original_context = context;
    const GoldSrcServerInfoEncodeResult encoded =
        EncodeGoldSrcServerInfo(context);
    assert(encoded.ok());
    const GoldSrcServerInfoDecodeResult round_trip =
        DecodeGoldSrcServerInfo(encoded.payload.bytes.data(), encoded.payload.size);
    AssertGoldenServerInfoDecoded(round_trip);

    assert(context.protocol_version == original_context.protocol_version);
    assert(context.spawn_count == original_context.spawn_count);
    assert(context.canonical_map_checksum == original_context.canonical_map_checksum);
    assert(context.client_dll_md5 == original_context.client_dll_md5);
    assert(context.max_clients == original_context.max_clients);
    assert(context.player_index == original_context.player_index);
    assert(context.deathmatch == original_context.deathmatch);
    assert(context.game_directory == original_context.game_directory);
    assert(context.hostname == original_context.hostname);
    assert(context.map_model_path == original_context.map_model_path);
    assert(context.mapcycle == original_context.mapcycle);
    assert(context.secure == original_context.secure);
    assert(
        context.fallback_game_directory
        == original_context.fallback_game_directory);
    assert(context.cheats == original_context.cheats);
}

void TestServerInfoDecoderRejectsTruncationAndMalformedGrammar()
{
    const std::vector<std::uint8_t> fixture = GoldenServerInfoFixture();
    for (std::size_t prefix = 0u; prefix < fixture.size(); ++prefix)
    {
        assert(!DecodeGoldSrcServerInfo(fixture.data(), prefix).ok());
    }

    // Exact byte offsets of the five protocol-string terminators in the
    // independent golden fixture.
    constexpr std::array<std::size_t, 5> string_terminators = {
        37u, 45u, 59u, 60u, 63u,
    };
    for (const std::size_t terminator : string_terminators)
    {
        const std::vector<std::uint8_t> missing_terminator(
            fixture.begin(),
            fixture.begin() + static_cast<std::ptrdiff_t>(terminator));
        assert(
            DecodeGoldSrcServerInfo(
                missing_terminator.data(),
                missing_terminator.size()).status
            == GoldSrcServerInfoDecodeStatus::kMissingStringTerminator);
    }

    assert(
        DecodeGoldSrcServerInfo(nullptr, 1u).status
        == GoldSrcServerInfoDecodeStatus::kNullInput);
    assert(
        DecodeGoldSrcServerInfo(nullptr, 0u).status
        == GoldSrcServerInfoDecodeStatus::kEmptyPayload);

    std::vector<std::uint8_t> malformed = fixture;
    malformed[0] = 12u;
    assert(
        DecodeGoldSrcServerInfo(malformed.data(), malformed.size()).status
        == GoldSrcServerInfoDecodeStatus::kWrongServerInfoOpcode);

    malformed = fixture;
    WriteLittleEndian32(&malformed, 1u, 47u);
    assert(
        DecodeGoldSrcServerInfo(malformed.data(), malformed.size()).status
        == GoldSrcServerInfoDecodeStatus::kUnsupportedProtocol);

    malformed = fixture;
    WriteLittleEndian32(
        &malformed,
        9u,
        MungeGoldSrcServerInfoChecksum(0u, 0u));
    assert(
        DecodeGoldSrcServerInfo(malformed.data(), malformed.size()).status
        == GoldSrcServerInfoDecodeStatus::kZeroMapChecksum);

    malformed = fixture;
    std::fill(
        malformed.begin() + 13,
        malformed.begin() + 29,
        static_cast<std::uint8_t>(0u));
    assert(
        DecodeGoldSrcServerInfo(malformed.data(), malformed.size()).status
        == GoldSrcServerInfoDecodeStatus::kZeroClientDllDigest);

    malformed = fixture;
    malformed[29] = 0u;
    assert(
        DecodeGoldSrcServerInfo(malformed.data(), malformed.size()).status
        == GoldSrcServerInfoDecodeStatus::kInvalidMaxClients);

    malformed = fixture;
    malformed[30] = 2u;
    assert(
        DecodeGoldSrcServerInfo(malformed.data(), malformed.size()).status
        == GoldSrcServerInfoDecodeStatus::kInvalidPlayerIndex);

    malformed = fixture;
    malformed[31] = 2u;
    assert(
        DecodeGoldSrcServerInfo(malformed.data(), malformed.size()).status
        == GoldSrcServerInfoDecodeStatus::kInvalidBooleanValue);

    malformed = fixture;
    malformed.erase(malformed.begin() + 32, malformed.begin() + 37);
    assert(
        DecodeGoldSrcServerInfo(malformed.data(), malformed.size()).status
        == GoldSrcServerInfoDecodeStatus::kEmptyRequiredString);

    malformed = fixture;
    malformed.insert(malformed.begin() + 37, 59u, 'g');
    assert(
        DecodeGoldSrcServerInfo(malformed.data(), malformed.size()).status
        == GoldSrcServerInfoDecodeStatus::kStringTooLong);

    malformed = fixture;
    malformed[32] = '\n';
    assert(
        DecodeGoldSrcServerInfo(malformed.data(), malformed.size()).status
        == GoldSrcServerInfoDecodeStatus::kInvalidControlByte);

    malformed = fixture;
    malformed[46] = 'x';
    assert(
        DecodeGoldSrcServerInfo(malformed.data(), malformed.size()).status
        == GoldSrcServerInfoDecodeStatus::kInvalidModelPath);

    malformed = fixture;
    malformed[61] = 1u;
    assert(
        DecodeGoldSrcServerInfo(malformed.data(), malformed.size()).status
        == GoldSrcServerInfoDecodeStatus::kSecureModeUnsupported);

    malformed = fixture;
    malformed[61] = 2u;
    assert(
        DecodeGoldSrcServerInfo(malformed.data(), malformed.size()).status
        == GoldSrcServerInfoDecodeStatus::kInvalidBooleanValue);

    malformed = fixture;
    malformed[62] = 55u;
    assert(
        DecodeGoldSrcServerInfo(malformed.data(), malformed.size()).status
        == GoldSrcServerInfoDecodeStatus::kWrongCompanionOpcode);

    malformed = fixture;
    malformed[64] = 1u;
    assert(
        DecodeGoldSrcServerInfo(malformed.data(), malformed.size()).status
        == GoldSrcServerInfoDecodeStatus::kCheatsUnsupported);

    malformed = fixture;
    malformed[64] = 2u;
    assert(
        DecodeGoldSrcServerInfo(malformed.data(), malformed.size()).status
        == GoldSrcServerInfoDecodeStatus::kInvalidBooleanValue);

    malformed = fixture;
    malformed.push_back(1u);
    assert(
        DecodeGoldSrcServerInfo(malformed.data(), malformed.size()).status
        == GoldSrcServerInfoDecodeStatus::kTrailingData);

    std::vector<std::uint8_t> oversized(
        kGoldSrcMaximumSignonPayloadBytes + 1u,
        0u);
    assert(
        DecodeGoldSrcServerInfo(oversized.data(), oversized.size()).status
        == GoldSrcServerInfoDecodeStatus::kPayloadTooLarge);
}

void TestServerInfoValidationAndPayloadBoundary()
{
    const GoldSrcServerInfoContext valid = GoldenContext();
    assert(ValidateGoldSrcServerInfoContext(valid).ok());

    GoldSrcServerInfoContext changed = valid;
    changed.protocol_version = 47u;
    assert(
        EncodeGoldSrcServerInfo(changed).status
        == GoldSrcServerInfoCodecStatus::kUnsupportedProtocol);

    changed = valid;
    changed.max_clients = 0u;
    assert(
        EncodeGoldSrcServerInfo(changed).status
        == GoldSrcServerInfoCodecStatus::kInvalidMaxClients);

    changed = valid;
    changed.player_index = changed.max_clients;
    assert(
        EncodeGoldSrcServerInfo(changed).status
        == GoldSrcServerInfoCodecStatus::kInvalidPlayerIndex);

    changed = valid;
    changed.canonical_map_checksum.reset();
    assert(
        EncodeGoldSrcServerInfo(changed).status
        == GoldSrcServerInfoCodecStatus::kMissingMapChecksum);

    changed = valid;
    changed.client_dll_md5.reset();
    assert(
        EncodeGoldSrcServerInfo(changed).status
        == GoldSrcServerInfoCodecStatus::kMissingClientDllDigest);

    changed = valid;
    changed.canonical_map_checksum = 0u;
    assert(
        ValidateGoldSrcServerInfoContext(changed).status
        == GoldSrcServerInfoCodecStatus::kZeroMapChecksum);

    changed = valid;
    changed.client_dll_md5 = std::array<std::uint8_t, 16>{};
    assert(
        ValidateGoldSrcServerInfoContext(changed).status
        == GoldSrcServerInfoCodecStatus::kZeroClientDllDigest);

    changed = valid;
    changed.game_directory.clear();
    assert(
        EncodeGoldSrcServerInfo(changed).status
        == GoldSrcServerInfoCodecStatus::kEmptyRequiredString);

    changed = valid;
    changed.game_directory.assign(kGoldSrcMaximumGameDirectoryBytes + 1u, 'g');
    assert(
        EncodeGoldSrcServerInfo(changed).status
        == GoldSrcServerInfoCodecStatus::kStringTooLong);

    changed = valid;
    changed.game_directory = std::string("val\0ve", 6u);
    assert(
        EncodeGoldSrcServerInfo(changed).status
        == GoldSrcServerInfoCodecStatus::kEmbeddedNul);

    changed = valid;
    changed.hostname = "bad\nname";
    assert(
        EncodeGoldSrcServerInfo(changed).status
        == GoldSrcServerInfoCodecStatus::kInvalidControlByte);

    changed = valid;
    changed.map_model_path = "c1a0.bsp";
    assert(
        EncodeGoldSrcServerInfo(changed).status
        == GoldSrcServerInfoCodecStatus::kInvalidModelPath);

    changed = valid;
    changed.secure = true;
    assert(
        EncodeGoldSrcServerInfo(changed).status
        == GoldSrcServerInfoCodecStatus::kSecureModeUnsupported);

    changed = valid;
    changed.cheats = true;
    assert(
        EncodeGoldSrcServerInfo(changed).status
        == GoldSrcServerInfoCodecStatus::kCheatsUnsupported);

    changed = valid;
    changed.hostname.assign(119u, 'h');
    changed.mapcycle.assign(kGoldSrcMaximumMapcycleBytes, 'm');
    const GoldSrcServerInfoEncodeResult maximum =
        EncodeGoldSrcServerInfo(changed);
    assert(maximum.ok());
    assert(maximum.payload.size == kGoldSrcMaximumSignonPayloadBytes);
    const GoldSrcServerInfoDecodeResult maximum_decoded =
        DecodeGoldSrcServerInfo(maximum.payload.bytes.data(), maximum.payload.size);
    assert(maximum_decoded.ok());
    assert(maximum_decoded.server_info.hostname.view().size() == 119u);
    assert(
        maximum_decoded.server_info.mapcycle.view().size()
        == kGoldSrcMaximumMapcycleBytes);

    changed.hostname.push_back('h');
    assert(
        EncodeGoldSrcServerInfo(changed).status
        == GoldSrcServerInfoCodecStatus::kPayloadTooLarge);
}

void TestServerInfoContextStringBoundariesAndDeterminism()
{
    const GoldSrcServerInfoContext valid = GoldenContext();
    const GoldSrcServerInfoEncodeResult first =
        EncodeGoldSrcServerInfo(valid);
    const GoldSrcServerInfoEncodeResult second =
        EncodeGoldSrcServerInfo(valid);
    assert(first.ok());
    assert(second.ok());
    assert(first.payload.size == second.payload.size);
    assert(std::equal(
        first.payload.bytes.begin(),
        first.payload.bytes.begin()
            + static_cast<std::ptrdiff_t>(first.payload.size),
        second.payload.bytes.begin()));

    GoldSrcServerInfoContext changed = valid;
    changed.spawn_count = 0x01020304u;
    const GoldSrcServerInfoEncodeResult changed_spawn =
        EncodeGoldSrcServerInfo(changed);
    assert(changed_spawn.ok());
    const GoldSrcServerInfoDecodeResult changed_spawn_decoded =
        DecodeGoldSrcServerInfo(
            changed_spawn.payload.bytes.data(),
            changed_spawn.payload.size);
    assert(changed_spawn_decoded.ok());
    assert(changed_spawn_decoded.server_info.spawn_count == 0x01020304u);

    changed = valid;
    changed.hostname.clear();
    assert(
        EncodeGoldSrcServerInfo(changed).status
        == GoldSrcServerInfoCodecStatus::kEmptyRequiredString);

    changed = valid;
    changed.map_model_path.clear();
    assert(
        EncodeGoldSrcServerInfo(changed).status
        == GoldSrcServerInfoCodecStatus::kEmptyRequiredString);

    changed = valid;
    changed.mapcycle = "c1a0;c1a1";
    const GoldSrcServerInfoEncodeResult non_empty_mapcycle =
        EncodeGoldSrcServerInfo(changed);
    assert(non_empty_mapcycle.ok());
    const GoldSrcServerInfoDecodeResult non_empty_mapcycle_decoded =
        DecodeGoldSrcServerInfo(
            non_empty_mapcycle.payload.bytes.data(),
            non_empty_mapcycle.payload.size);
    assert(non_empty_mapcycle_decoded.ok());
    assert(
        non_empty_mapcycle_decoded.server_info.mapcycle.view()
        == changed.mapcycle);

    changed = valid;
    changed.game_directory.assign(kGoldSrcMaximumGameDirectoryBytes, 'g');
    assert(EncodeGoldSrcServerInfo(changed).ok());
    changed.game_directory.push_back('g');
    assert(
        EncodeGoldSrcServerInfo(changed).status
        == GoldSrcServerInfoCodecStatus::kStringTooLong);

    changed = valid;
    changed.hostname.assign(kGoldSrcMaximumHostnameBytes, 'h');
    const GoldSrcServerInfoEncodeResult maximum_hostname =
        EncodeGoldSrcServerInfo(changed);
    assert(maximum_hostname.ok());
    assert(
        DecodeGoldSrcServerInfo(
            maximum_hostname.payload.bytes.data(),
            maximum_hostname.payload.size).server_info.hostname.view().size()
        == kGoldSrcMaximumHostnameBytes);
    changed.hostname.push_back('h');
    assert(
        EncodeGoldSrcServerInfo(changed).status
        == GoldSrcServerInfoCodecStatus::kStringTooLong);

    changed = valid;
    changed.map_model_path = "maps/"
        + std::string(kGoldSrcMaximumModelPathBytes - 9u, 'm')
        + ".bsp";
    assert(changed.map_model_path.size() == kGoldSrcMaximumModelPathBytes);
    assert(EncodeGoldSrcServerInfo(changed).ok());
    changed.map_model_path.insert(changed.map_model_path.size() - 4u, 1u, 'm');
    assert(
        EncodeGoldSrcServerInfo(changed).status
        == GoldSrcServerInfoCodecStatus::kStringTooLong);

    changed = valid;
    changed.mapcycle.assign(kGoldSrcMaximumMapcycleBytes, 'm');
    assert(EncodeGoldSrcServerInfo(changed).ok());
    changed.mapcycle.push_back('m');
    assert(
        EncodeGoldSrcServerInfo(changed).status
        == GoldSrcServerInfoCodecStatus::kStringTooLong);

    changed = valid;
    changed.fallback_game_directory.assign(
        kGoldSrcMaximumFallbackDirectoryBytes,
        'f');
    assert(EncodeGoldSrcServerInfo(changed).ok());
    changed.fallback_game_directory.push_back('f');
    assert(
        EncodeGoldSrcServerInfo(changed).status
        == GoldSrcServerInfoCodecStatus::kStringTooLong);
}

std::vector<std::uint8_t> GoldenBspWithReversedPhysicalLumps()
{
    std::vector<std::uint8_t> bsp(kGoldSrcBsp30HeaderBytes + 4u, 0u);
    WriteLittleEndian32(&bsp, 0u, 30u);

    // Physical order is lump 2 then lump 1. GoldSrc CRC_MapFile processes by
    // lump index, so the logical byte stream is still 01 02 03 04.
    WriteLittleEndian32(&bsp, 4u + (1u * 8u), 126u);
    WriteLittleEndian32(&bsp, 8u + (1u * 8u), 2u);
    WriteLittleEndian32(&bsp, 4u + (2u * 8u), 124u);
    WriteLittleEndian32(&bsp, 8u + (2u * 8u), 2u);
    bsp[124] = 3u;
    bsp[125] = 4u;
    bsp[126] = 1u;
    bsp[127] = 2u;
    return bsp;
}

void TestBsp30MapChecksumGoldenBoundsAndFile()
{
    const std::vector<std::uint8_t> bsp =
        GoldenBspWithReversedPhysicalLumps();
    const GoldSrcMapChecksumResult checksum =
        ComputeGoldSrcBsp30MapChecksum(bsp.data(), bsp.size());
    assert(checksum.ok());
    assert(checksum.checksum == 0x49C30432u);

    std::vector<std::uint8_t> entities_ignored(
        kGoldSrcBsp30HeaderBytes + 8u,
        0u);
    WriteLittleEndian32(&entities_ignored, 0u, 30u);
    WriteLittleEndian32(&entities_ignored, 4u, 124u);
    WriteLittleEndian32(&entities_ignored, 8u, 4u);
    WriteLittleEndian32(&entities_ignored, 12u, 128u);
    WriteLittleEndian32(&entities_ignored, 16u, 4u);
    entities_ignored[124] = 0xAAu;
    entities_ignored[125] = 0xBBu;
    entities_ignored[126] = 0xCCu;
    entities_ignored[127] = 0xDDu;
    entities_ignored[128] = 1u;
    entities_ignored[129] = 2u;
    entities_ignored[130] = 3u;
    entities_ignored[131] = 4u;
    const GoldSrcMapChecksumResult first_entities =
        ComputeGoldSrcBsp30MapChecksum(
            entities_ignored.data(),
            entities_ignored.size());
    entities_ignored[124] ^= 0xFFu;
    const GoldSrcMapChecksumResult changed_entities =
        ComputeGoldSrcBsp30MapChecksum(
            entities_ignored.data(),
            entities_ignored.size());
    assert(first_entities.ok());
    assert(changed_entities.ok());
    assert(first_entities.checksum == 0x49C30432u);
    assert(changed_entities.checksum == first_entities.checksum);

    std::vector<std::uint8_t> empty_lumps(kGoldSrcBsp30HeaderBytes, 0u);
    WriteLittleEndian32(&empty_lumps, 0u, 30u);
    const GoldSrcMapChecksumResult empty_checksum =
        ComputeGoldSrcBsp30MapChecksum(
            empty_lumps.data(),
            empty_lumps.size());
    assert(empty_checksum.ok());
    assert(empty_checksum.checksum == 0xFFFFFFFFu);

    assert(
        ComputeGoldSrcBsp30MapChecksum(nullptr, 0u).status
        == GoldSrcMapChecksumStatus::kNullInput);
    assert(
        ComputeGoldSrcBsp30MapChecksum(
            empty_lumps.data(),
            kGoldSrcBsp30HeaderBytes - 1u).status
        == GoldSrcMapChecksumStatus::kTruncatedHeader);

    std::vector<std::uint8_t> wrong_version = empty_lumps;
    WriteLittleEndian32(&wrong_version, 0u, 29u);
    assert(
        ComputeGoldSrcBsp30MapChecksum(
            wrong_version.data(),
            wrong_version.size()).status
        == GoldSrcMapChecksumStatus::kUnsupportedBspVersion);

    std::vector<std::uint8_t> invalid_bounds = empty_lumps;
    WriteLittleEndian32(
        &invalid_bounds,
        4u + (1u * 8u),
        static_cast<std::uint32_t>(invalid_bounds.size() + 1u));
    assert(
        ComputeGoldSrcBsp30MapChecksum(
            invalid_bounds.data(),
            invalid_bounds.size()).status
        == GoldSrcMapChecksumStatus::kInvalidLumpBounds);

    invalid_bounds = empty_lumps;
    WriteLittleEndian32(&invalid_bounds, 4u + (1u * 8u), 120u);
    WriteLittleEndian32(&invalid_bounds, 8u + (1u * 8u), 8u);
    assert(
        ComputeGoldSrcBsp30MapChecksum(
            invalid_bounds.data(),
            invalid_bounds.size()).status
        == GoldSrcMapChecksumStatus::kInvalidLumpBounds);

    invalid_bounds = empty_lumps;
    WriteLittleEndian32(&invalid_bounds, 4u + (1u * 8u), 0xFFFFFFFFu);
    assert(
        ComputeGoldSrcBsp30MapChecksum(
            invalid_bounds.data(),
            invalid_bounds.size()).status
        == GoldSrcMapChecksumStatus::kInvalidLumpBounds);

    const std::uint8_t dummy = 0u;
    assert(
        ComputeGoldSrcBsp30MapChecksum(
            &dummy,
            static_cast<std::size_t>(kGoldSrcMaximumBspFileBytes + 1u)).status
        == GoldSrcMapChecksumStatus::kFileTooLarge);

    const std::filesystem::path path = UniqueTemporaryPath(".bsp");
    {
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        assert(output.is_open());
        output.write(
            reinterpret_cast<const char*>(bsp.data()),
            static_cast<std::streamsize>(bsp.size()));
        assert(output.good());
    }
    const GoldSrcMapChecksumResult file_checksum =
        ComputeGoldSrcBsp30MapChecksum(path);
    assert(file_checksum.ok());
    assert(file_checksum.checksum == 0x49C30432u);
    std::error_code ignored;
    std::filesystem::remove(path, ignored);
    assert(
        ComputeGoldSrcBsp30MapChecksum(path).status
        == GoldSrcMapChecksumStatus::kOpenFailed);
}

void TestMd5MemoryAndClientDllFile()
{
    const std::array<std::uint8_t, 16> empty_expected = {
        0xD4u, 0x1Du, 0x8Cu, 0xD9u,
        0x8Fu, 0x00u, 0xB2u, 0x04u,
        0xE9u, 0x80u, 0x09u, 0x98u,
        0xECu, 0xF8u, 0x42u, 0x7Eu,
    };
    const GoldSrcMd5Result empty = ComputeGoldSrcMd5(nullptr, 0u);
    assert(empty.ok());
    assert(empty.digest == empty_expected);

    const std::vector<std::uint8_t> abc = Bytes("abc");
    const std::array<std::uint8_t, 16> abc_expected = {
        0x90u, 0x01u, 0x50u, 0x98u,
        0x3Cu, 0xD2u, 0x4Fu, 0xB0u,
        0xD6u, 0x96u, 0x3Fu, 0x7Du,
        0x28u, 0xE1u, 0x7Fu, 0x72u,
    };
    const GoldSrcMd5Result abc_hash =
        ComputeGoldSrcMd5(abc.data(), abc.size());
    assert(abc_hash.ok());
    assert(abc_hash.digest == abc_expected);
    assert(
        ComputeGoldSrcMd5(nullptr, 1u).status
        == GoldSrcMd5Status::kNullInput);

    const std::vector<std::uint8_t> client_fixture =
        Bytes("synthetic-client-dll-fixture\n");
    const std::array<std::uint8_t, 16> client_expected = {
        0xE9u, 0x70u, 0xC0u, 0xD0u,
        0x7Eu, 0x35u, 0x5Du, 0x80u,
        0x4Fu, 0x4Au, 0x42u, 0x65u,
        0x88u, 0x24u, 0x0Cu, 0x0Bu,
    };
    assert(
        ComputeGoldSrcMd5(
            client_fixture.data(),
            client_fixture.size()).digest
        == client_expected);

    const std::filesystem::path path = UniqueTemporaryPath("_client.dll");
    {
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        assert(output.is_open());
        output.write(
            reinterpret_cast<const char*>(client_fixture.data()),
            static_cast<std::streamsize>(client_fixture.size()));
        assert(output.good());
    }
    const GoldSrcMd5Result file_hash = ComputeGoldSrcClientDllMd5(path);
    assert(file_hash.ok());
    assert(file_hash.digest == client_expected);
    std::error_code ignored;
    std::filesystem::remove(path, ignored);
    assert(
        ComputeGoldSrcClientDllMd5(path).status
        == GoldSrcMd5Status::kOpenFailed);
}

float ReadFixtureFloat(
    const GoldSrcBootstrapTailPayload& payload,
    std::size_t offset)
{
    assert(offset <= payload.size);
    assert(payload.size - offset >= sizeof(float));
    const std::uint32_t bits =
        static_cast<std::uint32_t>(payload.bytes[offset])
        | (static_cast<std::uint32_t>(payload.bytes[offset + 1u]) << 8u)
        | (static_cast<std::uint32_t>(payload.bytes[offset + 2u]) << 16u)
        | (static_cast<std::uint32_t>(payload.bytes[offset + 3u]) << 24u);
    float value = 0.0f;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

void TestBootstrapTailReferenceOrderAndBounds()
{
    GoldSrcBootstrapTailContext context{};
    context.gravity = 1.0f;
    context.stop_speed = 2.0f;
    context.maximum_speed = 3.0f;
    context.spectator_maximum_speed = 4.0f;
    context.accelerate = 5.0f;
    context.air_accelerate = 6.0f;
    context.water_accelerate = 7.0f;
    context.friction = 8.0f;
    context.edge_friction = 9.0f;
    context.water_friction = 10.0f;
    context.entity_gravity = 11.0f;
    context.bounce = 12.0f;
    context.step_size = 13.0f;
    context.maximum_velocity = 14.0f;
    context.z_maximum = 15.0f;
    context.wave_height = 16.0f;
    context.footsteps = true;
    context.roll_angle = 17.0f;
    context.roll_speed = 18.0f;
    context.sky_color_red = 19.0f;
    context.sky_color_green = 20.0f;
    context.sky_color_blue = 21.0f;
    context.sky_vector_x = 22.0f;
    context.sky_vector_y = 23.0f;
    context.sky_vector_z = 24.0f;
    context.sky_name = "desert";
    context.cd_audio_track = 3u;
    context.view_entity = 1u;

    const GoldSrcBootstrapTailEncodeResult encoded =
        EncodeGoldSrcBootstrapTail(context);
    assert(encoded.ok());
    assert(encoded.payload.size == 111u);
    assert(encoded.payload.bytes[0u] == kGoldSrcNewMoveVarsOpcode);
    for (std::size_t index = 0u; index < 16u; ++index)
    {
        assert(
            ReadFixtureFloat(encoded.payload, 1u + (index * 4u))
            == static_cast<float>(index + 1u));
    }
    assert(encoded.payload.bytes[65u] == 1u);
    for (std::size_t index = 0u; index < 8u; ++index)
    {
        assert(
            ReadFixtureFloat(encoded.payload, 66u + (index * 4u))
            == static_cast<float>(index + 17u));
    }
    assert(std::equal(
        context.sky_name.begin(),
        context.sky_name.end(),
        encoded.payload.bytes.begin() + 98u));
    assert(encoded.payload.bytes[104u] == 0u);
    assert(encoded.payload.bytes[105u] == kGoldSrcCdTrackOpcode);
    assert(encoded.payload.bytes[106u] == 3u);
    assert(encoded.payload.bytes[107u] == 3u);
    assert(encoded.payload.bytes[108u] == kGoldSrcSetViewOpcode);
    assert(encoded.payload.bytes[109u] == 1u);
    assert(encoded.payload.bytes[110u] == 0u);

    const GoldSrcBootstrapTailEncodeResult repeated =
        EncodeGoldSrcBootstrapTail(context);
    assert(repeated.ok());
    assert(repeated.payload.size == encoded.payload.size);
    assert(std::equal(
        encoded.payload.bytes.begin(),
        encoded.payload.bytes.begin() + encoded.payload.size,
        repeated.payload.bytes.begin()));

    GoldSrcBootstrapTailContext invalid = context;
    invalid.gravity = std::numeric_limits<float>::infinity();
    assert(
        EncodeGoldSrcBootstrapTail(invalid).status
        == GoldSrcBootstrapTailCodecStatus::kNonFiniteMoveVariable);
    invalid = context;
    invalid.sky_name.assign(kGoldSrcMaximumSkyNameBytes + 1u, 'x');
    assert(
        EncodeGoldSrcBootstrapTail(invalid).status
        == GoldSrcBootstrapTailCodecStatus::kSkyNameTooLong);
    invalid = context;
    invalid.sky_name = std::string("sky\0name", 8u);
    assert(
        EncodeGoldSrcBootstrapTail(invalid).status
        == GoldSrcBootstrapTailCodecStatus::kEmbeddedNul);
    invalid = context;
    invalid.sky_name = "sky\nname";
    assert(
        EncodeGoldSrcBootstrapTail(invalid).status
        == GoldSrcBootstrapTailCodecStatus::kInvalidControlByte);
    invalid = context;
    invalid.view_entity = 0u;
    assert(
        EncodeGoldSrcBootstrapTail(invalid).status
        == GoldSrcBootstrapTailCodecStatus::kInvalidViewEntity);

    GoldSrcBootstrapTailContext maximum{};
    maximum.sky_name.assign(kGoldSrcMaximumSkyNameBytes, 'x');
    maximum.view_entity = kGoldSrcMaximumViewEntity;
    const GoldSrcBootstrapTailEncodeResult maximum_encoded =
        EncodeGoldSrcBootstrapTail(maximum);
    assert(maximum_encoded.ok());
    assert(maximum_encoded.payload.size == kGoldSrcMaximumBootstrapTailBytes);
    maximum.view_entity =
        static_cast<std::uint16_t>(kGoldSrcMaximumViewEntity + 1u);
    assert(
        EncodeGoldSrcBootstrapTail(maximum).status
        == GoldSrcBootstrapTailCodecStatus::kInvalidViewEntity);
}
} // namespace

int main()
{
    TestStrictClientNewDecoder();
    TestSignonStateExactOnceAndReset();
    TestCombinedSignonBootstrapExactOnceAndReset();
    TestMunge3AndServerInfoGoldenBytes();
    TestStandaloneSendExtraInfoGoldenAndBoundaries();
    TestServerInfoDecoderGoldenRoundTripAndNoMutation();
    TestServerInfoDecoderRejectsTruncationAndMalformedGrammar();
    TestServerInfoValidationAndPayloadBoundary();
    TestServerInfoContextStringBoundariesAndDeterminism();
    TestBsp30MapChecksumGoldenBoundsAndFile();
    TestMd5MemoryAndClientDllFile();
    TestBootstrapTailReferenceOrderAndBounds();
    return 0;
}
