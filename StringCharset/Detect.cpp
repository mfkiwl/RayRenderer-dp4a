#include "Detect.h"
#include "uchardetlib/nscore.h"
#include "uchardetlib/nsUniversalDetector.h"

namespace common::strchset
{
using namespace common::str;

namespace detail
{

class Uchardet : public nsUniversalDetector
{
protected:
    std::string Charset;
    virtual void Report(const char* charset) override
    {
        Charset = charset;
    }
public:
    Uchardet() : nsUniversalDetector(NS_FILTER_ALL) { }

    virtual void Reset() override
    {
        nsUniversalDetector::Reset();
        Charset.clear();
    }

    bool IsDone() const
    {
        return mDone == PR_TRUE;
    }

    std::string GetEncoding() const
    {
        return Charset.empty() ? (IsDone() ? mDetectedCharset : "") : Charset;
    }
};

}


CharsetDetector::CharsetDetector() : Tool(std::make_unique<detail::Uchardet>())
{
}

CharsetDetector::~CharsetDetector() {}

bool CharsetDetector::HandleData(const void *data, const size_t len) const
{
    for (size_t offset = 0; offset < len && !Tool->IsDone();)
    {
        const uint32_t batchLen = static_cast<uint32_t>(std::min(len - offset, (size_t)/*UINT32_MAX*/4096));
        if (Tool->HandleData(reinterpret_cast<const char*>(data) + offset, batchLen) != NS_OK)
            return false;
        offset += batchLen;
    }
    return true;
}

void CharsetDetector::Reset() const 
{
    Tool->Reset();
}

void CharsetDetector::EndData() const
{
    Tool->DataEnd();
}

std::string CharsetDetector::GetEncoding() const
{
    return Tool->GetEncoding();
}

std::string GetEncoding(const void *data, const size_t len)
{
    thread_local CharsetDetector detector;
    detector.Reset();
    if (!detector.HandleData(data, len))
        return "error";
    detector.EndData();
    return detector.GetEncoding();
}


}