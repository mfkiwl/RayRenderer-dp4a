#include "XCompRely.h"
#include "common/StrParsePack.hpp"
#include "common/ContainerEx.hpp"

#pragma message("Compiling miniBLAS with " STRINGIZE(COMMON_SIMD_INTRIN) )

namespace xcomp
{
using namespace std::string_literals;
using namespace std::string_view_literals;
using common::simd::VecDataInfo;


VTypeInfo ParseVDataType(const std::u32string_view type) noexcept
{
#define GENV(dtype, bit, n, least) VTypeInfo{ common::simd::VecDataInfo::DataTypes::dtype, n, 0, bit, least ? VTypeInfo::TypeFlags::MinBits : VTypeInfo::TypeFlags::Empty }
#define PERPFX(pfx, type, bit, least) \
    U"" STRINGIZE(pfx) ""sv,    GENV(type, bit, 1,  least), \
    U"" STRINGIZE(pfx) "v2"sv,  GENV(type, bit, 2,  least), \
    U"" STRINGIZE(pfx) "v3"sv,  GENV(type, bit, 3,  least), \
    U"" STRINGIZE(pfx) "v4"sv,  GENV(type, bit, 4,  least), \
    U"" STRINGIZE(pfx) "v8"sv,  GENV(type, bit, 8,  least), \
    U"" STRINGIZE(pfx) "v16"sv, GENV(type, bit, 16, least)  \

#define MIN2(tstr, type, bit)                    \
    PERPFX(PPCAT(tstr, bit),  type, bit, false), \
    PERPFX(PPCAT(tstr, bit+), type, bit, true)   \

    constexpr auto VTypeParser = PARSE_PACK(
        MIN2(u, Unsigned, 8),
        MIN2(u, Unsigned, 16),
        MIN2(u, Unsigned, 32),
        MIN2(u, Unsigned, 64),
        MIN2(i, Signed,   8),
        MIN2(i, Signed,   16),
        MIN2(i, Signed,   32),
        MIN2(i, Signed,   64),
        MIN2(f, Float,    16),
        MIN2(f, Float,    32),
        MIN2(f, Float,    64));
    return VTypeParser(type).value_or(VTypeInfo{});

#undef MIN2
#undef PERPFX
#undef GENV
}


#define GENV(type, flag, bit, n) static_cast<uint32_t>(VTypeInfo{ VecDataInfo::DataTypes::type, n, 0, bit, VTypeInfo::TypeFlags::flag })
#define MIN2(s, type, bit, n) \
    { GENV(type, Empty,   bit, n), U"" STRINGIZE(s) ""sv }, { GENV(type, MinBits, bit, n), U"" STRINGIZE(s) "+"sv }
#define PERPFX(pfx, type, bit) \
    MIN2(pfx,             type, bit, 1), \
    MIN2(PPCAT(pfx, v2),  type, bit, 2), \
    MIN2(PPCAT(pfx, v3),  type, bit, 3), \
    MIN2(PPCAT(pfx, v4),  type, bit, 4), \
    MIN2(PPCAT(pfx, v8),  type, bit, 8), \
    MIN2(PPCAT(pfx, v16), type, bit, 16)
static constexpr std::pair<uint32_t, std::u32string_view> VTypeMappings[] =
{
    PERPFX(u8,  Unsigned, 8),
    PERPFX(u16, Unsigned, 16),
    PERPFX(u32, Unsigned, 32),
    PERPFX(u64, Unsigned, 64),
    PERPFX(i8,  Signed,   8),
    PERPFX(i16, Signed,   16),
    PERPFX(i32, Signed,   32),
    PERPFX(i64, Signed,   64),
    PERPFX(f16, Float,    16),
    PERPFX(f32, Float,    32),
    PERPFX(f64, Float,    64),
};
#undef PERPFX
#undef MIN2
#undef GENV
std::u32string_view StringifyVDataType(const VTypeInfo vtype) noexcept
{
    constexpr auto Mapping = common::container::BuildTableStore<VTypeMappings>();
    if (vtype.IsCustomType())
        return U"Custom"sv;
    return Mapping(vtype).value_or(U"Error"sv);
}


}