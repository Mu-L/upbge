/* SPDX-FileCopyrightText: 2011-2013 Intel Corporation
 * SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "util/math_base.h"
#include "util/types_float4.h"

CCL_NAMESPACE_BEGIN

ccl_device_inline float4 zero_float4()
{
#ifdef __KERNEL_SSE__
  return float4(_mm_setzero_ps());
#else
  return make_float4(0.0f);
#endif
}

ccl_device_inline float4 one_float4()
{
  return make_float4(1.0f);
}

ccl_device_template_spec float4 make_zero()
{
  return zero_float4();
}

ccl_device_inline int4 cast(const float4 a)
{
#ifdef __KERNEL_SSE__
  return int4(_mm_castps_si128(a));
#else
  return make_int4(
      __float_as_int(a.x), __float_as_int(a.y), __float_as_int(a.z), __float_as_int(a.w));
#endif
}

#if !defined(__KERNEL_METAL__)
ccl_device_inline float4 operator-(const float4 &a)
{
#  ifdef __KERNEL_SSE__
  const __m128 mask = _mm_castsi128_ps(_mm_set1_epi32(0x80000000));
  return float4(_mm_xor_ps(a.m128, mask));
#  else
  return make_float4(-a.x, -a.y, -a.z, -a.w);
#  endif
}

ccl_device_inline float4 operator*(const float4 a, const float4 b)
{
#  ifdef __KERNEL_SSE__
  return float4(_mm_mul_ps(a.m128, b.m128));
#  else
  return make_float4(a.x * b.x, a.y * b.y, a.z * b.z, a.w * b.w);
#  endif
}

ccl_device_inline float4 operator*(const float4 a, const float f)
{
#  if defined(__KERNEL_SSE__)
  return a * make_float4(f);
#  else
  return make_float4(a.x * f, a.y * f, a.z * f, a.w * f);
#  endif
}

ccl_device_inline float4 operator*(float f, const float4 a)
{
  return a * f;
}

ccl_device_inline float4 operator/(const float4 a, const float f)
{
  return a * (1.0f / f);
}

ccl_device_inline float4 operator/(const float4 a, const float4 b)
{
#  ifdef __KERNEL_SSE__
  return float4(_mm_div_ps(a.m128, b.m128));
#  else
  return make_float4(a.x / b.x, a.y / b.y, a.z / b.z, a.w / b.w);
#  endif
}

ccl_device_inline float4 operator+(const float4 a, const float4 b)
{
#  ifdef __KERNEL_SSE__
  return float4(_mm_add_ps(a.m128, b.m128));
#  else
  return make_float4(a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w);
#  endif
}

ccl_device_inline float4 operator+(const float4 a, const float f)
{
  return a + make_float4(f);
}

ccl_device_inline float4 operator-(const float4 a, const float4 b)
{
#  ifdef __KERNEL_SSE__
  return float4(_mm_sub_ps(a.m128, b.m128));
#  else
  return make_float4(a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w);
#  endif
}

ccl_device_inline float4 operator-(const float4 a, const float f)
{
  return a - make_float4(f);
}

ccl_device_inline float4 operator+=(float4 &a, const float4 b)
{
  return a = a + b;
}

ccl_device_inline float4 operator-=(float4 &a, const float4 b)
{
  return a = a - b;
}

ccl_device_inline float4 operator*=(float4 &a, const float4 b)
{
  return a = a * b;
}

ccl_device_inline float4 operator*=(float4 &a, const float f)
{
  return a = a * f;
}

ccl_device_inline float4 operator/=(float4 &a, const float4 b)
{
  return a = a / b;
}

ccl_device_inline float4 operator/=(float4 &a, const float f)
{
  return a = a / f;
}

ccl_device_inline int4 operator<(const float4 a, const float4 b)
{
#  ifdef __KERNEL_SSE__
  return int4(_mm_castps_si128(_mm_cmplt_ps(a.m128, b.m128)));
#  else
  return make_int4(a.x < b.x, a.y < b.y, a.z < b.z, a.w < b.w);
#  endif
}

ccl_device_inline int4 operator>=(const float4 a, const float4 b)
{
#  ifdef __KERNEL_SSE__
  return int4(_mm_castps_si128(_mm_cmpge_ps(a.m128, b.m128)));
#  else
  return make_int4(a.x >= b.x, a.y >= b.y, a.z >= b.z, a.w >= b.w);
#  endif
}

ccl_device_inline int4 operator<=(const float4 a, const float4 b)
{
#  ifdef __KERNEL_SSE__
  return int4(_mm_castps_si128(_mm_cmple_ps(a.m128, b.m128)));
#  else
  return make_int4(a.x <= b.x, a.y <= b.y, a.z <= b.z, a.w <= b.w);
#  endif
}

ccl_device_inline bool operator==(const float4 a, const float4 b)
{
#  ifdef __KERNEL_SSE__
  return (_mm_movemask_ps(_mm_cmpeq_ps(a.m128, b.m128)) & 15) == 15;
#  else
  return (a.x == b.x && a.y == b.y && a.z == b.z && a.w == b.w);
#  endif
}

ccl_device_inline float4 operator^(const float4 a, const float4 b)
{
#  ifdef __KERNEL_SSE__
  return float4(_mm_xor_ps(a.m128, b.m128));
#  else
  return make_float4(__uint_as_float(__float_as_uint(a.x) ^ __float_as_uint(b.x)),
                     __uint_as_float(__float_as_uint(a.y) ^ __float_as_uint(b.y)),
                     __uint_as_float(__float_as_uint(a.z) ^ __float_as_uint(b.z)),
                     __uint_as_float(__float_as_uint(a.w) ^ __float_as_uint(b.w)));
#  endif
}

ccl_device_inline float4 min(const float4 a, const float4 b)
{
#  ifdef __KERNEL_SSE__
  return float4(_mm_min_ps(a.m128, b.m128));
#  else
  return make_float4(min(a.x, b.x), min(a.y, b.y), min(a.z, b.z), min(a.w, b.w));
#  endif
}

ccl_device_inline float4 max(const float4 a, const float4 b)
{
#  ifdef __KERNEL_SSE__
  return float4(_mm_max_ps(a.m128, b.m128));
#  else
  return make_float4(max(a.x, b.x), max(a.y, b.y), max(a.z, b.z), max(a.w, b.w));
#  endif
}

ccl_device_inline float4 clamp(const float4 a, const float4 mn, const float4 mx)
{
  return min(max(a, mn), mx);
}
#endif /* !__KERNEL_METAL__*/

