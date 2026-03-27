#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "game_api/hl_server_module.h"

namespace hl::game_api::detail
{
using CallbackCountMap = std::unordered_map<std::string, std::size_t>;

struct FrameLoopEntityPreview
{
    int edict_index = -1;
    bool frame_relevant = false;
    std::string summary;
};

struct FrameLoopValidationState
{
    bool activation_succeeded = false;
    bool server_active = false;
    bool worldspawn_spawned = false;
    bool edict0_valid = false;
    bool mapname_valid = false;
    bool max_clients_sane = false;
    std::string map_name;
    int max_clients = 0;
    int active_edicts = 0;
    int spawned_entities = 0;
    int removed_entities = 0;
    int deferred_entities = 0;
    std::vector<std::string> issues;
    std::vector<FrameLoopEntityPreview> preview_entities;
};

struct FrameLoopConfig
{
    int frames = 3;
    float frametime = 0.05f;
    std::size_t entity_preview_limit = 8;
    std::size_t trace_tail_limit = 32;
    std::size_t message_preview_limit = 8;
};

class FrameMessageBuffer
{
public:
    void Reset();
    void Begin(
        int destination,
        int message_type,
        std::string_view origin_text,
        int edict_index,
        std::string_view classname);
    std::string End();
    void Abort(std::string_view reason);

    bool HasActive() const noexcept;
    void WriteByte(int value);
    void WriteChar(int value);
    void WriteShort(int value);
    void WriteLong(int value);
    void WriteAngle(float value);
    void WriteCoord(float value);
    void WriteString(std::string_view value);
    void WriteEntity(int value);

    std::size_t CompletedCount() const noexcept;
    std::vector<std::string> CompletedPreview(std::size_t limit) const;

private:
    struct MessageRecord
    {
        int destination = 0;
        int message_type = 0;
        int edict_index = -1;
        std::string classname;
        std::string origin_text;
        std::size_t payload_size = 0;
        std::vector<std::string> writes;
        bool completed = false;
        bool aborted = false;
    };

    void AddWrite(std::string_view op_name, std::string value_text, std::size_t payload_bytes);
    static std::string Summarize(const MessageRecord& record);

    static constexpr std::size_t kMaxCompletedMessages = 32;

    bool has_active_message_ = false;
    MessageRecord active_message_;
    std::deque<MessageRecord> completed_messages_;
};

struct ServerFrameLoopHooks
{
    std::function<FrameLoopValidationState(std::size_t)> validate_frame_state;
    std::function<void(float)> advance_time;
    std::function<std::uint64_t()> host_frame_index;
    std::function<std::uint64_t()> server_frame_index;
    std::function<float()> global_time;
    std::function<float()> global_frametime;
    std::function<std::string()> globals_snapshot;
    std::function<std::string()> server_snapshot;
    std::function<bool()> start_frame_present;
    std::function<void(int, std::uint64_t, std::uint64_t, float, float)> begin_frame_observation;
    std::function<void()> mark_frame_success;
    std::function<void(unsigned int)> mark_frame_seh;
    std::function<bool(unsigned int* seh_code)> call_start_frame;
    std::function<void(int, std::uint64_t, std::uint64_t, float, float)> post_start_frame_lifecycle;
    std::function<std::vector<std::string>(std::size_t)> frame_trace_tail;
    std::function<std::size_t()> frame_trace_event_count;
    std::function<CallbackCountMap()> total_callback_counts;
    std::function<std::vector<std::string>(std::size_t)> completed_messages;
    std::function<void(int)> drain_pending_commands;
    std::function<bool(std::string* reason)> should_stop_after_frame;
    std::function<void(std::string_view)> log_info;
    std::function<void(std::string_view)> log_warn;
    std::function<void(std::string_view)> log_error;
};

std::vector<InvokedEngineCallback> BuildServerFrameCallbackDelta(
    const CallbackCountMap& before,
    const CallbackCountMap& after);
std::string BuildServerFrameLoopReadiness(const ServerFrameLoopStateSummary& summary);
ServerFrameLoopStateSummary RunServerFrameLoop(
    const FrameLoopConfig& config,
    const ServerFrameLoopHooks& hooks);
} // namespace hl::game_api::detail
