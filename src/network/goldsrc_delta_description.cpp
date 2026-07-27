#include "network/goldsrc_delta_description.h"

#include "network/goldsrc_bitstream.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <limits>
#include <system_error>
#include <utility>

namespace
{
using namespace hl::network;

inline constexpr std::uint32_t kDeltaBaseTypeMask =
    kGoldSrcDeltaTypeByte
    | kGoldSrcDeltaTypeShort
    | kGoldSrcDeltaTypeFloat
    | kGoldSrcDeltaTypeInteger
    | kGoldSrcDeltaTypeAngle
    | kGoldSrcDeltaTypeTimeWindow8
    | kGoldSrcDeltaTypeTimeWindowBig
    | kGoldSrcDeltaTypeString;
inline constexpr std::uint32_t kDeltaSupportedTypeMask =
    kDeltaBaseTypeMask | kGoldSrcDeltaTypeSigned;
inline constexpr double kDeltaMultiplierScale = 4000.0;

enum class TokenKind
{
    kEnd,
    kWord,
    kOpeningBrace,
    kClosingBrace,
    kOpeningParenthesis,
    kClosingParenthesis,
    kComma,
    kPipe,
};

struct Token final
{
    TokenKind kind = TokenKind::kEnd;
    std::string_view text;
    std::size_t offset = 0u;
};

bool IsAsciiWhitespace(char character) noexcept
{
    switch (character)
    {
    case ' ':
    case '\t':
    case '\r':
    case '\n':
    case '\f':
    case '\v':
        return true;
    default:
        return false;
    }
}

bool IsNameStart(char character) noexcept
{
    return (character >= 'A' && character <= 'Z')
        || (character >= 'a' && character <= 'z')
        || character == '_';
}

bool IsIdentifierCharacter(char character) noexcept
{
    return IsNameStart(character)
        || (character >= '0' && character <= '9');
}

bool IsValidIdentifier(std::string_view value) noexcept
{
    if (value.empty() || value.size() > kGoldSrcMaximumDeltaNameBytes
        || !IsNameStart(value.front()))
    {
        return false;
    }
    return std::all_of(
        value.begin() + 1,
        value.end(),
        [](char character)
        {
            return IsIdentifierCharacter(character);
        });
}

bool IsValidDeltaName(std::string_view value) noexcept
{
    if (value.empty() || value.size() > kGoldSrcMaximumDeltaNameBytes
        || !IsNameStart(value.front()))
    {
        return false;
    }
    return std::all_of(
        value.begin() + 1,
        value.end(),
        [](char character)
        {
            return IsIdentifierCharacter(character)
                || character == '.'
                || character == '['
                || character == ']';
        });
}

class Lexer final
{
public:
    explicit Lexer(std::string_view input) noexcept
        : input_(input)
    {
    }

    Token Next() noexcept
    {
        SkipIgnored();
        if (position_ >= input_.size())
        {
            return {TokenKind::kEnd, {}, position_};
        }

        const std::size_t offset = position_;
        const char character = input_[position_++];
        switch (character)
        {
        case '{':
            return {TokenKind::kOpeningBrace, input_.substr(offset, 1u), offset};
        case '}':
            return {TokenKind::kClosingBrace, input_.substr(offset, 1u), offset};
        case '(':
            return {
                TokenKind::kOpeningParenthesis,
                input_.substr(offset, 1u),
                offset};
        case ')':
            return {
                TokenKind::kClosingParenthesis,
                input_.substr(offset, 1u),
                offset};
        case ',':
            return {TokenKind::kComma, input_.substr(offset, 1u), offset};
        case '|':
            return {TokenKind::kPipe, input_.substr(offset, 1u), offset};
        default:
            break;
        }

        while (position_ < input_.size())
        {
            const char next = input_[position_];
            if (IsAsciiWhitespace(next) || IsPunctuation(next)
                || (next == '/' && position_ + 1u < input_.size()
                    && input_[position_ + 1u] == '/'))
            {
                break;
            }
            ++position_;
        }
        return {
            TokenKind::kWord,
            input_.substr(offset, position_ - offset),
            offset};
    }

private:
    static bool IsPunctuation(char character) noexcept
    {
        return character == '{' || character == '}'
            || character == '(' || character == ')'
            || character == ',' || character == '|';
    }

    void SkipIgnored() noexcept
    {
        for (;;)
        {
            while (position_ < input_.size()
                && IsAsciiWhitespace(input_[position_]))
            {
                ++position_;
            }
            if (position_ + 1u >= input_.size()
                || input_[position_] != '/'
                || input_[position_ + 1u] != '/')
            {
                return;
            }
            position_ += 2u;
            while (position_ < input_.size()
                && input_[position_] != '\r'
                && input_[position_] != '\n')
            {
                ++position_;
            }
        }
    }

