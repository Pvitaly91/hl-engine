#include "goldsrc_pmove_collision_test_api.h"

#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <vector>

#pragma warning(push, 0)
#include "pm_defs.h"
#pragma warning(pop)

namespace
{
using hl::game_api::detail::BspCollisionClipnode;
using hl::game_api::detail::BspCollisionPlane;
using hl::game_api::detail::BspInlineModelBounds;
using hl::game_api::detail::GoldSrcBspCollisionTraceForTesting;
using hl::game_api::detail::GoldSrcBspCollisionTraceResult;
using hl::game_api::detail::GoldSrcBspCollisionContentsForTesting;
using hl::game_api::detail::GoldSrcBspCollisionBackoffForTesting;
using hl::game_api::detail::WorldModelContext;

constexpr float kTolerance = 0.001f;

bool Near(float left, float right, float tolerance = kTolerance)
{
    return std::fabs(left - right) <= tolerance;
}

BspCollisionPlane Plane(
    float x,
    float y,
    float z,
    float distance,
    std::int32_t type)
{
    BspCollisionPlane result;
    result.normal = Vector(x, y, z);
    result.distance = distance;
    result.type = type;
    return result;
}

BspCollisionClipnode Node(
    std::int32_t plane,
    std::int32_t front,
    std::int32_t back)
{
    BspCollisionClipnode result;
    result.plane_index = plane;
    result.children = {front, back};
    return result;
}

WorldModelContext World(
    std::initializer_list<BspCollisionPlane> planes,
    std::initializer_list<BspCollisionClipnode> nodes)
{
    WorldModelContext world;
    world.model_path = "maps/analytical_collision_fixture.bsp";
    world.bsp_loaded = true;
    world.prepared = true;
    world.collision_loaded = true;
    world.collision_planes.assign(planes.begin(), planes.end());
    world.collision_clipnodes.assign(nodes.begin(), nodes.end());
    world.point_hull_nodes = world.collision_clipnodes;
    BspInlineModelBounds model;
    model.valid = true;
    model.mins = Vector(-4096.0f, -4096.0f, -4096.0f);
    model.maxs = Vector(4096.0f, 4096.0f, 4096.0f);
    model.headnodes = {0, 0, 0, 0};
    world.inline_models.push_back(model);
    return world;
}

WorldModelContext StepWorld(float height)
{
    return World(
        {
            Plane(1.0f, 0.0f, 0.0f, 0.0f, 0),
            Plane(0.0f, 0.0f, 1.0f, 36.0f, 2),
            Plane(0.0f, 0.0f, 1.0f, 36.0f + height, 2),
        },
        {
            Node(0, 2, 1),
            Node(1, CONTENTS_EMPTY, CONTENTS_SOLID),
            Node(2, CONTENTS_EMPTY, CONTENTS_SOLID),
        });
}

GoldSrcBspCollisionTraceResult Trace(
    const WorldModelContext& world,
    std::array<float, 3> start,
    std::array<float, 3> end,
    int usehull = 0)
{
    const auto result =
        GoldSrcBspCollisionTraceForTesting(world, start, end, usehull);
    assert(result.initialized);
    return result;
}

void AssertFreeSweep(const GoldSrcBspCollisionTraceResult& trace)
{
    assert(!trace.allsolid);
    assert(!trace.startsolid);
    assert(Near(trace.fraction, 1.0f));
    assert(trace.entity == -1);
    assert(trace.end_contents != CONTENTS_SOLID);
}


bool CanStockStep(const WorldModelContext& world, int usehull = 0)
{
    constexpr float kStockStepSize = 18.0f;
    const std::array<float, 3> start{-32.0f, 0.0f, 36.0f};
    const std::array<float, 3> raised{
        start[0], start[1], start[2] + kStockStepSize};
    const std::array<float, 3> forward{
        32.0f, raised[1], raised[2]};
    const auto up = Trace(world, start, raised, usehull);
    if (up.startsolid || up.allsolid || up.fraction < 1.0f)
    {
        return false;
    }
    const auto across = Trace(world, raised, forward, usehull);
    if (across.startsolid || across.allsolid || across.fraction < 1.0f)
    {
        return false;
    }
    const auto down = Trace(
        world,
        forward,
        {forward[0], forward[1], start[2]},
        usehull);
    return !down.startsolid && !down.allsolid
        && down.plane_normal[2] >= 0.7f
        && down.end_contents != CONTENTS_SOLID;
}
} // namespace

