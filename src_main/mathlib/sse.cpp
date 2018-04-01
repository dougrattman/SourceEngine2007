// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: SSE Math primitives.

#include "sse.h"

#include <float.h>  // Needed for FLT_EPSILON
#include <memory.h>
#include <cmath>
#include "mathlib/mathlib.h"
#include "mathlib/vector.h"
#include "tier0/include/basetypes.h"
#include "tier0/include/dbg.h"

#include "tier0/include/memdbgon.h"

#ifndef ARCH_CPU_X86_64

static const u32 _sincos_masks[] = {(u32)0x0, (u32)~0x0};
static const u32 _sincos_inv_masks[] = {(u32)~0x0, (u32)0x0};

// Macros and constants required by some of the SSE assembly:
#define _PS_EXTERN_CONST(Name, Val) \
  const alignas(16) f32 _ps_##Name[4] = {Val, Val, Val, Val}

#define _PS_EXTERN_CONST_TYPE(Name, Type, Val) \
  const alignas(16) Type _ps_##Name[4] = {Val, Val, Val, Val};

#define _EPI32_CONST(Name, Val) \
  static const alignas(16) i32 _epi32_##Name[4] = {Val, Val, Val, Val}
#define _PS_CONST(Name, Val) \
  static const alignas(16) f32 _ps_##Name[4] = {Val, Val, Val, Val}

_PS_EXTERN_CONST(am_0, 0.0f);
_PS_EXTERN_CONST(am_1, 1.0f);
_PS_EXTERN_CONST(am_m1, -1.0f);
_PS_EXTERN_CONST(am_0p5, 0.5f);
_PS_EXTERN_CONST(am_1p5, 1.5f);
_PS_EXTERN_CONST(am_pi, (f32)M_PI);
_PS_EXTERN_CONST(am_pi_o_2, (f32)(M_PI / 2.0));
_PS_EXTERN_CONST(am_2_o_pi, (f32)(2.0 / M_PI));
_PS_EXTERN_CONST(am_pi_o_4, (f32)(M_PI / 4.0));
_PS_EXTERN_CONST(am_4_o_pi, (f32)(4.0 / M_PI));
_PS_EXTERN_CONST_TYPE(am_sign_mask, i32, 0x80000000i32);
_PS_EXTERN_CONST_TYPE(am_inv_sign_mask, i32, ~0x80000000i32);
_PS_EXTERN_CONST_TYPE(am_min_norm_pos, i32, 0x00800000i32);
_PS_EXTERN_CONST_TYPE(am_mant_mask, i32, 0x7f800000i32);
_PS_EXTERN_CONST_TYPE(am_inv_mant_mask, i32, ~0x7f800000i32);

_EPI32_CONST(1, 1);
_EPI32_CONST(2, 2);

_PS_CONST(sincos_p0, 0.15707963267948963959e1f);
_PS_CONST(sincos_p1, -0.64596409750621907082e0f);
_PS_CONST(sincos_p2, 0.7969262624561800806e-1f);
_PS_CONST(sincos_p3, -0.468175413106023168e-2f);

#ifdef PFN_VECTORMA
void __cdecl _SSE_VectorMA(const f32 *start, f32 scale, const f32 *direction,
                           f32 *dest);
#endif

// SSE implementations of optimized routines:
f32 _SSE_Sqrt(f32 x) {
  Assert(s_bMathlibInitialized);
  f32 root = 0.f;
#ifdef _WIN32
  __asm
  {
		sqrtss		xmm0, x
		movss		root, xmm0
  }
#elif OS_POSIX
  __asm__ __volatile__(
      "movss %1,%%xmm2\n"
      "sqrtss %%xmm2,%%xmm1\n"
      "movss %%xmm1,%0"
      : "=m"(root)
      : "m"(x));
#endif
  return root;
}

