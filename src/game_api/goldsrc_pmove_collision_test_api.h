#pragma once

#include "world_bootstrap.h"

#include <array>

namespace hl::game_api::detail
{
struct GoldSrcBspCollisionTraceResult final
{
    bool initialized = false;
    bool allsolid = false;
    bool startsolid = false;
    bool inopen = false;
    bool inwater = false;
    float fraction = 0.0f;
    int entity = -1;
    std::array<float, 3> end_position{};
    std::array<float, 3> plane_normal{};
    float plane_distance = 0.0f;
    int backoff_steps = 0;
    int end_contents = 0;
};

struct GoldSrcBspCollisionContentsResult final
{
    bool initialized = false;
    bool hull_available = false;
    int contents = 0;
};

GoldSrcBspCollisionTraceResult GoldSrcBspCollisionTraceForTesting(
    const WorldModelContext& world,
    const std::array<float, 3>& start,
    const std::array<float, 3>& end,
    int usehull) noexcept;

GoldSrcBspCollisionContentsResult GoldSrcBspCollisionContentsForTesting(
    const WorldModelContext& world,
    const std::array<float, 3>& point,
    int usehull,
    int node) noexcept;

GoldSrcBspCollisionTraceResult GoldSrcBspCollisionBackoffForTesting(
    const WorldModelContext& world,
    const std::array<float, 3>& start,
    const std::array<float, 3>& end,
    float candidate_fraction,
    int usehull) noexcept;
} // namespace hl::game_api::detail
