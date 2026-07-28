#include "network/goldsrc_signon.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>

namespace hl::network
{
namespace
{
constexpr std::array<std::uint8_t, 16> kMungeTable3 = {
    0x20u, 0x07u, 0x13u, 0x61u,
    0x03u, 0x45u, 0x17u, 0x72u,
    0x0Au, 0x2Du, 0x48u, 0x0Cu,
    0x4Au, 0x12u, 0xA9u, 0xB5u,
};

std::uint32_t ReadLittleEndian32(const std::uint8_t* bytes) noexcept
{
    return static_cast<std::uint32_t>(bytes[0])
        | (static_cast<std::uint32_t>(bytes[1]) << 8u)
        | (static_cast<std::uint32_t>(bytes[2]) << 16u)
        | (static_cast<std::uint32_t>(bytes[3]) << 24u);
}

void WriteLittleEndian32(std::uint8_t* bytes, std::uint32_t value) noexcept
{
    bytes[0] = static_cast<std::uint8_t>(value & 0xFFu);
    bytes[1] = static_cast<std::uint8_t>((value >> 8u) & 0xFFu);
    bytes[2] = static_cast<std::uint8_t>((value >> 16u) & 0xFFu);
    bytes[3] = static_cast<std::uint8_t>((value >> 24u) & 0xFFu);
}

std::uint32_t ByteSwap32(std::uint32_t value) noexcept
{
    return ((value & 0x000000FFu) << 24u)
        | ((value & 0x0000FF00u) << 8u)
        | ((value & 0x00FF0000u) >> 8u)
        | ((value & 0xFF000000u) >> 24u);
}

bool HasInvalidControlByte(std::string_view value) noexcept
{
    return std::any_of(
        value.begin(),
        value.end(),
        [](char character)
        {
            const auto byte = static_cast<unsigned char>(character);
            return (byte < 0x20u && byte != 0u) || byte == 0x7Fu;
        });
}

bool IsValidMapModelPath(std::string_view model_path) noexcept
{
    constexpr std::string_view kMapsPrefix = "maps/";
    constexpr std::string_view kBspSuffix = ".bsp";
    const bool has_expected_prefix = model_path.size() >= kMapsPrefix.size()
        && model_path.substr(0u, kMapsPrefix.size()) == kMapsPrefix;
    const bool has_expected_suffix = model_path.size() >= kBspSuffix.size()
        && model_path.substr(model_path.size() - kBspSuffix.size())
            == kBspSuffix;
    return has_expected_prefix
        && has_expected_suffix
        && model_path.size() > kMapsPrefix.size() + kBspSuffix.size()
        && model_path.find('\\') == std::string_view::npos
        && model_path.find("..") == std::string_view::npos;
}

bool IsAllZero(
    const std::array<std::uint8_t, kGoldSrcClientDllDigestBytes>& bytes)
    noexcept
{
    return std::all_of(
        bytes.begin(),
        bytes.end(),
        [](std::uint8_t byte)
        {
            return byte == 0u;
        });
}

GoldSrcServerInfoCodecStatus ValidateString(
    std::string_view value,
    std::size_t maximum_size,
    bool required) noexcept
{
    if (required && value.empty())
    {
        return GoldSrcServerInfoCodecStatus::kEmptyRequiredString;
    }
    if (value.size() > maximum_size)
    {
        return GoldSrcServerInfoCodecStatus::kStringTooLong;
    }
    if (value.find('\0') != std::string_view::npos)
    {
        return GoldSrcServerInfoCodecStatus::kEmbeddedNul;
    }
    if (HasInvalidControlByte(value))
    {
        return GoldSrcServerInfoCodecStatus::kInvalidControlByte;
    }
    return GoldSrcServerInfoCodecStatus::kOk;
}

template <typename Payload>
class PayloadWriter final
{
public:
    explicit PayloadWriter(Payload* payload) noexcept
        : payload_(payload)
    {
    }

    bool WriteByte(std::uint8_t value) noexcept
    {
        if (payload_ == nullptr || payload_->size == payload_->bytes.size())
        {
            return false;
        }
        payload_->bytes[payload_->size++] = value;
        return true;
    }

    bool WriteLittleEndian32(std::uint32_t value) noexcept
    {
        if (payload_ == nullptr
            || value_size_ > payload_->bytes.size() - payload_->size)
        {
            return false;
        }
        hl::network::WriteLittleEndian32(
            payload_->bytes.data() + payload_->size,
            value);
        payload_->size += value_size_;
        return true;
    }

    bool WriteLittleEndian16(std::uint16_t value) noexcept
    {
        return WriteByte(static_cast<std::uint8_t>(value & 0xFFu))
            && WriteByte(static_cast<std::uint8_t>((value >> 8u) & 0xFFu));
    }

    bool WriteFloat(float value) noexcept
    {
        static_assert(sizeof(float) == sizeof(std::uint32_t));
        std::uint32_t bits = 0u;
        std::memcpy(&bits, &value, sizeof(bits));
        return WriteLittleEndian32(bits);
    }

    bool WriteBytes(const std::uint8_t* bytes, std::size_t size) noexcept
    {
        if (payload_ == nullptr || (bytes == nullptr && size != 0u)
            || size > payload_->bytes.size() - payload_->size)
        {
            return false;
        }
        if (size != 0u)
        {
            std::memcpy(payload_->bytes.data() + payload_->size, bytes, size);
            payload_->size += size;
        }
        return true;
    }

    bool WriteString(std::string_view value) noexcept
    {
        return WriteBytes(
                   reinterpret_cast<const std::uint8_t*>(value.data()),
                   value.size())
            && WriteByte(0u);
    }

private:
    static constexpr std::size_t value_size_ = 4u;
    Payload* payload_ = nullptr;
};

class PayloadReader final
{
public:
    PayloadReader(const std::uint8_t* bytes, std::size_t size) noexcept
        : bytes_(bytes), size_(size)
    {
    }

    bool ReadByte(std::uint8_t* value) noexcept
    {
        if (value == nullptr || offset_ == size_)
        {
            return false;
        }
        *value = bytes_[offset_++];
        return true;
    }

    bool ReadLittleEndian32(std::uint32_t* value) noexcept
    {
        if (value == nullptr || Remaining() < 4u)
        {
            return false;
        }
        *value = hl::network::ReadLittleEndian32(bytes_ + offset_);
        offset_ += 4u;
        return true;
    }

    bool ReadBytes(std::uint8_t* destination, std::size_t size) noexcept
    {
        if ((destination == nullptr && size != 0u) || Remaining() < size)
        {
            return false;
        }
        if (size != 0u)
        {
            std::memcpy(destination, bytes_ + offset_, size);
            offset_ += size;
        }
        return true;
    }

    template <std::size_t Capacity>
    GoldSrcServerInfoDecodeStatus ReadString(
        GoldSrcBoundedProtocolString<Capacity>* destination,
        bool required) noexcept
    {
        if (destination == nullptr)
        {
            return GoldSrcServerInfoDecodeStatus::kTruncatedField;
        }

        const std::size_t available = Remaining();
        const std::size_t scan_size =
            std::min(available, Capacity + 1u);
        std::size_t string_size = 0u;
        while (string_size < scan_size
            && bytes_[offset_ + string_size] != 0u)
        {
            ++string_size;
        }

        if (string_size == scan_size)
        {
            return available > Capacity
                ? GoldSrcServerInfoDecodeStatus::kStringTooLong
                : GoldSrcServerInfoDecodeStatus::kMissingStringTerminator;
        }
        if (required && string_size == 0u)
        {
            return GoldSrcServerInfoDecodeStatus::kEmptyRequiredString;
        }

        const std::string_view value(
            reinterpret_cast<const char*>(bytes_ + offset_),
            string_size);
        if (HasInvalidControlByte(value))
        {
            return GoldSrcServerInfoDecodeStatus::kInvalidControlByte;
        }

        if (string_size != 0u)
        {
            std::memcpy(destination->bytes.data(), value.data(), string_size);
        }
        destination->bytes[string_size] = '\0';
        destination->size = string_size;
        offset_ += string_size + 1u;
        return GoldSrcServerInfoDecodeStatus::kOk;
    }

