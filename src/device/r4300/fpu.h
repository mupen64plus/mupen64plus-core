/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - fpu.h                                                   *
 *   Mupen64Plus homepage: https://mupen64plus.org/                        *
 *   Copyright (C) 2010 Ari64                                              *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.          *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#ifndef M64P_DEVICE_R4300_FPU_H
#define M64P_DEVICE_R4300_FPU_H

#include <math.h>
#include <stdint.h>

#ifdef _MSC_VER
#define M64P_FPU_INLINE static __inline
#include <float.h>
#include <fenv.h>

static __inline double round(double x) { return floor(x + 0.5); }
static __inline float  roundf(float x) { return (float)floor(x + 0.5); }
static __inline double trunc(double x) { return (double)(int)x; }
static __inline float  truncf(float x) { return (float)(int)x; }
#if !defined(isnan)
  #define isnan _isnan
#endif

#else
#define M64P_FPU_INLINE static inline
#include <fenv.h>
#endif

#define FCR31_CMP_BIT UINT32_C(0x800000)

#define FCR31_CAUSE_BITS UINT32_C(0x01F000)

#define FCR31_CAUSE_INEXACT_BIT   UINT32_C(0x001000)
#define FCR31_CAUSE_UNDERFLOW_BIT UINT32_C(0x002000)
#define FCR31_CAUSE_OVERFLOW_BIT  UINT32_C(0x004000)
#define FCR31_CAUSE_DIVBYZERO_BIT UINT32_C(0x008000)
#define FCR31_CAUSE_INVALIDOP_BIT UINT32_C(0x010000)
#define FCR31_CAUSE_UNIMPLOP_BIT  UINT32_C(0x020000)

#define FCR31_ENABLE_INEXACT_BIT   UINT32_C(0x000080)
#define FCR31_ENABLE_UNDERFLOW_BIT UINT32_C(0x000100)
#define FCR31_ENABLE_OVERFLOW_BIT  UINT32_C(0x000200)
#define FCR31_ENABLE_DIVBYZERO_BIT UINT32_C(0x000400)
#define FCR31_ENABLE_INVALIDOP_BIT UINT32_C(0x000800)

#define FCR31_FLAG_INEXACT_BIT   UINT32_C(0x000004)
#define FCR31_FLAG_UNDERFLOW_BIT UINT32_C(0x000008)
#define FCR31_FLAG_OVERFLOW_BIT  UINT32_C(0x000010)
#define FCR31_FLAG_DIVBYZERO_BIT UINT32_C(0x000020)
#define FCR31_FLAG_INVALIDOP_BIT UINT32_C(0x000040)


M64P_FPU_INLINE void set_rounding(uint32_t fcr31)
{
    switch(fcr31 & 3) {
    case 0: /* Round to nearest, or to even if equidistant */
        fesetround(FE_TONEAREST);
        break;
    case 1: /* Truncate (toward 0) */
        fesetround(FE_TOWARDZERO);
        break;
    case 2: /* Round up (toward +Inf) */
        fesetround(FE_UPWARD);
        break;
    case 3: /* Round down (toward -Inf) */
        fesetround(FE_DOWNWARD);
        break;
    }
}

#ifdef ACCURATE_FPU_BEHAVIOR
M64P_FPU_INLINE void fpu_reset_cause(uint32_t* fcr31)
{
    (*fcr31) &= ~FCR31_CAUSE_BITS;
}

M64P_FPU_INLINE void fpu_reset_exceptions()
{
    feclearexcept(FE_ALL_EXCEPT);
}

M64P_FPU_INLINE int fpu_check_exceptions(uint32_t* fcr31)
{
    int fexceptions;

    fexceptions = fetestexcept(FE_ALL_EXCEPT) & FE_ALL_EXCEPT;

    if (fexceptions & FE_DIVBYZERO)
    {
        (*fcr31) |= FCR31_CAUSE_DIVBYZERO_BIT;
        (*fcr31) |= FCR31_FLAG_DIVBYZERO_BIT;
    }
    if (fexceptions & FE_INEXACT)
    {
        (*fcr31) |= FCR31_CAUSE_INEXACT_BIT;
        (*fcr31) |= FCR31_FLAG_INEXACT_BIT;
    }
    if (fexceptions & FE_UNDERFLOW)
    {
        (*fcr31) |= FCR31_CAUSE_UNDERFLOW_BIT;
        (*fcr31) |= FCR31_FLAG_UNDERFLOW_BIT;
    }
    if (fexceptions & FE_OVERFLOW)
    {
        (*fcr31) |= FCR31_CAUSE_OVERFLOW_BIT;
        (*fcr31) |= FCR31_FLAG_OVERFLOW_BIT;
    }
    if (fexceptions & FE_INVALID)
    {
        (*fcr31) |= FCR31_CAUSE_INVALIDOP_BIT;
        (*fcr31) |= FCR31_FLAG_INVALIDOP_BIT;
    }

    return 0; // TODO: exceptions
}

