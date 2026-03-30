#include "game_api/hl_server_module.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cctype>
#include <cstdarg>
#include <cstring>
#include <cstdio>
#include <cmath>
#include <deque>
#include <iomanip>
#include <initializer_list>
#include <intrin.h>
#include <limits>
#include <optional>
#include <sstream>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "common/logger.h"
#include "common/text_encoding.h"
#include "filesystem/file_system.h"
#include "game_api/dll_module.h"
#include "brush_door_bootstrap_controller.h"
#include "entity_think_scheduler.h"
#include "entity_lump_parser.h"
#include "map_logic_dispatcher.h"
#include "entity_var_access.h"
#include "path_mover_controller.h"
#include "scripted_logic_tracker.h"
#include "scripted_movement_controller.h"
#include "track_path_resolver.h"
#include "game_api/server_command_buffer.h"
#include "game_api/server_command_dispatcher.h"
#include "server_frame_loop.h"
#include "server_bootstrap.h"
#include "sound_precache_registry.h"
#include "spawn_trace.h"
#include "world_bootstrap.h"

#pragma warning(push, 0)
#include "extdll.h"
#include "shake.h"
#pragma warning(pop)

#if defined(_MSC_VER)
#pragma intrinsic(_ReturnAddress)
#endif

namespace
{
using GiveFnptrsToDllFn = void(__stdcall*)(enginefuncs_t*, globalvars_t*);
using GetEntityAPI2Fn = int(__cdecl*)(DLL_FUNCTIONS*, int*);
using LinkEntityExportFn = void(__cdecl*)(entvars_t*);

struct ParsedVectorField
{
    bool present = false;
    bool valid = false;
    std::string raw_text;
    Vector value = Vector(0.0f, 0.0f, 0.0f);
};

struct ChangeLevelValidationBspLumpHeader
{
    std::int32_t file_offset = 0;
    std::int32_t file_length = 0;
};

struct ChangeLevelValidationBspHeader
{
    std::int32_t version = 0;
    std::array<
        ChangeLevelValidationBspLumpHeader,
        hl::game_api::detail::kBspHeaderLumpCount> lumps{};
};

static_assert(
    sizeof(ChangeLevelValidationBspHeader) == 124,
    "Changelevel validation BSP header layout mismatch.");

enum class RuntimeEntitySupportState
{
    kPending,
    kSpawnedSuccessfully,
    kRemovedDuringSpawn,
    kDeferredUnsupported,
    kFailedDuringSpawn,
};

enum class RuntimeEntityLifecycleState
{
    kUnknown,
    kActiveSupported,
    kPassiveSupported,
    kDetectedButDeferred,
    kRemovedByGameLogic,
};

struct RuntimeEntityRecord
{
    std::size_t parse_index = 0;
    int edict_index = -1;
    bool dynamic_runtime = false;
    bool in_use = false;
    bool removed = false;
    std::string classname;
    std::string target;
    std::string targetname;
    std::string message;
    std::string origin_raw;
    Vector origin = Vector(0.0f, 0.0f, 0.0f);
    bool has_origin = false;
    std::string angles_raw;
    Vector angles = Vector(0.0f, 0.0f, 0.0f);
    bool has_angles = false;
    Vector movedir = Vector(0.0f, 0.0f, 0.0f);
    bool has_movedir = false;
    Vector mins = Vector(0.0f, 0.0f, 0.0f);
    Vector maxs = Vector(0.0f, 0.0f, 0.0f);
    bool has_size = false;
    std::string model;
    int modelindex = 0;
    bool spawned = false;
    bool deferred = false;
    float nextthink = 0.0f;
    float ltime = 0.0f;
    int flags = 0;
    int solid = 0;
    int movetype = 0;
    int effects = 0;
    float health = 0.0f;
    bool health_available = false;
    bool has_private_data = false;
    bool private_data_present = false;
    bool activation_candidate = false;
    bool scheduled_for_think = false;
    bool spawn_attempted = false;
    bool think_attempted = false;
    bool think_succeeded = false;
    int last_think_frame = -1;
    bool can_think = false;
    bool can_receive_use = false;
    bool can_emit_targets = false;
    int pending_scheduled_outputs = 0;
    bool triggered_this_frame = false;
    bool blocked_or_deferred = false;
    std::size_t target_resolutions = 0;
    std::size_t use_attempts = 0;
    std::size_t use_successes = 0;
    std::size_t use_deferred = 0;
    std::size_t use_failures = 0;
    int last_triggered_frame = -1;
    hl::game_api::MapLogicSupportState map_logic_support_state =
        hl::game_api::MapLogicSupportState::kDetectedButDeferred;
    hl::game_api::detail::ScriptedLogicStateTracker scripted_logic;
    std::vector<std::string> scripted_path_links;
    std::string scripted_actor_name;
    std::string scripted_actor_classname;
    std::string scripted_play;
    std::string scripted_idle;
    std::string scripted_blocked_reason;
    std::string scripted_movement_stage;
    std::string scripted_movement_support_state;
    bool scripted_actor_resolved = false;
    int scripted_actor_edict = -1;
    bool scripted_arrived = false;
    bool scripted_animation_ready = false;
    int scripted_move_to = 0;
    int scripted_spawnflags = 0;
    float scripted_radius = 0.0f;
    RuntimeEntitySupportState support_state = RuntimeEntitySupportState::kPending;
    RuntimeEntityLifecycleState lifecycle_state = RuntimeEntityLifecycleState::kUnknown;
    std::vector<std::string> unhandled_keyvalues;
    std::string source_preview;
    std::string note;
};

struct EntityBootstrapContext
{
    bool attempted = false;
    bool entities_lump_parsed = false;
    bool partially_parsed = false;
    std::string failure_reason;
    std::vector<hl::game_api::detail::EntityDefinition> parsed_entities;
    std::vector<hl::game_api::detail::EntityParseError> parse_errors;
    std::vector<RuntimeEntityRecord> runtime_entities;
    std::size_t worldspawn_count = 0;
    bool first_entity_is_worldspawn = false;
    std::size_t runtime_entities_allocated = 0;
    std::size_t keyvalues_dispatched = 0;
    std::size_t keyvalues_handled = 0;
    std::size_t spawn_attempts = 0;
    std::size_t successful_spawns = 0;
    std::size_t removed_entities = 0;
    std::size_t deferred_entities = 0;
    std::vector<hl::game_api::EntityClassCountSummary> top_classname_counts;
    std::vector<hl::game_api::EntityClassSupportSummary> classname_support_summary;
    std::vector<hl::game_api::InvokedEngineCallback> newly_exercised_engine_callbacks;
    std::unordered_map<std::string, std::size_t> callback_counts_before;
    std::size_t next_dynamic_runtime_index = 1000000;
};

struct RandomCallsiteRecord
{
    std::string callback_name;
    const void* return_address = nullptr;
    std::string context;
    std::size_t call_count = 0;
};

struct DeterministicRandomDiagnostics
{
    static constexpr std::size_t kMaxRecordedCallsites = 8;

    void Reset()
    {
        state = 0x4C43475u;
        random_long_calls = 0;
        random_float_calls = 0;
        recorded_callsites.clear();
    }

    std::uint32_t NextRaw()
    {
        state = state * 1664525u + 1013904223u;
        return state;
    }

    int32 NextLong(int32 low, int32 high)
    {
        const std::int64_t minimum = std::min<std::int64_t>(low, high);
        const std::int64_t maximum = std::max<std::int64_t>(low, high);
        const std::uint64_t span =
            static_cast<std::uint64_t>(maximum - minimum) + 1u;
        const std::uint64_t sample = static_cast<std::uint64_t>(NextRaw());
        return static_cast<int32>(minimum + static_cast<std::int64_t>(sample % span));
    }

    float NextFloat(float low, float high)
    {
        const float minimum = std::min(low, high);
        const float maximum = std::max(low, high);
        if (minimum == maximum)
        {
            return minimum;
        }

        const float fraction =
            static_cast<float>(NextRaw())
            / static_cast<float>(std::numeric_limits<std::uint32_t>::max());
        return minimum + ((maximum - minimum) * fraction);
    }

    bool RecordCallsite(
        std::string_view callback_name,
        const void* return_address,
        std::string_view context)
    {
        for (RandomCallsiteRecord& record : recorded_callsites)
        {
            if (record.callback_name == callback_name && record.return_address == return_address)
            {
                ++record.call_count;
                return false;
            }
        }

        if (recorded_callsites.size() >= kMaxRecordedCallsites)
        {
            return false;
        }

        recorded_callsites.push_back({
            std::string(callback_name),
            return_address,
            std::string(context),
            1,
        });
        return true;
    }

    std::uint32_t state = 0x4C43475u;
    std::uint64_t random_long_calls = 0;
    std::uint64_t random_float_calls = 0;
    std::vector<RandomCallsiteRecord> recorded_callsites;
};

struct StableIndexRegistry
{
    void Reset()
    {
        indices.clear();
        values.clear();
    }

    int Upsert(std::string value, bool* duplicate = nullptr)
    {
        if (duplicate != nullptr)
        {
            *duplicate = false;
        }

        if (value.empty())
        {
            return 0;
        }

        const auto existing = indices.find(value);
        if (existing != indices.end())
        {
            if (duplicate != nullptr)
            {
                *duplicate = true;
            }

            return existing->second;
        }

        const int index = static_cast<int>(values.size()) + 1;
        indices.emplace(value, index);
        values.push_back(std::move(value));
        return index;
    }

    int IndexOf(std::string_view value) const
    {
        const auto it = indices.find(std::string(value));
        return it == indices.end() ? 0 : it->second;
    }

    const std::string* ValueOf(int index) const noexcept
    {
        if (index <= 0 || static_cast<std::size_t>(index) > values.size())
        {
            return nullptr;
        }

        return &values[static_cast<std::size_t>(index - 1)];
    }

    std::size_t Size() const noexcept
    {
        return values.size();
    }

    std::unordered_map<std::string, int> indices;
    std::deque<std::string> values;
};

struct EngineShimState
{
    struct LoadedFileBuffer
    {
        std::unique_ptr<byte[]> bytes;
        int length = 0;
        std::filesystem::path path;
    };

    enginefuncs_t enginefuncs{};
    globalvars_t globalvars{};
    DLL_FUNCTIONS dll_functions{};
    hl::game_api::CvarRegistry cvar_registry;
    hl::game_api::ServerCommandBuffer command_buffer;
    hl::game_api::ServerCommandDispatchStats command_dispatch_stats;
    bool command_execute_in_progress = false;
    std::unordered_map<std::string, std::size_t> callback_counts;
    std::vector<std::string> callback_order;
    std::filesystem::path game_directory;
    hl::game_api::DllModule* module = nullptr;
    hl::game_api::detail::ServerState server_state;
    hl::game_api::detail::EngineStringPool string_pool;
    hl::game_api::detail::PrecacheRegistry precache_registry;
    hl::game_api::detail::SoundPrecacheRegistry sound_precache_registry;
    hl::game_api::detail::EdictStore edict_store;
    hl::filesystem::FileSystem file_system;
    hl::game_api::detail::WorldModelContext world_context;
    EntityBootstrapContext entity_bootstrap;
    hl::game_api::detail::WorldspawnSpawnDiagnostics worldspawn_spawn_diagnostics;
    hl::game_api::detail::ServerActivationDiagnostics server_activation_diagnostics;
    hl::game_api::ServerActivationStateSummary server_activation_state;
    hl::game_api::detail::ServerFrameDiagnostics server_frame_diagnostics;
    hl::game_api::detail::EntityThinkDiagnostics entity_think_diagnostics;
    hl::game_api::detail::FrameMessageBuffer frame_message_buffer;
    hl::game_api::FrameBootstrapOptions frame_bootstrap_options;
    hl::game_api::ServerFrameLoopStateSummary server_frame_loop_state;
    hl::game_api::EntityThinkSchedulerStateSummary entity_think_scheduler_state;
    hl::game_api::MapLogicDispatcherStateSummary map_logic_dispatcher_state;
    hl::game_api::ChangeLevelTransitionSummary changelevel_transition_state;
    hl::game_api::ScriptedLogicStateSummary scripted_logic_state;
    hl::game_api::ScriptedMovementStateSummary scripted_movement_state;
    DeterministicRandomDiagnostics random_diagnostics;
    std::unordered_map<void*, LoadedFileBuffer> loaded_files;
    std::unordered_set<std::string> path_track_terminal_dead_end_targets;
    std::unordered_map<int, std::string> light_styles;
    StableIndexRegistry generic_precache_registry;
    StableIndexRegistry event_precache_registry;
    StableIndexRegistry decal_registry;
    StableIndexRegistry user_message_registry;
};

EngineShimState* g_active_shim_state = nullptr;

bool SafeCallSpawn(int (*function)(edict_t*), edict_t* entity, int* result_value);
bool SafeCallKeyValue(void (*function)(edict_t*, KeyValueData*), edict_t* entity, KeyValueData* kvd);
std::string FormatSpawnTraceEvent(const hl::game_api::detail::SpawnTraceEvent& event);
bool SafeCallWorldspawnSpawnSeh(
    int (*function)(edict_t*),
    edict_t* entity,
    int* result_value,
    unsigned int* seh_code);
bool SafeCallServerActivateSeh(
    void (*function)(edict_t*, int, int),
    edict_t* edict_list,
    int edict_count,
    int client_max,
    unsigned int* seh_code);
bool SafeCallStartFrameSeh(void (*function)(), unsigned int* seh_code);
bool SafeCallThinkSeh(void (*function)(edict_t*), edict_t* entity, unsigned int* seh_code);
bool SafeCallUseSeh(
    void (*function)(edict_t*, edict_t*),
    edict_t* used_entity,
    edict_t* other_entity,
    unsigned int* seh_code);
void StubMessageBegin(int msg_dest, int msg_type, const float* origin, edict_t* entity);
void StubMessageEnd();
void StubWriteByte(int value);
void StubWriteShort(int value);
void StubWriteString(const char* value);
void LogWorldspawnDistinctCallbackTail(const EngineShimState& state);
void LogWorldspawnPrecacheTail(const EngineShimState& state);
void LogServerActivationTraceTail(const EngineShimState& state);
void LogServerActivationDistinctCallbackTail(const EngineShimState& state);
void LogServerFrameTraceTail(const EngineShimState& state);
void LogEntityThinkTraceTail(const EngineShimState& state);
void LogRandomDiagnostics(const EngineShimState& state, std::string_view header);
std::string BuildRandomDiagnosticContext(const EngineShimState& state);
std::string BuildEntitySnapshotSummary(const hl::game_api::detail::EntityStateSnapshot& snapshot);
void EmitAiConsoleDiagnostic(std::string_view message);
bool EqualsIgnoreCase(std::string_view left, std::string_view right);
const hl::game_api::detail::EntityDefinition* FindParsedEntityDefinitionByOrdinal(
    const EngineShimState& state,
    std::size_t ordinal);
const hl::game_api::detail::EntityDefinition* FindParsedTargetDefinitionByTargetname(
    const EngineShimState& state,
    std::string_view target_name,
    std::string_view classname_filter);
ParsedVectorField ParseAnglesField(const hl::game_api::detail::EntityDefinition& entity);
ParsedVectorField ParseOriginField(const hl::game_api::detail::EntityDefinition& entity);
bool ParseStrictFloat(std::string_view text, float* value);
void EnsureChangeLevelTargetValidation(EngineShimState& state);
void EnsureChangeLevelLifecycleGate(EngineShimState& state);
void RefreshChangeLevelLifecycleEntry(EngineShimState& state);
void RefreshChangeLevelLifecycleDispatch(EngineShimState& state);
void RefreshChangeLevelLifecycleExecution(EngineShimState& state);
void RefreshChangeLevelBootstrapPlan(EngineShimState& state);
void RefreshChangeLevelLandmarkTransform(EngineShimState& state);
void RefreshChangeLevelProjectedCarriedOrigin(EngineShimState& state);
void RefreshChangeLevelProjectedCarriedOrientation(EngineShimState& state);
void RefreshChangeLevelProjectedTransferSnapshot(EngineShimState& state);
void RefreshChangeLevelPlayerTransferApplyPlan(EngineShimState& state);
void RefreshChangeLevelPlayerTransferWriteSet(EngineShimState& state);
void RefreshChangeLevelPlayerTransferDeferredApplyGate(EngineShimState& state);
void RefreshChangeLevelPlayerTransferGateOpenCheckpoint(EngineShimState& state);
void RefreshChangeLevelPlayerTransferCheckpointSignalContract(EngineShimState& state);
hl::game_api::ChangeLevelTransitionSummary::ChangeLevelProjectedTransferSnapshotSummary
BuildChangeLevelProjectedTransferSnapshot(
    const hl::game_api::ChangeLevelTransitionSummary::ChangeLevelBootstrapPlanSummary& plan,
    const hl::game_api::ChangeLevelTransitionSummary::ChangeLevelProjectedCarriedOriginSummary&
        projected_origin,
    const hl::game_api::ChangeLevelTransitionSummary::ChangeLevelProjectedCarriedOrientationSummary&
        projected_orientation);
hl::game_api::ChangeLevelTransitionSummary::ChangeLevelPlayerTransferApplyPlanSummary
BuildChangeLevelPlayerTransferApplyPlan(
    const hl::game_api::ChangeLevelTransitionSummary::ChangeLevelProjectedTransferSnapshotSummary&
        snapshot);
hl::game_api::ChangeLevelTransitionSummary::ChangeLevelPlayerTransferWriteSetSummary
BuildChangeLevelPlayerTransferWriteSet(
    const hl::game_api::ChangeLevelTransitionSummary::ChangeLevelPlayerTransferApplyPlanSummary&
        plan);
hl::game_api::ChangeLevelTransitionSummary::ChangeLevelPlayerTransferDeferredApplyGateSummary
BuildChangeLevelPlayerTransferDeferredApplyGate(
    const hl::game_api::ChangeLevelTransitionSummary::ChangeLevelPlayerTransferWriteSetSummary&
        write_set);
hl::game_api::ChangeLevelTransitionSummary::ChangeLevelPlayerTransferGateOpenCheckpointSummary
BuildChangeLevelPlayerTransferGateOpenCheckpoint(
    const hl::game_api::ChangeLevelTransitionSummary::
        ChangeLevelPlayerTransferDeferredApplyGateSummary& gate);
hl::game_api::ChangeLevelTransitionSummary::ChangeLevelPlayerTransferCheckpointSignalContractSummary
BuildChangeLevelPlayerTransferCheckpointSignalContract(
    const hl::game_api::ChangeLevelTransitionSummary::
        ChangeLevelPlayerTransferGateOpenCheckpointSummary& checkpoint);
bool ParseStrictVector3(std::string_view text, Vector* value);
float NormalizeAngleDegrees(float value);
std::string FormatScalar(float value);
std::string FormatVector(const Vector& value);

const char* BoolToYesNo(bool value)
{
    return value ? "yes" : "no";
}

std::string DescribeChangeLevelSource(
    const hl::game_api::ChangeLevelTransitionSummary& summary)
{
    if (!summary.source_classname.empty())
    {
        return summary.source_classname;
    }

    if (summary.source_edict_index >= 0)
    {
        return "edict#" + std::to_string(summary.source_edict_index);
    }

    return "<none>";
}

std::string FormatChangeLevelCandidate(
    const hl::game_api::ChangeLevelTransitionSummary& summary)
{
    return "{map="
        + (summary.target_map.empty() ? std::string("<none>") : summary.target_map)
        + ", landmark="
        + (summary.landmark.empty() ? std::string("<none>") : summary.landmark)
        + ", source=" + DescribeChangeLevelSource(summary)
        + "}";
}

std::string FormatChangeLevelTransitionIntentSummary(
    const hl::game_api::ChangeLevelTransitionSummary& summary)
{
    return std::string("captured=") + BoolToYesNo(summary.transition_intent_captured)
        + ", consumed=" + BoolToYesNo(summary.transition_intent_consumed)
        + ", handoffLatched="
        + BoolToYesNo(summary.pre_changelevel_handoff.handoff_latched)
        + ", worldFrozen="
        + BoolToYesNo(summary.pre_changelevel_handoff.world_frozen)
        + ", stopRequested="
        + BoolToYesNo(summary.pre_changelevel_handoff.stop_requested)
        + ", action="
        + (summary.transition_intent_action.empty()
            ? std::string("<none>")
            : summary.transition_intent_action)
        + ", requestedMap="
        + (summary.target_map.empty() ? std::string("<none>") : summary.target_map)
        + ", landmark="
        + (summary.landmark.empty() ? std::string("<none>") : summary.landmark)
        + ", requestFrame="
        + (summary.transition_intent_request_frame >= 0
            ? std::to_string(summary.transition_intent_request_frame)
            : std::string("<none>"))
        + ", requestTime="
        + (summary.transition_intent_captured
            ? std::to_string(summary.transition_intent_request_time)
            : std::string("<none>"))
        ;
}

std::string FormatPreChangeLevelHandoffSummary(
    const hl::game_api::ChangeLevelTransitionSummary& summary)
{
    const hl::game_api::ChangeLevelTransitionSummary::PreChangeLevelHandoffSummary& handoff =
        summary.pre_changelevel_handoff;
    return std::string("active=") + BoolToYesNo(handoff.active)
        + ", handoffLatched=" + BoolToYesNo(handoff.handoff_latched)
        + ", worldFrozen=" + BoolToYesNo(handoff.world_frozen)
        + ", stopRequested=" + BoolToYesNo(handoff.stop_requested)
        + ", action="
        + (handoff.action.empty() ? std::string("<none>") : handoff.action)
        + ", requestedMap="
        + (summary.target_map.empty() ? std::string("<none>") : summary.target_map)
        + ", landmark="
        + (summary.landmark.empty() ? std::string("<none>") : summary.landmark)
        + ", source=" + DescribeChangeLevelSource(summary)
        + ", requestFrame="
        + (handoff.request_frame >= 0 ? std::to_string(handoff.request_frame) : std::string("<none>"))
        + ", requestTime="
        + (handoff.active ? std::to_string(handoff.request_time) : std::string("<none>"))
        + ", worldState="
        + (handoff.world_state.empty() ? std::string("<none>") : handoff.world_state)
        + ", mapLoad=" + BoolToYesNo(handoff.map_load_performed);
}

std::string FormatChangeLevelTargetValidationSummary(
    const hl::game_api::ChangeLevelTransitionSummary& summary)
{
    const hl::game_api::ChangeLevelTransitionSummary::ChangeLevelTargetValidationSummary& validation =
        summary.target_validation;
    std::string line =
        std::string("currentMap=")
        + (validation.current_map.empty() ? std::string("<none>") : validation.current_map)
        + ", requestedMap="
        + (validation.requested_map.empty() ? std::string("<none>") : validation.requested_map)
        + ", targetMapExists=" + BoolToYesNo(validation.target_map_exists)
        + ", landmark="
        + (validation.landmark.empty() ? std::string("<none>") : validation.landmark)
        + ", currentLandmark=" + BoolToYesNo(validation.current_landmark_found)
        + ", targetLandmark=" + BoolToYesNo(validation.target_landmark_found)
        + ", entityParse=" + (validation.entity_parse_succeeded ? "ok" : "fail")
        + ", action="
        + (validation.action.empty() ? std::string("<none>") : validation.action);
    if (!validation.missing_component.empty())
    {
        line += ", missing=" + validation.missing_component;
    }
    if (!validation.detail.empty())
    {
        line += ", note=" + validation.detail;
    }

    return line;
}

bool IsChangeLevelTargetValidationValid(
    const hl::game_api::ChangeLevelTransitionSummary::ChangeLevelTargetValidationSummary& validation)
{
    return validation.attempted
        && !validation.current_map.empty()
        && !validation.requested_map.empty()
        && validation.target_map_exists
        && !validation.landmark.empty()
        && validation.current_landmark_found
        && validation.target_landmark_found
        && validation.entity_parse_succeeded
        && validation.missing_component.empty();
}

std::string FormatChangeLevelLifecycleGateSummary(
    const hl::game_api::ChangeLevelTransitionSummary& summary)
{
    const hl::game_api::ChangeLevelTransitionSummary::ChangeLevelLifecycleGateSummary& gate =
        summary.lifecycle_gate;
    std::string line =
        std::string("intentConsumed=") + BoolToYesNo(gate.intent_consumed)
        + ", targetValidation=" + BoolToYesNo(gate.target_validation_passed)
        + ", bootstrapAllowed=" + BoolToYesNo(gate.bootstrap_allowed)
        + ", requestedMap="
        + (gate.requested_map.empty() ? std::string("<none>") : gate.requested_map)
        + ", landmark="
        + (gate.landmark.empty() ? std::string("<none>") : gate.landmark)
        + ", action="
        + (gate.action.empty() ? std::string("<none>") : gate.action);
    if (!gate.reason.empty())
    {
        line += ", reason=" + gate.reason;
    }

    return line;
}

std::string FormatChangeLevelLifecycleEntrySummary(
    const hl::game_api::ChangeLevelTransitionSummary& summary)
{
    const hl::game_api::ChangeLevelTransitionSummary::ChangeLevelLifecycleEntrySummary& entry =
        summary.lifecycle_entry;
    std::string line =
        std::string("gateChecked=") + BoolToYesNo(entry.gate_checked)
        + ", gatePassed=" + BoolToYesNo(entry.gate_passed)
        + ", eligible=" + BoolToYesNo(entry.eligible)
        + ", blockedByStopMode=" + BoolToYesNo(entry.blocked_by_stop_mode)
        + ", requestedMap="
        + (entry.requested_map.empty() ? std::string("<none>") : entry.requested_map)
        + ", landmark="
        + (entry.landmark.empty() ? std::string("<none>") : entry.landmark)
        + ", action="
        + (entry.action.empty() ? std::string("<none>") : entry.action);
    if (!entry.short_circuit_reason.empty())
    {
        line += ", shortCircuitReason=" + entry.short_circuit_reason;
    }

    return line;
}

std::string FormatChangeLevelLifecycleDispatchSummary(
    const hl::game_api::ChangeLevelTransitionSummary& summary)
{
    const hl::game_api::ChangeLevelTransitionSummary::ChangeLevelLifecycleDispatchSummary& dispatch =
        summary.lifecycle_dispatch;
    std::string line =
        std::string("dispatchChecked=") + BoolToYesNo(dispatch.dispatch_checked)
        + ", dispatchAllowed=" + BoolToYesNo(dispatch.dispatch_allowed)
        + ", dispatchBlocked=" + BoolToYesNo(dispatch.dispatch_blocked)
        + ", decisionSource="
        + (dispatch.decision_source.empty()
            ? std::string("<none>")
            : dispatch.decision_source)
        + ", requestedMap="
        + (dispatch.requested_map.empty() ? std::string("<none>") : dispatch.requested_map)
        + ", landmark="
        + (dispatch.landmark.empty() ? std::string("<none>") : dispatch.landmark)
        + ", action="
        + (dispatch.action.empty() ? std::string("<none>") : dispatch.action);
    if (!dispatch.short_circuit_reason.empty())
    {
        line += ", shortCircuitReason=" + dispatch.short_circuit_reason;
    }

    return line;
}

std::string FormatChangeLevelLifecycleExecutionSummary(
    const hl::game_api::ChangeLevelTransitionSummary& summary)
{
    const hl::game_api::ChangeLevelTransitionSummary::ChangeLevelLifecycleExecutionSummary& execution =
        summary.lifecycle_execution;
    std::string line =
        std::string("executionChecked=") + BoolToYesNo(execution.execution_checked)
        + ", executionArmed=" + BoolToYesNo(execution.execution_armed)
        + ", executionSkipped=" + BoolToYesNo(execution.execution_skipped)
        + ", decisionSource="
        + (execution.decision_source.empty()
            ? std::string("<none>")
            : execution.decision_source)
        + ", requestedMap="
        + (execution.requested_map.empty() ? std::string("<none>") : execution.requested_map)
        + ", landmark="
        + (execution.landmark.empty() ? std::string("<none>") : execution.landmark)
        + ", action="
        + (execution.action.empty() ? std::string("<none>") : execution.action);
    if (!execution.short_circuit_reason.empty())
    {
        line += ", shortCircuitReason=" + execution.short_circuit_reason;
    }

    return line;
}

std::string FormatChangeLevelBootstrapPlanSummary(
    const hl::game_api::ChangeLevelTransitionSummary& summary)
{
    const hl::game_api::ChangeLevelTransitionSummary::ChangeLevelBootstrapPlanSummary& plan =
        summary.changelevel_bootstrap_plan;
    std::string line =
        std::string("prepared=") + BoolToYesNo(plan.prepared)
        + ", skipped=" + BoolToYesNo(plan.skipped)
        + ", decisionSource="
        + (plan.decision_source.empty()
            ? std::string("<none>")
            : plan.decision_source)
        + ", action="
        + (plan.action.empty() ? std::string("<none>") : plan.action);
    if (plan.prepared)
    {
        line += ", currentMap="
            + (plan.current_map.empty() ? std::string("<none>") : plan.current_map)
            + ", requestedMap="
            + (plan.requested_map.empty() ? std::string("<none>") : plan.requested_map)
            + ", targetBspPath="
            + (plan.target_bsp_path.empty() ? std::string("<none>") : plan.target_bsp_path)
            + ", landmark="
            + (plan.landmark.empty() ? std::string("<none>") : plan.landmark)
            + ", targetWorldspawnPresent=" + BoolToYesNo(plan.target_worldspawn_present)
            + ", targetEntityParse=" + (plan.target_entity_parse_ok ? "ok" : "fail")
            + ", currentLandmarkOrigin="
            + (plan.current_landmark_origin_available
                ? plan.current_landmark_origin
                : std::string("<none>"))
            + ", targetLandmarkOrigin="
            + (plan.target_landmark_origin_available
                ? plan.target_landmark_origin
                : std::string("<none>"));
        return line;
    }

    if (!plan.requested_map.empty())
    {
        line += ", requestedMap=" + plan.requested_map;
    }
    if (!plan.landmark.empty())
    {
        line += ", landmark=" + plan.landmark;
    }
    if (!plan.short_circuit_reason.empty())
    {
        line += ", shortCircuitReason=" + plan.short_circuit_reason;
    }

    return line;
}

std::string FormatChangeLevelLandmarkTransformSummary(
    const hl::game_api::ChangeLevelTransitionSummary& summary)
{
    const hl::game_api::ChangeLevelTransitionSummary::ChangeLevelLandmarkTransformSummary&
        transform = summary.changelevel_landmark_transform;
    std::string line =
        std::string("transformComputed=") + BoolToYesNo(transform.transform_computed)
        + ", skipped=" + BoolToYesNo(transform.skipped)
        + ", decisionSource="
        + (transform.decision_source.empty()
            ? std::string("<none>")
            : transform.decision_source);
    if (transform.transform_computed)
    {
        line += ", currentLandmarkOrigin="
            + (transform.current_landmark_origin_available
                ? transform.current_landmark_origin
                : std::string("<none>"))
            + ", targetLandmarkOrigin="
            + (transform.target_landmark_origin_available
                ? transform.target_landmark_origin
                : std::string("<none>"))
            + ", translationDelta="
            + (transform.translation_delta.empty()
                ? std::string("<none>")
                : transform.translation_delta);
    }
    if (!transform.short_circuit_reason.empty())
    {
        line += ", shortCircuitReason=" + transform.short_circuit_reason;
    }

    line += ", action="
        + (transform.action.empty() ? std::string("<none>") : transform.action);
    return line;
}

std::string FormatChangeLevelProjectedCarriedOriginSummary(
    const hl::game_api::ChangeLevelTransitionSummary& summary)
{
    const hl::game_api::ChangeLevelTransitionSummary::ChangeLevelProjectedCarriedOriginSummary&
        projected = summary.changelevel_projected_carried_origin;
    std::string line =
        std::string("projected=") + BoolToYesNo(projected.projected)
        + ", skipped=" + BoolToYesNo(projected.skipped)
        + ", decisionSource="
        + (projected.decision_source.empty()
            ? std::string("<none>")
            : projected.decision_source);
    if (projected.projected)
    {
        line += ", currentCarriedOrigin="
            + (projected.current_carried_origin_available
                ? projected.current_carried_origin
                : std::string("<none>"))
            + ", translationDelta="
            + (projected.translation_delta.empty()
                ? std::string("<none>")
                : projected.translation_delta)
            + ", projectedTargetOrigin="
            + (projected.projected_target_origin.empty()
                ? std::string("<none>")
                : projected.projected_target_origin);
    }
    if (!projected.short_circuit_reason.empty())
    {
        line += ", shortCircuitReason=" + projected.short_circuit_reason;
    }

    line += ", action="
        + (projected.action.empty() ? std::string("<none>") : projected.action);
    return line;
}

std::string FormatChangeLevelProjectedCarriedOrientationSummary(
    const hl::game_api::ChangeLevelTransitionSummary& summary)
{
    const hl::game_api::ChangeLevelTransitionSummary::ChangeLevelProjectedCarriedOrientationSummary&
        projected = summary.changelevel_projected_carried_orientation;
    std::string line =
        std::string("projected=") + BoolToYesNo(projected.projected)
        + ", skipped=" + BoolToYesNo(projected.skipped)
        + ", decisionSource="
        + (projected.decision_source.empty()
            ? std::string("<none>")
            : projected.decision_source);
    if (projected.projected)
    {
        line += ", currentLandmarkAngles="
            + (projected.current_landmark_angles_available
                ? projected.current_landmark_angles
                : std::string("<none>"))
            + ", targetLandmarkAngles="
            + (projected.target_landmark_angles_available
                ? projected.target_landmark_angles
                : std::string("<none>"))
            + ", currentCarriedYaw="
            + (projected.current_carried_yaw_available
                ? projected.current_carried_yaw
                : std::string("<none>"))
            + ", yawDelta="
            + (projected.yaw_delta.empty() ? std::string("<none>") : projected.yaw_delta)
            + ", projectedTargetYaw="
            + (projected.projected_target_yaw.empty()
                ? std::string("<none>")
                : projected.projected_target_yaw);
    }
    if (!projected.short_circuit_reason.empty())
    {
        line += ", shortCircuitReason=" + projected.short_circuit_reason;
    }

    line += ", action="
        + (projected.action.empty() ? std::string("<none>") : projected.action);
    return line;
}

std::string FormatChangeLevelProjectedTransferSnapshotSummary(
    const hl::game_api::ChangeLevelTransitionSummary& summary)
{
    const hl::game_api::ChangeLevelTransitionSummary::ChangeLevelProjectedTransferSnapshotSummary&
        snapshot = summary.changelevel_projected_transfer_snapshot;
    std::string line =
        std::string("prepared=") + BoolToYesNo(snapshot.prepared)
        + ", skipped=" + BoolToYesNo(snapshot.skipped);
    if (!snapshot.decision_source.empty())
    {
        line += ", decisionSource=" + snapshot.decision_source;
    }
    if (snapshot.prepared)
    {
        line += ", currentMap="
            + (snapshot.current_map.empty() ? std::string("<none>") : snapshot.current_map)
            + ", requestedMap="
            + (snapshot.requested_map.empty() ? std::string("<none>") : snapshot.requested_map)
            + ", targetBspPath="
            + (snapshot.target_bsp_path.empty()
                ? std::string("<none>")
                : snapshot.target_bsp_path)
            + ", landmark="
            + (snapshot.landmark.empty() ? std::string("<none>") : snapshot.landmark)
            + ", projectedTargetOrigin="
            + (snapshot.projected_target_origin.empty()
                ? std::string("<none>")
                : snapshot.projected_target_origin)
            + ", projectedTargetYaw="
            + (snapshot.projected_target_yaw.empty()
                ? std::string("<none>")
                : snapshot.projected_target_yaw)
            + ", targetWorldspawnPresent=" + BoolToYesNo(snapshot.target_worldspawn_present)
            + ", targetEntityParse=" + (snapshot.target_entity_parse_ok ? "ok" : "fail");
    }
    if (!snapshot.short_circuit_reason.empty())
    {
        line += ", shortCircuitReason=" + snapshot.short_circuit_reason;
    }

    line += ", transferReady=" + std::string(BoolToYesNo(snapshot.transfer_ready))
        + ", action="
        + (snapshot.action.empty() ? std::string("<none>") : snapshot.action);
    return line;
}

std::string FormatChangeLevelPlayerTransferApplyPlanSummary(
    const hl::game_api::ChangeLevelTransitionSummary& summary)
{
    const hl::game_api::ChangeLevelTransitionSummary::ChangeLevelPlayerTransferApplyPlanSummary&
        plan = summary.changelevel_player_transfer_apply_plan;
    std::string line =
        std::string("prepared=") + BoolToYesNo(plan.prepared)
        + ", skipped=" + BoolToYesNo(plan.skipped);
    if (!plan.decision_source.empty())
    {
        line += ", decisionSource=" + plan.decision_source;
    }
    if (!plan.apply_target.empty())
    {
        line += ", applyTarget=" + plan.apply_target;
    }
    if (plan.prepared)
    {
        line += ", currentMap="
            + (plan.current_map.empty() ? std::string("<none>") : plan.current_map)
            + ", requestedMap="
            + (plan.requested_map.empty() ? std::string("<none>") : plan.requested_map)
            + ", targetBspPath="
            + (plan.target_bsp_path.empty() ? std::string("<none>") : plan.target_bsp_path)
            + ", landmark="
            + (plan.landmark.empty() ? std::string("<none>") : plan.landmark)
            + ", targetPlayerOrigin="
            + (plan.target_player_origin.empty()
                ? std::string("<none>")
                : plan.target_player_origin)
            + ", targetPlayerYaw="
            + (plan.target_player_yaw.empty()
                ? std::string("<none>")
                : plan.target_player_yaw)
            + ", originWritePrepared=" + BoolToYesNo(plan.origin_write_prepared)
            + ", yawWritePrepared=" + BoolToYesNo(plan.yaw_write_prepared)
            + ", inventoryWritePrepared=" + BoolToYesNo(plan.inventory_write_prepared)
            + ", velocityWritePrepared=" + BoolToYesNo(plan.velocity_write_prepared)
            + ", targetWorldspawnPresent=" + BoolToYesNo(plan.target_worldspawn_present)
            + ", targetEntityParse=" + (plan.target_entity_parse_ok ? "ok" : "fail");
    }
    if (!plan.short_circuit_reason.empty())
    {
        line += ", shortCircuitReason=" + plan.short_circuit_reason;
    }

    line += ", applyReady=" + std::string(BoolToYesNo(plan.apply_ready))
        + ", action=" + (plan.action.empty() ? std::string("<none>") : plan.action);
    return line;
}

std::string FormatChangeLevelPlayerTransferWriteSetSummary(
    const hl::game_api::ChangeLevelTransitionSummary& summary)
{
    const hl::game_api::ChangeLevelTransitionSummary::ChangeLevelPlayerTransferWriteSetSummary&
        write_set = summary.changelevel_player_transfer_write_set;
    std::string line =
        std::string("prepared=") + BoolToYesNo(write_set.prepared)
        + ", skipped=" + BoolToYesNo(write_set.skipped);
    if (!write_set.decision_source.empty())
    {
        line += ", decisionSource=" + write_set.decision_source;
    }
    if (!write_set.apply_target.empty())
    {
        line += ", applyTarget=" + write_set.apply_target;
    }
    if (write_set.prepared)
    {
        line += ", currentMap="
            + (write_set.current_map.empty() ? std::string("<none>") : write_set.current_map)
            + ", requestedMap="
            + (write_set.requested_map.empty()
                ? std::string("<none>")
                : write_set.requested_map)
            + ", targetBspPath="
            + (write_set.target_bsp_path.empty()
                ? std::string("<none>")
                : write_set.target_bsp_path)
            + ", landmark="
            + (write_set.landmark.empty() ? std::string("<none>") : write_set.landmark)
            + ", targetPlayerOrigin="
            + (write_set.target_player_origin.empty()
                ? std::string("<none>")
                : write_set.target_player_origin)
            + ", targetPlayerYaw="
            + (write_set.target_player_yaw.empty()
                ? std::string("<none>")
                : write_set.target_player_yaw)
            + ", writeOrigin=" + BoolToYesNo(write_set.write_origin)
            + ", writeYaw=" + BoolToYesNo(write_set.write_yaw)
            + ", writeInventory=" + BoolToYesNo(write_set.write_inventory)
            + ", writeVelocity=" + BoolToYesNo(write_set.write_velocity)
            + ", writeCount=" + std::to_string(write_set.write_count)
            + ", runtimeWriteSuppressed=" + BoolToYesNo(write_set.runtime_write_suppressed)
            + ", targetWorldspawnPresent=" + BoolToYesNo(write_set.target_worldspawn_present)
            + ", targetEntityParse=" + (write_set.target_entity_parse_ok ? "ok" : "fail");
    }
    if (!write_set.short_circuit_reason.empty())
    {
        line += ", shortCircuitReason=" + write_set.short_circuit_reason;
    }

    line += ", writeSetReady=" + std::string(BoolToYesNo(write_set.write_set_ready))
        + ", action="
        + (write_set.action.empty() ? std::string("<none>") : write_set.action);
    return line;
}

std::string FormatChangeLevelPlayerTransferDeferredApplyGateSummary(
    const hl::game_api::ChangeLevelTransitionSummary& summary)
{
    const hl::game_api::ChangeLevelTransitionSummary::
        ChangeLevelPlayerTransferDeferredApplyGateSummary& gate =
            summary.changelevel_player_transfer_deferred_apply_gate;
    std::string line =
        std::string("prepared=") + BoolToYesNo(gate.prepared)
        + ", skipped=" + BoolToYesNo(gate.skipped);
    if (!gate.decision_source.empty())
    {
        line += ", decisionSource=" + gate.decision_source;
    }
    if (!gate.apply_target.empty())
    {
        line += ", applyTarget=" + gate.apply_target;
    }
    if (gate.prepared)
    {
        line += ", currentMap="
            + (gate.current_map.empty() ? std::string("<none>") : gate.current_map)
            + ", requestedMap="
            + (gate.requested_map.empty() ? std::string("<none>") : gate.requested_map)
            + ", futureApplyPhase="
            + (gate.future_apply_phase.empty()
                ? std::string("<none>")
                : gate.future_apply_phase)
            + ", pendingWriteCount=" + std::to_string(gate.pending_write_count)
            + ", gateOpen=" + BoolToYesNo(gate.gate_open)
            + ", deferred=" + BoolToYesNo(gate.deferred)
            + ", gateReason="
            + (gate.gate_reason.empty() ? std::string("<none>") : gate.gate_reason)
            + ", runtimeWriteSuppressed="
            + BoolToYesNo(gate.runtime_write_suppressed);
    }
    if (!gate.short_circuit_reason.empty())
    {
        line += ", shortCircuitReason=" + gate.short_circuit_reason;
    }

    line += ", applyGateReady=" + std::string(BoolToYesNo(gate.apply_gate_ready))
        + ", action=" + (gate.action.empty() ? std::string("<none>") : gate.action);
    return line;
}

std::string FormatChangeLevelPlayerTransferGateOpenCheckpointSummary(
    const hl::game_api::ChangeLevelTransitionSummary& summary)
{
    const hl::game_api::ChangeLevelTransitionSummary::
        ChangeLevelPlayerTransferGateOpenCheckpointSummary& checkpoint =
            summary.changelevel_player_transfer_gate_open_checkpoint;
    std::string line =
        std::string("prepared=") + BoolToYesNo(checkpoint.prepared)
        + ", skipped=" + BoolToYesNo(checkpoint.skipped);
    if (!checkpoint.decision_source.empty())
    {
        line += ", decisionSource=" + checkpoint.decision_source;
    }
    if (!checkpoint.apply_target.empty())
    {
        line += ", applyTarget=" + checkpoint.apply_target;
    }
    if (checkpoint.prepared)
    {
        line += ", currentMap="
            + (checkpoint.current_map.empty() ? std::string("<none>") : checkpoint.current_map)
            + ", requestedMap="
            + (checkpoint.requested_map.empty()
                ? std::string("<none>")
                : checkpoint.requested_map)
            + ", futureApplyPhase="
            + (checkpoint.future_apply_phase.empty()
                ? std::string("<none>")
                : checkpoint.future_apply_phase)
            + ", targetRuntimeCheckpoint="
            + (checkpoint.target_runtime_checkpoint.empty()
                ? std::string("<none>")
                : checkpoint.target_runtime_checkpoint)
            + ", pendingWriteCount=" + std::to_string(checkpoint.pending_write_count)
            + ", checkpointSatisfied=" + BoolToYesNo(checkpoint.checkpoint_satisfied)
            + ", gateEligibleAtCheckpoint="
            + BoolToYesNo(checkpoint.gate_eligible_at_checkpoint)
            + ", gateOpen=" + BoolToYesNo(checkpoint.gate_open)
            + ", deferred=" + BoolToYesNo(checkpoint.deferred)
            + ", runtimeWriteSuppressed="
            + BoolToYesNo(checkpoint.runtime_write_suppressed);
    }
    if (!checkpoint.short_circuit_reason.empty())
    {
        line += ", shortCircuitReason=" + checkpoint.short_circuit_reason;
    }

    line += ", checkpointReady=" + std::string(BoolToYesNo(checkpoint.checkpoint_ready))
        + ", action="
        + (checkpoint.action.empty() ? std::string("<none>") : checkpoint.action);
    return line;
}

std::string FormatChangeLevelPlayerTransferCheckpointSignalContractSummary(
    const hl::game_api::ChangeLevelTransitionSummary& summary)
{
    const hl::game_api::ChangeLevelTransitionSummary::
        ChangeLevelPlayerTransferCheckpointSignalContractSummary& contract =
            summary.changelevel_player_transfer_checkpoint_signal_contract;
    std::string line =
        std::string("prepared=") + BoolToYesNo(contract.prepared)
        + ", skipped=" + BoolToYesNo(contract.skipped);
    if (!contract.decision_source.empty())
    {
        line += ", decisionSource=" + contract.decision_source;
    }
    if (!contract.apply_target.empty())
    {
        line += ", applyTarget=" + contract.apply_target;
    }
    if (contract.prepared)
    {
        line += ", currentMap="
            + (contract.current_map.empty() ? std::string("<none>") : contract.current_map)
            + ", requestedMap="
            + (contract.requested_map.empty()
                ? std::string("<none>")
                : contract.requested_map)
            + ", futureApplyPhase="
            + (contract.future_apply_phase.empty()
                ? std::string("<none>")
                : contract.future_apply_phase)
            + ", targetRuntimeCheckpoint="
            + (contract.target_runtime_checkpoint.empty()
                ? std::string("<none>")
                : contract.target_runtime_checkpoint)
            + ", requiredSignal="
            + (contract.required_signal.empty()
                ? std::string("<none>")
                : contract.required_signal)
            + ", pendingWriteCount=" + std::to_string(contract.pending_write_count)
            + ", signalObserved=" + BoolToYesNo(contract.signal_observed)
            + ", checkpointSatisfied=" + BoolToYesNo(contract.checkpoint_satisfied)
            + ", gateEligibleAtCheckpoint="
            + BoolToYesNo(contract.gate_eligible_at_checkpoint)
            + ", gateOpen=" + BoolToYesNo(contract.gate_open)
            + ", deferred=" + BoolToYesNo(contract.deferred)
            + ", runtimeWriteSuppressed="
            + BoolToYesNo(contract.runtime_write_suppressed);
    }
    if (!contract.short_circuit_reason.empty())
    {
        line += ", shortCircuitReason=" + contract.short_circuit_reason;
    }

    line += ", signalContractReady=" + std::string(BoolToYesNo(contract.signal_contract_ready))
        + ", action=" + (contract.action.empty() ? std::string("<none>") : contract.action);
    return line;
}

bool StartsWithText(std::string_view text, std::string_view prefix)
{
    return text.size() >= prefix.size() && text.substr(0, prefix.size()) == prefix;
}

std::string ExtractTokenValue(std::string_view text, std::string_view key)
{
    const std::size_t key_index = text.find(key);
    if (key_index == std::string_view::npos)
    {
        return {};
    }

    std::size_t value_begin = key_index + key.size();
    std::size_t value_end = value_begin;
    while (value_end < text.size())
    {
        const unsigned char ch = static_cast<unsigned char>(text[value_end]);
        if (std::isspace(ch) != 0 || ch == ',' || ch == ';' || ch == ']')
        {
            break;
        }
        ++value_end;
    }

    return std::string(text.substr(value_begin, value_end - value_begin));
}

bool TryExtractFloatTokenValue(
    std::string_view text,
    std::string_view key,
    float* value,
    std::string* formatted_value)
{
    const std::string token = ExtractTokenValue(text, key);
    if (token.empty())
    {
        return false;
    }

    float parsed = 0.0f;
    if (!ParseStrictFloat(token, &parsed))
    {
        return false;
    }

    if (value != nullptr)
    {
        *value = parsed;
    }
    if (formatted_value != nullptr)
    {
        *formatted_value = token;
    }
    return true;
}

bool TryExtractVectorTokenValue(
    std::string_view text,
    std::string_view key,
    Vector* value,
    std::string* formatted_value)
{
    const std::size_t key_index = text.find(key);
    if (key_index == std::string_view::npos)
    {
        return false;
    }

    std::istringstream stream{std::string(text.substr(key_index + key.size()))};
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    stream >> x >> y >> z;
    if (stream.fail())
    {
        return false;
    }

    const Vector parsed(x, y, z);
    if (value != nullptr)
    {
        *value = parsed;
    }
    if (formatted_value != nullptr)
    {
        *formatted_value = FormatVector(parsed);
    }
    return true;
}

bool TryResolveRequestTimeCarriedOrigin(
    const hl::game_api::ChangeLevelTransitionSummary& summary,
    Vector* origin,
    std::string* origin_text)
{
    if (TryExtractVectorTokenValue(
            summary.pending_request_detail,
            "surrogateOrigin=",
            origin,
            origin_text))
    {
        return true;
    }

    if (summary.closest_surrogate_origin_text.empty())
    {
        return false;
    }

    Vector parsed_origin(0.0f, 0.0f, 0.0f);
    if (!ParseStrictVector3(summary.closest_surrogate_origin_text, &parsed_origin))
    {
        return false;
    }

    if (origin != nullptr)
    {
        *origin = parsed_origin;
    }
    if (origin_text != nullptr)
    {
        *origin_text = summary.closest_surrogate_origin_text;
    }
    return true;
}

bool TryResolveRequestTimeCarriedYaw(
    const hl::game_api::ChangeLevelTransitionSummary& summary,
    float* yaw,
    std::string* yaw_text)
{
    return TryExtractFloatTokenValue(summary.pending_request_detail, "yaw=", yaw, yaw_text);
}

std::string FirstWord(std::string_view text)
{
    const std::size_t separator = text.find(' ');
    return std::string(text.substr(0, separator));
}

void SetFirstPostHandoffActivity(
    hl::game_api::ChangeLevelTransitionSummary::PostHandoffActivitySummary& activity,
    int frame_number,
    std::string action,
    std::string entity)
{
    if (activity.first_frame >= 0 || action.empty())
    {
        return;
    }

    activity.first_action = std::move(action);
    activity.first_entity = entity.empty() ? std::string("<none>") : std::move(entity);
    activity.first_frame = frame_number;
}

void PushPostHandoffSample(
    hl::game_api::ChangeLevelTransitionSummary::PostHandoffActivitySummary& activity,
    int frame_number,
    std::string sample)
{
    if (sample.empty() || activity.sample_effects.size() >= 3)
    {
        return;
    }

    sample = "frame=" + std::to_string(frame_number) + " " + sample;
    if (std::find(activity.sample_effects.begin(), activity.sample_effects.end(), sample)
        != activity.sample_effects.end())
    {
        return;
    }

    activity.sample_effects.push_back(std::move(sample));
}

std::vector<std::string> BuildRollingTraceDelta(
    const std::vector<std::string>& previous,
    const std::vector<std::string>& current)
{
    if (current.empty())
    {
        return {};
    }
    if (previous.empty())
    {
        return current;
    }

    const std::size_t max_overlap = std::min(previous.size(), current.size());
    std::size_t overlap = 0;
    for (std::size_t candidate = max_overlap; candidate > 0; --candidate)
    {
        bool matches = true;
        for (std::size_t index = 0; index < candidate; ++index)
        {
            if (previous[previous.size() - candidate + index] != current[index])
            {
                matches = false;
                break;
            }
        }
        if (matches)
        {
            overlap = candidate;
            break;
        }
    }

    return std::vector<std::string>(current.begin() + overlap, current.end());
}

void ObservePostHandoffDispatcherPreview(
    hl::game_api::ChangeLevelTransitionSummary::PostHandoffActivitySummary& activity,
    int frame_number,
    std::string_view preview)
{
    if (preview.empty() || StartsWithText(preview, "pending "))
    {
        return;
    }

    PushPostHandoffSample(activity, frame_number, std::string(preview));
    SetFirstPostHandoffActivity(
        activity,
        frame_number,
        FirstWord(preview),
        [&]()
        {
            std::string entity = ExtractTokenValue(preview, "classname=");
            if (entity.empty())
            {
                entity = ExtractTokenValue(preview, "source=");
            }
            if (entity.empty())
            {
                entity = ExtractTokenValue(preview, "target=");
            }
            return entity;
        }());
}

void ObservePostHandoffDispatcherTrace(
    hl::game_api::ChangeLevelTransitionSummary::PostHandoffActivitySummary& activity,
    int frame_number,
    std::string_view line)
{
    constexpr std::string_view kDispatcherPrefix = "MapLogicDispatcher: ";
    if (!StartsWithText(line, kDispatcherPrefix))
    {
        return;
    }

    const std::string_view detail = line.substr(kDispatcherPrefix.size());
    if (StartsWithText(detail, "pfnUse succeeded for edict#")
        || StartsWithText(detail, "custom dispatch handled edict#"))
    {
        ++activity.dispatch_successes;
    }

    const bool is_sample =
        StartsWithText(detail, "queued ")
        || StartsWithText(detail, "executed ")
        || StartsWithText(detail, "rescheduled ")
        || StartsWithText(detail, "deferred ")
        || StartsWithText(detail, "failed ")
        || StartsWithText(detail, "pfnUse succeeded for edict#")
        || StartsWithText(detail, "custom dispatch handled edict#");
    if (!is_sample)
    {
        return;
    }

    PushPostHandoffSample(activity, frame_number, std::string(detail));
    if (StartsWithText(detail, "pfnUse succeeded for edict#"))
    {
        SetFirstPostHandoffActivity(
            activity,
            frame_number,
            "pfnUse-succeeded",
            "edict#" + ExtractTokenValue(detail, "pfnUse succeeded for edict#"));
        return;
    }
    if (StartsWithText(detail, "custom dispatch handled edict#"))
    {
        SetFirstPostHandoffActivity(
            activity,
            frame_number,
            "custom-dispatch-handled",
            "edict#" + ExtractTokenValue(detail, "custom dispatch handled edict#"));
        return;
    }

    SetFirstPostHandoffActivity(
        activity,
        frame_number,
        FirstWord(detail),
        [&]()
        {
            std::string entity = ExtractTokenValue(detail, "classname=");
            if (entity.empty())
            {
                entity = ExtractTokenValue(detail, "source=");
            }
            if (entity.empty())
            {
                entity = ExtractTokenValue(detail, "target=");
            }
            return entity;
        }());
}

void ObservePostHandoffMessages(
    hl::game_api::ChangeLevelTransitionSummary::PostHandoffActivitySummary& activity,
    const hl::game_api::ServerFrameStateSummary& frame)
{
    activity.messages += static_cast<int>(frame.message_preview.size());
    for (const std::string& message : frame.message_preview)
    {
        PushPostHandoffSample(activity, frame.frame_number, "message " + message);
        SetFirstPostHandoffActivity(
            activity,
            frame.frame_number,
            "message",
            [&]()
            {
                std::string entity = ExtractTokenValue(message, "classname=");
                if (entity.empty() || entity == "<empty>")
                {
                    entity = "type=" + ExtractTokenValue(message, "type=");
                }
                return entity;
            }());
        if (activity.sample_effects.size() >= 3)
        {
            break;
        }
    }
}

hl::game_api::ChangeLevelTransitionSummary::PostHandoffActivitySummary BuildPostHandoffActivitySummary(
    const hl::game_api::ChangeLevelTransitionSummary& transition,
    const hl::game_api::MapLogicDispatcherStateSummary& map_logic,
    const hl::game_api::ServerFrameLoopStateSummary& frame_loop)
{
    hl::game_api::ChangeLevelTransitionSummary::PostHandoffActivitySummary activity;
    const hl::game_api::ChangeLevelTransitionSummary::PreChangeLevelHandoffSummary& handoff =
        transition.pre_changelevel_handoff;
    if (!handoff.active || !handoff.handoff_latched || handoff.stop_requested || handoff.request_frame < 0)
    {
        return activity;
    }

    activity.measured = true;
    activity.handoff_frame = handoff.request_frame;
    activity.handoff_time = handoff.request_time;

    std::vector<std::string> previous_trace_tail;
    for (const hl::game_api::MapLogicFrameStateSummary& frame : map_logic.frames)
    {
        if (frame.frame_number <= handoff.request_frame)
        {
            previous_trace_tail = frame.trace_tail;
            continue;
        }

        activity.scheduled_executed += frame.scheduled_executed;
        activity.dispatch_attempts += frame.target_chains_fired;
        for (const std::string& preview : frame.scheduled_action_preview)
        {
            ObservePostHandoffDispatcherPreview(activity, frame.frame_number, preview);
        }
        for (const std::string& trace_line : BuildRollingTraceDelta(previous_trace_tail, frame.trace_tail))
        {
            ObservePostHandoffDispatcherTrace(activity, frame.frame_number, trace_line);
        }

        previous_trace_tail = frame.trace_tail;
    }

    for (const hl::game_api::ServerFrameStateSummary& frame : frame_loop.frames)
    {
        if (frame.frame_number <= handoff.request_frame)
        {
            continue;
        }

        ++activity.observed_frames;
        if (!frame.message_preview.empty())
        {
            ObservePostHandoffMessages(activity, frame);
        }
    }

    return activity;
}

std::string FormatPostHandoffActivitySummary(
    const hl::game_api::ChangeLevelTransitionSummary& summary)
{
    const hl::game_api::ChangeLevelTransitionSummary::PostHandoffActivitySummary& activity =
        summary.post_handoff_activity;
    return "handoffFrame="
        + (activity.handoff_frame >= 0
            ? std::to_string(activity.handoff_frame)
            : std::string("<none>"))
        + ", handoffTime="
        + (activity.measured ? std::to_string(activity.handoff_time) : std::string("<none>"))
        + ", postHandoffFrames=" + std::to_string(activity.observed_frames)
        + ", postHandoffScheduledExecuted=" + std::to_string(activity.scheduled_executed)
        + ", postHandoffDispatchAttempts=" + std::to_string(activity.dispatch_attempts)
        + ", postHandoffDispatchSuccesses=" + std::to_string(activity.dispatch_successes)
        + ", postHandoffMessages=" + std::to_string(activity.messages);
}

std::string FormatPostHandoffFirstSummary(
    const hl::game_api::ChangeLevelTransitionSummary& summary)
{
    const hl::game_api::ChangeLevelTransitionSummary::PostHandoffActivitySummary& activity =
        summary.post_handoff_activity;
    return "firstPostHandoffAction="
        + (activity.first_action.empty() ? std::string("<none>") : activity.first_action)
        + ", firstPostHandoffEntity="
        + (activity.first_entity.empty() ? std::string("<none>") : activity.first_entity)
        + ", firstPostHandoffFrame="
        + (activity.first_frame >= 0
            ? std::to_string(activity.first_frame)
            : std::string("<none>"));
}

std::string TrimTrailingWhitespace(std::string value)
{
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())) != 0)
    {
        value.pop_back();
    }

    return value;
}

std::string TrimWhitespaceCopy(std::string_view value)
{
    std::size_t begin = 0;
    while (begin < value.size() && std::isspace(static_cast<unsigned char>(value[begin])) != 0)
    {
        ++begin;
    }

    std::size_t end = value.size();
    while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0)
    {
        --end;
    }

    return std::string(value.substr(begin, end - begin));
}

std::string NormalizeEngineResourcePath(std::string_view value)
{
    std::string normalized = TrimWhitespaceCopy(value);
    std::replace(normalized.begin(), normalized.end(), '\\', '/');
    std::transform(
        normalized.begin(),
        normalized.end(),
        normalized.begin(),
        [](unsigned char character)
        {
            return static_cast<char>(std::tolower(character));
        });
    return normalized;
}

std::string FormatPointer(const void* pointer)
{
    std::ostringstream stream;
    stream << "0x" << std::hex << std::uppercase
           << reinterpret_cast<std::uintptr_t>(pointer);
    return stream.str();
}

char* MutableEmptyString()
{
    static char empty[] = "";
    return empty;
}

const char* EmptyString()
{
    return "";
}

std::array<unsigned char, 1>& EmptyVisibilitySet()
{
    static std::array<unsigned char, 1> visibility = {0};
    return visibility;
}

int HandleSehException(const char* scope, unsigned int code)
{
    hl::common::Logger::Error(
        std::string("hl.dll ") + scope + " raised SEH exception 0x"
        + [&]()
        {
            std::ostringstream stream;
            stream << std::hex << std::uppercase << code;
            return stream.str();
        }());
    return EXCEPTION_EXECUTE_HANDLER;
}

EngineShimState& CurrentShimState()
{
    static EngineShimState fallback_state;
    static bool fallback_initialized = false;

    if (g_active_shim_state != nullptr)
    {
        return *g_active_shim_state;
    }

    if (!fallback_initialized)
    {
        fallback_state.string_pool.Reset();
        fallback_state.globalvars.pStringBase = fallback_state.string_pool.Base();
        fallback_state.globalvars.trace_fraction = 1.0f;
        fallback_initialized = true;
    }

    hl::common::Logger::Error(
        "hl.dll engine callback arrived without an active shim state; using fallback storage.");
    return fallback_state;
}

struct ScopedActivationTraceContext
{
    explicit ScopedActivationTraceContext(
        hl::game_api::detail::ServerActivationDiagnostics& diagnostics,
        int edict_index,
        std::string_view classname)
        : diagnostics_(diagnostics)
        , had_context_(diagnostics.HasCurrentEntityContext())
        , previous_edict_index_(diagnostics.CurrentEdictIndex())
        , previous_classname_(diagnostics.CurrentClassname())
    {
        diagnostics_.SetCurrentEntityContext(edict_index, classname);
    }

    ~ScopedActivationTraceContext()
    {
        if (had_context_)
        {
            diagnostics_.SetCurrentEntityContext(previous_edict_index_, previous_classname_);
        }
        else
        {
            diagnostics_.ClearCurrentEntityContext();
        }
    }

private:
    hl::game_api::detail::ServerActivationDiagnostics& diagnostics_;
    bool had_context_ = false;
    int previous_edict_index_ = -1;
    std::string previous_classname_;
};

void ResolveActivationTraceContext(
    const EngineShimState& state,
    const edict_t* entity,
    std::string_view classname_hint,
    int* edict_index,
    std::string* classname)
{
    if (edict_index != nullptr)
    {
        *edict_index = -1;
    }

    if (classname != nullptr)
    {
        classname->clear();
    }

    if (entity != nullptr)
    {
        const int index = state.edict_store.IndexOf(entity);
        if (edict_index != nullptr)
        {
            *edict_index = index;
        }

        if (classname != nullptr && index >= 0)
        {
            const hl::game_api::detail::EntityStateSnapshot snapshot =
                state.edict_store.SnapshotOf(entity, state.string_pool);
            if (!snapshot.classname.empty())
            {
                *classname = snapshot.classname;
            }
        }
    }

    if (classname != nullptr && classname->empty() && !classname_hint.empty())
    {
        *classname = std::string(classname_hint);
    }
}

void RecordActivationInternalEvent(
    EngineShimState& state,
    std::string_view name,
    std::string_view detail,
    const edict_t* entity = nullptr,
    std::string_view classname_hint = {})
{
    if (!state.server_activation_diagnostics.IsActive())
    {
        return;
    }

    int edict_index = -1;
    std::string classname;
    ResolveActivationTraceContext(state, entity, classname_hint, &edict_index, &classname);
    state.server_activation_diagnostics.RecordInternalEvent(name, detail, edict_index, classname);
    if (const hl::game_api::detail::SpawnTraceEvent* event =
            state.server_activation_diagnostics.LastEvent();
        event != nullptr)
    {
        hl::common::Logger::Info("ServerActivate trace: " + FormatSpawnTraceEvent(*event));
    }
}

void RecordCallback(
    std::string_view name,
    std::string_view detail = {},
    const edict_t* entity = nullptr,
    std::string_view classname_hint = {})
{
    EngineShimState& state = CurrentShimState();
    const std::string callback_name(name);
    auto [it, inserted] = state.callback_counts.try_emplace(callback_name, 0);
    if (inserted)
    {
        state.callback_order.push_back(callback_name);
    }

    ++it->second;

    std::string message = "hl.dll engine callback: " + callback_name;
    const std::string detail_text(detail);
    if (!detail.empty())
    {
        message += " [" + detail_text + "]";
    }

    const auto is_runtime_message_callback =
        [&](std::string_view candidate)
        {
            return EqualsIgnoreCase(candidate, "pfnAlertMessage")
                || EqualsIgnoreCase(candidate, "pfnMessageBegin")
                || EqualsIgnoreCase(candidate, "pfnMessageEnd")
                || EqualsIgnoreCase(candidate, "pfnWriteByte")
                || EqualsIgnoreCase(candidate, "pfnWriteChar")
                || EqualsIgnoreCase(candidate, "pfnWriteShort")
                || EqualsIgnoreCase(candidate, "pfnWriteLong")
                || EqualsIgnoreCase(candidate, "pfnWriteAngle")
                || EqualsIgnoreCase(candidate, "pfnWriteCoord")
                || EqualsIgnoreCase(candidate, "pfnWriteString")
                || EqualsIgnoreCase(candidate, "pfnWriteEntity")
                || EqualsIgnoreCase(candidate, "pfnServerPrint");
        };
    const bool runtime_frame_trace_active =
        state.server_frame_diagnostics.IsActive()
        || state.entity_think_diagnostics.IsActive();
    const bool emit_runtime_callback_text =
        state.frame_bootstrap_options.trace_callbacks
        || !runtime_frame_trace_active
        || is_runtime_message_callback(callback_name);

    if (state.worldspawn_spawn_diagnostics.IsActive())
    {
        state.worldspawn_spawn_diagnostics.RecordCallback(callback_name, detail_text);
        if (const hl::game_api::detail::SpawnTraceEvent* event =
                state.worldspawn_spawn_diagnostics.LastEvent();
            event != nullptr)
        {
            hl::common::Logger::Info("Worldspawn trace: " + FormatSpawnTraceEvent(*event));
        }
    }

    if (state.server_activation_diagnostics.IsActive())
    {
        int edict_index = -1;
        std::string classname;
        ResolveActivationTraceContext(state, entity, classname_hint, &edict_index, &classname);
        if (edict_index >= 0 || !classname.empty())
        {
            const ScopedActivationTraceContext activation_context(
                state.server_activation_diagnostics,
                edict_index,
                classname);
            state.server_activation_diagnostics.RecordCallback(
                callback_name,
                detail_text,
                edict_index,
                classname);
        }
        else
        {
            state.server_activation_diagnostics.RecordCallback(callback_name, detail_text);
        }

        if (const hl::game_api::detail::SpawnTraceEvent* event =
                state.server_activation_diagnostics.LastEvent();
            event != nullptr)
        {
            hl::common::Logger::Info("ServerActivate trace: " + FormatSpawnTraceEvent(*event));
        }
    }

    if (state.server_frame_diagnostics.IsActive())
    {
        int edict_index = -1;
        std::string classname;
        ResolveActivationTraceContext(state, entity, classname_hint, &edict_index, &classname);
        if (edict_index >= 0 || !classname.empty())
        {
            state.server_frame_diagnostics.RecordCallback(
                callback_name,
                detail_text,
                edict_index,
                classname);
        }
        else
        {
            state.server_frame_diagnostics.RecordCallback(callback_name, detail_text);
        }

        if (const hl::game_api::detail::SpawnTraceEvent* event =
                state.server_frame_diagnostics.LastEvent();
            event != nullptr && emit_runtime_callback_text)
        {
            hl::common::Logger::Info("StartFrame trace: " + FormatSpawnTraceEvent(*event));
        }
    }

    if (state.entity_think_diagnostics.IsActive())
    {
        int edict_index = -1;
        std::string classname;
        ResolveActivationTraceContext(state, entity, classname_hint, &edict_index, &classname);
        if (edict_index >= 0 || !classname.empty())
        {
            state.entity_think_diagnostics.RecordCallback(
                callback_name,
                detail_text,
                edict_index,
                classname);
        }
        else
        {
            state.entity_think_diagnostics.RecordCallback(callback_name, detail_text);
        }

        if (const hl::game_api::detail::SpawnTraceEvent* event =
                state.entity_think_diagnostics.LastEvent();
            event != nullptr && emit_runtime_callback_text)
        {
            hl::common::Logger::Info("Think trace: " + FormatSpawnTraceEvent(*event));
        }
    }

    if (emit_runtime_callback_text)
    {
        hl::common::Logger::Info(message);
    }
}

constexpr std::size_t kServerCommandExecuteSafetyLimit = 128;

std::string FormatExceptionCode(unsigned int code)
{
    std::ostringstream stream;
    stream << "0x" << std::hex << std::uppercase << code;
    return stream.str();
}

std::string FormatSpawnTraceEvent(const hl::game_api::detail::SpawnTraceEvent& event)
{
    std::string line =
        "#" + std::to_string(event.sequence)
        + " map=" + (event.map_name.empty() ? std::string("<unset>") : event.map_name)
        + " edict#" + std::to_string(event.edict_index)
        + " classname="
        + (event.classname.empty() ? std::string("<empty>") : event.classname)
        + " " + event.callback_name;
    if (!event.detail.empty())
    {
        line += " [" + event.detail + "]";
    }

    return line;
}

void LogCommandQueueSnapshot(
    const hl::game_api::ServerCommandBufferSnapshot& snapshot,
    std::string_view header)
{
    hl::common::Logger::Info(std::string(header));
    hl::common::Logger::Info(
        "  - queued total: " + std::to_string(snapshot.queued_total));
    hl::common::Logger::Info(
        "  - executed total: " + std::to_string(snapshot.executed_total));
    hl::common::Logger::Info(
        "  - pending total: " + std::to_string(snapshot.pending_total));

    if (snapshot.pending_commands.empty())
    {
        hl::common::Logger::Info("  - pending commands: <none>");
        return;
    }

    hl::common::Logger::Info("  - pending commands:");
    for (const std::string& command : snapshot.pending_commands)
    {
        hl::common::Logger::Info("    * " + command);
    }
}

void LogServerStateSnapshot(const EngineShimState& state, std::string_view header)
{
    hl::common::Logger::Info(std::string(header));
    hl::common::Logger::Info(
        "  - map: " + (state.server_state.map_name.empty()
            ? std::string("<unset>")
            : state.server_state.map_name));
    hl::common::Logger::Info(
        "  - active/loading/initialized: "
        + std::string(BoolToYesNo(state.server_state.active)) + "/"
        + BoolToYesNo(state.server_state.loading) + "/"
        + BoolToYesNo(state.server_state.initialized));
    hl::common::Logger::Info(
        "  - time/frame/maxClients: " + std::to_string(state.globalvars.time)
        + "/" + std::to_string(state.globalvars.frametime)
        + "/" + std::to_string(state.globalvars.maxClients));
    hl::common::Logger::Info(
        "  - globals deathmatch/coop: " + std::to_string(state.globalvars.deathmatch)
        + "/" + std::to_string(state.globalvars.coop));
    hl::common::Logger::Info(
        "  - world model: "
        + (state.world_context.model_path.empty()
            ? std::string("<unset>")
            : state.world_context.model_path)
        + " (index=" + std::to_string(state.world_context.world_model_index) + ")");
}

void LogGlobalsSnapshot(const EngineShimState& state, std::string_view header)
{
    hl::common::Logger::Info(std::string(header));
    hl::common::Logger::Info(
        "  - mapname/startspot: "
        + (state.string_pool.Describe(state.globalvars.mapname).empty()
            ? std::string("<empty>")
            : state.string_pool.Describe(state.globalvars.mapname))
        + "/"
        + (state.string_pool.Describe(state.globalvars.startspot).empty()
            ? std::string("<empty>")
            : state.string_pool.Describe(state.globalvars.startspot)));
    hl::common::Logger::Info(
        "  - time/frametime: " + std::to_string(state.globalvars.time)
        + "/" + std::to_string(state.globalvars.frametime));
    hl::common::Logger::Info(
        "  - deathmatch/coop: " + std::to_string(state.globalvars.deathmatch)
        + "/" + std::to_string(state.globalvars.coop));
    hl::common::Logger::Info(
        "  - maxClients/maxEntities: " + std::to_string(state.globalvars.maxClients)
        + "/" + std::to_string(state.globalvars.maxEntities));
}

void LogPrecacheRegistrySnapshot(const EngineShimState& state, std::string_view header)
{
    hl::common::Logger::Info(std::string(header));
    hl::common::Logger::Info(
        "  - model registry size: " + std::to_string(state.precache_registry.ModelCount()));
    hl::common::Logger::Info(
        "  - sound registry size: " + std::to_string(state.sound_precache_registry.RegistrySize()));
    hl::common::Logger::Info(
        "  - generic precache size: " + std::to_string(state.generic_precache_registry.Size()));
    hl::common::Logger::Info(
        "  - event precache size: " + std::to_string(state.event_precache_registry.Size()));
    hl::common::Logger::Info(
        "  - decal registry size: " + std::to_string(state.decal_registry.Size()));
    hl::common::Logger::Info(
        "  - user message registry size: " + std::to_string(state.user_message_registry.Size()));
    hl::common::Logger::Info(
        "  - light styles: " + std::to_string(state.light_styles.size()));
}

void LogEdictTableSummary(
    const EngineShimState& state,
    std::string_view header,
    std::size_t max_lines = 32)
{
    hl::common::Logger::Info(std::string(header));
    hl::common::Logger::Info(
        "  - max/allocated/active-range: "
        + std::to_string(state.edict_store.MaxEntities()) + "/"
        + std::to_string(state.edict_store.AllocatedCount()) + "/"
        + std::to_string(state.edict_store.NumberOfEntities()));

    std::size_t logged = 0;
    for (int index = 0; index < state.edict_store.NumberOfEntities(); ++index)
    {
        const edict_t* entity = state.edict_store.EntityOfIndex(index);
        const hl::game_api::detail::EntityStateSnapshot snapshot =
            state.edict_store.SnapshotOf(entity, state.string_pool);
        if (snapshot.index < 0)
        {
            continue;
        }

        const bool interesting =
            snapshot.in_use
            || snapshot.removed
            || snapshot.deferred
            || snapshot.private_data_present
            || snapshot.activation_candidate;
        if (!interesting)
        {
            continue;
        }

        hl::common::Logger::Info("  - " + BuildEntitySnapshotSummary(snapshot));
        ++logged;
        if (logged >= max_lines)
        {
            break;
        }
    }

    if (logged == 0)
    {
        hl::common::Logger::Info("  - <no interesting edicts>");
    }
}

std::vector<hl::game_api::detail::SpawnTraceEvent> TakeTraceTail(
    const std::vector<hl::game_api::detail::SpawnTraceEvent>& trace,
    std::size_t max_entries)
{
    if (trace.size() <= max_entries)
    {
        return trace;
    }

    return std::vector<hl::game_api::detail::SpawnTraceEvent>(
        trace.end() - static_cast<std::ptrdiff_t>(max_entries),
        trace.end());
}

void LogWorldspawnTraceTail(const EngineShimState& state)
{
    const std::vector<hl::game_api::detail::SpawnTraceEvent> trace =
        state.worldspawn_spawn_diagnostics.TraceSnapshot();
    if (trace.empty())
    {
        hl::common::Logger::Info("Worldspawn spawn trace tail: <empty>");
        return;
    }

    hl::common::Logger::Info("Worldspawn spawn trace tail:");
    for (const hl::game_api::detail::SpawnTraceEvent& event : trace)
    {
        hl::common::Logger::Info("  - " + FormatSpawnTraceEvent(event));
    }
}

void LogWorldspawnDistinctCallbackTail(const EngineShimState& state)
{
    const std::vector<hl::game_api::detail::SpawnTraceEvent> trace =
        state.worldspawn_spawn_diagnostics.DistinctCallbackSnapshot();
    if (trace.empty())
    {
        hl::common::Logger::Info("Worldspawn distinct callback tail: <empty>");
        return;
    }

    hl::common::Logger::Info("Worldspawn distinct callback tail:");
    for (const hl::game_api::detail::SpawnTraceEvent& event : trace)
    {
        hl::common::Logger::Info("  - " + FormatSpawnTraceEvent(event));
    }
}

void LogWorldspawnPrecacheTail(const EngineShimState& state)
{
    const std::vector<hl::game_api::detail::SpawnTraceEvent> trace =
        state.worldspawn_spawn_diagnostics.PrecacheSnapshot();
    if (trace.empty())
    {
        hl::common::Logger::Info("Worldspawn precache tail: <empty>");
        return;
    }

    hl::common::Logger::Info("Worldspawn precache tail:");
    for (const hl::game_api::detail::SpawnTraceEvent& event : trace)
    {
        hl::common::Logger::Info("  - " + FormatSpawnTraceEvent(event));
    }
}

void LogServerActivationTraceTail(const EngineShimState& state)
{
    const std::vector<hl::game_api::detail::SpawnTraceEvent> trace = TakeTraceTail(
        state.server_activation_diagnostics.TraceSnapshot(),
        64);
    if (trace.empty())
    {
        hl::common::Logger::Info("ServerActivate trace tail: <empty>");
        return;
    }

    hl::common::Logger::Info("ServerActivate trace tail:");
    for (const hl::game_api::detail::SpawnTraceEvent& event : trace)
    {
        hl::common::Logger::Info("  - " + FormatSpawnTraceEvent(event));
    }
}

void LogServerActivationDistinctCallbackTail(const EngineShimState& state)
{
    const std::vector<hl::game_api::detail::SpawnTraceEvent> trace = TakeTraceTail(
        state.server_activation_diagnostics.DistinctCallbackSnapshot(),
        32);
    if (trace.empty())
    {
        hl::common::Logger::Info("ServerActivate distinct callback tail: <empty>");
        return;
    }

    hl::common::Logger::Info("ServerActivate distinct callback tail:");
    for (const hl::game_api::detail::SpawnTraceEvent& event : trace)
    {
        hl::common::Logger::Info("  - " + FormatSpawnTraceEvent(event));
    }
}

void LogServerFrameTraceTail(const EngineShimState& state)
{
    const std::vector<hl::game_api::detail::SpawnTraceEvent> trace = TakeTraceTail(
        state.server_frame_diagnostics.TraceSnapshot(),
        32);
    if (trace.empty())
    {
        hl::common::Logger::Info("StartFrame trace tail: <empty>");
        return;
    }

    hl::common::Logger::Info("StartFrame trace tail:");
    for (const hl::game_api::detail::SpawnTraceEvent& event : trace)
    {
        hl::common::Logger::Info("  - " + FormatSpawnTraceEvent(event));
    }
}

void LogEntityThinkTraceTail(const EngineShimState& state)
{
    const std::vector<hl::game_api::detail::SpawnTraceEvent> trace = TakeTraceTail(
        state.entity_think_diagnostics.TraceSnapshot(),
        32);
    if (trace.empty())
    {
        hl::common::Logger::Info("EntityThink trace tail: <empty>");
        return;
    }

    hl::common::Logger::Info("EntityThink trace tail:");
    for (const hl::game_api::detail::SpawnTraceEvent& event : trace)
    {
        hl::common::Logger::Info("  - " + FormatSpawnTraceEvent(event));
    }
}

void LogRandomDiagnostics(const EngineShimState& state, std::string_view header)
{
    hl::common::Logger::Info(std::string(header));
    hl::common::Logger::Info(
        "  - RandomLong calls: " + std::to_string(state.random_diagnostics.random_long_calls));
    hl::common::Logger::Info(
        "  - RandomFloat calls: " + std::to_string(state.random_diagnostics.random_float_calls));

    if (state.random_diagnostics.recorded_callsites.empty())
    {
        hl::common::Logger::Info("  - recorded callsites: <none>");
        return;
    }

    hl::common::Logger::Info("  - recorded callsites:");
    for (const RandomCallsiteRecord& record : state.random_diagnostics.recorded_callsites)
    {
        hl::common::Logger::Info(
            "    * " + record.callback_name
            + " return=" + FormatPointer(record.return_address)
            + " calls=" + std::to_string(record.call_count)
            + " context=" + (record.context.empty() ? std::string("<none>") : record.context));
    }
}

std::string BuildRandomDiagnosticContext(const EngineShimState& state)
{
    if (!state.worldspawn_spawn_diagnostics.IsActive())
    {
        return "outside worldspawn spawn";
    }

    const std::vector<hl::game_api::detail::SpawnTraceEvent> precache_snapshot =
        state.worldspawn_spawn_diagnostics.PrecacheSnapshot();
    if (!precache_snapshot.empty())
    {
        const hl::game_api::detail::SpawnTraceEvent& last_precache = precache_snapshot.back();
        return "after " + last_precache.callback_name
            + (last_precache.detail.empty() ? std::string() : " [" + last_precache.detail + "]");
    }

    if (const hl::game_api::detail::SpawnTraceEvent* last_distinct =
            state.worldspawn_spawn_diagnostics.LastDistinctEvent();
        last_distinct != nullptr)
    {
        return "after " + last_distinct->callback_name
            + (last_distinct->detail.empty() ? std::string() : " [" + last_distinct->detail + "]");
    }

    return "during worldspawn spawn";
}

void MergeDispatchStats(
    hl::game_api::ServerCommandDispatchStats& destination,
    const hl::game_api::ServerCommandDispatchStats& delta)
{
    destination.execute_cycles += delta.execute_cycles;
    destination.executed_commands += delta.executed_commands;
    destination.executed_cfg_files += delta.executed_cfg_files;
    destination.updated_cvars_from_cfg += delta.updated_cvars_from_cfg;
    destination.queued_during_execution += delta.queued_during_execution;
    destination.remaining_pending_commands = delta.remaining_pending_commands;
    destination.hit_execute_limit = destination.hit_execute_limit || delta.hit_execute_limit;

    for (const std::string& name : delta.updated_cvar_names)
    {
        if (std::find(destination.updated_cvar_names.begin(), destination.updated_cvar_names.end(), name)
            == destination.updated_cvar_names.end())
        {
            destination.updated_cvar_names.push_back(name);
        }
    }

    for (const std::string& path : delta.executed_cfg_paths)
    {
        if (std::find(destination.executed_cfg_paths.begin(), destination.executed_cfg_paths.end(), path)
            == destination.executed_cfg_paths.end())
        {
            destination.executed_cfg_paths.push_back(path);
        }
    }
}

void ExecuteQueuedServerCommands(EngineShimState& state, std::string_view reason)
{
    const hl::game_api::ServerCommandBufferSnapshot before_snapshot = state.command_buffer.Snapshot();
    LogCommandQueueSnapshot(
        before_snapshot,
        "Server command queue before execute [" + std::string(reason) + "]:");

    if (state.command_execute_in_progress)
    {
        hl::common::Logger::Warn(
            "Server command execute re-entry detected; outer drain cycle will continue.");
        return;
    }

    state.command_execute_in_progress = true;
    hl::game_api::ServerCommandDispatcher dispatcher(
        state.file_system,
        state.game_directory,
        state.cvar_registry,
        state.command_buffer);
    dispatcher.DrainPending(kServerCommandExecuteSafetyLimit);
    state.command_execute_in_progress = false;

    const hl::game_api::ServerCommandDispatchStats cycle_stats = dispatcher.Stats();
    MergeDispatchStats(state.command_dispatch_stats, cycle_stats);
    state.command_buffer.MarkExecuted(cycle_stats.executed_commands);

    LogCommandQueueSnapshot(
        state.command_buffer.Snapshot(),
        "Server command queue after execute [" + std::string(reason) + "]:");
}

void SeedBuiltinCvars(
    hl::game_api::CvarRegistry& registry,
    const hl::game_api::detail::ServerState& server_state)
{
    registry.RegisterBuiltin("sv_gravity", "800");
    registry.RegisterBuiltin("sv_stepsize", "18");
    registry.RegisterBuiltin("sv_zmax", "4096");
    registry.RegisterBuiltin("sv_skyname", "");
    registry.RegisterBuiltin("sv_wateramp", "0");
    registry.RegisterBuiltin("sv_newunit", "0");
    registry.RegisterBuiltin("sv_language", "0");
    registry.RegisterBuiltin("sv_aim", "0.93");
    registry.RegisterBuiltin("sv_allow_autoaim", "1");
    registry.RegisterBuiltin("mp_footsteps", "1");
    registry.RegisterBuiltin("mp_defaultteam", "0");
    registry.RegisterBuiltin("room_type", "0");
    registry.RegisterBuiltin("skill", "1");
    registry.RegisterBuiltin("teamplay", "0");
    registry.RegisterBuiltin("v_dark", "0");
    registry.RegisterBuiltin("lservercfgfile", "");
    registry.RegisterBuiltin("servercfgfile", "");
    registry.RegisterBuiltin("mapcyclefile", "mapcycle.txt");
    registry.RegisterBuiltin("motdfile", "motd.txt");
    hl::game_api::detail::SeedServerCvars(registry, server_state);
}

void RefreshExecutionSummary(
    hl::game_api::HlServerModuleSummary& summary,
    const EngineShimState& state,
    const hl::game_api::ServerCommandDispatchStats* dispatch_stats = nullptr)
{
    const hl::game_api::ServerCommandDispatchStats& effective_stats =
        dispatch_stats != nullptr ? *dispatch_stats : state.command_dispatch_stats;
    summary.registered_cvars = state.cvar_registry.Count();
    summary.queued_server_commands = state.command_buffer.QueuedCount();
    summary.executed_server_commands = state.command_buffer.ExecutedCount();
    summary.executed_cfg_files = effective_stats.executed_cfg_files;
    summary.updated_cvars_from_cfg = effective_stats.updated_cvars_from_cfg;
    summary.auto_created_cvars = state.cvar_registry.AutoCreatedCount();
    summary.executed_cfg_paths = effective_stats.executed_cfg_paths;
}

std::vector<hl::game_api::CvarSnapshot> CollectSkillCvarExamples(
    const hl::game_api::CvarRegistry& registry,
    const hl::game_api::ServerCommandDispatchStats* dispatch_stats)
{
    std::vector<hl::game_api::CvarSnapshot> examples;
    std::vector<std::string> preferred_names;

    if (dispatch_stats != nullptr)
    {
        for (const std::string& name : dispatch_stats->updated_cvar_names)
        {
            if (name.rfind("sk_", 0) == 0)
            {
                preferred_names.push_back(name);
            }
        }
    }

    for (const std::string& name : preferred_names)
    {
        const std::optional<hl::game_api::CvarSnapshot> snapshot = registry.SnapshotOf(name);
        if (!snapshot.has_value())
        {
            continue;
        }

        examples.push_back(*snapshot);
        if (examples.size() >= 5)
        {
            return examples;
        }
    }

    const std::vector<hl::game_api::CvarSnapshot> fallback_examples =
        registry.SnapshotMatchingPrefix("sk_", 5 - examples.size());
    for (const hl::game_api::CvarSnapshot& snapshot : fallback_examples)
    {
        const auto duplicate = std::find_if(
            examples.begin(),
            examples.end(),
            [&](const hl::game_api::CvarSnapshot& existing)
            {
                return existing.name == snapshot.name;
            });
        if (duplicate == examples.end())
        {
            examples.push_back(snapshot);
        }

        if (examples.size() >= 5)
        {
            break;
        }
    }

    return examples;
}

std::string DescribeEdict(const EngineShimState& state, const edict_t* edict)
{
    const int index = state.edict_store.IndexOf(edict);
    return index >= 0 ? "edict#" + std::to_string(index) : "edict?<external>";
}

std::string DescribeEdictState(const EngineShimState& state, const edict_t* edict)
{
    return state.edict_store.DumpEntityState(edict, state.string_pool);
}

constexpr std::size_t kEntityBootstrapAllocationLimit = 192;
constexpr std::size_t kEntityDebugLogCount = 8;
constexpr std::size_t kClassnameSummaryCount = 8;
constexpr std::size_t kSampleSpawnedCount = 5;
constexpr std::size_t kActivationPreviewCount = 16;

std::string ToLowerCopy(std::string_view value)
{
    std::string text(value);
    std::transform(
        text.begin(),
        text.end(),
        text.begin(),
        [](unsigned char character)
        {
            return static_cast<char>(std::tolower(character));
        });
    return text;
}

bool EqualsIgnoreCase(std::string_view left, std::string_view right)
{
    return ToLowerCopy(left) == ToLowerCopy(right);
}

bool StartsWithIgnoreCase(std::string_view text, std::string_view prefix)
{
    return text.size() >= prefix.size()
        && EqualsIgnoreCase(text.substr(0, prefix.size()), prefix);
}

std::string ExtractPathTrackDeadEndTarget(std::string_view alert_detail)
{
    constexpr std::string_view kDeadEndMarker = "dead end link";

    const std::string normalized = ToLowerCopy(alert_detail);
    const std::size_t marker_index = normalized.find(kDeadEndMarker);
    if (marker_index == std::string::npos)
    {
        return {};
    }

    std::string target = TrimWhitespaceCopy(
        alert_detail.substr(marker_index + kDeadEndMarker.size()));
    while (!target.empty()
           && (target.back() == '.'
               || target.back() == ','
               || target.back() == ';'
               || target.back() == ':'))
    {
        target.pop_back();
    }

    return target;
}

hl::common::LogLevel VerboseTraceLevel(bool verbose_trace)
{
    return verbose_trace
        ? hl::common::LogLevel::Info
        : hl::common::LogLevel::Debug;
}

void LogSubsystemInfo(
    hl::common::LogCategory category,
    bool verbose_trace,
    std::string_view message)
{
    hl::common::Logger::Log(category, VerboseTraceLevel(verbose_trace), message);
}

hl::common::LogCategory ClassifyMovementMessageCategory(std::string_view message)
{
    if (StartsWithIgnoreCase(message, "PathNodeMessageRuntime:")
        || StartsWithIgnoreCase(message, "PathNodeEventSemanticsController:")
        || StartsWithIgnoreCase(message, "BrushDoorBootstrapController:")
        || StartsWithIgnoreCase(message, "PathAdvanceController:")
        || StartsWithIgnoreCase(message, "PathArrivalCalibrator:"))
    {
        return hl::common::LogCategory::PathEvent;
    }

    if (StartsWithIgnoreCase(message, "PathMoverController:")
        || StartsWithIgnoreCase(message, "PathTraversalController:")
        || StartsWithIgnoreCase(message, "TrackPathResolver:"))
    {
        return hl::common::LogCategory::Path;
    }

    return hl::common::LogCategory::Scripted;
}

void AppendJoinedStrings(
    std::ostringstream& stream,
    std::string_view label,
    const std::vector<std::string>& values)
{
    stream << '|' << label << '=';
    if (values.empty())
    {
        stream << "<none>";
        return;
    }

    for (std::size_t index = 0; index < values.size(); ++index)
    {
        if (index != 0)
        {
            stream << ';';
        }
        stream << values[index];
    }
}

std::string JoinStringValues(const std::vector<std::string>& values);

void AppendNamedCounts(
    std::ostringstream& stream,
    std::string_view label,
    const std::vector<hl::game_api::NamedCountSummary>& values)
{
    stream << '|' << label << '=';
    if (values.empty())
    {
        stream << "<none>";
        return;
    }

    for (std::size_t index = 0; index < values.size(); ++index)
    {
        if (index != 0)
        {
            stream << ';';
        }
        stream << values[index].name << ':' << values[index].count;
    }
}

std::string BuildMovementFrameLogLine(
    const hl::game_api::ScriptedMovementFrameStateSummary& frame)
{
    return "MovementFrame: frame " + std::to_string(frame.frame_number)
        + " scripted moving/arrived/blocked="
        + std::to_string(frame.scenes_moving) + "/"
        + std::to_string(frame.scene_arrivals) + "/"
        + std::to_string(frame.blocked_scenes)
        + " path active/moving/stopped/blocked="
        + std::to_string(frame.path_movers_active) + "/"
        + std::to_string(frame.path_movers_moving) + "/"
        + std::to_string(frame.stopped_path_movers) + "/"
        + std::to_string(frame.blocked_path_movers)
        + " pathArrivals=" + std::to_string(frame.path_node_arrivals)
        + " pathAdvances=" + std::to_string(frame.path_nodes_advanced)
        + " delayed due/executed/pending="
        + std::to_string(frame.delayed_actions_due) + "/"
        + std::to_string(frame.delayed_actions_executed) + "/"
        + std::to_string(frame.delayed_actions_pending)
        + " ftruck_a=" + (frame.ftruck_status.empty()
            ? std::string("<unset>")
            : frame.ftruck_status);
}

std::string BuildMovementFrameStateValue(
    const hl::game_api::ScriptedMovementFrameStateSummary& frame)
{
    std::ostringstream stream;
    stream
        << "moving=" << frame.scenes_moving
        << "|arrived=" << frame.scene_arrivals
        << "|blockedScenes=" << frame.blocked_scenes
        << "|pathActive=" << frame.path_movers_active
        << "|pathMoving=" << frame.path_movers_moving
        << "|pathStopped=" << frame.stopped_path_movers
        << "|pathBlocked=" << frame.blocked_path_movers
        << "|pathArrivals=" << frame.path_node_arrivals
        << "|pathAdvances=" << frame.path_nodes_advanced
        << "|delayedDue=" << frame.delayed_actions_due
        << "|delayedExecuted=" << frame.delayed_actions_executed
        << "|delayedPending=" << frame.delayed_actions_pending
        << "|ftruck=" << (frame.ftruck_status.empty() ? std::string("<unset>") : frame.ftruck_status);
    AppendNamedCounts(stream, "blockedReasons", frame.blocked_by_reason);
    AppendJoinedStrings(stream, "pathMessages", frame.path_messages_this_frame);
    AppendJoinedStrings(stream, "pathEvents", frame.path_events);
    return stream.str();
}

bool MovementFrameHasImportantEvent(
    const hl::game_api::ScriptedMovementFrameStateSummary& frame)
{
    return frame.scene_arrivals > 0
        || frame.blocked_scenes > 0
        || frame.blocked_path_movers > 0
        || frame.path_node_arrivals > 0
        || frame.path_nodes_advanced > 0
        || !frame.path_messages_this_frame.empty()
        || !frame.path_events.empty();
}

void LogMovementFrameSummary(
    const hl::game_api::FrameBootstrapOptions& options,
    const hl::game_api::ScriptedMovementFrameStateSummary& frame)
{
    const hl::common::LogLevel level = VerboseTraceLevel(options.trace_movement);
    const bool important_event = MovementFrameHasImportantEvent(frame);
    const std::string line = BuildMovementFrameLogLine(frame);
    bool header_logged = false;

    if (options.log_state_changes_only)
    {
        header_logged = hl::common::Logger::LogStateChange(
            hl::common::LogCategory::Path,
            level,
            "movement-frame",
            BuildMovementFrameStateValue(frame),
            line);
    }
    else
    {
        header_logged = hl::common::Logger::LogFrameSampled(
            hl::common::LogCategory::Path,
            level,
            frame.frame_number,
            options.log_frame_sample,
            line,
            false,
            important_event);
    }

    if (!header_logged)
    {
        return;
    }

    for (const std::string& path_message : frame.path_messages_this_frame)
    {
        hl::common::Logger::Log(
            hl::common::LogCategory::PathEvent,
            level,
            "  - path-message " + path_message);
    }
    for (const std::string& active_mover : frame.active_path_movers)
    {
        hl::common::Logger::Log(
            hl::common::LogCategory::Path,
            level,
            "  - path " + active_mover);
    }
    for (const std::string& path_event : frame.path_events)
    {
        hl::common::Logger::Log(
            hl::common::LogCategory::PathEvent,
            level,
            "  - path-event " + path_event);
    }
}

std::string BuildScriptedLogicFrameLogLine(
    const hl::game_api::ScriptedLogicFrameStateSummary& frame)
{
    return "ScriptedLogic: frame " + std::to_string(frame.frame_number)
        + " queue=" + std::to_string(frame.queue_size_before)
        + "->" + std::to_string(frame.queue_size_after)
        + " due/executed/rescheduled="
        + std::to_string(frame.due_delayed_actions) + "/"
        + std::to_string(frame.executed_delayed_actions) + "/"
        + std::to_string(frame.rescheduled_delayed_actions)
        + " longDelay pending/executed="
        + std::to_string(frame.long_delay_pending) + "/"
        + std::to_string(frame.long_delay_executed)
        + " progressed=" + std::to_string(frame.newly_progressed_scripted_entities)
        + " blocked=" + std::to_string(frame.blocked_scripted_entities);
}

std::string BuildScriptedLogicFrameStateValue(
    const hl::game_api::ScriptedLogicFrameStateSummary& frame)
{
    std::ostringstream stream;
    stream
        << "queueBefore=" << frame.queue_size_before
        << "|queueAfter=" << frame.queue_size_after
        << "|due=" << frame.due_delayed_actions
        << "|executed=" << frame.executed_delayed_actions
        << "|rescheduled=" << frame.rescheduled_delayed_actions
        << "|skipped=" << frame.skipped_delayed_actions
        << "|failed=" << frame.failed_delayed_actions
        << "|deferred=" << frame.deferred_delayed_actions
        << "|targets=" << frame.target_chains_fired
        << "|resolutions=" << frame.target_resolutions
        << "|useAttempts=" << frame.use_attempts
        << "|useSuccesses=" << frame.use_successes
        << "|useDeferred=" << frame.use_deferred
        << "|useFailures=" << frame.use_failures
        << "|longDelayPending=" << frame.long_delay_pending
        << "|longDelayExecuted=" << frame.long_delay_executed
        << "|progressed=" << frame.newly_progressed_scripted_entities
        << "|blocked=" << frame.blocked_scripted_entities;
    AppendNamedCounts(stream, "blockedReasons", frame.blocked_by_reason);
    return stream.str();
}

bool ScriptedLogicFrameHasImportantEvent(
    const hl::game_api::ScriptedLogicFrameStateSummary& frame)
{
    return frame.due_delayed_actions > 0
        || frame.executed_delayed_actions > 0
        || frame.rescheduled_delayed_actions > 0
        || frame.failed_delayed_actions > 0
        || frame.deferred_delayed_actions > 0
        || frame.long_delay_executed > 0
        || frame.newly_progressed_scripted_entities > 0
        || frame.blocked_scripted_entities > 0
        || !frame.progressing_preview.empty()
        || !frame.pending_delay_preview.empty();
}

void LogScriptedLogicFrameSummary(
    const hl::game_api::FrameBootstrapOptions& options,
    const hl::game_api::ScriptedLogicFrameStateSummary& frame)
{
    const hl::common::LogLevel level = VerboseTraceLevel(options.trace_scripted);
    const bool important_event = ScriptedLogicFrameHasImportantEvent(frame);
    const std::string line = BuildScriptedLogicFrameLogLine(frame);
    bool header_logged = false;

    if (options.log_state_changes_only)
    {
        header_logged = hl::common::Logger::LogStateChange(
            hl::common::LogCategory::Scripted,
            level,
            "scripted-logic-frame",
            BuildScriptedLogicFrameStateValue(frame),
            line);
    }
    else
    {
        header_logged = hl::common::Logger::LogFrameSampled(
            hl::common::LogCategory::Scripted,
            level,
            frame.frame_number,
            options.log_frame_sample,
            line,
            false,
            important_event);
    }

    if (!header_logged)
    {
        return;
    }

    if (!frame.progressing_preview.empty())
    {
        hl::common::Logger::Log(
            hl::common::LogCategory::Scripted,
            level,
            "  - scripted progress preview:");
        for (const std::string& progress_line : frame.progressing_preview)
        {
            hl::common::Logger::Log(
                hl::common::LogCategory::Scripted,
                level,
                "    * " + progress_line);
        }
    }

    if (!frame.blocked_by_reason.empty())
    {
        hl::common::Logger::Log(
            hl::common::LogCategory::Scripted,
            level,
            "  - scripted blocked reasons:");
        for (const hl::game_api::NamedCountSummary& entry : frame.blocked_by_reason)
        {
            hl::common::Logger::Log(
                hl::common::LogCategory::Scripted,
                level,
                "    * " + entry.name + ": " + std::to_string(entry.count));
        }
    }

    if (!frame.pending_delay_preview.empty())
    {
        hl::common::Logger::Log(
            hl::common::LogCategory::Scripted,
            level,
            "  - scripted pending delay preview:");
        for (const std::string& pending_line : frame.pending_delay_preview)
        {
            hl::common::Logger::Log(
                hl::common::LogCategory::Scripted,
                level,
                "    * " + pending_line);
        }
    }
}

void LogCompactServerModuleSummary(const hl::game_api::HlServerModuleSummary& summary)
{
    const hl::common::LoggerStatistics logger_statistics = hl::common::Logger::Statistics();
    const auto find_path_node_canary =
        [&](std::string_view node_name)
        -> const hl::game_api::PathNodeMessageCanarySummary*
        {
            for (const hl::game_api::PathNodeMessageCanarySummary& canary :
                 summary.scripted_movement.path_node_messages.canaries)
            {
                if (EqualsIgnoreCase(canary.node_name, node_name))
                {
                    return &canary;
                }
            }

            return nullptr;
        };
    const auto log_path_node_outcome =
        [&](std::string_view label,
            std::string_view node_name,
            const hl::game_api::PathNodeMessageCanarySummary* canary)
        {
            hl::common::Logger::Info(
                hl::common::LogCategory::Summary,
                "  - " + std::string(label)
                    + " (" + std::string(node_name) + "): reached="
                    + (canary != nullptr && canary->reached ? "yes" : "no")
                    + ", frame="
                    + (canary != nullptr && canary->first_reached_frame >= 0
                        ? std::to_string(canary->first_reached_frame)
                        : std::string("<none>"))
                    + ", time="
                    + (canary != nullptr && canary->reached
                        ? std::to_string(canary->first_reached_time)
                        : std::string("<none>"))
                    + ", message="
                    + (canary != nullptr && !canary->message.empty()
                        ? canary->message
                        : std::string("<none>"))
                    + ", dispatchAttempted="
                    + (canary != nullptr && canary->staged_dispatch_attempted ? "yes" : "no")
                    + ", dispatchResult="
                    + (canary != nullptr && !canary->dispatch_result.empty()
                        ? canary->dispatch_result
                        : std::string("<none>"))
                    + ", classification="
                    + (canary != nullptr && !canary->classification.empty()
                        ? canary->classification
                        : std::string("<none>"))
                    + ", dispatchMode="
                    + (canary != nullptr && !canary->dispatch_mode.empty()
                        ? canary->dispatch_mode
                        : std::string("<none>"))
                    + ", resolvedTargets="
                    + (canary != nullptr
                        ? std::to_string(canary->resolved_targets)
                        : std::string("0"))
                    + ", runtimeTargets="
                    + (canary != nullptr
                        ? std::to_string(canary->runtime_target_candidates)
                        : std::string("0"))
                    + ", parsedTargets="
                    + (canary != nullptr
                        ? std::to_string(canary->parsed_target_candidates)
                        : std::string("0"))
                    + ", pfnUseAttempted="
                    + (canary != nullptr && canary->pfn_use_attempted ? "yes" : "no")
                    + ", downstream="
                    + (canary != nullptr && canary->visible_downstream_progression ? "yes" : "no")
                    + ", downstreamTargetChains="
                    + (canary != nullptr
                        ? std::to_string(canary->downstream_target_chains)
                        : std::string("0"))
                    + ", downstreamScheduledActions="
                    + (canary != nullptr
                        ? std::to_string(canary->downstream_scheduled_actions)
                        : std::string("0"))
                    + ", alertCallbacks="
                    + (canary != nullptr
                        ? std::to_string(canary->downstream_alert_callbacks)
                        : std::string("0"))
                    + ", messageCallbacks="
                    + (canary != nullptr
                        ? std::to_string(canary->downstream_message_callbacks)
                        : std::string("0"))
                    + ", fadeChannelAvailable="
                    + (canary != nullptr && canary->fade_channel_available ? "yes" : "no")
                    + ", fadeChannelUsed="
                    + (canary != nullptr && canary->fade_channel_used ? "yes" : "no")
                    + ", messageChannelAvailable="
                    + (canary != nullptr && canary->message_channel_available ? "yes" : "no")
                    + ", messageChannelUsed="
                    + (canary != nullptr && canary->message_channel_used ? "yes" : "no")
                    + ", envMessageLinkageFound="
                    + (canary != nullptr && canary->env_message_linkage_found ? "yes" : "no")
                    + ", envMessageLinkageUsed="
                    + (canary != nullptr && canary->env_message_linkage_used ? "yes" : "no")
                    + ", summaryFallbackUsed="
                    + (canary != nullptr && canary->summary_only_fallback_used ? "yes" : "no")
                    + ", targetClassnames="
                    + (canary != nullptr
                        ? JoinStringValues(canary->target_classnames)
                        : std::string("<none>"))
                    + ", resolvedTargetDetails="
                    + (canary != nullptr
                        ? JoinStringValues(canary->resolved_target_details)
                        : std::string("<none>"))
                    + ", moverSpeed="
                    + (canary != nullptr
                        ? std::to_string(canary->mover_speed_at_encounter)
                        : std::string("0"))
                    + ", downstreamSummary="
                    + (canary != nullptr && !canary->downstream_summary.empty()
                        ? canary->downstream_summary
                        : std::string("<none>"))
                    + ", presentationLinkage="
                    + (canary != nullptr && !canary->presentation_linkage_detail.empty()
                        ? canary->presentation_linkage_detail
                        : std::string("<none>"))
                    + ", brushDoorAttempted="
                    + (canary != nullptr && canary->brush_door_handling_attempted ? "yes" : "no")
                    + ", brushDoorPath="
                    + (canary != nullptr && !canary->brush_door_dispatch_path.empty()
                        ? canary->brush_door_dispatch_path
                        : std::string("<none>"))
                    + ", brushDoorState="
                    + (canary != nullptr && !canary->brush_door_state.empty()
                        ? canary->brush_door_state
                        : std::string("<none>"))
                    + ", brushDoorSupport="
                    + (canary != nullptr && !canary->brush_door_support_state.empty()
                        ? canary->brush_door_support_state
                        : std::string("<none>"))
                    + ", brushDoorMoveStarted="
                    + (canary != nullptr && canary->brush_door_movement_started ? "yes" : "no")
                    + ", brushDoorMoveCompleted="
                    + (canary != nullptr && canary->brush_door_movement_completed ? "yes" : "no")
                    + ", brushDoorAudit="
                    + (canary != nullptr && !canary->brush_door_runtime_audit.empty()
                        ? canary->brush_door_runtime_audit
                        : std::string("<none>"))
                    + ", requiredSubsystem="
                    + (canary != nullptr && !canary->required_subsystem.empty()
                        ? canary->required_subsystem
                        : std::string("<none>")));
        };
    const hl::game_api::PathNodeMessageCanarySummary* room2train_canary =
        find_path_node_canary("trainstop8");
    const hl::game_api::PathNodeMessageCanarySummary* room2start_canary =
        find_path_node_canary("trainstop8a");
    const hl::game_api::PathNodeMessageCanarySummary* execute_sci_canary =
        find_path_node_canary("trainstop9");
    const hl::game_api::PathNodeMessageCanarySummary* fade_out_canary =
        find_path_node_canary("trainstop11");

    std::string readiness_summary = summary.scripted_movement.readiness;
    if (fade_out_canary != nullptr && fade_out_canary->reached)
    {
        readiness_summary = fade_out_canary->fade_channel_used
            ? "execute_sci and fade_out were both reached; fade_out now routes through staged ScreenFade bootstrap while deterministic traversal stays stable"
            : fade_out_canary->message_channel_used
            ? "execute_sci and fade_out were both reached; fade_out now routes through staged message-channel bootstrap without requiring full client UI"
            : fade_out_canary->env_message_linkage_used
            ? "execute_sci and fade_out were both reached; fade_out now routes through staged env_message linkage and can be narrowed further per-map if needed"
            : fade_out_canary->summary_only_fallback_used
            ? "execute_sci and fade_out were both reached; fade_out is no longer generic unsupported and is now captured by a staged summary-only presentation sink"
            : !fade_out_canary->required_subsystem.empty()
            ? "execute_sci and fade_out were both reached; next step is the remaining staged presentation gap for fade_out via "
                + fade_out_canary->required_subsystem
            : "execute_sci and fade_out were both reached; validate later downstream scripted aftermath while keeping deterministic traversal stable";
    }
    else if (execute_sci_canary != nullptr && execute_sci_canary->reached)
    {
        readiness_summary =
            execute_sci_canary->runtime_target_candidates == 0
            && execute_sci_canary->parsed_target_candidates > 0
            ? "execute_sci was reached through staged parsed-only dispatch; next step is broader scripted actor/bootstrap progression semantics before later fade/client work"
            : "execute_sci was reached; continue deterministic traversal toward fade_out while preserving current staged dispatch semantics";
    }

    hl::common::Logger::Info(hl::common::LogCategory::Summary, "hl.dll shim summary:");
    hl::common::Logger::Info(
        hl::common::LogCategory::Summary,
        "  - load/init: hl.dll="
            + std::string(BoolToYesNo(summary.hl_dll_loaded))
            + ", GiveFnptrsToDll="
            + BoolToYesNo(summary.give_fnptrs_to_dll_called)
            + ", GetEntityAPI2="
            + BoolToYesNo(summary.get_entity_api2_succeeded)
            + ", DLL_FUNCTIONS="
            + BoolToYesNo(summary.dll_functions_acquired)
            + ", pfnGameInit="
            + BoolToYesNo(summary.pfn_game_init_succeeded || !summary.pfn_game_init_present));
    hl::common::Logger::Info(
        hl::common::LogCategory::Summary,
        "  - cfg/cvars: executed_cfg="
            + std::to_string(summary.executed_cfg_files)
            + ", queued_cmds=" + std::to_string(summary.queued_server_commands)
            + ", executed_cmds=" + std::to_string(summary.executed_server_commands)
            + ", cvars_from_cfg=" + std::to_string(summary.updated_cvars_from_cfg)
            + ", auto_created_cvars=" + std::to_string(summary.auto_created_cvars));
    hl::common::Logger::Info(
        hl::common::LogCategory::Summary,
        "  - world/bootstrap: world="
            + std::string(BoolToYesNo(summary.world_bootstrap.completed))
            + ", entity_pipeline="
            + BoolToYesNo(
                summary.entity_pipeline.entities_lump_parsed
                || summary.entity_pipeline.partially_parsed)
            + ", worldspawn="
            + BoolToYesNo(summary.worldspawn_spawn.succeeded)
            + ", ServerActivate="
            + BoolToYesNo(summary.server_activation.succeeded));
    hl::common::Logger::Info(
        hl::common::LogCategory::Summary,
        "  - frame bootstrap: frames="
            + std::to_string(summary.frame_bootstrap_config.frames)
            + ", frametime=" + std::to_string(summary.frame_bootstrap_config.frametime)
            + ", think_limit=" + std::to_string(summary.frame_bootstrap_config.think_limit)
            + ", use_limit=" + std::to_string(summary.frame_bootstrap_config.use_limit)
            + ", scheduled_use_limit="
            + std::to_string(summary.frame_bootstrap_config.scheduled_use_limit)
            + ", sample=" + std::to_string(summary.frame_bootstrap_config.log_frame_sample)
            + ", state_changes_only="
            + BoolToYesNo(summary.frame_bootstrap_config.log_state_changes_only)
            + ", trace_scripted="
            + BoolToYesNo(summary.frame_bootstrap_config.trace_scripted)
            + ", trace_movement="
            + BoolToYesNo(summary.frame_bootstrap_config.trace_movement)
            + ", trace_think="
            + BoolToYesNo(summary.frame_bootstrap_config.trace_think)
            + ", trace_callbacks="
            + BoolToYesNo(summary.frame_bootstrap_config.trace_callbacks));
    hl::common::Logger::Info(
        hl::common::LogCategory::Summary,
        "  - frame loop: requested="
            + std::to_string(summary.server_frame_loop.frames_requested)
            + ", completed=" + std::to_string(summary.server_frame_loop.frames_completed)
            + ", stopped_early=" + BoolToYesNo(summary.server_frame_loop.stopped_early)
            + ", final_time=" + std::to_string(summary.server_frame_loop.final_time)
            + ", any_seh=" + BoolToYesNo(summary.server_frame_loop.any_seh));
    hl::common::Logger::Info(
        hl::common::LogCategory::Summary,
        "  - logger: repeat_suppressions="
            + std::to_string(logger_statistics.suppressed_repeat_count)
            + ", total_bytes_written="
            + std::to_string(logger_statistics.total_bytes_written));
    if (!summary.server_frame_loop.stop_reason.empty())
    {
        hl::common::Logger::Info(
            hl::common::LogCategory::Summary,
            "  - frame loop stop reason: " + summary.server_frame_loop.stop_reason);
    }
    hl::common::Logger::Info(
        hl::common::LogCategory::Summary,
        "  - think scheduler: due/executed/deferred/failures="
            + std::to_string(summary.entity_think_scheduler.total_due_thinks) + "/"
            + std::to_string(summary.entity_think_scheduler.total_executed_thinks) + "/"
            + std::to_string(summary.entity_think_scheduler.total_deferred_thinks) + "/"
            + std::to_string(summary.entity_think_scheduler.total_think_failures));
    hl::common::Logger::Info(
        hl::common::LogCategory::Summary,
        "  - use-target dispatcher: chains/resolutions/use_ok/use_deferred/use_failures="
            + std::to_string(summary.map_logic_dispatcher.total_target_chains_fired) + "/"
            + std::to_string(summary.map_logic_dispatcher.total_target_resolutions) + "/"
            + std::to_string(summary.map_logic_dispatcher.total_use_successes) + "/"
            + std::to_string(summary.map_logic_dispatcher.total_use_deferred) + "/"
            + std::to_string(summary.map_logic_dispatcher.total_use_failures));
    if (summary.changelevel_transition.candidate_present)
    {
        std::string line =
            "  - trigger_changelevel: candidate="
            + FormatChangeLevelCandidate(summary.changelevel_transition)
            + " staged_supported="
            + BoolToYesNo(summary.changelevel_transition.staged_supported)
            + ", deferred="
            + BoolToYesNo(summary.changelevel_transition.deferred_candidate);
        if (summary.changelevel_transition.pending_request_captured)
        {
            line += ", pending_changelevel_request="
                + FormatChangeLevelCandidate(summary.changelevel_transition);
        }
        else
        {
            line += ", pending_changelevel_request=<none>";
        }
        if (!summary.changelevel_transition.pending_request_detail.empty())
        {
            line += ", note=" + summary.changelevel_transition.pending_request_detail;
        }
        hl::common::Logger::Info(hl::common::LogCategory::Summary, line);
        if (summary.changelevel_transition.transition_intent_captured)
        {
            std::string intent_line =
                "  - changelevel_transition_intent: "
                + FormatChangeLevelTransitionIntentSummary(summary.changelevel_transition);
            if (!summary.changelevel_transition.transition_intent_detail.empty())
            {
                intent_line += ", note=" + summary.changelevel_transition.transition_intent_detail;
            }
            hl::common::Logger::Info(hl::common::LogCategory::Summary, intent_line);
        }
        if (summary.changelevel_transition.pre_changelevel_handoff.active)
        {
            std::string handoff_line =
                "  - pre_changelevel_handoff: "
                + FormatPreChangeLevelHandoffSummary(summary.changelevel_transition);
            if (!summary.changelevel_transition.pre_changelevel_handoff.detail.empty())
            {
                handoff_line += ", note="
                    + summary.changelevel_transition.pre_changelevel_handoff.detail;
            }
            hl::common::Logger::Info(hl::common::LogCategory::Summary, handoff_line);
        }
        if (summary.changelevel_transition.target_validation.attempted)
        {
            hl::common::Logger::Info(
                hl::common::LogCategory::Summary,
                "  - changelevel_target_validation: "
                    + FormatChangeLevelTargetValidationSummary(summary.changelevel_transition));
        }
        if (summary.changelevel_transition.lifecycle_gate.attempted)
        {
            hl::common::Logger::Info(
                hl::common::LogCategory::Summary,
                "  - changelevel_lifecycle_gate: "
                    + FormatChangeLevelLifecycleGateSummary(summary.changelevel_transition));
        }
        if (summary.changelevel_transition.lifecycle_entry.attempted)
        {
            hl::common::Logger::Info(
                hl::common::LogCategory::Summary,
                "  - changelevel_lifecycle_entry: "
                    + FormatChangeLevelLifecycleEntrySummary(summary.changelevel_transition));
        }
        if (summary.changelevel_transition.lifecycle_dispatch.attempted)
        {
            hl::common::Logger::Info(
                hl::common::LogCategory::Summary,
                "  - changelevel_lifecycle_dispatch: "
                    + FormatChangeLevelLifecycleDispatchSummary(summary.changelevel_transition));
        }
        if (summary.changelevel_transition.lifecycle_execution.attempted)
        {
            hl::common::Logger::Info(
                hl::common::LogCategory::Summary,
                "  - changelevel_lifecycle_execution: "
                    + FormatChangeLevelLifecycleExecutionSummary(summary.changelevel_transition));
        }
        if (summary.changelevel_transition.changelevel_bootstrap_plan.attempted)
        {
            hl::common::Logger::Info(
                hl::common::LogCategory::Summary,
                "  - changelevel_bootstrap_plan: "
                    + FormatChangeLevelBootstrapPlanSummary(summary.changelevel_transition));
        }
        if (summary.changelevel_transition.changelevel_landmark_transform.attempted)
        {
            hl::common::Logger::Info(
                hl::common::LogCategory::Summary,
                "  - changelevel_landmark_transform: "
                    + FormatChangeLevelLandmarkTransformSummary(summary.changelevel_transition));
        }
        if (summary.changelevel_transition.changelevel_projected_carried_origin.attempted)
        {
            hl::common::Logger::Info(
                hl::common::LogCategory::Summary,
                "  - changelevel_projected_carried_origin: "
                    + FormatChangeLevelProjectedCarriedOriginSummary(
                        summary.changelevel_transition));
        }
        if (summary.changelevel_transition.changelevel_projected_carried_orientation.attempted)
        {
            hl::common::Logger::Info(
                hl::common::LogCategory::Summary,
                "  - changelevel_projected_carried_orientation: "
                    + FormatChangeLevelProjectedCarriedOrientationSummary(
                        summary.changelevel_transition));
        }
        if (summary.changelevel_transition.changelevel_projected_transfer_snapshot.attempted)
        {
            hl::common::Logger::Info(
                hl::common::LogCategory::Summary,
                "  - changelevel_projected_transfer_snapshot: "
                    + FormatChangeLevelProjectedTransferSnapshotSummary(
                        summary.changelevel_transition));
        }
        if (summary.changelevel_transition.changelevel_player_transfer_apply_plan.attempted)
        {
            hl::common::Logger::Info(
                hl::common::LogCategory::Summary,
                "  - changelevel_player_transfer_apply_plan: "
                    + FormatChangeLevelPlayerTransferApplyPlanSummary(
                        summary.changelevel_transition));
        }
        if (summary.changelevel_transition.changelevel_player_transfer_write_set.attempted)
        {
            hl::common::Logger::Info(
                hl::common::LogCategory::Summary,
                "  - changelevel_player_transfer_write_set: "
                    + FormatChangeLevelPlayerTransferWriteSetSummary(
                        summary.changelevel_transition));
        }
        if (summary.changelevel_transition.changelevel_player_transfer_deferred_apply_gate
                .attempted)
        {
            hl::common::Logger::Info(
                hl::common::LogCategory::Summary,
                "  - changelevel_player_transfer_deferred_apply_gate: "
                    + FormatChangeLevelPlayerTransferDeferredApplyGateSummary(
                        summary.changelevel_transition));
        }
        if (summary.changelevel_transition.changelevel_player_transfer_gate_open_checkpoint
                .attempted)
        {
            hl::common::Logger::Info(
                hl::common::LogCategory::Summary,
                "  - changelevel_player_transfer_gate_open_checkpoint: "
                    + FormatChangeLevelPlayerTransferGateOpenCheckpointSummary(
                        summary.changelevel_transition));
        }
        if (summary.changelevel_transition.changelevel_player_transfer_checkpoint_signal_contract
                .attempted)
        {
            hl::common::Logger::Info(
                hl::common::LogCategory::Summary,
                "  - changelevel_player_transfer_checkpoint_signal_contract: "
                    + FormatChangeLevelPlayerTransferCheckpointSignalContractSummary(
                        summary.changelevel_transition));
        }
        if (summary.changelevel_transition.post_handoff_activity.measured)
        {
            hl::common::Logger::Info(
                hl::common::LogCategory::Summary,
                "  - post_handoff_activity: "
                    + FormatPostHandoffActivitySummary(summary.changelevel_transition));
            hl::common::Logger::Info(
                hl::common::LogCategory::Summary,
                "  - post_handoff_first: "
                    + FormatPostHandoffFirstSummary(summary.changelevel_transition));
            if (!summary.changelevel_transition.post_handoff_activity.sample_effects.empty())
            {
                hl::common::Logger::Info(
                    hl::common::LogCategory::Summary,
                    "  - post_handoff_samples: "
                        + JoinStringValues(
                            summary.changelevel_transition.post_handoff_activity.sample_effects));
            }
        }
        if (summary.changelevel_transition.moving_surrogate_samples > 0)
        {
            hl::common::Logger::Info(
                hl::common::LogCategory::Summary,
                "  - trigger_changelevel geometry: follow=ftruck_a, trigger_bounds="
                    + summary.changelevel_transition.trigger_bounds_text
                    + ", surrogate_path_envelope="
                    + summary.changelevel_transition.surrogate_path_envelope_text
                    + ", local_offset="
                    + summary.changelevel_transition.moving_surrogate_local_offset_text
                    + ", samples="
                    + std::to_string(summary.changelevel_transition.moving_surrogate_samples)
                    + ", closest_approach="
                    + std::to_string(summary.changelevel_transition.closest_approach_distance)
                    + ", closest_frame="
                    + std::to_string(summary.changelevel_transition.closest_approach_frame)
                    + ", closest_time="
                    + std::to_string(summary.changelevel_transition.closest_approach_time)
                    + ", closest_surrogate_origin="
                    + summary.changelevel_transition.closest_surrogate_origin_text
                    + ", overlap="
                    + (summary.changelevel_transition.closest_approach_distance == 0.0f
                        ? std::string("yes")
                        : std::string("no")));
        }
    }
    hl::common::Logger::Info(
        hl::common::LogCategory::Summary,
        "  - scripted logic: frames="
            + std::to_string(summary.scripted_logic.frames_completed)
            + ", progressed=" + std::to_string(summary.scripted_logic.total_progressed_entities)
            + ", long_delay pending/executed="
            + std::to_string(summary.scripted_logic.total_long_delay_pending) + "/"
            + std::to_string(summary.scripted_logic.total_long_delay_executed));
    hl::common::Logger::Info(
        hl::common::LogCategory::Summary,
        "  - scripted movement: frames="
            + std::to_string(summary.scripted_movement.frames_completed)
            + ", scene_movement success/block="
            + std::to_string(summary.scripted_movement.scene_movement_successes) + "/"
            + std::to_string(summary.scripted_movement.scene_movement_blocked)
            + ", path arrivals/advances/messages="
            + std::to_string(summary.scripted_movement.path_movers.arrived_at_node) + "/"
            + std::to_string(summary.scripted_movement.path_movers.path_advances) + "/"
            + std::to_string(summary.scripted_movement.path_messages_encountered.size()));
    hl::common::Logger::Info(
        hl::common::LogCategory::Summary,
        "  - brush-door bootstrap: tracked/use-supported/moving/opened/blocked/deferred="
            + std::to_string(summary.scripted_movement.brush_doors.tracked_doors) + "/"
            + std::to_string(summary.scripted_movement.brush_doors.use_supported) + "/"
            + std::to_string(summary.scripted_movement.brush_doors.moving) + "/"
            + std::to_string(summary.scripted_movement.brush_doors.opened) + "/"
            + std::to_string(summary.scripted_movement.brush_doors.blocked) + "/"
            + std::to_string(summary.scripted_movement.brush_doors.deferred)
            + ", callbacks="
            + JoinStringValues(summary.scripted_movement.brush_doors.exercised_callbacks));
    hl::common::Logger::Info(
        hl::common::LogCategory::Summary,
        "  - path-node callbacks exercised: "
            + JoinStringValues(summary.scripted_movement.path_mover_callbacks_exercised));
    hl::common::Logger::Info(
        hl::common::LogCategory::Summary,
        "  - movement/path callbacks exercised: "
            + JoinStringValues(summary.scripted_movement.exercised_callbacks));
    hl::common::Logger::Info(
        hl::common::LogCategory::Summary,
        "  - ftruck_a final: current="
            + (summary.scripted_movement.ftruck_final_current.empty()
                ? std::string("<none>")
                : summary.scripted_movement.ftruck_final_current)
            + ", next="
            + (summary.scripted_movement.ftruck_final_next.empty()
                ? std::string("<none>")
                : summary.scripted_movement.ftruck_final_next)
            + ", status="
            + (summary.scripted_movement.delayed_ftruck_status.empty()
                ? std::string("<unset>")
                : summary.scripted_movement.delayed_ftruck_status));
    if (summary.scripted_movement.path_node_messages.first_message_bearing_node_reached)
    {
        hl::common::Logger::Info(
            hl::common::LogCategory::Summary,
            "  - first message node: "
                + summary.scripted_movement.path_node_messages.first_message_bearing_node
                + " frame="
                + std::to_string(
                    summary.scripted_movement.path_node_messages.first_message_bearing_frame)
                + " time="
                + std::to_string(
                    summary.scripted_movement.path_node_messages.first_message_bearing_time));
    }
    hl::common::Logger::Info(
        hl::common::LogCategory::Summary,
        "  - path-event report: deepest="
            + (summary.scripted_movement.path_node_messages.deepest_node_reached.empty()
                ? std::string("<none>")
                : summary.scripted_movement.path_node_messages.deepest_node_reached)
            + ", reachedMessages="
            + JoinStringValues(summary.scripted_movement.path_node_messages.reached_message_nodes)
            + ", dispatch attempts/successes/deferred/failures/unresolved="
            + std::to_string(summary.scripted_movement.path_node_messages.staged_dispatch_attempts)
            + "/" + std::to_string(summary.scripted_movement.path_node_messages.staged_dispatch_successes)
            + "/" + std::to_string(summary.scripted_movement.path_node_messages.staged_dispatch_deferred)
            + "/" + std::to_string(summary.scripted_movement.path_node_messages.staged_dispatch_failures)
            + "/" + std::to_string(summary.scripted_movement.path_node_messages.unresolved_message_targets)
            + ", classified resolved/no-target/unresolved/unsupported="
            + std::to_string(summary.scripted_movement.path_node_messages.resolved_and_dispatched)
            + "/" + std::to_string(summary.scripted_movement.path_node_messages.encountered_no_target)
            + "/" + std::to_string(summary.scripted_movement.path_node_messages.encountered_unresolved_targets)
            + "/" + std::to_string(summary.scripted_movement.path_node_messages.encountered_unsupported));
    log_path_node_outcome("room2train", "trainstop8", room2train_canary);
    log_path_node_outcome("room2start", "trainstop8a", room2start_canary);
    log_path_node_outcome("execute_sci", "trainstop9", execute_sci_canary);
    log_path_node_outcome("fade_out", "trainstop11", fade_out_canary);
    if (fade_out_canary != nullptr
        && fade_out_canary->reached
        && !fade_out_canary->presentation_semantics_summary.empty())
    {
        hl::common::Logger::Info(
            hl::common::LogCategory::Summary,
            "  - fade_out semantics: " + fade_out_canary->presentation_semantics_summary);
    }
    hl::common::Logger::Info(
        hl::common::LogCategory::Summary,
        "  - readiness: "
            + (!readiness_summary.empty()
                ? readiness_summary
                : !summary.scripted_logic.readiness.empty()
                ? summary.scripted_logic.readiness
                : !summary.map_logic_dispatcher.readiness.empty()
                ? summary.map_logic_dispatcher.readiness
                : !summary.entity_think_scheduler.readiness.empty()
                ? summary.entity_think_scheduler.readiness
                : (!summary.server_frame_loop.readiness.empty()
                    ? summary.server_frame_loop.readiness
                    : std::string("not ready"))));
}

const char* RuntimeEntitySupportStateLabel(RuntimeEntitySupportState state)
{
    switch (state)
    {
    case RuntimeEntitySupportState::kSpawnedSuccessfully:
        return "spawned";
    case RuntimeEntitySupportState::kRemovedDuringSpawn:
        return "removed";
    case RuntimeEntitySupportState::kDeferredUnsupported:
        return "deferred";
    case RuntimeEntitySupportState::kFailedDuringSpawn:
        return "failed";
    default:
        return "pending";
    }
}

const char* RuntimeEntityLifecycleStateLabel(RuntimeEntityLifecycleState state)
{
    switch (state)
    {
    case RuntimeEntityLifecycleState::kActiveSupported:
        return "active supported";
    case RuntimeEntityLifecycleState::kPassiveSupported:
        return "passive supported";
    case RuntimeEntityLifecycleState::kDetectedButDeferred:
        return "detected but deferred";
    case RuntimeEntityLifecycleState::kRemovedByGameLogic:
        return "removed by game logic";
    default:
        return "unknown";
    }
}

struct EntitySupportDecision
{
    bool allow_spawn = false;
    bool keep_resident_when_deferred = false;
    std::string reason;
};

struct TriggerChangeLevelFields
{
    std::string map_name;
    std::string landmark;
};

void NoteTriggerChangeLevelCandidate(
    hl::game_api::ChangeLevelTransitionSummary& summary,
    const hl::game_api::detail::EntityDefinition* definition,
    const RuntimeEntityRecord* record);

void CapturePendingChangeLevelRequest(
    EngineShimState& state,
    const hl::game_api::detail::EntityDefinition* definition,
    const RuntimeEntityRecord* record,
    int frame_number,
    float time,
    std::string_view detail);

void ConsumePendingChangeLevelRequest(EngineShimState& state);

EntitySupportDecision ClassifyEntitySupport(std::string_view classname)
{
    static constexpr std::array<std::string_view, 11> kSafeSpawnClasses = {{
        "worldspawn",
        "light",
        "info_player_start",
        "info_player_deathmatch",
        "trigger_auto",
        "env_message",
        "env_glow",
        "light_spot",
        "multi_manager",
        "func_wall",
        "path_track",
    }};

    const std::string normalized = ToLowerCopy(classname);
    if (std::find(kSafeSpawnClasses.begin(), kSafeSpawnClasses.end(), normalized)
        != kSafeSpawnClasses.end())
    {
        return {true, false, {}};
    }

    if (normalized == "trigger_relay")
    {
        return {false, true, "trigger_relay staged bootstrap deferred"};
    }

    if (normalized == "func_door")
    {
        return {false, true, "func_door runtime target audit deferred"};
    }

    if (normalized == "ambient_generic")
    {
        return {false, true, "ambient sound support deferred"};
    }

    if (normalized == "func_tracktrain")
    {
        return {false, true, "func_tracktrain bootstrap deferred"};
    }

    if (normalized == "trigger_changelevel")
    {
        return {false, true, "trigger_changelevel staged changelevel candidate deferred"};
    }

    if (normalized.rfind("monster_", 0) == 0)
    {
        return {false, true, "monster support deferred"};
    }

    if (normalized == "scripted_sequence" || normalized.rfind("scripted_", 0) == 0)
    {
        return {false, true, "scripted entity deferred"};
    }

    return {false, false, "not in safe activation whitelist"};
}

hl::game_api::MapLogicSupportState ClassifyRuntimeMapLogicSupportState(
    const RuntimeEntityRecord& record)
{
    if (record.removed || !record.in_use || (record.flags & FL_KILLME) != 0)
    {
        return hl::game_api::MapLogicSupportState::kRemovedByGameLogic;
    }

    const std::string normalized = ToLowerCopy(record.classname);
    if (normalized == "trigger_auto"
        || normalized == "multi_manager"
        || normalized == "env_message"
        || normalized == "ambient_generic"
        || normalized == "scripted_sequence"
        || normalized == "func_tracktrain")
    {
        return hl::game_api::MapLogicSupportState::kUseSupported;
    }

    if (normalized == "trigger_changelevel")
    {
        return hl::game_api::MapLogicSupportState::kPassiveRecipientOnly;
    }

    if (normalized == "env_glow"
        || normalized == "path_track"
        || normalized == "info_player_start"
        || normalized == "info_player_deathmatch"
        || normalized == "monster_barney"
        || normalized == "monster_scientist"
        || normalized == "monster_sitting_scientist"
        || normalized == "worldspawn"
        || normalized == "func_wall"
        || normalized == "light"
        || normalized == "light_spot")
    {
        return hl::game_api::MapLogicSupportState::kPassiveRecipientOnly;
    }

    if ((normalized == "scripted_sequence" || normalized.rfind("scripted_", 0) == 0)
        && normalized != "scripted_sequence")
    {
        return hl::game_api::MapLogicSupportState::kDetectedButDeferred;
    }

    if (record.deferred)
    {
        return hl::game_api::MapLogicSupportState::kDetectedButDeferred;
    }

    return hl::game_api::MapLogicSupportState::kPassiveRecipientOnly;
}

void RefreshRuntimeMapLogicFlags(RuntimeEntityRecord& record)
{
    record.map_logic_support_state = ClassifyRuntimeMapLogicSupportState(record);
    record.can_think =
        EqualsIgnoreCase(record.classname, "trigger_auto");
    record.can_receive_use =
        record.map_logic_support_state == hl::game_api::MapLogicSupportState::kUseSupported;
    record.can_emit_targets =
        EqualsIgnoreCase(record.classname, "trigger_auto")
        || EqualsIgnoreCase(record.classname, "multi_manager")
        || EqualsIgnoreCase(record.classname, "scripted_sequence")
        || EqualsIgnoreCase(record.classname, "trigger_relay");
    record.blocked_or_deferred =
        (record.deferred
            && record.map_logic_support_state != hl::game_api::MapLogicSupportState::kUseSupported)
        || record.map_logic_support_state
            == hl::game_api::MapLogicSupportState::kDetectedButDeferred;
}

std::string FormatVector(const Vector& value)
{
    std::ostringstream stream;
    stream << value.x << ' ' << value.y << ' ' << value.z;
    return stream.str();
}

std::string FormatScalar(float value)
{
    std::ostringstream stream;
    stream << std::setprecision(9) << value;
    return stream.str();
}

float ComputeSignedAngleDeltaDegrees(float current, float target)
{
    float delta = NormalizeAngleDegrees(target) - NormalizeAngleDegrees(current);
    if (delta >= 180.0f)
    {
        delta -= 360.0f;
    }
    else if (delta < -180.0f)
    {
        delta += 360.0f;
    }

    return std::fabs(delta) < 0.0001f ? 0.0f : delta;
}

void AppendRecordNote(RuntimeEntityRecord& record, std::string_view note)
{
    if (note.empty())
    {
        return;
    }

    if (!record.note.empty())
    {
        record.note += "; ";
    }

    record.note += note;
}

std::string SafeClassname(const hl::game_api::detail::EntityDefinition& entity)
{
    return entity.classname.empty() ? "<missing>" : entity.classname;
}

std::string BuildParsedEntitySummary(const hl::game_api::detail::EntityDefinition& entity)
{
    return "#" + std::to_string(entity.ordinal)
        + " classname=" + SafeClassname(entity)
        + ", keys=" + std::to_string(entity.key_values.size())
        + ", preview="
        + (entity.source_preview.empty() ? std::string("<empty>") : entity.source_preview);
}

std::string BuildRuntimeRecordSummary(const RuntimeEntityRecord& record)
{
    std::string summary = "#" + std::to_string(record.parse_index)
        + " classname=" + (record.classname.empty() ? std::string("<missing>") : record.classname);

    if (record.edict_index >= 0)
    {
        summary += " -> edict#" + std::to_string(record.edict_index);
    }

    if (!record.targetname.empty())
    {
        summary += " targetname=" + record.targetname;
    }

    if (!record.target.empty())
    {
        summary += " target=" + record.target;
    }

    if (record.has_origin)
    {
        summary += " origin=" + (record.origin_raw.empty() ? FormatVector(record.origin) : record.origin_raw);
    }

    if (!record.model.empty())
    {
        summary += " model=" + record.model;
        summary += " modelindex=" + std::to_string(record.modelindex);
    }

    summary += " spawned=" + std::string(record.spawned ? "yes" : "no");
    summary += " removed=" + std::string(record.removed ? "yes" : "no");
    summary += " deferred=" + std::string(record.deferred ? "yes" : "no");
    summary += " private=" + std::string(record.has_private_data ? "yes" : "no");
    summary += " activate=" + std::string(record.activation_candidate ? "yes" : "no");
    summary += " support=" + std::string(RuntimeEntitySupportStateLabel(record.support_state));
    summary += " lifecycle=" + std::string(RuntimeEntityLifecycleStateLabel(record.lifecycle_state));
    summary += " nextthink=" + std::to_string(record.nextthink);
    summary += " ltime=" + std::to_string(record.ltime);
    summary += " scheduled=" + std::string(record.scheduled_for_think ? "yes" : "no");
    summary += " think_attempted=" + std::string(record.think_attempted ? "yes" : "no");
    summary += " think_succeeded=" + std::string(record.think_succeeded ? "yes" : "no");
    summary += " last_think_frame=" + std::to_string(record.last_think_frame);
    summary += " can_think=" + std::string(record.can_think ? "yes" : "no");
    summary += " can_use=" + std::string(record.can_receive_use ? "yes" : "no");
    summary += " emits_targets=" + std::string(record.can_emit_targets ? "yes" : "no");
    summary += " pending_outputs=" + std::to_string(record.pending_scheduled_outputs);
    summary += " triggered_this_frame=" + std::string(record.triggered_this_frame ? "yes" : "no");
    summary += " blocked=" + std::string(record.blocked_or_deferred ? "yes" : "no");
    summary += " maplogic=" + std::string(
        hl::game_api::detail::MapLogicSupportStateLabel(record.map_logic_support_state));
    summary += " target_resolutions=" + std::to_string(record.target_resolutions);
    summary += " use_attempts=" + std::to_string(record.use_attempts);
    summary += " use_successes=" + std::to_string(record.use_successes);
    summary += " use_deferred=" + std::to_string(record.use_deferred);
    summary += " use_failures=" + std::to_string(record.use_failures);
    summary += " last_triggered_frame=" + std::to_string(record.last_triggered_frame);
    if (hl::game_api::detail::IsRelevantScriptedLogicClass(record.classname))
    {
        summary += " scripted_support=" + std::string(
            hl::game_api::detail::ScriptedLogicSupportStateLabel(record.scripted_logic.support_state));
        summary += " scripted_use=" + std::to_string(record.scripted_logic.received_use_count);
        summary += " scripted_emit=" + std::to_string(record.scripted_logic.emitted_target_count);
        summary += " scripted_sched=" + std::to_string(record.scripted_logic.scheduled_output_count);
        summary += " scripted_progressed=" + std::string(
            record.scripted_logic.progressed_this_run ? "yes" : "no");
        summary += " scripted_changed=" + std::string(
            record.scripted_logic.internal_state_changed ? "yes" : "no");
        summary += " scripted_followup=" + std::string(
            record.scripted_logic.scheduled_follow_up ? "yes" : "no");
        if (!record.scripted_actor_name.empty())
        {
            summary += " actor=" + record.scripted_actor_name;
        }
        if (!record.scripted_actor_classname.empty())
        {
            summary += " actor_class=" + record.scripted_actor_classname;
        }
        if (record.scripted_move_to != 0)
        {
            summary += " move_to=" + std::to_string(record.scripted_move_to);
        }
        if (!record.scripted_path_links.empty())
        {
            summary += " path_links=" + std::to_string(record.scripted_path_links.size());
        }
        if (!record.scripted_logic.blocked_reason.empty())
        {
            summary += " scripted_blocked=" + record.scripted_logic.blocked_reason;
        }
    }
    summary += " flags=" + std::to_string(record.flags);
    summary += " solid=" + std::to_string(record.solid);
    summary += " movetype=" + std::to_string(record.movetype);
    summary += " effects=" + std::to_string(record.effects);
    summary += " movedir="
        + (record.has_movedir ? FormatVector(record.movedir) : std::string("<none>"));
    summary += " size="
        + (record.has_size
            ? (FormatVector(record.mins) + ".." + FormatVector(record.maxs))
            : std::string("<none>"));
    summary += " health="
        + (record.health_available ? std::to_string(record.health) : std::string("<unavailable>"));

    if (!record.note.empty())
    {
        summary += " note=" + record.note;
    }

    return summary;
}

std::string BuildGlobalsSnapshotText(const EngineShimState& state)
{
    return "mapname="
        + (state.string_pool.Describe(state.globalvars.mapname).empty()
            ? std::string("<empty>")
            : state.string_pool.Describe(state.globalvars.mapname))
        + ", startspot="
        + (state.string_pool.Describe(state.globalvars.startspot).empty()
            ? std::string("<empty>")
            : state.string_pool.Describe(state.globalvars.startspot))
        + ", time=" + std::to_string(state.globalvars.time)
        + ", frametime=" + std::to_string(state.globalvars.frametime)
        + ", deathmatch=" + std::to_string(state.globalvars.deathmatch)
        + ", coop=" + std::to_string(state.globalvars.coop)
        + ", maxClients=" + std::to_string(state.globalvars.maxClients)
        + ", maxEntities=" + std::to_string(state.globalvars.maxEntities);
}

std::string BuildServerFlagsSnapshotText(const EngineShimState& state)
{
    return "active=" + std::string(BoolToYesNo(state.server_state.active))
        + ", loading=" + BoolToYesNo(state.server_state.loading)
        + ", initialized=" + BoolToYesNo(state.server_state.initialized)
        + ", frame=" + std::to_string(state.server_state.frame_count)
        + ", server_frame=" + std::to_string(state.server_state.server_frame);
}

std::string BuildEntitySnapshotSummary(const hl::game_api::detail::EntityStateSnapshot& snapshot)
{
    std::string summary = "edict#" + std::to_string(snapshot.index)
        + " classname="
        + (snapshot.classname.empty() ? std::string("<empty>") : snapshot.classname);

    if (!snapshot.targetname.empty())
    {
        summary += " targetname=" + snapshot.targetname;
    }

    if (snapshot.has_origin)
    {
        summary += " origin="
            + (snapshot.origin_string.empty() ? FormatVector(snapshot.origin) : snapshot.origin_string);
    }

    if (!snapshot.model_string.empty())
    {
        summary += " model=" + snapshot.model_string;
    }

    summary += " modelindex=" + std::to_string(snapshot.model_index);
    summary += " spawned=" + std::string(snapshot.spawned ? "yes" : "no");
    summary += " removed=" + std::string(snapshot.removed ? "yes" : "no");
    summary += " deferred=" + std::string(snapshot.deferred ? "yes" : "no");
    summary += " private=" + std::string(snapshot.private_data_present ? "yes" : "no");
    summary += " activate=" + std::string(snapshot.activation_candidate ? "yes" : "no");
    return summary;
}

std::vector<hl::game_api::EntityClassSupportSummary> BuildClassSupportSummary(
    const std::vector<RuntimeEntityRecord>& records)
{
    std::unordered_map<std::string, hl::game_api::EntityClassSupportSummary> summary_by_classname;
    for (const RuntimeEntityRecord& record : records)
    {
        const std::string classname = record.classname.empty() ? "<missing>" : record.classname;
        auto [it, inserted] = summary_by_classname.try_emplace(
            classname,
            hl::game_api::EntityClassSupportSummary{classname});
        hl::game_api::EntityClassSupportSummary& summary = it->second;

        switch (record.support_state)
        {
        case RuntimeEntitySupportState::kSpawnedSuccessfully:
            ++summary.spawned_successfully;
            break;
        case RuntimeEntitySupportState::kRemovedDuringSpawn:
            ++summary.removed_during_spawn;
            break;
        case RuntimeEntitySupportState::kDeferredUnsupported:
            ++summary.deferred_unsupported;
            break;
        case RuntimeEntitySupportState::kFailedDuringSpawn:
            ++summary.failed_during_spawn;
            break;
        default:
            break;
        }
    }

    std::vector<hl::game_api::EntityClassSupportSummary> summaries;
    summaries.reserve(summary_by_classname.size());
    for (auto& [classname, summary] : summary_by_classname)
    {
        summaries.push_back(std::move(summary));
    }

    std::sort(
        summaries.begin(),
        summaries.end(),
        [](const hl::game_api::EntityClassSupportSummary& left,
           const hl::game_api::EntityClassSupportSummary& right)
        {
            const std::size_t left_total =
                left.spawned_successfully
                + left.removed_during_spawn
                + left.deferred_unsupported
                + left.failed_during_spawn;
            const std::size_t right_total =
                right.spawned_successfully
                + right.removed_during_spawn
                + right.deferred_unsupported
                + right.failed_during_spawn;
            if (left_total != right_total)
            {
                return left_total > right_total;
            }

            return left.classname < right.classname;
        });

    return summaries;
}

int MapLogicSupportRank(hl::game_api::MapLogicSupportState state)
{
    switch (state)
    {
    case hl::game_api::MapLogicSupportState::kUseSupported:
        return 3;
    case hl::game_api::MapLogicSupportState::kPassiveRecipientOnly:
        return 2;
    case hl::game_api::MapLogicSupportState::kDetectedButDeferred:
        return 1;
    case hl::game_api::MapLogicSupportState::kRemovedByGameLogic:
    default:
        return 0;
    }
}

std::vector<hl::game_api::MapLogicClassSummary> BuildMapLogicClassSummary(
    const std::vector<RuntimeEntityRecord>& records)
{
    std::unordered_map<std::string, hl::game_api::MapLogicClassSummary> by_classname;

    for (const RuntimeEntityRecord& record : records)
    {
        const std::string classname = record.classname.empty() ? "<empty>" : record.classname;
        hl::game_api::MapLogicClassSummary& summary = by_classname[classname];
        summary.classname = classname;
        if (summary.active_entities == 0
            && summary.can_think == 0
            && summary.can_receive_use == 0
            && summary.can_emit_targets == 0
            && summary.pending_scheduled_outputs == 0
            && summary.triggered_this_frame == 0
            && summary.blocked_or_deferred == 0
            && summary.target_resolutions == 0
            && summary.use_attempts == 0
            && summary.use_successes == 0
            && summary.use_deferred == 0
            && summary.use_failures == 0)
        {
            summary.support_state = record.map_logic_support_state;
        }
        else if (MapLogicSupportRank(record.map_logic_support_state)
            > MapLogicSupportRank(summary.support_state))
        {
            summary.support_state = record.map_logic_support_state;
        }

        if (record.in_use && !record.removed)
        {
            ++summary.active_entities;
        }
        if (record.can_think)
        {
            ++summary.can_think;
        }
        if (record.can_receive_use)
        {
            ++summary.can_receive_use;
        }
        if (record.can_emit_targets)
        {
            ++summary.can_emit_targets;
        }
        summary.pending_scheduled_outputs += static_cast<std::size_t>(record.pending_scheduled_outputs);
        if (record.triggered_this_frame)
        {
            ++summary.triggered_this_frame;
        }
        if (record.blocked_or_deferred)
        {
            ++summary.blocked_or_deferred;
        }
        summary.target_resolutions += record.target_resolutions;
        summary.use_attempts += record.use_attempts;
        summary.use_successes += record.use_successes;
        summary.use_deferred += record.use_deferred;
        summary.use_failures += record.use_failures;
    }

    std::vector<hl::game_api::MapLogicClassSummary> summaries;
    summaries.reserve(by_classname.size());
    for (auto& [classname, summary] : by_classname)
    {
        summaries.push_back(std::move(summary));
    }

    std::sort(
        summaries.begin(),
        summaries.end(),
        [](const hl::game_api::MapLogicClassSummary& left,
           const hl::game_api::MapLogicClassSummary& right)
        {
            const std::size_t left_total =
                left.use_attempts + left.target_resolutions + left.active_entities;
            const std::size_t right_total =
                right.use_attempts + right.target_resolutions + right.active_entities;
            if (left_total != right_total)
            {
                return left_total > right_total;
            }

            return left.classname < right.classname;
        });

    return summaries;
}

struct PathTrackNodeDiagnostic
{
    std::size_t parse_index = 0;
    int edict_index = -1;
    std::string targetname;
    std::string next_target;
    std::string message_target;
    float speed = 0.0f;
    bool next_resolved = false;
    bool message_resolved = false;
};

struct ScriptedPathGraph
{
    std::vector<PathTrackNodeDiagnostic> nodes;
    std::unordered_map<std::string, std::vector<std::string>> message_to_track_names;
    std::unordered_map<std::string, std::vector<std::string>> track_to_previous_names;
};

const RuntimeEntityRecord* FindRuntimeRecordByTargetname(
    const EngineShimState& state,
    std::string_view targetname);
RuntimeEntityRecord* FindRuntimeRecordByEdictIndex(
    EngineShimState& state,
    int edict_index);
ScriptedPathGraph BuildScriptedPathGraph(const EngineShimState& state);

int ScriptedSupportRank(hl::game_api::ScriptedLogicSupportState state)
{
    switch (state)
    {
    case hl::game_api::ScriptedLogicSupportState::kScriptedProgressing:
        return 7;
    case hl::game_api::ScriptedLogicSupportState::kScheduledUseSupported:
        return 6;
    case hl::game_api::ScriptedLogicSupportState::kUseSupported:
        return 5;
    case hl::game_api::ScriptedLogicSupportState::kBlockedOnMovement:
        return 4;
    case hl::game_api::ScriptedLogicSupportState::kBlockedOnActor:
        return 3;
    case hl::game_api::ScriptedLogicSupportState::kBlockedOnEngineCallback:
        return 2;
    case hl::game_api::ScriptedLogicSupportState::kPassiveRecipientOnly:
        return 1;
    case hl::game_api::ScriptedLogicSupportState::kRemovedByGameLogic:
    default:
        return 0;
    }
}

std::vector<std::string> CollectScriptedPathLinks(
    const RuntimeEntityRecord& record,
    const ScriptedPathGraph& graph)
{
    std::vector<std::string> links;
    auto append_for_target =
        [&](std::string_view key)
        {
            if (key.empty())
            {
                return;
            }

            const auto it = graph.message_to_track_names.find(std::string(key));
            if (it == graph.message_to_track_names.end())
            {
                return;
            }

            for (const std::string& track_name : it->second)
            {
                if (std::find(links.begin(), links.end(), track_name) == links.end())
                {
                    links.push_back(track_name);
                }
            }
        };

    append_for_target(record.targetname);
    append_for_target(record.scripted_actor_name);
    append_for_target(record.target);
    return links;
}

void RefreshScriptedDiagnostics(EngineShimState& state)
{
    const ScriptedPathGraph graph = BuildScriptedPathGraph(state);

    for (RuntimeEntityRecord& record : state.entity_bootstrap.runtime_entities)
    {
        if (!hl::game_api::detail::IsRelevantScriptedLogicClass(record.classname))
        {
            continue;
        }

        const hl::game_api::ScriptedLogicSupportState previous_support_state =
            record.scripted_logic.support_state;
        const std::string previous_blocked_reason = record.scripted_logic.blocked_reason;

        record.scripted_path_links.clear();
        record.scripted_actor_classname.clear();
        record.scripted_blocked_reason.clear();
        record.scripted_logic.actor_exists = false;
        record.scripted_logic.path_linked = false;
        record.scripted_logic.blocked_reason.clear();

        const std::string normalized = ToLowerCopy(record.classname);
        if (record.removed || !record.in_use || (record.flags & FL_KILLME) != 0)
        {
            record.scripted_logic.support_state =
                hl::game_api::ScriptedLogicSupportState::kRemovedByGameLogic;
            record.scripted_blocked_reason = "removed by game logic";
            continue;
        }

        if (normalized == "scripted_sequence")
        {
            const RuntimeEntityRecord* actor = nullptr;
            if (record.scripted_actor_resolved && record.scripted_actor_edict >= 0)
            {
                actor = FindRuntimeRecordByEdictIndex(
                    const_cast<EngineShimState&>(state),
                    record.scripted_actor_edict);
            }
            if (actor == nullptr)
            {
                actor = FindRuntimeRecordByTargetname(state, record.scripted_actor_name);
            }
            if ((record.scripted_actor_resolved || (actor != nullptr && actor->in_use && !actor->removed)))
            {
                record.scripted_logic.actor_exists = true;
                if (actor != nullptr)
                {
                    record.scripted_actor_classname = actor->classname;
                }
            }

            record.scripted_path_links = CollectScriptedPathLinks(record, graph);
            record.scripted_logic.path_linked = !record.scripted_path_links.empty();

            const bool movement_required = record.scripted_move_to != 0;
            const bool use_received =
                record.scripted_logic.received_use_count > 0 || record.use_successes > 0;
            const bool changed_state =
                record.scripted_logic.internal_state_changed || record.scheduled_for_think;
            const bool follow_up =
                record.scripted_logic.scheduled_follow_up
                || record.pending_scheduled_outputs > 0
                || record.scheduled_for_think;
            const bool progressing =
                record.scripted_logic.emitted_targets || record.scripted_logic.scheduled_output_count > 0;
            const bool movement_active =
                EqualsIgnoreCase(record.scripted_movement_stage, "moving")
                || EqualsIgnoreCase(record.scripted_movement_stage, "move-pending")
                || EqualsIgnoreCase(record.scripted_movement_stage, "actor-resolved");
            const bool movement_ready =
                record.scripted_arrived
                || record.scripted_animation_ready
                || EqualsIgnoreCase(record.scripted_movement_stage, "completed");

            if (!record.scripted_actor_name.empty() && !record.scripted_logic.actor_exists)
            {
                record.scripted_logic.support_state =
                    hl::game_api::ScriptedLogicSupportState::kBlockedOnActor;
                record.scripted_blocked_reason = !record.scripted_blocked_reason.empty()
                    ? record.scripted_blocked_reason
                    : "actor '" + record.scripted_actor_name + "' not resolved";
            }
            else if (movement_required && movement_ready)
            {
                record.scripted_logic.support_state =
                    hl::game_api::ScriptedLogicSupportState::kScriptedProgressing;
                record.scripted_blocked_reason.clear();
                record.scripted_logic.blocked_reason.clear();
            }
            else if (movement_required && movement_active)
            {
                record.scripted_logic.support_state =
                    hl::game_api::ScriptedLogicSupportState::kScriptedProgressing;
                record.scripted_blocked_reason.clear();
                record.scripted_logic.blocked_reason.clear();
            }
            else if (movement_required && (use_received || changed_state || follow_up))
            {
                record.scripted_logic.support_state =
                    hl::game_api::ScriptedLogicSupportState::kBlockedOnMovement;
                record.scripted_blocked_reason = !record.scripted_blocked_reason.empty()
                    ? record.scripted_blocked_reason
                    : "m_fMoveTo=" + std::to_string(record.scripted_move_to)
                        + " requires movement/path support";
            }
            else if (progressing)
            {
                record.scripted_logic.support_state =
                    hl::game_api::ScriptedLogicSupportState::kScriptedProgressing;
            }
            else if (follow_up || changed_state)
            {
                record.scripted_logic.support_state =
                    hl::game_api::ScriptedLogicSupportState::kScheduledUseSupported;
            }
            else if (use_received)
            {
                record.scripted_logic.support_state =
                    hl::game_api::ScriptedLogicSupportState::kUseSupported;
            }
            else
            {
                record.scripted_logic.support_state =
                    hl::game_api::ScriptedLogicSupportState::kUseSupported;
                if (!movement_required)
                {
                    record.scripted_blocked_reason.clear();
                }
            }
        }
        else if (normalized == "env_message" || normalized == "ambient_generic")
        {
            record.scripted_logic.support_state =
                hl::game_api::ScriptedLogicSupportState::kUseSupported;
        }
        else
        {
            record.scripted_logic.support_state =
                hl::game_api::ScriptedLogicSupportState::kPassiveRecipientOnly;
        }

        if (previous_support_state == hl::game_api::ScriptedLogicSupportState::kBlockedOnEngineCallback
            && !previous_blocked_reason.empty()
            && record.scripted_logic.support_state
                != hl::game_api::ScriptedLogicSupportState::kBlockedOnActor
            && record.scripted_logic.support_state
                != hl::game_api::ScriptedLogicSupportState::kBlockedOnMovement
            && record.scripted_logic.support_state
                != hl::game_api::ScriptedLogicSupportState::kRemovedByGameLogic
            && !record.scripted_logic.progressed_this_run)
        {
            record.scripted_logic.support_state =
                hl::game_api::ScriptedLogicSupportState::kBlockedOnEngineCallback;
            record.scripted_blocked_reason = previous_blocked_reason;
        }

        if (record.scripted_logic.support_state
                != hl::game_api::ScriptedLogicSupportState::kBlockedOnActor
            && record.scripted_logic.support_state
                != hl::game_api::ScriptedLogicSupportState::kBlockedOnMovement
            && record.scripted_logic.support_state
                != hl::game_api::ScriptedLogicSupportState::kBlockedOnEngineCallback
            && record.scripted_logic.blocked_reason.empty())
        {
            record.scripted_blocked_reason = record.scripted_logic.blocked_reason;
        }
        else if (!record.scripted_blocked_reason.empty())
        {
            record.scripted_logic.blocked_reason = record.scripted_blocked_reason;
        }
    }
}

hl::game_api::ScriptedLogicEntitySummary BuildScriptedLogicEntitySummary(
    const RuntimeEntityRecord& record)
{
    hl::game_api::ScriptedLogicEntitySummary summary;
    summary.edict_index = record.edict_index;
    summary.parse_index = record.parse_index;
    summary.classname = record.classname;
    summary.targetname = record.targetname;
    summary.target = record.target;
    summary.actor_name = record.scripted_actor_name;
    summary.actor_classname = record.scripted_actor_classname;
    summary.play = record.scripted_play;
    summary.idle = record.scripted_idle;
    summary.move_to = record.scripted_move_to;
    summary.radius = record.scripted_radius;
    summary.spawnflags = record.scripted_spawnflags;
    summary.received_use_count = record.scripted_logic.received_use_count;
    summary.emitted_target_count = record.scripted_logic.emitted_target_count;
    summary.scheduled_output_count = record.scripted_logic.scheduled_output_count;
    summary.last_trigger_frame = record.scripted_logic.last_trigger_frame;
    summary.last_trigger_time = record.scripted_logic.last_trigger_time;
    summary.last_source_entity = record.scripted_logic.last_source_entity;
    summary.blocked_reason = record.scripted_logic.blocked_reason;
    summary.support_state = record.scripted_logic.support_state;
    summary.progressed_this_frame = record.scripted_logic.progressed_this_frame;
    summary.progressed_this_run = record.scripted_logic.progressed_this_run;
    summary.internal_state_changed = record.scripted_logic.internal_state_changed;
    summary.scheduled_follow_up = record.scripted_logic.scheduled_follow_up;
    summary.emitted_targets = record.scripted_logic.emitted_targets;
    summary.actor_exists = record.scripted_logic.actor_exists;
    summary.path_linked = record.scripted_logic.path_linked;
    summary.path_links_preview = record.scripted_path_links;
    return summary;
}

std::vector<hl::game_api::ScriptedLogicClassSummary> BuildScriptedLogicClassSummary(
    const std::vector<RuntimeEntityRecord>& records)
{
    std::unordered_map<std::string, hl::game_api::ScriptedLogicClassSummary> by_classname;

    for (const RuntimeEntityRecord& record : records)
    {
        if (!hl::game_api::detail::IsRelevantScriptedLogicClass(record.classname))
        {
            continue;
        }

        const std::string key = record.classname.empty() ? std::string("<empty>") : record.classname;
        hl::game_api::ScriptedLogicClassSummary& summary = by_classname[key];
        summary.classname = key;
        ++summary.entity_count;
        summary.received_use_count += static_cast<std::size_t>(record.scripted_logic.received_use_count);
        summary.emitted_target_count += static_cast<std::size_t>(record.scripted_logic.emitted_target_count);
        summary.scheduled_output_count += static_cast<std::size_t>(record.scripted_logic.scheduled_output_count);
        if (record.scripted_logic.progressed_this_run)
        {
            ++summary.progressing_count;
        }
        if (!record.scripted_logic.blocked_reason.empty())
        {
            ++summary.blocked_count;
        }
        if (ScriptedSupportRank(record.scripted_logic.support_state)
            > ScriptedSupportRank(summary.support_state))
        {
            summary.support_state = record.scripted_logic.support_state;
        }
    }

    std::vector<hl::game_api::ScriptedLogicClassSummary> summaries;
    summaries.reserve(by_classname.size());
    for (auto& [classname, summary] : by_classname)
    {
        (void)classname;
        summaries.push_back(std::move(summary));
    }

    std::sort(
        summaries.begin(),
        summaries.end(),
        [](const hl::game_api::ScriptedLogicClassSummary& left,
           const hl::game_api::ScriptedLogicClassSummary& right)
        {
            const std::size_t left_weight =
                left.progressing_count + left.received_use_count + left.entity_count;
            const std::size_t right_weight =
                right.progressing_count + right.received_use_count + right.entity_count;
            if (left_weight != right_weight)
            {
                return left_weight > right_weight;
            }

            return left.classname < right.classname;
        });
    return summaries;
}

std::vector<hl::game_api::NamedCountSummary> BuildScriptedBlockedReasonSummary(
    const std::vector<RuntimeEntityRecord>& records)
{
    std::unordered_map<std::string, std::size_t> counts;
    for (const RuntimeEntityRecord& record : records)
    {
        if (!hl::game_api::detail::IsRelevantScriptedLogicClass(record.classname)
            || record.scripted_logic.blocked_reason.empty())
        {
            continue;
        }

        ++counts[record.scripted_logic.blocked_reason];
    }

    std::vector<hl::game_api::NamedCountSummary> summary;
    summary.reserve(counts.size());
    for (const auto& [name, count] : counts)
    {
        summary.push_back({name, count});
    }

    std::sort(
        summary.begin(),
        summary.end(),
        [](const hl::game_api::NamedCountSummary& left,
           const hl::game_api::NamedCountSummary& right)
        {
            if (left.count != right.count)
            {
                return left.count > right.count;
            }

            return left.name < right.name;
        });
    return summary;
}

hl::game_api::ScriptedSequenceProgressSummary BuildScriptedSequenceProgressSummary(
    const std::vector<RuntimeEntityRecord>& records)
{
    hl::game_api::ScriptedSequenceProgressSummary summary;
    for (const RuntimeEntityRecord& record : records)
    {
        if (!EqualsIgnoreCase(record.classname, "scripted_sequence"))
        {
            continue;
        }

        ++summary.total;
        if (record.scripted_logic.received_use_count > 0)
        {
            ++summary.received_use;
        }
        if (record.scripted_logic.internal_state_changed)
        {
            ++summary.changed_state;
        }
        if (record.scripted_logic.scheduled_follow_up)
        {
            ++summary.scheduled_follow_up;
        }
        if (record.scripted_logic.emitted_targets)
        {
            ++summary.emitted_targets;
        }
        if (record.scripted_logic.progressed_this_run)
        {
            ++summary.progressing;
        }

        switch (record.scripted_logic.support_state)
        {
        case hl::game_api::ScriptedLogicSupportState::kBlockedOnMovement:
            ++summary.blocked_on_movement;
            break;
        case hl::game_api::ScriptedLogicSupportState::kBlockedOnActor:
            ++summary.blocked_on_actor;
            break;
        case hl::game_api::ScriptedLogicSupportState::kBlockedOnEngineCallback:
            ++summary.blocked_on_engine_callback;
            break;
        default:
            break;
        }
    }

    return summary;
}

hl::game_api::PathTrackResolutionSummary BuildPathTrackResolutionSummary(
    const ScriptedPathGraph& graph,
    const std::vector<RuntimeEntityRecord>& records)
{
    hl::game_api::PathTrackResolutionSummary summary;
    summary.total_nodes = graph.nodes.size();

    std::unordered_map<std::string, bool> actors_with_path_links;
    for (const PathTrackNodeDiagnostic& node : graph.nodes)
    {
        if (!node.next_target.empty())
        {
            if (node.next_resolved)
            {
                ++summary.resolved_next_links;
            }
            else
            {
                ++summary.unresolved_next_links;
            }
        }

        if (!node.message_target.empty())
        {
            ++summary.message_links;
            if (node.message_resolved)
            {
                ++summary.resolved_message_targets;
            }
            else
            {
                ++summary.unresolved_message_targets;
            }
        }

        if (summary.preview.size() < 8)
        {
            summary.preview.push_back(
                "path_track " + (node.targetname.empty() ? std::string("<empty>") : node.targetname)
                + " -> "
                + (node.next_target.empty() ? std::string("<none>") : node.next_target)
                + (node.message_target.empty()
                    ? std::string()
                    : " message=" + node.message_target));
        }
    }

    for (const RuntimeEntityRecord& record : records)
    {
        if (EqualsIgnoreCase(record.classname, "scripted_sequence") && !record.scripted_path_links.empty())
        {
            ++summary.sequences_with_path_links;
            if (!record.scripted_actor_name.empty())
            {
                actors_with_path_links[record.scripted_actor_name] = record.scripted_logic.actor_exists;
            }
        }
    }

    for (const auto& [actor_name, actor_exists] : actors_with_path_links)
    {
        (void)actor_name;
        if (actor_exists)
        {
            ++summary.actors_with_path_links;
        }
    }

    return summary;
}

std::vector<hl::game_api::ScriptedLogicEntitySummary> BuildScriptedEntityPreview(
    const std::vector<RuntimeEntityRecord>& records,
    bool blocked,
    std::size_t limit)
{
    std::vector<hl::game_api::ScriptedLogicEntitySummary> preview;
    for (const RuntimeEntityRecord& record : records)
    {
        if (!hl::game_api::detail::IsRelevantScriptedLogicClass(record.classname))
        {
            continue;
        }

        const bool is_blocked = !record.scripted_logic.blocked_reason.empty();
        if (blocked != is_blocked)
        {
            continue;
        }

        const bool is_progressing =
            record.scripted_logic.progressed_this_run
            || record.scripted_logic.internal_state_changed
            || record.scripted_logic.scheduled_follow_up
            || record.scripted_logic.emitted_targets;
        if (!blocked && !is_progressing)
        {
            continue;
        }

        preview.push_back(BuildScriptedLogicEntitySummary(record));
        if (preview.size() >= limit)
        {
            break;
        }
    }

    return preview;
}

std::string BuildScriptedProgressPreview(const RuntimeEntityRecord& record)
{
    return "edict#" + std::to_string(record.edict_index)
        + " classname=" + (record.classname.empty() ? std::string("<empty>") : record.classname)
        + " targetname=" + (record.targetname.empty() ? std::string("<empty>") : record.targetname)
        + " support="
        + hl::game_api::detail::ScriptedLogicSupportStateLabel(record.scripted_logic.support_state)
        + (record.scripted_logic.blocked_reason.empty()
            ? std::string()
            : " blocked=" + record.scripted_logic.blocked_reason);
}

hl::game_api::ScriptedLogicFrameStateSummary BuildScriptedLogicFrameStateSummary(
    const EngineShimState& state)
{
    hl::game_api::ScriptedLogicFrameStateSummary summary;
    if (state.map_logic_dispatcher_state.frames.empty())
    {
        return summary;
    }

    const hl::game_api::MapLogicFrameStateSummary& dispatcher_frame =
        state.map_logic_dispatcher_state.frames.back();
    summary.frame_number = dispatcher_frame.frame_number;
    summary.host_frame_index = dispatcher_frame.host_frame_index;
    summary.server_frame_index = dispatcher_frame.server_frame_index;
    summary.time = dispatcher_frame.time;
    summary.frametime = dispatcher_frame.frametime;
    summary.queue_size_before = dispatcher_frame.queue_size_before;
    summary.queue_size_after = dispatcher_frame.queue_size_after;
    summary.due_delayed_actions = dispatcher_frame.scheduled_due;
    summary.executed_delayed_actions = dispatcher_frame.scheduled_executed;
    summary.rescheduled_delayed_actions = dispatcher_frame.scheduled_rescheduled;
    summary.skipped_delayed_actions = dispatcher_frame.scheduled_skipped;
    summary.failed_delayed_actions = dispatcher_frame.scheduled_failed;
    summary.deferred_delayed_actions = dispatcher_frame.scheduled_deferred;
    summary.target_chains_fired = dispatcher_frame.target_chains_fired;
    summary.target_resolutions = dispatcher_frame.target_resolutions;
    summary.use_attempts = dispatcher_frame.use_attempts;
    summary.use_successes = dispatcher_frame.use_successes;
    summary.use_deferred = dispatcher_frame.use_deferred;
    summary.use_failures = dispatcher_frame.use_failures;
    summary.long_delay_pending = dispatcher_frame.long_delay_pending;
    summary.long_delay_executed = dispatcher_frame.long_delay_executed;

    std::unordered_map<std::string, std::size_t> blocked_counts;
    for (const RuntimeEntityRecord& record : state.entity_bootstrap.runtime_entities)
    {
        if (!hl::game_api::detail::IsRelevantScriptedLogicClass(record.classname))
        {
            continue;
        }

        if (record.scripted_logic.progressed_this_frame)
        {
            ++summary.newly_progressed_scripted_entities;
            if (summary.progressing_preview.size() < 6)
            {
                summary.progressing_preview.push_back(BuildScriptedProgressPreview(record));
            }
        }

        if (!record.scripted_logic.blocked_reason.empty())
        {
            ++summary.blocked_scripted_entities;
            ++blocked_counts[record.scripted_logic.blocked_reason];
        }
    }

    for (const auto& [name, count] : blocked_counts)
    {
        summary.blocked_by_reason.push_back({name, count});
    }
    std::sort(
        summary.blocked_by_reason.begin(),
        summary.blocked_by_reason.end(),
        [](const hl::game_api::NamedCountSummary& left,
           const hl::game_api::NamedCountSummary& right)
        {
            if (left.count != right.count)
            {
                return left.count > right.count;
            }

            return left.name < right.name;
        });
    for (const std::string& line : dispatcher_frame.scheduled_action_preview)
    {
        if (summary.pending_delay_preview.size() >= 6)
        {
            break;
        }

        summary.pending_delay_preview.push_back(line);
    }

    return summary;
}

std::string BuildScriptedLogicReadiness(const hl::game_api::ScriptedLogicStateSummary& summary)
{
    if (summary.frames_completed == 0)
    {
        return "scripted logic tracker is waiting for post-StartFrame frames";
    }

    if (summary.scripted_sequence_summary.progressing > 0)
    {
        return "scripted logic is progressing; movement-dependent scene support is the next step";
    }

    if (summary.scripted_sequence_summary.scheduled_follow_up > 0)
    {
        return "scripted logic advances through use and delayed scheduling; unblock movement-driven follow-up next";
    }

    if (!summary.blocked_reasons.empty())
    {
        return "scripted logic is diagnosed; resolve the top blocked reason before broader AI simulation";
    }

    return "scripted logic diagnostics are stable; extend actor movement support next";
}

void RefreshScriptedLogicStateSummary(EngineShimState& state)
{
    RefreshScriptedDiagnostics(state);
    const ScriptedPathGraph graph = BuildScriptedPathGraph(state);

    state.scripted_logic_state.configured = true;
    state.scripted_logic_state.trace_scripted = state.frame_bootstrap_options.trace_scripted;
    state.scripted_logic_state.frames_attempted = state.map_logic_dispatcher_state.frames_attempted;
    state.scripted_logic_state.frames_completed = state.map_logic_dispatcher_state.frames_completed;
    state.scripted_logic_state.classname_summary =
        BuildScriptedLogicClassSummary(state.entity_bootstrap.runtime_entities);
    state.scripted_logic_state.blocked_reasons =
        BuildScriptedBlockedReasonSummary(state.entity_bootstrap.runtime_entities);
    state.scripted_logic_state.scripted_sequence_summary =
        BuildScriptedSequenceProgressSummary(state.entity_bootstrap.runtime_entities);
    state.scripted_logic_state.path_track_summary =
        BuildPathTrackResolutionSummary(graph, state.entity_bootstrap.runtime_entities);
    state.scripted_logic_state.progressing_entities_preview =
        BuildScriptedEntityPreview(state.entity_bootstrap.runtime_entities, false, 6);
    state.scripted_logic_state.blocked_entities_preview =
        BuildScriptedEntityPreview(state.entity_bootstrap.runtime_entities, true, 6);

    int total_received_use = 0;
    int total_emitted_targets = 0;
    int total_scheduled_outputs = 0;
    int total_internal_state_changes = 0;
    int total_scheduled_follow_ups = 0;
    std::size_t total_progressed_entities = 0;
    for (const RuntimeEntityRecord& record : state.entity_bootstrap.runtime_entities)
    {
        if (!hl::game_api::detail::IsRelevantScriptedLogicClass(record.classname))
        {
            continue;
        }

        total_received_use += record.scripted_logic.received_use_count;
        total_emitted_targets += record.scripted_logic.emitted_target_count;
        total_scheduled_outputs += record.scripted_logic.scheduled_output_count;
        total_internal_state_changes += record.scripted_logic.internal_state_changed ? 1 : 0;
        total_scheduled_follow_ups += record.scripted_logic.scheduled_follow_up ? 1 : 0;
        total_progressed_entities += record.scripted_logic.progressed_this_run ? 1u : 0u;
    }

    state.scripted_logic_state.total_received_use = total_received_use;
    state.scripted_logic_state.total_emitted_targets = total_emitted_targets;
    state.scripted_logic_state.total_scheduled_outputs = total_scheduled_outputs;
    state.scripted_logic_state.total_internal_state_changes = total_internal_state_changes;
    state.scripted_logic_state.total_scheduled_follow_ups = total_scheduled_follow_ups;
    state.scripted_logic_state.total_progressed_entities = total_progressed_entities;
    state.scripted_logic_state.total_long_delay_executed =
        state.map_logic_dispatcher_state.total_long_delay_executed;
    state.scripted_logic_state.total_long_delay_pending =
        state.map_logic_dispatcher_state.total_long_delay_pending;
    state.scripted_logic_state.readiness = BuildScriptedLogicReadiness(state.scripted_logic_state);
}

struct ActivationParticipant
{
    edict_t* entity = nullptr;
    hl::game_api::detail::EntityStateSnapshot snapshot;
    std::string summary;
};

struct ActivationPreparationResult
{
    std::vector<ActivationParticipant> participants;
    std::vector<std::string> pruned_entities;
    std::vector<std::string> validation_rejections;
    std::vector<hl::game_api::EntityClassCountSummary> class_counts;
    std::string likely_blocker;
    int pruned_count = 0;
};

void ComputeFallbackAbsBox(entvars_t& vars)
{
    if (vars.solid == SOLID_BSP && (vars.angles.x != 0.0f || vars.angles.y != 0.0f || vars.angles.z != 0.0f))
    {
        float extent = 0.0f;
        extent = std::max(extent, std::fabs(vars.mins.x));
        extent = std::max(extent, std::fabs(vars.mins.y));
        extent = std::max(extent, std::fabs(vars.mins.z));
        extent = std::max(extent, std::fabs(vars.maxs.x));
        extent = std::max(extent, std::fabs(vars.maxs.y));
        extent = std::max(extent, std::fabs(vars.maxs.z));
        vars.absmin = vars.origin - Vector(extent, extent, extent);
        vars.absmax = vars.origin + Vector(extent, extent, extent);
    }
    else
    {
        vars.absmin = vars.origin + vars.mins;
        vars.absmax = vars.origin + vars.maxs;
    }

    vars.absmin.x -= 1.0f;
    vars.absmin.y -= 1.0f;
    vars.absmin.z -= 1.0f;
    vars.absmax.x += 1.0f;
    vars.absmax.y += 1.0f;
    vars.absmax.z += 1.0f;
}

bool SafeCallSetAbsBoxSeh(
    void (*function)(edict_t*),
    edict_t* entity,
    unsigned int* seh_code)
{
    if (seh_code != nullptr)
    {
        *seh_code = 0;
    }

    __try
    {
        function(entity);
        return true;
    }
    __except (seh_code != nullptr
            ? (*seh_code = GetExceptionCode(), EXCEPTION_EXECUTE_HANDLER)
            : EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

const void* ReadPrivateDataVtable(const hl::game_api::detail::EntityStateSnapshot& snapshot)
{
    if (!snapshot.private_data_present || snapshot.private_data_address == 0
        || snapshot.private_data_bytes < sizeof(void*))
    {
        return nullptr;
    }

    return *reinterpret_cast<void* const*>(snapshot.private_data_address);
}

const entvars_t* ReadPrivateDataPev(const hl::game_api::detail::EntityStateSnapshot& snapshot)
{
    if (!snapshot.private_data_present || snapshot.private_data_address == 0
        || snapshot.private_data_bytes < (sizeof(void*) * 2))
    {
        return nullptr;
    }

    const auto* bytes = reinterpret_cast<const unsigned char*>(snapshot.private_data_address);
    return *reinterpret_cast<entvars_t* const*>(bytes + sizeof(void*));
}

bool IsPointerBackedByModule(HMODULE module_handle, const void* pointer)
{
    if (module_handle == nullptr || pointer == nullptr)
    {
        return false;
    }

    MEMORY_BASIC_INFORMATION info{};
    return VirtualQuery(pointer, &info, sizeof(info)) != 0 && info.AllocationBase == module_handle;
}

bool RefreshActivationAbsBox(
    EngineShimState& state,
    edict_t* entity,
    std::string_view source,
    std::vector<std::string>& issues)
{
    if (entity == nullptr)
    {
        issues.push_back("null edict");
        return false;
    }

    ComputeFallbackAbsBox(entity->v);

    if (state.dll_functions.pfnSetAbsBox == nullptr || state.edict_store.PrivateDataOf(entity) == nullptr)
    {
        return true;
    }

    unsigned int seh_code = 0;
    if (SafeCallSetAbsBoxSeh(state.dll_functions.pfnSetAbsBox, entity, &seh_code))
    {
        return true;
    }

    issues.push_back(
        "pfnSetAbsBox SEH " + FormatExceptionCode(seh_code)
        + " during " + std::string(source));
    return false;
}

std::vector<hl::game_api::EntityClassCountSummary> BuildActivationClassCounts(
    const std::vector<ActivationParticipant>& participants)
{
    std::unordered_map<std::string, std::size_t> counts;
    for (const ActivationParticipant& participant : participants)
    {
        const std::string classname =
            participant.snapshot.classname.empty() ? "<empty>" : participant.snapshot.classname;
        ++counts[classname];
    }

    std::vector<hl::game_api::EntityClassCountSummary> summary;
    summary.reserve(counts.size());
    for (const auto& [classname, count] : counts)
    {
        summary.push_back({classname, count});
    }

    std::sort(
        summary.begin(),
        summary.end(),
        [](const hl::game_api::EntityClassCountSummary& left,
           const hl::game_api::EntityClassCountSummary& right)
        {
            if (left.count != right.count)
            {
                return left.count > right.count;
            }

            return left.classname < right.classname;
        });
    return summary;
}

bool ValidateActivationParticipant(
    EngineShimState& state,
    edict_t* entity,
    hl::game_api::detail::EntityStateSnapshot& snapshot,
    std::vector<std::string>& issues)
{
    if (entity == nullptr || snapshot.index < 0)
    {
        issues.push_back("edict pointer not owned by store");
        return false;
    }

    if (snapshot.free_flag == snapshot.in_use)
    {
        issues.push_back("free/in_use flags are incoherent");
    }

    if (!snapshot.in_use)
    {
        issues.push_back("edict is not in use");
    }

    if (snapshot.removed)
    {
        issues.push_back("edict is marked removed");
    }

    if (snapshot.deferred)
    {
        issues.push_back("edict is marked deferred");
    }

    if (!snapshot.containing_entity_valid)
    {
        issues.push_back("pContainingEntity does not point back to the edict");
    }

    if (snapshot.classname.empty())
    {
        issues.push_back("classname is empty");
    }

    if (!snapshot.classname_index_valid)
    {
        issues.push_back(
            "classname string index is invalid: " + std::to_string(snapshot.classname_index));
    }

    if (!snapshot.targetname_index_valid)
    {
        issues.push_back(
            "targetname string index is invalid: " + std::to_string(snapshot.targetname_index));
    }

    if (!snapshot.model_string.empty())
    {
        const int expected_model_index = state.precache_registry.ModelIndex(snapshot.model_string);
        if (expected_model_index == 0)
        {
            issues.push_back("model string is not present in the model registry");
        }
        else if (snapshot.model_index != 0 && snapshot.model_index != expected_model_index)
        {
            issues.push_back(
                "model/modelindex mismatch: registry=" + std::to_string(expected_model_index)
                + ", edict=" + std::to_string(snapshot.model_index));
        }
    }
    else if (snapshot.model_index > 0)
    {
        issues.push_back("modelindex is set but model string is empty");
    }

    if (!snapshot.model_string_index_valid)
    {
        issues.push_back(
            "model string index is invalid: " + std::to_string(snapshot.model_index_string));
    }

    if (snapshot.index != 0 && !snapshot.private_data_present)
    {
        issues.push_back("private data is absent");
    }

    if (snapshot.private_data_present)
    {
        if (!snapshot.private_data_owned)
        {
            issues.push_back("private data pointer is present but not owned by EdictStore");
        }

        if (snapshot.private_data_address == 0)
        {
            issues.push_back("private data address is null");
        }

        if (snapshot.private_data_bytes < sizeof(void*))
        {
            issues.push_back("private data buffer is too small");
        }

        const void* vtable = ReadPrivateDataVtable(snapshot);
        if (vtable == nullptr)
        {
            issues.push_back("private data vtable pointer is null");
        }
        else if (!IsPointerBackedByModule(
                     state.module != nullptr ? state.module->NativeHandle() : nullptr,
                     vtable))
        {
            issues.push_back("private data vtable is outside hl.dll");
        }

        if (const entvars_t* stored_pev = ReadPrivateDataPev(snapshot);
            stored_pev == nullptr)
        {
            issues.push_back("private data pev pointer is null or unavailable");
        }
        else if (stored_pev != &entity->v)
        {
            issues.push_back("private data pev pointer does not match edict vars");
        }

        RefreshActivationAbsBox(state, entity, "activation validation", issues);
        snapshot = state.edict_store.SnapshotOf(entity, state.string_pool);
    }

    return issues.empty();
}

ActivationPreparationResult CollectActivationParticipants(EngineShimState& state)
{
    ActivationPreparationResult result;
    result.participants.reserve(static_cast<std::size_t>(state.edict_store.AllocatedCount()));

    for (int index = 0; index < state.edict_store.MaxEntities(); ++index)
    {
        edict_t* entity = state.edict_store.EntityOfIndex(index);
        if (entity == nullptr)
        {
            continue;
        }

        const hl::game_api::detail::EntityStateSnapshot before_update =
            state.edict_store.SnapshotOf(entity, state.string_pool);
        bool should_activate = false;
        if (before_update.index == 0)
        {
            should_activate =
                before_update.in_use
                && !before_update.removed
                && !before_update.deferred
                && before_update.spawned
                && EqualsIgnoreCase(before_update.classname, "worldspawn");
        }
        else
        {
            should_activate =
                before_update.in_use
                && !before_update.removed
                && !before_update.deferred
                && (before_update.spawned || before_update.private_data_present);
        }

        if (!should_activate)
        {
            state.edict_store.SetActivationCandidate(entity, false);
            continue;
        }

        std::vector<std::string> issues;
        hl::game_api::detail::EntityStateSnapshot snapshot = before_update;
        if (!ValidateActivationParticipant(state, entity, snapshot, issues))
        {
            std::string issue_text = BuildEntitySnapshotSummary(before_update) + " | reject=";
            for (std::size_t issue_index = 0; issue_index < issues.size(); ++issue_index)
            {
                if (issue_index != 0)
                {
                    issue_text += "; ";
                }

                issue_text += issues[issue_index];
            }

            state.edict_store.SetActivationCandidate(entity, false);
            result.validation_rejections.push_back(issue_text);
            if (result.likely_blocker.empty())
            {
                result.likely_blocker = issue_text;
            }

            if (snapshot.index == 0)
            {
                result.pruned_entities.push_back("worldspawn invalid: " + issue_text);
            }
            else
            {
                ++result.pruned_count;
                result.pruned_entities.push_back(issue_text);
                hl::common::Logger::Warn(
                    "Server activation safe mode pruned entity: " + issue_text);
                state.edict_store.RemoveEntity(entity);
            }

            continue;
        }

        state.edict_store.SetActivationCandidate(entity, true);
        snapshot = state.edict_store.SnapshotOf(entity, state.string_pool);
        result.participants.push_back({entity, snapshot, BuildEntitySnapshotSummary(snapshot)});
    }

    result.class_counts = BuildActivationClassCounts(result.participants);
    return result;
}

const std::string* FindLastKeyValue(
    const hl::game_api::detail::EntityDefinition& entity,
    std::string_view key_name)
{
    for (auto it = entity.key_values.rbegin(); it != entity.key_values.rend(); ++it)
    {
        if (EqualsIgnoreCase(it->key, key_name))
        {
            return &it->value;
        }
    }

    return nullptr;
}

TriggerChangeLevelFields ExtractTriggerChangeLevelFields(
    const hl::game_api::detail::EntityDefinition* definition)
{
    TriggerChangeLevelFields fields;
    if (definition == nullptr)
    {
        return fields;
    }

    if (const std::string* value = FindLastKeyValue(*definition, "map");
        value != nullptr)
    {
        fields.map_name = TrimWhitespaceCopy(*value);
    }

    if (const std::string* value = FindLastKeyValue(*definition, "landmark");
        value != nullptr)
    {
        fields.landmark = TrimWhitespaceCopy(*value);
    }

    return fields;
}

void NoteTriggerChangeLevelCandidate(
    hl::game_api::ChangeLevelTransitionSummary& summary,
    const hl::game_api::detail::EntityDefinition* definition,
    const RuntimeEntityRecord* record)
{
    summary.candidate_present = true;
    summary.staged_supported = true;
    if (record != nullptr && record->deferred)
    {
        summary.deferred_candidate = true;
    }

    if (summary.source_classname.empty())
    {
        if (record != nullptr && !record->classname.empty())
        {
            summary.source_classname = record->classname;
        }
        else if (definition != nullptr && !definition->classname.empty())
        {
            summary.source_classname = definition->classname;
        }
        else
        {
            summary.source_classname = "trigger_changelevel";
        }
    }

    if (summary.source_edict_index < 0 && record != nullptr)
    {
        summary.source_edict_index = record->edict_index;
    }

    const TriggerChangeLevelFields fields = ExtractTriggerChangeLevelFields(definition);
    if (summary.target_map.empty() && !fields.map_name.empty())
    {
        summary.target_map = fields.map_name;
    }
    if (summary.landmark.empty() && !fields.landmark.empty())
    {
        summary.landmark = fields.landmark;
    }

    if (summary.support_detail.empty())
    {
        summary.support_detail =
            "staged-safe candidate retained without full touch or map-change lifecycle";
    }

    if (!summary.pending_request_captured && summary.pending_request_detail.empty())
    {
        summary.pending_request_detail =
            "not exercised under current host constraints: no staged touch/changelevel-request path observed";
    }
}

bool ValidateChangeLevelBspLumpBounds(
    const ChangeLevelValidationBspLumpHeader& lump,
    std::uintmax_t file_size)
{
    if (lump.file_offset < 0 || lump.file_length < 0)
    {
        return false;
    }

    const std::uintmax_t offset = static_cast<std::uintmax_t>(lump.file_offset);
    const std::uintmax_t length = static_cast<std::uintmax_t>(lump.file_length);
    return offset <= file_size && length <= file_size - offset;
}

const hl::game_api::detail::EntityDefinition* FindLandmarkDefinitionByTargetname(
    const std::vector<hl::game_api::detail::EntityDefinition>& definitions,
    std::string_view landmark)
{
    if (landmark.empty())
    {
        return nullptr;
    }

    for (const hl::game_api::detail::EntityDefinition& definition : definitions)
    {
        if (!EqualsIgnoreCase(definition.classname, "info_landmark"))
        {
            continue;
        }

        const std::string* targetname = FindLastKeyValue(definition, "targetname");
        if (targetname != nullptr && EqualsIgnoreCase(*targetname, landmark))
        {
            return &definition;
        }
    }

    return nullptr;
}

struct ChangeLevelTargetMapDryRunProbe
{
    bool target_map_exists = false;
    bool entity_parse_succeeded = false;
    bool target_landmark_found = false;
    bool target_landmark_origin_available = false;
    std::string target_landmark_origin;
    bool target_landmark_angles_available = false;
    std::string target_landmark_angles;
    bool target_worldspawn_present = false;
    std::string relative_map_path;
    std::string detail;
};

std::string BuildChangeLevelTargetBspPath(
    const EngineShimState& state,
    std::string_view requested_map)
{
    const std::string relative_map_path =
        hl::game_api::detail::BuildMapModelPath(requested_map);
    if (relative_map_path.empty())
    {
        return {};
    }

    if (state.server_state.game_directory.empty())
    {
        return relative_map_path;
    }

    return hl::common::ToUtf8(
        (state.server_state.game_directory / hl::common::ToWide(relative_map_path)).lexically_normal());
}

std::optional<std::string> TryBuildLandmarkOriginText(
    const hl::game_api::detail::EntityDefinition* definition)
{
    if (definition == nullptr)
    {
        return std::nullopt;
    }

    const ParsedVectorField origin = ParseOriginField(*definition);
    if (!origin.present || !origin.valid)
    {
        return std::nullopt;
    }

    return !origin.raw_text.empty() ? origin.raw_text : FormatVector(origin.value);
}

std::optional<std::string> TryBuildLandmarkAnglesText(
    const hl::game_api::detail::EntityDefinition* definition)
{
    if (definition == nullptr)
    {
        return std::nullopt;
    }

    const ParsedVectorField angles = ParseAnglesField(*definition);
    if (angles.present)
    {
        if (!angles.valid)
        {
            return std::nullopt;
        }

        return FormatVector(angles.value);
    }

    return FormatVector(Vector(0.0f, 0.0f, 0.0f));
}

ChangeLevelTargetMapDryRunProbe ProbeChangeLevelTargetMapDryRun(
    const EngineShimState& state,
    std::string_view requested_map,
    std::string_view landmark)
{
    ChangeLevelTargetMapDryRunProbe probe;
    if (requested_map.empty())
    {
        return probe;
    }

    probe.relative_map_path = hl::game_api::detail::BuildMapModelPath(requested_map);
    const std::filesystem::path absolute_map_path =
        state.server_state.game_directory / hl::common::ToWide(probe.relative_map_path);
    if (!state.file_system.FileExists(absolute_map_path))
    {
        return probe;
    }

    probe.target_map_exists = true;

    const std::optional<std::vector<unsigned char>> bytes =
        state.file_system.ReadBinaryFile(absolute_map_path);
    if (!bytes.has_value())
    {
        probe.detail = "failed to read target BSP";
        return probe;
    }

    if (bytes->size() < sizeof(ChangeLevelValidationBspHeader))
    {
        probe.detail = "target BSP is smaller than a GoldSrc header";
        return probe;
    }

    ChangeLevelValidationBspHeader header{};
    std::memcpy(&header, bytes->data(), sizeof(header));
    if (header.version != hl::game_api::detail::kGoldSrcBspVersion)
    {
        probe.detail = "unsupported BSP version " + std::to_string(header.version);
        return probe;
    }

    const ChangeLevelValidationBspLumpHeader& entities_lump = header.lumps[0];
    if (!ValidateChangeLevelBspLumpBounds(entities_lump, bytes->size()))
    {
        probe.detail = "target BSP entities lump is out of file bounds";
        return probe;
    }

    if (entities_lump.file_length <= 0)
    {
        probe.detail = "target BSP entities lump is missing or empty";
        return probe;
    }

    std::string_view entity_lump_text(
        reinterpret_cast<const char*>(bytes->data()) + entities_lump.file_offset,
        static_cast<std::size_t>(entities_lump.file_length));
    const std::size_t terminator = entity_lump_text.find('\0');
    if (terminator != std::string_view::npos)
    {
        entity_lump_text = entity_lump_text.substr(0, terminator);
    }

    const hl::game_api::detail::EntityLumpParseResult parse_result =
        hl::game_api::detail::EntityLumpParser::Parse(entity_lump_text);
    probe.entity_parse_succeeded =
        parse_result.readable
        && parse_result.parsed_any
        && !parse_result.partially_parsed
        && parse_result.errors.empty()
        && parse_result.failure_reason.empty();
    probe.target_worldspawn_present =
        probe.entity_parse_succeeded
        && std::any_of(
            parse_result.entities.begin(),
            parse_result.entities.end(),
            [](const hl::game_api::detail::EntityDefinition& definition)
            {
                return EqualsIgnoreCase(definition.classname, "worldspawn");
            });
    const hl::game_api::detail::EntityDefinition* target_landmark_definition =
        FindLandmarkDefinitionByTargetname(parse_result.entities, landmark);
    probe.target_landmark_found = target_landmark_definition != nullptr;
    if (probe.entity_parse_succeeded)
    {
        if (const std::optional<std::string> origin =
                TryBuildLandmarkOriginText(target_landmark_definition);
            origin.has_value())
        {
            probe.target_landmark_origin_available = true;
            probe.target_landmark_origin = *origin;
        }
        if (const std::optional<std::string> angles =
                TryBuildLandmarkAnglesText(target_landmark_definition);
            angles.has_value())
        {
            probe.target_landmark_angles_available = true;
            probe.target_landmark_angles = *angles;
        }
    }
    if (!probe.entity_parse_succeeded)
    {
        probe.detail = !parse_result.failure_reason.empty()
            ? parse_result.failure_reason
            : "target BSP entities lump parse failed";
    }

    return probe;
}

struct ChangeLevelLandmarkOrientationBasis
{
    bool valid = false;
    std::string current_landmark_angles;
    std::string target_landmark_angles;
    float current_landmark_yaw = 0.0f;
    float target_landmark_yaw = 0.0f;
    std::string short_circuit_reason;
};

bool TryResolveChangeLevelLandmarkOrientationBasis(
    const EngineShimState& state,
    const hl::game_api::ChangeLevelTransitionSummary::ChangeLevelBootstrapPlanSummary& plan,
    ChangeLevelLandmarkOrientationBasis* basis)
{
    if (basis == nullptr)
    {
        return false;
    }

    *basis = {};
    if (plan.landmark.empty())
    {
        basis->short_circuit_reason = "landmark-name-unavailable";
        return false;
    }

    const hl::game_api::detail::EntityDefinition* current_landmark_definition =
        FindParsedTargetDefinitionByTargetname(state, plan.landmark, "info_landmark");
    if (current_landmark_definition == nullptr)
    {
        basis->short_circuit_reason = "current-landmark-unavailable";
        return false;
    }

    const std::optional<std::string> current_angles =
        TryBuildLandmarkAnglesText(current_landmark_definition);
    if (!current_angles.has_value())
    {
        basis->short_circuit_reason = "current-landmark-angles-unavailable";
        return false;
    }

    Vector current_angles_value(0.0f, 0.0f, 0.0f);
    if (!ParseStrictVector3(*current_angles, &current_angles_value))
    {
        basis->short_circuit_reason = "current-landmark-angle-parse-failed";
        return false;
    }

    const ChangeLevelTargetMapDryRunProbe target_probe =
        ProbeChangeLevelTargetMapDryRun(state, plan.requested_map, plan.landmark);
    if (!target_probe.target_map_exists)
    {
        basis->short_circuit_reason = "target-map-unavailable";
        return false;
    }
    if (!target_probe.entity_parse_succeeded)
    {
        basis->short_circuit_reason = "target-map-entity-parse-failed";
        return false;
    }
    if (!target_probe.target_landmark_found)
    {
        basis->short_circuit_reason = "target-landmark-unavailable";
        return false;
    }
    if (!target_probe.target_landmark_angles_available)
    {
        basis->short_circuit_reason = "target-landmark-angles-unavailable";
        return false;
    }

    Vector target_angles_value(0.0f, 0.0f, 0.0f);
    if (!ParseStrictVector3(target_probe.target_landmark_angles, &target_angles_value))
    {
        basis->short_circuit_reason = "target-landmark-angle-parse-failed";
        return false;
    }

    basis->valid = true;
    basis->current_landmark_angles = *current_angles;
    basis->target_landmark_angles = target_probe.target_landmark_angles;
    basis->current_landmark_yaw = current_angles_value.y;
    basis->target_landmark_yaw = target_angles_value.y;
    return true;
}

void EnsureChangeLevelTargetValidation(EngineShimState& state)
{
    hl::game_api::ChangeLevelTransitionSummary& summary = state.changelevel_transition_state;
    if (summary.target_validation.attempted
        || (!summary.pending_request_captured && !summary.transition_intent_captured))
    {
        return;
    }

    hl::game_api::ChangeLevelTransitionSummary::ChangeLevelTargetValidationSummary& validation =
        summary.target_validation;
    validation = {};
    validation.attempted = true;
    validation.action = "no-op dry-run validation";
    validation.current_map = !state.world_context.map_name.empty()
        ? state.world_context.map_name
        : hl::game_api::detail::NormalizeMapName(state.server_state.map_name);
    validation.requested_map = hl::game_api::detail::NormalizeMapName(summary.target_map);
    validation.target_bsp_path =
        BuildChangeLevelTargetBspPath(state, validation.requested_map);
    validation.landmark = TrimWhitespaceCopy(summary.landmark);
    const hl::game_api::detail::EntityDefinition* current_landmark_definition =
        FindParsedTargetDefinitionByTargetname(state, validation.landmark, "info_landmark");
    validation.current_landmark_found = current_landmark_definition != nullptr;
    if (const std::optional<std::string> origin =
            TryBuildLandmarkOriginText(current_landmark_definition);
        origin.has_value())
    {
        validation.current_landmark_origin_available = true;
        validation.current_landmark_origin = *origin;
    }

    const ChangeLevelTargetMapDryRunProbe target_probe =
        ProbeChangeLevelTargetMapDryRun(state, validation.requested_map, validation.landmark);
    validation.target_map_exists = target_probe.target_map_exists;
    validation.entity_parse_succeeded = target_probe.entity_parse_succeeded;
    validation.target_landmark_found = target_probe.target_landmark_found;
    validation.target_landmark_origin_available = target_probe.target_landmark_origin_available;
    validation.target_landmark_origin = target_probe.target_landmark_origin;
    validation.target_worldspawn_present = target_probe.target_worldspawn_present;

    if (validation.current_map.empty())
    {
        validation.missing_component = "current map name";
        return;
    }

    if (validation.requested_map.empty())
    {
        validation.missing_component = "requested map name";
        return;
    }

    if (!validation.target_map_exists)
    {
        validation.missing_component = target_probe.relative_map_path.empty()
            ? "requested map BSP"
            : "requested map BSP " + target_probe.relative_map_path;
        return;
    }

    if (validation.landmark.empty())
    {
        validation.missing_component = "landmark name";
        return;
    }

    if (!validation.current_landmark_found)
    {
        validation.missing_component =
            "current info_landmark targetname=" + validation.landmark;
        return;
    }

    if (!validation.entity_parse_succeeded)
    {
        validation.missing_component = "target map entity lump";
        validation.detail = target_probe.detail;
        return;
    }

    if (!validation.target_landmark_found)
    {
        validation.missing_component =
            "target info_landmark targetname=" + validation.landmark;
    }
}

void EnsureChangeLevelLifecycleGate(EngineShimState& state)
{
    hl::game_api::ChangeLevelTransitionSummary& summary = state.changelevel_transition_state;
    if (summary.lifecycle_gate.attempted || !summary.transition_intent_captured)
    {
        return;
    }

    EnsureChangeLevelTargetValidation(state);

    hl::game_api::ChangeLevelTransitionSummary::ChangeLevelLifecycleGateSummary& gate =
        summary.lifecycle_gate;
    gate = {};
    gate.attempted = true;
    gate.intent_consumed = summary.transition_intent_consumed;
    gate.target_validation_passed = IsChangeLevelTargetValidationValid(summary.target_validation);
    gate.requested_map = hl::game_api::detail::NormalizeMapName(summary.target_map);
    gate.landmark = TrimWhitespaceCopy(summary.landmark);

    if (!gate.intent_consumed)
    {
        gate.bootstrap_allowed = false;
        gate.action = "no-op gated-blocked";
        gate.reason = "transition intent not consumed";
        return;
    }

    if (!gate.target_validation_passed)
    {
        gate.bootstrap_allowed = false;
        gate.action = "no-op gated-blocked";
        gate.reason = !summary.target_validation.missing_component.empty()
            ? summary.target_validation.missing_component
            : "target validation unavailable";
        if (!summary.target_validation.detail.empty())
        {
            gate.reason += " (" + summary.target_validation.detail + ")";
        }
        return;
    }

    gate.bootstrap_allowed = true;
    gate.action = "no-op gated-ready";
}

void RefreshChangeLevelLifecycleEntry(EngineShimState& state)
{
    hl::game_api::ChangeLevelTransitionSummary& summary = state.changelevel_transition_state;
    if (!summary.lifecycle_gate.attempted)
    {
        return;
    }

    hl::game_api::ChangeLevelTransitionSummary::ChangeLevelLifecycleEntrySummary& entry =
        summary.lifecycle_entry;
    entry = {};
    entry.attempted = true;
    entry.gate_checked = true;
    entry.gate_passed = summary.lifecycle_gate.bootstrap_allowed;
    entry.requested_map = summary.lifecycle_gate.requested_map;
    entry.landmark = summary.lifecycle_gate.landmark;

    if (!entry.gate_passed)
    {
        entry.action = "no-op entry blocked";
        entry.short_circuit_reason = !summary.lifecycle_gate.reason.empty()
            ? summary.lifecycle_gate.reason
            : "bootstrap not allowed";
        return;
    }

    if (summary.pre_changelevel_handoff.stop_requested)
    {
        entry.blocked_by_stop_mode = true;
        entry.action = "no-op entry skipped by stop mode";
        entry.short_circuit_reason = "stop-on-changelevel-request";
        return;
    }

    entry.eligible = true;
    entry.action = "no-op entry armed";
}

// Dispatch chooses whether the sole future execution hook may arm.
void RefreshChangeLevelLifecycleDispatch(EngineShimState& state)
{
    hl::game_api::ChangeLevelTransitionSummary& summary = state.changelevel_transition_state;
    if (!summary.lifecycle_entry.attempted)
    {
        return;
    }

    const hl::game_api::ChangeLevelTransitionSummary::ChangeLevelLifecycleEntrySummary& entry =
        summary.lifecycle_entry;
    hl::game_api::ChangeLevelTransitionSummary::ChangeLevelLifecycleDispatchSummary& dispatch =
        summary.lifecycle_dispatch;
    dispatch = {};
    dispatch.attempted = true;
    dispatch.dispatch_checked = true;
    dispatch.decision_source = "changelevel_lifecycle_entry";
    dispatch.requested_map = entry.requested_map;
    dispatch.landmark = entry.landmark;

    if (entry.eligible)
    {
        dispatch.dispatch_allowed = true;
        dispatch.dispatch_blocked = false;
        dispatch.action = "no-op dispatch armed";
        return;
    }

    dispatch.dispatch_allowed = false;
    dispatch.dispatch_blocked = true;
    dispatch.short_circuit_reason = !entry.short_circuit_reason.empty()
        ? entry.short_circuit_reason
        : "changelevel_lifecycle_entry not eligible";

    if (entry.blocked_by_stop_mode)
    {
        dispatch.action = "no-op dispatch skipped";
        return;
    }

    dispatch.action = "no-op dispatch blocked";
}

// The single future execution hook for any next-map/bootstrap lifecycle work.
void RefreshChangeLevelLifecycleExecution(EngineShimState& state)
{
    hl::game_api::ChangeLevelTransitionSummary& summary = state.changelevel_transition_state;
    if (!summary.lifecycle_dispatch.attempted)
    {
        return;
    }

    const hl::game_api::ChangeLevelTransitionSummary::ChangeLevelLifecycleDispatchSummary& dispatch =
        summary.lifecycle_dispatch;
    hl::game_api::ChangeLevelTransitionSummary::ChangeLevelLifecycleExecutionSummary& execution =
        summary.lifecycle_execution;
    execution = {};
    execution.attempted = true;
    execution.execution_checked = true;
    execution.decision_source = "changelevel_lifecycle_dispatch";
    execution.current_map = summary.target_validation.current_map;
    execution.requested_map = dispatch.requested_map;
    execution.target_bsp_path = summary.target_validation.target_bsp_path;
    execution.landmark = dispatch.landmark;
    execution.target_worldspawn_present = summary.target_validation.target_worldspawn_present;
    execution.target_entity_parse_ok = summary.target_validation.entity_parse_succeeded;
    execution.current_landmark_origin_available =
        summary.target_validation.current_landmark_origin_available;
    execution.current_landmark_origin = summary.target_validation.current_landmark_origin;
    execution.target_landmark_origin_available =
        summary.target_validation.target_landmark_origin_available;
    execution.target_landmark_origin = summary.target_validation.target_landmark_origin;

    if (dispatch.dispatch_allowed)
    {
        execution.execution_armed = true;
        execution.execution_skipped = false;
        execution.action = "no-op execution armed";
        return;
    }

    execution.execution_armed = false;
    execution.execution_skipped = true;
    execution.short_circuit_reason = !dispatch.short_circuit_reason.empty()
        ? dispatch.short_circuit_reason
        : "changelevel_lifecycle_dispatch blocked";
    execution.action = "no-op execution skipped";
}

void RefreshChangeLevelBootstrapPlan(EngineShimState& state)
{
    hl::game_api::ChangeLevelTransitionSummary& summary = state.changelevel_transition_state;
    if (!summary.lifecycle_execution.attempted)
    {
        return;
    }

    const hl::game_api::ChangeLevelTransitionSummary::ChangeLevelLifecycleExecutionSummary& execution =
        summary.lifecycle_execution;
    hl::game_api::ChangeLevelTransitionSummary::ChangeLevelBootstrapPlanSummary& plan =
        summary.changelevel_bootstrap_plan;
    plan = {};
    plan.attempted = true;
    plan.decision_source = "changelevel_lifecycle_execution";
    plan.requested_map = execution.requested_map;
    plan.landmark = execution.landmark;

    if (execution.execution_armed)
    {
        plan.prepared = true;
        plan.skipped = false;
        plan.current_map = execution.current_map;
        plan.target_bsp_path = execution.target_bsp_path;
        plan.target_worldspawn_present = execution.target_worldspawn_present;
        plan.target_entity_parse_ok = execution.target_entity_parse_ok;
        plan.current_landmark_origin_available = execution.current_landmark_origin_available;
        plan.current_landmark_origin = execution.current_landmark_origin;
        plan.target_landmark_origin_available = execution.target_landmark_origin_available;
        plan.target_landmark_origin = execution.target_landmark_origin;
        plan.action = "prepared-no-load";
        return;
    }

    plan.prepared = false;
    plan.skipped = execution.execution_skipped;
    plan.short_circuit_reason = execution.short_circuit_reason;
    plan.action = execution.execution_skipped ? "plan skipped" : "plan unavailable";
}

void RefreshChangeLevelLandmarkTransform(EngineShimState& state)
{
    hl::game_api::ChangeLevelTransitionSummary& summary = state.changelevel_transition_state;
    if (!summary.changelevel_bootstrap_plan.attempted)
    {
        return;
    }

    const hl::game_api::ChangeLevelTransitionSummary::ChangeLevelBootstrapPlanSummary& plan =
        summary.changelevel_bootstrap_plan;
    hl::game_api::ChangeLevelTransitionSummary::ChangeLevelLandmarkTransformSummary& transform =
        summary.changelevel_landmark_transform;
    transform = {};
    transform.attempted = true;
    transform.decision_source = "changelevel_bootstrap_plan";
    transform.current_landmark_origin_available = plan.current_landmark_origin_available;
    transform.current_landmark_origin = plan.current_landmark_origin;
    transform.target_landmark_origin_available = plan.target_landmark_origin_available;
    transform.target_landmark_origin = plan.target_landmark_origin;

    if (plan.prepared)
    {
        Vector current_landmark_origin = Vector(0.0f, 0.0f, 0.0f);
        Vector target_landmark_origin = Vector(0.0f, 0.0f, 0.0f);
        if (plan.current_landmark_origin_available
            && plan.target_landmark_origin_available
            && ParseStrictVector3(plan.current_landmark_origin, &current_landmark_origin)
            && ParseStrictVector3(plan.target_landmark_origin, &target_landmark_origin))
        {
            transform.transform_computed = true;
            transform.skipped = false;
            transform.translation_delta =
                FormatVector(target_landmark_origin - current_landmark_origin);
            transform.action = "no-op transform prepared";
            return;
        }

        transform.transform_computed = false;
        transform.skipped = false;
        transform.short_circuit_reason =
            (!plan.current_landmark_origin_available || !plan.target_landmark_origin_available)
            ? "landmark-origin-unavailable"
            : "landmark-origin-parse-failed";
        transform.action = "transform unavailable";
        return;
    }

    transform.transform_computed = false;
    transform.skipped = plan.skipped;
    transform.short_circuit_reason = plan.short_circuit_reason;
    transform.action = plan.skipped ? "transform skipped" : "transform unavailable";
}

void RefreshChangeLevelProjectedCarriedOrigin(EngineShimState& state)
{
    hl::game_api::ChangeLevelTransitionSummary& summary = state.changelevel_transition_state;
    if (!summary.changelevel_landmark_transform.attempted)
    {
        return;
    }

    const hl::game_api::ChangeLevelTransitionSummary::ChangeLevelLandmarkTransformSummary&
        transform = summary.changelevel_landmark_transform;
    hl::game_api::ChangeLevelTransitionSummary::ChangeLevelProjectedCarriedOriginSummary&
        projected = summary.changelevel_projected_carried_origin;
    projected = {};
    projected.attempted = true;
    projected.decision_source = "changelevel_landmark_transform";
    projected.translation_delta = transform.translation_delta;

    if (transform.transform_computed)
    {
        Vector current_carried_origin = Vector(0.0f, 0.0f, 0.0f);
        Vector translation_delta = Vector(0.0f, 0.0f, 0.0f);
        std::string current_carried_origin_text;
        if (TryResolveRequestTimeCarriedOrigin(
                summary,
                &current_carried_origin,
                &current_carried_origin_text)
            && ParseStrictVector3(transform.translation_delta, &translation_delta))
        {
            projected.projected = true;
            projected.skipped = false;
            projected.current_carried_origin_available = true;
            projected.current_carried_origin = current_carried_origin_text;
            projected.projected_target_origin =
                FormatVector(current_carried_origin + translation_delta);
            projected.action = "no-op carried-origin projection";
            return;
        }

        projected.projected = false;
        projected.skipped = false;
        projected.short_circuit_reason = current_carried_origin_text.empty()
            ? "current-carried-origin-unavailable"
            : "translation-delta-parse-failed";
        projected.action = "projection unavailable";
        return;
    }

    projected.projected = false;
    projected.skipped = transform.skipped;
    projected.short_circuit_reason = transform.short_circuit_reason;
    projected.action = transform.skipped ? "projection skipped" : "projection unavailable";
}

void RefreshChangeLevelProjectedCarriedOrientation(EngineShimState& state)
{
    hl::game_api::ChangeLevelTransitionSummary& summary = state.changelevel_transition_state;
    if (!summary.changelevel_landmark_transform.attempted)
    {
        return;
    }

    const hl::game_api::ChangeLevelTransitionSummary::ChangeLevelLandmarkTransformSummary&
        transform = summary.changelevel_landmark_transform;
    hl::game_api::ChangeLevelTransitionSummary::ChangeLevelProjectedCarriedOrientationSummary&
        projected = summary.changelevel_projected_carried_orientation;
    projected = {};
    projected.attempted = true;
    projected.decision_source = "changelevel_landmark_transform";

    if (transform.transform_computed)
    {
        float current_carried_yaw = 0.0f;
        std::string current_carried_yaw_text;
        ChangeLevelLandmarkOrientationBasis basis;
        if (TryResolveRequestTimeCarriedYaw(
                summary,
                &current_carried_yaw,
                &current_carried_yaw_text)
            && TryResolveChangeLevelLandmarkOrientationBasis(
                state,
                summary.changelevel_bootstrap_plan,
                &basis))
        {
            const float yaw_delta =
                ComputeSignedAngleDeltaDegrees(basis.current_landmark_yaw, basis.target_landmark_yaw);
            const float projected_target_yaw =
                NormalizeAngleDegrees(current_carried_yaw + yaw_delta);
            projected.projected = true;
            projected.skipped = false;
            projected.current_carried_yaw_available = true;
            projected.current_carried_yaw = current_carried_yaw_text;
            projected.current_landmark_angles_available = true;
            projected.current_landmark_angles = basis.current_landmark_angles;
            projected.target_landmark_angles_available = true;
            projected.target_landmark_angles = basis.target_landmark_angles;
            projected.yaw_delta = FormatScalar(yaw_delta);
            projected.projected_target_yaw =
                FormatScalar(std::fabs(projected_target_yaw) < 0.0001f ? 0.0f : projected_target_yaw);
            projected.action = "no-op carried-orientation projection";
            return;
        }

        projected.projected = false;
        projected.skipped = false;
        projected.short_circuit_reason = current_carried_yaw_text.empty()
            ? "current-carried-yaw-unavailable"
            : (!basis.short_circuit_reason.empty()
                ? basis.short_circuit_reason
                : "landmark-orientation-basis-unavailable");
        projected.action = "orientation projection unavailable";
        return;
    }

    projected.projected = false;
    projected.skipped = transform.skipped;
    projected.short_circuit_reason = transform.short_circuit_reason;
    projected.action = transform.skipped
        ? "orientation projection skipped"
        : "orientation projection unavailable";
}

hl::game_api::ChangeLevelTransitionSummary::ChangeLevelProjectedTransferSnapshotSummary
BuildChangeLevelProjectedTransferSnapshot(
    const hl::game_api::ChangeLevelTransitionSummary::ChangeLevelBootstrapPlanSummary& plan,
    const hl::game_api::ChangeLevelTransitionSummary::ChangeLevelProjectedCarriedOriginSummary&
        projected_origin,
    const hl::game_api::ChangeLevelTransitionSummary::ChangeLevelProjectedCarriedOrientationSummary&
        projected_orientation)
{
    hl::game_api::ChangeLevelTransitionSummary::ChangeLevelProjectedTransferSnapshotSummary
        snapshot;
    snapshot.attempted = true;

    if (plan.prepared && projected_origin.projected && projected_orientation.projected)
    {
        snapshot.prepared = true;
        snapshot.skipped = false;
        snapshot.decision_source = "projected-carried-artifacts";
        snapshot.current_map = plan.current_map;
        snapshot.requested_map = plan.requested_map;
        snapshot.target_bsp_path = plan.target_bsp_path;
        snapshot.landmark = plan.landmark;
        snapshot.projected_target_origin = projected_origin.projected_target_origin;
        snapshot.projected_target_yaw = projected_orientation.projected_target_yaw;
        snapshot.target_worldspawn_present = plan.target_worldspawn_present;
        snapshot.target_entity_parse_ok = plan.target_entity_parse_ok;
        snapshot.transfer_ready = true;
        snapshot.action = "no-op transfer snapshot prepared";
        return snapshot;
    }

    snapshot.prepared = false;
    snapshot.transfer_ready = false;
    snapshot.short_circuit_reason = !plan.short_circuit_reason.empty()
        ? plan.short_circuit_reason
        : (!projected_origin.short_circuit_reason.empty()
            ? projected_origin.short_circuit_reason
            : projected_orientation.short_circuit_reason);

    if (plan.skipped || projected_origin.skipped || projected_orientation.skipped)
    {
        snapshot.skipped = true;
        snapshot.action = "transfer snapshot skipped";
        return snapshot;
    }

    snapshot.skipped = false;
    snapshot.decision_source = "projected-carried-artifacts";
    snapshot.action = "transfer snapshot unavailable";
    return snapshot;
}

hl::game_api::ChangeLevelTransitionSummary::ChangeLevelPlayerTransferApplyPlanSummary
BuildChangeLevelPlayerTransferApplyPlan(
    const hl::game_api::ChangeLevelTransitionSummary::ChangeLevelProjectedTransferSnapshotSummary&
        snapshot)
{
    hl::game_api::ChangeLevelTransitionSummary::ChangeLevelPlayerTransferApplyPlanSummary plan;
    plan.attempted = true;

    if (snapshot.prepared && snapshot.transfer_ready)
    {
        plan.prepared = true;
        plan.skipped = false;
        plan.decision_source = "projected-transfer-snapshot";
        plan.apply_target = "player";
        plan.current_map = snapshot.current_map;
        plan.requested_map = snapshot.requested_map;
        plan.target_bsp_path = snapshot.target_bsp_path;
        plan.landmark = snapshot.landmark;
        plan.target_player_origin = snapshot.projected_target_origin;
        plan.target_player_yaw = snapshot.projected_target_yaw;
        plan.origin_write_prepared = true;
        plan.yaw_write_prepared = true;
        plan.inventory_write_prepared = false;
        plan.velocity_write_prepared = false;
        plan.target_worldspawn_present = snapshot.target_worldspawn_present;
        plan.target_entity_parse_ok = snapshot.target_entity_parse_ok;
        plan.apply_ready = true;
        plan.action = "no-op player apply plan prepared";
        return plan;
    }

    plan.prepared = false;
    plan.apply_ready = false;
    plan.short_circuit_reason = snapshot.short_circuit_reason;

    if (snapshot.skipped)
    {
        plan.skipped = true;
        plan.action = "player apply plan skipped";
        return plan;
    }

    plan.skipped = false;
    plan.decision_source = "projected-transfer-snapshot";
    plan.apply_target = "player";
    plan.action = "player apply plan unavailable";
    return plan;
}

hl::game_api::ChangeLevelTransitionSummary::ChangeLevelPlayerTransferWriteSetSummary
BuildChangeLevelPlayerTransferWriteSet(
    const hl::game_api::ChangeLevelTransitionSummary::ChangeLevelPlayerTransferApplyPlanSummary&
        plan)
{
    hl::game_api::ChangeLevelTransitionSummary::ChangeLevelPlayerTransferWriteSetSummary write_set;
    write_set.attempted = true;

    if (plan.prepared && plan.apply_ready)
    {
        write_set.prepared = true;
        write_set.skipped = false;
        write_set.decision_source = "player-transfer-apply-plan";
        write_set.apply_target = plan.apply_target;
        write_set.current_map = plan.current_map;
        write_set.requested_map = plan.requested_map;
        write_set.target_bsp_path = plan.target_bsp_path;
        write_set.landmark = plan.landmark;
        write_set.target_player_origin = plan.target_player_origin;
        write_set.target_player_yaw = plan.target_player_yaw;
        write_set.write_origin = plan.origin_write_prepared;
        write_set.write_yaw = plan.yaw_write_prepared;
        write_set.write_inventory = plan.inventory_write_prepared;
        write_set.write_velocity = plan.velocity_write_prepared;
        write_set.write_count = (write_set.write_origin ? 1 : 0)
            + (write_set.write_yaw ? 1 : 0) + (write_set.write_inventory ? 1 : 0)
            + (write_set.write_velocity ? 1 : 0);
        write_set.runtime_write_suppressed = true;
        write_set.target_worldspawn_present = plan.target_worldspawn_present;
        write_set.target_entity_parse_ok = plan.target_entity_parse_ok;
        write_set.write_set_ready = true;
        write_set.action = "no-op player transfer write set prepared";
        return write_set;
    }

    write_set.prepared = false;
    write_set.write_set_ready = false;
    write_set.short_circuit_reason = plan.short_circuit_reason;

    if (plan.skipped)
    {
        write_set.skipped = true;
        write_set.action = "player transfer write set skipped";
        return write_set;
    }

    write_set.skipped = false;
    write_set.decision_source = "player-transfer-apply-plan";
    write_set.apply_target = plan.apply_target;
    write_set.action = "player transfer write set unavailable";
    return write_set;
}

hl::game_api::ChangeLevelTransitionSummary::ChangeLevelPlayerTransferDeferredApplyGateSummary
BuildChangeLevelPlayerTransferDeferredApplyGate(
    const hl::game_api::ChangeLevelTransitionSummary::ChangeLevelPlayerTransferWriteSetSummary&
        write_set)
{
    hl::game_api::ChangeLevelTransitionSummary::ChangeLevelPlayerTransferDeferredApplyGateSummary
        gate;
    gate.attempted = true;

    if (write_set.prepared && write_set.write_set_ready)
    {
        gate.prepared = true;
        gate.skipped = false;
        gate.decision_source = "player-transfer-write-set";
        gate.apply_target = write_set.apply_target;
        gate.current_map = write_set.current_map;
        gate.requested_map = write_set.requested_map;
        gate.future_apply_phase = "post-target-bootstrap-pre-player-resume";
        gate.pending_write_count = write_set.write_count;
        gate.gate_open = false;
        gate.deferred = true;
        gate.gate_reason = "target-map-runtime-not-active";
        gate.runtime_write_suppressed = write_set.runtime_write_suppressed;
        gate.apply_gate_ready = true;
        gate.action = "no-op deferred player apply gate prepared";
        return gate;
    }

    gate.prepared = false;
    gate.apply_gate_ready = false;
    gate.short_circuit_reason = write_set.short_circuit_reason;

    if (write_set.skipped)
    {
        gate.skipped = true;
        gate.action = "deferred player apply gate skipped";
        return gate;
    }

    gate.skipped = false;
    gate.decision_source = "player-transfer-write-set";
    gate.apply_target = write_set.apply_target;
    gate.current_map = write_set.current_map;
    gate.requested_map = write_set.requested_map;
    gate.pending_write_count = write_set.write_count;
    gate.runtime_write_suppressed = write_set.runtime_write_suppressed;
    gate.action = "deferred player apply gate unavailable";
    return gate;
}

hl::game_api::ChangeLevelTransitionSummary::ChangeLevelPlayerTransferGateOpenCheckpointSummary
BuildChangeLevelPlayerTransferGateOpenCheckpoint(
    const hl::game_api::ChangeLevelTransitionSummary::
        ChangeLevelPlayerTransferDeferredApplyGateSummary& gate)
{
    hl::game_api::ChangeLevelTransitionSummary::ChangeLevelPlayerTransferGateOpenCheckpointSummary
        checkpoint;
    checkpoint.attempted = true;

    if (gate.prepared && gate.apply_gate_ready)
    {
        checkpoint.prepared = true;
        checkpoint.skipped = false;
        checkpoint.decision_source = "player-transfer-deferred-apply-gate";
        checkpoint.apply_target = gate.apply_target;
        checkpoint.current_map = gate.current_map;
        checkpoint.requested_map = gate.requested_map;
        checkpoint.future_apply_phase = gate.future_apply_phase;
        checkpoint.target_runtime_checkpoint = "serveractivate-complete";
        checkpoint.pending_write_count = gate.pending_write_count;
        checkpoint.checkpoint_satisfied = false;
        checkpoint.gate_eligible_at_checkpoint = true;
        checkpoint.gate_open = gate.gate_open;
        checkpoint.deferred = gate.deferred;
        checkpoint.runtime_write_suppressed = gate.runtime_write_suppressed;
        checkpoint.checkpoint_ready = true;
        checkpoint.action = "no-op player transfer gate-open checkpoint prepared";
        return checkpoint;
    }

    checkpoint.prepared = false;
    checkpoint.checkpoint_ready = false;
    checkpoint.short_circuit_reason = gate.short_circuit_reason;

    if (gate.skipped)
    {
        checkpoint.skipped = true;
        checkpoint.action = "player transfer gate-open checkpoint skipped";
        return checkpoint;
    }

    checkpoint.skipped = false;
    checkpoint.decision_source = "player-transfer-deferred-apply-gate";
    checkpoint.apply_target = gate.apply_target;
    checkpoint.current_map = gate.current_map;
    checkpoint.requested_map = gate.requested_map;
    checkpoint.future_apply_phase = gate.future_apply_phase;
    checkpoint.target_runtime_checkpoint = "serveractivate-complete";
    checkpoint.pending_write_count = gate.pending_write_count;
    checkpoint.checkpoint_satisfied = false;
    checkpoint.gate_eligible_at_checkpoint = gate.apply_gate_ready;
    checkpoint.gate_open = gate.gate_open;
    checkpoint.deferred = gate.deferred;
    checkpoint.runtime_write_suppressed = gate.runtime_write_suppressed;
    checkpoint.action = "player transfer gate-open checkpoint unavailable";
    return checkpoint;
}

hl::game_api::ChangeLevelTransitionSummary::ChangeLevelPlayerTransferCheckpointSignalContractSummary
BuildChangeLevelPlayerTransferCheckpointSignalContract(
    const hl::game_api::ChangeLevelTransitionSummary::
        ChangeLevelPlayerTransferGateOpenCheckpointSummary& checkpoint)
{
    hl::game_api::ChangeLevelTransitionSummary::
        ChangeLevelPlayerTransferCheckpointSignalContractSummary contract;
    contract.attempted = true;

    if (checkpoint.prepared && checkpoint.checkpoint_ready)
    {
        contract.prepared = true;
        contract.skipped = false;
        contract.decision_source = "player-transfer-gate-open-checkpoint";
        contract.apply_target = checkpoint.apply_target;
        contract.current_map = checkpoint.current_map;
        contract.requested_map = checkpoint.requested_map;
        contract.future_apply_phase = checkpoint.future_apply_phase;
        contract.target_runtime_checkpoint = checkpoint.target_runtime_checkpoint;
        contract.required_signal = checkpoint.target_runtime_checkpoint;
        contract.pending_write_count = checkpoint.pending_write_count;
        contract.signal_observed = false;
        contract.checkpoint_satisfied = checkpoint.checkpoint_satisfied;
        contract.gate_eligible_at_checkpoint = checkpoint.gate_eligible_at_checkpoint;
        contract.gate_open = checkpoint.gate_open;
        contract.deferred = checkpoint.deferred;
        contract.runtime_write_suppressed = checkpoint.runtime_write_suppressed;
        contract.signal_contract_ready = true;
        contract.action = "no-op player transfer checkpoint signal contract prepared";
        return contract;
    }

    contract.prepared = false;
    contract.signal_contract_ready = false;
    contract.short_circuit_reason = checkpoint.short_circuit_reason;

    if (checkpoint.skipped)
    {
        contract.skipped = true;
        contract.action = "player transfer checkpoint signal contract skipped";
        return contract;
    }

    contract.skipped = false;
    contract.decision_source = "player-transfer-gate-open-checkpoint";
    contract.apply_target = checkpoint.apply_target;
    contract.current_map = checkpoint.current_map;
    contract.requested_map = checkpoint.requested_map;
    contract.future_apply_phase = checkpoint.future_apply_phase;
    contract.target_runtime_checkpoint = checkpoint.target_runtime_checkpoint;
    contract.required_signal = checkpoint.target_runtime_checkpoint;
    contract.pending_write_count = checkpoint.pending_write_count;
    contract.signal_observed = false;
    contract.checkpoint_satisfied = checkpoint.checkpoint_satisfied;
    contract.gate_eligible_at_checkpoint = checkpoint.gate_eligible_at_checkpoint;
    contract.gate_open = checkpoint.gate_open;
    contract.deferred = checkpoint.deferred;
    contract.runtime_write_suppressed = checkpoint.runtime_write_suppressed;
    contract.action = "player transfer checkpoint signal contract unavailable";
    return contract;
}

void RefreshChangeLevelProjectedTransferSnapshot(EngineShimState& state)
{
    hl::game_api::ChangeLevelTransitionSummary& summary = state.changelevel_transition_state;
    if (!summary.changelevel_bootstrap_plan.attempted
        || !summary.changelevel_projected_carried_origin.attempted
        || !summary.changelevel_projected_carried_orientation.attempted)
    {
        return;
    }

    const hl::game_api::ChangeLevelTransitionSummary::ChangeLevelBootstrapPlanSummary& plan =
        summary.changelevel_bootstrap_plan;
    const hl::game_api::ChangeLevelTransitionSummary::ChangeLevelProjectedCarriedOriginSummary&
        projected_origin = summary.changelevel_projected_carried_origin;
    const hl::game_api::ChangeLevelTransitionSummary::ChangeLevelProjectedCarriedOrientationSummary&
        projected_orientation = summary.changelevel_projected_carried_orientation;
    summary.changelevel_projected_transfer_snapshot = BuildChangeLevelProjectedTransferSnapshot(
        plan,
        projected_origin,
        projected_orientation);
    RefreshChangeLevelPlayerTransferApplyPlan(state);
}

void RefreshChangeLevelPlayerTransferApplyPlan(EngineShimState& state)
{
    hl::game_api::ChangeLevelTransitionSummary& summary = state.changelevel_transition_state;
    if (!summary.changelevel_projected_transfer_snapshot.attempted)
    {
        return;
    }

    summary.changelevel_player_transfer_apply_plan = BuildChangeLevelPlayerTransferApplyPlan(
        summary.changelevel_projected_transfer_snapshot);
    RefreshChangeLevelPlayerTransferWriteSet(state);
}

void RefreshChangeLevelPlayerTransferWriteSet(EngineShimState& state)
{
    hl::game_api::ChangeLevelTransitionSummary& summary = state.changelevel_transition_state;
    if (!summary.changelevel_player_transfer_apply_plan.attempted)
    {
        return;
    }

    summary.changelevel_player_transfer_write_set = BuildChangeLevelPlayerTransferWriteSet(
        summary.changelevel_player_transfer_apply_plan);
    RefreshChangeLevelPlayerTransferDeferredApplyGate(state);
}

void RefreshChangeLevelPlayerTransferDeferredApplyGate(EngineShimState& state)
{
    hl::game_api::ChangeLevelTransitionSummary& summary = state.changelevel_transition_state;
    if (!summary.changelevel_player_transfer_write_set.attempted)
    {
        return;
    }

    summary.changelevel_player_transfer_deferred_apply_gate =
        BuildChangeLevelPlayerTransferDeferredApplyGate(
            summary.changelevel_player_transfer_write_set);
    RefreshChangeLevelPlayerTransferGateOpenCheckpoint(state);
}

void RefreshChangeLevelPlayerTransferGateOpenCheckpoint(EngineShimState& state)
{
    hl::game_api::ChangeLevelTransitionSummary& summary = state.changelevel_transition_state;
    if (!summary.changelevel_player_transfer_deferred_apply_gate.attempted)
    {
        return;
    }

    summary.changelevel_player_transfer_gate_open_checkpoint =
        BuildChangeLevelPlayerTransferGateOpenCheckpoint(
            summary.changelevel_player_transfer_deferred_apply_gate);
    RefreshChangeLevelPlayerTransferCheckpointSignalContract(state);
}

void RefreshChangeLevelPlayerTransferCheckpointSignalContract(EngineShimState& state)
{
    hl::game_api::ChangeLevelTransitionSummary& summary = state.changelevel_transition_state;
    if (!summary.changelevel_player_transfer_gate_open_checkpoint.attempted)
    {
        return;
    }

    summary.changelevel_player_transfer_checkpoint_signal_contract =
        BuildChangeLevelPlayerTransferCheckpointSignalContract(
            summary.changelevel_player_transfer_gate_open_checkpoint);
}

void CapturePendingChangeLevelRequest(
    EngineShimState& state,
    const hl::game_api::detail::EntityDefinition* definition,
    const RuntimeEntityRecord* record,
    int frame_number,
    float time,
    std::string_view detail)
{
    hl::game_api::ChangeLevelTransitionSummary& summary = state.changelevel_transition_state;
    NoteTriggerChangeLevelCandidate(summary, definition, record);
    if (summary.pending_request_captured)
    {
        return;
    }

    summary.pending_request_captured = true;
    summary.pending_request_frame = frame_number;
    summary.pending_request_time = time;
    summary.pending_request_detail = detail.empty()
        ? "captured as staged-safe no-op host changelevel request"
        : std::string(detail);
    EnsureChangeLevelTargetValidation(state);
}

void ConsumePendingChangeLevelRequest(EngineShimState& state)
{
    hl::game_api::ChangeLevelTransitionSummary& summary = state.changelevel_transition_state;
    if (!summary.pending_request_captured || summary.transition_intent_consumed)
    {
        return;
    }

    summary.transition_intent_captured = true;
    summary.transition_intent_consumed = true;
    summary.transition_intent_request_frame = summary.pending_request_frame;
    summary.transition_intent_request_time = summary.pending_request_time;
    summary.pre_changelevel_handoff.active = true;
    summary.pre_changelevel_handoff.handoff_latched = true;
    summary.pre_changelevel_handoff.world_frozen = false;
    summary.pre_changelevel_handoff.stop_requested = false;
    summary.pre_changelevel_handoff.request_frame = summary.transition_intent_request_frame;
    summary.pre_changelevel_handoff.request_time = summary.transition_intent_request_time;
    summary.transition_intent_action = "no-op handoff boundary";
    summary.transition_intent_detail =
        "pending_changelevel_request consumed into staged-safe host transition intent; latch-only continuation remains active and no map load performed";
    summary.pre_changelevel_handoff.world_state = "latched|handoff-ready";
    summary.pre_changelevel_handoff.action = "no-op handoff boundary";
    summary.pre_changelevel_handoff.map_load_performed = false;
    summary.pre_changelevel_handoff.detail =
        "staged pre-changelevel handoff latched for latch-only continuation; world remains unfrozen and no map load performed";
    EnsureChangeLevelLifecycleGate(state);
    RefreshChangeLevelLifecycleEntry(state);
    RefreshChangeLevelLifecycleDispatch(state);
    RefreshChangeLevelLifecycleExecution(state);
    RefreshChangeLevelBootstrapPlan(state);
    RefreshChangeLevelLandmarkTransform(state);
    RefreshChangeLevelProjectedCarriedOrigin(state);
    RefreshChangeLevelProjectedCarriedOrientation(state);
    RefreshChangeLevelProjectedTransferSnapshot(state);
}

const hl::game_api::detail::EntityDefinition* FindParsedEntityDefinitionByOrdinal(
    const EngineShimState& state,
    std::size_t ordinal)
{
    for (const hl::game_api::detail::EntityDefinition& definition : state.entity_bootstrap.parsed_entities)
    {
        if (definition.ordinal == ordinal)
        {
            return &definition;
        }
    }

    return nullptr;
}

bool IsCommonMapEntityKey(std::string_view key_name)
{
    return EqualsIgnoreCase(key_name, "classname")
        || EqualsIgnoreCase(key_name, "origin")
        || EqualsIgnoreCase(key_name, "angles")
        || EqualsIgnoreCase(key_name, "angle")
        || EqualsIgnoreCase(key_name, "targetname")
        || EqualsIgnoreCase(key_name, "spawnflags")
        || EqualsIgnoreCase(key_name, "model")
        || EqualsIgnoreCase(key_name, "renderamt")
        || EqualsIgnoreCase(key_name, "rendercolor")
        || EqualsIgnoreCase(key_name, "renderfx")
        || EqualsIgnoreCase(key_name, "rendermode");
}

bool ParseStrictFloat(std::string_view text, float* value);
bool ParseStrictInteger(std::string_view text, int* value);

int ParseTriggerStateUseType(const hl::game_api::detail::EntityDefinition& definition)
{
    const std::string* trigger_state = FindLastKeyValue(definition, "triggerstate");
    if (trigger_state == nullptr || trigger_state->empty())
    {
        return 1;
    }

    int value = 0;
    if (!ParseStrictInteger(*trigger_state, &value))
    {
        return 1;
    }

    switch (value)
    {
    case 0:
        return 0;
    case 2:
        return 3;
    default:
        return 1;
    }
}

struct MultiManagerOutputDefinition
{
    std::size_t order = 0;
    std::string target_name;
    float delay = 0.0f;
    bool valid_delay = true;
};

struct TriggerRelayDispatchDefinition
{
    std::string target_name;
    std::string kill_target_name;
    float delay = 0.0f;
    bool valid_delay = true;
    int use_type = 1;
};

std::vector<MultiManagerOutputDefinition> CollectMultiManagerOutputs(
    const hl::game_api::detail::EntityDefinition& definition)
{
    std::vector<MultiManagerOutputDefinition> outputs;
    outputs.reserve(definition.key_values.size());

    std::size_t order = 0;
    for (const hl::game_api::detail::EntityKeyValuePair& key_value : definition.key_values)
    {
        if (EqualsIgnoreCase(key_value.key, "wait") || IsCommonMapEntityKey(key_value.key))
        {
            continue;
        }

        MultiManagerOutputDefinition output;
        output.order = order++;
        output.target_name = TrimWhitespaceCopy(key_value.key);
        output.valid_delay = ParseStrictFloat(key_value.value, &output.delay);
        if (!output.valid_delay)
        {
            output.delay = 0.0f;
        }

        if (!output.target_name.empty())
        {
            outputs.push_back(std::move(output));
        }
    }

    std::stable_sort(
        outputs.begin(),
        outputs.end(),
        [](const MultiManagerOutputDefinition& left, const MultiManagerOutputDefinition& right)
        {
            if (left.delay != right.delay)
            {
                return left.delay < right.delay;
            }

            return left.order < right.order;
        });
    return outputs;
}

TriggerRelayDispatchDefinition CollectTriggerRelayDispatchDefinition(
    const hl::game_api::detail::EntityDefinition& definition)
{
    TriggerRelayDispatchDefinition dispatch;
    dispatch.use_type = ParseTriggerStateUseType(definition);

    if (const std::string* target = FindLastKeyValue(definition, "target");
        target != nullptr)
    {
        dispatch.target_name = TrimWhitespaceCopy(*target);
    }

    if (const std::string* killtarget = FindLastKeyValue(definition, "killtarget");
        killtarget != nullptr)
    {
        dispatch.kill_target_name = TrimWhitespaceCopy(*killtarget);
    }

    if (const std::string* raw_delay = FindLastKeyValue(definition, "delay");
        raw_delay != nullptr && !raw_delay->empty())
    {
        dispatch.valid_delay = ParseStrictFloat(*raw_delay, &dispatch.delay);
        if (!dispatch.valid_delay)
        {
            dispatch.delay = 0.0f;
        }
    }

    return dispatch;
}

const char* BootstrapUseTypeLabel(int use_type) noexcept
{
    switch (use_type)
    {
    case 0:
        return "USE_OFF";
    case 1:
        return "USE_ON";
    case 2:
        return "USE_SET";
    case 3:
    default:
        return "USE_TOGGLE";
    }
}

bool ParseStrictFloat(std::string_view text, float* value)
{
    if (value == nullptr)
    {
        return false;
    }

    std::istringstream stream{std::string(text)};
    float parsed_value = 0.0f;
    stream >> parsed_value;
    if (stream.fail())
    {
        return false;
    }

    stream >> std::ws;
    if (!stream.eof())
    {
        return false;
    }

    *value = parsed_value;
    return true;
}

bool ParseStrictInteger(std::string_view text, int* value)
{
    if (value == nullptr)
    {
        return false;
    }

    std::istringstream stream{std::string(text)};
    int parsed_value = 0;
    stream >> parsed_value;
    if (stream.fail())
    {
        return false;
    }

    stream >> std::ws;
    if (!stream.eof())
    {
        return false;
    }

    *value = parsed_value;
    return true;
}

struct ScriptedSequenceFields
{
    std::string target;
    std::string actor_name;
    std::string play;
    std::string idle;
    int move_to = 0;
    float radius = 0.0f;
    int spawnflags = 0;
};

struct TrackTrainFields
{
    float speed = 0.0f;
    float start_speed = 0.0f;
};

bool ParseStrictVector3(std::string_view text, Vector* value);
ParsedVectorField ParseAnglesField(const hl::game_api::detail::EntityDefinition& entity);
ParsedVectorField ParseOriginField(const hl::game_api::detail::EntityDefinition& entity);

struct BrushDoorFields
{
    float speed = 100.0f;
    float lip = 8.0f;
    float wait = 0.0f;
    int spawnflags = 0;
    float damage = 0.0f;
    bool damage_available = false;
    Vector movedir = Vector(0.0f, 0.0f, 0.0f);
    bool movedir_available = false;
};

float NormalizeDoorMoveDir(Vector* value)
{
    if (value == nullptr)
    {
        return 0.0f;
    }

    const float length = std::sqrt(
        (value->x * value->x)
        + (value->y * value->y)
        + (value->z * value->z));
    if (length <= 0.0001f)
    {
        return 0.0f;
    }

    value->x /= length;
    value->y /= length;
    value->z /= length;
    return length;
}

Vector ForwardVectorFromAngles(const Vector& angles)
{
    constexpr float kDegToRad = 3.14159265358979323846f / 180.0f;
    const float pitch = angles.x * kDegToRad;
    const float yaw = angles.y * kDegToRad;
    const float cp = std::cos(pitch);
    const float sp = std::sin(pitch);
    const float cy = std::cos(yaw);
    const float sy = std::sin(yaw);
    return Vector(cp * cy, cp * sy, -sp);
}

BrushDoorFields ExtractBrushDoorFields(const hl::game_api::detail::EntityDefinition* definition)
{
    BrushDoorFields fields;
    if (definition == nullptr)
    {
        return fields;
    }

    if (const std::string* value = FindLastKeyValue(*definition, "speed");
        value != nullptr)
    {
        ParseStrictFloat(*value, &fields.speed);
    }
    if (const std::string* value = FindLastKeyValue(*definition, "lip");
        value != nullptr)
    {
        ParseStrictFloat(*value, &fields.lip);
    }
    if (const std::string* value = FindLastKeyValue(*definition, "wait");
        value != nullptr)
    {
        ParseStrictFloat(*value, &fields.wait);
    }
    if (const std::string* value = FindLastKeyValue(*definition, "spawnflags");
        value != nullptr)
    {
        ParseStrictInteger(*value, &fields.spawnflags);
    }
    if (const std::string* value = FindLastKeyValue(*definition, "dmg");
        value != nullptr)
    {
        fields.damage_available = ParseStrictFloat(*value, &fields.damage);
    }

    if (const std::string* raw_movedir = FindLastKeyValue(*definition, "movedir");
        raw_movedir != nullptr)
    {
        fields.movedir_available = ParseStrictVector3(*raw_movedir, &fields.movedir);
    }
    else
    {
        const ParsedVectorField angles = ParseAnglesField(*definition);
        if (angles.present && angles.valid)
        {
            fields.movedir = ForwardVectorFromAngles(angles.value);
            fields.movedir_available = NormalizeDoorMoveDir(&fields.movedir) > 0.0001f;
        }
    }

    if (fields.movedir_available)
    {
        NormalizeDoorMoveDir(&fields.movedir);
    }

    return fields;
}

ScriptedSequenceFields ExtractScriptedSequenceFields(
    const hl::game_api::detail::EntityDefinition* definition)
{
    ScriptedSequenceFields fields;
    if (definition == nullptr)
    {
        return fields;
    }

    if (const std::string* target = FindLastKeyValue(*definition, "target");
        target != nullptr)
    {
        fields.target = *target;
    }
    if (const std::string* actor = FindLastKeyValue(*definition, "m_iszEntity");
        actor != nullptr)
    {
        fields.actor_name = *actor;
    }
    if (const std::string* play = FindLastKeyValue(*definition, "m_iszPlay");
        play != nullptr)
    {
        fields.play = *play;
    }
    if (const std::string* idle = FindLastKeyValue(*definition, "m_iszIdle");
        idle != nullptr)
    {
        fields.idle = *idle;
    }
    if (const std::string* move_to = FindLastKeyValue(*definition, "m_fMoveTo");
        move_to != nullptr)
    {
        ParseStrictInteger(*move_to, &fields.move_to);
    }
    if (const std::string* radius = FindLastKeyValue(*definition, "m_flRadius");
        radius != nullptr)
    {
        ParseStrictFloat(*radius, &fields.radius);
    }
    if (const std::string* spawnflags = FindLastKeyValue(*definition, "spawnflags");
        spawnflags != nullptr)
    {
        ParseStrictInteger(*spawnflags, &fields.spawnflags);
    }

    return fields;
}

float ExtractEntityDelay(
    const hl::game_api::detail::EntityDefinition* definition)
{
    if (definition == nullptr)
    {
        return 0.0f;
    }

    float delay = 0.0f;
    if (const std::string* value = FindLastKeyValue(*definition, "delay");
        value != nullptr)
    {
        ParseStrictFloat(*value, &delay);
    }

    return delay;
}

float ExtractPathTrackSpeed(
    const hl::game_api::detail::EntityDefinition* definition)
{
    if (definition == nullptr)
    {
        return 0.0f;
    }

    float speed = 0.0f;
    if (const std::string* value = FindLastKeyValue(*definition, "speed");
        value != nullptr)
    {
        ParseStrictFloat(*value, &speed);
    }

    return speed;
}

TrackTrainFields ExtractTrackTrainFields(
    const hl::game_api::detail::EntityDefinition* definition)
{
    TrackTrainFields fields;
    if (definition == nullptr)
    {
        return fields;
    }

    if (const std::string* value = FindLastKeyValue(*definition, "speed");
        value != nullptr)
    {
        ParseStrictFloat(*value, &fields.speed);
    }
    if (const std::string* value = FindLastKeyValue(*definition, "startspeed");
        value != nullptr)
    {
        ParseStrictFloat(*value, &fields.start_speed);
    }
    if (fields.start_speed <= 0.0f)
    {
        fields.start_speed = fields.speed;
    }
    return fields;
}

bool IsPassiveActorClass(std::string_view classname)
{
    const std::string normalized = ToLowerCopy(classname);
    return normalized == "monster_scientist"
        || normalized == "monster_sitting_scientist"
        || normalized == "monster_barney";
}

const RuntimeEntityRecord* FindRuntimeRecordByTargetname(
    const EngineShimState& state,
    std::string_view targetname)
{
    if (targetname.empty())
    {
        return nullptr;
    }

    for (const RuntimeEntityRecord& record : state.entity_bootstrap.runtime_entities)
    {
        if (record.targetname.empty())
        {
            continue;
        }

        if (EqualsIgnoreCase(record.targetname, targetname))
        {
            return &record;
        }
    }

    return nullptr;
}

const RuntimeEntityRecord* FindRuntimeRecordByParseIndex(
    const EngineShimState& state,
    std::size_t parse_index)
{
    for (const RuntimeEntityRecord& record : state.entity_bootstrap.runtime_entities)
    {
        if (record.parse_index == parse_index)
        {
            return &record;
        }
    }

    return nullptr;
}

bool IsLiveRuntimeTargetRecord(const RuntimeEntityRecord& record) noexcept
{
    return record.edict_index >= 0
        && record.in_use
        && !record.removed
        && (record.flags & FL_KILLME) == 0;
}

std::string BuildParsedRuntimeAuditDetail(const RuntimeEntityRecord& record)
{
    return "runtimeAudit parsed#" + std::to_string(record.parse_index)
        + " edict#" + std::to_string(record.edict_index)
        + " live=" + BoolToYesNo(IsLiveRuntimeTargetRecord(record))
        + " inUse=" + BoolToYesNo(record.in_use)
        + " removed=" + BoolToYesNo(record.removed)
        + " deferred=" + BoolToYesNo(record.deferred)
        + " support=" + std::string(RuntimeEntitySupportStateLabel(record.support_state))
        + " lifecycle=" + std::string(RuntimeEntityLifecycleStateLabel(record.lifecycle_state))
        + " note=" + (record.note.empty() ? std::string("<none>") : record.note);
}

hl::game_api::detail::BrushDoorEntityView BuildBrushDoorEntityView(
    const EngineShimState& state,
    const RuntimeEntityRecord& record)
{
    hl::game_api::detail::BrushDoorEntityView view;
    view.edict_index = record.edict_index;
    view.parse_index = record.parse_index;
    view.classname = record.classname;
    view.targetname = record.targetname;
    view.model = record.model;
    view.modelindex = record.modelindex;
    view.origin = record.origin;
    view.has_origin = record.has_origin;
    view.angles = record.angles;
    view.has_angles = record.has_angles;
    view.movedir = record.movedir;
    view.has_movedir = record.has_movedir;
    view.mins = record.mins;
    view.maxs = record.maxs;
    view.has_size = record.has_size;
    view.health = record.health;
    view.health_available = record.health_available;
    view.in_use = record.in_use;
    view.removed = record.removed || (record.flags & FL_KILLME) != 0;
    view.deferred = record.deferred;
    view.has_private_data = record.has_private_data;
    view.lifecycle = RuntimeEntityLifecycleStateLabel(record.lifecycle_state);

    const hl::game_api::detail::EntityDefinition* definition =
        FindParsedEntityDefinitionByOrdinal(state, record.parse_index);
    const BrushDoorFields fields = ExtractBrushDoorFields(definition);
    view.speed = fields.speed;
    view.lip = fields.lip;
    view.wait = fields.wait;
    view.spawnflags = fields.spawnflags;
    view.damage = fields.damage;
    view.damage_available = fields.damage_available;
    if (!view.has_movedir && fields.movedir_available)
    {
        view.movedir = fields.movedir;
        view.has_movedir = true;
    }

    if (definition != nullptr)
    {
        const ParsedVectorField origin = ParseOriginField(*definition);
        if (!view.has_origin && origin.present && origin.valid)
        {
            view.origin = origin.value;
            view.has_origin = true;
        }

        if (!view.has_angles)
        {
            const ParsedVectorField angles = ParseAnglesField(*definition);
            if (angles.present && angles.valid)
            {
                view.angles = angles.value;
                view.has_angles = true;
            }
        }
    }

    auto try_parse_inline_model_index =
        [](std::string_view model_name, int* index)
        {
            if (index == nullptr || model_name.size() < 2 || model_name.front() != '*')
            {
                return false;
            }

            return ParseStrictInteger(model_name.substr(1), index);
        };
    int inline_model_index = -1;
    if (try_parse_inline_model_index(view.model, &inline_model_index)
        && inline_model_index >= 0
        && static_cast<std::size_t>(inline_model_index) < state.world_context.inline_models.size())
    {
        const hl::game_api::detail::BspInlineModelBounds& bounds =
            state.world_context.inline_models[static_cast<std::size_t>(inline_model_index)];
        if (bounds.valid)
        {
            if (!view.has_size)
            {
                view.mins = bounds.mins;
                view.maxs = bounds.maxs;
                view.has_size = true;
            }

            if (!view.has_origin)
            {
                // GoldSrc brush entities often sit at 0 0 0 while their inline BSP model
                // supplies the practical staged movement frame of reference.
                view.origin = bounds.origin;
                view.has_origin = true;
            }
        }
    }

    if (record.deferred && fields.movedir_available)
    {
        view.movedir = fields.movedir;
        view.has_movedir = true;
    }

    return view;
}

struct ChangeLevelTouchBounds
{
    bool valid = false;
    Vector absmin = Vector(0.0f, 0.0f, 0.0f);
    Vector absmax = Vector(0.0f, 0.0f, 0.0f);
};

struct ChangeLevelKinematicAnchorState
{
    bool valid = false;
    int edict_index = -1;
    Vector origin = Vector(0.0f, 0.0f, 0.0f);
    float yaw = 0.0f;
};

bool BoundsOverlap(
    const ChangeLevelTouchBounds& left,
    const ChangeLevelTouchBounds& right) noexcept
{
    return left.valid
        && right.valid
        && left.absmin.x <= right.absmax.x
        && left.absmax.x >= right.absmin.x
        && left.absmin.y <= right.absmax.y
        && left.absmax.y >= right.absmin.y
        && left.absmin.z <= right.absmax.z
        && left.absmax.z >= right.absmin.z;
}

float BoundsSeparationDistance(
    const ChangeLevelTouchBounds& left,
    const ChangeLevelTouchBounds& right) noexcept
{
    if (!left.valid || !right.valid)
    {
        return -1.0f;
    }

    const float dx = std::max(
        0.0f,
        std::max(left.absmin.x - right.absmax.x, right.absmin.x - left.absmax.x));
    const float dy = std::max(
        0.0f,
        std::max(left.absmin.y - right.absmax.y, right.absmin.y - left.absmax.y));
    const float dz = std::max(
        0.0f,
        std::max(left.absmin.z - right.absmax.z, right.absmin.z - left.absmax.z));
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

Vector RotateOffsetAroundYaw(const Vector& offset, float yaw_delta) noexcept
{
    constexpr float kDegToRad = 3.14159265358979323846f / 180.0f;
    const float radians = yaw_delta * kDegToRad;
    const float cosine = std::cos(radians);
    const float sine = std::sin(radians);
    return Vector(
        offset.x * cosine - offset.y * sine,
        offset.x * sine + offset.y * cosine,
        offset.z);
}

bool IsReasonableTouchCoordinate(float value) noexcept
{
    return std::isfinite(value) && std::fabs(value) < 1000000.0f;
}

bool IsReasonableTouchOrigin(const Vector& value) noexcept
{
    return IsReasonableTouchCoordinate(value.x)
        && IsReasonableTouchCoordinate(value.y)
        && IsReasonableTouchCoordinate(value.z);
}

bool IsReasonableTouchSize(const Vector& mins, const Vector& maxs) noexcept
{
    return IsReasonableTouchCoordinate(mins.x)
        && IsReasonableTouchCoordinate(mins.y)
        && IsReasonableTouchCoordinate(mins.z)
        && IsReasonableTouchCoordinate(maxs.x)
        && IsReasonableTouchCoordinate(maxs.y)
        && IsReasonableTouchCoordinate(maxs.z)
        && mins.x <= maxs.x
        && mins.y <= maxs.y
        && mins.z <= maxs.z;
}

bool TryResolveTouchBoundsFromSnapshot(
    const hl::game_api::detail::EntityVarSnapshot& snapshot,
    ChangeLevelTouchBounds* bounds)
{
    if (bounds == nullptr)
    {
        return false;
    }

    *bounds = {};
    if (!snapshot.valid || !snapshot.in_use || snapshot.removed)
    {
        return false;
    }

    if (!snapshot.has_origin && !snapshot.has_size)
    {
        return false;
    }

    entvars_t vars{};
    vars.origin = snapshot.origin;
    vars.mins = snapshot.mins;
    vars.maxs = snapshot.maxs;
    vars.solid = snapshot.solid;
    vars.angles = snapshot.angles;
    ComputeFallbackAbsBox(vars);

    bounds->valid = true;
    bounds->absmin = vars.absmin;
    bounds->absmax = vars.absmax;
    return true;
}

bool TryResolveTriggerChangeLevelTouchBounds(
    const EngineShimState& state,
    const RuntimeEntityRecord& record,
    ChangeLevelTouchBounds* bounds)
{
    if (bounds == nullptr)
    {
        return false;
    }

    *bounds = {};

    Vector origin = record.origin;
    bool has_origin = record.has_origin;
    Vector angles = record.angles;
    bool has_angles = record.has_angles;
    Vector mins = record.mins;
    Vector maxs = record.maxs;
    bool has_size = record.has_size;
    int solid = record.solid;
    std::string model = record.model;

    const edict_t* entity = state.edict_store.EntityOfIndex(record.edict_index);
    if (entity != nullptr)
    {
        hl::game_api::detail::EntityVarSnapshot snapshot;
        hl::game_api::detail::TryReadEntityVars(
            state.edict_store,
            state.string_pool,
            entity,
            &snapshot,
            {});
        if (snapshot.valid)
        {
            if (!has_origin && snapshot.has_origin)
            {
                origin = snapshot.origin;
                has_origin = true;
            }
            if (!has_angles && snapshot.has_angles)
            {
                angles = snapshot.angles;
                has_angles = true;
            }
            if (!has_size && snapshot.has_size)
            {
                mins = snapshot.mins;
                maxs = snapshot.maxs;
                has_size = true;
            }
            if (solid == 0 && snapshot.solid != 0)
            {
                solid = snapshot.solid;
            }
        }

        const hl::game_api::detail::EntityStateSnapshot store_snapshot =
            state.edict_store.SnapshotOf(entity, state.string_pool);
        if (model.empty() && !store_snapshot.model_string.empty())
        {
            model = store_snapshot.model_string;
        }
        if (!has_origin && store_snapshot.has_origin)
        {
            origin = store_snapshot.origin;
            has_origin = true;
        }
        if (!has_size && store_snapshot.has_size)
        {
            mins = store_snapshot.mins;
            maxs = store_snapshot.maxs;
            has_size = true;
        }
    }

    if (has_origin && !IsReasonableTouchOrigin(origin))
    {
        has_origin = false;
    }
    if (has_size && !IsReasonableTouchSize(mins, maxs))
    {
        has_size = false;
    }

    if (const hl::game_api::detail::EntityDefinition* definition =
            FindParsedEntityDefinitionByOrdinal(state, record.parse_index);
        definition != nullptr)
    {
        const ParsedVectorField parsed_origin = ParseOriginField(*definition);
        if (!has_origin && parsed_origin.present && parsed_origin.valid)
        {
            origin = parsed_origin.value;
            has_origin = true;
        }
    }

    int inline_model_index = -1;
    if (model.size() >= 2
        && model.front() == '*'
        && ParseStrictInteger(model.substr(1), &inline_model_index)
        && inline_model_index >= 0
        && static_cast<std::size_t>(inline_model_index) < state.world_context.inline_models.size())
    {
        const hl::game_api::detail::BspInlineModelBounds& inline_model =
            state.world_context.inline_models[static_cast<std::size_t>(inline_model_index)];
        if (inline_model.valid)
        {
            if (!has_size)
            {
                mins = inline_model.mins;
                maxs = inline_model.maxs;
                has_size = true;
            }
            if (!has_origin)
            {
                origin = inline_model.origin;
                has_origin = true;
            }
        }
    }

    if (!has_origin && !has_size)
    {
        return false;
    }

    entvars_t vars{};
    vars.origin = origin;
    vars.mins = mins;
    vars.maxs = maxs;
    vars.solid = solid;
    vars.angles = has_angles ? angles : Vector(0.0f, 0.0f, 0.0f);
    if (vars.solid == 0 && !model.empty() && model.front() == '*')
    {
        vars.solid = SOLID_BSP;
    }
    ComputeFallbackAbsBox(vars);

    bounds->valid = true;
    bounds->absmin = vars.absmin;
    bounds->absmax = vars.absmax;
    return true;
}

bool TryResolveSinglePlayerActivatorSurrogateOrigin(
    const EngineShimState& state,
    Vector* origin,
    std::string* detail)
{
    if (origin == nullptr)
    {
        return false;
    }

    *origin = Vector(0.0f, 0.0f, 0.0f);
    if (detail != nullptr)
    {
        detail->clear();
    }

    const RuntimeEntityRecord* player_start = nullptr;
    for (const RuntimeEntityRecord& record : state.entity_bootstrap.runtime_entities)
    {
        if (EqualsIgnoreCase(record.classname, "info_player_start"))
        {
            player_start = &record;
            break;
        }
    }

    if (player_start == nullptr)
    {
        return false;
    }

    Vector resolved_origin = player_start->origin;
    bool has_origin = player_start->has_origin;
    if (const hl::game_api::detail::EntityDefinition* definition =
            FindParsedEntityDefinitionByOrdinal(state, player_start->parse_index);
        definition != nullptr)
    {
        const ParsedVectorField parsed_origin = ParseOriginField(*definition);
        if (!has_origin && parsed_origin.present && parsed_origin.valid)
        {
            resolved_origin = parsed_origin.value;
            has_origin = true;
        }
    }

    if (!has_origin && player_start->edict_index >= 0)
    {
        if (const edict_t* entity = state.edict_store.EntityOfIndex(player_start->edict_index);
            entity != nullptr)
        {
            hl::game_api::detail::EntityVarSnapshot snapshot;
            hl::game_api::detail::TryReadEntityVars(
                state.edict_store,
                state.string_pool,
                entity,
                &snapshot,
                {});
            if (snapshot.valid && snapshot.has_origin)
            {
                resolved_origin = snapshot.origin;
                has_origin = true;
            }
        }
    }

    if (!has_origin)
    {
        return false;
    }

    *origin = resolved_origin;
    if (detail != nullptr)
    {
        *detail = "info_player_start origin=" + FormatVector(resolved_origin);
    }
    return true;
}

bool TryResolveSinglePlayerActivatorSurrogateBounds(
    const EngineShimState& state,
    ChangeLevelTouchBounds* bounds,
    std::string* detail)
{
    if (bounds == nullptr)
    {
        return false;
    }

    *bounds = {};

    Vector origin;
    if (!TryResolveSinglePlayerActivatorSurrogateOrigin(state, &origin, detail))
    {
        return false;
    }

    entvars_t vars{};
    vars.origin = origin;
    vars.mins = Vector(-16.0f, -16.0f, -36.0f);
    vars.maxs = Vector(16.0f, 16.0f, 36.0f);
    vars.solid = SOLID_SLIDEBOX;
    ComputeFallbackAbsBox(vars);

    bounds->valid = true;
    bounds->absmin = vars.absmin;
    bounds->absmax = vars.absmax;
    return true;
}

bool TryResolveTrackTrainAnchorState(
    const EngineShimState& state,
    std::string_view targetname,
    ChangeLevelKinematicAnchorState* anchor,
    std::string* detail)
{
    if (anchor == nullptr)
    {
        return false;
    }

    *anchor = {};
    if (detail != nullptr)
    {
        detail->clear();
    }

    const RuntimeEntityRecord* record = FindRuntimeRecordByTargetname(state, targetname);
    if (record == nullptr)
    {
        return false;
    }

    Vector origin = record->origin;
    bool has_origin = record->has_origin;
    Vector angles = record->angles;
    bool has_angles = record->has_angles;
    if (record->edict_index >= 0)
    {
        if (const edict_t* entity = state.edict_store.EntityOfIndex(record->edict_index);
            entity != nullptr)
        {
            hl::game_api::detail::EntityVarSnapshot snapshot;
            hl::game_api::detail::TryReadEntityVars(
                state.edict_store,
                state.string_pool,
                entity,
                &snapshot,
                {});
            if (snapshot.valid)
            {
                if (snapshot.has_origin)
                {
                    origin = snapshot.origin;
                    has_origin = true;
                }
                if (snapshot.has_angles)
                {
                    angles = snapshot.angles;
                    has_angles = true;
                }
            }

            const hl::game_api::detail::EntityStateSnapshot store_snapshot =
                state.edict_store.SnapshotOf(entity, state.string_pool);
            if (!has_origin && store_snapshot.has_origin)
            {
                origin = store_snapshot.origin;
                has_origin = true;
            }
            if (!has_angles && store_snapshot.has_angles)
            {
                angles = store_snapshot.angles;
                has_angles = true;
            }
        }
    }

    if (const hl::game_api::detail::EntityDefinition* definition =
            FindParsedEntityDefinitionByOrdinal(state, record->parse_index);
        definition != nullptr)
    {
        const ParsedVectorField parsed_origin = ParseOriginField(*definition);
        if (!has_origin && parsed_origin.present && parsed_origin.valid)
        {
            origin = parsed_origin.value;
            has_origin = true;
        }

        const ParsedVectorField parsed_angles = ParseAnglesField(*definition);
        if (!has_angles && parsed_angles.present && parsed_angles.valid)
        {
            angles = parsed_angles.value;
            has_angles = true;
        }
    }

    if (!has_origin)
    {
        return false;
    }

    anchor->valid = true;
    anchor->edict_index = record->edict_index;
    anchor->origin = origin;
    anchor->yaw = has_angles ? angles.y : 0.0f;
    if (detail != nullptr)
    {
        *detail = std::string(targetname)
            + " origin=" + FormatVector(origin)
            + " yaw=" + std::to_string(anchor->yaw);
    }
    return true;
}

bool TryResolveKinematicSinglePlayerActivatorSurrogateBounds(
    const EngineShimState& state,
    hl::game_api::ChangeLevelTransitionSummary* summary,
    ChangeLevelTouchBounds* bounds,
    Vector* surrogate_origin,
    std::string* detail)
{
    if (summary == nullptr || bounds == nullptr)
    {
        return false;
    }

    *bounds = {};
    if (surrogate_origin != nullptr)
    {
        *surrogate_origin = Vector(0.0f, 0.0f, 0.0f);
    }
    if (detail != nullptr)
    {
        detail->clear();
    }

    Vector player_start_origin;
    std::string player_start_detail;
    if (!TryResolveSinglePlayerActivatorSurrogateOrigin(
            state,
            &player_start_origin,
            &player_start_detail))
    {
        return false;
    }

    ChangeLevelKinematicAnchorState anchor;
    if (!TryResolveTrackTrainAnchorState(state, "ftruck_a", &anchor, nullptr))
    {
        return false;
    }

    if (!summary->moving_surrogate_initialized)
    {
        summary->moving_surrogate_initialized = true;
        summary->moving_surrogate_initial_anchor_yaw = anchor.yaw;
        const Vector local_offset = player_start_origin - anchor.origin;
        summary->moving_surrogate_local_offset_x = local_offset.x;
        summary->moving_surrogate_local_offset_y = local_offset.y;
        summary->moving_surrogate_local_offset_z = local_offset.z;
        summary->moving_surrogate_local_offset_text = FormatVector(local_offset);
    }

    const Vector local_offset(
        summary->moving_surrogate_local_offset_x,
        summary->moving_surrogate_local_offset_y,
        summary->moving_surrogate_local_offset_z);
    const Vector rotated_offset = RotateOffsetAroundYaw(
        local_offset,
        anchor.yaw - summary->moving_surrogate_initial_anchor_yaw);
    const Vector origin = anchor.origin + rotated_offset;

    entvars_t vars{};
    vars.origin = origin;
    vars.mins = Vector(-16.0f, -16.0f, -36.0f);
    vars.maxs = Vector(16.0f, 16.0f, 36.0f);
    vars.solid = SOLID_SLIDEBOX;
    ComputeFallbackAbsBox(vars);

    bounds->valid = true;
    bounds->absmin = vars.absmin;
    bounds->absmax = vars.absmax;
    if (surrogate_origin != nullptr)
    {
        *surrogate_origin = origin;
    }
    if (detail != nullptr)
    {
        *detail = "anchor=ftruck_a origin=" + FormatVector(anchor.origin)
            + " yaw=" + std::to_string(anchor.yaw)
            + " localOffset=" + summary->moving_surrogate_local_offset_text
            + " surrogateOrigin=" + FormatVector(origin)
            + " start=" + player_start_detail;
    }
    return true;
}

void NoteTriggerChangeLevelGeometrySample(
    hl::game_api::ChangeLevelTransitionSummary& summary,
    const ChangeLevelTouchBounds& trigger_bounds,
    const ChangeLevelTouchBounds& surrogate_bounds,
    const Vector& surrogate_origin,
    int frame_number,
    float time)
{
    if (!trigger_bounds.valid || !surrogate_bounds.valid)
    {
        return;
    }

    summary.trigger_bounds_absmin_x = trigger_bounds.absmin.x;
    summary.trigger_bounds_absmin_y = trigger_bounds.absmin.y;
    summary.trigger_bounds_absmin_z = trigger_bounds.absmin.z;
    summary.trigger_bounds_absmax_x = trigger_bounds.absmax.x;
    summary.trigger_bounds_absmax_y = trigger_bounds.absmax.y;
    summary.trigger_bounds_absmax_z = trigger_bounds.absmax.z;
    summary.trigger_bounds_text =
        FormatVector(trigger_bounds.absmin) + ".." + FormatVector(trigger_bounds.absmax);
    if (summary.moving_surrogate_samples == 0)
    {
        summary.surrogate_path_absmin_x = surrogate_bounds.absmin.x;
        summary.surrogate_path_absmin_y = surrogate_bounds.absmin.y;
        summary.surrogate_path_absmin_z = surrogate_bounds.absmin.z;
        summary.surrogate_path_absmax_x = surrogate_bounds.absmax.x;
        summary.surrogate_path_absmax_y = surrogate_bounds.absmax.y;
        summary.surrogate_path_absmax_z = surrogate_bounds.absmax.z;
    }
    else
    {
        summary.surrogate_path_absmin_x =
            std::min(summary.surrogate_path_absmin_x, surrogate_bounds.absmin.x);
        summary.surrogate_path_absmin_y =
            std::min(summary.surrogate_path_absmin_y, surrogate_bounds.absmin.y);
        summary.surrogate_path_absmin_z =
            std::min(summary.surrogate_path_absmin_z, surrogate_bounds.absmin.z);
        summary.surrogate_path_absmax_x =
            std::max(summary.surrogate_path_absmax_x, surrogate_bounds.absmax.x);
        summary.surrogate_path_absmax_y =
            std::max(summary.surrogate_path_absmax_y, surrogate_bounds.absmax.y);
        summary.surrogate_path_absmax_z =
            std::max(summary.surrogate_path_absmax_z, surrogate_bounds.absmax.z);
    }
    summary.surrogate_path_envelope_text =
        FormatVector(
            Vector(
                summary.surrogate_path_absmin_x,
                summary.surrogate_path_absmin_y,
                summary.surrogate_path_absmin_z))
        + ".."
        + FormatVector(
            Vector(
                summary.surrogate_path_absmax_x,
                summary.surrogate_path_absmax_y,
                summary.surrogate_path_absmax_z));
    ++summary.moving_surrogate_samples;

    const float distance = BoundsSeparationDistance(trigger_bounds, surrogate_bounds);
    if (summary.closest_approach_distance < 0.0f
        || (distance >= 0.0f && distance < summary.closest_approach_distance))
    {
        summary.closest_approach_distance = distance;
        summary.closest_approach_frame = frame_number;
        summary.closest_approach_time = time;
        summary.closest_surrogate_origin_text = FormatVector(surrogate_origin);
    }
}

void ObserveTriggerChangeLevelTouchState(
    EngineShimState& state,
    int frame_number,
    float time)
{
    hl::game_api::ChangeLevelTransitionSummary& summary = state.changelevel_transition_state;
    if (!summary.candidate_present || summary.pending_request_captured)
    {
        return;
    }

    RuntimeEntityRecord* trigger_record = nullptr;
    for (RuntimeEntityRecord& record : state.entity_bootstrap.runtime_entities)
    {
        if (EqualsIgnoreCase(record.classname, "trigger_changelevel"))
        {
            trigger_record = &record;
            break;
        }
    }

    if (trigger_record == nullptr)
    {
        summary.pending_request_detail = "not exercised: no runtime trigger retained for touch observation";
        return;
    }

    ChangeLevelTouchBounds trigger_bounds;
    if (TryResolveTriggerChangeLevelTouchBounds(state, *trigger_record, &trigger_bounds))
    {
        summary.touch_bounds_resolved = true;
    }

    const int max_client_edict =
        std::min(state.server_state.maxclients, state.edict_store.NumberOfEntities() - 1);
    for (int edict_index = 1; edict_index <= max_client_edict; ++edict_index)
    {
        edict_t* activator_entity = state.edict_store.EntityOfIndex(edict_index);
        if (activator_entity == nullptr)
        {
            continue;
        }

        hl::game_api::detail::EntityVarSnapshot activator_snapshot;
        hl::game_api::detail::TryReadEntityVars(
            state.edict_store,
            state.string_pool,
            activator_entity,
            &activator_snapshot,
            {});
        if (!activator_snapshot.valid
            || !activator_snapshot.in_use
            || activator_snapshot.removed)
        {
            continue;
        }

        summary.eligible_activator_observed = true;

        ChangeLevelTouchBounds activator_bounds;
        if (!TryResolveTouchBoundsFromSnapshot(activator_snapshot, &activator_bounds))
        {
            continue;
        }

        if (!trigger_bounds.valid || !BoundsOverlap(trigger_bounds, activator_bounds))
        {
            continue;
        }

        summary.overlap_candidate_observed = true;
        CapturePendingChangeLevelRequest(
            state,
            FindParsedEntityDefinitionByOrdinal(state, trigger_record->parse_index),
            trigger_record,
            frame_number,
            time,
            "captured from staged-safe trigger_changelevel touch overlap with activator edict#"
                + std::to_string(edict_index)
                + " classname="
                + (activator_snapshot.classname.empty()
                    ? std::string("<empty>")
                    : activator_snapshot.classname)
                + "; no map load performed");
        return;
    }

    ChangeLevelTouchBounds surrogate_bounds;
    Vector surrogate_origin;
    std::string surrogate_detail;
    if (!summary.eligible_activator_observed
        && TryResolveKinematicSinglePlayerActivatorSurrogateBounds(
            state,
            &summary,
            &surrogate_bounds,
            &surrogate_origin,
            &surrogate_detail))
    {
        summary.surrogate_activator_available = true;
        summary.moving_surrogate_active = true;
        NoteTriggerChangeLevelGeometrySample(
            summary,
            trigger_bounds,
            surrogate_bounds,
            surrogate_origin,
            frame_number,
            time);
        if (trigger_bounds.valid && BoundsOverlap(trigger_bounds, surrogate_bounds))
        {
            summary.overlap_candidate_observed = true;
            CapturePendingChangeLevelRequest(
                state,
                FindParsedEntityDefinitionByOrdinal(state, trigger_record->parse_index),
                trigger_record,
                frame_number,
                time,
                "captured from staged-safe moving host-only single-player activator surrogate touch overlap ("
                    + surrogate_detail
                    + "); no map load performed");
            return;
        }

        summary.pending_request_detail = trigger_bounds.valid
            ? "not exercised: moving host-only single-player activator surrogate followed ftruck_a but no overlap candidate was observed"
            : "not exercised: moving host-only single-player activator surrogate available, but trigger bounds are unavailable";
        return;
    }

    if (!summary.eligible_activator_observed
        && TryResolveSinglePlayerActivatorSurrogateBounds(
            state,
            &surrogate_bounds,
            &surrogate_detail))
    {
        summary.surrogate_activator_available = true;
        summary.pending_request_detail = trigger_bounds.valid
            ? "not exercised: stationary host-only single-player activator surrogate ("
                + surrogate_detail + ") has no overlap candidate"
            : "not exercised: stationary host-only single-player activator surrogate available, but trigger bounds are unavailable";
        return;
    }

    if (!summary.touch_bounds_resolved)
    {
        summary.pending_request_detail =
            "not exercised: trigger bounds unavailable for staged touch observation";
    }
    else if (!summary.eligible_activator_observed)
    {
        summary.pending_request_detail =
            "not exercised: no eligible client activator observed";
    }
    else if (!summary.overlap_candidate_observed)
    {
        summary.pending_request_detail =
            "not exercised: eligible client activator observed but no overlap candidate observed";
    }
}

std::vector<hl::game_api::detail::BrushDoorEntityView> BuildBrushDoorEntityViews(
    const EngineShimState& state)
{
    std::vector<hl::game_api::detail::BrushDoorEntityView> views;
    for (const RuntimeEntityRecord& record : state.entity_bootstrap.runtime_entities)
    {
        if (!EqualsIgnoreCase(record.classname, "func_door"))
        {
            continue;
        }

        views.push_back(BuildBrushDoorEntityView(state, record));
    }

    return views;
}

struct BrushDoorDispatchAggregate
{
    bool seen = false;
    bool handled = false;
    bool deferred = false;
    bool failed = false;
    bool use_succeeded = false;
    bool state_changed = false;
    bool movement_started = false;
    bool movement_completed = false;
    bool native_use_attempted = false;
    bool native_use_succeeded = false;
    bool staged_handling_attempted = false;
    std::string dispatch_path;
    std::string support_state;
    std::string door_state;
    std::string blocked_reason;
    std::string runtime_audit;
    std::string detail;

    void Merge(const hl::game_api::detail::BrushDoorDispatchResult& result)
    {
        seen = true;
        handled = handled || result.handled;
        deferred = deferred || result.deferred;
        failed = failed || result.failed;
        use_succeeded = use_succeeded || result.use_succeeded;
        state_changed = state_changed || result.state_changed;
        movement_started = movement_started || result.movement_started;
        movement_completed = movement_completed || result.movement_completed;
        native_use_attempted = native_use_attempted || result.native_use_attempted;
        native_use_succeeded = native_use_succeeded || result.native_use_succeeded;
        staged_handling_attempted =
            staged_handling_attempted || result.staged_bootstrap_attempted;
        if (!result.dispatch_path.empty())
        {
            dispatch_path = result.dispatch_path;
        }
        if (!result.support_state.empty())
        {
            support_state = result.support_state;
        }
        if (!result.door_state.empty())
        {
            door_state = result.door_state;
        }
        if (!result.blocked_reason.empty())
        {
            blocked_reason = result.blocked_reason;
        }
        if (!result.audit_line.empty())
        {
            runtime_audit = result.audit_line;
        }
        if (!result.detail.empty())
        {
            detail = result.detail;
        }
    }
};

const hl::game_api::detail::EntityDefinition* FindParsedTargetDefinitionByTargetname(
    const EngineShimState& state,
    std::string_view target_name,
    std::string_view classname_filter = {})
{
    if (target_name.empty())
    {
        return nullptr;
    }

    for (const hl::game_api::detail::EntityDefinition& definition :
         state.entity_bootstrap.parsed_entities)
    {
        const std::string* definition_targetname = FindLastKeyValue(definition, "targetname");
        if (definition_targetname == nullptr
            || !EqualsIgnoreCase(*definition_targetname, target_name))
        {
            continue;
        }

        if (!classname_filter.empty()
            && !EqualsIgnoreCase(definition.classname, classname_filter))
        {
            continue;
        }

        return &definition;
    }

    return nullptr;
}

ScriptedPathGraph BuildScriptedPathGraph(const EngineShimState& state)
{
    ScriptedPathGraph graph;
    std::unordered_map<std::string, std::size_t> track_name_to_index;

    for (const RuntimeEntityRecord& record : state.entity_bootstrap.runtime_entities)
    {
        if (!EqualsIgnoreCase(record.classname, "path_track"))
        {
            continue;
        }

        PathTrackNodeDiagnostic node;
        node.parse_index = record.parse_index;
        node.edict_index = record.edict_index;
        node.targetname = record.targetname;
        node.next_target = record.target;

        if (const hl::game_api::detail::EntityDefinition* definition =
                FindParsedEntityDefinitionByOrdinal(state, record.parse_index);
            definition != nullptr)
        {
            if (const std::string* message = FindLastKeyValue(*definition, "message");
                message != nullptr)
            {
                node.message_target = *message;
            }
            if (const std::string* speed = FindLastKeyValue(*definition, "speed");
                speed != nullptr)
            {
                ParseStrictFloat(*speed, &node.speed);
            }
        }

        if (!node.targetname.empty())
        {
            track_name_to_index[node.targetname] = graph.nodes.size();
        }
        graph.nodes.push_back(std::move(node));
    }

    for (PathTrackNodeDiagnostic& node : graph.nodes)
    {
        if (!node.next_target.empty())
        {
            node.next_resolved = track_name_to_index.find(node.next_target) != track_name_to_index.end();
            if (!node.targetname.empty())
            {
                graph.track_to_previous_names[node.next_target].push_back(node.targetname);
            }
        }
        if (!node.message_target.empty())
        {
            graph.message_to_track_names[node.message_target].push_back(node.targetname);
            node.message_resolved = FindRuntimeRecordByTargetname(state, node.message_target) != nullptr;
        }
    }

    return graph;
}

std::string BuildSourceEntityLabel(
    const EngineShimState& state,
    int source_edict_index,
    std::string_view source_classname)
{
    for (const RuntimeEntityRecord& source : state.entity_bootstrap.runtime_entities)
    {
        if (source.edict_index != source_edict_index)
        {
            continue;
        }

        std::string label = "edict#" + std::to_string(source.edict_index)
            + " classname="
            + (source.classname.empty() ? std::string("<empty>") : source.classname);
        if (!source.targetname.empty())
        {
            label += " targetname=" + source.targetname;
        }
        return label;
    }

    return "edict#" + std::to_string(source_edict_index)
        + " classname="
        + (source_classname.empty() ? std::string("<empty>") : std::string(source_classname));
}

bool DidUseProgressState(
    const hl::game_api::detail::EntityVarSnapshot& before,
    const hl::game_api::detail::EntityVarSnapshot& after)
{
    if (!before.valid || !after.valid)
    {
        return false;
    }

    return before.nextthink != after.nextthink
        || before.ltime != after.ltime
        || before.target != after.target
        || before.targetname != after.targetname
        || before.flags != after.flags
        || before.effects != after.effects
        || before.origin.x != after.origin.x
        || before.origin.y != after.origin.y
        || before.origin.z != after.origin.z
        || before.angles.x != after.angles.x
        || before.angles.y != after.angles.y
        || before.angles.z != after.angles.z;
}

bool ShouldTreatAsScheduledFollowUp(
    const hl::game_api::detail::EntityVarSnapshot& before,
    const hl::game_api::detail::EntityVarSnapshot& after)
{
    return after.valid
        && after.nextthink > 0.0f
        && (!before.valid || before.nextthink != after.nextthink);
}

struct PathNodeTargetProbe
{
    int runtime_target_candidates = 0;
    int parsed_target_candidates = 0;
    std::vector<std::string> target_classnames;
    std::vector<std::string> resolved_target_details;
};

struct PathNodePresentationLinkageProbe
{
    int env_message_runtime_candidates = 0;
    int env_message_parsed_candidates = 0;
    int env_fade_runtime_candidates = 0;
    int env_fade_parsed_candidates = 0;
    bool actionable_env_message_runtime = false;
    std::vector<std::string> details;
};

struct PathNodeMessageEmitResult
{
    bool emitted = false;
    std::string channel_name;
    int message_id = 0;
    std::string semantics_summary;
    std::string detail;
};

struct PathNodePresentationDispatchResult
{
    bool dispatched = false;
    bool deferred = false;
    std::string dispatch_mode;
    std::string classification;
    std::string required_subsystem;
    bool fade_channel_available = false;
    bool fade_channel_used = false;
    bool message_channel_available = false;
    bool message_channel_used = false;
    bool env_message_linkage_found = false;
    bool env_message_linkage_used = false;
    bool summary_only_fallback_used = false;
    std::string presentation_linkage_detail;
    std::string semantics_summary;
    std::string detail;
};

struct PathNodeDownstreamSnapshot
{
    int target_chains_fired = 0;
    int target_resolutions = 0;
    int use_attempts = 0;
    int use_successes = 0;
    int use_deferred = 0;
    int use_failures = 0;
    int scheduled_created = 0;
    int scheduled_executed = 0;
    int scheduled_rescheduled = 0;
    int scheduled_pending = 0;
    int scripted_received_use = 0;
    int scripted_emitted_targets = 0;
    int scripted_scheduled_outputs = 0;
    int scripted_internal_state_changes = 0;
    int scripted_scheduled_follow_ups = 0;
    int scripted_progressed = 0;
    int multi_manager_activity = 0;
    int alert_callbacks = 0;
    int message_callbacks = 0;
    int movement_callbacks = 0;
    struct TrackedEntityState
    {
        int edict_index = -1;
        std::string classname;
        std::string targetname;
        std::size_t target_resolutions = 0;
        std::size_t use_successes = 0;
        std::size_t use_deferred = 0;
        std::size_t use_failures = 0;
        int pending_scheduled_outputs = 0;
        int scripted_received_use = 0;
        int scripted_emitted_targets = 0;
        int scripted_scheduled_outputs = 0;
        bool scripted_internal_state_changed = false;
        bool scripted_scheduled_follow_up = false;
        bool scripted_progressed = false;
        std::string scripted_support_state;
        std::string scripted_blocked_reason;
        std::string scripted_movement_stage;
        bool scripted_actor_resolved = false;
        bool scripted_arrived = false;
        bool scripted_animation_ready = false;
        bool brush_door_tracked = false;
        bool brush_door_native_use_attempted = false;
        bool brush_door_native_use_succeeded = false;
        bool brush_door_staged_attempted = false;
        bool brush_door_staged_used = false;
        bool brush_door_movement_started = false;
        bool brush_door_movement_completed = false;
        int brush_door_last_use_frame = -1;
        float brush_door_last_use_time = 0.0f;
        std::string brush_door_support_state;
        std::string brush_door_dispatch_path;
        std::string brush_door_state;
        std::string brush_door_blocked_reason;
        std::string brush_door_last_source_event;
        std::string brush_door_runtime_audit;
    };

    std::vector<TrackedEntityState> tracked_entities;
};

void AppendUniqueValue(std::vector<std::string>& values, std::string value)
{
    if (value.empty())
    {
        return;
    }

    if (std::find(values.begin(), values.end(), value) != values.end())
    {
        return;
    }

    values.push_back(std::move(value));
}

bool IsMessageCallbackName(std::string_view callback_name)
{
    return EqualsIgnoreCase(callback_name, "pfnAlertMessage")
        || EqualsIgnoreCase(callback_name, "pfnMessageBegin")
        || EqualsIgnoreCase(callback_name, "pfnMessageEnd")
        || EqualsIgnoreCase(callback_name, "pfnWriteByte")
        || EqualsIgnoreCase(callback_name, "pfnWriteChar")
        || EqualsIgnoreCase(callback_name, "pfnWriteShort")
        || EqualsIgnoreCase(callback_name, "pfnWriteLong")
        || EqualsIgnoreCase(callback_name, "pfnWriteAngle")
        || EqualsIgnoreCase(callback_name, "pfnWriteCoord")
        || EqualsIgnoreCase(callback_name, "pfnWriteString")
        || EqualsIgnoreCase(callback_name, "pfnWriteEntity")
        || EqualsIgnoreCase(callback_name, "pfnServerPrint");
}

bool IsMovementCallbackName(std::string_view callback_name)
{
    return EqualsIgnoreCase(callback_name, "pfnSetOrigin")
        || EqualsIgnoreCase(callback_name, "pfnWalkMove")
        || EqualsIgnoreCase(callback_name, "pfnChangeYaw")
        || EqualsIgnoreCase(callback_name, "pfnChangePitch");
}

bool IsKnownDeferredPathNodeMessage(std::string_view message)
{
    return EqualsIgnoreCase(message, "execute_sci");
}

bool IsKnownPresentationPathNodeMessage(std::string_view message)
{
    return EqualsIgnoreCase(message, "fade_out");
}

bool ShouldTrackPathNodeDownstreamEntity(const RuntimeEntityRecord& record)
{
    const std::string normalized = ToLowerCopy(record.classname);
    return normalized == "multi_manager"
        || normalized == "scripted_sequence"
        || normalized == "monster_barney"
        || normalized == "monster_scientist"
        || normalized == "monster_sitting_scientist"
        || normalized == "func_tracktrain"
        || normalized == "trigger_relay"
        || normalized == "func_door"
        || normalized == "env_message"
        || normalized == "ambient_generic";
}

PathNodeDownstreamSnapshot::TrackedEntityState CapturePathNodeTrackedEntityState(
    const RuntimeEntityRecord& record,
    const hl::game_api::detail::BrushDoorBootstrapController* brush_door_controller)
{
    PathNodeDownstreamSnapshot::TrackedEntityState state;
    state.edict_index = record.edict_index;
    state.classname = record.classname;
    state.targetname = record.targetname;
    state.target_resolutions = record.target_resolutions;
    state.use_successes = record.use_successes;
    state.use_deferred = record.use_deferred;
    state.use_failures = record.use_failures;
    state.pending_scheduled_outputs = record.pending_scheduled_outputs;
    state.scripted_received_use = record.scripted_logic.received_use_count;
    state.scripted_emitted_targets = record.scripted_logic.emitted_target_count;
    state.scripted_scheduled_outputs = record.scripted_logic.scheduled_output_count;
    state.scripted_internal_state_changed = record.scripted_logic.internal_state_changed;
    state.scripted_scheduled_follow_up = record.scripted_logic.scheduled_follow_up;
    state.scripted_progressed = record.scripted_logic.progressed_this_run;
    state.scripted_support_state =
        hl::game_api::detail::ScriptedLogicSupportStateLabel(record.scripted_logic.support_state);
    state.scripted_blocked_reason = record.scripted_logic.blocked_reason;
    state.scripted_movement_stage = record.scripted_movement_stage;
    state.scripted_actor_resolved = record.scripted_actor_resolved;
    state.scripted_arrived = record.scripted_arrived;
    state.scripted_animation_ready = record.scripted_animation_ready;
    if (brush_door_controller != nullptr)
    {
        if (const hl::game_api::BrushDoorRuntimeSummary* door =
                brush_door_controller->FindDoorState(record.edict_index);
            door != nullptr)
        {
            state.brush_door_tracked = true;
            state.brush_door_native_use_attempted = door->native_use_attempted;
            state.brush_door_native_use_succeeded = door->native_use_succeeded;
            state.brush_door_staged_attempted = door->staged_bootstrap_attempted;
            state.brush_door_staged_used = door->staged_bootstrap_used;
            state.brush_door_movement_started = door->movement_started;
            state.brush_door_movement_completed = door->movement_completed;
            state.brush_door_last_use_frame = door->last_use_frame;
            state.brush_door_last_use_time = door->last_use_time;
            state.brush_door_support_state = door->support_state;
            state.brush_door_dispatch_path = door->dispatch_path;
            state.brush_door_state = door->movement_state;
            state.brush_door_blocked_reason = door->blocked_reason;
            state.brush_door_last_source_event = door->last_source_event;
            state.brush_door_runtime_audit = door->audit_line;
        }
    }
    return state;
}

const PathNodeDownstreamSnapshot::TrackedEntityState* FindPathNodeTrackedEntityState(
    const PathNodeDownstreamSnapshot& snapshot,
    int edict_index)
{
    const auto it = std::find_if(
        snapshot.tracked_entities.begin(),
        snapshot.tracked_entities.end(),
        [&](const PathNodeDownstreamSnapshot::TrackedEntityState& candidate)
        {
            return candidate.edict_index == edict_index;
        });
    return it != snapshot.tracked_entities.end() ? &(*it) : nullptr;
}

std::string DescribePathNodeTrackedEntity(
    const PathNodeDownstreamSnapshot::TrackedEntityState& state)
{
    return (state.classname.empty() ? std::string("<empty>") : state.classname)
        + "/"
        + (state.targetname.empty() ? std::string("edict#" + std::to_string(state.edict_index))
                                    : state.targetname);
}

PathNodeTargetProbe ProbePathNodeTargets(
    const EngineShimState& state,
    std::string_view target_name)
{
    PathNodeTargetProbe probe;
    if (target_name.empty())
    {
        return probe;
    }

    for (const RuntimeEntityRecord& record : state.entity_bootstrap.runtime_entities)
    {
        if (!EqualsIgnoreCase(record.targetname, target_name))
        {
            continue;
        }

        const bool live_runtime_target = IsLiveRuntimeTargetRecord(record);
        if (!live_runtime_target)
        {
            continue;
        }

        ++probe.runtime_target_candidates;
        const std::string classname =
            record.classname.empty() ? std::string("<empty>") : record.classname;
        AppendUniqueValue(probe.target_classnames, classname);
        if (probe.resolved_target_details.size() < 8)
        {
            probe.resolved_target_details.push_back(
                "runtime edict#" + std::to_string(record.edict_index)
                + " classname=" + classname
                + " targetname="
                + (record.targetname.empty() ? std::string("<empty>") : record.targetname));
        }
    }

    for (const hl::game_api::detail::EntityDefinition& definition :
         state.entity_bootstrap.parsed_entities)
    {
        const std::string* definition_targetname = FindLastKeyValue(definition, "targetname");
        if (definition_targetname == nullptr || !EqualsIgnoreCase(*definition_targetname, target_name))
        {
            continue;
        }

        ++probe.parsed_target_candidates;
        const std::string classname = SafeClassname(definition);
        AppendUniqueValue(probe.target_classnames, classname);
        if (probe.resolved_target_details.size() < 8)
        {
            probe.resolved_target_details.push_back(
                "parsed#" + std::to_string(definition.ordinal)
                + " classname=" + classname
                + " targetname=" + *definition_targetname);
        }

        const RuntimeEntityRecord* parsed_runtime =
            FindRuntimeRecordByParseIndex(state, definition.ordinal);
        if (parsed_runtime == nullptr)
        {
            if (probe.resolved_target_details.size() < 8)
            {
                probe.resolved_target_details.push_back(
                    "runtimeAudit parsed#" + std::to_string(definition.ordinal)
                    + " runtimeRecordMissing");
            }
        }
        else if (!IsLiveRuntimeTargetRecord(*parsed_runtime)
            && probe.resolved_target_details.size() < 8)
        {
            probe.resolved_target_details.push_back(
                BuildParsedRuntimeAuditDetail(*parsed_runtime));
        }
    }

    return probe;
}

struct ParsedTriggerRelayBootstrapDispatchResult
{
    bool attempted = false;
    bool handled = false;
    bool deferred = false;
    bool failed = false;
    int synthetic_resolved_targets = 0;
    std::string detail;
    std::string required_subsystem;
};

ParsedTriggerRelayBootstrapDispatchResult TryDispatchParsedOnlyTriggerRelayBootstrap(
    EngineShimState& state,
    const hl::game_api::detail::PathNodeEventDispatchRequest& request,
    float time,
    float frametime,
    hl::game_api::detail::MapLogicDispatcher& dispatcher,
    const hl::game_api::detail::MapLogicDispatcherHooks& hooks)
{
    ParsedTriggerRelayBootstrapDispatchResult result;

    const hl::game_api::detail::EntityDefinition* definition =
        FindParsedTargetDefinitionByTargetname(state, request.message, "trigger_relay");
    if (definition == nullptr)
    {
        return result;
    }

    result.attempted = true;

    const RuntimeEntityRecord* runtime_record =
        FindRuntimeRecordByParseIndex(state, definition->ordinal);
    const TriggerRelayDispatchDefinition relay_dispatch =
        CollectTriggerRelayDispatchDefinition(*definition);
    const PathNodeTargetProbe target_probe =
        ProbePathNodeTargets(state, relay_dispatch.target_name);
    const PathNodeTargetProbe kill_probe =
        ProbePathNodeTargets(state, relay_dispatch.kill_target_name);

    bool target_queued = false;
    bool target_dispatched = false;

    if (!relay_dispatch.valid_delay)
    {
        result.deferred = true;
        result.required_subsystem = "trigger_relay delay parsing/bootstrap validation";
    }
    else if (!relay_dispatch.kill_target_name.empty()
        && (kill_probe.runtime_target_candidates > 0
            || kill_probe.parsed_target_candidates > 0))
    {
        result.deferred = true;
        result.required_subsystem = "safe staged killtarget removal semantics";
    }
    else if (!relay_dispatch.target_name.empty())
    {
        if (target_probe.runtime_target_candidates > 0)
        {
            if (relay_dispatch.delay > 0.0f)
            {
                hl::game_api::detail::ScheduledUseAction action;
                action.fire_time = time + std::max(
                    relay_dispatch.delay,
                    std::max(frametime, 0.05f));
                action.source_edict_index =
                    runtime_record != nullptr ? runtime_record->edict_index : -1;
                action.source_classname =
                    "trigger_relay(parsed#" + std::to_string(definition->ordinal) + ")";
                action.target_name = relay_dispatch.target_name;
                action.use_type = relay_dispatch.use_type;
                action.value = 0.0f;
                action.reason =
                    "path-node parsed-trigger_relay#" + std::to_string(definition->ordinal);
                target_queued = dispatcher.QueueAction(action, hooks);
                result.handled = target_queued;
                result.failed = !target_queued;
            }
            else
            {
                hl::game_api::detail::MapLogicDispatchContext relay_context;
                relay_context.frame_number = request.frame_number;
                relay_context.source_edict_index =
                    runtime_record != nullptr ? runtime_record->edict_index : -1;
                relay_context.source_classname =
                    "trigger_relay(parsed#" + std::to_string(definition->ordinal) + ")";
                relay_context.target_name = relay_dispatch.target_name;
                relay_context.use_type = relay_dispatch.use_type;
                relay_context.value = 0.0f;
                relay_context.reason =
                    "parsed-trigger_relay path-node node="
                    + (request.node_name.empty() ? std::string("<empty>") : request.node_name);
                target_dispatched = dispatcher.DispatchTargetChain(relay_context, hooks);
                result.handled = target_dispatched;
                result.failed = !target_dispatched;
            }
        }
        else if (target_probe.parsed_target_candidates > 0)
        {
            result.deferred = true;
            result.required_subsystem = "parsed-only relay target bootstrap dispatch semantics";
        }
        else
        {
            // Parsed relay was reached and evaluated, but it currently points at no targets.
            result.handled = true;
        }
    }
    else
    {
        // Parsed relay with no downstream target still counts as a staged bootstrap dispatch.
        result.handled = true;
    }

    if (result.handled)
    {
        result.synthetic_resolved_targets = 1;
    }

    result.detail =
        "parsed-only trigger_relay staged bootstrap dispatch"
        " parsed#"
        + std::to_string(definition->ordinal)
        + " runtimeAudit="
        + (runtime_record != nullptr
            ? BuildParsedRuntimeAuditDetail(*runtime_record)
            : std::string("runtimeRecordMissing"))
        + " useType=" + std::string(BootstrapUseTypeLabel(relay_dispatch.use_type))
        + " target="
        + (relay_dispatch.target_name.empty()
            ? std::string("<none>")
            : relay_dispatch.target_name)
        + " targetRuntimeMatches="
        + std::to_string(target_probe.runtime_target_candidates)
        + " targetParsedMatches="
        + std::to_string(target_probe.parsed_target_candidates)
        + " targetQueued=" + BoolToYesNo(target_queued)
        + " targetDispatched=" + BoolToYesNo(target_dispatched)
        + " killtarget="
        + (relay_dispatch.kill_target_name.empty()
            ? std::string("<none>")
            : relay_dispatch.kill_target_name)
        + " killRuntimeMatches="
        + std::to_string(kill_probe.runtime_target_candidates)
        + " killParsedMatches="
        + std::to_string(kill_probe.parsed_target_candidates)
        + " delay=" + std::to_string(relay_dispatch.delay)
        + " outcome="
        + (result.handled ? std::string("handled")
                          : result.deferred ? std::string("deferred")
                                            : result.failed ? std::string("failed")
                                                            : std::string("noop"));
    return result;
}

int CallbackCountDelta(
    const std::unordered_map<std::string, std::size_t>& callbacks,
    std::string_view callback_name)
{
    const auto it = callbacks.find(std::string(callback_name));
    if (it == callbacks.end())
    {
        return 0;
    }

    return static_cast<int>(it->second);
}

PathNodeDownstreamSnapshot CapturePathNodeDownstreamSnapshot(
    const EngineShimState& state,
    const hl::game_api::detail::MapLogicDispatcher& dispatcher,
    const std::unordered_map<std::string, std::size_t>& callback_deltas,
    const hl::game_api::detail::BrushDoorBootstrapController* brush_door_controller)
{
    PathNodeDownstreamSnapshot snapshot;
    const hl::game_api::MapLogicDispatcherStateSummary& map_logic_summary = dispatcher.Summary();
    snapshot.target_chains_fired = map_logic_summary.total_target_chains_fired;
    snapshot.target_resolutions = map_logic_summary.total_target_resolutions;
    snapshot.use_attempts = map_logic_summary.total_use_attempts;
    snapshot.use_successes = map_logic_summary.total_use_successes;
    snapshot.use_deferred = map_logic_summary.total_use_deferred;
    snapshot.use_failures = map_logic_summary.total_use_failures;
    snapshot.scheduled_created = map_logic_summary.total_scheduled_created;
    snapshot.scheduled_executed = map_logic_summary.total_scheduled_executed;
    snapshot.scheduled_rescheduled = map_logic_summary.total_scheduled_rescheduled;
    snapshot.scheduled_pending = dispatcher.PendingActionCount();

    for (const RuntimeEntityRecord& record : state.entity_bootstrap.runtime_entities)
    {
        snapshot.scripted_received_use += record.scripted_logic.received_use_count;
        snapshot.scripted_emitted_targets += record.scripted_logic.emitted_target_count;
        snapshot.scripted_scheduled_outputs += record.scripted_logic.scheduled_output_count;
        snapshot.scripted_internal_state_changes += record.scripted_logic.internal_state_changed ? 1 : 0;
        snapshot.scripted_scheduled_follow_ups += record.scripted_logic.scheduled_follow_up ? 1 : 0;
        snapshot.scripted_progressed += record.scripted_logic.progressed_this_run ? 1 : 0;
        if (EqualsIgnoreCase(record.classname, "multi_manager"))
        {
            snapshot.multi_manager_activity += static_cast<int>(record.use_successes + record.target_resolutions);
        }
        if (ShouldTrackPathNodeDownstreamEntity(record))
        {
            snapshot.tracked_entities.push_back(
                CapturePathNodeTrackedEntityState(record, brush_door_controller));
        }
    }

    std::sort(
        snapshot.tracked_entities.begin(),
        snapshot.tracked_entities.end(),
        [](const PathNodeDownstreamSnapshot::TrackedEntityState& left,
           const PathNodeDownstreamSnapshot::TrackedEntityState& right)
        {
            if (left.edict_index != right.edict_index)
            {
                return left.edict_index < right.edict_index;
            }
            if (left.classname != right.classname)
            {
                return left.classname < right.classname;
            }
            return left.targetname < right.targetname;
        });

    snapshot.alert_callbacks = CallbackCountDelta(callback_deltas, "pfnAlertMessage");
    snapshot.message_callbacks =
        CallbackCountDelta(callback_deltas, "pfnMessageBegin")
        + CallbackCountDelta(callback_deltas, "pfnMessageEnd")
        + CallbackCountDelta(callback_deltas, "pfnWriteByte")
        + CallbackCountDelta(callback_deltas, "pfnWriteChar")
        + CallbackCountDelta(callback_deltas, "pfnWriteShort")
        + CallbackCountDelta(callback_deltas, "pfnWriteLong")
        + CallbackCountDelta(callback_deltas, "pfnWriteAngle")
        + CallbackCountDelta(callback_deltas, "pfnWriteCoord")
        + CallbackCountDelta(callback_deltas, "pfnWriteString")
        + CallbackCountDelta(callback_deltas, "pfnWriteEntity")
        + CallbackCountDelta(callback_deltas, "pfnServerPrint");
    snapshot.movement_callbacks =
        CallbackCountDelta(callback_deltas, "pfnSetOrigin")
        + CallbackCountDelta(callback_deltas, "pfnWalkMove")
        + CallbackCountDelta(callback_deltas, "pfnChangeYaw")
        + CallbackCountDelta(callback_deltas, "pfnChangePitch");
    return snapshot;
}

void AppendDeltaSummary(
    std::vector<std::string>& parts,
    std::string_view label,
    int value)
{
    if (value <= 0)
    {
        return;
    }

    parts.push_back(std::string(label) + "=+" + std::to_string(value));
}

std::string JoinStringValues(const std::vector<std::string>& values)
{
    if (values.empty())
    {
        return "<none>";
    }

    std::ostringstream stream;
    for (std::size_t index = 0; index < values.size(); ++index)
    {
        if (index != 0)
        {
            stream << ';';
        }
        stream << values[index];
    }

    return stream.str();
}

unsigned short FixedUnsigned16Bootstrap(float value, float scale)
{
    int output = static_cast<int>(value * scale);
    if (output < 0)
    {
        output = 0;
    }
    if (output > 0xFFFF)
    {
        output = 0xFFFF;
    }

    return static_cast<unsigned short>(output);
}

int ResolveUserMessageId(const EngineShimState& state, std::string_view message_name)
{
    const int index = state.user_message_registry.IndexOf(message_name);
    return index == 0 ? 0 : (64 + index);
}

std::string CompletedMessagePreviewSince(const EngineShimState& state, std::size_t before_count)
{
    if (state.frame_message_buffer.CompletedCount() <= before_count)
    {
        return "<none>";
    }

    const std::vector<std::string> preview = state.frame_message_buffer.CompletedPreview(1);
    return preview.empty() ? std::string("<none>") : preview.back();
}

bool TryParseMessageWriteValue(
    std::string_view write,
    std::string_view expected_name,
    int* value)
{
    if (value == nullptr)
    {
        return false;
    }

    const std::size_t separator = write.find('=');
    if (separator == std::string_view::npos || write.substr(0, separator) != expected_name)
    {
        return false;
    }

    const std::string_view value_text = write.substr(separator + 1);
    int parsed = 0;
    const auto [ptr, ec] =
        std::from_chars(value_text.data(), value_text.data() + value_text.size(), parsed);
    if (ec != std::errc{} || ptr != value_text.data() + value_text.size())
    {
        return false;
    }

    *value = parsed;
    return true;
}

std::string BuildObservedMessageCallbackPath(
    const hl::game_api::detail::FrameCompletedMessageObservation& observation)
{
    std::vector<std::string> operations;
    operations.reserve(observation.writes.size() + 2);
    operations.push_back("pfnMessageBegin");
    for (const std::string& write : observation.writes)
    {
        const std::size_t separator = write.find('=');
        const std::string_view op_name =
            separator == std::string::npos
            ? std::string_view(write)
            : std::string_view(write).substr(0, separator);
        operations.push_back("pfn" + std::string(op_name));
    }
    operations.push_back("pfnMessageEnd");
    return JoinStringValues(operations);
}

std::string BuildObservedPayloadWrites(
    const hl::game_api::detail::FrameCompletedMessageObservation& observation)
{
    return observation.writes.empty()
        ? std::string("<none>")
        : JoinStringValues(observation.writes);
}

bool TryBuildScreenFadeDecodedFields(
    const hl::game_api::detail::FrameCompletedMessageObservation& observation,
    std::string* decoded_fields)
{
    if (decoded_fields == nullptr || observation.writes.size() != 7)
    {
        return false;
    }

    int duration = 0;
    int hold = 0;
    int flags = 0;
    int red = 0;
    int green = 0;
    int blue = 0;
    int alpha = 0;
    if (!TryParseMessageWriteValue(observation.writes[0], "WriteShort", &duration)
        || !TryParseMessageWriteValue(observation.writes[1], "WriteShort", &hold)
        || !TryParseMessageWriteValue(observation.writes[2], "WriteShort", &flags)
        || !TryParseMessageWriteValue(observation.writes[3], "WriteByte", &red)
        || !TryParseMessageWriteValue(observation.writes[4], "WriteByte", &green)
        || !TryParseMessageWriteValue(observation.writes[5], "WriteByte", &blue)
        || !TryParseMessageWriteValue(observation.writes[6], "WriteByte", &alpha))
    {
        return false;
    }

    *decoded_fields =
        "duration=" + std::to_string(duration)
        + " hold=" + std::to_string(hold)
        + " flags=" + std::to_string(flags)
        + " rgba=" + std::to_string(red)
        + "/" + std::to_string(green)
        + "/" + std::to_string(blue)
        + "/" + std::to_string(alpha);
    return true;
}

std::string BuildPresentationSemanticsSummary(
    std::string_view message_name,
    int message_id,
    const hl::game_api::detail::FrameCompletedMessageObservation& observation)
{
    std::string summary =
        "handled=server-side-presentation-semantics"
        " message=" + (message_name.empty() ? std::string("<unknown>") : std::string(message_name))
        + " id=" + std::to_string(message_id)
        + " dest="
        + (observation.destination_name.empty()
            ? std::string("MSG_UNKNOWN")
            : observation.destination_name)
        + "(" + std::to_string(observation.destination) + ")"
        + " payloadBytes=" + std::to_string(observation.payload_size);

    std::string decoded_fields;
    if (TryBuildScreenFadeDecodedFields(observation, &decoded_fields))
    {
        summary += " " + decoded_fields;
    }
    else if (!observation.writes.empty())
    {
        summary += " payloadWrites=" + BuildObservedPayloadWrites(observation);
    }

    summary += " callbackPath=" + BuildObservedMessageCallbackPath(observation);
    return summary;
}

PathNodeMessageEmitResult EmitPathNodeScreenFadeBootstrap(
    EngineShimState& state,
    const hl::game_api::detail::PathNodeEventDispatchRequest& request)
{
    PathNodeMessageEmitResult result;
    result.channel_name = "ScreenFade";
    result.message_id = ResolveUserMessageId(state, "ScreenFade");
    if (result.message_id == 0)
    {
        result.detail = "fade channel ScreenFade is unavailable";
        return result;
    }

    const ScreenFade fade = {
        FixedUnsigned16Bootstrap(1.0f, 1 << 12),
        FixedUnsigned16Bootstrap(0.5f, 1 << 12),
        static_cast<short>(FFADE_OUT | FFADE_STAYOUT),
        0,
        0,
        0,
        255,
    };
    const std::size_t completed_before = state.frame_message_buffer.CompletedCount();
    StubMessageBegin(MSG_ALL, result.message_id, nullptr, nullptr);
    StubWriteShort(fade.duration);
    StubWriteShort(fade.holdTime);
    StubWriteShort(fade.fadeFlags);
    StubWriteByte(fade.r);
    StubWriteByte(fade.g);
    StubWriteByte(fade.b);
    StubWriteByte(fade.a);
    StubMessageEnd();

    result.emitted = state.frame_message_buffer.CompletedCount() > completed_before;
    hl::game_api::detail::FrameCompletedMessageObservation observation;
    if (result.emitted
        && state.frame_message_buffer.LastCompletedSince(completed_before, &observation))
    {
        result.semantics_summary =
            BuildPresentationSemanticsSummary("ScreenFade", result.message_id, observation);
    }

    result.detail =
        (!result.semantics_summary.empty()
            ? result.semantics_summary
            : std::string(
                "handled=server-side-presentation-semantics"
                " message=ScreenFade"
                " id=" + std::to_string(result.message_id)))
        + " node=" + (request.node_name.empty() ? std::string("<empty>") : request.node_name)
        + " event=" + (request.message.empty() ? std::string("<empty>") : request.message)
        + " emitted=" + BoolToYesNo(result.emitted)
        + " preview={" + CompletedMessagePreviewSince(state, completed_before) + "}";
    return result;
}

PathNodeMessageEmitResult EmitPathNodeTextMessageBootstrap(
    EngineShimState& state,
    const hl::game_api::detail::PathNodeEventDispatchRequest& request)
{
    PathNodeMessageEmitResult result;
    result.message_id = ResolveUserMessageId(state, "TextMsg");
    if (result.message_id > 0)
    {
        result.channel_name = "TextMsg";
        const std::string text =
            "fade_out staged bootstrap @" + (request.node_name.empty()
                ? std::string("<empty>")
                : request.node_name);
        const std::size_t completed_before = state.frame_message_buffer.CompletedCount();
        StubMessageBegin(MSG_ALL, result.message_id, nullptr, nullptr);
        StubWriteByte(HUD_PRINTCENTER);
        StubWriteString(text.c_str());
        StubMessageEnd();
        result.emitted = state.frame_message_buffer.CompletedCount() > completed_before;
        hl::game_api::detail::FrameCompletedMessageObservation observation;
        if (result.emitted
            && state.frame_message_buffer.LastCompletedSince(completed_before, &observation))
        {
            result.semantics_summary =
                BuildPresentationSemanticsSummary(result.channel_name, result.message_id, observation);
        }
        result.detail =
            (!result.semantics_summary.empty()
                ? result.semantics_summary
                : std::string(
                    "handled=server-side-presentation-semantics"
                    " message=TextMsg"
                    " id=" + std::to_string(result.message_id)))
            + " text=" + text
            + " emitted=" + BoolToYesNo(result.emitted)
            + " preview={" + CompletedMessagePreviewSince(state, completed_before) + "}";
        return result;
    }

    result.message_id = ResolveUserMessageId(state, "HudText");
    result.channel_name = "HudText";
    if (result.message_id == 0)
    {
        result.detail = "message channel TextMsg/HudText is unavailable";
        return result;
    }

    const std::string text =
        "fade_out staged bootstrap @" + (request.node_name.empty()
            ? std::string("<empty>")
            : request.node_name);
    const std::size_t completed_before = state.frame_message_buffer.CompletedCount();
    StubMessageBegin(MSG_ALL, result.message_id, nullptr, nullptr);
    StubWriteString(text.c_str());
    StubMessageEnd();
    result.emitted = state.frame_message_buffer.CompletedCount() > completed_before;
    hl::game_api::detail::FrameCompletedMessageObservation observation;
    if (result.emitted
        && state.frame_message_buffer.LastCompletedSince(completed_before, &observation))
    {
        result.semantics_summary =
            BuildPresentationSemanticsSummary(result.channel_name, result.message_id, observation);
    }
    result.detail =
        (!result.semantics_summary.empty()
            ? result.semantics_summary
            : std::string(
                "handled=server-side-presentation-semantics"
                " message=HudText"
                " id=" + std::to_string(result.message_id)))
        + " text=" + text
        + " emitted=" + BoolToYesNo(result.emitted)
        + " preview={" + CompletedMessagePreviewSince(state, completed_before) + "}";
    return result;
}

PathNodePresentationLinkageProbe ProbePathNodePresentationLinkage(
    const EngineShimState& state,
    std::string_view event_name)
{
    PathNodePresentationLinkageProbe probe;
    if (event_name.empty())
    {
        return probe;
    }

    const auto collect_match_fields =
        [&](std::string_view targetname,
            std::string_view message,
            std::string_view target)
        {
            std::vector<std::string> matches;
            if (EqualsIgnoreCase(targetname, event_name))
            {
                matches.push_back("targetname");
            }
            if (EqualsIgnoreCase(message, event_name))
            {
                matches.push_back("message");
            }
            if (EqualsIgnoreCase(target, event_name))
            {
                matches.push_back("target");
            }
            return matches;
        };

    for (const RuntimeEntityRecord& record : state.entity_bootstrap.runtime_entities)
    {
        const bool env_message = EqualsIgnoreCase(record.classname, "env_message");
        const bool env_fade = EqualsIgnoreCase(record.classname, "env_fade");
        if (!env_message && !env_fade)
        {
            continue;
        }

        const std::vector<std::string> matches =
            collect_match_fields(record.targetname, record.message, record.target);
        if (matches.empty())
        {
            continue;
        }

        if (env_message)
        {
            ++probe.env_message_runtime_candidates;
            probe.actionable_env_message_runtime =
                probe.actionable_env_message_runtime || IsLiveRuntimeTargetRecord(record);
        }
        else
        {
            ++probe.env_fade_runtime_candidates;
        }

        if (probe.details.size() < 8)
        {
            probe.details.push_back(
                "runtime edict#" + std::to_string(record.edict_index)
                + " classname=" + (record.classname.empty()
                    ? std::string("<empty>")
                    : record.classname)
                + " match=" + JoinStringValues(matches)
                + " live=" + BoolToYesNo(IsLiveRuntimeTargetRecord(record))
                + " targetname="
                + (record.targetname.empty() ? std::string("<empty>") : record.targetname)
                + " message="
                + (record.message.empty() ? std::string("<empty>") : record.message)
                + " target=" + (record.target.empty() ? std::string("<empty>") : record.target));
        }
    }

    for (const hl::game_api::detail::EntityDefinition& definition :
         state.entity_bootstrap.parsed_entities)
    {
        const bool env_message = EqualsIgnoreCase(definition.classname, "env_message");
        const bool env_fade = EqualsIgnoreCase(definition.classname, "env_fade");
        if (!env_message && !env_fade)
        {
            continue;
        }

        const std::string* targetname = FindLastKeyValue(definition, "targetname");
        const std::string* message = FindLastKeyValue(definition, "message");
        const std::string* target = FindLastKeyValue(definition, "target");
        const std::vector<std::string> matches = collect_match_fields(
            targetname != nullptr ? *targetname : std::string_view{},
            message != nullptr ? *message : std::string_view{},
            target != nullptr ? *target : std::string_view{});
        if (matches.empty())
        {
            continue;
        }

        if (env_message)
        {
            ++probe.env_message_parsed_candidates;
        }
        else
        {
            ++probe.env_fade_parsed_candidates;
        }

        const RuntimeEntityRecord* runtime_record =
            FindRuntimeRecordByParseIndex(state, definition.ordinal);
        if (env_message && runtime_record != nullptr)
        {
            probe.actionable_env_message_runtime =
                probe.actionable_env_message_runtime || IsLiveRuntimeTargetRecord(*runtime_record);
        }

        if (probe.details.size() < 8)
        {
            probe.details.push_back(
                "parsed#" + std::to_string(definition.ordinal)
                + " classname=" + SafeClassname(definition)
                + " match=" + JoinStringValues(matches)
                + " runtime="
                + (runtime_record != nullptr
                    ? BuildParsedRuntimeAuditDetail(*runtime_record)
                    : std::string("runtimeRecordMissing")));
        }
    }

    return probe;
}

std::string BuildPathNodePresentationLinkageSummary(
    const PathNodePresentationLinkageProbe& probe,
    bool fade_channel_available,
    int fade_message_id,
    bool message_channel_available,
    std::string_view message_channel_name,
    int message_channel_id)
{
    return "fadeChannel="
        + (fade_channel_available
            ? "available(ScreenFade#" + std::to_string(fade_message_id) + ")"
            : std::string("missing"))
        + " messageChannel="
        + (message_channel_available
            ? std::string("available(")
                + (message_channel_name.empty()
                    ? std::string("<unknown>")
                    : std::string(message_channel_name))
                + "#" + std::to_string(message_channel_id) + ")"
            : std::string("missing"))
        + " envMessageLinkage="
        + ((probe.env_message_runtime_candidates > 0 || probe.env_message_parsed_candidates > 0)
            ? (probe.actionable_env_message_runtime
                ? std::string("runtime-actionable")
                : std::string("present-nonactionable"))
            : std::string("absent"))
        + " envMessageRuntime=" + std::to_string(probe.env_message_runtime_candidates)
        + " envMessageParsed=" + std::to_string(probe.env_message_parsed_candidates)
        + " envFadeRuntime=" + std::to_string(probe.env_fade_runtime_candidates)
        + " envFadeParsed=" + std::to_string(probe.env_fade_parsed_candidates)
        + " probeDetails=" + JoinStringValues(probe.details);
}

PathNodePresentationDispatchResult DispatchKnownPathNodePresentationBootstrap(
    EngineShimState& state,
    const hl::game_api::detail::PathNodeEventDispatchRequest& request,
    const hl::game_api::detail::MapLogicDispatchContext& dispatch_context,
    const PathNodePresentationLinkageProbe& linkage_probe,
    hl::game_api::detail::MapLogicDispatcher& dispatcher,
    const hl::game_api::detail::MapLogicDispatcherHooks& hooks)
{
    PathNodePresentationDispatchResult result;

    const int fade_message_id = ResolveUserMessageId(state, "ScreenFade");
    const int text_message_id = ResolveUserMessageId(state, "TextMsg");
    const int hud_text_message_id = ResolveUserMessageId(state, "HudText");
    result.fade_channel_available = fade_message_id > 0;
    result.message_channel_available = text_message_id > 0 || hud_text_message_id > 0;
    result.env_message_linkage_found =
        linkage_probe.env_message_runtime_candidates > 0
        || linkage_probe.env_message_parsed_candidates > 0;
    result.presentation_linkage_detail = BuildPathNodePresentationLinkageSummary(
        linkage_probe,
        result.fade_channel_available,
        fade_message_id,
        result.message_channel_available,
        text_message_id > 0 ? std::string_view("TextMsg") : std::string_view("HudText"),
        text_message_id > 0 ? text_message_id : hud_text_message_id);

    if (result.fade_channel_available)
    {
        const PathNodeMessageEmitResult fade_emit =
            EmitPathNodeScreenFadeBootstrap(state, request);
        if (fade_emit.emitted)
        {
            result.dispatched = true;
            result.dispatch_mode = "fade-channel";
            result.classification = "resolved-and-dispatched-via-fade-channel";
            result.fade_channel_used = true;
            result.semantics_summary = fade_emit.semantics_summary;
            result.detail = fade_emit.detail;
            return result;
        }
    }

    if (result.message_channel_available)
    {
        const PathNodeMessageEmitResult message_emit =
            EmitPathNodeTextMessageBootstrap(state, request);
        if (message_emit.emitted)
        {
            result.dispatched = true;
            result.dispatch_mode = "message-channel";
            result.classification = "resolved-and-dispatched-via-message-channel";
            result.message_channel_used = true;
            result.semantics_summary = message_emit.semantics_summary;
            result.detail = message_emit.detail;
            return result;
        }
    }

    if (linkage_probe.actionable_env_message_runtime)
    {
        result.dispatch_mode = "env_message-linkage";
        result.env_message_linkage_used = dispatcher.DispatchTargetChain(dispatch_context, hooks);
        if (result.env_message_linkage_used)
        {
            result.dispatched = true;
            result.classification = "resolved-and-dispatched-via-env_message-linkage";
            result.detail =
                "env_message linkage dispatch target="
                + (dispatch_context.target_name.empty()
                    ? std::string("<empty>")
                    : dispatch_context.target_name)
                + " detail={" + result.presentation_linkage_detail + "}";
            return result;
        }

        result.deferred = true;
        result.classification = "deferred-missing-env-message-linkage";
        result.required_subsystem = "actionable env_message runtime linkage";
        result.detail =
            "env_message linkage matched but dispatcher did not produce a staged action"
            " detail={" + result.presentation_linkage_detail + "}";
        return result;
    }

    if (result.env_message_linkage_found)
    {
        result.deferred = true;
        result.dispatch_mode = "env_message-linkage";
        result.classification = "deferred-missing-env-message-linkage";
        result.required_subsystem = "actionable env_message runtime linkage";
        result.detail =
            "env_message linkage exists only in non-actionable parsed/runtime state"
            " detail={" + result.presentation_linkage_detail + "}";
        return result;
    }

    result.dispatched = true;
    result.dispatch_mode = "summary-only-fallback";
    result.classification = "summary-only-staged-fallback";
    result.summary_only_fallback_used = true;
    result.detail =
        "summary-only staged presentation sink recorded fade/message event"
        " node=" + (request.node_name.empty() ? std::string("<empty>") : request.node_name)
        + " event=" + (request.message.empty() ? std::string("<empty>") : request.message)
        + " detail={" + result.presentation_linkage_detail + "}";
    return result;
}

std::string BuildCallbackDeltaSummary(
    const std::vector<hl::game_api::InvokedEngineCallback>& callback_delta)
{
    std::ostringstream stream;
    bool first = true;
    int emitted = 0;
    for (const hl::game_api::InvokedEngineCallback& callback : callback_delta)
    {
        if (!IsMessageCallbackName(callback.name) && !IsMovementCallbackName(callback.name))
        {
            continue;
        }

        if (!first)
        {
            stream << ';';
        }
        stream << callback.name << ':' << callback.call_count;
        first = false;
        ++emitted;
        if (emitted >= 8)
        {
            break;
        }
    }

    return first ? std::string() : stream.str();
}

struct PathNodeTrackedChangeSummary
{
    int multi_manager_activity = 0;
    int scripted_sequence_activity = 0;
    int actor_state_changes = 0;
    int path_state_changes = 0;
    bool brush_door_use_succeeded = false;
    bool brush_door_state_changed = false;
    bool brush_door_movement_started = false;
    bool brush_door_movement_completed = false;
    std::vector<std::string> details;
};

PathNodeTrackedChangeSummary SummarizePathNodeTrackedEntityChanges(
    const PathNodeDownstreamSnapshot& before,
    const PathNodeDownstreamSnapshot& after)
{
    PathNodeTrackedChangeSummary summary;

    for (const PathNodeDownstreamSnapshot::TrackedEntityState& after_state : after.tracked_entities)
    {
        PathNodeDownstreamSnapshot::TrackedEntityState before_state{};
        before_state.edict_index = after_state.edict_index;
        before_state.classname = after_state.classname;
        before_state.targetname = after_state.targetname;
        if (const PathNodeDownstreamSnapshot::TrackedEntityState* existing =
                FindPathNodeTrackedEntityState(before, after_state.edict_index);
            existing != nullptr)
        {
            before_state = *existing;
        }

        std::vector<std::string> local_changes;
        AppendDeltaSummary(
            local_changes,
            "resolutions",
            static_cast<int>(after_state.target_resolutions - before_state.target_resolutions));
        AppendDeltaSummary(
            local_changes,
            "useSuccesses",
            static_cast<int>(after_state.use_successes - before_state.use_successes));
        AppendDeltaSummary(
            local_changes,
            "useDeferred",
            static_cast<int>(after_state.use_deferred - before_state.use_deferred));
        AppendDeltaSummary(
            local_changes,
            "useFailures",
            static_cast<int>(after_state.use_failures - before_state.use_failures));
        AppendDeltaSummary(
            local_changes,
            "pendingOutputs",
            after_state.pending_scheduled_outputs - before_state.pending_scheduled_outputs);
        AppendDeltaSummary(
            local_changes,
            "scriptedUse",
            after_state.scripted_received_use - before_state.scripted_received_use);
        AppendDeltaSummary(
            local_changes,
            "scriptedEmit",
            after_state.scripted_emitted_targets - before_state.scripted_emitted_targets);
        AppendDeltaSummary(
            local_changes,
            "scriptedScheduled",
            after_state.scripted_scheduled_outputs - before_state.scripted_scheduled_outputs);

        if (after_state.scripted_internal_state_changed
            && !before_state.scripted_internal_state_changed)
        {
            local_changes.push_back("stateChanged");
        }
        if (after_state.scripted_scheduled_follow_up
            && !before_state.scripted_scheduled_follow_up)
        {
            local_changes.push_back("scheduledFollowUp");
        }
        if (after_state.scripted_progressed && !before_state.scripted_progressed)
        {
            local_changes.push_back("progressed");
        }
        if (after_state.scripted_actor_resolved != before_state.scripted_actor_resolved)
        {
            local_changes.push_back(
                "actorResolved=" + std::string(after_state.scripted_actor_resolved ? "yes" : "no"));
        }
        if (after_state.scripted_arrived && !before_state.scripted_arrived)
        {
            local_changes.push_back("arrived");
        }
        if (after_state.scripted_animation_ready && !before_state.scripted_animation_ready)
        {
            local_changes.push_back("animationReady");
        }
        if (after_state.scripted_movement_stage != before_state.scripted_movement_stage)
        {
            local_changes.push_back(
                "movementStage="
                + (before_state.scripted_movement_stage.empty()
                    ? std::string("<none>")
                    : before_state.scripted_movement_stage)
                + "->"
                + (after_state.scripted_movement_stage.empty()
                    ? std::string("<none>")
                    : after_state.scripted_movement_stage));
        }
        if (after_state.scripted_support_state != before_state.scripted_support_state)
        {
            local_changes.push_back(
                "scriptedSupport="
                + (before_state.scripted_support_state.empty()
                    ? std::string("<none>")
                    : before_state.scripted_support_state)
                + "->"
                + (after_state.scripted_support_state.empty()
                    ? std::string("<none>")
                    : after_state.scripted_support_state));
        }
        if (after_state.scripted_blocked_reason != before_state.scripted_blocked_reason
            && !after_state.scripted_blocked_reason.empty())
        {
            local_changes.push_back("blocked=" + after_state.scripted_blocked_reason);
        }

        if (after_state.brush_door_tracked || before_state.brush_door_tracked)
        {
            if (after_state.brush_door_dispatch_path != before_state.brush_door_dispatch_path
                && !after_state.brush_door_dispatch_path.empty())
            {
                local_changes.push_back(
                    "doorPath="
                    + (before_state.brush_door_dispatch_path.empty()
                        ? std::string("<none>")
                        : before_state.brush_door_dispatch_path)
                    + "->" + after_state.brush_door_dispatch_path);
            }
            if (after_state.brush_door_support_state != before_state.brush_door_support_state
                && !after_state.brush_door_support_state.empty())
            {
                local_changes.push_back(
                    "doorSupport="
                    + (before_state.brush_door_support_state.empty()
                        ? std::string("<none>")
                        : before_state.brush_door_support_state)
                    + "->" + after_state.brush_door_support_state);
                summary.brush_door_state_changed = true;
            }
            if (after_state.brush_door_state != before_state.brush_door_state
                && !after_state.brush_door_state.empty())
            {
                local_changes.push_back(
                    "doorState="
                    + (before_state.brush_door_state.empty()
                        ? std::string("<none>")
                        : before_state.brush_door_state)
                    + "->" + after_state.brush_door_state);
                summary.brush_door_state_changed = true;
            }
            if (after_state.brush_door_movement_started && !before_state.brush_door_movement_started)
            {
                local_changes.push_back("doorMoveStarted");
                summary.brush_door_movement_started = true;
            }
            if (after_state.brush_door_movement_completed
                && !before_state.brush_door_movement_completed)
            {
                local_changes.push_back("doorMoveCompleted");
                summary.brush_door_movement_completed = true;
            }
            if (after_state.brush_door_last_use_frame != before_state.brush_door_last_use_frame
                && after_state.brush_door_last_use_frame >= 0)
            {
                local_changes.push_back(
                    "doorUseFrame=" + std::to_string(after_state.brush_door_last_use_frame));
                summary.brush_door_use_succeeded = true;
            }
            if (after_state.brush_door_blocked_reason != before_state.brush_door_blocked_reason
                && !after_state.brush_door_blocked_reason.empty())
            {
                local_changes.push_back("doorBlocked=" + after_state.brush_door_blocked_reason);
            }
            if (after_state.brush_door_last_source_event != before_state.brush_door_last_source_event
                && !after_state.brush_door_last_source_event.empty())
            {
                local_changes.push_back("doorSource=" + after_state.brush_door_last_source_event);
            }
        }

        if (local_changes.empty())
        {
            continue;
        }

        if (after_state.edict_index < 0)
        {
            continue;
        }

        const std::string normalized = ToLowerCopy(after_state.classname);
        if (normalized == "multi_manager")
        {
            ++summary.multi_manager_activity;
        }
        else if (normalized == "scripted_sequence")
        {
            ++summary.scripted_sequence_activity;
            if (after_state.scripted_actor_resolved != before_state.scripted_actor_resolved
                || after_state.scripted_arrived != before_state.scripted_arrived
                || after_state.scripted_animation_ready != before_state.scripted_animation_ready
                || after_state.scripted_movement_stage != before_state.scripted_movement_stage)
            {
                ++summary.actor_state_changes;
            }
        }
        else if (normalized == "func_tracktrain")
        {
            ++summary.path_state_changes;
        }
        else if (normalized == "func_door")
        {
            ++summary.path_state_changes;
        }
        else if (normalized == "monster_barney"
            || normalized == "monster_scientist"
            || normalized == "monster_sitting_scientist")
        {
            ++summary.actor_state_changes;
        }

        std::ostringstream stream;
        stream << DescribePathNodeTrackedEntity(after_state) << " ";
        for (std::size_t index = 0; index < local_changes.size(); ++index)
        {
            if (index != 0)
            {
                stream << ",";
            }
            stream << local_changes[index];
        }
        summary.details.push_back(stream.str());
    }

    return summary;
}

bool HasVisiblePathNodeDownstreamProgression(
    const PathNodeDownstreamSnapshot& before,
    const PathNodeDownstreamSnapshot& after)
{
    return (after.target_chains_fired - before.target_chains_fired) > 1
        || (after.use_attempts - before.use_attempts) > 0
        || (after.use_successes - before.use_successes) > 0
        || (after.use_deferred - before.use_deferred) > 0
        || (after.use_failures - before.use_failures) > 0
        || (after.scheduled_created - before.scheduled_created) > 0
        || (after.scheduled_executed - before.scheduled_executed) > 0
        || (after.scheduled_rescheduled - before.scheduled_rescheduled) > 0
        || (after.scripted_received_use - before.scripted_received_use) > 0
        || (after.scripted_emitted_targets - before.scripted_emitted_targets) > 0
        || (after.scripted_scheduled_outputs - before.scripted_scheduled_outputs) > 0
        || (after.scripted_internal_state_changes - before.scripted_internal_state_changes) > 0
        || (after.scripted_scheduled_follow_ups - before.scripted_scheduled_follow_ups) > 0
        || (after.scripted_progressed - before.scripted_progressed) > 0
        || (after.multi_manager_activity - before.multi_manager_activity) > 0
        || (after.movement_callbacks - before.movement_callbacks) > 0;
}

std::string BuildPathNodeDownstreamSummary(
    const PathNodeDownstreamSnapshot& before,
    const PathNodeDownstreamSnapshot& after,
    const std::vector<hl::game_api::InvokedEngineCallback>& callback_delta)
{
    std::vector<std::string> parts;
    const PathNodeTrackedChangeSummary tracked_changes =
        SummarizePathNodeTrackedEntityChanges(before, after);
    AppendDeltaSummary(parts, "chains", after.target_chains_fired - before.target_chains_fired);
    AppendDeltaSummary(parts, "resolutions", after.target_resolutions - before.target_resolutions);
    AppendDeltaSummary(parts, "useAttempts", after.use_attempts - before.use_attempts);
    AppendDeltaSummary(parts, "useSuccesses", after.use_successes - before.use_successes);
    AppendDeltaSummary(parts, "useDeferred", after.use_deferred - before.use_deferred);
    AppendDeltaSummary(parts, "useFailures", after.use_failures - before.use_failures);
    AppendDeltaSummary(parts, "scheduledCreated", after.scheduled_created - before.scheduled_created);
    AppendDeltaSummary(parts, "scheduledExecuted", after.scheduled_executed - before.scheduled_executed);
    AppendDeltaSummary(parts, "scheduledRescheduled", after.scheduled_rescheduled - before.scheduled_rescheduled);
    AppendDeltaSummary(parts, "scheduledPending", after.scheduled_pending - before.scheduled_pending);
    AppendDeltaSummary(parts, "scriptedUse", after.scripted_received_use - before.scripted_received_use);
    AppendDeltaSummary(parts, "scriptedEmit", after.scripted_emitted_targets - before.scripted_emitted_targets);
    AppendDeltaSummary(parts, "scriptedScheduled", after.scripted_scheduled_outputs - before.scripted_scheduled_outputs);
    AppendDeltaSummary(parts, "scriptedChanged", after.scripted_internal_state_changes - before.scripted_internal_state_changes);
    AppendDeltaSummary(parts, "scriptedFollowUp", after.scripted_scheduled_follow_ups - before.scripted_scheduled_follow_ups);
    AppendDeltaSummary(parts, "scriptedProgress", after.scripted_progressed - before.scripted_progressed);
    AppendDeltaSummary(parts, "multiManager", after.multi_manager_activity - before.multi_manager_activity);
    AppendDeltaSummary(parts, "alertCallbacks", after.alert_callbacks - before.alert_callbacks);
    AppendDeltaSummary(parts, "messageCallbacks", after.message_callbacks - before.message_callbacks);
    AppendDeltaSummary(parts, "movementCallbacks", after.movement_callbacks - before.movement_callbacks);

    const std::string callback_summary = BuildCallbackDeltaSummary(callback_delta);
    if (!callback_summary.empty())
    {
        parts.push_back("callbackDelta=" + callback_summary);
    }
    if (!tracked_changes.details.empty())
    {
        std::ostringstream stream;
        for (std::size_t index = 0; index < tracked_changes.details.size() && index < 6; ++index)
        {
            if (index != 0)
            {
                stream << ';';
            }
            stream << tracked_changes.details[index];
        }
        parts.push_back("entityDelta=" + stream.str());
    }
    if (tracked_changes.brush_door_use_succeeded)
    {
        parts.push_back("brushDoorUse=+1");
    }
    if (tracked_changes.brush_door_state_changed)
    {
        parts.push_back("brushDoorStateChange=yes");
    }
    if (tracked_changes.brush_door_movement_started)
    {
        parts.push_back("brushDoorMoveStarted=yes");
    }
    if (tracked_changes.brush_door_movement_completed)
    {
        parts.push_back("brushDoorMoveCompleted=yes");
    }

    if (parts.empty())
    {
        return "none";
    }

    std::ostringstream stream;
    for (std::size_t index = 0; index < parts.size(); ++index)
    {
        if (index != 0)
        {
            stream << ", ";
        }
        stream << parts[index];
    }

    return stream.str();
}

bool ParseStrictVector3(std::string_view text, Vector* value)
{
    if (value == nullptr)
    {
        return false;
    }

    std::istringstream stream{std::string(text)};
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    stream >> x >> y >> z;
    if (stream.fail())
    {
        return false;
    }

    stream >> std::ws;
    if (!stream.eof())
    {
        return false;
    }

    *value = Vector(x, y, z);
    return true;
}

ParsedVectorField ParseOriginField(const hl::game_api::detail::EntityDefinition& entity)
{
    ParsedVectorField field;
    const std::string* raw_origin = FindLastKeyValue(entity, "origin");
    if (raw_origin == nullptr)
    {
        return field;
    }

    field.present = true;
    field.raw_text = *raw_origin;
    field.valid = ParseStrictVector3(*raw_origin, &field.value);
    return field;
}

ParsedVectorField ParseAnglesField(const hl::game_api::detail::EntityDefinition& entity)
{
    ParsedVectorField field;
    if (const std::string* raw_angles = FindLastKeyValue(entity, "angles");
        raw_angles != nullptr)
    {
        field.present = true;
        field.raw_text = *raw_angles;
        field.valid = ParseStrictVector3(*raw_angles, &field.value);
        return field;
    }

    const std::string* raw_angle = FindLastKeyValue(entity, "angle");
    if (raw_angle == nullptr)
    {
        return field;
    }

    field.present = true;
    field.raw_text = *raw_angle;

    float yaw = 0.0f;
    if (!ParseStrictFloat(*raw_angle, &yaw))
    {
        return field;
    }

    if (yaw == -1.0f)
    {
        field.value = Vector(-90.0f, 0.0f, 0.0f);
    }
    else if (yaw == -2.0f)
    {
        field.value = Vector(90.0f, 0.0f, 0.0f);
    }
    else
    {
        field.value = Vector(0.0f, yaw, 0.0f);
    }

    field.valid = true;
    return field;
}

std::vector<hl::game_api::EntityClassCountSummary> BuildTopClassnameCounts(
    const std::vector<hl::game_api::detail::EntityDefinition>& entities,
    std::size_t max_count)
{
    std::unordered_map<std::string, std::size_t> counts;
    for (const hl::game_api::detail::EntityDefinition& entity : entities)
    {
        const std::string classname = entity.classname.empty()
            ? std::string("<missing>")
            : entity.classname;
        ++counts[classname];
    }

    std::vector<hl::game_api::EntityClassCountSummary> summary;
    summary.reserve(counts.size());
    for (const auto& [classname, count] : counts)
    {
        summary.push_back({classname, count});
    }

    std::sort(
        summary.begin(),
        summary.end(),
        [](const hl::game_api::EntityClassCountSummary& left,
           const hl::game_api::EntityClassCountSummary& right)
        {
            if (left.count != right.count)
            {
                return left.count > right.count;
            }

            return left.classname < right.classname;
        });

    if (summary.size() > max_count)
    {
        summary.resize(max_count);
    }

    return summary;
}

std::vector<hl::game_api::InvokedEngineCallback> BuildCallbackDelta(
    const std::unordered_map<std::string, std::size_t>& before,
    const std::unordered_map<std::string, std::size_t>& after)
{
    std::vector<hl::game_api::InvokedEngineCallback> delta;
    for (const auto& [name, after_count] : after)
    {
        const auto before_it = before.find(name);
        const std::size_t before_count = before_it != before.end() ? before_it->second : 0;
        if (after_count > before_count)
        {
            delta.push_back({name, after_count - before_count});
        }
    }

    std::sort(
        delta.begin(),
        delta.end(),
        [](const hl::game_api::InvokedEngineCallback& left,
           const hl::game_api::InvokedEngineCallback& right)
        {
            if (left.call_count != right.call_count)
            {
                return left.call_count > right.call_count;
            }

            return left.name < right.name;
        });

    return delta;
}

bool IsFrameRelevantClassname(std::string_view classname)
{
    const std::string normalized = ToLowerCopy(classname);
    return normalized == "worldspawn"
        || normalized.rfind("func_", 0) == 0
        || normalized.rfind("env_", 0) == 0
        || normalized.rfind("trigger_", 0) == 0
        || normalized.rfind("info_", 0) == 0;
}

hl::game_api::detail::FrameLoopValidationState CollectFrameLoopValidationState(
    const EngineShimState& state,
    std::size_t preview_limit)
{
    hl::game_api::detail::FrameLoopValidationState validation;
    validation.activation_succeeded = state.server_activation_state.succeeded;
    validation.server_active = state.server_state.active;
    validation.map_name = state.string_pool.Describe(state.globalvars.mapname);
    validation.mapname_valid = !validation.map_name.empty();
    validation.max_clients = state.globalvars.maxClients;
    validation.max_clients_sane =
        validation.max_clients > 0 && validation.max_clients <= state.edict_store.MaxEntities();

    const hl::game_api::detail::EntityStateSnapshot world_snapshot =
        state.edict_store.SnapshotOf(state.edict_store.World(), state.string_pool);
    validation.worldspawn_spawned =
        world_snapshot.index == 0
        && world_snapshot.in_use
        && world_snapshot.spawned
        && !world_snapshot.removed
        && EqualsIgnoreCase(world_snapshot.classname, "worldspawn")
        && state.worldspawn_spawn_diagnostics.Succeeded();
    validation.edict0_valid =
        world_snapshot.index == 0
        && world_snapshot.in_use
        && !world_snapshot.removed
        && EqualsIgnoreCase(world_snapshot.classname, "worldspawn");

    for (int index = 0; index < state.edict_store.MaxEntities(); ++index)
    {
        const edict_t* entity = state.edict_store.EntityOfIndex(index);
        const hl::game_api::detail::EntityStateSnapshot snapshot =
            state.edict_store.SnapshotOf(entity, state.string_pool);
        if (snapshot.index < 0)
        {
            continue;
        }

        const bool active = snapshot.in_use && !snapshot.removed;
        if (active)
        {
            ++validation.active_edicts;
        }

        if (snapshot.spawned && active)
        {
            ++validation.spawned_entities;
        }

        if (snapshot.removed)
        {
            ++validation.removed_entities;
        }

        if (snapshot.deferred)
        {
            ++validation.deferred_entities;
        }

        if (active && validation.preview_entities.size() < preview_limit)
        {
            const bool frame_relevant =
                snapshot.index == 0
                || snapshot.private_data_present
                || snapshot.activation_candidate
                || IsFrameRelevantClassname(snapshot.classname);
            validation.preview_entities.push_back({
                snapshot.index,
                frame_relevant,
                BuildEntitySnapshotSummary(snapshot),
            });
        }
    }

    if (!validation.activation_succeeded)
    {
        validation.issues.push_back("server activation has not succeeded");
    }
    if (!validation.server_active)
    {
        validation.issues.push_back("server_state.active=no");
    }
    if (!validation.worldspawn_spawned)
    {
        validation.issues.push_back("worldspawn is not marked as spawned");
    }
    if (!validation.edict0_valid)
    {
        validation.issues.push_back("edict#0 is not a valid worldspawn");
    }
    if (!validation.mapname_valid)
    {
        validation.issues.push_back("gpGlobals->mapname is empty");
    }
    if (!validation.max_clients_sane)
    {
        validation.issues.push_back(
            "maxClients is not sane: " + std::to_string(validation.max_clients));
    }
    if (validation.active_edicts <= 0)
    {
        validation.issues.push_back("no active edicts are available for frame execution");
    }
    if (validation.spawned_entities <= 0)
    {
        validation.issues.push_back("no spawned entities are available for frame execution");
    }

    return validation;
}

void AdvanceFrameClock(EngineShimState& state, float frametime)
{
    state.server_state.old_realtime = state.server_state.realtime;
    state.server_state.realtime += frametime;
    state.server_state.time += frametime;
    state.server_state.frametime = frametime;
    ++state.server_state.frame_count;
    ++state.server_state.server_frame;

    hl::game_api::detail::ApplyGlobalsFromServerState(
        state.server_state,
        state.string_pool,
        state.globalvars);
    state.globalvars.maxEntities = state.edict_store.MaxEntities();
}

RuntimeEntityLifecycleState ClassifyRuntimeLifecycleState(const RuntimeEntityRecord& record)
{
    if (record.removed || !record.in_use || (record.flags & FL_KILLME) != 0)
    {
        return RuntimeEntityLifecycleState::kRemovedByGameLogic;
    }

    if (record.deferred)
    {
        return RuntimeEntityLifecycleState::kDetectedButDeferred;
    }

    const std::string normalized = ToLowerCopy(record.classname);
    if (normalized == "trigger_auto"
        || normalized == "multi_manager"
        || normalized == "env_message")
    {
        return RuntimeEntityLifecycleState::kActiveSupported;
    }

    if (normalized == "worldspawn"
        || normalized == "func_wall"
        || normalized == "ambient_generic"
        || normalized == "env_glow"
        || normalized == "light"
        || normalized == "light_spot"
        || normalized == "path_track"
        || normalized == "info_player_start"
        || normalized == "info_player_deathmatch"
        || normalized == "monster_barney"
        || normalized == "monster_scientist"
        || normalized == "monster_sitting_scientist")
    {
        return RuntimeEntityLifecycleState::kPassiveSupported;
    }

    if (normalized == "scripted_sequence" || normalized.rfind("scripted_", 0) == 0)
    {
        return RuntimeEntityLifecycleState::kDetectedButDeferred;
    }

    return record.scheduled_for_think
        ? RuntimeEntityLifecycleState::kDetectedButDeferred
        : RuntimeEntityLifecycleState::kPassiveSupported;
}

RuntimeEntityRecord* FindRuntimeRecordByEdictIndex(EngineShimState& state, int edict_index)
{
    if (edict_index < 0)
    {
        return nullptr;
    }

    for (RuntimeEntityRecord& record : state.entity_bootstrap.runtime_entities)
    {
        if (record.edict_index == edict_index)
        {
            return &record;
        }
    }

    return nullptr;
}

RuntimeEntityRecord* EnsureRuntimeRecordForEntity(
    EngineShimState& state,
    const edict_t* entity,
    std::string_view source_tag)
{
    const int edict_index = state.edict_store.IndexOf(entity);
    if (edict_index < 0)
    {
        return nullptr;
    }

    if (RuntimeEntityRecord* existing = FindRuntimeRecordByEdictIndex(state, edict_index);
        existing != nullptr)
    {
        return existing;
    }

    RuntimeEntityRecord record;
    record.parse_index = state.entity_bootstrap.next_dynamic_runtime_index++;
    record.edict_index = edict_index;
    record.dynamic_runtime = true;
    record.source_preview = std::string(source_tag);
    state.entity_bootstrap.runtime_entities.push_back(std::move(record));
    return &state.entity_bootstrap.runtime_entities.back();
}

void SyncRuntimeRecordFromEdict(
    const EngineShimState& state,
    const edict_t* entity,
    RuntimeEntityRecord& record)
{
    const hl::game_api::detail::EntityStateSnapshot snapshot =
        state.edict_store.SnapshotOf(entity, state.string_pool);
    if (snapshot.index < 0)
    {
        return;
    }

    if (!record.dynamic_runtime
        && snapshot.parse_index >= 0
        && snapshot.parse_index != static_cast<int>(record.parse_index))
    {
        return;
    }

    record.edict_index = snapshot.index;
    record.in_use = snapshot.in_use;
    record.removed = snapshot.removed;
    if (!snapshot.classname.empty())
    {
        record.classname = snapshot.classname;
    }

    if (!snapshot.targetname.empty())
    {
        record.targetname = snapshot.targetname;
    }

    hl::game_api::detail::EntityVarSnapshot vars_snapshot;
    const bool vars_available = hl::game_api::detail::TryReadEntityVars(
        state.edict_store,
        state.string_pool,
        entity,
        &vars_snapshot,
        [&](std::string_view message)
        {
            hl::common::Logger::Warn(std::string(message));
        });
    if (vars_available)
    {
        record.target = vars_snapshot.target;
        record.message = vars_snapshot.message;
        record.nextthink = vars_snapshot.nextthink;
        record.ltime = vars_snapshot.ltime;
        record.flags = vars_snapshot.flags;
        record.solid = vars_snapshot.solid;
        record.movetype = vars_snapshot.movetype;
        record.effects = vars_snapshot.effects;
        record.health = vars_snapshot.health;
        record.health_available = vars_snapshot.health_available;
        record.has_private_data = vars_snapshot.has_private_data;
        record.private_data_present = vars_snapshot.has_private_data;
        record.scheduled_for_think = vars_snapshot.scheduled_for_think;
        record.movedir = vars_snapshot.movedir;
        record.has_movedir = vars_snapshot.has_movedir;
        record.mins = vars_snapshot.mins;
        record.maxs = vars_snapshot.maxs;
        record.has_size = vars_snapshot.has_size;
    }

    if (snapshot.has_origin)
    {
        record.has_origin = true;
        record.origin = snapshot.origin;
        if (record.origin_raw.empty())
        {
            record.origin_raw = snapshot.origin_string.empty()
                ? FormatVector(snapshot.origin)
                : snapshot.origin_string;
        }
    }

    if (snapshot.has_angles)
    {
        record.has_angles = true;
        record.angles = snapshot.angles;
        if (record.angles_raw.empty())
        {
            record.angles_raw = snapshot.angles_string.empty()
                ? FormatVector(snapshot.angles)
                : snapshot.angles_string;
        }
    }

    if (!snapshot.model_string.empty())
    {
        record.model = snapshot.model_string;
    }

    record.modelindex = snapshot.model_index;
    record.spawned = snapshot.spawned;
    record.deferred = snapshot.deferred;
    record.private_data_present = snapshot.private_data_present;
    record.has_private_data = snapshot.private_data_present;
    record.activation_candidate = snapshot.activation_candidate;

    record.scripted_actor_name.clear();
    record.scripted_play.clear();
    record.scripted_idle.clear();
    record.scripted_move_to = 0;
    record.scripted_radius = 0.0f;
    record.scripted_spawnflags = 0;
    if (const hl::game_api::detail::EntityDefinition* definition =
            FindParsedEntityDefinitionByOrdinal(state, record.parse_index);
        definition != nullptr
        && EqualsIgnoreCase(record.classname, "scripted_sequence"))
    {
        const ScriptedSequenceFields fields = ExtractScriptedSequenceFields(definition);
        record.scripted_actor_name = fields.actor_name;
        record.scripted_play = fields.play;
        record.scripted_idle = fields.idle;
        record.scripted_move_to = fields.move_to;
        record.scripted_radius = fields.radius;
        record.scripted_spawnflags = fields.spawnflags;
    }

    record.lifecycle_state = ClassifyRuntimeLifecycleState(record);
    RefreshRuntimeMapLogicFlags(record);
}

void ResetRuntimeRecordMapLogicFrameState(EngineShimState& state)
{
    for (RuntimeEntityRecord& record : state.entity_bootstrap.runtime_entities)
    {
        record.triggered_this_frame = false;
        hl::game_api::detail::ResetScriptedLogicFrameState(record.scripted_logic);
        RefreshRuntimeMapLogicFlags(record);
    }
}

std::vector<hl::game_api::detail::TrackPathNodeInput> BuildTrackPathInputs(
    const EngineShimState& state)
{
    std::vector<hl::game_api::detail::TrackPathNodeInput> inputs;
    for (const RuntimeEntityRecord& record : state.entity_bootstrap.runtime_entities)
    {
        if (!EqualsIgnoreCase(record.classname, "path_track"))
        {
            continue;
        }

        hl::game_api::detail::TrackPathNodeInput input;
        input.parse_index = record.parse_index;
        input.edict_index = record.edict_index;
        input.targetname = record.targetname;
        input.next_target = record.target;
        input.next_target_terminal_dead_end =
            !input.next_target.empty()
            && state.path_track_terminal_dead_end_targets.find(ToLowerCopy(input.next_target))
                != state.path_track_terminal_dead_end_targets.end();
        input.origin = record.origin;
        input.has_origin = record.has_origin;
        input.in_use = record.in_use;
        input.removed = record.removed;
        if (const hl::game_api::detail::EntityDefinition* definition =
                FindParsedEntityDefinitionByOrdinal(state, record.parse_index);
            definition != nullptr)
        {
            if (const std::string* message = FindLastKeyValue(*definition, "message");
                message != nullptr)
            {
                input.message_target = *message;
            }
            input.speed = ExtractPathTrackSpeed(definition);
        }

        inputs.push_back(std::move(input));
    }

    return inputs;
}

std::vector<hl::game_api::detail::ScriptedMovementEntityView> BuildScriptedMovementEntityViews(
    const EngineShimState& state)
{
    std::vector<hl::game_api::detail::ScriptedMovementEntityView> views;
    views.reserve(state.entity_bootstrap.runtime_entities.size());

    for (const RuntimeEntityRecord& record : state.entity_bootstrap.runtime_entities)
    {
        hl::game_api::detail::ScriptedMovementEntityView view;
        view.edict_index = record.edict_index;
        view.parse_index = record.parse_index;
        view.classname = record.classname;
        view.targetname = record.targetname;
        view.target = record.target;
        view.message = record.message;
        view.actor_name = record.scripted_actor_name;
        view.play = record.scripted_play;
        view.idle = record.scripted_idle;
        view.move_to = record.scripted_move_to;
        view.radius = record.scripted_radius;
        view.spawnflags = record.scripted_spawnflags;
        view.origin = record.origin;
        view.has_origin = record.has_origin;
        view.angles = record.angles;
        view.has_angles = record.has_angles;
        view.origin_text = record.origin_raw;
        view.angles_text = record.angles_raw;
        view.model = record.model;
        view.modelindex = record.modelindex;
        view.in_use = record.in_use;
        view.removed = record.removed;
        view.spawned = record.spawned;
        view.deferred = record.deferred;
        view.has_private_data = record.has_private_data;
        view.scheduled_for_think = record.scheduled_for_think;
        view.flags = record.flags;
        view.solid = record.solid;
        view.movetype = record.movetype;
        view.effects = record.effects;
        view.received_use_count = record.scripted_logic.received_use_count;
        view.use_successes = static_cast<int>(record.use_successes);
        view.pending_scheduled_outputs = record.pending_scheduled_outputs;
        view.last_trigger_frame = record.scripted_logic.last_trigger_frame;
        view.last_trigger_time = record.scripted_logic.last_trigger_time;
        view.last_source_entity = record.scripted_logic.last_source_entity;
        view.internal_state_changed = record.scripted_logic.internal_state_changed;
        view.scheduled_follow_up = record.scripted_logic.scheduled_follow_up;
        view.emitted_targets = record.scripted_logic.emitted_targets;
        view.progressed_this_run = record.scripted_logic.progressed_this_run;

        if (const hl::game_api::detail::EntityDefinition* definition =
                FindParsedEntityDefinitionByOrdinal(state, record.parse_index);
            definition != nullptr)
        {
            view.delay = ExtractEntityDelay(definition);
            if (EqualsIgnoreCase(record.classname, "path_track"))
            {
                view.speed = ExtractPathTrackSpeed(definition);
            }
            else if (EqualsIgnoreCase(record.classname, "func_tracktrain"))
            {
                const TrackTrainFields train = ExtractTrackTrainFields(definition);
                view.speed = train.speed;
                view.start_speed = train.start_speed;
            }
        }

        views.push_back(std::move(view));
    }

    return views;
}

void ApplySceneMovementStateToRuntimeRecord(
    RuntimeEntityRecord& record,
    const hl::game_api::detail::ScriptedMovementController& controller)
{
    if (!EqualsIgnoreCase(record.classname, "scripted_sequence"))
    {
        return;
    }

    const hl::game_api::ScriptedSceneRuntimeSummary* scene =
        controller.FindSceneState(record.edict_index);
    if (scene == nullptr)
    {
        record.scripted_actor_resolved = false;
        record.scripted_actor_edict = -1;
        record.scripted_movement_stage.clear();
        record.scripted_movement_support_state.clear();
        record.scripted_arrived = false;
        record.scripted_animation_ready = false;
        return;
    }

    record.scripted_actor_resolved = scene->actor_resolved;
    record.scripted_actor_edict = scene->actor_edict_index;
    record.scripted_actor_classname = scene->actor_classname;
    record.scripted_movement_stage = scene->stage;
    record.scripted_movement_support_state = scene->support_state;
    record.scripted_arrived =
        EqualsIgnoreCase(scene->stage, "arrived")
        || EqualsIgnoreCase(scene->stage, "animation-ready")
        || EqualsIgnoreCase(scene->stage, "completed");
    record.scripted_animation_ready =
        EqualsIgnoreCase(scene->stage, "animation-ready")
        || EqualsIgnoreCase(scene->stage, "completed");
    record.scripted_blocked_reason = scene->blocked_reason;
    record.scripted_logic.actor_exists = scene->actor_resolved;
    if (!scene->blocked_reason.empty())
    {
        record.scripted_logic.blocked_reason = scene->blocked_reason;
    }
    else if (controller.DidSceneMoveThisFrame(record.edict_index)
        || controller.DidSceneArriveThisFrame(record.edict_index)
        || controller.DidSceneStageChangeThisFrame(record.edict_index))
    {
        record.scripted_logic.blocked_reason.clear();
        hl::game_api::detail::MarkScriptedLogicProgressed(record.scripted_logic);
    }
}

void MergePathMoverStateIntoScriptedMovementSummary(
    hl::game_api::ScriptedMovementStateSummary& movement_summary,
    const hl::game_api::detail::PathMoverControllerStateSummary& path_summary)
{
    movement_summary.path_graph = path_summary.path_graph;
    movement_summary.path_movers = path_summary.path_movers;
    movement_summary.path_arrival_epsilon = path_summary.effective_arrival_epsilon;
    movement_summary.path_snap_to_node_occurred = path_summary.snap_to_node_occurred;
    movement_summary.flatbedstart_resolved = path_summary.flatbedstart_resolved;
    movement_summary.delayed_ftruck_status = path_summary.delayed_ftruck_status;
    movement_summary.ftruck_final_current = path_summary.ftruck_final_current;
    movement_summary.ftruck_final_next = path_summary.ftruck_final_next;
    movement_summary.ftruck_advanced_beyond_trainstop1a =
        path_summary.ftruck_advanced_beyond_trainstop1a;
    movement_summary.path_node_messages = path_summary.path_node_messages;
    movement_summary.path_messages_encountered =
        path_summary.path_node_messages.encountered_history;
    movement_summary.path_node_message_dispatch_count =
        path_summary.path_node_messages.staged_dispatch_attempts;
    movement_summary.path_node_message_triggered_dispatch =
        path_summary.path_node_messages.staged_dispatch_successes > 0;
    movement_summary.path_message_dispatch_history =
        path_summary.path_node_messages.dispatch_history;
    movement_summary.broken_path_link_events = path_summary.broken_link_events;
    movement_summary.path_movers_preview = path_summary.movers_preview;

    std::unordered_map<int, const hl::game_api::detail::PathMoverFrameState*> path_frames;
    for (const hl::game_api::detail::PathMoverFrameState& frame : path_summary.frames)
    {
        path_frames[frame.frame_number] = &frame;
    }

    for (hl::game_api::ScriptedMovementFrameStateSummary& frame : movement_summary.frames)
    {
        const auto it = path_frames.find(frame.frame_number);
        if (it == path_frames.end() || it->second == nullptr)
        {
            continue;
        }

        const hl::game_api::detail::PathMoverFrameState& path_frame = *it->second;
        frame.path_movers_active = path_frame.active_movers;
        frame.path_movers_moving = path_frame.moving_movers;
        frame.blocked_path_movers = path_frame.blocked_movers;
        frame.stopped_path_movers = path_frame.stopped_movers;
        frame.path_node_arrivals = path_frame.node_arrivals;
        frame.path_nodes_advanced = path_frame.path_advances;
        frame.ftruck_status = path_frame.ftruck_status;
        frame.active_path_movers = path_frame.active_movers_preview;
        frame.path_messages_this_frame = path_frame.messages_this_frame;
        frame.path_events = path_frame.events;
    }

    movement_summary.readiness =
        movement_summary.path_node_messages.staged_dispatch_successes > 0
        ? "ready for deeper path-node event semantics, passive actor/server groundwork, and later controlled client bootstrap groundwork"
        : movement_summary.path_node_messages.encountered > 0
        ? "path-node messages are surfacing; deepen staged dispatch semantics and controlled event-chain expansion next"
        : movement_summary.path_movers.path_advances > 0
        ? "path traversal is stable; deepen path-node event semantics and passive actor/server groundwork next"
        : movement_summary.path_movers.node_arrivals > 0
            || movement_summary.path_movers.moving > 0
        ? "path traversal is active; continue probing deeper nodes and staged path-node events"
        : movement_summary.scene_movement_successes > 0
        ? "scripted actor movement is stable; resolve remaining path mover bind or progression blockers next"
        : !movement_summary.movement_blocked_reasons.empty()
        ? "movement bootstrap is diagnosable; resolve the top blocked reason next"
        : "movement bootstrap is configured; longer deterministic runs can probe delayed movers";
}

void MarkRuntimeRecordTriggeredThisFrame(
    EngineShimState& state,
    int edict_index,
    int frame_number)
{
    RuntimeEntityRecord* record = FindRuntimeRecordByEdictIndex(state, edict_index);
    if (record == nullptr)
    {
        if (edict_t* entity = state.edict_store.EntityOfIndex(edict_index);
            entity != nullptr)
        {
            record = EnsureRuntimeRecordForEntity(state, entity, "map-logic-trigger");
            if (record != nullptr)
            {
                SyncRuntimeRecordFromEdict(state, entity, *record);
            }
        }
    }

    if (record == nullptr)
    {
        return;
    }

    record->triggered_this_frame = true;
    record->last_triggered_frame = frame_number;
    RefreshRuntimeMapLogicFlags(*record);
}

void NoteRuntimeRecordTargetEmission(
    EngineShimState& state,
    int source_edict_index,
    std::string_view source_classname,
    std::string_view target_name,
    int frame_number,
    float trigger_time)
{
    RuntimeEntityRecord* record = FindRuntimeRecordByEdictIndex(state, source_edict_index);
    if (record == nullptr || !hl::game_api::detail::IsRelevantScriptedLogicClass(record->classname))
    {
        return;
    }

    hl::game_api::detail::NoteScriptedLogicTargetEmission(
        record->scripted_logic,
        frame_number,
        trigger_time,
        BuildSourceEntityLabel(state, source_edict_index, source_classname));
    record->scripted_logic.support_state =
        hl::game_api::ScriptedLogicSupportState::kScriptedProgressing;
    RefreshRuntimeMapLogicFlags(*record);
    (void)target_name;
}

void SetRuntimeRecordPendingScheduledOutputs(
    EngineShimState& state,
    int edict_index,
    int pending_count)
{
    RuntimeEntityRecord* record = FindRuntimeRecordByEdictIndex(state, edict_index);
    if (record == nullptr)
    {
        return;
    }

    record->pending_scheduled_outputs = std::max(0, pending_count);
    RefreshRuntimeMapLogicFlags(*record);
}

void NoteRuntimeRecordScheduledActionEvent(
    EngineShimState& state,
    const hl::game_api::detail::ScheduledUseAction& action,
    std::string_view event_name,
    std::string_view detail,
    int frame_number,
    float trigger_time)
{
    RuntimeEntityRecord* record = FindRuntimeRecordByEdictIndex(state, action.source_edict_index);
    if (record == nullptr || !hl::game_api::detail::IsRelevantScriptedLogicClass(record->classname))
    {
        return;
    }

    if (event_name == "queued")
    {
        hl::game_api::detail::NoteScriptedLogicScheduledOutput(
            record->scripted_logic,
            frame_number,
            trigger_time,
            BuildSourceEntityLabel(state, action.source_edict_index, action.source_classname));
    }
    else if (event_name == "failed" || event_name == "deferred" || event_name == "expired")
    {
        hl::game_api::detail::MarkScriptedLogicBlocked(
            record->scripted_logic,
            hl::game_api::ScriptedLogicSupportState::kBlockedOnEngineCallback,
            std::string(detail));
    }

    RefreshRuntimeMapLogicFlags(*record);
}

void AccumulateRuntimeRecordTargetResolution(EngineShimState& state, int edict_index)
{
    RuntimeEntityRecord* record = FindRuntimeRecordByEdictIndex(state, edict_index);
    if (record == nullptr)
    {
        return;
    }

    ++record->target_resolutions;
}

void AccumulateRuntimeRecordUseStats(
    EngineShimState& state,
    int edict_index,
    int source_edict_index,
    std::string_view source_classname,
    bool attempted,
    bool succeeded,
    bool deferred,
    std::string_view detail,
    int frame_number,
    float trigger_time)
{
    RuntimeEntityRecord* record = FindRuntimeRecordByEdictIndex(state, edict_index);
    if (record == nullptr)
    {
        return;
    }

    if (attempted)
    {
        ++record->use_attempts;
    }
    if (succeeded)
    {
        ++record->use_successes;
    }
    else if (attempted)
    {
        ++record->use_failures;
    }
    if (deferred)
    {
        ++record->use_deferred;
    }

    if (hl::game_api::detail::IsRelevantScriptedLogicClass(record->classname))
    {
        if (attempted || succeeded)
        {
            hl::game_api::detail::NoteScriptedLogicUse(
                record->scripted_logic,
                frame_number,
                trigger_time,
                BuildSourceEntityLabel(state, source_edict_index, source_classname));
        }

        if (succeeded)
        {
            record->scripted_logic.support_state =
                hl::game_api::ScriptedLogicSupportState::kUseSupported;
        }

        if (deferred)
        {
            record->scripted_logic.blocked_reason = std::string(detail);
        }
        else if (attempted && !succeeded)
        {
            hl::game_api::detail::MarkScriptedLogicBlocked(
                record->scripted_logic,
                hl::game_api::ScriptedLogicSupportState::kBlockedOnEngineCallback,
                std::string(detail));
        }
    }
}

hl::game_api::MapLogicSupportState ClassifyMapLogicSupportFromSnapshot(
    const hl::game_api::detail::EntityVarSnapshot& snapshot)
{
    if (!snapshot.valid || snapshot.removed || !snapshot.in_use || (snapshot.flags & FL_KILLME) != 0)
    {
        return hl::game_api::MapLogicSupportState::kRemovedByGameLogic;
    }

    const std::string normalized = ToLowerCopy(snapshot.classname);
    if (normalized == "trigger_auto"
        || normalized == "multi_manager"
        || normalized == "env_message"
        || normalized == "ambient_generic"
        || normalized == "scripted_sequence"
        || normalized == "func_tracktrain")
    {
        return hl::game_api::MapLogicSupportState::kUseSupported;
    }

    if (normalized == "trigger_changelevel")
    {
        return hl::game_api::MapLogicSupportState::kPassiveRecipientOnly;
    }

    if ((normalized == "scripted_sequence" || normalized.rfind("scripted_", 0) == 0)
        && normalized != "scripted_sequence")
    {
        return hl::game_api::MapLogicSupportState::kDetectedButDeferred;
    }

    if (snapshot.deferred)
    {
        return hl::game_api::MapLogicSupportState::kDetectedButDeferred;
    }

    return hl::game_api::MapLogicSupportState::kPassiveRecipientOnly;
}

bool AllowUseForMapLogicSnapshot(const hl::game_api::detail::EntityVarSnapshot& snapshot)
{
    const std::string normalized = ToLowerCopy(snapshot.classname);
    return !snapshot.removed
        && snapshot.in_use
        && (normalized == "env_message"
            || normalized == "ambient_generic"
            || normalized == "env_glow"
            || normalized == "multi_manager"
            || normalized == "trigger_auto"
            || normalized == "scripted_sequence"
            || normalized == "func_tracktrain");
}

bool IsEngineManagedTriggerAuto(const hl::game_api::detail::EntityVarSnapshot& snapshot)
{
    return snapshot.valid
        && snapshot.in_use
        && !snapshot.removed
        && EqualsIgnoreCase(snapshot.classname, "trigger_auto");
}

void MarkRuntimeRecordThinkDeferred(
    EngineShimState& state,
    int edict_index,
    int frame_number,
    std::string_view reason)
{
    RuntimeEntityRecord* record =
        FindRuntimeRecordByEdictIndex(state, edict_index);
    if (record == nullptr)
    {
        if (edict_t* entity = state.edict_store.EntityOfIndex(edict_index);
            entity != nullptr)
        {
            record = EnsureRuntimeRecordForEntity(state, entity, "think-deferred");
        }
    }

    if (record == nullptr)
    {
        return;
    }

    if (!reason.empty())
    {
        AppendRecordNote(*record, "think deferred: " + std::string(reason));
    }

    record->scheduled_for_think = true;
    record->think_succeeded = false;
    record->last_think_frame = frame_number;
    record->lifecycle_state = ClassifyRuntimeLifecycleState(*record);
    RefreshRuntimeMapLogicFlags(*record);
}

void MarkRuntimeRecordThinkAttempt(
    EngineShimState& state,
    int edict_index,
    int frame_number,
    bool succeeded)
{
    RuntimeEntityRecord* record =
        FindRuntimeRecordByEdictIndex(state, edict_index);
    edict_t* entity = state.edict_store.EntityOfIndex(edict_index);
    if (record == nullptr && entity != nullptr)
    {
        record = EnsureRuntimeRecordForEntity(state, entity, "think-attempt");
    }

    if (record == nullptr)
    {
        return;
    }

    record->think_attempted = true;
    record->think_succeeded = succeeded;
    record->last_think_frame = frame_number;
    if (entity != nullptr)
    {
        SyncRuntimeRecordFromEdict(state, entity, *record);
    }
    record->lifecycle_state = ClassifyRuntimeLifecycleState(*record);
    RefreshRuntimeMapLogicFlags(*record);
}

void SweepKilledEntities(EngineShimState& state)
{
    for (int index = 1; index < state.edict_store.MaxEntities(); ++index)
    {
        edict_t* entity = state.edict_store.EntityOfIndex(index);
        if (entity == nullptr)
        {
            continue;
        }

        hl::game_api::detail::EntityVarSnapshot vars_snapshot;
        if (!hl::game_api::detail::TryReadEntityVars(
                state.edict_store,
                state.string_pool,
                entity,
                &vars_snapshot,
                [&](std::string_view message)
                {
                    hl::common::Logger::Warn(std::string(message));
                }))
        {
            continue;
        }

        if ((vars_snapshot.flags & FL_KILLME) == 0)
        {
            continue;
        }

        hl::common::Logger::Info(
            "EntityThinkScheduler: sweeping FL_KILLME edict#" + std::to_string(index)
                + " classname="
                + (vars_snapshot.classname.empty() ? std::string("<empty>") : vars_snapshot.classname)
                + ".");
        state.edict_store.RemoveEntity(entity);
        if (RuntimeEntityRecord* record = FindRuntimeRecordByEdictIndex(state, index);
            record != nullptr)
        {
            SyncRuntimeRecordFromEdict(state, entity, *record);
            record->removed = true;
            record->lifecycle_state = RuntimeEntityLifecycleState::kRemovedByGameLogic;
            RefreshRuntimeMapLogicFlags(*record);
        }
    }
}

bool SafeCallLinkEntityExport(LinkEntityExportFn function, entvars_t* variables)
{
    __try
    {
        function(variables);
        return true;
    }
    __except (HandleSehException("entity link export", GetExceptionCode()))
    {
        return false;
    }
}

bool BindEntityClassExport(
    EngineShimState& state,
    edict_t* entity,
    std::string_view classname,
    std::string_view log_context)
{
    if (entity == nullptr || classname.empty())
    {
        return false;
    }

    if (state.module == nullptr || !state.module->IsLoaded())
    {
        hl::common::Logger::Warn(
            "Entity bootstrap: " + std::string(log_context)
            + " cannot bind class export because hl.dll module is unavailable.");
        return false;
    }

    if (state.edict_store.PrivateDataOf(entity) != nullptr)
    {
        return true;
    }

    const std::string symbol_name(classname);
    LinkEntityExportFn function =
        reinterpret_cast<LinkEntityExportFn>(state.module->GetSymbolRaw(symbol_name.c_str()));
    if (function == nullptr)
    {
        hl::common::Logger::Warn(
            "Entity bootstrap: no linked export for classname '" + symbol_name
            + "' (" + std::string(log_context) + ").");
        return false;
    }

    hl::common::Logger::Info(
        "Entity bootstrap: binding classname '" + symbol_name + "' for "
        + std::string(log_context) + " (" + DescribeEdict(state, entity) + ").");

    if (!SafeCallLinkEntityExport(function, &entity->v))
    {
        return false;
    }

    const bool created_private_data = state.edict_store.PrivateDataOf(entity) != nullptr;
    hl::common::Logger::Info(
        "Entity bootstrap: class binding result for '" + symbol_name + "': "
        + std::string(created_private_data ? "private data allocated" : "no private data"));
    return created_private_data;
}

edict_t* CreateNamedEntityForMap(
    EngineShimState& state,
    const hl::game_api::detail::EntityDefinition& entity)
{
    edict_t* created = state.edict_store.CreateEntity();
    if (created == nullptr)
    {
        hl::common::Logger::Warn(
            "Entity bootstrap: no free edict slot for map entity #"
            + std::to_string(entity.ordinal) + " (" + SafeClassname(entity) + ").");
        return nullptr;
    }

    const string_t class_name = state.string_pool.Alloc(entity.classname);
    state.edict_store.SetClassname(created, class_name, entity.classname);
    state.edict_store.SetParseIndex(created, static_cast<int>(entity.ordinal));

    BindEntityClassExport(
        state,
        created,
        entity.classname,
        "map entity #" + std::to_string(entity.ordinal));
    return created;
}

void ApplyParsedCommonFields(
    EngineShimState& state,
    edict_t* entity,
    const hl::game_api::detail::EntityDefinition& definition,
    RuntimeEntityRecord& record,
    bool preserve_existing_model)
{
    if (entity == nullptr)
    {
        return;
    }

    state.edict_store.SetParseIndex(entity, static_cast<int>(definition.ordinal));
    record.classname = definition.classname;

    if (const std::string* targetname = FindLastKeyValue(definition, "targetname");
        targetname != nullptr && !targetname->empty())
    {
        state.edict_store.SetTargetname(entity, state.string_pool.Alloc(*targetname), *targetname);
        record.targetname = *targetname;
    }

    if (!preserve_existing_model)
    {
        if (const std::string* model = FindLastKeyValue(definition, "model");
            model != nullptr && !model->empty())
        {
            state.edict_store.SetModel(
                entity,
                state.string_pool.Alloc(*model),
                *model,
                entity->v.modelindex);
            record.model = *model;
            record.modelindex = entity->v.modelindex;
        }
    }

    const ParsedVectorField origin = ParseOriginField(definition);
    if (origin.present)
    {
        record.origin_raw = origin.raw_text;
        if (origin.valid)
        {
            state.edict_store.SetOrigin(entity, origin.value, origin.raw_text);
            record.origin = origin.value;
            record.has_origin = true;
        }
        else
        {
            AppendRecordNote(record, "invalid origin");
            hl::common::Logger::Warn(
                "Entity bootstrap: invalid origin for entity #"
                + std::to_string(definition.ordinal) + " [" + origin.raw_text + "].");
        }
    }

    const ParsedVectorField angles = ParseAnglesField(definition);
    if (angles.present)
    {
        record.angles_raw = angles.raw_text;
        if (angles.valid)
        {
            state.edict_store.SetAngles(entity, angles.value, angles.raw_text);
            record.angles = angles.value;
            record.has_angles = true;
        }
        else
        {
            AppendRecordNote(record, "invalid angles");
            hl::common::Logger::Warn(
                "Entity bootstrap: invalid angles for entity #"
                + std::to_string(definition.ordinal) + " [" + angles.raw_text + "].");
        }
    }

    SyncRuntimeRecordFromEdict(state, entity, record);
}

bool DispatchKeyValueToDll(
    EngineShimState& state,
    edict_t* entity,
    const hl::game_api::detail::EntityDefinition& definition,
    const hl::game_api::detail::EntityKeyValuePair& key_value,
    RuntimeEntityRecord& record,
    EntityBootstrapContext& context)
{
    std::string classname = record.classname.empty() ? definition.classname : record.classname;
    std::string key = key_value.key;
    std::string value = key_value.value;

    hl::common::Logger::Info(
        "Entity bootstrap keyvalue: #" + std::to_string(definition.ordinal)
        + " " + (classname.empty() ? std::string("<missing>") : classname)
        + " [" + key + "=" + value + "]");

    ++context.keyvalues_dispatched;

    if (state.dll_functions.pfnKeyValue == nullptr)
    {
        hl::common::Logger::Warn("Entity bootstrap: hl.dll did not expose pfnKeyValue.");
        record.unhandled_keyvalues.push_back(key + "=" + value);
        return false;
    }

    KeyValueData kvd{};
    kvd.szClassName = classname.empty() ? nullptr : classname.data();
    kvd.szKeyName = key.data();
    kvd.szValue = value.data();
    kvd.fHandled = FALSE;

    if (!SafeCallKeyValue(state.dll_functions.pfnKeyValue, entity, &kvd))
    {
        record.unhandled_keyvalues.push_back(key + "=" + value);
        AppendRecordNote(record, "pfnKeyValue SEH");
        return false;
    }

    if (kvd.fHandled != FALSE)
    {
        ++context.keyvalues_handled;
        hl::common::Logger::Info("Entity bootstrap keyvalue result: handled");
    }
    else
    {
        record.unhandled_keyvalues.push_back(key + "=" + value);
        hl::common::Logger::Info("Entity bootstrap keyvalue result: unhandled");
    }

    SyncRuntimeRecordFromEdict(state, entity, record);
    return kvd.fHandled != FALSE;
}

bool IsWorldspawnRecord(const RuntimeEntityRecord& record)
{
    return EqualsIgnoreCase(record.classname, "worldspawn");
}

bool ValidateWorldspawnPreflight(
    EngineShimState& state,
    edict_t* entity,
    const RuntimeEntityRecord& record)
{
    const hl::game_api::detail::EntityStateSnapshot snapshot =
        state.edict_store.SnapshotOf(entity, state.string_pool);
    const bool edict_valid = entity != nullptr && snapshot.index == 0;
    const bool in_use = snapshot.in_use;
    const bool classname_valid = EqualsIgnoreCase(snapshot.classname, "worldspawn");
    const bool model_string_valid = !snapshot.model_string.empty();
    const bool modelindex_valid = snapshot.model_index > 0;
    const bool private_data_present = state.edict_store.PrivateDataOf(entity) != nullptr;
    const std::string globals_map_name = state.string_pool.Describe(state.globalvars.mapname);
    const bool globals_sane =
        !globals_map_name.empty() && state.globalvars.maxClients > 0 && state.globalvars.time >= 0.0f;
    const bool server_state_exists = !state.server_state.game_directory.empty()
        && !state.server_state.map_name.empty();
    const bool world_model_registered =
        state.world_context.world_model_index > 0
        && state.precache_registry.ModelName(state.world_context.world_model_index) != nullptr;

    hl::common::Logger::Info("Worldspawn spawn validation:");
    hl::common::Logger::Info("  - edict#0 valid: " + std::string(BoolToYesNo(edict_valid)));
    hl::common::Logger::Info("  - edict in use: " + std::string(BoolToYesNo(in_use)));
    hl::common::Logger::Info("  - classname valid: " + std::string(BoolToYesNo(classname_valid)));
    hl::common::Logger::Info(
        "  - model string valid: " + std::string(BoolToYesNo(model_string_valid))
        + " [" + (snapshot.model_string.empty() ? std::string("<empty>") : snapshot.model_string) + "]");
    hl::common::Logger::Info(
        "  - modelindex valid: " + std::string(BoolToYesNo(modelindex_valid))
        + " [" + std::to_string(snapshot.model_index) + "]");
    hl::common::Logger::Info(
        "  - private data present: " + std::string(BoolToYesNo(private_data_present)));
    hl::common::Logger::Info(
        "  - gpGlobals sane: " + std::string(BoolToYesNo(globals_sane))
        + " [mapname=" + (globals_map_name.empty() ? std::string("<empty>") : globals_map_name)
        + ", time=" + std::to_string(state.globalvars.time)
        + ", maxClients=" + std::to_string(state.globalvars.maxClients) + "]");
    hl::common::Logger::Info(
        "  - server state exists: " + std::string(BoolToYesNo(server_state_exists)));
    hl::common::Logger::Info(
        "  - world model registered: " + std::string(BoolToYesNo(world_model_registered)));
    hl::common::Logger::Info(
        "  - runtime summary: " + BuildRuntimeRecordSummary(record));

    return edict_valid
        && in_use
        && classname_valid
        && model_string_valid
        && modelindex_valid
        && globals_sane
        && server_state_exists
        && world_model_registered;
}

bool SafeCallWorldspawnSpawn(
    EngineShimState& state,
    int (*function)(edict_t*),
    edict_t* entity,
    const RuntimeEntityRecord& record,
    int* result_value)
{
    state.worldspawn_spawn_diagnostics.BeginAttempt(
        state.edict_store.IndexOf(entity),
        record.classname,
        state.server_state.map_name,
        state.random_diagnostics.random_long_calls,
        state.random_diagnostics.random_float_calls);

    if (!ValidateWorldspawnPreflight(state, entity, record))
    {
        hl::common::Logger::Warn(
            "Worldspawn spawn preflight failed; aborting spawn call to keep host alive.");
        LogServerStateSnapshot(state, "Worldspawn preflight server snapshot:");
        LogCommandQueueSnapshot(
            state.command_buffer.Snapshot(),
            "Worldspawn preflight command queue snapshot:");
        return false;
    }

    unsigned int seh_code = 0;
    if (SafeCallWorldspawnSpawnSeh(function, entity, result_value, &seh_code))
    {
        state.worldspawn_spawn_diagnostics.MarkSucceeded();
        return true;
    }

    state.worldspawn_spawn_diagnostics.MarkSehException(seh_code);
    hl::common::Logger::Error(
        "Worldspawn spawn raised SEH " + FormatExceptionCode(seh_code)
        + " for edict#" + std::to_string(state.edict_store.IndexOf(entity))
        + " classname=" + record.classname
        + " map=" + state.server_state.map_name);
    LogWorldspawnTraceTail(state);
    LogWorldspawnDistinctCallbackTail(state);
    LogWorldspawnPrecacheTail(state);
    LogRandomDiagnostics(state, "Worldspawn crash random diagnostics:");
    LogServerStateSnapshot(state, "Worldspawn crash server snapshot:");
    LogCommandQueueSnapshot(
        state.command_buffer.Snapshot(),
        "Worldspawn crash command queue snapshot:");
    return false;
}

bool SpawnEntityFromDll(
    EngineShimState& state,
    edict_t* entity,
    RuntimeEntityRecord& record,
    EntityBootstrapContext& context)
{
    ++context.spawn_attempts;
    record.spawn_attempted = true;

    if (state.dll_functions.pfnSpawn == nullptr)
    {
        hl::common::Logger::Warn("Entity bootstrap: hl.dll did not expose pfnSpawn.");
        AppendRecordNote(record, "pfnSpawn missing");
        record.support_state = RuntimeEntitySupportState::kFailedDuringSpawn;
        if (!IsWorldspawnRecord(record))
        {
            state.edict_store.RemoveEntity(entity);
        }
        SyncRuntimeRecordFromEdict(state, entity, record);
        return false;
    }

    hl::common::Logger::Info(
        "Entity bootstrap spawn: attempting " + BuildRuntimeRecordSummary(record));

    int result = 0;
    const bool spawn_succeeded = IsWorldspawnRecord(record)
        ? SafeCallWorldspawnSpawn(state, state.dll_functions.pfnSpawn, entity, record, &result)
        : SafeCallSpawn(state.dll_functions.pfnSpawn, entity, &result);
    if (!spawn_succeeded)
    {
        if (IsWorldspawnRecord(record) && !state.worldspawn_spawn_diagnostics.SawSehException())
        {
            AppendRecordNote(record, "worldspawn spawn blocked");
        }
        else
        {
            AppendRecordNote(record, "pfnSpawn SEH");
        }
        record.support_state = RuntimeEntitySupportState::kFailedDuringSpawn;
        if (!IsWorldspawnRecord(record) && entity != nullptr)
        {
            state.edict_store.RemoveEntity(entity);
        }
        SyncRuntimeRecordFromEdict(state, entity, record);
        return false;
    }

    hl::common::Logger::Info(
        "Entity bootstrap spawn: pfnSpawn returned " + std::to_string(result));

    if (result < 0)
    {
        if (IsWorldspawnRecord(record))
        {
            state.worldspawn_spawn_diagnostics.SetSucceeded(false);
            state.edict_store.SetSpawned(entity, false);
            AppendRecordNote(record, "worldspawn requested removal");
            record.support_state = RuntimeEntitySupportState::kFailedDuringSpawn;
            SyncRuntimeRecordFromEdict(state, entity, record);
            return false;
        }
        state.edict_store.RemoveEntity(entity);
        AppendRecordNote(record, "spawn requested removal");
        record.support_state = RuntimeEntitySupportState::kRemovedDuringSpawn;
        SyncRuntimeRecordFromEdict(state, entity, record);
        return false;
    }

    SyncRuntimeRecordFromEdict(state, entity, record);
    if (!record.in_use)
    {
        if (IsWorldspawnRecord(record))
        {
            state.worldspawn_spawn_diagnostics.SetSucceeded(false);
            record.support_state = RuntimeEntitySupportState::kFailedDuringSpawn;
        }
        else
        {
            record.support_state = RuntimeEntitySupportState::kRemovedDuringSpawn;
        }
        AppendRecordNote(record, "entity removed during spawn");
        return false;
    }

    state.edict_store.SetSpawned(entity, true);
    state.edict_store.SetDeferred(entity, false);
    state.edict_store.SetActivationCandidate(entity, false);
    SyncRuntimeRecordFromEdict(state, entity, record);
    record.support_state = RuntimeEntitySupportState::kSpawnedSuccessfully;
    ++context.successful_spawns;
    return true;
}

std::size_t SumCallbackCounts(
    const EngineShimState& state,
    std::initializer_list<std::string_view> callback_names)
{
    std::size_t total = 0;
    for (const std::string_view callback_name : callback_names)
    {
        const auto it = state.callback_counts.find(std::string(callback_name));
        if (it != state.callback_counts.end())
        {
            total += it->second;
        }
    }

    return total;
}

void PopulateBootstrapSummary(
    hl::game_api::HlServerModuleSummary& summary,
    const EngineShimState& state)
{
    constexpr std::array<std::size_t, 7> kKeyLumpIndices = {{0, 1, 2, 3, 5, 7, 14}};

    summary.server_state = {
        state.server_state.initialized,
        state.server_state.game_directory_utf8,
        state.server_state.mod_name,
        state.server_state.hostname,
        state.server_state.maxclients,
        state.server_state.map_name,
        state.server_state.active,
        state.server_state.loading,
        state.server_state.frame_count,
        state.server_state.server_frame,
        state.server_state.time,
        state.server_state.frametime,
    };

    summary.globals_snapshot = {
        state.string_pool.Describe(state.globalvars.mapname),
        state.string_pool.Describe(state.globalvars.startspot),
        state.globalvars.time,
        state.globalvars.frametime,
        state.globalvars.deathmatch,
        state.globalvars.coop,
        state.globalvars.maxClients,
        state.globalvars.maxEntities,
    };

    summary.precache_callback_invocations = SumCallbackCounts(
        state,
        {"pfnPrecacheModel", "pfnPrecacheSound", "pfnPrecacheGeneric", "pfnPrecacheEvent"});
    summary.model_callback_invocations = SumCallbackCounts(
        state,
        {"pfnModelIndex", "pfnSetModel"});
    summary.string_callback_invocations = SumCallbackCounts(
        state,
        {"pfnAllocString", "pfnSzFromIndex", "pfnMakeString"});
    summary.entity_callback_invocations = SumCallbackCounts(
        state,
        {
            "pfnCreateEntity",
            "pfnCreateNamedEntity",
            "pfnRemoveEntity",
            "pfnPvAllocEntPrivateData",
            "pfnPvEntPrivateData",
            "pfnFreeEntPrivateData",
        });
    summary.ready_for_world_bootstrap =
        state.server_state.initialized && !state.server_state.active && !state.server_state.loading;

    summary.world_bootstrap = {};
    summary.world_bootstrap.attempted = summary.ready_for_world_bootstrap;
    summary.world_bootstrap.completed = state.world_context.prepared;
    summary.world_bootstrap.map_name = state.world_context.map_name;
    summary.world_bootstrap.model_path = state.world_context.model_path;
    summary.world_bootstrap.map_path = !state.world_context.file_path.empty()
        ? hl::common::ToUtf8(state.world_context.file_path)
        : std::string();
    summary.world_bootstrap.bsp_loaded = state.world_context.bsp_loaded;
    summary.world_bootstrap.bsp_version = state.world_context.bsp_version;
    summary.world_bootstrap.bsp_file_size = state.world_context.bsp_file_size;
    summary.world_bootstrap.bsp_lump_count = state.world_context.lumps.size();
    summary.world_bootstrap.world_model_index = state.world_context.world_model_index;
    summary.world_bootstrap.world_edict_index = state.world_context.world_edict_index;
    summary.world_bootstrap.entities_lump_size = state.world_context.entities.byte_size;
    summary.world_bootstrap.edict0_state =
        state.edict_store.DumpEntityState(state.edict_store.World(), state.string_pool);
    summary.world_bootstrap.ready_for_entity_parsing = state.world_context.prepared;
    summary.world_bootstrap.key_lumps.clear();
    if (state.world_context.bsp_loaded)
    {
        for (const std::size_t lump_index : kKeyLumpIndices)
        {
            const hl::game_api::detail::BspLumpMetadata& lump = state.world_context.lumps[lump_index];
            summary.world_bootstrap.key_lumps.push_back(
                {lump.name, lump.file_offset, lump.file_length, lump.within_file});
        }
    }

    summary.ready_for_entity_parsing = summary.world_bootstrap.ready_for_entity_parsing;
    summary.entity_pipeline = {};
    summary.entity_pipeline.attempted = state.entity_bootstrap.attempted;
    summary.entity_pipeline.entities_lump_parsed = state.entity_bootstrap.entities_lump_parsed;
    summary.entity_pipeline.partially_parsed = state.entity_bootstrap.partially_parsed;
    summary.entity_pipeline.failure_reason = state.entity_bootstrap.failure_reason;
    summary.entity_pipeline.parse_error_count = state.entity_bootstrap.parse_errors.size();
    summary.entity_pipeline.total_parsed_entities = state.entity_bootstrap.parsed_entities.size();
    summary.entity_pipeline.total_worldspawn_entities = state.entity_bootstrap.worldspawn_count;
    summary.entity_pipeline.first_entity_is_worldspawn =
        state.entity_bootstrap.first_entity_is_worldspawn;
    summary.entity_pipeline.total_runtime_entities_allocated =
        state.entity_bootstrap.runtime_entities_allocated;
    summary.entity_pipeline.total_keyvalues_dispatched =
        state.entity_bootstrap.keyvalues_dispatched;
    summary.entity_pipeline.total_keyvalues_handled = state.entity_bootstrap.keyvalues_handled;
    summary.entity_pipeline.total_spawn_attempts = state.entity_bootstrap.spawn_attempts;
    summary.entity_pipeline.total_successful_spawns = state.entity_bootstrap.successful_spawns;
    summary.entity_pipeline.total_removed_entities = state.entity_bootstrap.removed_entities;
    summary.entity_pipeline.total_deferred_entities = state.entity_bootstrap.deferred_entities;
    summary.entity_pipeline.top_classname_counts = state.entity_bootstrap.top_classname_counts;
    summary.entity_pipeline.classname_support_summary = state.entity_bootstrap.classname_support_summary;
    summary.entity_pipeline.newly_exercised_engine_callbacks =
        state.entity_bootstrap.newly_exercised_engine_callbacks;

    summary.entity_pipeline.sample_entities.clear();
    for (const RuntimeEntityRecord& record : state.entity_bootstrap.runtime_entities)
    {
        if (summary.entity_pipeline.sample_entities.size() < kEntityDebugLogCount)
        {
            summary.entity_pipeline.sample_entities.push_back(BuildRuntimeRecordSummary(record));
        }

        if (record.spawned && summary.entity_pipeline.sample_spawned_entities.size() < kSampleSpawnedCount)
        {
            summary.entity_pipeline.sample_spawned_entities.push_back(BuildRuntimeRecordSummary(record));
        }
    }

    summary.worldspawn_spawn = {};
    summary.worldspawn_spawn.attempted = state.worldspawn_spawn_diagnostics.Attempted();
    summary.worldspawn_spawn.succeeded = state.worldspawn_spawn_diagnostics.Succeeded();
    summary.worldspawn_spawn.seh_exception = state.worldspawn_spawn_diagnostics.SawSehException();
    summary.worldspawn_spawn.seh_code = state.worldspawn_spawn_diagnostics.ExceptionCode();
    summary.worldspawn_spawn.map_name = state.worldspawn_spawn_diagnostics.MapName();
    summary.worldspawn_spawn.edict_index = state.worldspawn_spawn_diagnostics.EdictIndex();
    summary.worldspawn_spawn.classname = state.worldspawn_spawn_diagnostics.Classname();
    const std::vector<hl::game_api::detail::SpawnTraceEvent> worldspawn_trace =
        state.worldspawn_spawn_diagnostics.TraceSnapshot();
    const std::vector<hl::game_api::detail::SpawnTraceEvent> distinct_trace =
        state.worldspawn_spawn_diagnostics.DistinctCallbackSnapshot();
    const std::vector<hl::game_api::detail::SpawnTraceEvent> precache_trace =
        state.worldspawn_spawn_diagnostics.PrecacheSnapshot();
    summary.worldspawn_spawn.precache_requests = precache_trace.size();
    summary.worldspawn_spawn.sound_precache_count =
        state.worldspawn_spawn_diagnostics.SoundPrecacheCount();
    summary.worldspawn_spawn.sound_registry_size = state.sound_precache_registry.RegistrySize();
    summary.worldspawn_spawn.missing_sound_files =
        state.worldspawn_spawn_diagnostics.MissingSoundFileCount();
    summary.worldspawn_spawn.duplicate_sound_precache_count =
        state.worldspawn_spawn_diagnostics.DuplicateSoundPrecacheCount();
    summary.worldspawn_spawn.random_long_call_count =
        summary.worldspawn_spawn.attempted
        && state.random_diagnostics.random_long_calls
            >= state.worldspawn_spawn_diagnostics.RandomLongBaseline()
            ? state.random_diagnostics.random_long_calls
                - state.worldspawn_spawn_diagnostics.RandomLongBaseline()
            : 0;
    if (const hl::game_api::detail::SpawnTraceEvent* last_event =
            state.worldspawn_spawn_diagnostics.LastEvent();
        last_event != nullptr)
    {
        summary.worldspawn_spawn.last_callback = last_event->callback_name;
    }

    for (const hl::game_api::detail::SpawnTraceEvent& event : distinct_trace)
    {
        summary.worldspawn_spawn.distinct_callback_tail.push_back(FormatSpawnTraceEvent(event));
    }

    for (const hl::game_api::detail::SpawnTraceEvent& event : worldspawn_trace)
    {
        summary.worldspawn_spawn.trace_tail.push_back(FormatSpawnTraceEvent(event));
    }

    for (const hl::game_api::detail::SpawnTraceEvent& event : precache_trace)
    {
        summary.worldspawn_spawn.precache_tail.push_back(FormatSpawnTraceEvent(event));
    }

    summary.server_activation = state.server_activation_state;
    if (summary.server_activation.attempted && summary.server_activation.map_name.empty())
    {
        summary.server_activation.map_name = state.server_activation_diagnostics.MapName();
    }
    if (summary.server_activation.globals_snapshot.empty())
    {
        summary.server_activation.globals_snapshot = BuildGlobalsSnapshotText(state);
    }
    if (summary.server_activation.server_state_snapshot.empty())
    {
        summary.server_activation.server_state_snapshot = BuildServerFlagsSnapshotText(state);
    }

    summary.frame_bootstrap_config = state.frame_bootstrap_options;
    summary.server_frame_loop = state.server_frame_loop_state;
    summary.entity_think_scheduler = state.entity_think_scheduler_state;
    summary.map_logic_dispatcher = state.map_logic_dispatcher_state;
    summary.changelevel_transition = state.changelevel_transition_state;
    if (!summary.changelevel_transition.candidate_present)
    {
        for (const RuntimeEntityRecord& record : state.entity_bootstrap.runtime_entities)
        {
            if (!EqualsIgnoreCase(record.classname, "trigger_changelevel"))
            {
                continue;
            }

            const hl::game_api::detail::EntityDefinition* definition =
                FindParsedEntityDefinitionByOrdinal(state, record.parse_index);
            NoteTriggerChangeLevelCandidate(summary.changelevel_transition, definition, &record);
            break;
        }
    }
    if (!summary.changelevel_transition.candidate_present)
    {
        for (const hl::game_api::detail::EntityDefinition& definition :
             state.entity_bootstrap.parsed_entities)
        {
            if (!EqualsIgnoreCase(definition.classname, "trigger_changelevel"))
            {
                continue;
            }

            NoteTriggerChangeLevelCandidate(summary.changelevel_transition, &definition, nullptr);
            break;
        }
    }
    summary.changelevel_transition.post_handoff_activity =
        BuildPostHandoffActivitySummary(
            summary.changelevel_transition,
            summary.map_logic_dispatcher,
            summary.server_frame_loop);
    summary.map_logic_dispatcher.classname_summary =
        BuildMapLogicClassSummary(state.entity_bootstrap.runtime_entities);
    summary.scripted_logic = state.scripted_logic_state;
    summary.scripted_movement = state.scripted_movement_state;

    summary.ready_for_server_activation =
        summary.world_bootstrap.completed
        && summary.worldspawn_spawn.succeeded
        && (summary.entity_pipeline.entities_lump_parsed || summary.entity_pipeline.partially_parsed);
}

void RegisterCvarImpl(const char* callback_name, cvar_t* cvar)
{
    if (cvar == nullptr || cvar->name == nullptr)
    {
        RecordCallback(callback_name, "<null>");
        return;
    }

    const std::string detail = cvar->string != nullptr
        ? std::string(cvar->name) + "=" + cvar->string
        : std::string(cvar->name) + "=<null>";
    RecordCallback(callback_name, detail);

    EngineShimState& state = CurrentShimState();
    state.cvar_registry.Register(cvar);
}

cvar_t* FindOrAutoCreateEngineCvar(
    EngineShimState& state,
    std::string_view name,
    std::string_view initial_value,
    const char* source)
{
    cvar_t* existing = state.cvar_registry.Find(name);
    if (existing != nullptr)
    {
        hl::common::Logger::Info(
            std::string(source) + ": using preexisting cvar '" + std::string(name) + "'.");
        return existing;
    }

    if (!hl::game_api::CvarRegistry::IsSafeAutoCreateName(name))
    {
        return nullptr;
    }

    const hl::game_api::CvarSetResult result =
        state.cvar_registry.SetValueOrCreate(name, initial_value);
    if (result.created)
    {
        hl::common::Logger::Warn(
            std::string(source) + ": auto-created engine cvar '" + std::string(name)
            + "' = " + std::string(initial_value) + "'.");
    }

    return result.cvar;
}

void StubCVarRegister(cvar_t* cvar)
{
    RegisterCvarImpl("pfnCVarRegister", cvar);
}

void StubCvarRegisterVariable(cvar_t* cvar)
{
    RegisterCvarImpl("pfnCvar_RegisterVariable", cvar);
}

cvar_t* StubCVarGetPointer(const char* variable_name)
{
    const std::string name = variable_name != nullptr ? variable_name : "";
    RecordCallback("pfnCVarGetPointer", name.empty() ? "<null>" : name);

    if (name.empty())
    {
        return nullptr;
    }

    EngineShimState& state = CurrentShimState();
    cvar_t* cvar = state.cvar_registry.Find(name);
    if (cvar == nullptr)
    {
        cvar = FindOrAutoCreateEngineCvar(state, name, "0", "pfnCVarGetPointer");
    }

    if (cvar == nullptr)
    {
        hl::common::Logger::Warn("Requested unknown cvar pointer: " + name);
    }

    return cvar;
}

float StubCVarGetFloat(const char* variable_name)
{
    const std::string name = variable_name != nullptr ? variable_name : "";
    RecordCallback("pfnCVarGetFloat", name.empty() ? "<null>" : name);

    if (name.empty())
    {
        return 0.0f;
    }

    EngineShimState& state = CurrentShimState();
    const cvar_t* cvar = state.cvar_registry.Find(name);
    if (cvar == nullptr)
    {
        cvar = FindOrAutoCreateEngineCvar(state, name, "0", "pfnCVarGetFloat");
        if (cvar == nullptr)
        {
            hl::common::Logger::Warn("Requested unknown cvar float: " + name);
            return 0.0f;
        }
    }

    return cvar->value;
}

const char* StubCVarGetString(const char* variable_name)
{
    const std::string name = variable_name != nullptr ? variable_name : "";
    RecordCallback("pfnCVarGetString", name.empty() ? "<null>" : name);

    if (name.empty())
    {
        return EmptyString();
    }

    EngineShimState& state = CurrentShimState();
    const cvar_t* cvar = state.cvar_registry.Find(name);
    if (cvar == nullptr)
    {
        cvar = FindOrAutoCreateEngineCvar(state, name, "", "pfnCVarGetString");
        if (cvar == nullptr)
        {
            hl::common::Logger::Warn("Requested unknown cvar string: " + name);
            return EmptyString();
        }
    }

    return cvar->string != nullptr ? cvar->string : EmptyString();
}

void StubCVarSetFloat(const char* variable_name, float value)
{
    const std::string name = variable_name != nullptr ? variable_name : "";
    RecordCallback(
        "pfnCVarSetFloat",
        name.empty() ? "<null>" : name + "=" + std::to_string(value));

    if (name.empty())
    {
        return;
    }

    EngineShimState& state = CurrentShimState();
    if (!state.cvar_registry.SetValue(name, std::to_string(value)))
    {
        const hl::game_api::CvarSetResult result = hl::game_api::CvarRegistry::IsSafeAutoCreateName(name)
            ? state.cvar_registry.SetValueOrCreate(name, std::to_string(value))
            : hl::game_api::CvarSetResult{};
        if (!result.updated)
        {
            hl::common::Logger::Warn("Attempted to set unknown cvar float: " + name);
        }
        else
        {
            hl::common::Logger::Info(
                "pfnCVarSetFloat applied " + name + " = " + std::to_string(value)
                + (result.created ? " (auto-created)" : " (preexisting)"));
        }
    }
    else
    {
        hl::common::Logger::Info(
            "pfnCVarSetFloat applied " + name + " = " + std::to_string(value)
            + " (preexisting)");
    }
}

void StubCVarSetString(const char* variable_name, const char* value)
{
    const std::string name = variable_name != nullptr ? variable_name : "";
    const std::string string_value = value != nullptr ? value : "";
    RecordCallback(
        "pfnCVarSetString",
        name.empty() ? "<null>" : name + "=" + string_value);

    if (name.empty())
    {
        return;
    }

    EngineShimState& state = CurrentShimState();
    if (!state.cvar_registry.SetValue(name, string_value))
    {
        const hl::game_api::CvarSetResult result = hl::game_api::CvarRegistry::IsSafeAutoCreateName(name)
            ? state.cvar_registry.SetValueOrCreate(name, string_value)
            : hl::game_api::CvarSetResult{};
        if (!result.updated)
        {
            hl::common::Logger::Warn("Attempted to set unknown cvar string: " + name);
        }
        else
        {
            hl::common::Logger::Info(
                "pfnCVarSetString applied " + name + " = " + string_value
                + (result.created ? " (auto-created)" : " (preexisting)"));
        }
    }
    else
    {
        hl::common::Logger::Info(
            "pfnCVarSetString applied " + name + " = " + string_value + " (preexisting)");
    }
}

void StubCvarDirectSet(struct cvar_s* variable, char* value)
{
    if (variable == nullptr || variable->name == nullptr)
    {
        RecordCallback("pfnCvar_DirectSet", "<null>");
        return;
    }

    const std::string string_value = value != nullptr ? value : "";
    RecordCallback("pfnCvar_DirectSet", std::string(variable->name) + "=" + string_value);

    EngineShimState& state = CurrentShimState();
    if (state.cvar_registry.Find(variable->name) == nullptr)
    {
        state.cvar_registry.Register(variable);
    }

    if (!state.cvar_registry.SetValue(variable->name, string_value))
    {
        hl::common::Logger::Warn("Attempted to direct-set unknown cvar: " + std::string(variable->name));
    }
}

void StubServerCommand(char* command)
{
    EngineShimState& state = CurrentShimState();
    const std::string text = command != nullptr ? TrimTrailingWhitespace(command) : std::string();
    RecordCallback("pfnServerCommand", text.empty() ? "<empty>" : text);
    state.command_buffer.Queue(text);
}

void StubServerExecute()
{
    RecordCallback("pfnServerExecute");
    ExecuteQueuedServerCommands(CurrentShimState(), "pfnServerExecute");
}

void StubServerPrint(const char* message)
{
    const std::string text = message != nullptr ? TrimTrailingWhitespace(message) : std::string();
    RecordCallback("pfnServerPrint", text.empty() ? "<empty>" : text);
}

void StubAlertMessage(ALERT_TYPE alert_type, char* format, ...)
{
    EngineShimState& state = CurrentShimState();
    std::string detail;
    switch (alert_type)
    {
    case at_notice:
        detail = "notice";
        break;
    case at_console:
        detail = "console";
        break;
    case at_aiconsole:
        detail = "aiconsole";
        break;
    case at_warning:
        detail = "warning";
        break;
    case at_error:
        detail = "error";
        break;
    case at_logged:
        detail = "logged";
        break;
    default:
        detail = "unknown";
        break;
    }

    if (format != nullptr)
    {
        std::array<char, 1024> buffer{};
        va_list arguments;
        va_start(arguments, format);
        const int written = vsnprintf(buffer.data(), buffer.size(), format, arguments);
        va_end(arguments);

        detail += ": ";
        detail += TrimTrailingWhitespace(
            written > 0 ? std::string(buffer.data()) : std::string(format));
    }

    if (state.server_activation_diagnostics.IsActive())
    {
        const std::string dead_end_target = ExtractPathTrackDeadEndTarget(detail);
        if (!dead_end_target.empty())
        {
            state.path_track_terminal_dead_end_targets.insert(ToLowerCopy(dead_end_target));
        }
    }

    RecordCallback("pfnAlertMessage", detail);
}

void EmitAiConsoleDiagnostic(std::string_view message)
{
    std::string owned(message);
    static char format[] = "%s";
    StubAlertMessage(at_aiconsole, format, owned.c_str());
}

void StubEngineFprintf(void* /*file*/, char* format, ...)
{
    const std::string text = format != nullptr ? TrimTrailingWhitespace(format) : std::string();
    RecordCallback("pfnEngineFprintf", text.empty() ? "<empty>" : text);
}

string_t StubMakeString(const char* value)
{
    const std::string text = value != nullptr ? value : "";
    EngineShimState& state = CurrentShimState();
    const string_t index = state.string_pool.MakeString(value);
    RecordCallback(
        "pfnMakeString",
        (text.empty() ? std::string("<empty>") : text) + " -> " + std::to_string(index));
    return index;
}

int StubAllocString(const char* value)
{
    EngineShimState& state = CurrentShimState();
    const std::string text = value != nullptr ? value : "";
    const string_t index = state.string_pool.Alloc(text);
    RecordCallback(
        "pfnAllocString",
        (text.empty() ? std::string("<empty>") : text) + " -> " + std::to_string(index));
    return static_cast<int>(index);
}

const char* StubSzFromIndex(int string_index)
{
    EngineShimState& state = CurrentShimState();
    RecordCallback(
        "pfnSzFromIndex",
        std::to_string(string_index) + " -> "
            + state.string_pool.Describe(static_cast<string_t>(string_index)));
    return state.string_pool.SzFromIndex(static_cast<string_t>(string_index));
}

int StubPrecacheModel(char* value)
{
    EngineShimState& state = CurrentShimState();
    const std::string text = value != nullptr ? value : "";
    const int previous = state.precache_registry.ModelIndex(text);
    const int index = state.precache_registry.PrecacheModel(text);
    const std::string detail =
        "requested=" + (text.empty() ? std::string("<empty>") : text)
        + ", normalized="
        + (state.precache_registry.ModelName(index) != nullptr
            ? *state.precache_registry.ModelName(index)
            : std::string("<empty>"))
        + ", index=" + std::to_string(index)
        + ", state=" + (previous != 0 ? "duplicate" : (index != 0 ? "new" : "invalid"));
    RecordCallback("pfnPrecacheModel", detail);
    if (state.worldspawn_spawn_diagnostics.IsActive())
    {
        state.worldspawn_spawn_diagnostics.RecordPrecacheOperation("pfnPrecacheModel", detail);
    }
    hl::common::Logger::Info(
        "Precache model: " + detail);
    return index;
}

int StubPrecacheSound(char* value)
{
    EngineShimState& state = CurrentShimState();
    const std::string text = value != nullptr ? value : "";
    const hl::game_api::detail::SoundPrecacheResult result =
        state.sound_precache_registry.PrecacheSound(
            text,
            state.server_state.game_directory,
            state.file_system,
            &state.string_pool);

    const std::string detail =
        "requested=" + (text.empty() ? std::string("<empty>") : text)
        + ", normalized="
        + (result.normalized_path.empty() ? std::string("<empty>") : result.normalized_path)
        + ", exists=" + BoolToYesNo(result.file_exists)
        + ", index=" + std::to_string(result.index)
        + ", state=" + (result.duplicate ? "duplicate" : (result.inserted ? "new" : "invalid"))
        + ", registry_size=" + std::to_string(result.registry_size)
        + ", string_index=" + std::to_string(result.string_index)
        + ", resolved="
        + (result.resolved_path.empty()
            ? std::string("<unresolved>")
            : hl::common::ToUtf8(result.resolved_path));

    RecordCallback("pfnPrecacheSound", detail);
    if (state.worldspawn_spawn_diagnostics.IsActive())
    {
        state.worldspawn_spawn_diagnostics.RecordPrecacheOperation("pfnPrecacheSound", detail);
        state.worldspawn_spawn_diagnostics.RecordSoundPrecache(result.duplicate, result.file_exists);
    }

    hl::common::Logger::Info(
        "Sound precache: " + detail
        + ", duplicate_requests=" + std::to_string(result.duplicate_requests)
        + ", missing_files=" + std::to_string(result.missing_files));

    if (!result.file_exists && !result.normalized_path.empty())
    {
        hl::common::Logger::Warn(
            "Sound precache missing asset under valve/sound: "
            + result.normalized_path + " (resolved "
            + (result.resolved_path.empty()
                ? std::string("<unresolved>")
                : hl::common::ToUtf8(result.resolved_path))
            + ")");
    }

    return result.index;
}

int StubPrecacheGeneric(char* value)
{
    EngineShimState& state = CurrentShimState();
    const std::string text = NormalizeEngineResourcePath(value != nullptr ? value : "");
    bool duplicate = false;
    const int index = state.generic_precache_registry.Upsert(text, &duplicate);
    const std::filesystem::path resolved = text.empty()
        ? std::filesystem::path()
        : (state.server_state.game_directory / std::filesystem::path(text)).lexically_normal();
    const bool exists = !text.empty() && state.file_system.FileExists(resolved);
    const std::string detail =
        "requested=" + std::string(value != nullptr ? value : "")
        + ", normalized=" + (text.empty() ? std::string("<empty>") : text)
        + ", exists=" + BoolToYesNo(exists)
        + ", index=" + std::to_string(index)
        + ", state=" + (duplicate ? "duplicate" : (index != 0 ? "new" : "invalid"));
    RecordCallback("pfnPrecacheGeneric", detail);
    if (state.worldspawn_spawn_diagnostics.IsActive())
    {
        state.worldspawn_spawn_diagnostics.RecordPrecacheOperation("pfnPrecacheGeneric", detail);
    }

    return index;
}

unsigned short StubPrecacheEvent(int type, const char* path)
{
    EngineShimState& state = CurrentShimState();
    const std::string requested = path != nullptr ? path : "";
    const std::string normalized = NormalizeEngineResourcePath(requested);
    const std::string key = std::to_string(type) + ":" + normalized;
    bool duplicate = false;
    const int index = state.event_precache_registry.Upsert(key, &duplicate);
    const std::filesystem::path resolved = normalized.empty()
        ? std::filesystem::path()
        : (state.server_state.game_directory / std::filesystem::path(normalized)).lexically_normal();
    const bool exists = !normalized.empty() && state.file_system.FileExists(resolved);
    const std::string detail =
        "type=" + std::to_string(type)
        + ", requested=" + (requested.empty() ? std::string("<empty>") : requested)
        + ", normalized=" + (normalized.empty() ? std::string("<empty>") : normalized)
        + ", exists=" + BoolToYesNo(exists)
        + ", index=" + std::to_string(index)
        + ", state=" + (duplicate ? "duplicate" : (index != 0 ? "new" : "invalid"));
    RecordCallback("pfnPrecacheEvent", detail);
    if (state.worldspawn_spawn_diagnostics.IsActive())
    {
        state.worldspawn_spawn_diagnostics.RecordPrecacheOperation("pfnPrecacheEvent", detail);
    }

    if (!exists && !normalized.empty())
    {
        hl::common::Logger::Warn(
            "Precache event missing asset: " + normalized + " (resolved "
            + hl::common::ToUtf8(resolved) + ")");
    }

    return static_cast<unsigned short>(std::max(index, 0));
}

int StubModelIndex(const char* value)
{
    EngineShimState& state = CurrentShimState();
    const std::string text = value != nullptr ? value : "";
    RecordCallback("pfnModelIndex", text.empty() ? "<empty>" : text);
    const int index = state.precache_registry.ModelIndex(text);
    if (index == 0 && !text.empty())
    {
        hl::common::Logger::Warn("pfnModelIndex requested an unregistered model: " + text);
        if (state.worldspawn_spawn_diagnostics.IsActive())
        {
            const int fallback_index = state.precache_registry.PrecacheModel(text);
            hl::common::Logger::Warn(
                "pfnModelIndex auto-registered model during worldspawn spawn: "
                + text + " -> " + std::to_string(fallback_index));
            return fallback_index;
        }
    }

    return index;
}

int StubModelFrames(int model_index)
{
    RecordCallback("pfnModelFrames", std::to_string(model_index));
    return model_index == 0 ? 0 : 1;
}

void StubLightStyle(int style, char* value)
{
    EngineShimState& state = CurrentShimState();
    const std::string pattern = value != nullptr ? value : "";
    RecordCallback(
        "pfnLightStyle",
        "style=" + std::to_string(style) + ", pattern=" + (pattern.empty() ? "<empty>" : pattern));
    state.light_styles[style] = pattern;
}

int StubDecalIndex(const char* name)
{
    EngineShimState& state = CurrentShimState();
    const std::string normalized = NormalizeEngineResourcePath(name != nullptr ? name : "");
    bool duplicate = false;
    const int index = state.decal_registry.Upsert(normalized, &duplicate);
    RecordCallback(
        "pfnDecalIndex",
        "requested=" + std::string(name != nullptr ? name : "")
            + ", normalized=" + (normalized.empty() ? std::string("<empty>") : normalized)
            + ", index=" + std::to_string(index)
            + ", state=" + (duplicate ? "duplicate" : (index != 0 ? "new" : "invalid")));
    return index;
}

void StubEmitSound(
    edict_t* entity,
    int channel,
    const char* sample,
    float volume,
    float attenuation,
    int flags,
    int pitch)
{
    EngineShimState& state = CurrentShimState();
    const std::string requested = sample != nullptr ? sample : "";
    int sound_index = state.sound_precache_registry.SoundIndex(requested);
    if (sound_index == 0 && !requested.empty())
    {
        hl::common::Logger::Warn(
            "pfnEmitSound received an unprecached sample; auto-registering: " + requested);
        sound_index = StubPrecacheSound(const_cast<char*>(requested.c_str()));
    }

    RecordCallback(
        "pfnEmitSound",
        DescribeEdict(state, entity)
            + ", channel=" + std::to_string(channel)
            + ", sample=" + (requested.empty() ? std::string("<empty>") : requested)
            + ", index=" + std::to_string(sound_index)
            + ", volume=" + std::to_string(volume)
            + ", attenuation=" + std::to_string(attenuation)
            + ", flags=" + std::to_string(flags)
            + ", pitch=" + std::to_string(pitch),
        entity);
}

void StubEmitAmbientSound(
    edict_t* entity,
    float* position,
    const char* sample,
    float volume,
    float attenuation,
    int flags,
    int pitch)
{
    EngineShimState& state = CurrentShimState();
    const std::string requested = sample != nullptr ? sample : "";
    int sound_index = state.sound_precache_registry.SoundIndex(requested);
    if (sound_index == 0 && !requested.empty())
    {
        hl::common::Logger::Warn(
            "pfnEmitAmbientSound received an unprecached sample; auto-registering: " + requested);
        sound_index = StubPrecacheSound(const_cast<char*>(requested.c_str()));
    }

    const std::string position_text = position != nullptr
        ? std::to_string(position[0]) + " " + std::to_string(position[1]) + " " + std::to_string(position[2])
        : std::string("<null>");
    RecordCallback(
        "pfnEmitAmbientSound",
        DescribeEdict(state, entity)
            + ", pos=" + position_text
            + ", sample=" + (requested.empty() ? std::string("<empty>") : requested)
            + ", index=" + std::to_string(sound_index)
            + ", volume=" + std::to_string(volume)
            + ", attenuation=" + std::to_string(attenuation)
            + ", flags=" + std::to_string(flags)
            + ", pitch=" + std::to_string(pitch),
        entity);
}

void* StubPvAllocEntPrivateData(edict_t* entity, int32 bytes)
{
    EngineShimState& state = CurrentShimState();
    RecordCallback(
        "pfnPvAllocEntPrivateData",
        DescribeEdict(state, entity) + ", bytes=" + std::to_string(bytes),
        entity);

    if (entity == nullptr || bytes <= 0)
    {
        return nullptr;
    }

    return state.edict_store.AllocatePrivateData(entity, static_cast<std::size_t>(bytes));
}

void* StubPvEntPrivateData(edict_t* entity)
{
    EngineShimState& state = CurrentShimState();
    RecordCallback("pfnPvEntPrivateData", DescribeEdict(state, entity), entity);
    return state.edict_store.PrivateDataOf(entity);
}

void StubFreeEntPrivateData(edict_t* entity)
{
    EngineShimState& state = CurrentShimState();
    RecordCallback("pfnFreeEntPrivateData", DescribeEdict(state, entity), entity);
    state.edict_store.FreePrivateData(entity);
}

void StubSetOrigin(edict_t* entity, const float* origin)
{
    EngineShimState& state = CurrentShimState();
    const std::string origin_text = origin != nullptr
        ? std::to_string(origin[0]) + " " + std::to_string(origin[1]) + " " + std::to_string(origin[2])
        : std::string("<null>");
    RecordCallback("pfnSetOrigin", DescribeEdict(state, entity) + " <- " + origin_text, entity);

    if (entity == nullptr || origin == nullptr)
    {
        return;
    }

    if (state.edict_store.IndexOf(entity) < 0)
    {
        hl::common::Logger::Warn("pfnSetOrigin received an edict outside the current store.");
        return;
    }

    state.edict_store.SetOrigin(entity, Vector(origin[0], origin[1], origin[2]), origin_text);
    ComputeFallbackAbsBox(entity->v);
}

void StubSetSize(edict_t* entity, const float* mins, const float* maxs)
{
    EngineShimState& state = CurrentShimState();
    const std::string mins_text = mins != nullptr
        ? std::to_string(mins[0]) + " " + std::to_string(mins[1]) + " " + std::to_string(mins[2])
        : std::string("<null>");
    const std::string maxs_text = maxs != nullptr
        ? std::to_string(maxs[0]) + " " + std::to_string(maxs[1]) + " " + std::to_string(maxs[2])
        : std::string("<null>");
    RecordCallback(
        "pfnSetSize",
        DescribeEdict(state, entity) + " mins=" + mins_text + " maxs=" + maxs_text,
        entity);

    if (entity == nullptr || mins == nullptr || maxs == nullptr)
    {
        return;
    }

    if (state.edict_store.IndexOf(entity) < 0)
    {
        hl::common::Logger::Warn("pfnSetSize received an edict outside the current store.");
        return;
    }

    state.edict_store.SetSize(
        entity,
        Vector(mins[0], mins[1], mins[2]),
        Vector(maxs[0], maxs[1], maxs[2]),
        mins_text,
        maxs_text);
    ComputeFallbackAbsBox(entity->v);
}

void StubSetModel(edict_t* entity, const char* model_name)
{
    EngineShimState& state = CurrentShimState();
    const std::string model = model_name != nullptr ? model_name : "";
    std::string detail = DescribeEdict(state, entity);
    if (!model.empty())
    {
        detail += " <- " + model;
    }
    RecordCallback("pfnSetModel", detail, entity);

    const int entity_index = state.edict_store.IndexOf(entity);
    if (entity_index < 0 || entity == nullptr)
    {
        hl::common::Logger::Warn("pfnSetModel received an edict outside the current store.");
        return;
    }

    int model_index = state.precache_registry.ModelIndex(model);
    if (model_index == 0 && !model.empty())
    {
        hl::common::Logger::Warn(
            "pfnSetModel received a model that was not precached yet; auto-registering: " + model);
        model_index = state.precache_registry.PrecacheModel(model);
    }

    state.edict_store.SetModel(
        entity,
        state.string_pool.Alloc(model),
        model,
        model_index);

    if (entity_index == 0)
    {
        state.world_context.world_model_index = model_index;
    }
}

edict_t* StubCreateEntity()
{
    EngineShimState& state = CurrentShimState();
    edict_t* entity = state.edict_store.CreateEntity();
    RecordCallback(
        "pfnCreateEntity",
        entity != nullptr ? DescribeEdict(state, entity) : "<no free edict>",
        entity);

    if (entity == nullptr)
    {
        hl::common::Logger::Warn("pfnCreateEntity could not allocate a free edict slot.");
    }

    return entity;
}

edict_t* StubCreateNamedEntity(int class_name)
{
    EngineShimState& state = CurrentShimState();
    const std::string class_description = state.string_pool.Describe(static_cast<string_t>(class_name));
    edict_t* entity = state.edict_store.CreateEntity();
    RecordCallback(
        "pfnCreateNamedEntity",
        (entity != nullptr ? DescribeEdict(state, entity) : "<no free edict>")
        + " [" + (class_description.empty() ? "<empty>" : class_description) + "]",
        entity,
        class_description);

    if (entity == nullptr)
    {
        hl::common::Logger::Warn("pfnCreateNamedEntity could not allocate a free edict slot.");
        return nullptr;
    }

    state.edict_store.SetClassname(entity, static_cast<string_t>(class_name), class_description);
    BindEntityClassExport(state, entity, class_description, "hl.dll pfnCreateNamedEntity");
    return entity;
}

void StubRemoveEntity(edict_t* entity)
{
    EngineShimState& state = CurrentShimState();
    const int edict_index = state.edict_store.IndexOf(entity);
    RecordCallback("pfnRemoveEntity", DescribeEdict(state, entity), entity);
    state.edict_store.RemoveEntity(entity);
    if (edict_index >= 0)
    {
        if (RuntimeEntityRecord* record = FindRuntimeRecordByEdictIndex(state, edict_index);
            record != nullptr)
        {
            SyncRuntimeRecordFromEdict(state, state.edict_store.EntityOfIndex(edict_index), *record);
            record->removed = true;
            record->lifecycle_state = RuntimeEntityLifecycleState::kRemovedByGameLogic;
            AppendRecordNote(*record, "removed by game logic");
        }
    }
}

entvars_t* StubGetVarsOfEnt(edict_t* entity)
{
    EngineShimState& state = CurrentShimState();
    RecordCallback("pfnGetVarsOfEnt", DescribeEdict(state, entity), entity);
    return state.edict_store.VarsOf(entity);
}

edict_t* StubPEntityOfEntOffset(int entity_offset)
{
    EngineShimState& state = CurrentShimState();
    edict_t* entity = state.edict_store.EntityOfOffset(entity_offset);
    RecordCallback(
        "pfnPEntityOfEntOffset",
        std::to_string(entity_offset) + " -> " + DescribeEdict(state, entity),
        entity);
    return entity;
}

int StubEntOffsetOfPEntity(const edict_t* entity)
{
    EngineShimState& state = CurrentShimState();
    const int offset = entity != nullptr ? state.edict_store.OffsetOf(entity) : 0;
    RecordCallback(
        "pfnEntOffsetOfPEntity",
        DescribeEdict(state, entity) + " -> " + std::to_string(offset),
        entity);
    return offset;
}

int StubIndexOfEdict(const edict_t* entity)
{
    EngineShimState& state = CurrentShimState();
    const int index = entity != nullptr ? state.edict_store.IndexOf(entity) : 0;
    RecordCallback(
        "pfnIndexOfEdict",
        DescribeEdict(state, entity) + " -> " + std::to_string(index < 0 ? 0 : index),
        entity);
    return index < 0 ? 0 : index;
}

edict_t* StubPEntityOfEntIndex(int entity_index)
{
    EngineShimState& state = CurrentShimState();
    edict_t* entity = nullptr;
    if (entity_index == 0)
    {
        entity = state.edict_store.World();
    }
    else if (entity_index > 0 && entity_index < state.edict_store.MaxEntities())
    {
        edict_t* candidate = state.edict_store.EntityOfIndex(entity_index);
        const hl::game_api::detail::EntityStateSnapshot snapshot =
            state.edict_store.SnapshotOf(candidate, state.string_pool);
        if (snapshot.index == entity_index
            && (snapshot.in_use || entity_index <= state.server_state.maxclients))
        {
            entity = candidate;
        }
    }

    RecordCallback(
        "pfnPEntityOfEntIndex",
        std::to_string(entity_index) + " -> " + DescribeEdict(state, entity),
        entity);
    return entity;
}

edict_t* StubFindEntityByVars(struct entvars_s* variables)
{
    EngineShimState& state = CurrentShimState();
    edict_t* entity = state.edict_store.FindByVars(variables);
    if (state.edict_store.IndexOf(entity) < 0)
    {
        entity = nullptr;
    }
    RecordCallback(
        "pfnFindEntityByVars",
        entity != nullptr ? DescribeEdict(state, entity) : "<null>",
        entity);
    return entity;
}

int StubRegUserMsg(const char* name, int size)
{
    EngineShimState& state = CurrentShimState();
    const std::string message_name = name != nullptr ? name : "";
    if (message_name.empty())
    {
        RecordCallback("pfnRegUserMsg", "name=<empty>, size=" + std::to_string(size));
        return 0;
    }

    bool duplicate = false;
    const int index = state.user_message_registry.Upsert(message_name, &duplicate);
    const int message_id = index == 0 ? 0 : (64 + index);
    RecordCallback(
        "pfnRegUserMsg",
        "name=" + message_name
            + ", size=" + std::to_string(size)
            + ", id=" + std::to_string(message_id)
            + ", state=" + (duplicate ? "duplicate" : "new"));
    return message_id;
}

std::string ReadEntityStringField(
    const EngineShimState& state,
    const edict_t* entity,
    std::string_view field_name)
{
    if (entity == nullptr)
    {
        return {};
    }

    if (EqualsIgnoreCase(field_name, "classname"))
    {
        return state.string_pool.Describe(entity->v.classname);
    }
    if (EqualsIgnoreCase(field_name, "targetname"))
    {
        return state.string_pool.Describe(entity->v.targetname);
    }
    if (EqualsIgnoreCase(field_name, "target"))
    {
        return state.string_pool.Describe(entity->v.target);
    }
    if (EqualsIgnoreCase(field_name, "globalname"))
    {
        return state.string_pool.Describe(entity->v.globalname);
    }
    if (EqualsIgnoreCase(field_name, "model"))
    {
        return state.string_pool.Describe(entity->v.model);
    }
    if (EqualsIgnoreCase(field_name, "message"))
    {
        return state.string_pool.Describe(entity->v.message);
    }
    if (EqualsIgnoreCase(field_name, "netname"))
    {
        return state.string_pool.Describe(entity->v.netname);
    }

    return {};
}

edict_t* StubFindEntityByString(
    edict_t* start_search_after,
    const char* field_name,
    const char* value)
{
    EngineShimState& state = CurrentShimState();
    const std::string field = field_name != nullptr ? field_name : "";
    const std::string expected = value != nullptr ? value : "";
    int start_index = 0;
    if (start_search_after != nullptr)
    {
        start_index = state.edict_store.IndexOf(start_search_after);
        if (start_index < 0)
        {
            start_index = 0;
        }
    }

    edict_t* found = nullptr;
    for (int index = start_index + 1; index < state.edict_store.NumberOfEntities(); ++index)
    {
        edict_t* candidate = state.edict_store.EntityOfIndex(index);
        const hl::game_api::detail::EntityStateSnapshot snapshot =
            state.edict_store.SnapshotOf(candidate, state.string_pool);
        if (snapshot.index < 0 || !snapshot.in_use || snapshot.removed)
        {
            continue;
        }

        if (ReadEntityStringField(state, candidate, field) == expected)
        {
            found = candidate;
            break;
        }
    }

    RecordCallback(
        "pfnFindEntityByString",
        "field=" + (field.empty() ? std::string("<empty>") : field)
            + ", value=" + (expected.empty() ? std::string("<empty>") : expected)
            + ", start=" + DescribeEdict(state, start_search_after)
            + ", result=" + DescribeEdict(state, found),
        found);
    return found;
}

edict_t* StubFindEntityInSphere(
    edict_t* start_search_after,
    const float* origin,
    float radius)
{
    EngineShimState& state = CurrentShimState();
    const Vector center = origin != nullptr
        ? Vector(origin[0], origin[1], origin[2])
        : Vector(0.0f, 0.0f, 0.0f);
    const float radius_squared = radius * radius;
    int start_index = 0;
    if (start_search_after != nullptr)
    {
        start_index = state.edict_store.IndexOf(start_search_after);
        if (start_index < 0)
        {
            start_index = 0;
        }
    }

    edict_t* found = nullptr;
    for (int index = start_index + 1; index < state.edict_store.NumberOfEntities(); ++index)
    {
        edict_t* candidate = state.edict_store.EntityOfIndex(index);
        const hl::game_api::detail::EntityStateSnapshot snapshot =
            state.edict_store.SnapshotOf(candidate, state.string_pool);
        if (snapshot.index < 0 || !snapshot.in_use || snapshot.removed)
        {
            continue;
        }

        float distance_squared = 0.0f;
        for (int axis = 0; axis < 3 && distance_squared <= radius_squared; ++axis)
        {
            float delta = 0.0f;
            if (center[axis] < snapshot.absmin[axis])
            {
                delta = center[axis] - snapshot.absmin[axis];
            }
            else if (center[axis] > snapshot.absmax[axis])
            {
                delta = center[axis] - snapshot.absmax[axis];
            }

            distance_squared += delta * delta;
        }

        if (distance_squared < radius_squared)
        {
            found = candidate;
            break;
        }
    }

    RecordCallback(
        "pfnFindEntityInSphere",
        "origin=" + FormatVector(center)
            + ", radius=" + std::to_string(radius)
            + ", start=" + DescribeEdict(state, start_search_after)
            + ", result=" + DescribeEdict(state, found),
        found);
    return found;
}

edict_t* StubEntitiesInPVS(edict_t* player)
{
    EngineShimState& state = CurrentShimState();
    edict_t* world = state.edict_store.World();
    RecordCallback(
        "pfnEntitiesInPVS",
        "player=" + DescribeEdict(state, player)
            + ", result=" + DescribeEdict(state, world),
        player);
    return world;
}

Vector VectorFromFloatPointer(const float* value)
{
    return value != nullptr
        ? Vector(value[0], value[1], value[2])
        : Vector(0.0f, 0.0f, 0.0f);
}

void ResetTraceResult(TraceResult* trace, const Vector& end_position, edict_t* hit_entity)
{
    if (trace == nullptr)
    {
        return;
    }

    *trace = {};
    trace->flFraction = 1.0f;
    trace->vecEndPos = end_position;
    trace->pHit = hit_entity;
}

void StubTraceLine(
    const float* start,
    const float* end,
    int no_monsters,
    edict_t* entity_to_skip,
    TraceResult* trace)
{
    EngineShimState& state = CurrentShimState();
    const Vector start_vec = VectorFromFloatPointer(start);
    const Vector end_vec = VectorFromFloatPointer(end);
    ResetTraceResult(trace, end_vec, state.edict_store.World());
    RecordCallback(
        "pfnTraceLine",
        "start=" + FormatVector(start_vec)
            + ", end=" + FormatVector(end_vec)
            + ", noMonsters=" + std::to_string(no_monsters)
            + ", skip=" + DescribeEdict(state, entity_to_skip)
            + ", fraction=1.0",
        entity_to_skip);
}

void StubTraceHull(
    const float* start,
    const float* end,
    int no_monsters,
    int hull_number,
    edict_t* entity_to_skip,
    TraceResult* trace)
{
    EngineShimState& state = CurrentShimState();
    const Vector start_vec = VectorFromFloatPointer(start);
    const Vector end_vec = VectorFromFloatPointer(end);
    ResetTraceResult(trace, end_vec, state.edict_store.World());
    RecordCallback(
        "pfnTraceHull",
        "start=" + FormatVector(start_vec)
            + ", end=" + FormatVector(end_vec)
            + ", noMonsters=" + std::to_string(no_monsters)
            + ", hull=" + std::to_string(hull_number)
            + ", skip=" + DescribeEdict(state, entity_to_skip)
            + ", fraction=1.0",
        entity_to_skip);
}

int StubPointContents(const float* vector)
{
    const Vector point = VectorFromFloatPointer(vector);
    RecordCallback("pfnPointContents", FormatVector(point) + " -> CONTENTS_EMPTY");
    return CONTENTS_EMPTY;
}

void StubMessageBegin(int msg_dest, int msg_type, const float* origin, edict_t* entity)
{
    EngineShimState& state = CurrentShimState();
    const Vector origin_vec = VectorFromFloatPointer(origin);
    const std::string origin_text =
        origin != nullptr ? FormatVector(origin_vec) : std::string("<unset>");
    const hl::game_api::detail::EntityStateSnapshot snapshot =
        state.edict_store.SnapshotOf(entity, state.string_pool);
    state.frame_message_buffer.Begin(
        msg_dest,
        msg_type,
        origin_text,
        snapshot.index,
        snapshot.classname);
    RecordCallback(
        "pfnMessageBegin",
        "dest=" + std::to_string(msg_dest)
            + ", type=" + std::to_string(msg_type)
            + ", origin=" + origin_text
            + ", entity=" + DescribeEdict(state, entity),
        entity);
}

void StubMessageEnd()
{
    EngineShimState& state = CurrentShimState();
    RecordCallback("pfnMessageEnd", state.frame_message_buffer.End());
}

void StubWriteByte(int value)
{
    CurrentShimState().frame_message_buffer.WriteByte(value);
    RecordCallback("pfnWriteByte", std::to_string(value));
}

void StubWriteChar(int value)
{
    CurrentShimState().frame_message_buffer.WriteChar(value);
    RecordCallback("pfnWriteChar", std::to_string(value));
}

void StubWriteShort(int value)
{
    CurrentShimState().frame_message_buffer.WriteShort(value);
    RecordCallback("pfnWriteShort", std::to_string(value));
}

void StubWriteLong(int value)
{
    CurrentShimState().frame_message_buffer.WriteLong(value);
    RecordCallback("pfnWriteLong", std::to_string(value));
}

void StubWriteAngle(float value)
{
    CurrentShimState().frame_message_buffer.WriteAngle(value);
    RecordCallback("pfnWriteAngle", std::to_string(value));
}

void StubWriteCoord(float value)
{
    CurrentShimState().frame_message_buffer.WriteCoord(value);
    RecordCallback("pfnWriteCoord", std::to_string(value));
}

void StubWriteString(const char* value)
{
    const std::string text = value != nullptr ? value : "";
    CurrentShimState().frame_message_buffer.WriteString(text);
    RecordCallback("pfnWriteString", text.empty() ? "<empty>" : text);
}

void StubWriteEntity(int value)
{
    CurrentShimState().frame_message_buffer.WriteEntity(value);
    RecordCallback("pfnWriteEntity", std::to_string(value));
}

void ComputeAngleVectors(
    const Vector& angles,
    Vector* forward,
    Vector* right,
    Vector* up)
{
    constexpr float kDegreesToRadians = 3.14159265358979323846f / 180.0f;
    const float pitch = angles.x * kDegreesToRadians;
    const float yaw = angles.y * kDegreesToRadians;
    const float roll = angles.z * kDegreesToRadians;

    const float sp = std::sin(pitch);
    const float cp = std::cos(pitch);
    const float sy = std::sin(yaw);
    const float cy = std::cos(yaw);
    const float sr = std::sin(roll);
    const float cr = std::cos(roll);

    if (forward != nullptr)
    {
        *forward = Vector(cp * cy, cp * sy, -sp);
    }

    if (right != nullptr)
    {
        *right = Vector(
            (-1.0f * sr * sp * cy) + (-1.0f * cr * -sy),
            (-1.0f * sr * sp * sy) + (-1.0f * cr * cy),
            -1.0f * sr * cp);
    }

    if (up != nullptr)
    {
        *up = Vector(
            (cr * sp * cy) + (-sr * -sy),
            (cr * sp * sy) + (-sr * cy),
            cr * cp);
    }
}

void StubMakeVectors(const float* vector)
{
    EngineShimState& state = CurrentShimState();
    const Vector angles = vector != nullptr
        ? Vector(vector[0], vector[1], vector[2])
        : Vector(0.0f, 0.0f, 0.0f);
    ComputeAngleVectors(angles, &state.globalvars.v_forward, &state.globalvars.v_right, &state.globalvars.v_up);
    RecordCallback("pfnMakeVectors", FormatVector(angles));
}

void StubAngleVectors(const float* vector, float* forward, float* right, float* up)
{
    const Vector angles = vector != nullptr
        ? Vector(vector[0], vector[1], vector[2])
        : Vector(0.0f, 0.0f, 0.0f);
    Vector out_forward;
    Vector out_right;
    Vector out_up;
    ComputeAngleVectors(angles, &out_forward, &out_right, &out_up);

    if (forward != nullptr)
    {
        forward[0] = out_forward.x;
        forward[1] = out_forward.y;
        forward[2] = out_forward.z;
    }
    if (right != nullptr)
    {
        right[0] = out_right.x;
        right[1] = out_right.y;
        right[2] = out_right.z;
    }
    if (up != nullptr)
    {
        up[0] = out_up.x;
        up[1] = out_up.y;
        up[2] = out_up.z;
    }

    RecordCallback("pfnAngleVectors", FormatVector(angles));
}

float NormalizeAngleDegrees(float value)
{
    while (value < 0.0f)
    {
        value += 360.0f;
    }
    while (value >= 360.0f)
    {
        value -= 360.0f;
    }
    return value;
}

void StubChangeYaw(edict_t* entity)
{
    EngineShimState& state = CurrentShimState();
    RecordCallback("pfnChangeYaw", DescribeEdict(state, entity), entity);
    if (entity == nullptr || state.edict_store.IndexOf(entity) < 0)
    {
        return;
    }

    const float current = NormalizeAngleDegrees(entity->v.angles.y);
    const float ideal = NormalizeAngleDegrees(entity->v.ideal_yaw);
    float move = ideal - current;
    if (move >= 180.0f)
    {
        move -= 360.0f;
    }
    else if (move <= -180.0f)
    {
        move += 360.0f;
    }

    const float delta = std::min(std::max(state.globalvars.frametime, 0.0f), 0.25f);
    const float speed = std::max(entity->v.yaw_speed, 1.0f) * delta * 2.0f;
    if (move > speed)
    {
        move = speed;
    }
    else if (move < -speed)
    {
        move = -speed;
    }

    entity->v.angles.y = NormalizeAngleDegrees(current + move);
    state.edict_store.SetAngles(entity, entity->v.angles, FormatVector(entity->v.angles));
    ComputeFallbackAbsBox(entity->v);
}

void StubChangePitch(edict_t* entity)
{
    EngineShimState& state = CurrentShimState();
    RecordCallback("pfnChangePitch", DescribeEdict(state, entity), entity);
    if (entity == nullptr || state.edict_store.IndexOf(entity) < 0)
    {
        return;
    }

    const float current = NormalizeAngleDegrees(entity->v.angles.x);
    const float ideal = NormalizeAngleDegrees(entity->v.idealpitch);
    float move = ideal - current;
    if (move >= 180.0f)
    {
        move -= 360.0f;
    }
    else if (move <= -180.0f)
    {
        move += 360.0f;
    }

    const float delta = std::min(std::max(state.globalvars.frametime, 0.0f), 0.25f);
    const float speed = std::max(entity->v.pitch_speed, 1.0f) * delta * 2.0f;
    if (move > speed)
    {
        move = speed;
    }
    else if (move < -speed)
    {
        move = -speed;
    }

    entity->v.angles.x = NormalizeAngleDegrees(current + move);
    state.edict_store.SetAngles(entity, entity->v.angles, FormatVector(entity->v.angles));
    ComputeFallbackAbsBox(entity->v);
}

int StubDropToFloor(edict_t* entity)
{
    EngineShimState& state = CurrentShimState();
    RecordCallback("pfnDropToFloor", DescribeEdict(state, entity), entity);
    if (entity == nullptr || state.edict_store.IndexOf(entity) < 0)
    {
        return -1;
    }

    ComputeFallbackAbsBox(entity->v);
    return 1;
}

int StubWalkMove(edict_t* entity, float yaw, float distance, int mode)
{
    EngineShimState& state = CurrentShimState();
    RecordCallback(
        "pfnWalkMove",
        DescribeEdict(state, entity)
            + ", yaw=" + std::to_string(yaw)
            + ", dist=" + std::to_string(distance)
            + ", mode=" + std::to_string(mode),
        entity);
    if (entity == nullptr || state.edict_store.IndexOf(entity) < 0)
    {
        return 0;
    }

    constexpr float kDegreesToRadians = 3.14159265358979323846f / 180.0f;
    const float radians = yaw * kDegreesToRadians;
    entity->v.origin.x += std::cos(radians) * distance;
    entity->v.origin.y += std::sin(radians) * distance;
    state.edict_store.SetOrigin(entity, entity->v.origin, FormatVector(entity->v.origin));
    ComputeFallbackAbsBox(entity->v);
    return 1;
}

void StubGetGameDir(char* output)
{
    EngineShimState& state = CurrentShimState();
    const std::string path = state.server_state.game_directory_utf8;
    RecordCallback("pfnGetGameDir", path.empty() ? "<empty>" : path);

    if (output == nullptr)
    {
        return;
    }

    strncpy_s(output, MAX_PATH, path.c_str(), _TRUNCATE);
}

float StubTime()
{
    RecordCallback("pfnTime");
    return CurrentShimState().globalvars.time;
}

int32 StubRandomLong(int32 low, int32 high)
{
    EngineShimState& state = CurrentShimState();
    const void* return_address = _ReturnAddress();
    const std::string context = BuildRandomDiagnosticContext(state);
    const int32 result = state.random_diagnostics.NextLong(low, high);
    ++state.random_diagnostics.random_long_calls;

    const bool new_callsite = state.random_diagnostics.RecordCallsite(
        "pfnRandomLong",
        return_address,
        context);
    if (new_callsite)
    {
        hl::common::Logger::Info(
            "Random diagnostics registered callsite: pfnRandomLong return="
            + FormatPointer(return_address)
            + " context=" + (context.empty() ? std::string("<none>") : context));
    }

    RecordCallback(
        "pfnRandomLong",
        "low=" + std::to_string(low)
            + ", high=" + std::to_string(high)
            + ", result=" + std::to_string(result)
            + ", return=" + FormatPointer(return_address)
            + ", context=" + (context.empty() ? std::string("<none>") : context));
    return result;
}

float StubRandomFloat(float low, float high)
{
    EngineShimState& state = CurrentShimState();
    const void* return_address = _ReturnAddress();
    const std::string context = BuildRandomDiagnosticContext(state);
    const float result = state.random_diagnostics.NextFloat(low, high);
    ++state.random_diagnostics.random_float_calls;

    const bool new_callsite = state.random_diagnostics.RecordCallsite(
        "pfnRandomFloat",
        return_address,
        context);
    if (new_callsite)
    {
        hl::common::Logger::Info(
            "Random diagnostics registered callsite: pfnRandomFloat return="
            + FormatPointer(return_address)
            + " context=" + (context.empty() ? std::string("<none>") : context));
    }

    RecordCallback(
        "pfnRandomFloat",
        "low=" + std::to_string(low)
            + ", high=" + std::to_string(high)
            + ", result=" + std::to_string(result)
            + ", return=" + FormatPointer(return_address)
            + ", context=" + (context.empty() ? std::string("<none>") : context));
    return result;
}

byte* StubLoadFileForMe(char* filename, int* length)
{
    EngineShimState& state = CurrentShimState();
    const std::string text = filename != nullptr ? filename : "";
    if (length != nullptr)
    {
        *length = 0;
    }

    if (text.empty())
    {
        RecordCallback("pfnLoadFileForMe", "<empty>");
        return nullptr;
    }

    const std::string normalized = NormalizeEngineResourcePath(text);
    std::filesystem::path requested = hl::common::ToWide(text);
    std::filesystem::path resolved_path = requested.is_absolute()
        ? requested
        : state.server_state.game_directory / requested;
    resolved_path = state.file_system.AbsolutePath(resolved_path);
    RecordCallback(
        "pfnLoadFileForMe",
        "requested=" + text
            + ", normalized=" + normalized
            + ", resolved=" + hl::common::ToUtf8(resolved_path));

    const std::optional<std::vector<unsigned char>> bytes =
        state.file_system.ReadBinaryFile(resolved_path);
    if (!bytes.has_value())
    {
        hl::common::Logger::Warn(
            "pfnLoadFileForMe could not open: " + hl::common::ToUtf8(resolved_path));
        return nullptr;
    }

    if (bytes->size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
    {
        hl::common::Logger::Warn(
            "pfnLoadFileForMe rejected oversized file: " + hl::common::ToUtf8(resolved_path));
        return nullptr;
    }

    EngineShimState::LoadedFileBuffer loaded_file;
    loaded_file.bytes = std::make_unique<byte[]>(bytes->size());
    loaded_file.length = static_cast<int>(bytes->size());
    loaded_file.path = resolved_path;

    if (!bytes->empty())
    {
        std::memcpy(loaded_file.bytes.get(), bytes->data(), bytes->size());
    }

    byte* raw_buffer = loaded_file.bytes.get();
    state.loaded_files.insert_or_assign(raw_buffer, std::move(loaded_file));

    if (length != nullptr)
    {
        *length = static_cast<int>(bytes->size());
    }

    hl::common::Logger::Info(
        "pfnLoadFileForMe loaded " + std::to_string(bytes->size())
        + " bytes from " + hl::common::ToUtf8(resolved_path));
    return raw_buffer;
}

void StubFreeFile(void* buffer)
{
    EngineShimState& state = CurrentShimState();
    const auto it = state.loaded_files.find(buffer);
    if (it == state.loaded_files.end())
    {
        RecordCallback("pfnFreeFile", "<unknown>");
        return;
    }

    RecordCallback("pfnFreeFile", hl::common::ToUtf8(it->second.path));
    state.loaded_files.erase(it);
}

int StubCompareFileTime(char* filename1, char* filename2, int* compare_result)
{
    std::string detail = filename1 != nullptr ? filename1 : "";
    detail += " vs ";
    detail += filename2 != nullptr ? filename2 : "";
    RecordCallback("pfnCompareFileTime", detail);

    if (compare_result != nullptr)
    {
        *compare_result = 0;
    }

    return 0;
}

int StubIsDedicatedServer()
{
    RecordCallback("pfnIsDedicatedServer");
    return TRUE;
}

const char* StubCmdArgs()
{
    EngineShimState& state = CurrentShimState();
    RecordCallback("pfnCmd_Args", state.server_state.command_line_tail.empty()
        ? "<empty>"
        : state.server_state.command_line_tail);
    return state.server_state.Args();
}

const char* StubCmdArgv(int argument_index)
{
    EngineShimState& state = CurrentShimState();
    const char* value = state.server_state.Argv(argument_index);
    RecordCallback(
        "pfnCmd_Argv",
        std::to_string(argument_index) + "=" + (value != nullptr && *value != '\0' ? value : "<empty>"));
    return value;
}

int StubCmdArgc()
{
    const int argc = CurrentShimState().server_state.Argc();
    RecordCallback("pfnCmd_Argc", std::to_string(argc));
    return argc;
}

void StubEndSection(const char* section_name)
{
    const std::string name = section_name != nullptr ? section_name : "";
    RecordCallback("pfnEndSection", name.empty() ? "<empty>" : name);
}

int StubCheckParm(const char* token, char** next)
{
    EngineShimState& state = CurrentShimState();
    const std::string text = token != nullptr ? token : "";
    const int index = state.server_state.CheckParm(token, next);
    RecordCallback(
        "pfnCheckParm",
        (text.empty() ? "<empty>" : text)
        + " -> " + (index == 0 ? "not found" : "found@" + std::to_string(index)));
    return index;
}

char* StubGetInfoKeyBuffer(edict_t* /*entity*/)
{
    RecordCallback("pfnGetInfoKeyBuffer");
    return MutableEmptyString();
}

char* StubInfoKeyValue(char* /*info_buffer*/, char* key)
{
    const std::string text = key != nullptr ? key : "";
    RecordCallback("pfnInfoKeyValue", text.empty() ? "<empty>" : text);
    return MutableEmptyString();
}

void StubSetKeyValue(char* /*info_buffer*/, char* key, char* value)
{
    std::string detail = key != nullptr ? key : "";
    detail += "=";
    detail += value != nullptr ? value : "";
    RecordCallback("pfnSetKeyValue", detail);
}

void StubSetClientKeyValue(int client_index, char* /*info_buffer*/, char* key, char* value)
{
    std::string detail = "client=" + std::to_string(client_index);
    detail += ", ";
    detail += key != nullptr ? key : "";
    detail += "=";
    detail += value != nullptr ? value : "";
    RecordCallback("pfnSetClientKeyValue", detail);
}

int StubNumberOfEntities()
{
    const int count = CurrentShimState().edict_store.NumberOfEntities();
    RecordCallback("pfnNumberOfEntities", std::to_string(count));
    return count;
}

const char* StubGetPlayerAuthId(edict_t* /*entity*/)
{
    RecordCallback("pfnGetPlayerAuthId");
    return "BOT";
}

unsigned char* StubSetFatPVS(float* /*origin*/)
{
    RecordCallback("pfnSetFatPVS");
    return EmptyVisibilitySet().data();
}

unsigned char* StubSetFatPAS(float* /*origin*/)
{
    RecordCallback("pfnSetFatPAS");
    return EmptyVisibilitySet().data();
}

void PopulateEngineFunctions(enginefuncs_t& engine_functions)
{
    engine_functions = {};
    engine_functions.pfnPrecacheModel = &StubPrecacheModel;
    engine_functions.pfnPrecacheSound = &StubPrecacheSound;
    engine_functions.pfnSetModel = &StubSetModel;
    engine_functions.pfnModelIndex = &StubModelIndex;
    engine_functions.pfnModelFrames = &StubModelFrames;
    engine_functions.pfnSetSize = &StubSetSize;
    engine_functions.pfnSetOrigin = &StubSetOrigin;
    engine_functions.pfnFindEntityByString = &StubFindEntityByString;
    engine_functions.pfnFindEntityInSphere = &StubFindEntityInSphere;
    engine_functions.pfnEntitiesInPVS = &StubEntitiesInPVS;
    engine_functions.pfnMakeVectors = &StubMakeVectors;
    engine_functions.pfnAngleVectors = &StubAngleVectors;
    engine_functions.pfnChangeYaw = &StubChangeYaw;
    engine_functions.pfnChangePitch = &StubChangePitch;
    engine_functions.pfnLightStyle = &StubLightStyle;
    engine_functions.pfnCreateEntity = &StubCreateEntity;
    engine_functions.pfnRemoveEntity = &StubRemoveEntity;
    engine_functions.pfnCreateNamedEntity = &StubCreateNamedEntity;
    engine_functions.pfnDropToFloor = &StubDropToFloor;
    engine_functions.pfnWalkMove = &StubWalkMove;
    engine_functions.pfnEmitSound = &StubEmitSound;
    engine_functions.pfnEmitAmbientSound = &StubEmitAmbientSound;
    engine_functions.pfnTraceLine = &StubTraceLine;
    engine_functions.pfnTraceHull = &StubTraceHull;
    engine_functions.pfnPointContents = &StubPointContents;
    engine_functions.pfnMessageBegin = &StubMessageBegin;
    engine_functions.pfnMessageEnd = &StubMessageEnd;
    engine_functions.pfnWriteByte = &StubWriteByte;
    engine_functions.pfnWriteChar = &StubWriteChar;
    engine_functions.pfnWriteShort = &StubWriteShort;
    engine_functions.pfnWriteLong = &StubWriteLong;
    engine_functions.pfnWriteAngle = &StubWriteAngle;
    engine_functions.pfnWriteCoord = &StubWriteCoord;
    engine_functions.pfnWriteString = &StubWriteString;
    engine_functions.pfnWriteEntity = &StubWriteEntity;
    engine_functions.pfnCVarRegister = &StubCVarRegister;
    engine_functions.pfnCVarGetFloat = &StubCVarGetFloat;
    engine_functions.pfnCVarGetString = &StubCVarGetString;
    engine_functions.pfnCVarSetFloat = &StubCVarSetFloat;
    engine_functions.pfnCVarSetString = &StubCVarSetString;
    engine_functions.pfnServerCommand = &StubServerCommand;
    engine_functions.pfnServerExecute = &StubServerExecute;
    engine_functions.pfnServerPrint = &StubServerPrint;
    engine_functions.pfnAlertMessage = &StubAlertMessage;
    engine_functions.pfnEngineFprintf = &StubEngineFprintf;
    engine_functions.pfnGetGameDir = &StubGetGameDir;
    engine_functions.pfnDecalIndex = &StubDecalIndex;
    engine_functions.pfnCvar_RegisterVariable = &StubCvarRegisterVariable;
    engine_functions.pfnCVarGetPointer = &StubCVarGetPointer;
    engine_functions.pfnCvar_DirectSet = &StubCvarDirectSet;
    engine_functions.pfnPvAllocEntPrivateData = &StubPvAllocEntPrivateData;
    engine_functions.pfnPvEntPrivateData = &StubPvEntPrivateData;
    engine_functions.pfnFreeEntPrivateData = &StubFreeEntPrivateData;
    engine_functions.pfnSzFromIndex = &StubSzFromIndex;
    engine_functions.pfnAllocString = &StubAllocString;
    engine_functions.pfnGetVarsOfEnt = &StubGetVarsOfEnt;
    engine_functions.pfnPEntityOfEntOffset = &StubPEntityOfEntOffset;
    engine_functions.pfnEntOffsetOfPEntity = &StubEntOffsetOfPEntity;
    engine_functions.pfnIndexOfEdict = &StubIndexOfEdict;
    engine_functions.pfnPEntityOfEntIndex = &StubPEntityOfEntIndex;
    engine_functions.pfnFindEntityByVars = &StubFindEntityByVars;
    engine_functions.pfnRegUserMsg = &StubRegUserMsg;
    engine_functions.pfnTime = &StubTime;
    engine_functions.pfnRandomLong = &StubRandomLong;
    engine_functions.pfnRandomFloat = &StubRandomFloat;
    engine_functions.pfnLoadFileForMe = &StubLoadFileForMe;
    engine_functions.pfnFreeFile = &StubFreeFile;
    engine_functions.pfnCompareFileTime = &StubCompareFileTime;
    engine_functions.pfnIsDedicatedServer = &StubIsDedicatedServer;
    engine_functions.pfnPrecacheGeneric = &StubPrecacheGeneric;
    engine_functions.pfnPrecacheEvent = &StubPrecacheEvent;
    engine_functions.pfnCmd_Args = &StubCmdArgs;
    engine_functions.pfnCmd_Argv = &StubCmdArgv;
    engine_functions.pfnCmd_Argc = &StubCmdArgc;
    engine_functions.pfnEndSection = &StubEndSection;
    engine_functions.pfnCheckParm = &StubCheckParm;
    engine_functions.pfnGetInfoKeyBuffer = &StubGetInfoKeyBuffer;
    engine_functions.pfnInfoKeyValue = &StubInfoKeyValue;
    engine_functions.pfnSetKeyValue = &StubSetKeyValue;
    engine_functions.pfnSetClientKeyValue = &StubSetClientKeyValue;
    engine_functions.pfnNumberOfEntities = &StubNumberOfEntities;
    engine_functions.pfnGetPlayerAuthId = &StubGetPlayerAuthId;
    engine_functions.pfnSetFatPVS = &StubSetFatPVS;
    engine_functions.pfnSetFatPAS = &StubSetFatPAS;
}

void PopulateGlobalVariables(
    globalvars_t& global_variables,
    hl::game_api::detail::EngineStringPool& string_pool,
    const hl::game_api::detail::ServerState& server_state,
    int max_entities)
{
    global_variables = {};
    global_variables.trace_fraction = 1.0f;
    global_variables.v_forward = Vector(1.0f, 0.0f, 0.0f);
    global_variables.v_right = Vector(0.0f, 1.0f, 0.0f);
    global_variables.v_up = Vector(0.0f, 0.0f, 1.0f);
    global_variables.maxEntities = max_entities;
    hl::game_api::detail::ApplyGlobalsFromServerState(server_state, string_pool, global_variables);
}

bool SafeCallSpawn(int (*function)(edict_t*), edict_t* entity, int* result_value)
{
    __try
    {
        *result_value = function(entity);
        return true;
    }
    __except (HandleSehException("pfnSpawn", GetExceptionCode()))
    {
        return false;
    }
}

bool SafeCallWorldspawnSpawnSeh(
    int (*function)(edict_t*),
    edict_t* entity,
    int* result_value,
    unsigned int* seh_code)
{
    if (seh_code != nullptr)
    {
        *seh_code = 0;
    }

    __try
    {
        *result_value = function(entity);
        return true;
    }
    __except (seh_code != nullptr
            ? (*seh_code = GetExceptionCode(), EXCEPTION_EXECUTE_HANDLER)
            : EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

bool SafeCallServerActivateSeh(
    void (*function)(edict_t*, int, int),
    edict_t* edict_list,
    int edict_count,
    int client_max,
    unsigned int* seh_code)
{
    if (seh_code != nullptr)
    {
        *seh_code = 0;
    }

    __try
    {
        function(edict_list, edict_count, client_max);
        return true;
    }
    __except (seh_code != nullptr
            ? (*seh_code = GetExceptionCode(), EXCEPTION_EXECUTE_HANDLER)
            : EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

bool SafeCallStartFrameSeh(void (*function)(), unsigned int* seh_code)
{
    if (seh_code != nullptr)
    {
        *seh_code = 0;
    }

    __try
    {
        function();
        return true;
    }
    __except (seh_code != nullptr
            ? (*seh_code = GetExceptionCode(), EXCEPTION_EXECUTE_HANDLER)
            : EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

bool SafeCallThinkSeh(void (*function)(edict_t*), edict_t* entity, unsigned int* seh_code)
{
    if (seh_code != nullptr)
    {
        *seh_code = 0;
    }

    __try
    {
        function(entity);
        return true;
    }
    __except (seh_code != nullptr
            ? (*seh_code = GetExceptionCode(), EXCEPTION_EXECUTE_HANDLER)
            : EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

bool SafeCallUseSeh(
    void (*function)(edict_t*, edict_t*),
    edict_t* used_entity,
    edict_t* other_entity,
    unsigned int* seh_code)
{
    if (seh_code != nullptr)
    {
        *seh_code = 0;
    }

    __try
    {
        function(used_entity, other_entity);
        return true;
    }
    __except (seh_code != nullptr
            ? (*seh_code = GetExceptionCode(), EXCEPTION_EXECUTE_HANDLER)
            : EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

bool SafeCallKeyValue(void (*function)(edict_t*, KeyValueData*), edict_t* entity, KeyValueData* kvd)
{
    __try
    {
        function(entity, kvd);
        return true;
    }
    __except (HandleSehException("pfnKeyValue", GetExceptionCode()))
    {
        return false;
    }
}

int SpawnEntityStub(edict_t* entity)
{
    EngineShimState& state = CurrentShimState();
    hl::common::Logger::Info(
        "Spawn pipeline stub: pfnSpawn requested for " + DescribeEdictState(state, entity));

    if (state.dll_functions.pfnSpawn == nullptr)
    {
        hl::common::Logger::Warn("Spawn pipeline stub: hl.dll did not expose pfnSpawn.");
        return 0;
    }

    int result = 0;
    if (!SafeCallSpawn(state.dll_functions.pfnSpawn, entity, &result))
    {
        return 0;
    }

    hl::common::Logger::Info(
        "Spawn pipeline stub: pfnSpawn returned " + std::to_string(result));
    return result;
}

void DispatchKeyValueStub(edict_t* entity, KeyValueData* kvd)
{
    EngineShimState& state = CurrentShimState();
    const std::string key_name = kvd != nullptr && kvd->szKeyName != nullptr ? kvd->szKeyName : "<null>";
    const std::string key_value = kvd != nullptr && kvd->szValue != nullptr ? kvd->szValue : "<null>";
    hl::common::Logger::Info(
        "Spawn pipeline stub: pfnKeyValue requested for "
        + DescribeEdictState(state, entity)
        + " [" + key_name + "=" + key_value + "]");

    if (state.dll_functions.pfnKeyValue == nullptr)
    {
        hl::common::Logger::Warn("Spawn pipeline stub: hl.dll did not expose pfnKeyValue.");
        return;
    }

    if (kvd == nullptr)
    {
        hl::common::Logger::Warn("Spawn pipeline stub: pfnKeyValue received null KeyValueData.");
        return;
    }

    SafeCallKeyValue(state.dll_functions.pfnKeyValue, entity, kvd);
}

void LogSpawnPipelineStubAvailability(const EngineShimState& state)
{
    hl::common::Logger::Info("World bootstrap spawn pipeline stubs:");
    hl::common::Logger::Info("  - pfnLoadFileForMe: ready");
    hl::common::Logger::Info("  - pfnFreeFile: ready");
    hl::common::Logger::Info("  - pfnCreateNamedEntity: ready");
    hl::common::Logger::Info("  - pfnSetOrigin: ready");
    hl::common::Logger::Info("  - pfnSetSize: ready");
    hl::common::Logger::Info("  - pfnFindEntityByString: ready");
    hl::common::Logger::Info("  - pfnFindEntityInSphere: ready");
    hl::common::Logger::Info("  - pfnMakeVectors: ready");
    hl::common::Logger::Info("  - pfnChangeYaw: ready");
    hl::common::Logger::Info("  - pfnChangePitch: ready");
    hl::common::Logger::Info("  - pfnDropToFloor: ready");
    hl::common::Logger::Info("  - pfnWalkMove: ready");
    hl::common::Logger::Info("  - pfnRegUserMsg: ready");
    hl::common::Logger::Info("  - pfnLightStyle: ready");
    hl::common::Logger::Info("  - pfnPrecacheSound: ready");
    hl::common::Logger::Info("  - pfnPrecacheGeneric: ready");
    hl::common::Logger::Info("  - pfnPrecacheEvent: ready");
    hl::common::Logger::Info("  - pfnEmitSound: ready");
    hl::common::Logger::Info("  - pfnEmitAmbientSound: ready");
    hl::common::Logger::Info("  - pfnDecalIndex: ready");
    hl::common::Logger::Info("  - pfnRandomLong: ready (deterministic diagnostics)");
    hl::common::Logger::Info("  - pfnRandomFloat: ready (deterministic diagnostics)");
    hl::common::Logger::Info("  - pfnEntitiesInPVS: ready");
    hl::common::Logger::Info("  - pfnTraceLine: ready (no-op world trace)");
    hl::common::Logger::Info("  - pfnTraceHull: ready (no-op world trace)");
    hl::common::Logger::Info("  - pfnPointContents: ready (returns CONTENTS_EMPTY)");
    hl::common::Logger::Info("  - pfnMessageBegin/End: ready (diagnostic buffer)");
    hl::common::Logger::Info("  - pfnWriteByte/Char/Short/Long: ready");
    hl::common::Logger::Info("  - pfnWriteAngle/Coord/String/Entity: ready");
    hl::common::Logger::Info("  - pfnPvAllocEntPrivateData: ready");
    hl::common::Logger::Info("  - pfnPvEntPrivateData: ready");
    hl::common::Logger::Info("  - pfnFreeEntPrivateData: ready");
    hl::common::Logger::Info(
        std::string("  - pfnSpawn: ")
        + (state.dll_functions.pfnSpawn != nullptr ? "ready" : "missing in hl.dll"));
    hl::common::Logger::Info(
        std::string("  - pfnThink: ")
        + (state.dll_functions.pfnThink != nullptr ? "ready" : "missing in hl.dll"));
    hl::common::Logger::Info(
        std::string("  - pfnKeyValue: ")
        + (state.dll_functions.pfnKeyValue != nullptr ? "ready" : "missing in hl.dll"));
    hl::common::Logger::Info(
        std::string("  - pfnServerActivate: ")
        + (state.dll_functions.pfnServerActivate != nullptr ? "ready" : "missing in hl.dll"));
}

void PerformWorldBootstrap()
{
    EngineShimState& state = CurrentShimState();
    state.world_context = {};
    state.worldspawn_spawn_diagnostics.Reset();

    if (!hl::game_api::detail::LoadWorldModelContext(
            state.file_system,
            state.server_state,
            state.world_context))
    {
        return;
    }

    edict_t* worldspawn = state.edict_store.World();
    if (worldspawn == nullptr)
    {
        state.world_context.failure_reason = "World bootstrap could not access edict #0.";
        hl::common::Logger::Error(state.world_context.failure_reason);
        return;
    }

    state.edict_store.SetInUse(worldspawn, true);
    state.edict_store.SetClassname(worldspawn, state.string_pool.Alloc("worldspawn"), "worldspawn");
    BindEntityClassExport(state, worldspawn, "worldspawn", "world bootstrap");

    const int world_model_index =
        StubPrecacheModel(const_cast<char*>(state.world_context.model_path.c_str()));
    StubSetModel(worldspawn, state.world_context.model_path.c_str());

    state.world_context.world_model_index = world_model_index;
    state.world_context.world_edict_index = state.edict_store.IndexOf(worldspawn);
    state.world_context.prepared =
        state.world_context.bsp_loaded
        && state.world_context.world_model_index != 0
        && state.world_context.world_edict_index == 0;

    hl::common::Logger::Info(
        std::string("World bootstrap: context prepared: ")
        + BoolToYesNo(state.world_context.prepared));
    hl::common::Logger::Info(
        "World bootstrap: world entity state: "
        + state.edict_store.DumpEntityState(worldspawn, state.string_pool));
}

void PerformEntityBootstrap()
{
    EngineShimState& state = CurrentShimState();
    state.changelevel_transition_state = {};

    EntityBootstrapContext context;
    context.attempted = true;
    context.callback_counts_before = state.callback_counts;

    if (!state.world_context.prepared)
    {
        context.failure_reason = "World bootstrap was not prepared; entity parsing skipped.";
        context.newly_exercised_engine_callbacks =
            BuildCallbackDelta(context.callback_counts_before, state.callback_counts);
        state.entity_bootstrap = std::move(context);
        hl::common::Logger::Warn("Entity bootstrap skipped because world bootstrap is incomplete.");
        return;
    }

    if (!state.world_context.entities.present || state.world_context.entities.text.empty())
    {
        context.failure_reason = "Entities lump is missing or empty.";
        context.newly_exercised_engine_callbacks =
            BuildCallbackDelta(context.callback_counts_before, state.callback_counts);
        state.entity_bootstrap = std::move(context);
        hl::common::Logger::Warn("Entity bootstrap skipped because the BSP entities lump is empty.");
        return;
    }

    hl::common::Logger::Info("Entity bootstrap: parsing BSP entities lump.");
    hl::game_api::detail::EntityLumpParseResult parse_result =
        hl::game_api::detail::EntityLumpParser::Parse(state.world_context.entities.text);

    context.entities_lump_parsed = parse_result.parsed_any;
    context.partially_parsed = parse_result.partially_parsed;
    context.failure_reason = parse_result.failure_reason;
    context.parsed_entities = std::move(parse_result.entities);
    context.parse_errors = std::move(parse_result.errors);
    context.worldspawn_count = static_cast<std::size_t>(std::count_if(
        context.parsed_entities.begin(),
        context.parsed_entities.end(),
        [](const hl::game_api::detail::EntityDefinition& entity)
        {
            return EqualsIgnoreCase(entity.classname, "worldspawn");
        }));
    context.first_entity_is_worldspawn =
        !context.parsed_entities.empty()
        && EqualsIgnoreCase(context.parsed_entities.front().classname, "worldspawn");
    context.top_classname_counts =
        BuildTopClassnameCounts(context.parsed_entities, kClassnameSummaryCount);

    hl::common::Logger::Info(
        "Entity bootstrap: total parsed entities = "
        + std::to_string(context.parsed_entities.size()));
    hl::common::Logger::Info(
        "Entity bootstrap: worldspawn entities found = "
        + std::to_string(context.worldspawn_count));
    hl::common::Logger::Info(
        "Entity bootstrap: first parsed entity is worldspawn = "
        + std::string(BoolToYesNo(context.first_entity_is_worldspawn)));

    if (!context.parse_errors.empty())
    {
        hl::common::Logger::Warn(
            "Entity bootstrap: parser reported "
            + std::to_string(context.parse_errors.size()) + " issue(s).");
        for (const hl::game_api::detail::EntityParseError& error : context.parse_errors)
        {
            hl::common::Logger::Warn(
                "  - entity #" + std::to_string(error.entity_index) + ": " + error.message);
            if (!error.nearby_text.empty())
            {
                hl::common::Logger::Warn("    nearby: " + error.nearby_text);
            }
        }
    }

    if (!context.top_classname_counts.empty())
    {
        hl::common::Logger::Info("Entity bootstrap: top classname counts:");
        for (const hl::game_api::EntityClassCountSummary& entry : context.top_classname_counts)
        {
            hl::common::Logger::Info(
                "  - " + entry.classname + ": " + std::to_string(entry.count));
        }
    }

    if (!context.parsed_entities.empty())
    {
        hl::common::Logger::Info("Entity bootstrap: first parsed entities:");
        for (std::size_t index = 0;
             index < std::min<std::size_t>(context.parsed_entities.size(), kEntityDebugLogCount);
             ++index)
        {
            hl::common::Logger::Info(
                "  - " + BuildParsedEntitySummary(context.parsed_entities[index]));
        }
    }

    if (!context.entities_lump_parsed && !context.partially_parsed)
    {
        context.newly_exercised_engine_callbacks =
            BuildCallbackDelta(context.callback_counts_before, state.callback_counts);
        state.entity_bootstrap = std::move(context);
        hl::common::Logger::Warn(
            "Entity bootstrap aborted because no entity definitions could be parsed.");
        return;
    }

    if (context.worldspawn_count == 0)
    {
        hl::common::Logger::Warn("Entity bootstrap: no worldspawn entity was found in the BSP lump.");
    }
    else if (!context.first_entity_is_worldspawn)
    {
        hl::common::Logger::Warn(
            "Entity bootstrap: first parsed entity is not worldspawn; using the first worldspawn encountered.");
    }

    bool worldspawn_processed = false;
    std::size_t allocated_non_world_entities = 0;
    context.runtime_entities.reserve(context.parsed_entities.size());

    for (const hl::game_api::detail::EntityDefinition& definition : context.parsed_entities)
    {
        RuntimeEntityRecord record;
        record.parse_index = definition.ordinal;
        record.classname = definition.classname;
        record.source_preview = definition.source_preview;

        edict_t* entity = nullptr;
        const bool is_worldspawn = EqualsIgnoreCase(definition.classname, "worldspawn");
        if (is_worldspawn && !worldspawn_processed)
        {
            entity = state.edict_store.World();
            if (entity == nullptr)
            {
                record.deferred = true;
                record.support_state = RuntimeEntitySupportState::kDeferredUnsupported;
                AppendRecordNote(record, "world edict unavailable");
                ++context.deferred_entities;
                context.runtime_entities.push_back(std::move(record));
                continue;
            }

            worldspawn_processed = true;
            ++context.runtime_entities_allocated;
            state.edict_store.SetInUse(entity, true);
            state.edict_store.SetClassname(entity, state.string_pool.Alloc("worldspawn"), "worldspawn");
            state.edict_store.SetParseIndex(entity, static_cast<int>(definition.ordinal));
            state.edict_store.SetDeferred(entity, false);
            state.edict_store.SetSpawned(entity, false);
            BindEntityClassExport(state, entity, "worldspawn", "parsed worldspawn");
            record.edict_index = state.edict_store.IndexOf(entity);
            record.in_use = true;
            record.model = state.world_context.model_path;
            record.modelindex = state.world_context.world_model_index;
            ApplyParsedCommonFields(state, entity, definition, record, true);
        }
        else if (is_worldspawn)
        {
            record.deferred = true;
            record.support_state = RuntimeEntitySupportState::kDeferredUnsupported;
            AppendRecordNote(record, "duplicate worldspawn");
            ++context.deferred_entities;
            context.runtime_entities.push_back(std::move(record));
            continue;
        }
        else if (definition.classname.empty())
        {
            record.deferred = true;
            record.support_state = RuntimeEntitySupportState::kDeferredUnsupported;
            AppendRecordNote(record, "missing classname");
            ++context.deferred_entities;
            context.runtime_entities.push_back(std::move(record));
            continue;
        }
        else if (allocated_non_world_entities >= kEntityBootstrapAllocationLimit)
        {
            record.deferred = true;
            record.support_state = RuntimeEntitySupportState::kDeferredUnsupported;
            AppendRecordNote(record, "allocation limit reached");
            ++context.deferred_entities;
            context.runtime_entities.push_back(std::move(record));
            continue;
        }
        else
        {
            entity = CreateNamedEntityForMap(state, definition);
            if (entity == nullptr)
            {
                record.deferred = true;
                record.support_state = RuntimeEntitySupportState::kDeferredUnsupported;
                AppendRecordNote(record, "no free edict");
                ++context.deferred_entities;
                context.runtime_entities.push_back(std::move(record));
                continue;
            }

            ++allocated_non_world_entities;
            ++context.runtime_entities_allocated;
            record.edict_index = state.edict_store.IndexOf(entity);
            record.in_use = true;
            ApplyParsedCommonFields(state, entity, definition, record, false);
        }

        for (const hl::game_api::detail::EntityKeyValuePair& key_value : definition.key_values)
        {
            DispatchKeyValueToDll(state, entity, definition, key_value, record, context);
        }

        SyncRuntimeRecordFromEdict(state, entity, record);

        const EntitySupportDecision support = ClassifyEntitySupport(definition.classname);
        if (!support.allow_spawn)
        {
            record.deferred = true;
            record.support_state = RuntimeEntitySupportState::kDeferredUnsupported;
            AppendRecordNote(
                record,
                "deferred: " + (support.reason.empty() ? std::string("unsupported") : support.reason));
            state.edict_store.SetDeferred(entity, true);
            state.edict_store.SetActivationCandidate(entity, false);
            state.edict_store.SetSpawned(entity, false);
            if (support.keep_resident_when_deferred)
            {
                hl::game_api::detail::TryWriteNextThink(
                    state.edict_store,
                    entity,
                    0.0f,
                    [&](std::string_view message)
                    {
                        hl::common::Logger::Warn(std::string(message));
                    });
                AppendRecordNote(record, "retained for deferred target resolution");
            }
            else if (entity != nullptr && !is_worldspawn)
            {
                state.edict_store.RemoveEntity(entity);
            }
            SyncRuntimeRecordFromEdict(state, entity, record);
            if (EqualsIgnoreCase(definition.classname, "trigger_changelevel"))
            {
                NoteTriggerChangeLevelCandidate(
                    state.changelevel_transition_state,
                    &definition,
                    &record);
            }
            ++context.deferred_entities;
            context.runtime_entities.push_back(std::move(record));
            continue;
        }

        SpawnEntityFromDll(state, entity, record, context);
        SyncRuntimeRecordFromEdict(state, entity, record);
        context.runtime_entities.push_back(std::move(record));
    }

    context.removed_entities = static_cast<std::size_t>(std::count_if(
        context.runtime_entities.begin(),
        context.runtime_entities.end(),
        [](const RuntimeEntityRecord& record)
        {
            return record.removed;
        }));
    context.classname_support_summary = BuildClassSupportSummary(context.runtime_entities);

    context.newly_exercised_engine_callbacks =
        BuildCallbackDelta(context.callback_counts_before, state.callback_counts);
    state.entity_bootstrap = std::move(context);

    const EntityBootstrapContext& summary = state.entity_bootstrap;
    hl::common::Logger::Info("Entity bootstrap summary:");
    hl::common::Logger::Info(
        std::string("  - entities lump parsed: ")
        + BoolToYesNo(summary.entities_lump_parsed || summary.partially_parsed));
    hl::common::Logger::Info(
        "  - total parsed entities: " + std::to_string(summary.parsed_entities.size()));
    hl::common::Logger::Info(
        "  - total worldspawn entities found: " + std::to_string(summary.worldspawn_count));
    hl::common::Logger::Info(
        "  - total runtime entities allocated: "
        + std::to_string(summary.runtime_entities_allocated));
    hl::common::Logger::Info(
        "  - total keyvalues dispatched: " + std::to_string(summary.keyvalues_dispatched));
    hl::common::Logger::Info(
        "  - total keyvalues handled: " + std::to_string(summary.keyvalues_handled));
    hl::common::Logger::Info(
        "  - total spawn attempts: " + std::to_string(summary.spawn_attempts));
    hl::common::Logger::Info(
        "  - total successful spawns: " + std::to_string(summary.successful_spawns));
    hl::common::Logger::Info(
        "  - total removed entities: " + std::to_string(summary.removed_entities));
    hl::common::Logger::Info(
        "  - total deferred entities: " + std::to_string(summary.deferred_entities));

    if (!summary.failure_reason.empty())
    {
        hl::common::Logger::Info("  - parser note: " + summary.failure_reason);
    }

    if (!summary.top_classname_counts.empty())
    {
        hl::common::Logger::Info("  - top classname counts:");
        for (const hl::game_api::EntityClassCountSummary& entry : summary.top_classname_counts)
        {
            hl::common::Logger::Info(
                "    * " + entry.classname + ": " + std::to_string(entry.count));
        }
    }

    if (!summary.classname_support_summary.empty())
    {
        hl::common::Logger::Info("  - classname support summary:");
        for (const hl::game_api::EntityClassSupportSummary& entry : summary.classname_support_summary)
        {
            hl::common::Logger::Info(
                "    * " + entry.classname
                + " | spawned=" + std::to_string(entry.spawned_successfully)
                + ", removed=" + std::to_string(entry.removed_during_spawn)
                + ", deferred=" + std::to_string(entry.deferred_unsupported)
                + ", failed=" + std::to_string(entry.failed_during_spawn));
        }
    }

    hl::common::Logger::Info("  - sample runtime entities:");
    for (std::size_t index = 0;
         index < std::min<std::size_t>(summary.runtime_entities.size(), kEntityDebugLogCount);
         ++index)
    {
        hl::common::Logger::Info(
            "    * " + BuildRuntimeRecordSummary(summary.runtime_entities[index]));
    }

    hl::common::Logger::Info("  - sample spawned entities:");
    std::size_t spawned_logged = 0;
    for (const RuntimeEntityRecord& record : summary.runtime_entities)
    {
        if (!record.spawned)
        {
            continue;
        }

        hl::common::Logger::Info("    * " + BuildRuntimeRecordSummary(record));
        ++spawned_logged;
        if (spawned_logged >= kSampleSpawnedCount)
        {
            break;
        }
    }
    if (spawned_logged == 0)
    {
        hl::common::Logger::Info("    * none");
    }

    if (!summary.newly_exercised_engine_callbacks.empty())
    {
        hl::common::Logger::Info("  - newly exercised engine callbacks during entity bootstrap:");
        for (const hl::game_api::InvokedEngineCallback& callback :
             summary.newly_exercised_engine_callbacks)
        {
            hl::common::Logger::Info(
                "    * " + callback.name + " x" + std::to_string(callback.call_count));
        }
    }
    else
    {
        hl::common::Logger::Info("  - newly exercised engine callbacks during entity bootstrap: none");
    }

    hl::common::Logger::Info(
        "  - readiness for next step: "
        + std::string(
            summary.entities_lump_parsed || summary.partially_parsed
                ? "server activation / broader entity support"
                : "entity parsing remediation"));
}

void PerformServerActivation()
{
    EngineShimState& state = CurrentShimState();
    state.server_activation_diagnostics.Reset();
    state.server_activation_state = {};
    state.path_track_terminal_dead_end_targets.clear();

    hl::game_api::ServerActivationStateSummary& summary = state.server_activation_state;
    summary.function_present = state.dll_functions.pfnServerActivate != nullptr;
    summary.map_name = state.server_state.map_name;
    summary.max_clients = state.globalvars.maxClients;
    summary.globals_snapshot = BuildGlobalsSnapshotText(state);
    summary.server_state_snapshot = BuildServerFlagsSnapshotText(state);

    for (int index = 1; index < state.edict_store.MaxEntities(); ++index)
    {
        edict_t* entity = state.edict_store.EntityOfIndex(index);
        if (entity == nullptr)
        {
            continue;
        }

        const hl::game_api::detail::EntityStateSnapshot snapshot =
            state.edict_store.SnapshotOf(entity, state.string_pool);
        const bool prune_helper =
            snapshot.in_use
            && !snapshot.removed
            && !snapshot.deferred
            && snapshot.parse_index < 0
            && !snapshot.spawned;
        if (!prune_helper)
        {
            continue;
        }

        hl::common::Logger::Warn(
            "Server activation: pruning helper edict before activation: "
            + BuildEntitySnapshotSummary(snapshot));
        state.edict_store.RemoveEntity(entity);
    }

    const hl::game_api::detail::EntityStateSnapshot world_snapshot =
        state.edict_store.SnapshotOf(state.edict_store.World(), state.string_pool);
    const ActivationPreparationResult preparation = CollectActivationParticipants(state);
    const std::vector<ActivationParticipant>& participants = preparation.participants;

    for (RuntimeEntityRecord& record : state.entity_bootstrap.runtime_entities)
    {
        if (record.edict_index < 0)
        {
            continue;
        }

        if (edict_t* entity = state.edict_store.EntityOfIndex(record.edict_index);
            entity != nullptr)
        {
            SyncRuntimeRecordFromEdict(state, entity, record);
        }
    }

    summary.activation_candidate_count = static_cast<int>(participants.size());
    summary.pruned_participants = preparation.pruned_count;
    summary.allocated_edicts = state.edict_store.AllocatedCount();
    summary.edict_count_passed = state.edict_store.NumberOfEntities();
    const bool world_rejected = std::any_of(
        preparation.pruned_entities.begin(),
        preparation.pruned_entities.end(),
        [](const std::string& line)
        {
            return line.rfind("worldspawn invalid:", 0) == 0;
        });
    summary.worldspawn_spawned =
        world_snapshot.spawned
        && !world_snapshot.removed
        && EqualsIgnoreCase(world_snapshot.classname, "worldspawn")
        && state.worldspawn_spawn_diagnostics.Succeeded()
        && !world_rejected;
    summary.edict0_valid =
        world_snapshot.index == 0
        && world_snapshot.in_use
        && !world_snapshot.removed
        && EqualsIgnoreCase(world_snapshot.classname, "worldspawn")
        && !world_rejected;

    for (int index = 0; index < state.edict_store.MaxEntities(); ++index)
    {
        const edict_t* entity = state.edict_store.EntityOfIndex(index);
        const hl::game_api::detail::EntityStateSnapshot snapshot =
            state.edict_store.SnapshotOf(entity, state.string_pool);
        if (snapshot.index < 0)
        {
            continue;
        }

        if (snapshot.spawned && snapshot.in_use)
        {
            ++summary.spawned_edicts;
        }

        if (snapshot.removed)
        {
            ++summary.removed_edicts;
        }

        if (snapshot.deferred)
        {
            ++summary.deferred_edicts;
        }
    }

    summary.activation_class_counts = preparation.class_counts;
    summary.pruned_entities_preview = preparation.pruned_entities;
    summary.validation_rejections = preparation.validation_rejections;
    summary.likely_blocker = preparation.likely_blocker;

    for (std::size_t index = 0;
         index < std::min<std::size_t>(participants.size(), kActivationPreviewCount);
         ++index)
    {
        summary.activation_entities_preview.push_back(participants[index].summary);
    }

    std::vector<std::string> activation_visit_order_preview;
    activation_visit_order_preview.reserve(kActivationPreviewCount);
    const edict_t* first_activation_entity = nullptr;
    for (int index = 0; index < summary.edict_count_passed; ++index)
    {
        const edict_t* entity = state.edict_store.EntityOfIndex(index);
        const hl::game_api::detail::EntityStateSnapshot snapshot =
            state.edict_store.SnapshotOf(entity, state.string_pool);
        if (snapshot.index < 0 || !snapshot.in_use || !snapshot.private_data_present)
        {
            continue;
        }

        if (index < summary.max_clients)
        {
            continue;
        }

        if (first_activation_entity == nullptr)
        {
            first_activation_entity = entity;
        }

        if (activation_visit_order_preview.size() < kActivationPreviewCount)
        {
            activation_visit_order_preview.push_back(BuildEntitySnapshotSummary(snapshot));
        }
    }

    hl::common::Logger::Info("Server activation preflight:");
    hl::common::Logger::Info(
        "  - pfnServerActivate present: " + std::string(BoolToYesNo(summary.function_present)));
    hl::common::Logger::Info(
        "  - worldspawn spawned: " + std::string(BoolToYesNo(summary.worldspawn_spawned)));
    hl::common::Logger::Info(
        "  - edict#0 valid: " + std::string(BoolToYesNo(summary.edict0_valid)));
    hl::common::Logger::Info(
        "  - allocated edicts: " + std::to_string(summary.allocated_edicts));
    hl::common::Logger::Info(
        "  - edict count passed: " + std::to_string(summary.edict_count_passed));
    hl::common::Logger::Info(
        "  - spawned edicts: " + std::to_string(summary.spawned_edicts));
    hl::common::Logger::Info(
        "  - removed/deferred: " + std::to_string(summary.removed_edicts)
        + "/" + std::to_string(summary.deferred_edicts));
    hl::common::Logger::Info(
        "  - activation candidates: " + std::to_string(summary.activation_candidate_count));
    hl::common::Logger::Info(
        "  - activation participants pruned: " + std::to_string(summary.pruned_participants));
    hl::common::Logger::Info(
        "  - maxClients: " + std::to_string(summary.max_clients));
    hl::common::Logger::Info(
        "  - map: " + (summary.map_name.empty() ? std::string("<empty>") : summary.map_name));
    hl::common::Logger::Info("  - gpGlobals: " + summary.globals_snapshot);
    hl::common::Logger::Info("  - server flags: " + summary.server_state_snapshot);

    if (!summary.activation_entities_preview.empty())
    {
        hl::common::Logger::Info("  - activation participants preview:");
        for (const std::string& line : summary.activation_entities_preview)
        {
            hl::common::Logger::Info("    * " + line);
        }
    }
    else
    {
        hl::common::Logger::Info("  - activation participants preview: <empty>");
    }

    if (!summary.activation_class_counts.empty())
    {
        hl::common::Logger::Info("  - activation classname counts:");
        for (const hl::game_api::EntityClassCountSummary& entry : summary.activation_class_counts)
        {
            hl::common::Logger::Info(
                "    * " + entry.classname + ": " + std::to_string(entry.count));
        }
    }

    if (!activation_visit_order_preview.empty())
    {
        hl::common::Logger::Info("  - activation visit order preview:");
        for (const std::string& line : activation_visit_order_preview)
        {
            hl::common::Logger::Info("    * " + line);
        }
    }

    if (!summary.pruned_entities_preview.empty())
    {
        hl::common::Logger::Info("  - activation pruned entities:");
        for (const std::string& line : summary.pruned_entities_preview)
        {
            hl::common::Logger::Info("    * " + line);
        }
    }

    const bool ready_for_activation =
        summary.function_present
        && summary.worldspawn_spawned
        && summary.edict0_valid
        && summary.allocated_edicts > 0
        && summary.edict_count_passed > 0
        && summary.activation_candidate_count > 0
        && summary.max_clients > 0
        && !summary.map_name.empty()
        && !state.string_pool.Describe(state.globalvars.mapname).empty();
    summary.preflight_succeeded = ready_for_activation;

    if (!ready_for_activation)
    {
        hl::common::Logger::Warn(
            "Server activation preflight failed; pfnServerActivate will not be called.");
        if (!summary.likely_blocker.empty())
        {
            hl::common::Logger::Warn("Server activation likely blocker: " + summary.likely_blocker);
        }
        return;
    }

    hl::common::Logger::Info(
        "Server activation: begin [edictCount=" + std::to_string(summary.edict_count_passed)
        + ", clientMax=" + std::to_string(summary.max_clients) + "]");

    summary.attempted = true;
    state.server_activation_diagnostics.BeginAttempt(
        summary.map_name,
        summary.edict_count_passed,
        summary.max_clients);
    RecordActivationInternalEvent(
        state,
        "activation-enter",
        "edictCount=" + std::to_string(summary.edict_count_passed)
            + ", clientMax=" + std::to_string(summary.max_clients)
            + ", participants=" + std::to_string(summary.activation_candidate_count)
            + ", pruned=" + std::to_string(summary.pruned_participants));
    if (!activation_visit_order_preview.empty())
    {
        RecordActivationInternalEvent(
            state,
            "activation-first-target",
            activation_visit_order_preview.front(),
            first_activation_entity);
    }

    unsigned int seh_code = 0;
    if (SafeCallServerActivateSeh(
            state.dll_functions.pfnServerActivate,
            state.edict_store.EntityOfIndex(0),
            summary.edict_count_passed,
            summary.max_clients,
            &seh_code))
    {
        state.server_activation_diagnostics.MarkSucceeded();
        summary.succeeded = true;
        state.server_state.active = true;
        summary.server_state_snapshot = BuildServerFlagsSnapshotText(state);
        hl::common::Logger::Info("Server activation: end [success]");
    }
    else
    {
        state.server_activation_diagnostics.MarkSehException(seh_code);
        summary.seh_exception = true;
        summary.seh_code = seh_code;
        hl::common::Logger::Error(
            "Server activation raised SEH " + FormatExceptionCode(seh_code));
        LogServerActivationDistinctCallbackTail(state);
        LogServerActivationTraceTail(state);
        hl::common::Logger::Info("Server activation crash participant snapshot:");
        for (const ActivationParticipant& participant : participants)
        {
            hl::common::Logger::Info("  - " + participant.summary);
        }
        LogEdictTableSummary(state, "Server activation crash edict table:");
        LogServerStateSnapshot(state, "Server activation crash server snapshot:");
        LogGlobalsSnapshot(state, "Server activation crash gpGlobals snapshot:");
        LogPrecacheRegistrySnapshot(state, "Server activation crash registry snapshot:");
        LogCommandQueueSnapshot(
            state.command_buffer.Snapshot(),
            "Server activation crash command queue snapshot:");
    }

    for (RuntimeEntityRecord& record : state.entity_bootstrap.runtime_entities)
    {
        if (record.edict_index < 0)
        {
            continue;
        }

        if (edict_t* entity = state.edict_store.EntityOfIndex(record.edict_index);
            entity != nullptr)
        {
            SyncRuntimeRecordFromEdict(state, entity, record);
        }
    }

    for (const hl::game_api::detail::SpawnTraceEvent& event :
         TakeTraceTail(state.server_activation_diagnostics.DistinctCallbackSnapshot(), 32))
    {
        summary.distinct_callback_tail.push_back(FormatSpawnTraceEvent(event));
    }

    for (const hl::game_api::detail::SpawnTraceEvent& event :
         TakeTraceTail(state.server_activation_diagnostics.TraceSnapshot(), 64))
    {
        summary.callback_trace_tail.push_back(FormatSpawnTraceEvent(event));
    }

    summary.trace_events_captured = state.server_activation_diagnostics.EventCount();
    if (!summary.succeeded && summary.likely_blocker.empty())
    {
        if (summary.seh_exception && state.server_activation_diagnostics.DistinctCallbackSnapshot().empty())
        {
            summary.likely_blocker = !activation_visit_order_preview.empty()
                ? "no engine callbacks observed; likely invalid private data/object state at first activatable entity: "
                    + activation_visit_order_preview.front()
                : "no engine callbacks observed before SEH inside pfnServerActivate";
        }
        else if (const hl::game_api::detail::SpawnTraceEvent* last_event =
                     state.server_activation_diagnostics.LastEvent();
                 last_event != nullptr)
        {
            summary.likely_blocker =
                "last activation event before failure: " + FormatSpawnTraceEvent(*last_event);
        }
    }

    state.entity_bootstrap.removed_entities = static_cast<std::size_t>(std::count_if(
        state.entity_bootstrap.runtime_entities.begin(),
        state.entity_bootstrap.runtime_entities.end(),
        [](const RuntimeEntityRecord& record)
        {
            return record.removed;
        }));
}

void PerformServerFrameLoop()
{
    EngineShimState& state = CurrentShimState();
    state.server_frame_diagnostics.Reset();
    state.entity_think_diagnostics.Reset();
    state.frame_message_buffer.Reset();
    state.server_frame_loop_state = {};
    state.entity_think_scheduler_state = {};
    state.map_logic_dispatcher_state = {};
    state.scripted_logic_state = {};
    state.scripted_movement_state = {};
    state.server_frame_loop_state.configured = true;
    state.server_frame_loop_state.frames_requested = state.frame_bootstrap_options.frames;
    state.server_frame_loop_state.fixed_frametime = state.frame_bootstrap_options.frametime;
    state.server_frame_loop_state.start_frame_present = state.dll_functions.pfnStartFrame != nullptr;
    state.server_frame_loop_state.activation_succeeded = state.server_activation_state.succeeded;
    state.entity_think_scheduler_state.configured = true;
    state.entity_think_scheduler_state.start_frame_present =
        state.dll_functions.pfnStartFrame != nullptr;
    state.entity_think_scheduler_state.think_dispatch_present =
        state.dll_functions.pfnThink != nullptr;
    state.entity_think_scheduler_state.think_limit = state.frame_bootstrap_options.think_limit;
    state.map_logic_dispatcher_state.configured = true;
    state.map_logic_dispatcher_state.use_dispatch_present = state.dll_functions.pfnUse != nullptr;
    state.map_logic_dispatcher_state.use_limit = state.frame_bootstrap_options.use_limit;
    state.map_logic_dispatcher_state.scheduled_use_limit =
        state.frame_bootstrap_options.scheduled_use_limit;
    state.scripted_logic_state.configured = true;
    state.scripted_logic_state.trace_scripted = state.frame_bootstrap_options.trace_scripted;

    if (!state.server_activation_state.succeeded)
    {
        state.server_frame_loop_state.readiness =
            hl::game_api::detail::BuildServerFrameLoopReadiness(state.server_frame_loop_state);
        state.entity_think_scheduler_state.readiness =
            "scheduled think waits for successful ServerActivate";
        state.map_logic_dispatcher_state.readiness =
            "map logic dispatcher waits for successful ServerActivate";
        RefreshScriptedLogicStateSummary(state);
        state.scripted_movement_state.configured = true;
        state.scripted_movement_state.trace_movement =
            state.frame_bootstrap_options.trace_movement;
        state.scripted_movement_state.brush_doors.readiness =
            "brush-door bootstrap waits for successful ServerActivate";
        state.scripted_movement_state.readiness =
            "scene movement bootstrap waits for successful ServerActivate";
        hl::common::Logger::Warn(
            "ServerFrameLoop: skipped because ServerActivate did not succeed.");
        return;
    }

    hl::game_api::detail::FrameLoopConfig config;
    config.frames = state.frame_bootstrap_options.frames;
    config.frametime = state.frame_bootstrap_options.frametime;
    config.entity_preview_limit = 8;
    config.trace_tail_limit = 32;
    config.message_preview_limit = 8;

    hl::game_api::detail::EntityThinkScheduler think_scheduler;
    hl::game_api::detail::EntityThinkSchedulerConfig think_config;
    think_config.think_limit = state.frame_bootstrap_options.think_limit;
    think_config.due_preview_limit = 8;
    think_config.trace_tail_limit = 32;
    think_config.rolling_trace_limit = 64;
    think_config.trace_think = state.frame_bootstrap_options.trace_think;
    think_scheduler.Configure(
        think_config,
        state.dll_functions.pfnStartFrame != nullptr,
        state.dll_functions.pfnThink != nullptr);

    hl::game_api::detail::MapLogicDispatcher map_logic_dispatcher;
    hl::game_api::detail::MapLogicDispatcherConfig map_logic_config;
    map_logic_config.use_limit = state.frame_bootstrap_options.use_limit;
    map_logic_config.scheduled_use_limit = state.frame_bootstrap_options.scheduled_use_limit;
    map_logic_config.trace_tail_limit = 32;
    map_logic_config.rolling_trace_limit = 64;
    map_logic_dispatcher.Configure(
        map_logic_config,
        state.dll_functions.pfnUse != nullptr);

    hl::game_api::detail::TrackPathResolver track_path_resolver;
    hl::game_api::detail::TrackPathResolverConfig track_path_config;
    track_path_config.preview_limit = 8;
    track_path_resolver.Configure(track_path_config);

    hl::game_api::detail::ScriptedMovementController movement_controller;
    hl::game_api::detail::ScriptedMovementControllerConfig movement_config;
    movement_config.trace_movement = state.frame_bootstrap_options.trace_movement;
    movement_config.preview_limit = 8;
    movement_config.frame_history_limit = 128;
    movement_config.canary_limit = 6;
    movement_controller.Configure(movement_config);

    hl::game_api::detail::PathMoverController path_mover_controller;
    hl::game_api::detail::PathMoverControllerConfig path_mover_config;
    path_mover_config.arrival_epsilon = state.frame_bootstrap_options.path_arrival_epsilon;
    path_mover_config.trace_movement = state.frame_bootstrap_options.trace_movement;
    path_mover_config.preview_limit = 8;
    path_mover_config.frame_history_limit = 128;
    path_mover_controller.Configure(path_mover_config);

    hl::game_api::detail::BrushDoorBootstrapController brush_door_controller;
    hl::game_api::detail::BrushDoorBootstrapConfig brush_door_config;
    brush_door_config.default_speed = 100.0f;
    brush_door_config.default_lip = 8.0f;
    brush_door_config.arrival_epsilon = 1.0f;
    brush_door_config.preview_limit = 8;
    brush_door_config.history_limit = 128;
    brush_door_controller.Configure(brush_door_config);

    std::unordered_set<std::string> movement_callbacks_exercised;
    std::unordered_set<std::string> path_mover_callbacks_exercised;
    std::unordered_set<std::string> brush_door_callbacks_exercised;

    hl::game_api::detail::ServerFrameLoopHooks hooks;
    hooks.validate_frame_state =
        [&](std::size_t preview_limit)
        {
            return CollectFrameLoopValidationState(state, preview_limit);
        };
    hooks.advance_time =
        [&](float frametime)
        {
            AdvanceFrameClock(state, frametime);
        };
    hooks.host_frame_index =
        [&]()
        {
            return state.server_state.frame_count;
        };
    hooks.server_frame_index =
        [&]()
        {
            return state.server_state.server_frame;
        };
    hooks.global_time =
        [&]()
        {
            return state.globalvars.time;
        };
    hooks.global_frametime =
        [&]()
        {
            return state.globalvars.frametime;
        };
    hooks.globals_snapshot =
        [&]()
        {
            return BuildGlobalsSnapshotText(state);
        };
    hooks.server_snapshot =
        [&]()
        {
            return BuildServerFlagsSnapshotText(state);
        };
    hooks.start_frame_present =
        [&]()
        {
            return state.dll_functions.pfnStartFrame != nullptr;
        };
    hooks.begin_frame_observation =
        [&](int frame_number,
            std::uint64_t host_frame_index,
            std::uint64_t server_frame_index,
            float time,
            float frametime)
        {
            state.frame_message_buffer.Reset();
            state.server_frame_diagnostics.BeginFrame(
                state.server_state.map_name,
                frame_number,
                host_frame_index,
                server_frame_index,
                time,
                frametime);
            state.server_frame_diagnostics.RecordInternalEvent(
                "frame-enter",
                "host=" + std::to_string(host_frame_index)
                    + ", server=" + std::to_string(server_frame_index)
                    + ", time=" + std::to_string(time)
                    + ", frametime=" + std::to_string(frametime),
                0,
                "worldspawn");
        };
    hooks.mark_frame_success =
        [&]()
        {
            state.server_frame_diagnostics.MarkSucceeded();
        };
    hooks.mark_frame_seh =
        [&](unsigned int seh_code)
        {
            state.server_frame_diagnostics.MarkSehException(seh_code);
            LogServerFrameTraceTail(state);
        };
    hooks.call_start_frame =
        [&](unsigned int* seh_code)
        {
            return SafeCallStartFrameSeh(state.dll_functions.pfnStartFrame, seh_code);
        };
    hooks.post_start_frame_lifecycle =
        [&](int frame_number,
            std::uint64_t host_frame_index,
            std::uint64_t server_frame_index,
            float time,
            float frametime)
        {
            state.entity_think_diagnostics.BeginFrame(
                state.server_state.map_name,
                frame_number,
                host_frame_index,
                server_frame_index,
                time,
                frametime);
            state.entity_think_diagnostics.RecordInternalEvent(
                "think-enter",
                "host=" + std::to_string(host_frame_index)
                    + ", server=" + std::to_string(server_frame_index)
                    + ", time=" + std::to_string(time)
                    + ", frametime=" + std::to_string(frametime)
                    + ", thinkLimit=" + std::to_string(state.frame_bootstrap_options.think_limit),
                0,
                "worldspawn");
            ResetRuntimeRecordMapLogicFrameState(state);

            hl::game_api::detail::ScriptedMovementFrameContext movement_frame_context;
            movement_frame_context.frame_number = frame_number;
            movement_frame_context.host_frame_index = host_frame_index;
            movement_frame_context.server_frame_index = server_frame_index;
            movement_frame_context.time = time;
            movement_frame_context.frametime = frametime;
            movement_controller.BeginFrame(movement_frame_context);
            path_mover_controller.BeginFrame(movement_frame_context);
            brush_door_controller.BeginFrame(movement_frame_context);

            hl::game_api::detail::MapLogicDispatcherHooks map_logic_hooks;
            map_logic_hooks.host_frame_index =
                [host_frame_index]()
                {
                    return host_frame_index;
                };
            map_logic_hooks.server_frame_index =
                [server_frame_index]()
                {
                    return server_frame_index;
                };
            map_logic_hooks.global_time =
                [time]()
                {
                    return time;
                };
            map_logic_hooks.global_frametime =
                [frametime]()
                {
                    return frametime;
                };
            map_logic_hooks.find_entity_by_string =
                [](edict_t* start_search_after, const char* field_name, const char* value)
                {
                    return StubFindEntityByString(start_search_after, field_name, value);
                };
            map_logic_hooks.edict_index_of =
                [&](const edict_t* entity)
                {
                    return state.edict_store.IndexOf(entity);
                };
            map_logic_hooks.ent_offset_of_pentity =
                [](const edict_t* entity)
                {
                    return StubEntOffsetOfPEntity(entity);
                };
            map_logic_hooks.inspect_entity =
                [&](int edict_index)
                {
                    hl::game_api::detail::EntityVarSnapshot vars_snapshot;
                    hl::game_api::detail::TryReadEntityVars(
                        state.edict_store,
                        state.string_pool,
                        state.edict_store.EntityOfIndex(edict_index),
                        &vars_snapshot,
                        [&](std::string_view message)
                        {
                            hl::common::Logger::Warn(std::string(message));
                        });
                    return vars_snapshot;
                };
            map_logic_hooks.classify_support =
                [](const hl::game_api::detail::EntityVarSnapshot& snapshot)
                {
                    return ClassifyMapLogicSupportFromSnapshot(snapshot);
                };
            map_logic_hooks.allow_use =
                [](const hl::game_api::detail::EntityVarSnapshot& snapshot)
                {
                    const std::string normalized = ToLowerCopy(snapshot.classname);
                    if (normalized == "multi_manager")
                    {
                        return false;
                    }

                    return AllowUseForMapLogicSnapshot(snapshot);
                };
            map_logic_hooks.dispatch_use =
                [&](int target_edict_index, int source_edict_index, unsigned int* seh_code)
                {
                    if (seh_code != nullptr)
                    {
                        *seh_code = 0;
                    }

                    edict_t* target_entity = state.edict_store.EntityOfIndex(target_edict_index);
                    edict_t* source_entity = state.edict_store.EntityOfIndex(source_edict_index);
                    if (target_entity == nullptr || state.dll_functions.pfnUse == nullptr)
                    {
                        return false;
                    }

                    movement_callbacks_exercised.insert("pfnUse");
                    path_mover_callbacks_exercised.insert("pfnUse");

                    hl::game_api::detail::EntityVarSnapshot before_use_snapshot;
                    hl::game_api::detail::TryReadEntityVars(
                        state.edict_store,
                        state.string_pool,
                        target_entity,
                        &before_use_snapshot,
                        [&](std::string_view message)
                        {
                            hl::common::Logger::Warn(std::string(message));
                        });

                    const bool succeeded = SafeCallUseSeh(
                        state.dll_functions.pfnUse,
                        target_entity,
                        source_entity != nullptr ? source_entity : target_entity,
                        seh_code);

                    hl::game_api::detail::EntityVarSnapshot after_use_snapshot;
                    hl::game_api::detail::TryReadEntityVars(
                        state.edict_store,
                        state.string_pool,
                        target_entity,
                        &after_use_snapshot,
                        [&](std::string_view message)
                        {
                            hl::common::Logger::Warn(std::string(message));
                        });

                    if (RuntimeEntityRecord* record =
                            FindRuntimeRecordByEdictIndex(state, target_edict_index);
                        record != nullptr)
                    {
                        SyncRuntimeRecordFromEdict(state, target_entity, *record);
                        if (succeeded
                            && hl::game_api::detail::IsRelevantScriptedLogicClass(record->classname))
                        {
                            if (DidUseProgressState(before_use_snapshot, after_use_snapshot))
                            {
                                record->scripted_logic.internal_state_changed = true;
                                hl::game_api::detail::MarkScriptedLogicProgressed(
                                    record->scripted_logic);
                            }

                            if (ShouldTreatAsScheduledFollowUp(before_use_snapshot, after_use_snapshot))
                            {
                                record->scripted_logic.scheduled_follow_up = true;
                                record->scripted_logic.support_state =
                                    hl::game_api::ScriptedLogicSupportState::kScheduledUseSupported;
                                hl::game_api::detail::MarkScriptedLogicProgressed(
                                    record->scripted_logic);
                            }
                        }
                    }

                    return succeeded;
                };
            map_logic_hooks.mark_triggered_this_frame =
                [&](int edict_index)
                {
                    MarkRuntimeRecordTriggeredThisFrame(state, edict_index, frame_number);
                };
            map_logic_hooks.record_target_resolution =
                [&](int edict_index)
                {
                    AccumulateRuntimeRecordTargetResolution(state, edict_index);
                };
            map_logic_hooks.record_use_result =
                [&](int edict_index,
                    int source_edict_index,
                    std::string_view source_classname,
                    bool attempted,
                    bool succeeded,
                    bool deferred,
                    std::string_view detail)
                {
                    AccumulateRuntimeRecordUseStats(
                        state,
                        edict_index,
                        source_edict_index,
                        source_classname,
                        attempted,
                        succeeded,
                        deferred,
                        detail,
                        frame_number,
                        time);
                };
            map_logic_hooks.record_target_emission =
                [&](int source_edict_index,
                    std::string_view source_classname,
                    std::string_view target_name,
                    int use_type,
                    float value)
                {
                    NoteRuntimeRecordTargetEmission(
                        state,
                        source_edict_index,
                        source_classname,
                        target_name,
                        frame_number,
                        time);
                    (void)use_type;
                    (void)value;
                };
            map_logic_hooks.set_pending_scheduled_outputs =
                [&](int edict_index, int pending_count)
                {
                    SetRuntimeRecordPendingScheduledOutputs(state, edict_index, pending_count);
                };
            map_logic_hooks.record_scheduled_action_event =
                [&](const hl::game_api::detail::ScheduledUseAction& action,
                    std::string_view event_name,
                    std::string_view detail)
                {
                    NoteRuntimeRecordScheduledActionEvent(
                        state,
                        action,
                        event_name,
                        detail,
                        frame_number,
                        time);
                };
            map_logic_hooks.total_callback_counts =
                [&]()
                {
                    return state.callback_counts;
                };
            map_logic_hooks.alert_ai_console =
                [](std::string_view message)
                {
                    EmitAiConsoleDiagnostic(message);
                };
            map_logic_hooks.log_info =
                [](std::string_view message)
                {
                    hl::common::Logger::Debug(
                        hl::common::LogCategory::Use,
                        std::string(message));
                };
            map_logic_hooks.log_warn =
                [](std::string_view message)
                {
                    hl::common::Logger::Warn(
                        hl::common::LogCategory::Use,
                        std::string(message));
                };
            map_logic_hooks.log_error =
                [](std::string_view message)
                {
                    hl::common::Logger::Error(
                        hl::common::LogCategory::Use,
                        std::string(message));
                };

            hl::game_api::detail::BrushDoorBootstrapHooks brush_door_hooks;
            brush_door_hooks.entity_by_index =
                [&](int edict_index)
                {
                    return state.edict_store.EntityOfIndex(edict_index);
                };
            brush_door_hooks.set_origin =
                [&](edict_t* entity, const Vector& origin)
                {
                    float values[3] = {origin.x, origin.y, origin.z};
                    brush_door_callbacks_exercised.insert("pfnSetOrigin");
                    movement_callbacks_exercised.insert("pfnSetOrigin");
                    path_mover_callbacks_exercised.insert("pfnSetOrigin");
                    StubSetOrigin(entity, values);
                };
            brush_door_hooks.set_velocity =
                [&](edict_t* entity, const Vector& velocity)
                {
                    if (entity == nullptr)
                    {
                        return;
                    }
                    entity->v.velocity = velocity;
                };
            brush_door_hooks.log_info =
                [&](std::string_view message)
                {
                    LogSubsystemInfo(
                        hl::common::LogCategory::PathEvent,
                        state.frame_bootstrap_options.trace_movement,
                        message);
                };
            brush_door_hooks.log_warn =
                [](std::string_view message)
                {
                    hl::common::Logger::Warn(
                        hl::common::LogCategory::PathEvent,
                        std::string(message));
                };

            BrushDoorDispatchAggregate current_brush_door_dispatch;

            map_logic_hooks.custom_dispatch_target =
                [&](const hl::game_api::detail::MapLogicDispatchContext& context,
                    const hl::game_api::detail::EntityVarSnapshot& target_snapshot,
                    std::string* detail)
                {
                    const std::string normalized = ToLowerCopy(target_snapshot.classname);
                    if (normalized == "func_tracktrain")
                    {
                        path_mover_controller.RequestActivation(
                            target_snapshot.edict_index,
                            target_snapshot.targetname,
                            time,
                            context.reason,
                            BuildSourceEntityLabel(
                                state,
                                context.source_edict_index,
                                context.source_classname));
                        if (detail != nullptr)
                        {
                            *detail = "path mover activation";
                        }
                        return hl::game_api::detail::MapLogicTargetDispatchResult::kHandled;
                    }

                    if (normalized == "func_door")
                    {
                        RuntimeEntityRecord* record =
                            FindRuntimeRecordByEdictIndex(state, target_snapshot.edict_index);
                        if (record == nullptr)
                        {
                            if (detail != nullptr)
                            {
                                *detail = "func_door runtime record missing";
                            }
                            return hl::game_api::detail::MapLogicTargetDispatchResult::kDeferred;
                        }

                        hl::game_api::detail::BrushDoorUseRequest request;
                        request.entity = BuildBrushDoorEntityView(state, *record);
                        request.source_edict_index = context.source_edict_index;
                        request.source_label = BuildSourceEntityLabel(
                            state,
                            context.source_edict_index,
                            context.source_classname);
                        request.reason = context.reason;
                        request.frame_number = frame_number;
                        request.time = time;
                        request.frametime = frametime;
                        request.native_use_attempted = false;
                        request.native_use_succeeded = false;
                        request.native_use_detail =
                            "skipped-native-use staged-safe bootstrap brush-door path";

                        const hl::game_api::detail::BrushDoorDispatchResult door_result =
                            brush_door_controller.HandleUse(request, brush_door_hooks);
                        current_brush_door_dispatch.Merge(door_result);

                        edict_t* entity = state.edict_store.EntityOfIndex(target_snapshot.edict_index);
                        if (entity != nullptr)
                        {
                            SyncRuntimeRecordFromEdict(state, entity, *record);
                        }

                        if (door_result.handled)
                        {
                            AccumulateRuntimeRecordUseStats(
                                state,
                                target_snapshot.edict_index,
                                context.source_edict_index,
                                context.source_classname,
                                false,
                                door_result.use_succeeded,
                                false,
                                door_result.detail,
                                frame_number,
                                time);
                        }

                        if (detail != nullptr)
                        {
                            *detail = door_result.detail;
                        }

                        if (door_result.handled)
                        {
                            return hl::game_api::detail::MapLogicTargetDispatchResult::kHandled;
                        }

                        if (door_result.deferred)
                        {
                            return hl::game_api::detail::MapLogicTargetDispatchResult::kDeferred;
                        }

                        return hl::game_api::detail::MapLogicTargetDispatchResult::kFailed;
                    }

                    if (normalized == "trigger_relay")
                    {
                        RuntimeEntityRecord* record =
                            FindRuntimeRecordByEdictIndex(state, target_snapshot.edict_index);
                        if (record == nullptr)
                        {
                            if (detail != nullptr)
                            {
                                *detail = "trigger_relay runtime record missing";
                            }
                            return hl::game_api::detail::MapLogicTargetDispatchResult::kDeferred;
                        }

                        const hl::game_api::detail::EntityDefinition* definition =
                            FindParsedEntityDefinitionByOrdinal(state, record->parse_index);
                        if (definition == nullptr)
                        {
                            if (detail != nullptr)
                            {
                                *detail = "trigger_relay parsed definition missing";
                            }
                            return hl::game_api::detail::MapLogicTargetDispatchResult::kDeferred;
                        }

                        const TriggerRelayDispatchDefinition relay_dispatch =
                            CollectTriggerRelayDispatchDefinition(*definition);
                        if (!relay_dispatch.valid_delay)
                        {
                            if (detail != nullptr)
                            {
                                *detail = "trigger_relay has invalid delay value";
                            }
                            return hl::game_api::detail::MapLogicTargetDispatchResult::kDeferred;
                        }

                        const PathNodeTargetProbe target_probe =
                            ProbePathNodeTargets(state, relay_dispatch.target_name);
                        const PathNodeTargetProbe kill_probe =
                            ProbePathNodeTargets(state, relay_dispatch.kill_target_name);
                        if (!relay_dispatch.kill_target_name.empty()
                            && (kill_probe.runtime_target_candidates > 0
                                || kill_probe.parsed_target_candidates > 0))
                        {
                            if (detail != nullptr)
                            {
                                *detail =
                                    "trigger_relay killtarget="
                                    + relay_dispatch.kill_target_name
                                    + " runtimeMatches="
                                    + std::to_string(kill_probe.runtime_target_candidates)
                                    + " parsedMatches="
                                    + std::to_string(kill_probe.parsed_target_candidates)
                                    + " pending safe staged removal semantics";
                            }
                            return hl::game_api::detail::MapLogicTargetDispatchResult::kDeferred;
                        }

                        bool target_queued = false;
                        if (!relay_dispatch.target_name.empty())
                        {
                            hl::game_api::detail::ScheduledUseAction action;
                            action.fire_time =
                                time + std::max(
                                    relay_dispatch.delay,
                                    std::max(frametime, 0.05f));
                            action.source_edict_index = target_snapshot.edict_index;
                            action.source_classname = target_snapshot.classname;
                            action.target_name = relay_dispatch.target_name;
                            action.use_type = relay_dispatch.use_type;
                            action.value = context.value;
                            action.reason = "trigger_relay";
                            target_queued = map_logic_dispatcher.QueueAction(action, map_logic_hooks);
                        }

                        if (detail != nullptr)
                        {
                            *detail =
                                "trigger_relay staged bootstrap dispatch"
                                " useType=" + std::string(BootstrapUseTypeLabel(relay_dispatch.use_type))
                                + " target="
                                + (relay_dispatch.target_name.empty()
                                    ? std::string("<none>")
                                    : relay_dispatch.target_name)
                                + " targetRuntimeMatches="
                                + std::to_string(target_probe.runtime_target_candidates)
                                + " targetParsedMatches="
                                + std::to_string(target_probe.parsed_target_candidates)
                                + " targetQueued=" + BoolToYesNo(target_queued)
                                + " killtarget="
                                + (relay_dispatch.kill_target_name.empty()
                                    ? std::string("<none>")
                                    : relay_dispatch.kill_target_name)
                                + " killRuntimeMatches="
                                + std::to_string(kill_probe.runtime_target_candidates)
                                + " killParsedMatches="
                                + std::to_string(kill_probe.parsed_target_candidates)
                                + " delay=" + std::to_string(relay_dispatch.delay);
                        }
                        return hl::game_api::detail::MapLogicTargetDispatchResult::kHandled;
                    }

                    if (normalized == "trigger_changelevel")
                    {
                        RuntimeEntityRecord* record =
                            FindRuntimeRecordByEdictIndex(state, target_snapshot.edict_index);
                        const hl::game_api::detail::EntityDefinition* definition =
                            record != nullptr
                            ? FindParsedEntityDefinitionByOrdinal(state, record->parse_index)
                            : nullptr;
                        CapturePendingChangeLevelRequest(
                            state,
                            definition,
                            record,
                            frame_number,
                            time,
                            "captured as staged-safe no-op host changelevel request; no map load performed");
                        if (detail != nullptr)
                        {
                            *detail =
                                "pending_changelevel_request="
                                + FormatChangeLevelCandidate(state.changelevel_transition_state);
                        }
                        return hl::game_api::detail::MapLogicTargetDispatchResult::kHandled;
                    }

                    if (normalized != "multi_manager")
                    {
                        if (normalized == "monster_scientist"
                            || normalized == "monster_sitting_scientist")
                        {
                            if (detail != nullptr)
                            {
                                *detail = "passive recipient only";
                            }
                            return hl::game_api::detail::MapLogicTargetDispatchResult::kDeferred;
                        }

                        return hl::game_api::detail::MapLogicTargetDispatchResult::kNotHandled;
                    }

                    RuntimeEntityRecord* record =
                        FindRuntimeRecordByEdictIndex(state, target_snapshot.edict_index);
                    if (record == nullptr)
                    {
                        if (detail != nullptr)
                        {
                            *detail = "runtime record missing";
                        }
                        return hl::game_api::detail::MapLogicTargetDispatchResult::kDeferred;
                    }

                    const hl::game_api::detail::EntityDefinition* definition =
                        FindParsedEntityDefinitionByOrdinal(state, record->parse_index);
                    if (definition == nullptr)
                    {
                        if (detail != nullptr)
                        {
                            *detail = "parsed definition missing";
                        }
                        return hl::game_api::detail::MapLogicTargetDispatchResult::kDeferred;
                    }

                    const std::vector<MultiManagerOutputDefinition> outputs =
                        CollectMultiManagerOutputs(*definition);
                    if (outputs.empty())
                    {
                        if (detail != nullptr)
                        {
                            *detail = "no queued outputs";
                        }
                        return hl::game_api::detail::MapLogicTargetDispatchResult::kHandled;
                    }

                    const float base_fire_time = time + frametime;
                    int queued = 0;
                    for (const MultiManagerOutputDefinition& output : outputs)
                    {
                        if (!output.valid_delay)
                        {
                            AccumulateRuntimeRecordUseStats(
                                state,
                                target_snapshot.edict_index,
                                context.source_edict_index,
                                context.source_classname,
                                false,
                                false,
                                true,
                                "multi_manager invalid delay",
                                frame_number,
                                time);
                            continue;
                        }

                        hl::game_api::detail::ScheduledUseAction action;
                        action.fire_time = base_fire_time + std::max(0.0f, output.delay);
                        action.source_edict_index = target_snapshot.edict_index;
                        action.source_classname = target_snapshot.classname;
                        action.target_name = output.target_name;
                        action.use_type = 3;
                        action.value = 0.0f;
                        action.reason = "multi_manager";
                        map_logic_dispatcher.QueueAction(action, map_logic_hooks);
                        ++queued;
                    }

                    if (detail != nullptr)
                    {
                        *detail = "queued_outputs=" + std::to_string(queued);
                    }
                    return hl::game_api::detail::MapLogicTargetDispatchResult::kHandled;
                };

            map_logic_dispatcher.BeginFrame(frame_number, map_logic_hooks);

            hl::game_api::detail::EntityThinkSchedulerHooks think_hooks;
            think_hooks.host_frame_index =
                [host_frame_index]()
                {
                    return host_frame_index;
                };
            think_hooks.server_frame_index =
                [server_frame_index]()
                {
                    return server_frame_index;
                };
            think_hooks.global_time =
                [time]()
                {
                    return time;
                };
            think_hooks.global_frametime =
                [frametime]()
                {
                    return frametime;
                };
            think_hooks.max_entities =
                [&]()
                {
                    return state.edict_store.MaxEntities();
                };
            think_hooks.inspect_entity =
                [&](int edict_index)
                {
                    hl::game_api::detail::EntityVarSnapshot vars_snapshot;
                    edict_t* entity = state.edict_store.EntityOfIndex(edict_index);
                    if (!hl::game_api::detail::TryReadEntityVars(
                            state.edict_store,
                            state.string_pool,
                            entity,
                            &vars_snapshot,
                            [&](std::string_view message)
                            {
                                hl::common::Logger::Warn(
                                    hl::common::LogCategory::Think,
                                    std::string(message));
                            }))
                    {
                        return hl::game_api::detail::EntityVarSnapshot{};
                    }

                    if (vars_snapshot.in_use
                        || vars_snapshot.removed
                        || vars_snapshot.has_private_data
                        || !vars_snapshot.classname.empty()
                        || vars_snapshot.scheduled_for_think)
                    {
                        if (RuntimeEntityRecord* record =
                                EnsureRuntimeRecordForEntity(state, entity, "think-scan");
                            record != nullptr)
                        {
                            SyncRuntimeRecordFromEdict(state, entity, *record);
                        }
                    }

                    return vars_snapshot;
                };
            think_hooks.dispatch_think =
                [&](int edict_index, int think_frame_number, unsigned int* seh_code)
                {
                    edict_t* entity = state.edict_store.EntityOfIndex(edict_index);
                    if (entity == nullptr || state.dll_functions.pfnThink == nullptr)
                    {
                        return false;
                    }

                    RuntimeEntityRecord* record =
                        EnsureRuntimeRecordForEntity(state, entity, "think-dispatch");
                    if (record != nullptr)
                    {
                        SyncRuntimeRecordFromEdict(state, entity, *record);
                    }

                    hl::game_api::detail::EntityVarSnapshot vars_snapshot;
                    hl::game_api::detail::TryReadEntityVars(
                        state.edict_store,
                        state.string_pool,
                        entity,
                        &vars_snapshot,
                        [&](std::string_view message)
                        {
                            hl::common::Logger::Warn(
                                hl::common::LogCategory::Think,
                                std::string(message));
                        });

                    const bool engine_managed_trigger_auto = IsEngineManagedTriggerAuto(vars_snapshot);
                    hl::game_api::detail::TryPrepareThinkDispatch(
                        state.edict_store,
                        entity,
                        time,
                        [&](std::string_view message)
                        {
                            hl::common::Logger::Warn(
                                hl::common::LogCategory::Think,
                                std::string(message));
                        });

                    state.entity_think_diagnostics.SetCurrentEntityContext(
                        edict_index,
                        vars_snapshot.classname);
                    state.entity_think_diagnostics.RecordInternalEvent(
                        "think-dispatch",
                        "nextthink=" + std::to_string(vars_snapshot.nextthink)
                            + ", now=" + std::to_string(time),
                        edict_index,
                        vars_snapshot.classname);

                    bool succeeded = false;
                    if (engine_managed_trigger_auto)
                    {
                        state.entity_think_diagnostics.RecordInternalEvent(
                            "think-engine-managed",
                            "trigger_auto emits use-target chain",
                            edict_index,
                            vars_snapshot.classname);

                        RuntimeEntityRecord* runtime_record =
                            FindRuntimeRecordByEdictIndex(state, edict_index);
                        const hl::game_api::detail::EntityDefinition* definition =
                            runtime_record != nullptr
                            ? FindParsedEntityDefinitionByOrdinal(state, runtime_record->parse_index)
                            : nullptr;
                        if (definition == nullptr)
                        {
                            hl::common::Logger::Warn(
                                hl::common::LogCategory::Use,
                                "MapLogicDispatcher: trigger_auto edict#"
                                + std::to_string(edict_index)
                                + " has no parsed definition; skipping.");
                            succeeded = true;
                        }
                        else
                        {
                            const std::string* global_state = FindLastKeyValue(*definition, "globalstate");
                            if (global_state != nullptr && !global_state->empty())
                            {
                                hl::common::Logger::Warn(
                                    hl::common::LogCategory::Use,
                                    "MapLogicDispatcher: trigger_auto edict#"
                                    + std::to_string(edict_index)
                                    + " deferred because globalstate support is not ready.");
                                AccumulateRuntimeRecordUseStats(
                                    state,
                                    edict_index,
                                    edict_index,
                                    vars_snapshot.classname,
                                    false,
                                    false,
                                    true,
                                    "trigger_auto globalstate not supported",
                                    frame_number,
                                    time);
                                succeeded = true;
                            }
                            else
                            {
                                const int use_type = ParseTriggerStateUseType(*definition);
                                float delay = 0.0f;
                                if (const std::string* raw_delay = FindLastKeyValue(*definition, "delay");
                                    raw_delay != nullptr && !raw_delay->empty())
                                {
                                    ParseStrictFloat(*raw_delay, &delay);
                                }

                                if (!vars_snapshot.target.empty())
                                {
                                    hl::game_api::detail::MapLogicDispatchContext dispatch_context;
                                    dispatch_context.frame_number = frame_number;
                                    dispatch_context.source_edict_index = edict_index;
                                    dispatch_context.source_classname = vars_snapshot.classname;
                                    dispatch_context.target_name = vars_snapshot.target;
                                    dispatch_context.use_type = use_type;
                                    dispatch_context.value = 0.0f;
                                    dispatch_context.reason = "trigger_auto";

                                    if (delay > 0.0f)
                                    {
                                        hl::game_api::detail::ScheduledUseAction action;
                                        action.fire_time = time + delay;
                                        action.source_edict_index = edict_index;
                                        action.source_classname = vars_snapshot.classname;
                                        action.target_name = vars_snapshot.target;
                                        action.use_type = use_type;
                                        action.value = 0.0f;
                                        action.reason = "trigger_auto-delay";
                                        map_logic_dispatcher.QueueAction(action, map_logic_hooks);
                                    }
                                    else
                                    {
                                        map_logic_dispatcher.DispatchTargetChain(
                                            dispatch_context,
                                            map_logic_hooks);
                                    }
                                }
                                else
                                {
                                    hl::common::Logger::Info(
                                        hl::common::LogCategory::Use,
                                        "MapLogicDispatcher: trigger_auto edict#"
                                        + std::to_string(edict_index)
                                        + " has no target.");
                                }

                                if ((static_cast<int>(entity->v.spawnflags) & 0x0001) != 0)
                                {
                                    state.edict_store.RemoveEntity(entity);
                                    if (runtime_record != nullptr)
                                    {
                                        SyncRuntimeRecordFromEdict(state, entity, *runtime_record);
                                        runtime_record->removed = true;
                                        runtime_record->lifecycle_state =
                                            RuntimeEntityLifecycleState::kRemovedByGameLogic;
                                        RefreshRuntimeMapLogicFlags(*runtime_record);
                                    }
                                }
                                succeeded = true;
                            }
                        }
                    }
                    else
                    {
                        succeeded =
                            SafeCallThinkSeh(state.dll_functions.pfnThink, entity, seh_code);
                    }

                    if (!succeeded && seh_code != nullptr)
                    {
                        state.entity_think_diagnostics.MarkEntitySeh(*seh_code);
                    }

                    state.entity_think_diagnostics.ClearCurrentEntityContext();
                    MarkRuntimeRecordThinkAttempt(
                        state,
                        edict_index,
                        think_frame_number,
                        succeeded);
                    return succeeded;
                };
            think_hooks.defer_think =
                [&](int edict_index,
                    int think_frame_number,
                    float deferred_nextthink,
                    bool write_nextthink,
                    std::string_view reason)
                {
                    edict_t* entity = state.edict_store.EntityOfIndex(edict_index);
                    RuntimeEntityRecord* record = nullptr;
                    if (entity != nullptr)
                    {
                        record = EnsureRuntimeRecordForEntity(state, entity, "think-deferred");
                        if (write_nextthink)
                        {
                            hl::game_api::detail::TryWriteNextThink(
                                state.edict_store,
                                entity,
                                deferred_nextthink,
                                [&](std::string_view message)
                                {
                                    hl::common::Logger::Warn(std::string(message));
                                });
                        }

                        if (record != nullptr)
                        {
                            SyncRuntimeRecordFromEdict(state, entity, *record);
                        }
                    }

                    MarkRuntimeRecordThinkDeferred(
                        state,
                        edict_index,
                        think_frame_number,
                        reason);

                    std::string classname;
                    if (record != nullptr)
                    {
                        classname = record->classname;
                    }
                    state.entity_think_diagnostics.RecordInternalEvent(
                        "think-deferred",
                        std::string(reason),
                        edict_index,
                        classname);
                };
            think_hooks.sweep_removed_entities =
                [&]()
                {
                    SweepKilledEntities(state);
                };
            think_hooks.trace_tail =
                [&](std::size_t limit)
                {
                    std::vector<std::string> lines;
                    for (const hl::game_api::detail::SpawnTraceEvent& event : TakeTraceTail(
                             state.entity_think_diagnostics.TraceSnapshot(),
                             limit))
                    {
                        lines.push_back(FormatSpawnTraceEvent(event));
                    }

                    return lines;
                };
            think_hooks.trace_event_count =
                [&]()
                {
                    return state.entity_think_diagnostics.EventCount();
                };
            think_hooks.total_callback_counts =
                [&]()
                {
                    return state.callback_counts;
                };
            think_hooks.log_info =
                [&](std::string_view message)
                {
                    LogSubsystemInfo(
                        hl::common::LogCategory::Think,
                        state.frame_bootstrap_options.trace_think,
                        message);
                };
            think_hooks.log_warn =
                [](std::string_view message)
                {
                    hl::common::Logger::Warn(
                        hl::common::LogCategory::Think,
                        std::string(message));
                };
            think_hooks.log_error =
                [](std::string_view message)
                {
                    hl::common::Logger::Error(
                        hl::common::LogCategory::Think,
                        std::string(message));
                };

            think_scheduler.RunFrame(frame_number, think_hooks);
            map_logic_dispatcher.ProcessDueQueue(map_logic_hooks);
            state.entity_think_scheduler_state = think_scheduler.Summary();

            if (const hl::game_api::MapLogicFrameStateSummary* dispatcher_frame =
                    map_logic_dispatcher.CurrentFrameSummary();
                dispatcher_frame != nullptr)
            {
                movement_frame_context.delayed_actions_due = dispatcher_frame->scheduled_due;
                movement_frame_context.delayed_actions_executed =
                    dispatcher_frame->scheduled_executed;
                movement_frame_context.delayed_actions_pending =
                    dispatcher_frame->scheduled_pending;
            }

            track_path_resolver.Rebuild(
                BuildTrackPathInputs(state),
                [&](std::string_view message)
                {
                    if (state.frame_bootstrap_options.trace_movement)
                    {
                        hl::common::Logger::Info(
                            hl::common::LogCategory::Path,
                            std::string(message));
                    }
                },
                [&](std::string_view message)
                {
                    if (state.frame_bootstrap_options.trace_movement || frame_number == 1)
                    {
                        hl::common::Logger::Warn(
                            hl::common::LogCategory::Path,
                            std::string(message));
                    }
                });

            hl::game_api::detail::ScriptedMovementControllerHooks movement_hooks;
            movement_hooks.entity_by_index =
                [&](int edict_index)
                {
                    return state.edict_store.EntityOfIndex(edict_index);
                };
            movement_hooks.set_origin =
                [&](edict_t* entity, const Vector& origin)
                {
                    float values[3] = {origin.x, origin.y, origin.z};
                    movement_callbacks_exercised.insert("pfnSetOrigin");
                    StubSetOrigin(entity, values);
                };
            movement_hooks.set_angles =
                [&](edict_t* entity, const Vector& angles)
                {
                    if (entity == nullptr || state.edict_store.IndexOf(entity) < 0)
                    {
                        return;
                    }
                    state.edict_store.SetAngles(entity, angles, FormatVector(angles));
                    ComputeFallbackAbsBox(entity->v);
                };
            movement_hooks.set_velocity =
                [&](edict_t* entity, const Vector& velocity)
                {
                    if (entity == nullptr)
                    {
                        return;
                    }
                    entity->v.velocity = velocity;
                };
            movement_hooks.set_avelocity =
                [&](edict_t* entity, const Vector& velocity)
                {
                    if (entity == nullptr)
                    {
                        return;
                    }
                    entity->v.avelocity = velocity;
                };
            movement_hooks.walk_move =
                [&](edict_t* entity, float yaw, float distance, int mode)
                {
                    movement_callbacks_exercised.insert("pfnWalkMove");
                    return StubWalkMove(entity, yaw, distance, mode);
                };
            movement_hooks.change_yaw =
                [&](edict_t* entity)
                {
                    movement_callbacks_exercised.insert("pfnChangeYaw");
                    StubChangeYaw(entity);
                };
            movement_hooks.change_pitch =
                [&](edict_t* entity)
                {
                    movement_callbacks_exercised.insert("pfnChangePitch");
                    StubChangePitch(entity);
                };
            movement_hooks.log_info =
                [&](std::string_view message)
                {
                    LogSubsystemInfo(
                        ClassifyMovementMessageCategory(message),
                        state.frame_bootstrap_options.trace_movement,
                        message);
                };
            movement_hooks.log_warn =
                [](std::string_view message)
                {
                    hl::common::Logger::Warn(
                        hl::common::LogCategory::Scripted,
                        std::string(message));
                };

            const std::vector<hl::game_api::detail::ScriptedMovementEntityView> movement_entities =
                BuildScriptedMovementEntityViews(state);
            movement_controller.RunFrame(
                movement_frame_context,
                track_path_resolver,
                movement_entities,
                movement_hooks);

            hl::game_api::detail::PathMoverControllerHooks path_mover_hooks;
            path_mover_hooks.entity_by_index =
                [&](int edict_index)
                {
                    return state.edict_store.EntityOfIndex(edict_index);
                };
            path_mover_hooks.set_origin =
                [&](edict_t* entity, const Vector& origin)
                {
                    float values[3] = {origin.x, origin.y, origin.z};
                    path_mover_callbacks_exercised.insert("pfnSetOrigin");
                    StubSetOrigin(entity, values);
                };
            path_mover_hooks.set_angles =
                [&](edict_t* entity, const Vector& angles)
                {
                    if (entity == nullptr || state.edict_store.IndexOf(entity) < 0)
                    {
                        return;
                    }
                    state.edict_store.SetAngles(entity, angles, FormatVector(angles));
                    ComputeFallbackAbsBox(entity->v);
                };
            path_mover_hooks.set_velocity =
                [&](edict_t* entity, const Vector& velocity)
                {
                    if (entity == nullptr)
                    {
                        return;
                    }
                    entity->v.velocity = velocity;
                };
            path_mover_hooks.set_avelocity =
                [&](edict_t* entity, const Vector& velocity)
                {
                    if (entity == nullptr)
                    {
                        return;
                    }
                    entity->v.avelocity = velocity;
                };
            path_mover_hooks.change_yaw =
                [&](edict_t* entity)
                {
                    path_mover_callbacks_exercised.insert("pfnChangeYaw");
                    StubChangeYaw(entity);
                };
            path_mover_hooks.log_info =
                [&](std::string_view message)
                {
                    LogSubsystemInfo(
                        ClassifyMovementMessageCategory(message),
                        state.frame_bootstrap_options.trace_movement,
                        message);
                };
            path_mover_hooks.log_warn =
                [](std::string_view message)
                {
                    hl::common::Logger::Warn(
                        hl::common::LogCategory::Path,
                        std::string(message));
                };
            path_mover_hooks.dispatch_target_event =
                [&](const hl::game_api::detail::PathNodeEventDispatchRequest& request)
                {
                    std::unordered_map<std::string, std::size_t> callbacks_before =
                        state.callback_counts;
                    current_brush_door_dispatch = {};
                    const PathNodeTargetProbe target_probe =
                        ProbePathNodeTargets(state, request.message);
                    const PathNodeDownstreamSnapshot downstream_before =
                        CapturePathNodeDownstreamSnapshot(
                            state,
                            map_logic_dispatcher,
                            std::unordered_map<std::string, std::size_t>{},
                            &brush_door_controller);
                    const hl::game_api::MapLogicFrameStateSummary* before_frame =
                        map_logic_dispatcher.CurrentFrameSummary();
                    const int before_use_attempts =
                        before_frame != nullptr ? before_frame->use_attempts : 0;
                    const int before_target_resolutions =
                        before_frame != nullptr ? before_frame->target_resolutions : 0;
                    const int before_use_successes =
                        before_frame != nullptr ? before_frame->use_successes : 0;
                    const int before_use_deferred =
                        before_frame != nullptr ? before_frame->use_deferred : 0;
                    const int before_use_failures =
                        before_frame != nullptr ? before_frame->use_failures : 0;
                    const int before_no_targets =
                        static_cast<int>(map_logic_dispatcher.Summary().total_no_targets_found);

                    hl::game_api::detail::MapLogicDispatchContext dispatch_context;
                    dispatch_context.frame_number = request.frame_number;
                    dispatch_context.source_edict_index = request.mover_edict_index;
                    dispatch_context.source_classname = request.mover_classname;
                    dispatch_context.target_name = request.message;
                    dispatch_context.use_type = 3;
                    dispatch_context.value = 0.0f;
                    dispatch_context.reason =
                        "bootstrap-path-node-event node="
                        + (request.node_name.empty()
                            ? std::string("<empty>")
                            : request.node_name);

                    const bool known_presentation_message =
                        IsKnownPresentationPathNodeMessage(request.message);
                    const PathNodePresentationLinkageProbe presentation_linkage_probe =
                        known_presentation_message
                        ? ProbePathNodePresentationLinkage(state, request.message)
                        : PathNodePresentationLinkageProbe{};
                    const bool known_parsed_only_message =
                        IsKnownDeferredPathNodeMessage(request.message)
                        && target_probe.runtime_target_candidates == 0
                        && target_probe.parsed_target_candidates > 0;
                    const bool skip_runtime_dispatch =
                        known_parsed_only_message || known_presentation_message;
                    const std::string runtime_dispatch_mode =
                        known_parsed_only_message
                        ? "skip-runtime-known-parsed-only"
                        : known_presentation_message
                        ? "presentation-bootstrap"
                        : "runtime-dispatch";
                    const bool dispatched = skip_runtime_dispatch
                        ? false
                        : map_logic_dispatcher.DispatchTargetChain(
                            dispatch_context,
                            map_logic_hooks);
                    const ParsedTriggerRelayBootstrapDispatchResult parsed_trigger_relay_bootstrap =
                        !known_presentation_message
                            && target_probe.runtime_target_candidates == 0
                            && target_probe.parsed_target_candidates > 0
                        ? TryDispatchParsedOnlyTriggerRelayBootstrap(
                            state,
                            request,
                            time,
                            frametime,
                            map_logic_dispatcher,
                            map_logic_hooks)
                        : ParsedTriggerRelayBootstrapDispatchResult{};
                    const PathNodePresentationDispatchResult presentation_dispatch =
                        known_presentation_message
                        ? DispatchKnownPathNodePresentationBootstrap(
                            state,
                            request,
                            dispatch_context,
                            presentation_linkage_probe,
                            map_logic_dispatcher,
                            map_logic_hooks)
                        : PathNodePresentationDispatchResult{};
                    const hl::game_api::MapLogicFrameStateSummary* after_frame =
                        map_logic_dispatcher.CurrentFrameSummary();
                    const int after_use_attempts =
                        after_frame != nullptr ? after_frame->use_attempts : before_use_attempts;
                    const int after_target_resolutions =
                        after_frame != nullptr ? after_frame->target_resolutions : before_target_resolutions;
                    const int after_use_successes =
                        after_frame != nullptr ? after_frame->use_successes : before_use_successes;
                    const int after_use_deferred =
                        after_frame != nullptr ? after_frame->use_deferred : before_use_deferred;
                    const int after_use_failures =
                        after_frame != nullptr ? after_frame->use_failures : before_use_failures;
                    const int after_no_targets =
                        static_cast<int>(map_logic_dispatcher.Summary().total_no_targets_found);
                    const std::vector<hl::game_api::InvokedEngineCallback> callback_delta =
                        BuildCallbackDelta(callbacks_before, state.callback_counts);
                    std::unordered_map<std::string, std::size_t> callback_delta_map;
                    for (const hl::game_api::InvokedEngineCallback& callback : callback_delta)
                    {
                        path_mover_callbacks_exercised.insert(callback.name);
                        callback_delta_map[callback.name] += callback.call_count;
                    }
                    const PathNodeDownstreamSnapshot downstream_after =
                        CapturePathNodeDownstreamSnapshot(
                            state,
                            map_logic_dispatcher,
                            callback_delta_map,
                            &brush_door_controller);
                    const PathNodeTrackedChangeSummary tracked_changes =
                        SummarizePathNodeTrackedEntityChanges(
                            downstream_before,
                            downstream_after);

                    const int use_attempt_delta = after_use_attempts - before_use_attempts;
                    const int resolved_delta = after_target_resolutions - before_target_resolutions;
                    const int success_delta = after_use_successes - before_use_successes;
                    const int deferred_delta = after_use_deferred - before_use_deferred;
                    const int failure_delta = after_use_failures - before_use_failures;
                    const int no_target_delta = after_no_targets - before_no_targets;
                    const int scheduled_created_delta =
                        downstream_after.scheduled_created - downstream_before.scheduled_created;
                    const int scheduled_executed_delta =
                        downstream_after.scheduled_executed - downstream_before.scheduled_executed;
                    const bool handled_without_runtime_use =
                        resolved_delta > 0
                        && success_delta == 0
                        && deferred_delta == 0
                        && failure_delta == 0
                        && no_target_delta == 0;
                    const bool handled_without_use =
                        handled_without_runtime_use
                        || parsed_trigger_relay_bootstrap.handled;

                    hl::game_api::detail::PathNodeEventDispatchFeedback feedback;
                    feedback.attempted = true;
                    feedback.runtime_target_candidates = target_probe.runtime_target_candidates;
                    feedback.parsed_target_candidates = target_probe.parsed_target_candidates;
                    feedback.target_classnames = target_probe.target_classnames;
                    feedback.resolved_target_details = target_probe.resolved_target_details;
                    feedback.resolved_targets =
                        std::max(0, resolved_delta)
                        + parsed_trigger_relay_bootstrap.synthetic_resolved_targets;
                    feedback.successful_targets =
                        std::max(0, success_delta)
                        + (handled_without_runtime_use ? std::max(0, resolved_delta) : 0)
                        + parsed_trigger_relay_bootstrap.synthetic_resolved_targets;
                    feedback.deferred_targets =
                        std::max(0, deferred_delta)
                        + (parsed_trigger_relay_bootstrap.deferred ? 1 : 0);
                    feedback.failed_targets =
                        std::max(0, failure_delta)
                        + (parsed_trigger_relay_bootstrap.failed ? 1 : 0);
                    feedback.pfn_use_attempted =
                        use_attempt_delta > 0 || current_brush_door_dispatch.native_use_attempted;
                    feedback.unresolved_target =
                        (no_target_delta > 0 && !parsed_trigger_relay_bootstrap.attempted)
                        || (!dispatched
                            && !parsed_trigger_relay_bootstrap.attempted
                            && resolved_delta <= 0
                            && success_delta <= 0
                            && deferred_delta <= 0
                            && failure_delta <= 0);
                    feedback.multi_manager_activity = tracked_changes.multi_manager_activity;
                    feedback.scripted_sequence_activity = tracked_changes.scripted_sequence_activity;
                    feedback.actor_state_changes = tracked_changes.actor_state_changes;
                    feedback.path_state_changes = tracked_changes.path_state_changes;
                    feedback.alert_callbacks =
                        downstream_after.alert_callbacks - downstream_before.alert_callbacks;
                    feedback.message_callbacks =
                        downstream_after.message_callbacks - downstream_before.message_callbacks;
                    feedback.downstream_target_chains =
                        downstream_after.target_chains_fired - downstream_before.target_chains_fired;
                    feedback.downstream_scheduled_actions =
                        (downstream_after.scheduled_created - downstream_before.scheduled_created)
                        + (downstream_after.scheduled_executed - downstream_before.scheduled_executed);
                    feedback.brush_door_handling_attempted =
                        current_brush_door_dispatch.staged_handling_attempted;
                    feedback.brush_door_use_succeeded =
                        current_brush_door_dispatch.use_succeeded
                        || tracked_changes.brush_door_use_succeeded;
                    feedback.brush_door_state_changed =
                        current_brush_door_dispatch.state_changed
                        || tracked_changes.brush_door_state_changed;
                    feedback.brush_door_movement_started =
                        current_brush_door_dispatch.movement_started
                        || tracked_changes.brush_door_movement_started;
                    feedback.brush_door_movement_completed =
                        current_brush_door_dispatch.movement_completed
                        || tracked_changes.brush_door_movement_completed;
                    feedback.brush_door_native_use_attempted =
                        current_brush_door_dispatch.native_use_attempted;
                    feedback.brush_door_native_use_succeeded =
                        current_brush_door_dispatch.native_use_succeeded;
                    feedback.brush_door_dispatch_path =
                        current_brush_door_dispatch.dispatch_path;
                    feedback.brush_door_support_state =
                        current_brush_door_dispatch.support_state;
                    feedback.brush_door_state =
                        current_brush_door_dispatch.door_state;
                    feedback.brush_door_blocked_reason =
                        current_brush_door_dispatch.blocked_reason;
                    feedback.brush_door_runtime_audit =
                        current_brush_door_dispatch.runtime_audit;
                    feedback.dispatch_mode = presentation_dispatch.dispatch_mode;
                    feedback.fade_channel_available = presentation_dispatch.fade_channel_available;
                    feedback.fade_channel_used = presentation_dispatch.fade_channel_used;
                    feedback.message_channel_available =
                        presentation_dispatch.message_channel_available;
                    feedback.message_channel_used = presentation_dispatch.message_channel_used;
                    feedback.env_message_linkage_found =
                        presentation_dispatch.env_message_linkage_found;
                    feedback.env_message_linkage_used =
                        presentation_dispatch.env_message_linkage_used;
                    feedback.summary_only_fallback_used =
                        presentation_dispatch.summary_only_fallback_used;
                    feedback.presentation_linkage_detail =
                        presentation_dispatch.presentation_linkage_detail;
                    feedback.presentation_semantics_summary =
                        presentation_dispatch.semantics_summary;
                    if (known_presentation_message)
                    {
                        feedback.unresolved_target = false;
                        if (presentation_dispatch.dispatched)
                        {
                            feedback.resolved_targets = std::max(feedback.resolved_targets, 1);
                            feedback.successful_targets = std::max(feedback.successful_targets, 1);
                            if (feedback.resolved_target_details.size() < 8
                                && !presentation_dispatch.detail.empty())
                            {
                                feedback.resolved_target_details.push_back(
                                    presentation_dispatch.detail);
                            }
                            if (presentation_dispatch.fade_channel_used)
                            {
                                AppendUniqueValue(
                                    feedback.target_classnames,
                                    "bootstrap_screenfade");
                            }
                            else if (presentation_dispatch.message_channel_used)
                            {
                                AppendUniqueValue(
                                    feedback.target_classnames,
                                    "bootstrap_message_channel");
                            }
                            else if (presentation_dispatch.summary_only_fallback_used)
                            {
                                AppendUniqueValue(
                                    feedback.target_classnames,
                                    "bootstrap_summary_sink");
                            }
                        }
                    }

                    if (feedback.successful_targets > 0)
                    {
                        feedback.outcome = hl::game_api::detail::PathNodeEventDispatchOutcome::kSucceeded;
                    }
                    else if (feedback.deferred_targets > 0)
                    {
                        feedback.outcome = hl::game_api::detail::PathNodeEventDispatchOutcome::kDeferred;
                    }
                    else if (feedback.failed_targets > 0)
                    {
                        feedback.outcome = hl::game_api::detail::PathNodeEventDispatchOutcome::kFailed;
                    }
                    else if (feedback.unresolved_target)
                    {
                        feedback.outcome =
                            hl::game_api::detail::PathNodeEventDispatchOutcome::kUnresolvedTarget;
                    }
                    else
                    {
                        feedback.outcome = handled_without_use
                            ? hl::game_api::detail::PathNodeEventDispatchOutcome::kSucceeded
                            : hl::game_api::detail::PathNodeEventDispatchOutcome::kDeferred;
                    }

                    feedback.visible_downstream_progression =
                        HasVisiblePathNodeDownstreamProgression(
                            downstream_before,
                            downstream_after)
                        || !tracked_changes.details.empty();
                    feedback.downstream_summary =
                        BuildPathNodeDownstreamSummary(
                            downstream_before,
                            downstream_after,
                            callback_delta);

                    if (EqualsIgnoreCase(request.message, "room2train")
                        && target_probe.parsed_target_candidates > 0
                        && target_probe.runtime_target_candidates == 0)
                    {
                        feedback.outcome =
                            hl::game_api::detail::PathNodeEventDispatchOutcome::kUnresolvedTarget;
                        feedback.unresolved_target = true;
                        feedback.classification_hint = "encountered-unresolved-target";
                    }
                    else if (IsKnownDeferredPathNodeMessage(request.message)
                        && target_probe.runtime_target_candidates == 0
                        && feedback.resolved_targets == 0
                        && !feedback.pfn_use_attempted)
                    {
                        feedback.outcome =
                            hl::game_api::detail::PathNodeEventDispatchOutcome::kDeferred;
                        feedback.unresolved_target = false;
                        feedback.classification_hint = "encountered-deferred";
                        feedback.required_subsystem =
                            !parsed_trigger_relay_bootstrap.required_subsystem.empty()
                            ? parsed_trigger_relay_bootstrap.required_subsystem
                            : "broader scripted actor/bootstrap progression semantics";
                    }
                    else if (known_presentation_message)
                    {
                        feedback.outcome = presentation_dispatch.dispatched
                            ? hl::game_api::detail::PathNodeEventDispatchOutcome::kSucceeded
                            : presentation_dispatch.deferred
                            ? hl::game_api::detail::PathNodeEventDispatchOutcome::kDeferred
                            : feedback.outcome;
                        feedback.unresolved_target = false;
                        feedback.classification_hint = !presentation_dispatch.classification.empty()
                            ? presentation_dispatch.classification
                            : "summary-only-staged-fallback";
                        feedback.required_subsystem = presentation_dispatch.required_subsystem;
                    }
                    else if (EqualsIgnoreCase(request.message, "room2train")
                        && target_probe.runtime_target_candidates > 0
                        && feedback.outcome
                            == hl::game_api::detail::PathNodeEventDispatchOutcome::kDeferred
                        && !feedback.pfn_use_attempted)
                    {
                        feedback.classification_hint = "encountered-deferred";
                        feedback.required_subsystem =
                            "safe brush door use/bootstrap movement callbacks";
                    }

                    if (!parsed_trigger_relay_bootstrap.required_subsystem.empty()
                        && feedback.required_subsystem.empty())
                    {
                        feedback.required_subsystem =
                            parsed_trigger_relay_bootstrap.required_subsystem;
                    }

                    if (feedback.downstream_summary.empty())
                    {
                        feedback.downstream_summary = "none";
                    }
                    if (!feedback.required_subsystem.empty())
                    {
                        if (feedback.downstream_summary == "none")
                        {
                            feedback.downstream_summary =
                                "blockedOn=" + feedback.required_subsystem;
                        }
                        else if (feedback.downstream_summary.find("blockedOn=") == std::string::npos)
                        {
                            feedback.downstream_summary +=
                                ", blockedOn=" + feedback.required_subsystem;
                        }
                    }

                    feedback.detail =
                        "staged bootstrap path-node dispatch node="
                        + (request.node_name.empty()
                            ? std::string("<empty>")
                            : request.node_name)
                        + " target="
                        + (request.message.empty()
                            ? std::string("<empty>")
                            : request.message)
                        + " nodeSpeed=" + std::to_string(request.node_speed)
                        + " speedBefore=" + std::to_string(request.speed_before_arrival)
                        + " speedAfter=" + std::to_string(request.speed_after_arrival)
                        + " speedChanged="
                        + (request.speed_changed_on_arrival ? "yes" : "no")
                        + " currentSpeed=" + std::to_string(request.current_speed)
                        + " speedDecision="
                        + (request.speed_decision.empty()
                            ? std::string("<unset>")
                            : request.speed_decision)
                        + " runtimeDispatchMode=" + runtime_dispatch_mode
                        + " dispatched=" + (dispatched ? "yes" : "no")
                        + " presentationDispatchMode="
                        + (feedback.dispatch_mode.empty()
                            ? std::string("<none>")
                            : feedback.dispatch_mode)
                        + " presentationDispatched="
                        + BoolToYesNo(presentation_dispatch.dispatched)
                        + " runtimeTargets="
                        + std::to_string(target_probe.runtime_target_candidates)
                        + " parsedTargets="
                        + std::to_string(target_probe.parsed_target_candidates)
                        + " fadeChannelAvailable="
                        + BoolToYesNo(feedback.fade_channel_available)
                        + " fadeChannelUsed=" + BoolToYesNo(feedback.fade_channel_used)
                        + " messageChannelAvailable="
                        + BoolToYesNo(feedback.message_channel_available)
                        + " messageChannelUsed=" + BoolToYesNo(feedback.message_channel_used)
                        + " envMessageLinkageFound="
                        + BoolToYesNo(feedback.env_message_linkage_found)
                        + " envMessageLinkageUsed="
                        + BoolToYesNo(feedback.env_message_linkage_used)
                        + " summaryFallbackUsed="
                        + BoolToYesNo(feedback.summary_only_fallback_used)
                        + " presentationLinkage="
                        + (feedback.presentation_linkage_detail.empty()
                            ? std::string("<none>")
                            : feedback.presentation_linkage_detail)
                        + " resolvedDelta=" + std::to_string(std::max(0, resolved_delta))
                        + " useAttemptDelta=" + std::to_string(std::max(0, use_attempt_delta))
                        + " successDelta=" + std::to_string(std::max(0, success_delta))
                        + " deferredDelta=" + std::to_string(std::max(0, deferred_delta))
                        + " failureDelta=" + std::to_string(std::max(0, failure_delta))
                        + " scheduledCreatedDelta="
                        + std::to_string(std::max(0, scheduled_created_delta))
                        + " scheduledExecutedDelta="
                        + std::to_string(std::max(0, scheduled_executed_delta))
                        + " noTargetDelta=" + std::to_string(std::max(0, no_target_delta))
                        + " targetClassnames=" + JoinStringValues(target_probe.target_classnames)
                        + " resolvedTargetDetails="
                        + JoinStringValues(target_probe.resolved_target_details)
                        + " multiManagerActivity="
                        + std::to_string(feedback.multi_manager_activity)
                        + " scriptedSequenceActivity="
                        + std::to_string(feedback.scripted_sequence_activity)
                        + " actorStateChanges="
                        + std::to_string(feedback.actor_state_changes)
                        + " pathStateChanges="
                        + std::to_string(feedback.path_state_changes)
                        + " brushDoorAttempted="
                        + BoolToYesNo(feedback.brush_door_handling_attempted)
                        + " brushDoorPath="
                        + (feedback.brush_door_dispatch_path.empty()
                            ? std::string("<none>")
                            : feedback.brush_door_dispatch_path)
                        + " brushDoorSupport="
                        + (feedback.brush_door_support_state.empty()
                            ? std::string("<none>")
                            : feedback.brush_door_support_state)
                        + " brushDoorState="
                        + (feedback.brush_door_state.empty()
                            ? std::string("<none>")
                            : feedback.brush_door_state)
                        + " brushDoorStarted="
                        + BoolToYesNo(feedback.brush_door_movement_started)
                        + " brushDoorCompleted="
                        + BoolToYesNo(feedback.brush_door_movement_completed)
                        + " alertCallbacks="
                        + std::to_string(std::max(0, feedback.alert_callbacks))
                        + " messageCallbacks="
                        + std::to_string(std::max(0, feedback.message_callbacks))
                        + " targetChainDelta="
                        + std::to_string(std::max(0, feedback.downstream_target_chains))
                        + " scheduledActionDelta="
                        + std::to_string(std::max(0, feedback.downstream_scheduled_actions))
                        + " downstream="
                        + (feedback.downstream_summary.empty()
                            ? std::string("<none>")
                            : feedback.downstream_summary)
                        + (handled_without_use ? " handledWithoutUse=yes" : std::string());

                    if (parsed_trigger_relay_bootstrap.attempted)
                    {
                        feedback.detail +=
                            " parsedBootstrap={" + parsed_trigger_relay_bootstrap.detail + "}";
                    }
                    if (!feedback.required_subsystem.empty())
                    {
                        feedback.detail +=
                            " requiredSubsystem=" + feedback.required_subsystem;
                    }
                    if (!presentation_dispatch.detail.empty())
                    {
                        feedback.detail +=
                            " presentationDetail={" + presentation_dispatch.detail + "}";
                    }
                    if (!feedback.brush_door_runtime_audit.empty())
                    {
                        feedback.detail +=
                            " brushDoorAudit={" + feedback.brush_door_runtime_audit + "}";
                    }

                    if (EqualsIgnoreCase(request.message, "room2train")
                        && target_probe.parsed_target_candidates > 0
                        && target_probe.runtime_target_candidates == 0)
                    {
                        feedback.detail +=
                            " note=parsed target exists but no live runtime entity was resolved";
                    }
                    else if (IsKnownDeferredPathNodeMessage(request.message)
                        && target_probe.runtime_target_candidates == 0)
                    {
                        feedback.detail += parsed_trigger_relay_bootstrap.handled
                            ? " note=execute_sci parsed trigger_relay bootstrap dispatch ran from parsed-only entity state; no downstream scripted actor progression was observed"
                            : " note=execute_sci currently surfaced as a staged scripted cue; broader actor/script progression semantics are still pending";
                    }
                    else if (known_presentation_message)
                    {
                        feedback.detail +=
                            presentation_dispatch.fade_channel_used
                            ? " note=fade_out emitted through staged ScreenFade bootstrap; no full client rendering/runtime was required"
                            : presentation_dispatch.message_channel_used
                            ? " note=fade_out emitted through staged message-channel bootstrap; no full HUD/UI runtime was required"
                            : presentation_dispatch.env_message_linkage_used
                            ? " note=fade_out used staged env_message linkage after channel probes"
                            : presentation_dispatch.summary_only_fallback_used
                            ? " note=fade_out was captured by a staged summary-only presentation sink"
                            : " note=fade_out remains on a concrete staged presentation path, but a narrower env_message/runtime link is still missing";
                    }

                    return feedback;
                };

            path_mover_controller.RunFrame(
                movement_frame_context,
                track_path_resolver,
                movement_entities,
                path_mover_hooks);

            brush_door_controller.RunFrame(
                movement_frame_context,
                BuildBrushDoorEntityViews(state),
                brush_door_hooks);

            map_logic_dispatcher.CompleteFrame(map_logic_hooks);
            state.map_logic_dispatcher_state = map_logic_dispatcher.Summary();

            for (RuntimeEntityRecord& record : state.entity_bootstrap.runtime_entities)
            {
                if (record.edict_index >= 0)
                {
                    if (edict_t* entity = state.edict_store.EntityOfIndex(record.edict_index);
                        entity != nullptr)
                    {
                        SyncRuntimeRecordFromEdict(state, entity, record);
                    }
                }

                ApplySceneMovementStateToRuntimeRecord(record, movement_controller);
            }

            state.scripted_movement_state = movement_controller.Summary();
            MergePathMoverStateIntoScriptedMovementSummary(
                state.scripted_movement_state,
                path_mover_controller.Summary());
            state.scripted_movement_state.brush_doors = brush_door_controller.Summary();
            state.scripted_movement_state.trace_movement =
                state.frame_bootstrap_options.trace_movement;
            std::unordered_set<std::string> all_movement_callbacks = movement_callbacks_exercised;
            all_movement_callbacks.insert(
                path_mover_callbacks_exercised.begin(),
                path_mover_callbacks_exercised.end());
            all_movement_callbacks.insert(
                brush_door_callbacks_exercised.begin(),
                brush_door_callbacks_exercised.end());
            state.scripted_movement_state.exercised_callbacks.assign(
                all_movement_callbacks.begin(),
                all_movement_callbacks.end());
            std::sort(
                state.scripted_movement_state.exercised_callbacks.begin(),
                state.scripted_movement_state.exercised_callbacks.end());
            state.scripted_movement_state.path_mover_callbacks_exercised.assign(
                path_mover_callbacks_exercised.begin(),
                path_mover_callbacks_exercised.end());
            std::sort(
                state.scripted_movement_state.path_mover_callbacks_exercised.begin(),
                state.scripted_movement_state.path_mover_callbacks_exercised.end());
            if (state.scripted_movement_state.brush_doors.opened > 0
                || state.scripted_movement_state.brush_doors.moving > 0)
            {
                state.scripted_movement_state.readiness =
                    "staged-safe brush door bootstrap is active; continue deeper deterministic traversal toward execute_sci and fade_out";
            }

            if (!state.scripted_movement_state.frames.empty())
            {
                const hl::game_api::ScriptedMovementFrameStateSummary& movement_frame =
                    state.scripted_movement_state.frames.back();
                LogMovementFrameSummary(state.frame_bootstrap_options, movement_frame);
            }

            ObserveTriggerChangeLevelTouchState(state, frame_number, time);
            ConsumePendingChangeLevelRequest(state);
            RefreshScriptedLogicStateSummary(state);
            state.scripted_logic_state.frames.push_back(BuildScriptedLogicFrameStateSummary(state));

            if (!state.scripted_logic_state.frames.empty())
            {
                const hl::game_api::ScriptedLogicFrameStateSummary& scripted_frame =
                    state.scripted_logic_state.frames.back();
                LogScriptedLogicFrameSummary(state.frame_bootstrap_options, scripted_frame);
            }
            state.entity_think_diagnostics.MarkCompleted();
        };
    hooks.frame_trace_tail =
        [&](std::size_t limit)
        {
            std::vector<std::string> lines;
            for (const hl::game_api::detail::SpawnTraceEvent& event : TakeTraceTail(
                     state.server_frame_diagnostics.TraceSnapshot(),
                     limit))
            {
                lines.push_back(FormatSpawnTraceEvent(event));
            }

            return lines;
        };
    hooks.frame_trace_event_count =
        [&]()
        {
            return state.server_frame_diagnostics.EventCount();
        };
    hooks.total_callback_counts =
        [&]()
        {
            return state.callback_counts;
        };
    hooks.completed_messages =
        [&](std::size_t limit)
        {
            return state.frame_message_buffer.CompletedPreview(limit);
        };
    hooks.drain_pending_commands =
        [&](int frame_number)
        {
            if (state.command_buffer.PendingCount() != 0)
            {
                ExecuteQueuedServerCommands(
                    state,
                    "server-frame-" + std::to_string(frame_number));
            }
        };
    hooks.should_stop_after_frame =
        [&](std::string* reason)
        {
            const hl::game_api::PathNodeMessageStateSummary& path_messages =
                state.scripted_movement_state.path_node_messages;
            if (state.frame_bootstrap_options.stop_on_first_message
                && path_messages.first_message_bearing_node_reached)
            {
                const std::string stop_reason =
                    "first message-bearing node reached: "
                    + (path_messages.first_message_bearing_node.empty()
                        ? std::string("<empty>")
                        : path_messages.first_message_bearing_node)
                    + " frame="
                    + std::to_string(path_messages.first_message_bearing_frame)
                    + " time="
                    + std::to_string(path_messages.first_message_bearing_time)
                    + " ftruck_a="
                    + (state.scripted_movement_state.delayed_ftruck_status.empty()
                        ? std::string("<unset>")
                        : state.scripted_movement_state.delayed_ftruck_status);
                if (reason != nullptr)
                {
                    *reason = stop_reason;
                }
                return true;
            }

            if (!state.frame_bootstrap_options.stop_on_node.empty())
            {
                const hl::game_api::PathMoverRuntimeSummary* ftruck =
                    path_mover_controller.FindMoverByTargetname("ftruck_a");
                if (ftruck != nullptr
                    && (EqualsIgnoreCase(ftruck->current_node, state.frame_bootstrap_options.stop_on_node)
                        || EqualsIgnoreCase(
                            path_messages.deepest_node_reached,
                            state.frame_bootstrap_options.stop_on_node)))
                {
                    const std::string stop_reason =
                        "stop-on-node reached: requested="
                        + state.frame_bootstrap_options.stop_on_node
                        + " previous="
                        + (ftruck->previous_node.empty()
                            ? std::string("<none>")
                            : ftruck->previous_node)
                        + " current="
                        + (ftruck->current_node.empty()
                            ? std::string("<none>")
                            : ftruck->current_node)
                        + " next="
                        + (ftruck->next_node.empty()
                            ? std::string("<none>")
                            : ftruck->next_node)
                        + " lastMessage="
                        + (ftruck->last_message.empty()
                            ? std::string("<none>")
                            : ftruck->last_message)
                        + " dispatch="
                        + (ftruck->last_message_dispatch_result.empty()
                            ? std::string("<none>")
                            : ftruck->last_message_dispatch_result)
                        + " status="
                        + (state.scripted_movement_state.delayed_ftruck_status.empty()
                            ? std::string("<unset>")
                            : state.scripted_movement_state.delayed_ftruck_status);
                    if (reason != nullptr)
                    {
                        *reason = stop_reason;
                    }
                    return true;
                }
            }

            if (state.frame_bootstrap_options.stop_on_changelevel_request
                && state.changelevel_transition_state.transition_intent_captured)
            {
                hl::game_api::ChangeLevelTransitionSummary& changelevel =
                    state.changelevel_transition_state;
                changelevel.pre_changelevel_handoff.active = true;
                changelevel.pre_changelevel_handoff.handoff_latched = true;
                changelevel.pre_changelevel_handoff.world_frozen = true;
                changelevel.pre_changelevel_handoff.stop_requested = true;
                changelevel.pre_changelevel_handoff.world_state = "frozen|latched|handoff-ready";
                changelevel.pre_changelevel_handoff.action = "no-op transition stop";
                changelevel.pre_changelevel_handoff.map_load_performed = false;
                changelevel.pre_changelevel_handoff.detail =
                    "staged pre-changelevel handoff latched, world frozen, and deterministic no-op transition stop requested; no map load performed";
                changelevel.transition_intent_action = "no-op transition stop";
                changelevel.transition_intent_detail =
                    "pending_changelevel_request consumed into staged-safe host transition intent; deterministic no-op transition stop requested before map load";
                RefreshChangeLevelLifecycleEntry(state);
                RefreshChangeLevelLifecycleDispatch(state);
                RefreshChangeLevelLifecycleExecution(state);
                RefreshChangeLevelBootstrapPlan(state);
                RefreshChangeLevelLandmarkTransform(state);
                RefreshChangeLevelProjectedCarriedOrigin(state);
                RefreshChangeLevelProjectedCarriedOrientation(state);
                RefreshChangeLevelProjectedTransferSnapshot(state);
                const std::string stop_reason =
                    "stop-on-changelevel-request reached: requestedMap="
                    + (changelevel.target_map.empty()
                        ? std::string("<none>")
                        : changelevel.target_map)
                    + " landmark="
                    + (changelevel.landmark.empty()
                        ? std::string("<none>")
                        : changelevel.landmark)
                    + " requestFrame="
                    + std::to_string(changelevel.transition_intent_request_frame)
                    + " requestTime="
                    + std::to_string(changelevel.transition_intent_request_time)
                    + " consumed=" + BoolToYesNo(changelevel.transition_intent_consumed)
                    + " action=" + changelevel.transition_intent_action;
                if (reason != nullptr)
                {
                    *reason = stop_reason;
                }
                return true;
            }

            return false;
        };
    hooks.log_info =
        [](std::string_view message)
        {
            hl::common::Logger::Debug(
                hl::common::LogCategory::Server,
                std::string(message));
        };
    hooks.log_warn =
        [](std::string_view message)
        {
            hl::common::Logger::Warn(
                hl::common::LogCategory::Server,
                std::string(message));
        };
    hooks.log_error =
        [](std::string_view message)
        {
            hl::common::Logger::Error(
                hl::common::LogCategory::Server,
                std::string(message));
        };

    state.server_frame_loop_state =
        hl::game_api::detail::RunServerFrameLoop(config, hooks);
    state.entity_think_scheduler_state = think_scheduler.Summary();
    state.map_logic_dispatcher_state = map_logic_dispatcher.Summary();
    RefreshScriptedLogicStateSummary(state);
}

void FinalizeServerBootstrapStep()
{
    EngineShimState& state = CurrentShimState();
    hl::game_api::detail::FinalizeServerState(state.server_state, state.cvar_registry);
    hl::game_api::detail::ApplyGlobalsFromServerState(
        state.server_state,
        state.string_pool,
        state.globalvars);
    state.globalvars.maxEntities = state.edict_store.MaxEntities();
    PerformWorldBootstrap();
    PerformEntityBootstrap();
    if (state.command_buffer.PendingCount() != 0)
    {
        ExecuteQueuedServerCommands(state, "post-worldspawn");
    }
    PerformServerActivation();
    if (state.command_buffer.PendingCount() != 0)
    {
        ExecuteQueuedServerCommands(state, "post-server-activate");
    }
    PerformServerFrameLoop();
    LogSpawnPipelineStubAvailability(state);
}

std::vector<hl::game_api::DllFunctionPointerStatus> CollectDllFunctionStatuses(const DLL_FUNCTIONS& functions)
{
    return {
        {"pfnGameInit", functions.pfnGameInit != nullptr},
        {"pfnSpawn", functions.pfnSpawn != nullptr},
        {"pfnThink", functions.pfnThink != nullptr},
        {"pfnUse", functions.pfnUse != nullptr},
        {"pfnTouch", functions.pfnTouch != nullptr},
        {"pfnBlocked", functions.pfnBlocked != nullptr},
        {"pfnKeyValue", functions.pfnKeyValue != nullptr},
        {"pfnSave", functions.pfnSave != nullptr},
        {"pfnRestore", functions.pfnRestore != nullptr},
        {"pfnSetAbsBox", functions.pfnSetAbsBox != nullptr},
        {"pfnSaveWriteFields", functions.pfnSaveWriteFields != nullptr},
        {"pfnSaveReadFields", functions.pfnSaveReadFields != nullptr},
        {"pfnSaveGlobalState", functions.pfnSaveGlobalState != nullptr},
        {"pfnRestoreGlobalState", functions.pfnRestoreGlobalState != nullptr},
        {"pfnResetGlobalState", functions.pfnResetGlobalState != nullptr},
        {"pfnClientConnect", functions.pfnClientConnect != nullptr},
        {"pfnClientDisconnect", functions.pfnClientDisconnect != nullptr},
        {"pfnClientKill", functions.pfnClientKill != nullptr},
        {"pfnClientPutInServer", functions.pfnClientPutInServer != nullptr},
        {"pfnClientCommand", functions.pfnClientCommand != nullptr},
        {"pfnClientUserInfoChanged", functions.pfnClientUserInfoChanged != nullptr},
        {"pfnServerActivate", functions.pfnServerActivate != nullptr},
        {"pfnServerDeactivate", functions.pfnServerDeactivate != nullptr},
        {"pfnPlayerPreThink", functions.pfnPlayerPreThink != nullptr},
        {"pfnPlayerPostThink", functions.pfnPlayerPostThink != nullptr},
        {"pfnStartFrame", functions.pfnStartFrame != nullptr},
        {"pfnParmsNewLevel", functions.pfnParmsNewLevel != nullptr},
        {"pfnParmsChangeLevel", functions.pfnParmsChangeLevel != nullptr},
        {"pfnGetGameDescription", functions.pfnGetGameDescription != nullptr},
        {"pfnPlayerCustomization", functions.pfnPlayerCustomization != nullptr},
        {"pfnSpectatorConnect", functions.pfnSpectatorConnect != nullptr},
        {"pfnSpectatorDisconnect", functions.pfnSpectatorDisconnect != nullptr},
        {"pfnSpectatorThink", functions.pfnSpectatorThink != nullptr},
        {"pfnSys_Error", functions.pfnSys_Error != nullptr},
        {"pfnPM_Move", functions.pfnPM_Move != nullptr},
        {"pfnPM_Init", functions.pfnPM_Init != nullptr},
        {"pfnPM_FindTextureType", functions.pfnPM_FindTextureType != nullptr},
        {"pfnSetupVisibility", functions.pfnSetupVisibility != nullptr},
        {"pfnUpdateClientData", functions.pfnUpdateClientData != nullptr},
        {"pfnAddToFullPack", functions.pfnAddToFullPack != nullptr},
        {"pfnCreateBaseline", functions.pfnCreateBaseline != nullptr},
        {"pfnRegisterEncoders", functions.pfnRegisterEncoders != nullptr},
        {"pfnGetWeaponData", functions.pfnGetWeaponData != nullptr},
        {"pfnCmdStart", functions.pfnCmdStart != nullptr},
        {"pfnCmdEnd", functions.pfnCmdEnd != nullptr},
        {"pfnConnectionlessPacket", functions.pfnConnectionlessPacket != nullptr},
        {"pfnGetHullBounds", functions.pfnGetHullBounds != nullptr},
        {"pfnCreateInstancedBaselines", functions.pfnCreateInstancedBaselines != nullptr},
        {"pfnInconsistentFile", functions.pfnInconsistentFile != nullptr},
        {"pfnAllowLagCompensation", functions.pfnAllowLagCompensation != nullptr},
    };
}

void RefreshInvokedCallbacks(
    hl::game_api::HlServerModuleSummary& summary,
    const EngineShimState& state)
{
    summary.invoked_engine_callbacks.clear();
    summary.invoked_engine_callbacks.reserve(state.callback_order.size());

    for (const std::string& callback_name : state.callback_order)
    {
        const auto count_it = state.callback_counts.find(callback_name);
        if (count_it == state.callback_counts.end())
        {
            continue;
        }

        summary.invoked_engine_callbacks.push_back({callback_name, count_it->second});
    }
}

void LogDllFunctionTable(const std::vector<hl::game_api::DllFunctionPointerStatus>& function_statuses)
{
    hl::common::Logger::Info(hl::common::LogCategory::Dll, "hl.dll DLL_FUNCTIONS table:");
    for (const hl::game_api::DllFunctionPointerStatus& status : function_statuses)
    {
        hl::common::Logger::Info(
            hl::common::LogCategory::Dll,
            "  - " + status.name + ": " + (status.present ? "present" : "missing"));
    }
}

void LogServerModuleSummary(const hl::game_api::HlServerModuleSummary& summary)
{
    LogCompactServerModuleSummary(summary);
    if (!hl::common::Logger::ShouldLog(
            hl::common::LogCategory::Summary,
            hl::common::LogLevel::Debug))
    {
        return;
    }

    hl::common::Logger::Info("hl.dll shim summary:");
    hl::common::Logger::Info(
        std::string("  - hl.dll loaded: ") + BoolToYesNo(summary.hl_dll_loaded));
    hl::common::Logger::Info(
        std::string("  - GiveFnptrsToDll called: ") + BoolToYesNo(summary.give_fnptrs_to_dll_called));
    hl::common::Logger::Info(
        std::string("  - GetEntityAPI2 succeeded: ") + BoolToYesNo(summary.get_entity_api2_succeeded));
    hl::common::Logger::Info(
        std::string("  - DLL_FUNCTIONS acquired: ") + BoolToYesNo(summary.dll_functions_acquired));
    hl::common::Logger::Info(
        std::string("  - pfnGameInit called: ") + BoolToYesNo(summary.pfn_game_init_called));

    if (summary.interface_version_requested != 0 || summary.interface_version_reported != 0)
    {
        hl::common::Logger::Info(
            "  - interface version: requested "
            + std::to_string(summary.interface_version_requested) + ", reported "
            + std::to_string(summary.interface_version_reported));
    }

    if (summary.invoked_engine_callbacks.empty())
    {
        hl::common::Logger::Info("  - engine callbacks invoked during init: none");
    }
    else
    {
        hl::common::Logger::Info("  - engine callbacks invoked during init:");
        for (const hl::game_api::InvokedEngineCallback& callback : summary.invoked_engine_callbacks)
        {
            hl::common::Logger::Info(
                "    * " + callback.name + " x" + std::to_string(callback.call_count));
        }
    }

    hl::common::Logger::Info(
        "  - registered cvars: " + std::to_string(summary.registered_cvars));
    hl::common::Logger::Info(
        "  - queued server commands: " + std::to_string(summary.queued_server_commands));
    hl::common::Logger::Info(
        "  - executed server commands: " + std::to_string(summary.executed_server_commands));
    hl::common::Logger::Info(
        "  - executed cfg files: " + std::to_string(summary.executed_cfg_files));
    hl::common::Logger::Info(
        "  - updated cvars from cfg: " + std::to_string(summary.updated_cvars_from_cfg));
    hl::common::Logger::Info(
        "  - auto-created cvars: " + std::to_string(summary.auto_created_cvars));
    if (summary.executed_cfg_paths.empty())
    {
        hl::common::Logger::Info("  - cfg files executed: none");
    }
    else
    {
        hl::common::Logger::Info("  - cfg files executed:");
        for (const std::string& path : summary.executed_cfg_paths)
        {
            hl::common::Logger::Info("    * " + path);
        }
    }

    if (summary.sample_skill_cvars.empty())
    {
        hl::common::Logger::Info("  - skill cvar examples after cfg: none");
    }
    else
    {
        hl::common::Logger::Info("  - skill cvar examples after cfg:");
        for (const hl::game_api::CvarSnapshot& cvar : summary.sample_skill_cvars)
        {
            hl::common::Logger::Info(
                "    * " + cvar.name + " = " + cvar.string_value
                + " (value=" + std::to_string(cvar.value) + ")");
        }
    }

    hl::common::Logger::Info(
        std::string("  - server state initialized: ")
        + BoolToYesNo(summary.server_state.initialized));
    hl::common::Logger::Info(
        "  - game dir: "
        + (summary.server_state.game_directory.empty()
            ? std::string("<unset>")
            : summary.server_state.game_directory));
    hl::common::Logger::Info(
        "  - mod name: "
        + (summary.server_state.mod_name.empty()
            ? std::string("<unset>")
            : summary.server_state.mod_name));
    hl::common::Logger::Info(
        "  - hostname: "
        + (summary.server_state.hostname.empty()
            ? std::string("<unset>")
            : summary.server_state.hostname));
    hl::common::Logger::Info(
        "  - map name: "
        + (summary.server_state.map_name.empty()
            ? std::string("<unset>")
            : summary.server_state.map_name));
    hl::common::Logger::Info(
        "  - server flags: active="
        + std::string(BoolToYesNo(summary.server_state.active))
        + ", loading=" + BoolToYesNo(summary.server_state.loading));
    hl::common::Logger::Info(
        "  - server counters: time=" + std::to_string(summary.server_state.time)
        + ", frametime=" + std::to_string(summary.server_state.frametime)
        + ", frame_count=" + std::to_string(summary.server_state.frame_count)
        + ", server_frame=" + std::to_string(summary.server_state.server_frame));
    hl::common::Logger::Info(
        "  - globals snapshot: mapname="
        + (summary.globals_snapshot.map_name.empty()
            ? std::string("<empty>")
            : summary.globals_snapshot.map_name)
        + ", startspot="
        + (summary.globals_snapshot.startspot.empty()
            ? std::string("<empty>")
            : summary.globals_snapshot.startspot)
        + ", time=" + std::to_string(summary.globals_snapshot.time)
        + ", frametime=" + std::to_string(summary.globals_snapshot.frametime)
        + ", deathmatch=" + std::to_string(summary.globals_snapshot.deathmatch)
        + ", coop=" + std::to_string(summary.globals_snapshot.coop)
        + ", maxClients=" + std::to_string(summary.globals_snapshot.max_clients));
    hl::common::Logger::Info(
        "  - callback groups: precache=" + std::to_string(summary.precache_callback_invocations)
        + ", model=" + std::to_string(summary.model_callback_invocations)
        + ", string=" + std::to_string(summary.string_callback_invocations)
        + ", entity=" + std::to_string(summary.entity_callback_invocations));
    hl::common::Logger::Info(
        std::string("  - ready for world bootstrap: ")
        + BoolToYesNo(summary.ready_for_world_bootstrap));
    hl::common::Logger::Info(
        std::string("  - world bootstrap: ")
        + BoolToYesNo(summary.world_bootstrap.completed));
    hl::common::Logger::Info(
        "  - world map path: "
        + (summary.world_bootstrap.map_path.empty()
            ? std::string("<unset>")
            : summary.world_bootstrap.map_path));
    hl::common::Logger::Info(
        std::string("  - BSP loaded: ") + BoolToYesNo(summary.world_bootstrap.bsp_loaded));
    hl::common::Logger::Info(
        "  - BSP version: " + std::to_string(summary.world_bootstrap.bsp_version));
    hl::common::Logger::Info(
        "  - BSP file size: " + std::to_string(summary.world_bootstrap.bsp_file_size));
    hl::common::Logger::Info(
        "  - world model index: " + std::to_string(summary.world_bootstrap.world_model_index));
    hl::common::Logger::Info(
        "  - entities lump size: " + std::to_string(summary.world_bootstrap.entities_lump_size));
    hl::common::Logger::Info(
        "  - edict #0 state: "
        + (summary.world_bootstrap.edict0_state.empty()
            ? std::string("<unset>")
            : summary.world_bootstrap.edict0_state));
    if (!summary.world_bootstrap.key_lumps.empty())
    {
        hl::common::Logger::Info("  - key BSP lumps:");
        for (const hl::game_api::BspLumpSummary& lump : summary.world_bootstrap.key_lumps)
        {
            hl::common::Logger::Info(
                "    * " + lump.name
                + ": offset=" + std::to_string(lump.file_offset)
                + ", size=" + std::to_string(lump.file_length)
                + ", valid=" + BoolToYesNo(lump.within_file));
        }
    }

    hl::common::Logger::Info(
        std::string("  - entity pipeline attempted: ")
        + BoolToYesNo(summary.entity_pipeline.attempted));
    hl::common::Logger::Info(
        std::string("  - entities lump parsed: ")
        + BoolToYesNo(summary.entity_pipeline.entities_lump_parsed || summary.entity_pipeline.partially_parsed));
    hl::common::Logger::Info(
        "  - entity parse errors: " + std::to_string(summary.entity_pipeline.parse_error_count));
    hl::common::Logger::Info(
        "  - parsed entity count: " + std::to_string(summary.entity_pipeline.total_parsed_entities));
    hl::common::Logger::Info(
        "  - worldspawn count: " + std::to_string(summary.entity_pipeline.total_worldspawn_entities));
    hl::common::Logger::Info(
        std::string("  - first entity is worldspawn: ")
        + BoolToYesNo(summary.entity_pipeline.first_entity_is_worldspawn));
    hl::common::Logger::Info(
        "  - runtime entities allocated: "
        + std::to_string(summary.entity_pipeline.total_runtime_entities_allocated));
    hl::common::Logger::Info(
        "  - keyvalues dispatched: "
        + std::to_string(summary.entity_pipeline.total_keyvalues_dispatched));
    hl::common::Logger::Info(
        "  - keyvalues handled: "
        + std::to_string(summary.entity_pipeline.total_keyvalues_handled));
    hl::common::Logger::Info(
        "  - spawn attempts: " + std::to_string(summary.entity_pipeline.total_spawn_attempts));
    hl::common::Logger::Info(
        "  - successful spawns: "
        + std::to_string(summary.entity_pipeline.total_successful_spawns));
    hl::common::Logger::Info(
        "  - removed entities: " + std::to_string(summary.entity_pipeline.total_removed_entities));
    hl::common::Logger::Info(
        "  - deferred entities: " + std::to_string(summary.entity_pipeline.total_deferred_entities));

    if (!summary.entity_pipeline.failure_reason.empty())
    {
        hl::common::Logger::Info(
            "  - entity pipeline note: " + summary.entity_pipeline.failure_reason);
    }

    if (!summary.entity_pipeline.top_classname_counts.empty())
    {
        hl::common::Logger::Info("  - top entity classnames:");
        for (const hl::game_api::EntityClassCountSummary& entry :
             summary.entity_pipeline.top_classname_counts)
        {
            hl::common::Logger::Info(
                "    * " + entry.classname + ": " + std::to_string(entry.count));
        }
    }

    if (!summary.entity_pipeline.classname_support_summary.empty())
    {
        hl::common::Logger::Info("  - classname support summary:");
        for (const hl::game_api::EntityClassSupportSummary& entry :
             summary.entity_pipeline.classname_support_summary)
        {
            hl::common::Logger::Info(
                "    * " + entry.classname
                + " | spawned=" + std::to_string(entry.spawned_successfully)
                + ", removed=" + std::to_string(entry.removed_during_spawn)
                + ", deferred=" + std::to_string(entry.deferred_unsupported)
                + ", failed=" + std::to_string(entry.failed_during_spawn));
        }
    }

    if (!summary.entity_pipeline.sample_entities.empty())
    {
        hl::common::Logger::Info("  - sample runtime entities:");
        for (const std::string& entity_line : summary.entity_pipeline.sample_entities)
        {
            hl::common::Logger::Info("    * " + entity_line);
        }
    }

    if (!summary.entity_pipeline.sample_spawned_entities.empty())
    {
        hl::common::Logger::Info("  - sample spawned entities:");
        for (const std::string& entity_line : summary.entity_pipeline.sample_spawned_entities)
        {
            hl::common::Logger::Info("    * " + entity_line);
        }
    }

    if (!summary.entity_pipeline.newly_exercised_engine_callbacks.empty())
    {
        hl::common::Logger::Info("  - newly exercised callbacks during entity bootstrap:");
        for (const hl::game_api::InvokedEngineCallback& callback :
             summary.entity_pipeline.newly_exercised_engine_callbacks)
        {
            hl::common::Logger::Info(
                "    * " + callback.name + " x" + std::to_string(callback.call_count));
        }
    }

    hl::common::Logger::Info(
        std::string("  - worldspawn spawn attempted: ")
        + BoolToYesNo(summary.worldspawn_spawn.attempted));
    hl::common::Logger::Info(
        std::string("  - worldspawn spawn succeeded: ")
        + BoolToYesNo(summary.worldspawn_spawn.succeeded));
    if (summary.worldspawn_spawn.seh_exception)
    {
        hl::common::Logger::Info(
            "  - worldspawn spawn exception: "
            + FormatExceptionCode(summary.worldspawn_spawn.seh_code));
    }
    hl::common::Logger::Info(
        "  - worldspawn last callback: "
        + (summary.worldspawn_spawn.last_callback.empty()
            ? std::string("<none>")
            : summary.worldspawn_spawn.last_callback));
    hl::common::Logger::Info(
        "  - worldspawn precache requests: "
        + std::to_string(summary.worldspawn_spawn.precache_requests));
    hl::common::Logger::Info(
        "  - worldspawn sound precache count: "
        + std::to_string(summary.worldspawn_spawn.sound_precache_count));
    hl::common::Logger::Info(
        "  - sound registry size: "
        + std::to_string(summary.worldspawn_spawn.sound_registry_size));
    hl::common::Logger::Info(
        "  - missing sound files: "
        + std::to_string(summary.worldspawn_spawn.missing_sound_files));
    hl::common::Logger::Info(
        "  - duplicate sound precaches: "
        + std::to_string(summary.worldspawn_spawn.duplicate_sound_precache_count));
    hl::common::Logger::Info(
        "  - RandomLong call count: "
        + std::to_string(summary.worldspawn_spawn.random_long_call_count));
    if (!summary.worldspawn_spawn.distinct_callback_tail.empty())
    {
        hl::common::Logger::Info("  - worldspawn distinct callback tail:");
        for (const std::string& trace_line : summary.worldspawn_spawn.distinct_callback_tail)
        {
            hl::common::Logger::Info("    * " + trace_line);
        }
    }
    if (!summary.worldspawn_spawn.trace_tail.empty())
    {
        hl::common::Logger::Info("  - worldspawn trace tail:");
        for (const std::string& trace_line : summary.worldspawn_spawn.trace_tail)
        {
            hl::common::Logger::Info("    * " + trace_line);
        }
    }
    if (!summary.worldspawn_spawn.precache_tail.empty())
    {
        hl::common::Logger::Info("  - worldspawn precache tail:");
        for (const std::string& trace_line : summary.worldspawn_spawn.precache_tail)
        {
            hl::common::Logger::Info("    * " + trace_line);
        }
    }

    hl::common::Logger::Info(
        std::string("  - server activation attempted: ")
        + BoolToYesNo(summary.server_activation.attempted));
    hl::common::Logger::Info(
        std::string("  - server activation preflight: ")
        + BoolToYesNo(summary.server_activation.preflight_succeeded));
    hl::common::Logger::Info(
        std::string("  - server activation succeeded: ")
        + BoolToYesNo(summary.server_activation.succeeded));
    hl::common::Logger::Info(
        "  - activation edicts passed: "
        + std::to_string(summary.server_activation.edict_count_passed));
    hl::common::Logger::Info(
        "  - activation allocated/spawned/removed/deferred: "
        + std::to_string(summary.server_activation.allocated_edicts) + "/"
        + std::to_string(summary.server_activation.spawned_edicts) + "/"
        + std::to_string(summary.server_activation.removed_edicts) + "/"
        + std::to_string(summary.server_activation.deferred_edicts));
    hl::common::Logger::Info(
        "  - activation candidates: "
        + std::to_string(summary.server_activation.activation_candidate_count));
    hl::common::Logger::Info(
        "  - activation participants pruned: "
        + std::to_string(summary.server_activation.pruned_participants));
    hl::common::Logger::Info(
        "  - activation trace events captured: "
        + std::to_string(summary.server_activation.trace_events_captured));
    hl::common::Logger::Info(
        "  - activation map/maxClients: "
        + (summary.server_activation.map_name.empty()
            ? std::string("<empty>")
            : summary.server_activation.map_name)
        + "/" + std::to_string(summary.server_activation.max_clients));
    hl::common::Logger::Info(
        "  - activation worldspawn/edict0: "
        + std::string(BoolToYesNo(summary.server_activation.worldspawn_spawned)) + "/"
        + BoolToYesNo(summary.server_activation.edict0_valid));
    if (summary.server_activation.seh_exception)
    {
        hl::common::Logger::Info(
            "  - server activation exception: "
            + FormatExceptionCode(summary.server_activation.seh_code));
    }
    if (!summary.server_activation.globals_snapshot.empty())
    {
        hl::common::Logger::Info(
            "  - activation gpGlobals: " + summary.server_activation.globals_snapshot);
    }
    if (!summary.server_activation.server_state_snapshot.empty())
    {
        hl::common::Logger::Info(
            "  - activation server flags: " + summary.server_activation.server_state_snapshot);
    }
    if (!summary.server_activation.activation_entities_preview.empty())
    {
        hl::common::Logger::Info("  - activation entity preview:");
        for (const std::string& line : summary.server_activation.activation_entities_preview)
        {
            hl::common::Logger::Info("    * " + line);
        }
    }
    if (!summary.server_activation.activation_class_counts.empty())
    {
        hl::common::Logger::Info("  - activation classname counts:");
        for (const hl::game_api::EntityClassCountSummary& entry :
             summary.server_activation.activation_class_counts)
        {
            hl::common::Logger::Info(
                "    * " + entry.classname + ": " + std::to_string(entry.count));
        }
    }
    if (!summary.server_activation.pruned_entities_preview.empty())
    {
        hl::common::Logger::Info("  - activation pruned entities:");
        for (const std::string& line : summary.server_activation.pruned_entities_preview)
        {
            hl::common::Logger::Info("    * " + line);
        }
    }
    if (!summary.server_activation.validation_rejections.empty())
    {
        hl::common::Logger::Info("  - activation validation rejections:");
        for (const std::string& line : summary.server_activation.validation_rejections)
        {
            hl::common::Logger::Info("    * " + line);
        }
    }
    if (!summary.server_activation.distinct_callback_tail.empty())
    {
        hl::common::Logger::Info("  - ServerActivate distinct callback tail:");
        for (const std::string& line : summary.server_activation.distinct_callback_tail)
        {
            hl::common::Logger::Info("    * " + line);
        }
    }
    if (!summary.server_activation.callback_trace_tail.empty())
    {
        hl::common::Logger::Info("  - ServerActivate callback trace tail:");
        for (const std::string& line : summary.server_activation.callback_trace_tail)
        {
            hl::common::Logger::Info("    * " + line);
        }
    }
    if (!summary.server_activation.likely_blocker.empty())
    {
        hl::common::Logger::Info(
            "  - activation likely blocker: " + summary.server_activation.likely_blocker);
    }

    hl::common::Logger::Info(
        "  - frame bootstrap config: frames="
        + std::to_string(summary.frame_bootstrap_config.frames)
        + ", frametime=" + std::to_string(summary.frame_bootstrap_config.frametime)
        + ", think_limit=" + std::to_string(summary.frame_bootstrap_config.think_limit)
        + ", use_limit=" + std::to_string(summary.frame_bootstrap_config.use_limit)
        + ", scheduled_use_limit="
        + std::to_string(summary.frame_bootstrap_config.scheduled_use_limit)
        + ", path_arrival_epsilon="
        + std::to_string(summary.frame_bootstrap_config.path_arrival_epsilon)
        + ", trace_scripted="
        + std::string(summary.frame_bootstrap_config.trace_scripted ? "1" : "0")
        + ", trace_movement="
        + std::string(summary.frame_bootstrap_config.trace_movement ? "1" : "0")
        + ", trace_think="
        + std::string(summary.frame_bootstrap_config.trace_think ? "1" : "0")
        + ", log_frame_sample="
        + std::to_string(summary.frame_bootstrap_config.log_frame_sample)
        + ", log_state_changes_only="
        + std::string(summary.frame_bootstrap_config.log_state_changes_only ? "1" : "0")
        + ", stop_on_first_message="
        + std::string(summary.frame_bootstrap_config.stop_on_first_message ? "1" : "0")
        + ", stop_on_changelevel_request="
        + std::string(summary.frame_bootstrap_config.stop_on_changelevel_request ? "1" : "0")
        + ", stop_on_node="
        + (summary.frame_bootstrap_config.stop_on_node.empty()
            ? std::string("<none>")
            : summary.frame_bootstrap_config.stop_on_node));
    hl::common::Logger::Info(
        std::string("  - frame loop attempted: ")
        + BoolToYesNo(summary.server_frame_loop.attempted));
    hl::common::Logger::Info(
        std::string("  - frame loop activation gate: ")
        + BoolToYesNo(summary.server_frame_loop.activation_succeeded));
    hl::common::Logger::Info(
        "  - frame loop frames requested/completed: "
        + std::to_string(summary.server_frame_loop.frames_requested) + "/"
        + std::to_string(summary.server_frame_loop.frames_completed));
    hl::common::Logger::Info(
        "  - frame loop final time: "
        + std::to_string(summary.server_frame_loop.final_time));
    hl::common::Logger::Info(
        "  - frame loop pfnStartFrame present: "
        + std::string(BoolToYesNo(summary.server_frame_loop.start_frame_present)));
    hl::common::Logger::Info(
        "  - frame loop trace events captured: "
        + std::to_string(summary.server_frame_loop.total_trace_events));
    hl::common::Logger::Info(
        "  - frame loop SEH observed: "
        + std::string(BoolToYesNo(summary.server_frame_loop.any_seh)));
    if (summary.server_frame_loop.any_seh)
    {
        hl::common::Logger::Info(
            "  - frame loop SEH code: "
            + FormatExceptionCode(summary.server_frame_loop.seh_code));
    }
    hl::common::Logger::Info(
        "  - frame loop stopped early: "
        + std::string(BoolToYesNo(summary.server_frame_loop.stopped_early)));
    if (summary.server_frame_loop.stopped_early)
    {
        hl::common::Logger::Info(
            "  - frame loop stop detail: frame="
            + std::to_string(summary.server_frame_loop.stop_frame)
            + " time=" + std::to_string(summary.server_frame_loop.stop_time)
            + " reason="
            + (summary.server_frame_loop.stop_reason.empty()
                ? std::string("<none>")
                : summary.server_frame_loop.stop_reason));
    }
    if (!summary.server_frame_loop.validation_failures.empty())
    {
        hl::common::Logger::Info("  - frame loop validation failures:");
        for (const std::string& line : summary.server_frame_loop.validation_failures)
        {
            hl::common::Logger::Info("    * " + line);
        }
    }
    if (!summary.server_frame_loop.callbacks_during_loop.empty())
    {
        hl::common::Logger::Info("  - callbacks during frame loop:");
        for (const hl::game_api::InvokedEngineCallback& callback :
             summary.server_frame_loop.callbacks_during_loop)
        {
            hl::common::Logger::Info(
                "    * " + callback.name + " x" + std::to_string(callback.call_count));
        }
    }
    if (!summary.server_frame_loop.frames.empty())
    {
        hl::common::Logger::Info("  - frame loop per-frame summary:");
        for (const hl::game_api::ServerFrameStateSummary& frame : summary.server_frame_loop.frames)
        {
            hl::common::Logger::Info(
                "    * frame " + std::to_string(frame.frame_number)
                + " host/server=" + std::to_string(frame.host_frame_index)
                + "/" + std::to_string(frame.server_frame_index)
                + " time=" + std::to_string(frame.time)
                + " frametime=" + std::to_string(frame.frametime)
                + " validation=" + BoolToYesNo(frame.validation_passed)
                + " startFrame="
                + (frame.start_frame_called
                    ? (frame.start_frame_succeeded ? "ok" : "failed")
                    : (frame.start_frame_present ? "skipped" : "missing")));

            if (!frame.validation_issues.empty())
            {
                hl::common::Logger::Info("      validation issues:");
                for (const std::string& issue : frame.validation_issues)
                {
                    hl::common::Logger::Info("        - " + issue);
                }
            }
            if (!frame.entity_preview.empty())
            {
                hl::common::Logger::Info("      entity preview:");
                for (const std::string& line : frame.entity_preview)
                {
                    hl::common::Logger::Info("        - " + line);
                }
            }
            if (!frame.callback_counts_this_frame.empty())
            {
                hl::common::Logger::Info("      callbacks this frame:");
                for (const hl::game_api::InvokedEngineCallback& callback :
                     frame.callback_counts_this_frame)
                {
                    hl::common::Logger::Info(
                        "        - " + callback.name + " x"
                        + std::to_string(callback.call_count));
                }
            }
            if (!frame.message_preview.empty())
            {
                hl::common::Logger::Info("      message preview:");
                for (const std::string& line : frame.message_preview)
                {
                    hl::common::Logger::Info("        - " + line);
                }
            }
            if (!frame.callback_trace_tail.empty())
            {
                hl::common::Logger::Info("      callback trace tail:");
                for (const std::string& line : frame.callback_trace_tail)
                {
                    hl::common::Logger::Info("        - " + line);
                }
            }
            if (frame.seh_exception)
            {
                hl::common::Logger::Info(
                    "      SEH: " + FormatExceptionCode(frame.seh_code));
            }
        }
    }

    hl::common::Logger::Info(
        std::string("  - think scheduler attempted: ")
        + BoolToYesNo(summary.entity_think_scheduler.attempted));
    hl::common::Logger::Info(
        "  - think scheduler pfnThink present: "
        + std::string(BoolToYesNo(summary.entity_think_scheduler.think_dispatch_present)));
    hl::common::Logger::Info(
        "  - think scheduler frames attempted/completed: "
        + std::to_string(summary.entity_think_scheduler.frames_attempted) + "/"
        + std::to_string(summary.entity_think_scheduler.frames_completed));
    hl::common::Logger::Info(
        "  - think scheduler due/executed/deferred: "
        + std::to_string(summary.entity_think_scheduler.total_due_thinks) + "/"
        + std::to_string(summary.entity_think_scheduler.total_executed_thinks) + "/"
        + std::to_string(summary.entity_think_scheduler.total_deferred_thinks));
    hl::common::Logger::Info(
        "  - think scheduler failures/SEH: "
        + std::to_string(summary.entity_think_scheduler.total_think_failures) + "/"
        + std::to_string(summary.entity_think_scheduler.total_seh_failures));
    hl::common::Logger::Info(
        "  - think scheduler removed by game logic: "
        + std::to_string(summary.entity_think_scheduler.total_removed_by_game_logic));
    if (!summary.entity_think_scheduler.active_entity_counts_by_support_state.empty())
    {
        hl::common::Logger::Info("  - active entity counts by support state:");
        for (const hl::game_api::EntityLifecycleStateCountSummary& entry :
             summary.entity_think_scheduler.active_entity_counts_by_support_state)
        {
            hl::common::Logger::Info(
                "    * " + entry.state + ": " + std::to_string(entry.count));
        }
    }
    if (!summary.entity_think_scheduler.classname_lifecycle_summary.empty())
    {
        hl::common::Logger::Info("  - classname lifecycle summary:");
        for (const hl::game_api::EntityLifecycleClassSummary& entry :
             summary.entity_think_scheduler.classname_lifecycle_summary)
        {
            hl::common::Logger::Info(
                "    * " + entry.classname
                + " active/passive/deferred/removed="
                + std::to_string(entry.active_supported) + "/"
                + std::to_string(entry.passive_supported) + "/"
                + std::to_string(entry.detected_but_deferred) + "/"
                + std::to_string(entry.removed_by_game_logic)
                + " due/executed/deferred="
                + std::to_string(entry.due_thinks) + "/"
                + std::to_string(entry.executed_thinks) + "/"
                + std::to_string(entry.deferred_thinks));
        }

        const auto log_logic_progress =
            [&](std::string_view classname)
            {
                const auto it = std::find_if(
                    summary.entity_think_scheduler.classname_lifecycle_summary.begin(),
                    summary.entity_think_scheduler.classname_lifecycle_summary.end(),
                    [&](const hl::game_api::EntityLifecycleClassSummary& entry)
                    {
                        return EqualsIgnoreCase(entry.classname, classname);
                    });
                if (it == summary.entity_think_scheduler.classname_lifecycle_summary.end())
                {
                    return;
                }

                const bool progressing = it->executed_thinks > 0;
                hl::common::Logger::Info(
                    "  - early logic progress [" + std::string(classname) + "]: "
                    + (progressing ? "progressing" : "not yet progressing")
                    + " due/executed/deferred=" + std::to_string(it->due_thinks)
                    + "/" + std::to_string(it->executed_thinks)
                    + "/" + std::to_string(it->deferred_thinks));
            };

        log_logic_progress("trigger_auto");
        log_logic_progress("multi_manager");
        log_logic_progress("env_message");
        log_logic_progress("path_track");
        log_logic_progress("scripted_sequence");
    }
    if (!summary.entity_think_scheduler.callbacks_during_scheduler.empty())
    {
        hl::common::Logger::Info("  - callbacks during think scheduler:");
        for (const hl::game_api::InvokedEngineCallback& callback :
             summary.entity_think_scheduler.callbacks_during_scheduler)
        {
            hl::common::Logger::Info(
                "    * " + callback.name + " x" + std::to_string(callback.call_count));
        }
    }
    if (!summary.entity_think_scheduler.frames.empty())
    {
        hl::common::Logger::Info("  - think scheduler per-frame summary:");
        for (const hl::game_api::EntityThinkFrameStateSummary& frame :
             summary.entity_think_scheduler.frames)
        {
            hl::common::Logger::Info(
                "    * frame " + std::to_string(frame.frame_number)
                + " host/server=" + std::to_string(frame.host_frame_index)
                + "/" + std::to_string(frame.server_frame_index)
                + " time=" + std::to_string(frame.time)
                + " frametime=" + std::to_string(frame.frametime)
                + " active=" + std::to_string(frame.active_entities)
                + " due/executed/deferred=" + std::to_string(frame.due_thinks)
                + "/" + std::to_string(frame.executed_thinks)
                + "/" + std::to_string(frame.deferred_thinks)
                + " failures=" + std::to_string(frame.think_failures)
                + " seh=" + std::to_string(frame.seh_failures)
                + " removed=" + std::to_string(frame.removed_by_game_logic));

            if (!frame.due_entities_preview.empty())
            {
                hl::common::Logger::Info("      due preview:");
                for (const std::string& line : frame.due_entities_preview)
                {
                    hl::common::Logger::Info("        - " + line);
                }
            }
            if (!frame.callback_counts_this_frame.empty())
            {
                hl::common::Logger::Info("      callbacks this frame:");
                for (const hl::game_api::InvokedEngineCallback& callback :
                     frame.callback_counts_this_frame)
                {
                    hl::common::Logger::Info(
                        "        - " + callback.name + " x"
                        + std::to_string(callback.call_count));
                }
            }
            if (!frame.callback_trace_tail.empty())
            {
                hl::common::Logger::Info("      think trace tail:");
                for (const std::string& line : frame.callback_trace_tail)
                {
                    hl::common::Logger::Info("        - " + line);
                }
            }
        }
    }
    if (!summary.entity_think_scheduler.rolling_trace_tail.empty())
    {
        hl::common::Logger::Info("  - think scheduler rolling trace tail:");
        for (const std::string& line : summary.entity_think_scheduler.rolling_trace_tail)
        {
            hl::common::Logger::Info("    * " + line);
        }
    }

    hl::common::Logger::Info(
        std::string("  - use-target dispatcher attempted: ")
        + BoolToYesNo(summary.map_logic_dispatcher.attempted));
    hl::common::Logger::Info(
        "  - use-target dispatcher pfnUse present: "
        + std::string(BoolToYesNo(summary.map_logic_dispatcher.use_dispatch_present)));
    hl::common::Logger::Info(
        "  - use-target dispatcher frames attempted/completed: "
        + std::to_string(summary.map_logic_dispatcher.frames_attempted) + "/"
        + std::to_string(summary.map_logic_dispatcher.frames_completed));
    hl::common::Logger::Info(
        "  - target chains fired/resolutions: "
        + std::to_string(summary.map_logic_dispatcher.total_target_chains_fired) + "/"
        + std::to_string(summary.map_logic_dispatcher.total_target_resolutions));
    hl::common::Logger::Info(
        "  - target lookup no/single/multi: "
        + std::to_string(summary.map_logic_dispatcher.total_no_targets_found) + "/"
        + std::to_string(summary.map_logic_dispatcher.total_single_target_hits) + "/"
        + std::to_string(summary.map_logic_dispatcher.total_multi_target_hits));
    hl::common::Logger::Info(
        "  - pfnUse attempts/successes/deferred/failures: "
        + std::to_string(summary.map_logic_dispatcher.total_use_attempts) + "/"
        + std::to_string(summary.map_logic_dispatcher.total_use_successes) + "/"
        + std::to_string(summary.map_logic_dispatcher.total_use_deferred) + "/"
        + std::to_string(summary.map_logic_dispatcher.total_use_failures));
    hl::common::Logger::Info(
        "  - pfnUse SEH failures: "
        + std::to_string(summary.map_logic_dispatcher.total_use_seh_failures));
    hl::common::Logger::Info(
        "  - scheduled-use queue created/executed/pending: "
        + std::to_string(summary.map_logic_dispatcher.total_scheduled_created) + "/"
        + std::to_string(summary.map_logic_dispatcher.total_scheduled_executed) + "/"
        + std::to_string(summary.map_logic_dispatcher.scheduled_pending));
    if (!summary.map_logic_dispatcher.classname_summary.empty())
    {
        hl::common::Logger::Info("  - classname map-logic summary:");
        for (const hl::game_api::MapLogicClassSummary& entry :
             summary.map_logic_dispatcher.classname_summary)
        {
            hl::common::Logger::Info(
                "    * " + entry.classname
                + " support="
                + hl::game_api::detail::MapLogicSupportStateLabel(entry.support_state)
                + " active=" + std::to_string(entry.active_entities)
                + " think/use/emit=" + std::to_string(entry.can_think)
                + "/" + std::to_string(entry.can_receive_use)
                + "/" + std::to_string(entry.can_emit_targets)
                + " pending=" + std::to_string(entry.pending_scheduled_outputs)
                + " triggered=" + std::to_string(entry.triggered_this_frame)
                + " blocked=" + std::to_string(entry.blocked_or_deferred)
                + " resolved=" + std::to_string(entry.target_resolutions)
                + " use=" + std::to_string(entry.use_attempts)
                + "/" + std::to_string(entry.use_successes)
                + "/" + std::to_string(entry.use_deferred)
                + "/" + std::to_string(entry.use_failures));
        }
    }
    if (!summary.map_logic_dispatcher.callbacks_during_dispatcher.empty())
    {
        hl::common::Logger::Info("  - callbacks during use-target dispatcher:");
        for (const hl::game_api::InvokedEngineCallback& callback :
             summary.map_logic_dispatcher.callbacks_during_dispatcher)
        {
            hl::common::Logger::Info(
                "    * " + callback.name + " x" + std::to_string(callback.call_count));
        }
    }
    if (!summary.map_logic_dispatcher.frames.empty())
    {
        hl::common::Logger::Info("  - use-target dispatcher per-frame summary:");
        for (const hl::game_api::MapLogicFrameStateSummary& frame :
             summary.map_logic_dispatcher.frames)
        {
            hl::common::Logger::Info(
                "    * frame " + std::to_string(frame.frame_number)
                + " host/server=" + std::to_string(frame.host_frame_index)
                + "/" + std::to_string(frame.server_frame_index)
                + " time=" + std::to_string(frame.time)
                + " frametime=" + std::to_string(frame.frametime)
                + " chains/resolved=" + std::to_string(frame.target_chains_fired)
                + "/" + std::to_string(frame.target_resolutions)
                + " use=" + std::to_string(frame.use_attempts)
                + "/" + std::to_string(frame.use_successes)
                + "/" + std::to_string(frame.use_deferred)
                + "/" + std::to_string(frame.use_failures)
                + " queued=" + std::to_string(frame.scheduled_created)
                + "/" + std::to_string(frame.scheduled_executed)
                + "/" + std::to_string(frame.scheduled_pending));
            if (!frame.callback_counts_this_frame.empty())
            {
                hl::common::Logger::Info("      callbacks this frame:");
                for (const hl::game_api::InvokedEngineCallback& callback :
                     frame.callback_counts_this_frame)
                {
                    hl::common::Logger::Info(
                        "        - " + callback.name + " x"
                        + std::to_string(callback.call_count));
                }
            }
            if (!frame.trace_tail.empty())
            {
                hl::common::Logger::Info("      map-logic trace tail:");
                for (const std::string& line : frame.trace_tail)
                {
                    hl::common::Logger::Info("        - " + line);
                }
            }
        }
    }
    if (!summary.map_logic_dispatcher.rolling_trace_tail.empty())
    {
        hl::common::Logger::Info("  - use-target dispatcher rolling trace tail:");
        for (const std::string& line : summary.map_logic_dispatcher.rolling_trace_tail)
        {
            hl::common::Logger::Info("    * " + line);
        }
    }

    hl::common::Logger::Info(
        std::string("  - scripted logic configured: ")
        + BoolToYesNo(summary.scripted_logic.configured));
    hl::common::Logger::Info(
        "  - scripted logic trace flag: "
        + std::string(BoolToYesNo(summary.scripted_logic.trace_scripted)));
    hl::common::Logger::Info(
        "  - scripted logic frames attempted/completed: "
        + std::to_string(summary.scripted_logic.frames_attempted) + "/"
        + std::to_string(summary.scripted_logic.frames_completed));
    hl::common::Logger::Info(
        "  - scripted totals use/emitted/scheduled/internal/follow-up: "
        + std::to_string(summary.scripted_logic.total_received_use) + "/"
        + std::to_string(summary.scripted_logic.total_emitted_targets) + "/"
        + std::to_string(summary.scripted_logic.total_scheduled_outputs) + "/"
        + std::to_string(summary.scripted_logic.total_internal_state_changes) + "/"
        + std::to_string(summary.scripted_logic.total_scheduled_follow_ups));
    hl::common::Logger::Info(
        "  - scripted progressed entities: "
        + std::to_string(summary.scripted_logic.total_progressed_entities));
    hl::common::Logger::Info(
        "  - scripted long-delay executed/pending: "
        + std::to_string(summary.scripted_logic.total_long_delay_executed) + "/"
        + std::to_string(summary.scripted_logic.total_long_delay_pending));
    hl::common::Logger::Info(
        "  - scripted_sequence total/used/changed/follow-up/emitted/progressing: "
        + std::to_string(summary.scripted_logic.scripted_sequence_summary.total) + "/"
        + std::to_string(summary.scripted_logic.scripted_sequence_summary.received_use) + "/"
        + std::to_string(summary.scripted_logic.scripted_sequence_summary.changed_state) + "/"
        + std::to_string(summary.scripted_logic.scripted_sequence_summary.scheduled_follow_up) + "/"
        + std::to_string(summary.scripted_logic.scripted_sequence_summary.emitted_targets) + "/"
        + std::to_string(summary.scripted_logic.scripted_sequence_summary.progressing));
    hl::common::Logger::Info(
        "  - scripted_sequence blocked movement/actor/engine: "
        + std::to_string(summary.scripted_logic.scripted_sequence_summary.blocked_on_movement)
        + "/"
        + std::to_string(summary.scripted_logic.scripted_sequence_summary.blocked_on_actor)
        + "/"
        + std::to_string(
            summary.scripted_logic.scripted_sequence_summary.blocked_on_engine_callback));
    hl::common::Logger::Info(
        "  - path_track nodes resolved next/unresolved next/message resolved/unresolved: "
        + std::to_string(summary.scripted_logic.path_track_summary.total_nodes) + " "
        + std::to_string(summary.scripted_logic.path_track_summary.resolved_next_links) + "/"
        + std::to_string(summary.scripted_logic.path_track_summary.unresolved_next_links)
        + " message="
        + std::to_string(summary.scripted_logic.path_track_summary.resolved_message_targets)
        + "/"
        + std::to_string(summary.scripted_logic.path_track_summary.unresolved_message_targets));
    hl::common::Logger::Info(
        "  - path_track linked sequences/actors: "
        + std::to_string(summary.scripted_logic.path_track_summary.sequences_with_path_links)
        + "/"
        + std::to_string(summary.scripted_logic.path_track_summary.actors_with_path_links));
    if (!summary.scripted_logic.classname_summary.empty())
    {
        hl::common::Logger::Info("  - scripted classname summary:");
        for (const hl::game_api::ScriptedLogicClassSummary& entry :
             summary.scripted_logic.classname_summary)
        {
            hl::common::Logger::Info(
                "    * " + entry.classname
                + " support="
                + hl::game_api::detail::ScriptedLogicSupportStateLabel(entry.support_state)
                + " count=" + std::to_string(entry.entity_count)
                + " use/emitted/scheduled="
                + std::to_string(entry.received_use_count) + "/"
                + std::to_string(entry.emitted_target_count) + "/"
                + std::to_string(entry.scheduled_output_count)
                + " progressing=" + std::to_string(entry.progressing_count)
                + " blocked=" + std::to_string(entry.blocked_count));
        }
    }
    if (!summary.scripted_logic.blocked_reasons.empty())
    {
        hl::common::Logger::Info("  - scripted blocked reasons:");
        for (const hl::game_api::NamedCountSummary& entry : summary.scripted_logic.blocked_reasons)
        {
            hl::common::Logger::Info(
                "    * " + entry.name + ": " + std::to_string(entry.count));
        }
    }
    if (!summary.scripted_logic.path_track_summary.preview.empty())
    {
        hl::common::Logger::Info("  - path_track preview:");
        for (const std::string& line : summary.scripted_logic.path_track_summary.preview)
        {
            hl::common::Logger::Info("    * " + line);
        }
    }
    if (!summary.scripted_logic.progressing_entities_preview.empty())
    {
        hl::common::Logger::Info("  - progressing scripted entities:");
        for (const hl::game_api::ScriptedLogicEntitySummary& entry :
             summary.scripted_logic.progressing_entities_preview)
        {
            hl::common::Logger::Info(
                "    * edict#" + std::to_string(entry.edict_index)
                + " classname=" + (entry.classname.empty() ? std::string("<empty>") : entry.classname)
                + " targetname="
                + (entry.targetname.empty() ? std::string("<empty>") : entry.targetname)
                + " support="
                + hl::game_api::detail::ScriptedLogicSupportStateLabel(entry.support_state)
                + " actor="
                + (entry.actor_name.empty() ? std::string("<none>") : entry.actor_name)
                + " pathLinked=" + BoolToYesNo(entry.path_linked)
                + " blocked="
                + (entry.blocked_reason.empty() ? std::string("<none>") : entry.blocked_reason));
        }
    }
    if (!summary.scripted_logic.blocked_entities_preview.empty())
    {
        hl::common::Logger::Info("  - blocked scripted entities:");
        for (const hl::game_api::ScriptedLogicEntitySummary& entry :
             summary.scripted_logic.blocked_entities_preview)
        {
            hl::common::Logger::Info(
                "    * edict#" + std::to_string(entry.edict_index)
                + " classname=" + (entry.classname.empty() ? std::string("<empty>") : entry.classname)
                + " targetname="
                + (entry.targetname.empty() ? std::string("<empty>") : entry.targetname)
                + " support="
                + hl::game_api::detail::ScriptedLogicSupportStateLabel(entry.support_state)
                + " actor="
                + (entry.actor_name.empty() ? std::string("<none>") : entry.actor_name)
                + " blocked="
                + (entry.blocked_reason.empty() ? std::string("<none>") : entry.blocked_reason));
        }
    }
    if (!summary.scripted_logic.frames.empty())
    {
        hl::common::Logger::Info("  - scripted logic per-frame summary:");
        for (const hl::game_api::ScriptedLogicFrameStateSummary& frame :
             summary.scripted_logic.frames)
        {
            hl::common::Logger::Info(
                "    * frame " + std::to_string(frame.frame_number)
                + " host/server=" + std::to_string(frame.host_frame_index)
                + "/" + std::to_string(frame.server_frame_index)
                + " time=" + std::to_string(frame.time)
                + " queue=" + std::to_string(frame.queue_size_before)
                + "->" + std::to_string(frame.queue_size_after)
                + " due/executed/rescheduled/skipped/failed/deferred="
                + std::to_string(frame.due_delayed_actions) + "/"
                + std::to_string(frame.executed_delayed_actions) + "/"
                + std::to_string(frame.rescheduled_delayed_actions) + "/"
                + std::to_string(frame.skipped_delayed_actions) + "/"
                + std::to_string(frame.failed_delayed_actions) + "/"
                + std::to_string(frame.deferred_delayed_actions)
                + " progressing=" + std::to_string(frame.newly_progressed_scripted_entities)
                + " blocked=" + std::to_string(frame.blocked_scripted_entities)
                + " longDelay pending/executed="
                + std::to_string(frame.long_delay_pending) + "/"
                + std::to_string(frame.long_delay_executed));
            if (!frame.progressing_preview.empty())
            {
                hl::common::Logger::Info("      progress preview:");
                for (const std::string& line : frame.progressing_preview)
                {
                    hl::common::Logger::Info("        - " + line);
                }
            }
            if (!frame.blocked_by_reason.empty())
            {
                hl::common::Logger::Info("      blocked reasons:");
                for (const hl::game_api::NamedCountSummary& entry : frame.blocked_by_reason)
                {
                    hl::common::Logger::Info(
                        "        - " + entry.name + ": " + std::to_string(entry.count));
                }
            }
            if (!frame.pending_delay_preview.empty())
            {
                hl::common::Logger::Info("      pending delay preview:");
                for (const std::string& line : frame.pending_delay_preview)
                {
                    hl::common::Logger::Info("        - " + line);
                }
            }
        }
    }

    hl::common::Logger::Info(
        std::string("  - scripted movement controller attempted: ")
        + BoolToYesNo(summary.scripted_movement.attempted));
    hl::common::Logger::Info(
        std::string("  - scripted movement trace flag: ")
        + BoolToYesNo(summary.scripted_movement.trace_movement));
    hl::common::Logger::Info(
        "  - total frames attempted/completed: "
        + std::to_string(summary.server_frame_loop.frames_requested) + "/"
        + std::to_string(summary.server_frame_loop.frames_completed));
    hl::common::Logger::Info(
        "  - scripted movement frames attempted/completed: "
        + std::to_string(summary.scripted_movement.frames_attempted) + "/"
        + std::to_string(summary.scripted_movement.frames_completed));
    hl::common::Logger::Info(
        "  - actor resolutions attempted/succeeded/failed: "
        + std::to_string(summary.scripted_movement.actor_resolutions_attempted) + "/"
        + std::to_string(summary.scripted_movement.actor_resolutions_succeeded) + "/"
        + std::to_string(summary.scripted_movement.actor_resolutions_failed));
    hl::common::Logger::Info(
        "  - scene movement attempts/successes/blocked: "
        + std::to_string(summary.scripted_movement.scene_movement_attempts) + "/"
        + std::to_string(summary.scripted_movement.scene_movement_successes) + "/"
        + std::to_string(summary.scripted_movement.scene_movement_blocked));
    hl::common::Logger::Info(
        "  - movement bootstrap mode: "
        + (summary.scripted_movement.bootstrap_mode.empty()
            ? std::string("<unset>")
            : summary.scripted_movement.bootstrap_mode));
    hl::common::Logger::Info(
        "  - path graph nodes/valid/broken/messages: "
        + std::to_string(summary.scripted_movement.path_graph.nodes) + "/"
        + std::to_string(summary.scripted_movement.path_graph.valid_links) + "/"
        + std::to_string(summary.scripted_movement.path_graph.broken_links) + "/"
        + std::to_string(summary.scripted_movement.path_graph.nodes_with_message));
    hl::common::Logger::Info(
        "  - path graph inactive/duplicates/orphans/cycles: "
        + std::to_string(summary.scripted_movement.path_graph.inactive_nodes) + "/"
        + std::to_string(summary.scripted_movement.path_graph.duplicate_targetnames) + "/"
        + std::to_string(summary.scripted_movement.path_graph.orphan_nodes) + "/"
        + std::to_string(summary.scripted_movement.path_graph.cycles));
    hl::common::Logger::Info(
        "  - path movers resolved/startResolved/graphBound/activated/moving/arrived/pathAdvances/stopped/blocked: "
        + std::to_string(summary.scripted_movement.path_movers.func_tracktrain_resolved) + "/"
        + std::to_string(summary.scripted_movement.path_movers.start_targets_resolved) + "/"
        + std::to_string(summary.scripted_movement.path_movers.track_bound) + "/"
        + std::to_string(summary.scripted_movement.path_movers.activated) + "/"
        + std::to_string(summary.scripted_movement.path_movers.moving) + "/"
        + std::to_string(summary.scripted_movement.path_movers.arrived_at_node) + "/"
        + std::to_string(summary.scripted_movement.path_movers.path_advances) + "/"
        + std::to_string(summary.scripted_movement.path_movers.stopped) + "/"
        + std::to_string(summary.scripted_movement.path_movers.blocked));
    hl::common::Logger::Info(
        "  - path mover nodeArrivals/stagedDispatches/brokenLinkHits: "
        + std::to_string(summary.scripted_movement.path_movers.node_arrivals) + "/"
        + std::to_string(summary.scripted_movement.path_movers.staged_message_dispatches) + "/"
        + std::to_string(summary.scripted_movement.path_movers.broken_link_hits));
    hl::common::Logger::Info(
        "  - path arrival epsilon used: "
        + std::to_string(summary.scripted_movement.path_arrival_epsilon));
    hl::common::Logger::Info(
        "  - path snap-to-node occurred: "
        + std::string(BoolToYesNo(summary.scripted_movement.path_snap_to_node_occurred)));
    hl::common::Logger::Info(
        "  - flatbedstart resolved: "
        + std::string(BoolToYesNo(summary.scripted_movement.flatbedstart_resolved)));
    hl::common::Logger::Info(
        "  - delayed ftruck_a status: "
        + (summary.scripted_movement.delayed_ftruck_status.empty()
            ? std::string("<unset>")
            : summary.scripted_movement.delayed_ftruck_status));
    hl::common::Logger::Info(
        "  - ftruck_a final current/next: "
        + (summary.scripted_movement.ftruck_final_current.empty()
            ? std::string("<none>")
            : summary.scripted_movement.ftruck_final_current)
        + "/"
        + (summary.scripted_movement.ftruck_final_next.empty()
            ? std::string("<none>")
            : summary.scripted_movement.ftruck_final_next));
    hl::common::Logger::Info(
        "  - deepest node reached: "
        + (summary.scripted_movement.path_node_messages.deepest_node_reached.empty()
            ? std::string("<none>")
            : summary.scripted_movement.path_node_messages.deepest_node_reached));
    hl::common::Logger::Info(
        "  - ftruck_a advanced beyond trainstop1a: "
        + std::string(summary.scripted_movement.ftruck_advanced_beyond_trainstop1a ? "yes" : "no"));
    if (!summary.scripted_movement.movement_blocked_reasons.empty())
    {
        hl::common::Logger::Info("  - movement blocked reasons:");
        for (const hl::game_api::NamedCountSummary& entry :
             summary.scripted_movement.movement_blocked_reasons)
        {
            hl::common::Logger::Info(
                "    * " + entry.name + ": " + std::to_string(entry.count));
        }
    }
    if (!summary.scripted_movement.scene_stage_summary.empty())
    {
        hl::common::Logger::Info("  - scripted movement stage summary:");
        for (const hl::game_api::NamedCountSummary& entry :
             summary.scripted_movement.scene_stage_summary)
        {
            hl::common::Logger::Info(
                "    * " + entry.name + ": " + std::to_string(entry.count));
        }
    }
    if (!summary.scripted_movement.path_graph.preview.empty())
    {
        hl::common::Logger::Info("  - path graph preview:");
        for (const std::string& line : summary.scripted_movement.path_graph.preview)
        {
            hl::common::Logger::Info("    * " + line);
        }
    }
    if (!summary.scripted_movement.path_graph.broken_link_preview.empty())
    {
        hl::common::Logger::Info("  - path graph broken links:");
        for (const std::string& line : summary.scripted_movement.path_graph.broken_link_preview)
        {
            hl::common::Logger::Info("    * " + line);
        }
    }
    if (!summary.scripted_movement.path_graph.duplicate_targetname_preview.empty())
    {
        hl::common::Logger::Info("  - path graph duplicate targetnames:");
        for (const std::string& line :
             summary.scripted_movement.path_graph.duplicate_targetname_preview)
        {
            hl::common::Logger::Info("    * " + line);
        }
    }
    if (!summary.scripted_movement.path_graph.orphan_preview.empty())
    {
        hl::common::Logger::Info("  - path graph orphans:");
        for (const std::string& line : summary.scripted_movement.path_graph.orphan_preview)
        {
            hl::common::Logger::Info("    * " + line);
        }
    }
    if (!summary.scripted_movement.path_graph.cycle_preview.empty())
    {
        hl::common::Logger::Info("  - path graph cycles:");
        for (const std::string& line : summary.scripted_movement.path_graph.cycle_preview)
        {
            hl::common::Logger::Info("    * " + line);
        }
    }
    if (!summary.scripted_movement.path_graph.message_node_preview.empty())
    {
        hl::common::Logger::Info("  - path graph message nodes:");
        for (const std::string& line : summary.scripted_movement.path_graph.message_node_preview)
        {
            hl::common::Logger::Info("    * " + line);
        }
    }
    if (!summary.scripted_movement.canaries.empty())
    {
        hl::common::Logger::Info("  - canary scene states:");
        for (const hl::game_api::CanarySceneStateSummary& canary :
             summary.scripted_movement.canaries)
        {
            hl::common::Logger::Info(
                "    * " + canary.canary_name
                + " actor=" + (canary.actor_name.empty() ? std::string("<none>") : canary.actor_name)
                + " stage=" + canary.stage
                + " progressed=" + BoolToYesNo(canary.progressed_further_than_before)
                + " status=" + canary.status);
        }
    }
    if (!summary.scripted_movement.scenes_preview.empty())
    {
        hl::common::Logger::Info("  - scripted movement scene preview:");
        for (const hl::game_api::ScriptedSceneRuntimeSummary& scene :
             summary.scripted_movement.scenes_preview)
        {
            hl::common::Logger::Info(
                "    * edict#" + std::to_string(scene.edict_index)
                + " targetname=" + (scene.targetname.empty() ? std::string("<empty>") : scene.targetname)
                + " stage=" + scene.stage
                + " actor="
                + (scene.entity_name.empty() ? std::string("<none>") : scene.entity_name)
                + " resolved=" + BoolToYesNo(scene.actor_resolved)
                + " blocked="
                + (scene.blocked_reason.empty() ? std::string("<none>") : scene.blocked_reason));
        }
    }
    if (!summary.scripted_movement.path_movers_preview.empty())
    {
        hl::common::Logger::Info("  - path mover preview:");
        for (const hl::game_api::PathMoverRuntimeSummary& mover :
             summary.scripted_movement.path_movers_preview)
        {
            hl::common::Logger::Info(
                "    * edict#" + std::to_string(mover.edict_index)
                + " targetname=" + (mover.targetname.empty() ? std::string("<empty>") : mover.targetname)
                + " model=" + (mover.model.empty() ? std::string("<none>") : mover.model)
                + " stage=" + mover.stage
                + " requestedStart="
                + (mover.requested_start_node.empty()
                    ? std::string("<none>")
                    : mover.requested_start_node)
                + " resolvedStart="
                + (mover.resolved_start_node.empty()
                    ? std::string("<none>")
                    : mover.resolved_start_node)
                + " previous=" + (mover.previous_node.empty() ? std::string("<none>") : mover.previous_node)
                + " current=" + (mover.current_node.empty() ? std::string("<none>") : mover.current_node)
                + " next=" + (mover.next_node.empty() ? std::string("<none>") : mover.next_node)
                + " origin=" + (mover.origin_text.empty() ? std::string("<unset>") : mover.origin_text)
                + " distanceToNext=" + std::to_string(mover.last_arrival_distance)
                + " arrivalDecision="
                + (mover.last_arrival_decision.empty()
                    ? std::string("<unset>")
                    : mover.last_arrival_decision)
                + " speed=" + std::to_string(mover.effective_speed)
                + " speedDecision="
                + (mover.last_speed_decision.empty()
                    ? std::string("<unset>")
                    : mover.last_speed_decision)
                + " speedBeforeArrival="
                + std::to_string(mover.last_speed_before_arrival)
                + " speedAfterArrival="
                + std::to_string(mover.last_speed_after_arrival)
                + " speedChanged="
                + (mover.last_speed_changed_on_arrival ? "yes" : "no")
                + " arrivals=" + std::to_string(mover.node_arrivals)
                + " advances=" + std::to_string(mover.node_advances)
                + " lastMessage="
                + (mover.last_message.empty() ? std::string("<none>") : mover.last_message)
                + " dispatch="
                + (mover.last_message_dispatch_result.empty()
                    ? std::string("<none>")
                    : mover.last_message_dispatch_result)
                + " blocked="
                + (mover.blocked_reason.empty() ? std::string("<none>") : mover.blocked_reason)
                + " stopped="
                + (mover.stopped_reason.empty() ? std::string("<none>") : mover.stopped_reason));
        }
    }
    hl::common::Logger::Info("  - path node messages encountered:");
    if (summary.scripted_movement.path_messages_encountered.empty())
    {
        hl::common::Logger::Info("    * <none>");
    }
    else
    {
        for (const std::string& message : summary.scripted_movement.path_messages_encountered)
        {
            hl::common::Logger::Info("    * " + message);
        }
    }
    hl::common::Logger::Info(
        "  - first message-bearing node reached: "
        + std::string(BoolToYesNo(
            summary.scripted_movement.path_node_messages.first_message_bearing_node_reached))
        + (summary.scripted_movement.path_node_messages.first_message_bearing_node_reached
            ? " node=" + summary.scripted_movement.path_node_messages.first_message_bearing_node
                + " frame="
                + std::to_string(
                    summary.scripted_movement.path_node_messages.first_message_bearing_frame)
                + " time="
                + std::to_string(
                    summary.scripted_movement.path_node_messages.first_message_bearing_time)
            : std::string()));
    hl::common::Logger::Info(
        "  - staged path-node dispatch attempts/successes/deferred/failures/unresolved: "
        + std::to_string(summary.scripted_movement.path_node_messages.staged_dispatch_attempts) + "/"
        + std::to_string(summary.scripted_movement.path_node_messages.staged_dispatch_successes) + "/"
        + std::to_string(summary.scripted_movement.path_node_messages.staged_dispatch_deferred) + "/"
        + std::to_string(summary.scripted_movement.path_node_messages.staged_dispatch_failures) + "/"
        + std::to_string(summary.scripted_movement.path_node_messages.unresolved_message_targets));
    hl::common::Logger::Info(
        "  - path-node message staged map logic dispatch: "
        + std::string(summary.scripted_movement.path_node_message_triggered_dispatch ? "yes" : "no")
        + " count=" + std::to_string(summary.scripted_movement.path_node_message_dispatch_count));
    hl::common::Logger::Info(
        "  - message-bearing nodes reached:");
    if (summary.scripted_movement.path_node_messages.reached_message_nodes.empty())
    {
        hl::common::Logger::Info("    * <none>");
    }
    else
    {
        for (const std::string& node : summary.scripted_movement.path_node_messages.reached_message_nodes)
        {
            hl::common::Logger::Info("    * " + node);
        }
    }
    if (!summary.scripted_movement.path_node_messages.message_records.empty())
    {
        hl::common::Logger::Info("  - message-node runtime records:");
        for (const hl::game_api::PathNodeMessageEncounterSummary& record :
             summary.scripted_movement.path_node_messages.message_records)
        {
            hl::common::Logger::Info(
                "    * node=" + record.node_name
                + " message=" + (record.message.empty() ? std::string("<none>") : record.message)
                + " first=" + (record.first_reached_frame >= 0
                    ? ("frame " + std::to_string(record.first_reached_frame)
                        + " time=" + std::to_string(record.first_reached_time))
                    : std::string("<none>"))
                + " nodeSpeed=" + std::to_string(record.node_speed_metadata)
                + " moverSpeed=" + std::to_string(record.mover_speed_at_encounter)
                + " speedBefore=" + std::to_string(record.mover_speed_before_encounter)
                + " speedAfter=" + std::to_string(record.mover_speed_after_encounter)
                + " speedChanged=" + BoolToYesNo(record.mover_speed_changed)
                + " dispatchAttempted=" + BoolToYesNo(record.dispatch_attempted)
                + " result="
                + (record.dispatch_result.empty()
                    ? std::string("<none>")
                    : record.dispatch_result)
                + " classification="
                + (record.classification.empty()
                    ? std::string("<none>")
                    : record.classification)
                + " dispatchMode="
                + (record.dispatch_mode.empty()
                    ? std::string("<none>")
                    : record.dispatch_mode)
                + " resolvedTargets=" + std::to_string(record.resolved_targets)
                + " runtimeTargets=" + std::to_string(record.runtime_target_candidates)
                + " parsedTargets=" + std::to_string(record.parsed_target_candidates)
                + " pfnUseAttempted=" + BoolToYesNo(record.pfn_use_attempted)
                + " visibleDownstream=" + BoolToYesNo(record.visible_downstream_progression)
                + " fadeChannelAvailable=" + BoolToYesNo(record.fade_channel_available)
                + " fadeChannelUsed=" + BoolToYesNo(record.fade_channel_used)
                + " messageChannelAvailable=" + BoolToYesNo(record.message_channel_available)
                + " messageChannelUsed=" + BoolToYesNo(record.message_channel_used)
                + " envMessageLinkageFound=" + BoolToYesNo(record.env_message_linkage_found)
                + " envMessageLinkageUsed=" + BoolToYesNo(record.env_message_linkage_used)
                + " summaryFallbackUsed=" + BoolToYesNo(record.summary_only_fallback_used)
                + " targetClassnames=" + JoinStringValues(record.target_classnames)
                + " resolvedTargetDetails=" + JoinStringValues(record.resolved_target_details)
                + " downstream="
                + (record.downstream_summary.empty()
                    ? std::string("<none>")
                    : record.downstream_summary)
                + " presentationLinkage="
                + (record.presentation_linkage_detail.empty()
                    ? std::string("<none>")
                    : record.presentation_linkage_detail)
                + " brushDoorAttempted=" + BoolToYesNo(record.brush_door_handling_attempted)
                + " brushDoorPath="
                + (record.brush_door_dispatch_path.empty()
                    ? std::string("<none>")
                    : record.brush_door_dispatch_path)
                + " brushDoorSupport="
                + (record.brush_door_support_state.empty()
                    ? std::string("<none>")
                    : record.brush_door_support_state)
                + " brushDoorState="
                + (record.brush_door_state.empty()
                    ? std::string("<none>")
                    : record.brush_door_state)
                + " brushDoorStarted=" + BoolToYesNo(record.brush_door_movement_started)
                + " brushDoorCompleted=" + BoolToYesNo(record.brush_door_movement_completed)
                + " brushDoorAudit="
                + (record.brush_door_runtime_audit.empty()
                    ? std::string("<none>")
                    : record.brush_door_runtime_audit)
                + " requiredSubsystem="
                + (record.required_subsystem.empty()
                    ? std::string("<none>")
                    : record.required_subsystem));
        }
    }
    if (!summary.scripted_movement.path_message_dispatch_history.empty())
    {
        hl::common::Logger::Info("  - path-node message dispatch history:");
        for (const std::string& line : summary.scripted_movement.path_message_dispatch_history)
        {
            hl::common::Logger::Info("    * " + line);
        }
    }
    if (!summary.scripted_movement.path_node_messages.canaries.empty())
    {
        hl::common::Logger::Info("  - message-node canaries:");
        for (const hl::game_api::PathNodeMessageCanarySummary& canary :
             summary.scripted_movement.path_node_messages.canaries)
        {
            hl::common::Logger::Info(
                "    * " + canary.node_name
                + " reached=" + BoolToYesNo(canary.reached)
                + " first=" + (canary.first_reached_frame >= 0
                    ? ("frame " + std::to_string(canary.first_reached_frame)
                        + " time=" + std::to_string(canary.first_reached_time))
                    : std::string("<none>"))
                + " message=" + BoolToYesNo(canary.message_encountered)
                + " stagedDispatch=" + BoolToYesNo(canary.staged_dispatch_attempted)
                + " result="
                + (canary.dispatch_result.empty()
                    ? std::string("<none>")
                    : canary.dispatch_result)
                + " classification="
                + (canary.classification.empty()
                    ? std::string("<none>")
                    : canary.classification)
                + " dispatchMode="
                + (canary.dispatch_mode.empty()
                    ? std::string("<none>")
                    : canary.dispatch_mode)
                + " nodeSpeed=" + std::to_string(canary.node_speed_metadata)
                + " speed=" + std::to_string(canary.mover_speed_at_encounter)
                + " speedBefore=" + std::to_string(canary.mover_speed_before_encounter)
                + " speedAfter=" + std::to_string(canary.mover_speed_after_encounter)
                + " speedChanged=" + BoolToYesNo(canary.mover_speed_changed)
                + " text=" + (canary.message.empty()
                    ? std::string("<none>")
                    : canary.message)
                + " fadeChannelAvailable=" + BoolToYesNo(canary.fade_channel_available)
                + " fadeChannelUsed=" + BoolToYesNo(canary.fade_channel_used)
                + " messageChannelAvailable=" + BoolToYesNo(canary.message_channel_available)
                + " messageChannelUsed=" + BoolToYesNo(canary.message_channel_used)
                + " envMessageLinkageFound=" + BoolToYesNo(canary.env_message_linkage_found)
                + " envMessageLinkageUsed=" + BoolToYesNo(canary.env_message_linkage_used)
                + " summaryFallbackUsed=" + BoolToYesNo(canary.summary_only_fallback_used)
                + " targetClassnames=" + JoinStringValues(canary.target_classnames)
                + " resolvedTargetDetails=" + JoinStringValues(canary.resolved_target_details)
                + " downstream="
                + (canary.downstream_summary.empty()
                    ? std::string("<none>")
                    : canary.downstream_summary)
                + " presentationLinkage="
                + (canary.presentation_linkage_detail.empty()
                    ? std::string("<none>")
                    : canary.presentation_linkage_detail)
                + " brushDoorAttempted=" + BoolToYesNo(canary.brush_door_handling_attempted)
                + " brushDoorPath="
                + (canary.brush_door_dispatch_path.empty()
                    ? std::string("<none>")
                    : canary.brush_door_dispatch_path)
                + " brushDoorSupport="
                + (canary.brush_door_support_state.empty()
                    ? std::string("<none>")
                    : canary.brush_door_support_state)
                + " brushDoorState="
                + (canary.brush_door_state.empty()
                    ? std::string("<none>")
                    : canary.brush_door_state)
                + " brushDoorStarted=" + BoolToYesNo(canary.brush_door_movement_started)
                + " brushDoorCompleted=" + BoolToYesNo(canary.brush_door_movement_completed)
                + " brushDoorAudit="
                + (canary.brush_door_runtime_audit.empty()
                    ? std::string("<none>")
                    : canary.brush_door_runtime_audit)
                + " requiredSubsystem="
                + (canary.required_subsystem.empty()
                    ? std::string("<none>")
                    : canary.required_subsystem));
        }
    }
    if (!summary.scripted_movement.path_node_messages.presentation_events.empty())
    {
        hl::common::Logger::Info("  - staged presentation events:");
        for (const hl::game_api::PathNodePresentationEventSummary& event :
             summary.scripted_movement.path_node_messages.presentation_events)
        {
            hl::common::Logger::Info(
                "    * event=" + (event.event_name.empty()
                    ? std::string("<none>")
                    : event.event_name)
                + " node=" + (event.source_node.empty()
                    ? std::string("<none>")
                    : event.source_node)
                + " frame=" + std::to_string(event.frame_number)
                + " time=" + std::to_string(event.time)
                + " attempted=" + BoolToYesNo(event.dispatch_attempted)
                + " succeeded=" + BoolToYesNo(event.succeeded)
                + " deferred=" + BoolToYesNo(event.deferred)
                + " mode="
                + (event.dispatch_mode.empty()
                    ? std::string("<none>")
                    : event.dispatch_mode)
                + " classification="
                + (event.classification.empty()
                    ? std::string("<none>")
                    : event.classification)
                + " fadeChannelAvailable=" + BoolToYesNo(event.fade_channel_available)
                + " fadeChannelUsed=" + BoolToYesNo(event.fade_channel_used)
                + " messageChannelAvailable=" + BoolToYesNo(event.message_channel_available)
                + " messageChannelUsed=" + BoolToYesNo(event.message_channel_used)
                + " envMessageLinkageFound=" + BoolToYesNo(event.env_message_linkage_found)
                + " envMessageLinkageUsed=" + BoolToYesNo(event.env_message_linkage_used)
                + " summaryFallbackUsed=" + BoolToYesNo(event.summary_only_fallback_used)
                + " requiredSubsystem="
                + (event.required_subsystem.empty()
                    ? std::string("<none>")
                    : event.required_subsystem)
                + " linkage="
                + (event.presentation_linkage_detail.empty()
                    ? std::string("<none>")
                    : event.presentation_linkage_detail)
                + " detail="
                + (event.dispatch_detail.empty()
                    ? std::string("<none>")
                    : event.dispatch_detail));
        }
    }
    if (!summary.scripted_movement.path_node_messages.rolling_trace.empty())
    {
        hl::common::Logger::Info("  - path-node rolling trace:");
        for (const std::string& line : summary.scripted_movement.path_node_messages.rolling_trace)
        {
            hl::common::Logger::Info("    * " + line);
        }
    }
    if (!summary.scripted_movement.broken_path_link_events.empty())
    {
        hl::common::Logger::Info("  - broken-link handling:");
        for (const std::string& line : summary.scripted_movement.broken_path_link_events)
        {
            hl::common::Logger::Info("    * " + line);
        }
    }
    if (!summary.scripted_movement.frames.empty())
    {
        hl::common::Logger::Info("  - scripted movement per-frame summary:");
        for (const hl::game_api::ScriptedMovementFrameStateSummary& frame :
             summary.scripted_movement.frames)
        {
            hl::common::Logger::Info(
                "    * frame " + std::to_string(frame.frame_number)
                + " host/server=" + std::to_string(frame.host_frame_index)
                + "/" + std::to_string(frame.server_frame_index)
                + " time=" + std::to_string(frame.time)
                + " moving=" + std::to_string(frame.scenes_moving)
                + " arrivals=" + std::to_string(frame.scene_arrivals)
                + " blocked=" + std::to_string(frame.blocked_scenes)
                + " pathActive=" + std::to_string(frame.path_movers_active)
                + " pathMoving=" + std::to_string(frame.path_movers_moving)
                + " pathStopped=" + std::to_string(frame.stopped_path_movers)
                + " pathBlocked=" + std::to_string(frame.blocked_path_movers)
                + " pathArrivals=" + std::to_string(frame.path_node_arrivals)
                + " pathAdvances=" + std::to_string(frame.path_nodes_advanced)
                + " delayed=" + std::to_string(frame.delayed_actions_due) + "/"
                + std::to_string(frame.delayed_actions_executed) + "/"
                + std::to_string(frame.delayed_actions_pending)
                + " ftruck_a=" + (frame.ftruck_status.empty()
                    ? std::string("<unset>")
                    : frame.ftruck_status));
            if (!frame.path_messages_this_frame.empty())
            {
                hl::common::Logger::Info("      path messages:");
                for (const std::string& line : frame.path_messages_this_frame)
                {
                    hl::common::Logger::Info("        - " + line);
                }
            }
            if (!frame.active_path_movers.empty())
            {
                hl::common::Logger::Info("      active movers:");
                for (const std::string& line : frame.active_path_movers)
                {
                    hl::common::Logger::Info("        - " + line);
                }
            }
            if (!frame.path_events.empty())
            {
                hl::common::Logger::Info("      path events:");
                for (const std::string& line : frame.path_events)
                {
                    hl::common::Logger::Info("        - " + line);
                }
            }
        }
    }
    if (!summary.scripted_movement.exercised_callbacks.empty())
    {
        hl::common::Logger::Info("  - movement/path callbacks exercised:");
        for (const std::string& callback : summary.scripted_movement.exercised_callbacks)
        {
            hl::common::Logger::Info("    * " + callback);
        }
    }
    if (!summary.scripted_movement.path_mover_callbacks_exercised.empty())
    {
        hl::common::Logger::Info("  - path mover callbacks exercised:");
        for (const std::string& callback : summary.scripted_movement.path_mover_callbacks_exercised)
        {
            hl::common::Logger::Info("    * " + callback);
        }
    }

    hl::common::Logger::Info(
        "  - readiness for next step: "
        + (!summary.scripted_movement.readiness.empty()
            ? summary.scripted_movement.readiness
            : !summary.scripted_logic.readiness.empty()
            ? summary.scripted_logic.readiness
            : !summary.map_logic_dispatcher.readiness.empty()
            ? summary.map_logic_dispatcher.readiness
            : !summary.entity_think_scheduler.readiness.empty()
            ? summary.entity_think_scheduler.readiness
            : (!summary.server_frame_loop.readiness.empty()
                ? summary.server_frame_loop.readiness
                : std::string(
                    summary.server_activation.succeeded
                        ? "broader entity lifecycle / start-frame groundwork"
                        : (summary.worldspawn_spawn.succeeded
                            ? "server activation hardening / broader entity lifecycle"
                            : (summary.ready_for_entity_parsing
                                ? "worldspawn bootstrap narrowing / next callback hardening"
                                : "not ready"))))));
}

bool SafeCallGiveFnptrsToDll(
    GiveFnptrsToDllFn function,
    enginefuncs_t* engine_functions,
    globalvars_t* global_variables)
{
    __try
    {
        function(engine_functions, global_variables);
        return true;
    }
    __except (HandleSehException("GiveFnptrsToDll", GetExceptionCode()))
    {
        return false;
    }
}

bool SafeCallGetEntityAPI2(
    GetEntityAPI2Fn function,
    DLL_FUNCTIONS* functions,
    int* interface_version,
    int* result_value)
{
    __try
    {
        *result_value = function(functions, interface_version);
        return true;
    }
    __except (HandleSehException("GetEntityAPI2", GetExceptionCode()))
    {
        return false;
    }
}

bool SafeCallGameInit(void (*function)())
{
    __try
    {
        function();
        return true;
    }
    __except (HandleSehException("pfnGameInit", GetExceptionCode()))
    {
        return false;
    }
}
} // namespace

namespace hl::game_api
{
struct HlServerModule::Impl
{
    DllModule module;
    GiveFnptrsToDllFn give_fnptrs_to_dll = nullptr;
    GetEntityAPI2Fn get_entity_api2 = nullptr;
    EngineShimState shim_state;
    HlServerModuleSummary summary;
};

HlServerModule::HlServerModule()
    : impl_(std::make_unique<Impl>())
{
}

HlServerModule::~HlServerModule()
{
    if (g_active_shim_state == &impl_->shim_state)
    {
        g_active_shim_state = nullptr;
    }
}

bool HlServerModule::Load(const std::filesystem::path& path)
{
    if (g_active_shim_state == &impl_->shim_state)
    {
        g_active_shim_state = nullptr;
    }

    impl_->summary = {};
    impl_->shim_state = EngineShimState{};
    impl_->give_fnptrs_to_dll = nullptr;
    impl_->get_entity_api2 = nullptr;

    common::Logger::Info(
        common::LogCategory::Dll,
        "Loading server module for engine shim: " + common::ToUtf8(path));
    if (!impl_->module.Load(path))
    {
        impl_->summary.loaded_path = path;
        impl_->summary.hl_dll_loaded = false;
        return false;
    }

    impl_->summary.loaded_path = impl_->module.LoadedPath();
    impl_->summary.hl_dll_loaded = true;

    impl_->give_fnptrs_to_dll =
        reinterpret_cast<GiveFnptrsToDllFn>(impl_->module.GetSymbolRaw("GiveFnptrsToDll"));
    impl_->get_entity_api2 =
        reinterpret_cast<GetEntityAPI2Fn>(impl_->module.GetSymbolRaw("GetEntityAPI2"));

    impl_->summary.give_fnptrs_to_dll_export_found = impl_->give_fnptrs_to_dll != nullptr;
    impl_->summary.get_entity_api2_export_found = impl_->get_entity_api2 != nullptr;

    common::Logger::Info(
        common::LogCategory::Dll,
        std::string("hl.dll export GiveFnptrsToDll: ")
        + (impl_->summary.give_fnptrs_to_dll_export_found ? "found" : "missing"));
    common::Logger::Info(
        common::LogCategory::Dll,
        std::string("hl.dll export GetEntityAPI2: ")
        + (impl_->summary.get_entity_api2_export_found ? "found" : "missing"));

    return impl_->summary.give_fnptrs_to_dll_export_found
        && impl_->summary.get_entity_api2_export_found;
}

bool HlServerModule::InitializeEngineShim(const HlServerModuleInitOptions& options)
{
    impl_->summary.give_fnptrs_to_dll_called = false;
    impl_->summary.get_entity_api2_succeeded = false;
    impl_->summary.dll_functions_acquired = false;
    impl_->summary.pfn_game_init_present = false;
    impl_->summary.pfn_game_init_called = false;
    impl_->summary.pfn_game_init_succeeded = false;
    impl_->summary.interface_version_requested = 0;
    impl_->summary.interface_version_reported = 0;
    impl_->summary.dll_functions.clear();
    impl_->summary.invoked_engine_callbacks.clear();
    impl_->summary.registered_cvars = 0;
    impl_->summary.queued_server_commands = 0;
    impl_->summary.executed_server_commands = 0;
    impl_->summary.executed_cfg_files = 0;
    impl_->summary.updated_cvars_from_cfg = 0;
    impl_->summary.auto_created_cvars = 0;
    impl_->summary.executed_cfg_paths.clear();
    impl_->summary.sample_skill_cvars.clear();
    impl_->summary.server_state = {};
    impl_->summary.globals_snapshot = {};
    impl_->summary.precache_callback_invocations = 0;
    impl_->summary.model_callback_invocations = 0;
    impl_->summary.string_callback_invocations = 0;
    impl_->summary.entity_callback_invocations = 0;
    impl_->summary.ready_for_world_bootstrap = false;
    impl_->summary.world_bootstrap = {};
    impl_->summary.ready_for_entity_parsing = false;
    impl_->summary.entity_pipeline = {};
    impl_->summary.worldspawn_spawn = {};
    impl_->summary.server_activation = {};
    impl_->summary.frame_bootstrap_config = {};
    impl_->summary.server_frame_loop = {};
    impl_->summary.entity_think_scheduler = {};
    impl_->summary.map_logic_dispatcher = {};
    impl_->summary.scripted_logic = {};
    impl_->summary.scripted_movement = {};
    impl_->summary.ready_for_server_activation = false;

    if (!impl_->module.IsLoaded())
    {
        common::Logger::Error(
            common::LogCategory::Server,
            "InitializeEngineShim called before hl.dll was loaded.");
        LogServerModuleSummary(impl_->summary);
        return false;
    }

    if (impl_->give_fnptrs_to_dll == nullptr || impl_->get_entity_api2 == nullptr)
    {
        common::Logger::Error(
            common::LogCategory::Server,
            "InitializeEngineShim called without required hl.dll exports.");
        LogServerModuleSummary(impl_->summary);
        return false;
    }

    impl_->shim_state = EngineShimState{};
    impl_->shim_state.module = &impl_->module;
    impl_->shim_state.game_directory = options.game_directory.empty()
        ? impl_->summary.loaded_path.parent_path().parent_path()
        : options.game_directory;
    impl_->shim_state.frame_bootstrap_options = options.frame_bootstrap;
    impl_->shim_state.server_frame_loop_state = {};
    impl_->shim_state.entity_think_scheduler_state = {};
    impl_->shim_state.map_logic_dispatcher_state = {};
    impl_->shim_state.scripted_logic_state = {};
    hl::game_api::detail::InitializeServerState(
        impl_->shim_state.server_state,
        impl_->shim_state.game_directory,
        options.mod_name,
        options.map_name,
        options.hostname,
        options.maxclients);
    SeedBuiltinCvars(impl_->shim_state.cvar_registry, impl_->shim_state.server_state);
    impl_->shim_state.precache_registry.Reset();
    impl_->shim_state.sound_precache_registry.Reset();
    impl_->shim_state.random_diagnostics.Reset();
    impl_->shim_state.generic_precache_registry.Reset();
    impl_->shim_state.event_precache_registry.Reset();
    impl_->shim_state.decal_registry.Reset();
    impl_->shim_state.user_message_registry.Reset();
    impl_->shim_state.server_frame_diagnostics.Reset();
    impl_->shim_state.frame_message_buffer.Reset();
    impl_->shim_state.edict_store.Reset(
        static_cast<std::size_t>(impl_->shim_state.server_state.maxclients),
        512);
    impl_->shim_state.string_pool.Reset();
    PopulateEngineFunctions(impl_->shim_state.enginefuncs);
    PopulateGlobalVariables(
        impl_->shim_state.globalvars,
        impl_->shim_state.string_pool,
        impl_->shim_state.server_state,
        impl_->shim_state.edict_store.MaxEntities());
    g_active_shim_state = &impl_->shim_state;

    common::Logger::Info(
        common::LogCategory::Summary,
        "Frame bootstrap requested: frames="
        + std::to_string(impl_->shim_state.frame_bootstrap_options.frames)
        + ", frametime="
        + std::to_string(impl_->shim_state.frame_bootstrap_options.frametime)
        + ", think_limit="
        + std::to_string(impl_->shim_state.frame_bootstrap_options.think_limit)
        + ", use_limit="
        + std::to_string(impl_->shim_state.frame_bootstrap_options.use_limit)
        + ", scheduled_use_limit="
        + std::to_string(impl_->shim_state.frame_bootstrap_options.scheduled_use_limit)
        + ", path_arrival_epsilon="
        + std::to_string(impl_->shim_state.frame_bootstrap_options.path_arrival_epsilon)
        + ", trace_scripted="
        + std::string(impl_->shim_state.frame_bootstrap_options.trace_scripted ? "1" : "0")
        + ", trace_movement="
        + std::string(impl_->shim_state.frame_bootstrap_options.trace_movement ? "1" : "0")
        + ", trace_think="
        + std::string(impl_->shim_state.frame_bootstrap_options.trace_think ? "1" : "0")
        + ", log_frame_sample="
        + std::to_string(impl_->shim_state.frame_bootstrap_options.log_frame_sample)
        + ", log_state_changes_only="
        + std::string(impl_->shim_state.frame_bootstrap_options.log_state_changes_only ? "1" : "0")
        + ", stop_on_first_message="
        + std::string(impl_->shim_state.frame_bootstrap_options.stop_on_first_message ? "1" : "0")
        + ", stop_on_changelevel_request="
        + std::string(
            impl_->shim_state.frame_bootstrap_options.stop_on_changelevel_request ? "1" : "0")
        + ", stop_on_node="
        + (impl_->shim_state.frame_bootstrap_options.stop_on_node.empty()
            ? std::string("<none>")
            : impl_->shim_state.frame_bootstrap_options.stop_on_node));

    common::Logger::Info(
        common::LogCategory::Dll,
        "Calling GiveFnptrsToDll with minimal engine shim.");
    if (!SafeCallGiveFnptrsToDll(
            impl_->give_fnptrs_to_dll,
            &impl_->shim_state.enginefuncs,
            &impl_->shim_state.globalvars))
    {
        RefreshExecutionSummary(impl_->summary, impl_->shim_state);
        PopulateBootstrapSummary(impl_->summary, impl_->shim_state);
        RefreshInvokedCallbacks(impl_->summary, impl_->shim_state);
        LogServerModuleSummary(impl_->summary);
        return false;
    }

    impl_->summary.give_fnptrs_to_dll_called = true;

    int interface_version = INTERFACE_VERSION;
    int get_entity_api_result = FALSE;
    impl_->summary.interface_version_requested = interface_version;

    common::Logger::Info(common::LogCategory::Dll, "Calling GetEntityAPI2.");
    if (!SafeCallGetEntityAPI2(
            impl_->get_entity_api2,
            &impl_->shim_state.dll_functions,
            &interface_version,
            &get_entity_api_result))
    {
        impl_->summary.interface_version_reported = interface_version;
        RefreshExecutionSummary(impl_->summary, impl_->shim_state);
        PopulateBootstrapSummary(impl_->summary, impl_->shim_state);
        RefreshInvokedCallbacks(impl_->summary, impl_->shim_state);
        LogServerModuleSummary(impl_->summary);
        return false;
    }

    impl_->summary.interface_version_reported = interface_version;
    impl_->summary.get_entity_api2_succeeded = get_entity_api_result != FALSE;
    impl_->summary.dll_functions_acquired = impl_->summary.get_entity_api2_succeeded;

    if (!impl_->summary.get_entity_api2_succeeded)
    {
        common::Logger::Error(
            common::LogCategory::Dll,
            "GetEntityAPI2 failed. Requested interface "
            + std::to_string(impl_->summary.interface_version_requested) + ", hl.dll reported "
            + std::to_string(impl_->summary.interface_version_reported) + ".");
        RefreshExecutionSummary(impl_->summary, impl_->shim_state);
        PopulateBootstrapSummary(impl_->summary, impl_->shim_state);
        RefreshInvokedCallbacks(impl_->summary, impl_->shim_state);
        LogServerModuleSummary(impl_->summary);
        return false;
    }

    impl_->summary.dll_functions = CollectDllFunctionStatuses(impl_->shim_state.dll_functions);
    LogDllFunctionTable(impl_->summary.dll_functions);

    impl_->summary.pfn_game_init_present = impl_->shim_state.dll_functions.pfnGameInit != nullptr;
    common::Logger::Info(
        common::LogCategory::Dll,
        std::string("hl.dll pfnGameInit pointer: ")
        + (impl_->summary.pfn_game_init_present ? "present" : "missing"));

    if (impl_->summary.pfn_game_init_present)
    {
        impl_->summary.pfn_game_init_called = true;
        common::Logger::Info(common::LogCategory::Dll, "Calling hl.dll pfnGameInit().");
        impl_->summary.pfn_game_init_succeeded =
            SafeCallGameInit(impl_->shim_state.dll_functions.pfnGameInit);
    }

    if (!impl_->summary.pfn_game_init_present || impl_->summary.pfn_game_init_succeeded)
    {
        ExecuteQueuedServerCommands(impl_->shim_state, "post-pfnGameInit");
        FinalizeServerBootstrapStep();
    }

    RefreshExecutionSummary(impl_->summary, impl_->shim_state, &impl_->shim_state.command_dispatch_stats);
    impl_->summary.sample_skill_cvars = CollectSkillCvarExamples(
        impl_->shim_state.cvar_registry,
        &impl_->shim_state.command_dispatch_stats);
    PopulateBootstrapSummary(impl_->summary, impl_->shim_state);
    RefreshInvokedCallbacks(impl_->summary, impl_->shim_state);
    LogServerModuleSummary(impl_->summary);

    const bool entity_pipeline_fundamentally_failed =
        impl_->summary.entity_pipeline.attempted
        && !impl_->summary.entity_pipeline.entities_lump_parsed
        && !impl_->summary.entity_pipeline.partially_parsed;

    return impl_->summary.get_entity_api2_succeeded
        && (!impl_->summary.pfn_game_init_present || impl_->summary.pfn_game_init_succeeded)
        && impl_->summary.world_bootstrap.completed
        && !entity_pipeline_fundamentally_failed
        && (!impl_->summary.ready_for_server_activation
            || impl_->summary.server_activation.succeeded)
        && !impl_->summary.server_frame_loop.any_seh;
}

const HlServerModuleSummary& HlServerModule::Summary() const noexcept
{
    return impl_->summary;
}
} // namespace hl::game_api
