#include "network/goldsrc_bitstream.h"

#include <algorithm>
#include <limits>

namespace hl::network
{
GoldSrcBitWriter::GoldSrcBitWriter(
    std::uint8_t* bytes,
    std::size_t capacity) noexcept
    : bytes_(bytes)
{
    if (capacity > std::numeric_limits<std::size_t>::max() / 8u
        || (bytes == nullptr && capacity != 0u))
    {
        return;
    }

    capacity_bits_ = capacity * 8u;
    valid_ = true;
    if (bytes_ != nullptr)
    {
        std::fill_n(bytes_, capacity, std::uint8_t{0});
    }
}

bool GoldSrcBitWriter::WriteBits(
    std::uint32_t value,
    std::size_t count) noexcept
{
    if (!valid_ || count > 32u || bit_position_ > capacity_bits_
        || count > capacity_bits_ - bit_position_)
    {
        return false;
    }

    for (std::size_t bit = 0; bit < count; ++bit)
    {
        if ((value & (std::uint32_t{1} << bit)) != 0u)
        {
            bytes_[bit_position_ / 8u] |= static_cast<std::uint8_t>(
                std::uint8_t{1} << (bit_position_ % 8u));
        }
        ++bit_position_;
    }
    return true;
}

bool GoldSrcBitWriter::WriteBytes(
    const std::uint8_t* bytes,
    std::size_t size) noexcept
{
    if (bytes == nullptr && size != 0u)
    {
        return false;
    }
    for (std::size_t index = 0; index < size; ++index)
    {
        if (!WriteBits(bytes[index], 8u))
        {
            return false;
        }
    }
    return true;
}

bool GoldSrcBitWriter::WriteString(std::string_view value) noexcept
{
    for (const char character : value)
    {
        if (!WriteBits(static_cast<std::uint8_t>(character), 8u))
        {
            return false;
        }
    }
    return WriteBits(0u, 8u);
}

bool GoldSrcBitWriter::PadToByte() noexcept
{
    const std::size_t remainder = bit_position_ % 8u;
    return remainder == 0u || WriteBits(0u, 8u - remainder);
}

bool GoldSrcBitWriter::valid() const noexcept
{
    return valid_;
}

std::size_t GoldSrcBitWriter::bit_position() const noexcept
{
    return bit_position_;
}

std::size_t GoldSrcBitWriter::bytes_written() const noexcept
{
    return bit_position_ / 8u + (bit_position_ % 8u == 0u ? 0u : 1u);
}

std::size_t GoldSrcBitWriter::capacity_bits() const noexcept
{
    return capacity_bits_;
}

GoldSrcBitReader::GoldSrcBitReader(
    const std::uint8_t* bytes,
    std::size_t size) noexcept
    : bytes_(bytes)
{
    if (size > std::numeric_limits<std::size_t>::max() / 8u
        || (bytes == nullptr && size != 0u))
    {
        return;
    }

    size_bits_ = size * 8u;
    valid_ = true;
}

bool GoldSrcBitReader::ReadBits(
    std::size_t count,
    std::uint32_t* value) noexcept
{
    if (!valid_ || value == nullptr || count > 32u
        || bit_position_ > size_bits_ || count > size_bits_ - bit_position_)
    {
        return false;
    }

    std::uint32_t output = 0u;
    for (std::size_t bit = 0; bit < count; ++bit)
    {
        const std::uint8_t source = bytes_[bit_position_ / 8u];
        if ((source
                & static_cast<std::uint8_t>(
                    std::uint8_t{1} << (bit_position_ % 8u)))
            != 0u)
        {
            output |= std::uint32_t{1} << bit;
        }
        ++bit_position_;
    }
    *value = output;
    return true;
}

bool GoldSrcBitReader::PeekBits(
    std::size_t count,
    std::uint32_t* value) const noexcept
{
    if (value == nullptr)
    {
        return false;
    }
    GoldSrcBitReader copy = *this;
    return copy.ReadBits(count, value);
}

bool GoldSrcBitReader::ReadBytes(
    std::uint8_t* bytes,
    std::size_t size) noexcept
{
    if (bytes == nullptr && size != 0u)
    {
        return false;
    }
    for (std::size_t index = 0; index < size; ++index)
    {
        std::uint32_t value = 0u;
        if (!ReadBits(8u, &value))
        {
            return false;
        }
        bytes[index] = static_cast<std::uint8_t>(value);
    }
    return true;
}

bool GoldSrcBitReader::ReadString(
    std::size_t maximum_bytes,
    std::string* value)
{
    if (value == nullptr)
    {
        return false;
    }

    value->clear();
    for (std::size_t index = 0; index <= maximum_bytes; ++index)
    {
        std::uint32_t character = 0u;
        if (!ReadBits(8u, &character))
        {
            value->clear();
            return false;
        }
        if (character == 0u)
        {
            return true;
        }
        if (index == maximum_bytes)
        {
            value->clear();
            return false;
        }
        value->push_back(static_cast<char>(character));
    }
    value->clear();
    return false;
}

bool GoldSrcBitReader::AlignToByte(bool require_zero_padding) noexcept
{
    if (!valid_)
    {
        return false;
    }
    const std::size_t remainder = bit_position_ % 8u;
    if (remainder == 0u)
    {
        return true;
    }

    const std::size_t padding = 8u - remainder;
    std::uint32_t value = 0u;
    return ReadBits(padding, &value)
        && (!require_zero_padding || value == 0u);
}

bool GoldSrcBitReader::valid() const noexcept
{
    return valid_;
}

std::size_t GoldSrcBitReader::bit_position() const noexcept
{
    return bit_position_;
}

std::size_t GoldSrcBitReader::bytes_read() const noexcept
{
    return bit_position_ / 8u + (bit_position_ % 8u == 0u ? 0u : 1u);
}

std::size_t GoldSrcBitReader::bits_remaining() const noexcept
{
    return bit_position_ <= size_bits_ ? size_bits_ - bit_position_ : 0u;
}
} // namespace hl::network
