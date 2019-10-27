#pragma once

/* Compier Test */

#if defined(__clang__)
#   define COMPILER_CLANG 1
#elif defined(__GNUC__)
#   define COMPILER_GCC 1
#elif defined(_MSC_VER)
#   define COMPILER_MSVC 1
#elif defined(__MINGW32__) || defined(__MINGW64__)
#   define COMPILER_MINGW 1
#endif

#ifndef COMPILER_CLANG
#   define COMPILER_CLANG 0
#endif
#ifndef COMPILER_GCC
#   define COMPILER_GCC 0
#endif
#ifndef COMPILER_MSVC
#   define COMPILER_MSVC 0
#endif
#ifndef COMPILER_MINGW
#   define COMPILER_MINGW 0
#endif



/* OS Test */

#if defined(_WIN32)
#   define COMMON_OS_WIN 1
#elif defined(__APPLE__)
#   define COMMON_OS_MACOS 1
#elif defined(__linux__)
#   define COMMON_OS_LINUX 1
#elif defined(__FreeBSD__)
#   define COMMON_OS_FREEBSD 1
#endif

#ifndef COMMON_OS_WIN
#   define COMMON_OS_WIN 0
#endif
#ifndef COMMON_OS_MACOS
#   define COMMON_OS_MACOS 0
#endif
#ifndef COMMON_OS_LINUX
#   define COMMON_OS_LINUX 0
#endif
#ifndef COMMON_OS_FREEBSD
#   define COMMON_OS_FREEBSD 0
#endif
#ifndef COMMON_OS_UNIX
#   if COMMON_OS_MACOS || COMMON_OS_LINUX || COMMON_OS_FREEBSD || defined(__unix__)
#       define COMMON_OS_UNIX 1
#   else
#       define COMMON_OS_UNIX 0
#   endif
#endif



/* C++ language test */

#if COMPILER_MSVC
#   if defined(_MSVC_LANG)
#       define COMMON_CPP_TIME _MSVC_LANG
#   else
#       define COMMON_CPP_TIME __cplusplus
#   endif
#else
#   define COMMON_CPP_TIME __cplusplus
#endif

#if COMMON_CPP_TIME >= 201703L 
#   define COMMON_CPP_17 1
#elif COMMON_CPP_TIME >= 201402L 
#   define COMMON_CPP_14 1
#elif COMMON_CPP_TIME >= 201103L 
#   define COMMON_CPP_11 1
#elif COMMON_CPP_TIME >= 199711L
#   define COMMON_CPP_03 1
#endif

#ifndef COMMON_CPP_17
#   define COMMON_CPP_17 0
#endif
#ifndef COMMON_CPP_14
#   define COMMON_CPP_14 0
#endif
#ifndef COMMON_CPP_11
#   define COMMON_CPP_11 0
#endif
#ifndef COMMON_CPP_03
#   define COMMON_CPP_03 0
#endif



/* over-aligned support */

#if COMMON_CPP_17 && defined(__cpp_aligned_new) && __cpp_aligned_new >= 201606L
#   define COMMON_OVER_ALIGNED 1
#else
#   define COMMON_OVER_ALIGNED 0
#endif



/* vectorcall fix */

#if COMMON_OS_WIN && !defined(_MANAGED) && !defined(_M_CEE)
#   define VECCALL __vectorcall
#else
#   define VECCALL 
#endif



/* c++ version compatible defines */




/* dynamic library defines */

#if defined(COMMON_OS_WIN)
# ifdef COMMON_EXPORT
#   define COMMONAPI _declspec(dllexport) 
#   define COMMONTPL _declspec(dllexport) 
# else
#   define COMMONAPI _declspec(dllimport) 
#   define COMMONTPL
# endif
#else
# ifdef COMMON_EXPORT
#   define COMMONAPI __attribute__((visibility("default")))
#   define COMMONTPL __attribute__((visibility("default")))
# else
#   define COMMONAPI 
#   define COMMONTPL
# endif
#endif



/* inline-related */

#if defined(_MSC_VER)
#   define forceinline      __forceinline
#   define forcenoinline    __declspec(noinline)
#elif defined(__GNUC__)
#   define forceinline      __inline__ __attribute__((always_inline))
#   define forcenoinline    __attribute__((noinline))
#   define _FILE_OFFSET_BITS 64
#else
#   define forceinline inline
#   define forcenoinline 
#endif



/* basic include */

