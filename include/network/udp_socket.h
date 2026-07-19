#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

#include "network/ipv4_endpoint.h"

namespace hl::network
{
// Uses the operating system's preferred cryptographic random-number provider.
bool SecureRandomUint32(std::uint32_t* value, std::string* error);

enum class UdpReceiveStatus
{
    Received,
    WouldBlock,
    Oversized,
    Error,
};

// A non-blocking IPv4 datagram socket. Winsock details remain private to the
// Win32 implementation so users of this header do not need Windows headers.
class UdpSocket final
{
public:
    UdpSocket();
    UdpSocket(const UdpSocket&) = delete;
    UdpSocket& operator=(const UdpSocket&) = delete;
    UdpSocket(UdpSocket&& other) noexcept;
    UdpSocket& operator=(UdpSocket&& other) noexcept;
    ~UdpSocket();

    // Opens and binds exactly the requested endpoint. A requested port of zero
    // asks Winsock to assign a port; LocalEndpoint() exposes the assigned value.
    bool Open(const Ipv4Endpoint& requested_endpoint);
    void Close() noexcept;
    bool IsOpen() const noexcept;

    // On Received, received_size and sender describe the complete datagram.
    // On Oversized, the datagram has been consumed, received_size is zero, and
    // sender is retained when Winsock supplied a valid source address.
    UdpReceiveStatus Receive(
        std::uint8_t* buffer,
        std::size_t buffer_size,
        std::size_t& received_size,
        Ipv4Endpoint& sender);

    // Succeeds only when Winsock accepted the complete datagram.
    bool Send(
        const std::uint8_t* data,
        std::size_t size,
        const Ipv4Endpoint& destination);

    const Ipv4Endpoint& RequestedEndpoint() const noexcept;
    const Ipv4Endpoint& LocalEndpoint() const noexcept;
    const std::string& LastError() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace hl::network
