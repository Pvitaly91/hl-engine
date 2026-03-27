#pragma once

#include <cstddef>
#include <deque>
#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>

#include "filesystem/file_system.h"

#pragma warning(push, 0)
#include "extdll.h"
#pragma warning(pop)

namespace hl::game_api::detail
{
class EngineStringPool;

struct SoundPrecacheEntry
{
    int index = 0;
    std::string requested_path;
    std::string normalized_path;
    std::filesystem::path resolved_path;
    bool file_exists = false;
    string_t string_index = 0;
    std::size_t request_count = 0;
};

struct SoundPrecacheResult
{
    int index = 0;
    string_t string_index = 0;
    const char* stable_path = "";
    std::string requested_path;
    std::string normalized_path;
    std::filesystem::path resolved_path;
    bool file_exists = false;
    bool inserted = false;
    bool duplicate = false;
    std::size_t registry_size = 0;
    std::size_t total_requests = 0;
    std::size_t duplicate_requests = 0;
    std::size_t missing_files = 0;
};

class SoundPrecacheRegistry
{
public:
    void Reset();

    SoundPrecacheResult PrecacheSound(
        std::string_view requested_path,
        const std::filesystem::path& game_directory,
        hl::filesystem::FileSystem& file_system,
        EngineStringPool* string_pool);

    int SoundIndex(std::string_view requested_path) const;
    const SoundPrecacheEntry* EntryByIndex(int index) const noexcept;
    const char* SoundName(int index) const noexcept;

    std::size_t RegistrySize() const noexcept;
    std::size_t TotalRequests() const noexcept;
    std::size_t DuplicateRequests() const noexcept;
    std::size_t MissingFileCount() const noexcept;

private:
    static std::string NormalizeSoundPath(std::string_view value);
    static std::filesystem::path ResolveSoundPath(
        const std::filesystem::path& game_directory,
        std::string_view normalized_path);

    std::unordered_map<std::string, int> indices_by_path_;
    std::deque<SoundPrecacheEntry> entries_;
    std::size_t total_requests_ = 0;
    std::size_t duplicate_requests_ = 0;
    std::size_t missing_file_count_ = 0;
};
} // namespace hl::game_api::detail
