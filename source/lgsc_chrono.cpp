/* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.  
SPDX-License-Identifier: Apache-2.0 */

#if _WIN32
#  define WIN32_LEAN_AND_MEAN
#  define NOMINMAX
#  include <windows.h>
#endif

#include "lgsc_chrono.h"

#if defined(HAVE_GETRUSAGE)
#  include <sys/resource.h>
#  include <sys/time.h>
#endif

namespace lgsc { namespace chrono {

#if _WIN32
namespace detail {
  using hundredns = std::chrono::duration<long long, std::ratio<1, 10'000'000>>;

  // Global state to emulate accounting for children (e.g., via a library launcher).
  static hundredns g_cumulative_time_children(0);
} // namespace detail

utime_inc_children_clock::time_point utime_inc_children_clock::now() noexcept {
  FILETIME createTime{}, exitTime{}, kernelTime{}, userTime{};
  ::GetProcessTimes(::GetCurrentProcess(), &createTime, &exitTime, &kernelTime, &userTime);

  ULARGE_INTEGER val{};
  val.LowPart  = userTime.dwLowDateTime;
  val.HighPart = userTime.dwHighDateTime;

  const auto user_100ns = detail::hundredns(static_cast<long long>(val.QuadPart));
  const auto total_100ns = user_100ns + detail::g_cumulative_time_children;

  const auto total_ns = std::chrono::duration_cast<duration>(total_100ns);
  return time_point(total_ns);
}
#elif defined(HAVE_GETRUSAGE)
utime_inc_children_clock::time_point utime_inc_children_clock::now() noexcept {
  std::chrono::nanoseconds total{0};

  rusage usage{};
  getrusage(RUSAGE_SELF, &usage);
  total += std::chrono::seconds(usage.ru_utime.tv_sec)
        +  std::chrono::microseconds(usage.ru_utime.tv_usec);

  if (getrusage(RUSAGE_CHILDREN, &usage) == 0) {
    total += std::chrono::seconds(usage.ru_utime.tv_sec)
          +  std::chrono::microseconds(usage.ru_utime.tv_usec);
  }

  return time_point(total);
}
#else
// Fallback: not true user CPU time, but provides a steady-like progression.
utime_inc_children_clock::time_point utime_inc_children_clock::now() noexcept {
  const auto now = std::chrono::steady_clock::now().time_since_epoch();
  return time_point(std::chrono::duration_cast<duration>(now));
}
#endif

} } // namespace lgsc::chrono
