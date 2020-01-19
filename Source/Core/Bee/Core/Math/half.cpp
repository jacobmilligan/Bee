/*
 *  half.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include "Bee/Core/Math/half.hpp"

//
// f32<->f16 conversion code adapted from:
// Branch-free implementation of half-precision floating point (Mike Acton):
// https://cellperformance.beyond3d.com/articles/2006/07/branchfree_implementation_of_h_1.html
//
// License below.
//
//  --------------------------------------------------------------
//
// Branch-free implementation of half-precision (16 bit) floating point
// Copyright 2006 Mike Acton <macton@gmail.com>
// 
// Permission is hereby granted, free of charge, to any person obtaining a 
// copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense, 
// and/or sell copies of the Software, and to permit persons to whom the 
// Software is furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included 
// in all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE
//
// Half-precision floating point format
// ------------------------------------
//
//   | Field    | Last | First | Note
//   |----------|------|-------|----------
//   | Sign     | 15   | 15    |
//   | Exponent | 14   | 10    | Bias = 15
//   | Mantissa | 9    | 0     |
//
// Features
// --------
//
//  * QNaN + <x>  = QNaN
//  * <x>  + +INF = +INF
//  * <x>  - -INF = -INF
//  * INF  - INF  = SNaN
//  * Denormalized values
//  * Difference of ZEROs is always +ZERO
//  * Sum round with guard + round + sticky bit (grs)
//  * And of course... no branching
// 
// Precision of Sum
// ----------------
//
//  (SUM)        u16 z = half_add( x, y );
//  (DIFFERENCE) u16 z = half_add( x, -y );
//
//     Will have exactly (0 ulps difference) the same result as:
//     (For 32 bit IEEE 784 floating point and same rounding mode)
//
//     union FLOAT_32
//     {
//       float    f32;
//       u32 u32;
//     };
//
//     union FLOAT_32 fx = { .u32 = half_to_float( x ) };
//     union FLOAT_32 fy = { .u32 = half_to_float( y ) };
//     union FLOAT_32 fz = { .f32 = fx.f32 + fy.f32    };
//     u16       z  = float_to_half( fz );
//

namespace bee {


// Load immediate
static inline u32 uint32_li(u32 a)
{
    return (a);
}

// Decrement
static inline u32 uint32_dec(u32 a)
{
    return (a - 1);
}

// Complement
static inline u32 uint32_not( u32 a )
{
    return (~a);
}

// Negate
static inline u32 uint32_neg( u32 a )
{
    return (u32)(-(int32_t)a);
}

// Extend sign
static inline u32 uint32_ext( u32 a )
{
    return (u32)(((int32_t)a)>>31);
}

// And
static inline u32 uint32_and( u32 a, u32 b )
{
    return (a & b);
}

// And with Complement
static inline u32 uint32_andc( u32 a, u32 b )
{
    return (a & ~b);
}

// Or
static inline u32 uint32_or( u32 a, u32 b )
{
    return (a | b);
}

// Shift Right Logical
static inline u32 uint32_srl( u32 a, int sa )
{
    return (a >> sa);
}

// Shift Left Logical
static inline u32 uint32_sll( u32 a, int sa )
{
    return (a << sa);
}

// Add
static inline u32 uint32_add( u32 a, u32 b )
{
    return (a + b);
}

// Subtract
static inline u32 uint32_sub( u32 a, u32 b )
{
    return (a - b);
}

// Select on Sign bit
static inline u32 uint32_sels( u32 test, u32 a, u32 b )
{
    const u32 mask   = uint32_ext( test );
    const u32 sel_a  = uint32_and(  a,     mask  );
    const u32 sel_b  = uint32_andc( b,     mask  );
    const u32 result = uint32_or(   sel_a, sel_b );

    return (result);
}

// Load Immediate
static inline u16 uint16_li( u16 a )
{
    return (a);
}

// Extend sign
static inline u16 uint16_ext( u16 a )
{
    return (u16)(((int16_t)a)>>15);
}

// Negate
static inline u16 uint16_neg( u16 a )
{
    return (u16)(-(int32_t)a);
}

// Complement
static inline u16 uint16_not( u16 a )
{
    return (~a);
}

// Decrement
static inline u16 uint16_dec( u16 a )
{
    return (u16)(a - 1);
}

// Shift Left Logical
static inline u16 uint16_sll( u16 a, int sa )
{
    return (a << sa);
}

// Shift Right Logical
static inline u16 uint16_srl( u16 a, int sa )
{
    return (a >> sa);
}

// Add
static inline u16 uint16_add( u16 a, u16 b )
{
    return (a + b);
}

// Subtract
static inline u16 uint16_sub( u16 a, u16 b )
{
    return (a - b);
}

// And
static inline u16 uint16_and( u16 a, u16 b )
{
    return (a & b);
}

// Or
static inline u16 uint16_or( u16 a, u16 b )
{
    return (a | b);
}

// Exclusive Or
static inline u16 uint16_xor( u16 a, u16 b )
{
    return (a ^ b);
}

// And with Complement
static inline u16 uint16_andc( u16 a, u16 b )
{
    return (a & ~b);
}

// And then Shift Right Logical
static inline u16 uint16_andsrl( u16 a, u16 b, int sa )
{
    return ((a & b) >> sa);
}

// Shift Right Logical then Mask
static inline u16 uint16_srlm( u16 a, int sa, u16 mask )
{
    return ((a >> sa) & mask);
}

// Add then Mask
static inline u16 uint16_addm( u16 a, u16 b, u16 mask )
{
    return ((a + b) & mask);
}


// Select on Sign bit
static inline u16 uint16_sels( u16 test, u16 a, u16 b )
{
    const u16 mask   = uint16_ext( test );
    const u16 sel_a  = uint16_and(  a,     mask  );
    const u16 sel_b  = uint16_andc( b,     mask  );
    const u16 result = uint16_or(   sel_a, sel_b );

    return (result);
}

// Count Leading Zeros
static inline u32 uint32_cntlz( u32 x )
{
#ifdef __GNUC__
    /* On PowerPC, this will map to insn: cntlzw */
    /* On Pentium, this will map to insn: clz    */
    u32 nlz = __builtin_clz( x );
    return (nlz);
