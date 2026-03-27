#pragma once

#include <cstddef>
#include <deque>
#include <string>
#include <string_view>
#include <vector>

namespace hl::game_api
{
struct ServerCommandBufferSnapshot
{
    std::size_t queued_total = 0;
    std::size_t executed_total = 0;
    std::size_t pending_total = 0;
    std::vector<std::string> pending_commands;
};

class ServerCommandBuffer
{
public:
    void Queue(std::string_view command_text);
    bool HasPending() const noexcept;
    std::string PopFront();
    std::vector<std::string> Drain();
    ServerCommandBufferSnapshot Snapshot() const;
    void MarkExecuted(std::size_t count) noexcept;

    std::size_t QueuedCount() const noexcept;
    std::size_t ExecutedCount() const noexcept;
    std::size_t PendingCount() const noexcept;

private:
    std::deque<std::string> pending_commands_;
    std::size_t queued_total_ = 0;
    std::size_t executed_total_ = 0;
};
} // namespace hl::game_api
