#pragma once
#include "NailangStruct.h"
#include "SystemCommon/Exceptions.h"
#include "common/StringPool.hpp"
#include "common/STLEx.hpp"
#include <boost/container/small_vector.hpp>
#include <map>
#include <vector>
#include <memory>

#if COMMON_COMPILER_MSVC
#   pragma warning(push)
#   pragma warning(disable:4275 4251)
#endif

namespace xziar::nailang
{
class NAILANGAPI NailangRuntime;


struct LocalFunc
{
    const Block* Body;
    common::span<const std::u32string_view> ArgNames;
    common::span<const Arg> CapturedArgs;

    [[nodiscard]] constexpr explicit operator bool() const noexcept { return Body != nullptr; }
};


class NAILANGAPI EvaluateContext
{
    friend NailangRuntime;
public:
    virtual ~EvaluateContext();
    /**
     * @brief locate the arg, create if necessary, return argptr, or empty if not found
     * @return argptr | empty
    */
    [[nodiscard]] virtual Arg LocateArg(const LateBindVar& var, const bool create) noexcept = 0;
    [[nodiscard]] virtual LocalFunc LookUpFunc(std::u32string_view name) const = 0;
    virtual bool SetFunc(const Block* block, common::span<std::pair<std::u32string_view, Arg>> capture, common::span<const Expr> args) = 0;
    virtual bool SetFunc(const Block* block, common::span<std::pair<std::u32string_view, Arg>> capture, common::span<const std::u32string_view> args) = 0;
    [[nodiscard]] virtual size_t GetArgCount() const noexcept = 0;
    [[nodiscard]] virtual size_t GetFuncCount() const noexcept = 0;
};

class NAILANGAPI BasicEvaluateContext : public EvaluateContext
{
private:
    std::vector<std::u32string_view> LocalFuncArgNames;
    std::vector<Arg> LocalFuncCapturedArgs;
    std::pair<uint32_t, uint32_t> InsertCaptures(
        common::span<std::pair<std::u32string_view, Arg>> capture);
    template<typename T>
    std::pair<uint32_t, uint32_t> InsertNames(
        common::span<std::pair<std::u32string_view, Arg>> capture, common::span<const T> argNames);
protected:
    using LocalFuncHolder = std::tuple<const Block*, std::pair<uint32_t, uint32_t>, std::pair<uint32_t, uint32_t>>;
    [[nodiscard]] virtual LocalFuncHolder LookUpFuncInside(std::u32string_view name) const = 0;
    virtual bool SetFuncInside(std::u32string_view name, LocalFuncHolder func) = 0;
public:
    ~BasicEvaluateContext() override;

    [[nodiscard]] LocalFunc LookUpFunc(std::u32string_view name) const override;
    bool SetFunc(const Block* block, common::span<std::pair<std::u32string_view, Arg>> capture, common::span<const Expr> args) override;
    bool SetFunc(const Block* block, common::span<std::pair<std::u32string_view, Arg>> capture, common::span<const std::u32string_view> args) override;
};

class NAILANGAPI LargeEvaluateContext : public BasicEvaluateContext
{
protected:
    std::map<std::u32string, Arg, std::less<>> ArgMap;
    std::map<std::u32string_view, LocalFuncHolder, std::less<>> LocalFuncMap;

    [[nodiscard]] LocalFuncHolder LookUpFuncInside(std::u32string_view name) const override;
    bool SetFuncInside(std::u32string_view name, LocalFuncHolder func) override;
public:
    ~LargeEvaluateContext() override;

    [[nodiscard]] Arg    LocateArg(const LateBindVar& var, const bool create) noexcept override;
    [[nodiscard]] size_t GetArgCount() const noexcept override;
    [[nodiscard]] size_t GetFuncCount() const noexcept override;
};

class NAILANGAPI CompactEvaluateContext : public BasicEvaluateContext
{
private:
    common::HashedStringPool<char32_t> ArgNames;
protected:
    std::vector<std::pair<common::StringPiece<char32_t>, Arg>> Args;
    std::vector<std::pair<std::u32string_view, LocalFuncHolder>> LocalFuncs;

    std::u32string_view GetStringView(common::StringPiece<char32_t> piece) const noexcept
    {
        return ArgNames.GetStringView(piece);
    }
    [[nodiscard]] LocalFuncHolder LookUpFuncInside(std::u32string_view name) const override;
    bool SetFuncInside(std::u32string_view name, LocalFuncHolder func) override;
public:
    ~CompactEvaluateContext() override;

