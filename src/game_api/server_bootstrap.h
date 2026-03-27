#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "game_api/cvar_registry.h"

#pragma warning(push, 0)
#include "extdll.h"
#pragma warning(pop)

namespace hl::game_api::detail
{
struct EntityStateSnapshot
{
    int index = -1;
    bool free_flag = true;
    bool in_use = false;
    bool removed = false;
    std::string classname;
    string_t classname_index = 0;
    bool classname_index_valid = false;
    std::string targetname;
    string_t targetname_index = 0;
    bool targetname_index_valid = false;
    std::string origin_string;
    Vector origin = Vector(0.0f, 0.0f, 0.0f);
    bool has_origin = false;
    std::string angles_string;
    Vector angles = Vector(0.0f, 0.0f, 0.0f);
    bool has_angles = false;
    std::string mins_string;
    Vector mins = Vector(0.0f, 0.0f, 0.0f);
    std::string maxs_string;
    Vector maxs = Vector(0.0f, 0.0f, 0.0f);
    bool has_size = false;
    Vector absmin = Vector(0.0f, 0.0f, 0.0f);
    Vector absmax = Vector(0.0f, 0.0f, 0.0f);
    std::string model_string;
    string_t model_index_string = 0;
    bool model_string_index_valid = false;
    int model_index = 0;
    bool spawned = false;
    bool deferred = false;
    bool private_data_present = false;
    bool private_data_owned = false;
    std::size_t private_data_bytes = 0;
    std::uintptr_t private_data_address = 0;
    bool containing_entity_valid = false;
    bool activation_candidate = false;
    int parse_index = -1;
};

struct ServerState
{
    std::filesystem::path game_directory;
    std::string game_directory_utf8;
    std::string mod_name;
    std::string hostname;
    int maxclients = 1;
    std::string map_name;
    std::string startspot;
    bool active = false;
    bool loading = true;
    bool initialized = false;
    double realtime = 0.0;
    double old_realtime = 0.0;
    std::uint64_t frame_count = 0;
    std::uint64_t server_frame = 0;
    float time = 0.0f;
    float frametime = 0.0f;
    float deathmatch = 1.0f;
    float coop = 0.0f;
    std::vector<std::string> command_line_args;
    std::string command_line_tail;

    int Argc() const noexcept;
    const char* Argv(int index) const noexcept;
    const char* Args() const noexcept;
    int CheckParm(const char* token, char** next) const noexcept;
};

class EngineStringPool
{
public:
    void Reset(std::size_t capacity = 1024 * 1024);

    string_t Alloc(std::string_view value);
    string_t MakeString(const char* value);

    const char* Base() const noexcept;
    const char* SzFromIndex(string_t index) const noexcept;
    bool OwnsIndex(string_t index) const noexcept;
    std::string Describe(string_t index) const;

private:
    std::unique_ptr<char[]> storage_;
    std::size_t capacity_ = 0;
    std::size_t size_ = 0;
    std::unordered_map<std::string, string_t> indices_by_value_;
    mutable std::unordered_map<string_t, std::string> values_by_index_;
};

class PrecacheRegistry
{
public:
    void Reset();

    int PrecacheModel(std::string_view name);
    int PrecacheSound(std::string_view name);
    int ModelIndex(std::string_view name) const noexcept;
    int EnsureModelIndex(std::string_view name);
    const std::string* ModelName(int index) const noexcept;
    const std::string* SoundName(int index) const noexcept;

    std::size_t ModelCount() const noexcept;
    std::size_t SoundCount() const noexcept;

private:
    int Upsert(
        std::string_view name,
        std::unordered_map<std::string, int>& indices,
        std::vector<std::string>& ordered_names);

    std::unordered_map<std::string, int> model_indices_;
    std::unordered_map<std::string, int> sound_indices_;
    std::vector<std::string> model_names_;
    std::vector<std::string> sound_names_;
};

class EdictStore
{
public:
    void Reset(std::size_t max_clients, std::size_t extra_entity_slots = 16);

