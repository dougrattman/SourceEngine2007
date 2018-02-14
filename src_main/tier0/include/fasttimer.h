// Copyright © 1996-2017, Valve Corporation, All rights reserved.

#ifndef SOURCE_TIER0_INCLUDE_FASTTIMER_H_
#define SOURCE_TIER0_INCLUDE_FASTTIMER_H_

#include "base/include/base_types.h"
#include "build/include/build_config.h"

#include <cassert>

#if OS_WIN
#include <intrin.h>
#endif

#include "tier0/include/platform.h"

static i64 g_ClockSpeed;
static unsigned long g_dwClockSpeed;

static f64 g_ClockSpeedMicrosecondsMultiplier;
static f64 g_ClockSpeedMillisecondsMultiplier;
static f64 g_ClockSpeedSecondsMultiplier;

class PLATFORM_CLASS CCycleCount {
  friend class CFastTimer;

 public:
  CCycleCount() { Init(0ui64); }
  CCycleCount(u64 cycles) { Init(cycles); }

  // Sample the clock.
  void Sample() {
#ifdef OS_WIN
    m_Int64 = __rdtsc();
#elif defined(OS_POSIX)
    unsigned long *pSample = (unsigned long *)&m_Int64;
    __asm__ __volatile__(
        "rdtsc\n\t"
        "movl %%eax,  (%0)\n\t"
        "movl %%edx, 4(%0)\n\t"
        : /* no output regs */
        : "D"(pSample)
        : "%eax", "%edx");
#endif
  }

  // Set to zero.
  void Init() { Init(0ui64); }
  void Init(f32 initTimeMsec) {
    if (g_ClockSpeedMillisecondsMultiplier > 0.0)
      Init((u64)(initTimeMsec / g_ClockSpeedMillisecondsMultiplier));
    else
      Init(0ui64);
  }
  void Init(f64 initTimeMsec) { Init((f32)initTimeMsec); }
  void Init(u64 cycles) { m_Int64 = cycles; }

  // Compare two counts.
  bool IsLessThan(CCycleCount const &other) const {
    return m_Int64 < other.m_Int64;
  }

  // Convert to other time representations. These functions are slow, so it's
  // preferable to call them during display rather than inside a timing block.
  unsigned long GetCycles() const { return (unsigned long)m_Int64; }
  u64 GetLongCycles() const { return m_Int64; }

  unsigned long GetMicroseconds() const {
    return (unsigned long)(m_Int64 * 1000000 / g_ClockSpeed);
  }
  u64 GetUlMicroseconds() const { return m_Int64 * 1000000 / g_ClockSpeed; }
  f64 GetMicrosecondsF() const {
    return (f64)(m_Int64 * g_ClockSpeedMicrosecondsMultiplier);
  }
  void SetMicroseconds(unsigned long nMicroseconds) {
    m_Int64 = ((u64)nMicroseconds * g_ClockSpeed) / 1000000;
  }

  unsigned long GetMilliseconds() const {
    return (unsigned long)(m_Int64 * 1000 / g_ClockSpeed);
  }
  f64 GetMillisecondsF() const {
    return (f64)(m_Int64 * g_ClockSpeedMillisecondsMultiplier);
  }
  f64 GetSeconds() const {
    return (f64)(m_Int64 * g_ClockSpeedSecondsMultiplier);
  }

  CCycleCount &operator+=(CCycleCount const &other) {
    m_Int64 += other.m_Int64;
    return *this;
  }

  // dest = rSrc1 + rSrc2
  static void Add(CCycleCount const &rSrc1, CCycleCount const &rSrc2,
                  CCycleCount &dest) {
    dest.m_Int64 = rSrc1.m_Int64 + rSrc2.m_Int64;
  }  // Add two samples together.

  // dest = rSrc1 - rSrc2
  static void Sub(CCycleCount const &rSrc1, CCycleCount const &rSrc2,
                  CCycleCount &dest) {
    dest.m_Int64 = rSrc1.m_Int64 - rSrc2.m_Int64;
  }  // Add two samples together.

  static u64 GetTimestamp() {
    CCycleCount c;
    c.Sample();
    return c.GetLongCycles();
  }

  u64 m_Int64;
};

class CClockSpeedInit {
 public:
  CClockSpeedInit() { Init(); }

  static void Init() {
    const CPUInformation &pi = GetCPUInformation();

    g_ClockSpeed = pi.m_Speed;
    g_dwClockSpeed = (unsigned long)g_ClockSpeed;

    g_ClockSpeedMicrosecondsMultiplier = 1000000.0 / (f64)g_ClockSpeed;
    g_ClockSpeedMillisecondsMultiplier = 1000.0 / (f64)g_ClockSpeed;
    g_ClockSpeedSecondsMultiplier = 1.0f / (f64)g_ClockSpeed;
  }
};

class PLATFORM_CLASS CFastTimer {
 public:
  // These functions are fast to call and should be called from your sampling
  // code.
  void Start() {
    m_Duration.Sample();
#ifdef DEBUG_FASTTIMER
    m_bRunning = true;
#endif
  }
  void End() {
    CCycleCount cnt;
    cnt.Sample();

    m_Duration.m_Int64 = cnt.m_Int64 - m_Duration.m_Int64;

#ifdef DEBUG_FASTTIMER
    m_bRunning = false;
#endif
  }

