#include "network/goldsrc_bitstream.h"
#include "network/goldsrc_delta_description.h"
#include "network/goldsrc_netchan.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
using namespace hl::network;

inline constexpr std::array<std::string_view, 15u>
    kVerifiedUsercmdFieldNames = {
        "lerp_msec",
        "msec",
        "viewangles[1]",
        "viewangles[0]",
        "buttons",
        "forwardmove",
        "lightlevel",
        "sidemove",
        "upmove",
        "impulse",
        "viewangles[2]",
        "impact_index",
        "impact_position[0]",
        "impact_position[1]",
        "impact_position[2]",
    };
inline constexpr std::array<std::uint16_t, 15u>
    kVerifiedUsercmdFieldOffsets = {
        0u,
        2u,
        8u,
        4u,
        30u,
        16u,
        28u,
        20u,
        24u,
        32u,
        12u,
        36u,
        40u,
        44u,
        48u,
    };
inline constexpr std::array<std::uint32_t, 15u>
    kVerifiedUsercmdFieldTypes = {
        kGoldSrcDeltaTypeShort,
        kGoldSrcDeltaTypeByte,
        kGoldSrcDeltaTypeAngle,
        kGoldSrcDeltaTypeAngle,
        kGoldSrcDeltaTypeShort,
        kGoldSrcDeltaTypeFloat | kGoldSrcDeltaTypeSigned,
        kGoldSrcDeltaTypeByte,
        kGoldSrcDeltaTypeFloat | kGoldSrcDeltaTypeSigned,
        kGoldSrcDeltaTypeFloat | kGoldSrcDeltaTypeSigned,
        kGoldSrcDeltaTypeByte,
        kGoldSrcDeltaTypeAngle,
        kGoldSrcDeltaTypeInteger,
        kGoldSrcDeltaTypeFloat | kGoldSrcDeltaTypeSigned,
        kGoldSrcDeltaTypeFloat | kGoldSrcDeltaTypeSigned,
        kGoldSrcDeltaTypeFloat | kGoldSrcDeltaTypeSigned,
    };
inline constexpr std::array<std::uint8_t, 15u>
    kVerifiedUsercmdSignificantBits = {
        9u,
        8u,
        16u,
        16u,
        16u,
        12u,
        8u,
        12u,
        12u,
        8u,
        16u,
        6u,
        16u,
        16u,
        16u,
    };
inline constexpr std::array<double, 15u>
    kVerifiedUsercmdPremultipliers = {
        1.0,
        1.0,
        1.0,
        1.0,
        1.0,
        1.0,
        1.0,
        1.0,
        1.0,
        1.0,
        1.0,
        1.0,
        8.0,
        8.0,
        8.0,
    };

GoldSrcDeltaFieldLayout LayoutField(
    std::string name,
    std::uint16_t offset,
    std::uint8_t size)
{
    return {std::move(name), offset, size};
}

GoldSrcDeltaTableLayout LayoutTable(
    std::string name,
    std::initializer_list<GoldSrcDeltaFieldLayout> fields)
{
    GoldSrcDeltaTableLayout table{};
    table.name = std::move(name);
    table.structure_size = 64u;
    table.fields.assign(fields);
    return table;
}

GoldSrcDeltaLayoutRegistry CanonicalLayouts()
{
    GoldSrcDeltaLayoutRegistry layouts{};
    layouts.tables = {
        LayoutTable(
            "event_t",
            {
                LayoutField("entindex", 0u, 4u),
                LayoutField("origin[0]", 4u, 4u),
            }),
        LayoutTable(
            "weapon_data_t",
            {
                LayoutField("m_iId", 0u, 4u),
            }),
        LayoutTable(
            "usercmd_t",
            {
                LayoutField("lerp_msec", 0u, 1u),
                LayoutField("msec", 2u, 1u),
                LayoutField("viewangles[1]", 8u, 1u),
                LayoutField("viewangles[0]", 4u, 1u),
                LayoutField("buttons", 30u, 1u),
                LayoutField("forwardmove", 16u, 1u),
                LayoutField("lightlevel", 28u, 1u),
                LayoutField("sidemove", 20u, 1u),
                LayoutField("upmove", 24u, 1u),
                LayoutField("impulse", 32u, 1u),
                LayoutField("viewangles[2]", 12u, 1u),
                LayoutField("impact_index", 36u, 1u),
                LayoutField("impact_position[0]", 40u, 1u),
                LayoutField("impact_position[1]", 44u, 1u),
                LayoutField("impact_position[2]", 48u, 1u),
            }),
        LayoutTable(
            "custom_entity_state_t",
            {
                LayoutField("scale", 0u, 4u),
            }),
        LayoutTable(
            "entity_state_player_t",
            {
                LayoutField("origin[0]", 0u, 4u),
            }),
        LayoutTable(
            "entity_state_t",
            {
                LayoutField("animtime", 0u, 4u),
            }),
        LayoutTable(
            "clientdata_t",
            {
                LayoutField("health", 0u, 4u),
            }),
    };
    return layouts;
}