    [[nodiscard]] Arg    LocateArg(const LateBindVar& var, const bool create) noexcept override;
    [[nodiscard]] size_t GetArgCount() const noexcept override;
    [[nodiscard]] size_t GetFuncCount() const noexcept override;
};


namespace detail
{
struct ExceptionTarget
{
    enum class Type { Empty, Arg, Expr, AssignExpr, FuncCall, RawBlock, Block };
    using VType = std::variant<std::monostate, Statement, Arg, Expr, FuncCall, const RawBlock*, const Block*>;
    VType Target;

    constexpr ExceptionTarget() noexcept {}
    ExceptionTarget(const CustomVar& arg) noexcept : Target{ arg.CopyToArg() } {}
    template<typename T>
    constexpr ExceptionTarget(T&& arg, std::enable_if_t<common::VariantHelper<VType>::Contains<std::decay_t<T>>()>* = nullptr) noexcept
        : Target{ std::forward<T>(arg) } {}
    template<typename T>
    constexpr ExceptionTarget(T&& arg, std::enable_if_t<common::is_specialization<std::decay_t<T>, std::variant>::value>* = nullptr) noexcept
        : Target{ common::VariantHelper<VType>::Convert(std::forward<T>(arg)) } {}
    template<typename T>
    constexpr ExceptionTarget(T&& arg, std::enable_if_t<std::is_base_of_v<FuncCall, std::decay_t<T>> && !std::is_same_v<std::decay_t<T>, FuncCall>>* = nullptr) noexcept
        : Target{ static_cast<const FuncCall&>(arg) } {}