    std::size_t Remaining() const noexcept
    {
        return size_ - offset_;
    }

private:
    const std::uint8_t* bytes_ = nullptr;
    std::size_t size_ = 0;
    std::size_t offset_ = 0;
};

constexpr std::array<std::uint32_t, 256> BuildCrc32Table() noexcept
{
    std::array<std::uint32_t, 256> table{};
    for (std::uint32_t index = 0; index < table.size(); ++index)
    {
        std::uint32_t value = index;
        for (unsigned bit = 0; bit < 8u; ++bit)
        {
            value = (value >> 1u)
                ^ ((value & 1u) != 0u ? 0xEDB88320u : 0u);
        }
        table[index] = value;
    }
    return table;
}

constexpr std::array<std::uint32_t, 256> kCrc32Table = BuildCrc32Table();

void ProcessCrc32(
    std::uint32_t* checksum,
    const std::uint8_t* bytes,
    std::size_t size) noexcept
{
    std::uint32_t value = *checksum;
    for (std::size_t index = 0; index < size; ++index)
    {
        value = kCrc32Table[(value ^ bytes[index]) & 0xFFu]
            ^ (value >> 8u);
    }
    *checksum = value;
}

struct BspLump final
{
    std::size_t offset = 0;
    std::size_t size = 0;
};

GoldSrcMapChecksumStatus ParseBsp30Header(
    const std::uint8_t* header,
    std::size_t header_size,
    std::uint64_t file_size,
    std::array<BspLump, kGoldSrcBspLumpCount>* lumps) noexcept
{
    if (header == nullptr || lumps == nullptr)
    {
        return GoldSrcMapChecksumStatus::kNullInput;
    }
    if (header_size < kGoldSrcBsp30HeaderBytes)
    {
        return GoldSrcMapChecksumStatus::kTruncatedHeader;
    }
    if (ReadLittleEndian32(header) != 30u)
    {
        return GoldSrcMapChecksumStatus::kUnsupportedBspVersion;
    }

    for (std::size_t index = 0; index < lumps->size(); ++index)
    {
        const std::uint8_t* descriptor = header + 4u + (index * 8u);
        const std::uint32_t raw_offset = ReadLittleEndian32(descriptor);
        const std::uint32_t raw_size = ReadLittleEndian32(descriptor + 4u);
        if (raw_offset > static_cast<std::uint32_t>(
                std::numeric_limits<std::int32_t>::max())
            || raw_size > static_cast<std::uint32_t>(
                std::numeric_limits<std::int32_t>::max()))
        {
            return GoldSrcMapChecksumStatus::kInvalidLumpBounds;
        }

        const std::uint64_t offset = raw_offset;
        const std::uint64_t size = raw_size;
        if (offset > file_size || size > file_size - offset)
        {
            return GoldSrcMapChecksumStatus::kInvalidLumpBounds;
        }
        (*lumps)[index] = {
            static_cast<std::size_t>(offset),
            static_cast<std::size_t>(size),
        };
    }
    return GoldSrcMapChecksumStatus::kOk;
}

constexpr std::array<std::uint32_t, 64> kMd5Shift = {
    7u, 12u, 17u, 22u, 7u, 12u, 17u, 22u,
    7u, 12u, 17u, 22u, 7u, 12u, 17u, 22u,
    5u, 9u, 14u, 20u, 5u, 9u, 14u, 20u,
    5u, 9u, 14u, 20u, 5u, 9u, 14u, 20u,
    4u, 11u, 16u, 23u, 4u, 11u, 16u, 23u,
    4u, 11u, 16u, 23u, 4u, 11u, 16u, 23u,
    6u, 10u, 15u, 21u, 6u, 10u, 15u, 21u,
    6u, 10u, 15u, 21u, 6u, 10u, 15u, 21u,
};

constexpr std::array<std::uint32_t, 64> kMd5Constant = {
    0xd76aa478u, 0xe8c7b756u, 0x242070dbu, 0xc1bdceeeu,
    0xf57c0fafu, 0x4787c62au, 0xa8304613u, 0xfd469501u,
    0x698098d8u, 0x8b44f7afu, 0xffff5bb1u, 0x895cd7beu,
    0x6b901122u, 0xfd987193u, 0xa679438eu, 0x49b40821u,
    0xf61e2562u, 0xc040b340u, 0x265e5a51u, 0xe9b6c7aau,
    0xd62f105du, 0x02441453u, 0xd8a1e681u, 0xe7d3fbc8u,
    0x21e1cde6u, 0xc33707d6u, 0xf4d50d87u, 0x455a14edu,
    0xa9e3e905u, 0xfcefa3f8u, 0x676f02d9u, 0x8d2a4c8au,
    0xfffa3942u, 0x8771f681u, 0x6d9d6122u, 0xfde5380cu,
    0xa4beea44u, 0x4bdecfa9u, 0xf6bb4b60u, 0xbebfbc70u,
    0x289b7ec6u, 0xeaa127fau, 0xd4ef3085u, 0x04881d05u,
    0xd9d4d039u, 0xe6db99e5u, 0x1fa27cf8u, 0xc4ac5665u,
    0xf4292244u, 0x432aff97u, 0xab9423a7u, 0xfc93a039u,
    0x655b59c3u, 0x8f0ccc92u, 0xffeff47du, 0x85845dd1u,
    0x6fa87e4fu, 0xfe2ce6e0u, 0xa3014314u, 0x4e0811a1u,
    0xf7537e82u, 0xbd3af235u, 0x2ad7d2bbu, 0xeb86d391u,
};

std::uint32_t RotateLeft(std::uint32_t value, std::uint32_t shift) noexcept
{
    return (value << shift) | (value >> (32u - shift));
}

class Md5Accumulator final
{
public:
    void Update(const std::uint8_t* bytes, std::size_t size) noexcept
    {
        bit_count_ += static_cast<std::uint64_t>(size) * 8u;
        if (buffer_size_ != 0u)
        {
            const std::size_t copied = std::min(size, buffer_.size() - buffer_size_);
            std::memcpy(buffer_.data() + buffer_size_, bytes, copied);
            buffer_size_ += copied;
            bytes += copied;
            size -= copied;
            if (buffer_size_ == buffer_.size())
            {
                Transform(buffer_.data());
                buffer_size_ = 0u;
            }
        }

        while (size >= buffer_.size())
        {
            Transform(bytes);
            bytes += buffer_.size();
            size -= buffer_.size();
        }
        if (size != 0u)
        {
            std::memcpy(buffer_.data(), bytes, size);
            buffer_size_ = size;
        }
    }

    std::array<std::uint8_t, kGoldSrcClientDllDigestBytes> Finalize() noexcept
    {
        const std::uint64_t original_bit_count = bit_count_;
        std::array<std::uint8_t, 64> padding{};
        padding[0] = 0x80u;
        const std::size_t padding_size = buffer_size_ < 56u
            ? 56u - buffer_size_
            : 120u - buffer_size_;
        Update(padding.data(), padding_size);

        std::array<std::uint8_t, 8> encoded_length{};
        for (std::size_t index = 0; index < encoded_length.size(); ++index)
        {
            encoded_length[index] = static_cast<std::uint8_t>(
                (original_bit_count >> (index * 8u)) & 0xFFu);
        }
        Update(encoded_length.data(), encoded_length.size());

        std::array<std::uint8_t, kGoldSrcClientDllDigestBytes> digest{};
        for (std::size_t index = 0; index < state_.size(); ++index)
        {
            WriteLittleEndian32(digest.data() + (index * 4u), state_[index]);
        }
        return digest;
    }

private:
    void Transform(const std::uint8_t* block) noexcept
    {
        std::array<std::uint32_t, 16> words{};
        for (std::size_t index = 0; index < words.size(); ++index)
        {
            words[index] = ReadLittleEndian32(block + (index * 4u));
        }

        std::uint32_t a = state_[0];
        std::uint32_t b = state_[1];
        std::uint32_t c = state_[2];
        std::uint32_t d = state_[3];
        for (std::uint32_t index = 0; index < 64u; ++index)
        {
            std::uint32_t function = 0;
            std::uint32_t word_index = 0;
            if (index < 16u)
            {
                function = (b & c) | (~b & d);
                word_index = index;
            }
            else if (index < 32u)
            {
                function = (d & b) | (~d & c);
                word_index = (5u * index + 1u) & 0x0Fu;
            }
            else if (index < 48u)
            {
                function = b ^ c ^ d;
                word_index = (3u * index + 5u) & 0x0Fu;
            }
            else
            {
                function = c ^ (b | ~d);
                word_index = (7u * index) & 0x0Fu;
            }

            const std::uint32_t previous_d = d;
            d = c;
            c = b;
            b += RotateLeft(
                a + function + kMd5Constant[index] + words[word_index],
                kMd5Shift[index]);
            a = previous_d;
        }

        state_[0] += a;
        state_[1] += b;
        state_[2] += c;
        state_[3] += d;
    }