std::string CanonicalDefinitions()
{
    return R"(
        // Source order is intentionally not wire order.
        clientdata_t none {
            DEFINE_DELTA(health DT_SIGNED | DT_FLOAT 10 1.0)
        }
        entity_state_t gamedll Entity_Encode {
            DEFINE_DELTA_POST(
                animtime,
                DT_TIMEWINDOW_8,
                8,
                1.0,
                2.0
            ),
        }
        custom_entity_state_t gamedll Custom_Encode {
            DEFINE_DELTA(scale, DT_FLOAT, 10, 4.0),
        }
        event_t none {
            DEFINE_DELTA(entindex, DT_INTEGER, 11, 1.0),
            DEFINE_DELTA(origin[0], DT_FLOAT | DT_SIGNED, 16, 8.0)
        }
        usercmd_t none {
            DEFINE_DELTA(lerp_msec, DT_SHORT, 9, 1.0),
            DEFINE_DELTA(msec, DT_BYTE, 8, 1.0),
            DEFINE_DELTA(viewangles[1], DT_ANGLE, 16, 1.0),
            DEFINE_DELTA(viewangles[0], DT_ANGLE, 16, 1.0),
            DEFINE_DELTA(buttons, DT_SHORT, 16, 1.0),
            DEFINE_DELTA(forwardmove, DT_SIGNED | DT_FLOAT, 12, 1.0),
            DEFINE_DELTA(lightlevel, DT_BYTE, 8, 1.0),
            DEFINE_DELTA(sidemove, DT_SIGNED | DT_FLOAT, 12, 1.0),
            DEFINE_DELTA(upmove, DT_SIGNED | DT_FLOAT, 12, 1.0),
            DEFINE_DELTA(impulse, DT_BYTE, 8, 1.0),
            DEFINE_DELTA(viewangles[2], DT_ANGLE, 16, 1.0),
            DEFINE_DELTA(impact_index, DT_INTEGER, 6, 1.0),
            DEFINE_DELTA(
                impact_position[0],
                DT_SIGNED | DT_FLOAT,
                16,
                8.0
            ),
            DEFINE_DELTA(
                impact_position[1],
                DT_SIGNED | DT_FLOAT,
                16,
                8.0
            ),
            DEFINE_DELTA(
                impact_position[2],
                DT_SIGNED | DT_FLOAT,
                16,
                8.0
            ),
        }
        entity_state_player_t gamedll Player_Encode {
            DEFINE_DELTA(origin[0], DT_SIGNED | DT_FLOAT, 16, 8.0)
        }
        weapon_data_t none {
            DEFINE_DELTA(m_iId, DT_INTEGER, 5, 1.0)
        }
    )";
}

const GoldSrcDeltaTable* FindTable(
    const GoldSrcDeltaRegistry& registry,
    std::string_view name)
{
    const auto found = std::find_if(
        registry.tables.begin(),
        registry.tables.end(),
        [name](const GoldSrcDeltaTable& table)
        {
            return table.name == name;
        });
    return found == registry.tables.end() ? nullptr : &*found;
}

void AssertVerifiedCompleteUsercmd(const GoldSrcDeltaTable& table)
{
    assert(table.name == "usercmd_t");
    assert(
        table.conditional_encoder_kind
        == GoldSrcDeltaConditionalEncoderKind::kNone);
    assert(table.conditional_encoder_name.empty());
    assert(table.fields.size() == kVerifiedUsercmdFieldNames.size());
    for (std::size_t index = 0u;
        index < kVerifiedUsercmdFieldNames.size();
        ++index)
    {
        const GoldSrcDeltaField& field = table.fields[index];
        assert(field.name == kVerifiedUsercmdFieldNames[index]);
        assert(field.field_type == kVerifiedUsercmdFieldTypes[index]);
        assert(field.field_offset == kVerifiedUsercmdFieldOffsets[index]);
        assert(field.field_size == 1u);
        assert(
            field.significant_bits
            == kVerifiedUsercmdSignificantBits[index]);
        assert(
            field.premultiply
            == kVerifiedUsercmdPremultipliers[index]);
        assert(field.postmultiply == 1.0);
    }
}

GoldSrcDeltaField WireField(
    std::string name,
    std::uint16_t offset = 0u,
    std::uint8_t size = 4u)
{
    GoldSrcDeltaField field{};
    field.field_type = kGoldSrcDeltaTypeFloat | kGoldSrcDeltaTypeSigned;
    field.name = std::move(name);
    field.field_offset = offset;
    field.field_size = size;
    field.significant_bits = 16u;
    field.premultiply = 1.25;
    field.postmultiply = 2.0;
    return field;
}

GoldSrcDeltaRegistry SmallCanonicalRegistry()
{
    GoldSrcDeltaRegistry registry{};
    for (std::size_t index = 0u;
        index < kGoldSrcCanonicalDeltaTableOrder.size();
        ++index)
    {
        GoldSrcDeltaTable table{};
        table.name = std::string(kGoldSrcCanonicalDeltaTableOrder[index]);
        table.fields.push_back(WireField(
            index == 2u ? "lerp_msec" : "field",
            static_cast<std::uint16_t>(index * 4u)));
        registry.tables.push_back(std::move(table));
    }
    return registry;
}

