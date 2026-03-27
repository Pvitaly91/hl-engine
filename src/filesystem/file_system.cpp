#include "filesystem/file_system.h"

#include <fstream>
#include <iterator>

namespace hl::filesystem
{
bool FileSystem::DirectoryExists(const std::filesystem::path& path) const noexcept
{
    std::error_code error_code;
    return std::filesystem::is_directory(path, error_code);
}

bool FileSystem::FileExists(const std::filesystem::path& path) const noexcept
{
    std::error_code error_code;
    return std::filesystem::is_regular_file(path, error_code);
}

std::filesystem::path FileSystem::AbsolutePath(const std::filesystem::path& path) const
{
    return std::filesystem::absolute(path);
}

std::optional<std::vector<unsigned char>> FileSystem::ReadBinaryFile(
    const std::filesystem::path& path) const
{
    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    if (!stream)
    {
        return std::nullopt;
    }

    const std::streamsize size = stream.tellg();
    if (size < 0)
    {
        return std::nullopt;
    }

    std::vector<unsigned char> contents(static_cast<std::size_t>(size));
    stream.seekg(0, std::ios::beg);
    if (!contents.empty())
    {
        stream.read(reinterpret_cast<char*>(contents.data()), size);
        if (!stream)
        {
            return std::nullopt;
        }
    }

    return contents;
}

std::optional<std::string> FileSystem::ReadTextFile(const std::filesystem::path& path) const
{
    std::ifstream stream(path, std::ios::binary);
    if (!stream)
    {
        return std::nullopt;
    }

    return std::string(
        std::istreambuf_iterator<char>(stream),
        std::istreambuf_iterator<char>());
}
} // namespace hl::filesystem
