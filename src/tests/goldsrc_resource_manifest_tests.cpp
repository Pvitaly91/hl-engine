#include "network/goldsrc_resource_manifest.h"
#include "network/goldsrc_netchan.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace
{
using namespace hl::network;

GoldSrcResourceEntry Resource(
    GoldSrcResourceType type,
    std::uint16_t index,
    std::string path,
    std::uint32_t download_size = 0u,
    std::uint8_t flags = 0u)
{
    GoldSrcResourceEntry entry{};
    entry.type = type;
    entry.index = index;
    entry.path = std::move(path);
    entry.download_size = download_size;
    entry.flags = flags;
    return entry;
}

GoldSrcResourceManifest MinimalManifest()
{
    GoldSrcResourceManifest manifest{};
    manifest.entries.push_back(Resource(
        GoldSrcResourceType::kModel,
        1u,
        "maps/a.bsp"));
    return manifest;
}

GoldSrcResourceManifest RichManifest()
{
    GoldSrcResourceManifest manifest{};

    GoldSrcResourceEntry skin = Resource(
        GoldSrcResourceType::kSkin,
        8u,
        "skins/player.bmp",
        80u);
    manifest.entries.push_back(skin);

    GoldSrcResourceEntry event = Resource(
        GoldSrcResourceType::kEvent,
        5u,
        "events/test.sc",
        50u);
    event.extra_info_present = true;
    for (std::size_t index = 0; index < event.extra_info.size(); ++index)
    {
        event.extra_info[index] = static_cast<std::uint8_t>(0x80u + index);
    }
    manifest.entries.push_back(event);

    manifest.entries.push_back(Resource(
        GoldSrcResourceType::kModel,
        2u,
        "MODELS\\crate.MDL",
        200u));
    manifest.entries.push_back(Resource(
        GoldSrcResourceType::kDecal,
        4u,
        "decals/test.wad",
        40u));
    manifest.entries.push_back(Resource(
        GoldSrcResourceType::kModel,
        1u,
        " MAPS\\TEST.BSP ",
        100u));
    manifest.entries.push_back(Resource(
        GoldSrcResourceType::kSound,
        3u,
        "sound/test.wav",
        30u));

    GoldSrcResourceEntry generic = Resource(
        GoldSrcResourceType::kGeneric,
        7u,
        "gfx/test.tga",
        70u,
        kGoldSrcResourceCustomFlag);
    for (std::size_t index = 0; index < generic.md5.size(); ++index)
    {
        generic.md5[index] = static_cast<std::uint8_t>(index + 1u);
    }
    manifest.entries.push_back(generic);
    return manifest;
}

void AssertUnusedBytesAreZero(const GoldSrcResourceManifestPayload& payload)
{
    assert(payload.size <= payload.bytes.size());
    assert(std::all_of(
        payload.bytes.begin() + static_cast<std::ptrdiff_t>(payload.size),
        payload.bytes.end(),
        [](std::uint8_t byte)
        {
            return byte == 0u;
        }));
}

void TestProtocolConstantsAndStableReasons()
{
    static_assert(static_cast<std::uint8_t>(GoldSrcResourceType::kSound) == 0u);
    static_assert(static_cast<std::uint8_t>(GoldSrcResourceType::kSkin) == 1u);
    static_assert(static_cast<std::uint8_t>(GoldSrcResourceType::kModel) == 2u);
    static_assert(static_cast<std::uint8_t>(GoldSrcResourceType::kDecal) == 3u);
    static_assert(static_cast<std::uint8_t>(GoldSrcResourceType::kGeneric) == 4u);
    static_assert(static_cast<std::uint8_t>(GoldSrcResourceType::kEvent) == 5u);
    static_assert(kGoldSrcResourceListOpcode == 43u);
    static_assert(kGoldSrcResourceRequestOpcode == 45u);
    static_assert(kGoldSrcMaximumResourceCount == 1280u);
    static_assert(kGoldSrcMaximumResourceManifestBytes == 65536u);

    assert(ReasonFor(GoldSrcResourceManifestStatus::kOk) == "ok");
    assert(
        ReasonFor(GoldSrcResourceManifestStatus::kRequiresFragmentation)
        == "requires_fragmentation");
    assert(
        ReasonFor(GoldSrcResourceManifestStatus::kDuplicateTypeIndex)
        == "duplicate_type_index");
}

void TestPreparationOutcomeCacheIsImmutableUntilReset()
{
    GoldSrcResourceManifestPreparationCache cache;
    assert(
        cache.state()
        == GoldSrcResourceManifestPreparationState::kNotAttempted);
    assert(!cache.TryStore(
        GoldSrcResourceManifestPreparationState::kNotAttempted));

    assert(cache.TryStore(
        GoldSrcResourceManifestPreparationState::kRequiresFragmentation));
    assert(
        cache.state()
        == GoldSrcResourceManifestPreparationState::kRequiresFragmentation);
    assert(!cache.TryStore(
        GoldSrcResourceManifestPreparationState::kRequiresFragmentation));
    assert(!cache.TryStore(
        GoldSrcResourceManifestPreparationState::kReady));

    cache.Reset();
    assert(
        cache.state()
        == GoldSrcResourceManifestPreparationState::kNotAttempted);
    assert(cache.TryStore(
        GoldSrcResourceManifestPreparationState::kReady));
    assert(
        cache.state()
        == GoldSrcResourceManifestPreparationState::kReady);
    assert(!cache.TryStore(
        GoldSrcResourceManifestPreparationState::kRequiresFragmentation));
}

void TestManifestSourceDispatchInvokesExactlyOneBuilder()
{
    int authoritative_runtime_calls = 0;
    int explicit_test_fixture_calls = 0;
    const auto authoritative_runtime_builder = [&]()
    {
        ++authoritative_runtime_calls;
        return true;
    };
    const auto explicit_test_fixture_builder = [&]()
    {
        ++explicit_test_fixture_calls;
        return false;
    };

    assert(DispatchGoldSrcResourceManifestBuild(
        GoldSrcResourceManifestSource::kAuthoritativeRuntime,
        authoritative_runtime_builder,
        explicit_test_fixture_builder));
    assert(authoritative_runtime_calls == 1);
    assert(explicit_test_fixture_calls == 0);
    assert(
        NameFor(GoldSrcResourceManifestSource::kAuthoritativeRuntime)
        == "authoritative-runtime");

    authoritative_runtime_calls = 0;
    assert(!DispatchGoldSrcResourceManifestBuild(
        GoldSrcResourceManifestSource::kExplicitTestFixture,
        authoritative_runtime_builder,
        explicit_test_fixture_builder));
    assert(authoritative_runtime_calls == 0);
    assert(explicit_test_fixture_calls == 1);
    assert(
        NameFor(GoldSrcResourceManifestSource::kExplicitTestFixture)
        == "explicit-test-fixture");

    authoritative_runtime_calls = 0;
    explicit_test_fixture_calls = 0;
    const GoldSrcResourceManifestSource invalid_source =
        static_cast<GoldSrcResourceManifestSource>(0xFFu);
    assert(!DispatchGoldSrcResourceManifestBuild(
        invalid_source,
        authoritative_runtime_builder,
        explicit_test_fixture_builder));
    assert(authoritative_runtime_calls == 0);
    assert(explicit_test_fixture_calls == 0);
    assert(NameFor(invalid_source) == "invalid");
}

void TestPathNormalizationAndBounds()
{
    const GoldSrcNormalizedResourcePath normalized =
        NormalizeGoldSrcResourcePath(" Models\\Player.MDL ");
    assert(normalized.ok());
    assert(normalized.value == "models/player.mdl");

    const std::string maximum(kGoldSrcMaximumResourcePathBytes, 'a');
    assert(NormalizeGoldSrcResourcePath(maximum).ok());
    assert(
        NormalizeGoldSrcResourcePath(maximum + "a").status
        == GoldSrcResourceManifestStatus::kPathTooLong);

    assert(
        NormalizeGoldSrcResourcePath("").status
        == GoldSrcResourceManifestStatus::kEmptyPath);
    assert(
        NormalizeGoldSrcResourcePath("   ").status
        == GoldSrcResourceManifestStatus::kEmptyPath);
    assert(
        NormalizeGoldSrcResourcePath(std::string("a\0b", 3u)).status
        == GoldSrcResourceManifestStatus::kEmbeddedNul);
    assert(
        NormalizeGoldSrcResourcePath("a\nb").status
        == GoldSrcResourceManifestStatus::kInvalidControlByte);
    assert(
        NormalizeGoldSrcResourcePath("/maps/a.bsp").status
        == GoldSrcResourceManifestStatus::kAbsolutePath);
    assert(
        NormalizeGoldSrcResourcePath("\\\\server\\share").status
        == GoldSrcResourceManifestStatus::kAbsolutePath);
    assert(
        NormalizeGoldSrcResourcePath("c:\\maps\\a.bsp").status
        == GoldSrcResourceManifestStatus::kDriveOrColonPath);
    assert(
        NormalizeGoldSrcResourcePath("models//a.mdl").status
        == GoldSrcResourceManifestStatus::kEmptyPathSegment);
    assert(
        NormalizeGoldSrcResourcePath("models/./a.mdl").status
        == GoldSrcResourceManifestStatus::kDotPathSegment);
    assert(
        NormalizeGoldSrcResourcePath("models/../a.mdl").status
        == GoldSrcResourceManifestStatus::kParentTraversal);
    assert(
        NormalizeGoldSrcResourcePath("models/a.mdl/").status
        == GoldSrcResourceManifestStatus::kEmptyPathSegment);
}

void TestResourceRequestCompanionGoldenAndDecoder()
{
    constexpr std::array<std::uint8_t, kGoldSrcResourceRequestBytes> golden = {
        0x2Du,
        0x78u, 0x56u, 0x34u, 0x12u,
        0x00u, 0x00u, 0x00u, 0x00u,
    };
    const GoldSrcResourceRequestEncodeResult encoded =
        EncodeGoldSrcResourceRequest(0x12345678u);
    assert(encoded.ok());
    assert(encoded.payload.size == golden.size());
    assert(encoded.payload.bytes == golden);

    const GoldSrcResourceRequestDecodeResult decoded =
        DecodeGoldSrcResourceRequest(golden.data(), golden.size());
    assert(decoded.ok());
    assert(decoded.spawn_count == 0x12345678u);

    assert(
        EncodeGoldSrcResourceRequest(1u, golden.size() - 1u).status
        == GoldSrcResourceManifestStatus::kOutputCapacityExceeded);
    assert(
        DecodeGoldSrcResourceRequest(nullptr, 1u).status
        == GoldSrcResourceManifestStatus::kNullInput);
    assert(
        DecodeGoldSrcResourceRequest(nullptr, 0u).status
        == GoldSrcResourceManifestStatus::kEmptyPayload);
    assert(
        DecodeGoldSrcResourceRequest(golden.data(), golden.size() - 1u).status
        == GoldSrcResourceManifestStatus::kTruncatedPayload);

    auto wrong_opcode = golden;
    wrong_opcode[0] = 0u;
    assert(
        DecodeGoldSrcResourceRequest(wrong_opcode.data(), wrong_opcode.size()).status
        == GoldSrcResourceManifestStatus::kWrongResourceRequestOpcode);
    auto nonzero_reserved = golden;
    nonzero_reserved[5] = 1u;
    assert(
        DecodeGoldSrcResourceRequest(
            nonzero_reserved.data(),
            nonzero_reserved.size()).status
        == GoldSrcResourceManifestStatus::kNonZeroResourceRequestReserved);
    std::array<std::uint8_t, kGoldSrcResourceRequestBytes + 1u> trailing{};
    std::copy(golden.begin(), golden.end(), trailing.begin());
    assert(
        DecodeGoldSrcResourceRequest(trailing.data(), trailing.size()).status
        == GoldSrcResourceManifestStatus::kTrailingData);
}

void TestResourceListGoldenAndStrictDecode()
{
    constexpr std::array<std::uint8_t, 20> golden = {
        0x2Bu, 0x01u, 0x20u,
        0x6Du, 0x61u, 0x70u, 0x73u, 0x2Fu,
        0x61u, 0x2Eu, 0x62u, 0x73u, 0x70u, 0x00u,
        0x01u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
    };
    const GoldSrcResourceManifest manifest = MinimalManifest();
    const GoldSrcResourceManifestEncodeResult encoded =
        EncodeGoldSrcResourceManifest(manifest);
    assert(encoded.ok());
    assert(encoded.payload.size == golden.size());
    assert(std::equal(
        golden.begin(),
        golden.end(),
        encoded.payload.bytes.begin()));
    AssertUnusedBytesAreZero(encoded.payload);

    const GoldSrcResourceManifestDecodeResult decoded =
        DecodeGoldSrcResourceManifest(golden.data(), golden.size());
    assert(decoded.ok());
    assert(decoded.manifest.entries.size() == 1u);
    assert(decoded.manifest.entries[0].type == GoldSrcResourceType::kModel);
    assert(decoded.manifest.entries[0].index == 1u);
    assert(decoded.manifest.entries[0].path == "maps/a.bsp");

    assert(
        DecodeGoldSrcResourceManifest(nullptr, 1u).status
        == GoldSrcResourceManifestStatus::kNullInput);
    assert(
        DecodeGoldSrcResourceManifest(nullptr, 0u).status
        == GoldSrcResourceManifestStatus::kEmptyPayload);
    assert(
        DecodeGoldSrcResourceManifest(golden.data(), golden.size() - 1u).status
        == GoldSrcResourceManifestStatus::kTruncatedPayload);

    auto wrong_opcode = golden;
    wrong_opcode[0] = 0u;
    assert(
        DecodeGoldSrcResourceManifest(wrong_opcode.data(), wrong_opcode.size()).status
        == GoldSrcResourceManifestStatus::kWrongResourceListOpcode);

    auto noncanonical_path = golden;
    noncanonical_path[3] = 'M';
    assert(
        DecodeGoldSrcResourceManifest(
            noncanonical_path.data(),
            noncanonical_path.size()).status
        == GoldSrcResourceManifestStatus::kNonCanonicalPath);

    auto consistency_present = golden;
    consistency_present.back() |= 0x01u;
    assert(
        DecodeGoldSrcResourceManifest(
            consistency_present.data(),
            consistency_present.size()).status
        == GoldSrcResourceManifestStatus::kConsistencyDataUnsupported);

    auto nonzero_padding = golden;
    nonzero_padding.back() |= 0x80u;
    assert(
        DecodeGoldSrcResourceManifest(
            nonzero_padding.data(),
            nonzero_padding.size()).status
        == GoldSrcResourceManifestStatus::kNonZeroPadding);

    std::array<std::uint8_t, golden.size() + 1u> trailing{};
    std::copy(golden.begin(), golden.end(), trailing.begin());
    assert(
        DecodeGoldSrcResourceManifest(trailing.data(), trailing.size()).status
        == GoldSrcResourceManifestStatus::kTrailingData);

    std::array<std::uint8_t, kGoldSrcMaximumResourceManifestBytes + 1u> oversized{};
    assert(
        DecodeGoldSrcResourceManifest(oversized.data(), oversized.size()).status
        == GoldSrcResourceManifestStatus::kPayloadTooLarge);
}

void TestDeterministicOrderingRoundTripAndInputImmutability()
{
    GoldSrcResourceManifest manifest = RichManifest();
    assert(manifest.entries.front().type == GoldSrcResourceType::kSkin);
    assert(manifest.entries[2].path == "MODELS\\crate.MDL");
    assert(manifest.entries[4].path == " MAPS\\TEST.BSP ");

    const GoldSrcResourceManifestValidationResult validated =
        ValidateGoldSrcResourceManifest(manifest);
    assert(validated.ok());
    const GoldSrcResourceManifestEncodeResult first =
        EncodeGoldSrcResourceManifest(manifest);
    const GoldSrcResourceManifestEncodeResult second =
        EncodeGoldSrcResourceManifest(manifest);
    assert(first.ok());
    assert(second.ok());
    assert(first.payload.size == validated.encoded_size);
    assert(first.payload.size == second.payload.size);
    assert(first.payload.bytes == second.payload.bytes);
    AssertUnusedBytesAreZero(first.payload);

    // Encoding normalizes and sorts a private prepared copy only.
    assert(manifest.entries.front().type == GoldSrcResourceType::kSkin);
    assert(manifest.entries[2].path == "MODELS\\crate.MDL");
    assert(manifest.entries[4].path == " MAPS\\TEST.BSP ");

    const GoldSrcResourceManifestDecodeResult decoded =
        DecodeGoldSrcResourceManifest(first.payload.bytes.data(), first.payload.size);
    assert(decoded.ok());
    assert(decoded.manifest.entries.size() == 7u);
    const auto& entries = decoded.manifest.entries;
    assert(entries[0].type == GoldSrcResourceType::kGeneric);
    assert(entries[1].type == GoldSrcResourceType::kSound);
    assert(entries[2].type == GoldSrcResourceType::kModel);
    assert(entries[2].index == 1u);
    assert(entries[2].path == "maps/test.bsp");
    assert(entries[3].type == GoldSrcResourceType::kModel);
    assert(entries[3].index == 2u);
    assert(entries[3].path == "models/crate.mdl");
    assert(entries[4].type == GoldSrcResourceType::kDecal);
    assert(entries[5].type == GoldSrcResourceType::kEvent);
    assert(entries[6].type == GoldSrcResourceType::kSkin);
    assert(entries[0].flags == kGoldSrcResourceCustomFlag);
    for (std::size_t index = 0; index < entries[0].md5.size(); ++index)
    {
        assert(entries[0].md5[index] == static_cast<std::uint8_t>(index + 1u));
    }
    assert(entries[5].extra_info_present);
    for (std::size_t index = 0; index < entries[5].extra_info.size(); ++index)
    {
        assert(
            entries[5].extra_info[index]
            == static_cast<std::uint8_t>(0x80u + index));
    }
}

void TestValidationFailuresAndBoundaries()
{
    GoldSrcResourceManifest missing_world{};
    missing_world.entries.push_back(Resource(
        GoldSrcResourceType::kGeneric,
        1u,
        "gfx/a.tga"));
    assert(
        ValidateGoldSrcResourceManifest(missing_world).status
        == GoldSrcResourceManifestStatus::kMissingWorldModel);

    GoldSrcResourceManifest invalid_world = MinimalManifest();
    invalid_world.entries[0].path = "models/not-world.mdl";
    assert(
        ValidateGoldSrcResourceManifest(invalid_world).status
        == GoldSrcResourceManifestStatus::kInvalidWorldModel);

    GoldSrcResourceManifest duplicate = MinimalManifest();
    duplicate.entries.push_back(Resource(
        GoldSrcResourceType::kModel,
        1u,
        "maps/other.bsp"));
    assert(
        ValidateGoldSrcResourceManifest(duplicate).status
        == GoldSrcResourceManifestStatus::kDuplicateTypeIndex);

    GoldSrcResourceManifest invalid_type = MinimalManifest();
    invalid_type.entries.push_back(Resource(
        static_cast<GoldSrcResourceType>(6u),
        2u,
        "bad/type"));
    assert(
        ValidateGoldSrcResourceManifest(invalid_type).status
        == GoldSrcResourceManifestStatus::kInvalidResourceType);

    GoldSrcResourceManifest invalid_index = MinimalManifest();
    invalid_index.entries.push_back(Resource(
        GoldSrcResourceType::kGeneric,
        static_cast<std::uint16_t>(kGoldSrcMaximumResourceIndex + 1u),
        "gfx/a.tga"));
    assert(
        ValidateGoldSrcResourceManifest(invalid_index).status
        == GoldSrcResourceManifestStatus::kResourceIndexOutOfRange);

    GoldSrcResourceManifest invalid_size = MinimalManifest();
    invalid_size.entries.push_back(Resource(
        GoldSrcResourceType::kGeneric,
        2u,
        "gfx/a.tga",
        kGoldSrcMaximumResourceDownloadBytes + 1u));
    assert(
        ValidateGoldSrcResourceManifest(invalid_size).status
        == GoldSrcResourceManifestStatus::kDownloadSizeOutOfRange);

    GoldSrcResourceManifest invalid_flags = MinimalManifest();
    invalid_flags.entries.push_back(Resource(
        GoldSrcResourceType::kGeneric,
        2u,
        "gfx/a.tga",
        0u,
        static_cast<std::uint8_t>(kGoldSrcMaximumResourceFlags + 1u)));
    assert(
        ValidateGoldSrcResourceManifest(invalid_flags).status
        == GoldSrcResourceManifestStatus::kResourceFlagsOutOfRange);

    const std::string maximum_world =
        "maps/" + std::string(54u, 'a') + ".bsp";
    assert(maximum_world.size() == kGoldSrcMaximumResourcePathBytes);
    GoldSrcResourceManifest max_path{};
    max_path.entries.push_back(Resource(
        GoldSrcResourceType::kModel,
        1u,
        maximum_world));
    const GoldSrcResourceManifestEncodeResult max_path_encoded =
        EncodeGoldSrcResourceManifest(max_path);
    assert(max_path_encoded.ok());
    assert(
        DecodeGoldSrcResourceManifest(
            max_path_encoded.payload.bytes.data(),
            max_path_encoded.payload.size).ok());
    max_path.entries[0].path += "x";
    assert(
        ValidateGoldSrcResourceManifest(max_path).status
        == GoldSrcResourceManifestStatus::kPathTooLong);
}

void TestCountCapacityFragmentationAndZeroInitialization()
{
    GoldSrcResourceManifest at_count_limit = MinimalManifest();
    for (std::size_t index = 0;
         at_count_limit.entries.size() < kGoldSrcMaximumResourceCount;
         ++index)
    {
        const std::string suffix = std::to_string(index);
        at_count_limit.entries.push_back(Resource(
            GoldSrcResourceType::kGeneric,
            static_cast<std::uint16_t>(index),
            "generic/"
                + std::string(
                    kGoldSrcMaximumResourcePathBytes
                        - std::string("generic/").size()
                        - suffix.size(),
                    'x')
                + suffix));
    }
    const GoldSrcResourceManifestValidationResult at_limit =
        ValidateGoldSrcResourceManifest(at_count_limit);
    assert(at_limit.status == GoldSrcResourceManifestStatus::kRequiresFragmentation);
    assert(at_limit.encoded_size > kGoldSrcMaximumResourceManifestBytes);

    GoldSrcResourceManifest one_over_count = at_count_limit;
    one_over_count.entries.push_back(Resource(
        GoldSrcResourceType::kSound,
        0u,
        "sound/a.wav"));
    assert(one_over_count.entries.size() == kGoldSrcMaximumResourceCount + 1u);
    assert(
        ValidateGoldSrcResourceManifest(one_over_count).status
        == GoldSrcResourceManifestStatus::kResourceCountExceeded);

    GoldSrcResourceManifest maximum_fitting = MinimalManifest();
    GoldSrcResourceManifest first_fragmenting{};
    for (std::size_t index = 0; index < 1000u; ++index)
    {
        GoldSrcResourceManifest candidate = maximum_fitting;
        candidate.entries.push_back(Resource(
            GoldSrcResourceType::kGeneric,
            static_cast<std::uint16_t>(index),
            "g/" + std::to_string(index)));
        const GoldSrcResourceManifestValidationResult validation =
            ValidateGoldSrcResourceManifest(candidate);
        if (validation.ok()
            && validation.encoded_size
                > kGoldSrcNetchanMaximumReliableBytes)
        {
            first_fragmenting = std::move(candidate);
            break;
        }
        assert(validation.ok());
        maximum_fitting = std::move(candidate);
    }
    assert(!first_fragmenting.entries.empty());

    const GoldSrcResourceManifestEncodeResult maximum_encoded =
        EncodeGoldSrcResourceManifest(maximum_fitting);
    assert(maximum_encoded.ok());
    assert(
        maximum_encoded.payload.size
        <= kGoldSrcNetchanMaximumReliableBytes);
    AssertUnusedBytesAreZero(maximum_encoded.payload);
    assert(
        EncodeGoldSrcResourceManifest(first_fragmenting).ok());
    assert(
        EncodeGoldSrcResourceManifest(
            first_fragmenting,
            kGoldSrcNetchanMaximumReliableBytes).status
        == GoldSrcResourceManifestStatus::kOutputCapacityExceeded);

    assert(maximum_encoded.payload.size > 0u);
    const GoldSrcResourceManifestEncodeResult too_small =
        EncodeGoldSrcResourceManifest(
            maximum_fitting,
            maximum_encoded.payload.size - 1u);
    assert(
        too_small.status
        == GoldSrcResourceManifestStatus::kOutputCapacityExceeded);
    assert(too_small.payload.size == 0u);
    assert(std::all_of(
        too_small.payload.bytes.begin(),
        too_small.payload.bytes.end(),
        [](std::uint8_t byte)
        {
            return byte == 0u;
        }));

    const GoldSrcResourceManifestEncodeResult exact_capacity =
        EncodeGoldSrcResourceManifest(
            maximum_fitting,
            maximum_encoded.payload.size);
    assert(exact_capacity.ok());
    assert(exact_capacity.payload.bytes == maximum_encoded.payload.bytes);
}
} // namespace

int main()
{
    TestProtocolConstantsAndStableReasons();
    TestPreparationOutcomeCacheIsImmutableUntilReset();
    TestManifestSourceDispatchInvokesExactlyOneBuilder();
    TestPathNormalizationAndBounds();
    TestResourceRequestCompanionGoldenAndDecoder();
    TestResourceListGoldenAndStrictDecode();
    TestDeterministicOrderingRoundTripAndInputImmutability();
    TestValidationFailuresAndBoundaries();
    TestCountCapacityFragmentationAndZeroInitialization();
    return 0;
}
