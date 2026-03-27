#include "platform/environment.h"

#include <string>
#include <stdexcept>
#include <vector>

#include <Windows.h>

#include "common/text_encoding.h"

namespace
{
std::wstring TrimTrailingWhitespace(std::wstring value)
{
    while (!value.empty())
    {
        const wchar_t tail = value.back();
        if (tail != L'\r' && tail != L'\n' && tail != L' ' && tail != L'\t')
        {
            break;
        }

        value.pop_back();
    }

    return value;
}

std::wstring FormatWindowsErrorMessage(DWORD error_code)
{
    LPWSTR buffer = nullptr;
    const DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER
        | FORMAT_MESSAGE_FROM_SYSTEM
        | FORMAT_MESSAGE_IGNORE_INSERTS;
    const DWORD length = FormatMessageW(
        flags,
        nullptr,
        error_code,
        0,
        reinterpret_cast<LPWSTR>(&buffer),
        0,
        nullptr);
    if (length == 0 || buffer == nullptr)
    {
        return L"Unknown Win32 error";
    }

    const std::wstring message(buffer, length);
    LocalFree(buffer);
    return TrimTrailingWhitespace(message);
}

[[noreturn]] void ThrowLastWin32Error(const wchar_t* api_name)
{
    const DWORD error_code = GetLastError();
    const std::wstring message = std::wstring(api_name)
        + L" failed with error "
        + std::to_wstring(error_code)
        + L": "
        + FormatWindowsErrorMessage(error_code);
    throw std::runtime_error(hl::common::ToUtf8(message));
}
} // namespace

namespace hl::platform
{
void ConfigureConsoleForUtf8()
{
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
}

std::filesystem::path GetCurrentWorkingDirectory()
{
    std::vector<wchar_t> buffer(MAX_PATH);

    while (true)
    {
        const DWORD length = GetCurrentDirectoryW(static_cast<DWORD>(buffer.size()), buffer.data());
        if (length == 0)
        {
            ThrowLastWin32Error(L"GetCurrentDirectoryW");
        }

        if (length < buffer.size())
        {
            return std::filesystem::path(std::wstring(buffer.data(), length));
        }

        buffer.resize(static_cast<std::size_t>(length) + 1);
    }
}

std::filesystem::path GetExecutablePath()
{
    std::vector<wchar_t> buffer(MAX_PATH);

    while (true)
    {
        const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length == 0)
        {
            ThrowLastWin32Error(L"GetModuleFileNameW");
        }

        if (length < buffer.size() - 1)
        {
            return std::filesystem::path(std::wstring(buffer.data(), length));
        }

        buffer.resize(buffer.size() * 2);
    }
}
} // namespace hl::platform