void AssertUnusedDescriptorBytesAreZero(
    const GoldSrcDeltaDescriptorPayload& payload)
{
    assert(payload.size <= payload.bytes.size());
    assert(std::all_of(
        payload.bytes.begin() + static_cast<std::ptrdiff_t>(payload.size),
        payload.bytes.end(),
        [](std::uint8_t value)
        {
            return value == 0u;
        }));
    if (payload.bit_count % 8u != 0u)
    {
        const std::uint8_t used_mask = static_cast<std::uint8_t>(
            (1u << (payload.bit_count % 8u)) - 1u);
        assert((payload.bytes[payload.size - 1u] & ~used_mask) == 0u);
    }
}

void AssertUnusedBundleBytesAreZero(
    const GoldSrcDeltaBundlePayload& payload)
{
    assert(payload.size <= payload.bytes.size());
    assert(std::all_of(
        payload.bytes.begin() + static_cast<std::ptrdiff_t>(payload.size),
        payload.bytes.end(),
        [](std::uint8_t value)
        {
            return value == 0u;
        }));
}

void TestProtocolConstantsAndStableReasons()
{
    static_assert(kGoldSrcDeltaDescriptionOpcode == 14u);
    static_assert(kGoldSrcMaximumDeltaDefinitionBytes == 65536u);
    static_assert(kGoldSrcMaximumDeltaTables == 32u);
    static_assert(kGoldSrcMaximumDeltaFieldsPerTable == 56u);
    static_assert(kGoldSrcMaximumDeltaNameBytes == 31u);
    static_assert(kGoldSrcCanonicalDeltaTableCount == 7u);
    static_assert(kGoldSrcDeltaTypeByte == 1u);
    static_assert(kGoldSrcDeltaTypeShort == 2u);
    static_assert(kGoldSrcDeltaTypeFloat == 4u);
    static_assert(kGoldSrcDeltaTypeInteger == 8u);
    static_assert(kGoldSrcDeltaTypeAngle == 0x10u);
    static_assert(kGoldSrcDeltaTypeTimeWindow8 == 0x20u);
    static_assert(kGoldSrcDeltaTypeTimeWindowBig == 0x40u);
    static_assert(kGoldSrcDeltaTypeString == 0x80u);
    static_assert(kGoldSrcDeltaTypeSigned == 0x80000000u);

    assert(ReasonFor(GoldSrcDeltaParseStatus::kOk) == "ok");
    assert(
        ReasonFor(GoldSrcDeltaParseStatus::kDuplicateTable)
        == "duplicate_table");
    assert(
        ReasonFor(GoldSrcDeltaEncodeStatus::kOutputCapacityExceeded)
        == "output_capacity_exceeded");
    assert(
        ReasonFor(GoldSrcDeltaDecodeStatus::kNonZeroPadding)
        == "non_zero_padding");
}

void TestBoundedBitstream()
{
    std::array<std::uint8_t, 2u> bytes = {0xffu, 0xffu};
    GoldSrcBitWriter writer(bytes.data(), bytes.size());
    assert(writer.valid());
    assert(bytes[0] == 0u && bytes[1] == 0u);
    assert(writer.WriteBits(0b101u, 3u));
    assert(writer.WriteBits(0b11u, 2u));
    assert(writer.PadToByte());
    assert(writer.bit_position() == 8u);
    assert(writer.bytes_written() == 1u);
    assert(bytes[0] == 0x1du);
    assert(bytes[1] == 0u);
    assert(!writer.WriteBits(0xffffffffu, 32u));

    GoldSrcBitReader reader(bytes.data(), 1u);
    std::uint32_t value = 0u;
    assert(reader.ReadBits(3u, &value) && value == 0b101u);
    assert(reader.ReadBits(2u, &value) && value == 0b11u);
    assert(reader.AlignToByte(true));
    assert(reader.bits_remaining() == 0u);
    assert(!reader.ReadBits(1u, &value));

    const std::array<std::uint8_t, 1u> nonzero_padding = {0xe5u};
    GoldSrcBitReader padding_reader(
        nonzero_padding.data(),
        nonzero_padding.size());
    assert(padding_reader.ReadBits(3u, &value));
    assert(!padding_reader.AlignToByte(true));
}

void TestParserAcceptsVerifiedGrammar()
{
    const GoldSrcDeltaParseResult parsed =
        ParseGoldSrcDeltaDefinitions(
            CanonicalDefinitions(),
            CanonicalLayouts());
    assert(parsed.ok());
    assert(parsed.registry.tables.size() == 7u);
    assert(parsed.registry.tables.front().name == "clientdata_t");

    const GoldSrcDeltaTable* usercmd =
        FindTable(parsed.registry, "usercmd_t");
    assert(usercmd != nullptr);
    AssertVerifiedCompleteUsercmd(*usercmd);

    const GoldSrcDeltaTable* event =
        FindTable(parsed.registry, "event_t");
    assert(event != nullptr);
    assert(event->fields[1].field_type
        == (kGoldSrcDeltaTypeFloat | kGoldSrcDeltaTypeSigned));

    const GoldSrcDeltaTable* entity =
        FindTable(parsed.registry, "entity_state_t");
    assert(entity != nullptr);
    assert(
        entity->conditional_encoder_kind
        == GoldSrcDeltaConditionalEncoderKind::kGameDll);
    assert(entity->conditional_encoder_name == "Entity_Encode");
    assert(entity->fields[0].postmultiply == 2.0);
}

