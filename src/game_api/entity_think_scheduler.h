#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "entity_var_access.h"
#include "game_api/hl_server_module.h"

namespace hl::game_api::detail
{
using ThinkCallbackCountMap = std::unordered_map<std::string, std::size_t>;

struct EntityThinkSchedulerConfig
{
    int think_limit = 32;
    std::size_t due_preview_limit = 8;
    std::size_t trace_tail_limit = 32;
    std::size_t rolling_trace_limit = 64;
    bool trace_think = false;
};

struct EntityThinkSchedulerHooks
{
    std::function<std::uint64_t()> host_frame_index;
    std::function<std::uint64_t()> server_frame_index;
    std::function<float()> global_time;
    std::function<float()> global_frametime;
    std::function<int()> max_entities;
    std::function<EntityVarSnapshot(int)> inspect_entity;
    std::function<bool(int, int, unsigned int* seh_code)> dispatch_think;
    std::function<void(
        int,
        int,
        float,
        bool,
        std::string_view)> defer_think;
    std::function<void()> sweep_removed_entities;
    std::function<std::vector<std::string>(std::size_t)> trace_tail;
    std::function<std::size_t()> trace_event_count;
    std::function<ThinkCallbackCountMap()> total_callback_counts;
    std::function<void(std::string_view)> log_info;
    std::function<void(std::string_view)> log_warn;
    std::function<void(std::string_view)> log_error;
};

class EntityThinkScheduler
{
public:
    void Configure(
        const EntityThinkSchedulerConfig& config,
        bool start_frame_present,
        bool think_dispatch_present);
    EntityThinkFrameStateSummary RunFrame(
        int frame_number,
        const EntityThinkSchedulerHooks& hooks);

    const EntityThinkSchedulerStateSummary& Summary() const noexcept;

private:
    enum class LifecycleSupportState
    {
        kActiveSupported,
        kPassiveSupported,
        kDetectedButDeferred,
        kRemovedByGameLogic,
    };

    struct DueEntity
    {
        EntityVarSnapshot snapshot;
        LifecycleSupportState lifecycle_state = LifecycleSupportState::kPassiveSupported;
    };

    static std::string SupportStateLabel(LifecycleSupportState state);
    static LifecycleSupportState ClassifyLifecycleState(const EntityVarSnapshot& snapshot);
    static std::string BuildDueEntitySummary(
        const EntityVarSnapshot& snapshot,
        std::string_view decision,
        std::string_view reason);
    static std::vector<InvokedEngineCallback> BuildCallbackDelta(
        const ThinkCallbackCountMap& before,
        const ThinkCallbackCountMap& after);
    static std::string FormatExceptionCode(unsigned int code);

    void ResetCurrentLifecycleCounts();
    void IncrementLifecycleCount(LifecycleSupportState state);
    void IncrementClassState(std::string_view classname, LifecycleSupportState state);
    void IncrementClassDue(std::string_view classname);
    void IncrementClassExecuted(std::string_view classname);
    void IncrementClassDeferred(std::string_view classname);
    void AppendRollingTrace(const std::vector<std::string>& lines);
    void RebuildSummaryViews();
    void Log(
        const std::function<void(std::string_view)>& sink,
        const std::string& message) const;

    EntityThinkSchedulerConfig config_{};
    EntityThinkSchedulerStateSummary summary_{};
    std::unordered_map<std::string, EntityLifecycleClassSummary> class_summary_by_name_;
    std::unordered_map<std::string, std::size_t> lifecycle_counts_by_name_;
    ThinkCallbackCountMap callback_totals_;
};

std::string BuildEntityThinkSchedulerReadiness(const EntityThinkSchedulerStateSummary& summary);
} // namespace hl::game_api::detail