M64P_FPU_INLINE void fpu_check_input_float(uint32_t* fcr31, const float* value)
{
    switch (fpclassify(*value))
    {
    default:
    case FP_SUBNORMAL: // TODO
        return;
    case FP_NAN:
        (*fcr31) |= FCR31_CAUSE_INVALIDOP_BIT;
        (*fcr31) |= FCR31_FLAG_INVALIDOP_BIT;
        break;
    }
}

M64P_FPU_INLINE void fpu_check_input_double(uint32_t* fcr31, const double* value)
{
    switch (fpclassify(*value))
    {
    default:
    case FP_SUBNORMAL: // TODO
        return;
    case FP_NAN:
        (*fcr31) |= FCR31_CAUSE_INVALIDOP_BIT;
        (*fcr31) |= FCR31_FLAG_INVALIDOP_BIT;
        break;
    }
}

M64P_FPU_INLINE void fpu_check_output_float(uint32_t* fcr31, const float* value)
{
    switch (fpclassify(*value))
    {
    case FP_SUBNORMAL:
        (*fcr31) |= FCR31_CAUSE_UNDERFLOW_BIT;
        (*fcr31) |= FCR31_FLAG_UNDERFLOW_BIT;
        (*fcr31) |= FCR31_CAUSE_INEXACT_BIT;
        (*fcr31) |= FCR31_FLAG_INEXACT_BIT;
        break;

    case FP_NAN:
        break;

    default:
        break;
    }
}

M64P_FPU_INLINE void fpu_check_output_double(uint32_t* fcr31, const double* value)
{
    switch (fpclassify(*value))
    {
    case FP_SUBNORMAL:
        (*fcr31) |= FCR31_CAUSE_UNDERFLOW_BIT;
        (*fcr31) |= FCR31_FLAG_UNDERFLOW_BIT;
        (*fcr31) |= FCR31_CAUSE_INEXACT_BIT;
        (*fcr31) |= FCR31_FLAG_INEXACT_BIT;
        break;

    case FP_NAN:
        break;

    default:
        break;
    }
}
#else
M64P_FPU_INLINE void fpu_reset_cause(uint32_t* fcr31)
{
}

M64P_FPU_INLINE void fpu_reset_exceptions()
{
}

M64P_FPU_INLINE int fpu_check_exceptions(uint32_t* fcr31)
{
    return 0;
}

M64P_FPU_INLINE void fpu_check_input_float(uint32_t* fcr31, const float* value)
{
}

M64P_FPU_INLINE void fpu_check_input_double(uint32_t* fcr31, const double* value)
{
}

M64P_FPU_INLINE void fpu_check_output_float(uint32_t* fcr31, const float* value)
{
}

M64P_FPU_INLINE void fpu_check_output_double(uint32_t* fcr31, const double* value)
{
}
#endif /* ACCURATE_FPU_BEHAVIOR */

M64P_FPU_INLINE void cvt_s_w(uint32_t* fcr31, const int32_t* source, float* dest)
{
    set_rounding(*fcr31);

    fpu_reset_cause(fcr31);
    fpu_reset_exceptions();

    *dest = (float)*source;

    fpu_check_exceptions(fcr31);
    fpu_check_output_float(fcr31, dest);
}
M64P_FPU_INLINE void cvt_d_w(uint32_t* fcr31, const int32_t* source, double* dest)
{
    fpu_reset_cause(fcr31);
    fpu_reset_exceptions();

    *dest = (double)*source;

    fpu_check_exceptions(fcr31);
    fpu_check_output_double(fcr31, dest);
}
M64P_FPU_INLINE void cvt_s_l(uint32_t* fcr31, const int64_t* source, float* dest)
{
    set_rounding(*fcr31);

    fpu_reset_cause(fcr31);
    fpu_reset_exceptions();

    *dest = (float)*source;

    fpu_check_exceptions(fcr31);
    fpu_check_output_float(fcr31, dest);
}
M64P_FPU_INLINE void cvt_d_l(uint32_t* fcr31, const int64_t* source, double* dest)
{
    set_rounding(*fcr31);

    fpu_reset_cause(fcr31);
    fpu_reset_exceptions();

    *dest = (double)*source;

    fpu_check_exceptions(fcr31);
    fpu_check_output_double(fcr31, dest);
}
M64P_FPU_INLINE void cvt_d_s(uint32_t* fcr31, const float* source, double* dest)
{
    fpu_reset_cause(fcr31);
    fpu_check_input_float(fcr31, source);

    fpu_reset_exceptions();

    *dest = (double)*source;

    fpu_check_exceptions(fcr31);
    fpu_check_output_double(fcr31, dest);
}
M64P_FPU_INLINE void cvt_s_d(uint32_t* fcr31, const double* source, float* dest)
{
    set_rounding(*fcr31);

    fpu_reset_cause(fcr31);
    fpu_check_input_double(fcr31, source);

    fpu_reset_exceptions();

    *dest = (float)*source;

    fpu_check_exceptions(fcr31);
    fpu_check_output_float(fcr31, dest);
}

