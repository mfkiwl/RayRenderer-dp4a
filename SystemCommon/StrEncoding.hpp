#pragma once

#include "common/CommonRely.hpp"
#include "common/StrBase.hpp"
#include <cstring>
#include <algorithm>


namespace common::str::charset
{


namespace detail
{
inline constexpr auto InvalidChar = static_cast<char32_t>(-1);
inline constexpr auto InvalidCharPair = std::pair<char32_t, uint32_t>{ InvalidChar, 0 };

//struct ConvertCPBase
//{
//    using ElementType = char;
//    static constexpr std::pair<char32_t, uint32_t> From(const ElementType* __restrict const, const size_t) noexcept
//    {
//        return InvalidCharPair;
//    }
//    static constexpr uint8_t To(const char32_t src, const size_t size, ElementType* __restrict const dest) noexcept
//    {
//        return 0;
//    }
//};
//struct ConvertByteBase
//{
//    static constexpr std::pair<char32_t, uint32_t> FromBytes(const uint8_t* __restrict const src, const size_t size) noexcept
//    {
//        return InvalidCharPair;
//    }
//    static constexpr uint8_t ToBytes(const char32_t src, const size_t size, uint8_t* __restrict const dest) noexcept
//    {
//        return 0;
//    }
//};

struct ConvertCPBase 
{ };
struct ConvertByteBase 
{ };

struct UTF7 : public ConvertCPBase, public ConvertByteBase
{
    using ElementType = char;
    static constexpr size_t MaxOutputUnit = 1;
    [[nodiscard]] inline static constexpr std::pair<char32_t, uint32_t> From(const ElementType* __restrict const src, const size_t size) noexcept
    {
        if (size >= 1 && src[0] > 0)
            return { src[0], 1 };
        return InvalidCharPair;
    }
    [[nodiscard]] inline static constexpr uint8_t To(const char32_t src, const size_t size, ElementType* __restrict const dest) noexcept
    {
        if (size >= 1 && src < 128)
        {
            dest[0] = char(src);
            return 1;
        }
        return 0;
    }
    [[nodiscard]] inline static constexpr std::pair<char32_t, uint32_t> FromBytes(const uint8_t* __restrict const src, const size_t size) noexcept
    {
        if (size >= 1 && src[0] > 0)
            return { src[0], 1 };
        return InvalidCharPair;
    }
    [[nodiscard]] inline static constexpr uint8_t ToBytes(const char32_t src, const size_t size, uint8_t* __restrict const dest) noexcept
    {
        if (size >= 1 && src < 128)
        {
            dest[0] = uint8_t(src);
            return 1;
        }
        return 0;
    }
};


struct UTF32 : public ConvertCPBase
{
    using ElementType = char32_t;
    static constexpr size_t MaxOutputUnit = 1;
    [[nodiscard]] inline static constexpr std::pair<char32_t, uint32_t> From(const ElementType* __restrict const src, const size_t size) noexcept
    {
        if (size >= 1u && src[0] < 0x200000u)
        {
            return { src[0], 1 };
        }
        return InvalidCharPair;
    }
    [[nodiscard]] inline static constexpr uint8_t To(const char32_t src, const size_t size, ElementType* __restrict const dest) noexcept
    {
        if (size >= 1u && src < 0x200000u)
        {
            dest[0] = src;
            return 1;
        }
        return 0;
    }
};
struct UTF32LE : public UTF32, public ConvertByteBase
{
    [[nodiscard]] inline static constexpr std::pair<char32_t, uint32_t> FromBytes(const uint8_t* __restrict const src, const size_t size) noexcept
    {
        if (size >= 4u)
        {
            const uint32_t ch = src[0] | (src[1] << 8) | (src[2] << 16) | (src[3] << 24);
            if (ch < 0x200000u)
                return { ch, 4 };
        }
        return InvalidCharPair;
    }
    [[nodiscard]] inline static constexpr uint8_t ToBytes(const char32_t src, const size_t size, uint8_t* __restrict const dest) noexcept
    {
        if (size >= 4u && src < 0x200000u)
        {
            dest[0] = static_cast<uint8_t>(src);       dest[1] = static_cast<uint8_t>(src >> 8);
            dest[2] = static_cast<uint8_t>(src >> 16); dest[3] = static_cast<uint8_t>(src >> 24);
            return 4;
        }
        return 0;
    }
};
struct UTF32BE : public UTF32, public ConvertByteBase
{
    [[nodiscard]] inline static constexpr std::pair<char32_t, uint32_t> FromBytes(const uint8_t* __restrict const src, const size_t size) noexcept
    {
        if (size >= 4u)
        {
            const uint32_t ch = src[3] | (src[2] << 8) | (src[1] << 16) | (src[0] << 24);
            if (ch < 0x200000u)
                return { ch, 4 };
        }
        return InvalidCharPair;
    }
    [[nodiscard]] inline static constexpr uint8_t ToBytes(const char32_t src, const size_t size, uint8_t* __restrict const dest) noexcept
    {
        if (size >= 4u && src < 0x200000u)
        {
            dest[3] = static_cast<uint8_t>(src);       dest[2] = static_cast<uint8_t>(src >> 8);
            dest[1] = static_cast<uint8_t>(src >> 16); dest[0] = static_cast<uint8_t>(src >> 24);
            return 4;
        }
        return 0;
    }
};

struct UTF8 : public ConvertCPBase, public ConvertByteBase
{
private:
    template<typename T>
    [[nodiscard]] inline static constexpr std::pair<char32_t, uint32_t> InnerFrom(const T* __restrict const src, const size_t size) noexcept
    {
        if (size == 0)
            return InvalidCharPair;
        if ((uint32_t)src[0] < 0x80u)//1 byte
            return { src[0], 1 };
        else if ((src[0] & 0xe0u) == 0xc0u)//2 byte
        {
            if (size >= 2 && (src[1] & 0xc0u) == 0x80u)
                return { ((src[0] & 0x1fu) << 6) | (src[1] & 0x3fu), 2 };
        }
        else if ((src[0] & 0xf0u) == 0xe0u)//3 byte
        {
            if (size >= 3 && ((src[1] & 0xc0u) == 0x80u) && ((src[2] & 0xc0u) == 0x80u))
                return { ((src[0] & 0x0fu) << 12) | ((src[1] & 0x3fu) << 6) | (src[2] & 0x3fu), 3 };
        }
        else if ((src[0] & 0xf8u) == 0xf0u)//4 byte
        {
            if (size >= 4 && ((src[1] & 0xc0u) == 0x80u) && ((src[2] & 0xc0u) == 0x80u) && ((src[3] & 0xc0u) == 0x80u))
                return { ((src[0] & 0x0fu) << 18) | ((src[1] & 0x3fu) << 12) | ((src[2] & 0x3fu) << 6) | (src[3] & 0x3fu), 4 };
        }
        return InvalidCharPair;
    }
    template<typename T>
    [[nodiscard]] inline static constexpr uint8_t InnerTo(const char32_t src, const size_t size, T* __restrict const dest) noexcept
    {
        if (src < 0x80)//1 byte
        {
            if (size >= 1)
            {
                dest[0] = (T)(src & 0x7f);
                return 1;
            }
        }
        else if (src < 0x800)//2 byte
        {
            if (size >= 2)
            {
                dest[0] = T(0b11000000 | ((src >> 6) & 0x3f)), dest[1] = T(0x80 | (src & 0x3f));
                return 2;
            }
        }
        else if (src < 0x10000)//3 byte
        {
            if (size >= 3)
            {
                dest[0] = T(0b11100000 | (src >> 12)), dest[1] = T(0x80 | ((src >> 6) & 0x3f)), dest[2] = T(0x80 | (src & 0x3f));
                return 3;
            }
        }
        else if (src < 0x200000)//4 byte
        {
            if (size >= 4)
            {
                dest[0] = T(0b11110000 | (src >> 18)), dest[1] = T(0x80 | ((src >> 12) & 0x3f));
                dest[2] = T(0x80 | ((src >> 6) & 0x3f)), dest[3] = T(0x80 | (src & 0x3f));
                return 4;
            }
        }
        return 0;
    }
public:
    using ElementType = u8ch_t;
    static constexpr size_t MaxOutputUnit = 4;
    [[nodiscard]] forceinline static constexpr std::pair<char32_t, uint32_t> From(const ElementType* __restrict const src, const size_t size) noexcept
    {
        return InnerFrom(src, size);
    }
    [[nodiscard]] forceinline static constexpr uint8_t To(const char32_t src, const size_t size, ElementType* __restrict const dest) noexcept
    {
        return InnerTo(src, size, dest);
    }
    [[nodiscard]] forceinline static constexpr std::pair<char32_t, uint32_t> FromBytes(const uint8_t* __restrict const src, const size_t size) noexcept
    {
        return InnerFrom(src, size);
    }
    [[nodiscard]] forceinline static constexpr uint8_t ToBytes(const char32_t src, const size_t size, uint8_t* __restrict const dest) noexcept
    {
        return InnerTo(src, size, dest);
    }
};

struct URI : public ConvertCPBase, public ConvertByteBase
{
private:
    inline static constexpr uint8_t ParseChar(uint8_t ch) noexcept
    {
        if (ch >= '0' && ch <= '9') return ch - '0';
        if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
        if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
        return 0xff;
    }
    template<typename T>
    [[nodiscard]] inline static constexpr std::pair<char32_t, uint32_t> InnerFrom(const T* __restrict const src, const size_t size) noexcept
    {
        if (size == 0)
            return InvalidCharPair;
        const auto ch0 = static_cast<uint8_t>(src[0]);
        if (ch0 >= 0x80u || ch0 <= 0x20u) // should be encoded
            return InvalidCharPair;
        if (src[0] != '%') // direct output
            return { ch0, 1 };
        // need decode
        if (size < 3)
            return InvalidCharPair;
        const auto val0Hi = ParseChar(src[1]), val0Lo = ParseChar(src[2]);
        if (val0Hi == 0xff || val0Lo == 0xff)
            return InvalidCharPair;
        const uint32_t val0 = (val0Hi << 4) + val0Lo;
        if (val0 < 0x80u) // 1 byte
            return { val0, 3 };
        else if ((val0 & 0xe0u) == 0xc0u) // 2 byte
        {
            if (size >= 6)
            {
                const auto val1Hi = ParseChar(src[4]), val1Lo = ParseChar(src[5]);
                if (src[3] == '%' && val1Hi != 0xff && val1Lo != 0xff)
                {
                    const uint32_t val1 = (val1Hi << 4) + val1Lo;
                    if ((val1 & 0xc0u) == 0x80u)
                        return { ((val0 & 0x1fu) << 6) | (val1 & 0x3fu), 6 };
                }
            }
        }
        else if ((val0 & 0xf0u) == 0xe0u) // 3 byte
        {
            if (size >= 9)
            {
                const auto val1Hi = ParseChar(src[4]), val1Lo = ParseChar(src[5]);
                const auto val2Hi = ParseChar(src[7]), val2Lo = ParseChar(src[8]);
                if (src[3] == '%' && src[6] == '%' && val1Hi != 0xff && val1Lo != 0xff && val2Hi != 0xff && val2Lo != 0xff)
                {
                    const uint32_t val1 = (val1Hi << 4) + val1Lo;
                    const uint32_t val2 = (val2Hi << 4) + val2Lo;
                    if ((val1 & 0xc0u) == 0x80u && (val2 & 0xc0u) == 0x80u)
                        return { ((val0 & 0x0fu) << 12) | ((val1 & 0x3fu) << 6) | (val2 & 0x3fu), 9 };
                }
            }
        }
        else if ((val0 & 0xf8u) == 0xf0u) // 4 byte
        {
            if (size >= 12)
            {
                const auto val1Hi = ParseChar(src[ 4]), val1Lo = ParseChar(src[ 5]);
                const auto val2Hi = ParseChar(src[ 7]), val2Lo = ParseChar(src[ 8]);
                const auto val3Hi = ParseChar(src[10]), val3Lo = ParseChar(src[11]);
                if (src[3] == '%' && src[6] == '%' && src[9] == '%' && val1Hi != 0xff && val1Lo != 0xff && 
                    val2Hi != 0xff && val2Lo != 0xff && val3Hi != 0xff && val3Lo != 0xff)
                {
                    const uint32_t val1 = (val1Hi << 4) + val1Lo;
                    const uint32_t val2 = (val2Hi << 4) + val2Lo;
                    const uint32_t val3 = (val3Hi << 4) + val3Lo;
                    if ((val1 & 0xc0u) == 0x80u && (val2 & 0xc0u) == 0x80u && (val3 & 0xc0u) == 0x80u)
                        return { ((val0 & 0x0fu) << 18) | ((val1 & 0x3fu) << 12) | ((val2 & 0x3fu) << 6) | (val3 & 0x3fu), 12 };
                }
            }
        }
        return InvalidCharPair;
    }
    template<typename T>
    [[nodiscard]] inline static constexpr uint8_t InnerTo(const char32_t src, const size_t size, T* __restrict const dest) noexcept
    {
        constexpr char Hex16[] = "0123456789ABCDEF";
        if (src < 0x80)
        {
            if (src > 0x20u && src != '%') // direct output
            {
                if (size >= 1)
                {
                    dest[0] = (T)(src & 0x7f);
                    return 1;
                }
            }
            else // 1 byte
            {
                if (size >= 3)
                {
                    dest[0] = '%'; dest[1] = Hex16[src >> 4]; dest[2] = Hex16[src & 0xf];
                    return 3;
                }
            }
        }
        else if (src < 0x800) // 2 byte
        {
            if (size >= 6)
            {
                const auto val0 = T(0b11000000 | ((src >> 6) & 0x3f)), val1 = T(0x80 | (src & 0x3f));
                dest[0] = '%'; dest[1] = Hex16[val0 >> 4]; dest[2] = Hex16[val0 & 0xf];
                dest[3] = '%'; dest[4] = Hex16[val1 >> 4]; dest[5] = Hex16[val1 & 0xf];
                return 6;
            }
        }
        else if (src < 0x10000) // 3 byte
        {
            if (size >= 9)
            {
                const auto val0 = T(0b11100000 | (src >> 12)), val1 = T(0x80 | ((src >> 6) & 0x3f)), val2 = T(0x80 | (src & 0x3f));
                dest[0] = '%'; dest[1] = Hex16[val0 >> 4]; dest[2] = Hex16[val0 & 0xf];
                dest[3] = '%'; dest[4] = Hex16[val1 >> 4]; dest[5] = Hex16[val1 & 0xf];
                dest[6] = '%'; dest[7] = Hex16[val2 >> 4]; dest[8] = Hex16[val2 & 0xf];
                return 9;
            }
        }
        else if (src < 0x200000) // 4 byte
        {
            if (size >= 12)
            {
                const auto val0 = T(0b11110000 | (src >> 18)), val1 = T(0x80 | ((src >> 12) & 0x3f));
                const auto val2 = T(0x80 | ((src >> 6) & 0x3f)), val3 = T(0x80 | (src & 0x3f));
                dest[0] = '%'; dest[ 1] = Hex16[val0 >> 4]; dest[ 2] = Hex16[val0 & 0xf];
                dest[3] = '%'; dest[ 4] = Hex16[val1 >> 4]; dest[ 5] = Hex16[val1 & 0xf];
                dest[6] = '%'; dest[ 7] = Hex16[val2 >> 4]; dest[ 8] = Hex16[val2 & 0xf];
                dest[9] = '%'; dest[10] = Hex16[val3 >> 4]; dest[11] = Hex16[val3 & 0xf];
                return 12;
            }
        }
        return 0;
    }
public:
    using ElementType = char;
    static constexpr size_t MaxOutputUnit = 12;
    [[nodiscard]] forceinline static constexpr std::pair<char32_t, uint32_t> From(const ElementType* __restrict const src, const size_t size) noexcept
    {
        return InnerFrom(src, size);
    }
    [[nodiscard]] forceinline static constexpr uint8_t To(const char32_t src, const size_t size, ElementType* __restrict const dest) noexcept
    {
        return InnerTo(src, size, dest);
    }
    [[nodiscard]] forceinline static constexpr std::pair<char32_t, uint32_t> FromBytes(const uint8_t* __restrict const src, const size_t size) noexcept
    {
        return InnerFrom(src, size);
    }
    [[nodiscard]] forceinline static constexpr uint8_t ToBytes(const char32_t src, const size_t size, uint8_t* __restrict const dest) noexcept
    {
        return InnerTo(src, size, dest);
    }
};

struct UTF16 : public ConvertCPBase
{
    using ElementType = char16_t;
    static constexpr size_t MaxOutputUnit = 2;
    [[nodiscard]] inline static constexpr std::pair<char32_t, uint32_t> From(const char16_t* __restrict const src, const size_t size) noexcept
    {
        if (size == 0)
            return InvalidCharPair;
        if (src[0] < 0xd800 || src[0] >= 0xe000)//1 unit
            return { src[0], 1 };
        if (src[0] <= 0xdbff)//2 unit
        {
            if (size >= 2 && src[1] >= 0xdc00 && src[1] <= 0xdfff)
                return { (((src[0] & 0x3ff) << 10) | (src[1] & 0x3ff)) + 0x10000, 2 };
        }
        return InvalidCharPair;
    }
    [[nodiscard]] inline static constexpr uint8_t To(const char32_t src, const size_t size, char16_t* __restrict const dest) noexcept
    {
        if (src < 0xd800)
        {
            dest[0] = (char16_t)src;
            return 1;
        }
        if (src < 0xe000)
            return 0;
        if (src < 0x10000)
        {
            dest[0] = (char16_t)src;
            return 1;
        }
        if (size >= 2 && src < 0x200000u)
        {
            const auto tmp = src - 0x10000;
            dest[0] = char16_t(0xd800 | (tmp >> 10)), dest[1] = char16_t(0xdc00 | (tmp & 0x3ff));
            return 2;
        }
        return 0;
    }
};
struct UTF16BE : public UTF16, public ConvertByteBase
{
    [[nodiscard]] inline static constexpr std::pair<char32_t, uint32_t> FromBytes(const uint8_t* __restrict const src, const size_t size) noexcept
    {
        if (size < 2)
            return InvalidCharPair;
        if (src[0] < 0xd8 || src[0] >= 0xe0)//1 unit
            return { (src[0] << 8) | src[1], 2 };
        if (src[0] < 0xdc)//2 unit
        {
            if (size >= 4 && src[2] >= 0xdc && src[2] <= 0xdf)
                return { (((src[0] & 0x3) << 18) | (src[1] << 10) | ((src[2] & 0x3) << 8) | src[3]) + 0x10000, 4 };
        }
        return InvalidCharPair;
    }
    [[nodiscard]] inline static constexpr uint8_t ToBytes(const char32_t src, const size_t size, uint8_t* __restrict const dest) noexcept
    {
        if (size < 2)
            return 0;
        if (src < 0xd800)
        {
            dest[0] = uint8_t(src >> 8);
            dest[1] = src & 0xff;
            return 2;
        }
        if (src < 0xe000)
            return 0;
        if (src < 0x10000)
        {
            dest[0] = uint8_t(src >> 8);
            dest[1] = src & 0xff;
            return 2;
        }
        if (size >= 4 && src < 0x200000u)
        {
            const auto tmp = src - 0x10000;
            dest[0] = uint8_t(0xd8 | (tmp >> 18)), dest[1] = ((tmp >> 10) & 0xff);
            dest[2] = 0xdc | ((tmp >> 8) & 0x3), dest[3] = tmp & 0xff;
            return 4;
        }
        return 0;
    }
};
struct UTF16LE : public UTF16, public ConvertByteBase
{
    [[nodiscard]] inline static constexpr std::pair<char32_t, uint32_t> FromBytes(const uint8_t* __restrict const src, const size_t size) noexcept
    {
        if (size < 2)
            return InvalidCharPair;
        if (src[1] < 0xd8 || src[1] >= 0xe0)//1 unit
            return { (src[1] << 8) | src[0], 2 };
        if (src[1] < 0xdc)//2 unit
        {
            if (size >= 4 && src[3] >= 0xdc && src[3] <= 0xdf)
                return { (((src[1] & 0x3) << 18) | (src[0] << 10) | ((src[3] & 0x3) << 8) | src[2]) + 0x10000, 4 };
        }
        return InvalidCharPair;
    }
    [[nodiscard]] inline static constexpr uint8_t ToBytes(const char32_t src, const size_t size, uint8_t* __restrict const dest) noexcept
    {
        if (size < 2)
            return 0;
        if (src < 0xd800)
        {
            dest[1] = uint8_t(src >> 8);
            dest[0] = src & 0xff;
            return 2;
        }
        if (src < 0xe000)
            return 0;
        if (src < 0x10000)
        {
            dest[1] = uint8_t(src >> 8);
            dest[0] = src & 0xff;
            return 2;
        }
        if (size >= 4 && src < 0x200000u)
        {
            const auto tmp = src - 0x10000;
            dest[1] = uint8_t(0xd8 | (tmp >> 18)), dest[0] = ((tmp >> 10) & 0xff);
            dest[3] = 0xdc | ((tmp >> 8) & 0x3), dest[2] = tmp & 0xff;
            return 4;
        }
        return 0;
    }
};

#include "LUT_gb18030.tab"

struct GB18030 : public ConvertCPBase, public ConvertByteBase
{
private:
    template<typename T>
    [[nodiscard]] inline static constexpr std::pair<char32_t, uint32_t> InnerFrom(const T* __restrict const src, const size_t size) noexcept
    {
        if (size == 0)
            return InvalidCharPair;
        if ((uint32_t)src[0] < 0x80)//1 byte
        {
            return { src[0], 1 };
        }
        if (size >= 2 && (uint32_t)src[0] >= 0x81 && (uint32_t)src[0] <= 0xfe)
        {
            if (((uint32_t)src[1] >= 0x40 && (uint32_t)src[1] < 0x7f) || ((uint32_t)src[1] >= 0x80 && (uint32_t)src[1] < 0xff))//2 byte
            {
                const uint32_t tmp = ((uint32_t)src[0] << 8) | (uint32_t)src[1];
                const auto ch = LUT_GB18030_2B[tmp - LUT_GB18030_2B_BEGIN];
                return { ch, 2 };
            }
            if (size >= 4 
                && (uint32_t)src[1] >= 0x30 && (uint32_t)src[1] <= 0x39 
                && (uint32_t)src[2] >= 0x81 && (uint32_t)src[2] <= 0xfe 
                && (uint32_t)src[3] >= 0x30 && (uint32_t)src[3] <= 0x39)//4 byte
            {
                const uint32_t tmp = (src[3] - 0x30u) 
                                   + (src[2] - 0x81u) * 10 
                                   + (src[1] - 0x30u) * (0xff - 0x81) * 10 
                                   + (src[0] - 0x81u) * (0xff - 0x81) * 10 * 10;
                const auto ch = tmp < LUT_GB18030_4B_SIZE ? LUT_GB18030_4B[tmp] : 0x10000 + (tmp - LUT_GB18030_4B_SIZE);
                return { ch, 4 };
            }
        }
        return InvalidCharPair;
    }
    template<typename T>
    [[nodiscard]] inline static constexpr uint8_t InnerTo(const char32_t src, const size_t size, T* __restrict const dest) noexcept
    {
        if (src < 0x80)//1 byte
        {
            if (size < 1)
                return 0;
            dest[0] = (char)(src & 0x7f);
            return 1;
        }
        if (src < 0x10000)//2 byte
        {
            const auto tmp = src - LUT_GB18030_R_BEGIN;
            const auto ch = LUT_GB18030_R[tmp];
            if (ch == 0)//invalid
                return 0;
            if (ch < 0xffff)
            {
                if (size < 2)
                    return 0;
                dest[0] = T(ch & 0xff); dest[1] = T(ch >> 8);
                return 2;
            }
            else
            {
                if (size < 4)
                    return 0;
                dest[0] = T(ch & 0xff); dest[1] = T(ch >> 8); dest[2] = T(ch >> 16); dest[3] = T(ch >> 24);
                return 4;
            }
        }
        if (src < 0x200000)//4 byte
        {
            if (size < 4)
                return 0;
            auto tmp = LUT_GB18030_4B_SIZE + src - 0x10000;
            dest[0] = char(tmp / ((0xff - 0x81) * 10 * 10) + 0x81);
            tmp = tmp % ((0xff - 0x81) * 10 * 10);
            dest[1] = char(tmp / ((0xff - 0x81) * 10) + 0x30);
            tmp = tmp % ((0xff - 0x81) * 10);
            dest[2] = char(tmp / 10 + 0x81);
            dest[3] = char((tmp % 10) + 0x30);
            return 4;
        }
        return 0;
    }
public:
    using ElementType = char;
    static constexpr size_t MaxOutputUnit = 4;
    [[nodiscard]] forceinline static constexpr std::pair<char32_t, uint32_t> From(const ElementType* __restrict const src, const size_t size) noexcept
    {
        return InnerFrom(src, size);
    }
    [[nodiscard]] forceinline static constexpr uint8_t To(const char32_t src, const size_t size, ElementType* __restrict const dest) noexcept
    {
        return InnerTo(src, size, dest);
    }
    [[nodiscard]] forceinline static constexpr std::pair<char32_t, uint32_t> FromBytes(const uint8_t* __restrict const src, const size_t size) noexcept
    {
        return InnerFrom(src, size);
    }
    [[nodiscard]] forceinline static constexpr uint8_t ToBytes(const char32_t src, const size_t size, uint8_t* __restrict const dest) noexcept
    {
        return InnerTo(src, size, dest);
    }
};



template<typename Conv, typename SrcType>
struct CommonDecoder
{
    static_assert(std::is_base_of_v<ConvertByteBase, Conv>, "Conv should be of ConvertByteBase");
protected:
    SrcType Source;
    [[nodiscard]] forceinline constexpr std::pair<char32_t, uint32_t> DecodeOnce() noexcept
    {
        if constexpr (std::is_same_v<typename Conv::ElementType, typename SrcType::value_type>)
            return Conv::From(Source.data(), Source.size());
        else
            return Conv::FromBytes(Source.data(), Source.size());
    }
public:
    constexpr CommonDecoder(const SrcType src) : Source(src)
    { }

