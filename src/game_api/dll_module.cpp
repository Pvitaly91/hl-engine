#include "game_api/dll_module.h"

#include <utility>
#include <vector>

#include "common/logger.h"
#include "common/text_encoding.h"

namespace
{
std::filesystem::path MakeAbsolutePath(const std::filesystem::path& path)
{
    std::error_code error_code;
    const std::filesystem::path absolute_path = std::filesystem::absolute(path, error_code);
    return error_code ? path : absolute_path;
}

constexpr DWORD kLoadLibraryExFlags =
    LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS;

class ScopedDllDirectories final
{
public:
    explicit ScopedDllDirectories(const std::vector<std::filesystem::path>& search_directories)
    {
        for (const std::filesystem::path& directory : search_directories)
        {
            const std::filesystem::path absolute_directory = MakeAbsolutePath(directory);
            hl::common::Logger::Info(
                hl::common::LogCategory::Dll,
                "LoadLibraryExW search directory: " + hl::common::ToUtf8(absolute_directory));

            DLL_DIRECTORY_COOKIE cookie = AddDllDirectory(absolute_directory.c_str());
            if (cookie == nullptr)
            {
                const hl::game_api::Win32ErrorInfo error = hl::game_api::GetLastWin32ErrorInfo();
                hl::common::Logger::Warn(
                    hl::common::LogCategory::Dll,
                    "AddDllDirectory failed for " + hl::common::ToUtf8(absolute_directory)
                    + " (code " + std::to_string(error.code) + "): "
                    + error.message_utf8);
                continue;
            }

            cookies_.push_back(cookie);
        }
    }

    ScopedDllDirectories(const ScopedDllDirectories&) = delete;
    ScopedDllDirectories& operator=(const ScopedDllDirectories&) = delete;

    ~ScopedDllDirectories()
    {
        for (DLL_DIRECTORY_COOKIE cookie : cookies_)
        {
            RemoveDllDirectory(cookie);
        }
    }

private:
    std::vector<DLL_DIRECTORY_COOKIE> cookies_;
};
} // namespace

namespace hl::game_api
{
DllModule::DllModule(DllModule&& other) noexcept
    : module_(other.module_)
    , loaded_path_(std::move(other.loaded_path_))
    , last_load_error_(std::move(other.last_load_error_))
{
    other.module_ = nullptr;
}

DllModule& DllModule::operator=(DllModule&& other) noexcept
{
    if (this == &other)
    {
        return *this;
    }

    Unload();
    module_ = other.module_;
    loaded_path_ = std::move(other.loaded_path_);
    last_load_error_ = std::move(other.last_load_error_);
    other.module_ = nullptr;
    return *this;
}

DllModule::~DllModule()
{
    Unload();
}

bool DllModule::Load(const std::filesystem::path& path)
{
    Unload();

    loaded_path_ = MakeAbsolutePath(path);
    common::Logger::Info(
        common::LogCategory::Dll,
        "LoadLibraryW attempt for " + common::ToUtf8(loaded_path_));
    module_ = LoadLibraryW(loaded_path_.c_str());
    if (module_ == nullptr)
    {
        last_load_error_ = GetLastWin32ErrorInfo();
        common::Logger::Error(
            common::LogCategory::Dll,
            "LoadLibraryW failed for " + common::ToUtf8(loaded_path_)
            + " (code " + std::to_string(last_load_error_.code) + "): "
            + last_load_error_.message_utf8);
        return false;
    }

    last_load_error_ = {};
    common::Logger::Info(
        common::LogCategory::Dll,
        "LoadLibraryW succeeded for " + common::ToUtf8(loaded_path_));
    return true;
}

bool DllModule::LoadWithSearchDirectories(
    const std::filesystem::path& path,
    const std::vector<std::filesystem::path>& search_directories)
{
    Unload();

    loaded_path_ = MakeAbsolutePath(path);
    common::Logger::Info(
        common::LogCategory::Dll,
        "LoadLibraryExW attempt for " + common::ToUtf8(loaded_path_));

    ScopedDllDirectories scoped_search_directories(search_directories);
    module_ = LoadLibraryExW(loaded_path_.c_str(), nullptr, kLoadLibraryExFlags);
    if (module_ == nullptr)
    {
        last_load_error_ = GetLastWin32ErrorInfo();
        common::Logger::Error(
            common::LogCategory::Dll,
            "LoadLibraryExW failed for " + common::ToUtf8(loaded_path_)
            + " (code " + std::to_string(last_load_error_.code) + "): "
            + last_load_error_.message_utf8);

        if (last_load_error_.code == ERROR_MOD_NOT_FOUND)
        {
            common::Logger::Warn(
                common::LogCategory::Dll,
                "LoadLibraryExW returned code 126 for " + common::ToUtf8(loaded_path_)
                + "; likely a dependency DLL is missing, not the target module itself.");
        }

        return false;
    }

    last_load_error_ = {};
    common::Logger::Info(
        common::LogCategory::Dll,
        "LoadLibraryExW succeeded for " + common::ToUtf8(loaded_path_));
    return true;
}

void DllModule::Unload()
{
    if (module_ == nullptr)
    {
        return;
    }

    const std::string loaded_path_utf8 = common::ToUtf8(loaded_path_);
    if (FreeLibrary(module_) == 0)
    {
        const Win32ErrorInfo unload_error = GetLastWin32ErrorInfo();
        common::Logger::Error(
            common::LogCategory::Dll,
            "FreeLibrary failed for " + loaded_path_utf8
            + " (code " + std::to_string(unload_error.code) + "): "
            + unload_error.message_utf8);
    }
    else
    {
        common::Logger::Info(
            common::LogCategory::Dll,
            "FreeLibrary succeeded for " + loaded_path_utf8);
    }

    module_ = nullptr;
    loaded_path_.clear();
}

bool DllModule::IsLoaded() const noexcept
{
    return module_ != nullptr;
}

void* DllModule::GetSymbolRaw(const char* name) const noexcept
{
    if (module_ == nullptr)
    {
        return nullptr;
    }

    return reinterpret_cast<void*>(GetProcAddress(module_, name));
}

HMODULE DllModule::NativeHandle() const noexcept
{
    return module_;
}

const std::filesystem::path& DllModule::LoadedPath() const noexcept
{
    return loaded_path_;
}

const Win32ErrorInfo& DllModule::LastLoadError() const noexcept
{
    return last_load_error_;
}
} // namespace hl::game_api
