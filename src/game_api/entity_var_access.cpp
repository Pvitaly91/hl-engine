#include "entity_var_access.h"

#include <sstream>

namespace
{
std::string FormatVector(const Vector& value)
{
    std::ostringstream stream;
    stream << value.x << ' ' << value.y << ' ' << value.z;
    return stream.str();
}

void LogInvalid(
    const hl::game_api::detail::EntityVarLogFn& log_warn,
    std::string_view message)
{
    if (log_warn)
    {
        log_warn(message);
    }
}
} // namespace

namespace hl::game_api::detail
{
bool TryReadEntityVars(
    const EdictStore& store,
    const EngineStringPool& string_pool,
    const edict_t* entity,
    EntityVarSnapshot* snapshot,
    const EntityVarLogFn& log_warn)
{
    if (snapshot == nullptr)
    {
        LogInvalid(log_warn, "EntityVarAccess: snapshot output is null.");
        return false;
    }

    *snapshot = {};

    const int edict_index = store.IndexOf(entity);
    if (entity == nullptr || edict_index < 0)
    {
        LogInvalid(log_warn, "EntityVarAccess: entity pointer is null or outside the current edict store.");
        return false;
    }

    const EntityStateSnapshot store_snapshot = store.SnapshotOf(entity, string_pool);
    if (store_snapshot.index < 0)
    {
        LogInvalid(log_warn, "EntityVarAccess: failed to snapshot entity state.");
        return false;
    }

    snapshot->edict_index = edict_index;
    snapshot->valid = true;
    snapshot->free_flag = store_snapshot.free_flag;
    snapshot->in_use = store_snapshot.in_use;
    snapshot->removed = store_snapshot.removed;
    snapshot->spawned = store_snapshot.spawned;
    snapshot->deferred = store_snapshot.deferred;
    snapshot->has_private_data = store_snapshot.private_data_present;
    snapshot->containing_entity_valid = store_snapshot.containing_entity_valid;
    snapshot->nextthink = entity->v.nextthink;
    snapshot->ltime = entity->v.ltime;
    snapshot->flags = static_cast<int>(entity->v.flags);
    snapshot->solid = entity->v.solid;
    snapshot->movetype = entity->v.movetype;
    snapshot->effects = entity->v.effects;
    snapshot->health = entity->v.health;
    snapshot->health_available = true;
    snapshot->target = string_pool.Describe(entity->v.target);
    snapshot->targetname = !store_snapshot.targetname.empty()
        ? store_snapshot.targetname
        : string_pool.Describe(entity->v.targetname);
    snapshot->message = string_pool.Describe(entity->v.message);
    snapshot->classname = !store_snapshot.classname.empty()
        ? store_snapshot.classname
        : string_pool.Describe(entity->v.classname);
    snapshot->origin = entity->v.origin;
    snapshot->has_origin = store_snapshot.has_origin
        || entity->v.origin.x != 0.0f
        || entity->v.origin.y != 0.0f
        || entity->v.origin.z != 0.0f;
    snapshot->angles = entity->v.angles;
    snapshot->has_angles = store_snapshot.has_angles
        || entity->v.angles.x != 0.0f
        || entity->v.angles.y != 0.0f
        || entity->v.angles.z != 0.0f;
    snapshot->movedir = entity->v.movedir;
    snapshot->has_movedir =
        entity->v.movedir.x != 0.0f
        || entity->v.movedir.y != 0.0f
        || entity->v.movedir.z != 0.0f;
    snapshot->mins = entity->v.mins;
    snapshot->maxs = entity->v.maxs;
    snapshot->has_size = store_snapshot.has_size
        || entity->v.mins.x != 0.0f
        || entity->v.mins.y != 0.0f
        || entity->v.mins.z != 0.0f
        || entity->v.maxs.x != 0.0f
        || entity->v.maxs.y != 0.0f
        || entity->v.maxs.z != 0.0f;
    snapshot->scheduled_for_think = snapshot->nextthink > 0.0f;

    if (!snapshot->containing_entity_valid)
    {
        LogInvalid(
            log_warn,
            "EntityVarAccess: edict#" + std::to_string(edict_index)
                + " has an unexpected pContainingEntity pointer.");
    }

    return true;
}

bool TryWriteNextThink(
    EdictStore& store,
    edict_t* entity,
    float nextthink,
    const EntityVarLogFn& log_warn)
{
    const int edict_index = store.IndexOf(entity);
    if (entity == nullptr || edict_index < 0)
    {
        LogInvalid(log_warn, "EntityVarAccess: cannot write nextthink for an invalid entity.");
        return false;
    }

    entvars_t* const vars = store.VarsOf(entity);
    if (vars == nullptr)
    {
        LogInvalid(
            log_warn,
            "EntityVarAccess: cannot write nextthink because entvars are unavailable for edict#"
                + std::to_string(edict_index) + ".");
        return false;
    }

    vars->nextthink = nextthink;

    return true;
}

bool TryPrepareThinkDispatch(
    EdictStore& store,
    edict_t* entity,
    float current_time,
    const EntityVarLogFn& log_warn)
{
    const int edict_index = store.IndexOf(entity);
    if (entity == nullptr || edict_index < 0)
    {
        LogInvalid(log_warn, "EntityVarAccess: cannot prepare think dispatch for an invalid entity.");
        return false;
    }

    entvars_t* const vars = store.VarsOf(entity);
    if (vars == nullptr)
    {
        LogInvalid(
            log_warn,
            "EntityVarAccess: cannot prepare think dispatch because entvars are unavailable for edict#"
                + std::to_string(edict_index) + ".");
        return false;
    }

    // Match the server-side think contract closely enough for scheduled logic:
    // clear the consumed nextthink slot before dispatch and advance local time
    // so entities that reschedule off pev->ltime do not stick on stale values.
    vars->ltime = current_time;
    vars->nextthink = 0.0f;
    return true;
}

std::string BuildEntityVarSummary(const EntityVarSnapshot& snapshot)
{
    if (!snapshot.valid)
    {
        return "edict?<invalid entity vars>";
    }

    std::ostringstream stream;
    stream << "edict#" << snapshot.edict_index
           << " classname=" << (snapshot.classname.empty() ? "<empty>" : snapshot.classname)
           << " targetname=" << (snapshot.targetname.empty() ? "<empty>" : snapshot.targetname)
           << " target=" << (snapshot.target.empty() ? "<empty>" : snapshot.target)
           << " nextthink=" << snapshot.nextthink
           << " ltime=" << snapshot.ltime
           << " flags=" << snapshot.flags
           << " solid=" << snapshot.solid
           << " movetype=" << snapshot.movetype
           << " effects=" << snapshot.effects
           << " health=" << (snapshot.health_available ? std::to_string(snapshot.health) : std::string("<unavailable>"))
           << " private=" << (snapshot.has_private_data ? "yes" : "no")
           << " deferred=" << (snapshot.deferred ? "yes" : "no")
           << " movedir=" << (snapshot.has_movedir ? FormatVector(snapshot.movedir) : std::string("<none>"))
           << " size=" << (snapshot.has_size
                ? (FormatVector(snapshot.mins) + ".." + FormatVector(snapshot.maxs))
                : std::string("<none>"))
           << " scheduled=" << (snapshot.scheduled_for_think ? "yes" : "no");
    return stream.str();
}
} // namespace hl::game_api::detail
