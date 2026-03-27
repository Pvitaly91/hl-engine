#include "game_api/game_module_loader.h"

#include <algorithm>

#include "common/logger.h"
#include "common/text_encoding.h"
#include "game_api/dll_search_directories.h"
#include "game_api/dll_module.h"

namespace
{
struct ExportDefinition
{
    std::string_view name;
    bool required = true;
};

bool FileExists(const std::filesystem::path& path)
{
    std::error_code error_code;
    return std::filesystem::is_regular_file(path, error_code);
}

const std::vector<ExportDefinition>& GetServerExports()
{
    static const std::vector<ExportDefinition> exports = {
        {"GiveFnptrsToDll", true},
        {"GetEntityAPI2", true},
        {"GetNewDLLFunctions", false},
    };
    return exports;
}

const std::vector<ExportDefinition>& GetClientExports()
{
    static const std::vector<ExportDefinition> exports = {
        {"Initialize", true},
        {"HUD_Init", true},
        {"HUD_VidInit", true},
        {"HUD_Frame", true},
        {"HUD_Redraw", true},
    };
    return exports;
}

const std::vector<std::string_view>& GetRequiredServerExports()
{
    static const std::vector<std::string_view> exports = {
        "GiveFnptrsToDll",
        "GetEntityAPI2",
    };
    return exports;
}

const std::vector<std::string_view>& GetRequiredClientExports()
{
    static const std::vector<std::string_view> exports = {
        "Initialize",
        "HUD_Init",
        "HUD_VidInit",
        "HUD_Frame",
        "HUD_Redraw",
    };
    return exports;
}
} // namespace

namespace hl::game_api
{
std::size_t GameModuleSmokeTestResult::FoundExportCount() const noexcept
{
    return static_cast<std::size_t>(std::count_if(
        exports.begin(),
        exports.end(),
        [](const ExportProbeResult& result)
        {
            return result.required && result.found;
        }));
}

std::size_t GameModuleSmokeTestResult::RequiredExportCount() const noexcept
{
    return static_cast<std::size_t>(std::count_if(
        exports.begin(),
        exports.end(),
        [](const ExportProbeResult& result)
        {
            return result.required;
        }));
}

bool GameModuleSmokeTestResult::HasAllRequiredExports() const noexcept
{
    return FoundExportCount() == RequiredExportCount();
}

bool GameModuleSmokeTestResult::IsSuccessful() const noexcept
{
    return loaded && HasAllRequiredExports();
}

std::wstring Win32GameModuleLoader::DescribePlannedLoad(
    const GameModuleDescriptor& descriptor,
    const std::filesystem::path& game_directory) const
{
    const std::vector<std::filesystem::path> module_paths =
        BuildCandidateModulePaths(game_directory, descriptor);

    if (module_paths.size() == 1)
    {
        return L"Planned module target [" + descriptor.display_name + L"]: "
            + module_paths.front().wstring();
    }

    return L"Planned module targets [" + descriptor.display_name + L"]: primary="
        + module_paths.at(0).wstring() + L", fallback=" + module_paths.at(1).wstring();
}

const std::vector<GameModuleDescriptor>& GetDefaultGameModuleDescriptors()
{
    static const std::vector<GameModuleDescriptor> descriptors = {
        {GameModuleKind::Server, L"hl.dll", std::filesystem::path(L"dlls/hl.dll")},
        {GameModuleKind::Client, L"client.dll", std::filesystem::path(L"cl_dlls/client.dll")},
    };

    return descriptors;
}

const std::vector<std::string_view>& GetRequiredExports(GameModuleKind kind)
{
    return kind == GameModuleKind::Server ? GetRequiredServerExports() : GetRequiredClientExports();
}

std::vector<std::filesystem::path> BuildCandidateModulePaths(
    const std::filesystem::path& game_directory,
    const GameModuleDescriptor& descriptor)
{
    if (descriptor.kind == GameModuleKind::Client)
    {
        return {
            game_directory / L"cl_dlls/client.dll",
            game_directory / L"dlls/client.dll",
        };
    }

    return {game_directory / descriptor.relative_path};
}

std::filesystem::path BuildExpectedModulePath(
    const std::filesystem::path& game_directory,
    const GameModuleDescriptor& descriptor)
{
    return BuildCandidateModulePaths(game_directory, descriptor).front();
}

GameModuleSmokeTestResult Win32GameModuleLoader::SmokeTestModule(
    const GameModuleDescriptor& descriptor,
    const std::filesystem::path& game_directory) const
{
    GameModuleSmokeTestResult result;
    result.descriptor = descriptor;
    result.candidate_paths = BuildCandidateModulePaths(game_directory, descriptor);
    for (const std::string_view export_name : GetRequiredExports(descriptor.kind))
    {
        result.exports.push_back({std::string(export_name), false, true});
    }

    common::Logger::Info(common::LogCategory::Dll, DescribePlannedLoad(descriptor, game_directory));

    DllModule module;
    for (std::size_t index = 0; index < result.candidate_paths.size(); ++index)
    {
        const std::filesystem::path& candidate_path = result.candidate_paths[index];
        if (!FileExists(candidate_path))
        {
            common::Logger::Warn(
                common::LogCategory::Dll,
                common::ToUtf8(descriptor.display_name) + " candidate missing: "
                + common::ToUtf8(candidate_path));
            continue;
        }

        const bool use_search_directories = descriptor.kind == GameModuleKind::Client;
        if (use_search_directories)
        {
            result.search_directories = BuildDllSearchDirectories(game_directory, candidate_path);
            common::Logger::Info(
                common::LogCategory::Dll,
                common::ToUtf8(descriptor.display_name) + " dependency search path prepared.");
        }

        const bool loaded = use_search_directories
            ? module.LoadWithSearchDirectories(candidate_path, result.search_directories)
            : module.Load(candidate_path);
        if (!loaded)
        {
            result.loaded_path = candidate_path;
            result.load_error_code = module.LastLoadError().code;
            result.load_error_message = module.LastLoadError().message_wide;
            continue;
        }

        result.loaded = true;
        result.loaded_path = module.LoadedPath();
        result.used_fallback = index > 0;
        break;
    }

    if (!result.loaded)
    {
        if (result.loaded_path.empty() && !result.candidate_paths.empty())
        {
            result.loaded_path = result.candidate_paths.front();
        }

        common::Logger::Error(
            common::LogCategory::Dll,
            common::ToUtf8(descriptor.display_name) + " smoke test failed to load any candidate DLL.");
        return result;
    }

    const std::vector<ExportDefinition>& export_definitions =
        descriptor.kind == GameModuleKind::Server ? GetServerExports() : GetClientExports();
    result.exports.clear();
    for (const ExportDefinition& export_definition : export_definitions)
    {
        const bool found = module.GetSymbolRaw(export_definition.name.data()) != nullptr;
        result.exports.push_back({
            std::string(export_definition.name),
            found,
            export_definition.required,
        });

        if (found)
        {
            common::Logger::Info(
                common::LogCategory::Dll,
                common::ToUtf8(descriptor.display_name) + " export found: "
                + std::string(export_definition.name));
        }
        else if (export_definition.required)
        {
            common::Logger::Warn(
                common::LogCategory::Dll,
                common::ToUtf8(descriptor.display_name) + " export missing: "
                + std::string(export_definition.name));
        }
        else
        {
            common::Logger::Info(
                common::LogCategory::Dll,
                common::ToUtf8(descriptor.display_name) + " optional export missing: "
                + std::string(export_definition.name));
        }
    }

    module.Unload();
    return result;
}
} // namespace hl::game_api