int main()
{
    const WorldModelContext floor = World(
        {Plane(0.0f, 0.0f, 1.0f, 36.0f, 2)},
        {Node(0, CONTENTS_EMPTY, CONTENTS_SOLID)});

    {
        const auto empty = GoldSrcBspCollisionContentsForTesting(
            floor, {0.0f, 0.0f, 36.001f}, 0, 0);
        const auto boundary = GoldSrcBspCollisionContentsForTesting(
            floor, {0.0f, 0.0f, 36.0f}, 0, 0);
        const auto solid = GoldSrcBspCollisionContentsForTesting(
            floor, {0.0f, 0.0f, 35.999f}, 0, 0);
        assert(empty.initialized && empty.hull_available);
        assert(empty.contents == CONTENTS_EMPTY);
        assert(boundary.contents == CONTENTS_EMPTY);
        assert(solid.contents == CONTENTS_SOLID);
        for (int usehull = 0; usehull <= 2; ++usehull)
        {
            const auto hull = GoldSrcBspCollisionContentsForTesting(
                floor, {0.0f, 0.0f, 64.0f}, usehull, 0);
            assert(hull.initialized && hull.hull_available);
            assert(hull.contents == CONTENTS_EMPTY);
        }
        const auto invalid_hull = GoldSrcBspCollisionContentsForTesting(
            floor, {0.0f, 0.0f, 64.0f}, 3, 0);
        assert(invalid_hull.initialized);
        assert(!invalid_hull.hull_available);
        assert(invalid_hull.contents == CONTENTS_SOLID);
        const auto invalid_node = GoldSrcBspCollisionContentsForTesting(
            floor, {0.0f, 0.0f, 64.0f}, 0, 1);
        assert(invalid_node.contents == CONTENTS_SOLID);
    }

    {
        const auto empty_to_empty = Trace(
            floor,
            {0.0f, 0.0f, 64.0f},
            {32.0f, 0.0f, 64.0f});
        AssertFreeSweep(empty_to_empty);
        const auto solid_to_solid = Trace(
            floor,
            {0.0f, 0.0f, 30.0f},
            {32.0f, 0.0f, 30.0f});
        assert(solid_to_solid.startsolid);
        assert(solid_to_solid.allsolid);
        const auto touching_inward = Trace(
            floor,
            {0.0f, 0.0f, 36.0f},
            {0.0f, 0.0f, 20.0f});
        assert(!touching_inward.startsolid);
        assert(touching_inward.fraction >= 0.0f);
        assert(touching_inward.fraction < 1.0f);
        assert(touching_inward.end_contents != CONTENTS_SOLID);
    }

    {
        const auto impact = Trace(
            floor,
            {0.0f, 0.0f, 64.0f},
            {0.0f, 0.0f, 0.0f});
        assert(!impact.startsolid);
        assert(!impact.allsolid);
        assert(impact.fraction > 0.0f && impact.fraction < 1.0f);
        assert(impact.entity == 0);
        assert(Near(impact.plane_normal[2], 1.0f));
        assert(Near(impact.plane_distance, 36.0f));
        assert(impact.end_contents != CONTENTS_SOLID);
    }

    {
        const WorldModelContext nested_near_impact = World(
            {
                Plane(1.0f, 0.0f, 0.0f, 0.0f, 0),
                Plane(1.0f, 0.0f, 0.0f, 5.0f, 0),
            },
            {
                Node(0, 1, CONTENTS_SOLID),
                Node(1, CONTENTS_EMPTY, CONTENTS_SOLID),
            });
        const auto first_impact = Trace(
            nested_near_impact,
            {10.0f, 0.0f, 64.0f},
            {-10.0f, 0.0f, 64.0f});
        assert(!first_impact.startsolid);
        assert(!first_impact.allsolid);
        assert(Near(first_impact.fraction, 0.2484375f));
        assert(Near(first_impact.end_position[0], 5.03125f));
        assert(Near(first_impact.plane_normal[0], 1.0f));
        assert(Near(first_impact.plane_normal[1], 0.0f));
        assert(Near(first_impact.plane_normal[2], 0.0f));
        assert(Near(first_impact.plane_distance, 5.0f));
        assert(first_impact.entity == 0);
        assert(first_impact.end_contents != CONTENTS_SOLID);
    }

    {
        const auto corrected = GoldSrcBspCollisionBackoffForTesting(
            floor,
            {0.0f, 0.0f, 64.0f},
            {0.0f, 0.0f, 0.0f},
            1.0f,
            0);
        assert(corrected.initialized);
        assert(corrected.entity == 0);
        assert(corrected.backoff_steps > 0);
        assert(corrected.backoff_steps <= 12);
        assert(corrected.fraction >= 0.0f
            && corrected.fraction <= 1.0f);
        assert(corrected.end_contents != CONTENTS_SOLID);

        const auto exhausted = GoldSrcBspCollisionBackoffForTesting(
            floor,
            {0.0f, 0.0f, 30.0f},
            {0.0f, 0.0f, 20.0f},
            1.0f,
            0);
        assert(exhausted.initialized);
        assert(exhausted.entity == -1);
        assert(exhausted.backoff_steps > 0);
        assert(exhausted.backoff_steps <= 12);
        assert(exhausted.end_contents == CONTENTS_SOLID);
    }

    {
        AssertFreeSweep(Trace(
            floor,
            {0.0f, 0.0f, 36.0f},
            {0.0f, 0.0f, 52.0f}));
        AssertFreeSweep(Trace(
            floor,
            {0.0f, 0.0f, 36.0f},
            {16.0f, 0.0f, 36.0f}));
        const auto embedded = Trace(
            floor,
            {0.0f, 0.0f, 35.0f},
            {0.0f, 0.0f, 40.0f});
        assert(embedded.startsolid);
        assert(!embedded.allsolid);
    }

    {
        const WorldModelContext inverted = World(
            {Plane(0.0f, 0.0f, 1.0f, 36.0f, 2)},
            {Node(0, CONTENTS_SOLID, CONTENTS_EMPTY)});
        const auto back_impact = Trace(
            inverted,
            {0.0f, 0.0f, 0.0f},
            {0.0f, 0.0f, 64.0f});
        assert(!back_impact.startsolid);
        assert(Near(back_impact.plane_normal[2], -1.0f));
        assert(Near(back_impact.plane_distance, -36.0f));
        assert(back_impact.end_contents != CONTENTS_SOLID);
    }

    const WorldModelContext step = StepWorld(18.0f);

    {
        const auto blocked = Trace(
            step,
            {-32.0f, 0.0f, 36.0f},
            {32.0f, 0.0f, 36.0f});
        assert(blocked.fraction < 1.0f);
        assert(!blocked.startsolid);
        assert(blocked.end_contents != CONTENTS_SOLID);

        AssertFreeSweep(Trace(
            step,
            {-32.0f, 0.0f, 36.0f},
            {-32.0f, 0.0f, 54.0f}));
        AssertFreeSweep(Trace(
            step,
            {-32.0f, 0.0f, 54.0f},
            {32.0f, 0.0f, 54.0f}));
        const auto down = Trace(
            step,
            {32.0f, 0.0f, 72.0f},
            {32.0f, 0.0f, 36.0f});
        assert(!down.startsolid);
        assert(down.fraction < 1.0f);
        assert(Near(down.plane_normal[2], 1.0f));
        assert(down.end_contents != CONTENTS_SOLID);
    }

    {
        assert(CanStockStep(StepWorld(0.5f)));
        assert(CanStockStep(StepWorld(17.0f)));
        assert(CanStockStep(StepWorld(18.0f)));
        assert(!CanStockStep(StepWorld(18.1f)));
        assert(!CanStockStep(StepWorld(32.0f)));
        assert(CanStockStep(StepWorld(18.0f), 0));
        assert(CanStockStep(StepWorld(18.0f), 1));
        const auto reverse = Trace(
            StepWorld(32.0f),
            {-0.05f, 0.0f, 36.05f},
            {-32.0f, 0.0f, 36.05f});
        AssertFreeSweep(reverse);
        const auto strafe = Trace(
            StepWorld(32.0f),
            {-0.05f, 0.0f, 36.05f},
            {-0.05f, 32.0f, 36.05f});
        AssertFreeSweep(strafe);
    }

    constexpr float kRampNormalX = -0.4472136f;
    constexpr float kRampNormalZ = 0.8944272f;
    const WorldModelContext ramp = World(
        {Plane(
            kRampNormalX,
            0.0f,
            kRampNormalZ,
            36.0f * kRampNormalZ,
            3)},
        {Node(0, CONTENTS_EMPTY, CONTENTS_SOLID)});

    {
        std::array<float, 3> position{-32.0f, 0.0f, 20.05f};
        for (int command = 0; command < 256; ++command)
        {
            const float direction = command % 128 < 64 ? 1.0f : -1.0f;
            std::array<float, 3> next = position;
            next[0] += direction;
            next[2] += 0.5f * direction;
            AssertFreeSweep(Trace(ramp, position, next));
            const auto ground = Trace(
                ramp,
                next,
                {next[0], next[1], next[2] - 2.0f});
            assert(!ground.startsolid);
            assert(!ground.allsolid);
            assert(ground.fraction < 1.0f);
            assert(ground.plane_normal[2] >= 0.7f);
            assert(ground.end_contents != CONTENTS_SOLID);
            position = next;
        }
        const auto stopped = Trace(ramp, position, position);
        AssertFreeSweep(stopped);
        const auto strafe = Trace(
            ramp,
            position,
            {position[0], position[1] + 32.0f, position[2]});
        AssertFreeSweep(strafe);
    }

    {
        constexpr float kSteepNormalX = -0.8660254f;
        constexpr float kSteepNormalZ = 0.5f;
        const WorldModelContext steep = World(
            {Plane(
                kSteepNormalX,
                0.0f,
                kSteepNormalZ,
                36.0f * kSteepNormalZ,
                3)},
            {Node(0, CONTENTS_EMPTY, CONTENTS_SOLID)});
        const auto contact = Trace(
            steep,
            {0.0f, 0.0f, 64.0f},
            {0.0f, 0.0f, 0.0f});
        assert(contact.fraction < 1.0f);
        assert(contact.plane_normal[2] < 0.7f);

        const WorldModelContext wall = World(
            {Plane(-1.0f, 0.0f, 0.0f, -64.0f, 3)},
            {Node(0, CONTENTS_EMPTY, CONTENTS_SOLID)});
        const auto blocked = Trace(
            wall,
            {0.0f, 0.0f, 36.0f},
            {100.0f, 0.0f, 36.0f});
        assert(blocked.fraction < 1.0f);
        assert(Near(blocked.plane_normal[2], 0.0f));
        AssertFreeSweep(Trace(
            wall,
            blocked.end_position,
            {blocked.end_position[0] - 32.0f,
             blocked.end_position[1],
             blocked.end_position[2]}));
        AssertFreeSweep(Trace(
            wall,
            blocked.end_position,
            {blocked.end_position[0],
             blocked.end_position[1] + 32.0f,
             blocked.end_position[2]}));
    }

    {
        const WorldModelContext seam = World(
            {
                Plane(1.0f, 0.0f, 0.0f, 0.0f, 0),
                Plane(0.0f, 0.0f, 1.0f, 36.0f, 2),
                Plane(0.0f, 0.0f, 1.0f, 36.02f, 2),
            },
            {
                Node(0, 2, 1),
                Node(1, CONTENTS_EMPTY, CONTENTS_SOLID),
                Node(2, CONTENTS_EMPTY, CONTENTS_SOLID),
            });
        for (int command = 0; command < 256; ++command)
        {
            const bool forward = (command % 2) == 0;
            const std::array<float, 3> start =
                forward ? std::array<float, 3>{-1.0f, 0.0f, 36.05f}
                        : std::array<float, 3>{1.0f, 0.0f, 36.05f};
            const std::array<float, 3> end =
                forward ? std::array<float, 3>{1.0f, 0.0f, 36.05f}
                        : std::array<float, 3>{-1.0f, 0.0f, 36.05f};
            AssertFreeSweep(Trace(seam, start, end));
            AssertFreeSweep(Trace(seam, end, end));
        }
    }

    {
        const WorldModelContext corner = World(
            {
                Plane(1.0f, 0.0f, 0.0f, 64.0f, 0),
                Plane(0.0f, 0.0f, 1.0f, 36.0f, 2),
            },
            {
                Node(0, CONTENTS_SOLID, 1),
                Node(1, CONTENTS_EMPTY, CONTENTS_SOLID),
            });
        for (int command = 0; command < 128; ++command)
        {
            const auto diagonal = Trace(
                corner,
                {0.0f, 0.0f, 100.0f},
                {100.0f, 0.0f, 0.0f});
            assert(!diagonal.startsolid);
            assert(!diagonal.allsolid);
            assert(diagonal.fraction < 1.0f);
            assert(diagonal.end_contents != CONTENTS_SOLID);
        }
    }


    {
        std::vector<BspCollisionClipnode> long_nodes(4098u);
        std::vector<BspCollisionPlane> long_planes(1u);
        long_planes[0] = Plane(1.0f, 0.0f, 0.0f, 0.0f, 0);
        for (std::size_t index = 0; index < long_nodes.size(); ++index)
        {
            long_nodes[index] = Node(
                0,
                index + 1u < long_nodes.size()
                    ? static_cast<std::int32_t>(index + 1u)
                    : CONTENTS_EMPTY,
                CONTENTS_SOLID);
        }
        WorldModelContext bounded;
        bounded.model_path = "maps/bounded_traversal_fixture.bsp";
        bounded.bsp_loaded = true;
        bounded.prepared = true;
        bounded.collision_loaded = true;
        bounded.collision_planes = long_planes;
        bounded.collision_clipnodes = long_nodes;
        bounded.point_hull_nodes = long_nodes;
        BspInlineModelBounds model;
        model.valid = true;
        model.headnodes = {0, 0, 0, 0};
        bounded.inline_models.push_back(model);
        const auto limited = GoldSrcBspCollisionContentsForTesting(
            bounded, {1.0f, 0.0f, 0.0f}, 0, 0);
        assert(limited.initialized);
        assert(limited.contents == CONTENTS_SOLID);
    }

    {
        WorldModelContext malformed = floor;
        malformed.collision_planes[0].normal =
            Vector(0.0f, 0.0f, 0.5f);
        const auto rejected = GoldSrcBspCollisionContentsForTesting(
            malformed, {0.0f, 0.0f, 64.0f}, 0, 0);
        assert(!rejected.initialized);
    }

    {
        std::array<float, 3> position{-32.0f, 0.0f, 20.05f};
        for (int command = 0; command < 10000; ++command)
        {
            const float direction = command % 200 < 100 ? 1.0f : -1.0f;
            const std::array<float, 3> next{
                position[0] + direction * 0.25f,
                position[1],
                position[2] + direction * 0.125f};
            const auto trace = Trace(ramp, position, next);
            AssertFreeSweep(trace);
            position = next;
        }
        assert(Near(position[0], -32.0f, 0.01f));
        assert(Near(position[2], 20.05f, 0.01f));
    }

    return 0;
}
