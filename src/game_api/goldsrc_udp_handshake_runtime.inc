// Included by hl_server_module.cpp after the authoritative dedicated admission
// helpers are defined. The service is owned by HlServerModule::Impl and pumped
// once per host frame by HostApplication; no transport loop lives in the codec.
class GoldSrcUdpHandshakeRuntime final
{
public:
    static constexpr std::size_t kChallengeCapacity = 64;
    static constexpr auto kChallengeLifetime = std::chrono::seconds(30);
    static constexpr int kMaximumDatagramsPerHostFrame = 8;
    static constexpr int kMaximumDatagramsSentPerHostFrame = 8;

    GoldSrcUdpHandshakeRuntime(
        EngineShimState& state,
        const hl::game_api::HlServerModuleInitOptions& options,
        hl::game_api::GoldSrcUdpHandshakeSummary& summary)
        : state_(state),
          summary_(summary),
          bind_address_(options.bind_address),
          requested_port_(options.server_port),
          timeout_ms_(options.goldsrc_handshake_timeout_ms),
          client_dll_path_(options.client_dll_path),
          netchan_enabled_(options.goldsrc_netchan_enabled),
          negative_proof_(options.goldsrc_netchan_negative_proof),
          serverinfo_enabled_(options.goldsrc_serverinfo_enabled),
          serverinfo_negative_proof_(options.goldsrc_serverinfo_negative_proof),
          dedicated_runtime_(
              options.runtime_mode == hl::game_api::ServerRuntimeMode::kDedicated)
    {
        summary_ = {};
        summary_.enabled = true;
        summary_.bind_address = bind_address_;
        summary_.requested_port = requested_port_;
        summary_.max_datagrams_per_frame = kMaximumDatagramsPerHostFrame;
        summary_.max_datagrams_sent_per_frame =
            kMaximumDatagramsSentPerHostFrame;
        summary_.netchan_enabled = netchan_enabled_;
        summary_.netchan_negative_proof = negative_proof_;
        summary_.serverinfo_enabled = serverinfo_enabled_;
        summary_.serverinfo_negative_proof = serverinfo_negative_proof_;
    }

    ~GoldSrcUdpHandshakeRuntime()
    {
        Shutdown();
    }

    GoldSrcUdpHandshakeRuntime(const GoldSrcUdpHandshakeRuntime&) = delete;
    GoldSrcUdpHandshakeRuntime& operator=(const GoldSrcUdpHandshakeRuntime&) = delete;

    bool Start()
    {
        if (started_)
        {
            return !transport_failed_;
        }
        if (!dedicated_runtime_)
        {
            return FailBind("dedicated-runtime-required");
        }
        if (negative_proof_ && !netchan_enabled_)
        {
            return FailBind("netchan-negative-proof-requires-netchan");
        }
        if (serverinfo_enabled_ && !netchan_enabled_)
        {
            return FailBind("serverinfo-requires-netchan");
        }
        if (serverinfo_negative_proof_ && !serverinfo_enabled_)
        {
            return FailBind("serverinfo-negative-proof-requires-serverinfo");
        }
        if (serverinfo_enabled_ && client_dll_path_.empty())
        {
            return FailBind("serverinfo-client-dll-path-missing");
        }
        if (requested_port_ < 1 || requested_port_ > 65535)
        {
            return FailBind("invalid-port");
        }

        const std::optional<hl::network::Ipv4Endpoint> requested_endpoint =
            hl::network::Ipv4Endpoint::Parse(
                bind_address_,
                static_cast<std::uint16_t>(requested_port_));
        if (!requested_endpoint.has_value())
        {
            return FailBind("invalid-ipv4-address");
        }
        if (!socket_.Open(*requested_endpoint))
        {
            summary_.last_reject_reason = "udp-bind-failed";
            complete_ = true;
            transport_failed_ = true;
            hl::common::Logger::Error(
                hl::common::LogCategory::Server,
                "goldsrc_udp_bind_failed: address=" + bind_address_
                    + ",port=" + std::to_string(requested_port_)
                    + ",platform_error=" + socket_.LastError());
            return false;
        }

        challenges_ = std::make_unique<hl::network::ChallengeTable>(
            kChallengeCapacity,
            kChallengeLifetime,
            []() -> std::int32_t
            {
                std::uint32_t random_value = 0;
                std::string random_error;
                if (!hl::network::SecureRandomUint32(&random_value, &random_error))
                {
                    throw std::runtime_error(
                        "secure challenge generation failed: " + random_error);
                }
                const std::uint32_t positive_value = random_value & 0x7FFFFFFFu;
                return static_cast<std::int32_t>(
                    positive_value == 0u ? 1u : positive_value);
            });
        processor_ = std::make_unique<hl::network::HandshakeProcessor>(
            *challenges_,
            [this](const hl::network::AdmissionRequest& request)
            {
                return PrepareAdmission(request);
            },
            [this](
                const hl::network::AdmissionRequest& request,
                const hl::network::AdmissionResult& plan)
            {
                return CommitAdmission(request, plan);
            });

        summary_.ready = true;
        summary_.bound_port = static_cast<int>(socket_.LocalEndpoint().port);
        started_ = true;
        hl::common::Logger::Info(
            hl::common::LogCategory::Server,
            "goldsrc_udp_ready: address=" + bind_address_
                + ",port=" + std::to_string(summary_.bound_port)
                + ",max_datagram_bytes="
                + std::to_string(hl::network::kGoldSrcMaximumDatagramBytes)
                + ",max_datagrams_per_frame="
                + std::to_string(kMaximumDatagramsPerHostFrame)
                + ",max_datagrams_sent_per_frame="
                + std::to_string(kMaximumDatagramsSentPerHostFrame)
                + ",netchan=" + (netchan_enabled_ ? "1" : "0"));
        return true;
    }

    bool PumpHostFrame()
    {
        if (!started_ || complete_)
        {
            return !transport_failed_;
        }

        const auto frame_time = std::chrono::steady_clock::now();
        if (!deadline_started_)
        {
            deadline_ = frame_time + std::chrono::milliseconds(timeout_ms_);
            deadline_started_ = true;
        }
        if (frame_time >= deadline_)
        {
            MarkTimeout();
            return true;
        }

        ++summary_.host_frames_pumped;
        datagrams_sent_this_frame_ = 0;
        if (negative_advance_pending_)
        {
            negative_advance_pending_ = false;
            if (!SendPendingOrdinaryAdvance())
            {
                return false;
            }
        }
        for (int packet_index = 0;
             packet_index < kMaximumDatagramsPerHostFrame;
             ++packet_index)
        {
            std::size_t received_size = 0;
            hl::network::Ipv4Endpoint sender;
            const hl::network::UdpReceiveStatus receive_status = socket_.Receive(
                buffer_.data(),
                buffer_.size(),
                received_size,
                sender);
            if (receive_status == hl::network::UdpReceiveStatus::WouldBlock)
            {
                break;
            }
            if (receive_status == hl::network::UdpReceiveStatus::Error)
            {
                return FailTransport(
                    "udp-receive-failed",
                    "goldsrc_udp_receive_failed: platform_error=" + socket_.LastError());
            }
            if (receive_status == hl::network::UdpReceiveStatus::Oversized)
            {
                ++summary_.datagrams_received;
                ++summary_.malformed_packets;
                summary_.last_reject_reason = "oversized-datagram";
                ++malformed_diagnostics_seen_;
                if (ShouldLogDiagnosticSample(malformed_diagnostics_seen_))
                {
                    hl::common::Logger::Warn(
                        hl::common::LogCategory::Server,
                        "goldsrc_udp_malformed: endpoint=" + sender.ToString()
                            + ",reason=oversized-datagram");
                }
                continue;
            }

            ++summary_.datagrams_received;
            if (!ProcessDatagram(sender, received_size))
            {
                return false;
            }
            if (complete_)
            {
                break;
            }
        }

        if (!complete_ && std::chrono::steady_clock::now() >= deadline_)
        {
            MarkTimeout();
        }
        return !transport_failed_;
    }

    bool Pending() const noexcept
    {
        return started_ && !complete_;
    }

