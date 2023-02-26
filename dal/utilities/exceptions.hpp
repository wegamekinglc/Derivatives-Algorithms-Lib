//
// Created by wegamekinglc on 18-1-15.
//

#pragma once

#include <dal/platform/config.hpp>
#include <dal/math/vectors.hpp>
#include <dal/string/strings.hpp>
#include <exception>

namespace Dal {

    class Date_;
    class DateTime_;

    class Exception_ : public std::runtime_error {
    public:
        Exception_(const std::string& file, long line, const std::string& functionName, const char* msg);
        Exception_(const std::string& file, long line, const std::string& functionName, const std::string& msg)
            : Exception_(file, line, functionName, msg.c_str()) {}
        Exception_(const std::string& file, long line, const std::string& functionName, const String_& msg)
            : Exception_(file, line, functionName, msg.c_str()) {}
    };

    namespace exception {
        class XStackInfo_ {
            const char* name_;
            const void* value_;
            enum class Type_ { INT, DBL, CSTR, STR, DATE, DATETIME, VOID } type_;
            template <class T_> XStackInfo_(const char*, T_){};

        public:
            XStackInfo_(const char* name, const int& val);
            XStackInfo_(const char* name, const double& val);
            XStackInfo_(const char* name, const char* val);
            XStackInfo_(const char* name, const Date_& val);
            XStackInfo_(const char* name, const DateTime_& val);
            XStackInfo_(const char* name, const String_& val);
            explicit XStackInfo_(const char* msg);
            std::string Message() const;
        };

        void PushStack(const XStackInfo_& info);
        void PopStack();

        struct StackRegister_ {
            ~StackRegister_() { PopStack(); }
            template <class T_> StackRegister_(const char* name, const T_& val) { PushStack(XStackInfo_(name, val)); }

            explicit StackRegister_(const char* msg) { PushStack(XStackInfo_(msg)); }
        };
    } // namespace exception
} // namespace Dal

#define THROW(msg) throw Dal::Exception_(__FILE__, __LINE__, __func__, msg)
#define THROW2(msg, exception_type) throw exception_type(__FILE__, __LINE__, __func__, msg)

#ifndef NDEBUG
#define ASSERT(cond, msg)                                                                                              \
    if (cond)                                                                                                          \
        ;                                                                                                              \
    else                                                                                                               \
        THROW(msg)
#else
#define ASSERT(cond, msg)
#endif

#ifdef DAL_USE_REQUIRE
#define REQUIRE(cond, msg)                                                                                             \
    if (cond)                                                                                                          \
        ;                                                                                                              \
    else                                                                                                               \
        THROW(msg)
#else
#define REQUIRE(cond, msg)
#endif

#ifdef DAL_USE_REQUIRE
#define REQUIRE2(cond, msg, exception_type)                                                                                             \
    if (cond)                                                                                                          \
        ;                                                                                                              \
    else                                                                                                               \
        THROW2(msg, exception_type)
#define ASSURE(cond, msg)                                                                                              \
    if (cond)                                                                                                          \
        ;                                                                                                              \
    else                                                                                                               \
        THROW(msg)
#else
#define REQUIRE2(cond, msg, exception_type)
#define ASSURE(cond, msg)
#endif

#ifdef DAL_USE_NOTE
#define XXNOTICE(u, n, v) Dal::exception::StackRegister_ __xsr##u(n, v)
#define XNOTICE(u, n, v) XXNOTICE(u, n, v)
#define NOTICE2(n, v) XNOTICE(__COUNTER__, n, v)
#define NOTICE(x) NOTICE2(#x, x)

#define XXNOTE(u, m) Dal::exception::StackRegister_ __xsr##u(m)
#define XNOTE(u, m) XXNOTE(u, m)
#define NOTE(msg) XNOTE(__COUNTER__, msg)
#else
#define XXNOTICE(u, n, v)
#define XNOTICE(u, n, v)
#define NOTICE2(n, v)
#define NOTICE(x)

#define XXNOTE(u, m)
#define XNOTE(u, m)
#define NOTE(msg)
#endif