void TestParserSupportsEveryVerifiedType()
{
    GoldSrcDeltaLayoutRegistry layouts{};
    GoldSrcDeltaTableLayout table{};
    table.name = "types_t";
    table.structure_size = 9u;
    for (std::uint16_t index = 0u; index < 8u; ++index)
    {
        table.fields.push_back(LayoutField(
            "f" + std::to_string(index),
            index,
            1u));
    }
    table.fields.push_back(LayoutField("rendercolor.r", 8u, 1u));
    layouts.tables.push_back(std::move(table));

    const std::string definitions = R"(
        types_t none {
            DEFINE_DELTA(f0, DT_BYTE, 8, 1)
            DEFINE_DELTA(f1, DT_SHORT, 8, 1)
            DEFINE_DELTA(f2, DT_FLOAT, 8, 1)
            DEFINE_DELTA(f3, DT_INTEGER, 8, 1)
            DEFINE_DELTA(f4, DT_ANGLE, 8, 1)
            DEFINE_DELTA(f5, DT_TIMEWINDOW_8, 8, 1)
            DEFINE_DELTA(f6, DT_TIMEWINDOW_BIG, 8, 1)
            DEFINE_DELTA(f7, DT_STRING, 8, 1)
            DEFINE_DELTA(rendercolor.r, DT_BYTE, 8, 1)
        }
    )";
    const GoldSrcDeltaParseResult parsed =
        ParseGoldSrcDeltaDefinitions(definitions, layouts);
    assert(parsed.ok());
    const std::array<std::uint32_t, 9u> expected = {
        kGoldSrcDeltaTypeByte,
        kGoldSrcDeltaTypeShort,
        kGoldSrcDeltaTypeFloat,
        kGoldSrcDeltaTypeInteger,
        kGoldSrcDeltaTypeAngle,
        kGoldSrcDeltaTypeTimeWindow8,
        kGoldSrcDeltaTypeTimeWindowBig,
        kGoldSrcDeltaTypeString,
        kGoldSrcDeltaTypeByte,
    };
    assert(parsed.registry.tables[0].fields.size() == expected.size());
    for (std::size_t index = 0u; index < expected.size(); ++index)
    {
        assert(
            parsed.registry.tables[0].fields[index].field_type
            == expected[index]);
    }
}

void TestParserBoundsGameDllConditionalEncoders()
{
    const GoldSrcDeltaLayoutRegistry layouts = CanonicalLayouts();
    auto status_for = [&layouts](std::string_view text)
    {
        return ParseGoldSrcDeltaDefinitions(text, layouts).status;
    };

    assert(
        status_for(
            "entity_state_t gamedll Entity_Encode {"
            "DEFINE_DELTA(animtime, DT_FLOAT, 8, 1)"
            "}")
        == GoldSrcDeltaParseStatus::kOk);
    assert(
        status_for(
            "entity_state_player_t gamedll Player_Encode {"
            "DEFINE_DELTA(origin[0], DT_FLOAT, 8, 1)"
            "}")
        == GoldSrcDeltaParseStatus::kOk);
    assert(
        status_for(
            "custom_entity_state_t gamedll Custom_Encode {"
            "DEFINE_DELTA(scale, DT_FLOAT, 8, 1)"
            "}")
        == GoldSrcDeltaParseStatus::kOk);

    assert(
        status_for(
            "entity_state_t gamedll Player_Encode {"
            "DEFINE_DELTA(animtime, DT_FLOAT, 8, 1)"
            "}")
        == GoldSrcDeltaParseStatus::kUnsupportedConditionalEncoder);
    assert(
        status_for(
            "event_t gamedll Entity_Encode {"
            "DEFINE_DELTA(entindex, DT_INTEGER, 11, 1)"
            "}")
        == GoldSrcDeltaParseStatus::kUnsupportedConditionalEncoder);
    assert(
        status_for(
            "entity_state_t gamedll Unknown_Encode {"
            "DEFINE_DELTA(animtime, DT_FLOAT, 8, 1)"
            "}")
        == GoldSrcDeltaParseStatus::kUnsupportedConditionalEncoder);
}