    bool Succeeded() const noexcept
    {
        const bool handshake_succeeded = complete_
            && accepted_
            && !transport_failed_
            && summary_.accepted == 1
            && summary_.session_count >= 1
            && summary_.allocated_slot > 0
            && !summary_.session_id.empty()
            && summary_.final_state == "connected"
            && summary_.connected
            && !summary_.put_in_server
            && !summary_.spawned
            && !summary_.active;
        if (!handshake_succeeded || !netchan_enabled_)
        {
            return handshake_succeeded;
        }

        const bool netchan_succeeded = summary_.netchan_initialized
            && summary_.netchan_state == "established"
            && summary_.reliable_acked == (serverinfo_enabled_ ? 2 : 1)
            && summary_.reliable_pending_bytes == 0
            && summary_.session_count == 1
            && summary_.duplicate_sessions == 0
            && summary_.partial_sessions == 0;
        if (!netchan_succeeded)
        {
            return false;
        }
        if (serverinfo_enabled_)
        {
            const bool serverinfo_succeeded = summary_.client_new_delivered == 1
                && summary_.duplicate_new_deliveries == 0
                && summary_.serverinfo_context_built
                && summary_.serverinfo_generations == 1
                && summary_.serverinfo_queued == 1
                && summary_.serverinfo_sent == 1
                && summary_.serverinfo_acked == 1
                && summary_.signon_phase == "serverinfo_acknowledged";
            if (!serverinfo_succeeded || !serverinfo_negative_proof_)
            {
                return serverinfo_succeeded;
            }
            return summary_.serverinfo_resent >= 1
                && summary_.wrong_reliable_ack_rejected
                && summary_.future_ack_rejected >= 1
                && summary_.duplicate_client_reliable_suppressed
                && summary_.duplicate_rejected >= 1
                && summary_.malformed_stringcmd_rejected >= 1
                && summary_.command_injection_rejected >= 1
                && summary_.unsupported_command_rejected >= 1
                && summary_.unsupported_opcode_rejected >= 1
                && summary_.endpoint_hijack_rejected >= 1
                && summary_.server_still_responsive;
        }
        if (!negative_proof_)
        {
            return true;
        }
        return summary_.reliable_resent >= 1
            && summary_.reliable_ack_mismatch >= 1
            && summary_.duplicate_rejected >= 1
            && summary_.out_of_order_rejected >= 1
            && summary_.malformed_rejected >= 1
            && summary_.fragment_rejected >= 1
            && summary_.future_ack_rejected >= 1
            && summary_.endpoint_hijack_rejected >= 1
            && summary_.server_still_responsive;
    }

    void Shutdown() noexcept
    {
        if (shutdown_)
        {
            return;
        }
        shutdown_ = true;

        const bool was_open = socket_.IsOpen();
        socket_.Close();
        if (!started_ && !was_open)
        {
            return;
        }

        summary_.clean_shutdown = true;
        if (netchan_enabled_)
        {
            RefreshAuthoritativeNetchanSessionSummary();
        }
        hl::common::Logger::Info(
            hl::common::LogCategory::Server,
            "goldsrc_udp_shutdown: address=" + bind_address_
                + ",port=" + std::to_string(summary_.bound_port)
                + ",clean=1");
        hl::common::Logger::Info(
            hl::common::LogCategory::Summary,
            "goldsrc_udp_summary: enabled=1,ready="
                + std::string(summary_.ready ? "1" : "0")
                + ",clean_shutdown=1,frames="
                + std::to_string(summary_.host_frames_pumped)
                + ",max_datagrams_per_frame="
                + std::to_string(summary_.max_datagrams_per_frame)
                + ",datagrams=" + std::to_string(summary_.datagrams_received)
                + ",challenges=" + std::to_string(summary_.challenges_issued)
                + ",connects=" + std::to_string(summary_.connect_requests)
                + ",accepted=" + std::to_string(summary_.accepted)
                + ",rejected=" + std::to_string(summary_.rejected)
                + ",last_reject_reason=" + summary_.last_reject_reason
                + ",session_count=" + std::to_string(summary_.session_count)
                + ",state=" + summary_.final_state);
        if (netchan_enabled_)
        {
            hl::common::Logger::Info(
                hl::common::LogCategory::Summary,
                "goldsrc_netchan_summary: enabled=1,initialized="
                    + std::string(summary_.netchan_initialized ? "1" : "0")
                    + ",netchan_state=" + summary_.netchan_state
                    + ",server_initial_sequence="
                    + std::to_string(summary_.server_initial_sequence)
                    + ",server_initial_ack="
                    + std::to_string(summary_.server_initial_acknowledgement)
                    + ",reliable_present="
                    + std::string(summary_.reliable_present ? "1" : "0")
                    + ",fragment_present="
                    + std::string(summary_.fragment_present ? "1" : "0")
                    + ",payload_transform="
                    + (summary_.payload_transform_passed ? "pass" : "fail")
                    + ",first_payload=" + summary_.first_payload
                    + ",reliable_queued="
                    + std::to_string(summary_.reliable_queued)
                    + ",reliable_sent=" + std::to_string(summary_.reliable_sent)
                    + ",reliable_resent=" + std::to_string(summary_.reliable_resent)
                    + ",reliable_acked=" + std::to_string(summary_.reliable_acked)
                    + ",reliable_ack_mismatch="
                    + std::to_string(summary_.reliable_ack_mismatch)
                    + ",reliable_pending_bytes="
                    + std::to_string(summary_.reliable_pending_bytes)
                    + ",sequenced_received="
                    + std::to_string(summary_.sequenced_datagrams_received)
                    + ",sequenced_sent="
                    + std::to_string(summary_.sequenced_datagrams_sent)
                    + ",duplicate_rejected="
                    + std::to_string(summary_.duplicate_rejected)
                    + ",out_of_order_rejected="
                    + std::to_string(summary_.out_of_order_rejected)
                    + ",malformed_rejected="
                    + std::to_string(summary_.malformed_rejected)
                    + ",fragment_rejected="
                    + std::to_string(summary_.fragment_rejected)
                    + ",future_ack_rejected="
                    + std::to_string(summary_.future_ack_rejected)
                    + ",endpoint_hijack_rejected="
                    + std::to_string(summary_.endpoint_hijack_rejected)
                    + ",duplicate_sessions="
                    + std::to_string(summary_.duplicate_sessions)
                    + ",partial_sessions="
                    + std::to_string(summary_.partial_sessions)
                    + ",server_still_responsive="
                    + std::string(summary_.server_still_responsive ? "1" : "0")
                    + ",session_count=" + std::to_string(summary_.session_count)
                    + ",state=" + summary_.final_state
                    + ",put_in_server="
                    + std::string(summary_.put_in_server ? "1" : "0")
                    + ",spawned=" + std::string(summary_.spawned ? "1" : "0")
                    + ",active=" + std::string(summary_.active ? "1" : "0")
                    + ",clean_shutdown=1");
        }
        if (serverinfo_enabled_)
        {
            hl::common::Logger::Info(
                hl::common::LogCategory::Summary,
                "goldsrc_serverinfo_summary: enabled=1,negative_proof="
                    + std::string(serverinfo_negative_proof_ ? "1" : "0")
                    + ",client_new_received="
                    + std::to_string(summary_.client_new_received)
                    + ",client_new_delivered="
                    + std::to_string(summary_.client_new_delivered)
                    + ",duplicate_new_deliveries="
                    + std::to_string(summary_.duplicate_new_deliveries)
                    + ",malformed_stringcmd_rejected="
                    + std::to_string(summary_.malformed_stringcmd_rejected)
                    + ",command_injection_rejected="
                    + std::to_string(summary_.command_injection_rejected)
                    + ",unsupported_command_rejected="
                    + std::to_string(summary_.unsupported_command_rejected)
                    + ",unsupported_opcode_rejected="
                    + std::to_string(summary_.unsupported_opcode_rejected)
                    + ",serverinfo_context_built="
                    + std::string(summary_.serverinfo_context_built ? "1" : "0")
                    + ",serverinfo_generations="
                    + std::to_string(summary_.serverinfo_generations)
                    + ",serverinfo_queued="
                    + std::to_string(summary_.serverinfo_queued)
                    + ",serverinfo_sent="
                    + std::to_string(summary_.serverinfo_sent)
                    + ",serverinfo_resent="
                    + std::to_string(summary_.serverinfo_resent)
                    + ",serverinfo_acked="
                    + std::to_string(summary_.serverinfo_acked)
                    + ",serverinfo_first_carrier_sequence="
                    + std::to_string(
                        summary_.serverinfo_first_carrier_sequence)
                    + ",serverinfo_latest_carrier_sequence="
                    + std::to_string(
                        summary_.serverinfo_latest_carrier_sequence)
                    + ",serverinfo_send_count="
                    + std::to_string(summary_.serverinfo_send_count)
                    + ",serverinfo_protocol="
                    + std::to_string(summary_.serverinfo_protocol)
                    + ",serverinfo_spawn_count="
                    + std::to_string(summary_.serverinfo_spawn_count)
                    + ",serverinfo_checksum=" + summary_.serverinfo_checksum
                    + ",serverinfo_client_dll_md5="
                    + summary_.serverinfo_client_dll_md5
                    + ",serverinfo_maxclients="
                    + std::to_string(summary_.serverinfo_maxclients)
                    + ",serverinfo_player_index="
                    + std::to_string(summary_.serverinfo_player_index)
                    + ",serverinfo_deathmatch="
                    + std::string(summary_.serverinfo_deathmatch ? "1" : "0")
                    + ",serverinfo_game_dir=" + summary_.serverinfo_game_dir
                    + ",serverinfo_hostname=" + summary_.serverinfo_hostname
                    + ",serverinfo_map=" + summary_.serverinfo_map
                    + ",serverinfo_mapcycle=" + summary_.serverinfo_mapcycle
                    + ",serverinfo_secure="
                    + std::string(summary_.serverinfo_secure ? "1" : "0")
                    + ",serverinfo_fallback_dir="
                    + summary_.serverinfo_fallback_dir
                    + ",serverinfo_allow_cheats="
                    + std::string(summary_.serverinfo_allow_cheats ? "1" : "0")
                    + ",serverinfo_payload_bytes="
                    + std::to_string(summary_.serverinfo_payload_bytes)
                    + ",wrong_reliable_ack_rejected="
                    + std::string(
                        summary_.wrong_reliable_ack_rejected ? "1" : "0")
                    + ",future_ack_rejected="
                    + std::to_string(summary_.future_ack_rejected)
                    + ",duplicate_client_reliable_suppressed="
                    + std::string(
                        summary_.duplicate_client_reliable_suppressed
                            ? "1"
                            : "0")
                    + ",signon_phase=" + summary_.signon_phase
                    + ",session_count=" + std::to_string(summary_.session_count)
                    + ",state=" + summary_.final_state
                    + ",netchan_state=" + summary_.netchan_state
                    + ",reliable_pending_bytes="
                    + std::to_string(summary_.reliable_pending_bytes)
                    + ",put_in_server="
                    + std::string(summary_.put_in_server ? "1" : "0")
                    + ",spawned=" + std::string(summary_.spawned ? "1" : "0")
                    + ",active=" + std::string(summary_.active ? "1" : "0")
                    + ",server_still_responsive="
                    + std::string(summary_.server_still_responsive ? "1" : "0")
                    + ",clean_shutdown=1");
        }
    }

private:
    static bool ShouldLogDiagnosticSample(std::uint64_t count) noexcept
    {
        // Preserve the first few diagnostics, then sample at powers of two.
        // Summary counters remain exact without giving unauthenticated traffic
        // a linear disk/console I/O amplifier.
        return count <= 4 || (count != 0 && (count & (count - 1)) == 0);
    }