    constexpr explicit operator bool() const noexcept { return Target.index() != 0; }
    [[nodiscard]] constexpr Type GetType() const noexcept
    {
        switch (Target.index())
        {
        case 0:  return Type::Empty;
        case 2:  return Type::Arg;
        case 3:  return Type::Expr;
        case 4:  return Type::FuncCall;
        case 5:  return Type::RawBlock;
        case 6:  return Type::Block;
        case 1:
            switch (std::get<1>(Target).TypeData)
            {
            case Statement::Type::Assign:   return Type::AssignExpr;
            case Statement::Type::FuncCall: return Type::FuncCall;
            case Statement::Type::RawBlock: return Type::RawBlock;
            case Statement::Type::Block:    return Type::Block;
            default:                        return Type::Empty;
            }
        default: return Type::Empty;
        }
    }
    template<Type T>
    [[nodiscard]] constexpr auto GetVar() const
    {
        Expects(GetType() == T);
        if constexpr (T == Type::Empty)
            return;
        else if constexpr (T == Type::Arg)
            return std::get<2>(Target);
        else if constexpr (T == Type::Expr)
            return std::get<3>(Target);
        else if constexpr (T == Type::AssignExpr)
            return std::get<1>(Target).Get<AssignExpr>();
        else if constexpr (T == Type::RawBlock)
        {
            switch (Target.index())
            {
            case 5:  return std::get<5>(Target);
            case 1:  return std::get<1>(Target).Get<RawBlock>();
            default: return nullptr;
            }
        }
        else if constexpr (T == Type::Block)
        {
            switch (Target.index())
            {
            case 6:  return std::get<6>(Target);
            case 1:  return std::get<1>(Target).Get<Block>();
            default: return nullptr;
            }
        }
        else if constexpr (T == Type::FuncCall)
        {
            switch (Target.index())
            {
            case 4:  return &std::get<4>(Target);
            case 1:  return  std::get<1>(Target).Get<FuncCall>();
            default: return nullptr;
            }
        }
        else
            static_assert(!common::AlwaysTrue2<T>, "");
    }
};
}


class NAILANGAPI NailangRuntimeException : public common::BaseException
{
    friend NailangRuntime;
    PREPARE_EXCEPTION(NailangRuntimeException, BaseException,
        detail::ExceptionTarget Target;
        detail::ExceptionTarget Scope;
        ExceptionInfo(const std::u16string_view msg, detail::ExceptionTarget target, detail::ExceptionTarget scope)
            : ExceptionInfo(TYPENAME, msg, target, scope)
        { }
    protected:
        template<typename T>
        ExceptionInfo(const char* type, T&& msg, detail::ExceptionTarget target, detail::ExceptionTarget scope)
            : TPInfo(type, std::forward<T>(msg)), Target(std::move(target)), Scope(std::move(scope))
        { }
    );
public:
    NailangRuntimeException(const std::u16string_view msg, detail::ExceptionTarget target = {}, detail::ExceptionTarget scope = {}) :
        NailangRuntimeException(T_<ExceptionInfo>{}, msg, std::move(target), std::move(scope))
    { }
};

class NAILANGAPI NailangFormatException : public NailangRuntimeException
{
    friend NailangRuntime;
    PREPARE_EXCEPTION(NailangFormatException, NailangRuntimeException,
        std::u32string Formatter;
        ExceptionInfo(const std::u16string_view msg, const std::u32string_view formatter, detail::ExceptionTarget target = {})
            : ExceptionInfo(TYPENAME, msg, formatter, target)
        { }
    protected:
        ExceptionInfo(const char* type, const std::u16string_view msg, const std::u32string_view formatter, detail::ExceptionTarget target)
            : TPInfo(type, msg, target, {}), Formatter(formatter)
        { }
    );
public:
    NailangFormatException(const std::u32string_view formatter, const std::runtime_error& err);
    NailangFormatException(const std::u32string_view formatter, const Arg& arg, const std::u16string_view reason = u"");
};

class NAILANGAPI NailangCodeException : public NailangRuntimeException
{
    PREPARE_EXCEPTION(NailangCodeException, NailangRuntimeException,
        ExceptionInfo(const std::u32string_view msg, detail::ExceptionTarget target, detail::ExceptionTarget scope)
            : ExceptionInfo(TYPENAME, msg, target, scope)
        { }
    protected:
        ExceptionInfo(const char* type, const std::u32string_view msg, detail::ExceptionTarget target, detail::ExceptionTarget scope);
    );
public:
    NailangCodeException(const std::u32string_view msg, detail::ExceptionTarget target = {}, detail::ExceptionTarget scope = {});
};


enum class ArgLimits { Exact, AtMost, AtLeast };


struct NAILANGAPI NailangHelper
{
    [[nodiscard]] static size_t BiDirIndexCheck(const size_t size, const Arg& idx, const Expr* src = nullptr);
    /*static Arg ExtractArg(Arg& target, QueryExpr query, NailangExecutor& executor);
    template<typename F>
    static bool LocateWrite(Arg& target, QueryExpr query, NailangExecutor& executor, const F& func);
    template<bool IsWrite, typename F>
    static auto LocateAndExecute(Arg& target, QueryExpr query, NailangExecutor& executor, const F& func)
        -> decltype(func(std::declval<Arg&>()));*/
};


struct FuncPack : public FuncCall
{
    common::span<Arg> Params;
    constexpr FuncPack(const FuncCall& call, common::span<Arg> params) noexcept :
        FuncCall(call), Params(params) { }
    auto NamePart(size_t idx) const { return Name->GetPart(idx); }
    constexpr size_t NamePartCount() const noexcept { return Name->PartCount; }
    common::Optional<Arg&> TryGet(size_t idx) noexcept
    {
        if (idx < Params.size())
            return { std::in_place_t{}, &Params[idx] };
        else
            return {};
    }
    common::Optional<const Arg&> TryGet(size_t idx) const noexcept
    {
        if (idx < Params.size())
            return { std::in_place_t{}, &Params[idx] };
        else
            return {};
    }
    template<typename... Args, typename R>
    common::OptionalT<R> TryGet(size_t idx, R(Arg::* func)(Args...), Args&&... args)
    {
        if (idx < Params.size())
            return { common::detail::OptionalT<R>::Create((Params[idx].*func)(std::forward<Args>(args)...)) };
        else
            return {};
    }
    template<typename... Args, typename R>
    common::OptionalT<R> TryGet(size_t idx, R(Arg::* func)(Args...) noexcept, Args&&... args) noexcept
    {
        if (idx < Params.size())
            return { common::detail::OptionalT<R>::Create((Params[idx].*func)(std::forward<Args>(args)...)) };
        else
            return {};
    }
    template<typename... Args, typename R>
    common::OptionalT<R> TryGet(size_t idx, R(Arg::* func)(Args...) const, Args&&... args) const
    {
        if (idx < Params.size())
            return { common::detail::OptionalT<R>::Create((Params[idx].*func)(std::forward<Args>(args)...)) };
        else
            return {};
    }
    template<typename... Args, typename R>
    common::OptionalT<R> TryGet(size_t idx, R(Arg::* func)(Args...) const noexcept, Args&&... args) const noexcept
    {
        if (idx < Params.size())
            return { common::detail::OptionalT<R>::Create((Params[idx].*func)(std::forward<Args>(args)...)) };
        else
            return {};
    }
    template<typename... Args, typename R>
    R TryGetOr(size_t idx, R(Arg::* func)(Args...), R def, Args&&... args)
    {
        if (idx < Params.size())
            return (Params[idx].*func)(std::forward<Args>(args)...);
        else
            return std::move(def);
    }
    template<typename... Args, typename R>
    R TryGetOr(size_t idx, R(Arg::* func)(Args...) noexcept, R def, Args&&... args) noexcept
    {
        if (idx < Params.size())
            return (Params[idx].*func)(std::forward<Args>(args)...);
        else
            return std::move(def);
    }
    template<typename... Args, typename R>
    R TryGetOr(size_t idx, R(Arg::* func)(Args...) const, R def, Args&&... args) const
    {
        if (idx < Params.size())
            return (Params[idx].*func)(std::forward<Args>(args)...);
        else
            return std::move(def);
    }
    template<typename... Args, typename R>
    R TryGetOr(size_t idx, R(Arg::* func)(Args...) const noexcept, R def, Args&&... args) const noexcept
    {
        if (idx < Params.size())
            return (Params[idx].*func)(std::forward<Args>(args)...);
        else
            return std::move(def);
    }
};


template<typename T>
class FlaggedItems
{
protected:
    struct ItemWrapper
    {
        const FlaggedItems& Host;
        size_t Index;
        T* operator->() const noexcept { return &Host.Container[Index]; }
        T& operator*()  const noexcept { return  Host.Container[Index]; }
        bool CheckFlag() const noexcept { return Host.FlagBitmap.Get(Index); }
        void SetFlag(bool value) noexcept { Host.FlagBitmap.Set(Index, value); }
    };
    [[nodiscard]] constexpr ItemWrapper Get(size_t index) const noexcept
    {
        return { *this, index };
    }
    using ItType = common::container::IndirectIterator<const FlaggedItems<T>, ItemWrapper, &FlaggedItems<T>::Get>;
    friend ItType;
    mutable common::SmallBitset FlagBitmap;
    common::span<T> Container;
public:
    FlaggedItems() noexcept { }
    FlaggedItems(common::span<T> items) noexcept : FlagBitmap(items.size(), false), Container(items) { }
    [[nodiscard]] constexpr ItType begin() const noexcept
    {
        return { this, 0 };
    }
    [[nodiscard]] constexpr ItType end() const noexcept
    {
        return { this, Container.size() };
    }
};


class EvalTempStore
{
    friend NailangExecutor;
private:
    boost::container::small_vector<Arg, 4> TempArg;
public:
    template<typename T>
    void Put(T&& arg) noexcept
    {
        TempArg.emplace_back(std::forward<T>(arg));
    }
    void Reset() noexcept
    {
        TempArg.clear();
    }
};
class MetaSet
{
    friend NailangExecutor;
private:
    std::vector<Arg> Args;
    std::vector<FuncPack> AllMetas;
    MemoryPool FuncNamePool;
    FlaggedItems<FuncPack> RealMetas;
    size_t MetaCount;
    void AllocateMeta(bool isPostMeta, const FuncCall& func)
    {
        auto it = isPostMeta ? AllMetas.end() : (AllMetas.begin() + MetaCount);
        AllMetas.emplace(it, func, common::span<Arg>{});
        if (!isPostMeta)
            MetaCount++;
    }
    template<typename... Args>
    forceinline void AllocateMeta(bool isPostMeta, const std::u32string_view metaName, FuncName::FuncInfo metaFlag, Args&&... args)
    {
        const auto funcName = FuncName::Create(FuncNamePool, metaName, metaFlag);
        AllocateMeta(isPostMeta, { funcName, std::forward<Args>(args)... });
    }
    forceinline void FillFuncPack(FuncPack& pack, NailangExecutor& executor);
    void PrepareLiveMetas() noexcept { RealMetas = common::to_span(AllMetas).subspan(0, MetaCount); }
public:
    const common::span<const FuncCall> OriginalMetas;
    const Statement& Target;
    MetaSet(common::span<const FuncCall> metas, const Statement& target) noexcept : 
        FuncNamePool(4096), MetaCount(0), OriginalMetas(metas), Target(target)
    { }
    const FlaggedItems<FuncPack>& LiveMetas() noexcept
    {
        return RealMetas;
    }
    common::span<FuncPack> PostMetas() noexcept
    {
        return common::to_span(AllMetas).subspan(MetaCount);
    }
    forceinline static common::span<FuncPack> GetPostMetas(MetaSet* metas) noexcept
    {
        if (metas)
            return metas->PostMetas();
        return {};
    }
};
struct FuncEvalPack : public FuncPack
{
    MetaSet* const Metas;
    constexpr FuncEvalPack(const FuncCall& call, common::span<Arg> params, MetaSet* metas) noexcept :
        FuncPack(call, params), Metas(metas) { }
    forceinline common::span<FuncPack> GetPostMetas() const noexcept
    {
        return MetaSet::GetPostMetas(Metas);
    }
    forceinline constexpr common::span<const FuncCall> GetOriginalMetas() const noexcept
    {
        if (Metas)
            return Metas->OriginalMetas;
        return {};
    }
};


class NailangFrameStack;
class NAILANGAPI NailangFrame
{
    friend NailangExecutor;
    friend NailangRuntime;
    friend NailangFrameStack;
private:
    struct FuncStackInfo
    {
        const FuncStackInfo* PrevInfo;
        const FuncCall* Func;
    };
    const FuncStackInfo* FuncInfo;
    class FuncInfoHolder;
public:
    enum class ProgramStatus : uint8_t 
    { 
        Next    = 0b00000000, 
        End     = 0b00000001,
        LoopEnd = 0b00000010,
    };
    enum class FrameFlags : uint8_t
    {
        Empty       = 0b00000000,
        VarScope    = 0b00000001,
        FlowScope   = 0b00000010,
        FuncCall    = FlowScope | VarScope,
        LoopScope   = 0b00000100,
    };

