#include "stacktrace.h"
#include "common/StrCharset.hpp"
#include "common/ThreadEx.inl"
#if defined(_WIN32)
#   define BOOST_STACKTRACE_USE_WINDBG_CACHED 1
#else
#   define BOOST_STACKTRACE_USE_BACKTRACE 1
#endif
#include <boost/stacktrace.hpp>
#include <thread>
#include <condition_variable>
#include <mutex>
#include <atomic>

namespace str = common::str;
namespace stacktrace
{

class StackExplainer
{
    std::thread WorkThread;
    std::mutex CallerMutex;
    std::condition_variable CallerCV;
    std::mutex WorkerMutex;
    std::condition_variable WorkerCV;
    const boost::stacktrace::stacktrace* Stk;
    std::vector<common::StackTraceItem>* Output;
    bool ShouldRun;
public:
    StackExplainer() : ShouldRun(true)
    {
        std::unique_lock<std::mutex> callerLock(CallerMutex);
        std::unique_lock<std::mutex> workerLock(WorkerMutex);
        WorkThread = std::thread([this]()
        {
            common::SetThreadName(u"StackExplainer");
            std::unique_lock<std::mutex> lock(WorkerMutex);
            CallerCV.notify_one();
            WorkerCV.wait(lock);
            while (ShouldRun)
            {
                for (const auto& frame : *Stk)
                    Output->emplace_back(str::to_u16string(frame.source_file()), str::to_u16string(frame.name()), frame.source_line());
                CallerCV.notify_one();
                WorkerCV.wait(lock);
            }
        });
        CallerCV.wait(workerLock);
    }
    ~StackExplainer()
    {
        std::unique_lock<std::mutex> callerLock(CallerMutex);
        std::unique_lock<std::mutex> workerLock(WorkerMutex);
        if (WorkThread.joinable())
        {
            ShouldRun = false;
            WorkerCV.notify_one();
            WorkThread.join();
        }
    }
    std::vector<common::StackTraceItem> Explain(const boost::stacktrace::stacktrace& st)
    {
        std::vector<common::StackTraceItem> ret;
        {
            std::unique_lock<std::mutex> callerLock(CallerMutex);
            std::unique_lock<std::mutex> workerLock(WorkerMutex);
            Stk = &st; Output = &ret;
            WorkerCV.notify_one();
            CallerCV.wait(workerLock);
        }
        return ret;
    }
};


std::vector<common::StackTraceItem> GetStack()
{
    static StackExplainer Explainer;
    boost::stacktrace::stacktrace st;
    return Explainer.Explain(st);
}


}
