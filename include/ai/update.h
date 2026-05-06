#pragma once

namespace ai {

/// Check GitHub for the latest release and self-update if a newer version
/// exists. Returns 0 on success (updated or already up to date), non-zero
/// on error.
int update();

}  // namespace ai