    bool FailBind(std::string reason)
    {
        summary_.last_reject_reason = std::move(reason);
        complete_ = true;
        transport_failed_ = true;
        hl::common::Logger::Error(
            hl::common::LogCategory::Server,
            "goldsrc_udp_bind_failed: address=" + bind_address_
                + ",port=" + std::to_string(requested_port_)
                + ",platform_error=" + summary_.last_reject_reason);
        return false;
    }

    bool FailTransport(std::string reason, std::string message)
    {
        summary_.last_reject_reason = std::move(reason);
        transport_failed_ = true;
        complete_ = true;
        hl::common::Logger::Error(
            hl::common::LogCategory::Server,
            std::move(message));
        return false;
    }

    void MarkTimeout()
    {
        summary_.timed_out = true;
        summary_.last_reject_reason = accepted_ && serverinfo_enabled_
            ? "serverinfo-timeout"
            : accepted_ && netchan_enabled_
            ? "netchan-timeout"
            : "handshake-timeout";
        complete_ = true;
        hl::common::Logger::Warn(
            hl::common::LogCategory::Server,
            "goldsrc_udp_timeout: address=" + bind_address_
                + ",port=" + std::to_string(summary_.bound_port)
                + ",timeout_ms=" + std::to_string(timeout_ms_)
                + ",frames=" + std::to_string(summary_.host_frames_pumped));
    }

    hl::network::AdmissionResult PrepareAdmission(
        const hl::network::AdmissionRequest& request) const
    {
        hl::network::AdmissionResult result;
        const int channel_identifier = request.connect.qport.has_value()
            ? static_cast<int>(*request.connect.qport)
            : static_cast<int>(request.endpoint.port);
        const std::string endpoint_text = request.endpoint.ToString();
        const std::size_t endpoint_separator = endpoint_text.rfind(':');
        const std::string_view remote_host(
            endpoint_text.data(),
            endpoint_separator == std::string::npos
                ? endpoint_text.size()
                : endpoint_separator);

        for (const DedicatedPlayerRuntimeSlot& candidate :
             state_.dedicated_multiplayer_foundation.slots)
        {
            if (!IsDedicatedSlotConnectedOccupant(candidate)
                || candidate.remote_endpoint.empty()
                || candidate.channel_identifier != channel_identifier)
            {
                continue;
            }

            const std::size_t candidate_separator = candidate.remote_endpoint.rfind(':');
            const std::string_view candidate_host(
                candidate.remote_endpoint.data(),
                candidate_separator == std::string::npos
                    ? candidate.remote_endpoint.size()
                    : candidate_separator);
            if (candidate_host == remote_host)
            {
                result.rejection_reason = "duplicate-client";
                return result;
            }
        }

        const std::string session_id =
            "goldsrc-udp-" + std::to_string(next_network_session_);
        int planned_slot = 0;
        std::string rejection_reason;
        if (!PlanDedicatedLoopbackPreauthPlayer(
                state_, session_id, &planned_slot, &rejection_reason))
        {
            result.rejection_reason = rejection_reason.empty()
                ? "admission-rejected"
                : rejection_reason;
            return result;
        }

        result.accepted = true;
        result.slot = planned_slot;
        result.session_identifier = session_id;
        result.session_state = "connected";
        result.spawned = false;
        result.active = false;
        result.rejection_reason.clear();
        return result;
    }

    struct PreparedAdmissionMetadata final
    {
        std::string remote_endpoint;
        std::string protocol_info;
        std::string user_info;
        int network_protocol = 0;
        int qport = 0;
        int channel_identifier = 0;
        int challenge = 0;
        int auth_protocol = 0;
        int protocol_extensions = 0;
        bool netchan_prepared = false;
        hl::network::GoldSrcNetchanState netchan;
    };

    bool CommitAdmission(
        const hl::network::AdmissionRequest& request,
        const hl::network::AdmissionResult& plan)
    {
        // Copy every attacker-controlled string while admission is still only a
        // plan. Allocation failure is therefore a rejection with no mutation.
        PreparedAdmissionMetadata metadata;
        metadata.remote_endpoint = request.endpoint.ToString();
        metadata.protocol_info = request.connect.protocol_info;
        metadata.user_info = request.connect.user_info;
        metadata.network_protocol = request.connect.protocol;
        metadata.qport = request.connect.qport.has_value()
            ? static_cast<int>(*request.connect.qport)
            : 0;
        metadata.channel_identifier = request.connect.qport.has_value()
            ? static_cast<int>(*request.connect.qport)
            : static_cast<int>(request.endpoint.port);
        metadata.challenge = request.connect.challenge;
        metadata.auth_protocol = request.connect.authentication_protocol;
        metadata.protocol_extensions = request.connect.extensions.value_or(0);
        if (netchan_enabled_)
        {
            const std::optional<std::uint16_t> connect_qport =
                request.connect.qport;
            if (!metadata.netchan.Initialize(
                    request.endpoint,
                    connect_qport,
                    static_cast<std::uint16_t>(metadata.channel_identifier),
                    std::chrono::steady_clock::now()))
            {
                return false;
            }
            if (serverinfo_enabled_)
            {
                metadata.netchan.SetIncomingPayloadPolicy(
                    hl::network::GoldSrcNetchanIncomingPayloadPolicy::
                        kAcceptBoundedApplicationPayload);
            }
            constexpr std::array<std::uint8_t, 1> kFirstReliablePayload = {
                hl::network::kGoldSrcServerNop,
            };
            if (metadata.netchan.QueueReliablePayload(
                    kFirstReliablePayload.data(),
                    kFirstReliablePayload.size(),
                    hl::network::GoldSrcNetchanReliablePayloadKind::
                        kTransportBootstrap)
                != hl::network::GoldSrcNetchanQueueResult::kQueued)
            {
                return false;
            }
            metadata.netchan_prepared = true;
        }
        return CommitPreparedAdmission(request, plan, std::move(metadata));
    }

    bool CommitPreparedAdmission(
        const hl::network::AdmissionRequest& request,
        const hl::network::AdmissionResult& plan,
        PreparedAdmissionMetadata metadata) noexcept
    {
        int admitted_slot = 0;
        DedicatedPlayerRuntimeSlot* admitted_slot_state = nullptr;
        std::string rejection_reason;
        if (!AdmitDedicatedLoopbackPreauthPlayer(
                state_,
                plan.session_identifier,
                request.connect.sanitized_name,
                true,
                &admitted_slot,
                &rejection_reason,
                &admitted_slot_state))
        {
            return false;
        }
        if (admitted_slot != plan.slot || admitted_slot_state == nullptr)
        {
            // The pure plan and immediate single-threaded commit share the same
            // authoritative state. A mismatch is an internal invariant breach,
            // not a protocol rejection after mutation.
            std::terminate();
        }

        DedicatedPlayerRuntimeSlot& slot_state = *admitted_slot_state;
        slot_state.remote_endpoint = std::move(metadata.remote_endpoint);
        slot_state.network_protocol = metadata.network_protocol;
        slot_state.qport = metadata.qport;
        slot_state.channel_identifier = metadata.channel_identifier;
        slot_state.challenge = metadata.challenge;
        slot_state.auth_protocol = metadata.auth_protocol;
        slot_state.protocol_extensions = metadata.protocol_extensions;
        slot_state.protocol_info = std::move(metadata.protocol_info);
        slot_state.user_info = std::move(metadata.user_info);
        slot_state.steam_authentication_performed = false;
        if (metadata.netchan_prepared)
        {
            slot_state.netchan = std::move(metadata.netchan);
        }
        ++next_network_session_;
        return true;
    }