M64P_FPU_INLINE void round_l_s(const float* source, int64_t* dest)
{
    float remainder = *source - floorf(*source);
    if (remainder == 0.5)
    {
        if (*source < 0)
        {
            *dest = (int64_t)truncf(*source) % 2 != 0 ? (int64_t)floorf(*source) : (int64_t) ceilf(*source);
        }
        else
        {
            *dest = (int64_t)truncf(*source) % 2 != 0 ? (int64_t)ceilf(*source) : (int64_t)floorf(*source);
        }
    }
    else
    {
        *dest = (int64_t)roundf(*source);
    }
}
M64P_FPU_INLINE void round_w_s(const float* source, int32_t* dest)
{
    float remainder = *source - floorf(*source);
    if (remainder == 0.5)
    {
        if (*source < 0)
        {
            *dest = (int32_t)truncf(*source) % 2 != 0 ? (int32_t)floorf(*source) : (int32_t)ceilf(*source);
        }
        else
        {
            *dest = (int32_t)truncf(*source) % 2 != 0 ? (int32_t)ceilf(*source) : (int32_t)floorf(*source);
        }
    }
    else
    {
        *dest = (int32_t)roundf(*source);
    }
}
M64P_FPU_INLINE void trunc_l_s(const float* source, int64_t* dest)
{
    *dest = (int64_t)truncf(*source);
}
M64P_FPU_INLINE void trunc_w_s(const float* source, int32_t* dest)
{
    *dest = (int32_t)truncf(*source);
}
M64P_FPU_INLINE void ceil_l_s(const float* source, int64_t* dest)
{
    *dest = (int64_t)ceilf(*source);
}
M64P_FPU_INLINE void ceil_w_s(const float* source, int32_t* dest)
{
    *dest = (int32_t)ceilf(*source);
}
M64P_FPU_INLINE void floor_l_s(const float* source, int64_t* dest)
{
    *dest = (int64_t)floorf(*source);
}
M64P_FPU_INLINE void floor_w_s(const float* source, int32_t* dest)
{
    *dest = (int32_t)floorf(*source);
}

M64P_FPU_INLINE void round_l_d(const double* source, int64_t* dest)
{
    double remainder = *source - floor(*source);
    if (remainder == 0.5)
    {
        if (*source < 0)
        {
            *dest = (int64_t)trunc(*source) % 2 != 0 ? (int64_t)floor(*source) : (int64_t)ceil(*source);
        }
        else
        {
            *dest = (int64_t)trunc(*source) % 2 != 0 ? (int64_t)ceil(*source) : (int64_t)floor(*source);
        }
    }
    else
    {
        *dest = (int64_t)round(*source);
    }
}
M64P_FPU_INLINE void round_w_d(const double* source, int32_t* dest)
{
    double remainder = *source - floor(*source);
    if (remainder == 0.5)
    {
        if (*source < 0)
        {
            *dest = (int32_t)trunc(*source) % 2 != 0 ? (int32_t)floor(*source) : (int32_t)ceil(*source);
        }
        else
        {
            *dest = (int32_t)trunc(*source) % 2 != 0 ? (int32_t)ceil(*source) : (int32_t)floor(*source);
        }
    }
    else
    {
        *dest = (int32_t)round(*source);
    }

}
M64P_FPU_INLINE void trunc_l_d(const double* source, int64_t* dest)
{
    *dest = (int64_t)trunc(*source);
}
M64P_FPU_INLINE void trunc_w_d(const double* source, int32_t* dest)
{
    *dest = (int32_t)trunc(*source);
}
M64P_FPU_INLINE void ceil_l_d(const double* source, int64_t* dest)
{
    *dest = (int64_t)ceil(*source);
}
M64P_FPU_INLINE void ceil_w_d(const double* source, int32_t* dest)
{
    *dest = (int32_t)ceil(*source);
}
M64P_FPU_INLINE void floor_l_d(const double* source, int64_t* dest)
{
    *dest = (int64_t)floor(*source);
}
M64P_FPU_INLINE void floor_w_d(const double* source, int32_t* dest)
{
    *dest = (int32_t)floor(*source);
}

