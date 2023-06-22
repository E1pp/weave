#pragma once

namespace weave::futures::thunks {

template <bool OnHeap, template <typename...> typename StorageImpl,
          typename... Futures>
struct QuorumControlBlock {};

}  // namespace weave::futures::thunks