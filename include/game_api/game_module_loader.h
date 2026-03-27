#pragma once

#include <filesystem>
#include <string_view>
#include <string>
#include <vector>

namespace hl::game_api
{
enum class GameModuleKind
{
    Server,
    Client
};

struct GameModuleDescriptor
{
    GameModuleKind kind = GameModuleKind::Server;
    std::wstring display_name;
    std::filesystem::path relative_path;
};

struct ExportProbeResult
{
    std::string name;
    bool found = false;
    bool required = true;
};

struct GameModuleSmokeTestResult
{
    GameModuleDescriptor descriptor;
    std::filesystem::path loaded_path;
    std::vector<std::filesystem::path> candidate_paths;
    std::vector<std::filesystem::path> search_directories;
    std::vector<ExportProbeResult> exports;
    bool loaded = false;
    bool used_fallback = false;
    unsigned long load_error_code = 0;
    std::wstring load_error_message;

    std::size_t FoundExportCount() const noexcept;
    std::size_t RequiredExportCount() const noexcept;
    bool HasAllRequiredExports() const noexcept;
    bool IsSuccessful() const noexcept;
};

class IGameModuleLoader
{
public:
    virtual ~IGameModuleLoader() = default;

    virtual std::wstring DescribePlannedLoad(
        const GameModuleDescriptor& descriptor,
        const std::filesystem::path& game_directory) const = 0;

    virtual GameModuleSmokeTestResult SmokeTestModule(
        const GameModuleDescriptor& descriptor,
        const std::filesystem::path& game_directory) const = 0;
};

class Win32GameModuleLoader final : public IGameModuleLoader
{
public:
    std::wstring DescribePlannedLoad(
        const GameModuleDescriptor& descriptor,
        const std::filesystem::path& game_directory) const override;

    GameModuleSmokeTestResult SmokeTestModule(
        const GameModuleDescriptor& descriptor,
        const std::filesystem::path& game_directory) const override;
};

const std::vector<GameModuleDescriptor>& GetDefaultGameModuleDescriptors();
const std::vector<std::string_view>& GetRequiredExports(GameModuleKind kind);
std::vector<std::filesystem::path> BuildCandidateModulePaths(
    const std::filesystem::path& game_directory,
    const GameModuleDescriptor& descriptor);
std::filesystem::path BuildExpectedModulePath(
    const std::filesystem::path& game_directory,
    const GameModuleDescriptor& descriptor);
} // namespace hl::game_api
