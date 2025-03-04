#include "oglPch.h"
#include "GLFuncWrapper.h"
#include "oglUtil.h"
#include "oglException.h"
#include "oglContext.h"
#include "oglBuffer.h"

#include <boost/preprocessor/control/if.hpp>
#include <boost/preprocessor/punctuation/comma_if.hpp>
#include <boost/preprocessor/seq/for_each_i.hpp>
#include <boost/preprocessor/seq/for_each.hpp>
#include <boost/preprocessor/variadic/to_seq.hpp>
#include <boost/preprocessor/tuple/elem.hpp>


typedef void (APIENTRYP PFNGLDRAWARRAYSPROC) (GLenum mode, GLint first, GLsizei count);
typedef void (APIENTRYP PFNGLDRAWELEMENTSPROC) (GLenum mode, GLsizei count, GLenum type, const void* indices);
typedef void (APIENTRYP PFNGLGENTEXTURESPROC) (GLsizei n, GLuint* textures);
typedef void (APIENTRYP PFNGLBINDTEXTUREPROC) (GLenum target, GLuint texture);
typedef void (APIENTRYP PFNGLDELETETEXTURESPROC) (GLsizei n, const GLuint* textures);
typedef void (APIENTRYP PFNGLCULLFACEPROC) (GLenum mode);
typedef void (APIENTRYP PFNGLFRONTFACEPROC) (GLenum mode);
typedef void (APIENTRYP PFNGLHINTPROC) (GLenum target, GLenum mode);
typedef void (APIENTRYP PFNGLDISABLEPROC) (GLenum cap);
typedef void (APIENTRYP PFNGLENABLEPROC) (GLenum cap);
typedef GLboolean (APIENTRYP PFNGLISENABLEDPROC) (GLenum cap);
typedef void (APIENTRYP PFNGLFINISHPROC) (void);
typedef void (APIENTRYP PFNGLFLUSHPROC) (void);
typedef void (APIENTRYP PFNGLCLEARPROC) (GLbitfield mask);
typedef void (APIENTRYP PFNGLCLEARCOLORPROC) (GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
typedef void (APIENTRYP PFNGLCLEARSTENCILPROC) (GLint s);
typedef void (APIENTRYP PFNGLCLEARDEPTHPROC) (GLdouble depth); typedef void (APIENTRYP PFNGLGETBOOLEANVPROC) (GLenum pname, GLboolean* data);
typedef void (APIENTRYP PFNGLGETDOUBLEVPROC) (GLenum pname, GLdouble* data);
typedef void (APIENTRYP PFNGLDEPTHFUNCPROC) (GLenum func);
typedef void (APIENTRYP PFNGLVIEWPORTPROC) (GLint x, GLint y, GLsizei width, GLsizei height);
typedef GLenum (APIENTRYP PFNGLGETERRORPROC) (void);
typedef void (APIENTRYP PFNGLGETFLOATVPROC) (GLenum pname, GLfloat* data);
typedef void (APIENTRYP PFNGLGETINTEGERVPROC) (GLenum pname, GLint* data);
typedef const GLubyte* (APIENTRYP PFNGLGETSTRINGPROC) (GLenum name);


#undef APIENTRY
#if COMMON_OS_WIN
#   define WIN32_LEAN_AND_MEAN 1
#   define NOMINMAX 1
#   include <Windows.h>
#   include "GL/wglext.h"
//fucking wingdi defines some terrible macro
#   undef ERROR
#   undef MemoryBarrier
#   pragma message("Compiling OpenGLUtil with wgl-ext[" STRINGIZE(WGL_WGLEXT_VERSION) "]")
    using DCType = HDC;
#elif COMMON_OS_UNIX
#   include <dlfcn.h>
#   include <X11/X.h>
#   include <X11/Xlib.h>
#   include <GL/glx.h>
#   include "GL/glxext.h"
//fucking X11 defines some terrible macro
#   undef Success
#   undef Always
#   undef None
#   undef Bool
#   undef Int
#   pragma message("Compiling OpenGLUtil with glx-ext[" STRINGIZE(GLX_GLXEXT_VERSION) "]")
    using DCType = Display*;
#else
#   error "unknown os"
#endif




namespace oglu
{
using namespace std::string_view_literals;
[[maybe_unused]] constexpr auto PlatFuncsSize = sizeof(PlatFuncs);
[[maybe_unused]] constexpr auto CtxFuncsSize = sizeof(CtxFuncs);

template<typename T>
struct ResourceKeeper
{
    std::vector<std::unique_ptr<T>> Items;
    common::SpinLocker Locker;
    auto Lock() { return Locker.LockScope(); }
    T* FindOrCreate(void* key)
    {
        const auto lock = Lock();
        for (const auto& item : Items)
        {
            if (item->Target == key)
                return item.get();
        }
        return Items.emplace_back(std::make_unique<T>(key)).get();
    }
    void Remove(void* key)
    {
        const auto lock = Lock();
        for (auto it = Items.begin(); it != Items.end(); ++it)
        {
            if ((*it)->Target == key)
            {
                Items.erase(it);
                return;
            }
        }
    }
};
thread_local const PlatFuncs* PlatFunc = nullptr;
thread_local const CtxFuncs* CtxFunc = nullptr;

static auto& GetPlatFuncsMap()
{
    static ResourceKeeper<PlatFuncs> PlatFuncMap;
    return PlatFuncMap;
}
static auto& GetCtxFuncsMap()
{
    static ResourceKeeper<CtxFuncs> CtxFuncMap;
    return CtxFuncMap;
}
static void PreparePlatFuncs(void* hDC)
{
    if (hDC == nullptr)
        PlatFunc = nullptr;
    else
    {
        auto& rmap = GetPlatFuncsMap();
        PlatFunc = rmap.FindOrCreate(hDC);
    }
}
static void PrepareCtxFuncs(void* hRC)
{
    if (hRC == nullptr)
        CtxFunc = nullptr;
    else
    {
        auto& rmap = GetCtxFuncsMap();
        CtxFunc = rmap.FindOrCreate(hRC);
    }
}

#if COMMON_OS_WIN
constexpr PIXELFORMATDESCRIPTOR PixFmtDesc =    // pfd Tells Windows How We Want Things To Be
{
    sizeof(PIXELFORMATDESCRIPTOR),              // Size Of This Pixel Format Descriptor
    1,                                          // Version Number
    PFD_DRAW_TO_WINDOW/*Support Window*/ | PFD_SUPPORT_OPENGL/*Support OpenGL*/ | PFD_DOUBLEBUFFER/*Support Double Buffering*/ | PFD_GENERIC_ACCELERATED,
    PFD_TYPE_RGBA,                              // Request An RGBA Format
    32,                                         // Select Our Color Depth
    0, 0, 0, 0, 0, 0,                           // Color Bits Ignored
    0, 0,                                       // No Alpha Buffer, Shift Bit Ignored
    0, 0, 0, 0, 0,                              // No Accumulation Buffer, Accumulation Bits Ignored
    24,                                         // 24Bit Z-Buffer (Depth Buffer) 
    8,                                          // 8Bit Stencil Buffer
    0,                                          // No Auxiliary Buffer
    PFD_MAIN_PLANE,                             // Main Drawing Layer
    0,                                          // Reserved
    0, 0, 0                                     // Layer Masks Ignored
};
void oglUtil::SetPixFormat(void* hdc_)
{
    const auto hdc = reinterpret_cast<HDC>(hdc_);
    const int pixFormat = ChoosePixelFormat(hdc, &PixFmtDesc);
    SetPixelFormat(hdc, pixFormat, &PixFmtDesc);
}
#elif COMMON_OS_LINUX
constexpr int VisualAttrs[] =
{
    GLX_X_RENDERABLE,   1,
    GLX_X_VISUAL_TYPE,  GLX_TRUE_COLOR,
    GLX_RENDER_TYPE,    GLX_RGBA_BIT,
    GLX_DRAWABLE_TYPE,  GLX_WINDOW_BIT,
    GLX_RED_SIZE,       8,
    GLX_GREEN_SIZE,     8,
    GLX_BLUE_SIZE,      8,
    GLX_ALPHA_SIZE,     8,
    GLX_DEPTH_SIZE,     24,
    GLX_STENCIL_SIZE,   8,
    GLX_DOUBLEBUFFER,   1,
    0
};
constexpr int VisualAttrs2[] =
{
    GLX_X_RENDERABLE,   1,
    GLX_X_VISUAL_TYPE,  GLX_TRUE_COLOR,
    GLX_RENDER_TYPE,    GLX_RGBA_BIT,
    GLX_DRAWABLE_TYPE,  GLX_PIXMAP_BIT,
    GLX_RED_SIZE,       8,
    GLX_GREEN_SIZE,     8,
    GLX_BLUE_SIZE,      8,
    GLX_ALPHA_SIZE,     8,
    GLX_DEPTH_SIZE,     24,
    GLX_STENCIL_SIZE,   8,
    GLX_DOUBLEBUFFER,   1,
    0
};
std::optional<GLContextInfo> oglUtil::GetBasicContextInfo(void* display, bool useOffscreen)
{
    GLContextInfo info;
    info.DeviceContext = display;
    const auto disp = reinterpret_cast<Display*>(display);
    int fbCount = 0;
    const auto configs = glXChooseFBConfig(disp, DefaultScreen(disp), useOffscreen ? VisualAttrs2 : VisualAttrs, &fbCount);
    if (!configs || fbCount == 0)
        return {};
    if (0 != glXGetFBConfigAttrib(disp, configs[0], GLX_VISUAL_ID, &info.VisualId))
        return {};
    info.FBConfigs = configs;
    info.IsWindowDrawable = !useOffscreen;
    return info;
}
void oglUtil::InitDrawable(GLContextInfo& info, uint32_t host)
{
    const auto display = reinterpret_cast<Display*>(info.DeviceContext);
    const auto config  = reinterpret_cast<GLXFBConfig*>(info.FBConfigs)[0];
    if (info.IsWindowDrawable)
    {
        const auto glxwindow = glXCreateWindow(display, config, host, nullptr);
        info.Drawable = glxwindow;
    }
    else
    {
        XVisualInfo* vi = glXGetVisualFromFBConfig(display, config);
        const auto glxpixmap = glXCreateGLXPixmap(display, vi, host);
        info.Drawable = glxpixmap;
    }
}
void oglUtil::DestroyDrawable(GLContextInfo& info)
{
    const auto display = reinterpret_cast<Display*>(info.DeviceContext);
    if (info.IsWindowDrawable)
    {
        glXDestroyWindow(display, info.Drawable);
    }
    else
    {
        glXDestroyGLXPixmap(display, info.Drawable);
    }
    XFree(info.FBConfigs);
}
#endif

oglContext oglContext_::InitContext(const GLContextInfo& info)
{
    constexpr uint32_t VERSIONS[] = { 46,45,44,43,42,41,40,33,32,31,30 };
    const auto oldCtx = Refresh();
    PreparePlatFuncs(info.DeviceContext); // ensure PlatFuncs exists
    Expects(PlatFunc);
    void* ctx = nullptr;
    if (LatestVersion == 0) // perform update
    {
        for (const auto ver : VERSIONS)
        {
            if (ctx = PlatFunc->CreateNewContext(info, ver); ctx && PlatFunc->MakeGLContextCurrent(info, ctx))
            {
                const auto verStr = common::str::to_u16string(
                    reinterpret_cast<const char*>(glGetString(GL_VERSION)), common::str::Encoding::UTF8);
                const auto vendor = common::str::to_u16string(
                    reinterpret_cast<const char*>(glGetString(GL_VENDOR)), common::str::Encoding::UTF8);
                int32_t major = 0, minor = 0;
                glGetIntegerv(GL_MAJOR_VERSION, &major);
                glGetIntegerv(GL_MINOR_VERSION, &minor);
                const uint32_t realVer = major * 10 + minor;
                oglLog().info(u"Latest GL version [{}] from [{}]\n", verStr, vendor);
                common::UpdateAtomicMaximum(LatestVersion, realVer);
                break;
            }
        }
    }
    else
    {
        ctx = PlatFunc->CreateNewContext(info, LatestVersion.load());
    }
    if (!ctx)
    {
#if COMMON_OS_WIN
        oglLog().error(u"failed to init context on HDC[{}], error: {}\n", info.DeviceContext, PlatFuncs::GetSystemError());
#elif COMMON_OS_LINUX
        oglLog().error(u"failed to init context on Display[{}] Drawable[{}], error: {}\n",
            info.DeviceContext, info.Drawable, PlatFuncs::GetSystemError());
#endif
        return {};
    }
#if COMMON_OS_WIN
    oglContext newCtx(new oglContext_(std::make_shared<detail::SharedContextCore>(), PlatFunc, ctx));
#elif COMMON_OS_LINUX
    oglContext newCtx(new oglContext_(std::make_shared<detail::SharedContextCore>(), PlatFunc, ctx, info.Drawable));
#endif
    newCtx->Init(true);
    PushToMap(newCtx);
    newCtx->UnloadContext();
    if (oldCtx)
        oldCtx->UseContext();
    return newCtx;
}


#if COMMON_OS_UNIX
static int TmpXErrorHandler(Display* disp, XErrorEvent* evt)
{
    thread_local std::string txtBuf;
    txtBuf.resize(1024, '\0');
    XGetErrorText(disp, evt->error_code, txtBuf.data(), 1024); // return value undocumented, cannot rely on that
    txtBuf.resize(std::char_traits<char>::length(txtBuf.data()));
    oglLog().warning(u"X11 report an error with code[{}][{}]:\t{}\n", evt->error_code, evt->minor_code,
        common::str::to_u16string(txtBuf, common::str::Encoding::UTF8));
    return 0;
}
#endif


static void ShowQuerySuc (const std::string_view tarName, const std::string_view ext, void* ptr)
{
    oglLog().verbose(FMT_STRING(u"Func [{}] uses [{}] ({:p})\n"), tarName, ext, (void*)ptr);
}
static void ShowQueryFail(const std::string_view tarName)
{
    oglLog().warning(FMT_STRING(u"Func [{}] not found\n"), tarName);
}
static void ShowQueryFall(const std::string_view tarName)
{
    oglLog().warning(FMT_STRING(u"Func [{}] fallback to default\n"), tarName);
}

template<typename T>
static void QueryPlatFunc(T& target, const std::string_view tarName, const std::pair<bool, bool> shouldPrint)
{
    const auto [printSuc, printFail] = shouldPrint;
#if COMMON_OS_WIN
    const auto ptr = wglGetProcAddress(tarName.data());
#elif COMMON_OS_DARWIN
    const auto ptr = NSGLGetProcAddress(tarName.data());
#else
    const auto ptr = glXGetProcAddress(reinterpret_cast<const GLubyte*>(tarName.data()));
#endif
    if (ptr)
    {
        target = reinterpret_cast<T>(ptr);
        if (printSuc)
            ShowQuerySuc(tarName, "", (void*)ptr);
    }
    else
    {
        target = nullptr;
        if (printFail)
            ShowQueryFail(tarName);
    }
}


static std::atomic<uint32_t>& GetDebugPrintCore() noexcept
{
    static std::atomic<uint32_t> ShouldPrint = 0;
    return ShouldPrint;
}
void SetFuncShouldPrint(const bool printSuc, const bool printFail) noexcept
{
    const uint32_t val = (printSuc ? 0x1u : 0x0u) | (printFail ? 0x2u : 0x0u);
    GetDebugPrintCore() = val;
}
static std::pair<bool, bool> GetFuncShouldPrint() noexcept
{
    const uint32_t val = GetDebugPrintCore();
    const bool printSuc  = (val & 0x1u) == 0x1u;
    const bool printFail = (val & 0x2u) == 0x2u;
    return { printSuc, printFail };
}


#if COMMON_OS_WIN
common::container::FrozenDenseSet<std::string_view> PlatFuncs::GetExtensions(void* hdc) const
{
    const char* exts = nullptr;
    if (wglGetExtensionsStringARB_)
    {
        exts = wglGetExtensionsStringARB_(hdc);
    }
    else if (wglGetExtensionsStringEXT_)
    {
        exts = wglGetExtensionsStringEXT_();
    }
    else
        COMMON_THROWEX(OGLException, OGLException::GLComponent::OGLU, u"no avaliable implementation for wglGetExtensionsString");
    return common::str::Split(exts, ' ', false);
}
#else
common::container::FrozenDenseSet<std::string_view> PlatFuncs::GetExtensions(void* dpy, int screen) const
{
    const char* exts = glXQueryExtensionsString((Display*)dpy, screen);
    return common::str::Split(exts, ' ', false);
}
#endif

PlatFuncs::PlatFuncs(void* target) : Target(target)
{
    const auto shouldPrint = GetFuncShouldPrint();

#define QUERY_FUNC(name) QueryPlatFunc(PPCAT(name,_), STRINGIZE(name), shouldPrint)
    
#if COMMON_OS_WIN
    const auto hdc = reinterpret_cast<HDC>(Target);
    HGLRC tmpHrc = nullptr;
    if (!wglGetCurrentContext())
    { // create temp hrc since wgl requires a context to use wglGetProcAddress
        tmpHrc = wglCreateContext(hdc);
        wglMakeCurrent(hdc, tmpHrc);
    }
    QUERY_FUNC(wglGetExtensionsStringARB);
    QUERY_FUNC(wglGetExtensionsStringEXT);
    Extensions = GetExtensions(hdc);
    QUERY_FUNC(wglCreateContextAttribsARB);
    if (tmpHrc)
    {
        wglMakeCurrent(hdc, nullptr);
        wglDeleteContext(tmpHrc);
    }
#else
    const auto display = reinterpret_cast<Display*>(Target);
    QUERY_FUNC(glXGetCurrentDisplay);
    Extensions = GetExtensions(display, DefaultScreen(display));
    QUERY_FUNC(glXCreateContextAttribsARB);
#endif

#if COMMON_OS_WIN
    SupportFlushControl = Extensions.Has("WGL_ARB_context_flush_control");
    SupportSRGB = Extensions.Has("WGL_ARB_framebuffer_sRGB") || Extensions.Has("WGL_EXT_framebuffer_sRGB");
#else
    SupportFlushControl = Extensions.Has("GLX_ARB_context_flush_control");
    SupportSRGB = Extensions.Has("GLX_ARB_framebuffer_sRGB") || Extensions.Has("GLX_EXT_framebuffer_sRGB");
#endif
    

#undef QUERY_FUNC
}


#if COMMON_OS_UNIX
struct X11Initor
{
    X11Initor()
    {
        XInitThreads();
        XSetErrorHandler(&TmpXErrorHandler);
    }
};
#endif

void PlatFuncs::InJectRenderDoc(const common::fs::path& dllPath)
{
#if COMMON_OS_WIN
    if (common::fs::exists(dllPath))
        LoadLibrary(dllPath.c_str());
    else
        LoadLibrary(L"renderdoc.dll");
#else
    if (common::fs::exists(dllPath))
        dlopen(dllPath.c_str(), RTLD_LAZY);
    else
        dlopen("renderdoc.dll", RTLD_LAZY);
#endif
}
void PlatFuncs::InitEnvironment()
{
#if COMMON_OS_WIN

    HWND tmpWND = CreateWindow(
        L"Static", L"Fake Window",          // window class, title
        WS_CLIPSIBLINGS | WS_CLIPCHILDREN,  // style
        0, 0,                               // position x, y
        1, 1,                               // width, height
        NULL, NULL,                         // parent window, menu
        nullptr, NULL);                     // instance, param
    HDC tmpDC = GetDC(tmpWND);
    const int PixelFormat = ChoosePixelFormat(tmpDC, &PixFmtDesc);
    SetPixelFormat(tmpDC, PixelFormat, &PixFmtDesc);
    HGLRC tmpRC = wglCreateContext(tmpDC);
    wglMakeCurrent(tmpDC, tmpRC);

    wglMakeCurrent(nullptr, nullptr);
    wglDeleteContext(tmpRC);
    DeleteDC(tmpDC);
    DestroyWindow(tmpWND);

#else
    
    XInitThreads();
    XSetErrorHandler(&TmpXErrorHandler);
    const char* const disp = getenv("DISPLAY");
    Display* display = XOpenDisplay(disp ? disp : ":0.0");
    /* open display */
    if (!display)
    {
        oglLog().error(u"Failed to open display\n");
        return;
    }

    /* get framebuffer configs, any is usable (might want to add proper attribs) */
    int fbcount = 0;
    ::GLXFBConfig* fbc = glXChooseFBConfig(display, DefaultScreen(display), VisualAttrs, &fbcount);
    if (!fbc)
    {
        oglLog().error(u"Failed to get FBConfig\n");
        return;
    }
    XVisualInfo* vi = glXGetVisualFromFBConfig(display, fbc[0]);
    const auto tmpRC = glXCreateContext(display, vi, 0, GL_TRUE);
    Window win = DefaultRootWindow(display);
    glXMakeCurrent(display, win, tmpRC);

    glXMakeCurrent(nullptr, win, nullptr);
    glXDestroyContext(display, tmpRC);

#endif
}

void* PlatFuncs::GetCurrentDeviceContext()
{
#if COMMON_OS_WIN
    const auto hDC = wglGetCurrentDC();
#else
    static X11Initor initor;
    const auto hDC = glXGetCurrentDisplay();
#endif
    PreparePlatFuncs(hDC);
    return hDC;
}
void* PlatFuncs::GetCurrentGLContext()
{
    [[maybe_unused]] const auto dummy = GetCurrentDeviceContext();
#if COMMON_OS_WIN
    const auto hRC = wglGetCurrentContext();
#else
    const auto hRC = glXGetCurrentContext();
#endif
    PrepareCtxFuncs(hRC);
    return hRC;
}
void PlatFuncs::DeleteGLContext(void* hRC) const
{
#if COMMON_OS_WIN
    wglDeleteContext((HGLRC)hRC);
#else
    glXDestroyContext((Display*)Target, (GLXContext)hRC);
#endif
    GetCtxFuncsMap().Remove(hRC);
}
#if COMMON_OS_WIN
bool  PlatFuncs::MakeGLContextCurrent(void* hRC) const
{
    const bool ret = wglMakeCurrent((HDC)Target, (HGLRC)hRC);
#else
bool  PlatFuncs::MakeGLContextCurrent(unsigned long DRW, void* hRC) const
{
    const bool ret = glXMakeCurrent((Display*)Target, DRW, (GLXContext)hRC);
#endif
    if (ret)
    {
        PrepareCtxFuncs(hRC);
    }
    return ret;
}
bool PlatFuncs::MakeGLContextCurrent(const GLContextInfo& info, void* hRC) const
{
    if (info.DeviceContext != Target) return false;
#if COMMON_OS_WIN
    return MakeGLContextCurrent(hRC);
#else
    return MakeGLContextCurrent(info.Drawable, hRC);
#endif
}
#if COMMON_OS_WIN
uint32_t PlatFuncs::GetSystemError()
{
    return GetLastError();
}
#else
unsigned long PlatFuncs::GetCurrentDrawable()
{
    return glXGetCurrentDrawable();
}
int32_t  PlatFuncs::GetSystemError()
{
    return errno;
}
#endif
std::vector<int32_t> PlatFuncs::GenerateContextAttrib(const uint32_t version, bool needFlushControl)
{
    std::vector<int32_t> ctxAttrb;
#if COMMON_OS_WIN
    ctxAttrb.insert(ctxAttrb.end(),
    {
        WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
        WGL_CONTEXT_FLAGS_ARB, WGL_CONTEXT_DEBUG_BIT_ARB
    });
    constexpr int32_t VerMajor  = WGL_CONTEXT_MAJOR_VERSION_ARB;
    constexpr int32_t VerMinor  = WGL_CONTEXT_MINOR_VERSION_ARB;
    constexpr int32_t FlushFlag = WGL_CONTEXT_RELEASE_BEHAVIOR_ARB;
    constexpr int32_t FlushVal  = WGL_CONTEXT_RELEASE_BEHAVIOR_NONE_ARB;
#else
    ctxAttrb.insert(ctxAttrb.end(),
    {
        GLX_CONTEXT_PROFILE_MASK_ARB, GLX_CONTEXT_CORE_PROFILE_BIT_ARB,
        GLX_CONTEXT_FLAGS_ARB, GLX_CONTEXT_DEBUG_BIT_ARB
    });
    constexpr int32_t VerMajor  = GLX_CONTEXT_MAJOR_VERSION_ARB;
    constexpr int32_t VerMinor  = GLX_CONTEXT_MINOR_VERSION_ARB;
    constexpr int32_t FlushFlag = GLX_CONTEXT_RELEASE_BEHAVIOR_ARB;
    constexpr int32_t FlushVal  = GLX_CONTEXT_RELEASE_BEHAVIOR_NONE_ARB;
    needFlushControl = false;
#endif
    if (version != 0)
    {
        ctxAttrb.insert(ctxAttrb.end(),
            {
                VerMajor, static_cast<int32_t>(version / 10),
                VerMinor, static_cast<int32_t>(version % 10),
            });
    }
    if (needFlushControl && PlatFunc->SupportFlushControl)
    {
        ctxAttrb.insert(ctxAttrb.end(), { FlushFlag, FlushVal }); // all the same
    }
    ctxAttrb.push_back(0);
    return ctxAttrb;
}
void* PlatFuncs::CreateNewContext(const GLContextInfo& info, const uint32_t version) const
{
    if (info.DeviceContext != Target)
        return nullptr;
    const auto target = reinterpret_cast<DCType>(Target);
    const auto ctxAttrb = GenerateContextAttrib(version, true);
#if COMMON_OS_WIN
    return wglCreateContextAttribsARB_(target, nullptr, ctxAttrb.data());
#elif COMMON_OS_LINUX
    return glXCreateContextAttribsARB_(target,
        reinterpret_cast<GLXFBConfig*>(info.FBConfigs)[0], nullptr, true, ctxAttrb.data());
#endif
}
void* PlatFuncs::CreateNewContext(const oglContext_* prevCtx, const bool isShared, const int32_t* attribs) const
{
    if (prevCtx == nullptr || &prevCtx->PlatHolder != this)
    {
        return nullptr;
    }
    else
    {
        const auto target = reinterpret_cast<DCType>(Target);
#if defined(_WIN32)
        const auto newHrc = wglCreateContextAttribsARB_(target, isShared ? prevCtx->Hrc : nullptr, attribs);
        return newHrc;
#else
        int num_fbc = 0;
        const auto fbc = glXChooseFBConfig(target, DefaultScreen(target), VisualAttrs, &num_fbc);
        const auto newHrc = glXCreateContextAttribsARB_(target, fbc[0], isShared ? prevCtx->Hrc : nullptr, true, attribs);
        return newHrc;
#endif
    }
}
void PlatFuncs::SwapBuffer(const oglContext_& ctx) const
{
    const auto target = reinterpret_cast<DCType>(Target);
#if COMMON_OS_WIN
    SwapBuffers(target);
#elif COMMON_OS_LINUX
    glXSwapBuffers(target, ctx.DRW);
#endif
}


template<typename T, size_t N>
static void QueryGLFunc(T& target, const std::string_view tarName, const std::pair<bool, bool> shouldPrint, const std::array<std::string_view, N> suffixes,
    void* fallback = nullptr)
{
    Expects(tarName.size() < 96);
    const auto [printSuc, printFail] = shouldPrint;
    std::array<char, 128> funcName = { 0 };
    funcName[0] = 'g', funcName[1] = 'l';
    memcpy_s(&funcName[2], funcName.size() - 2, tarName.data(), tarName.size());
    const size_t baseLen = tarName.size() + 2;
    for (const auto& sfx : suffixes)
    {
        memcpy_s(&funcName[baseLen], funcName.size() - baseLen, sfx.data(), sfx.size());
        funcName[baseLen + sfx.size()] = '\0';
#if COMMON_OS_WIN
        const auto ptr = wglGetProcAddress(funcName.data());
#elif COMMON_OS_DARWIN
        const auto ptr = NSGLGetProcAddress(funcName.data());
#else
        const auto ptr = glXGetProcAddress(reinterpret_cast<const GLubyte*>(funcName.data()));
#endif
        if (ptr)
        {
            target = reinterpret_cast<T>(ptr);
            if (printSuc)
                ShowQuerySuc(tarName, sfx, (void*)ptr);
            return;
        }
    }
    if (fallback)
    {
        target = reinterpret_cast<T>(fallback);
        if (printFail)
            ShowQueryFall(tarName);
    }
    else
    {
        target = nullptr;
        if (printFail)
            ShowQueryFail(tarName);
    }
}

template <typename L, typename R> struct FuncTypeMatcher
{
    static constexpr bool IsMatch = false;
};
template<typename L, typename R>
inline constexpr bool ArgTypeMatcher()
{
    if constexpr (std::is_same_v<L, R>) return true;
    if constexpr (std::is_same_v<L, GLenum> && std::is_same_v<R, GLint>) return true; // some function use GLint for GLEnum
    return false;
}
template <typename R, typename... A, typename... B>
struct FuncTypeMatcher<R(*)(A...), R(*)(B...)>
{
    static constexpr bool IsMatch = (... && ArgTypeMatcher<A, B>());
};


#if COMMON_COMPILER_MSVC
#   pragma warning(push)
#   pragma warning(disable:4003)
#endif

CtxFuncs::CtxFuncs(void* target) : Target(target)
{
    const auto shouldPrint = GetFuncShouldPrint();

#define FUNC_TYPE_CHECK_(dst, part, name, sfx) static_assert(FuncTypeMatcher<decltype(dst), \
    BOOST_PP_CAT(BOOST_PP_CAT(PFNGL, part), BOOST_PP_CAT(sfx, PROC))>::IsMatch, \
    "mismatch type for variant [" #sfx "] on [" STRINGIZE(name) "]");
#define FUNC_TYPE_CHECK(r, tp, sfx) FUNC_TYPE_CHECK_(BOOST_PP_TUPLE_ELEM(0, tp), BOOST_PP_TUPLE_ELEM(1, tp), BOOST_PP_TUPLE_ELEM(2, tp), sfx)
#define SFX_STR(r, data, i, sfx) BOOST_PP_COMMA_IF(i) STRINGIZE(sfx)""sv
#define QUERY_FUNC_(dst, part, name, sfxs, fall) BOOST_PP_SEQ_FOR_EACH(FUNC_TYPE_CHECK, (dst, part, name), sfxs)\
    QueryGLFunc(dst, #name, shouldPrint, std::array{ BOOST_PP_SEQ_FOR_EACH_I(SFX_STR, , sfxs) }, fall)
#define QUERY_FUNC(direct, fall, part, name, ...) QUERY_FUNC_(BOOST_PP_CAT(name, BOOST_PP_IF(direct, , _)), part, name, \
    BOOST_PP_VARIADIC_TO_SEQ(__VA_ARGS__), BOOST_PP_IF(fall, (void*)&gl##name, nullptr))
    
    // buffer related
    QUERY_FUNC(1, 0, GENBUFFERS,         GenBuffers,        , ARB);
    QUERY_FUNC(1, 0, DELETEBUFFERS,      DeleteBuffers,     , ARB);
    QUERY_FUNC(1, 0, BINDBUFFER,         BindBuffer,        , ARB);
    QUERY_FUNC(1, 0, BINDBUFFERBASE,     BindBufferBase,    , EXT, NV);
    QUERY_FUNC(1, 0, BINDBUFFERRANGE,    BindBufferRange,   , EXT, NV);
    QUERY_FUNC(0, 0, NAMEDBUFFERSTORAGE, NamedBufferStorage,, EXT);
    QUERY_FUNC(0, 0, BUFFERSTORAGE,      BufferStorage,     );
    QUERY_FUNC(0, 0, NAMEDBUFFERDATA,    NamedBufferData,   , EXT);
    QUERY_FUNC(0, 0, BUFFERDATA,         BufferData,        , ARB);
    QUERY_FUNC(0, 0, NAMEDBUFFERSUBDATA, NamedBufferSubData,, EXT);
    QUERY_FUNC(0, 0, BUFFERSUBDATA,      BufferSubData,     , ARB);
    QUERY_FUNC(0, 0, MAPNAMEDBUFFER,     MapNamedBuffer,    , EXT);
    QUERY_FUNC(0, 0, MAPBUFFER,          MapBuffer,         , ARB);
    QUERY_FUNC(0, 0, UNMAPNAMEDBUFFER,   UnmapNamedBuffer,  , EXT);
    QUERY_FUNC(0, 0, UNMAPBUFFER,        UnmapBuffer,       , ARB);

    // vao related
    QUERY_FUNC(1, 0, GENVERTEXARRAYS,         GenVertexArrays,        , APPLE);
    QUERY_FUNC(1, 0, DELETEVERTEXARRAYS,      DeleteVertexArrays,     , APPLE);
    QUERY_FUNC(0, 0, BINDVERTEXARRAY,         BindVertexArray,        , APPLE);
    QUERY_FUNC(0, 0, ENABLEVERTEXATTRIBARRAY, EnableVertexAttribArray,, ARB);
    QUERY_FUNC(0, 0, ENABLEVERTEXARRAYATTRIB, EnableVertexArrayAttrib,, EXT);
    QUERY_FUNC(0, 0, VERTEXATTRIBIPOINTER,    VertexAttribIPointer,   , EXT);
    QUERY_FUNC(0, 0, VERTEXATTRIBLPOINTER,    VertexAttribLPointer,   , EXT);
    QUERY_FUNC(0, 0, VERTEXATTRIBPOINTER,     VertexAttribPointer,    , ARB);
    QUERY_FUNC(1, 0, VERTEXATTRIBDIVISOR,     VertexAttribDivisor,    , ARB);

    // draw related
    QUERY_FUNC(1, 1, DRAWARRAYS,                                  DrawArrays,                                 , EXT);
    QUERY_FUNC(1, 1, DRAWELEMENTS,                                DrawElements,                               );
    QUERY_FUNC(1, 0, MULTIDRAWARRAYS,                             MultiDrawArrays,                            , EXT);
    QUERY_FUNC(1, 0, MULTIDRAWELEMENTS,                           MultiDrawElements,                          , EXT);
    QUERY_FUNC(0, 0, MULTIDRAWARRAYSINDIRECT,                     MultiDrawArraysIndirect,                    , AMD);
    QUERY_FUNC(0, 0, DRAWARRAYSINSTANCEDBASEINSTANCE,             DrawArraysInstancedBaseInstance,            );
    QUERY_FUNC(0, 0, DRAWARRAYSINSTANCED,                         DrawArraysInstanced,                        , ARB, EXT);
    QUERY_FUNC(0, 0, MULTIDRAWELEMENTSINDIRECT,                   MultiDrawElementsIndirect,                  , AMD);
    QUERY_FUNC(0, 0, DRAWELEMENTSINSTANCEDBASEVERTEXBASEINSTANCE, DrawElementsInstancedBaseVertexBaseInstance,);
    QUERY_FUNC(0, 0, DRAWELEMENTSINSTANCEDBASEINSTANCE,           DrawElementsInstancedBaseInstance,          );
    QUERY_FUNC(0, 0, DRAWELEMENTSINSTANCED,                       DrawElementsInstanced,                      , ARB, EXT);

    //texture related
    QUERY_FUNC(1, 1, GENTEXTURES,                    GenTextures,                   );
    QUERY_FUNC(0, 0, CREATETEXTURES,                 CreateTextures,                );
    QUERY_FUNC(1, 1, DELETETEXTURES,                 DeleteTextures,                );
    QUERY_FUNC(1, 0, ACTIVETEXTURE,                  ActiveTexture,                 , ARB);
    QUERY_FUNC(1, 1, BINDTEXTURE,                    BindTexture,                   );
    QUERY_FUNC(0, 0, BINDTEXTUREUNIT,                BindTextureUnit,               );
    QUERY_FUNC(0, 0, BINDMULTITEXTUREEXT,            BindMultiTextureEXT,           );
    QUERY_FUNC(1, 0, BINDIMAGETEXTURE,               BindImageTexture,              , EXT);
    QUERY_FUNC(1, 0, TEXTUREVIEW,                    TextureView,                   );
    QUERY_FUNC(0, 0, TEXTUREBUFFER,                  TextureBuffer,                 );
    QUERY_FUNC(0, 0, TEXTUREBUFFEREXT,               TextureBufferEXT,              );
    QUERY_FUNC(0, 0, TEXBUFFER,                      TexBuffer,                     , ARB, EXT);
    QUERY_FUNC(0, 0, GENERATETEXTUREMIPMAP,          GenerateTextureMipmap,         );
    QUERY_FUNC(0, 0, GENERATETEXTUREMIPMAPEXT,       GenerateTextureMipmapEXT,      );
    QUERY_FUNC(0, 0, GENERATEMIPMAP,                 GenerateMipmap,                , EXT);
    QUERY_FUNC(1, 0, GETTEXTUREHANDLE,               GetTextureHandle               , ARB, NV);
    QUERY_FUNC(1, 0, MAKETEXTUREHANDLERESIDENT,      MakeTextureHandleResident      , ARB, NV);
    QUERY_FUNC(1, 0, MAKETEXTUREHANDLENONRESIDENT,   MakeTextureHandleNonResident   , ARB, NV);
    QUERY_FUNC(1, 0, GETIMAGEHANDLE,                 GetImageHandle                 , ARB, NV);
    QUERY_FUNC(1, 0, MAKEIMAGEHANDLERESIDENT,        MakeImageHandleResident        , ARB, NV);
    QUERY_FUNC(1, 0, MAKEIMAGEHANDLENONRESIDENT,     MakeImageHandleNonResident     , ARB, NV);
    QUERY_FUNC(0, 0, TEXTUREPARAMETERI,              TextureParameteri,             );
    QUERY_FUNC(0, 0, TEXTUREPARAMETERIEXT,           TextureParameteriEXT,          );
    QUERY_FUNC(0, 0, TEXTURESUBIMAGE1D,              TextureSubImage1D,             );
    QUERY_FUNC(0, 0, TEXTURESUBIMAGE2D,              TextureSubImage2D,             );
    QUERY_FUNC(0, 0, TEXTURESUBIMAGE3D,              TextureSubImage3D,             );
    QUERY_FUNC(0, 0, TEXTURESUBIMAGE1DEXT,           TextureSubImage1DEXT,          );
    QUERY_FUNC(0, 0, TEXTURESUBIMAGE2DEXT,           TextureSubImage2DEXT,          );
    QUERY_FUNC(0, 0, TEXTURESUBIMAGE3DEXT,           TextureSubImage3DEXT,          );
    QUERY_FUNC(0, 0, TEXSUBIMAGE3D,                  TexSubImage3D,                 , EXT);
    QUERY_FUNC(0, 0, TEXTUREIMAGE1DEXT,              TextureImage1DEXT,             );
    QUERY_FUNC(0, 0, TEXTUREIMAGE2DEXT,              TextureImage2DEXT,             );
    QUERY_FUNC(0, 0, TEXTUREIMAGE3DEXT,              TextureImage3DEXT,             );
    QUERY_FUNC(0, 0, TEXIMAGE3D,                     TexImage3D,                    , EXT);
    QUERY_FUNC(0, 0, COMPRESSEDTEXTURESUBIMAGE1D,    CompressedTextureSubImage1D,   );
    QUERY_FUNC(0, 0, COMPRESSEDTEXTURESUBIMAGE2D,    CompressedTextureSubImage2D,   );
    QUERY_FUNC(0, 0, COMPRESSEDTEXTURESUBIMAGE3D,    CompressedTextureSubImage3D,   );
    QUERY_FUNC(0, 0, COMPRESSEDTEXTURESUBIMAGE1DEXT, CompressedTextureSubImage1DEXT,);
    QUERY_FUNC(0, 0, COMPRESSEDTEXTURESUBIMAGE2DEXT, CompressedTextureSubImage2DEXT,);
    QUERY_FUNC(0, 0, COMPRESSEDTEXTURESUBIMAGE3DEXT, CompressedTextureSubImage3DEXT,);
    QUERY_FUNC(0, 0, COMPRESSEDTEXSUBIMAGE1D,        CompressedTexSubImage1D,       , ARB);
    QUERY_FUNC(0, 0, COMPRESSEDTEXSUBIMAGE2D,        CompressedTexSubImage2D,       , ARB);
    QUERY_FUNC(0, 0, COMPRESSEDTEXSUBIMAGE3D,        CompressedTexSubImage3D,       , ARB);
    QUERY_FUNC(0, 0, COMPRESSEDTEXTUREIMAGE1DEXT,    CompressedTextureImage1DEXT,   );
    QUERY_FUNC(0, 0, COMPRESSEDTEXTUREIMAGE2DEXT,    CompressedTextureImage2DEXT,   );
    QUERY_FUNC(0, 0, COMPRESSEDTEXTUREIMAGE3DEXT,    CompressedTextureImage3DEXT,   );
    QUERY_FUNC(0, 0, COMPRESSEDTEXIMAGE1D,           CompressedTexImage1D,          , ARB);
    QUERY_FUNC(0, 0, COMPRESSEDTEXIMAGE2D,           CompressedTexImage2D,          , ARB);
    QUERY_FUNC(0, 0, COMPRESSEDTEXIMAGE3D,           CompressedTexImage3D,          , ARB);
    QUERY_FUNC(1, 0, COPYIMAGESUBDATA,               CopyImageSubData,              , NV);
    QUERY_FUNC(0, 0, TEXTURESTORAGE1D,               TextureStorage1D,              );
    QUERY_FUNC(0, 0, TEXTURESTORAGE2D,               TextureStorage2D,              );
    QUERY_FUNC(0, 0, TEXTURESTORAGE3D,               TextureStorage3D,              );
    QUERY_FUNC(0, 0, TEXTURESTORAGE1DEXT,            TextureStorage1DEXT,           );
    QUERY_FUNC(0, 0, TEXTURESTORAGE2DEXT,            TextureStorage2DEXT,           );
    QUERY_FUNC(0, 0, TEXTURESTORAGE3DEXT,            TextureStorage3DEXT,           );
    QUERY_FUNC(0, 0, TEXSTORAGE1D,                   TexStorage1D,                  );
    QUERY_FUNC(0, 0, TEXSTORAGE2D,                   TexStorage2D,                  );
    QUERY_FUNC(0, 0, TEXSTORAGE3D,                   TexStorage3D,                  );
    QUERY_FUNC(1, 0, CLEARTEXIMAGE,                  ClearTexImage,                 );
    QUERY_FUNC(1, 0, CLEARTEXSUBIMAGE,               ClearTexSubImage,              );
    QUERY_FUNC(0, 0, GETTEXTURELEVELPARAMETERIV,     GetTextureLevelParameteriv,    );
    QUERY_FUNC(0, 0, GETTEXTURELEVELPARAMETERIVEXT,  GetTextureLevelParameterivEXT, );
    QUERY_FUNC(0, 0, GETTEXTUREIMAGE,                GetTextureImage,               );
    QUERY_FUNC(0, 0, GETTEXTUREIMAGEEXT,             GetTextureImageEXT,            );
    QUERY_FUNC(0, 0, GETCOMPRESSEDTEXTUREIMAGE,      GetCompressedTextureImage,     );
    QUERY_FUNC(0, 0, GETCOMPRESSEDTEXTUREIMAGEEXT,   GetCompressedTextureImageEXT,  );
    QUERY_FUNC(0, 0, GETCOMPRESSEDTEXIMAGE,          GetCompressedTexImage,         , ARB);

    //rbo related
    QUERY_FUNC(0, 0, GENRENDERBUFFERS,                               GenRenderbuffers,                              , EXT);
    QUERY_FUNC(0, 0, CREATERENDERBUFFERS,                            CreateRenderbuffers,                           );
    QUERY_FUNC(1, 0, DELETERENDERBUFFERS,                            DeleteRenderbuffers,                           , EXT);
    QUERY_FUNC(0, 0, BINDRENDERBUFFER,                               BindRenderbuffer,                              , EXT);
    QUERY_FUNC(0, 0, NAMEDRENDERBUFFERSTORAGE,                       NamedRenderbufferStorage,                      , EXT);
    QUERY_FUNC(0, 0, RENDERBUFFERSTORAGE,                            RenderbufferStorage,                           , EXT);
    QUERY_FUNC(0, 0, NAMEDRENDERBUFFERSTORAGEMULTISAMPLE,            NamedRenderbufferStorageMultisample,           , EXT);
    QUERY_FUNC(0, 0, RENDERBUFFERSTORAGEMULTISAMPLE,                 RenderbufferStorageMultisample,                , EXT);
    QUERY_FUNC(0, 0, NAMEDRENDERBUFFERSTORAGEMULTISAMPLECOVERAGEEXT, NamedRenderbufferStorageMultisampleCoverageEXT,);
    QUERY_FUNC(0, 0, RENDERBUFFERSTORAGEMULTISAMPLECOVERAGENV,       RenderbufferStorageMultisampleCoverageNV,      );
    QUERY_FUNC(0, 0, GETRENDERBUFFERPARAMETERIV,                     GetRenderbufferParameteriv,                    , EXT);
    
    //fbo related
    QUERY_FUNC(0, 0, GENFRAMEBUFFERS,                          GenFramebuffers,                         , EXT);
    QUERY_FUNC(0, 0, CREATEFRAMEBUFFERS,                       CreateFramebuffers,                      );
    QUERY_FUNC(1, 0, DELETEFRAMEBUFFERS,                       DeleteFramebuffers,                      , EXT);
    QUERY_FUNC(0, 0, BINDFRAMEBUFFER,                          BindFramebuffer,                         , EXT);
    QUERY_FUNC(0, 0, BLITNAMEDFRAMEBUFFER,                     BlitNamedFramebuffer,                    );
    QUERY_FUNC(0, 0, BLITFRAMEBUFFER,                          BlitFramebuffer,                         , EXT);
    QUERY_FUNC(1, 0, DRAWBUFFERS,                              DrawBuffers,                             , ARB, ATI);
    QUERY_FUNC(0, 0, INVALIDATENAMEDFRAMEBUFFERDATA,           InvalidateNamedFramebufferData,          );
    QUERY_FUNC(0, 0, INVALIDATEFRAMEBUFFER,                    InvalidateFramebuffer,                   );
    //QUERY_FUC(0, 0, DISCARDFRAMEBUFFEREXT,                     DiscardFramebufferEXT,                   );
    QUERY_FUNC(0, 0, NAMEDFRAMEBUFFERRENDERBUFFER,             NamedFramebufferRenderbuffer,            , EXT);
    QUERY_FUNC(0, 0, FRAMEBUFFERRENDERBUFFER,                  FramebufferRenderbuffer,                 , EXT);
    QUERY_FUNC(0, 0, NAMEDFRAMEBUFFERTEXTURE1DEXT,             NamedFramebufferTexture1DEXT,            );
    QUERY_FUNC(0, 0, FRAMEBUFFERTEXTURE1D,                     FramebufferTexture1D,                    , EXT);
    QUERY_FUNC(0, 0, NAMEDFRAMEBUFFERTEXTURE2DEXT,             NamedFramebufferTexture2DEXT,            );
    QUERY_FUNC(0, 0, FRAMEBUFFERTEXTURE2D,                     FramebufferTexture2D,                    , EXT);
    QUERY_FUNC(0, 0, NAMEDFRAMEBUFFERTEXTURE3DEXT,             NamedFramebufferTexture3DEXT,            );
    QUERY_FUNC(0, 0, FRAMEBUFFERTEXTURE3D,                     FramebufferTexture3D,                    , EXT);
    QUERY_FUNC(0, 0, NAMEDFRAMEBUFFERTEXTURE,                  NamedFramebufferTexture,                 , EXT);
    QUERY_FUNC(0, 0, FRAMEBUFFERTEXTURE,                       FramebufferTexture,                      , EXT);
    QUERY_FUNC(0, 0, NAMEDFRAMEBUFFERTEXTURELAYER,             NamedFramebufferTextureLayer,            , EXT);
    QUERY_FUNC(0, 0, FRAMEBUFFERTEXTURELAYER,                  FramebufferTextureLayer,                 , EXT);
    QUERY_FUNC(0, 0, CHECKNAMEDFRAMEBUFFERSTATUS,              CheckNamedFramebufferStatus,             , EXT);
    QUERY_FUNC(0, 0, CHECKFRAMEBUFFERSTATUS,                   CheckFramebufferStatus,                  , EXT);
    QUERY_FUNC(0, 0, GETNAMEDFRAMEBUFFERATTACHMENTPARAMETERIV, GetNamedFramebufferAttachmentParameteriv,, EXT);
    QUERY_FUNC(0, 0, GETFRAMEBUFFERATTACHMENTPARAMETERIV,      GetFramebufferAttachmentParameteriv,     , EXT);
    QUERY_FUNC(1, 1, CLEAR,                                    Clear,                                   );
    QUERY_FUNC(1, 1, CLEARCOLOR,                               ClearColor,                              );
    QUERY_FUNC(0, 1, CLEARDEPTH,                               ClearDepth,                              );
    QUERY_FUNC(0, 0, CLEARDEPTHF,                              ClearDepthf,                             );
    QUERY_FUNC(1, 1, CLEARSTENCIL,                             ClearStencil,                            );
    QUERY_FUNC(0, 0, CLEARNAMEDFRAMEBUFFERIV,                  ClearNamedFramebufferiv,                 );
    QUERY_FUNC(0, 0, CLEARBUFFERIV,                            ClearBufferiv,                           );
    QUERY_FUNC(0, 0, CLEARNAMEDFRAMEBUFFERUIV,                 ClearNamedFramebufferuiv,                );
    QUERY_FUNC(0, 0, CLEARBUFFERUIV,                           ClearBufferuiv,                          );
    QUERY_FUNC(0, 0, CLEARNAMEDFRAMEBUFFERFV,                  ClearNamedFramebufferfv,                 );
    QUERY_FUNC(0, 0, CLEARBUFFERFV,                            ClearBufferfv,                           );
    QUERY_FUNC(0, 0, CLEARNAMEDFRAMEBUFFERFI,                  ClearNamedFramebufferfi,                 );
    QUERY_FUNC(0, 0, CLEARBUFFERFI,                            ClearBufferfi,                           );

    //shader related
    QUERY_FUNC(1, 0, CREATESHADER,     CreateShader,    );
    QUERY_FUNC(1, 0, DELETESHADER,     DeleteShader,    );
    QUERY_FUNC(1, 0, SHADERSOURCE,     ShaderSource,    );
    QUERY_FUNC(1, 0, COMPILESHADER,    CompileShader,   );
    QUERY_FUNC(1, 0, GETSHADERINFOLOG, GetShaderInfoLog,);
    QUERY_FUNC(1, 0, GETSHADERSOURCE,  GetShaderSource, );
    QUERY_FUNC(1, 0, GETSHADERIV,      GetShaderiv,     );

    //program related
    QUERY_FUNC(1, 0, CREATEPROGRAM,           CreateProgram,          );
    QUERY_FUNC(1, 0, DELETEPROGRAM,           DeleteProgram,          );
    QUERY_FUNC(1, 0, ATTACHSHADER,            AttachShader,           );
    QUERY_FUNC(1, 0, DETACHSHADER,            DetachShader,           );
    QUERY_FUNC(1, 0, LINKPROGRAM,             LinkProgram,            );
    QUERY_FUNC(1, 0, USEPROGRAM,              UseProgram,             );
    QUERY_FUNC(1, 0, DISPATCHCOMPUTE,         DispatchCompute,        );
    QUERY_FUNC(1, 0, DISPATCHCOMPUTEINDIRECT, DispatchComputeIndirect,); 

    QUERY_FUNC(0, 0, UNIFORM1F,        Uniform1f,       );
    QUERY_FUNC(0, 0, UNIFORM1FV,       Uniform1fv,      );
    QUERY_FUNC(0, 0, UNIFORM1I,        Uniform1i,       );
    QUERY_FUNC(0, 0, UNIFORM1IV,       Uniform1iv,      );
    QUERY_FUNC(0, 0, UNIFORM2F,        Uniform2f,       );
    QUERY_FUNC(0, 0, UNIFORM2FV,       Uniform2fv,      );
    QUERY_FUNC(0, 0, UNIFORM2I,        Uniform2i,       );
    QUERY_FUNC(0, 0, UNIFORM2IV,       Uniform2iv,      );
    QUERY_FUNC(0, 0, UNIFORM3F,        Uniform3f,       );
    QUERY_FUNC(0, 0, UNIFORM3FV,       Uniform3fv,      );
    QUERY_FUNC(0, 0, UNIFORM3I,        Uniform3i,       );
    QUERY_FUNC(0, 0, UNIFORM3IV,       Uniform3iv,      );
    QUERY_FUNC(0, 0, UNIFORM4F,        Uniform4f,       );
    QUERY_FUNC(0, 0, UNIFORM4FV,       Uniform4fv,      );
    QUERY_FUNC(0, 0, UNIFORM4I,        Uniform4i,       );
    QUERY_FUNC(0, 0, UNIFORM4IV,       Uniform4iv,      );
    QUERY_FUNC(0, 0, UNIFORMMATRIX2FV, UniformMatrix2fv,);
    QUERY_FUNC(0, 0, UNIFORMMATRIX3FV, UniformMatrix3fv,);
    QUERY_FUNC(0, 0, UNIFORMMATRIX4FV, UniformMatrix4fv,);
    
    QUERY_FUNC(1, 0, PROGRAMUNIFORM1D,          ProgramUniform1d,         );
    QUERY_FUNC(1, 0, PROGRAMUNIFORM1DV,         ProgramUniform1dv,        );
    QUERY_FUNC(1, 0, PROGRAMUNIFORM1F,          ProgramUniform1f,         , EXT);
    QUERY_FUNC(1, 0, PROGRAMUNIFORM1FV,         ProgramUniform1fv,        , EXT);
    QUERY_FUNC(1, 0, PROGRAMUNIFORM1I,          ProgramUniform1i,         , EXT);
    QUERY_FUNC(1, 0, PROGRAMUNIFORM1IV,         ProgramUniform1iv,        , EXT);
    QUERY_FUNC(1, 0, PROGRAMUNIFORM1UI,         ProgramUniform1ui,        , EXT);
    QUERY_FUNC(1, 0, PROGRAMUNIFORM1UIV,        ProgramUniform1uiv,       , EXT);
    QUERY_FUNC(1, 0, PROGRAMUNIFORM2D,          ProgramUniform2d,         );
    QUERY_FUNC(1, 0, PROGRAMUNIFORM2DV,         ProgramUniform2dv,        );
    QUERY_FUNC(1, 0, PROGRAMUNIFORM2F,          ProgramUniform2f,         , EXT);
    QUERY_FUNC(1, 0, PROGRAMUNIFORM2FV,         ProgramUniform2fv,        , EXT);
    QUERY_FUNC(1, 0, PROGRAMUNIFORM2I,          ProgramUniform2i,         , EXT);
    QUERY_FUNC(1, 0, PROGRAMUNIFORM2IV,         ProgramUniform2iv,        , EXT);
    QUERY_FUNC(1, 0, PROGRAMUNIFORM2UI,         ProgramUniform2ui,        , EXT);
    QUERY_FUNC(1, 0, PROGRAMUNIFORM2UIV,        ProgramUniform2uiv,       , EXT);
    QUERY_FUNC(1, 0, PROGRAMUNIFORM3D,          ProgramUniform3d,         );
    QUERY_FUNC(1, 0, PROGRAMUNIFORM3DV,         ProgramUniform3dv,        );
    QUERY_FUNC(1, 0, PROGRAMUNIFORM3F,          ProgramUniform3f,         , EXT);
    QUERY_FUNC(1, 0, PROGRAMUNIFORM3FV,         ProgramUniform3fv,        , EXT);
    QUERY_FUNC(1, 0, PROGRAMUNIFORM3I,          ProgramUniform3i,         , EXT);
    QUERY_FUNC(1, 0, PROGRAMUNIFORM3IV,         ProgramUniform3iv,        , EXT);
    QUERY_FUNC(1, 0, PROGRAMUNIFORM3UI,         ProgramUniform3ui,        , EXT);
    QUERY_FUNC(1, 0, PROGRAMUNIFORM3UIV,        ProgramUniform3uiv,       , EXT);
    QUERY_FUNC(1, 0, PROGRAMUNIFORM4D,          ProgramUniform4d,         );
    QUERY_FUNC(1, 0, PROGRAMUNIFORM4DV,         ProgramUniform4dv,        );
    QUERY_FUNC(1, 0, PROGRAMUNIFORM4F,          ProgramUniform4f,         , EXT);
    QUERY_FUNC(1, 0, PROGRAMUNIFORM4FV,         ProgramUniform4fv,        , EXT);
    QUERY_FUNC(1, 0, PROGRAMUNIFORM4I,          ProgramUniform4i,         , EXT);
    QUERY_FUNC(1, 0, PROGRAMUNIFORM4IV,         ProgramUniform4iv,        , EXT);
    QUERY_FUNC(1, 0, PROGRAMUNIFORM4UI,         ProgramUniform4ui,        , EXT);
    QUERY_FUNC(1, 0, PROGRAMUNIFORM4UIV,        ProgramUniform4uiv,       , EXT);
    QUERY_FUNC(1, 0, PROGRAMUNIFORMMATRIX2DV,   ProgramUniformMatrix2dv,  );
    QUERY_FUNC(1, 0, PROGRAMUNIFORMMATRIX2FV,   ProgramUniformMatrix2fv,  , EXT);
    QUERY_FUNC(1, 0, PROGRAMUNIFORMMATRIX2X3DV, ProgramUniformMatrix2x3dv,);
    QUERY_FUNC(1, 0, PROGRAMUNIFORMMATRIX2X3FV, ProgramUniformMatrix2x3fv,, EXT);
    QUERY_FUNC(1, 0, PROGRAMUNIFORMMATRIX2X4DV, ProgramUniformMatrix2x4dv,);
    QUERY_FUNC(1, 0, PROGRAMUNIFORMMATRIX2X4FV, ProgramUniformMatrix2x4fv,, EXT);
    QUERY_FUNC(1, 0, PROGRAMUNIFORMMATRIX3DV,   ProgramUniformMatrix3dv,  );
    QUERY_FUNC(1, 0, PROGRAMUNIFORMMATRIX3FV,   ProgramUniformMatrix3fv,  , EXT);
    QUERY_FUNC(1, 0, PROGRAMUNIFORMMATRIX3X2DV, ProgramUniformMatrix3x2dv,);
    QUERY_FUNC(1, 0, PROGRAMUNIFORMMATRIX3X2FV, ProgramUniformMatrix3x2fv,, EXT);
    QUERY_FUNC(1, 0, PROGRAMUNIFORMMATRIX3X4DV, ProgramUniformMatrix3x4dv,);
    QUERY_FUNC(1, 0, PROGRAMUNIFORMMATRIX3X4FV, ProgramUniformMatrix3x4fv,, EXT);
    QUERY_FUNC(1, 0, PROGRAMUNIFORMMATRIX4DV,   ProgramUniformMatrix4dv,  );
    QUERY_FUNC(1, 0, PROGRAMUNIFORMMATRIX4FV,   ProgramUniformMatrix4fv,  , EXT);
    QUERY_FUNC(1, 0, PROGRAMUNIFORMMATRIX4X2DV, ProgramUniformMatrix4x2dv,);
    QUERY_FUNC(1, 0, PROGRAMUNIFORMMATRIX4X2FV, ProgramUniformMatrix4x2fv,, EXT);
    QUERY_FUNC(1, 0, PROGRAMUNIFORMMATRIX4X3DV, ProgramUniformMatrix4x3dv,);
    QUERY_FUNC(1, 0, PROGRAMUNIFORMMATRIX4X3FV, ProgramUniformMatrix4x3fv,, EXT);
    QUERY_FUNC(1, 0, PROGRAMUNIFORMHANDLEUI64,  ProgramUniformHandleui64  , ARB, NV);

    QUERY_FUNC(1, 0, GETUNIFORMFV,                    GetUniformfv,                   );
    QUERY_FUNC(1, 0, GETUNIFORMIV,                    GetUniformiv,                   );
    QUERY_FUNC(1, 0, GETUNIFORMUIV,                   GetUniformuiv,                  );
    QUERY_FUNC(1, 0, GETPROGRAMINFOLOG,               GetProgramInfoLog,              );
    QUERY_FUNC(1, 0, GETPROGRAMIV,                    GetProgramiv,                   );
    QUERY_FUNC(1, 0, GETPROGRAMINTERFACEIV,           GetProgramInterfaceiv,          );
    QUERY_FUNC(1, 0, GETPROGRAMRESOURCEINDEX,         GetProgramResourceIndex,        );
    QUERY_FUNC(1, 0, GETPROGRAMRESOURCELOCATION,      GetProgramResourceLocation,     );
    QUERY_FUNC(1, 0, GETPROGRAMRESOURCELOCATIONINDEX, GetProgramResourceLocationIndex,); 
    QUERY_FUNC(1, 0, GETPROGRAMRESOURCENAME,          GetProgramResourceName,         );
    QUERY_FUNC(1, 0, GETPROGRAMRESOURCEIV,            GetProgramResourceiv,           );
    QUERY_FUNC(1, 0, GETACTIVESUBROUTINENAME,         GetActiveSubroutineName,        );
    QUERY_FUNC(1, 0, GETACTIVESUBROUTINEUNIFORMNAME,  GetActiveSubroutineUniformName, );
    QUERY_FUNC(1, 0, GETACTIVESUBROUTINEUNIFORMIV,    GetActiveSubroutineUniformiv,   );
    QUERY_FUNC(1, 0, GETPROGRAMSTAGEIV,               GetProgramStageiv,              );
    QUERY_FUNC(1, 0, GETSUBROUTINEINDEX,              GetSubroutineIndex,             );
    QUERY_FUNC(1, 0, GETSUBROUTINEUNIFORMLOCATION,    GetSubroutineUniformLocation,   );
    QUERY_FUNC(1, 0, GETUNIFORMSUBROUTINEUIV,         GetUniformSubroutineuiv,        );
    QUERY_FUNC(1, 0, UNIFORMSUBROUTINESUIV,           UniformSubroutinesuiv,          );
    QUERY_FUNC(1, 0, GETACTIVEUNIFORMBLOCKNAME,       GetActiveUniformBlockName,      );
    QUERY_FUNC(1, 0, GETACTIVEUNIFORMBLOCKIV,         GetActiveUniformBlockiv,        );
    QUERY_FUNC(1, 0, GETACTIVEUNIFORMNAME,            GetActiveUniformName,           );
    QUERY_FUNC(1, 0, GETACTIVEUNIFORMSIV,             GetActiveUniformsiv,            );
    QUERY_FUNC(1, 0, GETINTEGERI_V,                   GetIntegeri_v,                  );
    QUERY_FUNC(1, 0, GETUNIFORMBLOCKINDEX,            GetUniformBlockIndex,           );
    QUERY_FUNC(1, 0, GETUNIFORMINDICES,               GetUniformIndices,              );
    QUERY_FUNC(1, 0, UNIFORMBLOCKBINDING,             UniformBlockBinding,            );

    //query related
    QUERY_FUNC(1, 0, GENQUERIES,          GenQueries,         );
    QUERY_FUNC(1, 0, DELETEQUERIES,       DeleteQueries,      );
    QUERY_FUNC(1, 0, BEGINQUERY,          BeginQuery,         );
    QUERY_FUNC(1, 0, QUERYCOUNTER,        QueryCounter,       );
    QUERY_FUNC(1, 0, GETQUERYOBJECTIV,    GetQueryObjectiv,   );
    QUERY_FUNC(1, 0, GETQUERYOBJECTUIV,   GetQueryObjectuiv,  );
    QUERY_FUNC(1, 0, GETQUERYOBJECTI64V,  GetQueryObjecti64v, , EXT);
    QUERY_FUNC(1, 0, GETQUERYOBJECTUI64V, GetQueryObjectui64v,, EXT);
    QUERY_FUNC(1, 0, GETQUERYIV,          GetQueryiv,         );
    QUERY_FUNC(1, 0, FENCESYNC,           FenceSync,          );
    QUERY_FUNC(1, 0, DELETESYNC,          DeleteSync,         );
    QUERY_FUNC(1, 0, CLIENTWAITSYNC,      ClientWaitSync,     );
    QUERY_FUNC(1, 0, WAITSYNC,            WaitSync,           );
    QUERY_FUNC(1, 0, GETINTEGER64V,       GetInteger64v,      );
    QUERY_FUNC(1, 0, GETSYNCIV,           GetSynciv,          );

    //debug
    QUERY_FUNC(1, 0, DEBUGMESSAGECALLBACK,    DebugMessageCallback,   , ARB);
    QUERY_FUNC(1, 0, DEBUGMESSAGECALLBACKAMD, DebugMessageCallbackAMD,);
    QUERY_FUNC(0, 0, OBJECTLABEL,             ObjectLabel,            );
    QUERY_FUNC(0, 0, LABELOBJECTEXT,          LabelObjectEXT,         );
    QUERY_FUNC(0, 0, OBJECTPTRLABEL,          ObjectPtrLabel,         );
    QUERY_FUNC(0, 0, PUSHDEBUGGROUP,          PushDebugGroup,         );
    QUERY_FUNC(0, 0, POPDEBUGGROUP,           PopDebugGroup,          );
    QUERY_FUNC(0, 0, PUSHGROUPMARKEREXT,      PushGroupMarkerEXT,     );
    QUERY_FUNC(0, 0, POPGROUPMARKEREXT,       PopGroupMarkerEXT,      );
    QUERY_FUNC(0, 0, DEBUGMESSAGEINSERT,      DebugMessageInsert,     , ARB);
    QUERY_FUNC(1, 0, DEBUGMESSAGEINSERTAMD,   DebugMessageInsertAMD,  );
    QUERY_FUNC(1, 0, INSERTEVENTMARKEREXT,    InsertEventMarkerEXT,   );

    //others
    QUERY_FUNC(0, 1, GETERROR,          GetError,         );
    QUERY_FUNC(1, 1, GETFLOATV,         GetFloatv,        );
    QUERY_FUNC(1, 1, GETINTEGERV,       GetIntegerv,      );
    QUERY_FUNC(1, 1, GETSTRING,         GetString,        );
    QUERY_FUNC(1, 0, GETSTRINGI,        GetStringi,       );
    QUERY_FUNC(1, 1, ISENABLED,         IsEnabled,        );
    QUERY_FUNC(1, 1, ENABLE,            Enable,           );
    QUERY_FUNC(1, 1, DISABLE,           Disable,          );
    QUERY_FUNC(1, 1, FINISH,            Finish,           );
    QUERY_FUNC(1, 1, FLUSH,             Flush,            );
    QUERY_FUNC(1, 1, DEPTHFUNC,         DepthFunc,        );
    QUERY_FUNC(1, 1, CULLFACE,          CullFace,         );
    QUERY_FUNC(1, 1, FRONTFACE,         FrontFace,        );
    QUERY_FUNC(1, 1, VIEWPORT,          Viewport,         );
    QUERY_FUNC(1, 0, VIEWPORTARRAYV,    ViewportArrayv,   );
    QUERY_FUNC(1, 0, VIEWPORTINDEXEDF,  ViewportIndexedf, );
    QUERY_FUNC(1, 0, VIEWPORTINDEXEDFV, ViewportIndexedfv,);
    QUERY_FUNC(1, 0, CLIPCONTROL,       ClipControl,      );
    QUERY_FUNC(1, 0, MEMORYBARRIER,     MemoryBarrier,    , EXT);

    Extensions = GetExtensions();
    {
        VendorString = common::str::to_u16string(
            reinterpret_cast<const char*>(GetString(GL_VENDOR)), common::str::Encoding::UTF8);
        VersionString = common::str::to_u16string(
            reinterpret_cast<const char*>(GetString(GL_VERSION)), common::str::Encoding::UTF8);
        int32_t major = 0, minor = 0;
        GetIntegerv(GL_MAJOR_VERSION, &major);
        GetIntegerv(GL_MINOR_VERSION, &minor);
        Version = major * 10 + minor;
    }
    SupportDebug            = DebugMessageCallback != nullptr || DebugMessageCallbackAMD != nullptr;
    SupportSRGB             = PlatFunc->SupportSRGB && (Extensions.Has("GL_ARB_framebuffer_sRGB") || Extensions.Has("GL_EXT_framebuffer_sRGB"));
    SupportClipControl      = ClipControl != nullptr;
    SupportGeometryShader   = Version >= 33 || Extensions.Has("GL_ARB_geometry_shader4");
    SupportComputeShader    = Extensions.Has("GL_ARB_compute_shader");
    SupportTessShader       = Extensions.Has("GL_ARB_tessellation_shader");
    SupportBindlessTexture  = Extensions.Has("GL_ARB_bindless_texture") || Extensions.Has("GL_NV_bindless_texture");
    SupportImageLoadStore   = Extensions.Has("GL_ARB_shader_image_load_store") || Extensions.Has("GL_EXT_shader_image_load_store");
    SupportSubroutine       = Extensions.Has("GL_ARB_shader_subroutine");
    SupportIndirectDraw     = Extensions.Has("GL_ARB_draw_indirect");
    SupportBaseInstance     = Extensions.Has("GL_ARB_base_instance") || Extensions.Has("GL_EXT_base_instance");
    SupportVSMultiLayer     = Extensions.Has("GL_ARB_shader_viewport_layer_array") || Extensions.Has("GL_AMD_vertex_shader_layer");
    
    GetIntegerv(GL_MAX_UNIFORM_BUFFER_BINDINGS,         &MaxUBOUnits);
    GetIntegerv(GL_MAX_IMAGE_UNITS,                     &MaxImageUnits);
    GetIntegerv(GL_MAX_COLOR_ATTACHMENTS,               &MaxColorAttachment);
    GetIntegerv(GL_MAX_DRAW_BUFFERS,                    &MaxDrawBuffers);
    GetIntegerv(GL_MAX_LABEL_LENGTH,                    &MaxLabelLen);
    GetIntegerv(GL_MAX_DEBUG_MESSAGE_LENGTH,            &MaxMessageLen);
    if (SupportImageLoadStore)
        GetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &MaxTextureUnits);

#undef QUERY_FUNC
#undef QUERY_FUNC_
#undef SFX_STR
#undef FUNC_TYPE_CHECK
#undef FUNC_TYPE_CHECK_
}

#if COMMON_COMPILER_MSVC
#   pragma warning(pop)
#endif


#define CALL_EXISTS(func, ...) if (func) { return func(__VA_ARGS__); }


void CtxFuncs::NamedBufferStorage(GLenum target, GLuint buffer, GLsizeiptr size, const void* data, GLbitfield flags) const
{
    CALL_EXISTS(NamedBufferStorage_, buffer, size, data, flags)
    {
        BindBuffer(target, buffer);
        BufferStorage_(target, size, data, flags);
    }
}
void CtxFuncs::NamedBufferData(GLenum target, GLuint buffer, GLsizeiptr size, const void* data, GLenum usage) const
{
    CALL_EXISTS(NamedBufferData_, buffer, size, data, usage)
    {
        BindBuffer(target, buffer);
        BufferData_(target, size, data, usage);
    }
}
void CtxFuncs::NamedBufferSubData(GLenum target, GLuint buffer, GLintptr offset, GLsizeiptr size, const void* data) const
{
    CALL_EXISTS(NamedBufferSubData_, buffer, offset, size, data)
    {
        BindBuffer(target, buffer);
        BufferSubData_(target, offset, size, data);
    }
}
void* CtxFuncs::MapNamedBuffer(GLenum target, GLuint buffer, GLenum access) const
{
    CALL_EXISTS(MapNamedBuffer_, buffer, access)
    {
        BindBuffer(target, buffer);
        return MapBuffer_(target, access);
    }
}
GLboolean CtxFuncs::UnmapNamedBuffer(GLenum target, GLuint buffer) const
{
    CALL_EXISTS(UnmapNamedBuffer_, buffer)
    {
        BindBuffer(target, buffer);
        return UnmapBuffer_(target);
    }
}


struct VAOBinder : public common::NonCopyable
{
    common::SpinLocker::ScopeType Lock;
    const CtxFuncs& DSA;
    bool Changed;
    VAOBinder(const CtxFuncs* dsa, GLuint newVAO) noexcept :
        Lock(dsa->DataLock.LockScope()), DSA(*dsa), Changed(false)
    {
        if (newVAO != DSA.VAO)
            Changed = true, DSA.BindVertexArray_(newVAO);
    }
    ~VAOBinder()
    {
        if (Changed)
            DSA.BindVertexArray_(DSA.VAO);
    }
};
void CtxFuncs::RefreshVAOState() const
{
    const auto lock = DataLock.LockScope();
    GetIntegerv(GL_VERTEX_ARRAY_BINDING, reinterpret_cast<GLint*>(&VAO));
}
void CtxFuncs::BindVertexArray(GLuint vaobj) const
{
    const auto lock = DataLock.LockScope();
    BindVertexArray_(vaobj);
    VAO = vaobj;
}
void CtxFuncs::EnableVertexArrayAttrib(GLuint vaobj, GLuint index) const
{
    CALL_EXISTS(EnableVertexArrayAttrib_, vaobj, index)
    {
        BindVertexArray_(vaobj); // ensure be in binding
        EnableVertexAttribArray_(index);
    }
}
void CtxFuncs::VertexAttribPointer(GLuint index, GLint size, GLenum type, bool normalized, GLsizei stride, size_t offset) const
{
    const auto pointer = reinterpret_cast<const void*>(uintptr_t(offset));
    VertexAttribPointer_(index, size, type, normalized ? GL_TRUE : GL_FALSE, stride, pointer);
}
void CtxFuncs::VertexAttribIPointer(GLuint index, GLint size, GLenum type, GLsizei stride, size_t offset) const
{
    const auto pointer = reinterpret_cast<const void*>(uintptr_t(offset));
    VertexAttribIPointer_(index, size, type, stride, pointer);
}
void CtxFuncs::VertexAttribLPointer(GLuint index, GLint size, GLenum type, GLsizei stride, size_t offset) const
{
    const auto pointer = reinterpret_cast<const void*>(uintptr_t(offset));
    VertexAttribLPointer_(index, size, type, stride, pointer);
}


void CtxFuncs::DrawArraysInstanced(GLenum mode, GLint first, GLsizei count, uint32_t instancecount, uint32_t baseinstance) const
{
    if (baseinstance != 0)
        CALL_EXISTS(DrawArraysInstancedBaseInstance_, mode, first, count, static_cast<GLsizei>(instancecount), baseinstance)
    CALL_EXISTS(DrawArraysInstanced_, mode, first, count, static_cast<GLsizei>(instancecount)) // baseInstance ignored
    {
        for (uint32_t i = 0; i < instancecount; i++)
        {
            DrawArrays(mode, first, count);
        }
    }
}
void CtxFuncs::DrawElementsInstanced(GLenum mode, GLsizei count, GLenum type, const void* indices, uint32_t instancecount, uint32_t baseinstance) const
{
    if (baseinstance != 0)
        CALL_EXISTS(DrawElementsInstancedBaseInstance_, mode, count, type, indices, static_cast<GLsizei>(instancecount), baseinstance)
    CALL_EXISTS(DrawElementsInstanced_, mode, count, type, indices, static_cast<GLsizei>(instancecount)) // baseInstance ignored
    {
        for (uint32_t i = 0; i < instancecount; i++)
        {
            DrawElements(mode, count, type, indices);
        }
    }
}
void CtxFuncs::MultiDrawArraysIndirect(GLenum mode, const oglIndirectBuffer_& indirect, GLint offset, GLsizei drawcount) const
{
    if (MultiDrawArraysIndirect_)
    {
        BindBuffer(GL_DRAW_INDIRECT_BUFFER, indirect.BufferID); //IBO not included in VAO
        const auto pointer = reinterpret_cast<const void*>(uintptr_t(sizeof(oglIndirectBuffer_::DrawArraysIndirectCommand) * offset));
        MultiDrawArraysIndirect_(mode, pointer, drawcount, 0);
    }
    else if (DrawArraysInstancedBaseInstance_)
    {
        const auto& cmd = indirect.GetArrayCommands();
        for (GLsizei i = offset; i < drawcount; i++)
        {
            DrawArraysInstancedBaseInstance_(mode, cmd[i].first, cmd[i].count, cmd[i].instanceCount, cmd[i].baseInstance);
        }
    }
    else if (DrawArraysInstanced_)
    {
        const auto& cmd = indirect.GetArrayCommands();
        for (GLsizei i = offset; i < drawcount; i++)
        {
            DrawArraysInstanced_(mode, cmd[i].first, cmd[i].count, cmd[i].instanceCount); // baseInstance ignored
        }
    }
    else
    {
        COMMON_THROWEX(OGLException, OGLException::GLComponent::OGLU, u"no avaliable implementation for MultiDrawArraysIndirect");
    }
}
void CtxFuncs::MultiDrawElementsIndirect(GLenum mode, GLenum type, const oglIndirectBuffer_& indirect, GLint offset, GLsizei drawcount) const
{
    if (MultiDrawElementsIndirect_)
    {
        BindBuffer(GL_DRAW_INDIRECT_BUFFER, indirect.BufferID); //IBO not included in VAO
        const auto pointer = reinterpret_cast<const void*>(uintptr_t(sizeof(oglIndirectBuffer_::DrawArraysIndirectCommand) * offset));
        MultiDrawElementsIndirect_(mode, type, pointer, drawcount, 0);
    }
    else if (DrawElementsInstancedBaseVertexBaseInstance_)
    {
        const auto& cmd = indirect.GetElementCommands();
        for (GLsizei i = offset; i < drawcount; i++)
        {
            const auto pointer = reinterpret_cast<const void*>(uintptr_t(cmd[i].firstIndex));
            DrawElementsInstancedBaseVertexBaseInstance_(mode, cmd[i].count, type, pointer, cmd[i].instanceCount, cmd[i].baseVertex, cmd[i].baseInstance);
        }
    }
    else if (DrawElementsInstancedBaseInstance_)
    {
        const auto& cmd = indirect.GetElementCommands();
        for (GLsizei i = offset; i < drawcount; i++)
        {
            const auto pointer = reinterpret_cast<const void*>(uintptr_t(cmd[i].firstIndex));
            DrawElementsInstancedBaseInstance_(mode, cmd[i].count, type, pointer, cmd[i].instanceCount, cmd[i].baseInstance); // baseInstance ignored
        }
    }
    else if (DrawElementsInstanced_)
    {
        const auto& cmd = indirect.GetElementCommands();
        for (GLsizei i = offset; i < drawcount; i++)
        {
            const auto pointer = reinterpret_cast<const void*>(uintptr_t(cmd[i].firstIndex));
            DrawElementsInstanced_(mode, cmd[i].count, type, pointer, cmd[i].instanceCount); // baseInstance & baseVertex ignored
        }
    }
    else
    {
        COMMON_THROWEX(OGLException, OGLException::GLComponent::OGLU, u"no avaliable implementation for MultiDrawElementsIndirect");
    }
}


void CtxFuncs::CreateTextures(GLenum target, GLsizei n, GLuint* textures) const
{
    CALL_EXISTS(CreateTextures_, target, n, textures)
    {
        GenTextures(n, textures);
        ActiveTexture(GL_TEXTURE0);
        for (GLsizei i = 0; i < n; ++i)
            glBindTexture(target, textures[i]);
        glBindTexture(target, 0);
    }
}
void CtxFuncs::BindTextureUnit(GLuint unit, GLuint texture, GLenum target) const
{
    CALL_EXISTS(BindTextureUnit_,                   unit,         texture)
    CALL_EXISTS(BindMultiTextureEXT_, GL_TEXTURE0 + unit, target, texture)
    {
        ActiveTexture(GL_TEXTURE0 + unit);
        glBindTexture(target, texture);
    }
}
void CtxFuncs::TextureBuffer(GLuint texture, GLenum target, GLenum internalformat, GLuint buffer) const
{
    CALL_EXISTS(TextureBuffer_,    target,         internalformat, buffer)
    CALL_EXISTS(TextureBufferEXT_, target, target, internalformat, buffer)
    {
        ActiveTexture(GL_TEXTURE0);
        glBindTexture(target, texture);
        TexBuffer_(target, internalformat, buffer);
        glBindTexture(target, 0);
    }
}
void CtxFuncs::GenerateTextureMipmap(GLuint texture, GLenum target) const
{
    CALL_EXISTS(GenerateTextureMipmap_,    texture)
    CALL_EXISTS(GenerateTextureMipmapEXT_, texture, target)
    {
        ActiveTexture(GL_TEXTURE0);
        glBindTexture(target, texture);
        GenerateMipmap_(target);
    }
}
void CtxFuncs::TextureParameteri(GLuint texture, GLenum target, GLenum pname, GLint param) const
{
    CALL_EXISTS(TextureParameteri_,    texture,         pname, param)
    CALL_EXISTS(TextureParameteriEXT_, texture, target, pname, param)
    {
        glBindTexture(target, texture);
        glTexParameteri(target, pname, param);
    }
}
void CtxFuncs::TextureSubImage1D(GLuint texture, GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLenum type, const void* pixels) const
{
    CALL_EXISTS(TextureSubImage1D_,    texture,         level, xoffset, width, format, type, pixels)
    CALL_EXISTS(TextureSubImage1DEXT_, texture, target, level, xoffset, width, format, type, pixels)
    {
        ActiveTexture(GL_TEXTURE0);
        glBindTexture(target, texture);
        glTexSubImage1D(target, level, xoffset, width, format, type, pixels);
    }
}
void CtxFuncs::TextureSubImage2D(GLuint texture, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void* pixels) const
{
    CALL_EXISTS(TextureSubImage2D_,    texture,         level, xoffset, yoffset, width, height, format, type, pixels)
    CALL_EXISTS(TextureSubImage2DEXT_, texture, target, level, xoffset, yoffset, width, height, format, type, pixels)
    {
        ActiveTexture(GL_TEXTURE0);
        glBindTexture(target, texture);
        glTexSubImage2D(target, level, xoffset, yoffset, width, height, format, type, pixels);
    }
}
void CtxFuncs::TextureSubImage3D(GLuint texture, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const void* pixels) const
{
    CALL_EXISTS(TextureSubImage3D_,    texture,         level, xoffset, yoffset, zoffset, width, height, depth, format, type, pixels)
    CALL_EXISTS(TextureSubImage3DEXT_, texture, target, level, xoffset, yoffset, zoffset, width, height, depth, format, type, pixels)
    {
        ActiveTexture(GL_TEXTURE0);
        glBindTexture(target, texture);
        TexSubImage3D_(target, level, xoffset, yoffset, zoffset, width, height, depth, format, type, pixels);
    }
}
void CtxFuncs::TextureImage1D(GLuint texture, GLenum target, GLint level, GLint internalformat, GLsizei width, GLint border, GLenum format, GLenum type, const void* pixels) const
{
    CALL_EXISTS(TextureImage1DEXT_, texture, target, level, internalformat, width, border, format, type, pixels)
    {
        ActiveTexture(GL_TEXTURE0);
        glBindTexture(target, texture);
        glTexImage1D(target, level, internalformat, width, border, format, type, pixels);
    }
}
void CtxFuncs::TextureImage2D(GLuint texture, GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void* pixels) const
{
    CALL_EXISTS(TextureImage2DEXT_, texture, target, level, internalformat, width, height, border, format, type, pixels)
    {
        ActiveTexture(GL_TEXTURE0);
        glBindTexture(target, texture);
        glTexImage2D(target, level, internalformat, width, height, border, format, type, pixels);
    }
}
void CtxFuncs::TextureImage3D(GLuint texture, GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const void* pixels) const
{
    CALL_EXISTS(TextureImage3DEXT_, texture, target, level, internalformat, width, height, depth, border, format, type, pixels)
    {
        ActiveTexture(GL_TEXTURE0);
        glBindTexture(target, texture);
        TexImage3D_(target, level, internalformat, width, height, depth, border, format, type, pixels);
    }
}
void CtxFuncs::CompressedTextureSubImage1D(GLuint texture, GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLsizei imageSize, const void* data) const
{
    CALL_EXISTS(CompressedTextureSubImage1D_,    texture,         level, xoffset, width, format, imageSize, data)
    CALL_EXISTS(CompressedTextureSubImage1DEXT_, texture, target, level, xoffset, width, format, imageSize, data)
    {
        ActiveTexture(GL_TEXTURE0);
        glBindTexture(target, texture);
        CompressedTexSubImage1D_(target, level, xoffset, width, format, imageSize, data);
    }
}
void CtxFuncs::CompressedTextureSubImage2D(GLuint texture, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const void* data) const
{
    CALL_EXISTS(CompressedTextureSubImage2D_,    texture,         level, xoffset, yoffset, width, height, format, imageSize, data)
    CALL_EXISTS(CompressedTextureSubImage2DEXT_, texture, target, level, xoffset, yoffset, width, height, format, imageSize, data)
    {
        ActiveTexture(GL_TEXTURE0);
        glBindTexture(target, texture);
        CompressedTexSubImage2D_(target, level, xoffset, yoffset, width, height, format, imageSize, data);
    }
}
void CtxFuncs::CompressedTextureSubImage3D(GLuint texture, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLsizei imageSize, const void* data) const
{
    CALL_EXISTS(CompressedTextureSubImage3D_,    texture,         level, xoffset, yoffset, zoffset, width, height, depth, format, imageSize, data)
    CALL_EXISTS(CompressedTextureSubImage3DEXT_, texture, target, level, xoffset, yoffset, zoffset, width, height, depth, format, imageSize, data)
    {
        ActiveTexture(GL_TEXTURE0);
        glBindTexture(target, texture);
        CompressedTexSubImage3D_(target, level, xoffset, yoffset, zoffset, width, height, depth, format, imageSize, data);
    }
}
void CtxFuncs::CompressedTextureImage1D(GLuint texture, GLenum target, GLint level, GLenum internalformat, GLsizei width, GLint border, GLsizei imageSize, const void* data) const
{
    CALL_EXISTS(CompressedTextureImage1DEXT_, texture, target, level, internalformat, width, border, imageSize, data)
    {
        ActiveTexture(GL_TEXTURE0);
        glBindTexture(target, texture);
        CompressedTexImage1D_(target, level, internalformat, width, border, imageSize, data);
    }
}
void CtxFuncs::CompressedTextureImage2D(GLuint texture, GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize, const void* data) const
{
    CALL_EXISTS(CompressedTextureImage2DEXT_, texture, target, level, internalformat, width, height, border, imageSize, data)
    {
        ActiveTexture(GL_TEXTURE0);
        glBindTexture(target, texture);
        CompressedTexImage2D_(target, level, internalformat, width, height, border, imageSize, data);
    }
}
void CtxFuncs::CompressedTextureImage3D(GLuint texture, GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLsizei imageSize, const void* data) const
{
    CALL_EXISTS(CompressedTextureImage3DEXT_, texture, target, level, internalformat, width, height, depth, border, imageSize, data)
    {
        ActiveTexture(GL_TEXTURE0);
        glBindTexture(target, texture);
        CompressedTexImage3D_(target, level, internalformat, width, height, depth, border, imageSize, data);
    }
}
void CtxFuncs::TextureStorage1D(GLuint texture, GLenum target, GLsizei levels, GLenum internalformat, GLsizei width) const
{
    CALL_EXISTS(TextureStorage1D_,    texture,         levels, internalformat, width)
    CALL_EXISTS(TextureStorage1DEXT_, texture, target, levels, internalformat, width)
    {
        ActiveTexture(GL_TEXTURE0);
        glBindTexture(target, texture);
        TexStorage1D_(target, levels, internalformat, width);
    }
}
void CtxFuncs::TextureStorage2D(GLuint texture, GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height) const
{
    CALL_EXISTS(TextureStorage2D_,    texture,         levels, internalformat, width, height)
    CALL_EXISTS(TextureStorage2DEXT_, texture, target, levels, internalformat, width, height)
    {
        ActiveTexture(GL_TEXTURE0);
        glBindTexture(target, texture);
        TexStorage2D_(target, levels, internalformat, width, height);
    }
}
void CtxFuncs::TextureStorage3D(GLuint texture, GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth) const
{
    CALL_EXISTS(TextureStorage3D_,    texture,         levels, internalformat, width, height, depth)
    CALL_EXISTS(TextureStorage3DEXT_, texture, target, levels, internalformat, width, height, depth)
    {
        ActiveTexture(GL_TEXTURE0);
        glBindTexture(target, texture);
        TexStorage3D_(target, levels, internalformat, width, height, depth);
    }
}
void CtxFuncs::GetTextureLevelParameteriv(GLuint texture, GLenum target, GLint level, GLenum pname, GLint* params) const
{
    CALL_EXISTS(GetTextureLevelParameteriv_,    texture,         level, pname, params)
    CALL_EXISTS(GetTextureLevelParameterivEXT_, texture, target, level, pname, params)
    {
        glBindTexture(target, texture);
        glGetTexLevelParameteriv(target, level, pname, params);
    }
}
void CtxFuncs::GetTextureImage(GLuint texture, GLenum target, GLint level, GLenum format, GLenum type, size_t bufSize, void* pixels) const
{
    CALL_EXISTS(GetTextureImage_,    texture,         level, format, type, bufSize > INT32_MAX ? INT32_MAX : static_cast<GLsizei>(bufSize), pixels)
    CALL_EXISTS(GetTextureImageEXT_, texture, target, level, format, type, pixels)
    {
        glBindTexture(target, texture);
        glGetTexImage(target, level, format, type, pixels);
    }
}
void CtxFuncs::GetCompressedTextureImage(GLuint texture, GLenum target, GLint level, size_t bufSize, void* img) const
{
    CALL_EXISTS(GetCompressedTextureImage_,    texture,         level, bufSize > INT32_MAX ? INT32_MAX : static_cast<GLsizei>(bufSize), img)
    CALL_EXISTS(GetCompressedTextureImageEXT_, texture, target, level, img)
    {
        glBindTexture(target, texture);
        GetCompressedTexImage_(target, level, img);
    }
}


void CtxFuncs::CreateRenderbuffers(GLsizei n, GLuint* renderbuffers) const
{
    CALL_EXISTS(CreateRenderbuffers_, n, renderbuffers)
    {
        GenRenderbuffers_(n, renderbuffers);
        for (GLsizei i = 0; i < n; ++i)
            BindRenderbuffer_(GL_RENDERBUFFER, renderbuffers[i]);
    }
}
void CtxFuncs::NamedRenderbufferStorage(GLuint renderbuffer, GLenum internalformat, GLsizei width, GLsizei height) const
{
    CALL_EXISTS(NamedRenderbufferStorage_, renderbuffer, internalformat, width, height)
    {
        BindRenderbuffer_(GL_RENDERBUFFER, renderbuffer);
        RenderbufferStorage_(GL_RENDERBUFFER, internalformat, width, height);
    }
}
void CtxFuncs::NamedRenderbufferStorageMultisample(GLuint renderbuffer, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height) const
{
    CALL_EXISTS(NamedRenderbufferStorageMultisample_, renderbuffer, samples, internalformat, width, height)
    {
        BindRenderbuffer_(GL_RENDERBUFFER, renderbuffer);
        RenderbufferStorageMultisample_(GL_RENDERBUFFER, samples, internalformat, width, height);
    }
}
void CtxFuncs::NamedRenderbufferStorageMultisampleCoverage(GLuint renderbuffer, GLsizei coverageSamples, GLsizei colorSamples, GLenum internalformat, GLsizei width, GLsizei height) const
{
    CALL_EXISTS(NamedRenderbufferStorageMultisampleCoverageEXT_, renderbuffer, coverageSamples, colorSamples, internalformat, width, height)
    if (RenderbufferStorageMultisampleCoverageNV_)
    {
        BindRenderbuffer_(GL_RENDERBUFFER, renderbuffer);
        RenderbufferStorageMultisampleCoverageNV_(GL_RENDERBUFFER, coverageSamples, colorSamples, internalformat, width, height);
    }
    else
    {
        COMMON_THROWEX(OGLException, OGLException::GLComponent::OGLU, u"no avaliable implementation for RenderbufferStorageMultisampleCoverage");
    }
}


struct FBOBinder : public common::NonCopyable
{
    common::SpinLocker::ScopeType Lock;
    const CtxFuncs& DSA;
    bool ChangeRead, ChangeDraw;
    FBOBinder(const CtxFuncs* dsa) noexcept :
        Lock(dsa->DataLock.LockScope()), DSA(*dsa), ChangeRead(true), ChangeDraw(true)
    { }
    FBOBinder(const CtxFuncs* dsa, const std::pair<GLuint, GLuint> newFBO) noexcept :
        Lock(dsa->DataLock.LockScope()), DSA(*dsa), ChangeRead(false), ChangeDraw(false)
    {
        if (DSA.ReadFBO != newFBO.first)
            ChangeRead = true, DSA.BindFramebuffer_(GL_READ_FRAMEBUFFER, newFBO.first);
        if (DSA.DrawFBO != newFBO.second)
            ChangeDraw = true, DSA.BindFramebuffer_(GL_DRAW_FRAMEBUFFER, newFBO.second);
    }
    FBOBinder(const CtxFuncs* dsa, const GLenum target, const GLuint newFBO) noexcept :
        Lock(dsa->DataLock.LockScope()), DSA(*dsa), ChangeRead(false), ChangeDraw(false)
    {
        if (target == GL_READ_FRAMEBUFFER || target == GL_FRAMEBUFFER)
        {
            if (DSA.ReadFBO != newFBO)
                ChangeRead = true, DSA.BindFramebuffer_(GL_READ_FRAMEBUFFER, newFBO);
        }
        if (target == GL_DRAW_FRAMEBUFFER || target == GL_FRAMEBUFFER)
        {
            if (DSA.DrawFBO != newFBO)
                ChangeDraw = true, DSA.BindFramebuffer_(GL_DRAW_FRAMEBUFFER, newFBO);
        }
    }
    ~FBOBinder()
    {
        if (ChangeRead)
            DSA.BindFramebuffer_(GL_READ_FRAMEBUFFER, DSA.ReadFBO);
        if (ChangeDraw)
            DSA.BindFramebuffer_(GL_DRAW_FRAMEBUFFER, DSA.DrawFBO);
    }
};
void CtxFuncs::RefreshFBOState() const
{
    const auto lock = DataLock.LockScope();
    GetIntegerv(GL_READ_FRAMEBUFFER_BINDING, reinterpret_cast<GLint*>(&ReadFBO));
    GetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, reinterpret_cast<GLint*>(&DrawFBO));
}
void CtxFuncs::CreateFramebuffers(GLsizei n, GLuint* framebuffers) const
{
    CALL_EXISTS(CreateFramebuffers_, n, framebuffers)
    {
        GenFramebuffers_(n, framebuffers);
        const auto backup = FBOBinder(this);
        for (GLsizei i = 0; i < n; ++i)
            BindFramebuffer_(GL_READ_FRAMEBUFFER, framebuffers[i]);
    }
}
void CtxFuncs::BindFramebuffer(GLenum target, GLuint framebuffer) const
{
    const auto lock = DataLock.LockScope();
    BindFramebuffer_(target, framebuffer);
    if (target == GL_READ_FRAMEBUFFER || target == GL_FRAMEBUFFER)
        ReadFBO = framebuffer;
    if (target == GL_DRAW_FRAMEBUFFER || target == GL_FRAMEBUFFER)
        DrawFBO = framebuffer;
}
void CtxFuncs::BlitNamedFramebuffer(GLuint readFramebuffer, GLuint drawFramebuffer, GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter) const
{
    CALL_EXISTS(BlitNamedFramebuffer_, readFramebuffer, drawFramebuffer, srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter)
    {
        const auto backup = FBOBinder(this, { readFramebuffer, drawFramebuffer });
        BlitFramebuffer_(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
    }
}
void CtxFuncs::InvalidateNamedFramebufferData(GLuint framebuffer, GLsizei numAttachments, const GLenum* attachments) const
{
    CALL_EXISTS(InvalidateNamedFramebufferData_, framebuffer, numAttachments, attachments)
    const auto invalidator = InvalidateFramebuffer_ ? InvalidateFramebuffer_ : DiscardFramebufferEXT_;
    if (invalidator)
    {
        const auto backup = FBOBinder(this, GL_DRAW_FRAMEBUFFER, framebuffer);
        invalidator(GL_DRAW_FRAMEBUFFER, numAttachments, attachments);
    }
}
void CtxFuncs::NamedFramebufferRenderbuffer(GLuint framebuffer, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer) const
{
    CALL_EXISTS(NamedFramebufferRenderbuffer_, framebuffer, attachment, renderbuffertarget, renderbuffer)
    {
        const auto backup = FBOBinder(this, GL_DRAW_FRAMEBUFFER, framebuffer);
        FramebufferRenderbuffer_(GL_DRAW_FRAMEBUFFER, attachment, renderbuffertarget, renderbuffer);
    }
}
void CtxFuncs::NamedFramebufferTexture(GLuint framebuffer, GLenum attachment, GLenum textarget, GLuint texture, GLint level) const
{
    CALL_EXISTS(NamedFramebufferTexture_, framebuffer, attachment, texture, level)
    if (FramebufferTexture_)
    {
        const auto backup = FBOBinder(this, GL_DRAW_FRAMEBUFFER, framebuffer);
        FramebufferTexture_(GL_DRAW_FRAMEBUFFER, attachment, texture, level);
    }
    else
    {
        switch (textarget)
        {
        case GL_TEXTURE_1D:
            CALL_EXISTS(NamedFramebufferTexture1DEXT_, framebuffer, attachment, textarget, texture, level)
            {
                const auto backup = FBOBinder(this, GL_DRAW_FRAMEBUFFER, framebuffer);
                FramebufferTexture1D_(GL_DRAW_FRAMEBUFFER, attachment, textarget, texture, level);
            }
            break;
        case GL_TEXTURE_2D:
            CALL_EXISTS(NamedFramebufferTexture2DEXT_, framebuffer, attachment, textarget, texture, level)
            {
                const auto backup = FBOBinder(this, GL_DRAW_FRAMEBUFFER, framebuffer);
                FramebufferTexture2D_(GL_DRAW_FRAMEBUFFER, attachment, textarget, texture, level);
            }
            break;
        default:
            COMMON_THROWEX(OGLException, OGLException::GLComponent::OGLU, u"unsupported textarget with calling FramebufferTexture");
        }
    }
}
void CtxFuncs::NamedFramebufferTextureLayer(GLuint framebuffer, GLenum attachment, GLenum textarget, GLuint texture, GLint level, GLint layer) const
{
    CALL_EXISTS(NamedFramebufferTextureLayer_, framebuffer, attachment, texture, level, layer)
    if (FramebufferTextureLayer_)
    {
        const auto backup = FBOBinder(this, GL_DRAW_FRAMEBUFFER, framebuffer);
        FramebufferTextureLayer_(GL_DRAW_FRAMEBUFFER, attachment, texture, level, layer);
    }
    else
    {
        switch (textarget)
        {
        case GL_TEXTURE_3D:
            CALL_EXISTS(NamedFramebufferTexture3DEXT_, framebuffer, attachment, textarget, texture, level, layer)
            {
                const auto backup = FBOBinder(this, GL_DRAW_FRAMEBUFFER, framebuffer);
                FramebufferTexture3D_(GL_DRAW_FRAMEBUFFER, attachment, textarget, texture, level, layer);
            }
            break;
        default:
            COMMON_THROWEX(OGLException, OGLException::GLComponent::OGLU, u"unsupported textarget with calling FramebufferTextureLayer");
        }
    }
}
GLenum CtxFuncs::CheckNamedFramebufferStatus(GLuint framebuffer, GLenum target) const
{
    CALL_EXISTS(CheckNamedFramebufferStatus_, framebuffer, target)
    {
        const auto backup = FBOBinder(this, target, framebuffer);
        return CheckFramebufferStatus_(target);
    }
}
void CtxFuncs::GetNamedFramebufferAttachmentParameteriv(GLuint framebuffer, GLenum attachment, GLenum pname, GLint* params) const
{
    CALL_EXISTS(GetNamedFramebufferAttachmentParameteriv_, framebuffer, attachment, pname, params)
    {
        const auto backup = FBOBinder(this, GL_DRAW_FRAMEBUFFER, framebuffer);
        GetFramebufferAttachmentParameteriv_(GL_DRAW_FRAMEBUFFER, attachment, pname, params);
    }
}
void CtxFuncs::ClearDepth(GLclampd d) const
{
    CALL_EXISTS(ClearDepth_, d)
    CALL_EXISTS(ClearDepthf_, static_cast<GLclampf>(d))
    {
        COMMON_THROWEX(OGLException, OGLException::GLComponent::OGLU, u"unsupported textarget with calling ClearDepth");
    }
}
void CtxFuncs::ClearNamedFramebufferiv(GLuint framebuffer, GLenum buffer, GLint drawbuffer, const GLint* value) const
{
    CALL_EXISTS(ClearNamedFramebufferiv_, framebuffer, buffer, drawbuffer, value)
    {
        const auto backup = FBOBinder(this, GL_DRAW_FRAMEBUFFER, framebuffer);
        ClearBufferiv_(buffer, drawbuffer, value);
    }
}
void CtxFuncs::ClearNamedFramebufferuiv(GLuint framebuffer, GLenum buffer, GLint drawbuffer, const GLuint* value) const
{
    CALL_EXISTS(ClearNamedFramebufferuiv_, framebuffer, buffer, drawbuffer, value)
    {
        const auto backup = FBOBinder(this, GL_DRAW_FRAMEBUFFER, framebuffer);
        ClearBufferuiv_(buffer, drawbuffer, value);
    }
}
void CtxFuncs::ClearNamedFramebufferfv(GLuint framebuffer, GLenum buffer, GLint drawbuffer, const GLfloat* value) const
{
    CALL_EXISTS(ClearNamedFramebufferfv_, framebuffer, buffer, drawbuffer, value)
    {
        const auto backup = FBOBinder(this, GL_DRAW_FRAMEBUFFER, framebuffer);
        ClearBufferfv_(buffer, drawbuffer, value);
    }
}
void CtxFuncs::ClearNamedFramebufferDepthStencil(GLuint framebuffer, GLfloat depth, GLint stencil) const
{
    CALL_EXISTS(ClearNamedFramebufferfi_, framebuffer, GL_DEPTH_STENCIL, 0, depth, stencil)
    {
        const auto backup = FBOBinder(this, GL_DRAW_FRAMEBUFFER, framebuffer);
        ClearBufferfi_(GL_DEPTH_STENCIL, 0, depth, stencil);
    }
}


void CtxFuncs::SetObjectLabel(GLenum identifier, GLuint id, std::u16string_view name) const
{
    if (ObjectLabel_)
    {
        const auto str = common::str::to_u8string(name, common::str::Encoding::UTF16LE);
        ObjectLabel_(identifier, id,
            static_cast<GLsizei>(std::min<size_t>(str.size(), MaxLabelLen)),
            reinterpret_cast<const GLchar*>(str.c_str()));
    }
    else if (LabelObjectEXT_)
    {
        GLenum type;
        switch (identifier)
        {
        case GL_BUFFER:             type = GL_BUFFER_OBJECT_EXT;            break;
        case GL_SHADER:             type = GL_SHADER_OBJECT_EXT;            break;
        case GL_PROGRAM:            type = GL_PROGRAM_OBJECT_EXT;           break;
        case GL_VERTEX_ARRAY:       type = GL_VERTEX_ARRAY_OBJECT_EXT;      break;
        case GL_QUERY:              type = GL_QUERY_OBJECT_EXT;             break;
        case GL_PROGRAM_PIPELINE:   type = GL_PROGRAM_PIPELINE_OBJECT_EXT;  break;
        case GL_TRANSFORM_FEEDBACK: type = GL_TRANSFORM_FEEDBACK;           break;
        case GL_SAMPLER:            type = GL_SAMPLER;                      break;
        case GL_TEXTURE:            type = GL_TEXTURE;                      break;
        case GL_RENDERBUFFER:       type = GL_RENDERBUFFER;                 break;
        case GL_FRAMEBUFFER:        type = GL_FRAMEBUFFER;                  break;
        default:                    return;
        }
        const auto str = common::str::to_u8string(name, common::str::Encoding::UTF16LE);
        LabelObjectEXT_(type, id,
            static_cast<GLsizei>(str.size()),
            reinterpret_cast<const GLchar*>(str.c_str()));
    }
}
void CtxFuncs::SetObjectLabel(GLsync sync, std::u16string_view name) const
{
    if (ObjectPtrLabel_)
    {
        const auto str = common::str::to_u8string(name, common::str::Encoding::UTF16LE);
        ObjectPtrLabel_(sync,
            static_cast<GLsizei>(std::min<size_t>(str.size(), MaxLabelLen)),
            reinterpret_cast<const GLchar*>(str.c_str()));
    }
}
void CtxFuncs::PushDebugGroup(GLenum source, GLuint id, std::u16string_view message) const
{
    if (PushDebugGroup_)
    {
        const auto str = common::str::to_u8string(message, common::str::Encoding::UTF16LE);
        PushDebugGroup_(source, id,
            static_cast<GLsizei>(std::min<size_t>(str.size(), MaxMessageLen)),
            reinterpret_cast<const GLchar*>(str.c_str()));
    }
    else if (PushGroupMarkerEXT_)
    {
        const auto str = common::str::to_u8string(message, common::str::Encoding::UTF16LE);
        PushGroupMarkerEXT_(static_cast<GLsizei>(str.size()), reinterpret_cast<const GLchar*>(str.c_str()));
    }
}
void CtxFuncs::PopDebugGroup() const
{
    if (PopDebugGroup_)
        PopDebugGroup_();
    else if (PopGroupMarkerEXT_)
        PopGroupMarkerEXT_();
}
void CtxFuncs::InsertDebugMarker(GLuint id, std::u16string_view name) const
{
    if (DebugMessageInsert_)
    {
        const auto str = common::str::to_u8string(name, common::str::Encoding::UTF16LE);
        DebugMessageInsert_(GL_DEBUG_SOURCE_THIRD_PARTY, GL_DEBUG_TYPE_MARKER, id, GL_DEBUG_SEVERITY_NOTIFICATION,
            static_cast<GLsizei>(std::min<size_t>(str.size(), MaxMessageLen)),
            reinterpret_cast<const GLchar*>(str.c_str()));
    }
    else if (InsertEventMarkerEXT)
    {
        const auto str = common::str::to_u8string(name, common::str::Encoding::UTF16LE);
        InsertEventMarkerEXT(static_cast<GLsizei>(str.size()), reinterpret_cast<const GLchar*>(str.c_str()));
    }
    else if (DebugMessageInsertAMD)
    {
        const auto str = common::str::to_u8string(name, common::str::Encoding::UTF16LE);
        DebugMessageInsertAMD(GL_DEBUG_CATEGORY_OTHER_AMD, GL_DEBUG_SEVERITY_LOW_AMD, id,
            static_cast<GLsizei>(std::min<size_t>(str.size(), MaxMessageLen)),
            reinterpret_cast<const GLchar*>(str.c_str()));
    }
}


common::container::FrozenDenseSet<std::string_view> CtxFuncs::GetExtensions() const
{
    if (GetStringi)
    {
        GLint count;
        GetIntegerv(GL_NUM_EXTENSIONS, &count);
        std::vector<std::string_view> exts;
        exts.reserve(count);
        for (GLint i = 0; i < count; i++)
        {
            const GLubyte* ext = GetStringi(GL_EXTENSIONS, i);
            exts.emplace_back(reinterpret_cast<const char*>(ext));
        }
        return exts;
    }
    else
    {
        const GLubyte* exts = GetString(GL_EXTENSIONS);
        return common::str::Split(reinterpret_cast<const char*>(exts), ' ', false);
    }
}
std::optional<std::string_view> CtxFuncs::GetError() const
{
    const auto err = GetError_();
    switch (err)
    {
    case GL_NO_ERROR:                       return {};
    case GL_INVALID_ENUM:                   return "GL_INVALID_ENUM";
    case GL_INVALID_VALUE:                  return "GL_INVALID_VALUE";
    case GL_INVALID_OPERATION:              return "GL_INVALID_OPERATION";
    case GL_INVALID_FRAMEBUFFER_OPERATION:  return "GL_INVALID_FRAMEBUFFER_OPERATION";
    case GL_OUT_OF_MEMORY:                  return "GL_OUT_OF_MEMORY";
    case GL_STACK_UNDERFLOW:                return "GL_STACK_UNDERFLOW";
    case GL_STACK_OVERFLOW:                 return "GL_STACK_OVERFLOW";
    default:                                return "UNKNOWN_ERROR";
    }
}




}