M64P_FPU_INLINE void cvt_w_s(uint32_t* fcr31, const float* source, int32_t* dest)
{
    fpu_reset_cause(fcr31);

    fpu_check_input_float(fcr31, source);

    fpu_reset_exceptions();

    switch(*fcr31 & 3)
    {
    case 0: round_w_s(source, dest); return;
    case 1: trunc_w_s(source, dest); return;
    case 2: ceil_w_s (source, dest); return;
    case 3: floor_w_s(source, dest); return;
    }

    fpu_check_exceptions(fcr31);
}
M64P_FPU_INLINE void cvt_w_d(uint32_t* fcr31, const double* source, int32_t* dest)
{
    fpu_reset_cause(fcr31);

    fpu_check_input_double(fcr31, source);

    fpu_reset_exceptions();

    switch(*fcr31 & 3)
    {
    case 0: round_w_d(source, dest); return;
    case 1: trunc_w_d(source, dest); return;
    case 2: ceil_w_d (source, dest); return;
    case 3: floor_w_d(source, dest); return;
    }

    fpu_check_exceptions(fcr31);
}
M64P_FPU_INLINE void cvt_l_s(uint32_t* fcr31, const float* source, int64_t* dest)
{
    fpu_reset_cause(fcr31);
    fpu_reset_exceptions();

    switch(*fcr31 & 3)
    {
    case 0: round_l_s(source, dest); return;
    case 1: trunc_l_s(source, dest); return;
    case 2: ceil_l_s (source, dest); return;
    case 3: floor_l_s(source, dest); return;
    }

    fpu_check_exceptions(fcr31);
}
M64P_FPU_INLINE void cvt_l_d(uint32_t* fcr31, const double* source, int64_t* dest)
{
    fpu_reset_cause(fcr31);
    fpu_reset_exceptions();

    switch(*fcr31 & 3)
    {
    case 0: round_l_d(source, dest); return;
    case 1: trunc_l_d(source, dest); return;
    case 2: ceil_l_d (source, dest); return;
    case 3: floor_l_d(source, dest); return;
    }

    fpu_check_exceptions(fcr31);
}

M64P_FPU_INLINE void c_f_s(uint32_t* fcr31, const float* source, const float* target)
{
    fpu_reset_cause(fcr31);

    if ((isnan(*source) || isnan(*target)))
    {
        (*fcr31) &= ~FCR31_CMP_BIT;
        (*fcr31) |= FCR31_CAUSE_INVALIDOP_BIT;
        (*fcr31) |= FCR31_FLAG_INVALIDOP_BIT;
        return;
    }

    *fcr31 &= ~FCR31_CMP_BIT;
}
M64P_FPU_INLINE void c_un_s(uint32_t* fcr31, const float* source, const float* target)
{
    fpu_reset_cause(fcr31);

    if ((isnan(*source) || isnan(*target)))
    {
        (*fcr31) |= FCR31_CMP_BIT;
        return;
    }

    *fcr31 = (isnan(*source) || isnan(*target))
        ? (*fcr31 |  FCR31_CMP_BIT)
        : (*fcr31 & ~FCR31_CMP_BIT);
}

M64P_FPU_INLINE void c_eq_s(uint32_t* fcr31, const float* source, const float* target)
{
    fpu_reset_cause(fcr31);

    if (isnan(*source) || isnan(*target))
    { 
        (*fcr31) &= ~FCR31_CMP_BIT;
        return;
    }

    *fcr31 = (*source == *target)
        ? (*fcr31 |  FCR31_CMP_BIT)
        : (*fcr31 & ~FCR31_CMP_BIT);
}
M64P_FPU_INLINE void c_ueq_s(uint32_t* fcr31, const float* source, const float* target)
{
    fpu_reset_cause(fcr31);

    if (isnan(*source) || isnan(*target))
    { 
        (*fcr31) |= FCR31_CMP_BIT; 
        return; 
    }

    *fcr31 = (*source == *target)
        ? (*fcr31 |  FCR31_CMP_BIT)
        : (*fcr31 & ~FCR31_CMP_BIT);
}

M64P_FPU_INLINE void c_olt_s(uint32_t* fcr31, const float* source, const float* target)
{
    fpu_reset_cause(fcr31);

    if (isnan(*source) || isnan(*target))
    { 
        (*fcr31) &= ~FCR31_CMP_BIT;
        return;
    }

    *fcr31 = (*source < *target)
        ? (*fcr31 |  FCR31_CMP_BIT)
        : (*fcr31 & ~FCR31_CMP_BIT);
}
M64P_FPU_INLINE void c_ult_s(uint32_t* fcr31, const float* source, const float* target)
{
    fpu_reset_cause(fcr31);

    if (isnan(*source) || isnan(*target))
    { 
        (*fcr31) |= FCR31_CMP_BIT; 
        return; 
    }

    *fcr31 = (*source < *target)
        ? (*fcr31 |  FCR31_CMP_BIT)
        : (*fcr31 & ~FCR31_CMP_BIT);
}

M64P_FPU_INLINE void c_ole_s(uint32_t* fcr31, const float* source, const float* target)
{
    fpu_reset_cause(fcr31);

    if (isnan(*source) || isnan(*target))
    { 
        (*fcr31) &= ~FCR31_CMP_BIT;
        return;
    }

    *fcr31 = (*source <= *target)
        ? (*fcr31 |  FCR31_CMP_BIT)
        : (*fcr31 & ~FCR31_CMP_BIT);
}
M64P_FPU_INLINE void c_ule_s(uint32_t* fcr31, const float* source, const float* target)
{
    fpu_reset_cause(fcr31);

    if (isnan(*source) || isnan(*target))
    { 
        (*fcr31) |= FCR31_CMP_BIT; 
        return; 
    }

    *fcr31 = (*source <= *target)
        ? (*fcr31 |  FCR31_CMP_BIT)
        : (*fcr31 &~ FCR31_CMP_BIT);
}

