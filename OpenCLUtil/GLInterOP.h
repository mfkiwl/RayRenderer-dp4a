#pragma once
#include "oclRely.h"
#include "oclCmdQue.h"

namespace oclu
{
namespace detail
{

class OCLUAPI GLInterOP
{
protected:
    static cl_mem CreateMemFromGLBuf(const cl_context ctx, const cl_mem_flags flag, const GLuint bufId);
    static cl_mem CreateMemFromGLTex(const cl_context ctx, const cl_mem_flags flag, const cl_GLenum texType, const GLuint texId);
    void Lock(const cl_command_queue que, const cl_mem mem) const;
    void Unlock(const cl_command_queue que, const cl_mem mem) const;
};
template<typename Child>
class OCLUAPI GLShared : protected GLInterOP
{
protected:
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


}
}