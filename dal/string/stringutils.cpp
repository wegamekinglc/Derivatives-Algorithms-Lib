//
// Created by wegamekinglc on 2020/5/2.
//

#include <dal/platform/platform.hpp>
#include <dal/utilities/exceptions.hpp>
#include <dal/string/stringutils.hpp>
#include <dal/utilities/algorithms.hpp>
#include <dal/utilities/functionals.hpp>

namespace {
    bool CharToBool(char c) {
        switch(c) {
        case 'T': case 't':
        case 'Y': case 'y':
        case '1':
            return true;
        case 'F': case 'f':
        case 'N': case 'n':
        case '0':
            return false;
        }
        THROW(Dal::String_("Can't convert '" + Dal::String_(1, c) + "' to a boolean"));
    }

    bool TestTrue(const Dal::String_& c) {
        return(c.size() == 4
               && toupper(c[0]) == 'T'
               && toupper(c[1]) == 'R'
               && toupper(c[2]) == 'U'
               && toupper(c[3]) == 'E');
    }

    bool TestFalse(const Dal::String_& c) {
        return (c.size() == 5
                && toupper(c[0]) == 'F'
                && toupper(c[1]) == 'A'
                && toupper(c[2]) == 'L'
                && toupper(c[3]) == 'S'
                && toupper(c[4]) == 'E');
    }
}

namespace Dal {
    bool String::ToBool(const String_& src)
    {
        REQUIRE(!src.empty(), "Can't convert empty String_ to boolean");
        if (src.size() == 1)
            return CharToBool(src[0]);
        if (TestTrue(src))
            return true;
        REQUIRE(TestFalse(src), String_("Can't convert '" + src + "' to boolean"));
        return false;
    }

    Vector_<bool> String::ToBoolVector(const String_& src) {
        if (src.find(',') != src.npos)
            return Apply(AsFunctor(ToBool), Split(src, ',', true));
        // could be literal true/false -- but could also be, e.g. ttft
        if (TestTrue(src))
            return Vector_<bool>(1, true);
        if (TestFalse(src))
            return Vector_<bool>(1, false);
        // ok, convert individual characters
        return Apply(AsFunctor(CharToBool), src);
    }
}