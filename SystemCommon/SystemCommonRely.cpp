#include "SystemCommonPch.h"

namespace common
{

#if COMMON_OS_WIN
typedef LONG(WINAPI* RtlGetVersionPtr)(PRTL_OSVERSIONINFOW);
static uint32_t GetWinBuildImpl()
{
    HMODULE hMod = ::GetModuleHandleW(L"ntdll.dll");
    if (hMod)
    {
        RtlGetVersionPtr ptrGetVersion = reinterpret_cast<RtlGetVersionPtr>(::GetProcAddress(hMod, "RtlGetVersion"));
        if (ptrGetVersion != nullptr)
        {
            RTL_OSVERSIONINFOW rovi = { 0 };
            rovi.dwOSVersionInfoSize = sizeof(rovi);
            if (0 == ptrGetVersion(&rovi))
                return rovi.dwBuildNumber;
        }
    }
    return 0;
}

uint32_t GetWinBuildNumber()
{
    static uint32_t verNum = GetWinBuildImpl();
    return verNum;
}
#endif

static std::vector<void(*)()noexcept>& GetInitializerMap() noexcept
{
    static std::vector<void(*)()noexcept> initializers;
    return initializers;
}

uint32_t RegisterInitializer(void(*func)()noexcept) noexcept
{
    GetInitializerMap().emplace_back(func);
    return 0;
}

#if COMMON_OS_UNIX
__attribute__((constructor))
#endif
static void ExecuteInitializers() noexcept
{
    // printf("==[SystemCommon]==\tInit here.\n");
    for (const auto func : GetInitializerMap())
    {
        func();
    }
}

}


#if COMMON_OS_WIN

BOOL WINAPI DllMain(HINSTANCE, DWORD fdwReason, LPVOID)
{
    switch (fdwReason)
    {
    case DLL_PROCESS_ATTACH:
        common::ExecuteInitializers();
        break;
    case DLL_PROCESS_DETACH:
        break;
    case DLL_THREAD_ATTACH:
        break;
    case DLL_THREAD_DETACH:
        break;
    }
    return TRUE;
}

#endif
