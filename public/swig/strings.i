#ifndef DAL_STRINGS_I
#define DAL_STRINGS_I

%include "std_string.i"
%{
#include <dal/string/strings.hpp>
%}

class String_ {
public:
    String_(const char* src);
    String_(const std::string& src);
};

%extend String_ {
    std::string  __repr__() {
        std::ostringstream out;
        out << *self;
        return out.str();
      }
};

#endif