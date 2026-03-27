#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace hl::common
{
std::string ToUtf8(std::wstring_view value);
std::string ToUtf8(const std::wstring& value);
std::string ToUtf8(const std::filesystem::path& value);

std::wstring ToWide(std::string_view value);
std::wstring ToWide(const std::string& value);
} // namespace hl::common
