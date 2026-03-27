#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "entity_var_access.h"
#include "game_api/hl_server_module.h"

#pragma warning(push, 0)
#include "extdll.h"
#pragma warning(pop)

namespace hl::game_api::detail
{
using MapLogicCallbackCountMap = std::unordered_map<std::string, std::size_t>;

struct MapLogicDispatcherConfig
{
    int use_limit = 64;
    int scheduled_use_limit = 64;
    int scheduled_use_total_limit = 4096;
    int scheduled_use_max_reschedules = 8;
    std::size_t action_preview_limit = 8;
    std::size_t trace_tail_limit = 32;
    std::size_t rolling_trace_limit = 64;
};

struct MapLogicDispatchContext
{
    int frame_number = 0;
    int source_edict_index = -1;
    std::string source_classname;
    std::string target_name;
    int use_type = 3;
    float value = 0.0f;
    bool from_scheduled_queue = false;
    std::string reason;
};

struct ScheduledUseAction
{
    std::uint64_t sequence = 0;
    float fire_time = 0.0f;
    int source_edict_index = -1;
    std::string source_classname;
    std::string target_name;
    int use_type = 3;
    float value = 0.0f;
    std::string reason;
    int reschedule_count = 0;
};

enum class MapLogicTargetDispatchResult
{
    kNotHandled,
    kHandled,
    kDeferred,
    kFailed,
};

struct MapLogicDispatcherHooks
{
    std::function<std::uint64_t()> host_frame_index;
    std::function<std::uint64_t()> server_frame_index;
    std::function<float()> global_time;
    std::function<float()> global_frametime;
    std::function<edict_t*(edict_t*, const char*, const char*)> find_entity_by_string;
    std::function<int(const edict_t*)> edict_index_of;
    std::function<int(const edict_t*)> ent_offset_of_pentity;
    std::function<EntityVarSnapshot(int)> inspect_entity;
    std::function<MapLogicSupportState(const EntityVarSnapshot&)> classify_support;
    std::function<bool(const EntityVarSnapshot&)> allow_use;
    std::function<bool(int, int, unsigned int* seh_code)> dispatch_use;
    std::function<MapLogicTargetDispatchResult(
        const MapLogicDispatchContext&,
        const EntityVarSnapshot&,
        std::string* detail)> custom_dispatch_target;
    std::function<void(int)> mark_triggered_this_frame;
    std::function<void(int, std::string_view, std::string_view, int, float)> record_target_emission;
    std::function<void(int)> record_target_resolution;
    std::function<void(int, int, std::string_view, bool, bool, bool, std::string_view)> record_use_result;
    std::function<void(int, int)> set_pending_scheduled_outputs;
    std::function<void(const ScheduledUseAction&, std::string_view, std::string_view)> record_scheduled_action_event;
    std::function<MapLogicCallbackCountMap()> total_callback_counts;
    std::function<void(std::string_view)> alert_ai_console;
    std::function<void(std::string_view)> log_info;
    std::function<void(std::string_view)> log_warn;
    std::function<void(std::string_view)> log_error;
};

class MapLogicDispatcher
{
public:
    void Configure(const MapLogicDispatcherConfig& config, bool use_dispatch_present);
    void BeginFrame(int frame_number, const MapLogicDispatcherHooks& hooks);
    bool DispatchTargetChain(
        const MapLogicDispatchContext& context,
        const MapLogicDispatcherHooks& hooks);
    bool QueueAction(const ScheduledUseAction& action, const MapLogicDispatcherHooks& hooks);
    void ProcessDueQueue(const MapLogicDispatcherHooks& hooks);
    void CompleteFrame(const MapLogicDispatcherHooks& hooks);

    const MapLogicDispatcherStateSummary& Summary() const noexcept;
    const MapLogicFrameStateSummary* CurrentFrameSummary() const noexcept;
    int PendingActionCount() const noexcept;
    std::vector<ScheduledUseAction> QueuedActionsSnapshot() const;

private:
    struct CurrentFrameState
    {
        bool active = false;
        MapLogicFrameStateSummary summary;
        MapLogicCallbackCountMap callbacks_before;
    };

    void Log(
        const std::function<void(std::string_view)>& sink,
        const std::string& message) const;
    void AppendTrace(std::string line);
    void InsertQueuedAction(ScheduledUseAction action);
    void LogActionEvent(
        std::string_view event_name,
        const ScheduledUseAction& action,
        std::string_view detail,
        const MapLogicDispatcherHooks& hooks);
    static bool IsLongDelayAction(
        const ScheduledUseAction& action,
        float current_time,
        float frametime) noexcept;
    void RebuildPendingCounts(const MapLogicDispatcherHooks& hooks);
    static std::vector<InvokedEngineCallback> BuildCallbackDelta(
        const MapLogicCallbackCountMap& before,
        const MapLogicCallbackCountMap& after);
    static std::string DescribeScheduledAction(const ScheduledUseAction& action);
    static const char* UseTypeLabel(int use_type) noexcept;

    MapLogicDispatcherConfig config_{};
    MapLogicDispatcherStateSummary summary_{};
    CurrentFrameState current_frame_{};
    std::vector<ScheduledUseAction> queued_actions_;
    std::unordered_map<int, int> pending_counts_by_source_;
    std::uint64_t next_sequence_ = 1;
};

const char* MapLogicSupportStateLabel(MapLogicSupportState state) noexcept;
std::string BuildMapLogicDispatcherReadiness(const MapLogicDispatcherStateSummary& summary);
} // namespace hl::game_api::detail