ccl_device_inline float4 madd(const float4 a, const float4 b, const float4 c)
{
#ifdef __KERNEL_SSE__
#  ifdef __KERNEL_NEON__
  return float4(vfmaq_f32(c, a, b));
#  elif defined(__KERNEL_AVX2__)
  return float4(_mm_fmadd_ps(a, b, c));
#  else
  return a * b + c;
#  endif
#else
  return a * b + c;
#endif
}

ccl_device_inline float4 msub(const float4 a, const float4 b, const float4 c)
{
#ifdef __KERNEL_SSE__
#  ifdef __KERNEL_NEON__
  return float4(vfmaq_f32(vnegq_f32(c), a, b));
#  elif defined(__KERNEL_AVX2__)
  return float4(_mm_fmsub_ps(a, b, c));
#  else
  return a * b - c;
#  endif
#else
  return a * b - c;
#endif
}

#ifdef __KERNEL_SSE__
template<size_t i0, const size_t i1, const size_t i2, const size_t i3>
__forceinline float4 shuffle(const float4 a)
{
#  ifdef __KERNEL_NEON__
  return float4(shuffle_neon<float32x4_t, i0, i1, i2, i3>(a.m128));
#  else
  return float4(
      _mm_castsi128_ps(_mm_shuffle_epi32(_mm_castps_si128(a), _MM_SHUFFLE(i3, i2, i1, i0))));
#  endif
}

template<> __forceinline float4 shuffle<0, 1, 0, 1>(const float4 a)
{
  return float4(_mm_movelh_ps(a, a));
}

template<> __forceinline float4 shuffle<2, 3, 2, 3>(const float4 a)
{
  return float4(_mm_movehl_ps(a, a));
}

#  ifdef __KERNEL_SSE3__
template<> __forceinline float4 shuffle<0, 0, 2, 2>(const float4 a)
{
  return float4(_mm_moveldup_ps(a));
}

template<> __forceinline float4 shuffle<1, 1, 3, 3>(const float4 a)
{
  return float4(_mm_movehdup_ps(a));
}
#  endif /* __KERNEL_SSE3__ */

