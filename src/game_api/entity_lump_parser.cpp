#include "entity_lump_parser.h"

#include <algorithm>
#include <cctype>

namespace
{
enum class TokenType
{
    kEnd,
    kOpenBrace,
    kCloseBrace,
    kString,
    kError,
};

struct Token
{
    TokenType type = TokenType::kEnd;
    std::string text;
    std::size_t offset = 0;
    std::string error_message;
};

bool IsSpace(char character)
{
    return std::isspace(static_cast<unsigned char>(character)) != 0;
}

std::string CollapseWhitespace(std::string_view text)
{
    std::string collapsed;
    collapsed.reserve(text.size());

    bool previous_was_space = false;
    for (const char character : text)
    {
        if (IsSpace(character))
        {
            if (!previous_was_space)
            {
                collapsed.push_back(' ');
                previous_was_space = true;
            }

            continue;
        }

        previous_was_space = false;
        collapsed.push_back(character);
    }

    while (!collapsed.empty() && collapsed.front() == ' ')
    {
        collapsed.erase(collapsed.begin());
    }

    while (!collapsed.empty() && collapsed.back() == ' ')
    {
        collapsed.pop_back();
    }

    return collapsed;
}

std::string Truncate(std::string value, std::size_t max_chars)
{
    if (value.size() <= max_chars)
    {
        return value;
    }

    value.resize(max_chars);
    value += "...";
    return value;
}

std::string BuildSnippet(
    std::string_view text,
    std::size_t begin,
    std::size_t end,
    std::size_t max_chars)
{
    if (begin >= text.size())
    {
        return {};
    }

    end = std::min(end, text.size());
    if (end <= begin)
    {
        end = std::min(text.size(), begin + max_chars);
    }

    return Truncate(CollapseWhitespace(text.substr(begin, end - begin)), max_chars);
}

std::string BuildNearbyText(
    std::string_view text,
    std::size_t offset,
    std::size_t nearby_context_chars)
{
    const std::size_t begin = offset > nearby_context_chars ? offset - nearby_context_chars : 0;
    const std::size_t end = std::min(text.size(), offset + nearby_context_chars);
    return BuildSnippet(text, begin, end, nearby_context_chars * 2);
}

bool EqualsIgnoreCase(std::string_view left, std::string_view right)
{
    if (left.size() != right.size())
    {
        return false;
    }

    for (std::size_t index = 0; index < left.size(); ++index)
    {
        if (std::tolower(static_cast<unsigned char>(left[index]))
            != std::tolower(static_cast<unsigned char>(right[index])))
        {
            return false;
        }
    }

    return true;
}

class EntityLexer final
{
public:
    explicit EntityLexer(std::string_view text)
        : text_(text)
    {
    }

    Token Next()
    {
        SkipWhitespace();
        if (offset_ >= text_.size())
        {
            return {TokenType::kEnd, {}, offset_, {}};
        }

        const std::size_t token_offset = offset_;
        const char character = text_[offset_];
        if (character == '{')
        {
            ++offset_;
            return {TokenType::kOpenBrace, "{", token_offset, {}};
        }

        if (character == '}')
        {
            ++offset_;
            return {TokenType::kCloseBrace, "}", token_offset, {}};
        }

        if (character == '"')
        {
            return ParseQuoted(token_offset);
        }

        return ParseBare(token_offset);
    }

    std::size_t Offset() const noexcept
    {
        return offset_;
    }

private:
    Token ParseQuoted(std::size_t token_offset)
    {
        ++offset_;

        std::string text;
        while (offset_ < text_.size())
        {
            const char character = text_[offset_++];
            if (character == '"')
            {
                return {TokenType::kString, std::move(text), token_offset, {}};
            }

            if (character == '\\' && offset_ < text_.size())
            {
                const char escaped = text_[offset_++];
                switch (escaped)
                {
                case '\\':
                    text.push_back('\\');
                    break;
                case '"':
                    text.push_back('"');
                    break;
                case 'n':
                    text.push_back('\n');
                    break;
                case 'r':
                    text.push_back('\r');
                    break;
                case 't':
                    text.push_back('\t');
                    break;
                default:
                    text.push_back(escaped);
                    break;
                }

                continue;
            }

            text.push_back(character);
        }

        return {
            TokenType::kError,
            std::move(text),
            token_offset,
            "Unterminated quoted string.",
        };
    }

    Token ParseBare(std::size_t token_offset)
    {
        const std::size_t begin = offset_;
        while (offset_ < text_.size())
        {
            const char character = text_[offset_];
            if (IsSpace(character) || character == '{' || character == '}')
            {
                break;
            }

            ++offset_;
        }

        return {
            TokenType::kString,
            std::string(text_.substr(begin, offset_ - begin)),
            token_offset,
            {},
        };
    }

    void SkipWhitespace()
    {
        while (offset_ < text_.size() && IsSpace(text_[offset_]))
        {
            ++offset_;
        }
    }

    std::string_view text_;
    std::size_t offset_ = 0;
};

void AppendError(
    hl::game_api::detail::EntityLumpParseResult& result,
    std::size_t entity_index,
    std::string message,
    std::string_view text,
    std::size_t error_offset,
    std::size_t nearby_context_chars)
{
    result.errors.push_back({
        entity_index,
        std::move(message),
        BuildNearbyText(text, error_offset, nearby_context_chars),
    });
}
} // namespace

