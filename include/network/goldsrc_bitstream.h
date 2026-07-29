#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace hl::network
{
class GoldSrcBitWriter final
{
public:
    GoldSrcBitWriter(std::uint8_t* bytes, std::size_t capacity) noexcept;

    bool WriteBits(std::uint32_t value, std::size_t count) noexcept;
    bool WriteBytes(const std::uint8_t* bytes, std::size_t size) noexcept;
    bool WriteString(std::string_view value) noexcept;
    bool PadToByte() noexcept;

    bool valid() const noexcept;
    std::size_t bit_position() const noexcept;
    std::size_t bytes_written() const noexcept;
    std::size_t capacity_bits() const noexcept;

private:
    std::uint8_t* bytes_ = nullptr;
    std::size_t capacity_bits_ = 0;
    std::size_t bit_position_ = 0;
    bool valid_ = false;
};

class GoldSrcBitReader final
{
public:
    GoldSrcBitReader(const std::uint8_t* bytes, std::size_t size) noexcept;

    bool ReadBits(std::size_t count, std::uint32_t* value) noexcept;
    bool PeekBits(std::size_t count, std::uint32_t* value) const noexcept;
    bool ReadBytes(std::uint8_t* bytes, std::size_t size) noexcept;
    bool ReadString(std::size_t maximum_bytes, std::string* value);
    bool AlignToByte(bool require_zero_padding) noexcept;

    bool valid() const noexcept;
    std::size_t bit_position() const noexcept;
    std::size_t bytes_read() const noexcept;
    std::size_t bits_remaining() const noexcept;

private:
    const std::uint8_t* bytes_ = nullptr;
    std::size_t size_bits_ = 0;
    std::size_t bit_position_ = 0;
    bool valid_ = false;
};
} // namespace hl::network
