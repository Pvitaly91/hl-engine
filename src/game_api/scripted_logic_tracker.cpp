#include "scripted_logic_tracker.h"

#include <algorithm>
#include <cctype>

namespace
{
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
const char* ScriptedLogicSupportStateLabel(ScriptedLogicSupportState state) noexcept
{
    switch (state)
    {
    case ScriptedLogicSupportState::kPassiveRecipientOnly:
        return "passive-recipient-only";
    case ScriptedLogicSupportState::kUseSupported:
        return "use-supported";
    case ScriptedLogicSupportState::kScheduledUseSupported:
        return "scheduled-use-supported";
    case ScriptedLogicSupportState::kScriptedProgressing:
        return "scripted-progressing";
    case ScriptedLogicSupportState::kBlockedOnMovement:
        return "blocked-on-movement";
    case ScriptedLogicSupportState::kBlockedOnActor:
        return "blocked-on-actor";
    case ScriptedLogicSupportState::kBlockedOnEngineCallback:
        return "blocked-on-engine-callback";
    case ScriptedLogicSupportState::kRemovedByGameLogic:
        return "removed-by-game-logic";
    default:
        return "unknown";
    }
}

bool IsRelevantScriptedLogicClass(std::string_view classname)
{
    const std::string normalized = NormalizeClassname(classname);
    return normalized == "scripted_sequence"
        || normalized == "path_track"
        || normalized == "env_message"
        || normalized == "ambient_generic"
        || normalized == "monster_scientist"
        || normalized == "monster_sitting_scientist"
        || normalized == "monster_barney";
}

void ResetScriptedLogicFrameState(ScriptedLogicStateTracker& tracker)
{
    tracker.progressed_this_frame = false;
    tracker.received_use_this_frame = false;
}

void NoteScriptedLogicUse(
    ScriptedLogicStateTracker& tracker,
    int frame_number,
    float trigger_time,
    std::string source_entity)
{
    ++tracker.received_use_count;
    tracker.received_use_this_frame = true;
    tracker.last_trigger_frame = frame_number;
    tracker.last_trigger_time = trigger_time;
    tracker.last_source_entity = std::move(source_entity);
}

void NoteScriptedLogicTargetEmission(
    ScriptedLogicStateTracker& tracker,
    int frame_number,
    float trigger_time,
    std::string source_entity)
{
    ++tracker.emitted_target_count;
    tracker.emitted_targets = true;
    tracker.last_trigger_frame = frame_number;
    tracker.last_trigger_time = trigger_time;
    if (!source_entity.empty())
    {
        tracker.last_source_entity = std::move(source_entity);
    }
    MarkScriptedLogicProgressed(tracker);
}

void NoteScriptedLogicScheduledOutput(
    ScriptedLogicStateTracker& tracker,
    int frame_number,
    float trigger_time,
    std::string source_entity)
{
    ++tracker.scheduled_output_count;
    tracker.scheduled_follow_up = true;
    tracker.last_trigger_frame = frame_number;
    tracker.last_trigger_time = trigger_time;
    if (!source_entity.empty())
    {
        tracker.last_source_entity = std::move(source_entity);
    }
    MarkScriptedLogicProgressed(tracker);
}

void MarkScriptedLogicProgressed(ScriptedLogicStateTracker& tracker)
{
    tracker.progressed_this_frame = true;
    tracker.progressed_this_run = true;
}

void MarkScriptedLogicBlocked(
    ScriptedLogicStateTracker& tracker,
    ScriptedLogicSupportState state,
    std::string reason)
{
    tracker.support_state = state;
    tracker.blocked_reason = std::move(reason);
}
} // namespace hl::game_api::detail
