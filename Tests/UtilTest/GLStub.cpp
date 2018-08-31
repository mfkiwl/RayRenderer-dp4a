#include "TestRely.h"
#include "OpenGLUtil/OpenGLUtil.h"
#include "OpenGLUtil/oglException.h"
#include <algorithm>

using namespace common;
using namespace common::mlog;
using namespace oglu;
using std::string;
using std::cin;

static MiniLogger<false>& log()
{
    static MiniLogger<false> logger(u"OGLStub", { GetConsoleBackend() });
    return logger;
}

oglContext CreateContext()
{
    oglUtil::init(true);
    // uint32_t major, minor;
    // log().info(u"Input major and minor version...");
    // std::cin >> major >> minor;
    // std::cin.ignore();
    // auto ctx = oglContext::NewContext(oglContext::CurrentContext(), false, { (uint8_t)major,(uint8_t)minor });
    auto ctx = oglContext::NewContext(oglContext::CurrentContext(), false, oglu::oglContext::GetLatestVersion());
    ctx->UseContext();
    return ctx;
}
#if defined(_WIN32)
#   define WIN32_LEAN_AND_MEAN 1
#   define NOMINMAX 1
#   include <Windows.h>
#   pragma comment(lib, "Opengl32.lib")
oglContext InitContext()
{
    HWND tmpWND = CreateWindow(
        L"Core", L"Fake Window",            // window class, title
        WS_CLIPSIBLINGS | WS_CLIPCHILDREN,  // style
        0, 0,                               // position x, y
        1, 1,                               // width, height
        NULL, NULL,                         // parent window, menu
        nullptr, NULL);                     // instance, param
    HDC tmpDC = GetDC(tmpWND);
    static PIXELFORMATDESCRIPTOR pfd =  // pfd Tells Windows How We Want Things To Be
    {
        sizeof(PIXELFORMATDESCRIPTOR),  // Size Of This Pixel Format Descriptor
        1,                              // Version Number
        PFD_DRAW_TO_WINDOW/*Support Window*/ | PFD_SUPPORT_OPENGL/*Support OpenGL*/ | PFD_DOUBLEBUFFER/*Support Double Buffering*/ | PFD_GENERIC_ACCELERATED,
        PFD_TYPE_RGBA,                  // Request An RGBA Format
        32,                             // Select Our Color Depth
        0, 0, 0, 0, 0, 0,               // Color Bits Ignored
        0, 0,                           // No Alpha Buffer, Shift Bit Ignored
        0, 0, 0, 0, 0,                  // No Accumulation Buffer, Accumulation Bits Ignored
        24,                             // 24Bit Z-Buffer (Depth Buffer) 
        8,                              // 8Bit Stencil Buffer
        0,                              // No Auxiliary Buffer
        PFD_MAIN_PLANE,                 // Main Drawing Layer
        0,                              // Reserved
        0, 0, 0                         // Layer Masks Ignored
    };
    const int PixelFormat = ChoosePixelFormat(tmpDC, &pfd);
    SetPixelFormat(tmpDC, PixelFormat, &pfd);
    HGLRC tmpRC = wglCreateContext(tmpDC);
    wglMakeCurrent(tmpDC, tmpRC);

    auto ctx = CreateContext();
    wglDeleteContext(tmpRC);
    return ctx;
}
#else
#   include <X11/X.h>
#   include <X11/Xlib.h>
#   include <GL/gl.h>
#   include <GL/glx.h>
//fucking X11 defines some terrible macro
#   undef Always
oglContext InitContext()
{
    static int visual_attribs[] = 
    {
        GLX_X_RENDERABLE, true,
        GLX_RENDER_TYPE, GLX_RGBA_BIT,
        GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
        GLX_X_VISUAL_TYPE, GLX_TRUE_COLOR,
        GLX_DOUBLEBUFFER, true,
        GLX_RED_SIZE, 8,
        GLX_GREEN_SIZE, 8,
        GLX_BLUE_SIZE, 8,
        GLX_ALPHA_SIZE, 8,
        GLX_DEPTH_SIZE, 24,
        GLX_STENCIL_SIZE, 8,
        None
    };
    Display* display = XOpenDisplay(":0.0");
    /* open display */
    if (!display)
    {
        log().error(u"Failed to open display\n");
        COMMON_THROW(BaseException, u"Error");
    }

    /* get framebuffer configs, any is usable (might want to add proper attribs) */
    int fbcount = 0;
    GLXFBConfig* fbc = glXChooseFBConfig(display, DefaultScreen(display), visual_attribs, &fbcount);
    if (!fbc) 
    {
        log().error(u"Failed to get FBConfig\n");
        COMMON_THROW(BaseException, u"Error");
    }
    XVisualInfo *vi = glXGetVisualFromFBConfig(display, fbc[0]);
    const auto tmpCtx = glXCreateContext(display, vi, 0, GL_TRUE);
    Window win = DefaultRootWindow(display);
    glXMakeCurrent(display, win, tmpCtx);

    auto ctx = CreateContext();
    glXDestroyContext(display, tmpCtx);
    return ctx;
}
#endif

static void OGLStub()
{
    const auto ctx = InitContext();
    log().success(u"GL Context Version: [{}]\n", oglu::oglUtil::getVersion());
    while (true)
    {
        log().info(u"input opengl file:");
        string fpath;
        std::getline(cin, fpath);
        if (fpath == "EXTENSION")
        {
            string exttxts("Extensions:\n");
            for(const auto& ext : oglUtil::GetExtensions())
                exttxts.append(ext).append("\n");
            log().verbose(u"{}\n", exttxts);
            continue;
        }
        common::fs::path filepath(fpath);
        log().debug(u"loading gl file [{}]\n", filepath.u16string());
        try
        {
            oglDrawProgram glProg(u"GLProg");
            const auto shaderSrc = common::file::ReadAllText(fpath);
            try
            {
                glProg->AddExtShaders(shaderSrc);
                glProg->Link();
            }
            catch (const OGLException& gle)
            {
                log().error(u"OpenGL shader fail:\n{}\n", gle.message);
                const auto buildLog = gle.data.has_value() ? std::any_cast<u16string>(&gle.data) : nullptr;
                if (buildLog)
                    log().error(u"Extra info:{}\n", *buildLog);
                continue;
            }
        }
        catch (const BaseException& be)
        {
            log().error(u"Error here:\n{}\n\n", be.message);
        }
    }
    getchar();
}

const static uint32_t ID = RegistTest("OGLStub", &OGLStub);
