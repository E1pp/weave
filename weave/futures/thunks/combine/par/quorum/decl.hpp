#pragma once

namespace weave::futures::thunks {

template <bool OnHeap, typename Cons, template <typename...> typename Storage,
          typename... Suffix>
class QuorumControlBlock {};

}  // namespace weave::futures::thunks