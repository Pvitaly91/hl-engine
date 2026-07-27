#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace hl::network
{
inline constexpr std::uint8_t kGoldSrcClientNopOpcode = 1u;
inline constexpr std::uint8_t kGoldSrcClientStringCommandOpcode = 3u;
inline constexpr std::uint8_t kGoldSrcServerInfoOpcode = 11u;
inline constexpr std::uint8_t kGoldSrcSendExtraInfoOpcode = 54u;
inline constexpr std::uint32_t kGoldSrcServerInfoProtocolVersion = 48u;
inline constexpr std::size_t kGoldSrcClientDllDigestBytes = 16u;
inline constexpr std::size_t kGoldSrcMaximumSignonPayloadBytes = 1200u;
inline constexpr std::size_t kGoldSrcMaximumClientCommandBytes = 64u;
inline constexpr std::size_t kGoldSrcMaximumCloseMenusCompanions = 2u;
inline constexpr std::size_t kGoldSrcMaximumGameDirectoryBytes = 63u;
inline constexpr std::size_t kGoldSrcMaximumHostnameBytes = 255u;
inline constexpr std::size_t kGoldSrcMaximumModelPathBytes = 63u;
inline constexpr std::size_t kGoldSrcMaximumMapcycleBytes = 1023u;
inline constexpr std::size_t kGoldSrcMaximumFallbackDirectoryBytes = 63u;
inline constexpr std::size_t kGoldSrcMaximumSendExtraInfoPayloadBytes =
    1u + kGoldSrcMaximumFallbackDirectoryBytes + 1u + 1u;
inline constexpr std::size_t kGoldSrcBspLumpCount = 15u;
inline constexpr std::size_t kGoldSrcBsp30HeaderBytes =
    4u + (kGoldSrcBspLumpCount * 8u);
inline constexpr std::uint64_t kGoldSrcMaximumBspFileBytes =
    512ull * 1024ull * 1024ull;

enum class GoldSrcClientSignonCommand
{
    kNone,
    kNew,
    kSendResources,
};

enum class GoldSrcClientSignonCompanionCommand
{
    kNone,
    kCloseMenus,
};

enum class GoldSrcClientSignonDecodeStatus
{
    kOk,
    kNullInput,
    kEmptyPayload,
    kPayloadTooLarge,
    kMissingCommand,
    kUnsupportedOpcode,
    kMissingStringTerminator,
    kCommandTooLong,
    kEmptyCommand,
    kInvalidControlByte,
    kUnsupportedCommand,
    kMultipleCommands,
    kUnsupportedCompanionCommand,
    kUnsupportedCompanionCount,
    kTooManyCompanionCommands,
    kUnsupportedTrailingData,
};

std::string_view ReasonFor(GoldSrcClientSignonDecodeStatus status) noexcept;

struct GoldSrcClientSignonDecodeResult final
{
    GoldSrcClientSignonDecodeStatus status =
        GoldSrcClientSignonDecodeStatus::kEmptyPayload;
    GoldSrcClientSignonCommand command = GoldSrcClientSignonCommand::kNone;
    GoldSrcClientSignonCompanionCommand companion_command =
        GoldSrcClientSignonCompanionCommand::kNone;
    std::size_t companion_count = 0;
    std::size_t leading_nop_count = 0;
    std::size_t trailing_nop_count = 0;

    bool ok() const noexcept
    {
        return status == GoldSrcClientSignonDecodeStatus::kOk;
    }
};

// Decodes one bounded application payload after the client qport has already
// been removed by the netchan layer. The primary command must be exact `new`
// or `sendres`. The observed stock-client `sendres` payload may additionally
// contain exactly two `closemenus` companion commands with the observed
// single-space/LF suffix. NOP messages may surround or separate accepted
// commands. Companions are typed metadata only and are never routed for
// generic command execution.
GoldSrcClientSignonDecodeResult DecodeGoldSrcClientSignonPayload(
    const std::uint8_t* bytes,
    std::size_t size) noexcept;

enum class GoldSrcSignonPhase
{
    kNone,
    kAwaitingNew,
    kServerInfoQueued,
    kServerInfoSentAwaitingAck,
    kServerInfoAcknowledged,
    kAwaitingResourceRequest,
    kResourceManifestQueued,
    kResourceManifestSentAwaitingAck,
    kResourceManifestAcknowledged,
};

std::string_view NameFor(GoldSrcSignonPhase phase) noexcept;

enum class GoldSrcSignonTransitionResult
{
    kAdvanced,
    kAlreadyApplied,
    kInvalidPhase,
};

std::string_view ReasonFor(GoldSrcSignonTransitionResult result) noexcept;

enum class GoldSrcSignonCommandDisposition
{
    kDelivered,
    kDuplicateSuppressed,
    kWrongPhase,
    kUnsupported,
};

std::string_view ReasonFor(GoldSrcSignonCommandDisposition disposition) noexcept;

struct GoldSrcSignonDiagnostics final
{
    std::uint64_t client_new_received = 0;
    std::uint64_t client_new_delivered = 0;
    std::uint64_t duplicate_new_suppressed = 0;
    std::uint64_t new_wrong_phase = 0;
    std::uint64_t serverinfo_queued = 0;
    std::uint64_t serverinfo_sent = 0;
    std::uint64_t serverinfo_acknowledged = 0;
    std::uint64_t resource_request_received = 0;
    std::uint64_t resource_request_delivered = 0;
    std::uint64_t duplicate_resource_request_suppressed = 0;
    std::uint64_t resource_request_wrong_phase = 0;
    std::uint64_t resource_manifest_queued = 0;
    std::uint64_t resource_manifest_sent = 0;
    std::uint64_t resource_manifest_acknowledged = 0;
};

class GoldSrcSignonSessionState final
{
public:
    void Reset() noexcept;
    GoldSrcSignonTransitionResult EnterAwaitingNew() noexcept;
    GoldSrcSignonCommandDisposition HandleClientCommand(
        GoldSrcClientSignonCommand command) noexcept;
    GoldSrcSignonTransitionResult MarkServerInfoSent() noexcept;
    GoldSrcSignonTransitionResult MarkServerInfoAcknowledged() noexcept;
    GoldSrcSignonTransitionResult EnterAwaitingResourceRequest() noexcept;
    GoldSrcSignonTransitionResult MarkResourceManifestSent() noexcept;
    GoldSrcSignonTransitionResult MarkResourceManifestAcknowledged() noexcept;

    GoldSrcSignonPhase phase() const noexcept;
    const GoldSrcSignonDiagnostics& diagnostics() const noexcept;

private:
    GoldSrcSignonPhase phase_ = GoldSrcSignonPhase::kNone;
    GoldSrcSignonDiagnostics diagnostics_{};
};

struct GoldSrcServerInfoContext final
{
    std::uint32_t protocol_version = kGoldSrcServerInfoProtocolVersion;
    std::uint32_t spawn_count = 0;
    std::optional<std::uint32_t> canonical_map_checksum;
    std::optional<std::array<std::uint8_t, kGoldSrcClientDllDigestBytes>>
        client_dll_md5;
    std::uint8_t max_clients = 0;
    std::uint8_t player_index = 0;
    bool deathmatch = false;
    std::string game_directory;
    std::string hostname;
    std::string map_model_path;
    std::string mapcycle;
    bool secure = false;
    std::string fallback_game_directory;
    bool cheats = false;
};

enum class GoldSrcServerInfoCodecStatus
{
    kOk,
    kUnsupportedProtocol,
    kInvalidMaxClients,
    kInvalidPlayerIndex,
    kMissingMapChecksum,
    kZeroMapChecksum,
    kMissingClientDllDigest,
    kZeroClientDllDigest,
    kEmptyRequiredString,
    kStringTooLong,
    kEmbeddedNul,
    kInvalidControlByte,
    kInvalidModelPath,
    kSecureModeUnsupported,
    kCheatsUnsupported,
    kPayloadTooLarge,
};

std::string_view ReasonFor(GoldSrcServerInfoCodecStatus status) noexcept;

struct GoldSrcServerInfoValidationResult final
{
    GoldSrcServerInfoCodecStatus status =
        GoldSrcServerInfoCodecStatus::kEmptyRequiredString;

    bool ok() const noexcept
    {
        return status == GoldSrcServerInfoCodecStatus::kOk;
    }
};

GoldSrcServerInfoValidationResult ValidateGoldSrcServerInfoContext(
    const GoldSrcServerInfoContext& context) noexcept;

// Returns the exact uint32 value that ReHLDS writes after COM_Munge3. The
// caller still writes that value explicitly in little-endian order.
std::uint32_t MungeGoldSrcServerInfoChecksum(
    std::uint32_t canonical_checksum,
    std::uint8_t player_index) noexcept;

// Inverts MungeGoldSrcServerInfoChecksum for a checksum read from the wire.
std::uint32_t UnmungeGoldSrcServerInfoChecksum(
    std::uint32_t wire_checksum,
    std::uint8_t player_index) noexcept;

struct GoldSrcServerInfoPayload final
{
    std::array<std::uint8_t, kGoldSrcMaximumSignonPayloadBytes> bytes{};
    std::size_t size = 0;
};

struct GoldSrcServerInfoEncodeResult final
{
    GoldSrcServerInfoCodecStatus status =
        GoldSrcServerInfoCodecStatus::kEmptyRequiredString;
    GoldSrcServerInfoPayload payload;

    bool ok() const noexcept
    {
        return status == GoldSrcServerInfoCodecStatus::kOk;
    }
};

GoldSrcServerInfoEncodeResult EncodeGoldSrcServerInfo(
    const GoldSrcServerInfoContext& context) noexcept;

struct GoldSrcSendExtraInfoPayload final
{
    std::array<std::uint8_t, kGoldSrcMaximumSendExtraInfoPayloadBytes> bytes{};
    std::size_t size = 0;
};

struct GoldSrcSendExtraInfoEncodeResult final
{
    GoldSrcServerInfoCodecStatus status =
        GoldSrcServerInfoCodecStatus::kEmptyRequiredString;
    GoldSrcSendExtraInfoPayload payload;

    bool ok() const noexcept
    {
        return status == GoldSrcServerInfoCodecStatus::kOk;
    }
};

// Encodes exactly one bounded svc_sendextrainfo companion. This API is also
// used by EncodeGoldSrcServerInfo so the standalone and combined forms cannot
// drift apart.
GoldSrcSendExtraInfoEncodeResult EncodeGoldSrcSendExtraInfo(
    std::string_view fallback_game_directory,
    bool cheats) noexcept;

template <std::size_t Capacity>
struct GoldSrcBoundedProtocolString final
{
    std::array<char, Capacity + 1u> bytes{};
    std::size_t size = 0;

    std::string_view view() const noexcept
    {
        return std::string_view(bytes.data(), size);
    }
};

struct GoldSrcDecodedServerInfo final
{
    std::uint32_t protocol_version = 0;
    std::uint32_t spawn_count = 0;
    std::uint32_t wire_map_checksum = 0;
    std::uint32_t canonical_map_checksum = 0;
    std::array<std::uint8_t, kGoldSrcClientDllDigestBytes> client_dll_md5{};
    std::uint8_t max_clients = 0;
    std::uint8_t player_index = 0;
    bool deathmatch = false;
    GoldSrcBoundedProtocolString<kGoldSrcMaximumGameDirectoryBytes>
        game_directory;
    GoldSrcBoundedProtocolString<kGoldSrcMaximumHostnameBytes> hostname;
    GoldSrcBoundedProtocolString<kGoldSrcMaximumModelPathBytes> map_model_path;
    GoldSrcBoundedProtocolString<kGoldSrcMaximumMapcycleBytes> mapcycle;
    bool secure = false;
    GoldSrcBoundedProtocolString<kGoldSrcMaximumFallbackDirectoryBytes>
        fallback_game_directory;
    bool cheats = false;
};

enum class GoldSrcServerInfoDecodeStatus
{
    kOk,
    kNullInput,
    kEmptyPayload,
    kPayloadTooLarge,
    kTruncatedField,
    kWrongServerInfoOpcode,
    kUnsupportedProtocol,
    kZeroMapChecksum,
    kZeroClientDllDigest,
    kInvalidMaxClients,
    kInvalidPlayerIndex,
    kInvalidBooleanValue,
    kMissingStringTerminator,
    kStringTooLong,
    kEmptyRequiredString,
    kInvalidControlByte,
    kInvalidModelPath,
    kSecureModeUnsupported,
    kWrongCompanionOpcode,
    kCheatsUnsupported,
    kTrailingData,
};

std::string_view ReasonFor(GoldSrcServerInfoDecodeStatus status) noexcept;

struct GoldSrcServerInfoDecodeResult final
{
    GoldSrcServerInfoDecodeStatus status =
        GoldSrcServerInfoDecodeStatus::kEmptyPayload;
    GoldSrcDecodedServerInfo server_info;

    bool ok() const noexcept
    {
        return status == GoldSrcServerInfoDecodeStatus::kOk;
    }
};

// Parses exactly one svc_serverinfo followed by the required
// svc_sendextrainfo companion. The result owns all strings in fixed-capacity
// storage and no trailing opcode or byte is accepted.
GoldSrcServerInfoDecodeResult DecodeGoldSrcServerInfo(
    const std::uint8_t* bytes,
    std::size_t size) noexcept;

enum class GoldSrcMapChecksumStatus
{
    kOk,
    kNullInput,
    kOpenFailed,
    kReadFailed,
    kFileTooLarge,
    kTruncatedHeader,
    kUnsupportedBspVersion,
    kInvalidLumpBounds,
};

std::string_view ReasonFor(GoldSrcMapChecksumStatus status) noexcept;

struct GoldSrcMapChecksumResult final
{
    GoldSrcMapChecksumStatus status = GoldSrcMapChecksumStatus::kNullInput;
    std::uint32_t checksum = 0;

    bool ok() const noexcept
    {
        return status == GoldSrcMapChecksumStatus::kOk;
    }
};

// GoldSrc BSP30 map identity is the non-finalized CRC32 running accumulator:
// init FFFFFFFF, process lump payloads 1..14 in lump-index order, no final xor.
GoldSrcMapChecksumResult ComputeGoldSrcBsp30MapChecksum(
    const std::uint8_t* bytes,
    std::size_t size) noexcept;
GoldSrcMapChecksumResult ComputeGoldSrcBsp30MapChecksum(
    const std::filesystem::path& path);

enum class GoldSrcMd5Status
{
    kOk,
    kNullInput,
    kOpenFailed,
    kReadFailed,
};

std::string_view ReasonFor(GoldSrcMd5Status status) noexcept;

struct GoldSrcMd5Result final
{
    GoldSrcMd5Status status = GoldSrcMd5Status::kNullInput;
    std::array<std::uint8_t, kGoldSrcClientDllDigestBytes> digest{};

    bool ok() const noexcept
    {
        return status == GoldSrcMd5Status::kOk;
    }
};

GoldSrcMd5Result ComputeGoldSrcMd5(
    const std::uint8_t* bytes,
    std::size_t size) noexcept;
GoldSrcMd5Result ComputeGoldSrcClientDllMd5(
    const std::filesystem::path& resolved_client_dll_path);
} // namespace hl::network
