#pragma once

#include <filesystem>

namespace hl::platform
{
void ConfigureConsoleForUtf8();
std::filesystem::path GetCurrentWorkingDirectory();
std::filesystem::path GetExecutablePath();
} // namespace hl::platform
