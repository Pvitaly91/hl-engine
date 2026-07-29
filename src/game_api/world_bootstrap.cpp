#include "world_bootstrap.h"

#include <array>
#include <cmath>
#include <cstring>

#include "common/logger.h"
#include "common/text_encoding.h"

namespace
{
struct BspLumpHeader
{
    std::int32_t file_offset = 0;
    std::int32_t file_length = 0;
};

struct BspHeader
{
    std::int32_t version = 0;
    std::array<BspLumpHeader, hl::game_api::detail::kBspHeaderLumpCount> lumps{};
};

struct BspModelEntry
{
    Vector mins = Vector(0.0f, 0.0f, 0.0f);
    Vector maxs = Vector(0.0f, 0.0f, 0.0f);
    Vector origin = Vector(0.0f, 0.0f, 0.0f);
    std::array<std::int32_t, 4> headnodes{};
    std::int32_t visleafs = 0;
    std::int32_t firstface = 0;
    std::int32_t numfaces = 0;
};

struct BspPlaneEntry
{
    std::array<float, 3> normal{};
    float distance = 0.0f;
    std::int32_t type = 0;
};

struct BspClipnodeEntry
{
    std::int32_t plane_index = 0;
    std::array<std::int16_t, 2> children{};
};

struct BspNodeEntry
{
    std::int32_t plane_index = 0;
    std::array<std::int16_t, 2> children{};
    std::array<std::int16_t, 3> mins{};
    std::array<std::int16_t, 3> maxs{};
    std::uint16_t first_face = 0;
    std::uint16_t face_count = 0;
};

struct BspLeafEntry
{
    std::int32_t contents = 0;
    std::int32_t visibility_offset = 0;
    std::array<std::int16_t, 3> mins{};
    std::array<std::int16_t, 3> maxs{};
    std::uint16_t first_mark_surface = 0;
    std::uint16_t mark_surface_count = 0;
    std::array<std::uint8_t, 4> ambient_levels{};
};

static_assert(sizeof(BspHeader) == 124, "GoldSrc BSP header layout mismatch.");
static_assert(sizeof(BspModelEntry) == 64, "GoldSrc BSP model entry layout mismatch.");
static_assert(sizeof(BspPlaneEntry) == 20, "GoldSrc BSP plane layout mismatch.");
static_assert(sizeof(BspClipnodeEntry) == 8, "GoldSrc BSP clipnode layout mismatch.");
static_assert(sizeof(BspNodeEntry) == 24, "GoldSrc BSP node layout mismatch.");
static_assert(sizeof(BspLeafEntry) == 28, "GoldSrc BSP leaf layout mismatch.");

constexpr std::array<const char*, hl::game_api::detail::kBspHeaderLumpCount> kBspLumpNames = {{
    "entities",
    "planes",
    "textures",
    "vertexes",
    "visibility",
    "nodes",
    "texinfo",
    "faces",
    "lighting",
    "clipnodes",
    "leafs",
    "marksurfaces",
    "edges",
    "surfedges",
    "models",
}};

constexpr std::array<std::size_t, 7> kKeyLumpIndices = {{0, 1, 2, 3, 5, 7, 14}};

bool ValidateLump(
    const hl::game_api::detail::BspLumpMetadata& lump,
    std::uintmax_t file_size)
{
    if (lump.file_offset < 0 || lump.file_length < 0)
    {
        return false;
    }

    const std::uintmax_t offset = static_cast<std::uintmax_t>(lump.file_offset);
    const std::uintmax_t length = static_cast<std::uintmax_t>(lump.file_length);
    return offset <= file_size && length <= file_size - offset;
}

std::filesystem::path ResolveMapFilePath(
    const std::filesystem::path& game_directory,
    std::string_view map_name)
{
    return game_directory / hl::common::ToWide(hl::game_api::detail::BuildMapModelPath(map_name));
}
} // namespace