// Intel / Kipps SSE RSqrt.  Significantly faster than above.
f32 _SSE_RSqrtAccurate(f32 a) {
  f32 x;
  f32 half = 0.5f;
  f32 three = 3.f;

#ifdef _WIN32
  __asm
  {
		movss   xmm3, a;
		movss   xmm1, half;
		movss   xmm2, three;
		rsqrtss xmm0, xmm3;

		mulss   xmm3, xmm0;
		mulss   xmm1, xmm0;
		mulss   xmm3, xmm0;
		subss   xmm2, xmm3;
		mulss   xmm1, xmm2;

		movss   x,    xmm1;
  }
#elif OS_POSIX
  __asm__ __volatile__(
      "movss   %1, %%xmm3 \n\t"
      "movss   %2, %%xmm1 \n\t"
      "movss   %3, %%xmm2 \n\t"
      "rsqrtss %%xmm3, %%xmm0 \n\t"
      "mulss   %%xmm0, %%xmm3 \n\t"
      "mulss   %%xmm0, %%xmm1 \n\t"
      "mulss   %%xmm0, %%xmm3 \n\t"
      "subss   %%xmm3, %%xmm2 \n\t"
      "mulss   %%xmm2, %%xmm1 \n\t"
      "movss   %%xmm1, %0 \n\t"
      : "=m"(x)
      : "m"(a), "m"(half), "m"(three));
#else
#error "Not Implemented"
#endif

  return x;
}

// Simple SSE rsqrt.  Usually accurate to around 6 (relative) decimal places
// or so, so ok for closed transforms.  (ie, computing lighting normals)
f32 _SSE_RSqrtFast(f32 x) {
  Assert(s_bMathlibInitialized);

  f32 rroot;
#ifdef _WIN32
  __asm
  {
		rsqrtss	xmm0, x
		movss	rroot, xmm0
  }
#elif OS_POSIX
  __asm__ __volatile__(
      "rsqrtss %1, %%xmm0 \n\t"
      "movss %%xmm0, %0 \n\t"
      : "=m"(x)
      : "m"(rroot)
      : "%xmm0");
#else
#error
#endif

  return rroot;
}

f32 SOURCE_FASTCALL _SSE_VectorNormalize(Vector &vec) {
  Assert(s_bMathlibInitialized);

  // NOTE: This is necessary to prevent an memory overwrite...
  // sice vec only has 3 floats, we can't "movaps" directly into it.
  alignas(16) f32 result[4];

  f32 *v = &vec[0];
  f32 *r = &result[0];

  f32 radius = 0.f;
  // Blah, get rid of these comparisons ... in reality, if you have all 3 as
  // zero, it shouldn't be much of a performance win, considering you will very
  // likely miss 3 branch predicts in a row.
  if (v[0] || v[1] || v[2]) {
#ifdef _WIN32
    __asm
    {
			mov			eax, v
			mov			edx, r
#ifdef ALIGNED_VECTOR
			movaps		xmm4, [eax]  // r4 = vx, vy, vz, X
			movaps		xmm1, xmm4  // r1 = r4
#else
			movups		xmm4, [eax]  // r4 = vx, vy, vz, X
			movaps		xmm1, xmm4  // r1 = r4
#endif
			mulps		xmm1, xmm4  // r1 = vx * vx, vy * vy, vz * vz, X
			movhlps		xmm3, xmm1  // r3 = vz * vz, X, X, X
			movaps		xmm2, xmm1  // r2 = r1
			shufps		xmm2, xmm2, 1  // r2 = vy * vy, X, X, X
			addss		xmm1, xmm2  // r1 = (vx * vx) + (vy * vy), X, X, X
			addss		xmm1, xmm3  // r1 = (vx * vx) + (vy * vy) + (vz * vz), X, X, X
			sqrtss		xmm1, xmm1  // r1 = sqrt((vx * vx) + (vy * vy) + (vz * vz)), X, X, X
			movss		radius, xmm1  // radius = sqrt((vx * vx) + (vy * vy) + (vz * vz))
			rcpss		xmm1, xmm1  // r1 = 1/radius, X, X, X
			shufps		xmm1, xmm1, 0  // r1 = 1/radius, 1/radius, 1/radius, X
			mulps		xmm4, xmm1  // r4 = vx * 1/radius, vy * 1/radius, vz * 1/radius, X
			movaps		[edx], xmm4  // v = vx * 1/radius, vy * 1/radius, vz * 1/radius, X
    }
#elif OS_POSIX
    __asm__ __volatile__(
#ifdef ALIGNED_VECTOR
        "movaps          %2, %%xmm4 \n\t"
        "movaps          %%xmm4, %%xmm1 \n\t"
#else
        "movups          %2, %%xmm4 \n\t"
        "movaps          %%xmm4, %%xmm1 \n\t"
#endif
        "mulps           %%xmm4, %%xmm1 \n\t"
        "movhlps         %%xmm1, %%xmm3 \n\t"
        "movaps          %%xmm1, %%xmm2 \n\t"
        "shufps          $1, %%xmm2, %%xmm2 \n\t"
        "addss           %%xmm2, %%xmm1 \n\t"
        "addss           %%xmm3, %%xmm1 \n\t"
        "sqrtss          %%xmm1, %%xmm1 \n\t"
        "movss           %%xmm1, %0 \n\t"
        "rcpss           %%xmm1, %%xmm1 \n\t"
        "shufps          $0, %%xmm1, %%xmm1 \n\t"
        "mulps           %%xmm1, %%xmm4 \n\t"
        "movaps          %%xmm4, %1 \n\t"
        : "=m"(radius), "=m"(result)
        : "m"(*v));
#else
#error "Not Implemented"
#endif
    vec.x = result[0];
    vec.y = result[1];
    vec.z = result[2];
  }

  return radius;
}

