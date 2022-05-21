#ifndef DAL_STRINGS_I
#define DAL_STRINGS_I

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

%extend String_ {
      const char *  __repr__() {
        return $self->c_str();
      }
};

#endif