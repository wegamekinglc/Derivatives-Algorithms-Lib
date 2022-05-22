#ifndef DAL_DATE_I
#define DAL_DATE_I

%include "std_string.i"
%{
#include <dal/time/date.hpp>
%}

class Date_ {
public:
    Date_(int yyyy, int mm, int dd);
};

namespace Dal::Date {
    short Year(const Date_& dt);
    short Month(const Date_& dt);
    short Day(const Date_& dt);
};

%extend Date_ {
//      const char *  __repr__() {
//        return $self->c_str();
//      }

    std::string __repr__() {
        std::ostringstream out;
        out << Date::ToString(*$self);
        return out.str();
      }
};

#endif