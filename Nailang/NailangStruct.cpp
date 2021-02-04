#include "NailangStruct.h"
#include "StringUtil/Format.h"
#include "StringUtil/Convert.h"
#include "common/AlignedBase.hpp"

#include <boost/preprocessor/seq/for_each.hpp>
#include <boost/preprocessor/tuple/elem.hpp>
#include <boost/preprocessor/variadic/to_seq.hpp>

#include <cassert>

namespace xziar::nailang
{
using namespace std::string_view_literals;


std::pair<size_t, size_t> MemoryPool::Usage() const noexcept
{
    size_t used = 0, unused = 0;
    for (const auto& trunk : Trunks)
    {
        used += trunk.Offset, unused += trunk.Avaliable;
    }
    return { used, used + unused };
}


static constexpr PartedName::PartType DummyPart[1] = { {(uint16_t)0, (uint16_t)0} };
TempPartedNameBase::TempPartedNameBase(std::u32string_view name, common::span<const PartedName::PartType> parts, uint16_t info)
    : Var(name, parts, info)
{
    Expects(parts.size() > 0);
    constexpr auto SizeP = sizeof(PartedName::PartType);
    const auto partSize = parts.size() * SizeP;
    switch (parts.size())
    {
    case 1:
        break;
    case 2:
    case 3:
    case 4:
        memcpy_s(Extra.Parts, partSize, parts.data(), partSize);
        break;
    default:
    {
        const auto ptr = reinterpret_cast<PartedName*>(malloc(sizeof(PartedName) + partSize));
        Extra.Ptr = ptr;
        new (ptr) PartedName(name, parts, info);
        memcpy_s(ptr + 1, partSize, parts.data(), partSize);
        break;
    }
    }
}
TempPartedNameBase::TempPartedNameBase(const PartedName* var) noexcept : Var(*var, DummyPart, var->ExternInfo)
{
    Extra.Ptr = var;
}
TempPartedNameBase::TempPartedNameBase(TempPartedNameBase&& other) noexcept :
    Var(other.Var, DummyPart, other.Var.ExternInfo), Extra(other.Extra)
{
    Var.PartCount = other.Var.PartCount;
    other.Var.Ptr = nullptr;
    other.Var.PartCount = 0;
    other.Extra.Ptr = nullptr;
}
TempPartedNameBase::~TempPartedNameBase()
{
    if (Var.PartCount > 4)
        free(const_cast<PartedName*>(Extra.Ptr));
}
TempPartedNameBase TempPartedNameBase::Copy() const noexcept
{
    common::span<const PartedName::PartType> parts;
    if (Var.PartCount > 4 || Var.PartCount == 0)
    {
        parts = { reinterpret_cast<const PartedName::PartType*>(Extra.Ptr + 1), Extra.Ptr->PartCount };
    }
    else
    {
        parts = { Extra.Parts, Var.PartCount };
    }
    return TempPartedNameBase(Var, parts, Var.ExternInfo);
}


std::u32string_view EmbedOpHelper::GetOpName(EmbedOps op) noexcept
{
    switch (op)
    {
    #define RET_NAME(op) case EmbedOps::op: return U"" STRINGIZE(op)
    RET_NAME(Equal);
    RET_NAME(NotEqual);
    RET_NAME(Less);
    RET_NAME(LessEqual);
    RET_NAME(Greater);
    RET_NAME(GreaterEqual);
    RET_NAME(And);
    RET_NAME(Or);
    RET_NAME(Not);
    RET_NAME(Add);
    RET_NAME(Sub);
    RET_NAME(Mul);
    RET_NAME(Div);
    RET_NAME(Rem);
    default: return U"Unknwon"sv;
    }
}


std::u32string_view RawArg::TypeName(const RawArg::Type type) noexcept
{
    switch (type)
    {
    case RawArg::Type::Empty:   return U"empty"sv;
    case RawArg::Type::Func:    return U"func-call"sv;
    case RawArg::Type::Unary:   return U"unary-expr"sv;
    case RawArg::Type::Binary:  return U"binary-expr"sv;
    case RawArg::Type::Query:   return U"query-expr"sv;
    case RawArg::Type::Var:     return U"variable"sv;
    case RawArg::Type::Str:     return U"string"sv;
    case RawArg::Type::Uint:    return U"uint"sv;
    case RawArg::Type::Int:     return U"int"sv;
    case RawArg::Type::FP:      return U"fp"sv;
    case RawArg::Type::Bool:    return U"bool"sv;
    default:                    return U"error"sv;
    }
}


//Arg FixedArray::Get(size_t idx) const noexcept
//{
//    Expects(idx < Length);
//#define RET(tenum, type) case Type::tenum: return reinterpret_cast<const type*>(DataPtr)[idx]
//#define RETC(tenum, type, ttype) case Type::tenum: return static_cast<ttype>(reinterpret_cast<const type*>(DataPtr)[idx])
//    switch (ElementType)
//    {
//    RET(Any, Arg);
//    RET(Bool, bool); 
//    RETC(U8, uint8_t, uint64_t);
//    RETC(I8, int8_t, int64_t);
//    RETC(U16, uint16_t, uint64_t);
//    RETC(I16, int16_t, int64_t);
//    RETC(F16, half_float::half, double);
//    RETC(U32, uint32_t, uint64_t);
//    RETC(I32, int32_t, int64_t);
//    RETC(F32, float, double);
//    RET(U64, uint64_t);
//    RET(I64, int64_t);
//    RET(F64, double);
//    //RET(Str8, );
//    //RET(Str16, Arg);
//    //RETC(Str32, );
//    //RET(Sv8, Arg);
//    //RET(Sv16, Arg);
//    RET(Sv32, std::u32string_view);
//    default: return {};
//    }
//#undef RETC
//#undef RET
//}
//
//bool FixedArray::Set(size_t idx, Arg val) const noexcept
//{
//    Expects(!IsReadOnly);
//    Expects(idx < Length);
//    if (val.IsEmpty()) return false;
//#define TYPESET(tenum, type)                                                        \
//    if (ElementType == Type::tenum)                                                 \
//    {                                                                               \
//        reinterpret_cast<type*>(DataPtr)[idx] = static_cast<type>(real.value());    \
//        return true;                                                                \
//    }
//    switch (ElementType)
//    {
//    case Type::Any:
//        reinterpret_cast<Arg*>(DataPtr)[idx] = std::move(val);
//        return true;
//    case Type::Bool:
//        if (const auto real = val.GetBool(); real.has_value())
//        {
//            reinterpret_cast<bool*>(DataPtr)[idx] = real.value();
//            return true;
//        }
//        return false;
//    case Type::U8:
//    case Type::U16:
//    case Type::U32:
//    case Type::U64:
//        if (const auto real = val.GetUint(); real.has_value())
//        {
//            TYPESET(U8,  uint8_t)
//            TYPESET(U16, uint16_t)
//            TYPESET(U32, uint32_t)
//            TYPESET(U64, uint64_t)
//        }
//        return false;
//    case Type::I8:
//    case Type::I16:
//    case Type::I32:
//    case Type::I64:
//        if (const auto real = val.GetInt(); real.has_value())
//        {
//            TYPESET(I8,  int8_t)
//            TYPESET(I16, int16_t)
//            TYPESET(I32, int32_t)
//            TYPESET(I64, int64_t)
//        }
//        return false;
//    case Type::F16:
//    case Type::F32:
//    case Type::F64:
//        if (const auto real = val.GetFP(); real.has_value())
//        {
//            if (ElementType == Type::F16)
//            {
//                reinterpret_cast<half_float::half*>(DataPtr)[idx] = static_cast<float>(real.value());
//                return true;
//            }
//            TYPESET(F32, float)
//            TYPESET(F64, double)
//        }
//        return false;
//    case Type::Str8:
//    case Type::Str16:
//    case Type::Str32:
//        if (const auto real = val.GetStr(); real.has_value())
//        {
//            using namespace common::str;
//            if (ElementType == Type::Str8)
//            {
//                reinterpret_cast<std::string*>(DataPtr)[idx] = common::str::to_string(real.value(), Charset::UTF8);
//                return true;
//            }
//            if (ElementType == Type::Str16)
//            {
//                reinterpret_cast<std::u16string*>(DataPtr)[idx] = common::str::to_u16string(real.value(), Charset::UTF16);
//                return true;
//            }
//            if (ElementType == Type::Str32)
//            {
//                reinterpret_cast<std::u32string*>(DataPtr)[idx].assign(real.value());
//                return true;
//            }
//        }
//        return false;
//    default: 
//        return false;
//    }
//#undef TYPESET
//}


std::u32string_view NativeWrapper::TypeName(NativeWrapper::Type type) noexcept
{
#define RET(tenum) case Type::tenum: return U"" STRINGIZE(tenum) ""sv
    switch (type)
    {
    RET(Any);
    RET(Bool);
    RET(U8);
    RET(I8);
    RET(U16);
    RET(I16);
    RET(F16);
    RET(U32);
    RET(I32);
    RET(F32);
    RET(U64);
    RET(I64);
    RET(F64);
    RET(Str8);
    RET(Str16);
    RET(Str32);
    RET(Sv8);
    RET(Sv16);
    RET(Sv32);
    default: return U"Unknown"sv;
    }
#undef RET
}

ArgLocator NativeWrapper::GetLocator(Type type, uintptr_t pointer, bool isConst, size_t idx) noexcept
{
    using common::str::Charset;
    using common::str::to_u32string;
    using std::u32string;
    using std::u32string_view;
    using std::u16string;
    using std::u16string_view;
    using std::string;
    using std::string_view;
#if defined(__cpp_char8_t) && __cpp_char8_t >= 201811L
    using std::u8string;
    using std::u8string_view;
#endif
#define GetPtr(type) reinterpret_cast<type*>(pointer) + idx
#define RetNum(tenum, type, ttype, func) case Type::tenum: return {                 \
        [ptr = GetPtr(const type)]() -> Arg { return static_cast<ttype>(*ptr); },   \
        isConst ? ArgLocator::Setter{} : [ptr = GetPtr(type)](Arg val)              \
        {                                                                           \
            if (const auto real = val.func(); real.has_value())                     \
            {                                                                       \
                *ptr = static_cast<type>(real.value());                             \
                return true;                                                        \
            }                                                                       \
            return false;                                                           \
        }, 1 }
#define RetStr(tenum, type, ch) case Type::tenum: return {                          \
        [ptr = GetPtr(const type)]() -> Arg { return to_u32string(*ptr); },         \
        isConst ? ArgLocator::Setter{} : [ptr = GetPtr(type)](Arg val)              \
        {                                                                           \
            if (const auto real = val.GetStr(); real.has_value())                   \
            {                                                                       \
                *ptr = common::str::to_##type(*real, Charset::ch);                  \
                return true;                                                        \
            }                                                                       \
            return false;                                                           \
        }, 1 }