    [[nodiscard]] inline constexpr char32_t Decode() noexcept
    {
        while (Source.size() > 0)
        {
            const auto [cp, cnt] = DecodeOnce();
            if (cp != InvalidChar)
            {
                Source = { Source.data() + cnt, Source.size() - cnt };
                return cp;
            }
            else
            {
                Source = { Source.data() +   1, Source.size() -   1 };
            }
        }
        return InvalidChar;
    }
};
template<typename Conv, typename Char>
[[nodiscard]] inline constexpr auto GetDecoder(std::basic_string_view<Char> src)
{
    if constexpr (std::is_same_v<typename Conv::ElementType, Char> && std::is_base_of_v<ConvertCPBase, Conv>)
    {
        return CommonDecoder<Conv, std::basic_string_view<Char>>(src);
    }
    else
    {
        static_assert(std::is_base_of_v<ConvertByteBase, Conv>, "Conv should be of ConvertByteBase");
        common::span<const uint8_t> src2(reinterpret_cast<const uint8_t*>(src.data()), src.size() * sizeof(Char));
        return CommonDecoder<Conv, common::span<const uint8_t>>(src2);
    }
}



template<typename Conv, typename Char>
struct CommonEncoder
{
    static_assert(std::is_base_of_v<ConvertByteBase, Conv>, "Conv should be of ConvertByteBase");
public:
    std::basic_string<Char> Result;
private:
    Char Temp[Conv::MaxOutputUnit * sizeof(typename Conv::ElementType) / sizeof(Char)] = { 0 };
    uint8_t* __restrict TmpPtr;
    [[nodiscard]] forceinline constexpr size_t EncodeOnce(const char32_t cp) noexcept
    {
        if constexpr (std::is_same_v<typename Conv::ElementType, Char>)
            return Conv::To(cp, Conv::MaxOutputUnit, Temp);
        else
        {
            const auto bytes = Conv::ToBytes(cp, sizeof(Temp), TmpPtr);
            return bytes / sizeof(Char);
        }
    }
public:
    CommonEncoder(const size_t hint) : TmpPtr(reinterpret_cast<uint8_t*>(Temp))
    {
        Result.reserve(hint);
    }