void SOURCE_FASTCALL _SSE_VectorNormalizeFast(Vector &vec) {
  f32 ool = _SSE_RSqrtAccurate(FLT_EPSILON + vec.x * vec.x + vec.y * vec.y +
                               vec.z * vec.z);

  vec.x *= ool;
  vec.y *= ool;
  vec.z *= ool;
}

f32 _SSE_InvRSquared(const f32 *v) {
  f32 inv_r2 = 1.f;
#ifdef _WIN32
  __asm {  // Intel SSE only routine
		mov			eax, v
		movss		xmm5, inv_r2  // x5 = 1.0, 0, 0, 0
#ifdef ALIGNED_VECTOR
		movaps		xmm4, [eax]   // x4 = vx, vy, vz, X
#else
		movups		xmm4, [eax]  // x4 = vx, vy, vz, X
#endif
		movaps		xmm1, xmm4  // x1 = x4
		mulps		xmm1, xmm4  // x1 = vx * vx, vy * vy, vz * vz, X
		movhlps		xmm3, xmm1  // x3 = vz * vz, X, X, X
		movaps		xmm2, xmm1  // x2 = x1
		shufps		xmm2, xmm2, 1  // x2 = vy * vy, X, X, X
		addss		xmm1, xmm2  // x1 = (vx * vx) + (vy * vy), X, X, X
		addss		xmm1, xmm3  // x1 = (vx * vx) + (vy * vy) + (vz * vz), X, X, X
		maxss		xmm1, xmm5  // x1 = std::max( 1.0, x1 )
		rcpss		xmm0, xmm1  // x0 = 1 / std::max( 1.0, x1 )
		movss		inv_r2, xmm0  // inv_r2 = x0
  }
#elif OS_POSIX
  __asm__ __volatile__(
#ifdef ALIGNED_VECTOR
      "movaps          %1, %%xmm4 \n\t"
#else
      "movups          %1, %%xmm4 \n\t"
#endif
      "movaps          %%xmm4, %%xmm1 \n\t"
      "mulps           %%xmm4, %%xmm1 \n\t"
      "movhlps         %%xmm1, %%xmm3 \n\t"
      "movaps          %%xmm1, %%xmm2 \n\t"
      "shufps          $1, %%xmm2, %%xmm2 \n\t"
      "addss           %%xmm2, %%xmm1 \n\t"
      "addss           %%xmm3, %%xmm1 \n\t"
      "maxss           %%xmm5, %%xmm1 \n\t"
      "rcpss           %%xmm1, %%xmm0 \n\t"
      "movss           %%xmm0, %0 \n\t"
      : "=m"(inv_r2)
      : "m"(*v), "0"(inv_r2));
#else
#error "Not Implemented"
#endif

  return inv_r2;
}

