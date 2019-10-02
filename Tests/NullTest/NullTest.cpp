#include "SystemCommon/ColorConsole.h"
#include "common/CommonRely.hpp"
#include "common/Linq.hpp"
#include "common/RefObject.hpp"
#define FMT_STRING_ALIAS 1
#include "fmt/utfext.h"
#include "boost.stacktrace/stacktrace.h"
#include <vector>
#include <cstdio>
using common::BaseException;
using common::console::ConsoleHelper;
using common::linq::Linq;


struct AData
{
    int pp;
    AData(int d) : pp(d) { }
    ~AData() { printf("deconst AData %d\n", pp); }
};

struct BData : public AData
{
    int pk;
    BData(int d) : AData(d), pk(d) { }
    ~BData() { printf("deconst BData %d\n", pk); }
};

struct A : public common::RefObject<AData>
{
    A(common::RefObject<AData>&& other) : common::RefObject<AData>(std::move(other))
    { }
    A(int data) : common::RefObject<AData>(Create(data)) 
    { }
    using common::RefObject<AData>::RefObject;
    int GetIt() const
    {
        return GetSelf().pp;
    }
    A Child(int data)
    {
        return CreateWith<AData>(data);
    }
};

struct B : public common::RefObject<BData>
{
    B(common::RefObject<BData>&& other) : common::RefObject<BData>(std::move(other))
    { }
    B(int data) : common::RefObject<BData>(Create(data))
    { }
    using common::RefObject<BData>::RefObject;
    int GetIt() const
    {
        return GetSelf().pk;
    }

};

void TestRefObj()
{
    A a(1);
    B b(2);
    auto c = a.Child(3);
    printf("init\n");
    printf("a: %d\n", a.GetIt());
    printf("b: %d\n", b.GetIt());
    printf("c: %d\n", c.GetIt());
    a = b;
    printf("a=b\n");
    printf("a: %d\n", a.GetIt());
    printf("b: %d\n", b.GetIt());
    printf("c: %d\n", c.GetIt());
    a = std::move(c);
    printf("a=move(c)\n");
    printf("a: %d\n", a.GetIt());
    printf("b: %d\n", b.GetIt());
    printf("c: %d\n", c.GetIt());
}

void TestLinq()
{
    std::vector<int> src{ 1,2,3,4,5,6,7 };
    auto lq = Linq::FromIterable(src);
    for (auto& item : lq)
    {
        item = item + 1;
    }
    auto lqw = Linq::FromIterable(src).ToAnyEnumerable();
    for (const auto& item : lqw)
    {
        printf("%d", item);
    }
}

void TestStacktrace()
{
    fmt::basic_memory_buffer<char16_t> out;
    ConsoleHelper console;
    try
    {
        COMMON_THROWEX(BaseException, u"hey");
    }
    catch (const BaseException&)
    {
        try
        {
            COMMON_THROWEX(BaseException, u"hey");
        }
        catch (const BaseException & be2)
        {
            for (const auto kk : be2.Stack())
            {
                out.clear();
                fmt::format_to(out, FMT_STRING(u"[{}]:[{:d}]\t{}\n"), kk.File, kk.Line, kk.Func);
                console.Print(std::u16string_view(out.data(), out.size()));
            }
        }
    }
}

int main()
{
    TestRefObj();
    printf("\n\n");

    TestLinq();
    printf("\n\n");

    TestStacktrace();
    printf("\n\n");

    getchar();
}

