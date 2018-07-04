/**
 * @file    base\exception.h
 * @brief   Encapsulation for all exception classes of lite lib
 * @author  Nik Yan
 * @version 1.0
 * @date    2014-07-20
 */

#ifndef _LITE_EXCEPTION_H_
#define _LITE_EXCEPTION_H_

#include "lite_base.h"
#include <exception>
#include <string>
#include <iostream>
#include <sstream>
#include <stdlib.h>

#ifdef OS_LINUX
#include <execinfo.h>
#endif


namespace lite {

class LiteExceptionBase : public std::exception
{
public:
    explicit LiteExceptionBase() throw()
        : msg_("<unknown>")
    {
#ifdef OS_LINUX
        stacktrace_size_ = backtrace(stacktrace_, MAX_STACK_TRACE_SIZE);
#endif
    }

    explicit LiteExceptionBase(const std::string& msg) throw()
        : msg_(msg)
    {
#ifdef OS_LINUX
        stacktrace_size_ = backtrace(stacktrace_, MAX_STACK_TRACE_SIZE);
#endif
    }

    explicit LiteExceptionBase(const char* msg) throw()
        : msg_(msg)
    {
#ifdef OS_LINUX
        stacktrace_size_ = backtrace(stacktrace_, MAX_STACK_TRACE_SIZE);
#endif
    }

    virtual ~LiteExceptionBase() throw()
    {
    }

    const char* what() const throw()
    {
        return _ToString().c_str();
    }

protected:
    const std::string& _ToString() const;
#ifdef OS_LINUX
    std::string _GetStackTrace() const;
#endif
    std::string     msg_;

private:
#ifdef OS_LINUX
    enum { MAX_STACK_TRACE_SIZE = 50 };
    void*               stacktrace_[MAX_STACK_TRACE_SIZE];
    size_t              stacktrace_size_;
#endif
    mutable std::string what_;
}; // end of class LiteExceptionBase

inline
const std::string& LiteExceptionBase::_ToString() const
{
    if (what_.empty())
    {
        std::stringstream sstr("");

        if (!msg_.empty())
        {
            sstr << msg_;
        }
#ifdef OS_LINUX
        sstr << "\nStack Trace:\n";
        sstr << _GetStackTrace();
#endif
        what_ = sstr.str();
    }
    return what_;
}

#ifdef OS_LINUX
inline
std::string LiteExceptionBase::_GetStackTrace() const
{
    if (stacktrace_size_ == 0)
    {
        return "<No stack trace>\n";
    }

    char** strings = backtrace_symbols(stacktrace_, 10);
    if (strings == NULL)
    {
        return "<Unknown error: backtrace_symbols returned NULL>\n";
    }

    std::string result;
    for (size_t i = 0; i < stacktrace_size_; ++i)
    {
        std::string            mangled_name = strings[i];
        std::string::size_type begin        = mangled_name.find('(');
        std::string::size_type end          = mangled_name.find('+', begin);
        if (begin == std::string::npos || end == std::string::npos)
        {
                result += mangled_name;
                result += '\n';
                continue;
        }
        ++begin;
        int status;
        char* s = abi::__cxa_demangle(mangled_name.substr(begin, end - begin).c_str(),
                                      NULL,
                                      0,
                                      &status);
        if (status != 0)
        {
               result += mangled_name;
               result += '\n';
               continue;
        }
        std::string demangled_name(s);
        free(s);

        result += mangled_name.substr(0, begin);
        result += demangled_name;
        result += mangled_name.substr(end);
        result += '\n';
    }
    free(strings);
    return result;
}
#endif

/**
 * @brief   Custom exception class macro
 */
#define DEFINE_CUSTOM_LITE_EXCEPTION(exception_class, default_msg)                           \
    class exception_class : public LiteExceptionBase                                         \
    {                                                                                        \
    public:                                                                                  \
        explicit exception_class() throw() : LiteExceptionBase(default_msg)               {} \
        explicit exception_class(const std::string& msg) throw() : LiteExceptionBase(msg) {} \
        explicit exception_class(const char* msg) throw() : LiteExceptionBase(msg)        {} \
    };
// end of define

/**
 * @brief   Exception class
 */
DEFINE_CUSTOM_LITE_EXCEPTION(null_ptr_exception, "Null pointer exception");             ///> Null pointer
DEFINE_CUSTOM_LITE_EXCEPTION(logic_exception, "Program logic execption");               ///> Program logic
DEFINE_CUSTOM_LITE_EXCEPTION(runtime_exception, "Program runtime exception");           ///> Program runtime
DEFINE_CUSTOM_LITE_EXCEPTION(invalid_param_exception, "Invalid parameter exception");   ///> Invalid parameter
DEFINE_CUSTOM_LITE_EXCEPTION(access_violation_exception, "Access violation exception"); ///> Access violation

} // end of namespace

using namespace lite;

#endif // ifndef _LITE_EXCEPTION_H_
