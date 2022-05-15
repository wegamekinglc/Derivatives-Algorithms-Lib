#ifndef DAL_SWIG_I
#define DAL_SWIG_I

%module dal

%include "std_string.i"

%{
    #include <dal/string/strings.hpp>
    using Dal::String_;
%}

class String_ {
public:
    String_(const char* src);
    String_(const std::string& src);
};

#endif