    bool ProcessDatagram(
        const hl::network::Ipv4Endpoint& sender,
        std::size_t received_size)
    {
        if (!netchan_enabled_)
        {
            return ProcessConnectionlessDatagram(sender, received_size);
        }

        const bool connectionless = received_size >= 4u
            && buffer_[0] == 0xFFu
            && buffer_[1] == 0xFFu
            && buffer_[2] == 0xFFu
            && buffer_[3] == 0xFFu;
        return connectionless
            ? ProcessConnectionlessDatagram(sender, received_size)
            : ProcessSequencedDatagram(sender, received_size);
    }

    bool ProcessConnectionlessDatagram(
        const hl::network::Ipv4Endpoint& sender,
        std::size_t received_size)
    {
        hl::network::HandshakeResult handshake;
        try
        {
            handshake = processor_->Process(
                sender,
                buffer_.data(),
                received_size,
                std::chrono::steady_clock::now());
        }
        catch (const std::exception& exception)
        {
            return FailTransport(
                "handshake-processing-failed",
                "goldsrc_udp_processing_failed: endpoint=" + sender.ToString()
                    + ",reason=" + exception.what());
        }

        const bool connect_accepted =
            handshake.outcome == hl::network::HandshakeOutcome::kConnectAccepted;
        switch (handshake.outcome)
        {
        case hl::network::HandshakeOutcome::kChallengeIssued:
            ++summary_.challenge_requests;
            ++summary_.challenges_issued;
            ++challenge_diagnostics_seen_;
            if (ShouldLogDiagnosticSample(challenge_diagnostics_seen_))
            {
                hl::common::Logger::Info(
                    hl::common::LogCategory::Server,
                    "goldsrc_udp_challenge_request: endpoint=" + sender.ToString());
                hl::common::Logger::Info(
                    hl::common::LogCategory::Server,
                    "goldsrc_udp_challenge_issued: endpoint=" + sender.ToString()
                        + ",challenge="
                        + std::to_string(handshake.issued_challenge.value_or(0))
                        + ",ttl_ms="
                        + std::to_string(
                            std::chrono::duration_cast<std::chrono::milliseconds>(
                                kChallengeLifetime).count()));
            }
            break;
        case hl::network::HandshakeOutcome::kConnectRejected:
            ++summary_.connect_requests;
            ++summary_.rejected;
            summary_.last_reject_reason = handshake.reason;
            ++rejection_diagnostics_seen_;
            if (handshake.reason == "challenge_expired")
            {
                ++expired_diagnostics_seen_;
                if (ShouldLogDiagnosticSample(expired_diagnostics_seen_))
                {
                    hl::common::Logger::Info(
                        hl::common::LogCategory::Server,
                        "goldsrc_udp_challenge_expired: endpoint="
                            + sender.ToString());
                }
            }
            if (ShouldLogDiagnosticSample(rejection_diagnostics_seen_))
            {
                hl::common::Logger::Warn(
                    hl::common::LogCategory::Server,
                    "goldsrc_udp_connect_rejected: endpoint=" + sender.ToString()
                        + ",reason=" + handshake.reason);
            }
            break;
        case hl::network::HandshakeOutcome::kConnectAccepted:
            ++summary_.connect_requests;
            break;
        case hl::network::HandshakeOutcome::kMalformedDatagram:
            ++summary_.malformed_packets;
            summary_.last_reject_reason = handshake.reason;
            ++malformed_diagnostics_seen_;
            if (ShouldLogDiagnosticSample(malformed_diagnostics_seen_))
            {
                hl::common::Logger::Warn(
                    hl::common::LogCategory::Server,
                    "goldsrc_udp_malformed: endpoint=" + sender.ToString()
                        + ",reason=" + handshake.reason);
            }
            break;
        case hl::network::HandshakeOutcome::kUnsupportedCommand:
            ++summary_.malformed_packets;
            summary_.last_reject_reason = handshake.reason;
            ++malformed_diagnostics_seen_;
            if (ShouldLogDiagnosticSample(malformed_diagnostics_seen_))
            {
                hl::common::Logger::Warn(
                    hl::common::LogCategory::Server,
                    "goldsrc_udp_unsupported: endpoint=" + sender.ToString()
                        + ",reason=" + handshake.reason);
            }
            break;
        }

        if (handshake.response.has_value()
            && !SendBoundedDatagram(
                handshake.response->data(),
                handshake.response->size(),
                sender))
        {
            return FailTransport(
                "udp-send-failed",
                "goldsrc_udp_send_failed: endpoint=" + sender.ToString()
                    + ",platform_error=" + socket_.LastError());
        }

        if (connect_accepted)
        {
            ++summary_.accepted;
            accepted_ = true;
            if (!CaptureAcceptedSession(handshake))
            {
                return false;
            }
            if (netchan_enabled_)
            {
                if (!SendInitialNetchanDatagram())
                {
                    return false;
                }
                negative_advance_pending_ = negative_proof_;
            }
            else
            {
                complete_ = true;
            }
        }
        return true;
    }

    DedicatedPlayerRuntimeSlot* FindAdmittedNetchanSlot()
    {
        if (admitted_session_id_.empty())
        {
            return nullptr;
        }
        return FindDedicatedPlayerRuntimeSlotBySession(
            state_.dedicated_multiplayer_foundation,
            admitted_session_id_);
    }

    DedicatedPlayerRuntimeSlot* FindNetchanSlotForSender(
        const hl::network::Ipv4Endpoint& sender)
    {
        for (DedicatedPlayerRuntimeSlot& slot_state :
             state_.dedicated_multiplayer_foundation.slots)
        {
            if (IsDedicatedSlotConnectedOccupant(slot_state)
                && slot_state.netchan.initialized()
                && slot_state.netchan.remote_endpoint() == sender)
            {
                return &slot_state;
            }
        }
        return nullptr;
    }

    bool SendBoundedDatagram(
        const std::uint8_t* bytes,
        std::size_t size,
        const hl::network::Ipv4Endpoint& destination)
    {
        if (datagrams_sent_this_frame_ >= kMaximumDatagramsSentPerHostFrame)
        {
            return false;
        }
        if (!socket_.Send(bytes, size, destination))
        {
            return false;
        }
        ++datagrams_sent_this_frame_;
        ++summary_.datagrams_sent;
        return true;
    }

    void RefreshAuthoritativeNetchanSessionSummary()
    {
        const int session_count =
            CountDedicatedQueryPlayers(state_.dedicated_multiplayer_foundation);
        int partial_sessions = 0;
        for (const DedicatedPlayerRuntimeSlot& slot_state :
             state_.dedicated_multiplayer_foundation.slots)
        {
            const bool connected = IsDedicatedSlotConnectedOccupant(slot_state);
            const bool initialized = slot_state.netchan.initialized();
            const bool has_external_transport_metadata =
                !slot_state.remote_endpoint.empty() || initialized;
            if (has_external_transport_metadata && connected != initialized)
            {
                ++partial_sessions;
            }
        }

        summary_.session_count = session_count;
        summary_.duplicate_sessions = std::max(0, session_count - 1);
        summary_.partial_sessions = partial_sessions;
    }

    void SyncNetchanSummary(const DedicatedPlayerRuntimeSlot& slot_state)
    {
        RefreshAuthoritativeNetchanSessionSummary();
        const hl::network::GoldSrcNetchanState& netchan = slot_state.netchan;
        const hl::network::GoldSrcNetchanDiagnostics& diagnostics =
            netchan.diagnostics();
        summary_.netchan_initialized = netchan.initialized();
        summary_.netchan_state = std::string(
            hl::network::NameFor(netchan.transport_phase()));
        summary_.sequenced_datagrams_sent =
            static_cast<int>(diagnostics.sequenced_sent);
        summary_.reliable_queued =
            static_cast<int>(diagnostics.reliable_queued);
        summary_.reliable_sent =
            static_cast<int>(diagnostics.reliable_sent);
        summary_.reliable_resent =
            static_cast<int>(diagnostics.reliable_resent);
        summary_.reliable_acked =
            static_cast<int>(diagnostics.reliable_acked);
        summary_.reliable_ack_mismatch =
            static_cast<int>(diagnostics.reliable_ack_mismatch);
        summary_.reliable_pending_bytes =
            static_cast<int>(netchan.reliable_pending_bytes());
        summary_.duplicate_rejected =
            static_cast<int>(diagnostics.duplicate_sequence);
        summary_.out_of_order_rejected =
            static_cast<int>(diagnostics.out_of_order_sequence);
        summary_.malformed_rejected = static_cast<int>(
            diagnostics.malformed_header
            + diagnostics.unsupported_reserved_flags);
        summary_.fragment_rejected =
            static_cast<int>(diagnostics.unsupported_fragment);
        summary_.stale_ack_rejected =
            static_cast<int>(diagnostics.stale_ack);
        summary_.future_ack_rejected =
            static_cast<int>(diagnostics.future_ack);
        summary_.unsupported_payload_rejected =
            static_cast<int>(diagnostics.unsupported_payload);
        summary_.qport_mismatch_rejected =
            static_cast<int>(diagnostics.qport_mismatch);
        summary_.final_state =
            DedicatedPlayerLifecycleStateName(slot_state.lifecycle_state);
        summary_.connected = slot_state.connected;
        summary_.put_in_server = slot_state.put_in_server;
        summary_.spawned = slot_state.spawn_count > 0 || slot_state.alive;
        summary_.active = slot_state.put_in_server && slot_state.alive;
        if (serverinfo_enabled_)
        {
            const hl::network::GoldSrcSignonDiagnostics& signon_diagnostics =
                slot_state.goldsrc_signon.diagnostics();
            summary_.client_new_received = static_cast<int>(
                signon_diagnostics.client_new_received);
            summary_.client_new_delivered = static_cast<int>(
                signon_diagnostics.client_new_delivered);
            summary_.duplicate_new_deliveries = std::max(
                0,
                summary_.client_new_delivered - 1);
            summary_.serverinfo_queued = static_cast<int>(
                signon_diagnostics.serverinfo_queued);
            summary_.serverinfo_acked = static_cast<int>(
                signon_diagnostics.serverinfo_acknowledged);
            summary_.duplicate_client_reliable_suppressed =
                signon_diagnostics.duplicate_new_suppressed > 0u;
            summary_.signon_phase = std::string(
                hl::network::NameFor(slot_state.goldsrc_signon.phase()));
        }
    }

