#include "pch.h"

#if COMMON_COMPILER_GCC
#   pragma GCC push_options
#   pragma GCC target("avx2")
#elif COMMON_COMPILER_CLANG
#   pragma clang attribute push (__attribute__((target("avx2"))), apply_to=function)
#endif

namespace avx2
{
using namespace common;
#define COMMON_SIMD_LV 200
#include "common/simd/SIMD.hpp"
#if COMMON_SIMD_LV_ >= 200
#   include "common/simd/SIMD128.hpp"
#   include "common/simd/SIMD256.hpp"
using namespace common::simd;
#   include "SIMDBaseTest.h"
#   include "ShuffleTest.h"


RegisterSIMDBaseTest(I64x4,  200, SWE, SEL, Add, Sub, SatAdd, SatSub, MulLo,              Neg, Abs, Min, Max, SLL, SRL, SRA, And, Or, Xor, AndNot, Not);
RegisterSIMDBaseTest(U64x4,  200, SWE, SEL, Add, Sub, SatAdd, SatSub, MulLo,                   Abs, Min, Max, SLL, SRL, SRA);
RegisterSIMDBaseTest(I32x8,  200, SWE, SEL, Add, Sub, SatAdd, SatSub, MulLo,        MulX, Neg, Abs, Min, Max, SLL, SRL, SRA);
RegisterSIMDBaseTest(U32x8,  200, SWE, SEL, Add, Sub, SatAdd, SatSub, MulLo,        MulX,      Abs, Min, Max, SLL, SRL, SRA);
RegisterSIMDBaseTest(I16x16, 200, SWE, SEL, Add, Sub, SatAdd, SatSub, MulLo, MulHi, MulX, Neg, Abs, Min, Max, SLL, SRL, SRA);
RegisterSIMDBaseTest(U16x16, 200, SWE, SEL, Add, Sub, SatAdd, SatSub, MulLo, MulHi, MulX,      Abs, Min, Max, SLL, SRL, SRA);
RegisterSIMDBaseTest(I8x32,  200,      SEL, Add, Sub, SatAdd, SatSub, MulLo, MulHi, MulX, Neg, Abs, Min, Max, SLL, SRL, SRA);
RegisterSIMDBaseTest(U8x32,  200,      SEL, Add, Sub, SatAdd, SatSub, MulLo, MulHi, MulX,      Abs, Min, Max, SLL, SRL, SRA);


RegisterSIMDCastTest(F32x8,  200, F64x4);
RegisterSIMDCastTest(F64x4,  200, F32x8);
RegisterSIMDCastTest(I64x4,  200, I8x32,        I16x16,         I32x8                                   );
RegisterSIMDCastTest(U64x4,  200,        U8x32,         U16x16,        U32x8                            );
RegisterSIMDCastTest(I32x8,  200, I8x32,        I16x16,                       I64x4, U64x4, F32x8, F64x4);
RegisterSIMDCastTest(U32x8,  200,        U8x32,         U16x16,               I64x4, U64x4, F32x8, F64x4);
RegisterSIMDCastTest(I16x16, 200, I8x32,                        I32x8, U32x8, I64x4, U64x4, F32x8, F64x4);
RegisterSIMDCastTest(U16x16, 200,        U8x32,                 I32x8, U32x8, I64x4, U64x4, F32x8, F64x4);
RegisterSIMDCastTest(I8x32,  200,               I16x16, U16x16, I32x8, U32x8, I64x4, U64x4, F32x8, F64x4);
RegisterSIMDCastTest(U8x32,  200,               I16x16, U16x16, I32x8, U32x8, I64x4, U64x4, F32x8, F64x4);


RegisterSIMDCastModeTest(F32x8,  200, RangeSaturate, I16x16, U16x16, I8x32, U8x32);
RegisterSIMDCastModeTest(I32x8,  200, RangeSaturate, I16x16, U16x16);
RegisterSIMDCastModeTest(U32x8,  200, RangeSaturate, U16x16);
RegisterSIMDCastModeTest(I16x16, 200, RangeSaturate, I8x32, U8x32);
RegisterSIMDCastModeTest(U16x16, 200, RangeSaturate, U8x32);


RegisterSIMDCmpTest(200, I64x4, U64x4, I32x8, U32x8, I16x16, U16x16, I8x32, U8x32)


RegisterSIMDZipTest(200, F64x4, F32x8, I64x4, U64x4, I32x8, U32x8, I16x16, U16x16, I8x32, U8x32)
RegisterSIMDZipLaneTest(200, F64x4, F32x8, I64x4, U64x4, I32x8, U32x8, I16x16, U16x16, I8x32, U8x32)


RegisterSIMDTest(F64x4, 200, shuftest::ShuffleTest<F64x4>);
RegisterSIMDTest(I64x4, 200, shuftest::ShuffleTest<I64x4>);
RegisterSIMDTest(F64x2, 200, shuftest::ShuffleTest<F64x2>);
RegisterSIMDTest(I64x2, 200, shuftest::ShuffleTest<I64x2>);
RegisterSIMDTest(F32x4, 200, shuftest::ShuffleTest<F32x4>);
RegisterSIMDTest(I32x4, 200, shuftest::ShuffleTest<I32x4>);


RegisterSIMDTest(F64x4, 200, shuftest::ShuffleVarTest<F64x4>);
RegisterSIMDTest(I64x4, 200, shuftest::ShuffleVarTest<I64x4>);
RegisterSIMDTest(F32x8, 200, shuftest::ShuffleVarTest<F32x8>);
RegisterSIMDTest(I32x8, 200, shuftest::ShuffleVarTest<I32x8>);
RegisterSIMDTest(F64x2, 200, shuftest::ShuffleVarTest<F64x2>);
RegisterSIMDTest(I64x2, 200, shuftest::ShuffleVarTest<I64x2>);
RegisterSIMDTest(F32x4, 200, shuftest::ShuffleVarTest<F32x4>);
RegisterSIMDTest(I32x4, 200, shuftest::ShuffleVarTest<I32x4>);


#endif
}

#if COMMON_COMPILER_GCC
#   pragma GCC pop_options
#elif COMMON_COMPILER_CLANG
#   pragma clang attribute pop
#endif