#else
    const u32 x0  = uint32_srl(  x,  1 );
  const u32 x1  = uint32_or(   x,  x0 );
  const u32 x2  = uint32_srl(  x1, 2 );
  const u32 x3  = uint32_or(   x1, x2 );
  const u32 x4  = uint32_srl(  x3, 4 );
  const u32 x5  = uint32_or(   x3, x4 );
  const u32 x6  = uint32_srl(  x5, 8 );
  const u32 x7  = uint32_or(   x5, x6 );
  const u32 x8  = uint32_srl(  x7, 16 );
  const u32 x9  = uint32_or(   x7, x8 );
  const u32 xA  = uint32_not(  x9 );
  const u32 xB  = uint32_srl(  xA, 1 );
  const u32 xC  = uint32_and(  xB, 0x55555555 );
  const u32 xD  = uint32_sub(  xA, xC );
  const u32 xE  = uint32_and(  xD, 0x33333333 );
  const u32 xF  = uint32_srl(  xD, 2 );
  const u32 x10 = uint32_and(  xF, 0x33333333 );
  const u32 x11 = uint32_add(  xE, x10 );
  const u32 x12 = uint32_srl(  x11, 4 );
  const u32 x13 = uint32_add(  x11, x12 );
  const u32 x14 = uint32_and(  x13, 0x0f0f0f0f );
  const u32 x15 = uint32_srl(  x14, 8 );
  const u32 x16 = uint32_add(  x14, x15 );
  const u32 x17 = uint32_srl(  x16, 16 );
  const u32 x18 = uint32_add(  x16, x17 );
  const u32 x19 = uint32_and(  x18, 0x0000003f );
  return ( x19 );
#endif
}

