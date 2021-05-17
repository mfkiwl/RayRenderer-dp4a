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
enum class SubgroupReduceOp { Sum, Min, Max, And, Or, Xor };


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


struct TypedArgFunc;
struct SubgroupProvider
{
protected:
    [[nodiscard]] static std::optional<common::simd::VecDataInfo> DecideVtype(common::simd::VecDataInfo vtype,
        common::span<const common::simd::VecDataInfo> scalars) noexcept;
    [[nodiscard]] static std::u32string ScalarPatch(const std::u32string_view name, const std::u32string_view baseFunc, common::simd::VecDataInfo vtype,
        const std::u32string_view extraArg = {}, const std::u32string_view extraParam = {}) noexcept;
    [[nodiscard]] forceinline static std::u32string ScalarPatchBcastShuf(
        const std::u32string_view name, const std::u32string_view baseFunc, common::simd::VecDataInfo vtype) noexcept
    {
        return ScalarPatch(name, baseFunc, vtype, U", sgId", U", const uint sgId");
    }
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
        static constexpr uint8_t FeatureBroadcast  = 0x1;
        static constexpr uint8_t FeatureShuffle    = 0x2;
        static constexpr uint8_t FeatureShuffleRel = 0x4;
        uint8_t ID;
        uint8_t Features;
        DTypeSupport Bit64;
        DTypeSupport Bit32;
        DTypeSupport Bit16;
        DTypeSupport Bit8;
        constexpr Algorithm(uint8_t id, uint8_t feats) noexcept : ID(id), Features(feats) {}
    };
    struct AlgorithmResult
    {
        common::str::StrVariant<char32_t> FuncName;
        std::u32string_view Suffix;
    };
    boost::container::small_vector<Algorithm, 4> BShufSupport;

    common::mlog::MiniLogger<false>& Logger;
    NLCLContext& Context;
    const NLCLSubgroupCapbility Cap;
    bool EnableFP16 = false, EnableFP64 = false;
    void EnableVecType(common::simd::VecDataInfo type) noexcept;
    void AddWarning(common::str::StrVariant<char16_t> msg);
    /**
     * @brief return funcname and suffix for the given dtype
     * @param algoId algorithm id
     * @param vtype datatype
     * @return { bshuf func name, algo suffix }
    */
    virtual AlgorithmResult SubgroupBShufAlgo(uint8_t, common::simd::VecDataInfo) { return {}; };
private:
    TypedArgFunc SubgroupBShuf(common::simd::VecDataInfo origType, common::simd::VecDataInfo srcType, bool isConvert, uint8_t featMask);
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
    xcomp::ReplaceResult SubgroupBShuf(common::simd::VecDataInfo vtype, const std::u32string_view ele, const std::u32string_view idx, bool isShuf);
    virtual xcomp::ReplaceResult SubgroupBroadcast(common::simd::VecDataInfo, const std::u32string_view, const std::u32string_view) { return {}; };
    virtual xcomp::ReplaceResult SubgroupShuffle(common::simd::VecDataInfo, const std::u32string_view, const std::u32string_view) { return {}; };
    virtual xcomp::ReplaceResult SubgroupReduce(SubgroupReduceOp, common::simd::VecDataInfo, const std::u32string_view) { return {}; };
    virtual void OnBegin(NLCLRuntime&, const NLCLSubgroupExtension&, KernelContext&) {}
    virtual void OnFinish(NLCLRuntime&, const NLCLSubgroupExtension&, KernelContext&);
private:
    std::vector<std::u16string> Warnings;
};


