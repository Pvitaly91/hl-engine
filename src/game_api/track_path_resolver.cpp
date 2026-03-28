#include "track_path_resolver.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>
#include <sstream>
#include <utility>

namespace
{
using hl::game_api::detail::TrackPathLookupResult;
using hl::game_api::detail::TrackPathLookupState;
using hl::game_api::detail::TrackPathNodeInput;
using hl::game_api::detail::TrackPathNodeView;

std::string ToLowerCopy(std::string value)
{
    std::transform(
        value.begin(),
        value.end(),
        value.begin(),
        [](unsigned char character)
        {
            return static_cast<char>(std::tolower(character));
        });
    return value;
}

std::string FormatVector(const Vector& value)
{
    std::ostringstream stream;
    stream << value.x << " " << value.y << " " << value.z;
    return stream.str();
}

std::string BuildNodeLabel(const TrackPathNodeView& node)
{
    std::string label =
        node.targetname.empty() ? std::string("<empty>") : node.targetname;
    label += " edict#" + std::to_string(node.edict_index);
    if (node.has_origin)
    {
        label += " origin=" + FormatVector(node.origin);
    }
    return label;
}

std::size_t EditDistance(std::string_view left, std::string_view right)
{
    if (left.empty())
    {
        return right.size();
    }
    if (right.empty())
    {
        return left.size();
    }

    std::vector<std::size_t> previous(right.size() + 1u, 0u);
    std::vector<std::size_t> current(right.size() + 1u, 0u);
    for (std::size_t index = 0; index <= right.size(); ++index)
    {
        previous[index] = index;
    }

    for (std::size_t left_index = 0; left_index < left.size(); ++left_index)
    {
        current[0] = left_index + 1u;
        for (std::size_t right_index = 0; right_index < right.size(); ++right_index)
        {
            const std::size_t substitution_cost =
                std::tolower(static_cast<unsigned char>(left[left_index]))
                    == std::tolower(static_cast<unsigned char>(right[right_index]))
                ? 0u
                : 1u;
            current[right_index + 1u] = std::min(
                {
                    previous[right_index + 1u] + 1u,
                    current[right_index] + 1u,
                    previous[right_index] + substitution_cost,
                });
        }
        previous.swap(current);
    }

    return previous.back();
}

float DistanceBetween(const Vector& left, const Vector& right)
{
    const Vector delta = left - right;
    return std::sqrt(
        (delta.x * delta.x) + (delta.y * delta.y) + (delta.z * delta.z));
}
} // namespace