    std::string_view input_;
    std::size_t position_ = 0u;
};

const GoldSrcDeltaTableLayout* FindTableLayout(
    const GoldSrcDeltaLayoutRegistry& layouts,
    std::string_view name) noexcept
{
    const auto found = std::find_if(
        layouts.tables.begin(),
        layouts.tables.end(),
        [name](const GoldSrcDeltaTableLayout& candidate)
        {
            return candidate.name == name;
        });
    return found == layouts.tables.end() ? nullptr : &*found;
}

const GoldSrcDeltaFieldLayout* FindFieldLayout(
    const GoldSrcDeltaTableLayout& layout,
    std::string_view name) noexcept
{
    const auto found = std::find_if(
        layout.fields.begin(),
        layout.fields.end(),
        [name](const GoldSrcDeltaFieldLayout& candidate)
        {
            return candidate.name == name;
        });
    return found == layout.fields.end() ? nullptr : &*found;
}

bool IsSupportedGameDllConditionalEncoder(
    std::string_view table_name,
    std::string_view encoder_name) noexcept
{
    return (table_name == "entity_state_t"
            && encoder_name == "Entity_Encode")
        || (table_name == "entity_state_player_t"
            && encoder_name == "Player_Encode")
        || (table_name == "custom_entity_state_t"
            && encoder_name == "Custom_Encode");
}

bool LayoutRegistryIsValid(
    const GoldSrcDeltaLayoutRegistry& layouts) noexcept
{
    if (layouts.tables.size() > kGoldSrcMaximumDeltaTables)
    {
        return false;
    }
    for (std::size_t table_index = 0u;
        table_index < layouts.tables.size();
        ++table_index)
    {
        const GoldSrcDeltaTableLayout& table = layouts.tables[table_index];
        if (!IsValidDeltaName(table.name) || table.structure_size == 0u
            || table.structure_size > kGoldSrcMaximumDeltaDefinitionBytes
            || table.fields.size()
                > kGoldSrcMaximumDeltaLayoutFieldsPerTable)
        {
            return false;
        }
        for (std::size_t prior = 0u; prior < table_index; ++prior)
        {
            if (layouts.tables[prior].name == table.name)
            {
                return false;
            }
        }
        for (std::size_t field_index = 0u;
            field_index < table.fields.size();
            ++field_index)
        {
            const GoldSrcDeltaFieldLayout& field = table.fields[field_index];
            const std::uint32_t field_end =
                static_cast<std::uint32_t>(field.offset)
                + static_cast<std::uint32_t>(field.size);
            if (!IsValidDeltaName(field.name) || field.size == 0u
                || field_end > table.structure_size)
            {
                return false;
            }
            for (std::size_t prior = 0u; prior < field_index; ++prior)
            {
                if (table.fields[prior].name == field.name)
                {
                    return false;
                }
            }
        }
    }
    return true;
}

bool ParseUnsigned(
    std::string_view text,
    std::uint32_t* value,
    bool* overflow) noexcept
{
    if (value == nullptr || overflow == nullptr || text.empty())
    {
        return false;
    }
    *overflow = false;
    std::uint32_t parsed = 0u;
    const std::from_chars_result result = std::from_chars(
        text.data(),
        text.data() + text.size(),
        parsed,
        10);
    if (result.ec == std::errc::result_out_of_range)
    {
        *overflow = true;
        return false;
    }
    if (result.ec != std::errc{}
        || result.ptr != text.data() + text.size())
    {
        return false;
    }
    *value = parsed;
    return true;
}

bool ParseMultiplier(
    std::string_view text,
    double* value,
    bool* overflow) noexcept
{
    if (value == nullptr || overflow == nullptr || text.empty())
    {
        return false;
    }
    *overflow = false;
    double parsed = 0.0;
    const std::from_chars_result result = std::from_chars(
        text.data(),
        text.data() + text.size(),
        parsed,
        std::chars_format::general);
    if (result.ec == std::errc::result_out_of_range)
    {
        *overflow = true;
        return false;
    }
    if (result.ec != std::errc{}
        || result.ptr != text.data() + text.size()
        || !std::isfinite(parsed)
        || parsed <= 0.0)
    {
        return false;
    }
    *value = parsed;
    return true;
}

bool IsSupportedFieldType(std::uint32_t field_type) noexcept
{
    if ((field_type & ~kDeltaSupportedTypeMask) != 0u)
    {
        return false;
    }
    const std::uint32_t base_type = field_type & kDeltaBaseTypeMask;
    if (base_type == 0u || (base_type & (base_type - 1u)) != 0u)
    {
        return false;
    }
    return (field_type & kGoldSrcDeltaTypeSigned) == 0u
        || base_type != kGoldSrcDeltaTypeString;
}

std::uint32_t TypeFlagFor(std::string_view name) noexcept
{
    if (name == "DT_BYTE")
    {
        return kGoldSrcDeltaTypeByte;
    }
    if (name == "DT_SHORT")
    {
        return kGoldSrcDeltaTypeShort;
    }
    if (name == "DT_FLOAT")
    {
        return kGoldSrcDeltaTypeFloat;
    }
    if (name == "DT_INTEGER")
    {
        return kGoldSrcDeltaTypeInteger;
    }
    if (name == "DT_ANGLE")
    {
        return kGoldSrcDeltaTypeAngle;
    }
    if (name == "DT_TIMEWINDOW_8")
    {
        return kGoldSrcDeltaTypeTimeWindow8;
    }
    if (name == "DT_TIMEWINDOW_BIG")
    {
        return kGoldSrcDeltaTypeTimeWindowBig;
    }
    if (name == "DT_STRING")
    {
        return kGoldSrcDeltaTypeString;
    }
    if (name == "DT_SIGNED")
    {
        return kGoldSrcDeltaTypeSigned;
    }
    return 0u;
}

class DeltaParser final
{
public:
    DeltaParser(
        std::string_view input,
        const GoldSrcDeltaLayoutRegistry& layouts)
        : lexer_(input), layouts_(layouts)
    {
        current_ = lexer_.Next();
    }

    GoldSrcDeltaParseResult Parse()
    {
        if (!LayoutRegistryIsValid(layouts_))
        {
            return Fail(GoldSrcDeltaParseStatus::kInvalidLayout, 0u);
        }
        if (current_.kind == TokenKind::kEnd)
        {
            return Fail(GoldSrcDeltaParseStatus::kEmptyInput, current_.offset);
        }

        while (current_.kind != TokenKind::kEnd)
        {
            if (!ParseTable())
            {
                return std::move(result_);
            }
        }
        result_.status = GoldSrcDeltaParseStatus::kOk;
        result_.error_offset = 0u;
        return std::move(result_);
    }

private:
    GoldSrcDeltaParseResult Fail(
        GoldSrcDeltaParseStatus status,
        std::size_t offset)
    {
        result_.status = status;
        result_.error_offset = offset;
        result_.registry = {};
        return std::move(result_);
    }

    bool SetFailure(
        GoldSrcDeltaParseStatus status,
        std::size_t offset)
    {
        result_.status = status;
        result_.error_offset = offset;
        result_.registry = {};
        return false;
    }

    void Advance() noexcept
    {
        current_ = lexer_.Next();
    }

    bool Expect(TokenKind kind, GoldSrcDeltaParseStatus status)
    {
        if (current_.kind != kind)
        {
            return SetFailure(status, current_.offset);
        }
        Advance();
        return true;
    }

    void ConsumeOptionalComma() noexcept
    {
        if (current_.kind == TokenKind::kComma)
        {
            Advance();
        }
    }

