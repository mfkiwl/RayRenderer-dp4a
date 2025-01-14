#pragma once
#include "oclRely.h"
#include "oclDevice.h"
#include "oclContext.h"
#include "oclCmdQue.h"
#include "oclBuffer.h"
#include "oclImage.h"
#include "oclPromise.h"
#include "XComputeBase/XCompDebug.h"
#include "common/FileBase.hpp"
#include "common/CLikeConfig.hpp"
#include "common/StringPool.hpp"



#if COMMON_COMPILER_MSVC
#   pragma warning(push)
#   pragma warning(disable:4275 4251)
#endif

namespace oclu
{
class oclProgram_;
using oclProgram = std::shared_ptr<const oclProgram_>;
class oclKernel_;
using oclKernel = std::shared_ptr<const oclKernel_>;

namespace debug
{
struct NLCLDebugExtension;
}

struct WorkGroupInfo
{
    uint64_t LocalMemorySize;
    uint64_t PrivateMemorySize;
    uint64_t SpillMemSize;
    size_t WorkGroupSize;
    size_t CompiledWorkGroupSize[3];
    size_t PreferredWorkGroupSizeMultiple;
    constexpr bool HasCompiledWGSize() const noexcept
    {
        return CompiledWorkGroupSize[0] || CompiledWorkGroupSize[1] || CompiledWorkGroupSize[2];
    }
};

struct SubgroupInfo
{
    size_t SubgroupSize;
    size_t SubgroupCount;
    size_t CompiledSubgroupSize;
};

enum class KerArgType  : uint8_t { Any, Buffer, Image, Simple };
enum class KerArgSpace : uint8_t { Private, Global, Constant, Local };
enum class ImgAccess   : uint8_t { None, ReadOnly, WriteOnly, ReadWrite };
enum class KerArgFlag  : uint8_t { None = 0, Const = 0x1, Restrict = 0x2, Volatile = 0x4, Pipe = 0x8 };
MAKE_ENUM_BITFIELD(KerArgFlag)


struct OCLUAPI ArgFlags
{
    KerArgType  ArgType   = KerArgType::Any;
    KerArgSpace Space     = KerArgSpace::Private;
    ImgAccess   Access    = ImgAccess::None;
    KerArgFlag  Qualifier = KerArgFlag::None;
    [[nodiscard]] std::string_view GetArgTypeName() const noexcept;
    [[nodiscard]] std::string_view GetSpaceName() const noexcept;
    [[nodiscard]] std::string_view GetImgAccessName() const noexcept;
    [[nodiscard]] std::string_view GetQualifierName() const noexcept;
    [[nodiscard]] constexpr bool IsType(const KerArgType type) const noexcept 
    { return ArgType == type || ArgType == KerArgType::Any; }