    inline constexpr void Encode(const char32_t cp) noexcept
    {
        const auto cnt = EncodeOnce(cp);
        if (cnt > 0)
        {
            Result.append(Temp, cnt);
        }
    }
};
template<typename Conv, typename Char>
[[nodiscard]] inline constexpr auto GetEncoder(const size_t hint = 0)
{
    if constexpr (std::is_same_v<typename Conv::ElementType, Char> && std::is_base_of_v<ConvertCPBase, Conv>)
    {
        return CommonEncoder<Conv, Char>(hint);
    }
    else
    {
        static_assert(std::is_base_of_v<ConvertByteBase, Conv>, "Conv should be of ConvertByteBase");
        static_assert(sizeof(typename Conv::ElementType) % sizeof(Char) == 0, "Conv's minimal output should be multiple of Char type");
        return CommonEncoder<Conv, Char>(hint);
    }
}



//template<typename Dec = Decoder<UTF8, char>, typename Enc = Encoder<UTF16LE, char16_t>>
template<typename Dec, typename Enc>
[[nodiscard]] inline auto Transform(Dec decoder, Enc encoder)
{
    while (true)
    {
        const auto codepoint = decoder.Decode();
        if (codepoint == InvalidChar) 
            break;
        encoder.Encode(codepoint);
    }
    return std::move(encoder.Result);
}
template<typename Dec, typename Enc, typename TransFunc>
[[nodiscard]] inline auto Transform(Dec decoder, Enc encoder, TransFunc trans)
{
    static_assert(std::is_invocable_r_v<char32_t, TransFunc, char32_t>, "TransFunc should accept an char32_t and return another");
    while (true)
    {
        auto codepoint = decoder.Decode();
        if (codepoint == InvalidChar)
            break;
        codepoint = trans(codepoint);
        if (codepoint == InvalidChar)
            break; 
        encoder.Encode(codepoint);
    }
    return std::move(encoder.Result);
}



#define DST_CONSTRAINT(Conv, Char)                                      \
if constexpr(sizeof(typename Conv::ElementType) % sizeof(Char) != 0)    \
    /*Conv's minimal output should be multiple of Char type*/           \
    Expects(false);                                                     \
else                                                                    \



template<typename Dest, typename Src>
[[nodiscard]] forceinline std::basic_string<Dest> DirectAssign(const std::basic_string_view<Src> str) noexcept
{
    if constexpr (sizeof(Src) <= sizeof(Dest))
        return std::basic_string<Dest>(str.cbegin(), str.cend());
    else
    {
        Expects(sizeof(Src) <= sizeof(Dest));
        return {};
    }
}
template<typename Dst, typename Src>
[[nodiscard]] forceinline std::basic_string<Dst> DirectCopyStr(const std::basic_string_view<Src> str) noexcept
{
    return std::basic_string<Dst>(reinterpret_cast<const Dst*>(str.data()), str.size() * sizeof(Src) / sizeof(Dst));
}


template<typename Char>
[[nodiscard]] inline u8string to_u8string_impl(const std::basic_string_view<Char> str, const Encoding inchset)
{
    switch (inchset)
    {
    case Encoding::ASCII:
    case Encoding::UTF8:
        return DirectCopyStr<u8ch_t>(str);
    case Encoding::UTF16LE:
        return Transform(GetDecoder<UTF16LE>(str), GetEncoder<UTF8, u8ch_t>());
    case Encoding::UTF16BE:
        return Transform(GetDecoder<UTF16BE>(str), GetEncoder<UTF8, u8ch_t>());
    case Encoding::UTF32LE:
        return Transform(GetDecoder<UTF32LE>(str), GetEncoder<UTF8, u8ch_t>());
    case Encoding::UTF32BE:
        return Transform(GetDecoder<UTF32BE>(str), GetEncoder<UTF8, u8ch_t>());
    case Encoding::GB18030:
        return Transform(GetDecoder<GB18030>(str), GetEncoder<UTF8, u8ch_t>());
    default: // should not enter, to please compiler
        Expects(false);
        return {};
    }
}


template<typename Char>
[[nodiscard]] inline std::u16string to_u16string_impl(const std::basic_string_view<Char> str, const Encoding inchset)
{
    switch (inchset)
    {
    case Encoding::ASCII:
        return Transform(GetDecoder<UTF7>   (str), GetEncoder<UTF16LE, char16_t>());
    case Encoding::UTF8:
        return Transform(GetDecoder<UTF8>   (str), GetEncoder<UTF16LE, char16_t>());
    case Encoding::UTF16LE:
        return DirectCopyStr<char16_t>(str);
    case Encoding::UTF16BE:
        return Transform(GetDecoder<UTF16BE>(str), GetEncoder<UTF16LE, char16_t>());
    case Encoding::UTF32LE:
        return Transform(GetDecoder<UTF32LE>(str), GetEncoder<UTF16LE, char16_t>());
    case Encoding::UTF32BE:
        return Transform(GetDecoder<UTF32BE>(str), GetEncoder<UTF16LE, char16_t>());
    case Encoding::GB18030:
        return Transform(GetDecoder<GB18030>(str), GetEncoder<UTF16LE, char16_t>());
    default: // should not enter, to please compiler
        Expects(false);
        return {};
    }
}


template<typename Char>
[[nodiscard]] inline std::u32string to_u32string_impl(const std::basic_string_view<Char> str, const Encoding inchset)
{
    switch (inchset)
    {
    case Encoding::ASCII:
        return Transform(GetDecoder<UTF7>   (str), GetEncoder<UTF32LE, char32_t>());
    case Encoding::UTF8:
        return Transform(GetDecoder<UTF8>   (str), GetEncoder<UTF32LE, char32_t>());
    case Encoding::UTF16LE:
        return Transform(GetDecoder<UTF16LE>(str), GetEncoder<UTF32LE, char32_t>());
    case Encoding::UTF16BE:
        return Transform(GetDecoder<UTF16BE>(str), GetEncoder<UTF32LE, char32_t>());
    case Encoding::UTF32LE:
        return DirectCopyStr<char32_t>(str);
    case Encoding::UTF32BE:
        return Transform(GetDecoder<UTF32BE>(str), GetEncoder<UTF32LE, char32_t>());
    case Encoding::GB18030:
        return Transform(GetDecoder<GB18030>(str), GetEncoder<UTF32LE, char32_t>());
    default: // should not enter, to please compiler
        Expects(false);
        return {};
    }
}


template<typename Decoder>
[[nodiscard]] inline std::string to_string_impl(Decoder decoder, const Encoding outchset)
{
    switch (outchset)
    {
    case Encoding::ASCII:
        return Transform(std::move(decoder), GetEncoder<UTF7,    char>());
    case Encoding::UTF8:
        return Transform(std::move(decoder), GetEncoder<UTF8,    char>());
    case Encoding::UTF16LE:
        return Transform(std::move(decoder), GetEncoder<UTF16LE, char>());
    case Encoding::UTF16BE:
        return Transform(std::move(decoder), GetEncoder<UTF16BE, char>());
    case Encoding::UTF32LE:
        return Transform(std::move(decoder), GetEncoder<UTF32LE, char>());
    case Encoding::UTF32BE:
        return Transform(std::move(decoder), GetEncoder<UTF32BE, char>());
    case Encoding::GB18030:
        return Transform(std::move(decoder), GetEncoder<GB18030, char>());
    default: // should not enter, to please compiler
        Expects(false);
        return {};
    }
}
template<typename Char>
[[nodiscard]] inline std::string to_string_impl(const std::basic_string_view<Char> str, const Encoding inchset, const Encoding outchset)
{
    if (inchset == outchset)
        return DirectCopyStr<char>(str);
    switch (inchset)
    {
    case Encoding::ASCII:
        return to_string_impl(GetDecoder<UTF7>   (str), outchset);
    case Encoding::UTF8:
        return to_string_impl(GetDecoder<UTF8>   (str), outchset);
    case Encoding::UTF16LE:
        return to_string_impl(GetDecoder<UTF16LE>(str), outchset);
    case Encoding::UTF16BE:
        return to_string_impl(GetDecoder<UTF16BE>(str), outchset);
    case Encoding::UTF32LE:
        return to_string_impl(GetDecoder<UTF32LE>(str), outchset);
    case Encoding::UTF32BE:
        return to_string_impl(GetDecoder<UTF32BE>(str), outchset);
    case Encoding::GB18030:
        return to_string_impl(GetDecoder<GB18030>(str), outchset);
    default: // should not enter, to please compiler
        Expects(false);
        return {};
    }
}


}


template<typename T>
[[nodiscard]] forceinline std::string to_string(const T& str_, const Encoding outchset = Encoding::ASCII, const Encoding inchset = Encoding::ASCII)
{
    if constexpr (common::is_specialization<T, std::basic_string_view>::value)
    {
        using Char = typename T::value_type;
        return detail::to_string_impl<Char>(str_, inchset, outchset);

    }
    else
    {
        const auto str = ToStringView(str_);
        using Char = typename decltype(str)::value_type;
        return detail::to_string_impl<Char>(str_, inchset, outchset);
    }
}


template<typename T>
[[nodiscard]] forceinline u8string to_u8string(const T& str_, const Encoding inchset = Encoding::ASCII)
{
    if constexpr (common::is_specialization<T, std::basic_string_view>::value)
    {
        using Char = typename T::value_type;
        return detail::to_u8string_impl<Char>(str_, inchset);

    }
    else
    {
        const auto str = ToStringView(str_);
        using Char = typename decltype(str)::value_type;
        return detail::to_u8string_impl<Char>(str_, inchset);
    }
}


template<typename T>
[[nodiscard]] forceinline std::u16string to_u16string(const T& str_, const Encoding inchset = Encoding::ASCII)
{
    if constexpr (common::is_specialization<T, std::basic_string_view>::value)
    {
        using Char = typename T::value_type;
        return detail::to_u16string_impl<Char>(str_, inchset);

    }
    else
    {
        const auto str = ToStringView(str_);
        using Char = typename decltype(str)::value_type;
        return detail::to_u16string_impl<Char>(str_, inchset);
    }
}


template<typename T>
[[nodiscard]] forceinline std::u32string to_u32string(const T& str_, const Encoding inchset = Encoding::ASCII)
{
    if constexpr (common::is_specialization<T, std::basic_string_view>::value)
    {
        using Char = typename T::value_type;
        return detail::to_u32string_impl<Char>(str_, inchset);

    }
    else
    {
        const auto str = ToStringView(str_);
        using Char = typename decltype(str)::value_type;
        return detail::to_u32string_impl<Char>(str_, inchset);
    }
}


template<typename T>
[[nodiscard]] forceinline std::wstring to_wstring(const T& str_, const Encoding inchset = Encoding::ASCII)
{
    if constexpr (sizeof(wchar_t) == sizeof(char16_t))
    {
        auto ret = to_u16string(str_, inchset);
        return *reinterpret_cast<std::wstring*>(&ret);
    }
    else if constexpr (sizeof(wchar_t) == sizeof(char32_t))
    {
        auto ret = to_u32string(str_, inchset);
        return *reinterpret_cast<std::wstring*>(&ret);
    }
    else
    {
        static_assert(!common::AlwaysTrue<T>, "wchar_t size unrecognized");
    }
}



template<typename T>
[[nodiscard]] forceinline std::enable_if_t<std::is_integral_v<T>, std::string> to_string (const T val)
{
    return std::to_string(val);
}

template<typename T>
[[nodiscard]] forceinline std::enable_if_t<std::is_integral_v<T>, std::wstring> to_wstring(const T val)
{
    return std::to_wstring(val);
}



namespace detail
{


[[nodiscard]] forceinline constexpr char32_t EngUpper(const char32_t in) noexcept
{
    if (in >= U'a' && in <= U'z')
        return in - U'a' + U'A';
    else 
        return in;
}
[[nodiscard]] forceinline constexpr char32_t EngLower(const char32_t in) noexcept
{
    if (in >= U'A' && in <= U'Z')
        return in - U'A' + U'a';
    else 
        return in;
}

template<typename Char, typename T, typename TransFunc>
[[nodiscard]] inline std::basic_string<Char> DirectTransform(const std::basic_string_view<T> src, TransFunc&& trans)
{
    static_assert(std::is_invocable_r_v<Char, TransFunc, T>, "TransFunc should accept SrcChar and return DstChar");
    std::basic_string<Char> ret;
    ret.reserve(src.size());
    std::transform(src.cbegin(), src.cend(), std::back_inserter(ret), trans);
    return ret;
}



template<typename Conv, typename Char>
struct EngConver
{
private:
    static constexpr bool UseCodeUnit = std::is_same_v<typename Conv::ElementType, Char> && std::is_base_of_v<ConvertCPBase, Conv>;
    using SrcType = std::conditional_t<UseCodeUnit, std::basic_string_view<Char>, common::span<const uint8_t>>;
    