    NailangFrame* const PrevFrame;
    NailangExecutor* const Executor;
    std::shared_ptr<EvaluateContext> Context;
    FrameFlags Flags;
    ProgramStatus Status = ProgramStatus::Next;
    uint8_t IfRecord = 0;

    NailangFrame(NailangFrame* prev, NailangExecutor* executor, const std::shared_ptr<EvaluateContext>& ctx, FrameFlags flag) noexcept;
    virtual ~NailangFrame();
    COMMON_NO_COPY(NailangFrame)
    COMMON_NO_MOVE(NailangFrame)
    virtual size_t GetSize() const noexcept;
    [[nodiscard]] constexpr bool Has(FrameFlags flag) const noexcept;
    [[nodiscard]] constexpr bool Has(ProgramStatus status) const noexcept;
};
MAKE_ENUM_BITFIELD(NailangFrame::ProgramStatus)
MAKE_ENUM_BITFIELD(NailangFrame::FrameFlags)
inline constexpr bool NailangFrame::Has(FrameFlags flag) const noexcept
{
    return HAS_FIELD(Flags, flag);
}
inline constexpr bool NailangFrame::Has(ProgramStatus status) const noexcept
{
    return HAS_FIELD(Status, status);
}

class NAILANGAPI NailangBlockFrame : public NailangFrame
{
public:
    const common::span<const FuncCall> MetaScope;
    const Block* const BlockScope;
    const Statement* CurContent = nullptr;
    Arg ReturnArg;
    NailangBlockFrame(NailangFrame* prev, NailangExecutor* executor, const std::shared_ptr<EvaluateContext>& ctx, FrameFlags flag,
        const Block* block, common::span<const FuncCall> metas = {}) noexcept;
    ~NailangBlockFrame() override;
    size_t GetSize() const noexcept override;
    void Execute();
};

class NAILANGAPI NailangRawBlockFrame : public NailangFrame
{
public:
    common::span<const FuncCall> MetaScope;
    const RawBlock* const BlockScope;
    NailangRawBlockFrame(NailangFrame* prev, NailangExecutor* executor, const std::shared_ptr<EvaluateContext>& ctx, FrameFlags flag,
        const RawBlock* block, common::span<const FuncCall> metas = {}) noexcept;
    ~NailangRawBlockFrame() override;
    size_t GetSize() const noexcept override;
};


class NailangFrameStack : protected common::container::TrunckedContainer<std::byte>
{
    friend NailangRuntime;
private:
    template<typename T, typename... Args>
    [[nodiscard]] forceinline T* Create(Args&&... args) noexcept
    {
        const auto space = Alloc(sizeof(T), alignof(T));
        new (space.data()) T(TopFrame, std::forward<Args>(args)...);
        return reinterpret_cast<T*>(space.data());
    }
    NailangFrame* TopFrame = nullptr;
    class FrameHolderBase
    {
        friend NailangFrameStack;
    protected:
        NailangFrameStack* const Host;
        NailangFrame* Frame;
        constexpr FrameHolderBase(NailangFrameStack* host, NailangFrame* frame) noexcept :
            Host(host), Frame(frame) 
        { }
        NAILANGAPI ~FrameHolderBase();
    };
public:
    template<typename T>
    class [[nodiscard]] FrameHolder : protected FrameHolderBase
    {
        static_assert(std::is_base_of_v<NailangFrame, T>);
        friend NailangFrameStack;
    private:
        template<typename... Args>
        forceinline FrameHolder(NailangFrameStack* host, Args&&... args) noexcept : 
            FrameHolderBase(host, host->Create<T>(std::forward<Args>(args)...))
        { 
            Host->TopFrame = Frame;
        }
    public:
        template<typename U>
        forceinline FrameHolder(FrameHolder<U>&& other) noexcept : FrameHolderBase(other.Host, other.Frame)
        {
            static_assert(std::is_base_of_v<T, U>);
            other.Frame = nullptr;
        }
        COMMON_NO_COPY(FrameHolder)
        COMMON_NO_MOVE(FrameHolder)
        forceinline constexpr T* operator->() const noexcept
        {
            return static_cast<T*>(Frame);
        }
        forceinline constexpr T& operator*() const noexcept
        {
            return *static_cast<T*>(Frame);
        }
        template<typename U>
        forceinline U* As() const noexcept
        {
            static_assert(std::is_base_of_v<T, U>);
            return dynamic_cast<U*>(Frame);
        }
    };
    NailangFrameStack();
    ~NailangFrameStack();
    COMMON_NO_COPY(NailangFrameStack)
    COMMON_NO_MOVE(NailangFrameStack)
    template<typename T = NailangFrame, typename... Args>
    [[nodiscard]] forceinline FrameHolder<T> PushFrame(Args&&... args)
    {
        return { this, std::forward<Args>(args)... };
    }
    NAILANGAPI NailangBlockFrame* SetReturn(Arg returnVal) const noexcept;
    NAILANGAPI NailangFrame* SetBreak() const noexcept;
    NAILANGAPI NailangFrame* SetContinue() const noexcept;
    NAILANGAPI std::vector<common::StackTraceItem> CollectStacks() const noexcept;
    template<typename T>
    T* SearchFrameType(NailangFrame* from = nullptr) const noexcept
    {
        for (auto frame = from ? from : TopFrame; frame; frame = frame->PrevFrame)
        {
            if (const auto dframe = dynamic_cast<T*>(frame); dframe)
                return dframe;
        }
        return nullptr;
    }
};


class NAILANGAPI NailangBase
{
public:
    [[noreturn]] virtual void HandleException(const NailangRuntimeException& ex) const = 0;
    
