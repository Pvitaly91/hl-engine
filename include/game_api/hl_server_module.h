#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "game_api/cvar_registry.h"

namespace hl::game_api
{
enum class ServerRuntimeMode
{
    kListenHost,
    kDedicated,
};

struct DllFunctionPointerStatus
{
    std::string name;
    bool present = false;
};

struct InvokedEngineCallback
{
    std::string name;
    std::size_t call_count = 0;
};

struct NamedCountSummary
{
    std::string name;
    std::size_t count = 0;
};

struct FrameBootstrapOptions
{
    int frames = 1000;
    float frametime = 0.05f;
    int think_limit = 32;
    int use_limit = 64;
    int scheduled_use_limit = 128;
    float path_arrival_epsilon = 24.0f;
    bool trace_scripted = false;
    bool trace_movement = false;
    bool trace_think = false;
    bool trace_callbacks = false;
    int log_frame_sample = 10;
    bool log_state_changes_only = true;
    bool stop_on_first_message = false;
    bool stop_on_changelevel_request = false;
    std::string stop_on_node;
};

struct GlobalVariablesSnapshot
{
    std::string map_name;
    std::string startspot;
    float time = 0.0f;
    float frametime = 0.0f;
    float deathmatch = 0.0f;
    float coop = 0.0f;
    int max_clients = 0;
    int max_entities = 0;
};

struct BspLumpSummary
{
    std::string name;
    int file_offset = 0;
    int file_length = 0;
    bool within_file = false;
};

struct EntityClassCountSummary
{
    std::string classname;
    std::size_t count = 0;
};

struct EntityClassSupportSummary
{
    std::string classname;
    std::size_t spawned_successfully = 0;
    std::size_t removed_during_spawn = 0;
    std::size_t deferred_unsupported = 0;
    std::size_t failed_during_spawn = 0;
};

struct WorldBootstrapStateSummary
{
    bool attempted = false;
    bool completed = false;
    std::string map_name;
    std::string model_path;
    std::string map_path;
    bool bsp_loaded = false;
    int bsp_version = 0;
    std::uintmax_t bsp_file_size = 0;
    std::size_t bsp_lump_count = 0;
    std::vector<BspLumpSummary> key_lumps;
    int world_model_index = 0;
    int world_edict_index = -1;
    std::size_t entities_lump_size = 0;
    std::string edict0_state;
    bool ready_for_entity_parsing = false;
};

struct EntityPipelineStateSummary
{
    bool attempted = false;
    bool entities_lump_parsed = false;
    bool partially_parsed = false;
    std::string failure_reason;
    std::size_t parse_error_count = 0;
    std::size_t total_parsed_entities = 0;
    std::size_t total_worldspawn_entities = 0;
    bool first_entity_is_worldspawn = false;
    std::size_t total_runtime_entities_allocated = 0;
    std::size_t total_keyvalues_dispatched = 0;
    std::size_t total_keyvalues_handled = 0;
    std::size_t total_spawn_attempts = 0;
    std::size_t total_successful_spawns = 0;
    std::size_t total_removed_entities = 0;
    std::size_t total_deferred_entities = 0;
    std::vector<EntityClassCountSummary> top_classname_counts;
    std::vector<EntityClassSupportSummary> classname_support_summary;
    std::vector<std::string> sample_entities;
    std::vector<std::string> sample_spawned_entities;
    std::vector<InvokedEngineCallback> newly_exercised_engine_callbacks;
};

struct ServerBootstrapStateSummary
{
    bool initialized = false;
    std::string game_directory;
    std::string mod_name;
    std::string hostname;
    int maxclients = 0;
    std::string map_name;
    bool active = false;
    bool loading = false;
    std::uint64_t frame_count = 0;
    std::uint64_t server_frame = 0;
    float time = 0.0f;
    float frametime = 0.0f;
};

struct WorldspawnSpawnStateSummary
{
    bool attempted = false;
    bool succeeded = false;
    bool seh_exception = false;
    unsigned int seh_code = 0;
    std::string map_name;
    int edict_index = -1;
    std::string classname;
    std::string last_callback;
    std::vector<std::string> distinct_callback_tail;
    std::vector<std::string> trace_tail;
    std::vector<std::string> precache_tail;
    std::size_t precache_requests = 0;
    std::size_t sound_precache_count = 0;
    std::size_t sound_registry_size = 0;
    std::size_t missing_sound_files = 0;
    std::size_t duplicate_sound_precache_count = 0;
    std::uint64_t random_long_call_count = 0;
};

struct ServerActivationStateSummary
{
    bool function_present = false;
    bool attempted = false;
    bool preflight_succeeded = false;
    bool succeeded = false;
    bool seh_exception = false;
    unsigned int seh_code = 0;
    bool worldspawn_spawned = false;
    bool edict0_valid = false;
    int allocated_edicts = 0;
    int edict_count_passed = 0;
    int spawned_edicts = 0;
    int removed_edicts = 0;
    int deferred_edicts = 0;
    int activation_candidate_count = 0;
    int pruned_participants = 0;
    int max_clients = 0;
    std::string map_name;
    std::string globals_snapshot;
    std::string server_state_snapshot;
    std::size_t trace_events_captured = 0;
    std::vector<EntityClassCountSummary> activation_class_counts;
    std::vector<std::string> activation_entities_preview;
    std::vector<std::string> pruned_entities_preview;
    std::vector<std::string> validation_rejections;
    std::vector<std::string> distinct_callback_tail;
    std::vector<std::string> callback_trace_tail;
    std::string likely_blocker;
};

struct ServerFrameStateSummary
{
    int frame_number = 0;
    std::uint64_t host_frame_index = 0;
    std::uint64_t server_frame_index = 0;
    float time = 0.0f;
    float frametime = 0.0f;
    bool validation_passed = false;
    bool start_frame_present = false;
    bool start_frame_called = false;
    bool start_frame_succeeded = false;
    bool seh_exception = false;
    unsigned int seh_code = 0;
    std::string globals_snapshot;
    std::string server_state_snapshot;
    int active_edicts = 0;
    int spawned_entities = 0;
    int removed_entities = 0;
    int deferred_entities = 0;
    std::vector<std::string> validation_issues;
    std::vector<std::string> entity_preview;
    std::vector<std::string> message_preview;
    std::vector<std::string> callback_trace_tail;
    std::vector<InvokedEngineCallback> callback_counts_this_frame;
};

struct ServerFrameLoopStateSummary
{
    bool configured = false;
    bool attempted = false;
    bool activation_succeeded = false;
    bool start_frame_present = false;
    bool any_seh = false;
    bool stopped_early = false;
    unsigned int seh_code = 0;
    int frames_requested = 0;
    int frames_completed = 0;
    int stop_frame = -1;
    float fixed_frametime = 0.0f;
    float final_time = 0.0f;
    float stop_time = 0.0f;
    std::size_t total_trace_events = 0;
    std::vector<std::string> validation_failures;
    std::vector<InvokedEngineCallback> callbacks_during_loop;
    std::vector<ServerFrameStateSummary> frames;
    std::string stop_reason;
    std::string readiness;
};

struct EntityThinkFrameStateSummary
{
    int frame_number = 0;
    std::uint64_t host_frame_index = 0;
    std::uint64_t server_frame_index = 0;
    float time = 0.0f;
    float frametime = 0.0f;
    int active_entities = 0;
    int due_thinks = 0;
    int executed_thinks = 0;
    int deferred_thinks = 0;
    int think_failures = 0;
    int seh_failures = 0;
    int removed_by_game_logic = 0;
    std::vector<std::string> due_entities_preview;
    std::vector<std::string> callback_trace_tail;
    std::vector<InvokedEngineCallback> callback_counts_this_frame;
};

struct EntityLifecycleStateCountSummary
{
    std::string state;
    std::size_t count = 0;
};

struct EntityLifecycleClassSummary
{
    std::string classname;
    std::size_t active_supported = 0;
    std::size_t passive_supported = 0;
    std::size_t detected_but_deferred = 0;
    std::size_t removed_by_game_logic = 0;
    std::size_t due_thinks = 0;
    std::size_t executed_thinks = 0;
    std::size_t deferred_thinks = 0;
};

struct EntityThinkSchedulerStateSummary
{
    bool configured = false;
    bool attempted = false;
    bool start_frame_present = false;
    bool think_dispatch_present = false;
    int think_limit = 0;
    int frames_attempted = 0;
    int frames_completed = 0;
    int total_due_thinks = 0;
    int total_executed_thinks = 0;
    int total_deferred_thinks = 0;
    int total_think_failures = 0;
    int total_seh_failures = 0;
    int total_removed_by_game_logic = 0;
    std::vector<EntityLifecycleStateCountSummary> active_entity_counts_by_support_state;
    std::vector<EntityLifecycleClassSummary> classname_lifecycle_summary;
    std::vector<InvokedEngineCallback> callbacks_during_scheduler;
    std::vector<std::string> rolling_trace_tail;
    std::vector<EntityThinkFrameStateSummary> frames;
    std::string readiness;
};

enum class MapLogicSupportState
{
    kUseSupported,
    kPassiveRecipientOnly,
    kDetectedButDeferred,
    kRemovedByGameLogic,
};

enum class ScriptedLogicSupportState
{
    kPassiveRecipientOnly,
    kUseSupported,
    kScheduledUseSupported,
    kScriptedProgressing,
    kBlockedOnMovement,
    kBlockedOnActor,
    kBlockedOnEngineCallback,
    kRemovedByGameLogic,
};

struct MapLogicFrameStateSummary
{
    int frame_number = 0;
    std::uint64_t host_frame_index = 0;
    std::uint64_t server_frame_index = 0;
    float time = 0.0f;
    float frametime = 0.0f;
    int queue_size_before = 0;
    int queue_size_after = 0;
    int target_chains_fired = 0;
    int target_resolutions = 0;
    int use_attempts = 0;
    int use_successes = 0;
    int use_deferred = 0;
    int use_failures = 0;
    int use_seh_failures = 0;
    int scheduled_due = 0;
    int scheduled_created = 0;
    int scheduled_executed = 0;
    int scheduled_rescheduled = 0;
    int scheduled_skipped = 0;
    int scheduled_failed = 0;
    int scheduled_deferred = 0;
    int scheduled_pending = 0;
    int long_delay_executed = 0;
    int long_delay_pending = 0;
    std::vector<std::string> scheduled_action_preview;
    std::vector<std::string> trace_tail;
    std::vector<InvokedEngineCallback> callback_counts_this_frame;
};

struct MapLogicClassSummary
{
    std::string classname;
    MapLogicSupportState support_state = MapLogicSupportState::kDetectedButDeferred;
    std::size_t active_entities = 0;
    std::size_t can_think = 0;
    std::size_t can_receive_use = 0;
    std::size_t can_emit_targets = 0;
    std::size_t pending_scheduled_outputs = 0;
    std::size_t triggered_this_frame = 0;
    std::size_t blocked_or_deferred = 0;
    std::size_t target_resolutions = 0;
    std::size_t use_attempts = 0;
    std::size_t use_successes = 0;
    std::size_t use_deferred = 0;
    std::size_t use_failures = 0;
};

struct MapLogicDispatcherStateSummary
{
    bool configured = false;
    bool attempted = false;
    bool use_dispatch_present = false;
    int use_limit = 0;
    int scheduled_use_limit = 0;
    int frames_attempted = 0;
    int frames_completed = 0;
    int total_target_chains_fired = 0;
    int total_target_resolutions = 0;
    int total_no_targets_found = 0;
    int total_single_target_hits = 0;
    int total_multi_target_hits = 0;
    int total_use_attempts = 0;
    int total_use_successes = 0;
    int total_use_deferred = 0;
    int total_use_failures = 0;
    int total_use_seh_failures = 0;
    int total_scheduled_created = 0;
    int total_scheduled_executed = 0;
    int total_scheduled_rescheduled = 0;
    int total_scheduled_skipped = 0;
    int total_scheduled_failed = 0;
    int total_scheduled_deferred = 0;
    int total_long_delay_executed = 0;
    int total_long_delay_pending = 0;
    int scheduled_pending = 0;
    std::vector<MapLogicClassSummary> classname_summary;
    std::vector<InvokedEngineCallback> callbacks_during_dispatcher;
    std::vector<MapLogicFrameStateSummary> frames;
    std::vector<std::string> rolling_trace_tail;
    std::string readiness;
};

struct ChangeLevelTransitionSummary
{
    struct ChangeLevelTargetValidationSummary
    {
        bool attempted = false;
        std::string current_map;
        std::string requested_map;
        bool target_map_exists = false;
        std::string target_bsp_path;
        std::string landmark;
        bool current_landmark_found = false;
        bool current_landmark_origin_available = false;
        std::string current_landmark_origin;
        bool target_landmark_found = false;
        bool target_landmark_origin_available = false;
        std::string target_landmark_origin;
        bool target_worldspawn_present = false;
        bool entity_parse_succeeded = false;
        std::string action;
        std::string missing_component;
        std::string detail;
    };

    struct ChangeLevelLifecycleGateSummary
    {
        bool attempted = false;
        bool intent_consumed = false;
        bool target_validation_passed = false;
        bool bootstrap_allowed = false;
        std::string requested_map;
        std::string landmark;
        std::string action;
        std::string reason;
    };

    struct ChangeLevelLifecycleEntrySummary
    {
        bool attempted = false;
        bool gate_checked = false;
        bool gate_passed = false;
        bool eligible = false;
        bool blocked_by_stop_mode = false;
        std::string requested_map;
        std::string landmark;
        std::string action;
        std::string short_circuit_reason;
    };

    struct ChangeLevelLifecycleDispatchSummary
    {
        bool attempted = false;
        bool dispatch_checked = false;
        bool dispatch_allowed = false;
        bool dispatch_blocked = false;
        std::string decision_source;
        std::string requested_map;
        std::string landmark;
        std::string action;
        std::string short_circuit_reason;
    };

    struct ChangeLevelLifecycleExecutionSummary
    {
        bool attempted = false;
        bool execution_checked = false;
        bool execution_armed = false;
        bool execution_skipped = false;
        std::string decision_source;
        std::string current_map;
        std::string requested_map;
        std::string target_bsp_path;
        std::string landmark;
        bool target_worldspawn_present = false;
        bool target_entity_parse_ok = false;
        bool current_landmark_origin_available = false;
        std::string current_landmark_origin;
        bool target_landmark_origin_available = false;
        std::string target_landmark_origin;
        std::string action;
        std::string short_circuit_reason;
    };

    struct ChangeLevelBootstrapPlanSummary
    {
        bool attempted = false;
        bool prepared = false;
        bool skipped = false;
        std::string decision_source;
        std::string current_map;
        std::string requested_map;
        std::string target_bsp_path;
        std::string landmark;
        bool target_worldspawn_present = false;
        bool target_entity_parse_ok = false;
        bool current_landmark_origin_available = false;
        std::string current_landmark_origin;
        bool target_landmark_origin_available = false;
        std::string target_landmark_origin;
        std::string action;
        std::string short_circuit_reason;
    };

    struct ChangeLevelLandmarkTransformSummary
    {
        bool attempted = false;
        bool transform_computed = false;
        bool skipped = false;
        std::string decision_source;
        bool current_landmark_origin_available = false;
        std::string current_landmark_origin;
        bool target_landmark_origin_available = false;
        std::string target_landmark_origin;
        std::string translation_delta;
        std::string action;
        std::string short_circuit_reason;
    };

    struct ChangeLevelProjectedCarriedOriginSummary
    {
        bool attempted = false;
        bool projected = false;
        bool skipped = false;
        std::string decision_source;
        bool current_carried_origin_available = false;
        std::string current_carried_origin;
        std::string translation_delta;
        std::string projected_target_origin;
        std::string action;
        std::string short_circuit_reason;
    };

    struct ChangeLevelProjectedCarriedOrientationSummary
    {
        bool attempted = false;
        bool projected = false;
        bool skipped = false;
        std::string decision_source;
        bool current_carried_yaw_available = false;
        std::string current_carried_yaw;
        bool current_landmark_angles_available = false;
        std::string current_landmark_angles;
        bool target_landmark_angles_available = false;
        std::string target_landmark_angles;
        std::string yaw_delta;
        std::string projected_target_yaw;
        std::string action;
        std::string short_circuit_reason;
    };

    struct ChangeLevelProjectedTransferSnapshotSummary
    {
        bool attempted = false;
        bool prepared = false;
        bool skipped = false;
        std::string decision_source;
        std::string current_map;
        std::string requested_map;
        std::string target_bsp_path;
        std::string landmark;
        std::string projected_target_origin;
        std::string projected_target_yaw;
        bool target_worldspawn_present = false;
        bool target_entity_parse_ok = false;
        bool transfer_ready = false;
        std::string action;
        std::string short_circuit_reason;
    };

    struct ChangeLevelPlayerTransferApplyPlanSummary
    {
        bool attempted = false;
        bool prepared = false;
        bool skipped = false;
        std::string decision_source;
        std::string apply_target;
        std::string current_map;
        std::string requested_map;
        std::string target_bsp_path;
        std::string landmark;
        std::string target_player_origin;
        std::string target_player_yaw;
        bool origin_write_prepared = false;
        bool yaw_write_prepared = false;
        bool inventory_write_prepared = false;
        bool velocity_write_prepared = false;
        bool target_worldspawn_present = false;
        bool target_entity_parse_ok = false;
        bool apply_ready = false;
        std::string action;
        std::string short_circuit_reason;
    };

    struct ChangeLevelPlayerTransferWriteSetSummary
    {
        bool attempted = false;
        bool prepared = false;
        bool skipped = false;
        std::string decision_source;
        std::string apply_target;
        std::string current_map;
        std::string requested_map;
        std::string target_bsp_path;
        std::string landmark;
        std::string target_player_origin;
        std::string target_player_yaw;
        bool write_origin = false;
        bool write_yaw = false;
        bool write_inventory = false;
        bool write_velocity = false;
        int write_count = 0;
        bool runtime_write_suppressed = false;
        bool target_worldspawn_present = false;
        bool target_entity_parse_ok = false;
        bool write_set_ready = false;
        std::string action;
        std::string short_circuit_reason;
    };

    struct ChangeLevelPlayerTransferDeferredApplyGateSummary
    {
        bool attempted = false;
        bool prepared = false;
        bool skipped = false;
        std::string decision_source;
        std::string apply_target;
        std::string current_map;
        std::string requested_map;
        std::string future_apply_phase;
        int pending_write_count = 0;
        bool gate_open = false;
        bool deferred = false;
        std::string gate_reason;
        bool runtime_write_suppressed = false;
        bool apply_gate_ready = false;
        std::string action;
        std::string short_circuit_reason;
    };

    struct ChangeLevelPlayerTransferGateOpenCheckpointSummary
    {
        bool attempted = false;
        bool prepared = false;
        bool skipped = false;
        std::string decision_source;
        std::string apply_target;
        std::string current_map;
        std::string requested_map;
        std::string future_apply_phase;
        std::string target_runtime_checkpoint;
        int pending_write_count = 0;
        bool checkpoint_satisfied = false;
        bool gate_eligible_at_checkpoint = false;
        bool gate_open = false;
        bool deferred = false;
        bool runtime_write_suppressed = false;
        bool checkpoint_ready = false;
        std::string action;
        std::string short_circuit_reason;
    };

    struct ChangeLevelPlayerTransferCheckpointSignalContractSummary
    {
        bool attempted = false;
        bool prepared = false;
        bool skipped = false;
        std::string decision_source;
        std::string apply_target;
        std::string current_map;
        std::string requested_map;
        std::string future_apply_phase;
        std::string target_runtime_checkpoint;
        std::string required_signal;
        int pending_write_count = 0;
        bool signal_observed = false;
        bool checkpoint_satisfied = false;
        bool gate_eligible_at_checkpoint = false;
        bool gate_open = false;
        bool deferred = false;
        bool runtime_write_suppressed = false;
        bool signal_contract_ready = false;
        std::string action;
        std::string short_circuit_reason;
    };

    struct ChangeLevelPlayerTransferCheckpointSignalObservationStateSummary
    {
        bool attempted = false;
        bool prepared = false;
        bool skipped = false;
        std::string decision_source;
        std::string apply_target;
        std::string current_map;
        std::string requested_map;
        std::string future_apply_phase;
        std::string target_runtime_checkpoint;
        std::string required_signal;
        std::string observation_mode;
        bool signal_observed = false;
        bool checkpoint_satisfied = false;
        bool gate_eligible_at_checkpoint = false;
        bool gate_open = false;
        bool deferred = false;
        bool runtime_observation_suppressed = false;
        bool runtime_write_suppressed = false;
        bool observation_state_ready = false;
        std::string action;
        std::string short_circuit_reason;
    };

    struct ChangeLevelPlayerTransferCheckpointSignalHookPointSummary
    {
        bool attempted = false;
        bool prepared = false;
        bool skipped = false;
        std::string decision_source;
        std::string apply_target;
        std::string current_map;
        std::string requested_map;
        std::string future_apply_phase;
        std::string target_runtime_checkpoint;
        std::string required_signal;
        std::string observation_mode;
        std::string runtime_hook_point;
        bool hook_installed = false;
        bool signal_observed = false;
        bool checkpoint_satisfied = false;
        bool gate_open = false;
        bool deferred = false;
        bool runtime_hook_suppressed = false;
        bool runtime_observation_suppressed = false;
        bool runtime_write_suppressed = false;
        bool hook_point_ready = false;
        std::string action;
        std::string short_circuit_reason;
    };

    struct ChangeLevelPlayerTransferCheckpointSignalHookRegistrationStateSummary
    {
        bool attempted = false;
        bool prepared = false;
        bool skipped = false;
        std::string decision_source;
        std::string apply_target;
        std::string current_map;
        std::string requested_map;
        std::string future_apply_phase;
        std::string target_runtime_checkpoint;
        std::string required_signal;
        std::string observation_mode;
        std::string runtime_hook_point;
        std::string registration_state;
        bool hook_registration_planned = false;
        bool hook_installed = false;
        bool signal_observed = false;
        bool checkpoint_satisfied = false;
        bool gate_open = false;
        bool deferred = false;
        bool runtime_hook_registration_suppressed = false;
        bool runtime_hook_suppressed = false;
        bool runtime_observation_suppressed = false;
        bool runtime_write_suppressed = false;
        bool hook_registration_ready = false;
        std::string action;
        std::string short_circuit_reason;
    };

    struct ChangeLevelPlayerTransferCheckpointSignalHookInstallTokenSummary
    {
        bool attempted = false;
        bool prepared = false;
        bool skipped = false;
        std::string decision_source;
        std::string apply_target;
        std::string current_map;
        std::string requested_map;
        std::string future_apply_phase;
        std::string target_runtime_checkpoint;
        std::string required_signal;
        std::string observation_mode;
        std::string runtime_hook_point;
        std::string registration_state;
        bool hook_registration_planned = false;
        bool install_token_issued = false;
        bool hook_install_authorized = false;
        bool hook_installed = false;
        bool signal_observed = false;
        bool checkpoint_satisfied = false;
        bool gate_open = false;
        bool deferred = false;
        bool runtime_hook_installation_suppressed = false;
        bool runtime_hook_registration_suppressed = false;
        bool runtime_hook_suppressed = false;
        bool runtime_observation_suppressed = false;
        bool runtime_write_suppressed = false;
        bool install_token_ready = false;
        std::string action;
        std::string short_circuit_reason;
    };

    struct ChangeLevelPlayerTransferCheckpointSignalHookInstallEligibilityGateSummary
    {
        bool attempted = false;
        bool prepared = false;
        bool skipped = false;
        std::string decision_source;
        std::string apply_target;
        std::string current_map;
        std::string requested_map;
        std::string future_apply_phase;
        std::string target_runtime_checkpoint;
        std::string required_signal;
        std::string observation_mode;
        std::string runtime_hook_point;
        std::string registration_state;
        bool install_token_issued = false;
        bool hook_install_authorized = false;
        bool install_eligibility = false;
        bool install_gate_open = false;
        bool install_deferred = false;
        std::string eligibility_reason;
        bool hook_installed = false;
        bool signal_observed = false;
        bool checkpoint_satisfied = false;
        bool gate_open = false;
        bool deferred = false;
        bool runtime_hook_installation_suppressed = false;
        bool runtime_hook_registration_suppressed = false;
        bool runtime_hook_suppressed = false;
        bool runtime_observation_suppressed = false;
        bool runtime_write_suppressed = false;
        bool install_eligibility_gate_ready = false;
        std::string action;
        std::string short_circuit_reason;
    };

    struct ChangeLevelPlayerTransferCheckpointSignalHookInstallTokenIssueDecisionSummary
    {
        bool attempted = false;
        bool prepared = false;
        bool skipped = false;
        std::string decision_source;
        std::string apply_target;
        std::string current_map;
        std::string requested_map;
        std::string future_apply_phase;
        std::string target_runtime_checkpoint;
        std::string required_signal;
        std::string observation_mode;
        std::string runtime_hook_point;
        std::string registration_state;
        bool install_token_issued = false;
        bool hook_install_authorized = false;
        bool install_eligibility = false;
        bool install_gate_open = false;
        std::string token_issue_decision;
        bool token_issue_allowed = false;
        std::string token_issue_reason;
        bool hook_installed = false;
        bool signal_observed = false;
        bool checkpoint_satisfied = false;
        bool gate_open = false;
        bool deferred = false;
        bool runtime_hook_installation_suppressed = false;
        bool runtime_hook_registration_suppressed = false;
        bool runtime_hook_suppressed = false;
        bool runtime_observation_suppressed = false;
        bool runtime_write_suppressed = false;
        bool token_issue_decision_ready = false;
        std::string action;
        std::string short_circuit_reason;
    };

    struct ChangeLevelPlayerTransferCheckpointSignalHookInstallAuthorizationStateSummary
    {
        bool attempted = false;
        bool prepared = false;
        bool skipped = false;
        std::string decision_source;
        std::string apply_target;
        std::string current_map;
        std::string requested_map;
        std::string future_apply_phase;
        std::string target_runtime_checkpoint;
        std::string required_signal;
        std::string observation_mode;
        std::string runtime_hook_point;
        std::string registration_state;
        bool install_token_issued = false;
        std::string token_issue_decision;
        bool token_issue_allowed = false;
        std::string authorization_state;
        bool install_authorization_granted = false;
        std::string authorization_reason;
        bool hook_install_authorized = false;
        bool hook_installed = false;
        bool signal_observed = false;
        bool checkpoint_satisfied = false;
        bool gate_open = false;
        bool deferred = false;
        bool runtime_hook_authorization_suppressed = false;
        bool runtime_hook_installation_suppressed = false;
        bool runtime_hook_registration_suppressed = false;
        bool runtime_hook_suppressed = false;
        bool runtime_observation_suppressed = false;
        bool runtime_write_suppressed = false;
        bool install_authorization_state_ready = false;
        std::string action;
        std::string short_circuit_reason;
    };

    struct ChangeLevelPlayerTransferCheckpointSignalHookInstallAttemptGateSummary
    {
        bool attempted = false;
        bool prepared = false;
        bool skipped = false;
        std::string decision_source;
        std::string apply_target;
        std::string current_map;
        std::string requested_map;
        std::string future_apply_phase;
        std::string target_runtime_checkpoint;
        std::string required_signal;
        std::string observation_mode;
        std::string runtime_hook_point;
        std::string registration_state;
        bool install_token_issued = false;
        std::string token_issue_decision;
        bool token_issue_allowed = false;
        std::string authorization_state;
        bool install_authorization_granted = false;
        bool install_attempt_allowed = false;
        bool install_attempt_deferred = false;
        std::string install_attempt_reason;
        bool hook_install_authorized = false;
        bool hook_installed = false;
        bool signal_observed = false;
        bool checkpoint_satisfied = false;
        bool gate_open = false;
        bool deferred = false;
        bool runtime_hook_attempt_suppressed = false;
        bool runtime_hook_authorization_suppressed = false;
        bool runtime_hook_installation_suppressed = false;
        bool runtime_hook_registration_suppressed = false;
        bool runtime_hook_suppressed = false;
        bool runtime_observation_suppressed = false;
        bool runtime_write_suppressed = false;
        bool install_attempt_gate_ready = false;
        std::string action;
        std::string short_circuit_reason;
    };

    struct ChangeLevelPlayerTransferCheckpointSignalHookInstallExecutionPlanSummary
    {
        bool attempted = false;
        bool prepared = false;
        bool skipped = false;
        std::string decision_source;
        std::string apply_target;
        std::string current_map;
        std::string requested_map;
        std::string future_apply_phase;
        std::string target_runtime_checkpoint;
        std::string required_signal;
        std::string observation_mode;
        std::string runtime_hook_point;
        std::string registration_state;
        bool install_token_issued = false;
        std::string token_issue_decision;
        bool token_issue_allowed = false;
        std::string authorization_state;
        bool install_authorization_granted = false;
        bool install_attempt_allowed = false;
        bool install_attempt_deferred = false;
        bool install_execution_planned = false;
        bool install_execution_armed = false;
        bool install_execution_blocked = false;
        std::string install_execution_reason;
        bool hook_install_authorized = false;
        bool hook_installed = false;
        bool signal_observed = false;
        bool checkpoint_satisfied = false;
        bool gate_open = false;
        bool deferred = false;
        bool runtime_hook_execution_suppressed = false;
        bool runtime_hook_attempt_suppressed = false;
        bool runtime_hook_authorization_suppressed = false;
        bool runtime_hook_installation_suppressed = false;
        bool runtime_hook_registration_suppressed = false;
        bool runtime_hook_suppressed = false;
        bool runtime_observation_suppressed = false;
        bool runtime_write_suppressed = false;
        bool install_execution_plan_ready = false;
        std::string action;
        std::string short_circuit_reason;
    };

    struct ChangeLevelPlayerTransferCheckpointSignalHookInstallExecutionStateSummary
    {
        bool attempted = false;
        bool prepared = false;
        bool skipped = false;
        std::string decision_source;
        std::string apply_target;
        std::string current_map;
        std::string requested_map;
        std::string future_apply_phase;
        std::string target_runtime_checkpoint;
        std::string required_signal;
        std::string observation_mode;
        std::string runtime_hook_point;
        std::string registration_state;
        bool install_token_issued = false;
        std::string token_issue_decision;
        bool token_issue_allowed = false;
        std::string authorization_state;
        bool install_authorization_granted = false;
        bool install_attempt_allowed = false;
        bool install_attempt_deferred = false;
        bool install_execution_planned = false;
        bool install_execution_armed = false;
        bool install_execution_blocked = false;
        bool install_execution_attempted = false;
        bool install_execution_completed = false;
        std::string install_execution_state;
        std::string install_execution_state_reason;
        bool hook_install_authorized = false;
        bool hook_installed = false;
        bool signal_observed = false;
        bool checkpoint_satisfied = false;
        bool gate_open = false;
        bool deferred = false;
        bool runtime_hook_execution_suppressed = false;
        bool runtime_hook_attempt_suppressed = false;
        bool runtime_hook_authorization_suppressed = false;
        bool runtime_hook_installation_suppressed = false;
        bool runtime_hook_registration_suppressed = false;
        bool runtime_hook_suppressed = false;
        bool runtime_observation_suppressed = false;
        bool runtime_write_suppressed = false;
        bool install_execution_state_ready = false;
        std::string action;
        std::string short_circuit_reason;
    };

    struct ChangeLevelPlayerTransferCheckpointSignalHookInstallExecutionOutcomeSummary
    {
        bool attempted = false;
        bool prepared = false;
        bool skipped = false;
        std::string decision_source;
        std::string apply_target;
        std::string current_map;
        std::string requested_map;
        std::string future_apply_phase;
        std::string target_runtime_checkpoint;
        std::string required_signal;
        std::string observation_mode;
        std::string runtime_hook_point;
        std::string registration_state;
        bool install_token_issued = false;
        std::string token_issue_decision;
        bool token_issue_allowed = false;
        std::string authorization_state;
        bool install_authorization_granted = false;
        bool install_attempt_allowed = false;
        bool install_attempt_deferred = false;
        bool install_execution_planned = false;
        bool install_execution_armed = false;
        bool install_execution_blocked = false;
        bool install_execution_attempted = false;
        bool install_execution_completed = false;
        bool install_execution_succeeded = false;
        std::string install_execution_outcome;
        bool install_execution_outcome_available = false;
        std::string install_execution_outcome_reason;
        bool hook_install_authorized = false;
        bool hook_installed = false;
        bool signal_observed = false;
        bool checkpoint_satisfied = false;
        bool gate_open = false;
        bool deferred = false;
        bool runtime_hook_outcome_suppressed = false;
        bool runtime_hook_execution_suppressed = false;
        bool runtime_hook_attempt_suppressed = false;
        bool runtime_hook_authorization_suppressed = false;
        bool runtime_hook_installation_suppressed = false;
        bool runtime_hook_registration_suppressed = false;
        bool runtime_hook_suppressed = false;
        bool runtime_observation_suppressed = false;
        bool runtime_write_suppressed = false;
        bool install_execution_outcome_ready = false;
        std::string action;
        std::string short_circuit_reason;
    };

    struct ChangeLevelPlayerTransferCheckpointSignalHookInstallResultStateSummary
    {
        bool attempted = false;
        bool prepared = false;
        bool skipped = false;
        std::string decision_source;
        std::string apply_target;
        std::string current_map;
        std::string requested_map;
        std::string future_apply_phase;
        std::string target_runtime_checkpoint;
        std::string required_signal;
        std::string observation_mode;
        std::string runtime_hook_point;
        std::string registration_state;
        bool install_token_issued = false;
        std::string token_issue_decision;
        bool token_issue_allowed = false;
        std::string authorization_state;
        bool install_authorization_granted = false;
        bool install_attempt_allowed = false;
        bool install_attempt_deferred = false;
        bool install_execution_planned = false;
        bool install_execution_armed = false;
        bool install_execution_blocked = false;
        bool install_execution_attempted = false;
        bool install_execution_completed = false;
        bool install_execution_succeeded = false;
        std::string install_execution_outcome;
        bool install_execution_outcome_available = false;
        std::string result_state;
        bool result_produced = false;
        bool result_consumable = false;
        std::string result_reason;
        std::string completion_state;
        bool hook_install_authorized = false;
        bool hook_installed = false;
        bool signal_observed = false;
        bool checkpoint_satisfied = false;
        bool gate_open = false;
        bool deferred = false;
        bool runtime_hook_result_state_suppressed = false;
        bool runtime_hook_outcome_suppressed = false;
        bool runtime_hook_execution_suppressed = false;
        bool runtime_hook_attempt_suppressed = false;
        bool runtime_hook_authorization_suppressed = false;
        bool runtime_hook_installation_suppressed = false;
        bool runtime_hook_registration_suppressed = false;
        bool runtime_hook_suppressed = false;
        bool runtime_observation_suppressed = false;
        bool runtime_write_suppressed = false;
        bool install_result_state_ready = false;
        std::string action;
        std::string short_circuit_reason;
    };

    struct ChangeLevelPlayerTransferCheckpointSignalHookInstallResultConsumptionGateSummary
    {
        bool attempted = false;
        bool prepared = false;
        bool skipped = false;
        std::string decision_source;
        std::string apply_target;
        std::string current_map;
        std::string requested_map;
        std::string future_apply_phase;
        std::string target_runtime_checkpoint;
        std::string required_signal;
        std::string observation_mode;
        std::string runtime_hook_point;
        std::string registration_state;
        bool install_token_issued = false;
        std::string token_issue_decision;
        bool token_issue_allowed = false;
        std::string authorization_state;
        bool install_authorization_granted = false;
        std::string install_execution_outcome;
        std::string result_state;
        bool result_produced = false;
        bool result_consumable = false;
        bool result_consumption_allowed = false;
        bool result_consumption_deferred = false;
        std::string result_consumption_reason;
        std::string completion_state;
        bool hook_install_authorized = false;
        bool hook_installed = false;
        bool signal_observed = false;
        bool checkpoint_satisfied = false;
        bool gate_open = false;
        bool deferred = false;
        bool runtime_hook_result_consumption_suppressed = false;
        bool runtime_hook_result_state_suppressed = false;
        bool runtime_hook_outcome_suppressed = false;
        bool runtime_hook_execution_suppressed = false;
        bool runtime_hook_attempt_suppressed = false;
        bool runtime_hook_authorization_suppressed = false;
        bool runtime_hook_installation_suppressed = false;
        bool runtime_hook_registration_suppressed = false;
        bool runtime_hook_suppressed = false;
        bool runtime_observation_suppressed = false;
        bool runtime_write_suppressed = false;
        bool install_result_consumption_gate_ready = false;
        std::string action;
        std::string short_circuit_reason;
    };

    struct ChangeLevelPlayerTransferCheckpointSignalHookInstallResultConsumptionStateSummary
    {
        bool attempted = false;
        bool prepared = false;
        bool skipped = false;
        std::string decision_source;
        std::string apply_target;
        std::string current_map;
        std::string requested_map;
        std::string future_apply_phase;
        std::string target_runtime_checkpoint;
        std::string required_signal;
        std::string observation_mode;
        std::string runtime_hook_point;
        std::string registration_state;
        bool install_token_issued = false;
        std::string token_issue_decision;
        bool token_issue_allowed = false;
        std::string authorization_state;
        bool install_authorization_granted = false;
        std::string install_execution_outcome;
        std::string result_state;
        bool result_produced = false;
        bool result_consumable = false;
        bool result_consumption_allowed = false;
        bool result_consumption_deferred = false;
        bool result_consumption_attempted = false;
        bool result_consumed = false;
        std::string result_consumption_state;
        std::string result_consumption_state_reason;
        std::string completion_state;
        bool hook_install_authorized = false;
        bool hook_installed = false;
        bool signal_observed = false;
        bool checkpoint_satisfied = false;
        bool gate_open = false;
        bool deferred = false;
        bool runtime_hook_result_consumption_state_suppressed = false;
        bool runtime_hook_result_consumption_suppressed = false;
        bool runtime_hook_result_state_suppressed = false;
        bool runtime_hook_outcome_suppressed = false;
        bool runtime_hook_execution_suppressed = false;
        bool runtime_hook_attempt_suppressed = false;
        bool runtime_hook_authorization_suppressed = false;
        bool runtime_hook_installation_suppressed = false;
        bool runtime_hook_registration_suppressed = false;
        bool runtime_hook_suppressed = false;
        bool runtime_observation_suppressed = false;
        bool runtime_write_suppressed = false;
        bool install_result_consumption_state_ready = false;
        std::string action;
        std::string short_circuit_reason;
    };

    struct ChangeLevelPlayerTransferCheckpointSignalHookInstallResultConsumptionOutcomeSummary
    {
        bool attempted = false;
        bool prepared = false;
        bool skipped = false;
        std::string decision_source;
        std::string apply_target;
        std::string current_map;
        std::string requested_map;
        std::string future_apply_phase;
        std::string target_runtime_checkpoint;
        std::string required_signal;
        std::string observation_mode;
        std::string runtime_hook_point;
        std::string registration_state;
        bool install_token_issued = false;
        std::string token_issue_decision;
        bool token_issue_allowed = false;
        std::string authorization_state;
        bool install_authorization_granted = false;
        std::string install_execution_outcome;
        std::string result_state;
        bool result_produced = false;
        bool result_consumable = false;
        bool result_consumption_allowed = false;
        bool result_consumption_deferred = false;
        bool result_consumption_attempted = false;
        bool result_consumed = false;
        std::string result_consumption_state;
        bool result_consumption_completed = false;
        bool result_consumption_succeeded = false;
        std::string result_consumption_outcome;
        bool result_consumption_outcome_available = false;
        std::string result_consumption_outcome_reason;
        std::string completion_state;
        bool hook_install_authorized = false;
        bool hook_installed = false;
        bool signal_observed = false;
        bool checkpoint_satisfied = false;
        bool gate_open = false;
        bool deferred = false;
        bool runtime_hook_result_consumption_outcome_suppressed = false;
        bool runtime_hook_result_consumption_state_suppressed = false;
        bool runtime_hook_result_consumption_suppressed = false;
        bool runtime_hook_result_state_suppressed = false;
        bool runtime_hook_outcome_suppressed = false;
        bool runtime_hook_execution_suppressed = false;
        bool runtime_hook_attempt_suppressed = false;
        bool runtime_hook_authorization_suppressed = false;
        bool runtime_hook_installation_suppressed = false;
        bool runtime_hook_registration_suppressed = false;
        bool runtime_hook_suppressed = false;
        bool runtime_observation_suppressed = false;
        bool runtime_write_suppressed = false;
        bool install_result_consumption_outcome_ready = false;
        std::string action;
        std::string short_circuit_reason;
    };

    struct ChangeLevelPlayerTransferCheckpointSignalHookInstallResultConsumerContractSummary
    {
        bool attempted = false;
        bool prepared = false;
        bool skipped = false;
        std::string decision_source;
        std::string apply_target;
        std::string current_map;
        std::string requested_map;
        std::string future_apply_phase;
        std::string target_runtime_checkpoint;
        std::string required_signal;
        std::string observation_mode;
        std::string runtime_hook_point;
        std::string registration_state;
        bool install_token_issued = false;
        std::string token_issue_decision;
        bool token_issue_allowed = false;
        std::string authorization_state;
        bool install_authorization_granted = false;
        std::string install_execution_outcome;
        std::string result_state;
        bool result_produced = false;
        bool result_consumable = false;
        std::string result_consumption_outcome;
        bool result_consumption_outcome_available = false;
        bool result_consumer_contract_defined = false;
        bool result_consumer_available = false;
        bool result_consumer_allowed = false;
        std::string result_consumer_reason;
        std::string completion_state;
        bool hook_install_authorized = false;
        bool hook_installed = false;
        bool signal_observed = false;
        bool checkpoint_satisfied = false;
        bool gate_open = false;
        bool deferred = false;
        bool runtime_hook_result_consumer_suppressed = false;
        bool runtime_hook_result_consumption_outcome_suppressed = false;
        bool runtime_hook_result_consumption_state_suppressed = false;
        bool runtime_hook_result_consumption_suppressed = false;
        bool runtime_hook_result_state_suppressed = false;
        bool runtime_hook_outcome_suppressed = false;
        bool runtime_hook_execution_suppressed = false;
        bool runtime_hook_attempt_suppressed = false;
        bool runtime_hook_authorization_suppressed = false;
        bool runtime_hook_installation_suppressed = false;
        bool runtime_hook_registration_suppressed = false;
        bool runtime_hook_suppressed = false;
        bool runtime_observation_suppressed = false;
        bool runtime_write_suppressed = false;
        bool install_result_consumer_contract_ready = false;
        std::string action;
        std::string short_circuit_reason;
    };

    struct ChangeLevelPlayerTransferCheckpointSignalHookInstallResultConsumerReadinessStateSummary
    {
        bool attempted = false;
        bool prepared = false;
        bool skipped = false;
        std::string decision_source;
        std::string apply_target;
        std::string current_map;
        std::string requested_map;
        std::string future_apply_phase;
        std::string target_runtime_checkpoint;
        std::string required_signal;
        std::string observation_mode;
        std::string runtime_hook_point;
        std::string registration_state;
        bool install_token_issued = false;
        std::string token_issue_decision;
        bool token_issue_allowed = false;
        std::string authorization_state;
        bool install_authorization_granted = false;
        std::string install_execution_outcome;
        std::string result_state;
        bool result_produced = false;
        bool result_consumable = false;
        std::string result_consumption_outcome;
        bool result_consumption_outcome_available = false;
        bool result_consumer_contract_defined = false;
        bool result_consumer_available = false;
        bool result_consumer_allowed = false;
        bool result_consumer_ready = false;
        bool result_consumer_deferred = false;
        std::string result_consumer_readiness_state;
        std::string result_consumer_readiness_reason;
        std::string completion_state;
        bool hook_install_authorized = false;
        bool hook_installed = false;
        bool signal_observed = false;
        bool checkpoint_satisfied = false;
        bool gate_open = false;
        bool deferred = false;
        bool runtime_hook_result_consumer_readiness_suppressed = false;
        bool runtime_hook_result_consumer_suppressed = false;
        bool runtime_hook_result_consumption_outcome_suppressed = false;
        bool runtime_hook_result_consumption_state_suppressed = false;
        bool runtime_hook_result_consumption_suppressed = false;
        bool runtime_hook_result_state_suppressed = false;
        bool runtime_hook_outcome_suppressed = false;
        bool runtime_hook_execution_suppressed = false;
        bool runtime_hook_attempt_suppressed = false;
        bool runtime_hook_authorization_suppressed = false;
        bool runtime_hook_installation_suppressed = false;
        bool runtime_hook_registration_suppressed = false;
        bool runtime_hook_suppressed = false;
        bool runtime_observation_suppressed = false;
        bool runtime_write_suppressed = false;
        bool install_result_consumer_readiness_ready = false;
        std::string action;
        std::string short_circuit_reason;
    };

    struct ChangeLevelPlayerTransferCheckpointSignalHookInstallResultConsumerActivationGateSummary
    {
        bool attempted = false;
        bool prepared = false;
        bool skipped = false;
        std::string decision_source;
        std::string apply_target;
        std::string current_map;
        std::string requested_map;
        std::string future_apply_phase;
        std::string target_runtime_checkpoint;
        std::string required_signal;
        std::string observation_mode;
        std::string runtime_hook_point;
        std::string registration_state;
        bool install_token_issued = false;
        std::string token_issue_decision;
        bool token_issue_allowed = false;
        std::string authorization_state;
        bool install_authorization_granted = false;
        std::string install_execution_outcome;
        std::string result_state;
        bool result_produced = false;
        bool result_consumable = false;
        std::string result_consumption_outcome;
        bool result_consumption_outcome_available = false;
        bool result_consumer_contract_defined = false;
        bool result_consumer_available = false;
        bool result_consumer_allowed = false;
        bool result_consumer_ready = false;
        bool result_consumer_activation_allowed = false;
        bool result_consumer_activation_deferred = false;
        std::string result_consumer_activation_gate;
        std::string result_consumer_activation_reason;
        std::string completion_state;
        bool hook_install_authorized = false;
        bool hook_installed = false;
        bool signal_observed = false;
        bool checkpoint_satisfied = false;
        bool gate_open = false;
        bool deferred = false;
        bool runtime_hook_result_consumer_activation_suppressed = false;
        bool runtime_hook_result_consumer_readiness_suppressed = false;
        bool runtime_hook_result_consumer_suppressed = false;
        bool runtime_hook_result_consumption_outcome_suppressed = false;
        bool runtime_hook_result_consumption_state_suppressed = false;
        bool runtime_hook_result_consumption_suppressed = false;
        bool runtime_hook_result_state_suppressed = false;
        bool runtime_hook_outcome_suppressed = false;
        bool runtime_hook_execution_suppressed = false;
        bool runtime_hook_attempt_suppressed = false;
        bool runtime_hook_authorization_suppressed = false;
        bool runtime_hook_installation_suppressed = false;
        bool runtime_hook_registration_suppressed = false;
        bool runtime_hook_suppressed = false;
        bool runtime_observation_suppressed = false;
        bool runtime_write_suppressed = false;
        bool install_result_consumer_activation_gate_ready = false;
        std::string action;
        std::string short_circuit_reason;
    };

    struct ChangeLevelPlayerTransferCheckpointSignalHookInstallResultConsumerActivationStateSummary
    {
        bool attempted = false;
        bool prepared = false;
        bool skipped = false;
        std::string decision_source;
        std::string apply_target;
        std::string current_map;
        std::string requested_map;
        std::string future_apply_phase;
        std::string target_runtime_checkpoint;
        std::string required_signal;
        std::string observation_mode;
        std::string runtime_hook_point;
        std::string registration_state;
        bool install_token_issued = false;
        std::string token_issue_decision;
        bool token_issue_allowed = false;
        std::string authorization_state;
        bool install_authorization_granted = false;
        std::string install_execution_outcome;
        std::string result_state;
        bool result_produced = false;
        bool result_consumable = false;
        std::string result_consumption_outcome;
        bool result_consumption_outcome_available = false;
        bool result_consumer_contract_defined = false;
        bool result_consumer_available = false;
        bool result_consumer_allowed = false;
        bool result_consumer_ready = false;
        bool result_consumer_activation_allowed = false;
        bool result_consumer_activation_deferred = false;
        std::string result_consumer_activation_gate;
        bool result_consumer_activation_attempted = false;
        bool result_consumer_activated = false;
        std::string result_consumer_activation_state;
        std::string result_consumer_activation_state_reason;
        std::string completion_state;
        bool hook_install_authorized = false;
        bool hook_installed = false;
        bool signal_observed = false;
        bool checkpoint_satisfied = false;
        bool gate_open = false;
        bool deferred = false;
        bool runtime_hook_result_consumer_activation_state_suppressed = false;
        bool runtime_hook_result_consumer_activation_suppressed = false;
        bool runtime_hook_result_consumer_readiness_suppressed = false;
        bool runtime_hook_result_consumer_suppressed = false;
        bool runtime_hook_result_consumption_outcome_suppressed = false;
        bool runtime_hook_result_consumption_state_suppressed = false;
        bool runtime_hook_result_consumption_suppressed = false;
        bool runtime_hook_result_state_suppressed = false;
        bool runtime_hook_outcome_suppressed = false;
        bool runtime_hook_execution_suppressed = false;
        bool runtime_hook_attempt_suppressed = false;
        bool runtime_hook_authorization_suppressed = false;
        bool runtime_hook_installation_suppressed = false;
        bool runtime_hook_registration_suppressed = false;
        bool runtime_hook_suppressed = false;
        bool runtime_observation_suppressed = false;
        bool runtime_write_suppressed = false;
        bool install_result_consumer_activation_state_ready = false;
        std::string action;
        std::string short_circuit_reason;
    };

    struct ChangeLevelPlayerTransferCheckpointSignalHookInstallResultConsumerActivationOutcomeSummary
    {
        bool attempted = false;
        bool prepared = false;
        bool skipped = false;
        std::string decision_source;
        std::string apply_target;
        std::string current_map;
        std::string requested_map;
        std::string future_apply_phase;
        std::string target_runtime_checkpoint;
        std::string required_signal;
        std::string observation_mode;
        std::string runtime_hook_point;
        std::string registration_state;
        bool install_token_issued = false;
        std::string token_issue_decision;
        bool token_issue_allowed = false;
        std::string authorization_state;
        bool install_authorization_granted = false;
        std::string install_execution_outcome;
        std::string result_state;
        bool result_produced = false;
        bool result_consumable = false;
        std::string result_consumption_outcome;
        bool result_consumption_outcome_available = false;
        bool result_consumer_contract_defined = false;
        bool result_consumer_available = false;
        bool result_consumer_allowed = false;
        bool result_consumer_ready = false;
        bool result_consumer_activation_allowed = false;
        bool result_consumer_activation_deferred = false;
        std::string result_consumer_activation_gate;
        bool result_consumer_activation_attempted = false;
        bool result_consumer_activated = false;
        bool result_consumer_activation_completed = false;
        bool result_consumer_activation_succeeded = false;
        std::string result_consumer_activation_outcome;
        bool result_consumer_activation_outcome_available = false;
        std::string result_consumer_activation_outcome_reason;
        std::string completion_state;
        bool hook_install_authorized = false;
        bool hook_installed = false;
        bool signal_observed = false;
        bool checkpoint_satisfied = false;
        bool gate_open = false;
        bool deferred = false;
        bool runtime_hook_result_consumer_activation_outcome_suppressed = false;
        bool runtime_hook_result_consumer_activation_state_suppressed = false;
        bool runtime_hook_result_consumer_activation_suppressed = false;
        bool runtime_hook_result_consumer_readiness_suppressed = false;
        bool runtime_hook_result_consumer_suppressed = false;
        bool runtime_hook_result_consumption_outcome_suppressed = false;
        bool runtime_hook_result_consumption_state_suppressed = false;
        bool runtime_hook_result_consumption_suppressed = false;
        bool runtime_hook_result_state_suppressed = false;
        bool runtime_hook_outcome_suppressed = false;
        bool runtime_hook_execution_suppressed = false;
        bool runtime_hook_attempt_suppressed = false;
        bool runtime_hook_authorization_suppressed = false;
        bool runtime_hook_installation_suppressed = false;
        bool runtime_hook_registration_suppressed = false;
        bool runtime_hook_suppressed = false;
        bool runtime_observation_suppressed = false;
        bool runtime_write_suppressed = false;
        bool install_result_consumer_activation_outcome_ready = false;
        std::string action;
        std::string short_circuit_reason;
    };

    struct ChangeLevelPlayerTransferCheckpointSignalHookInstallResultConsumerBindingContractSummary
    {
        bool attempted = false;
        bool prepared = false;
        bool skipped = false;
        std::string decision_source;
        std::string apply_target;
        std::string current_map;
        std::string requested_map;
        std::string future_apply_phase;
        std::string target_runtime_checkpoint;
        std::string required_signal;
        std::string observation_mode;
        std::string runtime_hook_point;
        std::string registration_state;
        bool install_token_issued = false;
        std::string token_issue_decision;
        bool token_issue_allowed = false;
        std::string authorization_state;
        bool install_authorization_granted = false;
        std::string install_execution_outcome;
        std::string result_state;
        bool result_produced = false;
        bool result_consumable = false;
        std::string result_consumption_outcome;
        bool result_consumption_outcome_available = false;
        bool result_consumer_contract_defined = false;
        bool result_consumer_available = false;
        bool result_consumer_allowed = false;
        bool result_consumer_ready = false;
        bool result_consumer_activated = false;
        std::string result_consumer_activation_outcome;
        bool result_consumer_activation_outcome_available = false;
        bool result_binding_contract_defined = false;
        bool result_binding_available = false;
        bool result_binding_allowed = false;
        std::string result_binding_reason;
        std::string completion_state;
        bool hook_install_authorized = false;
        bool hook_installed = false;
        bool signal_observed = false;
        bool checkpoint_satisfied = false;
        bool gate_open = false;
        bool deferred = false;
        bool runtime_hook_result_binding_suppressed = false;
        bool runtime_hook_result_consumer_activation_outcome_suppressed = false;
        bool runtime_hook_result_consumer_activation_state_suppressed = false;
        bool runtime_hook_result_consumer_activation_suppressed = false;
        bool runtime_hook_result_consumer_readiness_suppressed = false;
        bool runtime_hook_result_consumer_suppressed = false;
        bool runtime_hook_result_consumption_outcome_suppressed = false;
        bool runtime_hook_result_consumption_state_suppressed = false;
        bool runtime_hook_result_consumption_suppressed = false;
        bool runtime_hook_result_state_suppressed = false;
        bool runtime_hook_outcome_suppressed = false;
        bool runtime_hook_execution_suppressed = false;
        bool runtime_hook_attempt_suppressed = false;
        bool runtime_hook_authorization_suppressed = false;
        bool runtime_hook_installation_suppressed = false;
        bool runtime_hook_registration_suppressed = false;
        bool runtime_hook_suppressed = false;
        bool runtime_observation_suppressed = false;
        bool runtime_write_suppressed = false;
        bool install_result_consumer_binding_contract_ready = false;
        std::string action;
        std::string short_circuit_reason;
    };

    struct ChangeLevelPlayerTransferCheckpointSignalHookInstallResultConsumerBindingReadinessStateSummary
    {
        bool attempted = false;
        bool prepared = false;
        bool skipped = false;
        std::string decision_source;
        std::string apply_target;
        std::string current_map;
        std::string requested_map;
        std::string future_apply_phase;
        std::string target_runtime_checkpoint;
        std::string required_signal;
        std::string observation_mode;
        std::string runtime_hook_point;
        std::string registration_state;
        bool install_token_issued = false;
        std::string token_issue_decision;
        bool token_issue_allowed = false;
        std::string authorization_state;
        bool install_authorization_granted = false;
        std::string install_execution_outcome;
        std::string result_state;
        bool result_produced = false;
        bool result_consumable = false;
        std::string result_consumption_outcome;
        bool result_consumption_outcome_available = false;
        bool result_consumer_contract_defined = false;
        bool result_consumer_available = false;
        bool result_consumer_allowed = false;
        bool result_consumer_ready = false;
        bool result_consumer_activated = false;
        std::string result_consumer_activation_outcome;
        bool result_consumer_activation_outcome_available = false;
        bool result_binding_contract_defined = false;
        bool result_binding_available = false;
        bool result_binding_allowed = false;
        bool result_binding_ready = false;
        bool result_binding_deferred = false;
        std::string result_binding_readiness_state;
        std::string result_binding_readiness_reason;
        std::string completion_state;
        bool hook_install_authorized = false;
        bool hook_installed = false;
        bool signal_observed = false;
        bool checkpoint_satisfied = false;
        bool gate_open = false;
        bool deferred = false;
        bool runtime_hook_result_binding_readiness_suppressed = false;
        bool runtime_hook_result_binding_suppressed = false;
        bool runtime_hook_result_consumer_activation_outcome_suppressed = false;
        bool runtime_hook_result_consumer_activation_state_suppressed = false;
        bool runtime_hook_result_consumer_activation_suppressed = false;
        bool runtime_hook_result_consumer_readiness_suppressed = false;
        bool runtime_hook_result_consumer_suppressed = false;
        bool runtime_hook_result_consumption_outcome_suppressed = false;
        bool runtime_hook_result_consumption_state_suppressed = false;
        bool runtime_hook_result_consumption_suppressed = false;
        bool runtime_hook_result_state_suppressed = false;
        bool runtime_hook_outcome_suppressed = false;
        bool runtime_hook_execution_suppressed = false;
        bool runtime_hook_attempt_suppressed = false;
        bool runtime_hook_authorization_suppressed = false;
        bool runtime_hook_installation_suppressed = false;
        bool runtime_hook_registration_suppressed = false;
        bool runtime_hook_suppressed = false;
        bool runtime_observation_suppressed = false;
        bool runtime_write_suppressed = false;
        bool install_result_binding_readiness_ready = false;
        std::string action;
        std::string short_circuit_reason;
    };

    struct ChangeLevelPlayerTransferCheckpointSignalHookInstallResultConsumerBindingActivationGateSummary
    {
        bool attempted = false;
        bool prepared = false;
        bool skipped = false;
        std::string decision_source;
        std::string apply_target;
        std::string current_map;
        std::string requested_map;
        std::string future_apply_phase;
        std::string target_runtime_checkpoint;
        std::string required_signal;
        std::string observation_mode;
        std::string runtime_hook_point;
        std::string registration_state;
        bool install_token_issued = false;
        std::string token_issue_decision;
        bool token_issue_allowed = false;
        std::string authorization_state;
        bool install_authorization_granted = false;
        std::string install_execution_outcome;
        std::string result_state;
        bool result_produced = false;
        bool result_consumable = false;
        std::string result_consumption_outcome;
        bool result_consumption_outcome_available = false;
        bool result_consumer_contract_defined = false;
        bool result_consumer_available = false;
        bool result_consumer_allowed = false;
        bool result_consumer_ready = false;
        bool result_consumer_activated = false;
        std::string result_consumer_activation_outcome;
        bool result_consumer_activation_outcome_available = false;
        bool result_binding_contract_defined = false;
        bool result_binding_available = false;
        bool result_binding_allowed = false;
        bool result_binding_ready = false;
        bool result_binding_activation_allowed = false;
        bool result_binding_activation_deferred = false;
        std::string result_binding_activation_gate;
        std::string result_binding_activation_reason;
        std::string completion_state;
        bool hook_install_authorized = false;
        bool hook_installed = false;
        bool signal_observed = false;
        bool checkpoint_satisfied = false;
        bool gate_open = false;
        bool deferred = false;
        bool runtime_hook_result_binding_activation_suppressed = false;
        bool runtime_hook_result_binding_readiness_suppressed = false;
        bool runtime_hook_result_binding_suppressed = false;
        bool runtime_hook_result_consumer_activation_outcome_suppressed = false;
        bool runtime_hook_result_consumer_activation_state_suppressed = false;
        bool runtime_hook_result_consumer_activation_suppressed = false;
        bool runtime_hook_result_consumer_readiness_suppressed = false;
        bool runtime_hook_result_consumer_suppressed = false;
        bool runtime_hook_result_consumption_outcome_suppressed = false;
        bool runtime_hook_result_consumption_state_suppressed = false;
        bool runtime_hook_result_consumption_suppressed = false;
        bool runtime_hook_result_state_suppressed = false;
        bool runtime_hook_outcome_suppressed = false;
        bool runtime_hook_execution_suppressed = false;
        bool runtime_hook_attempt_suppressed = false;
        bool runtime_hook_authorization_suppressed = false;
        bool runtime_hook_installation_suppressed = false;
        bool runtime_hook_registration_suppressed = false;
        bool runtime_hook_suppressed = false;
        bool runtime_observation_suppressed = false;
        bool runtime_write_suppressed = false;
        bool install_result_binding_activation_gate_ready = false;
        std::string action;
        std::string short_circuit_reason;
    };

    struct ChangeLevelPlayerTransferCheckpointSignalHookInstallResultConsumerBindingActivationStateSummary
    {
        bool attempted = false;
        bool prepared = false;
        bool skipped = false;
        std::string decision_source;
        std::string apply_target;
        std::string current_map;
        std::string requested_map;
        std::string future_apply_phase;
        std::string target_runtime_checkpoint;
        std::string required_signal;
        std::string observation_mode;
        std::string runtime_hook_point;
        std::string registration_state;
        bool install_token_issued = false;
        std::string token_issue_decision;
        bool token_issue_allowed = false;
        std::string authorization_state;
        bool install_authorization_granted = false;
        std::string install_execution_outcome;
        std::string result_state;
        bool result_produced = false;
        bool result_consumable = false;
        std::string result_consumption_outcome;
        bool result_consumption_outcome_available = false;
        bool result_consumer_contract_defined = false;
        bool result_consumer_available = false;
        bool result_consumer_allowed = false;
        bool result_consumer_ready = false;
        bool result_consumer_activated = false;
        std::string result_consumer_activation_outcome;
        bool result_consumer_activation_outcome_available = false;
        bool result_binding_contract_defined = false;
        bool result_binding_available = false;
        bool result_binding_allowed = false;
        bool result_binding_ready = false;
        bool result_binding_activation_allowed = false;
        bool result_binding_activation_deferred = false;
        std::string result_binding_activation_gate;
        bool result_binding_activation_attempted = false;
        bool result_binding_activated = false;
        std::string result_binding_activation_state;
        std::string result_binding_activation_state_reason;
        std::string completion_state;
        bool hook_install_authorized = false;
        bool hook_installed = false;
        bool signal_observed = false;
        bool checkpoint_satisfied = false;
        bool gate_open = false;
        bool deferred = false;
        bool runtime_hook_result_binding_activation_state_suppressed = false;
        bool runtime_hook_result_binding_activation_suppressed = false;
        bool runtime_hook_result_binding_readiness_suppressed = false;
        bool runtime_hook_result_binding_suppressed = false;
        bool runtime_hook_result_consumer_activation_outcome_suppressed = false;
        bool runtime_hook_result_consumer_activation_state_suppressed = false;
        bool runtime_hook_result_consumer_activation_suppressed = false;
        bool runtime_hook_result_consumer_readiness_suppressed = false;
        bool runtime_hook_result_consumer_suppressed = false;
        bool runtime_hook_result_consumption_outcome_suppressed = false;
        bool runtime_hook_result_consumption_state_suppressed = false;
        bool runtime_hook_result_consumption_suppressed = false;
        bool runtime_hook_result_state_suppressed = false;
        bool runtime_hook_outcome_suppressed = false;
        bool runtime_hook_execution_suppressed = false;
        bool runtime_hook_attempt_suppressed = false;
        bool runtime_hook_authorization_suppressed = false;
        bool runtime_hook_installation_suppressed = false;
        bool runtime_hook_registration_suppressed = false;
        bool runtime_hook_suppressed = false;
        bool runtime_observation_suppressed = false;
        bool runtime_write_suppressed = false;
        bool install_result_binding_activation_state_ready = false;
        std::string action;
        std::string short_circuit_reason;
    };

    struct ChangeLevelPlayerTransferCheckpointSignalHookInstallResultConsumerBindingActivationOutcomeSummary
    {
        bool attempted = false;
        bool prepared = false;
        bool skipped = false;
        std::string decision_source;
        std::string apply_target;
        std::string current_map;
        std::string requested_map;
        std::string future_apply_phase;
        std::string target_runtime_checkpoint;
        std::string required_signal;
        std::string observation_mode;
        std::string runtime_hook_point;
        std::string registration_state;
        bool install_token_issued = false;
        std::string token_issue_decision;
        bool token_issue_allowed = false;
        std::string authorization_state;
        bool install_authorization_granted = false;
        std::string install_execution_outcome;
        std::string result_state;
        bool result_produced = false;
        bool result_consumable = false;
        std::string result_consumption_outcome;
        bool result_consumption_outcome_available = false;
        bool result_consumer_contract_defined = false;
        bool result_consumer_available = false;
        bool result_consumer_allowed = false;
        bool result_consumer_ready = false;
        bool result_consumer_activated = false;
        std::string result_consumer_activation_outcome;
        bool result_consumer_activation_outcome_available = false;
        bool result_binding_contract_defined = false;
        bool result_binding_available = false;
        bool result_binding_allowed = false;
        bool result_binding_ready = false;
        bool result_binding_activation_allowed = false;
        bool result_binding_activation_deferred = false;
        std::string result_binding_activation_gate;
        bool result_binding_activation_attempted = false;
        bool result_binding_activated = false;
        bool result_binding_activation_completed = false;
        bool result_binding_activation_succeeded = false;
        std::string result_binding_activation_outcome;
        bool result_binding_activation_outcome_available = false;
        std::string result_binding_activation_outcome_reason;
        std::string completion_state;
        bool hook_install_authorized = false;
        bool hook_installed = false;
        bool signal_observed = false;
        bool checkpoint_satisfied = false;
        bool gate_open = false;
        bool deferred = false;
        bool runtime_hook_result_binding_activation_outcome_suppressed = false;
        bool runtime_hook_result_binding_activation_state_suppressed = false;
        bool runtime_hook_result_binding_activation_suppressed = false;
        bool runtime_hook_result_binding_readiness_suppressed = false;
        bool runtime_hook_result_binding_suppressed = false;
        bool runtime_hook_result_consumer_activation_outcome_suppressed = false;
        bool runtime_hook_result_consumer_activation_state_suppressed = false;
        bool runtime_hook_result_consumer_activation_suppressed = false;
        bool runtime_hook_result_consumer_readiness_suppressed = false;
        bool runtime_hook_result_consumer_suppressed = false;
        bool runtime_hook_result_consumption_outcome_suppressed = false;
        bool runtime_hook_result_consumption_state_suppressed = false;
        bool runtime_hook_result_consumption_suppressed = false;
        bool runtime_hook_result_state_suppressed = false;
        bool runtime_hook_outcome_suppressed = false;
        bool runtime_hook_execution_suppressed = false;
        bool runtime_hook_attempt_suppressed = false;
        bool runtime_hook_authorization_suppressed = false;
        bool runtime_hook_installation_suppressed = false;
        bool runtime_hook_registration_suppressed = false;
        bool runtime_hook_suppressed = false;
        bool runtime_observation_suppressed = false;
        bool runtime_write_suppressed = false;
        bool install_result_binding_activation_outcome_ready = false;
        std::string action;
        std::string short_circuit_reason;
    };

    struct ChangeLevelPlayerTransferCheckpointSignalHookInstallResultConsumerBindingOutcomeContractSummary
    {
        bool attempted = false;
        bool prepared = false;
        bool skipped = false;
        std::string decision_source;
        std::string apply_target;
        std::string current_map;
        std::string requested_map;
        std::string future_apply_phase;
        std::string target_runtime_checkpoint;
        std::string required_signal;
        std::string observation_mode;
        std::string runtime_hook_point;
        std::string registration_state;
        bool install_token_issued = false;
        std::string token_issue_decision;
        bool token_issue_allowed = false;
        std::string authorization_state;
        bool install_authorization_granted = false;
        std::string install_execution_outcome;
        std::string result_state;
        bool result_produced = false;
        bool result_consumable = false;
        std::string result_consumption_outcome;
        bool result_consumption_outcome_available = false;
        bool result_consumer_contract_defined = false;
        bool result_consumer_available = false;
        bool result_consumer_allowed = false;
        bool result_consumer_ready = false;
        bool result_consumer_activated = false;
        std::string result_consumer_activation_outcome;
        bool result_consumer_activation_outcome_available = false;
        bool result_binding_contract_defined = false;
        bool result_binding_available = false;
        bool result_binding_allowed = false;
        bool result_binding_ready = false;
        bool result_binding_activated = false;
        std::string result_binding_activation_outcome;
        bool result_binding_activation_outcome_available = false;
        bool result_binding_outcome_contract_defined = false;
        bool result_binding_outcome_available = false;
        bool result_binding_outcome_allowed = false;
        std::string result_binding_outcome_reason;
        std::string completion_state;
        bool hook_install_authorized = false;
        bool hook_installed = false;
        bool signal_observed = false;
        bool checkpoint_satisfied = false;
        bool gate_open = false;
        bool deferred = false;
        bool runtime_hook_result_binding_outcome_suppressed = false;
        bool runtime_hook_result_binding_activation_outcome_suppressed = false;
        bool runtime_hook_result_binding_activation_state_suppressed = false;
        bool runtime_hook_result_binding_activation_suppressed = false;
        bool runtime_hook_result_binding_readiness_suppressed = false;
        bool runtime_hook_result_binding_suppressed = false;
        bool runtime_hook_result_consumer_activation_outcome_suppressed = false;
        bool runtime_hook_result_consumer_activation_state_suppressed = false;
        bool runtime_hook_result_consumer_activation_suppressed = false;
        bool runtime_hook_result_consumer_readiness_suppressed = false;
        bool runtime_hook_result_consumer_suppressed = false;
        bool runtime_hook_result_consumption_outcome_suppressed = false;
        bool runtime_hook_result_consumption_state_suppressed = false;
        bool runtime_hook_result_consumption_suppressed = false;
        bool runtime_hook_result_state_suppressed = false;
        bool runtime_hook_outcome_suppressed = false;
        bool runtime_hook_execution_suppressed = false;
        bool runtime_hook_attempt_suppressed = false;
        bool runtime_hook_authorization_suppressed = false;
        bool runtime_hook_installation_suppressed = false;
        bool runtime_hook_registration_suppressed = false;
        bool runtime_hook_suppressed = false;
        bool runtime_observation_suppressed = false;
        bool runtime_write_suppressed = false;
        bool install_result_binding_outcome_contract_ready = false;
        std::string action;
        std::string short_circuit_reason;
    };

    struct ChangeLevelPlayerTransferCheckpointSignalHookInstallResultConsumerBindingOutcomeReadinessStateSummary
    {
        bool attempted = false;
        bool prepared = false;
        bool skipped = false;
        std::string decision_source;
        std::string apply_target;
        std::string current_map;
        std::string requested_map;
        std::string future_apply_phase;
        std::string target_runtime_checkpoint;
        std::string required_signal;
        std::string observation_mode;
        std::string runtime_hook_point;
        std::string registration_state;
        bool install_token_issued = false;
        std::string token_issue_decision;
        bool token_issue_allowed = false;
        std::string authorization_state;
        bool install_authorization_granted = false;
        std::string install_execution_outcome;
        std::string result_state;
        bool result_produced = false;
        bool result_consumable = false;
        std::string result_consumption_outcome;
        bool result_consumption_outcome_available = false;
        bool result_consumer_contract_defined = false;
        bool result_consumer_available = false;
        bool result_consumer_allowed = false;
        bool result_consumer_ready = false;
        bool result_consumer_activated = false;
        std::string result_consumer_activation_outcome;
        bool result_consumer_activation_outcome_available = false;
        bool result_binding_contract_defined = false;
        bool result_binding_available = false;
        bool result_binding_allowed = false;
        bool result_binding_ready = false;
        bool result_binding_activated = false;
        std::string result_binding_activation_outcome;
        bool result_binding_activation_outcome_available = false;
        bool result_binding_outcome_contract_defined = false;
        bool result_binding_outcome_available = false;
        bool result_binding_outcome_allowed = false;
        bool result_binding_outcome_ready = false;
        bool result_binding_outcome_deferred = false;
        std::string result_binding_outcome_readiness_state;
        std::string result_binding_outcome_readiness_reason;
        std::string completion_state;
        bool hook_install_authorized = false;
        bool hook_installed = false;
        bool signal_observed = false;
        bool checkpoint_satisfied = false;
        bool gate_open = false;
        bool deferred = false;
        bool runtime_hook_result_binding_outcome_readiness_suppressed = false;
        bool runtime_hook_result_binding_outcome_suppressed = false;
        bool runtime_hook_result_binding_activation_outcome_suppressed = false;
        bool runtime_hook_result_binding_activation_state_suppressed = false;
        bool runtime_hook_result_binding_activation_suppressed = false;
        bool runtime_hook_result_binding_readiness_suppressed = false;
        bool runtime_hook_result_binding_suppressed = false;
        bool runtime_hook_result_consumer_activation_outcome_suppressed = false;
        bool runtime_hook_result_consumer_activation_state_suppressed = false;
        bool runtime_hook_result_consumer_activation_suppressed = false;
        bool runtime_hook_result_consumer_readiness_suppressed = false;
        bool runtime_hook_result_consumer_suppressed = false;
        bool runtime_hook_result_consumption_outcome_suppressed = false;
        bool runtime_hook_result_consumption_state_suppressed = false;
        bool runtime_hook_result_consumption_suppressed = false;
        bool runtime_hook_result_state_suppressed = false;
        bool runtime_hook_outcome_suppressed = false;
        bool runtime_hook_execution_suppressed = false;
        bool runtime_hook_attempt_suppressed = false;
        bool runtime_hook_authorization_suppressed = false;
        bool runtime_hook_installation_suppressed = false;
        bool runtime_hook_registration_suppressed = false;
        bool runtime_hook_suppressed = false;
        bool runtime_observation_suppressed = false;
        bool runtime_write_suppressed = false;
        bool install_result_binding_outcome_readiness_ready = false;
        std::string action;
        std::string short_circuit_reason;
    };

    struct ChangeLevelPlayerTransferCheckpointSignalHookInstallResultConsumerBindingOutcomeActivationGateSummary
    {
        bool attempted = false;
        bool prepared = false;
        bool skipped = false;
        std::string decision_source;
        std::string apply_target;
        std::string current_map;
        std::string requested_map;
        std::string future_apply_phase;
        std::string target_runtime_checkpoint;
        std::string required_signal;
        std::string observation_mode;
        std::string runtime_hook_point;
        std::string registration_state;
        bool install_token_issued = false;
        std::string token_issue_decision;
        bool token_issue_allowed = false;
        std::string authorization_state;
        bool install_authorization_granted = false;
        std::string install_execution_outcome;
        std::string result_state;
        bool result_produced = false;
        bool result_consumable = false;
        std::string result_consumption_outcome;
        bool result_consumption_outcome_available = false;
        bool result_consumer_contract_defined = false;
        bool result_consumer_available = false;
        bool result_consumer_allowed = false;
        bool result_consumer_ready = false;
        bool result_consumer_activated = false;
        std::string result_consumer_activation_outcome;
        bool result_consumer_activation_outcome_available = false;
        bool result_binding_contract_defined = false;
        bool result_binding_available = false;
        bool result_binding_allowed = false;
        bool result_binding_ready = false;
        bool result_binding_activated = false;
        std::string result_binding_activation_outcome;
        bool result_binding_activation_outcome_available = false;
        bool result_binding_outcome_contract_defined = false;
        bool result_binding_outcome_available = false;
        bool result_binding_outcome_allowed = false;
        bool result_binding_outcome_ready = false;
        bool result_binding_outcome_activation_allowed = false;
        bool result_binding_outcome_activation_deferred = false;
        std::string result_binding_outcome_activation_gate;
        std::string result_binding_outcome_activation_reason;
        std::string completion_state;
        bool hook_install_authorized = false;
        bool hook_installed = false;
        bool signal_observed = false;
        bool checkpoint_satisfied = false;
        bool gate_open = false;
        bool deferred = false;
        bool runtime_hook_result_binding_outcome_activation_suppressed = false;
        bool runtime_hook_result_binding_outcome_readiness_suppressed = false;
        bool runtime_hook_result_binding_outcome_suppressed = false;
        bool runtime_hook_result_binding_activation_outcome_suppressed = false;
        bool runtime_hook_result_binding_activation_state_suppressed = false;
        bool runtime_hook_result_binding_activation_suppressed = false;
        bool runtime_hook_result_binding_readiness_suppressed = false;
        bool runtime_hook_result_binding_suppressed = false;
        bool runtime_hook_result_consumer_activation_outcome_suppressed = false;
        bool runtime_hook_result_consumer_activation_state_suppressed = false;
        bool runtime_hook_result_consumer_activation_suppressed = false;
        bool runtime_hook_result_consumer_readiness_suppressed = false;
        bool runtime_hook_result_consumer_suppressed = false;
        bool runtime_hook_result_consumption_outcome_suppressed = false;
        bool runtime_hook_result_consumption_state_suppressed = false;
        bool runtime_hook_result_consumption_suppressed = false;
        bool runtime_hook_result_state_suppressed = false;
        bool runtime_hook_outcome_suppressed = false;
        bool runtime_hook_execution_suppressed = false;
        bool runtime_hook_attempt_suppressed = false;
        bool runtime_hook_authorization_suppressed = false;
        bool runtime_hook_installation_suppressed = false;
        bool runtime_hook_registration_suppressed = false;
        bool runtime_hook_suppressed = false;
        bool runtime_observation_suppressed = false;
        bool runtime_write_suppressed = false;
        bool install_result_binding_outcome_activation_gate_ready = false;
        std::string action;
        std::string short_circuit_reason;
    };

    struct ChangeLevelPlayerTransferCheckpointSignalHookInstallResultConsumerBindingOutcomeActivationStateSummary
    {
        bool attempted = false;
        bool prepared = false;
        bool skipped = false;
        std::string decision_source;
        std::string apply_target;
        std::string current_map;
        std::string requested_map;
        std::string future_apply_phase;
        std::string target_runtime_checkpoint;
        std::string required_signal;
        std::string observation_mode;
        std::string runtime_hook_point;
        std::string registration_state;
        bool install_token_issued = false;
        std::string token_issue_decision;
        bool token_issue_allowed = false;
        std::string authorization_state;
        bool install_authorization_granted = false;
        std::string install_execution_outcome;
        std::string result_state;
        bool result_produced = false;
        bool result_consumable = false;
        std::string result_consumption_outcome;
        bool result_consumption_outcome_available = false;
        bool result_consumer_contract_defined = false;
        bool result_consumer_available = false;
        bool result_consumer_allowed = false;
        bool result_consumer_ready = false;
        bool result_consumer_activated = false;
        std::string result_consumer_activation_outcome;
        bool result_consumer_activation_outcome_available = false;
        bool result_binding_contract_defined = false;
        bool result_binding_available = false;
        bool result_binding_allowed = false;
        bool result_binding_ready = false;
        bool result_binding_activated = false;
        std::string result_binding_activation_outcome;
        bool result_binding_activation_outcome_available = false;
        bool result_binding_outcome_contract_defined = false;
        bool result_binding_outcome_available = false;
        bool result_binding_outcome_allowed = false;
        bool result_binding_outcome_ready = false;
        bool result_binding_outcome_activation_allowed = false;
        bool result_binding_outcome_activation_deferred = false;
        std::string result_binding_outcome_activation_gate;
        bool result_binding_outcome_activation_attempted = false;
        bool result_binding_outcome_activated = false;
        std::string result_binding_outcome_activation_state;
        std::string result_binding_outcome_activation_state_reason;
        std::string completion_state;
        bool hook_install_authorized = false;
        bool hook_installed = false;
        bool signal_observed = false;
        bool checkpoint_satisfied = false;
        bool gate_open = false;
        bool deferred = false;
        bool runtime_hook_result_binding_outcome_activation_state_suppressed = false;
        bool runtime_hook_result_binding_outcome_activation_suppressed = false;
        bool runtime_hook_result_binding_outcome_readiness_suppressed = false;
        bool runtime_hook_result_binding_outcome_suppressed = false;
        bool runtime_hook_result_binding_activation_outcome_suppressed = false;
        bool runtime_hook_result_binding_activation_state_suppressed = false;
        bool runtime_hook_result_binding_activation_suppressed = false;
        bool runtime_hook_result_binding_readiness_suppressed = false;
        bool runtime_hook_result_binding_suppressed = false;
        bool runtime_hook_result_consumer_activation_outcome_suppressed = false;
        bool runtime_hook_result_consumer_activation_state_suppressed = false;
        bool runtime_hook_result_consumer_activation_suppressed = false;
        bool runtime_hook_result_consumer_readiness_suppressed = false;
        bool runtime_hook_result_consumer_suppressed = false;
        bool runtime_hook_result_consumption_outcome_suppressed = false;
        bool runtime_hook_result_consumption_state_suppressed = false;
        bool runtime_hook_result_consumption_suppressed = false;
        bool runtime_hook_result_state_suppressed = false;
        bool runtime_hook_outcome_suppressed = false;
        bool runtime_hook_execution_suppressed = false;
        bool runtime_hook_attempt_suppressed = false;
        bool runtime_hook_authorization_suppressed = false;
        bool runtime_hook_installation_suppressed = false;
        bool runtime_hook_registration_suppressed = false;
        bool runtime_hook_suppressed = false;
        bool runtime_observation_suppressed = false;
        bool runtime_write_suppressed = false;
        bool install_result_binding_outcome_activation_state_ready = false;
        std::string action;
        std::string short_circuit_reason;
    };

    struct ChangeLevelPlayerTransferCheckpointSignalHookInstallResultConsumerBindingOutcomeActivationOutcomeSummary
    {
        bool attempted = false;
        bool prepared = false;
        bool skipped = false;
        std::string decision_source;
        std::string apply_target;
        std::string current_map;
        std::string requested_map;
        std::string future_apply_phase;
        std::string target_runtime_checkpoint;
        std::string required_signal;
        std::string observation_mode;
        std::string runtime_hook_point;
        std::string registration_state;
        bool install_token_issued = false;
        std::string token_issue_decision;
        bool token_issue_allowed = false;
        std::string authorization_state;
        bool install_authorization_granted = false;
        std::string install_execution_outcome;
        std::string result_state;
        bool result_produced = false;
        bool result_consumable = false;
        std::string result_consumption_outcome;
        bool result_consumption_outcome_available = false;
        bool result_consumer_contract_defined = false;
        bool result_consumer_available = false;
        bool result_consumer_allowed = false;
        bool result_consumer_ready = false;
        bool result_consumer_activated = false;
        std::string result_consumer_activation_outcome;
        bool result_consumer_activation_outcome_available = false;
        bool result_binding_contract_defined = false;
        bool result_binding_available = false;
        bool result_binding_allowed = false;
        bool result_binding_ready = false;
        bool result_binding_activated = false;
        std::string result_binding_activation_outcome;
        bool result_binding_activation_outcome_available = false;
        bool result_binding_outcome_contract_defined = false;
        bool result_binding_outcome_available = false;
        bool result_binding_outcome_allowed = false;
        bool result_binding_outcome_ready = false;
        bool result_binding_outcome_activation_allowed = false;
        bool result_binding_outcome_activation_deferred = false;
        std::string result_binding_outcome_activation_gate;
        bool result_binding_outcome_activation_attempted = false;
        bool result_binding_outcome_activated = false;
        bool result_binding_outcome_activation_completed = false;
        bool result_binding_outcome_activation_succeeded = false;
        std::string result_binding_outcome_activation_outcome;
        bool result_binding_outcome_activation_outcome_available = false;
        std::string result_binding_outcome_activation_outcome_reason;
        std::string completion_state;
        bool hook_install_authorized = false;
        bool hook_installed = false;
        bool signal_observed = false;
        bool checkpoint_satisfied = false;
        bool gate_open = false;
        bool deferred = false;
        bool runtime_hook_result_binding_outcome_activation_outcome_suppressed = false;
        bool runtime_hook_result_binding_outcome_activation_state_suppressed = false;
        bool runtime_hook_result_binding_outcome_activation_suppressed = false;
        bool runtime_hook_result_binding_outcome_readiness_suppressed = false;
        bool runtime_hook_result_binding_outcome_suppressed = false;
        bool runtime_hook_result_binding_activation_outcome_suppressed = false;
        bool runtime_hook_result_binding_activation_state_suppressed = false;
        bool runtime_hook_result_binding_activation_suppressed = false;
        bool runtime_hook_result_binding_readiness_suppressed = false;
        bool runtime_hook_result_binding_suppressed = false;
        bool runtime_hook_result_consumer_activation_outcome_suppressed = false;
        bool runtime_hook_result_consumer_activation_state_suppressed = false;
        bool runtime_hook_result_consumer_activation_suppressed = false;
        bool runtime_hook_result_consumer_readiness_suppressed = false;
        bool runtime_hook_result_consumer_suppressed = false;
        bool runtime_hook_result_consumption_outcome_suppressed = false;
        bool runtime_hook_result_consumption_state_suppressed = false;
        bool runtime_hook_result_consumption_suppressed = false;
        bool runtime_hook_result_state_suppressed = false;
        bool runtime_hook_outcome_suppressed = false;
        bool runtime_hook_execution_suppressed = false;
        bool runtime_hook_attempt_suppressed = false;
        bool runtime_hook_authorization_suppressed = false;
        bool runtime_hook_installation_suppressed = false;
        bool runtime_hook_registration_suppressed = false;
        bool runtime_hook_suppressed = false;
        bool runtime_observation_suppressed = false;
        bool runtime_write_suppressed = false;
        bool install_result_binding_outcome_activation_outcome_ready = false;
        std::string action;
        std::string short_circuit_reason;
    };

    struct ChangeLevelPlayerTransferRuntimeIntegrationBlockerSnapshotSummary
    {
        bool attempted = false;
        bool prepared = false;
        bool skipped = false;
        std::string decision_source;
        std::string apply_target;
        std::string current_map;
        std::string requested_map;
        std::string future_apply_phase;
        std::string target_runtime_checkpoint;
        bool runtime_integration_candidate = false;
        bool runtime_integration_blocked = false;
        bool runtime_integration_deferred = false;
        std::string blocking_stage;
        std::string blocking_reason;
        std::string next_required_integration;
        bool chain_terminal = false;
        bool runtime_observation_suppressed = false;
        bool runtime_write_suppressed = false;
        bool integration_blocker_snapshot_ready = false;
        std::string action;
        std::string short_circuit_reason;
    };

    struct ChangeLevelPlayerTransferRuntimeCheckpointObservationSummary
    {
        bool attempted = false;
        bool prepared = false;
        bool skipped = false;
        std::string decision_source;
        std::string apply_target;
        std::string current_map;
        std::string requested_map;
        std::string required_runtime_checkpoint;
        std::string runtime_signal_source;
        bool runtime_checkpoint_observed = false;
        bool runtime_signal_backed = false;
        std::string observation_scope;
        bool target_runtime_observed = false;
        bool runtime_integration_candidate = false;
        bool runtime_integration_blocked = false;
        std::string blocking_reason;
        std::string next_required_integration;
        bool runtime_observation_read_only = false;
        bool runtime_write_suppressed = false;
        bool runtime_checkpoint_observation_ready = false;
        std::string action;
        std::string short_circuit_reason;
    };

    struct ChangeLevelPlayerTransferTargetRuntimeCheckpointObservationSummary
    {
        bool attempted = false;
        bool prepared = false;
        bool skipped = false;
        std::string decision_source;
        std::string apply_target;
        std::string current_map;
        std::string requested_map;
        std::string required_runtime_checkpoint;
        std::string runtime_signal_source;
        std::string active_runtime_map;
        std::string target_runtime_map;
        bool runtime_signal_backed = false;
        std::string observation_scope;
        bool current_runtime_checkpoint_observed = false;
        bool target_runtime_available = false;
        bool target_runtime_checkpoint_observed = false;
        bool target_runtime_checkpoint_observable = false;
        bool target_runtime_observation_blocked = false;
        std::string target_runtime_observation_reason;
        std::string next_required_integration;
        bool runtime_observation_read_only = false;
        bool runtime_write_suppressed = false;
        bool target_runtime_checkpoint_observation_ready = false;
        std::string action;
        std::string short_circuit_reason;
    };

    struct ChangeLevelPlayerTransferTargetRuntimeMaterializationPlanSummary
    {
        bool attempted = false;
        bool prepared = false;
        bool skipped = false;
        std::string decision_source;
        std::string apply_target;
        std::string current_map;
        std::string requested_map;
        std::string active_runtime_map;
        std::string target_runtime_map;
        std::string target_bsp_path;
        std::string landmark;
        bool target_worldspawn_present = false;
        bool target_entity_parse_ok = false;
        bool materialization_candidate = false;
        bool materialization_planned = false;
        bool materialization_execution_suppressed = false;
        std::string materialization_mode;
        bool target_runtime_available = false;
        bool target_runtime_checkpoint_observed = false;
        std::string next_required_integration;
        bool runtime_observation_read_only = false;
        bool runtime_write_suppressed = false;
        bool target_runtime_materialization_plan_ready = false;
        std::string action;
        std::string short_circuit_reason;
    };

    struct ChangeLevelPlayerTransferTargetRuntimeBootstrapExecutionGateSummary
    {
        bool attempted = false;
        bool prepared = false;
        bool skipped = false;
        std::string decision_source;
        std::string apply_target;
        std::string current_map;
        std::string requested_map;
        std::string active_runtime_map;
        std::string target_runtime_map;
        std::string target_bsp_path;
        std::string landmark;
        bool target_worldspawn_present = false;
        bool target_entity_parse_ok = false;
        bool materialization_candidate = false;
        bool materialization_planned = false;
        bool materialization_execution_suppressed = false;
        bool bootstrap_execution_candidate = false;
        bool bootstrap_execution_allowed = false;
        bool bootstrap_execution_deferred = false;
        std::string bootstrap_execution_reason;
        bool map_load_suppressed = false;
        bool bsp_switch_suppressed = false;
        bool runtime_observation_read_only = false;
        bool runtime_write_suppressed = false;
        bool target_runtime_bootstrap_execution_gate_ready = false;
        std::string action;
        std::string short_circuit_reason;
    };

    struct ChangeLevelPlayerTransferTargetRuntimeBootstrapExecutionStateSummary
    {
        bool attempted = false;
        bool prepared = false;
        bool skipped = false;
        std::string decision_source;
        std::string apply_target;
        std::string current_map;
        std::string requested_map;
        std::string active_runtime_map;
        std::string target_runtime_map;
        std::string target_bsp_path;
        std::string landmark;
        bool target_worldspawn_present = false;
        bool target_entity_parse_ok = false;
        bool materialization_candidate = false;
        bool materialization_planned = false;
        bool materialization_execution_suppressed = false;
        bool bootstrap_execution_candidate = false;
        bool bootstrap_execution_allowed = false;
        bool bootstrap_execution_deferred = false;
        bool bootstrap_execution_attempted = false;
        bool bootstrap_execution_completed = false;
        std::string bootstrap_execution_state;
        std::string bootstrap_execution_state_reason;
        bool map_load_suppressed = false;
        bool bsp_switch_suppressed = false;
        bool runtime_observation_read_only = false;
        bool runtime_write_suppressed = false;
        bool target_runtime_bootstrap_execution_state_ready = false;
        std::string action;
        std::string short_circuit_reason;
    };

    struct ChangeLevelPlayerTransferTargetRuntimeBootstrapExecutionOutcomeSummary
    {
        bool attempted = false;
        bool prepared = false;
        bool skipped = false;
        std::string decision_source;
        std::string apply_target;
        std::string current_map;
        std::string requested_map;
        std::string active_runtime_map;
        std::string target_runtime_map;
        std::string target_bsp_path;
        std::string landmark;
        bool target_worldspawn_present = false;
        bool target_entity_parse_ok = false;
        bool materialization_candidate = false;
        bool materialization_planned = false;
        bool materialization_execution_suppressed = false;
        bool bootstrap_execution_candidate = false;
        bool bootstrap_execution_allowed = false;
        bool bootstrap_execution_deferred = false;
        bool bootstrap_execution_attempted = false;
        bool bootstrap_execution_completed = false;
        bool bootstrap_execution_succeeded = false;
        std::string bootstrap_execution_outcome;
        bool bootstrap_execution_outcome_available = false;
        std::string bootstrap_execution_outcome_reason;
        bool map_load_suppressed = false;
        bool bsp_switch_suppressed = false;
        bool runtime_observation_read_only = false;
        bool runtime_write_suppressed = false;
        bool target_runtime_bootstrap_execution_outcome_ready = false;
        std::string action;
        std::string short_circuit_reason;
    };

    struct ChangeLevelPlayerTransferTargetRuntimeBootstrapResultStateSummary
    {
        bool attempted = false;
        bool prepared = false;
        bool skipped = false;
        std::string decision_source;
        std::string apply_target;
        std::string current_map;
        std::string requested_map;
        std::string active_runtime_map;
        std::string target_runtime_map;
        std::string target_bsp_path;
        std::string landmark;
        bool target_worldspawn_present = false;
        bool target_entity_parse_ok = false;
        bool materialization_candidate = false;
        bool materialization_planned = false;
        bool materialization_execution_suppressed = false;
        bool bootstrap_execution_candidate = false;
        bool bootstrap_execution_allowed = false;
        bool bootstrap_execution_deferred = false;
        bool bootstrap_execution_attempted = false;
        bool bootstrap_execution_completed = false;
        bool bootstrap_execution_succeeded = false;
        std::string bootstrap_execution_outcome;
        bool bootstrap_execution_outcome_available = false;
        std::string bootstrap_result_state;
        bool bootstrap_result_produced = false;
        bool bootstrap_result_consumable = false;
        std::string bootstrap_result_reason;
        bool map_load_suppressed = false;
        bool bsp_switch_suppressed = false;
        bool runtime_observation_read_only = false;
        bool runtime_write_suppressed = false;
        bool target_runtime_bootstrap_result_state_ready = false;
        std::string action;
        std::string short_circuit_reason;
    };

    struct ChangeLevelPlayerTransferTargetRuntimeBootstrapResultConsumptionGateSummary
    {
        bool attempted = false;
        bool prepared = false;
        bool skipped = false;
        std::string decision_source;
        std::string apply_target;
        std::string current_map;
        std::string requested_map;
        std::string active_runtime_map;
        std::string target_runtime_map;
        std::string target_bsp_path;
        std::string landmark;
        bool target_worldspawn_present = false;
        bool target_entity_parse_ok = false;
        bool materialization_candidate = false;
        bool materialization_planned = false;
        bool materialization_execution_suppressed = false;
        bool bootstrap_execution_candidate = false;
        bool bootstrap_execution_allowed = false;
        bool bootstrap_execution_deferred = false;
        bool bootstrap_execution_attempted = false;
        bool bootstrap_execution_completed = false;
        bool bootstrap_execution_succeeded = false;
        std::string bootstrap_execution_outcome;
        bool bootstrap_execution_outcome_available = false;
        std::string bootstrap_result_state;
        bool bootstrap_result_produced = false;
        bool bootstrap_result_consumable = false;
        bool bootstrap_result_consumption_allowed = false;
        bool bootstrap_result_consumption_deferred = false;
        std::string bootstrap_result_consumption_reason;
        bool map_load_suppressed = false;
        bool bsp_switch_suppressed = false;
        bool runtime_observation_read_only = false;
        bool runtime_write_suppressed = false;
        bool target_runtime_bootstrap_result_consumption_gate_ready = false;
        std::string action;
        std::string short_circuit_reason;
    };

    struct ChangeLevelPlayerTransferTargetRuntimeBootstrapResultConsumptionStateSummary
    {
        bool attempted = false;
        bool prepared = false;
        bool skipped = false;
        std::string decision_source;
        std::string apply_target;
        std::string current_map;
        std::string requested_map;
        std::string active_runtime_map;
        std::string target_runtime_map;
        std::string target_bsp_path;
        std::string landmark;
        bool target_worldspawn_present = false;
        bool target_entity_parse_ok = false;
        bool materialization_candidate = false;
        bool materialization_planned = false;
        bool materialization_execution_suppressed = false;
        bool bootstrap_execution_candidate = false;
        bool bootstrap_execution_allowed = false;
        bool bootstrap_execution_deferred = false;
        bool bootstrap_execution_attempted = false;
        bool bootstrap_execution_completed = false;
        bool bootstrap_execution_succeeded = false;
        std::string bootstrap_execution_outcome;
        bool bootstrap_execution_outcome_available = false;
        std::string bootstrap_result_state;
        bool bootstrap_result_produced = false;
        bool bootstrap_result_consumable = false;
        bool bootstrap_result_consumption_allowed = false;
        bool bootstrap_result_consumption_deferred = false;
        bool bootstrap_result_consumption_attempted = false;
        bool bootstrap_result_consumed = false;
        std::string bootstrap_result_consumption_state;
        std::string bootstrap_result_consumption_state_reason;
        bool map_load_suppressed = false;
        bool bsp_switch_suppressed = false;
        bool runtime_observation_read_only = false;
        bool runtime_write_suppressed = false;
        bool target_runtime_bootstrap_result_consumption_state_ready = false;
        std::string action;
        std::string short_circuit_reason;
    };

    struct ChangeLevelPlayerTransferTargetRuntimeBootstrapResultConsumptionOutcomeSummary
    {
        bool attempted = false;
        bool prepared = false;
        bool skipped = false;
        std::string decision_source;
        std::string apply_target;
        std::string current_map;
        std::string requested_map;
        std::string active_runtime_map;
        std::string target_runtime_map;
        std::string target_bsp_path;
        std::string landmark;
        bool target_worldspawn_present = false;
        bool target_entity_parse_ok = false;
        bool materialization_candidate = false;
        bool materialization_planned = false;
        bool materialization_execution_suppressed = false;
        bool bootstrap_execution_candidate = false;
        bool bootstrap_execution_allowed = false;
        bool bootstrap_execution_deferred = false;
        bool bootstrap_execution_attempted = false;
        bool bootstrap_execution_completed = false;
        bool bootstrap_execution_succeeded = false;
        std::string bootstrap_execution_outcome;
        bool bootstrap_execution_outcome_available = false;
        std::string bootstrap_result_state;
        bool bootstrap_result_produced = false;
        bool bootstrap_result_consumable = false;
        bool bootstrap_result_consumption_allowed = false;
        bool bootstrap_result_consumption_deferred = false;
        bool bootstrap_result_consumption_attempted = false;
        bool bootstrap_result_consumed = false;
        std::string bootstrap_result_consumption_state;
        bool bootstrap_result_consumption_completed = false;
        bool bootstrap_result_consumption_succeeded = false;
        std::string bootstrap_result_consumption_outcome;
        bool bootstrap_result_consumption_outcome_available = false;
        std::string bootstrap_result_consumption_outcome_reason;
        bool map_load_suppressed = false;
        bool bsp_switch_suppressed = false;
        bool runtime_observation_read_only = false;
        bool runtime_write_suppressed = false;
        bool target_runtime_bootstrap_result_consumption_outcome_ready = false;
        std::string action;
        std::string short_circuit_reason;
    };

    struct ChangeLevelPlayerTransferTargetRuntimeMaterializationGateSummary
    {
        bool attempted = false;
        bool prepared = false;
        bool skipped = false;
        std::string decision_source;
        std::string bootstrap_execution_outcome;
        std::string bootstrap_result_state;
        bool bootstrap_result_produced = false;
        bool bootstrap_result_consumable = false;
        bool bootstrap_result_consumption_allowed = false;
        bool bootstrap_result_consumption_deferred = false;
        bool bootstrap_result_consumption_attempted = false;
        bool bootstrap_result_consumed = false;
        std::string bootstrap_result_consumption_state;
        std::string bootstrap_result_consumption_outcome;
        bool bootstrap_result_consumption_outcome_available = false;
        std::string bootstrap_result_consumption_outcome_reason;
        bool target_runtime_materialization_candidate = false;
        bool target_runtime_materialization_allowed = false;
        bool target_runtime_materialization_deferred = false;
        bool target_runtime_materialization_attempted = false;
        bool target_runtime_materialization_completed = false;
        bool target_runtime_materialization_succeeded = false;
        bool target_runtime_materialization_blocked = false;
        std::string target_runtime_materialization_block_reason;
        bool target_runtime_materialization_ready = false;
        bool target_runtime_materialization_gate_ready = false;
        std::string action;
        std::string short_circuit_reason;
    };

    struct ChangeLevelPlayerTransferTargetRuntimeMaterializationStateSummary
    {
        bool attempted = false;
        bool prepared = false;
        bool skipped = false;
        std::string decision_source;
        std::string bootstrap_execution_outcome;
        std::string bootstrap_result_state;
        bool bootstrap_result_produced = false;
        bool bootstrap_result_consumable = false;
        bool bootstrap_result_consumption_allowed = false;
        bool bootstrap_result_consumption_deferred = false;
        bool bootstrap_result_consumption_attempted = false;
        bool bootstrap_result_consumed = false;
        std::string bootstrap_result_consumption_state;
        std::string bootstrap_result_consumption_outcome;
        bool bootstrap_result_consumption_outcome_available = false;
        std::string bootstrap_result_consumption_outcome_reason;
        bool target_runtime_materialization_candidate = false;
        bool target_runtime_materialization_allowed = false;
        bool target_runtime_materialization_deferred = false;
        bool target_runtime_materialization_attempted = false;
        bool target_runtime_materialization_completed = false;
        bool target_runtime_materialization_succeeded = false;
        bool target_runtime_materialization_blocked = false;
        std::string target_runtime_materialization_block_reason;
        bool target_runtime_materialization_started = false;
        bool target_runtime_materialized = false;
        std::string target_runtime_materialization_state;
        std::string target_runtime_materialization_state_reason;
        bool target_runtime_materialization_ready = false;
        bool target_runtime_materialization_state_ready = false;
        std::string action;
        std::string short_circuit_reason;
    };

    struct ChangeLevelPlayerTransferTargetRuntimeMaterializationExecutionGuardAuditSummary
    {
        bool attempted = false;
        bool prepared = false;
        bool skipped = false;
        std::string decision_source;
        bool target_runtime_materialization_gate_ready = false;
        bool target_runtime_materialization_ready = false;
        bool target_runtime_present = false;
        bool current_runtime_present = false;
        bool bootstrap_result_produced = false;
        bool bootstrap_result_consumable = false;
        bool materialization_outer_guard_satisfied = false;
        bool materialization_inner_guard_satisfied = false;
        bool materialization_path_eligible = false;
        bool materialization_executed = false;
        std::string materialization_path_blocked_by;
        std::string action;
        std::string short_circuit_reason;
    };

    struct ChangeLevelPlayerTransferTargetRuntimeMaterializationOutcomeSummary
    {
        bool attempted = false;
        bool prepared = false;
        bool skipped = false;
        std::string decision_source;
        std::string bootstrap_execution_outcome;
        std::string bootstrap_result_state;
        bool bootstrap_result_produced = false;
        bool bootstrap_result_consumable = false;
        bool bootstrap_result_consumption_allowed = false;
        bool bootstrap_result_consumption_deferred = false;
        bool bootstrap_result_consumption_attempted = false;
        bool bootstrap_result_consumed = false;
        std::string bootstrap_result_consumption_state;
        std::string bootstrap_result_consumption_outcome;
        bool bootstrap_result_consumption_outcome_available = false;
        std::string bootstrap_result_consumption_outcome_reason;
        bool target_runtime_materialization_candidate = false;
        bool target_runtime_materialization_allowed = false;
        bool target_runtime_materialization_deferred = false;
        bool target_runtime_materialization_attempted = false;
        bool target_runtime_materialization_completed = false;
        bool target_runtime_materialization_succeeded = false;
        bool target_runtime_materialization_blocked = false;
        std::string target_runtime_materialization_block_reason;
        bool target_runtime_materialization_started = false;
        bool target_runtime_materialized = false;
        std::string target_runtime_materialization_state;
        std::string target_runtime_materialization_state_reason;
        std::string target_runtime_materialization_outcome;
        bool target_runtime_materialization_outcome_available = false;
        std::string target_runtime_materialization_outcome_reason;
        bool target_runtime_materialization_ready = false;
        bool target_runtime_materialization_outcome_ready = false;
        std::string action;
        std::string short_circuit_reason;
    };

    struct ChangeLevelPlayerTransferTargetRuntimeActivationGateSummary
    {
        bool attempted = false;
        bool prepared = false;
        bool skipped = false;
        std::string decision_source;
        std::string bootstrap_execution_outcome;
        std::string bootstrap_result_state;
        bool bootstrap_result_produced = false;
        bool bootstrap_result_consumable = false;
        bool bootstrap_result_consumption_allowed = false;
        bool bootstrap_result_consumption_deferred = false;
        bool bootstrap_result_consumption_attempted = false;
        bool bootstrap_result_consumed = false;
        std::string bootstrap_result_consumption_state;
        std::string bootstrap_result_consumption_outcome;
        bool bootstrap_result_consumption_outcome_available = false;
        std::string bootstrap_result_consumption_outcome_reason;
        bool target_runtime_materialization_candidate = false;
        bool target_runtime_materialization_allowed = false;
        bool target_runtime_materialization_deferred = false;
        bool target_runtime_materialization_attempted = false;
        bool target_runtime_materialization_completed = false;
        bool target_runtime_materialization_succeeded = false;
        bool target_runtime_materialization_blocked = false;
        std::string target_runtime_materialization_block_reason;
        bool target_runtime_materialization_started = false;
        bool target_runtime_materialized = false;
        std::string target_runtime_materialization_state;
        std::string target_runtime_materialization_state_reason;
        std::string target_runtime_materialization_outcome;
        bool target_runtime_materialization_outcome_available = false;
        std::string target_runtime_materialization_outcome_reason;
        bool target_runtime_activation_candidate = false;
        bool target_runtime_activation_allowed = false;
        bool target_runtime_activation_deferred = false;
        bool target_runtime_activation_attempted = false;
        bool target_runtime_activation_completed = false;
        bool target_runtime_activation_succeeded = false;
        bool target_runtime_activation_blocked = false;
        std::string target_runtime_activation_block_reason;
        bool target_runtime_activation_ready = false;
        bool target_runtime_activation_gate_ready = false;
        std::string action;
        std::string short_circuit_reason;
    };

    struct ChangeLevelPlayerTransferTargetRuntimeActivationStateSummary
    {
        bool attempted = false;
        bool prepared = false;
        bool skipped = false;
        std::string decision_source;
        std::string bootstrap_execution_outcome;
        std::string bootstrap_result_state;
        bool bootstrap_result_produced = false;
        bool bootstrap_result_consumable = false;
        bool bootstrap_result_consumption_allowed = false;
        bool bootstrap_result_consumption_deferred = false;
        bool bootstrap_result_consumption_attempted = false;
        bool bootstrap_result_consumed = false;
        std::string bootstrap_result_consumption_state;
        std::string bootstrap_result_consumption_outcome;
        bool bootstrap_result_consumption_outcome_available = false;
        std::string bootstrap_result_consumption_outcome_reason;
        bool target_runtime_materialization_candidate = false;
        bool target_runtime_materialization_allowed = false;
        bool target_runtime_materialization_deferred = false;
        bool target_runtime_materialization_attempted = false;
        bool target_runtime_materialization_completed = false;
        bool target_runtime_materialization_succeeded = false;
        bool target_runtime_materialization_blocked = false;
        std::string target_runtime_materialization_block_reason;
        bool target_runtime_materialization_started = false;
        bool target_runtime_materialized = false;
        std::string target_runtime_materialization_state;
        std::string target_runtime_materialization_state_reason;
        std::string target_runtime_materialization_outcome;
        bool target_runtime_materialization_outcome_available = false;
        std::string target_runtime_materialization_outcome_reason;
        bool target_runtime_activation_candidate = false;
        bool target_runtime_activation_allowed = false;
        bool target_runtime_activation_deferred = false;
        bool target_runtime_activation_attempted = false;
        bool target_runtime_activation_completed = false;
        bool target_runtime_activation_succeeded = false;
        bool target_runtime_activation_blocked = false;
        std::string target_runtime_activation_block_reason;
        bool target_runtime_activation_started = false;
        bool target_runtime_activated = false;
        std::string target_runtime_activation_state;
        std::string target_runtime_activation_state_reason;
        bool target_runtime_activation_ready = false;
        bool target_runtime_activation_state_ready = false;
        std::string action;
        std::string short_circuit_reason;
    };

    struct ChangeLevelPlayerTransferTargetRuntimeActivationExecutionGuardAuditSummary
    {
        bool attempted = false;
        bool prepared = false;
        bool skipped = false;
        std::string decision_source;
        bool target_runtime_activation_gate_ready = false;
        bool target_runtime_activation_ready = false;
        bool target_runtime_present = false;
        bool target_runtime_materialized = false;
        bool current_runtime_present = false;
        bool activation_outer_guard_satisfied = false;
        bool activation_inner_guard_satisfied = false;
        bool activation_path_eligible = false;
        bool activation_executed = false;
        std::string activation_path_blocked_by;
        std::string action;
        std::string short_circuit_reason;
    };

    struct ChangeLevelPlayerTransferTargetRuntimeActivationOutcomeSummary
    {
        bool attempted = false;
        bool prepared = false;
        bool skipped = false;
        std::string decision_source;
        std::string bootstrap_execution_outcome;
        std::string bootstrap_result_state;
        bool bootstrap_result_produced = false;
        bool bootstrap_result_consumable = false;
        bool bootstrap_result_consumption_allowed = false;
        bool bootstrap_result_consumption_deferred = false;
        bool bootstrap_result_consumption_attempted = false;
        bool bootstrap_result_consumed = false;
        std::string bootstrap_result_consumption_state;
        std::string bootstrap_result_consumption_outcome;
        bool bootstrap_result_consumption_outcome_available = false;
        std::string bootstrap_result_consumption_outcome_reason;
        bool target_runtime_materialization_candidate = false;
        bool target_runtime_materialization_allowed = false;
        bool target_runtime_materialization_deferred = false;
        bool target_runtime_materialization_attempted = false;
        bool target_runtime_materialization_completed = false;
        bool target_runtime_materialization_succeeded = false;
        bool target_runtime_materialization_blocked = false;
        std::string target_runtime_materialization_block_reason;
        bool target_runtime_materialization_started = false;
        bool target_runtime_materialized = false;
        std::string target_runtime_materialization_state;
        std::string target_runtime_materialization_state_reason;
        std::string target_runtime_materialization_outcome;
        bool target_runtime_materialization_outcome_available = false;
        std::string target_runtime_materialization_outcome_reason;
        bool target_runtime_activation_candidate = false;
        bool target_runtime_activation_allowed = false;
        bool target_runtime_activation_deferred = false;
        bool target_runtime_activation_attempted = false;
        bool target_runtime_activation_completed = false;
        bool target_runtime_activation_succeeded = false;
        bool target_runtime_activation_blocked = false;
        std::string target_runtime_activation_block_reason;
        bool target_runtime_activation_started = false;
        bool target_runtime_activated = false;
        std::string target_runtime_activation_state;
        std::string target_runtime_activation_state_reason;
        std::string target_runtime_activation_outcome;
        bool target_runtime_activation_outcome_available = false;
        std::string target_runtime_activation_outcome_reason;
        bool target_runtime_activation_ready = false;
        bool target_runtime_activation_outcome_ready = false;
        std::string action;
        std::string short_circuit_reason;
    };

    struct ChangeLevelPlayerTransferTargetRuntimeCutoverGateSummary
    {
        bool attempted = false;
        bool prepared = false;
        bool skipped = false;
        std::string decision_source;
        std::string bootstrap_execution_outcome;
        std::string bootstrap_result_state;
        bool bootstrap_result_produced = false;
        bool bootstrap_result_consumable = false;
        bool bootstrap_result_consumption_allowed = false;
        bool bootstrap_result_consumption_deferred = false;
        bool bootstrap_result_consumption_attempted = false;
        bool bootstrap_result_consumed = false;
        std::string bootstrap_result_consumption_state;
        std::string bootstrap_result_consumption_outcome;
        bool bootstrap_result_consumption_outcome_available = false;
        std::string bootstrap_result_consumption_outcome_reason;
        bool target_runtime_materialization_candidate = false;
        bool target_runtime_materialization_allowed = false;
        bool target_runtime_materialization_deferred = false;
        bool target_runtime_materialization_attempted = false;
        bool target_runtime_materialization_completed = false;
        bool target_runtime_materialization_succeeded = false;
        bool target_runtime_materialization_blocked = false;
        std::string target_runtime_materialization_block_reason;
        bool target_runtime_materialization_started = false;
        bool target_runtime_materialized = false;
        std::string target_runtime_materialization_state;
        std::string target_runtime_materialization_state_reason;
        std::string target_runtime_materialization_outcome;
        bool target_runtime_materialization_outcome_available = false;
        std::string target_runtime_materialization_outcome_reason;
        bool target_runtime_activation_candidate = false;
        bool target_runtime_activation_allowed = false;
        bool target_runtime_activation_deferred = false;
        bool target_runtime_activation_attempted = false;
        bool target_runtime_activation_completed = false;
        bool target_runtime_activation_succeeded = false;
        bool target_runtime_activation_blocked = false;
        std::string target_runtime_activation_block_reason;
        bool target_runtime_activation_started = false;
        bool target_runtime_activated = false;
        std::string target_runtime_activation_state;
        std::string target_runtime_activation_state_reason;
        std::string target_runtime_activation_outcome;
        bool target_runtime_activation_outcome_available = false;
        std::string target_runtime_activation_outcome_reason;
        bool target_runtime_cutover_candidate = false;
        bool target_runtime_cutover_allowed = false;
        bool target_runtime_cutover_deferred = false;
        bool target_runtime_cutover_attempted = false;
        bool target_runtime_cutover_completed = false;
        bool target_runtime_cutover_succeeded = false;
        bool target_runtime_cutover_blocked = false;
        std::string target_runtime_cutover_block_reason;
        bool target_runtime_cutover_ready = false;
        bool target_runtime_cutover_gate_ready = false;
        std::string action;
        std::string short_circuit_reason;
    };

    struct ChangeLevelPlayerTransferTargetRuntimeCutoverStateSummary
    {
        bool attempted = false;
        bool prepared = false;
        bool skipped = false;
        std::string decision_source;
        std::string bootstrap_execution_outcome;
        std::string bootstrap_result_state;
        bool bootstrap_result_produced = false;
        bool bootstrap_result_consumable = false;
        bool bootstrap_result_consumption_allowed = false;
        bool bootstrap_result_consumption_deferred = false;
        bool bootstrap_result_consumption_attempted = false;
        bool bootstrap_result_consumed = false;
        std::string bootstrap_result_consumption_state;
        std::string bootstrap_result_consumption_outcome;
        bool bootstrap_result_consumption_outcome_available = false;
        std::string bootstrap_result_consumption_outcome_reason;
        bool target_runtime_materialization_candidate = false;
        bool target_runtime_materialization_allowed = false;
        bool target_runtime_materialization_deferred = false;
        bool target_runtime_materialization_attempted = false;
        bool target_runtime_materialization_completed = false;
        bool target_runtime_materialization_succeeded = false;
        bool target_runtime_materialization_blocked = false;
        std::string target_runtime_materialization_block_reason;
        bool target_runtime_materialization_started = false;
        bool target_runtime_materialized = false;
        std::string target_runtime_materialization_state;
        std::string target_runtime_materialization_state_reason;
        std::string target_runtime_materialization_outcome;
        bool target_runtime_materialization_outcome_available = false;
        std::string target_runtime_materialization_outcome_reason;
        bool target_runtime_activation_candidate = false;
        bool target_runtime_activation_allowed = false;
        bool target_runtime_activation_deferred = false;
        bool target_runtime_activation_attempted = false;
        bool target_runtime_activation_completed = false;
        bool target_runtime_activation_succeeded = false;
        bool target_runtime_activation_blocked = false;
        std::string target_runtime_activation_block_reason;
        bool target_runtime_activation_started = false;
        bool target_runtime_activated = false;
        std::string target_runtime_activation_state;
        std::string target_runtime_activation_state_reason;
        std::string target_runtime_activation_outcome;
        bool target_runtime_activation_outcome_available = false;
        std::string target_runtime_activation_outcome_reason;
        bool target_runtime_cutover_candidate = false;
        bool target_runtime_cutover_allowed = false;
        bool target_runtime_cutover_deferred = false;
        bool target_runtime_cutover_attempted = false;
        bool target_runtime_cutover_completed = false;
        bool target_runtime_cutover_succeeded = false;
        bool target_runtime_cutover_blocked = false;
        std::string target_runtime_cutover_block_reason;
        bool target_runtime_cutover_started = false;
        bool target_runtime_cutover_applied = false;
        std::string target_runtime_cutover_state;
        std::string target_runtime_cutover_state_reason;
        bool target_runtime_cutover_ready = false;
        bool target_runtime_cutover_state_ready = false;
        std::string action;
        std::string short_circuit_reason;
    };

    struct ChangeLevelPlayerTransferTargetRuntimeCutoverExecutionGuardAuditSummary
    {
        bool attempted = false;
        bool prepared = false;
        bool skipped = false;
        std::string decision_source;
        bool target_runtime_cutover_gate_ready = false;
        bool target_runtime_cutover_ready = false;
        bool target_runtime_present = false;
        bool target_runtime_activated = false;
        bool current_runtime_present = false;
        bool cutover_outer_guard_satisfied = false;
        bool cutover_inner_guard_satisfied = false;
        bool cutover_path_eligible = false;
        bool cutover_executed = false;
        std::string cutover_path_blocked_by;
        std::string action;
        std::string short_circuit_reason;
    };

    struct ChangeLevelPlayerTransferTargetRuntimeCutoverOutcomeSummary
    {
        bool attempted = false;
        bool prepared = false;
        bool skipped = false;
        std::string decision_source;
        std::string bootstrap_execution_outcome;
        std::string bootstrap_result_state;
        bool bootstrap_result_produced = false;
        bool bootstrap_result_consumable = false;
        bool bootstrap_result_consumption_allowed = false;
        bool bootstrap_result_consumption_deferred = false;
        bool bootstrap_result_consumption_attempted = false;
        bool bootstrap_result_consumed = false;
        std::string bootstrap_result_consumption_state;
        std::string bootstrap_result_consumption_outcome;
        bool bootstrap_result_consumption_outcome_available = false;
        std::string bootstrap_result_consumption_outcome_reason;
        bool target_runtime_materialization_candidate = false;
        bool target_runtime_materialization_allowed = false;
        bool target_runtime_materialization_deferred = false;
        bool target_runtime_materialization_attempted = false;
        bool target_runtime_materialization_completed = false;
        bool target_runtime_materialization_succeeded = false;
        bool target_runtime_materialization_blocked = false;
        std::string target_runtime_materialization_block_reason;
        bool target_runtime_materialization_started = false;
        bool target_runtime_materialized = false;
        std::string target_runtime_materialization_state;
        std::string target_runtime_materialization_state_reason;
        std::string target_runtime_materialization_outcome;
        bool target_runtime_materialization_outcome_available = false;
        std::string target_runtime_materialization_outcome_reason;
        bool target_runtime_activation_candidate = false;
        bool target_runtime_activation_allowed = false;
        bool target_runtime_activation_deferred = false;
        bool target_runtime_activation_attempted = false;
        bool target_runtime_activation_completed = false;
        bool target_runtime_activation_succeeded = false;
        bool target_runtime_activation_blocked = false;
        std::string target_runtime_activation_block_reason;
        bool target_runtime_activation_started = false;
        bool target_runtime_activated = false;
        std::string target_runtime_activation_state;
        std::string target_runtime_activation_state_reason;
        std::string target_runtime_activation_outcome;
        bool target_runtime_activation_outcome_available = false;
        std::string target_runtime_activation_outcome_reason;
        bool target_runtime_cutover_candidate = false;
        bool target_runtime_cutover_allowed = false;
        bool target_runtime_cutover_deferred = false;
        bool target_runtime_cutover_attempted = false;
        bool target_runtime_cutover_completed = false;
        bool target_runtime_cutover_succeeded = false;
        bool target_runtime_cutover_blocked = false;
        std::string target_runtime_cutover_block_reason;
        bool target_runtime_cutover_started = false;
        bool target_runtime_cutover_applied = false;
        std::string target_runtime_cutover_state;
        std::string target_runtime_cutover_state_reason;
        std::string target_runtime_cutover_outcome;
        bool target_runtime_cutover_outcome_available = false;
        std::string target_runtime_cutover_outcome_reason;
        bool target_runtime_cutover_ready = false;
        bool target_runtime_cutover_outcome_ready = false;
        std::string action;
        std::string short_circuit_reason;
    };

    struct ChangeLevelPlayerTransferCurrentRuntimeDeactivationGateSummary
    {
        bool attempted = false;
        bool prepared = false;
        bool skipped = false;
        std::string decision_source;
        std::string bootstrap_execution_outcome;
        std::string bootstrap_result_state;
        bool bootstrap_result_produced = false;
        bool bootstrap_result_consumable = false;
        bool bootstrap_result_consumption_allowed = false;
        bool bootstrap_result_consumption_deferred = false;
        bool bootstrap_result_consumption_attempted = false;
        bool bootstrap_result_consumed = false;
        std::string bootstrap_result_consumption_state;
        std::string bootstrap_result_consumption_outcome;
        bool bootstrap_result_consumption_outcome_available = false;
        std::string bootstrap_result_consumption_outcome_reason;
        bool target_runtime_materialization_candidate = false;
        bool target_runtime_materialization_allowed = false;
        bool target_runtime_materialization_deferred = false;
        bool target_runtime_materialization_attempted = false;
        bool target_runtime_materialization_completed = false;
        bool target_runtime_materialization_succeeded = false;
        bool target_runtime_materialization_blocked = false;
        std::string target_runtime_materialization_block_reason;
        bool target_runtime_materialization_started = false;
        bool target_runtime_materialized = false;
        std::string target_runtime_materialization_state;
        std::string target_runtime_materialization_state_reason;
        std::string target_runtime_materialization_outcome;
        bool target_runtime_materialization_outcome_available = false;
        std::string target_runtime_materialization_outcome_reason;
        bool target_runtime_activation_candidate = false;
        bool target_runtime_activation_allowed = false;
        bool target_runtime_activation_deferred = false;
        bool target_runtime_activation_attempted = false;
        bool target_runtime_activation_completed = false;
        bool target_runtime_activation_succeeded = false;
        bool target_runtime_activation_blocked = false;
        std::string target_runtime_activation_block_reason;
        bool target_runtime_activation_started = false;
        bool target_runtime_activated = false;
        std::string target_runtime_activation_state;
        std::string target_runtime_activation_state_reason;
        std::string target_runtime_activation_outcome;
        bool target_runtime_activation_outcome_available = false;
        std::string target_runtime_activation_outcome_reason;
        bool target_runtime_cutover_candidate = false;
        bool target_runtime_cutover_allowed = false;
        bool target_runtime_cutover_deferred = false;
        bool target_runtime_cutover_attempted = false;
        bool target_runtime_cutover_completed = false;
        bool target_runtime_cutover_succeeded = false;
        bool target_runtime_cutover_blocked = false;
        std::string target_runtime_cutover_block_reason;
        bool target_runtime_cutover_started = false;
        bool target_runtime_cutover_applied = false;
        std::string target_runtime_cutover_state;
        std::string target_runtime_cutover_state_reason;
        std::string target_runtime_cutover_outcome;
        bool target_runtime_cutover_outcome_available = false;
        std::string target_runtime_cutover_outcome_reason;
        bool current_runtime_deactivation_candidate = false;
        bool current_runtime_deactivation_allowed = false;
        bool current_runtime_deactivation_deferred = false;
        bool current_runtime_deactivation_attempted = false;
        bool current_runtime_deactivation_completed = false;
        bool current_runtime_deactivation_succeeded = false;
        bool current_runtime_deactivation_blocked = false;
        std::string current_runtime_deactivation_block_reason;
        bool current_runtime_deactivation_ready = false;
        bool current_runtime_deactivation_gate_ready = false;
        std::string action;
        std::string short_circuit_reason;
    };

    struct ChangeLevelPlayerTransferCurrentRuntimeDeactivationStateSummary
    {
        bool attempted = false;
        bool prepared = false;
        bool skipped = false;
        std::string decision_source;
        std::string bootstrap_execution_outcome;
        std::string bootstrap_result_state;
        bool bootstrap_result_produced = false;
        bool bootstrap_result_consumable = false;
        bool bootstrap_result_consumption_allowed = false;
        bool bootstrap_result_consumption_deferred = false;
        bool bootstrap_result_consumption_attempted = false;
        bool bootstrap_result_consumed = false;
        std::string bootstrap_result_consumption_state;
        std::string bootstrap_result_consumption_outcome;
        bool bootstrap_result_consumption_outcome_available = false;
        std::string bootstrap_result_consumption_outcome_reason;
        bool target_runtime_materialization_candidate = false;
        bool target_runtime_materialization_allowed = false;
        bool target_runtime_materialization_deferred = false;
        bool target_runtime_materialization_attempted = false;
        bool target_runtime_materialization_completed = false;
        bool target_runtime_materialization_succeeded = false;
        bool target_runtime_materialization_blocked = false;
        std::string target_runtime_materialization_block_reason;
        bool target_runtime_materialization_started = false;
        bool target_runtime_materialized = false;
        std::string target_runtime_materialization_state;
        std::string target_runtime_materialization_state_reason;
        std::string target_runtime_materialization_outcome;
        bool target_runtime_materialization_outcome_available = false;
        std::string target_runtime_materialization_outcome_reason;
        bool target_runtime_activation_candidate = false;
        bool target_runtime_activation_allowed = false;
        bool target_runtime_activation_deferred = false;
        bool target_runtime_activation_attempted = false;
        bool target_runtime_activation_completed = false;
        bool target_runtime_activation_succeeded = false;
        bool target_runtime_activation_blocked = false;
        std::string target_runtime_activation_block_reason;
        bool target_runtime_activation_started = false;
        bool target_runtime_activated = false;
        std::string target_runtime_activation_state;
        std::string target_runtime_activation_state_reason;
        std::string target_runtime_activation_outcome;
        bool target_runtime_activation_outcome_available = false;
        std::string target_runtime_activation_outcome_reason;
        bool target_runtime_cutover_candidate = false;
        bool target_runtime_cutover_allowed = false;
        bool target_runtime_cutover_deferred = false;
        bool target_runtime_cutover_attempted = false;
        bool target_runtime_cutover_completed = false;
        bool target_runtime_cutover_succeeded = false;
        bool target_runtime_cutover_blocked = false;
        std::string target_runtime_cutover_block_reason;
        bool target_runtime_cutover_started = false;
        bool target_runtime_cutover_applied = false;
        std::string target_runtime_cutover_state;
        std::string target_runtime_cutover_state_reason;
        std::string target_runtime_cutover_outcome;
        bool target_runtime_cutover_outcome_available = false;
        std::string target_runtime_cutover_outcome_reason;
        bool current_runtime_deactivation_candidate = false;
        bool current_runtime_deactivation_allowed = false;
        bool current_runtime_deactivation_deferred = false;
        bool current_runtime_deactivation_attempted = false;
        bool current_runtime_deactivation_completed = false;
        bool current_runtime_deactivation_succeeded = false;
        bool current_runtime_deactivation_blocked = false;
        std::string current_runtime_deactivation_block_reason;
        bool current_runtime_deactivation_started = false;
        bool current_runtime_deactivated = false;
        std::string current_runtime_deactivation_state;
        std::string current_runtime_deactivation_state_reason;
        bool current_runtime_deactivation_ready = false;
        bool current_runtime_deactivation_state_ready = false;
        std::string action;
        std::string short_circuit_reason;
    };

    struct ChangeLevelPlayerTransferCurrentRuntimeDeactivationExecutionGuardAuditSummary
    {
        bool attempted = false;
        bool prepared = false;
        bool skipped = false;
        std::string decision_source;
        bool current_runtime_deactivation_gate_ready = false;
        bool current_runtime_deactivation_ready = false;
        bool current_runtime_present = false;
        bool deactivation_outer_guard_satisfied = false;
        bool deactivation_inner_guard_satisfied = false;
        bool deactivation_path_eligible = false;
        bool deactivation_executed = false;
        std::string deactivation_path_blocked_by;
        std::string action;
        std::string short_circuit_reason;
    };

    struct ChangeLevelPlayerTransferCurrentRuntimeDeactivationOutcomeSummary
    {
        bool attempted = false;
        bool prepared = false;
        bool skipped = false;
        std::string decision_source;
        std::string bootstrap_execution_outcome;
        std::string bootstrap_result_state;
        bool bootstrap_result_produced = false;
        bool bootstrap_result_consumable = false;
        bool bootstrap_result_consumption_allowed = false;
        bool bootstrap_result_consumption_deferred = false;
        bool bootstrap_result_consumption_attempted = false;
        bool bootstrap_result_consumed = false;
        std::string bootstrap_result_consumption_state;
        std::string bootstrap_result_consumption_outcome;
        bool bootstrap_result_consumption_outcome_available = false;
        std::string bootstrap_result_consumption_outcome_reason;
        bool target_runtime_materialization_candidate = false;
        bool target_runtime_materialization_allowed = false;
        bool target_runtime_materialization_deferred = false;
        bool target_runtime_materialization_attempted = false;
        bool target_runtime_materialization_completed = false;
        bool target_runtime_materialization_succeeded = false;
        bool target_runtime_materialization_blocked = false;
        std::string target_runtime_materialization_block_reason;
        bool target_runtime_materialization_started = false;
        bool target_runtime_materialized = false;
        std::string target_runtime_materialization_state;
        std::string target_runtime_materialization_state_reason;
        std::string target_runtime_materialization_outcome;
        bool target_runtime_materialization_outcome_available = false;
        std::string target_runtime_materialization_outcome_reason;
        bool target_runtime_activation_candidate = false;
        bool target_runtime_activation_allowed = false;
        bool target_runtime_activation_deferred = false;
        bool target_runtime_activation_attempted = false;
        bool target_runtime_activation_completed = false;
        bool target_runtime_activation_succeeded = false;
        bool target_runtime_activation_blocked = false;
        std::string target_runtime_activation_block_reason;
        bool target_runtime_activation_started = false;
        bool target_runtime_activated = false;
        std::string target_runtime_activation_state;
        std::string target_runtime_activation_state_reason;
        std::string target_runtime_activation_outcome;
        bool target_runtime_activation_outcome_available = false;
        std::string target_runtime_activation_outcome_reason;
        bool target_runtime_cutover_candidate = false;
        bool target_runtime_cutover_allowed = false;
        bool target_runtime_cutover_deferred = false;
        bool target_runtime_cutover_attempted = false;
        bool target_runtime_cutover_completed = false;
        bool target_runtime_cutover_succeeded = false;
        bool target_runtime_cutover_blocked = false;
        std::string target_runtime_cutover_block_reason;
        bool target_runtime_cutover_started = false;
        bool target_runtime_cutover_applied = false;
        std::string target_runtime_cutover_state;
        std::string target_runtime_cutover_state_reason;
        std::string target_runtime_cutover_outcome;
        bool target_runtime_cutover_outcome_available = false;
        std::string target_runtime_cutover_outcome_reason;
        bool current_runtime_deactivation_candidate = false;
        bool current_runtime_deactivation_allowed = false;
        bool current_runtime_deactivation_deferred = false;
        bool current_runtime_deactivation_attempted = false;
        bool current_runtime_deactivation_completed = false;
        bool current_runtime_deactivation_succeeded = false;
        bool current_runtime_deactivation_blocked = false;
        std::string current_runtime_deactivation_block_reason;
        bool current_runtime_deactivation_started = false;
        bool current_runtime_deactivated = false;
        std::string current_runtime_deactivation_state;
        std::string current_runtime_deactivation_state_reason;
        std::string current_runtime_deactivation_outcome;
        bool current_runtime_deactivation_outcome_available = false;
        std::string current_runtime_deactivation_outcome_reason;
        bool current_runtime_deactivation_ready = false;
        bool current_runtime_deactivation_outcome_ready = false;
        std::string action;
        std::string short_circuit_reason;
    };

    struct ChangeLevelPlayerTransferTargetRuntimeCheckpointApplicationGateSummary
    {
        bool attempted = false;
        bool prepared = false;
        bool skipped = false;
        std::string decision_source;
        std::string bootstrap_execution_outcome;
        std::string bootstrap_result_state;
        bool bootstrap_result_produced = false;
        bool bootstrap_result_consumable = false;
        bool bootstrap_result_consumption_allowed = false;
        bool bootstrap_result_consumption_deferred = false;
        bool bootstrap_result_consumption_attempted = false;
        bool bootstrap_result_consumed = false;
        std::string bootstrap_result_consumption_state;
        std::string bootstrap_result_consumption_outcome;
        bool bootstrap_result_consumption_outcome_available = false;
        std::string bootstrap_result_consumption_outcome_reason;
        bool target_runtime_materialization_candidate = false;
        bool target_runtime_materialization_allowed = false;
        bool target_runtime_materialization_deferred = false;
        bool target_runtime_materialization_attempted = false;
        bool target_runtime_materialization_completed = false;
        bool target_runtime_materialization_succeeded = false;
        bool target_runtime_materialization_blocked = false;
        std::string target_runtime_materialization_block_reason;
        bool target_runtime_materialization_started = false;
        bool target_runtime_materialized = false;
        std::string target_runtime_materialization_state;
        std::string target_runtime_materialization_state_reason;
        std::string target_runtime_materialization_outcome;
        bool target_runtime_materialization_outcome_available = false;
        std::string target_runtime_materialization_outcome_reason;
        bool target_runtime_activation_candidate = false;
        bool target_runtime_activation_allowed = false;
        bool target_runtime_activation_deferred = false;
        bool target_runtime_activation_attempted = false;
        bool target_runtime_activation_completed = false;
        bool target_runtime_activation_succeeded = false;
        bool target_runtime_activation_blocked = false;
        std::string target_runtime_activation_block_reason;
        bool target_runtime_activation_started = false;
        bool target_runtime_activated = false;
        std::string target_runtime_activation_state;
        std::string target_runtime_activation_state_reason;
        std::string target_runtime_activation_outcome;
        bool target_runtime_activation_outcome_available = false;
        std::string target_runtime_activation_outcome_reason;
        bool target_runtime_cutover_candidate = false;
        bool target_runtime_cutover_allowed = false;
        bool target_runtime_cutover_deferred = false;
        bool target_runtime_cutover_attempted = false;
        bool target_runtime_cutover_completed = false;
        bool target_runtime_cutover_succeeded = false;
        bool target_runtime_cutover_blocked = false;
        std::string target_runtime_cutover_block_reason;
        bool target_runtime_cutover_started = false;
        bool target_runtime_cutover_applied = false;
        std::string target_runtime_cutover_state;
        std::string target_runtime_cutover_state_reason;
        std::string target_runtime_cutover_outcome;
        bool target_runtime_cutover_outcome_available = false;
        std::string target_runtime_cutover_outcome_reason;
        bool current_runtime_deactivation_candidate = false;
        bool current_runtime_deactivation_allowed = false;
        bool current_runtime_deactivation_deferred = false;
        bool current_runtime_deactivation_attempted = false;
        bool current_runtime_deactivation_completed = false;
        bool current_runtime_deactivation_succeeded = false;
        bool current_runtime_deactivation_blocked = false;
        std::string current_runtime_deactivation_block_reason;
        bool current_runtime_deactivation_started = false;
        bool current_runtime_deactivated = false;
        std::string current_runtime_deactivation_state;
        std::string current_runtime_deactivation_state_reason;
        std::string current_runtime_deactivation_outcome;
        bool current_runtime_deactivation_outcome_available = false;
        std::string current_runtime_deactivation_outcome_reason;
        bool current_runtime_deactivation_ready = false;
        bool target_runtime_checkpoint_application_candidate = false;
        bool target_runtime_checkpoint_application_allowed = false;
        bool target_runtime_checkpoint_application_deferred = false;
        bool target_runtime_checkpoint_application_attempted = false;
        bool target_runtime_checkpoint_application_completed = false;
        bool target_runtime_checkpoint_application_succeeded = false;
        bool target_runtime_checkpoint_application_blocked = false;
        std::string target_runtime_checkpoint_application_block_reason;
        bool target_runtime_checkpoint_application_ready = false;
        bool target_runtime_checkpoint_application_gate_ready = false;
        std::string action;
        std::string short_circuit_reason;
    };

    struct ChangeLevelPlayerTransferTargetRuntimeCheckpointApplicationStateSummary
    {
        bool attempted = false;
        bool prepared = false;
        bool skipped = false;
        std::string decision_source;
        std::string bootstrap_execution_outcome;
        std::string bootstrap_result_state;
        bool bootstrap_result_produced = false;
        bool bootstrap_result_consumable = false;
        bool bootstrap_result_consumption_allowed = false;
        bool bootstrap_result_consumption_deferred = false;
        bool bootstrap_result_consumption_attempted = false;
        bool bootstrap_result_consumed = false;
        std::string bootstrap_result_consumption_state;
        std::string bootstrap_result_consumption_outcome;
        bool bootstrap_result_consumption_outcome_available = false;
        std::string bootstrap_result_consumption_outcome_reason;
        bool target_runtime_materialization_candidate = false;
        bool target_runtime_materialization_allowed = false;
        bool target_runtime_materialization_deferred = false;
        bool target_runtime_materialization_attempted = false;
        bool target_runtime_materialization_completed = false;
        bool target_runtime_materialization_succeeded = false;
        bool target_runtime_materialization_blocked = false;
        std::string target_runtime_materialization_block_reason;
        bool target_runtime_materialization_started = false;
        bool target_runtime_materialized = false;
        std::string target_runtime_materialization_state;
        std::string target_runtime_materialization_state_reason;
        std::string target_runtime_materialization_outcome;
        bool target_runtime_materialization_outcome_available = false;
        std::string target_runtime_materialization_outcome_reason;
        bool target_runtime_activation_candidate = false;
        bool target_runtime_activation_allowed = false;
        bool target_runtime_activation_deferred = false;
        bool target_runtime_activation_attempted = false;
        bool target_runtime_activation_completed = false;
        bool target_runtime_activation_succeeded = false;
        bool target_runtime_activation_blocked = false;
        std::string target_runtime_activation_block_reason;
        bool target_runtime_activation_started = false;
        bool target_runtime_activated = false;
        std::string target_runtime_activation_state;
        std::string target_runtime_activation_state_reason;
        std::string target_runtime_activation_outcome;
        bool target_runtime_activation_outcome_available = false;
        std::string target_runtime_activation_outcome_reason;
        bool target_runtime_cutover_candidate = false;
        bool target_runtime_cutover_allowed = false;
        bool target_runtime_cutover_deferred = false;
        bool target_runtime_cutover_attempted = false;
        bool target_runtime_cutover_completed = false;
        bool target_runtime_cutover_succeeded = false;
        bool target_runtime_cutover_blocked = false;
        std::string target_runtime_cutover_block_reason;
        bool target_runtime_cutover_started = false;
        bool target_runtime_cutover_applied = false;
        std::string target_runtime_cutover_state;
        std::string target_runtime_cutover_state_reason;
        std::string target_runtime_cutover_outcome;
        bool target_runtime_cutover_outcome_available = false;
        std::string target_runtime_cutover_outcome_reason;
        bool current_runtime_deactivation_candidate = false;
        bool current_runtime_deactivation_allowed = false;
        bool current_runtime_deactivation_deferred = false;
        bool current_runtime_deactivation_attempted = false;
        bool current_runtime_deactivation_completed = false;
        bool current_runtime_deactivation_succeeded = false;
        bool current_runtime_deactivation_blocked = false;
        std::string current_runtime_deactivation_block_reason;
        bool current_runtime_deactivation_started = false;
        bool current_runtime_deactivated = false;
        std::string current_runtime_deactivation_state;
        std::string current_runtime_deactivation_state_reason;
        std::string current_runtime_deactivation_outcome;
        bool current_runtime_deactivation_outcome_available = false;
        std::string current_runtime_deactivation_outcome_reason;
        bool current_runtime_deactivation_ready = false;
        bool target_runtime_checkpoint_application_candidate = false;
        bool target_runtime_checkpoint_application_allowed = false;
        bool target_runtime_checkpoint_application_deferred = false;
        bool target_runtime_checkpoint_application_attempted = false;
        bool target_runtime_checkpoint_application_completed = false;
        bool target_runtime_checkpoint_application_succeeded = false;
        bool target_runtime_checkpoint_application_blocked = false;
        std::string target_runtime_checkpoint_application_block_reason;
        bool target_runtime_checkpoint_application_started = false;
        bool target_runtime_checkpoint_applied = false;
        std::string target_runtime_checkpoint_application_state;
        std::string target_runtime_checkpoint_application_state_reason;
        bool target_runtime_checkpoint_application_ready = false;
        bool target_runtime_checkpoint_application_state_ready = false;
        std::string action;
        std::string short_circuit_reason;
    };

    struct ChangeLevelPlayerTransferTargetRuntimeCheckpointApplicationApplyGuardAuditSummary
    {
        bool attempted = false;
        bool prepared = false;
        bool skipped = false;
        std::string decision_source;
        bool checkpoint_application_gate_ready = false;
        bool checkpoint_application_ready = false;
        bool write_set_attempted = false;
        bool write_set_prepared = false;
        bool write_set_ready = false;
        bool write_origin = false;
        bool write_yaw = false;
        bool target_player_present = false;
        bool origin_parse_ready = false;
        bool yaw_parse_ready = false;
        bool apply_outer_guard_satisfied = false;
        bool apply_inner_guard_satisfied = false;
        bool apply_path_eligible = false;
        bool apply_executed = false;
        std::string apply_path_blocked_by;
        std::string action;
        std::string short_circuit_reason;
    };

    struct ChangeLevelPlayerTransferTargetRuntimeCheckpointApplicationOutcomeSummary
    {
        bool attempted = false;
        bool prepared = false;
        bool skipped = false;
        std::string decision_source;
        std::string bootstrap_execution_outcome;
        std::string bootstrap_result_state;
        bool bootstrap_result_produced = false;
        bool bootstrap_result_consumable = false;
        bool bootstrap_result_consumption_allowed = false;
        bool bootstrap_result_consumption_deferred = false;
        bool bootstrap_result_consumption_attempted = false;
        bool bootstrap_result_consumed = false;
        std::string bootstrap_result_consumption_state;
        std::string bootstrap_result_consumption_outcome;
        bool bootstrap_result_consumption_outcome_available = false;
        std::string bootstrap_result_consumption_outcome_reason;
        bool target_runtime_materialization_candidate = false;
        bool target_runtime_materialization_allowed = false;
        bool target_runtime_materialization_deferred = false;
        bool target_runtime_materialization_attempted = false;
        bool target_runtime_materialization_completed = false;
        bool target_runtime_materialization_succeeded = false;
        bool target_runtime_materialization_blocked = false;
        std::string target_runtime_materialization_block_reason;
        bool target_runtime_materialization_started = false;
        bool target_runtime_materialized = false;
        std::string target_runtime_materialization_state;
        std::string target_runtime_materialization_state_reason;
        std::string target_runtime_materialization_outcome;
        bool target_runtime_materialization_outcome_available = false;
        std::string target_runtime_materialization_outcome_reason;
        bool target_runtime_activation_candidate = false;
        bool target_runtime_activation_allowed = false;
        bool target_runtime_activation_deferred = false;
        bool target_runtime_activation_attempted = false;
        bool target_runtime_activation_completed = false;
        bool target_runtime_activation_succeeded = false;
        bool target_runtime_activation_blocked = false;
        std::string target_runtime_activation_block_reason;
        bool target_runtime_activation_started = false;
        bool target_runtime_activated = false;
        std::string target_runtime_activation_state;
        std::string target_runtime_activation_state_reason;
        std::string target_runtime_activation_outcome;
        bool target_runtime_activation_outcome_available = false;
        std::string target_runtime_activation_outcome_reason;
        bool target_runtime_cutover_candidate = false;
        bool target_runtime_cutover_allowed = false;
        bool target_runtime_cutover_deferred = false;
        bool target_runtime_cutover_attempted = false;
        bool target_runtime_cutover_completed = false;
        bool target_runtime_cutover_succeeded = false;
        bool target_runtime_cutover_blocked = false;
        std::string target_runtime_cutover_block_reason;
        bool target_runtime_cutover_started = false;
        bool target_runtime_cutover_applied = false;
        std::string target_runtime_cutover_state;
        std::string target_runtime_cutover_state_reason;
        std::string target_runtime_cutover_outcome;
        bool target_runtime_cutover_outcome_available = false;
        std::string target_runtime_cutover_outcome_reason;
        bool current_runtime_deactivation_candidate = false;
        bool current_runtime_deactivation_allowed = false;
        bool current_runtime_deactivation_deferred = false;
        bool current_runtime_deactivation_attempted = false;
        bool current_runtime_deactivation_completed = false;
        bool current_runtime_deactivation_succeeded = false;
        bool current_runtime_deactivation_blocked = false;
        std::string current_runtime_deactivation_block_reason;
        bool current_runtime_deactivation_started = false;
        bool current_runtime_deactivated = false;
        std::string current_runtime_deactivation_state;
        std::string current_runtime_deactivation_state_reason;
        std::string current_runtime_deactivation_outcome;
        bool current_runtime_deactivation_outcome_available = false;
        std::string current_runtime_deactivation_outcome_reason;
        bool current_runtime_deactivation_ready = false;
        bool target_runtime_checkpoint_application_candidate = false;
        bool target_runtime_checkpoint_application_allowed = false;
        bool target_runtime_checkpoint_application_deferred = false;
        bool target_runtime_checkpoint_application_attempted = false;
        bool target_runtime_checkpoint_application_completed = false;
        bool target_runtime_checkpoint_application_succeeded = false;
        bool target_runtime_checkpoint_application_blocked = false;
        std::string target_runtime_checkpoint_application_block_reason;
        bool target_runtime_checkpoint_application_started = false;
        bool target_runtime_checkpoint_applied = false;
        std::string target_runtime_checkpoint_application_state;
        std::string target_runtime_checkpoint_application_state_reason;
        std::string target_runtime_checkpoint_application_outcome;
        bool target_runtime_checkpoint_application_outcome_available = false;
        std::string target_runtime_checkpoint_application_outcome_reason;
        bool target_runtime_checkpoint_application_ready = false;
        bool target_runtime_checkpoint_application_outcome_ready = false;
        std::string action;
        std::string short_circuit_reason;
    };

    struct ChangeLevelPlayerTransferTargetRuntimePlayerPlacementGateSummary
    {
        bool attempted = false;
        bool prepared = false;
        bool skipped = false;
        std::string decision_source;
        std::string target_runtime_cutover_outcome;
        bool target_runtime_cutover_outcome_available = false;
        std::string target_runtime_cutover_outcome_reason;
        bool current_runtime_deactivation_candidate = false;
        bool current_runtime_deactivation_allowed = false;
        bool current_runtime_deactivation_deferred = false;
        bool current_runtime_deactivation_attempted = false;
        bool current_runtime_deactivation_completed = false;
        bool current_runtime_deactivation_succeeded = false;
        bool current_runtime_deactivation_blocked = false;
        std::string current_runtime_deactivation_block_reason;
        bool current_runtime_deactivation_started = false;
        bool current_runtime_deactivated = false;
        std::string current_runtime_deactivation_state;
        std::string current_runtime_deactivation_state_reason;
        std::string current_runtime_deactivation_outcome;
        bool current_runtime_deactivation_outcome_available = false;
        std::string current_runtime_deactivation_outcome_reason;
        bool target_runtime_checkpoint_application_candidate = false;
        bool target_runtime_checkpoint_application_allowed = false;
        bool target_runtime_checkpoint_application_deferred = false;
        bool target_runtime_checkpoint_application_attempted = false;
        bool target_runtime_checkpoint_application_completed = false;
        bool target_runtime_checkpoint_application_succeeded = false;
        bool target_runtime_checkpoint_application_blocked = false;
        std::string target_runtime_checkpoint_application_block_reason;
        bool target_runtime_checkpoint_application_started = false;
        bool target_runtime_checkpoint_applied = false;
        std::string target_runtime_checkpoint_application_state;
        std::string target_runtime_checkpoint_application_state_reason;
        std::string target_runtime_checkpoint_application_outcome;
        bool target_runtime_checkpoint_application_outcome_available = false;
        std::string target_runtime_checkpoint_application_outcome_reason;
        bool target_runtime_player_placement_candidate = false;
        bool target_runtime_player_placement_allowed = false;
        bool target_runtime_player_placement_deferred = false;
        bool target_runtime_player_placement_attempted = false;
        bool target_runtime_player_placement_completed = false;
        bool target_runtime_player_placement_succeeded = false;
        bool target_runtime_player_placement_blocked = false;
        std::string target_runtime_player_placement_block_reason;
        bool target_runtime_player_placement_ready = false;
        bool target_runtime_player_placement_gate_ready = false;
        std::string action;
        std::string short_circuit_reason;
    };

    struct ChangeLevelPlayerTransferTargetRuntimePlayerPlacementStateSummary
    {
        bool attempted = false;
        bool prepared = false;
        bool skipped = false;
        std::string decision_source;
        std::string target_runtime_cutover_outcome;
        bool target_runtime_cutover_outcome_available = false;
        std::string target_runtime_cutover_outcome_reason;
        bool current_runtime_deactivation_candidate = false;
        bool current_runtime_deactivation_allowed = false;
        bool current_runtime_deactivation_deferred = false;
        bool current_runtime_deactivation_attempted = false;
        bool current_runtime_deactivation_completed = false;
        bool current_runtime_deactivation_succeeded = false;
        bool current_runtime_deactivation_blocked = false;
        std::string current_runtime_deactivation_block_reason;
        bool current_runtime_deactivation_started = false;
        bool current_runtime_deactivated = false;
        std::string current_runtime_deactivation_state;
        std::string current_runtime_deactivation_state_reason;
        std::string current_runtime_deactivation_outcome;
        bool current_runtime_deactivation_outcome_available = false;
        std::string current_runtime_deactivation_outcome_reason;
        bool target_runtime_checkpoint_application_candidate = false;
        bool target_runtime_checkpoint_application_allowed = false;
        bool target_runtime_checkpoint_application_deferred = false;
        bool target_runtime_checkpoint_application_attempted = false;
        bool target_runtime_checkpoint_application_completed = false;
        bool target_runtime_checkpoint_application_succeeded = false;
        bool target_runtime_checkpoint_application_blocked = false;
        std::string target_runtime_checkpoint_application_block_reason;
        bool target_runtime_checkpoint_application_started = false;
        bool target_runtime_checkpoint_applied = false;
        std::string target_runtime_checkpoint_application_state;
        std::string target_runtime_checkpoint_application_state_reason;
        std::string target_runtime_checkpoint_application_outcome;
        bool target_runtime_checkpoint_application_outcome_available = false;
        std::string target_runtime_checkpoint_application_outcome_reason;
        bool target_runtime_player_placement_candidate = false;
        bool target_runtime_player_placement_allowed = false;
        bool target_runtime_player_placement_deferred = false;
        bool target_runtime_player_placement_attempted = false;
        bool target_runtime_player_placement_completed = false;
        bool target_runtime_player_placement_succeeded = false;
        bool target_runtime_player_placement_blocked = false;
        std::string target_runtime_player_placement_block_reason;
        bool target_runtime_player_placement_started = false;
        bool target_runtime_player_placed = false;
        std::string target_runtime_player_placement_state;
        std::string target_runtime_player_placement_state_reason;
        bool target_runtime_player_placement_ready = false;
        bool target_runtime_player_placement_state_ready = false;
        std::string action;
        std::string short_circuit_reason;
    };

    struct ChangeLevelPlayerTransferTargetRuntimePlayerPlacementOutcomeSummary
    {
        bool attempted = false;
        bool prepared = false;
        bool skipped = false;
        std::string decision_source;
        std::string target_runtime_cutover_outcome;
        bool target_runtime_cutover_outcome_available = false;
        std::string target_runtime_cutover_outcome_reason;
        bool current_runtime_deactivation_candidate = false;
        bool current_runtime_deactivation_allowed = false;
        bool current_runtime_deactivation_deferred = false;
        bool current_runtime_deactivation_attempted = false;
        bool current_runtime_deactivation_completed = false;
        bool current_runtime_deactivation_succeeded = false;
        bool current_runtime_deactivation_blocked = false;
        std::string current_runtime_deactivation_block_reason;
        bool current_runtime_deactivation_started = false;
        bool current_runtime_deactivated = false;
        std::string current_runtime_deactivation_state;
        std::string current_runtime_deactivation_state_reason;
        std::string current_runtime_deactivation_outcome;
        bool current_runtime_deactivation_outcome_available = false;
        std::string current_runtime_deactivation_outcome_reason;
        bool target_runtime_checkpoint_application_candidate = false;
        bool target_runtime_checkpoint_application_allowed = false;
        bool target_runtime_checkpoint_application_deferred = false;
        bool target_runtime_checkpoint_application_attempted = false;
        bool target_runtime_checkpoint_application_completed = false;
        bool target_runtime_checkpoint_application_succeeded = false;
        bool target_runtime_checkpoint_application_blocked = false;
        std::string target_runtime_checkpoint_application_block_reason;
        bool target_runtime_checkpoint_application_started = false;
        bool target_runtime_checkpoint_applied = false;
        std::string target_runtime_checkpoint_application_state;
        std::string target_runtime_checkpoint_application_state_reason;
        std::string target_runtime_checkpoint_application_outcome;
        bool target_runtime_checkpoint_application_outcome_available = false;
        std::string target_runtime_checkpoint_application_outcome_reason;
        bool target_runtime_player_placement_candidate = false;
        bool target_runtime_player_placement_allowed = false;
        bool target_runtime_player_placement_deferred = false;
        bool target_runtime_player_placement_attempted = false;
        bool target_runtime_player_placement_completed = false;
        bool target_runtime_player_placement_succeeded = false;
        bool target_runtime_player_placement_blocked = false;
        std::string target_runtime_player_placement_block_reason;
        bool target_runtime_player_placement_started = false;
        bool target_runtime_player_placed = false;
        std::string target_runtime_player_placement_state;
        std::string target_runtime_player_placement_state_reason;
        std::string target_runtime_player_placement_outcome;
        bool target_runtime_player_placement_outcome_available = false;
        std::string target_runtime_player_placement_outcome_reason;
        bool target_runtime_player_placement_ready = false;
        bool target_runtime_player_placement_outcome_ready = false;
        std::string action;
        std::string short_circuit_reason;
    };

    struct ChangeLevelPlayerTransferTargetRuntimePlayerAttachmentGateSummary
    {
        bool attempted = false;
        bool prepared = false;
        bool skipped = false;
        std::string decision_source;
        std::string target_runtime_cutover_outcome;
        bool target_runtime_cutover_outcome_available = false;
        std::string target_runtime_cutover_outcome_reason;
        bool current_runtime_deactivation_candidate = false;
        bool current_runtime_deactivation_allowed = false;
        bool current_runtime_deactivation_deferred = false;
        bool current_runtime_deactivation_attempted = false;
        bool current_runtime_deactivation_completed = false;
        bool current_runtime_deactivation_succeeded = false;
        bool current_runtime_deactivation_blocked = false;
        std::string current_runtime_deactivation_block_reason;
        bool current_runtime_deactivation_started = false;
        bool current_runtime_deactivated = false;
        std::string current_runtime_deactivation_state;
        std::string current_runtime_deactivation_state_reason;
        std::string current_runtime_deactivation_outcome;
        bool current_runtime_deactivation_outcome_available = false;
        std::string current_runtime_deactivation_outcome_reason;
        bool target_runtime_checkpoint_application_candidate = false;
        bool target_runtime_checkpoint_application_allowed = false;
        bool target_runtime_checkpoint_application_deferred = false;
        bool target_runtime_checkpoint_application_attempted = false;
        bool target_runtime_checkpoint_application_completed = false;
        bool target_runtime_checkpoint_application_succeeded = false;
        bool target_runtime_checkpoint_application_blocked = false;
        std::string target_runtime_checkpoint_application_block_reason;
        bool target_runtime_checkpoint_application_started = false;
        bool target_runtime_checkpoint_applied = false;
        std::string target_runtime_checkpoint_application_state;
        std::string target_runtime_checkpoint_application_state_reason;
        std::string target_runtime_checkpoint_application_outcome;
        bool target_runtime_checkpoint_application_outcome_available = false;
        std::string target_runtime_checkpoint_application_outcome_reason;
        bool target_runtime_player_placement_candidate = false;
        bool target_runtime_player_placement_allowed = false;
        bool target_runtime_player_placement_deferred = false;
        bool target_runtime_player_placement_attempted = false;
        bool target_runtime_player_placement_completed = false;
        bool target_runtime_player_placement_succeeded = false;
        bool target_runtime_player_placement_blocked = false;
        std::string target_runtime_player_placement_block_reason;
        bool target_runtime_player_placement_started = false;
        bool target_runtime_player_placed = false;
        std::string target_runtime_player_placement_state;
        std::string target_runtime_player_placement_state_reason;
        std::string target_runtime_player_placement_outcome;
        bool target_runtime_player_placement_outcome_available = false;
        std::string target_runtime_player_placement_outcome_reason;
        bool target_runtime_player_attachment_candidate = false;
        bool target_runtime_player_attachment_allowed = false;
        bool target_runtime_player_attachment_deferred = false;
        bool target_runtime_player_attachment_attempted = false;
        bool target_runtime_player_attachment_completed = false;
        bool target_runtime_player_attachment_succeeded = false;
        bool target_runtime_player_attachment_blocked = false;
        std::string target_runtime_player_attachment_block_reason;
        bool target_runtime_player_attachment_ready = false;
        bool target_runtime_player_attachment_gate_ready = false;
        std::string action;
        std::string short_circuit_reason;
    };

    struct ChangeLevelPlayerTransferTargetRuntimePlayerAttachmentStateSummary
    {
        bool attempted = false;
        bool prepared = false;
        bool skipped = false;
        std::string decision_source;
        std::string target_runtime_cutover_outcome;
        bool target_runtime_cutover_outcome_available = false;
        std::string target_runtime_cutover_outcome_reason;
        bool current_runtime_deactivation_candidate = false;
        bool current_runtime_deactivation_allowed = false;
        bool current_runtime_deactivation_deferred = false;
        bool current_runtime_deactivation_attempted = false;
        bool current_runtime_deactivation_completed = false;
        bool current_runtime_deactivation_succeeded = false;
        bool current_runtime_deactivation_blocked = false;
        std::string current_runtime_deactivation_block_reason;
        bool current_runtime_deactivation_started = false;
        bool current_runtime_deactivated = false;
        std::string current_runtime_deactivation_state;
        std::string current_runtime_deactivation_state_reason;
        std::string current_runtime_deactivation_outcome;
        bool current_runtime_deactivation_outcome_available = false;
        std::string current_runtime_deactivation_outcome_reason;
        bool target_runtime_checkpoint_application_candidate = false;
        bool target_runtime_checkpoint_application_allowed = false;
        bool target_runtime_checkpoint_application_deferred = false;
        bool target_runtime_checkpoint_application_attempted = false;
        bool target_runtime_checkpoint_application_completed = false;
        bool target_runtime_checkpoint_application_succeeded = false;
        bool target_runtime_checkpoint_application_blocked = false;
        std::string target_runtime_checkpoint_application_block_reason;
        bool target_runtime_checkpoint_application_started = false;
        bool target_runtime_checkpoint_applied = false;
        std::string target_runtime_checkpoint_application_state;
        std::string target_runtime_checkpoint_application_state_reason;
        std::string target_runtime_checkpoint_application_outcome;
        bool target_runtime_checkpoint_application_outcome_available = false;
        std::string target_runtime_checkpoint_application_outcome_reason;
        bool target_runtime_player_placement_candidate = false;
        bool target_runtime_player_placement_allowed = false;
        bool target_runtime_player_placement_deferred = false;
        bool target_runtime_player_placement_attempted = false;
        bool target_runtime_player_placement_completed = false;
        bool target_runtime_player_placement_succeeded = false;
        bool target_runtime_player_placement_blocked = false;
        std::string target_runtime_player_placement_block_reason;
        bool target_runtime_player_placement_started = false;
        bool target_runtime_player_placed = false;
        std::string target_runtime_player_placement_state;
        std::string target_runtime_player_placement_state_reason;
        std::string target_runtime_player_placement_outcome;
        bool target_runtime_player_placement_outcome_available = false;
        std::string target_runtime_player_placement_outcome_reason;
        bool target_runtime_player_attachment_candidate = false;
        bool target_runtime_player_attachment_allowed = false;
        bool target_runtime_player_attachment_deferred = false;
        bool target_runtime_player_attachment_attempted = false;
        bool target_runtime_player_attachment_completed = false;
        bool target_runtime_player_attachment_succeeded = false;
        bool target_runtime_player_attachment_blocked = false;
        std::string target_runtime_player_attachment_block_reason;
        bool target_runtime_player_attachment_started = false;
        bool target_runtime_player_attached = false;
        std::string target_runtime_player_attachment_state;
        std::string target_runtime_player_attachment_state_reason;
        bool target_runtime_player_attachment_ready = false;
        bool target_runtime_player_attachment_state_ready = false;
        std::string action;
        std::string short_circuit_reason;
    };

    struct ChangeLevelPlayerTransferTargetRuntimePlayerAttachmentOutcomeSummary
    {
        bool attempted = false;
        bool prepared = false;
        bool skipped = false;
        std::string decision_source;
        std::string target_runtime_cutover_outcome;
        bool target_runtime_cutover_outcome_available = false;
        std::string target_runtime_cutover_outcome_reason;
        bool current_runtime_deactivation_candidate = false;
        bool current_runtime_deactivation_allowed = false;
        bool current_runtime_deactivation_deferred = false;
        bool current_runtime_deactivation_attempted = false;
        bool current_runtime_deactivation_completed = false;
        bool current_runtime_deactivation_succeeded = false;
        bool current_runtime_deactivation_blocked = false;
        std::string current_runtime_deactivation_block_reason;
        bool current_runtime_deactivation_started = false;
        bool current_runtime_deactivated = false;
        std::string current_runtime_deactivation_state;
        std::string current_runtime_deactivation_state_reason;
        std::string current_runtime_deactivation_outcome;
        bool current_runtime_deactivation_outcome_available = false;
        std::string current_runtime_deactivation_outcome_reason;
        bool target_runtime_checkpoint_application_candidate = false;
        bool target_runtime_checkpoint_application_allowed = false;
        bool target_runtime_checkpoint_application_deferred = false;
        bool target_runtime_checkpoint_application_attempted = false;
        bool target_runtime_checkpoint_application_completed = false;
        bool target_runtime_checkpoint_application_succeeded = false;
        bool target_runtime_checkpoint_application_blocked = false;
        std::string target_runtime_checkpoint_application_block_reason;
        bool target_runtime_checkpoint_application_started = false;
        bool target_runtime_checkpoint_applied = false;
        std::string target_runtime_checkpoint_application_state;
        std::string target_runtime_checkpoint_application_state_reason;
        std::string target_runtime_checkpoint_application_outcome;
        bool target_runtime_checkpoint_application_outcome_available = false;
        std::string target_runtime_checkpoint_application_outcome_reason;
        bool target_runtime_player_placement_candidate = false;
        bool target_runtime_player_placement_allowed = false;
        bool target_runtime_player_placement_deferred = false;
        bool target_runtime_player_placement_attempted = false;
        bool target_runtime_player_placement_completed = false;
        bool target_runtime_player_placement_succeeded = false;
        bool target_runtime_player_placement_blocked = false;
        std::string target_runtime_player_placement_block_reason;
        bool target_runtime_player_placement_started = false;
        bool target_runtime_player_placed = false;
        std::string target_runtime_player_placement_state;
        std::string target_runtime_player_placement_state_reason;
        std::string target_runtime_player_placement_outcome;
        bool target_runtime_player_placement_outcome_available = false;
        std::string target_runtime_player_placement_outcome_reason;
        bool target_runtime_player_attachment_candidate = false;
        bool target_runtime_player_attachment_allowed = false;
        bool target_runtime_player_attachment_deferred = false;
        bool target_runtime_player_attachment_attempted = false;
        bool target_runtime_player_attachment_completed = false;
        bool target_runtime_player_attachment_succeeded = false;
        bool target_runtime_player_attachment_blocked = false;
        std::string target_runtime_player_attachment_block_reason;
        bool target_runtime_player_attachment_started = false;
        bool target_runtime_player_attached = false;
        std::string target_runtime_player_attachment_state;
        std::string target_runtime_player_attachment_state_reason;
        std::string target_runtime_player_attachment_outcome;
        bool target_runtime_player_attachment_outcome_available = false;
        std::string target_runtime_player_attachment_outcome_reason;
        bool target_runtime_player_attachment_ready = false;
        bool target_runtime_player_attachment_outcome_ready = false;
        std::string action;
        std::string short_circuit_reason;
    };

    struct ChangeLevelPlayerTransferTargetRuntimePlayerControlHandoffGateSummary
    {
        bool attempted = false;
        bool prepared = false;
        bool skipped = false;
        std::string decision_source;
        std::string target_runtime_cutover_outcome;
        bool target_runtime_cutover_outcome_available = false;
        std::string target_runtime_cutover_outcome_reason;
        bool current_runtime_deactivation_candidate = false;
        bool current_runtime_deactivation_allowed = false;
        bool current_runtime_deactivation_deferred = false;
        bool current_runtime_deactivation_attempted = false;
        bool current_runtime_deactivation_completed = false;
        bool current_runtime_deactivation_succeeded = false;
        bool current_runtime_deactivation_blocked = false;
        std::string current_runtime_deactivation_block_reason;
        bool current_runtime_deactivation_started = false;
        bool current_runtime_deactivated = false;
        std::string current_runtime_deactivation_state;
        std::string current_runtime_deactivation_state_reason;
        std::string current_runtime_deactivation_outcome;
        bool current_runtime_deactivation_outcome_available = false;
        std::string current_runtime_deactivation_outcome_reason;
        bool target_runtime_checkpoint_application_candidate = false;
        bool target_runtime_checkpoint_application_allowed = false;
        bool target_runtime_checkpoint_application_deferred = false;
        bool target_runtime_checkpoint_application_attempted = false;
        bool target_runtime_checkpoint_application_completed = false;
        bool target_runtime_checkpoint_application_succeeded = false;
        bool target_runtime_checkpoint_application_blocked = false;
        std::string target_runtime_checkpoint_application_block_reason;
        bool target_runtime_checkpoint_application_started = false;
        bool target_runtime_checkpoint_applied = false;
        std::string target_runtime_checkpoint_application_state;
        std::string target_runtime_checkpoint_application_state_reason;
        std::string target_runtime_checkpoint_application_outcome;
        bool target_runtime_checkpoint_application_outcome_available = false;
        std::string target_runtime_checkpoint_application_outcome_reason;
        bool target_runtime_player_placement_candidate = false;
        bool target_runtime_player_placement_allowed = false;
        bool target_runtime_player_placement_deferred = false;
        bool target_runtime_player_placement_attempted = false;
        bool target_runtime_player_placement_completed = false;
        bool target_runtime_player_placement_succeeded = false;
        bool target_runtime_player_placement_blocked = false;
        std::string target_runtime_player_placement_block_reason;
        bool target_runtime_player_placement_started = false;
        bool target_runtime_player_placed = false;
        std::string target_runtime_player_placement_state;
        std::string target_runtime_player_placement_state_reason;
        std::string target_runtime_player_placement_outcome;
        bool target_runtime_player_placement_outcome_available = false;
        std::string target_runtime_player_placement_outcome_reason;
        bool target_runtime_player_attachment_candidate = false;
        bool target_runtime_player_attachment_allowed = false;
        bool target_runtime_player_attachment_deferred = false;
        bool target_runtime_player_attachment_attempted = false;
        bool target_runtime_player_attachment_completed = false;
        bool target_runtime_player_attachment_succeeded = false;
        bool target_runtime_player_attachment_blocked = false;
        std::string target_runtime_player_attachment_block_reason;
        bool target_runtime_player_attachment_started = false;
        bool target_runtime_player_attached = false;
        std::string target_runtime_player_attachment_state;
        std::string target_runtime_player_attachment_state_reason;
        std::string target_runtime_player_attachment_outcome;
        bool target_runtime_player_attachment_outcome_available = false;
        std::string target_runtime_player_attachment_outcome_reason;
        bool target_runtime_player_attachment_ready = false;
        bool target_runtime_player_control_handoff_candidate = false;
        bool target_runtime_player_control_handoff_allowed = false;
        bool target_runtime_player_control_handoff_deferred = false;
        bool target_runtime_player_control_handoff_attempted = false;
        bool target_runtime_player_control_handoff_completed = false;
        bool target_runtime_player_control_handoff_succeeded = false;
        bool target_runtime_player_control_handoff_blocked = false;
        std::string target_runtime_player_control_handoff_block_reason;
        bool target_runtime_player_control_handoff_ready = false;
        bool target_runtime_player_control_handoff_gate_ready = false;
        std::string action;
        std::string short_circuit_reason;
    };

    struct ChangeLevelPlayerTransferTargetRuntimePlayerControlHandoffStateSummary
    {
        bool attempted = false;
        bool prepared = false;
        bool skipped = false;
        std::string decision_source;
        std::string target_runtime_cutover_outcome;
        bool target_runtime_cutover_outcome_available = false;
        std::string target_runtime_cutover_outcome_reason;
        bool current_runtime_deactivation_candidate = false;
        bool current_runtime_deactivation_allowed = false;
        bool current_runtime_deactivation_deferred = false;
        bool current_runtime_deactivation_attempted = false;
        bool current_runtime_deactivation_completed = false;
        bool current_runtime_deactivation_succeeded = false;
        bool current_runtime_deactivation_blocked = false;
        std::string current_runtime_deactivation_block_reason;
        bool current_runtime_deactivation_started = false;
        bool current_runtime_deactivated = false;
        std::string current_runtime_deactivation_state;
        std::string current_runtime_deactivation_state_reason;
        std::string current_runtime_deactivation_outcome;
        bool current_runtime_deactivation_outcome_available = false;
        std::string current_runtime_deactivation_outcome_reason;
        bool target_runtime_checkpoint_application_candidate = false;
        bool target_runtime_checkpoint_application_allowed = false;
        bool target_runtime_checkpoint_application_deferred = false;
        bool target_runtime_checkpoint_application_attempted = false;
        bool target_runtime_checkpoint_application_completed = false;
        bool target_runtime_checkpoint_application_succeeded = false;
        bool target_runtime_checkpoint_application_blocked = false;
        std::string target_runtime_checkpoint_application_block_reason;
        bool target_runtime_checkpoint_application_started = false;
        bool target_runtime_checkpoint_applied = false;
        std::string target_runtime_checkpoint_application_state;
        std::string target_runtime_checkpoint_application_state_reason;
        std::string target_runtime_checkpoint_application_outcome;
        bool target_runtime_checkpoint_application_outcome_available = false;
        std::string target_runtime_checkpoint_application_outcome_reason;
        bool target_runtime_player_placement_candidate = false;
        bool target_runtime_player_placement_allowed = false;
        bool target_runtime_player_placement_deferred = false;
        bool target_runtime_player_placement_attempted = false;
        bool target_runtime_player_placement_completed = false;
        bool target_runtime_player_placement_succeeded = false;
        bool target_runtime_player_placement_blocked = false;
        std::string target_runtime_player_placement_block_reason;
        bool target_runtime_player_placement_started = false;
        bool target_runtime_player_placed = false;
        std::string target_runtime_player_placement_state;
        std::string target_runtime_player_placement_state_reason;
        std::string target_runtime_player_placement_outcome;
        bool target_runtime_player_placement_outcome_available = false;
        std::string target_runtime_player_placement_outcome_reason;
        bool target_runtime_player_attachment_candidate = false;
        bool target_runtime_player_attachment_allowed = false;
        bool target_runtime_player_attachment_deferred = false;
        bool target_runtime_player_attachment_attempted = false;
        bool target_runtime_player_attachment_completed = false;
        bool target_runtime_player_attachment_succeeded = false;
        bool target_runtime_player_attachment_blocked = false;
        std::string target_runtime_player_attachment_block_reason;
        bool target_runtime_player_attachment_started = false;
        bool target_runtime_player_attached = false;
        std::string target_runtime_player_attachment_state;
        std::string target_runtime_player_attachment_state_reason;
        std::string target_runtime_player_attachment_outcome;
        bool target_runtime_player_attachment_outcome_available = false;
        std::string target_runtime_player_attachment_outcome_reason;
        bool target_runtime_player_attachment_ready = false;
        bool target_runtime_player_control_handoff_candidate = false;
        bool target_runtime_player_control_handoff_allowed = false;
        bool target_runtime_player_control_handoff_deferred = false;
        bool target_runtime_player_control_handoff_attempted = false;
        bool target_runtime_player_control_handoff_completed = false;
        bool target_runtime_player_control_handoff_succeeded = false;
        bool target_runtime_player_control_handoff_blocked = false;
        std::string target_runtime_player_control_handoff_block_reason;
        bool target_runtime_player_control_handoff_started = false;
        bool target_runtime_player_control_handed_off = false;
        std::string target_runtime_player_control_handoff_state;
        std::string target_runtime_player_control_handoff_state_reason;
        bool target_runtime_player_control_handoff_ready = false;
        bool target_runtime_player_control_handoff_state_ready = false;
        std::string action;
        std::string short_circuit_reason;
    };

    struct ChangeLevelPlayerTransferTargetRuntimePlayerControlHandoffOutcomeSummary
    {
        bool attempted = false;
        bool prepared = false;
        bool skipped = false;
        std::string decision_source;
        std::string target_runtime_cutover_outcome;
        bool target_runtime_cutover_outcome_available = false;
        std::string target_runtime_cutover_outcome_reason;
        bool current_runtime_deactivation_candidate = false;
        bool current_runtime_deactivation_allowed = false;
        bool current_runtime_deactivation_deferred = false;
        bool current_runtime_deactivation_attempted = false;
        bool current_runtime_deactivation_completed = false;
        bool current_runtime_deactivation_succeeded = false;
        bool current_runtime_deactivation_blocked = false;
        std::string current_runtime_deactivation_block_reason;
        bool current_runtime_deactivation_started = false;
        bool current_runtime_deactivated = false;
        std::string current_runtime_deactivation_state;
        std::string current_runtime_deactivation_state_reason;
        std::string current_runtime_deactivation_outcome;
        bool current_runtime_deactivation_outcome_available = false;
        std::string current_runtime_deactivation_outcome_reason;
        bool target_runtime_checkpoint_application_candidate = false;
        bool target_runtime_checkpoint_application_allowed = false;
        bool target_runtime_checkpoint_application_deferred = false;
        bool target_runtime_checkpoint_application_attempted = false;
        bool target_runtime_checkpoint_application_completed = false;
        bool target_runtime_checkpoint_application_succeeded = false;
        bool target_runtime_checkpoint_application_blocked = false;
        std::string target_runtime_checkpoint_application_block_reason;
        bool target_runtime_checkpoint_application_started = false;
        bool target_runtime_checkpoint_applied = false;
        std::string target_runtime_checkpoint_application_state;
        std::string target_runtime_checkpoint_application_state_reason;
        std::string target_runtime_checkpoint_application_outcome;
        bool target_runtime_checkpoint_application_outcome_available = false;
        std::string target_runtime_checkpoint_application_outcome_reason;
        bool target_runtime_player_placement_candidate = false;
        bool target_runtime_player_placement_allowed = false;
        bool target_runtime_player_placement_deferred = false;
        bool target_runtime_player_placement_attempted = false;
        bool target_runtime_player_placement_completed = false;
        bool target_runtime_player_placement_succeeded = false;
        bool target_runtime_player_placement_blocked = false;
        std::string target_runtime_player_placement_block_reason;
        bool target_runtime_player_placement_started = false;
        bool target_runtime_player_placed = false;
        std::string target_runtime_player_placement_state;
        std::string target_runtime_player_placement_state_reason;
        std::string target_runtime_player_placement_outcome;
        bool target_runtime_player_placement_outcome_available = false;
        std::string target_runtime_player_placement_outcome_reason;
        bool target_runtime_player_attachment_candidate = false;
        bool target_runtime_player_attachment_allowed = false;
        bool target_runtime_player_attachment_deferred = false;
        bool target_runtime_player_attachment_attempted = false;
        bool target_runtime_player_attachment_completed = false;
        bool target_runtime_player_attachment_succeeded = false;
        bool target_runtime_player_attachment_blocked = false;
        std::string target_runtime_player_attachment_block_reason;
        bool target_runtime_player_attachment_started = false;
        bool target_runtime_player_attached = false;
        std::string target_runtime_player_attachment_state;
        std::string target_runtime_player_attachment_state_reason;
        std::string target_runtime_player_attachment_outcome;
        bool target_runtime_player_attachment_outcome_available = false;
        std::string target_runtime_player_attachment_outcome_reason;
        bool target_runtime_player_attachment_ready = false;
        bool target_runtime_player_control_handoff_candidate = false;
        bool target_runtime_player_control_handoff_allowed = false;
        bool target_runtime_player_control_handoff_deferred = false;
        bool target_runtime_player_control_handoff_attempted = false;
        bool target_runtime_player_control_handoff_completed = false;
        bool target_runtime_player_control_handoff_succeeded = false;
        bool target_runtime_player_control_handoff_blocked = false;
        std::string target_runtime_player_control_handoff_block_reason;
        bool target_runtime_player_control_handoff_started = false;
        bool target_runtime_player_control_handed_off = false;
        std::string target_runtime_player_control_handoff_state;
        std::string target_runtime_player_control_handoff_state_reason;
        bool target_runtime_player_control_handoff_ready = false;
        std::string target_runtime_player_control_handoff_outcome;
        bool target_runtime_player_control_handoff_outcome_available = false;
        std::string target_runtime_player_control_handoff_outcome_reason;
        bool target_runtime_player_control_handoff_outcome_ready = false;
        std::string action;
        std::string short_circuit_reason;
    };

    struct ChangeLevelPlayerTransferTargetRuntimeCompletionGateSummary
    {
        bool attempted = false;
        bool prepared = false;
        bool skipped = false;
        std::string decision_source;
        std::string target_runtime_cutover_outcome;
        bool target_runtime_cutover_outcome_available = false;
        std::string target_runtime_cutover_outcome_reason;
        bool current_runtime_deactivation_candidate = false;
        bool current_runtime_deactivation_allowed = false;
        bool current_runtime_deactivation_deferred = false;
        bool current_runtime_deactivation_attempted = false;
        bool current_runtime_deactivation_completed = false;
        bool current_runtime_deactivation_succeeded = false;
        bool current_runtime_deactivation_blocked = false;
        std::string current_runtime_deactivation_block_reason;
        bool current_runtime_deactivation_started = false;
        bool current_runtime_deactivated = false;
        std::string current_runtime_deactivation_state;
        std::string current_runtime_deactivation_state_reason;
        std::string current_runtime_deactivation_outcome;
        bool current_runtime_deactivation_outcome_available = false;
        std::string current_runtime_deactivation_outcome_reason;
        bool target_runtime_checkpoint_application_candidate = false;
        bool target_runtime_checkpoint_application_allowed = false;
        bool target_runtime_checkpoint_application_deferred = false;
        bool target_runtime_checkpoint_application_attempted = false;
        bool target_runtime_checkpoint_application_completed = false;
        bool target_runtime_checkpoint_application_succeeded = false;
        bool target_runtime_checkpoint_application_blocked = false;
        std::string target_runtime_checkpoint_application_block_reason;
        bool target_runtime_checkpoint_application_started = false;
        bool target_runtime_checkpoint_applied = false;
        std::string target_runtime_checkpoint_application_state;
        std::string target_runtime_checkpoint_application_state_reason;
        std::string target_runtime_checkpoint_application_outcome;
        bool target_runtime_checkpoint_application_outcome_available = false;
        std::string target_runtime_checkpoint_application_outcome_reason;
        bool target_runtime_player_placement_candidate = false;
        bool target_runtime_player_placement_allowed = false;
        bool target_runtime_player_placement_deferred = false;
        bool target_runtime_player_placement_attempted = false;
        bool target_runtime_player_placement_completed = false;
        bool target_runtime_player_placement_succeeded = false;
        bool target_runtime_player_placement_blocked = false;
        std::string target_runtime_player_placement_block_reason;
        bool target_runtime_player_placement_started = false;
        bool target_runtime_player_placed = false;
        std::string target_runtime_player_placement_state;
        std::string target_runtime_player_placement_state_reason;
        std::string target_runtime_player_placement_outcome;
        bool target_runtime_player_placement_outcome_available = false;
        std::string target_runtime_player_placement_outcome_reason;
        bool target_runtime_player_attachment_candidate = false;
        bool target_runtime_player_attachment_allowed = false;
        bool target_runtime_player_attachment_deferred = false;
        bool target_runtime_player_attachment_attempted = false;
        bool target_runtime_player_attachment_completed = false;
        bool target_runtime_player_attachment_succeeded = false;
        bool target_runtime_player_attachment_blocked = false;
        std::string target_runtime_player_attachment_block_reason;
        bool target_runtime_player_attachment_started = false;
        bool target_runtime_player_attached = false;
        std::string target_runtime_player_attachment_state;
        std::string target_runtime_player_attachment_state_reason;
        std::string target_runtime_player_attachment_outcome;
        bool target_runtime_player_attachment_outcome_available = false;
        std::string target_runtime_player_attachment_outcome_reason;
        bool target_runtime_player_attachment_ready = false;
        bool target_runtime_player_control_handoff_candidate = false;
        bool target_runtime_player_control_handoff_allowed = false;
        bool target_runtime_player_control_handoff_deferred = false;
        bool target_runtime_player_control_handoff_attempted = false;
        bool target_runtime_player_control_handoff_completed = false;
        bool target_runtime_player_control_handoff_succeeded = false;
        bool target_runtime_player_control_handoff_blocked = false;
        std::string target_runtime_player_control_handoff_block_reason;
        bool target_runtime_player_control_handoff_started = false;
        bool target_runtime_player_control_handed_off = false;
        std::string target_runtime_player_control_handoff_state;
        std::string target_runtime_player_control_handoff_state_reason;
        bool target_runtime_player_control_handoff_ready = false;
        std::string target_runtime_player_control_handoff_outcome;
        bool target_runtime_player_control_handoff_outcome_available = false;
        std::string target_runtime_player_control_handoff_outcome_reason;
        bool target_runtime_completion_candidate = false;
        bool target_runtime_completion_allowed = false;
        bool target_runtime_completion_deferred = false;
        bool target_runtime_completion_attempted = false;
        bool target_runtime_completion_completed = false;
        bool target_runtime_completion_succeeded = false;
        bool target_runtime_completion_blocked = false;
        std::string target_runtime_completion_block_reason;
        bool target_runtime_completion_ready = false;
        bool target_runtime_completion_gate_ready = false;
        std::string action;
        std::string short_circuit_reason;
    };

    struct ChangeLevelPlayerTransferTargetRuntimeCompletionStateSummary
    {
        bool attempted = false;
        bool prepared = false;
        bool skipped = false;
        std::string decision_source;
        std::string target_runtime_cutover_outcome;
        bool target_runtime_cutover_outcome_available = false;
        std::string target_runtime_cutover_outcome_reason;
        bool current_runtime_deactivation_candidate = false;
        bool current_runtime_deactivation_allowed = false;
        bool current_runtime_deactivation_deferred = false;
        bool current_runtime_deactivation_attempted = false;
        bool current_runtime_deactivation_completed = false;
        bool current_runtime_deactivation_succeeded = false;
        bool current_runtime_deactivation_blocked = false;
        std::string current_runtime_deactivation_block_reason;
        bool current_runtime_deactivation_started = false;
        bool current_runtime_deactivated = false;
        std::string current_runtime_deactivation_state;
        std::string current_runtime_deactivation_state_reason;
        std::string current_runtime_deactivation_outcome;
        bool current_runtime_deactivation_outcome_available = false;
        std::string current_runtime_deactivation_outcome_reason;
        bool target_runtime_checkpoint_application_candidate = false;
        bool target_runtime_checkpoint_application_allowed = false;
        bool target_runtime_checkpoint_application_deferred = false;
        bool target_runtime_checkpoint_application_attempted = false;
        bool target_runtime_checkpoint_application_completed = false;
        bool target_runtime_checkpoint_application_succeeded = false;
        bool target_runtime_checkpoint_application_blocked = false;
        std::string target_runtime_checkpoint_application_block_reason;
        bool target_runtime_checkpoint_application_started = false;
        bool target_runtime_checkpoint_applied = false;
        std::string target_runtime_checkpoint_application_state;
        std::string target_runtime_checkpoint_application_state_reason;
        std::string target_runtime_checkpoint_application_outcome;
        bool target_runtime_checkpoint_application_outcome_available = false;
        std::string target_runtime_checkpoint_application_outcome_reason;
        bool target_runtime_player_placement_candidate = false;
        bool target_runtime_player_placement_allowed = false;
        bool target_runtime_player_placement_deferred = false;
        bool target_runtime_player_placement_attempted = false;
        bool target_runtime_player_placement_completed = false;
        bool target_runtime_player_placement_succeeded = false;
        bool target_runtime_player_placement_blocked = false;
        std::string target_runtime_player_placement_block_reason;
        bool target_runtime_player_placement_started = false;
        bool target_runtime_player_placed = false;
        std::string target_runtime_player_placement_state;
        std::string target_runtime_player_placement_state_reason;
        std::string target_runtime_player_placement_outcome;
        bool target_runtime_player_placement_outcome_available = false;
        std::string target_runtime_player_placement_outcome_reason;
        bool target_runtime_player_attachment_candidate = false;
        bool target_runtime_player_attachment_allowed = false;
        bool target_runtime_player_attachment_deferred = false;
        bool target_runtime_player_attachment_attempted = false;
        bool target_runtime_player_attachment_completed = false;
        bool target_runtime_player_attachment_succeeded = false;
        bool target_runtime_player_attachment_blocked = false;
        std::string target_runtime_player_attachment_block_reason;
        bool target_runtime_player_attachment_started = false;
        bool target_runtime_player_attached = false;
        std::string target_runtime_player_attachment_state;
        std::string target_runtime_player_attachment_state_reason;
        std::string target_runtime_player_attachment_outcome;
        bool target_runtime_player_attachment_outcome_available = false;
        std::string target_runtime_player_attachment_outcome_reason;
        bool target_runtime_player_attachment_ready = false;
        bool target_runtime_player_control_handoff_candidate = false;
        bool target_runtime_player_control_handoff_allowed = false;
        bool target_runtime_player_control_handoff_deferred = false;
        bool target_runtime_player_control_handoff_attempted = false;
        bool target_runtime_player_control_handoff_completed = false;
        bool target_runtime_player_control_handoff_succeeded = false;
        bool target_runtime_player_control_handoff_blocked = false;
        std::string target_runtime_player_control_handoff_block_reason;
        bool target_runtime_player_control_handoff_started = false;
        bool target_runtime_player_control_handed_off = false;
        std::string target_runtime_player_control_handoff_state;
        std::string target_runtime_player_control_handoff_state_reason;
        bool target_runtime_player_control_handoff_ready = false;
        std::string target_runtime_player_control_handoff_outcome;
        bool target_runtime_player_control_handoff_outcome_available = false;
        std::string target_runtime_player_control_handoff_outcome_reason;
        bool target_runtime_completion_candidate = false;
        bool target_runtime_completion_allowed = false;
        bool target_runtime_completion_deferred = false;
        bool target_runtime_completion_attempted = false;
        bool target_runtime_completion_completed = false;
        bool target_runtime_completion_succeeded = false;
        bool target_runtime_completion_blocked = false;
        std::string target_runtime_completion_block_reason;
        bool target_runtime_completion_started = false;
        bool target_runtime_completed = false;
        std::string target_runtime_completion_state;
        std::string target_runtime_completion_state_reason;
        bool target_runtime_completion_ready = false;
        bool target_runtime_completion_state_ready = false;
        std::string action;
        std::string short_circuit_reason;
    };

    struct ChangeLevelPlayerTransferTargetRuntimeCompletionOutcomeSummary
    {
        bool attempted = false;
        bool prepared = false;
        bool skipped = false;
        std::string decision_source;
        std::string target_runtime_cutover_outcome;
        bool target_runtime_cutover_outcome_available = false;
        std::string target_runtime_cutover_outcome_reason;
        bool current_runtime_deactivation_candidate = false;
        bool current_runtime_deactivation_allowed = false;
        bool current_runtime_deactivation_deferred = false;
        bool current_runtime_deactivation_attempted = false;
        bool current_runtime_deactivation_completed = false;
        bool current_runtime_deactivation_succeeded = false;
        bool current_runtime_deactivation_blocked = false;
        std::string current_runtime_deactivation_block_reason;
        bool current_runtime_deactivation_started = false;
        bool current_runtime_deactivated = false;
        std::string current_runtime_deactivation_state;
        std::string current_runtime_deactivation_state_reason;
        std::string current_runtime_deactivation_outcome;
        bool current_runtime_deactivation_outcome_available = false;
        std::string current_runtime_deactivation_outcome_reason;
        bool target_runtime_checkpoint_application_candidate = false;
        bool target_runtime_checkpoint_application_allowed = false;
        bool target_runtime_checkpoint_application_deferred = false;
        bool target_runtime_checkpoint_application_attempted = false;
        bool target_runtime_checkpoint_application_completed = false;
        bool target_runtime_checkpoint_application_succeeded = false;
        bool target_runtime_checkpoint_application_blocked = false;
        std::string target_runtime_checkpoint_application_block_reason;
        bool target_runtime_checkpoint_application_started = false;
        bool target_runtime_checkpoint_applied = false;
        std::string target_runtime_checkpoint_application_state;
        std::string target_runtime_checkpoint_application_state_reason;
        std::string target_runtime_checkpoint_application_outcome;
        bool target_runtime_checkpoint_application_outcome_available = false;
        std::string target_runtime_checkpoint_application_outcome_reason;
        bool target_runtime_player_placement_candidate = false;
        bool target_runtime_player_placement_allowed = false;
        bool target_runtime_player_placement_deferred = false;
        bool target_runtime_player_placement_attempted = false;
        bool target_runtime_player_placement_completed = false;
        bool target_runtime_player_placement_succeeded = false;
        bool target_runtime_player_placement_blocked = false;
        std::string target_runtime_player_placement_block_reason;
        bool target_runtime_player_placement_started = false;
        bool target_runtime_player_placed = false;
        std::string target_runtime_player_placement_state;
        std::string target_runtime_player_placement_state_reason;
        std::string target_runtime_player_placement_outcome;
        bool target_runtime_player_placement_outcome_available = false;
        std::string target_runtime_player_placement_outcome_reason;
        bool target_runtime_player_attachment_candidate = false;
        bool target_runtime_player_attachment_allowed = false;
        bool target_runtime_player_attachment_deferred = false;
        bool target_runtime_player_attachment_attempted = false;
        bool target_runtime_player_attachment_completed = false;
        bool target_runtime_player_attachment_succeeded = false;
        bool target_runtime_player_attachment_blocked = false;
        std::string target_runtime_player_attachment_block_reason;
        bool target_runtime_player_attachment_started = false;
        bool target_runtime_player_attached = false;
        std::string target_runtime_player_attachment_state;
        std::string target_runtime_player_attachment_state_reason;
        std::string target_runtime_player_attachment_outcome;
        bool target_runtime_player_attachment_outcome_available = false;
        std::string target_runtime_player_attachment_outcome_reason;
        bool target_runtime_player_attachment_ready = false;
        bool target_runtime_player_control_handoff_candidate = false;
        bool target_runtime_player_control_handoff_allowed = false;
        bool target_runtime_player_control_handoff_deferred = false;
        bool target_runtime_player_control_handoff_attempted = false;
        bool target_runtime_player_control_handoff_completed = false;
        bool target_runtime_player_control_handoff_succeeded = false;
        bool target_runtime_player_control_handoff_blocked = false;
        std::string target_runtime_player_control_handoff_block_reason;
        bool target_runtime_player_control_handoff_started = false;
        bool target_runtime_player_control_handed_off = false;
        std::string target_runtime_player_control_handoff_state;
        std::string target_runtime_player_control_handoff_state_reason;
        bool target_runtime_player_control_handoff_ready = false;
        std::string target_runtime_player_control_handoff_outcome;
        bool target_runtime_player_control_handoff_outcome_available = false;
        std::string target_runtime_player_control_handoff_outcome_reason;
        bool target_runtime_completion_candidate = false;
        bool target_runtime_completion_allowed = false;
        bool target_runtime_completion_deferred = false;
        bool target_runtime_completion_attempted = false;
        bool target_runtime_completion_completed = false;
        bool target_runtime_completion_succeeded = false;
        bool target_runtime_completion_blocked = false;
        std::string target_runtime_completion_block_reason;
        bool target_runtime_completion_started = false;
        bool target_runtime_completed = false;
        std::string target_runtime_completion_state;
        std::string target_runtime_completion_state_reason;
        std::string target_runtime_completion_outcome;
        bool target_runtime_completion_outcome_available = false;
        std::string target_runtime_completion_outcome_reason;
        bool target_runtime_completion_ready = false;
        bool target_runtime_completion_outcome_ready = false;
        std::string action;
        std::string short_circuit_reason;
    };

    struct ChangeLevelPlayerTransferTargetRuntimeCompletionMarkGateSummary
    {
        bool attempted = false;
        bool prepared = false;
        bool skipped = false;
        std::string decision_source;
        std::string target_runtime_cutover_outcome;
        bool target_runtime_cutover_outcome_available = false;
        std::string target_runtime_cutover_outcome_reason;
        bool current_runtime_deactivation_candidate = false;
        bool current_runtime_deactivation_allowed = false;
        bool current_runtime_deactivation_deferred = false;
        bool current_runtime_deactivation_attempted = false;
        bool current_runtime_deactivation_completed = false;
        bool current_runtime_deactivation_succeeded = false;
        bool current_runtime_deactivation_blocked = false;
        std::string current_runtime_deactivation_block_reason;
        bool current_runtime_deactivation_started = false;
        bool current_runtime_deactivated = false;
        std::string current_runtime_deactivation_state;
        std::string current_runtime_deactivation_state_reason;
        std::string current_runtime_deactivation_outcome;
        bool current_runtime_deactivation_outcome_available = false;
        std::string current_runtime_deactivation_outcome_reason;
        bool target_runtime_checkpoint_application_candidate = false;
        bool target_runtime_checkpoint_application_allowed = false;
        bool target_runtime_checkpoint_application_deferred = false;
        bool target_runtime_checkpoint_application_attempted = false;
        bool target_runtime_checkpoint_application_completed = false;
        bool target_runtime_checkpoint_application_succeeded = false;
        bool target_runtime_checkpoint_application_blocked = false;
        std::string target_runtime_checkpoint_application_block_reason;
        bool target_runtime_checkpoint_application_started = false;
        bool target_runtime_checkpoint_applied = false;
        std::string target_runtime_checkpoint_application_state;
        std::string target_runtime_checkpoint_application_state_reason;
        std::string target_runtime_checkpoint_application_outcome;
        bool target_runtime_checkpoint_application_outcome_available = false;
        std::string target_runtime_checkpoint_application_outcome_reason;
        bool target_runtime_player_placement_candidate = false;
        bool target_runtime_player_placement_allowed = false;
        bool target_runtime_player_placement_deferred = false;
        bool target_runtime_player_placement_attempted = false;
        bool target_runtime_player_placement_completed = false;
        bool target_runtime_player_placement_succeeded = false;
        bool target_runtime_player_placement_blocked = false;
        std::string target_runtime_player_placement_block_reason;
        bool target_runtime_player_placement_started = false;
        bool target_runtime_player_placed = false;
        std::string target_runtime_player_placement_state;
        std::string target_runtime_player_placement_state_reason;
        std::string target_runtime_player_placement_outcome;
        bool target_runtime_player_placement_outcome_available = false;
        std::string target_runtime_player_placement_outcome_reason;
        bool target_runtime_player_attachment_candidate = false;
        bool target_runtime_player_attachment_allowed = false;
        bool target_runtime_player_attachment_deferred = false;
        bool target_runtime_player_attachment_attempted = false;
        bool target_runtime_player_attachment_completed = false;
        bool target_runtime_player_attachment_succeeded = false;
        bool target_runtime_player_attachment_blocked = false;
        std::string target_runtime_player_attachment_block_reason;
        bool target_runtime_player_attachment_started = false;
        bool target_runtime_player_attached = false;
        std::string target_runtime_player_attachment_state;
        std::string target_runtime_player_attachment_state_reason;
        std::string target_runtime_player_attachment_outcome;
        bool target_runtime_player_attachment_outcome_available = false;
        std::string target_runtime_player_attachment_outcome_reason;
        bool target_runtime_player_attachment_ready = false;
        bool target_runtime_player_control_handoff_candidate = false;
        bool target_runtime_player_control_handoff_allowed = false;
        bool target_runtime_player_control_handoff_deferred = false;
        bool target_runtime_player_control_handoff_attempted = false;
        bool target_runtime_player_control_handoff_completed = false;
        bool target_runtime_player_control_handoff_succeeded = false;
        bool target_runtime_player_control_handoff_blocked = false;
        std::string target_runtime_player_control_handoff_block_reason;
        bool target_runtime_player_control_handoff_started = false;
        bool target_runtime_player_control_handed_off = false;
        std::string target_runtime_player_control_handoff_state;
        std::string target_runtime_player_control_handoff_state_reason;
        bool target_runtime_player_control_handoff_ready = false;
        std::string target_runtime_player_control_handoff_outcome;
        bool target_runtime_player_control_handoff_outcome_available = false;
        std::string target_runtime_player_control_handoff_outcome_reason;
        bool target_runtime_completion_candidate = false;
        bool target_runtime_completion_allowed = false;
        bool target_runtime_completion_deferred = false;
        bool target_runtime_completion_attempted = false;
        bool target_runtime_completion_completed = false;
        bool target_runtime_completion_succeeded = false;
        bool target_runtime_completion_blocked = false;
        std::string target_runtime_completion_block_reason;
        bool target_runtime_completion_started = false;
        bool target_runtime_completed = false;
        std::string target_runtime_completion_state;
        std::string target_runtime_completion_state_reason;
        std::string target_runtime_completion_outcome;
        bool target_runtime_completion_outcome_available = false;
        std::string target_runtime_completion_outcome_reason;
        bool target_runtime_completion_ready = false;
        bool target_runtime_completion_outcome_ready = false;
        bool target_runtime_completion_mark_candidate = false;
        bool target_runtime_completion_mark_allowed = false;
        bool target_runtime_completion_mark_deferred = false;
        bool target_runtime_completion_mark_attempted = false;
        bool target_runtime_completion_mark_completed = false;
        bool target_runtime_completion_mark_succeeded = false;
        bool target_runtime_completion_mark_blocked = false;
        std::string target_runtime_completion_mark_block_reason;
        bool target_runtime_completion_mark_started = false;
        bool target_runtime_completion_marked = false;
        bool target_runtime_completion_mark_ready = false;
        bool target_runtime_completion_mark_gate_ready = false;
        std::string action;
        std::string short_circuit_reason;
    };

    struct ChangeLevelPlayerTransferTargetRuntimeCompletionMarkStateSummary
    {
        bool attempted = false;
        bool prepared = false;
        bool skipped = false;
        std::string decision_source;
        std::string target_runtime_cutover_outcome;
        bool target_runtime_cutover_outcome_available = false;
        std::string target_runtime_cutover_outcome_reason;
        bool current_runtime_deactivation_candidate = false;
        bool current_runtime_deactivation_allowed = false;
        bool current_runtime_deactivation_deferred = false;
        bool current_runtime_deactivation_attempted = false;
        bool current_runtime_deactivation_completed = false;
        bool current_runtime_deactivation_succeeded = false;
        bool current_runtime_deactivation_blocked = false;
        std::string current_runtime_deactivation_block_reason;
        bool current_runtime_deactivation_started = false;
        bool current_runtime_deactivated = false;
        std::string current_runtime_deactivation_state;
        std::string current_runtime_deactivation_state_reason;
        std::string current_runtime_deactivation_outcome;
        bool current_runtime_deactivation_outcome_available = false;
        std::string current_runtime_deactivation_outcome_reason;
        bool target_runtime_checkpoint_application_candidate = false;
        bool target_runtime_checkpoint_application_allowed = false;
        bool target_runtime_checkpoint_application_deferred = false;
        bool target_runtime_checkpoint_application_attempted = false;
        bool target_runtime_checkpoint_application_completed = false;
        bool target_runtime_checkpoint_application_succeeded = false;
        bool target_runtime_checkpoint_application_blocked = false;
        std::string target_runtime_checkpoint_application_block_reason;
        bool target_runtime_checkpoint_application_started = false;
        bool target_runtime_checkpoint_applied = false;
        std::string target_runtime_checkpoint_application_state;
        std::string target_runtime_checkpoint_application_state_reason;
        std::string target_runtime_checkpoint_application_outcome;
        bool target_runtime_checkpoint_application_outcome_available = false;
        std::string target_runtime_checkpoint_application_outcome_reason;
        bool target_runtime_player_placement_candidate = false;
        bool target_runtime_player_placement_allowed = false;
        bool target_runtime_player_placement_deferred = false;
        bool target_runtime_player_placement_attempted = false;
        bool target_runtime_player_placement_completed = false;
        bool target_runtime_player_placement_succeeded = false;
        bool target_runtime_player_placement_blocked = false;
        std::string target_runtime_player_placement_block_reason;
        bool target_runtime_player_placement_started = false;
        bool target_runtime_player_placed = false;
        std::string target_runtime_player_placement_state;
        std::string target_runtime_player_placement_state_reason;
        std::string target_runtime_player_placement_outcome;
        bool target_runtime_player_placement_outcome_available = false;
        std::string target_runtime_player_placement_outcome_reason;
        bool target_runtime_player_attachment_candidate = false;
        bool target_runtime_player_attachment_allowed = false;
        bool target_runtime_player_attachment_deferred = false;
        bool target_runtime_player_attachment_attempted = false;
        bool target_runtime_player_attachment_completed = false;
        bool target_runtime_player_attachment_succeeded = false;
        bool target_runtime_player_attachment_blocked = false;
        std::string target_runtime_player_attachment_block_reason;
        bool target_runtime_player_attachment_started = false;
        bool target_runtime_player_attached = false;
        std::string target_runtime_player_attachment_state;
        std::string target_runtime_player_attachment_state_reason;
        std::string target_runtime_player_attachment_outcome;
        bool target_runtime_player_attachment_outcome_available = false;
        std::string target_runtime_player_attachment_outcome_reason;
        bool target_runtime_player_attachment_ready = false;
        bool target_runtime_player_control_handoff_candidate = false;
        bool target_runtime_player_control_handoff_allowed = false;
        bool target_runtime_player_control_handoff_deferred = false;
        bool target_runtime_player_control_handoff_attempted = false;
        bool target_runtime_player_control_handoff_completed = false;
        bool target_runtime_player_control_handoff_succeeded = false;
        bool target_runtime_player_control_handoff_blocked = false;
        std::string target_runtime_player_control_handoff_block_reason;
        bool target_runtime_player_control_handoff_started = false;
        bool target_runtime_player_control_handed_off = false;
        std::string target_runtime_player_control_handoff_state;
        std::string target_runtime_player_control_handoff_state_reason;
        bool target_runtime_player_control_handoff_ready = false;
        std::string target_runtime_player_control_handoff_outcome;
        bool target_runtime_player_control_handoff_outcome_available = false;
        std::string target_runtime_player_control_handoff_outcome_reason;
        bool target_runtime_completion_candidate = false;
        bool target_runtime_completion_allowed = false;
        bool target_runtime_completion_deferred = false;
        bool target_runtime_completion_attempted = false;
        bool target_runtime_completion_completed = false;
        bool target_runtime_completion_succeeded = false;
        bool target_runtime_completion_blocked = false;
        std::string target_runtime_completion_block_reason;
        bool target_runtime_completion_started = false;
        bool target_runtime_completed = false;
        std::string target_runtime_completion_state;
        std::string target_runtime_completion_state_reason;
        std::string target_runtime_completion_outcome;
        bool target_runtime_completion_outcome_available = false;
        std::string target_runtime_completion_outcome_reason;
        bool target_runtime_completion_ready = false;
        bool target_runtime_completion_outcome_ready = false;
        bool target_runtime_completion_mark_candidate = false;
        bool target_runtime_completion_mark_allowed = false;
        bool target_runtime_completion_mark_deferred = false;
        bool target_runtime_completion_mark_attempted = false;
        bool target_runtime_completion_mark_completed = false;
        bool target_runtime_completion_mark_succeeded = false;
        bool target_runtime_completion_mark_blocked = false;
        std::string target_runtime_completion_mark_block_reason;
        bool target_runtime_completion_mark_started = false;
        bool target_runtime_completion_marked = false;
        std::string target_runtime_completion_mark_state;
        std::string target_runtime_completion_mark_state_reason;
        bool target_runtime_completion_mark_ready = false;
        bool target_runtime_completion_mark_gate_ready = false;
        bool target_runtime_completion_mark_state_ready = false;
        std::string action;
        std::string short_circuit_reason;
    };

    struct ChangeLevelPlayerTransferTargetRuntimeCompletionMarkOutcomeSummary
        : ChangeLevelPlayerTransferTargetRuntimeCompletionMarkStateSummary
    {
        std::string target_runtime_completion_mark_outcome;
        bool target_runtime_completion_mark_outcome_available = false;
        std::string target_runtime_completion_mark_outcome_reason;
        bool target_runtime_completion_mark_outcome_ready = false;
    };

    struct ChangeLevelPlayerTransferLatchReleaseGateSummary
        : ChangeLevelPlayerTransferTargetRuntimeCompletionMarkOutcomeSummary
    {
        bool changelevel_latch_release_candidate = false;
        bool changelevel_latch_release_allowed = false;
        bool changelevel_latch_release_deferred = false;
        bool changelevel_latch_release_attempted = false;
        bool changelevel_latch_release_completed = false;
        bool changelevel_latch_release_succeeded = false;
        bool changelevel_latch_release_blocked = false;
        std::string changelevel_latch_release_block_reason;
        bool changelevel_latch_release_started = false;
        bool changelevel_latch_released = false;
        bool changelevel_latch_release_ready = false;
        bool changelevel_latch_release_gate_ready = false;
    };

    struct ChangeLevelPlayerTransferLatchReleaseStateSummary
        : ChangeLevelPlayerTransferLatchReleaseGateSummary
    {
        std::string changelevel_latch_release_state;
        std::string changelevel_latch_release_state_reason;
        bool changelevel_latch_release_state_ready = false;
    };

    struct ChangeLevelPlayerTransferLatchReleaseOutcomeSummary
        : ChangeLevelPlayerTransferLatchReleaseStateSummary
    {
        std::string changelevel_latch_release_outcome;
        bool changelevel_latch_release_outcome_available = false;
        std::string changelevel_latch_release_outcome_reason;
        bool changelevel_latch_release_outcome_ready = false;
    };

    struct ChangeLevelPlayerTransferRequestClearGateSummary
        : ChangeLevelPlayerTransferLatchReleaseOutcomeSummary
    {
        bool changelevel_request_clear_candidate = false;
        bool changelevel_request_clear_allowed = false;
        bool changelevel_request_clear_deferred = false;
        bool changelevel_request_clear_attempted = false;
        bool changelevel_request_clear_completed = false;
        bool changelevel_request_clear_succeeded = false;
        bool changelevel_request_clear_blocked = false;
        std::string changelevel_request_clear_block_reason;
        bool changelevel_request_clear_started = false;
        bool changelevel_request_cleared = false;
        bool changelevel_request_clear_ready = false;
        bool changelevel_request_clear_gate_ready = false;
    };

    struct ChangeLevelPlayerTransferRequestClearStateSummary
        : ChangeLevelPlayerTransferRequestClearGateSummary
    {
        std::string changelevel_request_clear_state;
        std::string changelevel_request_clear_state_reason;
        bool changelevel_request_clear_state_ready = false;
    };

    struct ChangeLevelPlayerTransferRequestClearOutcomeSummary
        : ChangeLevelPlayerTransferRequestClearStateSummary
    {
        std::string changelevel_request_clear_outcome;
        bool changelevel_request_clear_outcome_available = false;
        std::string changelevel_request_clear_outcome_reason;
        bool changelevel_request_clear_outcome_ready = false;
    };

    struct PreChangeLevelHandoffSummary
    {
        bool active = false;
        bool handoff_latched = false;
        bool world_frozen = false;
        bool stop_requested = false;
        int request_frame = -1;
        float request_time = 0.0f;
        std::string world_state;
        std::string action;
        bool map_load_performed = false;
        std::string detail;
    };

    struct PostHandoffActivitySummary
    {
        bool measured = false;
        int handoff_frame = -1;
        float handoff_time = 0.0f;
        int observed_frames = 0;
        int scheduled_executed = 0;
        int dispatch_attempts = 0;
        int dispatch_successes = 0;
        int messages = 0;
        std::string first_action;
        std::string first_entity;
        int first_frame = -1;
        std::vector<std::string> sample_effects;
    };

    bool candidate_present = false;
    bool staged_supported = false;
    bool deferred_candidate = false;
    std::string target_map;
    std::string landmark;
    std::string source_classname;
    int source_edict_index = -1;
    std::string support_detail;
    bool pending_request_captured = false;
    int pending_request_frame = -1;
    float pending_request_time = 0.0f;
    std::string pending_request_detail;
    bool transition_intent_captured = false;
    bool transition_intent_consumed = false;
    int transition_intent_request_frame = -1;
    float transition_intent_request_time = 0.0f;
    std::string transition_intent_action;
    std::string transition_intent_detail;
    ChangeLevelTargetValidationSummary target_validation;
    ChangeLevelLifecycleGateSummary lifecycle_gate;
    ChangeLevelLifecycleEntrySummary lifecycle_entry;
    ChangeLevelLifecycleDispatchSummary lifecycle_dispatch;
    ChangeLevelLifecycleExecutionSummary lifecycle_execution;
    ChangeLevelBootstrapPlanSummary changelevel_bootstrap_plan;
    ChangeLevelLandmarkTransformSummary changelevel_landmark_transform;
    ChangeLevelProjectedCarriedOriginSummary changelevel_projected_carried_origin;
    ChangeLevelProjectedCarriedOrientationSummary changelevel_projected_carried_orientation;
    ChangeLevelProjectedTransferSnapshotSummary changelevel_projected_transfer_snapshot;
    ChangeLevelPlayerTransferApplyPlanSummary changelevel_player_transfer_apply_plan;
    ChangeLevelPlayerTransferWriteSetSummary changelevel_player_transfer_write_set;
    ChangeLevelPlayerTransferDeferredApplyGateSummary
        changelevel_player_transfer_deferred_apply_gate;
    ChangeLevelPlayerTransferGateOpenCheckpointSummary
        changelevel_player_transfer_gate_open_checkpoint;
    ChangeLevelPlayerTransferCheckpointSignalContractSummary
        changelevel_player_transfer_checkpoint_signal_contract;
    ChangeLevelPlayerTransferCheckpointSignalObservationStateSummary
        changelevel_player_transfer_checkpoint_signal_observation_state;
    ChangeLevelPlayerTransferCheckpointSignalHookPointSummary
        changelevel_player_transfer_checkpoint_signal_hook_point;
    ChangeLevelPlayerTransferCheckpointSignalHookRegistrationStateSummary
        changelevel_player_transfer_checkpoint_signal_hook_registration_state;
    ChangeLevelPlayerTransferCheckpointSignalHookInstallTokenSummary
        changelevel_player_transfer_checkpoint_signal_hook_install_token;
    ChangeLevelPlayerTransferCheckpointSignalHookInstallEligibilityGateSummary
        changelevel_player_transfer_checkpoint_signal_hook_install_eligibility_gate;
    ChangeLevelPlayerTransferCheckpointSignalHookInstallTokenIssueDecisionSummary
        changelevel_player_transfer_checkpoint_signal_hook_install_token_issue_decision;
    ChangeLevelPlayerTransferCheckpointSignalHookInstallAuthorizationStateSummary
        changelevel_player_transfer_checkpoint_signal_hook_install_authorization_state;
    ChangeLevelPlayerTransferCheckpointSignalHookInstallAttemptGateSummary
        changelevel_player_transfer_checkpoint_signal_hook_install_attempt_gate;
    ChangeLevelPlayerTransferCheckpointSignalHookInstallExecutionPlanSummary
        changelevel_player_transfer_checkpoint_signal_hook_install_execution_plan;
    ChangeLevelPlayerTransferCheckpointSignalHookInstallExecutionStateSummary
        changelevel_player_transfer_checkpoint_signal_hook_install_execution_state;
    ChangeLevelPlayerTransferCheckpointSignalHookInstallExecutionOutcomeSummary
        changelevel_player_transfer_checkpoint_signal_hook_install_execution_outcome;
    ChangeLevelPlayerTransferCheckpointSignalHookInstallResultStateSummary
        changelevel_player_transfer_checkpoint_signal_hook_install_result_state;
    ChangeLevelPlayerTransferCheckpointSignalHookInstallResultConsumptionGateSummary
        changelevel_player_transfer_checkpoint_signal_hook_install_result_consumption_gate;
    ChangeLevelPlayerTransferCheckpointSignalHookInstallResultConsumptionStateSummary
        changelevel_player_transfer_checkpoint_signal_hook_install_result_consumption_state;
    ChangeLevelPlayerTransferCheckpointSignalHookInstallResultConsumptionOutcomeSummary
        changelevel_player_transfer_checkpoint_signal_hook_install_result_consumption_outcome;
    ChangeLevelPlayerTransferCheckpointSignalHookInstallResultConsumerContractSummary
        changelevel_player_transfer_checkpoint_signal_hook_install_result_consumer_contract;
    ChangeLevelPlayerTransferCheckpointSignalHookInstallResultConsumerReadinessStateSummary
        changelevel_player_transfer_checkpoint_signal_hook_install_result_consumer_readiness_state;
    ChangeLevelPlayerTransferCheckpointSignalHookInstallResultConsumerActivationGateSummary
        changelevel_player_transfer_checkpoint_signal_hook_install_result_consumer_activation_gate;
    ChangeLevelPlayerTransferCheckpointSignalHookInstallResultConsumerActivationStateSummary
        changelevel_player_transfer_checkpoint_signal_hook_install_result_consumer_activation_state;
    ChangeLevelPlayerTransferCheckpointSignalHookInstallResultConsumerActivationOutcomeSummary
        changelevel_player_transfer_checkpoint_signal_hook_install_result_consumer_activation_outcome;
    ChangeLevelPlayerTransferCheckpointSignalHookInstallResultConsumerBindingContractSummary
        changelevel_player_transfer_checkpoint_signal_hook_install_result_consumer_binding_contract;
    ChangeLevelPlayerTransferCheckpointSignalHookInstallResultConsumerBindingReadinessStateSummary
        changelevel_player_transfer_checkpoint_signal_hook_install_result_consumer_binding_readiness_state;
    ChangeLevelPlayerTransferCheckpointSignalHookInstallResultConsumerBindingActivationGateSummary
        changelevel_player_transfer_checkpoint_signal_hook_install_result_consumer_binding_activation_gate;
    ChangeLevelPlayerTransferCheckpointSignalHookInstallResultConsumerBindingActivationStateSummary
        changelevel_player_transfer_checkpoint_signal_hook_install_result_consumer_binding_activation_state;
    ChangeLevelPlayerTransferCheckpointSignalHookInstallResultConsumerBindingActivationOutcomeSummary
        changelevel_player_transfer_checkpoint_signal_hook_install_result_consumer_binding_activation_outcome;
    ChangeLevelPlayerTransferCheckpointSignalHookInstallResultConsumerBindingOutcomeContractSummary
        changelevel_player_transfer_checkpoint_signal_hook_install_result_consumer_binding_outcome_contract;
    ChangeLevelPlayerTransferCheckpointSignalHookInstallResultConsumerBindingOutcomeReadinessStateSummary
        changelevel_player_transfer_checkpoint_signal_hook_install_result_consumer_binding_outcome_readiness_state;
    ChangeLevelPlayerTransferCheckpointSignalHookInstallResultConsumerBindingOutcomeActivationGateSummary
        changelevel_player_transfer_checkpoint_signal_hook_install_result_consumer_binding_outcome_activation_gate;
    ChangeLevelPlayerTransferCheckpointSignalHookInstallResultConsumerBindingOutcomeActivationStateSummary
        changelevel_player_transfer_checkpoint_signal_hook_install_result_consumer_binding_outcome_activation_state;
    ChangeLevelPlayerTransferCheckpointSignalHookInstallResultConsumerBindingOutcomeActivationOutcomeSummary
        changelevel_player_transfer_checkpoint_signal_hook_install_result_consumer_binding_outcome_activation_outcome;
    ChangeLevelPlayerTransferRuntimeIntegrationBlockerSnapshotSummary
        changelevel_player_transfer_runtime_integration_blocker_snapshot;
    ChangeLevelPlayerTransferRuntimeCheckpointObservationSummary
        changelevel_player_transfer_runtime_checkpoint_observation;
    ChangeLevelPlayerTransferTargetRuntimeCheckpointObservationSummary
        changelevel_player_transfer_target_runtime_checkpoint_observation;
    ChangeLevelPlayerTransferTargetRuntimeMaterializationPlanSummary
        changelevel_player_transfer_target_runtime_materialization_plan;
    ChangeLevelPlayerTransferTargetRuntimeBootstrapExecutionGateSummary
        changelevel_player_transfer_target_runtime_bootstrap_execution_gate;
    ChangeLevelPlayerTransferTargetRuntimeBootstrapExecutionStateSummary
        changelevel_player_transfer_target_runtime_bootstrap_execution_state;
    ChangeLevelPlayerTransferTargetRuntimeBootstrapExecutionOutcomeSummary
        changelevel_player_transfer_target_runtime_bootstrap_execution_outcome;
    ChangeLevelPlayerTransferTargetRuntimeBootstrapResultStateSummary
        changelevel_player_transfer_target_runtime_bootstrap_result_state;
    ChangeLevelPlayerTransferTargetRuntimeBootstrapResultConsumptionGateSummary
        changelevel_player_transfer_target_runtime_bootstrap_result_consumption_gate;
    ChangeLevelPlayerTransferTargetRuntimeBootstrapResultConsumptionStateSummary
        changelevel_player_transfer_target_runtime_bootstrap_result_consumption_state;
    ChangeLevelPlayerTransferTargetRuntimeBootstrapResultConsumptionOutcomeSummary
        changelevel_player_transfer_target_runtime_bootstrap_result_consumption_outcome;
    ChangeLevelPlayerTransferTargetRuntimeMaterializationGateSummary
        changelevel_player_transfer_target_runtime_materialization_gate;
    ChangeLevelPlayerTransferTargetRuntimeMaterializationStateSummary
        changelevel_player_transfer_target_runtime_materialization_state;
    ChangeLevelPlayerTransferTargetRuntimeMaterializationExecutionGuardAuditSummary
        changelevel_player_transfer_target_runtime_materialization_execution_guard_audit;
    ChangeLevelPlayerTransferTargetRuntimeMaterializationOutcomeSummary
        changelevel_player_transfer_target_runtime_materialization_outcome;
    ChangeLevelPlayerTransferTargetRuntimeActivationGateSummary
        changelevel_player_transfer_target_runtime_activation_gate;
    ChangeLevelPlayerTransferTargetRuntimeActivationStateSummary
        changelevel_player_transfer_target_runtime_activation_state;
    ChangeLevelPlayerTransferTargetRuntimeActivationExecutionGuardAuditSummary
        changelevel_player_transfer_target_runtime_activation_execution_guard_audit;
    ChangeLevelPlayerTransferTargetRuntimeActivationOutcomeSummary
        changelevel_player_transfer_target_runtime_activation_outcome;
    ChangeLevelPlayerTransferTargetRuntimeCutoverGateSummary
        changelevel_player_transfer_target_runtime_cutover_gate;
    ChangeLevelPlayerTransferTargetRuntimeCutoverStateSummary
        changelevel_player_transfer_target_runtime_cutover_state;
    ChangeLevelPlayerTransferTargetRuntimeCutoverExecutionGuardAuditSummary
        changelevel_player_transfer_target_runtime_cutover_execution_guard_audit;
    ChangeLevelPlayerTransferTargetRuntimeCutoverOutcomeSummary
        changelevel_player_transfer_target_runtime_cutover_outcome;
    ChangeLevelPlayerTransferCurrentRuntimeDeactivationGateSummary
        changelevel_player_transfer_current_runtime_deactivation_gate;
    ChangeLevelPlayerTransferCurrentRuntimeDeactivationStateSummary
        changelevel_player_transfer_current_runtime_deactivation_state;
    ChangeLevelPlayerTransferCurrentRuntimeDeactivationExecutionGuardAuditSummary
        changelevel_player_transfer_current_runtime_deactivation_execution_guard_audit;
    ChangeLevelPlayerTransferCurrentRuntimeDeactivationOutcomeSummary
        changelevel_player_transfer_current_runtime_deactivation_outcome;
    ChangeLevelPlayerTransferTargetRuntimeCheckpointApplicationGateSummary
        changelevel_player_transfer_target_runtime_checkpoint_application_gate;
    ChangeLevelPlayerTransferTargetRuntimeCheckpointApplicationStateSummary
        changelevel_player_transfer_target_runtime_checkpoint_application_state;
    ChangeLevelPlayerTransferTargetRuntimeCheckpointApplicationApplyGuardAuditSummary
        changelevel_player_transfer_target_runtime_checkpoint_application_apply_guard_audit;
    ChangeLevelPlayerTransferTargetRuntimeCheckpointApplicationOutcomeSummary
        changelevel_player_transfer_target_runtime_checkpoint_application_outcome;
    ChangeLevelPlayerTransferTargetRuntimePlayerPlacementGateSummary
        changelevel_player_transfer_target_runtime_player_placement_gate;
    ChangeLevelPlayerTransferTargetRuntimePlayerPlacementStateSummary
        changelevel_player_transfer_target_runtime_player_placement_state;
    ChangeLevelPlayerTransferTargetRuntimePlayerPlacementOutcomeSummary
        changelevel_player_transfer_target_runtime_player_placement_outcome;
    ChangeLevelPlayerTransferTargetRuntimePlayerAttachmentGateSummary
        changelevel_player_transfer_target_runtime_player_attachment_gate;
    ChangeLevelPlayerTransferTargetRuntimePlayerAttachmentStateSummary
        changelevel_player_transfer_target_runtime_player_attachment_state;
    ChangeLevelPlayerTransferTargetRuntimePlayerAttachmentOutcomeSummary
        changelevel_player_transfer_target_runtime_player_attachment_outcome;
    ChangeLevelPlayerTransferTargetRuntimePlayerControlHandoffGateSummary
        changelevel_player_transfer_target_runtime_player_control_handoff_gate;
    ChangeLevelPlayerTransferTargetRuntimePlayerControlHandoffStateSummary
        changelevel_player_transfer_target_runtime_player_control_handoff_state;
    ChangeLevelPlayerTransferTargetRuntimePlayerControlHandoffOutcomeSummary
        changelevel_player_transfer_target_runtime_player_control_handoff_outcome;
    ChangeLevelPlayerTransferTargetRuntimeCompletionGateSummary
        changelevel_player_transfer_target_runtime_completion_gate;
    ChangeLevelPlayerTransferTargetRuntimeCompletionStateSummary
        changelevel_player_transfer_target_runtime_completion_state;
    ChangeLevelPlayerTransferTargetRuntimeCompletionOutcomeSummary
        changelevel_player_transfer_target_runtime_completion_outcome;
    ChangeLevelPlayerTransferTargetRuntimeCompletionMarkGateSummary
        changelevel_player_transfer_target_runtime_completion_mark_gate;
    ChangeLevelPlayerTransferTargetRuntimeCompletionMarkStateSummary
        changelevel_player_transfer_target_runtime_completion_mark_state;
    ChangeLevelPlayerTransferTargetRuntimeCompletionMarkOutcomeSummary
        changelevel_player_transfer_target_runtime_completion_mark_outcome;
    ChangeLevelPlayerTransferLatchReleaseGateSummary
        changelevel_player_transfer_latch_release_gate;
    ChangeLevelPlayerTransferLatchReleaseStateSummary
        changelevel_player_transfer_latch_release_state;
    ChangeLevelPlayerTransferLatchReleaseOutcomeSummary
        changelevel_player_transfer_latch_release_outcome;
    ChangeLevelPlayerTransferRequestClearGateSummary
        changelevel_player_transfer_request_clear_gate;
    ChangeLevelPlayerTransferRequestClearStateSummary
        changelevel_player_transfer_request_clear_state;
    ChangeLevelPlayerTransferRequestClearOutcomeSummary
        changelevel_player_transfer_request_clear_outcome;
    PreChangeLevelHandoffSummary pre_changelevel_handoff;
    PostHandoffActivitySummary post_handoff_activity;
    bool touch_bounds_resolved = false;
    bool eligible_activator_observed = false;
    bool overlap_candidate_observed = false;
    bool surrogate_activator_available = false;
    bool moving_surrogate_active = false;
    bool moving_surrogate_initialized = false;
    int moving_surrogate_samples = 0;
    float moving_surrogate_initial_anchor_yaw = 0.0f;
    float moving_surrogate_local_offset_x = 0.0f;
    float moving_surrogate_local_offset_y = 0.0f;
    float moving_surrogate_local_offset_z = 0.0f;
    float trigger_bounds_absmin_x = 0.0f;
    float trigger_bounds_absmin_y = 0.0f;
    float trigger_bounds_absmin_z = 0.0f;
    float trigger_bounds_absmax_x = 0.0f;
    float trigger_bounds_absmax_y = 0.0f;
    float trigger_bounds_absmax_z = 0.0f;
    float surrogate_path_absmin_x = 0.0f;
    float surrogate_path_absmin_y = 0.0f;
    float surrogate_path_absmin_z = 0.0f;
    float surrogate_path_absmax_x = 0.0f;
    float surrogate_path_absmax_y = 0.0f;
    float surrogate_path_absmax_z = 0.0f;
    float closest_approach_distance = -1.0f;
    int closest_approach_frame = -1;
    float closest_approach_time = 0.0f;
    std::string moving_surrogate_local_offset_text;
    std::string trigger_bounds_text;
    std::string surrogate_path_envelope_text;
    std::string closest_surrogate_origin_text;
};

struct ScriptedLogicEntitySummary
{
    int edict_index = -1;
    std::size_t parse_index = 0;
    std::string classname;
    std::string targetname;
    std::string target;
    std::string actor_name;
    std::string actor_classname;
    std::string play;
    std::string idle;
    int move_to = 0;
    float radius = 0.0f;
    int spawnflags = 0;
    int received_use_count = 0;
    int emitted_target_count = 0;
    int scheduled_output_count = 0;
    int last_trigger_frame = -1;
    float last_trigger_time = 0.0f;
    std::string last_source_entity;
    std::string blocked_reason;
    ScriptedLogicSupportState support_state = ScriptedLogicSupportState::kPassiveRecipientOnly;
    bool progressed_this_frame = false;
    bool progressed_this_run = false;
    bool internal_state_changed = false;
    bool scheduled_follow_up = false;
    bool emitted_targets = false;
    bool actor_exists = false;
    bool path_linked = false;
    std::vector<std::string> path_links_preview;
};

struct ScriptedLogicClassSummary
{
    std::string classname;
    ScriptedLogicSupportState support_state = ScriptedLogicSupportState::kPassiveRecipientOnly;
    std::size_t entity_count = 0;
    std::size_t received_use_count = 0;
    std::size_t emitted_target_count = 0;
    std::size_t scheduled_output_count = 0;
    std::size_t progressing_count = 0;
    std::size_t blocked_count = 0;
};

struct ScriptedSequenceProgressSummary
{
    std::size_t total = 0;
    std::size_t received_use = 0;
    std::size_t changed_state = 0;
    std::size_t scheduled_follow_up = 0;
    std::size_t emitted_targets = 0;
    std::size_t progressing = 0;
    std::size_t blocked_on_movement = 0;
    std::size_t blocked_on_actor = 0;
    std::size_t blocked_on_engine_callback = 0;
};

struct PathTrackResolutionSummary
{
    std::size_t total_nodes = 0;
    std::size_t resolved_next_links = 0;
    std::size_t unresolved_next_links = 0;
    std::size_t message_links = 0;
    std::size_t resolved_message_targets = 0;
    std::size_t unresolved_message_targets = 0;
    std::size_t sequences_with_path_links = 0;
    std::size_t actors_with_path_links = 0;
    std::vector<std::string> preview;
};

struct ScriptedLogicFrameStateSummary
{
    int frame_number = 0;
    std::uint64_t host_frame_index = 0;
    std::uint64_t server_frame_index = 0;
    float time = 0.0f;
    float frametime = 0.0f;
    int queue_size_before = 0;
    int queue_size_after = 0;
    int due_delayed_actions = 0;
    int executed_delayed_actions = 0;
    int rescheduled_delayed_actions = 0;
    int skipped_delayed_actions = 0;
    int failed_delayed_actions = 0;
    int deferred_delayed_actions = 0;
    int target_chains_fired = 0;
    int target_resolutions = 0;
    int use_attempts = 0;
    int use_successes = 0;
    int use_deferred = 0;
    int use_failures = 0;
    int long_delay_pending = 0;
    int long_delay_executed = 0;
    std::size_t newly_progressed_scripted_entities = 0;
    std::size_t blocked_scripted_entities = 0;
    std::vector<NamedCountSummary> blocked_by_reason;
    std::vector<std::string> progressing_preview;
    std::vector<std::string> pending_delay_preview;
};

struct ScriptedLogicStateSummary
{
    bool configured = false;
    bool trace_scripted = false;
    int frames_attempted = 0;
    int frames_completed = 0;
    int total_received_use = 0;
    int total_emitted_targets = 0;
    int total_scheduled_outputs = 0;
    int total_internal_state_changes = 0;
    int total_scheduled_follow_ups = 0;
    int total_long_delay_executed = 0;
    int total_long_delay_pending = 0;
    std::size_t total_progressed_entities = 0;
    std::vector<NamedCountSummary> blocked_reasons;
    std::vector<ScriptedLogicClassSummary> classname_summary;
    std::vector<ScriptedLogicEntitySummary> progressing_entities_preview;
    std::vector<ScriptedLogicEntitySummary> blocked_entities_preview;
    ScriptedSequenceProgressSummary scripted_sequence_summary;
    PathTrackResolutionSummary path_track_summary;
    std::vector<ScriptedLogicFrameStateSummary> frames;
    std::string readiness;
};

struct ScriptedSceneRuntimeSummary
{
    int edict_index = -1;
    std::size_t parse_index = 0;
    std::string targetname;
    std::string classname;
    std::string entity_name;
    std::string play;
    std::string idle;
    int move_to = 0;
    float radius = 0.0f;
    float delay = 0.0f;
    std::string origin_text;
    std::string angles_text;
    bool actor_resolved = false;
    int actor_edict_index = -1;
    std::string actor_classname;
    std::string support_state;
    std::string blocked_reason;
    std::string stage;
    int last_progress_frame = -1;
    float last_progress_time = 0.0f;
    std::string mark_origin_text;
    float mark_yaw = 0.0f;
    float arrival_epsilon = 0.0f;
    std::string movement_mode;
    bool requires_move_to_mark = false;
    bool bootstrap_no_collision = false;
    bool stage_changed_this_frame = false;
    bool moving_this_frame = false;
    bool arrived_this_frame = false;
};

struct TrackPathGraphSummary
{
    std::size_t nodes = 0;
    std::size_t valid_links = 0;
    std::size_t broken_links = 0;
    std::size_t nodes_with_message = 0;
    std::size_t inactive_nodes = 0;
    std::size_t duplicate_targetnames = 0;
    std::size_t orphan_nodes = 0;
    std::size_t cycles = 0;
    std::vector<std::string> preview;
    std::vector<std::string> broken_link_preview;
    std::vector<std::string> duplicate_targetname_preview;
    std::vector<std::string> orphan_preview;
    std::vector<std::string> cycle_preview;
    std::vector<std::string> message_node_preview;
};

struct PathNodePresentationEventSummary
{
    std::string event_name;
    std::string source_node;
    int frame_number = -1;
    float time = 0.0f;
    bool dispatch_attempted = false;
    bool succeeded = false;
    bool deferred = false;
    std::string dispatch_mode;
    std::string classification;
    bool fade_channel_available = false;
    bool fade_channel_used = false;
    bool message_channel_available = false;
    bool message_channel_used = false;
    bool env_message_linkage_found = false;
    bool env_message_linkage_used = false;
    bool summary_only_fallback_used = false;
    std::string required_subsystem;
    std::string presentation_linkage_detail;
    std::string dispatch_detail;
};

struct PathNodeMessageCanarySummary
{
    std::string node_name;
    bool reached = false;
    int first_reached_frame = -1;
    float first_reached_time = 0.0f;
    bool message_encountered = false;
    bool staged_dispatch_attempted = false;
    std::string dispatch_result;
    std::string classification;
    std::string dispatch_mode;
    bool fade_channel_available = false;
    bool fade_channel_used = false;
    bool message_channel_available = false;
    bool message_channel_used = false;
    bool env_message_linkage_found = false;
    bool env_message_linkage_used = false;
    bool summary_only_fallback_used = false;
    float node_speed_metadata = 0.0f;
    float mover_speed_at_encounter = 0.0f;
    float mover_speed_before_encounter = 0.0f;
    float mover_speed_after_encounter = 0.0f;
    bool mover_speed_changed = false;
    int resolved_targets = 0;
    int runtime_target_candidates = 0;
    int parsed_target_candidates = 0;
    bool pfn_use_attempted = false;
    bool visible_downstream_progression = false;
    std::string message;
    std::vector<std::string> target_classnames;
    std::vector<std::string> resolved_target_details;
    std::string downstream_summary;
    std::string presentation_linkage_detail;
    std::string presentation_semantics_summary;
    std::string dispatch_detail;
    std::string required_subsystem;
    bool brush_door_handling_attempted = false;
    bool brush_door_use_succeeded = false;
    bool brush_door_state_changed = false;
    bool brush_door_movement_started = false;
    bool brush_door_movement_completed = false;
    bool brush_door_native_use_attempted = false;
    bool brush_door_native_use_succeeded = false;
    std::string brush_door_dispatch_path;
    std::string brush_door_support_state;
    std::string brush_door_state;
    std::string brush_door_blocked_reason;
    std::string brush_door_runtime_audit;
    int downstream_target_chains = 0;
    int downstream_scheduled_actions = 0;
    int downstream_alert_callbacks = 0;
    int downstream_message_callbacks = 0;
};

struct PathNodeMessageEncounterSummary
{
    std::string node_name;
    std::string message;
    int first_reached_frame = -1;
    float first_reached_time = 0.0f;
    float node_speed_metadata = 0.0f;
    float mover_speed_at_encounter = 0.0f;
    float mover_speed_before_encounter = 0.0f;
    float mover_speed_after_encounter = 0.0f;
    bool mover_speed_changed = false;
    bool dispatch_attempted = false;
    std::string dispatch_result;
    std::string classification;
    std::string dispatch_mode;
    bool fade_channel_available = false;
    bool fade_channel_used = false;
    bool message_channel_available = false;
    bool message_channel_used = false;
    bool env_message_linkage_found = false;
    bool env_message_linkage_used = false;
    bool summary_only_fallback_used = false;
    int resolved_targets = 0;
    int runtime_target_candidates = 0;
    int parsed_target_candidates = 0;
    bool pfn_use_attempted = false;
    bool visible_downstream_progression = false;
    std::vector<std::string> target_classnames;
    std::vector<std::string> resolved_target_details;
    std::string downstream_summary;
    std::string presentation_linkage_detail;
    std::string dispatch_detail;
    std::string required_subsystem;
    bool brush_door_handling_attempted = false;
    bool brush_door_use_succeeded = false;
    bool brush_door_state_changed = false;
    bool brush_door_movement_started = false;
    bool brush_door_movement_completed = false;
    bool brush_door_native_use_attempted = false;
    bool brush_door_native_use_succeeded = false;
    std::string brush_door_dispatch_path;
    std::string brush_door_support_state;
    std::string brush_door_state;
    std::string brush_door_blocked_reason;
    std::string brush_door_runtime_audit;
    int downstream_target_chains = 0;
    int downstream_scheduled_actions = 0;
    int downstream_alert_callbacks = 0;
    int downstream_message_callbacks = 0;
};

struct PathNodeMessageStateSummary
{
    std::size_t encountered = 0;
    std::size_t staged_dispatch_attempts = 0;
    std::size_t staged_dispatch_successes = 0;
    std::size_t staged_dispatch_deferred = 0;
    std::size_t staged_dispatch_failures = 0;
    std::size_t unresolved_message_targets = 0;
    std::size_t encountered_no_target = 0;
    std::size_t encountered_unresolved_targets = 0;
    std::size_t encountered_unsupported = 0;
    std::size_t resolved_and_dispatched = 0;
    bool first_message_bearing_node_reached = false;
    std::string first_message_bearing_node;
    int first_message_bearing_frame = -1;
    float first_message_bearing_time = 0.0f;
    std::string deepest_node_reached;
    std::vector<std::string> reached_message_nodes;
    std::vector<std::string> encountered_history;
    std::vector<std::string> dispatch_attempt_history;
    std::vector<std::string> dispatch_history;
    std::vector<std::string> rolling_trace;
    std::vector<PathNodePresentationEventSummary> presentation_events;
    std::vector<PathNodeMessageEncounterSummary> message_records;
    std::vector<PathNodeMessageCanarySummary> canaries;
};

struct PathMoverAggregateSummary
{
    std::size_t func_tracktrain_resolved = 0;
    std::size_t start_targets_resolved = 0;
    std::size_t track_bound = 0;
    std::size_t activated = 0;
    std::size_t moving = 0;
    std::size_t arrived_at_node = 0;
    std::size_t node_arrivals = 0;
    std::size_t path_advances = 0;
    std::size_t stopped = 0;
    std::size_t blocked = 0;
    std::size_t completed = 0;
    std::size_t staged_message_dispatches = 0;
    std::size_t broken_link_hits = 0;
};

struct PathMoverRuntimeSummary
{
    int edict_index = -1;
    std::size_t parse_index = 0;
    std::string targetname;
    std::string classname;
    std::string model;
    int modelindex = 0;
    std::string requested_start_node;
    std::string resolved_start_node;
    std::string previous_node;
    std::string current_node;
    std::string next_node;
    std::string stage;
    std::string resolution_mode;
    std::string resolution_detail;
    std::string last_use_source;
    std::string origin_text;
    bool start_target_resolved = false;
    bool graph_valid = false;
    bool path_bound = false;
    bool activated = false;
    bool moving = false;
    bool stage_changed_this_frame = false;
    bool arrived_this_frame = false;
    float base_speed = 0.0f;
    float speed = 0.0f;
    float effective_speed = 0.0f;
    float arrival_epsilon = 0.0f;
    float last_arrival_distance = 0.0f;
    std::string last_arrival_decision;
    std::string last_arrival_trigger;
    std::string last_arrival_origin_text;
    std::string last_arrival_target_origin_text;
    bool last_arrival_snap_applied = false;
    bool any_arrival_snap_applied = false;
    std::string speed_policy;
    std::string speed_policy_detail;
    std::string blocked_reason;
    std::string stopped_reason;
    int node_arrivals = 0;
    int node_advances = 0;
    std::string last_message;
    std::string last_message_node;
    std::string last_message_dispatch_result;
    int last_node_arrival_frame = -1;
    float last_node_arrival_time = 0.0f;
    bool staged_message_dispatch_attempted = false;
    bool staged_message_dispatch_succeeded = false;
    std::string staged_message_dispatch_detail;
    float last_node_speed_metadata = 0.0f;
    float last_speed_before_arrival = 0.0f;
    float last_speed_after_arrival = 0.0f;
    bool last_speed_changed_on_arrival = false;
    std::string last_speed_decision;
    std::string last_speed_decision_detail;
    int last_progress_frame = -1;
    float last_progress_time = 0.0f;
};

struct BrushDoorRuntimeSummary
{
    int edict_index = -1;
    std::size_t parse_index = 0;
    std::string classname;
    std::string targetname;
    std::string model;
    int modelindex = 0;
    std::string origin_text;
    std::string angles_text;
    std::string movedir_text;
    float speed = 0.0f;
    float lip = 0.0f;
    float wait = 0.0f;
    int spawnflags = 0;
    float health = 0.0f;
    bool health_available = false;
    float damage = 0.0f;
    bool damage_available = false;
    std::string lifecycle;
    std::string support_state;
    std::string dispatch_path;
    std::string movement_state;
    std::string last_source_event;
    std::string blocked_reason;
    std::string audit_line;
    bool resolved_runtime_target = false;
    bool active = false;
    bool native_use_attempted = false;
    bool native_use_succeeded = false;
    bool staged_bootstrap_attempted = false;
    bool staged_bootstrap_used = false;
    bool movement_started = false;
    bool movement_completed = false;
    int last_use_frame = -1;
    float last_use_time = 0.0f;
    int last_state_change_frame = -1;
    float last_state_change_time = 0.0f;
};

struct BrushDoorBootstrapStateSummary
{
    bool configured = false;
    bool controller_ran = false;
    int frames_attempted = 0;
    int frames_completed = 0;
    std::size_t tracked_doors = 0;
    std::size_t use_supported = 0;
    std::size_t moving = 0;
    std::size_t opened = 0;
    std::size_t blocked = 0;
    std::size_t deferred = 0;
    std::vector<BrushDoorRuntimeSummary> doors_preview;
    std::vector<std::string> transition_history;
    std::vector<std::string> exercised_callbacks;
    std::string readiness;
};

struct CanarySceneStateSummary
{
    std::string canary_name;
    std::string actor_name;
    std::string stage;
    std::string status;
    bool progressed_further_than_before = false;
};

struct ScriptedMovementFrameStateSummary
{
    int frame_number = 0;
    std::uint64_t host_frame_index = 0;
    std::uint64_t server_frame_index = 0;
    float time = 0.0f;
    float frametime = 0.0f;
    int scenes_moving = 0;
    int scene_arrivals = 0;
    int blocked_scenes = 0;
    int path_movers_active = 0;
    int path_movers_moving = 0;
    int blocked_path_movers = 0;
    int stopped_path_movers = 0;
    int path_node_arrivals = 0;
    int path_nodes_advanced = 0;
    int delayed_actions_due = 0;
    int delayed_actions_executed = 0;
    int delayed_actions_pending = 0;
    std::string ftruck_status;
    std::vector<NamedCountSummary> blocked_by_reason;
    std::vector<std::string> canary_status;
    std::vector<std::string> active_path_movers;
    std::vector<std::string> path_messages_this_frame;
    std::vector<std::string> path_events;
};

struct ScriptedMovementStateSummary
{
    bool configured = false;
    bool attempted = false;
    bool controller_ran = false;
    bool trace_movement = false;
    std::string bootstrap_mode;
    int frames_attempted = 0;
    int frames_completed = 0;
    std::size_t actor_resolutions_attempted = 0;
    std::size_t actor_resolutions_succeeded = 0;
    std::size_t actor_resolutions_failed = 0;
    std::size_t scene_movement_attempts = 0;
    std::size_t scene_movement_successes = 0;
    std::size_t scene_movement_blocked = 0;
    std::vector<NamedCountSummary> movement_blocked_reasons;
    std::vector<NamedCountSummary> scene_stage_summary;
    TrackPathGraphSummary path_graph;
    PathMoverAggregateSummary path_movers;
    float path_arrival_epsilon = 0.0f;
    bool path_snap_to_node_occurred = false;
    bool flatbedstart_resolved = false;
    std::string delayed_ftruck_status;
    std::string ftruck_final_current;
    std::string ftruck_final_next;
    bool ftruck_advanced_beyond_trainstop1a = false;
    PathNodeMessageStateSummary path_node_messages;
    BrushDoorBootstrapStateSummary brush_doors;
    std::vector<std::string> path_messages_encountered;
    std::size_t path_node_message_dispatch_count = 0;
    bool path_node_message_triggered_dispatch = false;
    std::vector<std::string> path_message_dispatch_history;
    std::vector<std::string> broken_path_link_events;
    std::vector<CanarySceneStateSummary> canaries;
    std::vector<ScriptedSceneRuntimeSummary> scenes_preview;
    std::vector<PathMoverRuntimeSummary> path_movers_preview;
    std::vector<ScriptedMovementFrameStateSummary> frames;
    std::vector<std::string> exercised_callbacks;
    std::vector<std::string> path_mover_callbacks_exercised;
    std::string readiness;
};

struct DedicatedPlayerSlotSummary
{
    int slot = 0;
    std::string session_id;
    std::string player_name;
    std::string lifecycle_state;
    bool connected = false;
    bool put_in_server = false;
    bool alive = false;
    bool signon_ready = false;
    bool bootstrap_delivered = false;
    bool baseline_ready = false;
    bool bootstrap_sequence_completed = false;
    bool signon_catalog_ready = false;
    bool bootstrap_records_staged = false;
    bool signon_template_ready = false;
    bool template_records_delivered = false;
    bool signon_template_coverage_complete = false;
    bool remaining_template_records_delivered = false;
    bool signon_envelope_ready = false;
    bool framed_template_records_delivered = false;
    bool signon_batch_ready = false;
    bool pseudo_packet_batch_delivered = false;
    bool signon_wiremap_ready = false;
    bool wiremapped_batch_delivered = false;
    bool signon_burst_ready = false;
    bool pseudo_wire_burst_delivered = false;
    bool signon_stream_ready = false;
    bool contiguous_stream_delivered = false;
    bool signon_stream_window_ready = false;
    bool windowed_stream_delivered = false;
    bool signon_message_catalog_ready = false;
    bool message_boundaries_cataloged = false;
    bool signon_message_fetch_ready = false;
    bool targeted_message_fetch_delivered = false;
    bool signon_multi_message_fetch_ready = false;
    bool selective_message_set_delivered = false;
    bool signon_message_range_fetch_ready = false;
    bool range_message_set_delivered = false;
    bool signon_message_cursor_ready = false;
    bool message_cursor_checkpointed = false;
    bool signon_message_cursor_resume_ready = false;
    bool non_exhausted_cursor_resume_allowed = false;
    bool signon_message_cursor_carryover_ready = false;
    bool checkpoint_cursor_carried_over = false;
    bool signon_message_cursor_carried_resume_ready = false;
    bool non_exhausted_carried_cursor_resume_allowed = false;
    bool signon_message_cursor_carried_checkpoint_ready = false;
    bool carried_cursor_checkpointed = false;
    bool signon_message_cursor_carried_checkpoint_resume_ready = false;
    bool non_exhausted_carried_checkpoint_resume_allowed = false;
    bool signon_message_cursor_carried_checkpoint_resume_token_ready = false;
    bool carried_checkpoint_resume_token_issued = false;
    bool signon_message_cursor_carried_checkpoint_resume_token_claim_ready = false;
    bool carried_checkpoint_resume_token_claimed = false;
    bool signon_message_cursor_carried_checkpoint_claimed_resume_ready = false;
    bool non_exhausted_claimed_checkpoint_resume_allowed = false;
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_bridge_ready =
        false;
    bool claimed_checkpoint_bridge_materialized = false;
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_resume_ready =
        false;
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_resume_token_ready =
        false;
    bool claimed_checkpoint_resume_token_issued = false;
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_resume_token_claim_ready =
        false;
    bool claimed_checkpoint_resume_token_claimed = false;
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_resume_ready =
        false;
    bool non_exhausted_claimed_checkpoint_successor_resume_allowed = false;
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_bridge_ready =
        false;
    bool claimed_checkpoint_successor_bridge_materialized = false;
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_ready =
        false;
    bool non_exhausted_claimed_checkpoint_successor_checkpoint_resume_allowed =
        false;
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_ready =
        false;
    bool claimed_checkpoint_successor_resume_token_issued = false;
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_ready =
        false;
    bool claimed_checkpoint_successor_resume_token_claimed = false;
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_resume_ready =
        false;
    bool non_exhausted_claimed_checkpoint_successor_resume_token_claim_resume_allowed =
        false;
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_resume_range_ready =
        false;
    bool claimed_checkpoint_successor_resume_token_claim_resume_range_delivered =
        false;
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_resume_eof_ready =
        false;
    bool claimed_checkpoint_successor_resume_token_claim_resume_exhausted = false;
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_resumed_denied_ready =
        false;
    bool claimed_checkpoint_successor_resume_token_claim_resumed_denied = false;
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_bridge_ready =
        false;
    bool claimed_checkpoint_successor_resume_token_claim_bridge_materialized = false;
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_ready =
        false;
    bool non_exhausted_claimed_checkpoint_successor_resume_token_claim_checkpoint_resume_allowed =
        false;
    bool
        signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_ready =
            false;
    bool
        claimed_checkpoint_successor_resume_token_claim_checkpoint_resume_token_issued =
            false;
    bool
        signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_ready =
            false;
    bool
        claimed_checkpoint_successor_resume_token_claim_checkpoint_resume_token_claimed =
            false;
    bool
        signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_resume_ready =
            false;
    bool
        non_exhausted_claimed_checkpoint_successor_resume_token_claim_checkpoint_resume_token_claim_resume_allowed =
            false;
    bool
        signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_range_ready =
            false;
    bool
        claimed_checkpoint_successor_resume_token_claim_checkpoint_resume_token_claim_range_delivered =
            false;
    bool
        signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_range_ready =
            false;
    bool
        claimed_checkpoint_successor_resume_token_claim_checkpoint_resume_range_delivered =
            false;
    bool
        signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_eof_ready =
            false;
    bool
        claimed_checkpoint_successor_resume_token_claim_checkpoint_resume_exhausted =
            false;
    bool
        signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resumed_denied_ready =
            false;
    bool
        claimed_checkpoint_successor_resume_token_claim_checkpoint_resumed_denied =
            false;
    bool
        signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_advance_ready =
            false;
    bool claimed_checkpoint_successor_resume_token_claim_advanced_range_delivered =
        false;
    bool
        signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_eof_ready =
            false;
    bool claimed_checkpoint_successor_resume_token_claim_bridge_exhausted =
        false;
    bool
        signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_denied_ready =
            false;
    bool claimed_checkpoint_successor_resume_token_claim_bridge_resume_denied =
        false;
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_range_ready =
        false;
    bool claimed_checkpoint_successor_resume_token_claim_range_delivered = false;
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_eof_ready =
        false;
    bool claimed_checkpoint_successor_resume_token_claim_exhausted = false;
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_resume_denied_ready =
        false;
    bool claimed_checkpoint_successor_resume_token_claim_resume_denied = false;
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_eof_ready =
        false;
    bool claimed_checkpoint_successor_resume_token_claim_checkpoint_resume_token_claim_exhausted =
        false;
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_resume_denied_ready =
        false;
    bool claimed_checkpoint_successor_resume_token_claim_checkpoint_resume_token_claim_resume_denied =
        false;
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_range_ready =
        false;
    bool claimed_checkpoint_successor_checkpoint_resume_range_delivered = false;
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_eof_ready =
        false;
    bool claimed_checkpoint_successor_checkpoint_resume_exhausted = false;
    bool
        signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resumed_denied_ready =
            false;
    bool claimed_checkpoint_successor_checkpoint_resumed_denied = false;
    bool
        signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_advance_ready =
            false;
    bool claimed_checkpoint_successor_advanced_range_delivered = false;
    bool
        signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_eof_ready =
            false;
    bool claimed_checkpoint_successor_bridge_exhausted = false;
    bool
        signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_denied_ready =
            false;
    bool claimed_checkpoint_successor_bridge_resume_denied = false;
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_resume_range_ready =
        false;
    bool claimed_checkpoint_successor_resume_range_delivered = false;
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_resume_eof_ready =
        false;
    bool claimed_checkpoint_successor_resume_exhausted = false;
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_resumed_denied_ready =
        false;
    bool claimed_checkpoint_successor_resumed_denied = false;
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_range_ready =
        false;
    bool claimed_checkpoint_successor_range_delivered = false;
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_eof_ready =
        false;
    bool claimed_checkpoint_successor_exhausted = false;
    bool
        signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_resume_denied_ready =
            false;
    bool claimed_checkpoint_successor_resume_denied = false;
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_resume_range_ready =
        false;
    bool claimed_checkpoint_bridge_resume_range_delivered = false;
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_advance_ready =
        false;
    bool claimed_checkpoint_advanced_range_delivered = false;
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_eof_ready =
        false;
    bool claimed_checkpoint_bridge_exhausted = false;
    bool signon_message_cursor_carried_checkpoint_claimed_checkpoint_resume_denied_ready =
        false;
    bool claimed_checkpoint_bridge_resume_denied = false;
    bool signon_message_cursor_carried_checkpoint_claimed_resume_range_ready = false;
    bool claimed_checkpoint_resume_range_delivered = false;
    bool signon_message_cursor_carried_checkpoint_claimed_resume_eof_ready = false;
    bool claimed_checkpoint_resume_exhausted = false;
    bool signon_message_cursor_carried_checkpoint_claimed_resumed_denied_ready = false;
    bool claimed_checkpoint_resumed_denied = false;
    bool signon_message_cursor_carried_checkpoint_claimed_range_ready = false;
    bool claimed_checkpoint_range_delivered = false;
    bool signon_message_cursor_carried_checkpoint_claimed_eof_ready = false;
    bool claimed_checkpoint_exhausted = false;
    bool signon_message_cursor_carried_checkpoint_claimed_resume_denied_ready = false;
    bool claimed_checkpoint_resume_denied = false;
    bool signon_message_cursor_carried_checkpoint_resume_range_ready = false;
    bool carried_checkpoint_resume_range_delivered = false;
    bool signon_message_cursor_carried_checkpoint_resume_eof_ready = false;
    bool carried_checkpoint_resume_exhausted = false;
    bool signon_message_cursor_carried_checkpoint_resumed_denied_ready = false;
    bool carried_checkpoint_resumed_denied = false;
    bool signon_message_cursor_carried_checkpoint_advance_ready = false;
    bool carried_checkpoint_advanced_range_delivered = false;
    bool signon_message_cursor_carried_checkpoint_eof_ready = false;
    bool carried_checkpoint_exhausted = false;
    bool signon_message_cursor_carried_checkpoint_resume_denied_ready = false;
    bool carried_checkpoint_resume_denied = false;
    bool signon_message_cursor_carried_range_ready = false;
    bool carried_cursor_range_delivered = false;
    bool signon_message_cursor_carried_eof_ready = false;
    bool carried_cursor_exhausted = false;
    bool signon_message_cursor_carried_resume_denied_ready = false;
    bool carried_cursor_resume_denied = false;
    bool signon_message_cursor_advance_ready = false;
    bool cursor_advanced_range_delivered = false;
    bool signon_message_cursor_eof_ready = false;
    bool message_cursor_exhausted = false;
    bool signon_message_cursor_resume_denied_ready = false;
    bool exhausted_cursor_resume_denied = false;
    int spawn_count = 0;
    int death_count = 0;
    int respawn_count = 0;
    std::string last_origin;
    std::string last_detail;
};

struct DedicatedPlayerLifecycleEventSummary
{
    int sequence = 0;
    int slot = 0;
    std::string session_id;
    std::string player_name;
    std::string lifecycle_state;
    std::string detail;
};

struct DedicatedServerFoundationSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string ruleset = "singleplayer";
    std::string mod_name;
    float deathmatch = 0.0f;
    float coop = 0.0f;
    int requested_maxclients = 0;
    int effective_maxclients = 0;
    int reserved_client_slots = 0;
    int first_non_client_slot = 0;
    bool map_load = false;
    bool bsp_switch = false;
    bool changelevel_execution = false;
    bool authoritative = false;
    std::string transport = "n/a";
    std::string query_surface = "n/a";
};

struct DedicatedPlayerLifecycleFoundationSummary
{
    bool enabled = false;
    std::string mode = "listen";
    int synthetic_players = 0;
    int admitted = 0;
    int connected = 0;
    int put_in_server = 0;
    int spawned = 0;
    int signon_ready = 0;
    int bootstrap_delivered = 0;
    int baseline_ready = 0;
    int bootstrap_sequence_completed = 0;
    int signon_catalog_ready = 0;
    int bootstrap_records_staged = 0;
    int signon_template_ready = 0;
    int template_records_delivered = 0;
    int signon_template_coverage_complete = 0;
    int remaining_template_records_delivered = 0;
    int signon_envelope_ready = 0;
    int framed_template_records_delivered = 0;
    int signon_batch_ready = 0;
    int pseudo_packet_batch_delivered = 0;
    int signon_wiremap_ready = 0;
    int wiremapped_batch_delivered = 0;
    int signon_burst_ready = 0;
    int pseudo_wire_burst_delivered = 0;
    int signon_stream_ready = 0;
    int contiguous_stream_delivered = 0;
    int signon_stream_window_ready = 0;
    int windowed_stream_delivered = 0;
    int signon_message_catalog_ready = 0;
    int message_boundaries_cataloged = 0;
    int signon_message_fetch_ready = 0;
    int targeted_message_fetch_delivered = 0;
    int signon_multi_message_fetch_ready = 0;
    int selective_message_set_delivered = 0;
    int signon_message_range_fetch_ready = 0;
    int range_message_set_delivered = 0;
    int signon_message_cursor_ready = 0;
    int message_cursor_checkpointed = 0;
    int signon_message_cursor_resume_ready = 0;
    int non_exhausted_cursor_resume_allowed = 0;
    int signon_message_cursor_carryover_ready = 0;
    int checkpoint_cursor_carried_over = 0;
    int signon_message_cursor_carried_resume_ready = 0;
    int non_exhausted_carried_cursor_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_ready = 0;
    int carried_cursor_checkpointed = 0;
    int signon_message_cursor_carried_checkpoint_resume_ready = 0;
    int non_exhausted_carried_checkpoint_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_resume_token_ready = 0;
    int carried_checkpoint_resume_token_issued = 0;
    int signon_message_cursor_carried_checkpoint_resume_token_claim_ready = 0;
    int carried_checkpoint_resume_token_claimed = 0;
    int signon_message_cursor_carried_checkpoint_claimed_resume_ready = 0;
    int non_exhausted_claimed_checkpoint_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_bridge_ready = 0;
    int claimed_checkpoint_bridge_materialized = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_resume_ready = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_resume_token_ready = 0;
    int claimed_checkpoint_resume_token_issued = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_resume_token_claim_ready = 0;
    int claimed_checkpoint_resume_token_claimed = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_resume_ready = 0;
    int non_exhausted_claimed_checkpoint_successor_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_bridge_ready =
        0;
    int claimed_checkpoint_successor_bridge_materialized = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_ready =
        0;
    int non_exhausted_claimed_checkpoint_successor_checkpoint_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_ready =
        0;
    int claimed_checkpoint_successor_resume_token_issued = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claimed = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_resume_ready =
        0;
    int non_exhausted_claimed_checkpoint_successor_resume_token_claim_resume_allowed =
        0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_resume_range_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claim_resume_range_delivered =
        0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_resume_eof_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claim_resume_exhausted = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_resumed_denied_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claim_resumed_denied = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_bridge_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claim_bridge_materialized = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_ready =
        0;
    int non_exhausted_claimed_checkpoint_successor_resume_token_claim_checkpoint_resume_allowed =
        0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claim_checkpoint_resume_token_issued =
        0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claim_checkpoint_resume_token_claimed =
        0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_resume_ready =
        0;
    int non_exhausted_claimed_checkpoint_successor_resume_token_claim_checkpoint_resume_token_claim_resume_allowed =
        0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_resume_range_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claim_checkpoint_resume_token_claim_resume_range_delivered =
        0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_resume_eof_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claim_checkpoint_resume_token_claim_resume_exhausted =
        0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_range_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claim_checkpoint_resume_token_claim_range_delivered =
        0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_range_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claim_checkpoint_resume_range_delivered =
        0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_eof_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claim_checkpoint_resume_exhausted =
        0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resumed_denied_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claim_checkpoint_resumed_denied =
        0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_advance_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claim_advanced_range_delivered =
        0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_eof_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claim_bridge_exhausted = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_denied_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claim_bridge_resume_denied = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_range_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claim_range_delivered = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_eof_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claim_exhausted = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_resume_denied_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claim_resume_denied = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_eof_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claim_checkpoint_resume_token_claim_exhausted =
        0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_resume_denied_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claim_checkpoint_resume_token_claim_resume_denied =
        0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_range_ready =
        0;
    int claimed_checkpoint_successor_checkpoint_resume_range_delivered = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_eof_ready =
        0;
    int claimed_checkpoint_successor_checkpoint_resume_exhausted = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resumed_denied_ready =
        0;
    int claimed_checkpoint_successor_checkpoint_resumed_denied = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_advance_ready =
        0;
    int claimed_checkpoint_successor_advanced_range_delivered = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_eof_ready =
        0;
    int claimed_checkpoint_successor_bridge_exhausted = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_denied_ready =
        0;
    int claimed_checkpoint_successor_bridge_resume_denied = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_resume_range_ready = 0;
    int claimed_checkpoint_successor_resume_range_delivered = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_resume_eof_ready =
        0;
    int claimed_checkpoint_successor_resume_exhausted = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_resumed_denied_ready =
        0;
    int claimed_checkpoint_successor_resumed_denied = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_range_ready = 0;
    int claimed_checkpoint_successor_range_delivered = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_eof_ready = 0;
    int claimed_checkpoint_successor_exhausted = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_resume_denied_ready =
        0;
    int claimed_checkpoint_successor_resume_denied = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_resume_range_ready = 0;
    int claimed_checkpoint_bridge_resume_range_delivered = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_resume_eof_ready = 0;
    int claimed_checkpoint_bridge_resume_exhausted = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_resumed_denied_ready = 0;
    int claimed_checkpoint_bridge_resumed_denied = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_advance_ready = 0;
    int claimed_checkpoint_advanced_range_delivered = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_eof_ready = 0;
    int claimed_checkpoint_bridge_exhausted = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_resume_denied_ready = 0;
    int claimed_checkpoint_bridge_resume_denied = 0;
    int signon_message_cursor_carried_checkpoint_claimed_resume_range_ready = 0;
    int claimed_checkpoint_resume_range_delivered = 0;
    int signon_message_cursor_carried_checkpoint_claimed_resume_eof_ready = 0;
    int claimed_checkpoint_resume_exhausted = 0;
    int signon_message_cursor_carried_checkpoint_claimed_resumed_denied_ready = 0;
    int claimed_checkpoint_resumed_denied = 0;
    int signon_message_cursor_carried_checkpoint_claimed_range_ready = 0;
    int claimed_checkpoint_range_delivered = 0;
    int signon_message_cursor_carried_checkpoint_claimed_eof_ready = 0;
    int claimed_checkpoint_exhausted = 0;
    int signon_message_cursor_carried_checkpoint_claimed_resume_denied_ready = 0;
    int claimed_checkpoint_resume_denied = 0;
    int signon_message_cursor_carried_checkpoint_resume_range_ready = 0;
    int carried_checkpoint_resume_range_delivered = 0;
    int signon_message_cursor_carried_checkpoint_resume_eof_ready = 0;
    int carried_checkpoint_resume_exhausted = 0;
    int signon_message_cursor_carried_checkpoint_resumed_denied_ready = 0;
    int carried_checkpoint_resumed_denied = 0;
    int signon_message_cursor_carried_checkpoint_advance_ready = 0;
    int carried_checkpoint_advanced_range_delivered = 0;
    int signon_message_cursor_carried_checkpoint_eof_ready = 0;
    int carried_checkpoint_exhausted = 0;
    int signon_message_cursor_carried_checkpoint_resume_denied_ready = 0;
    int carried_checkpoint_resume_denied = 0;
    int signon_message_cursor_carried_range_ready = 0;
    int carried_cursor_range_delivered = 0;
    int signon_message_cursor_carried_eof_ready = 0;
    int carried_cursor_exhausted = 0;
    int signon_message_cursor_carried_resume_denied_ready = 0;
    int carried_cursor_resume_denied = 0;
    int signon_message_cursor_advance_ready = 0;
    int cursor_advanced_range_delivered = 0;
    int signon_message_cursor_eof_ready = 0;
    int message_cursor_exhausted = 0;
    int signon_message_cursor_resume_denied_ready = 0;
    int exhausted_cursor_resume_denied = 0;
    int deaths = 0;
    int respawns = 0;
    int disconnected = 0;
    int reserved_client_slots = 0;
    bool authoritative = false;
    std::string scoreboard_ready = "no";
    std::string replication_ready = "no";
    std::vector<DedicatedPlayerSlotSummary> slots;
    std::vector<DedicatedPlayerLifecycleEventSummary> events;
};

struct DedicatedQuerySurfaceSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string ruleset = "singleplayer";
    std::string mod_name;
    std::string bind = "disabled";
    int requested_port = 0;
    int bound_port = 0;
    std::string protocol_shape = "disabled";
    std::string authoritative_player_source = "n/a";
    int players = 0;
    int max_players = 0;
    std::string map_name;
    std::string server_name;
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedQueryProbeSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string probe = "disabled";
    std::string request_type = "disabled";
    bool response_received = false;
    bool parse_ok = false;
    int parsed_players = 0;
    int parsed_max_players = 0;
    std::string parsed_map;
    std::string parsed_name;
    std::string protocol_shape = "disabled";
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedConnectSurfaceSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string bind = "disabled";
    int requested_port = 0;
    int bound_port = 0;
    bool shared_with_query = false;
    std::string protocol_shape = "disabled";
    bool challenge_enabled = false;
    bool connect_enabled = false;
    std::string auth = "disabled";
    std::string signon = "disabled";
    std::string authoritative_admission = "n/a";
    int players = 0;
    int max_players = 0;
    int accepted = 0;
    int rejected = 0;
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedConnectProbeSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string probe = "disabled";
    int attempts = 0;
    int challenge_received = 0;
    int connect_attempted = 0;
    int accepted = 0;
    int rejected = 0;
    std::string last_reject_reason = "<none>";
    int post_admission_players = 0;
    int post_admission_max_players = 0;
    std::string protocol_shape = "disabled";
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedActivationSurfaceSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string bind = "disabled";
    int requested_port = 0;
    int bound_port = 0;
    bool shared_with_query = false;
    bool shared_with_connect = false;
    std::string protocol_shape = "disabled";
    bool activation_enabled = false;
    bool requires_accepted_admission = true;
    std::string activates_to = "put_in_server+spawned";
    std::string auth = "disabled";
    std::string signon = "disabled";
    std::string gameplay_transport = "no";
    int accepted = 0;
    int rejected = 0;
    int put_in_server = 0;
    int spawned = 0;
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedActivationProbeSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string probe = "disabled";
    int attempts = 0;
    int accepted = 0;
    int rejected = 0;
    std::string last_reject_reason = "<none>";
    int activated_put_in_server = 0;
    int activated_spawned = 0;
    int post_activation_players = 0;
    int post_activation_max_players = 0;
    std::string protocol_shape = "disabled";
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedBootstrapSurfaceSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string bind = "disabled";
    int requested_port = 0;
    int bound_port = 0;
    bool shared_with_query = false;
    bool shared_with_connect = false;
    bool shared_with_activation = false;
    std::string protocol_shape = "disabled";
    bool bootstrap_enabled = false;
    bool requires_activated_session = true;
    std::string bootstrap_state = "disabled";
    std::string bootstrap_payload = "disabled";
    std::string auth = "disabled";
    std::string signon = "disabled";
    std::string gameplay_transport = "no";
    int accepted = 0;
    int rejected = 0;
    int signon_ready = 0;
    int bootstrap_delivered = 0;
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedBootstrapProbeSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string probe = "disabled";
    int attempts = 0;
    int accepted = 0;
    int rejected = 0;
    std::string last_reject_reason = "<none>";
    std::string parsed_session;
    int parsed_slot = 0;
    std::string parsed_map;
    std::string parsed_name;
    std::string parsed_ruleset;
    int parsed_spawned = 0;
    int signon_ready = 0;
    int bootstrap_delivered = 0;
    std::string protocol_shape = "disabled";
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedBootstrapSequenceSurfaceSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string bind = "disabled";
    int requested_port = 0;
    int bound_port = 0;
    bool shared_with_query = false;
    bool shared_with_connect = false;
    bool shared_with_activation = false;
    bool shared_with_bootstrap = false;
    std::string protocol_shape = "disabled";
    bool sequence_enabled = false;
    bool requires_bootstrapped_session = true;
    std::string sequence_payload = "disabled";
    int sequence_steps = 0;
    std::string auth = "disabled";
    std::string signon = "disabled";
    std::string gameplay_transport = "no";
    int accepted = 0;
    int rejected = 0;
    int signon_ready = 0;
    int bootstrap_delivered = 0;
    int baseline_ready = 0;
    int bootstrap_sequence_completed = 0;
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedBootstrapSequenceProbeSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string probe = "disabled";
    int attempts = 0;
    int accepted = 0;
    int rejected = 0;
    std::string last_reject_reason = "<none>";
    std::string parsed_session;
    int parsed_step_count = 0;
    int parsed_final_step = -1;
    std::string parsed_map;
    std::string parsed_name;
    std::string parsed_ruleset;
    int parsed_spawned = 0;
    int signon_ready = 0;
    int bootstrap_delivered = 0;
    int baseline_ready = 0;
    int bootstrap_sequence_completed = 0;
    std::string protocol_shape = "disabled";
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonCatalogSurfaceSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string bind = "disabled";
    int requested_port = 0;
    int bound_port = 0;
    bool shared_with_query = false;
    bool shared_with_connect = false;
    bool shared_with_activation = false;
    bool shared_with_bootstrap = false;
    bool shared_with_bootstrap_sequence = false;
    std::string protocol_shape = "disabled";
    bool catalog_enabled = false;
    bool requires_sequenced_session = true;
    std::string catalog_payload = "disabled";
    int catalog_records = 0;
    std::string auth = "disabled";
    std::string signon = "disabled";
    std::string gameplay_transport = "no";
    int accepted = 0;
    int rejected = 0;
    int signon_ready = 0;
    int bootstrap_delivered = 0;
    int baseline_ready = 0;
    int bootstrap_sequence_completed = 0;
    int signon_catalog_ready = 0;
    int bootstrap_records_staged = 0;
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonCatalogProbeSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string probe = "disabled";
    int attempts = 0;
    int accepted = 0;
    int rejected = 0;
    std::string last_reject_reason = "<none>";
    std::string parsed_session;
    int parsed_record_count = 0;
    int parsed_final_record = -1;
    std::string parsed_map;
    std::string parsed_name;
    std::string parsed_ruleset;
    int parsed_spawned = 0;
    int signon_ready = 0;
    int bootstrap_delivered = 0;
    int baseline_ready = 0;
    int bootstrap_sequence_completed = 0;
    int signon_catalog_ready = 0;
    int bootstrap_records_staged = 0;
    std::string protocol_shape = "disabled";
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonTemplateSurfaceSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string bind = "disabled";
    int requested_port = 0;
    int bound_port = 0;
    bool shared_with_query = false;
    bool shared_with_connect = false;
    bool shared_with_activation = false;
    bool shared_with_bootstrap = false;
    bool shared_with_bootstrap_sequence = false;
    bool shared_with_signon_catalog = false;
    std::string protocol_shape = "disabled";
    bool template_enabled = false;
    bool requires_cataloged_session = true;
    std::string template_payload = "disabled";
    int template_records = 0;
    std::string auth = "disabled";
    std::string signon = "disabled";
    std::string gameplay_transport = "no";
    int accepted = 0;
    int rejected = 0;
    int signon_ready = 0;
    int bootstrap_delivered = 0;
    int baseline_ready = 0;
    int bootstrap_sequence_completed = 0;
    int signon_catalog_ready = 0;
    int bootstrap_records_staged = 0;
    int signon_template_ready = 0;
    int template_records_delivered = 0;
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonTemplateProbeSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string probe = "disabled";
    int attempts = 0;
    int accepted = 0;
    int rejected = 0;
    std::string last_reject_reason = "<none>";
    std::string parsed_session;
    int parsed_template_count = 0;
    int parsed_final_template = -1;
    std::string parsed_template_byte_lengths;
    std::string parsed_map;
    std::string parsed_name;
    std::string parsed_ruleset;
    int parsed_spawned = 0;
    int signon_ready = 0;
    int bootstrap_delivered = 0;
    int baseline_ready = 0;
    int bootstrap_sequence_completed = 0;
    int signon_catalog_ready = 0;
    int bootstrap_records_staged = 0;
    int signon_template_ready = 0;
    int template_records_delivered = 0;
    std::string protocol_shape = "disabled";
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonTemplateCompletionSurfaceSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string bind = "disabled";
    int requested_port = 0;
    int bound_port = 0;
    bool shared_with_query = false;
    bool shared_with_connect = false;
    bool shared_with_activation = false;
    bool shared_with_bootstrap = false;
    bool shared_with_bootstrap_sequence = false;
    bool shared_with_signon_catalog = false;
    bool shared_with_signon_template = false;
    std::string protocol_shape = "disabled";
    bool completion_enabled = false;
    bool requires_initially_templated_session = true;
    std::string completion_payload = "disabled";
    int completion_records = 0;
    std::string auth = "disabled";
    std::string signon = "disabled";
    std::string gameplay_transport = "no";
    int accepted = 0;
    int rejected = 0;
    int signon_ready = 0;
    int bootstrap_delivered = 0;
    int baseline_ready = 0;
    int bootstrap_sequence_completed = 0;
    int signon_catalog_ready = 0;
    int bootstrap_records_staged = 0;
    int signon_template_ready = 0;
    int template_records_delivered = 0;
    int signon_template_coverage_complete = 0;
    int remaining_template_records_delivered = 0;
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonTemplateCompletionProbeSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string probe = "disabled";
    int attempts = 0;
    int accepted = 0;
    int rejected = 0;
    std::string last_reject_reason = "<none>";
    std::string parsed_session;
    int parsed_completion_count = 0;
    int parsed_final_template = -1;
    std::string parsed_template_ids;
    std::string parsed_template_byte_lengths;
    std::string parsed_map;
    std::string parsed_name;
    std::string parsed_ruleset;
    int parsed_spawned = 0;
    int signon_ready = 0;
    int bootstrap_delivered = 0;
    int baseline_ready = 0;
    int bootstrap_sequence_completed = 0;
    int signon_catalog_ready = 0;
    int bootstrap_records_staged = 0;
    int signon_template_ready = 0;
    int template_records_delivered = 0;
    int signon_template_coverage_complete = 0;
    int remaining_template_records_delivered = 0;
    std::string protocol_shape = "disabled";
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonEnvelopeSurfaceSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string bind = "disabled";
    int requested_port = 0;
    int bound_port = 0;
    bool shared_with_query = false;
    bool shared_with_connect = false;
    bool shared_with_activation = false;
    bool shared_with_bootstrap = false;
    bool shared_with_bootstrap_sequence = false;
    bool shared_with_signon_catalog = false;
    bool shared_with_signon_template = false;
    bool shared_with_signon_template_completion = false;
    std::string protocol_shape = "disabled";
    bool envelope_enabled = false;
    bool requires_template_coverage_complete = true;
    std::string envelope_payload = "disabled";
    int envelope_frames = 0;
    std::string auth = "disabled";
    std::string signon = "disabled";
    std::string gameplay_transport = "no";
    int accepted = 0;
    int rejected = 0;
    int signon_ready = 0;
    int bootstrap_delivered = 0;
    int baseline_ready = 0;
    int bootstrap_sequence_completed = 0;
    int signon_catalog_ready = 0;
    int bootstrap_records_staged = 0;
    int signon_template_ready = 0;
    int template_records_delivered = 0;
    int signon_template_coverage_complete = 0;
    int remaining_template_records_delivered = 0;
    int signon_envelope_ready = 0;
    int framed_template_records_delivered = 0;
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonEnvelopeProbeSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string probe = "disabled";
    int attempts = 0;
    int accepted = 0;
    int rejected = 0;
    std::string last_reject_reason = "<none>";
    std::string parsed_session;
    int parsed_frame_count = 0;
    int parsed_final_frame = -1;
    std::string parsed_frame_ids;
    std::string parsed_frame_byte_lengths;
    std::string parsed_map;
    std::string parsed_name;
    std::string parsed_ruleset;
    int parsed_spawned = 0;
    int signon_ready = 0;
    int bootstrap_delivered = 0;
    int baseline_ready = 0;
    int bootstrap_sequence_completed = 0;
    int signon_catalog_ready = 0;
    int bootstrap_records_staged = 0;
    int signon_template_ready = 0;
    int template_records_delivered = 0;
    int signon_template_coverage_complete = 0;
    int remaining_template_records_delivered = 0;
    int signon_envelope_ready = 0;
    int framed_template_records_delivered = 0;
    std::string protocol_shape = "disabled";
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonBatchSurfaceSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string bind = "disabled";
    int requested_port = 0;
    int bound_port = 0;
    bool shared_with_query = false;
    bool shared_with_connect = false;
    bool shared_with_activation = false;
    bool shared_with_bootstrap = false;
    bool shared_with_bootstrap_sequence = false;
    bool shared_with_signon_catalog = false;
    bool shared_with_signon_template = false;
    bool shared_with_signon_template_completion = false;
    bool shared_with_signon_envelope = false;
    std::string protocol_shape = "disabled";
    bool batch_enabled = false;
    bool requires_envelope_ready_session = true;
    std::string batch_payload = "disabled";
    int batch_packets = 0;
    std::string auth = "disabled";
    std::string signon = "disabled";
    std::string gameplay_transport = "no";
    int accepted = 0;
    int rejected = 0;
    int signon_ready = 0;
    int bootstrap_delivered = 0;
    int baseline_ready = 0;
    int bootstrap_sequence_completed = 0;
    int signon_catalog_ready = 0;
    int bootstrap_records_staged = 0;
    int signon_template_ready = 0;
    int template_records_delivered = 0;
    int signon_template_coverage_complete = 0;
    int remaining_template_records_delivered = 0;
    int signon_envelope_ready = 0;
    int framed_template_records_delivered = 0;
    int signon_batch_ready = 0;
    int pseudo_packet_batch_delivered = 0;
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonBatchProbeSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string probe = "disabled";
    int attempts = 0;
    int accepted = 0;
    int rejected = 0;
    std::string last_reject_reason = "<none>";
    std::string parsed_session;
    int parsed_packet_count = 0;
    int parsed_final_packet = -1;
    std::string parsed_packet_ids;
    std::string parsed_packet_byte_lengths;
    std::string parsed_frame_coverage;
    std::string parsed_map;
    std::string parsed_name;
    std::string parsed_ruleset;
    int parsed_spawned = 0;
    int signon_ready = 0;
    int bootstrap_delivered = 0;
    int baseline_ready = 0;
    int bootstrap_sequence_completed = 0;
    int signon_catalog_ready = 0;
    int bootstrap_records_staged = 0;
    int signon_template_ready = 0;
    int template_records_delivered = 0;
    int signon_template_coverage_complete = 0;
    int remaining_template_records_delivered = 0;
    int signon_envelope_ready = 0;
    int framed_template_records_delivered = 0;
    int signon_batch_ready = 0;
    int pseudo_packet_batch_delivered = 0;
    std::string protocol_shape = "disabled";
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonWiremapSurfaceSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string bind = "disabled";
    int requested_port = 0;
    int bound_port = 0;
    bool shared_with_query = false;
    bool shared_with_connect = false;
    bool shared_with_activation = false;
    bool shared_with_bootstrap = false;
    bool shared_with_bootstrap_sequence = false;
    bool shared_with_signon_catalog = false;
    bool shared_with_signon_template = false;
    bool shared_with_signon_template_completion = false;
    bool shared_with_signon_envelope = false;
    bool shared_with_signon_batch = false;
    std::string protocol_shape = "disabled";
    bool wiremap_enabled = false;
    bool requires_batch_ready_session = true;
    std::string wiremap_payload = "disabled";
    int wiremap_packets = 0;
    std::string auth = "disabled";
    std::string signon = "disabled";
    std::string gameplay_transport = "no";
    int accepted = 0;
    int rejected = 0;
    int signon_ready = 0;
    int bootstrap_delivered = 0;
    int baseline_ready = 0;
    int bootstrap_sequence_completed = 0;
    int signon_catalog_ready = 0;
    int bootstrap_records_staged = 0;
    int signon_template_ready = 0;
    int template_records_delivered = 0;
    int signon_template_coverage_complete = 0;
    int remaining_template_records_delivered = 0;
    int signon_envelope_ready = 0;
    int framed_template_records_delivered = 0;
    int signon_batch_ready = 0;
    int pseudo_packet_batch_delivered = 0;
    int signon_wiremap_ready = 0;
    int wiremapped_batch_delivered = 0;
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonWiremapProbeSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string probe = "disabled";
    int attempts = 0;
    int accepted = 0;
    int rejected = 0;
    std::string last_reject_reason = "<none>";
    std::string parsed_session;
    int parsed_wiremap_count = 0;
    int parsed_final_packet = -1;
    std::string parsed_sequence_ids;
    std::string parsed_flags;
    std::string parsed_packet_byte_lengths;
    std::string parsed_map;
    std::string parsed_name;
    std::string parsed_ruleset;
    int parsed_spawned = 0;
    int signon_ready = 0;
    int bootstrap_delivered = 0;
    int baseline_ready = 0;
    int bootstrap_sequence_completed = 0;
    int signon_catalog_ready = 0;
    int bootstrap_records_staged = 0;
    int signon_template_ready = 0;
    int template_records_delivered = 0;
    int signon_template_coverage_complete = 0;
    int remaining_template_records_delivered = 0;
    int signon_envelope_ready = 0;
    int framed_template_records_delivered = 0;
    int signon_batch_ready = 0;
    int pseudo_packet_batch_delivered = 0;
    int signon_wiremap_ready = 0;
    int wiremapped_batch_delivered = 0;
    std::string protocol_shape = "disabled";
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonBurstSurfaceSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string bind = "disabled";
    int requested_port = 0;
    int bound_port = 0;
    bool shared_with_query = false;
    bool shared_with_connect = false;
    bool shared_with_activation = false;
    bool shared_with_bootstrap = false;
    bool shared_with_bootstrap_sequence = false;
    bool shared_with_signon_catalog = false;
    bool shared_with_signon_template = false;
    bool shared_with_signon_template_completion = false;
    bool shared_with_signon_envelope = false;
    bool shared_with_signon_batch = false;
    bool shared_with_signon_wiremap = false;
    std::string protocol_shape = "disabled";
    bool burst_enabled = false;
    bool requires_wiremap_ready_session = true;
    std::string burst_payload = "disabled";
    int burst_count = 0;
    std::string auth = "disabled";
    std::string signon = "disabled";
    std::string gameplay_transport = "no";
    int accepted = 0;
    int rejected = 0;
    int signon_ready = 0;
    int bootstrap_delivered = 0;
    int baseline_ready = 0;
    int bootstrap_sequence_completed = 0;
    int signon_catalog_ready = 0;
    int bootstrap_records_staged = 0;
    int signon_template_ready = 0;
    int template_records_delivered = 0;
    int signon_template_coverage_complete = 0;
    int remaining_template_records_delivered = 0;
    int signon_envelope_ready = 0;
    int framed_template_records_delivered = 0;
    int signon_batch_ready = 0;
    int pseudo_packet_batch_delivered = 0;
    int signon_wiremap_ready = 0;
    int wiremapped_batch_delivered = 0;
    int signon_burst_ready = 0;
    int pseudo_wire_burst_delivered = 0;
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonBurstProbeSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string probe = "disabled";
    int attempts = 0;
    int accepted = 0;
    int rejected = 0;
    std::string last_reject_reason = "<none>";
    std::string parsed_session;
    int parsed_burst_count = 0;
    int parsed_final_burst = -1;
    std::string parsed_packet_coverage;
    std::string parsed_burst_byte_lengths;
    std::string parsed_map;
    std::string parsed_name;
    std::string parsed_ruleset;
    int parsed_spawned = 0;
    int signon_ready = 0;
    int bootstrap_delivered = 0;
    int baseline_ready = 0;
    int bootstrap_sequence_completed = 0;
    int signon_catalog_ready = 0;
    int bootstrap_records_staged = 0;
    int signon_template_ready = 0;
    int template_records_delivered = 0;
    int signon_template_coverage_complete = 0;
    int remaining_template_records_delivered = 0;
    int signon_envelope_ready = 0;
    int framed_template_records_delivered = 0;
    int signon_batch_ready = 0;
    int pseudo_packet_batch_delivered = 0;
    int signon_wiremap_ready = 0;
    int wiremapped_batch_delivered = 0;
    int signon_burst_ready = 0;
    int pseudo_wire_burst_delivered = 0;
    std::string protocol_shape = "disabled";
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonStreamSurfaceSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string bind = "disabled";
    int requested_port = 0;
    int bound_port = 0;
    bool shared_with_query = false;
    bool shared_with_connect = false;
    bool shared_with_activation = false;
    bool shared_with_bootstrap = false;
    bool shared_with_bootstrap_sequence = false;
    bool shared_with_signon_catalog = false;
    bool shared_with_signon_template = false;
    bool shared_with_signon_template_completion = false;
    bool shared_with_signon_envelope = false;
    bool shared_with_signon_batch = false;
    bool shared_with_signon_wiremap = false;
    bool shared_with_signon_burst = false;
    std::string protocol_shape = "disabled";
    bool stream_enabled = false;
    bool requires_burst_ready_session = true;
    std::string stream_payload = "disabled";
    int stream_segments = 0;
    std::string auth = "disabled";
    std::string signon = "disabled";
    std::string gameplay_transport = "no";
    int accepted = 0;
    int rejected = 0;
    int signon_ready = 0;
    int bootstrap_delivered = 0;
    int baseline_ready = 0;
    int bootstrap_sequence_completed = 0;
    int signon_catalog_ready = 0;
    int bootstrap_records_staged = 0;
    int signon_template_ready = 0;
    int template_records_delivered = 0;
    int signon_template_coverage_complete = 0;
    int remaining_template_records_delivered = 0;
    int signon_envelope_ready = 0;
    int framed_template_records_delivered = 0;
    int signon_batch_ready = 0;
    int pseudo_packet_batch_delivered = 0;
    int signon_wiremap_ready = 0;
    int wiremapped_batch_delivered = 0;
    int signon_burst_ready = 0;
    int pseudo_wire_burst_delivered = 0;
    int signon_stream_ready = 0;
    int contiguous_stream_delivered = 0;
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonStreamProbeSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string probe = "disabled";
    int attempts = 0;
    int accepted = 0;
    int rejected = 0;
    std::string last_reject_reason = "<none>";
    std::string parsed_session;
    int parsed_segment_count = 0;
    int parsed_final_segment = -1;
    std::string parsed_segment_byte_lengths;
    std::string parsed_stream_offset_range;
    int parsed_final_stream_byte_length = 0;
    std::string parsed_map;
    std::string parsed_name;
    std::string parsed_ruleset;
    int parsed_spawned = 0;
    int signon_ready = 0;
    int bootstrap_delivered = 0;
    int baseline_ready = 0;
    int bootstrap_sequence_completed = 0;
    int signon_catalog_ready = 0;
    int bootstrap_records_staged = 0;
    int signon_template_ready = 0;
    int template_records_delivered = 0;
    int signon_template_coverage_complete = 0;
    int remaining_template_records_delivered = 0;
    int signon_envelope_ready = 0;
    int framed_template_records_delivered = 0;
    int signon_batch_ready = 0;
    int pseudo_packet_batch_delivered = 0;
    int signon_wiremap_ready = 0;
    int wiremapped_batch_delivered = 0;
    int signon_burst_ready = 0;
    int pseudo_wire_burst_delivered = 0;
    int signon_stream_ready = 0;
    int contiguous_stream_delivered = 0;
    std::string protocol_shape = "disabled";
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonStreamWindowSurfaceSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string bind = "disabled";
    int requested_port = 0;
    int bound_port = 0;
    bool shared_with_query = false;
    bool shared_with_connect = false;
    bool shared_with_activation = false;
    bool shared_with_bootstrap = false;
    bool shared_with_bootstrap_sequence = false;
    bool shared_with_signon_catalog = false;
    bool shared_with_signon_template = false;
    bool shared_with_signon_template_completion = false;
    bool shared_with_signon_envelope = false;
    bool shared_with_signon_batch = false;
    bool shared_with_signon_wiremap = false;
    bool shared_with_signon_burst = false;
    bool shared_with_signon_stream = false;
    std::string protocol_shape = "disabled";
    bool windowing_enabled = false;
    bool requires_stream_ready_session = true;
    std::string window_payload = "disabled";
    int window_count = 0;
    std::string replay_policy = "disabled";
    int replays_accepted = 0;
    std::string auth = "disabled";
    std::string signon = "disabled";
    std::string gameplay_transport = "no";
    int accepted = 0;
    int rejected = 0;
    int signon_ready = 0;
    int bootstrap_delivered = 0;
    int baseline_ready = 0;
    int bootstrap_sequence_completed = 0;
    int signon_catalog_ready = 0;
    int bootstrap_records_staged = 0;
    int signon_template_ready = 0;
    int template_records_delivered = 0;
    int signon_template_coverage_complete = 0;
    int remaining_template_records_delivered = 0;
    int signon_envelope_ready = 0;
    int framed_template_records_delivered = 0;
    int signon_batch_ready = 0;
    int pseudo_packet_batch_delivered = 0;
    int signon_wiremap_ready = 0;
    int wiremapped_batch_delivered = 0;
    int signon_burst_ready = 0;
    int pseudo_wire_burst_delivered = 0;
    int signon_stream_ready = 0;
    int contiguous_stream_delivered = 0;
    int signon_stream_window_ready = 0;
    int windowed_stream_delivered = 0;
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonStreamWindowProbeSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string probe = "disabled";
    int attempts = 0;
    int accepted = 0;
    int rejected = 0;
    int replays_accepted = 0;
    std::string last_reject_reason = "<none>";
    std::string parsed_session;
    int parsed_window_count = 0;
    int parsed_final_window = -1;
    std::string parsed_window_byte_lengths;
    std::string parsed_offset_ranges;
    int parsed_final_stream_byte_length = 0;
    int parsed_replay_window = -1;
    std::string parsed_map;
    std::string parsed_name;
    std::string parsed_ruleset;
    int parsed_spawned = 0;
    int signon_ready = 0;
    int bootstrap_delivered = 0;
    int baseline_ready = 0;
    int bootstrap_sequence_completed = 0;
    int signon_catalog_ready = 0;
    int bootstrap_records_staged = 0;
    int signon_template_ready = 0;
    int template_records_delivered = 0;
    int signon_template_coverage_complete = 0;
    int remaining_template_records_delivered = 0;
    int signon_envelope_ready = 0;
    int framed_template_records_delivered = 0;
    int signon_batch_ready = 0;
    int pseudo_packet_batch_delivered = 0;
    int signon_wiremap_ready = 0;
    int wiremapped_batch_delivered = 0;
    int signon_burst_ready = 0;
    int pseudo_wire_burst_delivered = 0;
    int signon_stream_ready = 0;
    int contiguous_stream_delivered = 0;
    int signon_stream_window_ready = 0;
    int windowed_stream_delivered = 0;
    std::string protocol_shape = "disabled";
    std::string compatibility = "disabled";
    std::string detail;
    unsigned int parsed_window_mask = 0u;
};

struct DedicatedSignonMessageCatalogSurfaceSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string bind = "disabled";
    int requested_port = 0;
    int bound_port = 0;
    bool shared_with_query = false;
    bool shared_with_connect = false;
    bool shared_with_activation = false;
    bool shared_with_bootstrap = false;
    bool shared_with_bootstrap_sequence = false;
    bool shared_with_signon_catalog = false;
    bool shared_with_signon_template = false;
    bool shared_with_signon_template_completion = false;
    bool shared_with_signon_envelope = false;
    bool shared_with_signon_batch = false;
    bool shared_with_signon_wiremap = false;
    bool shared_with_signon_burst = false;
    bool shared_with_signon_stream = false;
    bool shared_with_signon_stream_window = false;
    std::string protocol_shape = "disabled";
    bool message_catalog_enabled = false;
    bool requires_stream_window_ready_session = true;
    std::string message_boundary_payload = "disabled";
    int message_count = 0;
    std::string auth = "disabled";
    std::string signon = "disabled";
    std::string gameplay_transport = "no";
    int accepted = 0;
    int rejected = 0;
    int signon_ready = 0;
    int bootstrap_delivered = 0;
    int baseline_ready = 0;
    int bootstrap_sequence_completed = 0;
    int signon_catalog_ready = 0;
    int bootstrap_records_staged = 0;
    int signon_template_ready = 0;
    int template_records_delivered = 0;
    int signon_template_coverage_complete = 0;
    int remaining_template_records_delivered = 0;
    int signon_envelope_ready = 0;
    int framed_template_records_delivered = 0;
    int signon_batch_ready = 0;
    int pseudo_packet_batch_delivered = 0;
    int signon_wiremap_ready = 0;
    int wiremapped_batch_delivered = 0;
    int signon_burst_ready = 0;
    int pseudo_wire_burst_delivered = 0;
    int signon_stream_ready = 0;
    int contiguous_stream_delivered = 0;
    int signon_stream_window_ready = 0;
    int windowed_stream_delivered = 0;
    int signon_message_catalog_ready = 0;
    int message_boundaries_cataloged = 0;
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCatalogProbeSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string probe = "disabled";
    int attempts = 0;
    int accepted = 0;
    int rejected = 0;
    std::string last_reject_reason = "<none>";
    std::string parsed_session;
    int parsed_message_count = 0;
    int parsed_final_message = -1;
    std::string parsed_message_offsets;
    std::string parsed_message_byte_lengths;
    std::string parsed_semantic_tags;
    std::string parsed_map;
    std::string parsed_name;
    std::string parsed_ruleset;
    int parsed_spawned = 0;
    int signon_ready = 0;
    int bootstrap_delivered = 0;
    int baseline_ready = 0;
    int bootstrap_sequence_completed = 0;
    int signon_catalog_ready = 0;
    int bootstrap_records_staged = 0;
    int signon_template_ready = 0;
    int template_records_delivered = 0;
    int signon_template_coverage_complete = 0;
    int remaining_template_records_delivered = 0;
    int signon_envelope_ready = 0;
    int framed_template_records_delivered = 0;
    int signon_batch_ready = 0;
    int pseudo_packet_batch_delivered = 0;
    int signon_wiremap_ready = 0;
    int wiremapped_batch_delivered = 0;
    int signon_burst_ready = 0;
    int pseudo_wire_burst_delivered = 0;
    int signon_stream_ready = 0;
    int contiguous_stream_delivered = 0;
    int signon_stream_window_ready = 0;
    int windowed_stream_delivered = 0;
    int signon_message_catalog_ready = 0;
    int message_boundaries_cataloged = 0;
    std::string protocol_shape = "disabled";
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageFetchSurfaceSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string bind = "disabled";
    int requested_port = 0;
    int bound_port = 0;
    bool shared_with_query = false;
    bool shared_with_connect = false;
    bool shared_with_activation = false;
    bool shared_with_bootstrap = false;
    bool shared_with_bootstrap_sequence = false;
    bool shared_with_signon_catalog = false;
    bool shared_with_signon_template = false;
    bool shared_with_signon_template_completion = false;
    bool shared_with_signon_envelope = false;
    bool shared_with_signon_batch = false;
    bool shared_with_signon_wiremap = false;
    bool shared_with_signon_burst = false;
    bool shared_with_signon_stream = false;
    bool shared_with_signon_stream_window = false;
    bool shared_with_signon_message_catalog = false;
    std::string protocol_shape = "disabled";
    bool message_fetch_enabled = false;
    bool requires_message_catalog_ready_session = true;
    std::string fetch_selector = "disabled";
    std::string fetch_payload = "disabled";
    int fetch_count = 0;
    std::string auth = "disabled";
    std::string signon = "disabled";
    std::string gameplay_transport = "no";
    int accepted = 0;
    int rejected = 0;
    int last_fetched_message_index = -1;
    int signon_ready = 0;
    int bootstrap_delivered = 0;
    int baseline_ready = 0;
    int bootstrap_sequence_completed = 0;
    int signon_catalog_ready = 0;
    int bootstrap_records_staged = 0;
    int signon_template_ready = 0;
    int template_records_delivered = 0;
    int signon_template_coverage_complete = 0;
    int remaining_template_records_delivered = 0;
    int signon_envelope_ready = 0;
    int framed_template_records_delivered = 0;
    int signon_batch_ready = 0;
    int pseudo_packet_batch_delivered = 0;
    int signon_wiremap_ready = 0;
    int wiremapped_batch_delivered = 0;
    int signon_burst_ready = 0;
    int pseudo_wire_burst_delivered = 0;
    int signon_stream_ready = 0;
    int contiguous_stream_delivered = 0;
    int signon_stream_window_ready = 0;
    int windowed_stream_delivered = 0;
    int signon_message_catalog_ready = 0;
    int message_boundaries_cataloged = 0;
    int signon_message_fetch_ready = 0;
    int targeted_message_fetch_delivered = 0;
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageFetchProbeSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string probe = "disabled";
    int attempts = 0;
    int accepted = 0;
    int rejected = 0;
    std::string last_reject_reason = "<none>";
    std::string parsed_session;
    int requested_message_index = -1;
    int parsed_fetched_message_index = -1;
    std::string parsed_semantic_tag;
    std::string parsed_offsets;
    int parsed_byte_length = 0;
    std::string parsed_map;
    std::string parsed_name;
    std::string parsed_ruleset;
    int parsed_spawned = 0;
    int signon_ready = 0;
    int bootstrap_delivered = 0;
    int baseline_ready = 0;
    int bootstrap_sequence_completed = 0;
    int signon_catalog_ready = 0;
    int bootstrap_records_staged = 0;
    int signon_template_ready = 0;
    int template_records_delivered = 0;
    int signon_template_coverage_complete = 0;
    int remaining_template_records_delivered = 0;
    int signon_envelope_ready = 0;
    int framed_template_records_delivered = 0;
    int signon_batch_ready = 0;
    int pseudo_packet_batch_delivered = 0;
    int signon_wiremap_ready = 0;
    int wiremapped_batch_delivered = 0;
    int signon_burst_ready = 0;
    int pseudo_wire_burst_delivered = 0;
    int signon_stream_ready = 0;
    int contiguous_stream_delivered = 0;
    int signon_stream_window_ready = 0;
    int windowed_stream_delivered = 0;
    int signon_message_catalog_ready = 0;
    int message_boundaries_cataloged = 0;
    int signon_message_fetch_ready = 0;
    int targeted_message_fetch_delivered = 0;
    std::string protocol_shape = "disabled";
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMultiMessageFetchSurfaceSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string bind = "disabled";
    int requested_port = 0;
    int bound_port = 0;
    bool shared_with_query = false;
    bool shared_with_connect = false;
    bool shared_with_activation = false;
    bool shared_with_bootstrap = false;
    bool shared_with_bootstrap_sequence = false;
    bool shared_with_signon_catalog = false;
    bool shared_with_signon_template = false;
    bool shared_with_signon_template_completion = false;
    bool shared_with_signon_envelope = false;
    bool shared_with_signon_batch = false;
    bool shared_with_signon_wiremap = false;
    bool shared_with_signon_burst = false;
    bool shared_with_signon_stream = false;
    bool shared_with_signon_stream_window = false;
    bool shared_with_signon_message_catalog = false;
    bool shared_with_signon_message_fetch = false;
    std::string protocol_shape = "disabled";
    bool multi_message_fetch_enabled = false;
    bool requires_message_fetch_ready_session = true;
    std::string selector_shape = "disabled";
    std::string fetch_payload = "disabled";
    int requested_message_count = 0;
    std::string auth = "disabled";
    std::string signon = "disabled";
    std::string gameplay_transport = "no";
    int accepted = 0;
    int rejected = 0;
    std::string last_fetched_message_indices;
    int signon_ready = 0;
    int bootstrap_delivered = 0;
    int baseline_ready = 0;
    int bootstrap_sequence_completed = 0;
    int signon_catalog_ready = 0;
    int bootstrap_records_staged = 0;
    int signon_template_ready = 0;
    int template_records_delivered = 0;
    int signon_template_coverage_complete = 0;
    int remaining_template_records_delivered = 0;
    int signon_envelope_ready = 0;
    int framed_template_records_delivered = 0;
    int signon_batch_ready = 0;
    int pseudo_packet_batch_delivered = 0;
    int signon_wiremap_ready = 0;
    int wiremapped_batch_delivered = 0;
    int signon_burst_ready = 0;
    int pseudo_wire_burst_delivered = 0;
    int signon_stream_ready = 0;
    int contiguous_stream_delivered = 0;
    int signon_stream_window_ready = 0;
    int windowed_stream_delivered = 0;
    int signon_message_catalog_ready = 0;
    int message_boundaries_cataloged = 0;
    int signon_message_fetch_ready = 0;
    int targeted_message_fetch_delivered = 0;
    int signon_multi_message_fetch_ready = 0;
    int selective_message_set_delivered = 0;
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMultiMessageFetchProbeSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string probe = "disabled";
    int attempts = 0;
    int accepted = 0;
    int rejected = 0;
    std::string last_reject_reason = "<none>";
    std::string requested_message_indices;
    int parsed_fetched_message_count = 0;
    std::string parsed_fetched_message_indices;
    std::string parsed_semantic_tags;
    std::string parsed_byte_lengths;
    int parsed_combined_byte_length = 0;
    std::string parsed_map;
    std::string parsed_name;
    std::string parsed_ruleset;
    int parsed_spawned = 0;
    int signon_ready = 0;
    int bootstrap_delivered = 0;
    int baseline_ready = 0;
    int bootstrap_sequence_completed = 0;
    int signon_catalog_ready = 0;
    int bootstrap_records_staged = 0;
    int signon_template_ready = 0;
    int template_records_delivered = 0;
    int signon_template_coverage_complete = 0;
    int remaining_template_records_delivered = 0;
    int signon_envelope_ready = 0;
    int framed_template_records_delivered = 0;
    int signon_batch_ready = 0;
    int pseudo_packet_batch_delivered = 0;
    int signon_wiremap_ready = 0;
    int wiremapped_batch_delivered = 0;
    int signon_burst_ready = 0;
    int pseudo_wire_burst_delivered = 0;
    int signon_stream_ready = 0;
    int contiguous_stream_delivered = 0;
    int signon_stream_window_ready = 0;
    int windowed_stream_delivered = 0;
    int signon_message_catalog_ready = 0;
    int message_boundaries_cataloged = 0;
    int signon_message_fetch_ready = 0;
    int targeted_message_fetch_delivered = 0;
    int signon_multi_message_fetch_ready = 0;
    int selective_message_set_delivered = 0;
    std::string protocol_shape = "disabled";
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageRangeFetchSurfaceSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string bind = "disabled";
    int requested_port = 0;
    int bound_port = 0;
    bool shared_with_query = false;
    bool shared_with_connect = false;
    bool shared_with_activation = false;
    bool shared_with_bootstrap = false;
    bool shared_with_bootstrap_sequence = false;
    bool shared_with_signon_catalog = false;
    bool shared_with_signon_template = false;
    bool shared_with_signon_template_completion = false;
    bool shared_with_signon_envelope = false;
    bool shared_with_signon_batch = false;
    bool shared_with_signon_wiremap = false;
    bool shared_with_signon_burst = false;
    bool shared_with_signon_stream = false;
    bool shared_with_signon_stream_window = false;
    bool shared_with_signon_message_catalog = false;
    bool shared_with_signon_message_fetch = false;
    bool shared_with_signon_multi_message_fetch = false;
    std::string protocol_shape = "disabled";
    bool message_range_fetch_enabled = false;
    bool requires_multi_message_fetch_ready_session = true;
    std::string selector_shape = "disabled";
    std::string fetch_payload = "disabled";
    std::string requested_range;
    int accepted = 0;
    int rejected = 0;
    std::string last_fetched_range;
    std::string auth = "disabled";
    std::string signon = "disabled";
    std::string gameplay_transport = "no";
    int signon_ready = 0;
    int bootstrap_delivered = 0;
    int baseline_ready = 0;
    int bootstrap_sequence_completed = 0;
    int signon_catalog_ready = 0;
    int bootstrap_records_staged = 0;
    int signon_template_ready = 0;
    int template_records_delivered = 0;
    int signon_template_coverage_complete = 0;
    int remaining_template_records_delivered = 0;
    int signon_envelope_ready = 0;
    int framed_template_records_delivered = 0;
    int signon_batch_ready = 0;
    int pseudo_packet_batch_delivered = 0;
    int signon_wiremap_ready = 0;
    int wiremapped_batch_delivered = 0;
    int signon_burst_ready = 0;
    int pseudo_wire_burst_delivered = 0;
    int signon_stream_ready = 0;
    int contiguous_stream_delivered = 0;
    int signon_stream_window_ready = 0;
    int windowed_stream_delivered = 0;
    int signon_message_catalog_ready = 0;
    int message_boundaries_cataloged = 0;
    int signon_message_fetch_ready = 0;
    int targeted_message_fetch_delivered = 0;
    int signon_multi_message_fetch_ready = 0;
    int selective_message_set_delivered = 0;
    int signon_message_range_fetch_ready = 0;
    int range_message_set_delivered = 0;
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageRangeFetchProbeSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string probe = "disabled";
    int attempts = 0;
    int accepted = 0;
    int rejected = 0;
    std::string last_reject_reason = "<none>";
    std::string requested_range;
    int parsed_fetched_message_count = 0;
    std::string parsed_fetched_message_indices;
    std::string parsed_semantic_tags;
    std::string parsed_byte_lengths;
    int parsed_combined_byte_length = 0;
    std::string parsed_map;
    std::string parsed_name;
    std::string parsed_ruleset;
    int parsed_spawned = 0;
    int signon_ready = 0;
    int bootstrap_delivered = 0;
    int baseline_ready = 0;
    int bootstrap_sequence_completed = 0;
    int signon_catalog_ready = 0;
    int bootstrap_records_staged = 0;
    int signon_template_ready = 0;
    int template_records_delivered = 0;
    int signon_template_coverage_complete = 0;
    int remaining_template_records_delivered = 0;
    int signon_envelope_ready = 0;
    int framed_template_records_delivered = 0;
    int signon_batch_ready = 0;
    int pseudo_packet_batch_delivered = 0;
    int signon_wiremap_ready = 0;
    int wiremapped_batch_delivered = 0;
    int signon_burst_ready = 0;
    int pseudo_wire_burst_delivered = 0;
    int signon_stream_ready = 0;
    int contiguous_stream_delivered = 0;
    int signon_stream_window_ready = 0;
    int windowed_stream_delivered = 0;
    int signon_message_catalog_ready = 0;
    int message_boundaries_cataloged = 0;
    int signon_message_fetch_ready = 0;
    int targeted_message_fetch_delivered = 0;
    int signon_multi_message_fetch_ready = 0;
    int selective_message_set_delivered = 0;
    int signon_message_range_fetch_ready = 0;
    int range_message_set_delivered = 0;
    std::string protocol_shape = "disabled";
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorSurfaceSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string bind = "disabled";
    int requested_port = 0;
    int bound_port = 0;
    bool shared_with_query = false;
    bool shared_with_connect = false;
    bool shared_with_activation = false;
    bool shared_with_bootstrap = false;
    bool shared_with_bootstrap_sequence = false;
    bool shared_with_signon_catalog = false;
    bool shared_with_signon_template = false;
    bool shared_with_signon_template_completion = false;
    bool shared_with_signon_envelope = false;
    bool shared_with_signon_batch = false;
    bool shared_with_signon_wiremap = false;
    bool shared_with_signon_burst = false;
    bool shared_with_signon_stream = false;
    bool shared_with_signon_stream_window = false;
    bool shared_with_signon_message_catalog = false;
    bool shared_with_signon_message_fetch = false;
    bool shared_with_signon_multi_message_fetch = false;
    bool shared_with_signon_message_range = false;
    std::string protocol_shape = "disabled";
    bool message_cursor_enabled = false;
    bool requires_range_fetch_ready_session = true;
    std::string cursor_source = "disabled";
    std::string cursor_payload = "disabled";
    int checkpoint_count = 0;
    int accepted = 0;
    int rejected = 0;
    std::string last_checkpointed_range;
    int next_start_message_index = -1;
    int remaining_message_count = 0;
    std::string auth = "disabled";
    std::string signon = "disabled";
    std::string gameplay_transport = "no";
    int signon_ready = 0;
    int bootstrap_delivered = 0;
    int baseline_ready = 0;
    int bootstrap_sequence_completed = 0;
    int signon_catalog_ready = 0;
    int bootstrap_records_staged = 0;
    int signon_template_ready = 0;
    int template_records_delivered = 0;
    int signon_template_coverage_complete = 0;
    int remaining_template_records_delivered = 0;
    int signon_envelope_ready = 0;
    int framed_template_records_delivered = 0;
    int signon_batch_ready = 0;
    int pseudo_packet_batch_delivered = 0;
    int signon_wiremap_ready = 0;
    int wiremapped_batch_delivered = 0;
    int signon_burst_ready = 0;
    int pseudo_wire_burst_delivered = 0;
    int signon_stream_ready = 0;
    int contiguous_stream_delivered = 0;
    int signon_stream_window_ready = 0;
    int windowed_stream_delivered = 0;
    int signon_message_catalog_ready = 0;
    int message_boundaries_cataloged = 0;
    int signon_message_fetch_ready = 0;
    int targeted_message_fetch_delivered = 0;
    int signon_multi_message_fetch_ready = 0;
    int selective_message_set_delivered = 0;
    int signon_message_range_fetch_ready = 0;
    int range_message_set_delivered = 0;
    int signon_message_cursor_ready = 0;
    int message_cursor_checkpointed = 0;
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorProbeSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string probe = "disabled";
    int attempts = 0;
    int accepted = 0;
    int rejected = 0;
    std::string last_reject_reason = "<none>";
    std::string requested_cursor_source;
    std::string parsed_cursor_id;
    std::string parsed_current_range;
    int parsed_next_start_message_index = -1;
    int parsed_remaining_message_count = 0;
    std::string parsed_resume_policy;
    std::string parsed_map;
    std::string parsed_name;
    std::string parsed_ruleset;
    int parsed_spawned = 0;
    int signon_ready = 0;
    int bootstrap_delivered = 0;
    int baseline_ready = 0;
    int bootstrap_sequence_completed = 0;
    int signon_catalog_ready = 0;
    int bootstrap_records_staged = 0;
    int signon_template_ready = 0;
    int template_records_delivered = 0;
    int signon_template_coverage_complete = 0;
    int remaining_template_records_delivered = 0;
    int signon_envelope_ready = 0;
    int framed_template_records_delivered = 0;
    int signon_batch_ready = 0;
    int pseudo_packet_batch_delivered = 0;
    int signon_wiremap_ready = 0;
    int wiremapped_batch_delivered = 0;
    int signon_burst_ready = 0;
    int pseudo_wire_burst_delivered = 0;
    int signon_stream_ready = 0;
    int contiguous_stream_delivered = 0;
    int signon_stream_window_ready = 0;
    int windowed_stream_delivered = 0;
    int signon_message_catalog_ready = 0;
    int message_boundaries_cataloged = 0;
    int signon_message_fetch_ready = 0;
    int targeted_message_fetch_delivered = 0;
    int signon_multi_message_fetch_ready = 0;
    int selective_message_set_delivered = 0;
    int signon_message_range_fetch_ready = 0;
    int range_message_set_delivered = 0;
    int signon_message_cursor_ready = 0;
    int message_cursor_checkpointed = 0;
    std::string protocol_shape = "disabled";
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorAdvanceSurfaceSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string bind = "disabled";
    int requested_port = 0;
    int bound_port = 0;
    bool shared_with_query = false;
    bool shared_with_connect = false;
    bool shared_with_activation = false;
    bool shared_with_bootstrap = false;
    bool shared_with_bootstrap_sequence = false;
    bool shared_with_signon_catalog = false;
    bool shared_with_signon_template = false;
    bool shared_with_signon_template_completion = false;
    bool shared_with_signon_envelope = false;
    bool shared_with_signon_batch = false;
    bool shared_with_signon_wiremap = false;
    bool shared_with_signon_burst = false;
    bool shared_with_signon_stream = false;
    bool shared_with_signon_stream_window = false;
    bool shared_with_signon_message_catalog = false;
    bool shared_with_signon_message_fetch = false;
    bool shared_with_signon_multi_message_fetch = false;
    bool shared_with_signon_message_range = false;
    bool shared_with_signon_message_cursor = false;
    std::string protocol_shape = "disabled";
    bool message_cursor_advance_enabled = false;
    bool requires_message_cursor_ready_session = true;
    std::string selector_shape = "disabled";
    std::string advance_payload = "disabled";
    std::string requested_advance;
    int accepted = 0;
    int rejected = 0;
    std::string last_advanced_range;
    int next_start_message_index_after_advance = -1;
    int remaining_message_count_after_advance = 0;
    std::string auth = "disabled";
    std::string signon = "disabled";
    std::string gameplay_transport = "no";
    int signon_ready = 0;
    int bootstrap_delivered = 0;
    int baseline_ready = 0;
    int bootstrap_sequence_completed = 0;
    int signon_catalog_ready = 0;
    int bootstrap_records_staged = 0;
    int signon_template_ready = 0;
    int template_records_delivered = 0;
    int signon_template_coverage_complete = 0;
    int remaining_template_records_delivered = 0;
    int signon_envelope_ready = 0;
    int framed_template_records_delivered = 0;
    int signon_batch_ready = 0;
    int pseudo_packet_batch_delivered = 0;
    int signon_wiremap_ready = 0;
    int wiremapped_batch_delivered = 0;
    int signon_burst_ready = 0;
    int pseudo_wire_burst_delivered = 0;
    int signon_stream_ready = 0;
    int contiguous_stream_delivered = 0;
    int signon_stream_window_ready = 0;
    int windowed_stream_delivered = 0;
    int signon_message_catalog_ready = 0;
    int message_boundaries_cataloged = 0;
    int signon_message_fetch_ready = 0;
    int targeted_message_fetch_delivered = 0;
    int signon_multi_message_fetch_ready = 0;
    int selective_message_set_delivered = 0;
    int signon_message_range_fetch_ready = 0;
    int range_message_set_delivered = 0;
    int signon_message_cursor_ready = 0;
    int message_cursor_checkpointed = 0;
    int signon_message_cursor_advance_ready = 0;
    int cursor_advanced_range_delivered = 0;
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorAdvanceProbeSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string probe = "disabled";
    int attempts = 0;
    int accepted = 0;
    int rejected = 0;
    std::string last_reject_reason = "<none>";
    std::string requested_cursor_id;
    int requested_message_count = 0;
    std::string parsed_advanced_range;
    std::string parsed_fetched_message_indices;
    std::string parsed_semantic_tags;
    int parsed_combined_byte_length = 0;
    int parsed_next_start_message_index = -1;
    int parsed_remaining_message_count = 0;
    std::string parsed_resume_policy;
    std::string parsed_map;
    std::string parsed_name;
    std::string parsed_ruleset;
    int parsed_spawned = 0;
    int signon_ready = 0;
    int bootstrap_delivered = 0;
    int baseline_ready = 0;
    int bootstrap_sequence_completed = 0;
    int signon_catalog_ready = 0;
    int bootstrap_records_staged = 0;
    int signon_template_ready = 0;
    int template_records_delivered = 0;
    int signon_template_coverage_complete = 0;
    int remaining_template_records_delivered = 0;
    int signon_envelope_ready = 0;
    int framed_template_records_delivered = 0;
    int signon_batch_ready = 0;
    int pseudo_packet_batch_delivered = 0;
    int signon_wiremap_ready = 0;
    int wiremapped_batch_delivered = 0;
    int signon_burst_ready = 0;
    int pseudo_wire_burst_delivered = 0;
    int signon_stream_ready = 0;
    int contiguous_stream_delivered = 0;
    int signon_stream_window_ready = 0;
    int windowed_stream_delivered = 0;
    int signon_message_catalog_ready = 0;
    int message_boundaries_cataloged = 0;
    int signon_message_fetch_ready = 0;
    int targeted_message_fetch_delivered = 0;
    int signon_multi_message_fetch_ready = 0;
    int selective_message_set_delivered = 0;
    int signon_message_range_fetch_ready = 0;
    int range_message_set_delivered = 0;
    int signon_message_cursor_ready = 0;
    int message_cursor_checkpointed = 0;
    int signon_message_cursor_advance_ready = 0;
    int cursor_advanced_range_delivered = 0;
    std::string protocol_shape = "disabled";
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorEofSurfaceSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string bind = "disabled";
    int requested_port = 0;
    int bound_port = 0;
    bool shared_with_query = false;
    bool shared_with_connect = false;
    bool shared_with_activation = false;
    bool shared_with_bootstrap = false;
    bool shared_with_bootstrap_sequence = false;
    bool shared_with_signon_catalog = false;
    bool shared_with_signon_template = false;
    bool shared_with_signon_template_completion = false;
    bool shared_with_signon_envelope = false;
    bool shared_with_signon_batch = false;
    bool shared_with_signon_wiremap = false;
    bool shared_with_signon_burst = false;
    bool shared_with_signon_stream = false;
    bool shared_with_signon_stream_window = false;
    bool shared_with_signon_message_catalog = false;
    bool shared_with_signon_message_fetch = false;
    bool shared_with_signon_multi_message_fetch = false;
    bool shared_with_signon_message_range = false;
    bool shared_with_signon_message_cursor = false;
    bool shared_with_signon_message_cursor_advance = false;
    std::string protocol_shape = "disabled";
    bool message_cursor_eof_enabled = false;
    bool requires_message_cursor_advance_ready_session = true;
    std::string cursor_source = "disabled";
    std::string eof_payload = "disabled";
    int eof_count = 0;
    int accepted = 0;
    int rejected = 0;
    std::string last_eof_cursor_id;
    bool eof = false;
    bool exhausted = false;
    int current_start_message_index = -1;
    int current_message_count = 0;
    std::string next_start_message_index = "<unset>";
    int remaining_message_count = 0;
    std::string auth = "disabled";
    std::string signon = "disabled";
    std::string gameplay_transport = "no";
    int signon_ready = 0;
    int bootstrap_delivered = 0;
    int baseline_ready = 0;
    int bootstrap_sequence_completed = 0;
    int signon_catalog_ready = 0;
    int bootstrap_records_staged = 0;
    int signon_template_ready = 0;
    int template_records_delivered = 0;
    int signon_template_coverage_complete = 0;
    int remaining_template_records_delivered = 0;
    int signon_envelope_ready = 0;
    int framed_template_records_delivered = 0;
    int signon_batch_ready = 0;
    int pseudo_packet_batch_delivered = 0;
    int signon_wiremap_ready = 0;
    int wiremapped_batch_delivered = 0;
    int signon_burst_ready = 0;
    int pseudo_wire_burst_delivered = 0;
    int signon_stream_ready = 0;
    int contiguous_stream_delivered = 0;
    int signon_stream_window_ready = 0;
    int windowed_stream_delivered = 0;
    int signon_message_catalog_ready = 0;
    int message_boundaries_cataloged = 0;
    int signon_message_fetch_ready = 0;
    int targeted_message_fetch_delivered = 0;
    int signon_multi_message_fetch_ready = 0;
    int selective_message_set_delivered = 0;
    int signon_message_range_fetch_ready = 0;
    int range_message_set_delivered = 0;
    int signon_message_cursor_ready = 0;
    int message_cursor_checkpointed = 0;
    int signon_message_cursor_advance_ready = 0;
    int cursor_advanced_range_delivered = 0;
    int signon_message_cursor_eof_ready = 0;
    int message_cursor_exhausted = 0;
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorEofProbeSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string probe = "disabled";
    int attempts = 0;
    int accepted = 0;
    int rejected = 0;
    std::string last_reject_reason = "<none>";
    std::string requested_cursor_id;
    std::string parsed_cursor_id;
    bool parsed_eof = false;
    bool parsed_exhausted = false;
    std::string parsed_current_range;
    std::string parsed_next_start_message_index = "<unset>";
    int parsed_remaining_message_count = 0;
    std::string parsed_resume_policy;
    std::string parsed_map;
    std::string parsed_name;
    std::string parsed_ruleset;
    int parsed_spawned = 0;
    int signon_ready = 0;
    int bootstrap_delivered = 0;
    int baseline_ready = 0;
    int bootstrap_sequence_completed = 0;
    int signon_catalog_ready = 0;
    int bootstrap_records_staged = 0;
    int signon_template_ready = 0;
    int template_records_delivered = 0;
    int signon_template_coverage_complete = 0;
    int remaining_template_records_delivered = 0;
    int signon_envelope_ready = 0;
    int framed_template_records_delivered = 0;
    int signon_batch_ready = 0;
    int pseudo_packet_batch_delivered = 0;
    int signon_wiremap_ready = 0;
    int wiremapped_batch_delivered = 0;
    int signon_burst_ready = 0;
    int pseudo_wire_burst_delivered = 0;
    int signon_stream_ready = 0;
    int contiguous_stream_delivered = 0;
    int signon_stream_window_ready = 0;
    int windowed_stream_delivered = 0;
    int signon_message_catalog_ready = 0;
    int message_boundaries_cataloged = 0;
    int signon_message_fetch_ready = 0;
    int targeted_message_fetch_delivered = 0;
    int signon_multi_message_fetch_ready = 0;
    int selective_message_set_delivered = 0;
    int signon_message_range_fetch_ready = 0;
    int range_message_set_delivered = 0;
    int signon_message_cursor_ready = 0;
    int message_cursor_checkpointed = 0;
    int signon_message_cursor_advance_ready = 0;
    int cursor_advanced_range_delivered = 0;
    int signon_message_cursor_eof_ready = 0;
    int message_cursor_exhausted = 0;
    std::string protocol_shape = "disabled";
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorResumeDenialSurfaceSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string bind = "disabled";
    int requested_port = 0;
    int bound_port = 0;
    bool shared_with_query = false;
    bool shared_with_connect = false;
    bool shared_with_activation = false;
    bool shared_with_bootstrap = false;
    bool shared_with_bootstrap_sequence = false;
    bool shared_with_signon_catalog = false;
    bool shared_with_signon_template = false;
    bool shared_with_signon_template_completion = false;
    bool shared_with_signon_envelope = false;
    bool shared_with_signon_batch = false;
    bool shared_with_signon_wiremap = false;
    bool shared_with_signon_burst = false;
    bool shared_with_signon_stream = false;
    bool shared_with_signon_stream_window = false;
    bool shared_with_signon_message_catalog = false;
    bool shared_with_signon_message_fetch = false;
    bool shared_with_signon_multi_message_fetch = false;
    bool shared_with_signon_message_range = false;
    bool shared_with_signon_message_cursor = false;
    bool shared_with_signon_message_cursor_advance = false;
    bool shared_with_signon_message_cursor_eof = false;
    std::string protocol_shape = "disabled";
    bool message_cursor_resume_denial_enabled = false;
    bool requires_message_cursor_eof_ready_session = true;
    std::string denial_source = "disabled";
    std::string denial_payload = "disabled";
    int denial_count = 0;
    int accepted = 0;
    int rejected = 0;
    std::string last_denied_cursor_id;
    bool resume_allowed = false;
    std::string denial_reason = "<unset>";
    bool eof = false;
    bool exhausted = false;
    std::string next_start_message_index = "<unset>";
    int remaining_message_count = 0;
    std::string resume_policy = "<unset>";
    std::string auth = "disabled";
    std::string signon = "disabled";
    std::string gameplay_transport = "no";
    int signon_ready = 0;
    int bootstrap_delivered = 0;
    int baseline_ready = 0;
    int bootstrap_sequence_completed = 0;
    int signon_catalog_ready = 0;
    int bootstrap_records_staged = 0;
    int signon_template_ready = 0;
    int template_records_delivered = 0;
    int signon_template_coverage_complete = 0;
    int remaining_template_records_delivered = 0;
    int signon_envelope_ready = 0;
    int framed_template_records_delivered = 0;
    int signon_batch_ready = 0;
    int pseudo_packet_batch_delivered = 0;
    int signon_wiremap_ready = 0;
    int wiremapped_batch_delivered = 0;
    int signon_burst_ready = 0;
    int pseudo_wire_burst_delivered = 0;
    int signon_stream_ready = 0;
    int contiguous_stream_delivered = 0;
    int signon_stream_window_ready = 0;
    int windowed_stream_delivered = 0;
    int signon_message_catalog_ready = 0;
    int message_boundaries_cataloged = 0;
    int signon_message_fetch_ready = 0;
    int targeted_message_fetch_delivered = 0;
    int signon_multi_message_fetch_ready = 0;
    int selective_message_set_delivered = 0;
    int signon_message_range_fetch_ready = 0;
    int range_message_set_delivered = 0;
    int signon_message_cursor_ready = 0;
    int message_cursor_checkpointed = 0;
    int signon_message_cursor_advance_ready = 0;
    int cursor_advanced_range_delivered = 0;
    int signon_message_cursor_eof_ready = 0;
    int message_cursor_exhausted = 0;
    int signon_message_cursor_resume_denied_ready = 0;
    int exhausted_cursor_resume_denied = 0;
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorResumeDenialProbeSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string probe = "disabled";
    int attempts = 0;
    int accepted = 0;
    int rejected = 0;
    std::string last_reject_reason = "<none>";
    std::string requested_cursor_id;
    std::string parsed_cursor_id;
    bool parsed_resume_allowed = false;
    std::string parsed_denial_reason;
    bool parsed_eof = false;
    bool parsed_exhausted = false;
    std::string parsed_next_start_message_index = "<unset>";
    int parsed_remaining_message_count = 0;
    std::string parsed_resume_policy;
    std::string parsed_map;
    std::string parsed_name;
    std::string parsed_ruleset;
    int parsed_spawned = 0;
    int signon_ready = 0;
    int bootstrap_delivered = 0;
    int baseline_ready = 0;
    int bootstrap_sequence_completed = 0;
    int signon_catalog_ready = 0;
    int bootstrap_records_staged = 0;
    int signon_template_ready = 0;
    int template_records_delivered = 0;
    int signon_template_coverage_complete = 0;
    int remaining_template_records_delivered = 0;
    int signon_envelope_ready = 0;
    int framed_template_records_delivered = 0;
    int signon_batch_ready = 0;
    int pseudo_packet_batch_delivered = 0;
    int signon_wiremap_ready = 0;
    int wiremapped_batch_delivered = 0;
    int signon_burst_ready = 0;
    int pseudo_wire_burst_delivered = 0;
    int signon_stream_ready = 0;
    int contiguous_stream_delivered = 0;
    int signon_stream_window_ready = 0;
    int windowed_stream_delivered = 0;
    int signon_message_catalog_ready = 0;
    int message_boundaries_cataloged = 0;
    int signon_message_fetch_ready = 0;
    int targeted_message_fetch_delivered = 0;
    int signon_multi_message_fetch_ready = 0;
    int selective_message_set_delivered = 0;
    int signon_message_range_fetch_ready = 0;
    int range_message_set_delivered = 0;
    int signon_message_cursor_ready = 0;
    int message_cursor_checkpointed = 0;
    int signon_message_cursor_advance_ready = 0;
    int cursor_advanced_range_delivered = 0;
    int signon_message_cursor_eof_ready = 0;
    int message_cursor_exhausted = 0;
    int signon_message_cursor_resume_denied_ready = 0;
    int exhausted_cursor_resume_denied = 0;
    std::string protocol_shape = "disabled";
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorResumeAllowSurfaceSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string bind = "disabled";
    int requested_port = 0;
    int bound_port = 0;
    bool shared_with_query = false;
    bool shared_with_connect = false;
    bool shared_with_activation = false;
    bool shared_with_bootstrap = false;
    bool shared_with_bootstrap_sequence = false;
    bool shared_with_signon_catalog = false;
    bool shared_with_signon_template = false;
    bool shared_with_signon_template_completion = false;
    bool shared_with_signon_envelope = false;
    bool shared_with_signon_batch = false;
    bool shared_with_signon_wiremap = false;
    bool shared_with_signon_burst = false;
    bool shared_with_signon_stream = false;
    bool shared_with_signon_stream_window = false;
    bool shared_with_signon_message_catalog = false;
    bool shared_with_signon_message_fetch = false;
    bool shared_with_signon_multi_message_fetch = false;
    bool shared_with_signon_message_range = false;
    bool shared_with_signon_message_cursor = false;
    bool shared_with_signon_message_cursor_advance = false;
    bool shared_with_signon_message_cursor_eof = false;
    bool shared_with_signon_message_cursor_resume_denial = false;
    std::string protocol_shape = "disabled";
    bool message_cursor_resume_allow_enabled = false;
    bool requires_message_cursor_ready_session = true;
    bool requires_non_exhausted_cursor = true;
    std::string resume_source = "disabled";
    std::string resume_payload = "disabled";
    int resume_count = 0;
    int accepted = 0;
    int rejected = 0;
    std::string last_allowed_cursor_id;
    bool resume_allowed = false;
    std::string denial_reason = "<unset>";
    bool eof = false;
    bool exhausted = false;
    int current_start_message_index = -1;
    int current_message_count = 0;
    std::string next_start_message_index = "<unset>";
    int remaining_message_count = 0;
    std::string resume_policy = "<unset>";
    std::string auth = "disabled";
    std::string signon = "disabled";
    std::string gameplay_transport = "no";
    int signon_ready = 0;
    int bootstrap_delivered = 0;
    int baseline_ready = 0;
    int bootstrap_sequence_completed = 0;
    int signon_catalog_ready = 0;
    int bootstrap_records_staged = 0;
    int signon_template_ready = 0;
    int template_records_delivered = 0;
    int signon_template_coverage_complete = 0;
    int remaining_template_records_delivered = 0;
    int signon_envelope_ready = 0;
    int framed_template_records_delivered = 0;
    int signon_batch_ready = 0;
    int pseudo_packet_batch_delivered = 0;
    int signon_wiremap_ready = 0;
    int wiremapped_batch_delivered = 0;
    int signon_burst_ready = 0;
    int pseudo_wire_burst_delivered = 0;
    int signon_stream_ready = 0;
    int contiguous_stream_delivered = 0;
    int signon_stream_window_ready = 0;
    int windowed_stream_delivered = 0;
    int signon_message_catalog_ready = 0;
    int message_boundaries_cataloged = 0;
    int signon_message_fetch_ready = 0;
    int targeted_message_fetch_delivered = 0;
    int signon_multi_message_fetch_ready = 0;
    int selective_message_set_delivered = 0;
    int signon_message_range_fetch_ready = 0;
    int range_message_set_delivered = 0;
    int signon_message_cursor_ready = 0;
    int message_cursor_checkpointed = 0;
    int signon_message_cursor_advance_ready = 0;
    int cursor_advanced_range_delivered = 0;
    int signon_message_cursor_eof_ready = 0;
    int message_cursor_exhausted = 0;
    int signon_message_cursor_resume_denied_ready = 0;
    int exhausted_cursor_resume_denied = 0;
    int signon_message_cursor_resume_ready = 0;
    int non_exhausted_cursor_resume_allowed = 0;
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorResumeAllowProbeSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string probe = "disabled";
    int attempts = 0;
    int accepted = 0;
    int rejected = 0;
    std::string last_reject_reason = "<none>";
    std::string requested_cursor_id;
    std::string parsed_cursor_id;
    bool parsed_resume_allowed = false;
    std::string parsed_denial_reason;
    bool parsed_eof = false;
    bool parsed_exhausted = false;
    std::string parsed_current_range;
    int parsed_next_start_message_index = -1;
    int parsed_remaining_message_count = 0;
    std::string parsed_resume_policy;
    std::string parsed_map;
    std::string parsed_name;
    std::string parsed_ruleset;
    int parsed_spawned = 0;
    int signon_ready = 0;
    int bootstrap_delivered = 0;
    int baseline_ready = 0;
    int bootstrap_sequence_completed = 0;
    int signon_catalog_ready = 0;
    int bootstrap_records_staged = 0;
    int signon_template_ready = 0;
    int template_records_delivered = 0;
    int signon_template_coverage_complete = 0;
    int remaining_template_records_delivered = 0;
    int signon_envelope_ready = 0;
    int framed_template_records_delivered = 0;
    int signon_batch_ready = 0;
    int pseudo_packet_batch_delivered = 0;
    int signon_wiremap_ready = 0;
    int wiremapped_batch_delivered = 0;
    int signon_burst_ready = 0;
    int pseudo_wire_burst_delivered = 0;
    int signon_stream_ready = 0;
    int contiguous_stream_delivered = 0;
    int signon_stream_window_ready = 0;
    int windowed_stream_delivered = 0;
    int signon_message_catalog_ready = 0;
    int message_boundaries_cataloged = 0;
    int signon_message_fetch_ready = 0;
    int targeted_message_fetch_delivered = 0;
    int signon_multi_message_fetch_ready = 0;
    int selective_message_set_delivered = 0;
    int signon_message_range_fetch_ready = 0;
    int range_message_set_delivered = 0;
    int signon_message_cursor_ready = 0;
    int message_cursor_checkpointed = 0;
    int signon_message_cursor_advance_ready = 0;
    int cursor_advanced_range_delivered = 0;
    int signon_message_cursor_eof_ready = 0;
    int message_cursor_exhausted = 0;
    int signon_message_cursor_resume_denied_ready = 0;
    int exhausted_cursor_resume_denied = 0;
    int signon_message_cursor_resume_ready = 0;
    int non_exhausted_cursor_resume_allowed = 0;
    std::string protocol_shape = "disabled";
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarryoverSurfaceSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string bind = "disabled";
    int requested_port = 0;
    int bound_port = 0;
    bool shared_with_query = false;
    bool shared_with_connect = false;
    bool shared_with_activation = false;
    bool shared_with_bootstrap = false;
    bool shared_with_bootstrap_sequence = false;
    bool shared_with_signon_catalog = false;
    bool shared_with_signon_template = false;
    bool shared_with_signon_template_completion = false;
    bool shared_with_signon_envelope = false;
    bool shared_with_signon_batch = false;
    bool shared_with_signon_wiremap = false;
    bool shared_with_signon_burst = false;
    bool shared_with_signon_stream = false;
    bool shared_with_signon_stream_window = false;
    bool shared_with_signon_message_catalog = false;
    bool shared_with_signon_message_fetch = false;
    bool shared_with_signon_multi_message_fetch = false;
    bool shared_with_signon_message_range = false;
    bool shared_with_signon_message_cursor = false;
    bool shared_with_signon_message_cursor_advance = false;
    bool shared_with_signon_message_cursor_eof = false;
    bool shared_with_signon_message_cursor_resume_denial = false;
    bool shared_with_signon_message_cursor_resume_allow = false;
    std::string protocol_shape = "disabled";
    bool message_cursor_carryover_enabled = false;
    bool requires_source_resume_allowed_session = true;
    bool requires_target_activated_session = true;
    std::string carryover_source = "disabled";
    std::string carryover_payload = "disabled";
    int accepted = 0;
    int rejected = 0;
    std::string last_source_session;
    std::string last_target_session;
    std::string last_carried_cursor_id;
    int carried_current_start_message_index = -1;
    int carried_current_message_count = 0;
    int carried_next_start_message_index = -1;
    int carried_remaining_message_count = 0;
    std::string carryover_policy = "<unset>";
    std::string auth = "disabled";
    std::string signon = "disabled";
    std::string gameplay_transport = "no";
    int signon_ready = 0;
    int bootstrap_delivered = 0;
    int baseline_ready = 0;
    int bootstrap_sequence_completed = 0;
    int signon_catalog_ready = 0;
    int bootstrap_records_staged = 0;
    int signon_template_ready = 0;
    int template_records_delivered = 0;
    int signon_template_coverage_complete = 0;
    int remaining_template_records_delivered = 0;
    int signon_envelope_ready = 0;
    int framed_template_records_delivered = 0;
    int signon_batch_ready = 0;
    int pseudo_packet_batch_delivered = 0;
    int signon_wiremap_ready = 0;
    int wiremapped_batch_delivered = 0;
    int signon_burst_ready = 0;
    int pseudo_wire_burst_delivered = 0;
    int signon_stream_ready = 0;
    int contiguous_stream_delivered = 0;
    int signon_stream_window_ready = 0;
    int windowed_stream_delivered = 0;
    int signon_message_catalog_ready = 0;
    int message_boundaries_cataloged = 0;
    int signon_message_fetch_ready = 0;
    int targeted_message_fetch_delivered = 0;
    int signon_multi_message_fetch_ready = 0;
    int selective_message_set_delivered = 0;
    int signon_message_range_fetch_ready = 0;
    int range_message_set_delivered = 0;
    int signon_message_cursor_ready = 0;
    int message_cursor_checkpointed = 0;
    int signon_message_cursor_advance_ready = 0;
    int cursor_advanced_range_delivered = 0;
    int signon_message_cursor_eof_ready = 0;
    int message_cursor_exhausted = 0;
    int signon_message_cursor_resume_denied_ready = 0;
    int exhausted_cursor_resume_denied = 0;
    int signon_message_cursor_resume_ready = 0;
    int non_exhausted_cursor_resume_allowed = 0;
    int signon_message_cursor_carryover_ready = 0;
    int checkpoint_cursor_carried_over = 0;
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarryoverProbeSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string probe = "disabled";
    int attempts = 0;
    int accepted = 0;
    int rejected = 0;
    std::string last_reject_reason = "<none>";
    std::string requested_source_cursor_id;
    std::string requested_target_session;
    std::string parsed_source_session;
    std::string parsed_target_session;
    std::string parsed_cursor_id;
    std::string parsed_carried_current_range;
    int parsed_carried_next_start_message_index = -1;
    int parsed_carried_remaining_message_count = 0;
    std::string parsed_carryover_policy;
    int parsed_target_activated = 0;
    std::string parsed_map;
    std::string parsed_name;
    std::string parsed_ruleset;
    int parsed_spawned = 0;
    int signon_ready = 0;
    int bootstrap_delivered = 0;
    int baseline_ready = 0;
    int bootstrap_sequence_completed = 0;
    int signon_catalog_ready = 0;
    int bootstrap_records_staged = 0;
    int signon_template_ready = 0;
    int template_records_delivered = 0;
    int signon_template_coverage_complete = 0;
    int remaining_template_records_delivered = 0;
    int signon_envelope_ready = 0;
    int framed_template_records_delivered = 0;
    int signon_batch_ready = 0;
    int pseudo_packet_batch_delivered = 0;
    int signon_wiremap_ready = 0;
    int wiremapped_batch_delivered = 0;
    int signon_burst_ready = 0;
    int pseudo_wire_burst_delivered = 0;
    int signon_stream_ready = 0;
    int contiguous_stream_delivered = 0;
    int signon_stream_window_ready = 0;
    int windowed_stream_delivered = 0;
    int signon_message_catalog_ready = 0;
    int message_boundaries_cataloged = 0;
    int signon_message_fetch_ready = 0;
    int targeted_message_fetch_delivered = 0;
    int signon_multi_message_fetch_ready = 0;
    int selective_message_set_delivered = 0;
    int signon_message_range_fetch_ready = 0;
    int range_message_set_delivered = 0;
    int signon_message_cursor_ready = 0;
    int message_cursor_checkpointed = 0;
    int signon_message_cursor_advance_ready = 0;
    int cursor_advanced_range_delivered = 0;
    int signon_message_cursor_eof_ready = 0;
    int message_cursor_exhausted = 0;
    int signon_message_cursor_resume_denied_ready = 0;
    int exhausted_cursor_resume_denied = 0;
    int signon_message_cursor_resume_ready = 0;
    int non_exhausted_cursor_resume_allowed = 0;
    int signon_message_cursor_carryover_ready = 0;
    int checkpoint_cursor_carried_over = 0;
    std::string protocol_shape = "disabled";
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedRangeSurfaceSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string bind = "disabled";
    int requested_port = 0;
    int bound_port = 0;
    bool shared_with_query = false;
    bool shared_with_connect = false;
    bool shared_with_activation = false;
    bool shared_with_bootstrap = false;
    bool shared_with_bootstrap_sequence = false;
    bool shared_with_signon_catalog = false;
    bool shared_with_signon_template = false;
    bool shared_with_signon_template_completion = false;
    bool shared_with_signon_envelope = false;
    bool shared_with_signon_batch = false;
    bool shared_with_signon_wiremap = false;
    bool shared_with_signon_burst = false;
    bool shared_with_signon_stream = false;
    bool shared_with_signon_stream_window = false;
    bool shared_with_signon_message_catalog = false;
    bool shared_with_signon_message_fetch = false;
    bool shared_with_signon_multi_message_fetch = false;
    bool shared_with_signon_message_range = false;
    bool shared_with_signon_message_cursor = false;
    bool shared_with_signon_message_cursor_advance = false;
    bool shared_with_signon_message_cursor_eof = false;
    bool shared_with_signon_message_cursor_resume_denial = false;
    bool shared_with_signon_message_cursor_resume_allow = false;
    bool shared_with_signon_message_cursor_carryover = false;
    std::string protocol_shape = "disabled";
    bool carried_range_enabled = false;
    bool requires_target_carryover_ready_session = true;
    bool requires_source_resume_allowed_session = true;
    std::string fetch_selector = "disabled";
    std::string fetch_payload = "disabled";
    int requested_message_count = 0;
    int accepted = 0;
    int rejected = 0;
    std::string last_source_session;
    std::string last_target_session;
    std::string last_carried_cursor_id;
    std::string last_fetched_message_indices;
    int next_start_message_index_after_fetch = -1;
    int remaining_message_count_after_fetch = 0;
    std::string auth = "disabled";
    std::string signon = "disabled";
    std::string gameplay_transport = "no";
    int signon_ready = 0;
    int bootstrap_delivered = 0;
    int baseline_ready = 0;
    int bootstrap_sequence_completed = 0;
    int signon_catalog_ready = 0;
    int bootstrap_records_staged = 0;
    int signon_template_ready = 0;
    int template_records_delivered = 0;
    int signon_template_coverage_complete = 0;
    int remaining_template_records_delivered = 0;
    int signon_envelope_ready = 0;
    int framed_template_records_delivered = 0;
    int signon_batch_ready = 0;
    int pseudo_packet_batch_delivered = 0;
    int signon_wiremap_ready = 0;
    int wiremapped_batch_delivered = 0;
    int signon_burst_ready = 0;
    int pseudo_wire_burst_delivered = 0;
    int signon_stream_ready = 0;
    int contiguous_stream_delivered = 0;
    int signon_stream_window_ready = 0;
    int windowed_stream_delivered = 0;
    int signon_message_catalog_ready = 0;
    int message_boundaries_cataloged = 0;
    int signon_message_fetch_ready = 0;
    int targeted_message_fetch_delivered = 0;
    int signon_multi_message_fetch_ready = 0;
    int selective_message_set_delivered = 0;
    int signon_message_range_fetch_ready = 0;
    int range_message_set_delivered = 0;
    int signon_message_cursor_ready = 0;
    int message_cursor_checkpointed = 0;
    int signon_message_cursor_advance_ready = 0;
    int cursor_advanced_range_delivered = 0;
    int signon_message_cursor_eof_ready = 0;
    int message_cursor_exhausted = 0;
    int signon_message_cursor_resume_denied_ready = 0;
    int exhausted_cursor_resume_denied = 0;
    int signon_message_cursor_resume_ready = 0;
    int non_exhausted_cursor_resume_allowed = 0;
    int signon_message_cursor_carryover_ready = 0;
    int checkpoint_cursor_carried_over = 0;
    int signon_message_cursor_carried_range_ready = 0;
    int carried_cursor_range_delivered = 0;
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedRangeProbeSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string probe = "disabled";
    int attempts = 0;
    int accepted = 0;
    int rejected = 0;
    std::string last_reject_reason = "<none>";
    std::string requested_target_session;
    std::string requested_cursor_id;
    int requested_message_count = 0;
    std::string parsed_source_session;
    std::string parsed_target_session;
    std::string parsed_cursor_id;
    std::string parsed_fetched_message_indices;
    std::string parsed_semantic_tags;
    int parsed_combined_byte_length = 0;
    int parsed_next_start_message_index_after_fetch = -1;
    int parsed_remaining_message_count_after_fetch = 0;
    std::string parsed_carryover_policy;
    int parsed_target_activated = 0;
    std::string parsed_map;
    std::string parsed_name;
    std::string parsed_ruleset;
    int parsed_spawned = 0;
    int signon_ready = 0;
    int bootstrap_delivered = 0;
    int baseline_ready = 0;
    int bootstrap_sequence_completed = 0;
    int signon_catalog_ready = 0;
    int bootstrap_records_staged = 0;
    int signon_template_ready = 0;
    int template_records_delivered = 0;
    int signon_template_coverage_complete = 0;
    int remaining_template_records_delivered = 0;
    int signon_envelope_ready = 0;
    int framed_template_records_delivered = 0;
    int signon_batch_ready = 0;
    int pseudo_packet_batch_delivered = 0;
    int signon_wiremap_ready = 0;
    int wiremapped_batch_delivered = 0;
    int signon_burst_ready = 0;
    int pseudo_wire_burst_delivered = 0;
    int signon_stream_ready = 0;
    int contiguous_stream_delivered = 0;
    int signon_stream_window_ready = 0;
    int windowed_stream_delivered = 0;
    int signon_message_catalog_ready = 0;
    int message_boundaries_cataloged = 0;
    int signon_message_fetch_ready = 0;
    int targeted_message_fetch_delivered = 0;
    int signon_multi_message_fetch_ready = 0;
    int selective_message_set_delivered = 0;
    int signon_message_range_fetch_ready = 0;
    int range_message_set_delivered = 0;
    int signon_message_cursor_ready = 0;
    int message_cursor_checkpointed = 0;
    int signon_message_cursor_advance_ready = 0;
    int cursor_advanced_range_delivered = 0;
    int signon_message_cursor_eof_ready = 0;
    int message_cursor_exhausted = 0;
    int signon_message_cursor_resume_denied_ready = 0;
    int exhausted_cursor_resume_denied = 0;
    int signon_message_cursor_resume_ready = 0;
    int non_exhausted_cursor_resume_allowed = 0;
    int signon_message_cursor_carryover_ready = 0;
    int checkpoint_cursor_carried_over = 0;
    int signon_message_cursor_carried_range_ready = 0;
    int carried_cursor_range_delivered = 0;
    std::string protocol_shape = "disabled";
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedEofSurfaceSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string bind = "disabled";
    int requested_port = 0;
    int bound_port = 0;
    bool shared_with_query = false;
    bool shared_with_connect = false;
    bool shared_with_activation = false;
    bool shared_with_bootstrap = false;
    bool shared_with_bootstrap_sequence = false;
    bool shared_with_signon_catalog = false;
    bool shared_with_signon_template = false;
    bool shared_with_signon_template_completion = false;
    bool shared_with_signon_envelope = false;
    bool shared_with_signon_batch = false;
    bool shared_with_signon_wiremap = false;
    bool shared_with_signon_burst = false;
    bool shared_with_signon_stream = false;
    bool shared_with_signon_stream_window = false;
    bool shared_with_signon_message_catalog = false;
    bool shared_with_signon_message_fetch = false;
    bool shared_with_signon_multi_message_fetch = false;
    bool shared_with_signon_message_range = false;
    bool shared_with_signon_message_cursor = false;
    bool shared_with_signon_message_cursor_advance = false;
    bool shared_with_signon_message_cursor_eof = false;
    bool shared_with_signon_message_cursor_resume_denial = false;
    bool shared_with_signon_message_cursor_resume_allow = false;
    bool shared_with_signon_message_cursor_carryover = false;
    bool shared_with_signon_message_cursor_carried_range = false;
    std::string protocol_shape = "disabled";
    bool carried_eof_enabled = false;
    bool requires_target_carried_range_ready_session = true;
    bool requires_source_resume_allowed_session = true;
    std::string eof_source = "disabled";
    std::string eof_payload = "disabled";
    int accepted = 0;
    int rejected = 0;
    std::string last_source_session;
    std::string last_target_session;
    std::string last_carried_cursor_id;
    bool eof = false;
    bool exhausted = false;
    int current_start_message_index = -1;
    int current_message_count = 0;
    std::string next_start_message_index = "<unset>";
    int remaining_message_count = 0;
    std::string auth = "disabled";
    std::string signon = "disabled";
    std::string gameplay_transport = "no";
    int signon_ready = 0;
    int bootstrap_delivered = 0;
    int baseline_ready = 0;
    int bootstrap_sequence_completed = 0;
    int signon_catalog_ready = 0;
    int bootstrap_records_staged = 0;
    int signon_template_ready = 0;
    int template_records_delivered = 0;
    int signon_template_coverage_complete = 0;
    int remaining_template_records_delivered = 0;
    int signon_envelope_ready = 0;
    int framed_template_records_delivered = 0;
    int signon_batch_ready = 0;
    int pseudo_packet_batch_delivered = 0;
    int signon_wiremap_ready = 0;
    int wiremapped_batch_delivered = 0;
    int signon_burst_ready = 0;
    int pseudo_wire_burst_delivered = 0;
    int signon_stream_ready = 0;
    int contiguous_stream_delivered = 0;
    int signon_stream_window_ready = 0;
    int windowed_stream_delivered = 0;
    int signon_message_catalog_ready = 0;
    int message_boundaries_cataloged = 0;
    int signon_message_fetch_ready = 0;
    int targeted_message_fetch_delivered = 0;
    int signon_multi_message_fetch_ready = 0;
    int selective_message_set_delivered = 0;
    int signon_message_range_fetch_ready = 0;
    int range_message_set_delivered = 0;
    int signon_message_cursor_ready = 0;
    int message_cursor_checkpointed = 0;
    int signon_message_cursor_advance_ready = 0;
    int cursor_advanced_range_delivered = 0;
    int signon_message_cursor_eof_ready = 0;
    int message_cursor_exhausted = 0;
    int signon_message_cursor_resume_denied_ready = 0;
    int exhausted_cursor_resume_denied = 0;
    int signon_message_cursor_resume_ready = 0;
    int non_exhausted_cursor_resume_allowed = 0;
    int signon_message_cursor_carryover_ready = 0;
    int checkpoint_cursor_carried_over = 0;
    int signon_message_cursor_carried_range_ready = 0;
    int carried_cursor_range_delivered = 0;
    int signon_message_cursor_carried_eof_ready = 0;
    int carried_cursor_exhausted = 0;
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedEofProbeSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string probe = "disabled";
    int attempts = 0;
    int accepted = 0;
    int rejected = 0;
    std::string last_reject_reason = "<none>";
    std::string requested_target_session;
    std::string requested_cursor_id;
    std::string parsed_source_session;
    std::string parsed_target_session;
    std::string parsed_cursor_id;
    bool parsed_eof = false;
    bool parsed_exhausted = false;
    std::string parsed_current_range;
    std::string parsed_next_start_message_index = "<unset>";
    int parsed_remaining_message_count = 0;
    std::string parsed_resume_policy;
    int parsed_target_activated = 0;
    std::string parsed_map;
    std::string parsed_name;
    std::string parsed_ruleset;
    int parsed_spawned = 0;
    int signon_ready = 0;
    int bootstrap_delivered = 0;
    int baseline_ready = 0;
    int bootstrap_sequence_completed = 0;
    int signon_catalog_ready = 0;
    int bootstrap_records_staged = 0;
    int signon_template_ready = 0;
    int template_records_delivered = 0;
    int signon_template_coverage_complete = 0;
    int remaining_template_records_delivered = 0;
    int signon_envelope_ready = 0;
    int framed_template_records_delivered = 0;
    int signon_batch_ready = 0;
    int pseudo_packet_batch_delivered = 0;
    int signon_wiremap_ready = 0;
    int wiremapped_batch_delivered = 0;
    int signon_burst_ready = 0;
    int pseudo_wire_burst_delivered = 0;
    int signon_stream_ready = 0;
    int contiguous_stream_delivered = 0;
    int signon_stream_window_ready = 0;
    int windowed_stream_delivered = 0;
    int signon_message_catalog_ready = 0;
    int message_boundaries_cataloged = 0;
    int signon_message_fetch_ready = 0;
    int targeted_message_fetch_delivered = 0;
    int signon_multi_message_fetch_ready = 0;
    int selective_message_set_delivered = 0;
    int signon_message_range_fetch_ready = 0;
    int range_message_set_delivered = 0;
    int signon_message_cursor_ready = 0;
    int message_cursor_checkpointed = 0;
    int signon_message_cursor_advance_ready = 0;
    int cursor_advanced_range_delivered = 0;
    int signon_message_cursor_eof_ready = 0;
    int message_cursor_exhausted = 0;
    int signon_message_cursor_resume_denied_ready = 0;
    int exhausted_cursor_resume_denied = 0;
    int signon_message_cursor_resume_ready = 0;
    int non_exhausted_cursor_resume_allowed = 0;
    int signon_message_cursor_carryover_ready = 0;
    int checkpoint_cursor_carried_over = 0;
    int signon_message_cursor_carried_range_ready = 0;
    int carried_cursor_range_delivered = 0;
    int signon_message_cursor_carried_eof_ready = 0;
    int carried_cursor_exhausted = 0;
    std::string protocol_shape = "disabled";
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedResumeAllowSurfaceSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string bind = "disabled";
    int requested_port = 0;
    int bound_port = 0;
    bool shared_with_query = false;
    bool shared_with_connect = false;
    bool shared_with_activation = false;
    bool shared_with_bootstrap = false;
    bool shared_with_bootstrap_sequence = false;
    bool shared_with_signon_catalog = false;
    bool shared_with_signon_template = false;
    bool shared_with_signon_template_completion = false;
    bool shared_with_signon_envelope = false;
    bool shared_with_signon_batch = false;
    bool shared_with_signon_wiremap = false;
    bool shared_with_signon_burst = false;
    bool shared_with_signon_stream = false;
    bool shared_with_signon_stream_window = false;
    bool shared_with_signon_message_catalog = false;
    bool shared_with_signon_message_fetch = false;
    bool shared_with_signon_multi_message_fetch = false;
    bool shared_with_signon_message_range = false;
    bool shared_with_signon_message_cursor = false;
    bool shared_with_signon_message_cursor_advance = false;
    bool shared_with_signon_message_cursor_eof = false;
    bool shared_with_signon_message_cursor_resume_denial = false;
    bool shared_with_signon_message_cursor_resume_allow = false;
    bool shared_with_signon_message_cursor_carryover = false;
    bool shared_with_signon_message_cursor_carried_range = false;
    bool shared_with_signon_message_cursor_carried_eof = false;
    bool shared_with_signon_message_cursor_carried_resume_denial = false;
    std::string protocol_shape = "disabled";
    bool carried_resume_allow_enabled = false;
    bool requires_target_carryover_ready_session = true;
    bool requires_source_resume_allowed_session = true;
    bool requires_non_exhausted_carried_cursor = true;
    std::string allow_source = "disabled";
    std::string allow_payload = "disabled";
    int accepted = 0;
    int rejected = 0;
    std::string last_source_session;
    std::string last_target_session;
    std::string last_allowed_cursor_id;
    bool resume_allowed = false;
    std::string denial_reason = "<unset>";
    bool eof = false;
    bool exhausted = false;
    std::string next_start_message_index = "<unset>";
    int remaining_message_count = 0;
    std::string auth = "disabled";
    std::string signon = "disabled";
    std::string gameplay_transport = "no";
    int signon_ready = 0;
    int bootstrap_delivered = 0;
    int baseline_ready = 0;
    int bootstrap_sequence_completed = 0;
    int signon_catalog_ready = 0;
    int bootstrap_records_staged = 0;
    int signon_template_ready = 0;
    int template_records_delivered = 0;
    int signon_template_coverage_complete = 0;
    int remaining_template_records_delivered = 0;
    int signon_envelope_ready = 0;
    int framed_template_records_delivered = 0;
    int signon_batch_ready = 0;
    int pseudo_packet_batch_delivered = 0;
    int signon_wiremap_ready = 0;
    int wiremapped_batch_delivered = 0;
    int signon_burst_ready = 0;
    int pseudo_wire_burst_delivered = 0;
    int signon_stream_ready = 0;
    int contiguous_stream_delivered = 0;
    int signon_stream_window_ready = 0;
    int windowed_stream_delivered = 0;
    int signon_message_catalog_ready = 0;
    int message_boundaries_cataloged = 0;
    int signon_message_fetch_ready = 0;
    int targeted_message_fetch_delivered = 0;
    int signon_multi_message_fetch_ready = 0;
    int selective_message_set_delivered = 0;
    int signon_message_range_fetch_ready = 0;
    int range_message_set_delivered = 0;
    int signon_message_cursor_ready = 0;
    int message_cursor_checkpointed = 0;
    int signon_message_cursor_advance_ready = 0;
    int cursor_advanced_range_delivered = 0;
    int signon_message_cursor_eof_ready = 0;
    int message_cursor_exhausted = 0;
    int signon_message_cursor_resume_denied_ready = 0;
    int exhausted_cursor_resume_denied = 0;
    int signon_message_cursor_resume_ready = 0;
    int non_exhausted_cursor_resume_allowed = 0;
    int signon_message_cursor_carryover_ready = 0;
    int checkpoint_cursor_carried_over = 0;
    int signon_message_cursor_carried_resume_ready = 0;
    int non_exhausted_carried_cursor_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_ready = 0;
    int carried_cursor_checkpointed = 0;
    int signon_message_cursor_carried_range_ready = 0;
    int carried_cursor_range_delivered = 0;
    int signon_message_cursor_carried_eof_ready = 0;
    int carried_cursor_exhausted = 0;
    int signon_message_cursor_carried_resume_denied_ready = 0;
    int carried_cursor_resume_denied = 0;
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedResumeAllowProbeSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string probe = "disabled";
    int attempts = 0;
    int accepted = 0;
    int rejected = 0;
    std::string last_reject_reason = "<none>";
    std::string requested_target_session;
    std::string requested_cursor_id;
    std::string parsed_source_session;
    std::string parsed_target_session;
    std::string parsed_cursor_id;
    bool parsed_resume_allowed = false;
    std::string parsed_denial_reason;
    bool parsed_eof = false;
    bool parsed_exhausted = false;
    std::string parsed_current_range;
    std::string parsed_next_start_message_index = "<unset>";
    int parsed_remaining_message_count = 0;
    std::string parsed_resume_policy;
    int parsed_target_activated = 0;
    std::string parsed_map;
    std::string parsed_name;
    std::string parsed_ruleset;
    int parsed_spawned = 0;
    int signon_ready = 0;
    int bootstrap_delivered = 0;
    int baseline_ready = 0;
    int bootstrap_sequence_completed = 0;
    int signon_catalog_ready = 0;
    int bootstrap_records_staged = 0;
    int signon_template_ready = 0;
    int template_records_delivered = 0;
    int signon_template_coverage_complete = 0;
    int remaining_template_records_delivered = 0;
    int signon_envelope_ready = 0;
    int framed_template_records_delivered = 0;
    int signon_batch_ready = 0;
    int pseudo_packet_batch_delivered = 0;
    int signon_wiremap_ready = 0;
    int wiremapped_batch_delivered = 0;
    int signon_burst_ready = 0;
    int pseudo_wire_burst_delivered = 0;
    int signon_stream_ready = 0;
    int contiguous_stream_delivered = 0;
    int signon_stream_window_ready = 0;
    int windowed_stream_delivered = 0;
    int signon_message_catalog_ready = 0;
    int message_boundaries_cataloged = 0;
    int signon_message_fetch_ready = 0;
    int targeted_message_fetch_delivered = 0;
    int signon_multi_message_fetch_ready = 0;
    int selective_message_set_delivered = 0;
    int signon_message_range_fetch_ready = 0;
    int range_message_set_delivered = 0;
    int signon_message_cursor_ready = 0;
    int message_cursor_checkpointed = 0;
    int signon_message_cursor_advance_ready = 0;
    int cursor_advanced_range_delivered = 0;
    int signon_message_cursor_eof_ready = 0;
    int message_cursor_exhausted = 0;
    int signon_message_cursor_resume_denied_ready = 0;
    int exhausted_cursor_resume_denied = 0;
    int signon_message_cursor_resume_ready = 0;
    int non_exhausted_cursor_resume_allowed = 0;
    int signon_message_cursor_carryover_ready = 0;
    int checkpoint_cursor_carried_over = 0;
    int signon_message_cursor_carried_resume_ready = 0;
    int non_exhausted_carried_cursor_resume_allowed = 0;
    int signon_message_cursor_carried_range_ready = 0;
    int carried_cursor_range_delivered = 0;
    int signon_message_cursor_carried_eof_ready = 0;
    int carried_cursor_exhausted = 0;
    int signon_message_cursor_carried_resume_denied_ready = 0;
    int carried_cursor_resume_denied = 0;
    std::string protocol_shape = "disabled";
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointSurfaceSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string bind = "disabled";
    int requested_port = 0;
    int bound_port = 0;
    bool shared_with_query = false;
    bool shared_with_connect = false;
    bool shared_with_activation = false;
    bool shared_with_bootstrap = false;
    bool shared_with_bootstrap_sequence = false;
    bool shared_with_signon_catalog = false;
    bool shared_with_signon_template = false;
    bool shared_with_signon_template_completion = false;
    bool shared_with_signon_envelope = false;
    bool shared_with_signon_batch = false;
    bool shared_with_signon_wiremap = false;
    bool shared_with_signon_burst = false;
    bool shared_with_signon_stream = false;
    bool shared_with_signon_stream_window = false;
    bool shared_with_signon_message_catalog = false;
    bool shared_with_signon_message_fetch = false;
    bool shared_with_signon_multi_message_fetch = false;
    bool shared_with_signon_message_range = false;
    bool shared_with_signon_message_cursor = false;
    bool shared_with_signon_message_cursor_advance = false;
    bool shared_with_signon_message_cursor_eof = false;
    bool shared_with_signon_message_cursor_resume_denial = false;
    bool shared_with_signon_message_cursor_resume_allow = false;
    bool shared_with_signon_message_cursor_carryover = false;
    bool shared_with_signon_message_cursor_carried_range = false;
    bool shared_with_signon_message_cursor_carried_eof = false;
    bool shared_with_signon_message_cursor_carried_resume_denial = false;
    bool shared_with_signon_message_cursor_carried_resume_allow = false;
    std::string protocol_shape = "disabled";
    bool carried_checkpoint_enabled = false;
    bool requires_target_carried_resume_ready_session = true;
    bool requires_source_resume_allowed_session = true;
    bool requires_non_exhausted_carried_cursor = true;
    std::string bridge_source = "disabled";
    std::string bridge_payload = "disabled";
    int accepted = 0;
    int rejected = 0;
    std::string last_source_session;
    std::string last_target_session;
    std::string last_source_cursor_id;
    std::string last_target_checkpoint_cursor_id;
    std::string next_start_message_index = "<unset>";
    int remaining_message_count = 0;
    std::string auth = "disabled";
    std::string signon = "disabled";
    std::string gameplay_transport = "no";
    int signon_ready = 0;
    int bootstrap_delivered = 0;
    int baseline_ready = 0;
    int bootstrap_sequence_completed = 0;
    int signon_catalog_ready = 0;
    int bootstrap_records_staged = 0;
    int signon_template_ready = 0;
    int template_records_delivered = 0;
    int signon_template_coverage_complete = 0;
    int remaining_template_records_delivered = 0;
    int signon_envelope_ready = 0;
    int framed_template_records_delivered = 0;
    int signon_batch_ready = 0;
    int pseudo_packet_batch_delivered = 0;
    int signon_wiremap_ready = 0;
    int wiremapped_batch_delivered = 0;
    int signon_burst_ready = 0;
    int pseudo_wire_burst_delivered = 0;
    int signon_stream_ready = 0;
    int contiguous_stream_delivered = 0;
    int signon_stream_window_ready = 0;
    int windowed_stream_delivered = 0;
    int signon_message_catalog_ready = 0;
    int message_boundaries_cataloged = 0;
    int signon_message_fetch_ready = 0;
    int targeted_message_fetch_delivered = 0;
    int signon_multi_message_fetch_ready = 0;
    int selective_message_set_delivered = 0;
    int signon_message_range_fetch_ready = 0;
    int range_message_set_delivered = 0;
    int signon_message_cursor_ready = 0;
    int message_cursor_checkpointed = 0;
    int signon_message_cursor_advance_ready = 0;
    int cursor_advanced_range_delivered = 0;
    int signon_message_cursor_eof_ready = 0;
    int message_cursor_exhausted = 0;
    int signon_message_cursor_resume_denied_ready = 0;
    int exhausted_cursor_resume_denied = 0;
    int signon_message_cursor_resume_ready = 0;
    int non_exhausted_cursor_resume_allowed = 0;
    int signon_message_cursor_carryover_ready = 0;
    int checkpoint_cursor_carried_over = 0;
    int signon_message_cursor_carried_resume_ready = 0;
    int non_exhausted_carried_cursor_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_ready = 0;
    int carried_cursor_checkpointed = 0;
    int signon_message_cursor_carried_range_ready = 0;
    int carried_cursor_range_delivered = 0;
    int signon_message_cursor_carried_eof_ready = 0;
    int carried_cursor_exhausted = 0;
    int signon_message_cursor_carried_resume_denied_ready = 0;
    int carried_cursor_resume_denied = 0;
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointProbeSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string probe = "disabled";
    int attempts = 0;
    int accepted = 0;
    int rejected = 0;
    std::string last_reject_reason = "<none>";
    std::string requested_target_session;
    std::string requested_cursor_id;
    std::string parsed_source_session;
    std::string parsed_target_session;
    std::string parsed_source_cursor_id;
    std::string parsed_target_checkpoint_cursor_id;
    std::string parsed_current_range;
    std::string parsed_next_start_message_index = "<unset>";
    int parsed_remaining_message_count = 0;
    std::string parsed_bridge_policy;
    int parsed_target_activated = 0;
    std::string parsed_map;
    std::string parsed_name;
    std::string parsed_ruleset;
    int parsed_spawned = 0;
    int signon_ready = 0;
    int bootstrap_delivered = 0;
    int baseline_ready = 0;
    int bootstrap_sequence_completed = 0;
    int signon_catalog_ready = 0;
    int bootstrap_records_staged = 0;
    int signon_template_ready = 0;
    int template_records_delivered = 0;
    int signon_template_coverage_complete = 0;
    int remaining_template_records_delivered = 0;
    int signon_envelope_ready = 0;
    int framed_template_records_delivered = 0;
    int signon_batch_ready = 0;
    int pseudo_packet_batch_delivered = 0;
    int signon_wiremap_ready = 0;
    int wiremapped_batch_delivered = 0;
    int signon_burst_ready = 0;
    int pseudo_wire_burst_delivered = 0;
    int signon_stream_ready = 0;
    int contiguous_stream_delivered = 0;
    int signon_stream_window_ready = 0;
    int windowed_stream_delivered = 0;
    int signon_message_catalog_ready = 0;
    int message_boundaries_cataloged = 0;
    int signon_message_fetch_ready = 0;
    int targeted_message_fetch_delivered = 0;
    int signon_multi_message_fetch_ready = 0;
    int selective_message_set_delivered = 0;
    int signon_message_range_fetch_ready = 0;
    int range_message_set_delivered = 0;
    int signon_message_cursor_ready = 0;
    int message_cursor_checkpointed = 0;
    int signon_message_cursor_advance_ready = 0;
    int cursor_advanced_range_delivered = 0;
    int signon_message_cursor_eof_ready = 0;
    int message_cursor_exhausted = 0;
    int signon_message_cursor_resume_denied_ready = 0;
    int exhausted_cursor_resume_denied = 0;
    int signon_message_cursor_resume_ready = 0;
    int non_exhausted_cursor_resume_allowed = 0;
    int signon_message_cursor_carryover_ready = 0;
    int checkpoint_cursor_carried_over = 0;
    int signon_message_cursor_carried_resume_ready = 0;
    int non_exhausted_carried_cursor_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_ready = 0;
    int carried_cursor_checkpointed = 0;
    int signon_message_cursor_carried_range_ready = 0;
    int carried_cursor_range_delivered = 0;
    int signon_message_cursor_carried_eof_ready = 0;
    int carried_cursor_exhausted = 0;
    int signon_message_cursor_carried_resume_denied_ready = 0;
    int carried_cursor_resume_denied = 0;
    std::string protocol_shape = "disabled";
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointAdvanceSurfaceSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string bind = "disabled";
    int requested_port = 0;
    int bound_port = 0;
    bool shared_with_query = false;
    bool shared_with_connect = false;
    bool shared_with_activation = false;
    bool shared_with_bootstrap = false;
    bool shared_with_bootstrap_sequence = false;
    bool shared_with_signon_catalog = false;
    bool shared_with_signon_template = false;
    bool shared_with_signon_template_completion = false;
    bool shared_with_signon_envelope = false;
    bool shared_with_signon_batch = false;
    bool shared_with_signon_wiremap = false;
    bool shared_with_signon_burst = false;
    bool shared_with_signon_stream = false;
    bool shared_with_signon_stream_window = false;
    bool shared_with_signon_message_catalog = false;
    bool shared_with_signon_message_fetch = false;
    bool shared_with_signon_multi_message_fetch = false;
    bool shared_with_signon_message_range = false;
    bool shared_with_signon_message_cursor = false;
    bool shared_with_signon_message_cursor_advance = false;
    bool shared_with_signon_message_cursor_eof = false;
    bool shared_with_signon_message_cursor_resume_denial = false;
    bool shared_with_signon_message_cursor_resume_allow = false;
    bool shared_with_signon_message_cursor_carryover = false;
    bool shared_with_signon_message_cursor_carried_range = false;
    bool shared_with_signon_message_cursor_carried_eof = false;
    bool shared_with_signon_message_cursor_carried_resume_denial = false;
    bool shared_with_signon_message_cursor_carried_resume_allow = false;
    bool shared_with_signon_message_cursor_carried_checkpoint = false;
    std::string protocol_shape = "disabled";
    bool carried_checkpoint_advance_enabled = false;
    bool requires_target_carried_checkpoint_ready_session = true;
    bool requires_source_resume_allowed_session = true;
    bool requires_non_exhausted_carried_checkpoint = true;
    std::string fetch_selector = "disabled";
    std::string fetch_payload = "disabled";
    int requested_message_count = 0;
    int accepted = 0;
    int rejected = 0;
    std::string last_source_session;
    std::string last_target_session;
    std::string last_checkpoint_cursor_id;
    std::string last_fetched_message_indices;
    std::string next_start_message_index_after_advance = "<unset>";
    int remaining_message_count_after_advance = 0;
    std::string auth = "disabled";
    std::string signon = "disabled";
    std::string gameplay_transport = "no";
    int signon_ready = 0;
    int bootstrap_delivered = 0;
    int baseline_ready = 0;
    int bootstrap_sequence_completed = 0;
    int signon_catalog_ready = 0;
    int bootstrap_records_staged = 0;
    int signon_template_ready = 0;
    int template_records_delivered = 0;
    int signon_template_coverage_complete = 0;
    int remaining_template_records_delivered = 0;
    int signon_envelope_ready = 0;
    int framed_template_records_delivered = 0;
    int signon_batch_ready = 0;
    int pseudo_packet_batch_delivered = 0;
    int signon_wiremap_ready = 0;
    int wiremapped_batch_delivered = 0;
    int signon_burst_ready = 0;
    int pseudo_wire_burst_delivered = 0;
    int signon_stream_ready = 0;
    int contiguous_stream_delivered = 0;
    int signon_stream_window_ready = 0;
    int windowed_stream_delivered = 0;
    int signon_message_catalog_ready = 0;
    int message_boundaries_cataloged = 0;
    int signon_message_fetch_ready = 0;
    int targeted_message_fetch_delivered = 0;
    int signon_multi_message_fetch_ready = 0;
    int selective_message_set_delivered = 0;
    int signon_message_range_fetch_ready = 0;
    int range_message_set_delivered = 0;
    int signon_message_cursor_ready = 0;
    int message_cursor_checkpointed = 0;
    int signon_message_cursor_advance_ready = 0;
    int cursor_advanced_range_delivered = 0;
    int signon_message_cursor_eof_ready = 0;
    int message_cursor_exhausted = 0;
    int signon_message_cursor_resume_denied_ready = 0;
    int exhausted_cursor_resume_denied = 0;
    int signon_message_cursor_resume_ready = 0;
    int non_exhausted_cursor_resume_allowed = 0;
    int signon_message_cursor_carryover_ready = 0;
    int checkpoint_cursor_carried_over = 0;
    int signon_message_cursor_carried_resume_ready = 0;
    int non_exhausted_carried_cursor_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_ready = 0;
    int carried_cursor_checkpointed = 0;
    int signon_message_cursor_carried_checkpoint_advance_ready = 0;
    int carried_checkpoint_advanced_range_delivered = 0;
    int signon_message_cursor_carried_range_ready = 0;
    int carried_cursor_range_delivered = 0;
    int signon_message_cursor_carried_eof_ready = 0;
    int carried_cursor_exhausted = 0;
    int signon_message_cursor_carried_resume_denied_ready = 0;
    int carried_cursor_resume_denied = 0;
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointAdvanceProbeSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string probe = "disabled";
    int attempts = 0;
    int accepted = 0;
    int rejected = 0;
    std::string last_reject_reason = "<none>";
    std::string requested_target_session;
    int requested_message_count = 0;
    std::string requested_cursor_id;
    std::string parsed_source_session;
    std::string parsed_target_session;
    std::string parsed_cursor_id;
    std::string parsed_advanced_range;
    std::string parsed_fetched_message_indices;
    std::string parsed_semantic_tags;
    int parsed_combined_byte_length = 0;
    std::string parsed_next_start_message_index_after_advance = "<unset>";
    int parsed_remaining_message_count_after_advance = 0;
    std::string parsed_bridge_policy;
    int parsed_target_activated = 0;
    std::string parsed_map;
    std::string parsed_name;
    std::string parsed_ruleset;
    int parsed_spawned = 0;
    int signon_ready = 0;
    int bootstrap_delivered = 0;
    int baseline_ready = 0;
    int bootstrap_sequence_completed = 0;
    int signon_catalog_ready = 0;
    int bootstrap_records_staged = 0;
    int signon_template_ready = 0;
    int template_records_delivered = 0;
    int signon_template_coverage_complete = 0;
    int remaining_template_records_delivered = 0;
    int signon_envelope_ready = 0;
    int framed_template_records_delivered = 0;
    int signon_batch_ready = 0;
    int pseudo_packet_batch_delivered = 0;
    int signon_wiremap_ready = 0;
    int wiremapped_batch_delivered = 0;
    int signon_burst_ready = 0;
    int pseudo_wire_burst_delivered = 0;
    int signon_stream_ready = 0;
    int contiguous_stream_delivered = 0;
    int signon_stream_window_ready = 0;
    int windowed_stream_delivered = 0;
    int signon_message_catalog_ready = 0;
    int message_boundaries_cataloged = 0;
    int signon_message_fetch_ready = 0;
    int targeted_message_fetch_delivered = 0;
    int signon_multi_message_fetch_ready = 0;
    int selective_message_set_delivered = 0;
    int signon_message_range_fetch_ready = 0;
    int range_message_set_delivered = 0;
    int signon_message_cursor_ready = 0;
    int message_cursor_checkpointed = 0;
    int signon_message_cursor_advance_ready = 0;
    int cursor_advanced_range_delivered = 0;
    int signon_message_cursor_eof_ready = 0;
    int message_cursor_exhausted = 0;
    int signon_message_cursor_resume_denied_ready = 0;
    int exhausted_cursor_resume_denied = 0;
    int signon_message_cursor_resume_ready = 0;
    int non_exhausted_cursor_resume_allowed = 0;
    int signon_message_cursor_carryover_ready = 0;
    int checkpoint_cursor_carried_over = 0;
    int signon_message_cursor_carried_resume_ready = 0;
    int non_exhausted_carried_cursor_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_ready = 0;
    int carried_cursor_checkpointed = 0;
    int signon_message_cursor_carried_checkpoint_advance_ready = 0;
    int carried_checkpoint_advanced_range_delivered = 0;
    int signon_message_cursor_carried_range_ready = 0;
    int carried_cursor_range_delivered = 0;
    int signon_message_cursor_carried_eof_ready = 0;
    int carried_cursor_exhausted = 0;
    int signon_message_cursor_carried_resume_denied_ready = 0;
    int carried_cursor_resume_denied = 0;
    std::string protocol_shape = "disabled";
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointEofSurfaceSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string bind = "disabled";
    int requested_port = 0;
    int bound_port = 0;
    bool shared_with_query = false;
    bool shared_with_connect = false;
    bool shared_with_activation = false;
    bool shared_with_bootstrap = false;
    bool shared_with_bootstrap_sequence = false;
    bool shared_with_signon_catalog = false;
    bool shared_with_signon_template = false;
    bool shared_with_signon_template_completion = false;
    bool shared_with_signon_envelope = false;
    bool shared_with_signon_batch = false;
    bool shared_with_signon_wiremap = false;
    bool shared_with_signon_burst = false;
    bool shared_with_signon_stream = false;
    bool shared_with_signon_stream_window = false;
    bool shared_with_signon_message_catalog = false;
    bool shared_with_signon_message_fetch = false;
    bool shared_with_signon_multi_message_fetch = false;
    bool shared_with_signon_message_range = false;
    bool shared_with_signon_message_cursor = false;
    bool shared_with_signon_message_cursor_advance = false;
    bool shared_with_signon_message_cursor_eof = false;
    bool shared_with_signon_message_cursor_resume_denial = false;
    bool shared_with_signon_message_cursor_resume_allow = false;
    bool shared_with_signon_message_cursor_carryover = false;
    bool shared_with_signon_message_cursor_carried_range = false;
    bool shared_with_signon_message_cursor_carried_eof = false;
    bool shared_with_signon_message_cursor_carried_resume_denial = false;
    bool shared_with_signon_message_cursor_carried_resume_allow = false;
    bool shared_with_signon_message_cursor_carried_checkpoint = false;
    bool shared_with_signon_message_cursor_carried_checkpoint_advance = false;
    std::string protocol_shape = "disabled";
    bool carried_checkpoint_eof_enabled = false;
    bool requires_target_carried_checkpoint_advance_ready_session = true;
    bool requires_source_resume_allowed_session = true;
    std::string eof_source = "disabled";
    std::string eof_payload = "disabled";
    int accepted = 0;
    int rejected = 0;
    std::string last_source_session;
    std::string last_target_session;
    std::string last_checkpoint_cursor_id;
    bool eof = false;
    bool exhausted = false;
    std::string current_range = "<unset>";
    std::string next_start_message_index = "<unset>";
    int remaining_message_count = 0;
    std::string auth = "disabled";
    std::string signon = "disabled";
    std::string gameplay_transport = "no";
    int signon_ready = 0;
    int bootstrap_delivered = 0;
    int baseline_ready = 0;
    int bootstrap_sequence_completed = 0;
    int signon_catalog_ready = 0;
    int bootstrap_records_staged = 0;
    int signon_template_ready = 0;
    int template_records_delivered = 0;
    int signon_template_coverage_complete = 0;
    int remaining_template_records_delivered = 0;
    int signon_envelope_ready = 0;
    int framed_template_records_delivered = 0;
    int signon_batch_ready = 0;
    int pseudo_packet_batch_delivered = 0;
    int signon_wiremap_ready = 0;
    int wiremapped_batch_delivered = 0;
    int signon_burst_ready = 0;
    int pseudo_wire_burst_delivered = 0;
    int signon_stream_ready = 0;
    int contiguous_stream_delivered = 0;
    int signon_stream_window_ready = 0;
    int windowed_stream_delivered = 0;
    int signon_message_catalog_ready = 0;
    int message_boundaries_cataloged = 0;
    int signon_message_fetch_ready = 0;
    int targeted_message_fetch_delivered = 0;
    int signon_multi_message_fetch_ready = 0;
    int selective_message_set_delivered = 0;
    int signon_message_range_fetch_ready = 0;
    int range_message_set_delivered = 0;
    int signon_message_cursor_ready = 0;
    int message_cursor_checkpointed = 0;
    int signon_message_cursor_advance_ready = 0;
    int cursor_advanced_range_delivered = 0;
    int signon_message_cursor_eof_ready = 0;
    int message_cursor_exhausted = 0;
    int signon_message_cursor_resume_denied_ready = 0;
    int exhausted_cursor_resume_denied = 0;
    int signon_message_cursor_resume_ready = 0;
    int non_exhausted_cursor_resume_allowed = 0;
    int signon_message_cursor_carryover_ready = 0;
    int checkpoint_cursor_carried_over = 0;
    int signon_message_cursor_carried_resume_ready = 0;
    int non_exhausted_carried_cursor_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_ready = 0;
    int carried_cursor_checkpointed = 0;
    int signon_message_cursor_carried_checkpoint_advance_ready = 0;
    int carried_checkpoint_advanced_range_delivered = 0;
    int signon_message_cursor_carried_checkpoint_eof_ready = 0;
    int carried_checkpoint_exhausted = 0;
    int signon_message_cursor_carried_checkpoint_resume_denied_ready = 0;
    int carried_checkpoint_resume_denied = 0;
    int signon_message_cursor_carried_range_ready = 0;
    int carried_cursor_range_delivered = 0;
    int signon_message_cursor_carried_eof_ready = 0;
    int carried_cursor_exhausted = 0;
    int signon_message_cursor_carried_resume_denied_ready = 0;
    int carried_cursor_resume_denied = 0;
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointEofProbeSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string probe = "disabled";
    int attempts = 0;
    int accepted = 0;
    int rejected = 0;
    std::string last_reject_reason = "<none>";
    std::string requested_target_session;
    std::string requested_cursor_id;
    std::string parsed_source_session;
    std::string parsed_target_session;
    std::string parsed_cursor_id;
    bool parsed_eof = false;
    bool parsed_exhausted = false;
    std::string parsed_current_range;
    std::string parsed_next_start_message_index = "<unset>";
    int parsed_remaining_message_count = 0;
    std::string parsed_resume_policy;
    int parsed_target_activated = 0;
    std::string parsed_map;
    std::string parsed_name;
    std::string parsed_ruleset;
    int parsed_spawned = 0;
    int signon_ready = 0;
    int bootstrap_delivered = 0;
    int baseline_ready = 0;
    int bootstrap_sequence_completed = 0;
    int signon_catalog_ready = 0;
    int bootstrap_records_staged = 0;
    int signon_template_ready = 0;
    int template_records_delivered = 0;
    int signon_template_coverage_complete = 0;
    int remaining_template_records_delivered = 0;
    int signon_envelope_ready = 0;
    int framed_template_records_delivered = 0;
    int signon_batch_ready = 0;
    int pseudo_packet_batch_delivered = 0;
    int signon_wiremap_ready = 0;
    int wiremapped_batch_delivered = 0;
    int signon_burst_ready = 0;
    int pseudo_wire_burst_delivered = 0;
    int signon_stream_ready = 0;
    int contiguous_stream_delivered = 0;
    int signon_stream_window_ready = 0;
    int windowed_stream_delivered = 0;
    int signon_message_catalog_ready = 0;
    int message_boundaries_cataloged = 0;
    int signon_message_fetch_ready = 0;
    int targeted_message_fetch_delivered = 0;
    int signon_multi_message_fetch_ready = 0;
    int selective_message_set_delivered = 0;
    int signon_message_range_fetch_ready = 0;
    int range_message_set_delivered = 0;
    int signon_message_cursor_ready = 0;
    int message_cursor_checkpointed = 0;
    int signon_message_cursor_advance_ready = 0;
    int cursor_advanced_range_delivered = 0;
    int signon_message_cursor_eof_ready = 0;
    int message_cursor_exhausted = 0;
    int signon_message_cursor_resume_denied_ready = 0;
    int exhausted_cursor_resume_denied = 0;
    int signon_message_cursor_resume_ready = 0;
    int non_exhausted_cursor_resume_allowed = 0;
    int signon_message_cursor_carryover_ready = 0;
    int checkpoint_cursor_carried_over = 0;
    int signon_message_cursor_carried_resume_ready = 0;
    int non_exhausted_carried_cursor_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_ready = 0;
    int carried_cursor_checkpointed = 0;
    int signon_message_cursor_carried_checkpoint_advance_ready = 0;
    int carried_checkpoint_advanced_range_delivered = 0;
    int signon_message_cursor_carried_checkpoint_eof_ready = 0;
    int carried_checkpoint_exhausted = 0;
    int signon_message_cursor_carried_range_ready = 0;
    int carried_cursor_range_delivered = 0;
    int signon_message_cursor_carried_eof_ready = 0;
    int carried_cursor_exhausted = 0;
    int signon_message_cursor_carried_resume_denied_ready = 0;
    int carried_cursor_resume_denied = 0;
    std::string protocol_shape = "disabled";
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointResumeAllowSurfaceSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string bind = "disabled";
    int requested_port = 0;
    int bound_port = 0;
    bool shared_with_query = false;
    bool shared_with_connect = false;
    bool shared_with_activation = false;
    bool shared_with_bootstrap = false;
    bool shared_with_bootstrap_sequence = false;
    bool shared_with_signon_catalog = false;
    bool shared_with_signon_template = false;
    bool shared_with_signon_template_completion = false;
    bool shared_with_signon_envelope = false;
    bool shared_with_signon_batch = false;
    bool shared_with_signon_wiremap = false;
    bool shared_with_signon_burst = false;
    bool shared_with_signon_stream = false;
    bool shared_with_signon_stream_window = false;
    bool shared_with_signon_message_catalog = false;
    bool shared_with_signon_message_fetch = false;
    bool shared_with_signon_multi_message_fetch = false;
    bool shared_with_signon_message_range = false;
    bool shared_with_signon_message_cursor = false;
    bool shared_with_signon_message_cursor_advance = false;
    bool shared_with_signon_message_cursor_eof = false;
    bool shared_with_signon_message_cursor_resume_denial = false;
    bool shared_with_signon_message_cursor_resume_allow = false;
    bool shared_with_signon_message_cursor_carryover = false;
    bool shared_with_signon_message_cursor_carried_range = false;
    bool shared_with_signon_message_cursor_carried_eof = false;
    bool shared_with_signon_message_cursor_carried_resume_denial = false;
    bool shared_with_signon_message_cursor_carried_resume_allow = false;
    bool shared_with_signon_message_cursor_carried_checkpoint = false;
    bool shared_with_signon_message_cursor_carried_checkpoint_advance = false;
    bool shared_with_signon_message_cursor_carried_checkpoint_eof = false;
    bool shared_with_signon_message_cursor_carried_checkpoint_resume_denial = false;
    std::string protocol_shape = "disabled";
    bool carried_checkpoint_resume_allow_enabled = false;
    bool requires_target_carried_checkpoint_ready_session = true;
    bool requires_source_resume_allowed_session = true;
    bool requires_non_exhausted_carried_checkpoint = true;
    std::string allow_source = "disabled";
    std::string allow_payload = "disabled";
    int accepted = 0;
    int rejected = 0;
    std::string last_source_session;
    std::string last_target_session;
    std::string last_allowed_cursor_id;
    bool resume_allowed = false;
    std::string denial_reason = "<unset>";
    bool eof = false;
    bool exhausted = false;
    std::string next_start_message_index = "<unset>";
    int remaining_message_count = 0;
    std::string auth = "disabled";
    std::string signon = "disabled";
    std::string gameplay_transport = "no";
    int signon_ready = 0;
    int bootstrap_delivered = 0;
    int baseline_ready = 0;
    int bootstrap_sequence_completed = 0;
    int signon_catalog_ready = 0;
    int bootstrap_records_staged = 0;
    int signon_template_ready = 0;
    int template_records_delivered = 0;
    int signon_template_coverage_complete = 0;
    int remaining_template_records_delivered = 0;
    int signon_envelope_ready = 0;
    int framed_template_records_delivered = 0;
    int signon_batch_ready = 0;
    int pseudo_packet_batch_delivered = 0;
    int signon_wiremap_ready = 0;
    int wiremapped_batch_delivered = 0;
    int signon_burst_ready = 0;
    int pseudo_wire_burst_delivered = 0;
    int signon_stream_ready = 0;
    int contiguous_stream_delivered = 0;
    int signon_stream_window_ready = 0;
    int windowed_stream_delivered = 0;
    int signon_message_catalog_ready = 0;
    int message_boundaries_cataloged = 0;
    int signon_message_fetch_ready = 0;
    int targeted_message_fetch_delivered = 0;
    int signon_multi_message_fetch_ready = 0;
    int selective_message_set_delivered = 0;
    int signon_message_range_fetch_ready = 0;
    int range_message_set_delivered = 0;
    int signon_message_cursor_ready = 0;
    int message_cursor_checkpointed = 0;
    int signon_message_cursor_advance_ready = 0;
    int cursor_advanced_range_delivered = 0;
    int signon_message_cursor_eof_ready = 0;
    int message_cursor_exhausted = 0;
    int signon_message_cursor_resume_denied_ready = 0;
    int exhausted_cursor_resume_denied = 0;
    int signon_message_cursor_resume_ready = 0;
    int non_exhausted_cursor_resume_allowed = 0;
    int signon_message_cursor_carryover_ready = 0;
    int checkpoint_cursor_carried_over = 0;
    int signon_message_cursor_carried_resume_ready = 0;
    int non_exhausted_carried_cursor_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_ready = 0;
    int carried_cursor_checkpointed = 0;
    int signon_message_cursor_carried_checkpoint_resume_ready = 0;
    int non_exhausted_carried_checkpoint_resume_allowed = 0;
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointResumeAllowProbeSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string probe = "disabled";
    int attempts = 0;
    int accepted = 0;
    int rejected = 0;
    std::string last_reject_reason = "<none>";
    std::string requested_target_session;
    std::string requested_cursor_id;
    std::string parsed_source_session;
    std::string parsed_target_session;
    std::string parsed_cursor_id;
    bool parsed_resume_allowed = false;
    std::string parsed_denial_reason;
    bool parsed_eof = false;
    bool parsed_exhausted = false;
    std::string parsed_current_range;
    std::string parsed_next_start_message_index = "<unset>";
    int parsed_remaining_message_count = 0;
    std::string parsed_resume_policy;
    int parsed_target_activated = 0;
    std::string parsed_map;
    std::string parsed_name;
    std::string parsed_ruleset;
    int parsed_spawned = 0;
    int signon_ready = 0;
    int bootstrap_delivered = 0;
    int baseline_ready = 0;
    int bootstrap_sequence_completed = 0;
    int signon_catalog_ready = 0;
    int bootstrap_records_staged = 0;
    int signon_template_ready = 0;
    int template_records_delivered = 0;
    int signon_template_coverage_complete = 0;
    int remaining_template_records_delivered = 0;
    int signon_envelope_ready = 0;
    int framed_template_records_delivered = 0;
    int signon_batch_ready = 0;
    int pseudo_packet_batch_delivered = 0;
    int signon_wiremap_ready = 0;
    int wiremapped_batch_delivered = 0;
    int signon_burst_ready = 0;
    int pseudo_wire_burst_delivered = 0;
    int signon_stream_ready = 0;
    int contiguous_stream_delivered = 0;
    int signon_stream_window_ready = 0;
    int windowed_stream_delivered = 0;
    int signon_message_catalog_ready = 0;
    int message_boundaries_cataloged = 0;
    int signon_message_fetch_ready = 0;
    int targeted_message_fetch_delivered = 0;
    int signon_multi_message_fetch_ready = 0;
    int selective_message_set_delivered = 0;
    int signon_message_range_fetch_ready = 0;
    int range_message_set_delivered = 0;
    int signon_message_cursor_ready = 0;
    int message_cursor_checkpointed = 0;
    int signon_message_cursor_advance_ready = 0;
    int cursor_advanced_range_delivered = 0;
    int signon_message_cursor_eof_ready = 0;
    int message_cursor_exhausted = 0;
    int signon_message_cursor_resume_denied_ready = 0;
    int exhausted_cursor_resume_denied = 0;
    int signon_message_cursor_resume_ready = 0;
    int non_exhausted_cursor_resume_allowed = 0;
    int signon_message_cursor_carryover_ready = 0;
    int checkpoint_cursor_carried_over = 0;
    int signon_message_cursor_carried_resume_ready = 0;
    int non_exhausted_carried_cursor_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_ready = 0;
    int carried_cursor_checkpointed = 0;
    int signon_message_cursor_carried_checkpoint_resume_ready = 0;
    int non_exhausted_carried_checkpoint_resume_allowed = 0;
    std::string protocol_shape = "disabled";
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointResumeTokenSurfaceSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string bind = "disabled";
    int requested_port = 0;
    int bound_port = 0;
    std::string protocol_shape = "disabled";
    bool carried_checkpoint_resume_token_enabled = false;
    bool requires_target_carried_checkpoint_resume_ready_session = true;
    bool requires_source_resume_allowed_session = true;
    bool requires_non_exhausted_carried_checkpoint = true;
    std::string token_purpose = "disabled";
    std::string token_payload = "disabled";
    int accepted = 0;
    int rejected = 0;
    std::string last_source_session;
    std::string last_target_session;
    std::string last_checkpoint_cursor_id;
    std::string last_issued_token;
    std::string claim_validation_implemented = "no";
    std::string auth = "disabled";
    std::string signon = "disabled";
    std::string gameplay_transport = "no";
    int signon_ready = 0;
    int bootstrap_delivered = 0;
    int baseline_ready = 0;
    int bootstrap_sequence_completed = 0;
    int signon_catalog_ready = 0;
    int bootstrap_records_staged = 0;
    int signon_template_ready = 0;
    int template_records_delivered = 0;
    int signon_template_coverage_complete = 0;
    int remaining_template_records_delivered = 0;
    int signon_envelope_ready = 0;
    int framed_template_records_delivered = 0;
    int signon_batch_ready = 0;
    int pseudo_packet_batch_delivered = 0;
    int signon_wiremap_ready = 0;
    int wiremapped_batch_delivered = 0;
    int signon_burst_ready = 0;
    int pseudo_wire_burst_delivered = 0;
    int signon_stream_ready = 0;
    int contiguous_stream_delivered = 0;
    int signon_stream_window_ready = 0;
    int windowed_stream_delivered = 0;
    int signon_message_catalog_ready = 0;
    int message_boundaries_cataloged = 0;
    int signon_message_fetch_ready = 0;
    int targeted_message_fetch_delivered = 0;
    int signon_multi_message_fetch_ready = 0;
    int selective_message_set_delivered = 0;
    int signon_message_range_fetch_ready = 0;
    int range_message_set_delivered = 0;
    int signon_message_cursor_ready = 0;
    int message_cursor_checkpointed = 0;
    int signon_message_cursor_advance_ready = 0;
    int cursor_advanced_range_delivered = 0;
    int signon_message_cursor_eof_ready = 0;
    int message_cursor_exhausted = 0;
    int signon_message_cursor_resume_denied_ready = 0;
    int exhausted_cursor_resume_denied = 0;
    int signon_message_cursor_resume_ready = 0;
    int non_exhausted_cursor_resume_allowed = 0;
    int signon_message_cursor_carryover_ready = 0;
    int checkpoint_cursor_carried_over = 0;
    int signon_message_cursor_carried_resume_ready = 0;
    int non_exhausted_carried_cursor_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_ready = 0;
    int carried_cursor_checkpointed = 0;
    int signon_message_cursor_carried_checkpoint_resume_ready = 0;
    int non_exhausted_carried_checkpoint_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_resume_token_ready = 0;
    int carried_checkpoint_resume_token_issued = 0;
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointResumeTokenProbeSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string probe = "disabled";
    int attempts = 0;
    int accepted = 0;
    int rejected = 0;
    std::string last_reject_reason = "<none>";
    std::string requested_target_session;
    std::string requested_cursor_id;
    std::string parsed_source_session;
    std::string parsed_target_session;
    std::string parsed_cursor_id;
    std::string parsed_resume_token;
    std::string parsed_token_purpose;
    std::string parsed_token_state;
    std::string parsed_current_range;
    std::string parsed_next_start_message_index = "<unset>";
    int parsed_remaining_message_count = 0;
    bool parsed_claim_validation_implemented = false;
    int parsed_target_activated = 0;
    std::string parsed_map;
    std::string parsed_name;
    std::string parsed_ruleset;
    int parsed_spawned = 0;
    int signon_ready = 0;
    int bootstrap_delivered = 0;
    int baseline_ready = 0;
    int bootstrap_sequence_completed = 0;
    int signon_catalog_ready = 0;
    int bootstrap_records_staged = 0;
    int signon_template_ready = 0;
    int template_records_delivered = 0;
    int signon_template_coverage_complete = 0;
    int remaining_template_records_delivered = 0;
    int signon_envelope_ready = 0;
    int framed_template_records_delivered = 0;
    int signon_batch_ready = 0;
    int pseudo_packet_batch_delivered = 0;
    int signon_wiremap_ready = 0;
    int wiremapped_batch_delivered = 0;
    int signon_burst_ready = 0;
    int pseudo_wire_burst_delivered = 0;
    int signon_stream_ready = 0;
    int contiguous_stream_delivered = 0;
    int signon_stream_window_ready = 0;
    int windowed_stream_delivered = 0;
    int signon_message_catalog_ready = 0;
    int message_boundaries_cataloged = 0;
    int signon_message_fetch_ready = 0;
    int targeted_message_fetch_delivered = 0;
    int signon_multi_message_fetch_ready = 0;
    int selective_message_set_delivered = 0;
    int signon_message_range_fetch_ready = 0;
    int range_message_set_delivered = 0;
    int signon_message_cursor_ready = 0;
    int message_cursor_checkpointed = 0;
    int signon_message_cursor_advance_ready = 0;
    int cursor_advanced_range_delivered = 0;
    int signon_message_cursor_eof_ready = 0;
    int message_cursor_exhausted = 0;
    int signon_message_cursor_resume_denied_ready = 0;
    int exhausted_cursor_resume_denied = 0;
    int signon_message_cursor_resume_ready = 0;
    int non_exhausted_cursor_resume_allowed = 0;
    int signon_message_cursor_carryover_ready = 0;
    int checkpoint_cursor_carried_over = 0;
    int signon_message_cursor_carried_resume_ready = 0;
    int non_exhausted_carried_cursor_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_ready = 0;
    int carried_cursor_checkpointed = 0;
    int signon_message_cursor_carried_checkpoint_resume_ready = 0;
    int non_exhausted_carried_checkpoint_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_resume_token_ready = 0;
    int carried_checkpoint_resume_token_issued = 0;
    std::string protocol_shape = "disabled";
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointResumeTokenClaimSurfaceSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string bind = "disabled";
    int requested_port = 0;
    int bound_port = 0;
    std::string protocol_shape = "disabled";
    bool carried_checkpoint_resume_token_claim_enabled = false;
    bool requires_issued_resume_token = true;
    bool requires_claimant_activated_session = true;
    std::string claim_purpose = "disabled";
    std::string claim_payload = "disabled";
    int accepted = 0;
    int rejected = 0;
    std::string last_source_session;
    std::string last_issuer_target_session;
    std::string last_claimant_session;
    std::string last_issued_token;
    std::string claim_validation = "disabled";
    std::string auth = "disabled";
    std::string signon = "disabled";
    std::string gameplay_transport = "no";
    int signon_ready = 0;
    int bootstrap_delivered = 0;
    int baseline_ready = 0;
    int bootstrap_sequence_completed = 0;
    int signon_catalog_ready = 0;
    int bootstrap_records_staged = 0;
    int signon_template_ready = 0;
    int template_records_delivered = 0;
    int signon_template_coverage_complete = 0;
    int remaining_template_records_delivered = 0;
    int signon_envelope_ready = 0;
    int framed_template_records_delivered = 0;
    int signon_batch_ready = 0;
    int pseudo_packet_batch_delivered = 0;
    int signon_wiremap_ready = 0;
    int wiremapped_batch_delivered = 0;
    int signon_burst_ready = 0;
    int pseudo_wire_burst_delivered = 0;
    int signon_stream_ready = 0;
    int contiguous_stream_delivered = 0;
    int signon_stream_window_ready = 0;
    int windowed_stream_delivered = 0;
    int signon_message_catalog_ready = 0;
    int message_boundaries_cataloged = 0;
    int signon_message_fetch_ready = 0;
    int targeted_message_fetch_delivered = 0;
    int signon_multi_message_fetch_ready = 0;
    int selective_message_set_delivered = 0;
    int signon_message_range_fetch_ready = 0;
    int range_message_set_delivered = 0;
    int signon_message_cursor_ready = 0;
    int message_cursor_checkpointed = 0;
    int signon_message_cursor_advance_ready = 0;
    int cursor_advanced_range_delivered = 0;
    int signon_message_cursor_eof_ready = 0;
    int message_cursor_exhausted = 0;
    int signon_message_cursor_resume_denied_ready = 0;
    int exhausted_cursor_resume_denied = 0;
    int signon_message_cursor_resume_ready = 0;
    int non_exhausted_cursor_resume_allowed = 0;
    int signon_message_cursor_carryover_ready = 0;
    int checkpoint_cursor_carried_over = 0;
    int signon_message_cursor_carried_resume_ready = 0;
    int non_exhausted_carried_cursor_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_ready = 0;
    int carried_cursor_checkpointed = 0;
    int signon_message_cursor_carried_checkpoint_resume_ready = 0;
    int non_exhausted_carried_checkpoint_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_resume_token_ready = 0;
    int carried_checkpoint_resume_token_issued = 0;
    int signon_message_cursor_carried_checkpoint_resume_token_claim_ready = 0;
    int carried_checkpoint_resume_token_claimed = 0;
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointResumeTokenClaimProbeSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string probe = "disabled";
    int attempts = 0;
    int accepted = 0;
    int rejected = 0;
    std::string last_reject_reason = "<none>";
    std::string requested_claimant_session;
    std::string requested_resume_token;
    std::string parsed_source_session;
    std::string parsed_issuer_target_session;
    std::string parsed_claimant_session;
    std::string parsed_resume_token;
    std::string parsed_claim_status;
    std::string parsed_claim_validation;
    std::string parsed_current_range;
    std::string parsed_next_start_message_index = "<unset>";
    int parsed_remaining_message_count = 0;
    std::string parsed_claim_purpose;
    int parsed_target_activated = 0;
    std::string parsed_map;
    std::string parsed_name;
    std::string parsed_ruleset;
    int parsed_spawned = 0;
    int signon_ready = 0;
    int bootstrap_delivered = 0;
    int baseline_ready = 0;
    int bootstrap_sequence_completed = 0;
    int signon_catalog_ready = 0;
    int bootstrap_records_staged = 0;
    int signon_template_ready = 0;
    int template_records_delivered = 0;
    int signon_template_coverage_complete = 0;
    int remaining_template_records_delivered = 0;
    int signon_envelope_ready = 0;
    int framed_template_records_delivered = 0;
    int signon_batch_ready = 0;
    int pseudo_packet_batch_delivered = 0;
    int signon_wiremap_ready = 0;
    int wiremapped_batch_delivered = 0;
    int signon_burst_ready = 0;
    int pseudo_wire_burst_delivered = 0;
    int signon_stream_ready = 0;
    int contiguous_stream_delivered = 0;
    int signon_stream_window_ready = 0;
    int windowed_stream_delivered = 0;
    int signon_message_catalog_ready = 0;
    int message_boundaries_cataloged = 0;
    int signon_message_fetch_ready = 0;
    int targeted_message_fetch_delivered = 0;
    int signon_multi_message_fetch_ready = 0;
    int selective_message_set_delivered = 0;
    int signon_message_range_fetch_ready = 0;
    int range_message_set_delivered = 0;
    int signon_message_cursor_ready = 0;
    int message_cursor_checkpointed = 0;
    int signon_message_cursor_advance_ready = 0;
    int cursor_advanced_range_delivered = 0;
    int signon_message_cursor_eof_ready = 0;
    int message_cursor_exhausted = 0;
    int signon_message_cursor_resume_denied_ready = 0;
    int exhausted_cursor_resume_denied = 0;
    int signon_message_cursor_resume_ready = 0;
    int non_exhausted_cursor_resume_allowed = 0;
    int signon_message_cursor_carryover_ready = 0;
    int checkpoint_cursor_carried_over = 0;
    int signon_message_cursor_carried_resume_ready = 0;
    int non_exhausted_carried_cursor_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_ready = 0;
    int carried_cursor_checkpointed = 0;
    int signon_message_cursor_carried_checkpoint_resume_ready = 0;
    int non_exhausted_carried_checkpoint_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_resume_token_ready = 0;
    int carried_checkpoint_resume_token_issued = 0;
    int signon_message_cursor_carried_checkpoint_resume_token_claim_ready = 0;
    int carried_checkpoint_resume_token_claimed = 0;
    std::string protocol_shape = "disabled";
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointClaimedRangeSurfaceSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string bind = "disabled";
    int requested_port = 0;
    int bound_port = 0;
    std::string protocol_shape = "disabled";
    bool claimed_range_enabled = false;
    bool requires_claimed_checkpoint_session = true;
    bool requires_issued_resume_token = true;
    std::string fetch_selector = "disabled";
    std::string fetch_payload = "disabled";
    int requested_message_count = 0;
    int accepted = 0;
    int rejected = 0;
    std::string last_source_session;
    std::string last_issuer_target_session;
    std::string last_claimant_session;
    std::string last_claimed_cursor_id;
    std::string last_fetched_message_indices;
    std::string next_start_message_index_after_claimed_fetch = "<unset>";
    int remaining_message_count_after_claimed_fetch = 0;
    std::string auth = "disabled";
    std::string signon = "disabled";
    std::string gameplay_transport = "no";
    int signon_ready = 0;
    int bootstrap_delivered = 0;
    int baseline_ready = 0;
    int bootstrap_sequence_completed = 0;
    int signon_catalog_ready = 0;
    int bootstrap_records_staged = 0;
    int signon_template_ready = 0;
    int template_records_delivered = 0;
    int signon_template_coverage_complete = 0;
    int remaining_template_records_delivered = 0;
    int signon_envelope_ready = 0;
    int framed_template_records_delivered = 0;
    int signon_batch_ready = 0;
    int pseudo_packet_batch_delivered = 0;
    int signon_wiremap_ready = 0;
    int wiremapped_batch_delivered = 0;
    int signon_burst_ready = 0;
    int pseudo_wire_burst_delivered = 0;
    int signon_stream_ready = 0;
    int contiguous_stream_delivered = 0;
    int signon_stream_window_ready = 0;
    int windowed_stream_delivered = 0;
    int signon_message_catalog_ready = 0;
    int message_boundaries_cataloged = 0;
    int signon_message_fetch_ready = 0;
    int targeted_message_fetch_delivered = 0;
    int signon_multi_message_fetch_ready = 0;
    int selective_message_set_delivered = 0;
    int signon_message_range_fetch_ready = 0;
    int range_message_set_delivered = 0;
    int signon_message_cursor_ready = 0;
    int message_cursor_checkpointed = 0;
    int signon_message_cursor_resume_ready = 0;
    int non_exhausted_cursor_resume_allowed = 0;
    int signon_message_cursor_carryover_ready = 0;
    int checkpoint_cursor_carried_over = 0;
    int signon_message_cursor_carried_resume_ready = 0;
    int non_exhausted_carried_cursor_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_ready = 0;
    int carried_cursor_checkpointed = 0;
    int signon_message_cursor_carried_checkpoint_resume_ready = 0;
    int non_exhausted_carried_checkpoint_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_resume_token_ready = 0;
    int carried_checkpoint_resume_token_issued = 0;
    int signon_message_cursor_carried_checkpoint_resume_token_claim_ready = 0;
    int carried_checkpoint_resume_token_claimed = 0;
    int signon_message_cursor_carried_checkpoint_claimed_resume_ready = 0;
    int non_exhausted_claimed_checkpoint_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_claimed_range_ready = 0;
    int claimed_checkpoint_range_delivered = 0;
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointClaimedRangeProbeSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string probe = "disabled";
    int attempts = 0;
    int accepted = 0;
    int rejected = 0;
    std::string last_reject_reason = "<none>";
    std::string requested_claimant_session;
    int requested_message_count = 0;
    std::string requested_cursor_id;
    std::string parsed_source_session;
    std::string parsed_issuer_target_session;
    std::string parsed_claimant_session;
    std::string parsed_cursor_id;
    std::string parsed_fetched_message_indices;
    std::string parsed_semantic_tags;
    int parsed_combined_byte_length = 0;
    std::string parsed_next_start_message_index_after_claimed_fetch = "<unset>";
    int parsed_remaining_message_count_after_claimed_fetch = 0;
    std::string parsed_claim_purpose;
    int parsed_target_activated = 0;
    std::string parsed_map;
    std::string parsed_name;
    std::string parsed_ruleset;
    int parsed_spawned = 0;
    int signon_ready = 0;
    int bootstrap_delivered = 0;
    int baseline_ready = 0;
    int bootstrap_sequence_completed = 0;
    int signon_catalog_ready = 0;
    int bootstrap_records_staged = 0;
    int signon_template_ready = 0;
    int template_records_delivered = 0;
    int signon_template_coverage_complete = 0;
    int remaining_template_records_delivered = 0;
    int signon_envelope_ready = 0;
    int framed_template_records_delivered = 0;
    int signon_batch_ready = 0;
    int pseudo_packet_batch_delivered = 0;
    int signon_wiremap_ready = 0;
    int wiremapped_batch_delivered = 0;
    int signon_burst_ready = 0;
    int pseudo_wire_burst_delivered = 0;
    int signon_stream_ready = 0;
    int contiguous_stream_delivered = 0;
    int signon_stream_window_ready = 0;
    int windowed_stream_delivered = 0;
    int signon_message_catalog_ready = 0;
    int message_boundaries_cataloged = 0;
    int signon_message_fetch_ready = 0;
    int targeted_message_fetch_delivered = 0;
    int signon_multi_message_fetch_ready = 0;
    int selective_message_set_delivered = 0;
    int signon_message_range_fetch_ready = 0;
    int range_message_set_delivered = 0;
    int signon_message_cursor_ready = 0;
    int message_cursor_checkpointed = 0;
    int signon_message_cursor_resume_ready = 0;
    int non_exhausted_cursor_resume_allowed = 0;
    int signon_message_cursor_carryover_ready = 0;
    int checkpoint_cursor_carried_over = 0;
    int signon_message_cursor_carried_resume_ready = 0;
    int non_exhausted_carried_cursor_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_ready = 0;
    int carried_cursor_checkpointed = 0;
    int signon_message_cursor_carried_checkpoint_resume_ready = 0;
    int non_exhausted_carried_checkpoint_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_resume_token_ready = 0;
    int carried_checkpoint_resume_token_issued = 0;
    int signon_message_cursor_carried_checkpoint_resume_token_claim_ready = 0;
    int carried_checkpoint_resume_token_claimed = 0;
    int signon_message_cursor_carried_checkpoint_claimed_resume_ready = 0;
    int non_exhausted_claimed_checkpoint_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_claimed_range_ready = 0;
    int claimed_checkpoint_range_delivered = 0;
    std::string protocol_shape = "disabled";
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointClaimedEofSurfaceSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string bind = "disabled";
    int requested_port = 0;
    int bound_port = 0;
    std::string protocol_shape = "disabled";
    bool claimed_eof_enabled = false;
    bool requires_claimed_range_ready_session = true;
    bool requires_issued_resume_token = true;
    std::string eof_payload = "disabled";
    int accepted = 0;
    int rejected = 0;
    std::string last_source_session;
    std::string last_issuer_target_session;
    std::string last_claimant_session;
    std::string last_claimed_cursor_id;
    bool eof = false;
    bool exhausted = false;
    std::string next_start_message_index = "<unset>";
    int remaining_message_count = 0;
    std::string auth = "disabled";
    std::string signon = "disabled";
    std::string gameplay_transport = "no";
    int signon_ready = 0;
    int bootstrap_delivered = 0;
    int baseline_ready = 0;
    int bootstrap_sequence_completed = 0;
    int signon_catalog_ready = 0;
    int bootstrap_records_staged = 0;
    int signon_template_ready = 0;
    int template_records_delivered = 0;
    int signon_template_coverage_complete = 0;
    int remaining_template_records_delivered = 0;
    int signon_envelope_ready = 0;
    int framed_template_records_delivered = 0;
    int signon_batch_ready = 0;
    int pseudo_packet_batch_delivered = 0;
    int signon_wiremap_ready = 0;
    int wiremapped_batch_delivered = 0;
    int signon_burst_ready = 0;
    int pseudo_wire_burst_delivered = 0;
    int signon_stream_ready = 0;
    int contiguous_stream_delivered = 0;
    int signon_stream_window_ready = 0;
    int windowed_stream_delivered = 0;
    int signon_message_catalog_ready = 0;
    int message_boundaries_cataloged = 0;
    int signon_message_fetch_ready = 0;
    int targeted_message_fetch_delivered = 0;
    int signon_multi_message_fetch_ready = 0;
    int selective_message_set_delivered = 0;
    int signon_message_range_fetch_ready = 0;
    int range_message_set_delivered = 0;
    int signon_message_cursor_ready = 0;
    int message_cursor_checkpointed = 0;
    int signon_message_cursor_resume_ready = 0;
    int non_exhausted_cursor_resume_allowed = 0;
    int signon_message_cursor_carryover_ready = 0;
    int checkpoint_cursor_carried_over = 0;
    int signon_message_cursor_carried_resume_ready = 0;
    int non_exhausted_carried_cursor_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_ready = 0;
    int carried_cursor_checkpointed = 0;
    int signon_message_cursor_carried_checkpoint_resume_ready = 0;
    int non_exhausted_carried_checkpoint_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_resume_token_ready = 0;
    int carried_checkpoint_resume_token_issued = 0;
    int signon_message_cursor_carried_checkpoint_resume_token_claim_ready = 0;
    int carried_checkpoint_resume_token_claimed = 0;
    int signon_message_cursor_carried_checkpoint_claimed_resume_ready = 0;
    int non_exhausted_claimed_checkpoint_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_claimed_range_ready = 0;
    int claimed_checkpoint_range_delivered = 0;
    int signon_message_cursor_carried_checkpoint_claimed_eof_ready = 0;
    int claimed_checkpoint_exhausted = 0;
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointClaimedEofProbeSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string probe = "disabled";
    int attempts = 0;
    int accepted = 0;
    int rejected = 0;
    std::string last_reject_reason = "<none>";
    std::string requested_claimant_session;
    std::string requested_cursor_id;
    std::string parsed_source_session;
    std::string parsed_issuer_target_session;
    std::string parsed_claimant_session;
    std::string parsed_cursor_id;
    bool parsed_eof = false;
    bool parsed_exhausted = false;
    std::string parsed_current_range;
    std::string parsed_next_start_message_index = "<unset>";
    int parsed_remaining_message_count = 0;
    std::string parsed_resume_policy;
    int parsed_target_activated = 0;
    std::string parsed_map;
    std::string parsed_name;
    std::string parsed_ruleset;
    int parsed_spawned = 0;
    int signon_ready = 0;
    int bootstrap_delivered = 0;
    int baseline_ready = 0;
    int bootstrap_sequence_completed = 0;
    int signon_catalog_ready = 0;
    int bootstrap_records_staged = 0;
    int signon_template_ready = 0;
    int template_records_delivered = 0;
    int signon_template_coverage_complete = 0;
    int remaining_template_records_delivered = 0;
    int signon_envelope_ready = 0;
    int framed_template_records_delivered = 0;
    int signon_batch_ready = 0;
    int pseudo_packet_batch_delivered = 0;
    int signon_wiremap_ready = 0;
    int wiremapped_batch_delivered = 0;
    int signon_burst_ready = 0;
    int pseudo_wire_burst_delivered = 0;
    int signon_stream_ready = 0;
    int contiguous_stream_delivered = 0;
    int signon_stream_window_ready = 0;
    int windowed_stream_delivered = 0;
    int signon_message_catalog_ready = 0;
    int message_boundaries_cataloged = 0;
    int signon_message_fetch_ready = 0;
    int targeted_message_fetch_delivered = 0;
    int signon_multi_message_fetch_ready = 0;
    int selective_message_set_delivered = 0;
    int signon_message_range_fetch_ready = 0;
    int range_message_set_delivered = 0;
    int signon_message_cursor_ready = 0;
    int message_cursor_checkpointed = 0;
    int signon_message_cursor_resume_ready = 0;
    int non_exhausted_cursor_resume_allowed = 0;
    int signon_message_cursor_carryover_ready = 0;
    int checkpoint_cursor_carried_over = 0;
    int signon_message_cursor_carried_resume_ready = 0;
    int non_exhausted_carried_cursor_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_ready = 0;
    int carried_cursor_checkpointed = 0;
    int signon_message_cursor_carried_checkpoint_resume_ready = 0;
    int non_exhausted_carried_checkpoint_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_resume_token_ready = 0;
    int carried_checkpoint_resume_token_issued = 0;
    int signon_message_cursor_carried_checkpoint_resume_token_claim_ready = 0;
    int carried_checkpoint_resume_token_claimed = 0;
    int signon_message_cursor_carried_checkpoint_claimed_resume_ready = 0;
    int non_exhausted_claimed_checkpoint_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_claimed_range_ready = 0;
    int claimed_checkpoint_range_delivered = 0;
    int signon_message_cursor_carried_checkpoint_claimed_eof_ready = 0;
    int claimed_checkpoint_exhausted = 0;
    std::string protocol_shape = "disabled";
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointClaimedResumeAllowSurfaceSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string bind = "disabled";
    int requested_port = 0;
    int bound_port = 0;
    std::string protocol_shape = "disabled";
    bool claimed_resume_allow_enabled = false;
    bool requires_claimed_checkpoint_session = true;
    bool requires_issued_resume_token = true;
    bool requires_non_exhausted_claimed_checkpoint = true;
    std::string allow_payload = "disabled";
    int accepted = 0;
    int rejected = 0;
    std::string last_source_session;
    std::string last_issuer_target_session;
    std::string last_claimant_session;
    std::string last_allowed_cursor_id;
    bool resume_allowed = false;
    std::string denial_reason = "<unset>";
    bool eof = false;
    bool exhausted = false;
    std::string next_start_message_index = "<unset>";
    int remaining_message_count = 0;
    std::string auth = "disabled";
    std::string signon = "disabled";
    std::string gameplay_transport = "no";
    int signon_ready = 0;
    int bootstrap_delivered = 0;
    int baseline_ready = 0;
    int bootstrap_sequence_completed = 0;
    int signon_catalog_ready = 0;
    int bootstrap_records_staged = 0;
    int signon_template_ready = 0;
    int template_records_delivered = 0;
    int signon_template_coverage_complete = 0;
    int remaining_template_records_delivered = 0;
    int signon_envelope_ready = 0;
    int framed_template_records_delivered = 0;
    int signon_batch_ready = 0;
    int pseudo_packet_batch_delivered = 0;
    int signon_wiremap_ready = 0;
    int wiremapped_batch_delivered = 0;
    int signon_burst_ready = 0;
    int pseudo_wire_burst_delivered = 0;
    int signon_stream_ready = 0;
    int contiguous_stream_delivered = 0;
    int signon_stream_window_ready = 0;
    int windowed_stream_delivered = 0;
    int signon_message_catalog_ready = 0;
    int message_boundaries_cataloged = 0;
    int signon_message_fetch_ready = 0;
    int targeted_message_fetch_delivered = 0;
    int signon_multi_message_fetch_ready = 0;
    int selective_message_set_delivered = 0;
    int signon_message_range_fetch_ready = 0;
    int range_message_set_delivered = 0;
    int signon_message_cursor_ready = 0;
    int message_cursor_checkpointed = 0;
    int signon_message_cursor_resume_ready = 0;
    int non_exhausted_cursor_resume_allowed = 0;
    int signon_message_cursor_carryover_ready = 0;
    int checkpoint_cursor_carried_over = 0;
    int signon_message_cursor_carried_resume_ready = 0;
    int non_exhausted_carried_cursor_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_ready = 0;
    int carried_cursor_checkpointed = 0;
    int signon_message_cursor_carried_checkpoint_resume_ready = 0;
    int non_exhausted_carried_checkpoint_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_resume_token_ready = 0;
    int carried_checkpoint_resume_token_issued = 0;
    int signon_message_cursor_carried_checkpoint_resume_token_claim_ready = 0;
    int carried_checkpoint_resume_token_claimed = 0;
    int signon_message_cursor_carried_checkpoint_claimed_resume_ready = 0;
    int non_exhausted_claimed_checkpoint_resume_allowed = 0;
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointClaimedResumeAllowProbeSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string probe = "disabled";
    int attempts = 0;
    int accepted = 0;
    int rejected = 0;
    std::string last_reject_reason = "<none>";
    std::string requested_claimant_session;
    std::string requested_cursor_id;
    std::string parsed_source_session;
    std::string parsed_issuer_target_session;
    std::string parsed_claimant_session;
    std::string parsed_cursor_id;
    bool parsed_resume_allowed = false;
    std::string parsed_denial_reason;
    bool parsed_eof = false;
    bool parsed_exhausted = false;
    std::string parsed_current_range;
    std::string parsed_next_start_message_index = "<unset>";
    int parsed_remaining_message_count = 0;
    std::string parsed_resume_policy;
    int parsed_target_activated = 0;
    std::string parsed_map;
    std::string parsed_name;
    std::string parsed_ruleset;
    int parsed_spawned = 0;
    int signon_ready = 0;
    int bootstrap_delivered = 0;
    int baseline_ready = 0;
    int bootstrap_sequence_completed = 0;
    int signon_catalog_ready = 0;
    int bootstrap_records_staged = 0;
    int signon_template_ready = 0;
    int template_records_delivered = 0;
    int signon_template_coverage_complete = 0;
    int remaining_template_records_delivered = 0;
    int signon_envelope_ready = 0;
    int framed_template_records_delivered = 0;
    int signon_batch_ready = 0;
    int pseudo_packet_batch_delivered = 0;
    int signon_wiremap_ready = 0;
    int wiremapped_batch_delivered = 0;
    int signon_burst_ready = 0;
    int pseudo_wire_burst_delivered = 0;
    int signon_stream_ready = 0;
    int contiguous_stream_delivered = 0;
    int signon_stream_window_ready = 0;
    int windowed_stream_delivered = 0;
    int signon_message_catalog_ready = 0;
    int message_boundaries_cataloged = 0;
    int signon_message_fetch_ready = 0;
    int targeted_message_fetch_delivered = 0;
    int signon_multi_message_fetch_ready = 0;
    int selective_message_set_delivered = 0;
    int signon_message_range_fetch_ready = 0;
    int range_message_set_delivered = 0;
    int signon_message_cursor_ready = 0;
    int message_cursor_checkpointed = 0;
    int signon_message_cursor_resume_ready = 0;
    int non_exhausted_cursor_resume_allowed = 0;
    int signon_message_cursor_carryover_ready = 0;
    int checkpoint_cursor_carried_over = 0;
    int signon_message_cursor_carried_resume_ready = 0;
    int non_exhausted_carried_cursor_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_ready = 0;
    int carried_cursor_checkpointed = 0;
    int signon_message_cursor_carried_checkpoint_resume_ready = 0;
    int non_exhausted_carried_checkpoint_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_resume_token_ready = 0;
    int carried_checkpoint_resume_token_issued = 0;
    int signon_message_cursor_carried_checkpoint_resume_token_claim_ready = 0;
    int carried_checkpoint_resume_token_claimed = 0;
    int signon_message_cursor_carried_checkpoint_claimed_resume_ready = 0;
    int non_exhausted_claimed_checkpoint_resume_allowed = 0;
    std::string protocol_shape = "disabled";
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointBridgeSurfaceSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string bind = "disabled";
    int requested_port = 0;
    int bound_port = 0;
    std::string protocol_shape = "disabled";
    bool claimed_checkpoint_bridge_enabled = false;
    bool requires_claimed_resume_ready_session = true;
    bool requires_issued_resume_token = true;
    std::string bridge_payload = "disabled";
    int accepted = 0;
    int rejected = 0;
    std::string last_source_session;
    std::string last_issuer_target_session;
    std::string last_claimant_session;
    std::string last_source_claimed_cursor_id;
    std::string last_claimant_checkpoint_cursor_id;
    int current_start_message_index = -1;
    int current_message_count = 0;
    std::string next_start_message_index = "<unset>";
    int remaining_message_count = 0;
    std::string bridge_policy = "<unset>";
    std::string auth = "disabled";
    std::string signon = "disabled";
    std::string gameplay_transport = "no";
    int signon_ready = 0;
    int bootstrap_delivered = 0;
    int baseline_ready = 0;
    int bootstrap_sequence_completed = 0;
    int signon_catalog_ready = 0;
    int bootstrap_records_staged = 0;
    int signon_template_ready = 0;
    int template_records_delivered = 0;
    int signon_template_coverage_complete = 0;
    int remaining_template_records_delivered = 0;
    int signon_envelope_ready = 0;
    int framed_template_records_delivered = 0;
    int signon_batch_ready = 0;
    int pseudo_packet_batch_delivered = 0;
    int signon_wiremap_ready = 0;
    int wiremapped_batch_delivered = 0;
    int signon_burst_ready = 0;
    int pseudo_wire_burst_delivered = 0;
    int signon_stream_ready = 0;
    int contiguous_stream_delivered = 0;
    int signon_stream_window_ready = 0;
    int windowed_stream_delivered = 0;
    int signon_message_catalog_ready = 0;
    int message_boundaries_cataloged = 0;
    int signon_message_fetch_ready = 0;
    int targeted_message_fetch_delivered = 0;
    int signon_multi_message_fetch_ready = 0;
    int selective_message_set_delivered = 0;
    int signon_message_range_fetch_ready = 0;
    int range_message_set_delivered = 0;
    int signon_message_cursor_ready = 0;
    int message_cursor_checkpointed = 0;
    int signon_message_cursor_resume_ready = 0;
    int non_exhausted_cursor_resume_allowed = 0;
    int signon_message_cursor_carryover_ready = 0;
    int checkpoint_cursor_carried_over = 0;
    int signon_message_cursor_carried_resume_ready = 0;
    int non_exhausted_carried_cursor_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_ready = 0;
    int carried_cursor_checkpointed = 0;
    int signon_message_cursor_carried_checkpoint_resume_ready = 0;
    int non_exhausted_carried_checkpoint_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_resume_token_ready = 0;
    int carried_checkpoint_resume_token_issued = 0;
    int signon_message_cursor_carried_checkpoint_resume_token_claim_ready = 0;
    int carried_checkpoint_resume_token_claimed = 0;
    int signon_message_cursor_carried_checkpoint_claimed_resume_ready = 0;
    int non_exhausted_claimed_checkpoint_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_bridge_ready = 0;
    int claimed_checkpoint_bridge_materialized = 0;
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointBridgeProbeSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string probe = "disabled";
    int attempts = 0;
    int accepted = 0;
    int rejected = 0;
    std::string last_reject_reason = "<none>";
    std::string requested_claimant_session;
    std::string requested_cursor_id;
    std::string parsed_source_session;
    std::string parsed_issuer_target_session;
    std::string parsed_claimant_session;
    std::string parsed_source_claimed_cursor_id;
    std::string parsed_claimant_checkpoint_cursor_id;
    std::string parsed_current_range;
    std::string parsed_next_start_message_index = "<unset>";
    int parsed_remaining_message_count = 0;
    std::string parsed_bridge_policy;
    int parsed_target_activated = 0;
    std::string parsed_map;
    std::string parsed_name;
    std::string parsed_ruleset;
    int parsed_spawned = 0;
    int signon_ready = 0;
    int bootstrap_delivered = 0;
    int baseline_ready = 0;
    int bootstrap_sequence_completed = 0;
    int signon_catalog_ready = 0;
    int bootstrap_records_staged = 0;
    int signon_template_ready = 0;
    int template_records_delivered = 0;
    int signon_template_coverage_complete = 0;
    int remaining_template_records_delivered = 0;
    int signon_envelope_ready = 0;
    int framed_template_records_delivered = 0;
    int signon_batch_ready = 0;
    int pseudo_packet_batch_delivered = 0;
    int signon_wiremap_ready = 0;
    int wiremapped_batch_delivered = 0;
    int signon_burst_ready = 0;
    int pseudo_wire_burst_delivered = 0;
    int signon_stream_ready = 0;
    int contiguous_stream_delivered = 0;
    int signon_stream_window_ready = 0;
    int windowed_stream_delivered = 0;
    int signon_message_catalog_ready = 0;
    int message_boundaries_cataloged = 0;
    int signon_message_fetch_ready = 0;
    int targeted_message_fetch_delivered = 0;
    int signon_multi_message_fetch_ready = 0;
    int selective_message_set_delivered = 0;
    int signon_message_range_fetch_ready = 0;
    int range_message_set_delivered = 0;
    int signon_message_cursor_ready = 0;
    int message_cursor_checkpointed = 0;
    int signon_message_cursor_resume_ready = 0;
    int non_exhausted_cursor_resume_allowed = 0;
    int signon_message_cursor_carryover_ready = 0;
    int checkpoint_cursor_carried_over = 0;
    int signon_message_cursor_carried_resume_ready = 0;
    int non_exhausted_carried_cursor_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_ready = 0;
    int carried_cursor_checkpointed = 0;
    int signon_message_cursor_carried_checkpoint_resume_ready = 0;
    int non_exhausted_carried_checkpoint_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_resume_token_ready = 0;
    int carried_checkpoint_resume_token_issued = 0;
    int signon_message_cursor_carried_checkpoint_resume_token_claim_ready = 0;
    int carried_checkpoint_resume_token_claimed = 0;
    int signon_message_cursor_carried_checkpoint_claimed_resume_ready = 0;
    int non_exhausted_claimed_checkpoint_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_bridge_ready = 0;
    int claimed_checkpoint_bridge_materialized = 0;
    std::string protocol_shape = "disabled";
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointResumeAllowSurfaceSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string bind = "disabled";
    int requested_port = 0;
    int bound_port = 0;
    std::string protocol_shape = "disabled";
    bool claimed_checkpoint_resume_allow_enabled = false;
    bool requires_claimed_checkpoint_bridge_ready_session = true;
    std::string allow_payload = "disabled";
    int accepted = 0;
    int rejected = 0;
    std::string last_source_session;
    std::string last_issuer_target_session;
    std::string last_claimant_session;
    std::string last_allowed_cursor_id;
    bool resume_allowed = false;
    std::string denial_reason = "<unset>";
    bool eof = false;
    bool exhausted = false;
    int current_start_message_index = -1;
    int current_message_count = 0;
    std::string next_start_message_index = "<unset>";
    int remaining_message_count = 0;
    std::string auth = "disabled";
    std::string signon = "disabled";
    std::string gameplay_transport = "no";
    int signon_ready = 0;
    int bootstrap_delivered = 0;
    int baseline_ready = 0;
    int bootstrap_sequence_completed = 0;
    int signon_catalog_ready = 0;
    int bootstrap_records_staged = 0;
    int signon_template_ready = 0;
    int template_records_delivered = 0;
    int signon_template_coverage_complete = 0;
    int remaining_template_records_delivered = 0;
    int signon_envelope_ready = 0;
    int framed_template_records_delivered = 0;
    int signon_batch_ready = 0;
    int pseudo_packet_batch_delivered = 0;
    int signon_wiremap_ready = 0;
    int wiremapped_batch_delivered = 0;
    int signon_burst_ready = 0;
    int pseudo_wire_burst_delivered = 0;
    int signon_stream_ready = 0;
    int contiguous_stream_delivered = 0;
    int signon_stream_window_ready = 0;
    int windowed_stream_delivered = 0;
    int signon_message_catalog_ready = 0;
    int message_boundaries_cataloged = 0;
    int signon_message_fetch_ready = 0;
    int targeted_message_fetch_delivered = 0;
    int signon_multi_message_fetch_ready = 0;
    int selective_message_set_delivered = 0;
    int signon_message_range_fetch_ready = 0;
    int range_message_set_delivered = 0;
    int signon_message_cursor_ready = 0;
    int message_cursor_checkpointed = 0;
    int signon_message_cursor_resume_ready = 0;
    int non_exhausted_cursor_resume_allowed = 0;
    int signon_message_cursor_carryover_ready = 0;
    int checkpoint_cursor_carried_over = 0;
    int signon_message_cursor_carried_resume_ready = 0;
    int non_exhausted_carried_cursor_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_ready = 0;
    int carried_cursor_checkpointed = 0;
    int signon_message_cursor_carried_checkpoint_resume_ready = 0;
    int non_exhausted_carried_checkpoint_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_resume_token_ready = 0;
    int carried_checkpoint_resume_token_issued = 0;
    int signon_message_cursor_carried_checkpoint_resume_token_claim_ready = 0;
    int carried_checkpoint_resume_token_claimed = 0;
    int signon_message_cursor_carried_checkpoint_claimed_resume_ready = 0;
    int non_exhausted_claimed_checkpoint_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_bridge_ready = 0;
    int claimed_checkpoint_bridge_materialized = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_resume_ready = 0;
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointResumeAllowProbeSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string probe = "disabled";
    int attempts = 0;
    int accepted = 0;
    int rejected = 0;
    std::string last_reject_reason = "<none>";
    std::string requested_claimant_session;
    std::string requested_cursor_id;
    std::string parsed_source_session;
    std::string parsed_issuer_target_session;
    std::string parsed_claimant_session;
    std::string parsed_cursor_id;
    bool parsed_resume_allowed = false;
    std::string parsed_denial_reason;
    bool parsed_eof = false;
    bool parsed_exhausted = false;
    std::string parsed_current_range;
    std::string parsed_next_start_message_index = "<unset>";
    int parsed_remaining_message_count = 0;
    std::string parsed_resume_policy;
    int parsed_target_activated = 0;
    std::string parsed_map;
    std::string parsed_name;
    std::string parsed_ruleset;
    int parsed_spawned = 0;
    int signon_ready = 0;
    int bootstrap_delivered = 0;
    int baseline_ready = 0;
    int bootstrap_sequence_completed = 0;
    int signon_catalog_ready = 0;
    int bootstrap_records_staged = 0;
    int signon_template_ready = 0;
    int template_records_delivered = 0;
    int signon_template_coverage_complete = 0;
    int remaining_template_records_delivered = 0;
    int signon_envelope_ready = 0;
    int framed_template_records_delivered = 0;
    int signon_batch_ready = 0;
    int pseudo_packet_batch_delivered = 0;
    int signon_wiremap_ready = 0;
    int wiremapped_batch_delivered = 0;
    int signon_burst_ready = 0;
    int pseudo_wire_burst_delivered = 0;
    int signon_stream_ready = 0;
    int contiguous_stream_delivered = 0;
    int signon_stream_window_ready = 0;
    int windowed_stream_delivered = 0;
    int signon_message_catalog_ready = 0;
    int message_boundaries_cataloged = 0;
    int signon_message_fetch_ready = 0;
    int targeted_message_fetch_delivered = 0;
    int signon_multi_message_fetch_ready = 0;
    int selective_message_set_delivered = 0;
    int signon_message_range_fetch_ready = 0;
    int range_message_set_delivered = 0;
    int signon_message_cursor_ready = 0;
    int message_cursor_checkpointed = 0;
    int signon_message_cursor_resume_ready = 0;
    int non_exhausted_cursor_resume_allowed = 0;
    int signon_message_cursor_carryover_ready = 0;
    int checkpoint_cursor_carried_over = 0;
    int signon_message_cursor_carried_resume_ready = 0;
    int non_exhausted_carried_cursor_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_ready = 0;
    int carried_cursor_checkpointed = 0;
    int signon_message_cursor_carried_checkpoint_resume_ready = 0;
    int non_exhausted_carried_checkpoint_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_resume_token_ready = 0;
    int carried_checkpoint_resume_token_issued = 0;
    int signon_message_cursor_carried_checkpoint_resume_token_claim_ready = 0;
    int carried_checkpoint_resume_token_claimed = 0;
    int signon_message_cursor_carried_checkpoint_claimed_resume_ready = 0;
    int non_exhausted_claimed_checkpoint_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_bridge_ready = 0;
    int claimed_checkpoint_bridge_materialized = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_resume_ready = 0;
    std::string protocol_shape = "disabled";
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointAdvanceSurfaceSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string bind = "disabled";
    int requested_port = 0;
    int bound_port = 0;
    std::string protocol_shape = "disabled";
    bool claimed_checkpoint_advance_enabled = false;
    bool requires_claimed_checkpoint_bridge_ready_session = true;
    std::string fetch_selector = "disabled";
    std::string fetch_payload = "disabled";
    int requested_message_count = 0;
    int accepted = 0;
    int rejected = 0;
    std::string last_source_session;
    std::string last_issuer_target_session;
    std::string last_claimant_session;
    std::string last_checkpoint_cursor_id;
    std::string last_fetched_message_indices;
    std::string next_start_message_index_after_advance = "<unset>";
    int remaining_message_count_after_advance = 0;
    std::string auth = "disabled";
    std::string signon = "disabled";
    std::string gameplay_transport = "no";
    int signon_ready = 0;
    int bootstrap_delivered = 0;
    int baseline_ready = 0;
    int bootstrap_sequence_completed = 0;
    int signon_catalog_ready = 0;
    int bootstrap_records_staged = 0;
    int signon_template_ready = 0;
    int template_records_delivered = 0;
    int signon_template_coverage_complete = 0;
    int remaining_template_records_delivered = 0;
    int signon_envelope_ready = 0;
    int framed_template_records_delivered = 0;
    int signon_batch_ready = 0;
    int pseudo_packet_batch_delivered = 0;
    int signon_wiremap_ready = 0;
    int wiremapped_batch_delivered = 0;
    int signon_burst_ready = 0;
    int pseudo_wire_burst_delivered = 0;
    int signon_stream_ready = 0;
    int contiguous_stream_delivered = 0;
    int signon_stream_window_ready = 0;
    int windowed_stream_delivered = 0;
    int signon_message_catalog_ready = 0;
    int message_boundaries_cataloged = 0;
    int signon_message_fetch_ready = 0;
    int targeted_message_fetch_delivered = 0;
    int signon_multi_message_fetch_ready = 0;
    int selective_message_set_delivered = 0;
    int signon_message_range_fetch_ready = 0;
    int range_message_set_delivered = 0;
    int signon_message_cursor_ready = 0;
    int message_cursor_checkpointed = 0;
    int signon_message_cursor_resume_ready = 0;
    int non_exhausted_cursor_resume_allowed = 0;
    int signon_message_cursor_carryover_ready = 0;
    int checkpoint_cursor_carried_over = 0;
    int signon_message_cursor_carried_resume_ready = 0;
    int non_exhausted_carried_cursor_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_ready = 0;
    int carried_cursor_checkpointed = 0;
    int signon_message_cursor_carried_checkpoint_resume_ready = 0;
    int non_exhausted_carried_checkpoint_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_resume_token_ready = 0;
    int carried_checkpoint_resume_token_issued = 0;
    int signon_message_cursor_carried_checkpoint_resume_token_claim_ready = 0;
    int carried_checkpoint_resume_token_claimed = 0;
    int signon_message_cursor_carried_checkpoint_claimed_resume_ready = 0;
    int non_exhausted_claimed_checkpoint_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_bridge_ready = 0;
    int claimed_checkpoint_bridge_materialized = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_resume_ready = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_advance_ready = 0;
    int claimed_checkpoint_advanced_range_delivered = 0;
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointAdvanceProbeSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string probe = "disabled";
    int attempts = 0;
    int accepted = 0;
    int rejected = 0;
    std::string last_reject_reason = "<none>";
    std::string requested_claimant_session;
    int requested_message_count = 0;
    std::string requested_cursor_id;
    std::string parsed_source_session;
    std::string parsed_issuer_target_session;
    std::string parsed_claimant_session;
    std::string parsed_cursor_id;
    std::string parsed_fetched_message_indices;
    std::string parsed_semantic_tags;
    int parsed_combined_byte_length = 0;
    std::string parsed_next_start_message_index_after_advance = "<unset>";
    int parsed_remaining_message_count_after_advance = 0;
    std::string parsed_bridge_policy;
    int parsed_target_activated = 0;
    std::string parsed_map;
    std::string parsed_name;
    std::string parsed_ruleset;
    int parsed_spawned = 0;
    int signon_ready = 0;
    int bootstrap_delivered = 0;
    int baseline_ready = 0;
    int bootstrap_sequence_completed = 0;
    int signon_catalog_ready = 0;
    int bootstrap_records_staged = 0;
    int signon_template_ready = 0;
    int template_records_delivered = 0;
    int signon_template_coverage_complete = 0;
    int remaining_template_records_delivered = 0;
    int signon_envelope_ready = 0;
    int framed_template_records_delivered = 0;
    int signon_batch_ready = 0;
    int pseudo_packet_batch_delivered = 0;
    int signon_wiremap_ready = 0;
    int wiremapped_batch_delivered = 0;
    int signon_burst_ready = 0;
    int pseudo_wire_burst_delivered = 0;
    int signon_stream_ready = 0;
    int contiguous_stream_delivered = 0;
    int signon_stream_window_ready = 0;
    int windowed_stream_delivered = 0;
    int signon_message_catalog_ready = 0;
    int message_boundaries_cataloged = 0;
    int signon_message_fetch_ready = 0;
    int targeted_message_fetch_delivered = 0;
    int signon_multi_message_fetch_ready = 0;
    int selective_message_set_delivered = 0;
    int signon_message_range_fetch_ready = 0;
    int range_message_set_delivered = 0;
    int signon_message_cursor_ready = 0;
    int message_cursor_checkpointed = 0;
    int signon_message_cursor_resume_ready = 0;
    int non_exhausted_cursor_resume_allowed = 0;
    int signon_message_cursor_carryover_ready = 0;
    int checkpoint_cursor_carried_over = 0;
    int signon_message_cursor_carried_resume_ready = 0;
    int non_exhausted_carried_cursor_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_ready = 0;
    int carried_cursor_checkpointed = 0;
    int signon_message_cursor_carried_checkpoint_resume_ready = 0;
    int non_exhausted_carried_checkpoint_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_resume_token_ready = 0;
    int carried_checkpoint_resume_token_issued = 0;
    int signon_message_cursor_carried_checkpoint_resume_token_claim_ready = 0;
    int carried_checkpoint_resume_token_claimed = 0;
    int signon_message_cursor_carried_checkpoint_claimed_resume_ready = 0;
    int non_exhausted_claimed_checkpoint_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_bridge_ready = 0;
    int claimed_checkpoint_bridge_materialized = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_advance_ready = 0;
    int claimed_checkpoint_advanced_range_delivered = 0;
    std::string protocol_shape = "disabled";
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointEofSurfaceSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string bind = "disabled";
    int requested_port = 0;
    int bound_port = 0;
    std::string protocol_shape = "disabled";
    bool claimed_checkpoint_eof_enabled = false;
    bool requires_claimed_checkpoint_advance_ready_session = true;
    std::string eof_payload = "disabled";
    int accepted = 0;
    int rejected = 0;
    std::string last_source_session;
    std::string last_issuer_target_session;
    std::string last_claimant_session;
    std::string last_checkpoint_cursor_id;
    bool eof = false;
    bool exhausted = false;
    int current_start_message_index = -1;
    int current_message_count = 0;
    std::string next_start_message_index = "<unset>";
    int remaining_message_count = 0;
    std::string auth = "disabled";
    std::string signon = "disabled";
    std::string gameplay_transport = "no";
    int signon_ready = 0;
    int bootstrap_delivered = 0;
    int baseline_ready = 0;
    int bootstrap_sequence_completed = 0;
    int signon_catalog_ready = 0;
    int bootstrap_records_staged = 0;
    int signon_template_ready = 0;
    int template_records_delivered = 0;
    int signon_template_coverage_complete = 0;
    int remaining_template_records_delivered = 0;
    int signon_envelope_ready = 0;
    int framed_template_records_delivered = 0;
    int signon_batch_ready = 0;
    int pseudo_packet_batch_delivered = 0;
    int signon_wiremap_ready = 0;
    int wiremapped_batch_delivered = 0;
    int signon_burst_ready = 0;
    int pseudo_wire_burst_delivered = 0;
    int signon_stream_ready = 0;
    int contiguous_stream_delivered = 0;
    int signon_stream_window_ready = 0;
    int windowed_stream_delivered = 0;
    int signon_message_catalog_ready = 0;
    int message_boundaries_cataloged = 0;
    int signon_message_fetch_ready = 0;
    int targeted_message_fetch_delivered = 0;
    int signon_multi_message_fetch_ready = 0;
    int selective_message_set_delivered = 0;
    int signon_message_range_fetch_ready = 0;
    int range_message_set_delivered = 0;
    int signon_message_cursor_ready = 0;
    int message_cursor_checkpointed = 0;
    int signon_message_cursor_resume_ready = 0;
    int non_exhausted_cursor_resume_allowed = 0;
    int signon_message_cursor_carryover_ready = 0;
    int checkpoint_cursor_carried_over = 0;
    int signon_message_cursor_carried_resume_ready = 0;
    int non_exhausted_carried_cursor_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_ready = 0;
    int carried_cursor_checkpointed = 0;
    int signon_message_cursor_carried_checkpoint_resume_ready = 0;
    int non_exhausted_carried_checkpoint_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_resume_token_ready = 0;
    int carried_checkpoint_resume_token_issued = 0;
    int signon_message_cursor_carried_checkpoint_resume_token_claim_ready = 0;
    int carried_checkpoint_resume_token_claimed = 0;
    int signon_message_cursor_carried_checkpoint_claimed_resume_ready = 0;
    int non_exhausted_claimed_checkpoint_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_bridge_ready = 0;
    int claimed_checkpoint_bridge_materialized = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_advance_ready = 0;
    int claimed_checkpoint_advanced_range_delivered = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_eof_ready = 0;
    int claimed_checkpoint_bridge_exhausted = 0;
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointEofProbeSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string probe = "disabled";
    int attempts = 0;
    int accepted = 0;
    int rejected = 0;
    std::string last_reject_reason = "<none>";
    std::string requested_claimant_session;
    std::string requested_cursor_id;
    std::string parsed_source_session;
    std::string parsed_issuer_target_session;
    std::string parsed_claimant_session;
    std::string parsed_cursor_id;
    bool parsed_eof = false;
    bool parsed_exhausted = false;
    std::string parsed_current_range;
    std::string parsed_next_start_message_index = "<unset>";
    int parsed_remaining_message_count = 0;
    std::string parsed_resume_policy;
    int parsed_target_activated = 0;
    std::string parsed_map;
    std::string parsed_name;
    std::string parsed_ruleset;
    int parsed_spawned = 0;
    int signon_ready = 0;
    int bootstrap_delivered = 0;
    int baseline_ready = 0;
    int bootstrap_sequence_completed = 0;
    int signon_catalog_ready = 0;
    int bootstrap_records_staged = 0;
    int signon_template_ready = 0;
    int template_records_delivered = 0;
    int signon_template_coverage_complete = 0;
    int remaining_template_records_delivered = 0;
    int signon_envelope_ready = 0;
    int framed_template_records_delivered = 0;
    int signon_batch_ready = 0;
    int pseudo_packet_batch_delivered = 0;
    int signon_wiremap_ready = 0;
    int wiremapped_batch_delivered = 0;
    int signon_burst_ready = 0;
    int pseudo_wire_burst_delivered = 0;
    int signon_stream_ready = 0;
    int contiguous_stream_delivered = 0;
    int signon_stream_window_ready = 0;
    int windowed_stream_delivered = 0;
    int signon_message_catalog_ready = 0;
    int message_boundaries_cataloged = 0;
    int signon_message_fetch_ready = 0;
    int targeted_message_fetch_delivered = 0;
    int signon_multi_message_fetch_ready = 0;
    int selective_message_set_delivered = 0;
    int signon_message_range_fetch_ready = 0;
    int range_message_set_delivered = 0;
    int signon_message_cursor_ready = 0;
    int message_cursor_checkpointed = 0;
    int signon_message_cursor_resume_ready = 0;
    int non_exhausted_cursor_resume_allowed = 0;
    int signon_message_cursor_carryover_ready = 0;
    int checkpoint_cursor_carried_over = 0;
    int signon_message_cursor_carried_resume_ready = 0;
    int non_exhausted_carried_cursor_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_ready = 0;
    int carried_cursor_checkpointed = 0;
    int signon_message_cursor_carried_checkpoint_resume_ready = 0;
    int non_exhausted_carried_checkpoint_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_resume_token_ready = 0;
    int carried_checkpoint_resume_token_issued = 0;
    int signon_message_cursor_carried_checkpoint_resume_token_claim_ready = 0;
    int carried_checkpoint_resume_token_claimed = 0;
    int signon_message_cursor_carried_checkpoint_claimed_resume_ready = 0;
    int non_exhausted_claimed_checkpoint_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_bridge_ready = 0;
    int claimed_checkpoint_bridge_materialized = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_advance_ready = 0;
    int claimed_checkpoint_advanced_range_delivered = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_eof_ready = 0;
    int claimed_checkpoint_bridge_exhausted = 0;
    std::string protocol_shape = "disabled";
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointResumeDenialSurfaceSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string bind = "disabled";
    int requested_port = 0;
    int bound_port = 0;
    std::string protocol_shape = "disabled";
    bool claimed_checkpoint_resume_denial_enabled = false;
    bool requires_claimed_checkpoint_eof_ready_session = true;
    std::string denial_payload = "disabled";
    int accepted = 0;
    int rejected = 0;
    std::string last_source_session;
    std::string last_issuer_target_session;
    std::string last_claimant_session;
    std::string last_denied_cursor_id;
    bool resume_allowed = false;
    std::string denial_reason = "<unset>";
    bool eof = false;
    bool exhausted = false;
    int current_start_message_index = -1;
    int current_message_count = 0;
    std::string next_start_message_index = "<unset>";
    int remaining_message_count = 0;
    std::string auth = "disabled";
    std::string signon = "disabled";
    std::string gameplay_transport = "no";
    int signon_ready = 0;
    int bootstrap_delivered = 0;
    int baseline_ready = 0;
    int bootstrap_sequence_completed = 0;
    int signon_catalog_ready = 0;
    int bootstrap_records_staged = 0;
    int signon_template_ready = 0;
    int template_records_delivered = 0;
    int signon_template_coverage_complete = 0;
    int remaining_template_records_delivered = 0;
    int signon_envelope_ready = 0;
    int framed_template_records_delivered = 0;
    int signon_batch_ready = 0;
    int pseudo_packet_batch_delivered = 0;
    int signon_wiremap_ready = 0;
    int wiremapped_batch_delivered = 0;
    int signon_burst_ready = 0;
    int pseudo_wire_burst_delivered = 0;
    int signon_stream_ready = 0;
    int contiguous_stream_delivered = 0;
    int signon_stream_window_ready = 0;
    int windowed_stream_delivered = 0;
    int signon_message_catalog_ready = 0;
    int message_boundaries_cataloged = 0;
    int signon_message_fetch_ready = 0;
    int targeted_message_fetch_delivered = 0;
    int signon_multi_message_fetch_ready = 0;
    int selective_message_set_delivered = 0;
    int signon_message_range_fetch_ready = 0;
    int range_message_set_delivered = 0;
    int signon_message_cursor_ready = 0;
    int message_cursor_checkpointed = 0;
    int signon_message_cursor_resume_ready = 0;
    int non_exhausted_cursor_resume_allowed = 0;
    int signon_message_cursor_carryover_ready = 0;
    int checkpoint_cursor_carried_over = 0;
    int signon_message_cursor_carried_resume_ready = 0;
    int non_exhausted_carried_cursor_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_ready = 0;
    int carried_cursor_checkpointed = 0;
    int signon_message_cursor_carried_checkpoint_resume_ready = 0;
    int non_exhausted_carried_checkpoint_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_resume_token_ready = 0;
    int carried_checkpoint_resume_token_issued = 0;
    int signon_message_cursor_carried_checkpoint_resume_token_claim_ready = 0;
    int carried_checkpoint_resume_token_claimed = 0;
    int signon_message_cursor_carried_checkpoint_claimed_resume_ready = 0;
    int non_exhausted_claimed_checkpoint_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_bridge_ready = 0;
    int claimed_checkpoint_bridge_materialized = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_advance_ready = 0;
    int claimed_checkpoint_advanced_range_delivered = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_eof_ready = 0;
    int claimed_checkpoint_bridge_exhausted = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_resume_denied_ready = 0;
    int claimed_checkpoint_bridge_resume_denied = 0;
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointResumeDenialProbeSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string probe = "disabled";
    int attempts = 0;
    int accepted = 0;
    int rejected = 0;
    std::string last_reject_reason = "<none>";
    std::string requested_claimant_session;
    std::string requested_cursor_id;
    std::string parsed_source_session;
    std::string parsed_issuer_target_session;
    std::string parsed_claimant_session;
    std::string parsed_cursor_id;
    bool parsed_resume_allowed = false;
    std::string parsed_denial_reason;
    bool parsed_eof = false;
    bool parsed_exhausted = false;
    std::string parsed_current_range;
    std::string parsed_next_start_message_index = "<unset>";
    int parsed_remaining_message_count = 0;
    std::string parsed_resume_policy;
    int parsed_target_activated = 0;
    std::string parsed_map;
    std::string parsed_name;
    std::string parsed_ruleset;
    int parsed_spawned = 0;
    int signon_ready = 0;
    int bootstrap_delivered = 0;
    int baseline_ready = 0;
    int bootstrap_sequence_completed = 0;
    int signon_catalog_ready = 0;
    int bootstrap_records_staged = 0;
    int signon_template_ready = 0;
    int template_records_delivered = 0;
    int signon_template_coverage_complete = 0;
    int remaining_template_records_delivered = 0;
    int signon_envelope_ready = 0;
    int framed_template_records_delivered = 0;
    int signon_batch_ready = 0;
    int pseudo_packet_batch_delivered = 0;
    int signon_wiremap_ready = 0;
    int wiremapped_batch_delivered = 0;
    int signon_burst_ready = 0;
    int pseudo_wire_burst_delivered = 0;
    int signon_stream_ready = 0;
    int contiguous_stream_delivered = 0;
    int signon_stream_window_ready = 0;
    int windowed_stream_delivered = 0;
    int signon_message_catalog_ready = 0;
    int message_boundaries_cataloged = 0;
    int signon_message_fetch_ready = 0;
    int targeted_message_fetch_delivered = 0;
    int signon_multi_message_fetch_ready = 0;
    int selective_message_set_delivered = 0;
    int signon_message_range_fetch_ready = 0;
    int range_message_set_delivered = 0;
    int signon_message_cursor_ready = 0;
    int message_cursor_checkpointed = 0;
    int signon_message_cursor_resume_ready = 0;
    int non_exhausted_cursor_resume_allowed = 0;
    int signon_message_cursor_carryover_ready = 0;
    int checkpoint_cursor_carried_over = 0;
    int signon_message_cursor_carried_resume_ready = 0;
    int non_exhausted_carried_cursor_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_ready = 0;
    int carried_cursor_checkpointed = 0;
    int signon_message_cursor_carried_checkpoint_resume_ready = 0;
    int non_exhausted_carried_checkpoint_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_resume_token_ready = 0;
    int carried_checkpoint_resume_token_issued = 0;
    int signon_message_cursor_carried_checkpoint_resume_token_claim_ready = 0;
    int carried_checkpoint_resume_token_claimed = 0;
    int signon_message_cursor_carried_checkpoint_claimed_resume_ready = 0;
    int non_exhausted_claimed_checkpoint_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_bridge_ready = 0;
    int claimed_checkpoint_bridge_materialized = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_advance_ready = 0;
    int claimed_checkpoint_advanced_range_delivered = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_eof_ready = 0;
    int claimed_checkpoint_bridge_exhausted = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_resume_denied_ready = 0;
    int claimed_checkpoint_bridge_resume_denied = 0;
    std::string protocol_shape = "disabled";
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointResumeTokenSurfaceSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string bind = "disabled";
    int requested_port = 0;
    int bound_port = 0;
    std::string protocol_shape = "disabled";
    bool claimed_checkpoint_resume_token_enabled = false;
    bool requires_claimed_checkpoint_resume_ready_session = true;
    std::string token_purpose = "disabled";
    std::string token_payload = "disabled";
    int accepted = 0;
    int rejected = 0;
    std::string last_source_session;
    std::string last_issuer_target_session;
    std::string last_claimant_session;
    std::string last_checkpoint_cursor_id;
    std::string last_issued_token;
    std::string claim_validation_implemented = "no";
    std::string auth = "disabled";
    std::string signon = "disabled";
    std::string gameplay_transport = "no";
    int signon_ready = 0;
    int bootstrap_delivered = 0;
    int baseline_ready = 0;
    int bootstrap_sequence_completed = 0;
    int signon_catalog_ready = 0;
    int bootstrap_records_staged = 0;
    int signon_template_ready = 0;
    int template_records_delivered = 0;
    int signon_template_coverage_complete = 0;
    int remaining_template_records_delivered = 0;
    int signon_envelope_ready = 0;
    int framed_template_records_delivered = 0;
    int signon_batch_ready = 0;
    int pseudo_packet_batch_delivered = 0;
    int signon_wiremap_ready = 0;
    int wiremapped_batch_delivered = 0;
    int signon_burst_ready = 0;
    int pseudo_wire_burst_delivered = 0;
    int signon_stream_ready = 0;
    int contiguous_stream_delivered = 0;
    int signon_stream_window_ready = 0;
    int windowed_stream_delivered = 0;
    int signon_message_catalog_ready = 0;
    int message_boundaries_cataloged = 0;
    int signon_message_fetch_ready = 0;
    int targeted_message_fetch_delivered = 0;
    int signon_multi_message_fetch_ready = 0;
    int selective_message_set_delivered = 0;
    int signon_message_range_fetch_ready = 0;
    int range_message_set_delivered = 0;
    int signon_message_cursor_ready = 0;
    int message_cursor_checkpointed = 0;
    int signon_message_cursor_resume_ready = 0;
    int non_exhausted_cursor_resume_allowed = 0;
    int signon_message_cursor_carryover_ready = 0;
    int checkpoint_cursor_carried_over = 0;
    int signon_message_cursor_carried_resume_ready = 0;
    int non_exhausted_carried_cursor_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_ready = 0;
    int carried_cursor_checkpointed = 0;
    int signon_message_cursor_carried_checkpoint_resume_ready = 0;
    int non_exhausted_carried_checkpoint_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_resume_token_ready = 0;
    int carried_checkpoint_resume_token_issued = 0;
    int signon_message_cursor_carried_checkpoint_resume_token_claim_ready = 0;
    int carried_checkpoint_resume_token_claimed = 0;
    int signon_message_cursor_carried_checkpoint_claimed_resume_ready = 0;
    int non_exhausted_claimed_checkpoint_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_bridge_ready = 0;
    int claimed_checkpoint_bridge_materialized = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_resume_ready = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_resume_token_ready = 0;
    int claimed_checkpoint_resume_token_issued = 0;
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointResumeTokenProbeSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string probe = "disabled";
    int attempts = 0;
    int accepted = 0;
    int rejected = 0;
    std::string last_reject_reason = "<none>";
    std::string requested_claimant_session;
    std::string requested_cursor_id;
    std::string parsed_source_session;
    std::string parsed_issuer_target_session;
    std::string parsed_claimant_session;
    std::string parsed_cursor_id;
    std::string parsed_resume_token;
    std::string parsed_token_purpose;
    std::string parsed_token_state;
    std::string parsed_current_range;
    std::string parsed_next_start_message_index = "<unset>";
    int parsed_remaining_message_count = 0;
    bool parsed_claim_validation_implemented = false;
    int parsed_target_activated = 0;
    std::string parsed_map;
    std::string parsed_name;
    std::string parsed_ruleset;
    int parsed_spawned = 0;
    int signon_ready = 0;
    int bootstrap_delivered = 0;
    int baseline_ready = 0;
    int bootstrap_sequence_completed = 0;
    int signon_catalog_ready = 0;
    int bootstrap_records_staged = 0;
    int signon_template_ready = 0;
    int template_records_delivered = 0;
    int signon_template_coverage_complete = 0;
    int remaining_template_records_delivered = 0;
    int signon_envelope_ready = 0;
    int framed_template_records_delivered = 0;
    int signon_batch_ready = 0;
    int pseudo_packet_batch_delivered = 0;
    int signon_wiremap_ready = 0;
    int wiremapped_batch_delivered = 0;
    int signon_burst_ready = 0;
    int pseudo_wire_burst_delivered = 0;
    int signon_stream_ready = 0;
    int contiguous_stream_delivered = 0;
    int signon_stream_window_ready = 0;
    int windowed_stream_delivered = 0;
    int signon_message_catalog_ready = 0;
    int message_boundaries_cataloged = 0;
    int signon_message_fetch_ready = 0;
    int targeted_message_fetch_delivered = 0;
    int signon_multi_message_fetch_ready = 0;
    int selective_message_set_delivered = 0;
    int signon_message_range_fetch_ready = 0;
    int range_message_set_delivered = 0;
    int signon_message_cursor_ready = 0;
    int message_cursor_checkpointed = 0;
    int signon_message_cursor_resume_ready = 0;
    int non_exhausted_cursor_resume_allowed = 0;
    int signon_message_cursor_carryover_ready = 0;
    int checkpoint_cursor_carried_over = 0;
    int signon_message_cursor_carried_resume_ready = 0;
    int non_exhausted_carried_cursor_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_ready = 0;
    int carried_cursor_checkpointed = 0;
    int signon_message_cursor_carried_checkpoint_resume_ready = 0;
    int non_exhausted_carried_checkpoint_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_resume_token_ready = 0;
    int carried_checkpoint_resume_token_issued = 0;
    int signon_message_cursor_carried_checkpoint_resume_token_claim_ready = 0;
    int carried_checkpoint_resume_token_claimed = 0;
    int signon_message_cursor_carried_checkpoint_claimed_resume_ready = 0;
    int non_exhausted_claimed_checkpoint_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_bridge_ready = 0;
    int claimed_checkpoint_bridge_materialized = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_resume_ready = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_resume_token_ready = 0;
    int claimed_checkpoint_resume_token_issued = 0;
    std::string protocol_shape = "disabled";
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointResumeTokenClaimSurfaceSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string bind = "disabled";
    int requested_port = 0;
    int bound_port = 0;
    std::string protocol_shape = "disabled";
    bool claimed_checkpoint_resume_token_claim_enabled = false;
    bool requires_issued_resume_token = true;
    bool requires_successor_claimant_activated_session = true;
    std::string claim_purpose = "disabled";
    std::string claim_payload = "disabled";
    int accepted = 0;
    int rejected = 0;
    std::string last_source_session;
    std::string last_issuer_target_session;
    std::string last_claimant_session;
    std::string last_successor_claimant_session;
    std::string last_issued_token;
    std::string claim_validation = "disabled";
    std::string auth = "disabled";
    std::string signon = "disabled";
    std::string gameplay_transport = "no";
    int signon_ready = 0;
    int bootstrap_delivered = 0;
    int baseline_ready = 0;
    int bootstrap_sequence_completed = 0;
    int signon_catalog_ready = 0;
    int bootstrap_records_staged = 0;
    int signon_template_ready = 0;
    int template_records_delivered = 0;
    int signon_template_coverage_complete = 0;
    int remaining_template_records_delivered = 0;
    int signon_envelope_ready = 0;
    int framed_template_records_delivered = 0;
    int signon_batch_ready = 0;
    int pseudo_packet_batch_delivered = 0;
    int signon_wiremap_ready = 0;
    int wiremapped_batch_delivered = 0;
    int signon_burst_ready = 0;
    int pseudo_wire_burst_delivered = 0;
    int signon_stream_ready = 0;
    int contiguous_stream_delivered = 0;
    int signon_stream_window_ready = 0;
    int windowed_stream_delivered = 0;
    int signon_message_catalog_ready = 0;
    int message_boundaries_cataloged = 0;
    int signon_message_fetch_ready = 0;
    int targeted_message_fetch_delivered = 0;
    int signon_multi_message_fetch_ready = 0;
    int selective_message_set_delivered = 0;
    int signon_message_range_fetch_ready = 0;
    int range_message_set_delivered = 0;
    int signon_message_cursor_ready = 0;
    int message_cursor_checkpointed = 0;
    int signon_message_cursor_resume_ready = 0;
    int non_exhausted_cursor_resume_allowed = 0;
    int signon_message_cursor_carryover_ready = 0;
    int checkpoint_cursor_carried_over = 0;
    int signon_message_cursor_carried_resume_ready = 0;
    int non_exhausted_carried_cursor_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_ready = 0;
    int carried_cursor_checkpointed = 0;
    int signon_message_cursor_carried_checkpoint_resume_ready = 0;
    int non_exhausted_carried_checkpoint_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_resume_token_ready = 0;
    int carried_checkpoint_resume_token_issued = 0;
    int signon_message_cursor_carried_checkpoint_resume_token_claim_ready = 0;
    int carried_checkpoint_resume_token_claimed = 0;
    int signon_message_cursor_carried_checkpoint_claimed_resume_ready = 0;
    int non_exhausted_claimed_checkpoint_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_bridge_ready = 0;
    int claimed_checkpoint_bridge_materialized = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_resume_ready = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_resume_token_ready = 0;
    int claimed_checkpoint_resume_token_issued = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_resume_token_claim_ready = 0;
    int claimed_checkpoint_resume_token_claimed = 0;
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointResumeTokenClaimProbeSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string probe = "disabled";
    int attempts = 0;
    int accepted = 0;
    int rejected = 0;
    std::string last_reject_reason = "<none>";
    std::string requested_successor_claimant_session;
    std::string requested_resume_token;
    std::string parsed_source_session;
    std::string parsed_issuer_target_session;
    std::string parsed_claimant_session;
    std::string parsed_successor_claimant_session;
    std::string parsed_resume_token;
    std::string parsed_claim_status;
    std::string parsed_claim_validation;
    std::string parsed_current_range;
    std::string parsed_next_start_message_index = "<unset>";
    int parsed_remaining_message_count = 0;
    std::string parsed_claim_purpose;
    int parsed_target_activated = 0;
    std::string parsed_map;
    std::string parsed_name;
    std::string parsed_ruleset;
    int parsed_spawned = 0;
    int signon_ready = 0;
    int bootstrap_delivered = 0;
    int baseline_ready = 0;
    int bootstrap_sequence_completed = 0;
    int signon_catalog_ready = 0;
    int bootstrap_records_staged = 0;
    int signon_template_ready = 0;
    int template_records_delivered = 0;
    int signon_template_coverage_complete = 0;
    int remaining_template_records_delivered = 0;
    int signon_envelope_ready = 0;
    int framed_template_records_delivered = 0;
    int signon_batch_ready = 0;
    int pseudo_packet_batch_delivered = 0;
    int signon_wiremap_ready = 0;
    int wiremapped_batch_delivered = 0;
    int signon_burst_ready = 0;
    int pseudo_wire_burst_delivered = 0;
    int signon_stream_ready = 0;
    int contiguous_stream_delivered = 0;
    int signon_stream_window_ready = 0;
    int windowed_stream_delivered = 0;
    int signon_message_catalog_ready = 0;
    int message_boundaries_cataloged = 0;
    int signon_message_fetch_ready = 0;
    int targeted_message_fetch_delivered = 0;
    int signon_multi_message_fetch_ready = 0;
    int selective_message_set_delivered = 0;
    int signon_message_range_fetch_ready = 0;
    int range_message_set_delivered = 0;
    int signon_message_cursor_ready = 0;
    int message_cursor_checkpointed = 0;
    int signon_message_cursor_resume_ready = 0;
    int non_exhausted_cursor_resume_allowed = 0;
    int signon_message_cursor_carryover_ready = 0;
    int checkpoint_cursor_carried_over = 0;
    int signon_message_cursor_carried_resume_ready = 0;
    int non_exhausted_carried_cursor_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_ready = 0;
    int carried_cursor_checkpointed = 0;
    int signon_message_cursor_carried_checkpoint_resume_ready = 0;
    int non_exhausted_carried_checkpoint_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_resume_token_ready = 0;
    int carried_checkpoint_resume_token_issued = 0;
    int signon_message_cursor_carried_checkpoint_resume_token_claim_ready = 0;
    int carried_checkpoint_resume_token_claimed = 0;
    int signon_message_cursor_carried_checkpoint_claimed_resume_ready = 0;
    int non_exhausted_claimed_checkpoint_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_bridge_ready = 0;
    int claimed_checkpoint_bridge_materialized = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_resume_ready = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_resume_token_ready = 0;
    int claimed_checkpoint_resume_token_issued = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_resume_token_claim_ready = 0;
    int claimed_checkpoint_resume_token_claimed = 0;
    std::string protocol_shape = "disabled";
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorResumeAllowSurfaceSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string bind = "disabled";
    int requested_port = 0;
    int bound_port = 0;
    std::string protocol_shape = "disabled";
    bool successor_resume_allow_enabled = false;
    bool requires_claimed_checkpoint_resume_token_claim_ready_session = true;
    int accepted = 0;
    int rejected = 0;
    std::string last_source_session;
    std::string last_issuer_target_session;
    std::string last_claimant_session;
    std::string last_successor_claimant_session;
    std::string last_allowed_cursor_id;
    bool resume_allowed = false;
    std::string denial_reason = "<unset>";
    bool eof = false;
    bool exhausted = false;
    int current_start_message_index = -1;
    int current_message_count = 0;
    std::string next_start_message_index = "<unset>";
    int remaining_message_count = 0;
    std::string auth = "disabled";
    std::string signon = "disabled";
    std::string gameplay_transport = "no";
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_resume_token_claim_ready = 0;
    int claimed_checkpoint_resume_token_claimed = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_resume_ready = 0;
    int non_exhausted_claimed_checkpoint_successor_resume_allowed = 0;
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorResumeAllowProbeSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string probe = "disabled";
    int attempts = 0;
    int accepted = 0;
    int rejected = 0;
    std::string last_reject_reason = "<none>";
    std::string requested_successor_claimant_session;
    std::string requested_cursor_id;
    std::string parsed_source_session;
    std::string parsed_issuer_target_session;
    std::string parsed_claimant_session;
    std::string parsed_successor_claimant_session;
    std::string parsed_cursor_id;
    bool parsed_resume_allowed = false;
    std::string parsed_denial_reason = "<unset>";
    bool parsed_eof = false;
    bool parsed_exhausted = false;
    std::string parsed_current_range;
    std::string parsed_next_start_message_index = "<unset>";
    int parsed_remaining_message_count = 0;
    std::string parsed_resume_policy;
    int parsed_target_activated = 0;
    std::string parsed_map;
    std::string parsed_name;
    std::string parsed_ruleset;
    int parsed_spawned = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_resume_token_claim_ready = 0;
    int claimed_checkpoint_resume_token_claimed = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_resume_ready = 0;
    int non_exhausted_claimed_checkpoint_successor_resume_allowed = 0;
    std::string protocol_shape = "disabled";
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointBridgeSurfaceSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string bind = "disabled";
    int requested_port = 0;
    int bound_port = 0;
    std::string protocol_shape = "disabled";
    bool successor_checkpoint_bridge_enabled = false;
    bool requires_claimed_checkpoint_successor_resume_ready_session = true;
    int accepted = 0;
    int rejected = 0;
    std::string last_source_session;
    std::string last_issuer_target_session;
    std::string last_claimant_session;
    std::string last_successor_claimant_session;
    std::string last_source_claimed_cursor_id;
    std::string last_successor_checkpoint_cursor_id;
    int current_start_message_index = -1;
    int current_message_count = 0;
    std::string next_start_message_index = "<unset>";
    int remaining_message_count = 0;
    std::string bridge_policy = "<unset>";
    std::string auth = "disabled";
    std::string signon = "disabled";
    std::string gameplay_transport = "no";
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_resume_ready = 0;
    int non_exhausted_claimed_checkpoint_successor_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_bridge_ready =
        0;
    int claimed_checkpoint_successor_bridge_materialized = 0;
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointBridgeProbeSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string probe = "disabled";
    int attempts = 0;
    int accepted = 0;
    int rejected = 0;
    std::string last_reject_reason = "<none>";
    std::string requested_successor_claimant_session;
    std::string requested_cursor_id;
    std::string parsed_source_session;
    std::string parsed_issuer_target_session;
    std::string parsed_claimant_session;
    std::string parsed_successor_claimant_session;
    std::string parsed_source_claimed_cursor_id;
    std::string parsed_successor_checkpoint_cursor_id;
    std::string parsed_current_range;
    std::string parsed_next_start_message_index = "<unset>";
    int parsed_remaining_message_count = 0;
    std::string parsed_bridge_policy;
    int parsed_target_activated = 0;
    std::string parsed_map;
    std::string parsed_name;
    std::string parsed_ruleset;
    int parsed_spawned = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_resume_ready = 0;
    int non_exhausted_claimed_checkpoint_successor_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_bridge_ready =
        0;
    int claimed_checkpoint_successor_bridge_materialized = 0;
    std::string protocol_shape = "disabled";
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointResumeAllowSurfaceSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string bind = "disabled";
    int requested_port = 0;
    int bound_port = 0;
    std::string protocol_shape = "disabled";
    bool successor_checkpoint_resume_allow_enabled = false;
    bool requires_claimed_checkpoint_successor_checkpoint_bridge_ready_session = true;
    int accepted = 0;
    int rejected = 0;
    std::string last_source_session;
    std::string last_issuer_target_session;
    std::string last_claimant_session;
    std::string last_successor_claimant_session;
    std::string last_allowed_cursor_id;
    bool resume_allowed = false;
    std::string denial_reason = "<unset>";
    bool eof = false;
    bool exhausted = false;
    int current_start_message_index = -1;
    int current_message_count = 0;
    std::string next_start_message_index = "<unset>";
    int remaining_message_count = 0;
    std::string auth = "disabled";
    std::string signon = "disabled";
    std::string gameplay_transport = "no";
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_bridge_ready =
        0;
    int claimed_checkpoint_successor_bridge_materialized = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_ready =
        0;
    int non_exhausted_claimed_checkpoint_successor_checkpoint_resume_allowed = 0;
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointResumeAllowProbeSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string probe = "disabled";
    int attempts = 0;
    int accepted = 0;
    int rejected = 0;
    std::string last_reject_reason = "<none>";
    std::string requested_successor_claimant_session;
    std::string requested_cursor_id;
    std::string parsed_source_session;
    std::string parsed_issuer_target_session;
    std::string parsed_claimant_session;
    std::string parsed_successor_claimant_session;
    std::string parsed_cursor_id;
    bool parsed_resume_allowed = false;
    std::string parsed_denial_reason = "<unset>";
    bool parsed_eof = false;
    bool parsed_exhausted = false;
    std::string parsed_current_range;
    std::string parsed_next_start_message_index = "<unset>";
    int parsed_remaining_message_count = 0;
    std::string parsed_resume_policy;
    int parsed_target_activated = 0;
    std::string parsed_map;
    std::string parsed_name;
    std::string parsed_ruleset;
    int parsed_spawned = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_bridge_ready =
        0;
    int claimed_checkpoint_successor_bridge_materialized = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_ready =
        0;
    int non_exhausted_claimed_checkpoint_successor_checkpoint_resume_allowed = 0;
    std::string protocol_shape = "disabled";
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointResumeTokenSurfaceSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string bind = "disabled";
    int requested_port = 0;
    int bound_port = 0;
    std::string protocol_shape = "disabled";
    bool successor_checkpoint_resume_token_enabled = false;
    bool requires_claimed_checkpoint_successor_checkpoint_resume_ready_session = true;
    std::string token_purpose = "disabled";
    std::string token_payload = "disabled";
    int accepted = 0;
    int rejected = 0;
    std::string last_source_session;
    std::string last_issuer_target_session;
    std::string last_claimant_session;
    std::string last_successor_claimant_session;
    std::string last_checkpoint_cursor_id;
    std::string last_issued_token;
    std::string claim_validation_implemented = "no";
    std::string auth = "disabled";
    std::string signon = "disabled";
    std::string gameplay_transport = "no";
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_ready =
        0;
    int non_exhausted_claimed_checkpoint_successor_checkpoint_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_ready =
        0;
    int claimed_checkpoint_successor_resume_token_issued = 0;
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointResumeTokenProbeSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string probe = "disabled";
    int attempts = 0;
    int accepted = 0;
    int rejected = 0;
    std::string last_reject_reason = "<none>";
    std::string requested_successor_claimant_session;
    std::string requested_cursor_id;
    std::string parsed_source_session;
    std::string parsed_issuer_target_session;
    std::string parsed_claimant_session;
    std::string parsed_successor_claimant_session;
    std::string parsed_cursor_id;
    std::string parsed_resume_token;
    std::string parsed_token_purpose;
    std::string parsed_token_state;
    std::string parsed_current_range;
    std::string parsed_next_start_message_index = "<unset>";
    int parsed_remaining_message_count = 0;
    bool parsed_claim_validation_implemented = false;
    int parsed_target_activated = 0;
    std::string parsed_map;
    std::string parsed_name;
    std::string parsed_ruleset;
    int parsed_spawned = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_ready =
        0;
    int non_exhausted_claimed_checkpoint_successor_checkpoint_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_ready =
        0;
    int claimed_checkpoint_successor_resume_token_issued = 0;
    std::string protocol_shape = "disabled";
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointResumeTokenClaimSurfaceSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string bind = "disabled";
    int requested_port = 0;
    int bound_port = 0;
    std::string protocol_shape = "disabled";
    bool successor_checkpoint_resume_token_claim_enabled = false;
    bool requires_issued_resume_token = true;
    bool requires_tertiary_successor_claimant_activated_session = true;
    std::string claim_purpose = "disabled";
    std::string claim_payload = "disabled";
    int accepted = 0;
    int rejected = 0;
    std::string last_source_session;
    std::string last_issuer_target_session;
    std::string last_claimant_session;
    std::string last_successor_claimant_session;
    std::string last_tertiary_successor_claimant_session;
    std::string last_issued_token;
    std::string claim_validation = "disabled";
    std::string auth = "disabled";
    std::string signon = "disabled";
    std::string gameplay_transport = "no";
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_ready =
        0;
    int claimed_checkpoint_successor_resume_token_issued = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claimed = 0;
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointResumeTokenClaimProbeSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string probe = "disabled";
    int attempts = 0;
    int accepted = 0;
    int rejected = 0;
    std::string last_reject_reason = "<none>";
    std::string requested_tertiary_successor_claimant_session;
    std::string requested_resume_token;
    std::string parsed_source_session;
    std::string parsed_issuer_target_session;
    std::string parsed_claimant_session;
    std::string parsed_successor_claimant_session;
    std::string parsed_tertiary_successor_claimant_session;
    std::string parsed_resume_token;
    std::string parsed_claim_status;
    std::string parsed_claim_validation;
    std::string parsed_current_range;
    std::string parsed_next_start_message_index = "<unset>";
    int parsed_remaining_message_count = 0;
    std::string parsed_claim_purpose;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_ready =
        0;
    int claimed_checkpoint_successor_resume_token_issued = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claimed = 0;
    std::string protocol_shape = "disabled";
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointResumeTokenClaimRangeSurfaceSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string bind = "disabled";
    int requested_port = 0;
    int bound_port = 0;
    std::string protocol_shape = "disabled";
    bool successor_checkpoint_resume_token_claim_range_enabled = false;
    bool requires_claimed_successor_checkpoint_resume_token_claim_ready_session = true;
    std::string fetch_selector = "disabled";
    std::string fetch_payload = "disabled";
    int requested_message_count = 0;
    int accepted = 0;
    int rejected = 0;
    std::string last_source_session;
    std::string last_issuer_target_session;
    std::string last_claimant_session;
    std::string last_successor_claimant_session;
    std::string last_tertiary_successor_claimant_session;
    std::string last_checkpoint_cursor_id;
    std::string last_fetched_message_indices;
    std::string next_start_message_index_after_claim_fetch = "<unset>";
    int remaining_message_count_after_claim_fetch = 0;
    std::string auth = "disabled";
    std::string signon = "disabled";
    std::string gameplay_transport = "no";
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claimed = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_range_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claim_range_delivered = 0;
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointResumeTokenClaimRangeProbeSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string probe = "disabled";
    int attempts = 0;
    int accepted = 0;
    int rejected = 0;
    std::string last_reject_reason = "<none>";
    std::string requested_tertiary_successor_claimant_session;
    int requested_message_count = 0;
    std::string requested_cursor_id;
    std::string parsed_source_session;
    std::string parsed_issuer_target_session;
    std::string parsed_claimant_session;
    std::string parsed_successor_claimant_session;
    std::string parsed_tertiary_successor_claimant_session;
    std::string parsed_cursor_id;
    std::string parsed_fetched_message_indices;
    std::string parsed_semantic_tags;
    int parsed_combined_byte_length = 0;
    std::string parsed_next_start_message_index_after_claim_fetch = "<unset>";
    int parsed_remaining_message_count_after_claim_fetch = 0;
    std::string parsed_claim_purpose;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claimed = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_range_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claim_range_delivered = 0;
    std::string protocol_shape = "disabled";
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointResumeTokenClaimEofSurfaceSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string bind = "disabled";
    int requested_port = 0;
    int bound_port = 0;
    std::string protocol_shape = "disabled";
    bool successor_checkpoint_resume_token_claim_eof_enabled = false;
    bool requires_claimed_successor_checkpoint_resume_token_claim_range_ready_session = true;
    int accepted = 0;
    int rejected = 0;
    std::string last_source_session;
    std::string last_issuer_target_session;
    std::string last_claimant_session;
    std::string last_successor_claimant_session;
    std::string last_tertiary_successor_claimant_session;
    std::string last_checkpoint_cursor_id;
    bool eof = false;
    bool exhausted = false;
    std::string next_start_message_index = "<unset>";
    int remaining_message_count = 0;
    std::string auth = "disabled";
    std::string signon = "disabled";
    std::string gameplay_transport = "no";
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claimed = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_range_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claim_range_delivered = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_eof_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claim_exhausted = 0;
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointResumeTokenClaimEofProbeSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string probe = "disabled";
    int attempts = 0;
    int accepted = 0;
    int rejected = 0;
    std::string last_reject_reason = "<none>";
    std::string requested_tertiary_successor_claimant_session;
    std::string requested_cursor_id;
    std::string parsed_source_session;
    std::string parsed_issuer_target_session;
    std::string parsed_claimant_session;
    std::string parsed_successor_claimant_session;
    std::string parsed_tertiary_successor_claimant_session;
    std::string parsed_cursor_id;
    bool parsed_eof = false;
    bool parsed_exhausted = false;
    std::string parsed_current_range;
    std::string parsed_next_start_message_index = "<unset>";
    int parsed_remaining_message_count = 0;
    std::string parsed_resume_policy;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claimed = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_range_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claim_range_delivered = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_eof_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claim_exhausted = 0;
    std::string protocol_shape = "disabled";
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointResumeTokenClaimResumeDenialSurfaceSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string bind = "disabled";
    int requested_port = 0;
    int bound_port = 0;
    std::string protocol_shape = "disabled";
    bool successor_checkpoint_resume_token_claim_resume_denial_enabled = false;
    bool requires_claimed_successor_checkpoint_resume_token_claim_eof_ready_session = true;
    int accepted = 0;
    int rejected = 0;
    std::string last_source_session;
    std::string last_issuer_target_session;
    std::string last_claimant_session;
    std::string last_successor_claimant_session;
    std::string last_tertiary_successor_claimant_session;
    std::string last_denied_cursor_id;
    bool resume_allowed = false;
    std::string denial_reason = "<unset>";
    bool eof = false;
    bool exhausted = false;
    std::string next_start_message_index = "<unset>";
    int remaining_message_count = 0;
    std::string auth = "disabled";
    std::string signon = "disabled";
    std::string gameplay_transport = "no";
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claimed = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_range_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claim_range_delivered = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_eof_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claim_exhausted = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_resume_denied_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claim_resume_denied = 0;
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointResumeTokenClaimResumeDenialProbeSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string probe = "disabled";
    int attempts = 0;
    int accepted = 0;
    int rejected = 0;
    std::string last_reject_reason = "<none>";
    std::string requested_tertiary_successor_claimant_session;
    std::string requested_cursor_id;
    std::string parsed_source_session;
    std::string parsed_issuer_target_session;
    std::string parsed_claimant_session;
    std::string parsed_successor_claimant_session;
    std::string parsed_tertiary_successor_claimant_session;
    std::string parsed_cursor_id;
    bool parsed_resume_allowed = false;
    std::string parsed_denial_reason;
    bool parsed_eof = false;
    bool parsed_exhausted = false;
    std::string parsed_next_start_message_index = "<unset>";
    int parsed_remaining_message_count = 0;
    std::string parsed_resume_policy;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claimed = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_range_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claim_range_delivered = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_eof_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claim_exhausted = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_resume_denied_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claim_resume_denied = 0;
    std::string protocol_shape = "disabled";
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointResumeTokenClaimResumeAllowSurfaceSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string bind = "disabled";
    int requested_port = 0;
    int bound_port = 0;
    std::string protocol_shape = "disabled";
    bool successor_checkpoint_resume_token_claim_resume_allow_enabled = false;
    bool requires_claimed_successor_checkpoint_resume_token_claim_ready_session = true;
    int accepted = 0;
    int rejected = 0;
    std::string last_source_session;
    std::string last_issuer_target_session;
    std::string last_claimant_session;
    std::string last_successor_claimant_session;
    std::string last_tertiary_successor_claimant_session;
    std::string last_allowed_cursor_id;
    bool resume_allowed = false;
    std::string denial_reason = "<unset>";
    bool eof = false;
    bool exhausted = false;
    std::string next_start_message_index = "<unset>";
    int remaining_message_count = 0;
    std::string auth = "disabled";
    std::string signon = "disabled";
    std::string gameplay_transport = "no";
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claimed = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_resume_ready =
        0;
    int non_exhausted_claimed_checkpoint_successor_resume_token_claim_resume_allowed =
        0;
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointResumeTokenClaimResumeAllowProbeSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string probe = "disabled";
    int attempts = 0;
    int accepted = 0;
    int rejected = 0;
    std::string last_reject_reason = "<none>";
    std::string requested_tertiary_successor_claimant_session;
    std::string requested_cursor_id;
    std::string parsed_source_session;
    std::string parsed_issuer_target_session;
    std::string parsed_claimant_session;
    std::string parsed_successor_claimant_session;
    std::string parsed_tertiary_successor_claimant_session;
    std::string parsed_cursor_id;
    bool parsed_resume_allowed = false;
    std::string parsed_denial_reason = "<unset>";
    bool parsed_eof = false;
    bool parsed_exhausted = false;
    std::string parsed_current_range;
    std::string parsed_next_start_message_index = "<unset>";
    int parsed_remaining_message_count = 0;
    std::string parsed_resume_policy;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claimed = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_resume_ready =
        0;
    int non_exhausted_claimed_checkpoint_successor_resume_token_claim_resume_allowed =
        0;
    std::string protocol_shape = "disabled";
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointResumeTokenClaimResumeRangeSurfaceSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string bind = "disabled";
    int requested_port = 0;
    int bound_port = 0;
    std::string protocol_shape = "disabled";
    bool successor_checkpoint_resume_token_claim_resume_range_enabled = false;
    bool requires_claimed_successor_checkpoint_resume_token_claim_resume_ready_session = true;
    std::string fetch_selector = "disabled";
    std::string fetch_payload = "disabled";
    int requested_message_count = 0;
    int accepted = 0;
    int rejected = 0;
    std::string last_source_session;
    std::string last_issuer_target_session;
    std::string last_claimant_session;
    std::string last_successor_claimant_session;
    std::string last_tertiary_successor_claimant_session;
    std::string last_checkpoint_cursor_id;
    std::string last_fetched_message_indices;
    std::string next_start_message_index_after_resume_fetch = "<unset>";
    int remaining_message_count_after_resume_fetch = 0;
    std::string auth = "disabled";
    std::string signon = "disabled";
    std::string gameplay_transport = "no";
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_resume_ready =
        0;
    int non_exhausted_claimed_checkpoint_successor_resume_token_claim_resume_allowed =
        0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_resume_range_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claim_resume_range_delivered = 0;
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointResumeTokenClaimResumeRangeProbeSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string probe = "disabled";
    int attempts = 0;
    int accepted = 0;
    int rejected = 0;
    std::string last_reject_reason = "<none>";
    std::string requested_tertiary_successor_claimant_session;
    int requested_message_count = 0;
    std::string requested_cursor_id;
    std::string parsed_source_session;
    std::string parsed_issuer_target_session;
    std::string parsed_claimant_session;
    std::string parsed_successor_claimant_session;
    std::string parsed_tertiary_successor_claimant_session;
    std::string parsed_cursor_id;
    std::string parsed_fetched_message_indices;
    std::string parsed_semantic_tags;
    int parsed_combined_byte_length = 0;
    std::string parsed_next_start_message_index_after_resume_fetch = "<unset>";
    int parsed_remaining_message_count_after_resume_fetch = 0;
    std::string parsed_resume_policy;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_resume_ready =
        0;
    int non_exhausted_claimed_checkpoint_successor_resume_token_claim_resume_allowed =
        0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_resume_range_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claim_resume_range_delivered = 0;
    std::string protocol_shape = "disabled";
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointResumeTokenClaimResumeEofSurfaceSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string bind = "disabled";
    int requested_port = 0;
    int bound_port = 0;
    std::string protocol_shape = "disabled";
    bool successor_checkpoint_resume_token_claim_resume_eof_enabled = false;
    bool requires_claimed_successor_checkpoint_resume_token_claim_resume_range_ready_session =
        true;
    int accepted = 0;
    int rejected = 0;
    std::string last_source_session;
    std::string last_issuer_target_session;
    std::string last_claimant_session;
    std::string last_successor_claimant_session;
    std::string last_tertiary_successor_claimant_session;
    std::string last_checkpoint_cursor_id;
    bool eof = false;
    bool exhausted = false;
    std::string next_start_message_index = "<unset>";
    int remaining_message_count = 0;
    std::string auth = "disabled";
    std::string signon = "disabled";
    std::string gameplay_transport = "no";
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_resume_ready =
        0;
    int non_exhausted_claimed_checkpoint_successor_resume_token_claim_resume_allowed =
        0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_resume_range_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claim_resume_range_delivered = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_resume_eof_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claim_resume_exhausted = 0;
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointResumeTokenClaimResumeEofProbeSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string probe = "disabled";
    int attempts = 0;
    int accepted = 0;
    int rejected = 0;
    std::string last_reject_reason = "<none>";
    std::string requested_tertiary_successor_claimant_session;
    std::string requested_cursor_id;
    std::string parsed_source_session;
    std::string parsed_issuer_target_session;
    std::string parsed_claimant_session;
    std::string parsed_successor_claimant_session;
    std::string parsed_tertiary_successor_claimant_session;
    std::string parsed_cursor_id;
    bool parsed_eof = false;
    bool parsed_exhausted = false;
    std::string parsed_current_range;
    std::string parsed_next_start_message_index = "<unset>";
    int parsed_remaining_message_count = 0;
    std::string parsed_resume_policy;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_resume_ready =
        0;
    int non_exhausted_claimed_checkpoint_successor_resume_token_claim_resume_allowed =
        0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_resume_range_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claim_resume_range_delivered = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_resume_eof_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claim_resume_exhausted = 0;
    std::string protocol_shape = "disabled";
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointResumeTokenClaimResumedDenialSurfaceSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string bind = "disabled";
    int requested_port = 0;
    int bound_port = 0;
    std::string protocol_shape = "disabled";
    bool successor_checkpoint_resume_token_claim_resumed_denial_enabled = false;
    bool requires_claimed_successor_checkpoint_resume_token_claim_resume_eof_ready_session =
        true;
    int accepted = 0;
    int rejected = 0;
    std::string last_source_session;
    std::string last_issuer_target_session;
    std::string last_claimant_session;
    std::string last_successor_claimant_session;
    std::string last_tertiary_successor_claimant_session;
    std::string last_denied_cursor_id;
    bool resume_allowed = false;
    std::string denial_reason = "<unset>";
    bool eof = false;
    bool exhausted = false;
    std::string next_start_message_index = "<unset>";
    int remaining_message_count = 0;
    std::string auth = "disabled";
    std::string signon = "disabled";
    std::string gameplay_transport = "no";
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_resume_ready =
        0;
    int non_exhausted_claimed_checkpoint_successor_resume_token_claim_resume_allowed =
        0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_resume_range_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claim_resume_range_delivered = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_resume_eof_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claim_resume_exhausted = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_resumed_denied_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claim_resumed_denied = 0;
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointResumeTokenClaimResumedDenialProbeSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string probe = "disabled";
    int attempts = 0;
    int accepted = 0;
    int rejected = 0;
    std::string last_reject_reason = "<none>";
    std::string requested_tertiary_successor_claimant_session;
    std::string requested_cursor_id;
    std::string parsed_source_session;
    std::string parsed_issuer_target_session;
    std::string parsed_claimant_session;
    std::string parsed_successor_claimant_session;
    std::string parsed_tertiary_successor_claimant_session;
    std::string parsed_cursor_id;
    bool parsed_resume_allowed = false;
    std::string parsed_denial_reason;
    bool parsed_eof = false;
    bool parsed_exhausted = false;
    std::string parsed_next_start_message_index = "<unset>";
    int parsed_remaining_message_count = 0;
    std::string parsed_resume_policy;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_resume_ready =
        0;
    int non_exhausted_claimed_checkpoint_successor_resume_token_claim_resume_allowed =
        0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_resume_range_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claim_resume_range_delivered = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_resume_eof_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claim_resume_exhausted = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_resumed_denied_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claim_resumed_denied = 0;
    std::string protocol_shape = "disabled";
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointResumeTokenClaimCheckpointBridgeSurfaceSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string bind = "disabled";
    int requested_port = 0;
    int bound_port = 0;
    std::string protocol_shape = "disabled";
    bool successor_checkpoint_resume_token_claim_checkpoint_bridge_enabled = false;
    bool requires_claimed_successor_checkpoint_resume_token_claim_resume_ready_session =
        true;
    int accepted = 0;
    int rejected = 0;
    std::string last_source_session;
    std::string last_issuer_target_session;
    std::string last_claimant_session;
    std::string last_successor_claimant_session;
    std::string last_tertiary_successor_claimant_session;
    std::string last_source_claimed_cursor_id;
    std::string last_tertiary_successor_checkpoint_cursor_id;
    int current_start_message_index = -1;
    int current_message_count = 0;
    std::string next_start_message_index = "<unset>";
    int remaining_message_count = 0;
    std::string bridge_policy = "<unset>";
    std::string auth = "disabled";
    std::string signon = "disabled";
    std::string gameplay_transport = "no";
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_resume_ready =
        0;
    int non_exhausted_claimed_checkpoint_successor_resume_token_claim_resume_allowed =
        0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_bridge_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claim_bridge_materialized = 0;
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointResumeTokenClaimCheckpointBridgeProbeSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string probe = "disabled";
    int attempts = 0;
    int accepted = 0;
    int rejected = 0;
    std::string last_reject_reason = "<none>";
    std::string requested_tertiary_successor_claimant_session;
    std::string requested_cursor_id;
    std::string parsed_source_session;
    std::string parsed_issuer_target_session;
    std::string parsed_claimant_session;
    std::string parsed_successor_claimant_session;
    std::string parsed_tertiary_successor_claimant_session;
    std::string parsed_source_claimed_cursor_id;
    std::string parsed_tertiary_successor_checkpoint_cursor_id;
    std::string parsed_current_range;
    std::string parsed_next_start_message_index = "<unset>";
    int parsed_remaining_message_count = 0;
    std::string parsed_bridge_policy;
    int parsed_target_activated = 0;
    std::string parsed_map;
    std::string parsed_name;
    std::string parsed_ruleset;
    int parsed_spawned = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_resume_ready =
        0;
    int non_exhausted_claimed_checkpoint_successor_resume_token_claim_resume_allowed =
        0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_bridge_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claim_bridge_materialized = 0;
    std::string protocol_shape = "disabled";
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointResumeTokenClaimCheckpointResumeAllowSurfaceSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string bind = "disabled";
    int requested_port = 0;
    int bound_port = 0;
    std::string protocol_shape = "disabled";
    bool successor_checkpoint_resume_token_claim_checkpoint_resume_allow_enabled =
        false;
    bool requires_claimed_successor_checkpoint_resume_token_claim_checkpoint_bridge_ready_session =
        true;
    int accepted = 0;
    int rejected = 0;
    std::string last_source_session;
    std::string last_issuer_target_session;
    std::string last_claimant_session;
    std::string last_successor_claimant_session;
    std::string last_tertiary_successor_claimant_session;
    std::string last_allowed_cursor_id;
    bool resume_allowed = false;
    std::string denial_reason = "<unset>";
    bool eof = false;
    bool exhausted = false;
    std::string next_start_message_index = "<unset>";
    int remaining_message_count = 0;
    std::string auth = "disabled";
    std::string signon = "disabled";
    std::string gameplay_transport = "no";
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_bridge_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claim_bridge_materialized = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_ready =
        0;
    int non_exhausted_claimed_checkpoint_successor_resume_token_claim_checkpoint_resume_allowed =
        0;
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointResumeTokenClaimCheckpointResumeAllowProbeSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string probe = "disabled";
    int attempts = 0;
    int accepted = 0;
    int rejected = 0;
    std::string last_reject_reason = "<none>";
    std::string requested_tertiary_successor_claimant_session;
    std::string requested_cursor_id;
    std::string parsed_source_session;
    std::string parsed_issuer_target_session;
    std::string parsed_claimant_session;
    std::string parsed_successor_claimant_session;
    std::string parsed_tertiary_successor_claimant_session;
    std::string parsed_cursor_id;
    bool parsed_resume_allowed = false;
    std::string parsed_denial_reason;
    bool parsed_eof = false;
    bool parsed_exhausted = false;
    std::string parsed_current_range;
    std::string parsed_next_start_message_index = "<unset>";
    int parsed_remaining_message_count = 0;
    std::string parsed_resume_policy;
    int parsed_target_activated = 0;
    std::string parsed_map;
    std::string parsed_name;
    std::string parsed_ruleset;
    int parsed_spawned = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_bridge_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claim_bridge_materialized = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_ready =
        0;
    int non_exhausted_claimed_checkpoint_successor_resume_token_claim_checkpoint_resume_allowed =
        0;
    std::string protocol_shape = "disabled";
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointResumeTokenClaimCheckpointResumeTokenSurfaceSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string bind = "disabled";
    int requested_port = 0;
    int bound_port = 0;
    std::string protocol_shape = "disabled";
    bool successor_checkpoint_resume_token_enabled = false;
    bool requires_claimed_successor_checkpoint_resume_ready_session = true;
    std::string token_purpose = "disabled";
    std::string token_payload = "disabled";
    int accepted = 0;
    int rejected = 0;
    std::string last_source_session;
    std::string last_issuer_target_session;
    std::string last_claimant_session;
    std::string last_successor_claimant_session;
    std::string last_tertiary_successor_claimant_session;
    std::string last_checkpoint_cursor_id;
    std::string last_issued_token;
    std::string claim_validation_implemented = "no";
    std::string auth = "disabled";
    std::string signon = "disabled";
    std::string gameplay_transport = "no";
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_ready =
        0;
    int non_exhausted_claimed_checkpoint_successor_resume_token_claim_checkpoint_resume_allowed =
        0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claim_checkpoint_resume_token_issued =
        0;
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointResumeTokenClaimCheckpointResumeTokenProbeSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string probe = "disabled";
    int attempts = 0;
    int accepted = 0;
    int rejected = 0;
    std::string last_reject_reason = "<none>";
    std::string requested_tertiary_successor_claimant_session;
    std::string requested_cursor_id;
    std::string parsed_source_session;
    std::string parsed_issuer_target_session;
    std::string parsed_claimant_session;
    std::string parsed_successor_claimant_session;
    std::string parsed_tertiary_successor_claimant_session;
    std::string parsed_cursor_id;
    std::string parsed_resume_token;
    std::string parsed_token_purpose;
    std::string parsed_token_state;
    std::string parsed_current_range;
    std::string parsed_next_start_message_index = "<unset>";
    int parsed_remaining_message_count = 0;
    bool parsed_claim_validation_implemented = false;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_ready =
        0;
    int non_exhausted_claimed_checkpoint_successor_resume_token_claim_checkpoint_resume_allowed =
        0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claim_checkpoint_resume_token_issued =
        0;
    std::string protocol_shape = "disabled";
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointResumeTokenClaimCheckpointResumeTokenClaimSurfaceSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string bind = "disabled";
    int requested_port = 0;
    int bound_port = 0;
    std::string protocol_shape = "disabled";
    bool successor_checkpoint_resume_token_claim_enabled = false;
    bool requires_issued_resume_token = true;
    bool requires_quaternary_claimant_activated_session = true;
    std::string claimant_depth = "quinary";
    std::string claim_purpose = "disabled";
    std::string claim_payload = "disabled";
    int accepted = 0;
    int rejected = 0;
    std::string last_source_session;
    std::string last_issuer_target_session;
    std::string last_claimant_session;
    std::string last_successor_claimant_session;
    std::string last_tertiary_successor_claimant_session;
    std::string last_quaternary_claimant_session;
    std::string last_quinary_claimant_session;
    std::string last_issued_token;
    std::string claim_validation = "disabled";
    std::string auth = "disabled";
    std::string signon = "disabled";
    std::string gameplay_transport = "no";
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claim_checkpoint_resume_token_issued =
        0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claim_checkpoint_resume_token_claimed =
        0;
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointResumeTokenClaimCheckpointResumeTokenClaimProbeSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string probe = "disabled";
    int attempts = 0;
    int accepted = 0;
    int rejected = 0;
    std::string last_reject_reason = "<none>";
    std::string requested_quaternary_claimant_session;
    std::string requested_resume_token;
    std::string parsed_source_session;
    std::string parsed_issuer_target_session;
    std::string parsed_claimant_session;
    std::string parsed_successor_claimant_session;
    std::string parsed_tertiary_successor_claimant_session;
    std::string parsed_quaternary_claimant_session;
    std::string parsed_quinary_claimant_session;
    std::string parsed_resume_token;
    std::string parsed_claim_status;
    std::string parsed_claim_validation;
    std::string parsed_claimant_depth;
    std::string parsed_current_range;
    std::string parsed_next_start_message_index = "<unset>";
    int parsed_remaining_message_count = 0;
    std::string parsed_claim_purpose;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claim_checkpoint_resume_token_issued =
        0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claim_checkpoint_resume_token_claimed =
        0;
    std::string protocol_shape = "disabled";
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointResumeTokenClaimCheckpointResumeTokenClaimResumeAllowSurfaceSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string bind = "disabled";
    int requested_port = 0;
    int bound_port = 0;
    std::string protocol_shape = "disabled";
    bool successor_checkpoint_resume_token_claim_resume_allow_enabled = false;
    bool requires_claimed_successor_checkpoint_resume_token_claim_ready_session =
        true;
    std::string claimant_depth = "quinary";
    int accepted = 0;
    int rejected = 0;
    std::string last_source_session;
    std::string last_issuer_target_session;
    std::string last_claimant_session;
    std::string last_successor_claimant_session;
    std::string last_tertiary_successor_claimant_session;
    std::string last_quaternary_claimant_session;
    std::string last_quinary_claimant_session;
    std::string last_allowed_cursor_id;
    bool resume_allowed = false;
    std::string denial_reason = "<unset>";
    bool eof = false;
    bool exhausted = false;
    std::string next_start_message_index = "<unset>";
    int remaining_message_count = 0;
    std::string auth = "disabled";
    std::string signon = "disabled";
    std::string gameplay_transport = "no";
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claim_checkpoint_resume_token_claimed =
        0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_resume_ready =
        0;
    int non_exhausted_claimed_checkpoint_successor_resume_token_claim_checkpoint_resume_token_claim_resume_allowed =
        0;
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointResumeTokenClaimCheckpointResumeTokenClaimResumeAllowProbeSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string probe = "disabled";
    int attempts = 0;
    int accepted = 0;
    int rejected = 0;
    std::string last_reject_reason = "<none>";
    std::string requested_quaternary_claimant_session;
    std::string requested_quinary_claimant_session;
    std::string requested_cursor_id;
    std::string parsed_source_session;
    std::string parsed_issuer_target_session;
    std::string parsed_claimant_session;
    std::string parsed_successor_claimant_session;
    std::string parsed_tertiary_successor_claimant_session;
    std::string parsed_quaternary_claimant_session;
    std::string parsed_quinary_claimant_session;
    std::string parsed_claimant_depth = "quinary";
    std::string parsed_cursor_id;
    bool parsed_resume_allowed = false;
    std::string parsed_denial_reason;
    bool parsed_eof = false;
    bool parsed_exhausted = false;
    std::string parsed_current_range;
    std::string parsed_next_start_message_index = "<unset>";
    int parsed_remaining_message_count = 0;
    std::string parsed_resume_policy;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claim_checkpoint_resume_token_claimed =
        0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_resume_ready =
        0;
    int non_exhausted_claimed_checkpoint_successor_resume_token_claim_checkpoint_resume_token_claim_resume_allowed =
        0;
    std::string protocol_shape = "disabled";
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointResumeTokenClaimCheckpointResumeTokenClaimResumeRangeSurfaceSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string bind = "disabled";
    int requested_port = 0;
    int bound_port = 0;
    std::string protocol_shape = "disabled";
    bool successor_checkpoint_resume_token_claim_resume_range_enabled = false;
    bool requires_claimed_successor_checkpoint_resume_ready_session = true;
    std::string fetch_selector = "disabled";
    std::string fetch_payload = "disabled";
    int requested_message_count = 0;
    int accepted = 0;
    int rejected = 0;
    std::string last_source_session;
    std::string last_issuer_target_session;
    std::string last_claimant_session;
    std::string last_successor_claimant_session;
    std::string last_tertiary_successor_claimant_session;
    std::string last_quaternary_claimant_session;
    std::string last_quinary_claimant_session;
    std::string claimant_depth = "quinary";
    std::string last_checkpoint_cursor_id;
    std::string last_fetched_message_indices;
    std::string next_start_message_index_after_resume_fetch = "<unset>";
    int remaining_message_count_after_resume_fetch = 0;
    std::string auth = "disabled";
    std::string signon = "disabled";
    std::string gameplay_transport = "no";
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_resume_ready =
        0;
    int non_exhausted_claimed_checkpoint_successor_resume_token_claim_checkpoint_resume_token_claim_resume_allowed =
        0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_resume_range_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claim_checkpoint_resume_token_claim_resume_range_delivered =
        0;
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointResumeTokenClaimCheckpointResumeTokenClaimResumeRangeProbeSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string probe = "disabled";
    int attempts = 0;
    int accepted = 0;
    int rejected = 0;
    std::string last_reject_reason = "<none>";
    std::string requested_quaternary_claimant_session;
    std::string requested_quinary_claimant_session;
    int requested_message_count = 0;
    std::string requested_cursor_id;
    std::string parsed_source_session;
    std::string parsed_issuer_target_session;
    std::string parsed_claimant_session;
    std::string parsed_successor_claimant_session;
    std::string parsed_tertiary_successor_claimant_session;
    std::string parsed_quaternary_claimant_session;
    std::string parsed_quinary_claimant_session;
    std::string parsed_claimant_depth = "quinary";
    std::string parsed_cursor_id;
    std::string parsed_fetched_message_indices;
    std::string parsed_semantic_tags;
    int parsed_combined_byte_length = 0;
    std::string parsed_next_start_message_index_after_resume_fetch = "<unset>";
    int parsed_remaining_message_count_after_resume_fetch = 0;
    std::string parsed_resume_policy;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_resume_ready =
        0;
    int non_exhausted_claimed_checkpoint_successor_resume_token_claim_checkpoint_resume_token_claim_resume_allowed =
        0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_resume_range_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claim_checkpoint_resume_token_claim_resume_range_delivered =
        0;
    std::string protocol_shape = "disabled";
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointResumeTokenClaimCheckpointResumeTokenClaimResumeEofSurfaceSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string bind = "disabled";
    int requested_port = 0;
    int bound_port = 0;
    std::string protocol_shape = "disabled";
    bool successor_checkpoint_resume_token_claim_resume_eof_enabled = false;
    bool requires_claimed_successor_checkpoint_resume_token_claim_resume_range_ready_session =
        true;
    std::string claimant_depth = "quinary";
    int accepted = 0;
    int rejected = 0;
    std::string last_source_session;
    std::string last_issuer_target_session;
    std::string last_claimant_session;
    std::string last_successor_claimant_session;
    std::string last_tertiary_successor_claimant_session;
    std::string last_quaternary_claimant_session;
    std::string last_quinary_claimant_session;
    std::string last_checkpoint_cursor_id;
    bool eof = false;
    bool exhausted = false;
    std::string next_start_message_index = "<unset>";
    int remaining_message_count = 0;
    std::string auth = "disabled";
    std::string signon = "disabled";
    std::string gameplay_transport = "no";
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_resume_ready =
        0;
    int non_exhausted_claimed_checkpoint_successor_resume_token_claim_checkpoint_resume_token_claim_resume_allowed =
        0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_resume_range_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claim_checkpoint_resume_token_claim_resume_range_delivered =
        0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_resume_eof_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claim_checkpoint_resume_token_claim_resume_exhausted =
        0;
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointResumeTokenClaimCheckpointResumeTokenClaimResumeEofProbeSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string probe = "disabled";
    int attempts = 0;
    int accepted = 0;
    int rejected = 0;
    std::string last_reject_reason = "<none>";
    std::string requested_quaternary_claimant_session;
    std::string requested_quinary_claimant_session;
    std::string requested_cursor_id;
    std::string parsed_source_session;
    std::string parsed_issuer_target_session;
    std::string parsed_claimant_session;
    std::string parsed_successor_claimant_session;
    std::string parsed_tertiary_successor_claimant_session;
    std::string parsed_quaternary_claimant_session;
    std::string parsed_quinary_claimant_session;
    std::string parsed_claimant_depth = "quinary";
    std::string parsed_cursor_id;
    bool parsed_eof = false;
    bool parsed_exhausted = false;
    std::string parsed_current_range;
    std::string parsed_next_start_message_index = "<unset>";
    int parsed_remaining_message_count = 0;
    std::string parsed_resume_policy;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_resume_ready =
        0;
    int non_exhausted_claimed_checkpoint_successor_resume_token_claim_checkpoint_resume_token_claim_resume_allowed =
        0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_resume_range_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claim_checkpoint_resume_token_claim_resume_range_delivered =
        0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_resume_eof_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claim_checkpoint_resume_token_claim_resume_exhausted =
        0;
    std::string protocol_shape = "disabled";
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointResumeTokenClaimCheckpointResumeTokenClaimRangeSurfaceSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string bind = "disabled";
    int requested_port = 0;
    int bound_port = 0;
    std::string protocol_shape = "disabled";
    bool successor_checkpoint_resume_token_claim_range_enabled = false;
    bool requires_claimed_successor_checkpoint_resume_token_claim_ready_session =
        true;
    std::string claimant_depth = "quinary";
    std::string fetch_selector = "disabled";
    std::string fetch_payload = "disabled";
    int requested_message_count = 0;
    int accepted = 0;
    int rejected = 0;
    std::string last_source_session;
    std::string last_issuer_target_session;
    std::string last_claimant_session;
    std::string last_successor_claimant_session;
    std::string last_tertiary_successor_claimant_session;
    std::string last_quaternary_claimant_session;
    std::string last_quinary_claimant_session;
    std::string last_checkpoint_cursor_id;
    std::string last_fetched_message_indices;
    std::string next_start_message_index_after_claim_fetch = "<unset>";
    int remaining_message_count_after_claim_fetch = 0;
    std::string auth = "disabled";
    std::string signon = "disabled";
    std::string gameplay_transport = "no";
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claim_checkpoint_resume_token_claimed =
        0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_range_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claim_checkpoint_resume_token_claim_range_delivered =
        0;
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointResumeTokenClaimCheckpointResumeTokenClaimRangeProbeSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string probe = "disabled";
    int attempts = 0;
    int accepted = 0;
    int rejected = 0;
    std::string last_reject_reason = "<none>";
    std::string requested_quaternary_claimant_session;
    std::string requested_quinary_claimant_session;
    int requested_message_count = 0;
    std::string requested_cursor_id;
    std::string parsed_source_session;
    std::string parsed_issuer_target_session;
    std::string parsed_claimant_session;
    std::string parsed_successor_claimant_session;
    std::string parsed_tertiary_successor_claimant_session;
    std::string parsed_quaternary_claimant_session;
    std::string parsed_quinary_claimant_session;
    std::string parsed_claimant_depth = "quinary";
    std::string parsed_cursor_id;
    std::string parsed_fetched_message_indices;
    std::string parsed_semantic_tags;
    int parsed_combined_byte_length = 0;
    std::string parsed_next_start_message_index_after_claim_fetch = "<unset>";
    int parsed_remaining_message_count_after_claim_fetch = 0;
    std::string parsed_claim_purpose;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claim_checkpoint_resume_token_claimed =
        0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_range_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claim_checkpoint_resume_token_claim_range_delivered =
        0;
    std::string protocol_shape = "disabled";
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointResumeTokenClaimCheckpointResumeTokenClaimEofSurfaceSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string bind = "disabled";
    int requested_port = 0;
    int bound_port = 0;
    std::string protocol_shape = "disabled";
    bool successor_checkpoint_resume_token_claim_eof_enabled = false;
    bool requires_claimed_successor_checkpoint_resume_token_claim_range_ready_session =
        true;
    std::string claimant_depth = "quinary";
    int accepted = 0;
    int rejected = 0;
    std::string last_source_session;
    std::string last_issuer_target_session;
    std::string last_claimant_session;
    std::string last_successor_claimant_session;
    std::string last_tertiary_successor_claimant_session;
    std::string last_quaternary_claimant_session;
    std::string last_quinary_claimant_session;
    std::string last_checkpoint_cursor_id;
    bool eof = false;
    bool exhausted = false;
    std::string next_start_message_index = "<unset>";
    int remaining_message_count = 0;
    std::string auth = "disabled";
    std::string signon = "disabled";
    std::string gameplay_transport = "no";
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claim_checkpoint_resume_token_claimed =
        0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_range_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claim_checkpoint_resume_token_claim_range_delivered =
        0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_eof_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claim_checkpoint_resume_token_claim_exhausted =
        0;
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointResumeTokenClaimCheckpointResumeTokenClaimEofProbeSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string probe = "disabled";
    int attempts = 0;
    int accepted = 0;
    int rejected = 0;
    std::string last_reject_reason = "<none>";
    std::string requested_quaternary_claimant_session;
    std::string requested_quinary_claimant_session;
    std::string requested_cursor_id;
    std::string parsed_source_session;
    std::string parsed_issuer_target_session;
    std::string parsed_claimant_session;
    std::string parsed_successor_claimant_session;
    std::string parsed_tertiary_successor_claimant_session;
    std::string parsed_quaternary_claimant_session;
    std::string parsed_quinary_claimant_session;
    std::string parsed_claimant_depth = "quinary";
    std::string parsed_cursor_id;
    bool parsed_eof = false;
    bool parsed_exhausted = false;
    std::string parsed_current_range;
    std::string parsed_next_start_message_index = "<unset>";
    int parsed_remaining_message_count = 0;
    std::string parsed_resume_policy;
    std::string parsed_claim_purpose;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claim_checkpoint_resume_token_claimed =
        0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_range_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claim_checkpoint_resume_token_claim_range_delivered =
        0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_eof_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claim_checkpoint_resume_token_claim_exhausted =
        0;
    std::string protocol_shape = "disabled";
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointResumeTokenClaimCheckpointResumeTokenClaimResumeDenialSurfaceSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string bind = "disabled";
    int requested_port = 0;
    int bound_port = 0;
    std::string protocol_shape = "disabled";
    bool successor_checkpoint_resume_token_claim_resume_denial_enabled = false;
    bool requires_claimed_successor_checkpoint_resume_token_claim_eof_ready_session =
        true;
    std::string claimant_depth = "quinary";
    int accepted = 0;
    int rejected = 0;
    std::string last_source_session;
    std::string last_issuer_target_session;
    std::string last_claimant_session;
    std::string last_successor_claimant_session;
    std::string last_tertiary_successor_claimant_session;
    std::string last_quaternary_claimant_session;
    std::string last_quinary_claimant_session;
    std::string last_denied_cursor_id;
    bool resume_allowed = false;
    std::string denial_reason = "<unset>";
    bool eof = false;
    bool exhausted = false;
    std::string next_start_message_index = "<unset>";
    int remaining_message_count = 0;
    std::string auth = "disabled";
    std::string signon = "disabled";
    std::string gameplay_transport = "no";
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claim_checkpoint_resume_token_claimed =
        0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_range_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claim_checkpoint_resume_token_claim_range_delivered =
        0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_eof_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claim_checkpoint_resume_token_claim_exhausted =
        0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_resume_denied_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claim_checkpoint_resume_token_claim_resume_denied =
        0;
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointResumeTokenClaimCheckpointResumeTokenClaimResumeDenialProbeSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string probe = "disabled";
    int attempts = 0;
    int accepted = 0;
    int rejected = 0;
    std::string last_reject_reason = "<none>";
    std::string requested_quaternary_claimant_session;
    std::string requested_quinary_claimant_session;
    std::string requested_cursor_id;
    std::string parsed_source_session;
    std::string parsed_issuer_target_session;
    std::string parsed_claimant_session;
    std::string parsed_successor_claimant_session;
    std::string parsed_tertiary_successor_claimant_session;
    std::string parsed_quaternary_claimant_session;
    std::string parsed_quinary_claimant_session;
    std::string parsed_claimant_depth = "quinary";
    std::string parsed_cursor_id;
    bool parsed_resume_allowed = false;
    std::string parsed_denial_reason;
    bool parsed_eof = false;
    bool parsed_exhausted = false;
    std::string parsed_next_start_message_index = "<unset>";
    int parsed_remaining_message_count = 0;
    std::string parsed_resume_policy;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claim_checkpoint_resume_token_claimed =
        0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_range_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claim_checkpoint_resume_token_claim_range_delivered =
        0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_eof_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claim_checkpoint_resume_token_claim_exhausted =
        0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_resume_denied_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claim_checkpoint_resume_token_claim_resume_denied =
        0;
    std::string protocol_shape = "disabled";
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointResumeTokenClaimCheckpointResumeRangeSurfaceSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string bind = "disabled";
    int requested_port = 0;
    int bound_port = 0;
    std::string protocol_shape = "disabled";
    bool successor_checkpoint_resume_range_enabled = false;
    bool requires_claimed_successor_checkpoint_resume_ready_session = true;
    std::string fetch_selector = "disabled";
    std::string fetch_payload = "disabled";
    int requested_message_count = 0;
    int accepted = 0;
    int rejected = 0;
    std::string last_source_session;
    std::string last_issuer_target_session;
    std::string last_claimant_session;
    std::string last_successor_claimant_session;
    std::string last_tertiary_successor_claimant_session;
    std::string last_checkpoint_cursor_id;
    std::string last_fetched_message_indices;
    std::string next_start_message_index_after_resume_fetch = "<unset>";
    int remaining_message_count_after_resume_fetch = 0;
    std::string auth = "disabled";
    std::string signon = "disabled";
    std::string gameplay_transport = "no";
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_ready =
        0;
    int non_exhausted_claimed_checkpoint_successor_resume_token_claim_checkpoint_resume_allowed =
        0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claim_checkpoint_resume_token_issued =
        0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_range_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claim_checkpoint_resume_range_delivered =
        0;
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointResumeTokenClaimCheckpointResumeRangeProbeSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string probe = "disabled";
    int attempts = 0;
    int accepted = 0;
    int rejected = 0;
    std::string last_reject_reason = "<none>";
    std::string requested_tertiary_successor_claimant_session;
    int requested_message_count = 0;
    std::string requested_cursor_id;
    std::string parsed_source_session;
    std::string parsed_issuer_target_session;
    std::string parsed_claimant_session;
    std::string parsed_successor_claimant_session;
    std::string parsed_tertiary_successor_claimant_session;
    std::string parsed_cursor_id;
    std::string parsed_fetched_message_indices;
    std::string parsed_semantic_tags;
    int parsed_combined_byte_length = 0;
    std::string parsed_next_start_message_index_after_resume_fetch = "<unset>";
    int parsed_remaining_message_count_after_resume_fetch = 0;
    std::string parsed_resume_policy;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_ready =
        0;
    int non_exhausted_claimed_checkpoint_successor_resume_token_claim_checkpoint_resume_allowed =
        0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claim_checkpoint_resume_token_issued =
        0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_range_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claim_checkpoint_resume_range_delivered =
        0;
    std::string protocol_shape = "disabled";
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointResumeTokenClaimCheckpointResumeEofSurfaceSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string bind = "disabled";
    int requested_port = 0;
    int bound_port = 0;
    std::string protocol_shape = "disabled";
    bool successor_checkpoint_resume_eof_enabled = false;
    bool requires_claimed_successor_checkpoint_resume_range_ready_session = true;
    int accepted = 0;
    int rejected = 0;
    std::string last_source_session;
    std::string last_issuer_target_session;
    std::string last_claimant_session;
    std::string last_successor_claimant_session;
    std::string last_tertiary_successor_claimant_session;
    std::string last_checkpoint_cursor_id;
    bool eof = false;
    bool exhausted = false;
    std::string next_start_message_index = "<unset>";
    int remaining_message_count = 0;
    std::string auth = "disabled";
    std::string signon = "disabled";
    std::string gameplay_transport = "no";
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_ready =
        0;
    int non_exhausted_claimed_checkpoint_successor_resume_token_claim_checkpoint_resume_allowed =
        0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claim_checkpoint_resume_token_issued =
        0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_range_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claim_checkpoint_resume_range_delivered =
        0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_eof_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claim_checkpoint_resume_exhausted =
        0;
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointResumeTokenClaimCheckpointResumeEofProbeSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string probe = "disabled";
    int attempts = 0;
    int accepted = 0;
    int rejected = 0;
    std::string last_reject_reason = "<none>";
    std::string requested_tertiary_successor_claimant_session;
    std::string requested_cursor_id;
    std::string parsed_source_session;
    std::string parsed_issuer_target_session;
    std::string parsed_claimant_session;
    std::string parsed_successor_claimant_session;
    std::string parsed_tertiary_successor_claimant_session;
    std::string parsed_cursor_id;
    bool parsed_eof = false;
    bool parsed_exhausted = false;
    std::string parsed_current_range;
    std::string parsed_next_start_message_index = "<unset>";
    int parsed_remaining_message_count = 0;
    std::string parsed_resume_policy;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_ready =
        0;
    int non_exhausted_claimed_checkpoint_successor_resume_token_claim_checkpoint_resume_allowed =
        0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claim_checkpoint_resume_token_issued =
        0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_range_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claim_checkpoint_resume_range_delivered =
        0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_eof_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claim_checkpoint_resume_exhausted =
        0;
    std::string protocol_shape = "disabled";
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointResumeTokenClaimCheckpointResumedDenialSurfaceSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string bind = "disabled";
    int requested_port = 0;
    int bound_port = 0;
    std::string protocol_shape = "disabled";
    bool successor_checkpoint_resumed_denial_enabled = false;
    bool requires_claimed_successor_checkpoint_resume_eof_ready_session = true;
    int accepted = 0;
    int rejected = 0;
    std::string last_source_session;
    std::string last_issuer_target_session;
    std::string last_claimant_session;
    std::string last_successor_claimant_session;
    std::string last_tertiary_successor_claimant_session;
    std::string last_denied_cursor_id;
    bool resume_allowed = false;
    std::string denial_reason = "<none>";
    bool eof = false;
    bool exhausted = false;
    std::string next_start_message_index = "<unset>";
    int remaining_message_count = 0;
    std::string auth = "disabled";
    std::string signon = "disabled";
    std::string gameplay_transport = "no";
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_ready =
        0;
    int non_exhausted_claimed_checkpoint_successor_resume_token_claim_checkpoint_resume_allowed =
        0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claim_checkpoint_resume_token_issued =
        0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_range_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claim_checkpoint_resume_range_delivered =
        0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_eof_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claim_checkpoint_resume_exhausted =
        0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resumed_denied_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claim_checkpoint_resumed_denied =
        0;
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointResumeTokenClaimCheckpointResumedDenialProbeSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string probe = "disabled";
    int attempts = 0;
    int accepted = 0;
    int rejected = 0;
    std::string last_reject_reason = "<none>";
    std::string requested_tertiary_successor_claimant_session;
    std::string requested_cursor_id;
    std::string parsed_source_session;
    std::string parsed_issuer_target_session;
    std::string parsed_claimant_session;
    std::string parsed_successor_claimant_session;
    std::string parsed_tertiary_successor_claimant_session;
    std::string parsed_cursor_id;
    bool parsed_resume_allowed = false;
    std::string parsed_denial_reason;
    bool parsed_eof = false;
    bool parsed_exhausted = false;
    std::string parsed_next_start_message_index = "<unset>";
    int parsed_remaining_message_count = 0;
    std::string parsed_resume_policy;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_ready =
        0;
    int non_exhausted_claimed_checkpoint_successor_resume_token_claim_checkpoint_resume_allowed =
        0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claim_checkpoint_resume_token_issued =
        0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_range_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claim_checkpoint_resume_range_delivered =
        0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_eof_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claim_checkpoint_resume_exhausted =
        0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resumed_denied_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claim_checkpoint_resumed_denied =
        0;
    std::string protocol_shape = "disabled";
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointResumeTokenClaimCheckpointAdvanceSurfaceSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string bind = "disabled";
    int requested_port = 0;
    int bound_port = 0;
    std::string protocol_shape = "disabled";
    bool successor_checkpoint_resume_token_claim_checkpoint_advance_enabled = false;
    bool requires_claimed_successor_checkpoint_resume_token_claim_checkpoint_bridge_ready_session =
        true;
    std::string fetch_selector = "disabled";
    std::string fetch_payload = "disabled";
    int requested_message_count = 0;
    int accepted = 0;
    int rejected = 0;
    std::string last_source_session;
    std::string last_issuer_target_session;
    std::string last_claimant_session;
    std::string last_successor_claimant_session;
    std::string last_tertiary_successor_claimant_session;
    std::string last_checkpoint_cursor_id;
    std::string last_fetched_message_indices;
    std::string next_start_message_index_after_advance = "<unset>";
    int remaining_message_count_after_advance = 0;
    std::string auth = "disabled";
    std::string signon = "disabled";
    std::string gameplay_transport = "no";
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_bridge_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claim_bridge_materialized = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_advance_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claim_advanced_range_delivered =
        0;
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointResumeTokenClaimCheckpointAdvanceProbeSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string probe = "disabled";
    int attempts = 0;
    int accepted = 0;
    int rejected = 0;
    std::string last_reject_reason = "<none>";
    std::string requested_tertiary_successor_claimant_session;
    int requested_message_count = 0;
    std::string requested_cursor_id;
    std::string parsed_source_session;
    std::string parsed_issuer_target_session;
    std::string parsed_claimant_session;
    std::string parsed_successor_claimant_session;
    std::string parsed_tertiary_successor_claimant_session;
    std::string parsed_cursor_id;
    std::string parsed_fetched_message_indices;
    std::string parsed_semantic_tags;
    int parsed_combined_byte_length = 0;
    std::string parsed_next_start_message_index_after_advance = "<unset>";
    int parsed_remaining_message_count_after_advance = 0;
    std::string parsed_bridge_policy;
    int parsed_target_activated = 0;
    std::string parsed_map;
    std::string parsed_name;
    std::string parsed_ruleset;
    int parsed_spawned = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_bridge_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claim_bridge_materialized = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_advance_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claim_advanced_range_delivered =
        0;
    std::string protocol_shape = "disabled";
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointResumeTokenClaimCheckpointEofSurfaceSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string bind = "disabled";
    int requested_port = 0;
    int bound_port = 0;
    std::string protocol_shape = "disabled";
    bool successor_checkpoint_resume_token_claim_checkpoint_eof_enabled = false;
    bool requires_claimed_successor_checkpoint_resume_token_claim_checkpoint_advance_ready_session =
        true;
    int accepted = 0;
    int rejected = 0;
    std::string last_source_session;
    std::string last_issuer_target_session;
    std::string last_claimant_session;
    std::string last_successor_claimant_session;
    std::string last_tertiary_successor_claimant_session;
    std::string last_checkpoint_cursor_id;
    bool eof = false;
    bool exhausted = false;
    std::string next_start_message_index = "<unset>";
    int remaining_message_count = 0;
    std::string auth = "disabled";
    std::string signon = "disabled";
    std::string gameplay_transport = "no";
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_bridge_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claim_bridge_materialized = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_advance_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claim_advanced_range_delivered =
        0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_eof_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claim_bridge_exhausted = 0;
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointResumeTokenClaimCheckpointEofProbeSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string probe = "disabled";
    int attempts = 0;
    int accepted = 0;
    int rejected = 0;
    std::string last_reject_reason = "<none>";
    std::string requested_tertiary_successor_claimant_session;
    std::string requested_cursor_id;
    std::string parsed_source_session;
    std::string parsed_issuer_target_session;
    std::string parsed_claimant_session;
    std::string parsed_successor_claimant_session;
    std::string parsed_tertiary_successor_claimant_session;
    std::string parsed_cursor_id;
    bool parsed_eof = false;
    bool parsed_exhausted = false;
    std::string parsed_current_range;
    std::string parsed_next_start_message_index = "<unset>";
    int parsed_remaining_message_count = 0;
    std::string parsed_resume_policy;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_bridge_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claim_bridge_materialized = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_advance_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claim_advanced_range_delivered =
        0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_eof_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claim_bridge_exhausted = 0;
    std::string protocol_shape = "disabled";
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointResumeTokenClaimCheckpointResumeDenialSurfaceSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string bind = "disabled";
    int requested_port = 0;
    int bound_port = 0;
    std::string protocol_shape = "disabled";
    bool successor_checkpoint_resume_token_claim_checkpoint_resume_denial_enabled = false;
    bool requires_claimed_successor_checkpoint_resume_token_claim_checkpoint_eof_ready_session =
        true;
    int accepted = 0;
    int rejected = 0;
    std::string last_source_session;
    std::string last_issuer_target_session;
    std::string last_claimant_session;
    std::string last_successor_claimant_session;
    std::string last_tertiary_successor_claimant_session;
    std::string last_denied_cursor_id;
    bool resume_allowed = false;
    std::string denial_reason = "<none>";
    bool eof = false;
    bool exhausted = false;
    std::string next_start_message_index = "<unset>";
    int remaining_message_count = 0;
    std::string auth = "disabled";
    std::string signon = "disabled";
    std::string gameplay_transport = "no";
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_bridge_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claim_bridge_materialized = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_advance_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claim_advanced_range_delivered =
        0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_eof_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claim_bridge_exhausted = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_denied_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claim_bridge_resume_denied = 0;
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointResumeTokenClaimCheckpointResumeDenialProbeSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string probe = "disabled";
    int attempts = 0;
    int accepted = 0;
    int rejected = 0;
    std::string last_reject_reason = "<none>";
    std::string requested_tertiary_successor_claimant_session;
    std::string requested_cursor_id;
    std::string parsed_source_session;
    std::string parsed_issuer_target_session;
    std::string parsed_claimant_session;
    std::string parsed_successor_claimant_session;
    std::string parsed_tertiary_successor_claimant_session;
    std::string parsed_cursor_id;
    bool parsed_resume_allowed = false;
    std::string parsed_denial_reason;
    bool parsed_eof = false;
    bool parsed_exhausted = false;
    std::string parsed_next_start_message_index = "<unset>";
    int parsed_remaining_message_count = 0;
    std::string parsed_resume_policy;
    int parsed_target_activated = 0;
    std::string parsed_map;
    std::string parsed_name;
    std::string parsed_ruleset;
    int parsed_spawned = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_bridge_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claim_bridge_materialized = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_advance_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claim_advanced_range_delivered =
        0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_eof_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claim_bridge_exhausted = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_denied_ready =
        0;
    int claimed_checkpoint_successor_resume_token_claim_bridge_resume_denied = 0;
    std::string protocol_shape = "disabled";
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointResumeRangeSurfaceSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string bind = "disabled";
    int requested_port = 0;
    int bound_port = 0;
    std::string protocol_shape = "disabled";
    bool successor_checkpoint_resume_range_enabled = false;
    bool requires_claimed_checkpoint_successor_checkpoint_resume_ready_session = true;
    std::string fetch_selector = "disabled";
    std::string fetch_payload = "disabled";
    int requested_message_count = 0;
    int accepted = 0;
    int rejected = 0;
    std::string last_source_session;
    std::string last_issuer_target_session;
    std::string last_claimant_session;
    std::string last_successor_claimant_session;
    std::string last_checkpoint_cursor_id;
    std::string last_fetched_message_indices;
    std::string next_start_message_index_after_resume_fetch = "<unset>";
    int remaining_message_count_after_resume_fetch = 0;
    std::string auth = "disabled";
    std::string signon = "disabled";
    std::string gameplay_transport = "no";
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_ready =
        0;
    int non_exhausted_claimed_checkpoint_successor_checkpoint_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_range_ready =
        0;
    int claimed_checkpoint_successor_checkpoint_resume_range_delivered = 0;
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointResumeRangeProbeSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string probe = "disabled";
    int attempts = 0;
    int accepted = 0;
    int rejected = 0;
    std::string last_reject_reason = "<none>";
    std::string requested_successor_claimant_session;
    int requested_message_count = 0;
    std::string requested_cursor_id;
    std::string parsed_source_session;
    std::string parsed_issuer_target_session;
    std::string parsed_claimant_session;
    std::string parsed_successor_claimant_session;
    std::string parsed_cursor_id;
    std::string parsed_fetched_message_indices;
    std::string parsed_semantic_tags;
    int parsed_combined_byte_length = 0;
    std::string parsed_next_start_message_index_after_resume_fetch = "<unset>";
    int parsed_remaining_message_count_after_resume_fetch = 0;
    std::string parsed_resume_policy;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_ready =
        0;
    int non_exhausted_claimed_checkpoint_successor_checkpoint_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_ready =
        0;
    int claimed_checkpoint_successor_resume_token_issued = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_range_ready =
        0;
    int claimed_checkpoint_successor_checkpoint_resume_range_delivered = 0;
    std::string protocol_shape = "disabled";
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointResumeEofSurfaceSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string bind = "disabled";
    int requested_port = 0;
    int bound_port = 0;
    std::string protocol_shape = "disabled";
    bool successor_checkpoint_resume_eof_enabled = false;
    bool requires_claimed_checkpoint_successor_checkpoint_resume_range_ready_session = true;
    std::string eof_payload = "disabled";
    int accepted = 0;
    int rejected = 0;
    std::string last_source_session;
    std::string last_issuer_target_session;
    std::string last_claimant_session;
    std::string last_successor_claimant_session;
    std::string last_checkpoint_cursor_id;
    bool eof = false;
    bool exhausted = false;
    int current_start_message_index = -1;
    int current_message_count = 0;
    std::string next_start_message_index = "<unset>";
    int remaining_message_count = 0;
    std::string auth = "disabled";
    std::string signon = "disabled";
    std::string gameplay_transport = "no";
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_ready =
        0;
    int non_exhausted_claimed_checkpoint_successor_checkpoint_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_range_ready =
        0;
    int claimed_checkpoint_successor_checkpoint_resume_range_delivered = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_eof_ready =
        0;
    int claimed_checkpoint_successor_checkpoint_resume_exhausted = 0;
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointResumeEofProbeSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string probe = "disabled";
    int attempts = 0;
    int accepted = 0;
    int rejected = 0;
    std::string last_reject_reason = "<none>";
    std::string requested_successor_claimant_session;
    std::string requested_cursor_id;
    std::string parsed_source_session;
    std::string parsed_issuer_target_session;
    std::string parsed_claimant_session;
    std::string parsed_successor_claimant_session;
    std::string parsed_cursor_id;
    bool parsed_eof = false;
    bool parsed_exhausted = false;
    std::string parsed_current_range;
    std::string parsed_next_start_message_index = "<unset>";
    int parsed_remaining_message_count = 0;
    std::string parsed_resume_policy;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_ready =
        0;
    int non_exhausted_claimed_checkpoint_successor_checkpoint_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_range_ready =
        0;
    int claimed_checkpoint_successor_checkpoint_resume_range_delivered = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_eof_ready =
        0;
    int claimed_checkpoint_successor_checkpoint_resume_exhausted = 0;
    std::string protocol_shape = "disabled";
    std::string compatibility = "disabled";
    std::string detail;
};

struct
    DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointResumedDenialSurfaceSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string bind = "disabled";
    int requested_port = 0;
    int bound_port = 0;
    std::string protocol_shape = "disabled";
    bool successor_checkpoint_resumed_denial_enabled = false;
    bool requires_claimed_checkpoint_successor_checkpoint_resume_eof_ready_session = true;
    std::string denial_payload = "disabled";
    int accepted = 0;
    int rejected = 0;
    std::string last_source_session;
    std::string last_issuer_target_session;
    std::string last_claimant_session;
    std::string last_successor_claimant_session;
    std::string last_denied_cursor_id;
    bool resume_allowed = false;
    std::string denial_reason = "<unset>";
    bool eof = false;
    bool exhausted = false;
    int current_start_message_index = -1;
    int current_message_count = 0;
    std::string next_start_message_index = "<unset>";
    int remaining_message_count = 0;
    std::string auth = "disabled";
    std::string signon = "disabled";
    std::string gameplay_transport = "no";
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_ready =
        0;
    int non_exhausted_claimed_checkpoint_successor_checkpoint_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_range_ready =
        0;
    int claimed_checkpoint_successor_checkpoint_resume_range_delivered = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_eof_ready =
        0;
    int claimed_checkpoint_successor_checkpoint_resume_exhausted = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resumed_denied_ready =
        0;
    int claimed_checkpoint_successor_checkpoint_resumed_denied = 0;
    std::string compatibility = "disabled";
    std::string detail;
};

struct
    DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointResumedDenialProbeSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string probe = "disabled";
    int attempts = 0;
    int accepted = 0;
    int rejected = 0;
    std::string last_reject_reason = "<none>";
    std::string requested_successor_claimant_session;
    std::string requested_cursor_id;
    std::string parsed_source_session;
    std::string parsed_issuer_target_session;
    std::string parsed_claimant_session;
    std::string parsed_successor_claimant_session;
    std::string parsed_cursor_id;
    bool parsed_resume_allowed = false;
    std::string parsed_denial_reason;
    bool parsed_eof = false;
    bool parsed_exhausted = false;
    std::string parsed_next_start_message_index = "<unset>";
    int parsed_remaining_message_count = 0;
    std::string parsed_resume_policy;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_ready =
        0;
    int non_exhausted_claimed_checkpoint_successor_checkpoint_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_range_ready =
        0;
    int claimed_checkpoint_successor_checkpoint_resume_range_delivered = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_eof_ready =
        0;
    int claimed_checkpoint_successor_checkpoint_resume_exhausted = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resumed_denied_ready =
        0;
    int claimed_checkpoint_successor_checkpoint_resumed_denied = 0;
    std::string protocol_shape = "disabled";
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointAdvanceSurfaceSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string bind = "disabled";
    int requested_port = 0;
    int bound_port = 0;
    std::string protocol_shape = "disabled";
    bool successor_checkpoint_advance_enabled = false;
    bool requires_claimed_checkpoint_successor_checkpoint_bridge_ready_session = true;
    std::string fetch_selector = "disabled";
    std::string fetch_payload = "disabled";
    int requested_message_count = 0;
    int accepted = 0;
    int rejected = 0;
    std::string last_source_session;
    std::string last_issuer_target_session;
    std::string last_claimant_session;
    std::string last_successor_claimant_session;
    std::string last_checkpoint_cursor_id;
    std::string last_fetched_message_indices;
    std::string next_start_message_index_after_advance = "<unset>";
    int remaining_message_count_after_advance = 0;
    std::string auth = "disabled";
    std::string signon = "disabled";
    std::string gameplay_transport = "no";
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_bridge_ready =
        0;
    int claimed_checkpoint_successor_bridge_materialized = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_advance_ready =
        0;
    int claimed_checkpoint_successor_advanced_range_delivered = 0;
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointAdvanceProbeSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string probe = "disabled";
    int attempts = 0;
    int accepted = 0;
    int rejected = 0;
    std::string last_reject_reason = "<none>";
    std::string requested_successor_claimant_session;
    int requested_message_count = 0;
    std::string requested_cursor_id;
    std::string parsed_source_session;
    std::string parsed_issuer_target_session;
    std::string parsed_claimant_session;
    std::string parsed_successor_claimant_session;
    std::string parsed_cursor_id;
    std::string parsed_fetched_message_indices;
    std::string parsed_semantic_tags;
    int parsed_combined_byte_length = 0;
    std::string parsed_next_start_message_index_after_advance = "<unset>";
    int parsed_remaining_message_count_after_advance = 0;
    std::string parsed_bridge_policy;
    int parsed_target_activated = 0;
    std::string parsed_map;
    std::string parsed_name;
    std::string parsed_ruleset;
    int parsed_spawned = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_bridge_ready =
        0;
    int claimed_checkpoint_successor_bridge_materialized = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_advance_ready =
        0;
    int claimed_checkpoint_successor_advanced_range_delivered = 0;
    std::string protocol_shape = "disabled";
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointEofSurfaceSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string bind = "disabled";
    int requested_port = 0;
    int bound_port = 0;
    std::string protocol_shape = "disabled";
    bool successor_checkpoint_eof_enabled = false;
    bool requires_claimed_checkpoint_successor_checkpoint_advance_ready_session = true;
    std::string eof_payload = "disabled";
    int accepted = 0;
    int rejected = 0;
    std::string last_source_session;
    std::string last_issuer_target_session;
    std::string last_claimant_session;
    std::string last_successor_claimant_session;
    std::string last_checkpoint_cursor_id;
    bool eof = false;
    bool exhausted = false;
    int current_start_message_index = -1;
    int current_message_count = 0;
    std::string next_start_message_index = "<unset>";
    int remaining_message_count = 0;
    std::string auth = "disabled";
    std::string signon = "disabled";
    std::string gameplay_transport = "no";
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_bridge_ready =
        0;
    int claimed_checkpoint_successor_bridge_materialized = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_advance_ready =
        0;
    int claimed_checkpoint_successor_advanced_range_delivered = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_eof_ready =
        0;
    int claimed_checkpoint_successor_bridge_exhausted = 0;
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointEofProbeSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string probe = "disabled";
    int attempts = 0;
    int accepted = 0;
    int rejected = 0;
    std::string last_reject_reason = "<none>";
    std::string requested_successor_claimant_session;
    std::string requested_cursor_id;
    std::string parsed_source_session;
    std::string parsed_issuer_target_session;
    std::string parsed_claimant_session;
    std::string parsed_successor_claimant_session;
    std::string parsed_cursor_id;
    bool parsed_eof = false;
    bool parsed_exhausted = false;
    std::string parsed_current_range;
    std::string parsed_next_start_message_index = "<unset>";
    int parsed_remaining_message_count = 0;
    std::string parsed_resume_policy;
    int parsed_target_activated = 0;
    std::string parsed_map;
    std::string parsed_name;
    std::string parsed_ruleset;
    int parsed_spawned = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_bridge_ready =
        0;
    int claimed_checkpoint_successor_bridge_materialized = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_advance_ready =
        0;
    int claimed_checkpoint_successor_advanced_range_delivered = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_eof_ready =
        0;
    int claimed_checkpoint_successor_bridge_exhausted = 0;
    std::string protocol_shape = "disabled";
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointResumeDenialSurfaceSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string bind = "disabled";
    int requested_port = 0;
    int bound_port = 0;
    std::string protocol_shape = "disabled";
    bool successor_checkpoint_resume_denial_enabled = false;
    bool requires_claimed_checkpoint_successor_checkpoint_eof_ready_session = true;
    int accepted = 0;
    int rejected = 0;
    std::string last_source_session;
    std::string last_issuer_target_session;
    std::string last_claimant_session;
    std::string last_successor_claimant_session;
    std::string last_denied_cursor_id;
    bool resume_allowed = false;
    std::string denial_reason;
    bool eof = false;
    bool exhausted = false;
    int current_start_message_index = -1;
    int current_message_count = 0;
    std::string next_start_message_index = "<unset>";
    int remaining_message_count = 0;
    std::string auth = "disabled";
    std::string signon = "disabled";
    std::string gameplay_transport = "no";
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_bridge_ready =
        0;
    int claimed_checkpoint_successor_bridge_materialized = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_advance_ready =
        0;
    int claimed_checkpoint_successor_advanced_range_delivered = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_eof_ready =
        0;
    int claimed_checkpoint_successor_bridge_exhausted = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_denied_ready =
        0;
    int claimed_checkpoint_successor_bridge_resume_denied = 0;
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointResumeDenialProbeSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string probe = "disabled";
    int attempts = 0;
    int accepted = 0;
    int rejected = 0;
    std::string last_reject_reason = "<none>";
    std::string requested_successor_claimant_session;
    std::string requested_cursor_id;
    std::string parsed_source_session;
    std::string parsed_issuer_target_session;
    std::string parsed_claimant_session;
    std::string parsed_successor_claimant_session;
    std::string parsed_cursor_id;
    bool parsed_resume_allowed = false;
    std::string parsed_denial_reason;
    bool parsed_eof = false;
    bool parsed_exhausted = false;
    std::string parsed_current_range;
    std::string parsed_next_start_message_index = "<unset>";
    int parsed_remaining_message_count = 0;
    std::string parsed_resume_policy;
    int parsed_target_activated = 0;
    std::string parsed_map;
    std::string parsed_name;
    std::string parsed_ruleset;
    int parsed_spawned = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_bridge_ready =
        0;
    int claimed_checkpoint_successor_bridge_materialized = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_advance_ready =
        0;
    int claimed_checkpoint_successor_advanced_range_delivered = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_eof_ready =
        0;
    int claimed_checkpoint_successor_bridge_exhausted = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_denied_ready =
        0;
    int claimed_checkpoint_successor_bridge_resume_denied = 0;
    std::string protocol_shape = "disabled";
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorResumeRangeSurfaceSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string bind = "disabled";
    int requested_port = 0;
    int bound_port = 0;
    std::string protocol_shape = "disabled";
    bool successor_resume_range_enabled = false;
    bool requires_claimed_checkpoint_successor_resume_ready_session = true;
    std::string fetch_selector = "disabled";
    std::string fetch_payload = "disabled";
    int requested_message_count = 0;
    int accepted = 0;
    int rejected = 0;
    std::string last_source_session;
    std::string last_issuer_target_session;
    std::string last_claimant_session;
    std::string last_successor_claimant_session;
    std::string last_checkpoint_cursor_id;
    std::string last_fetched_message_indices;
    std::string next_start_message_index_after_resume_fetch = "<unset>";
    int remaining_message_count_after_resume_fetch = 0;
    std::string auth = "disabled";
    std::string signon = "disabled";
    std::string gameplay_transport = "no";
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_resume_ready = 0;
    int non_exhausted_claimed_checkpoint_successor_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_bridge_ready =
        0;
    int claimed_checkpoint_successor_bridge_materialized = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_resume_range_ready =
        0;
    int claimed_checkpoint_successor_resume_range_delivered = 0;
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorResumeRangeProbeSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string probe = "disabled";
    int attempts = 0;
    int accepted = 0;
    int rejected = 0;
    std::string last_reject_reason = "<none>";
    std::string requested_successor_claimant_session;
    int requested_message_count = 0;
    std::string requested_cursor_id;
    std::string parsed_source_session;
    std::string parsed_issuer_target_session;
    std::string parsed_claimant_session;
    std::string parsed_successor_claimant_session;
    std::string parsed_cursor_id;
    std::string parsed_fetched_message_indices;
    std::string parsed_semantic_tags;
    int parsed_combined_byte_length = 0;
    std::string parsed_next_start_message_index_after_resume_fetch = "<unset>";
    int parsed_remaining_message_count_after_resume_fetch = 0;
    std::string parsed_resume_policy;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_resume_ready = 0;
    int non_exhausted_claimed_checkpoint_successor_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_resume_range_ready =
        0;
    int claimed_checkpoint_successor_resume_range_delivered = 0;
    std::string protocol_shape = "disabled";
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorResumeEofSurfaceSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string bind = "disabled";
    int requested_port = 0;
    int bound_port = 0;
    std::string protocol_shape = "disabled";
    bool successor_resume_eof_enabled = false;
    bool requires_claimed_checkpoint_successor_resume_range_ready_session = true;
    std::string eof_payload = "disabled";
    int accepted = 0;
    int rejected = 0;
    std::string last_source_session;
    std::string last_issuer_target_session;
    std::string last_claimant_session;
    std::string last_successor_claimant_session;
    std::string last_checkpoint_cursor_id;
    bool eof = false;
    bool exhausted = false;
    int current_start_message_index = -1;
    int current_message_count = 0;
    std::string next_start_message_index = "<unset>";
    int remaining_message_count = 0;
    std::string auth = "disabled";
    std::string signon = "disabled";
    std::string gameplay_transport = "no";
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_resume_ready = 0;
    int non_exhausted_claimed_checkpoint_successor_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_resume_range_ready =
        0;
    int claimed_checkpoint_successor_resume_range_delivered = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_resume_eof_ready =
        0;
    int claimed_checkpoint_successor_resume_exhausted = 0;
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorResumeEofProbeSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string probe = "disabled";
    int attempts = 0;
    int accepted = 0;
    int rejected = 0;
    std::string last_reject_reason = "<none>";
    std::string requested_successor_claimant_session;
    std::string requested_cursor_id;
    std::string parsed_source_session;
    std::string parsed_issuer_target_session;
    std::string parsed_claimant_session;
    std::string parsed_successor_claimant_session;
    std::string parsed_cursor_id;
    bool parsed_eof = false;
    bool parsed_exhausted = false;
    std::string parsed_current_range;
    std::string parsed_next_start_message_index = "<unset>";
    int parsed_remaining_message_count = 0;
    std::string parsed_resume_policy;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_resume_ready = 0;
    int non_exhausted_claimed_checkpoint_successor_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_resume_range_ready =
        0;
    int claimed_checkpoint_successor_resume_range_delivered = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_resume_eof_ready =
        0;
    int claimed_checkpoint_successor_resume_exhausted = 0;
    std::string protocol_shape = "disabled";
    std::string compatibility = "disabled";
    std::string detail;
};

struct
    DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorResumedDenialSurfaceSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string bind = "disabled";
    int requested_port = 0;
    int bound_port = 0;
    std::string protocol_shape = "disabled";
    bool successor_resumed_denial_enabled = false;
    bool requires_claimed_checkpoint_successor_resume_eof_ready_session = true;
    std::string denial_payload = "disabled";
    int accepted = 0;
    int rejected = 0;
    std::string last_source_session;
    std::string last_issuer_target_session;
    std::string last_claimant_session;
    std::string last_successor_claimant_session;
    std::string last_denied_cursor_id;
    bool resume_allowed = false;
    std::string denial_reason = "<unset>";
    bool eof = false;
    bool exhausted = false;
    int current_start_message_index = -1;
    int current_message_count = 0;
    std::string next_start_message_index = "<unset>";
    int remaining_message_count = 0;
    std::string auth = "disabled";
    std::string signon = "disabled";
    std::string gameplay_transport = "no";
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_resume_ready = 0;
    int non_exhausted_claimed_checkpoint_successor_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_resume_range_ready =
        0;
    int claimed_checkpoint_successor_resume_range_delivered = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_resume_eof_ready =
        0;
    int claimed_checkpoint_successor_resume_exhausted = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_resumed_denied_ready =
        0;
    int claimed_checkpoint_successor_resumed_denied = 0;
    std::string compatibility = "disabled";
    std::string detail;
};

struct
    DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorResumedDenialProbeSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string probe = "disabled";
    int attempts = 0;
    int accepted = 0;
    int rejected = 0;
    std::string last_reject_reason = "<none>";
    std::string requested_successor_claimant_session;
    std::string requested_cursor_id;
    std::string parsed_source_session;
    std::string parsed_issuer_target_session;
    std::string parsed_claimant_session;
    std::string parsed_successor_claimant_session;
    std::string parsed_cursor_id;
    bool parsed_resume_allowed = false;
    std::string parsed_denial_reason;
    bool parsed_eof = false;
    bool parsed_exhausted = false;
    std::string parsed_next_start_message_index = "<unset>";
    int parsed_remaining_message_count = 0;
    std::string parsed_resume_policy;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_resume_ready = 0;
    int non_exhausted_claimed_checkpoint_successor_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_resume_range_ready =
        0;
    int claimed_checkpoint_successor_resume_range_delivered = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_resume_eof_ready =
        0;
    int claimed_checkpoint_successor_resume_exhausted = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_resumed_denied_ready =
        0;
    int claimed_checkpoint_successor_resumed_denied = 0;
    std::string protocol_shape = "disabled";
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorRangeSurfaceSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string bind = "disabled";
    int requested_port = 0;
    int bound_port = 0;
    std::string protocol_shape = "disabled";
    bool successor_range_enabled = false;
    bool requires_claimed_checkpoint_resume_token_claim_ready_session = true;
    std::string fetch_selector = "disabled";
    std::string fetch_payload = "disabled";
    int requested_message_count = 0;
    int accepted = 0;
    int rejected = 0;
    std::string last_source_session;
    std::string last_issuer_target_session;
    std::string last_claimant_session;
    std::string last_successor_claimant_session;
    std::string last_checkpoint_cursor_id;
    std::string last_fetched_message_indices;
    std::string next_start_message_index_after_successor_fetch = "<unset>";
    int remaining_message_count_after_successor_fetch = 0;
    std::string auth = "disabled";
    std::string signon = "disabled";
    std::string gameplay_transport = "no";
    int signon_ready = 0;
    int bootstrap_delivered = 0;
    int baseline_ready = 0;
    int bootstrap_sequence_completed = 0;
    int signon_catalog_ready = 0;
    int bootstrap_records_staged = 0;
    int signon_template_ready = 0;
    int template_records_delivered = 0;
    int signon_template_coverage_complete = 0;
    int remaining_template_records_delivered = 0;
    int signon_envelope_ready = 0;
    int framed_template_records_delivered = 0;
    int signon_batch_ready = 0;
    int pseudo_packet_batch_delivered = 0;
    int signon_wiremap_ready = 0;
    int wiremapped_batch_delivered = 0;
    int signon_burst_ready = 0;
    int pseudo_wire_burst_delivered = 0;
    int signon_stream_ready = 0;
    int contiguous_stream_delivered = 0;
    int signon_stream_window_ready = 0;
    int windowed_stream_delivered = 0;
    int signon_message_catalog_ready = 0;
    int message_boundaries_cataloged = 0;
    int signon_message_fetch_ready = 0;
    int targeted_message_fetch_delivered = 0;
    int signon_multi_message_fetch_ready = 0;
    int selective_message_set_delivered = 0;
    int signon_message_range_fetch_ready = 0;
    int range_message_set_delivered = 0;
    int signon_message_cursor_ready = 0;
    int message_cursor_checkpointed = 0;
    int signon_message_cursor_resume_ready = 0;
    int non_exhausted_cursor_resume_allowed = 0;
    int signon_message_cursor_carryover_ready = 0;
    int checkpoint_cursor_carried_over = 0;
    int signon_message_cursor_carried_resume_ready = 0;
    int non_exhausted_carried_cursor_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_ready = 0;
    int carried_cursor_checkpointed = 0;
    int signon_message_cursor_carried_checkpoint_resume_ready = 0;
    int non_exhausted_carried_checkpoint_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_resume_token_ready = 0;
    int carried_checkpoint_resume_token_issued = 0;
    int signon_message_cursor_carried_checkpoint_resume_token_claim_ready = 0;
    int carried_checkpoint_resume_token_claimed = 0;
    int signon_message_cursor_carried_checkpoint_claimed_resume_ready = 0;
    int non_exhausted_claimed_checkpoint_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_bridge_ready = 0;
    int claimed_checkpoint_bridge_materialized = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_resume_ready = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_resume_token_ready = 0;
    int claimed_checkpoint_resume_token_issued = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_resume_token_claim_ready = 0;
    int claimed_checkpoint_resume_token_claimed = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_range_ready = 0;
    int claimed_checkpoint_successor_range_delivered = 0;
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorRangeProbeSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string probe = "disabled";
    int attempts = 0;
    int accepted = 0;
    int rejected = 0;
    std::string last_reject_reason = "<none>";
    std::string requested_successor_claimant_session;
    int requested_message_count = 0;
    std::string requested_cursor_id;
    std::string parsed_source_session;
    std::string parsed_issuer_target_session;
    std::string parsed_claimant_session;
    std::string parsed_successor_claimant_session;
    std::string parsed_cursor_id;
    std::string parsed_fetched_message_indices;
    std::string parsed_semantic_tags;
    int parsed_combined_byte_length = 0;
    std::string parsed_next_start_message_index_after_successor_fetch = "<unset>";
    int parsed_remaining_message_count_after_successor_fetch = 0;
    std::string parsed_claim_purpose;
    int parsed_target_activated = 0;
    std::string parsed_map;
    std::string parsed_name;
    std::string parsed_ruleset;
    int parsed_spawned = 0;
    int signon_ready = 0;
    int bootstrap_delivered = 0;
    int baseline_ready = 0;
    int bootstrap_sequence_completed = 0;
    int signon_catalog_ready = 0;
    int bootstrap_records_staged = 0;
    int signon_template_ready = 0;
    int template_records_delivered = 0;
    int signon_template_coverage_complete = 0;
    int remaining_template_records_delivered = 0;
    int signon_envelope_ready = 0;
    int framed_template_records_delivered = 0;
    int signon_batch_ready = 0;
    int pseudo_packet_batch_delivered = 0;
    int signon_wiremap_ready = 0;
    int wiremapped_batch_delivered = 0;
    int signon_burst_ready = 0;
    int pseudo_wire_burst_delivered = 0;
    int signon_stream_ready = 0;
    int contiguous_stream_delivered = 0;
    int signon_stream_window_ready = 0;
    int windowed_stream_delivered = 0;
    int signon_message_catalog_ready = 0;
    int message_boundaries_cataloged = 0;
    int signon_message_fetch_ready = 0;
    int targeted_message_fetch_delivered = 0;
    int signon_multi_message_fetch_ready = 0;
    int selective_message_set_delivered = 0;
    int signon_message_range_fetch_ready = 0;
    int range_message_set_delivered = 0;
    int signon_message_cursor_ready = 0;
    int message_cursor_checkpointed = 0;
    int signon_message_cursor_resume_ready = 0;
    int non_exhausted_cursor_resume_allowed = 0;
    int signon_message_cursor_carryover_ready = 0;
    int checkpoint_cursor_carried_over = 0;
    int signon_message_cursor_carried_resume_ready = 0;
    int non_exhausted_carried_cursor_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_ready = 0;
    int carried_cursor_checkpointed = 0;
    int signon_message_cursor_carried_checkpoint_resume_ready = 0;
    int non_exhausted_carried_checkpoint_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_resume_token_ready = 0;
    int carried_checkpoint_resume_token_issued = 0;
    int signon_message_cursor_carried_checkpoint_resume_token_claim_ready = 0;
    int carried_checkpoint_resume_token_claimed = 0;
    int signon_message_cursor_carried_checkpoint_claimed_resume_ready = 0;
    int non_exhausted_claimed_checkpoint_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_bridge_ready = 0;
    int claimed_checkpoint_bridge_materialized = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_resume_ready = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_resume_token_ready = 0;
    int claimed_checkpoint_resume_token_issued = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_resume_token_claim_ready = 0;
    int claimed_checkpoint_resume_token_claimed = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_range_ready = 0;
    int claimed_checkpoint_successor_range_delivered = 0;
    std::string protocol_shape = "disabled";
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorEofSurfaceSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string bind = "disabled";
    int requested_port = 0;
    int bound_port = 0;
    std::string protocol_shape = "disabled";
    bool successor_eof_enabled = false;
    bool requires_claimed_checkpoint_resume_token_claim_ready_session = true;
    int accepted = 0;
    int rejected = 0;
    std::string last_source_session;
    std::string last_issuer_target_session;
    std::string last_claimant_session;
    std::string last_successor_claimant_session;
    std::string last_checkpoint_cursor_id;
    bool eof = false;
    bool exhausted = false;
    std::string next_start_message_index = "<unset>";
    int remaining_message_count = 0;
    std::string auth = "disabled";
    std::string signon = "disabled";
    std::string gameplay_transport = "no";
    int signon_ready = 0;
    int bootstrap_delivered = 0;
    int baseline_ready = 0;
    int bootstrap_sequence_completed = 0;
    int signon_catalog_ready = 0;
    int bootstrap_records_staged = 0;
    int signon_template_ready = 0;
    int template_records_delivered = 0;
    int signon_template_coverage_complete = 0;
    int remaining_template_records_delivered = 0;
    int signon_envelope_ready = 0;
    int framed_template_records_delivered = 0;
    int signon_batch_ready = 0;
    int pseudo_packet_batch_delivered = 0;
    int signon_wiremap_ready = 0;
    int wiremapped_batch_delivered = 0;
    int signon_burst_ready = 0;
    int pseudo_wire_burst_delivered = 0;
    int signon_stream_ready = 0;
    int contiguous_stream_delivered = 0;
    int signon_stream_window_ready = 0;
    int windowed_stream_delivered = 0;
    int signon_message_catalog_ready = 0;
    int message_boundaries_cataloged = 0;
    int signon_message_fetch_ready = 0;
    int targeted_message_fetch_delivered = 0;
    int signon_multi_message_fetch_ready = 0;
    int selective_message_set_delivered = 0;
    int signon_message_range_fetch_ready = 0;
    int range_message_set_delivered = 0;
    int signon_message_cursor_ready = 0;
    int message_cursor_checkpointed = 0;
    int signon_message_cursor_resume_ready = 0;
    int non_exhausted_cursor_resume_allowed = 0;
    int signon_message_cursor_carryover_ready = 0;
    int checkpoint_cursor_carried_over = 0;
    int signon_message_cursor_carried_resume_ready = 0;
    int non_exhausted_carried_cursor_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_ready = 0;
    int carried_cursor_checkpointed = 0;
    int signon_message_cursor_carried_checkpoint_resume_ready = 0;
    int non_exhausted_carried_checkpoint_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_resume_token_ready = 0;
    int carried_checkpoint_resume_token_issued = 0;
    int signon_message_cursor_carried_checkpoint_resume_token_claim_ready = 0;
    int carried_checkpoint_resume_token_claimed = 0;
    int signon_message_cursor_carried_checkpoint_claimed_resume_ready = 0;
    int non_exhausted_claimed_checkpoint_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_bridge_ready = 0;
    int claimed_checkpoint_bridge_materialized = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_resume_ready = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_resume_token_ready = 0;
    int claimed_checkpoint_resume_token_issued = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_resume_token_claim_ready = 0;
    int claimed_checkpoint_resume_token_claimed = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_resume_ready = 0;
    int non_exhausted_claimed_checkpoint_successor_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_range_ready = 0;
    int claimed_checkpoint_successor_range_delivered = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_eof_ready = 0;
    int claimed_checkpoint_successor_exhausted = 0;
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorEofProbeSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string probe = "disabled";
    int attempts = 0;
    int accepted = 0;
    int rejected = 0;
    std::string last_reject_reason = "<none>";
    std::string requested_successor_claimant_session;
    std::string requested_cursor_id;
    std::string parsed_source_session;
    std::string parsed_issuer_target_session;
    std::string parsed_claimant_session;
    std::string parsed_successor_claimant_session;
    std::string parsed_cursor_id;
    bool parsed_eof = false;
    bool parsed_exhausted = false;
    std::string parsed_current_range;
    std::string parsed_next_start_message_index = "<unset>";
    int parsed_remaining_message_count = 0;
    std::string parsed_resume_policy;
    int parsed_target_activated = 0;
    std::string parsed_map;
    std::string parsed_name;
    std::string parsed_ruleset;
    int parsed_spawned = 0;
    int signon_ready = 0;
    int bootstrap_delivered = 0;
    int baseline_ready = 0;
    int bootstrap_sequence_completed = 0;
    int signon_catalog_ready = 0;
    int bootstrap_records_staged = 0;
    int signon_template_ready = 0;
    int template_records_delivered = 0;
    int signon_template_coverage_complete = 0;
    int remaining_template_records_delivered = 0;
    int signon_envelope_ready = 0;
    int framed_template_records_delivered = 0;
    int signon_batch_ready = 0;
    int pseudo_packet_batch_delivered = 0;
    int signon_wiremap_ready = 0;
    int wiremapped_batch_delivered = 0;
    int signon_burst_ready = 0;
    int pseudo_wire_burst_delivered = 0;
    int signon_stream_ready = 0;
    int contiguous_stream_delivered = 0;
    int signon_stream_window_ready = 0;
    int windowed_stream_delivered = 0;
    int signon_message_catalog_ready = 0;
    int message_boundaries_cataloged = 0;
    int signon_message_fetch_ready = 0;
    int targeted_message_fetch_delivered = 0;
    int signon_multi_message_fetch_ready = 0;
    int selective_message_set_delivered = 0;
    int signon_message_range_fetch_ready = 0;
    int range_message_set_delivered = 0;
    int signon_message_cursor_ready = 0;
    int message_cursor_checkpointed = 0;
    int signon_message_cursor_resume_ready = 0;
    int non_exhausted_cursor_resume_allowed = 0;
    int signon_message_cursor_carryover_ready = 0;
    int checkpoint_cursor_carried_over = 0;
    int signon_message_cursor_carried_resume_ready = 0;
    int non_exhausted_carried_cursor_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_ready = 0;
    int carried_cursor_checkpointed = 0;
    int signon_message_cursor_carried_checkpoint_resume_ready = 0;
    int non_exhausted_carried_checkpoint_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_resume_token_ready = 0;
    int carried_checkpoint_resume_token_issued = 0;
    int signon_message_cursor_carried_checkpoint_resume_token_claim_ready = 0;
    int carried_checkpoint_resume_token_claimed = 0;
    int signon_message_cursor_carried_checkpoint_claimed_resume_ready = 0;
    int non_exhausted_claimed_checkpoint_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_bridge_ready = 0;
    int claimed_checkpoint_bridge_materialized = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_resume_ready = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_resume_token_ready = 0;
    int claimed_checkpoint_resume_token_issued = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_resume_token_claim_ready = 0;
    int claimed_checkpoint_resume_token_claimed = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_resume_ready = 0;
    int non_exhausted_claimed_checkpoint_successor_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_resume_range_ready =
        0;
    int claimed_checkpoint_successor_resume_range_delivered = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_range_ready = 0;
    int claimed_checkpoint_successor_range_delivered = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_eof_ready = 0;
    int claimed_checkpoint_successor_exhausted = 0;
    std::string protocol_shape = "disabled";
    std::string compatibility = "disabled";
    std::string detail;
};

struct
    DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorResumeDenialSurfaceSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string bind = "disabled";
    int requested_port = 0;
    int bound_port = 0;
    std::string protocol_shape = "disabled";
    bool successor_resume_denial_enabled = false;
    bool requires_claimed_checkpoint_resume_token_claim_ready_session = true;
    int accepted = 0;
    int rejected = 0;
    std::string last_source_session;
    std::string last_issuer_target_session;
    std::string last_claimant_session;
    std::string last_successor_claimant_session;
    std::string last_denied_cursor_id;
    bool resume_allowed = false;
    std::string denial_reason = "<unset>";
    bool eof = false;
    bool exhausted = false;
    int current_start_message_index = -1;
    int current_message_count = 0;
    std::string next_start_message_index = "<unset>";
    int remaining_message_count = 0;
    std::string auth = "disabled";
    std::string signon = "disabled";
    std::string gameplay_transport = "no";
    int signon_ready = 0;
    int bootstrap_delivered = 0;
    int baseline_ready = 0;
    int bootstrap_sequence_completed = 0;
    int signon_catalog_ready = 0;
    int bootstrap_records_staged = 0;
    int signon_template_ready = 0;
    int template_records_delivered = 0;
    int signon_template_coverage_complete = 0;
    int remaining_template_records_delivered = 0;
    int signon_envelope_ready = 0;
    int framed_template_records_delivered = 0;
    int signon_batch_ready = 0;
    int pseudo_packet_batch_delivered = 0;
    int signon_wiremap_ready = 0;
    int wiremapped_batch_delivered = 0;
    int signon_burst_ready = 0;
    int pseudo_wire_burst_delivered = 0;
    int signon_stream_ready = 0;
    int contiguous_stream_delivered = 0;
    int signon_stream_window_ready = 0;
    int windowed_stream_delivered = 0;
    int signon_message_catalog_ready = 0;
    int message_boundaries_cataloged = 0;
    int signon_message_fetch_ready = 0;
    int targeted_message_fetch_delivered = 0;
    int signon_multi_message_fetch_ready = 0;
    int selective_message_set_delivered = 0;
    int signon_message_range_fetch_ready = 0;
    int range_message_set_delivered = 0;
    int signon_message_cursor_ready = 0;
    int message_cursor_checkpointed = 0;
    int signon_message_cursor_resume_ready = 0;
    int non_exhausted_cursor_resume_allowed = 0;
    int signon_message_cursor_carryover_ready = 0;
    int checkpoint_cursor_carried_over = 0;
    int signon_message_cursor_carried_resume_ready = 0;
    int non_exhausted_carried_cursor_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_ready = 0;
    int carried_cursor_checkpointed = 0;
    int signon_message_cursor_carried_checkpoint_resume_ready = 0;
    int non_exhausted_carried_checkpoint_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_resume_token_ready = 0;
    int carried_checkpoint_resume_token_issued = 0;
    int signon_message_cursor_carried_checkpoint_resume_token_claim_ready = 0;
    int carried_checkpoint_resume_token_claimed = 0;
    int signon_message_cursor_carried_checkpoint_claimed_resume_ready = 0;
    int non_exhausted_claimed_checkpoint_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_bridge_ready = 0;
    int claimed_checkpoint_bridge_materialized = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_resume_ready = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_resume_token_ready = 0;
    int claimed_checkpoint_resume_token_issued = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_resume_token_claim_ready = 0;
    int claimed_checkpoint_resume_token_claimed = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_resume_ready = 0;
    int non_exhausted_claimed_checkpoint_successor_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_resume_range_ready = 0;
    int claimed_checkpoint_successor_resume_range_delivered = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_range_ready = 0;
    int claimed_checkpoint_successor_range_delivered = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_eof_ready = 0;
    int claimed_checkpoint_successor_exhausted = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_resume_denied_ready =
        0;
    int claimed_checkpoint_successor_resume_denied = 0;
    std::string compatibility = "disabled";
    std::string detail;
};

struct
    DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorResumeDenialProbeSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string probe = "disabled";
    int attempts = 0;
    int accepted = 0;
    int rejected = 0;
    std::string last_reject_reason = "<none>";
    std::string requested_successor_claimant_session;
    std::string requested_cursor_id;
    std::string parsed_source_session;
    std::string parsed_issuer_target_session;
    std::string parsed_claimant_session;
    std::string parsed_successor_claimant_session;
    std::string parsed_cursor_id;
    bool parsed_resume_allowed = false;
    std::string parsed_denial_reason;
    bool parsed_eof = false;
    bool parsed_exhausted = false;
    std::string parsed_current_range;
    std::string parsed_next_start_message_index = "<unset>";
    int parsed_remaining_message_count = 0;
    std::string parsed_resume_policy;
    int parsed_target_activated = 0;
    std::string parsed_map;
    std::string parsed_name;
    std::string parsed_ruleset;
    int parsed_spawned = 0;
    int signon_ready = 0;
    int bootstrap_delivered = 0;
    int baseline_ready = 0;
    int bootstrap_sequence_completed = 0;
    int signon_catalog_ready = 0;
    int bootstrap_records_staged = 0;
    int signon_template_ready = 0;
    int template_records_delivered = 0;
    int signon_template_coverage_complete = 0;
    int remaining_template_records_delivered = 0;
    int signon_envelope_ready = 0;
    int framed_template_records_delivered = 0;
    int signon_batch_ready = 0;
    int pseudo_packet_batch_delivered = 0;
    int signon_wiremap_ready = 0;
    int wiremapped_batch_delivered = 0;
    int signon_burst_ready = 0;
    int pseudo_wire_burst_delivered = 0;
    int signon_stream_ready = 0;
    int contiguous_stream_delivered = 0;
    int signon_stream_window_ready = 0;
    int windowed_stream_delivered = 0;
    int signon_message_catalog_ready = 0;
    int message_boundaries_cataloged = 0;
    int signon_message_fetch_ready = 0;
    int targeted_message_fetch_delivered = 0;
    int signon_multi_message_fetch_ready = 0;
    int selective_message_set_delivered = 0;
    int signon_message_range_fetch_ready = 0;
    int range_message_set_delivered = 0;
    int signon_message_cursor_ready = 0;
    int message_cursor_checkpointed = 0;
    int signon_message_cursor_resume_ready = 0;
    int non_exhausted_cursor_resume_allowed = 0;
    int signon_message_cursor_carryover_ready = 0;
    int checkpoint_cursor_carried_over = 0;
    int signon_message_cursor_carried_resume_ready = 0;
    int non_exhausted_carried_cursor_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_ready = 0;
    int carried_cursor_checkpointed = 0;
    int signon_message_cursor_carried_checkpoint_resume_ready = 0;
    int non_exhausted_carried_checkpoint_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_resume_token_ready = 0;
    int carried_checkpoint_resume_token_issued = 0;
    int signon_message_cursor_carried_checkpoint_resume_token_claim_ready = 0;
    int carried_checkpoint_resume_token_claimed = 0;
    int signon_message_cursor_carried_checkpoint_claimed_resume_ready = 0;
    int non_exhausted_claimed_checkpoint_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_bridge_ready = 0;
    int claimed_checkpoint_bridge_materialized = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_resume_ready = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_resume_token_ready = 0;
    int claimed_checkpoint_resume_token_issued = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_resume_token_claim_ready = 0;
    int claimed_checkpoint_resume_token_claimed = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_resume_ready = 0;
    int non_exhausted_claimed_checkpoint_successor_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_resume_range_ready = 0;
    int claimed_checkpoint_successor_resume_range_delivered = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_range_ready = 0;
    int claimed_checkpoint_successor_range_delivered = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_eof_ready = 0;
    int claimed_checkpoint_successor_exhausted = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_resume_denied_ready =
        0;
    int claimed_checkpoint_successor_resume_denied = 0;
    std::string protocol_shape = "disabled";
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointResumeRangeSurfaceSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string bind = "disabled";
    int requested_port = 0;
    int bound_port = 0;
    std::string protocol_shape = "disabled";
    bool claimed_checkpoint_resume_range_enabled = false;
    bool requires_claimed_checkpoint_resume_ready_session = true;
    std::string fetch_selector = "disabled";
    std::string fetch_payload = "disabled";
    int requested_message_count = 0;
    int accepted = 0;
    int rejected = 0;
    std::string last_source_session;
    std::string last_issuer_target_session;
    std::string last_claimant_session;
    std::string last_checkpoint_cursor_id;
    std::string last_fetched_message_indices;
    std::string next_start_message_index_after_resume_fetch = "<unset>";
    int remaining_message_count_after_resume_fetch = 0;
    std::string auth = "disabled";
    std::string signon = "disabled";
    std::string gameplay_transport = "no";
    int signon_ready = 0;
    int bootstrap_delivered = 0;
    int baseline_ready = 0;
    int bootstrap_sequence_completed = 0;
    int signon_catalog_ready = 0;
    int bootstrap_records_staged = 0;
    int signon_template_ready = 0;
    int template_records_delivered = 0;
    int signon_template_coverage_complete = 0;
    int remaining_template_records_delivered = 0;
    int signon_envelope_ready = 0;
    int framed_template_records_delivered = 0;
    int signon_batch_ready = 0;
    int pseudo_packet_batch_delivered = 0;
    int signon_wiremap_ready = 0;
    int wiremapped_batch_delivered = 0;
    int signon_burst_ready = 0;
    int pseudo_wire_burst_delivered = 0;
    int signon_stream_ready = 0;
    int contiguous_stream_delivered = 0;
    int signon_stream_window_ready = 0;
    int windowed_stream_delivered = 0;
    int signon_message_catalog_ready = 0;
    int message_boundaries_cataloged = 0;
    int signon_message_fetch_ready = 0;
    int targeted_message_fetch_delivered = 0;
    int signon_multi_message_fetch_ready = 0;
    int selective_message_set_delivered = 0;
    int signon_message_range_fetch_ready = 0;
    int range_message_set_delivered = 0;
    int signon_message_cursor_ready = 0;
    int message_cursor_checkpointed = 0;
    int signon_message_cursor_resume_ready = 0;
    int non_exhausted_cursor_resume_allowed = 0;
    int signon_message_cursor_carryover_ready = 0;
    int checkpoint_cursor_carried_over = 0;
    int signon_message_cursor_carried_resume_ready = 0;
    int non_exhausted_carried_cursor_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_ready = 0;
    int carried_cursor_checkpointed = 0;
    int signon_message_cursor_carried_checkpoint_resume_ready = 0;
    int non_exhausted_carried_checkpoint_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_resume_token_ready = 0;
    int carried_checkpoint_resume_token_issued = 0;
    int signon_message_cursor_carried_checkpoint_resume_token_claim_ready = 0;
    int carried_checkpoint_resume_token_claimed = 0;
    int signon_message_cursor_carried_checkpoint_claimed_resume_ready = 0;
    int non_exhausted_claimed_checkpoint_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_bridge_ready = 0;
    int claimed_checkpoint_bridge_materialized = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_resume_ready = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_resume_range_ready = 0;
    int claimed_checkpoint_bridge_resume_range_delivered = 0;
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointResumeRangeProbeSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string probe = "disabled";
    int attempts = 0;
    int accepted = 0;
    int rejected = 0;
    std::string last_reject_reason = "<none>";
    std::string requested_claimant_session;
    int requested_message_count = 0;
    std::string requested_cursor_id;
    std::string parsed_source_session;
    std::string parsed_issuer_target_session;
    std::string parsed_claimant_session;
    std::string parsed_cursor_id;
    std::string parsed_fetched_message_indices;
    std::string parsed_semantic_tags;
    int parsed_combined_byte_length = 0;
    std::string parsed_next_start_message_index_after_resume_fetch = "<unset>";
    int parsed_remaining_message_count_after_resume_fetch = 0;
    std::string parsed_resume_policy;
    int parsed_target_activated = 0;
    std::string parsed_map;
    std::string parsed_name;
    std::string parsed_ruleset;
    int parsed_spawned = 0;
    int signon_ready = 0;
    int bootstrap_delivered = 0;
    int baseline_ready = 0;
    int bootstrap_sequence_completed = 0;
    int signon_catalog_ready = 0;
    int bootstrap_records_staged = 0;
    int signon_template_ready = 0;
    int template_records_delivered = 0;
    int signon_template_coverage_complete = 0;
    int remaining_template_records_delivered = 0;
    int signon_envelope_ready = 0;
    int framed_template_records_delivered = 0;
    int signon_batch_ready = 0;
    int pseudo_packet_batch_delivered = 0;
    int signon_wiremap_ready = 0;
    int wiremapped_batch_delivered = 0;
    int signon_burst_ready = 0;
    int pseudo_wire_burst_delivered = 0;
    int signon_stream_ready = 0;
    int contiguous_stream_delivered = 0;
    int signon_stream_window_ready = 0;
    int windowed_stream_delivered = 0;
    int signon_message_catalog_ready = 0;
    int message_boundaries_cataloged = 0;
    int signon_message_fetch_ready = 0;
    int targeted_message_fetch_delivered = 0;
    int signon_multi_message_fetch_ready = 0;
    int selective_message_set_delivered = 0;
    int signon_message_range_fetch_ready = 0;
    int range_message_set_delivered = 0;
    int signon_message_cursor_ready = 0;
    int message_cursor_checkpointed = 0;
    int signon_message_cursor_resume_ready = 0;
    int non_exhausted_cursor_resume_allowed = 0;
    int signon_message_cursor_carryover_ready = 0;
    int checkpoint_cursor_carried_over = 0;
    int signon_message_cursor_carried_resume_ready = 0;
    int non_exhausted_carried_cursor_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_ready = 0;
    int carried_cursor_checkpointed = 0;
    int signon_message_cursor_carried_checkpoint_resume_ready = 0;
    int non_exhausted_carried_checkpoint_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_resume_token_ready = 0;
    int carried_checkpoint_resume_token_issued = 0;
    int signon_message_cursor_carried_checkpoint_resume_token_claim_ready = 0;
    int carried_checkpoint_resume_token_claimed = 0;
    int signon_message_cursor_carried_checkpoint_claimed_resume_ready = 0;
    int non_exhausted_claimed_checkpoint_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_bridge_ready = 0;
    int claimed_checkpoint_bridge_materialized = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_resume_ready = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_resume_range_ready = 0;
    int claimed_checkpoint_bridge_resume_range_delivered = 0;
    std::string protocol_shape = "disabled";
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointResumeEofSurfaceSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string bind = "disabled";
    int requested_port = 0;
    int bound_port = 0;
    std::string protocol_shape = "disabled";
    bool claimed_checkpoint_resume_eof_enabled = false;
    bool requires_claimed_checkpoint_resume_range_ready_session = true;
    std::string eof_payload = "disabled";
    int accepted = 0;
    int rejected = 0;
    std::string last_source_session;
    std::string last_issuer_target_session;
    std::string last_claimant_session;
    std::string last_checkpoint_cursor_id;
    bool eof = false;
    bool exhausted = false;
    std::string next_start_message_index = "<unset>";
    int remaining_message_count = 0;
    std::string auth = "disabled";
    std::string signon = "disabled";
    std::string gameplay_transport = "no";
    int signon_ready = 0;
    int bootstrap_delivered = 0;
    int baseline_ready = 0;
    int bootstrap_sequence_completed = 0;
    int signon_catalog_ready = 0;
    int bootstrap_records_staged = 0;
    int signon_template_ready = 0;
    int template_records_delivered = 0;
    int signon_template_coverage_complete = 0;
    int remaining_template_records_delivered = 0;
    int signon_envelope_ready = 0;
    int framed_template_records_delivered = 0;
    int signon_batch_ready = 0;
    int pseudo_packet_batch_delivered = 0;
    int signon_wiremap_ready = 0;
    int wiremapped_batch_delivered = 0;
    int signon_burst_ready = 0;
    int pseudo_wire_burst_delivered = 0;
    int signon_stream_ready = 0;
    int contiguous_stream_delivered = 0;
    int signon_stream_window_ready = 0;
    int windowed_stream_delivered = 0;
    int signon_message_catalog_ready = 0;
    int message_boundaries_cataloged = 0;
    int signon_message_fetch_ready = 0;
    int targeted_message_fetch_delivered = 0;
    int signon_multi_message_fetch_ready = 0;
    int selective_message_set_delivered = 0;
    int signon_message_range_fetch_ready = 0;
    int range_message_set_delivered = 0;
    int signon_message_cursor_ready = 0;
    int message_cursor_checkpointed = 0;
    int signon_message_cursor_resume_ready = 0;
    int non_exhausted_cursor_resume_allowed = 0;
    int signon_message_cursor_carryover_ready = 0;
    int checkpoint_cursor_carried_over = 0;
    int signon_message_cursor_carried_resume_ready = 0;
    int non_exhausted_carried_cursor_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_ready = 0;
    int carried_cursor_checkpointed = 0;
    int signon_message_cursor_carried_checkpoint_resume_ready = 0;
    int non_exhausted_carried_checkpoint_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_resume_token_ready = 0;
    int carried_checkpoint_resume_token_issued = 0;
    int signon_message_cursor_carried_checkpoint_resume_token_claim_ready = 0;
    int carried_checkpoint_resume_token_claimed = 0;
    int signon_message_cursor_carried_checkpoint_claimed_resume_ready = 0;
    int non_exhausted_claimed_checkpoint_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_bridge_ready = 0;
    int claimed_checkpoint_bridge_materialized = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_resume_ready = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_resume_range_ready = 0;
    int claimed_checkpoint_bridge_resume_range_delivered = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_resume_eof_ready = 0;
    int claimed_checkpoint_bridge_resume_exhausted = 0;
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointResumeEofProbeSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string probe = "disabled";
    int attempts = 0;
    int accepted = 0;
    int rejected = 0;
    std::string last_reject_reason = "<none>";
    std::string requested_claimant_session;
    std::string requested_cursor_id;
    std::string parsed_source_session;
    std::string parsed_issuer_target_session;
    std::string parsed_claimant_session;
    std::string parsed_cursor_id;
    bool parsed_eof = false;
    bool parsed_exhausted = false;
    std::string parsed_current_range;
    std::string parsed_next_start_message_index = "<unset>";
    int parsed_remaining_message_count = 0;
    std::string parsed_resume_policy;
    int parsed_target_activated = 0;
    std::string parsed_map;
    std::string parsed_name;
    std::string parsed_ruleset;
    int parsed_spawned = 0;
    int signon_ready = 0;
    int bootstrap_delivered = 0;
    int baseline_ready = 0;
    int bootstrap_sequence_completed = 0;
    int signon_catalog_ready = 0;
    int bootstrap_records_staged = 0;
    int signon_template_ready = 0;
    int template_records_delivered = 0;
    int signon_template_coverage_complete = 0;
    int remaining_template_records_delivered = 0;
    int signon_envelope_ready = 0;
    int framed_template_records_delivered = 0;
    int signon_batch_ready = 0;
    int pseudo_packet_batch_delivered = 0;
    int signon_wiremap_ready = 0;
    int wiremapped_batch_delivered = 0;
    int signon_burst_ready = 0;
    int pseudo_wire_burst_delivered = 0;
    int signon_stream_ready = 0;
    int contiguous_stream_delivered = 0;
    int signon_stream_window_ready = 0;
    int windowed_stream_delivered = 0;
    int signon_message_catalog_ready = 0;
    int message_boundaries_cataloged = 0;
    int signon_message_fetch_ready = 0;
    int targeted_message_fetch_delivered = 0;
    int signon_multi_message_fetch_ready = 0;
    int selective_message_set_delivered = 0;
    int signon_message_range_fetch_ready = 0;
    int range_message_set_delivered = 0;
    int signon_message_cursor_ready = 0;
    int message_cursor_checkpointed = 0;
    int signon_message_cursor_resume_ready = 0;
    int non_exhausted_cursor_resume_allowed = 0;
    int signon_message_cursor_carryover_ready = 0;
    int checkpoint_cursor_carried_over = 0;
    int signon_message_cursor_carried_resume_ready = 0;
    int non_exhausted_carried_cursor_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_ready = 0;
    int carried_cursor_checkpointed = 0;
    int signon_message_cursor_carried_checkpoint_resume_ready = 0;
    int non_exhausted_carried_checkpoint_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_resume_token_ready = 0;
    int carried_checkpoint_resume_token_issued = 0;
    int signon_message_cursor_carried_checkpoint_resume_token_claim_ready = 0;
    int carried_checkpoint_resume_token_claimed = 0;
    int signon_message_cursor_carried_checkpoint_claimed_resume_ready = 0;
    int non_exhausted_claimed_checkpoint_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_bridge_ready = 0;
    int claimed_checkpoint_bridge_materialized = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_resume_ready = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_resume_range_ready = 0;
    int claimed_checkpoint_bridge_resume_range_delivered = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_resume_eof_ready = 0;
    int claimed_checkpoint_bridge_resume_exhausted = 0;
    std::string protocol_shape = "disabled";
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointResumedDenialSurfaceSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string bind = "disabled";
    int requested_port = 0;
    int bound_port = 0;
    std::string protocol_shape = "disabled";
    bool claimed_checkpoint_resumed_denial_enabled = false;
    bool requires_claimed_checkpoint_resume_eof_ready_session = true;
    std::string denial_payload = "disabled";
    int accepted = 0;
    int rejected = 0;
    std::string last_source_session;
    std::string last_issuer_target_session;
    std::string last_claimant_session;
    std::string last_denied_cursor_id;
    bool resume_allowed = false;
    std::string denial_reason = "<unset>";
    bool eof = false;
    bool exhausted = false;
    std::string next_start_message_index = "<unset>";
    int remaining_message_count = 0;
    std::string auth = "disabled";
    std::string signon = "disabled";
    std::string gameplay_transport = "no";
    int signon_ready = 0;
    int bootstrap_delivered = 0;
    int baseline_ready = 0;
    int bootstrap_sequence_completed = 0;
    int signon_catalog_ready = 0;
    int bootstrap_records_staged = 0;
    int signon_template_ready = 0;
    int template_records_delivered = 0;
    int signon_template_coverage_complete = 0;
    int remaining_template_records_delivered = 0;
    int signon_envelope_ready = 0;
    int framed_template_records_delivered = 0;
    int signon_batch_ready = 0;
    int pseudo_packet_batch_delivered = 0;
    int signon_wiremap_ready = 0;
    int wiremapped_batch_delivered = 0;
    int signon_burst_ready = 0;
    int pseudo_wire_burst_delivered = 0;
    int signon_stream_ready = 0;
    int contiguous_stream_delivered = 0;
    int signon_stream_window_ready = 0;
    int windowed_stream_delivered = 0;
    int signon_message_catalog_ready = 0;
    int message_boundaries_cataloged = 0;
    int signon_message_fetch_ready = 0;
    int targeted_message_fetch_delivered = 0;
    int signon_multi_message_fetch_ready = 0;
    int selective_message_set_delivered = 0;
    int signon_message_range_fetch_ready = 0;
    int range_message_set_delivered = 0;
    int signon_message_cursor_ready = 0;
    int message_cursor_checkpointed = 0;
    int signon_message_cursor_resume_ready = 0;
    int non_exhausted_cursor_resume_allowed = 0;
    int signon_message_cursor_carryover_ready = 0;
    int checkpoint_cursor_carried_over = 0;
    int signon_message_cursor_carried_resume_ready = 0;
    int non_exhausted_carried_cursor_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_ready = 0;
    int carried_cursor_checkpointed = 0;
    int signon_message_cursor_carried_checkpoint_resume_ready = 0;
    int non_exhausted_carried_checkpoint_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_resume_token_ready = 0;
    int carried_checkpoint_resume_token_issued = 0;
    int signon_message_cursor_carried_checkpoint_resume_token_claim_ready = 0;
    int carried_checkpoint_resume_token_claimed = 0;
    int signon_message_cursor_carried_checkpoint_claimed_resume_ready = 0;
    int non_exhausted_claimed_checkpoint_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_bridge_ready = 0;
    int claimed_checkpoint_bridge_materialized = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_resume_ready = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_resume_range_ready = 0;
    int claimed_checkpoint_bridge_resume_range_delivered = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_resume_eof_ready = 0;
    int claimed_checkpoint_bridge_resume_exhausted = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_resumed_denied_ready = 0;
    int claimed_checkpoint_bridge_resumed_denied = 0;
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointResumedDenialProbeSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string probe = "disabled";
    int attempts = 0;
    int accepted = 0;
    int rejected = 0;
    std::string last_reject_reason = "<none>";
    std::string requested_claimant_session;
    std::string requested_cursor_id;
    std::string parsed_source_session;
    std::string parsed_issuer_target_session;
    std::string parsed_claimant_session;
    std::string parsed_cursor_id;
    bool parsed_resume_allowed = false;
    std::string parsed_denial_reason;
    bool parsed_eof = false;
    bool parsed_exhausted = false;
    std::string parsed_next_start_message_index = "<unset>";
    int parsed_remaining_message_count = 0;
    std::string parsed_resume_policy;
    int parsed_target_activated = 0;
    std::string parsed_map;
    std::string parsed_name;
    std::string parsed_ruleset;
    int parsed_spawned = 0;
    int signon_ready = 0;
    int bootstrap_delivered = 0;
    int baseline_ready = 0;
    int bootstrap_sequence_completed = 0;
    int signon_catalog_ready = 0;
    int bootstrap_records_staged = 0;
    int signon_template_ready = 0;
    int template_records_delivered = 0;
    int signon_template_coverage_complete = 0;
    int remaining_template_records_delivered = 0;
    int signon_envelope_ready = 0;
    int framed_template_records_delivered = 0;
    int signon_batch_ready = 0;
    int pseudo_packet_batch_delivered = 0;
    int signon_wiremap_ready = 0;
    int wiremapped_batch_delivered = 0;
    int signon_burst_ready = 0;
    int pseudo_wire_burst_delivered = 0;
    int signon_stream_ready = 0;
    int contiguous_stream_delivered = 0;
    int signon_stream_window_ready = 0;
    int windowed_stream_delivered = 0;
    int signon_message_catalog_ready = 0;
    int message_boundaries_cataloged = 0;
    int signon_message_fetch_ready = 0;
    int targeted_message_fetch_delivered = 0;
    int signon_multi_message_fetch_ready = 0;
    int selective_message_set_delivered = 0;
    int signon_message_range_fetch_ready = 0;
    int range_message_set_delivered = 0;
    int signon_message_cursor_ready = 0;
    int message_cursor_checkpointed = 0;
    int signon_message_cursor_resume_ready = 0;
    int non_exhausted_cursor_resume_allowed = 0;
    int signon_message_cursor_carryover_ready = 0;
    int checkpoint_cursor_carried_over = 0;
    int signon_message_cursor_carried_resume_ready = 0;
    int non_exhausted_carried_cursor_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_ready = 0;
    int carried_cursor_checkpointed = 0;
    int signon_message_cursor_carried_checkpoint_resume_ready = 0;
    int non_exhausted_carried_checkpoint_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_resume_token_ready = 0;
    int carried_checkpoint_resume_token_issued = 0;
    int signon_message_cursor_carried_checkpoint_resume_token_claim_ready = 0;
    int carried_checkpoint_resume_token_claimed = 0;
    int signon_message_cursor_carried_checkpoint_claimed_resume_ready = 0;
    int non_exhausted_claimed_checkpoint_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_bridge_ready = 0;
    int claimed_checkpoint_bridge_materialized = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_resume_ready = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_resume_range_ready = 0;
    int claimed_checkpoint_bridge_resume_range_delivered = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_resume_eof_ready = 0;
    int claimed_checkpoint_bridge_resume_exhausted = 0;
    int signon_message_cursor_carried_checkpoint_claimed_checkpoint_resumed_denied_ready = 0;
    int claimed_checkpoint_bridge_resumed_denied = 0;
    std::string protocol_shape = "disabled";
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointClaimedResumeRangeSurfaceSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string bind = "disabled";
    int requested_port = 0;
    int bound_port = 0;
    std::string protocol_shape = "disabled";
    bool claimed_resume_range_enabled = false;
    bool requires_claimed_resume_ready_session = true;
    bool requires_issued_resume_token = true;
    std::string fetch_selector = "disabled";
    std::string fetch_payload = "disabled";
    int requested_message_count = 0;
    int accepted = 0;
    int rejected = 0;
    std::string last_source_session;
    std::string last_issuer_target_session;
    std::string last_claimant_session;
    std::string last_claimed_cursor_id;
    std::string last_fetched_message_indices;
    std::string next_start_message_index_after_resume_fetch = "<unset>";
    int remaining_message_count_after_resume_fetch = 0;
    std::string auth = "disabled";
    std::string signon = "disabled";
    std::string gameplay_transport = "no";
    int signon_ready = 0;
    int bootstrap_delivered = 0;
    int baseline_ready = 0;
    int bootstrap_sequence_completed = 0;
    int signon_catalog_ready = 0;
    int bootstrap_records_staged = 0;
    int signon_template_ready = 0;
    int template_records_delivered = 0;
    int signon_template_coverage_complete = 0;
    int remaining_template_records_delivered = 0;
    int signon_envelope_ready = 0;
    int framed_template_records_delivered = 0;
    int signon_batch_ready = 0;
    int pseudo_packet_batch_delivered = 0;
    int signon_wiremap_ready = 0;
    int wiremapped_batch_delivered = 0;
    int signon_burst_ready = 0;
    int pseudo_wire_burst_delivered = 0;
    int signon_stream_ready = 0;
    int contiguous_stream_delivered = 0;
    int signon_stream_window_ready = 0;
    int windowed_stream_delivered = 0;
    int signon_message_catalog_ready = 0;
    int message_boundaries_cataloged = 0;
    int signon_message_fetch_ready = 0;
    int targeted_message_fetch_delivered = 0;
    int signon_multi_message_fetch_ready = 0;
    int selective_message_set_delivered = 0;
    int signon_message_range_fetch_ready = 0;
    int range_message_set_delivered = 0;
    int signon_message_cursor_ready = 0;
    int message_cursor_checkpointed = 0;
    int signon_message_cursor_resume_ready = 0;
    int non_exhausted_cursor_resume_allowed = 0;
    int signon_message_cursor_carryover_ready = 0;
    int checkpoint_cursor_carried_over = 0;
    int signon_message_cursor_carried_resume_ready = 0;
    int non_exhausted_carried_cursor_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_ready = 0;
    int carried_cursor_checkpointed = 0;
    int signon_message_cursor_carried_checkpoint_resume_ready = 0;
    int non_exhausted_carried_checkpoint_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_resume_token_ready = 0;
    int carried_checkpoint_resume_token_issued = 0;
    int signon_message_cursor_carried_checkpoint_resume_token_claim_ready = 0;
    int carried_checkpoint_resume_token_claimed = 0;
    int signon_message_cursor_carried_checkpoint_claimed_resume_ready = 0;
    int non_exhausted_claimed_checkpoint_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_claimed_resume_range_ready = 0;
    int claimed_checkpoint_resume_range_delivered = 0;
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointClaimedResumeRangeProbeSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string probe = "disabled";
    int attempts = 0;
    int accepted = 0;
    int rejected = 0;
    std::string last_reject_reason = "<none>";
    std::string requested_claimant_session;
    int requested_message_count = 0;
    std::string requested_cursor_id;
    std::string parsed_source_session;
    std::string parsed_issuer_target_session;
    std::string parsed_claimant_session;
    std::string parsed_cursor_id;
    std::string parsed_fetched_message_indices;
    std::string parsed_semantic_tags;
    int parsed_combined_byte_length = 0;
    std::string parsed_next_start_message_index_after_resume_fetch = "<unset>";
    int parsed_remaining_message_count_after_resume_fetch = 0;
    std::string parsed_resume_policy;
    int parsed_target_activated = 0;
    std::string parsed_map;
    std::string parsed_name;
    std::string parsed_ruleset;
    int parsed_spawned = 0;
    int signon_ready = 0;
    int bootstrap_delivered = 0;
    int baseline_ready = 0;
    int bootstrap_sequence_completed = 0;
    int signon_catalog_ready = 0;
    int bootstrap_records_staged = 0;
    int signon_template_ready = 0;
    int template_records_delivered = 0;
    int signon_template_coverage_complete = 0;
    int remaining_template_records_delivered = 0;
    int signon_envelope_ready = 0;
    int framed_template_records_delivered = 0;
    int signon_batch_ready = 0;
    int pseudo_packet_batch_delivered = 0;
    int signon_wiremap_ready = 0;
    int wiremapped_batch_delivered = 0;
    int signon_burst_ready = 0;
    int pseudo_wire_burst_delivered = 0;
    int signon_stream_ready = 0;
    int contiguous_stream_delivered = 0;
    int signon_stream_window_ready = 0;
    int windowed_stream_delivered = 0;
    int signon_message_catalog_ready = 0;
    int message_boundaries_cataloged = 0;
    int signon_message_fetch_ready = 0;
    int targeted_message_fetch_delivered = 0;
    int signon_multi_message_fetch_ready = 0;
    int selective_message_set_delivered = 0;
    int signon_message_range_fetch_ready = 0;
    int range_message_set_delivered = 0;
    int signon_message_cursor_ready = 0;
    int message_cursor_checkpointed = 0;
    int signon_message_cursor_resume_ready = 0;
    int non_exhausted_cursor_resume_allowed = 0;
    int signon_message_cursor_carryover_ready = 0;
    int checkpoint_cursor_carried_over = 0;
    int signon_message_cursor_carried_resume_ready = 0;
    int non_exhausted_carried_cursor_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_ready = 0;
    int carried_cursor_checkpointed = 0;
    int signon_message_cursor_carried_checkpoint_resume_ready = 0;
    int non_exhausted_carried_checkpoint_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_resume_token_ready = 0;
    int carried_checkpoint_resume_token_issued = 0;
    int signon_message_cursor_carried_checkpoint_resume_token_claim_ready = 0;
    int carried_checkpoint_resume_token_claimed = 0;
    int signon_message_cursor_carried_checkpoint_claimed_resume_ready = 0;
    int non_exhausted_claimed_checkpoint_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_claimed_resume_range_ready = 0;
    int claimed_checkpoint_resume_range_delivered = 0;
    std::string protocol_shape = "disabled";
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointClaimedResumeEofSurfaceSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string bind = "disabled";
    int requested_port = 0;
    int bound_port = 0;
    std::string protocol_shape = "disabled";
    bool claimed_resume_eof_enabled = false;
    bool requires_claimed_resume_range_ready_session = true;
    bool requires_issued_resume_token = true;
    std::string eof_payload = "disabled";
    int accepted = 0;
    int rejected = 0;
    std::string last_source_session;
    std::string last_issuer_target_session;
    std::string last_claimant_session;
    std::string last_claimed_cursor_id;
    bool eof = false;
    bool exhausted = false;
    std::string next_start_message_index = "<unset>";
    int remaining_message_count = 0;
    std::string auth = "disabled";
    std::string signon = "disabled";
    std::string gameplay_transport = "no";
    int signon_ready = 0;
    int bootstrap_delivered = 0;
    int baseline_ready = 0;
    int bootstrap_sequence_completed = 0;
    int signon_catalog_ready = 0;
    int bootstrap_records_staged = 0;
    int signon_template_ready = 0;
    int template_records_delivered = 0;
    int signon_template_coverage_complete = 0;
    int remaining_template_records_delivered = 0;
    int signon_envelope_ready = 0;
    int framed_template_records_delivered = 0;
    int signon_batch_ready = 0;
    int pseudo_packet_batch_delivered = 0;
    int signon_wiremap_ready = 0;
    int wiremapped_batch_delivered = 0;
    int signon_burst_ready = 0;
    int pseudo_wire_burst_delivered = 0;
    int signon_stream_ready = 0;
    int contiguous_stream_delivered = 0;
    int signon_stream_window_ready = 0;
    int windowed_stream_delivered = 0;
    int signon_message_catalog_ready = 0;
    int message_boundaries_cataloged = 0;
    int signon_message_fetch_ready = 0;
    int targeted_message_fetch_delivered = 0;
    int signon_multi_message_fetch_ready = 0;
    int selective_message_set_delivered = 0;
    int signon_message_range_fetch_ready = 0;
    int range_message_set_delivered = 0;
    int signon_message_cursor_ready = 0;
    int message_cursor_checkpointed = 0;
    int signon_message_cursor_resume_ready = 0;
    int non_exhausted_cursor_resume_allowed = 0;
    int signon_message_cursor_carryover_ready = 0;
    int checkpoint_cursor_carried_over = 0;
    int signon_message_cursor_carried_resume_ready = 0;
    int non_exhausted_carried_cursor_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_ready = 0;
    int carried_cursor_checkpointed = 0;
    int signon_message_cursor_carried_checkpoint_resume_ready = 0;
    int non_exhausted_carried_checkpoint_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_resume_token_ready = 0;
    int carried_checkpoint_resume_token_issued = 0;
    int signon_message_cursor_carried_checkpoint_resume_token_claim_ready = 0;
    int carried_checkpoint_resume_token_claimed = 0;
    int signon_message_cursor_carried_checkpoint_claimed_resume_ready = 0;
    int non_exhausted_claimed_checkpoint_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_claimed_resume_range_ready = 0;
    int claimed_checkpoint_resume_range_delivered = 0;
    int signon_message_cursor_carried_checkpoint_claimed_resume_eof_ready = 0;
    int claimed_checkpoint_resume_exhausted = 0;
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointClaimedResumeEofProbeSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string probe = "disabled";
    int attempts = 0;
    int accepted = 0;
    int rejected = 0;
    std::string last_reject_reason = "<none>";
    std::string requested_claimant_session;
    std::string requested_cursor_id;
    std::string parsed_source_session;
    std::string parsed_issuer_target_session;
    std::string parsed_claimant_session;
    std::string parsed_cursor_id;
    bool parsed_eof = false;
    bool parsed_exhausted = false;
    std::string parsed_current_range;
    std::string parsed_next_start_message_index = "<unset>";
    int parsed_remaining_message_count = 0;
    std::string parsed_resume_policy;
    int parsed_target_activated = 0;
    std::string parsed_map;
    std::string parsed_name;
    std::string parsed_ruleset;
    int parsed_spawned = 0;
    int signon_ready = 0;
    int bootstrap_delivered = 0;
    int baseline_ready = 0;
    int bootstrap_sequence_completed = 0;
    int signon_catalog_ready = 0;
    int bootstrap_records_staged = 0;
    int signon_template_ready = 0;
    int template_records_delivered = 0;
    int signon_template_coverage_complete = 0;
    int remaining_template_records_delivered = 0;
    int signon_envelope_ready = 0;
    int framed_template_records_delivered = 0;
    int signon_batch_ready = 0;
    int pseudo_packet_batch_delivered = 0;
    int signon_wiremap_ready = 0;
    int wiremapped_batch_delivered = 0;
    int signon_burst_ready = 0;
    int pseudo_wire_burst_delivered = 0;
    int signon_stream_ready = 0;
    int contiguous_stream_delivered = 0;
    int signon_stream_window_ready = 0;
    int windowed_stream_delivered = 0;
    int signon_message_catalog_ready = 0;
    int message_boundaries_cataloged = 0;
    int signon_message_fetch_ready = 0;
    int targeted_message_fetch_delivered = 0;
    int signon_multi_message_fetch_ready = 0;
    int selective_message_set_delivered = 0;
    int signon_message_range_fetch_ready = 0;
    int range_message_set_delivered = 0;
    int signon_message_cursor_ready = 0;
    int message_cursor_checkpointed = 0;
    int signon_message_cursor_resume_ready = 0;
    int non_exhausted_cursor_resume_allowed = 0;
    int signon_message_cursor_carryover_ready = 0;
    int checkpoint_cursor_carried_over = 0;
    int signon_message_cursor_carried_resume_ready = 0;
    int non_exhausted_carried_cursor_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_ready = 0;
    int carried_cursor_checkpointed = 0;
    int signon_message_cursor_carried_checkpoint_resume_ready = 0;
    int non_exhausted_carried_checkpoint_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_resume_token_ready = 0;
    int carried_checkpoint_resume_token_issued = 0;
    int signon_message_cursor_carried_checkpoint_resume_token_claim_ready = 0;
    int carried_checkpoint_resume_token_claimed = 0;
    int signon_message_cursor_carried_checkpoint_claimed_resume_ready = 0;
    int non_exhausted_claimed_checkpoint_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_claimed_resume_range_ready = 0;
    int claimed_checkpoint_resume_range_delivered = 0;
    int signon_message_cursor_carried_checkpoint_claimed_resume_eof_ready = 0;
    int claimed_checkpoint_resume_exhausted = 0;
    std::string protocol_shape = "disabled";
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointClaimedResumedDenialSurfaceSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string bind = "disabled";
    int requested_port = 0;
    int bound_port = 0;
    std::string protocol_shape = "disabled";
    bool claimed_resumed_denial_enabled = false;
    bool requires_claimed_resume_eof_ready_session = true;
    std::string denial_payload = "disabled";
    int accepted = 0;
    int rejected = 0;
    std::string last_source_session;
    std::string last_issuer_target_session;
    std::string last_claimant_session;
    std::string last_denied_cursor_id;
    bool resume_allowed = false;
    std::string denial_reason = "<unset>";
    bool eof = false;
    bool exhausted = false;
    std::string next_start_message_index = "<unset>";
    int remaining_message_count = 0;
    std::string auth = "disabled";
    std::string signon = "disabled";
    std::string gameplay_transport = "no";
    int signon_ready = 0;
    int bootstrap_delivered = 0;
    int baseline_ready = 0;
    int bootstrap_sequence_completed = 0;
    int signon_catalog_ready = 0;
    int bootstrap_records_staged = 0;
    int signon_template_ready = 0;
    int template_records_delivered = 0;
    int signon_template_coverage_complete = 0;
    int remaining_template_records_delivered = 0;
    int signon_envelope_ready = 0;
    int framed_template_records_delivered = 0;
    int signon_batch_ready = 0;
    int pseudo_packet_batch_delivered = 0;
    int signon_wiremap_ready = 0;
    int wiremapped_batch_delivered = 0;
    int signon_burst_ready = 0;
    int pseudo_wire_burst_delivered = 0;
    int signon_stream_ready = 0;
    int contiguous_stream_delivered = 0;
    int signon_stream_window_ready = 0;
    int windowed_stream_delivered = 0;
    int signon_message_catalog_ready = 0;
    int message_boundaries_cataloged = 0;
    int signon_message_fetch_ready = 0;
    int targeted_message_fetch_delivered = 0;
    int signon_multi_message_fetch_ready = 0;
    int selective_message_set_delivered = 0;
    int signon_message_range_fetch_ready = 0;
    int range_message_set_delivered = 0;
    int signon_message_cursor_ready = 0;
    int message_cursor_checkpointed = 0;
    int signon_message_cursor_resume_ready = 0;
    int non_exhausted_cursor_resume_allowed = 0;
    int signon_message_cursor_carryover_ready = 0;
    int checkpoint_cursor_carried_over = 0;
    int signon_message_cursor_carried_resume_ready = 0;
    int non_exhausted_carried_cursor_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_ready = 0;
    int carried_cursor_checkpointed = 0;
    int signon_message_cursor_carried_checkpoint_resume_ready = 0;
    int non_exhausted_carried_checkpoint_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_resume_token_ready = 0;
    int carried_checkpoint_resume_token_issued = 0;
    int signon_message_cursor_carried_checkpoint_resume_token_claim_ready = 0;
    int carried_checkpoint_resume_token_claimed = 0;
    int signon_message_cursor_carried_checkpoint_claimed_resume_ready = 0;
    int non_exhausted_claimed_checkpoint_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_claimed_resume_range_ready = 0;
    int claimed_checkpoint_resume_range_delivered = 0;
    int signon_message_cursor_carried_checkpoint_claimed_resume_eof_ready = 0;
    int claimed_checkpoint_resume_exhausted = 0;
    int signon_message_cursor_carried_checkpoint_claimed_resumed_denied_ready = 0;
    int claimed_checkpoint_resumed_denied = 0;
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointClaimedResumedDenialProbeSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string probe = "disabled";
    int attempts = 0;
    int accepted = 0;
    int rejected = 0;
    std::string last_reject_reason = "<none>";
    std::string requested_claimant_session;
    std::string requested_cursor_id;
    std::string parsed_source_session;
    std::string parsed_issuer_target_session;
    std::string parsed_claimant_session;
    std::string parsed_cursor_id;
    bool parsed_resume_allowed = false;
    std::string parsed_denial_reason;
    bool parsed_eof = false;
    bool parsed_exhausted = false;
    std::string parsed_next_start_message_index = "<unset>";
    int parsed_remaining_message_count = 0;
    std::string parsed_resume_policy;
    int parsed_target_activated = 0;
    std::string parsed_map;
    std::string parsed_name;
    std::string parsed_ruleset;
    int parsed_spawned = 0;
    int signon_ready = 0;
    int bootstrap_delivered = 0;
    int baseline_ready = 0;
    int bootstrap_sequence_completed = 0;
    int signon_catalog_ready = 0;
    int bootstrap_records_staged = 0;
    int signon_template_ready = 0;
    int template_records_delivered = 0;
    int signon_template_coverage_complete = 0;
    int remaining_template_records_delivered = 0;
    int signon_envelope_ready = 0;
    int framed_template_records_delivered = 0;
    int signon_batch_ready = 0;
    int pseudo_packet_batch_delivered = 0;
    int signon_wiremap_ready = 0;
    int wiremapped_batch_delivered = 0;
    int signon_burst_ready = 0;
    int pseudo_wire_burst_delivered = 0;
    int signon_stream_ready = 0;
    int contiguous_stream_delivered = 0;
    int signon_stream_window_ready = 0;
    int windowed_stream_delivered = 0;
    int signon_message_catalog_ready = 0;
    int message_boundaries_cataloged = 0;
    int signon_message_fetch_ready = 0;
    int targeted_message_fetch_delivered = 0;
    int signon_multi_message_fetch_ready = 0;
    int selective_message_set_delivered = 0;
    int signon_message_range_fetch_ready = 0;
    int range_message_set_delivered = 0;
    int signon_message_cursor_ready = 0;
    int message_cursor_checkpointed = 0;
    int signon_message_cursor_resume_ready = 0;
    int non_exhausted_cursor_resume_allowed = 0;
    int signon_message_cursor_carryover_ready = 0;
    int checkpoint_cursor_carried_over = 0;
    int signon_message_cursor_carried_resume_ready = 0;
    int non_exhausted_carried_cursor_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_ready = 0;
    int carried_cursor_checkpointed = 0;
    int signon_message_cursor_carried_checkpoint_resume_ready = 0;
    int non_exhausted_carried_checkpoint_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_resume_token_ready = 0;
    int carried_checkpoint_resume_token_issued = 0;
    int signon_message_cursor_carried_checkpoint_resume_token_claim_ready = 0;
    int carried_checkpoint_resume_token_claimed = 0;
    int signon_message_cursor_carried_checkpoint_claimed_resume_ready = 0;
    int non_exhausted_claimed_checkpoint_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_claimed_resume_range_ready = 0;
    int claimed_checkpoint_resume_range_delivered = 0;
    int signon_message_cursor_carried_checkpoint_claimed_resume_eof_ready = 0;
    int claimed_checkpoint_resume_exhausted = 0;
    int signon_message_cursor_carried_checkpoint_claimed_resumed_denied_ready = 0;
    int claimed_checkpoint_resumed_denied = 0;
    std::string protocol_shape = "disabled";
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointClaimedResumeDenialSurfaceSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string bind = "disabled";
    int requested_port = 0;
    int bound_port = 0;
    std::string protocol_shape = "disabled";
    bool claimed_resume_denial_enabled = false;
    bool requires_claimed_eof_ready_session = true;
    bool requires_issued_resume_token = true;
    std::string denial_payload = "disabled";
    int accepted = 0;
    int rejected = 0;
    std::string last_source_session;
    std::string last_issuer_target_session;
    std::string last_claimant_session;
    std::string last_denied_cursor_id;
    bool resume_allowed = false;
    std::string denial_reason = "<unset>";
    bool eof = false;
    bool exhausted = false;
    std::string next_start_message_index = "<unset>";
    int remaining_message_count = 0;
    std::string auth = "disabled";
    std::string signon = "disabled";
    std::string gameplay_transport = "no";
    int signon_ready = 0;
    int bootstrap_delivered = 0;
    int baseline_ready = 0;
    int bootstrap_sequence_completed = 0;
    int signon_catalog_ready = 0;
    int bootstrap_records_staged = 0;
    int signon_template_ready = 0;
    int template_records_delivered = 0;
    int signon_template_coverage_complete = 0;
    int remaining_template_records_delivered = 0;
    int signon_envelope_ready = 0;
    int framed_template_records_delivered = 0;
    int signon_batch_ready = 0;
    int pseudo_packet_batch_delivered = 0;
    int signon_wiremap_ready = 0;
    int wiremapped_batch_delivered = 0;
    int signon_burst_ready = 0;
    int pseudo_wire_burst_delivered = 0;
    int signon_stream_ready = 0;
    int contiguous_stream_delivered = 0;
    int signon_stream_window_ready = 0;
    int windowed_stream_delivered = 0;
    int signon_message_catalog_ready = 0;
    int message_boundaries_cataloged = 0;
    int signon_message_fetch_ready = 0;
    int targeted_message_fetch_delivered = 0;
    int signon_multi_message_fetch_ready = 0;
    int selective_message_set_delivered = 0;
    int signon_message_range_fetch_ready = 0;
    int range_message_set_delivered = 0;
    int signon_message_cursor_ready = 0;
    int message_cursor_checkpointed = 0;
    int signon_message_cursor_resume_ready = 0;
    int non_exhausted_cursor_resume_allowed = 0;
    int signon_message_cursor_carryover_ready = 0;
    int checkpoint_cursor_carried_over = 0;
    int signon_message_cursor_carried_resume_ready = 0;
    int non_exhausted_carried_cursor_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_ready = 0;
    int carried_cursor_checkpointed = 0;
    int signon_message_cursor_carried_checkpoint_resume_ready = 0;
    int non_exhausted_carried_checkpoint_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_resume_token_ready = 0;
    int carried_checkpoint_resume_token_issued = 0;
    int signon_message_cursor_carried_checkpoint_resume_token_claim_ready = 0;
    int carried_checkpoint_resume_token_claimed = 0;
    int signon_message_cursor_carried_checkpoint_claimed_resume_ready = 0;
    int non_exhausted_claimed_checkpoint_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_claimed_range_ready = 0;
    int claimed_checkpoint_range_delivered = 0;
    int signon_message_cursor_carried_checkpoint_claimed_eof_ready = 0;
    int claimed_checkpoint_exhausted = 0;
    int signon_message_cursor_carried_checkpoint_claimed_resume_denied_ready = 0;
    int claimed_checkpoint_resume_denied = 0;
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointClaimedResumeDenialProbeSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string probe = "disabled";
    int attempts = 0;
    int accepted = 0;
    int rejected = 0;
    std::string last_reject_reason = "<none>";
    std::string requested_claimant_session;
    std::string requested_cursor_id;
    std::string parsed_source_session;
    std::string parsed_issuer_target_session;
    std::string parsed_claimant_session;
    std::string parsed_cursor_id;
    bool parsed_resume_allowed = false;
    std::string parsed_denial_reason;
    bool parsed_eof = false;
    bool parsed_exhausted = false;
    std::string parsed_next_start_message_index = "<unset>";
    int parsed_remaining_message_count = 0;
    std::string parsed_resume_policy;
    int parsed_target_activated = 0;
    std::string parsed_map;
    std::string parsed_name;
    std::string parsed_ruleset;
    int parsed_spawned = 0;
    int signon_ready = 0;
    int bootstrap_delivered = 0;
    int baseline_ready = 0;
    int bootstrap_sequence_completed = 0;
    int signon_catalog_ready = 0;
    int bootstrap_records_staged = 0;
    int signon_template_ready = 0;
    int template_records_delivered = 0;
    int signon_template_coverage_complete = 0;
    int remaining_template_records_delivered = 0;
    int signon_envelope_ready = 0;
    int framed_template_records_delivered = 0;
    int signon_batch_ready = 0;
    int pseudo_packet_batch_delivered = 0;
    int signon_wiremap_ready = 0;
    int wiremapped_batch_delivered = 0;
    int signon_burst_ready = 0;
    int pseudo_wire_burst_delivered = 0;
    int signon_stream_ready = 0;
    int contiguous_stream_delivered = 0;
    int signon_stream_window_ready = 0;
    int windowed_stream_delivered = 0;
    int signon_message_catalog_ready = 0;
    int message_boundaries_cataloged = 0;
    int signon_message_fetch_ready = 0;
    int targeted_message_fetch_delivered = 0;
    int signon_multi_message_fetch_ready = 0;
    int selective_message_set_delivered = 0;
    int signon_message_range_fetch_ready = 0;
    int range_message_set_delivered = 0;
    int signon_message_cursor_ready = 0;
    int message_cursor_checkpointed = 0;
    int signon_message_cursor_resume_ready = 0;
    int non_exhausted_cursor_resume_allowed = 0;
    int signon_message_cursor_carryover_ready = 0;
    int checkpoint_cursor_carried_over = 0;
    int signon_message_cursor_carried_resume_ready = 0;
    int non_exhausted_carried_cursor_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_ready = 0;
    int carried_cursor_checkpointed = 0;
    int signon_message_cursor_carried_checkpoint_resume_ready = 0;
    int non_exhausted_carried_checkpoint_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_resume_token_ready = 0;
    int carried_checkpoint_resume_token_issued = 0;
    int signon_message_cursor_carried_checkpoint_resume_token_claim_ready = 0;
    int carried_checkpoint_resume_token_claimed = 0;
    int signon_message_cursor_carried_checkpoint_claimed_resume_ready = 0;
    int non_exhausted_claimed_checkpoint_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_claimed_range_ready = 0;
    int claimed_checkpoint_range_delivered = 0;
    int signon_message_cursor_carried_checkpoint_claimed_eof_ready = 0;
    int claimed_checkpoint_exhausted = 0;
    int signon_message_cursor_carried_checkpoint_claimed_resume_denied_ready = 0;
    int claimed_checkpoint_resume_denied = 0;
    std::string protocol_shape = "disabled";
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointResumeRangeSurfaceSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string bind = "disabled";
    int requested_port = 0;
    int bound_port = 0;
    std::string protocol_shape = "disabled";
    bool carried_checkpoint_resume_range_enabled = false;
    bool requires_target_carried_checkpoint_resume_ready_session = true;
    bool requires_source_resume_allowed_session = true;
    bool requires_non_exhausted_carried_checkpoint = true;
    std::string fetch_selector = "disabled";
    std::string fetch_payload = "disabled";
    int requested_message_count = 0;
    int accepted = 0;
    int rejected = 0;
    std::string last_source_session;
    std::string last_target_session;
    std::string last_checkpoint_cursor_id;
    std::string last_fetched_message_indices;
    std::string next_start_message_index_after_resume_fetch = "<unset>";
    int remaining_message_count_after_resume_fetch = 0;
    std::string auth = "disabled";
    std::string signon = "disabled";
    std::string gameplay_transport = "no";
    int signon_ready = 0;
    int bootstrap_delivered = 0;
    int baseline_ready = 0;
    int bootstrap_sequence_completed = 0;
    int signon_catalog_ready = 0;
    int bootstrap_records_staged = 0;
    int signon_template_ready = 0;
    int template_records_delivered = 0;
    int signon_template_coverage_complete = 0;
    int remaining_template_records_delivered = 0;
    int signon_envelope_ready = 0;
    int framed_template_records_delivered = 0;
    int signon_batch_ready = 0;
    int pseudo_packet_batch_delivered = 0;
    int signon_wiremap_ready = 0;
    int wiremapped_batch_delivered = 0;
    int signon_burst_ready = 0;
    int pseudo_wire_burst_delivered = 0;
    int signon_stream_ready = 0;
    int contiguous_stream_delivered = 0;
    int signon_stream_window_ready = 0;
    int windowed_stream_delivered = 0;
    int signon_message_catalog_ready = 0;
    int message_boundaries_cataloged = 0;
    int signon_message_fetch_ready = 0;
    int targeted_message_fetch_delivered = 0;
    int signon_multi_message_fetch_ready = 0;
    int selective_message_set_delivered = 0;
    int signon_message_range_fetch_ready = 0;
    int range_message_set_delivered = 0;
    int signon_message_cursor_ready = 0;
    int message_cursor_checkpointed = 0;
    int signon_message_cursor_advance_ready = 0;
    int cursor_advanced_range_delivered = 0;
    int signon_message_cursor_eof_ready = 0;
    int message_cursor_exhausted = 0;
    int signon_message_cursor_resume_denied_ready = 0;
    int exhausted_cursor_resume_denied = 0;
    int signon_message_cursor_resume_ready = 0;
    int non_exhausted_cursor_resume_allowed = 0;
    int signon_message_cursor_carryover_ready = 0;
    int checkpoint_cursor_carried_over = 0;
    int signon_message_cursor_carried_resume_ready = 0;
    int non_exhausted_carried_cursor_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_ready = 0;
    int carried_cursor_checkpointed = 0;
    int signon_message_cursor_carried_checkpoint_resume_ready = 0;
    int non_exhausted_carried_checkpoint_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_resume_range_ready = 0;
    int carried_checkpoint_resume_range_delivered = 0;
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointResumeRangeProbeSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string probe = "disabled";
    int attempts = 0;
    int accepted = 0;
    int rejected = 0;
    std::string last_reject_reason = "<none>";
    std::string requested_target_session;
    int requested_message_count = 0;
    std::string requested_cursor_id;
    std::string parsed_source_session;
    std::string parsed_target_session;
    std::string parsed_cursor_id;
    std::string parsed_fetched_message_indices;
    std::string parsed_semantic_tags;
    int parsed_combined_byte_length = 0;
    std::string parsed_next_start_message_index_after_resume_fetch = "<unset>";
    int parsed_remaining_message_count_after_resume_fetch = 0;
    std::string parsed_resume_policy;
    int parsed_target_activated = 0;
    std::string parsed_map;
    std::string parsed_name;
    std::string parsed_ruleset;
    int parsed_spawned = 0;
    int signon_ready = 0;
    int bootstrap_delivered = 0;
    int baseline_ready = 0;
    int bootstrap_sequence_completed = 0;
    int signon_catalog_ready = 0;
    int bootstrap_records_staged = 0;
    int signon_template_ready = 0;
    int template_records_delivered = 0;
    int signon_template_coverage_complete = 0;
    int remaining_template_records_delivered = 0;
    int signon_envelope_ready = 0;
    int framed_template_records_delivered = 0;
    int signon_batch_ready = 0;
    int pseudo_packet_batch_delivered = 0;
    int signon_wiremap_ready = 0;
    int wiremapped_batch_delivered = 0;
    int signon_burst_ready = 0;
    int pseudo_wire_burst_delivered = 0;
    int signon_stream_ready = 0;
    int contiguous_stream_delivered = 0;
    int signon_stream_window_ready = 0;
    int windowed_stream_delivered = 0;
    int signon_message_catalog_ready = 0;
    int message_boundaries_cataloged = 0;
    int signon_message_fetch_ready = 0;
    int targeted_message_fetch_delivered = 0;
    int signon_multi_message_fetch_ready = 0;
    int selective_message_set_delivered = 0;
    int signon_message_range_fetch_ready = 0;
    int range_message_set_delivered = 0;
    int signon_message_cursor_ready = 0;
    int message_cursor_checkpointed = 0;
    int signon_message_cursor_advance_ready = 0;
    int cursor_advanced_range_delivered = 0;
    int signon_message_cursor_eof_ready = 0;
    int message_cursor_exhausted = 0;
    int signon_message_cursor_resume_denied_ready = 0;
    int exhausted_cursor_resume_denied = 0;
    int signon_message_cursor_resume_ready = 0;
    int non_exhausted_cursor_resume_allowed = 0;
    int signon_message_cursor_carryover_ready = 0;
    int checkpoint_cursor_carried_over = 0;
    int signon_message_cursor_carried_resume_ready = 0;
    int non_exhausted_carried_cursor_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_ready = 0;
    int carried_cursor_checkpointed = 0;
    int signon_message_cursor_carried_checkpoint_resume_ready = 0;
    int non_exhausted_carried_checkpoint_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_resume_range_ready = 0;
    int carried_checkpoint_resume_range_delivered = 0;
    std::string protocol_shape = "disabled";
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointResumeEofSurfaceSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string bind = "disabled";
    int requested_port = 0;
    int bound_port = 0;
    std::string protocol_shape = "disabled";
    bool carried_checkpoint_resume_eof_enabled = false;
    bool requires_target_carried_checkpoint_resume_range_ready_session = true;
    bool requires_source_resume_allowed_session = true;
    std::string eof_source = "disabled";
    std::string eof_payload = "disabled";
    int accepted = 0;
    int rejected = 0;
    std::string last_source_session;
    std::string last_target_session;
    std::string last_checkpoint_cursor_id;
    bool eof = false;
    bool exhausted = false;
    std::string next_start_message_index = "<unset>";
    int remaining_message_count = 0;
    std::string auth = "disabled";
    std::string signon = "disabled";
    std::string gameplay_transport = "no";
    int signon_ready = 0;
    int bootstrap_delivered = 0;
    int baseline_ready = 0;
    int bootstrap_sequence_completed = 0;
    int signon_catalog_ready = 0;
    int bootstrap_records_staged = 0;
    int signon_template_ready = 0;
    int template_records_delivered = 0;
    int signon_template_coverage_complete = 0;
    int remaining_template_records_delivered = 0;
    int signon_envelope_ready = 0;
    int framed_template_records_delivered = 0;
    int signon_batch_ready = 0;
    int pseudo_packet_batch_delivered = 0;
    int signon_wiremap_ready = 0;
    int wiremapped_batch_delivered = 0;
    int signon_burst_ready = 0;
    int pseudo_wire_burst_delivered = 0;
    int signon_stream_ready = 0;
    int contiguous_stream_delivered = 0;
    int signon_stream_window_ready = 0;
    int windowed_stream_delivered = 0;
    int signon_message_catalog_ready = 0;
    int message_boundaries_cataloged = 0;
    int signon_message_fetch_ready = 0;
    int targeted_message_fetch_delivered = 0;
    int signon_multi_message_fetch_ready = 0;
    int selective_message_set_delivered = 0;
    int signon_message_range_fetch_ready = 0;
    int range_message_set_delivered = 0;
    int signon_message_cursor_ready = 0;
    int message_cursor_checkpointed = 0;
    int signon_message_cursor_resume_ready = 0;
    int non_exhausted_cursor_resume_allowed = 0;
    int signon_message_cursor_carryover_ready = 0;
    int checkpoint_cursor_carried_over = 0;
    int signon_message_cursor_carried_resume_ready = 0;
    int non_exhausted_carried_cursor_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_ready = 0;
    int carried_cursor_checkpointed = 0;
    int signon_message_cursor_carried_checkpoint_resume_ready = 0;
    int non_exhausted_carried_checkpoint_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_resume_range_ready = 0;
    int carried_checkpoint_resume_range_delivered = 0;
    int signon_message_cursor_carried_checkpoint_resume_eof_ready = 0;
    int carried_checkpoint_resume_exhausted = 0;
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointResumeEofProbeSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string probe = "disabled";
    int attempts = 0;
    int accepted = 0;
    int rejected = 0;
    std::string last_reject_reason = "<none>";
    std::string requested_target_session;
    std::string requested_cursor_id;
    std::string parsed_source_session;
    std::string parsed_target_session;
    std::string parsed_cursor_id;
    bool parsed_eof = false;
    bool parsed_exhausted = false;
    std::string parsed_current_range;
    std::string parsed_next_start_message_index = "<unset>";
    int parsed_remaining_message_count = 0;
    std::string parsed_resume_policy;
    int parsed_target_activated = 0;
    std::string parsed_map;
    std::string parsed_name;
    std::string parsed_ruleset;
    int parsed_spawned = 0;
    int signon_ready = 0;
    int bootstrap_delivered = 0;
    int baseline_ready = 0;
    int bootstrap_sequence_completed = 0;
    int signon_catalog_ready = 0;
    int bootstrap_records_staged = 0;
    int signon_template_ready = 0;
    int template_records_delivered = 0;
    int signon_template_coverage_complete = 0;
    int remaining_template_records_delivered = 0;
    int signon_envelope_ready = 0;
    int framed_template_records_delivered = 0;
    int signon_batch_ready = 0;
    int pseudo_packet_batch_delivered = 0;
    int signon_wiremap_ready = 0;
    int wiremapped_batch_delivered = 0;
    int signon_burst_ready = 0;
    int pseudo_wire_burst_delivered = 0;
    int signon_stream_ready = 0;
    int contiguous_stream_delivered = 0;
    int signon_stream_window_ready = 0;
    int windowed_stream_delivered = 0;
    int signon_message_catalog_ready = 0;
    int message_boundaries_cataloged = 0;
    int signon_message_fetch_ready = 0;
    int targeted_message_fetch_delivered = 0;
    int signon_multi_message_fetch_ready = 0;
    int selective_message_set_delivered = 0;
    int signon_message_range_fetch_ready = 0;
    int range_message_set_delivered = 0;
    int signon_message_cursor_ready = 0;
    int message_cursor_checkpointed = 0;
    int signon_message_cursor_resume_ready = 0;
    int non_exhausted_cursor_resume_allowed = 0;
    int signon_message_cursor_carryover_ready = 0;
    int checkpoint_cursor_carried_over = 0;
    int signon_message_cursor_carried_resume_ready = 0;
    int non_exhausted_carried_cursor_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_ready = 0;
    int carried_cursor_checkpointed = 0;
    int signon_message_cursor_carried_checkpoint_resume_ready = 0;
    int non_exhausted_carried_checkpoint_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_resume_range_ready = 0;
    int carried_checkpoint_resume_range_delivered = 0;
    int signon_message_cursor_carried_checkpoint_resume_eof_ready = 0;
    int carried_checkpoint_resume_exhausted = 0;
    std::string protocol_shape = "disabled";
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointResumeDenialSurfaceSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string bind = "disabled";
    int requested_port = 0;
    int bound_port = 0;
    bool shared_with_query = false;
    bool shared_with_connect = false;
    bool shared_with_activation = false;
    bool shared_with_bootstrap = false;
    bool shared_with_bootstrap_sequence = false;
    bool shared_with_signon_catalog = false;
    bool shared_with_signon_template = false;
    bool shared_with_signon_template_completion = false;
    bool shared_with_signon_envelope = false;
    bool shared_with_signon_batch = false;
    bool shared_with_signon_wiremap = false;
    bool shared_with_signon_burst = false;
    bool shared_with_signon_stream = false;
    bool shared_with_signon_stream_window = false;
    bool shared_with_signon_message_catalog = false;
    bool shared_with_signon_message_fetch = false;
    bool shared_with_signon_multi_message_fetch = false;
    bool shared_with_signon_message_range = false;
    bool shared_with_signon_message_cursor = false;
    bool shared_with_signon_message_cursor_advance = false;
    bool shared_with_signon_message_cursor_eof = false;
    bool shared_with_signon_message_cursor_resume_denial = false;
    bool shared_with_signon_message_cursor_resume_allow = false;
    bool shared_with_signon_message_cursor_carryover = false;
    bool shared_with_signon_message_cursor_carried_range = false;
    bool shared_with_signon_message_cursor_carried_eof = false;
    bool shared_with_signon_message_cursor_carried_resume_denial = false;
    bool shared_with_signon_message_cursor_carried_resume_allow = false;
    bool shared_with_signon_message_cursor_carried_checkpoint = false;
    bool shared_with_signon_message_cursor_carried_checkpoint_advance = false;
    bool shared_with_signon_message_cursor_carried_checkpoint_eof = false;
    std::string protocol_shape = "disabled";
    bool carried_checkpoint_resume_denial_enabled = false;
    bool requires_target_carried_checkpoint_eof_ready_session = true;
    bool requires_source_resume_allowed_session = true;
    std::string denial_source = "disabled";
    std::string denial_payload = "disabled";
    int accepted = 0;
    int rejected = 0;
    std::string last_source_session;
    std::string last_target_session;
    std::string last_denied_cursor_id;
    bool resume_allowed = false;
    std::string denial_reason = "<unset>";
    bool eof = false;
    bool exhausted = false;
    std::string next_start_message_index = "<unset>";
    int remaining_message_count = 0;
    std::string auth = "disabled";
    std::string signon = "disabled";
    std::string gameplay_transport = "no";
    int signon_ready = 0;
    int bootstrap_delivered = 0;
    int baseline_ready = 0;
    int bootstrap_sequence_completed = 0;
    int signon_catalog_ready = 0;
    int bootstrap_records_staged = 0;
    int signon_template_ready = 0;
    int template_records_delivered = 0;
    int signon_template_coverage_complete = 0;
    int remaining_template_records_delivered = 0;
    int signon_envelope_ready = 0;
    int framed_template_records_delivered = 0;
    int signon_batch_ready = 0;
    int pseudo_packet_batch_delivered = 0;
    int signon_wiremap_ready = 0;
    int wiremapped_batch_delivered = 0;
    int signon_burst_ready = 0;
    int pseudo_wire_burst_delivered = 0;
    int signon_stream_ready = 0;
    int contiguous_stream_delivered = 0;
    int signon_stream_window_ready = 0;
    int windowed_stream_delivered = 0;
    int signon_message_catalog_ready = 0;
    int message_boundaries_cataloged = 0;
    int signon_message_fetch_ready = 0;
    int targeted_message_fetch_delivered = 0;
    int signon_multi_message_fetch_ready = 0;
    int selective_message_set_delivered = 0;
    int signon_message_range_fetch_ready = 0;
    int range_message_set_delivered = 0;
    int signon_message_cursor_ready = 0;
    int message_cursor_checkpointed = 0;
    int signon_message_cursor_advance_ready = 0;
    int cursor_advanced_range_delivered = 0;
    int signon_message_cursor_eof_ready = 0;
    int message_cursor_exhausted = 0;
    int signon_message_cursor_resume_denied_ready = 0;
    int exhausted_cursor_resume_denied = 0;
    int signon_message_cursor_resume_ready = 0;
    int non_exhausted_cursor_resume_allowed = 0;
    int signon_message_cursor_carryover_ready = 0;
    int checkpoint_cursor_carried_over = 0;
    int signon_message_cursor_carried_resume_ready = 0;
    int non_exhausted_carried_cursor_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_ready = 0;
    int carried_cursor_checkpointed = 0;
    int signon_message_cursor_carried_checkpoint_advance_ready = 0;
    int carried_checkpoint_advanced_range_delivered = 0;
    int signon_message_cursor_carried_checkpoint_eof_ready = 0;
    int carried_checkpoint_exhausted = 0;
    int signon_message_cursor_carried_checkpoint_resume_denied_ready = 0;
    int carried_checkpoint_resume_denied = 0;
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointResumeDenialProbeSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string probe = "disabled";
    int attempts = 0;
    int accepted = 0;
    int rejected = 0;
    std::string last_reject_reason = "<none>";
    std::string requested_target_session;
    std::string requested_cursor_id;
    std::string parsed_source_session;
    std::string parsed_target_session;
    std::string parsed_cursor_id;
    bool parsed_resume_allowed = false;
    std::string parsed_denial_reason;
    bool parsed_eof = false;
    bool parsed_exhausted = false;
    std::string parsed_next_start_message_index = "<unset>";
    int parsed_remaining_message_count = 0;
    std::string parsed_resume_policy;
    int parsed_target_activated = 0;
    std::string parsed_map;
    std::string parsed_name;
    std::string parsed_ruleset;
    int parsed_spawned = 0;
    int signon_ready = 0;
    int bootstrap_delivered = 0;
    int baseline_ready = 0;
    int bootstrap_sequence_completed = 0;
    int signon_catalog_ready = 0;
    int bootstrap_records_staged = 0;
    int signon_template_ready = 0;
    int template_records_delivered = 0;
    int signon_template_coverage_complete = 0;
    int remaining_template_records_delivered = 0;
    int signon_envelope_ready = 0;
    int framed_template_records_delivered = 0;
    int signon_batch_ready = 0;
    int pseudo_packet_batch_delivered = 0;
    int signon_wiremap_ready = 0;
    int wiremapped_batch_delivered = 0;
    int signon_burst_ready = 0;
    int pseudo_wire_burst_delivered = 0;
    int signon_stream_ready = 0;
    int contiguous_stream_delivered = 0;
    int signon_stream_window_ready = 0;
    int windowed_stream_delivered = 0;
    int signon_message_catalog_ready = 0;
    int message_boundaries_cataloged = 0;
    int signon_message_fetch_ready = 0;
    int targeted_message_fetch_delivered = 0;
    int signon_multi_message_fetch_ready = 0;
    int selective_message_set_delivered = 0;
    int signon_message_range_fetch_ready = 0;
    int range_message_set_delivered = 0;
    int signon_message_cursor_ready = 0;
    int message_cursor_checkpointed = 0;
    int signon_message_cursor_advance_ready = 0;
    int cursor_advanced_range_delivered = 0;
    int signon_message_cursor_eof_ready = 0;
    int message_cursor_exhausted = 0;
    int signon_message_cursor_resume_denied_ready = 0;
    int exhausted_cursor_resume_denied = 0;
    int signon_message_cursor_resume_ready = 0;
    int non_exhausted_cursor_resume_allowed = 0;
    int signon_message_cursor_carryover_ready = 0;
    int checkpoint_cursor_carried_over = 0;
    int signon_message_cursor_carried_resume_ready = 0;
    int non_exhausted_carried_cursor_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_ready = 0;
    int carried_cursor_checkpointed = 0;
    int signon_message_cursor_carried_checkpoint_advance_ready = 0;
    int carried_checkpoint_advanced_range_delivered = 0;
    int signon_message_cursor_carried_checkpoint_eof_ready = 0;
    int carried_checkpoint_exhausted = 0;
    int signon_message_cursor_carried_checkpoint_resume_denied_ready = 0;
    int carried_checkpoint_resume_denied = 0;
    std::string protocol_shape = "disabled";
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointResumedDenialSurfaceSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string bind = "disabled";
    int requested_port = 0;
    int bound_port = 0;
    bool shared_with_query = false;
    bool shared_with_connect = false;
    bool shared_with_activation = false;
    bool shared_with_bootstrap = false;
    bool shared_with_bootstrap_sequence = false;
    bool shared_with_signon_catalog = false;
    bool shared_with_signon_template = false;
    bool shared_with_signon_template_completion = false;
    bool shared_with_signon_envelope = false;
    bool shared_with_signon_batch = false;
    bool shared_with_signon_wiremap = false;
    bool shared_with_signon_burst = false;
    bool shared_with_signon_stream = false;
    bool shared_with_signon_stream_window = false;
    bool shared_with_signon_message_catalog = false;
    bool shared_with_signon_message_fetch = false;
    bool shared_with_signon_multi_message_fetch = false;
    bool shared_with_signon_message_range = false;
    bool shared_with_signon_message_cursor = false;
    bool shared_with_signon_message_cursor_resume_allow = false;
    bool shared_with_signon_message_cursor_carryover = false;
    bool shared_with_signon_message_cursor_carried_resume_allow = false;
    bool shared_with_signon_message_cursor_carried_checkpoint = false;
    bool shared_with_signon_message_cursor_carried_checkpoint_resume_allow = false;
    bool shared_with_signon_message_cursor_carried_checkpoint_resume_range = false;
    bool shared_with_signon_message_cursor_carried_checkpoint_resume_eof = false;
    std::string protocol_shape = "disabled";
    bool carried_checkpoint_resumed_denial_enabled = false;
    bool requires_target_carried_checkpoint_resume_eof_ready_session = true;
    bool requires_source_resume_allowed_session = true;
    std::string denial_source = "disabled";
    std::string denial_payload = "disabled";
    int accepted = 0;
    int rejected = 0;
    std::string last_source_session;
    std::string last_target_session;
    std::string last_denied_cursor_id;
    bool resume_allowed = false;
    std::string denial_reason = "<unset>";
    bool eof = false;
    bool exhausted = false;
    std::string next_start_message_index = "<unset>";
    int remaining_message_count = 0;
    std::string auth = "disabled";
    std::string signon = "disabled";
    std::string gameplay_transport = "no";
    int signon_ready = 0;
    int bootstrap_delivered = 0;
    int baseline_ready = 0;
    int bootstrap_sequence_completed = 0;
    int signon_catalog_ready = 0;
    int bootstrap_records_staged = 0;
    int signon_template_ready = 0;
    int template_records_delivered = 0;
    int signon_template_coverage_complete = 0;
    int remaining_template_records_delivered = 0;
    int signon_envelope_ready = 0;
    int framed_template_records_delivered = 0;
    int signon_batch_ready = 0;
    int pseudo_packet_batch_delivered = 0;
    int signon_wiremap_ready = 0;
    int wiremapped_batch_delivered = 0;
    int signon_burst_ready = 0;
    int pseudo_wire_burst_delivered = 0;
    int signon_stream_ready = 0;
    int contiguous_stream_delivered = 0;
    int signon_stream_window_ready = 0;
    int windowed_stream_delivered = 0;
    int signon_message_catalog_ready = 0;
    int message_boundaries_cataloged = 0;
    int signon_message_fetch_ready = 0;
    int targeted_message_fetch_delivered = 0;
    int signon_multi_message_fetch_ready = 0;
    int selective_message_set_delivered = 0;
    int signon_message_range_fetch_ready = 0;
    int range_message_set_delivered = 0;
    int signon_message_cursor_ready = 0;
    int message_cursor_checkpointed = 0;
    int signon_message_cursor_resume_ready = 0;
    int non_exhausted_cursor_resume_allowed = 0;
    int signon_message_cursor_carryover_ready = 0;
    int checkpoint_cursor_carried_over = 0;
    int signon_message_cursor_carried_resume_ready = 0;
    int non_exhausted_carried_cursor_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_ready = 0;
    int carried_cursor_checkpointed = 0;
    int signon_message_cursor_carried_checkpoint_resume_ready = 0;
    int non_exhausted_carried_checkpoint_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_resume_range_ready = 0;
    int carried_checkpoint_resume_range_delivered = 0;
    int signon_message_cursor_carried_checkpoint_resume_eof_ready = 0;
    int carried_checkpoint_resume_exhausted = 0;
    int signon_message_cursor_carried_checkpoint_resumed_denied_ready = 0;
    int carried_checkpoint_resumed_denied = 0;
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedCheckpointResumedDenialProbeSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string probe = "disabled";
    int attempts = 0;
    int accepted = 0;
    int rejected = 0;
    std::string last_reject_reason = "<none>";
    std::string requested_target_session;
    std::string requested_cursor_id;
    std::string parsed_source_session;
    std::string parsed_target_session;
    std::string parsed_cursor_id;
    bool parsed_resume_allowed = false;
    std::string parsed_denial_reason;
    bool parsed_eof = false;
    bool parsed_exhausted = false;
    std::string parsed_next_start_message_index = "<unset>";
    int parsed_remaining_message_count = 0;
    std::string parsed_resume_policy;
    int parsed_target_activated = 0;
    std::string parsed_map;
    std::string parsed_name;
    std::string parsed_ruleset;
    int parsed_spawned = 0;
    int signon_ready = 0;
    int bootstrap_delivered = 0;
    int baseline_ready = 0;
    int bootstrap_sequence_completed = 0;
    int signon_catalog_ready = 0;
    int bootstrap_records_staged = 0;
    int signon_template_ready = 0;
    int template_records_delivered = 0;
    int signon_template_coverage_complete = 0;
    int remaining_template_records_delivered = 0;
    int signon_envelope_ready = 0;
    int framed_template_records_delivered = 0;
    int signon_batch_ready = 0;
    int pseudo_packet_batch_delivered = 0;
    int signon_wiremap_ready = 0;
    int wiremapped_batch_delivered = 0;
    int signon_burst_ready = 0;
    int pseudo_wire_burst_delivered = 0;
    int signon_stream_ready = 0;
    int contiguous_stream_delivered = 0;
    int signon_stream_window_ready = 0;
    int windowed_stream_delivered = 0;
    int signon_message_catalog_ready = 0;
    int message_boundaries_cataloged = 0;
    int signon_message_fetch_ready = 0;
    int targeted_message_fetch_delivered = 0;
    int signon_multi_message_fetch_ready = 0;
    int selective_message_set_delivered = 0;
    int signon_message_range_fetch_ready = 0;
    int range_message_set_delivered = 0;
    int signon_message_cursor_ready = 0;
    int message_cursor_checkpointed = 0;
    int signon_message_cursor_resume_ready = 0;
    int non_exhausted_cursor_resume_allowed = 0;
    int signon_message_cursor_carryover_ready = 0;
    int checkpoint_cursor_carried_over = 0;
    int signon_message_cursor_carried_resume_ready = 0;
    int non_exhausted_carried_cursor_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_ready = 0;
    int carried_cursor_checkpointed = 0;
    int signon_message_cursor_carried_checkpoint_resume_ready = 0;
    int non_exhausted_carried_checkpoint_resume_allowed = 0;
    int signon_message_cursor_carried_checkpoint_resume_range_ready = 0;
    int carried_checkpoint_resume_range_delivered = 0;
    int signon_message_cursor_carried_checkpoint_resume_eof_ready = 0;
    int carried_checkpoint_resume_exhausted = 0;
    int signon_message_cursor_carried_checkpoint_resumed_denied_ready = 0;
    int carried_checkpoint_resumed_denied = 0;
    std::string protocol_shape = "disabled";
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedResumeDenialSurfaceSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string bind = "disabled";
    int requested_port = 0;
    int bound_port = 0;
    bool shared_with_query = false;
    bool shared_with_connect = false;
    bool shared_with_activation = false;
    bool shared_with_bootstrap = false;
    bool shared_with_bootstrap_sequence = false;
    bool shared_with_signon_catalog = false;
    bool shared_with_signon_template = false;
    bool shared_with_signon_template_completion = false;
    bool shared_with_signon_envelope = false;
    bool shared_with_signon_batch = false;
    bool shared_with_signon_wiremap = false;
    bool shared_with_signon_burst = false;
    bool shared_with_signon_stream = false;
    bool shared_with_signon_stream_window = false;
    bool shared_with_signon_message_catalog = false;
    bool shared_with_signon_message_fetch = false;
    bool shared_with_signon_multi_message_fetch = false;
    bool shared_with_signon_message_range = false;
    bool shared_with_signon_message_cursor = false;
    bool shared_with_signon_message_cursor_advance = false;
    bool shared_with_signon_message_cursor_eof = false;
    bool shared_with_signon_message_cursor_resume_denial = false;
    bool shared_with_signon_message_cursor_resume_allow = false;
    bool shared_with_signon_message_cursor_carryover = false;
    bool shared_with_signon_message_cursor_carried_range = false;
    bool shared_with_signon_message_cursor_carried_eof = false;
    std::string protocol_shape = "disabled";
    bool carried_resume_denial_enabled = false;
    bool requires_target_carried_eof_ready_session = true;
    bool requires_source_resume_allowed_session = true;
    std::string denial_source = "disabled";
    std::string denial_payload = "disabled";
    int accepted = 0;
    int rejected = 0;
    std::string last_source_session;
    std::string last_target_session;
    std::string last_denied_cursor_id;
    bool resume_allowed = false;
    std::string denial_reason = "<unset>";
    bool eof = false;
    bool exhausted = false;
    std::string next_start_message_index = "<unset>";
    int remaining_message_count = 0;
    std::string auth = "disabled";
    std::string signon = "disabled";
    std::string gameplay_transport = "no";
    int signon_ready = 0;
    int bootstrap_delivered = 0;
    int baseline_ready = 0;
    int bootstrap_sequence_completed = 0;
    int signon_catalog_ready = 0;
    int bootstrap_records_staged = 0;
    int signon_template_ready = 0;
    int template_records_delivered = 0;
    int signon_template_coverage_complete = 0;
    int remaining_template_records_delivered = 0;
    int signon_envelope_ready = 0;
    int framed_template_records_delivered = 0;
    int signon_batch_ready = 0;
    int pseudo_packet_batch_delivered = 0;
    int signon_wiremap_ready = 0;
    int wiremapped_batch_delivered = 0;
    int signon_burst_ready = 0;
    int pseudo_wire_burst_delivered = 0;
    int signon_stream_ready = 0;
    int contiguous_stream_delivered = 0;
    int signon_stream_window_ready = 0;
    int windowed_stream_delivered = 0;
    int signon_message_catalog_ready = 0;
    int message_boundaries_cataloged = 0;
    int signon_message_fetch_ready = 0;
    int targeted_message_fetch_delivered = 0;
    int signon_multi_message_fetch_ready = 0;
    int selective_message_set_delivered = 0;
    int signon_message_range_fetch_ready = 0;
    int range_message_set_delivered = 0;
    int signon_message_cursor_ready = 0;
    int message_cursor_checkpointed = 0;
    int signon_message_cursor_advance_ready = 0;
    int cursor_advanced_range_delivered = 0;
    int signon_message_cursor_eof_ready = 0;
    int message_cursor_exhausted = 0;
    int signon_message_cursor_resume_denied_ready = 0;
    int exhausted_cursor_resume_denied = 0;
    int signon_message_cursor_resume_ready = 0;
    int non_exhausted_cursor_resume_allowed = 0;
    int signon_message_cursor_carryover_ready = 0;
    int checkpoint_cursor_carried_over = 0;
    int signon_message_cursor_carried_range_ready = 0;
    int carried_cursor_range_delivered = 0;
    int signon_message_cursor_carried_eof_ready = 0;
    int carried_cursor_exhausted = 0;
    int signon_message_cursor_carried_resume_denied_ready = 0;
    int carried_cursor_resume_denied = 0;
    std::string compatibility = "disabled";
    std::string detail;
};

struct DedicatedSignonMessageCursorCarriedResumeDenialProbeSummary
{
    bool enabled = false;
    std::string mode = "listen";
    std::string probe = "disabled";
    int attempts = 0;
    int accepted = 0;
    int rejected = 0;
    std::string last_reject_reason = "<none>";
    std::string requested_target_session;
    std::string requested_cursor_id;
    std::string parsed_source_session;
    std::string parsed_target_session;
    std::string parsed_cursor_id;
    bool parsed_resume_allowed = false;
    std::string parsed_denial_reason;
    bool parsed_eof = false;
    bool parsed_exhausted = false;
    std::string parsed_next_start_message_index = "<unset>";
    int parsed_remaining_message_count = 0;
    std::string parsed_resume_policy;
    int parsed_target_activated = 0;
    std::string parsed_map;
    std::string parsed_name;
    std::string parsed_ruleset;
    int parsed_spawned = 0;
    int signon_ready = 0;
    int bootstrap_delivered = 0;
    int baseline_ready = 0;
    int bootstrap_sequence_completed = 0;
    int signon_catalog_ready = 0;
    int bootstrap_records_staged = 0;
    int signon_template_ready = 0;
    int template_records_delivered = 0;
    int signon_template_coverage_complete = 0;
    int remaining_template_records_delivered = 0;
    int signon_envelope_ready = 0;
    int framed_template_records_delivered = 0;
    int signon_batch_ready = 0;
    int pseudo_packet_batch_delivered = 0;
    int signon_wiremap_ready = 0;
    int wiremapped_batch_delivered = 0;
    int signon_burst_ready = 0;
    int pseudo_wire_burst_delivered = 0;
    int signon_stream_ready = 0;
    int contiguous_stream_delivered = 0;
    int signon_stream_window_ready = 0;
    int windowed_stream_delivered = 0;
    int signon_message_catalog_ready = 0;
    int message_boundaries_cataloged = 0;
    int signon_message_fetch_ready = 0;
    int targeted_message_fetch_delivered = 0;
    int signon_multi_message_fetch_ready = 0;
    int selective_message_set_delivered = 0;
    int signon_message_range_fetch_ready = 0;
    int range_message_set_delivered = 0;
    int signon_message_cursor_ready = 0;
    int message_cursor_checkpointed = 0;
    int signon_message_cursor_advance_ready = 0;
    int cursor_advanced_range_delivered = 0;
    int signon_message_cursor_eof_ready = 0;
    int message_cursor_exhausted = 0;
    int signon_message_cursor_resume_denied_ready = 0;
    int exhausted_cursor_resume_denied = 0;
    int signon_message_cursor_resume_ready = 0;
    int non_exhausted_cursor_resume_allowed = 0;
    int signon_message_cursor_carryover_ready = 0;
    int checkpoint_cursor_carried_over = 0;
    int signon_message_cursor_carried_range_ready = 0;
    int carried_cursor_range_delivered = 0;
    int signon_message_cursor_carried_eof_ready = 0;
    int carried_cursor_exhausted = 0;
    int signon_message_cursor_carried_resume_denied_ready = 0;
    int carried_cursor_resume_denied = 0;
    std::string protocol_shape = "disabled";
    std::string compatibility = "disabled";
    std::string detail;
};

struct HlServerModuleInitOptions
{
    std::filesystem::path game_directory;
    std::string mod_name = "valve";
    std::string map_name = "c0a0";
    std::string hostname = "HLengine Test Server";
    ServerRuntimeMode runtime_mode = ServerRuntimeMode::kListenHost;
    int requested_maxclients = 1;
    int maxclients = 1;
    float deathmatch = 0.0f;
    float coop = 0.0f;
    int synthetic_players = 0;
    bool query_surface_enabled = false;
    bool query_probe_enabled = false;
    int query_port = 0;
    bool connect_surface_enabled = false;
    bool connect_probe_enabled = false;
    std::string connect_probe_scenario = "accept";
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
    FrameBootstrapOptions frame_bootstrap;
};

struct HlServerModuleSummary
{
    std::filesystem::path loaded_path;
    bool hl_dll_loaded = false;
    bool give_fnptrs_to_dll_export_found = false;
    bool get_entity_api2_export_found = false;
    bool give_fnptrs_to_dll_called = false;
    bool get_entity_api2_succeeded = false;
    bool dll_functions_acquired = false;
    bool pfn_game_init_present = false;
    bool pfn_game_init_called = false;
    bool pfn_game_init_succeeded = false;
    int interface_version_requested = 0;
    int interface_version_reported = 0;
    std::vector<DllFunctionPointerStatus> dll_functions;
    std::vector<InvokedEngineCallback> invoked_engine_callbacks;
    std::size_t registered_cvars = 0;
    std::size_t queued_server_commands = 0;
    std::size_t executed_server_commands = 0;
    std::size_t executed_cfg_files = 0;
    std::size_t updated_cvars_from_cfg = 0;
    std::size_t auto_created_cvars = 0;
    std::vector<std::string> executed_cfg_paths;
    std::vector<CvarSnapshot> sample_skill_cvars;
    ServerBootstrapStateSummary server_state;
    GlobalVariablesSnapshot globals_snapshot;
    std::size_t precache_callback_invocations = 0;
    std::size_t model_callback_invocations = 0;
    std::size_t string_callback_invocations = 0;
    std::size_t entity_callback_invocations = 0;
    bool ready_for_world_bootstrap = false;
    WorldBootstrapStateSummary world_bootstrap;
    bool ready_for_entity_parsing = false;
    EntityPipelineStateSummary entity_pipeline;
    WorldspawnSpawnStateSummary worldspawn_spawn;
    ServerActivationStateSummary server_activation;
    FrameBootstrapOptions frame_bootstrap_config;
    ServerFrameLoopStateSummary server_frame_loop;
    EntityThinkSchedulerStateSummary entity_think_scheduler;
    MapLogicDispatcherStateSummary map_logic_dispatcher;
    ChangeLevelTransitionSummary changelevel_transition;
    ScriptedLogicStateSummary scripted_logic;
    ScriptedMovementStateSummary scripted_movement;
    DedicatedServerFoundationSummary dedicated_server_foundation;
    DedicatedPlayerLifecycleFoundationSummary dedicated_player_lifecycle_foundation;
    DedicatedQuerySurfaceSummary dedicated_query_surface;
    DedicatedQueryProbeSummary dedicated_query_probe;
    DedicatedConnectSurfaceSummary dedicated_connect_surface;
    DedicatedConnectProbeSummary dedicated_connect_probe;
    DedicatedActivationSurfaceSummary dedicated_activation_surface;
    DedicatedActivationProbeSummary dedicated_activation_probe;
    DedicatedBootstrapSurfaceSummary dedicated_bootstrap_surface;
    DedicatedBootstrapProbeSummary dedicated_bootstrap_probe;
    DedicatedBootstrapSequenceSurfaceSummary dedicated_bootstrap_sequence_surface;
    DedicatedBootstrapSequenceProbeSummary dedicated_bootstrap_sequence_probe;
    DedicatedSignonCatalogSurfaceSummary dedicated_signon_catalog_surface;
    DedicatedSignonCatalogProbeSummary dedicated_signon_catalog_probe;
    DedicatedSignonTemplateSurfaceSummary dedicated_signon_template_surface;
    DedicatedSignonTemplateProbeSummary dedicated_signon_template_probe;
    DedicatedSignonTemplateCompletionSurfaceSummary dedicated_signon_template_completion_surface;
    DedicatedSignonTemplateCompletionProbeSummary dedicated_signon_template_completion_probe;
    DedicatedSignonEnvelopeSurfaceSummary dedicated_signon_envelope_surface;
    DedicatedSignonEnvelopeProbeSummary dedicated_signon_envelope_probe;
    DedicatedSignonBatchSurfaceSummary dedicated_signon_batch_surface;
    DedicatedSignonBatchProbeSummary dedicated_signon_batch_probe;
    DedicatedSignonWiremapSurfaceSummary dedicated_signon_wiremap_surface;
    DedicatedSignonWiremapProbeSummary dedicated_signon_wiremap_probe;
    DedicatedSignonBurstSurfaceSummary dedicated_signon_burst_surface;
    DedicatedSignonBurstProbeSummary dedicated_signon_burst_probe;
    DedicatedSignonStreamSurfaceSummary dedicated_signon_stream_surface;
    DedicatedSignonStreamProbeSummary dedicated_signon_stream_probe;
    DedicatedSignonStreamWindowSurfaceSummary dedicated_signon_stream_window_surface;
    DedicatedSignonStreamWindowProbeSummary dedicated_signon_stream_window_probe;
    DedicatedSignonMessageCatalogSurfaceSummary dedicated_signon_message_catalog_surface;
    DedicatedSignonMessageCatalogProbeSummary dedicated_signon_message_catalog_probe;
    DedicatedSignonMessageFetchSurfaceSummary dedicated_signon_message_fetch_surface;
    DedicatedSignonMessageFetchProbeSummary dedicated_signon_message_fetch_probe;
    DedicatedSignonMultiMessageFetchSurfaceSummary dedicated_signon_multi_message_fetch_surface;
    DedicatedSignonMultiMessageFetchProbeSummary dedicated_signon_multi_message_fetch_probe;
    DedicatedSignonMessageRangeFetchSurfaceSummary dedicated_signon_message_range_surface;
    DedicatedSignonMessageRangeFetchProbeSummary dedicated_signon_message_range_probe;
    DedicatedSignonMessageCursorSurfaceSummary dedicated_signon_message_cursor_surface;
    DedicatedSignonMessageCursorProbeSummary dedicated_signon_message_cursor_probe;
    DedicatedSignonMessageCursorAdvanceSurfaceSummary
        dedicated_signon_message_cursor_advance_surface;
    DedicatedSignonMessageCursorAdvanceProbeSummary dedicated_signon_message_cursor_advance_probe;
    DedicatedSignonMessageCursorEofSurfaceSummary dedicated_signon_message_cursor_eof_surface;
    DedicatedSignonMessageCursorEofProbeSummary dedicated_signon_message_cursor_eof_probe;
    DedicatedSignonMessageCursorResumeDenialSurfaceSummary
        dedicated_signon_message_cursor_resume_denial_surface;
    DedicatedSignonMessageCursorResumeDenialProbeSummary
        dedicated_signon_message_cursor_resume_denial_probe;
    DedicatedSignonMessageCursorResumeAllowSurfaceSummary
        dedicated_signon_message_cursor_resume_allow_surface;
    DedicatedSignonMessageCursorResumeAllowProbeSummary
        dedicated_signon_message_cursor_resume_allow_probe;
    DedicatedSignonMessageCursorCarryoverSurfaceSummary
        dedicated_signon_message_cursor_carryover_surface;
    DedicatedSignonMessageCursorCarryoverProbeSummary
        dedicated_signon_message_cursor_carryover_probe;
    DedicatedSignonMessageCursorCarriedRangeSurfaceSummary
        dedicated_signon_message_cursor_carried_range_surface;
    DedicatedSignonMessageCursorCarriedRangeProbeSummary
        dedicated_signon_message_cursor_carried_range_probe;
    DedicatedSignonMessageCursorCarriedEofSurfaceSummary
        dedicated_signon_message_cursor_carried_eof_surface;
    DedicatedSignonMessageCursorCarriedEofProbeSummary
        dedicated_signon_message_cursor_carried_eof_probe;
    DedicatedSignonMessageCursorCarriedResumeAllowSurfaceSummary
        dedicated_signon_message_cursor_carried_resume_allow_surface;
    DedicatedSignonMessageCursorCarriedResumeAllowProbeSummary
        dedicated_signon_message_cursor_carried_resume_allow_probe;
    DedicatedSignonMessageCursorCarriedCheckpointSurfaceSummary
        dedicated_signon_message_cursor_carried_checkpoint_surface;
    DedicatedSignonMessageCursorCarriedCheckpointProbeSummary
        dedicated_signon_message_cursor_carried_checkpoint_probe;
    DedicatedSignonMessageCursorCarriedCheckpointResumeAllowSurfaceSummary
        dedicated_signon_message_cursor_carried_checkpoint_resume_allow_surface;
    DedicatedSignonMessageCursorCarriedCheckpointResumeAllowProbeSummary
        dedicated_signon_message_cursor_carried_checkpoint_resume_allow_probe;
    DedicatedSignonMessageCursorCarriedCheckpointResumeTokenSurfaceSummary
        dedicated_signon_message_cursor_carried_checkpoint_resume_token_surface;
    DedicatedSignonMessageCursorCarriedCheckpointResumeTokenProbeSummary
        dedicated_signon_message_cursor_carried_checkpoint_resume_token_probe;
    DedicatedSignonMessageCursorCarriedCheckpointResumeTokenClaimSurfaceSummary
        dedicated_signon_message_cursor_carried_checkpoint_resume_token_claim_surface;
    DedicatedSignonMessageCursorCarriedCheckpointResumeTokenClaimProbeSummary
        dedicated_signon_message_cursor_carried_checkpoint_resume_token_claim_probe;
    DedicatedSignonMessageCursorCarriedCheckpointClaimedResumeAllowSurfaceSummary
        dedicated_signon_message_cursor_carried_checkpoint_claimed_resume_allow_surface;
    DedicatedSignonMessageCursorCarriedCheckpointClaimedResumeAllowProbeSummary
        dedicated_signon_message_cursor_carried_checkpoint_claimed_resume_allow_probe;
    DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointBridgeSurfaceSummary
        dedicated_signon_message_cursor_carried_checkpoint_claimed_checkpoint_bridge_surface;
    DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointBridgeProbeSummary
        dedicated_signon_message_cursor_carried_checkpoint_claimed_checkpoint_bridge_probe;
    DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointResumeAllowSurfaceSummary
        dedicated_signon_message_cursor_carried_checkpoint_claimed_checkpoint_resume_allow_surface;
    DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointResumeAllowProbeSummary
        dedicated_signon_message_cursor_carried_checkpoint_claimed_checkpoint_resume_allow_probe;
    DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointResumeTokenSurfaceSummary
        dedicated_signon_message_cursor_carried_checkpoint_claimed_checkpoint_resume_token_surface;
    DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointResumeTokenProbeSummary
        dedicated_signon_message_cursor_carried_checkpoint_claimed_checkpoint_resume_token_probe;
    DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointResumeTokenClaimSurfaceSummary
        dedicated_signon_message_cursor_carried_checkpoint_claimed_checkpoint_resume_token_claim_surface;
    DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointResumeTokenClaimProbeSummary
        dedicated_signon_message_cursor_carried_checkpoint_claimed_checkpoint_resume_token_claim_probe;
    DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorResumeAllowSurfaceSummary
        dedicated_signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_resume_allow_surface;
    DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorResumeAllowProbeSummary
        dedicated_signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_resume_allow_probe;
    DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointBridgeSurfaceSummary
        dedicated_signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_bridge_surface;
    DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointBridgeProbeSummary
        dedicated_signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_bridge_probe;
    DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointResumeAllowSurfaceSummary
        dedicated_signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_allow_surface;
    DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointResumeAllowProbeSummary
        dedicated_signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_allow_probe;
    DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointResumeTokenSurfaceSummary
        dedicated_signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_surface;
    DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointResumeTokenProbeSummary
        dedicated_signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_probe;
    DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointResumeTokenClaimSurfaceSummary
        dedicated_signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_surface;
    DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointResumeTokenClaimProbeSummary
        dedicated_signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_probe;
    DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointResumeTokenClaimRangeSurfaceSummary
        dedicated_signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_range_surface;
    DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointResumeTokenClaimRangeProbeSummary
        dedicated_signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_range_probe;
    DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointResumeTokenClaimEofSurfaceSummary
        dedicated_signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_eof_surface;
    DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointResumeTokenClaimEofProbeSummary
        dedicated_signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_eof_probe;
    DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointResumeTokenClaimResumeDenialSurfaceSummary
        dedicated_signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_resume_denial_surface;
    DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointResumeTokenClaimResumeDenialProbeSummary
        dedicated_signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_resume_denial_probe;
    DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointResumeTokenClaimResumeAllowSurfaceSummary
        dedicated_signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_resume_allow_surface;
    DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointResumeTokenClaimResumeAllowProbeSummary
        dedicated_signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_resume_allow_probe;
    DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointResumeTokenClaimResumeRangeSurfaceSummary
        dedicated_signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_resume_range_surface;
    DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointResumeTokenClaimResumeRangeProbeSummary
        dedicated_signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_resume_range_probe;
    DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointResumeTokenClaimResumeEofSurfaceSummary
        dedicated_signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_resume_eof_surface;
    DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointResumeTokenClaimResumeEofProbeSummary
        dedicated_signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_resume_eof_probe;
    DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointResumeTokenClaimResumedDenialSurfaceSummary
        dedicated_signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_resumed_denial_surface;
    DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointResumeTokenClaimResumedDenialProbeSummary
        dedicated_signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_resumed_denial_probe;
    DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointResumeTokenClaimCheckpointBridgeSurfaceSummary
        dedicated_signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_bridge_surface;
    DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointResumeTokenClaimCheckpointBridgeProbeSummary
        dedicated_signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_bridge_probe;
    DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointResumeTokenClaimCheckpointResumeAllowSurfaceSummary
        dedicated_signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_allow_surface;
    DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointResumeTokenClaimCheckpointResumeAllowProbeSummary
        dedicated_signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_allow_probe;
    DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointResumeTokenClaimCheckpointResumeTokenSurfaceSummary
        dedicated_signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_surface;
    DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointResumeTokenClaimCheckpointResumeTokenProbeSummary
        dedicated_signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_probe;
    DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointResumeTokenClaimCheckpointResumeTokenClaimSurfaceSummary
        dedicated_signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_surface;
    DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointResumeTokenClaimCheckpointResumeTokenClaimProbeSummary
        dedicated_signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_probe;
    DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointResumeTokenClaimCheckpointResumeTokenClaimResumeAllowSurfaceSummary
        dedicated_signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_resume_allow_surface;
    DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointResumeTokenClaimCheckpointResumeTokenClaimResumeAllowProbeSummary
        dedicated_signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_resume_allow_probe;
    DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointResumeTokenClaimCheckpointResumeTokenClaimResumeRangeSurfaceSummary
        dedicated_signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_resume_range_surface;
    DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointResumeTokenClaimCheckpointResumeTokenClaimResumeRangeProbeSummary
        dedicated_signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_resume_range_probe;
    DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointResumeTokenClaimCheckpointResumeTokenClaimResumeEofSurfaceSummary
        dedicated_signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_resume_eof_surface;
    DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointResumeTokenClaimCheckpointResumeTokenClaimResumeEofProbeSummary
        dedicated_signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_resume_eof_probe;
    DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointResumeTokenClaimCheckpointResumeTokenClaimRangeSurfaceSummary
        dedicated_signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_range_surface;
    DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointResumeTokenClaimCheckpointResumeTokenClaimRangeProbeSummary
        dedicated_signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_range_probe;
    DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointResumeTokenClaimCheckpointResumeTokenClaimEofSurfaceSummary
        dedicated_signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_eof_surface;
    DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointResumeTokenClaimCheckpointResumeTokenClaimEofProbeSummary
        dedicated_signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_eof_probe;
    DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointResumeTokenClaimCheckpointResumeTokenClaimResumeDenialSurfaceSummary
        dedicated_signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_resume_denial_surface;
    DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointResumeTokenClaimCheckpointResumeTokenClaimResumeDenialProbeSummary
        dedicated_signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_token_claim_resume_denial_probe;
    DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointResumeTokenClaimCheckpointResumeRangeSurfaceSummary
        dedicated_signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_range_surface;
    DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointResumeTokenClaimCheckpointResumeRangeProbeSummary
        dedicated_signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_range_probe;
    DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointResumeTokenClaimCheckpointResumeEofSurfaceSummary
        dedicated_signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_eof_surface;
    DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointResumeTokenClaimCheckpointResumeEofProbeSummary
        dedicated_signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_eof_probe;
    DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointResumeTokenClaimCheckpointResumedDenialSurfaceSummary
        dedicated_signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resumed_denial_surface;
    DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointResumeTokenClaimCheckpointResumedDenialProbeSummary
        dedicated_signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resumed_denial_probe;
    DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointResumeTokenClaimCheckpointAdvanceSurfaceSummary
        dedicated_signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_advance_surface;
    DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointResumeTokenClaimCheckpointAdvanceProbeSummary
        dedicated_signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_advance_probe;
    DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointResumeTokenClaimCheckpointEofSurfaceSummary
        dedicated_signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_eof_surface;
    DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointResumeTokenClaimCheckpointEofProbeSummary
        dedicated_signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_eof_probe;
    DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointResumeTokenClaimCheckpointResumeDenialSurfaceSummary
        dedicated_signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_denial_surface;
    DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointResumeTokenClaimCheckpointResumeDenialProbeSummary
        dedicated_signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_token_claim_checkpoint_resume_denial_probe;
    DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointResumeRangeSurfaceSummary
        dedicated_signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_range_surface;
    DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointResumeRangeProbeSummary
        dedicated_signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_range_probe;
    DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointResumeEofSurfaceSummary
        dedicated_signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_eof_surface;
    DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointResumeEofProbeSummary
        dedicated_signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_eof_probe;
    DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointResumedDenialSurfaceSummary
        dedicated_signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resumed_denial_surface;
    DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointResumedDenialProbeSummary
        dedicated_signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resumed_denial_probe;
    DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointAdvanceSurfaceSummary
        dedicated_signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_advance_surface;
    DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointAdvanceProbeSummary
        dedicated_signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_advance_probe;
    DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointEofSurfaceSummary
        dedicated_signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_eof_surface;
    DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointEofProbeSummary
        dedicated_signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_eof_probe;
    DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointResumeDenialSurfaceSummary
        dedicated_signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_denial_surface;
    DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorCheckpointResumeDenialProbeSummary
        dedicated_signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_checkpoint_resume_denial_probe;
    DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorResumeRangeSurfaceSummary
        dedicated_signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_resume_range_surface;
    DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorResumeRangeProbeSummary
        dedicated_signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_resume_range_probe;
    DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorResumeEofSurfaceSummary
        dedicated_signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_resume_eof_surface;
    DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorResumeEofProbeSummary
        dedicated_signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_resume_eof_probe;
    DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorResumedDenialSurfaceSummary
        dedicated_signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_resumed_denial_surface;
    DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorResumedDenialProbeSummary
        dedicated_signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_resumed_denial_probe;
    DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorRangeSurfaceSummary
        dedicated_signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_range_surface;
    DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorRangeProbeSummary
        dedicated_signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_range_probe;
    DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorEofSurfaceSummary
        dedicated_signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_eof_surface;
    DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorEofProbeSummary
        dedicated_signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_eof_probe;
    DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorResumeDenialSurfaceSummary
        dedicated_signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_resume_denial_surface;
    DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointSuccessorResumeDenialProbeSummary
        dedicated_signon_message_cursor_carried_checkpoint_claimed_checkpoint_successor_resume_denial_probe;
    DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointResumeRangeSurfaceSummary
        dedicated_signon_message_cursor_carried_checkpoint_claimed_checkpoint_resume_range_surface;
    DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointResumeRangeProbeSummary
        dedicated_signon_message_cursor_carried_checkpoint_claimed_checkpoint_resume_range_probe;
    DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointResumeEofSurfaceSummary
        dedicated_signon_message_cursor_carried_checkpoint_claimed_checkpoint_resume_eof_surface;
    DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointResumeEofProbeSummary
        dedicated_signon_message_cursor_carried_checkpoint_claimed_checkpoint_resume_eof_probe;
    DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointResumedDenialSurfaceSummary
        dedicated_signon_message_cursor_carried_checkpoint_claimed_checkpoint_resumed_denial_surface;
    DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointResumedDenialProbeSummary
        dedicated_signon_message_cursor_carried_checkpoint_claimed_checkpoint_resumed_denial_probe;
    DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointAdvanceSurfaceSummary
        dedicated_signon_message_cursor_carried_checkpoint_claimed_checkpoint_advance_surface;
    DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointAdvanceProbeSummary
        dedicated_signon_message_cursor_carried_checkpoint_claimed_checkpoint_advance_probe;
    DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointEofSurfaceSummary
        dedicated_signon_message_cursor_carried_checkpoint_claimed_checkpoint_eof_surface;
    DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointEofProbeSummary
        dedicated_signon_message_cursor_carried_checkpoint_claimed_checkpoint_eof_probe;
    DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointResumeDenialSurfaceSummary
        dedicated_signon_message_cursor_carried_checkpoint_claimed_checkpoint_resume_denial_surface;
    DedicatedSignonMessageCursorCarriedCheckpointClaimedCheckpointResumeDenialProbeSummary
        dedicated_signon_message_cursor_carried_checkpoint_claimed_checkpoint_resume_denial_probe;
    DedicatedSignonMessageCursorCarriedCheckpointClaimedResumeRangeSurfaceSummary
        dedicated_signon_message_cursor_carried_checkpoint_claimed_resume_range_surface;
    DedicatedSignonMessageCursorCarriedCheckpointClaimedResumeRangeProbeSummary
        dedicated_signon_message_cursor_carried_checkpoint_claimed_resume_range_probe;
    DedicatedSignonMessageCursorCarriedCheckpointClaimedResumeEofSurfaceSummary
        dedicated_signon_message_cursor_carried_checkpoint_claimed_resume_eof_surface;
    DedicatedSignonMessageCursorCarriedCheckpointClaimedResumeEofProbeSummary
        dedicated_signon_message_cursor_carried_checkpoint_claimed_resume_eof_probe;
    DedicatedSignonMessageCursorCarriedCheckpointClaimedResumedDenialSurfaceSummary
        dedicated_signon_message_cursor_carried_checkpoint_claimed_resumed_denial_surface;
    DedicatedSignonMessageCursorCarriedCheckpointClaimedResumedDenialProbeSummary
        dedicated_signon_message_cursor_carried_checkpoint_claimed_resumed_denial_probe;
    DedicatedSignonMessageCursorCarriedCheckpointClaimedRangeSurfaceSummary
        dedicated_signon_message_cursor_carried_checkpoint_claimed_range_surface;
    DedicatedSignonMessageCursorCarriedCheckpointClaimedRangeProbeSummary
        dedicated_signon_message_cursor_carried_checkpoint_claimed_range_probe;
    DedicatedSignonMessageCursorCarriedCheckpointClaimedEofSurfaceSummary
        dedicated_signon_message_cursor_carried_checkpoint_claimed_eof_surface;
    DedicatedSignonMessageCursorCarriedCheckpointClaimedEofProbeSummary
        dedicated_signon_message_cursor_carried_checkpoint_claimed_eof_probe;
    DedicatedSignonMessageCursorCarriedCheckpointClaimedResumeDenialSurfaceSummary
        dedicated_signon_message_cursor_carried_checkpoint_claimed_resume_denial_surface;
    DedicatedSignonMessageCursorCarriedCheckpointClaimedResumeDenialProbeSummary
        dedicated_signon_message_cursor_carried_checkpoint_claimed_resume_denial_probe;
    DedicatedSignonMessageCursorCarriedCheckpointResumeRangeSurfaceSummary
        dedicated_signon_message_cursor_carried_checkpoint_resume_range_surface;
    DedicatedSignonMessageCursorCarriedCheckpointResumeRangeProbeSummary
        dedicated_signon_message_cursor_carried_checkpoint_resume_range_probe;
    DedicatedSignonMessageCursorCarriedCheckpointResumeEofSurfaceSummary
        dedicated_signon_message_cursor_carried_checkpoint_resume_eof_surface;
    DedicatedSignonMessageCursorCarriedCheckpointResumeEofProbeSummary
        dedicated_signon_message_cursor_carried_checkpoint_resume_eof_probe;
    DedicatedSignonMessageCursorCarriedCheckpointResumedDenialSurfaceSummary
        dedicated_signon_message_cursor_carried_checkpoint_resumed_denial_surface;
    DedicatedSignonMessageCursorCarriedCheckpointResumedDenialProbeSummary
        dedicated_signon_message_cursor_carried_checkpoint_resumed_denial_probe;
    DedicatedSignonMessageCursorCarriedCheckpointAdvanceSurfaceSummary
        dedicated_signon_message_cursor_carried_checkpoint_advance_surface;
    DedicatedSignonMessageCursorCarriedCheckpointAdvanceProbeSummary
        dedicated_signon_message_cursor_carried_checkpoint_advance_probe;
    DedicatedSignonMessageCursorCarriedCheckpointEofSurfaceSummary
        dedicated_signon_message_cursor_carried_checkpoint_eof_surface;
    DedicatedSignonMessageCursorCarriedCheckpointEofProbeSummary
        dedicated_signon_message_cursor_carried_checkpoint_eof_probe;
    DedicatedSignonMessageCursorCarriedCheckpointResumeDenialSurfaceSummary
        dedicated_signon_message_cursor_carried_checkpoint_resume_denial_surface;
    DedicatedSignonMessageCursorCarriedCheckpointResumeDenialProbeSummary
        dedicated_signon_message_cursor_carried_checkpoint_resume_denial_probe;
    DedicatedSignonMessageCursorCarriedResumeDenialSurfaceSummary
        dedicated_signon_message_cursor_carried_resume_denial_surface;
    DedicatedSignonMessageCursorCarriedResumeDenialProbeSummary
        dedicated_signon_message_cursor_carried_resume_denial_probe;
    std::string dedicated_multiplayer_readiness;
    bool ready_for_server_activation = false;
};

class HlServerModule final
{
public:
    HlServerModule();
    ~HlServerModule();

    HlServerModule(const HlServerModule&) = delete;
    HlServerModule& operator=(const HlServerModule&) = delete;

    bool Load(const std::filesystem::path& path);
    bool InitializeEngineShim(const HlServerModuleInitOptions& options);

    const HlServerModuleSummary& Summary() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace hl::game_api
