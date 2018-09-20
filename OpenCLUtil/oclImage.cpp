#include "oclRely.h"
#include "oclImage.h"
#include "oclUtil.h"
#include "oclPromise.hpp"


namespace oclu::detail
{

void CL_CALLBACK OnMemDestroyed(cl_mem memobj, void *user_data)
{
    const auto& buf = *reinterpret_cast<_oclImage*>(user_data);
    const auto[w, h, d] = buf.GetSize();
    oclLog().debug(u"oclImage {:p} with size [{}x{}x{}], being destroyed.\n", (void*)memobj, w, h, d);
    //async callback, should not access cl-func since buffer may be released at any time.
    //size_t size = 0;
    //clGetMemObjectInfo(memobj, CL_MEM_SIZE, sizeof(size), &size, nullptr);
}

using oglu::TextureDataFormat;

TextureDataFormat ParseImageFormat(const cl_image_format& format)
{
    TextureDataFormat dformat = TextureDataFormat::EMPTY_MASK;
    switch(format.image_channel_data_type)
    {
    case CL_SIGNED_INT8:        dformat |= TextureDataFormat::TYPE_I8  | TextureDataFormat::INTEGER_MASK; break;
    case CL_UNSIGNED_INT8:      dformat |= TextureDataFormat::TYPE_U8  | TextureDataFormat::INTEGER_MASK; break;
    case CL_SIGNED_INT16:       dformat |= TextureDataFormat::TYPE_I16 | TextureDataFormat::INTEGER_MASK; break;
    case CL_UNSIGNED_INT16:     dformat |= TextureDataFormat::TYPE_U16 | TextureDataFormat::INTEGER_MASK; break;
    case CL_SIGNED_INT32:       dformat |= TextureDataFormat::TYPE_I32 | TextureDataFormat::INTEGER_MASK; break;
    case CL_UNSIGNED_INT32:     dformat |= TextureDataFormat::TYPE_U32 | TextureDataFormat::INTEGER_MASK; break;
    case CL_SNORM_INT8:         dformat |= TextureDataFormat::TYPE_I8; break;
    case CL_UNORM_INT8:         dformat |= TextureDataFormat::TYPE_U8; break;
    case CL_SNORM_INT16:        dformat |= TextureDataFormat::TYPE_I16; break;
    case CL_UNORM_INT16:        dformat |= TextureDataFormat::TYPE_U16; break;
    case CL_HALF_FLOAT:         dformat |= TextureDataFormat::TYPE_HALF; break;
    case CL_FLOAT:              dformat |= TextureDataFormat::TYPE_FLOAT; break;
    case CL_UNORM_SHORT_565:    dformat |= TextureDataFormat::TYPE_565; break;
    case CL_UNORM_SHORT_555:    dformat |= TextureDataFormat::TYPE_5551; break;
    case CL_UNORM_INT_101010:   dformat |= TextureDataFormat::TYPE_10_2; break;
    default:                    return TextureDataFormat::EMPTY_MASK;
    }
    switch(format.image_channel_order)
    {
    case CL_A:
    case CL_LUMINANCE:
    case CL_INTENSITY:
    case CL_R:          dformat |= TextureDataFormat::FORMAT_R; break;
    case CL_RA:
    case CL_RG:         dformat |= TextureDataFormat::FORMAT_RG; break;
    case CL_RGB:        dformat |= TextureDataFormat::FORMAT_RGB; break;
    case CL_RGBx:
    case CL_RGBA:       dformat |= TextureDataFormat::FORMAT_RGBA; break;
    case CL_BGRA:       dformat |= TextureDataFormat::FORMAT_BGRA; break;
    default:            return TextureDataFormat::EMPTY_MASK;
    }
    return dformat;
}
static cl_image_format ParseImageFormat(const TextureDataFormat dformat)
{
    cl_image_format format;
    if (HAS_FIELD(dformat, TextureDataFormat::INTEGER_MASK))
    {
        switch (dformat & TextureDataFormat::TYPE_RAW_MASK)
        {
        case TextureDataFormat::TYPE_U8:     format.image_channel_data_type = CL_UNSIGNED_INT8; break;
        case TextureDataFormat::TYPE_I8:     format.image_channel_data_type = CL_SIGNED_INT8; break;
        case TextureDataFormat::TYPE_U16:    format.image_channel_data_type = CL_UNSIGNED_INT16; break;
        case TextureDataFormat::TYPE_I16:    format.image_channel_data_type = CL_SIGNED_INT16; break;
        case TextureDataFormat::TYPE_U32:    format.image_channel_data_type = CL_UNSIGNED_INT32; break;
        case TextureDataFormat::TYPE_I32:    format.image_channel_data_type = CL_SIGNED_INT32; break;
        default: COMMON_THROW(OCLWrongFormatException, u"unsupported integer format", dformat); // should not enter
        }
    }
    else
    {
        switch (dformat & TextureDataFormat::TYPE_RAW_MASK)
        {
        case TextureDataFormat::TYPE_U8:     format.image_channel_data_type = CL_UNORM_INT8; break;
        case TextureDataFormat::TYPE_I8:     format.image_channel_data_type = CL_SNORM_INT8; break;
        case TextureDataFormat::TYPE_U16:    format.image_channel_data_type = CL_UNORM_INT16; break;
        case TextureDataFormat::TYPE_I16:    format.image_channel_data_type = CL_SNORM_INT16; break;
        case TextureDataFormat::TYPE_HALF:   format.image_channel_data_type = CL_HALF_FLOAT; break;
        case TextureDataFormat::TYPE_FLOAT:  format.image_channel_data_type = CL_FLOAT; break;
        case TextureDataFormat::TYPE_565:    format.image_channel_data_type = CL_UNORM_SHORT_565; format.image_channel_order = CL_RGBx; return format;
        case TextureDataFormat::TYPE_5551:   format.image_channel_data_type = CL_UNORM_SHORT_555; format.image_channel_order = CL_RGBx; return format;
        case TextureDataFormat::TYPE_10_2:   format.image_channel_data_type = CL_UNORM_INT_101010; format.image_channel_order = CL_RGBx; return format;
        default: COMMON_THROW(OCLWrongFormatException, u"unsupported normalized/float format", dformat);
        }
    }
    switch (dformat & TextureDataFormat::FORMAT_MASK)
    {
    case TextureDataFormat::FORMAT_R:        format.image_channel_order = CL_R; break;
    case TextureDataFormat::FORMAT_RG:       format.image_channel_order = CL_RG; break;
    case TextureDataFormat::FORMAT_RGB:      format.image_channel_order = CL_RGB; break;
    case TextureDataFormat::FORMAT_RGBA:     format.image_channel_order = CL_RGBA; break;
    case TextureDataFormat::FORMAT_BGRA:     format.image_channel_order = CL_BGRA; break;
    default: COMMON_THROW(OCLWrongFormatException, u"unsupported channel", dformat);
    }
    if (format.image_channel_order == CL_RGB)
    {
        if (format.image_channel_data_type != CL_UNORM_SHORT_565 && format.image_channel_data_type != CL_UNORM_SHORT_555 && format.image_channel_data_type != CL_UNORM_INT_101010)
            COMMON_THROW(OCLWrongFormatException, u"unsupported format for RGB channel image", dformat);
    }
    if (format.image_channel_order == CL_BGRA)
    {
        const auto rdtype = dformat & TextureDataFormat::TYPE_RAW_MASK;
        if (rdtype != TextureDataFormat::TYPE_U8 && rdtype != TextureDataFormat::TYPE_I8)
            COMMON_THROW(OCLWrongFormatException, u"unsupported format for BGRA channel image", dformat);
    }
    return format;
}

static cl_image_desc CreateImageDesc(cl_mem_object_type type, const uint32_t width, const uint32_t height, const uint32_t depth = 1)
{
    cl_image_desc desc;
    desc.image_type = type;
    desc.image_width = width;
    desc.image_height = height;
    desc.image_depth = depth;
    desc.image_array_size = 0;
    desc.image_row_pitch = 0;
    desc.image_slice_pitch = 0;
    desc.num_mip_levels = 0;
    desc.num_samples = 0;
    desc.buffer = 0;
    return desc;
}
static cl_mem CreateMem(const cl_context ctx, const cl_mem_flags flag, const cl_image_desc& desc, const oglu::TextureDataFormat dformat)
{
    cl_int errcode;
    const auto format = ParseImageFormat(dformat);
    const auto id = clCreateImage(ctx, flag, &format, &desc, nullptr, &errcode);
    if (errcode != CL_SUCCESS)
        COMMON_THROW(OCLException, OCLException::CLComponent::Driver, errString(u"cannot create image", errcode));
    return id;
}

bool _oclImage::CheckFormatCompatible(oglu::TextureDataFormat format)
{
    try
    {
        ParseImageFormat(format);
        return true;
    }
    catch (const BaseException&)
    {
        return false;
    }
}
bool _oclImage::CheckFormatCompatible(oglu::TextureInnerFormat format)
{
    try
    {
        return CheckFormatCompatible(oglu::TexFormatUtil::ConvertDtypeFrom(format));
    }
    catch (const BaseException&)
    {
        return false;
    }
}

_oclImage::_oclImage(const oclContext& ctx, const MemFlag flag, const uint32_t width, const uint32_t height, const uint32_t depth, const oglu::TextureDataFormat dformat, const cl_mem id)
    :Context(ctx), Width(width), Height(height), Depth(depth), Flags(flag), Format(dformat), memID(id)
{
    clSetMemObjectDestructorCallback(memID, &OnMemDestroyed, this);
}
_oclImage::_oclImage(const oclContext& ctx, const MemFlag flag, const uint32_t width, const uint32_t height, const uint32_t depth, const oglu::TextureDataFormat dformat, cl_mem_object_type type)
    :_oclImage(ctx, flag, width, height, depth, dformat, CreateMem(ctx->context, (cl_mem_flags)flag, CreateImageDesc(type, width, height, depth), dformat))
{ }


_oclImage::~_oclImage()
{
#ifdef _DEBUG
    uint32_t refCount = 0;
    clGetMemObjectInfo(memID, CL_MEM_REFERENCE_COUNT, sizeof(uint32_t), &refCount, nullptr);
    if (refCount == 1)
    {
        oclLog().debug(u"oclImage {:p} with size [{}x{}], has {} reference being release.\n", (void*)memID, Width, Height, refCount);
        clReleaseMemObject(memID);
    }
    else
        oclLog().warning(u"oclImage {:p} with size [{}x{}], has {} reference and not able to release.\n", (void*)memID, Width, Height, refCount);
#else
    clReleaseMemObject(memID);
#endif
}


PromiseResult<void> _oclImage::Write(const oclCmdQue que, const void *data, const size_t size, const bool shouldBlock) const
{
    constexpr size_t origin[3] = { 0,0,0 };
    if (Width*Height*Depth*oglu::TexFormatUtil::ParseFormatSize(Format) > size)
        COMMON_THROW(BaseException, u"write size not sufficient");
    const size_t region[3] = { Width,Height,Depth };
    cl_event e;
    const auto ret = clEnqueueWriteImage(que->cmdque, memID, shouldBlock ? CL_TRUE : CL_FALSE, origin, region, 0, 0, data, 0, nullptr, &e);
    if (ret != CL_SUCCESS)
        COMMON_THROW(OCLException, OCLException::CLComponent::Driver, errString(u"cannot write clImage", ret));
    if (shouldBlock)
        return {};
    else
        return std::make_shared<detail::oclPromiseVoid>(e, que->cmdque);
}
PromiseResult<void> _oclImage::Write(const oclCmdQue que, const Image& image, const bool shouldBlock) const
{
    if (image.GetWidth() != Width || image.GetHeight() != Height * Depth)
        COMMON_THROW(BaseException, u"write image size mismatch");
    const auto wantFormat = oglu::TexFormatUtil::ConvertToImgType(Format, true);
    if (wantFormat != image.GetDataType())
        COMMON_THROW(OCLWrongFormatException, u"image datatype mismatch", Format, std::any(image.GetDataType()));
    return Write(que, image.GetRawPtr(), image.GetSize(), shouldBlock);
}
PromiseResult<void> _oclImage::Read(const oclCmdQue que, void *data, const bool shouldBlock) const
{
    constexpr size_t origin[3] = { 0,0,0 };
    const size_t region[3] = { Width,Height,Depth };
    cl_event e;
    const auto ret = clEnqueueReadImage(que->cmdque, memID, shouldBlock ? CL_TRUE : CL_FALSE, origin, region, 0, 0, data, 0, nullptr, &e);
    if (ret != CL_SUCCESS)
        COMMON_THROW(OCLException, OCLException::CLComponent::Driver, errString(u"cannot read clImage", ret));
    if (shouldBlock)
        return {};
    else
        return std::make_shared<detail::oclPromiseVoid>(e, que->cmdque);
}
PromiseResult<void> _oclImage::Read(const oclCmdQue que, Image& image, const bool shouldBlock) const
{
    image = Image(oglu::TexFormatUtil::ConvertToImgType(Format, true));
    image.SetSize(Width, Height*Depth);
    return _oclImage::Read(que, image.GetRawPtr(), shouldBlock);
}
PromiseResult<Image> _oclImage::Read(const oclCmdQue que) const
{
    auto pms = std::make_shared<detail::oclPromise<Image>>();
    pms->Result = Image(oglu::TexFormatUtil::ConvertToImgType(Format, true));
    pms->Result.SetSize(Width, Height*Depth);
    constexpr size_t origin[3] = { 0,0,0 };
    const size_t region[3] = { Width,Height,Depth };
    cl_event e;
    const auto ret = clEnqueueReadImage(que->cmdque, memID, CL_FALSE, origin, region, 0, 0, pms->Result.GetRawPtr(), 0, nullptr, &e);
    if (ret != CL_SUCCESS)
        COMMON_THROW(OCLException, OCLException::CLComponent::Driver, errString(u"cannot read clImage", ret));
    glFlush();
    return pms;
}
PromiseResult<common::AlignedBuffer<32>> _oclImage::ReadRaw(const oclCmdQue que) const
{
    const size_t size = Width * Height * Depth * oglu::TexFormatUtil::ParseFormatSize(Format);
    auto pms = std::make_shared<detail::oclPromise<common::AlignedBuffer<32>>>(size);
    constexpr size_t origin[3] = { 0,0,0 };
    const size_t region[3] = { Width,Height,Depth };
    cl_event e;
    const auto ret = clEnqueueReadImage(que->cmdque, memID, CL_FALSE, origin, region, 0, 0, pms->Result.GetRawPtr(), 0, nullptr, &e);
    if (ret != CL_SUCCESS)
        COMMON_THROW(OCLException, OCLException::CLComponent::Driver, errString(u"cannot read clImage", ret));
    glFlush();
    return pms;
}

_oclImage2D::_oclImage2D(const oclContext& ctx, const MemFlag flag, const uint32_t width, const uint32_t height, const oglu::TextureDataFormat dformat)
    : _oclImage(ctx, flag, width, height, 1, dformat, CL_MEM_OBJECT_IMAGE2D)
{ }
_oclImage2D::_oclImage2D(const oclContext& ctx, const MemFlag flag, const Image& image, const oclCmdQue que, const bool isNormalized)
    : _oclImage2D(ctx, flag, image.GetWidth(), image.GetHeight(), image.GetDataType(), isNormalized)
{
    Write(que, image);
}

_oclImage3D::_oclImage3D(const oclContext& ctx, const MemFlag flag, const uint32_t width, const uint32_t height, const uint32_t depth, const oglu::TextureDataFormat dformat)
    : _oclImage(ctx, flag, width, height, depth, dformat, CL_MEM_OBJECT_IMAGE3D)
{ }


_oclGLImage2D::_oclGLImage2D(const oclContext& ctx, const MemFlag flag, const oglu::oglTex2D& tex)
    : _oclImage2D(ctx, flag, tex->GetSize().first, tex->GetSize().second, 1, 
        oglu::TexFormatUtil::ConvertDtypeFrom(tex->GetInnerFormat()), CreateMemFromGLTex(ctx, flag, tex))
{ }


_oclGLImage3D::_oclGLImage3D(const oclContext& ctx, const MemFlag flag, const oglu::oglTex3D& tex)
    : _oclImage3D(ctx, flag, std::get<0>(tex->GetSize()), std::get<1>(tex->GetSize()), std::get<2>(tex->GetSize()),
        oglu::TexFormatUtil::ConvertDtypeFrom(tex->GetInnerFormat()), CreateMemFromGLTex(ctx, flag, tex))
{ }


}
