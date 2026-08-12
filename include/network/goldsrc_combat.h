#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace hl::network
{
inline constexpr int kGoldSrcStockGlockWeaponId = 2;

enum class GoldSrcCombatPhase
{
    kDisabled,
    kAwaitingPlayer,
    kAwaitingWeaponState,
    kCombatReady,
    kAttackExecuting,
    kDamageObserved,
    kCombatStable,
};

const char* NameFor(GoldSrcCombatPhase phase) noexcept;

struct GoldSrcCombatInput final
{
    std::uint16_t buttons = 0u;
    bool attack_received = false;
    bool attack_enabled = false;
    bool attack2_masked = false;
    std::size_t unsupported_bits_masked = 0u;
};

GoldSrcCombatInput FilterGoldSrcCombatInput(
    std::uint16_t buttons,
    bool combat_enabled,
    GoldSrcCombatPhase phase) noexcept;

bool GoldSrcCombatWeaponPresenceMatches(
    double expected_health,
    bool expected_weapon_present,
    bool observed_weapon_present) noexcept;

struct GoldSrcCombatTraceCandidate final
{
    int slot = 0;
    std::uint64_t generation = 0u;
    std::uint64_t authoritative_generation = 0u;
    bool connected = false;
    bool spawned = false;
    bool damageable = false;
    std::array<float, 3> minimum{};
    std::array<float, 3> maximum{};
};

enum class GoldSrcCombatTraceHitType
{
    kNone,
    kWorld,
    kPlayer,
};

struct GoldSrcCombatTraceSelection final
{
    GoldSrcCombatTraceHitType hit_type = GoldSrcCombatTraceHitType::kNone;
    float world_fraction = 1.0f;
    float player_fraction = 1.0f;
    float selected_fraction = 1.0f;
    int player_slot = 0;
    std::uint64_t player_generation = 0u;
    bool world_occluded = false;
    bool shooter_ignored = false;
};

GoldSrcCombatTraceSelection SelectGoldSrcCombatLineHit(
    const std::array<float, 3>& start,
    const std::array<float, 3>& end,
    float world_fraction,
    const std::vector<GoldSrcCombatTraceCandidate>& candidates,
    int ignored_slot,
    std::uint64_t ignored_generation) noexcept;

class GoldSrcCombatFrameCadence final
{
public:
    bool BeginFrame(bool enabled, std::uint64_t host_frame) noexcept;
    void Reset() noexcept;
    std::uint64_t call_count() const noexcept;

private:
    bool initialized_ = false;
    std::uint64_t last_host_frame_ = 0u;
    std::uint64_t call_count_ = 0u;
};
} // namespace hl::network
