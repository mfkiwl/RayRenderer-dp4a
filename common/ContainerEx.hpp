#pragma once

#include "CommonUtil.hpp"
#include <cstdint>
#include <optional>
#include <tuple>
#include <algorithm>
#include <functional>
#include <type_traits>
#include <utility>

namespace common::container
{

template<typename Child, typename StringType>
struct COMMONTPL NamedSetValue
{
    bool operator<(const Child& other) const noexcept { return ((const Child*)this)->Name < other.Name; }
};
template<typename Child, typename StringType>
forceinline bool operator<(const NamedSetValue<Child, StringType>& obj, const StringType& name) noexcept { return ((const Child*)&obj)->Name < name; }
template<typename Child, typename StringType>
forceinline bool operator<(const StringType& name, const NamedSetValue<Child, StringType>& obj) noexcept { return name < ((const Child*)&obj)->Name; }

template<typename Key, typename Val>
struct PairLess
{
    using is_transparent = void;
    constexpr bool operator()(const std::pair<Key, Val>& left, const std::pair<Key, Val>& right) const
    {
        return (left < right);
    }
    constexpr bool operator()(const std::pair<Key, Val>& left, const Key& right) const
    {
        return (left.first < right);
    }
    constexpr bool operator()(const Key& left, const std::pair<Key, Val>& right) const
    {
        return (left < right.first);
    }
};

template<typename Key, typename Val>
struct PairGreater
{
    using is_transparent = void;
    constexpr bool operator()(const std::pair<Key, Val>& left, const std::pair<Key, Val>& right) const
    {
        return (left > right);
    }
    constexpr bool operator()(const std::pair<Key, Val>& left, const Key& right) const
    {
        return (left.first > right);
    }
    constexpr bool operator()(const Key& left, const std::pair<Key, Val>& right) const
    {
        return (left > right.first);
    }
};

namespace detail
{

template<typename T, typename Ele>
struct EleTyper
{
    using type = /*typename*/ Ele;
};
template<typename T, typename Ele>
struct EleTyper<const T, Ele>
{
    using type = /*typename*/ const Ele;
};

}

template<class Set, typename Key, typename Val = typename detail::EleTyper<Set, typename Set::value_type>::type>
inline const Val* FindInSet(Set& theset, const Key& key)
{
    const auto it = theset.find(key);
    if (it == theset.end())//not exist
        return nullptr;
    return &(*it);
}

template<class Set, typename Key, typename Val = typename detail::EleTyper<Set, typename Set::value_type>::type>
inline std::optional<Val> FindInSet(Set& theset, const Key& key, const std::in_place_t)
{
    const auto it = theset.find(key);
    if (it == theset.end())//not exist
        return {};
    return *it;
}

template<class Set, typename Key, typename Val = typename detail::EleTyper<Set, typename Set::value_type>::type>
inline Val FindInSetOrDefault(Set& theset, const Key& key, const Val def = Val{})
{
    const auto it = theset.find(key);
    if (it == theset.end())//not exist
        return def;
    return *it;
}

template<class Map, typename Key = typename Map::key_type, typename Val = typename detail::EleTyper<Map, typename Map::mapped_type>::type>
inline Val* FindInMap(Map& themap, const Key& key)
{
    const auto it = themap.find(key);
    if (it == themap.end())//not exist
        return nullptr;
    return &it->second;
}

template<class Map, typename Key = typename Map::key_type, typename Val = typename Map::mapped_type>
inline std::optional<Val> FindInMap(Map& themap, const Key& key, const std::in_place_t)
{
    const auto it = themap.find(key);
    if (it == themap.end())//not exist
        return {};
    return it->second;
}

template<class Map, typename Key = typename Map::key_type, typename Val = typename detail::EleTyper<Map, typename Map::mapped_type>::type>
inline Val FindInMapOrDefault(Map& themap, const Key& key, const Val def = Val{})
{
    const auto it = themap.find(key);
    if (it == themap.end())//not exist
        return def;
    return it->second;
}

template<class Vec, typename Predictor, typename Val = typename detail::EleTyper<Vec, typename Vec::value_type>::type>
inline Val* FindInVec(Vec& thevec, const Predictor& pred)
{
    const auto it = std::find_if(thevec.begin(), thevec.end(), pred);
    if (it == thevec.end())//not exist
        return nullptr;
    return &(*it);
}

template<class Vec, typename Predictor, typename Val = typename Vec::value_type>
inline std::optional<Val> FindInVec(Vec& thevec, const Predictor& pred, const std::in_place_t)
{
    const auto it = std::find_if(thevec.begin(), thevec.end(), pred);
    if (it == thevec.end())//not exist
        return {};
    return *it;
}

template<class Vec>
inline bool ContainInVec(Vec& thevec, const typename Vec::value_type& val)
{
    for (const auto& ele : thevec)
        if (ele == val)
            return true;
    return false;
}

template<class Vec, typename Predictor, typename Val = typename Vec::value_type>
inline Val FindInVecOrDefault(Vec& thevec, const Predictor& pred, const Val def = Val{})
{
    const auto it = std::find_if(thevec.begin(), thevec.end(), pred);
    if (it == thevec.end())//not exist
        return def;
    return *it;
}

template<class Vec, typename Predictor, typename Val = typename detail::EleTyper<Vec, typename Vec::value_type>::type>
inline size_t ReplaceInVec(Vec& thevec, const Predictor& pred, const Val& val, const size_t cnt = SIZE_MAX)
{
    if (cnt == 0)
        return 0;
    size_t replacedCnt = 0;
    for (auto& obj : thevec)
    {
        if (pred(obj))
        {
            obj = val;
            if (++replacedCnt >= cnt)
                break;
        }
    }
    return replacedCnt;
}


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
    const std::tuple<Args&&...> srcs;
public:
    ZIPContainer(Args&&... args) : srcs(std::forward_as_tuple(std::forward<Args>(args)...)) {}
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
inline constexpr detail::ZIPContainer<Args...> zip(Args&&... args)
{
    return detail::ZIPContainer<Args...>(std::forward<Args>(args)...);
}
}