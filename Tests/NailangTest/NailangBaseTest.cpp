#include "rely.h"
#include "ParserCommon.h"
#include "Nailang/NailangParserRely.h"
#include "Nailang/NailangParser.h"
#include "SystemCommon/StringFormat.h"
#include <algorithm>


using namespace std::string_view_literals;
using common::parser::ParserContext;
using common::parser::ContextReader;



#define CHECK_TK(token, etype, type, action, val) do { EXPECT_EQ(token.GetIDEnum<etype>(), etype::type); EXPECT_EQ(token.action(), val); } while(0)
#define CHECK_BASE_TK(token, type, action, val) CHECK_TK(token, common::parser::BaseToken, type, action, val)


struct MemPool_ : public xziar::nailang::MemoryPool
{
    using MemoryPool::Trunks;
    using MemoryPool::DefaultTrunkLength;
};

static std::string PrintMemPool(const xziar::nailang::MemoryPool& pool_) noexcept
{
    const auto& pool = static_cast<const MemPool_&>(pool_);
    const auto [used, total] = pool.Usage();
    std::string txt = fmt::format("MemoryPool[{} trunks(default {} bytes)]: [{}/{}]\n", 
        pool.Trunks.size(), pool.DefaultTrunkLength, used, total);
    auto ins = std::back_inserter(txt);
    size_t i = 0;
    for (const auto& [ptr, offset, avaliable] : pool.Trunks)
    {
        fmt::format_to(ins, "[{}] ptr[{}], offset[{}], avaliable[{}]\n", i++, (void*)ptr, offset, avaliable);
    }
    return txt;
}

TEST(NailangBase, MemoryPool)
{
#define EXPECT_ALIGN(ptr, align) EXPECT_EQ(reinterpret_cast<uintptr_t>(ptr) % align, 0u)
    xziar::nailang::MemoryPool pool;
    {
        const auto space = pool.Alloc(10, 1);
        std::generate(space.begin(), space.end(), [i = 0]() mutable { return std::byte(i++); });
        {
            int i = 0;
            for (const auto dat : space)
                EXPECT_EQ(dat, std::byte(i++));
        }
    }
    {
        const auto space = pool.Alloc(4, 128);
        EXPECT_ALIGN(space.data(), 128) << PrintMemPool(pool);
    }
    {
        const auto& arr = *pool.Create<std::array<double, 3>>(std::array{ 1.0,2.0,3.0 });
        EXPECT_ALIGN(&arr, alignof(double)) << PrintMemPool(pool);
        EXPECT_THAT(arr, testing::ElementsAre(1.0, 2.0, 3.0)) << PrintMemPool(pool);
    }
    {
        const std::array arr{ 1.0,2.0,3.0 };
        const auto sp = pool.CreateArray(arr);
        EXPECT_ALIGN(sp.data(), alignof(double)) << PrintMemPool(pool);
        std::vector<double> vec(sp.begin(), sp.end());
        EXPECT_THAT(vec, testing::ElementsAre(1.0, 2.0, 3.0)) << PrintMemPool(pool);
    }
    {
        const auto space = pool.Alloc(3 * 1024 * 1024, 4096);
        EXPECT_ALIGN(space.data(), 4096) << PrintMemPool(pool);
        const auto space2 = pool.Alloc(128, 1);
        EXPECT_EQ(space2.data() - space.data(), 3 * 1024 * 1024) << PrintMemPool(pool);
    }
    {
        const auto space = pool.Alloc(36 * 1024 * 1024, 8192);
        EXPECT_ALIGN(space.data(), 4096) << PrintMemPool(pool);
    }
#undef EXPECT_ALIGN
}