template<size_t i0, const size_t i1, const size_t i2, const size_t i3>
__forceinline float4 shuffle(const float4 a, const float4 b)
{
#  ifdef __KERNEL_NEON__
  return float4(shuffle_neon<float32x4_t, i0, i1, i2, i3>(a, b));
#  else
  return float4(_mm_shuffle_ps(a, b, _MM_SHUFFLE(i3, i2, i1, i0)));
#  endif
}

template<size_t i0> __forceinline float4 shuffle(const float4 a)
{
  return shuffle<i0, i0, i0, i0>(a);
}
template<size_t i0> __forceinline float4 shuffle(const float4 a, const float4 b)
{
#  ifdef __KERNEL_NEON__
  return float4(shuffle_neon<float32x4_t, i0, i0, i0, i0>(a, b));
#  else
  return float4(_mm_shuffle_ps(a, b, _MM_SHUFFLE(i0, i0, i0, i0)));
#  endif
}

template<> __forceinline float4 shuffle<0, 1, 0, 1>(const float4 a, const float4 b)
{
  return float4(_mm_movelh_ps(a, b));
}

template<> __forceinline float4 shuffle<2, 3, 2, 3>(const float4 a, const float4 b)
{
  return float4(_mm_movehl_ps(b, a));
}

template<size_t i> __forceinline float extract(const float4 a)
{
  return _mm_cvtss_f32(shuffle<i, i, i, i>(a));
}
template<> __forceinline float extract<0>(const float4 a)
{
  return _mm_cvtss_f32(a);
}
#endif

ccl_device_inline float reduce_add(const float4 a)
{
#if defined(__KERNEL_SSE__)
#  if defined(__KERNEL_NEON__)
  return vaddvq_f32(a);
#  elif defined(__KERNEL_SSE3__)
  float4 h(_mm_hadd_ps(a.m128, a.m128));
  return _mm_cvtss_f32(_mm_hadd_ps(h.m128, h.m128));
#  else
  float4 h(shuffle<1, 0, 3, 2>(a) + a);
  return _mm_cvtss_f32(shuffle<2, 3, 0, 1>(h) + h);
#  endif
#else
  return a.x + a.y + a.z + a.w;
#endif
}

ccl_device_inline float reduce_min(const float4 a)
{
#if defined(__KERNEL_SSE__)
#  if defined(__KERNEL_NEON__)
  return vminvq_f32(a);
#  else
  float4 h = min(shuffle<1, 0, 3, 2>(a), a);
  return _mm_cvtss_f32(min(shuffle<2, 3, 0, 1>(h), h));
#  endif
#else
  return min(min(a.x, a.y), min(a.z, a.w));
#endif
}

ccl_device_inline float reduce_max(const float4 a)
{
#if defined(__KERNEL_SSE__)
#  if defined(__KERNEL_NEON__)
  return vmaxvq_f32(a);
#  else
  float4 h = max(shuffle<1, 0, 3, 2>(a), a);
  return _mm_cvtss_f32(max(shuffle<2, 3, 0, 1>(h), h));
#  endif
#else
  return max(max(a.x, a.y), max(a.z, a.w));
#endif
}

#if !defined(__KERNEL_METAL__)
ccl_device_inline float dot(const float4 a, const float4 b)
{
#  if defined(__KERNEL_SSE42__) && defined(__KERNEL_SSE__)
#    if defined(__KERNEL_NEON__)
  const __m128 t = vmulq_f32(a, b);
  return vaddvq_f32(t);
#    else
  return _mm_cvtss_f32(_mm_dp_ps(a, b, 0xFF));
#    endif
#  else
  return (a.x * b.x + a.y * b.y) + (a.z * b.z + a.w * b.w);
#  endif
}
#endif /* !defined(__KERNEL_METAL__) */

ccl_device_inline float len(const float4 a)
{
  return sqrtf(dot(a, a));
}

ccl_device_inline float len_squared(const float4 a)
{
  return dot(a, a);
}

ccl_device_inline float4 sqr(const float4 a)
{
  return a * a;
}

#if !defined(__KERNEL_METAL__)
ccl_device_inline float distance(const float4 a, const float4 b)
{
  return len(a - b);
}

ccl_device_inline float4 sqrt(const float4 a)
{
#  ifdef __KERNEL_SSE__
  return float4(_mm_sqrt_ps(a.m128));
#  else
  return make_float4(sqrtf(a.x), sqrtf(a.y), sqrtf(a.z), sqrtf(a.w));
#  endif
}

