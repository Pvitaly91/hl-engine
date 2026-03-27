#pragma once

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

struct cvar_s;
using cvar_t = cvar_s;

namespace hl::game_api
{
struct CvarRegistrationResult
{
    cvar_t* cvar = nullptr;
    bool inserted = false;
    bool duplicate = false;
};

struct CvarSnapshot
{
    std::string name;
    std::string string_value;
    float value = 0.0f;
    int flags = 0;
    bool auto_created = false;
};

struct CvarSetResult
{
    cvar_t* cvar = nullptr;
    bool updated = false;
    bool created = false;
};

class CvarRegistry
{
public:
    CvarRegistry();
    ~CvarRegistry();

    CvarRegistry(const CvarRegistry&) = delete;
    CvarRegistry& operator=(const CvarRegistry&) = delete;
    CvarRegistry(CvarRegistry&&) noexcept;
    CvarRegistry& operator=(CvarRegistry&&) noexcept;

    CvarRegistrationResult Register(cvar_t* cvar);
    cvar_t* RegisterBuiltin(std::string_view name, std::string_view value, int flags = 0);

    cvar_t* Find(std::string_view name) noexcept;
    const cvar_t* Find(std::string_view name) const noexcept;

    bool SetValue(std::string_view name, std::string_view value);
    CvarSetResult SetValueOrCreate(
        std::string_view name,
        std::string_view value,
        int flags = 0);
    std::optional<CvarSnapshot> SnapshotOf(std::string_view name) const;
    std::vector<CvarSnapshot> SnapshotMatchingPrefix(
        std::string_view prefix,
        std::size_t limit = 0) const;

    std::size_t Count() const noexcept;
    std::size_t AutoCreatedCount() const noexcept;

    static bool IsSafeAutoCreateName(std::string_view name) noexcept;

private:
    struct Record;

    static float ParseNumericValue(std::string_view value);
    Record& UpsertRecord(std::string_view name);
    void RefreshRecord(Record& record);
    void SyncExternalBindings(Record& record);

    std::unordered_map<std::string, std::unique_ptr<Record>> records_;
    std::size_t auto_created_count_ = 0;
};
} // namespace hl::game_api
