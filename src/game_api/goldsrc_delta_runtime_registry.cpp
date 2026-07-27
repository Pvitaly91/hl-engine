#include "goldsrc_delta_runtime_registry.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>

#pragma warning(push, 0)
#include "extdll.h"
#include "entity_state.h"
#include "event_args.h"
#include "usercmd.h"
#include "weaponinfo.h"
#pragma warning(pop)

namespace hl::game_api::detail
{
namespace
{
using hl::network::GoldSrcDeltaFieldLayout;
using hl::network::GoldSrcDeltaLayoutRegistry;
using hl::network::GoldSrcDeltaTableLayout;

void AddField(
    GoldSrcDeltaTableLayout& table,
    std::string name,
    std::size_t offset)
{
    if (offset > std::numeric_limits<std::uint16_t>::max())
    {
        std::terminate();
    }
    table.fields.push_back({
        std::move(name),
        static_cast<std::uint16_t>(offset),
        1u,
    });
}

#define HL_ADD_SCALAR(table, type, member) \
    AddField((table), #member, offsetof(type, member))

#define HL_ADD_VECTOR(table, type, member) \
    AddField((table), #member "[0]", offsetof(type, member)); \
    AddField( \
        (table), \
        #member "[1]", \
        offsetof(type, member) + sizeof(float)); \
    AddField( \
        (table), \
        #member "[2]", \
        offsetof(type, member) + (2u * sizeof(float)))

#define HL_ADD_BYTE_ARRAY_2(table, type, member) \
    AddField((table), #member "[0]", offsetof(type, member)); \
    AddField((table), #member "[1]", offsetof(type, member) + sizeof(byte))

#define HL_ADD_BYTE_ARRAY_4(table, type, member) \
    HL_ADD_BYTE_ARRAY_2((table), type, member); \
    AddField((table), #member "[2]", offsetof(type, member) + (2u * sizeof(byte))); \
    AddField((table), #member "[3]", offsetof(type, member) + (3u * sizeof(byte)))

GoldSrcDeltaTableLayout BuildClientDataLayout()
{
    GoldSrcDeltaTableLayout table;
    table.name = "clientdata_t";
    table.structure_size = static_cast<std::uint32_t>(sizeof(clientdata_t));

    HL_ADD_SCALAR(table, clientdata_t, flTimeStepSound);
    HL_ADD_VECTOR(table, clientdata_t, origin);
    HL_ADD_VECTOR(table, clientdata_t, velocity);
    HL_ADD_SCALAR(table, clientdata_t, m_flNextAttack);
    HL_ADD_SCALAR(table, clientdata_t, ammo_nails);
    HL_ADD_SCALAR(table, clientdata_t, ammo_shells);
    HL_ADD_SCALAR(table, clientdata_t, ammo_cells);
    HL_ADD_SCALAR(table, clientdata_t, ammo_rockets);
    HL_ADD_SCALAR(table, clientdata_t, m_iId);
    HL_ADD_VECTOR(table, clientdata_t, punchangle);
    HL_ADD_SCALAR(table, clientdata_t, flags);
    HL_ADD_SCALAR(table, clientdata_t, weaponanim);
    HL_ADD_SCALAR(table, clientdata_t, health);
    HL_ADD_SCALAR(table, clientdata_t, maxspeed);
    HL_ADD_SCALAR(table, clientdata_t, flDuckTime);
    HL_ADD_VECTOR(table, clientdata_t, view_ofs);
    HL_ADD_SCALAR(table, clientdata_t, viewmodel);
    HL_ADD_SCALAR(table, clientdata_t, weapons);
    HL_ADD_SCALAR(table, clientdata_t, pushmsec);
    HL_ADD_SCALAR(table, clientdata_t, deadflag);
    HL_ADD_SCALAR(table, clientdata_t, fov);
    HL_ADD_SCALAR(table, clientdata_t, physinfo);
    HL_ADD_SCALAR(table, clientdata_t, bInDuck);
    HL_ADD_SCALAR(table, clientdata_t, flSwimTime);
    HL_ADD_SCALAR(table, clientdata_t, waterjumptime);
    HL_ADD_SCALAR(table, clientdata_t, waterlevel);
    HL_ADD_SCALAR(table, clientdata_t, iuser1);
    HL_ADD_SCALAR(table, clientdata_t, iuser2);
    HL_ADD_VECTOR(table, clientdata_t, vuser1);
    HL_ADD_VECTOR(table, clientdata_t, vuser2);
    HL_ADD_VECTOR(table, clientdata_t, vuser3);
    HL_ADD_VECTOR(table, clientdata_t, vuser4);
    HL_ADD_SCALAR(table, clientdata_t, fuser1);
    HL_ADD_SCALAR(table, clientdata_t, fuser2);
    HL_ADD_SCALAR(table, clientdata_t, fuser3);
    HL_ADD_SCALAR(table, clientdata_t, fuser4);
    return table;
}

GoldSrcDeltaTableLayout BuildEntityStateLayout(std::string name)
{
    GoldSrcDeltaTableLayout table;
    table.name = std::move(name);
    table.structure_size = static_cast<std::uint32_t>(sizeof(entity_state_t));

    HL_ADD_SCALAR(table, entity_state_t, animtime);
    HL_ADD_SCALAR(table, entity_state_t, frame);
    HL_ADD_VECTOR(table, entity_state_t, origin);
    HL_ADD_VECTOR(table, entity_state_t, angles);
    HL_ADD_SCALAR(table, entity_state_t, sequence);
    HL_ADD_SCALAR(table, entity_state_t, modelindex);
    HL_ADD_SCALAR(table, entity_state_t, movetype);
    HL_ADD_SCALAR(table, entity_state_t, solid);
    HL_ADD_VECTOR(table, entity_state_t, mins);
    HL_ADD_VECTOR(table, entity_state_t, maxs);
    HL_ADD_VECTOR(table, entity_state_t, endpos);
    HL_ADD_VECTOR(table, entity_state_t, startpos);
    HL_ADD_SCALAR(table, entity_state_t, impacttime);
    HL_ADD_SCALAR(table, entity_state_t, starttime);
    HL_ADD_SCALAR(table, entity_state_t, weaponmodel);
    HL_ADD_SCALAR(table, entity_state_t, owner);
    HL_ADD_SCALAR(table, entity_state_t, effects);
    HL_ADD_SCALAR(table, entity_state_t, eflags);
    HL_ADD_SCALAR(table, entity_state_t, colormap);
    HL_ADD_SCALAR(table, entity_state_t, framerate);
    HL_ADD_SCALAR(table, entity_state_t, skin);
    HL_ADD_BYTE_ARRAY_4(table, entity_state_t, controller);
    HL_ADD_BYTE_ARRAY_2(table, entity_state_t, blending);
    HL_ADD_SCALAR(table, entity_state_t, body);
    HL_ADD_SCALAR(table, entity_state_t, rendermode);
    HL_ADD_SCALAR(table, entity_state_t, renderamt);
    HL_ADD_SCALAR(table, entity_state_t, renderfx);
    HL_ADD_SCALAR(table, entity_state_t, scale);
    AddField(
        table,
        "rendercolor.r",
        offsetof(entity_state_t, rendercolor));
    AddField(
        table,
        "rendercolor.g",
        offsetof(entity_state_t, rendercolor) + sizeof(byte));
    AddField(
        table,
        "rendercolor.b",
        offsetof(entity_state_t, rendercolor) + (2u * sizeof(byte)));
    HL_ADD_SCALAR(table, entity_state_t, aiment);
    HL_ADD_VECTOR(table, entity_state_t, basevelocity);
    HL_ADD_SCALAR(table, entity_state_t, playerclass);
    HL_ADD_SCALAR(table, entity_state_t, gaitsequence);
    HL_ADD_SCALAR(table, entity_state_t, team);
    HL_ADD_SCALAR(table, entity_state_t, friction);
    HL_ADD_SCALAR(table, entity_state_t, usehull);
    HL_ADD_SCALAR(table, entity_state_t, gravity);
    HL_ADD_SCALAR(table, entity_state_t, spectator);
    return table;
}

GoldSrcDeltaTableLayout BuildUserCommandLayout()
{
    GoldSrcDeltaTableLayout table;
    table.name = "usercmd_t";
    table.structure_size = static_cast<std::uint32_t>(sizeof(usercmd_t));
    HL_ADD_SCALAR(table, usercmd_t, lerp_msec);
    HL_ADD_SCALAR(table, usercmd_t, msec);
    HL_ADD_VECTOR(table, usercmd_t, viewangles);
    HL_ADD_SCALAR(table, usercmd_t, buttons);
    HL_ADD_SCALAR(table, usercmd_t, forwardmove);
    HL_ADD_SCALAR(table, usercmd_t, lightlevel);
    HL_ADD_SCALAR(table, usercmd_t, sidemove);
    HL_ADD_SCALAR(table, usercmd_t, upmove);
    HL_ADD_SCALAR(table, usercmd_t, impulse);
    HL_ADD_SCALAR(table, usercmd_t, impact_index);
    HL_ADD_VECTOR(table, usercmd_t, impact_position);
    return table;
}

GoldSrcDeltaTableLayout BuildWeaponDataLayout()
{
    GoldSrcDeltaTableLayout table;
    table.name = "weapon_data_t";
    table.structure_size = static_cast<std::uint32_t>(sizeof(weapon_data_t));
    HL_ADD_SCALAR(table, weapon_data_t, m_flTimeWeaponIdle);
    HL_ADD_SCALAR(table, weapon_data_t, m_flNextPrimaryAttack);
    HL_ADD_SCALAR(table, weapon_data_t, m_flNextReload);
    HL_ADD_SCALAR(table, weapon_data_t, m_fNextAimBonus);
    HL_ADD_SCALAR(table, weapon_data_t, m_flNextSecondaryAttack);
    HL_ADD_SCALAR(table, weapon_data_t, m_iClip);
    HL_ADD_SCALAR(table, weapon_data_t, m_flPumpTime);
    HL_ADD_SCALAR(table, weapon_data_t, m_fInSpecialReload);
    HL_ADD_SCALAR(table, weapon_data_t, m_fReloadTime);
    HL_ADD_SCALAR(table, weapon_data_t, m_fInReload);
    HL_ADD_SCALAR(table, weapon_data_t, m_fAimedDamage);
    HL_ADD_SCALAR(table, weapon_data_t, m_fInZoom);
    HL_ADD_SCALAR(table, weapon_data_t, m_iWeaponState);
    HL_ADD_SCALAR(table, weapon_data_t, m_iId);
    HL_ADD_SCALAR(table, weapon_data_t, iuser1);
    HL_ADD_SCALAR(table, weapon_data_t, iuser2);
    HL_ADD_SCALAR(table, weapon_data_t, iuser3);
    HL_ADD_SCALAR(table, weapon_data_t, fuser1);
    HL_ADD_SCALAR(table, weapon_data_t, fuser2);
    HL_ADD_SCALAR(table, weapon_data_t, fuser3);
    return table;
}

GoldSrcDeltaTableLayout BuildEventLayout()
{
    GoldSrcDeltaTableLayout table;
    table.name = "event_t";
    table.structure_size = static_cast<std::uint32_t>(sizeof(event_args_t));
    HL_ADD_SCALAR(table, event_args_t, entindex);
    HL_ADD_SCALAR(table, event_args_t, bparam1);
    HL_ADD_SCALAR(table, event_args_t, bparam2);
    HL_ADD_VECTOR(table, event_args_t, origin);
    HL_ADD_SCALAR(table, event_args_t, fparam1);
    HL_ADD_SCALAR(table, event_args_t, fparam2);
    HL_ADD_SCALAR(table, event_args_t, iparam1);
    HL_ADD_SCALAR(table, event_args_t, iparam2);
    HL_ADD_VECTOR(table, event_args_t, angles);
    HL_ADD_SCALAR(table, event_args_t, ducking);
    return table;
}

#undef HL_ADD_BYTE_ARRAY_4
#undef HL_ADD_BYTE_ARRAY_2
#undef HL_ADD_VECTOR
#undef HL_ADD_SCALAR
} // namespace

GoldSrcDeltaLayoutRegistry BuildGoldSrcProtocol48DeltaLayouts()
{
    GoldSrcDeltaLayoutRegistry layouts;
    layouts.tables.reserve(hl::network::kGoldSrcCanonicalDeltaTableCount);
    layouts.tables.push_back(BuildClientDataLayout());
    layouts.tables.push_back(BuildEntityStateLayout("entity_state_t"));
    layouts.tables.push_back(
        BuildEntityStateLayout("entity_state_player_t"));
    layouts.tables.push_back(
        BuildEntityStateLayout("custom_entity_state_t"));
    layouts.tables.push_back(BuildUserCommandLayout());
    layouts.tables.push_back(BuildWeaponDataLayout());
    layouts.tables.push_back(BuildEventLayout());
    return layouts;
}
} // namespace hl::game_api::detail
