#pragma once

#ifdef IMGUTIL_EXPORT
#   define IMGUTILAPI _declspec(dllexport)
#   define COMMON_EXPORT
#else
#   define IMGUTILAPI _declspec(dllimport)
#endif

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>
#include <array>
#include <algorithm>
#include <filesystem>
#include "common/CommonRely.hpp"
#include "common/CommonMacro.hpp"
#include "common/AlignedContainer.hpp"
#include "common/FileEx.hpp"
#include "common/TimeUtil.hpp"
#include "common/Wrapper.hpp"

namespace xziar::img
{
namespace fs = std::experimental::filesystem;
using std::string;
using std::wstring;
}

#ifdef IMGUTIL_EXPORT
#include "common/miniLogger/miniLogger.h"
namespace xziar::img
{
common::mlog::logger& ImgLog();
}
#endif