#pragma once

#include <weave/futures/model/output.hpp>

namespace weave::futures {

template <typename C, typename T>
concept Consumer = requires(C& consumer, Output<T> o, Result<T> r){
    consumer.Consume(std::move(o));
    consumer.Consume(std::move(r));
    // consumer->CancelToken()
};

} // namespace weave::futures 

