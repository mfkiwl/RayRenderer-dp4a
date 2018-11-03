#pragma once

#include "CommonRely.hpp"
#include "Exceptions.hpp"
#include "StrBase.hpp"
#include <cstring>
#include <algorithm>


namespace common::str
{


namespace detail
{
inline constexpr auto InvalidChar = static_cast<char32_t>(-1);
inline constexpr auto InvalidCharPair = std::pair<char32_t, uint32_t>{ InvalidChar, 0 };

//struct ConvertCPBase
//{
//    using ElementType = char;
//    static constexpr std::pair<char32_t, uint32_t> From(const ElementType* __restrict const, const size_t)
//    {
//        return InvalidCharPair;
//    }
//    static constexpr uint8_t To(const char32_t src, const size_t size, ElementType* __restrict const dest)
//    {
//        return 0;
//    }
//};
//struct ConvertByteBase
//{
//    static constexpr std::pair<char32_t, uint32_t> FromBytes(const uint8_t* __restrict const src, const size_t size)
//    {
//        return InvalidCharPair;
//    }
//    static constexpr uint8_t ToBytes(const char32_t src, const size_t size, uint8_t* __restrict const dest)
//    {
//        return 0;
//    }
//};

struct ConvertCPBase {};
struct ConvertByteBase {};

struct UTF7 : public ConvertCPBase, public ConvertByteBase
{
    using ElementType = char;
    static constexpr std::pair<char32_t, uint32_t> From(const ElementType* __restrict const src, const size_t size)
    {
        if (size >= 1 && src[0] > 0)
            return { src[0], 1 };
        return InvalidCharPair;
    }
    static constexpr uint8_t To(const char32_t src, const size_t size, ElementType* __restrict const dest)
    {
        if (size >= 1 && src < 128)
        {
            dest[0] = char(src);
            return 1;
        }
        return 0;
    }
    static constexpr std::pair<char32_t, uint32_t> FromBytes(const uint8_t* __restrict const src, const size_t size)
    {
        if (size >= 1 && src[0] > 0)
            return { src[0], 1 };
        return InvalidCharPair;
    }
    static constexpr uint8_t ToBytes(const char32_t src, const size_t size, uint8_t* __restrict const dest)
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
    static constexpr std::pair<char32_t, uint32_t> From(const ElementType* __restrict const src, const size_t size)
    {
        if (size >= 1 && src[0] < 0x200000)
        {
            return { src[0], 1 };
        }
        return InvalidCharPair;
    }
    static constexpr uint8_t To(const char32_t src, const size_t size, ElementType* __restrict const dest)
    {
        if (size >= 1 && src < 0x200000)
        {
            dest[0] = src;
            return 1;
        }
        return 0;
    }
};
struct UTF32LE : public UTF32, public ConvertByteBase
{
    static constexpr std::pair<char32_t, uint32_t> FromBytes(const uint8_t* __restrict const src, const size_t size)
    {
        if (size >= 4)
        {
            const uint32_t ch = src[0] | (src[1] << 8) | (src[2] << 16) | (src[3] << 24);
            if (ch < 0x200000u)
                return { ch, 4 };
        }
        return InvalidCharPair;
    }
    static constexpr uint8_t ToBytes(const char32_t src, const size_t size, uint8_t* __restrict const dest)
    {
        if (size >= 1 && src < 0x200000u)
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
    static constexpr std::pair<char32_t, uint32_t> FromBytes(const uint8_t* __restrict const src, const size_t size)
    {
        if (size >= 4)
        {
            const uint32_t ch = src[3] | (src[2] << 8) | (src[1] << 16) | (src[0] << 24);
            if (ch < 0x200000u)
                return { ch, 4 };
        }
        return InvalidCharPair;
    }
    static constexpr uint8_t ToBytes(const char32_t src, const size_t size, uint8_t* __restrict const dest)
    {
        if (size >= 1 && src < 0x200000)
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
    static constexpr std::pair<char32_t, uint32_t> InnerFrom(const T* __restrict const src, const size_t size)
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
            if (size >= 3 && ((src[1] & 0xc0u) == 0x80) && ((src[2] & 0xc0u) == 0x80u))
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
    static constexpr uint8_t InnerTo(const char32_t src, const size_t size, T* __restrict const dest)
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
    using ElementType = char;
    static constexpr std::pair<char32_t, uint32_t> From(const ElementType* __restrict const src, const size_t size)
    {
        return InnerFrom(src, size);
    }
    static constexpr uint8_t To(const char32_t src, const size_t size, ElementType* __restrict const dest)
    {
        return InnerTo(src, size, dest);
    }
    static constexpr std::pair<char32_t, uint32_t> FromBytes(const uint8_t* __restrict const src, const size_t size)
    {
        return InnerFrom(src, size);
    }
    static constexpr uint8_t ToBytes(const char32_t src, const size_t size, uint8_t* __restrict const dest)
    {
        return InnerTo(src, size, dest);
    }
};

struct UTF16 : public ConvertCPBase
{
    using ElementType = char32_t;
    static constexpr std::pair<char32_t, uint32_t> From(const char16_t* __restrict const src, const size_t size)
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
    static constexpr uint8_t To(const char32_t src, const size_t size, char16_t* __restrict const dest)
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
        if (size >= 2 && src < 0x200000)
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
    static constexpr std::pair<char32_t, uint32_t> FromBytes(const uint8_t* __restrict const src, const size_t size)
    {
        if (size < 2)
            return InvalidCharPair;
        if (src[0] < 0xd8 || src[0] >= 0xe0)//1 unit
            return { (src[0] << 8) | src[1], 2 };
        if (src[0] < 0xdc)//2 unit
        {
            if (size >= 2 && src[2] >= 0xdc && src[2] <= 0xdf)
                return { (((src[0] & 0x3) << 18) | (src[1] << 10) | ((src[2] & 0x3) << 8) | src[3]) + 0x10000, 4 };
        }
        return InvalidCharPair;
    }
    static constexpr uint8_t ToBytes(const char32_t src, const size_t size, uint8_t* __restrict const dest)
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
        if (size >= 4 && src < 0x200000)
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
    static constexpr std::pair<char32_t, uint32_t> FromBytes(const uint8_t* __restrict const src, const size_t size)
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
    static constexpr uint8_t ToBytes(const char32_t src, const size_t size, uint8_t* __restrict const dest)
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
        if (size >= 4 && src < 0x200000)
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
    static constexpr std::pair<char32_t, uint32_t> InnerFrom(const T* __restrict const src, const size_t size)
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
    static constexpr uint8_t InnerTo(const char32_t src, const size_t size, T* __restrict const dest)
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
    static constexpr std::pair<char32_t, uint32_t> From(const ElementType* __restrict const src, const size_t size)
    {
        return InnerFrom(src, size);
    }
    static constexpr uint8_t To(const char32_t src, const size_t size, ElementType* __restrict const dest)
    {
        return InnerTo(src, size, dest);
    }
    static constexpr std::pair<char32_t, uint32_t> FromBytes(const uint8_t* __restrict const src, const size_t size)
    {
        return InnerFrom(src, size);
    }
    static constexpr uint8_t ToBytes(const char32_t src, const size_t size, uint8_t* __restrict const dest)
    {
        return InnerTo(src, size, dest);
    }
};


template<class From, typename CharT, typename Consumer>
inline void ForEachChar(const CharT* __restrict const str, const size_t size, Consumer consumer)
{
    static_assert(std::is_base_of_v<ConvertByteBase, From>, "Covertor [From] should support ConvertByteBase");
    const uint8_t* __restrict src = reinterpret_cast<const uint8_t*>(str);
    for (size_t srcBytes = size * sizeof(CharT); srcBytes > 0;)
    {
        const auto[codepoint, len] = From::FromBytes(src, srcBytes);
        if (codepoint == InvalidChar)//fail
        {
            //move to next element
            srcBytes -= sizeof(CharT);
            src += sizeof(CharT);
            continue;
        }
        else
        {
            srcBytes -= len;
            src += len;
        }
        if (consumer(codepoint))
            return;
    }
}

template<class From1, class From2, typename CharT1, typename CharT2, typename Consumer>
inline void ForEachCharPair(const CharT1* __restrict const str1, const size_t size1, const CharT2* __restrict const str2, const size_t size2, Consumer consumer)
{
    static_assert(std::is_base_of_v<ConvertByteBase, From1>, "Covertor [From1] should support ConvertByteBase");
    static_assert(std::is_base_of_v<ConvertByteBase, From2>, "Covertor [From2] should support ConvertByteBase");
    const uint8_t* __restrict src1 = reinterpret_cast<const uint8_t*>(str1);
    const uint8_t* __restrict src2 = reinterpret_cast<const uint8_t*>(str2);
    for (size_t srcBytes1 = size1 * sizeof(CharT1), srcBytes2 = size2 * sizeof(CharT2); srcBytes1 > 0 && srcBytes2 > 0;)
    {
        const auto[cp1, len1] = From1::FromBytes(src1, srcBytes1);
        const auto[cp2, len2] = From2::FromBytes(src2, srcBytes2);
        if (cp1 == InvalidChar)//fail
        {
            //move to next element
            srcBytes1 -= sizeof(CharT1);
            src1 += sizeof(CharT1);
        }
        if (cp2 == InvalidChar)
        {
            //move to next element
            srcBytes2 -= sizeof(CharT2);
            src2 += sizeof(CharT2);
        }
        if(cp1 != InvalidChar && cp2 != InvalidChar)
        {
            srcBytes1 -= len1, srcBytes2 -= len2;
            src1 += len1, src2 += len2;
        }
        if (consumer(cp1, cp2))
            return;
    }
}

template<class From, class To, typename SrcType, typename DestType>
class CharsetConvertor
{
    static_assert(std::is_base_of_v<ConvertByteBase, From>, "Covertor [From] should support ConvertByteBase");
    static_assert(std::is_base_of_v<ConvertByteBase, To>, "Covertor [To] should support ConvertByteBase");
private:
    forceinline static char32_t Dummy(const char32_t in) { return in; }
public:
    template<typename TransformFunc>
    static std::basic_string<DestType> Transform(const SrcType* __restrict const str, const size_t size, TransformFunc transFunc)
    {
        std::basic_string<DestType> ret((size * 4) + 3 / sizeof(DestType), 0);//reserve space fit for all codepoint
        const uint8_t* __restrict src = reinterpret_cast<const uint8_t*>(str);
        size_t cacheidx = 0;
        for (size_t srcBytes = size * sizeof(SrcType); srcBytes > 0;)
        {
            auto[codepoint, len] = From::FromBytes(src, srcBytes);
            codepoint = transFunc(codepoint);
            if (codepoint == InvalidChar)//fail
            {
                //move to next element
                srcBytes -= sizeof(SrcType);
                src += sizeof(SrcType);
                continue;
            }
            else
            {
                srcBytes -= len;
                src += len;
            }
            uint8_t* __restrict dest = reinterpret_cast<uint8_t*>(ret.data()) + cacheidx;
            const auto len2 = To::ToBytes(codepoint, sizeof(char32_t), dest);
            if (len2 == 0)//fail
            {
                ;//do nothing, skip
            }
            else
            {
                cacheidx += len2;
            }
        }
        const auto destSize = (cacheidx + sizeof(DestType) - 1) / sizeof(DestType);
        ret.resize(destSize);
        return ret;
    }
    static std::basic_string<DestType> Convert(const SrcType* __restrict const str, const size_t size)
    {
        if constexpr (std::is_same_v<From, To> && sizeof(SrcType) == sizeof(DestType))
        {
            return std::basic_string<DestType>(str, str + size);
        }
        /* special optimize for UTF16 */
        else if constexpr (std::is_base_of_v<UTF16, From> && std::is_base_of_v<UTF16, To>)
        {
            static_assert(sizeof(SrcType) <= 2 && sizeof(DestType) <= 2, "char size too big for UTF16");
            const auto destcount = (size * sizeof(SrcType) + sizeof(DestType) - 1) / sizeof(DestType);
            const auto destcount2 = ((destcount * sizeof(DestType) + 1) / 2) * 2 / sizeof(DestType);
            std::basic_string<DestType> ret(destcount2, (DestType)0);
            const uint8_t * __restrict srcPtr = reinterpret_cast<const uint8_t*>(str);
            uint8_t * __restrict destPtr = reinterpret_cast<uint8_t*>(ret.data());
            for (auto procCnt = size * sizeof(SrcType) / 2; procCnt--; srcPtr += 2, destPtr += 2)
                destPtr[0] = srcPtr[1], destPtr[1] = srcPtr[0];
            return ret;
        }
        /* special optimize for UTF32 */
        else if constexpr (std::is_base_of_v<UTF32, From> && std::is_base_of_v<UTF32, To>)
        {
            static_assert(sizeof(SrcType) <= 4 && sizeof(DestType) <= 4, "char size too big for UTF32");
            const auto destcount = (size * sizeof(SrcType) + sizeof(DestType) - 1) / sizeof(DestType);
            const auto destcount2 = ((destcount * sizeof(DestType) + 3) / 4) * 4 / sizeof(DestType);
            std::basic_string<DestType> ret(destcount2, (DestType)0);
            const uint8_t * __restrict srcPtr = reinterpret_cast<const uint8_t*>(str);
            uint8_t * __restrict destPtr = reinterpret_cast<uint8_t*>(ret.data());
            for (auto procCnt = size * sizeof(SrcType) / 4; procCnt--; srcPtr += 4, destPtr += 4)
                destPtr[0] = srcPtr[3], destPtr[1] = srcPtr[2], destPtr[2] = srcPtr[1], destPtr[3] = srcPtr[0];
            return ret;
        }
        else
            return Transform(str, size, Dummy);
    }
};

}

#define CONCAT_TYPE_SIZE_STR(TYPE, SIZE) STRINGIZE(TYPE) " string should has type of at most size[" STRINGIZE(SIZE) "]"
#define CHK_CHAR_SIZE_MOST(TYPE, SIZE) \
    if constexpr(sizeof(Char) > SIZE) \
        COMMON_THROW(BaseException, UTF16ER(CONCAT_TYPE_SIZE_STR(TYPE, SIZE))); \
    else \
    {
#define CHK_CHAR_SIZE_END \
    }

template<typename Char>
inline std::string to_string(const Char *str, const size_t size, const Charset outchset = Charset::ASCII, const Charset inchset = Charset::ASCII)
{
    if (outchset == inchset)//just copy
        return std::string(reinterpret_cast<const char*>(str), reinterpret_cast<const char*>(str) + size * sizeof(Char));
    switch (inchset)
    {
    case Charset::ASCII:
        CHK_CHAR_SIZE_MOST(ASCII, 1)
            switch (outchset)
            {
            case Charset::UTF8:
                return detail::CharsetConvertor<detail::UTF7, detail::UTF8, Char, char>::Convert(str, size);
            case Charset::UTF16LE:
                return detail::CharsetConvertor<detail::UTF7, detail::UTF16LE, Char, char>::Convert(str, size);
            case Charset::UTF16BE:
                return detail::CharsetConvertor<detail::UTF7, detail::UTF16BE, Char, char>::Convert(str, size);
            case Charset::UTF32LE:
                return detail::CharsetConvertor<detail::UTF7, detail::UTF32LE, Char, char>::Convert(str, size);
            case Charset::UTF32BE:
                return detail::CharsetConvertor<detail::UTF7, detail::UTF32BE, Char, char>::Convert(str, size);
            case Charset::GB18030:
                return detail::CharsetConvertor<detail::UTF7, detail::GB18030, Char, char>::Convert(str, size);
            default: // should not enter, to please compiler
                return {};
            }
        CHK_CHAR_SIZE_END
        break;
    case Charset::UTF8:
        CHK_CHAR_SIZE_MOST(UTF8, 1)
            switch (outchset)
            {
            case Charset::ASCII:
                return detail::CharsetConvertor<detail::UTF8, detail::UTF7, Char, char>::Convert(str, size);
            case Charset::UTF16LE:
                return detail::CharsetConvertor<detail::UTF8, detail::UTF16LE, Char, char>::Convert(str, size);
            case Charset::UTF16BE:
                return detail::CharsetConvertor<detail::UTF8, detail::UTF16BE, Char, char>::Convert(str, size);
            case Charset::UTF32LE:
                return detail::CharsetConvertor<detail::UTF8, detail::UTF32LE, Char, char>::Convert(str, size);
            case Charset::UTF32BE:
                return detail::CharsetConvertor<detail::UTF8, detail::UTF32BE, Char, char>::Convert(str, size);
            case Charset::GB18030:
                return detail::CharsetConvertor<detail::UTF8, detail::GB18030, Char, char>::Convert(str, size);
            default: // should not enter, to please compiler
                return {};
            }
        CHK_CHAR_SIZE_END
        break;
    case Charset::UTF16LE:
        CHK_CHAR_SIZE_MOST(UTF16LE, 2)
            switch (outchset)
            {
            case Charset::ASCII:
                return detail::CharsetConvertor<detail::UTF16LE, detail::UTF7, Char, char>::Convert(str, size);
            case Charset::UTF8:
                return detail::CharsetConvertor<detail::UTF16LE, detail::UTF8, Char, char>::Convert(str, size);
            case Charset::UTF16BE:
                return detail::CharsetConvertor<detail::UTF16LE, detail::UTF16BE, Char, char>::Convert(str, size);
            case Charset::UTF32LE:
                return detail::CharsetConvertor<detail::UTF16LE, detail::UTF32LE, Char, char>::Convert(str, size);
            case Charset::UTF32BE:
                return detail::CharsetConvertor<detail::UTF16LE, detail::UTF32BE, Char, char>::Convert(str, size);
            case Charset::GB18030:
                return detail::CharsetConvertor<detail::UTF16LE, detail::GB18030, Char, char>::Convert(str, size);
            default: // should not enter, to please compiler
                return {};
            }
        CHK_CHAR_SIZE_END
        break;
    case Charset::UTF16BE:
        CHK_CHAR_SIZE_MOST(UTF16BE, 2)
            switch (outchset)
            {
            case Charset::ASCII:
                return detail::CharsetConvertor<detail::UTF16BE, detail::UTF7, Char, char>::Convert(str, size);
            case Charset::UTF8:
                return detail::CharsetConvertor<detail::UTF16BE, detail::UTF8, Char, char>::Convert(str, size);
            case Charset::UTF16BE:
                return detail::CharsetConvertor<detail::UTF16BE, detail::UTF16BE, Char, char>::Convert(str, size);
            case Charset::UTF32LE:
                return detail::CharsetConvertor<detail::UTF16BE, detail::UTF32LE, Char, char>::Convert(str, size);
            case Charset::UTF32BE:
                return detail::CharsetConvertor<detail::UTF16BE, detail::UTF32BE, Char, char>::Convert(str, size);
            case Charset::GB18030:
                return detail::CharsetConvertor<detail::UTF16BE, detail::GB18030, Char, char>::Convert(str, size);
            default: // should not enter, to please compiler
                return {};
            }
        CHK_CHAR_SIZE_END
        break;
    case Charset::UTF32LE:
        CHK_CHAR_SIZE_MOST(UTF32LE, 4)
            switch (outchset)
            {
            case Charset::ASCII:
                return detail::CharsetConvertor<detail::UTF32LE, detail::UTF7, Char, char>::Convert(str, size);
            case Charset::UTF8:
                return detail::CharsetConvertor<detail::UTF32LE, detail::UTF8, Char, char>::Convert(str, size);
            case Charset::UTF16LE:
                return detail::CharsetConvertor<detail::UTF32LE, detail::UTF16LE, Char, char>::Convert(str, size);
            case Charset::UTF16BE:
                return detail::CharsetConvertor<detail::UTF32LE, detail::UTF16BE, Char, char>::Convert(str, size);
            case Charset::UTF32BE:
                return detail::CharsetConvertor<detail::UTF32LE, detail::UTF32BE, Char, char>::Convert(str, size);
            case Charset::GB18030:
                return detail::CharsetConvertor<detail::UTF32LE, detail::GB18030, Char, char>::Convert(str, size);
            default: // should not enter, to please compiler
                return {};
            }
        CHK_CHAR_SIZE_END
            break;
    case Charset::UTF32BE:
        CHK_CHAR_SIZE_MOST(UTF32BE, 4)
            switch (outchset)
            {
            case Charset::ASCII:
                return detail::CharsetConvertor<detail::UTF32BE, detail::UTF7, Char, char>::Convert(str, size);
            case Charset::UTF8:
                return detail::CharsetConvertor<detail::UTF32BE, detail::UTF8, Char, char>::Convert(str, size);
            case Charset::UTF16LE:
                return detail::CharsetConvertor<detail::UTF32BE, detail::UTF16LE, Char, char>::Convert(str, size);
            case Charset::UTF16BE:
                return detail::CharsetConvertor<detail::UTF32BE, detail::UTF16BE, Char, char>::Convert(str, size);
            case Charset::UTF32LE:
                return detail::CharsetConvertor<detail::UTF32BE, detail::UTF32LE, Char, char>::Convert(str, size);
            case Charset::GB18030:
                return detail::CharsetConvertor<detail::UTF32BE, detail::GB18030, Char, char>::Convert(str, size);
            default: // should not enter, to please compiler
                return {};
            }
        CHK_CHAR_SIZE_END
            break;
    case Charset::GB18030:
        CHK_CHAR_SIZE_MOST(GB18030, 1)
            switch (outchset)
            {
            case Charset::ASCII:
                return detail::CharsetConvertor<detail::GB18030, detail::UTF7, Char, char>::Convert(str, size);
            case Charset::UTF8:
                return detail::CharsetConvertor<detail::GB18030, detail::UTF8, Char, char>::Convert(str, size);
            case Charset::UTF16LE:
                return detail::CharsetConvertor<detail::GB18030, detail::UTF16LE, Char, char>::Convert(str, size);
            case Charset::UTF16BE:
                return detail::CharsetConvertor<detail::GB18030, detail::UTF16BE, Char, char>::Convert(str, size);
            case Charset::UTF32LE:
                return detail::CharsetConvertor<detail::GB18030, detail::UTF32LE, Char, char>::Convert(str, size);
            case Charset::UTF32BE:
                return detail::CharsetConvertor<detail::GB18030, detail::UTF32BE, Char, char>::Convert(str, size);
            default: // should not enter, to please compiler
                return {};
            }
        CHK_CHAR_SIZE_END
        break;
    default:
        COMMON_THROW(BaseException, u"unknow charset", inchset);
    }
}


template<typename T>
inline std::enable_if_t<detail::IsDirectString<T>(), std::string>
    to_string(const T& str, const Charset outchset = Charset::ASCII, const Charset inchset = Charset::ASCII)
{
    return to_string(str.data(), str.size(), outchset, inchset);
}
template<typename Char>
inline std::string to_string(const Char *str, const Charset outchset = Charset::ASCII, const Charset inchset = Charset::ASCII)
{
    return to_string(str, std::char_traits<Char>::length(str), outchset, inchset);
}


template<typename Char>
inline std::string to_u8string(const Char *str, const size_t size, const Charset inchset = Charset::ASCII)
{
    return to_string(str, size, Charset::UTF8, inchset);
}
template<typename T>
inline std::enable_if_t<detail::IsDirectString<T>(), std::string>
    to_u8string(const T& str, const Charset inchset = Charset::ASCII)
{
    return to_string(str.data(), str.size(), Charset::UTF8, inchset);
}
template<typename Char>
inline std::string to_u8string(const Char *str, const Charset inchset = Charset::ASCII)
{
    return to_string(str, std::char_traits<Char>::length(str), Charset::UTF8, inchset);
}


template<typename Char>
inline std::u16string to_u16string(const Char *str, const size_t size, const Charset inchset = Charset::ASCII)
{
    switch (inchset)
    {
    case Charset::ASCII:
        CHK_CHAR_SIZE_MOST(ASCII, 1)
            return std::u16string(str, str + size);
        CHK_CHAR_SIZE_END
    case Charset::UTF8:
        CHK_CHAR_SIZE_MOST(UTF8, 1)
            return detail::CharsetConvertor<detail::UTF8, detail::UTF16LE, Char, char16_t>::Convert(str, size);
        CHK_CHAR_SIZE_END
    case Charset::UTF16LE:
        CHK_CHAR_SIZE_MOST(UTF16LE, 2)
            return detail::CharsetConvertor<detail::UTF16LE, detail::UTF16LE, Char, char16_t>::Convert(str, size);
        CHK_CHAR_SIZE_END
    case Charset::UTF16BE:
        CHK_CHAR_SIZE_MOST(UTF16BE, 2)
            return detail::CharsetConvertor<detail::UTF16BE, detail::UTF16LE, Char, char16_t>::Convert(str, size);
        CHK_CHAR_SIZE_END
    case Charset::UTF32LE:
        CHK_CHAR_SIZE_MOST(UTF32LE, 4)
            return detail::CharsetConvertor<detail::UTF32LE, detail::UTF16LE, Char, char16_t>::Convert(str, size);
        CHK_CHAR_SIZE_END
    case Charset::UTF32BE:
        CHK_CHAR_SIZE_MOST(UTF32BE, 4)
            return detail::CharsetConvertor<detail::UTF32BE, detail::UTF16LE, Char, char16_t>::Convert(str, size);
        CHK_CHAR_SIZE_END
    case Charset::GB18030:
        CHK_CHAR_SIZE_MOST(GB18030, 1)
            return detail::CharsetConvertor<detail::GB18030, detail::UTF16LE, Char, char16_t>::Convert(str, size);
        CHK_CHAR_SIZE_END
    default: // should not enter, to please compiler
        return {};
    }
}
template<typename T>
inline std::enable_if_t<detail::IsDirectString<T>(), std::u16string>
    to_u16string(const T& str, const Charset inchset = Charset::ASCII)
{
    return to_u16string(str.data(), str.size(), inchset);
}
template<typename Char>
inline std::u16string to_u16string(const Char *str, const Charset inchset = Charset::ASCII)
{
    return to_u16string(str, std::char_traits<Char>::length(str), inchset);
}


template<typename Char>
inline std::u32string to_u32string(const Char *str, const size_t size, const Charset inchset = Charset::ASCII)
{
    switch (inchset)
    {
    case Charset::ASCII:
        CHK_CHAR_SIZE_MOST(ASCII, 1)
            return std::u32string(str, str + size);
        CHK_CHAR_SIZE_END
    case Charset::UTF8:
        CHK_CHAR_SIZE_MOST(UTF8, 1)
            return detail::CharsetConvertor<detail::UTF8, detail::UTF32LE, Char, char32_t>::Convert(str, size);
        CHK_CHAR_SIZE_END
    case Charset::UTF16LE:
        CHK_CHAR_SIZE_MOST(UTF16LE, 2)
            return detail::CharsetConvertor<detail::UTF16LE, detail::UTF32LE, Char, char32_t>::Convert(str, size);
        CHK_CHAR_SIZE_END
    case Charset::UTF16BE:
        CHK_CHAR_SIZE_MOST(UTF16BE, 2)
            return detail::CharsetConvertor<detail::UTF16BE, detail::UTF32LE, Char, char32_t>::Convert(str, size);
        CHK_CHAR_SIZE_END
    case Charset::UTF32LE:
        CHK_CHAR_SIZE_MOST(UTF32LE, 4)
            return detail::CharsetConvertor<detail::UTF32LE, detail::UTF32LE, Char, char32_t>::Convert(str, size);
        CHK_CHAR_SIZE_END
    case Charset::UTF32BE:
        CHK_CHAR_SIZE_MOST(UTF32BE, 4)
            return detail::CharsetConvertor<detail::UTF32BE, detail::UTF32LE, Char, char32_t>::Convert(str, size);
        CHK_CHAR_SIZE_END
    case Charset::GB18030:
        CHK_CHAR_SIZE_MOST(GB18030, 1)
            return detail::CharsetConvertor<detail::GB18030, detail::UTF32LE, Char, char32_t>::Convert(str, size);
        CHK_CHAR_SIZE_END
    default: // should not enter, to please compiler
        return {};
    }
}
template<typename T>
inline std::enable_if_t<detail::IsDirectString<T>(), std::u32string>
    to_u32string(const T& str, const Charset inchset = Charset::ASCII)
{
    return to_u32string(str.data(), str.size(), inchset);
}
template<typename Char>
inline std::u32string to_u32string(const Char *str, const Charset inchset = Charset::ASCII)
{
    return to_u32string(str, std::char_traits<Char>::length(str), inchset);
}


template<typename Char>
inline std::wstring to_wstring(const Char *str, const size_t size, const Charset inchset = Charset::ASCII)
{
    static_assert(sizeof(Char) == sizeof(char32_t) || sizeof(Char) == sizeof(char16_t), "unrecognized wchar_t size");
    using To = std::conditional_t<sizeof(Char) == sizeof(char32_t), detail::UTF32LE, detail::UTF16LE>;
    switch (inchset)
    {
    case Charset::ASCII:
        CHK_CHAR_SIZE_MOST(ASCII, 1)
            return std::wstring(str, str + size);
        CHK_CHAR_SIZE_END
    case Charset::UTF8:
        CHK_CHAR_SIZE_MOST(UTF8, 1)
            return detail::CharsetConvertor<detail::UTF8, To, Char, wchar_t>::Convert(str, size);
        CHK_CHAR_SIZE_END
    case Charset::UTF16LE:
        CHK_CHAR_SIZE_MOST(UTF16LE, 2)
            return detail::CharsetConvertor<detail::UTF16LE, To, Char, wchar_t>::Convert(str, size);
        CHK_CHAR_SIZE_END
    case Charset::UTF16BE:
        CHK_CHAR_SIZE_MOST(UTF16BE, 2)
            return detail::CharsetConvertor<detail::UTF16BE, To, Char, wchar_t>::Convert(str, size);
        CHK_CHAR_SIZE_END
    case Charset::UTF32LE:
        CHK_CHAR_SIZE_MOST(UTF32LE, 4)
            return detail::CharsetConvertor<detail::UTF32LE, To, Char, wchar_t>::Convert(str, size);
        CHK_CHAR_SIZE_END
    case Charset::UTF32BE:
        CHK_CHAR_SIZE_MOST(UTF32BE, 4)
            return detail::CharsetConvertor<detail::UTF32BE, To, Char, wchar_t>::Convert(str, size);
        CHK_CHAR_SIZE_END
    case Charset::GB18030:
        CHK_CHAR_SIZE_MOST(GB18030, 1)
            return detail::CharsetConvertor<detail::GB18030, To, Char, wchar_t>::Convert(str, size);
        CHK_CHAR_SIZE_END
    default: // should not enter, to please compiler
        return {};
    }
}
template<typename T>
inline std::enable_if_t<detail::IsDirectString<T>(), std::wstring>
    to_wstring(const T& str, const Charset inchset = Charset::ASCII)
{
    return to_wstring(str.data(), str.size(), inchset);
}
template<typename Char>
inline std::wstring to_wstring(const Char *str, const Charset inchset = Charset::ASCII)
{
    return to_wstring(str, std::char_traits<Char>::length(str), inchset);
}


template<typename T>
forceinline std::enable_if_t<std::is_integral_v<T>, std::string> to_string(const T val)
{
    return std::to_string(val);
}

template<typename T>
forceinline std::enable_if_t<std::is_integral_v<T>, std::wstring> to_wstring(const T val)
{
    return std::to_wstring(val);
}


template<typename Char, typename Consumer>
inline void ForEachChar(const Char *str, const size_t size, const Consumer& consumer, const Charset inchset = Charset::ASCII)
{
    switch (inchset)
    {
    case Charset::ASCII:
        CHK_CHAR_SIZE_MOST(ASCII, 1)
            detail::ForEachChar<detail::UTF7, Char, Consumer>(str, size, consumer);
        CHK_CHAR_SIZE_END
    case Charset::UTF8:
        CHK_CHAR_SIZE_MOST(UTF8, 1)
            detail::ForEachChar<detail::UTF8, Char, Consumer>(str, size, consumer);
        CHK_CHAR_SIZE_END
    case Charset::UTF16LE:
        CHK_CHAR_SIZE_MOST(UTF16LE, 2)
            detail::ForEachChar<detail::UTF16LE, Char, Consumer>(str, size, consumer);
        CHK_CHAR_SIZE_END
    case Charset::UTF16BE:
        CHK_CHAR_SIZE_MOST(UTF16BE, 2)
            detail::ForEachChar<detail::UTF16BE, Char, Consumer>(str, size, consumer);
        CHK_CHAR_SIZE_END
    case Charset::UTF32LE:
        CHK_CHAR_SIZE_MOST(UTF32LE, 4)
            detail::ForEachChar<detail::UTF32LE, Char, Consumer>(str, size, consumer);
        CHK_CHAR_SIZE_END
    case Charset::UTF32BE:
        CHK_CHAR_SIZE_MOST(UTF32BE, 4)
            detail::ForEachChar<detail::UTF32BE, Char, Consumer>(str, size, consumer);
        CHK_CHAR_SIZE_END
    case Charset::GB18030:
        CHK_CHAR_SIZE_MOST(GB18030, 1)
            detail::ForEachChar<detail::GB18030, Char, Consumer>(str, size, consumer);
        CHK_CHAR_SIZE_END
    default:
        COMMON_THROW(BaseException, u"unsupported charset");
    }
}
template<typename T, typename Consumer>
inline std::enable_if_t<detail::IsDirectString<T>(), void>
    ForEachChar(const T& str, const Consumer& consumer, const Charset inchset = Charset::ASCII)
{
    return ForEachChar(str.data(), str.size(), consumer, inchset);
}
template<typename Char, typename Consumer>
inline void ForEachChar(const Char *str, const Consumer& consumer, const Charset inchset = Charset::ASCII)
{
    return ForEachChar(str, std::char_traits<Char>::length(str), consumer, inchset);
}


namespace detail
{
forceinline constexpr char32_t EngUpper(const char32_t in)
{
    if (in >= U'a' && in <= U'z')
        return in - U'a' + U'A';
    else return in;
}
forceinline constexpr char32_t EngLower(const char32_t in)
{
    if (in >= U'A' && in <= U'Z')
        return in - U'A' + U'a';
    else return in;
}
}


template<typename Char>
inline std::basic_string<Char> ToUpperEng(const Char *str, const size_t size, const Charset inchset = Charset::ASCII)
{
    switch (inchset)
    {
    case Charset::ASCII:
        CHK_CHAR_SIZE_MOST(ASCII, 1)
            //can only be 1-byte string
            std::basic_string<Char> ret; 
            ret.reserve(size);
            std::transform(str, str + size, std::back_inserter(ret), [](const Char ch) { return (ch >= 'a' && ch <= 'z') ? (Char)(ch - 'a' + 'A') : ch; });
            return ret;
        CHK_CHAR_SIZE_END
    case Charset::UTF8:
        CHK_CHAR_SIZE_MOST(UTF8, 1)
            return detail::CharsetConvertor<detail::UTF8, detail::UTF8, Char, Char>::Transform(str, size, detail::EngUpper);
        CHK_CHAR_SIZE_END
    case Charset::UTF16LE:
        CHK_CHAR_SIZE_MOST(UTF16LE, 2)
            return detail::CharsetConvertor<detail::UTF16LE, detail::UTF16LE, Char, Char>::Transform(str, size, detail::EngUpper);
        CHK_CHAR_SIZE_END
    case Charset::UTF16BE:
        CHK_CHAR_SIZE_MOST(UTF16BE, 2)
            return detail::CharsetConvertor<detail::UTF16BE, detail::UTF16BE, Char, Char>::Transform(str, size, detail::EngUpper);
        CHK_CHAR_SIZE_END
    case Charset::UTF32LE:
        CHK_CHAR_SIZE_MOST(UTF32LE, 4)
            if constexpr (std::is_same_v<Char, char32_t>)
            {
                std::basic_string<Char> ret; ret.reserve(size);
                std::transform(str, str + size, std::back_inserter(ret), detail::EngUpper);
                return ret;
            }
            else
                return detail::CharsetConvertor<detail::UTF32LE, detail::UTF32LE, Char, Char>::Transform(str, size, detail::EngUpper);
        CHK_CHAR_SIZE_END
    case Charset::UTF32BE:
        CHK_CHAR_SIZE_MOST(UTF32BE, 4)
            return detail::CharsetConvertor<detail::UTF32BE, detail::UTF32BE, Char, Char>::Transform(str, size, detail::EngUpper);
        CHK_CHAR_SIZE_END
    case Charset::GB18030:
        CHK_CHAR_SIZE_MOST(GB18030, 1)
            return detail::CharsetConvertor<detail::GB18030, detail::GB18030, Char, Char>::Transform(str, size, detail::EngUpper);
        CHK_CHAR_SIZE_END
    default:
        COMMON_THROW(BaseException, u"unsupported charset");
    }
}
template<typename T>
inline std::enable_if_t<detail::IsDirectString<T>(), std::basic_string<typename T::value_type>>
    ToUpperEng(const T& str, const Charset inchset = Charset::ASCII)
{
    return ToUpperEng(str.data(), str.size(), inchset);
}
template<typename Char>
inline void ToUpperEng(const Char *str, const Charset inchset = Charset::ASCII)
{
    return ToUpperEng(str, std::char_traits<Char>::length(str), inchset);
}


template<typename Char>
inline std::basic_string<Char> ToLowerEng(const Char *str, const size_t size, const Charset inchset = Charset::ASCII)
{
    switch (inchset)
    {
    case Charset::ASCII:
        CHK_CHAR_SIZE_MOST(ASCII, 1)
            //can only be 1-byte string
            std::basic_string<Char> ret;
            ret.reserve(size);
            std::transform(str, str + size, std::back_inserter(ret), [](const Char ch) { return (ch >= 'A' && ch <= 'Z') ? (Char)(ch - 'A' + 'a') : ch; });
            return ret;
        CHK_CHAR_SIZE_END
    case Charset::UTF8:
        CHK_CHAR_SIZE_MOST(UTF8, 1)
            return detail::CharsetConvertor<detail::UTF8, detail::UTF8, Char, Char>::Transform(str, size, detail::EngLower);
        CHK_CHAR_SIZE_END
    case Charset::UTF16LE:
        CHK_CHAR_SIZE_MOST(UTF16LE, 2)
            return detail::CharsetConvertor<detail::UTF16LE, detail::UTF16LE, Char, Char>::Transform(str, size, detail::EngLower);
        CHK_CHAR_SIZE_END
    case Charset::UTF16BE:
        CHK_CHAR_SIZE_MOST(UTF16BE, 2)
            return detail::CharsetConvertor<detail::UTF16BE, detail::UTF16BE, Char, Char>::Transform(str, size, detail::EngLower);
        CHK_CHAR_SIZE_END
    case Charset::UTF32LE:
        CHK_CHAR_SIZE_MOST(UTF32LE, 4)
            if constexpr (std::is_same_v<Char, char32_t>)
            {
                std::basic_string<Char> ret; ret.reserve(size);
                std::transform(str, str + size, std::back_inserter(ret), detail::EngLower);
                return ret;
            }
            else
                return detail::CharsetConvertor<detail::UTF32LE, detail::UTF32LE, Char, Char>::Transform(str, size, detail::EngLower);
        CHK_CHAR_SIZE_END
    case Charset::UTF32BE:
        CHK_CHAR_SIZE_MOST(UTF32BE, 4)
            return detail::CharsetConvertor<detail::UTF32BE, detail::UTF32BE, Char, Char>::Transform(str, size, detail::EngLower);
        CHK_CHAR_SIZE_END
    case Charset::GB18030:
        CHK_CHAR_SIZE_MOST(GB18030, 1)
            return detail::CharsetConvertor<detail::GB18030, detail::GB18030, Char, Char>::Transform(str, size, detail::EngLower);
        CHK_CHAR_SIZE_END
    default:
        COMMON_THROW(BaseException, u"unsupported charset");
    }
}
template<typename T>
inline std::enable_if_t<detail::IsDirectString<T>(), std::basic_string<typename T::value_type>>
    ToLowerEng(const T& str, const Charset inchset = Charset::ASCII)
{
    return ToLowerEng(str.data(), str.size(), inchset);
}
template<typename Char>
inline void ToLowerEng(const Char *str, const Charset inchset = Charset::ASCII)
{
    return ToLowerEng(str, std::char_traits<Char>::length(str), inchset);
}


namespace detail
{
template<class From1, class From2, typename CharT1, typename CharT2>
inline bool CaseInsensitiveCompare(const CharT1 *str, const size_t size1, const CharT2 *prefix, const size_t size2)
{
    bool isEqual = true;
    ForEachCharPair(str, size1, prefix, size2, [&](const char32_t ch1, const char32_t ch2) 
    {
        const auto ch3 = (ch1 >= U'a' && ch1 <= U'z') ? (ch1 - U'a' + U'A') : ch1;
        const auto ch4 = (ch2 >= U'a' && ch2 <= U'z') ? (ch2 - U'a' + U'A') : ch2;
        isEqual = ch3 == ch4;
        return !isEqual;
    });
    return isEqual;
}
}
template<typename Char>
inline bool IsIBeginWith(const Char *str, const size_t size1, const Char *prefix, const size_t size2, const Charset strchset = Charset::ASCII)
{
    if (size2 > size1)
        return false;
    switch (strchset)
    {
    case Charset::ASCII:
        CHK_CHAR_SIZE_MOST(ASCII, 1)
            return detail::CaseInsensitiveCompare<detail::UTF7, detail::UTF7, Char, Char>(str, size1, prefix, size2);
        CHK_CHAR_SIZE_END
    case Charset::UTF8:
        CHK_CHAR_SIZE_MOST(UTF8, 1)
            return detail::CaseInsensitiveCompare<detail::UTF8, detail::UTF8, Char, Char>(str, size1, prefix, size2);
        CHK_CHAR_SIZE_END
    case Charset::UTF16LE:
        CHK_CHAR_SIZE_MOST(UTF16LE, 2)
            return detail::CaseInsensitiveCompare<detail::UTF16LE, detail::UTF16LE, Char, Char>(str, size1, prefix, size2);
        CHK_CHAR_SIZE_END
    case Charset::UTF16BE:
        CHK_CHAR_SIZE_MOST(UTF16BE, 2)
            return detail::CaseInsensitiveCompare<detail::UTF16BE, detail::UTF16BE, Char, Char>(str, size1, prefix, size2);
        CHK_CHAR_SIZE_END
    case Charset::UTF32LE:
        CHK_CHAR_SIZE_MOST(UTF32LE, 4)
            return detail::CaseInsensitiveCompare<detail::UTF32LE, detail::UTF32LE, Char, Char>(str, size1, prefix, size2);
        CHK_CHAR_SIZE_END
    case Charset::UTF32BE:
        CHK_CHAR_SIZE_MOST(UTF32BE, 4)
            return detail::CaseInsensitiveCompare<detail::UTF32BE, detail::UTF32BE, Char, Char>(str, size1, prefix, size2);
        CHK_CHAR_SIZE_END
    case Charset::GB18030:
        CHK_CHAR_SIZE_MOST(GB18030, 1)
            return detail::CaseInsensitiveCompare<detail::GB18030, detail::GB18030, Char, Char>(str, size1, prefix, size2);
        CHK_CHAR_SIZE_END
    default:
        COMMON_THROW(BaseException, u"unsupported charset");
    }
}
template<typename T1, typename T2>
inline std::enable_if_t<detail::IsDirectString<T1>() && detail::IsDirectString<T2>(), bool>
    IsIBeginWith(const T1& str, const T2& prefix, const Charset strchset = Charset::ASCII)
{
    return IsIBeginWith(str.data(), str.size(), prefix.data(), prefix.size(), strchset);
}
template<typename Char>
inline bool IsIBeginWith(const Char *str, const Char *prefix, const Charset strchset = Charset::ASCII)
{
    return IsIBeginWith(str, std::char_traits<Char>::length(str), prefix, std::char_traits<Char>::length(prefix), strchset);
}


}


