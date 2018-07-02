// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: Random number generator

#ifndef SOURCE_VSTDLIB_RANDOM_H_
#define SOURCE_VSTDLIB_RANDOM_H_

#ifdef _WIN32
#pragma once
#endif

#include "tier0/include/basetypes.h"
#include "tier0/include/threadtools.h"
#include "tier1/interface.h"
#include "vstdlib/vstdlib.h"

// A generator of uniformly distributed random numbers.
the_interface IUniformRandomStream {
 public:
  // Sets the seed of the random number generator.
  virtual void SetSeed(i32 seed) = 0;

  // Generates random numbers
  virtual f32 RandomFloat(f32 min_value = 0.0f, f32 max_value = 1.0f) = 0;
  virtual i32 RandomInt(i32 min_value, i32 max_value) = 0;
  virtual f32 RandomFloatExp(f32 min_value = 0.0f, f32 max_value = 1.0f,
                             f32 exponent = 1.0f) = 0;
};

#define NTAB 32

// The standard generator of uniformly distributed random numbers.
class VSTDLIB_CLASS CUniformRandomStream : public IUniformRandomStream {
 public:
  CUniformRandomStream();

  // Sets the seed of the random number generator.
  void SetSeed(i32 seed) override;

  // Generates random numbers
  f32 RandomFloat(f32 min_value = 0.0f, f32 max_value = 1.0f) override;
  i32 RandomInt(i32 min_value, i32 max_value) override;
  f32 RandomFloatExp(f32 min_value = 0.0f, f32 max_value = 1.0f,
                     f32 exponent = 1.0f) override;

 private:
  i32 GenerateRandomNumber();

  i32 m_idum;
  i32 m_iy;
  i32 m_iv[NTAB];

  CThreadFastMutex m_mutex;
};

// A generator of gaussian distributed random numbers.
class VSTDLIB_CLASS CGaussianRandomStream {
 public:
  // Passing in nullptr will cause the gaussian stream to use the
  // installed global random number generator
  CGaussianRandomStream(IUniformRandomStream *stream = nullptr);

  // Attaches to a random uniform stream
  void AttachToStream(IUniformRandomStream *stream = nullptr);

  // Generates random numbers
  f32 RandomFloat(f32 mean = 0.0f, f32 std_deviation = 1.0f);

 private:
  IUniformRandomStream *uniform_stream_;
  bool have_value_;
  f32 random_value_;

  CThreadFastMutex mutex_;
};

// A couple of convenience functions to access the library's global uniform
// stream.
VSTDLIB_INTERFACE void RandomSeed(i32 seed);
VSTDLIB_INTERFACE f32 RandomFloat(f32 min_value = 0.0f, f32 max_value = 1.0f);
VSTDLIB_INTERFACE f32 RandomFloatExp(f32 min_value = 0.0f, f32 max_value = 1.0f,
                                     f32 exponent = 1.0f);
VSTDLIB_INTERFACE i32 RandomInt(i32 min_value, i32 max_value);
VSTDLIB_INTERFACE f32 RandomGaussianFloat(f32 mean = 0.0f,
                                          f32 std_deviation = 1.0f);

// Installs a global random number generator, which will affect the Random
// functions above.
VSTDLIB_INTERFACE void InstallUniformRandomStream(IUniformRandomStream *stream);

#endif  // SOURCE_VSTDLIB_RANDOM_H_