// Count Leading Zeros
static inline u16 uint16_cntlz( u16 x )
{
#ifdef __GNUC__
    /* On PowerPC, this will map to insn: cntlzw */
    /* On Pentium, this will map to insn: clz    */
    u32 x32   = uint32_sll( x, 16 );
    u16 nlz       = (u16)__builtin_clz( x32 ); // NOLINT(modernize-use-auto)
    return (nlz);
#else
    const u16 x0  = uint16_srl(  x,  1 );
  const u16 x1  = uint16_or(   x,  x0 );
  const u16 x2  = uint16_srl(  x1, 2 );
  const u16 x3  = uint16_or(   x1, x2 );
  const u16 x4  = uint16_srl(  x3, 4 );
  const u16 x5  = uint16_or(   x3, x4 );
  const u16 x6  = uint16_srl(  x5, 8 );
  const u16 x7  = uint16_or(   x5, x6 );
  const u16 x8  = uint16_not(  x7 );
  const u16 x9  = uint16_srlm( x8, 1, 0x5555 );
  const u16 xA  = uint16_sub(  x8, x9 );
  const u16 xB  = uint16_and(  xA, 0x3333 );
  const u16 xC  = uint16_srlm( xA, 2, 0x3333 );
  const u16 xD  = uint16_add(  xB, xC );
  const u16 xE  = uint16_srl(  xD, 4 );
  const u16 xF  = uint16_addm( xD, xE, 0x0f0f );
  const u16 x10 = uint16_srl(  xF, 8 );
  const u16 x11 = uint16_addm( xF, x10, 0x001f );
  return ( x11 );
#endif
}

u16 half::half_from_float(float f) const
{
    union {
        u32 uval;
        float fval;
    } conv {};

    conv.fval = f;

    const u32 one                        = uint32_li(0x00000001);
    const u32 f_e_mask                   = uint32_li(0x7f800000);
    const u32 f_m_mask                   = uint32_li(0x007fffff);
    const u32 f_s_mask                   = uint32_li(0x80000000);
    const u32 h_e_mask                   = uint32_li(0x00007c00);
    const u32 f_e_pos                    = uint32_li(0x00000017);
    const u32 f_m_round_bit              = uint32_li(0x00001000);
    const u32 h_nan_em_min               = uint32_li(0x00007c01);
    const u32 f_h_s_pos_offset           = uint32_li(0x00000010);
    const u32 f_m_hidden_bit             = uint32_li(0x00800000);
    const u32 f_h_m_pos_offset           = uint32_li(0x0000000d);
    const u32 f_h_bias_offset            = uint32_li(0x38000000);
    const u32 f_m_snan_mask              = uint32_li(0x003fffff);
    const u16 h_snan_mask                = (u16)uint32_li(0x00007e00); // NOLINT(modernize-use-auto)
    const u32 f_e                        = uint32_and( conv.uval, f_e_mask  );
    const u32 f_m                        = uint32_and( conv.uval, f_m_mask  );
    const u32 f_s                        = uint32_and( conv.uval, f_s_mask  );
    const u32 f_e_h_bias                 = uint32_sub( f_e,               f_h_bias_offset );
    const u32 f_e_h_bias_amount          = uint32_srl( f_e_h_bias,        f_e_pos         );
    const u32 f_m_round_mask             = uint32_and( f_m,               f_m_round_bit     );
    const u32 f_m_round_offset           = uint32_sll( f_m_round_mask,    one               );
    const u32 f_m_rounded                = uint32_add( f_m,               f_m_round_offset  );
    const u32 f_m_rounded_overflow       = uint32_and( f_m_rounded,       f_m_hidden_bit    );
    const u32 f_m_denorm_sa              = uint32_sub( one,               f_e_h_bias_amount );
    const u32 f_m_with_hidden            = uint32_or(  f_m_rounded,       f_m_hidden_bit    );
    const u32 f_m_denorm                 = uint32_srl( f_m_with_hidden,   f_m_denorm_sa     );
    const u32 f_em_norm_packed           = uint32_or(  f_e_h_bias,        f_m_rounded       );
    const u32 f_e_overflow               = uint32_add( f_e_h_bias,        f_m_hidden_bit    );
    const u32 h_s                        = uint32_srl( f_s,               f_h_s_pos_offset );
    const u32 h_m_nan                    = uint32_srl( f_m,               f_h_m_pos_offset );
    const u32 h_m_denorm                 = uint32_srl( f_m_denorm,        f_h_m_pos_offset );
    const u32 h_em_norm                  = uint32_srl( f_em_norm_packed,  f_h_m_pos_offset );
    const u32 h_em_overflow              = uint32_srl( f_e_overflow,      f_h_m_pos_offset );
    const u32 is_e_eqz_msb               = uint32_dec(f_e);
    const u32 is_m_nez_msb               = uint32_neg(  f_m     );
    const u32 is_h_m_nan_nez_msb         = uint32_neg(  h_m_nan );
    const u32 is_e_nflagged_msb          = uint32_sub(  f_e,                 f_e_mask          );
    const u32 is_ninf_msb                = uint32_or(   is_e_nflagged_msb,   is_m_nez_msb      );
    const u32 is_underflow_msb           = uint32_sub(  is_e_eqz_msb,        f_h_bias_offset   );
    const u32 is_nan_nunderflow_msb      = uint32_or(   is_h_m_nan_nez_msb,  is_e_nflagged_msb );
    const u32 is_m_snan_msb              = uint32_sub(  f_m_snan_mask,       f_m               );
    const u32 is_snan_msb                = uint32_andc( is_m_snan_msb,       is_e_nflagged_msb );
    const u32 is_overflow_msb            = uint32_neg(  f_m_rounded_overflow );
    const u32 h_nan_underflow_result     = uint32_sels( is_nan_nunderflow_msb, h_em_norm,                h_nan_em_min       );
    const u32 h_inf_result               = uint32_sels( is_ninf_msb,           h_nan_underflow_result,   h_e_mask           );
    const u32 h_underflow_result         = uint32_sels( is_underflow_msb,      h_m_denorm,               h_inf_result       );
    const u32 h_overflow_result          = uint32_sels( is_overflow_msb,       h_em_overflow,            h_underflow_result );
    const u32 h_em_result                = uint32_sels( is_snan_msb,           h_snan_mask,              h_overflow_result  );
    const u32 h_result                   = uint32_or( h_em_result, h_s );

    return (u16)(h_result);
}

