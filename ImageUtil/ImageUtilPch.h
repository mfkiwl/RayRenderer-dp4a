#pragma once
#include "ImageUtilRely.h"
#include "DataConvertor.hpp"
#include "RGB15Converter.hpp"

#include "MiniLogger/MiniLogger.h"
#include "SystemCommon/FileEx.h"
#include "SystemCommon/FileMapperEx.h"
#include "SystemCommon/CopyEx.h"
#include "StringUtil/Convert.h"

#include "common/FileBase.hpp"
#include "common/Linq2.hpp"
#include "common/SpinLock.hpp"
#include "common/StringEx.hpp"
#include "common/TimeUtil.hpp"

#include <array>
#include <cmath>
#include <algorithm>
#include <functional>

namespace xziar::img
{
common::mlog::MiniLogger<false>& ImgLog();
}