void TestParserRejectsMalformedDefinitions()
{
    const GoldSrcDeltaLayoutRegistry layouts = CanonicalLayouts();
    auto status_for = [&layouts](std::string_view text)
    {
        return ParseGoldSrcDeltaDefinitions(text, layouts).status;
    };

    assert(
        status_for("{ }")
        == GoldSrcDeltaParseStatus::kMissingTableName);
    assert(
        status_for(
            "event_t none { DEFINE_DELTA(entindex, DT_BYTE, 8, 1) }"
            "event_t none { DEFINE_DELTA(entindex, DT_BYTE, 8, 1) }")
        == GoldSrcDeltaParseStatus::kDuplicateTable);
    assert(
        status_for(
            "event_t none {"
            "DEFINE_DELTA(entindex, DT_BYTE, 8, 1)"
            "DEFINE_DELTA(entindex, DT_BYTE, 8, 1)"
            "}")
        == GoldSrcDeltaParseStatus::kDuplicateField);
    assert(
        status_for(
            "event_t none {"
            "DEFINE_DELTA(entindex, DT_VECTOR, 8, 1)"
            "}")
        == GoldSrcDeltaParseStatus::kUnknownFieldType);
    assert(
        status_for(
            "event_t none {"
            "DEFINE_DELTA(entindex, DT_BYTE, 0, 1)"
            "}")
        == GoldSrcDeltaParseStatus::kInvalidBitCount);
    assert(
        status_for(
            "event_t none {"
            "DEFINE_DELTA(entindex, DT_BYTE, 42949672960, 1)"
            "}")
        == GoldSrcDeltaParseStatus::kNumericOverflow);
    assert(
        status_for(
            "event_t none {"
            "DEFINE_DELTA(entindex, DT_BYTE, 8, nan)"
            "}")
        == GoldSrcDeltaParseStatus::kInvalidMultiplier);
    assert(
        status_for(
            "event_t clientdll Client_Encode {"
            "DEFINE_DELTA(entindex, DT_BYTE, 8, 1)"
            "}")
        == GoldSrcDeltaParseStatus::kUnsupportedConditionalEncoder);
    assert(
        status_for(
            "event_t none {"
            "DEFINE_DELTA(entindex, DT_BYTE, 8, 1")
        == GoldSrcDeltaParseStatus::kIncompleteDeclaration);

    const std::string long_name(32u, 'a');
    assert(
        status_for(long_name + " none { }")
        == GoldSrcDeltaParseStatus::kNameTooLong);

    std::string oversized(kGoldSrcMaximumDeltaDefinitionBytes + 1u, ' ');
    assert(
        ParseGoldSrcDeltaDefinitions(oversized, layouts).status
        == GoldSrcDeltaParseStatus::kInputTooLarge);
}

void TestParserEnforcesLayoutAndCountBounds()
{
    {
        GoldSrcDeltaLayoutRegistry invalid = CanonicalLayouts();
        invalid.tables[0].fields[0].size = 0u;
        assert(
            ParseGoldSrcDeltaDefinitions(
                "event_t none {"
                "DEFINE_DELTA(entindex, DT_BYTE, 8, 1)"
                "}",
                invalid)
                .status
            == GoldSrcDeltaParseStatus::kInvalidLayout);
    }
    {
        GoldSrcDeltaLayoutRegistry invalid = CanonicalLayouts();
        invalid.tables[0].fields[0].offset = 63u;
        invalid.tables[0].fields[0].size = 2u;
        assert(
            ParseGoldSrcDeltaDefinitions(
                "event_t none {"
                "DEFINE_DELTA(entindex, DT_BYTE, 8, 1)"
                "}",
                invalid)
                .status
            == GoldSrcDeltaParseStatus::kInvalidLayout);
    }
    {
        GoldSrcDeltaLayoutRegistry layouts{};
        std::string definitions;
        for (std::size_t index = 0u;
            index < kGoldSrcMaximumDeltaTables;
            ++index)
        {
            GoldSrcDeltaTableLayout table{};
            table.name = "t" + std::to_string(index);
            table.structure_size = 1u;
            table.fields.push_back(LayoutField("f", 0u, 1u));
            definitions += table.name
                + " none { DEFINE_DELTA(f, DT_BYTE, 8, 1) }\n";
            layouts.tables.push_back(std::move(table));
        }
        definitions +=
            "t0 none { DEFINE_DELTA(f, DT_BYTE, 8, 1) }";
        assert(
            ParseGoldSrcDeltaDefinitions(definitions, layouts).status
            == GoldSrcDeltaParseStatus::kTableCountExceeded);
    }
    {
        GoldSrcDeltaLayoutRegistry layouts{};
        GoldSrcDeltaTableLayout table{};
        table.name = "wide_t";
        table.structure_size = 57u;
        std::string definitions = "wide_t none {";
        for (std::size_t index = 0u;
            index <= kGoldSrcMaximumDeltaFieldsPerTable;
            ++index)
        {
            const std::string name = "f" + std::to_string(index);
            table.fields.push_back(LayoutField(
                name,
                static_cast<std::uint16_t>(index),
                1u));
            definitions +=
                "DEFINE_DELTA(" + name + ", DT_BYTE, 8, 1)";
        }
        definitions += "}";
        layouts.tables.push_back(std::move(table));
        assert(
            ParseGoldSrcDeltaDefinitions(definitions, layouts).status
            == GoldSrcDeltaParseStatus::kFieldCountExceeded);
    }
    {
        GoldSrcDeltaLayoutRegistry layouts{};
        GoldSrcDeltaTableLayout table{};
        table.name = "layout_union_t";
        table.structure_size =
            static_cast<std::uint32_t>(
                kGoldSrcMaximumDeltaLayoutFieldsPerTable);
        for (std::size_t index = 0u;
            index < kGoldSrcMaximumDeltaLayoutFieldsPerTable;
            ++index)
        {
            table.fields.push_back(LayoutField(
                "f" + std::to_string(index),
                static_cast<std::uint16_t>(index),
                1u));
        }
        layouts.tables.push_back(std::move(table));
        assert(
            ParseGoldSrcDeltaDefinitions(
                "layout_union_t none {"
                "DEFINE_DELTA(f255, DT_BYTE, 8, 1)"
                "}",
                layouts)
                .ok());
    }
}

