#include "XCompNailang.h"
#include "StringUtil/Convert.h"
#include "common/StrParsePack.hpp"
#include <shared_mutex>

namespace xcomp
{
using namespace std::string_literals;
using namespace std::string_view_literals;
using xziar::nailang::Arg;
using xziar::nailang::RawArg;
using xziar::nailang::Block;
using xziar::nailang::RawBlock;
using xziar::nailang::BlockContent;
using xziar::nailang::FuncCall;
using xziar::nailang::ArgLimits;
using xziar::nailang::FrameFlags;
using xziar::nailang::NailangRuntimeBase;
using xziar::nailang::NailangRuntimeException;
using xziar::nailang::detail::ExceptionTarget;
using common::mlog::LogLevel;
using common::str::Charset;
using common::str::IsBeginWith;
using common::simd::VecDataInfo;
using FuncInfo = xziar::nailang::FuncName::FuncInfo;

MAKE_ENABLER_IMPL(ReplaceDepend)
MAKE_ENABLER_IMPL(XCNLProgram)


#define NLRT_THROW_EX(...) this->HandleException(CREATE_EXCEPTION(NailangRuntimeException, __VA_ARGS__))
#define APPEND_FMT(str, syntax, ...) fmt::format_to(std::back_inserter(str), FMT_STRING(syntax), __VA_ARGS__)



struct TemplateBlockInfo : public OutputBlock::BlockInfo
{
    std::vector<std::u32string_view> ReplaceArgs;
    TemplateBlockInfo(common::span<const xziar::nailang::RawArg> args)
    {
        ReplaceArgs.reserve(args.size());
        for (const auto& arg : args)
        {
            ReplaceArgs.emplace_back(*arg.GetVar<RawArg::Type::Var>());
        }
    }
    ~TemplateBlockInfo() override {}
};


std::u32string_view OutputBlock::BlockTypeName(const BlockType type) noexcept
{
    switch (type)
    {
    case BlockType::Global:     return U"Global"sv;
    case BlockType::Struct:     return U"Struct"sv;
    case BlockType::Instance:   return U"Instance"sv;
    case BlockType::Template:   return U"Template"sv;
    default:                    return U"Unknwon"sv;
    }
}


NamedTextHolder::NamedText::NamedText(std::u32string_view content,
    common::StringPiece<char32_t> idstr, size_t offset, size_t cnt) :
    Content(content), IDStr(idstr), Dependency{gsl::narrow_cast<uint32_t>(offset), gsl::narrow_cast<uint32_t>(cnt) }
{ }

NamedTextHolder::~NamedTextHolder()
{ }

void NamedTextHolder::ForceAdd(std::vector<NamedText>& container, std::u32string_view id, common::str::StrVariant<char32_t> content, U32StrSpan depends)
{
    const auto offset = Dependencies.size();
    Dependencies.reserve(offset + depends.size());
    for (const auto& dep : depends)
    {
        if (dep == id) // early check for self-dependency
            COMMON_THROW(common::BaseException, FMTSTR(u"self dependency at [{}] for [{}]"sv, &dep - depends.data(), id));

        bool match = false;
        for (size_t i = 0; i < container.size(); ++i)
        {
            const auto& item = container[i];
            if (GetID(item) == dep)
            {
                const auto idx = gsl::narrow_cast<uint32_t>(i);
                Dependencies.emplace_back(item.IDStr, idx);
                match = true;
                break;
            }
        }

        if (!match)
        {
            const auto str = Names.AllocateString(dep);
            Dependencies.emplace_back(str, UINT32_MAX);
        }
    }
    const auto idstr = Names.AllocateString(id);
    container.push_back({ content.ExtractStr(), idstr, offset, depends.size() });
}

void NamedTextHolder::Write(std::u32string& output, const NamedText& item) const
{
    APPEND_FMT(output, U"    //vvvvvvvv below injected by {}  vvvvvvvv\r\n"sv, GetID(item));
    output.append(item.Content).append(U"\r\n"sv);
    APPEND_FMT(output, U"    //^^^^^^^^ above injected by {}  ^^^^^^^^\r\n\r\n"sv, GetID(item));
}

void NamedTextHolder::Write(std::u32string& output, const std::vector<NamedText>& container) const
{
    if (Dependencies.empty() || container.empty()) // fast path
    {
        for (const auto& item : container)
            Write(output, item);
        return;
    }
    std::vector<bool> outputMask(container.size(), false);
    size_t waiting = container.size();
    while (waiting > 0)
    {
        const auto last = waiting;
        for (size_t i = 0; i < container.size(); ++i)
        {
            if (outputMask[i])
                continue;
            const auto& item = container[i];
            if (item.DependCount() != 0)
            {
                bool satisfy = true;
                for (uint32_t j = 0; satisfy && j < item.DependCount(); ++j)
                {
                    const auto depIdx = SearchDepend(container, item, j);
                    if (!depIdx.has_value())
                    {
                        satisfy = false;
                        COMMON_THROW(common::BaseException, FMTSTR(u"unsolved dependency [{}] for [{}]"sv,
                            GetDepend(item, j), GetID(item)));
                    }
                    else if (!outputMask[depIdx.value()])
                    {
                        satisfy = false;
                    }
                }
                if (!satisfy) // skip this item
                    continue;
            }
            Write(output, item);
            outputMask[i] = true;
            waiting--;
        }
        if (last == waiting) // some item cannot be resolved
            break;
    }
    if (waiting > 0) // generate report and throw
    {
        std::u16string report = u"unmatched dependencies:\r\n"s;
        for (size_t i = 0; i < container.size(); ++i)
        {
            if (outputMask[i])
                continue;
            const auto& item = container[i];
            Expects(item.DependCount() != 0);

            APPEND_FMT(report, u"[{}]: total [{}] depends, unmatch:[ "sv, GetID(item), item.DependCount());
            for (uint32_t j = 0, unmth = 0; j < item.DependCount(); ++j)
            {
                const auto depIdx = SearchDepend(container, item, j);
                Expects(depIdx.has_value());
                if (!outputMask[depIdx.value()])
                {
                    if (unmth++ > 0)
                        report.append(u", "sv);
                    const auto dstName = GetID(container[depIdx.value()]);
                    report.append(common::str::to_u16string(dstName));
                }
            }
            report.append(u" ]\r\n"sv);
        }
        COMMON_THROW(common::BaseException, report);
    }
}

std::shared_ptr<const ReplaceDepend> NamedTextHolder::GenerateReplaceDepend(const NamedText& item, bool isPthBlk, 
    U32StrSpan pthBlk, U32StrSpan bdyPfx) const
{
    if (item.DependCount() == 0)
    {
        return ReplaceDepend::Create(pthBlk, bdyPfx);
    }

    std::vector<std::u32string_view> newDeps;
    auto& oldDeps = isPthBlk ? pthBlk : bdyPfx;
    newDeps.reserve(oldDeps.size() + item.DependCount());
    newDeps.insert(newDeps.begin(), oldDeps.begin(), oldDeps.end());
    for (uint32_t i = 0; i < item.DependCount(); ++i)
    {
        newDeps.push_back(GetDepend(item, i));
    }
    oldDeps = newDeps;
    return ReplaceDepend::Create(pthBlk, bdyPfx);
}


std::shared_ptr<const ReplaceDepend> ReplaceDepend::Create(U32StrSpan patchedBlk, U32StrSpan bodyPfx)
{
    static_assert(sizeof(std::u32string_view) % sizeof(char32_t) == 0);

    const auto count = patchedBlk.size() + bodyPfx.size();
    if (count == 0)
        return Empty();

    const auto ret = MAKE_ENABLER_SHARED(ReplaceDepend, ());
    std::vector<common::StringPiece<char32_t>> tmp; tmp.reserve(count + 1);
    {
        std::u32string dummy; dummy.resize(count * sizeof(std::u32string_view) / sizeof(char32_t));
        tmp.push_back(ret->Names.AllocateString(dummy));
    }
    for (const auto& str : patchedBlk)
    {
        tmp.push_back(ret->Names.AllocateString(str));
    }
    for (const auto& str : bodyPfx)
    {
        tmp.push_back(ret->Names.AllocateString(str));
    }

    const auto tmp2Space = ret->Names.GetStringView(tmp[0]);
    const auto ptr = reinterpret_cast<std::u32string_view*>(const_cast<char32_t*>(tmp2Space.data()));
    const common::span<std::u32string_view> space{ ptr, count };
    for (size_t i = 0; i < count; ++i)
    {
        space[i] = ret->Names.GetStringView(tmp[i + 1]);
    }

    ret->PatchedBlock = space.subspan(0, patchedBlk.size());
    ret->BodyPrefixes = space.subspan(patchedBlk.size(), bodyPfx.size());
    return ret;
}

std::shared_ptr<const ReplaceDepend> ReplaceDepend::Empty()
{
    static auto EMPTY = MAKE_ENABLER_SHARED(ReplaceDepend, ());
    return EMPTY;
}


XCNLExtension::XCNLExtension(XCNLContext& context) : Context(context), ID(UINT32_MAX)
{ }
XCNLExtension::~XCNLExtension() { }


struct ExtHolder
{
    std::vector<XCNLExtension::XCNLExtGen> Container;
    std::shared_mutex Mutex;
    auto LockShared() { return std::shared_lock<std::shared_mutex>(Mutex); }
    auto LockUnique() { return std::unique_lock<std::shared_mutex>(Mutex); }
};
static auto& XCNLExtGenerators() noexcept
{
    static ExtHolder Holder;
    return Holder;
}
uintptr_t XCNLExtension::RegExt(XCNLExtGen creator) noexcept
{
    auto& holder = XCNLExtGenerators();
    const auto locker = holder.LockUnique();
    holder.Container.emplace_back(creator);
    return reinterpret_cast<uintptr_t>(creator);
}


XCNLContext::XCNLContext(const common::CLikeDefines& info)
{
    for (const auto [key, val] : info)
    {
        const auto varName = common::str::to_u32string(key, Charset::UTF8);
        const auto var = xziar::nailang::LateBindVar::CreateSimple(varName);
        switch (val.index())
        {
        case 1: SetArg(var, std::get<1>(val), true); break;
        case 2: SetArg(var, std::get<2>(val), true); break;
        case 3: SetArg(var, std::get<3>(val), true); break;
        case 4: SetArg(var, common::str::to_u32string(std::get<4>(val), Charset::UTF8), true); break;
        case 0:
        default:
            break;
        }
    }
}
XCNLContext::~XCNLContext()
{ }

XCNLExtension* XCNLContext::FindExt(XCNLExtension::XCNLExtGen func) const
{
    const auto id = reinterpret_cast<uintptr_t>(func);
    for (const auto& ext : Extensions)
    {
        if (ext->ID == id)
            return ext.get();
    }
    return nullptr;
}

void XCNLContext::Write(std::u32string& output, const NamedText& item) const
{
    APPEND_FMT(output, U"/* Patched Block [{}] */\r\n"sv, GetID(item));
    output.append(item.Content).append(U"\r\n\r\n"sv);
}


XCNLRuntime::XCNLRuntime(common::mlog::MiniLogger<false>& logger, std::shared_ptr<XCNLContext> evalCtx) :
    NailangRuntimeBase(evalCtx), XCContext(*evalCtx), Logger(logger)
{ }
XCNLRuntime::~XCNLRuntime()
{ }

void XCNLRuntime::HandleException(const xziar::nailang::NailangParseException& ex) const
{
    Logger.error(u"{}\n", ex.Message());
    ReplaceEngine::HandleException(ex);
}

void XCNLRuntime::HandleException(const NailangRuntimeException& ex) const
{
    Logger.error(u"{}\n", ex.Message());
    NailangRuntimeBase::HandleException(ex);
}

void XCNLRuntime::ThrowByReplacerArgCount(const std::u32string_view call, U32StrSpan args,
    const size_t count, const ArgLimits limit) const
{
    std::u32string_view prefix;
    switch (limit)
    {
    case ArgLimits::Exact:
        if (args.size() == count) return;
        prefix = U""; break;
    case ArgLimits::AtMost:
        if (args.size() <= count) return;
        prefix = U"at most"; break;
    case ArgLimits::AtLeast:
        if (args.size() >= count) return;
        prefix = U"at least"; break;
    }
    NLRT_THROW_EX(FMTSTR(u"Repalcer-Func [{}] requires {} [{}] args, which gives [{}]."sv, call, prefix, count, args.size()));
}

void XCNLRuntime::InnerLog(common::mlog::LogLevel level, std::u32string_view str)
{
    Logger.log(level, u"[XCNL]{}\n", str);
}

static std::u16string VecTypeErrorStr(const std::u32string_view var, const std::u16string_view reason, const std::variant<std::u16string_view, std::function<std::u16string(void)>>& exInfo)
{
    switch (exInfo.index())
    {
    case 0:
        if (const auto str = std::get<0>(exInfo); str.size() == 0)
            return FMTSTR(u"Type [{}] {}."sv, var, reason);
        else
            return FMTSTR(u"Type [{}] {} when {}."sv, var, reason, str);
    case 1:
        return FMTSTR(u"Type [{}] {}, {}."sv, var, reason, std::get<1>(exInfo)());
    default:
        return {};
    }
}

common::simd::VecDataInfo XCNLRuntime::ParseVecType(const std::u32string_view var,
    std::variant<std::u16string_view, std::function<std::u16string(void)>> extraInfo) const
{
    const auto vtype = XCContext.ParseVecType(var);
    if (!vtype)
        NLRT_THROW_EX(VecTypeErrorStr(var, u"not recognized as VecType"sv, extraInfo));
    if (!vtype.TypeSupport)
        Logger.warning(u"Potential use of unsupported type[{}] with [{}].\n", StringifyVDataType(vtype.Info), var);

    return vtype.Info;
}

std::u32string_view XCNLRuntime::GetVecTypeName(const std::u32string_view vname,
    std::variant<std::u16string_view, std::function<std::u16string(void)>> extraInfo) const
{
    const auto vtype = ParseVecType(vname, extraInfo);
    const auto str = XCContext.GetVecTypeName(vtype);
    if (str.empty())
        NLRT_THROW_EX(VecTypeErrorStr(vname, u"not supported"sv, extraInfo));
    return str;
}

constexpr auto LogLevelParser = SWITCH_PACK(Hash, 
    (U"error",   common::mlog::LogLevel::Error), 
    (U"success", common::mlog::LogLevel::Success),
    (U"warning", common::mlog::LogLevel::Warning),
    (U"info",    common::mlog::LogLevel::Info),
    (U"verbose", common::mlog::LogLevel::Verbose), 
    (U"debug",   common::mlog::LogLevel::Debug));
std::optional<common::mlog::LogLevel> ParseLogLevel(const Arg& arg) noexcept
{
    const auto str = arg.GetStr();
    if (str)
        return LogLevelParser(str.value());
    return {};
}
std::optional<Arg> XCNLRuntime::CommonFunc(const std::u32string_view name, const FuncCall& call, MetaFuncs)
{
    switch (hash_(name))
    {
    HashCase(name, U"GetVecTypeName")
    {
        const auto arg = EvaluateFirstFuncArg(call, Arg::Type::String);
        return GetVecTypeName(arg.GetStr().value());
    }
    HashCase(name, U"Log")
    {
        ThrowIfNotFuncTarget(call, FuncInfo::Empty);
        const auto args2 = EvaluateFuncArgs<2, ArgLimits::AtLeast>(call, { Arg::Type::String, Arg::Type::String });
        const auto logLevel = ParseLogLevel(args2[0]);
        if (!logLevel)
            NLRT_THROW_EX(u"Arg[0] of [Log] should be LogLevel"sv, call);
        const auto fmtStr = args2[1].GetStr().value();
        if (call.Args.size() == 2)
            InnerLog(logLevel.value(), fmtStr);
        else
        {
            try
            {
                const auto str = FormatString(fmtStr, call.Args.subspan(2));
                InnerLog(logLevel.value(), str);
            }
            catch (const xziar::nailang::NailangFormatException& nfe)
            {
                Logger.error(u"Error when formating inner log: {}\n", nfe.Message());
            }
        }
    } return Arg{};
    default: break;
    }
    return {};
}

std::optional<common::str::StrVariant<char32_t>> XCNLRuntime::CommonReplaceFunc(const std::u32string_view name, const std::u32string_view call,
    U32StrSpan args, BlockCookie& cookie)
{
    switch (hash_(name))
    {
    HashCase(name, U"GetVecTypeName")
    {
        ThrowByReplacerArgCount(call, args, 1, ArgLimits::Exact);
        return GetVecTypeName(args[0]);
    }
    HashCase(name, U"CodeBlock")
    {
        ThrowByReplacerArgCount(call, args, 1, ArgLimits::AtLeast);
        const OutputBlock* block = nullptr;
        for (const auto& blk : XCContext.TemplateBlocks)
            if (blk.Block->Name == args[0])
            {
                block = &blk; break;
            }
        if (block)
        {
            const auto& extra = static_cast<const TemplateBlockInfo&>(*block->ExtraInfo);
            ThrowByReplacerArgCount(call, args, extra.ReplaceArgs.size() + 1, ArgLimits::AtLeast);
            auto frame = PushFrame(extra.ReplaceArgs.size() > 0, FrameFlags::FlowScope);
            for (size_t i = 0; i < extra.ReplaceArgs.size(); ++i)
            {
                const auto var_ = DecideDynamicVar(extra.ReplaceArgs[i], u"CodeBlock"sv);
                {
                    using xziar::nailang::LateBindVar;
                    auto& var2 = const_cast<LateBindVar&>(var_.Get());
                    var2.Info() |= LateBindVar::VarInfo::Local;
                }
                const auto ret = LookUpArg(var_);
                SetArg(var_, args[i + 1]);
            }
            auto output = FMTSTR(U"// template block [{}]\r\n"sv, args[0]);
            NestedCookie newCookie(cookie, *block);
            DirectOutput(newCookie, output);
            return output;
        }
        break;
    }
    default: break;
    }
    return {};
}

ReplaceResult XCNLRuntime::ExtensionReplaceFunc(std::u32string_view func, U32StrSpan args)
{
    for (const auto& ext : XCContext.Extensions)
    {
        if (!ext) continue;
        auto ret = ext->ReplaceFunc(*this, func, args);
        const auto str = ret.GetStr();
        if (!ret && ret.CheckAllowFallback())
        {
            if (!str.empty())
                Logger.warning(FMT_STRING(u"when replace-func [{}]: {}\r\n"), func, ret.GetStr());
            continue;
        }
        if (ret)
        {
            if (const auto dep = ret.GetDepends(); dep)
            {
                auto txt = FMTSTR(u"Result for ReplaceFunc[{}] : [{}]\r\n"sv, func, str);
                if (!dep->PatchedBlock.empty())
                {
                    txt += u"Depend on PatchedBlock:\r\n";
                    for (const auto& name : dep->PatchedBlock)
                        APPEND_FMT(txt, u" - [{}]\r\n", name);
                }
                if (!dep->BodyPrefixes.empty())
                {
                    txt += u"Depend on BodyPrefix:\r\n";
                    for (const auto& name : dep->BodyPrefixes)
                        APPEND_FMT(txt, u" - [{}]\r\n", name);
                }
                Logger.verbose(txt);
            }
        }
        return ret;
    }
    return { FMTSTR(U"[{}] with [{}]args not resolved", func, args.size()), false };
}

void XCNLRuntime::OutputConditions(MetaFuncs metas, std::u32string& dst) const
{
    // std::set<std::u32string_view> lateVars;
    for (const auto& meta : metas)
    {
        std::u32string_view prefix;
        if (*meta.Name == U"If"sv)
            prefix = U"If "sv;
        else if (*meta.Name == U"Skip"sv)
            prefix = U"Skip if "sv;
        else
            continue;
        if (meta.Args.size() == 1)
            APPEND_FMT(dst, U"    /* {:7} :  {} */\r\n"sv, prefix, xziar::nailang::Serializer::Stringify(meta.Args[0]));
    }
}

void XCNLRuntime::DirectOutput(BlockCookie& cookie, std::u32string& dst)
{
    Expects(CurFrame != nullptr);
    const auto& block = cookie.Block;
    OutputConditions(block.MetaFunc, dst);
    common::str::StrVariant<char32_t> source(block.Block->Source);
    for (const auto& [var, arg] : block.PreAssignArgs)
    {
        SetArg(*var, EvaluateArg(arg));
    }
    if (block.ReplaceVar || block.ReplaceFunc)
        source = ProcessOptBlock(source.StrView(), U"$$@"sv, U"@$$"sv);
    if (block.ReplaceVar)
        source = ProcessVariable(source.StrView(), U"$$!{"sv, U"}"sv);
    if (block.ReplaceFunc)
        source = ProcessFunction(source.StrView(), U"$$!"sv, U""sv, &cookie);
    dst.append(source.StrView());
}

void XCNLRuntime::ProcessInstance(BlockCookie& cookie)
{
    auto& kerCtx = *cookie.GetInstanceCtx();
    for (const auto& ext : XCContext.Extensions)
    {
        ext->BeginInstance(*this, kerCtx);
    }

    for (const auto& meta : cookie.Block.MetaFunc)
    {
        switch (const auto newMeta = HandleMetaIf(meta); newMeta.index())
        {
        case 0:     HandleInstanceMeta(meta, kerCtx); break;
        case 2:     HandleInstanceMeta({ std::get<2>(newMeta).Ptr(), meta.Args.subspan(2) }, kerCtx); break;
        default:    break;
        }
    }

    DirectOutput(cookie, kerCtx.Content);

    for (const auto& ext : XCContext.Extensions)
    {
        ext->FinishInstance(*this, kerCtx);
    }
}

void XCNLRuntime::OnReplaceOptBlock(std::u32string& output, void*, std::u32string_view cond, std::u32string_view content)
{
    if (cond.empty())
    {
        NLRT_THROW_EX(u"Empty condition occurred when replace-opt-block"sv);
        return;
    }
    Arg ret;
    if (cond.back() == U';')
    {
        ret = EvaluateRawStatement(cond);
    }
    else
    {
        std::u32string str(cond);
        str.push_back(U';');
        ret = EvaluateRawStatement(str);
    }
    if (ret.IsEmpty())
    {
        NLRT_THROW_EX(u"No value returned as condition when replace-opt-block"sv);
        return;
    }
    else if (!HAS_FIELD(ret.TypeData, Arg::Type::Boolable))
    {
        NLRT_THROW_EX(u"Evaluated condition is not boolable when replace-opt-block"sv);
        return;
    }
    else if (ret.GetBool().value())
    {
        output.append(content);
    }
}

void XCNLRuntime::OnReplaceVariable(std::u32string& output, [[maybe_unused]] void* cookie, std::u32string_view var)
{
    if (var.size() == 0)
        NLRT_THROW_EX(u"replace-variable does not accept empty"sv);

    if (var[0] == U'@')
    {
        const auto str = GetVecTypeName(var.substr(1), u"replace-variable"sv);
        output.append(str);
    }
    else // '@' and '#' not allowed in var anyway
    {
        bool toVecName = false;
        if (var[0] == U'#')
            toVecName = true, var.remove_prefix(1);
        const auto var_ = DecideDynamicVar(var, u"ReplaceVariable"sv);
        const auto val = LookUpArg(var_);
        if (val.IsEmpty() || val.TypeData == Arg::Type::Var)
        {
            NLRT_THROW_EX(FMTSTR(u"Arg [{}] not found when replace-variable"sv, var));
            return;
        }
        if (toVecName)
        {
            if (!val.IsStr())
                NLRT_THROW_EX(FMTSTR(u"Arg [{}] is not a vetype string when replace-variable"sv, var));
            const auto str = GetVecTypeName(val.GetStr().value(), u"replace-variable"sv);
            output.append(str);
        }
        else
            output.append(val.ToString().StrView());
    }
}

void XCNLRuntime::OnReplaceFunction(std::u32string& output, void* cookie, std::u32string_view func, U32StrSpan args)
{
    if (IsBeginWith(func, U"xcomp."sv))
    {
        const auto subName = func.substr(6);
        auto ret = CommonReplaceFunc(subName, func, args, *reinterpret_cast<BlockCookie*>(cookie));
        if (ret.has_value())
        {
            output.append(ret->StrView());
            return;
        }
    }

    {
        const auto ret = ExtensionReplaceFunc(func, args);
        if (ret)
        {
            output.append(ret.GetStr());
        }
        else
        {
            NLRT_THROW_EX(FMTSTR(u"replace-func [{}] with [{}]args error: {}", func, args.size(), ret.GetStr()));
        }
    }
}

void XCNLRuntime::OnRawBlock(const RawBlock& block, MetaFuncs metas)
{
    ProcessRawBlock(block, metas);
}

Arg XCNLRuntime::EvaluateFunc(const FuncCall& call, MetaFuncs metas)
{
    if (call.Name->PartCount == 2 && (*call.Name)[0] == U"xcomp"sv)
    {
        auto ret = CommonFunc((*call.Name)[1], call, metas);
        if (ret.has_value())
            return std::move(ret.value());
    }
    for (const auto& ext : XCContext.Extensions)
    {
        auto ret = ext->XCNLFunc(*this, call, metas);
        if (ret)
            return std::move(ret.value());
    }
    return NailangRuntimeBase::EvaluateFunc(call, metas);
}

OutputBlock::BlockType XCNLRuntime::GetBlockType(const RawBlock& block, MetaFuncs) const noexcept
{
    switch (hash_(block.Type))
    {
    HashCase(block.Type, U"xcomp.Global")   return OutputBlock::BlockType::Global;
    HashCase(block.Type, U"xcomp.Struct")   return OutputBlock::BlockType::Struct;
    HashCase(block.Type, U"xcomp.Kernel")   return OutputBlock::BlockType::Instance;
    HashCase(block.Type, U"xcomp.Template") return OutputBlock::BlockType::Template;
    default:                                break;
    }
    return OutputBlock::BlockType::None;
}

std::unique_ptr<OutputBlock::BlockInfo> XCNLRuntime::PrepareBlockInfo(OutputBlock& blk)
{
    if (blk.Type == OutputBlock::BlockType::Template)
    {
        common::span<const RawArg> tpArgs;
        for (const auto& meta : blk.MetaFunc)
        {
            if (*meta.Name == U"xcomp.TemplateArgs")
            {
                for (uint32_t i = 0; i < meta.Args.size(); ++i)
                {
                    if (meta.Args[i].TypeData != RawArg::Type::Var)
                        NLRT_THROW_EX(FMTSTR(u"TemplateArgs's arg[{}] is [{}]. not [Var]"sv,
                            i, meta.Args[i].GetTypeName()), meta, blk.Block);
                }
                tpArgs = meta.Args;
                break;
            }
        }
        return std::make_unique<TemplateBlockInfo>(tpArgs);
    }
    return std::move(blk.ExtraInfo);
}

void XCNLRuntime::HandleInstanceMeta(const FuncCall& meta, InstanceContext& ctx)
{
    for (const auto& ext : XCContext.Extensions)
    {
        ext->InstanceMeta(*this, meta, ctx);
    }
}

void XCNLRuntime::BeforeOutputBlock(const OutputBlock& block, std::u32string& dst) const
{
    APPEND_FMT(dst, U"\r\n/* From {} Block [{}] */\r\n"sv, block.GetBlockTypeName(), block.Block->Name);

    // std::set<std::u32string_view> lateVars;

    OutputConditions(block.MetaFunc, dst);
}

void XCNLRuntime::BeforeFinishOutput(std::u32string&, std::u32string&)
{ }

void XCNLRuntime::ProcessRawBlock(const xziar::nailang::RawBlock& block, MetaFuncs metas)
{
    auto frame = PushFrame(FrameFlags::FlowScope);

    if (!HandleMetaFuncs(metas, BlockContent::Generate(&block)))
        return;
    
    const auto type = GetBlockType(block, metas);
    if (type == OutputBlock::BlockType::None)
        return;
    auto& dst = type == OutputBlock::BlockType::Template ? XCContext.TemplateBlocks : XCContext.OutputBlocks;
    dst.emplace_back(&block, metas, type);
    auto& blk = dst.back();

    for (const auto& fcall : metas)
    {
        if (*fcall.Name == U"xcomp.ReplaceVariable"sv)
            blk.ReplaceVar = true;
        else if (*fcall.Name == U"xcomp.ReplaceFunction"sv)
            blk.ReplaceFunc = true;
        else if (*fcall.Name == U"xcomp.Replace"sv)
            blk.ReplaceVar = blk.ReplaceFunc = true;
        else if (*fcall.Name == U"xcomp.PreAssign"sv)
        {
            ThrowByArgCount(fcall, 2, ArgLimits::Exact);
            ThrowByArgType(fcall, RawArg::Type::Var, 0);
            blk.PreAssignArgs.emplace_back(fcall.Args[0].GetVar<RawArg::Type::Var>(), fcall.Args[1]);
        }
    }

    blk.ExtraInfo = PrepareBlockInfo(blk);
}

std::string XCNLRuntime::GenerateOutput()
{
    std::u32string prefix, output;

    for (const auto& block : XCContext.OutputBlocks)
    {
        if (block.Type == OutputBlock::BlockType::None || block.Type == OutputBlock::BlockType::Template)
        {
            Logger.warning(u"Unexpected RawBlock (Type[{}], Name[{}]) when generating output.\n", block.Block->Type, block.Block->Name); break;
            continue;
        }
        BeforeOutputBlock(block, output);
        auto frame = PushFrame(FrameFlags::FlowScope);
        if (block.Type == OutputBlock::BlockType::Instance)
        {
            auto cookie = PrepareInstance(block);
            OutputInstance(*cookie, output);
        }
        else
        {
            BlockCookie cookie(block);
            switch (block.Type)
            {
            case OutputBlock::BlockType::Global:    DirectOutput(cookie, output); continue;
            case OutputBlock::BlockType::Struct:    OutputStruct(cookie, output); continue;
            default:                                Expects(false); break;
            }
        }
    }
    for (const auto& ext : XCContext.Extensions)
    {
        ext->FinishXCNL(*this);
    }

    BeforeFinishOutput(prefix, output);

    // Output patched blocks
    XCContext.WritePatchedBlock(prefix);

    return common::str::to_string(prefix + output, Charset::UTF8, Charset::UTF32);
}


class XComputeParser : xziar::nailang::BlockParser
{
    using BlockParser::BlockParser;
public:
    static void GetBlock(xziar::nailang::MemoryPool& pool, const std::u32string_view src, xziar::nailang::Block& dst)
    {
        common::parser::ParserContext context(src);
        XComputeParser parser(pool, context);
        parser.ParseContentIntoBlock(true, dst);
    }
};


std::shared_ptr<XCNLProgram> XCNLProgram::Create_(std::u32string source)
{
    return MAKE_ENABLER_SHARED(XCNLProgram, (std::move(source)));
}

std::shared_ptr<XCNLProgram> XCNLProgram::Create(std::u32string source)
{
    return CreateBy<XComputeParser>(std::move(source));
}


XCNLProgStub::XCNLProgStub(const std::shared_ptr<const XCNLProgram>& program, std::shared_ptr<XCNLContext>&& context,
    std::unique_ptr<XCNLRuntime>&& runtime) : Program(program), Context(std::move(context)), Runtime(std::move(runtime))
{
    Prepare(Runtime->Logger);
}
XCNLProgStub::~XCNLProgStub()
{ }
void XCNLProgStub::Prepare(common::mlog::MiniLogger<false>& logger)
{
    Context->Extensions.clear();
    auto& holder = XCNLExtGenerators();
    const auto locker = holder.LockShared();
    for (const auto& creator : holder.Container)
    {
        auto ext = creator(logger, *Context);
        if (ext)
        {
            ext->ID = reinterpret_cast<uintptr_t>(creator);
            Context->Extensions.push_back(std::move(ext));
        }
    }
}


}