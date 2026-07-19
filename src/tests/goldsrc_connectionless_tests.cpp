#include "network/goldsrc_connectionless.h"
#include "network/ipv4_endpoint.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <chrono>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace
{
using namespace hl::network;

std::vector<std::uint8_t> Connectionless(std::string_view body)
{
    std::vector<std::uint8_t> packet = {0xFFu, 0xFFu, 0xFFu, 0xFFu};
    packet.insert(packet.end(), body.begin(), body.end());
    return packet;
}

std::vector<std::uint8_t> Connectionless(const std::vector<std::uint8_t>& body)
{
    std::vector<std::uint8_t> packet = {0xFFu, 0xFFu, 0xFFu, 0xFFu};
    packet.insert(packet.end(), body.begin(), body.end());
    return packet;
}

std::vector<std::uint8_t> ConnectPacket(
    std::string_view protocol,
    std::string_view challenge,
    std::string_view protocol_info,
    std::string_view user_info,
    const std::vector<std::uint8_t>& auth_tail = {})
{
    std::string line = "connect ";
    line += protocol;
    line += ' ';
    line += challenge;
    line += " \"";
    line += protocol_info;
    line += "\" \"";
    line += user_info;
    line += "\"\n";

    std::vector<std::uint8_t> packet = Connectionless(line);
    packet.insert(packet.end(), auth_tail.begin(), auth_tail.end());
    return packet;
}

void TestIpv4Endpoint()
{
    const auto endpoint = Ipv4Endpoint::Parse("127.0.0.1", 27015u);
    assert(endpoint.has_value());
    assert(endpoint->address[0] == 127u);
    assert(endpoint->address[3] == 1u);
    assert(endpoint->port == 27015u);
    assert(endpoint->ToString() == "127.0.0.1:27015");

    const auto changed_port = Ipv4Endpoint::Parse("127.0.0.1", 30000u);
    assert(changed_port.has_value());
    assert(endpoint->SameHost(*changed_port));
    assert(*endpoint != *changed_port);

    assert(!Ipv4Endpoint::Parse("127.0.0", 1u).has_value());
    assert(!Ipv4Endpoint::Parse("127.0.0.256", 1u).has_value());
    assert(!Ipv4Endpoint::Parse(" 127.0.0.1", 1u).has_value());
    assert(!Ipv4Endpoint::Parse("127.0.0.1junk", 1u).has_value());
    assert(!Ipv4Endpoint::Parse("127..0.1", 1u).has_value());
}

void TestConnectionlessFramingAndChallengeCodec()
{
    const auto exact = Connectionless("getchallenge steam\n");
    const ConnectionlessDecodeResult decoded = DecodeConnectionlessDatagram(exact);
    assert(decoded.ok());
    assert(decoded.command == ConnectionlessCommand::kGetChallenge);

    std::vector<std::uint8_t> nul_terminated_body;
    const std::string canonical = "getchallenge steam\n";
    nul_terminated_body.insert(
        nul_terminated_body.end(),
        canonical.begin(),
        canonical.end());
    nul_terminated_body.push_back(0u);
    assert(DecodeConnectionlessDatagram(Connectionless(nul_terminated_body)).ok());
    assert(DecodeConnectionlessDatagram(Connectionless("getchallenge")).ok());
    assert(DecodeConnectionlessDatagram(Connectionless("getchallenge\n")).ok());
    assert(DecodeConnectionlessDatagram(Connectionless("getchallenge steam")).ok());

    std::vector<std::uint8_t> trailing = nul_terminated_body;
    trailing.push_back(static_cast<std::uint8_t>('x'));
    const auto trailing_result = DecodeConnectionlessDatagram(Connectionless(trailing));
    assert(trailing_result.status == ConnectionlessDecodeStatus::kMalformed);

    const std::vector<std::uint8_t> short_packet = {0xFFu, 0xFFu, 0xFFu};
    assert(
        DecodeConnectionlessDatagram(short_packet).status
        == ConnectionlessDecodeStatus::kTruncated);
    const std::vector<std::uint8_t> prefix_only = {0xFFu, 0xFFu, 0xFFu, 0xFFu};
    assert(
        DecodeConnectionlessDatagram(prefix_only).status
        == ConnectionlessDecodeStatus::kTruncated);
    const std::vector<std::uint8_t> wrong_prefix = {0xFEu, 0xFFu, 0xFFu, 0xFFu, 'x'};
    assert(
        DecodeConnectionlessDatagram(wrong_prefix).status
        == ConnectionlessDecodeStatus::kInvalidPrefix);
    assert(
        DecodeConnectionlessDatagram(Connectionless("status\n")).status
        == ConnectionlessDecodeStatus::kUnsupportedCommand);

    std::vector<std::uint8_t> oversized(kGoldSrcMaximumDatagramBytes + 1u, 0xFFu);
    assert(
        DecodeConnectionlessDatagram(oversized).status
        == ConnectionlessDecodeStatus::kOversized);

    // Exact fixture shape is from the in-tree Xash notes/client and observed
    // public ReHLDS protocol-48 response behavior; no source code is copied.
    const std::vector<std::uint8_t> challenge_response = BuildChallengeResponse(123456789);
    std::vector<std::uint8_t> expected = Connectionless("A00000000 123456789 3 0 0\n");
    expected.push_back(0u);
    assert(challenge_response == expected);
}

void TestConnectCodec()
{
    const std::string protocol_info =
        "\\prot\\3\\raw\\steam\\qport\\27015\\ext\\-1";
    const std::string user_info = "\\name\\Gordon Freeman\\model\\gordon";
    const std::vector<std::uint8_t> auth_tail = {0x00u, 0x01u, 0xFEu, 0xFFu};
    const std::vector<std::uint8_t> packet =
        ConnectPacket("48", "123456789", protocol_info, user_info, auth_tail);

    const ConnectionlessDecodeResult decoded = DecodeConnectionlessDatagram(packet);
    assert(decoded.ok());
    assert(decoded.command == ConnectionlessCommand::kConnect);
    assert(decoded.connect.has_value());
    const ConnectRequest& request = *decoded.connect;
    assert(request.protocol == 48);
    assert(request.challenge == 123456789);
    assert(request.authentication_protocol == 3);
    assert(request.authentication_raw == "steam");
    assert(request.qport.has_value() && *request.qport == 27015u);
    assert(request.extensions.has_value() && *request.extensions == -1);
    assert(request.name == "Gordon Freeman");
    assert(request.sanitized_name == "Gordon Freeman");
    assert(request.auth_tail == auth_tail);
    assert(request.parsed_user_info.Find("MODEL") != nullptr);

    const auto malformed_quote = Connectionless(
        "connect 48 1 \"\\prot\\3\\raw\\steam\" \"\\name\\Gordon\n");
    const auto malformed_quote_result = DecodeConnectionlessDatagram(malformed_quote);
    assert(malformed_quote_result.status == ConnectionlessDecodeStatus::kMalformed);
    assert(malformed_quote_result.reason == "connect_unterminated_quote");

    const auto quote_with_suffix = Connectionless(
        "connect 48 1 \"\\prot\\3\\raw\\steam\"oops \"\\name\\Gordon\"\n");
    assert(
        DecodeConnectionlessDatagram(quote_with_suffix).status
        == ConnectionlessDecodeStatus::kMalformed);

    const auto missing_newline = Connectionless(
        "connect 48 1 \"\\prot\\3\\raw\\steam\" \"\\name\\Gordon\"");
    assert(
        DecodeConnectionlessDatagram(missing_newline).status
        == ConnectionlessDecodeStatus::kMalformed);

    assert(
        DecodeConnectionlessDatagram(
            ConnectPacket("47", "1", protocol_info, user_info)).status
        == ConnectionlessDecodeStatus::kUnsupportedProtocol);
    assert(
        DecodeConnectionlessDatagram(
            ConnectPacket("2147483648", "1", protocol_info, user_info)).status
        == ConnectionlessDecodeStatus::kMalformed);
    assert(
        DecodeConnectionlessDatagram(
            ConnectPacket("48", "2147483648", protocol_info, user_info)).status
        == ConnectionlessDecodeStatus::kMalformed);
    assert(
        DecodeConnectionlessDatagram(
            ConnectPacket(
                "48",
                "1",
                "\\prot\\2\\raw\\steam",
                user_info)).status
        == ConnectionlessDecodeStatus::kUnsupportedAuthentication);
    assert(
        DecodeConnectionlessDatagram(
            ConnectPacket("48", "1", "\\raw\\steam", user_info)).reason
        == "missing_auth_protocol");
    assert(
        DecodeConnectionlessDatagram(
            ConnectPacket("48", "1", "\\prot\\3", user_info)).reason
        == "missing_auth_raw");
    assert(
        DecodeConnectionlessDatagram(
            ConnectPacket("48", "1", protocol_info, "\\model\\gordon")).reason
        == "userinfo_missing_name");
    assert(
        DecodeConnectionlessDatagram(
            ConnectPacket(
                "48",
                "1",
                "\\prot\\3\\raw\\steam\\qport\\65536",
                user_info)).reason
        == "invalid_qport");
    assert(
        DecodeConnectionlessDatagram(
            ConnectPacket(
                "48",
                "1",
                "\\prot\\3\\raw\\steam\\ext\\2147483648",
                user_info)).reason
        == "invalid_extensions");

    std::string high_byte_name = "\\name\\G";
    high_byte_name.push_back(static_cast<char>(0xE9u));
    const auto high_byte_result = DecodeConnectionlessDatagram(
        ConnectPacket("48", "1", protocol_info, high_byte_name));
    assert(high_byte_result.ok());
    assert(high_byte_result.connect->sanitized_name == "G?");
}

void TestInfoStringLimits()
{
    const InfoParseResult valid = ParseInfoString("\\name\\Gordon\\model\\gordon");
    assert(valid.ok());
    assert(valid.info.pairs.size() == 2u);

    assert(ParseInfoString(std::string("\\") + std::string(255u, 'a')).status
        == InfoParseStatus::kTooLong);
    assert(ParseInfoString("name\\Gordon").status
        == InfoParseStatus::kMissingLeadingBackslash);
    assert(ParseInfoString("\\name").status == InfoParseStatus::kMissingValue);
    assert(ParseInfoString("\\name\\").status == InfoParseStatus::kEmptyComponent);
    assert(ParseInfoString("\\name\\a\\Name\\b").status
        == InfoParseStatus::kDuplicateKey);
    assert(ParseInfoString(std::string("\\") + std::string(64u, 'k') + "\\v").status
        == InfoParseStatus::kKeyTooLong);
    assert(ParseInfoString(std::string("\\k\\") + std::string(128u, 'v')).status
        == InfoParseStatus::kValueTooLong);

    std::string controlled = "\\name\\Bad";
    controlled.push_back('\n');
    assert(ParseInfoString(controlled).status == InfoParseStatus::kControlCharacter);

    std::string too_many_pairs;
    for (int index = 0; index < 33; ++index)
    {
        too_many_pairs += "\\k" + std::to_string(index) + "\\v";
    }
    assert(too_many_pairs.size() <= kGoldSrcMaximumInfoBytes);
    assert(ParseInfoString(too_many_pairs).status == InfoParseStatus::kTooManyPairs);
}

void TestChallengeTable()
{
    using namespace std::chrono_literals;
    const Ipv4Endpoint endpoint_a{{127u, 0u, 0u, 1u}, 20000u};
    const Ipv4Endpoint endpoint_a_new_port{{127u, 0u, 0u, 1u}, 20001u};
    const Ipv4Endpoint endpoint_b{{127u, 0u, 0u, 2u}, 20000u};
    const ChallengeTable::TimePoint start{};

    std::int32_t generated = 100;
    ChallengeTable table(4u, 10s, [&generated]() { return generated++; });
    const auto issued = table.Issue(endpoint_a, start);
    assert(issued.challenge == 100);
    assert(table.Validate(endpoint_a_new_port, 100, start) == ChallengeValidationResult::kValid);
    assert(table.Validate(endpoint_b, 100, start) == ChallengeValidationResult::kEndpointMismatch);
    assert(table.Validate(endpoint_a, 999, start) == ChallengeValidationResult::kUnknown);
    assert(table.Consume(endpoint_a_new_port, 100, start) == ChallengeValidationResult::kValid);
    assert(table.Validate(endpoint_a, 100, start) == ChallengeValidationResult::kConsumed);

    const auto replacement = table.Issue(endpoint_a_new_port, start + 1s);
    assert(replacement.replaced_same_host);
    assert(table.size() == 1u);
    assert(table.Validate(endpoint_a, replacement.challenge, start + 11s)
        == ChallengeValidationResult::kExpired);
    assert(table.Expire(start + 11s) == 1u);
    assert(table.size() == 0u);

    std::int32_t eviction_value = 1;
    ChallengeTable eviction_table(
        2u,
        30s,
        [&eviction_value]() { return eviction_value++; });
    const Ipv4Endpoint endpoint_c{{127u, 0u, 0u, 3u}, 20000u};
    eviction_table.Issue(endpoint_a, start);
    eviction_table.Issue(endpoint_b, start);
    const auto third = eviction_table.Issue(endpoint_c, start);
    assert(third.evicted_oldest);
    assert(eviction_table.size() == 2u);
    assert(eviction_table.Validate(endpoint_a, 1, start) == ChallengeValidationResult::kUnknown);
    assert(eviction_table.Validate(endpoint_b, 2, start) == ChallengeValidationResult::kValid);
    assert(eviction_table.Validate(endpoint_c, 3, start) == ChallengeValidationResult::kValid);
}

void TestHandshakeAdmission()
{
    using namespace std::chrono_literals;
    const Ipv4Endpoint endpoint{{127u, 0u, 0u, 1u}, 40000u};
    const Ipv4Endpoint other_host{{127u, 0u, 0u, 2u}, 40000u};
    const ChallengeTable::TimePoint start{};
    std::int32_t generated = 424242;
    int admission_calls = 0;
    int commit_calls = 0;
    int created_sessions = 0;
    bool accept = false;
    bool return_active = false;

    ChallengeTable challenges(8u, 5s, [&generated]() { return generated++; });
    HandshakeProcessor processor(
        challenges,
        [&](const AdmissionRequest& request)
        {
            ++admission_calls;
            assert(request.endpoint == endpoint);
            assert(request.connect.protocol == 48);
            if (!accept)
            {
                AdmissionResult rejected{};
                rejected.rejection_reason = "server_full";
                return rejected;
            }

            AdmissionResult accepted{};
            accepted.accepted = true;
            accepted.slot = 0;
            accepted.session_identifier = "client-1";
            accepted.session_state = "AwaitingNetchan";
            accepted.active = return_active;
            return accepted;
        },
        [&](const AdmissionRequest& request, const AdmissionResult& admission)
        {
            ++commit_calls;
            assert(request.endpoint == endpoint);
            assert(admission.accepted);
            assert(admission.slot == 0);
            ++created_sessions;
            return true;
        });

    std::vector<std::uint8_t> challenge_request = Connectionless("getchallenge steam\n");
    challenge_request.push_back(0u);
    const HandshakeResult challenge_result =
        processor.Process(endpoint, challenge_request, start);
    assert(challenge_result.outcome == HandshakeOutcome::kChallengeIssued);
    assert(challenge_result.issued_challenge.has_value());
    assert(*challenge_result.issued_challenge == 424242);
    assert(challenge_result.response == BuildChallengeResponse(424242));

    const std::string protocol_info =
        "\\prot\\3\\raw\\steam\\qport\\777\\ext\\0";
    const std::string user_info = "\\name\\Gordon\\model\\gordon";
    const std::vector<std::uint8_t> valid_connect =
        ConnectPacket("48", "424242", protocol_info, user_info, {0x03u, 0x00u});

    const HandshakeResult failed_admission =
        processor.Process(endpoint, valid_connect, start + 1s);
    assert(failed_admission.outcome == HandshakeOutcome::kConnectRejected);
    assert(failed_admission.reason == "server_full");
    assert(created_sessions == 0);
    assert(challenges.Validate(endpoint, 424242, start + 1s)
        == ChallengeValidationResult::kValid);

    accept = true;
    const HandshakeResult accepted = processor.Process(endpoint, valid_connect, start + 2s);
    assert(accepted.outcome == HandshakeOutcome::kConnectAccepted);
    assert(accepted.response == BuildAcceptanceResponse(0, endpoint));
    std::vector<std::uint8_t> expected_acceptance =
        Connectionless("B 0 \"127.0.0.1:40000\" 0 5971");
    expected_acceptance.push_back(0u);
    assert(*accepted.response == expected_acceptance);
    assert(accepted.admission.has_value());
    assert(accepted.admission->session_state == "AwaitingNetchan");
    assert(!accepted.admission->spawned);
    assert(!accepted.admission->active);
    assert(created_sessions == 1);
    assert(challenges.Validate(endpoint, 424242, start + 2s)
        == ChallengeValidationResult::kConsumed);

    const int calls_before_replay = admission_calls;
    const HandshakeResult replay = processor.Process(endpoint, valid_connect, start + 3s);
    assert(replay.outcome == HandshakeOutcome::kConnectRejected);
    assert(replay.reason == "challenge_consumed");
    assert(admission_calls == calls_before_replay);
    assert(commit_calls == 1);
    assert(created_sessions == 1);

    const std::vector<std::uint8_t> unknown =
        ConnectPacket("48", "999999", protocol_info, user_info);
    assert(processor.Process(endpoint, unknown, start + 3s).reason == "challenge_unknown");

    const auto mismatch_issue = challenges.Issue(endpoint, start + 3s);
    const std::vector<std::uint8_t> mismatch = ConnectPacket(
        "48",
        std::to_string(mismatch_issue.challenge),
        protocol_info,
        user_info);
    assert(processor.Process(other_host, mismatch, start + 3s).reason
        == "challenge_endpoint_mismatch");
    assert(processor.Process(endpoint, mismatch, start + 8s).reason == "challenge_expired");

    const int calls_before_protocol_reject = admission_calls;
    assert(
        processor.Process(
            endpoint,
            ConnectPacket("47", "1", protocol_info, user_info),
            start + 3s).outcome
        == HandshakeOutcome::kConnectRejected);
    assert(admission_calls == calls_before_protocol_reject);

    const auto active_issue = challenges.Issue(endpoint, start + 9s);
    return_active = true;
    const std::vector<std::uint8_t> active_connect = ConnectPacket(
        "48",
        std::to_string(active_issue.challenge),
        protocol_info,
        user_info);
    const HandshakeResult active_result =
        processor.Process(endpoint, active_connect, start + 9s);
    assert(active_result.outcome == HandshakeOutcome::kConnectRejected);
    assert(active_result.reason == "admission_active_state");
    assert(commit_calls == 1);
    assert(created_sessions == 1);
    assert(challenges.Validate(endpoint, active_issue.challenge, start + 9s)
        == ChallengeValidationResult::kValid);

    const std::vector<std::uint8_t> exact_rejection = BuildRejectionResponse("server_full");
    std::vector<std::uint8_t> expected_rejection = Connectionless("9server_full");
    expected_rejection.push_back(0u);
    assert(exact_rejection == expected_rejection);
    const std::string long_reason(200u, 'x');
    assert(BuildRejectionResponse(long_reason).size() == 4u + 1u + 127u + 1u);
}

void TestTwoPhaseAdmissionFailurePaths()
{
    using namespace std::chrono_literals;

    enum class FailureMode
    {
        kCooperativeRejection,
        kInvalidSlot,
        kMissingState,
        kActivePlan,
        kSpawnedPlan,
        kPreflightException,
        kCommitFailure,
        kCommitException,
    };

    struct FailureCase final
    {
        FailureMode mode;
        std::string_view expected_reason;
        int expected_commit_calls;
    };

    const std::vector<FailureCase> cases = {
        {FailureMode::kCooperativeRejection, "server_full", 0},
        {FailureMode::kInvalidSlot, "admission_invalid_slot", 0},
        {FailureMode::kMissingState, "admission_missing_state", 0},
        {FailureMode::kActivePlan, "admission_active_state", 0},
        {FailureMode::kSpawnedPlan, "admission_active_state", 0},
        {FailureMode::kPreflightException, "admission_preflight_exception", 0},
        {FailureMode::kCommitFailure, "admission_commit_failed", 1},
        {FailureMode::kCommitException, "admission_commit_exception", 1},
    };

    const Ipv4Endpoint endpoint{{127u, 0u, 0u, 1u}, 41000u};
    const ChallengeTable::TimePoint start{};
    const std::string protocol_info = "\\prot\\3\\raw\\steam\\qport\\777";
    const std::string user_info = "\\name\\Two Phase";

    for (const FailureCase& failure_case : cases)
    {
        std::int32_t generated = 515151;
        int preflight_calls = 0;
        int commit_calls = 0;
        int created_sessions = 0;
        ChallengeTable challenges(
            4u,
            5s,
            [&generated]() { return generated++; });
        const ChallengeTable::IssueResult issue = challenges.Issue(endpoint, start);
        const std::vector<std::uint8_t> connect = ConnectPacket(
            "48",
            std::to_string(issue.challenge),
            protocol_info,
            user_info);

        HandshakeProcessor processor(
            challenges,
            [&](const AdmissionRequest& request) -> AdmissionResult
            {
                ++preflight_calls;
                assert(request.endpoint == endpoint);

                if (failure_case.mode == FailureMode::kPreflightException)
                {
                    throw std::runtime_error("preflight failed");
                }

                AdmissionResult admission{};
                if (failure_case.mode == FailureMode::kCooperativeRejection)
                {
                    admission.rejection_reason = "server_full";
                    return admission;
                }

                admission.accepted = true;
                admission.slot = 0;
                admission.session_identifier = "planned-client";
                admission.session_state = "AwaitingNetchan";
                if (failure_case.mode == FailureMode::kInvalidSlot)
                {
                    admission.slot = -1;
                }
                else if (failure_case.mode == FailureMode::kMissingState)
                {
                    admission.session_state.clear();
                }
                else if (failure_case.mode == FailureMode::kActivePlan)
                {
                    admission.active = true;
                }
                else if (failure_case.mode == FailureMode::kSpawnedPlan)
                {
                    admission.spawned = true;
                }
                return admission;
            },
            [&](const AdmissionRequest& request, const AdmissionResult& admission)
                -> bool
            {
                ++commit_calls;
                assert(request.endpoint == endpoint);
                assert(admission.accepted);
                if (failure_case.mode == FailureMode::kCommitException)
                {
                    throw std::runtime_error("commit failed");
                }
                if (failure_case.mode == FailureMode::kCommitFailure)
                {
                    return false;
                }

                ++created_sessions;
                return true;
            });

        const HandshakeResult failure = processor.Process(
            endpoint,
            connect,
            start + 1s);
        assert(failure.outcome == HandshakeOutcome::kConnectRejected);
        assert(failure.reason == failure_case.expected_reason);
        assert(preflight_calls == 1);
        assert(commit_calls == failure_case.expected_commit_calls);
        assert(created_sessions == 0);
        assert(challenges.Validate(endpoint, issue.challenge, start + 1s)
            == ChallengeValidationResult::kValid);
    }
}
} // namespace

int main()
{
    TestIpv4Endpoint();
    TestConnectionlessFramingAndChallengeCodec();
    TestConnectCodec();
    TestInfoStringLimits();
    TestChallengeTable();
    TestHandshakeAdmission();
    TestTwoPhaseAdmissionFailurePaths();
    return 0;
}
