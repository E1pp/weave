#pragma once

#include <cstdlib>

namespace weave::executors::tp::fast {

// launch modes

const bool kDisableStealing = false;

const bool kDisbaleLifoInteraction = false;

const bool kPreventLifoStealing = false;

const bool kCollectMetrics = true;

///////////////////////////////////////////////////////////////////

const size_t kVyukovGQueue = 61;

}  // namespace weave::executors::tp::fast