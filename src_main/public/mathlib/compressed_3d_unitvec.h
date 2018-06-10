// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef SOURCE_MATHLIB_COMPRESSED_3D_UNITVEC_H_
#define SOURCE_MATHLIB_COMPRESSED_3D_UNITVEC_H_

#include "base/include/base_types.h"
#include "mathlib/vector.h"
#include "tier0/include/dbg.h"

#define UNITVEC_DECLARE_STATICS           \
  f32 cUnitVector::mUVAdjustment[0x2000]; \
  Vector cUnitVector::mTmpVec;

// upper 3 bits
#define SIGN_MASK 0xe000
#define XSIGN_MASK 0x8000
#define YSIGN_MASK 0x4000
#define ZSIGN_MASK 0x2000

// middle 6 bits - xbits
#define TOP_MASK 0x1f80

// lower 7 bits - ybits
#define BOTTOM_MASK 0x007f

// unitcomp.cpp : A Unit Vector to 16-bit word conversion
// algorithm based on work of Rafael Baptista (rafael@oroboro.com)
// Accuracy improved by O.D. (punkfloyd@rocketmail.com)
// Used with Permission.

// a compressed unit vector. reasonable fidelty for unit
// vectors in a 16 bit package. Good enough for surface normals
// we hope.
class cUnitVector {
 public:
  cUnitVector() { mVec = 0; }
  cUnitVector(const Vector& vec) { packVector(vec); }
  cUnitVector(u16 val) { mVec = val; }

  cUnitVector& operator=(const Vector& vec) {
    packVector(vec);
    return *this;
  }

  operator Vector() {
    unpackVector(mTmpVec);
    return mTmpVec;
  }

  void packVector(const Vector& vec) {
    // convert from Vector to cUnitVector

    Assert(vec.IsValid());
    Vector tmp = vec;

    // input vector does not have to be unit length
    // Assert( tmp.length() <= 1.001f );

    mVec = 0;
    if (tmp.x < 0) {
      mVec |= XSIGN_MASK;
      tmp.x = -tmp.x;
    }
    if (tmp.y < 0) {
      mVec |= YSIGN_MASK;
      tmp.y = -tmp.y;
    }
    if (tmp.z < 0) {
      mVec |= ZSIGN_MASK;
      tmp.z = -tmp.z;
    }

    // project the normal onto the plane that goes through
    // X0=(1,0,0),Y0=(0,1,0),Z0=(0,0,1).
    // on that plane we choose an (projective!) coordinate system
    // such that X0->(0,0), Y0->(126,0), Z0->(0,126),(0,0,0)->Infinity

    // a little slower... old pack was 4 multiplies and 2 adds.
    // This is 2 multiplies, 2 adds, and a divide....
    f32 w = 126.0f / (tmp.x + tmp.y + tmp.z);
    long xbits = (long)(tmp.x * w);
    long ybits = (long)(tmp.y * w);

    Assert(xbits < 127);
    Assert(xbits >= 0);
    Assert(ybits < 127);
    Assert(ybits >= 0);

    // Now we can be sure that 0<=xp<=126, 0<=yp<=126, 0<=xp+yp<=126
    // however for the sampling we want to transform this triangle
    // into a rectangle.
    if (xbits >= 64) {
      xbits = 127 - xbits;
      ybits = 127 - ybits;
    }

    // now we that have xp in the range (0,127) and yp in
    // the range (0,63), we can pack all the bits together
    mVec |= (xbits << 7);
    mVec |= ybits;
  }

  void unpackVector(Vector& vec) const {
    // if we do a straightforward backward transform
    // we will get points on the plane X0,Y0,Z0
    // however we need points on a sphere that goes through
    // these points. Therefore we need to adjust x,y,z so
    // that x^2+y^2+z^2=1 by normalizing the vector. We have
    // already precalculated the amount by which we need to
    // scale, so all we do is a table lookup and a
    // multiplication

    // get the x and y bits
    long xbits = ((mVec & TOP_MASK) >> 7);
    long ybits = (mVec & BOTTOM_MASK);

    // map the numbers back to the triangle (0,0)-(0,126)-(126,0)
    if ((xbits + ybits) >= 127) {
      xbits = 127 - xbits;
      ybits = 127 - ybits;
    }

    // do the inverse transform and normalization
    // costs 3 extra multiplies and 2 subtracts. No big deal.
    f32 uvadj = mUVAdjustment[mVec & ~SIGN_MASK];
    vec.x = uvadj * (f32)xbits;
    vec.y = uvadj * (f32)ybits;
    vec.z = uvadj * (f32)(126 - xbits - ybits);

    // set all the sign bits
    if (mVec & XSIGN_MASK) vec.x = -vec.x;
    if (mVec & YSIGN_MASK) vec.y = -vec.y;
    if (mVec & ZSIGN_MASK) vec.z = -vec.z;

    Assert(vec.IsValid());
  }

  static void initializeStatics() {
    for (int idx = 0; idx < 0x2000; idx++) {
      long xbits = idx >> 7;
      long ybits = idx & BOTTOM_MASK;

      // map the numbers back to the triangle (0,0)-(0,127)-(127,0)
      if ((xbits + ybits) >= 127) {
        xbits = 127 - xbits;
        ybits = 127 - ybits;
      }

      // convert to 3D vectors
      f32 x = (f32)xbits;
      f32 y = (f32)ybits;
      f32 z = (f32)(126 - xbits - ybits);

      // calculate the amount of normalization required
      mUVAdjustment[idx] = 1.0f / sqrtf(y * y + z * z + x * x);
      Assert(_finite(mUVAdjustment[idx]));

      // cerr << mUVAdjustment[idx] << "\t";
      // if ( xbits == 0 ) cerr << "\n";
    }
  }

  // protected: // !!!!

  u16 mVec;
  static f32 mUVAdjustment[0x2000];
  static Vector mTmpVec;
};

#endif  // SOURCE_MATHLIB_COMPRESSED_3D_UNITVEC_H_
