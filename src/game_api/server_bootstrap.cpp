#include "server_bootstrap.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <limits>
#include <optional>
#include <utility>

#include "common/logger.h"
#include "common/text_encoding.h"

namespace
{
std::string Trim(std::string_view value)
{
    std::size_t begin = 0;
    while (begin < value.size() && std::isspace(static_cast<unsigned char>(value[begin])) != 0)
    {
        ++begin;
    }

    std::size_t end = value.size();
    while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0)
    {
        --end;
    }

    return std::string(value.substr(begin, end - begin));
}

std::string ToLower(std::string value)
{
    std::transform(
        value.begin(),
        value.end(),
        value.begin(),
        [](unsigned char character)
        {
            return static_cast<char>(std::tolower(character));
        });
    return value;
}

std::string NormalizeResourcePath(std::string_view value)
{
    std::string normalized = Trim(value);
    std::replace(normalized.begin(), normalized.end(), '\\', '/');
    return ToLower(normalized);
}

std::string NormalizeLeafLikeValue(std::string_view value, std::string_view fallback)
{
    std::string normalized = Trim(value);
    if (normalized.empty())
    {
        normalized = std::string(fallback);
    }

    std::replace(normalized.begin(), normalized.end(), '\\', '/');

    const std::size_t slash = normalized.find_last_of('/');
    if (slash != std::string::npos)
    {
        normalized = normalized.substr(slash + 1);
    }

    const std::size_t dot = normalized.rfind('.');
    if (dot != std::string::npos)
    {
        normalized = normalized.substr(0, dot);
    }

    return ToLower(normalized.empty() ? std::string(fallback) : normalized);
}

std::string JoinArgs(const std::vector<std::string>& args)
{
    if (args.size() <= 1)
    {
        return {};
    }

    std::string joined;
    for (std::size_t index = 1; index < args.size(); ++index)
    {
        if (!joined.empty())
        {
            joined += ' ';
        }

        joined += args[index];
    }

    return joined;
}

bool TryCopyCString(const char* value, char* buffer, std::size_t capacity, bool* truncated)
{
    if (buffer == nullptr || capacity == 0)
    {
        return false;
    }

    if (truncated != nullptr)
    {
        *truncated = false;
    }

    if (value == nullptr)
    {
        buffer[0] = '\0';
        return true;
    }

    __try
    {
        const std::size_t max_chars = capacity - 1;
        for (std::size_t index = 0; index < max_chars; ++index)
        {
            const char character = value[index];
            buffer[index] = character;
            if (character == '\0')
            {
                return true;
            }
        }

        buffer[max_chars] = '\0';
        if (truncated != nullptr)
        {
            *truncated = true;
        }
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        if (buffer != nullptr)
        {
            buffer[0] = '\0';
        }
        return false;
    }
}

std::optional<std::string> TryReadCString(const char* value, std::size_t max_length = 512)
{
    constexpr std::size_t kBufferCapacity = 513;
    if (max_length + 1 > kBufferCapacity)
    {
        max_length = kBufferCapacity - 1;
    }

    char buffer[kBufferCapacity] = {};
    bool truncated = false;
    if (!TryCopyCString(value, buffer, max_length + 1, &truncated))
    {
        return std::nullopt;
    }

    std::string text(buffer);
    if (truncated)
    {
        text += "...";
    }

    return text;
}

float ReadFloatCvar(
    const hl::game_api::CvarRegistry& registry,
    std::string_view name,
    float fallback)
{
    const std::optional<hl::game_api::CvarSnapshot> snapshot = registry.SnapshotOf(name);
    return snapshot.has_value() ? snapshot->value : fallback;
}

std::string ReadStringCvar(
    const hl::game_api::CvarRegistry& registry,
    std::string_view name,
    std::string_view fallback)
{
    const std::optional<hl::game_api::CvarSnapshot> snapshot = registry.SnapshotOf(name);
    return snapshot.has_value() && !snapshot->string_value.empty()
        ? snapshot->string_value
        : std::string(fallback);
}
} // namespace

