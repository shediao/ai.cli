#pragma once

#include "ai/args.h"

namespace ai {

/// Check GitHub for the latest release and self-update if a newer version
/// exists. Returns 0 on success (updated or already up to date), non-zero
/// on error.
int update(AiArgs const& args);

}  // namespace ai
