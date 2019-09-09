#pragma once

#include "ImageUtilRely.h"
#include "ImageSupport.hpp"
#include "ImageCore.h"

namespace xziar::img
{

IMGUTILAPI Image ReadImage(const fs::path& path, const ImageDataType dataType = ImageDataType::RGBA);
IMGUTILAPI Image ReadImage(RandomInputStream& stream, const std::u16string& ext, const ImageDataType dataType = ImageDataType::RGBA);
IMGUTILAPI void WriteImage(const Image& image, const fs::path& path, const uint8_t quality = 90);
IMGUTILAPI void WriteImage(const Image& image, RandomOutputStream& stream, const std::u16string& ext, const uint8_t quality = 90);

}