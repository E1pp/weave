#pragma once

#include <cstdlib>
#include <tuple>
#include <variant>

namespace weave::fibers {

template <typename T>
class Channel;

template <size_t N, typename First, typename... Pack>
struct NthTypeImpl {
  using Type = typename NthTypeImpl<N - 1, Pack...>::Type;
};

template <typename First, typename... Pack>
struct NthTypeImpl<0, First, Pack...> {
  using Type = First;
};

template <size_t N, typename... Pack>
using NthType = typename NthTypeImpl<N, Pack...>::Type;

template <typename T>
struct [[nodiscard]] Send {
 public:
  using Type = T;
  using ResultType = std::monostate;

  Send(Channel<T>& chan, T value)
      : chan_(&chan),
        storage_(std::move(value)) {
  }

  Send(Send&& that)
      : chan_(that.chan_),
        storage_(std::move(that.storage_)) {
  }

 public:
  Channel<T>* chan_;
  T storage_;
};

template <typename T>
class [[nodiscard]] Receive {
 public:
  using Type = T;
  using ResultType = T;

  explicit Receive(Channel<T>& chan)
      : chan_(&chan) {
  }

 public:
  Channel<T>* chan_;
};

template <typename T>
concept SelectorAlternative = requires(T alt) {
  typename T::Type;
  typename T::ResultType;

  alt.chan_;
}
&&(std::same_as<T, Send<typename T::Type>> ||
   std::same_as<T, Receive<typename T::Type>>);

template <typename... T>
struct SelectedValueImpl {};

template <SelectorAlternative... Alts>
struct SelectedValueImpl<Alts...> {
  using Type = std::variant<typename Alts::ResultType...>;
};

template <typename... T>
struct MaybeSelectedValueImpl {};

template <SelectorAlternative... Alts>
struct MaybeSelectedValueImpl<Alts...> {
  using Type = std::variant<typename Alts::ResultType..., std::monostate>;
};

template <typename... Types>
using SelectedValue = typename SelectedValueImpl<Types...>::Type;

template <typename... Types>
using MaybeSelectedValue = typename MaybeSelectedValueImpl<Types...>::Type;

}  // namespace weave::fibers