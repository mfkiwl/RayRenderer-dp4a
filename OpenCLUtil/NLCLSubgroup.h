#pragma once
#include "oclNLCLRely.h"
#include "common/StrParsePack.hpp"

namespace oclu
{
struct SubgroupProvider;
struct NLCLSubgroupExtension;

struct SubgroupAttributes
{
    enum class MimicType { None, Auto, Local, Ptx };
    std::string Args;
    MimicType Mimic = MimicType::None;
};
enum class SubgroupReduceOp  : uint8_t { Add, Mul, Min, Max, And, Or, Xor };
enum class SubgroupShuffleOp : uint8_t { Broadcast, Shuffle, ShuffleXor, ShuffleDown, ShuffleUp };


struct NLCLSubgroupCapbility : public SubgroupCapbility
{
    bool SupportFP16            :1;
    bool SupportFP64            :1;
    bool SupportBasicSubgroup   :1;
    explicit constexpr NLCLSubgroupCapbility() noexcept : SupportFP16(false), SupportFP64(false), SupportBasicSubgroup(false) {}
    explicit constexpr NLCLSubgroupCapbility(NLCLContext& context) noexcept : SubgroupCapbility(context.SubgroupCaps),
        SupportFP16(context.SupportFP16), SupportFP64(context.SupportFP64), SupportBasicSubgroup(false) {}
};

struct NLCLSubgroupExtension : public NLCLExtension
{
    std::shared_ptr<SubgroupProvider> DefaultProvider;
    common::mlog::MiniLogger<false>& Logger;
    std::shared_ptr<SubgroupProvider> Provider;
    uint32_t SubgroupSize = 0;

    NLCLSubgroupExtension(common::mlog::MiniLogger<false>& logger, NLCLContext& context) : 
        NLCLExtension(context), DefaultProvider(Generate(logger, context, {}, {})), Logger(logger)
    { }
    ~NLCLSubgroupExtension() override { }
    
    void  BeginInstance(xcomp::XCNLRuntime&, xcomp::InstanceContext& ctx) final;
    void FinishInstance(xcomp::XCNLRuntime&, xcomp::InstanceContext& ctx) final;
    void InstanceMeta(xcomp::XCNLExecutor& executor, const xziar::nailang::FuncPack& meta, xcomp::InstanceContext& ctx) final;

    [[nodiscard]] xcomp::ReplaceResult ReplaceFunc(xcomp::XCNLRawExecutor& executor, std::u32string_view func,
        common::span<const std::u32string_view> args) final;
    [[nodiscard]] std::optional<xziar::nailang::Arg> ConfigFunc(xcomp::XCNLExecutor& executor, xziar::nailang::FuncEvalPack& call) final;

    static NLCLSubgroupCapbility GenerateCapabiity(NLCLContext& context, const SubgroupAttributes& attr);
    static std::shared_ptr<SubgroupProvider> Generate(common::mlog::MiniLogger<false>& logger, NLCLContext& context, std::u32string_view mimic, std::u32string_view args);
    
