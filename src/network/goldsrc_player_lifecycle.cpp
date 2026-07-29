#include "network/goldsrc_player_lifecycle.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstring>
#include <limits>

namespace hl::network
{
namespace
{
bool IsSafeText(std::string_view value, std::size_t maximum) noexcept
{
    if (value.empty() || value.size() > maximum)
    {
        return false;
    }
    for (const unsigned char character : value)
    {
        if (character < 0x20u || character == 0x7Fu)
        {
            return false;
        }
    }
    return true;
}

bool ParseDecimal(
    std::string_view text,
    std::uint32_t* value,
    bool* overflow) noexcept
{
    if (overflow != nullptr)
    {
        *overflow = false;
    }
    if (value == nullptr || text.empty())
    {
        return false;
    }
    for (const char character : text)
    {
        if (character < '0' || character > '9')
        {
            return false;
        }
    }
    std::uint64_t parsed = 0u;
    const auto result = std::from_chars(
        text.data(), text.data() + text.size(), parsed, 10);
    if (result.ec == std::errc::result_out_of_range
        || parsed > std::numeric_limits<std::uint32_t>::max())
    {
        if (overflow != nullptr)
        {
            *overflow = true;
        }
        return false;
    }
    if (result.ec != std::errc()
        || result.ptr != text.data() + text.size())
    {
        return false;
    }
    *value = static_cast<std::uint32_t>(parsed);
    return true;
}

std::uint16_t EncodeAngle(float angle) noexcept
{
    const double scaled = static_cast<double>(angle) * 65536.0 / 360.0;
    const auto integral = static_cast<std::int64_t>(scaled);
    return static_cast<std::uint16_t>(
        static_cast<std::uint64_t>(integral) & 0xFFFFu);
}

float DecodeAngle(std::uint16_t encoded) noexcept
{
    return static_cast<float>(
        static_cast<double>(encoded) * 360.0 / 65536.0);
}
} // namespace

std::string_view NameFor(GoldSrcPlayerLifecyclePhase phase) noexcept
{
    switch (phase)
    {
    case GoldSrcPlayerLifecyclePhase::kNoClientEdict:
        return "no_client_edict";
    case GoldSrcPlayerLifecyclePhase::kClientEdictAllocated:
        return "client_edict_allocated";
    case GoldSrcPlayerLifecyclePhase::kAwaitingGameDllConnect:
        return "awaiting_game_dll_connect";
    case GoldSrcPlayerLifecyclePhase::kGameDllConnected:
        return "game_dll_connected";
    case GoldSrcPlayerLifecyclePhase::kAwaitingPutInServer:
        return "awaiting_put_in_server";
    case GoldSrcPlayerLifecyclePhase::kPutInServer:
        return "put_in_server";
    case GoldSrcPlayerLifecyclePhase::kPlayerEntityReady:
        return "player_entity_ready";
    case GoldSrcPlayerLifecyclePhase::kAwaitingSignonProgression:
        return "awaiting_signon_progression";
    case GoldSrcPlayerLifecyclePhase::kPlayerViewEstablished:
        return "player_view_established";
    case GoldSrcPlayerLifecyclePhase::kPreMovementReady:
        return "pre_movement_ready";
    case GoldSrcPlayerLifecyclePhase::kRejected:
        return "rejected";
    case GoldSrcPlayerLifecyclePhase::kDisconnected:
    default:
        return "disconnected";
    }
}

std::string_view ReasonFor(GoldSrcClientEdictBindingResult result) noexcept
{
    switch (result)
    {
    case GoldSrcClientEdictBindingResult::kBound: return "bound";
    case GoldSrcClientEdictBindingResult::kDuplicateSession:
        return "duplicate_session";
    case GoldSrcClientEdictBindingResult::kSlotOutOfRange:
        return "slot_out_of_range";
    case GoldSrcClientEdictBindingResult::kGenerationInvalid:
        return "generation_invalid";
    case GoldSrcClientEdictBindingResult::kStaleSession:
        return "stale_session";
    case GoldSrcClientEdictBindingResult::kNotBound:
    default:
        return "not_bound";
    }
}

GoldSrcClientEdictRegistry::GoldSrcClientEdictRegistry(
    std::uint16_t maximum_clients) noexcept
{
    Reset(maximum_clients);
}

void GoldSrcClientEdictRegistry::Reset(
    std::uint16_t maximum_clients) noexcept
{
    maximum_clients_ = std::clamp<std::uint16_t>(
        maximum_clients, 1u, static_cast<std::uint16_t>(bindings_.size()));
    bindings_.fill(std::nullopt);
    generations_.fill(0u);
}

GoldSrcClientEdictBindingResult GoldSrcClientEdictRegistry::Bind(
    std::uint16_t client_slot,
    std::uint64_t network_session_generation,
    GoldSrcClientEdictBinding* binding) noexcept
{
    if (client_slot == 0u || client_slot > maximum_clients_)
    {
        return GoldSrcClientEdictBindingResult::kSlotOutOfRange;
    }
    if (network_session_generation == 0u)
    {
        return GoldSrcClientEdictBindingResult::kGenerationInvalid;
    }
    const std::size_t offset = static_cast<std::size_t>(client_slot - 1u);
    if (bindings_[offset].has_value()
        && bindings_[offset]->network_session_generation
            == network_session_generation)
    {
        if (binding != nullptr)
        {
            *binding = *bindings_[offset];
        }
        return GoldSrcClientEdictBindingResult::kDuplicateSession;
    }
    std::uint32_t generation = generations_[offset] + 1u;
    if (generation == 0u)
    {
        generation = 1u;
    }
    generations_[offset] = generation;
    GoldSrcClientEdictBinding created;
    created.network_session_generation = network_session_generation;
    created.edict_generation = generation;
    created.client_slot = client_slot;
    created.edict_index = client_slot;
    created.valid = true;
    bindings_[offset] = created;
    if (binding != nullptr)
    {
        *binding = created;
    }
    return GoldSrcClientEdictBindingResult::kBound;
}

GoldSrcClientEdictBindingResult GoldSrcClientEdictRegistry::Validate(
    const GoldSrcClientEdictBinding& binding) const noexcept
{
    if (!binding.valid
        || binding.client_slot == 0u
        || binding.client_slot > maximum_clients_
        || binding.edict_index != binding.client_slot)
    {
        return GoldSrcClientEdictBindingResult::kNotBound;
    }
    const auto& current =
        bindings_[static_cast<std::size_t>(binding.client_slot - 1u)];
    if (!current.has_value())
    {
        return GoldSrcClientEdictBindingResult::kNotBound;
    }
    if (current->network_session_generation
            != binding.network_session_generation
        || current->edict_generation != binding.edict_generation)
    {
        return GoldSrcClientEdictBindingResult::kStaleSession;
    }
    return GoldSrcClientEdictBindingResult::kBound;
}

GoldSrcClientEdictBindingResult GoldSrcClientEdictRegistry::Disconnect(
    const GoldSrcClientEdictBinding& binding) noexcept
{
    const GoldSrcClientEdictBindingResult validation = Validate(binding);
    if (validation != GoldSrcClientEdictBindingResult::kBound)
    {
        return validation;
    }
    bindings_[static_cast<std::size_t>(binding.client_slot - 1u)].reset();
    return GoldSrcClientEdictBindingResult::kBound;
}

std::optional<GoldSrcClientEdictBinding>
GoldSrcClientEdictRegistry::BindingForSlot(
    std::uint16_t client_slot) const noexcept
{
    if (client_slot == 0u || client_slot > maximum_clients_)
    {
        return std::nullopt;
    }
    return bindings_[static_cast<std::size_t>(client_slot - 1u)];
}

std::uint16_t GoldSrcClientEdictRegistry::maximum_clients() const noexcept
{
    return maximum_clients_;
}

std::string_view ReasonFor(
    GoldSrcClientLifecycleCommandStatus status) noexcept
{
    switch (status)
    {
    case GoldSrcClientLifecycleCommandStatus::kOk: return "ok";
    case GoldSrcClientLifecycleCommandStatus::kNullInput: return "null_input";
    case GoldSrcClientLifecycleCommandStatus::kEmptyPayload: return "empty_payload";
    case GoldSrcClientLifecycleCommandStatus::kPayloadTooLarge:
        return "payload_too_large";
    case GoldSrcClientLifecycleCommandStatus::kUnsupportedOpcode:
        return "unsupported_opcode";
    case GoldSrcClientLifecycleCommandStatus::kMissingTerminator:
        return "missing_terminator";
    case GoldSrcClientLifecycleCommandStatus::kEmbeddedNul: return "embedded_nul";
    case GoldSrcClientLifecycleCommandStatus::kInvalidControlCharacter:
        return "invalid_control_character";
    case GoldSrcClientLifecycleCommandStatus::kCommandInjection:
        return "command_injection";
    case GoldSrcClientLifecycleCommandStatus::kUnsupportedCommand:
        return "unsupported_command";
    case GoldSrcClientLifecycleCommandStatus::kWrongArgumentCount:
        return "wrong_argument_count";
    case GoldSrcClientLifecycleCommandStatus::kInvalidSpawnCount:
        return "invalid_spawn_count";
    case GoldSrcClientLifecycleCommandStatus::kInvalidChecksum:
        return "invalid_checksum";
    case GoldSrcClientLifecycleCommandStatus::kNumericOverflow:
        return "numeric_overflow";
    case GoldSrcClientLifecycleCommandStatus::kUnsupportedTrailingData:
    default:
        return "unsupported_trailing_data";
    }
}

GoldSrcClientLifecycleCommandDecodeResult
DecodeGoldSrcClientLifecycleCommand(
    const std::uint8_t* bytes,
    std::size_t size) noexcept
{
    GoldSrcClientLifecycleCommandDecodeResult result;
    if (bytes == nullptr)
    {
        result.status = size == 0u
            ? GoldSrcClientLifecycleCommandStatus::kEmptyPayload
            : GoldSrcClientLifecycleCommandStatus::kNullInput;
        return result;
    }
    if (size == 0u)
    {
        return result;
    }
    if (size > kGoldSrcMaximumLifecycleCommandBytes)
    {
        result.status =
            GoldSrcClientLifecycleCommandStatus::kPayloadTooLarge;
        return result;
    }

    std::size_t cursor = 0u;
    while (cursor < size && bytes[cursor] == kGoldSrcClientNopOpcode)
    {
        ++cursor;
    }
    if (cursor >= size
        || bytes[cursor++] != kGoldSrcClientStringCommandOpcode)
    {
        result.status =
            GoldSrcClientLifecycleCommandStatus::kUnsupportedOpcode;
        return result;
    }
    const std::size_t command_begin = cursor;
    while (cursor < size && bytes[cursor] != 0u)
    {
        const std::uint8_t character = bytes[cursor];
        if (character < 0x20u || character == 0x7Fu)
        {
            result.status =
                GoldSrcClientLifecycleCommandStatus::kInvalidControlCharacter;
            return result;
        }
        if (character == ';')
        {
            result.status =
                GoldSrcClientLifecycleCommandStatus::kCommandInjection;
            return result;
        }
        ++cursor;
    }
    if (cursor >= size)
    {
        result.status =
            GoldSrcClientLifecycleCommandStatus::kMissingTerminator;
        return result;
    }
    const std::string_view command(
        reinterpret_cast<const char*>(bytes + command_begin),
        cursor - command_begin);
    ++cursor;
    while (cursor < size && bytes[cursor] == kGoldSrcClientNopOpcode)
    {
        ++cursor;
    }
    if (cursor != size)
    {
        result.status =
            bytes[cursor] == 0u
            ? GoldSrcClientLifecycleCommandStatus::kEmbeddedNul
            : GoldSrcClientLifecycleCommandStatus::kUnsupportedTrailingData;
        return result;
    }

    std::array<std::string_view, 4u> tokens{};
    std::size_t token_count = 0u;
    std::size_t position = 0u;
    while (position < command.size())
    {
        if (command[position] == ' ')
        {
            result.status =
                GoldSrcClientLifecycleCommandStatus::kWrongArgumentCount;
            return result;
        }
        const std::size_t separator = command.find(' ', position);
        if (token_count >= tokens.size())
        {
            result.status =
                GoldSrcClientLifecycleCommandStatus::kWrongArgumentCount;
            return result;
        }
        tokens[token_count++] = command.substr(
            position,
            separator == std::string_view::npos
                ? command.size() - position
                : separator - position);
        if (separator == std::string_view::npos)
        {
            break;
        }
        position = separator + 1u;
    }
    if (token_count == 0u || tokens[0] != "spawn")
    {
        result.status =
            GoldSrcClientLifecycleCommandStatus::kUnsupportedCommand;
        return result;
    }
    if (token_count != 3u)
    {
        result.status =
            GoldSrcClientLifecycleCommandStatus::kWrongArgumentCount;
        return result;
    }
    bool overflow = false;
    if (!ParseDecimal(tokens[1], &result.command.spawn_count, &overflow))
    {
        result.status = overflow
            ? GoldSrcClientLifecycleCommandStatus::kNumericOverflow
            : GoldSrcClientLifecycleCommandStatus::kInvalidSpawnCount;
        return result;
    }
    if (!ParseDecimal(tokens[2], &result.command.checksum_token, &overflow))
    {
        result.status = overflow
            ? GoldSrcClientLifecycleCommandStatus::kNumericOverflow
            : GoldSrcClientLifecycleCommandStatus::kInvalidChecksum;
        return result;
    }
    result.command.kind = GoldSrcClientLifecycleCommandKind::kSpawn;
    result.status = GoldSrcClientLifecycleCommandStatus::kOk;
    return result;
}

std::string_view ReasonFor(GoldSrcPlayerLifecycleResult result) noexcept
{
    switch (result)
    {
    case GoldSrcPlayerLifecycleResult::kAdvanced: return "advanced";
    case GoldSrcPlayerLifecycleResult::kAlreadyApplied:
        return "already_applied";
    case GoldSrcPlayerLifecycleResult::kInvalidPhase: return "invalid_phase";
    case GoldSrcPlayerLifecycleResult::kInvalidBinding:
        return "invalid_binding";
    case GoldSrcPlayerLifecycleResult::kInvalidName: return "invalid_name";
    case GoldSrcPlayerLifecycleResult::kInvalidAddress:
        return "invalid_address";
    case GoldSrcPlayerLifecycleResult::kSpawnCountMismatch:
        return "spawn_count_mismatch";
    case GoldSrcPlayerLifecycleResult::kCallbackMissing:
        return "callback_missing";
    case GoldSrcPlayerLifecycleResult::kGameDllRejected:
        return "game_dll_rejected";
    case GoldSrcPlayerLifecycleResult::kPlayerMaterializationFailed:
        return "player_materialization_failed";
    case GoldSrcPlayerLifecycleResult::kInvalidViewEntity:
        return "invalid_view_entity";
    case GoldSrcPlayerLifecycleResult::kStaleSession:
    default:
        return "stale_session";
    }
}

void GoldSrcPlayerLifecycleSession::Reset() noexcept
{
    phase_ = GoldSrcPlayerLifecyclePhase::kNoClientEdict;
    binding_ = {};
    player_ = {};
    diagnostics_ = {};
    reject_reason_.fill('\0');
    player_name_.clear();
    view_entity_index_ = 0u;
    first_player_snapshot_frame_id_.reset();
    game_dll_connected_ = false;
    put_in_server_ = false;
    signon_complete_ = false;
}

GoldSrcPlayerLifecycleResult
GoldSrcPlayerLifecycleSession::BindClientEdict(
    const GoldSrcClientEdictBinding& binding) noexcept
{
    if (!binding.valid
        || binding.client_slot == 0u
        || binding.edict_index != binding.client_slot
        || binding.network_session_generation == 0u
        || binding.edict_generation == 0u)
    {
        return GoldSrcPlayerLifecycleResult::kInvalidBinding;
    }
    if (phase_ != GoldSrcPlayerLifecyclePhase::kNoClientEdict)
    {
        return binding_.network_session_generation
                    == binding.network_session_generation
                && binding_.edict_generation == binding.edict_generation
            ? GoldSrcPlayerLifecycleResult::kAlreadyApplied
            : GoldSrcPlayerLifecycleResult::kStaleSession;
    }
    binding_ = binding;
    phase_ = GoldSrcPlayerLifecyclePhase::kClientEdictAllocated;
    return GoldSrcPlayerLifecycleResult::kAdvanced;
}

GoldSrcPlayerLifecycleResult
GoldSrcPlayerLifecycleSession::RecordInitialUserInfo(
    std::string_view player_name) noexcept
{
    if (phase_ != GoldSrcPlayerLifecyclePhase::kClientEdictAllocated)
    {
        return GoldSrcPlayerLifecycleResult::kInvalidPhase;
    }
    if (!IsSafeText(player_name, kGoldSrcMaximumPlayerNameBytes))
    {
        return GoldSrcPlayerLifecycleResult::kInvalidName;
    }
    player_name_ = std::string(player_name);
    ++diagnostics_.userinfo_notifications;
    phase_ = GoldSrcPlayerLifecyclePhase::kAwaitingGameDllConnect;
    return GoldSrcPlayerLifecycleResult::kAdvanced;
}

GoldSrcPlayerLifecycleResult
GoldSrcPlayerLifecycleSession::ConnectGameDll(
    std::string_view player_name,
    std::string_view network_address,
    const GoldSrcClientConnectCallback& callback) noexcept
{
    if (game_dll_connected_)
    {
        ++diagnostics_.duplicate_connect_suppressed;
        return GoldSrcPlayerLifecycleResult::kAlreadyApplied;
    }
    if (phase_ != GoldSrcPlayerLifecyclePhase::kAwaitingGameDllConnect)
    {
        ++diagnostics_.invalid_phase_rejections;
        return GoldSrcPlayerLifecycleResult::kInvalidPhase;
    }
    if (!IsSafeText(player_name, kGoldSrcMaximumPlayerNameBytes)
        || player_name != player_name_)
    {
        return GoldSrcPlayerLifecycleResult::kInvalidName;
    }
    if (!IsSafeText(network_address, 63u))
    {
        return GoldSrcPlayerLifecycleResult::kInvalidAddress;
    }
    if (!callback)
    {
        return GoldSrcPlayerLifecycleResult::kCallbackMissing;
    }
    reject_reason_.fill('\0');
    constexpr std::string_view kDefaultReason = "Connection rejected by game\n";
    std::copy(
        kDefaultReason.begin(),
        kDefaultReason.end(),
        reject_reason_.begin());
    ++diagnostics_.client_connect_calls;
    const bool accepted = callback(
        binding_, player_name, network_address, reject_reason_);
    reject_reason_.back() = '\0';
    if (!accepted)
    {
        ++diagnostics_.client_connect_rejections;
        phase_ = GoldSrcPlayerLifecyclePhase::kRejected;
        return GoldSrcPlayerLifecycleResult::kGameDllRejected;
    }
    ++diagnostics_.client_connect_accepts;
    game_dll_connected_ = true;
    phase_ = GoldSrcPlayerLifecyclePhase::kGameDllConnected;
    return GoldSrcPlayerLifecycleResult::kAdvanced;
}

GoldSrcPlayerLifecycleResult
GoldSrcPlayerLifecycleSession::EnterAwaitingPutInServer() noexcept
{
    if (phase_ == GoldSrcPlayerLifecyclePhase::kGameDllConnected)
    {
        phase_ = GoldSrcPlayerLifecyclePhase::kAwaitingPutInServer;
        return GoldSrcPlayerLifecycleResult::kAdvanced;
    }
    if (phase_ == GoldSrcPlayerLifecyclePhase::kAwaitingPutInServer
        || put_in_server_)
    {
        return GoldSrcPlayerLifecycleResult::kAlreadyApplied;
    }
    ++diagnostics_.invalid_phase_rejections;
    return GoldSrcPlayerLifecycleResult::kInvalidPhase;
}

GoldSrcPlayerLifecycleResult GoldSrcPlayerLifecycleSession::PutInServer(
    std::uint32_t expected_spawn_count,
    const GoldSrcClientLifecycleCommand& command,
    const GoldSrcClientPutInServerCallback& callback) noexcept
{
    if (put_in_server_)
    {
        ++diagnostics_.duplicate_put_in_server_suppressed;
        return GoldSrcPlayerLifecycleResult::kAlreadyApplied;
    }
    if (phase_ != GoldSrcPlayerLifecyclePhase::kAwaitingPutInServer)
    {
        ++diagnostics_.invalid_phase_rejections;
        return GoldSrcPlayerLifecycleResult::kInvalidPhase;
    }
    if (command.kind != GoldSrcClientLifecycleCommandKind::kSpawn
        || command.spawn_count != expected_spawn_count)
    {
        ++diagnostics_.wrong_spawn_count_rejections;
        return GoldSrcPlayerLifecycleResult::kSpawnCountMismatch;
    }
    return PutInServerImpl(callback);
}

GoldSrcPlayerLifecycleResult
GoldSrcPlayerLifecycleSession::PutInServerFromReconciliation(
    const GoldSrcClientPutInServerCallback& callback) noexcept
{
    if (put_in_server_)
    {
        ++diagnostics_.duplicate_put_in_server_suppressed;
        return GoldSrcPlayerLifecycleResult::kAlreadyApplied;
    }
    if (phase_ != GoldSrcPlayerLifecyclePhase::kAwaitingPutInServer)
    {
        ++diagnostics_.invalid_phase_rejections;
        return GoldSrcPlayerLifecycleResult::kInvalidPhase;
    }
    return PutInServerImpl(callback);
}

GoldSrcPlayerLifecycleResult
GoldSrcPlayerLifecycleSession::PutInServerImpl(
    const GoldSrcClientPutInServerCallback& callback) noexcept
{
    if (!callback)
    {
        return GoldSrcPlayerLifecycleResult::kCallbackMissing;
    }
    ++diagnostics_.client_put_in_server_calls;
    phase_ = GoldSrcPlayerLifecyclePhase::kPutInServer;
    const GoldSrcPlayerMaterialization created = callback(binding_);
    if (!created.private_data_ready
        || !created.player_entity_ready
        || !created.player_spawned
        || !created.finite_network_state
        || created.model_index == 0u)
    {
        player_ = {};
        phase_ = GoldSrcPlayerLifecyclePhase::kGameDllConnected;
        return GoldSrcPlayerLifecycleResult::kPlayerMaterializationFailed;
    }
    player_ = created;
    put_in_server_ = true;
    ++diagnostics_.client_put_in_server_successes;
    phase_ = GoldSrcPlayerLifecyclePhase::kPlayerEntityReady;
    return GoldSrcPlayerLifecycleResult::kAdvanced;
}

GoldSrcPlayerLifecycleResult
GoldSrcPlayerLifecycleSession::EstablishView(
    std::uint16_t view_entity_index) noexcept
{
    if (phase_ == GoldSrcPlayerLifecyclePhase::kPlayerViewEstablished
        || phase_ == GoldSrcPlayerLifecyclePhase::kPreMovementReady)
    {
        return view_entity_index_ == view_entity_index
            ? GoldSrcPlayerLifecycleResult::kAlreadyApplied
            : GoldSrcPlayerLifecycleResult::kInvalidViewEntity;
    }
    if (phase_ != GoldSrcPlayerLifecyclePhase::kPlayerEntityReady
        && phase_
            != GoldSrcPlayerLifecyclePhase::kAwaitingSignonProgression)
    {
        ++diagnostics_.invalid_phase_rejections;
        return GoldSrcPlayerLifecycleResult::kInvalidPhase;
    }
    if (view_entity_index == 0u
        || view_entity_index != binding_.edict_index)
    {
        return GoldSrcPlayerLifecycleResult::kInvalidViewEntity;
    }
    phase_ = GoldSrcPlayerLifecyclePhase::kAwaitingSignonProgression;
    view_entity_index_ = view_entity_index;
    ++diagnostics_.view_assignments;
    phase_ = GoldSrcPlayerLifecyclePhase::kPlayerViewEstablished;
    return GoldSrcPlayerLifecycleResult::kAdvanced;
}

GoldSrcPlayerLifecycleResult
GoldSrcPlayerLifecycleSession::MarkPlayerSnapshot(
    std::uint32_t frame_id) noexcept
{
    if (phase_ != GoldSrcPlayerLifecyclePhase::kPlayerViewEstablished
        && phase_ != GoldSrcPlayerLifecyclePhase::kPreMovementReady)
    {
        ++diagnostics_.invalid_phase_rejections;
        return GoldSrcPlayerLifecycleResult::kInvalidPhase;
    }
    ++diagnostics_.player_snapshot_frames;
    if (!first_player_snapshot_frame_id_.has_value())
    {
        first_player_snapshot_frame_id_ = frame_id;
        return GoldSrcPlayerLifecycleResult::kAdvanced;
    }
    return GoldSrcPlayerLifecycleResult::kAlreadyApplied;
}

GoldSrcPlayerLifecycleResult
GoldSrcPlayerLifecycleSession::MarkSignonComplete() noexcept
{
    if (signon_complete_)
    {
        return GoldSrcPlayerLifecycleResult::kAlreadyApplied;
    }
    if (phase_ != GoldSrcPlayerLifecyclePhase::kPlayerViewEstablished
        || !first_player_snapshot_frame_id_.has_value())
    {
        ++diagnostics_.invalid_phase_rejections;
        return GoldSrcPlayerLifecycleResult::kInvalidPhase;
    }
    signon_complete_ = true;
    phase_ = GoldSrcPlayerLifecyclePhase::kPreMovementReady;
    return GoldSrcPlayerLifecycleResult::kAdvanced;
}

GoldSrcPlayerLifecycleResult GoldSrcPlayerLifecycleSession::Disconnect(
    const GoldSrcClientDisconnectCallback& callback) noexcept
{
    if (phase_ == GoldSrcPlayerLifecyclePhase::kDisconnected)
    {
        return GoldSrcPlayerLifecycleResult::kAlreadyApplied;
    }
    if (game_dll_connected_)
    {
        if (!callback)
        {
            return GoldSrcPlayerLifecycleResult::kCallbackMissing;
        }
        callback(binding_);
        ++diagnostics_.disconnect_calls;
    }
    binding_.valid = false;
    player_ = {};
    view_entity_index_ = 0u;
    first_player_snapshot_frame_id_.reset();
    game_dll_connected_ = false;
    put_in_server_ = false;
    signon_complete_ = false;
    phase_ = GoldSrcPlayerLifecyclePhase::kDisconnected;
    return GoldSrcPlayerLifecycleResult::kAdvanced;
}

GoldSrcPlayerLifecyclePhase
GoldSrcPlayerLifecycleSession::phase() const noexcept
{
    return phase_;
}

const GoldSrcClientEdictBinding&
GoldSrcPlayerLifecycleSession::binding() const noexcept
{
    return binding_;
}

const GoldSrcPlayerMaterialization&
GoldSrcPlayerLifecycleSession::player() const noexcept
{
    return player_;
}

const GoldSrcPlayerLifecycleDiagnostics&
GoldSrcPlayerLifecycleSession::diagnostics() const noexcept
{
    return diagnostics_;
}

std::string_view
GoldSrcPlayerLifecycleSession::reject_reason() const noexcept
{
    return std::string_view(
        reject_reason_.data(),
        std::char_traits<char>::length(reject_reason_.data()));
}

std::uint16_t
GoldSrcPlayerLifecycleSession::view_entity_index() const noexcept
{
    return view_entity_index_;
}

std::optional<std::uint32_t>
GoldSrcPlayerLifecycleSession::first_player_snapshot_frame_id() const noexcept
{
    return first_player_snapshot_frame_id_;
}

bool GoldSrcPlayerLifecycleSession::game_dll_connected() const noexcept
{
    return game_dll_connected_;
}

bool GoldSrcPlayerLifecycleSession::put_in_server() const noexcept
{
    return put_in_server_;
}

bool GoldSrcPlayerLifecycleSession::signon_complete() const noexcept
{
    return signon_complete_;
}

bool GoldSrcPlayerLifecycleSession::gameplay_active() const noexcept
{
    return false;
}

std::string_view ReasonFor(
    GoldSrcPlayerLifecycleControlStatus status) noexcept
{
    switch (status)
    {
    case GoldSrcPlayerLifecycleControlStatus::kOk: return "ok";
    case GoldSrcPlayerLifecycleControlStatus::kNullInput: return "null_input";
    case GoldSrcPlayerLifecycleControlStatus::kWrongSize: return "wrong_size";
    case GoldSrcPlayerLifecycleControlStatus::kInvalidViewEntity:
        return "invalid_view_entity";
    case GoldSrcPlayerLifecycleControlStatus::kNonFiniteAngle:
        return "non_finite_angle";
    case GoldSrcPlayerLifecycleControlStatus::kWrongMessageOrder:
        return "wrong_message_order";
    case GoldSrcPlayerLifecycleControlStatus::kWrongSignonNumber:
    default:
        return "wrong_signon_number";
    }
}

GoldSrcPlayerLifecycleControlEncodeResult
EncodeGoldSrcPlayerLifecycleControl(
    const GoldSrcPlayerLifecycleControl& control) noexcept
{
    GoldSrcPlayerLifecycleControlEncodeResult result;
    if (control.view_entity_index == 0u
        || control.view_entity_index > kGoldSrcMaximumViewEntity)
    {
        result.status =
            GoldSrcPlayerLifecycleControlStatus::kInvalidViewEntity;
        return result;
    }
    for (const float angle : control.view_angles)
    {
        if (!std::isfinite(angle))
        {
            result.status =
                GoldSrcPlayerLifecycleControlStatus::kNonFiniteAngle;
            return result;
        }
    }
    if (control.signon_number != kGoldSrcPlayerLifecycleSignonNumber)
    {
        result.status =
            GoldSrcPlayerLifecycleControlStatus::kWrongSignonNumber;
        return result;
    }
    auto& bytes = result.payload.bytes;
    bytes[0] = kGoldSrcSetViewOpcode;
    bytes[1] = static_cast<std::uint8_t>(
        control.view_entity_index & 0xFFu);
    bytes[2] = static_cast<std::uint8_t>(
        (control.view_entity_index >> 8u) & 0xFFu);
    bytes[3] = kGoldSrcSetAngleOpcode;
    for (std::size_t index = 0u; index < 3u; ++index)
    {
        const std::uint16_t encoded = EncodeAngle(control.view_angles[index]);
        bytes[4u + index * 2u] =
            static_cast<std::uint8_t>(encoded & 0xFFu);
        bytes[5u + index * 2u] =
            static_cast<std::uint8_t>((encoded >> 8u) & 0xFFu);
    }
    bytes[10] = kGoldSrcLifecycleSignonNumberOpcode;
    bytes[11] = control.signon_number;
    result.payload.size = kGoldSrcPlayerLifecycleControlBytes;
    result.status = GoldSrcPlayerLifecycleControlStatus::kOk;
    return result;
}

GoldSrcPlayerLifecycleControlDecodeResult
DecodeGoldSrcPlayerLifecycleControl(
    const std::uint8_t* bytes,
    std::size_t size) noexcept
{
    GoldSrcPlayerLifecycleControlDecodeResult result;
    if (bytes == nullptr)
    {
        result.status = size == 0u
            ? GoldSrcPlayerLifecycleControlStatus::kWrongSize
            : GoldSrcPlayerLifecycleControlStatus::kNullInput;
        return result;
    }
    if (size != kGoldSrcPlayerLifecycleControlBytes)
    {
        result.status = GoldSrcPlayerLifecycleControlStatus::kWrongSize;
        return result;
    }
    if (bytes[0] != kGoldSrcSetViewOpcode
        || bytes[3] != kGoldSrcSetAngleOpcode
        || bytes[10] != kGoldSrcLifecycleSignonNumberOpcode)
    {
        result.status =
            GoldSrcPlayerLifecycleControlStatus::kWrongMessageOrder;
        return result;
    }
    if (bytes[11] != kGoldSrcPlayerLifecycleSignonNumber)
    {
        result.status =
            GoldSrcPlayerLifecycleControlStatus::kWrongSignonNumber;
        return result;
    }
    result.control.view_entity_index = static_cast<std::uint16_t>(
        bytes[1] | (static_cast<std::uint16_t>(bytes[2]) << 8u));
    if (result.control.view_entity_index == 0u
        || result.control.view_entity_index > kGoldSrcMaximumViewEntity)
    {
        result.status =
            GoldSrcPlayerLifecycleControlStatus::kInvalidViewEntity;
        return result;
    }
    for (std::size_t index = 0u; index < 3u; ++index)
    {
        const std::uint16_t encoded = static_cast<std::uint16_t>(
            bytes[4u + index * 2u]
            | (static_cast<std::uint16_t>(bytes[5u + index * 2u]) << 8u));
        result.control.view_angles[index] = DecodeAngle(encoded);
    }
    result.control.signon_number = bytes[11];
    result.status = GoldSrcPlayerLifecycleControlStatus::kOk;
    return result;
}
} // namespace hl::network