    bool ParseTypeExpression(std::uint32_t* field_type)
    {
        if (field_type == nullptr || current_.kind != TokenKind::kWord)
        {
            return SetFailure(
                GoldSrcDeltaParseStatus::kUnknownFieldType,
                current_.offset);
        }

        std::uint32_t parsed = 0u;
        for (;;)
        {
            const std::uint32_t flag = TypeFlagFor(current_.text);
            if (flag == 0u)
            {
                return SetFailure(
                    GoldSrcDeltaParseStatus::kUnknownFieldType,
                    current_.offset);
            }
            if ((parsed & flag) != 0u)
            {
                return SetFailure(
                    GoldSrcDeltaParseStatus::kDuplicateFieldType,
                    current_.offset);
            }
            if (flag != kGoldSrcDeltaTypeSigned
                && (parsed & kDeltaBaseTypeMask) != 0u)
            {
                return SetFailure(
                    GoldSrcDeltaParseStatus::kDuplicateFieldType,
                    current_.offset);
            }
            parsed |= flag;
            Advance();
            if (current_.kind != TokenKind::kPipe)
            {
                break;
            }
            Advance();
            if (current_.kind != TokenKind::kWord)
            {
                return SetFailure(
                    GoldSrcDeltaParseStatus::kUnknownFieldType,
                    current_.offset);
            }
        }
        if (!IsSupportedFieldType(parsed))
        {
            return SetFailure(
                GoldSrcDeltaParseStatus::kUnknownFieldType,
                current_.offset);
        }
        *field_type = parsed;
        return true;
    }

    bool ParseField(
        GoldSrcDeltaTable* table,
        const GoldSrcDeltaTableLayout& layout)
    {
        if (table == nullptr)
        {
            return SetFailure(
                GoldSrcDeltaParseStatus::kInvalidLayout,
                current_.offset);
        }
        const bool has_postmultiply = current_.text == "DEFINE_DELTA_POST";
        if (!has_postmultiply && current_.text != "DEFINE_DELTA")
        {
            return SetFailure(
                GoldSrcDeltaParseStatus::kUnknownDirective,
                current_.offset);
        }
        const std::size_t directive_offset = current_.offset;
        Advance();
        if (!Expect(
                TokenKind::kOpeningParenthesis,
                GoldSrcDeltaParseStatus::kIncompleteDeclaration))
        {
            return false;
        }
        if (current_.kind != TokenKind::kWord)
        {
            return SetFailure(
                GoldSrcDeltaParseStatus::kIncompleteDeclaration,
                current_.offset);
        }
        if (current_.text.size() > kGoldSrcMaximumDeltaNameBytes)
        {
            return SetFailure(
                GoldSrcDeltaParseStatus::kNameTooLong,
                current_.offset);
        }
        if (!IsValidDeltaName(current_.text))
        {
            return SetFailure(
                GoldSrcDeltaParseStatus::kInvalidName,
                current_.offset);
        }
        const std::string field_name(current_.text);
        const GoldSrcDeltaFieldLayout* field_layout =
            FindFieldLayout(layout, field_name);
        if (field_layout == nullptr)
        {
            return SetFailure(
                GoldSrcDeltaParseStatus::kMissingFieldLayout,
                current_.offset);
        }
        if (std::any_of(
                table->fields.begin(),
                table->fields.end(),
                [&field_name](const GoldSrcDeltaField& field)
                {
                    return field.name == field_name;
                }))
        {
            return SetFailure(
                GoldSrcDeltaParseStatus::kDuplicateField,
                current_.offset);
        }
        Advance();
        ConsumeOptionalComma();

        GoldSrcDeltaField field{};
        field.name = field_name;
        field.field_offset = field_layout->offset;
        field.field_size = field_layout->size;
        if (!ParseTypeExpression(&field.field_type))
        {
            return false;
        }
        ConsumeOptionalComma();

        if (current_.kind != TokenKind::kWord)
        {
            return SetFailure(
                GoldSrcDeltaParseStatus::kIncompleteDeclaration,
                current_.offset);
        }
        std::uint32_t significant_bits = 0u;
        bool overflow = false;
        if (!ParseUnsigned(current_.text, &significant_bits, &overflow))
        {
            return SetFailure(
                overflow
                    ? GoldSrcDeltaParseStatus::kNumericOverflow
                    : GoldSrcDeltaParseStatus::kInvalidBitCount,
                current_.offset);
        }
        if (significant_bits == 0u || significant_bits > 32u)
        {
            return SetFailure(
                GoldSrcDeltaParseStatus::kInvalidBitCount,
                current_.offset);
        }
        field.significant_bits =
            static_cast<std::uint8_t>(significant_bits);
        Advance();
        ConsumeOptionalComma();

        if (current_.kind != TokenKind::kWord)
        {
            return SetFailure(
                GoldSrcDeltaParseStatus::kIncompleteDeclaration,
                current_.offset);
        }
        if (!ParseMultiplier(current_.text, &field.premultiply, &overflow))
        {
            return SetFailure(
                overflow
                    ? GoldSrcDeltaParseStatus::kNumericOverflow
                    : GoldSrcDeltaParseStatus::kInvalidMultiplier,
                current_.offset);
        }
        Advance();
        field.postmultiply = 1.0;
        if (has_postmultiply)
        {
            ConsumeOptionalComma();
            if (current_.kind != TokenKind::kWord)
            {
                return SetFailure(
                    GoldSrcDeltaParseStatus::kIncompleteDeclaration,
                    current_.offset);
            }
            if (!ParseMultiplier(
                    current_.text,
                    &field.postmultiply,
                    &overflow))
            {
                return SetFailure(
                    overflow
                        ? GoldSrcDeltaParseStatus::kNumericOverflow
                        : GoldSrcDeltaParseStatus::kInvalidMultiplier,
                    current_.offset);
            }
            Advance();
        }

        if (!Expect(
                TokenKind::kClosingParenthesis,
                GoldSrcDeltaParseStatus::kIncompleteDeclaration))
        {
            return false;
        }
        if (current_.kind == TokenKind::kComma)
        {
            Advance();
        }
        if (table->fields.size() >= kGoldSrcMaximumDeltaFieldsPerTable)
        {
            return SetFailure(
                GoldSrcDeltaParseStatus::kFieldCountExceeded,
                directive_offset);
        }
        table->fields.push_back(std::move(field));
        return true;
    }

