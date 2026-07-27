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
          delta_descriptions_enabled_(
              options.goldsrc_delta_descriptions_enabled),
          delta_descriptions_negative_proof_(
              options.goldsrc_delta_descriptions_negative_proof),
          delta_descriptions_fixture_(
              options.goldsrc_delta_descriptions_fixture),
          resource_manifest_enabled_(
              options.goldsrc_resource_manifest_enabled),
          resource_manifest_negative_proof_(
              options.goldsrc_resource_manifest_negative_proof),
          resource_manifest_fixture_(
              options.goldsrc_resource_manifest_fixture),
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
        summary_.delta_descriptions_enabled = delta_descriptions_enabled_;
        summary_.delta_descriptions_negative_proof =
            delta_descriptions_negative_proof_;
        summary_.resource_manifest_enabled = resource_manifest_enabled_;
        summary_.resource_manifest_negative_proof =
            resource_manifest_negative_proof_;
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
        if (delta_descriptions_enabled_ && !serverinfo_enabled_)
        {
            return FailBind("delta-descriptions-require-serverinfo");
        }
        if (delta_descriptions_negative_proof_
            && !delta_descriptions_enabled_)
        {
            return FailBind(
                "delta-descriptions-negative-proof-requires-delta-descriptions");
        }
        if (delta_descriptions_fixture_.has_value()
            && !delta_descriptions_enabled_)
        {
            return FailBind(
                "delta-descriptions-fixture-requires-delta-descriptions");
        }
        if (resource_manifest_enabled_ && !serverinfo_enabled_)
        {
            return FailBind("resource-manifest-requires-serverinfo");
        }
        if (resource_manifest_negative_proof_ && !resource_manifest_enabled_)
        {
            return FailBind(
                "resource-manifest-negative-proof-requires-resource-manifest");
        }
        if (resource_manifest_fixture_.has_value()
            && !resource_manifest_enabled_)
        {
            return FailBind("resource-manifest-fixture-requires-resource-manifest");
        }
        if ((resource_manifest_enabled_ || delta_descriptions_enabled_)
            && bind_address_ != "127.0.0.1")
        {
            return FailBind("extended-signon-loopback-required");
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
        if (DedicatedPlayerRuntimeSlot* slot_state =
                FindAdmittedNetchanSlot();
            slot_state != nullptr
            && slot_state->netchan.ExpireFragmentTransfer(frame_time))
        {
            SyncNetchanSummary(*slot_state);
            return FailTransport(
                "fragment-transfer-timeout",
                "goldsrc_fragment_transfer_failed: session_id="
                    + slot_state->session_id + ",reason=timeout");
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
        const int expected_accept_count =
            summary_.delta_slot_resets > 0 ? 2 : 1;
        const bool handshake_succeeded = complete_
            && accepted_
            && !transport_failed_
            && summary_.accepted == expected_accept_count
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

        const bool legacy_fragmentation_rejection_succeeded =
            resource_manifest_enabled_
            && resource_manifest_negative_proof_
            && summary_.requires_fragmentation_reported
            && summary_.oversized_manifest_not_truncated
            && summary_.resource_manifest_preparation_attempts == 1
            && summary_.resource_manifest_cached_outcome_reuses >= 1;
        int expected_reliable_acknowledgements = 1;
        if (serverinfo_enabled_)
        {
            expected_reliable_acknowledgements +=
                delta_descriptions_enabled_
                    && summary_.signon_bootstrap_fragmented
                ? summary_.signon_bootstrap_fragment_count
                : 1;
        }
        if (resource_manifest_enabled_
            && !legacy_fragmentation_rejection_succeeded)
        {
            expected_reliable_acknowledgements +=
                summary_.resource_manifest_fragmented
                ? summary_.resource_manifest_fragment_count
                : 1;
        }
        const bool netchan_succeeded = summary_.netchan_initialized
            && summary_.netchan_state == "established"
            && summary_.reliable_acked == expected_reliable_acknowledgements
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
                && summary_.serverinfo_acked == 1;
            if (!serverinfo_succeeded)
            {
                return false;
            }
            if (delta_descriptions_enabled_)
            {
                const bool delta_succeeded =
                    summary_.delta_registry_built
                    && summary_.delta_preparation_attempts == 1
                    && summary_.delta_table_count
                        == static_cast<int>(
                            hl::network::
                                kGoldSrcCanonicalDeltaTableCount)
                    && summary_.usercmd_delta_present
                    && summary_.delta_bundle_bytes > 0
                    && summary_.signon_bootstrap_tail_bytes > 0
                    && summary_.signon_bootstrap_bytes
                        == summary_.serverinfo_payload_bytes
                            + summary_.delta_bundle_bytes
                            + summary_.signon_bootstrap_tail_bytes
                    && summary_.delta_descriptions_queued == 1
                    && summary_.delta_descriptions_sent == 1
                    && summary_.delta_descriptions_acked == 1
                    && summary_.signon_bootstrap_queued == 1
                    && summary_.signon_bootstrap_sent == 1
                    && summary_.signon_bootstrap_acked == 1
                    && (!summary_.signon_bootstrap_fragmented
                        || (summary_.signon_bootstrap_fragment_count > 1
                            && summary_
                                .signon_bootstrap_fragment_acknowledged_count
                                == summary_
                                    .signon_bootstrap_fragment_count));
                if (!delta_succeeded)
                {
                    return false;
                }
            }
            if (resource_manifest_enabled_)
            {
                if (legacy_fragmentation_rejection_succeeded)
                {
                    return summary_.resource_manifest_generations == 0
                        && summary_.resource_manifest_queued == 0
                        && summary_.resource_manifest_sent == 0
                        && summary_.resource_manifest_acked == 0
                        && summary_.reliable_pending_bytes == 0
                        && summary_.signon_phase
                            == "awaiting_resource_request";
                }
                const bool resource_manifest_succeeded =
                    summary_.resource_request_deliveries == 1
                    && summary_.resource_manifest_preparation_attempts == 1
                    && summary_.resource_manifest_cached_outcome_reuses == 0
                    && summary_.resource_manifest_context_built
                    && summary_.resource_manifest_generations == 1
                    && summary_.resource_manifest_queued == 1
                    && summary_.resource_manifest_sent == 1
                    && summary_.resource_manifest_acked == 1
                    && (!summary_.resource_manifest_fragmented
                        || (summary_.resource_manifest_fragment_count > 1
                            && summary_
                                .resource_manifest_fragment_acknowledged_count
                                == summary_
                                    .resource_manifest_fragment_count))
                    && summary_.signon_phase
                        == "resource_manifest_acknowledged";
                if (!resource_manifest_succeeded
                    || (!resource_manifest_negative_proof_
                        && !delta_descriptions_negative_proof_))
                {
                    return resource_manifest_succeeded;
                }
                return (!resource_manifest_negative_proof_
                        || ResourceManifestNegativeEvidenceSatisfied())
                    && (!delta_descriptions_negative_proof_
                        || DeltaDescriptionsNegativeEvidenceSatisfied())
                    && summary_.server_still_responsive;
            }
            const bool serverinfo_phase_succeeded =
                summary_.signon_phase
                    == (delta_descriptions_enabled_
                            ? "signon_bootstrap_acknowledged"
                            : "serverinfo_acknowledged");
            if (!serverinfo_phase_succeeded
                || (!serverinfo_negative_proof_
                    && !delta_descriptions_negative_proof_))
            {
                return serverinfo_phase_succeeded;
            }
            return (!serverinfo_negative_proof_
                    || ServerInfoNegativeEvidenceSatisfied())
                && (!delta_descriptions_negative_proof_
                    || DeltaDescriptionsNegativeEvidenceSatisfied())
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
                    + ",fragment_transfer_started="
                    + std::string(
                        summary_.fragment_transfer_started ? "1" : "0")
                    + ",fragment_transfer_completed="
                    + std::string(
                        summary_.fragment_transfer_completed ? "1" : "0")
                    + ",fragment_transfer_active="
                    + std::string(
                        summary_.fragment_transfer_active ? "1" : "0")
                    + ",fragment_transfer_phase="
                    + summary_.fragment_transfer_phase
                    + ",fragment_count="
                    + std::to_string(summary_.fragment_count)
                    + ",fragment_send_count="
                    + std::to_string(summary_.fragment_send_count)
                    + ",fragment_resend_count="
                    + std::to_string(summary_.fragment_resend_count)
                    + ",fragment_acknowledged_count="
                    + std::to_string(summary_.fragment_acknowledged_count)
                    + ",fragment_transfer_expired="
                    + std::to_string(summary_.fragment_transfer_expired)
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
        if (resource_manifest_enabled_)
        {
            hl::common::Logger::Info(
                hl::common::LogCategory::Summary,
                "goldsrc_resource_manifest_summary: enabled=1,negative_proof="
                    + std::string(
                        resource_manifest_negative_proof_ ? "1" : "0")
                    + ",resource_request_received="
                    + std::to_string(summary_.resource_request_received)
                    + ",resource_request_deliveries="
                    + std::to_string(summary_.resource_request_deliveries)
                    + ",duplicate_resource_requests_suppressed="
                    + std::to_string(
                        summary_.duplicate_resource_requests_suppressed)
                    + ",observed_continuation_received="
                    + std::string(
                        summary_.observed_continuation_received
                            ? "true"
                            : "false")
                    + ",observed_continuation_deliveries="
                    + std::to_string(
                        summary_.observed_continuation_deliveries)
                    + ",close_menus_companions_accepted="
                    + std::to_string(
                        summary_.close_menus_companions_accepted)
                    + ",observed_continuation_incomplete_rejected="
                    + std::string(
                        summary_.observed_continuation_incomplete_rejected
                            ? "true"
                            : "false")
                    + ",observed_continuation_unsupported_rejected="
                    + std::string(
                        summary_.observed_continuation_unsupported_rejected
                            ? "true"
                            : "false")
                    + ",resource_manifest_preparation_attempts="
                    + std::to_string(
                        summary_.resource_manifest_preparation_attempts)
                    + ",resource_manifest_cached_outcome_reuses="
                    + std::to_string(
                        summary_.resource_manifest_cached_outcome_reuses)
                    + ",resource_manifest_context_built="
                    + std::string(
                        summary_.resource_manifest_context_built ? "1" : "0")
                    + ",resource_manifest_generations="
                    + std::to_string(summary_.resource_manifest_generations)
                    + ",resource_manifest_queued="
                    + std::to_string(summary_.resource_manifest_queued)
                    + ",resource_manifest_sent="
                    + std::to_string(summary_.resource_manifest_sent)
                    + ",resource_manifest_resent="
                    + std::to_string(summary_.resource_manifest_resent)
                    + ",resource_manifest_acked="
                    + std::to_string(summary_.resource_manifest_acked)
                    + ",resource_manifest_entry_count="
                    + std::to_string(summary_.resource_manifest_entry_count)
                    + ",resource_manifest_payload_bytes="
                    + std::to_string(summary_.resource_manifest_payload_bytes)
                    + ",resource_manifest_first_carrier_sequence="
                    + std::to_string(
                        summary_.resource_manifest_first_carrier_sequence)
                    + ",resource_manifest_latest_carrier_sequence="
                    + std::to_string(
                        summary_.resource_manifest_latest_carrier_sequence)
                    + ",resource_manifest_send_count="
                    + std::to_string(summary_.resource_manifest_send_count)
                    + ",resource_manifest_retransmitted="
                    + std::string(
                        summary_.resource_manifest_retransmitted
                            ? "true"
                            : "false")
                    + ",resource_manifest_fragmented="
                    + std::string(
                        summary_.resource_manifest_fragmented
                            ? "true"
                            : "false")
                    + ",resource_manifest_fragment_count="
                    + std::to_string(
                        summary_.resource_manifest_fragment_count)
                    + ",resource_manifest_fragment_acknowledged_count="
                    + std::to_string(
                        summary_
                            .resource_manifest_fragment_acknowledged_count)
                    + ",duplicate_request_suppressed="
                    + std::string(
                        summary_.duplicate_resource_request_suppressed
                            ? "true"
                            : "false")
                    + ",oversized_manifest_not_truncated="
                    + std::string(
                        summary_.oversized_manifest_not_truncated
                            ? "true"
                            : "false")
                    + ",requires_fragmentation_reported="
                    + std::string(
                        summary_.requires_fragmentation_reported
                            ? "true"
                            : "false")
                    + ",payload_too_large="
                    + std::string(
                        summary_.payload_too_large ? "true" : "false")
                    + ",final_manifest_ack="
                    + std::string(
                        summary_.resource_manifest_acked == 1
                            ? "true"
                            : "false")
                    + ",wrong_reliable_ack_rejected="
                    + std::string(
                        summary_.wrong_reliable_ack_rejected ? "1" : "0")
                    + ",signon_phase=" + summary_.signon_phase
                    + ",session_count=" + std::to_string(summary_.session_count)
                    + ",lifecycle_state=" + summary_.final_state
                    + ",netchan_state=" + summary_.netchan_state
                    + ",reliable_pending_bytes="
                    + std::to_string(summary_.reliable_pending_bytes)
                    + ",put_in_server="
                    + std::string(summary_.put_in_server ? "1" : "0")
                    + ",spawned=" + std::string(summary_.spawned ? "1" : "0")
                    + ",active=" + std::string(summary_.active ? "1" : "0")
                    + ",server_still_responsive="
                    + std::string(
                        summary_.server_still_responsive ? "true" : "false")
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
        if (delta_descriptions_enabled_)
        {
            hl::common::Logger::Info(
                hl::common::LogCategory::Summary,
                "goldsrc_delta_description_summary: enabled=1,negative_proof="
                    + std::string(
                        delta_descriptions_negative_proof_ ? "1" : "0")
                    + ",source=" + summary_.delta_source
                    + ",registry_built="
                    + std::string(
                        summary_.delta_registry_built ? "true" : "false")
                    + ",preparation_attempts="
                    + std::to_string(
                        summary_.delta_preparation_attempts)
                    + ",cached_outcome_reuses="
                    + std::to_string(
                        summary_.delta_cached_outcome_reuses)
                    + ",table_count="
                    + std::to_string(summary_.delta_table_count)
                    + ",field_count="
                    + std::to_string(summary_.delta_field_count)
                    + ",usercmd_table_present="
                    + std::string(
                        summary_.usercmd_delta_present ? "true" : "false")
                    + ",delta_bundle_bytes="
                    + std::to_string(summary_.delta_bundle_bytes)
                    + ",bootstrap_tail_bytes="
                    + std::to_string(
                        summary_.signon_bootstrap_tail_bytes)
                    + ",bootstrap_bytes="
                    + std::to_string(summary_.signon_bootstrap_bytes)
                    + ",delta_queued="
                    + std::to_string(
                        summary_.delta_descriptions_queued)
                    + ",delta_sent="
                    + std::to_string(
                        summary_.delta_descriptions_sent)
                    + ",delta_acked="
                    + std::to_string(
                        summary_.delta_descriptions_acked)
                    + ",bootstrap_queued="
                    + std::to_string(summary_.signon_bootstrap_queued)
                    + ",bootstrap_sent="
                    + std::to_string(summary_.signon_bootstrap_sent)
                    + ",bootstrap_resent="
                    + std::to_string(summary_.signon_bootstrap_resent)
                    + ",bootstrap_acked="
                    + std::to_string(summary_.signon_bootstrap_acked)
                    + ",first_carrier_sequence="
                    + std::to_string(
                        summary_
                            .signon_bootstrap_first_carrier_sequence)
                    + ",latest_carrier_sequence="
                    + std::to_string(
                        summary_
                            .signon_bootstrap_latest_carrier_sequence)
                    + ",carrier_send_count="
                    + std::to_string(
                        summary_.signon_bootstrap_send_count)
                    + ",fragmented="
                    + std::string(
                        summary_.signon_bootstrap_fragmented
                            ? "true"
                            : "false")
                    + ",fragment_count="
                    + std::to_string(
                        summary_.signon_bootstrap_fragment_count)
                    + ",fragment_acknowledged_count="
                    + std::to_string(
                        summary_
                            .signon_bootstrap_fragment_acknowledged_count)
                    + ",slot_resets="
                    + std::to_string(summary_.delta_slot_resets)
                    + ",pending_state_cleared_on_reset="
                    + std::string(
                        summary_.delta_pending_state_cleared_on_reset
                            ? "true"
                            : "false")
                    + ",fresh_sessions_after_reset="
                    + std::to_string(
                        summary_.delta_fresh_sessions_after_reset)
                    + ",slot_reused_after_reset="
                    + std::string(
                        summary_.delta_slot_reused_after_reset
                            ? "true"
                            : "false")
                    + ",signon_phase=" + summary_.signon_phase
                    + ",session_count="
                    + std::to_string(summary_.session_count)
                    + ",put_in_server="
                    + std::string(
                        summary_.put_in_server ? "1" : "0")
                    + ",spawned="
                    + std::string(summary_.spawned ? "1" : "0")
                    + ",active="
                    + std::string(summary_.active ? "1" : "0")
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
        summary_.last_reject_reason = accepted_ && resource_manifest_enabled_
            ? "resource-manifest-timeout"
            : accepted_ && delta_descriptions_enabled_
            ? "delta-descriptions-timeout"
            : accepted_ && serverinfo_enabled_
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
        const hl::network::GoldSrcFragmentSender& fragment_sender =
            netchan.fragment_sender();
        const hl::network::GoldSrcFragmentTransferDiagnostics&
            fragment_diagnostics = fragment_sender.diagnostics();
        summary_.fragment_transfer_started =
            fragment_diagnostics.transfers_planned > 0u;
        summary_.fragment_transfer_completed =
            fragment_diagnostics.transfers_completed > 0u;
        summary_.fragment_transfer_active = fragment_sender.active();
        summary_.fragment_transfer_phase =
            std::string(hl::network::NameFor(fragment_sender.phase()));
        summary_.fragment_count =
            static_cast<int>(fragment_sender.fragment_count());
        summary_.fragment_send_count =
            static_cast<int>(fragment_sender.send_count());
        summary_.fragment_resend_count =
            static_cast<int>(fragment_sender.resend_count());
        summary_.fragment_acknowledged_count =
            static_cast<int>(
                fragment_diagnostics.fragments_acknowledged);
        summary_.fragment_transfer_expired =
            static_cast<int>(fragment_diagnostics.transfers_expired);
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
            summary_.delta_descriptions_queued = static_cast<int>(
                signon_diagnostics.delta_descriptions_queued);
            summary_.delta_descriptions_sent = static_cast<int>(
                signon_diagnostics.delta_descriptions_sent);
            summary_.delta_descriptions_acked = static_cast<int>(
                signon_diagnostics.delta_descriptions_acknowledged);
            summary_.signon_bootstrap_queued = static_cast<int>(
                signon_diagnostics.signon_bootstrap_queued);
            summary_.signon_bootstrap_sent = static_cast<int>(
                signon_diagnostics.signon_bootstrap_sent);
            summary_.signon_bootstrap_acked = static_cast<int>(
                signon_diagnostics.signon_bootstrap_acknowledged);
            summary_.duplicate_client_reliable_suppressed =
                signon_diagnostics.duplicate_new_suppressed > 0u;
            summary_.resource_request_received = static_cast<int>(
                signon_diagnostics.resource_request_received);
            summary_.resource_request_deliveries = static_cast<int>(
                signon_diagnostics.resource_request_delivered);
            summary_.duplicate_resource_requests_suppressed =
                static_cast<int>(
                    signon_diagnostics
                        .duplicate_resource_request_suppressed);
            summary_.resource_manifest_queued = static_cast<int>(
                signon_diagnostics.resource_manifest_queued);
            summary_.resource_manifest_acked = static_cast<int>(
                signon_diagnostics.resource_manifest_acknowledged);
            summary_.duplicate_resource_request_suppressed =
                signon_diagnostics
                    .duplicate_resource_request_suppressed > 0u;
            summary_.signon_phase = std::string(
                hl::network::NameFor(slot_state.goldsrc_signon.phase()));
        }
    }

    void ResetResourceManifestSessionSummary() noexcept
    {
        summary_.resource_request_received = 0;
        summary_.resource_request_deliveries = 0;
        summary_.duplicate_resource_requests_suppressed = 0;
        summary_.observed_continuation_received = false;
        summary_.observed_continuation_deliveries = 0;
        summary_.close_menus_companions_accepted = 0;
        summary_.observed_continuation_incomplete_rejected = false;
        summary_.observed_continuation_unsupported_rejected = false;
        summary_.resource_manifest_preparation_attempts = 0;
        summary_.resource_manifest_cached_outcome_reuses = 0;
        summary_.resource_manifest_context_built = false;
        summary_.resource_manifest_generations = 0;
        summary_.resource_manifest_queued = 0;
        summary_.resource_manifest_sent = 0;
        summary_.resource_manifest_resent = 0;
        summary_.resource_manifest_acked = 0;
        summary_.resource_manifest_entry_count = 0;
        summary_.resource_manifest_payload_bytes = 0;
        summary_.resource_manifest_first_carrier_sequence = 0u;
        summary_.resource_manifest_latest_carrier_sequence = 0u;
        summary_.resource_manifest_send_count = 0;
        summary_.resource_manifest_retransmitted = false;
        summary_.resource_manifest_fragmented = false;
        summary_.resource_manifest_fragment_count = 0;
        summary_.resource_manifest_fragment_acknowledged_count = 0;
        summary_.duplicate_resource_request_suppressed = false;
        summary_.fragment_transfer_started = false;
        summary_.fragment_transfer_completed = false;
        summary_.fragment_transfer_active = false;
        summary_.fragment_transfer_phase = "none";
        summary_.fragment_count = 0;
        summary_.fragment_send_count = 0;
        summary_.fragment_resend_count = 0;
        summary_.fragment_acknowledged_count = 0;
        summary_.fragment_transfer_expired = 0;
        summary_.payload_too_large = false;
        summary_.requires_fragmentation_reported = false;
        summary_.oversized_manifest_not_truncated = false;
        summary_.wrong_reliable_ack_rejected = false;
    }

    void ResetDeltaDescriptionSessionSummary() noexcept
    {
        summary_.delta_registry_built = false;
        summary_.delta_source = "none";
        summary_.delta_preparation_attempts = 0;
        summary_.delta_cached_outcome_reuses = 0;
        summary_.delta_table_count = 0;
        summary_.delta_field_count = 0;
        summary_.usercmd_delta_present = false;
        summary_.delta_bundle_bytes = 0;
        summary_.signon_bootstrap_tail_bytes = 0;
        summary_.signon_bootstrap_bytes = 0;
        summary_.delta_descriptions_queued = 0;
        summary_.delta_descriptions_sent = 0;
        summary_.delta_descriptions_acked = 0;
        summary_.signon_bootstrap_queued = 0;
        summary_.signon_bootstrap_sent = 0;
        summary_.signon_bootstrap_resent = 0;
        summary_.signon_bootstrap_acked = 0;
        summary_.signon_bootstrap_first_carrier_sequence = 0u;
        summary_.signon_bootstrap_latest_carrier_sequence = 0u;
        summary_.signon_bootstrap_send_count = 0;
        summary_.signon_bootstrap_fragmented = false;
        summary_.signon_bootstrap_fragment_count = 0;
        summary_.signon_bootstrap_fragment_acknowledged_count = 0;
    }

    void ResetServerInfoSessionSummary() noexcept
    {
        summary_.serverinfo_context_built = false;
        summary_.serverinfo_generations = 0;
        summary_.serverinfo_queued = 0;
        summary_.serverinfo_sent = 0;
        summary_.serverinfo_resent = 0;
        summary_.serverinfo_acked = 0;
        summary_.serverinfo_first_carrier_sequence = 0u;
        summary_.serverinfo_latest_carrier_sequence = 0u;
        summary_.serverinfo_send_count = 0;
        summary_.serverinfo_protocol = 0;
        summary_.serverinfo_spawn_count = 0u;
        summary_.serverinfo_checksum = "none";
        summary_.serverinfo_client_dll_md5 = "none";
        summary_.serverinfo_maxclients = 0;
        summary_.serverinfo_player_index = -1;
        summary_.serverinfo_deathmatch = false;
        summary_.serverinfo_game_dir = "none";
        summary_.serverinfo_hostname = "none";
        summary_.serverinfo_map = "none";
        summary_.serverinfo_mapcycle = "none";
        summary_.serverinfo_secure = false;
        summary_.serverinfo_fallback_dir = "none";
        summary_.serverinfo_allow_cheats = false;
        summary_.serverinfo_payload_bytes = 0;
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

    bool PrepareDeltaDescriptions(DedicatedPlayerRuntimeSlot& slot_state)
    {
        if (slot_state.goldsrc_delta_registry.has_value()
            && slot_state.goldsrc_delta_bundle.has_value()
            && slot_state.goldsrc_signon_bootstrap_size != 0u)
        {
            ++summary_.delta_cached_outcome_reuses;
            return true;
        }
        if (!slot_state.goldsrc_serverinfo_payload.has_value())
        {
            return FailTransport(
                "delta-serverinfo-payload-missing",
                "goldsrc_delta_description_failed: reason=serverinfo-payload-missing");
        }

        ++summary_.delta_preparation_attempts;
        const std::filesystem::path definition_path =
            delta_descriptions_fixture_.has_value()
            ? *delta_descriptions_fixture_
            : state_.server_state.game_directory / L"delta.lst";
        summary_.delta_source = delta_descriptions_fixture_.has_value()
            ? "synthetic_fixture"
            : "runtime_game_dir";

        std::error_code file_error;
        const std::uintmax_t file_size =
            std::filesystem::file_size(definition_path, file_error);
        if (file_error)
        {
            return FailTransport(
                "delta-definition-missing",
                "goldsrc_delta_description_failed: reason=definition-missing");
        }
        if (file_size == 0u
            || file_size
                > hl::network::kGoldSrcMaximumDeltaDefinitionBytes)
        {
            return FailTransport(
                "delta-definition-size-invalid",
                "goldsrc_delta_description_failed: reason=definition-size-invalid");
        }

        std::ifstream input(definition_path, std::ios::binary);
        if (!input)
        {
            return FailTransport(
                "delta-definition-open-failed",
                "goldsrc_delta_description_failed: reason=definition-open-failed");
        }
        std::string definitions(
            static_cast<std::size_t>(file_size),
            '\0');
        if (!input.read(
                definitions.data(),
                static_cast<std::streamsize>(definitions.size()))
            || input.peek() != std::char_traits<char>::eof())
        {
            return FailTransport(
                "delta-definition-read-failed",
                "goldsrc_delta_description_failed: reason=definition-read-failed");
        }

        const hl::network::GoldSrcDeltaLayoutRegistry layouts =
            hl::game_api::detail::BuildGoldSrcProtocol48DeltaLayouts();
        hl::network::GoldSrcDeltaParseResult parsed =
            hl::network::ParseGoldSrcDeltaDefinitions(definitions, layouts);
        if (!parsed.ok())
        {
            return FailTransport(
                "delta-definition-parse-failed",
                "goldsrc_delta_description_failed: reason="
                    + std::string(
                        hl::network::ReasonFor(parsed.status))
                    + ",offset="
                    + std::to_string(parsed.error_offset));
        }

        std::size_t field_count = 0u;
        bool usercmd_present = false;
        for (const hl::network::GoldSrcDeltaTable& table :
             parsed.registry.tables)
        {
            field_count += table.fields.size();
            if (table.name == "usercmd_t" && !table.fields.empty())
            {
                usercmd_present = true;
            }
        }
        if (!usercmd_present)
        {
            return FailTransport(
                "delta-usercmd-table-missing",
                "goldsrc_delta_description_failed: reason=missing-usercmd-table");
        }

        hl::network::GoldSrcDeltaBundleEncodeResult encoded =
            hl::network::EncodeGoldSrcCanonicalDeltaBundle(parsed.registry);
        if (!encoded.ok())
        {
            return FailTransport(
                "delta-bundle-encode-failed",
                "goldsrc_delta_description_failed: reason="
                    + std::string(
                        hl::network::ReasonFor(encoded.status)));
        }

        const auto move_variable =
            [this](std::string_view name, float fallback)
            {
                const std::optional<hl::game_api::CvarSnapshot> snapshot =
                    state_.cvar_registry.SnapshotOf(name);
                return snapshot.has_value() ? snapshot->value : fallback;
            };
        hl::network::GoldSrcBootstrapTailContext tail_context{};
        tail_context.gravity = move_variable("sv_gravity", 800.0f);
        tail_context.stop_speed = move_variable("sv_stopspeed", 100.0f);
        tail_context.maximum_speed = move_variable("sv_maxspeed", 320.0f);
        tail_context.spectator_maximum_speed =
            move_variable("sv_spectatormaxspeed", 500.0f);
        tail_context.accelerate = move_variable("sv_accelerate", 10.0f);
        tail_context.air_accelerate =
            move_variable("sv_airaccelerate", 10.0f);
        tail_context.water_accelerate =
            move_variable("sv_wateraccelerate", 10.0f);
        tail_context.friction = move_variable("sv_friction", 4.0f);
        tail_context.edge_friction =
            move_variable("edgefriction", 2.0f);
        tail_context.water_friction =
            move_variable("sv_waterfriction", 1.0f);
        tail_context.entity_gravity = 1.0f;
        tail_context.bounce = move_variable("sv_bounce", 1.0f);
        tail_context.step_size = move_variable("sv_stepsize", 18.0f);
        tail_context.maximum_velocity =
            move_variable("sv_maxvelocity", 2000.0f);
        tail_context.z_maximum = move_variable("sv_zmax", 4096.0f);
        tail_context.wave_height = move_variable("sv_wateramp", 0.0f);
        tail_context.footsteps =
            move_variable("mp_footsteps", 1.0f) != 0.0f;
        tail_context.roll_angle = move_variable("sv_rollangle", 0.0f);
        tail_context.roll_speed = move_variable("sv_rollspeed", 0.0f);
        tail_context.sky_color_red =
            move_variable("sv_skycolor_r", 0.0f);
        tail_context.sky_color_green =
            move_variable("sv_skycolor_g", 0.0f);
        tail_context.sky_color_blue =
            move_variable("sv_skycolor_b", 0.0f);
        tail_context.sky_vector_x = move_variable("sv_skyvec_x", 0.0f);
        tail_context.sky_vector_y = move_variable("sv_skyvec_y", 0.0f);
        tail_context.sky_vector_z = move_variable("sv_skyvec_z", 0.0f);
        if (const std::optional<hl::game_api::CvarSnapshot> sky_name =
                state_.cvar_registry.SnapshotOf("sv_skyname");
            sky_name.has_value() && !sky_name->string_value.empty())
        {
            tail_context.sky_name = sky_name->string_value;
        }
        else
        {
            tail_context.sky_name = "desert";
        }
        if (state_.globalvars.cdAudioTrack < 0
            || state_.globalvars.cdAudioTrack
                > std::numeric_limits<std::uint8_t>::max()
            || slot_state.slot <= 0
            || slot_state.slot
                > std::numeric_limits<std::uint16_t>::max())
        {
            return FailTransport(
                "delta-bootstrap-tail-context-invalid",
                "goldsrc_delta_description_failed: reason=bootstrap-tail-context-invalid");
        }
        tail_context.cd_audio_track =
            static_cast<std::uint8_t>(state_.globalvars.cdAudioTrack);
        tail_context.view_entity =
            static_cast<std::uint16_t>(slot_state.slot);
        const hl::network::GoldSrcBootstrapTailEncodeResult tail =
            hl::network::EncodeGoldSrcBootstrapTail(tail_context);
        if (!tail.ok())
        {
            return FailTransport(
                "delta-bootstrap-tail-encode-failed",
                "goldsrc_delta_description_failed: reason="
                    + std::string(hl::network::ReasonFor(tail.status)));
        }
        const std::size_t combined_size =
            slot_state.goldsrc_serverinfo_payload->size
            + encoded.payload.size
            + tail.payload.size;
        if (combined_size
            > slot_state.goldsrc_signon_bootstrap.size())
        {
            return FailTransport(
                "delta-bootstrap-capacity-exceeded",
                "goldsrc_delta_description_failed: reason=bootstrap-capacity-exceeded");
        }

        slot_state.goldsrc_signon_bootstrap.fill(0u);
        std::copy_n(
            slot_state.goldsrc_serverinfo_payload->bytes.begin(),
            slot_state.goldsrc_serverinfo_payload->size,
            slot_state.goldsrc_signon_bootstrap.begin());
        std::copy_n(
            encoded.payload.bytes.begin(),
            encoded.payload.size,
            slot_state.goldsrc_signon_bootstrap.begin()
                + slot_state.goldsrc_serverinfo_payload->size);
        std::copy_n(
            tail.payload.bytes.begin(),
            tail.payload.size,
            slot_state.goldsrc_signon_bootstrap.begin()
                + slot_state.goldsrc_serverinfo_payload->size
                + encoded.payload.size);
        slot_state.goldsrc_signon_bootstrap_size = combined_size;
        slot_state.goldsrc_delta_registry = std::move(parsed.registry);
        slot_state.goldsrc_delta_bundle = std::move(encoded.payload);

        summary_.delta_registry_built = true;
        summary_.delta_table_count = static_cast<int>(
            slot_state.goldsrc_delta_registry->tables.size());
        summary_.delta_field_count = static_cast<int>(field_count);
        summary_.usercmd_delta_present = true;
        summary_.delta_bundle_bytes = static_cast<int>(
            slot_state.goldsrc_delta_bundle->size);
        summary_.signon_bootstrap_tail_bytes =
            static_cast<int>(tail.payload.size);
        summary_.signon_bootstrap_bytes =
            static_cast<int>(combined_size);
        hl::common::Logger::Info(
            hl::common::LogCategory::Server,
            "goldsrc_delta_description_built: session_id="
                + slot_state.session_id
                + ",source=" + summary_.delta_source
                + ",tables="
                + std::to_string(summary_.delta_table_count)
                + ",fields="
                + std::to_string(summary_.delta_field_count)
                + ",usercmd=present,delta_bytes="
                + std::to_string(summary_.delta_bundle_bytes)
                + ",tail_bytes="
                + std::to_string(summary_.signon_bootstrap_tail_bytes)
                + ",bootstrap_bytes="
                + std::to_string(summary_.signon_bootstrap_bytes));
        return true;
    }

    enum class ResourceManifestPrepareResult
    {
        kReady,
        kRequiresFragmentation,
        kFailed,
    };

    static bool ParseFixtureUnsigned(
        std::string_view text,
        std::uint32_t* value) noexcept
    {
        if (value == nullptr || text.empty())
        {
            return false;
        }
        std::uint32_t parsed = 0;
        const char* const begin = text.data();
        const char* const end = begin + text.size();
        const std::from_chars_result result =
            std::from_chars(begin, end, parsed, 10);
        if (result.ec != std::errc{} || result.ptr != end)
        {
            return false;
        }
        *value = parsed;
        return true;
    }

    static bool FixtureResourceType(
        std::string_view name,
        hl::network::GoldSrcResourceType* type) noexcept
    {
        if (type == nullptr)
        {
            return false;
        }
        if (name == "generic")
        {
            *type = hl::network::GoldSrcResourceType::kGeneric;
            return true;
        }
        if (name == "sound")
        {
            *type = hl::network::GoldSrcResourceType::kSound;
            return true;
        }
        if (name == "model")
        {
            *type = hl::network::GoldSrcResourceType::kModel;
            return true;
        }
        if (name == "decal")
        {
            *type = hl::network::GoldSrcResourceType::kDecal;
            return true;
        }
        if (name == "event")
        {
            *type = hl::network::GoldSrcResourceType::kEvent;
            return true;
        }
        return false;
    }

    bool AppendResourceManifestEntry(
        hl::network::GoldSrcResourceManifest* manifest,
        hl::network::GoldSrcResourceType type,
        std::uint32_t index,
        std::uintmax_t download_size,
        std::uint32_t flags,
        std::string_view path,
        std::string* failure_reason) const
    {
        if (manifest == nullptr || failure_reason == nullptr)
        {
            return false;
        }
        if (index > hl::network::kGoldSrcMaximumResourceIndex)
        {
            *failure_reason = "resource-index-out-of-range";
            return false;
        }
        if (download_size
            > hl::network::kGoldSrcMaximumResourceDownloadBytes)
        {
            *failure_reason = "resource-download-size-out-of-range";
            return false;
        }
        if (flags > hl::network::kGoldSrcMaximumResourceFlags)
        {
            *failure_reason = "resource-flags-out-of-range";
            return false;
        }
        const hl::network::GoldSrcNormalizedResourcePath normalized =
            hl::network::NormalizeGoldSrcResourcePath(path);
        if (!normalized.ok())
        {
            *failure_reason = std::string(
                hl::network::ReasonFor(normalized.status));
            return false;
        }

        hl::network::GoldSrcResourceEntry entry;
        entry.type = type;
        entry.index = static_cast<std::uint16_t>(index);
        entry.download_size = static_cast<std::uint32_t>(download_size);
        entry.flags = static_cast<std::uint8_t>(flags);
        entry.path = normalized.value;
        manifest->entries.push_back(std::move(entry));
        return true;
    }

    static bool NormalizeRuntimeResourcePath(
        std::string_view path,
        std::string* normalized_path,
        std::string* failure_reason)
    {
        if (normalized_path == nullptr || failure_reason == nullptr)
        {
            return false;
        }
        const hl::network::GoldSrcNormalizedResourcePath normalized =
            hl::network::NormalizeGoldSrcResourcePath(path);
        if (!normalized.ok())
        {
            *failure_reason = std::string(
                hl::network::ReasonFor(normalized.status));
            return false;
        }
        *normalized_path = normalized.value;
        return true;
    }

    static std::uintmax_t ResourceFileSize(
        const std::filesystem::path& path) noexcept
    {
        if (path.empty())
        {
            return 0u;
        }
        std::error_code error;
        const std::uintmax_t size = std::filesystem::file_size(path, error);
        return error ? 0u : size;
    }

    bool BuildResourceManifestFromFixture(
        hl::network::GoldSrcResourceManifest* manifest,
        std::string* failure_reason) const
    {
        if (manifest == nullptr || failure_reason == nullptr
            || !resource_manifest_fixture_.has_value())
        {
            return false;
        }
        std::ifstream fixture(*resource_manifest_fixture_);
        if (!fixture.is_open())
        {
            *failure_reason = "fixture-open-failed";
            return false;
        }

        std::string line;
        while (std::getline(fixture, line))
        {
            if (line.empty() || line[0] == '#')
            {
                continue;
            }
            if (line.size() > 4096u)
            {
                *failure_reason = "fixture-line-too-long";
                return false;
            }
            if (std::count(line.begin(), line.end(), '\t') != 4)
            {
                *failure_reason = "fixture-field-count-invalid";
                return false;
            }

            std::array<std::string_view, 5> fields{};
            std::size_t field_count = 0u;
            std::size_t field_begin = 0u;
            while (field_count < fields.size())
            {
                const std::size_t separator = line.find('\t', field_begin);
                const std::size_t field_end = separator == std::string::npos
                    ? line.size()
                    : separator;
                fields[field_count++] = std::string_view(
                    line.data() + field_begin,
                    field_end - field_begin);
                if (separator == std::string::npos)
                {
                    break;
                }
                field_begin = separator + 1u;
            }
            if (field_count != fields.size())
            {
                *failure_reason = "fixture-field-count-invalid";
                return false;
            }

            hl::network::GoldSrcResourceType type{};
            std::uint32_t index = 0;
            std::uint32_t download_size = 0;
            std::uint32_t flags = 0;
            if (!FixtureResourceType(fields[0], &type)
                || !ParseFixtureUnsigned(fields[1], &index)
                || !ParseFixtureUnsigned(fields[2], &download_size)
                || !ParseFixtureUnsigned(fields[3], &flags))
            {
                *failure_reason = "fixture-field-invalid";
                return false;
            }
            if (!AppendResourceManifestEntry(
                    manifest,
                    type,
                    index,
                    download_size,
                    flags,
                    fields[4],
                    failure_reason))
            {
                return false;
            }
            if (manifest->entries.size()
                > hl::network::kGoldSrcMaximumResourceCount)
            {
                *failure_reason = "resource-count-exceeded";
                return false;
            }
        }
        if (!fixture.eof())
        {
            *failure_reason = "fixture-read-failed";
            return false;
        }
        if (manifest->entries.empty())
        {
            *failure_reason = "fixture-empty";
            return false;
        }
        return true;
    }

    bool BuildResourceManifestFromRuntime(
        hl::network::GoldSrcResourceManifest* manifest,
        std::string* failure_reason) const
    {
        if (manifest == nullptr || failure_reason == nullptr)
        {
            return false;
        }

        const std::filesystem::path& game_directory =
            state_.server_state.game_directory;
        for (std::size_t offset = 0;
             offset < state_.generic_precache_registry.Size();
             ++offset)
        {
            const int index = static_cast<int>(offset + 1u);
            const std::string* path =
                state_.generic_precache_registry.ValueOf(index);
            if (path == nullptr)
            {
                *failure_reason = "generic-registry-gap";
                return false;
            }
            std::string normalized_path;
            if (!NormalizeRuntimeResourcePath(
                    *path,
                    &normalized_path,
                    failure_reason)
                || !AppendResourceManifestEntry(
                    manifest,
                    hl::network::GoldSrcResourceType::kGeneric,
                    static_cast<std::uint32_t>(index),
                    ResourceFileSize(game_directory / normalized_path),
                    hl::network::kGoldSrcResourceFatalIfMissingFlag,
                    normalized_path,
                    failure_reason))
            {
                return false;
            }
        }

        for (std::size_t offset = 0;
             offset < state_.sound_precache_registry.RegistrySize();
             ++offset)
        {
            const int index = static_cast<int>(offset + 1u);
            const hl::game_api::detail::SoundPrecacheEntry* entry =
                state_.sound_precache_registry.EntryByIndex(index);
            if (entry == nullptr)
            {
                *failure_reason = "sound-registry-gap";
                return false;
            }
            std::string normalized_path;
            if (!NormalizeRuntimeResourcePath(
                    entry->normalized_path,
                    &normalized_path,
                    failure_reason)
                || !AppendResourceManifestEntry(
                    manifest,
                    hl::network::GoldSrcResourceType::kSound,
                    static_cast<std::uint32_t>(index),
                    ResourceFileSize(
                        game_directory / "sound" / normalized_path),
                    0u,
                    normalized_path,
                    failure_reason))
            {
                return false;
            }
        }

        for (std::size_t offset = 0;
             offset < state_.precache_registry.ModelCount();
             ++offset)
        {
            const int index = static_cast<int>(offset + 1u);
            const std::string* path = state_.precache_registry.ModelName(index);
            if (path == nullptr)
            {
                *failure_reason = "model-registry-gap";
                return false;
            }
            std::string normalized_path;
            if (!NormalizeRuntimeResourcePath(
                    *path,
                    &normalized_path,
                    failure_reason))
            {
                return false;
            }
            const std::uintmax_t size = index == 1
                ? state_.world_context.bsp_file_size
                : (!normalized_path.empty() && normalized_path[0] == '*'
                    ? 0u
                    : ResourceFileSize(game_directory / normalized_path));
            if (!AppendResourceManifestEntry(
                    manifest,
                    hl::network::GoldSrcResourceType::kModel,
                    static_cast<std::uint32_t>(index),
                    size,
                    0u,
                    normalized_path,
                    failure_reason))
            {
                return false;
            }
        }

        for (std::size_t offset = 0;
             offset < state_.decal_registry.Size();
             ++offset)
        {
            const int registry_index = static_cast<int>(offset + 1u);
            const std::string* name =
                state_.decal_registry.ValueOf(registry_index);
            if (name == nullptr)
            {
                *failure_reason = "decal-registry-gap";
                return false;
            }
            std::string normalized_name;
            if (!NormalizeRuntimeResourcePath(
                    *name,
                    &normalized_name,
                    failure_reason)
                || !AppendResourceManifestEntry(
                    manifest,
                    hl::network::GoldSrcResourceType::kDecal,
                    static_cast<std::uint32_t>(offset),
                    0u,
                    0u,
                    normalized_name,
                    failure_reason))
            {
                return false;
            }
        }

        for (std::size_t offset = 0;
             offset < state_.event_precache_registry.Size();
             ++offset)
        {
            const int index = static_cast<int>(offset + 1u);
            const std::string* value =
                state_.event_precache_registry.ValueOf(index);
            if (value == nullptr)
            {
                *failure_reason = "event-registry-gap";
                return false;
            }
            const std::size_t separator = value->find(':');
            if (separator == std::string::npos
                || separator + 1u >= value->size())
            {
                *failure_reason = "event-registry-value-invalid";
                return false;
            }
            const std::string_view path(
                value->data() + separator + 1u,
                value->size() - separator - 1u);
            std::string normalized_path;
            if (!NormalizeRuntimeResourcePath(
                    path,
                    &normalized_path,
                    failure_reason)
                || !AppendResourceManifestEntry(
                    manifest,
                    hl::network::GoldSrcResourceType::kEvent,
                    static_cast<std::uint32_t>(index),
                    ResourceFileSize(game_directory / normalized_path),
                    hl::network::kGoldSrcResourceFatalIfMissingFlag,
                    normalized_path,
                    failure_reason))
            {
                return false;
            }
        }
        return true;
    }

    ResourceManifestPrepareResult PrepareResourceManifestContext(
        DedicatedPlayerRuntimeSlot& slot_state)
    {
        using PreparationState =
            hl::network::GoldSrcResourceManifestPreparationState;
        const PreparationState preparation_state =
            slot_state.goldsrc_resource_manifest_preparation.state();
        if (preparation_state == PreparationState::kRequiresFragmentation)
        {
            ++summary_.resource_manifest_cached_outcome_reuses;
            return ResourceManifestPrepareResult::kRequiresFragmentation;
        }
        if (preparation_state == PreparationState::kReady)
        {
            if (!slot_state.goldsrc_resource_manifest.has_value()
                || slot_state.goldsrc_resource_manifest_response_size == 0u)
            {
                FailTransport(
                    "resource-manifest-preparation-cache-inconsistent",
                    "goldsrc_resource_manifest_context_failed: reason=ready-cache-missing-payload");
                return ResourceManifestPrepareResult::kFailed;
            }
            return ResourceManifestPrepareResult::kReady;
        }
        if (slot_state.goldsrc_resource_manifest.has_value()
            || slot_state.goldsrc_resource_manifest_response_size != 0u)
        {
            FailTransport(
                "resource-manifest-preparation-cache-inconsistent",
                "goldsrc_resource_manifest_context_failed: reason=unrecorded-payload");
            return ResourceManifestPrepareResult::kFailed;
        }

        ++summary_.resource_manifest_preparation_attempts;

        hl::network::GoldSrcResourceManifest manifest;
        std::string failure_reason;
        const hl::network::GoldSrcResourceManifestSource manifest_source =
            resource_manifest_fixture_.has_value()
            ? hl::network::GoldSrcResourceManifestSource::kExplicitTestFixture
            : hl::network::GoldSrcResourceManifestSource::
                kAuthoritativeRuntime;
        const bool built =
            hl::network::DispatchGoldSrcResourceManifestBuild(
                manifest_source,
                [&]()
                {
                    return BuildResourceManifestFromRuntime(
                        &manifest,
                        &failure_reason);
                },
                [&]()
                {
                    return BuildResourceManifestFromFixture(
                        &manifest,
                        &failure_reason);
                });
        if (!built)
        {
            FailTransport(
                "resource-manifest-context-invalid",
                "goldsrc_resource_manifest_context_failed: reason="
                    + SanitizeLogField(failure_reason));
            return ResourceManifestPrepareResult::kFailed;
        }

        const hl::network::GoldSrcResourceManifestValidationResult validation =
            hl::network::ValidateGoldSrcResourceManifest(manifest);
        if (!validation.ok()
            && validation.status
                != hl::network::GoldSrcResourceManifestStatus::
                    kRequiresFragmentation)
        {
            FailTransport(
                "resource-manifest-validation-failed",
                "goldsrc_resource_manifest_context_failed: reason="
                    + std::string(
                        hl::network::ReasonFor(validation.status)));
            return ResourceManifestPrepareResult::kFailed;
        }

        const hl::network::GoldSrcResourceRequestEncodeResult companion =
            hl::network::EncodeGoldSrcResourceRequest(
                state_.server_state.map_spawn_count);
        if (!companion.ok())
        {
            FailTransport(
                "resource-manifest-companion-encode-failed",
                "goldsrc_resource_manifest_context_failed: reason="
                    + std::string(
                        hl::network::ReasonFor(companion.status)));
            return ResourceManifestPrepareResult::kFailed;
        }

        const bool preserve_legacy_unfragmented_limit =
            resource_manifest_negative_proof_
            && !resource_manifest_fixture_.has_value();
        const std::size_t response_capacity =
            preserve_legacy_unfragmented_limit
            ? hl::network::kGoldSrcNetchanMaximumReliableBytes
            : hl::network::kGoldSrcMaximumFragmentTransferBytes;
        if (companion.payload.size > response_capacity)
        {
            FailTransport(
                "resource-manifest-response-capacity-invalid",
                "goldsrc_resource_manifest_context_failed: reason=companion-exceeds-response-capacity");
            return ResourceManifestPrepareResult::kFailed;
        }
        const std::size_t list_capacity =
            response_capacity - companion.payload.size;
        const hl::network::GoldSrcResourceManifestEncodeResult encoded =
            hl::network::EncodeGoldSrcResourceManifest(
                manifest,
                list_capacity);
        if (encoded.status
                == hl::network::GoldSrcResourceManifestStatus::
                    kRequiresFragmentation
            || encoded.status
                == hl::network::GoldSrcResourceManifestStatus::
                    kOutputCapacityExceeded)
        {
            if (!slot_state.goldsrc_resource_manifest_preparation.TryStore(
                    PreparationState::kRequiresFragmentation))
            {
                FailTransport(
                    "resource-manifest-preparation-cache-store-failed",
                    "goldsrc_resource_manifest_context_failed: reason=preparation-cache-store-failed");
                return ResourceManifestPrepareResult::kFailed;
            }
            slot_state.goldsrc_resource_manifest.reset();
            slot_state.goldsrc_resource_manifest_response.fill(0u);
            slot_state.goldsrc_resource_manifest_response_size = 0u;
            summary_.requires_fragmentation_reported = true;
            summary_.oversized_manifest_not_truncated = true;
            summary_.payload_too_large =
                companion.payload.size
                    > hl::network::kGoldSrcMaximumFragmentTransferBytes
                || validation.encoded_size
                    > hl::network::kGoldSrcMaximumFragmentTransferBytes
                        - companion.payload.size;
            hl::common::Logger::Warn(
                hl::common::LogCategory::Server,
                "goldsrc_resource_manifest_not_queued: reason=requires_fragmentation"
                ",resource_count="
                    + std::to_string(manifest.entries.size())
                    + ",encoded_bytes="
                    + std::to_string(validation.encoded_size)
                    + ",response_capacity="
                    + std::to_string(response_capacity)
                    + ",payload_too_large="
                    + std::string(summary_.payload_too_large ? "1" : "0")
                    + ",source="
                    + std::string(hl::network::NameFor(manifest_source))
                    + ",signon_phase=" + summary_.signon_phase);
            return ResourceManifestPrepareResult::kRequiresFragmentation;
        }
        if (!encoded.ok())
        {
            FailTransport(
                "resource-manifest-encode-failed",
                "goldsrc_resource_manifest_context_failed: reason="
                    + std::string(hl::network::ReasonFor(encoded.status)));
            return ResourceManifestPrepareResult::kFailed;
        }

        if (!slot_state.goldsrc_resource_manifest_preparation.TryStore(
                PreparationState::kReady))
        {
            FailTransport(
                "resource-manifest-preparation-cache-store-failed",
                "goldsrc_resource_manifest_context_failed: reason=preparation-cache-store-failed");
            return ResourceManifestPrepareResult::kFailed;
        }

        slot_state.goldsrc_resource_manifest_response.fill(0u);
        std::copy_n(
            companion.payload.bytes.begin(),
            companion.payload.size,
            slot_state.goldsrc_resource_manifest_response.begin());
        std::copy_n(
            encoded.payload.bytes.begin(),
            encoded.payload.size,
            slot_state.goldsrc_resource_manifest_response.begin()
                + companion.payload.size);
        slot_state.goldsrc_resource_manifest_response_size =
            companion.payload.size + encoded.payload.size;
        slot_state.goldsrc_resource_manifest = std::move(manifest);

        summary_.resource_manifest_context_built = true;
        ++summary_.resource_manifest_generations;
        summary_.resource_manifest_entry_count = static_cast<int>(
            slot_state.goldsrc_resource_manifest->entries.size());
        summary_.resource_manifest_payload_bytes = static_cast<int>(
            slot_state.goldsrc_resource_manifest_response_size);
        hl::common::Logger::Info(
            hl::common::LogCategory::Server,
            "goldsrc_resource_manifest_context_built: session_id="
                + slot_state.session_id
                + ",resource_count="
                + std::to_string(summary_.resource_manifest_entry_count)
                + ",payload_bytes="
                + std::to_string(summary_.resource_manifest_payload_bytes)
                + ",source="
                + std::string(hl::network::NameFor(manifest_source)));
        return ResourceManifestPrepareResult::kReady;
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

    static std::string DescribeClientPayloadForDiagnostic(
        const hl::network::GoldSrcNetchanPacket& packet)
    {
        constexpr std::size_t kMaximumReportedMessages = 4u;
        constexpr std::size_t kMaximumReportedCommandNameBytes = 32u;
        std::string description;
        std::size_t cursor = 0u;
        std::size_t reported_messages = 0u;

        const auto append_separator = [&description]()
        {
            if (!description.empty())
            {
                description.push_back(',');
            }
        };

        while (cursor < packet.payload_size
            && reported_messages < kMaximumReportedMessages)
        {
            const std::uint8_t opcode = packet.payload[cursor++];
            if (opcode == hl::network::kGoldSrcClientNopOpcode)
            {
                continue;
            }

            append_separator();
            ++reported_messages;
            if (opcode != hl::network::kGoldSrcClientStringCommandOpcode)
            {
                description += "opcode:";
                description += std::to_string(opcode);
                break;
            }

            description += "stringcmd:";
            const std::size_t command_begin = cursor;
            while (cursor < packet.payload_size
                && packet.payload[cursor] != 0u)
            {
                ++cursor;
            }
            if (cursor == packet.payload_size)
            {
                description += "unterminated";
                break;
            }

            std::size_t name_size = 0u;
            while (name_size < cursor - command_begin
                && name_size < kMaximumReportedCommandNameBytes)
            {
                const std::uint8_t byte =
                    packet.payload[command_begin + name_size];
                if (byte == ' ' || byte == '\t' || byte == ';'
                    || byte < 0x20u || byte > 0x7Eu)
                {
                    break;
                }
                description.push_back(static_cast<char>(byte));
                ++name_size;
            }
            if (name_size == 0u)
            {
                description += "<empty>";
            }
            else if (name_size == kMaximumReportedCommandNameBytes
                && name_size < cursor - command_begin)
            {
                description += "...";
            }
            if (name_size < cursor - command_begin)
            {
                description += ":suffix=";
                const std::uint8_t suffix =
                    packet.payload[command_begin + name_size];
                switch (suffix)
                {
                case ' ':
                    description += "argument";
                    break;
                case '\t':
                    description += "tab";
                    break;
                case '\n':
                    description += "lf";
                    break;
                case '\r':
                    description += "cr";
                    break;
                default:
                    description += "non_printing";
                    break;
                }
                if (suffix == ' ')
                {
                    std::size_t argument_cursor =
                        command_begin + name_size;
                    std::size_t space_count = 0u;
                    while (argument_cursor < cursor
                        && packet.payload[argument_cursor] == ' ')
                    {
                        ++argument_cursor;
                        ++space_count;
                    }
                    description += ":spaces=";
                    description += std::to_string(space_count);
                    std::uint32_t decimal_value = 0u;
                    bool decimal_seen = false;
                    bool decimal_bounded = true;
                    while (argument_cursor < cursor
                        && packet.payload[argument_cursor] >= '0'
                        && packet.payload[argument_cursor] <= '9')
                    {
                        decimal_seen = true;
                        const std::uint32_t digit =
                            packet.payload[argument_cursor] - '0';
                        if (decimal_value > 6553u
                            || (decimal_value == 6553u && digit > 5u))
                        {
                            decimal_bounded = false;
                        }
                        if (decimal_bounded)
                        {
                            decimal_value = decimal_value * 10u + digit;
                        }
                        ++argument_cursor;
                    }
                    if (decimal_seen && decimal_bounded)
                    {
                        description += ":u16=";
                        description += std::to_string(decimal_value);
                    }
                    if (argument_cursor < cursor)
                    {
                        if (packet.payload[argument_cursor] == '\n')
                        {
                            description += ":line=lf";
                        }
                        else if (packet.payload[argument_cursor] == '\r')
                        {
                            description += ":line=cr";
                        }
                        else
                        {
                            description += ":tail=opaque";
                        }
                    }
                }
            }
            ++cursor;
        }

        if (description.empty())
        {
            return "nop_only";
        }
        if (cursor < packet.payload_size
            && reported_messages == kMaximumReportedMessages)
        {
            description += ",more";
        }
        return description;
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
                     kUnsupportedCompanionCommand
            || status == hl::network::GoldSrcClientSignonDecodeStatus::
                     kUnsupportedCompanionCount
            || status == hl::network::GoldSrcClientSignonDecodeStatus::
                     kTooManyCompanionCommands
            || status == hl::network::GoldSrcClientSignonDecodeStatus::
                     kUnsupportedTrailingData)
        {
            if (status
                    == hl::network::GoldSrcClientSignonDecodeStatus::
                        kUnsupportedCompanionCommand
                || status
                    == hl::network::GoldSrcClientSignonDecodeStatus::
                        kUnsupportedCompanionCount
                || status
                    == hl::network::GoldSrcClientSignonDecodeStatus::
                        kTooManyCompanionCommands)
            {
                ++summary_.unsupported_command_rejected;
            }
            else if (PayloadLooksLikeCommandInjection(packet)
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
            if (status
                    == hl::network::GoldSrcClientSignonDecodeStatus::
                        kUnsupportedCompanionCommand
                || status
                    == hl::network::GoldSrcClientSignonDecodeStatus::
                        kUnsupportedCompanionCount
                || status
                    == hl::network::GoldSrcClientSignonDecodeStatus::
                        kTooManyCompanionCommands)
            {
                summary_.observed_continuation_unsupported_rejected = true;
            }
        }
        else
        {
            ++summary_.malformed_stringcmd_rejected;
            if (status
                == hl::network::GoldSrcClientSignonDecodeStatus::
                    kMissingStringTerminator)
            {
                summary_.observed_continuation_incomplete_rejected = true;
            }
        }
        hl::common::Logger::Warn(
            hl::common::LogCategory::Server,
            "goldsrc_client_signon_rejected: reason="
                + std::string(hl::network::ReasonFor(status))
                + ",identity="
                + DescribeClientPayloadForDiagnostic(packet));
    }

    bool HandleClientSignonPayload(
        DedicatedPlayerRuntimeSlot& slot_state,
        const hl::network::GoldSrcNetchanPacket& packet,
        hl::network::GoldSrcNetchanReliablePayloadKind* queued_kind)
    {
        if (queued_kind == nullptr)
        {
            return false;
        }
        *queued_kind = hl::network::GoldSrcNetchanReliablePayloadKind::kNone;
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
                        kUnsupportedCompanionCommand
                || decoded.status
                    == hl::network::GoldSrcClientSignonDecodeStatus::
                        kUnsupportedCompanionCount
                || decoded.status
                    == hl::network::GoldSrcClientSignonDecodeStatus::
                        kTooManyCompanionCommands
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

        if (decoded.command
            == hl::network::GoldSrcClientSignonCommand::kDisconnect)
        {
            const int disconnected_slot = slot_state.slot;
            const std::string disconnected_session = slot_state.session_id;
            const bool delta_pending =
                slot_state.goldsrc_signon_bootstrap_size > 0u
                && slot_state.netchan.pending_reliable_kind()
                    == hl::network::GoldSrcNetchanReliablePayloadKind::
                        kSignonBootstrap
                && (slot_state.netchan.reliable_pending()
                    || slot_state.netchan.fragment_sender().active());
            ApplyDedicatedClientDisconnect(state_, slot_state);
            RecordDedicatedLifecycleTransition(
                state_.dedicated_multiplayer_foundation,
                slot_state,
                DedicatedPlayerLifecycleState::kDisconnected,
                "bounded client disconnect during GoldSrc signon");
            const bool state_cleared =
                !slot_state.netchan.initialized()
                && slot_state.goldsrc_signon.phase()
                    == hl::network::GoldSrcSignonPhase::kNone
                && !slot_state.goldsrc_serverinfo_context.has_value()
                && !slot_state.goldsrc_serverinfo_payload.has_value()
                && !slot_state.goldsrc_delta_registry.has_value()
                && !slot_state.goldsrc_delta_bundle.has_value()
                && slot_state.goldsrc_signon_bootstrap_size == 0u
                && !slot_state.goldsrc_resource_manifest.has_value()
                && slot_state.goldsrc_resource_manifest_response_size == 0u;
            ++summary_.delta_slot_resets;
            summary_.delta_pending_state_cleared_on_reset =
                summary_.delta_pending_state_cleared_on_reset
                || (delta_pending && state_cleared);
            delta_reset_slot_ = disconnected_slot;
            awaiting_fresh_session_after_delta_reset_ = true;
            admitted_session_id_.clear();
            negative_advance_pending_ = false;
            summary_.final_state = "disconnected";
            summary_.connected = false;
            summary_.put_in_server = false;
            summary_.spawned = false;
            summary_.active = false;
            SyncNetchanSummary(slot_state);
            hl::common::Logger::Info(
                hl::common::LogCategory::Server,
                "goldsrc_delta_session_reset: session_id="
                    + disconnected_session
                    + ",slot=" + std::to_string(disconnected_slot)
                    + ",pending_before_reset="
                    + std::string(delta_pending ? "true" : "false")
                    + ",state_cleared="
                    + std::string(state_cleared ? "true" : "false")
                    + ",session_count="
                    + std::to_string(summary_.session_count)
                    + ",put_in_server=0,spawned=0,active=0");
            return true;
        }

        if (decoded.command
            == hl::network::GoldSrcClientSignonCommand::kNew)
        {
            if (slot_state.goldsrc_signon.phase()
                != hl::network::GoldSrcSignonPhase::kAwaitingNew)
            {
                const hl::network::GoldSrcSignonCommandDisposition disposition =
                    slot_state.goldsrc_signon.HandleClientCommand(
                        decoded.command);
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
                            + slot_state.session_id
                            + ",reason=already-delivered");
                    return true;
                }
                SyncNetchanSummary(slot_state);
                return true;
            }
            if (!PrepareServerInfoContext(slot_state))
            {
                return false;
            }
            if (delta_descriptions_enabled_
                && !PrepareDeltaDescriptions(slot_state))
            {
                return false;
            }
            if (!slot_state.goldsrc_serverinfo_payload.has_value())
            {
                return FailTransport(
                    "serverinfo-payload-missing",
                    "goldsrc_serverinfo_queue_failed: reason=payload-missing");
            }
            const std::uint8_t* const bootstrap_bytes =
                delta_descriptions_enabled_
                ? slot_state.goldsrc_signon_bootstrap.data()
                : slot_state.goldsrc_serverinfo_payload->bytes.data();
            const std::size_t bootstrap_size =
                delta_descriptions_enabled_
                ? slot_state.goldsrc_signon_bootstrap_size
                : slot_state.goldsrc_serverinfo_payload->size;
            const hl::network::GoldSrcNetchanReliablePayloadKind
                bootstrap_kind = delta_descriptions_enabled_
                ? hl::network::GoldSrcNetchanReliablePayloadKind::
                    kSignonBootstrap
                : hl::network::GoldSrcNetchanReliablePayloadKind::
                    kServerInfo;
            if (bootstrap_size == 0u)
            {
                return FailTransport(
                    "signon-bootstrap-payload-missing",
                    "goldsrc_serverinfo_queue_failed: reason=bootstrap-payload-missing");
            }
            const hl::network::GoldSrcNetchanQueueResult queue_result =
                slot_state.netchan.QueueReliablePayload(
                    bootstrap_bytes,
                    bootstrap_size,
                    bootstrap_kind);
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
            *queued_kind = bootstrap_kind;
            SyncNetchanSummary(slot_state);
            hl::common::Logger::Info(
                hl::common::LogCategory::Server,
                (delta_descriptions_enabled_
                        ? "goldsrc_signon_bootstrap_queued: session_id="
                        : "goldsrc_serverinfo_queued: session_id=")
                    + slot_state.session_id
                    + ",payload_bytes="
                    + std::to_string(bootstrap_size)
                    + ",signon_phase=" + summary_.signon_phase);
            return true;
        }

        if (decoded.command
            != hl::network::GoldSrcClientSignonCommand::kSendResources
            || !resource_manifest_enabled_)
        {
            ++summary_.unsupported_command_rejected;
            return true;
        }

        const bool exact_observed_continuation =
            decoded.companion_command
                == hl::network::GoldSrcClientSignonCompanionCommand::
                    kCloseMenus
            && decoded.companion_count
                == hl::network::kGoldSrcMaximumCloseMenusCompanions;
        if (exact_observed_continuation)
        {
            summary_.observed_continuation_received = true;
        }

        if (slot_state.goldsrc_signon.phase()
            != hl::network::GoldSrcSignonPhase::kAwaitingResourceRequest)
        {
            const hl::network::GoldSrcSignonCommandDisposition disposition =
                slot_state.goldsrc_signon.HandleClientCommand(decoded.command);
            if (disposition
                == hl::network::GoldSrcSignonCommandDisposition::
                    kDuplicateSuppressed)
            {
                ++summary_.inbound_reliable_retransmitted;
                summary_.duplicate_resource_request_suppressed = true;
                SyncNetchanSummary(slot_state);
                hl::common::Logger::Info(
                    hl::common::LogCategory::Server,
                    "goldsrc_resource_request_suppressed: session_id="
                        + slot_state.session_id + ",reason=already-delivered");
                return true;
            }
            SyncNetchanSummary(slot_state);
            return true;
        }

        const bool reusing_cached_fragmentation_outcome =
            slot_state.goldsrc_resource_manifest_preparation.state()
            == hl::network::GoldSrcResourceManifestPreparationState::
                kRequiresFragmentation;
        const ResourceManifestPrepareResult prepare_result =
            PrepareResourceManifestContext(slot_state);
        if (prepare_result == ResourceManifestPrepareResult::kFailed)
        {
            return false;
        }
        if (prepare_result
            == ResourceManifestPrepareResult::kRequiresFragmentation)
        {
            if (reusing_cached_fragmentation_outcome)
            {
                hl::common::Logger::Info(
                    hl::common::LogCategory::Server,
                    "goldsrc_resource_request_suppressed: session_id="
                        + slot_state.session_id
                        + ",reason=requires-fragmentation-cached");
            }
            SyncNetchanSummary(slot_state);
            return true;
        }
        if (!slot_state.goldsrc_resource_manifest.has_value()
            || slot_state.goldsrc_resource_manifest_response_size == 0u)
        {
            return FailTransport(
                "resource-manifest-payload-missing",
                "goldsrc_resource_manifest_queue_failed: reason=payload-missing");
        }
        const hl::network::GoldSrcNetchanQueueResult queue_result =
            slot_state.netchan.QueueReliablePayload(
                slot_state.goldsrc_resource_manifest_response.data(),
                slot_state.goldsrc_resource_manifest_response_size,
                hl::network::GoldSrcNetchanReliablePayloadKind::
                    kResourceManifest);
        if (queue_result != hl::network::GoldSrcNetchanQueueResult::kQueued)
        {
            return FailTransport(
                "resource-manifest-reliable-queue-failed",
                "goldsrc_resource_manifest_queue_failed: reason="
                    + std::string(hl::network::ReasonFor(queue_result)));
        }
        const hl::network::GoldSrcSignonCommandDisposition disposition =
            slot_state.goldsrc_signon.HandleClientCommand(decoded.command);
        if (disposition
            != hl::network::GoldSrcSignonCommandDisposition::kDelivered)
        {
            return FailTransport(
                "resource-manifest-signon-commit-failed",
                "goldsrc_resource_manifest_queue_failed: reason=signon-commit-failed");
        }
        if (exact_observed_continuation)
        {
            ++summary_.observed_continuation_deliveries;
            summary_.close_menus_companions_accepted =
                static_cast<int>(decoded.companion_count);
        }
        *queued_kind =
            hl::network::GoldSrcNetchanReliablePayloadKind::kResourceManifest;
        SyncNetchanSummary(slot_state);
        hl::common::Logger::Info(
            hl::common::LogCategory::Server,
            "goldsrc_resource_manifest_queued: session_id="
                + slot_state.session_id
                + ",payload_bytes="
                + std::to_string(
                    slot_state.goldsrc_resource_manifest_response_size)
                + ",close_menus_companions="
                + std::to_string(decoded.companion_count)
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
            && (pending_kind
                    == hl::network::GoldSrcNetchanReliablePayloadKind::
                        kServerInfo
                || pending_kind
                    == hl::network::GoldSrcNetchanReliablePayloadKind::
                        kSignonBootstrap)
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
            if (pending_kind
                == hl::network::GoldSrcNetchanReliablePayloadKind::
                    kSignonBootstrap)
            {
                if (summary_.signon_bootstrap_send_count == 0)
                {
                    summary_.signon_bootstrap_first_carrier_sequence =
                        decoded.packet.sequence;
                }
                summary_.signon_bootstrap_latest_carrier_sequence =
                    decoded.packet.sequence;
                ++summary_.signon_bootstrap_send_count;
            }
            const hl::network::GoldSrcNetchanDiagnostics& diagnostics =
                slot_state.netchan.diagnostics();
            if (diagnostics.reliable_resent > reliable_resent_before)
            {
                ++summary_.serverinfo_resent;
                if (pending_kind
                    == hl::network::GoldSrcNetchanReliablePayloadKind::
                        kSignonBootstrap)
                {
                    ++summary_.signon_bootstrap_resent;
                }
            }
            else if (diagnostics.reliable_sent > reliable_sent_before
                && (pending_kind
                            == hl::network::
                                GoldSrcNetchanReliablePayloadKind::
                                    kSignonBootstrap
                        ? summary_.signon_bootstrap_sent == 0
                        : summary_.serverinfo_sent == 0))
            {
                ++summary_.serverinfo_sent;
                const hl::network::GoldSrcSignonTransitionResult
                    sent_transition = pending_kind
                            == hl::network::
                                GoldSrcNetchanReliablePayloadKind::
                                    kSignonBootstrap
                        ? slot_state.goldsrc_signon.MarkSignonBootstrapSent()
                        : slot_state.goldsrc_signon.MarkServerInfoSent();
                if (pending_kind
                    == hl::network::GoldSrcNetchanReliablePayloadKind::
                        kSignonBootstrap)
                {
                    ++summary_.signon_bootstrap_sent;
                    const hl::network::GoldSrcFragmentSender&
                        fragment_sender =
                            slot_state.netchan.fragment_sender();
                    summary_.signon_bootstrap_fragmented =
                        fragment_sender.active();
                    summary_.signon_bootstrap_fragment_count =
                        static_cast<int>(
                            fragment_sender.fragment_count());
                }
                if (sent_transition
                    != hl::network::GoldSrcSignonTransitionResult::kAdvanced)
                {
                    return FailTransport(
                        "signon-bootstrap-sent-transition-failed",
                        "goldsrc_signon_bootstrap_send_failed: reason=invalid-signon-phase");
                }
            }
        }
        if (resource_manifest_enabled_
            && pending_kind
                == hl::network::GoldSrcNetchanReliablePayloadKind::
                    kResourceManifest
            && decoded.packet.reliable_present)
        {
            if (summary_.resource_manifest_send_count == 0)
            {
                summary_.resource_manifest_first_carrier_sequence =
                    decoded.packet.sequence;
                const hl::network::GoldSrcFragmentSender&
                    fragment_sender =
                        slot_state.netchan.fragment_sender();
                summary_.resource_manifest_fragmented =
                    fragment_sender.active();
                summary_.resource_manifest_fragment_count =
                    static_cast<int>(fragment_sender.fragment_count());
            }
            summary_.resource_manifest_latest_carrier_sequence =
                decoded.packet.sequence;
            ++summary_.resource_manifest_send_count;
            const hl::network::GoldSrcNetchanDiagnostics& diagnostics =
                slot_state.netchan.diagnostics();
            if (diagnostics.reliable_resent > reliable_resent_before)
            {
                ++summary_.resource_manifest_resent;
                summary_.resource_manifest_retransmitted = true;
            }
            else if (diagnostics.reliable_sent > reliable_sent_before
                && summary_.resource_manifest_sent == 0)
            {
                ++summary_.resource_manifest_sent;
                if (slot_state.goldsrc_signon.MarkResourceManifestSent()
                    != hl::network::GoldSrcSignonTransitionResult::kAdvanced)
                {
                    return FailTransport(
                        "resource-manifest-sent-transition-failed",
                        "goldsrc_resource_manifest_send_failed: reason=invalid-signon-phase");
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
                + ",fragment="
                + (decoded.packet.fragment_present ? "1" : "0")
                + ",payload_bytes="
                + std::to_string(decoded.packet.payload_size)
                + (decoded.packet.fragment_present
                        ? ",fragment_index="
                            + std::to_string(
                                decoded.packet.fragment_metadata
                                    .descriptors[0]
                                    .fragment_index)
                            + ",fragment_count="
                            + std::to_string(
                                decoded.packet.fragment_metadata
                                    .descriptors[0]
                                    .fragment_count)
                        : "")
                + (serverinfo_enabled_
                        && (pending_kind
                                == hl::network::
                                    GoldSrcNetchanReliablePayloadKind::
                                        kServerInfo
                            || pending_kind
                                == hl::network::
                                    GoldSrcNetchanReliablePayloadKind::
                                        kSignonBootstrap)
                    ? ",signon_phase=" + summary_.signon_phase
                        + ",serverinfo_generations="
                        + std::to_string(summary_.serverinfo_generations)
                        + ",delta_generations="
                        + std::to_string(
                            summary_.delta_preparation_attempts)
                    : resource_manifest_enabled_
                            && pending_kind
                                == hl::network::
                                    GoldSrcNetchanReliablePayloadKind::
                                        kResourceManifest
                    ? ",signon_phase=" + summary_.signon_phase
                        + ",resource_manifest_generations="
                        + std::to_string(
                            summary_.resource_manifest_generations)
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

    bool DeltaDescriptionsNegativeEvidenceSatisfied() const noexcept
    {
        return summary_.delta_registry_built
            && summary_.delta_preparation_attempts == 1
            && summary_.signon_bootstrap_resent >= 1
            && summary_.wrong_reliable_ack_rejected
            && summary_.duplicate_client_reliable_suppressed
            && summary_.signon_bootstrap_acked == 1
            && summary_.delta_slot_resets == 1
            && summary_.delta_pending_state_cleared_on_reset
            && summary_.delta_fresh_sessions_after_reset == 1
            && summary_.delta_slot_reused_after_reset
            && (!summary_.signon_bootstrap_fragmented
                || summary_.signon_bootstrap_fragment_acknowledged_count
                    == summary_.signon_bootstrap_fragment_count);
    }

    bool ResourceManifestNegativeEvidenceSatisfied() const noexcept
    {
        const bool base_evidence =
            summary_.resource_manifest_resent >= 1
            && summary_.wrong_reliable_ack_rejected
            && summary_.resource_request_deliveries == 1
            && summary_.resource_manifest_generations == 1;
        if (!base_evidence)
        {
            return false;
        }
        if (summary_.fragment_transfer_started)
        {
            return summary_.fragment_transfer_completed
                && !summary_.fragment_transfer_active
                && summary_.fragment_count > 1
                && summary_.fragment_acknowledged_count
                    == summary_.fragment_count
                && summary_.future_ack_rejected >= 1;
        }
        return summary_.duplicate_resource_request_suppressed
            && summary_.unsupported_command_rejected >= 1;
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
                    == hl::network::GoldSrcNetchanCodecStatus::kUnsupportedFragment
                || decoded.status
                    == hl::network::GoldSrcNetchanCodecStatus::kMalformedFragment)
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
            && (pending_kind_before
                    == hl::network::GoldSrcNetchanReliablePayloadKind::
                        kServerInfo
                || pending_kind_before
                    == hl::network::GoldSrcNetchanReliablePayloadKind::
                        kSignonBootstrap
                || pending_kind_before
                    == hl::network::GoldSrcNetchanReliablePayloadKind::
                        kResourceManifest)
            && slot_state->netchan.diagnostics().reliable_ack_mismatch
                > reliable_ack_mismatch_before)
        {
            summary_.wrong_reliable_ack_rejected = true;
        }
        if (serverinfo_enabled_
            && (process_outcome.acknowledged_reliable_kind
                    == hl::network::GoldSrcNetchanReliablePayloadKind::
                        kServerInfo
                || process_outcome.acknowledged_reliable_kind
                    == hl::network::GoldSrcNetchanReliablePayloadKind::
                        kSignonBootstrap))
        {
            const bool combined_bootstrap =
                process_outcome.acknowledged_reliable_kind
                == hl::network::GoldSrcNetchanReliablePayloadKind::
                    kSignonBootstrap;
            const hl::network::GoldSrcSignonTransitionResult
                acknowledged_transition = combined_bootstrap
                ? slot_state->goldsrc_signon
                    .MarkSignonBootstrapAcknowledged()
                : slot_state->goldsrc_signon.MarkServerInfoAcknowledged();
            if (acknowledged_transition
                != hl::network::GoldSrcSignonTransitionResult::kAdvanced)
            {
                return FailTransport(
                    "serverinfo-ack-transition-failed",
                    "goldsrc_serverinfo_ack_failed: reason=invalid-signon-phase");
            }
            if (resource_manifest_enabled_
                && slot_state->goldsrc_signon.EnterAwaitingResourceRequest()
                    != hl::network::GoldSrcSignonTransitionResult::kAdvanced)
            {
                return FailTransport(
                    "resource-request-await-transition-failed",
                    "goldsrc_resource_manifest_transition_failed: reason=invalid-signon-phase");
            }
            if (combined_bootstrap)
            {
                ++summary_.signon_bootstrap_acked;
                summary_.signon_bootstrap_fragment_acknowledged_count =
                    summary_.signon_bootstrap_fragmented
                    ? summary_.signon_bootstrap_fragment_count
                    : 0;
            }
            SyncNetchanSummary(*slot_state);
            hl::common::Logger::Info(
                hl::common::LogCategory::Server,
                (combined_bootstrap
                        ? "goldsrc_signon_bootstrap_acknowledged: session_id="
                        : "goldsrc_serverinfo_acknowledged: session_id=")
                    + slot_state->session_id
                    + ",ack="
                    + std::to_string(decoded.packet.acknowledgement)
                    + ",ack_generation="
                    + std::to_string(
                        process_outcome.reliable_acknowledgement_generation)
                    + ",signon_phase=" + summary_.signon_phase);
        }
        if (resource_manifest_enabled_
            && process_outcome.acknowledged_reliable_kind
                == hl::network::GoldSrcNetchanReliablePayloadKind::
                    kResourceManifest)
        {
            if (slot_state->goldsrc_signon.MarkResourceManifestAcknowledged()
                != hl::network::GoldSrcSignonTransitionResult::kAdvanced)
            {
                return FailTransport(
                    "resource-manifest-ack-transition-failed",
                    "goldsrc_resource_manifest_ack_failed: reason=invalid-signon-phase");
            }
            summary_.resource_manifest_fragment_acknowledged_count =
                summary_.resource_manifest_fragmented
                ? summary_.resource_manifest_fragment_count
                : 0;
            SyncNetchanSummary(*slot_state);
            hl::common::Logger::Info(
                hl::common::LogCategory::Server,
                "goldsrc_resource_manifest_acknowledged: session_id="
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
                const hl::network::GoldSrcSignonBootstrapMode bootstrap_mode =
                    delta_descriptions_enabled_
                    ? hl::network::GoldSrcSignonBootstrapMode::
                        kServerInfoWithDeltaDescriptions
                    : hl::network::GoldSrcSignonBootstrapMode::
                        kServerInfoOnly;
                if (slot_state->goldsrc_signon.EnterAwaitingNew(
                        bootstrap_mode)
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

        hl::network::GoldSrcNetchanReliablePayloadKind queued_kind =
            hl::network::GoldSrcNetchanReliablePayloadKind::kNone;
        if (serverinfo_enabled_
            && slot_state->netchan.transport_phase()
                == hl::network::GoldSrcNetchanTransportPhase::kEstablished)
        {
            if (!HandleClientSignonPayload(
                    *slot_state,
                    decoded.packet,
                    &queued_kind))
            {
                return false;
            }
            if (admitted_session_id_.empty())
            {
                // An exact reliable `dropclient\n` resets the authoritative
                // slot and all session-owned transport state in the handler.
                return true;
            }
            if (queued_kind
                != hl::network::GoldSrcNetchanReliablePayloadKind::kNone)
            {
                return SendNetchanDatagram(
                    *slot_state,
                    true,
                    queued_kind
                            == hl::network::
                                GoldSrcNetchanReliablePayloadKind::
                                    kSignonBootstrap
                        ? "goldsrc_signon_bootstrap_sent"
                        : queued_kind
                                == hl::network::
                                    GoldSrcNetchanReliablePayloadKind::
                                        kServerInfo
                        ? "goldsrc_serverinfo_sent"
                        : "goldsrc_resource_manifest_sent");
            }
        }

        if (slot_state->netchan.fragment_sender().needs_fragment_staging())
        {
            return SendNetchanDatagram(
                *slot_state,
                true,
                "goldsrc_fragment_sent");
        }

        if (slot_state->netchan.reliable_pending())
        {
            const bool resend_expected =
                slot_state->netchan.next_datagram_will_include_reliable();
            return SendNetchanDatagram(
                *slot_state,
                resend_expected,
                resend_expected
                    ? "goldsrc_netchan_reliable_resent"
                    : "goldsrc_netchan_ordinary_sent");
        }

        if (resource_manifest_enabled_)
        {
            if (!SendNetchanDatagram(
                    *slot_state,
                    false,
                    "goldsrc_resource_manifest_ack_response"))
            {
                return false;
            }
            if (slot_state->goldsrc_signon.phase()
                == hl::network::GoldSrcSignonPhase::
                    kResourceManifestAcknowledged)
            {
                if (!resource_manifest_negative_proof_
                    && !delta_descriptions_negative_proof_)
                {
                    complete_ = true;
                }
                else if ((!resource_manifest_negative_proof_
                            || ResourceManifestNegativeEvidenceSatisfied())
                    && (!delta_descriptions_negative_proof_
                            || DeltaDescriptionsNegativeEvidenceSatisfied()))
                {
                    summary_.server_still_responsive = true;
                    complete_ = true;
                }
            }
            else if (resource_manifest_negative_proof_
                && summary_.requires_fragmentation_reported
                && summary_.oversized_manifest_not_truncated
                && summary_.resource_manifest_cached_outcome_reuses >= 1)
            {
                summary_.server_still_responsive = true;
                complete_ = true;
            }
            return true;
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
                        kServerInfoAcknowledged
                || slot_state->goldsrc_signon.phase()
                    == hl::network::GoldSrcSignonPhase::
                        kSignonBootstrapAcknowledged)
            {
                if (!serverinfo_negative_proof_
                    && !delta_descriptions_negative_proof_)
                {
                    complete_ = true;
                }
                else if ((!serverinfo_negative_proof_
                            || ServerInfoNegativeEvidenceSatisfied())
                    && (!delta_descriptions_negative_proof_
                            || DeltaDescriptionsNegativeEvidenceSatisfied()))
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
        if (awaiting_fresh_session_after_delta_reset_)
        {
            if (slot_state->slot != delta_reset_slot_)
            {
                return FailTransport(
                    "delta-slot-reuse-mismatch",
                    "goldsrc_delta_session_reset_failed: reason=slot-reuse-mismatch");
            }
            ++summary_.delta_fresh_sessions_after_reset;
            summary_.delta_slot_reused_after_reset = true;
            awaiting_fresh_session_after_delta_reset_ = false;
            hl::common::Logger::Info(
                hl::common::LogCategory::Server,
                "goldsrc_delta_fresh_session: session_id="
                    + slot_state->session_id
                    + ",slot=" + std::to_string(slot_state->slot)
                    + ",slot_reused=true,session_count="
                    + std::to_string(summary_.session_count)
                    + ",put_in_server=0,spawned=0,active=0");
        }
        if (delta_descriptions_enabled_)
        {
            ResetDeltaDescriptionSessionSummary();
        }
        if (resource_manifest_enabled_)
        {
            ResetResourceManifestSessionSummary();
        }
        if (serverinfo_enabled_)
        {
            ResetServerInfoSessionSummary();
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
    bool delta_descriptions_enabled_ = false;
    bool delta_descriptions_negative_proof_ = false;
    std::optional<std::filesystem::path> delta_descriptions_fixture_;
    bool resource_manifest_enabled_ = false;
    bool resource_manifest_negative_proof_ = false;
    std::optional<std::filesystem::path> resource_manifest_fixture_;
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
    bool awaiting_fresh_session_after_delta_reset_ = false;
    int delta_reset_slot_ = 0;
};
