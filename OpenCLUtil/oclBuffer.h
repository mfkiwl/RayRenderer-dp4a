#pragma once

#include "oclRely.h"
#include "oclPromiseTask.h"
#include "oclCmdQue.h"

namespace oclu
{

enum class MemFlag : cl_mem_flags
{
    ReadOnly = CL_MEM_READ_ONLY, WriteOnly = CL_MEM_WRITE_ONLY, ReadWrite = CL_MEM_READ_WRITE,
    UseHost = CL_MEM_USE_HOST_PTR, HostAlloc = CL_MEM_ALLOC_HOST_PTR, HostCopy = CL_MEM_COPY_HOST_PTR,
    HostWriteOnly = CL_MEM_HOST_WRITE_ONLY, HostReadOnly = CL_MEM_HOST_READ_ONLY, HostNoAccess = CL_MEM_HOST_NO_ACCESS
};
MAKE_ENUM_BITFIELD(MemFlag)

namespace detail
{

class OCLUAPI _oclBuffer : public NonCopyable, public NonMovable
{
    friend class _oclKernel;
    friend class _oclContext;
protected:
    const oclContext Context;
public:
    const MemFlag Flags;
    const size_t Size;
protected:
    const cl_mem memID;
    _oclBuffer(const oclContext& ctx, const MemFlag flag, const size_t size, const cl_mem id);
public:
    _oclBuffer(const oclContext& ctx, const MemFlag flag, const size_t size);
    virtual ~_oclBuffer();
    oclPromise Read(const oclCmdQue& que, void *buf, const size_t size, const size_t offset = 0, const bool shouldBlock = true) const;
    template<class T, class A>
    oclPromise Read(const oclCmdQue& que, vector<T, A>& buf, size_t count = 0, const size_t offset = 0, const bool shouldBlock = true) const
    {
        if (offset >= Size)
            COMMON_THROW(BaseException, L"offset overflow");
        if (count == 0)
            count = (Size - offset) / sizeof(T);
        else if(count * sizeof(T) + offset > Size)
            COMMON_THROW(BaseException, L"read size overflow");
        buf.resize(count);
        return Read(que, buf.data(), count * sizeof(T), offset, shouldBlock);
    }
    oclPromise Write(const oclCmdQue& que, const void * const buf, const size_t size, const size_t offset = 0, const bool shouldBlock = true) const;
    template<class T, class A>
    oclPromise Write(const oclCmdQue& que, const vector<T, A>& buf, size_t count = 0, const size_t offset = 0, const bool shouldBlock = true) const
    {
        const auto vsize = buf.size();
        if (count == 0)
            count = buf.size();
        else if (count > buf.size())
            COMMON_THROW(BaseException, L"write size overflow");
        const auto wsize = count * sizeof(T);
        return Write(que, buf.data(), wsize, offset, shouldBlock);
    }
    template<class T, size_t N>
    oclPromise Write(const oclCmdQue& que, const T(&buf)[N], const size_t offset = 0, const bool shouldBlock = true) const
    {
        auto wsize = N * sizeof(T);
        return Write(que, buf, wsize, offset, shouldBlock);
    }
};

class OCLUAPI GLInterOP
{
protected:
    void Lock(const cl_command_queue que, const cl_mem mem) const;
    void Unlock(const cl_command_queue que, const cl_mem mem) const;
};
template<typename Child>
struct OCLUAPI GLShared : GLInterOP
{
public:
    void Lock(const oclCmdQue& que) const
    {
        Lock(que->cmdque, static_cast<const Child*>(this)->memID);
    }
    void Unlock(const oclCmdQue& que) const
    {
        Unlock(que->cmdque, static_cast<const Child*>(this)->memID);
    }
};

class OCLUAPI _oclGLBuffer : public _oclBuffer, public GLShared<_oclGLBuffer>
{
private:
    const std::variant<oglu::oglBuffer, std::shared_ptr<oglu::detail::_oglTexBase>> GlResource;
public:
    _oclGLBuffer(const oclContext& ctx, const MemFlag flag, const oglu::oglBuffer buf);
    _oclGLBuffer(const oclContext& ctx, const MemFlag flag, const std::shared_ptr<oglu::detail::_oglTexBase> tex);
    virtual ~_oclGLBuffer() override;
};

}
using oclBuffer = Wrapper<detail::_oclBuffer>;
using oclGLBuffer = Wrapper<detail::_oclGLBuffer>;

}