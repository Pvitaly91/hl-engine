#include "game_api/goldsrc_pmove_runtime.h"

#include "goldsrc_pmove_collision_test_api.h"
#include "world_bootstrap.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <limits>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#pragma warning(push, 0)
#include "extdll.h"
#include "com_model.h"
#include "entity_state.h"
#include "in_buttons.h"
#include "pm_defs.h"
#include "pm_movevars.h"
#pragma warning(pop)

namespace hl::game_api::detail
{
namespace
{
constexpr std::size_t kMaximumClients = 32u;
constexpr int kMaximumTraceDepth = 4096;
constexpr std::size_t kMaximumTraceWork = 65536u;
constexpr int kMaximumTraceBackoffSteps = 12;
constexpr float kTraceEpsilon = 0.03125f;
constexpr float kTraceBackoffFraction = 0.1f;
constexpr float kMaximumCoordinate = 32768.0f;
constexpr std::array<std::array<float, 3>, 4> kPlayerMins = {{
    {{-16.0f, -16.0f, -36.0f}},
    {{-16.0f, -16.0f, -18.0f}},
    {{0.0f, 0.0f, 0.0f}},
    {{-16.0f, -16.0f, -18.0f}},
}};
constexpr std::array<std::array<float, 3>, 4> kPlayerMaxs = {{
    {{16.0f, 16.0f, 36.0f}},
    {{16.0f, 16.0f, 18.0f}},
    {{0.0f, 0.0f, 0.0f}},
    {{16.0f, 16.0f, 18.0f}},
}};

bool FiniteVector(const float* value) noexcept
{
    return value != nullptr
        && std::isfinite(value[0])
        && std::isfinite(value[1])
        && std::isfinite(value[2]);
}

float PlaneDistance(const mplane_t& plane, const float* point) noexcept
{
    if (plane.type < 3)
    {
        return point[plane.type] - plane.dist;
    }
    return point[0] * plane.normal[0]
        + point[1] * plane.normal[1]
        + point[2] * plane.normal[2]
        - plane.dist;
}

void CopyVector(const float* source, float* destination) noexcept
{
    destination[0] = source[0];
    destination[1] = source[1];
    destination[2] = source[2];
}

void SetVector(float* destination, float x, float y, float z) noexcept
{
    destination[0] = x;
    destination[1] = y;
    destination[2] = z;
}

enum class RecursiveTraceResult
{
    kClear,
    kHit,
    kInvalid,
};

class BspCollisionWorld final
{
public:
    bool Initialize(const WorldModelContext& world)
    {
        ready_ = false;
        planes_.clear();
        clipnodes_.clear();
        point_nodes_.clear();
        model_ = {};
        if (!world.collision_loaded
            || world.inline_models.empty()
            || world.collision_planes.empty()
            || world.collision_clipnodes.empty()
            || world.point_hull_nodes.empty())
        {
            return false;
        }

        planes_.resize(world.collision_planes.size());
        for (std::size_t index = 0; index < planes_.size(); ++index)
        {
            const BspCollisionPlane& source = world.collision_planes[index];
            const float normal_length_squared =
                source.normal.x * source.normal.x
                + source.normal.y * source.normal.y
                + source.normal.z * source.normal.z;
            if (!std::isfinite(normal_length_squared)
                || std::fabs(normal_length_squared - 1.0f) > 0.02f
                || source.type < 0 || source.type > 5)
            {
                return false;
            }
            mplane_t& output = planes_[index];
            SetVector(
                output.normal,
                source.normal.x,
                source.normal.y,
                source.normal.z);
            output.dist = source.distance;
            output.type = static_cast<byte>(source.type);
            output.signbits = static_cast<byte>(
                (output.normal[0] < 0.0f ? 1 : 0)
                | (output.normal[1] < 0.0f ? 2 : 0)
                | (output.normal[2] < 0.0f ? 4 : 0));
        }

        clipnodes_.resize(world.collision_clipnodes.size());
        for (std::size_t index = 0; index < clipnodes_.size(); ++index)
        {
            const BspCollisionClipnode& source =
                world.collision_clipnodes[index];
            dclipnode_t& output = clipnodes_[index];
            output.planenum = source.plane_index;
            for (std::size_t side = 0; side < 2; ++side)
            {
                const std::int32_t child = source.children[side];
                if (child < std::numeric_limits<short>::min()
                    || child > std::numeric_limits<short>::max())
                {
                    return false;
                }
                output.children[side] = static_cast<short>(child);
            }
        }

        point_nodes_.resize(world.point_hull_nodes.size());
        for (std::size_t index = 0; index < point_nodes_.size(); ++index)
        {
            const BspCollisionClipnode& source =
                world.point_hull_nodes[index];
            dclipnode_t& output = point_nodes_[index];
            output.planenum = source.plane_index;
            for (std::size_t side = 0; side < 2; ++side)
            {
                const std::int32_t child = source.children[side];
                if (child < std::numeric_limits<short>::min()
                    || child > std::numeric_limits<short>::max())
                {
                    return false;
                }
                output.children[side] = static_cast<short>(child);
            }
        }

        const BspInlineModelBounds& world_model =
            world.inline_models.front();
        std::snprintf(
            model_.name,
            sizeof(model_.name),
            "%s",
            world.model_path.c_str());
        model_.type = mod_brush;
        model_.numplanes = static_cast<int>(planes_.size());
        model_.planes = planes_.data();
        model_.numclipnodes = static_cast<int>(clipnodes_.size());
        model_.clipnodes = clipnodes_.data();
        SetVector(
            model_.mins,
            world_model.mins.x,
            world_model.mins.y,
            world_model.mins.z);
        SetVector(
            model_.maxs,
            world_model.maxs.x,
            world_model.maxs.y,
            world_model.maxs.z);

        ConfigureHull(
            &model_.hulls[0],
            point_nodes_.data(),
            point_nodes_.size(),
            world_model.headnodes[0],
            kPlayerMins[2],
            kPlayerMaxs[2]);
        ConfigureHull(
            &model_.hulls[1],
            clipnodes_.data(),
            clipnodes_.size(),
            world_model.headnodes[1],
            kPlayerMins[0],
            kPlayerMaxs[0]);
        ConfigureHull(
            &model_.hulls[2],
            clipnodes_.data(),
            clipnodes_.size(),
            world_model.headnodes[2],
            {{-32.0f, -32.0f, -32.0f}},
            {{32.0f, 32.0f, 32.0f}});
        ConfigureHull(
            &model_.hulls[3],
            clipnodes_.data(),
            clipnodes_.size(),
            world_model.headnodes[3],
            kPlayerMins[1],
            kPlayerMaxs[1]);
        for (hull_t& hull : model_.hulls)
        {
            hull.planes = planes_.data();
        }
        ready_ = true;
        return true;
    }

    bool ready() const noexcept
    {
        return ready_;
    }

    model_t* model() noexcept
    {
        return ready_ ? &model_ : nullptr;
    }

    const model_t* model() const noexcept
    {
        return ready_ ? &model_ : nullptr;
    }

    hull_t* HullForUseHull(int usehull, float* offset) noexcept
    {
        if (!ready_ || usehull < 0 || usehull > 2)
        {
            return nullptr;
        }
        const int model_hull = usehull == 1 ? 3 : usehull == 2 ? 0 : 1;
        hull_t* hull = &model_.hulls[model_hull];
        if (offset != nullptr)
        {
            for (int axis = 0; axis < 3; ++axis)
            {
                offset[axis] =
                    hull->clip_mins[axis] - kPlayerMins[usehull][axis];
            }
        }
        return hull;
    }

    int HullPointContents(
        const hull_t* hull,
        int node,
        const float* point,
        std::size_t* trace_work = nullptr) const noexcept
    {
        if (!ready_ || hull == nullptr || point == nullptr
            || hull->clipnodes == nullptr || hull->planes != planes_.data())
        {
            return CONTENTS_SOLID;
        }
        int steps = 0;
        while (node >= 0)
        {
            if (++steps > kMaximumTraceDepth
                || (trace_work != nullptr
                    && ++(*trace_work) > kMaximumTraceWork)
                || node < hull->firstclipnode
                || node > hull->lastclipnode)
            {
                return CONTENTS_SOLID;
            }
            const dclipnode_t& clipnode = hull->clipnodes[node];
            if (clipnode.planenum < 0
                || clipnode.planenum >= static_cast<int>(planes_.size()))
            {
                return CONTENTS_SOLID;
            }
            const mplane_t& plane = planes_[clipnode.planenum];
            node = clipnode.children[
                PlaneDistance(plane, point) < 0.0f ? 1 : 0];
        }
        return node;
    }

    int PointContents(const float* point) const noexcept
    {
        if (!ready_)
        {
            return CONTENTS_SOLID;
        }
        const hull_t& hull = model_.hulls[0];
        std::size_t trace_work = 0u;
        return HullPointContents(
            &hull,
            hull.firstclipnode,
            point,
            &trace_work);
    }

    pmtrace_t Trace(
        const float* start,
        const float* end,
        int usehull) noexcept
    {
        last_trace_backoff_steps_ = 0;
        pmtrace_t trace{};
        trace.allsolid = TRUE;
        trace.fraction = 1.0f;
        trace.ent = -1;
        if (!FiniteVector(start) || !FiniteVector(end))
        {
            trace.startsolid = TRUE;
            trace.fraction = 0.0f;
            return trace;
        }

        float offset[3]{};
        hull_t* hull = HullForUseHull(usehull, offset);
        if (hull == nullptr)
        {
            trace.startsolid = TRUE;
            trace.fraction = 0.0f;
            return trace;
        }

        float local_start[3]{};
        float local_end[3]{};
        for (int axis = 0; axis < 3; ++axis)
        {
            local_start[axis] = start[axis] - offset[axis];
            local_end[axis] = end[axis] - offset[axis];
            trace.endpos[axis] = end[axis];
        }
        std::size_t trace_work = 0u;
        const RecursiveTraceResult trace_result = RecursiveTrace(
            *hull,
            hull->firstclipnode,
            0.0f,
            1.0f,
            local_start,
            local_end,
            &trace,
            0,
            &trace_work);
        if (trace_result == RecursiveTraceResult::kInvalid)
        {
            trace.fraction = 0.0f;
        }
        if (trace.fraction < 1.0f)
        {
            for (int axis = 0; axis < 3; ++axis)
            {
                trace.endpos[axis] =
                    start[axis] + trace.fraction * (end[axis] - start[axis]);
            }
            trace.ent = 0;
        }
        else if (trace.startsolid)
        {
            trace.ent = 0;
        }
        return trace;
    }