TEST(NailangBase, OpSymbolTokenizer)
{
    using common::parser::BaseToken;
    using xziar::nailang::EmbedOps;
    using xziar::nailang::ExtraOps;
    using xziar::nailang::AssignOps;
    constexpr auto ParseAll = [](const std::u32string_view src)
    {
        return TKParse<xziar::nailang::tokenizer::OpSymbolTokenizer, common::parser::tokenizer::IntTokenizer>(src);
    };
#define CHECK_BASE_INT(token, val) CHECK_BASE_TK(token, Int, GetInt, val)
#define CHECK_EMBED_OP(token, type) CHECK_TK(token, xziar::nailang::tokenizer::NailangToken, OpSymbol, GetInt, common::enum_cast(xziar::nailang::type))

#define CHECK_BIN_OP(src, type) do          \
    {                                       \
        const auto tokens = ParseAll(src);  \
        CHECK_BASE_INT(tokens[0], 1);       \
        CHECK_EMBED_OP(tokens[1], type);    \
        CHECK_BASE_INT(tokens[2], 2);       \
    } while(0)
#define CHECK_UN_OP(src, type) do           \
    {                                       \
        const auto tokens = ParseAll(src);  \
        CHECK_EMBED_OP(tokens[0], type);    \
        CHECK_BASE_INT(tokens[1], 1);       \
    } while(0)

    CHECK_BIN_OP(U"1==2"sv,   EmbedOps::Equal);
    CHECK_BIN_OP(U"1 == 2"sv, EmbedOps::Equal);
    CHECK_BIN_OP(U"1 != 2"sv, EmbedOps::NotEqual);
    CHECK_BIN_OP(U"1 < 2"sv,  EmbedOps::Less);
    CHECK_BIN_OP(U"1 <= 2"sv, EmbedOps::LessEqual);
    CHECK_BIN_OP(U"1 > 2"sv,  EmbedOps::Greater);
    CHECK_BIN_OP(U"1 >= 2"sv, EmbedOps::GreaterEqual);
    CHECK_BIN_OP(U"1 && 2"sv, EmbedOps::And);
    CHECK_BIN_OP(U"1 || 2"sv, EmbedOps::Or);
    CHECK_BIN_OP(U"1 + 2"sv,  EmbedOps::Add);
    CHECK_BIN_OP(U"1 - 2"sv,  EmbedOps::Sub);
    CHECK_BIN_OP(U"1 * 2"sv,  EmbedOps::Mul);
    CHECK_BIN_OP(U"1 / 2"sv,  EmbedOps::Div);
    CHECK_BIN_OP(U"1 % 2"sv,  EmbedOps::Rem);
    CHECK_BIN_OP(U"1 & 2"sv,  EmbedOps::BitAnd);
    CHECK_BIN_OP(U"1 | 2"sv,  EmbedOps::BitOr);
    CHECK_BIN_OP(U"1 ^ 2"sv,  EmbedOps::BitXor);
    CHECK_BIN_OP(U"1 << 2"sv, EmbedOps::BitShiftLeft);
    CHECK_BIN_OP(U"1 >> 2"sv, EmbedOps::BitShiftRight);
    CHECK_BIN_OP(U"1 = 2"sv,  AssignOps::Assign);
    CHECK_BIN_OP(U"1 := 2"sv, AssignOps::NewCreate);
    CHECK_BIN_OP(U"1 ?= 2"sv, AssignOps::NilAssign);
    CHECK_BIN_OP(U"1 += 2"sv, AssignOps::AddAssign);
    CHECK_BIN_OP(U"1 -= 2"sv, AssignOps::SubAssign);
    CHECK_BIN_OP(U"1 *= 2"sv, AssignOps::MulAssign);
    CHECK_BIN_OP(U"1 /= 2"sv, AssignOps::DivAssign);
    CHECK_BIN_OP(U"1 %= 2"sv, AssignOps::RemAssign);
    CHECK_BIN_OP(U"1 &= 2"sv, AssignOps::BitAndAssign);
    CHECK_BIN_OP(U"1 |= 2"sv, AssignOps::BitOrAssign);
    CHECK_BIN_OP(U"1 ^= 2"sv, AssignOps::BitXorAssign);
    CHECK_BIN_OP(U"1 <<= 2"sv, AssignOps::BitShiftLeftAssign);
    CHECK_BIN_OP(U"1 >>= 2"sv, AssignOps::BitShiftRightAssign);
    CHECK_UN_OP(U"?1"sv, ExtraOps::Quest);
    CHECK_UN_OP(U"!1"sv, EmbedOps::Not);
    CHECK_UN_OP(U"~1"sv, EmbedOps::BitNot);
    {
        const auto tokens = ParseAll(U"1? 2 : 3"sv);
        CHECK_BASE_INT(tokens[0], 1);
        CHECK_EMBED_OP(tokens[1], ExtraOps::Quest);
        CHECK_BASE_INT(tokens[2], 2);
        CHECK_EMBED_OP(tokens[3], ExtraOps::Colon);
        CHECK_BASE_INT(tokens[4], 3);
    }
#undef CHECK_BIN_OP
#undef CHECK_EMBED_OP
#undef CHECK_BASE_UINT
}


