#include "NailangParser.h"
#include "ParserRely.h"
#include "StringUtil/Format.h"
#include <boost/range/adaptor/reversed.hpp>

namespace xziar::nailang
{
using tokenizer::NailangToken;


std::vector<PartedName::PartType> PartedName::GetParts(std::u32string_view name)
{
    std::vector<PartType> parts;
    for (const auto piece : common::str::SplitStream(name, U'.', true))
    {
        const auto offset = piece.data() - name.data();
        if (offset > UINT16_MAX)
            COMMON_THROW(NailangPartedNameException, u"Name too long"sv, name, piece);
        if (piece.size() > UINT16_MAX)
            COMMON_THROW(NailangPartedNameException, u"Name part too long"sv, name, piece);
        if (piece.size() == 0)
            COMMON_THROW(NailangPartedNameException, u"Empty name part"sv, name, piece);
        parts.emplace_back(static_cast<uint16_t>(offset), static_cast<uint16_t>(piece.size()));
    }
    return parts;
}
PartedName* PartedName::Create(MemoryPool& pool, std::u32string_view name, uint16_t exinfo)
{
    Expects(name.size() > 0);
    const auto parts = GetParts(name);
    if (parts.size() == 1)
    {
        const auto space = pool.Alloc<PartedName>();
        const auto ptr = reinterpret_cast<PartedName*>(space.data());
        new (ptr) PartedName(name, parts, exinfo);
        return ptr;
    }
    else
    {
        const auto partSize = parts.size() * sizeof(PartType);
        const auto space = pool.Alloc(sizeof(PartedName) + partSize, alignof(PartedName));
        const auto ptr = reinterpret_cast<PartedName*>(space.data());
        new (ptr) PartedName(name, parts, exinfo);
        memcpy_s(ptr + 1, partSize, parts.data(), partSize);
        return ptr;
    }
}

struct ExpectParentheseL
{
    static constexpr ParserToken Token = ParserToken(NailangToken::Parenthese, U'(');
};
struct ExpectParentheseR
{
    static constexpr ParserToken Token = ParserToken(NailangToken::Parenthese, U')');
};
struct ExpectCurlyBraceL
{
    static constexpr ParserToken Token = ParserToken(NailangToken::CurlyBrace, U'{');
};
struct ExpectCurlyBraceR
{
    static constexpr ParserToken Token = ParserToken(NailangToken::CurlyBrace, U'}');
};


std::pair<uint32_t, uint32_t> NailangParser::GetPosition(const common::parser::ParserContext& context, const bool ignoreCol) noexcept
{
    return { gsl::narrow_cast<uint32_t>(context.Row + 1), ignoreCol ? 0u : gsl::narrow_cast<uint32_t>(context.Col) };
}


std::u16string NailangParser::DescribeTokenID(const uint16_t tid) const noexcept
{
#define RET_TK_ID(type) case NailangToken::type:        return u ## #type
    switch (static_cast<NailangToken>(tid))
    {
        RET_TK_ID(Raw);
        RET_TK_ID(Block);
        RET_TK_ID(MetaFunc);
        RET_TK_ID(Func);
        RET_TK_ID(Var);
        RET_TK_ID(SubField);
        RET_TK_ID(EmbedOp);
        RET_TK_ID(Parenthese);
        RET_TK_ID(SquareBracket);
        RET_TK_ID(CurlyBrace);
        RET_TK_ID(Assign);
#undef RET_TK_ID
    default:
        return ParserBase::DescribeTokenID(tid);
    }
}

common::str::StrVariant<char16_t> NailangParser::GetCurrentFileName() const noexcept
{
    if (SubScopeName.empty())
        return ParserBase::GetCurrentFileName();
    
    std::u16string fileName;
    fileName.reserve(Context.SourceName.size() + SubScopeName.size() + 3);
    fileName.append(Context.SourceName).append(u" ["sv).append(SubScopeName).append(u"]");
    return fileName;
}

void NailangParser::HandleException(const NailangParseException& ex) const
{
    auto& info = ex.GetInfo();
    info.File = GetCurrentFileName().StrView();
    info.Position = GetCurrentPosition();
    if (ex.GetDetailMessage().empty())
        ex.Attach("detail", FMTSTR(u"at row[{}] col[{}], file [{}]", info.Position.first, info.Position.second, info.File));
    ex.ThrowSelf();
}

#define NLPS_THROW_EX(...) HandleException(CREATE_EXCEPTION(NailangParseException, __VA_ARGS__))

common::parser::ParserToken NailangParser::OnUnExpectedToken(const common::parser::ParserToken& token, const std::u16string_view extraInfo) const
{
    const auto msg = fmt::format(FMT_STRING(u"Unexpected token [{}]{}{}"sv), DescribeToken(token), extraInfo.empty() ? u' ' : u',', extraInfo);
    HandleException(UnexpectedTokenException(msg, token));
    return token;
}

void NailangParser::EatLeftParenthese()
{
    EatSingleToken<ExpectParentheseL, tokenizer::ParentheseTokenizer>();
}
void NailangParser::EatRightParenthese()
{
    EatSingleToken<ExpectParentheseR, tokenizer::ParentheseTokenizer>();
}
void NailangParser::EatLeftCurlyBrace()
{
    EatSingleToken<ExpectCurlyBraceL, tokenizer::CurlyBraceTokenizer>();
}
void NailangParser::EatRightCurlyBrace()
{
    EatSingleToken<ExpectCurlyBraceR, tokenizer::CurlyBraceTokenizer>();
}


std::pair<std::optional<RawArg>, char32_t> ComplexArgParser::ParseArg(std::string_view stopDelim)
{
    using common::parser::detail::TokenMatcherHelper;
    using common::parser::detail::EmptyTokenArray;
    
    const DelimTokenizer StopTokenizer = stopDelim;
    const auto ArgLexer = ParserLexerBase<CommentTokenizer, DelimTokenizer, 
        tokenizer::ParentheseTokenizer, tokenizer::NormalFuncPrefixTokenizer,
        StringTokenizer, IntTokenizer, FPTokenizer, BoolTokenizer, 
        tokenizer::VariableTokenizer, tokenizer::EmbedOpTokenizer, 
        tokenizer::SubFieldTokenizer, tokenizer::SquareBracketTokenizer>(StopTokenizer);
    const auto OpLexer = ParserLexerBase<CommentTokenizer, DelimTokenizer,
        tokenizer::EmbedOpTokenizer, tokenizer::SubFieldTokenizer, tokenizer::SquareBracketTokenizer>(StopTokenizer);
    
    std::optional<RawArg> oprend1, oprend2;
    std::vector<RawArg> tmpQueries;
    std::optional<EmbedOps> op;
    char32_t stopChar = common::parser::special::CharEnd;

    const auto CleanupQueries = [&](std::optional<RawArg>& opr)
    {
        if (!tmpQueries.empty())
        {
            Expects(opr.has_value());
            Expects(opr->TypeData != RawArg::Type::Query);
            const auto sp = MemPool.CreateArray(tmpQueries);
            const auto query = MemPool.Create<QueryExpr>(*opr, sp);
            opr.emplace(query);
            tmpQueries.clear();
        }
    };
    const auto EnsureNotLiteralType = [&](const auto oprType, const auto token)
    {
        if (tmpQueries.empty()) // first query
        {
            switch (oprType)
            {
            case RawArg::Type::Func:
            case RawArg::Type::Unary:
            case RawArg::Type::Binary:
            case RawArg::Type::Var:
                break;
            default:
                OnUnExpectedToken(token, u"SubQuery should not follow a litteral type"sv);
                break;
            }
        }
    };

    while (true) 
    {
        const bool isAtOp = oprend1.has_value() && !op.has_value() && !oprend2.has_value();
        auto& targetOpr = op.has_value() ? oprend2 : oprend1;
        ParserToken token = isAtOp ?
            GetNextToken(OpLexer,  IgnoreBlank, IgnoreCommentToken) :
            GetNextToken(ArgLexer, IgnoreBlank, IgnoreCommentToken);

        {
            const auto type = token.GetIDEnum();
            if (type == BaseToken::Unknown || type == BaseToken::Error)
            {
                if (isAtOp)
                    OnUnExpectedToken(token, u"expect an operator"sv);
                else
                    OnUnExpectedToken(token, u"unknown or error token"sv);
            }
            if (type == BaseToken::Delim || type == BaseToken::End)
            {
                stopChar = token.GetChar();
                break;
            }
        }

        if (token.GetIDEnum<NailangToken>() == NailangToken::EmbedOp)
        {
            if (op.has_value())
                OnUnExpectedToken(token, u"already has op"sv);
            const auto opval = static_cast<EmbedOps>(token.GetUInt());
            const bool isUnary = EmbedOpHelper::IsUnaryOp(opval);
            if (isUnary && oprend1.has_value())
                OnUnExpectedToken(token, u"expect no operand before unary operator"sv);
            if (!isUnary && !oprend1.has_value())
                OnUnExpectedToken(token, u"expect operand before binary operator"sv);
            op = opval;
            CleanupQueries(targetOpr);
        }
        else if (token.GetIDEnum<NailangToken>() == NailangToken::SquareBracket) // Indexer
        {
            if (token.GetChar() == U']')
                OnUnExpectedToken(token, u"Unexpected right square bracket"sv);
            if (!targetOpr.has_value())
                OnUnExpectedToken(token, u"Indexer should follow a RawArg"sv);
            EnsureNotLiteralType(targetOpr->TypeData, token);
            auto index = ParseArg("]"sv).first;
            if (!index.has_value())
                OnUnExpectedToken(token, u"lack of index"sv);
            SubQuery::PushQuery(tmpQueries, index.value());
            // target = MemPool.Create<IndexerExpr>(*target, *index);
        }
        else if (token.GetIDEnum<NailangToken>() == NailangToken::SubField) // SubField
        {
            if (!targetOpr.has_value())
                OnUnExpectedToken(token, u"SubField should follow a RawArg"sv);
            EnsureNotLiteralType(targetOpr->TypeData, token);
            SubQuery::PushQuery(tmpQueries, token.GetString());
            // target = MemPool.Create<IndexerExpr>(*target, *index);
        }
        else
        {
            if (targetOpr.has_value())
                OnUnExpectedToken(token, u"already has oprend"sv);
#define EID(id) case common::enum_cast(id)
            switch (token.GetID())
            {
            EID(BaseToken::Uint)        : targetOpr = token.GetUInt();      break;
            EID(BaseToken::Int)         : targetOpr = token.GetInt();       break;
            EID(BaseToken::FP)          : targetOpr = token.GetDouble();    break;
            EID(BaseToken::Bool)        : targetOpr = token.GetBool();      break;
            EID(NailangToken::Var)      : 
                targetOpr = MemPool.Create<LateBindVar>(token.GetString());    
                break;
            EID(BaseToken::String)      : 
                targetOpr = ProcessString(token.GetString(), MemPool);
                break;
            EID(NailangToken::Func)  :
                targetOpr = MemPool.Create<FuncCall>(ParseFuncBody(token.GetString(), MemPool, Context, FuncName::FuncInfo::ExprPart));
                break;
            EID(NailangToken::Parenthese):
                if (token.GetChar() == U'(')
                    targetOpr = ParseArg(")"sv).first;
                else // == U')'
                    OnUnExpectedToken(token, u"Unexpected right parenthese"sv);
                break; 
            default                     : OnUnExpectedToken(token, u"Unexpected token"sv);
            }
#undef EID
        }
    }
    // exit from delim or end

    CleanupQueries(op.has_value() ? oprend2 : oprend1);
    if (!op.has_value())
    {
        Expects(!oprend2.has_value());
        return { std::move(oprend1), stopChar };
    }
    else if (EmbedOpHelper::IsUnaryOp(*op))
    {
        Expects(!oprend1.has_value());
        if (!oprend2.has_value())
            NLPS_THROW_EX(u"Lack oprend for unary operator"sv);
        return { MemPool.Create<UnaryExpr>(*op, std::move(*oprend2)), stopChar };

    }
    else
    {
        Expects(oprend1.has_value());
        if (!oprend2.has_value())
            NLPS_THROW_EX(u"Lack 2nd oprend for binary operator"sv);
        return { MemPool.Create<BinaryExpr>(*op, std::move(*oprend1), std::move(*oprend2)), stopChar };
    }
}

FuncName* ComplexArgParser::CreateFuncName(std::u32string_view name, FuncName::FuncInfo info) const
{
    try
    {
        return FuncName::Create(MemPool, name, info);
    }
    catch (const NailangPartedNameException&)
    {
        HandleException(NailangParseException(u"FuncCall's name not valid"sv));
    }
    return nullptr;
}

RawArg ComplexArgParser::ProcessString(const std::u32string_view str, MemoryPool& pool)
{
    Expects(str.size() == 0 || str.back() != U'\\');
    const auto idx = str.find(U'\\');
    if (idx == std::u32string_view::npos)
        return str;
    std::u32string output; output.reserve(str.size() - 1);
    output.assign(str.data(), idx);

    for (size_t i = idx; i < str.size(); ++i)
    {
        if (str[i] != U'\\')
            output.push_back(str[i]);
        else
        {
            switch (str[++i]) // promise '\' won't be last element.
            {
            case U'\\': output.push_back(U'\\');  break;
            case U'0' : output.push_back(U'\0');  break;
            case U'r' : output.push_back(U'\r');  break;
            case U'n' : output.push_back(U'\n');  break;
            case U't' : output.push_back(U'\t');  break;
            case U'"' : output.push_back(U'"' );  break;
            default   : output.push_back(str[i]); break;
            }
        }
    }
    const auto space = pool.CreateArray(output);
    return std::u32string_view(space.data(), space.size());
}

FuncCall ComplexArgParser::ParseFuncBody(std::u32string_view name, MemoryPool& pool, common::parser::ParserContext& context, FuncName::FuncInfo info)
{
    const auto pos = GetPosition(context);
    ComplexArgParser parser(pool, context);
    const auto funcName = parser.CreateFuncName(name, info);
    parser.EatLeftParenthese();
    std::vector<RawArg> args;
    while (true)
    {
        auto [arg, delim] = parser.ParseArg(",)"sv);
        if (arg.has_value())
            args.emplace_back(std::move(*arg));
        else
            if (delim != U')')
                parser.NLPS_THROW_EX(u"Does not allow empty argument"sv);
        if (delim == U')')
            break;
        if (delim == common::parser::special::CharEnd)
            parser.NLPS_THROW_EX(u"Expect ')' before reaching end"sv);
    }
    const auto sp = pool.CreateArray(args);
    return { funcName, sp, pos };
}

std::optional<RawArg> ComplexArgParser::ParseSingleArg(std::string_view stopDelim, MemoryPool& pool, common::parser::ParserContext& context)
{
    ComplexArgParser parser(pool, context);
    auto [arg, delim] = parser.ParseArg(stopDelim);
    bool hasMatchedDeim = false;
    if (!stopDelim.empty())
    {
        for (const auto ch : stopDelim)
            if (static_cast<char32_t>(ch) == delim)
            {
                hasMatchedDeim = true;
                break;
            }
    }
    else
        hasMatchedDeim = true;
    if (!hasMatchedDeim)
        parser.NLPS_THROW_EX(u"Expected end with delim"sv);
    return arg;
}


struct ExpectSemoColon
{
    static constexpr ParserToken Token = ParserToken(BaseToken::Delim, U';');
};
void RawBlockParser::EatSemiColon()
{
    using SemiColonTokenizer = tokenizer::KeyCharTokenizer<BaseToken::Delim, U';'>;
    EatSingleToken<ExpectSemoColon, SemiColonTokenizer>();
}

void RawBlockParser::FillBlockName(RawBlock& block)
{
    using common::parser::detail::TokenMatcherHelper;
    using common::parser::detail::EmptyTokenArray;
    constexpr auto ExpectString = TokenMatcherHelper::GetMatcher(EmptyTokenArray{}, BaseToken::String);

    EatLeftParenthese();
    constexpr auto NameLexer = ParserLexerBase<CommentTokenizer, StringTokenizer>();
    const auto sectorNameToken = ExpectNextToken(NameLexer, IgnoreBlank, IgnoreCommentToken, ExpectString);
    block.Name = sectorNameToken.GetString();
    EatRightParenthese();
}

void RawBlockParser::FillFileName(RawBlock& block) const noexcept
{
    block.FileName = Context.SourceName;
}

RawBlock RawBlockParser::FillRawBlock(const std::u32string_view name)
{
    RawBlock block;
    block.Position = GetPosition(Context, true);
    block.Type = name;

    FillBlockName(block);

    EatLeftCurlyBrace();

    ContextReader reader(Context);
    auto guardString = std::u32string(reader.ReadLine());
    FillFileName(block);

    guardString.append(U"}");
    block.Source = reader.ReadUntil(guardString);
    block.Source.remove_suffix(guardString.size());
    return block;
}

RawBlockWithMeta RawBlockParser::GetNextRawBlock()
{
    using common::parser::detail::TokenMatcherHelper;
    using common::parser::detail::EmptyTokenArray;
       
    constexpr auto ExpectRawOrMeta = TokenMatcherHelper::GetMatcher(EmptyTokenArray{}, NailangToken::Raw, NailangToken::MetaFunc);

    constexpr auto MainLexer = ParserLexerBase<CommentTokenizer, tokenizer::MetaFuncPrefixTokenizer, tokenizer::BlockPrefixTokenizer>();

    std::vector<FuncCall> metaFuncs;
    while (true)
    {
        const auto token = ExpectNextToken(MainLexer, IgnoreBlank, IgnoreCommentToken, ExpectRawOrMeta);
        switch (token.GetIDEnum<NailangToken>())
        {
        case NailangToken::Raw:
        {
            RawBlockWithMeta block;
            static_cast<RawBlock&>(block) = FillRawBlock(token.GetString());
            block.MetaFunctions = MemPool.CreateArray(metaFuncs);
            return block;
        } break;
        case NailangToken::MetaFunc:
            metaFuncs.emplace_back(ComplexArgParser::ParseFuncBody(token.GetString(), MemPool, Context, FuncName::FuncInfo::Meta));
            break;
        default:
            Expects(false);
            break;
        }
    }
}

std::vector<RawBlockWithMeta> RawBlockParser::GetAllRawBlocks()
{
    std::vector<RawBlockWithMeta> sectors;
    while (true)
    {
        ContextReader reader(Context);
        reader.ReadWhile(IgnoreBlank);
        if (reader.PeekNext() == common::parser::special::CharEnd)
            break;
        sectors.emplace_back(GetNextRawBlock());
    }
    return sectors;
}


Assignment BlockParser::ParseAssignment(const std::u32string_view var)
{
    using common::parser::detail::TokenMatcherHelper;
    using common::parser::detail::EmptyTokenArray;
    using tokenizer::AssignOps;

    constexpr auto FirstLexer  = ParserLexerBase<CommentTokenizer, 
        tokenizer::SubFieldTokenizer, tokenizer::SquareBracketTokenizer, tokenizer::AssignOpTokenizer>();

    std::vector<RawArg> tmpQueries;
    Assignment assign(var);
    assign.Position = GetPosition(Context);

    for (bool shouldCont = true; shouldCont;)
    {
        auto token = GetNextToken(FirstLexer, IgnoreBlank, IgnoreCommentToken);
        switch (token.GetIDEnum<NailangToken>())
        {
        case NailangToken::SquareBracket: // Indexer
        {
            if (token.GetChar() == U']')
                OnUnExpectedToken(token, u"Unexpected right square bracket"sv);
            const auto index = ComplexArgParser::ParseSingleArg("]"sv, MemPool, Context);
            if (!index.has_value())
                OnUnExpectedToken(token, u"lack of index"sv);
            SubQuery::PushQuery(tmpQueries, *index);
        } break;
        case NailangToken::SubField: // SubField
        {
            SubQuery::PushQuery(tmpQueries, token.GetString());
        } break;
        case NailangToken::Assign:
        {
            bool reqNotNull = false;
            bool reqNull    = false;
            std::optional<EmbedOps> selfOp;
            switch (static_cast<AssignOps>(token.GetUInt()))
            {
            case AssignOps::   Assign:                                            break;
            case AssignOps::NilAssign: reqNotNull = false; reqNull = true;        break;
            case AssignOps::AndAssign: reqNotNull = true; selfOp = EmbedOps::And; break;
            case AssignOps:: OrAssign: reqNotNull = true; selfOp = EmbedOps:: Or; break;
            case AssignOps::AddAssign: reqNotNull = true; selfOp = EmbedOps::Add; break;
            case AssignOps::SubAssign: reqNotNull = true; selfOp = EmbedOps::Sub; break;
            case AssignOps::MulAssign: reqNotNull = true; selfOp = EmbedOps::Mul; break;
            case AssignOps::DivAssign: reqNotNull = true; selfOp = EmbedOps::Div; break;
            case AssignOps::RemAssign: reqNotNull = true; selfOp = EmbedOps::Rem; break;
            default: OnUnExpectedToken(token, u"expect assign op"sv); break;
            }
            reqNotNull |= !tmpQueries.empty();
            if (reqNotNull && reqNull)
                OnUnExpectedToken(token, u"NilAssign cannot be applied on query"sv);
            const auto stmt = ComplexArgParser::ParseSingleStatement(MemPool, Context);
            if (!stmt.has_value())
                NLPS_THROW_EX(u"expect statement"sv);
            if (reqNotNull)   assign.CheckNil = NilCheck::ReqNotNull;
            else if (reqNull) assign.CheckNil = NilCheck::ReqNull;
            if (!selfOp.has_value())
            {
                assign.Statement = *stmt;
            }
            else
            {
                BinaryExpr statement(*selfOp, MemPool.Create<LateBindVar>(assign.Target), *stmt);
                assign.Statement = MemPool.Create<BinaryExpr>(statement);
            }
            assign.Queries.Queries = MemPool.CreateArray(tmpQueries);
            tmpQueries.clear();
            shouldCont = false;
        } break;
        default:
            OnUnExpectedToken(token, u"expect [assign op] or [indexer] or [subfield]"sv);
            break;
        }
    }

    return assign;
}

void BlockParser::ParseContentIntoBlock(const bool allowNonBlock, Block& block, const bool tillTheEnd)
{
    using common::parser::detail::TokenMatcherHelper;
    using common::parser::detail::EmptyTokenArray;
    using tokenizer::AssignOps;

    constexpr auto MainLexer = ParserLexerBase<CommentTokenizer, 
        tokenizer::MetaFuncPrefixTokenizer, tokenizer::BlockPrefixTokenizer, tokenizer::NormalFuncPrefixTokenizer, 
        tokenizer::VariableTokenizer, tokenizer::CurlyBraceTokenizer>();

    std::vector<BlockContentItem> contents;
    std::vector<FuncCall> allMetaFuncs;
    std::vector<FuncCall> metaFuncs;
    const auto AppendMetaFuncs = [&]() 
    {
        const auto offset = gsl::narrow_cast<uint32_t>(allMetaFuncs.size());
        const auto count  = gsl::narrow_cast<uint32_t>(   metaFuncs.size());
        allMetaFuncs.reserve(static_cast<size_t>(offset) + count);
        std::move(metaFuncs.begin(), metaFuncs.end(), std::back_inserter(allMetaFuncs));
        metaFuncs.clear();
        return std::pair{ offset, count };
    };
    while (true)
    {
        const auto token = GetNextToken(MainLexer, IgnoreBlank, IgnoreCommentToken);
        switch (token.GetIDEnum<NailangToken>())
        {
        case NailangToken::MetaFunc:
        {
            metaFuncs.emplace_back(ComplexArgParser::ParseFuncBody(token.GetString(), MemPool, Context, FuncName::FuncInfo::Meta));
        } continue;
        case NailangToken::Raw:
        {
            const auto target = MemPool.Create<RawBlock>(FillRawBlock(token.GetString()));
            const auto [offset, count] = AppendMetaFuncs();
            contents.push_back(BlockContentItem::Generate(target, offset, count));
            metaFuncs.clear();
        } continue;
        case NailangToken::Block:
        {
            Block inlineBlk;
            inlineBlk.Position = GetPosition(Context, true);
            FillBlockName(inlineBlk);
            inlineBlk.Type = token.GetString();
            EatLeftCurlyBrace();
            {
                ContextReader reader(Context);
                reader.ReadLine();
            }
            FillFileName(inlineBlk);
            {
                const auto idxBegin = Context.Index;
                ParseContentIntoBlock(true, inlineBlk, false);
                const auto idxEnd = Context.Index;
                inlineBlk.Source = Context.Source.substr(idxBegin, idxEnd - idxBegin - 1);
            }
            const auto target = MemPool.Create<Block>(inlineBlk);
            const auto [offset, count] = AppendMetaFuncs();
            contents.push_back(BlockContentItem::Generate(target, offset, count));
            metaFuncs.clear();
        } continue;
        case NailangToken::Func:
        {
            if (!allowNonBlock)
                OnUnExpectedToken(token, u"Function call not supported here"sv);
            FuncCall funccall;
            static_cast<FuncCall&>(funccall) = ComplexArgParser::ParseFuncBody(token.GetString(), MemPool, Context);
            EatSemiColon();
            const auto target = MemPool.Create<FuncCall>(funccall);
            const auto [offset, count] = AppendMetaFuncs();
            contents.push_back(BlockContentItem::Generate(target, offset, count));
            metaFuncs.clear();
        } continue;
        case NailangToken::Var:
        {
            if (!allowNonBlock)
                OnUnExpectedToken(token, u"Variable assignment not supported here"sv);
            Assignment assign = ParseAssignment(token.GetString());
            const auto target = MemPool.Create<Assignment>(assign);
            const auto [offset, count] = AppendMetaFuncs();
            contents.push_back(BlockContentItem::Generate(target, offset, count));
            metaFuncs.clear();
        } continue;
        case NailangToken::CurlyBrace:
        {
            if (tillTheEnd || token.GetChar() != U'}')
                OnUnExpectedToken(token, u"when parsing block contents"sv);
            if (metaFuncs.size() > 0)
                OnUnExpectedToken(token, u"expect block/assignment/funccall after metafuncs"sv);
        } break;
        default:
        {
            if (token.GetIDEnum() != BaseToken::End)
                OnUnExpectedToken(token, u"when parsing block contents"sv);
            if (metaFuncs.size() > 0)
                OnUnExpectedToken(token, u"expect block/assignment/funccall after metafuncs"sv);
            if (!tillTheEnd)
                OnUnExpectedToken(token, u"expect '}' to close the scope"sv);
        } break;
        }
        break;
    }
    block.Content = MemPool.CreateArray(contents);
    block.MetaFuncations = MemPool.CreateArray(allMetaFuncs);
}

Block BlockParser::ParseRawBlock(const RawBlock& block, MemoryPool& pool)
{
    common::parser::ParserContext context(block.Source, block.FileName);
    std::tie(context.Row, context.Col) = block.Position;
    BlockParser parser(pool, context);

    Block ret;
    static_cast<RawBlock&>(ret) = block;
    parser.ParseContentIntoBlock(true, ret);
    return ret;
}

Block BlockParser::ParseAllAsBlock(MemoryPool& pool, common::parser::ParserContext& context)
{
    BlockParser parser(pool, context);

    Block ret;
    ret.Position = GetPosition(context, true);
    parser.FillFileName(ret);
    parser.ParseContentIntoBlock(false, ret);
    return ret;
}


std::u32string_view ReplaceEngine::TrimStrBlank(const std::u32string_view str) noexcept
{
    size_t len = str.size();
    for (const auto ch : boost::adaptors::reverse(str))
    {
        if (!IgnoreBlank(ch))
            break;
        len--;
    }
    return str.substr(0, len);
}

void ReplaceEngine::HandleException(const NailangParseException& ex) const
{
    /*ex.File = GetCurrentFileName().StrView();
    ex.Position = GetCurrentPosition();*/
    ex.ThrowSelf();
}

void ReplaceEngine::OnReplaceOptBlock(std::u32string&, void*, std::u32string_view, std::u32string_view)
{
    NLPS_THROW_EX(u"ReplaceOptBlock unimplemented"sv);
}
void ReplaceEngine::OnReplaceVariable(std::u32string&, void*, std::u32string_view)
{
    NLPS_THROW_EX(u"ReplaceVariable unimplemented"sv);
}
void ReplaceEngine::OnReplaceFunction(std::u32string&, void*, std::u32string_view, common::span<const std::u32string_view>)
{
    NLPS_THROW_EX(u"ReplaceFunction unimplemented"sv);
}

std::u32string ReplaceEngine::ProcessOptBlock(const std::u32string_view source, const std::u32string_view prefix, const std::u32string_view suffix, void* cookie)
{
    Expects(!prefix.empty() && !suffix.empty()); // Illegal prefix/suffix
    common::parser::ParserContext context(source);
    ContextReader reader(context);
    std::u32string output;
    while (true)
    {
        auto before = reader.ReadUntil(prefix);
        if (before.empty()) // reaching end
        {
            output.append(reader.ReadAll());
            break;
        }
        {
            before.remove_suffix(prefix.size());
            output.append(before);
        }
        reader.ReadWhile(IgnoreBlank);
        if (reader.ReadNext() != U'{')
            NLPS_THROW_EX(u"Expect '{' for cond"sv);
        reader.ReadWhile(IgnoreBlank);
        auto cond = reader.ReadUntil(U"}");
        cond.remove_suffix(1);
        reader.ReadLine();
        auto str = reader.ReadUntil(suffix);
        if (str.empty())
        {
            if (reader.IsEnd())
                NLPS_THROW_EX(u"End before block content"sv);
            else
                NLPS_THROW_EX(u"No suffix found!"sv);
        }
        str.remove_suffix(suffix.size());
        // find a opt block replacement
        OnReplaceOptBlock(output, cookie, TrimStrBlank(cond), str);
        reader.ReadLine();
    }
    return output;
}

std::u32string ReplaceEngine::ProcessVariable(const std::u32string_view source, const std::u32string_view prefix, const std::u32string_view suffix, void* cookie)
{
    Expects(!prefix.empty() && !suffix.empty()); // Illegal prefix/suffix
    common::parser::ParserContext context(source);
    ContextReader reader(context);
    std::u32string output;
    while (true)
    {
        auto before = reader.ReadUntil(prefix);
        if (before.empty()) // reaching end
        {
            output.append(reader.ReadAll());
            break;
        }
        {
            before.remove_suffix(prefix.size());
            output.append(before);
        }
        reader.ReadWhile(IgnoreBlank);
        auto var = reader.ReadUntil(suffix);
        if (var.empty())
        {
            if (reader.IsEnd())
                NLPS_THROW_EX(u"End before variable name"sv);
            else
                NLPS_THROW_EX(u"No suffix found!"sv);
        }
        var.remove_suffix(suffix.size());
        // find a variable replacement
        OnReplaceVariable(output, cookie, TrimStrBlank(var));
    }
    return output;
}

std::u32string ReplaceEngine::ProcessFunction(const std::u32string_view source, const std::u32string_view prefix, const std::u32string_view suffix, void* cookie)
{
    Expects(!prefix.empty()); // Illegal suffix
    common::parser::ParserContext context(source);
    ContextReader reader(context);
    std::u32string output;
    while (true)
    {
        auto before = reader.ReadUntil(prefix);
        if (before.empty()) // reaching end
        {
            output.append(reader.ReadAll());
            break;
        }
        {
            before.remove_suffix(prefix.size());
            output.append(before);
        }
        reader.ReadWhile(IgnoreBlank);
        auto funcName = reader.ReadUntil(U"("sv);
        if (funcName.empty())
        {
            if (reader.IsEnd())
                NLPS_THROW_EX(u"End before func name"sv);
            else
                NLPS_THROW_EX(u"No '(' found!"sv);
        }
        funcName.remove_suffix(1);

        std::vector<std::u32string_view> args;
        enum class States : uint32_t { Init, Pending, InComma, InSlash, End };
        States state = States::Init;
        uint32_t nestedLevel = 0;
        const auto PushArg = [&]() 
        {
            Expects(nestedLevel == 0);
            auto part = reader.CommitRead();
            part.remove_suffix(1);
            args.push_back(TrimStrBlank(part));
        };
        reader.ReadWhile(IgnoreBlank);
        while (state != States::End)
        {
            const auto ch = reader.PeekNext();
            if (ch == common::parser::special::CharEnd)
                break;
            reader.MoveNext();
            switch (state)
            {
            case States::Init:
            case States::Pending:
                switch (ch)
                {
                case U'"':
                    state = States::InComma;
                    break;
                case U'(':
                    state = States::Pending;
                    nestedLevel++;
                    break;
                case U')':
                    if (nestedLevel)
                    {
                        state = States::Pending;
                        nestedLevel--;
                    }
                    else
                    {
                        if (state == States::Init)
                        {
                            if (args.size() > 0)
                                NLPS_THROW_EX(u"empty arg not allowed"sv);
                            reader.CommitRead();
                        }
                        else
                            PushArg();
                        state = States::End;
                    }
                    break;
                case U',':
                    if (state == States::Init)
                        NLPS_THROW_EX(u"empty arg not allowed"sv);
                    else if (nestedLevel == 0)
                    {
                        PushArg();
                        state = States::Init;
                        reader.ReadWhile(IgnoreBlank);
                    }
                    break;
                default:
                    state = States::Pending;
                    break;
                }
                break;
            case States::InComma:
                if (ch == U'"')
                    state = States::Pending;
                else if (ch == U'\\')
                    state = States::InSlash;
                break;
            case States::InSlash:
                state = States::InComma;
                break;
            case States::End:
                Expects(false);
                break;
            }
        }
        if (state != States::End)
            NLPS_THROW_EX(u"End before arg list finishes"sv);
        if (!suffix.empty())
        {
            reader.ReadWhile(IgnoreBlank);
            if (!reader.ReadMatch(suffix))
            {
                if (reader.IsEnd())
                    NLPS_THROW_EX(u"End before suffix"sv);
                else
                    NLPS_THROW_EX(u"Unexpeccted char before suffix!"sv);
            }
        }
        // find a function replacement
        OnReplaceFunction(output, cookie, TrimStrBlank(funcName), args);
    }
    return output;
}

ReplaceEngine::~ReplaceEngine()
{
}

}
