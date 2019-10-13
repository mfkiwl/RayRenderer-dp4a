#include "oglPch.h"
#include "oglWorker.h"
#include "oglUtil.h"
#if defined(_WIN32)
#   include "glew/wglew.h"
#   define GetError() GetLastError()
#else
#   include "glew/glxew.h"
#   define GetError() errno
#endif

namespace oglu
{
using std::string;
using std::u16string;

void oglWorker::Start()
{
    const auto curCtx = oglContext_::CurrentContext();
    ShareContext = oglContext_::NewContext(curCtx, true);
    IsolateContext = oglContext_::NewContext(curCtx, false);
    ShareExecutor.Start([&]()
    {
        const auto& prefix = u"[oglShare]" + Name;
        common::SetThreadName(prefix);
        if (!ShareContext->UseContext())
        {
            oglLog().error(u"{} with HDC[{}] HRC[{}], error: {}\n", prefix, ShareContext->Hdc, ShareContext->Hrc, GetError());
        }
        oglLog().info(u"{} use HDC[{}] HRC[{}], GL version {}\n", prefix, ShareContext->Hdc, ShareContext->Hrc, oglUtil::GetVersionStr());
        ShareContext->SetDebug(MsgSrc::All, MsgType::All, MsgLevel::Notfication);
    }, [&]()
    {
        const auto& prefix = u"[oglShare]" + Name;
        if (!ShareContext->UnloadContext())
        {
            oglLog().error(u"{} terminate with HDC[{}] HRC[{}], error: {}\n", prefix, ShareContext->Hdc, ShareContext->Hrc, GetError());
        }
        ShareContext.reset();
    });
    IsolateExecutor.Start([&]()
    {
        const auto& prefix = u"[oglIsolate]" + Name;
        common::SetThreadName(prefix);
        if (!IsolateContext->UseContext())
        {
            oglLog().error(u"{} with HDC[{}] HRC[{}], error: {}\n", prefix, IsolateContext->Hdc, IsolateContext->Hrc, GetError());
        }
        oglLog().info(u"{} use HDC[{}] HRC[{}], GL version {}\n", prefix, IsolateContext->Hdc, IsolateContext->Hrc, oglUtil::GetVersionStr());
        IsolateContext->SetDebug(MsgSrc::All, MsgType::All, MsgLevel::Notfication);
    }, [&]()
    {
        const auto& prefix = u"[oglIsolate]" + Name;
        if (!IsolateContext->UnloadContext())
        {
            oglLog().error(u"{} terminate with HDC[{}] HRC[{}], error: {}\n", prefix, IsolateContext->Hdc, IsolateContext->Hrc, GetError());
        }
        IsolateContext.reset();
    });
}


}