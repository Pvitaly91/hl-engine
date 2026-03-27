#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace hl::filesystem
{
class FileSystem
{
public:
    bool DirectoryExists(const std::filesystem::path& path) const noexcept;
    bool FileExists(const std::filesystem::path& path) const noexcept;
    std::filesystem::path AbsolutePath(const std::filesystem::path& path) const;
    std::optional<std::vector<unsigned char>> ReadBinaryFile(
        const std::filesystem::path& path) const;
    std::optional<std::string> ReadTextFile(const std::filesystem::path& path) const;
};
} // namespace hl::filesystem
