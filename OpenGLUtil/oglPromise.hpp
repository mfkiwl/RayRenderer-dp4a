#pragma once
#include "oglPch.h"
#include "SystemCommon/AsyncAgent.h"

namespace oglu
{

class COMMON_EMPTY_BASES oglPromiseCore : public detail::oglCtxObject<true>, public common::PromiseProvider
{
public:
    GLsync SyncObj;
    uint64_t TimeBegin = 0, TimeEnd = 0;
    GLuint Query;
    oglPromiseCore()
    {
        CtxFunc->GenQueries(1, &Query);
        CtxFunc->GetInteger64v(GL_TIMESTAMP, (GLint64*)&TimeBegin); //suppose it is the time all commands are issued.
        CtxFunc->QueryCounter(Query, GL_TIMESTAMP); //this should be the time all commands are completed.
        SyncObj = CtxFunc->FenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
        CtxFunc->Flush(); //ensure sync object sended
    }
    ~oglPromiseCore()
    {
        if (EnsureValid())
        {
            CtxFunc->DeleteSync(SyncObj);
            CtxFunc->DeleteQueries(1, &Query);
        }
    }
    common::PromiseState GetState() noexcept override
    {
        CheckCurrent();
        switch (CtxFunc->ClientWaitSync(SyncObj, 0, 0))
        {
        case GL_TIMEOUT_EXPIRED:
            return common::PromiseState::Executing;
        case GL_ALREADY_SIGNALED:
            return common::PromiseState::Success;
        case GL_WAIT_FAILED:
            return common::PromiseState::Error;
        case GL_CONDITION_SATISFIED:
            return common::PromiseState::Executed;
        case GL_INVALID_VALUE:
            [[fallthrough]];
        default:
            return common::PromiseState::Invalid;
        }
    }
    common::PromiseState WaitPms() noexcept override
    {
        CheckCurrent();
        do
        {
            switch (CtxFunc->ClientWaitSync(SyncObj, 0, 1000'000'000))
            {
            case GL_TIMEOUT_EXPIRED:
                continue;
            case GL_ALREADY_SIGNALED:
                return common::PromiseState::Success;
            case GL_WAIT_FAILED:
                return common::PromiseState::Error;
            case GL_CONDITION_SATISFIED:
                return common::PromiseState::Executed;
            case GL_INVALID_VALUE:
                [[fallthrough]];
            default:
                return common::PromiseState::Invalid;
            }
        } while (true);
        return common::PromiseState::Invalid;
    }
    uint64_t ElapseNs() noexcept override
    {
        if (TimeEnd == 0)
        {
            CheckCurrent();
            CtxFunc->GetQueryObjectui64v(Query, GL_QUERY_RESULT, &TimeEnd);
        }
        if (TimeEnd == 0)
            return 0;
        else
            return TimeEnd - TimeBegin;
    }
};


template<typename T>
class COMMON_EMPTY_BASES oglPromise : protected common::detail::ResultHolder<T>, public common::detail::PromiseResult_<T>
{
    friend class oglUtil;
protected:
    common::CachedPromiseProvider<oglPromiseCore> Promise;
    common::PromiseProvider& GetPromise() noexcept override { return Promise; }
    T GetResult() override
    {
        this->CheckResultExtracted();
        if constexpr (std::is_same_v<T, void>)
            return;
        else
            return std::move(this->Result);
    }
public:
    template<typename U>
    oglPromise(U&& data) : common::detail::ResultHolder<T>(std::forward<U>(data)) { }
    ~oglPromise() override { }
};


class COMMON_EMPTY_BASES oglFinishPromise : public common::detail::PromiseResult_<void>, protected common::PromiseProvider
{
protected:
    common::PromiseState GetState() noexcept override
    {
        return common::PromiseState::Executed; // simply return, make it invoke wait
    }
    void PreparePms() override 
    { 
        glFinish();
    }
    common::PromiseState WaitPms() noexcept override
    {
        return common::PromiseState::Executed;
    }
    common::PromiseProvider& GetPromise() noexcept override 
    { 
        return *this;
    }
    void GetResult() override
    { }
public:
};


}
