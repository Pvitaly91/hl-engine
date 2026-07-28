#include "network/goldsrc_world_baseline.h"
#include "network/goldsrc_signon.h"

#include <cassert>
#include <cstdint>
#include <iostream>

namespace
{
using namespace hl::network;

GoldSrcDeltaTable MakeTable(
    const char* name,
    std::uint8_t model_bits = 10u)
{
    GoldSrcDeltaTable table;
    table.name = name;
    table.fields.push_back({
        kGoldSrcDeltaTypeInteger,
        "modelindex",
        0u,
        1u,
        model_bits,
        1.0,
        1.0,
    });
    table.fields.push_back({
        kGoldSrcDeltaTypeFloat | kGoldSrcDeltaTypeSigned,
        "origin[0]",
        4u,
        1u,
        16u,
        8.0,
        1.0,
    });
    return table;
}

GoldSrcDeltaRegistry MakeRegistry()
{
    GoldSrcDeltaRegistry registry;
    registry.tables.push_back(MakeTable("entity_state_t"));
    registry.tables.push_back(MakeTable("entity_state_player_t"));
    registry.tables.push_back(
        MakeTable("custom_entity_state_t", 16u));
    return registry;
}

GoldSrcEntityBaseline MakeBaseline(
    std::uint16_t index,
    GoldSrcBaselineKind kind,
    std::uint16_t model,
    double origin = 0.0)
{
    GoldSrcEntityBaseline baseline;
    baseline.entity_index = index;
    baseline.kind = kind;
    baseline.model_index = model;
    baseline.provenance =
        GoldSrcBaselineProvenance::kSyntheticFixture;
    baseline.state.field_count = 2u;
    baseline.state.values[0].kind =
        GoldSrcDeltaValueKind::kUnsignedInteger;
    baseline.state.values[0].unsigned_value = model;
    baseline.state.values[1].kind =
        GoldSrcDeltaValueKind::kFloatingPoint;
    baseline.state.values[1].floating_value = origin;
    return baseline;
}

GoldSrcBaselineBundle MakeBundle()
{
    GoldSrcBaselineBundle bundle;
    bundle.maximum_clients = 1u;
    bundle.entities.push_back(
        MakeBaseline(0u, GoldSrcBaselineKind::kWorld, 1u));
    bundle.entities.push_back(
        MakeBaseline(1u, GoldSrcBaselineKind::kPlayer, 82u));
    bundle.entities.push_back(
        MakeBaseline(17u, GoldSrcBaselineKind::kEntity, 7u, -12.5));
    bundle.entities.push_back(
        MakeBaseline(21u, GoldSrcBaselineKind::kCustomEntity, 9u));
    return bundle;
}

void TestModelValidation()
{
    GoldSrcBaselineBundle bundle = MakeBundle();
    assert(ValidateGoldSrcBaselineBundle(bundle)
        == GoldSrcBaselineBuildStatus::kOk);

    GoldSrcBaselineBundle invalid = bundle;
    invalid.entities.erase(invalid.entities.begin());
    assert(ValidateGoldSrcBaselineBundle(invalid)
        == GoldSrcBaselineBuildStatus::kMissingWorld);

    invalid = bundle;
    invalid.entities[1].entity_index = 0u;
    assert(ValidateGoldSrcBaselineBundle(invalid)
        == GoldSrcBaselineBuildStatus::kDuplicateEntityIndex);

    invalid = bundle;
    invalid.entities[2].entity_index =
        kGoldSrcMaximumEntityIndex + 1u;
    assert(ValidateGoldSrcBaselineBundle(invalid)
        == GoldSrcBaselineBuildStatus::kInvalidEntityIndex);

    invalid = bundle;
    invalid.entities[2].model_index = 0u;
    assert(ValidateGoldSrcBaselineBundle(invalid)
        == GoldSrcBaselineBuildStatus::kInvalidModelIndex);

    invalid = bundle;
    invalid.entities[1].entity_index = 2u;
    assert(ValidateGoldSrcBaselineBundle(invalid)
        == GoldSrcBaselineBuildStatus::kInvalidPlayerSlot);

    invalid = bundle;
    invalid.entities.resize(kGoldSrcMaximumEntityBaselines + 1u);
    assert(ValidateGoldSrcBaselineBundle(invalid)
        == GoldSrcBaselineBuildStatus::kBaselineCountExceeded);

    invalid = bundle;
    invalid.instances.resize(kGoldSrcMaximumInstancedBaselines + 1u);
    assert(ValidateGoldSrcBaselineBundle(invalid)
        == GoldSrcBaselineBuildStatus::kInstanceCountExceeded);
}

void TestEncodeDecodeAndDeterminism()
{
    const GoldSrcDeltaRegistry registry = MakeRegistry();
    const GoldSrcBaselineBundle bundle = MakeBundle();
    const GoldSrcBaselineEncodeResult first =
        EncodeGoldSrcBaselineBundle(bundle, registry);
    const GoldSrcBaselineEncodeResult second =
        EncodeGoldSrcBaselineBundle(bundle, registry);
    assert(first.ok());
    assert(second.ok());
    assert(first.payload.size == second.payload.size);
    assert(std::equal(
        first.payload.bytes.begin(),
        first.payload.bytes.begin() + first.payload.size,
        second.payload.bytes.begin()));

    const GoldSrcBaselineDecodeResult decoded =
        DecodeGoldSrcBaselineBundle(
            first.payload.bytes.data(),
            first.payload.size,
            bundle.maximum_clients,
            registry);
    assert(decoded.ok());
    assert(decoded.bundle.entities.size() == bundle.entities.size());
    assert(decoded.bundle.entities[0].kind == GoldSrcBaselineKind::kWorld);
    assert(decoded.bundle.entities[1].kind == GoldSrcBaselineKind::kPlayer);
    assert(decoded.bundle.entities[2].kind == GoldSrcBaselineKind::kEntity);
    assert(decoded.bundle.entities[3].kind
        == GoldSrcBaselineKind::kCustomEntity);
    assert(decoded.bundle.entities[0].model_index == 1u);
    assert(decoded.bundle.entities[1].model_index == 82u);
    assert(decoded.bundle.entities[2].model_index == 7u);
    assert(decoded.bundle.entities[3].model_index == 9u);
    assert(
        decoded.bundle.entities[2].state.values[1].floating_value
        == -12.5);

    assert(!EncodeGoldSrcBaselineBundle(bundle, registry, 2u).ok());
    assert(!DecodeGoldSrcBaselineBundle(
        first.payload.bytes.data(),
        first.payload.size - 1u,
        bundle.maximum_clients,
        registry).ok());
}

void TestSchemaFailures()
{
    const GoldSrcBaselineBundle bundle = MakeBundle();
    GoldSrcDeltaRegistry registry = MakeRegistry();
    registry.tables.erase(registry.tables.begin());
    assert(EncodeGoldSrcBaselineBundle(bundle, registry).status
        == GoldSrcBaselineEncodeStatus::kMissingDeltaTable);

    registry = MakeRegistry();
    registry.tables.push_back(MakeTable("entity_state_t"));
    assert(EncodeGoldSrcBaselineBundle(bundle, registry).status
        == GoldSrcBaselineEncodeStatus::kDuplicateDeltaTable);

    registry = MakeRegistry();
    GoldSrcBaselineBundle invalid = bundle;
    invalid.entities[0].state.field_count = 1u;
    assert(EncodeGoldSrcBaselineBundle(invalid, registry).status
        == GoldSrcBaselineEncodeStatus::kInvalidDeltaState);

    invalid = bundle;
    invalid.entities[2].state.values[1].floating_value = 100000.0;
    assert(EncodeGoldSrcBaselineBundle(invalid, registry).status
        == GoldSrcBaselineEncodeStatus::kValueOutOfRange);
}

void TestSignonStateAndSendEntities()
{
    GoldSrcSignonSessionState state;
    assert(state.EnterAwaitingNew(
        GoldSrcSignonBootstrapMode::kServerInfoWithDeltaDescriptions)
        == GoldSrcSignonTransitionResult::kAdvanced);
    assert(state.HandleClientCommand(GoldSrcClientSignonCommand::kNew)
        == GoldSrcSignonCommandDisposition::kDelivered);
    assert(state.MarkSignonBootstrapSent()
        == GoldSrcSignonTransitionResult::kAdvanced);
    assert(state.MarkSignonBootstrapAcknowledged()
        == GoldSrcSignonTransitionResult::kAdvanced);
    assert(state.EnterAwaitingResourceRequest()
        == GoldSrcSignonTransitionResult::kAdvanced);
    assert(state.HandleClientCommand(
        GoldSrcClientSignonCommand::kSendResources)
        == GoldSrcSignonCommandDisposition::kDelivered);
    assert(state.MarkResourceManifestSent()
        == GoldSrcSignonTransitionResult::kAdvanced);
    assert(state.MarkResourceManifestAcknowledged()
        == GoldSrcSignonTransitionResult::kAdvanced);
    assert(state.EnterAwaitingPostResourceCommand()
        == GoldSrcSignonTransitionResult::kAdvanced);
    assert(state.HandlePostResourceMove()
        == GoldSrcSignonCommandDisposition::kDelivered);
    assert(state.EnterAwaitingBaselineBootstrap()
        == GoldSrcSignonTransitionResult::kAdvanced);
    assert(state.MarkBaselineBootstrapQueued()
        == GoldSrcSignonTransitionResult::kAdvanced);
    assert(state.MarkBaselineBootstrapQueued()
        == GoldSrcSignonTransitionResult::kAlreadyApplied);
    assert(state.MarkBaselineBootstrapSent()
        == GoldSrcSignonTransitionResult::kAdvanced);
    assert(state.MarkBaselineBootstrapAcknowledged()
        == GoldSrcSignonTransitionResult::kAdvanced);
    assert(state.EnterAwaitingFirstSnapshot()
        == GoldSrcSignonTransitionResult::kAdvanced);
    assert(state.HandleSendEntities()
        == GoldSrcSignonCommandDisposition::kDelivered);
    assert(state.phase() == GoldSrcSignonPhase::kAwaitingFirstSnapshot);

    constexpr std::uint8_t send_entities[] = {
        kGoldSrcClientStringCommandOpcode,
        's', 'e', 'n', 'd', 'e', 'n', 't', 's', 0u,
    };
    const GoldSrcClientSignonDecodeResult decoded =
        DecodeGoldSrcClientSignonPayload(
            send_entities,
            sizeof(send_entities));
    assert(decoded.ok());
    assert(decoded.command == GoldSrcClientSignonCommand::kSendEntities);
    assert(decoded.primary_command_bytes == sizeof(send_entities));

    state.Reset();
    assert(state.phase() == GoldSrcSignonPhase::kNone);
    assert(state.diagnostics().baseline_bootstrap_queued == 0u);
}
} // namespace

int main()
{
    TestModelValidation();
    TestEncodeDecodeAndDeterminism();
    TestSchemaFailures();
    TestSignonStateAndSendEntities();
    std::cout
        << "goldsrc_world_baseline_tests: model=pass,delta=pass,"
           "bundle_order=pass,determinism=pass,bounds=pass,signon=pass\n";
    return 0;
}