namespace hl::game_api::detail
{
EntityLumpParseResult EntityLumpParser::Parse(
    std::string_view raw_text,
    std::size_t max_source_preview_chars,
    std::size_t nearby_context_chars)
{
    EntityLumpParseResult result;
    result.readable = !raw_text.empty();

    EntityLexer lexer(raw_text);
    std::size_t expected_entity_index = 0;

    while (true)
    {
        const Token begin = lexer.Next();
        if (begin.type == TokenType::kEnd)
        {
            break;
        }

        if (begin.type == TokenType::kError)
        {
            AppendError(
                result,
                expected_entity_index,
                begin.error_message,
                raw_text,
                begin.offset,
                nearby_context_chars);
            break;
        }

        if (begin.type != TokenType::kOpenBrace)
        {
            AppendError(
                result,
                expected_entity_index,
                "Expected '{' at entity start, found '" + begin.text + "'.",
                raw_text,
                begin.offset,
                nearby_context_chars);
            continue;
        }

        EntityDefinition entity;
        entity.ordinal = expected_entity_index;
        const std::size_t entity_begin_offset = begin.offset;
        bool entity_closed = false;

        while (true)
        {
            const Token key = lexer.Next();
            if (key.type == TokenType::kEnd)
            {
                AppendError(
                    result,
                    entity.ordinal,
                    "Reached end of entities lump before closing '}'.",
                    raw_text,
                    lexer.Offset(),
                    nearby_context_chars);
                entity.source_preview = BuildSnippet(
                    raw_text,
                    entity_begin_offset,
                    lexer.Offset(),
                    max_source_preview_chars);
                if (!entity.key_values.empty() || !entity.classname.empty())
                {
                    result.entities.push_back(std::move(entity));
                    ++expected_entity_index;
                }

                result.parsed_any = !result.entities.empty();
                result.partially_parsed = result.parsed_any && !result.errors.empty();
                if (!result.parsed_any && result.failure_reason.empty())
                {
                    result.failure_reason = "Entities lump ended before a complete entity could be parsed.";
                }
                return result;
            }

            if (key.type == TokenType::kError)
            {
                AppendError(
                    result,
                    entity.ordinal,
                    key.error_message,
                    raw_text,
                    key.offset,
                    nearby_context_chars);
                continue;
            }

            if (key.type == TokenType::kCloseBrace)
            {
                entity_closed = true;
                entity.source_preview = BuildSnippet(
                    raw_text,
                    entity_begin_offset,
                    lexer.Offset(),
                    max_source_preview_chars);
                break;
            }

            if (key.type != TokenType::kString)
            {
                AppendError(
                    result,
                    entity.ordinal,
                    "Unexpected token while reading entity key.",
                    raw_text,
                    key.offset,
                    nearby_context_chars);
                continue;
            }

            const Token value = lexer.Next();
            if (value.type == TokenType::kEnd)
            {
                AppendError(
                    result,
                    entity.ordinal,
                    "Reached end of entities lump while reading value for key '" + key.text + "'.",
                    raw_text,
                    lexer.Offset(),
                    nearby_context_chars);
                entity.source_preview = BuildSnippet(
                    raw_text,
                    entity_begin_offset,
                    lexer.Offset(),
                    max_source_preview_chars);
                if (!entity.key_values.empty() || !entity.classname.empty())
                {
                    result.entities.push_back(std::move(entity));
                    ++expected_entity_index;
                }

                result.parsed_any = !result.entities.empty();
                result.partially_parsed = result.parsed_any && !result.errors.empty();
                if (!result.parsed_any && result.failure_reason.empty())
                {
                    result.failure_reason = "Entities lump ended before a complete key/value pair could be parsed.";
                }
                return result;
            }

            if (value.type == TokenType::kError)
            {
                AppendError(
                    result,
                    entity.ordinal,
                    value.error_message,
                    raw_text,
                    value.offset,
                    nearby_context_chars);
                continue;
            }

            if (value.type == TokenType::kCloseBrace)
            {
                AppendError(
                    result,
                    entity.ordinal,
                    "Missing value for key '" + key.text + "' before closing '}'.",
                    raw_text,
                    value.offset,
                    nearby_context_chars);
                entity_closed = true;
                entity.source_preview = BuildSnippet(
                    raw_text,
                    entity_begin_offset,
                    lexer.Offset(),
                    max_source_preview_chars);
                break;
            }

            if (value.type != TokenType::kString)
            {
                AppendError(
                    result,
                    entity.ordinal,
                    "Unexpected token while reading value for key '" + key.text + "'.",
                    raw_text,
                    value.offset,
                    nearby_context_chars);
                continue;
            }

            entity.key_values.push_back({key.text, value.text});
            if (EqualsIgnoreCase(key.text, "classname"))
            {
                entity.classname = value.text;
            }
        }

        if (!entity_closed)
        {
            continue;
        }

        result.entities.push_back(std::move(entity));
        ++expected_entity_index;
    }

    result.parsed_any = !result.entities.empty();
    result.partially_parsed = result.parsed_any && !result.errors.empty();
    if (!result.parsed_any && result.failure_reason.empty())
    {
        result.failure_reason = raw_text.empty()
            ? "Entities lump text is empty."
            : "No entity definitions could be parsed from the entities lump.";
    }

    return result;
}
} // namespace hl::game_api::detail