    XCNL_EXT_REG(NLCLSubgroupExtension,
    {
        if(const auto ctx = dynamic_cast<NLCLContext*>(&context); ctx)
            return std::make_unique<NLCLSubgroupExtension>(logger, *ctx);
        return {};
    });
};

struct ExtraParam
{
    std::u32string Param;
    std::u32string Arg;
    explicit operator bool() const noexcept
    {
        return !Arg.empty();
    }
};

struct SubgroupProvider
{
public:
    struct TypedAlgoResult
    {
        static constexpr common::simd::VecDataInfo Dummy{ common::simd::VecDataInfo::DataTypes::Empty, 0, 0, 0 };
        std::shared_ptr<const xcomp::ReplaceDepend> Depends;
        common::str::StrVariant<char32_t> Func;
        ExtraParam Extra;
        std::u32string_view AlgorithmSuffix;
        uint8_t Features;
        common::simd::VecDataInfo SrcType;
        common::simd::VecDataInfo DstType;
        TypedAlgoResult() noexcept : Features(0), SrcType(Dummy), DstType(Dummy) { }
        TypedAlgoResult(common::str::StrVariant<char32_t> func, const std::u32string_view algoSfx, const uint8_t features,
            const common::simd::VecDataInfo src, const common::simd::VecDataInfo dst,
            std::shared_ptr<const xcomp::ReplaceDepend> depends = {}) noexcept :
            Depends(std::move(depends)), Func(std::move(func)), AlgorithmSuffix(algoSfx), 
            Features(features), SrcType(src), DstType(dst)
        { }
        TypedAlgoResult(common::str::StrVariant<char32_t> func, const std::u32string_view algoSfx, const uint8_t features, 
            const common::simd::VecDataInfo type, std::shared_ptr<const xcomp::ReplaceDepend> depends = {}) noexcept : 
            Depends(std::move(depends)), Func(std::move(func)), AlgorithmSuffix(algoSfx), 
            Features(features), SrcType(type), DstType(type)
        { }
        xcomp::U32StrSpan GetPatchedBlockDepend() const noexcept
        {
            if (Depends) return Depends->PatchedBlock;
            return {};
        }
        std::u32string_view GetFuncName() const noexcept
        {
            return Func.StrView();
        }
        constexpr explicit operator bool() const noexcept
        {
            return !Func.empty();
        }
    };
protected:
    [[nodiscard]] static std::u32string GenerateFuncName(std::u32string_view op, std::u32string_view suffix, 
        common::simd::VecDataInfo vtype) noexcept;
    [[nodiscard]] static std::optional<common::simd::VecDataInfo> DecideVtype(common::simd::VecDataInfo vtype,
        common::span<const common::simd::VecDataInfo> scalars) noexcept;
    [[nodiscard]] static std::u32string ScalarPatch(const std::u32string_view name, const std::u32string_view baseFunc, 
        common::simd::VecDataInfo vtype, common::simd::VecDataInfo castType,
        std::u32string_view pfxParam, std::u32string_view pfxArg,
        std::u32string_view sfxParam, std::u32string_view sfxArg) noexcept;
    template<typename Op, typename... Args>
    [[nodiscard]] TypedAlgoResult VectorizePatch(const common::simd::VecDataInfo vtype,
        TypedAlgoResult& scalarAlgo, Args&&... args);
    [[nodiscard]] static std::u32string HiLoPatch(const std::u32string_view name, const std::u32string_view baseFunc, common::simd::VecDataInfo vtype,
        const std::u32string_view extraArg = {}, const std::u32string_view extraParam = {}) noexcept;
    struct Algorithm
    {
        struct DTypeSupport
        {
            xcomp::VecDimSupport Int = xcomp::VTypeDimSupport::Empty;
            xcomp::VecDimSupport FP  = xcomp::VTypeDimSupport::Empty;
            constexpr void Set(xcomp::VecDimSupport support) noexcept
            {
                Int = FP = support;
            }
            constexpr void Set(xcomp::VecDimSupport supInt, xcomp::VecDimSupport supFp) noexcept
            {
                Int = supInt; FP = supFp;
            }
        };
        uint8_t ID;
        uint8_t Features;
        DTypeSupport Bit64;
        DTypeSupport Bit32;
        DTypeSupport Bit16;
        DTypeSupport Bit8;
        constexpr Algorithm(uint8_t id, uint8_t feats) noexcept : ID(id), Features(feats) {}
    };
    struct Shuffle;
    boost::container::small_vector<Algorithm, 4> ShuffleSupport;
    struct Reduce;
    boost::container::small_vector<Algorithm, 4> ReduceSupport;