ccl_device_inline float4 cross(const float4 a, const float4 b)
{
#  ifdef __KERNEL_SSE__
  return (shuffle<1, 2, 0, 0>(a) * shuffle<2, 0, 1, 0>(b)) -
         (shuffle<2, 0, 1, 0>(a) * shuffle<1, 2, 0, 0>(b));
#  else
  return make_float4(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x, 0.0f);
#  endif
}

ccl_device_inline bool is_zero(const float4 a)
{
#  ifdef __KERNEL_SSE__
  return a == zero_float4();
#  else
  return (a.x == 0.0f && a.y == 0.0f && a.z == 0.0f && a.w == 0.0f);
#  endif
}

ccl_device_inline float average(const float4 a)
{
  return reduce_add(a) * 0.25f;
}

ccl_device_inline float4 normalize(const float4 a)
{
  return a / len(a);
}

ccl_device_inline float4 safe_normalize(const float4 a)
{
  const float t = len(a);
  return (t != 0.0f) ? a / t : a;
}

ccl_device_inline float4 fabs(const float4 a)
{
#  if defined(__KERNEL_SSE__)
#    if defined(__KERNEL_NEON__)
  return float4(vabsq_f32(a));
#    else
  return float4(_mm_and_ps(a.m128, _mm_castsi128_ps(_mm_set1_epi32(0x7fffffff))));
#    endif
#  else
  return make_float4(fabsf(a.x), fabsf(a.y), fabsf(a.z), fabsf(a.w));
#  endif
}

/* The floating-point remainder of the division operation a / b calculated by this function is
 * exactly the value a - iquot * b, where iquot is a / b with its fractional part truncated.
 *
 * The returned value has the same sign as a and is less than b in magnitude. */
ccl_device_inline float4 fmod(const float4 a, const float b)
{
#  if defined(__KERNEL_NEON__)
  /* Use native Neon instructions.
   * The logic is the same as the SSE code below, but on Apple M2 Ultra this seems to be faster.
   * Possibly due to some runtime checks in _mm_round_ps which do not get properly inlined. */
  const float32x4_t iquot = vrndq_f32(a / b);
  return float4(vsubq_f32(a, vmulq_f32(iquot, vdupq_n_f32(b))));
#  elif defined(__KERNEL_SSE42__) && defined(__KERNEL_SSE__)
  const __m128 iquot = _mm_round_ps(a / b, _MM_FROUND_TRUNC);
  return float4(_mm_sub_ps(a, _mm_mul_ps(iquot, _mm_set1_ps(b))));
#  else
  return make_float4(fmodf(a.x, b), fmodf(a.y, b), fmodf(a.z, b), fmodf(a.w, b));
#  endif
}

ccl_device_inline float4 floor(const float4 a)
{
#  ifdef __KERNEL_SSE__
#    if defined(__KERNEL_NEON__)
  return float4(vrndmq_f32(a));
#    else
  return float4(_mm_floor_ps(a));
#    endif
#  else
  return make_float4(floorf(a.x), floorf(a.y), floorf(a.z), floorf(a.w));
#  endif
}

ccl_device_inline float4 floorfrac(const float4 x, ccl_private int4 *i)
{
#  ifdef __KERNEL_SSE__
  const float4 f = floor(x);
  *i = int4(_mm_cvttps_epi32(f.m128));
  return x - f;
#  else
  float4 r;
  r.x = floorfrac(x.x, &i->x);
  r.y = floorfrac(x.y, &i->y);
  r.z = floorfrac(x.z, &i->z);
  r.w = floorfrac(x.w, &i->w);
  return r;
#  endif
}

ccl_device_inline float4 mix(const float4 a, const float4 b, const float t)
{
  return a + t * (b - a);
}

ccl_device_inline float4 mix(const float4 a, const float4 b, const float4 t)
{
  return a + t * (b - a);
}

ccl_device_inline float4 saturate(const float4 a)
{
  return make_float4(saturatef(a.x), saturatef(a.y), saturatef(a.z), saturatef(a.w));
}

ccl_device_inline float4 exp(const float4 v)
{
  return make_float4(expf(v.x), expf(v.y), expf(v.z), expf(v.z));
}

ccl_device_inline float4 log(const float4 v)
{
  return make_float4(logf(v.x), logf(v.y), logf(v.z), logf(v.z));
}

