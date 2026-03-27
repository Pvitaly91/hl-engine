#pragma once

#include <string>

#include <Windows.h>

namespace hl::game_api
{
struct Win32ErrorInfo
{
    DWORD code = ERROR_SUCCESS;
    std::wstring message_wide;
    std::string message_utf8;
};

Win32ErrorInfo DescribeWin32Error(DWORD error_code);
Win32ErrorInfo GetLastWin32ErrorInfo();
} // namespace hl::game_api
