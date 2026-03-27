#pragma once

#include <string>
#include <string_view>

#include "game_api/hl_server_module.h"

#pragma warning(push, 0)
#include "extdll.h"
#pragma warning(pop)

namespace hl::game_api::detail
{
enum class PathMoverStage
{
    kUnresolved,
    kStartResolved,
    kGraphBound,
    kActivated,
    kMoving,
    kArrivedAtNode,
    kStopped,
    kBlocked,
    kCompleted,
};

struct PathMoverMutableState
{
    PathMoverRuntimeSummary summary;
    PathMoverStage stage = PathMoverStage::kUnresolved;
    bool snapped_to_start = false;
    Vector previous_origin = Vector(0.0f, 0.0f, 0.0f);
    bool previous_origin_valid = false;
};

const char* PathMoverStageLabel(PathMoverStage stage) noexcept;
int PathMoverStageRank(PathMoverStage stage) noexcept;
std::string FormatPathMoverVector(const Vector& value);
bool PathMoverEqualsIgnoreCase(std::string_view left, std::string_view right) noexcept;
} // namespace hl::game_api::detail