    void ThrowByArgCount(const FuncCall& func, const size_t count, const ArgLimits limit = ArgLimits::Exact) const;
    void ThrowByArgType(const FuncCall& func, const Expr::Type type, size_t idx) const;
    void ThrowByArgTypes(const FuncCall& func, common::span<const Expr::Type> types, const ArgLimits limit, size_t offset = 0) const;
    template<size_t N, ArgLimits Limit = ArgLimits::Exact>
    forceinline void ThrowByArgTypes(const FuncCall& func, const std::array<Expr::Type, N>& types, size_t offset = 0) const
    {
        ThrowByArgTypes(func, types, Limit, offset);
    }
    template<size_t Min, size_t Max>
    forceinline void ThrowByArgTypes(const FuncCall& func, const std::array<Expr::Type, Max>& types, size_t offset = 0) const
    {
        ThrowByArgCount(func, Min + offset, ArgLimits::AtLeast);
        ThrowByArgTypes(func, types, ArgLimits::AtMost, offset);
    }
    void ThrowByParamType(const FuncCall& func, const Arg& arg, const Arg::Type type, size_t idx) const;
    void ThrowByParamTypes(const FuncPack& func, common::span<const Arg::Type> types, const ArgLimits limit = ArgLimits::Exact, size_t offset = 0) const;
    template<size_t N, ArgLimits Limit = ArgLimits::Exact>
    forceinline void ThrowByParamTypes(const FuncPack& func, const std::array<Arg::Type, N>& types, size_t offset = 0) const
    {
        ThrowByParamTypes(func, types, Limit, offset);
    }
    template<size_t Min, size_t Max>
    forceinline void ThrowByParamTypes(const FuncPack& func, const std::array<Arg::Type, Max>& types, size_t offset = 0) const
    {
        ThrowByArgCount(func, Min + offset, ArgLimits::AtLeast);
        ThrowByParamTypes(func, types, ArgLimits::AtMost, offset);
    }
    void ThrowIfNotFuncTarget(const FuncCall& call, const FuncName::FuncInfo type) const;
    void ThrowIfStatement(const FuncCall& meta, const Statement& target, const Statement::Type type) const;
    void ThrowIfNotStatement(const FuncCall& meta, const Statement& target, const Statement::Type type) const;
    bool ThrowIfNotBool(const Arg& arg, const std::u32string_view varName) const;

