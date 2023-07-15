#pragma once

#include <weave/futures/model/output.hpp>

namespace weave::futures {

template <typename C, typename T>
concept Consumer = requires(C* consumer, Output<T> o){
    consumer->Consume(std::move(o));
    // consumer->CancelToken()
};

} // namespace weave::futures 