namespace hl::game_api::detail
{
void TrackPathResolver::Configure(const TrackPathResolverConfig& config)
{
    config_ = config;
    if (config_.preview_limit == 0)
    {
        config_.preview_limit = 8;
    }

    summary_ = {};
    nodes_.clear();
    indices_by_name_.clear();
    index_by_edict_.clear();
    chain_start_indices_.clear();
}

void TrackPathResolver::Rebuild(
    const std::vector<TrackPathNodeInput>& inputs,
    const std::function<void(std::string_view)>& log_info,
    const std::function<void(std::string_view)>& log_warn)
{
    summary_ = {};
    nodes_.clear();
    indices_by_name_.clear();
    index_by_edict_.clear();
    chain_start_indices_.clear();

    for (const TrackPathNodeInput& input : inputs)
    {
        if (!input.in_use || input.removed)
        {
            ++summary_.inactive_nodes;
            continue;
        }

        TrackPathNodeView node;
        node.parse_index = input.parse_index;
        node.edict_index = input.edict_index;
        node.targetname = input.targetname;
        node.next_target = input.next_target;
        node.next_target_terminal_dead_end = input.next_target_terminal_dead_end;
        node.message_target = input.message_target;
        node.origin = input.origin;
        node.has_origin = input.has_origin;
        node.speed = input.speed;
        if (!node.targetname.empty())
        {
            indices_by_name_[ToLowerCopy(node.targetname)].push_back(nodes_.size());
        }
        if (node.edict_index >= 0)
        {
            index_by_edict_[node.edict_index] = nodes_.size();
        }
        nodes_.push_back(std::move(node));
    }

    summary_.nodes = nodes_.size();

    for (auto& [name, indices] : indices_by_name_)
    {
        if (indices.size() <= 1)
        {
            continue;
        }

        ++summary_.duplicate_targetnames;
        for (const std::size_t index : indices)
        {
            if (index < nodes_.size())
            {
                nodes_[index].duplicate_targetname = true;
            }
        }

        if (summary_.duplicate_targetname_preview.size() < config_.preview_limit)
        {
            std::string detail = "duplicate targetname=" + name + " edicts=";
            bool first = true;
            for (const std::size_t index : indices)
            {
                if (index >= nodes_.size())
                {
                    continue;
                }
                if (!first)
                {
                    detail += ",";
                }
                detail += std::to_string(nodes_[index].edict_index);
                first = false;
            }
            summary_.duplicate_targetname_preview.push_back(std::move(detail));
        }
    }

    for (TrackPathNodeView& node : nodes_)
    {
        if (!node.next_target.empty())
        {
            if (const TrackPathNodeView* next = FindNodeByName(node.next_target);
                next != nullptr)
            {
                node.next_resolved = true;
                ++summary_.valid_links;
                if (const auto it = index_by_edict_.find(next->edict_index);
                    it != index_by_edict_.end() && it->second < nodes_.size())
                {
                    ++nodes_[it->second].incoming_links;
                }
            }
            else
            {
                if (!node.next_target_terminal_dead_end)
                {
                    ++summary_.broken_links;
                    if (summary_.broken_link_preview.size() < config_.preview_limit)
                    {
                        summary_.broken_link_preview.push_back(
                            "broken " + BuildNodeLabel(node)
                            + " -> " + node.next_target);
                    }
                }
            }
        }

        if (!node.message_target.empty())
        {
            ++summary_.nodes_with_message;
            if (summary_.message_node_preview.size() < config_.preview_limit)
            {
                summary_.message_node_preview.push_back(
                    "message " + BuildNodeLabel(node)
                    + " message=" + node.message_target
                    + (node.speed > 0.0f
                        ? " speed=" + std::to_string(node.speed)
                        : std::string()));
            }
        }
    }

    for (TrackPathNodeView& node : nodes_)
    {
        node.orphan =
            node.incoming_links == 0u
            && !node.next_resolved
            && !node.next_target_terminal_dead_end;
        if (node.orphan)
        {
            ++summary_.orphan_nodes;
            if (summary_.orphan_preview.size() < config_.preview_limit)
            {
                summary_.orphan_preview.push_back(
                    "orphan " + BuildNodeLabel(node)
                    + " next=" + (node.next_target.empty()
                        ? std::string("<none>")
                        : node.next_target));
            }
        }

        if (node.incoming_links == 0u && IsGraphValidStartNode(node))
        {
            if (const auto it = index_by_edict_.find(node.edict_index);
                it != index_by_edict_.end())
            {
                chain_start_indices_.insert(it->second);
            }
        }

        if (summary_.preview.size() < config_.preview_limit)
        {
            summary_.preview.push_back(
                "path_track "
                + (node.targetname.empty() ? std::string("<empty>") : node.targetname)
                + " -> "
                + (node.next_target.empty() ? std::string("<none>") : node.next_target)
                + (node.next_target_terminal_dead_end ? " terminal-dead-end" : std::string())
                + (node.message_target.empty()
                    ? std::string()
                    : " message=" + node.message_target)
                + (node.speed > 0.0f ? " speed=" + std::to_string(node.speed) : std::string()));
        }
    }

    std::unordered_set<std::size_t> visited;
    std::unordered_set<std::size_t> active_stack;
    std::vector<std::size_t> stack;
    std::function<void(std::size_t)> visit =
        [&](std::size_t index)
        {
            if (index >= nodes_.size())
            {
                return;
            }
            if (visited.find(index) != visited.end())
            {
                return;
            }

            visited.insert(index);
            active_stack.insert(index);
            stack.push_back(index);

            const TrackPathNodeView& node = nodes_[index];
            const TrackPathNodeView* next = ResolveNext(node);
            if (next != nullptr)
            {
                const auto it = index_by_edict_.find(next->edict_index);
                if (it != index_by_edict_.end())
                {
                    const std::size_t next_index = it->second;
                    if (active_stack.find(next_index) != active_stack.end())
                    {
                        ++summary_.cycles;
                        bool record_cycle = summary_.cycle_preview.size() < config_.preview_limit;
                        std::string detail = "cycle ";
                        bool append = false;
                        for (auto stack_it = stack.rbegin(); stack_it != stack.rend(); ++stack_it)
                        {
                            if (record_cycle)
                            {
                                if (append)
                                {
                                    detail += " -> ";
                                }
                                detail += nodes_[*stack_it].targetname.empty()
                                    ? std::string("<empty>")
                                    : nodes_[*stack_it].targetname;
                                append = true;
                            }
                            nodes_[*stack_it].cycle_member = true;
                            if (*stack_it == next_index)
                            {
                                break;
                            }
                        }
                        nodes_[next_index].cycle_member = true;
                        if (record_cycle)
                        {
                            detail += " -> "
                                + (nodes_[next_index].targetname.empty()
                                    ? std::string("<empty>")
                                    : nodes_[next_index].targetname);
                            summary_.cycle_preview.push_back(std::move(detail));
                        }
                    }
                    else
                    {
                        visit(next_index);
                    }
                }
            }

            stack.pop_back();
            active_stack.erase(index);
        };

    for (std::size_t index = 0; index < nodes_.size(); ++index)
    {
        visit(index);
    }

    if (log_info)
    {
        log_info(
            "TrackPathResolver: nodes=" + std::to_string(summary_.nodes)
            + " valid=" + std::to_string(summary_.valid_links)
            + " broken=" + std::to_string(summary_.broken_links)
            + " message=" + std::to_string(summary_.nodes_with_message)
            + " duplicates=" + std::to_string(summary_.duplicate_targetnames)
            + " orphans=" + std::to_string(summary_.orphan_nodes)
            + " cycles=" + std::to_string(summary_.cycles));
    }

    if (log_warn)
    {
        for (const std::string& line : summary_.broken_link_preview)
        {
            log_warn("TrackPathResolver: " + line);
        }
        for (const std::string& line : summary_.duplicate_targetname_preview)
        {
            log_warn("TrackPathResolver: " + line);
        }
        for (const std::string& line : summary_.cycle_preview)
        {
            log_warn("TrackPathResolver: " + line);
        }
    }
}

const TrackPathGraphSummary& TrackPathResolver::Summary() const noexcept
{
    return summary_;
}

const std::vector<TrackPathNodeView>& TrackPathResolver::Nodes() const noexcept
{
    return nodes_;
}

const TrackPathNodeView* TrackPathResolver::FindNodeByName(std::string_view targetname) const noexcept
{
    if (targetname.empty())
    {
        return nullptr;
    }

    const auto it = indices_by_name_.find(ToLowerCopy(std::string(targetname)));
    if (it == indices_by_name_.end() || it->second.empty())
    {
        return nullptr;
    }

    const std::size_t index = it->second.front();
    return index < nodes_.size() ? &nodes_[index] : nullptr;
}

std::vector<const TrackPathNodeView*> TrackPathResolver::FindNodesByName(
    std::string_view targetname) const
{
    std::vector<const TrackPathNodeView*> matches;
    if (targetname.empty())
    {
        return matches;
    }

    const auto it = indices_by_name_.find(ToLowerCopy(std::string(targetname)));
    if (it == indices_by_name_.end())
    {
        return matches;
    }

    matches.reserve(it->second.size());
    for (const std::size_t index : it->second)
    {
        if (index < nodes_.size())
        {
            matches.push_back(&nodes_[index]);
        }
    }
    return matches;
}

const TrackPathNodeView* TrackPathResolver::FindNodeByEdictIndex(int edict_index) const noexcept
{
    const auto it = index_by_edict_.find(edict_index);
    if (it == index_by_edict_.end())
    {
        return nullptr;
    }

    return it->second < nodes_.size() ? &nodes_[it->second] : nullptr;
}

const TrackPathNodeView* TrackPathResolver::ResolveNext(const TrackPathNodeView& node) const noexcept
{
    return FindNodeByName(node.next_target);
}

bool TrackPathResolver::HasTerminalDeadEndLink(const TrackPathNodeView& node) const noexcept
{
    return !node.next_target.empty() && node.next_target_terminal_dead_end;
}

bool TrackPathResolver::IsTerminalNode(const TrackPathNodeView& node) const noexcept
{
    return node.next_target.empty() || HasTerminalDeadEndLink(node);
}

const TrackPathNodeView* TrackPathResolver::FindNearest(
    const Vector& origin,
    float max_distance) const noexcept
{
    if (max_distance <= 0.0f)
    {
        return nullptr;
    }

    const float max_distance_squared = max_distance * max_distance;
    const TrackPathNodeView* nearest = nullptr;
    float nearest_distance_squared = max_distance_squared;
    for (const TrackPathNodeView& node : nodes_)
    {
        if (!node.has_origin)
        {
            continue;
        }

        const Vector delta = node.origin - origin;
        const float distance_squared =
            (delta.x * delta.x) + (delta.y * delta.y) + (delta.z * delta.z);
        if (distance_squared > nearest_distance_squared)
        {
            continue;
        }

        nearest = &node;
        nearest_distance_squared = distance_squared;
    }

    return nearest;
}

TrackPathLookupResult TrackPathResolver::ResolveStartNode(
    std::string_view targetname,
    const Vector* origin,
    float max_distance,
    std::size_t similar_limit) const
{
    TrackPathLookupResult result;
    result.similar_names = FindSimilarNames(targetname, similar_limit);

    const std::vector<const TrackPathNodeView*> exact_matches = FindNodesByName(targetname);
    result.candidate_count = exact_matches.size();
    if (exact_matches.size() > 1u)
    {
        result.state = TrackPathLookupState::kFoundButGraphBroken;
        result.node = exact_matches.front();
        result.resolution_mode = "ambiguous-targetname";
        result.detail =
            "requested='" + std::string(targetname)
            + "' candidates=" + std::to_string(result.candidate_count);
        return result;
    }

    if (exact_matches.size() == 1u)
    {
        result.node = exact_matches.front();
        result.resolution_mode = "exact-targetname";
        result.state = IsGraphValidStartNode(*result.node)
            ? TrackPathLookupState::kFoundAndGraphValid
            : TrackPathLookupState::kFoundButGraphBroken;
        result.detail =
            "requested='" + std::string(targetname)
            + "' candidates=1 resolved="
            + (result.node->targetname.empty() ? std::string("<empty>") : result.node->targetname);
        return result;
    }

    if (origin != nullptr)
    {
        if (const TrackPathNodeView* chain_start = FindNearestChainStart(*origin, max_distance);
            chain_start != nullptr)
        {
            result.node = chain_start;
            result.used_fallback = true;
            result.resolution_mode = "fallback-nearest-chain-start";
            result.state = TrackPathLookupState::kFoundAndGraphValid;
            result.detail =
                "requested='" + std::string(targetname)
                + "' candidates=0 fallback=" + chain_start->targetname
                + " distance=" + std::to_string(DistanceBetween(chain_start->origin, *origin));
            return result;
        }

        if (const TrackPathNodeView* nearest = FindNearestValidStart(*origin, max_distance);
            nearest != nullptr)
        {
            result.node = nearest;
            result.used_fallback = true;
            result.resolution_mode = "fallback-nearest-valid-node";
            result.state = TrackPathLookupState::kFoundAndGraphValid;
            result.detail =
                "requested='" + std::string(targetname)
                + "' candidates=0 fallback=" + nearest->targetname
                + " distance=" + std::to_string(DistanceBetween(nearest->origin, *origin));
            return result;
        }
    }

    result.state = TrackPathLookupState::kNoNodeFound;
    result.resolution_mode = "unresolved";
    result.detail =
        "requested='" + std::string(targetname)
        + "' candidates=0";
    return result;
}

std::vector<std::string> TrackPathResolver::FindSimilarNames(
    std::string_view targetname,
    std::size_t limit) const
{
    std::vector<std::pair<std::size_t, std::string>> ranked;
    if (targetname.empty() || limit == 0u)
    {
        return {};
    }

    const std::string lowered_query = ToLowerCopy(std::string(targetname));
    ranked.reserve(nodes_.size());
    for (const TrackPathNodeView& node : nodes_)
    {
        if (node.targetname.empty())
        {
            continue;
        }

        ranked.emplace_back(
            EditDistance(lowered_query, ToLowerCopy(node.targetname)),
            node.targetname);
    }

    std::sort(
        ranked.begin(),
        ranked.end(),
        [](const auto& left, const auto& right)
        {
            if (left.first != right.first)
            {
                return left.first < right.first;
            }
            return left.second < right.second;
        });

    std::vector<std::string> similar;
    for (const auto& [distance, name] : ranked)
    {
        (void)distance;
        if (std::find(similar.begin(), similar.end(), name) != similar.end())
        {
            continue;
        }
        similar.push_back(name);
        if (similar.size() >= limit)
        {
            break;
        }
    }
    return similar;
}

bool TrackPathResolver::IsGraphValidStartNode(const TrackPathNodeView& node) const noexcept
{
    if (node.duplicate_targetname || !node.has_origin)
    {
        return false;
    }

    return ResolveNext(node) != nullptr;
}

bool TrackPathResolver::IsDuplicateTargetname(std::string_view targetname) const noexcept
{
    if (targetname.empty())
    {
        return false;
    }

    const auto it = indices_by_name_.find(ToLowerCopy(std::string(targetname)));
    return it != indices_by_name_.end() && it->second.size() > 1u;
}

const TrackPathNodeView* TrackPathResolver::FindNearestChainStart(
    const Vector& origin,
    float max_distance) const noexcept
{
    if (max_distance <= 0.0f)
    {
        return nullptr;
    }

    const TrackPathNodeView* nearest = nullptr;
    float nearest_distance = max_distance;
    for (const std::size_t index : chain_start_indices_)
    {
        if (index >= nodes_.size())
        {
            continue;
        }

        const TrackPathNodeView& node = nodes_[index];
        if (!node.has_origin)
        {
            continue;
        }

        const float distance = DistanceBetween(node.origin, origin);
        if (distance > nearest_distance)
        {
            continue;
        }

        nearest = &node;
        nearest_distance = distance;
    }

    return nearest;
}

const TrackPathNodeView* TrackPathResolver::FindNearestValidStart(
    const Vector& origin,
    float max_distance) const noexcept
{
    if (max_distance <= 0.0f)
    {
        return nullptr;
    }

    const TrackPathNodeView* nearest = nullptr;
    float nearest_distance = max_distance;
    for (const TrackPathNodeView& node : nodes_)
    {
        if (!IsGraphValidStartNode(node))
        {
            continue;
        }

        const float distance = DistanceBetween(node.origin, origin);
        if (distance > nearest_distance)
        {
            continue;
        }

        nearest = &node;
        nearest_distance = distance;
    }

    return nearest;
}
} // namespace hl::game_api::detail