void TestMetaDeltaZeroAndGoldenVector()
{
    {
        const GoldSrcDeltaDescriptorEncodeResult encoded =
            EncodeGoldSrcDeltaFieldDescriptor({});
        assert(encoded.ok());
        assert(encoded.payload.bit_count == 3u);
        assert(encoded.payload.size == 1u);
        assert(encoded.payload.bytes[0] == 0u);
        AssertUnusedDescriptorBytesAreZero(encoded.payload);
    }
    {
        GoldSrcDeltaField field{};
        field.field_type = kGoldSrcDeltaTypeByte;
        const GoldSrcDeltaDescriptorEncodeResult encoded =
            EncodeGoldSrcDeltaFieldDescriptor(field);
        const std::array<std::uint8_t, 6u> golden = {
            0x09u,
            0x08u,
            0x00u,
            0x00u,
            0x00u,
            0x00u,
        };
        assert(encoded.ok());
        assert(encoded.payload.bit_count == 43u);
        assert(encoded.payload.size == golden.size());
        assert(std::equal(
            golden.begin(),
            golden.end(),
            encoded.payload.bytes.begin()));
        AssertUnusedDescriptorBytesAreZero(encoded.payload);
    }
}

void TestMetaDeltaAllMembersAndCapacity()
{
    GoldSrcDeltaField field{};
    field.field_type =
        kGoldSrcDeltaTypeFloat | kGoldSrcDeltaTypeSigned;
    field.name = "origin[0]";
    field.field_offset = 0x1234u;
    field.field_size = 4u;
    field.significant_bits = 21u;
    field.premultiply = 1.23456;
    field.postmultiply = 2.0;

    const GoldSrcDeltaDescriptorEncodeResult encoded =
        EncodeGoldSrcDeltaFieldDescriptor(field);
    assert(encoded.ok());
    assert(encoded.payload.bit_count == 219u);
    assert(encoded.payload.size == 28u);
    AssertUnusedDescriptorBytesAreZero(encoded.payload);

    GoldSrcBitReader reader(
        encoded.payload.bytes.data(),
        encoded.payload.size);
    std::uint32_t value = 0u;
    assert(reader.ReadBits(3u, &value) && value == 1u);
    assert(reader.ReadBits(8u, &value) && value == 0x7fu);
    assert(
        reader.ReadBits(32u, &value)
        && value
            == (kGoldSrcDeltaTypeFloat | kGoldSrcDeltaTypeSigned));
    std::string name;
    assert(reader.ReadString(kGoldSrcMaximumDeltaNameBytes, &name));
    assert(name == "origin[0]");
    assert(reader.ReadBits(16u, &value) && value == 0x1234u);
    assert(reader.ReadBits(8u, &value) && value == 4u);
    assert(reader.ReadBits(8u, &value) && value == 21u);
    assert(reader.ReadBits(32u, &value) && value == 4938u);
    assert(reader.ReadBits(32u, &value) && value == 8000u);
    assert(reader.bit_position() == encoded.payload.bit_count);

    const GoldSrcDeltaDescriptorEncodeResult exact_capacity =
        EncodeGoldSrcDeltaFieldDescriptor(field, encoded.payload.size);
    assert(exact_capacity.ok());
    const GoldSrcDeltaDescriptorEncodeResult one_byte_short =
        EncodeGoldSrcDeltaFieldDescriptor(field, encoded.payload.size - 1u);
    assert(
        one_byte_short.status
        == GoldSrcDeltaEncodeStatus::kOutputCapacityExceeded);
    AssertUnusedDescriptorBytesAreZero(one_byte_short.payload);
}

void TestMetaDeltaRejectsInvalidSparseMembers()
{
    {
        GoldSrcDeltaField field{};
        field.field_type = 0x100u;
        assert(
            EncodeGoldSrcDeltaFieldDescriptor(field).status
            == GoldSrcDeltaEncodeStatus::kInvalidFieldType);
    }
    {
        GoldSrcDeltaField field{};
        field.name = std::string(32u, 'a');
        assert(
            EncodeGoldSrcDeltaFieldDescriptor(field).status
            == GoldSrcDeltaEncodeStatus::kInvalidFieldName);
    }
    {
        GoldSrcDeltaField field{};
        field.field_offset = 0xffffu;
        field.field_size = 2u;
        assert(
            EncodeGoldSrcDeltaFieldDescriptor(field).status
            == GoldSrcDeltaEncodeStatus::kInvalidFieldLayout);
    }
    {
        GoldSrcDeltaField field{};
        field.significant_bits = 33u;
        assert(
            EncodeGoldSrcDeltaFieldDescriptor(field).status
            == GoldSrcDeltaEncodeStatus::kInvalidBitCount);
    }
    {
        GoldSrcDeltaField field{};
        field.premultiply =
            std::numeric_limits<double>::infinity();
        assert(
            EncodeGoldSrcDeltaFieldDescriptor(field).status
            == GoldSrcDeltaEncodeStatus::kInvalidMultiplier);
    }
}

