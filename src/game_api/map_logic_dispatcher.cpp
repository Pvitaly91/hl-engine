#include "map_logic_dispatcher.h"

#include <algorithm>
#include <sstream>
#include <utility>

namespace hl::game_api::detail
{
namespace
{
std::string DescribeTarget(
    const EntityVarSnapshot& snapshot,
    int offset,
    MapLogicSupportState support_state)
{
    std::ostringstream stream;
    stream << "edict#" << snapshot.edict_index
           << " classname="
           << (snapshot.classname.empty() ? "<empty>" : snapshot.classname)
           << " targetname="
           << (snapshot.targetname.empty() ? "<empty>" : snapshot.targetname)
           << " offset=" << offset
           << " support=" << MapLogicSupportStateLabel(support_state);
    return stream.str();
}

bool ActionOrderLess(const ScheduledUseAction& left, const ScheduledUseAction& right)
{
    if (left.fire_time != right.fire_time)
    {
        return left.fire_time < right.fire_time;
    }

    return left.sequence < right.sequence;
}
} // namespace

const char* MapLogicSupportStateLabel(MapLogicSupportState state) noexcept
{
    switch (state)
    {
    case MapLogicSupportState::kUseSupported:
        return "use-supported";
    case MapLogicSupportState::kPassiveRecipientOnly:
        return "passive-recipient-only";
    case MapLogicSupportState::kDetectedButDeferred:
        return "detected-but-deferred";
    case MapLogicSupportState::kRemovedByGameLogic:
        return "removed-by-game-logic";
    default:
        return "unknown";
    }
}

void MapLogicDispatcher::Configure(
    const MapLogicDispatcherConfig& config,
    bool use_dispatch_present)
{
    config_ = config;
    if (config_.use_limit <= 0)
    {
        config_.use_limit = 64;
    }
    if (config_.scheduled_use_limit <= 0)
    {
        config_.scheduled_use_limit = 64;
    }
    if (config_.scheduled_use_total_limit <= 0)
    {
        config_.scheduled_use_total_limit = 4096;
    }
    if (config_.scheduled_use_max_reschedules <= 0)
    {
        config_.scheduled_use_max_reschedules = 8;
    }
    if (config_.action_preview_limit == 0)
    {
        config_.action_preview_limit = 8;
    }
    if (config_.trace_tail_limit == 0)
    {
        config_.trace_tail_limit = 32;
    }
    if (config_.rolling_trace_limit == 0)
    {
        config_.rolling_trace_limit = 64;
    }

    summary_ = {};
    summary_.configured = true;
    summary_.use_dispatch_present = use_dispatch_present;
    summary_.use_limit = config_.use_limit;
    summary_.scheduled_use_limit = config_.scheduled_use_limit;
    queued_actions_.clear();
    pending_counts_by_source_.clear();
    next_sequence_ = 1;
    current_frame_ = {};
}

void MapLogicDispatcher::BeginFrame(int frame_number, const MapLogicDispatcherHooks& hooks)
{
    current_frame_ = {};
    current_frame_.active = true;
    current_frame_.summary.frame_number = frame_number;
    current_frame_.summary.host_frame_index =
        hooks.host_frame_index ? hooks.host_frame_index() : 0;
    current_frame_.summary.server_frame_index =
        hooks.server_frame_index ? hooks.server_frame_index() : 0;
    current_frame_.summary.time = hooks.global_time ? hooks.global_time() : 0.0f;
    current_frame_.summary.frametime = hooks.global_frametime ? hooks.global_frametime() : 0.0f;
    current_frame_.summary.queue_size_before = static_cast<int>(queued_actions_.size());
    current_frame_.summary.scheduled_pending = current_frame_.summary.queue_size_before;
    current_frame_.summary.long_delay_pending = 0;
    for (const ScheduledUseAction& action : queued_actions_)
    {
        if (IsLongDelayAction(
                action,
                current_frame_.summary.time,
                current_frame_.summary.frametime))
        {
            ++current_frame_.summary.long_delay_pending;
            if (current_frame_.summary.scheduled_action_preview.size() < config_.action_preview_limit)
            {
                current_frame_.summary.scheduled_action_preview.push_back(
                    "pending " + DescribeScheduledAction(action));
            }
        }
    }
    if (hooks.total_callback_counts)
    {
        current_frame_.callbacks_before = hooks.total_callback_counts();
    }

    summary_.attempted = true;
    ++summary_.frames_attempted;

    AppendTrace(
        "frame " + std::to_string(frame_number)
        + " begin host/server=" + std::to_string(current_frame_.summary.host_frame_index)
        + "/" + std::to_string(current_frame_.summary.server_frame_index)
        + " time=" + std::to_string(current_frame_.summary.time)
        + " frametime=" + std::to_string(current_frame_.summary.frametime)
        + " queueBefore=" + std::to_string(current_frame_.summary.queue_size_before)
        + " longDelayPending=" + std::to_string(current_frame_.summary.long_delay_pending));
}

bool MapLogicDispatcher::DispatchTargetChain(
    const MapLogicDispatchContext& context,
    const MapLogicDispatcherHooks& hooks)
{
    if (!current_frame_.active || context.target_name.empty())
    {
        return false;
    }

    if (hooks.mark_triggered_this_frame && context.source_edict_index >= 0)
    {
        hooks.mark_triggered_this_frame(context.source_edict_index);
    }
    if (hooks.record_target_emission && context.source_edict_index >= 0)
    {
        hooks.record_target_emission(
            context.source_edict_index,
            context.source_classname,
            context.target_name,
            context.use_type,
            context.value);
    }

    if (hooks.alert_ai_console)
    {
        hooks.alert_ai_console("Firing: (" + context.target_name + ")");
    }

    ++current_frame_.summary.target_chains_fired;
    ++summary_.total_target_chains_fired;

    std::vector<edict_t*> resolved_entities;
    edict_t* cursor = nullptr;
    while (hooks.find_entity_by_string)
    {
        edict_t* found =
            hooks.find_entity_by_string(cursor, "targetname", context.target_name.c_str());
        if (found == nullptr)
        {
            break;
        }

        resolved_entities.push_back(found);
        cursor = found;
    }

    std::string chain_header =
        "MapLogicDispatcher: chain source=edict#"
        + std::to_string(context.source_edict_index)
        + " classname="
        + (context.source_classname.empty() ? std::string("<empty>") : context.source_classname)
        + " target=" + context.target_name
        + " useType=" + std::string(UseTypeLabel(context.use_type))
        + " value=" + std::to_string(context.value)
        + " mode=" + (context.from_scheduled_queue ? "scheduled" : "immediate");
    Log(hooks.log_info, chain_header);
    AppendTrace(chain_header);

    if (resolved_entities.empty())
    {
        ++summary_.total_no_targets_found;
        const std::string message =
            "MapLogicDispatcher: no targets found for '" + context.target_name + "'.";
        Log(hooks.log_warn, message);
        AppendTrace(message);
        return true;
    }

    if (resolved_entities.size() == 1)
    {
        ++summary_.total_single_target_hits;
        Log(
            hooks.log_info,
            "MapLogicDispatcher: one target resolved for '" + context.target_name + "'.");
    }
    else
    {
        ++summary_.total_multi_target_hits;
        Log(
            hooks.log_info,
            "MapLogicDispatcher: multiple targets resolved for '" + context.target_name + "': "
                + std::to_string(resolved_entities.size()) + ".");
    }

    current_frame_.summary.target_resolutions += static_cast<int>(resolved_entities.size());
    summary_.total_target_resolutions += static_cast<int>(resolved_entities.size());

    for (edict_t* entity : resolved_entities)
    {
        const int edict_index = hooks.edict_index_of ? hooks.edict_index_of(entity) : -1;
        const int offset = hooks.ent_offset_of_pentity ? hooks.ent_offset_of_pentity(entity) : 0;
        const EntityVarSnapshot target_snapshot =
            hooks.inspect_entity ? hooks.inspect_entity(edict_index) : EntityVarSnapshot{};
        const MapLogicSupportState support_state =
            hooks.classify_support
            ? hooks.classify_support(target_snapshot)
            : MapLogicSupportState::kDetectedButDeferred;

        if (hooks.mark_triggered_this_frame && edict_index >= 0)
        {
            hooks.mark_triggered_this_frame(edict_index);
        }
        if (hooks.record_target_resolution && edict_index >= 0)
        {
            hooks.record_target_resolution(edict_index);
        }

        const std::string target_summary = DescribeTarget(target_snapshot, offset, support_state);
        Log(hooks.log_info, "MapLogicDispatcher: resolved " + target_summary);
        AppendTrace("resolved " + target_summary);

        if (hooks.alert_ai_console)
        {
            hooks.alert_ai_console(
                "Found: "
                + (target_snapshot.classname.empty()
                    ? std::string("<empty>")
                    : target_snapshot.classname)
                + ", dispatching (" + context.target_name + ")");
        }

        std::string detail;
        const MapLogicTargetDispatchResult custom_result =
            hooks.custom_dispatch_target
            ? hooks.custom_dispatch_target(context, target_snapshot, &detail)
            : MapLogicTargetDispatchResult::kNotHandled;

        if (custom_result == MapLogicTargetDispatchResult::kHandled)
        {
            const std::string message =
                "MapLogicDispatcher: custom dispatch handled edict#"
                + std::to_string(edict_index)
                + (detail.empty() ? std::string() : " [" + detail + "]");
            Log(hooks.log_info, message);
            AppendTrace(message);
            continue;
        }

        if (custom_result == MapLogicTargetDispatchResult::kDeferred)
        {
            ++current_frame_.summary.use_deferred;
            ++summary_.total_use_deferred;
            if (hooks.record_use_result && edict_index >= 0)
            {
                const std::string deferred_detail = detail.empty()
                    ? std::string("custom dispatch deferred")
                    : std::string("custom dispatch deferred: ") + detail;
                hooks.record_use_result(
                    edict_index,
                    context.source_edict_index,
                    context.source_classname,
                    false,
                    false,
                    true,
                    deferred_detail);
            }
            const std::string message =
                "MapLogicDispatcher: custom dispatch deferred edict#"
                + std::to_string(edict_index)
                + (detail.empty() ? std::string() : " [" + detail + "]");
            Log(hooks.log_warn, message);
            AppendTrace(message);
            continue;
        }

        if (custom_result == MapLogicTargetDispatchResult::kFailed)
        {
            const std::string message =
                "MapLogicDispatcher: custom dispatch failed edict#"
                + std::to_string(edict_index)
                + (detail.empty() ? std::string() : " [" + detail + "]");
            Log(hooks.log_error, message);
            AppendTrace(message);
            continue;
        }

        if (support_state == MapLogicSupportState::kRemovedByGameLogic)
        {
            ++current_frame_.summary.use_deferred;
            ++summary_.total_use_deferred;
            if (hooks.record_use_result && edict_index >= 0)
            {
                hooks.record_use_result(
                    edict_index,
                    context.source_edict_index,
                    context.source_classname,
                    false,
                    false,
                    true,
                    "target removed");
            }
            const std::string message =
                "MapLogicDispatcher: deferred removed target edict#"
                + std::to_string(edict_index) + ".";
            Log(hooks.log_warn, message);
            AppendTrace(message);
            continue;
        }

        if (!hooks.allow_use || !hooks.allow_use(target_snapshot))
        {
            ++current_frame_.summary.use_deferred;
            ++summary_.total_use_deferred;
            if (hooks.record_use_result && edict_index >= 0)
            {
                hooks.record_use_result(
                    edict_index,
                    context.source_edict_index,
                    context.source_classname,
                    false,
                    false,
                    true,
                    "allow_use rejected");
            }
            const std::string message =
                "MapLogicDispatcher: deferred use for edict#"
                + std::to_string(edict_index)
                + " support=" + MapLogicSupportStateLabel(support_state) + ".";
            Log(hooks.log_warn, message);
            AppendTrace(message);
            continue;
        }

        if (!summary_.use_dispatch_present || !hooks.dispatch_use)
        {
            ++current_frame_.summary.use_deferred;
            ++summary_.total_use_deferred;
            if (hooks.record_use_result && edict_index >= 0)
            {
                hooks.record_use_result(
                    edict_index,
                    context.source_edict_index,
                    context.source_classname,
                    false,
                    false,
                    true,
                    "pfnUse unavailable");
            }
            const std::string message =
                "MapLogicDispatcher: deferred use because pfnUse is unavailable for edict#"
                + std::to_string(edict_index) + ".";
            Log(hooks.log_warn, message);
            AppendTrace(message);
            continue;
        }

        if (current_frame_.summary.use_attempts >= config_.use_limit)
        {
            ++current_frame_.summary.use_deferred;
            ++summary_.total_use_deferred;
            if (hooks.record_use_result && edict_index >= 0)
            {
                hooks.record_use_result(
                    edict_index,
                    context.source_edict_index,
                    context.source_classname,
                    false,
                    false,
                    true,
                    "frame use limit reached");
            }
            const std::string message =
                "MapLogicDispatcher: deferred use because frame use limit was reached for edict#"
                + std::to_string(edict_index) + ".";
            Log(hooks.log_warn, message);
            AppendTrace(message);
            continue;
        }

        unsigned int seh_code = 0;
        ++current_frame_.summary.use_attempts;
        ++summary_.total_use_attempts;
        const bool success =
            hooks.dispatch_use(edict_index, context.source_edict_index, &seh_code);
        if (success)
        {
            ++current_frame_.summary.use_successes;
            ++summary_.total_use_successes;
            if (hooks.record_use_result && edict_index >= 0)
            {
                hooks.record_use_result(
                    edict_index,
                    context.source_edict_index,
                    context.source_classname,
                    true,
                    true,
                    false,
                    "pfnUse succeeded");
            }
            const std::string message =
                "MapLogicDispatcher: pfnUse succeeded for edict#"
                + std::to_string(edict_index) + ".";
            Log(hooks.log_info, message);
            AppendTrace(message);
        }
        else
        {
            ++current_frame_.summary.use_failures;
            ++summary_.total_use_failures;
            if (hooks.record_use_result && edict_index >= 0)
            {
                hooks.record_use_result(
                    edict_index,
                    context.source_edict_index,
                    context.source_classname,
                    true,
                    false,
                    false,
                    "pfnUse failed");
            }
            if (seh_code != 0)
            {
                ++current_frame_.summary.use_seh_failures;
                ++summary_.total_use_seh_failures;
            }

            const std::string message =
                "MapLogicDispatcher: pfnUse failed for edict#"
                + std::to_string(edict_index)
                + (seh_code != 0
                    ? std::string(" seh=0x") + [&]()
                    {
                        std::ostringstream stream;
                        stream << std::hex << std::uppercase << seh_code;
                        return stream.str();
                    }()
                    : std::string());
            Log(hooks.log_error, message);
            AppendTrace(message);
        }
    }

    return true;
}

bool MapLogicDispatcher::QueueAction(
    const ScheduledUseAction& action,
    const MapLogicDispatcherHooks& hooks)
{
    if (action.target_name.empty())
    {
        LogActionEvent("skipped", action, "empty target", hooks);
        if (current_frame_.active)
        {
            ++current_frame_.summary.scheduled_skipped;
        }
        ++summary_.total_scheduled_skipped;
        return false;
    }

    ScheduledUseAction queued = action;
    if (queued.sequence == 0)
    {
        queued.sequence = next_sequence_++;
    }

    if (static_cast<int>(queued_actions_.size()) >= config_.scheduled_use_total_limit)
    {
        LogActionEvent("skipped", queued, "scheduled-use queue safety cap reached", hooks);
        if (current_frame_.active)
        {
            ++current_frame_.summary.scheduled_skipped;
        }
        ++summary_.total_scheduled_skipped;
        return false;
    }

    InsertQueuedAction(queued);
    if (current_frame_.active)
    {
        ++current_frame_.summary.scheduled_created;
    }
    ++summary_.total_scheduled_created;
    if (current_frame_.active
        && IsLongDelayAction(
            queued,
            current_frame_.summary.time,
            current_frame_.summary.frametime))
    {
        current_frame_.summary.long_delay_pending =
            std::max(current_frame_.summary.long_delay_pending, 0) + 1;
    }

    LogActionEvent("queued", queued, queued.reason, hooks);
    RebuildPendingCounts(hooks);
    return true;
}

void MapLogicDispatcher::ProcessDueQueue(const MapLogicDispatcherHooks& hooks)
{
    if (!current_frame_.active)
    {
        return;
    }

    int processed = 0;
    const float current_time = hooks.global_time ? hooks.global_time() : 0.0f;
    const float current_frametime = hooks.global_frametime ? hooks.global_frametime() : 0.0f;
    while (!queued_actions_.empty())
    {
        const ScheduledUseAction front = queued_actions_.front();
        if (front.fire_time > current_time)
        {
            break;
        }

        ++current_frame_.summary.scheduled_due;

        if (processed >= config_.scheduled_use_limit)
        {
            queued_actions_.erase(queued_actions_.begin());

            ScheduledUseAction rescheduled = front;
            ++rescheduled.reschedule_count;
            if (rescheduled.reschedule_count > config_.scheduled_use_max_reschedules)
            {
                ++current_frame_.summary.scheduled_skipped;
                ++summary_.total_scheduled_skipped;
                LogActionEvent("expired", rescheduled, "reschedule limit reached", hooks);
                RebuildPendingCounts(hooks);
                continue;
            }

            rescheduled.fire_time = current_time + std::max(current_frametime, 0.05f);
            InsertQueuedAction(rescheduled);
            ++current_frame_.summary.scheduled_rescheduled;
            ++summary_.total_scheduled_rescheduled;
            LogActionEvent(
                "rescheduled",
                rescheduled,
                "frame scheduled-use limit reached",
                hooks);
            RebuildPendingCounts(hooks);
            continue;
        }

        ScheduledUseAction action = queued_actions_.front();
        queued_actions_.erase(queued_actions_.begin());
        ++processed;
        ++current_frame_.summary.scheduled_executed;
        ++summary_.total_scheduled_executed;
        if (IsLongDelayAction(action, current_time, current_frametime))
        {
            ++current_frame_.summary.long_delay_executed;
            ++summary_.total_long_delay_executed;
        }
        LogActionEvent("executed", action, action.reason, hooks);
        RebuildPendingCounts(hooks);

        MapLogicDispatchContext context;
        context.frame_number = current_frame_.summary.frame_number;
        context.source_edict_index = action.source_edict_index;
        context.source_classname = action.source_classname;
        context.target_name = action.target_name;
        context.use_type = action.use_type;
        context.value = action.value;
        context.from_scheduled_queue = true;
        context.reason = action.reason;
        const int deferred_before = current_frame_.summary.use_deferred;
        const int failed_before = current_frame_.summary.use_failures;
        const bool dispatch_result = DispatchTargetChain(context, hooks);
        if (!dispatch_result)
        {
            ++current_frame_.summary.scheduled_failed;
            ++summary_.total_scheduled_failed;
            LogActionEvent("failed", action, "dispatch rejected", hooks);
        }
        else if (current_frame_.summary.use_failures > failed_before)
        {
            ++current_frame_.summary.scheduled_failed;
            ++summary_.total_scheduled_failed;
            LogActionEvent("failed", action, "downstream use failure", hooks);
        }
        else if (current_frame_.summary.use_deferred > deferred_before)
        {
            ++current_frame_.summary.scheduled_deferred;
            ++summary_.total_scheduled_deferred;
            LogActionEvent("deferred", action, "downstream target deferred", hooks);
        }
    }

    current_frame_.summary.queue_size_after = static_cast<int>(queued_actions_.size());
    current_frame_.summary.scheduled_pending = static_cast<int>(queued_actions_.size());
    summary_.scheduled_pending = current_frame_.summary.scheduled_pending;
}

void MapLogicDispatcher::CompleteFrame(const MapLogicDispatcherHooks& hooks)
{
    if (!current_frame_.active)
    {
        return;
    }

    current_frame_.summary.queue_size_after = static_cast<int>(queued_actions_.size());
    current_frame_.summary.scheduled_pending = static_cast<int>(queued_actions_.size());
    current_frame_.summary.long_delay_pending = 0;
    for (const ScheduledUseAction& action : queued_actions_)
    {
        if (IsLongDelayAction(
                action,
                current_frame_.summary.time,
                current_frame_.summary.frametime))
        {
            ++current_frame_.summary.long_delay_pending;
        }
    }
    summary_.scheduled_pending = current_frame_.summary.scheduled_pending;
    summary_.total_long_delay_pending = current_frame_.summary.long_delay_pending;

    if (hooks.total_callback_counts)
    {
        const MapLogicCallbackCountMap callbacks_after = hooks.total_callback_counts();
        const std::vector<InvokedEngineCallback> delta =
            BuildCallbackDelta(current_frame_.callbacks_before, callbacks_after);
        current_frame_.summary.callback_counts_this_frame = delta;
        for (const InvokedEngineCallback& callback : delta)
        {
            auto it = std::find_if(
                summary_.callbacks_during_dispatcher.begin(),
                summary_.callbacks_during_dispatcher.end(),
                [&](const InvokedEngineCallback& existing)
                {
                    return existing.name == callback.name;
                });
            if (it == summary_.callbacks_during_dispatcher.end())
            {
                summary_.callbacks_during_dispatcher.push_back(callback);
            }
            else
            {
                it->call_count += callback.call_count;
            }
        }
    }

    AppendTrace(
        "frame " + std::to_string(current_frame_.summary.frame_number)
        + " end queueAfter=" + std::to_string(current_frame_.summary.queue_size_after)
        + " due/executed/rescheduled/skipped/failed/deferred="
        + std::to_string(current_frame_.summary.scheduled_due) + "/"
        + std::to_string(current_frame_.summary.scheduled_executed) + "/"
        + std::to_string(current_frame_.summary.scheduled_rescheduled) + "/"
        + std::to_string(current_frame_.summary.scheduled_skipped) + "/"
        + std::to_string(current_frame_.summary.scheduled_failed) + "/"
        + std::to_string(current_frame_.summary.scheduled_deferred));

    current_frame_.summary.trace_tail = summary_.rolling_trace_tail.size() <= config_.trace_tail_limit
        ? summary_.rolling_trace_tail
        : std::vector<std::string>(
            summary_.rolling_trace_tail.end() - static_cast<std::ptrdiff_t>(config_.trace_tail_limit),
            summary_.rolling_trace_tail.end());

    summary_.frames.push_back(current_frame_.summary);
    ++summary_.frames_completed;
    summary_.readiness = BuildMapLogicDispatcherReadiness(summary_);
    current_frame_ = {};
}

const MapLogicDispatcherStateSummary& MapLogicDispatcher::Summary() const noexcept
{
    return summary_;
}

const MapLogicFrameStateSummary* MapLogicDispatcher::CurrentFrameSummary() const noexcept
{
    return current_frame_.active ? &current_frame_.summary : nullptr;
}

int MapLogicDispatcher::PendingActionCount() const noexcept
{
    return static_cast<int>(queued_actions_.size());
}

std::vector<ScheduledUseAction> MapLogicDispatcher::QueuedActionsSnapshot() const
{
    return queued_actions_;
}

void MapLogicDispatcher::Log(
    const std::function<void(std::string_view)>& sink,
    const std::string& message) const
{
    if (sink)
    {
        sink(message);
    }
}

void MapLogicDispatcher::AppendTrace(std::string line)
{
    summary_.rolling_trace_tail.push_back(std::move(line));
    if (summary_.rolling_trace_tail.size() > config_.rolling_trace_limit)
    {
        summary_.rolling_trace_tail.erase(summary_.rolling_trace_tail.begin());
    }
}

void MapLogicDispatcher::InsertQueuedAction(ScheduledUseAction action)
{
    const auto insert_at = std::lower_bound(
        queued_actions_.begin(),
        queued_actions_.end(),
        action,
        [](const ScheduledUseAction& left, const ScheduledUseAction& right)
        {
            return ActionOrderLess(left, right);
        });
    queued_actions_.insert(insert_at, std::move(action));
}

void MapLogicDispatcher::LogActionEvent(
    std::string_view event_name,
    const ScheduledUseAction& action,
    std::string_view detail,
    const MapLogicDispatcherHooks& hooks)
{
    std::string message =
        "MapLogicDispatcher: " + std::string(event_name) + " " + DescribeScheduledAction(action);
    if (!detail.empty())
    {
        message += " reason=" + std::string(detail);
    }

    if (event_name == "failed" || event_name == "expired")
    {
        Log(hooks.log_error, message);
    }
    else if (event_name == "deferred" || event_name == "rescheduled" || event_name == "skipped")
    {
        Log(hooks.log_warn, message);
    }
    else
    {
        Log(hooks.log_info, message);
    }

    AppendTrace(message);

    if (hooks.record_scheduled_action_event)
    {
        hooks.record_scheduled_action_event(action, event_name, detail);
    }
    if (current_frame_.active
        && current_frame_.summary.scheduled_action_preview.size() < config_.action_preview_limit)
    {
        current_frame_.summary.scheduled_action_preview.push_back(
            std::string(event_name) + " " + DescribeScheduledAction(action));
    }
}

bool MapLogicDispatcher::IsLongDelayAction(
    const ScheduledUseAction& action,
    float current_time,
    float frametime) noexcept
{
    return (action.fire_time - current_time) > std::max(frametime, 0.05f);
}

void MapLogicDispatcher::RebuildPendingCounts(const MapLogicDispatcherHooks& hooks)
{
    std::unordered_map<int, int> rebuilt;
    for (const ScheduledUseAction& action : queued_actions_)
    {
        if (action.source_edict_index >= 0)
        {
            ++rebuilt[action.source_edict_index];
        }
    }

    if (hooks.set_pending_scheduled_outputs)
    {
        for (const auto& [edict_index, count] : pending_counts_by_source_)
        {
            if (rebuilt.find(edict_index) == rebuilt.end())
            {
                hooks.set_pending_scheduled_outputs(edict_index, 0);
            }
        }

        for (const auto& [edict_index, count] : rebuilt)
        {
            hooks.set_pending_scheduled_outputs(edict_index, count);
        }
    }

    pending_counts_by_source_ = std::move(rebuilt);
}

std::string MapLogicDispatcher::DescribeScheduledAction(const ScheduledUseAction& action)
{
    return "fireTime=" + std::to_string(action.fire_time)
        + " source=edict#" + std::to_string(action.source_edict_index)
        + " classname="
        + (action.source_classname.empty() ? std::string("<empty>") : action.source_classname)
        + " target=" + (action.target_name.empty() ? std::string("<empty>") : action.target_name)
        + " useType=" + std::string(UseTypeLabel(action.use_type))
        + " value=" + std::to_string(action.value)
        + " seq=" + std::to_string(action.sequence)
        + " rescheduled=" + std::to_string(action.reschedule_count);
}

std::vector<InvokedEngineCallback> MapLogicDispatcher::BuildCallbackDelta(
    const MapLogicCallbackCountMap& before,
    const MapLogicCallbackCountMap& after)
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

const char* MapLogicDispatcher::UseTypeLabel(int use_type) noexcept
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

std::string BuildMapLogicDispatcherReadiness(const MapLogicDispatcherStateSummary& summary)
{
    if (!summary.configured)
    {
        return "map logic dispatcher is not configured";
    }

    if (!summary.attempted)
    {
        return "map logic dispatcher is configured and waiting for post-think frames";
    }

    if (summary.total_target_chains_fired == 0)
    {
        return "map logic dispatcher is idle; no target chains were emitted yet";
    }

    if (summary.total_use_failures > 0
        || summary.total_use_seh_failures > 0
        || summary.total_scheduled_failed > 0)
    {
        return "map logic dispatcher is active; narrow the failing use path before broader scripted support";
    }

    if (summary.total_target_resolutions == 0)
    {
        return "target chains are emitted but unresolved; retain more deferred recipients next";
    }

    if (summary.scheduled_pending > 0)
    {
        return "map logic dispatcher is stable; drain queued scripted outputs across more frames";
    }

    if (summary.total_use_successes > 0)
    {
        return "map logic dispatcher is stable; broader scripted cleanup and passive actor support are next";
    }

    return "target chains now resolve deterministically; broaden deferred recipients or safe use targets next";
}
} // namespace hl::game_api::detail
