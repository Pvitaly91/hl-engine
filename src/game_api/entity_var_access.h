#pragma once

#include <functional>
#include <string>
#include <string_view>

#include "server_bootstrap.h"

#pragma warning(push, 0)
#include "extdll.h"
#pragma warning(pop)

namespace hl::game_api::detail
{
using EntityVarLogFn = std::function<void(std::string_view)>;

struct EntityVarSnapshot
{
    int edict_index = -1;
    bool valid = false;
    bool free_flag = true;
    bool in_use = false;
    bool removed = false;
    bool spawned = false;
    bool deferred = false;
    bool has_private_data = false;
    bool containing_entity_valid = false;
    float nextthink = 0.0f;
    float ltime = 0.0f;
    int flags = 0;
    int solid = 0;
    int movetype = 0;
    int effects = 0;
    float health = 0.0f;
    bool health_available = false;
    std::string target;
    std::string targetname;
    std::string message;
    std::string classname;
    Vector origin = Vector(0.0f, 0.0f, 0.0f);
    bool has_origin = false;
    Vector angles = Vector(0.0f, 0.0f, 0.0f);
    bool has_angles = false;
    Vector movedir = Vector(0.0f, 0.0f, 0.0f);
    bool has_movedir = false;
    Vector mins = Vector(0.0f, 0.0f, 0.0f);
    Vector maxs = Vector(0.0f, 0.0f, 0.0f);
    bool has_size = false;
    bool scheduled_for_think = false;
};

bool TryReadEntityVars(
    const EdictStore& store,
    const EngineStringPool& string_pool,
    const edict_t* entity,
    EntityVarSnapshot* snapshot,
    const EntityVarLogFn& log_warn);

bool TryWriteNextThink(
    EdictStore& store,
    edict_t* entity,
    float nextthink,
    const EntityVarLogFn& log_warn);

bool TryPrepareThinkDispatch(
    EdictStore& store,
    edict_t* entity,
    float current_time,
    const EntityVarLogFn& log_warn);

std::string BuildEntityVarSummary(const EntityVarSnapshot& snapshot);
} // namespace hl::game_api::detail
