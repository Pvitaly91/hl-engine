#include "network/goldsrc_connectionless.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <stdexcept>
#include <utility>

namespace
{
using hl::network::ConnectionlessCommand;
using hl::network::ConnectionlessDecodeResult;
using hl::network::ConnectionlessDecodeStatus;
using hl::network::ConnectRequest;
using hl::network::InfoParseResult;
using hl::network::InfoParseStatus;

constexpr std::array<std::uint8_t, 4> kConnectionlessPrefix = {
    0xFFu,
    0xFFu,
    0xFFu,
    0xFFu,
};

bool AsciiEqualsIgnoreCase(std::string_view left, std::string_view right) noexcept
{
    if (left.size() != right.size())
    {
        return false;
    }

    for (std::size_t index = 0; index < left.size(); ++index)
    {
        const auto left_byte = static_cast<unsigned char>(left[index]);
        const auto right_byte = static_cast<unsigned char>(right[index]);
        const unsigned char folded_left =
            left_byte >= static_cast<unsigned char>('A')
                && left_byte <= static_cast<unsigned char>('Z')
            ? static_cast<unsigned char>(left_byte + ('a' - 'A'))
            : left_byte;
        const unsigned char folded_right =
            right_byte >= static_cast<unsigned char>('A')
                && right_byte <= static_cast<unsigned char>('Z')
            ? static_cast<unsigned char>(right_byte + ('a' - 'A'))
            : right_byte;
        if (folded_left != folded_right)
        {
            return false;
        }
    }

    return true;
}

bool ContainsControlCharacter(std::string_view text) noexcept
{
    return std::any_of(
        text.begin(),
        text.end(),
        [](char character)
        {
            const auto byte = static_cast<unsigned char>(character);
            return byte < 0x20u || byte == 0x7Fu;
        });
}

template <typename Integer>
bool ParseInteger(std::string_view text, Integer& value) noexcept
{
    if (text.empty())
    {
        return false;
    }

    const char* const begin = text.data();
    const char* const end = begin + text.size();
    const auto result = std::from_chars(begin, end, value, 10);
    return result.ec == std::errc{} && result.ptr == end;
}

struct CommandToken final
{
    std::string text;
    bool quoted = false;
};

bool IsCommandWhitespace(char character) noexcept
{
    return character == ' ' || character == '\t';
}

bool TokenizeCommandLine(
    std::string_view line,
    std::vector<CommandToken>& tokens,
    std::string& reason)
{
    std::size_t offset = 0;
    while (offset < line.size())
    {
        while (offset < line.size() && IsCommandWhitespace(line[offset]))
        {
            ++offset;
        }

        if (offset == line.size())
        {
            break;
        }

        CommandToken token{};
        if (line[offset] == '"')
        {
            token.quoted = true;
            ++offset;
            const std::size_t value_begin = offset;
            while (offset < line.size() && line[offset] != '"')
            {
                const auto byte = static_cast<unsigned char>(line[offset]);
                if (byte < 0x20u || byte == 0x7Fu)
                {
                    reason = "connect_control_character";
                    return false;
                }

                ++offset;
            }

            if (offset == line.size())
            {
                reason = "connect_unterminated_quote";
                return false;
            }

            token.text.assign(line.substr(value_begin, offset - value_begin));
            ++offset;
            if (offset < line.size() && !IsCommandWhitespace(line[offset]))
            {
                reason = "connect_text_after_quote";
                return false;
            }
        }
        else
        {
            const std::size_t value_begin = offset;
            while (offset < line.size() && !IsCommandWhitespace(line[offset]))
            {
                const auto byte = static_cast<unsigned char>(line[offset]);
                if (line[offset] == '"')
                {
                    reason = "connect_unexpected_quote";
                    return false;
                }

                if (byte < 0x21u || byte > 0x7Eu)
                {
                    reason = "connect_control_character";
                    return false;
                }

                ++offset;
            }

            token.text.assign(line.substr(value_begin, offset - value_begin));
        }

        tokens.push_back(std::move(token));
    }

    return true;
}

ConnectionlessDecodeResult MalformedConnect(
    ConnectionlessDecodeStatus status,
    std::string reason)
{
    ConnectionlessDecodeResult result{};
    result.status = status;
    result.command = ConnectionlessCommand::kConnect;
    result.reason = std::move(reason);
    return result;
}

ConnectionlessDecodeResult DecodeConnect(
    const std::uint8_t* body,
    std::size_t body_size)
{
    const auto* const newline = static_cast<const std::uint8_t*>(
        std::find(body, body + body_size, static_cast<std::uint8_t>('\n')));
    if (newline == body + body_size)
    {
        return MalformedConnect(
            ConnectionlessDecodeStatus::kMalformed,
            "connect_missing_newline");
    }

    const std::size_t line_size = static_cast<std::size_t>(newline - body);
    const std::string_view line(
        reinterpret_cast<const char*>(body),
        line_size);
    std::vector<CommandToken> tokens;
    std::string token_error;
    if (!TokenizeCommandLine(line, tokens, token_error))
    {
        return MalformedConnect(ConnectionlessDecodeStatus::kMalformed, token_error);
    }

    if (tokens.size() != 5u || tokens[0].text != "connect")
    {
        return MalformedConnect(
            ConnectionlessDecodeStatus::kMalformed,
            "connect_field_count");
    }

    if (tokens[0].quoted || tokens[1].quoted || tokens[2].quoted
        || !tokens[3].quoted || !tokens[4].quoted)
    {
        return MalformedConnect(
            ConnectionlessDecodeStatus::kMalformed,
            "connect_quoting");
    }

    ConnectRequest request{};
    if (!ParseInteger(tokens[1].text, request.protocol))
    {
        return MalformedConnect(
            ConnectionlessDecodeStatus::kMalformed,
            "connect_invalid_protocol");
    }

    if (request.protocol != hl::network::kGoldSrcProtocolVersion)
    {
        return MalformedConnect(
            ConnectionlessDecodeStatus::kUnsupportedProtocol,
            "unsupported_protocol");
    }

    if (!ParseInteger(tokens[2].text, request.challenge))
    {
        return MalformedConnect(
            ConnectionlessDecodeStatus::kMalformed,
            "connect_invalid_challenge");
    }

    request.protocol_info = std::move(tokens[3].text);
    request.user_info = std::move(tokens[4].text);

    InfoParseResult protocol_info = hl::network::ParseInfoString(request.protocol_info);
    if (!protocol_info.ok())
    {
        return MalformedConnect(
            ConnectionlessDecodeStatus::kMalformed,
            "protinfo_" + protocol_info.reason);
    }

    InfoParseResult user_info = hl::network::ParseInfoString(request.user_info);
    if (!user_info.ok())
    {
        return MalformedConnect(
            ConnectionlessDecodeStatus::kMalformed,
            "userinfo_" + user_info.reason);
    }

    request.parsed_protocol_info = std::move(protocol_info.info);
    request.parsed_user_info = std::move(user_info.info);

    const std::string* const authentication_protocol =
        request.parsed_protocol_info.Find("prot");
    if (authentication_protocol == nullptr)
    {
        return MalformedConnect(
            ConnectionlessDecodeStatus::kUnsupportedAuthentication,
            "missing_auth_protocol");
    }

    if (!ParseInteger(*authentication_protocol, request.authentication_protocol))
    {
        return MalformedConnect(
            ConnectionlessDecodeStatus::kMalformed,
            "invalid_auth_protocol");
    }

    if (request.authentication_protocol != 3)
    {
        return MalformedConnect(
            ConnectionlessDecodeStatus::kUnsupportedAuthentication,
            "unsupported_auth_protocol");
    }

    const std::string* const authentication_raw =
        request.parsed_protocol_info.Find("raw");
    if (authentication_raw == nullptr)
    {
        return MalformedConnect(
            ConnectionlessDecodeStatus::kUnsupportedAuthentication,
            "missing_auth_raw");
    }

    if (*authentication_raw != "steam")
    {
        return MalformedConnect(
            ConnectionlessDecodeStatus::kUnsupportedAuthentication,
            "unsupported_auth_raw");
    }
    request.authentication_raw = *authentication_raw;

    const std::string* const player_name = request.parsed_user_info.Find("name");
    if (player_name == nullptr)
    {
        return MalformedConnect(
            ConnectionlessDecodeStatus::kMalformed,
            "userinfo_missing_name");
    }
    request.name = *player_name;
    request.sanitized_name = hl::network::SanitizePlayerName(request.name);

    const std::string* const qport_text = request.parsed_protocol_info.Find("qport");
    if (qport_text != nullptr)
    {
        std::uint32_t parsed_qport = 0;
        if (!ParseInteger(*qport_text, parsed_qport) || parsed_qport > 65535u)
        {
            return MalformedConnect(
                ConnectionlessDecodeStatus::kMalformed,
                "invalid_qport");
        }
        request.qport = static_cast<std::uint16_t>(parsed_qport);
    }

    const std::string* const extensions_text = request.parsed_protocol_info.Find("ext");
    if (extensions_text != nullptr)
    {
        std::int32_t parsed_extensions = 0;
        if (!ParseInteger(*extensions_text, parsed_extensions))
        {
            return MalformedConnect(
                ConnectionlessDecodeStatus::kMalformed,
                "invalid_extensions");
        }
        request.extensions = parsed_extensions;
    }

    const std::uint8_t* const auth_begin = newline + 1;
    request.auth_tail.assign(auth_begin, body + body_size);

    ConnectionlessDecodeResult result{};
    result.status = ConnectionlessDecodeStatus::kOk;
    result.command = ConnectionlessCommand::kConnect;
    result.reason = "ok";
    result.connect = std::move(request);
    return result;
}

std::string SanitizeRejectionReason(std::string_view reason)
{
    if (reason.empty())
    {
        reason = "admission_rejected";
    }

    std::string sanitized;
    sanitized.reserve(std::min(reason.size(), hl::network::kGoldSrcMaximumRejectionReasonBytes));
    for (const char character : reason)
    {
        if (sanitized.size() == hl::network::kGoldSrcMaximumRejectionReasonBytes)
        {
            break;
        }

        const auto byte = static_cast<unsigned char>(character);
        sanitized.push_back(byte >= 0x20u && byte <= 0x7Eu ? character : '?');
    }
    return sanitized;
}

void AppendConnectionlessPrefix(std::vector<std::uint8_t>& output)
{
    output.insert(output.end(), kConnectionlessPrefix.begin(), kConnectionlessPrefix.end());
}

void AppendAscii(std::vector<std::uint8_t>& output, std::string_view text)
{
    output.insert(output.end(), text.begin(), text.end());
}
} // namespace