    edict_t* World() noexcept;
    const edict_t* World() const noexcept;
    edict_t* CreateEntity();
    void RemoveEntity(edict_t* entity);
    void SetInUse(edict_t* entity, bool in_use);
    void SetClassname(edict_t* entity, string_t class_name, std::string_view class_name_text);
    void SetTargetname(edict_t* entity, string_t target_name, std::string_view target_name_text);
    void SetOrigin(edict_t* entity, const Vector& origin, std::string_view origin_text);
    void SetAngles(edict_t* entity, const Vector& angles, std::string_view angles_text);
    void SetSize(
        edict_t* entity,
        const Vector& mins,
        const Vector& maxs,
        std::string_view mins_text,
        std::string_view maxs_text);
    void SetModel(
        edict_t* entity,
        string_t model_name,
        std::string_view model_name_text,
        int model_index);
    void SetSpawned(edict_t* entity, bool spawned);
    void SetDeferred(edict_t* entity, bool deferred);
    void SetActivationCandidate(edict_t* entity, bool activation_candidate);
    void SetParseIndex(edict_t* entity, int parse_index);
    void* AllocatePrivateData(edict_t* entity, std::size_t bytes);
    void* PrivateDataOf(edict_t* entity) noexcept;
    const void* PrivateDataOf(const edict_t* entity) const noexcept;
    void FreePrivateData(edict_t* entity);

    edict_t* EntityOfIndex(int index) noexcept;
    const edict_t* EntityOfIndex(int index) const noexcept;
    edict_t* EntityOfOffset(int offset) noexcept;
    entvars_t* VarsOf(edict_t* entity) noexcept;
    edict_t* FindByVars(entvars_t* vars) noexcept;

    int IndexOf(const edict_t* entity) const noexcept;
    int OffsetOf(const edict_t* entity) const noexcept;
    int MaxEntities() const noexcept;
    int NumberOfEntities() const noexcept;
    int AllocatedCount() const noexcept;
    EntityStateSnapshot SnapshotOf(
        const edict_t* entity,
        const EngineStringPool& string_pool) const;
    std::string DumpEntityState(const edict_t* entity, const EngineStringPool& string_pool) const;

private:
    struct EntitySlotState
    {
        bool in_use = false;
        bool removed = false;
        std::string classname;
        std::string targetname;
        std::string origin_string;
        Vector origin = Vector(0.0f, 0.0f, 0.0f);
        bool has_origin = false;
        std::string angles_string;
        Vector angles = Vector(0.0f, 0.0f, 0.0f);
        bool has_angles = false;
        std::string mins_string;
        Vector mins = Vector(0.0f, 0.0f, 0.0f);
        std::string maxs_string;
        Vector maxs = Vector(0.0f, 0.0f, 0.0f);
        bool has_size = false;
        std::string model_string;
        int model_index = 0;
        bool spawned = false;
        bool deferred = false;
        bool activation_candidate = false;
        int parse_index = -1;
        std::unique_ptr<unsigned char[]> private_data;
        std::size_t private_data_bytes = 0;
    };

    void ClearEdict(std::size_t index, bool free_slot);

    std::vector<edict_t> edicts_;
    std::vector<EntitySlotState> states_;
    std::size_t max_clients_ = 0;
    int next_serial_ = 1;
};

std::string NormalizeModName(std::string_view value);
std::string NormalizeMapName(std::string_view value);
std::string NormalizeHostname(std::string_view value);
int NormalizeMaxClients(int value);
std::string BuildMapModelPath(std::string_view map_name);

void InitializeServerState(
    ServerState& state,
    const std::filesystem::path& game_directory,
    std::string_view mod_name,
    std::string_view map_name,
    std::string_view hostname,
    int maxclients);

void SeedServerCvars(CvarRegistry& registry, const ServerState& state);
void FinalizeServerState(ServerState& state, const CvarRegistry& registry);
void ApplyGlobalsFromServerState(
    const ServerState& state,
    EngineStringPool& string_pool,
    globalvars_t& global_variables);
} // namespace hl::game_api::detail
