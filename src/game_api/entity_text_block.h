#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace hl::game_api::detail
{
struct EntityTextBlockPreview
{
    bool present = false;
    bool truncated_preview = false;
    std::size_t byte_size = 0;
    std::string text;
    std::vector<std::string> preview_lines;

    std::string BuildPreviewText() const;
};

EntityTextBlockPreview BuildEntityTextBlockPreview(
    std::string_view raw_text,
    std::size_t max_preview_lines = 32);
} // namespace hl::game_api::detail
