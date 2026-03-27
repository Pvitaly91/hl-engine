#include "entity_think_scheduler.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <sstream>

#pragma warning(push, 0)
#include "extdll.h"
#pragma warning(pop)

namespace
{
using hl::game_api::EntityLifecycleClassSummary;
using hl::game_api::EntityLifecycleStateCountSummary;
using hl::game_api::EntityThinkFrameStateSummary;
using hl::game_api::InvokedEngineCallback;
using hl::game_api::detail::EntityThinkScheduler;
using hl::game_api::detail::EntityThinkSchedulerHooks;
using hl::game_api::detail::EntityVarSnapshot;
using hl::game_api::detail::ThinkCallbackCountMap;

std::string NormalizeClassname(std::string_view classname)
{
    std::string normalized(classname);
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
} // namespace

namespace hl::game_api::detail
{
void EntityThinkScheduler::Configure(
    const EntityThinkSchedulerConfig& config,
    bool start_frame_present,
    bool think_dispatch_present)
{
    config_ = config;
    if (config_.think_limit <= 0)
    {
        config_.think_limit = 32;
    }

    summary_ = {};
    summary_.configured = true;
    summary_.start_frame_present = start_frame_present;
    summary_.think_dispatch_present = think_dispatch_present;
    summary_.think_limit = config_.think_limit;
    class_summary_by_name_.clear();
    lifecycle_counts_by_name_.clear();
    callback_totals_.clear();
    summary_.readiness = BuildEntityThinkSchedulerReadiness(summary_);
}

EntityThinkFrameStateSummary EntityThinkScheduler::RunFrame(
    int frame_number,
    const EntityThinkSchedulerHooks& hooks)
{
    EntityThinkFrameStateSummary frame;
    frame.frame_number = frame_number;
    if (hooks.host_frame_index)
    {
        frame.host_frame_index = hooks.host_frame_index();
    }
    if (hooks.server_frame_index)
    {
        frame.server_frame_index = hooks.server_frame_index();
    }
    if (hooks.global_time)
    {
        frame.time = hooks.global_time();
    }
    if (hooks.global_frametime)
    {
        frame.frametime = hooks.global_frametime();
    }

    summary_.attempted = true;
    ++summary_.frames_attempted;

    const ThinkCallbackCountMap before =
        hooks.total_callback_counts ? hooks.total_callback_counts() : ThinkCallbackCountMap{};

    std::vector<DueEntity> due_entities;
    const int max_entities = hooks.max_entities ? hooks.max_entities() : 0;
    due_entities.reserve(static_cast<std::size_t>(std::max(max_entities, 0)));

    for (int edict_index = 0; edict_index < max_entities; ++edict_index)
    {
        const EntityVarSnapshot snapshot =
            hooks.inspect_entity ? hooks.inspect_entity(edict_index) : EntityVarSnapshot{};
        if (!snapshot.valid)
        {
            continue;
        }

        const bool due =
            snapshot.in_use
            && !snapshot.removed
            && (snapshot.flags & FL_KILLME) == 0
            && snapshot.nextthink > 0.0f
            && snapshot.nextthink <= frame.time;
        if (!due)
        {
            continue;
        }

        const LifecycleSupportState lifecycle_state = ClassifyLifecycleState(snapshot);
        due_entities.push_back({snapshot, lifecycle_state});
        ++frame.due_thinks;
        ++summary_.total_due_thinks;
        IncrementClassDue(snapshot.classname);
    }

    if (due_entities.empty())
    {
        if (config_.trace_think)
        {
            Log(
                hooks.log_info,
                "EntityThinkScheduler: frame " + std::to_string(frame_number)
                    + " found no due thinks at time " + std::to_string(frame.time) + ".");
        }
    }
    else if (config_.trace_think)
    {
        Log(
            hooks.log_info,
            "EntityThinkScheduler: frame " + std::to_string(frame_number)
                + " due scan found " + std::to_string(frame.due_thinks)
                + " entities at time " + std::to_string(frame.time)
                + " [think_limit=" + std::to_string(config_.think_limit) + "].");
    }

    int executed_attempts = 0;
    for (const DueEntity& due : due_entities)
    {
        const EntityVarSnapshot& snapshot = due.snapshot;
        const std::string classname =
            snapshot.classname.empty() ? std::string("<empty>") : snapshot.classname;

        std::string decision = "deferred";
        std::string reason = "unknown";
        bool success = false;
        bool failure = false;
        unsigned int seh_code = 0;

        if (executed_attempts >= config_.think_limit)
        {
            reason = "think limit reached";
            if (hooks.defer_think)
            {
                hooks.defer_think(
                    snapshot.edict_index,
                    frame_number,
                    frame.time + std::max(frame.frametime, 0.05f),
                    true,
                    reason);
            }
            ++frame.deferred_thinks;
            ++summary_.total_deferred_thinks;
            IncrementClassDeferred(classname);
            Log(
                hooks.log_warn,
                config_.trace_think
                    ? "EntityThinkScheduler: due edict#" + std::to_string(snapshot.edict_index)
                        + " classname=" + classname
                        + " nextthink=" + std::to_string(snapshot.nextthink)
                        + " now=" + std::to_string(frame.time)
                        + " decision=deferred reason=" + reason + "."
                    : "EntityThinkScheduler: deferred classname=" + classname
                        + " reason=" + reason + ".");
        }
        else if (!summary_.think_dispatch_present)
        {
            reason = "pfnThink missing";
            if (hooks.defer_think)
            {
                hooks.defer_think(snapshot.edict_index, frame_number, 0.0f, false, reason);
            }
            ++frame.deferred_thinks;
            ++summary_.total_deferred_thinks;
            IncrementClassDeferred(classname);
            Log(
                hooks.log_warn,
                config_.trace_think
                    ? "EntityThinkScheduler: due edict#" + std::to_string(snapshot.edict_index)
                        + " classname=" + classname
                        + " nextthink=" + std::to_string(snapshot.nextthink)
                        + " now=" + std::to_string(frame.time)
                        + " decision=deferred reason=" + reason + "."
                    : "EntityThinkScheduler: deferred classname=" + classname
                        + " reason=" + reason + ".");
        }
        else if (due.lifecycle_state == LifecycleSupportState::kDetectedButDeferred)
        {
            reason = "class deferred by safe lifecycle policy";
            if (hooks.defer_think)
            {
                hooks.defer_think(snapshot.edict_index, frame_number, 0.0f, false, reason);
            }
            ++frame.deferred_thinks;
            ++summary_.total_deferred_thinks;
            IncrementClassDeferred(classname);
            Log(
                hooks.log_warn,
                config_.trace_think
                    ? "EntityThinkScheduler: due edict#" + std::to_string(snapshot.edict_index)
                        + " classname=" + classname
                        + " nextthink=" + std::to_string(snapshot.nextthink)
                        + " now=" + std::to_string(frame.time)
                        + " decision=deferred reason=" + reason + "."
                    : "EntityThinkScheduler: deferred classname=" + classname
                        + " reason=" + reason + ".");
        }
        else if (!snapshot.has_private_data)
        {
            reason = "private data unavailable";
            if (hooks.defer_think)
            {
                hooks.defer_think(snapshot.edict_index, frame_number, 0.0f, false, reason);
            }
            ++frame.deferred_thinks;
            ++summary_.total_deferred_thinks;
            IncrementClassDeferred(classname);
            Log(
                hooks.log_warn,
                config_.trace_think
                    ? "EntityThinkScheduler: due edict#" + std::to_string(snapshot.edict_index)
                        + " classname=" + classname
                        + " nextthink=" + std::to_string(snapshot.nextthink)
                        + " now=" + std::to_string(frame.time)
                        + " decision=deferred reason=" + reason + "."
                    : "EntityThinkScheduler: deferred classname=" + classname
                        + " reason=" + reason + ".");
        }
        else
        {
            decision = "executed";
            reason = "dispatch";
            ++executed_attempts;
            if (config_.trace_think)
            {
                Log(
                    hooks.log_info,
                    "EntityThinkScheduler: due edict#" + std::to_string(snapshot.edict_index)
                        + " classname=" + classname
                        + " nextthink=" + std::to_string(snapshot.nextthink)
                        + " now=" + std::to_string(frame.time)
                        + " decision=execute.");
            }

            success = hooks.dispatch_think
                ? hooks.dispatch_think(snapshot.edict_index, frame_number, &seh_code)
                : false;
            if (success)
            {
                ++frame.executed_thinks;
                ++summary_.total_executed_thinks;
                IncrementClassExecuted(classname);
                if (config_.trace_think)
                {
                    Log(
                        hooks.log_info,
                        "EntityThinkScheduler: think ok edict#"
                            + std::to_string(snapshot.edict_index)
                            + " classname=" + classname + ".");
                }
            }
            else
            {
                failure = true;
                reason = "SEH " + FormatExceptionCode(seh_code);
                ++frame.think_failures;
                ++frame.seh_failures;
                ++summary_.total_think_failures;
                ++summary_.total_seh_failures;
                Log(
                    hooks.log_error,
                    "EntityThinkScheduler: think failed edict#"
                        + std::to_string(snapshot.edict_index)
                        + " classname=" + classname
                        + " seh=" + FormatExceptionCode(seh_code) + ".");
                if (hooks.trace_tail)
                {
                    const std::vector<std::string> failure_trace =
                        hooks.trace_tail(config_.trace_tail_limit);
                    if (!failure_trace.empty())
                    {
                        Log(hooks.log_info, "EntityThinkScheduler: failure trace tail:");
                        for (const std::string& line : failure_trace)
                        {
                            Log(hooks.log_info, "  - " + line);
                        }
                    }
                }
            }
        }

        if (frame.due_entities_preview.size() < config_.due_preview_limit)
        {
            frame.due_entities_preview.push_back(
                BuildDueEntitySummary(snapshot, decision, reason));
        }

        const EntityVarSnapshot post_snapshot =
            hooks.inspect_entity ? hooks.inspect_entity(snapshot.edict_index) : EntityVarSnapshot{};
        if (post_snapshot.valid
            && (!post_snapshot.in_use
                || post_snapshot.removed
                || (post_snapshot.flags & FL_KILLME) != 0))
        {
            ++frame.removed_by_game_logic;
            ++summary_.total_removed_by_game_logic;
        }

        (void)failure;
    }

    if (hooks.sweep_removed_entities)
    {
        hooks.sweep_removed_entities();
    }

    ResetCurrentLifecycleCounts();
    for (int edict_index = 0; edict_index < max_entities; ++edict_index)
    {
        const EntityVarSnapshot snapshot =
            hooks.inspect_entity ? hooks.inspect_entity(edict_index) : EntityVarSnapshot{};
        if (!snapshot.valid)
        {
            continue;
        }

        if (snapshot.in_use && !snapshot.removed && (snapshot.flags & FL_KILLME) == 0)
        {
            ++frame.active_entities;
        }

        const LifecycleSupportState lifecycle_state = ClassifyLifecycleState(snapshot);
        IncrementLifecycleCount(lifecycle_state);
        IncrementClassState(snapshot.classname, lifecycle_state);
    }

    const ThinkCallbackCountMap after =
        hooks.total_callback_counts ? hooks.total_callback_counts() : ThinkCallbackCountMap{};
    frame.callback_counts_this_frame = BuildCallbackDelta(before, after);
    for (const InvokedEngineCallback& callback : frame.callback_counts_this_frame)
    {
        callback_totals_[callback.name] += callback.call_count;
    }

    if (hooks.trace_tail)
    {
        frame.callback_trace_tail = hooks.trace_tail(config_.trace_tail_limit);
        AppendRollingTrace(frame.callback_trace_tail);
    }

    ++summary_.frames_completed;
    summary_.frames.push_back(frame);
    RebuildSummaryViews();
    summary_.readiness = BuildEntityThinkSchedulerReadiness(summary_);

    if (config_.trace_think && frame.callback_counts_this_frame.empty())
    {
        Log(hooks.log_info, "EntityThinkScheduler: callback delta this frame: none");
    }
    else if (config_.trace_think)
    {
        Log(hooks.log_info, "EntityThinkScheduler: callback delta this frame:");
        for (const InvokedEngineCallback& callback : frame.callback_counts_this_frame)
        {
            Log(
                hooks.log_info,
                "  - " + callback.name + " x" + std::to_string(callback.call_count));
        }
    }

    if (config_.trace_think && !frame.due_entities_preview.empty())
    {
        Log(hooks.log_info, "EntityThinkScheduler: due entity preview:");
        for (const std::string& line : frame.due_entities_preview)
        {
            Log(hooks.log_info, "  - " + line);
        }
    }

    return frame;
}

const EntityThinkSchedulerStateSummary& EntityThinkScheduler::Summary() const noexcept
{
    return summary_;
}

std::string EntityThinkScheduler::SupportStateLabel(LifecycleSupportState state)
{
    switch (state)
    {
    case LifecycleSupportState::kActiveSupported:
        return "active supported";
    case LifecycleSupportState::kPassiveSupported:
        return "passive supported";
    case LifecycleSupportState::kDetectedButDeferred:
        return "detected but deferred";
    case LifecycleSupportState::kRemovedByGameLogic:
        return "removed by game logic";
    default:
        return "unknown";
    }
}

EntityThinkScheduler::LifecycleSupportState EntityThinkScheduler::ClassifyLifecycleState(
    const EntityVarSnapshot& snapshot)
{
    if (!snapshot.valid)
    {
        return LifecycleSupportState::kDetectedButDeferred;
    }

    if (!snapshot.in_use || snapshot.removed || (snapshot.flags & FL_KILLME) != 0)
    {
        return LifecycleSupportState::kRemovedByGameLogic;
    }

    if (snapshot.deferred)
    {
        return LifecycleSupportState::kDetectedButDeferred;
    }

    const std::string normalized = NormalizeClassname(snapshot.classname);
    static constexpr std::array<std::string_view, 3> kActiveClasses = {{
        "trigger_auto",
        "multi_manager",
        "env_message",
    }};
    static constexpr std::array<std::string_view, 8> kPassiveClasses = {{
        "worldspawn",
        "func_wall",
        "env_glow",
        "light",
        "light_spot",
        "path_track",
        "info_player_start",
        "info_player_deathmatch",
    }};

    if (std::find(kActiveClasses.begin(), kActiveClasses.end(), normalized) != kActiveClasses.end())
    {
        return LifecycleSupportState::kActiveSupported;
    }

    if (std::find(kPassiveClasses.begin(), kPassiveClasses.end(), normalized) != kPassiveClasses.end())
    {
        return LifecycleSupportState::kPassiveSupported;
    }

    if (normalized == "scripted_sequence" || normalized.rfind("scripted_", 0) == 0)
    {
        return LifecycleSupportState::kDetectedButDeferred;
    }

    return snapshot.scheduled_for_think
        ? LifecycleSupportState::kDetectedButDeferred
        : LifecycleSupportState::kPassiveSupported;
}

std::string EntityThinkScheduler::BuildDueEntitySummary(
    const EntityVarSnapshot& snapshot,
    std::string_view decision,
    std::string_view reason)
{
    return "edict#" + std::to_string(snapshot.edict_index)
        + " classname=" + (snapshot.classname.empty() ? std::string("<empty>") : snapshot.classname)
        + " targetname=" + (snapshot.targetname.empty() ? std::string("<empty>") : snapshot.targetname)
        + " nextthink=" + std::to_string(snapshot.nextthink)
        + " decision=" + std::string(decision)
        + " reason=" + std::string(reason);
}

std::vector<InvokedEngineCallback> EntityThinkScheduler::BuildCallbackDelta(
    const ThinkCallbackCountMap& before,
    const ThinkCallbackCountMap& after)
{
    std::vector<InvokedEngineCallback> delta;
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
        [](const InvokedEngineCallback& left, const InvokedEngineCallback& right)
        {
            if (left.call_count != right.call_count)
            {
                return left.call_count > right.call_count;
            }

            return left.name < right.name;
        });
    return delta;
}

std::string EntityThinkScheduler::FormatExceptionCode(unsigned int code)
{
    std::ostringstream stream;
    stream << "0x" << std::hex << std::uppercase << code;
    return stream.str();
}

void EntityThinkScheduler::ResetCurrentLifecycleCounts()
{
    lifecycle_counts_by_name_.clear();
    for (auto& [classname, summary] : class_summary_by_name_)
    {
        (void)classname;
        summary.active_supported = 0;
        summary.passive_supported = 0;
        summary.detected_but_deferred = 0;
        summary.removed_by_game_logic = 0;
    }
}

void EntityThinkScheduler::IncrementLifecycleCount(LifecycleSupportState state)
{
    ++lifecycle_counts_by_name_[SupportStateLabel(state)];
}

void EntityThinkScheduler::IncrementClassState(
    std::string_view classname,
    LifecycleSupportState state)
{
    const std::string key = classname.empty() ? std::string("<empty>") : std::string(classname);
    EntityLifecycleClassSummary& summary = class_summary_by_name_[key];
    summary.classname = key;

    switch (state)
    {
    case LifecycleSupportState::kActiveSupported:
        ++summary.active_supported;
        break;
    case LifecycleSupportState::kPassiveSupported:
        ++summary.passive_supported;
        break;
    case LifecycleSupportState::kDetectedButDeferred:
        ++summary.detected_but_deferred;
        break;
    case LifecycleSupportState::kRemovedByGameLogic:
        ++summary.removed_by_game_logic;
        break;
    default:
        break;
    }
}

void EntityThinkScheduler::IncrementClassDue(std::string_view classname)
{
    const std::string key = classname.empty() ? std::string("<empty>") : std::string(classname);
    EntityLifecycleClassSummary& summary = class_summary_by_name_[key];
    summary.classname = key;
    ++summary.due_thinks;
}

void EntityThinkScheduler::IncrementClassExecuted(std::string_view classname)
{
    const std::string key = classname.empty() ? std::string("<empty>") : std::string(classname);
    EntityLifecycleClassSummary& summary = class_summary_by_name_[key];
    summary.classname = key;
    ++summary.executed_thinks;
}

void EntityThinkScheduler::IncrementClassDeferred(std::string_view classname)
{
    const std::string key = classname.empty() ? std::string("<empty>") : std::string(classname);
    EntityLifecycleClassSummary& summary = class_summary_by_name_[key];
    summary.classname = key;
    ++summary.deferred_thinks;
}

void EntityThinkScheduler::AppendRollingTrace(const std::vector<std::string>& lines)
{
    for (const std::string& line : lines)
    {
        summary_.rolling_trace_tail.push_back(line);
        if (summary_.rolling_trace_tail.size() > config_.rolling_trace_limit)
        {
            summary_.rolling_trace_tail.erase(summary_.rolling_trace_tail.begin());
        }
    }
}

void EntityThinkScheduler::RebuildSummaryViews()
{
    summary_.active_entity_counts_by_support_state.clear();
    static constexpr std::array<std::string_view, 4> kSupportOrder = {{
        "active supported",
        "passive supported",
        "detected but deferred",
        "removed by game logic",
    }};

    for (const std::string_view label : kSupportOrder)
    {
        const auto it = lifecycle_counts_by_name_.find(std::string(label));
        summary_.active_entity_counts_by_support_state.push_back({
            std::string(label),
            it != lifecycle_counts_by_name_.end() ? it->second : 0,
        });
    }

    summary_.classname_lifecycle_summary.clear();
    summary_.classname_lifecycle_summary.reserve(class_summary_by_name_.size());
    for (const auto& [classname, summary] : class_summary_by_name_)
    {
        (void)classname;
        summary_.classname_lifecycle_summary.push_back(summary);
    }

    std::sort(
        summary_.classname_lifecycle_summary.begin(),
        summary_.classname_lifecycle_summary.end(),
        [](const EntityLifecycleClassSummary& left, const EntityLifecycleClassSummary& right)
        {
            const std::size_t left_weight =
                left.executed_thinks + left.due_thinks + left.active_supported;
            const std::size_t right_weight =
                right.executed_thinks + right.due_thinks + right.active_supported;
            if (left_weight != right_weight)
            {
                return left_weight > right_weight;
            }

            return left.classname < right.classname;
        });

    summary_.callbacks_during_scheduler.clear();
    summary_.callbacks_during_scheduler.reserve(callback_totals_.size());
    for (const auto& [name, count] : callback_totals_)
    {
        summary_.callbacks_during_scheduler.push_back({name, count});
    }

    std::sort(
        summary_.callbacks_during_scheduler.begin(),
        summary_.callbacks_during_scheduler.end(),
        [](const InvokedEngineCallback& left, const InvokedEngineCallback& right)
        {
            if (left.call_count != right.call_count)
            {
                return left.call_count > right.call_count;
            }

            return left.name < right.name;
        });
}

void EntityThinkScheduler::Log(
    const std::function<void(std::string_view)>& sink,
    const std::string& message) const
{
    if (sink)
    {
        sink(message);
    }
}

std::string BuildEntityThinkSchedulerReadiness(const EntityThinkSchedulerStateSummary& summary)
{
    if (!summary.start_frame_present)
    {
        return "scheduled think waits for a successful StartFrame-capable frame loop";
    }

    if (!summary.attempted)
    {
        return "scheduled think is wired and will run after the next successful StartFrame";
    }

    if (summary.total_seh_failures > 0)
    {
        return "scheduled think is isolated; inspect failing entity traces before broadening lifecycle support";
    }

    if (summary.total_due_thinks == 0)
    {
        return "scheduled think is stable; expand early logic coverage and use-target diagnostics";
    }

    if (summary.total_executed_thinks > 0 && summary.total_deferred_thinks == 0)
    {
        return "ready for broader entity logic, use-target chains, and later player/server groundwork";
    }

    if (summary.total_executed_thinks > 0)
    {
        return "scheduled think is progressing; harden deferred classes and missing callback paths next";
    }

    return "due thinks are detected but deferred; implement the missing safe dispatch path before broader simulation";
}
} // namespace hl::game_api::detail