float half::half_to_float(u16 h) const
{
    const u32 h_e_mask              = uint32_li(0x00007c00);
    const u32 h_m_mask              = uint32_li(0x000003ff);
    const u32 h_s_mask              = uint32_li(0x00008000);
    const u32 h_f_s_pos_offset      = uint32_li(0x00000010);
    const u32 h_f_e_pos_offset      = uint32_li(0x0000000d);
    const u32 h_f_bias_offset       = uint32_li(0x0001c000);
    const u32 f_e_mask              = uint32_li(0x7f800000);
    const u32 f_m_mask              = uint32_li(0x007fffff);
    const u32 h_f_e_denorm_bias     = uint32_li(0x0000007e);
    const u32 h_f_m_denorm_sa_bias  = uint32_li(0x00000008);
    const u32 f_e_pos               = uint32_li(0x00000017);
    const u32 h_e_mask_minus_one    = uint32_li(0x00007bff);
    const u32 h_e                   = uint32_and( h, h_e_mask );
    const u32 h_m                   = uint32_and( h, h_m_mask );
    const u32 h_s                   = uint32_and( h, h_s_mask );
    const u32 h_e_f_bias            = uint32_add( h_e, h_f_bias_offset );
    const u32 h_m_nlz               = uint32_cntlz( h_m );
    const u32 f_s                   = uint32_sll( h_s,        h_f_s_pos_offset );
    const u32 f_e                   = uint32_sll( h_e_f_bias, h_f_e_pos_offset );
    const u32 f_m                   = uint32_sll( h_m,        h_f_e_pos_offset );
    const u32 f_em                  = uint32_or(  f_e,        f_m              );
    const u32 h_f_m_sa              = uint32_sub( h_m_nlz,             h_f_m_denorm_sa_bias );
    const u32 f_e_denorm_unpacked   = uint32_sub( h_f_e_denorm_bias,   h_f_m_sa             );
    const u32 h_f_m                 = uint32_sll( h_m,                 h_f_m_sa             );
    const u32 f_m_denorm            = uint32_and( h_f_m,               f_m_mask             );
    const u32 f_e_denorm            = uint32_sll( f_e_denorm_unpacked, f_e_pos              );
    const u32 f_em_denorm           = uint32_or(  f_e_denorm,          f_m_denorm           );
    const u32 f_em_nan              = uint32_or(  f_e_mask,            f_m                  );
    const u32 is_e_eqz_msb          = uint32_dec(h_e);
    const u32 is_m_nez_msb          = uint32_neg(  h_m );
    const u32 is_e_flagged_msb      = uint32_sub(  h_e_mask_minus_one, h_e );
    const u32 is_zero_msb           = uint32_andc( is_e_eqz_msb,       is_m_nez_msb );
    const u32 is_inf_msb            = uint32_andc( is_e_flagged_msb,   is_m_nez_msb );
    const u32 is_denorm_msb         = uint32_and(  is_m_nez_msb,       is_e_eqz_msb );
    const u32 is_nan_msb            = uint32_and(  is_e_flagged_msb,   is_m_nez_msb );
    const u32 is_zero               = uint32_ext(  is_zero_msb );
    const u32 f_zero_result         = uint32_andc( f_em, is_zero );
    const u32 f_denorm_result       = uint32_sels( is_denorm_msb, f_em_denorm, f_zero_result );
    const u32 f_inf_result          = uint32_sels( is_inf_msb,    f_e_mask,    f_denorm_result );
    const u32 f_nan_result          = uint32_sels( is_nan_msb,    f_em_nan,    f_inf_result    );
    const u32 f_result              = uint32_or( f_s, f_nan_result );

    union {
        u32 uval;
        float fval;
    } conv {};
    conv.uval = f_result;

    return conv.fval;
}

