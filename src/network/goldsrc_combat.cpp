#include "network/goldsrc_combat.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace hl::network
{
namespace
{
constexpr std::uint16_t kButtonAttack = 1u << 0u;
constexpr std::uint16_t kButtonJump = 1u << 1u;
constexpr std::uint16_t kButtonDuck = 1u << 2u;
constexpr std::uint16_t kButtonForward = 1u << 3u;
constexpr std::uint16_t kButtonBack = 1u << 4u;
constexpr std::uint16_t kButtonLeft = 1u << 7u;
constexpr std::uint16_t kButtonRight = 1u << 8u;
constexpr std::uint16_t kButtonMoveLeft = 1u << 9u;
constexpr std::uint16_t kButtonMoveRight = 1u << 10u;
constexpr std::uint16_t kButtonAttack2 = 1u << 11u;
constexpr std::uint16_t kButtonRun = 1u << 12u;
constexpr std::uint16_t kMovementButtons =
    kButtonJump | kButtonDuck | kButtonForward | kButtonBack | kButtonLeft
    | kButtonRight | kButtonMoveLeft | kButtonMoveRight | kButtonRun;
constexpr std::uint16_t kCombatButtons = kMovementButtons | kButtonAttack;

bool FiniteVector(const std::array<float, 3>& value) noexcept
{
    return std::isfinite(value[0]) && std::isfinite(value[1])
        && std::isfinite(value[2]);
}

bool RayAabbFraction(
    const std::array<float, 3>& start,
    const std::array<float, 3>& end,
    const std::array<float, 3>& minimum,
    const std::array<float, 3>& maximum,
    float* fraction) noexcept
{
    if (fraction == nullptr || !FiniteVector(start) || !FiniteVector(end)
        || !FiniteVector(minimum) || !FiniteVector(maximum))
    {
        return false;
    }
    float entering = 0.0f;
    float leaving = 1.0f;
    for (std::size_t axis = 0; axis < 3u; ++axis)
    {
        if (minimum[axis] > maximum[axis])
        {
            return false;
        }
        const float direction = end[axis] - start[axis];
        if (std::fabs(direction) <= std::numeric_limits<float>::epsilon())
        {
            if (start[axis] < minimum[axis] || start[axis] > maximum[axis])
            {
                return false;
            }
            continue;
        }
        float first = (minimum[axis] - start[axis]) / direction;
        float second = (maximum[axis] - start[axis]) / direction;
        if (first > second)
        {
            std::swap(first, second);
        }
        entering = (std::max)(entering, first);
        leaving = (std::min)(leaving, second);
        if (entering > leaving)
        {
            return false;
        }
    }
    if (leaving < 0.0f || entering > 1.0f)
    {
        return false;
    }
    *fraction = std::clamp(entering, 0.0f, 1.0f);
    return true;
}
} // namespace

const char* NameFor(GoldSrcCombatPhase phase) noexcept
{
    switch (phase)
    {
    case GoldSrcCombatPhase::kDisabled:
        return "disabled";
    case GoldSrcCombatPhase::kAwaitingPlayer:
        return "awaiting_player";
    case GoldSrcCombatPhase::kAwaitingWeaponState:
        return "awaiting_weapon_state";
    case GoldSrcCombatPhase::kCombatReady:
        return "combat_ready";
    case GoldSrcCombatPhase::kAttackExecuting:
        return "attack_executing";
    case GoldSrcCombatPhase::kDamageObserved:
        return "damage_observed";
    case GoldSrcCombatPhase::kCombatStable:
        return "combat_stable";
    }
    return "disabled";
}

GoldSrcCombatInput FilterGoldSrcCombatInput(
    std::uint16_t buttons,
    bool combat_enabled,
    GoldSrcCombatPhase phase) noexcept
{
    GoldSrcCombatInput result;
    result.attack_received = (buttons & kButtonAttack) != 0u;
    result.attack2_masked = (buttons & kButtonAttack2) != 0u;
    const bool ready = phase == GoldSrcCombatPhase::kCombatReady
        || phase == GoldSrcCombatPhase::kAttackExecuting
        || phase == GoldSrcCombatPhase::kDamageObserved
        || phase == GoldSrcCombatPhase::kCombatStable;
    const std::uint16_t allowed = combat_enabled && ready
        ? kCombatButtons
        : kMovementButtons;
    result.buttons = buttons & allowed;
    result.attack_enabled = (result.buttons & kButtonAttack) != 0u;
    std::uint16_t masked = buttons & static_cast<std::uint16_t>(~allowed);
    while (masked != 0u)
    {
        result.unsupported_bits_masked += masked & 1u;
        masked = static_cast<std::uint16_t>(masked >> 1u);
    }
    return result;
}

bool GoldSrcCombatWeaponPresenceMatches(
    double expected_health,
    bool expected_weapon_present,
    bool observed_weapon_present) noexcept
{
    if (!std::isfinite(expected_health))
    {
        return false;
    }
    if (expected_health <= 0.0)
    {
        return expected_weapon_present == observed_weapon_present;
    }
    return expected_weapon_present && observed_weapon_present;
}

GoldSrcCombatTraceSelection SelectGoldSrcCombatLineHit(
    const std::array<float, 3>& start,
    const std::array<float, 3>& end,
    float world_fraction,
    const std::vector<GoldSrcCombatTraceCandidate>& candidates,
    int ignored_slot,
    std::uint64_t ignored_generation) noexcept
{
    GoldSrcCombatTraceSelection result;
    result.world_fraction = std::isfinite(world_fraction)
        ? std::clamp(world_fraction, 0.0f, 1.0f)
        : 1.0f;
    result.selected_fraction = result.world_fraction;
    if (result.world_fraction < 1.0f)
    {
        result.hit_type = GoldSrcCombatTraceHitType::kWorld;
    }
    for (const GoldSrcCombatTraceCandidate& candidate : candidates)
    {
        if (candidate.slot == ignored_slot
            && (ignored_generation == 0u
                || candidate.generation == ignored_generation))
        {
            result.shooter_ignored = true;
            continue;
        }
        if (candidate.slot < 1 || candidate.generation == 0u
            || candidate.authoritative_generation == 0u
            || candidate.generation != candidate.authoritative_generation
            || !candidate.connected || !candidate.spawned
            || !candidate.damageable)
        {
            continue;
        }
        float fraction = 1.0f;
        if (!RayAabbFraction(
                start,
                end,
                candidate.minimum,
                candidate.maximum,
                &fraction))
        {
            continue;
        }
        if (fraction < result.player_fraction)
        {
            result.player_fraction = fraction;
        }
        if (fraction < result.selected_fraction)
        {
            result.selected_fraction = fraction;
            result.player_slot = candidate.slot;
            result.player_generation = candidate.generation;
            result.hit_type = GoldSrcCombatTraceHitType::kPlayer;
        }
        else if (fraction >= result.world_fraction)
        {
            result.world_occluded = true;
        }
    }
    return result;
}

bool GoldSrcCombatFrameCadence::BeginFrame(
    bool enabled,
    std::uint64_t host_frame) noexcept
{
    if (!enabled || (initialized_ && host_frame == last_host_frame_))
    {
        return false;
    }
    initialized_ = true;
    last_host_frame_ = host_frame;
    ++call_count_;
    return true;
}

void GoldSrcCombatFrameCadence::Reset() noexcept
{
    initialized_ = false;
    last_host_frame_ = 0u;
    call_count_ = 0u;
}

std::uint64_t GoldSrcCombatFrameCadence::call_count() const noexcept
{
    return call_count_;
}
} // namespace hl::network
