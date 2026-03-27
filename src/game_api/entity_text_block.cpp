#include "entity_text_block.h"

#include <algorithm>
#include <iomanip>
#include <sstream>

namespace
{
std::string TrimTrailingCarriageReturn(std::string value)
{
    while (!value.empty() && value.back() == '\r')
    {
        value.pop_back();
    }

    return value;
}

std::string TruncatePreviewLine(std::string value, std::size_t max_length = 160)
{
    if (value.size() <= max_length)
    {
        return value;
    }

    value.resize(max_length);
    value += "...";
    return value;
}
} // namespace

namespace hl::game_api::detail
{
std::string EntityTextBlockPreview::BuildPreviewText() const
{
    if (!present)
    {
        return "<missing>";
    }

    if (preview_lines.empty())
    {
        return "<empty>";
    }

    std::ostringstream stream;
    for (std::size_t index = 0; index < preview_lines.size(); ++index)
    {
        if (index > 0)
        {
            stream << '\n';
        }

        stream << std::setw(2) << std::setfill('0') << (index + 1) << " | " << preview_lines[index];
    }

    if (truncated_preview)
    {
        stream << "\n.. | ...";
    }

    return stream.str();
}

EntityTextBlockPreview BuildEntityTextBlockPreview(
    std::string_view raw_text,
    std::size_t max_preview_lines)
{
    EntityTextBlockPreview preview;
    preview.present = true;
    preview.byte_size = raw_text.size();

    const std::size_t terminator = raw_text.find('\0');
    preview.text = terminator == std::string_view::npos
        ? std::string(raw_text)
        : std::string(raw_text.substr(0, terminator));

    std::size_t line_begin = 0;
    while (line_begin <= preview.text.size())
    {
        const std::size_t line_end = preview.text.find('\n', line_begin);
        const std::size_t span = line_end == std::string::npos
            ? preview.text.size() - line_begin
            : line_end - line_begin;
        const std::string line = TruncatePreviewLine(
            TrimTrailingCarriageReturn(preview.text.substr(line_begin, span)));

        if (preview.preview_lines.size() < max_preview_lines)
        {
            preview.preview_lines.push_back(line);
        }
        else
        {
            preview.truncated_preview = true;
            break;
        }

        if (line_end == std::string::npos)
        {
            break;
        }

        line_begin = line_end + 1;
    }

    if (preview.preview_lines.size() == max_preview_lines)
    {
        const std::size_t trailing_line = preview.text.find('\n', line_begin);
        preview.truncated_preview = preview.truncated_preview
            || trailing_line != std::string::npos;
    }

    return preview;
}
} // namespace hl::game_api::detail