u16 half::half_add(const u16 x, const u16 y) const
{
    const u16 one                       = uint16_li( 0x0001 );
    const u16 msb_to_lsb_sa             = uint16_li( 0x000f );
    const u16 h_s_mask                  = uint16_li( 0x8000 );
    const u16 h_e_mask                  = uint16_li( 0x7c00 );
    const u16 h_m_mask                  = uint16_li( 0x03ff );
    const u16 h_m_msb_mask              = uint16_li( 0x2000 );
    const u16 h_m_msb_sa                = uint16_li( 0x000d );
    const u16 h_m_hidden                = uint16_li( 0x0400 );
    const u16 h_e_pos                   = uint16_li( 0x000a );
    const u16 h_e_bias_minus_one        = uint16_li( 0x000e );
    const u16 h_m_grs_carry             = uint16_li( 0x4000 );
    const u16 h_m_grs_carry_pos         = uint16_li( 0x000e );
    const u16 h_grs_size                = uint16_li( 0x0003 );
    const u16 h_snan                    = uint16_li( 0xfe00 );
    const u16 h_e_mask_minus_one        = uint16_li( 0x7bff );
    const u16 h_grs_round_carry         = uint16_sll( one, h_grs_size );
    const u16 h_grs_round_mask          = uint16_sub( h_grs_round_carry, one );
    const u16 x_e                       = uint16_and( x, h_e_mask );
    const u16 y_e                       = uint16_and( y, h_e_mask );
    const u16 is_y_e_larger_msb         = uint16_sub( x_e, y_e );
    const u16 a                         = uint16_sels( is_y_e_larger_msb, y, x);
    const u16 a_s                       = uint16_and( a, h_s_mask );
    const u16 a_e                       = uint16_and( a, h_e_mask );
    const u16 a_m_no_hidden_bit         = uint16_and( a, h_m_mask );
    const u16 a_em_no_hidden_bit        = uint16_or( a_e, a_m_no_hidden_bit );
    const u16 b                         = uint16_sels( is_y_e_larger_msb, x, y);
    const u16 b_s                       = uint16_and( b, h_s_mask );
    const u16 b_e                       = uint16_and( b, h_e_mask );
    const u16 b_m_no_hidden_bit         = uint16_and( b, h_m_mask );
    const u16 b_em_no_hidden_bit        = uint16_or( b_e, b_m_no_hidden_bit );
    const u16 is_diff_sign_msb          = uint16_xor( a_s, b_s );
    const u16 is_a_inf_msb              = uint16_sub( h_e_mask_minus_one, a_em_no_hidden_bit );
    const u16 is_b_inf_msb              = uint16_sub( h_e_mask_minus_one, b_em_no_hidden_bit );
    const u16 is_undenorm_msb           = uint16_dec( a_e );
    const u16 is_undenorm               = uint16_ext( is_undenorm_msb );
    const u16 is_both_inf_msb           = uint16_and( is_a_inf_msb, is_b_inf_msb );
    const u16 is_invalid_inf_op_msb     = uint16_and( is_both_inf_msb, b_s );
    const u16 is_a_e_nez_msb            = uint16_neg( a_e );
    const u16 is_b_e_nez_msb            = uint16_neg( b_e );
    const u16 is_a_e_nez                = uint16_ext( is_a_e_nez_msb );
    const u16 is_b_e_nez                = uint16_ext( is_b_e_nez_msb );
    const u16 a_m_hidden_bit            = uint16_and( is_a_e_nez, h_m_hidden );
    const u16 b_m_hidden_bit            = uint16_and( is_b_e_nez, h_m_hidden );
    const u16 a_m_no_grs                = uint16_or( a_m_no_hidden_bit, a_m_hidden_bit );
    const u16 b_m_no_grs                = uint16_or( b_m_no_hidden_bit, b_m_hidden_bit );
    const u16 diff_e                    = uint16_sub( a_e,        b_e );
    const u16 a_e_unbias                = uint16_sub( a_e,        h_e_bias_minus_one );
    const u16 a_m                       = uint16_sll( a_m_no_grs, h_grs_size );
    const u16 a_e_biased                = uint16_srl( a_e,        h_e_pos );
    const u16 m_sa_unbias               = uint16_srl( a_e_unbias, h_e_pos );
    const u16 m_sa_default              = uint16_srl( diff_e,     h_e_pos );
    const u16 m_sa_unbias_mask          = uint16_andc( is_a_e_nez_msb,   is_b_e_nez_msb );
    const u16 m_sa                      = uint16_sels( m_sa_unbias_mask, m_sa_unbias, m_sa_default );
    const u16 b_m_no_sticky             = uint16_sll( b_m_no_grs,        h_grs_size );
    const u16 sh_m                      = uint16_srl( b_m_no_sticky,     m_sa );
    const u16 sticky_overflow           = uint16_sll( one,               m_sa );
    const u16 sticky_mask               = uint16_dec( sticky_overflow );
    const u16 sticky_collect            = uint16_and( b_m_no_sticky, sticky_mask );
    const u16 is_sticky_set_msb         = uint16_neg( sticky_collect );
    const u16 sticky                    = uint16_srl( is_sticky_set_msb, msb_to_lsb_sa);
    const u16 b_m                       = uint16_or( sh_m, sticky );
    const u16 is_c_m_ab_pos_msb         = uint16_sub( b_m, a_m );
    const u16 c_inf                     = uint16_or( a_s, h_e_mask );
    const u16 c_m_sum                   = uint16_add( a_m, b_m );
    const u16 c_m_diff_ab               = uint16_sub( a_m, b_m );
    const u16 c_m_diff_ba               = uint16_sub( b_m, a_m );
    const u16 c_m_smag_diff             = uint16_sels( is_c_m_ab_pos_msb, c_m_diff_ab, c_m_diff_ba );
    const u16 c_s_diff                  = uint16_sels( is_c_m_ab_pos_msb, a_s,         b_s         );
    const u16 c_s                       = uint16_sels( is_diff_sign_msb,  c_s_diff,    a_s         );
    const u16 c_m_smag_diff_nlz         = uint16_cntlz( c_m_smag_diff );
    const u16 diff_norm_sa              = uint16_sub( c_m_smag_diff_nlz, one );
    const u16 is_diff_denorm_msb        = uint16_sub( a_e_biased, diff_norm_sa );
    const u16 is_diff_denorm            = uint16_ext( is_diff_denorm_msb );
    const u16 is_a_or_b_norm_msb        = uint16_neg( a_e_biased );
    const u16 diff_denorm_sa            = uint16_dec( a_e_biased );
    const u16 c_m_diff_denorm           = uint16_sll( c_m_smag_diff, diff_denorm_sa );
    const u16 c_m_diff_norm             = uint16_sll( c_m_smag_diff, diff_norm_sa );
    const u16 c_e_diff_norm             = uint16_sub( a_e_biased,  diff_norm_sa );
    const u16 c_m_diff_ab_norm          = uint16_sels( is_diff_denorm_msb, c_m_diff_denorm, c_m_diff_norm );
    const u16 c_e_diff_ab_norm          = uint16_andc( c_e_diff_norm, is_diff_denorm );
    const u16 c_m_diff                  = uint16_sels( is_a_or_b_norm_msb, c_m_diff_ab_norm, c_m_smag_diff );
    const u16 c_e_diff                  = uint16_sels( is_a_or_b_norm_msb, c_e_diff_ab_norm, a_e_biased    );
    const u16 is_diff_eqz_msb           = uint16_dec( c_m_diff );
    const u16 is_diff_exactly_zero_msb  = uint16_and( is_diff_sign_msb, is_diff_eqz_msb );
    const u16 is_diff_exactly_zero      = uint16_ext( is_diff_exactly_zero_msb );
    const u16 c_m_added                 = uint16_sels( is_diff_sign_msb, c_m_diff, c_m_sum );
    const u16 c_e_added                 = uint16_sels( is_diff_sign_msb, c_e_diff, a_e_biased );
    const u16 c_m_carry                 = uint16_and( c_m_added, h_m_grs_carry );
    const u16 is_c_m_carry_msb          = uint16_neg( c_m_carry );
    const u16 c_e_hidden_offset         = uint16_andsrl( c_m_added, h_m_grs_carry, h_m_grs_carry_pos );
    const u16 c_m_sub_hidden            = uint16_srl( c_m_added, one );
    const u16 c_m_no_hidden             = uint16_sels( is_c_m_carry_msb, c_m_sub_hidden, c_m_added );
    const u16 c_e_no_hidden             = uint16_add( c_e_added,         c_e_hidden_offset  );
    const u16 c_m_no_hidden_msb         = uint16_and( c_m_no_hidden,     h_m_msb_mask       );
    const u16 undenorm_m_msb_odd        = uint16_srl( c_m_no_hidden_msb, h_m_msb_sa         );
    const u16 undenorm_fix_e            = uint16_and( is_undenorm,       undenorm_m_msb_odd );
    const u16 c_e_fixed                 = uint16_add( c_e_no_hidden,     undenorm_fix_e     );
    const u16 c_m_round_amount          = uint16_and( c_m_no_hidden,     h_grs_round_mask   );
    const u16 c_m_rounded               = uint16_add( c_m_no_hidden,     c_m_round_amount   );
    const u16 c_m_round_overflow        = uint16_andsrl( c_m_rounded, h_m_grs_carry, h_m_grs_carry_pos );
    const u16 c_e_rounded               = uint16_add( c_e_fixed, c_m_round_overflow );
    const u16 c_m_no_grs                = uint16_srlm( c_m_rounded, h_grs_size,  h_m_mask );
    const u16 c_e                       = uint16_sll( c_e_rounded, h_e_pos );
    const u16 c_em                      = uint16_or( c_e, c_m_no_grs );
    const u16 c_normal                  = uint16_or( c_s, c_em );
    const u16 c_inf_result              = uint16_sels( is_a_inf_msb, c_inf, c_normal );
    const u16 c_zero_result             = uint16_andc( c_inf_result, is_diff_exactly_zero );
    const u16 c_result                  = uint16_sels( is_invalid_inf_op_msb, h_snan, c_zero_result );

    return (c_result);
}


} // namespace bee