void DoThrow()
{
    throw xziar::nailang::NailangPartedNameException(u"a", U"b", U"c");
}

TEST(NailangBase, FuncName)
{
    {
        EXPECT_THROW(DoThrow(), xziar::nailang::NailangPartedNameException);

        try
        {
            xziar::nailang::DoThrow();
        }
        catch (const xziar::nailang::NailangPartedNameException&)
        {
            TestCout() << "recieve PatedNameEx\n";
        }
        catch (const common::BaseException& e1)
        {
            TestCout() << "recieve BaseEx, " << (dynamic_cast<const xziar::nailang::NailangPartedNameException*>(&e1) ? "Y"sv : "N"sv) << "\n";
        }
        catch (const std::runtime_error& e2)
        {
            const auto& tiE = typeid(e2);
            const auto& tiT = typeid(xziar::nailang::NailangPartedNameException);
            TestCout() << "recieve stdre, " << (dynamic_cast<const xziar::nailang::NailangPartedNameException*>(&e2) ? "Y"sv : "N"sv)
                << ", [" << tiT.name() << " -vs- " << tiE.name() << "]\n";
        }
        catch (const std::exception& e3)
        {
            TestCout() << "recieve stdEx, " << (dynamic_cast<const xziar::nailang::NailangPartedNameException*>(&e3) ? "Y"sv : "N"sv) << "\n";
        }
        catch (...)
        {
            TestCout() << "recieve unknown \n";
        }

        EXPECT_THROW(xziar::nailang::DoThrow(), xziar::nailang::NailangPartedNameException);
    }
    using xziar::nailang::FuncName;
    using xziar::nailang::TempFuncName;
    using VI = xziar::nailang::FuncName::FuncInfo;
    xziar::nailang::MemoryPool Pool;
    {
        constexpr auto name = U"a.b.c"sv;
        const auto func = FuncName::Create(Pool, name, FuncName::FuncInfo::Empty);
        EXPECT_EQ(*func, name);
        EXPECT_EQ(func->Info(), VI::Empty);
        EXPECT_THAT(func->Parts(), testing::ElementsAre(U"a"sv, U"b"sv, U"c"sv));
        EXPECT_EQ(func->GetRest(0), name);
        EXPECT_EQ(func->GetRest(1), name.substr(2));
    }
    std::vector<TempFuncName> funcs;
    {
        ASSERT_EQ(funcs.size(), 0u);
        constexpr auto name = U"a.b.c"sv;
        auto func_ = FuncName::CreateTemp(name, FuncName::FuncInfo::Empty);
        {
            const FuncName& func = func_;
            EXPECT_EQ(reinterpret_cast<const void*>(&func), reinterpret_cast<const void*>(&func_));
            EXPECT_EQ(func, name);
        }
        funcs.push_back(std::move(func_));
        {
            const FuncName& func = func_;
            EXPECT_EQ(&func, nullptr);
        }
        {
            const FuncName& func = funcs.back();
            EXPECT_EQ(func.Info(), VI::Empty);
            EXPECT_THAT(func.Parts(), testing::ElementsAre(U"a"sv, U"b"sv, U"c"sv));
            EXPECT_EQ(func.GetRest(0), name);
            EXPECT_EQ(func.GetRest(1), name.substr(2));
        }
    }
    {
        constexpr auto name = U"a..c"sv;
        EXPECT_THROW(FuncName::Create(Pool, name, FuncName::FuncInfo::Empty), xziar::nailang::NailangPartedNameException);
    }
}