void _SSE_SinCos(f32 x, f32 *s, f32 *c) {
#ifdef _WIN32
  f32 t4, t8, t12;

  __asm
  {
		movss	xmm0, x
		movss	t12, xmm0
		movss	xmm1, _ps_am_inv_sign_mask
		mov		eax, t12
		mulss	xmm0, _ps_am_2_o_pi
		andps	xmm0, xmm1
		and		eax, 0x80000000

		cvttss2si	edx, xmm0
		mov		ecx, edx
		mov		t12, esi
		mov		esi, edx
		add		edx, 0x1	
		shl		ecx, (31 - 1)
		shl		edx, (31 - 1)

		movss	xmm4, _ps_am_1
		cvtsi2ss	xmm3, esi
		mov		t8, eax
		and		esi, 0x1

		subss	xmm0, xmm3
		movss	xmm3, _sincos_inv_masks[esi * 4]
		minss	xmm0, xmm4

		subss	xmm4, xmm0

		movss	xmm6, xmm4
		andps	xmm4, xmm3
		and		ecx, 0x80000000
		movss	xmm2, xmm3
		andnps	xmm3, xmm0
		and		edx, 0x80000000
		movss	xmm7, t8
		andps	xmm0, xmm2
		mov		t8, ecx
		mov		t4, edx
		orps	xmm4, xmm3

		mov		eax, s  // mov eax, [esp + 4 + 16]
		mov		edx, c  // mov edx, [esp + 4 + 16 + 4]

		andnps	xmm2, xmm6
		orps	xmm0, xmm2

		movss	xmm2, t8
		movss	xmm1, xmm0
		movss	xmm5, xmm4
		xorps	xmm7, xmm2
		movss	xmm3, _ps_sincos_p3
		mulss	xmm0, xmm0
		mulss	xmm4, xmm4
		movss	xmm2, xmm0
		movss	xmm6, xmm4
		orps	xmm1, xmm7
		movss	xmm7, _ps_sincos_p2
		mulss	xmm0, xmm3
		mulss	xmm4, xmm3
		movss	xmm3, _ps_sincos_p1
		addss	xmm0, xmm7
		addss	xmm4, xmm7
		movss	xmm7, _ps_sincos_p0
		mulss	xmm0, xmm2
		mulss	xmm4, xmm6
		addss	xmm0, xmm3
		addss	xmm4, xmm3
		movss	xmm3, t4
		mulss	xmm0, xmm2
		mulss	xmm4, xmm6
		orps	xmm5, xmm3
		mov		esi, t12
		addss	xmm0, xmm7
		addss	xmm4, xmm7
		mulss	xmm0, xmm1
		mulss	xmm4, xmm5

        // use full stores since caller might reload with full loads
		movss	[eax], xmm0
		movss	[edx], xmm4
  }
#elif OS_POSIX
#warning "_SSE_sincos NOT implemented!"
#else
#error "Not Implemented"
#endif
}

f32 _SSE_cos(f32 x) {
#ifdef _WIN32
  f32 temp;
  __asm
  {
		movss	xmm0, x
		movss	xmm1, _ps_am_inv_sign_mask
		andps	xmm0, xmm1
		addss	xmm0, _ps_am_pi_o_2
		mulss	xmm0, _ps_am_2_o_pi

		cvttss2si	ecx, xmm0
		movss	xmm5, _ps_am_1
		mov		edx, ecx
		shl		edx, (31 - 1)
		cvtsi2ss	xmm1, ecx
		and		edx, 0x80000000
		and		ecx, 0x1

		subss	xmm0, xmm1
		movss	xmm6, _sincos_masks[ecx * 4]
		minss	xmm0, xmm5

		movss	xmm1, _ps_sincos_p3
		subss	xmm5, xmm0

		andps	xmm5, xmm6
		movss	xmm7, _ps_sincos_p2
		andnps	xmm6, xmm0
		mov		temp, edx
		orps	xmm5, xmm6
		movss	xmm0, xmm5

		mulss	xmm5, xmm5
		movss	xmm4, _ps_sincos_p1
		movss	xmm2, xmm5
		mulss	xmm5, xmm1
		movss	xmm1, _ps_sincos_p0
		addss	xmm5, xmm7
		mulss	xmm5, xmm2
		movss	xmm3, temp
		addss	xmm5, xmm4
		mulss	xmm5, xmm2
		orps	xmm0, xmm3
		addss	xmm5, xmm1
		mulss	xmm0, xmm5
		
		movss   x,    xmm0

  }
#elif OS_POSIX
#warning "_SSE_cos NOT implemented!"
#else
#error "Not Implemented"
#endif

  return x;
}