#define __STDC_WANT_SECURE_LIB__ 1
#define __STDC_WANT_LIB_EXT1__ 1
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <numeric>
#include <type_traits> 
#include "3rdParty/gsl/gsl_assert"

#if UINTPTR_MAX == UINT64_MAX
#   define COMMON_OSBIT 64
#else
#   define COMMON_OSBIT 32
#endif



/* *_s fix */

#if defined(__GNUC__)
#   if !defined(__STDC_LIB_EXT1__)
#       include <errno.h>
forceinline std::remove_reference<decltype(errno)>::type memcpy_s(void* dest, size_t destsz, const void* src, size_t count)
{
    if (count == 0)
        return 0;
    if (src == nullptr || dest == nullptr || destsz < count)
    {
        memset(dest, 0, destsz);
        return EINVAL;
    }
    memcpy(dest, src, count);
    return 0;
}
forceinline std::remove_reference<decltype(errno)>::type memmove_s(void* dest, size_t destsz, const void* src, size_t count)
{
    if (count == 0)
        return 0;
    if (src == nullptr || dest == nullptr)
        return EINVAL;
    if (destsz < count)
        return ERANGE;
    memmove(dest, src, count);
    return 0;
}
#   endif
#endif



/*
* Concatenate preprocessor tokens A and B without expanding macro definitions
* (however, if invoked from a macro, macro arguments are expanded).
*/
#define PPCAT_NX(A, B) A ## B

/*
* Concatenate preprocessor tokens A and B after macro-expanding them.
*/
#define PPCAT(A, B) PPCAT_NX(A, B)

/*
* Turn A into a string literal without expanding macro definitions
* (however, if invoked from a macro, macro arguments are expanded).
*/
#define STRINGIZE_NX(A) #A

/*
* Turn A into a string literal after macro-expanding it.
*/
#define STRINGIZE(A) STRINGIZE_NX(A)

#define WIDEN_NX(X) L ## X
#define WIDEN(X) WIDEN_NX(X)

#define UTF16ER_NX(X) u ## X
#define UTF16ER(X) UTF16ER_NX(X)



/* hacker for std::make_shared/std::make_unique */

#include <memory>
#define MAKE_ENABLER() struct make_enabler

#define MAKE_ENABLER_IMPL(clz)                  \
struct clz::make_enabler : public clz           \
{                                               \
    template<typename... Args>                  \
    make_enabler(Args... args) :                \
        clz(std::forward<Args>(args)...) { }    \
};                                              \


#define MAKE_ENABLER_SHARED(clz, arg)       std::static_pointer_cast<clz>(std::make_shared<std::decay_t<clz>::make_enabler>arg)
#define MAKE_ENABLER_UNIQUE(clz, arg)       std::make_unique<std::decay_t<clz>::make_enabler>arg



/* compile-time str hash support */

/**
** @brief calculate simple hash for string, used for switch-string
** @param str std-string_view/string for the text
** @return uint64_t the hash
**/
template<typename T>
[[nodiscard]] inline constexpr uint64_t hash_(const T& str) noexcept
{
    uint64_t hash = 0;
    for (size_t a = 0, len = str.length(); a < len; ++a)
        hash = hash * 33 + str[a];
    return hash;
}
/**
** @brief calculate simple hash for string, used for switch-string
** @param str c-string for the text
** @return uint64_t the hash
**/
[[nodiscard]] inline constexpr uint64_t hash_(const char *str) noexcept
{
    uint64_t hash = 0;
    for (; *str != '\0'; ++str)
        hash = hash * 33 + *str;
    return hash;
}
/**
** @brief calculate simple hash for string, used for switch-string
** @return uint64_t the hash
**/
[[nodiscard]] inline constexpr uint64_t operator "" _hash(const char *str, size_t) noexcept
{
    return hash_(str);
}


