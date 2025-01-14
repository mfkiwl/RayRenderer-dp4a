#include "TestRely.h"
#include "SystemCommon/FileEx.h"
#include "common/TimeUtil.hpp"

using namespace common::mlog;
using namespace common;
using std::vector;


static void TestLog()
{
    static MiniLogger<false> conLog(u"LogTest", { GetConsoleBackend() });
    static MiniLogger<false> dbgLog(u"LogTest", { GetDebuggerBackend() });
    SimpleTimer timer;
    conLog.info(u"Plain call.\n");
    std::string name = "tst";
    timer.Start();
    for (uint32_t i = 0; i < 5000; ++i)
        conLog.verbose(FMT_STRING(u"Dummy Data Here {}.\n"), name);
    timer.Stop();
    conLog.success(u"Total {} us, each takes [{}] ns", timer.ElapseUs(), timer.ElapseNs() / 5000);
    dbgLog.success(u"Total {} us, each takes [{}] ns", timer.ElapseUs(), timer.ElapseNs() / 5000);
    getchar();
}


const static uint32_t ID = RegistTest("LogTest", &TestLog);
