#include "oclPch.h"
#include "oclPromise.h"
#include "oclException.h"
#include "oclCmdQue.h"
#include "oclUtil.h"
#include "common/ContainerEx.hpp"
#include "common/Linq2.hpp"
#include <algorithm>

namespace oclu
{
using namespace std::string_view_literals;


DependEvents::DependEvents(const common::PromiseStub& pmss) noexcept
{
    auto clpmss = pmss.FilterOut<oclPromiseCore>();
    /*if (clpmss.size() < pmss.size())
        oclLog().warning(u"Some non-ocl promise detected as dependent events, will be ignored!\n");*/
    for (const auto& clpms : clpmss)
    {
        for (const auto& que : clpms->Depends.Queues)
        {
            if (que && !common::container::ContainInVec(Queues, que))
                Queues.push_back(que);
        }
        if (clpms->Event != nullptr)
        {
            Events.push_back(clpms->Event);
            const auto& que = clpms->Queue;
            if (!common::container::ContainInVec(Queues, que))
                Queues.push_back(que);
        }
    }
    Events.shrink_to_fit();
}
DependEvents::DependEvents() noexcept
{ }

void DependEvents::FlushAllQueues() const
{
    for (const auto& que : Queues)
        que->Flush();
}


void CL_CALLBACK oclPromiseCore::EventCallback(cl_event event, [[maybe_unused]]cl_int event_command_exec_status, void* user_data)
{
    auto& self = *reinterpret_cast<std::shared_ptr<oclPromiseCore>*>(user_data);
    Expects(event == self->Event);
    self->ExecutePmsCallbacks();
    delete &self;
}

oclPromiseCore::oclPromiseCore(DependEvents&& depend, const cl_event e, oclCmdQue que) : 
    Depends(std::move(depend)), Event(e), Queue(std::move(que))
{
    if (const auto it = std::find(Depends.Queues.begin(), Depends.Queues.end(), Queue); it != Depends.Queues.end())
    {
        Depends.Queues.erase(it);
    }
    for (const auto evt : Depends.Events)
        clRetainEvent(evt);
}
oclPromiseCore::~oclPromiseCore()
{
    for (const auto evt : Depends.Events)
        clReleaseEvent(evt);
    if (Event)
        clReleaseEvent(Event);
}

void oclPromiseCore::Flush()
{
    Depends.FlushAllQueues();
    if (Queue)
        clFlush(Queue->CmdQue);
}

[[nodiscard]] common::PromiseState oclPromiseCore::QueryState() noexcept
{
    using common::PromiseState;
    cl_int status;
    const auto ret = clGetEventInfo(Event, CL_EVENT_COMMAND_EXECUTION_STATUS, sizeof(cl_int), &status, nullptr);
    if (ret != CL_SUCCESS)
    {
        oclLog().warning(u"Error in reading cl_event's status: {}\n", oclUtil::GetErrorString(ret));
        return PromiseState::Invalid;
    }
    if (status < 0)
    {
        oclLog().warning(u"cl_event's status shows an error: {}\n", oclUtil::GetErrorString(status));
        return PromiseState::Error;
    }
    switch (status)
    {
    case CL_QUEUED:     return PromiseState::Unissued;
    case CL_SUBMITTED:  return PromiseState::Issued;
    case CL_RUNNING:    return PromiseState::Executing;
    case CL_COMPLETE:   return PromiseState::Success;
    default:            return PromiseState::Invalid;
    }
}

std::optional<common::BaseException> oclPromiseCore::Wait() noexcept
{
    if (!Event)
        return {};
    const auto ret = clWaitForEvents(1, &Event);
    if (ret != CL_SUCCESS)
    {
        if (Queue) Queue->Finish();
        return CREATE_EXCEPTION(OCLException, OCLException::CLComponent::Driver, ret, u"wait for event error");
    }
    return {};
}

bool oclPromiseCore::RegisterCallback(const common::PmsCore& pms)
{
    if (Queue->Context->Version < 11)
        return false;
    auto core = std::dynamic_pointer_cast<oclPromiseCore>(pms);
    const auto ptr = new std::shared_ptr<oclPromiseCore>(std::move(core));
    const auto ret = clSetEventCallback(Event, CL_COMPLETE, &EventCallback, ptr);
    return ret == CL_SUCCESS;
}

[[nodiscard]] uint64_t oclPromiseCore::ElapseNs() noexcept
{
    if (Event)
    {
        uint64_t from = 0, to = 0;
        const auto ret1 = clGetEventProfilingInfo(Event, CL_PROFILING_COMMAND_START, sizeof(cl_ulong), &from, nullptr);
        const auto ret2 = clGetEventProfilingInfo(Event, CL_PROFILING_COMMAND_END,   sizeof(cl_ulong), &to,   nullptr);
        if (ret1 == CL_SUCCESS && ret2 == CL_SUCCESS)
            return to - from;
    }
    return 0;
}

uint64_t oclPromiseCore::QueryTime(oclPromiseCore::TimeType type) const noexcept
{
    if (Event)
    {
        cl_profiling_info info = UINT32_MAX;
        switch (type)
        {
        case TimeType::Queued: info = CL_PROFILING_COMMAND_QUEUED; break;
        case TimeType::Submit: info = CL_PROFILING_COMMAND_SUBMIT; break;
        case TimeType::Start:  info = CL_PROFILING_COMMAND_START;  break;
        case TimeType::End:    info = CL_PROFILING_COMMAND_END;    break;
        default: break;
        }
        if (info != UINT32_MAX)
        {
            uint64_t time;
            clGetEventProfilingInfo(Event, info, sizeof(cl_ulong), &time, nullptr);
            return time;
        }
    }
    return 0;
}

std::string_view oclPromiseCore::GetEventName() const noexcept
{
    if (Event)
    {
        cl_command_type type;
        const auto ret = clGetEventInfo(Event, CL_EVENT_COMMAND_TYPE, sizeof(cl_command_type), &type, nullptr);
        if (ret != CL_SUCCESS)
        {
            oclLog().warning(u"Error in reading cl_event's coommand type: {}\n", oclUtil::GetErrorString(ret));
            return "Error";
        }
        switch (type)
        {
        case CL_COMMAND_NDRANGE_KERNEL:         return "NDRangeKernel"sv;
        case CL_COMMAND_TASK:                   return "Task"sv;
        case CL_COMMAND_NATIVE_KERNEL:          return "NativeKernel"sv;
        case CL_COMMAND_READ_BUFFER:            return "ReadBuffer"sv;
        case CL_COMMAND_WRITE_BUFFER:           return "WriteBuffer"sv;
        case CL_COMMAND_COPY_BUFFER:            return "CopyBuffer"sv;
        case CL_COMMAND_READ_IMAGE:             return "ReadImage"sv;
        case CL_COMMAND_WRITE_IMAGE:            return "WriteImage"sv;
        case CL_COMMAND_COPY_IMAGE:             return "CopyImage"sv;
        case CL_COMMAND_COPY_BUFFER_TO_IMAGE:   return "CopyBufferToImage"sv;
        case CL_COMMAND_COPY_IMAGE_TO_BUFFER:   return "CopyImageToBuffer"sv;
        case CL_COMMAND_MAP_BUFFER:             return "MapBuffer"sv;
        case CL_COMMAND_MAP_IMAGE:              return "MapImage"sv;
        case CL_COMMAND_UNMAP_MEM_OBJECT:       return "UnmapMemObject"sv;
        case CL_COMMAND_MARKER:                 return "Maker"sv;
        case CL_COMMAND_ACQUIRE_GL_OBJECTS:     return "AcquireGLObject"sv;
        case CL_COMMAND_RELEASE_GL_OBJECTS:     return "ReleaseGLObject"sv;
        case CL_COMMAND_READ_BUFFER_RECT:       return "ReadBufferRect"sv;
        case CL_COMMAND_WRITE_BUFFER_RECT:      return "WriteBufferRect"sv;
        case CL_COMMAND_COPY_BUFFER_RECT:       return "CopyBufferRect"sv;
        case CL_COMMAND_USER:                   return "User"sv;
        case CL_COMMAND_BARRIER:                return "Barrier"sv;
        case CL_COMMAND_MIGRATE_MEM_OBJECTS:    return "MigrateMemObject"sv;
        case CL_COMMAND_FILL_BUFFER:            return "FillBuffer"sv;
        case CL_COMMAND_FILL_IMAGE:             return "FillImage"sv;
        case CL_COMMAND_SVM_FREE:               return "SVMFree"sv;
        case CL_COMMAND_SVM_MEMCPY:             return "SVMMemcopy"sv;
        case CL_COMMAND_SVM_MEMFILL:            return "SVMMemfill"sv;
        case CL_COMMAND_SVM_MAP:                return "SVMMap"sv;
        case CL_COMMAND_SVM_UNMAP:              return "SVMUnmap"sv;
        // case CL_COMMAND_SVM_MIGRATE_MEM:        return "SVMMigrateMem"sv;
        default:                                return "Unknown"sv;
        }
    }
    return ""sv;
}


oclCustomEvent::oclCustomEvent(common::PmsCore&& pms, cl_event evt) : oclPromiseCore({}, evt, {}), Pms(std::move(pms))
{ }
oclCustomEvent::~oclCustomEvent()
{ }

void oclCustomEvent::Init()
{
    Pms->AddCallback([self = std::static_pointer_cast<oclCustomEvent>(shared_from_this())]()
        {
        clSetUserEventStatus(self->Event, CL_COMPLETE);
        self->ExecuteCallback();
        });
}


}
