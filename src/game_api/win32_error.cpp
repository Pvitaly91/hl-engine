#include "game_api/win32_error.h"

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
} // namespace

namespace hl::game_api
{
Win32ErrorInfo DescribeWin32Error(DWORD error_code)
{
    Win32ErrorInfo error_info;
    error_info.code = error_code;

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
        error_info.message_wide = L"Unknown Win32 error";
    }
    else
    {
        error_info.message_wide = TrimTrailingWhitespace(std::wstring(buffer, length));
        LocalFree(buffer);
    }

    error_info.message_utf8 = common::ToUtf8(error_info.message_wide);
    return error_info;
}

Win32ErrorInfo GetLastWin32ErrorInfo()
{
    return DescribeWin32Error(GetLastError());
}
} // namespace hl::game_api
