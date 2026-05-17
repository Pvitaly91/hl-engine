#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "common/logger.h"

namespace hl::app
{
enum class RuntimeMode
{
    kListen,
    kDedicated,
};

enum class RegressionGuardProfile
{
    kTrainstop26TerminalProbe,
    kTrainstop26Baseline,
    kChangelevelLatchOnlyContinuation,
    kChangelevelRequestConsumed,
};

struct LaunchOptions
{
    RuntimeMode runtime_mode = RuntimeMode::kListen;
    std::optional<std::filesystem::path> game_directory;
    std::optional<std::wstring> map_name;
    std::optional<RegressionGuardProfile> regression_guard;
    std::optional<std::string> run_label;
    std::optional<std::string> prompt_id;
    int maxclients = 1;
    int deathmatch = 0;
    int coop = 0;
    int synthetic_players = 0;
    bool query_surface_enabled = false;
    bool query_probe_enabled = false;
    int query_port = 0;
    bool connect_surface_enabled = false;
    bool connect_probe_enabled = false;
    std::string connect_probe_scenario = "accept";
    bool hlds_getchallenge_diagnostic_surface_enabled = false;
    bool hlds_getchallenge_diagnostic_probe_enabled = false;
    std::string hlds_getchallenge_diagnostic_probe_scenario = "happy";
    bool hlds_connect_diagnostic_surface_enabled = false;
    bool hlds_connect_diagnostic_probe_enabled = false;
    std::string hlds_connect_diagnostic_probe_scenario = "happy";
    bool hlds_serverinfo_diagnostic_surface_enabled = false;
    bool hlds_serverinfo_diagnostic_probe_enabled = false;
    std::string hlds_serverinfo_diagnostic_probe_scenario = "happy";
    bool hlds_connectionless_loopback_udp_diagnostic_surface_enabled = false;
    bool hlds_connectionless_loopback_udp_diagnostic_probe_enabled = false;
    std::string hlds_connectionless_loopback_udp_diagnostic_probe_scenario = "happy";
    bool hlds_address_scoped_challenge_cache_diagnostic_surface_enabled = false;
    bool hlds_address_scoped_challenge_cache_diagnostic_probe_enabled = false;
    std::string hlds_address_scoped_challenge_cache_diagnostic_probe_scenario = "happy";
    bool hlds_userinfo_validation_policy_diagnostic_surface_enabled = false;
    bool hlds_userinfo_validation_policy_diagnostic_probe_enabled = false;
    std::string hlds_userinfo_validation_policy_diagnostic_probe_scenario = "happy";
    bool hlds_connectionless_diagnostic_lifecycle_acceptance_gate_enabled = false;
    bool hlds_connectionless_diagnostic_lifecycle_acceptance_probe_enabled = false;
    std::string hlds_connectionless_diagnostic_lifecycle_acceptance_probe_scenario = "happy";
    bool hlds_production_loopback_connectionless_socket_pump_diagnostic_surface_enabled = false;
    bool hlds_production_loopback_connectionless_socket_pump_diagnostic_probe_enabled = false;
    std::string hlds_production_loopback_connectionless_socket_pump_diagnostic_probe_scenario =
        "happy";
    bool hlds_production_loopback_connectionless_socket_pump_lifecycle_registration_stub_enabled =
        false;
    bool hlds_production_loopback_connectionless_socket_pump_lifecycle_registration_probe_enabled =
        false;
    std::string
        hlds_production_loopback_connectionless_socket_pump_lifecycle_registration_probe_scenario =
            "happy";
    bool hlds_production_loopback_connectionless_socket_pump_diagnostic_frame_wiring_enabled =
        false;
    bool hlds_production_loopback_connectionless_socket_pump_diagnostic_frame_wiring_probe_enabled =
        false;
    std::string
        hlds_production_loopback_connectionless_socket_pump_diagnostic_frame_wiring_probe_scenario =
            "happy";
    bool hlds_production_loopback_connectionless_socket_pump_localhost_client_smoke_harness_enabled =
        false;
    bool
        hlds_production_loopback_connectionless_socket_pump_localhost_client_smoke_harness_probe_enabled =
            false;
    std::string
        hlds_production_loopback_connectionless_socket_pump_localhost_client_smoke_harness_probe_scenario =
            "happy";
    bool hlds_serverinfo_fixture_contract_validator_diagnostic_probe_enabled = false;
    std::string hlds_serverinfo_fixture_contract_validator_diagnostic_probe_scenario =
        "happy";
    bool hlds_serverinfo_contract_backed_diagnostic_builder_parser_probe_enabled =
        false;
    std::string
        hlds_serverinfo_contract_backed_diagnostic_builder_parser_probe_scenario =
            "happy";
    bool hlds_serverinfo_unresolved_fixture_evidence_gap_guard_probe_enabled =
        false;
    std::string
        hlds_serverinfo_unresolved_fixture_evidence_gap_guard_probe_scenario =
            "happy";
    bool hlds_connectionless_query_info_byte_level_builder_parser_probe_enabled =
        false;
    std::string
        hlds_connectionless_query_info_byte_level_builder_parser_probe_scenario =
            "happy";
    bool
        hlds_connectionless_query_info_byte_level_diagnostic_path_integration_probe_enabled =
            false;
    std::string
        hlds_connectionless_query_info_byte_level_diagnostic_path_integration_probe_scenario =
            "happy";
    bool
        hlds_connectionless_query_info_byte_level_loopback_query_response_swap_probe_enabled =
            false;
    std::string
        hlds_connectionless_query_info_byte_level_loopback_query_response_swap_probe_scenario =
            "happy";
    bool hlds_query_info_byte_level_loopback_query_client_smoke_probe_enabled =
        false;
    std::string
        hlds_query_info_byte_level_loopback_query_client_smoke_probe_scenario =
            "happy";
    bool hlds_query_info_loopback_regression_acceptance_probe_enabled = false;
    std::string hlds_query_info_loopback_regression_acceptance_probe_scenario =
        "happy";
    bool hlds_query_info_regression_ci_manifest_drift_gate_probe_enabled =
        false;
    std::string
        hlds_query_info_regression_ci_manifest_drift_gate_probe_scenario =
            "happy";
    bool hlds_qport_session_offline_fixture_validator_probe_enabled = false;
    std::string hlds_qport_session_offline_fixture_validator_probe_scenario =
        "happy";
    bool hlds_qport_session_no_client_capture_policy_gate_probe_enabled =
        false;
    std::string
        hlds_qport_session_no_client_capture_policy_gate_probe_scenario =
            "happy";
    bool
        hlds_qport_session_capture_preflight_dry_run_validator_probe_enabled =
            false;
    std::string
        hlds_qport_session_capture_preflight_dry_run_validator_probe_scenario =
            "happy";
    bool hlds_qport_session_no_client_capture_shell_probe_enabled = false;
    std::string
        hlds_qport_session_no_client_capture_shell_probe_scenario =
            "happy";
    bool hlds_qport_session_capture_shell_ci_drift_gate_probe_enabled =
        false;
    std::string
        hlds_qport_session_capture_shell_ci_drift_gate_probe_scenario =
            "happy";
    bool hlds_qport_session_capture_final_policy_gate_probe_enabled =
        false;
    std::string
        hlds_qport_session_capture_final_policy_gate_probe_scenario =
            "happy";
    bool
        hlds_qport_session_no_client_capture_runtime_skeleton_probe_enabled =
            false;
    std::string
        hlds_qport_session_no_client_capture_runtime_skeleton_probe_scenario =
            "happy";
    bool hlds_qport_session_runtime_skeleton_ci_drift_gate_probe_enabled =
        false;
    std::string
        hlds_qport_session_runtime_skeleton_ci_drift_gate_probe_scenario =
            "happy";
    bool
        hlds_qport_session_capture_execution_final_policy_gate_probe_enabled =
            false;
    std::string
        hlds_qport_session_capture_execution_final_policy_gate_probe_scenario =
            "happy";
    bool hlds_qport_session_no_client_capture_execution_skeleton_probe_enabled =
        false;
    std::string
        hlds_qport_session_no_client_capture_execution_skeleton_probe_scenario =
            "happy";
    bool hlds_qport_session_execution_skeleton_ci_drift_gate_probe_enabled =
        false;
    std::string
        hlds_qport_session_execution_skeleton_ci_drift_gate_probe_scenario =
            "happy";
    bool
        hlds_qport_session_no_client_capture_execution_implementation_skeleton_probe_enabled =
            false;
    std::string
        hlds_qport_session_no_client_capture_execution_implementation_skeleton_probe_scenario =
            "happy";
    bool
        hlds_qport_session_execution_implementation_skeleton_ci_drift_gate_probe_enabled =
            false;
    std::string
        hlds_qport_session_execution_implementation_skeleton_ci_drift_gate_probe_scenario =
            "happy";
    bool
        hlds_qport_session_capture_execution_implementation_final_gate_probe_enabled =
            false;
    std::string
        hlds_qport_session_capture_execution_implementation_final_gate_probe_scenario =
            "happy";
    bool
        hlds_qport_session_no_client_capture_execution_implementation_probe_enabled =
            false;
    std::string
        hlds_qport_session_no_client_capture_execution_implementation_probe_scenario =
            "happy";
    bool hlds_serverinfo_contract_backed_diagnostic_path_integration_probe_enabled =
        false;
    std::string
        hlds_serverinfo_contract_backed_diagnostic_path_integration_probe_scenario =
            "happy";
    bool hlds_serverinfo_contract_backed_diagnostic_localhost_smoke_swap_probe_enabled =
        false;
    std::string
        hlds_serverinfo_contract_backed_diagnostic_localhost_smoke_swap_probe_scenario =
            "happy";
    bool activation_surface_enabled = false;
    bool activation_probe_enabled = false;
    std::string activation_probe_scenario = "happy";
    bool bootstrap_surface_enabled = false;
    bool bootstrap_probe_enabled = false;
    std::string bootstrap_probe_scenario = "happy";
    bool bootstrap_sequence_surface_enabled = false;
    bool bootstrap_sequence_probe_enabled = false;
    std::string bootstrap_sequence_probe_scenario = "happy";
    bool signon_catalog_surface_enabled = false;
    bool signon_catalog_probe_enabled = false;
    std::string signon_catalog_probe_scenario = "happy";
    bool signon_template_surface_enabled = false;
    bool signon_template_probe_enabled = false;
    std::string signon_template_probe_scenario = "happy";
    bool signon_template_completion_surface_enabled = false;
    bool signon_template_completion_probe_enabled = false;
    std::string signon_template_completion_probe_scenario = "happy";
    bool signon_envelope_surface_enabled = false;
    bool signon_envelope_probe_enabled = false;
    std::string signon_envelope_probe_scenario = "happy";
    bool signon_batch_surface_enabled = false;
    bool signon_batch_probe_enabled = false;
    std::string signon_batch_probe_scenario = "happy";
    bool signon_wiremap_surface_enabled = false;
    bool signon_wiremap_probe_enabled = false;
    std::string signon_wiremap_probe_scenario = "happy";
    bool signon_burst_surface_enabled = false;
    bool signon_burst_probe_enabled = false;
    std::string signon_burst_probe_scenario = "happy";
    bool signon_stream_surface_enabled = false;
    bool signon_stream_probe_enabled = false;
    std::string signon_stream_probe_scenario = "happy";
    bool signon_stream_window_surface_enabled = false;
    bool signon_stream_window_probe_enabled = false;
    std::string signon_stream_window_probe_scenario = "happy";
    bool signon_message_catalog_surface_enabled = false;
    bool signon_message_catalog_probe_enabled = false;
    std::string signon_message_catalog_probe_scenario = "happy";
    bool signon_message_fetch_surface_enabled = false;
    bool signon_message_fetch_probe_enabled = false;
    std::string signon_message_fetch_probe_scenario = "happy";
    bool signon_multi_message_fetch_surface_enabled = false;
    bool signon_multi_message_fetch_probe_enabled = false;
    std::string signon_multi_message_fetch_probe_scenario = "happy";
    bool signon_message_range_fetch_surface_enabled = false;
    bool signon_message_range_fetch_probe_enabled = false;
    std::string signon_message_range_fetch_probe_scenario = "happy";
    bool signon_message_cursor_surface_enabled = false;
    bool signon_message_cursor_probe_enabled = false;
    std::string signon_message_cursor_probe_scenario = "happy";
    bool signon_message_cursor_advance_surface_enabled = false;
    bool signon_message_cursor_advance_probe_enabled = false;
    std::string signon_message_cursor_advance_probe_scenario = "happy";
    bool signon_message_cursor_eof_surface_enabled = false;
    bool signon_message_cursor_eof_probe_enabled = false;
    std::string signon_message_cursor_eof_probe_scenario = "happy";
    bool signon_message_cursor_resume_denial_surface_enabled = false;
    bool signon_message_cursor_resume_denial_probe_enabled = false;
    std::string signon_message_cursor_resume_denial_probe_scenario = "happy";
    bool signon_message_cursor_resume_allow_surface_enabled = false;
    bool signon_message_cursor_resume_allow_probe_enabled = false;
    std::string signon_message_cursor_resume_allow_probe_scenario = "happy";
    bool signon_message_cursor_carryover_surface_enabled = false;
    bool signon_message_cursor_carryover_probe_enabled = false;
    std::string signon_message_cursor_carryover_probe_scenario = "happy";
    bool signon_message_cursor_carried_range_surface_enabled = false;
    bool signon_message_cursor_carried_range_probe_enabled = false;
    std::string signon_message_cursor_carried_range_probe_scenario = "happy";
    bool signon_message_cursor_carried_eof_surface_enabled = false;
    bool signon_message_cursor_carried_eof_probe_enabled = false;
    std::string signon_message_cursor_carried_eof_probe_scenario = "happy";
    bool signon_message_cursor_carried_resume_allow_surface_enabled = false;
    bool signon_message_cursor_carried_resume_allow_probe_enabled = false;
    std::string signon_message_cursor_carried_resume_allow_probe_scenario = "happy";
    bool signon_message_cursor_carried_checkpoint_surface_enabled = false;
    bool signon_message_cursor_carried_checkpoint_probe_enabled = false;
    std::string signon_message_cursor_carried_checkpoint_probe_scenario = "happy";
    bool signon_message_cursor_carried_checkpoint_resume_allow_surface_enabled = false;
    bool signon_message_cursor_carried_checkpoint_resume_allow_probe_enabled = false;
    std::string signon_message_cursor_carried_checkpoint_resume_allow_probe_scenario =
        "happy";
    bool signon_message_cursor_carried_checkpoint_resume_token_surface_enabled = false;
    bool signon_message_cursor_carried_checkpoint_resume_token_probe_enabled = false;
    std::string signon_message_cursor_carried_checkpoint_resume_token_probe_scenario =
        "happy";
    bool signon_message_cursor_carried_checkpoint_resume_token_claim_surface_enabled = false;
    bool signon_message_cursor_carried_checkpoint_resume_token_claim_probe_enabled = false;
    std::string
        signon_message_cursor_carried_checkpoint_resume_token_claim_probe_scenario =
            "happy";
    bool signon_message_cursor_carried_checkpoint_claimed_resume_allow_surface_enabled =
        false;
    bool signon_message_cursor_carried_checkpoint_claimed_resume_allow_probe_enabled = false;
    std::string
        signon_message_cursor_carried_checkpoint_claimed_resume_allow_probe_scenario =
            "happy";
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_bridge_surface_enabled =
        false;
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_bridge_probe_enabled =
        false;
    std::string
        signon_message_cursor_carried_checkpoint_claimed_checkpoint_bridge_probe_scenario =
            "happy";
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_resume_allow_surface_enabled =
        false;
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_resume_allow_probe_enabled =
        false;
    std::string
        signon_message_cursor_carried_checkpoint_claimed_checkpoint_resume_allow_probe_scenario =
            "happy";
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_resume_token_surface_enabled =
        false;
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_resume_token_probe_enabled =
        false;
    std::string
        signon_message_cursor_carried_checkpoint_claimed_checkpoint_resume_token_probe_scenario =
            "happy";
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_resume_token_claim_surface_enabled =
        false;
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_resume_token_claim_probe_enabled =
        false;
    std::string
        signon_message_cursor_carried_checkpoint_claimed_checkpoint_resume_token_claim_probe_scenario =
            "happy";
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_resume_allow_surface_enabled =
        false;
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_resume_allow_probe_enabled =
        false;
    std::string
        signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_resume_allow_probe_scenario =
            "happy";
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_bridge_surface_enabled =
        false;
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_bridge_probe_enabled =
        false;
    std::string
        signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_bridge_probe_scenario =
            "happy";
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_allow_surface_enabled =
        false;
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_allow_probe_enabled =
        false;
    std::string
        signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_allow_probe_scenario =
            "happy";
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_surface_enabled =
        false;
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_probe_enabled =
        false;
    std::string
        signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_probe_scenario =
            "happy";
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_surface_enabled =
        false;
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_probe_enabled =
        false;
    std::string
        signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_probe_scenario =
            "happy";
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_range_surface_enabled =
        false;
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_range_probe_enabled =
        false;
    std::string
        signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_range_probe_scenario =
            "happy";
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_eof_surface_enabled =
        false;
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_eof_probe_enabled =
        false;
    std::string
        signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_eof_probe_scenario =
            "happy";
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_resume_denial_surface_enabled =
        false;
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_resume_denial_probe_enabled =
        false;
    std::string
        signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_resume_denial_probe_scenario =
            "happy";
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_resume_allow_surface_enabled =
        false;
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_resume_allow_probe_enabled =
        false;
    std::string
        signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_resume_allow_probe_scenario =
            "happy";
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_resume_range_surface_enabled =
        false;
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_resume_range_probe_enabled =
        false;
    std::string
        signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_resume_range_probe_scenario =
            "happy";
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_resume_eof_surface_enabled =
        false;
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_resume_eof_probe_enabled =
        false;
    std::string
        signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_resume_eof_probe_scenario =
            "happy";
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_resumed_denial_surface_enabled =
        false;
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_resumed_denial_probe_enabled =
        false;
    std::string
        signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_resumed_denial_probe_scenario =
            "happy";
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_bridge_surface_enabled =
        false;
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_bridge_probe_enabled =
        false;
    std::string
        signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_bridge_probe_scenario =
            "happy";
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_allow_surface_enabled =
        false;
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_allow_probe_enabled =
        false;
    std::string
        signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_allow_probe_scenario =
            "happy";
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_surface_enabled =
        false;
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_probe_enabled =
        false;
    std::string
        signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_probe_scenario =
            "happy";
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_surface_enabled =
        false;
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_probe_enabled =
        false;
    std::string
        signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_probe_scenario =
            "happy";
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_range_surface_enabled =
        false;
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_range_probe_enabled =
        false;
    std::string
        signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_range_probe_scenario =
            "happy";
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_eof_surface_enabled =
        false;
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_eof_probe_enabled =
        false;
    std::string
        signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_eof_probe_scenario =
            "happy";
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_resume_denial_surface_enabled =
        false;
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_resume_denial_probe_enabled =
        false;
    std::string
        signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_resume_denial_probe_scenario =
            "happy";
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_resume_allow_surface_enabled =
        false;
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_resume_allow_probe_enabled =
        false;
    std::string
        signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_resume_allow_probe_scenario =
            "happy";
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_resume_range_surface_enabled =
        false;
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_resume_range_probe_enabled =
        false;
    std::string
        signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_resume_range_probe_scenario =
            "happy";
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_resume_eof_surface_enabled =
        false;
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_resume_eof_probe_enabled =
        false;
    std::string
        signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_resume_eof_probe_scenario =
            "happy";
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_resumed_denial_surface_enabled =
        false;
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_resumed_denial_probe_enabled =
        false;
    std::string
        signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_resumed_denial_probe_scenario =
            "happy";
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_bridge_surface_enabled =
        false;
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_bridge_probe_enabled =
        false;
    std::string
        signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_bridge_probe_scenario =
            "happy";
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_allow_surface_enabled =
        false;
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_allow_probe_enabled =
        false;
    std::string
        signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_allow_probe_scenario =
            "happy";
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_range_surface_enabled =
        false;
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_range_probe_enabled =
        false;
    std::string
        signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_range_probe_scenario =
            "happy";
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_eof_surface_enabled =
        false;
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_eof_probe_enabled =
        false;
    std::string
        signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_eof_probe_scenario =
            "happy";
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resumed_denial_surface_enabled =
        false;
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resumed_denial_probe_enabled =
        false;
    std::string
        signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resumed_denial_probe_scenario =
            "happy";
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_token_surface_enabled =
        false;
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_token_probe_enabled =
        false;
    std::string
        signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_token_probe_scenario =
            "happy";
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_token_claim_surface_enabled =
        false;
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_token_claim_probe_enabled =
        false;
    std::string
        signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_token_claim_probe_scenario =
            "happy";
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_token_claim_range_surface_enabled =
        false;
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_token_claim_range_probe_enabled =
        false;
    std::string
        signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_token_claim_range_probe_scenario =
            "happy";
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_token_claim_eof_surface_enabled =
        false;
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_token_claim_eof_probe_enabled =
        false;
    std::string
        signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_token_claim_eof_probe_scenario =
            "happy";
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_token_claim_resume_allow_surface_enabled =
        false;
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_token_claim_resume_allow_probe_enabled =
        false;
    std::string
        signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_token_claim_resume_allow_probe_scenario =
            "happy";
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_token_claim_resume_range_surface_enabled =
        false;
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_token_claim_resume_range_probe_enabled =
        false;
    std::string
        signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_token_claim_resume_range_probe_scenario =
            "happy";
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_token_claim_resume_eof_surface_enabled =
        false;
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_token_claim_resume_eof_probe_enabled =
        false;
    std::string
        signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_token_claim_resume_eof_probe_scenario =
            "happy";
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_token_claim_resumed_denial_surface_enabled =
        false;
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_token_claim_resumed_denial_probe_enabled =
        false;
    std::string
        signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_token_claim_resumed_denial_probe_scenario =
            "happy";
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_bridge_surface_enabled =
        false;
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_bridge_probe_enabled =
        false;
    std::string
        signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_bridge_probe_scenario =
            "happy";
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_advance_surface_enabled =
        false;
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_advance_probe_enabled =
        false;
    std::string
        signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_advance_probe_scenario =
            "happy";
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_eof_surface_enabled =
        false;
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_eof_probe_enabled =
        false;
    std::string
        signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_eof_probe_scenario =
            "happy";
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_denial_surface_enabled =
        false;
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_denial_probe_enabled =
        false;
    std::string
        signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_denial_probe_scenario =
            "happy";
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_allow_surface_enabled =
        false;
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_allow_probe_enabled =
        false;
    std::string
        signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_allow_probe_scenario =
            "happy";
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_range_surface_enabled =
        false;
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_range_probe_enabled =
        false;
    std::string
        signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_range_probe_scenario =
            "happy";
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_eof_surface_enabled =
        false;
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_eof_probe_enabled =
        false;
    std::string
        signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_eof_probe_scenario =
            "happy";
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resumed_denial_surface_enabled =
        false;
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resumed_denial_probe_enabled =
        false;
    std::string
        signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resumed_denial_probe_scenario =
            "happy";
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_token_surface_enabled =
        false;
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_token_probe_enabled =
        false;
    std::string
        signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_token_probe_scenario =
            "happy";
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_token_claim_surface_enabled =
        false;
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_token_claim_probe_enabled =
        false;
    std::string
        signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_token_claim_probe_scenario =
            "happy";
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_token_claim_range_surface_enabled =
        false;
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_token_claim_range_probe_enabled =
        false;
    std::string
        signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_token_claim_range_probe_scenario =
            "happy";
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_token_claim_eof_surface_enabled =
        false;
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_token_claim_eof_probe_enabled =
        false;
    std::string
        signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_token_claim_eof_probe_scenario =
            "happy";
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_token_claim_resume_denial_surface_enabled =
        false;
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_token_claim_resume_denial_probe_enabled =
        false;
    std::string
        signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_token_claim_resume_denial_probe_scenario =
            "happy";
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_token_claim_resume_denial_matrix_surface_enabled =
        false;
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_token_claim_resume_denial_matrix_probe_enabled =
        false;
    std::string
        signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_token_claim_resume_denial_matrix_probe_scenario =
            "happy";
    bool signon_resume_lifecycle_acceptance_gate_enabled = false;
    bool signon_resume_lifecycle_acceptance_probe_enabled = false;
    std::string signon_resume_lifecycle_acceptance_probe_scenario = "happy";
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_token_claim_resume_allow_surface_enabled =
        false;
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_token_claim_resume_allow_probe_enabled =
        false;
    std::string
        signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_token_claim_resume_allow_probe_scenario =
            "happy";
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_token_claim_resume_range_surface_enabled =
        false;
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_token_claim_resume_range_probe_enabled =
        false;
    std::string
        signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_token_claim_resume_range_probe_scenario =
            "happy";
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_token_claim_resume_eof_surface_enabled =
        false;
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_token_claim_resume_eof_probe_enabled =
        false;
    std::string
        signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_token_claim_resume_eof_probe_scenario =
            "happy";
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_token_claim_resume_denial_surface_enabled =
        false;
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_token_claim_resume_denial_probe_enabled =
        false;
    std::string
        signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_token_claim_resume_denial_probe_scenario =
            "happy";
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_advance_surface_enabled =
        false;
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_advance_probe_enabled =
        false;
    std::string
        signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_advance_probe_scenario =
            "happy";
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_eof_surface_enabled =
        false;
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_eof_probe_enabled =
        false;
    std::string
        signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_eof_probe_scenario =
            "happy";
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_denial_surface_enabled =
        false;
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_denial_probe_enabled =
        false;
    std::string
        signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_checkpoint_resume_denial_probe_scenario =
            "happy";
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_range_surface_enabled =
        false;
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_range_probe_enabled =
        false;
    std::string
        signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_range_probe_scenario =
            "happy";
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_eof_surface_enabled =
        false;
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_eof_probe_enabled =
        false;
    std::string
        signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_eof_probe_scenario =
            "happy";
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resumed_denial_surface_enabled =
        false;
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resumed_denial_probe_enabled =
        false;
    std::string
        signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resumed_denial_probe_scenario =
            "happy";
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_advance_surface_enabled =
        false;
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_advance_probe_enabled =
        false;
    std::string
        signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_advance_probe_scenario =
            "happy";
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_eof_surface_enabled =
        false;
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_eof_probe_enabled =
        false;
    std::string
        signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_eof_probe_scenario =
            "happy";
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_denial_surface_enabled =
        false;
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_denial_probe_enabled =
        false;
    std::string
        signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_denial_probe_scenario =
            "happy";
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_range_surface_enabled =
        false;
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_range_probe_enabled =
        false;
    std::string
        signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_range_probe_scenario =
            "happy";
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_eof_surface_enabled =
        false;
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_eof_probe_enabled =
        false;
    std::string
        signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_eof_probe_scenario =
            "happy";
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resumed_denial_surface_enabled =
        false;
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resumed_denial_probe_enabled =
        false;
    std::string
        signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resumed_denial_probe_scenario =
            "happy";
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_advance_surface_enabled =
        false;
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_advance_probe_enabled =
        false;
    std::string
        signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_advance_probe_scenario =
            "happy";
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_eof_surface_enabled =
        false;
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_eof_probe_enabled =
        false;
    std::string
        signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_eof_probe_scenario =
            "happy";
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_denial_surface_enabled =
        false;
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_denial_probe_enabled =
        false;
    std::string
        signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_denial_probe_scenario =
            "happy";
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_resume_range_surface_enabled =
        false;
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_resume_range_probe_enabled =
        false;
    std::string
        signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_resume_range_probe_scenario =
            "happy";
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_resume_eof_surface_enabled =
        false;
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_resume_eof_probe_enabled =
        false;
    std::string
        signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_resume_eof_probe_scenario =
            "happy";
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_resumed_denial_surface_enabled =
        false;
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_resumed_denial_probe_enabled =
        false;
    std::string
        signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_resumed_denial_probe_scenario =
            "happy";
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_range_surface_enabled =
        false;
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_range_probe_enabled =
        false;
    std::string
        signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_range_probe_scenario =
            "happy";
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_eof_surface_enabled =
        false;
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_eof_probe_enabled =
        false;
    std::string
        signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_eof_probe_scenario =
            "happy";
    bool
        signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_resume_denial_surface_enabled =
            false;
    bool
        signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_resume_denial_probe_enabled =
            false;
    std::string
        signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_resume_denial_probe_scenario =
            "happy";
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_resume_range_surface_enabled =
        false;
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_resume_range_probe_enabled =
        false;
    std::string
        signon_message_cursor_carried_checkpoint_claimed_checkpoint_resume_range_probe_scenario =
            "happy";
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_resume_eof_surface_enabled =
        false;
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_resume_eof_probe_enabled =
        false;
    std::string
        signon_message_cursor_carried_checkpoint_claimed_checkpoint_resume_eof_probe_scenario =
            "happy";
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_resumed_denial_surface_enabled =
        false;
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_resumed_denial_probe_enabled =
        false;
    std::string
        signon_message_cursor_carried_checkpoint_claimed_checkpoint_resumed_denial_probe_scenario =
            "happy";
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_advance_surface_enabled =
        false;
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_advance_probe_enabled =
        false;
    std::string
        signon_message_cursor_carried_checkpoint_claimed_checkpoint_advance_probe_scenario =
            "happy";
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_eof_surface_enabled =
        false;
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_eof_probe_enabled =
        false;
    std::string
        signon_message_cursor_carried_checkpoint_claimed_checkpoint_eof_probe_scenario =
            "happy";
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_resume_denial_surface_enabled =
        false;
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_resume_denial_probe_enabled =
        false;
    std::string
        signon_message_cursor_carried_checkpoint_claimed_checkpoint_resume_denial_probe_scenario =
            "happy";
    bool signon_message_cursor_carried_checkpoint_claimed_resume_range_surface_enabled =
        false;
    bool signon_message_cursor_carried_checkpoint_claimed_resume_range_probe_enabled = false;
    std::string
        signon_message_cursor_carried_checkpoint_claimed_resume_range_probe_scenario =
            "happy";
    bool signon_message_cursor_carried_checkpoint_claimed_resume_eof_surface_enabled =
        false;
    bool signon_message_cursor_carried_checkpoint_claimed_resume_eof_probe_enabled = false;
    std::string
        signon_message_cursor_carried_checkpoint_claimed_resume_eof_probe_scenario =
            "happy";
    bool signon_message_cursor_carried_checkpoint_claimed_resumed_denial_surface_enabled =
        false;
    bool signon_message_cursor_carried_checkpoint_claimed_resumed_denial_probe_enabled = false;
    std::string
        signon_message_cursor_carried_checkpoint_claimed_resumed_denial_probe_scenario =
            "happy";
    bool signon_message_cursor_carried_checkpoint_claimed_range_surface_enabled = false;
    bool signon_message_cursor_carried_checkpoint_claimed_range_probe_enabled = false;
    std::string signon_message_cursor_carried_checkpoint_claimed_range_probe_scenario =
        "happy";
    bool signon_message_cursor_carried_checkpoint_claimed_eof_surface_enabled = false;
    bool signon_message_cursor_carried_checkpoint_claimed_eof_probe_enabled = false;
    std::string signon_message_cursor_carried_checkpoint_claimed_eof_probe_scenario =
        "happy";
    bool signon_message_cursor_carried_checkpoint_claimed_resume_denial_surface_enabled =
        false;
    bool signon_message_cursor_carried_checkpoint_claimed_resume_denial_probe_enabled = false;
    std::string
        signon_message_cursor_carried_checkpoint_claimed_resume_denial_probe_scenario =
            "happy";
    bool signon_message_cursor_carried_checkpoint_resume_range_surface_enabled = false;
    bool signon_message_cursor_carried_checkpoint_resume_range_probe_enabled = false;
    std::string signon_message_cursor_carried_checkpoint_resume_range_probe_scenario =
        "happy";
    bool signon_message_cursor_carried_checkpoint_resume_eof_surface_enabled = false;
    bool signon_message_cursor_carried_checkpoint_resume_eof_probe_enabled = false;
    std::string signon_message_cursor_carried_checkpoint_resume_eof_probe_scenario =
        "happy";
    bool signon_message_cursor_carried_checkpoint_resumed_denial_surface_enabled = false;
    bool signon_message_cursor_carried_checkpoint_resumed_denial_probe_enabled = false;
    std::string signon_message_cursor_carried_checkpoint_resumed_denial_probe_scenario =
        "happy";
    bool signon_message_cursor_carried_checkpoint_advance_surface_enabled = false;
    bool signon_message_cursor_carried_checkpoint_advance_probe_enabled = false;
    std::string signon_message_cursor_carried_checkpoint_advance_probe_scenario = "happy";
    bool signon_message_cursor_carried_checkpoint_eof_surface_enabled = false;
    bool signon_message_cursor_carried_checkpoint_eof_probe_enabled = false;
    std::string signon_message_cursor_carried_checkpoint_eof_probe_scenario = "happy";
    bool signon_message_cursor_carried_checkpoint_resume_denial_surface_enabled = false;
    bool signon_message_cursor_carried_checkpoint_resume_denial_probe_enabled = false;
    std::string signon_message_cursor_carried_checkpoint_resume_denial_probe_scenario =
        "happy";
    bool signon_message_cursor_carried_resume_denial_surface_enabled = false;
    bool signon_message_cursor_carried_resume_denial_probe_enabled = false;
    std::string signon_message_cursor_carried_resume_denial_probe_scenario = "happy";
    int frame_count = 1000;
    float frame_time = 0.05f;
    int think_limit = 32;
    int use_limit = 64;
    int scheduled_use_limit = 128;
    float path_arrival_epsilon = 24.0f;
    bool trace_scripted = false;
    bool trace_movement = false;
    bool trace_think = false;
    bool trace_callbacks = false;
    bool verbose = false;
    bool stop_on_first_message = false;
    bool stop_on_changelevel_request = false;
    std::optional<std::wstring> stop_on_node;
    std::filesystem::path log_directory = std::filesystem::path(L"logs/latest/runtime");
    bool log_directory_explicit = false;
    bool log_to_file = true;
    int log_max_mb = 10;
    std::optional<common::LogLevel> log_level;
    common::LogLevel log_console_level = common::LogLevel::Info;
    common::LogLevel log_file_level = common::LogLevel::Info;
    std::vector<common::LogCategory> log_categories;
    std::vector<common::LogCategory> log_disable_categories;
    std::vector<common::LogCategory> log_category_files;
    int log_frame_sample = 10;
    bool log_state_changes_only = true;
    bool log_summary_file = true;
    bool log_suppress_repeats = true;
    bool show_help = false;
};

struct LaunchOptionsParseResult
{
    LaunchOptions options;
    std::optional<std::wstring> error_message;
};

LaunchOptionsParseResult ParseLaunchOptions(int argc, wchar_t* argv[]);
std::wstring BuildUsageText(const std::filesystem::path& executable_path);
} // namespace hl::app
