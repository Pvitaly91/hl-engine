#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace hl::network
{
inline constexpr std::uint8_t kGoldSrcResourceListOpcode = 43u;
inline constexpr std::uint8_t kGoldSrcResourceRequestOpcode = 45u;
inline constexpr std::size_t kGoldSrcMaximumResourcePathBytes = 63u;
inline constexpr std::size_t kGoldSrcMaximumResourceCount = 1280u;
inline constexpr std::size_t kGoldSrcMaximumResourceManifestBytes = 65536u;
inline constexpr std::size_t kGoldSrcResourceRequestBytes = 9u;
inline constexpr std::size_t kGoldSrcResourceDigestBytes = 16u;
inline constexpr std::size_t kGoldSrcResourceExtraInfoBytes = 32u;
inline constexpr std::uint16_t kGoldSrcMaximumResourceIndex = 0x0FFFu;
inline constexpr std::uint32_t kGoldSrcMaximumResourceDownloadBytes = 0x00FFFFFFu;
inline constexpr std::uint8_t kGoldSrcMaximumResourceFlags = 0x07u;
inline constexpr std::uint8_t kGoldSrcResourceFatalIfMissingFlag = 1u << 0u;
inline constexpr std::uint8_t kGoldSrcResourceWasMissingFlag = 1u << 1u;
inline constexpr std::uint8_t kGoldSrcResourceCustomFlag = 1u << 2u;

enum class GoldSrcResourceType : std::uint8_t
{
    kSound = 0u,
    kSkin = 1u,
    kModel = 2u,
    kDecal = 3u,
    kGeneric = 4u,
    kEvent = 5u,
};

enum class GoldSrcResourceManifestStatus
{
    kOk,
    kNullInput,
    kEmptyPayload,
    kPayloadTooLarge,
    kWrongResourceRequestOpcode,
    kWrongResourceListOpcode,
    kNonZeroResourceRequestReserved,
    kResourceCountExceeded,
    kInvalidResourceType,
    kResourceIndexOutOfRange,
    kDownloadSizeOutOfRange,
    kResourceFlagsOutOfRange,
    kEmptyPath,
    kPathTooLong,
    kEmbeddedNul,
    kInvalidControlByte,
    kAbsolutePath,
    kDriveOrColonPath,
    kEmptyPathSegment,
    kDotPathSegment,
    kParentTraversal,
    kNonCanonicalPath,
    kDuplicateTypeIndex,
    kMissingWorldModel,
    kInvalidWorldModel,
    kInvalidResourceOrder,
    kRequiresFragmentation,
    kOutputCapacityExceeded,
    kTruncatedPayload,
    kConsistencyDataUnsupported,
    kNonZeroPadding,
    kTrailingData,
};

enum class GoldSrcResourceManifestPreparationState
{
    kNotAttempted,
    kReady,
    kRequiresFragmentation,
};

enum class GoldSrcResourceManifestSource : std::uint8_t
{
    kAuthoritativeRuntime,
    kExplicitTestFixture,
};

std::string_view NameFor(GoldSrcResourceManifestSource source) noexcept;

template<
    typename AuthoritativeRuntimeBuilder,
    typename ExplicitTestFixtureBuilder>
bool DispatchGoldSrcResourceManifestBuild(
    GoldSrcResourceManifestSource source,
    AuthoritativeRuntimeBuilder&& authoritative_runtime_builder,
    ExplicitTestFixtureBuilder&& explicit_test_fixture_builder)
{
    switch (source)
    {
    case GoldSrcResourceManifestSource::kAuthoritativeRuntime:
        return authoritative_runtime_builder();
    case GoldSrcResourceManifestSource::kExplicitTestFixture:
        return explicit_test_fixture_builder();
    default:
        return false;
    }
}

class GoldSrcResourceManifestPreparationCache final
{
public:
    GoldSrcResourceManifestPreparationState state() const noexcept;
    bool TryStore(GoldSrcResourceManifestPreparationState state) noexcept;
    void Reset() noexcept;

private:
    GoldSrcResourceManifestPreparationState state_ =
        GoldSrcResourceManifestPreparationState::kNotAttempted;
};

std::string_view ReasonFor(GoldSrcResourceManifestStatus status) noexcept;

struct GoldSrcNormalizedResourcePath final
{
    GoldSrcResourceManifestStatus status =
        GoldSrcResourceManifestStatus::kEmptyPath;
    std::string value;

    bool ok() const noexcept
    {
        return status == GoldSrcResourceManifestStatus::kOk;
    }
};

GoldSrcNormalizedResourcePath NormalizeGoldSrcResourcePath(
    std::string_view path);

struct GoldSrcResourceEntry final
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

struct GoldSrcResourceManifest final
{
    std::vector<GoldSrcResourceEntry> entries;
};

struct GoldSrcResourceManifestValidationResult final
{
    GoldSrcResourceManifestStatus status =
        GoldSrcResourceManifestStatus::kMissingWorldModel;
    std::size_t encoded_size = 0;

    bool ok() const noexcept
    {
        return status == GoldSrcResourceManifestStatus::kOk;
    }
};

GoldSrcResourceManifestValidationResult ValidateGoldSrcResourceManifest(
    const GoldSrcResourceManifest& manifest) noexcept;

struct GoldSrcResourceManifestPayload final
{
    std::array<std::uint8_t, kGoldSrcMaximumResourceManifestBytes> bytes{};
    std::size_t size = 0;
};

struct GoldSrcResourceManifestEncodeResult final
{
    GoldSrcResourceManifestStatus status =
        GoldSrcResourceManifestStatus::kMissingWorldModel;
    GoldSrcResourceManifestPayload payload;

    bool ok() const noexcept
    {
        return status == GoldSrcResourceManifestStatus::kOk;
    }
};

// Encodes exactly one svc_resourcelist. The caller may supply a smaller
// capacity to exercise or enforce a tighter transport bound. The returned
// fixed storage is zero-initialized, including padding and unused bytes.
GoldSrcResourceManifestEncodeResult EncodeGoldSrcResourceManifest(
    const GoldSrcResourceManifest& manifest,
    std::size_t output_capacity = kGoldSrcMaximumResourceManifestBytes) noexcept;

struct GoldSrcResourceRequestPayload final
{
    std::array<std::uint8_t, kGoldSrcResourceRequestBytes> bytes{};
    std::size_t size = 0;
};

struct GoldSrcResourceRequestEncodeResult final
{
    GoldSrcResourceManifestStatus status =
        GoldSrcResourceManifestStatus::kOutputCapacityExceeded;
    GoldSrcResourceRequestPayload payload;

    bool ok() const noexcept
    {
        return status == GoldSrcResourceManifestStatus::kOk;
    }
};

// The reference flow sends this fixed-width companion separately, before the
// bit-packed resource list. The reserved uint32 is always encoded as zero.
GoldSrcResourceRequestEncodeResult EncodeGoldSrcResourceRequest(
    std::uint32_t spawn_count,
    std::size_t output_capacity = kGoldSrcResourceRequestBytes) noexcept;

struct GoldSrcResourceRequestDecodeResult final
{
    GoldSrcResourceManifestStatus status =
        GoldSrcResourceManifestStatus::kEmptyPayload;
    std::uint32_t spawn_count = 0;

    bool ok() const noexcept
    {
        return status == GoldSrcResourceManifestStatus::kOk;
    }
};

GoldSrcResourceRequestDecodeResult DecodeGoldSrcResourceRequest(
    const std::uint8_t* bytes,
    std::size_t size) noexcept;

struct GoldSrcResourceManifestDecodeResult final
{
    GoldSrcResourceManifestStatus status =
        GoldSrcResourceManifestStatus::kEmptyPayload;
    GoldSrcResourceManifest manifest;

    bool ok() const noexcept
    {
        return status == GoldSrcResourceManifestStatus::kOk;
    }
};

// Test/proof-side semantic decoder for exactly one svc_resourcelist. It
// rejects non-canonical paths/order, unsupported consistency data, non-zero
// padding, and trailing bytes.
GoldSrcResourceManifestDecodeResult DecodeGoldSrcResourceManifest(
    const std::uint8_t* bytes,
    std::size_t size) noexcept;
} // namespace hl::network
