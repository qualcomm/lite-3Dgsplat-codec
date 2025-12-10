/* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <chrono>

namespace lgsc { namespace chrono {

struct utime_inc_children_clock {
  using duration   = std::chrono::nanoseconds;
  using rep        = duration::rep;
  using period     = duration::period;
  using time_point = std::chrono::time_point<utime_inc_children_clock, duration>;

  static constexpr bool is_steady = true;

  static time_point now() noexcept;
};

/**
 * Simple cumulative stopwatch measuring elapsed time intervals using the provided Clock.
 */
template <typename Clock>
class Stopwatch {
public:
  using duration = typename Clock::duration;

  /// Reset the accumulated interval count.
  void reset() noexcept { cumulative_time_ = duration::zero(); }

  /// Mark the beginning of a measurement period.
  void start() noexcept { start_time_ = Clock::now(); }

  /// Mark the end of a measurement period and return the interval since start().
  duration stop() noexcept {
    const auto delta = duration(Clock::now() - start_time_);
    cumulative_time_ += delta;
    return delta;
  }

  /// Sum of all completed elapsed time intervals (excludes any currently active period).
  [[nodiscard]] constexpr duration count() const noexcept { return cumulative_time_; }

private:
  typename Clock::time_point start_time_{};
  duration                   cumulative_time_{duration::zero()};
};

}} // namespace lgsc::chrono