#define RetSv(tenum, type, ch) case Type::tenum: return \
    { [ptr = GetPtr(const type##_view)]() -> Arg { return to_u32string(*ptr); }, {}, 1 }

    switch (type)
    {
    RetNum(Bool, bool,             bool,      GetBool);
    RetNum(U8,   uint8_t,          uint64_t,  GetUint);
    RetNum(U16,  uint16_t,         uint64_t,  GetUint);
    RetNum(U32,  uint32_t,         uint64_t,  GetUint);
    RetNum(U64,  uint64_t,         uint64_t,  GetUint);
    RetNum(I8,   int8_t,           int64_t,   GetInt);
    RetNum(I16,  int16_t,          int64_t,   GetInt);
    RetNum(I32,  int32_t,          int64_t,   GetInt);
    RetNum(I64,  int64_t,          int64_t,   GetInt);
    RetNum(F16,  half_float::half, double,    GetFP);
    RetNum(F32,  float,            double,    GetFP);
    RetNum(F64,  double,           double,    GetFP);
    case Type::Sv32:  return 
        { [ptr = GetPtr(const u32string_view)] () -> Arg { return *ptr; }, {}, 1 };
    case Type::Str32: return { 
        [ptr = GetPtr(const u32string)]() -> Arg { return *ptr; },
        isConst ? ArgLocator::Setter{} : [ptr = GetPtr(u32string)](Arg val)
        {
            if (const auto real = val.GetStr(); real.has_value())
            {
                *ptr = real.value();
                return true;
            }
            return false;
        }, 1 };
#if defined(__cpp_char8_t) && __cpp_char8_t >= 201811L
    RetStr(Str8,  u8string,   UTF8);
    RetSv (Sv8,   u8string,   UTF8);
#else
    RetStr(Str8, string, UTF8);
    RetSv (Sv8,  string, UTF8);
#endif
    RetStr(Str16, u16string,  UTF16);
    RetSv (Sv16,  u16string,  UTF16);
    case Type::Any:
        if (isConst)
            return { GetPtr(const Arg), 1 };
        else
            return { GetPtr(Arg), 1 };
    default: return {};
    }
#undef RetSv
#undef RetStr
#undef RetNum
#undef GetPtr
}


#define NATIVE_TYPE_MAP BOOST_PP_VARIADIC_TO_SEQ(   \
    (Any,    Arg),                  \
    (Bool,   bool),                 \
    (U8,     uint8_t),              \
    (I8,     int8_t),               \
    (U16,    uint16_t),             \
    (I16,    int16_t),              \
    (F16,    half_float::half),     \
    (U32,    uint32_t),             \
    (I32,    int32_t),              \
    (F32,    float),                \
    (U64,    uint64_t),             \
    (I64,    int64_t),              \
    (F64,    double),               \
    (Str8,   std::string),          \
    (Str16,  std::u16string),       \
    (Str32,  std::u32string),       \
    (Sv8,    std::string_view),     \
    (Sv16,   std::u16string_view),  \
    (Sv32,   std::u32string_view))

#define NATIVE_TYPE_EACH_(r, func, tp) func(BOOST_PP_TUPLE_ELEM(2, 0, tp), BOOST_PP_TUPLE_ELEM(2, 1, tp))
#define NATIVE_TYPE_EACH(func) BOOST_PP_SEQ_FOR_EACH(NATIVE_TYPE_EACH_, func, NATIVE_TYPE_MAP)

FixedArray::SpanVariant FixedArray::GetSpan() const noexcept
{
#define SP(type) common::span<type>{ reinterpret_cast<type*>(DataPtr), static_cast<size_t>(Length) }
#define RET(tenum, type) case Type::tenum:      \
        if (IsReadOnly) return SP(const type);  \
        else return SP(type);                   \

    switch (ElementType)
    {
    NATIVE_TYPE_EACH(RET)
    default: return {};
    }
#undef RET
#undef SP
}

void FixedArray::PrintToStr(std::u32string& str, std::u32string_view delim) const
{
    constexpr auto Append = [](std::u32string& str, auto delim, auto ptr, const size_t len)
    {
        for (uint32_t idx = 0; idx < len; ++idx)
        {
            if (idx > 0) str.append(delim);
            if constexpr (std::is_same_v<const Arg&, decltype(*ptr)>)
                str.append(ptr[idx].ToString().StrView());
            else
                fmt::format_to(std::back_inserter(str), FMT_STRING(U"{}"), ptr[idx]);
        }
    };
#define PRT(tenum, type) case Type::tenum:                              \
    Append(str, delim, reinterpret_cast<const type*>(DataPtr), Length); \
    return;

    switch (ElementType)
    {
    NATIVE_TYPE_EACH(PRT)
    default: return;
    }
#undef PRT
}

#undef NATIVE_TYPE_EACH
#undef NATIVE_TYPE_EACH_
#undef NATIVE_TYPE_MAP


Arg::Arg(const Arg& other) noexcept :
    Data0(other.Data0), Data1(other.Data1), Data2(other.Data2), Data3(other.Data3), TypeData(other.TypeData)
{
    if (TypeData == Type::U32Str && Data1 > 0)
    {
        auto str = other.GetVar<Type::U32Str>();
        common::SharedString<char32_t>::PlacedIncrease(str);
    }
    else if (TypeData == Type::Var)
    {
        auto& var = GetCustom();
        var.Host->IncreaseRef(var);
    }
}

Arg& Arg::operator=(const Arg& other) noexcept
{
    Release();
    Data0 = other.Data0;
    Data1 = other.Data1;
    Data2 = other.Data2;
    Data3 = other.Data3;
    TypeData = other.TypeData;
    if (TypeData == Type::U32Str && Data1 > 0)
    {
        auto str = other.GetVar<Type::U32Str>();
        common::SharedString<char32_t>::PlacedIncrease(str);
    }
    else if (TypeData == Type::Var)
    {
        auto& var = GetCustom();
        var.Host->IncreaseRef(var);
    }
    return *this;
}

void Arg::Release() noexcept
{
    if (TypeData == Type::U32Str && Data1 > 0)
    {
        auto str = GetVar<Type::U32Str>();
        common::SharedString<char32_t>::PlacedDecrease(str);
    }
    else if (TypeData == Type::Var)
    {
        auto& var = GetCustom();
        var.Host->DecreaseRef(var);
    }
    TypeData = Type::Empty;
}

std::optional<bool> Arg::GetBool() const noexcept
{
    switch (TypeData)
    {
    case Type::Uint:    return  GetVar<Type::Uint>() != 0;
    case Type::Int:     return  GetVar<Type::Int>() != 0;
    case Type::FP:      return  GetVar<Type::FP>() != 0;
    case Type::Bool:    return  GetVar<Type::Bool>();
    case Type::U32Str:  return !GetVar<Type::U32Str>().empty();
    case Type::U32Sv:   return !GetVar<Type::U32Sv>().empty();
    case Type::Var:     return  ConvertCustomType(Type::Bool).GetBool();
    default:            return {};
    }
}
std::optional<uint64_t> Arg::GetUint() const noexcept
{
    switch (TypeData)
    {
    case Type::Uint:    return GetVar<Type::Uint>();
    case Type::Int:     return static_cast<uint64_t>(GetVar<Type::Int>());
    case Type::FP:      return static_cast<uint64_t>(GetVar<Type::FP>());
    case Type::Bool:    return GetVar<Type::Bool>() ? 1 : 0;
    case Type::Var:     return ConvertCustomType(Type::Uint).GetUint();
    default:            return {};
    }
}
std::optional<int64_t> Arg::GetInt() const noexcept
{
    switch (TypeData)
    {
    case Type::Uint:    return static_cast<int64_t>(GetVar<Type::Uint>());
    case Type::Int:     return GetVar<Type::Int>();
    case Type::FP:      return static_cast<int64_t>(GetVar<Type::FP>());
    case Type::Bool:    return GetVar<Type::Bool>() ? 1 : 0;
    case Type::Var:     return ConvertCustomType(Type::Int).GetInt();
    default:            return {};
    }
}
std::optional<double> Arg::GetFP() const noexcept
{
    switch (TypeData)
    {
    case Type::Uint:    return static_cast<double>(GetVar<Type::Uint>());
    case Type::Int:     return static_cast<double>(GetVar<Type::Int>());
    case Type::FP:      return GetVar<Type::FP>();
    case Type::Bool:    return GetVar<Type::Bool>() ? 1. : 0.;
    case Type::Var:     return ConvertCustomType(Type::FP).GetFP();
    default:            return {};
    }
}
std::optional<std::u32string_view> Arg::GetStr() const noexcept
{
    switch (TypeData)
    {
    case Type::U32Str:  return GetVar<Type::U32Str>();
    case Type::U32Sv:   return GetVar<Type::U32Sv>();
    case Type::Var:     return ConvertCustomType(Type::U32Sv).GetStr();
    default:            return {};
    }
}

common::str::StrVariant<char32_t> Arg::ToString() const noexcept
{
    return Visit([](const auto& val) -> common::str::StrVariant<char32_t>
        {
            using T = std::decay_t<decltype(val)>;
            if constexpr (std::is_same_v<T, CustomVar>)
                return val.Host->ToString(val);
            else if constexpr (std::is_same_v<T, FixedArray>)
            {
                std::u32string ret = U"[";
                val.PrintToStr(ret, U", "sv);
                /*for (uint64_t i = 0; i < val.Length; ++i)
                {
                    if (i > 0)
                        ret.append(U", "sv);
                    ret.append(val.Get(i).ToString().StrView());
                }*/
                ret.append(U"]"sv);
                return std::move(ret);
            }
            else if constexpr (std::is_same_v<T, std::nullopt_t>)
                return {};
            else if constexpr (std::is_same_v<T, std::u32string_view>)
                return val;
            else
                return fmt::format(FMT_STRING(U"{}"), val);
        });
}

std::u32string_view Arg::TypeName(const Arg::Type type) noexcept
{
    switch (type)
    {
    case Arg::Type::Empty:    return U"empty"sv;
    case Arg::Type::Var:      return U"variable"sv;
    case Arg::Type::U32Str:   return U"string"sv;
    case Arg::Type::U32Sv:    return U"string_view"sv;
    case Arg::Type::Uint:     return U"uint"sv;
    case Arg::Type::Int:      return U"int"sv;
    case Arg::Type::FP:       return U"fp"sv;
    case Arg::Type::Bool:     return U"bool"sv;
    case Arg::Type::Custom:   return U"custom"sv;
    case Arg::Type::Boolable: return U"boolable"sv;
    case Arg::Type::String:   return U"string-ish"sv;
    case Arg::Type::Number:   return U"number"sv;
    case Arg::Type::Integer:  return U"integer"sv;
    default:                  return U"error"sv;
    }
}


void CustomVar::Handler::IncreaseRef(CustomVar&) noexcept { }
void CustomVar::Handler::DecreaseRef(CustomVar&) noexcept { }
Arg CustomVar::Handler::IndexerGetter(const CustomVar&, const Arg&, const RawArg&) 
{ 
    return {};
}
Arg CustomVar::Handler::SubfieldGetter(const CustomVar&, std::u32string_view)
{
    return {};
}
bool CustomVar::Handler::HandleAssign(CustomVar&, Arg)
{
    return false;
}
common::str::StrVariant<char32_t> CustomVar::Handler::ToString(const CustomVar&) noexcept
{
    return GetTypeName();
}
Arg CustomVar::Handler::ConvertToCommon(const CustomVar&, Arg::Type) noexcept
{
    return {};
}
std::u32string_view CustomVar::Handler::GetTypeName() noexcept 
{ 
    return U"CustomVar"sv;
}


ArgLocator::ArgLocator(Getter getter, Setter setter, uint32_t consumed) noexcept : 
    Val{}, Consumed(consumed), Type(LocateType::GetSet), Flag(LocateFlags::Empty)
{
    if (const bool bg = (bool)getter, bs = (bool)setter; bg == bs)
        Flag = bg ? LocateFlags::ReadWrite : LocateFlags::Empty;
    else
        Flag = bg ? LocateFlags::ReadOnly : LocateFlags::WriteOnly;
    if (Flag == LocateFlags::Empty)
        Type = LocateType::Empty;
    else
    {
        auto ptr = new GetSet(std::move(getter), std::move(setter));
        Val = PointerToVal(ptr);
    }
}
ArgLocator::~ArgLocator()
{
    if (Type == LocateType::GetSet)
    {
        auto ptr = reinterpret_cast<GetSet*>(static_cast<uintptr_t>(Val.GetUint().value()));
        delete ptr;
    }
}
Arg ArgLocator::Get() const
{
    switch (Type)
    {
    case LocateType::Ptr:       return *reinterpret_cast<const Arg*>(static_cast<uintptr_t>(Val.GetUint().value()));
    case LocateType::GetSet:    return reinterpret_cast<const GetSet*>(static_cast<uintptr_t>(Val.GetUint().value()))->Get();
    case LocateType::Arg:       return Val;
    default:                    return {};
    }
}
Arg ArgLocator::ExtractGet()
{
    switch (Type)
    {
    case LocateType::Ptr:       return *reinterpret_cast<const Arg*>(static_cast<uintptr_t>(Val.GetUint().value()));
    case LocateType::GetSet:    return reinterpret_cast<const GetSet*>(static_cast<uintptr_t>(Val.GetUint().value()))->Get();
    case LocateType::Arg:       return std::move(Val);
    default:                    return {};
    }
}
bool ArgLocator::Set(Arg val)
{
    switch (Type)
    {
    case LocateType::Ptr:
        *reinterpret_cast<Arg*>(static_cast<uintptr_t>(Val.GetUint().value())) = std::move(val); 
        return true;
    case LocateType::GetSet:
        reinterpret_cast<const GetSet*>(static_cast<uintptr_t>(Val.GetUint().value()))->Set(std::move(val));
        return true;
    case LocateType::Arg:
        if (Val.IsCustom())
            return Val.GetCustom().Call<&CustomVar::Handler::HandleAssign>(std::move(val));
        return false;
    default:
        return false;
    }
}
ArgLocator ArgLocator::HandleQuery(SubQuery subq, NailangRuntimeBase& runtime)
{
    switch (Type)
    {
    case LocateType::Ptr:    return reinterpret_cast<Arg*>(static_cast<uintptr_t>(*Val.GetUint()))->HandleQuery(subq, runtime);
    case LocateType::GetSet: return reinterpret_cast<const GetSet*>(static_cast<uintptr_t>(*Val.GetUint()))->Get().HandleQuery(subq, runtime);
    case LocateType::Arg:    return Val.HandleQuery(subq, runtime);
    default:                 return {};
    }
}


void Serializer::Stringify(std::u32string& output, const SubQuery& subq)
{
    for (size_t i = 0; i < subq.Size(); ++i)
    {
        const auto [type, query] = subq[i];
        switch (type)
        {
        case SubQuery::QueryType::Index:
            output.push_back(U'[');
            Stringify(output, query, false);
            output.push_back(U']');
            break;
        case SubQuery::QueryType::Sub:
            output.push_back(U'.');
            output.append(query.GetVar<RawArg::Type::Str>());
            break;
        default:
            break;
        }
    }
}

void Serializer::Stringify(std::u32string& output, const RawArg& arg, const bool requestParenthese)
{
    arg.Visit([&](const auto& data)
        {
            using T = std::decay_t<decltype(data)>;
            if constexpr (std::is_same_v<T, std::nullopt_t>)
                Expects(false);
            else if constexpr (std::is_same_v<T, const BinaryExpr*>)
                Stringify(output, data, requestParenthese);
            else
                Stringify(output, data);
        });
}

void Serializer::Stringify(std::u32string& output, const FuncCall* call)
{
    output.append(*call->Name).push_back(U'(');
    size_t i = 0;
    for (const auto& arg : call->Args)
    {
        if (i++ > 0)
            output.append(U", "sv);
        Stringify(output, arg);
    }
    output.push_back(U')');
}

void Serializer::Stringify(std::u32string& output, const UnaryExpr* expr)
{
    Expects(EmbedOpHelper::IsUnaryOp(expr->Operator));
    switch (expr->Operator)
    {
    case EmbedOps::Not:
        output.append(U"!"sv);
        Stringify(output, expr->Oprend, true);
        break;
    default:
        Expects(false);
        break;
    }
}

void Serializer::Stringify(std::u32string& output, const BinaryExpr* expr, const bool requestParenthese)
{
    Expects(!EmbedOpHelper::IsUnaryOp(expr->Operator));
    std::u32string_view opStr;
#define SET_OP_STR(op, str) case EmbedOps::op: opStr = U##str##sv; break;
    switch (expr->Operator)
    {
        SET_OP_STR(Equal,       " == ");
        SET_OP_STR(NotEqual,    " != ");
        SET_OP_STR(Less,        " < ");
        SET_OP_STR(LessEqual,   " <= ");
        SET_OP_STR(Greater,     " > ");
        SET_OP_STR(GreaterEqual," >= ");
        SET_OP_STR(And,         " && ");
        SET_OP_STR(Or,          " || ");
        SET_OP_STR(Add,         " + ");
        SET_OP_STR(Sub,         " - ");
        SET_OP_STR(Mul,         " * ");
        SET_OP_STR(Div,         " / ");
        SET_OP_STR(Rem,         " % ");
    default:
        assert(false); // Expects(false);
        return;
    }
#undef SET_OP_STR
    if (requestParenthese)
        output.push_back(U'(');
    Stringify(output, expr->LeftOprend, true);
    output.append(opStr);
    Stringify(output, expr->RightOprend, true);
    if (requestParenthese)
        output.push_back(U')');
}

void Serializer::Stringify(std::u32string& output, const QueryExpr* expr)
{
    Stringify(output, expr->Target, true);
    Stringify(output, *expr);
}

void Serializer::Stringify(std::u32string& output, const LateBindVar& var)
{
    if (HAS_FIELD(var.Info, LateBindVar::VarInfo::Root))
        output.push_back(U'`');
    else if (HAS_FIELD(var.Info, LateBindVar::VarInfo::Local))
        output.push_back(U':');
    output.append(var.Name);
}

void Serializer::Stringify(std::u32string& output, const std::u32string_view str)
{
    output.push_back(U'"');
    output.append(str); //TODO:
    output.push_back(U'"');
}

void Serializer::Stringify(std::u32string& output, const uint64_t u64)
{
    const auto str = fmt::to_string(u64);
    output.insert(output.end(), str.begin(), str.end());
}

void Serializer::Stringify(std::u32string& output, const int64_t i64)
{
    const auto str = fmt::to_string(i64);
    output.insert(output.end(), str.begin(), str.end());
}

void Serializer::Stringify(std::u32string& output, const double f64)
{
    const auto str = fmt::to_string(f64);
    output.insert(output.end(), str.begin(), str.end());
}

void Serializer::Stringify(std::u32string& output, const bool boolean)
{
    output.append(boolean ? U"true"sv : U"false"sv);
}


AutoVarHandlerBase::Accessor::Accessor() noexcept :
    AutoMember{}, IsSimple(false), IsConst(true)
{ }
AutoVarHandlerBase::Accessor::Accessor(Accessor&& other) noexcept : 
    AutoMember{}, IsSimple(other.IsSimple), IsConst(other.IsConst)
{
    if (IsSimple)
    {
        this->SimpleMember = std::move(other.SimpleMember);
    }
    else
    {
        this->AutoMember = std::move(other.AutoMember);
    }
}
void AutoVarHandlerBase::Accessor::SetCustom(std::function<CustomVar(void*)> accessor) noexcept
{
    IsSimple = false;
    IsConst = true;
    this->AutoMember = std::move(accessor);
}
void AutoVarHandlerBase::Accessor::SetGetSet(std::function<Arg(void*)> getter, std::function<void(void*, Arg)> setter) noexcept
{
    IsSimple = true;
    IsConst = !(bool)setter;
    this->SimpleMember = std::make_pair(std::move(getter), std::move(setter));
}
AutoVarHandlerBase::Accessor::~Accessor()
{ }
AutoVarHandlerBase::AccessorBuilder& AutoVarHandlerBase::AccessorBuilder::SetConst(bool isConst)
{
    if (!isConst && Host.IsSimple && !Host.SimpleMember.second)
        COMMON_THROW(common::BaseException, u"No setter provided");
    Host.IsConst = isConst;
    return *this;
}


AutoVarHandlerBase::AutoVarHandlerBase(std::u32string_view typeName, size_t typeSize) : TypeName(typeName), TypeSize(typeSize)
{ }
AutoVarHandlerBase::~AutoVarHandlerBase() { }
bool AutoVarHandlerBase::HandleAssign(CustomVar& var, Arg val)
{
    if (var.Meta1 < UINT32_MAX) // is array
        return false;
    if (!Assigner)
        return false;
    const auto ptr = reinterpret_cast<void*>(var.Meta0);
    Assigner(ptr, std::move(val));
    return true;
}

AutoVarHandlerBase::Accessor* AutoVarHandlerBase::FindMember(std::u32string_view name, bool create)
{
    const common::str::HashedStrView hsv(name);
    for (auto& [pos, acc] : MemberList)
    {
        if (NamePool.GetHashedStr(pos) == hsv)
            return &acc;
    }
    if (create)
    {
        const auto piece = NamePool.AllocateString(name);
        return &MemberList.emplace_back(piece, Accessor{}).second;
    }
    return nullptr;
}


}
