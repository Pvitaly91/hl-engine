#include <windows.h>
#include <winternl.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace
{
constexpr ULONG kSystemExtendedHandleInformation = 64;
constexpr ULONG kObjectNameInformation = 1;
constexpr ULONG kObjectTypeInformation = 2;
constexpr LONG kStatusInfoLengthMismatch = static_cast<LONG>(0xC0000004u);
constexpr LONG kStatusBufferOverflow = static_cast<LONG>(0x80000005u);
constexpr LONG kStatusBufferTooSmall = static_cast<LONG>(0xC0000023u);
constexpr std::size_t kMaximumSystemHandles = 1'000'000u;
constexpr std::size_t kMaximumTargetHandles = 65'536u;
constexpr std::size_t kMaximumNativeBuffer = 128u * 1024u * 1024u;
constexpr std::size_t kMaximumObjectBuffer = 64u * 1024u;

struct NativeHandleEntry
{
    void* object{};
    ULONG_PTR process_id{};
    ULONG_PTR handle_value{};
    ULONG granted_access{};
    USHORT creator_backtrace_index{};
    USHORT object_type_index{};
    ULONG handle_attributes{};
    ULONG reserved{};
};

struct NativeHandleInformation
{
    ULONG_PTR handle_count{};
    ULONG_PTR reserved{};
    NativeHandleEntry handles[1]{};
};

using NtQuerySystemInformationFn = LONG(NTAPI*)(
    ULONG,
    void*,
    ULONG,
    ULONG*);
using NtQueryObjectFn = LONG(NTAPI*)(
    HANDLE,
    ULONG,
    void*,
    ULONG,
    ULONG*);

class UniqueHandle
{
public:
    UniqueHandle() = default;
    explicit UniqueHandle(HANDLE value) noexcept : value_(value) {}
    ~UniqueHandle() { Reset(); }

    UniqueHandle(const UniqueHandle&) = delete;
    UniqueHandle& operator=(const UniqueHandle&) = delete;

    UniqueHandle(UniqueHandle&& other) noexcept : value_(other.Release()) {}
    UniqueHandle& operator=(UniqueHandle&& other) noexcept
    {
        if (this != &other)
        {
            Reset(other.Release());
        }
        return *this;
    }

    HANDLE Get() const noexcept { return value_; }
    explicit operator bool() const noexcept
    {
        return value_ != nullptr && value_ != INVALID_HANDLE_VALUE;
    }
    HANDLE Release() noexcept
    {
        const HANDLE value = value_;
        value_ = nullptr;
        return value;
    }
    void Reset(HANDLE value = nullptr) noexcept
    {
        if (*this)
        {
            CloseHandle(value_);
        }
        value_ = value;
    }

private:
    HANDLE value_{};
};

struct Arguments
{
    DWORD pid{};
    std::wstring expected_image;
    std::optional<std::uint64_t> expected_creation_time;
    std::wstring object_name;
    bool inspect_only{};
    bool close_requested{};
    bool json{};

    bool test_holder{};
    std::wstring ready_file;
    std::wstring shutdown_file;
    std::wstring control_object_name;
    int duplicate_count{1};
};

struct Result
{
    std::string status{"failed"};
    DWORD target_pid{};
    bool target_image_verified{};
    bool target_creation_time_verified{};
    std::wstring requested_object_name;
    std::size_t matched_handle_count{};
    std::size_t mutant_handle_count{};
    std::wstring matched_object_type;
    std::wstring matched_object_name;
    bool inspect_only{};
    bool close_requested{};
    bool handle_closed{};
    DWORD windows_error{};
    std::string blocker{"internal_failure"};
};

std::string NarrowAscii(std::wstring_view value)
{
    std::string converted;
    converted.reserve(value.size());
    for (const wchar_t character : value)
    {
        converted.push_back(
            character >= 0 && character <= 0x7f
                ? static_cast<char>(character)
                : '?');
    }
    return converted;
}

std::string JsonEscape(std::string_view value)
{
    std::ostringstream stream;
    for (const unsigned char character : value)
    {
        switch (character)
        {
        case '"': stream << "\\\""; break;
        case '\\': stream << "\\\\"; break;
        case '\b': stream << "\\b"; break;
        case '\f': stream << "\\f"; break;
        case '\n': stream << "\\n"; break;
        case '\r': stream << "\\r"; break;
        case '\t': stream << "\\t"; break;
        default:
            if (character < 0x20)
            {
                constexpr char digits[] = "0123456789abcdef";
                stream << "\\u00"
                       << digits[(character >> 4u) & 0x0fu]
                       << digits[character & 0x0fu];
            }
            else
            {
                stream << static_cast<char>(character);
            }
            break;
        }
    }
    return stream.str();
}

void PrintResult(const Result& result)
{
    std::cout
        << "{\"status\":\"" << JsonEscape(result.status)
        << "\",\"target_pid\":" << result.target_pid
        << ",\"target_image_verified\":"
        << (result.target_image_verified ? "true" : "false")
        << ",\"target_creation_time_verified\":"
        << (result.target_creation_time_verified ? "true" : "false")
        << ",\"requested_object_name\":\""
        << JsonEscape(NarrowAscii(result.requested_object_name))
        << "\",\"matched_handle_count\":"
        << result.matched_handle_count
        << ",\"mutant_handle_count\":"
        << result.mutant_handle_count
        << ",\"matched_object_type\":\""
        << JsonEscape(NarrowAscii(result.matched_object_type))
        << "\",\"matched_object_name\":\""
        << JsonEscape(NarrowAscii(result.matched_object_name))
        << "\",\"inspect_only\":"
        << (result.inspect_only ? "true" : "false")
        << ",\"close_requested\":"
        << (result.close_requested ? "true" : "false")
        << ",\"handle_closed\":"
        << (result.handle_closed ? "true" : "false")
        << ",\"windows_error\":" << result.windows_error
        << ",\"blocker\":\"" << JsonEscape(result.blocker)
        << "\"}\n";
}

bool ParseUnsigned(std::wstring_view text, std::uint64_t& value)
{
    if (text.empty())
    {
        return false;
    }
    wchar_t* end = nullptr;
    const std::wstring owned(text);
    const unsigned long long parsed = wcstoull(owned.c_str(), &end, 10);
    if (end == nullptr || *end != L'\0')
    {
        return false;
    }
    value = static_cast<std::uint64_t>(parsed);
    return true;
}

std::optional<Arguments> ParseArguments(int argc, wchar_t** argv)
{
    Arguments arguments;
    for (int index = 1; index < argc; ++index)
    {
        const std::wstring_view argument(argv[index]);
        const auto require_value = [&]() -> std::optional<std::wstring>
        {
            if (index + 1 >= argc)
            {
                return std::nullopt;
            }
            return std::wstring(argv[++index]);
        };

        if (argument == L"--pid")
        {
            const auto value = require_value();
            std::uint64_t parsed = 0;
            if (!value || !ParseUnsigned(*value, parsed)
                || parsed == 0 || parsed > MAXDWORD)
            {
                return std::nullopt;
            }
            arguments.pid = static_cast<DWORD>(parsed);
        }
        else if (argument == L"--expected-image")
        {
            const auto value = require_value();
            if (!value) return std::nullopt;
            arguments.expected_image = *value;
        }
        else if (argument == L"--expected-creation-time")
        {
            const auto value = require_value();
            std::uint64_t parsed = 0;
            if (!value || !ParseUnsigned(*value, parsed))
            {
                return std::nullopt;
            }
            arguments.expected_creation_time = parsed;
        }
        else if (argument == L"--object-name")
        {
            const auto value = require_value();
            if (!value) return std::nullopt;
            arguments.object_name = *value;
        }
        else if (argument == L"--inspect-only")
        {
            arguments.inspect_only = true;
        }
        else if (argument == L"--close")
        {
            arguments.close_requested = true;
        }
        else if (argument == L"--json")
        {
            arguments.json = true;
        }
        else if (argument == L"--test-holder")
        {
            arguments.test_holder = true;
        }
        else if (argument == L"--ready-file")
        {
            const auto value = require_value();
            if (!value) return std::nullopt;
            arguments.ready_file = *value;
        }
        else if (argument == L"--shutdown-file")
        {
            const auto value = require_value();
            if (!value) return std::nullopt;
            arguments.shutdown_file = *value;
        }
        else if (argument == L"--control-object-name")
        {
            const auto value = require_value();
            if (!value) return std::nullopt;
            arguments.control_object_name = *value;
        }
        else if (argument == L"--duplicate-count")
        {
            const auto value = require_value();
            std::uint64_t parsed = 0;
            if (!value || !ParseUnsigned(*value, parsed)
                || parsed == 0 || parsed > 2)
            {
                return std::nullopt;
            }
            arguments.duplicate_count = static_cast<int>(parsed);
        }
        else
        {
            return std::nullopt;
        }
    }

    if (arguments.test_holder)
    {
        if (arguments.object_name.empty() || arguments.ready_file.empty()
            || arguments.shutdown_file.empty())
        {
            return std::nullopt;
        }
        return arguments;
    }
    if (arguments.pid == 0 || arguments.expected_image.empty()
        || arguments.object_name.empty() || !arguments.json
        || arguments.inspect_only == arguments.close_requested)
    {
        return std::nullopt;
    }
    return arguments;
}

std::wstring CanonicalPath(std::wstring value)
{
    std::error_code error;
    std::filesystem::path path(value);
    path = std::filesystem::absolute(path, error);
    if (!error)
    {
        path = path.lexically_normal();
        value = path.wstring();
    }
    if (value.rfind(L"\\\\?\\", 0) == 0)
    {
        value.erase(0, 4);
    }
    std::replace(value.begin(), value.end(), L'/', L'\\');
    std::transform(
        value.begin(),
        value.end(),
        value.begin(),
        [](wchar_t character) { return std::towlower(character); });
    return value;
}

std::optional<std::wstring> QueryProcessImage(HANDLE process)
{
    std::wstring buffer(32768, L'\0');
    DWORD size = static_cast<DWORD>(buffer.size());
    if (!QueryFullProcessImageNameW(process, 0, buffer.data(), &size))
    {
        return std::nullopt;
    }
    buffer.resize(size);
    return buffer;
}

std::optional<std::uint64_t> QueryCreationTime(HANDLE process)
{
    FILETIME creation{};
    FILETIME exit{};
    FILETIME kernel{};
    FILETIME user{};
    if (!GetProcessTimes(process, &creation, &exit, &kernel, &user))
    {
        return std::nullopt;
    }
    ULARGE_INTEGER value{};
    value.LowPart = creation.dwLowDateTime;
    value.HighPart = creation.dwHighDateTime;
    return value.QuadPart;
}

std::wstring NormalizeRequestedName(std::wstring value)
{
    std::replace(value.begin(), value.end(), L'/', L'\\');
    while (!value.empty() && value.front() == L'\\')
    {
        value.erase(value.begin());
    }
    if (value.rfind(L"Local\\", 0) == 0
        || value.rfind(L"Global\\", 0) == 0)
    {
        value.erase(0, value.find(L'\\') + 1);
    }
    return value;
}

bool ExactObjectNameMatches(
    std::wstring_view actual,
    std::wstring_view requested)
{
    if (actual == requested)
    {
        return true;
    }
    std::wstring suffix(L"\\");
    suffix.append(requested);
    return actual.size() >= suffix.size()
        && actual.substr(actual.size() - suffix.size()) == suffix;
}

std::optional<std::wstring> QueryObjectString(
    NtQueryObjectFn query,
    HANDLE handle,
    ULONG information_class)
{
    std::size_t size = 512;
    while (size <= kMaximumObjectBuffer)
    {
        std::vector<std::byte> buffer(size);
        ULONG required = 0;
        const LONG status = query(
            handle,
            information_class,
            buffer.data(),
            static_cast<ULONG>(buffer.size()),
            &required);
        if (status >= 0)
        {
            const auto* value = reinterpret_cast<const UNICODE_STRING*>(
                buffer.data());
            if (value->Buffer == nullptr || value->Length == 0)
            {
                return std::wstring();
            }
            return std::wstring(
                value->Buffer,
                value->Length / sizeof(wchar_t));
        }
        if (status != kStatusInfoLengthMismatch
            && status != kStatusBufferOverflow
            && status != kStatusBufferTooSmall)
        {
            return std::nullopt;
        }
        size = (std::max)(size * 2u, static_cast<std::size_t>(required));
    }
    return std::nullopt;
}

std::optional<std::vector<std::byte>> QuerySystemHandles(
    NtQuerySystemInformationFn query)
{
    std::size_t size = 1u * 1024u * 1024u;
    while (size <= kMaximumNativeBuffer)
    {
        std::vector<std::byte> buffer(size);
        ULONG required = 0;
        const LONG status = query(
            kSystemExtendedHandleInformation,
            buffer.data(),
            static_cast<ULONG>(buffer.size()),
            &required);
        if (status >= 0)
        {
            return buffer;
        }
        if (status != kStatusInfoLengthMismatch)
        {
            return std::nullopt;
        }
        size = (std::max)(size * 2u, static_cast<std::size_t>(required));
    }
    return std::nullopt;
}

struct Match
{
    ULONG_PTR handle_value{};
    std::wstring type;
    std::wstring name;
};

int RunUnlocker(const Arguments& arguments)
{
    Result result;
    result.target_pid = arguments.pid;
    result.requested_object_name = arguments.object_name;
    result.inspect_only = arguments.inspect_only;
    result.close_requested = arguments.close_requested;

    UniqueHandle process(OpenProcess(
        PROCESS_DUP_HANDLE | PROCESS_QUERY_LIMITED_INFORMATION
            | SYNCHRONIZE,
        FALSE,
        arguments.pid));
    if (!process)
    {
        result.windows_error = GetLastError();
        result.blocker = result.windows_error == ERROR_ACCESS_DENIED
            ? "access_denied"
            : "target_process_verification_failed";
        PrintResult(result);
        return result.windows_error == ERROR_ACCESS_DENIED ? 5 : 3;
    }

    const auto actual_image = QueryProcessImage(process.Get());
    if (!actual_image
        || CanonicalPath(*actual_image)
            != CanonicalPath(arguments.expected_image))
    {
        result.blocker = "target_image_mismatch";
        PrintResult(result);
        return 3;
    }
    result.target_image_verified = true;

    const auto creation_time = QueryCreationTime(process.Get());
    if (!creation_time)
    {
        result.windows_error = GetLastError();
        result.blocker = "target_creation_time_unavailable";
        PrintResult(result);
        return 3;
    }
    if (arguments.expected_creation_time.has_value())
    {
        if (*creation_time != *arguments.expected_creation_time)
        {
            result.blocker = "target_creation_time_mismatch";
            PrintResult(result);
            return 3;
        }
        result.target_creation_time_verified = true;
    }
    else if (arguments.close_requested)
    {
        result.blocker = "expected_creation_time_required_for_close";
        PrintResult(result);
        return 3;
    }

    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    const auto query_system = reinterpret_cast<NtQuerySystemInformationFn>(
        GetProcAddress(ntdll, "NtQuerySystemInformation"));
    const auto query_object = reinterpret_cast<NtQueryObjectFn>(
        GetProcAddress(ntdll, "NtQueryObject"));
    if (query_system == nullptr || query_object == nullptr)
    {
        result.blocker = "native_query_api_unavailable";
        PrintResult(result);
        return 6;
    }

    UniqueHandle type_probe(CreateMutexW(nullptr, FALSE, nullptr));
    if (!type_probe)
    {
        result.windows_error = GetLastError();
        result.blocker = "mutant_type_probe_failed";
        PrintResult(result);
        return 6;
    }

    const auto native_buffer = QuerySystemHandles(query_system);
    if (!native_buffer)
    {
        result.blocker = "system_handle_query_failed";
        PrintResult(result);
        return 6;
    }
    const auto* information = reinterpret_cast<const NativeHandleInformation*>(
        native_buffer->data());
    if (information->handle_count > kMaximumSystemHandles)
    {
        result.blocker = "system_handle_limit_exceeded";
        PrintResult(result);
        return 6;
    }

    std::optional<USHORT> mutant_type_index;
    const ULONG_PTR probe_value = reinterpret_cast<ULONG_PTR>(
        type_probe.Get());
    for (ULONG_PTR index = 0; index < information->handle_count; ++index)
    {
        const NativeHandleEntry& entry = information->handles[index];
        if (entry.process_id == GetCurrentProcessId()
            && entry.handle_value == probe_value)
        {
            mutant_type_index = entry.object_type_index;
            break;
        }
    }
    if (!mutant_type_index)
    {
        result.blocker = "mutant_type_index_unavailable";
        PrintResult(result);
        return 6;
    }

    const std::wstring requested = NormalizeRequestedName(
        arguments.object_name);
    std::vector<Match> matches;
    std::size_t target_handles = 0;
    bool query_failed = false;
    for (ULONG_PTR index = 0; index < information->handle_count; ++index)
    {
        const NativeHandleEntry& entry = information->handles[index];
        if (entry.process_id != arguments.pid)
        {
            continue;
        }
        if (++target_handles > kMaximumTargetHandles)
        {
            result.blocker = "target_handle_limit_exceeded";
            PrintResult(result);
            return 6;
        }
        if (entry.object_type_index != *mutant_type_index)
        {
            continue;
        }

        HANDLE duplicated_value = nullptr;
        if (!DuplicateHandle(
                process.Get(),
                reinterpret_cast<HANDLE>(entry.handle_value),
                GetCurrentProcess(),
                &duplicated_value,
                0,
                FALSE,
                DUPLICATE_SAME_ACCESS))
        {
            query_failed = true;
            continue;
        }
        UniqueHandle duplicated(duplicated_value);
        const auto type = QueryObjectString(
            query_object,
            duplicated.Get(),
            kObjectTypeInformation);
        if (!type)
        {
            query_failed = true;
            continue;
        }
        if (*type != L"Mutant")
        {
            continue;
        }
        ++result.mutant_handle_count;
        const auto name = QueryObjectString(
            query_object,
            duplicated.Get(),
            kObjectNameInformation);
        if (!name)
        {
            query_failed = true;
            continue;
        }
        if (!name->empty() && ExactObjectNameMatches(*name, requested))
        {
            matches.push_back({entry.handle_value, *type, *name});
        }
    }

    result.matched_handle_count = matches.size();
    if (matches.empty())
    {
        result.status = "not_found";
        result.blocker = query_failed
            ? "handle_query_failed"
            : "mutex_not_found";
        PrintResult(result);
        return query_failed ? 6 : 2;
    }
    if (matches.size() != 1u)
    {
        result.status = "ambiguous";
        result.blocker = "ambiguous_mutex_matches";
        PrintResult(result);
        return 4;
    }

    const Match& match = matches.front();
    result.matched_object_type = match.type;
    result.matched_object_name = match.name;
    if (arguments.inspect_only)
    {
        result.status = "inspected";
        result.blocker = "none";
        PrintResult(result);
        return 0;
    }

    HANDLE verification_value = nullptr;
    if (!DuplicateHandle(
            process.Get(),
            reinterpret_cast<HANDLE>(match.handle_value),
            GetCurrentProcess(),
            &verification_value,
            0,
            FALSE,
            DUPLICATE_SAME_ACCESS))
    {
        result.windows_error = GetLastError();
        result.blocker = "matched_handle_revalidation_failed";
        PrintResult(result);
        return 7;
    }
    UniqueHandle verification(verification_value);
    const auto verification_type = QueryObjectString(
        query_object,
        verification.Get(),
        kObjectTypeInformation);
    const auto verification_name = verification_type
            && *verification_type == L"Mutant"
        ? QueryObjectString(
            query_object,
            verification.Get(),
            kObjectNameInformation)
        : std::nullopt;
    if (!verification_type || !verification_name
        || *verification_type != L"Mutant"
        || !ExactObjectNameMatches(*verification_name, requested))
    {
        result.blocker = "matched_handle_identity_changed";
        PrintResult(result);
        return 7;
    }

    HANDLE closed_duplicate_value = nullptr;
    if (!DuplicateHandle(
            process.Get(),
            reinterpret_cast<HANDLE>(match.handle_value),
            GetCurrentProcess(),
            &closed_duplicate_value,
            0,
            FALSE,
            DUPLICATE_SAME_ACCESS | DUPLICATE_CLOSE_SOURCE))
    {
        result.windows_error = GetLastError();
        result.blocker = "handle_close_failed";
        PrintResult(result);
        return 7;
    }
    UniqueHandle closed_duplicate(closed_duplicate_value);
    result.status = "closed";
    result.handle_closed = true;
    result.blocker = "none";
    PrintResult(result);
    return 0;
}

bool WriteTextAtomically(
    const std::filesystem::path& path,
    const std::string& text)
{
    const std::filesystem::path temporary = path.wstring() + L".tmp";
    {
        std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
        if (!stream)
        {
            return false;
        }
        stream.write(text.data(), static_cast<std::streamsize>(text.size()));
        if (!stream)
        {
            return false;
        }
    }
    std::error_code error;
    std::filesystem::remove(path, error);
    error.clear();
    std::filesystem::rename(temporary, path, error);
    return !error;
}

int RunTestHolder(const Arguments& arguments)
{
    std::vector<UniqueHandle> mutexes;
    for (int index = 0; index < arguments.duplicate_count; ++index)
    {
        UniqueHandle mutex(CreateMutexW(
            nullptr,
            FALSE,
            arguments.object_name.c_str()));
        if (!mutex)
        {
            return 20;
        }
        mutexes.push_back(std::move(mutex));
    }
    UniqueHandle control;
    if (!arguments.control_object_name.empty())
    {
        control.Reset(CreateMutexW(
            nullptr,
            FALSE,
            arguments.control_object_name.c_str()));
        if (!control)
        {
            return 21;
        }
    }
    UniqueHandle self(OpenProcess(
        PROCESS_QUERY_LIMITED_INFORMATION,
        FALSE,
        GetCurrentProcessId()));
    const auto creation = QueryCreationTime(self.Get());
    if (!creation)
    {
        return 22;
    }
    std::ostringstream ready;
    ready << "{\"pid\":" << GetCurrentProcessId()
          << ",\"creation_time\":" << *creation
          << ",\"handle_count\":" << mutexes.size()
          << "}\n";
    if (!WriteTextAtomically(arguments.ready_file, ready.str()))
    {
        return 23;
    }
    const auto deadline = std::chrono::steady_clock::now()
        + std::chrono::minutes(5);
    while (std::chrono::steady_clock::now() < deadline)
    {
        if (std::filesystem::exists(arguments.shutdown_file))
        {
            return 0;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    return 24;
}
} // namespace

int wmain(int argc, wchar_t** argv)
{
    const auto arguments = ParseArguments(argc, argv);
    if (!arguments)
    {
        Result result;
        result.blocker = "invalid_arguments";
        PrintResult(result);
        return 8;
    }
    if (arguments->test_holder)
    {
        return RunTestHolder(*arguments);
    }
    return RunUnlocker(*arguments);
}