    [[nodiscard]] std::u32string FormatString(const std::u32string_view formatter, common::span<const Arg> args);
    [[nodiscard]] TempFuncName CreateTempFuncName(std::u32string_view name, FuncName::FuncInfo info = FuncName::FuncInfo::Empty) const;
    [[nodiscard]] LateBindVar DecideDynamicVar(const Expr& arg, const std::u16string_view reciever) const;
};

class NAILANGAPI NailangExecutor : public NailangBase
{
    friend NailangRuntime;
    friend NailangBlockFrame;
private:
    void ExecuteFrame(NailangBlockFrame& frame);
    template<typename T, Arg(Arg::* F)(SubQuery<T>&)>
    [[nodiscard]] Arg EvaluateQuery(Arg target, SubQuery<T> query, EvalTempStore& store, bool forWrite);
protected:
    NailangRuntime* Runtime = nullptr;
    [[nodiscard]] forceinline constexpr NailangFrame& GetFrame() const noexcept;
    [[nodiscard]] forceinline constexpr xziar::nailang::NailangFrameStack& GetFrameStack() const noexcept;
    [[nodiscard]] forceinline std::shared_ptr<xziar::nailang::EvaluateContext> CreateContext() const;
    void EvalFuncArgs(const FuncCall& func, Arg* args, const Arg::Type* types, const size_t size, size_t offset = 0);
    template<size_t N, ArgLimits Limit = ArgLimits::Exact>
    [[nodiscard]] forceinline std::array<Arg, N> EvalFuncArgs(const FuncCall& func, size_t offset = 0)
    {
        ThrowByArgCount(func, N + offset, Limit);
        std::array<Arg, N> args = {};
        EvalFuncArgs(func, args.data(), nullptr, std::min(N + offset, func.Args.size()), offset);
        return args;
    }
    template<size_t N, ArgLimits Limit = ArgLimits::Exact>
    [[nodiscard]] forceinline std::array<Arg, N> EvalFuncArgs(const FuncCall& func, const std::array<Arg::Type, N>& types, size_t offset = 0)
    {
        ThrowByArgCount(func, N + offset, Limit);
        std::array<Arg, N> args = {};
        EvalFuncArgs(func, args.data(), types.data(), std::min(N + offset, func.Args.size()), offset);
        return args;
    }
    [[nodiscard]] forceinline Arg EvalFuncSingleArg(const FuncCall& func, Arg::Type type, ArgLimits limit = ArgLimits::Exact, size_t offset = 0)
    {
        if (limit == ArgLimits::AtMost && func.Args.size() == offset)
            return {};
        ThrowByArgCount(func, 1 + offset, limit);
        EvalTempStore store;
        auto arg = EvaluateExpr(func.Args[offset], store);
        ThrowByParamType(func, arg, type, offset);
        return arg;
    }
    std::vector<Arg> EvalFuncAllArgs(const FuncCall& func);
public:
    enum class MetaFuncResult : uint8_t { Unhandled, Next, Skip, Return };
    NailangExecutor(NailangRuntime* runtime) noexcept;
    virtual ~NailangExecutor();
    [[nodiscard]] virtual NailangFrameStack::FrameHolder<NailangFrame> PushFrame(std::shared_ptr<EvaluateContext> ctx, NailangFrame::FrameFlags flag);
    [[nodiscard]] virtual NailangFrameStack::FrameHolder<NailangBlockFrame> PushBlockFrame(std::shared_ptr<EvaluateContext> ctx, NailangFrame::FrameFlags flag, const Block* block, common::span<const FuncCall> metas = {});
    [[nodiscard]] virtual NailangFrameStack::FrameHolder<NailangRawBlockFrame> PushRawBlockFrame(std::shared_ptr<EvaluateContext> ctx, NailangFrame::FrameFlags flag, const RawBlock* block, common::span<const FuncCall> metas = {});
    [[nodiscard]] virtual MetaFuncResult HandleMetaFunc(const FuncCall& meta, MetaSet& allMetas);
    [[nodiscard]] virtual MetaFuncResult HandleMetaFunc(FuncPack& meta, MetaSet& allMetas);
    [[nodiscard]] virtual Arg EvaluateFunc(FuncEvalPack& func);
    [[nodiscard]] virtual Arg EvaluateExtendMathFunc(FuncEvalPack& func);
    [[nodiscard]] virtual Arg EvaluateLocalFunc(const LocalFunc& func, FuncEvalPack& pack);
    [[nodiscard]] virtual Arg EvaluateUnknwonFunc(FuncEvalPack& func);
    [[nodiscard]] Arg EvaluateUnaryExpr(const UnaryExpr& expr, EvalTempStore& store);
    [[nodiscard]] Arg EvaluateBinaryExpr(const BinaryExpr& expr, EvalTempStore& store);
    [[nodiscard]] Arg EvaluateTernaryExpr(const TernaryExpr& expr, EvalTempStore& store);
    /*[[nodiscard]] Arg EvaluateSubfieldQuery(Arg target, SubQuery<const Expr> query, EvalTempStore& store, bool forWrite);
    [[nodiscard]] Arg EvaluateIndexQuery(Arg target, SubQuery<Arg> query, EvalTempStore& store, bool forWrite);*/
    [[noreturn]] void HandleException(const NailangRuntimeException& ex) const final;
    void HandleContent(const Statement& content, EvalTempStore& store, MetaSet* metas);
    [[nodiscard]] bool HandleMetaFuncs(MetaSet& allMetas); // return if need eval target
    [[nodiscard]] virtual Arg EvaluateExpr(const Expr& arg, EvalTempStore& store, bool forWrite = false);
    [[nodiscard]] virtual Arg EvaluateFunc(const FuncCall& func, EvalTempStore& store, MetaSet* metas);
    virtual void EvaluateAssign(const AssignExpr& assign, EvalTempStore& store, MetaSet* metas);
    virtual void EvaluateRawBlock(const RawBlock& block, common::span<const FuncCall> metas);
    virtual void EvaluateBlock(const Block& block, common::span<const FuncCall> metas);
};

inline void MetaSet::FillFuncPack(FuncPack& pack, NailangExecutor& executor)
{
    const auto begin = Args.size();
    EvalTempStore store;
    for (const auto& arg : pack.Args)
    {
        Args.emplace_back(executor.EvaluateExpr(arg, store));
        store.Reset();
    }
    pack.Params = common::to_span(Args).subspan(begin);
}
inline void NailangBlockFrame::Execute() { Executor->ExecuteFrame(*this); }


class NAILANGAPI NailangRuntime : public NailangBase
{
    friend NailangExecutor;
protected:
    MemoryPool MemPool;
    std::shared_ptr<EvaluateContext> RootContext;
    NailangFrameStack FrameStack;