    std::array<std::uint32_t, 4> state_ = {
        0x67452301u,
        0xefcdab89u,
        0x98badcfeu,
        0x10325476u,
    };
    std::uint64_t bit_count_ = 0;
    std::array<std::uint8_t, 64> buffer_{};
    std::size_t buffer_size_ = 0;
};
} // namespace

std::string_view ReasonFor(GoldSrcClientSignonDecodeStatus status) noexcept
{
    switch (status)
    {
    case GoldSrcClientSignonDecodeStatus::kOk:
        return "ok";
    case GoldSrcClientSignonDecodeStatus::kNullInput:
        return "null_input";
    case GoldSrcClientSignonDecodeStatus::kEmptyPayload:
        return "empty_payload";
    case GoldSrcClientSignonDecodeStatus::kPayloadTooLarge:
        return "payload_too_large";
    case GoldSrcClientSignonDecodeStatus::kMissingCommand:
        return "missing_command";
    case GoldSrcClientSignonDecodeStatus::kUnsupportedOpcode:
        return "unsupported_opcode";
    case GoldSrcClientSignonDecodeStatus::kMissingStringTerminator:
        return "missing_string_terminator";
    case GoldSrcClientSignonDecodeStatus::kCommandTooLong:
        return "command_too_long";
    case GoldSrcClientSignonDecodeStatus::kEmptyCommand:
        return "empty_command";
    case GoldSrcClientSignonDecodeStatus::kInvalidControlByte:
        return "invalid_control_byte";
    case GoldSrcClientSignonDecodeStatus::kUnsupportedCommand:
        return "unsupported_command";
    case GoldSrcClientSignonDecodeStatus::kMultipleCommands:
        return "multiple_commands";
    case GoldSrcClientSignonDecodeStatus::kUnsupportedCompanionCommand:
        return "unsupported_companion_command";
    case GoldSrcClientSignonDecodeStatus::kUnsupportedCompanionCount:
        return "unsupported_companion_count";
    case GoldSrcClientSignonDecodeStatus::kTooManyCompanionCommands:
        return "too_many_companion_commands";
    case GoldSrcClientSignonDecodeStatus::kUnsupportedTrailingData:
        return "unsupported_trailing_data";
    default:
        return "invalid_client_signon_message";
    }
}

GoldSrcClientSignonDecodeResult DecodeGoldSrcClientSignonPayload(
    const std::uint8_t* bytes,
    std::size_t size) noexcept
{
    GoldSrcClientSignonDecodeResult result{};
    if (bytes == nullptr)
    {
        result.status = size == 0u
            ? GoldSrcClientSignonDecodeStatus::kEmptyPayload
            : GoldSrcClientSignonDecodeStatus::kNullInput;
        return result;
    }
    if (size == 0u)
    {
        result.status = GoldSrcClientSignonDecodeStatus::kEmptyPayload;
        return result;
    }
    if (size > kGoldSrcMaximumSignonPayloadBytes)
    {
        result.status = GoldSrcClientSignonDecodeStatus::kPayloadTooLarge;
        return result;
    }

    std::size_t cursor = 0;
    while (cursor < size && bytes[cursor] == kGoldSrcClientNopOpcode)
    {
        ++cursor;
        ++result.leading_nop_count;
    }
    if (cursor == size)
    {
        result.status = GoldSrcClientSignonDecodeStatus::kMissingCommand;
        return result;
    }
    if (bytes[cursor] != kGoldSrcClientStringCommandOpcode)
    {
        result.status = GoldSrcClientSignonDecodeStatus::kUnsupportedOpcode;
        return result;
    }
    ++cursor;

    const std::size_t command_begin = cursor;
    while (cursor < size && bytes[cursor] != 0u)
    {
        if (cursor - command_begin >= kGoldSrcMaximumClientCommandBytes)
        {
            result.status = GoldSrcClientSignonDecodeStatus::kCommandTooLong;
            return result;
        }
        const std::uint8_t byte = bytes[cursor];
        constexpr std::array<std::uint8_t, 10> kDropClientText = {
            'd', 'r', 'o', 'p', 'c', 'l', 'i', 'e', 'n', 't',
        };
        const bool exact_dropclient_line_feed =
            byte == '\n'
            && cursor - command_begin == kDropClientText.size()
            && cursor + 1u < size
            && bytes[cursor + 1u] == 0u
            && std::equal(
                kDropClientText.begin(),
                kDropClientText.end(),
                bytes + command_begin);
        if ((byte < 0x20u && !exact_dropclient_line_feed)
            || byte == 0x7Fu)
        {
            result.status = GoldSrcClientSignonDecodeStatus::kInvalidControlByte;
            return result;
        }
        ++cursor;
    }
    if (cursor == size)
    {
        result.status = GoldSrcClientSignonDecodeStatus::kMissingStringTerminator;
        return result;
    }

    const std::size_t command_size = cursor - command_begin;
    if (command_size == 0u)
    {
        result.status = GoldSrcClientSignonDecodeStatus::kEmptyCommand;
        return result;
    }
    constexpr std::array<std::uint8_t, 3> kNew = {'n', 'e', 'w'};
    constexpr std::array<std::uint8_t, 7> kSendResources = {
        's', 'e', 'n', 'd', 'r', 'e', 's',
    };
    constexpr std::array<std::uint8_t, 8> kSendEntities = {
        's', 'e', 'n', 'd', 'e', 'n', 't', 's',
    };
    constexpr std::array<std::uint8_t, 11> kDropClient = {
        'd', 'r', 'o', 'p', 'c', 'l', 'i', 'e', 'n', 't', '\n',
    };
    GoldSrcClientSignonCommand command = GoldSrcClientSignonCommand::kNone;
    if (command_size == kNew.size()
        && std::equal(kNew.begin(), kNew.end(), bytes + command_begin))
    {
        command = GoldSrcClientSignonCommand::kNew;
    }
    else if (command_size == kSendResources.size()
        && std::equal(
            kSendResources.begin(),
            kSendResources.end(),
            bytes + command_begin))
    {
        command = GoldSrcClientSignonCommand::kSendResources;
    }
    else if (command_size == kSendEntities.size()
        && std::equal(
            kSendEntities.begin(),
            kSendEntities.end(),
            bytes + command_begin))
    {
        command = GoldSrcClientSignonCommand::kSendEntities;
    }
    else if (command_size == kDropClient.size()
        && std::equal(
            kDropClient.begin(),
            kDropClient.end(),
            bytes + command_begin))
    {
        command = GoldSrcClientSignonCommand::kDisconnect;
    }
    else
    {
        result.status = GoldSrcClientSignonDecodeStatus::kUnsupportedCommand;
        return result;
    }
    ++cursor;
    result.command = command;
    result.primary_command_bytes = cursor;

    constexpr std::array<std::uint8_t, 12> kObservedCloseMenus = {
        'c', 'l', 'o', 's', 'e', 'm', 'e', 'n', 'u', 's', ' ', '\n',
    };
    std::size_t companion_count = 0;
    while (cursor < size)
    {
        if (bytes[cursor] == kGoldSrcClientNopOpcode)
        {
            ++cursor;
            ++result.trailing_nop_count;
            continue;
        }
        if (bytes[cursor] != kGoldSrcClientStringCommandOpcode)
        {
            result.status =
                GoldSrcClientSignonDecodeStatus::kUnsupportedTrailingData;
            return result;
        }
        if (command != GoldSrcClientSignonCommand::kSendResources)
        {
            result.status = GoldSrcClientSignonDecodeStatus::kMultipleCommands;
            return result;
        }

        ++cursor;
        const std::size_t companion_begin = cursor;
        while (cursor < size && bytes[cursor] != 0u)
        {
            if (cursor - companion_begin >= kGoldSrcMaximumClientCommandBytes)
            {
                result.status =
                    GoldSrcClientSignonDecodeStatus::kCommandTooLong;
                return result;
            }
            const std::uint8_t byte = bytes[cursor];
            if ((byte < 0x20u && byte != '\n') || byte == 0x7Fu)
            {
                result.status =
                    GoldSrcClientSignonDecodeStatus::kInvalidControlByte;
                return result;
            }
            ++cursor;
        }
        if (cursor == size)
        {
            result.status =
                GoldSrcClientSignonDecodeStatus::kMissingStringTerminator;
            return result;
        }

        const std::size_t companion_size = cursor - companion_begin;
        if (companion_size == 0u
            || companion_size != kObservedCloseMenus.size()
            || !std::equal(
                kObservedCloseMenus.begin(),
                kObservedCloseMenus.end(),
                bytes + companion_begin))
        {
            result.status =
                GoldSrcClientSignonDecodeStatus::kUnsupportedCompanionCommand;
            return result;
        }
        if (companion_count >= kGoldSrcMaximumCloseMenusCompanions)
        {
            result.status =
                GoldSrcClientSignonDecodeStatus::kTooManyCompanionCommands;
            return result;
        }
        ++companion_count;
        ++cursor;
    }

    if (companion_count == 1u)
    {
        result.status =
            GoldSrcClientSignonDecodeStatus::kUnsupportedCompanionCount;
        return result;
    }
    result.status = GoldSrcClientSignonDecodeStatus::kOk;
    result.companion_command = companion_count == 0u
        ? GoldSrcClientSignonCompanionCommand::kNone
        : GoldSrcClientSignonCompanionCommand::kCloseMenus;
    result.companion_count = companion_count;
    return result;
}

GoldSrcSendEntitiesEnvelopeDecodeResult DecodeGoldSrcSendEntitiesEnvelope(
    const std::uint8_t* bytes,
    std::size_t size) noexcept
{
    GoldSrcSendEntitiesEnvelopeDecodeResult result;
    if (bytes == nullptr)
    {
        result.status = size == 0u
            ? GoldSrcClientSignonDecodeStatus::kEmptyPayload
            : GoldSrcClientSignonDecodeStatus::kNullInput;
        return result;
    }
    if (size == 0u)
    {
        result.status = GoldSrcClientSignonDecodeStatus::kEmptyPayload;
        return result;
    }
    if (size > kGoldSrcMaximumSignonPayloadBytes)
    {
        result.status = GoldSrcClientSignonDecodeStatus::kPayloadTooLarge;
        return result;
    }

    std::size_t cursor = 0u;
    while (cursor < size && bytes[cursor] == kGoldSrcClientNopOpcode)
    {
        ++cursor;
    }
    constexpr std::array<std::uint8_t, 14> kObservedVModEnable = {
        kGoldSrcClientStringCommandOpcode,
        'V', 'M', 'o', 'd', 'E', 'n', 'a', 'b', 'l', 'e', ' ', '1', 0u,
    };
    if (size - cursor >= kObservedVModEnable.size()
        && std::equal(
            kObservedVModEnable.begin(),
            kObservedVModEnable.end(),
            bytes + cursor))
    {
        cursor += kObservedVModEnable.size();
        result.observed_vmod_enable_prefix = true;
        while (cursor < size && bytes[cursor] == kGoldSrcClientNopOpcode)
        {
            ++cursor;
        }
    }

    const GoldSrcClientSignonDecodeResult decoded =
        DecodeGoldSrcClientSignonPayload(bytes + cursor, size - cursor);
    if (decoded.command != GoldSrcClientSignonCommand::kSendEntities
        || (decoded.status != GoldSrcClientSignonDecodeStatus::kOk
            && decoded.status
                != GoldSrcClientSignonDecodeStatus::kUnsupportedTrailingData))
    {
        result.status = decoded.status;
        return result;
    }
    result.application_offset = cursor + decoded.primary_command_bytes;
    result.status = GoldSrcClientSignonDecodeStatus::kOk;
    return result;
}

std::string_view NameFor(GoldSrcSignonPhase phase) noexcept
{
    switch (phase)
    {
    case GoldSrcSignonPhase::kAwaitingNew:
        return "awaiting_new";
    case GoldSrcSignonPhase::kServerInfoQueued:
        return "serverinfo_queued";
    case GoldSrcSignonPhase::kServerInfoSentAwaitingAck:
        return "serverinfo_sent_awaiting_ack";
    case GoldSrcSignonPhase::kServerInfoAcknowledged:
        return "serverinfo_acknowledged";
    case GoldSrcSignonPhase::kSignonBootstrapQueued:
        return "signon_bootstrap_queued";
    case GoldSrcSignonPhase::kSignonBootstrapSentAwaitingAck:
        return "signon_bootstrap_sent_awaiting_ack";
    case GoldSrcSignonPhase::kSignonBootstrapAcknowledged:
        return "signon_bootstrap_acknowledged";
    case GoldSrcSignonPhase::kAwaitingResourceRequest:
        return "awaiting_resource_request";
    case GoldSrcSignonPhase::kResourceManifestQueued:
        return "resource_manifest_queued";
    case GoldSrcSignonPhase::kResourceManifestSentAwaitingAck:
        return "resource_manifest_sent_awaiting_ack";
    case GoldSrcSignonPhase::kResourceManifestAcknowledged:
        return "resource_manifest_acknowledged";
    case GoldSrcSignonPhase::kAwaitingPostResourceCommand:
        return "awaiting_post_resource_command";
    case GoldSrcSignonPhase::kAwaitingServerBaselineOrSnapshot:
        return "awaiting_server_baseline_or_snapshot";
    case GoldSrcSignonPhase::kAwaitingBaselineBootstrap:
        return "awaiting_baseline_bootstrap";
    case GoldSrcSignonPhase::kBaselineBootstrapQueued:
        return "baseline_bootstrap_queued";
    case GoldSrcSignonPhase::kBaselineBootstrapSentAwaitingAck:
        return "baseline_bootstrap_sent_awaiting_ack";
    case GoldSrcSignonPhase::kBaselineBootstrapAcknowledged:
        return "baseline_bootstrap_acknowledged";
    case GoldSrcSignonPhase::kAwaitingFirstSnapshot:
        return "awaiting_first_snapshot";
    case GoldSrcSignonPhase::kNone:
    default:
        return "none";
    }
}

std::string_view ReasonFor(GoldSrcSignonTransitionResult result) noexcept
{
    switch (result)
    {
    case GoldSrcSignonTransitionResult::kAdvanced:
        return "advanced";
    case GoldSrcSignonTransitionResult::kAlreadyApplied:
        return "already_applied";
    case GoldSrcSignonTransitionResult::kInvalidPhase:
    default:
        return "invalid_phase";
    }
}

std::string_view ReasonFor(
    GoldSrcSignonCommandDisposition disposition) noexcept
{
    switch (disposition)
    {
    case GoldSrcSignonCommandDisposition::kDelivered:
        return "delivered";
    case GoldSrcSignonCommandDisposition::kDuplicateSuppressed:
        return "duplicate_suppressed";
    case GoldSrcSignonCommandDisposition::kWrongPhase:
        return "wrong_phase";
    case GoldSrcSignonCommandDisposition::kUnsupported:
    default:
        return "unsupported";
    }
}

void GoldSrcSignonSessionState::Reset() noexcept
{
    phase_ = GoldSrcSignonPhase::kNone;
    bootstrap_mode_ = GoldSrcSignonBootstrapMode::kServerInfoOnly;
    diagnostics_ = {};
}

GoldSrcSignonTransitionResult
GoldSrcSignonSessionState::EnterAwaitingNew(
    GoldSrcSignonBootstrapMode mode) noexcept
{
    if (phase_ == GoldSrcSignonPhase::kNone)
    {
        bootstrap_mode_ = mode;
        phase_ = GoldSrcSignonPhase::kAwaitingNew;
        return GoldSrcSignonTransitionResult::kAdvanced;
    }
    if (phase_ == GoldSrcSignonPhase::kAwaitingNew
        && bootstrap_mode_ == mode)
    {
        return GoldSrcSignonTransitionResult::kAlreadyApplied;
    }
    return GoldSrcSignonTransitionResult::kInvalidPhase;
}

GoldSrcSignonCommandDisposition
GoldSrcSignonSessionState::HandleClientCommand(
    GoldSrcClientSignonCommand command) noexcept
{
    if (command == GoldSrcClientSignonCommand::kNew)
    {
        ++diagnostics_.client_new_received;
        if (phase_ == GoldSrcSignonPhase::kAwaitingNew)
        {
            if (bootstrap_mode_
                == GoldSrcSignonBootstrapMode::
                    kServerInfoWithDeltaDescriptions)
            {
                phase_ = GoldSrcSignonPhase::kSignonBootstrapQueued;
                ++diagnostics_.delta_descriptions_queued;
                ++diagnostics_.signon_bootstrap_queued;
            }
            else
            {
                phase_ = GoldSrcSignonPhase::kServerInfoQueued;
            }
            ++diagnostics_.client_new_delivered;
            ++diagnostics_.serverinfo_queued;
            return GoldSrcSignonCommandDisposition::kDelivered;
        }
        if (phase_ == GoldSrcSignonPhase::kServerInfoQueued
            || phase_ == GoldSrcSignonPhase::kServerInfoSentAwaitingAck
            || phase_ == GoldSrcSignonPhase::kServerInfoAcknowledged
            || phase_ == GoldSrcSignonPhase::kSignonBootstrapQueued
            || phase_
                == GoldSrcSignonPhase::kSignonBootstrapSentAwaitingAck
            || phase_ == GoldSrcSignonPhase::kSignonBootstrapAcknowledged
            || phase_ == GoldSrcSignonPhase::kAwaitingResourceRequest
            || phase_ == GoldSrcSignonPhase::kResourceManifestQueued
            || phase_
                == GoldSrcSignonPhase::kResourceManifestSentAwaitingAck
            || phase_ == GoldSrcSignonPhase::kResourceManifestAcknowledged
            || phase_ == GoldSrcSignonPhase::kAwaitingPostResourceCommand
            || phase_
                == GoldSrcSignonPhase::kAwaitingServerBaselineOrSnapshot
            || phase_ == GoldSrcSignonPhase::kAwaitingBaselineBootstrap
            || phase_ == GoldSrcSignonPhase::kBaselineBootstrapQueued
            || phase_
                == GoldSrcSignonPhase::kBaselineBootstrapSentAwaitingAck
            || phase_ == GoldSrcSignonPhase::kBaselineBootstrapAcknowledged
            || phase_ == GoldSrcSignonPhase::kAwaitingFirstSnapshot)
        {
            ++diagnostics_.duplicate_new_suppressed;
            return GoldSrcSignonCommandDisposition::kDuplicateSuppressed;
        }

        ++diagnostics_.new_wrong_phase;
        return GoldSrcSignonCommandDisposition::kWrongPhase;
    }

    if (command == GoldSrcClientSignonCommand::kSendResources)
    {
        ++diagnostics_.resource_request_received;
        if (phase_ == GoldSrcSignonPhase::kAwaitingResourceRequest)
        {
            phase_ = GoldSrcSignonPhase::kResourceManifestQueued;
            ++diagnostics_.resource_request_delivered;
            ++diagnostics_.resource_manifest_queued;
            return GoldSrcSignonCommandDisposition::kDelivered;
        }
        if (phase_ == GoldSrcSignonPhase::kResourceManifestQueued
            || phase_
                == GoldSrcSignonPhase::kResourceManifestSentAwaitingAck
            || phase_ == GoldSrcSignonPhase::kResourceManifestAcknowledged
            || phase_ == GoldSrcSignonPhase::kAwaitingPostResourceCommand
            || phase_
                == GoldSrcSignonPhase::kAwaitingServerBaselineOrSnapshot
            || phase_ == GoldSrcSignonPhase::kAwaitingBaselineBootstrap
            || phase_ == GoldSrcSignonPhase::kBaselineBootstrapQueued
            || phase_
                == GoldSrcSignonPhase::kBaselineBootstrapSentAwaitingAck
            || phase_ == GoldSrcSignonPhase::kBaselineBootstrapAcknowledged
            || phase_ == GoldSrcSignonPhase::kAwaitingFirstSnapshot)
        {
            ++diagnostics_.duplicate_resource_request_suppressed;
            return GoldSrcSignonCommandDisposition::kDuplicateSuppressed;
        }

        ++diagnostics_.resource_request_wrong_phase;
        return GoldSrcSignonCommandDisposition::kWrongPhase;
    }

    return GoldSrcSignonCommandDisposition::kUnsupported;
}

GoldSrcSignonTransitionResult
GoldSrcSignonSessionState::MarkServerInfoSent() noexcept
{
    if (phase_ == GoldSrcSignonPhase::kServerInfoQueued)
    {
        phase_ = GoldSrcSignonPhase::kServerInfoSentAwaitingAck;
        ++diagnostics_.serverinfo_sent;
        return GoldSrcSignonTransitionResult::kAdvanced;
    }
    if (phase_ == GoldSrcSignonPhase::kServerInfoSentAwaitingAck
        || phase_ == GoldSrcSignonPhase::kServerInfoAcknowledged
        || phase_ == GoldSrcSignonPhase::kAwaitingResourceRequest
        || phase_ == GoldSrcSignonPhase::kResourceManifestQueued
        || phase_ == GoldSrcSignonPhase::kResourceManifestSentAwaitingAck
        || phase_ == GoldSrcSignonPhase::kResourceManifestAcknowledged)
    {
        return GoldSrcSignonTransitionResult::kAlreadyApplied;
    }
    return GoldSrcSignonTransitionResult::kInvalidPhase;
}

GoldSrcSignonTransitionResult
GoldSrcSignonSessionState::MarkServerInfoAcknowledged() noexcept
{
    if (phase_ == GoldSrcSignonPhase::kServerInfoSentAwaitingAck)
    {
        phase_ = GoldSrcSignonPhase::kServerInfoAcknowledged;
        ++diagnostics_.serverinfo_acknowledged;
        return GoldSrcSignonTransitionResult::kAdvanced;
    }
    if (phase_ == GoldSrcSignonPhase::kServerInfoAcknowledged
        || phase_ == GoldSrcSignonPhase::kAwaitingResourceRequest
        || phase_ == GoldSrcSignonPhase::kResourceManifestQueued
        || phase_ == GoldSrcSignonPhase::kResourceManifestSentAwaitingAck
        || phase_ == GoldSrcSignonPhase::kResourceManifestAcknowledged)
    {
        return GoldSrcSignonTransitionResult::kAlreadyApplied;
    }
    return GoldSrcSignonTransitionResult::kInvalidPhase;
}

GoldSrcSignonTransitionResult
GoldSrcSignonSessionState::MarkSignonBootstrapSent() noexcept
{
    if (phase_ == GoldSrcSignonPhase::kSignonBootstrapQueued)
    {
        phase_ = GoldSrcSignonPhase::kSignonBootstrapSentAwaitingAck;
        ++diagnostics_.serverinfo_sent;
        ++diagnostics_.delta_descriptions_sent;
        ++diagnostics_.signon_bootstrap_sent;
        return GoldSrcSignonTransitionResult::kAdvanced;
    }
    if (phase_ == GoldSrcSignonPhase::kSignonBootstrapSentAwaitingAck
        || phase_ == GoldSrcSignonPhase::kSignonBootstrapAcknowledged
        || phase_ == GoldSrcSignonPhase::kAwaitingResourceRequest
        || phase_ == GoldSrcSignonPhase::kResourceManifestQueued
        || phase_ == GoldSrcSignonPhase::kResourceManifestSentAwaitingAck
        || phase_ == GoldSrcSignonPhase::kResourceManifestAcknowledged)
    {
        return GoldSrcSignonTransitionResult::kAlreadyApplied;
    }
    return GoldSrcSignonTransitionResult::kInvalidPhase;
}

GoldSrcSignonTransitionResult
GoldSrcSignonSessionState::MarkSignonBootstrapAcknowledged() noexcept
{
    if (phase_ == GoldSrcSignonPhase::kSignonBootstrapSentAwaitingAck)
    {
        phase_ = GoldSrcSignonPhase::kSignonBootstrapAcknowledged;
        ++diagnostics_.serverinfo_acknowledged;
        ++diagnostics_.delta_descriptions_acknowledged;
        ++diagnostics_.signon_bootstrap_acknowledged;
        return GoldSrcSignonTransitionResult::kAdvanced;
    }
    if (phase_ == GoldSrcSignonPhase::kSignonBootstrapAcknowledged
        || phase_ == GoldSrcSignonPhase::kAwaitingResourceRequest
        || phase_ == GoldSrcSignonPhase::kResourceManifestQueued
        || phase_ == GoldSrcSignonPhase::kResourceManifestSentAwaitingAck
        || phase_ == GoldSrcSignonPhase::kResourceManifestAcknowledged)
    {
        return GoldSrcSignonTransitionResult::kAlreadyApplied;
    }
    return GoldSrcSignonTransitionResult::kInvalidPhase;
}

GoldSrcSignonTransitionResult
GoldSrcSignonSessionState::EnterAwaitingResourceRequest() noexcept
{
    if (phase_ == GoldSrcSignonPhase::kServerInfoAcknowledged
        || phase_ == GoldSrcSignonPhase::kSignonBootstrapAcknowledged)
    {
        phase_ = GoldSrcSignonPhase::kAwaitingResourceRequest;
        return GoldSrcSignonTransitionResult::kAdvanced;
    }
    if (phase_ == GoldSrcSignonPhase::kAwaitingResourceRequest
        || phase_ == GoldSrcSignonPhase::kResourceManifestQueued
        || phase_ == GoldSrcSignonPhase::kResourceManifestSentAwaitingAck
        || phase_ == GoldSrcSignonPhase::kResourceManifestAcknowledged)
    {
        return GoldSrcSignonTransitionResult::kAlreadyApplied;
    }
    return GoldSrcSignonTransitionResult::kInvalidPhase;
}

GoldSrcSignonTransitionResult
GoldSrcSignonSessionState::MarkResourceManifestSent() noexcept
{
    if (phase_ == GoldSrcSignonPhase::kResourceManifestQueued)
    {
        phase_ = GoldSrcSignonPhase::kResourceManifestSentAwaitingAck;
        ++diagnostics_.resource_manifest_sent;
        return GoldSrcSignonTransitionResult::kAdvanced;
    }
    if (phase_ == GoldSrcSignonPhase::kResourceManifestSentAwaitingAck
        || phase_ == GoldSrcSignonPhase::kResourceManifestAcknowledged
        || phase_ == GoldSrcSignonPhase::kAwaitingPostResourceCommand
        || phase_ == GoldSrcSignonPhase::kAwaitingServerBaselineOrSnapshot)
    {
        return GoldSrcSignonTransitionResult::kAlreadyApplied;
    }
    return GoldSrcSignonTransitionResult::kInvalidPhase;
}

GoldSrcSignonTransitionResult
GoldSrcSignonSessionState::MarkResourceManifestAcknowledged() noexcept
{
    if (phase_ == GoldSrcSignonPhase::kResourceManifestSentAwaitingAck)
    {
        phase_ = GoldSrcSignonPhase::kResourceManifestAcknowledged;
        ++diagnostics_.resource_manifest_acknowledged;
        return GoldSrcSignonTransitionResult::kAdvanced;
    }
    if (phase_ == GoldSrcSignonPhase::kResourceManifestAcknowledged
        || phase_ == GoldSrcSignonPhase::kAwaitingPostResourceCommand
        || phase_ == GoldSrcSignonPhase::kAwaitingServerBaselineOrSnapshot)
    {
        return GoldSrcSignonTransitionResult::kAlreadyApplied;
    }
    return GoldSrcSignonTransitionResult::kInvalidPhase;
}

GoldSrcSignonTransitionResult
GoldSrcSignonSessionState::EnterAwaitingPostResourceCommand() noexcept
{
    if (phase_ == GoldSrcSignonPhase::kResourceManifestAcknowledged)
    {
        phase_ = GoldSrcSignonPhase::kAwaitingPostResourceCommand;
        return GoldSrcSignonTransitionResult::kAdvanced;
    }
    if (phase_ == GoldSrcSignonPhase::kAwaitingPostResourceCommand
        || phase_ == GoldSrcSignonPhase::kAwaitingServerBaselineOrSnapshot)
    {
        return GoldSrcSignonTransitionResult::kAlreadyApplied;
    }
    return GoldSrcSignonTransitionResult::kInvalidPhase;
}

GoldSrcSignonCommandDisposition
GoldSrcSignonSessionState::HandlePostResourceMove() noexcept
{
    ++diagnostics_.post_resource_command_received;
    if (phase_ == GoldSrcSignonPhase::kAwaitingPostResourceCommand)
    {
        phase_ = GoldSrcSignonPhase::kAwaitingServerBaselineOrSnapshot;
        ++diagnostics_.post_resource_command_delivered;
        ++diagnostics_.post_resource_command_state_advances;
        return GoldSrcSignonCommandDisposition::kDelivered;
    }
    if (phase_ == GoldSrcSignonPhase::kAwaitingServerBaselineOrSnapshot)
    {
        ++diagnostics_.post_resource_command_delivered;
        return GoldSrcSignonCommandDisposition::kDelivered;
    }
    ++diagnostics_.post_resource_command_wrong_phase;
    return GoldSrcSignonCommandDisposition::kWrongPhase;
}

GoldSrcSignonTransitionResult
GoldSrcSignonSessionState::EnterAwaitingBaselineBootstrap() noexcept
{
    if (phase_ == GoldSrcSignonPhase::kAwaitingServerBaselineOrSnapshot)
    {
        phase_ = GoldSrcSignonPhase::kAwaitingBaselineBootstrap;
        return GoldSrcSignonTransitionResult::kAdvanced;
    }
    if (phase_ == GoldSrcSignonPhase::kAwaitingBaselineBootstrap
        || phase_ == GoldSrcSignonPhase::kBaselineBootstrapQueued
        || phase_ == GoldSrcSignonPhase::kBaselineBootstrapSentAwaitingAck
        || phase_ == GoldSrcSignonPhase::kBaselineBootstrapAcknowledged
        || phase_ == GoldSrcSignonPhase::kAwaitingFirstSnapshot)
    {
        return GoldSrcSignonTransitionResult::kAlreadyApplied;
    }
    return GoldSrcSignonTransitionResult::kInvalidPhase;
}

GoldSrcSignonTransitionResult
GoldSrcSignonSessionState::MarkBaselineBootstrapQueued() noexcept
{
    if (phase_ == GoldSrcSignonPhase::kAwaitingBaselineBootstrap)
    {
        phase_ = GoldSrcSignonPhase::kBaselineBootstrapQueued;
        ++diagnostics_.baseline_bootstrap_queued;
        return GoldSrcSignonTransitionResult::kAdvanced;
    }
    if (phase_ == GoldSrcSignonPhase::kBaselineBootstrapQueued
        || phase_ == GoldSrcSignonPhase::kBaselineBootstrapSentAwaitingAck
        || phase_ == GoldSrcSignonPhase::kBaselineBootstrapAcknowledged
        || phase_ == GoldSrcSignonPhase::kAwaitingFirstSnapshot)
    {
        return GoldSrcSignonTransitionResult::kAlreadyApplied;
    }
    return GoldSrcSignonTransitionResult::kInvalidPhase;
}

GoldSrcSignonTransitionResult
GoldSrcSignonSessionState::MarkBaselineBootstrapSent() noexcept
{
    if (phase_ == GoldSrcSignonPhase::kBaselineBootstrapQueued)
    {
        phase_ = GoldSrcSignonPhase::kBaselineBootstrapSentAwaitingAck;
        ++diagnostics_.baseline_bootstrap_sent;
        return GoldSrcSignonTransitionResult::kAdvanced;
    }
    if (phase_ == GoldSrcSignonPhase::kBaselineBootstrapSentAwaitingAck
        || phase_ == GoldSrcSignonPhase::kBaselineBootstrapAcknowledged
        || phase_ == GoldSrcSignonPhase::kAwaitingFirstSnapshot)
    {
        return GoldSrcSignonTransitionResult::kAlreadyApplied;
    }
    return GoldSrcSignonTransitionResult::kInvalidPhase;
}

GoldSrcSignonTransitionResult
GoldSrcSignonSessionState::MarkBaselineBootstrapAcknowledged() noexcept
{
    if (phase_ == GoldSrcSignonPhase::kBaselineBootstrapSentAwaitingAck)
    {
        phase_ = GoldSrcSignonPhase::kBaselineBootstrapAcknowledged;
        ++diagnostics_.baseline_bootstrap_acknowledged;
        return GoldSrcSignonTransitionResult::kAdvanced;
    }
    if (phase_ == GoldSrcSignonPhase::kBaselineBootstrapAcknowledged
        || phase_ == GoldSrcSignonPhase::kAwaitingFirstSnapshot)
    {
        return GoldSrcSignonTransitionResult::kAlreadyApplied;
    }
    return GoldSrcSignonTransitionResult::kInvalidPhase;
}

GoldSrcSignonTransitionResult
GoldSrcSignonSessionState::EnterAwaitingFirstSnapshot() noexcept
{
    if (phase_ == GoldSrcSignonPhase::kBaselineBootstrapAcknowledged)
    {
        phase_ = GoldSrcSignonPhase::kAwaitingFirstSnapshot;
        return GoldSrcSignonTransitionResult::kAdvanced;
    }
    if (phase_ == GoldSrcSignonPhase::kAwaitingFirstSnapshot)
    {
        return GoldSrcSignonTransitionResult::kAlreadyApplied;
    }
    return GoldSrcSignonTransitionResult::kInvalidPhase;
}

GoldSrcSignonCommandDisposition
GoldSrcSignonSessionState::HandleSendEntities() noexcept
{
    ++diagnostics_.send_entities_received;
    if (phase_ == GoldSrcSignonPhase::kAwaitingFirstSnapshot)
    {
        ++diagnostics_.send_entities_delivered;
        return GoldSrcSignonCommandDisposition::kDelivered;
    }
    ++diagnostics_.send_entities_wrong_phase;
    return GoldSrcSignonCommandDisposition::kWrongPhase;
}

GoldSrcSignonPhase GoldSrcSignonSessionState::phase() const noexcept
{
    return phase_;
}

const GoldSrcSignonDiagnostics&
GoldSrcSignonSessionState::diagnostics() const noexcept
{
    return diagnostics_;
}

std::string_view ReasonFor(GoldSrcServerInfoCodecStatus status) noexcept
{
    switch (status)
    {
    case GoldSrcServerInfoCodecStatus::kOk:
        return "ok";
    case GoldSrcServerInfoCodecStatus::kUnsupportedProtocol:
        return "unsupported_protocol";
    case GoldSrcServerInfoCodecStatus::kInvalidMaxClients:
        return "invalid_max_clients";
    case GoldSrcServerInfoCodecStatus::kInvalidPlayerIndex:
        return "invalid_player_index";
    case GoldSrcServerInfoCodecStatus::kMissingMapChecksum:
        return "missing_map_checksum";
    case GoldSrcServerInfoCodecStatus::kZeroMapChecksum:
        return "zero_map_checksum";
    case GoldSrcServerInfoCodecStatus::kMissingClientDllDigest:
        return "missing_client_dll_digest";
    case GoldSrcServerInfoCodecStatus::kZeroClientDllDigest:
        return "zero_client_dll_digest";
    case GoldSrcServerInfoCodecStatus::kEmptyRequiredString:
        return "empty_required_string";
    case GoldSrcServerInfoCodecStatus::kStringTooLong:
        return "string_too_long";
    case GoldSrcServerInfoCodecStatus::kEmbeddedNul:
        return "embedded_nul";
    case GoldSrcServerInfoCodecStatus::kInvalidControlByte:
        return "invalid_control_byte";
    case GoldSrcServerInfoCodecStatus::kInvalidModelPath:
        return "invalid_model_path";
    case GoldSrcServerInfoCodecStatus::kSecureModeUnsupported:
        return "secure_mode_unsupported";
    case GoldSrcServerInfoCodecStatus::kCheatsUnsupported:
        return "cheats_unsupported";
    case GoldSrcServerInfoCodecStatus::kPayloadTooLarge:
        return "payload_too_large";
    default:
        return "invalid_serverinfo_context";
    }
}

GoldSrcServerInfoValidationResult ValidateGoldSrcServerInfoContext(
    const GoldSrcServerInfoContext& context) noexcept
{
    GoldSrcServerInfoValidationResult result{};
    if (context.protocol_version != kGoldSrcServerInfoProtocolVersion)
    {
        result.status = GoldSrcServerInfoCodecStatus::kUnsupportedProtocol;
        return result;
    }
    if (context.max_clients == 0u)
    {
        result.status = GoldSrcServerInfoCodecStatus::kInvalidMaxClients;
        return result;
    }
    if (context.player_index >= context.max_clients)
    {
        result.status = GoldSrcServerInfoCodecStatus::kInvalidPlayerIndex;
        return result;
    }
    if (!context.canonical_map_checksum.has_value())
    {
        result.status = GoldSrcServerInfoCodecStatus::kMissingMapChecksum;
        return result;
    }
    if (*context.canonical_map_checksum == 0u)
    {
        result.status = GoldSrcServerInfoCodecStatus::kZeroMapChecksum;
        return result;
    }
    if (!context.client_dll_md5.has_value())
    {
        result.status = GoldSrcServerInfoCodecStatus::kMissingClientDllDigest;
        return result;
    }
    if (IsAllZero(*context.client_dll_md5))
    {
        result.status = GoldSrcServerInfoCodecStatus::kZeroClientDllDigest;
        return result;
    }

    const std::array<GoldSrcServerInfoCodecStatus, 5> string_statuses = {
        ValidateString(
            context.game_directory,
            kGoldSrcMaximumGameDirectoryBytes,
            true),
        ValidateString(context.hostname, kGoldSrcMaximumHostnameBytes, true),
        ValidateString(
            context.map_model_path,
            kGoldSrcMaximumModelPathBytes,
            true),
        ValidateString(context.mapcycle, kGoldSrcMaximumMapcycleBytes, false),
        ValidateString(
            context.fallback_game_directory,
            kGoldSrcMaximumFallbackDirectoryBytes,
            false),
    };
    const auto invalid_string = std::find_if(
        string_statuses.begin(),
        string_statuses.end(),
        [](GoldSrcServerInfoCodecStatus status)
        {
            return status != GoldSrcServerInfoCodecStatus::kOk;
        });
    if (invalid_string != string_statuses.end())
    {
        result.status = *invalid_string;
        return result;
    }

    const std::string_view model_path = context.map_model_path;
    if (!IsValidMapModelPath(model_path))
    {
        result.status = GoldSrcServerInfoCodecStatus::kInvalidModelPath;
        return result;
    }
    if (context.secure)
    {
        result.status = GoldSrcServerInfoCodecStatus::kSecureModeUnsupported;
        return result;
    }
    if (context.cheats)
    {
        result.status = GoldSrcServerInfoCodecStatus::kCheatsUnsupported;
        return result;
    }

    const std::size_t encoded_size = 40u
        + context.game_directory.size()
        + context.hostname.size()
        + context.map_model_path.size()
        + context.mapcycle.size()
        + context.fallback_game_directory.size();
    if (encoded_size > kGoldSrcMaximumSignonPayloadBytes)
    {
        result.status = GoldSrcServerInfoCodecStatus::kPayloadTooLarge;
        return result;
    }

    result.status = GoldSrcServerInfoCodecStatus::kOk;
    return result;
}

std::uint32_t MungeGoldSrcServerInfoChecksum(
    std::uint32_t canonical_checksum,
    std::uint8_t player_index) noexcept
{
    const std::uint32_t key =
        static_cast<std::uint8_t>(0xFFu - player_index);
    std::uint32_t value = canonical_checksum ^ ~key;
    value = ByteSwap32(value);

    std::array<std::uint8_t, 4> bytes{};
    WriteLittleEndian32(bytes.data(), value);
    for (std::size_t index = 0; index < bytes.size(); ++index)
    {
        const auto byte_index = static_cast<std::uint32_t>(index);
        const std::uint8_t mask = static_cast<std::uint8_t>(
            0xA5u
            | (byte_index << byte_index)
            | byte_index
            | kMungeTable3[index]);
        bytes[index] ^= mask;
    }
    value = ReadLittleEndian32(bytes.data()) ^ key;
    return value;
}

std::uint32_t UnmungeGoldSrcServerInfoChecksum(
    std::uint32_t wire_checksum,
    std::uint8_t player_index) noexcept
{
    const std::uint32_t key =
        static_cast<std::uint8_t>(0xFFu - player_index);
    std::uint32_t value = wire_checksum ^ key;

    std::array<std::uint8_t, 4> bytes{};
    WriteLittleEndian32(bytes.data(), value);
    for (std::size_t index = 0; index < bytes.size(); ++index)
    {
        const auto byte_index = static_cast<std::uint32_t>(index);
        const std::uint8_t mask = static_cast<std::uint8_t>(
            0xA5u
            | (byte_index << byte_index)
            | byte_index
            | kMungeTable3[index]);
        bytes[index] ^= mask;
    }
    value = ByteSwap32(ReadLittleEndian32(bytes.data()));
    return value ^ ~key;
}

GoldSrcServerInfoEncodeResult EncodeGoldSrcServerInfo(
    const GoldSrcServerInfoContext& context) noexcept
{
    GoldSrcServerInfoEncodeResult result{};
    const GoldSrcServerInfoValidationResult validation =
        ValidateGoldSrcServerInfoContext(context);
    if (!validation.ok())
    {
        result.status = validation.status;
        return result;
    }

    const GoldSrcSendExtraInfoEncodeResult extra_info =
        EncodeGoldSrcSendExtraInfo(
            context.fallback_game_directory,
            context.cheats);
    if (!extra_info.ok())
    {
        result.status = extra_info.status;
        return result;
    }

    PayloadWriter<GoldSrcServerInfoPayload> writer(&result.payload);
    const std::uint32_t wire_checksum = MungeGoldSrcServerInfoChecksum(
        *context.canonical_map_checksum,
        context.player_index);
    const bool encoded = writer.WriteByte(kGoldSrcServerInfoOpcode)
        && writer.WriteLittleEndian32(context.protocol_version)
        && writer.WriteLittleEndian32(context.spawn_count)
        && writer.WriteLittleEndian32(wire_checksum)
        && writer.WriteBytes(
            context.client_dll_md5->data(),
            context.client_dll_md5->size())
        && writer.WriteByte(context.max_clients)
        && writer.WriteByte(context.player_index)
        && writer.WriteByte(context.deathmatch ? 1u : 0u)
        && writer.WriteString(context.game_directory)
        && writer.WriteString(context.hostname)
        && writer.WriteString(context.map_model_path)
        && writer.WriteString(context.mapcycle)
        && writer.WriteByte(0u)
        && writer.WriteBytes(
            extra_info.payload.bytes.data(),
            extra_info.payload.size);
    if (!encoded)
    {
        result.payload = {};
        result.status = GoldSrcServerInfoCodecStatus::kPayloadTooLarge;
        return result;
    }

    result.status = GoldSrcServerInfoCodecStatus::kOk;
    return result;
}

GoldSrcSendExtraInfoEncodeResult EncodeGoldSrcSendExtraInfo(
    std::string_view fallback_game_directory,
    bool cheats) noexcept
{
    GoldSrcSendExtraInfoEncodeResult result{};
    const GoldSrcServerInfoCodecStatus string_status = ValidateString(
        fallback_game_directory,
        kGoldSrcMaximumFallbackDirectoryBytes,
        false);
    if (string_status != GoldSrcServerInfoCodecStatus::kOk)
    {
        result.status = string_status;
        return result;
    }
    if (cheats)
    {
        result.status = GoldSrcServerInfoCodecStatus::kCheatsUnsupported;
        return result;
    }

    PayloadWriter<GoldSrcSendExtraInfoPayload> writer(&result.payload);
    const bool encoded = writer.WriteByte(kGoldSrcSendExtraInfoOpcode)
        && writer.WriteString(fallback_game_directory)
        && writer.WriteByte(0u);
    if (!encoded)
    {
        result.payload = {};
        result.status = GoldSrcServerInfoCodecStatus::kPayloadTooLarge;
        return result;
    }

    result.status = GoldSrcServerInfoCodecStatus::kOk;
    return result;
}

std::string_view ReasonFor(GoldSrcBootstrapTailCodecStatus status) noexcept
{
    switch (status)
    {
    case GoldSrcBootstrapTailCodecStatus::kOk:
        return "ok";
    case GoldSrcBootstrapTailCodecStatus::kNonFiniteMoveVariable:
        return "non_finite_move_variable";
    case GoldSrcBootstrapTailCodecStatus::kSkyNameTooLong:
        return "sky_name_too_long";
    case GoldSrcBootstrapTailCodecStatus::kEmbeddedNul:
        return "embedded_nul";
    case GoldSrcBootstrapTailCodecStatus::kInvalidControlByte:
        return "invalid_control_byte";
    case GoldSrcBootstrapTailCodecStatus::kInvalidViewEntity:
        return "invalid_view_entity";
    case GoldSrcBootstrapTailCodecStatus::kPayloadTooLarge:
        return "payload_too_large";
    default:
        return "invalid_bootstrap_tail";
    }
}

GoldSrcBootstrapTailEncodeResult EncodeGoldSrcBootstrapTail(
    const GoldSrcBootstrapTailContext& context) noexcept
{
    GoldSrcBootstrapTailEncodeResult result{};
    const std::array<float, 24> move_variables = {
        context.gravity,
        context.stop_speed,
        context.maximum_speed,
        context.spectator_maximum_speed,
        context.accelerate,
        context.air_accelerate,
        context.water_accelerate,
        context.friction,
        context.edge_friction,
        context.water_friction,
        context.entity_gravity,
        context.bounce,
        context.step_size,
        context.maximum_velocity,
        context.z_maximum,
        context.wave_height,
        context.roll_angle,
        context.roll_speed,
        context.sky_color_red,
        context.sky_color_green,
        context.sky_color_blue,
        context.sky_vector_x,
        context.sky_vector_y,
        context.sky_vector_z,
    };
    if (!std::all_of(
            move_variables.begin(),
            move_variables.end(),
            [](float value)
            {
                return std::isfinite(value);
            }))
    {
        result.status =
            GoldSrcBootstrapTailCodecStatus::kNonFiniteMoveVariable;
        return result;
    }
    const GoldSrcServerInfoCodecStatus sky_status = ValidateString(
        context.sky_name,
        kGoldSrcMaximumSkyNameBytes,
        false);
    if (sky_status != GoldSrcServerInfoCodecStatus::kOk)
    {
        result.status =
            sky_status == GoldSrcServerInfoCodecStatus::kStringTooLong
            ? GoldSrcBootstrapTailCodecStatus::kSkyNameTooLong
            : sky_status == GoldSrcServerInfoCodecStatus::kEmbeddedNul
            ? GoldSrcBootstrapTailCodecStatus::kEmbeddedNul
            : GoldSrcBootstrapTailCodecStatus::kInvalidControlByte;
        return result;
    }
    if (context.view_entity == 0u
        || context.view_entity > kGoldSrcMaximumViewEntity)
    {
        result.status = GoldSrcBootstrapTailCodecStatus::kInvalidViewEntity;
        return result;
    }

    PayloadWriter<GoldSrcBootstrapTailPayload> writer(&result.payload);
    bool encoded = writer.WriteByte(kGoldSrcNewMoveVarsOpcode);
    for (std::size_t index = 0u; encoded && index < 16u; ++index)
    {
        encoded = writer.WriteFloat(move_variables[index]);
    }
    encoded = encoded && writer.WriteByte(context.footsteps ? 1u : 0u);
    for (std::size_t index = 16u;
         encoded && index < move_variables.size();
         ++index)
    {
        encoded = writer.WriteFloat(move_variables[index]);
    }
    encoded = encoded
        && writer.WriteString(context.sky_name)
        && writer.WriteByte(kGoldSrcCdTrackOpcode)
        && writer.WriteByte(context.cd_audio_track)
        && writer.WriteByte(context.cd_audio_track)
        && writer.WriteByte(kGoldSrcSetViewOpcode)
        && writer.WriteLittleEndian16(context.view_entity);
    if (!encoded)
    {
        result.payload = {};
        result.status = GoldSrcBootstrapTailCodecStatus::kPayloadTooLarge;
        return result;
    }

    result.status = GoldSrcBootstrapTailCodecStatus::kOk;
    return result;
}

std::string_view ReasonFor(GoldSrcServerInfoDecodeStatus status) noexcept
{
    switch (status)
    {
    case GoldSrcServerInfoDecodeStatus::kOk:
        return "ok";
    case GoldSrcServerInfoDecodeStatus::kNullInput:
        return "null_input";
    case GoldSrcServerInfoDecodeStatus::kEmptyPayload:
        return "empty_payload";
    case GoldSrcServerInfoDecodeStatus::kPayloadTooLarge:
        return "payload_too_large";
    case GoldSrcServerInfoDecodeStatus::kTruncatedField:
        return "truncated_field";
    case GoldSrcServerInfoDecodeStatus::kWrongServerInfoOpcode:
        return "wrong_serverinfo_opcode";
    case GoldSrcServerInfoDecodeStatus::kUnsupportedProtocol:
        return "unsupported_protocol";
    case GoldSrcServerInfoDecodeStatus::kZeroMapChecksum:
        return "zero_map_checksum";
    case GoldSrcServerInfoDecodeStatus::kZeroClientDllDigest:
        return "zero_client_dll_digest";
    case GoldSrcServerInfoDecodeStatus::kInvalidMaxClients:
        return "invalid_max_clients";
    case GoldSrcServerInfoDecodeStatus::kInvalidPlayerIndex:
        return "invalid_player_index";
    case GoldSrcServerInfoDecodeStatus::kInvalidBooleanValue:
        return "invalid_boolean_value";
    case GoldSrcServerInfoDecodeStatus::kMissingStringTerminator:
        return "missing_string_terminator";
    case GoldSrcServerInfoDecodeStatus::kStringTooLong:
        return "string_too_long";
    case GoldSrcServerInfoDecodeStatus::kEmptyRequiredString:
        return "empty_required_string";
    case GoldSrcServerInfoDecodeStatus::kInvalidControlByte:
        return "invalid_control_byte";
    case GoldSrcServerInfoDecodeStatus::kInvalidModelPath:
        return "invalid_model_path";
    case GoldSrcServerInfoDecodeStatus::kSecureModeUnsupported:
        return "secure_mode_unsupported";
    case GoldSrcServerInfoDecodeStatus::kWrongCompanionOpcode:
        return "wrong_companion_opcode";
    case GoldSrcServerInfoDecodeStatus::kCheatsUnsupported:
        return "cheats_unsupported";
    case GoldSrcServerInfoDecodeStatus::kTrailingData:
        return "trailing_data";
    default:
        return "invalid_serverinfo_payload";
    }
}

GoldSrcServerInfoDecodeResult DecodeGoldSrcServerInfo(
    const std::uint8_t* bytes,
    std::size_t size) noexcept
{
    GoldSrcServerInfoDecodeResult result{};
    if (bytes == nullptr && size != 0u)
    {
        result.status = GoldSrcServerInfoDecodeStatus::kNullInput;
        return result;
    }
    if (size == 0u)
    {
        result.status = GoldSrcServerInfoDecodeStatus::kEmptyPayload;
        return result;
    }
    if (size > kGoldSrcMaximumSignonPayloadBytes)
    {
        result.status = GoldSrcServerInfoDecodeStatus::kPayloadTooLarge;
        return result;
    }

    PayloadReader reader(bytes, size);
    GoldSrcDecodedServerInfo decoded{};
    std::uint8_t opcode = 0u;
    if (!reader.ReadByte(&opcode))
    {
        result.status = GoldSrcServerInfoDecodeStatus::kTruncatedField;
        return result;
    }
    if (opcode != kGoldSrcServerInfoOpcode)
    {
        result.status = GoldSrcServerInfoDecodeStatus::kWrongServerInfoOpcode;
        return result;
    }
    if (!reader.ReadLittleEndian32(&decoded.protocol_version)
        || !reader.ReadLittleEndian32(&decoded.spawn_count)
        || !reader.ReadLittleEndian32(&decoded.wire_map_checksum)
        || !reader.ReadBytes(
            decoded.client_dll_md5.data(),
            decoded.client_dll_md5.size()))
    {
        result.status = GoldSrcServerInfoDecodeStatus::kTruncatedField;
        return result;
    }
    if (decoded.protocol_version != kGoldSrcServerInfoProtocolVersion)
    {
        result.status = GoldSrcServerInfoDecodeStatus::kUnsupportedProtocol;
        return result;
    }
    if (IsAllZero(decoded.client_dll_md5))
    {
        result.status = GoldSrcServerInfoDecodeStatus::kZeroClientDllDigest;
        return result;
    }

    std::uint8_t deathmatch = 0u;
    if (!reader.ReadByte(&decoded.max_clients)
        || !reader.ReadByte(&decoded.player_index)
        || !reader.ReadByte(&deathmatch))
    {
        result.status = GoldSrcServerInfoDecodeStatus::kTruncatedField;
        return result;
    }
    if (decoded.max_clients == 0u)
    {
        result.status = GoldSrcServerInfoDecodeStatus::kInvalidMaxClients;
        return result;
    }
    if (decoded.player_index >= decoded.max_clients)
    {
        result.status = GoldSrcServerInfoDecodeStatus::kInvalidPlayerIndex;
        return result;
    }
    if (deathmatch > 1u)
    {
        result.status = GoldSrcServerInfoDecodeStatus::kInvalidBooleanValue;
        return result;
    }
    decoded.deathmatch = deathmatch != 0u;
    decoded.canonical_map_checksum = UnmungeGoldSrcServerInfoChecksum(
        decoded.wire_map_checksum,
        decoded.player_index);
    if (decoded.canonical_map_checksum == 0u)
    {
        result.status = GoldSrcServerInfoDecodeStatus::kZeroMapChecksum;
        return result;
    }

    GoldSrcServerInfoDecodeStatus string_status = reader.ReadString(
        &decoded.game_directory,
        true);
    if (string_status != GoldSrcServerInfoDecodeStatus::kOk)
    {
        result.status = string_status;
        return result;
    }
    string_status = reader.ReadString(&decoded.hostname, true);
    if (string_status != GoldSrcServerInfoDecodeStatus::kOk)
    {
        result.status = string_status;
        return result;
    }
    string_status = reader.ReadString(&decoded.map_model_path, true);
    if (string_status != GoldSrcServerInfoDecodeStatus::kOk)
    {
        result.status = string_status;
        return result;
    }
    if (!IsValidMapModelPath(decoded.map_model_path.view()))
    {
        result.status = GoldSrcServerInfoDecodeStatus::kInvalidModelPath;
        return result;
    }
    string_status = reader.ReadString(&decoded.mapcycle, false);
    if (string_status != GoldSrcServerInfoDecodeStatus::kOk)
    {
        result.status = string_status;
        return result;
    }

    std::uint8_t secure = 0u;
    if (!reader.ReadByte(&secure))
    {
        result.status = GoldSrcServerInfoDecodeStatus::kTruncatedField;
        return result;
    }
    if (secure > 1u)
    {
        result.status = GoldSrcServerInfoDecodeStatus::kInvalidBooleanValue;
        return result;
    }
    decoded.secure = secure != 0u;
    if (decoded.secure)
    {
        result.status = GoldSrcServerInfoDecodeStatus::kSecureModeUnsupported;
        return result;
    }

    if (!reader.ReadByte(&opcode))
    {
        result.status = GoldSrcServerInfoDecodeStatus::kTruncatedField;
        return result;
    }
    if (opcode != kGoldSrcSendExtraInfoOpcode)
    {
        result.status = GoldSrcServerInfoDecodeStatus::kWrongCompanionOpcode;
        return result;
    }
    string_status = reader.ReadString(
        &decoded.fallback_game_directory,
        false);
    if (string_status != GoldSrcServerInfoDecodeStatus::kOk)
    {
        result.status = string_status;
        return result;
    }

    std::uint8_t cheats = 0u;
    if (!reader.ReadByte(&cheats))
    {
        result.status = GoldSrcServerInfoDecodeStatus::kTruncatedField;
        return result;
    }
    if (cheats > 1u)
    {
        result.status = GoldSrcServerInfoDecodeStatus::kInvalidBooleanValue;
        return result;
    }
    decoded.cheats = cheats != 0u;
    if (decoded.cheats)
    {
        result.status = GoldSrcServerInfoDecodeStatus::kCheatsUnsupported;
        return result;
    }
    if (reader.Remaining() != 0u)
    {
        result.status = GoldSrcServerInfoDecodeStatus::kTrailingData;
        return result;
    }

    result.server_info = decoded;
    result.status = GoldSrcServerInfoDecodeStatus::kOk;
    return result;
}

std::string_view ReasonFor(GoldSrcMapChecksumStatus status) noexcept
{
    switch (status)
    {
    case GoldSrcMapChecksumStatus::kOk:
        return "ok";
    case GoldSrcMapChecksumStatus::kNullInput:
        return "null_input";
    case GoldSrcMapChecksumStatus::kOpenFailed:
        return "open_failed";
    case GoldSrcMapChecksumStatus::kReadFailed:
        return "read_failed";
    case GoldSrcMapChecksumStatus::kFileTooLarge:
        return "file_too_large";
    case GoldSrcMapChecksumStatus::kTruncatedHeader:
        return "truncated_header";
    case GoldSrcMapChecksumStatus::kUnsupportedBspVersion:
        return "unsupported_bsp_version";
    case GoldSrcMapChecksumStatus::kInvalidLumpBounds:
        return "invalid_lump_bounds";
    default:
        return "invalid_bsp";
    }
}

GoldSrcMapChecksumResult ComputeGoldSrcBsp30MapChecksum(
    const std::uint8_t* bytes,
    std::size_t size) noexcept
{
    GoldSrcMapChecksumResult result{};
    if (bytes == nullptr)
    {
        result.status = GoldSrcMapChecksumStatus::kNullInput;
        return result;
    }
    if (size > kGoldSrcMaximumBspFileBytes)
    {
        result.status = GoldSrcMapChecksumStatus::kFileTooLarge;
        return result;
    }

    std::array<BspLump, kGoldSrcBspLumpCount> lumps{};
    result.status = ParseBsp30Header(
        bytes,
        size,
        static_cast<std::uint64_t>(size),
        &lumps);
    if (!result.ok())
    {
        return result;
    }

    std::uint32_t checksum = 0xFFFFFFFFu;
    for (std::size_t index = 1u; index < lumps.size(); ++index)
    {
        ProcessCrc32(
            &checksum,
            bytes + lumps[index].offset,
            lumps[index].size);
    }
    result.checksum = checksum;
    return result;
}

GoldSrcMapChecksumResult ComputeGoldSrcBsp30MapChecksum(
    const std::filesystem::path& path)
{
    GoldSrcMapChecksumResult result{};
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input.is_open())
    {
        result.status = GoldSrcMapChecksumStatus::kOpenFailed;
        return result;
    }

