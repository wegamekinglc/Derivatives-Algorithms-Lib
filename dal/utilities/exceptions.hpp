//
// Created by wegamekinglc on 18-1-15.
//

#pragma once

#include <exception>
#include <dal/string/strings.hpp>
#include <dal/math/vector.hpp>

namespace dal {

    class Date_;
    class DateTime_;

    class Exception_ : public std::runtime_error {
    public:
        explicit Exception_(const char *msg);
        explicit Exception_(const std::string &msg) : Exception_(msg.c_str()) {}
        explicit Exception_(const String_& msg): Exception_(msg.c_str()) {}
    };

    namespace exception {
        class XStackInfo_ {
            const char* name_;
            const void* value_;
            enum class Type_ { INT, DBL, CSTR, DATE, DATETIME, VOID} type_;
            template <class T_>
            XStackInfo_(const char*, T_) = default;

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

    }
}

#define THROW(msg) throw Exception_(msg)
#define REQUIRE(cond, msg) throw if (cond); else THROW(msg)