M64P_FPU_INLINE void c_sf_s(uint32_t* fcr31, const float* source, const float* target)
{
    fpu_reset_cause(fcr31);

    if (isnan(*source) || isnan(*target))
    {
        (*fcr31) &= ~FCR31_CMP_BIT;
        (*fcr31) |= FCR31_CAUSE_INVALIDOP_BIT;
        (*fcr31) |= FCR31_FLAG_INVALIDOP_BIT;
        return;
    }

    *fcr31 &= ~FCR31_CMP_BIT;
}
M64P_FPU_INLINE void c_ngle_s(uint32_t* fcr31, const float* source, const float* target)
{
    fpu_reset_cause(fcr31);

    if (isnan(*source) || isnan(*target))
    {
        (*fcr31) |= FCR31_CMP_BIT;
        (*fcr31) |= FCR31_CAUSE_INVALIDOP_BIT;
        (*fcr31) |= FCR31_FLAG_INVALIDOP_BIT;
        return;
    }

    *fcr31 &= ~FCR31_CMP_BIT;
}

M64P_FPU_INLINE void c_seq_s(uint32_t* fcr31, const float* source, const float* target)
{
    fpu_reset_cause(fcr31);

    if (isnan(*source) || isnan(*target))
    {
        (*fcr31) &= ~FCR31_CMP_BIT;
        (*fcr31) |= FCR31_CAUSE_INVALIDOP_BIT;
        (*fcr31) |= FCR31_FLAG_INVALIDOP_BIT;
        return;
    }

    *fcr31 = (*source == *target)
        ? (*fcr31 |  FCR31_CMP_BIT)
        : (*fcr31 &~ FCR31_CMP_BIT);
}
M64P_FPU_INLINE void c_ngl_s(uint32_t* fcr31, const float* source, const float* target)
{
    fpu_reset_cause(fcr31);

    if (isnan(*source) || isnan(*target))
    {
        (*fcr31) |= FCR31_CMP_BIT;
        (*fcr31) |= FCR31_CAUSE_INVALIDOP_BIT;
        (*fcr31) |= FCR31_FLAG_INVALIDOP_BIT;
        return;
    }

    *fcr31 = (*source == *target)
        ? (*fcr31 |  FCR31_CMP_BIT)
        : (*fcr31 & ~FCR31_CMP_BIT);
}

M64P_FPU_INLINE void c_lt_s(uint32_t* fcr31, const float* source, const float* target)
{
    fpu_reset_cause(fcr31);

    if (isnan(*source) || isnan(*target))
    {
        (*fcr31) &= ~FCR31_CMP_BIT;
        (*fcr31) |= FCR31_CAUSE_INVALIDOP_BIT;
        (*fcr31) |= FCR31_FLAG_INVALIDOP_BIT;
        return;
    }

    *fcr31 = (*source < *target)
        ? (*fcr31 |  FCR31_CMP_BIT)
        : (*fcr31 & ~FCR31_CMP_BIT);
}
M64P_FPU_INLINE void c_nge_s(uint32_t* fcr31, const float* source, const float* target)
{
    fpu_reset_cause(fcr31);

    if (isnan(*source) || isnan(*target))
    {
        (*fcr31) |= FCR31_CMP_BIT;
        (*fcr31) |= FCR31_CAUSE_INVALIDOP_BIT;
        (*fcr31) |= FCR31_FLAG_INVALIDOP_BIT;
        return;
    }

    *fcr31 = (*source < *target)
        ? (*fcr31 |  FCR31_CMP_BIT)
        : (*fcr31 & ~FCR31_CMP_BIT);
}

M64P_FPU_INLINE void c_le_s(uint32_t* fcr31, const float* source, const float* target)
{
    fpu_reset_cause(fcr31);

    if (isnan(*source) || isnan(*target))
    {
        (*fcr31) &= ~FCR31_CMP_BIT;
        (*fcr31) |= FCR31_CAUSE_INVALIDOP_BIT;
        (*fcr31) |= FCR31_FLAG_INVALIDOP_BIT;
        return;
    }

    *fcr31 = (*source <= *target)
        ? (*fcr31 |  FCR31_CMP_BIT)
        : (*fcr31 & ~FCR31_CMP_BIT);
}
M64P_FPU_INLINE void c_ngt_s(uint32_t* fcr31, const float* source, const float* target)
{
    fpu_reset_cause(fcr31);

    if (isnan(*source) || isnan(*target))
    {
        (*fcr31) |= FCR31_CMP_BIT;
        (*fcr31) |= FCR31_CAUSE_INVALIDOP_BIT;
        (*fcr31) |= FCR31_FLAG_INVALIDOP_BIT;
        return;
    }

    *fcr31 = (*source <= *target)
        ? (*fcr31 |  FCR31_CMP_BIT)
        : (*fcr31 & ~FCR31_CMP_BIT);
}

