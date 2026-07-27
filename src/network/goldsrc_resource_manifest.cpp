#include "network/goldsrc_resource_manifest.h"

#include "network/goldsrc_bitstream.h"

#include <algorithm>
#include <set>
#include <utility>

namespace
{
using namespace hl::network;

struct PreparedResource final
{
    GoldSrcResourceType type = GoldSrcResourceType::kGeneric;
    std::uint16_t index = 0;
    std::uint32_t download_size = 0;
    std::uint8_t flags = 0;
    std::string path;
    std::array<std::uint8_t, kGoldSrcResourceDigestBytes> md5{};
    bool extra_info_present = false;
    std::array<std::uint8_t, kGoldSrcResourceExtraInfoBytes> extra_info{};
};

struct PreparedManifest final
{
    std::vector<PreparedResource> entries;
    std::size_t encoded_bits = 0;
    std::size_t encoded_size = 0;
};

bool IsKnownResourceType(GoldSrcResourceType type) noexcept
{
    switch (type)
    {
    case GoldSrcResourceType::kSound:
    case GoldSrcResourceType::kSkin:
    case GoldSrcResourceType::kModel:
    case GoldSrcResourceType::kDecal:
    case GoldSrcResourceType::kGeneric:
    case GoldSrcResourceType::kEvent:
        return true;
    }
    return false;
}

int ReferenceOrderRank(GoldSrcResourceType type) noexcept
{
    // The observed server order is generic, sound, model, decal, event.
    // Skin is retained as a protocol type for custom resources and follows
    // those server-owned categories deterministically.
    switch (type)
    {
    case GoldSrcResourceType::kGeneric:
        return 0;
    case GoldSrcResourceType::kSound:
        return 1;
    case GoldSrcResourceType::kModel:
        return 2;
    case GoldSrcResourceType::kDecal:
        return 3;
    case GoldSrcResourceType::kEvent:
        return 4;
    case GoldSrcResourceType::kSkin:
        return 5;
    }
    return 6;
}

bool ResourceLess(
    const PreparedResource& left,
    const PreparedResource& right) noexcept
{
    const int left_rank = ReferenceOrderRank(left.type);
    const int right_rank = ReferenceOrderRank(right.type);
    if (left_rank != right_rank)
    {
        return left_rank < right_rank;
    }
    if (left.index != right.index)
    {
        return left.index < right.index;
    }
    return left.path < right.path;
}

bool ResourceLess(
    const GoldSrcResourceEntry& left,
    const GoldSrcResourceEntry& right) noexcept
{
    const int left_rank = ReferenceOrderRank(left.type);
    const int right_rank = ReferenceOrderRank(right.type);
    if (left_rank != right_rank)
    {
        return left_rank < right_rank;
    }
    if (left.index != right.index)
    {
        return left.index < right.index;
    }
    return left.path < right.path;
}

bool IsWorldModelPath(std::string_view path) noexcept
{
    constexpr std::string_view prefix = "maps/";
    constexpr std::string_view suffix = ".bsp";
    return path.size() > prefix.size() + suffix.size()
        && path.compare(0u, prefix.size(), prefix) == 0
        && path.compare(path.size() - suffix.size(), suffix.size(), suffix) == 0;
}

GoldSrcResourceManifestStatus PrepareManifest(
    const GoldSrcResourceManifest& manifest,
    PreparedManifest* prepared) noexcept
{
    if (prepared == nullptr)
    {
        return GoldSrcResourceManifestStatus::kNullInput;
    }

    *prepared = {};
    if (manifest.entries.size() > kGoldSrcMaximumResourceCount)
    {
        return GoldSrcResourceManifestStatus::kResourceCountExceeded;
    }

    prepared->entries.reserve(manifest.entries.size());
    std::set<std::pair<std::uint8_t, std::uint16_t>> owned_indices;
    bool world_model_found = false;

    for (const GoldSrcResourceEntry& entry : manifest.entries)
    {
        if (!IsKnownResourceType(entry.type))
        {
            return GoldSrcResourceManifestStatus::kInvalidResourceType;
        }
        if (entry.index > kGoldSrcMaximumResourceIndex)
        {
            return GoldSrcResourceManifestStatus::kResourceIndexOutOfRange;
        }
        if (entry.download_size > kGoldSrcMaximumResourceDownloadBytes)
        {
            return GoldSrcResourceManifestStatus::kDownloadSizeOutOfRange;
        }
        if (entry.flags > kGoldSrcMaximumResourceFlags)
        {
            return GoldSrcResourceManifestStatus::kResourceFlagsOutOfRange;
        }

        const GoldSrcNormalizedResourcePath normalized =
            NormalizeGoldSrcResourcePath(entry.path);
        if (!normalized.ok())
        {
            return normalized.status;
        }

        const auto ownership = std::make_pair(
            static_cast<std::uint8_t>(entry.type),
            entry.index);
        if (!owned_indices.insert(ownership).second)
        {
            return GoldSrcResourceManifestStatus::kDuplicateTypeIndex;
        }

        if (entry.type == GoldSrcResourceType::kModel && entry.index == 1u)
        {
            if (!IsWorldModelPath(normalized.value))
            {
                return GoldSrcResourceManifestStatus::kInvalidWorldModel;
            }
            world_model_found = true;
        }

        PreparedResource output{};
        output.type = entry.type;
        output.index = entry.index;
        output.download_size = entry.download_size;
        output.flags = entry.flags;
        output.path = normalized.value;
        output.md5 = entry.md5;
        output.extra_info_present = entry.extra_info_present;
        output.extra_info = entry.extra_info;
        prepared->entries.push_back(std::move(output));
    }

    if (!world_model_found)
    {
        return GoldSrcResourceManifestStatus::kMissingWorldModel;
    }

    std::sort(
        prepared->entries.begin(),
        prepared->entries.end(),
        [](const PreparedResource& left, const PreparedResource& right)
        {
            return ResourceLess(left, right);
        });

    std::size_t bits = 8u + 12u;
    for (const PreparedResource& entry : prepared->entries)
    {
        bits += 4u;
        bits += (entry.path.size() + 1u) * 8u;
        bits += 12u + 24u + 3u;
        if ((entry.flags & kGoldSrcResourceCustomFlag) != 0u)
        {
            bits += kGoldSrcResourceDigestBytes * 8u;
        }
        bits += 1u;
        if (entry.extra_info_present)
        {
            bits += kGoldSrcResourceExtraInfoBytes * 8u;
        }
    }
    bits += 1u;

    prepared->encoded_bits = bits;
    prepared->encoded_size = (bits + 7u) / 8u;
    if (prepared->encoded_size > kGoldSrcMaximumResourceManifestBytes)
    {
        return GoldSrcResourceManifestStatus::kRequiresFragmentation;
    }
    return GoldSrcResourceManifestStatus::kOk;
}

GoldSrcResourceManifestStatus ReadProtocolPath(
    GoldSrcBitReader* reader,
    std::string* output) noexcept
{
    if (reader == nullptr || output == nullptr)
    {
        return GoldSrcResourceManifestStatus::kNullInput;
    }
    output->clear();
    for (std::size_t index = 0; index <= kGoldSrcMaximumResourcePathBytes; ++index)
    {
        std::uint32_t character = 0;
        if (!reader->ReadBits(8u, &character))
        {
            return GoldSrcResourceManifestStatus::kTruncatedPayload;
        }
        if (character == 0u)
        {
            return GoldSrcResourceManifestStatus::kOk;
        }
        if (index == kGoldSrcMaximumResourcePathBytes)
        {
            return GoldSrcResourceManifestStatus::kPathTooLong;
        }
        output->push_back(static_cast<char>(character));
    }
    return GoldSrcResourceManifestStatus::kPathTooLong;
}

void WriteLittleEndian32(
    std::array<std::uint8_t, kGoldSrcResourceRequestBytes>* bytes,
    std::size_t offset,
    std::uint32_t value) noexcept
{
    (*bytes)[offset] = static_cast<std::uint8_t>(value & 0xFFu);
    (*bytes)[offset + 1u] = static_cast<std::uint8_t>((value >> 8u) & 0xFFu);
    (*bytes)[offset + 2u] = static_cast<std::uint8_t>((value >> 16u) & 0xFFu);
    (*bytes)[offset + 3u] = static_cast<std::uint8_t>((value >> 24u) & 0xFFu);
}

std::uint32_t ReadLittleEndian32(
    const std::uint8_t* bytes,
    std::size_t offset) noexcept
{
    return static_cast<std::uint32_t>(bytes[offset])
        | (static_cast<std::uint32_t>(bytes[offset + 1u]) << 8u)
        | (static_cast<std::uint32_t>(bytes[offset + 2u]) << 16u)
        | (static_cast<std::uint32_t>(bytes[offset + 3u]) << 24u);
}
} // namespace

