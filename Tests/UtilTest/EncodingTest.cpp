﻿#include "TestRely.h"
#include "SystemCommon/FileEx.h"
#include "common/Linq2.hpp"
#include "common/TimeUtil.hpp"
#include "SystemCommon/StringConvert.h"
#include "SystemCommon/StringDetect.h"

using namespace common::mlog;
using namespace common;
using std::vector;
using std::byte;
using str::Encoding;

static MiniLogger<false>& log()
{
    static MiniLogger<false> log(u"EncodingTest", { GetConsoleBackend() });
    return log;
}


#pragma warning(disable:4996)

struct gb18030_utf16_cvter : public std::codecvt_byname<wchar_t, char, std::mbstate_t>
{
    gb18030_utf16_cvter() : std::codecvt_byname<wchar_t, char, std::mbstate_t>("zh_CN.GB18030"/*"54936"*/) {}
    ~gb18030_utf16_cvter() {}
};

static void TestStrConv()
{
    const fs::path basePath = FindPath() / u"Tests" / u"Data";

    const auto ret = str::to_u16string(std::u32string(U"hh"));
    std::u16string utf16(u"𤭢");
    std::string utf8 = str::to_string(utf16, Encoding::UTF8);
    vector<uint8_t> tmp(utf8.cbegin(), utf8.cend());
    for (size_t i = 0; i < tmp.size(); ++i)
        log().debug(u"byte {} : {:#x}\n", i, tmp[i]);

    tmp.assign({ 0x31,0x30,0x68,0xe6,0x88,0x91 });
    utf8.assign(tmp.cbegin(), tmp.cend());
    utf16 = str::to_u16string(utf8, Encoding::UTF8);
    log().debug(u"output u16: {}\n", utf16);

    const auto utf16be = str::to_string(utf16, Encoding::UTF16BE, Encoding::UTF16LE);
    for (size_t i = 0; i < utf8.size(); ++i)
        log().debug(u"byte {} : LE {:#x}\tBE {:#x}\n", i, ((const uint8_t*)(utf16.data()))[i], (uint8_t)(utf16be[i]));

    std::string u8raw;
    file::ReadAll(basePath / u"utf8-sample.html", u8raw);
    const auto chtype = common::str::DetectEncoding(u8raw);
    utf16 = str::to_u16string(u8raw, chtype);
    //log().debug(u"csv-u16 txt:\n{}\n", utf16);
    vector<std::byte> csvdest(utf16.size() * 2 + 2, std::byte(0));
    csvdest[0] = byte(0xff), csvdest[1] = byte(0xfe);
    memcpy_s(&csvdest[2], csvdest.size() - 2, utf16.data(), utf16.size() * 2);
    file::WriteAll(basePath / u"utf8-sample-utf16.html", csvdest);

    const auto myget = str::to_string(utf16, Encoding::GB18030);
    file::WriteAll(basePath / u"utf8-sample-gb18030.html", myget);
    const auto myout = str::to_u8string(myget, Encoding::GB18030);
    file::WriteAll(basePath / u"utf8-sample-myout.html", myout);
    {
        const auto allMatch = common::linq::FromIterable(u8raw)
            .Pair(common::linq::FromIterable(myout))
            .AllIf([idx = 0](const auto& p) mutable
            {
                const uint8_t raw = p.first, my = p.second;
                const bool ret = raw == my;
                if (!ret)
                    log().debug(u"diff at byte {} : Raw {:#x}\tMy {:#x}\n", idx, (uint8_t)(raw), (uint8_t)(my));
                idx++;
                return ret;
            });
        log().log(allMatch ? LogLevel::Success : LogLevel::Error, u"Test gb18030->utf8 convert over!\n");
    }
    if (false) // will throw exception for unknown codepoint
    {
        std::wstring_convert<gb18030_utf16_cvter> gb18030_utf16_cvt(new gb18030_utf16_cvter());
        const auto refget = gb18030_utf16_cvt.to_bytes(*(const std::wstring*) &utf16);
        const auto allMatch = common::linq::FromIterable(refget)
            .Pair(common::linq::FromIterable(myget))
            .AllIf([idx = 0](const auto& p) mutable
            {
                auto [ref, my] = p;
                const bool ret = ref == my;
                if (!ret)
                    log().debug(u"diff at byte {} : Raw {:#x}\tMy {:#x}\n", idx, (uint8_t)(ref), (uint8_t)(my));
                idx++;
                return ret;
            });
        log().log(allMatch ? LogLevel::Success : LogLevel::Error, u"Test utf16->gb18030 convert over!\n");
    }

    getchar();
}
#pragma warning(default:4996)

const static uint32_t ID = RegistTest("EncodingTest", &TestStrConv);