    common::mlog::MiniLogger<false>& Logger;
    NLCLContext& Context;
    const NLCLSubgroupCapbility Cap;
    bool EnableFP16 = false, EnableFP64 = false;
    void EnableVecType(common::simd::VecDataInfo type) noexcept;
    void AddWarning(common::str::StrVariant<char16_t> msg);
    /**
     * @brief return funcname and suffix for the given dtype
     * @param algo reference of algorithm
     * @param vtype datatype
     * @param op shuffle op
     * @return TypedAlgoResult
    */
    virtual TypedAlgoResult HandleShuffleAlgo(const Algorithm&, common::simd::VecDataInfo, SubgroupShuffleOp) { return {}; };
    /**
     * @brief return funcname and suffix for the given dtype
     * @param algo reference of algorithm
     * @param vtype datatype
     * @param op shuffle op
     * @return TypedAlgoResult
    */
    virtual TypedAlgoResult HandleReduceAlgo(const Algorithm&, common::simd::VecDataInfo, SubgroupReduceOp) { return {}; };
private:
    template<typename Op, bool ExactBit, typename... Args>
    TypedAlgoResult HandleSubgroupOperation(common::simd::VecDataInfo vtype, uint8_t featMask, Args&&... args);
public:
    SubgroupProvider(common::mlog::MiniLogger<false>& logger, NLCLContext& context, NLCLSubgroupCapbility cap);
    virtual ~SubgroupProvider() { }
    virtual xcomp::ReplaceResult GetSubgroupSize() { return {}; };
    virtual xcomp::ReplaceResult GetMaxSubgroupSize() { return {}; };
    virtual xcomp::ReplaceResult GetSubgroupCount() { return {}; };
    virtual xcomp::ReplaceResult GetSubgroupId() { return {}; };
    virtual xcomp::ReplaceResult GetSubgroupLocalId() { return {}; };
    virtual xcomp::ReplaceResult SubgroupAll(const std::u32string_view) { return {}; };
    virtual xcomp::ReplaceResult SubgroupAny(const std::u32string_view) { return {}; };
    xcomp::ReplaceResult SubgroupShuffle(common::simd::VecDataInfo vtype, const std::u32string_view ele, const std::u32string_view idx, 
        SubgroupShuffleOp op, bool nonUniform);
    virtual xcomp::ReplaceResult SubgroupReduce(SubgroupReduceOp, common::simd::VecDataInfo, const std::u32string_view) { return {}; };
    virtual void OnBegin(NLCLRuntime&, const NLCLSubgroupExtension&, KernelContext&) {}
    virtual void OnFinish(NLCLRuntime&, const NLCLSubgroupExtension&, KernelContext&);
private:
    std::vector<std::u16string> Warnings;
};


class NLCLSubgroupKHR : public SubgroupProvider
{
protected:
    static constexpr uint8_t AlgoKHRBroadcast   = 1;
    static constexpr uint8_t AlgoKHRBroadcastNU = 2;
    static constexpr uint8_t AlgoKHRShuffle     = 3;
    static constexpr uint8_t AlgoKHRReduce      = 1;
    static constexpr uint8_t AlgoKHRReduceNU    = 2;
    bool EnableKHRBasic = false, EnableKHRExtType = false, EnableKHRShuffle = false, EnableKHRShuffleRel = false, 
        EnableKHRBallot = false, EnableKHRNUArith = false;
    void OnFinish(NLCLRuntime& runtime, const NLCLSubgroupExtension& ext, KernelContext& kernel) override;
    void WarnFP(common::simd::VecDataInfo vtype, const std::u16string_view func);
    // internal use, won't check type
    xcomp::ReplaceResult SubgroupReduceArith(SubgroupReduceOp op, common::simd::VecDataInfo vtype, common::simd::VecDataInfo realType, const std::u32string_view ele);
    TypedAlgoResult HandleShuffleAlgo(const Algorithm& algo, common::simd::VecDataInfo vtype, SubgroupShuffleOp op) override;
    TypedAlgoResult HandleReduceAlgo(const Algorithm& algo, common::simd::VecDataInfo vtype, SubgroupReduceOp op) override;
public:
    NLCLSubgroupKHR(common::mlog::MiniLogger<false>& logger, NLCLContext& context, NLCLSubgroupCapbility cap);
    ~NLCLSubgroupKHR() override { }

    xcomp::ReplaceResult GetSubgroupSize() override;
    xcomp::ReplaceResult GetMaxSubgroupSize() override;
    xcomp::ReplaceResult GetSubgroupCount() override;
    xcomp::ReplaceResult GetSubgroupId() override;
    xcomp::ReplaceResult GetSubgroupLocalId() override;
    xcomp::ReplaceResult SubgroupAll(const std::u32string_view predicate) override;
    xcomp::ReplaceResult SubgroupAny(const std::u32string_view predicate) override;
    xcomp::ReplaceResult SubgroupReduce(SubgroupReduceOp op, common::simd::VecDataInfo vtype, const std::u32string_view ele) override;
};

class NLCLSubgroupIntel : public NLCLSubgroupKHR
{
private:
    /**
     * @brief check if need specific extention
     * @param type vector type
    */
    void EnableExtraVecType(common::simd::VecDataInfo type) noexcept;
protected:
    static constexpr uint8_t AlgoIntelShuffle = 8;
    bool EnableIntel = false, EnableIntel16 = false, EnableIntel8 = false;
    void OnFinish(NLCLRuntime& runtime, const NLCLSubgroupExtension& ext, KernelContext& kernel) override;
    xcomp::ReplaceResult SubgroupReduceArith(SubgroupReduceOp op, common::simd::VecDataInfo vtype, const std::u32string_view ele);
    xcomp::ReplaceResult SubgroupReduceBitwise(SubgroupReduceOp op, common::simd::VecDataInfo vtype, const std::u32string_view ele);
    TypedAlgoResult HandleShuffleAlgo(const Algorithm& algo, common::simd::VecDataInfo vtype, SubgroupShuffleOp op) override;
    TypedAlgoResult HandleReduceAlgo(const Algorithm& algo, common::simd::VecDataInfo vtype, SubgroupReduceOp op) override;
public:
    NLCLSubgroupIntel(common::mlog::MiniLogger<false>& logger, NLCLContext& context, NLCLSubgroupCapbility cap);
    ~NLCLSubgroupIntel() override { }

