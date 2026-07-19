#include "network/udp_socket.h"

#include <array>
#include <cstring>
#include <iomanip>
#include <limits>
#include <sstream>

#include <winsock2.h>
#include <Windows.h>
#include <bcrypt.h>
#include <mstcpip.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "bcrypt.lib")

namespace
{
std::string TrimTrailingWhitespace(std::string value)
{
    while (!value.empty())
    {
        const char tail = value.back();
        if (tail != '\r' && tail != '\n' && tail != ' ' && tail != '\t')
        {
            break;
        }

        value.pop_back();
    }

    return value;
}

std::string DescribeWsaError(int error_code)
{
    std::array<char, 512> buffer{};
    const DWORD length = FormatMessageA(
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        static_cast<DWORD>(error_code),
        0,
        buffer.data(),
        static_cast<DWORD>(buffer.size()),
        nullptr);

    if (length == 0)
    {
        return "Unknown Winsock error";
    }

    return TrimTrailingWhitespace(std::string(buffer.data(), length));
}

std::string MakeWsaError(const std::string& operation, int error_code)
{
    return operation
        + " failed (WSA error "
        + std::to_string(error_code)
        + "): "
        + DescribeWsaError(error_code);
}

std::string FormatEndpoint(const hl::network::Ipv4Endpoint& endpoint)
{
    return std::to_string(endpoint.address[0]) + '.'
        + std::to_string(endpoint.address[1]) + '.'
        + std::to_string(endpoint.address[2]) + '.'
        + std::to_string(endpoint.address[3]) + ':'
        + std::to_string(endpoint.port);
}

sockaddr_in ToSockaddr(const hl::network::Ipv4Endpoint& endpoint)
{
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(endpoint.port);
    static_assert(
        sizeof(address.sin_addr.s_addr) == std::tuple_size<decltype(endpoint.address)>::value,
        "Ipv4Endpoint must contain exactly four address bytes");
    std::memcpy(
        &address.sin_addr.s_addr,
        endpoint.address.data(),
        endpoint.address.size());
    return address;
}

bool TryFromSockaddr(
    const sockaddr_in& address,
    int address_size,
    hl::network::Ipv4Endpoint& endpoint)
{
    if (address_size < static_cast<int>(sizeof(sockaddr_in)) || address.sin_family != AF_INET)
    {
        return false;
    }

    std::memcpy(
        endpoint.address.data(),
        &address.sin_addr.s_addr,
        endpoint.address.size());
    endpoint.port = ntohs(address.sin_port);
    return true;
}

constexpr int kMaximumWinsockBufferSize = (std::numeric_limits<int>::max)();
constexpr DWORD kSioUdpConnectionReset = _WSAIOW(IOC_VENDOR, 12);
} // namespace

