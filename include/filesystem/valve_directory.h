#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "filesystem/file_system.h"

namespace hl::filesystem
{
struct ValidationCheckStatus
{
    std::wstring label;
    std::filesystem::path resolved_path;
    std::vector<std::filesystem::path> checked_paths;
    bool found = false;
    bool optional = false;
};

struct ValidationIssue
{
    std::filesystem::path relative_path;
    std::wstring message;
};

struct ValveDirectoryValidationResult
{
    std::filesystem::path root_path;
    ValidationCheckStatus valve_directory;
    ValidationCheckStatus hl_dll;
    ValidationCheckStatus client_dll;
    ValidationCheckStatus pak0_pak;
    std::vector<ValidationIssue> issues;

    bool IsValid() const noexcept;
};

class ValveDirectory
{
public:
    explicit ValveDirectory(std::filesystem::path root_path);

    const std::filesystem::path& RootPath() const noexcept;
    ValveDirectoryValidationResult Validate(const FileSystem& file_system) const;

private:
    std::filesystem::path root_path_;
};

std::optional<std::filesystem::path> ResolveValveDirectory(
    const FileSystem& file_system,
    const std::optional<std::filesystem::path>& requested_path,
    const std::filesystem::path& executable_path,
    const std::filesystem::path& working_directory,
    std::vector<std::filesystem::path>* checked_candidates = nullptr);
} // namespace hl::filesystem
