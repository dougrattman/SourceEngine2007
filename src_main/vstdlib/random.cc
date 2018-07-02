// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: Random number generator

#include "vstdlib/random.h"

#include <cmath>
#include "tier0/include/dbg.h"
#include "tier0/include/memdbgon.h"

#define IA 16807
#define IM 2147483647
#define IQ 127773
#define IR 2836
#define NDIV (1 + (IM - 1) / NTAB)
#define MAX_RANDOM_RANGE 0x7FFFFFFFUL

// fran1 -- return a random floating-point number on the interval [0,1)
#define AM (1.0 / IM)
#define EPS 1.2e-7f
#define RNMX (1.0f - EPS)

namespace {
static CUniformRandomStream s_UniformStream;
static CGaussianRandomStream s_GaussianStream;
static IUniformRandomStream *s_pUniformStream = &s_UniformStream;
}  // namespace

// Installs a global random number generator, which will affect the Random
// functions above
void InstallUniformRandomStream(IUniformRandomStream *pStream) {
  s_pUniformStream = pStream ? pStream : &s_UniformStream;
}

// A couple of convenience functions to access the library's global uniform
// stream
void RandomSeed(i32 iSeed) { s_pUniformStream->SetSeed(iSeed); }

f32 RandomFloat(f32 flMinVal, f32 flMaxVal) {
  return s_pUniformStream->RandomFloat(flMinVal, flMaxVal);
}

f32 RandomFloatExp(f32 flMinVal, f32 flMaxVal, f32 flExponent) {
  return s_pUniformStream->RandomFloatExp(flMinVal, flMaxVal, flExponent);
}

int RandomInt(i32 iMinVal, i32 iMaxVal) {
  return s_pUniformStream->RandomInt(iMinVal, iMaxVal);
}

f32 RandomGaussianFloat(f32 flMean, f32 flStdDev) {
  return s_GaussianStream.RandomFloat(flMean, flStdDev);
}

// Implementation of the uniform random number stream
CUniformRandomStream::CUniformRandomStream() { SetSeed(0); }

void CUniformRandomStream::SetSeed(i32 iSeed) {
  AUTO_LOCK(m_mutex);
  m_idum = ((iSeed < 0) ? iSeed : -iSeed);
  m_iy = 0;
}

int CUniformRandomStream::GenerateRandomNumber() {
  AUTO_LOCK(m_mutex);
  int j, k;

  if (m_idum <= 0 || !m_iy) {
    if (-(m_idum) < 1)
      m_idum = 1;
    else
      m_idum = -(m_idum);

    for (j = NTAB + 7; j >= 0; j--) {
      k = (m_idum) / IQ;
      m_idum = IA * (m_idum - k * IQ) - IR * k;
      if (m_idum < 0) m_idum += IM;
      if (j < NTAB) m_iv[j] = m_idum;
    }
    m_iy = m_iv[0];
  }
  k = (m_idum) / IQ;
  m_idum = IA * (m_idum - k * IQ) - IR * k;
  if (m_idum < 0) m_idum += IM;
  j = m_iy / NDIV;

  m_iy = m_iv[j];
  m_iv[j] = m_idum;

  return m_iy;
}

f32 CUniformRandomStream::RandomFloat(f32 flLow, f32 flHigh) {
  // f32 in [0,1)
  f32 fl = AM * GenerateRandomNumber();
  if (fl > RNMX) fl = RNMX;

  return fl * (flHigh - flLow) + flLow;  // f32 in [low,high)
}

f32 CUniformRandomStream::RandomFloatExp(f32 flMinVal, f32 flMaxVal,
                                         f32 flExponent) {
  // f32 in [0,1)
  f32 fl = AM * GenerateRandomNumber();

  if (fl > RNMX) fl = RNMX;
  if (flExponent != 1.0f) fl = powf(fl, flExponent);

  return fl * (flMaxVal - flMinVal) + flMinVal;  // f32 in [low,high)
}

int CUniformRandomStream::RandomInt(i32 iLow, i32 iHigh) {
  // ASSERT(lLow <= lHigh);
  unsigned int x = iHigh - iLow + 1;
  if (x <= 1 || MAX_RANDOM_RANGE < x - 1) return iLow;

  // The following maps a uniform distribution on the interval
  // [0,MAX_RANDOM_RANGE] to a smaller, client-specified range of [0,x-1] in a
  // way that doesn't bias the uniform distribution unfavorably. Even for a
  // worst case x, the loop is guaranteed to be taken no more than half the
  // time, so for that worst case x, the average number of times through the
  // loop is 2. For cases where x is much smaller than MAX_RANDOM_RANGE, the
  // average number of times through the loop is very close to 1.
  //
  unsigned int maxAcceptable = MAX_RANDOM_RANGE - ((MAX_RANDOM_RANGE + 1) % x);
  unsigned int n;
  do {
    n = GenerateRandomNumber();
  } while (n > maxAcceptable);

  return iLow + (n % x);
}

// Implementation of the gaussian random number stream
// We're gonna use the Box-Muller method (which actually generates 2
// gaussian-distributed numbers at once)
CGaussianRandomStream::CGaussianRandomStream(IUniformRandomStream *stream) {
  AttachToStream(stream);
}

// Attaches to a random uniform stream
void CGaussianRandomStream::AttachToStream(IUniformRandomStream *stream) {
  AUTO_LOCK(mutex_);
  uniform_stream_ = stream;
  have_value_ = false;
}

// Generates random numbers
f32 CGaussianRandomStream::RandomFloat(f32 flMean, f32 flStdDev) {
  AUTO_LOCK(mutex_);

  if (!have_value_) {
    IUniformRandomStream *stream =
        uniform_stream_ ? uniform_stream_ : s_pUniformStream;
    f32 rsq, v1, v2;
    // Pick 2 random #s from -1 to 1
    // Make sure they lie inside the unit circle. If they don't, try again
    do {
      v1 = 2.0f * stream->RandomFloat() - 1.0f;
      v2 = 2.0f * stream->RandomFloat() - 1.0f;
      rsq = v1 * v1 + v2 * v2;
    } while ((rsq > 1.0f) || (rsq == 0.0f));

    // The box-muller transformation to get the two gaussian numbers
    f32 fac = sqrtf(-2.0f * log(rsq) / rsq);

    // Store off one value for later use
    random_value_ = v1 * fac;
    have_value_ = true;

    return flStdDev * (v2 * fac) + flMean;
  }

  have_value_ = false;

  return flStdDev * random_value_ + flMean;
}
