#include "game_api/cvar_registry.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdlib>
#include <utility>

#include "common/logger.h"

#pragma warning(push, 0)
#include "cvardef.h"
#pragma warning(pop)

namespace
{
char* MutableEmptyString()
{
    static char empty[] = "";
    return empty;
}

std::string_view TrimWhitespace(std::string_view value)
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

    return value.substr(begin, end - begin);
}

std::string_view TrimQuotes(std::string_view value)
{
    value = TrimWhitespace(value);
    if (value.size() >= 2 && value.front() == '"' && value.back() == '"')
    {
        return value.substr(1, value.size() - 2);
    }

    return value;
}

std::string ToLowerCopy(std::string_view value)
{
    std::string normalized(value);
    std::transform(
        normalized.begin(),
        normalized.end(),
        normalized.begin(),
        [](unsigned char character)
        {
            return static_cast<char>(std::tolower(character));
        });
    return normalized;
}

void SyncExternalBinding(cvar_t* external_cvar, const cvar_t& owned_cvar)
{
    if (external_cvar == nullptr)
    {
        return;
    }

    external_cvar->name = owned_cvar.name;
    external_cvar->string = owned_cvar.string;
    external_cvar->flags = owned_cvar.flags;
    external_cvar->value = owned_cvar.value;
    external_cvar->next = owned_cvar.next;
}
} // namespace