    bool ParseTable()
    {
        if (current_.kind != TokenKind::kWord)
        {
            return SetFailure(
                GoldSrcDeltaParseStatus::kMissingTableName,
                current_.offset);
        }
        if (result_.registry.tables.size() >= kGoldSrcMaximumDeltaTables)
        {
            return SetFailure(
                GoldSrcDeltaParseStatus::kTableCountExceeded,
                current_.offset);
        }
        if (current_.text.size() > kGoldSrcMaximumDeltaNameBytes)
        {
            return SetFailure(
                GoldSrcDeltaParseStatus::kNameTooLong,
                current_.offset);
        }
        if (!IsValidDeltaName(current_.text))
        {
            return SetFailure(
                GoldSrcDeltaParseStatus::kInvalidName,
                current_.offset);
        }

        GoldSrcDeltaTable table{};
        table.name.assign(current_.text);
        const std::size_t table_offset = current_.offset;
        if (std::any_of(
                result_.registry.tables.begin(),
                result_.registry.tables.end(),
                [&table](const GoldSrcDeltaTable& candidate)
                {
                    return candidate.name == table.name;
                }))
        {
            return SetFailure(
                GoldSrcDeltaParseStatus::kDuplicateTable,
                table_offset);
        }
        const GoldSrcDeltaTableLayout* layout =
            FindTableLayout(layouts_, table.name);
        if (layout == nullptr)
        {
            return SetFailure(
                GoldSrcDeltaParseStatus::kMissingTableLayout,
                table_offset);
        }
        Advance();

        if (current_.kind != TokenKind::kWord)
        {
            return SetFailure(
                GoldSrcDeltaParseStatus::kUnsupportedConditionalEncoder,
                current_.offset);
        }
        if (current_.text == "none")
        {
            table.conditional_encoder_kind =
                GoldSrcDeltaConditionalEncoderKind::kNone;
            Advance();
        }
        else if (current_.text == "gamedll")
        {
            table.conditional_encoder_kind =
                GoldSrcDeltaConditionalEncoderKind::kGameDll;
            Advance();
            if (current_.kind != TokenKind::kWord)
            {
                return SetFailure(
                    GoldSrcDeltaParseStatus::kMissingConditionalEncoderName,
                    current_.offset);
            }
            if (current_.text.size() > kGoldSrcMaximumDeltaNameBytes)
            {
                return SetFailure(
                    GoldSrcDeltaParseStatus::kNameTooLong,
                    current_.offset);
            }
            if (!IsValidIdentifier(current_.text))
            {
                return SetFailure(
                    GoldSrcDeltaParseStatus::kInvalidName,
                    current_.offset);
            }
            if (!IsSupportedGameDllConditionalEncoder(
                    table.name,
                    current_.text))
            {
                return SetFailure(
                    GoldSrcDeltaParseStatus::kUnsupportedConditionalEncoder,
                    current_.offset);
            }
            table.conditional_encoder_name.assign(current_.text);
            Advance();
        }
        else
        {
            return SetFailure(
                GoldSrcDeltaParseStatus::kUnsupportedConditionalEncoder,
                current_.offset);
        }

        if (!Expect(
                TokenKind::kOpeningBrace,
                GoldSrcDeltaParseStatus::kMissingOpeningBrace))
        {
            return false;
        }
        while (current_.kind != TokenKind::kClosingBrace)
        {
            if (current_.kind == TokenKind::kEnd)
            {
                return SetFailure(
                    GoldSrcDeltaParseStatus::kMissingClosingBrace,
                    current_.offset);
            }
            if (current_.kind == TokenKind::kComma)
            {
                Advance();
                continue;
            }
            if (current_.kind != TokenKind::kWord)
            {
                return SetFailure(
                    GoldSrcDeltaParseStatus::kUnknownDirective,
                    current_.offset);
            }
            if (!ParseField(&table, *layout))
            {
                return false;
            }
        }
        Advance();
        if (current_.kind == TokenKind::kComma)
        {
            Advance();
        }
        result_.registry.tables.push_back(std::move(table));
        return true;
    }

