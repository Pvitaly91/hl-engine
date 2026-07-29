#pragma once

#include "network/goldsrc_signon.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace hl::network
{
inline constexpr std::size_t kGoldSrcMaximumPlayerNameBytes = 31u;
inline constexpr std::size_t kGoldSrcClientRejectReasonBytes = 128u;
inline constexpr std::size_t kGoldSrcMaximumLifecycleCommandBytes = 96u;
inline constexpr std::uint8_t kGoldSrcSetAngleOpcode = 10u;
inline constexpr std::uint8_t kGoldSrcLifecycleSignonNumberOpcode = 25u;
inline constexpr std::uint8_t kGoldSrcPlayerLifecycleSignonNumber = 1u;
inline constexpr std::size_t kGoldSrcPlayerLifecycleControlBytes = 12u;

enum class GoldSrcPlayerLifecyclePhase
{
    kNoClientEdict,
    kClientEdictAllocated,
    kAwaitingGameDllConnect,
    kGameDllConnected,
    kAwaitingPutInServer,
    kPutInServer,
    kPlayerEntityReady,
    kAwaitingSignonProgression,
    kPlayerViewEstablished,
    kPreMovementReady,
    kRejected,
    kDisconnected,
};

std::string_view NameFor(GoldSrcPlayerLifecyclePhase phase) noexcept;

struct GoldSrcClientEdictBinding final
{
    std::uint64_t network_session_generation = 0u;
    std::uint32_t edict_generation = 0u;
    std::uint16_t client_slot = 0u;
    std::uint16_t edict_index = 0u;
    bool valid = false;
};

enum class GoldSrcClientEdictBindingResult
{
    kBound,
    kDuplicateSession,
    kSlotOutOfRange,
    kGenerationInvalid,
    kStaleSession,
    kNotBound,
};

std::string_view ReasonFor(GoldSrcClientEdictBindingResult result) noexcept;

class GoldSrcClientEdictRegistry final
{
public:
    explicit GoldSrcClientEdictRegistry(std::uint16_t maximum_clients = 32u) noexcept;

    void Reset(std::uint16_t maximum_clients = 32u) noexcept;
    GoldSrcClientEdictBindingResult Bind(
        std::uint16_t client_slot,
        std::uint64_t network_session_generation,
        GoldSrcClientEdictBinding* binding = nullptr) noexcept;
    GoldSrcClientEdictBindingResult Validate(
        const GoldSrcClientEdictBinding& binding) const noexcept;
    GoldSrcClientEdictBindingResult Disconnect(
        const GoldSrcClientEdictBinding& binding) noexcept;
    std::optional<GoldSrcClientEdictBinding> BindingForSlot(
        std::uint16_t client_slot) const noexcept;

    std::uint16_t maximum_clients() const noexcept;

private:
    std::array<std::optional<GoldSrcClientEdictBinding>, 32u> bindings_{};
    std::array<std::uint32_t, 32u> generations_{};
    std::uint16_t maximum_clients_ = 32u;
};

enum class GoldSrcClientLifecycleCommandKind
{
    kNone,
    kSpawn,
};

enum class GoldSrcClientLifecycleCommandStatus
{
    kOk,
    kNullInput,
    kEmptyPayload,
    kPayloadTooLarge,
    kUnsupportedOpcode,
    kMissingTerminator,
    kEmbeddedNul,
    kInvalidControlCharacter,
    kCommandInjection,
    kUnsupportedCommand,
    kWrongArgumentCount,
    kInvalidSpawnCount,
    kInvalidChecksum,
    kNumericOverflow,
    kUnsupportedTrailingData,
};

std::string_view ReasonFor(
    GoldSrcClientLifecycleCommandStatus status) noexcept;

struct GoldSrcClientLifecycleCommand final
{
    GoldSrcClientLifecycleCommandKind kind =
        GoldSrcClientLifecycleCommandKind::kNone;
    std::uint32_t spawn_count = 0u;
    std::uint32_t checksum_token = 0u;
};

struct GoldSrcClientLifecycleCommandDecodeResult final
{
    GoldSrcClientLifecycleCommandStatus status =
        GoldSrcClientLifecycleCommandStatus::kEmptyPayload;
    GoldSrcClientLifecycleCommand command;

    bool ok() const noexcept
    {
        return status == GoldSrcClientLifecycleCommandStatus::kOk;
    }
};

// Decodes exactly one clc_stringcmd containing
// `spawn <decimal-spawn-count> <decimal-checksum>`. Protocol NOP bytes may
// surround the command. The decoder never executes or forwards text.
GoldSrcClientLifecycleCommandDecodeResult
DecodeGoldSrcClientLifecycleCommand(
    const std::uint8_t* bytes,
    std::size_t size) noexcept;

struct GoldSrcPlayerMaterialization final
{
    bool private_data_ready = false;
    bool player_entity_ready = false;
    bool player_spawned = false;
    bool finite_network_state = false;
    std::uint16_t model_index = 0u;
};

using GoldSrcClientConnectCallback = std::function<bool(
    const GoldSrcClientEdictBinding&,
    std::string_view,
    std::string_view,
    std::array<char, kGoldSrcClientRejectReasonBytes>&)>;
using GoldSrcClientPutInServerCallback = std::function<
    GoldSrcPlayerMaterialization(const GoldSrcClientEdictBinding&)>;
using GoldSrcClientDisconnectCallback =
    std::function<void(const GoldSrcClientEdictBinding&)>;

enum class GoldSrcPlayerLifecycleResult
{
    kAdvanced,
    kAlreadyApplied,
    kInvalidPhase,
    kInvalidBinding,
    kInvalidName,
    kInvalidAddress,
    kSpawnCountMismatch,
    kCallbackMissing,
    kGameDllRejected,
    kPlayerMaterializationFailed,
    kInvalidViewEntity,
    kStaleSession,
};

std::string_view ReasonFor(GoldSrcPlayerLifecycleResult result) noexcept;

struct GoldSrcPlayerLifecycleDiagnostics final
{
    std::uint64_t userinfo_notifications = 0u;
    std::uint64_t client_connect_calls = 0u;
    std::uint64_t client_connect_accepts = 0u;
    std::uint64_t client_connect_rejections = 0u;
    std::uint64_t duplicate_connect_suppressed = 0u;
    std::uint64_t client_put_in_server_calls = 0u;
    std::uint64_t client_put_in_server_successes = 0u;
    std::uint64_t duplicate_put_in_server_suppressed = 0u;
    std::uint64_t wrong_spawn_count_rejections = 0u;
    std::uint64_t invalid_phase_rejections = 0u;
    std::uint64_t view_assignments = 0u;
    std::uint64_t player_snapshot_frames = 0u;
    std::uint64_t disconnect_calls = 0u;
};

class GoldSrcPlayerLifecycleSession final
{
public:
    void Reset() noexcept;
    GoldSrcPlayerLifecycleResult BindClientEdict(
        const GoldSrcClientEdictBinding& binding) noexcept;
    GoldSrcPlayerLifecycleResult RecordInitialUserInfo(
        std::string_view player_name) noexcept;
    GoldSrcPlayerLifecycleResult ConnectGameDll(
        std::string_view player_name,
        std::string_view network_address,
        const GoldSrcClientConnectCallback& callback) noexcept;
    GoldSrcPlayerLifecycleResult EnterAwaitingPutInServer() noexcept;
    GoldSrcPlayerLifecycleResult PutInServer(
        std::uint32_t expected_spawn_count,
        const GoldSrcClientLifecycleCommand& command,
        const GoldSrcClientPutInServerCallback& callback) noexcept;
    GoldSrcPlayerLifecycleResult PutInServerFromReconciliation(
        const GoldSrcClientPutInServerCallback& callback) noexcept;
    GoldSrcPlayerLifecycleResult EstablishView(
        std::uint16_t view_entity_index) noexcept;
    GoldSrcPlayerLifecycleResult MarkPlayerSnapshot(
        std::uint32_t frame_id) noexcept;
    GoldSrcPlayerLifecycleResult MarkSignonComplete() noexcept;
    GoldSrcPlayerLifecycleResult Disconnect(
        const GoldSrcClientDisconnectCallback& callback) noexcept;

    GoldSrcPlayerLifecyclePhase phase() const noexcept;
    const GoldSrcClientEdictBinding& binding() const noexcept;
    const GoldSrcPlayerMaterialization& player() const noexcept;
    const GoldSrcPlayerLifecycleDiagnostics& diagnostics() const noexcept;
    std::string_view reject_reason() const noexcept;
    std::uint16_t view_entity_index() const noexcept;
    std::optional<std::uint32_t> first_player_snapshot_frame_id() const noexcept;
    bool game_dll_connected() const noexcept;
    bool put_in_server() const noexcept;
    bool signon_complete() const noexcept;
    bool gameplay_active() const noexcept;

private:
    GoldSrcPlayerLifecycleResult PutInServerImpl(
        const GoldSrcClientPutInServerCallback& callback) noexcept;

    GoldSrcPlayerLifecyclePhase phase_ =
        GoldSrcPlayerLifecyclePhase::kNoClientEdict;
    GoldSrcClientEdictBinding binding_{};
    GoldSrcPlayerMaterialization player_{};
    GoldSrcPlayerLifecycleDiagnostics diagnostics_{};
    std::array<char, kGoldSrcClientRejectReasonBytes> reject_reason_{};
    std::string player_name_;
    std::uint16_t view_entity_index_ = 0u;
    std::optional<std::uint32_t> first_player_snapshot_frame_id_;
    bool game_dll_connected_ = false;
    bool put_in_server_ = false;
    bool signon_complete_ = false;
};

struct GoldSrcPlayerLifecycleControl final
{
    std::uint16_t view_entity_index = 0u;
    std::array<float, 3u> view_angles{};
    std::uint8_t signon_number = kGoldSrcPlayerLifecycleSignonNumber;
};

struct GoldSrcPlayerLifecycleControlPayload final
{
    std::array<std::uint8_t, kGoldSrcPlayerLifecycleControlBytes> bytes{};
    std::size_t size = 0u;
};

enum class GoldSrcPlayerLifecycleControlStatus
{
    kOk,
    kNullInput,
    kWrongSize,
    kInvalidViewEntity,
    kNonFiniteAngle,
    kWrongMessageOrder,
    kWrongSignonNumber,
};

std::string_view ReasonFor(
    GoldSrcPlayerLifecycleControlStatus status) noexcept;

struct GoldSrcPlayerLifecycleControlEncodeResult final
{
    GoldSrcPlayerLifecycleControlStatus status =
        GoldSrcPlayerLifecycleControlStatus::kInvalidViewEntity;
    GoldSrcPlayerLifecycleControlPayload payload;

    bool ok() const noexcept
    {
        return status == GoldSrcPlayerLifecycleControlStatus::kOk;
    }
};

struct GoldSrcPlayerLifecycleControlDecodeResult final
{
    GoldSrcPlayerLifecycleControlStatus status =
        GoldSrcPlayerLifecycleControlStatus::kWrongSize;
    GoldSrcPlayerLifecycleControl control;

    bool ok() const noexcept
    {
        return status == GoldSrcPlayerLifecycleControlStatus::kOk;
    }
};

GoldSrcPlayerLifecycleControlEncodeResult
EncodeGoldSrcPlayerLifecycleControl(
    const GoldSrcPlayerLifecycleControl& control) noexcept;
GoldSrcPlayerLifecycleControlDecodeResult
DecodeGoldSrcPlayerLifecycleControl(
    const std::uint8_t* bytes,
    std::size_t size) noexcept;
} // namespace hl::network