namespace hl::network
{
bool SecureRandomUint32(std::uint32_t* value, std::string* error)
{
    if (value == nullptr)
    {
        if (error != nullptr)
        {
            *error = "BCryptGenRandom rejected a null output pointer";
        }
        return false;
    }

    *value = 0;
    const NTSTATUS status = BCryptGenRandom(
        nullptr,
        reinterpret_cast<PUCHAR>(value),
        static_cast<ULONG>(sizeof(*value)),
        BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    if (status != 0)
    {
        if (error != nullptr)
        {
            std::ostringstream message;
            message << "BCryptGenRandom failed (NTSTATUS 0x"
                    << std::hex << std::uppercase
                    << static_cast<unsigned long>(status) << ')';
            *error = message.str();
        }
        return false;
    }

    if (error != nullptr)
    {
        error->clear();
    }
    return true;
}

struct UdpSocket::Impl
{
    Impl()
    {
        WSADATA data{};
        const int startup_result = WSAStartup(MAKEWORD(2, 2), &data);
        if (startup_result != 0)
        {
            last_error = MakeWsaError("WSAStartup", startup_result);
            return;
        }

        winsock_started = true;
        if (LOBYTE(data.wVersion) != 2 || HIBYTE(data.wVersion) != 2)
        {
            last_error = MakeWsaError("WSAStartup version negotiation", WSAVERNOTSUPPORTED);
            WSACleanup();
            winsock_started = false;
        }
    }

    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;

    ~Impl()
    {
        CloseSocket();
        if (winsock_started)
        {
            WSACleanup();
        }
    }

    void CloseSocket() noexcept
    {
        if (socket_handle != INVALID_SOCKET)
        {
            closesocket(socket_handle);
            socket_handle = INVALID_SOCKET;
        }
    }

    bool winsock_started = false;
    SOCKET socket_handle = INVALID_SOCKET;
    Ipv4Endpoint requested_endpoint{};
    Ipv4Endpoint local_endpoint{};
    std::string last_error;
};

UdpSocket::UdpSocket()
    : impl_(std::make_unique<Impl>())
{
}

UdpSocket::UdpSocket(UdpSocket&& other) noexcept = default;

UdpSocket& UdpSocket::operator=(UdpSocket&& other) noexcept = default;

UdpSocket::~UdpSocket() = default;

bool UdpSocket::Open(const Ipv4Endpoint& requested_endpoint)
{
    if (!impl_)
    {
        impl_ = std::make_unique<Impl>();
    }

    impl_->CloseSocket();
    impl_->requested_endpoint = requested_endpoint;
    impl_->local_endpoint = {};

    if (!impl_->winsock_started)
    {
        if (impl_->last_error.empty())
        {
            impl_->last_error = MakeWsaError("WSAStartup", WSANOTINITIALISED);
        }
        return false;
    }

    const SOCKET socket_handle = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (socket_handle == INVALID_SOCKET)
    {
        impl_->last_error = MakeWsaError("socket", WSAGetLastError());
        return false;
    }

    impl_->socket_handle = socket_handle;

    // A Windows UDP socket otherwise reports a received ICMP port-unreachable
    // as WSAECONNRESET on a later recvfrom. A connectionless server must not
    // let a peer that closes its source port terminate the receive service.
    BOOL report_udp_connection_reset = FALSE;
    DWORD control_bytes_returned = 0;
    if (WSAIoctl(
            impl_->socket_handle,
            kSioUdpConnectionReset,
            &report_udp_connection_reset,
            static_cast<DWORD>(sizeof(report_udp_connection_reset)),
            nullptr,
            0,
            &control_bytes_returned,
            nullptr,
            nullptr) == SOCKET_ERROR)
    {
        impl_->last_error = MakeWsaError(
            "WSAIoctl(SIO_UDP_CONNRESET)",
            WSAGetLastError());
        impl_->CloseSocket();
        return false;
    }

    u_long non_blocking = 1;
    if (ioctlsocket(impl_->socket_handle, FIONBIO, &non_blocking) == SOCKET_ERROR)
    {
        impl_->last_error = MakeWsaError("ioctlsocket(FIONBIO)", WSAGetLastError());
        impl_->CloseSocket();
        return false;
    }

    const sockaddr_in bind_address = ToSockaddr(requested_endpoint);
    if (bind(
            impl_->socket_handle,
            reinterpret_cast<const sockaddr*>(&bind_address),
            static_cast<int>(sizeof(bind_address))) == SOCKET_ERROR)
    {
        impl_->last_error = MakeWsaError(
            "bind " + FormatEndpoint(requested_endpoint),
            WSAGetLastError());
        impl_->CloseSocket();
        return false;
    }

    sockaddr_in local_address{};
    int local_address_size = static_cast<int>(sizeof(local_address));
    if (getsockname(
            impl_->socket_handle,
            reinterpret_cast<sockaddr*>(&local_address),
            &local_address_size) == SOCKET_ERROR)
    {
        impl_->last_error = MakeWsaError("getsockname", WSAGetLastError());
        impl_->CloseSocket();
        return false;
    }

    if (!TryFromSockaddr(local_address, local_address_size, impl_->local_endpoint))
    {
        impl_->last_error = MakeWsaError("getsockname address conversion", WSAEAFNOSUPPORT);
        impl_->CloseSocket();
        return false;
    }

    impl_->last_error.clear();
    return true;
}

void UdpSocket::Close() noexcept
{
    if (impl_)
    {
        impl_->CloseSocket();
    }
}

bool UdpSocket::IsOpen() const noexcept
{
    return impl_ && impl_->socket_handle != INVALID_SOCKET;
}

UdpReceiveStatus UdpSocket::Receive(
    std::uint8_t* buffer,
    std::size_t buffer_size,
    std::size_t& received_size,
    Ipv4Endpoint& sender)
{
    received_size = 0;
    sender = {};

    if (!impl_ || impl_->socket_handle == INVALID_SOCKET)
    {
        if (impl_)
        {
            impl_->last_error = MakeWsaError("recvfrom", WSAENOTSOCK);
        }
        return UdpReceiveStatus::Error;
    }

    if (buffer == nullptr && buffer_size != 0)
    {
        impl_->last_error = MakeWsaError("recvfrom", WSAEFAULT);
        return UdpReceiveStatus::Error;
    }

    if (buffer_size > static_cast<std::size_t>(kMaximumWinsockBufferSize))
    {
        impl_->last_error = MakeWsaError("recvfrom", WSAEMSGSIZE);
        return UdpReceiveStatus::Error;
    }

    char zero_capacity_probe = 0;
    char* receive_buffer = buffer_size == 0
        ? &zero_capacity_probe
        : reinterpret_cast<char*>(buffer);
    const int receive_capacity = buffer_size == 0
        ? 1
        : static_cast<int>(buffer_size);

    sockaddr_in source_address{};
    int source_address_size = static_cast<int>(sizeof(source_address));
    const int result = recvfrom(
        impl_->socket_handle,
        receive_buffer,
        receive_capacity,
        0,
        reinterpret_cast<sockaddr*>(&source_address),
        &source_address_size);

    if (result == SOCKET_ERROR)
    {
        const int error_code = WSAGetLastError();
        if (error_code == WSAEWOULDBLOCK)
        {
            impl_->last_error.clear();
            return UdpReceiveStatus::WouldBlock;
        }

        if (error_code == WSAEMSGSIZE)
        {
            TryFromSockaddr(source_address, source_address_size, sender);
            impl_->last_error = MakeWsaError("recvfrom", error_code);
            return UdpReceiveStatus::Oversized;
        }

        impl_->last_error = MakeWsaError("recvfrom", error_code);
        return UdpReceiveStatus::Error;
    }

    if (!TryFromSockaddr(source_address, source_address_size, sender))
    {
        impl_->last_error = MakeWsaError("recvfrom address conversion", WSAEAFNOSUPPORT);
        return UdpReceiveStatus::Error;
    }

    if (buffer_size == 0 && result != 0)
    {
        impl_->last_error = MakeWsaError("recvfrom", WSAEMSGSIZE);
        return UdpReceiveStatus::Oversized;
    }

    received_size = static_cast<std::size_t>(result);
    impl_->last_error.clear();
    return UdpReceiveStatus::Received;
}

bool UdpSocket::Send(
    const std::uint8_t* data,
    std::size_t size,
    const Ipv4Endpoint& destination)
{
    if (!impl_ || impl_->socket_handle == INVALID_SOCKET)
    {
        if (impl_)
        {
            impl_->last_error = MakeWsaError("sendto", WSAENOTSOCK);
        }
        return false;
    }

    if (data == nullptr && size != 0)
    {
        impl_->last_error = MakeWsaError("sendto", WSAEFAULT);
        return false;
    }

    if (size > static_cast<std::size_t>(kMaximumWinsockBufferSize))
    {
        impl_->last_error = MakeWsaError("sendto", WSAEMSGSIZE);
        return false;
    }

    const sockaddr_in destination_address = ToSockaddr(destination);
    const char zero_length_datagram = 0;
    const char* send_buffer = size == 0
        ? &zero_length_datagram
        : reinterpret_cast<const char*>(data);
    const int send_size = static_cast<int>(size);
    const int result = sendto(
        impl_->socket_handle,
        send_buffer,
        send_size,
        0,
        reinterpret_cast<const sockaddr*>(&destination_address),
        static_cast<int>(sizeof(destination_address)));

    if (result == SOCKET_ERROR)
    {
        impl_->last_error = MakeWsaError("sendto", WSAGetLastError());
        return false;
    }

    if (result != send_size)
    {
        impl_->last_error = MakeWsaError("sendto incomplete datagram", WSAEMSGSIZE);
        return false;
    }

    impl_->last_error.clear();
    return true;
}

const Ipv4Endpoint& UdpSocket::RequestedEndpoint() const noexcept
{
    static const Ipv4Endpoint empty_endpoint{};
    return impl_ ? impl_->requested_endpoint : empty_endpoint;
}

const Ipv4Endpoint& UdpSocket::LocalEndpoint() const noexcept
{
    static const Ipv4Endpoint empty_endpoint{};
    return impl_ ? impl_->local_endpoint : empty_endpoint;
}

const std::string& UdpSocket::LastError() const noexcept
{
    static const std::string empty_error;
    return impl_ ? impl_->last_error : empty_error;
}
} // namespace hl::network
