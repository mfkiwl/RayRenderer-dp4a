#pragma once

#include "CommonRely.hpp"
#include <type_traits>
#include <tuple>

namespace common
{


struct func_with_cookie {};

template<typename Func, bool WithCookie = std::is_base_of<func_with_cookie, Func>::value>
struct ForEach;

template<typename Func>
struct ForEach<Func, true>
{
    using CookieType = typename Func::CookieType;
private:
    ForEach() {};
    template<typename Tuple, std::size_t... Indexes>
    static CookieType EachTuple_impl(CookieType cookie, Tuple&& args, std::index_sequence<Indexes...>)
    {
        return Each_impl(cookie, std::get<Indexes>(args)...);
    }
    template<typename T>
    static CookieType Each_impl(CookieType& cookie, T&& arg)
    {
        Func::Each(cookie, arg);
        return cookie;
    }
    template<typename T, typename... Args>
    static CookieType Each_impl(CookieType& cookie, T&& arg, Args&&... args)
    {
        Each_impl(cookie, arg);
        return Each_impl(cookie, std::forward<Args>(args)...);
    }
public:
    template<typename... Args>
    static CookieType Each(Args&&... args)
    {
        auto cookie = Func::Init();
        return Each_impl(cookie, std::forward<Args>(args)...);
    }
    template<typename... Args>
    static CookieType Each2(CookieType cookie, Args&&... args)
    {
        return Each_impl(cookie, std::forward<Args>(args)...);
    }
    template<typename Tuple>
    static CookieType EachTuple(Tuple&& args)
    {
        auto cookie = Func::Init();
        return EachTuple_impl(cookie, args, std::make_index_sequence<std::tuple_size_v<std::decay_t<Tuple>>>{});
    }
    template<typename Tuple>
    static CookieType EachTuple2(CookieType cookie, Tuple&& args)
    {
        return EachTuple_impl(cookie, args, std::make_index_sequence<std::tuple_size_v<std::decay_t<Tuple>>>{});
    }
};

template<typename Func>
struct ForEach<Func, false>
{
private:
    ForEach() {};
    template<typename Tuple, std::size_t... Indexes>
    static void EachTuple_impl(Tuple&& args, std::index_sequence<Indexes...>)
    {
        Each(std::get<Indexes>(args)...);
    }
public:
    template<typename T>
    static void Each(T&& arg)
    {
        Func::Each(std::forward<T>(arg));
    }
    template<typename T, typename... Args>
    static void Each(T&& arg, Args&&... args)
    {
        Each(std::forward<T>(arg));
        Each(std::forward<Args>(args)...);
    }
    template<typename Tuple>
    static void EachTuple(Tuple&& args)
    {
        EachTuple_impl(args, std::make_index_sequence<std::tuple_size_v<std::decay_t<Tuple>>>{});
    }
};


template<typename Func, bool WithCookie = std::is_base_of<func_with_cookie, Func>::value>
struct Mapping;

template<typename Func>
struct Mapping<Func, false>
{
private:
    Mapping() { }
    template<typename Tuple, std::size_t... Indexes>
    static auto MapTuple_impl(Tuple&& args, std::index_sequence<Indexes...>)
    {
        return Map(std::get<Indexes>(args)...);
    }
    template<typename Tuple, std::size_t... Indexes>
    static auto FlatMapTuple_impl(Tuple&& args, std::index_sequence<Indexes...>)
    {
        return FlatMap(std::get<Indexes>(args)...);
    }
public:
    template<typename T>
    static auto Map(T&& arg)
    {
        return std::make_tuple(Func::Map(std::forward<T>(arg)));
    }
    template<typename T, typename... Args>
    static auto Map(T&& arg, Args&&... args)
    {
        return std::tuple_cat(Map(std::forward<T>(arg)), Map(std::forward<Args>(args)...));
    }
    template<typename T>
    static auto FlatMap(T&& arg)
    {
        //using RetType = decltype(Func::FlatMap(arg));
        //static_assert(std::is_base_of<std::tuple, RetType>::value, "FlatMap should return a tuple");
        return Func::FlatMap(arg);
    }
    template<typename T, typename... Args>
    static auto FlatMap(T&& arg, Args&&... args)
    {
        return std::tuple_cat(FlatMap(std::forward<T>(arg)), FlatMap(std::forward<Args>(args)...));
    }
    template<typename Tuple>
    static auto MapTuple(Tuple&& args)
    {
        return MapTuple_impl(args, std::make_index_sequence<std::tuple_size_v<std::decay_t<Tuple>>>{});
    }
    template<typename Tuple>
    static auto FlatMapTuple(Tuple&& args)
    {
        return FlatMapTuple_impl(args, std::make_index_sequence<std::tuple_size_v<std::decay_t<Tuple>>>{});
    }
};


namespace container
{

namespace detail
{

template<class... Args>
class ZIPContainer
{
private:
    struct Incer
    {
        template<typename T>
        static void Each(T& arg) { ++arg; }
    };
    struct Sizer : public func_with_cookie
    {
        using CookieType = size_t;
        static CookieType Init() { return SIZE_MAX; }
        template<typename T>
        static void Each(CookieType& cookie, T& arg) { cookie = std::min(arg.size(), cookie); }
    };
    struct MapBegin
    {
        template<typename T>
        static auto Map(T& arg) { return arg.begin(); }
    };
    const std::tuple<Args&& ...> srcs;
public:
    ZIPContainer(Args&& ... args) : srcs(std::forward_as_tuple(std::forward<Args>(args)...)) {}
    size_t size() const
    {
        return ForEach<Sizer>::EachTuple(srcs);
    }
    template<class Func>
    void foreach(const Func& func) const
    {
        auto begins = Mapping<MapBegin>::MapTuple(srcs);
        for (auto a = size(); a--;)
        {
            std::apply(func, begins);
            ForEach<Incer>::EachTuple(begins);
        }
    }
};
}

template<class... Args>
inline constexpr detail::ZIPContainer<Args...> zip(Args&& ... args)
{
    return detail::ZIPContainer<Args...>(std::forward<Args>(args)...);
}

}

}