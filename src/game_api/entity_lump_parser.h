#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace hl::game_api::detail
{
struct EntityKeyValuePair
{
    std::string key;
    std::string value;
};

struct EntityDefinition
{
    std::size_t ordinal = 0;
    std::string classname;
    std::vector<EntityKeyValuePair> key_values;
    std::string source_preview;
};

struct EntityParseError
{
    std::size_t entity_index = 0;
    std::string message;
    std::string nearby_text;
};

struct EntityLumpParseResult
{
    bool readable = false;
    bool parsed_any = false;
    bool partially_parsed = false;
    std::string failure_reason;
    std::vector<EntityDefinition> entities;
    std::vector<EntityParseError> errors;
};

class EntityLumpParser final
{
public:
    static EntityLumpParseResult Parse(
        std::string_view raw_text,
        std::size_t max_source_preview_chars = 160,
        std::size_t nearby_context_chars = 96);
};
} // namespace hl::game_api::detail