void TestCanonicalBundleAndSemanticRoundTrip()
{
    const GoldSrcDeltaParseResult parsed =
        ParseGoldSrcDeltaDefinitions(
            CanonicalDefinitions(),
            CanonicalLayouts());
    assert(parsed.ok());
    const GoldSrcDeltaBundleEncodeResult encoded =
        EncodeGoldSrcCanonicalDeltaBundle(parsed.registry);
    assert(encoded.ok());
    assert(encoded.payload.table_count == 7u);
    assert(encoded.payload.field_count == 22u);
    assert(encoded.payload.size <= kGoldSrcNetchanMaximumReliableBytes);
    AssertUnusedBundleBytesAreZero(encoded.payload);

    GoldSrcBitReader header(
        encoded.payload.bytes.data(),
        encoded.payload.size);
    std::uint32_t value = 0u;
    assert(
        header.ReadBits(8u, &value)
        && value == kGoldSrcDeltaDescriptionOpcode);
    std::string table_name;
    assert(header.ReadString(kGoldSrcMaximumDeltaNameBytes, &table_name));
    assert(table_name == "event_t");
    assert(header.ReadBits(16u, &value) && value == 2u);

    const GoldSrcDeltaBundleDecodeResult decoded =
        DecodeGoldSrcCanonicalDeltaBundle(
            encoded.payload.bytes.data(),
            encoded.payload.size);
    assert(decoded.ok());
    assert(decoded.bytes_consumed == encoded.payload.size);
    assert(decoded.registry.tables.size() == 7u);
    for (std::size_t index = 0u;
        index < decoded.registry.tables.size();
        ++index)
    {
        assert(
            decoded.registry.tables[index].name
            == kGoldSrcCanonicalDeltaTableOrder[index]);
    }
    const GoldSrcDeltaTable* usercmd =
        FindTable(decoded.registry, "usercmd_t");
    assert(usercmd != nullptr);
    AssertVerifiedCompleteUsercmd(*usercmd);
}

void TestBundleDeterminism()
{
    const GoldSrcDeltaRegistry registry = SmallCanonicalRegistry();
    const GoldSrcDeltaBundleEncodeResult first =
        EncodeGoldSrcCanonicalDeltaBundle(registry);
    const GoldSrcDeltaBundleEncodeResult second =
        EncodeGoldSrcCanonicalDeltaBundle(registry);
    assert(first.ok() && second.ok());
    assert(first.payload.size == second.payload.size);
    assert(std::equal(
        first.payload.bytes.begin(),
        first.payload.bytes.begin()
            + static_cast<std::ptrdiff_t>(first.payload.size),
        second.payload.bytes.begin()));
}

void TestBundleMissingRequiredTable()
{
    GoldSrcDeltaRegistry registry = SmallCanonicalRegistry();
    registry.tables.erase(registry.tables.begin() + 2);
    assert(
        EncodeGoldSrcCanonicalDeltaBundle(registry).status
        == GoldSrcDeltaEncodeStatus::kMissingRequiredTable);
}

void TestBundleDuplicateRequiredTable()
{
    GoldSrcDeltaRegistry registry = SmallCanonicalRegistry();
    registry.tables.push_back(registry.tables[0]);
    assert(
        EncodeGoldSrcCanonicalDeltaBundle(registry).status
        == GoldSrcDeltaEncodeStatus::kDuplicateRequiredTable);
}

void TestBundleUnexpectedTable()
{
    GoldSrcDeltaRegistry registry = SmallCanonicalRegistry();
    GoldSrcDeltaTable table{};
    table.name = "extra_t";
    table.fields.push_back(WireField("field"));
    registry.tables.push_back(std::move(table));
    assert(
        EncodeGoldSrcCanonicalDeltaBundle(registry).status
        == GoldSrcDeltaEncodeStatus::kUnexpectedTable);
}

void TestBundleEmptyTable()
{
    GoldSrcDeltaRegistry registry = SmallCanonicalRegistry();
    registry.tables[0].fields.clear();
    assert(
        EncodeGoldSrcCanonicalDeltaBundle(registry).status
        == GoldSrcDeltaEncodeStatus::kEmptyTable);
}

void TestBundleExactOutputCapacity()
{
    const GoldSrcDeltaRegistry registry = SmallCanonicalRegistry();
    const GoldSrcDeltaBundleEncodeResult full =
        EncodeGoldSrcCanonicalDeltaBundle(registry);
    assert(full.ok());
    assert(full.payload.size > 0u);
    const GoldSrcDeltaBundleEncodeResult exact =
        EncodeGoldSrcCanonicalDeltaBundle(registry, full.payload.size);
    assert(exact.ok());
}

void TestBundleOneByteShortOutputCapacity()
{
    const GoldSrcDeltaRegistry registry = SmallCanonicalRegistry();
    const GoldSrcDeltaBundleEncodeResult full =
        EncodeGoldSrcCanonicalDeltaBundle(registry);
    assert(full.ok());
    assert(full.payload.size > 0u);
    const GoldSrcDeltaBundleEncodeResult one_byte_short =
        EncodeGoldSrcCanonicalDeltaBundle(
            registry,
            full.payload.size - 1u);
    assert(
        one_byte_short.status
        == GoldSrcDeltaEncodeStatus::kOutputCapacityExceeded);
    AssertUnusedBundleBytesAreZero(one_byte_short.payload);
}