M64P_FPU_INLINE void c_f_d(uint32_t* fcr31)
{
    fpu_reset_cause(fcr31);
    
    *fcr31 &= ~FCR31_CMP_BIT;
}
M64P_FPU_INLINE void c_un_d(uint32_t* fcr31, const double* source, const double* target)
{
    fpu_reset_cause(fcr31);

    if (isnan(*source) || isnan(*target))
    { 
        (*fcr31) &= ~FCR31_CMP_BIT;
        return;
    }

    *fcr31 = (isnan(*source) || isnan(*target))
        ? (*fcr31 |  FCR31_CMP_BIT)
        : (*fcr31 & ~FCR31_CMP_BIT);
}

M64P_FPU_INLINE void c_eq_d(uint32_t* fcr31, const double* source, const double* target)
{
    fpu_reset_cause(fcr31);

    if (isnan(*source) || isnan(*target))
    { 
        (*fcr31) &= ~FCR31_CMP_BIT;
        return;
    }

    *fcr31 = (*source == *target)
        ? (*fcr31 |  FCR31_CMP_BIT)
        : (*fcr31 & ~FCR31_CMP_BIT);
}
M64P_FPU_INLINE void c_ueq_d(uint32_t* fcr31, const double* source, const double* target)
{
    fpu_reset_cause(fcr31);

    if (isnan(*source) || isnan(*target))
    {
        (*fcr31) |= FCR31_CMP_BIT; 
        return;
    }

    *fcr31 = (*source == *target)
        ? (*fcr31 |  FCR31_CMP_BIT)
        : (*fcr31 & ~FCR31_CMP_BIT);
}

M64P_FPU_INLINE void c_olt_d(uint32_t* fcr31, const double* source, const double* target)
{
    fpu_reset_cause(fcr31);

    if (isnan(*source) || isnan(*target))
    { 
        (*fcr31) &= ~FCR31_CMP_BIT;
        return;
    }

    *fcr31 = (*source < *target)
        ? (*fcr31 |  FCR31_CMP_BIT)
        : (*fcr31 & ~FCR31_CMP_BIT);
}
M64P_FPU_INLINE void c_ult_d(uint32_t* fcr31, const double* source, const double* target)
{
    fpu_reset_cause(fcr31);

    if (isnan(*source) || isnan(*target))
    { 
        (*fcr31) |= FCR31_CMP_BIT; 
        return; 
    }

    *fcr31 = (*source < *target)
        ? (*fcr31 |  FCR31_CMP_BIT)
        : (*fcr31 & ~FCR31_CMP_BIT);
}

M64P_FPU_INLINE void c_ole_d(uint32_t* fcr31, const double* source, const double* target)
{
    fpu_reset_cause(fcr31);

    if (isnan(*source) || isnan(*target))
    { 
        (*fcr31) &= ~FCR31_CMP_BIT;
        return;
    }

    *fcr31 = (*source <= *target)
        ? (*fcr31 |  FCR31_CMP_BIT)
        : (*fcr31 & ~FCR31_CMP_BIT);
}
M64P_FPU_INLINE void c_ule_d(uint32_t* fcr31, const double* source, const double* target)
{
    fpu_reset_cause(fcr31);

    if (isnan(*source) || isnan(*target))
    { 
        (*fcr31) |= FCR31_CMP_BIT; 
        return; 
    }

    *fcr31 = (*source <= *target)
        ? (*fcr31 |  FCR31_CMP_BIT)
        : (*fcr31 & ~FCR31_CMP_BIT);
}

M64P_FPU_INLINE void c_sf_d(uint32_t* fcr31, const double* source, const double* target)
{
    fpu_reset_cause(fcr31);

    if (isnan(*source) || isnan(*target))
    {
        (*fcr31) &= ~FCR31_CMP_BIT;
        (*fcr31) |= FCR31_CAUSE_INVALIDOP_BIT;
        (*fcr31) |= FCR31_FLAG_INVALIDOP_BIT;
        return;
    }

    *fcr31 &= ~FCR31_CMP_BIT;
}
M64P_FPU_INLINE void c_ngle_d(uint32_t* fcr31, const double* source, const double* target)
{
    fpu_reset_cause(fcr31);

    if (isnan(*source) || isnan(*target))
    {
        (*fcr31) |= FCR31_CMP_BIT;
        (*fcr31) |= FCR31_CAUSE_INVALIDOP_BIT;
        (*fcr31) |= FCR31_FLAG_INVALIDOP_BIT;
        return;
    }

    *fcr31 &= ~FCR31_CMP_BIT;
}