// SSE2 implementations of optimized routines:// any x

void _SSE2_SinCos(f32 x, f32 *s, f32 *c){
#ifdef _WIN32
    // clang-format off
  __asm
  {
    movss	xmm0, x
    movaps	xmm7, xmm0
    movss	xmm1, _ps_am_inv_sign_mask
    movss	xmm2, _ps_am_sign_mask
    movss	xmm3, _ps_am_2_o_pi
    andps	xmm0, xmm1
    andps	xmm7, xmm2
    mulss	xmm0, xmm3

    pxor	xmm3, xmm3
    movd	xmm5, _epi32_1
    movss	xmm4, _ps_am_1

    cvttps2dq	xmm2, xmm0
    pand	xmm5, xmm2
    movd	xmm1, _epi32_2
    pcmpeqd	xmm5, xmm3
    movd	xmm3, _epi32_1
    cvtdq2ps	xmm6, xmm2
    paddd	xmm3, xmm2
    pand	xmm2, xmm1
    pand	xmm3, xmm1
    subss	xmm0, xmm6
    pslld	xmm2, (31 - 1)
    minss	xmm0, xmm4

    mov		eax, s  // mov eax, [esp + 4 + 16]
    mov		edx, c  // mov edx, [esp + 4 + 16 + 4]

    subss	xmm4, xmm0
    pslld	xmm3, (31 - 1)

    movaps	xmm6, xmm4
    xorps	xmm2, xmm7
    movaps	xmm7, xmm5
    andps	xmm6, xmm7
    andnps	xmm7, xmm0
    andps	xmm0, xmm5
    andnps	xmm5, xmm4
    movss	xmm4, _ps_sincos_p3
    orps	xmm6, xmm7
    orps	xmm0, xmm5
    movss	xmm5, _ps_sincos_p2

    movaps	xmm1, xmm0
    movaps	xmm7, xmm6
    mulss	xmm0, xmm0
    mulss	xmm6, xmm6
    orps	xmm1, xmm2
    orps	xmm7, xmm3
    movaps	xmm2, xmm0
    movaps	xmm3, xmm6
    mulss	xmm0, xmm4
    mulss	xmm6, xmm4
    movss	xmm4, _ps_sincos_p1
    addss	xmm0, xmm5
    addss	xmm6, xmm5
    movss	xmm5, _ps_sincos_p0
    mulss	xmm0, xmm2
    mulss	xmm6, xmm3
    addss	xmm0, xmm4
    addss	xmm6, xmm4
    mulss	xmm0, xmm2
    mulss	xmm6, xmm3
    addss	xmm0, xmm5
    addss	xmm6, xmm5
    mulss	xmm0, xmm1
    mulss	xmm6, xmm7

    // use full stores since caller might reload with full loads
    movss [eax], xmm0
    movss [edx], xmm6
  }
// clang-format on
#elif OS_POSIX
#warning "_SSE2_SinCos NOT implemented!"
#else
#error "Not Implemented"
#endif
}

