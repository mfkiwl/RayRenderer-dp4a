//
// pch.cpp
// Include the standard header and generate the precompiled header.
//

#include "pch.h"
#if COMMON_ARCH_X86
#   include "3rdParty/libcpuid/libcpuid/libcpuid.h"
//#elif COMMON_OS_LINUX
//#   include <sys/auxv.h>
//#   include <asm/hwcap.h>
#endif

template<size_t N>
std::vector<std::array<uint8_t, N>> GenerateAllPoses() noexcept
{
    std::vector<std::array<uint8_t, N>> all;
    constexpr auto Count = Pow<N, N>();
    all.resize(Count);
    for (size_t j = 0; j < Count; ++j)
    {
        size_t val = j;
        for (size_t i = 0; i < N; ++i)
        {
            all[j][i] = static_cast<uint8_t>(val % N);
            val /= N;
        }
    }
    return all;
}


template<> 
std::vector<std::array<uint8_t, 2>> PosesHolder<2>::Poses = GenerateAllPoses<2>();
template<>
std::vector<std::array<uint8_t, 4>> PosesHolder<4>::Poses = GenerateAllPoses<4>();
template<>
std::vector<std::array<uint8_t, 8>> PosesHolder<8>::Poses = GenerateAllPoses<8>();


std::mt19937& GetRanEng()
{
    static std::mt19937 gen(std::random_device{}());
    return gen;
}
uint32_t GetARand()
{
    return GetRanEng()();
}


alignas(32) const std::array<uint8_t, RandValBytes / 1> RandVals = []()
{
    std::array<uint8_t, RandValBytes / 1> vals = {};
    for (auto& val : vals)
        val = static_cast<uint8_t>(GetARand());
    return vals;
}();
alignas(32) const std::array<float, RandValBytes / 4> RandValsF32 = []()
{
    std::array<float, RandValBytes / 4> vals = {};
    for (auto& val : vals)
        val = static_cast<float>(static_cast<int32_t>(GetARand()));
    return vals;
}();
alignas(32) const std::array<double, RandValBytes / 8> RandValsF64 = []()
{
    std::array<double, RandValBytes / 8> vals = {};
    for (auto& val : vals)
        val = static_cast<double>(static_cast<int32_t>(GetARand()));
    return vals;
}();


static uint32_t GetSIMDLevel_()
{
#if COMMON_ARCH_X86
    if (!cpuid_present())               return 0;
    struct cpu_raw_data_t raw;
    if (cpuid_get_raw_data(&raw) < 0)   return 0;
    struct cpu_id_t data;
    if (cpu_identify(&raw, &data) < 0)  return 0;

    if (data.flags[CPU_FEATURE_AVX512BW] && data.flags[CPU_FEATURE_AVX512DQ] && data.flags[CPU_FEATURE_AVX512VL])
        return 320;
    if (data.flags[CPU_FEATURE_AVX512F] && data.flags[CPU_FEATURE_AVX512CD])
        return 310;
    if (data.flags[CPU_FEATURE_AVX2])   return 200;
    if (data.flags[CPU_FEATURE_FMA3])   return 150;
    if (data.flags[CPU_FEATURE_AVX])    return 100;
    if (data.flags[CPU_FEATURE_SSE4_2]) return 42;
    if (data.flags[CPU_FEATURE_SSE4_1]) return 41;
    if (data.flags[CPU_FEATURE_SSSE3])  return 31;
    if (data.flags[CPU_FEATURE_PNI])    return 30;
    if (data.flags[CPU_FEATURE_SSE2])   return 20;
    if (data.flags[CPU_FEATURE_SSE])    return 10;
    return 0;
#else
# if defined(_M_ARM64) || defined(__aarch64__) || defined(__ARM_ARCH_ISA_A64)
    return 200;
# elif defined(__ARM_ARCH) && __ARM_ARCH >= 8
    return 100;
# elif defined(__ARM_VFPV5__)
    return 50;
# elif defined(__ARM_VFPV4__)
    return 40;
# elif defined(__ARM_VFPV3__)
    return 30;
# elif defined(__ARM_VFPV2__)
    return 20;
# else
    return 10;
# endif
#endif
}

uint32_t SIMDFixture::GetSIMDLevel()
{
    static const uint32_t LEVEL = GetSIMDLevel_();
    return LEVEL;
}

std::string_view SIMDFixture::GetSIMDLevelName(const uint32_t level)
{
#if COMMON_ARCH_X86
    if (level >= 320)
        return "AVX512";
    if (level >= 310)
        return "AVX512";
    if (level >= 200)
        return "AVX2";
    if (level >= 150)
        return "FMA";
    if (level >= 100)
        return "AVX";
    if (level >= 42)
        return "SSE4_2";
    if (level >= 41)
        return "SSE4_1";
    if (level >= 31)
        return "SSSE3";
    if (level >= 30)
        return "SSE3";
    if (level >= 20)
        return "SSE2";
    if (level >= 10)
        return "SSE";
#else
    if (level >= 200)
        return "A64";
    if (level >= 100)
        return "A32";
    if (level >= 50)
        return "NEON_VFPv5";
    if (level >= 40)
        return "NEON_VFPv4";
    if (level >= 30)
        return "NEON_VFPv3";
    if (level >= 20)
        return "NEON_VFPv2";
    if (level >= 10)
        return "NEON";
#endif
    else
        return "NONE";
}


GTEST_DEFAULT_MAIN
