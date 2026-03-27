#include "world_bootstrap.h"

#include <array>
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

static_assert(sizeof(BspHeader) == 124, "GoldSrc BSP header layout mismatch.");

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