    static std::string HexUint32(std::uint32_t value)
    {
        std::ostringstream stream;
        stream << std::hex << std::setfill('0') << std::setw(8) << value;
        return stream.str();
    }

    static std::string HexDigest(
        const std::array<
            std::uint8_t,
            hl::network::kGoldSrcClientDllDigestBytes>& digest)
    {
        std::ostringstream stream;
        stream << std::hex << std::setfill('0');
        for (const std::uint8_t byte : digest)
        {
            stream << std::setw(2) << static_cast<unsigned int>(byte);
        }
        return stream.str();
    }

    bool PrepareServerInfoContext(DedicatedPlayerRuntimeSlot& slot_state)
    {
        if (slot_state.goldsrc_serverinfo_payload.has_value())
        {
            return true;
        }
        if (!state_.world_context.prepared
            || !state_.world_context.bsp_loaded
            || state_.world_context.file_path.empty())
        {
            return FailTransport(
                "serverinfo-map-context-unavailable",
                "goldsrc_serverinfo_context_failed: reason=map-context-unavailable");
        }
        if (state_.server_state.maxclients < 1
            || state_.server_state.maxclients > 255
            || slot_state.slot < 1
            || slot_state.slot > state_.server_state.maxclients
            || slot_state.slot > 256)
        {
            return FailTransport(
                "serverinfo-slot-context-invalid",
                "goldsrc_serverinfo_context_failed: reason=slot-context-invalid");
        }

        const hl::network::GoldSrcMapChecksumResult map_checksum =
            hl::network::ComputeGoldSrcBsp30MapChecksum(
                state_.world_context.file_path);
        if (!map_checksum.ok())
        {
            return FailTransport(
                "serverinfo-map-checksum-failed",
                "goldsrc_serverinfo_context_failed: reason="
                    + std::string(hl::network::ReasonFor(map_checksum.status)));
        }
        const hl::network::GoldSrcMd5Result client_dll_md5 =
            hl::network::ComputeGoldSrcClientDllMd5(client_dll_path_);
        if (!client_dll_md5.ok())
        {
            return FailTransport(
                "serverinfo-client-dll-md5-failed",
                "goldsrc_serverinfo_context_failed: reason="
                    + std::string(hl::network::ReasonFor(client_dll_md5.status)));
        }
        const std::optional<hl::game_api::CvarSnapshot> mapcycle =
            state_.cvar_registry.SnapshotOf("mapcyclefile");
        if (!mapcycle.has_value())
        {
            return FailTransport(
                "serverinfo-mapcycle-unavailable",
                "goldsrc_serverinfo_context_failed: reason=mapcycle-unavailable");
        }

        hl::network::GoldSrcServerInfoContext context;
        context.protocol_version =
            hl::network::kGoldSrcServerInfoProtocolVersion;
        context.spawn_count = state_.server_state.map_spawn_count;
        context.canonical_map_checksum = map_checksum.checksum;
        context.client_dll_md5 = client_dll_md5.digest;
        context.max_clients = static_cast<std::uint8_t>(
            state_.server_state.maxclients);
        context.player_index = static_cast<std::uint8_t>(slot_state.slot - 1);
        context.deathmatch = state_.server_state.coop == 0.0f
            && state_.server_state.deathmatch != 0.0f;
        context.game_directory = state_.server_state.mod_name;
        context.hostname = state_.server_state.hostname;
        context.map_model_path = state_.world_context.model_path;
        context.mapcycle = mapcycle->string_value;
        context.secure = false;
        context.fallback_game_directory.clear();
        context.cheats = false;

        const hl::network::GoldSrcServerInfoValidationResult validation =
            hl::network::ValidateGoldSrcServerInfoContext(context);
        if (!validation.ok())
        {
            return FailTransport(
                "serverinfo-context-validation-failed",
                "goldsrc_serverinfo_context_failed: reason="
                    + std::string(hl::network::ReasonFor(validation.status)));
        }
        hl::network::GoldSrcServerInfoEncodeResult encoded =
            hl::network::EncodeGoldSrcServerInfo(context);
        if (!encoded.ok())
        {
            return FailTransport(
                "serverinfo-encode-failed",
                "goldsrc_serverinfo_context_failed: reason="
                    + std::string(hl::network::ReasonFor(encoded.status)));
        }

        summary_.serverinfo_context_built = true;
        ++summary_.serverinfo_generations;
        summary_.serverinfo_protocol = static_cast<int>(context.protocol_version);
        summary_.serverinfo_spawn_count = context.spawn_count;
        summary_.serverinfo_checksum = HexUint32(
            hl::network::MungeGoldSrcServerInfoChecksum(
                *context.canonical_map_checksum,
                context.player_index));
        summary_.serverinfo_client_dll_md5 = HexDigest(
            *context.client_dll_md5);
        summary_.serverinfo_maxclients = static_cast<int>(context.max_clients);
        summary_.serverinfo_player_index = static_cast<int>(context.player_index);
        summary_.serverinfo_deathmatch = context.deathmatch;
        summary_.serverinfo_game_dir = SanitizeLogField(context.game_directory);
        summary_.serverinfo_hostname = SanitizeLogField(context.hostname);
        summary_.serverinfo_map = SanitizeLogField(context.map_model_path);
        summary_.serverinfo_mapcycle = SanitizeLogField(context.mapcycle);
        summary_.serverinfo_secure = context.secure;
        summary_.serverinfo_fallback_dir = context.fallback_game_directory.empty()
            ? "<empty>"
            : SanitizeLogField(context.fallback_game_directory);
        summary_.serverinfo_allow_cheats = context.cheats;
        summary_.serverinfo_payload_bytes = static_cast<int>(encoded.payload.size);
        slot_state.goldsrc_serverinfo_context = std::move(context);
        slot_state.goldsrc_serverinfo_payload = std::move(encoded.payload);
        hl::common::Logger::Info(
            hl::common::LogCategory::Server,
            "goldsrc_serverinfo_context_built: session_id="
                + slot_state.session_id
                + ",protocol=" + std::to_string(summary_.serverinfo_protocol)
                + ",spawn_count="
                + std::to_string(summary_.serverinfo_spawn_count)
                + ",player_index="
                + std::to_string(summary_.serverinfo_player_index)
                + ",payload_bytes="
                + std::to_string(summary_.serverinfo_payload_bytes));
        return true;
    }

    static bool PayloadIsClientNopOnly(
        const hl::network::GoldSrcNetchanPacket& packet) noexcept
    {
        for (std::size_t index = 0; index < packet.payload_size; ++index)
        {
            if (packet.payload[index] != hl::network::kGoldSrcClientNop)
            {
                return false;
            }
        }
        return true;
    }

    static bool PayloadLooksLikeCommandInjection(
        const hl::network::GoldSrcNetchanPacket& packet) noexcept
    {
        std::size_t index = 0;
        while (index < packet.payload_size
            && packet.payload[index] == hl::network::kGoldSrcClientNop)
        {
            ++index;
        }
        if (index >= packet.payload_size
            || packet.payload[index]
                != hl::network::kGoldSrcClientStringCommandOpcode)
        {
            return false;
        }
        ++index;
        constexpr std::array<std::uint8_t, 3> kNew = {'n', 'e', 'w'};
        std::size_t command_size = 0;
        bool starts_with_new = true;
        bool contains_separator = false;
        for (; index < packet.payload_size && packet.payload[index] != 0u;
             ++index, ++command_size)
        {
            if (command_size < kNew.size()
                && packet.payload[index] != kNew[command_size])
            {
                starts_with_new = false;
            }
            contains_separator = contains_separator
                || packet.payload[index] == ';'
                || packet.payload[index] == '\n'
                || packet.payload[index] == '\r';
        }
        return contains_separator
            || (starts_with_new && command_size > kNew.size());
    }

