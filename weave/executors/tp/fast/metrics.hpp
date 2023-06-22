#pragma once

#include <weave/executors/tp/fast/launch_settings.hpp>

#include <twist/ed/stdlike/atomic.hpp>

#include <cstdlib>

#include <iostream>

namespace weave::executors::tp::fast {

struct WorkerMetrics {
  WorkerMetrics() = default;
  WorkerMetrics(const WorkerMetrics& that) {
    operator+=(that);
  }
  // tasks launched
  size_t launched_from_lifo_slot_{0};
  size_t launched_from_local_queue_{0};
  size_t launched_from_global_queue_{0};

  // times we did something
  size_t times_discarded_lifo_slot_{0};
  size_t times_offloaded_to_global_queue_{0};
  size_t times_went_to_sleep_{0};

  // stealing
  size_t times_tried_stealing_{0};
  size_t times_we_iterated_stealing_{0};
  size_t times_we_failed_to_start_spinning_{0};
  size_t times_stolen_from_lifo_slot_{0};
  size_t tasks_stolen_from_local_queue_{0};

  WorkerMetrics& operator+=(const WorkerMetrics& that) {
    launched_from_lifo_slot_ += that.launched_from_lifo_slot_;
    launched_from_local_queue_ += that.launched_from_local_queue_;
    launched_from_global_queue_ += that.launched_from_global_queue_;

    times_discarded_lifo_slot_ += that.times_discarded_lifo_slot_;
    times_offloaded_to_global_queue_ += that.times_offloaded_to_global_queue_;
    times_went_to_sleep_ += that.times_went_to_sleep_;

    times_tried_stealing_ += that.times_tried_stealing_;
    times_we_iterated_stealing_ += that.times_we_iterated_stealing_;
    times_we_failed_to_start_spinning_ +=
        that.times_we_failed_to_start_spinning_;
    times_stolen_from_lifo_slot_ += that.times_stolen_from_lifo_slot_;
    tasks_stolen_from_local_queue_ += that.tasks_stolen_from_local_queue_;

    return *this;
  }
};

struct PoolMetrics {
  void Print() {
    if constexpr (kCollectMetrics) {
      std::cout << "Tasks launched from lifo: "
                << metrics_.launched_from_lifo_slot_ << std::endl;
      std::cout << "Tasks launched from local queue: "
                << metrics_.launched_from_local_queue_ << std::endl;
      std::cout << "Tasks launched from global queue: "
                << metrics_.launched_from_global_queue_ << std::endl;
      std::cout << std::endl;
      std::cout << "Discarded lifo_slots: "
                << metrics_.times_discarded_lifo_slot_ << std::endl;
      std::cout << "Overflows in local queue: "
                << metrics_.times_offloaded_to_global_queue_ << std::endl;
      std::cout << "Times we parked on syscall: "
                << metrics_.times_went_to_sleep_ << std::endl;
      std::cout << std::endl;
      std::cout << "Steal attempts: " << metrics_.times_tried_stealing_
                << std::endl;
      std::cout << "Steal Iteration attempts: "
                << metrics_.times_we_iterated_stealing_ << std::endl;
      std::cout << "Times denied by coordinator: "
                << metrics_.times_we_failed_to_start_spinning_ << std::endl;
      std::cout << std::endl;
      std::cout << "Tasks stolen from lifo: "
                << metrics_.times_stolen_from_lifo_slot_ << std::endl;
      std::cout << "Tasks stolen from local queue: "
                << metrics_.tasks_stolen_from_local_queue_ << std::endl;
    }
  }

  WorkerMetrics metrics_;
};

}  // namespace weave::executors::tp::fast