TEST(NailangBase, Serializer)
{
    using xziar::nailang::Serializer;
    using xziar::nailang::Expr;
    using xziar::nailang::LateBindVar;
    using xziar::nailang::EmbedOps;
    using xziar::nailang::FuncName;
    using xziar::nailang::UnaryExpr;
    using xziar::nailang::QueryExpr;
    Expr a1{ true };
    Expr a2{ false };
    Expr a3{ uint64_t(1234) };
    Expr a4{ int64_t(-5678) };
    Expr a5{ U"10ab"sv };
    Expr a6{ LateBindVar(U"`cdef"sv) };
    EXPECT_EQ(Serializer::Stringify(a1), U"true"sv);
    EXPECT_EQ(Serializer::Stringify(a2), U"false"sv);
    EXPECT_EQ(Serializer::Stringify(a3), U"1234"sv);
    EXPECT_EQ(Serializer::Stringify(a4), U"-5678"sv);
    EXPECT_EQ(Serializer::Stringify(a5), U"\"10ab\""sv);
    EXPECT_EQ(Serializer::Stringify(a6), U"`cdef"sv);
    {
        UnaryExpr expr(EmbedOps::Not, a1);
        EXPECT_EQ(Serializer::Stringify(&expr), U"!true"sv);
    }
    {
        UnaryExpr expr(EmbedOps::CheckExist, a6);
        EXPECT_EQ(Serializer::Stringify(&expr), U"?`cdef"sv);
    }
    {
        QueryExpr expr(a5, { &a3,1 }, QueryExpr::QueryType::Index);
        EXPECT_EQ(Serializer::Stringify(&expr), U"\"10ab\"[1234]"sv);
    }
    {
        QueryExpr expr(a6, { &a4,1 }, QueryExpr::QueryType::Index);
        EXPECT_EQ(Serializer::Stringify(&expr), U"`cdef[-5678]"sv);
    }
    {
        Expr tmp(U"xyzw"sv);
        QueryExpr expr(a6, { &tmp,1 }, QueryExpr::QueryType::Sub);
        EXPECT_EQ(Serializer::Stringify(&expr), U"`cdef.xyzw"sv);
    }
    {
        Expr tmp[2] = { U"xyzw"sv, U"abc"sv };
        QueryExpr expr(a6, tmp, QueryExpr::QueryType::Sub);
        EXPECT_EQ(Serializer::Stringify(&expr), U"`cdef.xyzw.abc"sv);
    }
    {
        Expr tmp1(U"xyzw"sv);
        Expr tmp2(U"abcd"sv);
        QueryExpr expr1(a6, { &tmp1,1 }, QueryExpr::QueryType::Sub);
        QueryExpr expr2(&expr1, { &a3,1 }, QueryExpr::QueryType::Index);
        QueryExpr expr3(&expr2, { &tmp2,1 }, QueryExpr::QueryType::Sub);
        EXPECT_EQ(Serializer::Stringify(&expr3), U"`cdef.xyzw[1234].abcd"sv);
    }
    {
        xziar::nailang::BinaryExpr expr(EmbedOps::Add, a1, a2);
        EXPECT_EQ(Serializer::Stringify(&expr), U"true + false"sv);
    }
    {
        xziar::nailang::BinaryExpr expr(EmbedOps::NotEqual, a2, a3);
        EXPECT_EQ(Serializer::Stringify(&expr), U"false != 1234"sv);
    }
    {
        xziar::nailang::TernaryExpr expr(a1, a2, a3);
        EXPECT_EQ(Serializer::Stringify(&expr), U"true ? false : 1234"sv);
    }
    {
        xziar::nailang::BinaryExpr expr0(EmbedOps::Equal, a1, a2);
        xziar::nailang::UnaryExpr expr(EmbedOps::Not, &expr0);
        EXPECT_EQ(Serializer::Stringify(&expr), U"!(true == false)"sv);
    }
    {
        xziar::nailang::BinaryExpr expr0(EmbedOps::Or, a1, a2);
        xziar::nailang::BinaryExpr expr(EmbedOps::And, &expr0, a3);
        EXPECT_EQ(Serializer::Stringify(&expr), U"(true || false) && 1234"sv);
    }
    {
        xziar::nailang::TernaryExpr expr0(a1, a2, a3);
        xziar::nailang::BinaryExpr expr(EmbedOps::And, &expr0, a3);
        EXPECT_EQ(Serializer::Stringify(&expr), U"(true ? false : 1234) && 1234"sv);
    }
    {
        xziar::nailang::BinaryExpr expr0(EmbedOps::Equal, a1, a2);
        xziar::nailang::BinaryExpr expr1(EmbedOps::NotEqual, a2, a3);
        xziar::nailang::TernaryExpr expr2(&expr0, &expr1, a3);
        xziar::nailang::BinaryExpr expr(EmbedOps::ValueOr, a6, &expr2);
        EXPECT_EQ(Serializer::Stringify(&expr), U"`cdef ?? ((true == false) ? (false != 1234) : 1234)"sv);
    }
    {
        xziar::nailang::BinaryExpr expr0(EmbedOps::BitShiftRight, a3, a4);
        xziar::nailang::BinaryExpr expr1(EmbedOps::BitXor, a3, a4);
        xziar::nailang::BinaryExpr expr2(EmbedOps::BitAnd, a3, a4);
        xziar::nailang::TernaryExpr expr3(&expr0, &expr1, &expr2);
        xziar::nailang::BinaryExpr expr(EmbedOps::BitShiftLeft, a6, &expr3);
        EXPECT_EQ(Serializer::Stringify(&expr), U"`cdef << ((1234 >> -5678) ? (1234 ^ -5678) : (1234 & -5678))"sv);
    }
    {
        const auto name = FuncName::CreateTemp(U"Func"sv, FuncName::FuncInfo::Empty);
        std::vector<Expr> args{ a1 };
        xziar::nailang::FuncCall call{ &name.Get(), args };
        EXPECT_EQ(Serializer::Stringify(&call), U"Func(true)"sv);
    }
    {
        const auto name = FuncName::CreateTemp(U"Func2"sv, FuncName::FuncInfo::Empty);
        std::vector<Expr> args{ a2,a3,a4 };
        xziar::nailang::FuncCall call{ &name.Get(), args };
        EXPECT_EQ(Serializer::Stringify(&call), U"Func2(false, 1234, -5678)"sv);
    }
    {
        const auto name = FuncName::CreateTemp(U"Func3"sv, FuncName::FuncInfo::Empty);
        xziar::nailang::BinaryExpr expr0(EmbedOps::Or, a1, a2);
        xziar::nailang::BinaryExpr expr(EmbedOps::And, &expr0, a3);
        std::vector<Expr> args{ &expr,a4 };
        xziar::nailang::FuncCall call{ &name.Get(), args };
        EXPECT_EQ(Serializer::Stringify(&call), U"Func3((true || false) && 1234, -5678)"sv);
    }
    {
        const auto name = FuncName::CreateTemp(U"Func4"sv, FuncName::FuncInfo::Empty);
        xziar::nailang::BinaryExpr expr0(EmbedOps::Or, a1, a2);
        xziar::nailang::BinaryExpr expr1(EmbedOps::And, &expr0, a3);
        std::vector<Expr> args{ &expr1,a4 };
        xziar::nailang::FuncCall call{ &name.Get(), args };
        xziar::nailang::BinaryExpr expr(EmbedOps::Div, a5, &call);
        EXPECT_EQ(Serializer::Stringify(&expr), U"\"10ab\" / Func4((true || false) && 1234, -5678)"sv);
    }
}