#pragma once

#include <weave/satellite/logger.hpp>

namespace weave::executors::tp::fast {

#if defined(__WEAVE_METRICS__)
inline const bool kCollectMetrics = true;
#else
inline const bool kCollectMetrics = false;
#endif

#if defined(__WEAVE_REALTIME__)
inline const bool kAtomicMetrics = true;
#else
inline const bool kAtomicMetrics = false;
#endif

inline const std::vector<std::string> kMetrics{"Launched from lifo",
                                               "Launched from local queue",
                                               "Launched from global queue",
                                               "Discarded lifo_slots",
                                               "Overflows in local queue",
                                               "Syscal parkings",
                                               "Steal attempts",
                                               "Times denied by coordinator",
                                               "Stolen from local queue"};

//////////////////////////////////////////////////////////////////////////////////////////

using Logger = satellite::LoggerImpl<kCollectMetrics, kAtomicMetrics>;

}  // namespace weave::executors::tp::fast
