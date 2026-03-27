#include "sound_precache_registry.h"

#include <algorithm>
#include <cctype>
#include <vector>

#include "server_bootstrap.h"

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
} // namespace

namespace hl::game_api::detail
{
void SoundPrecacheRegistry::Reset()
{
    indices_by_path_.clear();
    entries_.clear();
    total_requests_ = 0;
    duplicate_requests_ = 0;
    missing_file_count_ = 0;
}

SoundPrecacheResult SoundPrecacheRegistry::PrecacheSound(
    std::string_view requested_path,
    const std::filesystem::path& game_directory,
    hl::filesystem::FileSystem& file_system,
    EngineStringPool* string_pool)
{
    SoundPrecacheResult result;
    result.requested_path = std::string(requested_path);
    result.normalized_path = NormalizeSoundPath(requested_path);

    if (result.normalized_path.empty())
    {
        result.registry_size = entries_.size();
        result.total_requests = total_requests_;
        result.duplicate_requests = duplicate_requests_;
        result.missing_files = missing_file_count_;
        return result;
    }

    ++total_requests_;

    const auto existing = indices_by_path_.find(result.normalized_path);
    if (existing != indices_by_path_.end())
    {
        ++duplicate_requests_;
        SoundPrecacheEntry& entry = entries_[static_cast<std::size_t>(existing->second - 1)];
        ++entry.request_count;

        result.index = entry.index;
        result.string_index = entry.string_index;
        result.stable_path = entry.normalized_path.c_str();
        result.resolved_path = entry.resolved_path;
        result.file_exists = entry.file_exists;
        result.duplicate = true;
        result.registry_size = entries_.size();
        result.total_requests = total_requests_;
        result.duplicate_requests = duplicate_requests_;
        result.missing_files = missing_file_count_;
        return result;
    }

    SoundPrecacheEntry entry;
    entry.index = static_cast<int>(entries_.size()) + 1;
    entry.requested_path = result.requested_path;
    entry.normalized_path = result.normalized_path;
    entry.resolved_path = ResolveSoundPath(game_directory, entry.normalized_path);
    entry.file_exists = file_system.FileExists(entry.resolved_path);
    entry.string_index = string_pool != nullptr ? string_pool->Alloc(entry.normalized_path) : 0;
    entry.request_count = 1;

    if (!entry.file_exists)
    {
        ++missing_file_count_;
    }

    indices_by_path_.emplace(entry.normalized_path, entry.index);
    entries_.push_back(entry);

    const SoundPrecacheEntry& stored_entry = entries_.back();
    result.index = stored_entry.index;
    result.string_index = stored_entry.string_index;
    result.stable_path = stored_entry.normalized_path.c_str();
    result.resolved_path = stored_entry.resolved_path;
    result.file_exists = stored_entry.file_exists;
    result.inserted = true;
    result.registry_size = entries_.size();
    result.total_requests = total_requests_;
    result.duplicate_requests = duplicate_requests_;
    result.missing_files = missing_file_count_;
    return result;
}

int SoundPrecacheRegistry::SoundIndex(std::string_view requested_path) const
{
    const auto it = indices_by_path_.find(NormalizeSoundPath(requested_path));
    return it == indices_by_path_.end() ? 0 : it->second;
}

const SoundPrecacheEntry* SoundPrecacheRegistry::EntryByIndex(int index) const noexcept
{
    if (index <= 0 || static_cast<std::size_t>(index) > entries_.size())
    {
        return nullptr;
    }

    return &entries_[static_cast<std::size_t>(index - 1)];
}

const char* SoundPrecacheRegistry::SoundName(int index) const noexcept
{
    const SoundPrecacheEntry* entry = EntryByIndex(index);
    return entry != nullptr ? entry->normalized_path.c_str() : nullptr;
}

std::size_t SoundPrecacheRegistry::RegistrySize() const noexcept
{
    return entries_.size();
}

std::size_t SoundPrecacheRegistry::TotalRequests() const noexcept
{
    return total_requests_;
}

std::size_t SoundPrecacheRegistry::DuplicateRequests() const noexcept
{
    return duplicate_requests_;
}

std::size_t SoundPrecacheRegistry::MissingFileCount() const noexcept
{
    return missing_file_count_;
}

std::string SoundPrecacheRegistry::NormalizeSoundPath(std::string_view value)
{
    std::string normalized = ToLower(Trim(value));
    std::replace(normalized.begin(), normalized.end(), '\\', '/');

    std::vector<std::string> segments;
    std::size_t segment_begin = 0;
    while (segment_begin <= normalized.size())
    {
        const std::size_t slash = normalized.find('/', segment_begin);
        const std::size_t segment_end =
            slash == std::string::npos ? normalized.size() : slash;
        const std::string segment = normalized.substr(segment_begin, segment_end - segment_begin);
        if (!segment.empty() && segment != ".")
        {
            segments.push_back(segment);
        }

        if (slash == std::string::npos)
        {
            break;
        }

        segment_begin = slash + 1;
    }

    if (!segments.empty() && segments.front() == "sound")
    {
        segments.erase(segments.begin());
    }

    std::string joined;
    for (const std::string& segment : segments)
    {
        if (!joined.empty())
        {
            joined += '/';
        }

        joined += segment;
    }

    return joined;
}

std::filesystem::path SoundPrecacheRegistry::ResolveSoundPath(
    const std::filesystem::path& game_directory,
    std::string_view normalized_path)
{
    if (normalized_path.empty())
    {
        return game_directory / L"sound";
    }

    return (game_directory / L"sound" / std::filesystem::path(std::string(normalized_path)))
        .lexically_normal();
}
} // namespace hl::game_api::detail
