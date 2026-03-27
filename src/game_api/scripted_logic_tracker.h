#pragma once

#include <string>
#include <string_view>

#include "game_api/hl_server_module.h"

namespace hl::game_api::detail
{
struct ScriptedLogicStateTracker
{
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
    bool received_use_this_frame = false;
    bool internal_state_changed = false;
    bool scheduled_follow_up = false;
    bool emitted_targets = false;
    bool actor_exists = false;
    bool path_linked = false;
};

const char* ScriptedLogicSupportStateLabel(ScriptedLogicSupportState state) noexcept;
bool IsRelevantScriptedLogicClass(std::string_view classname);
void ResetScriptedLogicFrameState(ScriptedLogicStateTracker& tracker);
void NoteScriptedLogicUse(
    ScriptedLogicStateTracker& tracker,
    int frame_number,
    float trigger_time,
    std::string source_entity);
void NoteScriptedLogicTargetEmission(
    ScriptedLogicStateTracker& tracker,
    int frame_number,
    float trigger_time,
    std::string source_entity);
void NoteScriptedLogicScheduledOutput(
    ScriptedLogicStateTracker& tracker,
    int frame_number,
    float trigger_time,
    std::string source_entity);
void MarkScriptedLogicProgressed(ScriptedLogicStateTracker& tracker);
void MarkScriptedLogicBlocked(
    ScriptedLogicStateTracker& tracker,
    ScriptedLogicSupportState state,
    std::string reason);
} // namespace hl::game_api::detail