    Lexer lexer_;
    const GoldSrcDeltaLayoutRegistry& layouts_;
    Token current_{};
    GoldSrcDeltaParseResult result_{};
};

bool QuantizeMultiplier(
    double multiplier,
    std::uint32_t* quantized) noexcept
{
    if (quantized == nullptr || !std::isfinite(multiplier)
        || multiplier <= 0.0)
    {
        return false;
    }
    const double scaled = multiplier * kDeltaMultiplierScale;
    if (!std::isfinite(scaled) || scaled < 1.0
        || scaled
            > static_cast<double>(
                std::numeric_limits<std::uint32_t>::max()))
    {
        return false;
    }
    *quantized = static_cast<std::uint32_t>(scaled);
    return *quantized != 0u;
}

GoldSrcDeltaEncodeStatus ValidateFieldForWire(
    const GoldSrcDeltaField& field) noexcept
{
    if (!IsSupportedFieldType(field.field_type))
    {
        return GoldSrcDeltaEncodeStatus::kInvalidFieldType;
    }
    if (!IsValidDeltaName(field.name))
    {
        return GoldSrcDeltaEncodeStatus::kInvalidFieldName;
    }
    if (field.field_size == 0u
        || static_cast<std::uint32_t>(field.field_offset)
                + static_cast<std::uint32_t>(field.field_size)
            > kGoldSrcMaximumDeltaDefinitionBytes)
    {
        return GoldSrcDeltaEncodeStatus::kInvalidFieldLayout;
    }
    if (field.significant_bits == 0u || field.significant_bits > 32u)
    {
        return GoldSrcDeltaEncodeStatus::kInvalidBitCount;
    }
    if (!std::isfinite(field.premultiply)
        || !std::isfinite(field.postmultiply)
        || field.premultiply <= 0.0
        || field.postmultiply <= 0.0)
    {
        return GoldSrcDeltaEncodeStatus::kInvalidMultiplier;
    }
    std::uint32_t unused = 0u;
    if (!QuantizeMultiplier(field.premultiply, &unused)
        || !QuantizeMultiplier(field.postmultiply, &unused))
    {
        return GoldSrcDeltaEncodeStatus::kMultiplierOutOfRange;
    }
    return GoldSrcDeltaEncodeStatus::kOk;
}

GoldSrcDeltaEncodeStatus WriteFieldDescriptor(
    GoldSrcBitWriter* writer,
    const GoldSrcDeltaField& field) noexcept
{
    if (writer == nullptr)
    {
        return GoldSrcDeltaEncodeStatus::kOutputCapacityExceeded;
    }

    if (field.field_type != 0u
        && !IsSupportedFieldType(field.field_type))
    {
        return GoldSrcDeltaEncodeStatus::kInvalidFieldType;
    }
    if (!field.name.empty() && !IsValidDeltaName(field.name))
    {
        return GoldSrcDeltaEncodeStatus::kInvalidFieldName;
    }
    if (field.field_size != 0u
        && static_cast<std::uint32_t>(field.field_offset)
                + static_cast<std::uint32_t>(field.field_size)
            > kGoldSrcMaximumDeltaDefinitionBytes)
    {
        return GoldSrcDeltaEncodeStatus::kInvalidFieldLayout;
    }
    if (field.significant_bits > 32u)
    {
        return GoldSrcDeltaEncodeStatus::kInvalidBitCount;
    }

    std::uint32_t premultiply = 0u;
    std::uint32_t postmultiply = 0u;
    if (field.premultiply != 0.0
        && !QuantizeMultiplier(field.premultiply, &premultiply))
    {
        return std::isfinite(field.premultiply)
            && field.premultiply > 0.0
            ? GoldSrcDeltaEncodeStatus::kMultiplierOutOfRange
            : GoldSrcDeltaEncodeStatus::kInvalidMultiplier;
    }
    if (field.postmultiply != 0.0
        && !QuantizeMultiplier(field.postmultiply, &postmultiply))
    {
        return std::isfinite(field.postmultiply)
            && field.postmultiply > 0.0
            ? GoldSrcDeltaEncodeStatus::kMultiplierOutOfRange
            : GoldSrcDeltaEncodeStatus::kInvalidMultiplier;
    }

    std::uint8_t mask = 0u;
    if (field.field_type != 0u)
    {
        mask |= 1u << 0u;
    }
    if (!field.name.empty())
    {
        mask |= 1u << 1u;
    }
    if (field.field_offset != 0u)
    {
        mask |= 1u << 2u;
    }
    if (field.field_size != 0u)
    {
        mask |= 1u << 3u;
    }
    if (field.significant_bits != 0u)
    {
        mask |= 1u << 4u;
    }
    if (premultiply != 0u)
    {
        mask |= 1u << 5u;
    }
    if (postmultiply != 0u)
    {
        mask |= 1u << 6u;
    }

    const std::uint32_t mask_byte_count = mask == 0u ? 0u : 1u;
    bool wrote = writer->WriteBits(mask_byte_count, 3u);
    if (mask_byte_count != 0u)
    {
        wrote = wrote && writer->WriteBits(mask, 8u);
    }
    if ((mask & (1u << 0u)) != 0u)
    {
        wrote = wrote && writer->WriteBits(field.field_type, 32u);
    }
    if ((mask & (1u << 1u)) != 0u)
    {
        wrote = wrote && writer->WriteString(field.name);
    }
    if ((mask & (1u << 2u)) != 0u)
    {
        wrote = wrote && writer->WriteBits(field.field_offset, 16u);
    }
    if ((mask & (1u << 3u)) != 0u)
    {
        wrote = wrote && writer->WriteBits(field.field_size, 8u);
    }
    if ((mask & (1u << 4u)) != 0u)
    {
        wrote = wrote && writer->WriteBits(field.significant_bits, 8u);
    }
    if ((mask & (1u << 5u)) != 0u)
    {
        wrote = wrote && writer->WriteBits(premultiply, 32u);
    }
    if ((mask & (1u << 6u)) != 0u)
    {
        wrote = wrote && writer->WriteBits(postmultiply, 32u);
    }
    return wrote
        ? GoldSrcDeltaEncodeStatus::kOk
        : GoldSrcDeltaEncodeStatus::kOutputCapacityExceeded;
}

const GoldSrcDeltaTable* FindRegistryTable(
    const GoldSrcDeltaRegistry& registry,
    std::string_view name,
    std::size_t* count) noexcept
{
    const GoldSrcDeltaTable* found = nullptr;
    std::size_t matches = 0u;
    for (const GoldSrcDeltaTable& table : registry.tables)
    {
        if (table.name == name)
        {
            found = &table;
            ++matches;
        }
    }
    if (count != nullptr)
    {
        *count = matches;
    }
    return found;
}

enum class NameReadResult
{
    kOk,
    kTruncated,
    kTooLong,
};

NameReadResult ReadBoundedName(
    GoldSrcBitReader* reader,
    std::string* output)
{
    if (reader == nullptr || output == nullptr)
    {
        return NameReadResult::kTruncated;
    }
    output->clear();
    for (std::size_t index = 0u;
        index <= kGoldSrcMaximumDeltaNameBytes;
        ++index)
    {
        std::uint32_t value = 0u;
        if (!reader->ReadBits(8u, &value))
        {
            output->clear();
            return NameReadResult::kTruncated;
        }
        if (value == 0u)
        {
            return NameReadResult::kOk;
        }
        if (index == kGoldSrcMaximumDeltaNameBytes)
        {
            output->clear();
            return NameReadResult::kTooLong;
        }
        output->push_back(static_cast<char>(value));
    }
    output->clear();
    return NameReadResult::kTooLong;
}

GoldSrcDeltaDecodeStatus DecodeFieldDescriptor(
    GoldSrcBitReader* reader,
    GoldSrcDeltaField* field)
{
    if (reader == nullptr || field == nullptr)
    {
        return GoldSrcDeltaDecodeStatus::kTruncatedPayload;
    }
    *field = {};
    std::uint32_t mask_byte_count = 0u;
    if (!reader->ReadBits(3u, &mask_byte_count))
    {
        return GoldSrcDeltaDecodeStatus::kTruncatedPayload;
    }
    if (mask_byte_count != 1u)
    {
        return GoldSrcDeltaDecodeStatus::kInvalidMaskByteCount;
    }
    std::uint32_t mask = 0u;
    if (!reader->ReadBits(8u, &mask))
    {
        return GoldSrcDeltaDecodeStatus::kTruncatedPayload;
    }
    if ((mask & 0x80u) != 0u)
    {
        return GoldSrcDeltaDecodeStatus::kInvalidMask;
    }
    if ((mask & (1u << 1u)) == 0u)
    {
        return GoldSrcDeltaDecodeStatus::kMissingFieldName;
    }

    std::uint32_t value = 0u;
    if ((mask & (1u << 0u)) != 0u)
    {
        if (!reader->ReadBits(32u, &field->field_type))
        {
            return GoldSrcDeltaDecodeStatus::kTruncatedPayload;
        }
    }
    if ((mask & (1u << 1u)) != 0u)
    {
        const NameReadResult name_result = ReadBoundedName(reader, &field->name);
        if (name_result == NameReadResult::kTruncated)
        {
            return GoldSrcDeltaDecodeStatus::kTruncatedPayload;
        }
        if (name_result == NameReadResult::kTooLong)
        {
            return GoldSrcDeltaDecodeStatus::kNameTooLong;
        }
    }
    if ((mask & (1u << 2u)) != 0u)
    {
        if (!reader->ReadBits(16u, &value))
        {
            return GoldSrcDeltaDecodeStatus::kTruncatedPayload;
        }
        field->field_offset = static_cast<std::uint16_t>(value);
    }
    if ((mask & (1u << 3u)) != 0u)
    {
        if (!reader->ReadBits(8u, &value))
        {
            return GoldSrcDeltaDecodeStatus::kTruncatedPayload;
        }
        field->field_size = static_cast<std::uint8_t>(value);
    }
    if ((mask & (1u << 4u)) != 0u)
    {
        if (!reader->ReadBits(8u, &value))
        {
            return GoldSrcDeltaDecodeStatus::kTruncatedPayload;
        }
        field->significant_bits = static_cast<std::uint8_t>(value);
    }
    if ((mask & (1u << 5u)) != 0u)
    {
        if (!reader->ReadBits(32u, &value))
        {
            return GoldSrcDeltaDecodeStatus::kTruncatedPayload;
        }
        field->premultiply =
            static_cast<double>(value) / kDeltaMultiplierScale;
    }
    if ((mask & (1u << 6u)) != 0u)
    {
        if (!reader->ReadBits(32u, &value))
        {
            return GoldSrcDeltaDecodeStatus::kTruncatedPayload;
        }
        field->postmultiply =
            static_cast<double>(value) / kDeltaMultiplierScale;
    }

    if (!IsSupportedFieldType(field->field_type))
    {
        return GoldSrcDeltaDecodeStatus::kInvalidFieldType;
    }
    if (!IsValidDeltaName(field->name))
    {
        return GoldSrcDeltaDecodeStatus::kInvalidName;
    }
    if (field->field_size == 0u
        || static_cast<std::uint32_t>(field->field_offset)
                + static_cast<std::uint32_t>(field->field_size)
            > kGoldSrcMaximumDeltaDefinitionBytes)
    {
        return GoldSrcDeltaDecodeStatus::kInvalidFieldLayout;
    }
    if (field->significant_bits == 0u || field->significant_bits > 32u)
    {
        return GoldSrcDeltaDecodeStatus::kInvalidBitCount;
    }
    if (!std::isfinite(field->premultiply)
        || !std::isfinite(field->postmultiply)
        || field->premultiply <= 0.0
        || field->postmultiply <= 0.0)
    {
        return GoldSrcDeltaDecodeStatus::kInvalidMultiplier;
    }
    return GoldSrcDeltaDecodeStatus::kOk;
}
} // namespace