    int last_trace_backoff_steps() const noexcept
    {
        return last_trace_backoff_steps_;
    }

    bool BackoffCandidateForTesting(
        const float* start,
        const float* end,
        int usehull,
        float* fraction,
        float* middle_fraction,
        float* middle) noexcept
    {
        float offset[3]{};
        hull_t* hull = HullForUseHull(usehull, offset);
        if (hull == nullptr || !FiniteVector(start) || !FiniteVector(end)
            || fraction == nullptr || !std::isfinite(*fraction)
            || *fraction < 0.0f || *fraction > 1.0f
            || middle_fraction == nullptr || middle == nullptr)
        {
            return false;
        }
        float local_start[3]{};
        float local_end[3]{};
        for (int axis = 0; axis < 3; ++axis)
        {
            local_start[axis] = start[axis] - offset[axis];
            local_end[axis] = end[axis] - offset[axis];
            middle[axis] = local_start[axis]
                + *fraction * (local_end[axis] - local_start[axis]);
        }
        *middle_fraction = *fraction;
        last_trace_backoff_steps_ = 0;
        std::size_t trace_work = 0u;
        const bool corrected = BackoffImpactPoint(
            *hull,
            0.0f,
            1.0f,
            local_start,
            local_end,
            fraction,
            middle_fraction,
            middle,
            &trace_work);
        if (corrected)
        {
            for (int axis = 0; axis < 3; ++axis)
            {
                middle[axis] += offset[axis];
            }
        }
        return corrected;
    }

private:
    bool BackoffImpactPoint(
        const hull_t& hull,
        float start_fraction,
        float end_fraction,
        const float* start,
        const float* end,
        float* fraction,
        float* middle_fraction,
        float* middle,
        std::size_t* trace_work) const noexcept
    {
        if (fraction == nullptr || middle_fraction == nullptr
            || middle == nullptr || trace_work == nullptr)
        {
            return false;
        }

        int backoff_steps = 0;
        while (true)
        {
            if (*trace_work >= kMaximumTraceWork)
            {
                return false;
            }
            const int contents = HullPointContents(
                &hull,
                hull.firstclipnode,
                middle,
                trace_work);
            if (*trace_work > kMaximumTraceWork)
            {
                return false;
            }
            if (contents != CONTENTS_SOLID)
            {
                return true;
            }
            if (++backoff_steps > kMaximumTraceBackoffSteps)
            {
                return false;
            }

            ++last_trace_backoff_steps_;
            *fraction -= kTraceBackoffFraction;
            if (*fraction < 0.0f)
            {
                *fraction = 0.0f;
            }
            *middle_fraction =
                start_fraction
                + (end_fraction - start_fraction) * *fraction;
            for (int axis = 0; axis < 3; ++axis)
            {
                middle[axis] =
                    start[axis] + *fraction * (end[axis] - start[axis]);
            }
            if (*fraction == 0.0f)
            {
                if (*trace_work >= kMaximumTraceWork)
                {
                    return false;
                }
                const int start_contents = HullPointContents(
                    &hull,
                    hull.firstclipnode,
                    middle,
                    trace_work);
                return *trace_work <= kMaximumTraceWork
                    && start_contents != CONTENTS_SOLID;
            }
        }
    }

    static void ConfigureHull(
        hull_t* hull,
        dclipnode_t* nodes,
        std::size_t node_count,
        int headnode,
        const std::array<float, 3>& mins,
        const std::array<float, 3>& maxs) noexcept
    {
        *hull = {};
        hull->clipnodes = nodes;
        hull->firstclipnode = headnode;
        hull->lastclipnode =
            node_count == 0u ? -1 : static_cast<int>(node_count - 1u);
        for (int axis = 0; axis < 3; ++axis)
        {
            hull->clip_mins[axis] = mins[axis];
            hull->clip_maxs[axis] = maxs[axis];
        }
    }

    RecursiveTraceResult RecursiveTrace(
        const hull_t& hull,
        int node,
        float start_fraction,
        float end_fraction,
        const float* start,
        const float* end,
        pmtrace_t* trace,
        int depth,
        std::size_t* trace_work) const noexcept
    {
        if (trace == nullptr || trace_work == nullptr
            || depth > kMaximumTraceDepth
            || ++(*trace_work) > kMaximumTraceWork)
        {
            return RecursiveTraceResult::kInvalid;
        }
        if (node < 0)
        {
            if (node != CONTENTS_SOLID)
            {
                trace->allsolid = FALSE;
                if (node == CONTENTS_EMPTY)
                {
                    trace->inopen = TRUE;
                }
                else
                {
                    trace->inwater = TRUE;
                }
            }
            else
            {
                trace->startsolid = TRUE;
            }
            return RecursiveTraceResult::kClear;
        }
        if (node < hull.firstclipnode || node > hull.lastclipnode)
        {
            return RecursiveTraceResult::kInvalid;
        }
        const dclipnode_t& clipnode = hull.clipnodes[node];
        if (clipnode.planenum < 0
            || clipnode.planenum >= static_cast<int>(planes_.size()))
        {
            return RecursiveTraceResult::kInvalid;
        }
        const mplane_t& plane = planes_[clipnode.planenum];
        const float start_distance = PlaneDistance(plane, start);
        const float end_distance = PlaneDistance(plane, end);
        if (start_distance >= 0.0f && end_distance >= 0.0f)
        {
            return RecursiveTrace(
                hull,
                clipnode.children[0],
                start_fraction,
                end_fraction,
                start,
                end,
                trace,
                depth + 1,
                trace_work);
        }
        if (start_distance < 0.0f && end_distance < 0.0f)
        {
            return RecursiveTrace(
                hull,
                clipnode.children[1],
                start_fraction,
                end_fraction,
                start,
                end,
                trace,
                depth + 1,
                trace_work);
        }

        int side = 0;
        float fraction = 0.0f;
        if (start_distance < end_distance)
        {
            side = 1;
            fraction =
                (start_distance + kTraceEpsilon)
                / (start_distance - end_distance);
        }
        else
        {
            side = 0;
            fraction =
                (start_distance - kTraceEpsilon)
                / (start_distance - end_distance);
        }
        fraction = (std::max)(0.0f, (std::min)(fraction, 1.0f));
        float middle_fraction =
            start_fraction
            + (end_fraction - start_fraction) * fraction;
        float middle[3]{};
        for (int axis = 0; axis < 3; ++axis)
        {
            middle[axis] = start[axis] + fraction * (end[axis] - start[axis]);
        }
        const RecursiveTraceResult near_result = RecursiveTrace(
            hull,
            clipnode.children[side],
            start_fraction,
            middle_fraction,
            start,
            middle,
            trace,
            depth + 1,
            trace_work);
        if (near_result != RecursiveTraceResult::kClear)
        {
            return near_result;
        }
        const int far_contents = HullPointContents(
            &hull,
            clipnode.children[side ^ 1],
            middle,
            trace_work);
        if (*trace_work > kMaximumTraceWork)
        {
            return RecursiveTraceResult::kInvalid;
        }
        if (far_contents != CONTENTS_SOLID)
        {
            return RecursiveTrace(
                hull,
                clipnode.children[side ^ 1],
                middle_fraction,
                end_fraction,
                middle,
                end,
                trace,
                depth + 1,
                trace_work);
        }
        if (trace->allsolid)
        {
            trace->fraction = 0.0f;
            return RecursiveTraceResult::kHit;
        }

        if (side == 0)
        {
            CopyVector(plane.normal, trace->plane.normal);
            trace->plane.dist = plane.dist;
        }
        else
        {
            SetVector(
                trace->plane.normal,
                -plane.normal[0],
                -plane.normal[1],
                -plane.normal[2]);
            trace->plane.dist = -plane.dist;
        }

        if (!BackoffImpactPoint(
                hull,
                start_fraction,
                end_fraction,
                start,
                end,
                &fraction,
                &middle_fraction,
                middle,
                trace_work))
        {
            return RecursiveTraceResult::kInvalid;
        }
        trace->fraction = middle_fraction;
        return RecursiveTraceResult::kHit;
    }

