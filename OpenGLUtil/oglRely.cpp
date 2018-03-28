#include "oglRely.h"
#include "common/ThreadEx.inl"

#pragma comment (lib, "opengl32.lib")// link Microsoft OpenGL lib
#pragma comment (lib, "glu32.lib")// link OpenGL Utility lib

#pragma message("Compiling OpenGLUtil with " STRINGIZE(COMMON_SIMD_INTRIN) )


namespace oglu
{

using namespace common::mlog;
MiniLogger<false>& oglLog()
{
    static MiniLogger<false> ogllog(u"OpenGLUtil", { GetConsoleBackend() });
    return ogllog;
}


}