namespace hl::game_api::detail
{
bool LoadWorldModelContext(
    const filesystem::FileSystem& file_system,
    const ServerState& server_state,
    WorldModelContext& world_context)
{
    world_context = {};
    world_context.map_name = NormalizeMapName(server_state.map_name);
    world_context.model_path = BuildMapModelPath(world_context.map_name);
    world_context.file_path = ResolveMapFilePath(server_state.game_directory, world_context.map_name);

    common::Logger::Info(
        "World bootstrap: resolving map path '" + world_context.model_path + "'.");
    common::Logger::Info(
        "World bootstrap: resolved BSP file path: "
        + common::ToUtf8(world_context.file_path));

    if (!file_system.FileExists(world_context.file_path))
    {
        world_context.failure_reason =
            "BSP file not found: " + common::ToUtf8(world_context.file_path);
        common::Logger::Error(world_context.failure_reason);
        return false;
    }

    const std::optional<std::vector<unsigned char>> bytes =
        file_system.ReadBinaryFile(world_context.file_path);
    if (!bytes.has_value())
    {
        world_context.failure_reason =
            "Failed to open BSP file: " + common::ToUtf8(world_context.file_path);
        common::Logger::Error(world_context.failure_reason);
        return false;
    }

    world_context.bsp_file_size = bytes->size();
    if (bytes->size() < sizeof(BspHeader))
    {
        world_context.failure_reason =
            "BSP file is smaller than a GoldSrc header: " + common::ToUtf8(world_context.file_path);
        common::Logger::Error(world_context.failure_reason);
        return false;
    }

    BspHeader header{};
    std::memcpy(&header, bytes->data(), sizeof(header));

    world_context.bsp_version = header.version;
    if (header.version != kGoldSrcBspVersion)
    {
        world_context.failure_reason =
            "Unsupported BSP version " + std::to_string(header.version)
            + " for " + common::ToUtf8(world_context.file_path);
        common::Logger::Error(world_context.failure_reason);
        return false;
    }

    for (std::size_t index = 0; index < world_context.lumps.size(); ++index)
    {
        BspLumpMetadata lump;
        lump.index = index;
        lump.name = kBspLumpNames[index];
        lump.file_offset = header.lumps[index].file_offset;
        lump.file_length = header.lumps[index].file_length;
        lump.within_file = ValidateLump(lump, world_context.bsp_file_size);
        world_context.lumps[index] = lump;

        if (!lump.within_file)
        {
            world_context.failure_reason =
                "BSP lump '" + std::string(lump.name) + "' is out of file bounds.";
            common::Logger::Error(world_context.failure_reason);
            return false;
        }
    }

    const BspLumpMetadata& entities_lump = world_context.lumps[0];
    if (entities_lump.file_length > 0)
    {
        const char* entities_begin =
            reinterpret_cast<const char*>(bytes->data()) + entities_lump.file_offset;
        world_context.entities = BuildEntityTextBlockPreview(
            std::string_view(
                entities_begin,
                static_cast<std::size_t>(entities_lump.file_length)));
    }
    else
    {
        world_context.entities.present = false;
    }

    const BspLumpMetadata& models_lump = world_context.lumps[14];
    if (models_lump.file_length > 0)
    {
        const std::size_t model_count =
            static_cast<std::size_t>(models_lump.file_length) / sizeof(BspModelEntry);
        world_context.inline_models.resize(model_count);
        for (std::size_t index = 0; index < model_count; ++index)
        {
            BspModelEntry model{};
            std::memcpy(
                &model,
                bytes->data() + models_lump.file_offset + (index * sizeof(BspModelEntry)),
                sizeof(BspModelEntry));

            BspInlineModelBounds bounds;
            bounds.model_index = static_cast<int>(index);
            bounds.mins = model.mins;
            bounds.maxs = model.maxs;
            bounds.origin = model.origin;
            bounds.headnodes = model.headnodes;
            bounds.valid = true;
            world_context.inline_models[index] = bounds;
        }
    }

    const BspLumpMetadata& planes_lump = world_context.lumps[1];
    const BspLumpMetadata& nodes_lump = world_context.lumps[5];
    const BspLumpMetadata& clipnodes_lump = world_context.lumps[9];
    const BspLumpMetadata& leafs_lump = world_context.lumps[10];
    if (planes_lump.file_length <= 0
        || nodes_lump.file_length <= 0
        || clipnodes_lump.file_length <= 0
        || leafs_lump.file_length <= 0
        || planes_lump.file_length % sizeof(BspPlaneEntry) != 0
        || nodes_lump.file_length % sizeof(BspNodeEntry) != 0
        || clipnodes_lump.file_length % sizeof(BspClipnodeEntry) != 0
        || leafs_lump.file_length % sizeof(BspLeafEntry) != 0)
    {
        world_context.failure_reason =
            "BSP collision lumps are missing or structurally invalid.";
        common::Logger::Error(world_context.failure_reason);
        return false;
    }

    const std::size_t plane_count =
        static_cast<std::size_t>(planes_lump.file_length)
        / sizeof(BspPlaneEntry);
    world_context.collision_planes.resize(plane_count);
    for (std::size_t index = 0; index < plane_count; ++index)
    {
        BspPlaneEntry plane{};
        std::memcpy(
            &plane,
            bytes->data() + planes_lump.file_offset
                + index * sizeof(BspPlaneEntry),
            sizeof(plane));
        BspCollisionPlane output;
        output.normal =
            Vector(plane.normal[0], plane.normal[1], plane.normal[2]);
        output.distance = plane.distance;
        output.type = plane.type;
        if (!std::isfinite(output.normal.x)
            || !std::isfinite(output.normal.y)
            || !std::isfinite(output.normal.z)
            || !std::isfinite(output.distance)
            || output.type < 0 || output.type > 5)
        {
            world_context.failure_reason =
                "BSP collision plane is invalid.";
            common::Logger::Error(world_context.failure_reason);
            return false;
        }
        world_context.collision_planes[index] = output;
    }

    const std::size_t clipnode_count =
        static_cast<std::size_t>(clipnodes_lump.file_length)
        / sizeof(BspClipnodeEntry);
    world_context.collision_clipnodes.resize(clipnode_count);
    for (std::size_t index = 0; index < clipnode_count; ++index)
    {
        BspClipnodeEntry clipnode{};
        std::memcpy(
            &clipnode,
            bytes->data() + clipnodes_lump.file_offset
                + index * sizeof(BspClipnodeEntry),
            sizeof(clipnode));
        if (clipnode.plane_index < 0
            || static_cast<std::size_t>(clipnode.plane_index) >= plane_count)
        {
            world_context.failure_reason =
                "BSP collision clipnode references an invalid plane.";
            common::Logger::Error(world_context.failure_reason);
            return false;
        }
        BspCollisionClipnode output;
        output.plane_index = clipnode.plane_index;
        output.children = {
            static_cast<std::int32_t>(clipnode.children[0]),
            static_cast<std::int32_t>(clipnode.children[1]),
        };
        for (const std::int32_t child : output.children)
        {
            if (child >= 0
                && static_cast<std::size_t>(child) >= clipnode_count)
            {
                world_context.failure_reason =
                    "BSP collision clipnode child is out of bounds.";
                common::Logger::Error(world_context.failure_reason);
                return false;
            }
        }
        world_context.collision_clipnodes[index] = output;
    }

    const std::size_t leaf_count =
        static_cast<std::size_t>(leafs_lump.file_length)
        / sizeof(BspLeafEntry);
    std::vector<std::int32_t> leaf_contents(leaf_count);
    for (std::size_t index = 0; index < leaf_count; ++index)
    {
        BspLeafEntry leaf{};
        std::memcpy(
            &leaf,
            bytes->data() + leafs_lump.file_offset
                + index * sizeof(BspLeafEntry),
            sizeof(leaf));
        if (leaf.contents >= 0)
        {
            world_context.failure_reason =
                "BSP collision leaf has invalid contents.";
            common::Logger::Error(world_context.failure_reason);
            return false;
        }
        leaf_contents[index] = leaf.contents;
    }

    const std::size_t node_count =
        static_cast<std::size_t>(nodes_lump.file_length)
        / sizeof(BspNodeEntry);
    world_context.point_hull_nodes.resize(node_count);
    for (std::size_t index = 0; index < node_count; ++index)
    {
        BspNodeEntry node{};
        std::memcpy(
            &node,
            bytes->data() + nodes_lump.file_offset
                + index * sizeof(BspNodeEntry),
            sizeof(node));
        if (node.plane_index < 0
            || static_cast<std::size_t>(node.plane_index) >= plane_count)
        {
            world_context.failure_reason =
                "BSP point-hull node references an invalid plane.";
            common::Logger::Error(world_context.failure_reason);
            return false;
        }
        BspCollisionClipnode output;
        output.plane_index = node.plane_index;
        for (std::size_t side = 0; side < 2; ++side)
        {
            const std::int32_t child = node.children[side];
            if (child >= 0)
            {
                if (static_cast<std::size_t>(child) >= node_count)
                {
                    world_context.failure_reason =
                        "BSP point-hull node child is out of bounds.";
                    common::Logger::Error(world_context.failure_reason);
                    return false;
                }
                output.children[side] = child;
            }
            else
            {
                const std::size_t leaf_index =
                    static_cast<std::size_t>(-child - 1);
                if (leaf_index >= leaf_contents.size())
                {
                    world_context.failure_reason =
                        "BSP point-hull leaf reference is out of bounds.";
                    common::Logger::Error(world_context.failure_reason);
                    return false;
                }
                output.children[side] = leaf_contents[leaf_index];
            }
        }
        world_context.point_hull_nodes[index] = output;
    }

    if (world_context.inline_models.empty())
    {
        world_context.failure_reason =
            "BSP collision world model is missing.";
        common::Logger::Error(world_context.failure_reason);
        return false;
    }
    const BspInlineModelBounds& world_model =
        world_context.inline_models.front();
    if (world_model.headnodes[0] < 0
        || static_cast<std::size_t>(world_model.headnodes[0])
            >= world_context.point_hull_nodes.size())
    {
        world_context.failure_reason =
            "BSP point-hull headnode is invalid.";
        common::Logger::Error(world_context.failure_reason);
        return false;
    }
    for (std::size_t hull = 1; hull < world_model.headnodes.size(); ++hull)
    {
        if (world_model.headnodes[hull] < 0
            || static_cast<std::size_t>(world_model.headnodes[hull])
                >= world_context.collision_clipnodes.size())
        {
            world_context.failure_reason =
                "BSP player-hull headnode is invalid.";
            common::Logger::Error(world_context.failure_reason);
            return false;
        }
    }
    world_context.collision_loaded = true;
    world_context.bsp_loaded = true;

    common::Logger::Info("World bootstrap BSP summary:");
    common::Logger::Info("  - bsp version: " + std::to_string(world_context.bsp_version));
    common::Logger::Info("  - file size: " + std::to_string(world_context.bsp_file_size));
    common::Logger::Info(
        "  - lump entries: " + std::to_string(world_context.lumps.size()));
    common::Logger::Info("  - key lump sizes:");
    for (const std::size_t lump_index : kKeyLumpIndices)
    {
        const BspLumpMetadata& lump = world_context.lumps[lump_index];
        common::Logger::Info(
            "    * " + std::string(lump.name)
            + ": offset=" + std::to_string(lump.file_offset)
            + ", size=" + std::to_string(lump.file_length));
    }

    if (world_context.entities.present)
    {
        common::Logger::Info(
            "World bootstrap entities lump: size="
            + std::to_string(world_context.entities.byte_size));
        common::Logger::Info("World bootstrap entities preview:");
        for (const std::string& line : world_context.entities.preview_lines)
        {
            common::Logger::Info("  | " + line);
        }
        if (world_context.entities.truncated_preview)
        {
            common::Logger::Info("  | ...");
        }
    }
    else
    {
        common::Logger::Info("World bootstrap entities lump: missing or empty.");
    }

    return true;
}
} // namespace hl::game_api::detail