void TestLargeBundleIsFragmentationEligible()
{
    GoldSrcDeltaRegistry registry{};
    for (const std::string_view table_name :
        kGoldSrcCanonicalDeltaTableOrder)
    {
        GoldSrcDeltaTable table{};
        table.name = std::string(table_name);
        for (std::size_t index = 0u;
            index < kGoldSrcMaximumDeltaFieldsPerTable;
            ++index)
        {
            table.fields.push_back(WireField(
                "field" + std::to_string(index),
                static_cast<std::uint16_t>(index * 4u)));
        }
        registry.tables.push_back(std::move(table));
    }

    const GoldSrcDeltaBundleEncodeResult encoded =
        EncodeGoldSrcCanonicalDeltaBundle(registry);
    assert(encoded.ok());
    assert(encoded.payload.field_count == 7u * 56u);
    assert(encoded.payload.size > kGoldSrcNetchanMaximumReliableBytes);
    assert(encoded.payload.size <= kGoldSrcMaximumDeltaBundleBytes);
    assert(
        DecodeGoldSrcCanonicalDeltaBundle(
            encoded.payload.bytes.data(),
            encoded.payload.size)
            .ok());
}

void TestDecoderRejectsWrongOpcode()
{
    const GoldSrcDeltaBundleEncodeResult encoded =
        EncodeGoldSrcCanonicalDeltaBundle(SmallCanonicalRegistry());
    assert(encoded.ok());
    std::array<std::uint8_t, kGoldSrcMaximumDeltaBundleBytes> bytes =
        encoded.payload.bytes;
    bytes[0] = 0u;
    assert(
        DecodeGoldSrcCanonicalDeltaBundle(
            bytes.data(),
            encoded.payload.size)
            .status
        == GoldSrcDeltaDecodeStatus::kWrongOpcode);
}

void TestDecoderRejectsWrongTableOrder()
{
    const GoldSrcDeltaBundleEncodeResult encoded =
        EncodeGoldSrcCanonicalDeltaBundle(SmallCanonicalRegistry());
    assert(encoded.ok());
    std::array<std::uint8_t, kGoldSrcMaximumDeltaBundleBytes> bytes =
        encoded.payload.bytes;
    bytes[1] = static_cast<std::uint8_t>('x');
    assert(
        DecodeGoldSrcCanonicalDeltaBundle(
            bytes.data(),
            encoded.payload.size)
            .status
        == GoldSrcDeltaDecodeStatus::kUnexpectedTable);
}

void TestDecoderRejectsNonZeroPadding()
{
    const GoldSrcDeltaBundleEncodeResult encoded =
        EncodeGoldSrcCanonicalDeltaBundle(SmallCanonicalRegistry());
    assert(encoded.ok());
    std::array<std::uint8_t, kGoldSrcMaximumDeltaBundleBytes> bytes =
        encoded.payload.bytes;
    bytes[encoded.payload.size - 1u] |= 0x80u;
    assert(
        DecodeGoldSrcCanonicalDeltaBundle(
            bytes.data(),
            encoded.payload.size)
            .status
        == GoldSrcDeltaDecodeStatus::kNonZeroPadding);
}

void TestDecoderRejectsInvalidPayloadBounds()
{
    const GoldSrcDeltaBundleEncodeResult encoded =
        EncodeGoldSrcCanonicalDeltaBundle(SmallCanonicalRegistry());
    assert(encoded.ok());
    assert(
        DecodeGoldSrcCanonicalDeltaBundle(
            encoded.payload.bytes.data(),
            encoded.payload.size - 1u)
            .status
        == GoldSrcDeltaDecodeStatus::kTruncatedPayload);
    assert(
        DecodeGoldSrcCanonicalDeltaBundle(
            encoded.payload.bytes.data(),
            encoded.payload.size + 1u)
            .status
        == GoldSrcDeltaDecodeStatus::kTrailingData);
    assert(
        DecodeGoldSrcCanonicalDeltaBundle(nullptr, 1u).status
        == GoldSrcDeltaDecodeStatus::kNullInput);
    assert(
        DecodeGoldSrcCanonicalDeltaBundle(nullptr, 0u).status
        == GoldSrcDeltaDecodeStatus::kEmptyPayload);
}
} // namespace

int main()
{
    TestProtocolConstantsAndStableReasons();
    TestBoundedBitstream();
    TestParserAcceptsVerifiedGrammar();
    TestParserSupportsEveryVerifiedType();
    TestParserBoundsGameDllConditionalEncoders();
    TestParserRejectsMalformedDefinitions();
    TestParserEnforcesLayoutAndCountBounds();
    TestMetaDeltaZeroAndGoldenVector();
    TestMetaDeltaAllMembersAndCapacity();
    TestMetaDeltaRejectsInvalidSparseMembers();
    TestCanonicalBundleAndSemanticRoundTrip();
    TestBundleDeterminism();
    TestBundleMissingRequiredTable();
    TestBundleDuplicateRequiredTable();
    TestBundleUnexpectedTable();
    TestBundleEmptyTable();
    TestBundleExactOutputCapacity();
    TestBundleOneByteShortOutputCapacity();
    TestLargeBundleIsFragmentationEligible();
    TestDecoderRejectsWrongOpcode();
    TestDecoderRejectsWrongTableOrder();
    TestDecoderRejectsNonZeroPadding();
    TestDecoderRejectsInvalidPayloadBounds();
    return 0;
}