    const std::streampos end = input.tellg();
    if (end < std::streampos(0))
    {
        result.status = GoldSrcMapChecksumStatus::kReadFailed;
        return result;
    }
    const std::uint64_t file_size = static_cast<std::uint64_t>(end);
    if (file_size > kGoldSrcMaximumBspFileBytes)
    {
        result.status = GoldSrcMapChecksumStatus::kFileTooLarge;
        return result;
    }
    if (file_size < kGoldSrcBsp30HeaderBytes)
    {
        result.status = GoldSrcMapChecksumStatus::kTruncatedHeader;
        return result;
    }

    input.seekg(0, std::ios::beg);
    std::array<std::uint8_t, kGoldSrcBsp30HeaderBytes> header{};
    input.read(
        reinterpret_cast<char*>(header.data()),
        static_cast<std::streamsize>(header.size()));
    if (input.gcount() != static_cast<std::streamsize>(header.size()))
    {
        result.status = GoldSrcMapChecksumStatus::kReadFailed;
        return result;
    }

    std::array<BspLump, kGoldSrcBspLumpCount> lumps{};
    result.status = ParseBsp30Header(
        header.data(),
        header.size(),
        file_size,
        &lumps);
    if (!result.ok())
    {
        return result;
    }

    std::uint32_t checksum = 0xFFFFFFFFu;
    std::array<std::uint8_t, 32u * 1024u> chunk{};
    for (std::size_t index = 1u; index < lumps.size(); ++index)
    {
        std::size_t remaining = lumps[index].size;
        if (remaining == 0u)
        {
            continue;
        }
        input.clear();
        input.seekg(
            static_cast<std::streamoff>(lumps[index].offset),
            std::ios::beg);
        if (!input)
        {
            result.status = GoldSrcMapChecksumStatus::kReadFailed;
            return result;
        }
        while (remaining != 0u)
        {
            const std::size_t requested = std::min(remaining, chunk.size());
            input.read(
                reinterpret_cast<char*>(chunk.data()),
                static_cast<std::streamsize>(requested));
            if (input.gcount() != static_cast<std::streamsize>(requested))
            {
                result.status = GoldSrcMapChecksumStatus::kReadFailed;
                return result;
            }
            ProcessCrc32(&checksum, chunk.data(), requested);
            remaining -= requested;
        }
    }