    constexpr NailangFrame* CurFrame() const noexcept { return FrameStack.TopFrame; }

    [[nodiscard]] std::shared_ptr<EvaluateContext> GetContext(bool innerScope) const;
    [[noreturn]] void HandleException(const NailangRuntimeException& ex) const override;
    /**
     * @brief locate the arg through thr framestack, create if necessary
     * @return arglocator
    */
    [[nodiscard]] Arg LocateArg(const LateBindVar& var, const bool create) const;
    /**
     * @brief locate the arg, perform nilcheck, return locator, or empty if skipped
     * @return arglocator | empty
    */
    [[nodiscard]] Arg LocateArgForWrite(const LateBindVar& var, NilCheck nilCheck, std::variant<bool, EmbedOps> extra) const;
    bool SetFunc(const Block* block, common::span<std::pair<std::u32string_view, Arg>> capture, common::span<const Expr> args);
    bool SetFunc(const Block* block, common::span<std::pair<std::u32string_view, Arg>> capture, common::span<const std::u32string_view> args);

    [[nodiscard]] virtual std::shared_ptr<EvaluateContext> ConstructEvalContext() const;
    /**
     * @brief locate the arg, perform nilcheck, decay, then return
     * @return arg
    */
    [[nodiscard]] virtual Arg LookUpArg(const LateBindVar& var, const bool checkNull = true) const;
    [[nodiscard]] virtual LocalFunc LookUpFunc(std::u32string_view name) const;

public:
    NailangRuntime(std::shared_ptr<EvaluateContext> context);
    virtual ~NailangRuntime();
};

inline constexpr NailangFrame& NailangExecutor::GetFrame() const noexcept
{ 
    return *Runtime->CurFrame();
}
inline constexpr xziar::nailang::NailangFrameStack& NailangExecutor::GetFrameStack() const noexcept
{
    return Runtime->FrameStack;
}
inline std::shared_ptr<xziar::nailang::EvaluateContext> NailangExecutor::CreateContext() const
{
    return Runtime->ConstructEvalContext();
}


class NAILANGAPI NailangBasicRuntime : public NailangRuntime
{
protected:
    NailangExecutor Executor;
public:
    NailangBasicRuntime(std::shared_ptr<EvaluateContext> context);
    ~NailangBasicRuntime() override;
    void ExecuteBlock(const Block& block, common::span<const FuncCall> metas, std::shared_ptr<EvaluateContext> ctx, const bool checkMetas = true);
    void ExecuteBlock(const Block& block, common::span<const FuncCall> metas, const bool checkMetas, const bool innerScope)
    {
        ExecuteBlock(block, metas, GetContext(innerScope), checkMetas);
    }
    Arg EvaluateRawStatement(std::u32string_view content, const bool innerScope = true);
};


}

#if COMMON_COMPILER_MSVC
#   pragma warning(pop)
#endif
