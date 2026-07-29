#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "entity_text_block.h"
#include "filesystem/file_system.h"
#include "server_bootstrap.h"

namespace hl::game_api::detail
{
constexpr std::size_t kBspHeaderLumpCount = 15;
constexpr std::int32_t kGoldSrcBspVersion = 30;

struct BspLumpMetadata
{
    std::size_t index = 0;
    const char* name = "unknown";
    std::int32_t file_offset = 0;
    std::int32_t file_length = 0;
    bool within_file = false;
};

struct BspInlineModelBounds
{
    int model_index = 0;
    Vector mins = Vector(0.0f, 0.0f, 0.0f);
    Vector maxs = Vector(0.0f, 0.0f, 0.0f);
    Vector origin = Vector(0.0f, 0.0f, 0.0f);
    std::array<std::int32_t, 4> headnodes{};
    bool valid = false;
};

struct BspCollisionPlane
{
    Vector normal = Vector(0.0f, 0.0f, 0.0f);
    float distance = 0.0f;
    std::int32_t type = 0;
};

struct BspCollisionClipnode
{
    std::int32_t plane_index = 0;
    std::array<std::int32_t, 2> children{};
};

struct WorldModelContext
{
    std::string map_name;
    std::string model_path;
    std::filesystem::path file_path;
    bool bsp_loaded = false;
    bool prepared = false;
    std::int32_t bsp_version = 0;
    std::uintmax_t bsp_file_size = 0;
    std::array<BspLumpMetadata, kBspHeaderLumpCount> lumps{};
    std::vector<BspInlineModelBounds> inline_models;
    std::vector<BspCollisionPlane> collision_planes;
    std::vector<BspCollisionClipnode> collision_clipnodes;
    std::vector<BspCollisionClipnode> point_hull_nodes;
    bool collision_loaded = false;
    EntityTextBlockPreview entities;
    int world_model_index = 0;
    int world_edict_index = -1;
    std::string failure_reason;
};

bool LoadWorldModelContext(
    const filesystem::FileSystem& file_system,
    const ServerState& server_state,
    WorldModelContext& world_context);
} // namespace hl::game_api::detail
