#pragma once
#include "oclRely.h"
#include "oclUtil.h"

namespace oclu
{


class OCLException : public common::BaseException
{
public:
    EXCEPTION_CLONE_EX(OCLException);
    enum class CLComponent { Compiler, Driver, Accellarator, OCLU };
    const CLComponent exceptionSource;
protected:
    OCLException(const char* const type, const CLComponent source, const std::u16string_view& msg, const std::any& data_ = std::any())
        : BaseException(type, msg, data_), exceptionSource(source) { }
public:
    OCLException(const CLComponent source, const std::u16string_view& msg, const std::any& data_ = std::any())
        : OCLException(TYPENAME, source, msg, data_)
    { }
    virtual ~OCLException() {}
};




}