M64P_FPU_INLINE void c_seq_d(uint32_t* fcr31, const double* source, const double* target)
{
    fpu_reset_cause(fcr31);

    if (isnan(*source) || isnan(*target))
    {
        (*fcr31) &= ~FCR31_CMP_BIT;
        (*fcr31) |= FCR31_CAUSE_INVALIDOP_BIT;
        (*fcr31) |= FCR31_FLAG_INVALIDOP_BIT;
        return;
    }

    *fcr31 = (*source == *target)
        ? (*fcr31 |  FCR31_CMP_BIT)
        : (*fcr31 & ~FCR31_CMP_BIT);
}
M64P_FPU_INLINE void c_ngl_d(uint32_t* fcr31, const double* source, const double* target)
{
    fpu_reset_cause(fcr31);

    if (isnan(*source) || isnan(*target))
    {
        (*fcr31) |= FCR31_CMP_BIT;
        (*fcr31) |= FCR31_CAUSE_INVALIDOP_BIT;
        (*fcr31) |= FCR31_FLAG_INVALIDOP_BIT;
        return;
    }

    *fcr31 = (*source == *target)
        ? (*fcr31 |  FCR31_CMP_BIT)
        : (*fcr31 & ~FCR31_CMP_BIT);
}

M64P_FPU_INLINE void c_lt_d(uint32_t* fcr31, const double* source, const double* target)
{
    fpu_reset_cause(fcr31);

    if (isnan(*source) || isnan(*target))
    {
        (*fcr31) &= ~FCR31_CMP_BIT;
        (*fcr31) |= FCR31_CAUSE_INVALIDOP_BIT;
        (*fcr31) |= FCR31_FLAG_INVALIDOP_BIT;
        return;
    }

    *fcr31 = (*source < *target)
        ? (*fcr31 |  FCR31_CMP_BIT)
        : (*fcr31 & ~FCR31_CMP_BIT);
}
M64P_FPU_INLINE void c_nge_d(uint32_t* fcr31, const double* source, const double* target)
{
    fpu_reset_cause(fcr31);

    if (isnan(*source) || isnan(*target))
    {
        (*fcr31) |= FCR31_CMP_BIT;
        (*fcr31) |= FCR31_CAUSE_INVALIDOP_BIT;
        (*fcr31) |= FCR31_FLAG_INVALIDOP_BIT;
        return;
    }

    *fcr31 = (*source < *target)
        ? (*fcr31 |  FCR31_CMP_BIT)
        : (*fcr31 & ~FCR31_CMP_BIT);
}

M64P_FPU_INLINE void c_le_d(uint32_t* fcr31, const double* source, const double* target)
{
    fpu_reset_cause(fcr31);

    if (isnan(*source) || isnan(*target))
    {
        (*fcr31) &= ~FCR31_CMP_BIT;
        (*fcr31) |= FCR31_CAUSE_INVALIDOP_BIT;
        (*fcr31) |= FCR31_FLAG_INVALIDOP_BIT;
        return;
    }

    *fcr31 = (*source <= *target)
        ? (*fcr31 |  FCR31_CMP_BIT)
        : (*fcr31 & ~FCR31_CMP_BIT);
}
M64P_FPU_INLINE void c_ngt_d(uint32_t* fcr31, const double* source, const double* target)
{
    fpu_reset_cause(fcr31);

    if (isnan(*source) || isnan(*target))
    {
        (*fcr31) |= FCR31_CMP_BIT;
        (*fcr31) |= FCR31_CAUSE_INVALIDOP_BIT;
        (*fcr31) |= FCR31_FLAG_INVALIDOP_BIT;
        return;
    }

    *fcr31 = (*source <= *target)
        ? (*fcr31 |  FCR31_CMP_BIT)
        : (*fcr31 & ~FCR31_CMP_BIT);
}