    xcomp::ReplaceResult SubgroupReduce(SubgroupReduceOp op, common::simd::VecDataInfo vtype, const std::u32string_view ele) override;

};

class NLCLSubgroupLocal : public NLCLSubgroupKHR
{
protected:
    static constexpr uint8_t AlgoLocalBroadcast = 8, AlgoLocalShuffle = 9;
    std::u32string_view KernelName;
    bool NeedSubgroupSize = false, NeedLocalTemp = false;
    void OnBegin(NLCLRuntime& runtime, const NLCLSubgroupExtension& ext, KernelContext& kernel) override;
    void OnFinish(NLCLRuntime& runtime, const NLCLSubgroupExtension& ext, KernelContext& kernel) override;
    std::u32string GenerateKID(std::u32string_view type) const noexcept;
    xcomp::ReplaceResult BroadcastPatch (const common::simd::VecDataInfo vtype) noexcept;
    xcomp::ReplaceResult ShufflePatch   (const common::simd::VecDataInfo vtype) noexcept;
    xcomp::ReplaceResult ShuffleXorPatch(const common::simd::VecDataInfo vtype) noexcept;
    TypedAlgoResult HandleShuffleAlgo(const Algorithm& algo, common::simd::VecDataInfo vtype, SubgroupShuffleOp op) override;
public:
    NLCLSubgroupLocal(common::mlog::MiniLogger<false>& logger, NLCLContext& context, NLCLSubgroupCapbility cap);
    ~NLCLSubgroupLocal() override { }

    xcomp::ReplaceResult GetSubgroupSize() override;
    xcomp::ReplaceResult GetMaxSubgroupSize() override;
    xcomp::ReplaceResult GetSubgroupCount() override;
    xcomp::ReplaceResult GetSubgroupId() override;
    xcomp::ReplaceResult GetSubgroupLocalId() override;
    xcomp::ReplaceResult SubgroupAll(const std::u32string_view predicate) override;
    xcomp::ReplaceResult SubgroupAny(const std::u32string_view predicate) override;

};

class NLCLSubgroupPtx : public SubgroupProvider
{
protected:
    static constexpr uint8_t AlgoPtxShuffle = 8;
    std::u32string_view ExtraSync, ExtraMask;
    const uint32_t SMVersion;
    xcomp::ReplaceResult ShufflePatch(common::simd::VecDataInfo vtype, SubgroupShuffleOp op) noexcept;
    xcomp::ReplaceResult SubgroupReduceSM80(SubgroupReduceOp op, common::simd::VecDataInfo vtype, const std::u32string_view ele);
    xcomp::ReplaceResult SubgroupReduceSM30(SubgroupReduceOp op, common::simd::VecDataInfo vtype, const std::u32string_view ele);
    TypedAlgoResult HandleShuffleAlgo(const Algorithm& algo, common::simd::VecDataInfo vtype, SubgroupShuffleOp op) override;
public:
    NLCLSubgroupPtx(common::mlog::MiniLogger<false>& logger, NLCLContext& context, NLCLSubgroupCapbility cap, const uint32_t smVersion = 30);
    ~NLCLSubgroupPtx() override { }

    xcomp::ReplaceResult GetSubgroupSize() override;
    xcomp::ReplaceResult GetMaxSubgroupSize() override;
    xcomp::ReplaceResult GetSubgroupCount() override;
    xcomp::ReplaceResult GetSubgroupId() override;
    xcomp::ReplaceResult GetSubgroupLocalId() override;
    xcomp::ReplaceResult SubgroupAll(const std::u32string_view predicate) override;
    xcomp::ReplaceResult SubgroupAny(const std::u32string_view predicate) override;
    xcomp::ReplaceResult SubgroupReduce(SubgroupReduceOp op, common::simd::VecDataInfo vtype, const std::u32string_view ele) override;

};



}