    bool ready_ = false;
    std::vector<mplane_t> planes_;
    std::vector<dclipnode_t> clipnodes_;
    std::vector<dclipnode_t> point_nodes_;
    model_t model_{};
    mutable int last_trace_backoff_steps_ = 0;
};

bool SafeWorldTrace(
    BspCollisionWorld* world,
    const float* start,
    const float* end,
    int use_hull,
    pmtrace_t* output) noexcept
{
    if (world == nullptr || output == nullptr)
    {
        return false;
    }
#if defined(_MSC_VER)
    __try
    {
        *output = world->Trace(start, end, use_hull);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
#else
    *output = world->Trace(start, end, use_hull);
    return true;
#endif
}

bool SafePmInit(
    GoldSrcPmInitCallback callback,
    playermove_t* context) noexcept
{
#if defined(_MSC_VER)
    __try
    {
        callback(context);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
#else
    callback(context);
    return true;
#endif
}

bool SafePmMove(
    GoldSrcPmMoveCallback callback,
    playermove_t* context) noexcept
{
#if defined(_MSC_VER)
    __try
    {
        callback(context, TRUE);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
#else
    callback(context, TRUE);
    return true;
#endif
}

bool SafeCmdStart(
    GoldSrcCmdStartCallback callback,
    edict_t* player,
    const usercmd_t* command,
    unsigned int seed) noexcept
{
#if defined(_MSC_VER)
    __try
    {
        callback(player, command, seed);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
#else
    callback(player, command, seed);
    return true;
#endif
}

bool SafeCmdEnd(
    GoldSrcCmdEndCallback callback,
    edict_t* player) noexcept
{
#if defined(_MSC_VER)
    __try
    {
        callback(player);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
#else
    callback(player);
    return true;
#endif
}

bool SafePlayerThink(
    GoldSrcPlayerThinkCallback callback,
    edict_t* player) noexcept
{
    if (callback == nullptr || player == nullptr)
    {
        return false;
    }
#if defined(_MSC_VER)
    __try
    {
        callback(player);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
#else
    callback(player);
    return true;
#endif
}

usercmd_t ConvertCommand(
    const network::GoldSrcDecodedUserCommand& input,
    std::uint8_t msec,
    std::uint16_t filtered_buttons) noexcept
{
    usercmd_t output{};
    output.lerp_msec = static_cast<short>(input.lerp_msec);
    output.msec = msec;
    for (int axis = 0; axis < 3; ++axis)
    {
        output.viewangles[axis] = input.viewangles[axis];
        output.impact_position[axis] = input.impact_position[axis];
    }
    output.forwardmove = input.forwardmove;
    output.sidemove = input.sidemove;
    output.upmove = input.upmove;
    output.lightlevel = input.lightlevel;
    output.buttons = static_cast<unsigned short>(filtered_buttons);
    output.impulse = 0;
    output.weaponselect = 0;
    output.impact_index = 0;
    return output;
}

bool ValidMovevars(const GoldSrcMovevarsConfig& value) noexcept
{
    const std::array<float, 18> fields = {{
        value.gravity,
        value.stop_speed,
        value.maximum_speed,
        value.spectator_maximum_speed,
        value.accelerate,
        value.air_accelerate,
        value.water_accelerate,
        value.friction,
        value.edge_friction,
        value.water_friction,
        value.entity_gravity,
        value.bounce,
        value.step_size,
        value.maximum_velocity,
        value.z_maximum,
        value.wave_height,
        value.roll_angle,
        value.roll_speed,
    }};
    for (const float field : fields)
    {
        if (!std::isfinite(field))
        {
            return false;
        }
    }
    return value.gravity > 0.0f
        && value.stop_speed > 0.0f
        && value.maximum_speed > 0.0f
        && value.accelerate > 0.0f
        && value.air_accelerate >= 0.0f
        && value.friction > 0.0f
        && value.entity_gravity > 0.0f
        && value.step_size > 0.0f
        && value.maximum_velocity >= value.maximum_speed
        && value.z_maximum > 0.0f;
}

void FillMovevars(
    const GoldSrcMovevarsConfig& source,
    movevars_t* destination) noexcept
{
    *destination = {};
    destination->gravity = source.gravity;
    destination->stopspeed = source.stop_speed;
    destination->maxspeed = source.maximum_speed;
    destination->spectatormaxspeed = source.spectator_maximum_speed;
    destination->accelerate = source.accelerate;
    destination->airaccelerate = source.air_accelerate;
    destination->wateraccelerate = source.water_accelerate;
    destination->friction = source.friction;
    destination->edgefriction = source.edge_friction;
    destination->waterfriction = source.water_friction;
    destination->entgravity = source.entity_gravity;
    destination->bounce = source.bounce;
    destination->stepsize = source.step_size;
    destination->maxvelocity = source.maximum_velocity;
    destination->zmax = source.z_maximum;
    destination->waveHeight = source.wave_height;
    destination->footsteps = source.footsteps ? TRUE : FALSE;
    destination->rollangle = source.roll_angle;
    destination->rollspeed = source.roll_speed;
}
} // namespace

void PrepareGoldSrcClientDataForGameDll(clientdata_s* output) noexcept
{
    if (output != nullptr)
    {
        std::memset(output, 0, sizeof(*output));
    }
}

GoldSrcBspCollisionTraceResult GoldSrcBspCollisionTraceForTesting(
    const WorldModelContext& world,
    const std::array<float, 3>& start,
    const std::array<float, 3>& end,
    int usehull) noexcept
{
    GoldSrcBspCollisionTraceResult result;
    BspCollisionWorld collision;
    result.initialized = collision.Initialize(world);
    if (!result.initialized)
    {
        return result;
    }

    const pmtrace_t trace = collision.Trace(
        start.data(),
        end.data(),
        usehull);
    result.allsolid = trace.allsolid != FALSE;
    result.startsolid = trace.startsolid != FALSE;
    result.inopen = trace.inopen != FALSE;
    result.inwater = trace.inwater != FALSE;
    result.fraction = trace.fraction;
    result.entity = trace.ent;
    result.end_position = {
        trace.endpos[0], trace.endpos[1], trace.endpos[2]};
    result.plane_normal = {
        trace.plane.normal[0],
        trace.plane.normal[1],
        trace.plane.normal[2]};
    result.plane_distance = trace.plane.dist;
    result.backoff_steps = collision.last_trace_backoff_steps();
    hull_t* hull = collision.HullForUseHull(usehull, nullptr);
    result.end_contents = hull == nullptr
        ? CONTENTS_SOLID
        : collision.HullPointContents(
            hull,
            hull->firstclipnode,
            result.end_position.data());
    return result;
}

GoldSrcBspCollisionContentsResult GoldSrcBspCollisionContentsForTesting(
    const WorldModelContext& world,
    const std::array<float, 3>& point,
    int usehull,
    int node) noexcept
{
    GoldSrcBspCollisionContentsResult result;
    BspCollisionWorld collision;
    result.initialized = collision.Initialize(world);
    if (!result.initialized)
    {
        return result;
    }
    hull_t* hull = collision.HullForUseHull(usehull, nullptr);
    result.hull_available = hull != nullptr;
    result.contents = hull == nullptr
        ? CONTENTS_SOLID
        : collision.HullPointContents(hull, node, point.data());
    return result;
}

GoldSrcBspCollisionTraceResult GoldSrcBspCollisionBackoffForTesting(
    const WorldModelContext& world,
    const std::array<float, 3>& start,
    const std::array<float, 3>& end,
    float candidate_fraction,
    int usehull) noexcept
{
    GoldSrcBspCollisionTraceResult result;
    BspCollisionWorld collision;
    result.initialized = collision.Initialize(world);
    if (!result.initialized)
    {
        return result;
    }
    float middle_fraction = candidate_fraction;
    std::array<float, 3> middle{};
    const bool corrected = collision.BackoffCandidateForTesting(
        start.data(),
        end.data(),
        usehull,
        &candidate_fraction,
        &middle_fraction,
        middle.data());
    result.fraction = middle_fraction;
    result.end_position = middle;
    result.backoff_steps = collision.last_trace_backoff_steps();
    hull_t* hull = collision.HullForUseHull(usehull, nullptr);
    result.end_contents = corrected && hull != nullptr
        ? collision.HullPointContents(
            hull,
            hull->firstclipnode,
            middle.data())
        : CONTENTS_SOLID;
    result.entity = corrected ? 0 : -1;
    return result;
}

struct GoldSrcPmoveRuntime::Impl
{
    class ActiveScope final
    {
    public:
        explicit ActiveScope(Impl* value) noexcept
            : previous_(active)
        {
            active = value;
        }
        ~ActiveScope()
        {
            active = previous_;
        }
    private:
        Impl* previous_ = nullptr;
    };

    static thread_local Impl* active;

    bool InitializeWorld(const WorldModelContext& context)
    {
        diagnostics.world_collision_ready = world.Initialize(context);
        return diagnostics.world_collision_ready;
    }

    bool InitializeGameDll(
        const GoldSrcPmoveGameDllCallbacks& value,
        const std::filesystem::path& directory,
        const GoldSrcMovevarsConfig& config)
    {
        callbacks = value;
        game_directory = directory;
        movevars_config = config;
        FillMovevars(config, &movevars);
        diagnostics.callback_table_complete =
            callbacks.pm_init != nullptr
            && callbacks.pm_move != nullptr
            && callbacks.cmd_start != nullptr
            && callbacks.cmd_end != nullptr
            && (!callbacks.combat_enabled
                || (callbacks.player_pre_think != nullptr
                    && callbacks.player_post_think != nullptr
                    && callbacks.global_time != nullptr
                    && callbacks.global_frametime != nullptr
                    && callbacks.active_attack_postthink_slot != nullptr))
            && ValidMovevars(config);
        if (callbacks.active_attack_postthink_slot != nullptr)
        {
            *callbacks.active_attack_postthink_slot = 0;
        }
        for (GoldSrcCombatClientDiagnostics& client : combat_clients)
        {
            client = {};
            client.phase = callbacks.combat_enabled
                ? network::GoldSrcCombatPhase::kAwaitingPlayer
                : network::GoldSrcCombatPhase::kDisabled;
        }
        last_command_was_weapon_attack.fill(false);
        if (diagnostics.callback_table_complete)
        {
            gameplay_fail_stop_latched = false;
        }
        return diagnostics.callback_table_complete;
    }

    bool EnsurePmInit() noexcept
    {
        if (diagnostics.pm_init_complete)
        {
            return true;
        }
        if (!diagnostics.callback_table_complete || !world.ready())
        {
            return false;
        }
        playermove_t context{};
        BindServices(&context);
        ActiveScope scope(this);
        ++diagnostics.pm_init_calls;
        diagnostics.pm_init_complete =
            SafePmInit(callbacks.pm_init, &context);
        return diagnostics.pm_init_complete;
    }

    void BindServices(playermove_t* context) noexcept
    {
        context->PM_Info_ValueForKey = &InfoValueForKey;
        context->PM_Particle = &Particle;
        context->PM_TestPlayerPosition = &TestPlayerPosition;
        context->Con_NPrintf = &ConNPrintf;
        context->Con_DPrintf = &ConDPrintf;
        context->Con_Printf = &ConPrintf;
        context->Sys_FloatTime = &FloatTime;
        context->PM_StuckTouch = &StuckTouch;
        context->PM_PointContents = &PointContents;
        context->PM_TruePointContents = &TruePointContents;
        context->PM_HullPointContents = &HullPointContents;
        context->PM_PlayerTrace = &PlayerTrace;
        context->PM_TraceLine = &TraceLine;
        context->RandomLong = &RandomLong;
        context->RandomFloat = &RandomFloat;
        context->PM_GetModelType = &GetModelType;
        context->PM_GetModelBounds = &GetModelBounds;
        context->PM_HullForBsp = &HullForBsp;
        context->PM_TraceModel = &TraceModel;
        context->COM_FileSize = &FileSize;
        context->COM_LoadFile = &LoadFile;
        context->COM_FreeFile = &FreeFile;
        context->memfgets = &MemoryGets;
        context->PM_PlaySound = &PlaySound;
        context->PM_TraceTexture = &TraceTexture;
        context->PM_PlaybackEventFull = &PlaybackEvent;
        context->PM_PlayerTraceEx = nullptr;
        context->PM_TestPlayerPositionEx = nullptr;
        context->PM_TraceLineEx = nullptr;
    }

    bool BuildContext(
        const edict_t* player,
        std::size_t client_slot,
        const usercmd_t& command,
        std::uint64_t command_time_msec,
        playermove_t* context) noexcept
    {
        if (player == nullptr || client_slot < 1u
            || client_slot > kMaximumClients
            || context == nullptr || !world.ready())
        {
            return false;
        }
        const entvars_t& vars = player->v;
        if (!FiniteVector(vars.origin)
            || !FiniteVector(vars.velocity)
            || !FiniteVector(vars.v_angle)
            || !FiniteVector(vars.view_ofs)
            || !FiniteVector(vars.punchangle)
            || !FiniteVector(vars.basevelocity))
        {
            return false;
        }

        *context = {};
        context->player_index = static_cast<int>(client_slot - 1u);
        context->server = TRUE;
        context->multiplayer =
            movevars_config.multiplayer ? TRUE : FALSE;
        context->time = static_cast<float>(command_time_msec);
        context->frametime =
            static_cast<float>(command.msec) / 1000.0f;
        CopyVector(vars.origin, context->origin);
        CopyVector(vars.v_angle, context->angles);
        CopyVector(vars.v_angle, context->oldangles);
        CopyVector(vars.velocity, context->velocity);
        CopyVector(vars.movedir, context->movedir);
        CopyVector(vars.basevelocity, context->basevelocity);
        CopyVector(vars.view_ofs, context->view_ofs);
        CopyVector(vars.punchangle, context->punchangle);
        context->flDuckTime = vars.flDuckTime;
        context->bInDuck = vars.bInDuck;
        context->flTimeStepSound = vars.flTimeStepSound;
        context->iStepLeft = vars.iStepLeft;
        context->flFallVelocity = vars.flFallVelocity;
        context->flSwimTime = vars.flSwimTime;
        context->effects = vars.effects;
        context->flags = vars.flags;
        context->usehull =
            (vars.flags & FL_DUCKING) != 0 ? 1 : 0;
        context->gravity =
            std::isfinite(vars.gravity) && vars.gravity > 0.0f
            ? vars.gravity
            : movevars_config.entity_gravity;
        context->friction =
            std::isfinite(vars.friction) && vars.friction > 0.0f
            ? vars.friction
            : 1.0f;
        context->oldbuttons = vars.oldbuttons;
        context->waterjumptime = vars.teleport_time;
        context->dead = vars.health <= 0.0f;
        context->deadflag = vars.deadflag;
        context->spectator = FALSE;
        context->movetype = vars.movetype;
        context->onground =
            (vars.flags & FL_ONGROUND) != 0 ? 0 : -1;
        context->waterlevel = vars.waterlevel;
        context->watertype = vars.watertype;
        context->maxspeed = movevars_config.maximum_speed;
        context->clientmaxspeed =
            std::isfinite(vars.maxspeed) && vars.maxspeed > 0.0f
            ? vars.maxspeed
            : movevars_config.maximum_speed;
        context->iuser1 = vars.iuser1;
        context->iuser2 = vars.iuser2;
        context->iuser3 = vars.iuser3;
        context->iuser4 = vars.iuser4;
        context->fuser1 = vars.fuser1;
        context->fuser2 = vars.fuser2;
        context->fuser3 = vars.fuser3;
        context->fuser4 = vars.fuser4;
        CopyVector(vars.vuser1, context->vuser1);
        CopyVector(vars.vuser2, context->vuser2);
        CopyVector(vars.vuser3, context->vuser3);
        CopyVector(vars.vuser4, context->vuser4);
        context->cmd = command;
        context->movevars = &movevars;
        for (std::size_t hull = 0; hull < kPlayerMins.size(); ++hull)
        {
            for (int axis = 0; axis < 3; ++axis)
            {
                context->player_mins[hull][axis] =
                    kPlayerMins[hull][axis];
                context->player_maxs[hull][axis] =
                    kPlayerMaxs[hull][axis];
            }
        }
        context->numphysent = 1;
        std::snprintf(
            context->physents[0].name,
            sizeof(context->physents[0].name),
            "%s",
            "world");
        context->physents[0].model = world.model();
        context->physents[0].info = 0;
        context->physents[0].solid = SOLID_BSP;
        context->physents[0].movetype = MOVETYPE_NONE;
        context->numvisent = 1;
        context->visents[0] = context->physents[0];
        context->nummoveent = 0;
        context->numtouch = 0;
        context->runfuncs = TRUE;
        BindServices(context);
        ++diagnostics.contexts_built;
        return true;
    }

    std::string_view ValidateOutput(const playermove_t& context) noexcept
    {
        if (!FiniteVector(context.origin)
            || !FiniteVector(context.velocity)
            || !FiniteVector(context.angles)
            || !FiniteVector(context.view_ofs)
            || !FiniteVector(context.punchangle)
            || !FiniteVector(context.basevelocity))
        {
            return "non_finite_movement_state";
        }
        for (int axis = 0; axis < 3; ++axis)
        {
            if (std::fabs(context.origin[axis]) > kMaximumCoordinate)
            {
                return "coordinate_limit";
            }
            if (std::fabs(context.velocity[axis])
                > movevars_config.maximum_velocity)
            {
                return "velocity_limit";
            }
        }
        if (context.movetype < MOVETYPE_NONE
            || context.movetype > MOVETYPE_FOLLOW)
        {
            return "movetype_range";
        }
        if (context.usehull < 0 || context.usehull > 2)
        {
            return "hull_range";
        }
        if (context.waterlevel < 0 || context.waterlevel > 3)
        {
            return "waterlevel_range";
        }
        if (context.numphysent != 1)
        {
            return "physent_count";
        }
        if (context.onground < -1
            || context.onground >= context.numphysent)
        {
            return "ground_entity_range";
        }
        if (context.numtouch < 0
            || context.numtouch > MAX_PHYSENTS)
        {
            return "touch_count";
        }
        for (int index = 0; index < context.numtouch; ++index)
        {
            if (context.touchindex[index].ent < 0
                || context.touchindex[index].ent >= context.numphysent)
            {
                return "touch_entity_range";
            }
        }
        // PM_Move has already resolved the swept player hull.  A zero-length
        // follow-up hull trace can report allsolid for a valid position that
        // is exactly flush with a BSP boundary because it lacks PM_Move's
        // impact-plane context.  Keep the finite/range/index validation above
        // authoritative and let the swept trace contract guard penetration.
        return "ok";
    }

    void Commit(
        edict_t* player,
        edict_t* world_edict,
        const playermove_t& context,
        bool* state_changed) noexcept
    {
        entvars_t& vars = player->v;
        const bool origin_changed =
            std::memcmp(vars.origin, context.origin, sizeof(vec3_t)) != 0;
        const bool velocity_changed =
            std::memcmp(vars.velocity, context.velocity, sizeof(vec3_t)) != 0;
        const bool ground_changed =
            ((vars.flags & FL_ONGROUND) != 0)
            != (context.onground != -1);
        const bool hull_changed =
            ((vars.flags & FL_DUCKING) != 0)
            != (context.usehull == 1);

        CopyVector(context.origin, vars.origin);
        CopyVector(context.velocity, vars.velocity);
        CopyVector(context.basevelocity, vars.basevelocity);
        CopyVector(context.angles, vars.v_angle);
        vars.angles[0] = -context.angles[0] / 3.0f;
        vars.angles[1] = context.angles[1];
        vars.angles[2] = context.angles[2];
        CopyVector(context.view_ofs, vars.view_ofs);
        CopyVector(context.punchangle, vars.punchangle);
        CopyVector(context.movedir, vars.movedir);
        vars.flags = context.flags;
        vars.movetype = context.movetype;
        vars.waterlevel = context.waterlevel;
        vars.watertype = context.watertype;
        vars.friction = context.friction;
        vars.maxspeed = context.clientmaxspeed;
        vars.bInDuck = context.bInDuck;
        vars.flDuckTime = context.flDuckTime;
        vars.flTimeStepSound = context.flTimeStepSound;
        vars.iStepLeft = context.iStepLeft;
        vars.flFallVelocity = context.flFallVelocity;
        vars.flSwimTime = context.flSwimTime;
        vars.teleport_time = context.waterjumptime;
        vars.oldbuttons = context.cmd.buttons;
        vars.groundentity =
            context.onground == 0 ? world_edict : nullptr;
        vars.iuser1 = context.iuser1;
        vars.iuser2 = context.iuser2;
        vars.iuser3 = context.iuser3;
        vars.iuser4 = context.iuser4;
        vars.fuser1 = context.fuser1;
        vars.fuser2 = context.fuser2;
        vars.fuser3 = context.fuser3;
        vars.fuser4 = context.fuser4;
        CopyVector(context.vuser1, vars.vuser1);
        CopyVector(context.vuser2, vars.vuser2);
        CopyVector(context.vuser3, vars.vuser3);
        CopyVector(context.vuser4, vars.vuser4);

        const std::size_t hull =
            context.usehull == 1 ? 1u : 0u;
        for (int axis = 0; axis < 3; ++axis)
        {
            vars.mins[axis] = kPlayerMins[hull][axis];
            vars.maxs[axis] = kPlayerMaxs[hull][axis];
            vars.size[axis] = vars.maxs[axis] - vars.mins[axis];
            vars.absmin[axis] = vars.origin[axis] + vars.mins[axis] - 1.0f;
            vars.absmax[axis] = vars.origin[axis] + vars.maxs[axis] + 1.0f;
        }
        ++diagnostics.movement_commits;
        diagnostics.player_origin_updates += origin_changed ? 1u : 0u;
        diagnostics.player_velocity_updates += velocity_changed ? 1u : 0u;
        diagnostics.player_ground_state_updates += ground_changed ? 1u : 0u;
        diagnostics.player_hull_updates += hull_changed ? 1u : 0u;
        if (state_changed != nullptr)
        {
            *state_changed = *state_changed
                || origin_changed || velocity_changed
                || ground_changed || hull_changed;
        }
    }

    static Impl* Current() noexcept
    {
        return active;
    }

    static void Count(std::uint64_t GoldSrcPmoveDiagnostics::*field) noexcept
    {
        if (Impl* self = Current())
        {
            ++self->diagnostics.service_callback_calls;
            ++(self->diagnostics.*field);
        }
    }

    static const char* InfoValueForKey(
        const char* info,
        const char* key)
    {
        if (Impl* self = Current())
        {
            ++self->diagnostics.service_callback_calls;
        }
        static thread_local std::array<char, 256> value{};
        value.fill('\0');
        if (info == nullptr || key == nullptr || *key == '\0')
        {
            return value.data();
        }
        const std::string source(info);
        const std::string wanted(key);
        std::size_t cursor = source.empty() ? 0u : 1u;
        while (cursor < source.size())
        {
            const std::size_t key_end = source.find('\\', cursor);
            if (key_end == std::string::npos)
            {
                break;
            }
            const std::size_t value_end =
                source.find('\\', key_end + 1u);
            const std::string_view candidate(
                source.data() + cursor,
                key_end - cursor);
            const std::size_t count =
                (value_end == std::string::npos ? source.size() : value_end)
                - key_end - 1u;
            if (candidate == wanted)
            {
                const std::size_t copy =
                    std::min(count, value.size() - 1u);
                std::memcpy(
                    value.data(),
                    source.data() + key_end + 1u,
                    copy);
                return value.data();
            }
            if (value_end == std::string::npos)
            {
                break;
            }
            cursor = value_end + 1u;
        }
        return value.data();
    }

    static void Particle(float*, int, float, int, int)
    {
        if (Impl* self = Current())
        {
            ++self->diagnostics.service_callback_calls;
        }
    }

    static int TestPlayerPosition(float* position, pmtrace_t* trace)
    {
        Count(&GoldSrcPmoveDiagnostics::test_position_calls);
        Impl* self = Current();
        if (self == nullptr)
        {
            return 0;
        }
        pmtrace_t result = self->world.Trace(
            position,
            position,
            CurrentUseHull(*self));
        if (trace != nullptr)
        {
            *trace = result;
        }
        return result.startsolid ? 0 : -1;
    }

    static void ConNPrintf(int, char*, ...)
    {
        if (Impl* self = Current())
        {
            ++self->diagnostics.service_callback_calls;
        }
    }
    static void ConDPrintf(char*, ...)
    {
        if (Impl* self = Current())
        {
            ++self->diagnostics.service_callback_calls;
        }
    }
    static void ConPrintf(char*, ...)
    {
        if (Impl* self = Current())
        {
            ++self->diagnostics.service_callback_calls;
        }
    }

    static double FloatTime()
    {
        if (Impl* self = Current())
        {
            ++self->diagnostics.service_callback_calls;
            return self->current_time_seconds;
        }
        return 0.0;
    }

    static void StuckTouch(int, pmtrace_t*)
    {
        if (Impl* self = Current())
        {
            ++self->diagnostics.service_callback_calls;
        }
    }

    static int PointContents(float* point, int* true_contents)
    {
        Count(&GoldSrcPmoveDiagnostics::point_contents_calls);
        Impl* self = Current();
        const int result =
            self != nullptr ? self->world.PointContents(point) : CONTENTS_SOLID;
        if (true_contents != nullptr)
        {
            *true_contents = result;
        }
        return result;
    }

    static int TruePointContents(float* point)
    {
        return PointContents(point, nullptr);
    }

    static int HullPointContents(
        hull_t* hull,
        int node,
        float* point)
    {
        Count(&GoldSrcPmoveDiagnostics::hull_contents_calls);
        Impl* self = Current();
        return self != nullptr
            ? self->world.HullPointContents(hull, node, point)
            : CONTENTS_SOLID;
    }

    static pmtrace_t PlayerTrace(
        float* start,
        float* end,
        int,
        int)
    {
        Count(&GoldSrcPmoveDiagnostics::player_trace_calls);
        Impl* self = Current();
        return self != nullptr
            ? self->world.Trace(start, end, CurrentUseHull(*self))
            : pmtrace_t{};
    }

    static pmtrace_t* TraceLine(
        float* start,
        float* end,
        int,
        int usehull,
        int)
    {
        Count(&GoldSrcPmoveDiagnostics::player_trace_calls);
        static thread_local pmtrace_t trace{};
        Impl* self = Current();
        trace = self != nullptr
            ? self->world.Trace(start, end, usehull)
            : pmtrace_t{};
        return &trace;
    }

    static int32 RandomLong(int32 low, int32 high)
    {
        Count(&GoldSrcPmoveDiagnostics::random_calls);
        Impl* self = Current();
        if (self == nullptr)
        {
            return low;
        }
        self->random_state =
            self->random_state * 1664525u + 1013904223u;
        const std::int64_t minimum = std::min<std::int64_t>(low, high);
        const std::int64_t maximum = std::max<std::int64_t>(low, high);
        const std::uint64_t span =
            static_cast<std::uint64_t>(maximum - minimum) + 1u;
        return static_cast<int32>(
            minimum + self->random_state % span);
    }

    static float RandomFloat(float low, float high)
    {
        Count(&GoldSrcPmoveDiagnostics::random_calls);
        Impl* self = Current();
        if (self == nullptr || low == high)
        {
            return low;
        }
        self->random_state =
            self->random_state * 1664525u + 1013904223u;
        const float fraction =
            static_cast<float>(self->random_state)
            / static_cast<float>(
                std::numeric_limits<std::uint32_t>::max());
        return std::min(low, high)
            + fraction * std::fabs(high - low);
    }

    static int GetModelType(model_t* model)
    {
        Count(&GoldSrcPmoveDiagnostics::model_service_calls);
        Impl* self = Current();
        return self != nullptr && model == self->world.model()
            ? mod_brush
            : mod_brush;
    }

    static void GetModelBounds(
        model_t* model,
        float* mins,
        float* maxs)
    {
        Count(&GoldSrcPmoveDiagnostics::model_service_calls);
        Impl* self = Current();
        if (self == nullptr || model != self->world.model()
            || mins == nullptr || maxs == nullptr)
        {
            return;
        }
        CopyVector(model->mins, mins);
        CopyVector(model->maxs, maxs);
    }

    static void* HullForBsp(physent_t* entity, float* offset)
    {
        Count(&GoldSrcPmoveDiagnostics::model_service_calls);
        Impl* self = Current();
        if (self == nullptr || entity == nullptr
            || entity->model != self->world.model())
        {
            return nullptr;
        }
        return self->world.HullForUseHull(CurrentUseHull(*self), offset);
    }

    static float TraceModel(
        physent_t* entity,
        float* start,
        float* end,
        trace_t* output)
    {
        Count(&GoldSrcPmoveDiagnostics::model_service_calls);
        Impl* self = Current();
        if (self == nullptr || entity == nullptr
            || entity->model != self->world.model())
        {
            return 1.0f;
        }
        const pmtrace_t trace =
            self->world.Trace(start, end, CurrentUseHull(*self));
        if (output != nullptr)
        {
            *output = {};
            output->allsolid = trace.allsolid;
            output->startsolid = trace.startsolid;
            output->inopen = trace.inopen;
            output->inwater = trace.inwater;
            output->fraction = trace.fraction;
            CopyVector(trace.endpos, output->endpos);
            CopyVector(trace.plane.normal, output->plane.normal);
            output->plane.dist = trace.plane.dist;
        }
        return trace.fraction;
    }

    static bool SafeMaterialPath(const char* path) noexcept
    {
        if (path == nullptr)
        {
            return false;
        }
        std::string normalized(path);
        std::replace(normalized.begin(), normalized.end(), '\\', '/');
        std::transform(
            normalized.begin(),
            normalized.end(),
            normalized.begin(),
            [](unsigned char character)
            {
                return static_cast<char>(std::tolower(character));
            });
        return normalized == "sound/materials.txt";
    }

    static int FileSize(char* path)
    {
        Count(&GoldSrcPmoveDiagnostics::file_service_calls);
        Impl* self = Current();
        if (self == nullptr || !SafeMaterialPath(path))
        {
            return -1;
        }
        std::ifstream stream(
            self->game_directory / "sound" / "materials.txt",
            std::ios::binary | std::ios::ate);
        if (!stream)
        {
            return -1;
        }
        const std::streamoff size = stream.tellg();
        return size >= 0
            && size <= std::numeric_limits<int>::max()
            ? static_cast<int>(size)
            : -1;
    }

    static byte* LoadFile(char* path, int, int* length)
    {
        Count(&GoldSrcPmoveDiagnostics::file_service_calls);
        if (length != nullptr)
        {
            *length = 0;
        }
        Impl* self = Current();
        const int size = FileSize(path);
        if (self == nullptr || size <= 0)
        {
            return nullptr;
        }
        std::ifstream stream(
            self->game_directory / "sound" / "materials.txt",
            std::ios::binary);
        if (!stream)
        {
            return nullptr;
        }
        std::vector<byte> bytes(static_cast<std::size_t>(size));
        stream.read(
            reinterpret_cast<char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
        if (!stream)
        {
            return nullptr;
        }
        auto allocation = std::make_unique<byte[]>(bytes.size());
        std::memcpy(allocation.get(), bytes.data(), bytes.size());
        byte* result = allocation.get();
        self->files.emplace(result, std::move(allocation));
        if (length != nullptr)
        {
            *length = size;
        }
        return result;
    }

    static void FreeFile(void* buffer)
    {
        Count(&GoldSrcPmoveDiagnostics::file_service_calls);
        if (Impl* self = Current())
        {
            self->files.erase(buffer);
        }
    }

    static char* MemoryGets(
        byte* memory,
        int file_size,
        int* file_position,
        char* output,
        int output_size)
    {
        if (memory == nullptr || file_position == nullptr
            || output == nullptr || output_size <= 1
            || file_size < 0 || *file_position < 0
            || *file_position >= file_size)
        {
            return nullptr;
        }
        int written = 0;
        while (*file_position < file_size
            && written < output_size - 1)
        {
            const char value =
                static_cast<char>(memory[(*file_position)++]);
            output[written++] = value;
            if (value == '\n')
            {
                break;
            }
        }
        output[written] = '\0';
        return output;
    }

    static void PlaySound(
        int,
        const char*,
        float,
        float,
        int,
        int)
    {
        if (Impl* self = Current())
        {
            ++self->diagnostics.service_callback_calls;
        }
    }

    static const char* TraceTexture(int, float*, float*)
    {
        if (Impl* self = Current())
        {
            ++self->diagnostics.service_callback_calls;
        }
        return "CONCRETE";
    }

    static void PlaybackEvent(
        int,
        int,
        unsigned short,
        float,
        float*,
        float*,
        float,
        float,
        int,
        int,
        int,
        int)
    {
        if (Impl* self = Current())
        {
            ++self->diagnostics.service_callback_calls;
        }
    }

    BspCollisionWorld world;
    GoldSrcPmoveGameDllCallbacks callbacks{};
    GoldSrcMovevarsConfig movevars_config{};
    movevars_t movevars{};
    std::filesystem::path game_directory;
    std::array<network::GoldSrcCommandExecutionState, kMaximumClients>
        command_states{};
    std::array<GoldSrcCombatClientDiagnostics, kMaximumClients>
        combat_clients{};
    std::array<std::optional<std::uint64_t>, kMaximumClients>
        combat_time_bases_msec{};
    std::array<bool, kMaximumClients>
        last_command_was_weapon_attack{};
    bool gameplay_fail_stop_latched = false;
    GoldSrcPmoveDiagnostics diagnostics{};
    std::unordered_map<void*, std::unique_ptr<byte[]>> files;
    std::uint32_t random_state = 0x504D4F56u;
    double current_time_seconds = 0.0;
    static int CurrentUseHull(const Impl& self) noexcept
    {
        return self.current_context != nullptr
            ? self.current_context->usehull
            : 0;
    }
    playermove_t* current_context = nullptr;
};

thread_local GoldSrcPmoveRuntime::Impl*
    GoldSrcPmoveRuntime::Impl::active = nullptr;

std::string_view ReasonFor(GoldSrcPmoveExecutionStatus status) noexcept
{
    switch (status)
    {
    case GoldSrcPmoveExecutionStatus::kOk:
        return "ok";
    case GoldSrcPmoveExecutionStatus::kInvalidClient:
        return "invalid_client";
    case GoldSrcPmoveExecutionStatus::kNotReady:
        return "movement_not_ready";
    case GoldSrcPmoveExecutionStatus::kWorldUnavailable:
        return "world_collision_unavailable";
    case GoldSrcPmoveExecutionStatus::kCallbackMissing:
        return "game_dll_pmove_callback_missing";
    case GoldSrcPmoveExecutionStatus::kPmInitFailed:
        return "pm_init_failed";
    case GoldSrcPmoveExecutionStatus::kCommandPlanRejected:
        return "command_plan_rejected";
    case GoldSrcPmoveExecutionStatus::kContextInvalid:
        return "pmove_context_invalid";
    case GoldSrcPmoveExecutionStatus::kPmMoveFailed:
        return "pmove_callback_failed";
    case GoldSrcPmoveExecutionStatus::kOutputInvalid:
        return "invalid_pmove_output";
    }
    return "movement_not_ready";
}

GoldSrcPmoveRuntime::GoldSrcPmoveRuntime()
    : impl_(std::make_unique<Impl>())
{
}

GoldSrcPmoveRuntime::~GoldSrcPmoveRuntime() = default;
GoldSrcPmoveRuntime::GoldSrcPmoveRuntime(GoldSrcPmoveRuntime&&) noexcept =
    default;
GoldSrcPmoveRuntime& GoldSrcPmoveRuntime::operator=(
    GoldSrcPmoveRuntime&&) noexcept = default;

bool GoldSrcPmoveRuntime::InitializeWorld(
    const WorldModelContext& world)
{
    return impl_->InitializeWorld(world);
}

bool GoldSrcPmoveRuntime::InitializeGameDll(
    const GoldSrcPmoveGameDllCallbacks& callbacks,
    const std::filesystem::path& game_directory,
    const GoldSrcMovevarsConfig& movevars)
{
    return impl_->InitializeGameDll(
        callbacks,
        game_directory,
        movevars);
}

bool GoldSrcPmoveRuntime::IsPlayerPositionValid(
    const float* origin,
    int use_hull) const noexcept
{
    if (origin == nullptr || use_hull < 0 || use_hull > 1
        || !impl_->world.ready())
    {
        return false;
    }
    const pmtrace_t trace = impl_->world.Trace(origin, origin, use_hull);
    if (trace.startsolid == FALSE && trace.allsolid == FALSE)
    {
        return true;
    }
    float raised[3]{origin[0], origin[1], origin[2] + 1.0f};
    const pmtrace_t raised_trace = impl_->world.Trace(
        raised,
        raised,
        use_hull);
    return raised_trace.startsolid == FALSE
        && raised_trace.allsolid == FALSE;
}

bool GoldSrcPmoveRuntime::IsWorldPointOpen(const float* point) const noexcept
{
    return point != nullptr && FiniteVector(point) && impl_->world.ready()
        && impl_->world.PointContents(point) != CONTENTS_SOLID;
}

bool GoldSrcPmoveRuntime::TraceWorldLine(
    const float* start,
    const float* end,
    GoldSrcWorldLineTrace* output) const noexcept
{
    return TraceWorldHull(start, end, 2, output);
}

bool GoldSrcPmoveRuntime::TraceWorldHull(
    const float* start,
    const float* end,
    int use_hull,
    GoldSrcWorldLineTrace* output) const noexcept
{
    if (start == nullptr || end == nullptr || output == nullptr
        || !FiniteVector(start) || !FiniteVector(end)
        || use_hull < 0 || use_hull > 2 || !impl_->world.ready())
    {
        return false;
    }
    pmtrace_t trace{};
    if (!SafeWorldTrace(
            &impl_->world,
            start,
            end,
            use_hull,
            &trace))
    {
        *output = {};
        output->fraction = 1.0f;
        CopyVector(end, output->end_position);
        return false;
    }
    *output = {};
    output->fraction = (std::max)(
        0.0f,
        (std::min)(trace.fraction, 1.0f));
    CopyVector(trace.endpos, output->end_position);
    CopyVector(trace.plane.normal, output->plane_normal);
    output->plane_distance = trace.plane.dist;
    output->all_solid = trace.allsolid != FALSE;
    output->start_solid = trace.startsolid != FALSE;
    output->hit_world = output->fraction < 1.0f
        || output->all_solid || output->start_solid;
    return true;
}

void GoldSrcPmoveRuntime::SetCombatPlayerReady(
    std::size_t client_slot,
    bool player_ready,
    int active_weapon_id,
    bool glock_state_ready) noexcept
{
    if (client_slot < 1u || client_slot > impl_->combat_clients.size())
    {
        return;
    }
    GoldSrcCombatClientDiagnostics& client =
        impl_->combat_clients[client_slot - 1u];
    if (!impl_->callbacks.combat_enabled)
    {
        client.phase = network::GoldSrcCombatPhase::kDisabled;
    }
    else if (!player_ready)
    {
        client.phase = network::GoldSrcCombatPhase::kAwaitingPlayer;
    }
    else if (active_weapon_id != network::kGoldSrcStockGlockWeaponId
        || !glock_state_ready)
    {
        client.phase = network::GoldSrcCombatPhase::kAwaitingWeaponState;
    }
    else if (client.phase != network::GoldSrcCombatPhase::kDamageObserved)
    {
        client.phase = network::GoldSrcCombatPhase::kCombatReady;
    }
}

const GoldSrcCombatClientDiagnostics*
GoldSrcPmoveRuntime::CombatDiagnostics(
    std::size_t client_slot) const noexcept
{
    return client_slot >= 1u && client_slot <= impl_->combat_clients.size()
        ? &impl_->combat_clients[client_slot - 1u]
        : nullptr;
}

void GoldSrcPmoveRuntime::ResetClient(
    std::size_t client_slot) noexcept
{
    if (client_slot >= 1u && client_slot <= impl_->command_states.size())
    {
        impl_->command_states[client_slot - 1u].Reset();
        impl_->combat_clients[client_slot - 1u] = {};
        impl_->combat_clients[client_slot - 1u].phase =
            impl_->callbacks.combat_enabled
                ? network::GoldSrcCombatPhase::kAwaitingPlayer
                : network::GoldSrcCombatPhase::kDisabled;
        impl_->combat_time_bases_msec[client_slot - 1u].reset();
        impl_->last_command_was_weapon_attack[client_slot - 1u] = false;
    }
}

void GoldSrcPmoveRuntime::ResetGameDll() noexcept
{
    if (impl_->callbacks.active_attack_postthink_slot != nullptr)
    {
        *impl_->callbacks.active_attack_postthink_slot = 0;
    }
    impl_->diagnostics.pm_init_complete = false;
    impl_->files.clear();
    for (auto& state : impl_->command_states)
    {
        state.Reset();
    }
    for (std::size_t index = 0; index < impl_->combat_clients.size(); ++index)
    {
        impl_->combat_clients[index] = {};
        impl_->combat_clients[index].phase = network::GoldSrcCombatPhase::kDisabled;
        impl_->combat_time_bases_msec[index].reset();
        impl_->last_command_was_weapon_attack[index] = false;
    }
}

GoldSrcPmoveExecutionResult GoldSrcPmoveRuntime::Execute(
    std::size_t client_slot,
    const network::GoldSrcDecodedMoveCommand& move,
    std::uint32_t packet_sequence,
    std::uint64_t host_time_msec,
    edict_s* player,
    edict_s* world) noexcept
{
    GoldSrcPmoveExecutionResult result;
    if (client_slot < 1u
        || client_slot > impl_->command_states.size()
        || player == nullptr || world == nullptr)
    {
        result.status = GoldSrcPmoveExecutionStatus::kInvalidClient;
        return result;
    }
    if (impl_->gameplay_fail_stop_latched)
    {
        result.status = GoldSrcPmoveExecutionStatus::kPmMoveFailed;
        result.gameplay_failure_stage = "fail_stop_latched";
        result.gameplay_callback_failure = true;
        result.gameplay_fail_stop = true;
        return result;
    }
    if (!impl_->world.ready())
    {
        result.status = GoldSrcPmoveExecutionStatus::kWorldUnavailable;
        return result;
    }
    if (!impl_->diagnostics.callback_table_complete)
    {
        result.status = GoldSrcPmoveExecutionStatus::kCallbackMissing;
        return result;
    }
    if (!impl_->EnsurePmInit())
    {
        result.status = GoldSrcPmoveExecutionStatus::kPmInitFailed;
        return result;
    }

    network::GoldSrcCommandExecutionState& state =
        impl_->command_states[client_slot - 1u];
    const network::GoldSrcCommandExecutionPlan plan =
        state.Plan(move, packet_sequence, host_time_msec);
    result.command_status = plan.status;
    result.duplicates_suppressed =
        plan.duplicate_backups_suppressed;
    result.fresh_commands = plan.new_commands;
    result.backup_commands = move.backup_command_count;
    result.recovered_commands = plan.recovered_backups;
    result.synthetic_replays = plan.replayed_last_commands;
    result.raw_netchan_sequence_distance =
        plan.raw_netchan_sequence_distance;
    result.packet_command_msec = plan.total_command_msec;
    result.host_elapsed_msec = plan.host_elapsed_msec;
    result.command_elapsed_msec = plan.command_elapsed_msec;
    result.proposed_command_elapsed_msec =
        plan.proposed_command_elapsed_msec;
    if (plan.status
        == network::GoldSrcCommandPlanStatus::kTemporarilyAhead)
    {
        state.CommitObservedMovePacket(
            packet_sequence,
            true,
            true);
        result.last_observed_move_sequence =
            state.last_observed_packet_sequence();
        result.last_validated_move_sequence =
            state.last_validated_move_sequence();
        result.last_executed_move_sequence =
            state.last_executed_move_sequence();
    }
    if (!plan.ok())
    {
        result.status = GoldSrcPmoveExecutionStatus::kCommandPlanRejected;
        return result;
    }

    const network::GoldSrcCommandExecutionState state_before_execution = state;
    const entvars_t authoritative_before = player->v;
    GoldSrcCombatClientDiagnostics& combat =
        impl_->combat_clients[client_slot - 1u];
    std::optional<std::uint64_t>& combat_time_base =
        impl_->combat_time_bases_msec[client_slot - 1u];
    const std::optional<std::uint64_t> combat_time_base_before_execution =
        combat_time_base;
    const std::uint64_t combat_gameplay_time_before_execution =
        combat.gameplay_time_msec;
    const network::GoldSrcCombatPhase combat_phase_before_execution =
        combat.phase;
    const auto restore_combat_execution_state = [&]() noexcept
    {
        combat_time_base = combat_time_base_before_execution;
        combat.gameplay_time_msec = combat_gameplay_time_before_execution;
        combat.phase = combat_phase_before_execution;
    };
    bool gameplay_callbacks_started = false;
    if (impl_->callbacks.combat_enabled
        && impl_->last_command_was_weapon_attack[client_slot - 1u]
        && plan.suppressed_backup_matched_last_command)
    {
        ++combat.duplicate_attack_commands_suppressed;
    }
    std::uint64_t pending_time_msec = state.command_time_msec();
    std::size_t local_commits = 0u;
    bool last_planned_command_was_weapon_attack = false;
    const auto finish_failed_execution =
        [&](GoldSrcPmoveExecutionStatus status) noexcept
    {
        result.status = status;
        if (gameplay_callbacks_started)
        {
            // CmdStart and later Game-DLL callbacks can mutate private weapon
            // data or other entities. Keep those side effects coherent, consume
            // this packet once, and prevent every client from entering gameplay
            // callbacks again until the Game DLL is reinitialized.
            result.gameplay_callback_failure = true;
            result.gameplay_fail_stop = true;
            impl_->gameplay_fail_stop_latched = true;
            state.CommitObservedMovePacket(packet_sequence, true);
            result.last_observed_move_sequence =
                state.last_observed_packet_sequence();
            result.last_validated_move_sequence =
                state.last_validated_move_sequence();
            result.last_executed_move_sequence =
                state.last_executed_move_sequence();
            return;
        }

        result.recoverable_rollback = true;
        restore_combat_execution_state();
        player->v = authoritative_before;
        impl_->diagnostics.movement_commits -= local_commits;
        state = state_before_execution;
        state.RecordRollback();
        ++impl_->diagnostics.movement_rollbacks;
    };
    for (std::size_t command_index = 0;
         command_index < plan.command_count;
         ++command_index)
    {
        const network::GoldSrcPlannedUserCommand& planned =
            plan.commands[command_index];
        network::GoldSrcCombatInput initial_combat_input =
            network::FilterGoldSrcCombatInput(
                planned.command.buttons,
                impl_->callbacks.combat_enabled,
                combat.phase);
        const bool initial_respawn_input =
            impl_->callbacks.combat_enabled
            && player->v.deadflag != DEAD_NO
            && (planned.command.buttons & IN_ATTACK) != 0u;
        if (initial_respawn_input)
        {
            // IN_ATTACK is also the stock multiplayer respawn command.  A
            // dead player has no active Glock, so the combat-readiness filter
            // would otherwise mask the click forever.  Forward the button to
            // PlayerDeathThink without treating it as a weapon attack.
            const bool attack_was_masked =
                (initial_combat_input.buttons & IN_ATTACK) == 0u;
            initial_combat_input.buttons |= IN_ATTACK;
            initial_combat_input.attack_received = false;
            initial_combat_input.attack_enabled = false;
            if (attack_was_masked
                && initial_combat_input.unsupported_bits_masked > 0u)
            {
                --initial_combat_input.unsupported_bits_masked;
            }
            ++combat.respawn_inputs_forwarded;
        }
        combat.attack_commands_received +=
            initial_combat_input.attack_received ? 1u : 0u;
        combat.unsupported_gameplay_inputs_masked +=
            initial_combat_input.unsupported_bits_masked;
        const network::GoldSrcPmoveSplitResult split =
            network::SplitGoldSrcPmoveCommand(planned.command.msec);
        if (!split.valid)
        {
            finish_failed_execution(
                GoldSrcPmoveExecutionStatus::kContextInvalid);
            return result;
        }

        bool respawn_command_consumed = false;
        bool command_was_weapon_attack = false;
        for (std::size_t piece_index = 0;
             piece_index < split.count;
             ++piece_index)
        {
            network::GoldSrcCombatInput piece_combat_input =
                network::FilterGoldSrcCombatInput(
                    planned.command.buttons,
                    impl_->callbacks.combat_enabled,
                    combat.phase);
            const bool piece_respawn_input =
                impl_->callbacks.combat_enabled
                && player->v.deadflag != DEAD_NO
                && !respawn_command_consumed
                && (planned.command.buttons & IN_ATTACK) != 0u;
            if (piece_respawn_input)
            {
                piece_combat_input.buttons |= IN_ATTACK;
                piece_combat_input.attack_received = false;
                piece_combat_input.attack_enabled = false;
            }
            else if (respawn_command_consumed)
            {
                piece_combat_input.buttons = static_cast<std::uint16_t>(
                    piece_combat_input.buttons & ~IN_ATTACK);
                piece_combat_input.attack_received = false;
                piece_combat_input.attack_enabled = false;
            }
            usercmd_t command =
                ConvertCommand(
                    planned.command,
                    split.msec[piece_index],
                    piece_combat_input.buttons);
            const bool player_was_dead =
                impl_->callbacks.combat_enabled
                && player->v.deadflag != DEAD_NO;
            gameplay_callbacks_started = gameplay_callbacks_started
                || impl_->callbacks.combat_enabled;
            if (!SafeCmdStart(
                    impl_->callbacks.cmd_start,
                    player,
                    &command,
                    packet_sequence
                        - static_cast<std::uint32_t>(command_index)))
            {
                result.gameplay_failure_stage = "cmd_start";
                finish_failed_execution(
                    GoldSrcPmoveExecutionStatus::kPmMoveFailed);
                return result;
            }
            ++impl_->diagnostics.cmd_start_calls;

            pending_time_msec += command.msec;
            std::uint64_t movement_time_msec = pending_time_msec;
            const bool gameplay_callbacks =
                impl_->callbacks.combat_enabled;
            if (gameplay_callbacks)
            {
                const std::uint64_t gameplay_time_floor = (std::max)(
                    combat_time_base.value_or(0u),
                    (std::max)(combat.gameplay_time_msec, host_time_msec));
                const std::uint64_t command_msec = command.msec;
                const std::uint64_t gameplay_time = gameplay_time_floor
                        > std::numeric_limits<std::uint64_t>::max()
                            - command_msec
                    ? std::numeric_limits<std::uint64_t>::max()
                    : gameplay_time_floor + command_msec;
                combat_time_base = gameplay_time;
                combat.gameplay_time_msec = gameplay_time;
                movement_time_msec = gameplay_time;
                *impl_->callbacks.global_time =
                    static_cast<float>(gameplay_time) / 1000.0f;
                *impl_->callbacks.global_frametime =
                    static_cast<float>(command.msec) / 1000.0f;
                player->v.button = command.buttons;
                player->v.impulse = command.impulse;
                player->v.light_level = command.lightlevel;
                CopyVector(command.viewangles, player->v.v_angle);
                if (piece_combat_input.attack_enabled)
                {
                    combat.phase =
                        network::GoldSrcCombatPhase::kAttackExecuting;
                }
                if (!SafePlayerThink(
                        impl_->callbacks.player_pre_think,
                        player))
                {
                    result.gameplay_failure_stage = "player_prethink";
                    const bool cmd_end_ok =
                        SafeCmdEnd(impl_->callbacks.cmd_end, player);
                    ++impl_->diagnostics.cmd_end_calls;
                    ++combat.callback_failures;
                    if (!cmd_end_ok)
                    {
                        ++combat.callback_failures;
                    }
                    finish_failed_execution(
                        GoldSrcPmoveExecutionStatus::kPmMoveFailed);
                    return result;
                }
                ++combat.player_prethink_calls;
                if (impl_->callbacks.entity_think != nullptr
                    && player->v.nextthink > 0.0f
                    && player->v.nextthink
                        <= *impl_->callbacks.global_time)
                {
                    player->v.nextthink = 0.0f;
                    if (!SafePlayerThink(
                            impl_->callbacks.entity_think,
                            player))
                    {
                        result.gameplay_failure_stage = "entity_think";
                        const bool cmd_end_ok =
                            SafeCmdEnd(impl_->callbacks.cmd_end, player);
                        ++impl_->diagnostics.cmd_end_calls;
                        ++combat.callback_failures;
                        if (!cmd_end_ok)
                        {
                            ++combat.callback_failures;
                        }
                        finish_failed_execution(
                            GoldSrcPmoveExecutionStatus::kPmMoveFailed);
                        return result;
                    }
                    ++combat.entity_think_calls;
                }
                if (player_was_dead && player->v.deadflag == DEAD_NO)
                {
                    // PlayerDeathThink consumed this lifecycle click.  Do
                    // not let the same held IN_ATTACK bit become a Glock
                    // shot later in this split command after Spawn().
                    respawn_command_consumed = true;
                    command.buttons = static_cast<unsigned short>(
                        command.buttons & ~IN_ATTACK);
                    player->v.button &= ~IN_ATTACK;
                    combat.phase =
                        network::GoldSrcCombatPhase::kAwaitingWeaponState;
                }
                else if (!player_was_dead
                    && player->v.deadflag != DEAD_NO)
                {
                    // If PreThink/Think changes the lifecycle state, the
                    // command is no longer a live weapon execution.  Consume
                    // the bit before PM_Move and wait for fresh inventory.
                    respawn_command_consumed = true;
                    command.buttons = static_cast<unsigned short>(
                        command.buttons & ~IN_ATTACK);
                    player->v.button &= ~IN_ATTACK;
                    piece_combat_input.attack_enabled = false;
                    combat.phase =
                        network::GoldSrcCombatPhase::kAwaitingWeaponState;
                }
            }
            playermove_t context{};
            if (!impl_->BuildContext(
                    player,
                    client_slot,
                    command,
                    movement_time_msec,
                    &context))
            {
                result.gameplay_failure_stage = "context_build";
                const bool cmd_end_ok =
                    SafeCmdEnd(impl_->callbacks.cmd_end, player);
                ++impl_->diagnostics.cmd_end_calls;
                if (!cmd_end_ok && gameplay_callbacks)
                {
                    ++combat.callback_failures;
                }
                finish_failed_execution(
                    GoldSrcPmoveExecutionStatus::kContextInvalid);
                return result;
            }
            impl_->current_context = &context;
            impl_->current_time_seconds =
                static_cast<double>(movement_time_msec) / 1000.0;
            bool callback_ok = false;
            {
                Impl::ActiveScope scope(impl_.get());
                ++impl_->diagnostics.pm_move_calls;
                callback_ok =
                    SafePmMove(impl_->callbacks.pm_move, &context);
            }
            impl_->current_context = nullptr;
            result.output_validation_reason = callback_ok
                ? impl_->ValidateOutput(context)
                : "pm_move_callback_failed";
            const bool output_ok = callback_ok
                && result.output_validation_reason == "ok";
            bool changed = false;
            if (output_ok)
            {
                impl_->Commit(player, world, context, &changed);
                ++local_commits;
                result.authoritative_state_changed =
                    result.authoritative_state_changed || changed;
                result.grounded = context.onground != -1;
                result.ducked = context.usehull == 1;
            }
            bool postthink_ok = true;
            if (output_ok && gameplay_callbacks)
            {
                *impl_->callbacks.active_attack_postthink_slot =
                    piece_combat_input.attack_enabled
                        ? static_cast<int>(client_slot)
                        : 0;
                postthink_ok = SafePlayerThink(
                    impl_->callbacks.player_post_think,
                    player);
                *impl_->callbacks.active_attack_postthink_slot = 0;
                if (postthink_ok)
                {
                    ++combat.player_postthink_calls;
                    combat.attack_commands_executed +=
                        piece_combat_input.attack_enabled ? 1u : 0u;
                    command_was_weapon_attack =
                        command_was_weapon_attack
                        || piece_combat_input.attack_enabled;
                    if (piece_combat_input.attack_enabled)
                    {
                        combat.phase =
                            network::GoldSrcCombatPhase::kCombatStable;
                    }
                }
                else
                {
                    ++combat.callback_failures;
                }
            }
            const bool cmd_end_ok =
                SafeCmdEnd(impl_->callbacks.cmd_end, player);
            ++impl_->diagnostics.cmd_end_calls;
            if (!cmd_end_ok && gameplay_callbacks)
            {
                ++combat.callback_failures;
            }
            if (!output_ok || !postthink_ok || !cmd_end_ok)
            {
                if (!callback_ok)
                {
                    result.gameplay_failure_stage = "pm_move";
                }
                else if (!output_ok)
                {
                    result.gameplay_failure_stage = "output_validation";
                }
                else if (!postthink_ok)
                {
                    result.gameplay_failure_stage = "player_postthink";
                }
                else
                {
                    result.gameplay_failure_stage = "cmd_end";
                }
                finish_failed_execution(
                    callback_ok && postthink_ok && cmd_end_ok
                        ? GoldSrcPmoveExecutionStatus::kOutputInvalid
                        : GoldSrcPmoveExecutionStatus::kPmMoveFailed);
                return result;
            }
            if (gameplay_callbacks
                && !player_was_dead
                && player->v.deadflag != DEAD_NO)
            {
                // A live attack can become lethal during a split command.
                // Never reinterpret its remaining held pieces as an
                // immediate respawn click or another weapon execution.
                respawn_command_consumed = true;
                combat.phase =
                    network::GoldSrcCombatPhase::kAwaitingWeaponState;
            }
            ++result.subcommands_executed;
        }
        last_planned_command_was_weapon_attack =
            command_was_weapon_attack;
    }

    state.CommitObservedMovePacket(packet_sequence, true);
    const std::uint64_t recoveries_before =
        state.diagnostics().command_clock_recoveries;
    state.CommitExecutedBatch(plan, packet_sequence, host_time_msec);
    if (plan.command_count > 0u)
    {
        impl_->last_command_was_weapon_attack[client_slot - 1u] =
            last_planned_command_was_weapon_attack;
    }
    result.commands_executed = plan.command_count;
    result.backups_replayed = plan.recovered_backups;
    result.last_observed_move_sequence =
        state.last_observed_packet_sequence();
    result.last_validated_move_sequence =
        state.last_validated_move_sequence();
    result.last_executed_move_sequence =
        state.last_executed_move_sequence();
    result.clock_recovered =
        state.diagnostics().command_clock_recoveries
            > recoveries_before;
    result.status = GoldSrcPmoveExecutionStatus::kOk;
    return result;
}

const network::GoldSrcCommandExecutionState*
GoldSrcPmoveRuntime::CommandState(std::size_t client_slot) const noexcept
{
    if (client_slot < 1u
        || client_slot > impl_->command_states.size())
    {
        return nullptr;
    }
    return &impl_->command_states[client_slot - 1u];
}

const GoldSrcPmoveDiagnostics&
GoldSrcPmoveRuntime::diagnostics() const noexcept
{
    return impl_->diagnostics;
}

std::size_t
GoldSrcPmoveRuntime::implemented_service_callback_count() const noexcept
{
    return 25u;
}

bool GoldSrcPmoveRuntime::movement_ready(
    std::size_t client_slot) const noexcept
{
    const auto* state = CommandState(client_slot);
    return state != nullptr
        && state->phase()
            != network::GoldSrcMovementPhase::
                kPlayerSpawnedAwaitingMovement;
}

bool GoldSrcPmoveRuntime::movement_executed(
    std::size_t client_slot) const noexcept
{
    const auto* state = CommandState(client_slot);
    return state != nullptr
        && state->diagnostics().commands_executed > 0u;
}

void GoldSrcPmoveRuntime::RecordMovementSnapshot(
    std::size_t client_slot) noexcept
{
    if (movement_executed(client_slot))
    {
        ++impl_->diagnostics.movement_snapshots_sent;
    }
}
} // namespace hl::game_api::detail
