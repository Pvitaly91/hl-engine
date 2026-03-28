#pragma once

#include <cstddef>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "game_api/hl_server_module.h"

#pragma warning(push, 0)
#include "extdll.h"
#pragma warning(pop)

namespace hl::game_api::detail
{
struct TrackPathNodeInput
{
    std::size_t parse_index = 0;
    int edict_index = -1;
    std::string targetname;
    std::string next_target;
    bool next_target_terminal_dead_end = false;
    std::string message_target;
    Vector origin = Vector(0.0f, 0.0f, 0.0f);
    bool has_origin = false;
    float speed = 0.0f;
    bool in_use = false;
    bool removed = false;
};

struct TrackPathNodeView
{
    std::size_t parse_index = 0;
    int edict_index = -1;
    std::string targetname;
    std::string next_target;
    bool next_target_terminal_dead_end = false;
    std::string message_target;
    Vector origin = Vector(0.0f, 0.0f, 0.0f);
    bool has_origin = false;
    float speed = 0.0f;
    bool next_resolved = false;
    bool duplicate_targetname = false;
    bool orphan = false;
    bool cycle_member = false;
    std::size_t incoming_links = 0;
};

enum class TrackPathLookupState
{
    kNoNodeFound,
    kFoundButGraphBroken,
    kFoundAndGraphValid,
};

struct TrackPathLookupResult
{
    TrackPathLookupState state = TrackPathLookupState::kNoNodeFound;
    const TrackPathNodeView* node = nullptr;
    std::size_t candidate_count = 0;
    bool used_fallback = false;
    std::string resolution_mode;
    std::string detail;
    std::vector<std::string> similar_names;
};

struct TrackPathResolverConfig
{
    std::size_t preview_limit = 8;
};

class TrackPathResolver
{
public:
    void Configure(const TrackPathResolverConfig& config);
    void Rebuild(
        const std::vector<TrackPathNodeInput>& nodes,
        const std::function<void(std::string_view)>& log_info,
        const std::function<void(std::string_view)>& log_warn);

    const TrackPathGraphSummary& Summary() const noexcept;
    const std::vector<TrackPathNodeView>& Nodes() const noexcept;
    const TrackPathNodeView* FindNodeByName(std::string_view targetname) const noexcept;
    std::vector<const TrackPathNodeView*> FindNodesByName(std::string_view targetname) const;
    const TrackPathNodeView* FindNodeByEdictIndex(int edict_index) const noexcept;
    const TrackPathNodeView* ResolveNext(const TrackPathNodeView& node) const noexcept;
    bool HasTerminalDeadEndLink(const TrackPathNodeView& node) const noexcept;
    bool IsTerminalNode(const TrackPathNodeView& node) const noexcept;
    const TrackPathNodeView* FindNearest(const Vector& origin, float max_distance) const noexcept;
    TrackPathLookupResult ResolveStartNode(
        std::string_view targetname,
        const Vector* origin,
        float max_distance,
        std::size_t similar_limit) const;
    std::vector<std::string> FindSimilarNames(std::string_view targetname, std::size_t limit) const;
    bool IsGraphValidStartNode(const TrackPathNodeView& node) const noexcept;
    bool IsDuplicateTargetname(std::string_view targetname) const noexcept;

private:
    const TrackPathNodeView* FindNearestChainStart(
        const Vector& origin,
        float max_distance) const noexcept;
    const TrackPathNodeView* FindNearestValidStart(
        const Vector& origin,
        float max_distance) const noexcept;

    TrackPathResolverConfig config_{};
    TrackPathGraphSummary summary_{};
    std::vector<TrackPathNodeView> nodes_;
    std::unordered_map<std::string, std::vector<std::size_t>> indices_by_name_;
    std::unordered_map<int, std::size_t> index_by_edict_;
    std::unordered_set<std::size_t> chain_start_indices_;
};
} // namespace hl::game_api::detail