    [[nodiscard]] static std::string_view ToCLString(const KerArgSpace space) noexcept;
    [[nodiscard]] static std::string_view ToCLString(const ImgAccess access) noexcept;
};

struct KernelArgInfo : public ArgFlags
{
    std::string_view Name;
    std::string_view Type;
};

class OCLUAPI KernelArgStore
{
    friend oclKernel_;
    friend NLCLRawExecutor;
    friend NLCLRuntime;
    friend KernelContext;
protected:
    struct ArgInfo : public ArgFlags
    {
        common::StringPiece<char> Name;
        common::StringPiece<char> Type;
    };
    common::StringPool<char> ArgTexts;
    std::vector<ArgInfo> ArgsInfo;
    std::shared_ptr<xcomp::debug::InfoProvider> InfoProv;
    uint32_t DebugBuffer;
    bool HasInfo, HasDebug;
    KernelArgStore(const detail::PlatFuncs* funcs, CLHandle<detail::CLKernel> kernel, const KernelArgStore& reference);
    const ArgInfo* GetArg(const size_t idx, const bool check = true) const;
    void AddArg(const KerArgType argType, const KerArgSpace space, const ImgAccess access, const KerArgFlag qualifier,
        const std::string_view name, const std::string_view type);
    KernelArgInfo GetArgInfo(const size_t idx) const noexcept;
    using ItType = common::container::IndirectIterator<const KernelArgStore, KernelArgInfo, &KernelArgStore::GetArgInfo>;
    friend ItType;
public:
    KernelArgStore() : DebugBuffer(0), HasInfo(false), HasDebug(false) {}
    KernelArgInfo operator[](size_t idx) const noexcept { return GetArgInfo(idx); }
    size_t GetSize() const noexcept { return ArgsInfo.size(); }
    ItType begin() const noexcept { return { this, 0 }; }
    ItType end()   const noexcept { return { this, ArgsInfo.size() }; }
};


struct CallArgs
{
    friend oclKernel_;
private:
    using ArgType = std::variant<oclSubBuffer, oclImage, std::vector<std::byte>, std::array<std::byte, 32>, common::span<const std::byte>>;
    std::vector<ArgType> Args;
public:
    void PushArg(oclSubBuffer buf)
    {
        Args.push_back(buf);
    }
    void PushArg(oclImage img)
    {
        Args.push_back(img);
    }
    void PushArg(const void* dat, const size_t size, const bool ref = false)
    {
        if (ref)
        {
            common::span<const std::byte> tmp(reinterpret_cast<const std::byte*>(dat), size);
            Args.push_back(tmp);
        }
        else if (size <= 31)
        {
            std::array<std::byte, 32> tmp;
            memcpy_s(&tmp[1], size, dat, size);
            tmp[0] = static_cast<std::byte>(size);
            Args.push_back(tmp);
        }
        else
        {
            std::vector<std::byte> tmp;
            tmp.resize(size);
            memcpy_s(&tmp[0], size, dat, size);
            Args.push_back(tmp);
        }
    }
    template<typename T, bool OnlyRef = false>
    void PushSpanArg(T&& dat)
    {
        const auto space = common::as_bytes(common::to_span(dat));
        return PushArg(space.data(), space.size(), OnlyRef);
    }
    template<typename T>
    void PushSimpleArg(T&& dat)
    {
        static_assert(!std::is_same_v<T, bool>, "boolean is implementation-defined and cannot be pass as kernel argument.");
        return PushArg(&dat, sizeof(T));
    }
};


struct OCLUAPI CallResult
{
    std::shared_ptr<xcomp::debug::DebugManager> DebugMan;
    std::shared_ptr<xcomp::debug::InfoProvider> InfoProv;
    oclKernel Kernel;
    oclCmdQue Queue;
    oclBuffer InfoBuf;
    oclBuffer DebugBuf;

    std::unique_ptr<xcomp::debug::DebugPackage> GetDebugData(const bool releaseRuntime = false) const;
};


template<size_t N>
struct SizeN
{
    size_t Data[N];
    constexpr SizeN() noexcept : Data{ 0 }
    { }
    constexpr SizeN(const size_t(&data)[N]) noexcept : Data{ 0 }
    {
        for (size_t i = 0; i < N; ++i)
            Data[i] = data[i];
    }
    constexpr SizeN(const std::array<size_t, N>& data) noexcept : Data{ 0 }
    {
        for (size_t i = 0; i < N; ++i)
            Data[i] = data[i];
    }
    constexpr SizeN(const std::initializer_list<size_t>& data) noexcept : Data{ 0 }
    {
        Expects(data.size() == N);
        size_t i = 0;
        for (const auto dat : data)
            Data[i++] = dat;
    }
    constexpr const size_t* GetData(const bool zeroToNull = false) const noexcept
    {
        if (zeroToNull)
        {
            for (size_t i = 0; i < N; ++i)
                if (Data[i] != 0)
                    return Data;
            return nullptr;
        }
        else
            return Data;
    }
};


class OCLUAPI oclKernel_ : public detail::oclCommon
{
    friend class oclProgram_;
private:
    MAKE_ENABLER();

    template<size_t N>
    [[nodiscard]] constexpr static const size_t* CheckLocalSize(const std::array<size_t, N>& localsize)
    {
        for (size_t i = 0; i < N; ++i)
            if (localsize[i] != 0)
                return localsize.data();
        return nullptr;
    }

    struct CallSiteInternal
    {
        oclKernel KernelHost;
        CLHandle<detail::CLKernel> Kernel;

        OCLUAPI CallSiteInternal(const oclKernel_* kernel);
        OCLUAPI void SetArg(const uint32_t idx, const oclSubBuffer_& buf) const;
        OCLUAPI void SetArg(const uint32_t idx, const oclImage_& img) const;
        OCLUAPI void SetArg(const uint32_t idx, const void* dat, const size_t size) const;
        template<typename T>
        void SetSpanArg(const uint32_t idx, const T& dat) const
        {
            const auto space = common::as_bytes(common::to_span(dat));
            return SetArg(idx, space.data(), space.size());
        }
        template<typename T>
        void SetSimpleArg(const uint32_t idx, const T& dat) const
        {
            static_assert(!std::is_same_v<T, bool>, "boolean is implementation-defined and cannot be pass as kernel argument.");
            return SetArg(idx, &dat, sizeof(T));
        }
        OCLUAPI [[nodiscard]] common::PromiseResult<CallResult> Run(const uint8_t dim, DependEvents depend,
            const oclCmdQue& que, const size_t* worksize, const size_t* workoffset, const size_t* localsize);
    };

