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
    static constexpr std::size_t kMaximumCombatEntityThinksPerHostFrame = 64;
    static constexpr std::size_t kMaximumCombatEntityRemovalsPerHostFrame = 64;

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
          world_baselines_enabled_(
              options.goldsrc_world_baselines_enabled),
          world_baselines_negative_proof_(
              options.goldsrc_world_baselines_negative_proof),
          first_snapshot_enabled_(
              options.goldsrc_first_snapshot_enabled),
          first_snapshot_negative_proof_(
              options.goldsrc_first_snapshot_negative_proof),
          continuous_snapshots_enabled_(
              options.goldsrc_continuous_snapshots_enabled),
          continuous_snapshots_negative_proof_(
              options.goldsrc_continuous_snapshots_negative_proof),
          player_lifecycle_enabled_(
              options.goldsrc_player_lifecycle_enabled),
          player_lifecycle_negative_proof_(
              options.goldsrc_player_lifecycle_negative_proof),
          pmove_enabled_(options.goldsrc_pmove_enabled),
          pmove_negative_proof_(options.goldsrc_pmove_negative_proof),
          pmove_persistent_(options.goldsrc_pmove_persistent),
          combat_enabled_(options.goldsrc_combat_enabled),
          fall_damage_proof_(options.goldsrc_fall_damage_proof),
          pmove_observation_seconds_(
              static_cast<double>(options.goldsrc_pmove_observation_ms)
              / 1000.0),
          manual_shutdown_file_(options.goldsrc_manual_shutdown_file),
          manual_disconnect_file_(options.goldsrc_manual_disconnect_file),
          stock_test_control_file_(
              options.goldsrc_stock_test_control_file),
          manual_session_lifecycle_({
              options.goldsrc_pmove_persistent,
              static_cast<std::uint64_t>(
                  options.goldsrc_handshake_timeout_ms),
              static_cast<std::uint64_t>(
                  options.goldsrc_pmove_observation_ms),
              30000u}),
          snapshot_rate_hz_(options.goldsrc_snapshot_rate_hz),
          dedicated_runtime_(
              options.runtime_mode == hl::game_api::ServerRuntimeMode::kDedicated)
    {
        summary_ = {};
        summary_.enabled = true;
        summary_.max_clients = state_.server_state.maxclients;
        summary_.multi_client_routing_implemented = true;
        summary_.two_client_admission_implemented =
            state_.server_state.maxclients >= 2;
        summary_.per_client_signon_state = true;
        summary_.per_client_frame_history = true;
        summary_.per_client_delta_base = true;
        summary_.per_client_command_clock = true;
        summary_.per_client_pmove_state = true;
        summary_.two_client_visibility_policy =
            "all_connected_players_plus_existing_modeled_entities_no_pvs";
        summary_.multi_client_receive_policy =
            "bounded_endpoint_routed_host_frame_drain";
        summary_.multi_client_send_policy =
            "bounded_round_robin_per_client_due_scheduler";
        summary_.client_replication.resize(static_cast<std::size_t>(
            (std::max)(0, state_.server_state.maxclients)));
        awaiting_fresh_session_after_delta_reset_.resize(
            summary_.client_replication.size(),
            false);
        stock_test_spawn_anchors_.resize(summary_.client_replication.size());
        stock_test_reset_transitions_.assign(
            summary_.client_replication.size(),
            std::vector<bool>(summary_.client_replication.size(), false));
        stock_test_nointerp_clear_pending_.assign(
            summary_.client_replication.size(),
            std::vector<bool>(summary_.client_replication.size(), false));
        for (std::size_t index = 0;
             index < summary_.client_replication.size();
             ++index)
        {
            summary_.client_replication[index].slot =
                static_cast<int>(index + 1u);
            summary_.client_replication[index].edict =
                static_cast<int>(index + 1u);
        }
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
        summary_.world_baselines_enabled = world_baselines_enabled_;
        summary_.world_baselines_negative_proof =
            world_baselines_negative_proof_;
        summary_.first_snapshot_enabled = first_snapshot_enabled_;
        summary_.pmove_enabled = pmove_enabled_;
        summary_.pmove_negative_proof = pmove_negative_proof_;
        summary_.pmove_persistent = pmove_persistent_;
        summary_.combat_enabled = combat_enabled_;
        summary_.combat_callback_order_verified = false;
        summary_.combat_time_contract_verified = false;
        summary_.weapon_postframe_contract_verified = false;
        summary_.weapon_replication_contract_verified = false;
        summary_.player_aware_trace = false;
        summary_.per_client_weapondata = false;
        summary_.frame_history_weapondata = false;
        summary_.manual_session_phase =
            pmove_persistent_ ? "waiting_for_client" : "disabled";
        summary_.gameplay_active = false;
        if (pmove_enabled_)
        {
            summary_.command_execution_contract_verified = true;
            summary_.command_replay_contract_verified = true;
            summary_.game_dll_movement_callback_order_verified = true;
            summary_.command_time_contract_verified = true;
            summary_.pm_init_implemented = true;
            summary_.pmove_services_implemented = true;
            summary_.pmove_service_callback_count = static_cast<int>(
                pmove_runtime_.implemented_service_callback_count());
            summary_.pmove_physent_policy = "world_only_static_bsp";
            summary_.movevars_source = "authoritative_server_cvar_registry";
            summary_.movevars_client_sync_verified = true;
            summary_.world_bsp_collision_implemented = true;
            summary_.standing_hull_supported = true;
            summary_.duck_hull_supported = true;
            summary_.pmove_context_builder_implemented = true;
            summary_.pmove_execution_implemented = true;
            summary_.authoritative_movement_commit_implemented = true;
        }
        summary_.first_snapshot_negative_proof =
            first_snapshot_negative_proof_;
        summary_.continuous_snapshots_enabled =
            continuous_snapshots_enabled_;
        summary_.continuous_snapshots_negative_proof =
            continuous_snapshots_negative_proof_;
        summary_.player_lifecycle_enabled = player_lifecycle_enabled_;
        summary_.player_lifecycle_negative_proof =
            player_lifecycle_negative_proof_;
        summary_.player_lifecycle_phase =
            player_lifecycle_enabled_ ? "no_client_edict" : "disabled";
        summary_.snapshot_rate_hz = snapshot_rate_hz_;
        summary_.snapshot_interval_ms =
            1000.0 / snapshot_rate_hz_;
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
            MarkComplete("udp_bind_failed");
            transport_failed_ = true;
            hl::common::Logger::Error(
                hl::common::LogCategory::Server,
                "goldsrc_udp_bind_failed: address=" + bind_address_
                    + ",port=" + std::to_string(requested_port_)
                    + ",platform_error=" + socket_.LastError());
            return false;
        }
        if (pmove_enabled_ && !player_lifecycle_enabled_)
        {
            return FailBind("pmove-requires-player-lifecycle");
        }
        if (pmove_negative_proof_ && !pmove_enabled_)
        {
            return FailBind("pmove-negative-proof-requires-pmove");
        }
        if (pmove_persistent_ && !pmove_enabled_)
        {
            return FailBind("pmove-persistent-requires-pmove");
        }
        if (combat_enabled_
            && (!pmove_enabled_ || !player_lifecycle_enabled_
                || !continuous_snapshots_enabled_))
        {
            return FailBind("combat-requires-pmove-lifecycle-snapshots");
        }
        if (combat_enabled_ && bind_address_ != "127.0.0.1")
        {
            return FailBind("combat-loopback-required");
        }
        if (combat_enabled_
            && (state_.dll_functions.pfnPlayerPreThink == nullptr
                || state_.dll_functions.pfnPlayerPostThink == nullptr
                || state_.dll_functions.pfnStartFrame == nullptr
                || state_.dll_functions.pfnThink == nullptr
                || state_.dll_functions.pfnGetWeaponData == nullptr
                || state_.dll_functions.pfnUpdateClientData == nullptr))
        {
            return FailBind("combat-game-dll-callback-contract-unavailable");
        }
        if (pmove_persistent_ && pmove_negative_proof_)
        {
            return FailBind("pmove-persistent-conflicts-with-negative-proof");
        }
        if (manual_shutdown_file_.has_value() && !pmove_persistent_)
        {
            return FailBind("manual-shutdown-file-requires-persistent-pmove");
        }
        if (manual_shutdown_file_.has_value())
        {
            std::error_code shutdown_file_error;
            if (std::filesystem::exists(
                    *manual_shutdown_file_,
                    shutdown_file_error)
                || shutdown_file_error)
            {
                return FailBind("manual-shutdown-file-not-ready");
            }
        }
        if (manual_disconnect_file_.has_value() && !pmove_persistent_)
        {
            return FailBind("manual-disconnect-file-requires-persistent-pmove");
        }
        if (manual_disconnect_file_.has_value())
        {
            std::error_code disconnect_file_error;
            if (std::filesystem::exists(
                    *manual_disconnect_file_,
                    disconnect_file_error)
                || disconnect_file_error)
            {
                return FailBind("manual-disconnect-file-not-ready");
            }
        }
        if (stock_test_control_file_.has_value()
            && (!pmove_persistent_ || bind_address_ != "127.0.0.1"))
        {
            return FailBind(
                "stock-test-control-requires-loopback-persistent-pmove");
        }
        if (stock_test_control_file_.has_value())
        {
            std::error_code control_file_error;
            if (!IsExternalStockTestControlPath(*stock_test_control_file_))
            {
                return FailBind("stock-test-control-path-not-external");
            }
            const bool control_file_exists = std::filesystem::exists(
                *stock_test_control_file_,
                control_file_error);
            if (control_file_error)
            {
                return FailBind("stock-test-control-file-probe-failed");
            }
            if (control_file_exists)
            {
                return FailBind("stock-test-control-file-already-exists");
            }
        }
        lifecycle_edict_registry_.Reset(
            static_cast<std::uint16_t>(state_.server_state.maxclients));
        if (pmove_enabled_)
        {
            const auto move_variable =
                [this](std::string_view name, float fallback)
                {
                    const std::optional<hl::game_api::CvarSnapshot> snapshot =
                        state_.cvar_registry.SnapshotOf(name);
                    return snapshot.has_value() ? snapshot->value : fallback;
                };
            hl::game_api::detail::GoldSrcMovevarsConfig movevars{};
            movevars.gravity = move_variable("sv_gravity", 800.0f);
            movevars.stop_speed = move_variable("sv_stopspeed", 100.0f);
            movevars.maximum_speed = move_variable("sv_maxspeed", 320.0f);
            movevars.spectator_maximum_speed =
                move_variable("sv_spectatormaxspeed", 500.0f);
            movevars.accelerate = move_variable("sv_accelerate", 10.0f);
            movevars.air_accelerate =
                move_variable("sv_airaccelerate", 10.0f);
            movevars.water_accelerate =
                move_variable("sv_wateraccelerate", 10.0f);
            movevars.friction = move_variable("sv_friction", 4.0f);
            movevars.edge_friction = move_variable("edgefriction", 2.0f);
            movevars.water_friction =
                move_variable("sv_waterfriction", 1.0f);
            movevars.bounce = move_variable("sv_bounce", 1.0f);
            movevars.step_size = move_variable("sv_stepsize", 18.0f);
            movevars.maximum_velocity =
                move_variable("sv_maxvelocity", 2000.0f);
            movevars.z_maximum = move_variable("sv_zmax", 4096.0f);
            movevars.wave_height = move_variable("sv_wateramp", 0.0f);
            movevars.footsteps =
                move_variable("mp_footsteps", 1.0f) != 0.0f;
            movevars.roll_angle = move_variable("sv_rollangle", 0.0f);
            movevars.roll_speed = move_variable("sv_rollspeed", 0.0f);
            movevars.multiplayer = state_.server_state.maxclients > 1;

            hl::game_api::detail::GoldSrcPmoveGameDllCallbacks callbacks{};
            callbacks.pm_init = state_.dll_functions.pfnPM_Init;
            callbacks.pm_move = state_.dll_functions.pfnPM_Move;
            callbacks.cmd_start = state_.dll_functions.pfnCmdStart;
            callbacks.cmd_end = state_.dll_functions.pfnCmdEnd;
            callbacks.player_pre_think =
                state_.dll_functions.pfnPlayerPreThink;
            callbacks.player_post_think =
                state_.dll_functions.pfnPlayerPostThink;
            callbacks.entity_think = state_.dll_functions.pfnThink;
            callbacks.global_time = &state_.globalvars.time;
            callbacks.global_frametime = &state_.globalvars.frametime;
            callbacks.active_attack_postthink_slot =
                &state_.goldsrc_combat_active_attack_postthink_slot;
            callbacks.combat_enabled = combat_enabled_;
            if (!pmove_runtime_.InitializeWorld(state_.world_context))
            {
                return FailBind("pmove-world-collision-unavailable");
            }
            if (!pmove_runtime_.InitializeGameDll(
                    callbacks,
                    state_.game_directory,
                    movevars))
            {
                return FailBind("pmove-game-dll-contract-unavailable");
            }
            if (combat_enabled_)
            {
                state_.goldsrc_combat_trace_calls = 0u;
                state_.goldsrc_combat_player_hits = 0u;
                state_.goldsrc_combat_world_occlusions = 0u;
                state_.goldsrc_combat_trace_hits_by_slot.fill(0u);
                state_.goldsrc_combat_world_occlusions_by_slot.fill(0u);
                state_.goldsrc_combat_weapon_range_traces_by_slot.fill(0u);
                state_.goldsrc_combat_shooter_ignored_by_slot.fill(0u);
                state_.goldsrc_combat_weapon_range_player_hits_by_slot.fill(
                    0u);
                state_.goldsrc_combat_weapon_range_world_hits_by_slot.fill(
                    0u);
                state_.goldsrc_combat_playback_events_by_slot.fill(0u);
                state_.goldsrc_combat_last_trace_target_by_slot.fill(0);
                state_.goldsrc_combat_lifecycle_registry =
                    &lifecycle_edict_registry_;
                state_.goldsrc_combat_pmove_runtime = &pmove_runtime_;
                state_.goldsrc_combat_trace_enabled = true;
            }
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
        if (pmove_persistent_)
        {
            manual_session_lifecycle_.Start(MonotonicMilliseconds(
                std::chrono::steady_clock::now()));
            SyncManualSessionSummary();
            summary_.manual_session_started = true;
            summary_.manual_session_active = true;
            hl::common::Logger::Info(
                hl::common::LogCategory::Server,
                "goldsrc_manual_session_started: mode=persistent"
                ",phase=waiting_for_client,runtime_active=1");
        }
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

        if (combat_enabled_)
        {
            state_.edict_store.BeginFrameReuseBarrier();
        }

        const auto frame_time = std::chrono::steady_clock::now();
        if (CheckManualShutdownRequest(frame_time))
        {
            return !transport_failed_;
        }
        if (CheckManualDisconnectRequest(frame_time))
        {
            return !transport_failed_;
        }
        CheckStockTestControlRequest(frame_time);
        double host_frame_elapsed = 0.0;
        if (!network_server_clock_started_)
        {
            network_server_clock_started_ = true;
            last_network_host_frame_time_ = frame_time;
            network_server_time_ = state_.server_state.time;
        }
        else
        {
            const double elapsed = std::chrono::duration<double>(
                frame_time - last_network_host_frame_time_).count();
            if (std::isfinite(elapsed) && elapsed >= 0.0)
            {
                network_server_time_ += elapsed;
                host_frame_elapsed = elapsed;
            }
            last_network_host_frame_time_ = frame_time;
        }
        if (!deadline_started_)
        {
            deadline_ = frame_time + std::chrono::milliseconds(timeout_ms_);
            deadline_started_ = true;
        }
        if (frame_time >= deadline_
            && !PersistentDeadlineSuppressed())
        {
            MarkTimeout();
            return true;
        }
        if (world_baselines_enabled_
            && (!resource_manifest_enabled_
                || !delta_descriptions_enabled_))
        {
            return FailBind(
                "world-baselines-require-resource-and-delta-bootstrap");
        }
        if (world_baselines_negative_proof_
            && !world_baselines_enabled_)
        {
            return FailBind(
                "world-baselines-negative-proof-requires-world-baselines");
        }
        if (first_snapshot_enabled_ && !world_baselines_enabled_)
        {
            return FailBind(
                "first-snapshot-requires-world-baselines");
        }
        if (first_snapshot_negative_proof_
            && !first_snapshot_enabled_)
        {
            return FailBind(
                "first-snapshot-negative-proof-requires-first-snapshot");
        }
        if (continuous_snapshots_enabled_ && !first_snapshot_enabled_)
        {
            return FailBind(
                "continuous-snapshots-require-first-snapshot");
        }
        if (continuous_snapshots_negative_proof_
            && !continuous_snapshots_enabled_)
        {
            return FailBind(
                "continuous-snapshot-negative-proof-requires-streaming");
        }
        if (player_lifecycle_enabled_ && !continuous_snapshots_enabled_)
        {
            return FailBind(
                "player-lifecycle-requires-continuous-snapshots");
        }
        if (player_lifecycle_negative_proof_
            && !player_lifecycle_enabled_)
        {
            return FailBind(
                "player-lifecycle-negative-proof-requires-lifecycle");
        }
        if (player_lifecycle_enabled_ && bind_address_ != "127.0.0.1")
        {
            return FailBind("player-lifecycle-loopback-required");
        }
        if (!std::isfinite(snapshot_rate_hz_)
            || snapshot_rate_hz_
                < hl::network::kGoldSrcMinimumSnapshotRateHz
            || snapshot_rate_hz_
                > hl::network::kGoldSrcMaximumSnapshotRateHz)
        {
            return FailBind("continuous-snapshot-rate-out-of-range");
        }
        for (DedicatedPlayerRuntimeSlot& slot_state :
             state_.dedicated_multiplayer_foundation.slots)
        {
            if (!IsDedicatedSlotConnectedOccupant(slot_state)
                || !slot_state.netchan.initialized()
                || !slot_state.netchan.ExpireFragmentTransfer(frame_time))
            {
                continue;
            }
            SyncNetchanSummary(slot_state);
            if (!HandleClientDisconnect(slot_state))
            {
                return false;
            }
            hl::common::Logger::Warn(
                hl::common::LogCategory::Server,
                "goldsrc_fragment_transfer_failed: session_id="
                    + slot_state.session_id
                    + ",reason=timeout,scope=client");
        }

        ++summary_.host_frames_pumped;
        if (!CallCombatStartFrame(host_frame_elapsed))
        {
            return FailTransport(
                "combat-start-frame-failed",
                "goldsrc_combat_failed: reason=start-frame-callback");
        }
        if (!DispatchCombatEntityThinks())
        {
            return FailTransport(
                "combat-entity-think-failed",
                "goldsrc_combat_failed: reason=entity-think-callback");
        }
        datagrams_sent_this_frame_ = 0;
        if (!SendPendingOrdinaryAdvances())
        {
            return false;
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

        DedicatedPlayerRuntimeSlot* legacy_completion_slot = nullptr;
        if (!complete_ && continuous_snapshots_enabled_)
        {
            UpdateStockTestSpawnAnchors();
            for (DedicatedPlayerRuntimeSlot& slot_state :
                 state_.dedicated_multiplayer_foundation.slots)
            {
                if (!IsDedicatedSlotConnectedOccupant(slot_state))
                {
                    continue;
                }
                if (legacy_completion_slot == nullptr)
                {
                    legacy_completion_slot = &slot_state;
                }
                const bool established_movement_streaming =
                    pmove_enabled_
                    && slot_state.goldsrc_player_lifecycle.put_in_server()
                    && slot_state.goldsrc_player_lifecycle.signon_complete();
                summary_.receive_before_snapshot_in_streaming_phase =
                    summary_.receive_before_snapshot_in_streaming_phase
                    || established_movement_streaming;
            }
            if (!SendDueSnapshotsFairly())
            {
                return false;
            }
            // Stock GoldSrc keeps EF_MUZZLEFLASH visible for the complete
            // per-client send sweep, then clears it once before the next host
            // frame. Clearing here lets every receiver observe the shot while
            // preventing the one-frame effect from becoming a permanent glow.
            state_.edict_store.ClearTransientMuzzleFlashEffects();
            LogClientProgressSamples();
            for (DedicatedPlayerRuntimeSlot& slot_state :
                 state_.dedicated_multiplayer_foundation.slots)
            {
                if (!IsDedicatedSlotConnectedOccupant(slot_state)
                    || !player_lifecycle_enabled_
                    || !slot_state
                            .goldsrc_continuous_snapshot_started_server_time
                            .has_value()
                    || slot_state.goldsrc_continuous_snapshots_sent < 3u
                    || slot_state.goldsrc_distinct_frame_references < 2u)
                {
                    continue;
                }
                if (!slot_state.goldsrc_player_lifecycle.put_in_server()
                    && slot_state.goldsrc_player_lifecycle.phase()
                        == hl::network::GoldSrcPlayerLifecyclePhase::
                            kAwaitingPutInServer
                    && !ReconcilePlayerLifecycle(slot_state))
                {
                    return false;
                }
                if (pmove_persistent_
                    && slot_state.slot == 1
                    && slot_state.goldsrc_player_lifecycle.signon_complete())
                {
                    ProcessManualSessionEvents(
                        manual_session_lifecycle_.Advance(
                            MonotonicMilliseconds(frame_time),
                            summary_.movement_snapshot_integration));
                }
                SyncClientReplicationSummary(slot_state);
            }
        }

        if (!complete_ && continuous_snapshots_enabled_
            && summary_.max_clients == 1
            && legacy_completion_slot != nullptr)
        {
            DedicatedPlayerRuntimeSlot& slot_state =
                *legacy_completion_slot;
            if (player_lifecycle_enabled_
                && slot_state
                    .goldsrc_continuous_snapshot_started_server_time
                    .has_value()
                && summary_.continuous_snapshots_sent >= 3
                && summary_.continuous_delta_snapshots >= 1
                && summary_.distinct_frame_references >= 2)
            {
                if (slot_state.goldsrc_player_lifecycle.signon_complete()
                    && pmove_enabled_ && !pmove_persistent_
                    && slot_state.goldsrc_first_movement_server_time.has_value()
                    && network_server_time_
                            - *slot_state.goldsrc_first_movement_server_time
                        >= pmove_observation_seconds_
                    && summary_.movement_executed
                    && summary_.movement_snapshot_integration
                    && summary_.movement_snapshots_sent >= 3)
                {
                    summary_.continuous_snapshot_stable = true;
                    summary_.server_still_responsive = true;
                    summary_.next_observed_boundary =
                        "two_client_player_replication_required";
                    summary_.previous_player_lifecycle_boundary_resolved =
                        true;
                    MarkComplete("legacy_pmove_proof_complete");
                }
                else if (slot_state.goldsrc_player_lifecycle.signon_complete()
                    && !pmove_enabled_
                    && network_server_time_
                            - *slot_state
                                .goldsrc_continuous_snapshot_started_server_time
                        >= 10.0
                    && summary_.player_snapshot_frames
                        >= (player_lifecycle_negative_proof_ ? 500 : 30))
                {
                    summary_.continuous_snapshot_stable = true;
                    summary_.server_still_responsive = true;
                    summary_.next_observed_boundary =
                        "movement_execution_required";
                    summary_.previous_player_lifecycle_boundary_resolved =
                        true;
                    MarkComplete("legacy_player_lifecycle_proof_complete");
                }
            }
            else if (slot_state
                    .goldsrc_continuous_snapshot_started_server_time
                    .has_value()
                && network_server_time_
                        - *slot_state
                            .goldsrc_continuous_snapshot_started_server_time
                    >= 10.0
                && summary_.continuous_snapshots_sent
                    >= (continuous_snapshots_negative_proof_ ? 400 : 30)
                && summary_.continuous_delta_snapshots >= 1
                && summary_.distinct_frame_references >= 2
                && (!continuous_snapshots_negative_proof_
                    || (continuous_negative_unknown_observed_
                        && continuous_negative_stale_after_unknown_observed_)))
            {
                summary_.continuous_snapshot_stable = true;
                summary_.server_still_responsive = true;
                summary_.next_observed_boundary =
                    "player_lifecycle_or_signon_progression_required";
                MarkComplete("legacy_continuous_snapshot_proof_complete");
            }
        }

        if (!complete_ && std::chrono::steady_clock::now() >= deadline_
            && !PersistentDeadlineSuppressed())
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
        const bool ordinary_combat_evidence_satisfied =
            summary_.combat_callback_order_verified
                && summary_.combat_time_contract_verified
                && summary_.weapon_postframe_contract_verified
                && summary_.weapon_replication_contract_verified
                && summary_.player_aware_trace
                && summary_.per_client_weapondata
                && summary_.frame_history_weapondata
                && summary_.gameplay_active;
        const bool combat_evidence_satisfied = !combat_enabled_
            ? true
            : fall_damage_proof_
                ? stock_test_fall_acceptance_.phase
                    == StockTestFallAcceptancePhase::kComplete
                : ordinary_combat_evidence_satisfied;
        if (pmove_persistent_
            && manual_session_lifecycle_.shutdown_requested())
        {
            return complete_ && !transport_failed_
                && summary_.ready && summary_.clean_shutdown
                && combat_evidence_satisfied;
        }
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
            && summary_.put_in_server == player_lifecycle_enabled_
            && summary_.spawned == player_lifecycle_enabled_
            && !summary_.active;
        if (!handshake_succeeded || !netchan_enabled_)
        {
            return handshake_succeeded && combat_evidence_satisfied;
        }
        if (!combat_evidence_satisfied)
        {
            return false;
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
        if (world_baselines_enabled_)
        {
            expected_reliable_acknowledgements +=
                summary_.baseline_fragmented
                ? summary_.baseline_fragment_count
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
                const bool post_resource_command_succeeded =
                    summary_.clc_move_delivered == 1
                    && summary_.post_resource_state_advances == 1
                    && summary_.post_resource_checksum_validated
                    && summary_.post_resource_delta_schema_used
                    && summary_.post_resource_pre_spawn_ignored
                    && summary_.previous_unidentified_command_boundary_resolved
                    && (first_snapshot_enabled_
                        || summary_.signon_phase
                            == (world_baselines_enabled_
                                ? "awaiting_first_snapshot"
                                : "awaiting_server_baseline_or_snapshot"));
                const bool first_snapshot_succeeded =
                    !first_snapshot_enabled_
                    || (summary_.snapshot_contract_verified
                        && summary_.snapshot_message_order_verified
                        && summary_.frame_ack_contract_verified
                        && summary_.frame_history_implemented
                        && summary_.frame_history_depth
                            == static_cast<int>(
                                hl::network::
                                    kGoldSrcClientFrameHistoryDepth)
                        && summary_.clientdata_implemented
                        && !summary_.weapon_data_required
                        && summary_.packet_entities_implemented
                        && summary_.first_snapshot_entity_count > 0
                        && summary_.first_snapshot_payload_bytes > 0
                        && summary_.first_snapshot_prepared == 1
                        && summary_.first_snapshot_sent == 1
                        && summary_.first_snapshot_acked == 1
                        && summary_.previous_first_snapshot_boundary_resolved
                        && summary_.stock_advanced_past_previous_boundary
                        && summary_.signon_phase
                            == (continuous_snapshots_enabled_
                                ? "continuous_snapshot_stable"
                                : "first_snapshot_acknowledged")
                        && (!continuous_snapshots_enabled_
                            || (summary_
                                    .continuous_snapshot_contract_verified
                                && summary_
                                    .delta_packet_entities_contract_verified
                                && summary_
                                    .frame_reference_resolution_contract_verified
                                && summary_.loss_recovery_contract_verified
                                && summary_.continuous_snapshots_sent >= 30
                                && summary_.continuous_delta_snapshots >= 1
                                && summary_.distinct_frame_references >= 2
                                && summary_.continuous_server_time_monotonic
                                && summary_.continuous_frame_ids_advanced
                                && summary_.newest_acknowledged_base_selected
                                 && summary_.continuous_frame_history_bounded
                                 && summary_.continuous_snapshot_stable)));
                const bool player_lifecycle_succeeded =
                    !player_lifecycle_enabled_
                    || (summary_.player_lifecycle_contract_verified
                        && summary_
                            .deterministic_client_edict_contract_verified
                        && summary_.game_dll_player_callbacks_verified
                        && summary_.game_dll_callback_order_verified
                        && summary_.view_entity_contract_verified
                        && summary_.player_snapshot_contract_verified
                        && summary_.player_signon_progression_verified
                        && summary_.client_edict_index
                            == summary_.allocated_slot
                        && summary_.client_userinfo_changed_calls == 1
                        && summary_.client_connect_calls == 1
                        && summary_.client_connect_accepts == 1
                        && summary_.client_connect_rejections == 0
                        && summary_.client_put_in_server_calls == 1
                        && summary_.client_put_in_server_successes == 1
                        && summary_.game_dll_client_connected
                        && summary_.player_private_data_ready
                        && summary_.player_entity_ready
                        && summary_.player_spawned
                        && summary_.player_view_established
                        && summary_.player_view_entity
                            == summary_.client_edict_index
                        && summary_.player_snapshot_frames >= 3
                        && summary_.first_player_snapshot_frame_id != 0u
                        && summary_.player_snapshot_adds >= 1
                        && summary_.player_clientdata_implemented
                        && summary_.player_signon_complete
                        && !summary_.gameplay_active
                        && summary_
                            .previous_player_lifecycle_boundary_resolved
                        && summary_.next_observed_boundary
                            == (pmove_enabled_
                                ? "two_client_player_replication_required"
                                : "movement_execution_required")
                        && (!pmove_enabled_
                            || (summary_
                                    .command_execution_contract_verified
                                && summary_
                                    .command_replay_contract_verified
                                && summary_
                                    .game_dll_movement_callback_order_verified
                                && summary_.command_time_contract_verified
                                && summary_.pm_init_call_count == 1
                                && summary_.pmove_calls > 0
                                && summary_.movement_commands_executed > 0
                                && summary_.movement_ready
                                && summary_.movement_executed
                                && summary_
                                    .authoritative_movement_commit_implemented
                                && summary_.movement_snapshot_integration
                                && summary_.movement_snapshots_sent >= 3
                                && !summary_.gameplay_active)));
                const bool world_baseline_succeeded =
                    !world_baselines_enabled_
                    || (summary_.baseline_contract_verified
                        && summary_.baseline_message_order_verified
                        && summary_.baseline_context_built
                        && summary_.baseline_build_attempts == 1
                        && summary_.baseline_generations == 1
                        && summary_.baseline_entity_count
                            >= state_.server_state.maxclients + 1
                        && summary_.baseline_player_count
                            == state_.server_state.maxclients
                        && summary_.baseline_callback_calls
                            == summary_.baseline_entity_count
                        && summary_.baseline_instanced_callback_calls == 1
                        && summary_.baseline_payload_bytes > 0
                        && summary_.baseline_queued == 1
                        && summary_.baseline_sent == 1
                        && summary_.baseline_acked == 1
                        && (!summary_.baseline_fragmented
                            || (summary_.baseline_fragment_count > 1
                                && summary_
                                    .baseline_fragment_acknowledged_count
                                    == summary_.baseline_fragment_count))
                        && summary_.send_entities_delivered == 1
                        && summary_
                            .previous_server_baseline_boundary_resolved
                        && summary_.next_observed_boundary
                            == (pmove_enabled_
                                ? "two_client_player_replication_required"
                                : player_lifecycle_enabled_
                                ? "movement_execution_required"
                                : continuous_snapshots_enabled_
                                ? "player_lifecycle_or_signon_progression_required"
                                : first_snapshot_enabled_
                                ? "continuous_snapshot_cadence_required"
                                : "first_snapshot_required")
                        && first_snapshot_succeeded
                        && player_lifecycle_succeeded);
                const bool resource_manifest_phase_succeeded =
                    summary_.signon_phase
                        == "resource_manifest_acknowledged"
                    || post_resource_command_succeeded;
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
                    && resource_manifest_phase_succeeded
                    && world_baseline_succeeded;
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
        if (state_.goldsrc_combat_lifecycle_registry
            == &lifecycle_edict_registry_)
        {
            state_.goldsrc_combat_trace_enabled = false;
            state_.goldsrc_combat_lifecycle_registry = nullptr;
        }
        if (state_.goldsrc_combat_pmove_runtime == &pmove_runtime_)
        {
            state_.goldsrc_combat_trace_enabled = false;
            state_.goldsrc_combat_pmove_runtime = nullptr;
        }
        RefreshTwoClientReplicationSummary();

        if (player_lifecycle_enabled_)
        {
            for (DedicatedPlayerRuntimeSlot& slot_state :
                 state_.dedicated_multiplayer_foundation.slots)
            {
                if (!slot_state.goldsrc_player_lifecycle
                        .game_dll_connected())
                {
                    continue;
                }
                if (!DisconnectPlayerLifecycle(
                        slot_state,
                        "host-shutdown"))
                {
                    transport_failed_ = true;
                    summary_.last_reject_reason =
                        "player-lifecycle-shutdown-disconnect-failed";
                }
                ApplyDedicatedClientDisconnect(state_, slot_state);
                SyncClientReplicationSummary(slot_state);
            }
        }
        if (pmove_persistent_)
        {
            if (manual_session_lifecycle_.MarkClientDisconnected())
            {
                hl::common::Logger::Info(
                    hl::common::LogCategory::Server,
                    "goldsrc_client_disconnected: mode=persistent"
                    ",reason=host_shutdown,reconnect_ready=0");
            }
            manual_session_lifecycle_.RequestShutdown();
            if (!complete_)
            {
                MarkComplete("process_shutdown");
            }
            SyncManualSessionSummary();
            LogManualSessionShutdown("process_shutdown");
        }
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
        RefreshTwoClientReplicationSummary();
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
                + ",completion_reason=" + completion_reason_
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
        if (summary_.clc_move_decoded > 0 || summary_.clc_move_rejected > 0)
        {
            hl::common::Logger::Info(
                hl::common::LogCategory::Summary,
                "goldsrc_post_resource_command_summary: opcode=2,identity="
                    + summary_.post_resource_command_identity
                    + ",reliability=unreliable,decoded="
                    + std::to_string(summary_.clc_move_decoded)
                    + ",delivered="
                    + std::to_string(summary_.clc_move_delivered)
                    + ",rejected="
                    + std::to_string(summary_.clc_move_rejected)
                    + ",checksum_rejected="
                    + std::to_string(summary_.clc_move_checksum_rejected)
                    + ",count_rejected="
                    + std::to_string(summary_.clc_move_count_rejected)
                    + ",truncated_rejected="
                    + std::to_string(summary_.clc_move_truncated_rejected)
                    + ",invalid_phase_rejected="
                    + std::to_string(
                        summary_.clc_move_invalid_phase_rejected)
                    + ",state_advances="
                    + std::to_string(summary_.post_resource_state_advances)
                    + ",checksum_validated="
                    + std::string(
                        summary_.post_resource_checksum_validated
                            ? "true"
                            : "false")
                    + ",delta_schema_used="
                    + std::string(
                        summary_.post_resource_delta_schema_used
                            ? "true"
                            : "false")
                    + ",pre_spawn_ignored="
                    + std::string(
                        summary_.post_resource_pre_spawn_ignored
                            ? "true"
                            : "false")
                    + ",previous_boundary_resolved="
                    + std::string(
                        summary_.previous_unidentified_command_boundary_resolved
                            ? "true"
                            : "false")
                    + ",pending_reliable_preserved="
                    + std::string(
                        summary_.post_resource_pending_reliable_preserved
                            ? "true"
                            : "false")
                    + ",packet_loss="
                    + std::to_string(summary_.last_move_packet_loss)
                    + ",backup_commands="
                    + std::to_string(summary_.last_move_backup_commands)
                    + ",new_commands="
                    + std::to_string(summary_.last_move_new_commands)
                    + ",total_commands="
                    + std::to_string(summary_.last_move_total_commands)
                    + ",new_command_msec="
                    + std::to_string(summary_.last_move_new_command_msec)
                    + ",continuation="
                    + summary_.post_resource_continuation
                    + ",signon_phase=" + summary_.signon_phase
                    + ",session_count=" + std::to_string(summary_.session_count)
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
        if (world_baselines_enabled_)
        {
            hl::common::Logger::Info(
                hl::common::LogCategory::Summary,
                "goldsrc_world_baseline_summary: enabled=1,negative_proof="
                    + std::string(
                        world_baselines_negative_proof_ ? "1" : "0")
                    + ",contract_verified="
                    + std::string(
                        summary_.baseline_contract_verified
                            ? "true"
                            : "false")
                    + ",message_order_verified="
                    + std::string(
                        summary_.baseline_message_order_verified
                            ? "true"
                            : "false")
                    + ",source=runtime_map,build_attempts="
                    + std::to_string(summary_.baseline_build_attempts)
                    + ",generations="
                    + std::to_string(summary_.baseline_generations)
                    + ",cached_reuses="
                    + std::to_string(
                        summary_.baseline_cached_outcome_reuses)
                    + ",callback_calls="
                    + std::to_string(summary_.baseline_callback_calls)
                    + ",instanced_callback_calls="
                    + std::to_string(
                        summary_.baseline_instanced_callback_calls)
                    + ",entity_count="
                    + std::to_string(summary_.baseline_entity_count)
                    + ",player_count="
                    + std::to_string(summary_.baseline_player_count)
                    + ",non_player_count="
                    + std::to_string(summary_.baseline_non_player_count)
                    + ",instance_count="
                    + std::to_string(summary_.baseline_instance_count)
                    + ",payload_bytes="
                    + std::to_string(summary_.baseline_payload_bytes)
                    + ",queued="
                    + std::to_string(summary_.baseline_queued)
                    + ",sent=" + std::to_string(summary_.baseline_sent)
                    + ",resent=" + std::to_string(summary_.baseline_resent)
                    + ",acked=" + std::to_string(summary_.baseline_acked)
                    + ",fragmented="
                    + std::string(
                        summary_.baseline_fragmented ? "true" : "false")
                    + ",fragment_count="
                    + std::to_string(summary_.baseline_fragment_count)
                    + ",fragment_acknowledged_count="
                    + std::to_string(
                        summary_
                            .baseline_fragment_acknowledged_count)
                    + ",sendents_received="
                    + std::to_string(summary_.send_entities_received)
                    + ",sendents_delivered="
                    + std::to_string(summary_.send_entities_delivered)
                    + ",previous_boundary_resolved="
                    + std::string(
                        summary_
                            .previous_server_baseline_boundary_resolved
                            ? "true"
                            : "false")
                    + ",next_boundary="
                    + summary_.next_observed_boundary
                    + ",signon_phase=" + summary_.signon_phase
                    + ",session_count="
                    + std::to_string(summary_.session_count)
                    + ",put_in_server="
                    + std::string(summary_.put_in_server ? "1" : "0")
                    + ",spawned="
                    + std::string(summary_.spawned ? "1" : "0")
                    + ",active="
                    + std::string(summary_.active ? "1" : "0")
                    + ",clean_shutdown=1");
        }
        if (first_snapshot_enabled_)
        {
            hl::common::Logger::Info(
                hl::common::LogCategory::Summary,
                "goldsrc_first_snapshot_summary: enabled=1,negative_proof="
                    + std::string(
                        first_snapshot_negative_proof_ ? "1" : "0")
                    + ",contract_verified="
                    + std::string(
                        summary_.snapshot_contract_verified
                            ? "true"
                            : "false")
                    + ",message_order_verified="
                    + std::string(
                        summary_.snapshot_message_order_verified
                            ? "true"
                            : "false")
                    + ",frame_ack_contract_verified="
                    + std::string(
                        summary_.frame_ack_contract_verified
                            ? "true"
                            : "false")
                    + ",messages=svc_time,svc_clientdata,svc_packetentities"
                    + ",reliability=unreliable"
                    + ",server_time="
                    + std::to_string(summary_.first_snapshot_server_time)
                    + ",clientdata="
                    + std::string(
                        summary_.clientdata_implemented
                            ? "implemented"
                            : "missing")
                    + ",weapon_data="
                    + std::string(
                        summary_.weapon_data_required
                            ? "required"
                            : "not_required")
                    + ",packet_entities="
                    + std::string(
                        summary_.packet_entities_implemented
                            ? "implemented"
                            : "missing")
                    + ",entity_count="
                    + std::to_string(summary_.first_snapshot_entity_count)
                    + ",payload_bytes="
                    + std::to_string(
                        summary_.first_snapshot_payload_bytes)
                    + ",frame_id="
                    + std::to_string(summary_.first_snapshot_frame_id)
                    + ",visibility_policy="
                    + summary_.first_snapshot_visibility_policy
                    + ",frame_history_implemented="
                    + std::string(
                        summary_.frame_history_implemented
                            ? "true"
                            : "false")
                    + ",frame_history_depth="
                    + std::to_string(summary_.frame_history_depth)
                    + ",frame_ack_source="
                    + summary_.frame_ack_source
                    + ",prepared="
                    + std::to_string(summary_.first_snapshot_prepared)
                    + ",sent="
                    + std::to_string(summary_.first_snapshot_sent)
                    + ",acked="
                    + std::to_string(summary_.first_snapshot_acked)
                    + ",duplicate_ack="
                    + std::to_string(
                        summary_.duplicate_frame_acknowledgements)
                    + ",unknown_frame="
                    + std::to_string(summary_.unknown_frame_references)
                    + ",future_frame="
                    + std::to_string(summary_.future_frame_references)
                    + ",evicted_frame="
                    + std::to_string(summary_.evicted_frame_references)
                    + ",stale_frame="
                    + std::to_string(summary_.stale_frame_references)
                    + ",previous_boundary_resolved="
                    + std::string(
                        summary_.previous_first_snapshot_boundary_resolved
                            ? "true"
                            : "false")
                    + ",advanced_past_previous_boundary="
                    + std::string(
                        summary_.stock_advanced_past_previous_boundary
                            ? "true"
                            : "false")
                    + ",next_boundary="
                    + summary_.next_observed_boundary
                    + ",signon_phase=" + summary_.signon_phase
                    + ",session_count="
                    + std::to_string(summary_.session_count)
                    + ",put_in_server="
                    + std::string(summary_.put_in_server ? "1" : "0")
                    + ",spawned="
                    + std::string(summary_.spawned ? "1" : "0")
                    + ",active="
                    + std::string(summary_.active ? "1" : "0")
                    + ",server_still_responsive="
                    + std::string(
                        summary_.server_still_responsive ? "true" : "false")
                    + ",clean_shutdown=1");
        }
        if (continuous_snapshots_enabled_)
        {
            hl::common::Logger::Info(
                hl::common::LogCategory::Summary,
                "goldsrc_continuous_snapshot_summary: enabled=1,negative_proof="
                    + std::string(
                        continuous_snapshots_negative_proof_ ? "1" : "0")
                    + ",contract_verified="
                    + std::string(
                        summary_.continuous_snapshot_contract_verified
                            ? "true"
                            : "false")
                    + ",delta_contract_verified="
                    + std::string(
                        summary_.delta_packet_entities_contract_verified
                            ? "true"
                            : "false")
                    + ",frame_reference_contract_verified="
                    + std::string(
                        summary_
                                .frame_reference_resolution_contract_verified
                            ? "true"
                            : "false")
                    + ",loss_recovery_contract_verified="
                    + std::string(
                        summary_.loss_recovery_contract_verified
                            ? "true"
                            : "false")
                    + ",snapshot_rate_hz="
                    + std::to_string(summary_.snapshot_rate_hz)
                    + ",snapshot_interval_ms="
                    + std::to_string(summary_.snapshot_interval_ms)
                    + ",minimum_snapshot_gap_ms="
                    + std::to_string(summary_.minimum_snapshot_gap_ms)
                    + ",median_snapshot_interval_ms="
                    + std::to_string(
                        summary_.median_snapshot_interval_ms)
                    + ",p95_snapshot_interval_ms="
                    + std::to_string(
                        summary_.p95_snapshot_interval_ms)
                    + ",maximum_snapshot_gap_ms="
                    + std::to_string(summary_.maximum_snapshot_gap_ms)
                    + ",snapshot_burst_count="
                    + std::to_string(summary_.snapshot_burst_count)
                    + ",missed_snapshot_intervals="
                    + std::to_string(summary_.missed_snapshot_intervals)
                    + ",generated="
                    + std::to_string(
                        summary_.continuous_snapshots_generated)
                    + ",sent="
                    + std::to_string(summary_.continuous_snapshots_sent)
                    + ",acked="
                    + std::to_string(summary_.continuous_snapshots_acked)
                    + ",full="
                    + std::to_string(summary_.continuous_full_snapshots + 1)
                    + ",delta="
                    + std::to_string(summary_.continuous_delta_snapshots)
                    + ",full_fallbacks="
                    + std::to_string(
                        summary_.continuous_full_fallbacks)
                    + ",delta_recoveries="
                    + std::to_string(
                        summary_.continuous_delta_recoveries)
                    + ",distinct_frame_references="
                    + std::to_string(summary_.distinct_frame_references)
                    + ",server_time_monotonic="
                    + std::string(
                        summary_.continuous_server_time_monotonic
                            ? "true"
                            : "false")
                    + ",frame_ids_advanced="
                    + std::string(
                        summary_.continuous_frame_ids_advanced
                            ? "true"
                            : "false")
                    + ",newest_acknowledged_base_selected="
                    + std::string(
                        summary_.newest_acknowledged_base_selected
                            ? "true"
                            : "false")
                    + ",frame_history_bounded="
                    + std::string(
                        summary_.continuous_frame_history_bounded
                            ? "true"
                            : "false")
                    + ",streaming="
                    + std::string(
                        summary_.continuous_snapshot_streaming
                            ? "true"
                            : "false")
                    + ",stable="
                    + std::string(
                        summary_.continuous_snapshot_stable
                            ? "true"
                            : "false")
                    + ",next_boundary="
                    + summary_.next_observed_boundary
                    + ",session_count="
                    + std::to_string(summary_.session_count)
                    + ",put_in_server="
                    + std::string(summary_.put_in_server ? "1" : "0")
                    + ",spawned="
                    + std::string(summary_.spawned ? "1" : "0")
                    + ",active="
                    + std::string(summary_.active ? "1" : "0")
                    + ",clean_shutdown=1");
        }
        if (player_lifecycle_enabled_)
        {
            hl::common::Logger::Info(
                hl::common::LogCategory::Summary,
                "goldsrc_player_lifecycle_summary: enabled=1,negative_proof="
                    + std::string(
                        player_lifecycle_negative_proof_ ? "1" : "0")
                    + ",contract_verified="
                    + std::string(
                        summary_.player_lifecycle_contract_verified
                            ? "true" : "false")
                    + ",callback_order_verified="
                    + std::string(
                        summary_.game_dll_callback_order_verified
                            ? "true" : "false")
                    + ",view_contract_verified="
                    + std::string(
                        summary_.view_entity_contract_verified
                            ? "true" : "false")
                    + ",trigger=" + summary_.player_lifecycle_trigger
                    + ",phase=" + summary_.player_lifecycle_phase
                    + ",client_edict_index="
                    + std::to_string(summary_.client_edict_index)
                    + ",userinfo_changed_calls="
                    + std::to_string(
                        summary_.client_userinfo_changed_calls)
                    + ",client_connect_calls="
                    + std::to_string(summary_.client_connect_calls)
                    + ",client_connect_accepts="
                    + std::to_string(summary_.client_connect_accepts)
                    + ",client_connect_rejections="
                    + std::to_string(summary_.client_connect_rejections)
                    + ",client_put_in_server_calls="
                    + std::to_string(
                        summary_.client_put_in_server_calls)
                    + ",client_put_in_server_successes="
                    + std::to_string(
                        summary_.client_put_in_server_successes)
                    + ",private_data_ready="
                    + std::string(
                        summary_.player_private_data_ready
                            ? "true" : "false")
                    + ",player_entity_ready="
                    + std::string(
                        summary_.player_entity_ready ? "true" : "false")
                    + ",player_spawned="
                    + std::string(
                        summary_.player_spawned ? "true" : "false")
                    + ",view_entity="
                    + std::to_string(summary_.player_view_entity)
                    + ",player_snapshot_frames="
                    + std::to_string(summary_.player_snapshot_frames)
                    + ",first_player_snapshot_frame_id="
                    + std::to_string(
                        summary_.first_player_snapshot_frame_id)
                    + ",player_adds="
                    + std::to_string(summary_.player_snapshot_adds)
                    + ",player_updates="
                    + std::to_string(summary_.player_snapshot_updates)
                    + ",player_removes="
                    + std::to_string(summary_.player_snapshot_removes)
                    + ",clientdata_implemented="
                    + std::string(
                        summary_.player_clientdata_implemented
                            ? "true" : "false")
                    + ",clientdata_playerstate_consistent="
                    + std::string(
                        summary_.clientdata_playerstate_consistent
                            ? "true" : "false")
                    + ",signon_complete="
                    + std::string(
                        summary_.player_signon_complete
                            ? "true" : "false")
                    + ",previous_boundary_resolved="
                    + std::string(
                        summary_
                                .previous_player_lifecycle_boundary_resolved
                            ? "true" : "false")
                    + ",next_boundary="
                    + summary_.next_observed_boundary
                    + ",movement_executed="
                    + std::string(
                        summary_.movement_executed ? "true" : "false")
                    + ",gameplay_active="
                    + std::string(
                        summary_.gameplay_active ? "true" : "false")
                    + ",disconnect_calls="
                    + std::to_string(summary_.client_disconnect_calls)
                    + ",server_still_responsive="
                    + std::string(
                        summary_.server_still_responsive
                            ? "true" : "false")
                    + ",clean_shutdown=1");
        }
        if (pmove_enabled_)
        {
            hl::common::Logger::Info(
                hl::common::LogCategory::Summary,
                "goldsrc_pmove_summary: enabled=1"
                    + std::string(
                        ",negative_proof="
                        + std::string(
                            pmove_negative_proof_ ? "1" : "0"))
                    + ",command_contract="
                    + std::string(
                        summary_.command_execution_contract_verified
                            ? "true" : "false")
                    + ",pm_init_calls="
                    + std::to_string(summary_.pm_init_call_count)
                    + ",pm_move_calls="
                    + std::to_string(summary_.pmove_calls)
                    + ",player_trace_calls="
                    + std::to_string(summary_.pmove_player_trace_calls)
                    + ",commands_received="
                    + std::to_string(summary_.movement_commands_received)
                    + ",commands_executed="
                    + std::to_string(summary_.movement_commands_executed)
                    + ",backup_replayed="
                    + std::to_string(summary_.backup_commands_replayed)
                    + ",duplicates_suppressed="
                    + std::to_string(
                        summary_.duplicate_commands_suppressed)
                    + ",move_packets_observed="
                    + std::to_string(summary_.move_packets_observed)
                    + ",move_packets_validated="
                    + std::to_string(summary_.move_packets_validated)
                    + ",move_packets_executed="
                    + std::to_string(summary_.move_packets_executed)
                    + ",temporary_clock_rejections="
                    + std::to_string(
                        summary_.move_packets_temporarily_rejected)
                    + ",permanent_move_rejections="
                    + std::to_string(
                        summary_.move_packets_permanently_rejected)
                    + ",clock_recoveries="
                    + std::to_string(summary_.command_clock_recoveries)
                    + ",clock_resynchronizations="
                    + std::to_string(
                        summary_.command_clock_resynchronizations)
                    + ",raw_netchan_sequence_gaps="
                    + std::to_string(
                        summary_.raw_netchan_sequence_gaps)
                    + ",raw_netchan_gap_move_replays="
                    + std::to_string(
                        summary_.raw_netchan_gap_move_replays)
                    + ",synthetic_replays="
                    + std::to_string(
                        summary_.synthetic_command_replays)
                    + ",maximum_move_execution_gap_ms="
                    + std::to_string(
                        summary_.maximum_move_execution_gap_ms)
                    + ",movement_discontinuities="
                    + std::to_string(
                        summary_.movement_discontinuity_count)
                    + ",authoritative_position_jumps="
                    + std::to_string(
                        summary_.authoritative_position_jump_count)
                    + ",receive_before_snapshot="
                    + std::string(
                        summary_
                                .receive_before_snapshot_in_streaming_phase
                            ? "true" : "false")
                    + ",snapshot_move_ack_coherence="
                    + std::string(
                        summary_.snapshot_move_ack_coherence
                            ? "true" : "false")
                    + ",same_host_frame_movement_snapshots="
                    + std::to_string(
                        summary_.same_host_frame_movement_snapshots)
                    + ",rollbacks="
                    + std::to_string(summary_.movement_rollbacks)
                    + ",movement_ready="
                    + std::string(summary_.movement_ready ? "true" : "false")
                    + ",movement_executed="
                    + std::string(
                        summary_.movement_executed ? "true" : "false")
                    + ",movement_snapshots="
                    + std::to_string(summary_.movement_snapshots_sent)
                    + ",authoritative_movement="
                    + std::string(
                        summary_.stock_authoritative_movement
                            ? "true" : "false")
                    + ",forward_input="
                    + std::string(
                        summary_.stock_forward_input ? "true" : "false")
                    + ",backward_input="
                    + std::string(
                        summary_.stock_backward_input ? "true" : "false")
                    + ",strafe_input="
                    + std::string(
                        summary_.stock_strafe_input ? "true" : "false")
                    + ",jump_input="
                    + std::string(
                        summary_.stock_jump_input ? "true" : "false")
                    + ",duck_input="
                    + std::string(
                        summary_.stock_duck_input ? "true" : "false")
                    + ",forward_movement="
                    + std::string(
                        summary_.stock_forward_movement ? "true" : "false")
                    + ",backward_movement="
                    + std::string(
                        summary_.stock_backward_movement ? "true" : "false")
                    + ",strafe_movement="
                    + std::string(
                        summary_.stock_strafe_movement ? "true" : "false")
                    + ",jump="
                    + std::string(summary_.stock_jump ? "true" : "false")
                    + ",duck="
                    + std::string(summary_.stock_duck ? "true" : "false")
                    + ",grounding="
                    + std::string(
                        summary_.stock_grounding ? "true" : "false")
                    + ",persistent="
                    + std::string(pmove_persistent_ ? "true" : "false")
                    + ",movement_milestone="
                    + std::string(
                        summary_.movement_milestone_reached
                            ? "true" : "false")
                    + ",manual_session_phase="
                    + summary_.manual_session_phase
                    + ",manual_session_heartbeats="
                    + std::to_string(summary_.manual_session_heartbeats)
                    + ",manual_session_disconnects="
                    + std::to_string(summary_.manual_session_disconnects)
                    + ",manual_session_reconnects="
                    + std::to_string(summary_.manual_session_reconnects)
                    + ",clc_moves_after_milestone="
                    + std::to_string(
                        manual_session_lifecycle_
                            .clc_moves_after_milestone())
                    + ",snapshots_after_milestone="
                    + std::to_string(
                        manual_session_lifecycle_
                            .snapshots_after_milestone())
                    + ",next_boundary="
                    + summary_.next_observed_boundary
                    + ",gameplay_active="
                    + (summary_.gameplay_active ? "true" : "false")
                    + ",clean_shutdown=1");
        }
        if (combat_enabled_)
        {
            for (std::size_t client_slot = 1u;
                 client_slot <= summary_.client_replication.size();
                 ++client_slot)
            {
                SyncPmoveSummary(client_slot);
            }
            const hl::game_api::GoldSrcClientReplicationSummary empty{};
            const auto& client_a = summary_.client_replication.size() >= 1u
                ? summary_.client_replication[0]
                : empty;
            const auto& client_b = summary_.client_replication.size() >= 2u
                ? summary_.client_replication[1]
                : empty;
            const auto* const combat_a =
                pmove_runtime_.CombatDiagnostics(1u);
            const auto* const combat_b =
                pmove_runtime_.CombatDiagnostics(2u);
            const auto engine_callback_count =
                [this](std::string_view callback_name) noexcept
                {
                    const auto it = state_.callback_counts.find(
                        std::string(callback_name));
                    return it != state_.callback_counts.end()
                        ? it->second
                        : 0u;
                };
            hl::common::Logger::Info(
                hl::common::LogCategory::Summary,
                std::string("goldsrc_combat_summary: enabled=1")
                    + ",callback_order=cmdstart_prethink_think_pmove_commit_postthink_cmdend"
                    + ",callback_order_verified="
                    + (summary_.combat_callback_order_verified
                        ? "true" : "false")
                    + ",time_contract_verified="
                    + (summary_.combat_time_contract_verified
                        ? "true" : "false")
                    + ",weapon_postframe_verified="
                    + (summary_.weapon_postframe_contract_verified
                        ? "true" : "false")
                    + ",weapon_replication_verified="
                    + (summary_.weapon_replication_contract_verified
                        ? "true" : "false")
                    + ",player_aware_trace="
                    + (summary_.player_aware_trace ? "true" : "false")
                    + ",per_client_weapondata="
                    + (summary_.per_client_weapondata ? "true" : "false")
                    + ",frame_history_weapondata="
                    + (summary_.frame_history_weapondata
                        ? "true" : "false")
                    + ",trace_policy=nearest_player_before_static_world"
                    ",start_frame_calls="
                    + std::to_string(summary_.start_frame_call_count)
                    + ",scheduled_entity_think_calls="
                    + std::to_string(combat_entity_think_calls_)
                    + ",attack_received="
                    + std::to_string(summary_.attack_commands_received)
                    + ",attack_executed="
                    + std::to_string(summary_.attack_commands_executed)
                    + ",duplicate_attack_suppressed="
                    + std::to_string(
                        summary_.duplicate_attack_commands_suppressed)
                    + ",trace_calls="
                    + std::to_string(summary_.combat_trace_calls)
                    + ",player_hits="
                    + std::to_string(summary_.combat_player_hits)
                    + ",world_occlusions="
                    + std::to_string(summary_.combat_world_occlusions)
                    + ",playback_events="
                    + std::to_string(summary_.playback_event_calls)
                    + ",vec_to_angles_calls="
                    + std::to_string(
                        engine_callback_count("pfnVecToAngles"))
                    + ",crosshair_angle_calls="
                    + std::to_string(
                        engine_callback_count("pfnCrosshairAngle"))
                    + ",a_phase=" + client_a.combat_phase
                    + ",a_prethink="
                    + std::to_string(client_a.player_prethink_calls)
                    + ",a_postthink="
                    + std::to_string(client_a.player_postthink_calls)
                    + ",a_callback_failures="
                    + std::to_string(
                        combat_a != nullptr
                            ? combat_a->callback_failures
                            : 0u)
                    + ",a_active_weapon="
                    + std::to_string(client_a.active_weapon_id)
                    + ",a_glock_present="
                    + (client_a.glock_entity_present ? "true" : "false")
                    + ",a_clip_before="
                    + std::to_string(client_a.initial_glock_clip)
                    + ",a_clip_after="
                    + std::to_string(client_a.glock_clip)
                    + ",a_clip_min="
                    + std::to_string(client_a.minimum_glock_clip)
                    + ",a_reserve_before="
                    + std::to_string(client_a.initial_glock_reserve_ammo)
                    + ",a_reserve_after="
                    + std::to_string(client_a.glock_reserve_ammo)
                    + ",a_health_before="
                    + std::to_string(client_a.initial_authoritative_health)
                    + ",a_health_after="
                    + std::to_string(client_a.authoritative_health)
                    + ",a_clientdata_health_after="
                    + std::to_string(client_a.clientdata_health)
                    + ",a_playback_events="
                    + std::to_string(client_a.playback_event_calls)
                    + ",a_player_hits="
                    + std::to_string(client_a.player_trace_hits)
                    + ",a_world_occlusions="
                    + std::to_string(client_a.world_trace_occlusions)
                    + ",a_shot_traces="
                    + std::to_string(client_a.weapon_range_trace_calls)
                    + ",a_shooter_ignored="
                    + std::to_string(
                        client_a.weapon_range_shooter_ignored)
                    + ",a_shot_player_hits="
                    + std::to_string(client_a.weapon_range_player_hits)
                    + ",a_shot_world_hits="
                    + std::to_string(client_a.weapon_range_world_hits)
                    + ",a_last_target="
                    + std::to_string(client_a.last_trace_target_slot)
                    + ",a_weapondata_frames="
                    + std::to_string(client_a.weapondata_frames)
                    + ",b_phase=" + client_b.combat_phase
                    + ",b_prethink="
                    + std::to_string(client_b.player_prethink_calls)
                    + ",b_postthink="
                    + std::to_string(client_b.player_postthink_calls)
                    + ",b_callback_failures="
                    + std::to_string(
                        combat_b != nullptr
                            ? combat_b->callback_failures
                            : 0u)
                    + ",b_active_weapon="
                    + std::to_string(client_b.active_weapon_id)
                    + ",b_glock_present="
                    + (client_b.glock_entity_present ? "true" : "false")
                    + ",b_clip_before="
                    + std::to_string(client_b.initial_glock_clip)
                    + ",b_clip_after="
                    + std::to_string(client_b.glock_clip)
                    + ",b_clip_min="
                    + std::to_string(client_b.minimum_glock_clip)
                    + ",b_health_before="
                    + std::to_string(client_b.initial_authoritative_health)
                    + ",b_health_after="
                    + std::to_string(client_b.authoritative_health)
                    + ",b_health_min="
                    + std::to_string(client_b.minimum_authoritative_health)
                    + ",b_clientdata_health_after="
                    + std::to_string(client_b.clientdata_health)
                    + ",b_world_occlusions="
                    + std::to_string(client_b.world_trace_occlusions)
                    + ",b_shot_traces="
                    + std::to_string(client_b.weapon_range_trace_calls)
                    + ",b_shooter_ignored="
                    + std::to_string(
                        client_b.weapon_range_shooter_ignored)
                    + ",b_shot_player_hits="
                    + std::to_string(client_b.weapon_range_player_hits)
                    + ",b_shot_world_hits="
                    + std::to_string(client_b.weapon_range_world_hits)
                    + ",b_deadflag="
                    + std::to_string(client_b.authoritative_deadflag)
                    + ",b_weapondata_frames="
                    + std::to_string(client_b.weapondata_frames)
                    + ",gameplay_active="
                    + (summary_.gameplay_active ? "true" : "false")
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
        if (state_.server_state.maxclients >= 2)
        {
            RefreshTwoClientReplicationSummary();
            for (const auto& client : summary_.client_replication)
            {
                if (client.admission_count == 0)
                {
                    continue;
                }
                hl::common::Logger::Info(
                    hl::common::LogCategory::Summary,
                    "goldsrc_client_replication_summary: slot="
                        + std::to_string(client.slot)
                        + ",edict=" + std::to_string(client.edict)
                        + ",admissions="
                        + std::to_string(client.admission_count)
                        + ",disconnects="
                        + std::to_string(client.disconnect_count)
                        + ",reconnects="
                        + std::to_string(client.reconnect_count)
                        + ",connected="
                        + (client.connected ? "true" : "false")
                        + ",put_in_server="
                        + (client.put_in_server ? "true" : "false")
                        + ",spawned="
                        + (client.spawned ? "true" : "false")
                        + ",signon_complete="
                        + (client.signon_complete ? "true" : "false")
                        + ",private_data_ready="
                        + (client.private_data_ready ? "true" : "false")
                        + ",snapshots_sent="
                        + std::to_string(client.snapshots_sent)
                        + ",snapshots_due="
                        + std::to_string(client.snapshots_due)
                        + ",snapshots_deferred="
                        + std::to_string(client.snapshots_deferred)
                        + ",snapshots_starved="
                        + std::to_string(client.snapshots_starved)
                        + ",snapshot_interval_min_ms="
                        + std::to_string(
                            client.snapshot_interval_minimum_ms)
                        + ",snapshot_interval_median_ms="
                        + std::to_string(client.snapshot_interval_median_ms)
                        + ",snapshot_interval_p95_ms="
                        + std::to_string(client.snapshot_interval_p95_ms)
                        + ",snapshot_interval_max_ms="
                        + std::to_string(
                            client.snapshot_interval_maximum_ms)
                        + ",frames_acknowledged="
                        + std::to_string(client.frames_acknowledged)
                        + ",move_packets_received="
                        + std::to_string(client.move_packets_received)
                        + ",move_packets_validated="
                        + std::to_string(client.move_packets_validated)
                        + ",move_packets_executed="
                        + std::to_string(client.move_packets_executed)
                        + ",move_packets_temporarily_rejected="
                        + std::to_string(
                            client.move_packets_temporarily_rejected)
                        + ",pmove_calls="
                        + std::to_string(client.pmove_calls)
                        + ",movement_commands_executed="
                        + std::to_string(client.movement_commands_executed)
                        + ",movement_snapshots="
                        + std::to_string(client.movement_snapshots_sent)
                        + ",remote_adds="
                        + std::to_string(client.remote_player_adds)
                        + ",remote_updates="
                        + std::to_string(client.remote_player_updates)
                        + ",remote_removes="
                        + std::to_string(client.remote_player_removes)
                        + ",remote_readds="
                        + std::to_string(client.remote_player_readds)
                        + ",remote_origin_discontinuities="
                        + std::to_string(
                            client.remote_origin_discontinuities)
                        + ",maximum_remote_snapshot_gap_ms="
                        + std::to_string(
                            client.maximum_remote_snapshot_gap_ms)
                        + ",remote_update_interval_median_ms="
                        + std::to_string(
                            client.remote_update_interval_median_ms)
                        + ",remote_update_interval_p95_ms="
                        + std::to_string(
                            client.remote_update_interval_p95_ms)
                        + ",remote_update_interval_max_ms="
                        + std::to_string(
                            client.maximum_remote_snapshot_gap_ms)
                        + ",remote_origin_delta_last="
                        + std::to_string(client.last_remote_origin_delta)
                        + ",remote_origin_delta_max="
                        + std::to_string(client.maximum_remote_origin_delta)
                        + ",remote_animtime_monotonic="
                        + (client.remote_animtime_monotonic ? "true" : "false")
                        + ",permanent_remote_nointerp="
                        + (client.permanent_remote_nointerp ? "true" : "false")
                        + ",remote_entity_present_consecutively="
                        + (client.remote_entity_present_consecutively
                            ? "true" : "false")
                        + ",ordinary_ack_sent="
                        + std::to_string(client.ordinary_ack_datagrams_sent)
                        + ",ordinary_ack_coalesced="
                        + std::to_string(
                            client.ordinary_ack_datagrams_coalesced)
                        + ",ordinary_ack_deferred="
                        + std::to_string(
                            client.ordinary_ack_datagrams_deferred)
                        + ",last_successful_move_time_ms="
                        + std::to_string(
                            client.last_successful_move_time_ms)
                        + ",last_successful_snapshot_time_ms="
                        + std::to_string(
                            client.last_successful_snapshot_time_ms)
                        + ",last_successful_frame_ack_time_ms="
                        + std::to_string(
                            client.last_successful_frame_ack_time_ms)
                        + ",slot_reuse_clean="
                        + (client.slot_reuse_clean ? "true" : "false"));
            }
            hl::common::Logger::Info(
                hl::common::LogCategory::Summary,
                "goldsrc_two_client_replication_summary: max_clients="
                    + std::to_string(summary_.max_clients)
                    + ",connected_clients="
                    + std::to_string(summary_.connected_client_count)
                    + ",routing=per_slot_endpoint_netchan_generation"
                    + ",visibility_policy="
                    + summary_.two_client_visibility_policy
                    + ",receive_policy="
                    + summary_.multi_client_receive_policy
                    + ",send_policy="
                    + summary_.multi_client_send_policy
                    + ",distinct_edicts="
                    + (summary_.two_distinct_edicts ? "true" : "false")
                    + ",distinct_private_data="
                    + (summary_.two_distinct_private_data ? "true" : "false")
                    + ",mutual_visibility="
                    + (summary_.mutual_player_visibility ? "true" : "false")
                    + ",remote_movement_smooth="
                    + (summary_.remote_player_movement_smooth
                        ? "true" : "false")
                    + ",remote_interpolation_contract_verified="
                    + (summary_.remote_interpolation_contract_verified
                        ? "true" : "false")
                    + ",empty_ack_coalescing="
                    + (summary_.empty_ack_coalescing ? "true" : "false")
                    + ",snapshot_capacity_reserved="
                    + (summary_.snapshot_capacity_reserved ? "true" : "false")
                    + ",reliable_priority_preserved="
                    + (summary_.reliable_priority_preserved ? "true" : "false")
                    + ",per_client_snapshot_fairness="
                    + (summary_.per_client_snapshot_fairness ? "true" : "false")
                    + ",ordinary_ack_datagrams_sent="
                    + std::to_string(summary_.ordinary_ack_datagrams_sent)
                    + ",ordinary_ack_datagrams_coalesced="
                    + std::to_string(summary_.ordinary_ack_datagrams_coalesced)
                    + ",ordinary_ack_datagrams_deferred="
                    + std::to_string(summary_.ordinary_ack_datagrams_deferred)
                    + ",reliable_datagrams_sent="
                    + std::to_string(summary_.reliable_datagrams_sent)
                    + ",snapshot_datagrams_sent="
                    + std::to_string(summary_.snapshot_datagrams_sent)
                    + ",snapshot_budget_starvations="
                    + std::to_string(summary_.snapshot_budget_starvations)
                    + ",maximum_snapshot_starvation_frames="
                    + std::to_string(
                        summary_.maximum_consecutive_snapshot_starvation_frames)
                    + ",simultaneous_movement="
                    + (summary_.simultaneous_movement ? "true" : "false")
                    + ",disconnect_isolation="
                    + (summary_.disconnect_isolation ? "true" : "false")
                    + ",reconnect_slot_reuse="
                    + (summary_.reconnect_slot_reuse ? "true" : "false")
                    + ",cross_client_state_leak="
                    + (summary_.cross_client_state_leak ? "true" : "false")
                    + ",gameplay_active="
                    + (summary_.gameplay_active ? "true" : "false")
                    + ",clean_shutdown=1");
        }
    }

private:
    enum class StockTestFallAcceptancePhase : std::uint8_t
    {
        kInactive,
        kLandingArranged,
        kLandingObserved,
        kResetObserved,
        kDamageArranged,
        kDamageObserved,
        kComplete,
        kFailed,
    };

    struct StockTestFallAcceptance final
    {
        StockTestFallAcceptancePhase phase =
            StockTestFallAcceptancePhase::kInactive;
        int slot = 0;
        std::uint64_t session_generation = 0u;
        float health_before = 0.0f;
        Vector responsiveness_origin_a = Vector(0.0f, 0.0f, 0.0f);
        Vector responsiveness_origin_b = Vector(0.0f, 0.0f, 0.0f);
        std::uint64_t responsiveness_generation_a = 0u;
        std::uint64_t responsiveness_generation_b = 0u;
        bool responsiveness_a = false;
        bool responsiveness_b = false;
    };

    struct StockTestSpawnAnchor final
    {
        bool valid = false;
        bool stability_observed = false;
        double stationary_since = 0.0;
        std::uint64_t session_generation = 0u;
        std::uint16_t edict = 0u;
        Vector candidate_origin = Vector(0.0f, 0.0f, 0.0f);
        Vector origin = Vector(0.0f, 0.0f, 0.0f);
        Vector angles = Vector(0.0f, 0.0f, 0.0f);
        Vector v_angle = Vector(0.0f, 0.0f, 0.0f);
        Vector mins = Vector(0.0f, 0.0f, 0.0f);
        Vector maxs = Vector(0.0f, 0.0f, 0.0f);
        int flags = 0;
        int ground_edict = 0;
    };

    static std::uint64_t MonotonicMilliseconds(
        std::chrono::steady_clock::time_point value) noexcept
    {
        const auto count = std::chrono::duration_cast<std::chrono::milliseconds>(
            value.time_since_epoch()).count();
        return count > 0 ? static_cast<std::uint64_t>(count) : 0u;
    }

    bool CallCombatStartFrame(double elapsed_seconds) noexcept
    {
        if (!combat_enabled_)
        {
            return true;
        }

        // GoldSrc calls StartFrame for an active server even when there are no
        // signed-on clients; client lifecycle must not gate host-frame work.
        if (!combat_frame_cadence_.BeginFrame(
                true,
                summary_.host_frames_pumped))
        {
            return true;
        }
        if (state_.dll_functions.pfnStartFrame == nullptr)
        {
            return false;
        }
        const double fallback =
            std::isfinite(state_.globalvars.frametime)
                && state_.globalvars.frametime > 0.0f
            ? state_.globalvars.frametime
            : 0.01;
        const double requested_frametime =
            std::isfinite(elapsed_seconds) && elapsed_seconds > 0.0
            ? elapsed_seconds
            : fallback;
        double bounded_frametime_value = requested_frametime;
        if (bounded_frametime_value < 0.0001)
        {
            bounded_frametime_value = 0.0001;
        }
        if (bounded_frametime_value > 0.25)
        {
            bounded_frametime_value = 0.25;
        }
        const float bounded_frametime =
            static_cast<float>(bounded_frametime_value);
        state_.globalvars.time = static_cast<float>(network_server_time_);
        state_.globalvars.frametime = bounded_frametime;
        const std::uint64_t callback_count_before =
            state_.total_engine_callback_calls;
        const std::size_t message_count_before =
            state_.frame_message_buffer.CompletedCount();
        if (!SafeCallStartFrameSeh(
                state_.dll_functions.pfnStartFrame,
                nullptr))
        {
            const bool callback_observed =
                state_.total_engine_callback_calls > callback_count_before;
            hl::game_api::detail::FrameCompletedMessageObservation message;
            const bool message_observed =
                state_.frame_message_buffer.LastCompletedSince(
                    message_count_before,
                    &message);
            hl::common::Logger::Error(
                hl::common::LogCategory::Server,
                std::string(
                    "goldsrc_combat_failed: reason=start-frame-callback")
                    + ",exception_caught=true"
                    + ",engine_callback_observed="
                    + std::string(callback_observed ? "true" : "false")
                    + ",last_engine_callback="
                    + (callback_observed
                        ? state_.last_engine_callback_name
                        : std::string("none"))
                    + ",message_observed="
                    + std::string(message_observed ? "true" : "false")
                    + ",message_destination="
                    + std::to_string(
                        message_observed ? message.destination : -1)
                    + ",message_type="
                    + std::to_string(
                        message_observed ? message.message_type : -1));
            return false;
        }
        ++combat_start_frame_calls_;
        return true;
    }

    bool DispatchCombatEntityThinks() noexcept
    {
        if (!combat_enabled_)
        {
            return true;
        }
        if (state_.dll_functions.pfnThink == nullptr)
        {
            return false;
        }

        std::array<int, kMaximumCombatEntityThinksPerHostFrame> due_indices{};
        std::size_t due_count = 0;
        const float current_time = static_cast<float>(network_server_time_);
        state_.globalvars.time = current_time;
        const int first_non_client = state_.server_state.maxclients + 1;
        const int max_entities = state_.edict_store.MaxEntities();
        std::size_t removal_count = 0u;
        const auto remove_marked_entities = [&]() noexcept {
            for (int entity_index = first_non_client;
                 entity_index < max_entities
                     && removal_count
                         < kMaximumCombatEntityRemovalsPerHostFrame;
                 ++entity_index)
            {
                edict_t* const entity =
                    state_.edict_store.EntityOfIndex(entity_index);
                if (entity == nullptr
                    || entity->free != FALSE
                    || (entity->v.flags & FL_KILLME) == 0)
                {
                    continue;
                }
                state_.edict_store.RemoveEntity(entity);
                ++removal_count;
            }
        };

        // Stock UTIL_Remove is deferred: it marks FL_KILLME and expects the
        // engine to retire the edict after callbacks have unwound. Drain marks
        // left by StartFrame or an earlier bounded frame before dispatching
        // more entity callbacks.
        remove_marked_entities();
        for (int entity_index = first_non_client;
             entity_index < max_entities
                 && due_count < due_indices.size();
             ++entity_index)
        {
            edict_t* const entity =
                state_.edict_store.EntityOfIndex(entity_index);
            if (entity == nullptr
                || entity->free != FALSE
                || entity->pvPrivateData == nullptr
                || (entity->v.flags & (FL_DORMANT | FL_KILLME)) != 0
                || entity->v.nextthink <= 0.0f
                || entity->v.nextthink > current_time)
            {
                continue;
            }
            due_indices[due_count++] = entity_index;
        }

        for (std::size_t due_index = 0; due_index < due_count; ++due_index)
        {
            edict_t* const entity = state_.edict_store.EntityOfIndex(
                due_indices[due_index]);
            if (entity == nullptr
                || entity->free != FALSE
                || entity->pvPrivateData == nullptr
                || (entity->v.flags & (FL_DORMANT | FL_KILLME)) != 0
                || entity->v.nextthink <= 0.0f
                || entity->v.nextthink > current_time)
            {
                continue;
            }

            entity->v.ltime = current_time;
            entity->v.nextthink = 0.0f;
            if (!SafeCallThinkSeh(
                    state_.dll_functions.pfnThink,
                    entity,
                    nullptr))
            {
                hl::common::Logger::Error(
                    hl::common::LogCategory::Server,
                    "goldsrc_combat_failed: reason=entity-think-callback"
                        ",entity="
                        + std::to_string(due_indices[due_index]));
                return false;
            }
            ++combat_entity_think_calls_;
            if (entity->free == FALSE
                && (entity->v.flags & FL_KILLME) != 0
                && removal_count
                    < kMaximumCombatEntityRemovalsPerHostFrame)
            {
                state_.edict_store.RemoveEntity(entity);
                ++removal_count;
            }
        }
        // A Think callback may mark a different entity. A final bounded pass
        // retires it while the current frame's reuse barrier is still active.
        remove_marked_entities();
        return true;
    }

    template <std::size_t Size>
    static std::uint32_t PercentileFromHistogram(
        const std::array<std::uint64_t, Size>& histogram,
        std::uint64_t samples,
        std::uint64_t percentile) noexcept
    {
        if (samples == 0u)
        {
            return 0u;
        }
        const std::uint64_t rank = (std::max<std::uint64_t>)(
            1u,
            (samples * percentile + 99u) / 100u);
        std::uint64_t cumulative = 0u;
        for (std::size_t bucket = 0u; bucket < histogram.size(); ++bucket)
        {
            cumulative += histogram[bucket];
            if (cumulative >= rank)
            {
                return static_cast<std::uint32_t>(bucket);
            }
        }
        return static_cast<std::uint32_t>(histogram.size() - 1u);
    }

    void SyncManualSessionSummary() noexcept
    {
        if (!pmove_persistent_)
        {
            return;
        }
        summary_.manual_session_phase = std::string(
            hl::network::NameFor(manual_session_lifecycle_.phase()));
        summary_.movement_milestone_reached =
            manual_session_lifecycle_.proof_complete();
        summary_.manual_session_active =
            manual_session_lifecycle_.can_pump();
        summary_.manual_session_disconnects = static_cast<int>(
            manual_session_lifecycle_.disconnect_count());
        summary_.manual_session_reconnects = static_cast<int>(
            manual_session_lifecycle_.reconnect_count());
    }

    void LogManualSessionShutdown(std::string_view reason) noexcept
    {
        if (!pmove_persistent_ || manual_session_shutdown_logged_)
        {
            return;
        }
        manual_session_shutdown_logged_ = true;
        summary_.manual_session_shutdown = true;
        summary_.manual_session_active = false;
        summary_.manual_session_phase = "stopped";
        hl::common::Logger::Info(
            hl::common::LogCategory::Server,
            "goldsrc_manual_session_shutdown: reason="
                + std::string(reason)
                + ",movement_milestone="
                + std::string(
                    summary_.movement_milestone_reached ? "1" : "0")
                + ",clean_runtime_stop=1");
    }

    static bool IsExternalStockTestControlPath(
        const std::filesystem::path& path)
    {
        if (path.empty() || !path.is_absolute() || !path.has_root_name()
            || !path.has_filename() || path.parent_path().empty())
        {
            return false;
        }
        std::error_code repository_error;
        std::filesystem::path repository = std::filesystem::weakly_canonical(
            std::filesystem::current_path(repository_error),
            repository_error);
        std::error_code parent_error;
        const std::filesystem::path candidate_parent =
            std::filesystem::weakly_canonical(
                path.parent_path(),
                parent_error);
        const std::filesystem::path candidate =
            (candidate_parent / path.filename()).lexically_normal();
        if (repository_error || parent_error || candidate.empty()
            || repository.empty())
        {
            return false;
        }
#if defined(_WIN32)
        std::wstring repository_text = repository.native();
        std::wstring candidate_text = candidate.native();
        std::transform(
            repository_text.begin(), repository_text.end(),
            repository_text.begin(), ::towlower);
        std::transform(
            candidate_text.begin(), candidate_text.end(),
            candidate_text.begin(), ::towlower);
#else
        const std::string repository_text = repository.native();
        const std::string candidate_text = candidate.native();
#endif
        if (candidate_text.size() <= repository_text.size()
            || candidate_text.compare(
                0u, repository_text.size(), repository_text) != 0)
        {
            return true;
        }
        const auto separator = candidate_text[repository_text.size()];
        return separator != std::filesystem::path::preferred_separator
            && separator != '/';
    }

    static double StockTestDistance(
        const Vector& a,
        const Vector& b) noexcept
    {
        const double dx = static_cast<double>(a.x) - b.x;
        const double dy = static_cast<double>(a.y) - b.y;
        const double dz = static_cast<double>(a.z) - b.z;
        return std::sqrt(dx * dx + dy * dy + dz * dz);
    }

    void UpdateStockTestSpawnAnchors()
    {
        if (!stock_test_control_file_.has_value())
        {
            return;
        }
        for (DedicatedPlayerRuntimeSlot& slot_state :
             state_.dedicated_multiplayer_foundation.slots)
        {
            if (slot_state.slot < 1
                || static_cast<std::size_t>(slot_state.slot)
                    > stock_test_spawn_anchors_.size()
                || !IsDedicatedSlotConnectedOccupant(slot_state)
                || !slot_state.goldsrc_player_lifecycle.put_in_server()
                || !slot_state.goldsrc_player_lifecycle.signon_complete())
            {
                continue;
            }
            const auto& binding =
                slot_state.goldsrc_player_lifecycle.binding();
            StockTestSpawnAnchor& anchor = stock_test_spawn_anchors_[
                static_cast<std::size_t>(slot_state.slot - 1)];
            if (anchor.session_generation
                != binding.network_session_generation)
            {
                anchor = {};
                anchor.session_generation =
                    binding.network_session_generation;
                anchor.edict = binding.edict_index;
            }
            if (anchor.valid)
            {
                continue;
            }
            edict_t* const player = state_.edict_store.EntityOfIndex(
                binding.edict_index);
            if (player == nullptr
                || !std::isfinite(player->v.origin.x)
                || !std::isfinite(player->v.origin.y)
                || !std::isfinite(player->v.origin.z)
                || !std::isfinite(player->v.velocity.x)
                || !std::isfinite(player->v.velocity.y)
                || !std::isfinite(player->v.velocity.z)
                || !pmove_runtime_.IsPlayerPositionValid(
                    player->v.origin,
                    (player->v.flags & FL_DUCKING) != 0 ? 1 : 0))
            {
                anchor.stability_observed = false;
                continue;
            }
            const double speed = std::sqrt(
                static_cast<double>(player->v.velocity.x)
                    * player->v.velocity.x
                + static_cast<double>(player->v.velocity.y)
                    * player->v.velocity.y
                + static_cast<double>(player->v.velocity.z)
                    * player->v.velocity.z);
            if (speed > 16.0)
            {
                anchor.stability_observed = false;
                continue;
            }
            if (!anchor.stability_observed
                || StockTestDistance(
                    anchor.candidate_origin,
                    player->v.origin) > 0.25)
            {
                anchor.stability_observed = true;
                anchor.stationary_since = network_server_time_;
                anchor.candidate_origin = player->v.origin;
                continue;
            }
            if (network_server_time_ - anchor.stationary_since < 0.5)
            {
                continue;
            }
            anchor.valid = true;
            anchor.origin = player->v.origin;
            anchor.angles = player->v.angles;
            anchor.v_angle = player->v.v_angle;
            anchor.mins = player->v.mins;
            anchor.maxs = player->v.maxs;
            anchor.flags = player->v.flags;
            anchor.ground_edict = player->v.groundentity == nullptr
                ? -1
                : state_.edict_store.IndexOf(player->v.groundentity);
            hl::common::Logger::Info(
                hl::common::LogCategory::Server,
                "goldsrc_stock_test_anchor: slot="
                    + std::to_string(slot_state.slot)
                    + ",session_generation="
                    + std::to_string(anchor.session_generation)
                    + ",edict=" + std::to_string(anchor.edict)
                    + ",status=ready");
        }
    }

    void AcknowledgeStockTestReset(
        std::string request_id,
        int slot,
        std::string_view status,
        std::string_view reason)
    {
        request_id = SanitizeLogField(std::move(request_id));
        summary_.latest_test_reset_request_id = request_id;
        summary_.latest_test_reset_slot = slot;
        summary_.latest_test_reset_reason = std::string(reason);
        if (status == "applied")
        {
            ++summary_.test_reset_applied;
        }
        else
        {
            ++summary_.test_reset_rejected;
        }
        hl::common::Logger::Info(
            hl::common::LogCategory::Server,
            "goldsrc_stock_test_reset_ack: request_id=" + request_id
                + ",slot=" + std::to_string(slot)
                + ",status=" + std::string(status)
                + ",reason=" + std::string(reason));
    }

    static void ApplyStockTestCombatPose(
        edict_t* player,
        const Vector& origin,
        float yaw,
        int ground_flags,
        edict_t* ground_entity)
    {
        if (player == nullptr)
        {
            return;
        }
        player->v.origin = origin;
        player->v.angles = Vector(0.0f, yaw, 0.0f);
        player->v.v_angle = Vector(0.0f, yaw, 0.0f);
        player->v.punchangle = Vector(0.0f, 0.0f, 0.0f);
        player->v.fixangle = TRUE;
        player->v.velocity = Vector(0.0f, 0.0f, 0.0f);
        player->v.basevelocity = Vector(0.0f, 0.0f, 0.0f);
        player->v.button = 0;
        player->v.oldbuttons = 0;
        player->v.flags = (player->v.flags & ~(FL_ONGROUND | FL_DUCKING))
            | (ground_flags & (FL_ONGROUND | FL_DUCKING));
        player->v.groundentity = ground_entity;
        for (int axis = 0; axis < 3; ++axis)
        {
            player->v.size[axis] = player->v.maxs[axis] - player->v.mins[axis];
            player->v.absmin[axis] =
                player->v.origin[axis] + player->v.mins[axis] - 1.0f;
            player->v.absmax[axis] =
                player->v.origin[axis] + player->v.maxs[axis] + 1.0f;
        }
    }

    bool ArrangeStockTestCombatPair(
        DedicatedPlayerRuntimeSlot& shooter_slot,
        DedicatedPlayerRuntimeSlot& target_slot,
        std::string_view mode,
        float* aim_yaw,
        float* world_fraction,
        std::string* failure_reason)
    {
        if (aim_yaw == nullptr || world_fraction == nullptr
            || failure_reason == nullptr)
        {
            return false;
        }
        const auto& shooter_binding =
            shooter_slot.goldsrc_player_lifecycle.binding();
        const auto& target_binding =
            target_slot.goldsrc_player_lifecycle.binding();
        edict_t* const shooter = state_.edict_store.EntityOfIndex(
            shooter_binding.edict_index);
        edict_t* const target = state_.edict_store.EntityOfIndex(
            target_binding.edict_index);
        if (shooter == nullptr || target == nullptr)
        {
            *failure_reason = "edict_unavailable";
            return false;
        }
        const StockTestSpawnAnchor& anchor = stock_test_spawn_anchors_[
            static_cast<std::size_t>(shooter_slot.slot - 1)];
        const StockTestSpawnAnchor& target_anchor = stock_test_spawn_anchors_[
            static_cast<std::size_t>(target_slot.slot - 1)];
        if (!anchor.valid
            || anchor.session_generation
                != shooter_binding.network_session_generation)
        {
            *failure_reason = "shooter_anchor_not_ready";
            return false;
        }
        if (!target_anchor.valid
            || target_anchor.session_generation
                != target_binding.network_session_generation)
        {
            *failure_reason = "target_anchor_not_ready";
            return false;
        }
        const bool want_blocked = mode == "blocked";
        const bool want_miss = mode == "miss";
        const bool want_aim = mode == "aim";
        if (mode != "clear" && !want_blocked && !want_miss
            && !want_aim)
        {
            *failure_reason = "invalid_mode";
            return false;
        }

        constexpr std::array<float, 2> height_offsets = {{
            0.0f, 36.0f,
        }};
        constexpr std::array<float, 10> clear_distances = {{
            40.0f, 48.0f, 64.0f, 80.0f,
            96.0f, 128.0f, 192.0f, 256.0f,
            384.0f, 512.0f,
        }};
        constexpr std::array<float, 10> wall_beyond_distances = {{
            40.0f, 48.0f, 64.0f, 80.0f, 96.0f,
            128.0f, 192.0f, 256.0f, 384.0f, 512.0f,
        }};
        constexpr std::array<float, 8> yaws = {{
            0.0f, 45.0f, 90.0f, 135.0f,
            180.0f, 225.0f, 270.0f, 315.0f,
        }};
        bool found = false;
        std::size_t shooter_position_count = 0u;
        std::uint64_t fixture_trace_count = 0u;
        Vector selected_shooter;
        Vector selected_target;
        float selected_yaw = 0.0f;
        float selected_command_yaw = 0.0f;
        float selected_world_fraction = 1.0f;
        constexpr float kDegreesToRadians = 0.01745329251994329577f;
        constexpr float kStockPrimaryGlockRange = 8192.0f;
        constexpr float kFixtureSpreadSafetyScale = 1.25f;
        const float stock_primary_glock_spread =
            (anchor.flags & FL_ONGROUND) != 0 ? 0.01f : 0.1f;
        // Stock autoaim samples BodyTarget from view_ofs * [0.5, 1.1].
        // Include the no-autoaim ray, bracket that interval, and test a
        // conservative square around the circular primary-fire spread.
        constexpr std::size_t kBodyTargetIntervalSamples = 15u;
        constexpr float kBodyTargetScaleMinimum = 0.45f;
        constexpr float kBodyTargetScaleMaximum = 1.15f;
        constexpr std::array<float, 9> spread_axis_samples = {{
            -1.0f, -0.75f, -0.5f, -0.25f, 0.0f,
            0.25f, 0.5f, 0.75f, 1.0f,
        }};
        std::vector<Vector> shooter_positions;
        shooter_positions.reserve(2u);
        const auto append_shooter_position =
            [&](const Vector& candidate)
            {
                if (shooter_positions.size() >= 2u
                    || !pmove_runtime_.IsPlayerPositionValid(candidate, 0))
                {
                    return;
                }
                shooter_positions.push_back(candidate);
            };
        append_shooter_position(shooter->v.origin);
        append_shooter_position(anchor.origin);
        shooter_position_count = shooter_positions.size();
        const auto blocked_envelope_is_world_occluded =
            [&](const Vector& shooter_origin,
                const Vector& target_candidate)
            {
                const Vector shot_source(
                    shooter_origin.x + shooter->v.view_ofs.x,
                    shooter_origin.y + shooter->v.view_ofs.y,
                    shooter_origin.z + shooter->v.view_ofs.z);
                const Vector target_center(
                    target_candidate.x
                        + (target->v.mins.x + target->v.maxs.x) * 0.5f,
                    target_candidate.y
                        + (target->v.mins.y + target->v.maxs.y) * 0.5f,
                    target_candidate.z
                        + (target->v.mins.z + target->v.maxs.z) * 0.5f);
                hl::network::GoldSrcCombatTraceCandidate trace_candidate;
                trace_candidate.slot = target_slot.slot;
                trace_candidate.generation =
                    target_binding.network_session_generation;
                trace_candidate.authoritative_generation =
                    target_binding.network_session_generation;
                trace_candidate.connected = true;
                trace_candidate.spawned = true;
                trace_candidate.damageable = true;
                for (std::size_t axis = 0u; axis < 3u; ++axis)
                {
                    trace_candidate.minimum[axis] =
                        target_candidate[axis] + target->v.mins[axis];
                    trace_candidate.maximum[axis] =
                        target_candidate[axis] + target->v.maxs[axis];
                }
                const std::vector<hl::network::GoldSrcCombatTraceCandidate>
                    trace_candidates = {trace_candidate};
                const float spread = stock_primary_glock_spread
                    * kFixtureSpreadSafetyScale;

                for (std::size_t body_index = 0u;
                     body_index <= kBodyTargetIntervalSamples;
                     ++body_index)
                {
                    const float body_scale = body_index == 0u
                        ? 0.0f
                        : kBodyTargetScaleMinimum
                            + (kBodyTargetScaleMaximum
                               - kBodyTargetScaleMinimum)
                                * static_cast<float>(body_index - 1u)
                                / static_cast<float>(
                                    kBodyTargetIntervalSamples - 1u);
                    const Vector body_target(
                        target_center.x + target->v.view_ofs.x * body_scale,
                        target_center.y + target->v.view_ofs.y * body_scale,
                        target_center.z + target->v.view_ofs.z * body_scale);
                    const float delta_x = body_target.x - shot_source.x;
                    const float delta_y = body_target.y - shot_source.y;
                    const float delta_z = body_target.z - shot_source.z;
                    const float distance = std::sqrt(
                        delta_x * delta_x + delta_y * delta_y
                        + delta_z * delta_z);
                    const float horizontal_distance = std::sqrt(
                        delta_x * delta_x + delta_y * delta_y);
                    if (!std::isfinite(distance)
                        || !std::isfinite(horizontal_distance)
                        || distance < 1.0f || horizontal_distance < 1.0f)
                    {
                        return false;
                    }
                    const float forward_x = delta_x / distance;
                    const float forward_y = delta_y / distance;
                    const float forward_z = delta_z / distance;
                    const float right_x = -delta_y / horizontal_distance;
                    const float right_y = delta_x / horizontal_distance;
                    const float up_x =
                        -forward_z * delta_x / horizontal_distance;
                    const float up_y =
                        -forward_z * delta_y / horizontal_distance;
                    const float up_z = horizontal_distance / distance;

                    for (float spread_x : spread_axis_samples)
                    {
                        for (float spread_y : spread_axis_samples)
                        {
                            const float direction_x = forward_x
                                + right_x * spread_x * spread
                                + up_x * spread_y * spread;
                            const float direction_y = forward_y
                                + right_y * spread_x * spread
                                + up_y * spread_y * spread;
                            const float direction_z = forward_z
                                + up_z * spread_y * spread;
                            const float trace_end[3] = {
                                shot_source.x
                                    + direction_x * kStockPrimaryGlockRange,
                                shot_source.y
                                    + direction_y * kStockPrimaryGlockRange,
                                shot_source.z
                                    + direction_z * kStockPrimaryGlockRange,
                            };
                            hl::game_api::detail::GoldSrcWorldLineTrace
                                world_trace{};
                            if (!pmove_runtime_.TraceWorldLine(
                                    shot_source,
                                    trace_end,
                                    &world_trace)
                                || world_trace.start_solid
                                || world_trace.all_solid)
                            {
                                return false;
                            }
                            ++fixture_trace_count;
                            const auto selection =
                                hl::network::SelectGoldSrcCombatLineHit(
                                    {{shot_source.x,
                                      shot_source.y,
                                      shot_source.z}},
                                    {{trace_end[0],
                                      trace_end[1],
                                      trace_end[2]}},
                                    world_trace.fraction,
                                    trace_candidates,
                                    shooter_slot.slot,
                                    shooter_binding
                                        .network_session_generation);
                            const bool center_ray = spread_x == 0.0f
                                && spread_y == 0.0f;
                            if (selection.hit_type
                                    == hl::network::GoldSrcCombatTraceHitType::kPlayer
                                || (center_ray
                                    && selection.hit_type
                                        != hl::network::GoldSrcCombatTraceHitType::kWorld))
                            {
                                return false;
                            }
                        }
                    }
                }
                return true;
            };
        const auto try_fixture_candidate =
            [&](const Vector& shooter_origin,
                const Vector& target_candidate,
                float yaw)
            {
                const float start[3] = {
                    shooter_origin.x + shooter->v.view_ofs.x,
                    shooter_origin.y + shooter->v.view_ofs.y,
                    shooter_origin.z + shooter->v.view_ofs.z,
                };
                const float end[3] = {
                    target_candidate.x,
                    target_candidate.y,
                    start[2],
                };
                const float support_end[3] = {
                    target_candidate.x,
                    target_candidate.y,
                    target_candidate.z - 8.0f,
                };
                if (!pmove_runtime_.IsPlayerPositionValid(
                        target_candidate,
                        0)
                    || !pmove_runtime_.IsWorldPointOpen(end))
                {
                    return false;
                }
                if (want_blocked)
                {
                    hl::game_api::detail::GoldSrcWorldLineTrace support{};
                    if (!pmove_runtime_.TraceWorldHull(
                            target_candidate,
                            support_end,
                            0,
                            &support)
                        || support.start_solid || support.all_solid
                        || support.fraction >= 0.999f
                        || support.plane_normal[2] < 0.7f)
                    {
                        return false;
                    }
                }
                hl::game_api::detail::GoldSrcWorldLineTrace trace{};
                if (!pmove_runtime_.TraceWorldLine(start, end, &trace)
                    || trace.start_solid || trace.all_solid)
                {
                    return false;
                }
                ++fixture_trace_count;
                const bool blocked = trace.fraction > 0.01f
                    && trace.fraction < 0.999f;
                if (blocked != want_blocked)
                {
                    return false;
                }
                if (want_blocked
                    && !blocked_envelope_is_world_occluded(
                        shooter_origin,
                        target_candidate))
                {
                    return false;
                }
                float command_yaw = want_miss
                    ? std::fmod(yaw + 90.0f, 360.0f)
                    : yaw;
                if (want_aim)
                {
                    constexpr float kAutoaimThreshold =
                        0.1736481776669f;
                    constexpr float kAutoaimRayLength = 8192.0f;
                    constexpr std::array<float, 7> body_scales = {{
                        0.5f, 0.6f, 0.7f, 0.8f,
                        0.9f, 1.0f, 1.1f,
                    }};
                    const float delta_x = target_candidate.x - start[0];
                    const float delta_y = target_candidate.y - start[1];
                    const float horizontal_distance = std::sqrt(
                        delta_x * delta_x + delta_y * delta_y);
                    const float half_x = std::max(
                        std::abs(target->v.mins.x),
                        std::abs(target->v.maxs.x));
                    const float half_y = std::max(
                        std::abs(target->v.mins.y),
                        std::abs(target->v.maxs.y));
                    const float horizontal_radius = std::sqrt(
                        half_x * half_x + half_y * half_y);
                    if (!std::isfinite(horizontal_distance)
                        || horizontal_distance < 192.0f
                        || !std::isfinite(horizontal_radius))
                    {
                        return false;
                    }
                    const float offset_degrees =
                        std::atan2(
                            horizontal_radius + 0.5f,
                            horizontal_distance)
                            / kDegreesToRadians
                        + 0.25f;
                    if (!std::isfinite(offset_degrees)
                        || offset_degrees <= 0.0f
                        || offset_degrees >= 9.0f)
                    {
                        return false;
                    }
                    command_yaw = std::fmod(yaw + offset_degrees, 360.0f);
                    const float command_radians =
                        command_yaw * kDegreesToRadians;
                    const float command_forward_x =
                        std::cos(command_radians);
                    const float command_forward_y =
                        std::sin(command_radians);
                    const float command_right_x = -command_forward_y;
                    const float command_right_y = command_forward_x;
                    const float direct_end[3] = {
                        start[0] + command_forward_x * kAutoaimRayLength,
                        start[1] + command_forward_y * kAutoaimRayLength,
                        start[2],
                    };
                    hl::network::GoldSrcCombatTraceCandidate target_trace;
                    target_trace.slot = target_slot.slot;
                    target_trace.generation =
                        target_binding.network_session_generation;
                    target_trace.authoritative_generation =
                        target_binding.network_session_generation;
                    target_trace.connected = true;
                    target_trace.spawned = true;
                    target_trace.damageable = true;
                    for (std::size_t axis = 0u; axis < 3u; ++axis)
                    {
                        target_trace.minimum[axis] =
                            target_candidate[axis] + target->v.mins[axis];
                        target_trace.maximum[axis] =
                            target_candidate[axis] + target->v.maxs[axis];
                    }
                    const std::vector<
                        hl::network::GoldSrcCombatTraceCandidate>
                        target_traces = {target_trace};
                    hl::game_api::detail::GoldSrcWorldLineTrace direct_world{};
                    if (!pmove_runtime_.TraceWorldLine(
                            start,
                            direct_end,
                            &direct_world)
                        || direct_world.start_solid
                        || direct_world.all_solid)
                    {
                        return false;
                    }
                    ++fixture_trace_count;
                    const auto direct_selection =
                        hl::network::SelectGoldSrcCombatLineHit(
                            {{start[0], start[1], start[2]}},
                            {{direct_end[0], direct_end[1], direct_end[2]}},
                            direct_world.fraction,
                            target_traces,
                            shooter_slot.slot,
                            shooter_binding.network_session_generation);
                    if (direct_selection.hit_type
                        == hl::network::GoldSrcCombatTraceHitType::kPlayer)
                    {
                        return false;
                    }

                    for (float body_scale : body_scales)
                    {
                        const float body_target[3] = {
                            target_candidate.x
                                + (target->v.mins.x + target->v.maxs.x)
                                    * 0.5f
                                + target->v.view_ofs.x * body_scale,
                            target_candidate.y
                                + (target->v.mins.y + target->v.maxs.y)
                                    * 0.5f
                                + target->v.view_ofs.y * body_scale,
                            target_candidate.z
                                + (target->v.mins.z + target->v.maxs.z)
                                    * 0.5f
                                + target->v.view_ofs.z * body_scale,
                        };
                        const float body_delta_x = body_target[0] - start[0];
                        const float body_delta_y = body_target[1] - start[1];
                        const float body_delta_z = body_target[2] - start[2];
                        const float body_distance = std::sqrt(
                            body_delta_x * body_delta_x
                            + body_delta_y * body_delta_y
                            + body_delta_z * body_delta_z);
                        if (!std::isfinite(body_distance)
                            || body_distance < 1.0f)
                        {
                            return false;
                        }
                        const float direction_x =
                            body_delta_x / body_distance;
                        const float direction_y =
                            body_delta_y / body_distance;
                        const float direction_z =
                            body_delta_z / body_distance;
                        const float autoaim_metric =
                            (std::abs(
                                 direction_x * command_right_x
                                 + direction_y * command_right_y)
                             + std::abs(direction_z) * 0.5f)
                            * (1.0f
                               + 0.2f
                                   * (body_distance / kAutoaimRayLength));
                        if (!std::isfinite(autoaim_metric)
                            || autoaim_metric >= kAutoaimThreshold)
                        {
                            return false;
                        }
                        hl::game_api::detail::GoldSrcWorldLineTrace
                            visibility_world{};
                        if (!pmove_runtime_.TraceWorldLine(
                                start,
                                body_target,
                                &visibility_world)
                            || visibility_world.start_solid
                            || visibility_world.all_solid)
                        {
                            return false;
                        }
                        ++fixture_trace_count;
                        const auto visibility_selection =
                            hl::network::SelectGoldSrcCombatLineHit(
                                {{start[0], start[1], start[2]}},
                                {{body_target[0],
                                  body_target[1],
                                  body_target[2]}},
                                visibility_world.fraction,
                                target_traces,
                                shooter_slot.slot,
                                shooter_binding.network_session_generation);
                        if (visibility_selection.hit_type
                                != hl::network::
                                    GoldSrcCombatTraceHitType::kPlayer
                            || visibility_selection.player_slot
                                != target_slot.slot)
                        {
                            return false;
                        }
                    }
                }
                selected_shooter = shooter_origin;
                selected_target = target_candidate;
                selected_yaw = yaw;
                selected_command_yaw = command_yaw;
                selected_world_fraction = trace.fraction;
                return true;
            };
        constexpr std::array<float, 2> direct_target_height_offsets = {{
            0.0f, 1.0f,
        }};
        for (const Vector& shooter_origin : shooter_positions)
        {
            for (const Vector& base_target :
                 {target->v.origin, target_anchor.origin})
            {
                const float delta_x = base_target.x - shooter_origin.x;
                const float delta_y = base_target.y - shooter_origin.y;
                const float minimum_direct_distance =
                    want_aim ? 192.0f : 96.0f;
                if (delta_x * delta_x + delta_y * delta_y
                    < minimum_direct_distance * minimum_direct_distance)
                {
                    continue;
                }
                float yaw = std::atan2(delta_y, delta_x)
                    / kDegreesToRadians;
                if (yaw < 0.0f)
                {
                    yaw += 360.0f;
                }
                if (want_blocked)
                {
                    found = try_fixture_candidate(
                        shooter_origin,
                        base_target,
                        yaw);
                }
                else
                {
                    for (float height_offset : direct_target_height_offsets)
                    {
                        const Vector target_candidate(
                            base_target.x,
                            base_target.y,
                            base_target.z + height_offset);
                        const float target_relative_height =
                            shooter_origin.z + shooter->v.view_ofs.z
                            - target_candidate.z;
                        if (target_relative_height < target->v.mins.z
                            || target_relative_height > target->v.maxs.z)
                        {
                            continue;
                        }
                        if (try_fixture_candidate(
                                shooter_origin,
                                target_candidate,
                                yaw))
                        {
                            found = true;
                            break;
                        }
                    }
                }
                if (found)
                {
                    break;
                }
            }
            if (found)
            {
                break;
            }
        }
        constexpr std::array<float, 6> support_probe_height_offsets = {{
            1.0f, 18.0f, 36.0f, 54.0f, 72.0f, 96.0f,
        }};
        constexpr float kSupportProbeDrop = 384.0f;
        const auto find_supported_target_origin =
            [&](float x,
                float y,
                float reference_z,
                Vector* supported_origin)
            {
                if (supported_origin == nullptr
                    || !std::isfinite(x) || !std::isfinite(y)
                    || !std::isfinite(reference_z))
                {
                    return false;
                }
                for (float height_offset : support_probe_height_offsets)
                {
                    const Vector probe_start(
                        x,
                        y,
                        reference_z + height_offset);
                    const Vector probe_end(
                        x,
                        y,
                        probe_start.z - kSupportProbeDrop);
                    if (!pmove_runtime_.IsPlayerPositionValid(
                            probe_start,
                            0))
                    {
                        continue;
                    }
                    hl::game_api::detail::GoldSrcWorldLineTrace support{};
                    if (!pmove_runtime_.TraceWorldHull(
                            probe_start,
                            probe_end,
                            0,
                            &support))
                    {
                        continue;
                    }
                    ++fixture_trace_count;
                    if (support.start_solid || support.all_solid
                        || support.fraction >= 0.999f
                        || support.plane_normal[2] < 0.7f)
                    {
                        continue;
                    }
                    const Vector candidate(
                        support.end_position[0],
                        support.end_position[1],
                        support.end_position[2]);
                    if (!pmove_runtime_.IsPlayerPositionValid(candidate, 0))
                    {
                        continue;
                    }
                    *supported_origin = candidate;
                    return true;
                }
                return false;
            };
        for (const Vector& shooter_origin : shooter_positions)
        {
            if (found)
            {
                break;
            }
            for (float yaw : yaws)
            {
                const float radians = yaw * kDegreesToRadians;
                const float direction_x = std::cos(radians);
                const float direction_y = std::sin(radians);
                if (want_blocked)
                {
                    const float start[3] = {
                        shooter_origin.x + shooter->v.view_ofs.x,
                        shooter_origin.y + shooter->v.view_ofs.y,
                        shooter_origin.z + shooter->v.view_ofs.z,
                    };
                    const float far_end[3] = {
                        start[0] + direction_x * 2048.0f,
                        start[1] + direction_y * 2048.0f,
                        start[2],
                    };
                    hl::game_api::detail::GoldSrcWorldLineTrace wall{};
                    if (!pmove_runtime_.TraceWorldLine(
                            start,
                            far_end,
                            &wall)
                        || wall.start_solid || wall.all_solid
                        || wall.fraction <= 0.01f
                        || wall.fraction >= 0.999f)
                    {
                        continue;
                    }
                    ++fixture_trace_count;
                    for (float beyond_wall : wall_beyond_distances)
                    {
                        const float target_x = wall.end_position[0]
                            + direction_x * beyond_wall;
                        const float target_y = wall.end_position[1]
                            + direction_y * beyond_wall;
                        const std::array<float, 3> support_reference_zs = {{
                            shooter_origin.z,
                            target->v.origin.z,
                            target_anchor.origin.z,
                        }};
                        for (float reference_z : support_reference_zs)
                        {
                            Vector target_candidate;
                            if (!find_supported_target_origin(
                                    target_x,
                                    target_y,
                                    reference_z,
                                    &target_candidate))
                            {
                                continue;
                            }
                            if (try_fixture_candidate(
                                    shooter_origin,
                                    target_candidate,
                                    yaw))
                            {
                                found = true;
                                break;
                            }
                        }
                        if (found)
                        {
                            break;
                        }
                    }
                }
                else
                {
                    for (float distance : clear_distances)
                    {
                        for (float height_offset : height_offsets)
                        {
                            const Vector target_candidate(
                                shooter_origin.x + direction_x * distance,
                                shooter_origin.y + direction_y * distance,
                                shooter_origin.z + height_offset);
                            if (try_fixture_candidate(
                                    shooter_origin,
                                    target_candidate,
                                    yaw))
                            {
                                found = true;
                                break;
                            }
                        }
                        if (found)
                        {
                            break;
                        }
                    }
                }
                if (found)
                {
                    break;
                }
            }
                if (found)
                {
                    break;
                }
            }
        if (!found)
        {
            *failure_reason = want_blocked
                ? "blocked_fixture_not_found_s"
                    + std::to_string(shooter_position_count)
                    + "_n" + std::to_string(fixture_trace_count)
                : "clear_fixture_not_found";
            return false;
        }

        edict_t* const world = state_.edict_store.World();
        ApplyStockTestCombatPose(
            shooter,
            selected_shooter,
            want_miss ? std::fmod(selected_yaw + 90.0f, 360.0f)
                      : selected_yaw,
            anchor.flags,
            world);
        ApplyStockTestCombatPose(
            target,
            selected_target,
            std::fmod(selected_yaw + 180.0f, 360.0f),
            target_anchor.flags,
            world);
        for (const auto& [slot_state, player] :
             {std::pair<DedicatedPlayerRuntimeSlot*, edict_t*>{
                  &shooter_slot,
                  shooter},
              std::pair<DedicatedPlayerRuntimeSlot*, edict_t*>{
                  &target_slot,
                  target}})
        {
            auto* const client =
                FindClientReplicationSummary(slot_state->slot);
            if (client == nullptr || client->combat_fixture_baseline_captured)
            {
                continue;
            }
            client->combat_fixture_baseline_captured = true;
            client->initial_authoritative_health = player->v.health;
            client->minimum_authoritative_health = player->v.health;
            client->initial_clientdata_health = client->clientdata_health;
            client->minimum_clientdata_health = client->clientdata_health;
            client->initial_glock_clip = client->glock_clip;
            client->minimum_glock_clip = client->glock_clip;
            client->initial_glock_reserve_ammo =
                client->glock_reserve_ammo;
        }
        for (std::size_t receiver = 0u;
             receiver < stock_test_reset_transitions_.size(); ++receiver)
        {
            for (int changed_slot : {shooter_slot.slot, target_slot.slot})
            {
                if (receiver != static_cast<std::size_t>(changed_slot - 1))
                {
                    stock_test_reset_transitions_[receiver][
                        static_cast<std::size_t>(changed_slot - 1)] = true;
                    stock_test_nointerp_clear_pending_[receiver][
                        static_cast<std::size_t>(changed_slot - 1)] = true;
                }
            }
        }
        *aim_yaw = selected_command_yaw;
        *world_fraction = selected_world_fraction;
        *failure_reason = "none";
        return true;
    }

    void AcknowledgeStockTestCombatFixture(
        std::string request_id,
        int shooter_slot,
        int target_slot,
        std::string_view mode,
        std::string_view status,
        std::string_view reason,
        float aim_yaw,
        float world_fraction)
    {
        request_id = SanitizeLogField(std::move(request_id));
        hl::common::Logger::Info(
            hl::common::LogCategory::Server,
            "goldsrc_stock_test_combat_ack: request_id=" + request_id
                + ",shooter_slot=" + std::to_string(shooter_slot)
                + ",target_slot=" + std::to_string(target_slot)
                + ",mode=" + std::string(mode)
                + ",status=" + std::string(status)
                + ",reason=" + std::string(reason)
                + ",aim_yaw=" + std::to_string(aim_yaw)
                + ",world_fraction=" + std::to_string(world_fraction));
    }

    void AcknowledgeStockTestFallFixture(
        std::string request_id,
        int slot,
        std::string_view mode,
        std::string_view status,
        std::string_view reason)
    {
        request_id = SanitizeLogField(std::move(request_id));
        const std::string_view reported_mode = mode == "landing"
            ? "landing"
            : mode == "damage" ? "damage" : "invalid";
        hl::common::Logger::Info(
            hl::common::LogCategory::Server,
            "goldsrc_stock_test_fall_ack: request_id=" + request_id
                + ",slot=" + std::to_string(slot)
                + ",mode=" + std::string(reported_mode)
                + ",status=" + std::string(status)
                + ",reason=" + std::string(reason));
    }

    void ReportStockTestFallObservation(
        int slot,
        std::uint64_t session_generation,
        std::string_view mode,
        std::string_view status,
        std::string_view reason)
    {
        const std::string_view reported_mode = mode == "landing"
            ? "landing"
            : mode == "damage" ? "damage" : "invalid";
        hl::common::Logger::Info(
            hl::common::LogCategory::Server,
            "goldsrc_stock_test_fall_observation: slot="
                + std::to_string(slot)
                + ",session_generation="
                + std::to_string(session_generation)
                + ",mode=" + std::string(reported_mode)
                + ",status=" + std::string(status)
                + ",reason=" + std::string(reason));
    }

    void ReportStockTestFallResponsiveness(
        std::string_view status,
        std::string_view reason)
    {
        hl::common::Logger::Info(
            hl::common::LogCategory::Server,
            "goldsrc_stock_test_fall_responsiveness: status="
                + std::string(status)
                + ",reason=" + std::string(reason));
    }

    bool ArrangeStockTestFallLanding(
        DedicatedPlayerRuntimeSlot& slot_state,
        std::string_view mode,
        std::string* failure_reason)
    {
        if (failure_reason == nullptr || slot_state.slot < 1
            || static_cast<std::size_t>(slot_state.slot)
                > stock_test_spawn_anchors_.size())
        {
            return false;
        }
        const bool damage_mode = mode == "damage";
        if (mode != "landing" && !damage_mode)
        {
            *failure_reason = "invalid_mode";
            return false;
        }
        const auto& binding =
            slot_state.goldsrc_player_lifecycle.binding();
        const StockTestFallAcceptancePhase required_phase = damage_mode
            ? StockTestFallAcceptancePhase::kResetObserved
            : StockTestFallAcceptancePhase::kInactive;
        if (stock_test_fall_acceptance_.phase != required_phase
            || (damage_mode
                && (stock_test_fall_acceptance_.slot != slot_state.slot
                    || stock_test_fall_acceptance_.session_generation
                        != binding.network_session_generation)))
        {
            *failure_reason = "invalid_sequence";
            return false;
        }
        const StockTestSpawnAnchor& anchor = stock_test_spawn_anchors_[
            static_cast<std::size_t>(slot_state.slot - 1)];
        if (!anchor.valid
            || anchor.session_generation
                != binding.network_session_generation)
        {
            *failure_reason = "anchor_not_ready";
            return false;
        }
        if ((anchor.flags & FL_ONGROUND) == 0
            || (anchor.flags & FL_DUCKING) != 0
            || anchor.ground_edict != 0)
        {
            *failure_reason = "world_support_required";
            return false;
        }
        edict_t* const player = state_.edict_store.EntityOfIndex(
            binding.edict_index);
        if (player == nullptr)
        {
            *failure_reason = "edict_unavailable";
            return false;
        }
        constexpr float kFixedFallDamage = 10.0f;
        if (!std::isfinite(player->v.health)
            || player->v.health <= kFixedFallDamage
            || player->v.deadflag != 0
            || player->v.takedamage == 0.0f
            || player->v.waterlevel != 0
            || player->v.movetype != MOVETYPE_WALK
            || (player->v.flags & FL_GODMODE) != 0)
        {
            *failure_reason = "player_not_nonlethal_ready";
            return false;
        }

        // PM_PlayerMove probes two units down while categorizing position.
        // Start above that probe, but within one 10 ms fall step, so the proof
        // exercises the swept landing trace instead of beginning grounded.
        constexpr float kAirborneOffset = 3.0f;
        constexpr float kSupportProbeDepth = 8.0f;
        constexpr float kLandingVelocity = 400.0f;
        constexpr float kDamageVelocity = 600.0f;
        const float fall_velocity = damage_mode
            ? kDamageVelocity
            : kLandingVelocity;
        const Vector airborne_origin(
            anchor.origin.x,
            anchor.origin.y,
            anchor.origin.z + kAirborneOffset);
        const Vector support_end(
            anchor.origin.x,
            anchor.origin.y,
            anchor.origin.z - kSupportProbeDepth);
        if (!pmove_runtime_.IsPlayerPositionValid(airborne_origin, 0))
        {
            *failure_reason = "airborne_position_invalid";
            return false;
        }
        hl::game_api::detail::GoldSrcWorldLineTrace support{};
        if (!pmove_runtime_.TraceWorldHull(
                airborne_origin,
                support_end,
                0,
                &support)
            || support.start_solid || support.all_solid
            || !support.hit_world || support.fraction <= 0.0f
            || support.fraction >= 0.999f
            || support.plane_normal[2] < 0.7f)
        {
            *failure_reason = "landing_support_invalid";
            return false;
        }

        // Keep the proof independent of an installed server.cfg while still
        // exercising the stock multiplayer fixed-damage Game DLL path.
        if (!state_.cvar_registry.SetValue("mp_falldamage", "0"))
        {
            *failure_reason = "fall_damage_cvar_unavailable";
            return false;
        }
        const std::optional<hl::game_api::CvarSnapshot> fall_damage_cvar =
            state_.cvar_registry.SnapshotOf("mp_falldamage");
        if (!fall_damage_cvar.has_value()
            || !std::isfinite(fall_damage_cvar->value)
            || std::abs(fall_damage_cvar->value) > 0.0001f)
        {
            *failure_reason = "fall_damage_cvar_not_fixed";
            return false;
        }

        player->v.origin = airborne_origin;
        player->v.angles = anchor.angles;
        player->v.v_angle = anchor.v_angle;
        player->v.mins = anchor.mins;
        player->v.maxs = anchor.maxs;
        player->v.punchangle = Vector(0.0f, 0.0f, 0.0f);
        player->v.fixangle = TRUE;
        player->v.velocity = Vector(0.0f, 0.0f, -fall_velocity);
        player->v.basevelocity = Vector(0.0f, 0.0f, 0.0f);
        player->v.flFallVelocity = fall_velocity;
        player->v.button = 0;
        player->v.oldbuttons = 0;
        player->v.flags &= ~(FL_ONGROUND | FL_DUCKING);
        player->v.groundentity = nullptr;
        for (int axis = 0; axis < 3; ++axis)
        {
            player->v.size[axis] =
                player->v.maxs[axis] - player->v.mins[axis];
            player->v.absmin[axis] =
                player->v.origin[axis] + player->v.mins[axis] - 1.0f;
            player->v.absmax[axis] =
                player->v.origin[axis] + player->v.maxs[axis] + 1.0f;
        }
        for (std::size_t receiver = 0u;
             receiver < stock_test_reset_transitions_.size();
             ++receiver)
        {
            if (receiver != static_cast<std::size_t>(slot_state.slot - 1))
            {
                stock_test_reset_transitions_[receiver][
                    static_cast<std::size_t>(slot_state.slot - 1)] = true;
                stock_test_nointerp_clear_pending_[receiver][
                    static_cast<std::size_t>(slot_state.slot - 1)] = true;
            }
        }
        *failure_reason = "none";
        stock_test_fall_acceptance_.phase = damage_mode
            ? StockTestFallAcceptancePhase::kDamageArranged
            : StockTestFallAcceptancePhase::kLandingArranged;
        stock_test_fall_acceptance_.slot = slot_state.slot;
        stock_test_fall_acceptance_.session_generation =
            binding.network_session_generation;
        stock_test_fall_acceptance_.health_before = player->v.health;
        return true;
    }

    void FailStockTestFallObservation(
        std::string_view mode,
        std::string_view reason)
    {
        stock_test_fall_acceptance_.phase =
            StockTestFallAcceptancePhase::kFailed;
        ReportStockTestFallObservation(
            stock_test_fall_acceptance_.slot,
            stock_test_fall_acceptance_.session_generation,
            mode,
            "failed",
            reason);
    }

    bool CaptureStockTestFallResponsivenessOrigins()
    {
        DedicatedPlayerRuntimeSlot* slots[2] = {nullptr, nullptr};
        for (DedicatedPlayerRuntimeSlot& candidate :
             state_.dedicated_multiplayer_foundation.slots)
        {
            if (candidate.slot >= 1 && candidate.slot <= 2
                && IsDedicatedSlotConnectedOccupant(candidate)
                && candidate.goldsrc_player_lifecycle.put_in_server())
            {
                slots[candidate.slot - 1] = &candidate;
            }
        }
        if (slots[0] == nullptr || slots[1] == nullptr)
        {
            return false;
        }
        const auto& binding_a = slots[0]->goldsrc_player_lifecycle.binding();
        const auto& binding_b = slots[1]->goldsrc_player_lifecycle.binding();
        if (lifecycle_edict_registry_.Validate(binding_a)
                != hl::network::GoldSrcClientEdictBindingResult::kBound
            || lifecycle_edict_registry_.Validate(binding_b)
                != hl::network::GoldSrcClientEdictBindingResult::kBound)
        {
            return false;
        }
        edict_t* const player_a =
            state_.edict_store.EntityOfIndex(binding_a.edict_index);
        edict_t* const player_b =
            state_.edict_store.EntityOfIndex(binding_b.edict_index);
        if (player_a == nullptr || player_b == nullptr)
        {
            return false;
        }
        stock_test_fall_acceptance_.responsiveness_origin_a =
            player_a->v.origin;
        stock_test_fall_acceptance_.responsiveness_origin_b =
            player_b->v.origin;
        stock_test_fall_acceptance_.responsiveness_generation_a =
            binding_a.network_session_generation;
        stock_test_fall_acceptance_.responsiveness_generation_b =
            binding_b.network_session_generation;
        stock_test_fall_acceptance_.responsiveness_a = false;
        stock_test_fall_acceptance_.responsiveness_b = false;
        return true;
    }

    void ObserveStockTestFallLanding(
        const DedicatedPlayerRuntimeSlot& slot_state,
        const hl::network::GoldSrcClientEdictBinding& binding,
        edict_t* player,
        const hl::game_api::detail::GoldSrcPmoveExecutionResult& executed)
    {
        const bool landing_mode = stock_test_fall_acceptance_.phase
            == StockTestFallAcceptancePhase::kLandingArranged;
        const bool damage_mode = stock_test_fall_acceptance_.phase
            == StockTestFallAcceptancePhase::kDamageArranged;
        if ((!landing_mode && !damage_mode)
            || slot_state.slot != stock_test_fall_acceptance_.slot
            || binding.network_session_generation
                != stock_test_fall_acceptance_.session_generation
            || executed.commands_executed == 0u)
        {
            return;
        }
        if (!executed.grounded)
        {
            return;
        }

        const std::string_view mode = damage_mode ? "damage" : "landing";
        edict_t* const world = state_.edict_store.World();
        if (player == nullptr || world == nullptr
            || (player->v.flags & FL_ONGROUND) == 0
            || player->v.groundentity != world)
        {
            FailStockTestFallObservation(mode, "world_ground_required");
            return;
        }
        constexpr float kFixedFallDamage = 10.0f;
        const float expected_health = stock_test_fall_acceptance_.health_before
            - (damage_mode ? kFixedFallDamage : 0.0f);
        if (!std::isfinite(player->v.health)
            || std::abs(player->v.health - expected_health) > 0.01f
            || player->v.health <= 0.0f || player->v.deadflag != 0)
        {
            FailStockTestFallObservation(mode, "health_transition_invalid");
            return;
        }

        if (damage_mode && !CaptureStockTestFallResponsivenessOrigins())
        {
            FailStockTestFallObservation(mode, "peer_not_ready");
            return;
        }
        stock_test_fall_acceptance_.phase = damage_mode
            ? StockTestFallAcceptancePhase::kDamageObserved
            : StockTestFallAcceptancePhase::kLandingObserved;
        ReportStockTestFallObservation(
            stock_test_fall_acceptance_.slot,
            stock_test_fall_acceptance_.session_generation,
            mode,
            "pass",
            "none");
    }

    void ObserveStockTestFallResponsiveness(
        const DedicatedPlayerRuntimeSlot& slot_state,
        const hl::network::GoldSrcClientEdictBinding& binding,
        const edict_t* player,
        const hl::game_api::detail::GoldSrcPmoveExecutionResult& executed,
        bool movement_requested)
    {
        if (stock_test_fall_acceptance_.phase
                != StockTestFallAcceptancePhase::kDamageObserved
            || player == nullptr || executed.commands_executed == 0u
            || !movement_requested || !executed.authoritative_state_changed)
        {
            return;
        }
        const auto moved_from = [&](const Vector& origin)
        {
            const float dx = player->v.origin.x - origin.x;
            const float dy = player->v.origin.y - origin.y;
            return std::isfinite(dx) && std::isfinite(dy)
                && (dx * dx + dy * dy) > 1.0f;
        };
        if (slot_state.slot == 1
            && binding.network_session_generation
                == stock_test_fall_acceptance_.responsiveness_generation_a
            && moved_from(
                stock_test_fall_acceptance_.responsiveness_origin_a))
        {
            stock_test_fall_acceptance_.responsiveness_a = true;
        }
        else if (slot_state.slot == 2
            && binding.network_session_generation
                == stock_test_fall_acceptance_.responsiveness_generation_b
            && moved_from(
                stock_test_fall_acceptance_.responsiveness_origin_b))
        {
            stock_test_fall_acceptance_.responsiveness_b = true;
        }
        if (stock_test_fall_acceptance_.responsiveness_a
            && stock_test_fall_acceptance_.responsiveness_b)
        {
            stock_test_fall_acceptance_.phase =
                StockTestFallAcceptancePhase::kComplete;
            ReportStockTestFallResponsiveness("pass", "none");
        }
    }

    void CheckStockTestControlRequest(
        std::chrono::steady_clock::time_point frame_time)
    {
        if (!stock_test_control_file_.has_value()
            || frame_time < next_stock_test_control_probe_time_)
        {
            return;
        }
        next_stock_test_control_probe_time_ =
            frame_time + std::chrono::milliseconds(100);
        std::error_code file_error;
        const bool requested = std::filesystem::exists(
            *stock_test_control_file_, file_error);
        if (file_error)
        {
            return;
        }
        if (stock_test_control_consumed_)
        {
            if (!requested)
            {
                stock_test_control_consumed_ = false;
            }
            return;
        }
        if (!requested)
        {
            return;
        }
        stock_test_control_consumed_ = true;
        ++summary_.test_reset_requests;
        std::ifstream stream(*stock_test_control_file_);
        std::string command;
        int slot = 0;
        int target_slot_number = 0;
        std::uint64_t generation = 0u;
        std::uint64_t target_generation = 0u;
        std::string request_id = "invalid";
        std::string mode = "clear";
        std::string token;
        if (stream)
        {
            stream >> command;
            while (stream >> token)
            {
                const std::size_t equals = token.find('=');
                if (equals == std::string::npos)
                {
                    continue;
                }
                const std::string key = token.substr(0u, equals);
                const std::string value = token.substr(equals + 1u);
                try
                {
                    if (key == "slot")
                    {
                        slot = std::stoi(value);
                    }
                    else if (key == "expected_session_generation")
                    {
                        generation = std::stoull(value);
                    }
                    else if (key == "target_slot")
                    {
                        target_slot_number = std::stoi(value);
                    }
                    else if (key == "expected_target_session_generation")
                    {
                        target_generation = std::stoull(value);
                    }
                    else if (key == "request_id")
                    {
                        request_id = value;
                    }
                    else if (key == "mode")
                    {
                        mode = value;
                    }
                }
                catch (...)
                {
                    command.clear();
                }
            }
        }
        if (command == "arrange_combat_pair")
        {
            if (!combat_enabled_ || slot < 1 || target_slot_number < 1
                || slot == target_slot_number || generation == 0u
                || target_generation == 0u || request_id.empty())
            {
                AcknowledgeStockTestCombatFixture(
                    request_id,
                    slot,
                    target_slot_number,
                    mode,
                    "rejected",
                    "invalid_request",
                    0.0f,
                    1.0f);
                return;
            }
            DedicatedPlayerRuntimeSlot* shooter = nullptr;
            DedicatedPlayerRuntimeSlot* target = nullptr;
            for (DedicatedPlayerRuntimeSlot& candidate :
                 state_.dedicated_multiplayer_foundation.slots)
            {
                if (!IsDedicatedSlotConnectedOccupant(candidate)
                    || !candidate.goldsrc_player_lifecycle.put_in_server())
                {
                    continue;
                }
                if (candidate.slot == slot)
                {
                    shooter = &candidate;
                }
                else if (candidate.slot == target_slot_number)
                {
                    target = &candidate;
                }
            }
            if (shooter == nullptr || target == nullptr)
            {
                AcknowledgeStockTestCombatFixture(
                    request_id,
                    slot,
                    target_slot_number,
                    mode,
                    "rejected",
                    "slot_not_active",
                    0.0f,
                    1.0f);
                return;
            }
            if (shooter->goldsrc_player_lifecycle.binding()
                        .network_session_generation != generation
                || target->goldsrc_player_lifecycle.binding()
                        .network_session_generation != target_generation)
            {
                AcknowledgeStockTestCombatFixture(
                    request_id,
                    slot,
                    target_slot_number,
                    mode,
                    "rejected",
                    "stale_generation",
                    0.0f,
                    1.0f);
                return;
            }
            float aim_yaw = 0.0f;
            float world_fraction = 1.0f;
            std::string failure_reason;
            const bool arranged = ArrangeStockTestCombatPair(
                *shooter,
                *target,
                mode,
                &aim_yaw,
                &world_fraction,
                &failure_reason);
            AcknowledgeStockTestCombatFixture(
                request_id,
                slot,
                target_slot_number,
                mode,
                arranged ? "applied" : "rejected",
                failure_reason,
                aim_yaw,
                world_fraction);
            return;
        }
        if (command == "arrange_fall_landing")
        {
            if (!combat_enabled_ || !fall_damage_proof_ || slot < 1
                || static_cast<std::size_t>(slot)
                    > stock_test_spawn_anchors_.size()
                || generation == 0u || request_id.empty())
            {
                AcknowledgeStockTestFallFixture(
                    request_id,
                    slot,
                    mode,
                    "rejected",
                    "invalid_request");
                return;
            }
            DedicatedPlayerRuntimeSlot* target = nullptr;
            for (DedicatedPlayerRuntimeSlot& candidate :
                 state_.dedicated_multiplayer_foundation.slots)
            {
                if (candidate.slot == slot
                    && IsDedicatedSlotConnectedOccupant(candidate)
                    && candidate.goldsrc_player_lifecycle.put_in_server())
                {
                    target = &candidate;
                    break;
                }
            }
            if (target == nullptr)
            {
                AcknowledgeStockTestFallFixture(
                    request_id,
                    slot,
                    mode,
                    "rejected",
                    "slot_not_active");
                return;
            }
            if (target->goldsrc_player_lifecycle.binding()
                    .network_session_generation != generation)
            {
                AcknowledgeStockTestFallFixture(
                    request_id,
                    slot,
                    mode,
                    "rejected",
                    "stale_generation");
                return;
            }
            std::string failure_reason;
            const bool arranged = ArrangeStockTestFallLanding(
                *target,
                mode,
                &failure_reason);
            AcknowledgeStockTestFallFixture(
                request_id,
                slot,
                mode,
                arranged ? "applied" : "rejected",
                failure_reason);
            return;
        }
        if (command != "reset_player_to_spawn_anchor"
            || slot < 1
            || static_cast<std::size_t>(slot)
                > stock_test_spawn_anchors_.size()
            || generation == 0u || request_id.empty())
        {
            AcknowledgeStockTestReset(
                request_id, slot, "rejected", "invalid_request");
            return;
        }
        DedicatedPlayerRuntimeSlot* target = nullptr;
        for (DedicatedPlayerRuntimeSlot& candidate :
             state_.dedicated_multiplayer_foundation.slots)
        {
            if (candidate.slot == slot
                && IsDedicatedSlotConnectedOccupant(candidate))
            {
                target = &candidate;
                break;
            }
        }
        if (target == nullptr
            || !target->goldsrc_player_lifecycle.put_in_server())
        {
            AcknowledgeStockTestReset(
                request_id, slot, "rejected", "slot_not_active");
            return;
        }
        const auto& binding = target->goldsrc_player_lifecycle.binding();
        if (binding.network_session_generation != generation)
        {
            AcknowledgeStockTestReset(
                request_id, slot, "rejected", "stale_generation");
            return;
        }
        StockTestSpawnAnchor& anchor = stock_test_spawn_anchors_[
            static_cast<std::size_t>(slot - 1)];
        if (!anchor.valid || anchor.session_generation != generation)
        {
            AcknowledgeStockTestReset(
                request_id, slot, "rejected", "anchor_not_ready");
            return;
        }
        edict_t* const player = state_.edict_store.EntityOfIndex(
            binding.edict_index);
        if (player == nullptr)
        {
            AcknowledgeStockTestReset(
                request_id, slot, "rejected", "edict_unavailable");
            return;
        }
        player->v.origin = anchor.origin;
        player->v.angles = anchor.angles;
        player->v.v_angle = anchor.v_angle;
        player->v.mins = anchor.mins;
        player->v.maxs = anchor.maxs;
        player->v.velocity = Vector(0.0f, 0.0f, 0.0f);
        player->v.basevelocity = Vector(0.0f, 0.0f, 0.0f);
        player->v.button = 0;
        player->v.oldbuttons = 0;
        player->v.flags = (player->v.flags & ~(FL_ONGROUND | FL_DUCKING))
            | (anchor.flags & (FL_ONGROUND | FL_DUCKING));
        player->v.groundentity = anchor.ground_edict < 0
            ? nullptr
            : state_.edict_store.EntityOfIndex(anchor.ground_edict);
        for (int axis = 0; axis < 3; ++axis)
        {
            player->v.size[axis] = player->v.maxs[axis] - player->v.mins[axis];
            player->v.absmin[axis] =
                player->v.origin[axis] + player->v.mins[axis] - 1.0f;
            player->v.absmax[axis] =
                player->v.origin[axis] + player->v.maxs[axis] + 1.0f;
        }
        for (std::size_t receiver = 0u;
             receiver < stock_test_reset_transitions_.size(); ++receiver)
        {
            if (receiver != static_cast<std::size_t>(slot - 1))
            {
                stock_test_reset_transitions_[receiver][
                    static_cast<std::size_t>(slot - 1)] = true;
                stock_test_nointerp_clear_pending_[receiver][
                    static_cast<std::size_t>(slot - 1)] = true;
            }
        }
        if (stock_test_fall_acceptance_.phase
                == StockTestFallAcceptancePhase::kLandingObserved
            && stock_test_fall_acceptance_.slot == slot
            && stock_test_fall_acceptance_.session_generation == generation)
        {
            stock_test_fall_acceptance_.phase =
                StockTestFallAcceptancePhase::kResetObserved;
        }
        AcknowledgeStockTestReset(
            request_id, slot, "applied", "none");
    }

    bool CheckManualShutdownRequest(
        std::chrono::steady_clock::time_point frame_time)
    {
        if (!pmove_persistent_ || !manual_shutdown_file_.has_value()
            || frame_time < next_manual_shutdown_probe_time_)
        {
            return false;
        }
        next_manual_shutdown_probe_time_ =
            frame_time + std::chrono::milliseconds(100);
        std::error_code shutdown_file_error;
        const bool requested = std::filesystem::exists(
            *manual_shutdown_file_,
            shutdown_file_error);
        if (shutdown_file_error)
        {
            return !FailTransport(
                "manual-shutdown-file-check-failed",
                "goldsrc_manual_session_failed: reason=shutdown-file-check");
        }
        if (!requested)
        {
            return false;
        }
        manual_session_lifecycle_.RequestShutdown();
        SyncManualSessionSummary();
        LogManualSessionShutdown("launcher_request");
        MarkComplete("manual_shutdown_request");
        return true;
    }

    bool CheckManualDisconnectRequest(
        std::chrono::steady_clock::time_point frame_time)
    {
        if (!pmove_persistent_ || !manual_disconnect_file_.has_value()
            || frame_time < next_manual_disconnect_probe_time_)
        {
            return false;
        }
        next_manual_disconnect_probe_time_ =
            frame_time + std::chrono::milliseconds(100);
        std::error_code disconnect_file_error;
        const bool requested = std::filesystem::exists(
            *manual_disconnect_file_,
            disconnect_file_error);
        if (disconnect_file_error)
        {
            return !FailTransport(
                "manual-disconnect-file-check-failed",
                "goldsrc_manual_client_disconnect_failed: reason=file-check");
        }
        if (manual_disconnect_consumed_)
        {
            if (!requested)
            {
                manual_disconnect_consumed_ = false;
            }
            return false;
        }
        if (!requested)
        {
            return false;
        }
        std::ifstream request_stream(*manual_disconnect_file_);
        std::string request_command;
        if (!request_stream
            || !std::getline(request_stream, request_command))
        {
            return !FailTransport(
                "manual-disconnect-file-read-failed",
                "goldsrc_manual_client_disconnect_failed: reason=file-read");
        }
        const int requested_slot = request_command == "disconnect-owned-slot-1"
            ? 1
            : request_command == "disconnect-owned-slot-2" ? 2 : 0;
        if (requested_slot == 0)
        {
            return !FailTransport(
                "manual-disconnect-request-invalid",
                "goldsrc_manual_client_disconnect_failed: reason=request-invalid");
        }
        DedicatedPlayerRuntimeSlot* target = nullptr;
        for (DedicatedPlayerRuntimeSlot& candidate :
             state_.dedicated_multiplayer_foundation.slots)
        {
            if (candidate.slot == requested_slot
                && IsDedicatedSlotConnectedOccupant(candidate))
            {
                target = &candidate;
                break;
            }
        }
        if (target == nullptr)
        {
            return !FailTransport(
                "manual-disconnect-target-not-connected",
                "goldsrc_manual_client_disconnect_failed: reason=slot-1-not-connected");
        }
        if (!HandleClientDisconnect(*target))
        {
            return true;
        }
        manual_disconnect_consumed_ = true;
        hl::common::Logger::Info(
            hl::common::LogCategory::Server,
            "goldsrc_manual_client_disconnect: slot="
                + std::to_string(requested_slot)
                + ",status=completed");
        return true;
    }

    void ProcessManualSessionEvents(
        const hl::network::GoldSrcManualSessionEvents& events)
    {
        if (!pmove_persistent_)
        {
            return;
        }
        if (events.movement_milestone_reached)
        {
            summary_.continuous_snapshot_stable = true;
            summary_.server_still_responsive = true;
            summary_.next_observed_boundary =
                "persistent_manual_session_active";
            summary_.previous_player_lifecycle_boundary_resolved = true;
            SyncManualSessionSummary();
            hl::common::Logger::Info(
                hl::common::LogCategory::Server,
                "goldsrc_movement_milestone_reached: mode=persistent"
                ",milestone_count="
                    + std::to_string(
                        manual_session_lifecycle_
                            .movement_milestone_count())
                    + ",commands_executed="
                    + std::to_string(summary_.movement_commands_executed)
                    + ",movement_snapshots="
                    + std::to_string(summary_.movement_snapshots_sent)
                    + ",runtime_active=1");
        }
        if (events.heartbeat_due)
        {
            ++summary_.manual_session_heartbeats;
            SyncManualSessionSummary();
            hl::common::Logger::Info(
                hl::common::LogCategory::Server,
                "goldsrc_manual_session_still_active: phase="
                    + summary_.manual_session_phase
                    + ",heartbeat="
                    + std::to_string(summary_.manual_session_heartbeats)
                    + ",clc_move_delivered="
                    + std::to_string(summary_.clc_move_delivered)
                    + ",continuous_snapshots_sent="
                    + std::to_string(summary_.continuous_snapshots_sent)
                    + ",movement_milestone="
                    + std::string(
                        summary_.movement_milestone_reached ? "1" : "0"));
        }
    }

    static bool ShouldLogDiagnosticSample(std::uint64_t count) noexcept
    {
        // Preserve the first few diagnostics, then sample at powers of two.
        // Summary counters remain exact without giving unauthenticated traffic
        // a linear disk/console I/O amplifier.
        return count <= 4 || (count != 0 && (count & (count - 1)) == 0);
    }

    void MarkComplete(std::string_view reason)
    {
        completion_reason_.assign(reason.data(), reason.size());
        complete_ = true;
    }

    bool FailBind(std::string reason)
    {
        summary_.last_reject_reason = std::move(reason);
        MarkComplete("bind_failure");
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
        MarkComplete("transport_failure");
        hl::common::Logger::Error(
            hl::common::LogCategory::Server,
            std::move(message));
        return false;
    }

    bool CallClientUserInfoChanged(
        edict_t* client,
        std::string& user_info) noexcept
    {
        if (client == nullptr
            || state_.dll_functions.pfnClientUserInfoChanged == nullptr)
        {
            return false;
        }
        __try
        {
            state_.dll_functions.pfnClientUserInfoChanged(
                client,
                user_info.data());
            return true;
        }
        __except (HandleSehException(
            "pfnClientUserInfoChanged",
            GetExceptionCode()))
        {
            return false;
        }
    }

    bool CallClientConnect(
        edict_t* client,
        const char* player_name,
        const char* address,
        std::array<
            char,
            hl::network::kGoldSrcClientRejectReasonBytes>& reject_reason,
        bool* accepted) noexcept
    {
        if (client == nullptr || player_name == nullptr || address == nullptr
            || accepted == nullptr
            || state_.dll_functions.pfnClientConnect == nullptr)
        {
            return false;
        }
        __try
        {
            *accepted = state_.dll_functions.pfnClientConnect(
                client,
                player_name,
                address,
                reject_reason.data()) != FALSE;
            return true;
        }
        __except (HandleSehException("pfnClientConnect", GetExceptionCode()))
        {
            return false;
        }
    }

    bool CallClientPutInServer(edict_t* client) noexcept
    {
        if (client == nullptr
            || state_.dll_functions.pfnClientPutInServer == nullptr)
        {
            return false;
        }
        __try
        {
            state_.dll_functions.pfnClientPutInServer(client);
            return true;
        }
        __except (HandleSehException(
            "pfnClientPutInServer",
            GetExceptionCode(),
            GetExceptionInformation()))
        {
            return false;
        }
    }

    bool CallClientDisconnect(edict_t* client) noexcept
    {
        if (client == nullptr
            || state_.dll_functions.pfnClientDisconnect == nullptr)
        {
            return false;
        }
        __try
        {
            state_.dll_functions.pfnClientDisconnect(client);
            return true;
        }
        // The lifecycle caller already records a bounded semantic failure.
        // Keep shutdown diagnostics free of exception codes and addresses.
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return false;
        }
    }

    enum class AddToFullPackResult
    {
        kIncluded,
        kOmitted,
        kFault,
    };

    AddToFullPackResult CallAddToFullPack(
        entity_state_t* output,
        int entity_index,
        edict_t* entity,
        edict_t* host) noexcept
    {
        if (output == nullptr || entity == nullptr || host == nullptr
            || state_.dll_functions.pfnAddToFullPack == nullptr)
        {
            return AddToFullPackResult::kFault;
        }
        __try
        {
            const int included = state_.dll_functions.pfnAddToFullPack(
                output,
                entity_index,
                entity,
                host,
                0,
                1,
                nullptr);
            return included != 0
                ? AddToFullPackResult::kIncluded
                : AddToFullPackResult::kOmitted;
        }
        // A callback fault is distinct from the stock omission return.  The
        // caller still fails closed, without exposing exception details.
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return AddToFullPackResult::kFault;
        }
    }

    bool CallUpdateClientData(
        const edict_t* client,
        clientdata_t* output,
        bool send_weapons) noexcept
    {
        if (client == nullptr || output == nullptr
            || state_.dll_functions.pfnUpdateClientData == nullptr)
        {
            return false;
        }
        hl::game_api::detail::PrepareGoldSrcClientDataForGameDll(output);
        __try
        {
            state_.dll_functions.pfnUpdateClientData(
                client,
                send_weapons ? 1 : 0,
                output);
            return true;
        }
        __except (send_weapons
            ? EXCEPTION_EXECUTE_HANDLER
            : HandleSehException(
                "pfnUpdateClientData",
                GetExceptionCode()))
        {
            return false;
        }
    }

    bool CallGetWeaponData(
        edict_t* client,
        weapon_data_t* output) noexcept
    {
        if (client == nullptr || output == nullptr
            || state_.dll_functions.pfnGetWeaponData == nullptr)
        {
            return false;
        }
        __try
        {
            return state_.dll_functions.pfnGetWeaponData(
                client,
                output) != 0;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return false;
        }
    }

    void SyncPlayerLifecycleSummary(
        const DedicatedPlayerRuntimeSlot& slot_state) noexcept
    {
        const auto& lifecycle = slot_state.goldsrc_player_lifecycle;
        const auto& diagnostics = lifecycle.diagnostics();
        const auto& player = lifecycle.player();
        summary_.player_lifecycle_phase =
            std::string(hl::network::NameFor(lifecycle.phase()));
        summary_.client_edict_index =
            static_cast<int>(lifecycle.binding().edict_index);
        summary_.client_userinfo_changed_calls =
            static_cast<int>(diagnostics.userinfo_notifications);
        summary_.client_connect_calls =
            static_cast<int>(diagnostics.client_connect_calls);
        summary_.client_connect_accepts =
            static_cast<int>(diagnostics.client_connect_accepts);
        summary_.client_connect_rejections =
            static_cast<int>(diagnostics.client_connect_rejections);
        summary_.duplicate_client_connect_suppressed =
            static_cast<int>(diagnostics.duplicate_connect_suppressed);
        summary_.client_put_in_server_calls =
            static_cast<int>(diagnostics.client_put_in_server_calls);
        summary_.client_put_in_server_successes =
            static_cast<int>(diagnostics.client_put_in_server_successes);
        summary_.duplicate_client_put_in_server_suppressed =
            static_cast<int>(diagnostics.duplicate_put_in_server_suppressed);
        summary_.client_disconnect_calls =
            static_cast<int>(diagnostics.disconnect_calls);
        summary_.game_dll_client_connected =
            lifecycle.game_dll_connected();
        summary_.player_private_data_ready = player.private_data_ready;
        summary_.player_entity_ready = player.player_entity_ready;
        summary_.player_spawned = player.player_spawned;
        summary_.player_view_established =
            lifecycle.view_entity_index() != 0u;
        summary_.player_view_entity =
            static_cast<int>(lifecycle.view_entity_index());
        summary_.player_snapshot_frames =
            static_cast<int>(diagnostics.player_snapshot_frames);
        summary_.first_player_snapshot_frame_id =
            lifecycle.first_player_snapshot_frame_id().value_or(0u);
        summary_.player_signon_complete = lifecycle.signon_complete();
        summary_.gameplay_active = summary_.gameplay_active
            || lifecycle.gameplay_active();
    }

    bool BindPlayerLifecycleAtAdmission(
        DedicatedPlayerRuntimeSlot& slot_state,
        std::uint64_t network_session_generation)
    {
        hl::network::GoldSrcClientEdictBinding binding;
        const auto binding_result = lifecycle_edict_registry_.Bind(
            static_cast<std::uint16_t>(slot_state.slot),
            network_session_generation,
            &binding);
        if (binding_result
                != hl::network::GoldSrcClientEdictBindingResult::kBound
            || slot_state.goldsrc_player_lifecycle.BindClientEdict(binding)
                != hl::network::GoldSrcPlayerLifecycleResult::kAdvanced
            || slot_state.goldsrc_player_lifecycle.RecordInitialUserInfo(
                slot_state.player_name)
                != hl::network::GoldSrcPlayerLifecycleResult::kAdvanced)
        {
            return FailTransport(
                "player-lifecycle-edict-bind-failed",
                "goldsrc_player_lifecycle_failed: reason=edict-bind");
        }
        edict_t* const client =
            state_.edict_store.EntityOfIndex(binding.edict_index);
        if (client == nullptr
            || state_.edict_store.IndexOf(client) != binding.edict_index
            || !CallClientUserInfoChanged(client, slot_state.user_info))
        {
            return FailTransport(
                "player-lifecycle-userinfo-callback-failed",
                "goldsrc_player_lifecycle_failed: reason=userinfo-callback");
        }
        summary_.deterministic_client_edict_contract_verified = true;
        summary_.game_dll_player_callbacks_verified =
            state_.dll_functions.pfnClientConnect != nullptr
            && state_.dll_functions.pfnClientUserInfoChanged != nullptr
            && state_.dll_functions.pfnClientPutInServer != nullptr
            && state_.dll_functions.pfnClientDisconnect != nullptr
            && state_.dll_functions.pfnAddToFullPack != nullptr
            && state_.dll_functions.pfnUpdateClientData != nullptr;
        SyncPlayerLifecycleSummary(slot_state);
        hl::common::Logger::Info(
            hl::common::LogCategory::Server,
            "goldsrc_player_edict_bound: session_id="
                + slot_state.session_id
                + ",slot=" + std::to_string(slot_state.slot)
                + ",edict=" + std::to_string(binding.edict_index)
                + ",generation="
                + std::to_string(binding.edict_generation)
                + ",userinfo_callback=1");
        return true;
    }

    bool ConnectPlayerToGameDll(
        DedicatedPlayerRuntimeSlot& slot_state)
    {
        bool callback_safe = true;
        const hl::network::GoldSrcPlayerLifecycleResult result =
            slot_state.goldsrc_player_lifecycle.ConnectGameDll(
                slot_state.player_name,
                slot_state.remote_endpoint,
                [this, &callback_safe](
                    const hl::network::GoldSrcClientEdictBinding& binding,
                    std::string_view player_name,
                    std::string_view network_address,
                    std::array<
                        char,
                        hl::network::kGoldSrcClientRejectReasonBytes>&
                        reject_reason)
                {
                    edict_t* const client =
                        state_.edict_store.EntityOfIndex(
                            binding.edict_index);
                    const std::string name(player_name);
                    const std::string address(network_address);
                    bool accepted = false;
                    callback_safe = CallClientConnect(
                        client,
                        name.c_str(),
                        address.c_str(),
                        reject_reason,
                        &accepted);
                    return callback_safe && accepted;
                });
        SyncPlayerLifecycleSummary(slot_state);
        if (!callback_safe)
        {
            return FailTransport(
                "player-lifecycle-client-connect-callback-failed",
                "goldsrc_player_lifecycle_failed: reason=client-connect-callback");
        }
        if (result
            == hl::network::GoldSrcPlayerLifecycleResult::kGameDllRejected)
        {
            summary_.last_reject_reason =
                std::string(
                    slot_state.goldsrc_player_lifecycle.reject_reason());
            return FailTransport(
                "player-lifecycle-client-connect-rejected",
                "goldsrc_player_lifecycle_rejected: reason=game-dll");
        }
        if (result
                != hl::network::GoldSrcPlayerLifecycleResult::kAdvanced
            && result
                != hl::network::GoldSrcPlayerLifecycleResult::kAlreadyApplied)
        {
            return FailTransport(
                "player-lifecycle-client-connect-transition-failed",
                "goldsrc_player_lifecycle_failed: reason=client-connect-transition");
        }
        if (slot_state.goldsrc_player_lifecycle.EnterAwaitingPutInServer()
                != hl::network::GoldSrcPlayerLifecycleResult::kAdvanced
            && slot_state.goldsrc_player_lifecycle.phase()
                != hl::network::GoldSrcPlayerLifecyclePhase::
                    kAwaitingPutInServer)
        {
            return FailTransport(
                "player-lifecycle-await-put-transition-failed",
                "goldsrc_player_lifecycle_failed: reason=await-put-transition");
        }
        SyncPlayerLifecycleSummary(slot_state);
        hl::common::Logger::Info(
            hl::common::LogCategory::Server,
            "goldsrc_game_dll_client_connected: session_id="
                + slot_state.session_id
                + ",edict="
                + std::to_string(
                    slot_state.goldsrc_player_lifecycle.binding()
                        .edict_index)
                + ",client_connect_calls="
                + std::to_string(summary_.client_connect_calls)
                + ",next=awaiting_put_in_server");
        return true;
    }

    static bool PlayerNetworkStateFinite(const entvars_t& vars) noexcept
    {
        const auto vector_finite = [](const Vector& value)
        {
            return std::isfinite(value.x)
                && std::isfinite(value.y)
                && std::isfinite(value.z);
        };
        return vector_finite(vars.origin)
            && vector_finite(vars.angles)
            && vector_finite(vars.velocity)
            && vector_finite(vars.view_ofs)
            && vector_finite(vars.mins)
            && vector_finite(vars.maxs)
            && std::isfinite(vars.health)
            && std::isfinite(vars.armorvalue)
            && std::isfinite(vars.fov)
            && std::isfinite(vars.animtime)
            && std::isfinite(vars.framerate);
    }

    bool ReconcilePlayerLifecycle(
        DedicatedPlayerRuntimeSlot& slot_state)
    {
        if (slot_state.goldsrc_player_lifecycle.put_in_server())
        {
            return true;
        }
        bool callback_safe = true;
        const hl::network::GoldSrcPlayerLifecycleResult result =
            slot_state.goldsrc_player_lifecycle
                .PutInServerFromReconciliation(
                    [this, &slot_state, &callback_safe](
                        const hl::network::GoldSrcClientEdictBinding&
                            binding)
                    {
                        hl::network::GoldSrcPlayerMaterialization player;
                        if (lifecycle_edict_registry_.Validate(binding)
                            != hl::network::
                                GoldSrcClientEdictBindingResult::kBound)
                        {
                            return player;
                        }
                        state_.edict_store.PrepareClientForPutInServer(
                            state_.edict_store.EntityOfIndex(
                                binding.edict_index));
                        PrimeDedicatedClientEdict(state_, slot_state);
                        edict_t* const client =
                            state_.edict_store.EntityOfIndex(
                                binding.edict_index);
                        if (client == nullptr)
                        {
                            return player;
                        }
                        client->v.colormap = binding.edict_index;
                        client->v.netname =
                            state_.string_pool.Alloc(slot_state.player_name);
                        state_.globalvars.time =
                            static_cast<float>(network_server_time_);
                        callback_safe = CallClientPutInServer(client);
                        player.private_data_ready =
                            callback_safe
                            && state_.edict_store.PrivateDataOf(client)
                                != nullptr;
                        player.player_entity_ready =
                            callback_safe && client->free == FALSE
                            && state_.edict_store.IndexOf(client)
                                == binding.edict_index;
                        player.player_spawned =
                            player.player_entity_ready
                            && client->v.modelindex > 0
                            && client->v.model != 0
                            && client->v.solid != SOLID_NOT;
                        player.finite_network_state =
                            player.player_entity_ready
                            && PlayerNetworkStateFinite(client->v);
                        if (client->v.modelindex > 0
                            && client->v.modelindex
                                <= hl::network::
                                    kGoldSrcMaximumModelIndex)
                        {
                            player.model_index =
                                static_cast<std::uint16_t>(
                                    client->v.modelindex);
                        }
                        if (player.private_data_ready
                            && player.player_entity_ready
                            && player.player_spawned
                            && player.finite_network_state
                            && player.model_index != 0u)
                        {
                            state_.edict_store.SetSpawned(client, true);
                        }
                        return player;
                    });
        SyncPlayerLifecycleSummary(slot_state);
        if (!callback_safe)
        {
            return FailTransport(
                "player-lifecycle-put-in-server-callback-failed",
                "goldsrc_player_lifecycle_failed: reason=put-in-server-callback");
        }
        if (result
            != hl::network::GoldSrcPlayerLifecycleResult::kAdvanced)
        {
            return FailTransport(
                "player-lifecycle-materialization-failed",
                "goldsrc_player_lifecycle_failed: reason="
                    + std::string(hl::network::ReasonFor(result)));
        }
        const std::uint16_t view_entity =
            slot_state.goldsrc_player_lifecycle.binding().edict_index;
        if (slot_state.goldsrc_player_lifecycle.EstablishView(view_entity)
            != hl::network::GoldSrcPlayerLifecycleResult::kAdvanced)
        {
            return FailTransport(
                "player-lifecycle-view-establishment-failed",
                "goldsrc_player_lifecycle_failed: reason=view-establishment");
        }
        slot_state.put_in_server = true;
        slot_state.spawn_count = std::max(1, slot_state.spawn_count);
        slot_state.alive = false;
        summary_.player_lifecycle_trigger =
            "stable_snapshot_ack_reconciliation_after_premature_signonnum_1";
        summary_.player_lifecycle_contract_verified = true;
        summary_.game_dll_callback_order_verified =
            summary_.client_userinfo_changed_calls == 1
            && summary_.client_connect_calls == 1
            && summary_.client_put_in_server_calls == 1;
        summary_.view_entity_contract_verified = true;
        SyncPlayerLifecycleSummary(slot_state);
        SyncClientReplicationSummary(slot_state);
        RefreshTwoClientReplicationSummary();
        hl::common::Logger::Info(
            hl::common::LogCategory::Server,
            "goldsrc_player_materialized: session_id="
                + slot_state.session_id
                + ",edict=" + std::to_string(view_entity)
                + ",private_data=1,player_entity=1,spawned=1"
                  ",view_established=1,gameplay_active=0");
        return true;
    }

    bool DisconnectPlayerLifecycle(
        DedicatedPlayerRuntimeSlot& slot_state,
        std::string_view reason)
    {
        const hl::network::GoldSrcClientEdictBinding binding =
            slot_state.goldsrc_player_lifecycle.binding();
        bool callback_safe = true;
        const auto result =
            slot_state.goldsrc_player_lifecycle.Disconnect(
                [this, &callback_safe](
                    const hl::network::GoldSrcClientEdictBinding&
                        disconnected)
                {
                    callback_safe = CallClientDisconnect(
                        state_.edict_store.EntityOfIndex(
                            disconnected.edict_index));
                });
        if (!callback_safe
            || (result
                    != hl::network::GoldSrcPlayerLifecycleResult::kAdvanced
                && result
                    != hl::network::
                        GoldSrcPlayerLifecycleResult::kAlreadyApplied))
        {
            return false;
        }
        if (result
                == hl::network::GoldSrcPlayerLifecycleResult::kAdvanced
            && binding.valid)
        {
            edict_t* const client = state_.edict_store.EntityOfIndex(
                binding.edict_index);
            if (client != nullptr)
            {
                state_.edict_store.SetSpawned(client, false);
                state_.edict_store.SetActivationCandidate(client, false);
                client->v.flags &= FL_DORMANT;
                client->v.takedamage = DAMAGE_NO;
                client->v.solid = SOLID_NOT;
            }
        }
        if (binding.valid)
        {
            const auto invalidated =
                lifecycle_edict_registry_.Disconnect(binding);
            if (invalidated
                    != hl::network::
                        GoldSrcClientEdictBindingResult::kBound
                && invalidated
                    != hl::network::
                        GoldSrcClientEdictBindingResult::kNotBound)
            {
                return false;
            }
        }
        summary_.client_disconnect_calls = static_cast<int>(
            slot_state.goldsrc_player_lifecycle.diagnostics()
                .disconnect_calls);
        hl::common::Logger::Info(
            hl::common::LogCategory::Server,
            "goldsrc_game_dll_client_disconnected: session_id="
                + slot_state.session_id
                + ",reason=" + SanitizeLogField(std::string(reason))
                + ",callback_count="
                + std::to_string(summary_.client_disconnect_calls));
        return true;
    }

    void MarkTimeout()
    {
        summary_.timed_out = true;
        summary_.last_reject_reason = accepted_ && world_baselines_enabled_
            ? "world-baseline-timeout"
            : accepted_ && resource_manifest_enabled_
            ? "resource-manifest-timeout"
            : accepted_ && delta_descriptions_enabled_
            ? "delta-descriptions-timeout"
            : accepted_ && serverinfo_enabled_
            ? "serverinfo-timeout"
            : accepted_ && netchan_enabled_
            ? "netchan-timeout"
            : "handshake-timeout";
        MarkComplete("timeout");
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
            if (world_baselines_enabled_)
            {
                if (!metadata.netchan
                         .SetIncomingNormalFragmentAcceptance(true))
                {
                    return false;
                }
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
        if (player_lifecycle_enabled_
            && !BindPlayerLifecycleAtAdmission(
                slot_state,
                next_network_session_))
        {
            return false;
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
                DedicatedPlayerRuntimeSlot* const slot_state =
                    handshake.admission.has_value()
                    ? FindDedicatedPlayerRuntimeSlotBySession(
                        state_.dedicated_multiplayer_foundation,
                        handshake.admission->session_identifier)
                    : nullptr;
                if (slot_state == nullptr
                    || !SendInitialNetchanDatagram(*slot_state))
                {
                    return false;
                }
                slot_state->goldsrc_negative_advance_pending =
                    negative_proof_;
            }
            else
            {
                MarkComplete("connectionless_only_complete");
            }
        }
        return true;
    }

    hl::game_api::GoldSrcClientReplicationSummary*
    FindClientReplicationSummary(int slot) noexcept
    {
        if (slot < 1
            || static_cast<std::size_t>(slot)
                > summary_.client_replication.size())
        {
            return nullptr;
        }
        return &summary_.client_replication[
            static_cast<std::size_t>(slot - 1)];
    }

    int ConnectedClientCount() const noexcept
    {
        return CountDedicatedQueryPlayers(
            state_.dedicated_multiplayer_foundation);
    }

    bool PersistentDeadlineSuppressed() const noexcept
    {
        return pmove_persistent_
            && (manual_session_lifecycle_.global_deadline_suppressed()
                || ConnectedClientCount() > 0);
    }

    void SyncClientReplicationSummary(
        const DedicatedPlayerRuntimeSlot& slot_state)
    {
        hl::game_api::GoldSrcClientReplicationSummary* client =
            FindClientReplicationSummary(slot_state.slot);
        if (client == nullptr)
        {
            return;
        }
        client->edict = slot_state.slot;
        client->connected = client->connected
            || IsDedicatedSlotConnectedOccupant(slot_state);
        client->put_in_server = client->put_in_server
            || slot_state.goldsrc_player_lifecycle.put_in_server();
        client->spawned = client->spawned
            || slot_state.goldsrc_player_lifecycle.player().player_spawned;
        client->signon_complete = client->signon_complete
            || slot_state.goldsrc_player_lifecycle.signon_complete();
        client->private_data_ready = client->private_data_ready
            || slot_state.goldsrc_player_lifecycle.player().private_data_ready;
        if (slot_state.goldsrc_signon.phase()
            != hl::network::GoldSrcSignonPhase::kNone)
        {
            client->signon_phase = std::string(
                hl::network::NameFor(slot_state.goldsrc_signon.phase()));
        }
        client->frame_history_independent =
            client->frame_history_independent
            || slot_state.goldsrc_first_snapshot.history().capacity()
                == hl::network::kGoldSrcClientFrameHistoryDepth;
        client->delta_base_independent = true;
        client->command_clock_independent = pmove_enabled_;
        client->pmove_state_independent = pmove_enabled_;
        if (slot_state.goldsrc_player_lifecycle.put_in_server())
        {
            client->session_generation = slot_state
                .goldsrc_player_lifecycle.binding()
                .network_session_generation;
        }
        if (slot_state.slot >= 1
            && static_cast<std::size_t>(slot_state.slot)
                <= stock_test_spawn_anchors_.size())
        {
            const StockTestSpawnAnchor& anchor = stock_test_spawn_anchors_[
                static_cast<std::size_t>(slot_state.slot - 1)];
            client->spawn_anchor_valid = anchor.valid
                && anchor.session_generation == client->session_generation;
            client->spawn_anchor_origin_x = anchor.origin.x;
            client->spawn_anchor_origin_y = anchor.origin.y;
            client->spawn_anchor_origin_z = anchor.origin.z;
        }
        client->pmove_calls = slot_state.goldsrc_pmove_calls;
        client->snapshots_due = slot_state.goldsrc_snapshots_due;
        client->snapshots_deferred = slot_state.goldsrc_snapshots_deferred;
        client->snapshots_starved = slot_state.goldsrc_snapshots_starved;
        client->ordinary_ack_datagrams_sent =
            slot_state.goldsrc_ordinary_ack_datagrams_sent;
        client->ordinary_ack_datagrams_coalesced =
            slot_state.goldsrc_ordinary_ack_datagrams_coalesced;
        client->ordinary_ack_datagrams_deferred =
            slot_state.goldsrc_ordinary_ack_datagrams_deferred;
        client->last_successful_move_time_ms =
            slot_state.goldsrc_last_successful_move_time_ms;
        client->last_successful_snapshot_time_ms =
            slot_state.goldsrc_last_successful_snapshot_time_ms;
        client->last_successful_frame_ack_time_ms =
            slot_state.goldsrc_last_successful_frame_ack_time_ms;
        const auto& schedule =
            slot_state.goldsrc_first_snapshot.scheduler().state();
        client->snapshot_interval_minimum_ms = static_cast<int>(std::llround(
            schedule.minimum_due_interval_seconds * 1000.0));
        client->snapshot_interval_median_ms =
            static_cast<int>(schedule.median_due_interval_msec);
        client->snapshot_interval_p95_ms =
            static_cast<int>(schedule.p95_due_interval_msec);
        client->snapshot_interval_maximum_ms = static_cast<int>(std::llround(
            schedule.maximum_due_interval_seconds * 1000.0));
        if (pmove_enabled_)
        {
            if (const auto* command_state = pmove_runtime_.CommandState(
                    static_cast<std::size_t>(slot_state.slot));
                command_state != nullptr)
            {
                const auto& commands = command_state->diagnostics();
                client->move_packets_received = commands.packets_received;
                client->move_packets_validated =
                    commands.move_packets_validated;
                client->move_packets_executed = commands.move_packets_executed;
                client->move_packets_temporarily_rejected =
                    commands.move_packets_temporarily_rejected;
                client->movement_commands_executed =
                    commands.commands_executed;
                client->command_clock_rejections =
                    commands.move_packets_temporarily_rejected;
                client->command_clock_recoveries =
                    commands.command_clock_recoveries;
            }
        }
        if (slot_state.goldsrc_player_lifecycle.put_in_server())
        {
            const auto& binding =
                slot_state.goldsrc_player_lifecycle.binding();
            if (const edict_t* player =
                    state_.edict_store.EntityOfIndex(binding.edict_index);
                player != nullptr)
            {
                client->authoritative_origin_x = player->v.origin.x;
                client->authoritative_origin_y = player->v.origin.y;
                client->authoritative_origin_z = player->v.origin.z;
                client->authoritative_velocity_x = player->v.velocity.x;
                client->authoritative_velocity_y = player->v.velocity.y;
                client->authoritative_velocity_z = player->v.velocity.z;
            }
        }
    }

    void RefreshTwoClientReplicationSummary()
    {
        summary_.connected_client_count = (std::max)(
            summary_.connected_client_count,
            ConnectedClientCount());
        std::vector<const void*> private_data;
        int ready_edicts = 0;
        for (const DedicatedPlayerRuntimeSlot& slot_state :
             state_.dedicated_multiplayer_foundation.slots)
        {
            SyncClientReplicationSummary(slot_state);
            if (!slot_state.goldsrc_player_lifecycle.put_in_server())
            {
                continue;
            }
            const auto& binding =
                slot_state.goldsrc_player_lifecycle.binding();
            edict_t* const player =
                state_.edict_store.EntityOfIndex(binding.edict_index);
            const void* const data = player == nullptr
                ? nullptr
                : state_.edict_store.PrivateDataOf(player);
            if (binding.edict_index == slot_state.slot && data != nullptr)
            {
                ++ready_edicts;
                private_data.push_back(data);
            }
        }
        summary_.two_distinct_edicts = summary_.two_distinct_edicts
            || ready_edicts >= 2;
        summary_.two_distinct_private_data =
            summary_.two_distinct_private_data
            || (private_data.size() >= 2u
                && private_data[0] != private_data[1]);
        if (summary_.client_replication.size() >= 2u)
        {
            const auto& a = summary_.client_replication[0];
            const auto& b = summary_.client_replication[1];
            summary_.mutual_player_visibility =
                (a.remote_player_adds + a.remote_player_updates) > 0u
                && (b.remote_player_adds + b.remote_player_updates) > 0u;
            summary_.remote_player_movement_smooth =
                summary_.mutual_player_visibility
                && a.remote_origin_discontinuities == 0u
                && b.remote_origin_discontinuities == 0u
                && a.remote_update_interval_p95_ms <= 80
                && b.remote_update_interval_p95_ms <= 80
                && a.maximum_remote_snapshot_gap_ms <= 160
                && b.maximum_remote_snapshot_gap_ms <= 160
                && a.remote_animtime_monotonic
                && b.remote_animtime_monotonic
                && !a.permanent_remote_nointerp
                && !b.permanent_remote_nointerp
                && a.remote_entity_present_consecutively
                && b.remote_entity_present_consecutively;
            summary_.remote_interpolation_contract_verified =
                summary_.remote_player_movement_smooth;
            summary_.reconnect_slot_reuse =
                a.reconnect_count > 0 && a.slot_reuse_clean;
        }
        RefreshCombatEvidenceSummary();
    }

    void LogClientProgressSamples()
    {
        const std::uint64_t server_time_ms =
            static_cast<std::uint64_t>(std::llround(
                (std::max)(0.0, network_server_time_) * 1000.0));
        bool emitted_progress = false;
        int connected_clients = 0;
        int spawned_players = 0;
        for (DedicatedPlayerRuntimeSlot& slot_state :
             state_.dedicated_multiplayer_foundation.slots)
        {
            if (IsDedicatedSlotConnectedOccupant(slot_state))
            {
                ++connected_clients;
                if (slot_state.goldsrc_player_lifecycle.put_in_server())
                {
                    ++spawned_players;
                }
            }
            if (!IsDedicatedSlotConnectedOccupant(slot_state)
                || !slot_state.goldsrc_player_lifecycle.put_in_server()
                || network_server_time_
                    - slot_state.goldsrc_last_progress_log_server_time < 1.0)
            {
                continue;
            }
            slot_state.goldsrc_last_progress_log_server_time =
                network_server_time_;
            SyncClientReplicationSummary(slot_state);
            const hl::game_api::GoldSrcClientReplicationSummary* const client =
                FindClientReplicationSummary(slot_state.slot);
            if (client == nullptr)
            {
                continue;
            }
            emitted_progress = true;
            const edict_t* const player = state_.edict_store.EntityOfIndex(
                slot_state.goldsrc_player_lifecycle.binding().edict_index);
            const auto* const combat = pmove_runtime_.CombatDiagnostics(
                static_cast<std::size_t>(slot_state.slot));
            const bool movement_stalled = client->move_packets_received > 0u
                && slot_state.goldsrc_last_successful_move_time_ms > 0u
                && server_time_ms
                    > slot_state.goldsrc_last_successful_move_time_ms + 2000u;
            const bool frame_ack_stalled = client->snapshots_sent > 0u
                && slot_state.goldsrc_last_successful_frame_ack_time_ms > 0u
                && server_time_ms
                    > slot_state.goldsrc_last_successful_frame_ack_time_ms
                        + 2000u;
            if (movement_stalled)
            {
                ++movement_stalled_diagnostics_seen_;
                if (ShouldLogDiagnosticSample(
                        movement_stalled_diagnostics_seen_))
                {
                    hl::common::Logger::Warn(
                        hl::common::LogCategory::Server,
                        "goldsrc_client_movement_stalled: slot="
                            + std::to_string(slot_state.slot)
                            + ",stall_ms="
                            + std::to_string(
                                server_time_ms
                                - slot_state
                                    .goldsrc_last_successful_move_time_ms));
                }
            }
            if (frame_ack_stalled)
            {
                ++frame_ack_stalled_diagnostics_seen_;
                if (ShouldLogDiagnosticSample(
                        frame_ack_stalled_diagnostics_seen_))
                {
                    hl::common::Logger::Warn(
                        hl::common::LogCategory::Server,
                        "goldsrc_frame_ack_stalled: slot="
                            + std::to_string(slot_state.slot)
                            + ",stall_ms="
                            + std::to_string(
                                server_time_ms
                                - slot_state
                                    .goldsrc_last_successful_frame_ack_time_ms));
                }
            }
            hl::common::Logger::Info(
                hl::common::LogCategory::Server,
                "goldsrc_client_progress: slot="
                    + std::to_string(slot_state.slot)
                    + ",connected=1"
                    + ",signon_complete="
                    + (client->signon_complete ? "1" : "0")
                    + ",spawned=" + (client->spawned ? "1" : "0")
                    + ",edict=" + std::to_string(client->edict)
                    + ",client_endpoint_port="
                    + std::to_string(
                        slot_state.netchan.remote_endpoint().port)
                    + ",server_time_ms=" + std::to_string(server_time_ms)
                    + ",combat_phase=" + client->combat_phase
                    + ",combat_attack_received="
                    + std::to_string(client->attack_commands_received)
                    + ",combat_attack_executed="
                    + std::to_string(client->attack_commands_executed)
                    + ",combat_prethink="
                    + std::to_string(
                        combat != nullptr
                            ? combat->player_prethink_calls
                            : 0u)
                    + ",combat_postthink="
                    + std::to_string(
                        combat != nullptr
                            ? combat->player_postthink_calls
                            : 0u)
                    + ",combat_callback_failures="
                    + std::to_string(
                        combat != nullptr
                            ? combat->callback_failures
                            : 0u)
                    + ",combat_active_weapon="
                    + std::to_string(client->active_weapon_id)
                    + ",combat_glock_present="
                    + (client->glock_entity_present ? "1" : "0")
                    + ",combat_glock_clip="
                    + std::to_string(client->glock_clip)
                    + ",combat_clientdata_health="
                    + std::to_string(client->clientdata_health)
                    + ",combat_shot_traces="
                    + std::to_string(client->weapon_range_trace_calls)
                    + ",combat_shooter_ignored="
                    + std::to_string(
                        client->weapon_range_shooter_ignored)
                    + ",combat_shot_player_hits="
                    + std::to_string(client->weapon_range_player_hits)
                    + ",combat_shot_world_hits="
                    + std::to_string(client->weapon_range_world_hits)
                    + ",combat_last_target="
                    + std::to_string(client->last_trace_target_slot)
                    + ",incoming_sequence="
                    + std::to_string(slot_state.netchan.incoming_sequence())
                    + ",outgoing_sequence="
                    + std::to_string(slot_state.netchan.outgoing_sequence())
                    + ",clc_move_received="
                    + std::to_string(client->move_packets_received)
                    + ",clc_move_validated="
                    + std::to_string(client->move_packets_validated)
                    + ",clc_move_executed="
                    + std::to_string(client->move_packets_executed)
                    + ",clc_move_temporarily_rejected="
                    + std::to_string(
                        client->move_packets_temporarily_rejected)
                    + ",command_clock_recoveries="
                    + std::to_string(client->command_clock_recoveries)
                    + ",pmove_calls=" + std::to_string(client->pmove_calls)
                    + ",movement_commands_executed="
                    + std::to_string(client->movement_commands_executed)
                    + ",forward_input_packets="
                    + std::to_string(client->forward_input_packets)
                    + ",backward_input_packets="
                    + std::to_string(client->backward_input_packets)
                    + ",strafe_input_packets="
                    + std::to_string(client->strafe_input_packets)
                    + ",jump_input_packets="
                    + std::to_string(client->jump_input_packets)
                    + ",duck_input_packets="
                    + std::to_string(client->duck_input_packets)
                    + ",movement_snapshots="
                    + std::to_string(client->movement_snapshots_sent)
                    + ",snapshots_due="
                    + std::to_string(client->snapshots_due)
                    + ",snapshots_sent="
                    + std::to_string(client->snapshots_sent)
                    + ",snapshots_deferred="
                    + std::to_string(client->snapshots_deferred)
                    + ",snapshots_starved="
                    + std::to_string(client->snapshots_starved)
                    + ",remote_updates="
                    + std::to_string(client->remote_player_updates)
                    + ",remote_adds="
                    + std::to_string(client->remote_player_adds)
                    + ",remote_removes="
                    + std::to_string(client->remote_player_removes)
                    + ",remote_readds="
                    + std::to_string(client->remote_player_readds)
                    + ",remote_update_p95_ms="
                    + std::to_string(
                        client->remote_update_interval_p95_ms)
                    + ",maximum_remote_snapshot_gap_ms="
                    + std::to_string(client->maximum_remote_snapshot_gap_ms)
                    + ",frames_acknowledged="
                    + std::to_string(client->frames_acknowledged)
                    + ",latest_acknowledged_frame="
                    + std::to_string(
                        slot_state.goldsrc_first_snapshot.history()
                                .last_acknowledged_frame()
                                .value_or(0u))
                    + ",last_move_received_server_time_ms="
                    + std::to_string(
                        client->last_successful_move_time_ms)
                    + ",last_move_executed_server_time_ms="
                    + std::to_string(
                        client->last_successful_move_time_ms)
                    + ",last_successful_move_time_ms="
                    + std::to_string(
                        client->last_successful_move_time_ms)
                    + ",last_successful_snapshot_time_ms="
                    + std::to_string(
                        client->last_successful_snapshot_time_ms)
                    + ",last_successful_frame_ack_time_ms="
                    + std::to_string(
                        client->last_successful_frame_ack_time_ms)
                    + ",pending_empty_ack="
                    + (slot_state.goldsrc_pending_empty_ack ? "1" : "0")
                    + ",origin_x="
                    + std::to_string(client->authoritative_origin_x)
                    + ",origin_y="
                    + std::to_string(client->authoritative_origin_y)
                    + ",origin_z="
                    + std::to_string(client->authoritative_origin_z)
                    + ",velocity_x="
                    + std::to_string(client->authoritative_velocity_x)
                    + ",velocity_y="
                    + std::to_string(client->authoritative_velocity_y)
                    + ",velocity_z="
                    + std::to_string(client->authoritative_velocity_z)
                    + ",health="
                    + std::to_string(player != nullptr ? player->v.health : 0.0f)
                    + ",deadflag="
                    + std::to_string(player != nullptr ? player->v.deadflag : -1)
                    + ",movetype="
                    + std::to_string(player != nullptr ? player->v.movetype : -1)
                    + ",solid="
                    + std::to_string(player != nullptr ? player->v.solid : -1)
                    + ",flags="
                    + std::to_string(player != nullptr ? player->v.flags : 0)
                    + ",maxspeed="
                    + std::to_string(player != nullptr ? player->v.maxspeed : 0.0f)
                    + ",session_generation="
                    + std::to_string(client->session_generation)
                    + ",spawn_anchor_valid="
                    + (client->spawn_anchor_valid ? "1" : "0")
                    + ",spawn_anchor_x="
                    + std::to_string(client->spawn_anchor_origin_x)
                    + ",spawn_anchor_y="
                    + std::to_string(client->spawn_anchor_origin_y)
                    + ",spawn_anchor_z="
                    + std::to_string(client->spawn_anchor_origin_z)
                    + ",test_reset_requests="
                    + std::to_string(summary_.test_reset_requests)
                    + ",test_reset_applied="
                    + std::to_string(summary_.test_reset_applied)
                    + ",test_reset_rejected="
                    + std::to_string(summary_.test_reset_rejected)
                    + ",reset_transition_nointerp_frames="
                    + std::to_string(
                        client->reset_transition_nointerp_frames)
                    + ",nointerp_cleared_after_reset="
                    + (client->nointerp_cleared_after_reset ? "1" : "0")
                    + ",interpolation_samples_excluded_for_reset="
                    + std::to_string(
                        client->interpolation_samples_excluded_for_reset));
        }
        if (emitted_progress)
        {
            hl::common::Logger::Info(
                hl::common::LogCategory::Server,
                "goldsrc_two_client_progress: connected_clients="
                    + std::to_string(connected_clients)
                    + ",spawned_players=" + std::to_string(spawned_players)
                    + ",host_frames="
                    + std::to_string(summary_.host_frames_pumped)
                    + ",ordinary_acks_sent="
                    + std::to_string(summary_.ordinary_ack_datagrams_sent)
                    + ",ordinary_acks_coalesced="
                    + std::to_string(
                        summary_.ordinary_ack_datagrams_coalesced)
                    + ",snapshot_budget_starvations="
                    + std::to_string(summary_.snapshot_budget_starvations)
                    + ",maximum_snapshot_starvation_frames="
                    + std::to_string(
                        summary_.maximum_consecutive_snapshot_starvation_frames)
                    + ",cross_client_state_leak="
                    + (summary_.cross_client_state_leak ? "1" : "0")
                    + ",transport_failed="
                    + (transport_failed_ ? "1" : "0")
                    + ",last_failure_reason=none");
        }
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
        summary_.duplicate_sessions = 0;
        summary_.partial_sessions = partial_sessions;
        RefreshTwoClientReplicationSummary();
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
            summary_.clc_move_delivered = static_cast<int>(
                signon_diagnostics.post_resource_command_delivered);
            summary_.post_resource_state_advances = static_cast<int>(
                signon_diagnostics.post_resource_command_state_advances);
            summary_.clc_move_invalid_phase_rejected = static_cast<int>(
                signon_diagnostics.post_resource_command_wrong_phase);
            summary_.duplicate_resource_request_suppressed =
                signon_diagnostics
                    .duplicate_resource_request_suppressed > 0u;
            summary_.signon_phase = std::string(
                hl::network::NameFor(slot_state.goldsrc_signon.phase()));
            if (first_snapshot_enabled_
                && slot_state.goldsrc_first_snapshot.phase()
                    != hl::network::GoldSrcFirstSnapshotPhase::kNone)
            {
                summary_.signon_phase = std::string(
                    hl::network::NameFor(
                        slot_state.goldsrc_first_snapshot.phase()));
            }
        }
        if (player_lifecycle_enabled_)
        {
            SyncPlayerLifecycleSummary(slot_state);
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

    void ResetWorldBaselineSessionSummary() noexcept
    {
        // The encoded world baseline is map-scoped and may be reused, but its
        // reliable delivery state belongs to one admitted netchan session.
        summary_.baseline_queued = 0;
        summary_.baseline_sent = 0;
        summary_.baseline_resent = 0;
        summary_.baseline_acked = 0;
        summary_.baseline_fragmented = false;
        summary_.baseline_fragment_count = 0;
        summary_.baseline_fragment_acknowledged_count = 0;
        summary_.send_entities_received = 0;
        summary_.send_entities_delivered = 0;
        summary_.previous_server_baseline_boundary_resolved = false;
    }

    void ResetSnapshotSessionSummary() noexcept
    {
        summary_.snapshot_contract_verified = false;
        summary_.snapshot_message_order_verified = false;
        summary_.frame_ack_contract_verified = false;
        summary_.frame_history_implemented = false;
        summary_.frame_history_depth = 0;
        summary_.clientdata_implemented = false;
        summary_.weapon_data_required = false;
        summary_.packet_entities_implemented = false;
        summary_.first_snapshot_entity_count = 0;
        summary_.first_snapshot_payload_bytes = 0;
        summary_.first_snapshot_frame_id = 0u;
        summary_.first_snapshot_server_time = 0.0f;
        summary_.first_snapshot_visibility_policy = "none";
        summary_.frame_ack_source = "none";
        summary_.first_snapshot_prepared = 0;
        summary_.first_snapshot_sent = 0;
        summary_.first_snapshot_acked = 0;
        summary_.duplicate_frame_acknowledgements = 0;
        summary_.unknown_frame_references = 0;
        summary_.future_frame_references = 0;
        summary_.evicted_frame_references = 0;
        summary_.stale_frame_references = 0;
        summary_.ambiguous_frame_references = 0;
        summary_.previous_first_snapshot_boundary_resolved = false;
        summary_.stock_advanced_past_previous_boundary = false;

        summary_.continuous_snapshot_contract_verified = false;
        summary_.delta_packet_entities_contract_verified = false;
        summary_.frame_reference_resolution_contract_verified = false;
        summary_.loss_recovery_contract_verified = false;
        summary_.minimum_snapshot_gap_ms = 0;
        summary_.median_snapshot_interval_ms = 0;
        summary_.p95_snapshot_interval_ms = 0;
        summary_.maximum_snapshot_gap_ms = 0;
        summary_.snapshot_burst_count = 0;
        summary_.missed_snapshot_intervals = 0;
        summary_.continuous_snapshots_generated = 0;
        summary_.continuous_snapshots_sent = 0;
        summary_.continuous_snapshots_acked = 0;
        summary_.continuous_full_snapshots = 0;
        summary_.continuous_delta_snapshots = 0;
        summary_.continuous_full_fallbacks = 0;
        summary_.continuous_delta_recoveries = 0;
        summary_.continuous_snapshot_adds = 0;
        summary_.continuous_snapshot_updates = 0;
        summary_.continuous_snapshot_removes = 0;
        summary_.continuous_snapshot_unchanged = 0;
        summary_.distinct_frame_references = 0;
        summary_.continuous_server_time_monotonic = false;
        summary_.continuous_frame_ids_advanced = false;
        summary_.newest_acknowledged_base_selected = false;
        summary_.continuous_frame_history_bounded = false;
        summary_.continuous_snapshot_streaming = false;
        summary_.continuous_snapshot_stable = false;
        summary_.latest_continuous_frame_id = 0u;
        summary_.latest_continuous_base_frame_id = 0u;
        summary_.latest_continuous_server_time = 0.0f;
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

    static const hl::network::GoldSrcDeltaTable* FindBaselineDeltaTable(
        const hl::network::GoldSrcDeltaRegistry& registry,
        std::string_view name) noexcept
    {
        const hl::network::GoldSrcDeltaTable* found = nullptr;
        for (const hl::network::GoldSrcDeltaTable& table : registry.tables)
        {
            if (table.name != name)
            {
                continue;
            }
            if (found != nullptr)
            {
                return nullptr;
            }
            found = &table;
        }
        return found;
    }

    static bool CaptureDeltaState(
        const void* source,
        std::size_t source_size,
        const hl::network::GoldSrcDeltaTable& table,
        hl::network::GoldSrcDecodedDeltaRecord* output,
        std::string* failure_detail,
        bool normalize_nonfinite = false) noexcept
    {
        const auto fail = [failure_detail](
                              const hl::network::GoldSrcDeltaField* field,
                              std::string_view reason)
        {
            if (failure_detail != nullptr)
            {
                *failure_detail = "reason=" + std::string(reason);
                if (field != nullptr)
                {
                    *failure_detail += ",field=" + field->name
                        + ",offset="
                        + std::to_string(field->field_offset)
                        + ",type="
                        + std::to_string(field->field_type);
                }
            }
            return false;
        };
        if (failure_detail != nullptr)
        {
            failure_detail->clear();
        }
        if (source == nullptr || source_size == 0u || output == nullptr
            || table.fields.empty()
            || table.fields.size()
                > hl::network::kGoldSrcMaximumDeltaFieldsPerTable)
        {
            return fail(nullptr, "invalid-record");
        }
        output->field_count = table.fields.size();
        const auto* bytes =
            reinterpret_cast<const std::uint8_t*>(source);
        for (std::size_t index = 0; index < table.fields.size(); ++index)
        {
            const hl::network::GoldSrcDeltaField& field =
                table.fields[index];
            hl::network::GoldSrcDecodedDeltaValue& value =
                output->values[index];
            const std::uint32_t base =
                field.field_type
                & ~hl::network::kGoldSrcDeltaTypeSigned;
            const bool is_signed =
                (field.field_type
                    & hl::network::kGoldSrcDeltaTypeSigned)
                != 0u;
            if (field.field_offset >= source_size)
            {
                return fail(&field, "offset-out-of-range");
            }
            if (base == hl::network::kGoldSrcDeltaTypeByte)
            {
                value.kind =
                    hl::network::GoldSrcDeltaValueKind::kUnsignedInteger;
                value.unsigned_value = bytes[field.field_offset];
            }
            else if (base == hl::network::kGoldSrcDeltaTypeShort)
            {
                if (source_size < sizeof(std::int16_t)
                    || field.field_offset
                        > source_size - sizeof(std::int16_t))
                {
                    return fail(&field, "short-out-of-range");
                }
                std::int16_t scalar = 0;
                std::memcpy(
                    &scalar,
                    bytes + field.field_offset,
                    sizeof(scalar));
                if (is_signed)
                {
                    value.kind =
                        hl::network::GoldSrcDeltaValueKind::kSignedInteger;
                    value.signed_value = scalar;
                }
                else
                {
                    value.kind =
                        hl::network::GoldSrcDeltaValueKind::
                            kUnsignedInteger;
                    value.unsigned_value =
                        static_cast<std::uint16_t>(scalar);
                }
            }
            else if (base == hl::network::kGoldSrcDeltaTypeInteger)
            {
                if (source_size < sizeof(std::int32_t)
                    || field.field_offset
                        > source_size - sizeof(std::int32_t))
                {
                    return fail(&field, "integer-out-of-range");
                }
                std::int32_t scalar = 0;
                std::memcpy(
                    &scalar,
                    bytes + field.field_offset,
                    sizeof(scalar));
                if (is_signed)
                {
                    value.kind =
                        hl::network::GoldSrcDeltaValueKind::kSignedInteger;
                    value.signed_value = scalar;
                }
                else
                {
                    value.kind =
                        hl::network::GoldSrcDeltaValueKind::
                            kUnsignedInteger;
                    value.unsigned_value =
                        static_cast<std::uint32_t>(scalar);
                }
            }
            else if (base == hl::network::kGoldSrcDeltaTypeFloat
                || base == hl::network::kGoldSrcDeltaTypeAngle
                || base == hl::network::kGoldSrcDeltaTypeTimeWindow8
                || base == hl::network::kGoldSrcDeltaTypeTimeWindowBig)
            {
                if (source_size < sizeof(float)
                    || field.field_offset
                        > source_size - sizeof(float))
                {
                    return fail(&field, "float-out-of-range");
                }
                float scalar = 0.0f;
                std::memcpy(
                    &scalar,
                    bytes + field.field_offset,
                    sizeof(scalar));
                if (!std::isfinite(scalar))
                {
                    if (!normalize_nonfinite)
                    {
                        return fail(&field, "non-finite-float");
                    }
                    // Prediction/weapon extension fields can remain
                    // uninitialized until movement begins. This slice does
                    // not execute movement, so serialize their neutral stock
                    // value instead of putting a non-finite scalar on wire.
                    scalar = 0.0f;
                }
                value.kind =
                    hl::network::GoldSrcDeltaValueKind::kFloatingPoint;
                value.floating_value = scalar;
            }
            else if (base == hl::network::kGoldSrcDeltaTypeString)
            {
                if (is_signed)
                {
                    return fail(&field, "signed-string");
                }
                std::size_t field_extent =
                    source_size - field.field_offset;
                for (const hl::network::GoldSrcDeltaField& candidate :
                     table.fields)
                {
                    if (candidate.field_offset > field.field_offset)
                    {
                        field_extent = std::min(
                            field_extent,
                            static_cast<std::size_t>(
                                candidate.field_offset
                                - field.field_offset));
                    }
                }
                const std::size_t scan_size = std::min(
                    field_extent,
                    value.string_value.size());
                const void* const terminator = std::memchr(
                    bytes + field.field_offset,
                    '\0',
                    scan_size);
                if (terminator == nullptr)
                {
                    return fail(&field, "unterminated-string");
                }
                value.kind =
                    hl::network::GoldSrcDeltaValueKind::kString;
                value.string_size =
                    static_cast<const std::uint8_t*>(terminator)
                    - (bytes + field.field_offset);
                std::memcpy(
                    value.string_value.data(),
                    bytes + field.field_offset,
                    value.string_size);
                value.string_value[value.string_size] = '\0';
            }
            else
            {
                return fail(&field, "unsupported-field-type");
            }
        }
        return true;
    }

    static bool CaptureBaselineDeltaState(
        const entity_state_t& source,
        const hl::network::GoldSrcDeltaTable& table,
        hl::network::GoldSrcDecodedDeltaRecord* output,
        std::string* failure_detail) noexcept
    {
        return CaptureDeltaState(
            &source,
            sizeof(source),
            table,
            output,
            failure_detail);
    }

    static bool CapturedDeltaNumericMatches(
        const hl::network::GoldSrcDeltaTable& table,
        const hl::network::GoldSrcDecodedDeltaRecord& record,
        std::string_view field_name,
        double expected) noexcept
    {
        for (std::size_t index = 0u; index < table.fields.size(); ++index)
        {
            if (table.fields[index].name != field_name
                || index >= record.field_count)
            {
                continue;
            }
            const hl::network::GoldSrcDecodedDeltaValue& value =
                record.values[index];
            double observed = 0.0;
            switch (value.kind)
            {
            case hl::network::GoldSrcDeltaValueKind::kUnsignedInteger:
                observed = static_cast<double>(value.unsigned_value);
                break;
            case hl::network::GoldSrcDeltaValueKind::kSignedInteger:
                observed = static_cast<double>(value.signed_value);
                break;
            case hl::network::GoldSrcDeltaValueKind::kFloatingPoint:
                observed = value.floating_value;
                break;
            case hl::network::GoldSrcDeltaValueKind::kString:
            default:
                return false;
            }
            return std::isfinite(observed)
                && std::isfinite(expected)
                && std::abs(observed - expected) <= 0.0001;
        }
        return false;
    }

    bool BuildPlayerSnapshotInput(
        DedicatedPlayerRuntimeSlot& slot_state,
        hl::network::GoldSrcPlayerSnapshotInput* output,
        std::string* failure_detail)
    {
        if (output == nullptr
            || !slot_state.goldsrc_player_lifecycle.put_in_server()
            || !slot_state.goldsrc_delta_registry.has_value())
        {
            if (failure_detail != nullptr)
            {
                *failure_detail = "missing-player-context";
            }
            return false;
        }
        const auto& receiver_binding =
            slot_state.goldsrc_player_lifecycle.binding();
        if (lifecycle_edict_registry_.Validate(receiver_binding)
            != hl::network::GoldSrcClientEdictBindingResult::kBound)
        {
            if (failure_detail != nullptr)
            {
                *failure_detail = "stale-player-edict-binding";
            }
            return false;
        }
        edict_t* const receiver =
            state_.edict_store.EntityOfIndex(receiver_binding.edict_index);
        if (receiver == nullptr
            || state_.edict_store.PrivateDataOf(receiver) == nullptr)
        {
            if (failure_detail != nullptr)
            {
                *failure_detail = "missing-player-private-data";
            }
            return false;
        }
        const hl::network::GoldSrcDeltaTable* const player_table =
            FindBaselineDeltaTable(
                *slot_state.goldsrc_delta_registry,
                "entity_state_player_t");
        const hl::network::GoldSrcDeltaTable* const clientdata_table =
            FindBaselineDeltaTable(
                *slot_state.goldsrc_delta_registry,
                "clientdata_t");
        const hl::network::GoldSrcDeltaTable* const weapon_table =
            combat_enabled_
            ? FindBaselineDeltaTable(
                *slot_state.goldsrc_delta_registry,
                "weapon_data_t")
            : nullptr;
        if (player_table == nullptr || clientdata_table == nullptr
            || (combat_enabled_ && weapon_table == nullptr))
        {
            if (failure_detail != nullptr)
            {
                *failure_detail = "missing-player-delta-table";
            }
            return false;
        }
        const auto capture_entity =
            [this,
             receiver,
             &slot_state,
             player_table,
             failure_detail](
                DedicatedPlayerRuntimeSlot& candidate,
                hl::network::GoldSrcSnapshotEntityState* captured,
                entity_state_t* raw_state) -> AddToFullPackResult
        {
            if (captured == nullptr
                || !candidate.goldsrc_player_lifecycle.put_in_server())
            {
                if (failure_detail != nullptr)
                {
                    *failure_detail = "missing-visible-player-context";
                }
                return AddToFullPackResult::kFault;
            }
            const auto& binding =
                candidate.goldsrc_player_lifecycle.binding();
            if (lifecycle_edict_registry_.Validate(binding)
                != hl::network::GoldSrcClientEdictBindingResult::kBound)
            {
                if (failure_detail != nullptr)
                {
                    *failure_detail = "stale-visible-player-edict-binding";
                }
                return AddToFullPackResult::kFault;
            }
            edict_t* const entity =
                state_.edict_store.EntityOfIndex(binding.edict_index);
            if (entity == nullptr
                || state_.edict_store.PrivateDataOf(entity) == nullptr)
            {
                if (failure_detail != nullptr)
                {
                    *failure_detail = "missing-visible-player-private-data";
                }
                return AddToFullPackResult::kFault;
            }
            entity_state_t state{};
            const AddToFullPackResult pack_result = CallAddToFullPack(
                &state,
                binding.edict_index,
                entity,
                receiver);
            if (pack_result != AddToFullPackResult::kIncluded)
            {
                if (pack_result == AddToFullPackResult::kFault
                    && failure_detail != nullptr)
                {
                    *failure_detail = "add-to-full-pack-fault";
                }
                return pack_result;
            }
            for (int axis = 0; axis < 3; ++axis)
            {
                state.origin[axis] = entity->v.origin[axis];
                state.velocity[axis] = entity->v.velocity[axis];
                state.basevelocity[axis] = entity->v.basevelocity[axis];
                state.mins[axis] = entity->v.mins[axis];
                state.maxs[axis] = entity->v.maxs[axis];
            }
            state.usehull =
                (entity->v.flags & FL_DUCKING) != 0 ? 1 : 0;
            state.oldbuttons = entity->v.oldbuttons;
            state.onground = entity->v.groundentity != nullptr ? 0 : -1;
            const auto prior = std::find_if(
                slot_state.goldsrc_remote_player_observations.begin(),
                slot_state.goldsrc_remote_player_observations.end(),
                [&candidate](const GoldSrcRemotePlayerObservation& value)
                {
                    return value.slot == candidate.slot;
                });
            const bool first_for_receiver = candidate.slot == slot_state.slot
                ? slot_state.goldsrc_continuous_snapshots_sent == 0u
                : prior
                        == slot_state.goldsrc_remote_player_observations.end()
                    || !prior->visible
                    || prior->session_id != candidate.session_id;
            const bool reset_transition = candidate.slot != slot_state.slot
                && slot_state.slot >= 1 && candidate.slot >= 1
                && static_cast<std::size_t>(slot_state.slot)
                    <= stock_test_reset_transitions_.size()
                && static_cast<std::size_t>(candidate.slot)
                    <= stock_test_reset_transitions_[
                        static_cast<std::size_t>(slot_state.slot - 1)].size()
                && stock_test_reset_transitions_[
                    static_cast<std::size_t>(slot_state.slot - 1)][
                    static_cast<std::size_t>(candidate.slot - 1)];
            state.animtime = static_cast<float>(network_server_time_);
            if (reset_transition)
            {
                state.effects |= EF_NOINTERP;
            }
            else if (!first_for_receiver)
            {
                // ClientPutInServer marks EF_NOINTERP for the spawn transition.
                // The stock engine treats that bit as one-shot per receiver.
                state.effects &= ~EF_NOINTERP;
            }
            captured->entity_index = binding.edict_index;
            captured->kind = hl::network::GoldSrcBaselineKind::kPlayer;
            if (!CaptureDeltaState(
                    &state,
                    sizeof(state),
                    *player_table,
                    &captured->state,
                    failure_detail))
            {
                return AddToFullPackResult::kFault;
            }
            if (raw_state != nullptr)
            {
                *raw_state = state;
            }
            return AddToFullPackResult::kIncluded;
        };

        hl::network::GoldSrcPlayerSnapshotInput captured;
        entity_state_t entity_state{};
        const AddToFullPackResult local_capture = capture_entity(
            slot_state,
            &captured.entity,
            &entity_state);
        if (local_capture == AddToFullPackResult::kFault)
        {
            return false;
        }
        captured.include_local_entity =
            local_capture == AddToFullPackResult::kIncluded;
        for (DedicatedPlayerRuntimeSlot& candidate :
             state_.dedicated_multiplayer_foundation.slots)
        {
            if (candidate.slot == slot_state.slot
                || !IsDedicatedSlotConnectedOccupant(candidate)
                || !candidate.goldsrc_player_lifecycle.put_in_server())
            {
                continue;
            }
            hl::network::GoldSrcSnapshotEntityState remote;
            const AddToFullPackResult remote_capture = capture_entity(
                candidate,
                &remote,
                nullptr);
            if (remote_capture == AddToFullPackResult::kFault)
            {
                return false;
            }
            if (remote_capture == AddToFullPackResult::kIncluded)
            {
                captured.remote_entities.push_back(std::move(remote));
            }
        }
        clientdata_t clientdata{};
        if (!CallUpdateClientData(
                receiver,
                &clientdata,
                combat_enabled_))
        {
            if (failure_detail != nullptr)
            {
                *failure_detail = "update-client-data-failed";
            }
            return false;
        }
        // The stock engine supplements the Game DLL callbacks with the
        // authoritative movement fields it owns.  Keep the entity and local
        // client channels on one committed PM_Move state before delta capture.
        for (int axis = 0; axis < 3; ++axis)
        {
            clientdata.origin[axis] = receiver->v.origin[axis];
            clientdata.velocity[axis] = receiver->v.velocity[axis];
            clientdata.view_ofs[axis] = receiver->v.view_ofs[axis];
        }
        clientdata.flags = receiver->v.flags;
        clientdata.waterlevel = receiver->v.waterlevel;
        clientdata.watertype = receiver->v.watertype;
        clientdata.bInDuck = receiver->v.bInDuck;
        clientdata.flDuckTime = static_cast<int>(receiver->v.flDuckTime);
        clientdata.flTimeStepSound =
            static_cast<int>(receiver->v.flTimeStepSound);
        clientdata.flSwimTime = static_cast<int>(receiver->v.flSwimTime);
        clientdata.waterjumptime =
            static_cast<int>(receiver->v.teleport_time);
        clientdata.maxspeed = receiver->v.maxspeed;

        bool prediction_state_consistent =
            clientdata.bInDuck == receiver->v.bInDuck
            && clientdata.waterlevel == receiver->v.waterlevel
            && clientdata.watertype == receiver->v.watertype;
        for (int axis = 0; axis < 3; ++axis)
        {
            prediction_state_consistent = prediction_state_consistent
                && std::isfinite(receiver->v.origin[axis])
                && std::isfinite(receiver->v.velocity[axis])
                && std::isfinite(clientdata.origin[axis])
                && std::isfinite(clientdata.velocity[axis])
                && std::fabs(
                    receiver->v.origin[axis]
                        - clientdata.origin[axis]) <= 0.0001f
                && std::fabs(
                    receiver->v.velocity[axis]
                        - clientdata.velocity[axis]) <= 0.0001f;
            if (captured.include_local_entity)
            {
                prediction_state_consistent = prediction_state_consistent
                    && std::isfinite(entity_state.origin[axis])
                    && std::isfinite(entity_state.velocity[axis])
                    && std::fabs(
                        entity_state.origin[axis]
                            - clientdata.origin[axis]) <= 0.0001f
                    && std::fabs(
                        entity_state.velocity[axis]
                            - clientdata.velocity[axis]) <= 0.0001f;
            }
        }
        if (captured.include_local_entity)
        {
            prediction_state_consistent = prediction_state_consistent
                && entity_state.usehull
                    == ((clientdata.flags & FL_DUCKING) != 0 ? 1 : 0);
        }
        if (!prediction_state_consistent)
        {
            ++summary_.movement_discontinuity_count;
            ++prediction_state_diagnostics_seen_;
            if (ShouldLogDiagnosticSample(
                    prediction_state_diagnostics_seen_))
            {
                hl::common::Logger::Warn(
                    hl::common::LogCategory::Server,
                    "goldsrc_prediction_state_discontinuity: reason=entity-clientdata-mismatch");
            }
            if (failure_detail != nullptr)
            {
                *failure_detail = "prediction-state-discontinuity";
            }
            return false;
        }
        summary_.clientdata_playerstate_consistent = true;
        if (!combat_enabled_)
        {
            // Preserve the established movement-only snapshot contract.
            clientdata.m_flNextAttack = 0.0f;
            clientdata.ammo_nails = 0;
            clientdata.ammo_shells = 0;
            clientdata.ammo_cells = 0;
            clientdata.ammo_rockets = 0;
            clientdata.m_iId = 0;
            clientdata.fuser1 = 0.0f;
            clientdata.fuser2 = 0.0f;
            clientdata.fuser3 = 0.0f;
            clientdata.fuser4 = 0.0f;
            clientdata.vuser1 = Vector(0.0f, 0.0f, 0.0f);
            clientdata.vuser2 = Vector(0.0f, 0.0f, 0.0f);
            clientdata.vuser3 = Vector(0.0f, 0.0f, 0.0f);
            clientdata.vuser4 = Vector(0.0f, 0.0f, 0.0f);
        }
        if (!CaptureDeltaState(
                &clientdata,
                sizeof(clientdata),
                *clientdata_table,
                &captured.clientdata.state,
                failure_detail,
                true))
        {
            return false;
        }
        if (combat_enabled_
            && !CapturedDeltaNumericMatches(
                *clientdata_table,
                captured.clientdata.state,
                "health",
                clientdata.health))
        {
            if (failure_detail != nullptr)
            {
                *failure_detail = "clientdata-health-capture-mismatch";
            }
            return false;
        }
        if (combat_enabled_)
        {
            std::array<weapon_data_t, 32> weapon_data{};
            if (!CallGetWeaponData(receiver, weapon_data.data()))
            {
                if (failure_detail != nullptr)
                {
                    *failure_detail = "get-weapon-data-failed";
                }
                pmove_runtime_.SetCombatPlayerReady(
                    static_cast<std::size_t>(slot_state.slot),
                    true,
                    clientdata.m_iId,
                    false);
                return false;
            }
            bool active_weapon_present = false;
            bool glock_present = false;
            int glock_clip = -1;
            for (std::size_t index = 0u; index < weapon_data.size(); ++index)
            {
                const weapon_data_t& raw = weapon_data[index];
                if (raw.m_iId == 0)
                {
                    continue;
                }
                if (raw.m_iId < 0
                    || raw.m_iId >= static_cast<int>(weapon_data.size())
                    || raw.m_iId != static_cast<int>(index))
                {
                    if (failure_detail != nullptr)
                    {
                        *failure_detail = "invalid-weapon-index";
                    }
                    return false;
                }
                hl::network::GoldSrcWeaponState weapon;
                weapon.weapon_index = static_cast<std::uint8_t>(raw.m_iId);
                if (!CaptureDeltaState(
                        &raw,
                        sizeof(raw),
                        *weapon_table,
                        &weapon.state,
                        failure_detail,
                        true))
                {
                    return false;
                }
                if (raw.m_iId
                        == hl::network::kGoldSrcStockGlockWeaponId
                    && !CapturedDeltaNumericMatches(
                        *weapon_table,
                        weapon.state,
                        "m_iClip",
                        raw.m_iClip))
                {
                    if (failure_detail != nullptr)
                    {
                        *failure_detail = "glock-clip-capture-mismatch";
                    }
                    return false;
                }
                active_weapon_present = active_weapon_present
                    || raw.m_iId == clientdata.m_iId;
                if (raw.m_iId == hl::network::kGoldSrcStockGlockWeaponId)
                {
                    glock_present = true;
                    glock_clip = raw.m_iClip;
                }
                captured.weapons.push_back(std::move(weapon));
            }
            pmove_runtime_.SetCombatPlayerReady(
                static_cast<std::size_t>(slot_state.slot),
                true,
                clientdata.m_iId,
                glock_present);
            if (hl::game_api::GoldSrcClientReplicationSummary* const client =
                    FindClientReplicationSummary(slot_state.slot);
                client != nullptr)
            {
                const bool first_combat_observation =
                    !client->combat_observation_started;
                const int previous_glock_clip = client->glock_clip;
                const float previous_health = client->authoritative_health;
                client->active_weapon_id = clientdata.m_iId;
                client->active_weapon_present = active_weapon_present;
                client->glock_entity_present = glock_present;
                client->glock_clip = glock_clip;
                client->glock_reserve_ammo = static_cast<int>(
                    clientdata.vuser1.x);
                client->authoritative_health = receiver->v.health;
                client->clientdata_health = clientdata.health;
                client->authoritative_deadflag = receiver->v.deadflag;
                if (!client->combat_observation_started)
                {
                    client->combat_observation_started = true;
                    client->initial_authoritative_health = receiver->v.health;
                    client->minimum_authoritative_health = receiver->v.health;
                    client->initial_clientdata_health = clientdata.health;
                    client->minimum_clientdata_health = clientdata.health;
                    client->initial_glock_reserve_ammo =
                        client->glock_reserve_ammo;
                }
                else
                {
                    if (receiver->v.health
                        < client->minimum_authoritative_health)
                    {
                        client->minimum_authoritative_health =
                            receiver->v.health;
                    }
                    if (clientdata.health < client->minimum_clientdata_health)
                    {
                        client->minimum_clientdata_health = clientdata.health;
                    }
                }
                if (glock_clip >= 0 && client->initial_glock_clip < 0)
                {
                    client->initial_glock_clip = glock_clip;
                    client->minimum_glock_clip = glock_clip;
                }
                else if (glock_clip >= 0
                    && (client->minimum_glock_clip < 0
                        || glock_clip < client->minimum_glock_clip))
                {
                    client->minimum_glock_clip = glock_clip;
                }
                ++client->weapondata_frames;
                if (first_combat_observation
                    || previous_glock_clip != client->glock_clip
                    || std::fabs(
                        previous_health - client->authoritative_health)
                        > 0.0001f)
                {
                    hl::common::Logger::Info(
                        hl::common::LogCategory::Server,
                        "goldsrc_combat_observation: slot="
                            + std::to_string(slot_state.slot)
                            + ",session_generation="
                            + std::to_string(
                                slot_state.goldsrc_player_lifecycle.binding()
                                    .network_session_generation)
                            + ",active_weapon="
                            + std::to_string(client->active_weapon_id)
                            + ",glock_present="
                            + (client->glock_entity_present
                                ? "true" : "false")
                            + ",clip="
                            + std::to_string(client->glock_clip)
                            + ",reserve="
                            + std::to_string(client->glock_reserve_ammo)
                            + ",health="
                            + std::to_string(
                                client->authoritative_health)
                            + ",clientdata_health="
                            + std::to_string(client->clientdata_health)
                            + ",deadflag="
                            + std::to_string(
                                client->authoritative_deadflag));
                }
            }
        }
        *output = std::move(captured);
        RefreshCombatEvidenceSummary();
        return true;
    }

    bool CallCreateBaseline(
        int player,
        int entity_index,
        entity_state_t* baseline,
        edict_t* entity,
        int player_model_index,
        float* player_mins,
        float* player_maxs) noexcept
    {
        if (state_.dll_functions.pfnCreateBaseline == nullptr)
        {
            return false;
        }
        __try
        {
            state_.dll_functions.pfnCreateBaseline(
                player,
                entity_index,
                baseline,
                entity,
                player_model_index,
                player_mins,
                player_maxs);
            return true;
        }
        __except (HandleSehException(
            "pfnCreateBaseline",
            GetExceptionCode()))
        {
            return false;
        }
    }

    bool CallCreateInstancedBaselines() noexcept
    {
        if (state_.dll_functions.pfnCreateInstancedBaselines == nullptr)
        {
            return false;
        }
        __try
        {
            state_.dll_functions.pfnCreateInstancedBaselines();
            return true;
        }
        __except (HandleSehException(
            "pfnCreateInstancedBaselines",
            GetExceptionCode()))
        {
            return false;
        }
    }

    void RefreshCombatEvidenceSummary()
    {
        if (!combat_enabled_)
        {
            return;
        }

        const auto& runtime = pmove_runtime_.diagnostics();
        bool callback_sequence_observed = false;
        bool gameplay_time_observed = false;
        bool glock_round_observed = false;
        std::size_t combat_participants = 0u;
        std::size_t weapondata_clients = 0u;
        std::size_t weapondata_history_clients = 0u;
        std::size_t weapondata_ack_clients = 0u;
        for (std::size_t index = 0u;
             index < summary_.client_replication.size();
             ++index)
        {
            hl::game_api::GoldSrcClientReplicationSummary& client =
                summary_.client_replication[index];
            const auto* combat = pmove_runtime_.CombatDiagnostics(index + 1u);
            if (combat != nullptr && combat->attack_commands_executed > 0u)
            {
                callback_sequence_observed = callback_sequence_observed
                    || (combat->player_prethink_calls
                            >= combat->attack_commands_executed
                        && combat->player_postthink_calls
                            >= combat->attack_commands_executed
                        && combat->callback_failures == 0u);
                gameplay_time_observed = gameplay_time_observed
                    || combat->gameplay_time_msec > 0u;
            }
            const bool participant = client.admission_count > 0
                && client.put_in_server;
            const bool weapondata_observed = client.weapondata_frames > 0u
                && client.glock_entity_present
                && client.glock_clip >= 0;
            if (participant)
            {
                ++combat_participants;
                weapondata_clients += weapondata_observed ? 1u : 0u;
                const bool history_observed = weapondata_observed
                    && client.frame_history_independent
                    && client.snapshots_sent > 0u;
                weapondata_history_clients += history_observed ? 1u : 0u;
                weapondata_ack_clients += history_observed
                        && client.frames_acknowledged > 0u
                    ? 1u
                    : 0u;
            }
            const bool shot_observed = client.initial_glock_clip >= 0
                && client.minimum_glock_clip >= 0
                && client.minimum_glock_clip < client.initial_glock_clip;
            glock_round_observed = glock_round_observed || shot_observed;
            const bool damage_observed = client.combat_observation_started
                && (client.minimum_authoritative_health + 0.0001f
                        < client.initial_authoritative_health
                    || client.minimum_clientdata_health + 0.0001f
                        < client.initial_clientdata_health);
            if (damage_observed)
            {
                client.combat_phase = std::string(hl::network::NameFor(
                    hl::network::GoldSrcCombatPhase::kDamageObserved));
            }
        }

        const bool paired_command_callbacks = runtime.cmd_start_calls > 0u
            && runtime.cmd_start_calls == runtime.cmd_end_calls;
        summary_.combat_callback_order_verified =
            summary_.combat_callback_order_verified
            || (combat_start_frame_calls_ > 0u
                && paired_command_callbacks
                && callback_sequence_observed);
        summary_.combat_time_contract_verified =
            summary_.combat_time_contract_verified
            || (callback_sequence_observed && gameplay_time_observed);
        summary_.weapon_postframe_contract_verified =
            summary_.weapon_postframe_contract_verified
            || (summary_.attack_commands_executed > 0u
                && glock_round_observed);
        summary_.weapon_replication_contract_verified =
            summary_.weapon_replication_contract_verified
            || (combat_participants > 0u
                && weapondata_ack_clients == combat_participants);
        summary_.player_aware_trace = summary_.player_aware_trace
            || state_.goldsrc_combat_player_hits > 0u;
        summary_.per_client_weapondata = summary_.per_client_weapondata
            || (combat_participants > 0u
                && weapondata_clients == combat_participants);
        summary_.frame_history_weapondata = summary_.frame_history_weapondata
            || (combat_participants > 0u
                && weapondata_history_clients == combat_participants);
        summary_.gameplay_active = summary_.gameplay_active
            || (summary_.attack_commands_executed > 0u
                && callback_sequence_observed);
    }

    void SyncPmoveSummary(std::size_t client_slot)
    {
        const auto& runtime = pmove_runtime_.diagnostics();
        summary_.pm_init_call_count =
            static_cast<int>(runtime.pm_init_calls);
        summary_.pmove_calls = static_cast<int>(runtime.pm_move_calls);
        summary_.pmove_player_trace_calls = static_cast<int>(
            runtime.player_trace_calls);
        summary_.movement_rollbacks =
            static_cast<int>(runtime.movement_rollbacks);
        summary_.movement_snapshots_sent =
            static_cast<int>(runtime.movement_snapshots_sent);
        summary_.movement_ready =
            pmove_runtime_.movement_ready(client_slot);
        summary_.movement_executed =
            pmove_runtime_.movement_executed(client_slot);
        if (const auto* combat = pmove_runtime_.CombatDiagnostics(client_slot);
            combat != nullptr)
        {
            if (hl::game_api::GoldSrcClientReplicationSummary* const client =
                    FindClientReplicationSummary(
                        static_cast<int>(client_slot));
                client != nullptr)
            {
                client->combat_phase = std::string(
                    hl::network::NameFor(combat->phase));
                client->attack_commands_received = (std::max)(
                    client->attack_commands_received,
                    combat->attack_commands_received);
                client->attack_commands_executed = (std::max)(
                    client->attack_commands_executed,
                    combat->attack_commands_executed);
                client->duplicate_attack_commands_suppressed = (std::max)(
                    client->duplicate_attack_commands_suppressed,
                    combat->duplicate_attack_commands_suppressed);
                client->unsupported_gameplay_inputs_masked = (std::max)(
                    client->unsupported_gameplay_inputs_masked,
                    combat->unsupported_gameplay_inputs_masked);
                client->player_prethink_calls = (std::max)(
                    client->player_prethink_calls,
                    combat->player_prethink_calls);
                client->player_postthink_calls = (std::max)(
                    client->player_postthink_calls,
                    combat->player_postthink_calls);
                if (client_slot >= 1u
                    && client_slot
                        <= state_.goldsrc_combat_trace_hits_by_slot.size())
                {
                    const std::size_t index = client_slot - 1u;
                    client->player_trace_hits =
                        state_.goldsrc_combat_trace_hits_by_slot[index];
                    client->world_trace_occlusions =
                        state_.goldsrc_combat_world_occlusions_by_slot[index];
                    client->weapon_range_trace_calls =
                        state_.goldsrc_combat_weapon_range_traces_by_slot[
                            index];
                    client->weapon_range_shooter_ignored =
                        state_.goldsrc_combat_shooter_ignored_by_slot[index];
                    client->weapon_range_player_hits =
                        state_.goldsrc_combat_weapon_range_player_hits_by_slot[
                            index];
                    client->weapon_range_world_hits =
                        state_.goldsrc_combat_weapon_range_world_hits_by_slot[
                            index];
                    client->playback_event_calls =
                        state_.goldsrc_combat_playback_events_by_slot[index];
                    client->last_trace_target_slot =
                        state_.goldsrc_combat_last_trace_target_by_slot[index];
                }
            }
        }
        summary_.start_frame_call_count = combat_start_frame_calls_;
        summary_.combat_trace_calls = state_.goldsrc_combat_trace_calls;
        summary_.combat_player_hits = state_.goldsrc_combat_player_hits;
        summary_.combat_world_occlusions =
            state_.goldsrc_combat_world_occlusions;
        const auto playback = state_.callback_counts.find("pfnPlaybackEvent");
        summary_.playback_event_calls = playback == state_.callback_counts.end()
            ? 0u
            : playback->second;
        summary_.attack_commands_received = 0u;
        summary_.attack_commands_executed = 0u;
        summary_.duplicate_attack_commands_suppressed = 0u;
        summary_.unsupported_gameplay_inputs_masked = 0u;
        for (const hl::game_api::GoldSrcClientReplicationSummary& client :
             summary_.client_replication)
        {
            summary_.attack_commands_received +=
                client.attack_commands_received;
            summary_.attack_commands_executed +=
                client.attack_commands_executed;
            summary_.duplicate_attack_commands_suppressed +=
                client.duplicate_attack_commands_suppressed;
            summary_.unsupported_gameplay_inputs_masked +=
                client.unsupported_gameplay_inputs_masked;
        }
        if (const auto* state = pmove_runtime_.CommandState(client_slot);
            state != nullptr)
        {
            const auto& commands = state->diagnostics();
            summary_.movement_commands_received =
                static_cast<int>(commands.commands_received);
            summary_.movement_commands_executed =
                static_cast<int>(commands.commands_executed);
            summary_.backup_commands_replayed =
                static_cast<int>(commands.backup_commands_replayed);
            summary_.duplicate_commands_suppressed =
                static_cast<int>(commands.duplicate_commands_suppressed);
            summary_.move_packets_observed =
                static_cast<int>(commands.move_packets_observed);
            summary_.move_packets_validated =
                static_cast<int>(commands.move_packets_validated);
            summary_.move_packets_executed =
                static_cast<int>(commands.move_packets_executed);
            summary_.move_packets_temporarily_rejected =
                static_cast<int>(
                    commands.move_packets_temporarily_rejected);
            summary_.command_clock_recoveries =
                static_cast<int>(commands.command_clock_recoveries);
            summary_.command_clock_resynchronizations =
                static_cast<int>(
                    commands.command_clock_resynchronizations);
            summary_.raw_netchan_sequence_gaps =
                static_cast<int>(commands.raw_netchan_sequence_gaps);
            summary_.raw_netchan_gap_move_replays =
                static_cast<int>(commands.raw_netchan_gap_move_replays);
            summary_.synthetic_command_replays =
                static_cast<int>(commands.synthetic_command_replays);
            summary_.maximum_move_execution_gap_ms =
                static_cast<int>(commands.maximum_move_execution_gap_msec);
        }
        RefreshCombatEvidenceSummary();
    }

    bool HandleClientDisconnect(DedicatedPlayerRuntimeSlot& slot_state)
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
        if (player_lifecycle_enabled_
            && !DisconnectPlayerLifecycle(slot_state, "client-command"))
        {
            return FailTransport(
                "player-lifecycle-disconnect-callback-failed",
                "goldsrc_player_lifecycle_failed: reason=disconnect-callback");
        }
        if (pmove_enabled_)
        {
            pmove_runtime_.ResetClient(
                static_cast<std::size_t>(disconnected_slot));
            slot_state.goldsrc_last_movement_commit_packet_sequence.reset();
            slot_state.goldsrc_last_movement_commit_host_frame = 0;
            SyncPmoveSummary(static_cast<std::size_t>(disconnected_slot));
        }
        ApplyDedicatedClientDisconnect(state_, slot_state);
        RecordDedicatedLifecycleTransition(
            state_.dedicated_multiplayer_foundation,
            slot_state,
            DedicatedPlayerLifecycleState::kDisconnected,
            "bounded client disconnect during GoldSrc streaming");
        const bool state_cleared =
            !slot_state.netchan.initialized()
            && slot_state.goldsrc_signon.phase()
                == hl::network::GoldSrcSignonPhase::kNone
            && slot_state.goldsrc_first_snapshot.phase()
                == hl::network::GoldSrcFirstSnapshotPhase::kNone
            && slot_state.goldsrc_first_snapshot.history().size() == 0u
            && !slot_state.goldsrc_first_snapshot.prepared().has_value()
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
        const std::size_t reset_index = static_cast<std::size_t>(
            disconnected_slot - 1);
        if (reset_index < awaiting_fresh_session_after_delta_reset_.size())
        {
            awaiting_fresh_session_after_delta_reset_[reset_index] = true;
        }
        slot_state.goldsrc_negative_advance_pending = false;
        slot_state.goldsrc_continuous_snapshot_started_server_time.reset();
        slot_state.goldsrc_last_counted_acknowledged_frame.reset();
        slot_state.goldsrc_last_progress_log_server_time = 0.0;
        slot_state.goldsrc_last_successful_move_time_ms = 0u;
        slot_state.goldsrc_last_successful_snapshot_time_ms = 0u;
        slot_state.goldsrc_last_successful_frame_ack_time_ms = 0u;
        slot_state.goldsrc_pending_empty_ack = false;
        slot_state.goldsrc_pending_empty_ack_since_server_time = 0.0;
        slot_state.goldsrc_pending_empty_ack_packets = 0u;
        slot_state.goldsrc_consecutive_snapshot_starvation_frames = 0u;
        if (hl::game_api::GoldSrcClientReplicationSummary* client =
                FindClientReplicationSummary(disconnected_slot);
            client != nullptr)
        {
            ++client->disconnect_count;
        }
        summary_.final_state = "disconnected";
        summary_.connected = false;
        summary_.put_in_server = false;
        summary_.spawned = false;
        summary_.active = false;
        if (pmove_persistent_ && disconnected_slot == 1
            && manual_session_lifecycle_.MarkClientDisconnected())
        {
            SyncManualSessionSummary();
            hl::common::Logger::Info(
                hl::common::LogCategory::Server,
                "goldsrc_client_disconnected: mode=persistent"
                ",reason=client_command,reconnect_ready=1");
        }
        bool surviving_client = false;
        for (const DedicatedPlayerRuntimeSlot& candidate :
             state_.dedicated_multiplayer_foundation.slots)
        {
            if (candidate.slot == disconnected_slot
                || !IsDedicatedSlotConnectedOccupant(candidate))
            {
                continue;
            }
            surviving_client = true;
            if (hl::game_api::GoldSrcClientReplicationSummary* survivor =
                    FindClientReplicationSummary(candidate.slot);
                survivor != nullptr)
            {
                survivor->survived_other_disconnect = true;
            }
        }
        summary_.disconnect_isolation =
            summary_.disconnect_isolation || surviving_client;
        SyncNetchanSummary(slot_state);
        SyncClientReplicationSummary(slot_state);
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

    bool ExecutePmove(
        DedicatedPlayerRuntimeSlot& slot_state,
        const hl::network::GoldSrcDecodedMoveCommand& move,
        std::uint32_t packet_sequence)
    {
        if (!pmove_enabled_
            || !slot_state.goldsrc_player_lifecycle.put_in_server()
            || !slot_state.goldsrc_player_lifecycle.signon_complete())
        {
            return true;
        }
        const auto& binding =
            slot_state.goldsrc_player_lifecycle.binding();
        if (lifecycle_edict_registry_.Validate(binding)
            != hl::network::GoldSrcClientEdictBindingResult::kBound)
        {
            return FailTransport(
                "pmove-client-binding-invalid",
                "goldsrc_pmove_failed: reason=stale-client-binding");
        }
        edict_t* const player =
            state_.edict_store.EntityOfIndex(binding.edict_index);
        edict_t* const world = state_.edict_store.World();
        if (player == nullptr || world == nullptr
            || state_.edict_store.PrivateDataOf(player) == nullptr)
        {
            return FailTransport(
                "pmove-player-unavailable",
                "goldsrc_pmove_failed: reason=player-unavailable");
        }

        const std::uint64_t host_time_msec = static_cast<std::uint64_t>(
            (std::max)(0.0, network_server_time_) * 1000.0);
        bool forward_requested = false;
        bool backward_requested = false;
        bool strafe_requested = false;
        bool jump_requested = false;
        bool duck_requested = false;
        for (std::size_t index = 0; index < move.command_count; ++index)
        {
            const auto& command = move.commands[index];
            forward_requested =
                forward_requested || command.forwardmove > 0.0f;
            backward_requested =
                backward_requested || command.forwardmove < 0.0f;
            strafe_requested =
                strafe_requested || command.sidemove != 0.0f;
            jump_requested =
                jump_requested || (command.buttons & IN_JUMP) != 0;
            duck_requested =
                duck_requested || (command.buttons & IN_DUCK) != 0;
        }
        const hl::game_api::detail::GoldSrcPmoveExecutionResult executed =
            pmove_runtime_.Execute(
                static_cast<std::size_t>(slot_state.slot),
                move,
                packet_sequence,
                host_time_msec,
                player,
                world);
        SyncPmoveSummary(static_cast<std::size_t>(slot_state.slot));
        if (summary_.maximum_move_execution_gap_ms
                > slot_state.goldsrc_last_observed_move_execution_gap_ms)
        {
            slot_state.goldsrc_last_observed_move_execution_gap_ms =
                summary_.maximum_move_execution_gap_ms;
            if (summary_.maximum_move_execution_gap_ms
                    > static_cast<int>(summary_.snapshot_interval_ms * 2.0))
            {
                ++pmove_execution_gap_diagnostics_seen_;
                if (ShouldLogDiagnosticSample(
                        pmove_execution_gap_diagnostics_seen_))
                {
                    hl::common::Logger::Warn(
                        hl::common::LogCategory::Server,
                        "goldsrc_pmove_execution_gap: packet_sequence="
                            + std::to_string(packet_sequence)
                            + ",maximum_gap_msec="
                            + std::to_string(
                                summary_.maximum_move_execution_gap_ms));
                }
            }
        }
        if (summary_.command_clock_resynchronizations
                > slot_state
                    .goldsrc_last_reported_clock_resynchronizations)
        {
            slot_state.goldsrc_last_reported_clock_resynchronizations =
                summary_.command_clock_resynchronizations;
            ++pmove_clock_resync_diagnostics_seen_;
            if (ShouldLogDiagnosticSample(
                    pmove_clock_resync_diagnostics_seen_))
            {
                hl::common::Logger::Info(
                    hl::common::LogCategory::Server,
                    "goldsrc_pmove_clock_resynchronized: packet_sequence="
                        + std::to_string(packet_sequence)
                        + ",resynchronizations="
                        + std::to_string(
                            summary_.command_clock_resynchronizations));
            }
        }
        if (!executed.ok() && executed.gameplay_callback_failure)
        {
            const std::string_view reported_stage =
                executed.gameplay_failure_stage == "fail_stop_latched"
                    ? "fail_stop_latched"
                : executed.gameplay_failure_stage == "cmd_start"
                    ? "cmd_start"
                : executed.gameplay_failure_stage == "player_prethink"
                    ? "player_prethink"
                : executed.gameplay_failure_stage == "entity_think"
                    ? "entity_think"
                : executed.gameplay_failure_stage == "context_build"
                    ? "context_build"
                : executed.gameplay_failure_stage == "pm_move"
                    ? "pm_move"
                : executed.gameplay_failure_stage == "output_validation"
                    ? "output_validation"
                : executed.gameplay_failure_stage == "player_postthink"
                    ? "player_postthink"
                : executed.gameplay_failure_stage == "cmd_end"
                    ? "cmd_end"
                    : "unknown";
            ++summary_.clc_move_rejected;
            return FailTransport(
                "combat-gameplay-callback-failed-"
                    + std::string(reported_stage),
                "goldsrc_combat_failed: reason=gameplay-callback,stage="
                    + std::string(reported_stage));
        }
        if (!executed.ok())
        {
            ++summary_.clc_move_rejected;
            if (executed.command_status
                == hl::network::GoldSrcCommandPlanStatus::kTemporarilyAhead)
            {
                ++pmove_clock_rejection_diagnostics_seen_;
                if (ShouldLogDiagnosticSample(
                        pmove_clock_rejection_diagnostics_seen_))
                {
                    hl::common::Logger::Warn(
                        hl::common::LogCategory::Server,
                        "goldsrc_pmove_clock_rejected: packet_sequence="
                            + std::to_string(packet_sequence)
                            + ",last_observed_move_sequence="
                            + std::to_string(
                                executed.last_observed_move_sequence)
                            + ",last_validated_move_sequence="
                            + std::to_string(
                                executed.last_validated_move_sequence)
                            + ",last_executed_move_sequence="
                            + std::to_string(
                                executed.last_executed_move_sequence)
                            + ",raw_netchan_sequence_distance="
                            + std::to_string(
                                executed.raw_netchan_sequence_distance)
                            + ",backup_commands="
                            + std::to_string(executed.backup_commands)
                            + ",fresh_commands="
                            + std::to_string(executed.fresh_commands)
                            + ",recovered_commands="
                            + std::to_string(executed.recovered_commands)
                            + ",synthetic_replays="
                            + std::to_string(executed.synthetic_replays)
                            + ",packet_command_msec="
                            + std::to_string(executed.packet_command_msec)
                            + ",host_elapsed_msec="
                            + std::to_string(executed.host_elapsed_msec)
                            + ",command_elapsed_msec="
                            + std::to_string(
                                executed.command_elapsed_msec)
                            + ",proposed_command_lead_msec="
                            + std::to_string(
                                executed.proposed_command_elapsed_msec
                                    > executed.host_elapsed_msec
                                ? executed.proposed_command_elapsed_msec
                                    - executed.host_elapsed_msec
                                : 0u));
                }
            }
            else
            {
                ++summary_.move_packets_permanently_rejected;
                ++pmove_rejection_diagnostics_seen_;
                if (ShouldLogDiagnosticSample(
                        pmove_rejection_diagnostics_seen_))
                {
                    hl::common::Logger::Warn(
                        hl::common::LogCategory::Server,
                        "goldsrc_pmove_rejected: reason="
                            + std::string(
                                hl::game_api::detail::ReasonFor(
                                    executed.status))
                            + ",command_reason="
                            + std::string(
                                hl::network::ReasonFor(
                                    executed.command_status))
                            + ",output_reason="
                            + std::string(
                                executed.output_validation_reason));
                }
            }
            return true;
        }
        if (executed.clock_recovered)
        {
            ++pmove_clock_recovery_diagnostics_seen_;
            if (ShouldLogDiagnosticSample(
                    pmove_clock_recovery_diagnostics_seen_))
            {
                hl::common::Logger::Info(
                    hl::common::LogCategory::Server,
                    "goldsrc_pmove_clock_recovered: packet_sequence="
                        + std::to_string(packet_sequence)
                        + ",last_executed_move_sequence="
                        + std::to_string(
                            executed.last_executed_move_sequence)
                        + ",host_elapsed_msec="
                        + std::to_string(executed.host_elapsed_msec)
                        + ",command_elapsed_msec="
                        + std::to_string(
                            executed.proposed_command_elapsed_msec));
            }
        }
        if (executed.commands_executed > 0u)
        {
            slot_state.goldsrc_last_movement_commit_packet_sequence =
                packet_sequence;
            slot_state.goldsrc_last_movement_commit_host_frame =
                summary_.host_frames_pumped;
            slot_state.goldsrc_pmove_calls += executed.commands_executed;
            slot_state.goldsrc_last_successful_move_time_ms =
                static_cast<std::uint64_t>(std::llround(
                    (std::max)(0.0, network_server_time_) * 1000.0));
            if (hl::game_api::GoldSrcClientReplicationSummary* client =
                    FindClientReplicationSummary(slot_state.slot);
                client != nullptr)
            {
                client->pmove_calls += executed.commands_executed;
                client->forward_input_packets += forward_requested ? 1u : 0u;
                client->backward_input_packets += backward_requested ? 1u : 0u;
                client->strafe_input_packets += strafe_requested ? 1u : 0u;
                client->jump_input_packets += jump_requested ? 1u : 0u;
                client->duck_input_packets += duck_requested ? 1u : 0u;
            }
            for (const DedicatedPlayerRuntimeSlot& other :
                 state_.dedicated_multiplayer_foundation.slots)
            {
                if (other.slot != slot_state.slot
                    && other.goldsrc_last_movement_commit_host_frame
                        == summary_.host_frames_pumped)
                {
                    summary_.simultaneous_movement = true;
                    break;
                }
            }
            if (pmove_persistent_ && slot_state.slot == 1)
            {
                manual_session_lifecycle_.RecordClcMove(
                    MonotonicMilliseconds(last_network_host_frame_time_));
            }
            summary_.stock_forward_input =
                summary_.stock_forward_input || forward_requested;
            summary_.stock_backward_input =
                summary_.stock_backward_input || backward_requested;
            summary_.stock_strafe_input =
                summary_.stock_strafe_input || strafe_requested;
            summary_.stock_jump_input =
                summary_.stock_jump_input || jump_requested;
            summary_.stock_duck_input =
                summary_.stock_duck_input || duck_requested;
            if (!slot_state.goldsrc_first_movement_server_time.has_value())
            {
                slot_state.goldsrc_first_movement_server_time =
                    network_server_time_;
            }
            summary_.movement_ready = true;
            summary_.movement_executed = true;
            summary_.stock_authoritative_movement =
                summary_.stock_authoritative_movement
                || executed.authoritative_state_changed;
            summary_.stock_forward_movement =
                summary_.stock_forward_movement
                || (forward_requested
                    && executed.authoritative_state_changed);
            summary_.stock_backward_movement =
                summary_.stock_backward_movement
                || (backward_requested
                    && executed.authoritative_state_changed);
            summary_.stock_strafe_movement =
                summary_.stock_strafe_movement
                || (strafe_requested
                    && executed.authoritative_state_changed);
            summary_.stock_jump =
                summary_.stock_jump
                || (jump_requested
                    && (executed.authoritative_state_changed
                        || player->v.velocity.z > 0.0f));
            summary_.stock_duck =
                summary_.stock_duck
                || (duck_requested && executed.ducked);
            summary_.stock_grounding =
                summary_.stock_grounding || executed.grounded;
            ObserveStockTestFallLanding(
                slot_state,
                binding,
                player,
                executed);
            ObserveStockTestFallResponsiveness(
                slot_state,
                binding,
                player,
                executed,
                forward_requested || backward_requested || strafe_requested);
            summary_.previous_player_lifecycle_boundary_resolved = true;
            summary_.stock_advanced_past_previous_boundary = true;
            summary_.next_observed_boundary =
                "two_client_player_replication_required";
            SyncClientReplicationSummary(slot_state);
        }
        return true;
    }

    bool PrepareWorldBaselineBundle(
        DedicatedPlayerRuntimeSlot& slot_state)
    {
        if (world_baseline_payload_.has_value()
            && world_baseline_bundle_.has_value())
        {
            ++summary_.baseline_cached_outcome_reuses;
            return true;
        }
        ++summary_.baseline_build_attempts;
        if (!slot_state.goldsrc_delta_registry.has_value())
        {
            return FailTransport(
                "baseline-delta-registry-missing",
                "goldsrc_world_baseline_failed: reason=missing-delta-registry");
        }
        if (!state_.world_context.prepared
            || !state_.world_context.bsp_loaded
            || state_.world_context.world_edict_index != 0
            || state_.world_context.world_model_index <= 0)
        {
            return FailTransport(
                "baseline-world-context-invalid",
                "goldsrc_world_baseline_failed: reason=invalid-world-context");
        }
        edict_t* const world = state_.edict_store.World();
        const std::string* const world_model =
            state_.precache_registry.ModelName(
                state_.world_context.world_model_index);
        if (world == nullptr || world->free != FALSE
            || world->v.modelindex
                != state_.world_context.world_model_index
            || world_model == nullptr
            || *world_model != state_.world_context.model_path)
        {
            return FailTransport(
                "baseline-world-model-invalid",
                "goldsrc_world_baseline_failed: reason=invalid-world-model");
        }
        const int player_model_index =
            state_.precache_registry.ModelIndex("models/player.mdl");
        if (player_model_index <= 0
            || player_model_index
                > hl::network::kGoldSrcMaximumModelIndex
            || state_.server_state.maxclients <= 0
            || state_.server_state.maxclients
                > hl::network::kGoldSrcMaximumEntityIndex)
        {
            return FailTransport(
                "baseline-player-model-invalid",
                "goldsrc_world_baseline_failed: reason=invalid-player-model");
        }

        hl::network::GoldSrcBaselineBundle bundle;
        bundle.maximum_clients = static_cast<std::uint16_t>(
            state_.server_state.maxclients);
        const std::array<float, 3> player_mins = {
            -16.0f, -16.0f, -36.0f,
        };
        const std::array<float, 3> player_maxs = {
            16.0f, 16.0f, 36.0f,
        };
        const int maximum_entities = state_.edict_store.MaxEntities();
        for (int entity_index = 0;
             entity_index < maximum_entities;
             ++entity_index)
        {
            edict_t* const entity =
                state_.edict_store.EntityOfIndex(entity_index);
            const bool player =
                entity_index >= 1
                && entity_index <= state_.server_state.maxclients;
            if (entity == nullptr
                || (!player
                    && entity_index != 0
                    && (entity->free != FALSE
                        || entity->v.modelindex <= 0)))
            {
                continue;
            }

            entity_state_t captured{};
            std::memset(&captured, 0, sizeof(captured));
            captured.number = entity_index;
            captured.entityType =
                (entity->v.flags & FL_CUSTOMENTITY) != 0
                ? ENTITY_BEAM
                : ENTITY_NORMAL;
            std::array<float, 3> mins = player_mins;
            std::array<float, 3> maxs = player_maxs;
            if (!CallCreateBaseline(
                    player ? 1 : 0,
                    entity_index,
                    &captured,
                    entity,
                    player_model_index,
                    mins.data(),
                    maxs.data()))
            {
                return FailTransport(
                    "baseline-callback-failed",
                    "goldsrc_world_baseline_failed: reason=create-baseline-callback-failed");
            }
            ++summary_.baseline_callback_calls;
            if (captured.modelindex <= 0
                || captured.modelindex
                    > hl::network::kGoldSrcMaximumModelIndex)
            {
                return FailTransport(
                    "baseline-callback-model-invalid",
                    "goldsrc_world_baseline_failed: reason=callback-model-out-of-range");
            }

            hl::network::GoldSrcEntityBaseline baseline;
            baseline.entity_index =
                static_cast<std::uint16_t>(entity_index);
            baseline.kind = entity_index == 0
                ? hl::network::GoldSrcBaselineKind::kWorld
                : player
                    ? hl::network::GoldSrcBaselineKind::kPlayer
                    : captured.entityType == ENTITY_BEAM
                        ? hl::network::GoldSrcBaselineKind::kCustomEntity
                        : hl::network::GoldSrcBaselineKind::kEntity;
            baseline.model_index =
                static_cast<std::uint16_t>(captured.modelindex);
            baseline.provenance =
                hl::network::GoldSrcBaselineProvenance::kRuntimeMap;
            const std::string_view table_name =
                baseline.kind
                    == hl::network::GoldSrcBaselineKind::kPlayer
                ? "entity_state_player_t"
                : baseline.kind
                    == hl::network::GoldSrcBaselineKind::kCustomEntity
                ? "custom_entity_state_t"
                : "entity_state_t";
            const hl::network::GoldSrcDeltaTable* const table =
                FindBaselineDeltaTable(
                    *slot_state.goldsrc_delta_registry,
                    table_name);
            std::string capture_failure;
            if (table == nullptr
                || !CaptureBaselineDeltaState(
                    captured,
                    *table,
                    &baseline.state,
                    &capture_failure))
            {
                return FailTransport(
                    "baseline-state-capture-failed",
                    "goldsrc_world_baseline_failed: reason=delta-state-capture-failed"
                        ",entity=" + std::to_string(entity_index)
                        + ",table=" + std::string(table_name)
                        + ",source_angles="
                        + std::to_string(entity->v.angles.x) + "/"
                        + std::to_string(entity->v.angles.y) + "/"
                        + std::to_string(entity->v.angles.z)
                        + ",captured_angles="
                        + std::to_string(captured.angles.x) + "/"
                        + std::to_string(captured.angles.y) + "/"
                        + std::to_string(captured.angles.z)
                        + (capture_failure.empty()
                            ? ",detail=missing-table"
                            : "," + capture_failure));
            }
            bundle.entities.push_back(std::move(baseline));
        }

        if (!CallCreateInstancedBaselines())
        {
            return FailTransport(
                "baseline-instance-callback-failed",
                "goldsrc_world_baseline_failed: reason=create-instanced-baselines-callback-failed");
        }
        ++summary_.baseline_instanced_callback_calls;
        const hl::network::GoldSrcBaselineBuildStatus validation =
            hl::network::ValidateGoldSrcBaselineBundle(bundle);
        if (validation != hl::network::GoldSrcBaselineBuildStatus::kOk)
        {
            return FailTransport(
                "baseline-model-validation-failed",
                "goldsrc_world_baseline_failed: reason="
                    + std::string(hl::network::ReasonFor(validation)));
        }
        hl::network::GoldSrcBaselineEncodeResult encoded =
            hl::network::EncodeGoldSrcBaselineBundle(
                bundle,
                *slot_state.goldsrc_delta_registry,
                hl::network::kGoldSrcMaximumBaselineBundleBytes,
                state_.server_state.time);
        if (!encoded.ok())
        {
            return FailTransport(
                "baseline-encode-failed",
                "goldsrc_world_baseline_failed: reason="
                    + std::string(
                        hl::network::ReasonFor(encoded.status)));
        }
        const hl::network::GoldSrcBaselineDecodeResult semantic =
            hl::network::DecodeGoldSrcBaselineBundle(
                encoded.payload.bytes.data(),
                encoded.payload.size,
                bundle.maximum_clients,
                *slot_state.goldsrc_delta_registry,
                state_.server_state.time);
        if (!semantic.ok()
            || semantic.bundle.entities.size()
                != bundle.entities.size())
        {
            return FailTransport(
                "baseline-semantic-validation-failed",
                "goldsrc_world_baseline_failed: reason=semantic-round-trip-failed");
        }

        summary_.baseline_contract_verified = true;
        summary_.baseline_message_order_verified = true;
        summary_.baseline_context_built = true;
        ++summary_.baseline_generations;
        summary_.baseline_entity_count =
            static_cast<int>(bundle.entities.size());
        summary_.baseline_player_count =
            state_.server_state.maxclients;
        summary_.baseline_non_player_count =
            summary_.baseline_entity_count
            - summary_.baseline_player_count - 1;
        summary_.baseline_instance_count =
            static_cast<int>(bundle.instances.size());
        summary_.baseline_payload_bytes =
            static_cast<int>(encoded.payload.size);
        world_baseline_bundle_ = std::move(bundle);
        world_baseline_payload_ = std::move(encoded.payload);
        hl::common::Logger::Info(
            hl::common::LogCategory::Server,
            "goldsrc_world_baseline_built: source=runtime_map"
                ",entities="
                + std::to_string(summary_.baseline_entity_count)
                + ",players="
                + std::to_string(summary_.baseline_player_count)
                + ",non_players="
                + std::to_string(summary_.baseline_non_player_count)
                + ",instances="
                + std::to_string(summary_.baseline_instance_count)
                + ",payload_bytes="
                + std::to_string(summary_.baseline_payload_bytes)
                + ",callbacks="
                + std::to_string(summary_.baseline_callback_calls)
                + ",put_in_server=0,spawned=0,active=0");
        return true;
    }

    bool PrepareFirstSnapshot(DedicatedPlayerRuntimeSlot& slot_state)
    {
        if (!first_snapshot_enabled_)
        {
            return true;
        }
        if (slot_state.goldsrc_first_snapshot.phase()
            == hl::network::GoldSrcFirstSnapshotPhase::
                kFirstSnapshotPrepared)
        {
            return true;
        }
        if (!world_baseline_bundle_.has_value()
            || !slot_state.goldsrc_delta_registry.has_value())
        {
            return FailTransport(
                "first-snapshot-context-missing",
                "goldsrc_first_snapshot_failed: reason=missing-baseline-or-delta-context");
        }
        if (slot_state.put_in_server || slot_state.alive)
        {
            return FailTransport(
                "first-snapshot-player-lifecycle-required",
                "goldsrc_first_snapshot_failed: reason=unexpected-active-player");
        }

        const std::uint32_t frame_id =
            slot_state.netchan.outgoing_sequence();
        const float server_time =
            static_cast<float>(state_.server_state.time);
        hl::network::GoldSrcSnapshotBuildResult built =
            hl::network::BuildGoldSrcFirstSnapshot(
                frame_id,
                server_time,
                *world_baseline_bundle_,
                *slot_state.goldsrc_delta_registry);
        if (!built.ok())
        {
            return FailTransport(
                "first-snapshot-build-failed",
                "goldsrc_first_snapshot_failed: reason="
                    + std::string(
                        hl::network::ReasonFor(built.status)));
        }
        const hl::network::GoldSrcSnapshotDecodeResult decoded =
            hl::network::DecodeGoldSrcFirstSnapshot(
                built.bundle.payload.bytes.data(),
                built.bundle.payload.size,
                frame_id,
                *world_baseline_bundle_,
                *slot_state.goldsrc_delta_registry);
        if (!decoded.ok()
            || decoded.frame.entities.size()
                != built.bundle.frame.entities.size())
        {
            return FailTransport(
                "first-snapshot-self-check-failed",
                "goldsrc_first_snapshot_failed: reason=semantic-self-check");
        }
        if (slot_state.goldsrc_first_snapshot.Prepare(
                std::move(built.bundle))
            != hl::network::
                GoldSrcFirstSnapshotTransitionResult::kAdvanced)
        {
            return FailTransport(
                "first-snapshot-prepare-transition-failed",
                "goldsrc_first_snapshot_failed: reason=invalid-prepare-phase");
        }

        const hl::network::GoldSrcFirstSnapshotBundle& prepared =
            *slot_state.goldsrc_first_snapshot.prepared();
        summary_.snapshot_contract_verified = true;
        summary_.snapshot_message_order_verified = true;
        summary_.frame_ack_contract_verified = true;
        summary_.frame_history_implemented = true;
        summary_.frame_history_depth = static_cast<int>(
            slot_state.goldsrc_first_snapshot.history().capacity());
        summary_.clientdata_implemented = true;
        summary_.weapon_data_required = !prepared.frame.weapons.empty();
        summary_.packet_entities_implemented = true;
        summary_.first_snapshot_entity_count =
            static_cast<int>(prepared.frame.entities.size());
        summary_.first_snapshot_payload_bytes =
            static_cast<int>(prepared.payload.size);
        summary_.first_snapshot_frame_id = prepared.frame.frame_id;
        summary_.first_snapshot_server_time = prepared.frame.server_time;
        summary_.first_snapshot_visibility_policy =
            "runtime_map_modeled_nonplayer_baselines_no_pvs";
        summary_.frame_ack_source = "clc_delta_low8_server_frame";
        ++summary_.first_snapshot_prepared;
        SyncNetchanSummary(slot_state);
        hl::common::Logger::Info(
            hl::common::LogCategory::Server,
            "goldsrc_first_snapshot_prepared: session_id="
                + slot_state.session_id
                + ",frame_id="
                + std::to_string(summary_.first_snapshot_frame_id)
                + ",server_time="
                + std::to_string(summary_.first_snapshot_server_time)
                + ",messages=svc_time,svc_clientdata,svc_packetentities"
                + ",entities="
                + std::to_string(summary_.first_snapshot_entity_count)
                + ",weapon_data="
                + std::string(
                    summary_.weapon_data_required
                        ? "required"
                        : "not_required")
                + ",payload_bytes="
                + std::to_string(summary_.first_snapshot_payload_bytes)
                + ",visibility_policy="
                + summary_.first_snapshot_visibility_policy
                + ",reliability=unreliable"
                + ",signon_phase=" + summary_.signon_phase
                + ",put_in_server=0,spawned=0,active=0");
        return true;
    }

    bool QueueWorldBaselineBundle(
        DedicatedPlayerRuntimeSlot& slot_state,
        hl::network::GoldSrcNetchanReliablePayloadKind* queued_kind)
    {
        if (!PrepareWorldBaselineBundle(slot_state)
            || !world_baseline_payload_.has_value())
        {
            return false;
        }
        const hl::network::GoldSrcNetchanQueueResult queue =
            slot_state.netchan.QueueReliablePayload(
                world_baseline_payload_->bytes.data(),
                world_baseline_payload_->size,
                hl::network::GoldSrcNetchanReliablePayloadKind::
                    kBaselineBootstrap);
        if (queue
            != hl::network::GoldSrcNetchanQueueResult::kQueued)
        {
            return FailTransport(
                "baseline-queue-failed",
                "goldsrc_world_baseline_queue_failed: reason="
                    + std::string(hl::network::ReasonFor(queue)));
        }
        if (slot_state.goldsrc_signon.MarkBaselineBootstrapQueued()
            != hl::network::GoldSrcSignonTransitionResult::kAdvanced)
        {
            return FailTransport(
                "baseline-queue-transition-failed",
                "goldsrc_world_baseline_queue_failed: reason=invalid-signon-phase");
        }
        ++summary_.baseline_queued;
        *queued_kind =
            hl::network::GoldSrcNetchanReliablePayloadKind::
                kBaselineBootstrap;
        SyncNetchanSummary(slot_state);
        hl::common::Logger::Info(
            hl::common::LogCategory::Server,
            "goldsrc_world_baseline_queued: session_id="
                + slot_state.session_id
                + ",payload_bytes="
                + std::to_string(summary_.baseline_payload_bytes)
                + ",signon_phase=" + summary_.signon_phase);
        return true;
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
            const bool reliable_pending_before =
                slot_state.netchan.reliable_pending();
            if (!slot_state.goldsrc_delta_registry.has_value())
            {
                ++summary_.clc_move_rejected;
                hl::common::Logger::Warn(
                    hl::common::LogCategory::Server,
                    "goldsrc_post_resource_command_rejected: session_id="
                        + slot_state.session_id
                        + ",sequence=" + std::to_string(packet.sequence)
                        + ",reason=missing-usercmd-registry,signon_phase="
                        + std::string(hl::network::NameFor(
                            slot_state.goldsrc_signon.phase()))
                        + ",reliable_pending_bytes="
                        + std::to_string(
                            slot_state.netchan.reliable_pending_bytes()));
                return true;
            }
            const hl::network::GoldSrcClientApplicationDecodeResult
                application =
                    hl::network::DecodeGoldSrcClientApplicationPayload(
                        packet.payload.data(),
                        packet.payload_size,
                        packet.sequence,
                        *slot_state.goldsrc_delta_registry);
            if (!application.ok())
            {
                ++summary_.clc_move_rejected;
                if (application.status
                    == hl::network::GoldSrcClientApplicationDecodeStatus::
                        kUnknownOpcode)
                {
                    ++summary_.unsupported_opcode_rejected;
                }
                if (application.move_status
                    == hl::network::GoldSrcClientMoveDecodeStatus::
                        kInvalidChecksum)
                {
                    ++summary_.clc_move_checksum_rejected;
                }
                if (application.move_status
                    == hl::network::GoldSrcClientMoveDecodeStatus::
                        kCommandCountExceeded
                    || application.move_status
                        == hl::network::GoldSrcClientMoveDecodeStatus::
                            kPacketLossOutOfRange)
                {
                    ++summary_.clc_move_count_rejected;
                }
                if (application.move_status
                        == hl::network::GoldSrcClientMoveDecodeStatus::
                            kTruncatedEnvelope
                    || application.move_status
                        == hl::network::GoldSrcClientMoveDecodeStatus::
                            kTruncatedBitstream)
                {
                    ++summary_.clc_move_truncated_rejected;
                }
                summary_.post_resource_pending_reliable_preserved =
                    summary_.post_resource_pending_reliable_preserved
                    || (reliable_pending_before
                        && slot_state.netchan.reliable_pending());
                hl::common::Logger::Warn(
                    hl::common::LogCategory::Server,
                    "goldsrc_post_resource_command_rejected: session_id="
                        + slot_state.session_id
                        + ",sequence=" + std::to_string(packet.sequence)
                        + ",reason="
                        + std::string(hl::network::ReasonFor(
                            application.status))
                        + ",move_reason="
                        + std::string(hl::network::ReasonFor(
                            application.move_status))
                        + ",signon_phase="
                        + std::string(hl::network::NameFor(
                            slot_state.goldsrc_signon.phase()))
                        + ",reliable_pending_bytes="
                        + std::to_string(
                            slot_state.netchan.reliable_pending_bytes()));
                return true;
            }

            if (application.disconnect_present)
            {
                ++summary_.clc_stringcmd_decoded;
                return HandleClientDisconnect(slot_state);
            }

            if (first_snapshot_enabled_
                && (slot_state.goldsrc_first_snapshot.phase()
                        == hl::network::GoldSrcFirstSnapshotPhase::
                            kFirstSnapshotSentAwaitingClientReference
                    || slot_state.goldsrc_first_snapshot.phase()
                        == hl::network::GoldSrcFirstSnapshotPhase::
                            kFirstSnapshotAcknowledged
                    || slot_state.goldsrc_first_snapshot.phase()
                        == hl::network::GoldSrcFirstSnapshotPhase::
                            kContinuousSnapshotStreaming
                    || slot_state.goldsrc_first_snapshot.phase()
                        == hl::network::GoldSrcFirstSnapshotPhase::
                            kContinuousSnapshotStable))
            {
                if (!application.frame_reference_present)
                {
                    if (application.move_present)
                    {
                        ++summary_.clc_move_decoded;
                        if (!ExecutePmove(
                                slot_state,
                                application.move,
                                packet.sequence))
                        {
                            return false;
                        }
                        ++first_snapshot_keepalive_diagnostics_seen_;
                        if (ShouldLogDiagnosticSample(
                                first_snapshot_keepalive_diagnostics_seen_))
                        {
                            hl::common::Logger::Info(
                                hl::common::LogCategory::Server,
                                "goldsrc_first_snapshot_keepalive: session_id="
                                    + slot_state.session_id
                                    + ",sequence="
                                    + std::to_string(packet.sequence)
                                    + ",frame_reference=absent"
                                    + ",handling=decoded_pre_spawn_keepalive"
                                    + ",put_in_server=0,spawned=0,active=0");
                        }
                    }
                    return true;
                }
                const hl::network::GoldSrcFrameAcknowledgeResult
                    acknowledgement =
                        slot_state.goldsrc_first_snapshot.Acknowledge(
                            application.frame_reference,
                            packet.acknowledgement);
                switch (acknowledgement)
                {
                case hl::network::GoldSrcFrameAcknowledgeResult::
                    kAcknowledged:
                    ++slot_state.goldsrc_frames_acknowledged;
                    slot_state.goldsrc_last_successful_frame_ack_time_ms =
                        static_cast<std::uint64_t>(std::llround(
                            (std::max)(0.0, network_server_time_) * 1000.0));
                    if (hl::game_api::GoldSrcClientReplicationSummary* client =
                            FindClientReplicationSummary(slot_state.slot);
                        client != nullptr)
                    {
                        ++client->frames_acknowledged;
                    }
                    if (!slot_state
                            .goldsrc_continuous_snapshot_started_server_time
                            .has_value())
                    {
                        ++summary_.first_snapshot_acked;
                        summary_.previous_first_snapshot_boundary_resolved =
                            true;
                        summary_.stock_advanced_past_previous_boundary = true;
                        summary_.next_observed_boundary =
                            "continuous_snapshot_cadence_required";
                        if (continuous_snapshots_enabled_)
                        {
                            if (!slot_state.goldsrc_first_snapshot
                                    .StartContinuous(
                                        network_server_time_,
                                        snapshot_rate_hz_))
                            {
                                return FailTransport(
                                    "continuous-snapshot-scheduler-start-failed",
                                    "goldsrc_continuous_snapshot_failed: reason=scheduler-start");
                            }
                            slot_state
                                .goldsrc_continuous_snapshot_started_server_time =
                                network_server_time_;
                            summary_.continuous_snapshot_contract_verified =
                                true;
                            summary_.delta_packet_entities_contract_verified =
                                true;
                            summary_
                                .frame_reference_resolution_contract_verified =
                                true;
                            summary_.loss_recovery_contract_verified = true;
                            summary_.continuous_snapshot_streaming = true;
                        }
                    }
                    else
                    {
                        ++summary_.continuous_snapshots_acked;
                    }
                    if (slot_state.goldsrc_first_snapshot.history()
                            .last_acknowledged_frame()
                            .has_value())
                    {
                        const std::uint32_t acknowledged_frame =
                            *slot_state.goldsrc_first_snapshot.history()
                                 .last_acknowledged_frame();
                        if (!slot_state
                                .goldsrc_last_counted_acknowledged_frame
                                .has_value()
                            || *slot_state
                                .goldsrc_last_counted_acknowledged_frame
                                != acknowledged_frame)
                        {
                            slot_state
                                .goldsrc_last_counted_acknowledged_frame =
                                acknowledged_frame;
                            ++slot_state.goldsrc_distinct_frame_references;
                            ++summary_.distinct_frame_references;
                        }
                        const auto first_player_frame =
                            slot_state.goldsrc_player_lifecycle
                                .first_player_snapshot_frame_id();
                        if (player_lifecycle_enabled_
                            && first_player_frame.has_value()
                            && (acknowledged_frame == *first_player_frame
                                || hl::network::IsGoldSrcServerFrameNewer(
                                    acknowledged_frame,
                                    *first_player_frame))
                            && !slot_state.goldsrc_player_lifecycle
                                .signon_complete())
                        {
                            if (slot_state.goldsrc_player_lifecycle
                                    .MarkSignonComplete()
                                != hl::network::
                                    GoldSrcPlayerLifecycleResult::kAdvanced)
                            {
                                return FailTransport(
                                    "player-signon-transition-failed",
                                    "goldsrc_player_lifecycle_failed: reason=signon-transition");
                            }
                            summary_.player_signon_progression_verified =
                                true;
                            summary_
                                .previous_player_lifecycle_boundary_resolved =
                                true;
                            summary_.next_observed_boundary =
                                "movement_execution_required";
                            SyncPlayerLifecycleSummary(slot_state);
                            if (pmove_persistent_ && slot_state.slot == 1)
                            {
                                manual_session_lifecycle_
                                    .MarkSessionEstablished(
                                        MonotonicMilliseconds(
                                            last_network_host_frame_time_));
                                SyncManualSessionSummary();
                            }
                        }
                    }
                    summary_.server_still_responsive = true;
                    if (!first_snapshot_negative_proof_
                        && !continuous_snapshots_enabled_)
                    {
                        MarkComplete("first_snapshot_complete");
                    }
                    SyncClientReplicationSummary(slot_state);
                    break;
                case hl::network::GoldSrcFrameAcknowledgeResult::kDuplicate:
                    ++summary_.duplicate_frame_acknowledgements;
                    if (first_snapshot_negative_proof_
                        && !continuous_snapshots_enabled_
                        && summary_.first_snapshot_acked == 1
                        && summary_.future_frame_references >= 1
                        && summary_.evicted_frame_references >= 1)
                    {
                        summary_.server_still_responsive = true;
                        MarkComplete("first_snapshot_negative_complete");
                    }
                    break;
                case hl::network::GoldSrcFrameAcknowledgeResult::kFuture:
                    ++summary_.future_frame_references;
                    break;
                case hl::network::GoldSrcFrameAcknowledgeResult::kEvicted:
                    ++summary_.evicted_frame_references;
                    break;
                case hl::network::GoldSrcFrameAcknowledgeResult::kStale:
                    ++summary_.stale_frame_references;
                    if (continuous_snapshots_negative_proof_
                        && continuous_negative_unknown_observed_)
                    {
                        continuous_negative_stale_after_unknown_observed_ =
                            true;
                    }
                    break;
                case hl::network::GoldSrcFrameAcknowledgeResult::kAmbiguous:
                    ++summary_.ambiguous_frame_references;
                    break;
                case hl::network::GoldSrcFrameAcknowledgeResult::kInvalidPhase:
                case hl::network::GoldSrcFrameAcknowledgeResult::kUnknown:
                default:
                    ++summary_.unknown_frame_references;
                    if (continuous_snapshots_negative_proof_)
                    {
                        continuous_negative_unknown_observed_ = true;
                    }
                    break;
                }
                SyncNetchanSummary(slot_state);
                const bool log_frame_reference =
                    acknowledgement
                        == hl::network::GoldSrcFrameAcknowledgeResult::
                            kAcknowledged
                    || (continuous_snapshots_negative_proof_
                        && (acknowledgement
                                == hl::network::
                                    GoldSrcFrameAcknowledgeResult::kUnknown
                            || acknowledgement
                                == hl::network::
                                    GoldSrcFrameAcknowledgeResult::kStale))
                    || ShouldLogDiagnosticSample(
                        ++frame_reference_diagnostics_seen_);
                if (log_frame_reference)
                {
                    hl::common::Logger::Info(
                        hl::common::LogCategory::Server,
                        "goldsrc_first_snapshot_frame_reference: session_id="
                        + slot_state.session_id
                        + ",sequence=" + std::to_string(packet.sequence)
                        + ",wire_frame="
                        + std::to_string(application.frame_reference)
                        + ",resolved_frame="
                        + (slot_state.goldsrc_first_snapshot.history()
                                    .last_acknowledged_frame()
                                    .has_value()
                                ? std::to_string(
                                    *slot_state.goldsrc_first_snapshot
                                         .history()
                                         .last_acknowledged_frame())
                                : "none")
                        + ",result="
                        + std::string(
                            hl::network::ReasonFor(acknowledgement))
                        + ",source=clc_delta"
                        + ",signon_phase=" + summary_.signon_phase
                        + ",next_boundary="
                        + summary_.next_observed_boundary
                        + ",put_in_server="
                        + (slot_state.goldsrc_player_lifecycle
                                .put_in_server()
                            ? "1" : "0")
                        + ",spawned="
                        + (slot_state.goldsrc_player_lifecycle
                                .player()
                                .player_spawned
                            ? "1" : "0")
                        + ",active=0");
                }
                if (application.move_present)
                {
                    ++summary_.clc_move_decoded;
                    if (!ExecutePmove(
                            slot_state,
                            application.move,
                            packet.sequence))
                    {
                        return false;
                    }
                }
                return true;
            }

            if (slot_state.goldsrc_signon.phase()
                == hl::network::GoldSrcSignonPhase::
                    kResourceManifestAcknowledged)
            {
                if (slot_state.goldsrc_signon
                        .EnterAwaitingPostResourceCommand()
                    != hl::network::GoldSrcSignonTransitionResult::kAdvanced)
                {
                    return FailTransport(
                        "post-resource-transition-failed",
                        "goldsrc_post_resource_transition_failed: reason=awaiting-command");
                }
            }
            const hl::network::GoldSrcSignonCommandDisposition disposition =
                slot_state.goldsrc_signon.HandlePostResourceMove();
            if (disposition
                != hl::network::GoldSrcSignonCommandDisposition::kDelivered)
            {
                ++summary_.clc_move_rejected;
                summary_.post_resource_pending_reliable_preserved =
                    summary_.post_resource_pending_reliable_preserved
                    || (reliable_pending_before
                        && slot_state.netchan.reliable_pending());
                SyncNetchanSummary(slot_state);
                hl::common::Logger::Warn(
                    hl::common::LogCategory::Server,
                    "goldsrc_post_resource_command_rejected: session_id="
                        + slot_state.session_id
                        + ",sequence=" + std::to_string(packet.sequence)
                        + ",reason=invalid-signon-phase,signon_phase="
                        + summary_.signon_phase
                        + ",reliable_pending_bytes="
                        + std::to_string(
                            slot_state.netchan.reliable_pending_bytes()));
                return true;
            }

            ++summary_.clc_move_decoded;
            summary_.post_resource_checksum_validated = true;
            summary_.post_resource_delta_schema_used = true;
            summary_.post_resource_pre_spawn_ignored = true;
            summary_.previous_unidentified_command_boundary_resolved = true;
            summary_.last_move_packet_loss =
                application.move.packet_loss;
            summary_.last_move_backup_commands =
                application.move.backup_command_count;
            summary_.last_move_new_commands =
                application.move.new_command_count;
            summary_.last_move_total_commands =
                static_cast<int>(application.move.command_count);
            summary_.last_move_new_command_msec =
                static_cast<int>(application.move.new_command_time_msec);
            summary_.post_resource_command_identity = "clc_move";
            summary_.post_resource_continuation =
                "awaiting_server_baseline_or_snapshot";
            SyncNetchanSummary(slot_state);
            hl::common::Logger::Info(
                hl::common::LogCategory::Server,
                "goldsrc_post_resource_clc_move_accepted: session_id="
                    + slot_state.session_id
                    + ",sequence=" + std::to_string(packet.sequence)
                    + ",packet_loss="
                    + std::to_string(application.move.packet_loss)
                    + ",backup_commands="
                    + std::to_string(
                        application.move.backup_command_count)
                    + ",new_commands="
                    + std::to_string(application.move.new_command_count)
                    + ",total_commands="
                    + std::to_string(application.move.command_count)
                    + ",new_command_msec="
                    + std::to_string(
                        application.move.new_command_time_msec)
                    + ",checksum=valid,delta_schema=usercmd_t"
                    + ",handling=decoded_pre_spawn_keepalive"
                    + ",signon_phase=" + summary_.signon_phase
                    + ",put_in_server=0,spawned=0,active=0");
            if (world_baselines_enabled_)
            {
                const hl::network::GoldSrcSignonTransitionResult transition =
                    slot_state.goldsrc_signon
                        .EnterAwaitingBaselineBootstrap();
                if (transition
                        == hl::network::GoldSrcSignonTransitionResult::
                            kAdvanced)
                {
                    return QueueWorldBaselineBundle(
                        slot_state,
                        queued_kind);
                }
                if (transition
                    != hl::network::GoldSrcSignonTransitionResult::
                        kAlreadyApplied)
                {
                    return FailTransport(
                        "baseline-await-transition-failed",
                        "goldsrc_world_baseline_failed: reason=await-transition-failed");
                }
            }
            return true;
        }

        const hl::network::GoldSrcClientSignonDecodeResult decoded =
            hl::network::DecodeGoldSrcClientSignonPayload(
                packet.payload.data(),
                packet.payload_size);
        const hl::network::GoldSrcSendEntitiesEnvelopeDecodeResult
            send_entities_envelope =
                hl::network::DecodeGoldSrcSendEntitiesEnvelope(
                    packet.payload.data(),
                    packet.payload_size);
        bool send_entities_with_trailing_move = false;
        if (!decoded.ok()
            && world_baselines_enabled_
            && packet.reliable_present
            && send_entities_envelope.ok()
            && send_entities_envelope.application_offset < packet.payload_size
            && slot_state.goldsrc_delta_registry.has_value())
        {
            const hl::network::GoldSrcClientApplicationDecodeResult
                trailing_application =
                    hl::network::DecodeGoldSrcClientApplicationPayload(
                        packet.payload.data()
                            + send_entities_envelope.application_offset,
                        packet.payload_size
                            - send_entities_envelope.application_offset,
                        packet.sequence,
                        *slot_state.goldsrc_delta_registry);
            send_entities_with_trailing_move = trailing_application.ok();
        }
        if (!decoded.ok() && !send_entities_with_trailing_move)
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
            return HandleClientDisconnect(slot_state);
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
            if (player_lifecycle_enabled_
                && !ConnectPlayerToGameDll(slot_state))
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
                == hl::network::GoldSrcClientSignonCommand::kSendEntities
            || send_entities_with_trailing_move)
        {
            const hl::network::GoldSrcSignonCommandDisposition disposition =
                slot_state.goldsrc_signon.HandleSendEntities();
            ++summary_.send_entities_received;
            if (!world_baselines_enabled_
                || disposition
                    != hl::network::GoldSrcSignonCommandDisposition::
                        kDelivered)
            {
                return true;
            }
            ++summary_.send_entities_delivered;
            summary_.previous_server_baseline_boundary_resolved = true;
            summary_.next_observed_boundary = first_snapshot_enabled_
                ? "first_snapshot_scheduled"
                : "first_snapshot_required";
            summary_.server_still_responsive = true;
            if (first_snapshot_enabled_
                && !PrepareFirstSnapshot(slot_state))
            {
                return false;
            }
            SyncNetchanSummary(slot_state);
            hl::common::Logger::Info(
                hl::common::LogCategory::Server,
                "goldsrc_sendents_accepted: session_id="
                    + slot_state.session_id
                    + ",signon_phase=" + summary_.signon_phase
                    + ",trailing_clc_move="
                    + std::string(
                        send_entities_with_trailing_move ? "validated" : "none")
                    + ",vmod_enable_prefix="
                    + std::string(
                        send_entities_envelope.observed_vmod_enable_prefix
                            ? "validated"
                            : "none")
                    + ",next_boundary="
                    + summary_.next_observed_boundary
                    + ",put_in_server=0,spawned=0,active=0");
            if (!first_snapshot_enabled_)
            {
                MarkComplete("post_resource_command_complete");
            }
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
        if (!expect_reliable
            && datagrams_sent_this_frame_
                >= kMaximumDatagramsSentPerHostFrame)
        {
            // Stock clients can burst several keepalives in the same host
            // frame. An ordinary empty ACK is expendable; preserving the
            // bounded send budget and the already-prioritized snapshot is
            // preferable to treating that burst as a socket failure.
            return true;
        }
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

        if (expect_reliable)
        {
            ++summary_.reliable_datagrams_sent;
            summary_.reliable_priority_preserved = true;
        }
        else
        {
            ++summary_.ordinary_ack_datagrams_sent;
            if (slot_state.goldsrc_pending_empty_ack)
            {
                ++slot_state.goldsrc_ordinary_ack_datagrams_sent;
            }
        }
        slot_state.goldsrc_pending_empty_ack = false;
        slot_state.goldsrc_pending_empty_ack_packets = 0u;
        slot_state.goldsrc_pending_empty_ack_since_server_time = 0.0;

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
        if (world_baselines_enabled_
            && pending_kind
                == hl::network::GoldSrcNetchanReliablePayloadKind::
                    kBaselineBootstrap
            && decoded.packet.reliable_present)
        {
            const hl::network::GoldSrcNetchanDiagnostics& diagnostics =
                slot_state.netchan.diagnostics();
            if (diagnostics.reliable_resent > reliable_resent_before)
            {
                ++summary_.baseline_resent;
            }
            else if (diagnostics.reliable_sent > reliable_sent_before
                && summary_.baseline_sent == 0)
            {
                ++summary_.baseline_sent;
                if (slot_state.goldsrc_signon
                        .MarkBaselineBootstrapSent()
                    != hl::network::GoldSrcSignonTransitionResult::
                        kAdvanced)
                {
                    return FailTransport(
                        "baseline-sent-transition-failed",
                        "goldsrc_world_baseline_send_failed: reason=invalid-signon-phase");
                }
                const hl::network::GoldSrcFragmentSender& sender =
                    slot_state.netchan.fragment_sender();
                summary_.baseline_fragmented = sender.active();
                summary_.baseline_fragment_count =
                    summary_.baseline_fragmented
                    ? static_cast<int>(sender.fragment_count())
                    : 0;
            }
        }

        SyncNetchanSummary(slot_state);
        const bool steady_ack_response =
            std::string_view(event_name).find("_ack_response")
            != std::string_view::npos;
        if (!steady_ack_response
            || ShouldLogDiagnosticSample(
                ++steady_netchan_send_diagnostics_seen_))
        {
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
        }
        return true;
    }

    void RecordRemotePlayerSnapshot(
        DedicatedPlayerRuntimeSlot& receiver,
        const hl::network::GoldSrcPlayerSnapshotInput& snapshot)
    {
        hl::game_api::GoldSrcClientReplicationSummary* const client =
            FindClientReplicationSummary(receiver.slot);
        if (client == nullptr)
        {
            return;
        }
        std::vector<int> visible_slots;
        visible_slots.reserve(snapshot.remote_entities.size());
        for (const hl::network::GoldSrcSnapshotEntityState& remote :
             snapshot.remote_entities)
        {
            const int remote_slot = static_cast<int>(remote.entity_index);
            DedicatedPlayerRuntimeSlot* const candidate =
                FindDedicatedPlayerRuntimeSlot(
                    state_.dedicated_multiplayer_foundation,
                    remote_slot);
            edict_t* const entity = candidate == nullptr
                ? nullptr
                : state_.edict_store.EntityOfIndex(remote_slot);
            if (candidate == nullptr || entity == nullptr)
            {
                continue;
            }
            visible_slots.push_back(remote_slot);
            auto observation = std::find_if(
                receiver.goldsrc_remote_player_observations.begin(),
                receiver.goldsrc_remote_player_observations.end(),
                [remote_slot](const GoldSrcRemotePlayerObservation& value)
                {
                    return value.slot == remote_slot;
                });
            if (observation
                == receiver.goldsrc_remote_player_observations.end())
            {
                receiver.goldsrc_remote_player_observations.push_back({});
                observation =
                    receiver.goldsrc_remote_player_observations.end() - 1;
                observation->slot = remote_slot;
            }
            const bool same_generation =
                observation->session_id == candidate->session_id;
            const bool first_for_receiver =
                !observation->visible || !same_generation;
            const std::size_t receiver_index =
                static_cast<std::size_t>(receiver.slot - 1);
            const std::size_t remote_index =
                static_cast<std::size_t>(remote_slot - 1);
            const bool reset_transition =
                receiver_index < stock_test_reset_transitions_.size()
                && remote_index
                    < stock_test_reset_transitions_[receiver_index].size()
                && stock_test_reset_transitions_[receiver_index][remote_index];
            const int network_effects = first_for_receiver || reset_transition
                ? entity->v.effects
                : entity->v.effects & ~EF_NOINTERP;
            const bool network_nointerp = reset_transition
                || (network_effects & EF_NOINTERP) != 0;
            const float network_animtime =
                static_cast<float>(network_server_time_);
            if (first_for_receiver)
            {
                ++client->remote_player_adds;
                observation->cadence_warmed_up = false;
                if (!observation->session_id.empty() && !same_generation)
                {
                    ++client->remote_player_readds;
                }
            }
            else if (reset_transition)
            {
                ++client->remote_player_updates;
                ++client->reset_transition_nointerp_frames;
                ++client->interpolation_samples_excluded_for_reset;
                client->nointerp_cleared_after_reset = false;
                observation->cadence_warmed_up = false;
                stock_test_reset_transitions_[receiver_index][remote_index] =
                    false;
            }
            else
            {
                ++client->remote_player_updates;
                client->remote_entity_present_consecutively = true;
                if (network_animtime + 0.0005f < observation->animtime)
                {
                    client->remote_animtime_monotonic = false;
                }
                if (network_nointerp)
                {
                    client->permanent_remote_nointerp = true;
                }
                if (receiver_index
                        < stock_test_nointerp_clear_pending_.size()
                    && remote_index
                        < stock_test_nointerp_clear_pending_[receiver_index]
                            .size()
                    && stock_test_nointerp_clear_pending_[receiver_index][
                        remote_index]
                    && !network_nointerp)
                {
                    client->nointerp_cleared_after_reset = true;
                    stock_test_nointerp_clear_pending_[receiver_index][
                        remote_index] = false;
                }
                const double elapsed = network_server_time_
                    - observation->server_time;
                if (std::isfinite(elapsed) && elapsed >= 0.0)
                {
                    const int gap_ms = static_cast<int>(std::llround(
                        elapsed * 1000.0));
                    if (!observation->cadence_warmed_up)
                    {
                        observation->cadence_warmed_up = true;
                    }
                    else
                    {
                        const std::size_t interval_bucket = (std::min)(
                            observation->interval_histogram.size() - 1u,
                            static_cast<std::size_t>((std::max)(0, gap_ms)));
                        ++observation->interval_histogram[interval_bucket];
                        ++observation->interval_samples;
                        client->remote_update_interval_median_ms =
                            static_cast<int>(PercentileFromHistogram(
                                observation->interval_histogram,
                                observation->interval_samples,
                                50u));
                        client->remote_update_interval_p95_ms =
                            static_cast<int>(PercentileFromHistogram(
                                observation->interval_histogram,
                                observation->interval_samples,
                                95u));
                        client->maximum_remote_snapshot_gap_ms = (std::max)(
                            client->maximum_remote_snapshot_gap_ms,
                            gap_ms);
                        if (gap_ms
                            > static_cast<int>(
                                summary_.snapshot_interval_ms * 3.0))
                        {
                            ++client->remote_snapshot_update_gaps;
                        }
                        const double dx =
                            static_cast<double>(entity->v.origin.x)
                            - observation->origin.x;
                        const double dy =
                            static_cast<double>(entity->v.origin.y)
                            - observation->origin.y;
                        const double dz =
                            static_cast<double>(entity->v.origin.z)
                            - observation->origin.z;
                        const double distance = std::sqrt(
                            dx * dx + dy * dy + dz * dz);
                        client->last_remote_origin_delta = distance;
                        client->maximum_remote_origin_delta = (std::max)(
                            client->maximum_remote_origin_delta,
                            distance);
                        const double allowed_distance = (std::max)(
                            128.0,
                            elapsed * 2048.0);
                        if (!std::isfinite(distance)
                            || distance > allowed_distance)
                        {
                            ++client->remote_origin_discontinuities;
                        }
                    }
                }
            }
            observation->session_id = candidate->session_id;
            observation->origin = entity->v.origin;
            observation->velocity = entity->v.velocity;
            observation->server_time = network_server_time_;
            observation->animtime = network_animtime;
            observation->sequence = entity->v.sequence;
            observation->frame = entity->v.frame;
            observation->framerate = entity->v.framerate;
            observation->effects = network_effects;
            observation->nointerp = network_nointerp;
            observation->visible = true;
        }
        for (GoldSrcRemotePlayerObservation& observation :
             receiver.goldsrc_remote_player_observations)
        {
            if (!observation.visible
                || std::find(
                    visible_slots.begin(),
                    visible_slots.end(),
                    observation.slot) != visible_slots.end())
            {
                continue;
            }
            observation.visible = false;
            ++client->remote_player_removes;
        }
        RefreshTwoClientReplicationSummary();
    }

    bool SendContinuousSnapshotDatagram(
        DedicatedPlayerRuntimeSlot& slot_state)
    {
        if (!continuous_snapshots_enabled_
            || !world_baseline_bundle_.has_value()
            || !slot_state.goldsrc_delta_registry.has_value())
        {
            return FailTransport(
                "continuous-snapshot-send-invalid",
                "goldsrc_continuous_snapshot_failed: reason=missing-context");
        }

        const std::uint32_t frame_id =
            slot_state.netchan.outgoing_sequence();
        const hl::network::GoldSrcServerFrame* acknowledged_base =
            slot_state.goldsrc_first_snapshot.history()
                .last_acknowledged();
        std::optional<hl::network::GoldSrcPlayerSnapshotInput>
            player_snapshot;
        if (player_lifecycle_enabled_
            && slot_state.goldsrc_player_lifecycle.put_in_server())
        {
            std::string player_snapshot_failure;
            player_snapshot.emplace();
            if (!BuildPlayerSnapshotInput(
                    slot_state,
                    &*player_snapshot,
                    &player_snapshot_failure))
            {
                return FailTransport(
                    "player-snapshot-build-failed",
                    "goldsrc_player_snapshot_failed: reason="
                        + SanitizeLogField(player_snapshot_failure));
            }
        }
        const hl::network::GoldSrcContinuousSnapshotBuildResult built =
            hl::network::BuildGoldSrcContinuousSnapshot(
                frame_id,
                static_cast<float>(network_server_time_),
                acknowledged_base,
                *world_baseline_bundle_,
                *slot_state.goldsrc_delta_registry,
                hl::network::kGoldSrcMaximumSnapshotBytes,
                player_snapshot.has_value()
                    ? &*player_snapshot
                    : nullptr);
        if (!built.ok())
        {
            std::string failure =
                "goldsrc_continuous_snapshot_failed: reason="
                + std::string(hl::network::ReasonFor(built.status));
            if (!built.failure_table.empty())
            {
                failure += ",table="
                    + SanitizeLogField(
                        std::string(built.failure_table));
            }
            if (!built.failure_field.empty())
            {
                failure += ",field="
                    + SanitizeLogField(
                        std::string(built.failure_field));
            }
            return FailTransport(
                "continuous-snapshot-build-failed",
                std::move(failure));
        }
        ++summary_.continuous_snapshots_generated;
        if (combat_enabled_)
        {
            const hl::game_api::GoldSrcClientReplicationSummary* const
                combat_client =
                    FindClientReplicationSummary(slot_state.slot);
            hl::common::Logger::Info(
                hl::common::LogCategory::Server,
                "goldsrc_combat_snapshot_frame: slot="
                    + std::to_string(slot_state.slot)
                    + ",frame_id="
                    + std::to_string(built.bundle.frame.frame_id)
                    + ",base_frame="
                    + (built.bundle.base_frame_id.has_value()
                        ? std::to_string(*built.bundle.base_frame_id)
                        : "none")
                    + ",health="
                    + std::to_string(
                        combat_client != nullptr
                            ? combat_client->clientdata_health
                            : 0.0f)
                    + ",clip="
                    + std::to_string(
                        combat_client != nullptr
                            ? combat_client->glock_clip
                            : -1)
                    + ",origin_x="
                    + std::to_string(
                        combat_client != nullptr
                            ? combat_client->authoritative_origin_x
                            : 0.0f)
                    + ",origin_y="
                    + std::to_string(
                        combat_client != nullptr
                            ? combat_client->authoritative_origin_y
                            : 0.0f));
        }

        hl::network::GoldSrcNetchanDatagram datagram;
        if (!slot_state.netchan.BuildOutgoingUnreliableDatagram(
                built.bundle.payload.bytes.data(),
                built.bundle.payload.size,
                &datagram))
        {
            return FailTransport(
                "continuous-snapshot-netchan-encode-failed",
                "goldsrc_continuous_snapshot_failed: reason=unreliable-netchan-encode");
        }
        const hl::network::GoldSrcNetchanDecodeResult carrier =
            hl::network::DecodeGoldSrcNetchanDatagram(
                hl::network::GoldSrcNetchanDirection::kServerToClient,
                datagram.bytes.data(),
                datagram.size);
        if (!carrier.ok() || carrier.packet.reliable_present
            || carrier.packet.fragment_present
            || carrier.packet.sequence != built.bundle.frame.frame_id
            || carrier.packet.acknowledgement
                != slot_state.netchan.incoming_sequence()
            || carrier.packet.payload_size != built.bundle.payload.size)
        {
            return FailTransport(
                "continuous-snapshot-carrier-self-check-failed",
                "goldsrc_continuous_snapshot_failed: reason=carrier-self-check");
        }
        const bool same_frame_movement_commit =
            slot_state.goldsrc_last_movement_commit_packet_sequence
                .has_value()
            && slot_state.goldsrc_last_movement_commit_host_frame
                == summary_.host_frames_pumped;
        if (same_frame_movement_commit)
        {
            const std::uint32_t movement_sequence =
                *slot_state.goldsrc_last_movement_commit_packet_sequence;
            const bool acknowledgement_covers_movement =
                carrier.packet.acknowledgement == movement_sequence
                || hl::network::IsGoldSrcNetchanSequenceNewer(
                    carrier.packet.acknowledgement,
                    movement_sequence);
            if (!acknowledgement_covers_movement)
            {
                return FailTransport(
                    "continuous-snapshot-movement-ack-incoherent",
                    "goldsrc_continuous_snapshot_failed: reason=movement-ack-incoherent");
            }
            summary_.snapshot_move_ack_coherence = true;
            summary_.same_host_frame_movement_visible = true;
            ++summary_.same_host_frame_movement_snapshots;
        }

        hl::network::GoldSrcSnapshotDecodeResult decoded;
        if (built.bundle.kind
            == hl::network::GoldSrcSnapshotKind::kDelta)
        {
            if (acknowledged_base == nullptr
                || !built.bundle.base_frame_id.has_value()
                || acknowledged_base->frame_id
                    != *built.bundle.base_frame_id)
            {
                return FailTransport(
                    "continuous-snapshot-base-selection-failed",
                    "goldsrc_continuous_snapshot_failed: reason=acknowledged-base-drift");
            }
            decoded = hl::network::DecodeGoldSrcDeltaSnapshot(
                carrier.packet.payload.data(),
                carrier.packet.payload_size,
                carrier.packet.sequence,
                *acknowledged_base,
                *world_baseline_bundle_,
                *slot_state.goldsrc_delta_registry);
            summary_.newest_acknowledged_base_selected = true;
        }
        else
        {
            decoded = hl::network::DecodeGoldSrcFirstSnapshot(
                carrier.packet.payload.data(),
                carrier.packet.payload_size,
                carrier.packet.sequence,
                *world_baseline_bundle_,
                *slot_state.goldsrc_delta_registry);
        }
        if (!decoded.ok()
            || decoded.frame.entities.size()
                != built.bundle.frame.entities.size())
        {
            return FailTransport(
                "continuous-snapshot-payload-self-check-failed",
                "goldsrc_continuous_snapshot_failed: reason=payload-self-check");
        }
        if (combat_enabled_ && player_snapshot.has_value())
        {
            const auto field_index =
                [&slot_state](std::string_view table_name,
                              std::string_view field_name)
                    -> std::optional<std::size_t>
                {
                    const hl::network::GoldSrcDeltaTable* const table =
                        FindBaselineDeltaTable(
                            *slot_state.goldsrc_delta_registry,
                            table_name);
                    if (table == nullptr)
                    {
                        return std::nullopt;
                    }
                    for (std::size_t index = 0u;
                         index < table->fields.size();
                         ++index)
                    {
                        if (table->fields[index].name == field_name)
                        {
                            return index;
                        }
                    }
                    return std::nullopt;
                };
            const auto numeric_value = [](
                const hl::network::GoldSrcDecodedDeltaValue& value,
                double* output) noexcept
            {
                if (output == nullptr)
                {
                    return false;
                }
                switch (value.kind)
                {
                case hl::network::GoldSrcDeltaValueKind::kUnsignedInteger:
                    *output = static_cast<double>(value.unsigned_value);
                    return true;
                case hl::network::GoldSrcDeltaValueKind::kSignedInteger:
                    *output = static_cast<double>(value.signed_value);
                    return true;
                case hl::network::GoldSrcDeltaValueKind::kFloatingPoint:
                    *output = value.floating_value;
                    return std::isfinite(*output);
                case hl::network::GoldSrcDeltaValueKind::kString:
                default:
                    return false;
                }
            };
            const auto records_match =
                [&numeric_value](
                    const hl::network::GoldSrcDecodedDeltaRecord& expected,
                    const hl::network::GoldSrcDecodedDeltaRecord& observed,
                    std::size_t index) noexcept
                {
                    if (index >= expected.field_count
                        || index >= observed.field_count)
                    {
                        return false;
                    }
                    double expected_value = 0.0;
                    double observed_value = 0.0;
                    return numeric_value(
                               expected.values[index],
                               &expected_value)
                        && numeric_value(
                               observed.values[index],
                               &observed_value)
                        && std::abs(expected_value - observed_value)
                            <= 0.0001;
                };
            const auto find_weapon = [](
                const hl::network::GoldSrcServerFrame& frame,
                std::uint8_t weapon_index)
                -> const hl::network::GoldSrcWeaponState*
            {
                const auto found = std::find_if(
                    frame.weapons.begin(),
                    frame.weapons.end(),
                    [weapon_index](
                        const hl::network::GoldSrcWeaponState& weapon)
                    {
                        return weapon.weapon_index == weapon_index;
                    });
                return found == frame.weapons.end() ? nullptr : &*found;
            };
            const std::optional<std::size_t> health_index =
                field_index("clientdata_t", "health");
            const std::optional<std::size_t> clip_index =
                field_index("weapon_data_t", "m_iClip");
            const hl::network::GoldSrcWeaponState* const expected_glock =
                find_weapon(
                    built.bundle.frame,
                    hl::network::kGoldSrcStockGlockWeaponId);
            const hl::network::GoldSrcWeaponState* const observed_glock =
                find_weapon(
                    decoded.frame,
                    hl::network::kGoldSrcStockGlockWeaponId);
            double expected_health = 0.0;
            const bool health_matches = health_index.has_value()
                && numeric_value(
                    built.bundle.frame.clientdata.state.values[*health_index],
                    &expected_health)
                && records_match(
                    built.bundle.frame.clientdata.state,
                    decoded.frame.clientdata.state,
                    *health_index);
            const bool weapon_presence_matches =
                hl::network::GoldSrcCombatWeaponPresenceMatches(
                    expected_health,
                    expected_glock != nullptr,
                    observed_glock != nullptr);
            const bool weapon_clip_matches = expected_glock == nullptr
                || (clip_index.has_value()
                    && expected_glock != nullptr
                    && observed_glock != nullptr
                    && records_match(
                        expected_glock->state,
                        observed_glock->state,
                        *clip_index));
            if (!health_matches || !weapon_presence_matches
                || !weapon_clip_matches)
            {
                return FailTransport(
                    "continuous-snapshot-combat-semantic-self-check-failed",
                    "goldsrc_continuous_snapshot_failed: reason=combat-semantic-self-check");
            }
        }
        if (!SendBoundedDatagram(
                datagram.bytes.data(),
                datagram.size,
                slot_state.netchan.remote_endpoint()))
        {
            return FailTransport(
                "continuous-snapshot-send-failed",
                "goldsrc_continuous_snapshot_failed: platform_error="
                    + socket_.LastError());
        }
        ++summary_.snapshot_datagrams_sent;
        summary_.snapshot_capacity_reserved = true;
        slot_state.goldsrc_pending_empty_ack = false;
        slot_state.goldsrc_pending_empty_ack_packets = 0u;
        slot_state.goldsrc_pending_empty_ack_since_server_time = 0.0;
        if (slot_state.goldsrc_first_snapshot.MarkContinuousSent(
                built.bundle)
            != hl::network::GoldSrcFrameStoreResult::kStored)
        {
            return FailTransport(
                "continuous-snapshot-history-store-failed",
                "goldsrc_continuous_snapshot_failed: reason=history-store");
        }

        const auto& scheduler =
            slot_state.goldsrc_first_snapshot.scheduler().state();
        summary_.minimum_snapshot_gap_ms = static_cast<int>(
            std::llround(
                scheduler.minimum_due_interval_seconds * 1000.0));
        summary_.median_snapshot_interval_ms = static_cast<int>(
            scheduler.median_due_interval_msec);
        summary_.p95_snapshot_interval_ms = static_cast<int>(
            scheduler.p95_due_interval_msec);
        summary_.maximum_snapshot_gap_ms = static_cast<int>(
            std::llround(
                scheduler.maximum_due_interval_seconds * 1000.0));
        summary_.snapshot_burst_count = static_cast<int>(
            scheduler.snapshot_burst_count);
        summary_.missed_snapshot_intervals = static_cast<int>(
            scheduler.skipped_snapshots);
        if (summary_.maximum_snapshot_gap_ms
                > slot_state.goldsrc_last_observed_snapshot_gap_ms)
        {
            slot_state.goldsrc_last_observed_snapshot_gap_ms =
                summary_.maximum_snapshot_gap_ms;
            if (summary_.maximum_snapshot_gap_ms
                    > static_cast<int>(summary_.snapshot_interval_ms * 2.0))
            {
                ++snapshot_cadence_gap_diagnostics_seen_;
                if (ShouldLogDiagnosticSample(
                        snapshot_cadence_gap_diagnostics_seen_))
                {
                    hl::common::Logger::Warn(
                        hl::common::LogCategory::Server,
                        "goldsrc_snapshot_cadence_gap: snapshot_frame_id="
                            + std::to_string(built.bundle.frame.frame_id)
                            + ",snapshot_gap_msec="
                            + std::to_string(
                                summary_.maximum_snapshot_gap_ms));
                }
            }
        }

        ++summary_.continuous_snapshots_sent;
        ++slot_state.goldsrc_continuous_snapshots_sent;
        slot_state.goldsrc_last_successful_snapshot_time_ms =
            static_cast<std::uint64_t>(std::llround(
                (std::max)(0.0, network_server_time_) * 1000.0));
        if (hl::game_api::GoldSrcClientReplicationSummary* client =
                FindClientReplicationSummary(slot_state.slot);
            client != nullptr)
        {
            ++client->snapshots_sent;
        }
        if (player_snapshot.has_value())
        {
            RecordRemotePlayerSnapshot(slot_state, *player_snapshot);
            const std::uint16_t player_index =
                slot_state.goldsrc_player_lifecycle.binding().edict_index;
            bool player_operation_counted = false;
            for (const auto& operation : built.bundle.entity_diff.operations)
            {
                if (operation.entity_index != player_index)
                {
                    continue;
                }
                player_operation_counted = true;
                if (operation.kind
                    == hl::network::
                        GoldSrcEntityDeltaOperationKind::kAdd)
                {
                    ++summary_.player_snapshot_adds;
                }
                else if (operation.kind
                    == hl::network::
                        GoldSrcEntityDeltaOperationKind::kUpdate)
                {
                    ++summary_.player_snapshot_updates;
                }
                else if (operation.kind
                    == hl::network::
                        GoldSrcEntityDeltaOperationKind::kRemove)
                {
                    ++summary_.player_snapshot_removes;
                }
            }
            if (!player_operation_counted
                && player_snapshot->include_local_entity
                && !slot_state.goldsrc_player_lifecycle
                    .first_player_snapshot_frame_id().has_value())
            {
                ++summary_.player_snapshot_adds;
            }
            if (pmove_enabled_
                && pmove_runtime_.movement_executed(
                    static_cast<std::size_t>(slot_state.slot)))
            {
                pmove_runtime_.RecordMovementSnapshot(
                    static_cast<std::size_t>(slot_state.slot));
                if (pmove_persistent_ && slot_state.slot == 1)
                {
                    manual_session_lifecycle_.RecordMovementSnapshot();
                }
                summary_.movement_snapshot_integration = true;
                if (hl::game_api::GoldSrcClientReplicationSummary* client =
                        FindClientReplicationSummary(slot_state.slot);
                    client != nullptr)
                {
                    ++client->movement_snapshots_sent;
                }
                SyncPmoveSummary(
                    static_cast<std::size_t>(slot_state.slot));
            }
            const auto mark_result =
                slot_state.goldsrc_player_lifecycle.MarkPlayerSnapshot(
                    built.bundle.frame.frame_id);
            if (mark_result
                    != hl::network::GoldSrcPlayerLifecycleResult::kAdvanced
                && mark_result
                    != hl::network::
                        GoldSrcPlayerLifecycleResult::kAlreadyApplied)
            {
                return FailTransport(
                    "player-snapshot-lifecycle-transition-failed",
                    "goldsrc_player_snapshot_failed: reason=lifecycle-transition");
            }
            summary_.player_snapshot_contract_verified = true;
            summary_.player_clientdata_implemented = true;
            SyncPlayerLifecycleSummary(slot_state);
        }
        if (built.bundle.kind
            == hl::network::GoldSrcSnapshotKind::kDelta)
        {
            ++summary_.continuous_delta_snapshots;
            if (built.bundle.base_frame_id.has_value())
            {
                summary_.latest_continuous_base_frame_id =
                    *built.bundle.base_frame_id;
                if (built.bundle.frame.frame_id
                    != hl::network::NextGoldSrcNetchanSequence(
                        *built.bundle.base_frame_id))
                {
                    ++summary_.continuous_delta_recoveries;
                }
            }
        }
        else
        {
            ++summary_.continuous_full_snapshots;
            if (summary_.continuous_snapshots_sent > 1)
            {
                ++summary_.continuous_full_fallbacks;
            }
        }
        summary_.continuous_snapshot_adds += static_cast<int>(
            built.bundle.entity_diff.adds);
        summary_.continuous_snapshot_updates += static_cast<int>(
            built.bundle.entity_diff.updates);
        summary_.continuous_snapshot_removes += static_cast<int>(
            built.bundle.entity_diff.removes);
        summary_.continuous_snapshot_unchanged += static_cast<int>(
            built.bundle.entity_diff.unchanged);
        summary_.continuous_frame_ids_advanced =
            summary_.latest_continuous_frame_id == 0u
            || hl::network::IsGoldSrcServerFrameNewer(
                built.bundle.frame.frame_id,
                summary_.latest_continuous_frame_id);
        summary_.latest_continuous_frame_id =
            built.bundle.frame.frame_id;
        summary_.continuous_server_time_monotonic =
            built.bundle.frame.server_time
            >= summary_.latest_continuous_server_time;
        summary_.latest_continuous_server_time =
            built.bundle.frame.server_time;
        summary_.continuous_frame_history_bounded =
            slot_state.goldsrc_first_snapshot.history().size()
            <= slot_state.goldsrc_first_snapshot.history().capacity();
        summary_.continuous_snapshot_streaming = true;
        summary_.continuous_snapshot_stable =
            slot_state.goldsrc_first_snapshot.phase()
            == hl::network::GoldSrcFirstSnapshotPhase::
                kContinuousSnapshotStable;
        SyncNetchanSummary(slot_state);
        ++continuous_snapshot_diagnostics_seen_;
        if (ShouldLogDiagnosticSample(
                continuous_snapshot_diagnostics_seen_))
        {
            hl::common::Logger::Info(
                hl::common::LogCategory::Server,
                "goldsrc_continuous_snapshot_sent: session_id="
                + slot_state.session_id
                + ",frame_id="
                + std::to_string(built.bundle.frame.frame_id)
                + ",server_time="
                + std::to_string(built.bundle.frame.server_time)
                + ",kind="
                + std::string(
                    hl::network::NameFor(built.bundle.kind))
                + ",base_frame="
                + (built.bundle.base_frame_id.has_value()
                    ? std::to_string(*built.bundle.base_frame_id)
                    : "none")
                + ",entities="
                + std::to_string(built.bundle.frame.entities.size())
                + ",adds="
                + std::to_string(built.bundle.entity_diff.adds)
                + ",updates="
                + std::to_string(built.bundle.entity_diff.updates)
                + ",removes="
                + std::to_string(built.bundle.entity_diff.removes)
                + ",unchanged="
                + std::to_string(built.bundle.entity_diff.unchanged)
                + ",history_size="
                + std::to_string(
                    slot_state.goldsrc_first_snapshot.history().size())
                + ",put_in_server="
                + (slot_state.goldsrc_player_lifecycle.put_in_server()
                    ? "1" : "0")
                + ",spawned="
                + (slot_state.goldsrc_player_lifecycle.player().player_spawned
                    ? "1" : "0")
                + ",active=0");
        }
        return true;
    }

    bool SendFirstSnapshotDatagram(DedicatedPlayerRuntimeSlot& slot_state)
    {
        if (!first_snapshot_enabled_
            || slot_state.goldsrc_first_snapshot.phase()
                != hl::network::GoldSrcFirstSnapshotPhase::
                    kFirstSnapshotPrepared
            || !slot_state.goldsrc_first_snapshot.prepared().has_value()
            || !world_baseline_bundle_.has_value()
            || !slot_state.goldsrc_delta_registry.has_value())
        {
            return FailTransport(
                "first-snapshot-send-invalid",
                "goldsrc_first_snapshot_send_failed: reason=invalid-prepared-state");
        }
        const hl::network::GoldSrcFirstSnapshotBundle& snapshot =
            *slot_state.goldsrc_first_snapshot.prepared();
        if (snapshot.frame.frame_id
            != slot_state.netchan.outgoing_sequence())
        {
            return FailTransport(
                "first-snapshot-frame-id-drift",
                "goldsrc_first_snapshot_send_failed: reason=carrier-sequence-drift");
        }

        hl::network::GoldSrcNetchanDatagram datagram;
        if (!slot_state.netchan.BuildOutgoingUnreliableDatagram(
                snapshot.payload.bytes.data(),
                snapshot.payload.size,
                &datagram))
        {
            return FailTransport(
                "first-snapshot-netchan-encode-failed",
                "goldsrc_first_snapshot_send_failed: reason=unreliable-netchan-encode");
        }
        const hl::network::GoldSrcNetchanDecodeResult carrier =
            hl::network::DecodeGoldSrcNetchanDatagram(
                hl::network::GoldSrcNetchanDirection::kServerToClient,
                datagram.bytes.data(),
                datagram.size);
        if (!carrier.ok() || carrier.packet.reliable_present
            || carrier.packet.fragment_present
            || carrier.packet.sequence != snapshot.frame.frame_id
            || carrier.packet.payload_size != snapshot.payload.size)
        {
            return FailTransport(
                "first-snapshot-carrier-self-check-failed",
                "goldsrc_first_snapshot_send_failed: reason=carrier-self-check");
        }
        const hl::network::GoldSrcSnapshotDecodeResult decoded =
            hl::network::DecodeGoldSrcFirstSnapshot(
                carrier.packet.payload.data(),
                carrier.packet.payload_size,
                carrier.packet.sequence,
                *world_baseline_bundle_,
                *slot_state.goldsrc_delta_registry);
        if (!decoded.ok()
            || decoded.frame.entities.size()
                != snapshot.frame.entities.size())
        {
            return FailTransport(
                "first-snapshot-payload-self-check-failed",
                "goldsrc_first_snapshot_send_failed: reason=payload-self-check");
        }
        if (!SendBoundedDatagram(
                datagram.bytes.data(),
                datagram.size,
                slot_state.netchan.remote_endpoint()))
        {
            return FailTransport(
                "first-snapshot-send-failed",
                "goldsrc_first_snapshot_send_failed: platform_error="
                    + socket_.LastError());
        }
        if (slot_state.goldsrc_first_snapshot.MarkSent()
            != hl::network::
                GoldSrcFirstSnapshotTransitionResult::kAdvanced)
        {
            return FailTransport(
                "first-snapshot-sent-transition-failed",
                "goldsrc_first_snapshot_send_failed: reason=history-store-or-transition");
        }
        ++summary_.first_snapshot_sent;
        summary_.next_observed_boundary =
            "client_frame_reference_required";
        SyncNetchanSummary(slot_state);
        hl::common::Logger::Info(
            hl::common::LogCategory::Server,
            "goldsrc_first_snapshot_sent: session_id="
                + slot_state.session_id
                + ",frame_id="
                + std::to_string(snapshot.frame.frame_id)
                + ",sequence="
                + std::to_string(carrier.packet.sequence)
                + ",payload_bytes="
                + std::to_string(snapshot.payload.size)
                + ",messages=svc_time,svc_clientdata,svc_packetentities"
                + ",entities="
                + std::to_string(snapshot.frame.entities.size())
                + ",reliable=0,fragment=0"
                + ",history_depth="
                + std::to_string(
                    slot_state.goldsrc_first_snapshot.history().capacity())
                + ",history_size="
                + std::to_string(
                    slot_state.goldsrc_first_snapshot.history().size())
                + ",signon_phase=" + summary_.signon_phase
                + ",put_in_server=0,spawned=0,active=0");
        return true;
    }

    bool SendDueSnapshotsFairly()
    {
        auto& slots = state_.dedicated_multiplayer_foundation.slots;
        if (!continuous_snapshots_enabled_ || slots.empty())
        {
            return true;
        }
        const std::size_t count = slots.size();
        const std::size_t start = next_snapshot_scheduler_index_ % count;
        std::vector<bool> active(count, false);
        std::vector<bool> due_snapshot(count, false);
        std::vector<bool> snapshot_sent(count, false);
        std::vector<hl::network::GoldSrcOutgoingClientDemand> demands(count);
        std::size_t due_count = 0u;
        for (std::size_t index = 0u; index < count; ++index)
        {
            DedicatedPlayerRuntimeSlot& slot_state = slots[index];
            active[index] = IsDedicatedSlotConnectedOccupant(slot_state)
                && slot_state.netchan.initialized();
            demands[index].active = active[index];
            if (!active[index])
            {
                continue;
            }
            demands[index].incoming_frontier =
                slot_state.netchan.incoming_sequence();
            const hl::network::GoldSrcSnapshotDueResult due =
                slot_state.goldsrc_first_snapshot.CheckContinuousDue(
                    network_server_time_);
            if (due == hl::network::GoldSrcSnapshotDueResult::kDue)
            {
                due_snapshot[index] = true;
                demands[index].snapshot_due = true;
                ++due_count;
                ++slot_state.goldsrc_snapshots_due;
            }
            else if (due
                    == hl::network::GoldSrcSnapshotDueResult::kInvalidServerTime
                || due
                    == hl::network::GoldSrcSnapshotDueResult::
                        kNonMonotonicServerTime)
            {
                return FailTransport(
                    "continuous-snapshot-scheduler-invalid",
                    "goldsrc_continuous_snapshot_failed: reason="
                        + std::string(hl::network::ReasonFor(due)));
            }
            const double ack_age = network_server_time_
                - slot_state.goldsrc_pending_empty_ack_since_server_time;
            demands[index].empty_acknowledgement_pending =
                slot_state.goldsrc_pending_empty_ack
                && std::isfinite(ack_age)
                && ack_age >= 0.100;
        }

        summary_.snapshot_capacity_reserved = true;
        summary_.reliable_priority_preserved = true;
        summary_.per_client_snapshot_fairness = true;
        if (due_count > 0u
            && datagrams_sent_this_frame_
                >= kMaximumDatagramsSentPerHostFrame)
        {
            ++summary_.frames_with_all_send_budget_used_before_snapshot;
        }
        const std::size_t remaining_budget =
            datagrams_sent_this_frame_ < kMaximumDatagramsSentPerHostFrame
                ? kMaximumDatagramsSentPerHostFrame
                    - datagrams_sent_this_frame_
                : 0u;
        const hl::network::GoldSrcOutgoingScheduleResult schedule =
            hl::network::BuildGoldSrcBoundedOutgoingSchedule(
                demands,
                remaining_budget,
                start);
        for (const hl::network::GoldSrcOutgoingAction& action :
             schedule.actions)
        {
            DedicatedPlayerRuntimeSlot& slot_state =
                slots[action.client_index];
            if (action.kind
                == hl::network::GoldSrcOutgoingActionKind::kSnapshot)
            {
                if (!SendContinuousSnapshotDatagram(slot_state))
                {
                    return false;
                }
                snapshot_sent[action.client_index] = true;
                slot_state.goldsrc_consecutive_snapshot_starvation_frames = 0u;
            }
            else if (action.kind
                == hl::network::GoldSrcOutgoingActionKind::
                    kEmptyAcknowledgement)
            {
                if (!SendNetchanDatagram(
                        slot_state,
                        false,
                        "goldsrc_coalesced_empty_ack_sent"))
                {
                    return false;
                }
            }
        }

        bool frame_starved = false;
        for (std::size_t index = 0u; index < count; ++index)
        {
            DedicatedPlayerRuntimeSlot& slot_state = slots[index];
            if (!active[index] || !due_snapshot[index]
                || snapshot_sent[index])
            {
                continue;
            }
            frame_starved = true;
            ++slot_state.goldsrc_snapshots_deferred;
            ++slot_state.goldsrc_snapshots_starved;
            ++slot_state.goldsrc_consecutive_snapshot_starvation_frames;
            slot_state.goldsrc_maximum_consecutive_snapshot_starvation_frames =
                (std::max)(
                    slot_state
                        .goldsrc_maximum_consecutive_snapshot_starvation_frames,
                    slot_state
                        .goldsrc_consecutive_snapshot_starvation_frames);
            ++summary_.snapshot_budget_starvations;
            ++snapshot_starved_diagnostics_seen_;
            if (ShouldLogDiagnosticSample(
                    snapshot_starved_diagnostics_seen_))
            {
                hl::common::Logger::Warn(
                    hl::common::LogCategory::Server,
                    "goldsrc_snapshot_starved: slot="
                        + std::to_string(slot_state.slot)
                        + ",consecutive_frames="
                        + std::to_string(
                            slot_state
                                .goldsrc_consecutive_snapshot_starvation_frames));
            }
        }
        if (frame_starved)
        {
            ++summary_.consecutive_snapshot_starvation_frames;
            summary_.maximum_consecutive_snapshot_starvation_frames =
                (std::max)(
                    summary_.maximum_consecutive_snapshot_starvation_frames,
                    summary_.consecutive_snapshot_starvation_frames);
        }
        else
        {
            summary_.consecutive_snapshot_starvation_frames = 0u;
        }

        for (std::size_t index = 0u; index < count; ++index)
        {
            DedicatedPlayerRuntimeSlot& slot_state = slots[index];
            if (!active[index] || !slot_state.goldsrc_pending_empty_ack
                || !demands[index].empty_acknowledgement_pending)
            {
                continue;
            }
            const bool carrier_sent = std::any_of(
                schedule.actions.begin(),
                schedule.actions.end(),
                [index](const hl::network::GoldSrcOutgoingAction& action)
                {
                    return action.client_index == index;
                });
            if (!carrier_sent)
            {
                ++slot_state.goldsrc_ordinary_ack_datagrams_deferred;
                ++summary_.ordinary_ack_datagrams_deferred;
            }
        }
        next_snapshot_scheduler_index_ = schedule.next_fair_client_index;
        return true;
    }

    bool SendInitialNetchanDatagram(
        DedicatedPlayerRuntimeSlot& slot_state)
    {
        if (!slot_state.netchan.initialized())
        {
            return FailTransport(
                "netchan-session-missing",
                "goldsrc_netchan_send_failed: reason=session-missing");
        }

        summary_.netchan_initialized = true;
        summary_.netchan_state = std::string(
            hl::network::NameFor(slot_state.netchan.transport_phase()));
        hl::common::Logger::Info(
            hl::common::LogCategory::Server,
            "goldsrc_netchan_initialized: session_id=" + slot_state.session_id
                + ",slot=" + std::to_string(slot_state.slot)
                + ",endpoint=" + slot_state.netchan.remote_endpoint().ToString()
                + ",outgoing_sequence=1,incoming_sequence=0,qport_wire=absent"
                + ",channel_id="
                + std::to_string(slot_state.netchan.channel_identifier()));
        hl::common::Logger::Info(
                hl::common::LogCategory::Server,
                "goldsrc_netchan_reliable_queued: session_id="
                + slot_state.session_id
                + ",payload=svc_nop,payload_bytes=1,reliable_toggle=1");

        if (!SendNetchanDatagram(
                slot_state,
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

    bool SendPendingOrdinaryAdvances()
    {
        for (DedicatedPlayerRuntimeSlot& slot_state :
             state_.dedicated_multiplayer_foundation.slots)
        {
            if (!slot_state.goldsrc_negative_advance_pending)
            {
                continue;
            }
            if (datagrams_sent_this_frame_
                >= kMaximumDatagramsSentPerHostFrame)
            {
                return true;
            }
            slot_state.goldsrc_negative_advance_pending = false;
            if (!IsDedicatedSlotConnectedOccupant(slot_state)
                || !slot_state.netchan.initialized()
                || !slot_state.netchan.reliable_pending())
            {
                return FailTransport(
                    "netchan-negative-advance-invalid",
                    "goldsrc_netchan_send_failed: reason=negative-advance-invalid");
            }
            if (!SendNetchanDatagram(
                    slot_state,
                    false,
                    "goldsrc_netchan_ordinary_sent"))
            {
                return false;
            }
        }
        return true;
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

        const bool established_streaming = continuous_snapshots_enabled_
            && slot_state->goldsrc_player_lifecycle.put_in_server()
            && slot_state->goldsrc_player_lifecycle.signon_complete();
        if (established_streaming)
        {
            if (slot_state->goldsrc_pending_empty_ack)
            {
                ++slot_state->goldsrc_ordinary_ack_datagrams_coalesced;
                ++summary_.ordinary_ack_datagrams_coalesced;
                ++empty_ack_coalesced_diagnostics_seen_;
                if (ShouldLogDiagnosticSample(
                        empty_ack_coalesced_diagnostics_seen_))
                {
                    hl::common::Logger::Info(
                        hl::common::LogCategory::Server,
                        "goldsrc_empty_ack_coalesced: slot="
                            + std::to_string(slot_state->slot)
                            + ",pending_packets="
                            + std::to_string(
                                slot_state->goldsrc_pending_empty_ack_packets
                                    + 1u));
                }
            }
            else
            {
                slot_state->goldsrc_pending_empty_ack = true;
                slot_state->goldsrc_pending_empty_ack_since_server_time =
                    network_server_time_;
                slot_state->goldsrc_pending_empty_ack_packets = 0u;
            }
            ++slot_state->goldsrc_pending_empty_ack_packets;
            summary_.empty_ack_coalescing = true;
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
        if (world_baselines_enabled_
            && process_outcome.acknowledged_reliable_kind
                == hl::network::GoldSrcNetchanReliablePayloadKind::
                    kBaselineBootstrap)
        {
            if (slot_state->goldsrc_signon
                    .MarkBaselineBootstrapAcknowledged()
                    != hl::network::GoldSrcSignonTransitionResult::
                        kAdvanced
                || slot_state->goldsrc_signon
                    .EnterAwaitingFirstSnapshot()
                    != hl::network::GoldSrcSignonTransitionResult::
                        kAdvanced)
            {
                return FailTransport(
                    "baseline-ack-transition-failed",
                    "goldsrc_world_baseline_ack_failed: reason=invalid-signon-phase");
            }
            if (first_snapshot_enabled_
                && slot_state->goldsrc_first_snapshot.EnterAwaiting()
                    != hl::network::
                        GoldSrcFirstSnapshotTransitionResult::kAdvanced)
            {
                return FailTransport(
                    "first-snapshot-await-transition-failed",
                    "goldsrc_first_snapshot_failed: reason=invalid-await-phase");
            }
            ++summary_.baseline_acked;
            summary_.baseline_fragment_acknowledged_count =
                summary_.baseline_fragmented
                ? summary_.baseline_fragment_count
                : 0;
            SyncNetchanSummary(*slot_state);
            hl::common::Logger::Info(
                hl::common::LogCategory::Server,
                "goldsrc_world_baseline_acknowledged: session_id="
                    + slot_state->session_id
                    + ",ack="
                    + std::to_string(decoded.packet.acknowledgement)
                    + ",signon_phase=" + summary_.signon_phase
                    + ",put_in_server=0,spawned=0,active=0");
        }

        const bool movement_streaming =
            slot_state->goldsrc_player_lifecycle.put_in_server()
            && slot_state->goldsrc_player_lifecycle.signon_complete();
        if (!movement_streaming
            || ShouldLogDiagnosticSample(
                ++steady_netchan_receive_diagnostics_seen_))
        {
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
        }
        if (process_outcome.incoming_fragment_consumed)
        {
            hl::common::Logger::Info(
                hl::common::LogCategory::Server,
                "goldsrc_client_fragment_consumed: session_id="
                    + slot_state->session_id
                    + ",sequence="
                    + std::to_string(decoded.packet.sequence)
                    + ",fragment_index="
                    + std::to_string(
                        decoded.packet.fragment_metadata.descriptors[0]
                            .fragment_index)
                    + ",fragment_count="
                    + std::to_string(
                        decoded.packet.fragment_metadata.descriptors[0]
                            .fragment_count)
                    + ",transfer_completed="
                    + (process_outcome.incoming_fragment_transfer_completed
                            ? "true"
                            : "false")
                    + ",reassembled_payload_bytes="
                    + std::to_string(
                        process_outcome.incoming_fragment_payload_size));
        }

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
                    MarkComplete("netchan_only_complete");
                }
                return true;
            }
        }

        hl::network::GoldSrcNetchanReliablePayloadKind queued_kind =
            hl::network::GoldSrcNetchanReliablePayloadKind::kNone;
        if (serverinfo_enabled_
            && slot_state->netchan.transport_phase()
                == hl::network::GoldSrcNetchanTransportPhase::kEstablished
            && !process_outcome.incoming_fragment_consumed)
        {
            if (!HandleClientSignonPayload(
                    *slot_state,
                    decoded.packet,
                    &queued_kind))
            {
                return false;
            }
            if (!IsDedicatedSlotConnectedOccupant(*slot_state))
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
                                        kBaselineBootstrap
                        ? "goldsrc_world_baseline_sent"
                        : queued_kind
                                == hl::network::
                                    GoldSrcNetchanReliablePayloadKind::
                                        kServerInfo
                        ? "goldsrc_serverinfo_sent"
                        : "goldsrc_resource_manifest_sent");
            }
        }

        if (first_snapshot_enabled_
            && slot_state->goldsrc_first_snapshot.phase()
                == hl::network::GoldSrcFirstSnapshotPhase::
                    kFirstSnapshotPrepared)
        {
            return SendFirstSnapshotDatagram(*slot_state);
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
            const bool continuous_movement_streaming =
                continuous_snapshots_enabled_
                && slot_state->goldsrc_player_lifecycle.put_in_server()
                && slot_state->goldsrc_player_lifecycle.signon_complete();
            if (continuous_movement_streaming)
            {
                // The due snapshot is the stock-style server response for an
                // established movement stream. Sending an extra empty ack for
                // every clc_move would advance the server sequence much faster
                // than snapshot history and make low-8 delta references
                // ambiguous after wrap.
                return true;
            }
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
                if (world_baselines_enabled_)
                {
                    // The stock client may acknowledge the resource bundle
                    // with a bounded reliable fragment before its first move.
                    // Keep pumping until clc_move queues the baseline bundle.
                }
                else if (delta_descriptions_negative_proof_
                    && summary_.clc_move_rejected > 0)
                {
                    // A post-resource negative proof intentionally remains
                    // at this phase until one fully valid move envelope
                    // establishes the next bounded checkpoint.
                }
                else if (summary_.max_clients == 1
                    && !resource_manifest_negative_proof_
                    && !delta_descriptions_negative_proof_)
                {
                    MarkComplete("resource_manifest_complete");
                }
                else if (summary_.max_clients == 1
                    && ((!resource_manifest_negative_proof_
                            || ResourceManifestNegativeEvidenceSatisfied())
                    && (!delta_descriptions_negative_proof_
                            || DeltaDescriptionsNegativeEvidenceSatisfied())))
                {
                    summary_.server_still_responsive = true;
                    MarkComplete("resource_manifest_negative_complete");
                }
            }
            else if (slot_state->goldsrc_signon.phase()
                    == hl::network::GoldSrcSignonPhase::
                        kAwaitingServerBaselineOrSnapshot
                && summary_.clc_move_delivered >= 1)
            {
                if (summary_.max_clients == 1
                    && (!delta_descriptions_negative_proof_
                        || DeltaDescriptionsNegativeEvidenceSatisfied()))
                {
                    summary_.server_still_responsive = true;
                    MarkComplete("post_resource_move_complete");
                }
            }
            else if (summary_.max_clients == 1
                && resource_manifest_negative_proof_
                && summary_.requires_fragmentation_reported
                && summary_.oversized_manifest_not_truncated
                && summary_.resource_manifest_cached_outcome_reuses >= 1)
            {
                summary_.server_still_responsive = true;
                MarkComplete("resource_manifest_fragment_complete");
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
                if (summary_.max_clients == 1
                    && !serverinfo_negative_proof_
                    && !delta_descriptions_negative_proof_)
                {
                    MarkComplete("serverinfo_complete");
                }
                else if (summary_.max_clients == 1
                    && ((!serverinfo_negative_proof_
                            || ServerInfoNegativeEvidenceSatisfied())
                    && (!delta_descriptions_negative_proof_
                            || DeltaDescriptionsNegativeEvidenceSatisfied())))
                {
                    summary_.server_still_responsive = true;
                    MarkComplete("serverinfo_negative_complete");
                }
            }
            return true;
        }

        if (summary_.max_clients == 1
            && negative_proof_
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
            MarkComplete("netchan_negative_complete");
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
        hl::game_api::GoldSrcClientReplicationSummary* const client_summary =
            FindClientReplicationSummary(slot_state->slot);
        if (client_summary != nullptr)
        {
            const bool reconnect = client_summary->admission_count > 0;
            ++client_summary->admission_count;
            if (reconnect)
            {
                ++client_summary->reconnect_count;
                client_summary->slot_reuse_clean =
                    slot_state->goldsrc_first_snapshot.history().size() == 0u
                    && !slot_state->goldsrc_first_snapshot
                        .prepared().has_value()
                    && !slot_state->goldsrc_last_counted_acknowledged_frame
                        .has_value()
                    && slot_state->goldsrc_pmove_calls == 0u;
            }
        }
        if (pmove_persistent_ && slot_state->slot == 1)
        {
            const bool reconnected =
                manual_session_lifecycle_.MarkClientAdmitted();
            SyncManualSessionSummary();
            if (reconnected)
            {
                hl::common::Logger::Info(
                    hl::common::LogCategory::Server,
                    "goldsrc_client_reconnected: mode=persistent"
                    ",phase=handshake_in_progress");
            }
        }
        summary_.remote_endpoint = slot_state->remote_endpoint;
        summary_.final_state =
            DedicatedPlayerLifecycleStateName(slot_state->lifecycle_state);
        summary_.connected = slot_state->connected;
        summary_.put_in_server = slot_state->put_in_server;
        summary_.spawned = slot_state->spawn_count > 0 || slot_state->alive;
        summary_.active = slot_state->put_in_server && slot_state->alive;
        summary_.last_reject_reason = "<none>";
        const std::size_t reset_index = static_cast<std::size_t>(
            slot_state->slot - 1);
        if (reset_index < awaiting_fresh_session_after_delta_reset_.size()
            && awaiting_fresh_session_after_delta_reset_[reset_index])
        {
            ++summary_.delta_fresh_sessions_after_reset;
            summary_.delta_slot_reused_after_reset = true;
            awaiting_fresh_session_after_delta_reset_[reset_index] = false;
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
        if (world_baselines_enabled_)
        {
            ResetWorldBaselineSessionSummary();
        }
        if (first_snapshot_enabled_)
        {
            ResetSnapshotSessionSummary();
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
        SyncClientReplicationSummary(*slot_state);
        RefreshTwoClientReplicationSummary();

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
    bool world_baselines_enabled_ = false;
    bool world_baselines_negative_proof_ = false;
    bool first_snapshot_enabled_ = false;
    bool first_snapshot_negative_proof_ = false;
    bool continuous_snapshots_enabled_ = false;
    bool continuous_snapshots_negative_proof_ = false;
    bool continuous_negative_unknown_observed_ = false;
    bool continuous_negative_stale_after_unknown_observed_ = false;
    bool player_lifecycle_enabled_ = false;
    bool player_lifecycle_negative_proof_ = false;
    bool pmove_enabled_ = false;
    bool pmove_negative_proof_ = false;
    bool pmove_persistent_ = false;
    bool combat_enabled_ = false;
    bool fall_damage_proof_ = false;
    double pmove_observation_seconds_ = 2.0;
    std::optional<std::filesystem::path> manual_shutdown_file_;
    std::optional<std::filesystem::path> manual_disconnect_file_;
    std::optional<std::filesystem::path> stock_test_control_file_;
    hl::network::GoldSrcManualSessionLifecycle manual_session_lifecycle_;
    hl::game_api::detail::GoldSrcPmoveRuntime pmove_runtime_;
    hl::network::GoldSrcCombatFrameCadence combat_frame_cadence_;
    std::uint64_t combat_start_frame_calls_ = 0u;
    std::uint64_t combat_entity_think_calls_ = 0u;
    hl::network::GoldSrcClientEdictRegistry lifecycle_edict_registry_;
    double snapshot_rate_hz_ =
        hl::network::kGoldSrcDefaultSnapshotRateHz;
    std::optional<hl::network::GoldSrcBaselineBundle>
        world_baseline_bundle_;
    std::optional<hl::network::GoldSrcBaselinePayload>
        world_baseline_payload_;
    bool dedicated_runtime_ = false;
    hl::network::UdpSocket socket_;
    std::unique_ptr<hl::network::ChallengeTable> challenges_;
    std::unique_ptr<hl::network::HandshakeProcessor> processor_;
    std::array<std::uint8_t, hl::network::kGoldSrcMaximumDatagramBytes> buffer_{};
    std::chrono::steady_clock::time_point deadline_{};
    std::chrono::steady_clock::time_point last_network_host_frame_time_{};
    std::chrono::steady_clock::time_point next_manual_shutdown_probe_time_{};
    std::chrono::steady_clock::time_point next_manual_disconnect_probe_time_{};
    std::chrono::steady_clock::time_point next_stock_test_control_probe_time_{};
    bool network_server_clock_started_ = false;
    bool manual_disconnect_consumed_ = false;
    bool stock_test_control_consumed_ = false;
    StockTestFallAcceptance stock_test_fall_acceptance_{};
    double network_server_time_ = 0.0;
    std::uint64_t challenge_diagnostics_seen_ = 0;
    std::uint64_t rejection_diagnostics_seen_ = 0;
    std::uint64_t malformed_diagnostics_seen_ = 0;
    std::uint64_t expired_diagnostics_seen_ = 0;
    std::uint64_t unknown_endpoint_diagnostics_seen_ = 0;
    std::uint64_t invalid_netchan_diagnostics_seen_ = 0;
    std::uint64_t frame_reference_diagnostics_seen_ = 0;
    std::uint64_t first_snapshot_keepalive_diagnostics_seen_ = 0;
    std::uint64_t steady_netchan_receive_diagnostics_seen_ = 0;
    std::uint64_t steady_netchan_send_diagnostics_seen_ = 0;
    std::uint64_t pmove_rejection_diagnostics_seen_ = 0;
    std::uint64_t pmove_clock_rejection_diagnostics_seen_ = 0;
    std::uint64_t pmove_clock_recovery_diagnostics_seen_ = 0;
    std::uint64_t pmove_clock_resync_diagnostics_seen_ = 0;
    std::uint64_t pmove_execution_gap_diagnostics_seen_ = 0;
    std::uint64_t snapshot_cadence_gap_diagnostics_seen_ = 0;
    std::uint64_t prediction_state_diagnostics_seen_ = 0;
    std::uint64_t continuous_snapshot_diagnostics_seen_ = 0;
    std::uint64_t empty_ack_coalesced_diagnostics_seen_ = 0;
    std::uint64_t snapshot_starved_diagnostics_seen_ = 0;
    std::uint64_t movement_stalled_diagnostics_seen_ = 0;
    std::uint64_t frame_ack_stalled_diagnostics_seen_ = 0;
    std::uint64_t next_network_session_ = 1;
    int datagrams_sent_this_frame_ = 0;
    std::size_t next_snapshot_scheduler_index_ = 0u;
    bool deadline_started_ = false;
    bool started_ = false;
    bool complete_ = false;
    std::string completion_reason_ = "pending";
    bool transport_failed_ = false;
    bool accepted_ = false;
    bool shutdown_ = false;
    bool manual_session_shutdown_logged_ = false;
    std::vector<bool> awaiting_fresh_session_after_delta_reset_;
    std::vector<StockTestSpawnAnchor> stock_test_spawn_anchors_;
    std::vector<std::vector<bool>> stock_test_reset_transitions_;
    std::vector<std::vector<bool>> stock_test_nointerp_clear_pending_;
};
