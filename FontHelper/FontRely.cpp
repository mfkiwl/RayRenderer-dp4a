#include "FontRely.h"
#include "FontInternal.h"
#include "../common/TimeUtil.inl"

namespace oglu
{

using namespace common::mlog;
logger& fntLog()
{
	static logger fntlog(L"FontHelper", nullptr, nullptr, LogOutput::Console, LogLevel::Debug);
	return fntlog;
}



}