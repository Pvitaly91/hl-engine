#include "path_mover_runtime.h"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace hl::game_api::detail
{
const char* PathMoverStageLabel(PathMoverStage stage) noexcept
{
    switch (stage)
    {
    case PathMoverStage::kUnresolved:
        return "unresolved";
    case PathMoverStage::kStartResolved:
        return "start-resolved";
    case PathMoverStage::kGraphBound:
        return "graph-bound";
    case PathMoverStage::kActivated:
        return "activated";
    case PathMoverStage::kMoving:
        return "moving";
    case PathMoverStage::kArrivedAtNode:
        return "arrived-at-node";
    case PathMoverStage::kStopped:
        return "stopped";
    case PathMoverStage::kBlocked:
        return "blocked";
    case PathMoverStage::kCompleted:
        return "completed";
    default:
        return "unknown";
    }
}

int PathMoverStageRank(PathMoverStage stage) noexcept
{
    switch (stage)
    {
    case PathMoverStage::kMoving:
        return 8;
    case PathMoverStage::kArrivedAtNode:
        return 7;
    case PathMoverStage::kActivated:
        return 6;
    case PathMoverStage::kGraphBound:
        return 5;
    case PathMoverStage::kCompleted:
        return 4;
    case PathMoverStage::kStopped:
        return 3;
    case PathMoverStage::kStartResolved:
        return 2;
    case PathMoverStage::kBlocked:
        return 1;
    case PathMoverStage::kUnresolved:
    default:
        return 0;
    }
}

std::string FormatPathMoverVector(const Vector& value)
{
    std::ostringstream stream;
    stream << value.x << " " << value.y << " " << value.z;
    return stream.str();
}

bool PathMoverEqualsIgnoreCase(std::string_view left, std::string_view right) noexcept
{
    if (left.size() != right.size())
    {
        return false;
    }

    return std::equal(
        left.begin(),
        left.end(),
        right.begin(),
        [](char lhs, char rhs)
        {
            return std::tolower(static_cast<unsigned char>(lhs))
                == std::tolower(static_cast<unsigned char>(rhs));
        });
}
} // namespace hl::game_api::detail
