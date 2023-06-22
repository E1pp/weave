#pragma once

namespace weave::futures::thunks {

template <bool OnHeap, template <typename...> typename StorageImpl,
          typename... Futures>
struct FirstControlBlock {};

}  // namespace weave::futures::thunks