f32 _SSE2_cos(f32 x) {
#ifdef _WIN32
  __asm
  {
		movss	xmm0, x
		movss	xmm1, _ps_am_inv_sign_mask
		movss	xmm2, _ps_am_pi_o_2
		movss	xmm3, _ps_am_2_o_pi
		andps	xmm0, xmm1
		addss	xmm0, xmm2
		mulss	xmm0, xmm3

		pxor	xmm3, xmm3
		movd	xmm5, _epi32_1
		movss	xmm4, _ps_am_1
		cvttps2dq	xmm2, xmm0
		pand	xmm5, xmm2
		movd	xmm1, _epi32_2
		pcmpeqd	xmm5, xmm3
		cvtdq2ps	xmm6, xmm2
		pand	xmm2, xmm1
		pslld	xmm2, (31 - 1)

		subss	xmm0, xmm6
		movss	xmm3, _ps_sincos_p3
		minss	xmm0, xmm4
		subss	xmm4, xmm0
		andps	xmm0, xmm5
		andnps	xmm5, xmm4
		orps	xmm0, xmm5

		movaps	xmm1, xmm0
		movss	xmm4, _ps_sincos_p2
		mulss	xmm0, xmm0
		movss	xmm5, _ps_sincos_p1
		orps	xmm1, xmm2
		movaps	xmm7, xmm0
		mulss	xmm0, xmm3
		movss	xmm6, _ps_sincos_p0
		addss	xmm0, xmm4
		mulss	xmm0, xmm7
		addss	xmm0, xmm5
		mulss	xmm0, xmm7
		addss	xmm0, xmm6
		mulss	xmm0, xmm1
		movss   x,    xmm0
  }
#elif OS_POSIX
#warning "_SSE2_cos NOT implemented!"
#else
#error "Not Implemented"
#endif

  return x;
}

// SSE Version of VectorTransform
void VectorTransformSSE(const f32 *in1, const matrix3x4_t &in2, f32 *out1) {
  Assert(s_bMathlibInitialized);
  Assert(in1 != out1);

#ifdef _WIN32
  __asm
  {
		mov eax, in1;
		mov ecx, in2;
		mov edx, out1;

		movss xmm0, [eax];
		mulss xmm0, [ecx];
		movss xmm1, [eax+4];
		mulss xmm1, [ecx+4];
		movss xmm2, [eax+8];
		mulss xmm2, [ecx+8];
		addss xmm0, xmm1;
		addss xmm0, xmm2;
		addss xmm0, [ecx+12]
 		movss [edx], xmm0;
		add ecx, 16;

		movss xmm0, [eax];
		mulss xmm0, [ecx];
		movss xmm1, [eax+4];
		mulss xmm1, [ecx+4];
		movss xmm2, [eax+8];
		mulss xmm2, [ecx+8];
		addss xmm0, xmm1;
		addss xmm0, xmm2;
		addss xmm0, [ecx+12]
		movss [edx+4], xmm0;
		add ecx, 16;

		movss xmm0, [eax];
		mulss xmm0, [ecx];
		movss xmm1, [eax+4];
		mulss xmm1, [ecx+4];
		movss xmm2, [eax+8];
		mulss xmm2, [ecx+8];
		addss xmm0, xmm1;
		addss xmm0, xmm2;
		addss xmm0, [ecx+12]
		movss [edx+8], xmm0;
  }
#elif OS_POSIX
#warning "VectorTransformSSE C implementation only"
  out1[0] = DotProduct(in1, in2[0]) + in2[0][3];
  out1[1] = DotProduct(in1, in2[1]) + in2[1][3];
  out1[2] = DotProduct(in1, in2[2]) + in2[2][3];
#else
#error "Not Implemented"
#endif
}

void VectorRotateSSE(const f32 *in1, const matrix3x4_t &in2, f32 *out1) {
  Assert(s_bMathlibInitialized);
  Assert(in1 != out1);

#ifdef _WIN32
  __asm
  {
		mov eax, in1;
		mov ecx, in2;
		mov edx, out1;

		movss xmm0, [eax];
		mulss xmm0, [ecx];
		movss xmm1, [eax+4];
		mulss xmm1, [ecx+4];
		movss xmm2, [eax+8];
		mulss xmm2, [ecx+8];
		addss xmm0, xmm1;
		addss xmm0, xmm2;
 		movss [edx], xmm0;
		add ecx, 16;

		movss xmm0, [eax];
		mulss xmm0, [ecx];
		movss xmm1, [eax+4];
		mulss xmm1, [ecx+4];
		movss xmm2, [eax+8];
		mulss xmm2, [ecx+8];
		addss xmm0, xmm1;
		addss xmm0, xmm2;
		movss [edx+4], xmm0;
		add ecx, 16;

		movss xmm0, [eax];
		mulss xmm0, [ecx];
		movss xmm1, [eax+4];
		mulss xmm1, [ecx+4];
		movss xmm2, [eax+8];
		mulss xmm2, [ecx+8];
		addss xmm0, xmm1;
		addss xmm0, xmm2;
		movss [edx+8], xmm0;
  }
#elif OS_POSIX
#warning "VectorRotateSSE C implementation only"
  out1[0] = DotProduct(in1, in2[0]);
  out1[1] = DotProduct(in1, in2[1]);
  out1[2] = DotProduct(in1, in2[2]);
#else
#error "Not Implemented"
#endif
}

