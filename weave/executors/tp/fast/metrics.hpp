#pragma once

#include <weave/executors/tp/fast/logger.hpp>

namespace weave::executors::tp::fast {

const bool kCollectMetrics = true;

inline const std::vector<std::string> kMetrics {
   "Launched from lifo"
  ,"Launched from local queue"
  ,"Launched from global queue"
  ,"Discarded lifo_slots"
  ,"Overflows in local queue"
  ,"Syscal parkings"
  ,"Steal attempts"
  ,"Times denied by coordinator"
  ,"Stolen from local queue"
};

//////////////////////////////////////////////////////////////////////////////////////////

using Logger = LoggerImpl<kCollectMetrics>;

}  // namespace weave::executors::tp::fast
