#pragma once

namespace weave::futures::thunks {

template <bool OnHeap, template <typename...> typename StorageImpl,
          typename... Futures>
struct AllControlBlock {};

}  // namespace weave::futures::thunks