M64P_FPU_INLINE void add_s(uint32_t* fcr31, const float* source1, const float* source2, float* target)
{
    set_rounding(*fcr31);

    fpu_reset_cause(fcr31);
    fpu_check_input_float(fcr31, source1);
    fpu_check_input_float(fcr31, source2);

    fpu_reset_exceptions();

    *target = *source1 + *source2;

    fpu_check_exceptions(fcr31);

    fpu_check_output_float(fcr31, target);
}
M64P_FPU_INLINE void sub_s(uint32_t* fcr31, const float* source1, const float* source2, float* target)
{
    set_rounding(*fcr31);

    fpu_reset_cause(fcr31);
    fpu_check_input_float(fcr31, source1);
    fpu_check_input_float(fcr31, source2);

    fpu_reset_exceptions();

    *target = *source1 - *source2;

    fpu_check_exceptions(fcr31);

    fpu_check_output_float(fcr31, target);
}
M64P_FPU_INLINE void mul_s(uint32_t* fcr31, const float* source1, const float* source2, float* target)
{
    set_rounding(*fcr31);

    fpu_reset_cause(fcr31);
    fpu_check_input_float(fcr31, source1);
    fpu_check_input_float(fcr31, source2);

    fpu_reset_exceptions();

    *target = *source1 * *source2;

    fpu_check_exceptions(fcr31);

    fpu_check_output_float(fcr31, target);
}
M64P_FPU_INLINE void div_s(uint32_t* fcr31, const float* source1, const float* source2, float* target)
{
    set_rounding(*fcr31);

    fpu_reset_cause(fcr31);
    fpu_check_input_float(fcr31, source1);
    fpu_check_input_float(fcr31, source2);

    fpu_reset_exceptions();

    *target = *source1 / *source2;

    fpu_check_exceptions(fcr31);

    fpu_check_output_float(fcr31, target);
}
M64P_FPU_INLINE void sqrt_s(uint32_t* fcr31, const float* source, float* target)
{
    set_rounding(*fcr31);

    fpu_reset_cause(fcr31);
    fpu_check_input_float(fcr31, source);

    fpu_reset_exceptions();

    *target = sqrtf(*source);

    fpu_check_exceptions(fcr31);

    fpu_check_output_float(fcr31, target);
}
M64P_FPU_INLINE void abs_s(uint32_t* fcr31, const float* source, float* target)
{
    fpu_reset_cause(fcr31);
    fpu_check_input_float(fcr31, source);

    *target = fabsf(*source);

    fpu_check_output_float(fcr31, target);
}
M64P_FPU_INLINE void mov_s(const float* source, float* target)
{
    *target = *source;
}
M64P_FPU_INLINE void neg_s(uint32_t* fcr31, const float* source, float* target)
{
    fpu_reset_cause(fcr31);
    fpu_check_input_float(fcr31, source);

    *target = - *source;

    fpu_check_output_float(fcr31, target);
}
M64P_FPU_INLINE void add_d(uint32_t* fcr31, const double* source1, const double* source2, double* target)
{
    set_rounding(*fcr31);

    fpu_reset_cause(fcr31);
    fpu_check_input_double(fcr31, source1);
    fpu_check_input_double(fcr31, source2);

    fpu_reset_exceptions();

    *target = *source1 + *source2;

    fpu_check_exceptions(fcr31);

    fpu_check_output_double(fcr31, target);
}
M64P_FPU_INLINE void sub_d(uint32_t* fcr31, const double* source1, const double* source2, double* target)
{
    set_rounding(*fcr31);

    fpu_reset_cause(fcr31);
    fpu_check_input_double(fcr31, source1);
    fpu_check_input_double(fcr31, source2);

    fpu_reset_exceptions();

    *target = *source1 - *source2;

    fpu_check_exceptions(fcr31);

    fpu_check_output_double(fcr31, target);
}
M64P_FPU_INLINE void mul_d(uint32_t* fcr31, const double* source1, const double* source2, double* target)
{
    set_rounding(*fcr31);

    fpu_reset_cause(fcr31);
    fpu_check_input_double(fcr31, source1);
    fpu_check_input_double(fcr31, source2);

    fpu_reset_exceptions();

    *target = *source1 * *source2;

    fpu_check_exceptions(fcr31);

    fpu_check_output_double(fcr31, target);
}
M64P_FPU_INLINE void div_d(uint32_t* fcr31, const double* source1, const double* source2, double* target)
{
    set_rounding(*fcr31);

    fpu_reset_cause(fcr31);
    fpu_check_input_double(fcr31, source1);
    fpu_check_input_double(fcr31, source2);

    fpu_reset_exceptions();

    *target = *source1 / *source2;

    fpu_check_exceptions(fcr31);

    fpu_check_output_double(fcr31, target);
}
M64P_FPU_INLINE void sqrt_d(uint32_t* fcr31, const double* source, double* target)
{
    set_rounding(*fcr31);


    fpu_reset_cause(fcr31);
    fpu_check_input_double(fcr31, source);

    fpu_reset_exceptions();

    *target = sqrt(*source);

    fpu_check_exceptions(fcr31);

    fpu_check_output_double(fcr31, target);
}
M64P_FPU_INLINE void abs_d(uint32_t* fcr31, const double* source, double* target)
{
    fpu_reset_cause(fcr31);
    fpu_check_input_double(fcr31, source);

    fpu_reset_exceptions();

    *target = fabs(*source);

    fpu_check_exceptions(fcr31);

    fpu_check_output_double(fcr31, target);
}
M64P_FPU_INLINE void mov_d(const double* source, double* target)
{
    *target = *source;
}
M64P_FPU_INLINE void neg_d(uint32_t* fcr31, const double* source, double* target)
{
    fpu_reset_cause(fcr31);
    fpu_check_input_double(fcr31, source);

    *target = - *source;

    fpu_check_output_double(fcr31, target);
}

#endif /* M64P_DEVICE_R4300_FPU_H */
