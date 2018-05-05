#pragma once
#include "MiniLoggerRely.h"
#include "common/ThreadEx.h"
#if defined(_MSC_VER)
#   define _SILENCE_CXX17_OLD_ALLOCATOR_MEMBERS_DEPRECATION_WARNING 1
#   pragma warning(disable:4996)
#   include "boost/lockfree/queue.hpp"
#   pragma warning(default:4996)
#else
#   include "boost/lockfree/queue.hpp"
#endif
#include <mutex>
#include <condition_variable>


namespace common::mlog
{

class MINILOGAPI LoggerQBackend : public LoggerBackend
{
protected:
    boost::lockfree::queue<LogMessage*> MsgQueue;
    ThreadObject CurThread;
    std::mutex RunningMtx;
    std::condition_variable CondWait;
    std::atomic_bool ShouldRun = false;
    std::atomic_bool IsWaiting = false;
    void LoggerWorker();
    void virtual OnPrint(const LogMessage& msg) = 0;
    void virtual OnStart() {}
    void virtual OnStop() {}
    void Start();
public:
    LoggerQBackend(const size_t initSize = 64);
    virtual ~LoggerQBackend() override;
    void virtual Print(LogMessage* msg) override;
    void Flush();
    template<class T, typename... Args>
    static std::unique_ptr<T> InitialQBackend(Args&&... args)
    {
        auto backend = std::make_unique<T>(std::forward<Args>(args)...);
        backend->Start();
        return backend;
    }
};


}
