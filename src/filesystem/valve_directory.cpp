#include "filesystem/valve_directory.h"

#include <algorithm>
#include <array>
#include <utility>

namespace
{
void AddCandidate(
    const hl::filesystem::FileSystem& file_system,
    const std::filesystem::path& candidate,
    std::vector<std::filesystem::path>& candidates)
{
    if (candidate.empty())
    {
        return;
    }

    const std::filesystem::path absolute_candidate = file_system.AbsolutePath(candidate);
    const auto found = std::find(candidates.begin(), candidates.end(), absolute_candidate);
    if (found == candidates.end())
    {
        candidates.push_back(absolute_candidate);
    }
}

void AppendValveCandidatesFromBase(
    const hl::filesystem::FileSystem& file_system,
    const std::filesystem::path& base_path,
    std::vector<std::filesystem::path>& candidates)
{
    if (base_path.empty())
    {
        return;
    }

    std::filesystem::path current = file_system.AbsolutePath(base_path);
    for (int depth = 0; depth < 6; ++depth)
    {
        if (current.filename() == L"valve")
        {
            AddCandidate(file_system, current, candidates);
        }

        AddCandidate(file_system, current / L"valve", candidates);

        const std::filesystem::path parent = current.parent_path();
        if (parent.empty() || parent == current)
        {
            break;
        }

        current = parent;
    }
}

hl::filesystem::ValidationCheckStatus MakeValidationCheck(
    std::wstring label,
    std::filesystem::path path,
    bool found,
    bool optional = false)
{
    hl::filesystem::ValidationCheckStatus status;
    status.label = std::move(label);
    status.resolved_path = std::move(path);
    status.checked_paths.push_back(status.resolved_path);
    status.found = found;
    status.optional = optional;
    return status;
}

hl::filesystem::ValidationCheckStatus MakeClientDllCheck(
    const hl::filesystem::FileSystem& file_system,
    const std::filesystem::path& root_path)
{
    hl::filesystem::ValidationCheckStatus status;
    status.label = L"client.dll";
    status.checked_paths = {
        root_path / L"cl_dlls/client.dll",
        root_path / L"dlls/client.dll",
    };
    status.resolved_path = status.checked_paths.front();

    for (const std::filesystem::path& candidate : status.checked_paths)
    {
        if (file_system.FileExists(candidate))
        {
            status.found = true;
            status.resolved_path = candidate;
            break;
        }
    }

    return status;
}
} // namespace

namespace hl::filesystem
{
bool ValveDirectoryValidationResult::IsValid() const noexcept
{
    return valve_directory.found && hl_dll.found && client_dll.found;
}

ValveDirectory::ValveDirectory(std::filesystem::path root_path)
    : root_path_(std::move(root_path))
{
}

const std::filesystem::path& ValveDirectory::RootPath() const noexcept
{
    return root_path_;
}

ValveDirectoryValidationResult ValveDirectory::Validate(const FileSystem& file_system) const
{
    ValveDirectoryValidationResult result;
    result.root_path = file_system.AbsolutePath(root_path_);
    result.valve_directory = MakeValidationCheck(
        L"valve directory",
        result.root_path,
        file_system.DirectoryExists(result.root_path));
    result.hl_dll = MakeValidationCheck(
        L"hl.dll",
        result.root_path / L"dlls/hl.dll",
        file_system.FileExists(result.root_path / L"dlls/hl.dll"));
    result.client_dll = MakeClientDllCheck(file_system, result.root_path);
    result.pak0_pak = MakeValidationCheck(
        L"pak0.pak",
        result.root_path / L"pak0.pak",
        file_system.FileExists(result.root_path / L"pak0.pak"),
        true);

    if (!result.valve_directory.found)
    {
        result.issues.push_back({
            std::filesystem::path(),
            L"Game directory does not exist: " + result.root_path.wstring(),
        });
    }

    if (!result.hl_dll.found)
    {
        result.issues.push_back({
            std::filesystem::path(L"dlls/hl.dll"),
            L"Missing required file: " + result.hl_dll.resolved_path.wstring(),
        });
    }

    if (!result.client_dll.found)
    {
        const std::wstring primary_client_path = result.client_dll.checked_paths.at(0).wstring();
        const std::wstring fallback_client_path = result.client_dll.checked_paths.at(1).wstring();
        result.issues.push_back({
            std::filesystem::path(L"cl_dlls/client.dll"),
            L"Missing required file: " + primary_client_path
                + L" (fallback checked: " + fallback_client_path + L")",
        });
    }

    return result;
}

std::optional<std::filesystem::path> ResolveValveDirectory(
    const FileSystem& file_system,
    const std::optional<std::filesystem::path>& requested_path,
    const std::filesystem::path& executable_path,
    const std::filesystem::path& working_directory,
    std::vector<std::filesystem::path>* checked_candidates)
{
    std::vector<std::filesystem::path> candidates;

    if (requested_path.has_value())
    {
        AddCandidate(file_system, *requested_path, candidates);
    }
    else
    {
        AppendValveCandidatesFromBase(file_system, executable_path.parent_path(), candidates);
        AppendValveCandidatesFromBase(file_system, working_directory, candidates);
    }

    if (checked_candidates != nullptr)
    {
        *checked_candidates = candidates;
    }

    for (const std::filesystem::path& candidate : candidates)
    {
        if (file_system.DirectoryExists(candidate))
        {
            return candidate;
        }
    }

    return std::nullopt;
}
} // namespace hl::filesystem