    result.status = GoldSrcMapChecksumStatus::kOk;
    result.checksum = checksum;
    return result;
}

std::string_view ReasonFor(GoldSrcMd5Status status) noexcept
{
    switch (status)
    {
    case GoldSrcMd5Status::kOk:
        return "ok";
    case GoldSrcMd5Status::kNullInput:
        return "null_input";
    case GoldSrcMd5Status::kOpenFailed:
        return "open_failed";
    case GoldSrcMd5Status::kReadFailed:
        return "read_failed";
    default:
        return "md5_failed";
    }
}

GoldSrcMd5Result ComputeGoldSrcMd5(
    const std::uint8_t* bytes,
    std::size_t size) noexcept
{
    GoldSrcMd5Result result{};
    if (bytes == nullptr && size != 0u)
    {
        result.status = GoldSrcMd5Status::kNullInput;
        return result;
    }

    Md5Accumulator accumulator;
    if (size != 0u)
    {
        accumulator.Update(bytes, size);
    }
    result.digest = accumulator.Finalize();
    result.status = GoldSrcMd5Status::kOk;
    return result;
}

GoldSrcMd5Result ComputeGoldSrcClientDllMd5(
    const std::filesystem::path& resolved_client_dll_path)
{
    GoldSrcMd5Result result{};
    std::ifstream input(resolved_client_dll_path, std::ios::binary);
    if (!input.is_open())
    {
        result.status = GoldSrcMd5Status::kOpenFailed;
        return result;
    }

    Md5Accumulator accumulator;
    std::array<std::uint8_t, 32u * 1024u> chunk{};
    for (;;)
    {
        input.read(
            reinterpret_cast<char*>(chunk.data()),
            static_cast<std::streamsize>(chunk.size()));
        const std::streamsize count = input.gcount();
        if (count > 0)
        {
            accumulator.Update(chunk.data(), static_cast<std::size_t>(count));
        }
        if (input.bad())
        {
            result.status = GoldSrcMd5Status::kReadFailed;
            return result;
        }
        if (input.eof())
        {
            break;
        }
        if (!input)
        {
            result.status = GoldSrcMd5Status::kReadFailed;
            return result;
        }
    }

    result.digest = accumulator.Finalize();
    result.status = GoldSrcMd5Status::kOk;
    return result;
}
} // namespace hl::network
