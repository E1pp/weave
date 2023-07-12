#pragma once

#include <weave/threads/lockfree/atomic_array.hpp>

#include <fmt/core.h>

#include <wheels/core/assert.hpp>

#include <optional>
#include <unordered_map>
#include <vector>

namespace weave::satellite {

template<bool CollectMetrics, bool AtomicMetrics>
class LoggerImpl {
 public:
  class LoggerShard;
  class Metrics;

  explicit LoggerImpl(const std::vector<std::string>&, size_t){
  }

  LoggerImpl(const LoggerImpl&) = delete;
  LoggerImpl& operator=(const LoggerImpl&) = delete;

  LoggerImpl(LoggerImpl&&) = delete;
  LoggerImpl& operator=(LoggerImpl&&) = delete;

  Metrics GatherMetrics(){
    return Metrics();
  }

  LoggerShard* MakeShard(size_t){
    return &singleton;
  }

  void Accumulate(){
  }

  class LoggerShard {
   public:
    void Increment(std::string_view, size_t){
    }

    LoggerShard& operator+=(const LoggerShard&){
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
    
    void Print(){
    }
  };

 private:
  static inline LoggerShard singleton{};
};

template<bool AtomicMetrics>
class LoggerImpl<true, AtomicMetrics> {
 public:
  class LoggerShard;
  class Metrics;

  friend class LoggerShard;
  friend class Metrics;

  LoggerImpl() = delete;

  explicit LoggerImpl(const std::vector<std::string>& names, size_t num_shards) : shards_(num_shards, std::nullopt), total_(this, names.size()) {
    const size_t size = names.size();

    for(size_t i = 0; i < size; ++i){
      indeces_[names[i]] = i;
    }
  }

  LoggerImpl(const LoggerImpl&) = delete;
  LoggerImpl& operator=(const LoggerImpl&) = delete;

  LoggerImpl(LoggerImpl&&) = delete;
  LoggerImpl& operator=(LoggerImpl&&) = delete;

  Metrics GatherMetrics(){
    Accumulate();

    return total_.GetMetrics();
  }

  LoggerShard* MakeShard(size_t index){
    shards_[index].emplace(this);

    return &*shards_[index];
  }

  void Accumulate(){
    const size_t size = total_.metrics_.Size();

    for(size_t i = 0; i < size; ++i){
      total_.metrics_.Store(i, 0, std::memory_order::relaxed);
    }

    for(auto& shard : shards_){
        total_ += *shard;
    }
  }

 public:

  /////////////////////////////////////////////////////////////////////////////

  class LoggerShard {
    using Logger = LoggerImpl<true, AtomicMetrics>;

    template <bool A, bool B>
    friend class LoggerImpl;
    friend class Metrics;

   public:
    LoggerShard() = delete;

    // Never called but needed for std::optional to work correctly
    LoggerShard(const LoggerShard&) : metrics_(1) {
      std::abort();
    }

    LoggerShard& operator=(const LoggerShard&) = delete;

    LoggerShard(LoggerShard&&) = delete;
    LoggerShard& operator=(LoggerShard&&) = delete;

    explicit LoggerShard(Logger* owner) : owner_(owner), metrics_(owner_->indeces_.size()){
    }

    void Increment(std::string_view name, size_t diff){
      WHEELS_VERIFY(owner_->indeces_.contains(name), "You must use a valid metric name!");
      
      metrics_.FetchAdd(owner_->indeces_[name], diff, std::memory_order::relaxed);
    }

    LoggerShard& operator+=(LoggerShard& that){
      WHEELS_VERIFY(that.owner_ == owner_, "Different Loggers!");

      const size_t size = metrics_.Size();

      for(size_t i = 0; i < size; ++i){
        metrics_.FetchAdd(i, that.LookUp(i), std::memory_order::relaxed);
      }

      return *this;
    }

   private:
    // One consumer
    Metrics GetMetrics(){
      return Metrics(*this);
    }

    size_t LookUp(size_t index){
      return metrics_.Load(index, std::memory_order::relaxed);
    }

    LoggerShard(Logger* owner, size_t count) : owner_(owner), metrics_(count){
    }

   private:
    Logger* owner_;
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
    
    void Print(){
      for(auto [name, count] : data_){
        fmt::println("{}: {}", name, count);
      }
    }

   private:
    explicit Metrics(LoggerShard& source){
      for(auto [name, index] : source.owner_->indeces_){
        data_.emplace_back(name, source.LookUp(index));
      }
    }

   private:
    std::vector<std::pair<std::string, size_t>> data_{};
  };

 private:
  std::unordered_map<std::string_view, size_t> indeces_{};
  std::vector<std::optional<LoggerShard>> shards_;
  LoggerShard total_;
};

} // namespace weave::satellite