    void RecordClientSignonRejection(
        const hl::network::GoldSrcNetchanPacket& packet,
        hl::network::GoldSrcClientSignonDecodeStatus status)
    {
        if (status == hl::network::GoldSrcClientSignonDecodeStatus::
                kUnsupportedOpcode)
        {
            ++summary_.unsupported_opcode_rejected;
        }
        else if (status == hl::network::GoldSrcClientSignonDecodeStatus::
                     kUnsupportedCommand
            || status == hl::network::GoldSrcClientSignonDecodeStatus::
                     kMultipleCommands
            || status == hl::network::GoldSrcClientSignonDecodeStatus::
                     kUnsupportedTrailingData)
        {
            if (PayloadLooksLikeCommandInjection(packet)
                || status
                    != hl::network::GoldSrcClientSignonDecodeStatus::
                        kUnsupportedCommand)
            {
                ++summary_.command_injection_rejected;
            }
            else
            {
                ++summary_.unsupported_command_rejected;
            }
        }
        else
        {
            ++summary_.malformed_stringcmd_rejected;
        }
        hl::common::Logger::Warn(
            hl::common::LogCategory::Server,
            "goldsrc_client_signon_rejected: reason="
                + std::string(hl::network::ReasonFor(status)));
    }

    bool HandleClientSignonPayload(
        DedicatedPlayerRuntimeSlot& slot_state,
        const hl::network::GoldSrcNetchanPacket& packet,
        bool* serverinfo_queued)
    {
        if (serverinfo_queued == nullptr)
        {
            return false;
        }
        *serverinfo_queued = false;
        if (PayloadIsClientNopOnly(packet))
        {
            return true;
        }
        if (packet.reliable_present)
        {
            ++summary_.inbound_reliable_detected;
        }
        if (!packet.reliable_present)
        {
            ++summary_.unsupported_command_rejected;
            hl::common::Logger::Warn(
                hl::common::LogCategory::Server,
                "goldsrc_client_signon_rejected: reason=unreliable-command");
            return true;
        }

        const hl::network::GoldSrcClientSignonDecodeResult decoded =
            hl::network::DecodeGoldSrcClientSignonPayload(
                packet.payload.data(),
                packet.payload_size);
        if (!decoded.ok())
        {
            if (decoded.status
                    == hl::network::GoldSrcClientSignonDecodeStatus::
                        kUnsupportedCommand
                || decoded.status
                    == hl::network::GoldSrcClientSignonDecodeStatus::
                        kMultipleCommands
                || decoded.status
                    == hl::network::GoldSrcClientSignonDecodeStatus::
                        kUnsupportedTrailingData)
            {
                ++summary_.clc_stringcmd_decoded;
            }
            RecordClientSignonRejection(packet, decoded.status);
            return true;
        }
        ++summary_.clc_stringcmd_decoded;
        if (decoded.command != hl::network::GoldSrcClientSignonCommand::kNew)
        {
            ++summary_.unsupported_command_rejected;
            return true;
        }

        if (slot_state.goldsrc_signon.phase()
            != hl::network::GoldSrcSignonPhase::kAwaitingNew)
        {
            const hl::network::GoldSrcSignonCommandDisposition disposition =
                slot_state.goldsrc_signon.HandleClientCommand(decoded.command);
            if (disposition
                == hl::network::GoldSrcSignonCommandDisposition::
                    kDuplicateSuppressed)
            {
                ++summary_.inbound_reliable_retransmitted;
                summary_.duplicate_client_reliable_suppressed = true;
                SyncNetchanSummary(slot_state);
                hl::common::Logger::Info(
                    hl::common::LogCategory::Server,
                    "goldsrc_client_new_suppressed: session_id="
                        + slot_state.session_id + ",reason=already-delivered");
                return true;
            }
            SyncNetchanSummary(slot_state);
            return true;
        }
        if (!PrepareServerInfoContext(slot_state))
        {
            return false;
        }
        if (!slot_state.goldsrc_serverinfo_payload.has_value())
        {
            return FailTransport(
                "serverinfo-payload-missing",
                "goldsrc_serverinfo_queue_failed: reason=payload-missing");
        }
        const hl::network::GoldSrcNetchanQueueResult queue_result =
            slot_state.netchan.QueueReliablePayload(
                slot_state.goldsrc_serverinfo_payload->bytes.data(),
                slot_state.goldsrc_serverinfo_payload->size,
                hl::network::GoldSrcNetchanReliablePayloadKind::kServerInfo);
        if (queue_result != hl::network::GoldSrcNetchanQueueResult::kQueued)
        {
            return FailTransport(
                "serverinfo-reliable-queue-failed",
                "goldsrc_serverinfo_queue_failed: reason="
                    + std::string(hl::network::ReasonFor(queue_result)));
        }
        const hl::network::GoldSrcSignonCommandDisposition disposition =
            slot_state.goldsrc_signon.HandleClientCommand(decoded.command);
        if (disposition
            != hl::network::GoldSrcSignonCommandDisposition::kDelivered)
        {
            return FailTransport(
                "serverinfo-signon-commit-failed",
                "goldsrc_serverinfo_queue_failed: reason=signon-commit-failed");
        }
        *serverinfo_queued = true;
        SyncNetchanSummary(slot_state);
        hl::common::Logger::Info(
            hl::common::LogCategory::Server,
            "goldsrc_serverinfo_queued: session_id=" + slot_state.session_id
                + ",payload_bytes="
                + std::to_string(slot_state.goldsrc_serverinfo_payload->size)
                + ",signon_phase=" + summary_.signon_phase);
        return true;
    }

    bool SendNetchanDatagram(
        DedicatedPlayerRuntimeSlot& slot_state,
        bool expect_reliable,
        std::string_view event_name)
    {
        const hl::network::GoldSrcNetchanReliablePayloadKind pending_kind =
            slot_state.netchan.pending_reliable_kind();
        const std::uint64_t reliable_sent_before =
            slot_state.netchan.diagnostics().reliable_sent;
        const std::uint64_t reliable_resent_before =
            slot_state.netchan.diagnostics().reliable_resent;
        hl::network::GoldSrcNetchanDatagram datagram;
        if (!slot_state.netchan.BuildOutgoingDatagram(&datagram))
        {
            return FailTransport(
                "netchan-encode-failed",
                "goldsrc_netchan_send_failed: endpoint="
                    + slot_state.netchan.remote_endpoint().ToString()
                    + ",reason=encode-failed");
        }
        const hl::network::GoldSrcNetchanDecodeResult decoded =
            hl::network::DecodeGoldSrcNetchanDatagram(
                hl::network::GoldSrcNetchanDirection::kServerToClient,
                datagram.bytes.data(),
                datagram.size);
        if (!decoded.ok() || decoded.packet.reliable_present != expect_reliable)
        {
            return FailTransport(
                "netchan-self-check-failed",
                "goldsrc_netchan_send_failed: endpoint="
                    + slot_state.netchan.remote_endpoint().ToString()
                    + ",reason=self-check-failed");
        }
        if (!SendBoundedDatagram(
                datagram.bytes.data(),
                datagram.size,
                slot_state.netchan.remote_endpoint()))
        {
            return FailTransport(
                "netchan-send-failed",
                "goldsrc_netchan_send_failed: endpoint="
                    + slot_state.netchan.remote_endpoint().ToString()
                    + ",platform_error=" + socket_.LastError());
        }

        if (serverinfo_enabled_
            && pending_kind
                == hl::network::GoldSrcNetchanReliablePayloadKind::kServerInfo
            && decoded.packet.reliable_present)
        {
            if (summary_.serverinfo_send_count == 0)
            {
                summary_.serverinfo_first_carrier_sequence =
                    decoded.packet.sequence;
            }
            summary_.serverinfo_latest_carrier_sequence =
                decoded.packet.sequence;
            ++summary_.serverinfo_send_count;
            const hl::network::GoldSrcNetchanDiagnostics& diagnostics =
                slot_state.netchan.diagnostics();
            if (diagnostics.reliable_resent > reliable_resent_before)
            {
                ++summary_.serverinfo_resent;
            }
            else if (diagnostics.reliable_sent > reliable_sent_before)
            {
                ++summary_.serverinfo_sent;
                if (slot_state.goldsrc_signon.MarkServerInfoSent()
                    != hl::network::GoldSrcSignonTransitionResult::kAdvanced)
                {
                    return FailTransport(
                        "serverinfo-sent-transition-failed",
                        "goldsrc_serverinfo_send_failed: reason=invalid-signon-phase");
                }
            }
        }

        SyncNetchanSummary(slot_state);
        hl::common::Logger::Info(
            hl::common::LogCategory::Server,
            std::string(event_name)
                + ": session_id=" + slot_state.session_id
                + ",endpoint=" + slot_state.netchan.remote_endpoint().ToString()
                + ",sequence=" + std::to_string(decoded.packet.sequence)
                + ",ack=" + std::to_string(decoded.packet.acknowledgement)
                + ",reliable="
                + (decoded.packet.reliable_present ? "1" : "0")
                + ",fragment=0,payload_bytes="
                + std::to_string(decoded.packet.payload_size)
                + (serverinfo_enabled_
                        && pending_kind
                            == hl::network::
                                GoldSrcNetchanReliablePayloadKind::kServerInfo
                    ? ",signon_phase=" + summary_.signon_phase
                        + ",serverinfo_generations="
                        + std::to_string(summary_.serverinfo_generations)
                    : ""));
        return true;
    }