namespace common
{


template<typename T>
using remove_cvref_t = typename std::remove_cv_t<std::remove_reference_t<T>>;


template<typename T>
constexpr bool AlwaysTrue() { return true; }


template<typename T, typename... Args>
[[nodiscard]] constexpr bool MatchAny(const T& obj, Args... args)
{
    return (... || (obj == args));
}


template<typename T>
[[nodiscard]] forceinline constexpr bool IsPower2(const T num)
{
    static_assert(std::is_unsigned_v<T>, "only for unsigned type");
    return (num & (num - 1)) == 0; 
}



template <typename, template <typename...> typename Op, typename... T>
struct is_detected_impl : std::false_type {};
template <template <typename...> typename Op, typename... T>
struct is_detected_impl<std::void_t<Op<T...>>, Op, T...> : std::true_type {};

template <template <typename...> typename Op, typename... T>
using is_detected = is_detected_impl<void, Op, T...>;
template <template <typename...> typename Op, typename... T>
inline constexpr bool is_detected_v = is_detected_impl<void, Op, T...>::value;


template<template<typename...> class Base, typename...Ts>
std::true_type is_base_of_template_impl(const Base<Ts...>*);
template<template<typename...> class Base>
std::false_type is_base_of_template_impl(...);
template<template<typename...> class Base, typename Derived>
using is_base_of_template = decltype(is_base_of_template_impl<Base>(std::declval<Derived*>()));

template <class T, template <typename...> class Template>
struct is_specialization : std::false_type {};
template <template <typename...> class Template, typename... Ts>
struct is_specialization<Template<Ts...>, Template> : std::true_type {};
#if defined(__cpp_nontype_template_parameter_auto) || (defined(__cplusplus) && (__cplusplus >= 201703L))
template <class T, template <auto...> class Template>
struct is_specialization2 : std::false_type {};
template <template <auto...> class Template, auto... Vs>
struct is_specialization2<Template<Vs...>, Template> : std::true_type {};
#endif


namespace detail
{
template<typename T>
using HasValueTypeCheck = typename T::value_type;
template<typename A, typename B>
using EqualCompareCheck = decltype(std::declval<const A&>() == std::declval<const B&>());
template<typename A, typename B>
using NotEqualCompareCheck = decltype(std::declval<const A&>() != std::declval<const B&>());
}
template<typename T>
inline constexpr bool has_valuetype_v = is_detected_v<detail::HasValueTypeCheck, T>;
template<typename A, typename B>
inline constexpr bool is_equal_comparable_v = common::is_detected_v<detail::EqualCompareCheck, A, B>;
template<typename A, typename B>
inline constexpr bool is_notequal_comparable_v = common::is_detected_v<detail::NotEqualCompareCheck, A, B>;


template<typename T, template <typename...> class Base>
struct TemplateDerivedHelper
{
private:
    template<typename... Ts>
    static std::true_type is_derived_from_base_impl(const Base<Ts...>*);
    static std::false_type is_derived_from_base_impl(const void*);
public:
    static constexpr bool IsDerivedFromBase = decltype(is_derived_from_base_impl(std::declval<T*>()))::value;
};



template<typename T>
[[nodiscard]] constexpr const T& max(const T& left, const T& right)
{
    static_assert(std::is_arithmetic_v<T>, "only support arithmetic type");
    return left < right ? right : left;
}
template<typename T>
[[nodiscard]] constexpr const T& min(const T& left, const T& right)
{
    static_assert(std::is_arithmetic_v<T>, "only support arithmetic type");
    return left < right ? left : right;
}
template<typename T, typename U>
[[nodiscard]] constexpr T saturate_cast(const U val)
{
    static_assert(std::is_arithmetic_v<T> && std::is_arithmetic_v<U>, "only support arithmetic type");
    constexpr bool MaxFit = static_cast<std::make_unsigned_t<U>>(std::numeric_limits<U>::max())
        <= static_cast<std::make_unsigned_t<T>>(std::numeric_limits<T>::max());
    constexpr bool MinFit = static_cast<std::make_signed_t<U>>(std::numeric_limits<U>::min())
        >= static_cast<std::make_signed_t<T>>(std::numeric_limits<T>::min());
    if constexpr (MaxFit)
    {
        if constexpr (MinFit)
            return val;
        else
            return static_cast<T>(max(static_cast<U>(std::numeric_limits<T>::min()), val));
    }
    else
    {
        if constexpr (MinFit)
            return static_cast<T>(min(static_cast<U>(std::numeric_limits<T>::max()), val));
        else
            return static_cast<T>(min(
                static_cast<U>(std::numeric_limits<T>::max()),
                max(static_cast<U>(std::numeric_limits<T>::min()), val)
            ));
    }
}


struct NonCopyable
{
    NonCopyable() noexcept = default;
    ~NonCopyable() noexcept = default;
    NonCopyable(const NonCopyable&) noexcept = delete;
    NonCopyable& operator= (const NonCopyable&) noexcept = delete;
    NonCopyable(NonCopyable&&) noexcept = default;
    NonCopyable& operator= (NonCopyable&&) noexcept = default;
};

struct NonMovable
{
    NonMovable() noexcept = default;
    ~NonMovable() noexcept = default;
    NonMovable(NonMovable&&) noexcept = delete;
    NonMovable& operator= (NonMovable&&) noexcept = delete;
    NonMovable(const NonMovable&) noexcept = default;
    NonMovable& operator= (const NonMovable&) noexcept = default;
};


[[nodiscard]] inline uint32_t TailZero(const uint32_t num) noexcept
{
#if COMPILER_MSVC
    unsigned long idx = 0;
    return _BitScanReverse(&idx, num) ? idx : UINT32_MAX;
#else
    return num == 0 ? UINT32_MAX : __builtin_ctz(num);
#endif
}

[[nodiscard]] inline uint32_t TailZero(const uint64_t num) noexcept
{
#if COMPILER_MSVC
    unsigned long idx = 0;
#   if COMMON_OSBIT == 64
    return _BitScanReverse64(&idx, num) ? idx : UINT32_MAX;
#   else
    if (!_BitScanReverse(&idx, static_cast<uint32_t>(num)))
    {
        return _BitScanReverse(&idx, static_cast<uint32_t>(num >> 32)) ? idx + 32 : UINT32_MAX;
    }
    return idx;
#   endif
#else
#   if COMMON_OSBIT == 64
    return num == 0 ? UINT32_MAX : __builtin_ctzl(num);
#   else
    if (num == 0) return UINT32_MAX;
    const auto loCnt = TailZero(static_cast<uint32_t>(num));
    return loCnt == UINT32_MAX ? TailZero(static_cast<uint32_t>(num >> 32)) + 32 : loCnt;
#   endif
#endif
}


}



