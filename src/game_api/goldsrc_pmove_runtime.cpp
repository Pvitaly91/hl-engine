#include "game_api/goldsrc_pmove_runtime.h"

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
constexpr float kTraceEpsilon = 0.03125f;
constexpr float kMaximumCoordinate = 32768.0f;
constexpr std::uint16_t kMovementButtonMask =
    IN_JUMP | IN_DUCK | IN_FORWARD | IN_BACK | IN_LEFT | IN_RIGHT
    | IN_MOVELEFT | IN_MOVERIGHT | IN_RUN;

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
        const float* point) const noexcept
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
        return HullPointContents(&hull, hull.firstclipnode, point);
    }

    pmtrace_t Trace(
        const float* start,
        const float* end,
        int usehull) noexcept
    {
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
        if (!RecursiveTrace(
                *hull,
                hull->firstclipnode,
                0.0f,
                1.0f,
                local_start,
                local_end,
                &trace,
                0))
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

private:
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

    bool RecursiveTrace(
        const hull_t& hull,
        int node,
        float start_fraction,
        float end_fraction,
        const float* start,
        const float* end,
        pmtrace_t* trace,
        int depth) const noexcept
    {
        if (trace == nullptr || depth > kMaximumTraceDepth)
        {
            return false;
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
            return true;
        }
        if (node < hull.firstclipnode || node > hull.lastclipnode)
        {
            return false;
        }
        const dclipnode_t& clipnode = hull.clipnodes[node];
        if (clipnode.planenum < 0
            || clipnode.planenum >= static_cast<int>(planes_.size()))
        {
            return false;
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
                depth + 1);
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
                depth + 1);
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
        const float middle_fraction =
            start_fraction
            + (end_fraction - start_fraction) * fraction;
        float middle[3]{};
        for (int axis = 0; axis < 3; ++axis)
        {
            middle[axis] = start[axis] + fraction * (end[axis] - start[axis]);
        }
        if (!RecursiveTrace(
                hull,
                clipnode.children[side],
                start_fraction,
                middle_fraction,
                start,
                middle,
                trace,
                depth + 1))
        {
            return false;
        }
        if (HullPointContents(
                &hull,
                clipnode.children[side ^ 1],
                middle)
            != CONTENTS_SOLID)
        {
            return RecursiveTrace(
                hull,
                clipnode.children[side ^ 1],
                middle_fraction,
                end_fraction,
                middle,
                end,
                trace,
                depth + 1);
        }
        if (trace->allsolid)
        {
            return false;
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
        trace->fraction = middle_fraction;
        return true;
    }

    bool ready_ = false;
    std::vector<mplane_t> planes_;
    std::vector<dclipnode_t> clipnodes_;
    std::vector<dclipnode_t> point_nodes_;
    model_t model_{};
};

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

usercmd_t ConvertCommand(
    const network::GoldSrcDecodedUserCommand& input,
    std::uint8_t msec) noexcept
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
    output.buttons =
        static_cast<unsigned short>(input.buttons & kMovementButtonMask);
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
            && ValidMovevars(config);
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

void GoldSrcPmoveRuntime::ResetClient(
    std::size_t client_slot) noexcept
{
    if (client_slot >= 1u && client_slot <= impl_->command_states.size())
    {
        impl_->command_states[client_slot - 1u].Reset();
    }
}

void GoldSrcPmoveRuntime::ResetGameDll() noexcept
{
    impl_->diagnostics.pm_init_complete = false;
    impl_->files.clear();
    for (auto& state : impl_->command_states)
    {
        state.Reset();
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
    std::uint64_t pending_time_msec = state.command_time_msec();
    std::size_t local_commits = 0u;
    for (std::size_t command_index = 0;
         command_index < plan.command_count;
         ++command_index)
    {
        const network::GoldSrcPlannedUserCommand& planned =
            plan.commands[command_index];
        const network::GoldSrcPmoveSplitResult split =
            network::SplitGoldSrcPmoveCommand(planned.command.msec);
        if (!split.valid)
        {
            player->v = authoritative_before;
            state = state_before_execution;
            state.RecordRollback();
            ++impl_->diagnostics.movement_rollbacks;
            result.status = GoldSrcPmoveExecutionStatus::kContextInvalid;
            return result;
        }

        for (std::size_t piece_index = 0;
             piece_index < split.count;
             ++piece_index)
        {
            usercmd_t command =
                ConvertCommand(planned.command, split.msec[piece_index]);
            if (!SafeCmdStart(
                    impl_->callbacks.cmd_start,
                    player,
                    &command,
                    packet_sequence
                        - static_cast<std::uint32_t>(command_index)))
            {
                player->v = authoritative_before;
                state = state_before_execution;
                state.RecordRollback();
                ++impl_->diagnostics.movement_rollbacks;
                result.status = GoldSrcPmoveExecutionStatus::kPmMoveFailed;
                return result;
            }
            ++impl_->diagnostics.cmd_start_calls;

            pending_time_msec += command.msec;
            playermove_t context{};
            if (!impl_->BuildContext(
                    player,
                    client_slot,
                    command,
                    pending_time_msec,
                    &context))
            {
                SafeCmdEnd(impl_->callbacks.cmd_end, player);
                ++impl_->diagnostics.cmd_end_calls;
                player->v = authoritative_before;
                state = state_before_execution;
                state.RecordRollback();
                ++impl_->diagnostics.movement_rollbacks;
                result.status = GoldSrcPmoveExecutionStatus::kContextInvalid;
                return result;
            }
            impl_->current_context = &context;
            impl_->current_time_seconds =
                static_cast<double>(pending_time_msec) / 1000.0;
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
            const bool cmd_end_ok =
                SafeCmdEnd(impl_->callbacks.cmd_end, player);
            ++impl_->diagnostics.cmd_end_calls;
            if (!output_ok || !cmd_end_ok)
            {
                player->v = authoritative_before;
                impl_->diagnostics.movement_commits -= local_commits;
                state = state_before_execution;
                state.RecordRollback();
                ++impl_->diagnostics.movement_rollbacks;
                result.status = callback_ok
                    ? GoldSrcPmoveExecutionStatus::kOutputInvalid
                    : GoldSrcPmoveExecutionStatus::kPmMoveFailed;
                return result;
            }
            ++result.subcommands_executed;
        }
    }

    state.CommitObservedMovePacket(packet_sequence, true);
    const std::uint64_t recoveries_before =
        state.diagnostics().command_clock_recoveries;
    state.CommitExecutedBatch(plan, packet_sequence, host_time_msec);
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
