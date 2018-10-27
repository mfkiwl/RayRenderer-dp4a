#include "MiniLoggerRely.h"
#include "common/ThreadEx.inl"
#include "common/ColorConsole.inl"
#include <array>

#include <boost/version.hpp>
#pragma message("Compiling miniLogger with boost[" STRINGIZE(BOOST_LIB_VERSION) "]" )
#pragma message("Compiling miniLogger with fmt[" STRINGIZE(FMT_VERSION) "]" )


namespace common::mlog
{

namespace detail
{

template<>
MINILOGAPI fmt::basic_memory_buffer<char>& StrFormater::GetBuffer<char>()
{
    static thread_local fmt::basic_memory_buffer<char> out;
    return out;
}
template<>
MINILOGAPI fmt::basic_memory_buffer<char16_t>& StrFormater::GetBuffer<char16_t>()
{
    static thread_local fmt::basic_memory_buffer<char16_t> out;
    return out;
}
template<>
MINILOGAPI fmt::basic_memory_buffer<char32_t>& StrFormater::GetBuffer<char32_t>()
{
    static thread_local fmt::basic_memory_buffer<char32_t> out;
    return out;
}
template<>
MINILOGAPI fmt::basic_memory_buffer<wchar_t>& StrFormater::GetBuffer<wchar_t>()
{
    static thread_local fmt::basic_memory_buffer<wchar_t> out;
    return out;
}


}


constexpr auto GenLevelNumStr()
{
    std::array<char16_t, 256 * 4> ret{ u'\0' };
    for (uint16_t i = 0, j = 0; i < 256; ++i)
    {
        ret[j++] = i / 100 + u'0';
        ret[j++] = (i % 100) / 10 + u'0';
        ret[j] = (i % 10) + u'0';
        j += 2;
    }
    return ret;
}
static constexpr auto LevelNumStr = GenLevelNumStr();

const char16_t* GetLogLevelStr(const LogLevel level)
{
    switch (level)
    {
    case LogLevel::Debug: return u"Debug";
    case LogLevel::Verbose: return u"Verbose";
    case LogLevel::Info: return u"Info";
    case LogLevel::Warning: return u"Warning";
    case LogLevel::Success: return u"Success";
    case LogLevel::Error: return u"Error";
    case LogLevel::None: return u"None";
    default:
        return &LevelNumStr[(uint8_t)level * 4];
    }
}

}

