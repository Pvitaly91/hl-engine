#pragma once

#include "network/goldsrc_delta_description.h"

namespace hl::game_api::detail
{
// Builds the protocol-48 layout metadata from the public HLSDK structures.
// delta.lst supplies wire policy; this registry supplies only canonical byte
// offsets and the scalar field-size value used by GoldSrc descriptions.
hl::network::GoldSrcDeltaLayoutRegistry
BuildGoldSrcProtocol48DeltaLayouts();
} // namespace hl::game_api::detail
