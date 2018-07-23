#pragma once

#include "TexUtilRely.h"
#include "ispc_texcomp/ispc_texcomp.h"
#include "OpenGLUtil/oglException.h"

namespace oglu::texutil::detail
{

static common::AlignedBuffer<32> CompressBC1(const Image& img)
{
    if (HAS_FIELD(img.DataType, ImageDataType::FLOAT_MASK))
        COMMON_THROW(OGLException, OGLException::GLComponent::OGLU, L"float data type not supported in BC1");

    if (img.DataType != ImageDataType::RGBA)
    {
        const auto tmpImg = img.ConvertTo(ImageDataType::RGBA);
        return CompressBC1(tmpImg);
    }
    const rgba_surface surface{ const_cast<uint8_t*>(img.GetRawPtr<uint8_t>()), (int32_t)img.Width, (int32_t)img.Height, (int32_t)(img.Width*img.ElementSize) };
    const uint32_t blkCount = img.Width * img.Height / 4 / 4;
    common::AlignedBuffer<32> buffer(8 * blkCount);
    CompressBlocksBC1(&surface, buffer.GetRawPtr<uint8_t>());
    return buffer;
}

static common::AlignedBuffer<32> CompressBC3(const Image& img)
{
    if (HAS_FIELD(img.DataType, ImageDataType::FLOAT_MASK))
        COMMON_THROW(OGLException, OGLException::GLComponent::OGLU, L"float data type not supported in BC3");

    if (img.DataType != ImageDataType::RGBA)
    {
        const auto tmpImg = img.ConvertTo(ImageDataType::RGBA);
        return CompressBC3(tmpImg);
    }
    const rgba_surface surface{ const_cast<uint8_t*>(img.GetRawPtr<uint8_t>()), (int32_t)img.Width, (int32_t)img.Height, (int32_t)(img.Width*img.ElementSize) };
    const uint32_t blkCount = img.Width * img.Height / 4 / 4;
    common::AlignedBuffer<32> buffer(16 * blkCount);
    CompressBlocksBC3(&surface, buffer.GetRawPtr<uint8_t>());
    return buffer;
}

static common::AlignedBuffer<32> CompressBC7(const Image& img, const bool needAlpha)
{
    if (HAS_FIELD(img.DataType, ImageDataType::FLOAT_MASK))
        COMMON_THROW(OGLException, OGLException::GLComponent::OGLU, L"float data type not supported in BC7");

    if (img.DataType != ImageDataType::RGBA)
    {
        const auto tmpImg = img.ConvertTo(ImageDataType::RGBA);
        return CompressBC7(tmpImg, needAlpha);
    }
    const rgba_surface surface{ const_cast<uint8_t*>(img.GetRawPtr<uint8_t>()), (int32_t)img.Width, (int32_t)img.Height, (int32_t)(img.Width*img.ElementSize) };
    const uint32_t blkCount = img.Width * img.Height / 4 / 4;
    common::AlignedBuffer<32> buffer(16 * blkCount);
    bc7_enc_settings settings;
    needAlpha ? GetProfile_alpha_ultrafast(&settings) : GetProfile_ultrafast(&settings);
    CompressBlocksBC7(&surface, buffer.GetRawPtr<uint8_t>(), &settings);
    return buffer;
}


}