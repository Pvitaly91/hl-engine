#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace hl::network
{
struct Ipv4Endpoint final
{
    std::array<std::uint8_t, 4> address{};
    std::uint16_t port = 0;

    static std::optional<Ipv4Endpoint> Parse(
        std::string_view dotted_decimal_address,
        std::uint16_t port);

    bool SameHost(const Ipv4Endpoint& other) const noexcept;
    std::string ToString() const;
};

bool operator==(const Ipv4Endpoint& left, const Ipv4Endpoint& right) noexcept;
bool operator!=(const Ipv4Endpoint& left, const Ipv4Endpoint& right) noexcept;
} // namespace hl::network