namespace hl::network
{
const std::string* InfoString::Find(std::string_view key) const noexcept
{
    const auto found = std::find_if(
        pairs.begin(),
        pairs.end(),
        [key](const InfoPair& pair)
        {
            return AsciiEqualsIgnoreCase(pair.key, key);
        });
    return found == pairs.end() ? nullptr : &found->value;
}

InfoParseResult ParseInfoString(std::string_view text)
{
    InfoParseResult result{};
    if (text.size() > kGoldSrcMaximumInfoBytes)
    {
        result.status = InfoParseStatus::kTooLong;
        result.reason = std::string(ReasonFor(result.status));
        return result;
    }

    if (text.empty() || text.front() != '\\')
    {
        result.status = InfoParseStatus::kMissingLeadingBackslash;
        result.reason = std::string(ReasonFor(result.status));
        return result;
    }

    std::vector<std::string_view> components;
    std::size_t component_begin = 1;
    while (true)
    {
        const std::size_t separator = text.find('\\', component_begin);
        const std::size_t component_end =
            separator == std::string_view::npos ? text.size() : separator;
        const std::string_view component =
            text.substr(component_begin, component_end - component_begin);
        if (component.empty())
        {
            result.status = InfoParseStatus::kEmptyComponent;
            result.reason = std::string(ReasonFor(result.status));
            return result;
        }
        components.push_back(component);

        if (separator == std::string_view::npos)
        {
            break;
        }
        component_begin = separator + 1;
    }

    if (components.size() % 2u != 0u)
    {
        result.status = InfoParseStatus::kMissingValue;
        result.reason = std::string(ReasonFor(result.status));
        return result;
    }

    const std::size_t pair_count = components.size() / 2u;
    if (pair_count > kGoldSrcMaximumInfoPairs)
    {
        result.status = InfoParseStatus::kTooManyPairs;
        result.reason = std::string(ReasonFor(result.status));
        return result;
    }

    result.info.pairs.reserve(pair_count);
    for (std::size_t pair_index = 0; pair_index < pair_count; ++pair_index)
    {
        const std::string_view key = components[pair_index * 2u];
        const std::string_view value = components[pair_index * 2u + 1u];
        if (key.size() > kGoldSrcMaximumInfoKeyBytes)
        {
            result.status = InfoParseStatus::kKeyTooLong;
            result.reason = std::string(ReasonFor(result.status));
            return result;
        }
        if (value.size() > kGoldSrcMaximumInfoValueBytes)
        {
            result.status = InfoParseStatus::kValueTooLong;
            result.reason = std::string(ReasonFor(result.status));
            return result;
        }
        if (ContainsControlCharacter(key) || ContainsControlCharacter(value))
        {
            result.status = InfoParseStatus::kControlCharacter;
            result.reason = std::string(ReasonFor(result.status));
            return result;
        }
        if (result.info.Find(key) != nullptr)
        {
            result.status = InfoParseStatus::kDuplicateKey;
            result.reason = std::string(ReasonFor(result.status));
            return result;
        }

        result.info.pairs.push_back({std::string(key), std::string(value)});
    }

    result.status = InfoParseStatus::kOk;
    result.reason = std::string(ReasonFor(result.status));
    return result;
}

std::string SanitizePlayerName(std::string_view name)
{
    std::string sanitized;
    sanitized.reserve(name.size());
    for (const char character : name)
    {
        const auto byte = static_cast<unsigned char>(character);
        const bool safe = byte >= 0x20u && byte <= 0x7Eu
            && character != '"' && character != '\\';
        sanitized.push_back(safe ? character : '?');
    }
    return sanitized;
}

std::string_view ReasonFor(InfoParseStatus status) noexcept
{
    switch (status)
    {
    case InfoParseStatus::kOk:
        return "ok";
    case InfoParseStatus::kTooLong:
        return "too_long";
    case InfoParseStatus::kMissingLeadingBackslash:
        return "missing_leading_backslash";
    case InfoParseStatus::kMissingValue:
        return "missing_value";
    case InfoParseStatus::kEmptyComponent:
        return "empty_component";
    case InfoParseStatus::kTooManyPairs:
        return "too_many_pairs";
    case InfoParseStatus::kKeyTooLong:
        return "key_too_long";
    case InfoParseStatus::kValueTooLong:
        return "value_too_long";
    case InfoParseStatus::kControlCharacter:
        return "control_character";
    case InfoParseStatus::kDuplicateKey:
        return "duplicate_key";
    }
    return "unknown_info_status";
}

ConnectionlessDecodeResult DecodeConnectionlessDatagram(
    const std::uint8_t* bytes,
    std::size_t size)
{
    ConnectionlessDecodeResult result{};
    if (size > kGoldSrcMaximumDatagramBytes)
    {
        result.status = ConnectionlessDecodeStatus::kOversized;
        result.reason = std::string(ReasonFor(result.status));
        return result;
    }

    if (bytes == nullptr || size < kConnectionlessPrefix.size())
    {
        result.status = ConnectionlessDecodeStatus::kTruncated;
        result.reason = std::string(ReasonFor(result.status));
        return result;
    }

    if (!std::equal(kConnectionlessPrefix.begin(), kConnectionlessPrefix.end(), bytes))
    {
        result.status = ConnectionlessDecodeStatus::kInvalidPrefix;
        result.reason = std::string(ReasonFor(result.status));
        return result;
    }

    if (size == kConnectionlessPrefix.size())
    {
        result.status = ConnectionlessDecodeStatus::kTruncated;
        result.reason = "missing_connectionless_command";
        return result;
    }

    const std::uint8_t* const body = bytes + kConnectionlessPrefix.size();
    const std::size_t body_size = size - kConnectionlessPrefix.size();
    std::size_t command_size = 0;
    while (command_size < body_size)
    {
        const auto character = static_cast<unsigned char>(body[command_size]);
        if (character <= 0x20u || character == 0x7Fu)
        {
            break;
        }
        ++command_size;
    }

    const std::string_view command(
        reinterpret_cast<const char*>(body),
        command_size);
    if (command == "getchallenge")
    {
        result.command = ConnectionlessCommand::kGetChallenge;
        std::string_view request_body(
            reinterpret_cast<const char*>(body),
            body_size);
        if (!request_body.empty() && request_body.back() == '\0')
        {
            request_body.remove_suffix(1);
        }
        if (!request_body.empty() && request_body.back() == '\n')
        {
            request_body.remove_suffix(1);
        }

        if (request_body == "getchallenge" || request_body == "getchallenge steam")
        {
            result.status = ConnectionlessDecodeStatus::kOk;
            result.reason = "ok";
            return result;
        }

        result.status = ConnectionlessDecodeStatus::kMalformed;
        result.reason = "malformed_getchallenge";
        return result;
    }

    if (command == "connect")
    {
        return DecodeConnect(body, body_size);
    }

    result.status = ConnectionlessDecodeStatus::kUnsupportedCommand;
    result.command = ConnectionlessCommand::kUnknown;
    result.reason = std::string(ReasonFor(result.status));
    return result;
}

ConnectionlessDecodeResult DecodeConnectionlessDatagram(
    const std::vector<std::uint8_t>& datagram)
{
    return DecodeConnectionlessDatagram(datagram.data(), datagram.size());
}

std::string_view ReasonFor(ConnectionlessDecodeStatus status) noexcept
{
    switch (status)
    {
    case ConnectionlessDecodeStatus::kOk:
        return "ok";
    case ConnectionlessDecodeStatus::kTruncated:
        return "truncated_datagram";
    case ConnectionlessDecodeStatus::kOversized:
        return "oversized_datagram";
    case ConnectionlessDecodeStatus::kInvalidPrefix:
        return "invalid_connectionless_prefix";
    case ConnectionlessDecodeStatus::kMalformed:
        return "malformed_datagram";
    case ConnectionlessDecodeStatus::kUnsupportedCommand:
        return "unsupported_command";
    case ConnectionlessDecodeStatus::kUnsupportedProtocol:
        return "unsupported_protocol";
    case ConnectionlessDecodeStatus::kUnsupportedAuthentication:
        return "unsupported_authentication";
    }
    return "unknown_decode_status";
}

std::vector<std::uint8_t> BuildChallengeResponse(std::int32_t challenge)
{
    std::array<char, 16> challenge_buffer{};
    const auto converted = std::to_chars(
        challenge_buffer.data(),
        challenge_buffer.data() + challenge_buffer.size(),
        challenge,
        10);

    std::vector<std::uint8_t> response;
    response.reserve(40);
    AppendConnectionlessPrefix(response);
    AppendAscii(response, "A00000000 ");
    AppendAscii(
        response,
        std::string_view(
            challenge_buffer.data(),
            static_cast<std::size_t>(converted.ptr - challenge_buffer.data())));
    AppendAscii(response, " 3 0 0\n");
    response.push_back(0u);
    return response;
}

std::vector<std::uint8_t> BuildRejectionResponse(std::string_view reason)
{
    const std::string sanitized_reason = SanitizeRejectionReason(reason);
    std::vector<std::uint8_t> response;
    response.reserve(kConnectionlessPrefix.size() + 1u + sanitized_reason.size() + 1u);
    AppendConnectionlessPrefix(response);
    response.push_back(static_cast<std::uint8_t>('9'));
    AppendAscii(response, sanitized_reason);
    response.push_back(0u);
    return response;
}

std::vector<std::uint8_t> BuildAcceptanceResponse(
    int slot,
    const Ipv4Endpoint& endpoint)
{
    const std::string payload = "B " + std::to_string(slot) + " \""
        + endpoint.ToString() + "\" 0 5971";
    std::vector<std::uint8_t> response;
    response.reserve(kConnectionlessPrefix.size() + payload.size() + 1u);
    AppendConnectionlessPrefix(response);
    AppendAscii(response, payload);
    response.push_back(0u);
    return response;
}

std::string_view ReasonFor(ChallengeValidationResult validation_result) noexcept
{
    switch (validation_result)
    {
    case ChallengeValidationResult::kValid:
        return "challenge_valid";
    case ChallengeValidationResult::kUnknown:
        return "challenge_unknown";
    case ChallengeValidationResult::kEndpointMismatch:
        return "challenge_endpoint_mismatch";
    case ChallengeValidationResult::kExpired:
        return "challenge_expired";
    case ChallengeValidationResult::kConsumed:
        return "challenge_consumed";
    }
    return "challenge_unknown_result";
}

ChallengeTable::ChallengeTable(
    std::size_t table_capacity,
    Duration time_to_live,
    Generator generator)
    : capacity_(table_capacity),
      time_to_live_(time_to_live),
      generator_(std::move(generator))
{
    if (capacity_ == 0u)
    {
        throw std::invalid_argument("challenge capacity must be positive");
    }
    if (time_to_live_ <= Duration::zero())
    {
        throw std::invalid_argument("challenge TTL must be positive");
    }
    if (!generator_)
    {
        throw std::invalid_argument("challenge generator is required");
    }
    entries_.reserve(capacity_);
}

ChallengeTable::IssueResult ChallengeTable::Issue(
    const Ipv4Endpoint& endpoint,
    TimePoint now)
{
    IssueResult result{};
    result.expired_count = Expire(now);

    const auto existing = std::find_if(
        entries_.begin(),
        entries_.end(),
        [&endpoint](const Entry& entry)
        {
            return entry.endpoint.SameHost(endpoint);
        });
    if (existing != entries_.end())
    {
        entries_.erase(existing);
        result.replaced_same_host = true;
    }

    if (entries_.size() == capacity_)
    {
        const auto oldest = std::min_element(
            entries_.begin(),
            entries_.end(),
            [](const Entry& left, const Entry& right)
            {
                return left.issued_at < right.issued_at
                    || (left.issued_at == right.issued_at && left.serial < right.serial);
            });
        entries_.erase(oldest);
        result.evicted_oldest = true;
    }

    result.challenge = generator_();
    Entry entry{};
    entry.endpoint = endpoint;
    entry.challenge = result.challenge;
    entry.issued_at = now;
    entry.expires_at = now + time_to_live_;
    entry.serial = next_serial_++;
    entries_.push_back(std::move(entry));
    return result;
}

ChallengeValidationResult ChallengeTable::Validate(
    const Ipv4Endpoint& endpoint,
    std::int32_t challenge,
    TimePoint now) const noexcept
{
    const Entry* matching_entry = nullptr;
    bool challenge_seen = false;
    for (const Entry& entry : entries_)
    {
        if (entry.challenge != challenge)
        {
            continue;
        }

        challenge_seen = true;
        if (entry.endpoint.SameHost(endpoint))
        {
            matching_entry = &entry;
            break;
        }
    }

    if (matching_entry == nullptr)
    {
        return challenge_seen
            ? ChallengeValidationResult::kEndpointMismatch
            : ChallengeValidationResult::kUnknown;
    }
    if (now >= matching_entry->expires_at)
    {
        return ChallengeValidationResult::kExpired;
    }
    if (matching_entry->consumed)
    {
        return ChallengeValidationResult::kConsumed;
    }
    return ChallengeValidationResult::kValid;
}

ChallengeValidationResult ChallengeTable::Consume(
    const Ipv4Endpoint& endpoint,
    std::int32_t challenge,
    TimePoint now) noexcept
{
    const ChallengeValidationResult validation = Validate(endpoint, challenge, now);
    if (validation != ChallengeValidationResult::kValid)
    {
        return validation;
    }

    const auto matching_entry = std::find_if(
        entries_.begin(),
        entries_.end(),
        [&endpoint, challenge](const Entry& entry)
        {
            return entry.challenge == challenge && entry.endpoint.SameHost(endpoint);
        });
    if (matching_entry == entries_.end())
    {
        return ChallengeValidationResult::kUnknown;
    }

    matching_entry->consumed = true;
    return ChallengeValidationResult::kValid;
}

std::size_t ChallengeTable::Expire(TimePoint now) noexcept
{
    const std::size_t old_size = entries_.size();
    entries_.erase(
        std::remove_if(
            entries_.begin(),
            entries_.end(),
            [now](const Entry& entry)
            {
                return now >= entry.expires_at;
            }),
        entries_.end());
    return old_size - entries_.size();
}

std::size_t ChallengeTable::size() const noexcept
{
    return entries_.size();
}

std::size_t ChallengeTable::capacity() const noexcept
{
    return capacity_;
}

std::string_view ReasonFor(HandshakeOutcome outcome) noexcept
{
    switch (outcome)
    {
    case HandshakeOutcome::kMalformedDatagram:
        return "malformed_datagram";
    case HandshakeOutcome::kUnsupportedCommand:
        return "unsupported_command";
    case HandshakeOutcome::kChallengeIssued:
        return "challenge_issued";
    case HandshakeOutcome::kConnectRejected:
        return "connect_rejected";
    case HandshakeOutcome::kConnectAccepted:
        return "connect_accepted";
    }
    return "unknown_handshake_outcome";
}

HandshakeProcessor::HandshakeProcessor(
    ChallengeTable& challenges,
    AdmissionPreflightCallback admission_preflight,
    AdmissionCommitCallback admission_commit)
    : challenges_(challenges),
      admission_preflight_(std::move(admission_preflight)),
      admission_commit_(std::move(admission_commit))
{
}

HandshakeResult HandshakeProcessor::Process(
    const Ipv4Endpoint& endpoint,
    const std::uint8_t* bytes,
    std::size_t size,
    ChallengeTable::TimePoint now)
{
    HandshakeResult result{};
    result.endpoint = endpoint;

    ConnectionlessDecodeResult decoded = DecodeConnectionlessDatagram(bytes, size);
    if (!decoded.ok())
    {
        result.reason = decoded.reason;
        if (decoded.command == ConnectionlessCommand::kConnect)
        {
            result.outcome = HandshakeOutcome::kConnectRejected;
            result.response = BuildRejectionResponse(result.reason);
        }
        else if (decoded.status == ConnectionlessDecodeStatus::kUnsupportedCommand)
        {
            result.outcome = HandshakeOutcome::kUnsupportedCommand;
        }
        else
        {
            result.outcome = HandshakeOutcome::kMalformedDatagram;
        }
        return result;
    }

    if (decoded.command == ConnectionlessCommand::kGetChallenge)
    {
        const ChallengeTable::IssueResult issue = challenges_.Issue(endpoint, now);
        result.outcome = HandshakeOutcome::kChallengeIssued;
        result.reason = std::string(ReasonFor(result.outcome));
        result.issued_challenge = issue.challenge;
        result.response = BuildChallengeResponse(issue.challenge);
        return result;
    }

    if (decoded.command != ConnectionlessCommand::kConnect || !decoded.connect.has_value())
    {
        result.outcome = HandshakeOutcome::kUnsupportedCommand;
        result.reason = std::string(ReasonFor(result.outcome));
        return result;
    }

    result.connect = std::move(decoded.connect);
    const ChallengeValidationResult validation = challenges_.Validate(
        endpoint,
        result.connect->challenge,
        now);
    if (validation != ChallengeValidationResult::kValid)
    {
        result.outcome = HandshakeOutcome::kConnectRejected;
        result.reason = std::string(ReasonFor(validation));
        result.response = BuildRejectionResponse(result.reason);
        return result;
    }

    if (!admission_preflight_)
    {
        result.outcome = HandshakeOutcome::kConnectRejected;
        result.reason = "admission_preflight_unavailable";
        result.response = BuildRejectionResponse(result.reason);
        return result;
    }

    const AdmissionRequest admission_request{endpoint, *result.connect};
    AdmissionResult admission{};
    try
    {
        admission = admission_preflight_(admission_request);
    }
    catch (...)
    {
        result.outcome = HandshakeOutcome::kConnectRejected;
        result.reason = "admission_preflight_exception";
        result.response = BuildRejectionResponse(result.reason);
        return result;
    }
    result.admission = admission;

    if (!admission.accepted)
    {
        result.outcome = HandshakeOutcome::kConnectRejected;
        result.reason = SanitizeRejectionReason(admission.rejection_reason);
        result.response = BuildRejectionResponse(result.reason);
        return result;
    }
    if (admission.slot < 0)
    {
        result.outcome = HandshakeOutcome::kConnectRejected;
        result.reason = "admission_invalid_slot";
        result.response = BuildRejectionResponse(result.reason);
        return result;
    }
    if (admission.session_state.empty())
    {
        result.outcome = HandshakeOutcome::kConnectRejected;
        result.reason = "admission_missing_state";
        result.response = BuildRejectionResponse(result.reason);
        return result;
    }
    if (admission.spawned || admission.active)
    {
        result.outcome = HandshakeOutcome::kConnectRejected;
        result.reason = "admission_active_state";
        result.response = BuildRejectionResponse(result.reason);
        return result;
    }

    if (!admission_commit_)
    {
        result.outcome = HandshakeOutcome::kConnectRejected;
        result.reason = "admission_commit_unavailable";
        result.response = BuildRejectionResponse(result.reason);
        return result;
    }

    bool committed = false;
    try
    {
        committed = admission_commit_(admission_request, admission);
    }
    catch (...)
    {
        result.outcome = HandshakeOutcome::kConnectRejected;
        result.reason = "admission_commit_exception";
        result.response = BuildRejectionResponse(result.reason);
        return result;
    }
    if (!committed)
    {
        result.outcome = HandshakeOutcome::kConnectRejected;
        result.reason = "admission_commit_failed";
        result.response = BuildRejectionResponse(result.reason);
        return result;
    }

    const ChallengeValidationResult consumed = challenges_.Consume(
        endpoint,
        result.connect->challenge,
        now);
    if (consumed != ChallengeValidationResult::kValid)
    {
        // A successful atomic commit is the mutation boundary. Revalidating at
        // the same time point can only fail if a commit callback violates its
        // contract by mutating the processor's challenge table. Do not turn
        // that internal invariant breach into a rejected request after the
        // authoritative session has already been created.
        throw std::logic_error("challenge changed during admission commit");
    }

    result.outcome = HandshakeOutcome::kConnectAccepted;
    result.reason = std::string(ReasonFor(result.outcome));
    result.response = BuildAcceptanceResponse(admission.slot, endpoint);
    return result;
}

HandshakeResult HandshakeProcessor::Process(
    const Ipv4Endpoint& endpoint,
    const std::vector<std::uint8_t>& datagram,
    ChallengeTable::TimePoint now)
{
    return Process(endpoint, datagram.data(), datagram.size(), now);
}
} // namespace hl::network