  const CCycleCount &GetDuration() const {
#ifdef DEBUG_FASTTIMER
    assert(!m_bRunning);
#endif
    return m_Duration;
  }

  // Get the elapsed time between Start and End calls.
  CCycleCount GetDurationInProgress() const {
    CCycleCount cnt;
    cnt.Sample();

    CCycleCount result;
    result.m_Int64 = cnt.m_Int64 - m_Duration.m_Int64;

    return result;
  }  // Call without ending. Not that cheap.

  // Return number of cycles per second on this processor.
  static inline unsigned long GetClockSpeed() { return g_dwClockSpeed; }

 private:
  CCycleCount m_Duration;
#ifdef DEBUG_FASTTIMER
  bool m_bRunning;  // Are we currently running?
#endif
};

// This is a helper class that times whatever block of code it's in
class CTimeScope {
 public:
  CTimeScope(CFastTimer *timer) : timer_{timer} { timer_->Start(); }
  ~CTimeScope() { timer_->End(); }

 private:
  CFastTimer *timer_;
};

// This is a helper class that times whatever block of code it's in and
// adds the total (i32 microseconds) to a global counter.
class CTimeAdder {
 public:
  CTimeAdder(CCycleCount *total_cycles)
      : total_cycles_{total_cycles}, timer_{} {
    timer_.Start();
  }
  ~CTimeAdder() { End(); }

  void End() {
    if (total_cycles_) {
      timer_.End();
      *total_cycles_ += timer_.GetDuration();
      total_cycles_ = nullptr;
    }
  }

 private:
  CCycleCount *total_cycles_;
  CFastTimer timer_;
};

// Simple tool to support timing a block of code, and reporting the results on
// program exit or at each iteration
// Macros used because dbg.h uses this header, thus Msg() is unavailable
#define PROFILE_SCOPE(name)                                                 \
  class C##name##ACC : public CAverageCycleCounter {                        \
   public:                                                                  \
    ~C##name##ACC() {                                                       \
      Msg("%-48s: %6.3f avg (%8.1f total, %7.3f peak, %5d iters)\n", #name, \
          GetAverageMilliseconds(), GetTotalMilliseconds(),                 \
          GetPeakMilliseconds(), GetIters());                               \
    }                                                                       \
  };                                                                        \
  static C##name##ACC name##_ACC;                                           \
  CAverageTimeMarker name##_ATM { &name##_ACC }

#define TIME_SCOPE(name)                                                    \
  class CTimeScopeMsg_##name {                                              \
   public:                                                                  \
    CTimeScopeMsg_##name() { timer_.Start(); }                              \
    ~CTimeScopeMsg_##name() {                                               \
      timer_.End();                                                         \
      Msg(#name "time: %.4fms\n", timer_.GetDuration().GetMillisecondsF()); \
    }                                                                       \
                                                                            \
   private:                                                                 \
    CFastTimer timer_;                                                      \
  } name##_TSM;

class CAverageCycleCounter {
 public:
  CAverageCycleCounter() : iterations_count_{0} {}

  void Init() {
    total_cycles_.Init();
    peak_cycles_.Init();
    iterations_count_ = 0;
  }
  void MarkIter(const CCycleCount &duration) {
    ++iterations_count_;
    total_cycles_ += duration;
    if (peak_cycles_.IsLessThan(duration)) peak_cycles_ = duration;
  }

  u32 GetIters() const { return iterations_count_; }

  f64 GetAverageMilliseconds() const {
    if (iterations_count_)
      return (total_cycles_.GetMillisecondsF() / (f64)iterations_count_);

    return 0.0;
  }
  f64 GetTotalMilliseconds() const { return total_cycles_.GetMillisecondsF(); }
  f64 GetPeakMilliseconds() const { return peak_cycles_.GetMillisecondsF(); }

 private:
  CCycleCount total_cycles_;
  CCycleCount peak_cycles_;
  u32 iterations_count_;
};

class CAverageTimeMarker {
 public:
  CAverageTimeMarker(CAverageCycleCounter *counter)
      : counter_{counter}, timer_{} {
    timer_.Start();
  }
  ~CAverageTimeMarker() {
    timer_.End();
    counter_->MarkIter(timer_.GetDuration());
  }

 private:
  CAverageCycleCounter *counter_;
  CFastTimer timer_;
};

// Use this to time whether a desired interval of time has passed.  It's
// extremely fast to check while running.
class CLimitTimer {
 public:
  // Purpose: Initializes the limit timer with a period of time to measure.
  // Input: cMicroSecDuration - How long a time period to measure.
  void SetLimit(u64 duration_microseconds) {
    const u64 cycles = duration_microseconds * g_dwClockSpeed / 1000000;
    CCycleCount cycleCount;
    cycleCount.Sample();
    cycle_limit_ = cycleCount.GetLongCycles() + cycles;
  }
  // Purpose: Determines whether our specified time period has passed
  // Output:	true if at least the specified time period has passed
  bool BLimitReached() {
    CCycleCount cycleCount;
    cycleCount.Sample();
    return cycleCount.GetLongCycles() >= cycle_limit_;
  }

 private:
  u64 cycle_limit_;
};

#endif  // SOURCE_TIER0_INCLUDE_FASTTIMER_H_