#ifdef _WIN32
void _declspec(naked) _SSE_VectorMA(const f32 *start, f32 scale,
                                    const f32 *direction, f32 *dest) {
  // TODO(d.rattman): This don't work!! It will overwrite memory in the write to
  // dest
  Assert(0);

  Assert(s_bMathlibInitialized);
  __asm {  // Intel SSE only routine
		mov	eax, DWORD PTR [esp+0x04]	; *start, s0..s2
		mov ecx, DWORD PTR [esp+0x0c]	; *direction, d0..d2
		mov edx, DWORD PTR [esp+0x10]	; *dest
		movss	xmm2, [esp+0x08]		; x2 = scale, 0, 0, 0
#ifdef ALIGNED_VECTOR
		movaps	xmm3, [ecx]				; x3 = dir0,dir1,dir2,X
		pshufd	xmm2, xmm2, 0			; x2 = scale, scale, scale, scale
		movaps	xmm1, [eax]				; x1 = start1, start2, start3, X
		mulps	xmm3, xmm2				; x3 *= x2
		addps	xmm3, xmm1				; x3 += x1
		movaps	[edx], xmm3				; *dest = x3
#else
		movups	xmm3, [ecx]				; x3 = dir0,dir1,dir2,X
		pshufd	xmm2, xmm2, 0			; x2 = scale, scale, scale, scale
		movups	xmm1, [eax]				; x1 = start1, start2, start3, X
		mulps	xmm3, xmm2				; x3 *= x2
		addps	xmm3, xmm1				; x3 += x1
		movups	[edx], xmm3				; *dest = x3
#endif
  }
}
#endif

#ifdef _WIN32
#ifdef PFN_VECTORMA
void _declspec(naked) __cdecl _SSE_VectorMA(const Vector &start, f32 scale,
                                            const Vector &direction,
                                            Vector &dest) {
  // TODO(d.rattman): This don't work!! It will overwrite memory in the write to
  // dest
  Assert(0);

  Assert(s_bMathlibInitialized);
  __asm
  {
    // Intel SSE only routine
		mov	eax, DWORD PTR [esp+0x04]	; *start, s0..s2
		mov ecx, DWORD PTR [esp+0x0c]	; *direction, d0..d2
		mov edx, DWORD PTR [esp+0x10]	; *dest
		movss	xmm2, [esp+0x08]		; x2 = scale, 0, 0, 0
#ifdef ALIGNED_VECTOR
		movaps	xmm3, [ecx]				; x3 = dir0,dir1,dir2,X
		pshufd	xmm2, xmm2, 0			; x2 = scale, scale, scale, scale
		movaps	xmm1, [eax]				; x1 = start1, start2, start3, X
		mulps	xmm3, xmm2				; x3 *= x2
		addps	xmm3, xmm1				; x3 += x1
		movaps	[edx], xmm3				; *dest = x3
#else
		movups	xmm3, [ecx]				; x3 = dir0,dir1,dir2,X
		pshufd	xmm2, xmm2, 0			; x2 = scale, scale, scale, scale
		movups	xmm1, [eax]				; x1 = start1, start2, start3, X
		mulps	xmm3, xmm2				; x3 *= x2
		addps	xmm3, xmm1				; x3 += x1
		movups	[edx], xmm3				; *dest = x3
#endif
  }
}
f32(__cdecl *pfVectorMA)(Vector &v) = _VectorMA;
#endif
#endif

#endif  // !defined ARCH_CPU_X86_64