class NLCLSubgroupKHR : public SubgroupProvider
{
private:
    /**
     * @brief check if need extended_type, only applies to broadcast,reduce,scan
     * @param type vector type 
    */
    void EnableVecExtType(common::simd::VecDataInfo type) noexcept;
protected:
    static constexpr uint8_t AlgoKHRBroadcast  = 1;
    static constexpr uint8_t AlgoKHRShuffle    = 2;
    bool EnableKHRBasic = false, EnableKHRExtType = false, EnableKHRShuffle = false, EnableKHRShuffleRel = false;
    void OnFinish(NLCLRuntime& runtime, const NLCLSubgroupExtension& ext, KernelContext& kernel) override;
    void WarnFP(common::simd::VecDataInfo vtype, const std::u16string_view func);
    // internal use, won't check type
    xcomp::ReplaceResult SubgroupReduceArith(SubgroupReduceOp op, common::simd::VecDataInfo vtype, common::simd::VecDataInfo realType, const std::u32string_view ele);
    AlgorithmResult SubgroupBShufAlgo(uint8_t algoId, common::simd::VecDataInfo vtype) override;
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
    //xcomp::ReplaceResult SubgroupBroadcast(common::simd::VecDataInfo vtype, const std::u32string_view ele, const std::u32string_view idx) override;
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
    std::vector<common::simd::VecDataInfo> CommonSupport;
    bool EnableIntel = false, EnableIntel16 = false, EnableIntel8 = false;
    [[nodiscard]] static std::u32string VectorPatch(const std::u32string_view funcName, const std::u32string_view baseFunc,
        common::simd::VecDataInfo vtype, common::simd::VecDataInfo mid, 
        const std::u32string_view extraArg = {}, const std::u32string_view extraParam = {}) noexcept;
    [[nodiscard]] forceinline static std::u32string VectorPatchBcastShuf(const std::u32string_view funcName, 
        const std::u32string_view baseFunc, common::simd::VecDataInfo vtype, common::simd::VecDataInfo mid) noexcept
    {
        return VectorPatch(funcName, baseFunc, vtype, mid, U", sgId", U", const uint sgId");
    }
    void OnFinish(NLCLRuntime& runtime, const NLCLSubgroupExtension& ext, KernelContext& kernel) override;
    xcomp::ReplaceResult DirectShuffle(common::simd::VecDataInfo vtype, const std::u32string_view ele, const std::u32string_view idx);
    xcomp::ReplaceResult SubgroupReduceArith(SubgroupReduceOp op, common::simd::VecDataInfo vtype, const std::u32string_view ele);
    xcomp::ReplaceResult SubgroupReduceBitwise(SubgroupReduceOp op, common::simd::VecDataInfo vtype, const std::u32string_view ele);
    AlgorithmResult SubgroupBShufAlgo(uint8_t algoId, common::simd::VecDataInfo vtype) override;
public:
    NLCLSubgroupIntel(common::mlog::MiniLogger<false>& logger, NLCLContext& context, NLCLSubgroupCapbility cap);
    ~NLCLSubgroupIntel() override { }

    //xcomp::ReplaceResult SubgroupBroadcast(common::simd::VecDataInfo vtype, const std::u32string_view ele, const std::u32string_view idx) override;
    //xcomp::ReplaceResult SubgroupShuffle(common::simd::VecDataInfo vtype, const std::u32string_view ele, const std::u32string_view idx) override;
    xcomp::ReplaceResult SubgroupReduce(SubgroupReduceOp op, common::simd::VecDataInfo vtype, const std::u32string_view ele) override;

};

class NLCLSubgroupLocal : public NLCLSubgroupKHR
{
protected:
    std::u32string_view KernelName;
    bool NeedSubgroupSize = false, NeedLocalTemp = false;
    void OnBegin(NLCLRuntime& runtime, const NLCLSubgroupExtension& ext, KernelContext& kernel) override;
    void OnFinish(NLCLRuntime& runtime, const NLCLSubgroupExtension& ext, KernelContext& kernel) override;
    std::u32string GenerateKID(std::u32string_view type) const noexcept;
    std::u32string BroadcastPatch(const std::u32string_view funcName, const common::simd::VecDataInfo vtype) noexcept;
    std::u32string ShufflePatch(const std::u32string_view funcName, const common::simd::VecDataInfo vtype) noexcept;
public:
    using NLCLSubgroupKHR::NLCLSubgroupKHR;
    ~NLCLSubgroupLocal() override { }

    xcomp::ReplaceResult GetSubgroupSize() override;
    xcomp::ReplaceResult GetMaxSubgroupSize() override;
    xcomp::ReplaceResult GetSubgroupCount() override;
    xcomp::ReplaceResult GetSubgroupId() override;
    xcomp::ReplaceResult GetSubgroupLocalId() override;
    xcomp::ReplaceResult SubgroupAll(const std::u32string_view predicate) override;
    xcomp::ReplaceResult SubgroupAny(const std::u32string_view predicate) override;
    xcomp::ReplaceResult SubgroupBroadcast(common::simd::VecDataInfo vtype, const std::u32string_view ele, const std::u32string_view idx) override;
    xcomp::ReplaceResult SubgroupShuffle(common::simd::VecDataInfo vtype, const std::u32string_view ele, const std::u32string_view idx) override;

};

class NLCLSubgroupPtx : public SubgroupProvider
{
protected:
    std::u32string_view ExtraSync, ExtraMask;
    const uint32_t SMVersion;
    std::u32string ShufflePatch(const uint8_t bit) noexcept;
    xcomp::ReplaceResult SubgroupReduceSM80(SubgroupReduceOp op, common::simd::VecDataInfo vtype, const std::u32string_view ele);
    xcomp::ReplaceResult SubgroupReduceSM30(SubgroupReduceOp op, common::simd::VecDataInfo vtype, const std::u32string_view ele);
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
    xcomp::ReplaceResult SubgroupBroadcast(common::simd::VecDataInfo vtype, const std::u32string_view ele, const std::u32string_view idx) override;
    xcomp::ReplaceResult SubgroupShuffle(common::simd::VecDataInfo vtype, const std::u32string_view ele, const std::u32string_view idx) override;
    xcomp::ReplaceResult SubgroupReduce(SubgroupReduceOp op, common::simd::VecDataInfo vtype, const std::u32string_view ele) override;

};



}