#pragma once

#include <filesystem>
#include <vector>

#include <Windows.h>

#include "game_api/win32_error.h"

namespace hl::game_api
{
class DllModule final
{
public:
    DllModule() noexcept = default;
    DllModule(const DllModule&) = delete;
    DllModule& operator=(const DllModule&) = delete;
    DllModule(DllModule&& other) noexcept;
    DllModule& operator=(DllModule&& other) noexcept;
    ~DllModule();

    bool Load(const std::filesystem::path& path);
    bool LoadWithSearchDirectories(
        const std::filesystem::path& path,
        const std::vector<std::filesystem::path>& search_directories);
    void Unload();
    bool IsLoaded() const noexcept;
    void* GetSymbolRaw(const char* name) const noexcept;
    HMODULE NativeHandle() const noexcept;

    const std::filesystem::path& LoadedPath() const noexcept;
    const Win32ErrorInfo& LastLoadError() const noexcept;

private:
    HMODULE module_ = nullptr;
    std::filesystem::path loaded_path_;
    Win32ErrorInfo last_load_error_;
};
} // namespace hl::game_api