    bool SendInitialNetchanDatagram()
    {
        DedicatedPlayerRuntimeSlot* slot_state = FindAdmittedNetchanSlot();
        if (slot_state == nullptr || !slot_state->netchan.initialized())
        {
            return FailTransport(
                "netchan-session-missing",
                "goldsrc_netchan_send_failed: reason=session-missing");
        }

        summary_.netchan_initialized = true;
        summary_.netchan_state = std::string(
            hl::network::NameFor(slot_state->netchan.transport_phase()));
        hl::common::Logger::Info(
            hl::common::LogCategory::Server,
            "goldsrc_netchan_initialized: session_id=" + slot_state->session_id
                + ",slot=" + std::to_string(slot_state->slot)
                + ",endpoint=" + slot_state->netchan.remote_endpoint().ToString()
                + ",outgoing_sequence=1,incoming_sequence=0,qport_wire=absent"
                + ",channel_id="
                + std::to_string(slot_state->netchan.channel_identifier()));
        hl::common::Logger::Info(
            hl::common::LogCategory::Server,
            "goldsrc_netchan_reliable_queued: session_id="
                + slot_state->session_id
                + ",payload=svc_nop,payload_bytes=1,reliable_toggle=1");

        if (!SendNetchanDatagram(
                *slot_state,
                true,
                "goldsrc_netchan_reliable_sent"))
        {
            return false;
        }

        summary_.server_initial_sequence = 1u;
        summary_.server_initial_acknowledgement = 0u;
        summary_.reliable_present = true;
        summary_.fragment_present = false;
        summary_.payload_transform_passed = true;
        summary_.first_payload = "svc_nop";
        return true;
    }

    bool SendPendingOrdinaryAdvance()
    {
        DedicatedPlayerRuntimeSlot* slot_state = FindAdmittedNetchanSlot();
        if (slot_state == nullptr
            || !slot_state->netchan.initialized()
            || !slot_state->netchan.reliable_pending())
        {
            return FailTransport(
                "netchan-negative-advance-invalid",
                "goldsrc_netchan_send_failed: reason=negative-advance-invalid");
        }
        return SendNetchanDatagram(
            *slot_state,
            false,
            "goldsrc_netchan_ordinary_sent");
    }

    bool NegativeEvidenceSatisfied() const noexcept
    {
        return summary_.duplicate_rejected >= 1
            && summary_.out_of_order_rejected >= 1
            && summary_.malformed_rejected >= 1
            && summary_.fragment_rejected >= 1
            && summary_.future_ack_rejected >= 1
            && summary_.endpoint_hijack_rejected >= 1;
    }

    bool ServerInfoNegativeEvidenceSatisfied() const noexcept
    {
        return summary_.serverinfo_resent >= 1
            && summary_.wrong_reliable_ack_rejected
            && summary_.future_ack_rejected >= 1
            && summary_.duplicate_client_reliable_suppressed
            && summary_.duplicate_rejected >= 1
            && summary_.malformed_stringcmd_rejected >= 3
            && summary_.command_injection_rejected >= 2
            && summary_.unsupported_command_rejected >= 1
            && summary_.unsupported_opcode_rejected >= 1
            && summary_.endpoint_hijack_rejected >= 1;
    }

    bool ProcessSequencedDatagram(
        const hl::network::Ipv4Endpoint& sender,
        std::size_t received_size)
    {
        ++summary_.sequenced_datagrams_received;
        DedicatedPlayerRuntimeSlot* slot_state =
            netchan_enabled_ ? FindNetchanSlotForSender(sender) : nullptr;
        if (slot_state == nullptr)
        {
            ++summary_.endpoint_hijack_rejected;
            RefreshAuthoritativeNetchanSessionSummary();
            ++unknown_endpoint_diagnostics_seen_;
            if (ShouldLogDiagnosticSample(unknown_endpoint_diagnostics_seen_))
            {
                hl::common::Logger::Warn(
                    hl::common::LogCategory::Server,
                    "goldsrc_netchan_unknown_endpoint: endpoint="
                        + sender.ToString());
            }
            return true;
        }

        const hl::network::GoldSrcNetchanDecodeResult decoded =
            hl::network::DecodeGoldSrcNetchanDatagram(
                hl::network::GoldSrcNetchanDirection::kClientToServer,
                buffer_.data(),
                received_size);
        if (!decoded.ok())
        {
            hl::network::GoldSrcNetchanProcessResult rejection =
                hl::network::GoldSrcNetchanProcessResult::kMalformedHeader;
            if (decoded.status
                == hl::network::GoldSrcNetchanCodecStatus::kUnsupportedFragment)
            {
                rejection =
                    hl::network::GoldSrcNetchanProcessResult::kUnsupportedFragment;
            }
            else if (decoded.status
                == hl::network::GoldSrcNetchanCodecStatus::kUnsupportedReservedFlags)
            {
                rejection = hl::network::GoldSrcNetchanProcessResult::
                    kUnsupportedReservedFlags;
            }
            slot_state->netchan.RecordRejected(rejection);
            SyncNetchanSummary(*slot_state);
            ++invalid_netchan_diagnostics_seen_;
            if (ShouldLogDiagnosticSample(invalid_netchan_diagnostics_seen_))
            {
                hl::common::Logger::Warn(
                    hl::common::LogCategory::Server,
                    "goldsrc_netchan_rejected: endpoint=" + sender.ToString()
                        + ",reason="
                        + std::string(hl::network::ReasonFor(rejection)));
            }
            return true;
        }

        const hl::network::GoldSrcNetchanTransportPhase phase_before =
            slot_state->netchan.transport_phase();
        const hl::network::GoldSrcNetchanReliablePayloadKind pending_kind_before =
            slot_state->netchan.pending_reliable_kind();
        const std::uint64_t reliable_ack_mismatch_before =
            slot_state->netchan.diagnostics().reliable_ack_mismatch;
        const hl::network::GoldSrcNetchanProcessOutcome process_outcome =
            slot_state->netchan.ProcessIncomingDatagramDetailed(
                sender,
                decoded.packet,
                std::chrono::steady_clock::now());
        const hl::network::GoldSrcNetchanProcessResult process_result =
            process_outcome.result;
        SyncNetchanSummary(*slot_state);
        if (process_result != hl::network::GoldSrcNetchanProcessResult::kAccepted)
        {
            ++invalid_netchan_diagnostics_seen_;
            if (ShouldLogDiagnosticSample(invalid_netchan_diagnostics_seen_))
            {
                hl::common::Logger::Warn(
                    hl::common::LogCategory::Server,
                    "goldsrc_netchan_rejected: endpoint=" + sender.ToString()
                        + ",sequence=" + std::to_string(decoded.packet.sequence)
                        + ",ack=" + std::to_string(decoded.packet.acknowledgement)
                        + ",reason="
                        + std::string(hl::network::ReasonFor(process_result)));
            }
            return true;
        }

        if (serverinfo_enabled_
            && pending_kind_before
                == hl::network::GoldSrcNetchanReliablePayloadKind::kServerInfo
            && slot_state->netchan.diagnostics().reliable_ack_mismatch
                > reliable_ack_mismatch_before)
        {
            summary_.wrong_reliable_ack_rejected = true;
        }
        if (serverinfo_enabled_
            && process_outcome.acknowledged_reliable_kind
                == hl::network::GoldSrcNetchanReliablePayloadKind::kServerInfo)
        {
            if (slot_state->goldsrc_signon.MarkServerInfoAcknowledged()
                != hl::network::GoldSrcSignonTransitionResult::kAdvanced)
            {
                return FailTransport(
                    "serverinfo-ack-transition-failed",
                    "goldsrc_serverinfo_ack_failed: reason=invalid-signon-phase");
            }
            SyncNetchanSummary(*slot_state);
            hl::common::Logger::Info(
                hl::common::LogCategory::Server,
                "goldsrc_serverinfo_acknowledged: session_id="
                    + slot_state->session_id
                    + ",ack="
                    + std::to_string(decoded.packet.acknowledgement)
                    + ",ack_generation="
                    + std::to_string(
                        process_outcome.reliable_acknowledgement_generation)
                    + ",signon_phase=" + summary_.signon_phase);
        }

        hl::common::Logger::Info(
            hl::common::LogCategory::Server,
            "goldsrc_netchan_received: endpoint=" + sender.ToString()
                + ",sequence=" + std::to_string(decoded.packet.sequence)
                + ",ack=" + std::to_string(decoded.packet.acknowledgement)
                + ",reliable="
                + (decoded.packet.reliable_present ? "1" : "0")
                + ",reliable_ack="
                + (decoded.packet.reliable_acknowledgement ? "1" : "0")
                + ",result=accepted");

        const bool established_now =
            phase_before != hl::network::GoldSrcNetchanTransportPhase::kEstablished
            && slot_state->netchan.transport_phase()
                == hl::network::GoldSrcNetchanTransportPhase::kEstablished;
        if (established_now)
        {
            hl::common::Logger::Info(
                hl::common::LogCategory::Server,
                "goldsrc_netchan_established: session_id="
                    + slot_state->session_id
                    + ",session_count=" + std::to_string(summary_.session_count)
                    + ",state=" + summary_.final_state
                    + ",netchan_state=established,reliable_acked="
                    + std::to_string(summary_.reliable_acked)
                    + ",reliable_pending_bytes=0,put_in_server="
                    + (summary_.put_in_server ? "1" : "0")
                    + ",spawned=" + (summary_.spawned ? "1" : "0")
                    + ",active=" + (summary_.active ? "1" : "0"));
            if (serverinfo_enabled_)
            {
                if (slot_state->goldsrc_signon.EnterAwaitingNew()
                        != hl::network::GoldSrcSignonTransitionResult::kAdvanced)
                {
                    return FailTransport(
                        "signon-session-binding-failed",
                        "goldsrc_signon_transition_failed: reason=session-binding");
                }
                SyncNetchanSummary(*slot_state);
                hl::common::Logger::Info(
                    hl::common::LogCategory::Server,
                    "goldsrc_signon_awaiting_new: session_id="
                        + slot_state->session_id
                        + ",signon_phase=" + summary_.signon_phase);
            }
            else
            {
                if (!negative_proof_)
                {
                    complete_ = true;
                }
                return true;
            }
        }

        bool serverinfo_queued = false;
        if (serverinfo_enabled_
            && slot_state->netchan.transport_phase()
                == hl::network::GoldSrcNetchanTransportPhase::kEstablished)
        {
            if (!HandleClientSignonPayload(
                    *slot_state,
                    decoded.packet,
                    &serverinfo_queued))
            {
                return false;
            }
            if (serverinfo_queued)
            {
                return SendNetchanDatagram(
                    *slot_state,
                    true,
                    "goldsrc_serverinfo_sent");
            }
        }

        if (slot_state->netchan.reliable_pending())
        {
            const bool resend_expected =
                hl::network::GoldSrcNetchanSentWindowContains(
                    slot_state->netchan.highest_accepted_acknowledgement(),
                    slot_state->netchan.highest_sequence_sent(),
                    slot_state->netchan.diagnostics().sequenced_sent)
                && hl::network::IsGoldSrcNetchanSequenceNewer(
                    slot_state->netchan.highest_accepted_acknowledgement(),
                    slot_state->netchan.last_reliable_sequence())
                && slot_state->netchan.incoming_reliable_acknowledgement()
                    != slot_state->netchan.local_reliable_sequence();
            return SendNetchanDatagram(
                *slot_state,
                resend_expected,
                resend_expected
                    ? "goldsrc_netchan_reliable_resent"
                    : "goldsrc_netchan_ordinary_sent");
        }

        if (serverinfo_enabled_)
        {
            if (!SendNetchanDatagram(
                    *slot_state,
                    false,
                    "goldsrc_serverinfo_ack_response"))
            {
                return false;
            }
            if (slot_state->goldsrc_signon.phase()
                == hl::network::GoldSrcSignonPhase::
                    kServerInfoAcknowledged)
            {
                if (!serverinfo_negative_proof_)
                {
                    complete_ = true;
                }
                else if (ServerInfoNegativeEvidenceSatisfied())
                {
                    summary_.server_still_responsive = true;
                    complete_ = true;
                }
            }
            return true;
        }

        if (negative_proof_
            && slot_state->netchan.transport_phase()
                == hl::network::GoldSrcNetchanTransportPhase::kEstablished
            && NegativeEvidenceSatisfied())
        {
            if (!SendNetchanDatagram(
                    *slot_state,
                    false,
                    "goldsrc_netchan_responsive_ack"))
            {
                return false;
            }
            summary_.server_still_responsive = true;
            complete_ = true;
        }
        return true;
    }