    template<uint8_t N, typename... Args>
    class [[nodiscard]] KernelCallSite : protected CallSiteInternal
    {
        friend class oclKernel_;
    private:
        // clSetKernelArg does not hold parameter ownership, so need to manully hold it
        std::tuple<Args...> Paras;

        template<size_t Idx>
        forceinline void InitArg() const
        {
            using ArgType = common::remove_cvref_t<std::tuple_element_t<Idx, std::tuple<Args...>>>;
            if constexpr (std::is_convertible_v<ArgType, oclSubBuffer> || std::is_convertible_v<ArgType, oclImage>)
                SetArg(Idx, *std::get<Idx>(Paras));
            else if constexpr (common::CanToSpan<ArgType>)
                SetSpanArg(Idx, std::get<Idx>(Paras));
            else
                SetSimpleArg(Idx, std::get<Idx>(Paras));
            if constexpr (Idx != 0)
                InitArg<Idx - 1>();
        }

        KernelCallSite(const oclKernel_* kernel, Args&& ... args) : 
            CallSiteInternal(kernel), Paras(std::forward<Args>(args)...)
        {
            constexpr size_t argCount = sizeof...(Args);
            if constexpr(argCount > 0)
                InitArg<argCount - 1>();
        }
    public:
        [[nodiscard]] common::PromiseResult<CallResult> operator()(const common::PromiseStub& pmss, const oclCmdQue& que, 
            const SizeN<N> worksize, const SizeN<N> localsize = {}, const SizeN<N> workoffset = {})
        {
            return Run(N, pmss, que, worksize.GetData(), workoffset.GetData(), localsize.GetData(true));
        }
        [[nodiscard]] common::PromiseResult<CallResult> operator()(const oclCmdQue& que, 
            const SizeN<N> worksize, const SizeN<N> localsize = {}, const SizeN<N> workoffset = {})
        {
            return Run(N, {}, que, worksize.GetData(), workoffset.GetData(), localsize.GetData(true));
        }
    };

    class KernelDynCallSiteInternal : protected CallSiteInternal
    {
        friend class oclKernel_;
    private:
        // clSetKernelArg does not hold parameter ownership, so need to manully hold it
        CallArgs Args;
    protected:
        OCLUAPI KernelDynCallSiteInternal(const oclKernel_* kernel, CallArgs&& args);
    };

    template<uint8_t N>
    class [[nodiscard]] KernelDynCallSite : protected KernelDynCallSiteInternal
    {
        friend class oclKernel_;
    public:
        using KernelDynCallSiteInternal::KernelDynCallSiteInternal;
        [[nodiscard]] common::PromiseResult<CallResult> operator()(const common::PromiseStub& pmss, const oclCmdQue& que,
            const SizeN<N> worksize, const SizeN<N> localsize = {}, const SizeN<N> workoffset = {})
        {
            return Run(N, pmss, que, worksize.GetData(), workoffset.GetData(), localsize.GetData(true));
        }
        [[nodiscard]] common::PromiseResult<CallResult> operator()(const oclCmdQue& que,
            const SizeN<N> worksize, const SizeN<N> localsize = {}, const SizeN<N> workoffset = {})
        {
            return Run(N, {}, que, worksize.GetData(), workoffset.GetData(), localsize.GetData(true));
        }
    };

    oclKernel_(const oclProgram_* prog, void* kerID, std::string name, KernelArgStore&& argStore);
public:
    COMMON_NO_COPY(oclKernel_)
    COMMON_NO_MOVE(oclKernel_)
    ~oclKernel_();