namespace hl::game_api
{
struct CvarRegistry::Record
{
    cvar_t cvar{};
    std::string name_storage;
    std::string string_storage;
    std::vector<cvar_t*> external_bindings;
    bool auto_created = false;
};

CvarRegistry::CvarRegistry() = default;

CvarRegistry::~CvarRegistry() = default;

CvarRegistry::CvarRegistry(CvarRegistry&&) noexcept = default;

CvarRegistry& CvarRegistry::operator=(CvarRegistry&&) noexcept = default;

float CvarRegistry::ParseNumericValue(std::string_view value)
{
    const std::string text(TrimQuotes(value));
    char* end = nullptr;
    const float parsed = std::strtof(text.c_str(), &end);
    return end == text.c_str() ? 0.0f : parsed;
}

CvarRegistry::Record& CvarRegistry::UpsertRecord(std::string_view name)
{
    const std::string key(name);
    auto [it, inserted] = records_.try_emplace(key, nullptr);
    if (inserted || it->second == nullptr)
    {
        it->second = std::make_unique<Record>();
        it->second->name_storage = key;
    }

    return *it->second;
}

void CvarRegistry::RefreshRecord(Record& record)
{
    record.cvar.name = record.name_storage.empty() ? nullptr : record.name_storage.data();
    record.cvar.string =
        record.string_storage.empty() ? MutableEmptyString() : record.string_storage.data();
    record.cvar.value = ParseNumericValue(record.string_storage);
    record.cvar.next = nullptr;
}

void CvarRegistry::SyncExternalBindings(Record& record)
{
    for (cvar_t* external_cvar : record.external_bindings)
    {
        SyncExternalBinding(external_cvar, record.cvar);
    }
}

CvarRegistrationResult CvarRegistry::Register(cvar_t* cvar)
{
    if (cvar == nullptr || cvar->name == nullptr || std::string_view(cvar->name).empty())
    {
        return {};
    }

    const std::string_view name(cvar->name);
    const bool already_registered = records_.find(std::string(name)) != records_.end();
    Record& record = UpsertRecord(name);

    if (already_registered)
    {
        common::Logger::Warn("Duplicate cvar registration detected for '" + std::string(name) + "'.");
    }

    if (std::find(record.external_bindings.begin(), record.external_bindings.end(), cvar)
        == record.external_bindings.end())
    {
        record.external_bindings.push_back(cvar);
    }

    record.cvar.flags = cvar->flags;
    record.string_storage = cvar->string != nullptr ? cvar->string : "0";
    RefreshRecord(record);
    SyncExternalBindings(record);

    return {&record.cvar, !already_registered, already_registered};
}

cvar_t* CvarRegistry::RegisterBuiltin(std::string_view name, std::string_view value, int flags)
{
    if (name.empty())
    {
        return nullptr;
    }

    Record& record = UpsertRecord(name);
    record.cvar.flags = flags;
    record.string_storage = std::string(value);
    RefreshRecord(record);
    SyncExternalBindings(record);
    return &record.cvar;
}

cvar_t* CvarRegistry::Find(std::string_view name) noexcept
{
    const auto it = records_.find(std::string(name));
    return it == records_.end() ? nullptr : &it->second->cvar;
}

const cvar_t* CvarRegistry::Find(std::string_view name) const noexcept
{
    const auto it = records_.find(std::string(name));
    return it == records_.end() ? nullptr : &it->second->cvar;
}

bool CvarRegistry::SetValue(std::string_view name, std::string_view value)
{
    const auto it = records_.find(std::string(name));
    if (it == records_.end())
    {
        return false;
    }

    Record& record = *it->second;
    record.string_storage = std::string(value);
    RefreshRecord(record);
    SyncExternalBindings(record);
    return true;
}

CvarSetResult CvarRegistry::SetValueOrCreate(
    std::string_view name,
    std::string_view value,
    int flags)
{
    if (name.empty())
    {
        return {};
    }

    auto it = records_.find(std::string(name));
    if (it == records_.end())
    {
        Record& record = UpsertRecord(name);
        record.cvar.flags = flags;
        record.string_storage = std::string(value);
        record.auto_created = true;
        RefreshRecord(record);
        SyncExternalBindings(record);
        ++auto_created_count_;
        return {&record.cvar, true, true};
    }

    Record& record = *it->second;
    record.string_storage = std::string(value);
    record.cvar.flags = flags != 0 ? flags : record.cvar.flags;
    RefreshRecord(record);
    SyncExternalBindings(record);
    return {&record.cvar, true, false};
}

std::optional<CvarSnapshot> CvarRegistry::SnapshotOf(std::string_view name) const
{
    const auto it = records_.find(std::string(name));
    if (it == records_.end())
    {
        return std::nullopt;
    }

    const Record& record = *it->second;
    return CvarSnapshot{
        record.name_storage,
        record.string_storage,
        record.cvar.value,
        record.cvar.flags,
        record.auto_created,
    };
}

std::vector<CvarSnapshot> CvarRegistry::SnapshotMatchingPrefix(
    std::string_view prefix,
    std::size_t limit) const
{
    std::vector<CvarSnapshot> snapshots;
    snapshots.reserve(records_.size());

    for (const auto& [name, record] : records_)
    {
        if (!prefix.empty() && name.rfind(prefix.data(), 0) != 0)
        {
            continue;
        }

        snapshots.push_back({
            record->name_storage,
            record->string_storage,
            record->cvar.value,
            record->cvar.flags,
            record->auto_created,
        });
    }

    std::sort(
        snapshots.begin(),
        snapshots.end(),
        [](const CvarSnapshot& left, const CvarSnapshot& right)
        {
            return left.name < right.name;
        });

    if (limit != 0 && snapshots.size() > limit)
    {
        snapshots.resize(limit);
    }

    return snapshots;
}

std::size_t CvarRegistry::Count() const noexcept
{
    return records_.size();
}

std::size_t CvarRegistry::AutoCreatedCount() const noexcept
{
    return auto_created_count_;
}

bool CvarRegistry::IsSafeAutoCreateName(std::string_view name) noexcept
{
    if (name.empty())
    {
        return false;
    }

    const std::string normalized = ToLowerCopy(name);
    static constexpr std::array<std::string_view, 10> kExplicitSafeNames = {{
        "deathmatch",
        "coop",
        "hostname",
        "skill",
        "teamplay",
        "room_type",
        "lservercfgfile",
        "servercfgfile",
        "mapcyclefile",
        "motdfile",
    }};

    if (std::find(kExplicitSafeNames.begin(), kExplicitSafeNames.end(), normalized)
        != kExplicitSafeNames.end())
    {
        return true;
    }

    static constexpr std::array<std::string_view, 4> kSafePrefixes = {{
        "sv_",
        "mp_",
        "v_",
        "room_",
    }};

    for (const std::string_view prefix : kSafePrefixes)
    {
        if (normalized.rfind(prefix.data(), 0) == 0)
        {
            return true;
        }
    }

    return false;
}
} // namespace hl::game_api
