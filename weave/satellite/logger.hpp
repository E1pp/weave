#pragma once

#include <weave/result/types/unit.hpp>

#include <weave/threads/lockfree/atomic_array.hpp>

#include <fmt/core.h>

#include <wheels/core/assert.hpp>

#include <optional>
#include <unordered_map>
#include <vector>

namespace weave::satellite {

/////////////////////////////////////////////////////////////////////////////

template <bool CollectMetrics, bool AtomicMetrics>
class Logger {
 public:
  class LoggerShard;
  class Metrics;

  explicit Logger(const std::vector<std::string>&, size_t) {
  }

  Logger(const Logger&) = delete;
  Logger& operator=(const Logger&) = delete;

  Logger(Logger&&) = delete;
  Logger& operator=(Logger&&) = delete;

  Metrics GatherMetrics() {
    return Metrics();
  }

  LoggerShard* MakeShard(size_t) {
    return &singleton;
  }

  void Accumulate() {
  }

  class LoggerShard {
   public:
    void Increment(std::string_view, size_t) {
    }

    LoggerShard& operator+=(const LoggerShard&) {
      return *this;
    }
  };

  class Metrics {
   public:
    Metrics() = default;

    Metrics(const Metrics&) = default;
    Metrics& operator=(const Metrics&) = default;

    Metrics(Metrics&&) = default;
    Metrics& operator=(Metrics&&) = default;

    void Print() {
    }

    Unit Data() && {
      return {};
    }
  };

 private:
  static inline LoggerShard singleton{};
};

template <bool AtomicMetrics>
class Logger<true, AtomicMetrics> {
 public:
  class LoggerShard;
  class Metrics;

  friend class LoggerShard;
  friend class Metrics;

  Logger() = delete;

  explicit Logger(const std::vector<std::string>& names, size_t num_shards)
      : shards_(num_shards, std::nullopt),
        total_(this, names.size()) {
    const size_t size = names.size();

    for (size_t i = 0; i < size; ++i) {
      indices_[names[i]] = i;
    }
  }

  Logger(const Logger&) = delete;
  Logger& operator=(const Logger&) = delete;

  Logger(Logger&&) = delete;
  Logger& operator=(Logger&&) = delete;

  Metrics GatherMetrics() {
    Accumulate();

    return total_.GetMetrics();
  }

  LoggerShard* MakeShard(size_t index) {
    shards_[index].emplace(this);

    return &*shards_[index];
  }

 private:
  void Accumulate() {
    const size_t size = total_.metrics_.Size();

    for (size_t i = 0; i < size; ++i) {
      total_.metrics_.Store(i, 0, std::memory_order::relaxed);
    }

    for (auto& shard : shards_) {
      total_ += *shard;
    }
  }

 public:
  /////////////////////////////////////////////////////////////////////////////

  class LoggerShard {
    using Owner = Logger<true, AtomicMetrics>;

    template <bool A, bool B>
    friend class Logger;
    friend class Metrics;

   public:
    LoggerShard() = delete;

    // Never called but needed for std::optional to work correctly
    LoggerShard(const LoggerShard&)
        : metrics_(1) {
      std::abort();
    }

    LoggerShard& operator=(const LoggerShard&) = delete;

    LoggerShard(LoggerShard&&) = delete;
    LoggerShard& operator=(LoggerShard&&) = delete;

    explicit LoggerShard(Owner* owner)
        : owner_(owner),
          metrics_(owner_->indices_.size()) {
    }

    void Increment(std::string_view name, size_t diff) {
      auto pos = owner_->indices_.find(name);

      WHEELS_VERIFY(pos != owner_->indices_.end(),
                    "You must use a valid metric name!");

      // C++20 doesn't support heterogenous lookup for operator[]
      // but find does. Treat this line as owner_->indices_[name]
      size_t index = pos->second;

      metrics_.FetchAdd(index, diff,
                        std::memory_order::relaxed);
    }

    LoggerShard& operator+=(LoggerShard& that) {
      WHEELS_VERIFY(that.owner_ == owner_, "Different Loggers!");

      const size_t size = metrics_.Size();

      for (size_t i = 0; i < size; ++i) {
        metrics_.FetchAdd(i, that.LookUp(i), std::memory_order::relaxed);
      }

      return *this;
    }

   private:
    // One consumer
    Metrics GetMetrics() {
      return Metrics(*this);
    }

    size_t LookUp(size_t index) {
      return metrics_.Load(index, std::memory_order::relaxed);
    }

    LoggerShard(Owner* owner, size_t count)
        : owner_(owner),
          metrics_(count) {
    }

   private:
    Owner* owner_;
    threads::lockfree::MaybeAtomicArray<size_t, AtomicMetrics> metrics_;
  };

  /////////////////////////////////////////////////////////////////////////////

  class Metrics {
    friend class LoggerShard;

   public:
    Metrics() = delete;

    Metrics(const Metrics&) = default;
    Metrics& operator=(const Metrics&) = default;

    Metrics(Metrics&&) = default;
    Metrics& operator=(Metrics&&) = default;

    void Print() {
      for (auto [name, count] : data_) {
        fmt::println("{}: {}", name, count);
      }
    }

    auto Data() && {
      return std::move(data_);
    }

   private:
    explicit Metrics(LoggerShard& source) {
      for (auto [name, index] : source.owner_->indices_) {
        data_.emplace_back(name, source.LookUp(index));
      }
    }

   private:
    std::vector<std::pair<std::string, size_t>> data_{};
  };

 private:

  //////////////////////////////////////////////////////////////////////////////////
  // heterogenous lookup https://www.cppstories.com/2021/heterogeneous-access-cpp20/

  struct StringHash { // NOLINT
    using is_transparent = void; // NOLINT
    [[nodiscard]] size_t operator()(const char *txt) const {
      return std::hash<std::string_view>{}(txt);
    }
    [[nodiscard]] size_t operator()(std::string_view txt) const {
      return std::hash<std::string_view>{}(txt);
    }
    [[nodiscard]] size_t operator()(const std::string &txt) const {
      return std::hash<std::string>{}(txt);
    }
  };

  std::unordered_map<std::string, size_t, StringHash, std::equal_to<>> indices_{};
  std::vector<std::optional<LoggerShard>> shards_;
  LoggerShard total_;
};

}  // namespace weave::satellite