#pragma once

#if defined(_WIN32) || defined(__CYGWIN__)
# ifdef OCLU_EXPORT
#   define OCLUAPI _declspec(dllexport)
#   define COMMON_EXPORT
# else
#   define OCLUAPI _declspec(dllimport)
# endif
#else
# ifdef OCLU_EXPORT
#   define OCLUAPI __attribute__((visibility("default")))
#   define COMMON_EXPORT
# else
#   define OCLUAPI
# endif
#endif

#include "3DBasic/miniBLAS.hpp"
#include "common/CommonRely.hpp"
#include "common/Wrapper.hpp"
#include "common/Exceptions.hpp"
#include "common/ContainerEx.hpp"
#include "common/StringEx.hpp"
#include "common/AlignedContainer.hpp"
#include "common/PromiseTask.hpp"
#include "common/Linq.hpp"

#include "ImageUtil/ImageCore.h"
#include "StringCharset/Convert.h"
#include "OpenGLUtil/oglBuffer.h"
#include "OpenGLUtil/oglTexture.h"
#include "OpenGLUtil/oglContext.h"

#include <cstdio>
#include <memory>
#include <functional>
#include <type_traits>
#include <assert.h>
#include <string>
#include <string_view>
#include <cstring>
#include <vector>
#include <set>
#include <map>
#include <tuple>
#include <optional>


#define CL_TARGET_OPENCL_VERSION 220
#define CL_USE_DEPRECATED_OPENCL_1_2_APIS
#define CL_USE_DEPRECATED_OPENCL_2_0_APIS
#include "3rdParty/CL/opencl.h"
#include "3rdParty/CL/cl_ext_intel.h"

namespace oclu
{
namespace str = common::str;
namespace fs = common::fs;
using std::string;
using std::wstring;
using std::u16string;
using std::string_view;
using std::u16string_view;
using std::wstring_view;
using std::tuple;
using std::set;
using std::map;
using std::vector;
using std::function;
using std::optional;
using common::min;
using common::max;
using common::Wrapper;
using common::SimpleTimer;
using common::NonCopyable;
using common::NonMovable;
using common::vectorEx;
using common::PromiseResult;
using common::BaseException;
using common::FileException;
using common::linq::Linq;

class oclUtil;
using MessageCallBack = std::function<void(const u16string&)>;

enum class Vendor { Other = 0, NVIDIA, Intel, AMD };

namespace detail
{
class _oclPlatform;
class _oclContext;
class GLSharedCore;
class _oclMem;
class _oclBuffer;
class _oclImage;
class _oclKernel;
}

}

#ifdef OCLU_EXPORT
#include "common/miniLogger/miniLogger.h"
namespace oclu
{
common::mlog::MiniLogger<false>& oclLog();
}
#endif
