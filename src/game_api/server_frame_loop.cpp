#include "server_frame_loop.h"

#include <algorithm>
#include <array>
#include <sstream>
#include <utility>

#pragma warning(push, 0)
#include "extdll.h"
#pragma warning(pop)

namespace
{
using hl::game_api::InvokedEngineCallback;
using hl::game_api::ServerFrameLoopStateSummary;
using hl::game_api::ServerFrameStateSummary;
using hl::game_api::detail::CallbackCountMap;
using hl::game_api::detail::FrameLoopValidationState;
using hl::game_api::detail::ServerFrameLoopHooks;

void LogWith(
    const std::function<void(std::string_view)>& sink,
    const std::string& message)
{
    if (sink)
    {
        sink(message);
    }
}

bool IsValidationPassed(const FrameLoopValidationState& validation)
{
    return validation.activation_succeeded
        && validation.server_active
        && validation.worldspawn_spawned
        && validation.edict0_valid
        && validation.mapname_valid
        && validation.max_clients_sane
        && validation.issues.empty();
}

std::string FormatExceptionCode(unsigned int code)
{
    std::ostringstream stream;
    stream << "0x" << std::hex << std::uppercase << code;
    return stream.str();
}
} // namespace

namespace hl::game_api::detail
{
namespace
{
const char* MessageDestinationName(int destination)
{
    switch (destination)
    {
    case MSG_BROADCAST:
        return "MSG_BROADCAST";
    case MSG_ONE:
        return "MSG_ONE";
    case MSG_ALL:
        return "MSG_ALL";
    case MSG_INIT:
        return "MSG_INIT";
    case MSG_PVS:
        return "MSG_PVS";
    case MSG_PAS:
        return "MSG_PAS";
    case MSG_PVS_R:
        return "MSG_PVS_R";
    case MSG_PAS_R:
        return "MSG_PAS_R";
    case MSG_ONE_UNRELIABLE:
        return "MSG_ONE_UNRELIABLE";
    case MSG_SPEC:
        return "MSG_SPEC";
    default:
        return "MSG_UNKNOWN";
    }
}
} // namespace

void FrameMessageBuffer::Reset()
{
    has_active_message_ = false;
    active_message_ = {};
    completed_messages_.clear();
}

void FrameMessageBuffer::Begin(
    int destination,
    int message_type,
    std::string_view origin_text,
    int edict_index,
    std::string_view classname)
{
    if (has_active_message_)
    {
        Abort("implicit abort before new begin");
    }

    has_active_message_ = true;
    active_message_ = {};
    active_message_.destination = destination;
    active_message_.message_type = message_type;
    active_message_.edict_index = edict_index;
    active_message_.classname = std::string(classname);
    active_message_.origin_text = std::string(origin_text);
}

std::string FrameMessageBuffer::End()
{
    if (!has_active_message_)
    {
        return "message=<none active>";
    }

    active_message_.completed = true;
    const std::string summary = Summarize(active_message_);
    completed_messages_.push_back(active_message_);
    while (completed_messages_.size() > kMaxCompletedMessages)
    {
        completed_messages_.pop_front();
    }

    has_active_message_ = false;
    active_message_ = {};
    return summary;
}

void FrameMessageBuffer::Abort(std::string_view reason)
{
    if (!has_active_message_)
    {
        return;
    }

    active_message_.aborted = true;
    active_message_.writes.push_back("abort=" + std::string(reason));
    completed_messages_.push_back(active_message_);
    while (completed_messages_.size() > kMaxCompletedMessages)
    {
        completed_messages_.pop_front();
    }

    has_active_message_ = false;
    active_message_ = {};
}

bool FrameMessageBuffer::HasActive() const noexcept
{
    return has_active_message_;
}

void FrameMessageBuffer::WriteByte(int value)
{
    AddWrite("WriteByte", std::to_string(value), 1);
}

void FrameMessageBuffer::WriteChar(int value)
{
    AddWrite("WriteChar", std::to_string(value), 1);
}

void FrameMessageBuffer::WriteShort(int value)
{
    AddWrite("WriteShort", std::to_string(value), 2);
}

void FrameMessageBuffer::WriteLong(int value)
{
    AddWrite("WriteLong", std::to_string(value), 4);
}

void FrameMessageBuffer::WriteAngle(float value)
{
    AddWrite("WriteAngle", std::to_string(value), 4);
}

void FrameMessageBuffer::WriteCoord(float value)
{
    AddWrite("WriteCoord", std::to_string(value), 4);
}

void FrameMessageBuffer::WriteString(std::string_view value)
{
    AddWrite("WriteString", std::string(value), value.size() + 1);
}

void FrameMessageBuffer::WriteEntity(int value)
{
    AddWrite("WriteEntity", std::to_string(value), 4);
}

std::size_t FrameMessageBuffer::CompletedCount() const noexcept
{
    return completed_messages_.size();
}

std::vector<std::string> FrameMessageBuffer::CompletedPreview(std::size_t limit) const
{
    std::vector<std::string> preview;
    if (limit == 0 || completed_messages_.empty())
    {
        return preview;
    }

    const std::size_t start =
        completed_messages_.size() > limit ? completed_messages_.size() - limit : 0;
    preview.reserve(completed_messages_.size() - start);
    for (std::size_t index = start; index < completed_messages_.size(); ++index)
    {
        preview.push_back(Summarize(completed_messages_[index]));
    }

    return preview;
}

void FrameMessageBuffer::AddWrite(
    std::string_view op_name,
    std::string value_text,
    std::size_t payload_bytes)
{
    if (!has_active_message_)
    {
        return;
    }

    active_message_.payload_size += payload_bytes;
    active_message_.writes.push_back(std::string(op_name) + "=" + std::move(value_text));
}

std::string FrameMessageBuffer::Summarize(const MessageRecord& record)
{
    std::string summary =
        std::string(MessageDestinationName(record.destination))
        + "(" + std::to_string(record.destination) + ")"
        + ", type=" + std::to_string(record.message_type)
        + ", edict#=" + std::to_string(record.edict_index)
        + ", classname="
        + (record.classname.empty() ? std::string("<empty>") : record.classname)
        + ", origin="
        + (record.origin_text.empty() ? std::string("<unset>") : record.origin_text)
        + ", size=" + std::to_string(record.payload_size);

    if (record.aborted)
    {
        summary += ", state=aborted";
    }
    else if (record.completed)
    {
        summary += ", state=completed";
    }

    if (record.writes.empty())
    {
        summary += ", writes=<none>";
        return summary;
    }

    summary += ", writes=";
    const std::size_t preview_count = std::min<std::size_t>(record.writes.size(), 6);
    for (std::size_t index = 0; index < preview_count; ++index)
    {
        if (index != 0)
        {
            summary += " | ";
        }

        summary += record.writes[index];
    }

    if (record.writes.size() > preview_count)
    {
        summary += " | ... +" + std::to_string(record.writes.size() - preview_count) + " writes";
    }

    return summary;
}

std::vector<InvokedEngineCallback> BuildServerFrameCallbackDelta(
    const CallbackCountMap& before,
    const CallbackCountMap& after)
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

std::string BuildServerFrameLoopReadiness(const ServerFrameLoopStateSummary& summary)
{
    if (!summary.activation_succeeded)
    {
        return "server activation must succeed before broader frame lifecycle work";
    }

    if (summary.stopped_early)
    {
        return "debug stop condition triggered; inspect focused diagnostics or rerun without the stop gate";
    }

    if (summary.any_seh)
    {
        return "inspect the last StartFrame trace and harden newly exercised engine callbacks";
    }

    if (!summary.validation_failures.empty())
    {
        return "stabilize frame pre-validation before expanding entity think/use/touch work";
    }

    if (summary.start_frame_present)
    {
        return "ready for broader entity lifecycle, scheduled think callbacks, and later client bootstrap";
    }

    return "ready to broaden server frame lifecycle even before pfnStartFrame is exercised";
}

ServerFrameLoopStateSummary RunServerFrameLoop(
    const FrameLoopConfig& config,
    const ServerFrameLoopHooks& hooks)
{
    ServerFrameLoopStateSummary summary;
    summary.configured = true;
    summary.frames_requested = std::max(config.frames, 0);
    summary.fixed_frametime = config.frametime > 0.0f ? config.frametime : 0.05f;
    summary.start_frame_present = hooks.start_frame_present ? hooks.start_frame_present() : false;

    if (summary.frames_requested <= 0)
    {
        summary.readiness = "frame bootstrap disabled; rerun with --frames > 0 when ready";
        LogWith(hooks.log_info, "ServerFrameLoop: disabled because frames=0.");
        return summary;
    }

    summary.attempted = true;
    const CallbackCountMap loop_before =
        hooks.total_callback_counts ? hooks.total_callback_counts() : CallbackCountMap{};

    LogWith(
        hooks.log_info,
        "ServerFrameLoop: begin [frames=" + std::to_string(summary.frames_requested)
            + ", frametime=" + std::to_string(summary.fixed_frametime)
            + ", pfnStartFrame=" + std::string(summary.start_frame_present ? "present" : "missing")
            + "]");

    summary.frames.reserve(static_cast<std::size_t>(summary.frames_requested));

    for (int frame_number = 1; frame_number <= summary.frames_requested; ++frame_number)
    {
        LogWith(
            hooks.log_info,
            "ServerFrameLoop: ===== frame " + std::to_string(frame_number) + "/"
                + std::to_string(summary.frames_requested) + " begin =====");

        ServerFrameStateSummary frame_summary;
        frame_summary.frame_number = frame_number;
        frame_summary.start_frame_present = summary.start_frame_present;
        if (hooks.host_frame_index)
        {
            frame_summary.host_frame_index = hooks.host_frame_index();
        }
        if (hooks.server_frame_index)
        {
            frame_summary.server_frame_index = hooks.server_frame_index();
        }
        if (hooks.global_time)
        {
            frame_summary.time = hooks.global_time();
        }
        if (hooks.global_frametime)
        {
            frame_summary.frametime = hooks.global_frametime();
        }

        const FrameLoopValidationState validation =
            hooks.validate_frame_state ? hooks.validate_frame_state(config.entity_preview_limit)
                                       : FrameLoopValidationState{};
        summary.activation_succeeded =
            summary.activation_succeeded || validation.activation_succeeded;
        frame_summary.validation_passed = IsValidationPassed(validation);
        frame_summary.active_edicts = validation.active_edicts;
        frame_summary.spawned_entities = validation.spawned_entities;
        frame_summary.removed_entities = validation.removed_entities;
        frame_summary.deferred_entities = validation.deferred_entities;
        frame_summary.validation_issues = validation.issues;

        LogWith(
            hooks.log_info,
            "ServerFrameLoop: frame preflight [activation="
                + std::string(validation.activation_succeeded ? "yes" : "no")
                + ", active=" + std::string(validation.server_active ? "yes" : "no")
                + ", worldspawn=" + std::string(validation.worldspawn_spawned ? "yes" : "no")
                + ", edict0=" + std::string(validation.edict0_valid ? "yes" : "no")
                + ", map="
                + (validation.map_name.empty() ? std::string("<empty>") : validation.map_name)
                + ", maxClients=" + std::to_string(validation.max_clients)
                + ", active/spawned/removed/deferred="
                + std::to_string(validation.active_edicts) + "/"
                + std::to_string(validation.spawned_entities) + "/"
                + std::to_string(validation.removed_entities) + "/"
                + std::to_string(validation.deferred_entities) + "]");

        for (const FrameLoopEntityPreview& preview : validation.preview_entities)
        {
            frame_summary.entity_preview.push_back(
                preview.summary
                + " frame_relevant=" + (preview.frame_relevant ? std::string("yes") : "no"));
        }

        if (!frame_summary.entity_preview.empty())
        {
            LogWith(hooks.log_info, "ServerFrameLoop: active entity preview:");
            for (const std::string& line : frame_summary.entity_preview)
            {
                LogWith(hooks.log_info, "  - " + line);
            }
        }

        if (!frame_summary.validation_passed)
        {
            if (frame_summary.validation_issues.empty())
            {
                frame_summary.validation_issues.push_back("unknown validation failure");
            }

            LogWith(
                hooks.log_warn,
                "ServerFrameLoop: skipping frame " + std::to_string(frame_number)
                    + " because pre-validation failed.");
            for (const std::string& issue : frame_summary.validation_issues)
            {
                LogWith(hooks.log_warn, "  - " + issue);
                summary.validation_failures.push_back(
                    "frame " + std::to_string(frame_number) + ": " + issue);
            }

            summary.frames.push_back(std::move(frame_summary));
            LogWith(
                hooks.log_info,
                "ServerFrameLoop: ===== frame " + std::to_string(frame_number)
                    + "/" + std::to_string(summary.frames_requested)
                    + " end [skipped] =====");
            continue;
        }

        const CallbackCountMap frame_before =
            hooks.total_callback_counts ? hooks.total_callback_counts() : CallbackCountMap{};

        if (hooks.advance_time)
        {
            hooks.advance_time(summary.fixed_frametime);
        }
        if (hooks.host_frame_index)
        {
            frame_summary.host_frame_index = hooks.host_frame_index();
        }
        if (hooks.server_frame_index)
        {
            frame_summary.server_frame_index = hooks.server_frame_index();
        }
        if (hooks.global_time)
        {
            frame_summary.time = hooks.global_time();
        }
        if (hooks.global_frametime)
        {
            frame_summary.frametime = hooks.global_frametime();
        }
        if (hooks.globals_snapshot)
        {
            frame_summary.globals_snapshot = hooks.globals_snapshot();
        }
        if (hooks.server_snapshot)
        {
            frame_summary.server_state_snapshot = hooks.server_snapshot();
        }

        LogWith(
            hooks.log_info,
            "ServerFrameLoop: frame timing [frame=" + std::to_string(frame_number)
                + ", host=" + std::to_string(frame_summary.host_frame_index)
                + ", server=" + std::to_string(frame_summary.server_frame_index)
                + ", time=" + std::to_string(frame_summary.time)
                + ", frametime=" + std::to_string(frame_summary.frametime) + "]");
        if (!frame_summary.globals_snapshot.empty())
        {
            LogWith(hooks.log_info, "ServerFrameLoop: gpGlobals " + frame_summary.globals_snapshot);
        }
        if (!frame_summary.server_state_snapshot.empty())
        {
            LogWith(
                hooks.log_info,
                "ServerFrameLoop: server flags " + frame_summary.server_state_snapshot);
        }

        if (hooks.begin_frame_observation)
        {
            hooks.begin_frame_observation(
                frame_number,
                frame_summary.host_frame_index,
                frame_summary.server_frame_index,
                frame_summary.time,
                frame_summary.frametime);
        }

        if (summary.start_frame_present)
        {
            frame_summary.start_frame_called = true;
            LogWith(
                hooks.log_info,
                "ServerFrameLoop: calling pfnStartFrame for frame " + std::to_string(frame_number)
                    + ".");

            unsigned int seh_code = 0;
            const bool succeeded =
                hooks.call_start_frame ? hooks.call_start_frame(&seh_code) : false;
            if (succeeded)
            {
                if (hooks.post_start_frame_lifecycle)
                {
                    hooks.post_start_frame_lifecycle(
                        frame_number,
                        frame_summary.host_frame_index,
                        frame_summary.server_frame_index,
                        frame_summary.time,
                        frame_summary.frametime);
                }

                frame_summary.start_frame_succeeded = true;
                if (hooks.mark_frame_success)
                {
                    hooks.mark_frame_success();
                }
            }
            else
            {
                frame_summary.seh_exception = true;
                frame_summary.seh_code = seh_code;
                summary.any_seh = true;
                summary.seh_code = seh_code;
                if (hooks.mark_frame_seh)
                {
                    hooks.mark_frame_seh(seh_code);
                }

                LogWith(
                    hooks.log_error,
                    "ServerFrameLoop: pfnStartFrame raised SEH "
                        + FormatExceptionCode(seh_code) + " on frame "
                        + std::to_string(frame_number) + ".");
            }
        }
        else
        {
            LogWith(
                hooks.log_info,
                "ServerFrameLoop: pfnStartFrame is missing; frame " + std::to_string(frame_number)
                    + " will only advance deterministic time.");
            if (hooks.mark_frame_success)
            {
                hooks.mark_frame_success();
            }
        }

        if (!frame_summary.seh_exception && hooks.drain_pending_commands)
        {
            hooks.drain_pending_commands(frame_number);
        }

        const CallbackCountMap frame_after =
            hooks.total_callback_counts ? hooks.total_callback_counts() : CallbackCountMap{};
        frame_summary.callback_counts_this_frame =
            BuildServerFrameCallbackDelta(frame_before, frame_after);

        if (hooks.frame_trace_event_count)
        {
            summary.total_trace_events += hooks.frame_trace_event_count();
        }
        if (hooks.frame_trace_tail)
        {
            frame_summary.callback_trace_tail = hooks.frame_trace_tail(config.trace_tail_limit);
        }
        if (hooks.completed_messages)
        {
            frame_summary.message_preview = hooks.completed_messages(config.message_preview_limit);
        }

        if (frame_summary.callback_counts_this_frame.empty())
        {
            LogWith(hooks.log_info, "ServerFrameLoop: frame callback delta: none");
        }
        else
        {
            LogWith(hooks.log_info, "ServerFrameLoop: frame callback delta:");
            for (const InvokedEngineCallback& callback : frame_summary.callback_counts_this_frame)
            {
                LogWith(
                    hooks.log_info,
                    "  - " + callback.name + " x" + std::to_string(callback.call_count));
            }
        }

        if (!frame_summary.message_preview.empty())
        {
            LogWith(hooks.log_info, "ServerFrameLoop: completed messages:");
            for (const std::string& message : frame_summary.message_preview)
            {
                LogWith(hooks.log_info, "  - " + message);
            }
        }

        bool stop_after_frame = false;
        std::string stop_reason;
        if (!frame_summary.seh_exception && hooks.should_stop_after_frame)
        {
            stop_after_frame = hooks.should_stop_after_frame(&stop_reason);
            if (stop_after_frame)
            {
                summary.stopped_early = true;
                summary.stop_frame = frame_number;
                summary.stop_time = frame_summary.time;
                summary.stop_reason =
                    stop_reason.empty() ? std::string("stop requested after frame completion")
                                        : stop_reason;
                LogWith(
                    hooks.log_warn,
                    "ServerFrameLoop: stop condition triggered after frame "
                        + std::to_string(frame_number)
                        + " [reason=" + summary.stop_reason + "]");
            }
        }

        if (frame_summary.seh_exception)
        {
            if (frame_summary.callback_trace_tail.empty())
            {
                LogWith(hooks.log_info, "ServerFrameLoop: frame trace tail: <empty>");
            }
            else
            {
                LogWith(hooks.log_info, "ServerFrameLoop: frame trace tail:");
                for (const std::string& line : frame_summary.callback_trace_tail)
                {
                    LogWith(hooks.log_info, "  - " + line);
                }
            }
        }
        else
        {
            ++summary.frames_completed;
        }

        summary.frames.push_back(std::move(frame_summary));
        LogWith(
            hooks.log_info,
            "ServerFrameLoop: ===== frame " + std::to_string(frame_number)
                + "/" + std::to_string(summary.frames_requested)
                + " end ["
                + (summary.frames.back().seh_exception ? std::string("failed") : std::string("ok"))
                + "] =====");

        if (summary.frames.back().seh_exception)
        {
            break;
        }
        if (stop_after_frame)
        {
            break;
        }
    }

    const CallbackCountMap loop_after =
        hooks.total_callback_counts ? hooks.total_callback_counts() : CallbackCountMap{};
    summary.callbacks_during_loop = BuildServerFrameCallbackDelta(loop_before, loop_after);
    if (hooks.global_time)
    {
        summary.final_time = hooks.global_time();
    }
    summary.readiness = BuildServerFrameLoopReadiness(summary);

    LogWith(
        hooks.log_info,
        "ServerFrameLoop: summary [completed=" + std::to_string(summary.frames_completed) + "/"
            + std::to_string(summary.frames_requested)
            + ", final_time=" + std::to_string(summary.final_time)
            + ", pfnStartFrame=" + std::string(summary.start_frame_present ? "present" : "missing")
            + ", seh=" + std::string(summary.any_seh ? "yes" : "no")
            + ", stoppedEarly=" + std::string(summary.stopped_early ? "yes" : "no") + "]");
    if (summary.stopped_early)
    {
        LogWith(
            hooks.log_info,
            "ServerFrameLoop: stop summary [frame=" + std::to_string(summary.stop_frame)
                + ", time=" + std::to_string(summary.stop_time)
                + ", reason=" + summary.stop_reason + "]");
    }
    LogWith(
        hooks.log_info,
        "ServerFrameLoop: readiness for next step: " + summary.readiness);

    return summary;
}
} // namespace hl::game_api::detail