namespace hl::network
{
std::string_view ReasonFor(GoldSrcDeltaParseStatus status) noexcept
{
    switch (status)
    {
    case GoldSrcDeltaParseStatus::kOk:
        return "ok";
    case GoldSrcDeltaParseStatus::kEmptyInput:
        return "empty_input";
    case GoldSrcDeltaParseStatus::kInputTooLarge:
        return "input_too_large";
    case GoldSrcDeltaParseStatus::kMissingTableName:
        return "missing_table_name";
    case GoldSrcDeltaParseStatus::kNameTooLong:
        return "name_too_long";
    case GoldSrcDeltaParseStatus::kInvalidName:
        return "invalid_name";
    case GoldSrcDeltaParseStatus::kUnsupportedConditionalEncoder:
        return "unsupported_conditional_encoder";
    case GoldSrcDeltaParseStatus::kMissingConditionalEncoderName:
        return "missing_conditional_encoder_name";
    case GoldSrcDeltaParseStatus::kMissingOpeningBrace:
        return "missing_opening_brace";
    case GoldSrcDeltaParseStatus::kMissingClosingBrace:
        return "missing_closing_brace";
    case GoldSrcDeltaParseStatus::kUnknownDirective:
        return "unknown_directive";
    case GoldSrcDeltaParseStatus::kIncompleteDeclaration:
        return "incomplete_declaration";
    case GoldSrcDeltaParseStatus::kUnknownFieldType:
        return "unknown_field_type";
    case GoldSrcDeltaParseStatus::kDuplicateFieldType:
        return "duplicate_field_type";
    case GoldSrcDeltaParseStatus::kInvalidBitCount:
        return "invalid_bit_count";
    case GoldSrcDeltaParseStatus::kInvalidMultiplier:
        return "invalid_multiplier";
    case GoldSrcDeltaParseStatus::kNumericOverflow:
        return "numeric_overflow";
    case GoldSrcDeltaParseStatus::kTableCountExceeded:
        return "table_count_exceeded";
    case GoldSrcDeltaParseStatus::kFieldCountExceeded:
        return "field_count_exceeded";
    case GoldSrcDeltaParseStatus::kDuplicateTable:
        return "duplicate_table";
    case GoldSrcDeltaParseStatus::kDuplicateField:
        return "duplicate_field";
    case GoldSrcDeltaParseStatus::kMissingTableLayout:
        return "missing_table_layout";
    case GoldSrcDeltaParseStatus::kMissingFieldLayout:
        return "missing_field_layout";
    case GoldSrcDeltaParseStatus::kInvalidLayout:
        return "invalid_layout";
    case GoldSrcDeltaParseStatus::kTrailingData:
        return "trailing_data";
    default:
        return "invalid_layout";
    }
}

GoldSrcDeltaParseResult ParseGoldSrcDeltaDefinitions(
    std::string_view definitions,
    const GoldSrcDeltaLayoutRegistry& layouts)
{
    if (definitions.size() > kGoldSrcMaximumDeltaDefinitionBytes)
    {
        GoldSrcDeltaParseResult result{};
        result.status = GoldSrcDeltaParseStatus::kInputTooLarge;
        result.error_offset = kGoldSrcMaximumDeltaDefinitionBytes;
        return result;
    }
    return DeltaParser(definitions, layouts).Parse();
}

std::string_view ReasonFor(GoldSrcDeltaEncodeStatus status) noexcept
{
    switch (status)
    {
    case GoldSrcDeltaEncodeStatus::kOk:
        return "ok";
    case GoldSrcDeltaEncodeStatus::kMissingRequiredTable:
        return "missing_required_table";
    case GoldSrcDeltaEncodeStatus::kDuplicateRequiredTable:
        return "duplicate_required_table";
    case GoldSrcDeltaEncodeStatus::kUnexpectedTable:
        return "unexpected_table";
    case GoldSrcDeltaEncodeStatus::kEmptyTable:
        return "empty_table";
    case GoldSrcDeltaEncodeStatus::kFieldCountExceeded:
        return "field_count_exceeded";
    case GoldSrcDeltaEncodeStatus::kInvalidFieldType:
        return "invalid_field_type";
    case GoldSrcDeltaEncodeStatus::kInvalidFieldName:
        return "invalid_field_name";
    case GoldSrcDeltaEncodeStatus::kInvalidFieldLayout:
        return "invalid_field_layout";
    case GoldSrcDeltaEncodeStatus::kInvalidBitCount:
        return "invalid_bit_count";
    case GoldSrcDeltaEncodeStatus::kInvalidMultiplier:
        return "invalid_multiplier";
    case GoldSrcDeltaEncodeStatus::kMultiplierOutOfRange:
        return "multiplier_out_of_range";
    case GoldSrcDeltaEncodeStatus::kDuplicateField:
        return "duplicate_field";
    case GoldSrcDeltaEncodeStatus::kOutputCapacityExceeded:
        return "output_capacity_exceeded";
    default:
        return "invalid_field_type";
    }
}

GoldSrcDeltaDescriptorEncodeResult EncodeGoldSrcDeltaFieldDescriptor(
    const GoldSrcDeltaField& field,
    std::size_t output_capacity) noexcept
{
    GoldSrcDeltaDescriptorEncodeResult result{};
    const std::size_t bounded_capacity =
        std::min(output_capacity, result.payload.bytes.size());
    GoldSrcBitWriter writer(result.payload.bytes.data(), bounded_capacity);
    result.status = WriteFieldDescriptor(&writer, field);
    if (result.status != GoldSrcDeltaEncodeStatus::kOk)
    {
        result.payload = {};
        return result;
    }
    result.payload.bit_count = writer.bit_position();
    result.payload.size = writer.bytes_written();
    return result;
}

GoldSrcDeltaBundleEncodeResult EncodeGoldSrcCanonicalDeltaBundle(
    const GoldSrcDeltaRegistry& registry,
    std::size_t output_capacity) noexcept
{
    GoldSrcDeltaBundleEncodeResult result{};

    const std::size_t bounded_capacity = std::min(
        output_capacity,
        result.payload.bytes.size());
    std::size_t output_offset = 0u;
    for (const std::string_view required_name :
        kGoldSrcCanonicalDeltaTableOrder)
    {
        std::size_t matches = 0u;
        const GoldSrcDeltaTable* table =
            FindRegistryTable(registry, required_name, &matches);
        if (matches == 0u || table == nullptr)
        {
            result.status = GoldSrcDeltaEncodeStatus::kMissingRequiredTable;
            result.payload = {};
            return result;
        }
        if (matches != 1u)
        {
            result.status = GoldSrcDeltaEncodeStatus::kDuplicateRequiredTable;
            result.payload = {};
            return result;
        }
        if (table->fields.empty())
        {
            result.status = GoldSrcDeltaEncodeStatus::kEmptyTable;
            result.payload = {};
            return result;
        }
        if (table->fields.size() > kGoldSrcMaximumDeltaFieldsPerTable)
        {
            result.status = GoldSrcDeltaEncodeStatus::kFieldCountExceeded;
            result.payload = {};
            return result;
        }

        for (std::size_t field_index = 0u;
            field_index < table->fields.size();
            ++field_index)
        {
            const GoldSrcDeltaField& field = table->fields[field_index];
            result.status = ValidateFieldForWire(field);
            if (result.status != GoldSrcDeltaEncodeStatus::kOk)
            {
                result.payload = {};
                return result;
            }
            for (std::size_t prior = 0u; prior < field_index; ++prior)
            {
                if (table->fields[prior].name == field.name)
                {
                    result.status =
                        GoldSrcDeltaEncodeStatus::kDuplicateField;
                    result.payload = {};
                    return result;
                }
            }
        }

        if (output_offset >= bounded_capacity)
        {
            result.status =
                GoldSrcDeltaEncodeStatus::kOutputCapacityExceeded;
            result.payload = {};
            return result;
        }
        GoldSrcBitWriter writer(
            result.payload.bytes.data() + output_offset,
            bounded_capacity - output_offset);
        bool wrote = writer.WriteBits(kGoldSrcDeltaDescriptionOpcode, 8u)
            && writer.WriteString(required_name)
            && writer.WriteBits(
                static_cast<std::uint32_t>(table->fields.size()),
                16u);
        for (const GoldSrcDeltaField& field : table->fields)
        {
            if (!wrote)
            {
                break;
            }
            const GoldSrcDeltaEncodeStatus field_status =
                WriteFieldDescriptor(&writer, field);
            if (field_status != GoldSrcDeltaEncodeStatus::kOk)
            {
                result.status = field_status;
                result.payload = {};
                return result;
            }
        }
        wrote = wrote && writer.PadToByte();
        if (!wrote || writer.bytes_written() == 0u
            || writer.bytes_written() > bounded_capacity - output_offset)
        {
            result.status =
                GoldSrcDeltaEncodeStatus::kOutputCapacityExceeded;
            result.payload = {};
            return result;
        }
        output_offset += writer.bytes_written();
        ++result.payload.table_count;
        result.payload.field_count += table->fields.size();
    }

    for (const GoldSrcDeltaTable& table : registry.tables)
    {
        if (std::find(
                kGoldSrcCanonicalDeltaTableOrder.begin(),
                kGoldSrcCanonicalDeltaTableOrder.end(),
                table.name)
            == kGoldSrcCanonicalDeltaTableOrder.end())
        {
            result.status = GoldSrcDeltaEncodeStatus::kUnexpectedTable;
            result.payload = {};
            return result;
        }
    }

    result.payload.size = output_offset;
    result.payload.bit_count = output_offset * 8u;
    result.status = GoldSrcDeltaEncodeStatus::kOk;
    return result;
}

std::string_view ReasonFor(GoldSrcDeltaDecodeStatus status) noexcept
{
    switch (status)
    {
    case GoldSrcDeltaDecodeStatus::kOk:
        return "ok";
    case GoldSrcDeltaDecodeStatus::kNullInput:
        return "null_input";
    case GoldSrcDeltaDecodeStatus::kEmptyPayload:
        return "empty_payload";
    case GoldSrcDeltaDecodeStatus::kPayloadTooLarge:
        return "payload_too_large";
    case GoldSrcDeltaDecodeStatus::kTruncatedPayload:
        return "truncated_payload";
    case GoldSrcDeltaDecodeStatus::kWrongOpcode:
        return "wrong_opcode";
    case GoldSrcDeltaDecodeStatus::kUnexpectedTable:
        return "unexpected_table";
    case GoldSrcDeltaDecodeStatus::kNameTooLong:
        return "name_too_long";
    case GoldSrcDeltaDecodeStatus::kInvalidName:
        return "invalid_name";
    case GoldSrcDeltaDecodeStatus::kInvalidFieldCount:
        return "invalid_field_count";
    case GoldSrcDeltaDecodeStatus::kInvalidMaskByteCount:
        return "invalid_mask_byte_count";
    case GoldSrcDeltaDecodeStatus::kInvalidMask:
        return "invalid_mask";
    case GoldSrcDeltaDecodeStatus::kMissingFieldName:
        return "missing_field_name";
    case GoldSrcDeltaDecodeStatus::kInvalidFieldType:
        return "invalid_field_type";
    case GoldSrcDeltaDecodeStatus::kInvalidFieldLayout:
        return "invalid_field_layout";
    case GoldSrcDeltaDecodeStatus::kInvalidBitCount:
        return "invalid_bit_count";
    case GoldSrcDeltaDecodeStatus::kInvalidMultiplier:
        return "invalid_multiplier";
    case GoldSrcDeltaDecodeStatus::kDuplicateField:
        return "duplicate_field";
    case GoldSrcDeltaDecodeStatus::kNonZeroPadding:
        return "non_zero_padding";
    case GoldSrcDeltaDecodeStatus::kTrailingData:
        return "trailing_data";
    default:
        return "truncated_payload";
    }
}

GoldSrcDeltaBundleDecodeResult DecodeGoldSrcCanonicalDeltaBundle(
    const std::uint8_t* bytes,
    std::size_t size)
{
    GoldSrcDeltaBundleDecodeResult result{};
    if (bytes == nullptr)
    {
        result.status = size == 0u
            ? GoldSrcDeltaDecodeStatus::kEmptyPayload
            : GoldSrcDeltaDecodeStatus::kNullInput;
        return result;
    }
    if (size == 0u)
    {
        result.status = GoldSrcDeltaDecodeStatus::kEmptyPayload;
        return result;
    }
    if (size > kGoldSrcMaximumDeltaBundleBytes)
    {
        result.status = GoldSrcDeltaDecodeStatus::kPayloadTooLarge;
        return result;
    }

    std::size_t input_offset = 0u;
    for (const std::string_view expected_name :
        kGoldSrcCanonicalDeltaTableOrder)
    {
        if (input_offset >= size)
        {
            result.status = GoldSrcDeltaDecodeStatus::kTruncatedPayload;
            result.registry = {};
            return result;
        }
        GoldSrcBitReader reader(bytes + input_offset, size - input_offset);
        std::uint32_t value = 0u;
        if (!reader.ReadBits(8u, &value))
        {
            result.status = GoldSrcDeltaDecodeStatus::kTruncatedPayload;
            result.registry = {};
            return result;
        }
        if (value != kGoldSrcDeltaDescriptionOpcode)
        {
            result.status = GoldSrcDeltaDecodeStatus::kWrongOpcode;
            result.registry = {};
            return result;
        }

        GoldSrcDeltaTable table{};
        const NameReadResult table_name_result =
            ReadBoundedName(&reader, &table.name);
        if (table_name_result == NameReadResult::kTruncated)
        {
            result.status = GoldSrcDeltaDecodeStatus::kTruncatedPayload;
            result.registry = {};
            return result;
        }
        if (table_name_result == NameReadResult::kTooLong)
        {
            result.status = GoldSrcDeltaDecodeStatus::kNameTooLong;
            result.registry = {};
            return result;
        }
        if (!IsValidDeltaName(table.name))
        {
            result.status = GoldSrcDeltaDecodeStatus::kInvalidName;
            result.registry = {};
            return result;
        }
        if (table.name != expected_name)
        {
            result.status = GoldSrcDeltaDecodeStatus::kUnexpectedTable;
            result.registry = {};
            return result;
        }
        if (!reader.ReadBits(16u, &value))
        {
            result.status = GoldSrcDeltaDecodeStatus::kTruncatedPayload;
            result.registry = {};
            return result;
        }
        const std::size_t field_count = value;
        if (field_count == 0u
            || field_count > kGoldSrcMaximumDeltaFieldsPerTable)
        {
            result.status = GoldSrcDeltaDecodeStatus::kInvalidFieldCount;
            result.registry = {};
            return result;
        }
        table.fields.reserve(field_count);
        for (std::size_t field_index = 0u;
            field_index < field_count;
            ++field_index)
        {
            GoldSrcDeltaField field{};
            result.status = DecodeFieldDescriptor(&reader, &field);
            if (result.status != GoldSrcDeltaDecodeStatus::kOk)
            {
                result.registry = {};
                return result;
            }
            if (std::any_of(
                    table.fields.begin(),
                    table.fields.end(),
                    [&field](const GoldSrcDeltaField& candidate)
                    {
                        return candidate.name == field.name;
                    }))
            {
                result.status = GoldSrcDeltaDecodeStatus::kDuplicateField;
                result.registry = {};
                return result;
            }
            table.fields.push_back(std::move(field));
        }

        const std::size_t remainder = reader.bit_position() % 8u;
        if (remainder != 0u)
        {
            const std::size_t padding = 8u - remainder;
            if (reader.bits_remaining() < padding)
            {
                result.status = GoldSrcDeltaDecodeStatus::kTruncatedPayload;
                result.registry = {};
                return result;
            }
            std::uint32_t padding_value = 0u;
            if (!reader.ReadBits(padding, &padding_value))
            {
                result.status = GoldSrcDeltaDecodeStatus::kTruncatedPayload;
                result.registry = {};
                return result;
            }
            if (padding_value != 0u)
            {
                result.status = GoldSrcDeltaDecodeStatus::kNonZeroPadding;
                result.registry = {};
                return result;
            }
        }
        input_offset += reader.bytes_read();
        result.registry.tables.push_back(std::move(table));
    }

    if (input_offset != size)
    {
        result.status = GoldSrcDeltaDecodeStatus::kTrailingData;
        result.registry = {};
        return result;
    }
    result.bytes_consumed = input_offset;
    result.bits_consumed = input_offset * 8u;
    result.status = GoldSrcDeltaDecodeStatus::kOk;
    return result;
}
} // namespace hl::network