    [[nodiscard]] std::optional<SubgroupInfo> GetSubgroupInfo(const uint8_t dim, const size_t* localsize) const;
    [[nodiscard]] bool HasOCLUDebug() const noexcept { return ArgStore.HasDebug; }
    template<uint8_t N>
    [[nodiscard]] std::optional<SubgroupInfo> GetSubgroupInfo(const std::array<size_t, N>& localsize) const
    {
        static_assert(N > 0 && N < 4, "local dim should be in [1,3]");
        return GetSubgroupInfo(N, localsize.data());
    }
    template<uint8_t N, typename... Args>
    [[nodiscard]] auto Call(Args&&... args) const
    {
        static_assert(N > 0 && N < 4, "work dim should be in [1,3]");
        return KernelCallSite<N, Args...>(this, std::forward<Args>(args)...);
    }
    template<uint8_t N>
    [[nodiscard]] auto CallDynamic(CallArgs&& args) const
    {
        static_assert(N > 0 && N < 4, "work dim should be in [1,3]");
        return KernelDynCallSite<N>(this, std::move(args));
    }

private:
    CLHandle<detail::CLKernel> Kernel;
    const oclProgram_& Program;
    uint32_t ReqDbgBufSize;
public:
    KernelArgStore ArgStore;
    WorkGroupInfo WgInfo;
    std::string Name;
};



struct CLProgConfig
{
    common::CLikeDefines Defines;
    std::set<std::string> Flags{ "-cl-fast-relaxed-math", "-cl-mad-enable" };
    uint32_t Version = 0;
};


class OCLUAPI [[nodiscard]] oclProgStub : public common::NonCopyable
{
    friend class oclProgram_;
    friend class NLCLProcessor;
    friend struct NLCLDebugExtension;
private:
    CLHandle<detail::CLProgram> Program;
    oclContext Context;
    oclDevice Device;
    std::string Source;
    std::vector<std::pair<std::string, KernelArgStore>> ImportedKernelInfo;
    std::shared_ptr<xcomp::debug::DebugManager> DebugMan;
    oclProgStub(const oclContext& ctx, const oclDevice& dev, std::string&& str);
public:
    ~oclProgStub();
    void Build(const CLProgConfig& config);
    [[nodiscard]] std::u16string GetBuildLog() const;
    [[nodiscard]] oclProgram Finish();
};

class OCLUAPI oclProgram_ : public detail::oclCommon, public std::enable_shared_from_this<oclProgram_>
{
    friend class oclContext_;
    friend class oclKernel_;
    friend class oclProgStub;
private:
    MAKE_ENABLER();
    CLHandle<detail::CLProgram> Program;
    const oclContext Context;
    const oclDevice Device;
    const std::string Source;
    std::vector<std::string> KernelNames;
    std::vector<std::unique_ptr<oclKernel_>> Kernels;
    std::shared_ptr<xcomp::debug::DebugManager> DebugMan;

    [[nodiscard]] static std::u16string GetProgBuildLog(const detail::PlatFuncs* funcs, CLHandle<detail::CLProgram> progID, CLHandle<detail::CLDevice> dev);
    [[nodiscard]] oclKernel GetKernelByIdx(const size_t idx) const
    {
        return std::shared_ptr<const oclKernel_>(shared_from_this(), Kernels[idx].get());
    }
    oclProgram_(oclProgStub* stub);

    using ItKernel = common::container::IndirectIterator<const oclProgram_, oclKernel, &oclProgram_::GetKernelByIdx>;
    friend ItKernel;
    class KernelContainer
    {
        friend class oclProgram_;
        const oclProgram_* Host;
        constexpr KernelContainer(const oclProgram_* host) noexcept : Host(host) { }
    public:
        ItKernel begin() const noexcept { return { Host, 0 }; }
        ItKernel end()   const noexcept { return { Host, Host->Kernels.size() }; }
        size_t Size() const noexcept { return Host->Kernels.size(); }
        oclKernel operator[](size_t idx) const { return Host->GetKernelByIdx(idx); }
        operator std::vector<oclKernel>() const
        {
            std::vector<oclKernel> vec;
            vec.reserve(Size());
            for (auto x : *this)
                vec.emplace_back(x);
            return vec;
        }
    };
public:
    COMMON_NO_COPY(oclProgram_)
    COMMON_NO_MOVE(oclProgram_)
    ~oclProgram_();
    [[nodiscard]] std::string_view GetSource() const noexcept { return Source; }
    [[nodiscard]] oclKernel GetKernel(const std::string_view& name) const;
    [[nodiscard]] constexpr KernelContainer GetKernels() const { return this; }
    [[nodiscard]] const std::vector<std::string>& GetKernelNames() const { return KernelNames; }
    [[nodiscard]] std::u16string GetBuildLog() const { return GetProgBuildLog(Funcs, Program, Device->DeviceID); }
    [[nodiscard]] std::vector<std::byte> GetBinary() const;

    [[nodiscard]] static oclProgStub Create(const oclContext& ctx, std::string str, const oclDevice& dev = {});
    [[nodiscard]] static oclProgram CreateAndBuild(const oclContext& ctx, std::string str, const CLProgConfig& config, const oclDevice& dev = {});
};


}



#if COMMON_COMPILER_MSVC
#   pragma warning(pop)
#endif