#endif /* !__KERNEL_METAL__*/

ccl_device_inline bool isequal(const float4 a, const float4 b)
{
#if defined(__KERNEL_METAL__)
  return all(a == b);
#else
  return a == b;
#endif
}

template<class MaskType>
ccl_device_inline float4 select(const MaskType mask, const float4 a, const float4 b)
{
#if defined(__KERNEL_METAL__)
  return metal::select(b, a, bool4(mask));
#elif defined(__KERNEL_SSE__)
#  ifdef __KERNEL_SSE42__
  return float4(_mm_blendv_ps(b.m128, a.m128, _mm_castsi128_ps(mask.m128)));
#  else
  return float4(
      _mm_or_ps(_mm_and_ps(_mm_castsi128_ps(mask), a), _mm_andnot_ps(_mm_castsi128_ps(mask), b)));
#  endif
#else
  return make_float4(
      (mask.x) ? a.x : b.x, (mask.y) ? a.y : b.y, (mask.z) ? a.z : b.z, (mask.w) ? a.w : b.w);
#endif
}

template<class MaskType> ccl_device_inline float4 mask(const MaskType mask, const float4 a)
{
  /* Replace elements of x with zero where mask isn't set. */
  return select(mask, a, zero_float4());
}

#ifndef __KERNEL_GPU__

ccl_device_inline float4 load_float4(const ccl_private float *v)
{
#  ifdef __KERNEL_SSE__
  return float4(_mm_loadu_ps(v));
#  else
  return make_float4(v[0], v[1], v[2], v[3]);
#  endif
}

#endif /* !__KERNEL_GPU__ */

ccl_device_inline float4 safe_divide(const float4 a, const float b)
{
  return (b != 0.0f) ? a / b : zero_float4();
}

ccl_device_inline float4 safe_divide(const float4 a, const float4 b)
{
  return make_float4((b.x != 0.0f) ? a.x / b.x : 0.0f,
                     (b.y != 0.0f) ? a.y / b.y : 0.0f,
                     (b.z != 0.0f) ? a.z / b.z : 0.0f,
                     (b.w != 0.0f) ? a.w / b.w : 0.0f);
}

ccl_device_inline bool isfinite_safe(const float4 v)
{
  return isfinite_safe(v.x) && isfinite_safe(v.y) && isfinite_safe(v.z) && isfinite_safe(v.w);
}

ccl_device_inline float4 ensure_finite(const float4 v)
{
  float4 r = v;
  if (!isfinite_safe(r.x)) {
    r.x = 0.0f;
  }
  if (!isfinite_safe(r.y)) {
    r.y = 0.0f;
  }
  if (!isfinite_safe(r.z)) {
    r.z = 0.0f;
  }
  if (!isfinite_safe(r.w)) {
    r.w = 0.0f;
  }
  return r;
}

/* Consistent name for this would be pow, but HIP compiler crashes in name mangling. */
ccl_device_inline float4 power(const float4 v, const float e)
{
  return make_float4(powf(v.x, e), powf(v.y, e), powf(v.z, e), powf(v.w, e));
}

ccl_device_inline float4 interp(float4 a, float4 b, float t)
{
  return a + t * (b - a);
}

#if !defined(__KERNEL_METAL__) && !defined(__KERNEL_ONEAPI__)
/* Int/Float conversion */
ccl_device_inline int4 __float4_as_int4(const float4 f)
{
#  ifdef __KERNEL_SSE__
  return int4(_mm_castps_si128(f.m128));
#  else
  return make_int4(
      __float_as_int(f.x), __float_as_int(f.y), __float_as_int(f.z), __float_as_int(f.w));
#  endif
}

ccl_device_inline float4 __int4_as_float4(const int4 i)
{
#  ifdef __KERNEL_SSE__
  return float4(_mm_castsi128_ps(i.m128));
#  else
  return make_float4(
      __int_as_float(i.x), __int_as_float(i.y), __int_as_float(i.z), __int_as_float(i.w));
#  endif
}
#endif /* !defined(__KERNEL_METAL__) && !defined(__KERNEL_ONEAPI__) */

ccl_device_inline void copy_v4_v4(ccl_private float *r, const float4 val)
{
  r[0] = val.x;
  r[1] = val.y;
  r[2] = val.z;
  r[3] = val.w;
}

CCL_NAMESPACE_END
