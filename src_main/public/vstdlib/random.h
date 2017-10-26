// Copyright © 1996-2017, Valve Corporation, All rights reserved.
//
// Purpose: Random number generator

#ifndef SOURCE_VSTDLIB_RANDOM_H_
#define SOURCE_VSTDLIB_RANDOM_H_

#include "tier0/basetypes.h"
#include "tier0/threadtools.h"
#include "tier1/interface.h"
#include "vstdlib/vstdlib.h"

#define NTAB 32

// A generator of uniformly distributed random numbers
abstract_class IUniformRandomStream {
 public:
  // Sets the seed of the random number generator
  virtual void SetSeed(int seed) = 0;

  // Generates random numbers
  virtual float RandomFloat(float min_value = 0.0f, float max_value = 1.0f) = 0;
  virtual int RandomInt(int min_value, int max_value) = 0;
  virtual float RandomFloatExp(float min_value = 0.0f, float max_value = 1.0f,
                               float exponent = 1.0f) = 0;
};

// The standard generator of uniformly distributed random numbers
class VSTDLIB_CLASS CUniformRandomStream : public IUniformRandomStream {
 public:
  CUniformRandomStream();

  // Sets the seed of the random number generator
  void SetSeed(int iSeed) override;

  // Generates random numbers
  float RandomFloat(float flMinVal = 0.0f, float flMaxVal = 1.0f) override;
  int RandomInt(int iMinVal, int iMaxVal) override;
  float RandomFloatExp(float flMinVal = 0.0f, float flMaxVal = 1.0f,
                       float flExponent = 1.0f) override;

 private:
  int GenerateRandomNumber();

  int m_idum;
  int m_iy;
  int m_iv[NTAB];

  CThreadFastMutex m_mutex;
};

// A generator of gaussian distributed random numbers
class VSTDLIB_CLASS CGaussianRandomStream {
 public:
  // Passing in NULL will cause the gaussian stream to use the
  // installed global random number generator
  CGaussianRandomStream(IUniformRandomStream *pUniformStream = NULL);

  // Attaches to a random uniform stream
  void AttachToStream(IUniformRandomStream *pUniformStream = NULL);

  // Generates random numbers
  float RandomFloat(float flMean = 0.0f, float flStdDev = 1.0f);

 private:
  IUniformRandomStream *m_pUniformStream;
  bool m_bHaveValue;
  float m_flRandomValue;

  CThreadFastMutex m_mutex;
};

// A couple of convenience functions to access the library's global uniform
// stream
VSTDLIB_INTERFACE void RandomSeed(int iSeed);
VSTDLIB_INTERFACE float RandomFloat(float flMinVal = 0.0f,
                                    float flMaxVal = 1.0f);
VSTDLIB_INTERFACE float RandomFloatExp(float flMinVal = 0.0f,
                                       float flMaxVal = 1.0f,
                                       float flExponent = 1.0f);
VSTDLIB_INTERFACE int RandomInt(int iMinVal, int iMaxVal);
VSTDLIB_INTERFACE float RandomGaussianFloat(float flMean = 0.0f,
                                            float flStdDev = 1.0f);

// Installs a global random number generator, which will affect the Random
// functions above
VSTDLIB_INTERFACE void InstallUniformRandomStream(
    IUniformRandomStream *pStream);

#endif  // SOURCE_VSTDLIB_RANDOM_H_