/* span compatible include */
#if (defined(__cplusplus) && (__cplusplus >= 201709L)) && false
#   include <span>
namespace common
{
template <class ElementType, size_t Extent = std::dynamic_extent>
using span = std::span<ElementType, Extent>;
using std::as_bytes;
using std::as_writable_bytes;
namespace detail
{
template<typename T>
struct is_span : std::false_type { };
template <typename ElementType, size_t Extent>
struct is_span<std::span<ElementType, Extent>> : std::true_type { };
}
template <typename T>
inline constexpr bool is_span_v = detail::is_span<common::remove_cvref_t<T>>::value;
}
#else
#   include "3rdParty/gsl/span"
namespace common
{
template <class ElementType, ptrdiff_t Extent = gsl::dynamic_extent>
using span = gsl::span<ElementType, Extent>;
using gsl::as_bytes;
inline constexpr auto as_writable_bytes = [](auto&& t) constexpr -> decltype(auto) { return gsl::as_writeable_bytes(t); };
template <typename T>
inline constexpr bool is_span_v = gsl::details::is_span<T>::value;
}
#endif
namespace common
{
template <typename T, size_t N>
constexpr auto to_span(T(&arr)[N]) noexcept
{
    return common::span<T>(arr, N);

}
template <typename T>
constexpr auto to_span(T&& arg) noexcept
{
    using U = common::remove_cvref_t<T>;
    if constexpr (common::is_span_v<U>)
        return arg;
    else if constexpr (common::has_valuetype_v<U>)
    {
        constexpr auto IsConst = std::is_const_v<std::remove_reference_t<T>>;
        using EleType = std::conditional_t<IsConst, std::add_const_t<typename U::value_type>, typename U::value_type>;
        if constexpr (std::is_constructible_v<common::span<EleType>, T>)
            return common::span<EleType>(std::forward<T>(arg));
        else if constexpr (std::is_convertible_v<T, common::span<EleType>>)
            return (common::span<EleType>)arg;
        else
            static_assert(!common::AlwaysTrue<T>(), "unsupported");
    }
    else 
        static_assert(!common::AlwaysTrue<T>(), "unsupported");
}
}



/* variant extra support */

#   include <variant>
namespace common
{
#if defined(__cpp_lib_variant)
template <typename> struct variant_tag { };
template <typename T, typename... Ts>
inline constexpr size_t get_variant_index(variant_tag<std::variant<Ts...>>)
{
    return std::variant<variant_tag<Ts>...>(variant_tag<T>()).index();
}
template <typename T, typename V>
inline constexpr size_t get_variant_index_v()
{
    return get_variant_index<T>(variant_tag<V>());
}
#endif
}