    SrcType Source;
    std::basic_string<Char> Result;
    size_t OutIdx = 0;

    [[nodiscard]] static constexpr SrcType GenerateSource(std::basic_string_view<Char> src) noexcept
    {
        if constexpr (UseCodeUnit)
            return src;
        else
            return common::span<const uint8_t>(reinterpret_cast<const uint8_t*>(src.data()), src.size() * sizeof(Char));
    }
    [[nodiscard]] forceinline constexpr std::pair<char32_t, uint32_t> DecodeOnce() noexcept
    {
        if constexpr (UseCodeUnit)
            return Conv::From     (Source.data(), Source.size());
        else
            return Conv::FromBytes(Source.data(), Source.size());
    }
    [[nodiscard]] forceinline constexpr size_t EncodeOnce(const char32_t cp, const size_t cnt) noexcept
    {
        if constexpr (UseCodeUnit)
            return Conv::To     (cp, cnt, &Result[OutIdx]);
        else
            return Conv::ToBytes(cp, cnt, reinterpret_cast<uint8_t*>(&Result[OutIdx]));
    }
    forceinline constexpr void MoveForward(const size_t cnt) noexcept
    {
        if constexpr (UseCodeUnit)
            OutIdx += cnt;
        else
            OutIdx += cnt / sizeof(Char);
    }
public:
    EngConver(const std::basic_string_view<Char> src) : 
        Source(GenerateSource(src)), Result(src)
    { }

