#include "oclPch.h"
#include "oclContext.h"
#include "oclDevice.h"
#include "oclPlatform.h"
#include "oclUtil.h"
#include "oclImage.h"
#include "oclPromise.h"


namespace oclu
{
using std::string;
using std::u16string;
using std::string_view;
using std::u16string_view;
using std::vector;
using std::set;
using common::container::FindInVec;


MAKE_ENABLER_IMPL(oclCustomEvent)


static void CL_CALLBACK onNotify(const char * errinfo, [[maybe_unused]]const void * private_info, size_t, void *user_data)
{
    const oclContext_& ctx = *(oclContext_*)user_data;
    const auto u16Info = common::str::to_u16string(errinfo, common::str::Encoding::UTF8);
    ctx.OnMessage(u16Info);
}


extern xziar::img::TextureFormat ParseCLImageFormat(const cl_image_format& format);

static common::container::FrozenDenseSet<xziar::img::TextureFormat> GetSupportedImageFormat(
    const detail::PlatFuncs* funcs, const cl_context& ctx, const cl_mem_object_type type)
{
    cl_uint count;
    funcs->clGetSupportedImageFormats(ctx, CL_MEM_READ_ONLY, type, 0, nullptr, &count);
    vector<cl_image_format> formats(count);
    funcs->clGetSupportedImageFormats(ctx, CL_MEM_READ_ONLY, type, count, formats.data(), &count);
    set<xziar::img::TextureFormat, std::less<>> dformats;
    for (const auto format : formats)
        dformats.insert(ParseCLImageFormat(format));
    return dformats;
}

oclContext_::oclContext_(const oclPlatform_* plat, vector<cl_context_properties> props, common::span<const oclDevice> devices) :
    detail::oclCommon(*plat), Plat(std::move(plat)), Devices(devices.begin(), devices.end()), Version(Plat->Version)
{
    if (Plat->Version < 12)
        oclLog().warning(u"Try to create context on [{}], which does not even support OpenCL1.2\n", Plat->Ver);
    OnMessage += [](const auto& msg) 
    { 
        if (!common::str::IsBeginWith(msg, u"Performance hint:"))
            oclLog().verbose(u"{}\n", msg);
    };

    cl_int ret;
    vector<cl_device_id> DeviceIDs;
    DeviceIDs.reserve(Devices.size());
    bool supportIntelDiag = true;
    for (const auto& dev : Devices)
    {
        DeviceIDs.push_back(*dev->DeviceID);
        supportIntelDiag &= dev->Extensions.Has("cl_intel_driver_diagnostics");
    }
    constexpr cl_context_properties intelDiagnostics = CL_CONTEXT_DIAGNOSTICS_LEVEL_BAD_INTEL | CL_CONTEXT_DIAGNOSTICS_LEVEL_GOOD_INTEL | CL_CONTEXT_DIAGNOSTICS_LEVEL_NEUTRAL_INTEL;
    if (supportIntelDiag)
        props.insert(--props.cend(), { CL_CONTEXT_SHOW_DIAGNOSTICS_INTEL, intelDiagnostics });
    Context = Funcs->clCreateContext(props.data(), (cl_uint)DeviceIDs.size(), DeviceIDs.data(), &onNotify, this, &ret);
    if (ret != CL_SUCCESS)
        COMMON_THROW(OCLException, OCLException::CLComponent::Driver, ret, u"cannot create opencl-context");

    Img2DFormatSupport = GetSupportedImageFormat(Funcs, *Context, CL_MEM_OBJECT_IMAGE2D);
    Img3DFormatSupport = GetSupportedImageFormat(Funcs, *Context, CL_MEM_OBJECT_IMAGE3D);
}

oclContext_::~oclContext_()
{
#ifdef _DEBUG
    uint32_t refCount = 0;
    Funcs->clGetContextInfo(*Context, CL_CONTEXT_REFERENCE_COUNT, sizeof(uint32_t), &refCount, nullptr);
    if (refCount == 1)
    {
        oclLog().debug(u"oclContext {:p} named {}, has {} reference being release.\n", (void*)*Context, GetPlatformName(), refCount);
        Funcs->clReleaseContext(*Context);
    }
    else
        oclLog().warning(u"oclContext {:p} named {}, has {} reference and not able to release.\n", (void*)*Context, GetPlatformName(), refCount);
#else
    Funcs->clReleaseContext(*Context);
#endif
}

u16string oclContext_::GetPlatformName() const
{
    return Plat->Name;
}

Vendors oclContext_::GetVendor() const
{
    return Plat->PlatVendor;
}


oclDevice oclContext_::GetGPUDevice() const
{
    const auto gpuDev = std::find_if(Devices.cbegin(), Devices.cend(), [&](const oclDevice& dev)
    {
        return dev->Type == DeviceType::GPU;
    });
    return gpuDev == Devices.cend() ? oclDevice{} : *gpuDev;
}

void oclContext_::SetDebugResource(const bool shouldEnable) const
{
    DebugResource = shouldEnable;
}

bool oclContext_::ShouldDebugResurce() const
{
    return DebugResource;
}

bool oclContext_::CheckExtensionSupport(const std::string_view name) const
{
    if (Plat->GetExtensions().Has(name))
    {
        for (const auto& dev : Devices)
            if (!dev->Extensions.Has(name))
                return false;
        return true;
    }
    return false;
}

bool oclContext_::CheckIncludeDevice(const oclDevice dev) const noexcept
{
    for (const auto d : Devices)
        if (d == dev)
            return true;
    return false;
}

common::PromiseResult<void> oclContext_::CreateUserEvent(common::PmsCore pms)
{
    if (const auto clEvt = std::dynamic_pointer_cast<oclPromiseCore>(pms); clEvt)
    {
        if (const auto voidEvt = std::dynamic_pointer_cast<oclPromise<void>>(clEvt); voidEvt)
            return voidEvt;
        else
            oclLog().warning(u"creating user-event from cl-event with result.");
    }
    if (Version < 11)
        COMMON_THROW(OCLException, OCLException::CLComponent::OCLU, u"clUserEvent requires version at least 1.1");
    cl_int err;
    const auto evt = Funcs->clCreateUserEvent(*Context, &err);
    if (err != CL_SUCCESS)
        COMMON_THROW(OCLException, OCLException::CLComponent::Driver, err, u"cannot create user-event");
    auto ret = MAKE_ENABLER_SHARED(oclCustomEvent, (std::move(pms), evt));
    ret->Init();
    return ret;
}


}
