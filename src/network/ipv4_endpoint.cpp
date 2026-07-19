#include "network/ipv4_endpoint.h"

#include <charconv>

namespace hl::network
{
std::optional<Ipv4Endpoint> Ipv4Endpoint::Parse(
    std::string_view dotted_decimal_address,
    std::uint16_t endpoint_port)
{
    Ipv4Endpoint endpoint{};
    endpoint.port = endpoint_port;

    std::size_t offset = 0;
    for (std::size_t octet_index = 0; octet_index < endpoint.address.size(); ++octet_index)
    {
        const std::size_t separator = dotted_decimal_address.find('.', offset);
        const bool final_octet = octet_index + 1 == endpoint.address.size();
        if (final_octet != (separator == std::string_view::npos))
        {
            return std::nullopt;
        }

        const std::size_t end = final_octet ? dotted_decimal_address.size() : separator;
        const std::string_view octet_text = dotted_decimal_address.substr(offset, end - offset);
        if (octet_text.empty())
        {
            return std::nullopt;
        }

        unsigned int octet = 0;
        const char* const begin_pointer = octet_text.data();
        const char* const end_pointer = begin_pointer + octet_text.size();
        const auto parse_result = std::from_chars(begin_pointer, end_pointer, octet, 10);
        if (parse_result.ec != std::errc{} || parse_result.ptr != end_pointer || octet > 255u)
        {
            return std::nullopt;
        }

        endpoint.address[octet_index] = static_cast<std::uint8_t>(octet);
        offset = final_octet ? end : end + 1;
    }

    return endpoint;
}

bool Ipv4Endpoint::SameHost(const Ipv4Endpoint& other) const noexcept
{
    return address == other.address;
}

std::string Ipv4Endpoint::ToString() const
{
    return std::to_string(address[0]) + '.'
        + std::to_string(address[1]) + '.'
        + std::to_string(address[2]) + '.'
        + std::to_string(address[3]) + ':'
        + std::to_string(port);
}

bool operator==(const Ipv4Endpoint& left, const Ipv4Endpoint& right) noexcept
{
    return left.address == right.address && left.port == right.port;
}

bool operator!=(const Ipv4Endpoint& left, const Ipv4Endpoint& right) noexcept
{
    return !(left == right);
}
} // namespace hl::network