    template<typename TransFunc>
    void Transform(TransFunc&& trans) noexcept
    {
        bool canSkip = true;
        while (Source.size() > 0)
        {
            const auto [cp, cnt] = DecodeOnce();
            if (cp == InvalidChar)
            {
                Source = { Source.data() +   1, Source.size() -   1 };
                canSkip = false;
                continue;
            }
            else
            {
                Source = { Source.data() + cnt, Source.size() - cnt };
                const auto newcp = (cp <= 0xff) ? trans(cp) : cp;
                if (newcp != cp || !canSkip)
                {
                    const auto outcnt = EncodeOnce(newcp, cnt);
                    MoveForward(outcnt);
                    canSkip &= outcnt == cnt;
                }
                else
                    MoveForward(cnt);
            }
        }
        Result.resize(OutIdx);
    }

    template<bool IsUpper>
    void Transform2() noexcept
    {
        bool canSkip = true;
        while (Source.size() > 0)
        {
            const auto [cp, cnt] = DecodeOnce();
            if (cp == InvalidChar)
            {
                Source = { Source.data() + 1, Source.size() - 1 };
                canSkip = false;
                continue;
            }
            else
            {
                Source = { Source.data() + cnt, Source.size() - cnt };
                auto newcp = cp;
                if constexpr (IsUpper)
                {
                    if (cp >= U'a' && cp <= U'z')
                        newcp = cp - U'a' + U'A';
                }
                else
                {
                    if (cp >= U'A' && cp <= U'Z')
                        newcp = cp - U'A' + U'a';
                }
                if (newcp != cp || !canSkip)
                {
                    const auto outcnt = EncodeOnce(newcp, cnt);
                    MoveForward(outcnt);
                    canSkip &= outcnt == cnt;
                }
                else
                    MoveForward(cnt);
            }
        }
        Result.resize(OutIdx);
    }