    bool CaptureAcceptedSession(const hl::network::HandshakeResult& handshake)
    {
        if (!handshake.admission.has_value())
        {
            return FailTransport(
                "accepted-admission-missing",
                "goldsrc_udp_processing_failed: reason=accepted-admission-missing");
        }

        const hl::network::AdmissionResult& admission = *handshake.admission;
        DedicatedPlayerRuntimeSlot* slot_state =
            FindDedicatedPlayerRuntimeSlotBySession(
                state_.dedicated_multiplayer_foundation,
                admission.session_identifier);
        if (slot_state == nullptr)
        {
            return FailTransport(
                "accepted-session-missing",
                "goldsrc_udp_processing_failed: reason=accepted-session-missing");
        }

        summary_.session_count =
            CountDedicatedQueryPlayers(state_.dedicated_multiplayer_foundation);
        summary_.allocated_slot = slot_state->slot;
        summary_.session_id = slot_state->session_id;
        admitted_session_id_ = slot_state->session_id;
        summary_.remote_endpoint = slot_state->remote_endpoint;
        summary_.final_state =
            DedicatedPlayerLifecycleStateName(slot_state->lifecycle_state);
        summary_.connected = slot_state->connected;
        summary_.put_in_server = slot_state->put_in_server;
        summary_.spawned = slot_state->spawn_count > 0 || slot_state->alive;
        summary_.active = slot_state->put_in_server && slot_state->alive;
        summary_.last_reject_reason = "<none>";
        if (serverinfo_enabled_)
        {
            ++summary_.signon_state_resets;
            hl::common::Logger::Info(
                hl::common::LogCategory::Server,
                "goldsrc_signon_reset: session_id=" + slot_state->session_id
                    + ",slot=" + std::to_string(slot_state->slot)
                    + ",reason=admission,signon_phase=none");
        }
        if (netchan_enabled_)
        {
            if (!slot_state->netchan.initialized())
            {
                return FailTransport(
                    "netchan-initialization-missing",
                    "goldsrc_netchan_initialization_failed: session_id="
                        + slot_state->session_id);
            }
            SyncNetchanSummary(*slot_state);
        }

        hl::common::Logger::Info(
            hl::common::LogCategory::Server,
            "goldsrc_udp_accept: session_id=" + summary_.session_id
                + ",slot=" + std::to_string(summary_.allocated_slot)
                + ",endpoint=" + summary_.remote_endpoint
                + ",state=" + summary_.final_state
                + ",name=" + SanitizeLogField(slot_state->player_name));
        hl::common::Logger::Info(
            hl::common::LogCategory::Server,
            "goldsrc_udp_session: count=" + std::to_string(summary_.session_count)
                + ",state=" + summary_.final_state
                + ",connected=" + (summary_.connected ? "1" : "0")
                + ",put_in_server=" + (summary_.put_in_server ? "1" : "0")
                + ",spawned=" + (summary_.spawned ? "1" : "0")
                + ",active=" + (summary_.active ? "1" : "0")
                + ",slot=" + std::to_string(summary_.allocated_slot)
                + ",endpoint=" + summary_.remote_endpoint
                + ",protocol=" + std::to_string(slot_state->network_protocol)
                + ",qport=" + std::to_string(slot_state->qport)
                + ",channel_id=" + std::to_string(slot_state->channel_identifier)
                + ",challenge=" + std::to_string(slot_state->challenge)
                + ",auth_protocol=" + std::to_string(slot_state->auth_protocol)
                + ",extensions=" + std::to_string(slot_state->protocol_extensions)
                + ",steam_auth="
                + (slot_state->steam_authentication_performed ? "1" : "0")
                + ",protocol_info="
                + (slot_state->protocol_info.empty() ? "missing" : "present")
                + ",user_info="
                + (slot_state->user_info.empty() ? "missing" : "present"));
        return true;
    }

    static std::string SanitizeLogField(std::string value)
    {
        for (char& character : value)
        {
            const unsigned char byte = static_cast<unsigned char>(character);
            if (character == ',' || character == '=' || character == ' '
                || byte < 0x20 || byte > 0x7E)
            {
                character = '_';
            }
        }
        return value;
    }

    EngineShimState& state_;
    hl::game_api::GoldSrcUdpHandshakeSummary& summary_;
    std::string bind_address_;
    int requested_port_ = 0;
    int timeout_ms_ = 0;
    std::filesystem::path client_dll_path_;
    bool netchan_enabled_ = false;
    bool negative_proof_ = false;
    bool serverinfo_enabled_ = false;
    bool serverinfo_negative_proof_ = false;
    bool dedicated_runtime_ = false;
    hl::network::UdpSocket socket_;
    std::unique_ptr<hl::network::ChallengeTable> challenges_;
    std::unique_ptr<hl::network::HandshakeProcessor> processor_;
    std::array<std::uint8_t, hl::network::kGoldSrcMaximumDatagramBytes> buffer_{};
    std::chrono::steady_clock::time_point deadline_{};
    std::uint64_t challenge_diagnostics_seen_ = 0;
    std::uint64_t rejection_diagnostics_seen_ = 0;
    std::uint64_t malformed_diagnostics_seen_ = 0;
    std::uint64_t expired_diagnostics_seen_ = 0;
    std::uint64_t unknown_endpoint_diagnostics_seen_ = 0;
    std::uint64_t invalid_netchan_diagnostics_seen_ = 0;
    std::uint64_t next_network_session_ = 1;
    std::string admitted_session_id_;
    int datagrams_sent_this_frame_ = 0;
    bool negative_advance_pending_ = false;
    bool deadline_started_ = false;
    bool started_ = false;
    bool complete_ = false;
    bool transport_failed_ = false;
    bool accepted_ = false;
    bool shutdown_ = false;
};
