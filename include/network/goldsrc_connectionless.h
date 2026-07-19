#pragma once

#include "network/ipv4_endpoint.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace hl::network
{
// Framing and protocol-48 connect grammar follow public Xash3D protocol notes
// and client behavior. Response markers and shapes match observed public ReHLDS
// behavior; no implementation source is copied.
inline constexpr std::size_t kGoldSrcMaximumDatagramBytes = 2048;
inline constexpr std::int32_t kGoldSrcProtocolVersion = 48;
inline constexpr std::size_t kGoldSrcMaximumInfoBytes = 255;
inline constexpr std::size_t kGoldSrcMaximumInfoPairs = 32;
inline constexpr std::size_t kGoldSrcMaximumInfoKeyBytes = 63;
inline constexpr std::size_t kGoldSrcMaximumInfoValueBytes = 127;
inline constexpr std::size_t kGoldSrcMaximumRejectionReasonBytes = 127;

struct InfoPair final
{
    std::string key;
    std::string value;
};

struct InfoString final
{
    std::vector<InfoPair> pairs;

    const std::string* Find(std::string_view key) const noexcept;
};

enum class InfoParseStatus
{
    kOk,
    kTooLong,
    kMissingLeadingBackslash,
    kMissingValue,
    kEmptyComponent,
    kTooManyPairs,
    kKeyTooLong,
    kValueTooLong,
    kControlCharacter,
    kDuplicateKey,
};

struct InfoParseResult final
{
    InfoParseStatus status = InfoParseStatus::kOk;
    std::string reason = "ok";
    InfoString info;

    bool ok() const noexcept
    {
        return status == InfoParseStatus::kOk;
    }
};

InfoParseResult ParseInfoString(std::string_view text);
std::string SanitizePlayerName(std::string_view name);
std::string_view ReasonFor(InfoParseStatus status) noexcept;

struct ConnectRequest final
{
    std::int32_t protocol = 0;
    std::int32_t challenge = 0;
    std::string protocol_info;
    std::string user_info;
    InfoString parsed_protocol_info;
    InfoString parsed_user_info;
    std::int32_t authentication_protocol = 0;
    std::string authentication_raw;
    std::optional<std::uint16_t> qport;
    std::optional<std::int32_t> extensions;
    std::string name;
    std::string sanitized_name;
    std::vector<std::uint8_t> auth_tail;
};

enum class ConnectionlessCommand
{
    kUnknown,
    kGetChallenge,
    kConnect,
};

enum class ConnectionlessDecodeStatus
{
    kOk,
    kTruncated,
    kOversized,
    kInvalidPrefix,
    kMalformed,
    kUnsupportedCommand,
    kUnsupportedProtocol,
    kUnsupportedAuthentication,
};

struct ConnectionlessDecodeResult final
{
    ConnectionlessDecodeStatus status = ConnectionlessDecodeStatus::kMalformed;
    ConnectionlessCommand command = ConnectionlessCommand::kUnknown;
    std::string reason = "malformed_datagram";
    std::optional<ConnectRequest> connect;

    bool ok() const noexcept
    {
        return status == ConnectionlessDecodeStatus::kOk;
    }
};

ConnectionlessDecodeResult DecodeConnectionlessDatagram(
    const std::uint8_t* bytes,
    std::size_t size);
ConnectionlessDecodeResult DecodeConnectionlessDatagram(
    const std::vector<std::uint8_t>& datagram);
std::string_view ReasonFor(ConnectionlessDecodeStatus status) noexcept;

std::vector<std::uint8_t> BuildChallengeResponse(std::int32_t challenge);
std::vector<std::uint8_t> BuildRejectionResponse(std::string_view reason);
std::vector<std::uint8_t> BuildAcceptanceResponse(
    int slot,
    const Ipv4Endpoint& endpoint);

enum class ChallengeValidationResult
{
    kValid,
    kUnknown,
    kEndpointMismatch,
    kExpired,
    kConsumed,
};

std::string_view ReasonFor(ChallengeValidationResult result) noexcept;

class ChallengeTable final
{
public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;
    using Duration = Clock::duration;
    using Generator = std::function<std::int32_t()>;

    struct IssueResult final
    {
        std::int32_t challenge = 0;
        std::size_t expired_count = 0;
        bool replaced_same_host = false;
        bool evicted_oldest = false;
    };

    ChallengeTable(std::size_t capacity, Duration time_to_live, Generator generator);

    IssueResult Issue(const Ipv4Endpoint& endpoint, TimePoint now);
    ChallengeValidationResult Validate(
        const Ipv4Endpoint& endpoint,
        std::int32_t challenge,
        TimePoint now) const noexcept;
    ChallengeValidationResult Consume(
        const Ipv4Endpoint& endpoint,
        std::int32_t challenge,
        TimePoint now) noexcept;
    std::size_t Expire(TimePoint now) noexcept;
    std::size_t size() const noexcept;
    std::size_t capacity() const noexcept;

private:
    struct Entry final
    {
        Ipv4Endpoint endpoint;
        std::int32_t challenge = 0;
        TimePoint issued_at{};
        TimePoint expires_at{};
        std::uint64_t serial = 0;
        bool consumed = false;
    };

    std::size_t capacity_ = 0;
    Duration time_to_live_{};
    Generator generator_;
    std::uint64_t next_serial_ = 0;
    std::vector<Entry> entries_;
};

struct AdmissionRequest final
{
    Ipv4Endpoint endpoint;
    ConnectRequest connect;
};

struct AdmissionResult final
{
    bool accepted = false;
    int slot = -1;
    std::string session_identifier;
    std::string session_state;
    bool spawned = false;
    bool active = false;
    std::string rejection_reason = "admission_rejected";
};

// Preflight must only inspect admission state and return the plan that commit
// will apply. HandshakeProcessor validates the plan before invoking commit.
// Commit is an all-or-nothing operation: false or an exception must leave the
// authoritative session state unchanged. It must not re-enter this processor
// or mutate the challenge table owned by it.
using AdmissionPreflightCallback =
    std::function<AdmissionResult(const AdmissionRequest&)>;
using AdmissionCommitCallback =
    std::function<bool(const AdmissionRequest&, const AdmissionResult&)>;

enum class HandshakeOutcome
{
    kMalformedDatagram,
    kUnsupportedCommand,
    kChallengeIssued,
    kConnectRejected,
    kConnectAccepted,
};

std::string_view ReasonFor(HandshakeOutcome outcome) noexcept;

struct HandshakeResult final
{
    HandshakeOutcome outcome = HandshakeOutcome::kMalformedDatagram;
    std::string reason = "malformed_datagram";
    Ipv4Endpoint endpoint;
    std::optional<std::vector<std::uint8_t>> response;
    std::optional<std::int32_t> issued_challenge;
    std::optional<ConnectRequest> connect;
    std::optional<AdmissionResult> admission;
};

class HandshakeProcessor final
{
public:
    HandshakeProcessor(
        ChallengeTable& challenges,
        AdmissionPreflightCallback admission_preflight,
        AdmissionCommitCallback admission_commit);

    HandshakeResult Process(
        const Ipv4Endpoint& endpoint,
        const std::uint8_t* bytes,
        std::size_t size,
        ChallengeTable::TimePoint now);
    HandshakeResult Process(
        const Ipv4Endpoint& endpoint,
        const std::vector<std::uint8_t>& datagram,
        ChallengeTable::TimePoint now);

private:
    ChallengeTable& challenges_;
    AdmissionPreflightCallback admission_preflight_;
    AdmissionCommitCallback admission_commit_;
};
} // namespace hl::network