    template<typename TransFunc>
    static std::basic_string<Char> Convert([[maybe_unused]] const std::basic_string_view<Char> src, [[maybe_unused]] TransFunc&& trans) noexcept
    {
        if constexpr (sizeof(typename Conv::ElementType) % sizeof(Char) != 0)
        {   /*Conv's minimal output should be multiple of Char type*/
            Expects(false); return {};
        }
        else
        {
            EngConver<Conv, Char> conv(src);
            conv.Transform<TransFunc>(std::forward<TransFunc>(trans));
            return std::move(conv.Result);
        }
    }
};



// work around for MSVC's Release build 
template<typename Char, typename TransFunc>
[[nodiscard]] forceinline std::basic_string<Char> DirectConv2(const std::basic_string_view<Char> str, const Encoding inchset, TransFunc&& trans)
{
    //static_assert(std::is_invocable_r_v<char32_t, TransFunc, char32_t>, "TransFunc should accept char32_t and return char32_t");
    switch (inchset)
    {
    case Encoding::ASCII:
        return EngConver<UTF7,    Char>::Convert(str, std::forward<TransFunc>(trans));
    case Encoding::UTF8:
        return EngConver<UTF8,    Char>::Convert(str, std::forward<TransFunc>(trans));
    case Encoding::GB18030:
        return EngConver<GB18030, Char>::Convert(str, std::forward<TransFunc>(trans));
    default: // should not enter, to please compiler
        Expects(false);
        return {};
    }
}
template<typename Char, typename TransFunc>
[[nodiscard]] forceinline std::basic_string<Char> DirectConv(const std::basic_string_view<Char> str, const Encoding inchset, TransFunc&& trans)
{
    //static_assert(std::is_invocable_r_v<char32_t, TransFunc, char32_t>, "TransFunc should accept char32_t and return char32_t");
    switch (inchset)
    {
    case Encoding::UTF16LE:
        return EngConver<UTF16LE, Char>::Convert(str, std::forward<TransFunc>(trans));
    case Encoding::UTF16BE:
        return EngConver<UTF16BE, Char>::Convert(str, std::forward<TransFunc>(trans));
    case Encoding::UTF32LE:
        return EngConver<UTF32LE, Char>::Convert(str, std::forward<TransFunc>(trans));
    case Encoding::UTF32BE:
        return EngConver<UTF32BE, Char>::Convert(str, std::forward<TransFunc>(trans));
    default: // should not enter, to please compiler
        return DirectConv2<Char, TransFunc>(str, inchset, std::forward<TransFunc>(trans));
    }
}

}



template<typename T>
[[nodiscard]] forceinline auto ToUpperEng(const T& str_, const Encoding inchset = Encoding::ASCII)
{
    if constexpr (common::is_specialization<T, std::basic_string_view>::value)
    {
        using Char = typename T::value_type;
        return detail::DirectConv<Char>(str_, inchset, detail::EngUpper);
    }
    else
    {
        const auto str = ToStringView(str_);
        using Char = typename decltype(str)::value_type;
        return detail::DirectConv<Char>(str,  inchset, detail::EngUpper);
    }
}


template<typename T>
[[nodiscard]] forceinline auto ToLowerEng(const T& str_, const Encoding inchset = Encoding::ASCII)
{
    if constexpr (common::is_specialization<T, std::basic_string_view>::value)
    {
        using Char = typename T::value_type;
        return detail::DirectConv<Char>(str_, inchset, detail::EngLower);

    }
    else
    {
        const auto str = ToStringView(str_);
        using Char = typename decltype(str)::value_type;
        return detail::DirectConv<Char>(str,  inchset, detail::EngLower);
    }
}



namespace detail
{
template<typename Decoder>
[[nodiscard]] inline constexpr bool CaseInsensitiveCompare(Decoder str, Decoder prefix) noexcept
{
    while (true)
    {
        const auto strCp    =    str.Decode();
        const auto prefixCp = prefix.Decode();
        if (prefixCp == InvalidChar) // prefix finish
            break;
        if (strCp == InvalidChar) // str finish before prefix
            return false;
        if (strCp != prefixCp) // may need convert
        {
            const auto ch1 = (strCp    >= U'a' && strCp    <= U'z') ? (strCp    - U'a' + U'A') : strCp;
            const auto ch2 = (prefixCp >= U'a' && prefixCp <= U'z') ? (prefixCp - U'a' + U'A') : prefixCp;
            if (ch1 != ch2) // not the same, fast return
                return false;
        }
    }
    return true;
}


template<typename Char>
[[nodiscard]] inline bool IsIBeginWith_impl(const std::basic_string_view<Char> str, const std::basic_string_view<Char> prefix, const Encoding strchset)
{
    switch (strchset)
    {
    case Encoding::ASCII:
        return CaseInsensitiveCompare(GetDecoder<UTF7>   (str), GetDecoder<UTF7>   (prefix));
    case Encoding::UTF8:
        return CaseInsensitiveCompare(GetDecoder<UTF8>   (str), GetDecoder<UTF8>   (prefix));
    case Encoding::UTF16LE:
        return CaseInsensitiveCompare(GetDecoder<UTF16LE>(str), GetDecoder<UTF16LE>(prefix));
    case Encoding::UTF16BE:
        return CaseInsensitiveCompare(GetDecoder<UTF16BE>(str), GetDecoder<UTF16BE>(prefix));
    case Encoding::UTF32LE:
        return CaseInsensitiveCompare(GetDecoder<UTF32LE>(str), GetDecoder<UTF32LE>(prefix));
    case Encoding::UTF32BE:
        return CaseInsensitiveCompare(GetDecoder<UTF32BE>(str), GetDecoder<UTF32BE>(prefix));
    case Encoding::GB18030:
        return CaseInsensitiveCompare(GetDecoder<GB18030>(str), GetDecoder<GB18030>(prefix));
    default: // should not enter, to please compiler
        Expects(false);
        return false;
    }
}


}
template<typename T1, typename T2>
[[nodiscard]] forceinline bool IsIBeginWith(const T1& str_, const T2& prefix_, const Encoding strchset = Encoding::ASCII)
{
    const auto str = ToStringView(str_);
    using Char = typename decltype(str)::value_type;
    const auto prefix = ToStringView(prefix_);
    using CharP = typename decltype(prefix)::value_type;
    static_assert(std::is_same_v<Char, CharP>, "str and prefix should be of the same char_type");
    return detail::IsIBeginWith_impl(str, prefix, strchset);
}


}