namespace hl::game_api::detail
{
int ServerState::Argc() const noexcept
{
    return static_cast<int>(command_line_args.size());
}

const char* ServerState::Argv(int index) const noexcept
{
    if (index < 0 || static_cast<std::size_t>(index) >= command_line_args.size())
    {
        return "";
    }

    return command_line_args[static_cast<std::size_t>(index)].c_str();
}

const char* ServerState::Args() const noexcept
{
    return command_line_tail.c_str();
}

int ServerState::CheckParm(const char* token, char** next) const noexcept
{
    if (next != nullptr)
    {
        *next = nullptr;
    }

    if (token == nullptr || *token == '\0')
    {
        return 0;
    }

    for (std::size_t index = 0; index < command_line_args.size(); ++index)
    {
        if (command_line_args[index] != token)
        {
            continue;
        }

        if (next != nullptr && index + 1 < command_line_args.size())
        {
            *next = const_cast<char*>(command_line_args[index + 1].c_str());
        }

        return static_cast<int>(index);
    }

    return 0;
}

void EngineStringPool::Reset(std::size_t capacity)
{
    capacity_ = std::max<std::size_t>(capacity, 1);
    storage_ = std::make_unique<char[]>(capacity_);
    storage_[0] = '\0';
    size_ = 1;
    indices_by_value_.clear();
    values_by_index_.clear();
    values_by_index_.emplace(0, std::string());
}

string_t EngineStringPool::Alloc(std::string_view value)
{
    if (storage_ == nullptr)
    {
        Reset();
    }

    const std::string text(value);
    if (text.empty())
    {
        return 0;
    }

    const auto existing = indices_by_value_.find(text);
    if (existing != indices_by_value_.end())
    {
        return existing->second;
    }

    const std::size_t required = size_ + text.size() + 1;
    if (required > capacity_)
    {
        common::Logger::Warn(
            "Engine string pool capacity exceeded; returning null string for '" + text + "'.");
        return 0;
    }

    const string_t index = static_cast<string_t>(size_);
    std::memcpy(storage_.get() + size_, text.c_str(), text.size() + 1);
    size_ = required;

    indices_by_value_.emplace(text, index);
    values_by_index_.emplace(index, text);
    return index;
}

string_t EngineStringPool::MakeString(const char* value)
{
    if (value == nullptr || *value == '\0')
    {
        return 0;
    }

    const std::optional<std::string> text = TryReadCString(value);
    if (!text.has_value())
    {
        common::Logger::Warn("Engine string pool could not read external string for MakeString.");
        return 0;
    }

    return Alloc(*text);
}

const char* EngineStringPool::Base() const noexcept
{
    return storage_ != nullptr ? storage_.get() : "";
}

const char* EngineStringPool::SzFromIndex(string_t index) const noexcept
{
    if (index == 0)
    {
        return Base();
    }

    if (OwnsIndex(index))
    {
        const auto base = reinterpret_cast<std::uintptr_t>(Base());
        return reinterpret_cast<const char*>(
            base + static_cast<std::uintptr_t>(static_cast<std::uint32_t>(index)));
    }

    const auto cached = values_by_index_.find(index);
    if (cached != values_by_index_.end())
    {
        return cached->second.c_str();
    }

    const auto base = reinterpret_cast<std::uintptr_t>(Base());
    const auto offset = static_cast<std::uintptr_t>(static_cast<std::uint32_t>(index));
    const char* external_pointer = reinterpret_cast<const char*>(base + offset);
    const std::optional<std::string> external_text = TryReadCString(external_pointer);
    if (!external_text.has_value())
    {
        return Base();
    }

    const auto [it, inserted] = values_by_index_.try_emplace(index, *external_text);
    if (!inserted)
    {
        it->second = *external_text;
    }

    return it->second.c_str();
}

bool EngineStringPool::OwnsIndex(string_t index) const noexcept
{
    return index < size_;
}

std::string EngineStringPool::Describe(string_t index) const
{
    const auto it = values_by_index_.find(index);
    if (it != values_by_index_.end())
    {
        return it->second;
    }

    if (index == 0)
    {
        return {};
    }

    if (OwnsIndex(index))
    {
        return SzFromIndex(index);
    }

    const char* external_pointer = SzFromIndex(index);
    if (external_pointer != Base())
    {
        return external_pointer;
    }

    return "<unreadable external index=" + std::to_string(index) + ">";
}

void PrecacheRegistry::Reset()
{
    model_indices_.clear();
    sound_indices_.clear();
    model_names_.clear();
    sound_names_.clear();
}

int PrecacheRegistry::Upsert(
    std::string_view name,
    std::unordered_map<std::string, int>& indices,
    std::vector<std::string>& ordered_names)
{
    const std::string text = NormalizeResourcePath(name);
    if (text.empty())
    {
        return 0;
    }

    const auto existing = indices.find(text);
    if (existing != indices.end())
    {
        return existing->second;
    }

    const int index = static_cast<int>(ordered_names.size()) + 1;
    ordered_names.push_back(text);
    indices.emplace(text, index);
    return index;
}

int PrecacheRegistry::PrecacheModel(std::string_view name)
{
    return Upsert(name, model_indices_, model_names_);
}

int PrecacheRegistry::PrecacheSound(std::string_view name)
{
    return Upsert(name, sound_indices_, sound_names_);
}

int PrecacheRegistry::ModelIndex(std::string_view name) const noexcept
{
    const auto it = model_indices_.find(NormalizeResourcePath(name));
    return it == model_indices_.end() ? 0 : it->second;
}

int PrecacheRegistry::EnsureModelIndex(std::string_view name)
{
    const int existing = ModelIndex(name);
    return existing != 0 ? existing : PrecacheModel(name);
}

std::size_t PrecacheRegistry::ModelCount() const noexcept
{
    return model_names_.size();
}

const std::string* PrecacheRegistry::ModelName(int index) const noexcept
{
    if (index <= 0 || static_cast<std::size_t>(index) > model_names_.size())
    {
        return nullptr;
    }

    return &model_names_[static_cast<std::size_t>(index - 1)];
}

const std::string* PrecacheRegistry::SoundName(int index) const noexcept
{
    if (index <= 0 || static_cast<std::size_t>(index) > sound_names_.size())
    {
        return nullptr;
    }

    return &sound_names_[static_cast<std::size_t>(index - 1)];
}

std::size_t PrecacheRegistry::SoundCount() const noexcept
{
    return sound_names_.size();
}

void EdictStore::Reset(std::size_t max_clients, std::size_t extra_entity_slots)
{
    max_clients_ = max_clients;
    next_serial_ = 1;
    edicts_.assign(1 + max_clients_ + extra_entity_slots, {});
    states_ = std::vector<EntitySlotState>(edicts_.size());

    for (std::size_t index = 0; index < edicts_.size(); ++index)
    {
        ClearEdict(index, true);
    }

    if (!edicts_.empty())
    {
        ClearEdict(0, false);
    }
}

edict_t* EdictStore::World() noexcept
{
    return EntityOfIndex(0);
}

const edict_t* EdictStore::World() const noexcept
{
    return EntityOfIndex(0);
}

edict_t* EdictStore::CreateEntity()
{
    const std::size_t first_non_client_slot = 1 + max_clients_;
    for (std::size_t index = first_non_client_slot; index < edicts_.size(); ++index)
    {
        if (edicts_[index].free == FALSE)
        {
            continue;
        }

        ClearEdict(index, false);
        return &edicts_[index];
    }

    return nullptr;
}

void EdictStore::RemoveEntity(edict_t* entity)
{
    const int index = IndexOf(entity);
    if (index <= 0)
    {
        return;
    }

    const std::size_t slot = static_cast<std::size_t>(index);
    EntitySlotState removed_state = std::move(states_[slot]);
    removed_state.in_use = false;
    removed_state.removed = true;
    removed_state.spawned = false;
    removed_state.activation_candidate = false;
    removed_state.private_data.reset();
    removed_state.private_data_bytes = 0;

    edict_t& edict = edicts_[slot];
    edict = {};
    edict.free = TRUE;
    edict.serialnumber = next_serial_++;
    edict.headnode = -1;
    edict.freetime = 0.0f;
    edict.v.pContainingEntity = &edict;

    states_[slot] = std::move(removed_state);
}

void EdictStore::SetInUse(edict_t* entity, bool in_use)
{
    const int index = IndexOf(entity);
    if (index < 0)
    {
        return;
    }

    edicts_[static_cast<std::size_t>(index)].free = in_use ? FALSE : TRUE;
    EntitySlotState& state = states_[static_cast<std::size_t>(index)];
    state.in_use = in_use;
    if (in_use)
    {
        state.removed = false;
    }
    else
    {
        state.activation_candidate = false;
    }
}

void EdictStore::SetClassname(edict_t* entity, string_t class_name, std::string_view class_name_text)
{
    const int index = IndexOf(entity);
    if (index < 0)
    {
        return;
    }

    entity->v.classname = class_name;
    states_[static_cast<std::size_t>(index)].classname = std::string(class_name_text);
}

void EdictStore::SetTargetname(
    edict_t* entity,
    string_t target_name,
    std::string_view target_name_text)
{
    const int index = IndexOf(entity);
    if (index < 0)
    {
        return;
    }

    entity->v.targetname = target_name;
    states_[static_cast<std::size_t>(index)].targetname = std::string(target_name_text);
}

void EdictStore::SetOrigin(edict_t* entity, const Vector& origin, std::string_view origin_text)
{
    const int index = IndexOf(entity);
    if (index < 0)
    {
        return;
    }

    entity->v.origin = origin;

    EntitySlotState& state = states_[static_cast<std::size_t>(index)];
    state.origin = origin;
    state.origin_string = std::string(origin_text);
    state.has_origin = true;
}

void EdictStore::SetAngles(edict_t* entity, const Vector& angles, std::string_view angles_text)
{
    const int index = IndexOf(entity);
    if (index < 0)
    {
        return;
    }

    entity->v.angles = angles;

    EntitySlotState& state = states_[static_cast<std::size_t>(index)];
    state.angles = angles;
    state.angles_string = std::string(angles_text);
    state.has_angles = true;
}

void EdictStore::SetSize(
    edict_t* entity,
    const Vector& mins,
    const Vector& maxs,
    std::string_view mins_text,
    std::string_view maxs_text)
{
    const int index = IndexOf(entity);
    if (index < 0)
    {
        return;
    }

    entity->v.mins = mins;
    entity->v.maxs = maxs;
    entity->v.size = maxs - mins;

    EntitySlotState& state = states_[static_cast<std::size_t>(index)];
    state.mins = mins;
    state.mins_string = std::string(mins_text);
    state.maxs = maxs;
    state.maxs_string = std::string(maxs_text);
    state.has_size = true;
}

void EdictStore::SetModel(
    edict_t* entity,
    string_t model_name,
    std::string_view model_name_text,
    int model_index)
{
    const int index = IndexOf(entity);
    if (index < 0)
    {
        return;
    }

    entity->v.model = model_name;
    entity->v.modelindex = model_index;

    EntitySlotState& state = states_[static_cast<std::size_t>(index)];
    state.model_string = std::string(model_name_text);
    state.model_index = model_index;
}

void EdictStore::SetSpawned(edict_t* entity, bool spawned)
{
    const int index = IndexOf(entity);
    if (index < 0)
    {
        return;
    }

    EntitySlotState& state = states_[static_cast<std::size_t>(index)];
    state.spawned = spawned;
    if (spawned)
    {
        state.removed = false;
    }
}

void EdictStore::SetDeferred(edict_t* entity, bool deferred)
{
    const int index = IndexOf(entity);
    if (index < 0)
    {
        return;
    }

    states_[static_cast<std::size_t>(index)].deferred = deferred;
}

void EdictStore::SetActivationCandidate(edict_t* entity, bool activation_candidate)
{
    const int index = IndexOf(entity);
    if (index < 0)
    {
        return;
    }

    states_[static_cast<std::size_t>(index)].activation_candidate = activation_candidate;
}

void EdictStore::SetParseIndex(edict_t* entity, int parse_index)
{
    const int index = IndexOf(entity);
    if (index < 0)
    {
        return;
    }

    states_[static_cast<std::size_t>(index)].parse_index = parse_index;
}

void* EdictStore::AllocatePrivateData(edict_t* entity, std::size_t bytes)
{
    const int index = IndexOf(entity);
    if (index < 0 || bytes == 0)
    {
        return nullptr;
    }

    EntitySlotState& state = states_[static_cast<std::size_t>(index)];
    if (entity->pvPrivateData != nullptr && state.private_data != nullptr)
    {
        return entity->pvPrivateData;
    }

    state.private_data = std::make_unique<unsigned char[]>(bytes);
    std::memset(state.private_data.get(), 0, bytes);
    state.private_data_bytes = bytes;
    entity->pvPrivateData = state.private_data.get();
    return entity->pvPrivateData;
}

void* EdictStore::PrivateDataOf(edict_t* entity) noexcept
{
    return entity != nullptr ? entity->pvPrivateData : nullptr;
}

const void* EdictStore::PrivateDataOf(const edict_t* entity) const noexcept
{
    return entity != nullptr ? entity->pvPrivateData : nullptr;
}

void EdictStore::FreePrivateData(edict_t* entity)
{
    const int index = IndexOf(entity);
    if (index < 0)
    {
        return;
    }

    EntitySlotState& state = states_[static_cast<std::size_t>(index)];
    state.private_data.reset();
    state.private_data_bytes = 0;
    entity->pvPrivateData = nullptr;
}

edict_t* EdictStore::EntityOfIndex(int index) noexcept
{
    if (index < 0 || static_cast<std::size_t>(index) >= edicts_.size())
    {
        return nullptr;
    }

    return &edicts_[static_cast<std::size_t>(index)];
}

const edict_t* EdictStore::EntityOfIndex(int index) const noexcept
{
    if (index < 0 || static_cast<std::size_t>(index) >= edicts_.size())
    {
        return nullptr;
    }

    return &edicts_[static_cast<std::size_t>(index)];
}

edict_t* EdictStore::EntityOfOffset(int offset) noexcept
{
    if (offset < 0 || edicts_.empty())
    {
        return nullptr;
    }

    const std::size_t byte_offset = static_cast<std::size_t>(offset);
    const std::size_t total_bytes = edicts_.size() * sizeof(edict_t);
    if (byte_offset >= total_bytes || (byte_offset % sizeof(edict_t)) != 0)
    {
        return nullptr;
    }

    unsigned char* const base = reinterpret_cast<unsigned char*>(edicts_.data());
    return reinterpret_cast<edict_t*>(base + byte_offset);
}

entvars_t* EdictStore::VarsOf(edict_t* entity) noexcept
{
    return entity != nullptr ? &entity->v : nullptr;
}

edict_t* EdictStore::FindByVars(entvars_t* vars) noexcept
{
    return vars != nullptr ? vars->pContainingEntity : nullptr;
}

int EdictStore::IndexOf(const edict_t* entity) const noexcept
{
    if (entity == nullptr || edicts_.empty())
    {
        return -1;
    }

    const edict_t* begin = edicts_.data();
    const edict_t* end = begin + edicts_.size();
    if (entity < begin || entity >= end)
    {
        return -1;
    }

    return static_cast<int>(entity - begin);
}

int EdictStore::OffsetOf(const edict_t* entity) const noexcept
{
    if (entity == nullptr || edicts_.empty())
    {
        return 0;
    }

    const edict_t* begin = edicts_.data();
    const edict_t* end = begin + edicts_.size();
    if (entity < begin || entity >= end)
    {
        return 0;
    }

    const auto* base = reinterpret_cast<const unsigned char*>(begin);
    const auto* target = reinterpret_cast<const unsigned char*>(entity);
    return static_cast<int>(target - base);
}

int EdictStore::MaxEntities() const noexcept
{
    return static_cast<int>(edicts_.size());
}

int EdictStore::NumberOfEntities() const noexcept
{
    for (std::size_t index = edicts_.size(); index > 0; --index)
    {
        if (edicts_[index - 1].free == FALSE)
        {
            return static_cast<int>(index);
        }
    }

    return 0;
}

int EdictStore::AllocatedCount() const noexcept
{
    return static_cast<int>(std::count_if(
        edicts_.begin(),
        edicts_.end(),
        [](const edict_t& edict)
        {
            return edict.free == FALSE;
        }));
}

EntityStateSnapshot EdictStore::SnapshotOf(
    const edict_t* entity,
    const EngineStringPool& string_pool) const
{
    EntityStateSnapshot snapshot;
    const int index = IndexOf(entity);
    if (index < 0)
    {
        return snapshot;
    }

    const std::size_t slot = static_cast<std::size_t>(index);
    const edict_t& edict = edicts_[slot];
    const EntitySlotState& state = states_[slot];

    snapshot.index = index;
    snapshot.free_flag = edict.free != FALSE;
    snapshot.in_use = edict.free == FALSE;
    snapshot.removed = state.removed;
    snapshot.classname_index = edict.v.classname;
    snapshot.classname_index_valid =
        edict.v.classname == 0 || string_pool.OwnsIndex(edict.v.classname);
    snapshot.classname = !state.classname.empty()
        ? state.classname
        : string_pool.Describe(edict.v.classname);
    snapshot.targetname_index = edict.v.targetname;
    snapshot.targetname_index_valid =
        edict.v.targetname == 0 || string_pool.OwnsIndex(edict.v.targetname);
    snapshot.targetname = !state.targetname.empty()
        ? state.targetname
        : string_pool.Describe(edict.v.targetname);
    snapshot.origin_string = state.origin_string;
    snapshot.origin = state.origin;
    snapshot.has_origin = state.has_origin;
    snapshot.angles_string = state.angles_string;
    snapshot.angles = state.angles;
    snapshot.has_angles = state.has_angles;
    snapshot.mins_string = state.mins_string;
    snapshot.mins = state.mins;
    snapshot.maxs_string = state.maxs_string;
    snapshot.maxs = state.maxs;
    snapshot.has_size = state.has_size;
    snapshot.absmin = edict.v.absmin;
    snapshot.absmax = edict.v.absmax;
    snapshot.model_index_string = edict.v.model;
    snapshot.model_string_index_valid =
        edict.v.model == 0 || string_pool.OwnsIndex(edict.v.model);
    snapshot.model_string = !state.model_string.empty()
        ? state.model_string
        : string_pool.Describe(edict.v.model);
    snapshot.model_index = state.model_index != 0 ? state.model_index : edict.v.modelindex;
    snapshot.spawned = state.spawned;
    snapshot.deferred = state.deferred;
    snapshot.private_data_present = state.private_data != nullptr || edict.pvPrivateData != nullptr;
    snapshot.private_data_owned = state.private_data != nullptr;
    snapshot.private_data_bytes = state.private_data_bytes;
    snapshot.private_data_address = reinterpret_cast<std::uintptr_t>(edict.pvPrivateData);
    snapshot.containing_entity_valid = edict.v.pContainingEntity == &edicts_[slot];
    snapshot.activation_candidate = state.activation_candidate;
    snapshot.parse_index = state.parse_index;
    return snapshot;
}

std::string EdictStore::DumpEntityState(const edict_t* entity, const EngineStringPool& string_pool) const
{
    const EntityStateSnapshot snapshot = SnapshotOf(entity, string_pool);
    if (snapshot.index < 0)
    {
        return "edict?<external>";
    }

    return "edict#" + std::to_string(snapshot.index)
        + " { in_use=" + std::string(snapshot.in_use ? "yes" : "no")
        + ", removed=" + std::string(snapshot.removed ? "yes" : "no")
        + ", classname="
        + (snapshot.classname.empty() ? std::string("<empty>") : snapshot.classname)
        + ", targetname="
        + (snapshot.targetname.empty() ? std::string("<empty>") : snapshot.targetname)
        + ", origin="
        + (snapshot.has_origin
            ? (snapshot.origin_string.empty()
                ? std::to_string(snapshot.origin.x) + " "
                    + std::to_string(snapshot.origin.y) + " "
                    + std::to_string(snapshot.origin.z)
                : snapshot.origin_string)
            : std::string("<unset>"))
        + ", model="
        + (snapshot.model_string.empty() ? std::string("<empty>") : snapshot.model_string)
        + ", modelindex=" + std::to_string(snapshot.model_index)
        + ", spawned=" + std::string(snapshot.spawned ? "yes" : "no")
        + ", deferred=" + std::string(snapshot.deferred ? "yes" : "no")
        + ", private_data=" + std::string(snapshot.private_data_present ? "yes" : "no")
        + ", activate=" + std::string(snapshot.activation_candidate ? "yes" : "no")
        + ", parse_index=" + std::to_string(snapshot.parse_index)
        + " }";
}

void EdictStore::ClearEdict(std::size_t index, bool free_slot)
{
    edict_t& edict = edicts_[index];
    EntitySlotState& state = states_[index];
    state = {};

    edict = {};
    edict.free = free_slot ? TRUE : FALSE;
    edict.serialnumber = next_serial_++;
    edict.headnode = -1;
    edict.freetime = 0.0f;
    edict.v.pContainingEntity = &edict;
    state.in_use = !free_slot;
    state.removed = false;
}

std::string NormalizeModName(std::string_view value)
{
    return NormalizeLeafLikeValue(value, "valve");
}

std::string NormalizeMapName(std::string_view value)
{
    return NormalizeLeafLikeValue(value, "c0a0");
}

std::string NormalizeHostname(std::string_view value)
{
    const std::string normalized = Trim(value);
    return normalized.empty() ? "HLengine Test Server" : normalized;
}

int NormalizeMaxClients(int value)
{
    if (value < 1)
    {
        return 1;
    }

    return value > 32 ? 32 : value;
}

std::string BuildMapModelPath(std::string_view map_name)
{
    return "maps/" + NormalizeMapName(map_name) + ".bsp";
}

void InitializeServerState(
    ServerState& state,
    const std::filesystem::path& game_directory,
    std::string_view mod_name,
    std::string_view map_name,
    std::string_view hostname,
    int maxclients,
    bool dedicated,
    float deathmatch,
    float coop)
{
    state = {};
    state.game_directory = game_directory;
    state.game_directory_utf8 = common::ToUtf8(game_directory);
    state.mod_name = NormalizeModName(mod_name);
    state.hostname = NormalizeHostname(hostname);
    state.dedicated = dedicated;
    state.requested_maxclients = NormalizeMaxClients(maxclients);
    state.maxclients = NormalizeMaxClients(maxclients);
    state.map_name = NormalizeMapName(map_name);
    state.startspot.clear();
    state.active = false;
    state.loading = true;
    state.initialized = false;
    state.realtime = 0.0;
    state.old_realtime = 0.0;
    state.frame_count = 0;
    state.server_frame = 0;
    state.time = 0.0f;
    state.frametime = 0.0f;
    state.deathmatch = deathmatch == 0.0f ? 0.0f : 1.0f;
    state.coop = coop == 0.0f ? 0.0f : 1.0f;

    state.command_line_args = {
        "hlhost.exe",
        "-game",
        state.mod_name,
        "--gamedir",
        state.game_directory_utf8,
        "--map",
        state.map_name,
    };
    if (state.dedicated)
    {
        state.command_line_args.push_back("-dedicated");
    }
    state.command_line_tail = JoinArgs(state.command_line_args);
}

void SeedServerCvars(CvarRegistry& registry, const ServerState& state)
{
    registry.RegisterBuiltin("hostname", state.hostname);
    registry.RegisterBuiltin("deathmatch", state.deathmatch == 0.0f ? "0" : "1");
    registry.RegisterBuiltin("coop", state.coop == 0.0f ? "0" : "1");
}

void FinalizeServerState(ServerState& state, const CvarRegistry& registry)
{
    state.hostname = NormalizeHostname(ReadStringCvar(registry, "hostname", state.hostname));
    state.deathmatch = ReadFloatCvar(registry, "deathmatch", state.deathmatch);
    state.coop = ReadFloatCvar(registry, "coop", state.coop);
    state.loading = false;
    state.initialized = true;
}

void ApplyGlobalsFromServerState(
    const ServerState& state,
    EngineStringPool& string_pool,
    globalvars_t& global_variables)
{
    global_variables.mapname = string_pool.Alloc(state.map_name);
    global_variables.startspot = state.startspot.empty() ? 0 : string_pool.Alloc(state.startspot);
    global_variables.time = state.time;
    global_variables.frametime = state.frametime;
    global_variables.deathmatch = state.deathmatch;
    global_variables.coop = state.coop;
    global_variables.maxClients = state.maxclients;
    global_variables.pStringBase = string_pool.Base();
}
} // namespace hl::game_api::detail
