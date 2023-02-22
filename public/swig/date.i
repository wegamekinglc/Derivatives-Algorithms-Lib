#ifndef DAL_DATE_I
#define DAL_DATE_I

%include "std_string.i"
%include "std_vector.i"
%{
#include <dal/time/date.hpp>
%}

class Date_ {
public:
    Date_(int yyyy, int mm, int dd);
    Date_ AddDays(int days);
};

namespace Dal::Date {
    short Year(const Date_& dt);
    short Month(const Date_& dt);
    short Day(const Date_& dt);
};

%extend Date_ {

    std::string __repr__() {
        std::ostringstream out;
        out << Date::ToString(*$self);
        return out.str();
    }

    bool __lt__(Date_* other) {
        return *$self < *other;
    }

    bool __le__(Date_* other) {
        return *$self <= *other;
    }

    bool __gt__(Date_* other) {
        return *$self > *other;
    }

    bool __ge__(Date_* other) {
        return *$self >= *other;
    }

    bool __eq__(Date_* other) {
        return *$self == *other;
    }
};

%template(DateVector) std::vector<Date_>;

#endif