#include "game_api/server_command_buffer.h"

#include <cctype>

#include "common/logger.h"

namespace
{
std::string TrimWhitespace(std::string_view text)
{
    std::size_t begin = 0;
    while (begin < text.size() && std::isspace(static_cast<unsigned char>(text[begin])) != 0)
    {
        ++begin;
    }

    std::size_t end = text.size();
    while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1])) != 0)
    {
        --end;
    }

    return std::string(text.substr(begin, end - begin));
}
} // namespace

namespace hl::game_api
{
void ServerCommandBuffer::Queue(std::string_view command_text)
{
    std::size_t cursor = 0;
    while (cursor <= command_text.size())
    {
        const std::size_t line_end = command_text.find_first_of("\r\n", cursor);
        const std::string command = TrimWhitespace(
            command_text.substr(cursor, line_end == std::string_view::npos
                ? std::string_view::npos
                : line_end - cursor));

        if (!command.empty())
        {
            pending_commands_.push_back(command);
            ++queued_total_;
            common::Logger::Info(
                "Queued server command: " + command
                + " (pending=" + std::to_string(pending_commands_.size()) + ")");
        }

        if (line_end == std::string_view::npos)
        {
            break;
        }

        cursor = command_text.find_first_not_of("\r\n", line_end);
        if (cursor == std::string_view::npos)
        {
            break;
        }
    }
}

bool ServerCommandBuffer::HasPending() const noexcept
{
    return !pending_commands_.empty();
}

std::string ServerCommandBuffer::PopFront()
{
    if (pending_commands_.empty())
    {
        return {};
    }

    std::string command = std::move(pending_commands_.front());
    pending_commands_.pop_front();
    return command;
}

std::vector<std::string> ServerCommandBuffer::Drain()
{
    std::vector<std::string> drained;
    drained.reserve(pending_commands_.size());
    while (!pending_commands_.empty())
    {
        drained.push_back(std::move(pending_commands_.front()));
        pending_commands_.pop_front();
    }
    return drained;
}

ServerCommandBufferSnapshot ServerCommandBuffer::Snapshot() const
{
    ServerCommandBufferSnapshot snapshot;
    snapshot.queued_total = queued_total_;
    snapshot.executed_total = executed_total_;
    snapshot.pending_total = pending_commands_.size();
    snapshot.pending_commands.assign(pending_commands_.begin(), pending_commands_.end());
    return snapshot;
}

void ServerCommandBuffer::MarkExecuted(std::size_t count) noexcept
{
    executed_total_ += count;
}

std::size_t ServerCommandBuffer::QueuedCount() const noexcept
{
    return queued_total_;
}

std::size_t ServerCommandBuffer::ExecutedCount() const noexcept
{
    return executed_total_;
}

std::size_t ServerCommandBuffer::PendingCount() const noexcept
{
    return pending_commands_.size();
}
} // namespace hl::game_api
