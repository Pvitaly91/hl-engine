#include "common/text_encoding.h"

#include <stdexcept>

#include <Windows.h>

namespace
{
std::runtime_error BuildConversionError(const char* operation_name)
{
    return std::runtime_error(std::string(operation_name) + " failed.");
}
} // namespace

namespace hl::common
{
std::string ToUtf8(std::wstring_view value)
{
    if (value.empty())
    {
        return {};
    }

    const int required_size = WideCharToMultiByte(
        CP_UTF8,
        0,
        value.data(),
        static_cast<int>(value.size()),
        nullptr,
        0,
        nullptr,
        nullptr);
    if (required_size <= 0)
    {
        throw BuildConversionError("WideCharToMultiByte");
    }

    std::string utf8(static_cast<std::size_t>(required_size), '\0');
    const int written_size = WideCharToMultiByte(
        CP_UTF8,
        0,
        value.data(),
        static_cast<int>(value.size()),
        utf8.data(),
        required_size,
        nullptr,
        nullptr);
    if (written_size != required_size)
    {
        throw BuildConversionError("WideCharToMultiByte");
    }

    return utf8;
}

std::string ToUtf8(const std::wstring& value)
{
    return ToUtf8(std::wstring_view(value));
}

std::string ToUtf8(const std::filesystem::path& value)
{
    return ToUtf8(value.native());
}

std::wstring ToWide(std::string_view value)
{
    if (value.empty())
    {
        return {};
    }

    const int required_size = MultiByteToWideChar(
        CP_UTF8,
        0,
        value.data(),
        static_cast<int>(value.size()),
        nullptr,
        0);
    if (required_size <= 0)
    {
        throw BuildConversionError("MultiByteToWideChar");
    }

    std::wstring wide(static_cast<std::size_t>(required_size), L'\0');
    const int written_size = MultiByteToWideChar(
        CP_UTF8,
        0,
        value.data(),
        static_cast<int>(value.size()),
        wide.data(),
        required_size);
    if (written_size != required_size)
    {
        throw BuildConversionError("MultiByteToWideChar");
    }

    return wide;
}

std::wstring ToWide(const std::string& value)
{
    return ToWide(std::string_view(value));
}
} // namespace hl::common