namespace hl::network
{
std::string_view ReasonFor(GoldSrcResourceManifestStatus status) noexcept
{
    switch (status)
    {
    case GoldSrcResourceManifestStatus::kOk:
        return "ok";
    case GoldSrcResourceManifestStatus::kNullInput:
        return "null_input";
    case GoldSrcResourceManifestStatus::kEmptyPayload:
        return "empty_payload";
    case GoldSrcResourceManifestStatus::kPayloadTooLarge:
        return "payload_too_large";
    case GoldSrcResourceManifestStatus::kWrongResourceRequestOpcode:
        return "wrong_resource_request_opcode";
    case GoldSrcResourceManifestStatus::kWrongResourceListOpcode:
        return "wrong_resource_list_opcode";
    case GoldSrcResourceManifestStatus::kNonZeroResourceRequestReserved:
        return "nonzero_resource_request_reserved";
    case GoldSrcResourceManifestStatus::kResourceCountExceeded:
        return "resource_count_exceeded";
    case GoldSrcResourceManifestStatus::kInvalidResourceType:
        return "invalid_resource_type";
    case GoldSrcResourceManifestStatus::kResourceIndexOutOfRange:
        return "resource_index_out_of_range";
    case GoldSrcResourceManifestStatus::kDownloadSizeOutOfRange:
        return "download_size_out_of_range";
    case GoldSrcResourceManifestStatus::kResourceFlagsOutOfRange:
        return "resource_flags_out_of_range";
    case GoldSrcResourceManifestStatus::kEmptyPath:
        return "empty_path";
    case GoldSrcResourceManifestStatus::kPathTooLong:
        return "path_too_long";
    case GoldSrcResourceManifestStatus::kEmbeddedNul:
        return "embedded_nul";
    case GoldSrcResourceManifestStatus::kInvalidControlByte:
        return "invalid_control_byte";
    case GoldSrcResourceManifestStatus::kAbsolutePath:
        return "absolute_path";
    case GoldSrcResourceManifestStatus::kDriveOrColonPath:
        return "drive_or_colon_path";
    case GoldSrcResourceManifestStatus::kEmptyPathSegment:
        return "empty_path_segment";
    case GoldSrcResourceManifestStatus::kDotPathSegment:
        return "dot_path_segment";
    case GoldSrcResourceManifestStatus::kParentTraversal:
        return "parent_traversal";
    case GoldSrcResourceManifestStatus::kNonCanonicalPath:
        return "noncanonical_path";
    case GoldSrcResourceManifestStatus::kDuplicateTypeIndex:
        return "duplicate_type_index";
    case GoldSrcResourceManifestStatus::kMissingWorldModel:
        return "missing_world_model";
    case GoldSrcResourceManifestStatus::kInvalidWorldModel:
        return "invalid_world_model";
    case GoldSrcResourceManifestStatus::kInvalidResourceOrder:
        return "invalid_resource_order";
    case GoldSrcResourceManifestStatus::kRequiresFragmentation:
        return "requires_fragmentation";
    case GoldSrcResourceManifestStatus::kOutputCapacityExceeded:
        return "output_capacity_exceeded";
    case GoldSrcResourceManifestStatus::kTruncatedPayload:
        return "truncated_payload";
    case GoldSrcResourceManifestStatus::kConsistencyDataUnsupported:
        return "consistency_data_unsupported";
    case GoldSrcResourceManifestStatus::kNonZeroPadding:
        return "nonzero_padding";
    case GoldSrcResourceManifestStatus::kTrailingData:
        return "trailing_data";
    }
    return "unknown_resource_manifest_status";
}

std::string_view NameFor(GoldSrcResourceManifestSource source) noexcept
{
    switch (source)
    {
    case GoldSrcResourceManifestSource::kAuthoritativeRuntime:
        return "authoritative-runtime";
    case GoldSrcResourceManifestSource::kExplicitTestFixture:
        return "explicit-test-fixture";
    default:
        return "invalid";
    }
}

GoldSrcResourceManifestPreparationState
GoldSrcResourceManifestPreparationCache::state() const noexcept
{
    return state_;
}

bool GoldSrcResourceManifestPreparationCache::TryStore(
    GoldSrcResourceManifestPreparationState state) noexcept
{
    if (state == GoldSrcResourceManifestPreparationState::kNotAttempted
        || state_
            != GoldSrcResourceManifestPreparationState::kNotAttempted)
    {
        return false;
    }
    state_ = state;
    return true;
}

void GoldSrcResourceManifestPreparationCache::Reset() noexcept
{
    state_ = GoldSrcResourceManifestPreparationState::kNotAttempted;
}

GoldSrcNormalizedResourcePath NormalizeGoldSrcResourcePath(
    std::string_view path)
{
    GoldSrcNormalizedResourcePath result{};
    if (path.empty())
    {
        result.status = GoldSrcResourceManifestStatus::kEmptyPath;
        return result;
    }

    for (const char character : path)
    {
        const unsigned char value = static_cast<unsigned char>(character);
        if (value == 0u)
        {
            result.status = GoldSrcResourceManifestStatus::kEmbeddedNul;
            return result;
        }
        if (value < 0x20u || value == 0x7Fu)
        {
            result.status = GoldSrcResourceManifestStatus::kInvalidControlByte;
            return result;
        }
        if (character == ':')
        {
            result.status = GoldSrcResourceManifestStatus::kDriveOrColonPath;
            return result;
        }
    }

    std::size_t begin = 0;
    while (begin < path.size() && path[begin] == ' ')
    {
        ++begin;
    }
    std::size_t end = path.size();
    while (end > begin && path[end - 1u] == ' ')
    {
        --end;
    }
    if (begin == end)
    {
        result.status = GoldSrcResourceManifestStatus::kEmptyPath;
        return result;
    }
    if (path[begin] == '/' || path[begin] == '\\')
    {
        result.status = GoldSrcResourceManifestStatus::kAbsolutePath;
        return result;
    }

    result.value.reserve(end - begin);
    for (std::size_t index = begin; index < end; ++index)
    {
        unsigned char character = static_cast<unsigned char>(path[index]);
        if (character == '\\')
        {
            character = '/';
        }
        else if (character >= 'A' && character <= 'Z')
        {
            character = static_cast<unsigned char>(character - 'A' + 'a');
        }
        result.value.push_back(static_cast<char>(character));
    }

    if (result.value.size() > kGoldSrcMaximumResourcePathBytes)
    {
        result.status = GoldSrcResourceManifestStatus::kPathTooLong;
        result.value.clear();
        return result;
    }

    std::size_t segment_begin = 0;
    while (segment_begin <= result.value.size())
    {
        const std::size_t slash = result.value.find('/', segment_begin);
        const std::size_t segment_end =
            slash == std::string::npos ? result.value.size() : slash;
        const std::string_view segment(
            result.value.data() + segment_begin,
            segment_end - segment_begin);
        if (segment.empty())
        {
            result.status = GoldSrcResourceManifestStatus::kEmptyPathSegment;
            result.value.clear();
            return result;
        }
        if (segment == ".")
        {
            result.status = GoldSrcResourceManifestStatus::kDotPathSegment;
            result.value.clear();
            return result;
        }
        if (segment == "..")
        {
            result.status = GoldSrcResourceManifestStatus::kParentTraversal;
            result.value.clear();
            return result;
        }
        if (slash == std::string::npos)
        {
            break;
        }
        segment_begin = slash + 1u;
    }

    result.status = GoldSrcResourceManifestStatus::kOk;
    return result;
}

GoldSrcResourceManifestValidationResult ValidateGoldSrcResourceManifest(
    const GoldSrcResourceManifest& manifest) noexcept
{
    PreparedManifest prepared{};
    GoldSrcResourceManifestValidationResult result{};
    result.status = PrepareManifest(manifest, &prepared);
    result.encoded_size = prepared.encoded_size;
    return result;
}

GoldSrcResourceManifestEncodeResult EncodeGoldSrcResourceManifest(
    const GoldSrcResourceManifest& manifest,
    std::size_t output_capacity) noexcept
{
    GoldSrcResourceManifestEncodeResult result{};
    PreparedManifest prepared{};
    result.status = PrepareManifest(manifest, &prepared);
    if (result.status != GoldSrcResourceManifestStatus::kOk)
    {
        return result;
    }
    if (prepared.encoded_size > output_capacity)
    {
        result.status = GoldSrcResourceManifestStatus::kOutputCapacityExceeded;
        return result;
    }

    const std::size_t bounded_capacity = std::min(
        output_capacity,
        result.payload.bytes.size());
    GoldSrcBitWriter writer(result.payload.bytes.data(), bounded_capacity);
    bool wrote = writer.WriteBits(kGoldSrcResourceListOpcode, 8u)
        && writer.WriteBits(
            static_cast<std::uint32_t>(prepared.entries.size()),
            12u);

    for (const PreparedResource& entry : prepared.entries)
    {
        wrote = wrote
            && writer.WriteBits(static_cast<std::uint8_t>(entry.type), 4u)
            && writer.WriteString(entry.path)
            && writer.WriteBits(entry.index, 12u)
            && writer.WriteBits(entry.download_size, 24u)
            && writer.WriteBits(entry.flags, 3u);
        if ((entry.flags & kGoldSrcResourceCustomFlag) != 0u)
        {
            wrote = wrote && writer.WriteBytes(entry.md5.data(), entry.md5.size());
        }
        wrote = wrote && writer.WriteBits(entry.extra_info_present ? 1u : 0u, 1u);
        if (entry.extra_info_present)
        {
            wrote = wrote
                && writer.WriteBytes(entry.extra_info.data(), entry.extra_info.size());
        }
    }
    wrote = wrote && writer.WriteBits(0u, 1u);

    if (!wrote || writer.bit_position() != prepared.encoded_bits)
    {
        result.status = GoldSrcResourceManifestStatus::kOutputCapacityExceeded;
        result.payload = {};
        return result;
    }

    result.payload.size = prepared.encoded_size;
    result.status = GoldSrcResourceManifestStatus::kOk;
    return result;
}

GoldSrcResourceRequestEncodeResult EncodeGoldSrcResourceRequest(
    std::uint32_t spawn_count,
    std::size_t output_capacity) noexcept
{
    GoldSrcResourceRequestEncodeResult result{};
    if (output_capacity < kGoldSrcResourceRequestBytes)
    {
        result.status = GoldSrcResourceManifestStatus::kOutputCapacityExceeded;
        return result;
    }
    result.payload.bytes[0] = kGoldSrcResourceRequestOpcode;
    WriteLittleEndian32(&result.payload.bytes, 1u, spawn_count);
    WriteLittleEndian32(&result.payload.bytes, 5u, 0u);
    result.payload.size = kGoldSrcResourceRequestBytes;
    result.status = GoldSrcResourceManifestStatus::kOk;
    return result;
}

GoldSrcResourceRequestDecodeResult DecodeGoldSrcResourceRequest(
    const std::uint8_t* bytes,
    std::size_t size) noexcept
{
    GoldSrcResourceRequestDecodeResult result{};
    if (bytes == nullptr)
    {
        result.status = size == 0u
            ? GoldSrcResourceManifestStatus::kEmptyPayload
            : GoldSrcResourceManifestStatus::kNullInput;
        return result;
    }
    if (size == 0u)
    {
        result.status = GoldSrcResourceManifestStatus::kEmptyPayload;
        return result;
    }
    if (size < kGoldSrcResourceRequestBytes)
    {
        result.status = GoldSrcResourceManifestStatus::kTruncatedPayload;
        return result;
    }
    if (size > kGoldSrcResourceRequestBytes)
    {
        result.status = GoldSrcResourceManifestStatus::kTrailingData;
        return result;
    }
    if (bytes[0] != kGoldSrcResourceRequestOpcode)
    {
        result.status = GoldSrcResourceManifestStatus::kWrongResourceRequestOpcode;
        return result;
    }
    if (ReadLittleEndian32(bytes, 5u) != 0u)
    {
        result.status =
            GoldSrcResourceManifestStatus::kNonZeroResourceRequestReserved;
        return result;
    }
    result.spawn_count = ReadLittleEndian32(bytes, 1u);
    result.status = GoldSrcResourceManifestStatus::kOk;
    return result;
}

GoldSrcResourceManifestDecodeResult DecodeGoldSrcResourceManifest(
    const std::uint8_t* bytes,
    std::size_t size) noexcept
{
    GoldSrcResourceManifestDecodeResult result{};
    if (bytes == nullptr)
    {
        result.status = size == 0u
            ? GoldSrcResourceManifestStatus::kEmptyPayload
            : GoldSrcResourceManifestStatus::kNullInput;
        return result;
    }
    if (size == 0u)
    {
        result.status = GoldSrcResourceManifestStatus::kEmptyPayload;
        return result;
    }
    if (size > kGoldSrcMaximumResourceManifestBytes)
    {
        result.status = GoldSrcResourceManifestStatus::kPayloadTooLarge;
        return result;
    }

    GoldSrcBitReader reader(bytes, size);
    std::uint32_t value = 0;
    if (!reader.ReadBits(8u, &value))
    {
        result.status = GoldSrcResourceManifestStatus::kTruncatedPayload;
        return result;
    }
    if (value != kGoldSrcResourceListOpcode)
    {
        result.status = GoldSrcResourceManifestStatus::kWrongResourceListOpcode;
        return result;
    }
    if (!reader.ReadBits(12u, &value))
    {
        result.status = GoldSrcResourceManifestStatus::kTruncatedPayload;
        return result;
    }
    const std::size_t resource_count = value;
    if (resource_count > kGoldSrcMaximumResourceCount)
    {
        result.status = GoldSrcResourceManifestStatus::kResourceCountExceeded;
        return result;
    }
    result.manifest.entries.reserve(resource_count);

    for (std::size_t resource = 0; resource < resource_count; ++resource)
    {
        GoldSrcResourceEntry entry{};
        if (!reader.ReadBits(4u, &value))
        {
            result.status = GoldSrcResourceManifestStatus::kTruncatedPayload;
            return result;
        }
        if (value > static_cast<std::uint8_t>(GoldSrcResourceType::kEvent))
        {
            result.status = GoldSrcResourceManifestStatus::kInvalidResourceType;
            return result;
        }
        entry.type = static_cast<GoldSrcResourceType>(value);

        result.status = ReadProtocolPath(&reader, &entry.path);
        if (result.status != GoldSrcResourceManifestStatus::kOk)
        {
            return result;
        }
        const GoldSrcNormalizedResourcePath normalized =
            NormalizeGoldSrcResourcePath(entry.path);
        if (!normalized.ok())
        {
            result.status = normalized.status;
            return result;
        }
        if (normalized.value != entry.path)
        {
            result.status = GoldSrcResourceManifestStatus::kNonCanonicalPath;
            return result;
        }

        if (!reader.ReadBits(12u, &value))
        {
            result.status = GoldSrcResourceManifestStatus::kTruncatedPayload;
            return result;
        }
        entry.index = static_cast<std::uint16_t>(value);
        if (!reader.ReadBits(24u, &value))
        {
            result.status = GoldSrcResourceManifestStatus::kTruncatedPayload;
            return result;
        }
        entry.download_size = value;
        if (!reader.ReadBits(3u, &value))
        {
            result.status = GoldSrcResourceManifestStatus::kTruncatedPayload;
            return result;
        }
        entry.flags = static_cast<std::uint8_t>(value);

        if ((entry.flags & kGoldSrcResourceCustomFlag) != 0u)
        {
            for (std::uint8_t& byte : entry.md5)
            {
                if (!reader.ReadBits(8u, &value))
                {
                    result.status = GoldSrcResourceManifestStatus::kTruncatedPayload;
                    return result;
                }
                byte = static_cast<std::uint8_t>(value);
            }
        }

        if (!reader.ReadBits(1u, &value))
        {
            result.status = GoldSrcResourceManifestStatus::kTruncatedPayload;
            return result;
        }
        entry.extra_info_present = value != 0u;
        if (entry.extra_info_present)
        {
            for (std::uint8_t& byte : entry.extra_info)
            {
                if (!reader.ReadBits(8u, &value))
                {
                    result.status = GoldSrcResourceManifestStatus::kTruncatedPayload;
                    return result;
                }
                byte = static_cast<std::uint8_t>(value);
            }
        }

        if (!result.manifest.entries.empty()
            && ResourceLess(entry, result.manifest.entries.back()))
        {
            result.status = GoldSrcResourceManifestStatus::kInvalidResourceOrder;
            return result;
        }
        result.manifest.entries.push_back(std::move(entry));
    }

    if (!reader.ReadBits(1u, &value))
    {
        result.status = GoldSrcResourceManifestStatus::kTruncatedPayload;
        return result;
    }
    if (value != 0u)
    {
        result.status = GoldSrcResourceManifestStatus::kConsistencyDataUnsupported;
        return result;
    }

    const std::size_t rounded_size = (reader.bit_position() + 7u) / 8u;
    if (rounded_size < size)
    {
        result.status = GoldSrcResourceManifestStatus::kTrailingData;
        return result;
    }
    while ((reader.bit_position() % 8u) != 0u)
    {
        if (!reader.ReadBits(1u, &value))
        {
            result.status = GoldSrcResourceManifestStatus::kTruncatedPayload;
            return result;
        }
        if (value != 0u)
        {
            result.status = GoldSrcResourceManifestStatus::kNonZeroPadding;
            return result;
        }
    }

    PreparedManifest prepared{};
    result.status = PrepareManifest(result.manifest, &prepared);
    return result;
}
} // namespace hl::network
