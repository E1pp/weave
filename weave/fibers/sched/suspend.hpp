#pragma once

#include <weave/fibers/core/awaiter.hpp>

namespace weave::fibers {

// Sets currently active fiber's handle_interface to a given one
// then Switches context back
// creating Awaiter implementation instance right
// before the Suspend call (in the same scope)
// is enough to ensure its lifetime won't
// expire until object is fully used
void Suspend(Awaiter);

}  // namespace weave::fibers